/*
 * net/sched/cls_flower.c		Flower classifier
 *
 * Copyright (c) 2015 Jiri Pirko <jiri@resnulli.us>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/rhashtable.h>
#include <linux/workqueue.h>

#include <linux/if_ether.h>
#include <linux/in6.h>
#include <linux/ip.h>
#include <linux/mpls.h>

#include <net/sch_generic.h>
#include <net/pkt_cls.h>
#include <net/ip.h>
#include <net/flow_dissector.h>

#include <net/dst.h>
#include <net/dst_metadata.h>

struct fl_flow_key {
	int	indev_ifindex;
	struct flow_dissector_key_control control;
	struct flow_dissector_key_control enc_control;
	struct flow_dissector_key_basic basic;
	struct flow_dissector_key_eth_addrs eth;
	struct flow_dissector_key_vlan vlan;
	union {
		struct flow_dissector_key_ipv4_addrs ipv4;
		struct flow_dissector_key_ipv6_addrs ipv6;
	};
	struct flow_dissector_key_ports tp;
	struct flow_dissector_key_icmp icmp;
	struct flow_dissector_key_arp arp;
	struct flow_dissector_key_keyid enc_key_id;
	union {
		struct flow_dissector_key_ipv4_addrs enc_ipv4;
		struct flow_dissector_key_ipv6_addrs enc_ipv6;
	};
	struct flow_dissector_key_ports enc_tp;
	struct flow_dissector_key_mpls mpls;
	struct flow_dissector_key_tcp tcp;
	struct flow_dissector_key_ip ip;
} __aligned(BITS_PER_LONG / 8); /* Ensure that we can do comparisons as longs. */

struct fl_flow_mask_range {
	unsigned short int start;
	unsigned short int end;
};

struct fl_flow_mask {
	struct fl_flow_key key;
	struct fl_flow_mask_range range;
	struct rhash_head ht_node;
	struct rhashtable ht;
	struct rhashtable_params filter_ht_params;
	struct flow_dissector dissector;
	struct list_head filters;
	struct rcu_work rwork;
	struct list_head list;
};

struct cls_fl_head {
	struct rhashtable ht;
	struct list_head masks;
	struct rcu_work rwork;
	struct idr handle_idr;
};

struct cls_fl_filter {
	struct fl_flow_mask *mask;
	struct rhash_head ht_node;
	struct fl_flow_key mkey;
	struct tcf_exts exts;
	struct tcf_result res;
	struct fl_flow_key key;
	struct list_head list;
	u32 handle;
	u32 flags;
	struct rcu_work rwork;
	struct net_device *hw_dev;
};

static const struct rhashtable_params mask_ht_params = {
	.key_offset = offsetof(struct fl_flow_mask, key),
	.key_len = sizeof(struct fl_flow_key),
	.head_offset = offsetof(struct fl_flow_mask, ht_node),
	.automatic_shrinking = true,
};

static unsigned short int fl_mask_range(const struct fl_flow_mask *mask)
{
	return mask->range.end - mask->range.start;
}

static void fl_mask_update_range(struct fl_flow_mask *mask)
{
	const u8 *bytes = (const u8 *) &mask->key;
	size_t size = sizeof(mask->key);
	size_t i, first = 0, last;

	for (i = 0; i < size; i++) {
		if (bytes[i]) {
			first = i;
			break;
		}
	}
	last = first;
	for (i = size - 1; i != first; i--) {
		if (bytes[i]) {
			last = i;
			break;
		}
	}
	mask->range.start = rounddown(first, sizeof(long));
	mask->range.end = roundup(last + 1, sizeof(long));
}

static void *fl_key_get_start(struct fl_flow_key *key,
			      const struct fl_flow_mask *mask)
{
	return (u8 *) key + mask->range.start;
}

static void fl_set_masked_key(struct fl_flow_key *mkey, struct fl_flow_key *key,
			      struct fl_flow_mask *mask)
{
	const long *lkey = fl_key_get_start(key, mask);
	const long *lmask = fl_key_get_start(&mask->key, mask);
	long *lmkey = fl_key_get_start(mkey, mask);
	int i;

	for (i = 0; i < fl_mask_range(mask); i += sizeof(long))
		*lmkey++ = *lkey++ & *lmask++;
}

static void fl_clear_masked_range(struct fl_flow_key *key,
				  struct fl_flow_mask *mask)
{
	memset(fl_key_get_start(key, mask), 0, fl_mask_range(mask));
}

static struct cls_fl_filter *fl_lookup(struct fl_flow_mask *mask,
				       struct fl_flow_key *mkey)
{
	return rhashtable_lookup_fast(&mask->ht, fl_key_get_start(mkey, mask),
				      mask->filter_ht_params);
}

static int fl_classify(struct sk_buff *skb, const struct tcf_proto *tp,
		       struct tcf_result *res)
{
	struct cls_fl_head *head = rcu_dereference_bh(tp->root);
	struct cls_fl_filter *f;
	struct fl_flow_mask *mask;
	struct fl_flow_key skb_key;
	struct fl_flow_key skb_mkey;

	list_for_each_entry_rcu(mask, &head->masks, list) {
		fl_clear_masked_range(&skb_key, mask);

		skb_key.indev_ifindex = skb->skb_iif;
		/* skb_flow_dissect() does not set n_proto in case an unknown
		 * protocol, so do it rather here.
		 */
		skb_key.basic.n_proto = skb->protocol;
		skb_flow_dissect_tunnel_info(skb, &mask->dissector, &skb_key);
		skb_flow_dissect(skb, &mask->dissector, &skb_key, 0);

		fl_set_masked_key(&skb_mkey, &skb_key, mask);

		f = fl_lookup(mask, &skb_mkey);
		if (f && !tc_skip_sw(f->flags)) {
			*res = f->res;
			return tcf_exts_exec(skb, &f->exts, res);
		}
	}
	return -1;
}

static int fl_init(struct tcf_proto *tp)
{
	struct cls_fl_head *head;

	head = kzalloc(sizeof(*head), GFP_KERNEL);
	if (!head)
		return -ENOBUFS;

	INIT_LIST_HEAD_RCU(&head->masks);
	rcu_assign_pointer(tp->root, head);
	idr_init(&head->handle_idr);

	return rhashtable_init(&head->ht, &mask_ht_params);
}

static void fl_mask_free(struct fl_flow_mask *mask)
{
	rhashtable_destroy(&mask->ht);
	kfree(mask);
}

static void fl_mask_free_work(struct work_struct *work)
{
	struct fl_flow_mask *mask = container_of(to_rcu_work(work),
						 struct fl_flow_mask, rwork);

	fl_mask_free(mask);
}

static bool fl_mask_put(struct cls_fl_head *head, struct fl_flow_mask *mask,
			bool async)
{
	if (!list_empty(&mask->filters))
		return false;

	rhashtable_remove_fast(&head->ht, &mask->ht_node, mask_ht_params);
	list_del_rcu(&mask->list);
	if (async)
		tcf_queue_work(&mask->rwork, fl_mask_free_work);
	else
		fl_mask_free(mask);

	return true;
}

static void __fl_destroy_filter(struct cls_fl_filter *f)
{
	tcf_exts_destroy(&f->exts);
	tcf_exts_put_net(&f->exts);
	kfree(f);
}

static void fl_destroy_filter_work(struct work_struct *work)
{
	struct cls_fl_filter *f = container_of(to_rcu_work(work),
					struct cls_fl_filter, rwork);

	rtnl_lock();
	__fl_destroy_filter(f);
	rtnl_unlock();
}

static void fl_hw_destroy_filter(struct tcf_proto *tp, struct cls_fl_filter *f,
				 struct netlink_ext_ack *extack)
{
	struct tc_cls_flower_offload cls_flower = {};
	struct tcf_block *block = tp->chain->block;

	tc_cls_common_offload_init(&cls_flower.common, tp, f->flags, extack);
	cls_flower.command = TC_CLSFLOWER_DESTROY;
	cls_flower.cookie = (unsigned long) f;

	tc_setup_cb_call(block, &f->exts, TC_SETUP_CLSFLOWER,
			 &cls_flower, false);
	tcf_block_offload_dec(block, &f->flags);
}

