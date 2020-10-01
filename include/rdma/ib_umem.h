/* SPDX-License-Identifier: GPL-2.0 OR Linux-OpenIB */
/*
 * Copyright (c) 2007 Cisco Systems.  All rights reserved.
 */

#ifndef IB_UMEM_H
#define IB_UMEM_H

#include <linux/list.h>
#include <linux/scatterlist.h>
#include <linux/workqueue.h>
#include <rdma/ib_verbs.h>

struct ib_ucontext;
struct ib_umem_odp;

struct ib_umem {
	struct ib_device       *ibdev;
	struct mm_struct       *owning_mm;
	size_t			length;
	unsigned long		address;
	u32 writable : 1;
	u32 is_odp : 1;
	/* Placing at the end of the bitfield list is ABI preserving on LE */
	u32 is_peer : 1;
	struct work_struct	work;
	struct sg_table sg_head;
	int             nmap;
	unsigned int    sg_nents;
};

typedef void (*umem_invalidate_func_t)(struct ib_umem *umem, void *priv);
enum ib_peer_mem_flags {
	IB_PEER_MEM_ALLOW = 1 << 0,
	IB_PEER_MEM_INVAL_SUPP = 1 << 1,
};

/* Returns the offset of the umem start relative to the first page. */
static inline int ib_umem_offset(struct ib_umem *umem)
{
	return umem->address & ~PAGE_MASK;
}

static inline size_t ib_umem_num_pages(struct ib_umem *umem)
{
	return (ALIGN(umem->address + umem->length, PAGE_SIZE) -
		ALIGN_DOWN(umem->address, PAGE_SIZE)) >>
	       PAGE_SHIFT;
}

#ifdef CONFIG_INFINIBAND_USER_MEM

struct ib_umem *ib_umem_get(struct ib_device *device, unsigned long addr,
			    size_t size, int access);
void ib_umem_release(struct ib_umem *umem);
int ib_umem_page_count(struct ib_umem *umem);
int ib_umem_copy_from(void *dst, struct ib_umem *umem, size_t offset,
		      size_t length);
unsigned long ib_umem_find_best_pgsz(struct ib_umem *umem,
				     unsigned long pgsz_bitmap,
				     unsigned long virt);

struct ib_umem *ib_umem_get_peer(struct ib_device *device, unsigned long addr,
				 size_t size, int access,
				 unsigned long peer_mem_flags);
void ib_umem_activate_invalidation_notifier(struct ib_umem *umem,
					    umem_invalidate_func_t func,
					    void *cookie);

#else /* CONFIG_INFINIBAND_USER_MEM */

#include <linux/err.h>

static inline struct ib_umem *ib_umem_get(struct ib_device *device,
					  unsigned long addr, size_t size,
					  int access)
{
	return ERR_PTR(-EINVAL);
}
static inline void ib_umem_release(struct ib_umem *umem) { }
static inline int ib_umem_page_count(struct ib_umem *umem) { return 0; }
static inline int ib_umem_copy_from(void *dst, struct ib_umem *umem, size_t offset,
		      		    size_t length) {
	return -EINVAL;
}
static inline int ib_umem_find_best_pgsz(struct ib_umem *umem,
					 unsigned long pgsz_bitmap,
					 unsigned long virt) {
	return -EINVAL;
}

static inline struct ib_umem *ib_umem_get_peer(struct ib_udata *udata,
					       unsigned long addr, size_t size,
					       int access,
					       unsigned long peer_mem_flags)
{
	return ERR_PTR(-EINVAL);
}

static inline void ib_umem_activate_invalidation_notifier(
	struct ib_umem *umem, umem_invalidate_func_t func, void *cookie)
{
}

#endif /* CONFIG_INFINIBAND_USER_MEM */

#endif /* IB_UMEM_H */