static int fl_hw_replace_filter(struct tcf_proto *tp,
				struct cls_fl_filter *f,
				struct netlink_ext_ack *extack)
{
	struct tc_cls_flower_offload cls_flower = {};
	struct tcf_block *block = tp->chain->block;
	bool skip_sw = tc_skip_sw(f->flags);
	int err;

	tc_cls_common_offload_init(&cls_flower.common, tp, f->flags, extack);
	cls_flower.command = TC_CLSFLOWER_REPLACE;
	cls_flower.cookie = (unsigned long) f;
	cls_flower.dissector = &f->mask->dissector;
	cls_flower.mask = &f->mask->key;
	cls_flower.key = &f->mkey;
	cls_flower.exts = &f->exts;
	cls_flower.classid = f->res.classid;

	err = tc_setup_cb_call(block, &f->exts, TC_SETUP_CLSFLOWER,
			       &cls_flower, skip_sw);
	if (err < 0) {
		fl_hw_destroy_filter(tp, f, NULL);
		return err;
	} else if (err > 0) {
		tcf_block_offload_inc(block, &f->flags);
	}

	if (skip_sw && !(f->flags & TCA_CLS_FLAGS_IN_HW))
		return -EINVAL;

	return 0;
}

static void fl_hw_update_stats(struct tcf_proto *tp, struct cls_fl_filter *f)
{
	struct tc_cls_flower_offload cls_flower = {};
	struct tcf_block *block = tp->chain->block;

	tc_cls_common_offload_init(&cls_flower.common, tp, f->flags, NULL);
	cls_flower.command = TC_CLSFLOWER_STATS;
	cls_flower.cookie = (unsigned long) f;
	cls_flower.exts = &f->exts;
	cls_flower.classid = f->res.classid;

	tc_setup_cb_call(block, &f->exts, TC_SETUP_CLSFLOWER,
			 &cls_flower, false);
}

static bool __fl_delete(struct tcf_proto *tp, struct cls_fl_filter *f,
			struct netlink_ext_ack *extack)
{
	struct cls_fl_head *head = rtnl_dereference(tp->root);
	bool async = tcf_exts_get_net(&f->exts);
	bool last;

	idr_remove(&head->handle_idr, f->handle);
	list_del_rcu(&f->list);
	last = fl_mask_put(head, f->mask, async);
	if (!tc_skip_hw(f->flags))
		fl_hw_destroy_filter(tp, f, extack);
	tcf_unbind_filter(tp, &f->res);
	if (async)
		tcf_queue_work(&f->rwork, fl_destroy_filter_work);
	else
		__fl_destroy_filter(f);

	return last;
}

static void fl_destroy_sleepable(struct work_struct *work)
{
	struct cls_fl_head *head = container_of(to_rcu_work(work),
						struct cls_fl_head,
						rwork);

	rhashtable_destroy(&head->ht);
	kfree(head);
	module_put(THIS_MODULE);
}

static void fl_destroy(struct tcf_proto *tp, struct netlink_ext_ack *extack)
{
	struct cls_fl_head *head = rtnl_dereference(tp->root);
	struct fl_flow_mask *mask, *next_mask;
	struct cls_fl_filter *f, *next;

	list_for_each_entry_safe(mask, next_mask, &head->masks, list) {
		list_for_each_entry_safe(f, next, &mask->filters, list) {
			if (__fl_delete(tp, f, extack))
				break;
		}
	}
	idr_destroy(&head->handle_idr);

	__module_get(THIS_MODULE);
	tcf_queue_work(&head->rwork, fl_destroy_sleepable);
}

static void *fl_get(struct tcf_proto *tp, u32 handle)
{
	struct cls_fl_head *head = rtnl_dereference(tp->root);

	return idr_find(&head->handle_idr, handle);
}

static const struct nla_policy fl_policy[TCA_FLOWER_MAX + 1] = {
	[TCA_FLOWER_UNSPEC]		= { .type = NLA_UNSPEC },
	[TCA_FLOWER_CLASSID]		= { .type = NLA_U32 },
	[TCA_FLOWER_INDEV]		= { .type = NLA_STRING,
					    .len = IFNAMSIZ },
	[TCA_FLOWER_KEY_ETH_DST]	= { .len = ETH_ALEN },
	[TCA_FLOWER_KEY_ETH_DST_MASK]	= { .len = ETH_ALEN },
	[TCA_FLOWER_KEY_ETH_SRC]	= { .len = ETH_ALEN },
	[TCA_FLOWER_KEY_ETH_SRC_MASK]	= { .len = ETH_ALEN },
	[TCA_FLOWER_KEY_ETH_TYPE]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_IP_PROTO]	= { .type = NLA_U8 },
	[TCA_FLOWER_KEY_IPV4_SRC]	= { .type = NLA_U32 },
	[TCA_FLOWER_KEY_IPV4_SRC_MASK]	= { .type = NLA_U32 },
	[TCA_FLOWER_KEY_IPV4_DST]	= { .type = NLA_U32 },
	[TCA_FLOWER_KEY_IPV4_DST_MASK]	= { .type = NLA_U32 },
	[TCA_FLOWER_KEY_IPV6_SRC]	= { .len = sizeof(struct in6_addr) },
	[TCA_FLOWER_KEY_IPV6_SRC_MASK]	= { .len = sizeof(struct in6_addr) },
	[TCA_FLOWER_KEY_IPV6_DST]	= { .len = sizeof(struct in6_addr) },
	[TCA_FLOWER_KEY_IPV6_DST_MASK]	= { .len = sizeof(struct in6_addr) },
	[TCA_FLOWER_KEY_TCP_SRC]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_TCP_DST]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_UDP_SRC]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_UDP_DST]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_VLAN_ID]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_VLAN_PRIO]	= { .type = NLA_U8 },
	[TCA_FLOWER_KEY_VLAN_ETH_TYPE]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_ENC_KEY_ID]	= { .type = NLA_U32 },
	[TCA_FLOWER_KEY_ENC_IPV4_SRC]	= { .type = NLA_U32 },
	[TCA_FLOWER_KEY_ENC_IPV4_SRC_MASK] = { .type = NLA_U32 },
	[TCA_FLOWER_KEY_ENC_IPV4_DST]	= { .type = NLA_U32 },
	[TCA_FLOWER_KEY_ENC_IPV4_DST_MASK] = { .type = NLA_U32 },
	[TCA_FLOWER_KEY_ENC_IPV6_SRC]	= { .len = sizeof(struct in6_addr) },
	[TCA_FLOWER_KEY_ENC_IPV6_SRC_MASK] = { .len = sizeof(struct in6_addr) },
	[TCA_FLOWER_KEY_ENC_IPV6_DST]	= { .len = sizeof(struct in6_addr) },
	[TCA_FLOWER_KEY_ENC_IPV6_DST_MASK] = { .len = sizeof(struct in6_addr) },
	[TCA_FLOWER_KEY_TCP_SRC_MASK]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_TCP_DST_MASK]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_UDP_SRC_MASK]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_UDP_DST_MASK]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_SCTP_SRC_MASK]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_SCTP_DST_MASK]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_SCTP_SRC]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_SCTP_DST]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_ENC_UDP_SRC_PORT]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_ENC_UDP_SRC_PORT_MASK]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_ENC_UDP_DST_PORT]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_ENC_UDP_DST_PORT_MASK]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_FLAGS]		= { .type = NLA_U32 },
	[TCA_FLOWER_KEY_FLAGS_MASK]	= { .type = NLA_U32 },
	[TCA_FLOWER_KEY_ICMPV4_TYPE]	= { .type = NLA_U8 },
	[TCA_FLOWER_KEY_ICMPV4_TYPE_MASK] = { .type = NLA_U8 },
	[TCA_FLOWER_KEY_ICMPV4_CODE]	= { .type = NLA_U8 },
	[TCA_FLOWER_KEY_ICMPV4_CODE_MASK] = { .type = NLA_U8 },
	[TCA_FLOWER_KEY_ICMPV6_TYPE]	= { .type = NLA_U8 },
	[TCA_FLOWER_KEY_ICMPV6_TYPE_MASK] = { .type = NLA_U8 },
	[TCA_FLOWER_KEY_ICMPV6_CODE]	= { .type = NLA_U8 },
	[TCA_FLOWER_KEY_ICMPV6_CODE_MASK] = { .type = NLA_U8 },
	[TCA_FLOWER_KEY_ARP_SIP]	= { .type = NLA_U32 },
	[TCA_FLOWER_KEY_ARP_SIP_MASK]	= { .type = NLA_U32 },
	[TCA_FLOWER_KEY_ARP_TIP]	= { .type = NLA_U32 },
	[TCA_FLOWER_KEY_ARP_TIP_MASK]	= { .type = NLA_U32 },
	[TCA_FLOWER_KEY_ARP_OP]		= { .type = NLA_U8 },
	[TCA_FLOWER_KEY_ARP_OP_MASK]	= { .type = NLA_U8 },
	[TCA_FLOWER_KEY_ARP_SHA]	= { .len = ETH_ALEN },
	[TCA_FLOWER_KEY_ARP_SHA_MASK]	= { .len = ETH_ALEN },
	[TCA_FLOWER_KEY_ARP_THA]	= { .len = ETH_ALEN },
	[TCA_FLOWER_KEY_ARP_THA_MASK]	= { .len = ETH_ALEN },
	[TCA_FLOWER_KEY_MPLS_TTL]	= { .type = NLA_U8 },
	[TCA_FLOWER_KEY_MPLS_BOS]	= { .type = NLA_U8 },
	[TCA_FLOWER_KEY_MPLS_TC]	= { .type = NLA_U8 },
	[TCA_FLOWER_KEY_MPLS_LABEL]	= { .type = NLA_U32 },
	[TCA_FLOWER_KEY_TCP_FLAGS]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_TCP_FLAGS_MASK]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_IP_TOS]		= { .type = NLA_U8 },
	[TCA_FLOWER_KEY_IP_TOS_MASK]	= { .type = NLA_U8 },
	[TCA_FLOWER_KEY_IP_TTL]		= { .type = NLA_U8 },
	[TCA_FLOWER_KEY_IP_TTL_MASK]	= { .type = NLA_U8 },
};

static void fl_set_key_val(struct nlattr **tb,
			   void *val, int val_type,
			   void *mask, int mask_type, int len)
{
	if (!tb[val_type])
		return;
	memcpy(val, nla_data(tb[val_type]), len);
	if (mask_type == TCA_FLOWER_UNSPEC || !tb[mask_type])
		memset(mask, 0xff, len);
	else
		memcpy(mask, nla_data(tb[mask_type]), len);
}

static int fl_set_key_mpls(struct nlattr **tb,
			   struct flow_dissector_key_mpls *key_val,
			   struct flow_dissector_key_mpls *key_mask)
{
	if (tb[TCA_FLOWER_KEY_MPLS_TTL]) {
		key_val->mpls_ttl = nla_get_u8(tb[TCA_FLOWER_KEY_MPLS_TTL]);
		key_mask->mpls_ttl = MPLS_TTL_MASK;
	}
	if (tb[TCA_FLOWER_KEY_MPLS_BOS]) {
		u8 bos = nla_get_u8(tb[TCA_FLOWER_KEY_MPLS_BOS]);

		if (bos & ~MPLS_BOS_MASK)
			return -EINVAL;
		key_val->mpls_bos = bos;
		key_mask->mpls_bos = MPLS_BOS_MASK;
	}
	if (tb[TCA_FLOWER_KEY_MPLS_TC]) {
		u8 tc = nla_get_u8(tb[TCA_FLOWER_KEY_MPLS_TC]);

		if (tc & ~MPLS_TC_MASK)
			return -EINVAL;
		key_val->mpls_tc = tc;
		key_mask->mpls_tc = MPLS_TC_MASK;
	}
	if (tb[TCA_FLOWER_KEY_MPLS_LABEL]) {
		u32 label = nla_get_u32(tb[TCA_FLOWER_KEY_MPLS_LABEL]);

		if (label & ~MPLS_LABEL_MASK)
			return -EINVAL;
		key_val->mpls_label = label;
		key_mask->mpls_label = MPLS_LABEL_MASK;
	}
	return 0;
}

static void fl_set_key_vlan(struct nlattr **tb,
			    struct flow_dissector_key_vlan *key_val,
			    struct flow_dissector_key_vlan *key_mask)
{
#define VLAN_PRIORITY_MASK	0x7

	if (tb[TCA_FLOWER_KEY_VLAN_ID]) {
		key_val->vlan_id =
			nla_get_u16(tb[TCA_FLOWER_KEY_VLAN_ID]) & VLAN_VID_MASK;
		key_mask->vlan_id = VLAN_VID_MASK;
	}
	if (tb[TCA_FLOWER_KEY_VLAN_PRIO]) {
		key_val->vlan_priority =
			nla_get_u8(tb[TCA_FLOWER_KEY_VLAN_PRIO]) &
			VLAN_PRIORITY_MASK;
		key_mask->vlan_priority = VLAN_PRIORITY_MASK;
	}
}

static void fl_set_key_flag(u32 flower_key, u32 flower_mask,
			    u32 *dissector_key, u32 *dissector_mask,
			    u32 flower_flag_bit, u32 dissector_flag_bit)
{
	if (flower_mask & flower_flag_bit) {
		*dissector_mask |= dissector_flag_bit;
		if (flower_key & flower_flag_bit)
			*dissector_key |= dissector_flag_bit;
	}
}

static int fl_set_key_flags(struct nlattr **tb,
			    u32 *flags_key, u32 *flags_mask)
{
	u32 key, mask;

	/* mask is mandatory for flags */
	if (!tb[TCA_FLOWER_KEY_FLAGS_MASK])
		return -EINVAL;

	key = be32_to_cpu(nla_get_u32(tb[TCA_FLOWER_KEY_FLAGS]));
	mask = be32_to_cpu(nla_get_u32(tb[TCA_FLOWER_KEY_FLAGS_MASK]));

	*flags_key  = 0;
	*flags_mask = 0;

	fl_set_key_flag(key, mask, flags_key, flags_mask,
			TCA_FLOWER_KEY_FLAGS_IS_FRAGMENT, FLOW_DIS_IS_FRAGMENT);
	fl_set_key_flag(key, mask, flags_key, flags_mask,
			TCA_FLOWER_KEY_FLAGS_FRAG_IS_FIRST,
			FLOW_DIS_FIRST_FRAG);

	return 0;
}

static void fl_set_key_ip(struct nlattr **tb,
			  struct flow_dissector_key_ip *key,
			  struct flow_dissector_key_ip *mask)
{
		fl_set_key_val(tb, &key->tos, TCA_FLOWER_KEY_IP_TOS,
			       &mask->tos, TCA_FLOWER_KEY_IP_TOS_MASK,
			       sizeof(key->tos));

		fl_set_key_val(tb, &key->ttl, TCA_FLOWER_KEY_IP_TTL,
			       &mask->ttl, TCA_FLOWER_KEY_IP_TTL_MASK,
			       sizeof(key->ttl));
}

static int fl_set_key(struct net *net, struct nlattr **tb,
		      struct fl_flow_key *key, struct fl_flow_key *mask,
		      struct netlink_ext_ack *extack)
{
	__be16 ethertype;
	int ret = 0;
#ifdef CONFIG_NET_CLS_IND
	if (tb[TCA_FLOWER_INDEV]) {
		int err = tcf_change_indev(net, tb[TCA_FLOWER_INDEV], extack);
		if (err < 0)
			return err;
		key->indev_ifindex = err;
		mask->indev_ifindex = 0xffffffff;
	}
#endif

	fl_set_key_val(tb, key->eth.dst, TCA_FLOWER_KEY_ETH_DST,
		       mask->eth.dst, TCA_FLOWER_KEY_ETH_DST_MASK,
		       sizeof(key->eth.dst));
	fl_set_key_val(tb, key->eth.src, TCA_FLOWER_KEY_ETH_SRC,
		       mask->eth.src, TCA_FLOWER_KEY_ETH_SRC_MASK,
		       sizeof(key->eth.src));

	if (tb[TCA_FLOWER_KEY_ETH_TYPE]) {
		ethertype = nla_get_be16(tb[TCA_FLOWER_KEY_ETH_TYPE]);

		if (ethertype == htons(ETH_P_8021Q)) {
			fl_set_key_vlan(tb, &key->vlan, &mask->vlan);
			fl_set_key_val(tb, &key->basic.n_proto,
				       TCA_FLOWER_KEY_VLAN_ETH_TYPE,
				       &mask->basic.n_proto, TCA_FLOWER_UNSPEC,
				       sizeof(key->basic.n_proto));
		} else {
			key->basic.n_proto = ethertype;
			mask->basic.n_proto = cpu_to_be16(~0);
		}
	}

	if (key->basic.n_proto == htons(ETH_P_IP) ||
	    key->basic.n_proto == htons(ETH_P_IPV6)) {
		fl_set_key_val(tb, &key->basic.ip_proto, TCA_FLOWER_KEY_IP_PROTO,
			       &mask->basic.ip_proto, TCA_FLOWER_UNSPEC,
			       sizeof(key->basic.ip_proto));
		fl_set_key_ip(tb, &key->ip, &mask->ip);
	}

	if (tb[TCA_FLOWER_KEY_IPV4_SRC] || tb[TCA_FLOWER_KEY_IPV4_DST]) {
		key->control.addr_type = FLOW_DISSECTOR_KEY_IPV4_ADDRS;
		mask->control.addr_type = ~0;
		fl_set_key_val(tb, &key->ipv4.src, TCA_FLOWER_KEY_IPV4_SRC,
			       &mask->ipv4.src, TCA_FLOWER_KEY_IPV4_SRC_MASK,
			       sizeof(key->ipv4.src));
		fl_set_key_val(tb, &key->ipv4.dst, TCA_FLOWER_KEY_IPV4_DST,
			       &mask->ipv4.dst, TCA_FLOWER_KEY_IPV4_DST_MASK,
			       sizeof(key->ipv4.dst));
	} else if (tb[TCA_FLOWER_KEY_IPV6_SRC] || tb[TCA_FLOWER_KEY_IPV6_DST]) {
		key->control.addr_type = FLOW_DISSECTOR_KEY_IPV6_ADDRS;
		mask->control.addr_type = ~0;
		fl_set_key_val(tb, &key->ipv6.src, TCA_FLOWER_KEY_IPV6_SRC,
			       &mask->ipv6.src, TCA_FLOWER_KEY_IPV6_SRC_MASK,
			       sizeof(key->ipv6.src));
		fl_set_key_val(tb, &key->ipv6.dst, TCA_FLOWER_KEY_IPV6_DST,
			       &mask->ipv6.dst, TCA_FLOWER_KEY_IPV6_DST_MASK,
			       sizeof(key->ipv6.dst));
	}

	if (key->basic.ip_proto == IPPROTO_TCP) {
		fl_set_key_val(tb, &key->tp.src, TCA_FLOWER_KEY_TCP_SRC,
			       &mask->tp.src, TCA_FLOWER_KEY_TCP_SRC_MASK,
			       sizeof(key->tp.src));
		fl_set_key_val(tb, &key->tp.dst, TCA_FLOWER_KEY_TCP_DST,
			       &mask->tp.dst, TCA_FLOWER_KEY_TCP_DST_MASK,
			       sizeof(key->tp.dst));
		fl_set_key_val(tb, &key->tcp.flags, TCA_FLOWER_KEY_TCP_FLAGS,
			       &mask->tcp.flags, TCA_FLOWER_KEY_TCP_FLAGS_MASK,
			       sizeof(key->tcp.flags));
	} else if (key->basic.ip_proto == IPPROTO_UDP) {
		fl_set_key_val(tb, &key->tp.src, TCA_FLOWER_KEY_UDP_SRC,
			       &mask->tp.src, TCA_FLOWER_KEY_UDP_SRC_MASK,
			       sizeof(key->tp.src));
		fl_set_key_val(tb, &key->tp.dst, TCA_FLOWER_KEY_UDP_DST,
			       &mask->tp.dst, TCA_FLOWER_KEY_UDP_DST_MASK,
			       sizeof(key->tp.dst));
	} else if (key->basic.ip_proto == IPPROTO_SCTP) {
		fl_set_key_val(tb, &key->tp.src, TCA_FLOWER_KEY_SCTP_SRC,
			       &mask->tp.src, TCA_FLOWER_KEY_SCTP_SRC_MASK,
			       sizeof(key->tp.src));
		fl_set_key_val(tb, &key->tp.dst, TCA_FLOWER_KEY_SCTP_DST,
			       &mask->tp.dst, TCA_FLOWER_KEY_SCTP_DST_MASK,
			       sizeof(key->tp.dst));
	} else if (key->basic.n_proto == htons(ETH_P_IP) &&
		   key->basic.ip_proto == IPPROTO_ICMP) {
		fl_set_key_val(tb, &key->icmp.type, TCA_FLOWER_KEY_ICMPV4_TYPE,
			       &mask->icmp.type,
			       TCA_FLOWER_KEY_ICMPV4_TYPE_MASK,
			       sizeof(key->icmp.type));
		fl_set_key_val(tb, &key->icmp.code, TCA_FLOWER_KEY_ICMPV4_CODE,
			       &mask->icmp.code,
			       TCA_FLOWER_KEY_ICMPV4_CODE_MASK,
			       sizeof(key->icmp.code));
	} else if (key->basic.n_proto == htons(ETH_P_IPV6) &&
		   key->basic.ip_proto == IPPROTO_ICMPV6) {
		fl_set_key_val(tb, &key->icmp.type, TCA_FLOWER_KEY_ICMPV6_TYPE,
			       &mask->icmp.type,
			       TCA_FLOWER_KEY_ICMPV6_TYPE_MASK,
			       sizeof(key->icmp.type));
		fl_set_key_val(tb, &key->icmp.code, TCA_FLOWER_KEY_ICMPV6_CODE,
			       &mask->icmp.code,
			       TCA_FLOWER_KEY_ICMPV6_CODE_MASK,
			       sizeof(key->icmp.code));
	} else if (key->basic.n_proto == htons(ETH_P_MPLS_UC) ||
		   key->basic.n_proto == htons(ETH_P_MPLS_MC)) {
		ret = fl_set_key_mpls(tb, &key->mpls, &mask->mpls);
		if (ret)
			return ret;
	} else if (key->basic.n_proto == htons(ETH_P_ARP) ||
		   key->basic.n_proto == htons(ETH_P_RARP)) {
		fl_set_key_val(tb, &key->arp.sip, TCA_FLOWER_KEY_ARP_SIP,
			       &mask->arp.sip, TCA_FLOWER_KEY_ARP_SIP_MASK,
			       sizeof(key->arp.sip));
		fl_set_key_val(tb, &key->arp.tip, TCA_FLOWER_KEY_ARP_TIP,
			       &mask->arp.tip, TCA_FLOWER_KEY_ARP_TIP_MASK,
			       sizeof(key->arp.tip));
		fl_set_key_val(tb, &key->arp.op, TCA_FLOWER_KEY_ARP_OP,
			       &mask->arp.op, TCA_FLOWER_KEY_ARP_OP_MASK,
			       sizeof(key->arp.op));
		fl_set_key_val(tb, key->arp.sha, TCA_FLOWER_KEY_ARP_SHA,
			       mask->arp.sha, TCA_FLOWER_KEY_ARP_SHA_MASK,
			       sizeof(key->arp.sha));
		fl_set_key_val(tb, key->arp.tha, TCA_FLOWER_KEY_ARP_THA,
			       mask->arp.tha, TCA_FLOWER_KEY_ARP_THA_MASK,
			       sizeof(key->arp.tha));
	}

	if (tb[TCA_FLOWER_KEY_ENC_IPV4_SRC] ||
	    tb[TCA_FLOWER_KEY_ENC_IPV4_DST]) {
		key->enc_control.addr_type = FLOW_DISSECTOR_KEY_IPV4_ADDRS;
		mask->enc_control.addr_type = ~0;
		fl_set_key_val(tb, &key->enc_ipv4.src,
			       TCA_FLOWER_KEY_ENC_IPV4_SRC,
			       &mask->enc_ipv4.src,
			       TCA_FLOWER_KEY_ENC_IPV4_SRC_MASK,
			       sizeof(key->enc_ipv4.src));
		fl_set_key_val(tb, &key->enc_ipv4.dst,
			       TCA_FLOWER_KEY_ENC_IPV4_DST,
			       &mask->enc_ipv4.dst,
			       TCA_FLOWER_KEY_ENC_IPV4_DST_MASK,
			       sizeof(key->enc_ipv4.dst));
	}

	if (tb[TCA_FLOWER_KEY_ENC_IPV6_SRC] ||
	    tb[TCA_FLOWER_KEY_ENC_IPV6_DST]) {
		key->enc_control.addr_type = FLOW_DISSECTOR_KEY_IPV6_ADDRS;
		mask->enc_control.addr_type = ~0;
		fl_set_key_val(tb, &key->enc_ipv6.src,
			       TCA_FLOWER_KEY_ENC_IPV6_SRC,
			       &mask->enc_ipv6.src,
			       TCA_FLOWER_KEY_ENC_IPV6_SRC_MASK,
			       sizeof(key->enc_ipv6.src));
		fl_set_key_val(tb, &key->enc_ipv6.dst,
			       TCA_FLOWER_KEY_ENC_IPV6_DST,
			       &mask->enc_ipv6.dst,
			       TCA_FLOWER_KEY_ENC_IPV6_DST_MASK,
			       sizeof(key->enc_ipv6.dst));
	}

	fl_set_key_val(tb, &key->enc_key_id.keyid, TCA_FLOWER_KEY_ENC_KEY_ID,
		       &mask->enc_key_id.keyid, TCA_FLOWER_UNSPEC,
		       sizeof(key->enc_key_id.keyid));

	fl_set_key_val(tb, &key->enc_tp.src, TCA_FLOWER_KEY_ENC_UDP_SRC_PORT,
		       &mask->enc_tp.src, TCA_FLOWER_KEY_ENC_UDP_SRC_PORT_MASK,
		       sizeof(key->enc_tp.src));

	fl_set_key_val(tb, &key->enc_tp.dst, TCA_FLOWER_KEY_ENC_UDP_DST_PORT,
		       &mask->enc_tp.dst, TCA_FLOWER_KEY_ENC_UDP_DST_PORT_MASK,
		       sizeof(key->enc_tp.dst));

	if (tb[TCA_FLOWER_KEY_FLAGS])
		ret = fl_set_key_flags(tb, &key->control.flags, &mask->control.flags);

	return ret;
}

static void fl_mask_copy(struct fl_flow_mask *dst,
			 struct fl_flow_mask *src)
{
	const void *psrc = fl_key_get_start(&src->key, src);
	void *pdst = fl_key_get_start(&dst->key, src);

	memcpy(pdst, psrc, fl_mask_range(src));
	dst->range = src->range;
}

static const struct rhashtable_params fl_ht_params = {
	.key_offset = offsetof(struct cls_fl_filter, mkey), /* base offset */
	.head_offset = offsetof(struct cls_fl_filter, ht_node),
	.automatic_shrinking = true,
};

static int fl_init_mask_hashtable(struct fl_flow_mask *mask)
{
	mask->filter_ht_params = fl_ht_params;
	mask->filter_ht_params.key_len = fl_mask_range(mask);
	mask->filter_ht_params.key_offset += mask->range.start;

	return rhashtable_init(&mask->ht, &mask->filter_ht_params);
}

#define FL_KEY_MEMBER_OFFSET(member) offsetof(struct fl_flow_key, member)
#define FL_KEY_MEMBER_SIZE(member) (sizeof(((struct fl_flow_key *) 0)->member))

#define FL_KEY_IS_MASKED(mask, member)						\
	memchr_inv(((char *)mask) + FL_KEY_MEMBER_OFFSET(member),		\
		   0, FL_KEY_MEMBER_SIZE(member))				\

#define FL_KEY_SET(keys, cnt, id, member)					\
	do {									\
		keys[cnt].key_id = id;						\
		keys[cnt].offset = FL_KEY_MEMBER_OFFSET(member);		\
		cnt++;								\
	} while(0);

#define FL_KEY_SET_IF_MASKED(mask, keys, cnt, id, member)			\
	do {									\
		if (FL_KEY_IS_MASKED(mask, member))				\
			FL_KEY_SET(keys, cnt, id, member);			\
	} while(0);

static void fl_init_dissector(struct fl_flow_mask *mask)
{
	struct flow_dissector_key keys[FLOW_DISSECTOR_KEY_MAX];
	size_t cnt = 0;

	FL_KEY_SET(keys, cnt, FLOW_DISSECTOR_KEY_CONTROL, control);
	FL_KEY_SET(keys, cnt, FLOW_DISSECTOR_KEY_BASIC, basic);
	FL_KEY_SET_IF_MASKED(&mask->key, keys, cnt,
			     FLOW_DISSECTOR_KEY_ETH_ADDRS, eth);
	FL_KEY_SET_IF_MASKED(&mask->key, keys, cnt,
			     FLOW_DISSECTOR_KEY_IPV4_ADDRS, ipv4);
	FL_KEY_SET_IF_MASKED(&mask->key, keys, cnt,
			     FLOW_DISSECTOR_KEY_IPV6_ADDRS, ipv6);
	FL_KEY_SET_IF_MASKED(&mask->key, keys, cnt,
			     FLOW_DISSECTOR_KEY_PORTS, tp);
	FL_KEY_SET_IF_MASKED(&mask->key, keys, cnt,
			     FLOW_DISSECTOR_KEY_IP, ip);
	FL_KEY_SET_IF_MASKED(&mask->key, keys, cnt,
			     FLOW_DISSECTOR_KEY_TCP, tcp);
	FL_KEY_SET_IF_MASKED(&mask->key, keys, cnt,
			     FLOW_DISSECTOR_KEY_ICMP, icmp);
	FL_KEY_SET_IF_MASKED(&mask->key, keys, cnt,
			     FLOW_DISSECTOR_KEY_ARP, arp);
	FL_KEY_SET_IF_MASKED(&mask->key, keys, cnt,
			     FLOW_DISSECTOR_KEY_MPLS, mpls);
	FL_KEY_SET_IF_MASKED(&mask->key, keys, cnt,
			     FLOW_DISSECTOR_KEY_VLAN, vlan);
	FL_KEY_SET_IF_MASKED(&mask->key, keys, cnt,
			     FLOW_DISSECTOR_KEY_ENC_KEYID, enc_key_id);
	FL_KEY_SET_IF_MASKED(&mask->key, keys, cnt,
			     FLOW_DISSECTOR_KEY_ENC_IPV4_ADDRS, enc_ipv4);
	FL_KEY_SET_IF_MASKED(&mask->key, keys, cnt,
			     FLOW_DISSECTOR_KEY_ENC_IPV6_ADDRS, enc_ipv6);
	if (FL_KEY_IS_MASKED(&mask->key, enc_ipv4) ||
	    FL_KEY_IS_MASKED(&mask->key, enc_ipv6))
		FL_KEY_SET(keys, cnt, FLOW_DISSECTOR_KEY_ENC_CONTROL,
			   enc_control);
	FL_KEY_SET_IF_MASKED(&mask->key, keys, cnt,
			     FLOW_DISSECTOR_KEY_ENC_PORTS, enc_tp);

	skb_flow_dissector_init(&mask->dissector, keys, cnt);
}

static struct fl_flow_mask *fl_create_new_mask(struct cls_fl_head *head,
					       struct fl_flow_mask *mask)
{
	struct fl_flow_mask *newmask;
	int err;

	newmask = kzalloc(sizeof(*newmask), GFP_KERNEL);
	if (!newmask)
		return ERR_PTR(-ENOMEM);

	fl_mask_copy(newmask, mask);

	err = fl_init_mask_hashtable(newmask);
	if (err)
		goto errout_free;

	fl_init_dissector(newmask);

	INIT_LIST_HEAD_RCU(&newmask->filters);

	err = rhashtable_insert_fast(&head->ht, &newmask->ht_node,
				     mask_ht_params);
	if (err)
		goto errout_destroy;

	list_add_tail_rcu(&newmask->list, &head->masks);

	return newmask;

errout_destroy:
	rhashtable_destroy(&newmask->ht);
errout_free:
	kfree(newmask);

	return ERR_PTR(err);
}

static int fl_check_assign_mask(struct cls_fl_head *head,
				struct cls_fl_filter *fnew,
				struct cls_fl_filter *fold,
				struct fl_flow_mask *mask)
{
	struct fl_flow_mask *newmask;

	fnew->mask = rhashtable_lookup_fast(&head->ht, mask, mask_ht_params);
	if (!fnew->mask) {
		if (fold)
			return -EINVAL;

		newmask = fl_create_new_mask(head, mask);
		if (IS_ERR(newmask))
			return PTR_ERR(newmask);

		fnew->mask = newmask;
	} else if (fold && fold->mask != fnew->mask) {
		return -EINVAL;
	}

	return 0;
}

static int fl_set_parms(struct net *net, struct tcf_proto *tp,
			struct cls_fl_filter *f, struct fl_flow_mask *mask,
			unsigned long base, struct nlattr **tb,
			struct nlattr *est, bool ovr,
			struct netlink_ext_ack *extack)
{
	int err;

	err = tcf_exts_validate(net, tp, tb, est, &f->exts, ovr, extack);
	if (err < 0)
		return err;

	if (tb[TCA_FLOWER_CLASSID]) {
		f->res.classid = nla_get_u32(tb[TCA_FLOWER_CLASSID]);
		tcf_bind_filter(tp, &f->res, base);
	}

	err = fl_set_key(net, tb, &f->key, &mask->key, extack);
	if (err)
		return err;

	fl_mask_update_range(mask);
	fl_set_masked_key(&f->mkey, &f->key, mask);

	return 0;
}

static int fl_change(struct net *net, struct sk_buff *in_skb,
		     struct tcf_proto *tp, unsigned long base,
		     u32 handle, struct nlattr **tca,
		     void **arg, bool ovr, struct netlink_ext_ack *extack)
{
	struct cls_fl_head *head = rtnl_dereference(tp->root);
	struct cls_fl_filter *fold = *arg;
	struct cls_fl_filter *fnew;
	struct nlattr **tb;
	struct fl_flow_mask mask = {};
	int err;

	if (!tca[TCA_OPTIONS])
		return -EINVAL;

	tb = kcalloc(TCA_FLOWER_MAX + 1, sizeof(struct nlattr *), GFP_KERNEL);
	if (!tb)
		return -ENOBUFS;

	err = nla_parse_nested(tb, TCA_FLOWER_MAX, tca[TCA_OPTIONS],
			       fl_policy, NULL);
	if (err < 0)
		goto errout_tb;

	if (fold && handle && fold->handle != handle) {
		err = -EINVAL;
		goto errout_tb;
	}

	fnew = kzalloc(sizeof(*fnew), GFP_KERNEL);
	if (!fnew) {
		err = -ENOBUFS;
		goto errout_tb;
	}

	err = tcf_exts_init(&fnew->exts, TCA_FLOWER_ACT, 0);
	if (err < 0)
		goto errout;

	if (!handle) {
		handle = 1;
		err = idr_alloc_u32(&head->handle_idr, fnew, &handle,
				    INT_MAX, GFP_KERNEL);
	} else if (!fold) {
		/* user specifies a handle and it doesn't exist */
		err = idr_alloc_u32(&head->handle_idr, fnew, &handle,
				    handle, GFP_KERNEL);
	}
	if (err)
		goto errout;
	fnew->handle = handle;

	if (tb[TCA_FLOWER_FLAGS]) {
		fnew->flags = nla_get_u32(tb[TCA_FLOWER_FLAGS]);

		if (!tc_flags_valid(fnew->flags)) {
			err = -EINVAL;
			goto errout_idr;
		}
	}

	err = fl_set_parms(net, tp, fnew, &mask, base, tb, tca[TCA_RATE], ovr,
			   extack);
	if (err)
		goto errout_idr;

	err = fl_check_assign_mask(head, fnew, fold, &mask);
	if (err)
		goto errout_idr;

	if (!tc_skip_sw(fnew->flags)) {
		if (!fold && fl_lookup(fnew->mask, &fnew->mkey)) {
			err = -EEXIST;
			goto errout_mask;
		}

		err = rhashtable_insert_fast(&fnew->mask->ht, &fnew->ht_node,
					     fnew->mask->filter_ht_params);
		if (err)
			goto errout_mask;
	}

	if (!tc_skip_hw(fnew->flags)) {
		err = fl_hw_replace_filter(tp, fnew, extack);
		if (err)
			goto errout_mask;
	}

	if (!tc_in_hw(fnew->flags))
		fnew->flags |= TCA_CLS_FLAGS_NOT_IN_HW;

	if (fold) {
		if (!tc_skip_sw(fold->flags))
			rhashtable_remove_fast(&fold->mask->ht,
					       &fold->ht_node,
					       fold->mask->filter_ht_params);
		if (!tc_skip_hw(fold->flags))
			fl_hw_destroy_filter(tp, fold, NULL);
	}

	*arg = fnew;

	if (fold) {
		idr_replace(&head->handle_idr, fnew, fnew->handle);
		list_replace_rcu(&fold->list, &fnew->list);
		tcf_unbind_filter(tp, &fold->res);
		tcf_exts_get_net(&fold->exts);
		tcf_queue_work(&fold->rwork, fl_destroy_filter_work);
	} else {
		list_add_tail_rcu(&fnew->list, &fnew->mask->filters);
	}

	kfree(tb);
	return 0;

errout_mask:
	fl_mask_put(head, fnew->mask, false);

errout_idr:
	if (!fold)
		idr_remove(&head->handle_idr, fnew->handle);
errout:
	tcf_exts_destroy(&fnew->exts);
	kfree(fnew);
errout_tb:
	kfree(tb);
	return err;
}

static int fl_delete(struct tcf_proto *tp, void *arg, bool *last,
		     struct netlink_ext_ack *extack)
{
	struct cls_fl_head *head = rtnl_dereference(tp->root);
	struct cls_fl_filter *f = arg;

	if (!tc_skip_sw(f->flags))
		rhashtable_remove_fast(&f->mask->ht, &f->ht_node,
				       f->mask->filter_ht_params);
	__fl_delete(tp, f, extack);
	*last = list_empty(&head->masks);
	return 0;
}

static void fl_walk(struct tcf_proto *tp, struct tcf_walker *arg)
{
	struct cls_fl_head *head = rtnl_dereference(tp->root);
	struct cls_fl_filter *f;
	struct fl_flow_mask *mask;

	list_for_each_entry_rcu(mask, &head->masks, list) {
		list_for_each_entry_rcu(f, &mask->filters, list) {
			if (arg->count < arg->skip)
				goto skip;
			if (arg->fn(tp, f, arg) < 0) {
				arg->stop = 1;
				break;
			}
skip:
			arg->count++;
		}
	}
}

static int fl_dump_key_val(struct sk_buff *skb,
			   void *val, int val_type,
			   void *mask, int mask_type, int len)
{
	int err;

	if (!memchr_inv(mask, 0, len))
		return 0;
	err = nla_put(skb, val_type, len, val);
	if (err)
		return err;
	if (mask_type != TCA_FLOWER_UNSPEC) {
		err = nla_put(skb, mask_type, len, mask);
		if (err)
			return err;
	}
	return 0;
}

static int fl_dump_key_mpls(struct sk_buff *skb,
			    struct flow_dissector_key_mpls *mpls_key,
			    struct flow_dissector_key_mpls *mpls_mask)
{
	int err;

	if (!memchr_inv(mpls_mask, 0, sizeof(*mpls_mask)))
		return 0;
	if (mpls_mask->mpls_ttl) {
		err = nla_put_u8(skb, TCA_FLOWER_KEY_MPLS_TTL,
				 mpls_key->mpls_ttl);
		if (err)
			return err;
	}
	if (mpls_mask->mpls_tc) {
		err = nla_put_u8(skb, TCA_FLOWER_KEY_MPLS_TC,
				 mpls_key->mpls_tc);
		if (err)
			return err;
	}
	if (mpls_mask->mpls_label) {
		err = nla_put_u32(skb, TCA_FLOWER_KEY_MPLS_LABEL,
				  mpls_key->mpls_label);
		if (err)
			return err;
	}
	if (mpls_mask->mpls_bos) {
		err = nla_put_u8(skb, TCA_FLOWER_KEY_MPLS_BOS,
				 mpls_key->mpls_bos);
		if (err)
			return err;
	}
	return 0;
}

static int fl_dump_key_ip(struct sk_buff *skb,
			  struct flow_dissector_key_ip *key,
			  struct flow_dissector_key_ip *mask)
{
	if (fl_dump_key_val(skb, &key->tos, TCA_FLOWER_KEY_IP_TOS, &mask->tos,
			    TCA_FLOWER_KEY_IP_TOS_MASK, sizeof(key->tos)) ||
	    fl_dump_key_val(skb, &key->ttl, TCA_FLOWER_KEY_IP_TTL, &mask->ttl,
			    TCA_FLOWER_KEY_IP_TTL_MASK, sizeof(key->ttl)))
		return -1;

	return 0;
}

static int fl_dump_key_vlan(struct sk_buff *skb,
			    struct flow_dissector_key_vlan *vlan_key,
			    struct flow_dissector_key_vlan *vlan_mask)
{
	int err;

	if (!memchr_inv(vlan_mask, 0, sizeof(*vlan_mask)))
		return 0;
	if (vlan_mask->vlan_id) {
		err = nla_put_u16(skb, TCA_FLOWER_KEY_VLAN_ID,
				  vlan_key->vlan_id);
		if (err)
			return err;
	}
	if (vlan_mask->vlan_priority) {
		err = nla_put_u8(skb, TCA_FLOWER_KEY_VLAN_PRIO,
				 vlan_key->vlan_priority);
		if (err)
			return err;
	}
	return 0;
}

static void fl_get_key_flag(u32 dissector_key, u32 dissector_mask,
			    u32 *flower_key, u32 *flower_mask,
			    u32 flower_flag_bit, u32 dissector_flag_bit)
{
	if (dissector_mask & dissector_flag_bit) {
		*flower_mask |= flower_flag_bit;
		if (dissector_key & dissector_flag_bit)
			*flower_key |= flower_flag_bit;
	}
}

static int fl_dump_key_flags(struct sk_buff *skb, u32 flags_key, u32 flags_mask)
{
	u32 key, mask;
	__be32 _key, _mask;
	int err;

	if (!memchr_inv(&flags_mask, 0, sizeof(flags_mask)))
		return 0;

	key = 0;
	mask = 0;

	fl_get_key_flag(flags_key, flags_mask, &key, &mask,
			TCA_FLOWER_KEY_FLAGS_IS_FRAGMENT, FLOW_DIS_IS_FRAGMENT);
	fl_get_key_flag(flags_key, flags_mask, &key, &mask,
			TCA_FLOWER_KEY_FLAGS_FRAG_IS_FIRST,
			FLOW_DIS_FIRST_FRAG);

	_key = cpu_to_be32(key);
	_mask = cpu_to_be32(mask);

	err = nla_put(skb, TCA_FLOWER_KEY_FLAGS, 4, &_key);
	if (err)
		return err;

	return nla_put(skb, TCA_FLOWER_KEY_FLAGS_MASK, 4, &_mask);
}

static int fl_dump(struct net *net, struct tcf_proto *tp, void *fh,
		   struct sk_buff *skb, struct tcmsg *t)
{
	struct cls_fl_filter *f = fh;
	struct nlattr *nest;
	struct fl_flow_key *key, *mask;

	if (!f)
		return skb->len;

	t->tcm_handle = f->handle;

	nest = nla_nest_start(skb, TCA_OPTIONS);
	if (!nest)
		goto nla_put_failure;

	if (f->res.classid &&
	    nla_put_u32(skb, TCA_FLOWER_CLASSID, f->res.classid))
		goto nla_put_failure;

	key = &f->key;
	mask = &f->mask->key;

	if (mask->indev_ifindex) {
		struct net_device *dev;

		dev = __dev_get_by_index(net, key->indev_ifindex);
		if (dev && nla_put_string(skb, TCA_FLOWER_INDEV, dev->name))
			goto nla_put_failure;
	}

	if (!tc_skip_hw(f->flags))
		fl_hw_update_stats(tp, f);

	if (fl_dump_key_val(skb, key->eth.dst, TCA_FLOWER_KEY_ETH_DST,
			    mask->eth.dst, TCA_FLOWER_KEY_ETH_DST_MASK,
			    sizeof(key->eth.dst)) ||
	    fl_dump_key_val(skb, key->eth.src, TCA_FLOWER_KEY_ETH_SRC,
			    mask->eth.src, TCA_FLOWER_KEY_ETH_SRC_MASK,
			    sizeof(key->eth.src)) ||
	    fl_dump_key_val(skb, &key->basic.n_proto, TCA_FLOWER_KEY_ETH_TYPE,
			    &mask->basic.n_proto, TCA_FLOWER_UNSPEC,
			    sizeof(key->basic.n_proto)))
		goto nla_put_failure;

	if (fl_dump_key_mpls(skb, &key->mpls, &mask->mpls))
		goto nla_put_failure;

	if (fl_dump_key_vlan(skb, &key->vlan, &mask->vlan))
		goto nla_put_failure;

	if ((key->basic.n_proto == htons(ETH_P_IP) ||
	     key->basic.n_proto == htons(ETH_P_IPV6)) &&
	    (fl_dump_key_val(skb, &key->basic.ip_proto, TCA_FLOWER_KEY_IP_PROTO,
			    &mask->basic.ip_proto, TCA_FLOWER_UNSPEC,
			    sizeof(key->basic.ip_proto)) ||
	    fl_dump_key_ip(skb, &key->ip, &mask->ip)))
		goto nla_put_failure;

	if (key->control.addr_type == FLOW_DISSECTOR_KEY_IPV4_ADDRS &&
	    (fl_dump_key_val(skb, &key->ipv4.src, TCA_FLOWER_KEY_IPV4_SRC,
			     &mask->ipv4.src, TCA_FLOWER_KEY_IPV4_SRC_MASK,
			     sizeof(key->ipv4.src)) ||
	     fl_dump_key_val(skb, &key->ipv4.dst, TCA_FLOWER_KEY_IPV4_DST,
			     &mask->ipv4.dst, TCA_FLOWER_KEY_IPV4_DST_MASK,
			     sizeof(key->ipv4.dst))))
		goto nla_put_failure;
	else if (key->control.addr_type == FLOW_DISSECTOR_KEY_IPV6_ADDRS &&
		 (fl_dump_key_val(skb, &key->ipv6.src, TCA_FLOWER_KEY_IPV6_SRC,
				  &mask->ipv6.src, TCA_FLOWER_KEY_IPV6_SRC_MASK,
				  sizeof(key->ipv6.src)) ||
		  fl_dump_key_val(skb, &key->ipv6.dst, TCA_FLOWER_KEY_IPV6_DST,
				  &mask->ipv6.dst, TCA_FLOWER_KEY_IPV6_DST_MASK,
				  sizeof(key->ipv6.dst))))
		goto nla_put_failure;

	if (key->basic.ip_proto == IPPROTO_TCP &&
	    (fl_dump_key_val(skb, &key->tp.src, TCA_FLOWER_KEY_TCP_SRC,
			     &mask->tp.src, TCA_FLOWER_KEY_TCP_SRC_MASK,
			     sizeof(key->tp.src)) ||
	     fl_dump_key_val(skb, &key->tp.dst, TCA_FLOWER_KEY_TCP_DST,
			     &mask->tp.dst, TCA_FLOWER_KEY_TCP_DST_MASK,
			     sizeof(key->tp.dst)) ||
	     fl_dump_key_val(skb, &key->tcp.flags, TCA_FLOWER_KEY_TCP_FLAGS,
			     &mask->tcp.flags, TCA_FLOWER_KEY_TCP_FLAGS_MASK,
			     sizeof(key->tcp.flags))))
		goto nla_put_failure;
	else if (key->basic.ip_proto == IPPROTO_UDP &&
		 (fl_dump_key_val(skb, &key->tp.src, TCA_FLOWER_KEY_UDP_SRC,
				  &mask->tp.src, TCA_FLOWER_KEY_UDP_SRC_MASK,
				  sizeof(key->tp.src)) ||
		  fl_dump_key_val(skb, &key->tp.dst, TCA_FLOWER_KEY_UDP_DST,
				  &mask->tp.dst, TCA_FLOWER_KEY_UDP_DST_MASK,
				  sizeof(key->tp.dst))))
		goto nla_put_failure;
	else if (key->basic.ip_proto == IPPROTO_SCTP &&
		 (fl_dump_key_val(skb, &key->tp.src, TCA_FLOWER_KEY_SCTP_SRC,
				  &mask->tp.src, TCA_FLOWER_KEY_SCTP_SRC_MASK,
				  sizeof(key->tp.src)) ||
		  fl_dump_key_val(skb, &key->tp.dst, TCA_FLOWER_KEY_SCTP_DST,
				  &mask->tp.dst, TCA_FLOWER_KEY_SCTP_DST_MASK,
				  sizeof(key->tp.dst))))
		goto nla_put_failure;
	else if (key->basic.n_proto == htons(ETH_P_IP) &&
		 key->basic.ip_proto == IPPROTO_ICMP &&
		 (fl_dump_key_val(skb, &key->icmp.type,
				  TCA_FLOWER_KEY_ICMPV4_TYPE, &mask->icmp.type,
				  TCA_FLOWER_KEY_ICMPV4_TYPE_MASK,
				  sizeof(key->icmp.type)) ||
		  fl_dump_key_val(skb, &key->icmp.code,
				  TCA_FLOWER_KEY_ICMPV4_CODE, &mask->icmp.code,
				  TCA_FLOWER_KEY_ICMPV4_CODE_MASK,
				  sizeof(key->icmp.code))))
		goto nla_put_failure;
	else if (key->basic.n_proto == htons(ETH_P_IPV6) &&
		 key->basic.ip_proto == IPPROTO_ICMPV6 &&
		 (fl_dump_key_val(skb, &key->icmp.type,
				  TCA_FLOWER_KEY_ICMPV6_TYPE, &mask->icmp.type,
				  TCA_FLOWER_KEY_ICMPV6_TYPE_MASK,
				  sizeof(key->icmp.type)) ||
		  fl_dump_key_val(skb, &key->icmp.code,
				  TCA_FLOWER_KEY_ICMPV6_CODE, &mask->icmp.code,
				  TCA_FLOWER_KEY_ICMPV6_CODE_MASK,
				  sizeof(key->icmp.code))))
		goto nla_put_failure;
	else if ((key->basic.n_proto == htons(ETH_P_ARP) ||
		  key->basic.n_proto == htons(ETH_P_RARP)) &&
		 (fl_dump_key_val(skb, &key->arp.sip,
				  TCA_FLOWER_KEY_ARP_SIP, &mask->arp.sip,
				  TCA_FLOWER_KEY_ARP_SIP_MASK,
				  sizeof(key->arp.sip)) ||
		  fl_dump_key_val(skb, &key->arp.tip,
				  TCA_FLOWER_KEY_ARP_TIP, &mask->arp.tip,
				  TCA_FLOWER_KEY_ARP_TIP_MASK,
				  sizeof(key->arp.tip)) ||
		  fl_dump_key_val(skb, &key->arp.op,
				  TCA_FLOWER_KEY_ARP_OP, &mask->arp.op,
				  TCA_FLOWER_KEY_ARP_OP_MASK,
				  sizeof(key->arp.op)) ||
		  fl_dump_key_val(skb, key->arp.sha, TCA_FLOWER_KEY_ARP_SHA,
				  mask->arp.sha, TCA_FLOWER_KEY_ARP_SHA_MASK,
				  sizeof(key->arp.sha)) ||
		  fl_dump_key_val(skb, key->arp.tha, TCA_FLOWER_KEY_ARP_THA,
				  mask->arp.tha, TCA_FLOWER_KEY_ARP_THA_MASK,
				  sizeof(key->arp.tha))))
		goto nla_put_failure;

	if (key->enc_control.addr_type == FLOW_DISSECTOR_KEY_IPV4_ADDRS &&
	    (fl_dump_key_val(skb, &key->enc_ipv4.src,
			    TCA_FLOWER_KEY_ENC_IPV4_SRC, &mask->enc_ipv4.src,
			    TCA_FLOWER_KEY_ENC_IPV4_SRC_MASK,
			    sizeof(key->enc_ipv4.src)) ||
	     fl_dump_key_val(skb, &key->enc_ipv4.dst,
			     TCA_FLOWER_KEY_ENC_IPV4_DST, &mask->enc_ipv4.dst,
			     TCA_FLOWER_KEY_ENC_IPV4_DST_MASK,
			     sizeof(key->enc_ipv4.dst))))
		goto nla_put_failure;
	else if (key->enc_control.addr_type == FLOW_DISSECTOR_KEY_IPV6_ADDRS &&
		 (fl_dump_key_val(skb, &key->enc_ipv6.src,
			    TCA_FLOWER_KEY_ENC_IPV6_SRC, &mask->enc_ipv6.src,
			    TCA_FLOWER_KEY_ENC_IPV6_SRC_MASK,
			    sizeof(key->enc_ipv6.src)) ||
		 fl_dump_key_val(skb, &key->enc_ipv6.dst,
				 TCA_FLOWER_KEY_ENC_IPV6_DST,
				 &mask->enc_ipv6.dst,
				 TCA_FLOWER_KEY_ENC_IPV6_DST_MASK,
			    sizeof(key->enc_ipv6.dst))))
		goto nla_put_failure;

	if (fl_dump_key_val(skb, &key->enc_key_id, TCA_FLOWER_KEY_ENC_KEY_ID,
			    &mask->enc_key_id, TCA_FLOWER_UNSPEC,
			    sizeof(key->enc_key_id)) ||
	    fl_dump_key_val(skb, &key->enc_tp.src,
			    TCA_FLOWER_KEY_ENC_UDP_SRC_PORT,
			    &mask->enc_tp.src,
			    TCA_FLOWER_KEY_ENC_UDP_SRC_PORT_MASK,
			    sizeof(key->enc_tp.src)) ||
	    fl_dump_key_val(skb, &key->enc_tp.dst,
			    TCA_FLOWER_KEY_ENC_UDP_DST_PORT,
			    &mask->enc_tp.dst,
			    TCA_FLOWER_KEY_ENC_UDP_DST_PORT_MASK,
			    sizeof(key->enc_tp.dst)))
		goto nla_put_failure;

	if (fl_dump_key_flags(skb, key->control.flags, mask->control.flags))
		goto nla_put_failure;

	if (f->flags && nla_put_u32(skb, TCA_FLOWER_FLAGS, f->flags))
		goto nla_put_failure;

	if (tcf_exts_dump(skb, &f->exts))
		goto nla_put_failure;

	nla_nest_end(skb, nest);

	if (tcf_exts_dump_stats(skb, &f->exts) < 0)
		goto nla_put_failure;

	return skb->len;

nla_put_failure:
	nla_nest_cancel(skb, nest);
	return -1;
}

static void fl_bind_class(void *fh, u32 classid, unsigned long cl)
{
	struct cls_fl_filter *f = fh;

	if (f && f->res.classid == classid)
		f->res.class = cl;
}

static struct tcf_proto_ops cls_fl_ops __read_mostly = {
	.kind		= "flower",
	.classify	= fl_classify,
	.init		= fl_init,
	.destroy	= fl_destroy,
	.get		= fl_get,
	.change		= fl_change,
	.delete		= fl_delete,
	.walk		= fl_walk,
	.dump		= fl_dump,
	.bind_class	= fl_bind_class,
	.owner		= THIS_MODULE,
};

static int __init cls_fl_init(void)
{
	return register_tcf_proto_ops(&cls_fl_ops);
}

static void __exit cls_fl_exit(void)
{
	unregister_tcf_proto_ops(&cls_fl_ops);
}

module_init(cls_fl_init);
module_exit(cls_fl_exit);

MODULE_AUTHOR("Jiri Pirko <jiri@resnulli.us>");
MODULE_DESCRIPTION("Flower classifier");
MODULE_LICENSE("GPL v2");
