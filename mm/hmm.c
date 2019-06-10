// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright 2013 Red Hat Inc.
 *
 * Authors: Jérôme Glisse <jglisse@redhat.com>
 */
/*
 * Refer to include/linux/hmm.h for information about heterogeneous memory
 * management or HMM for short.
 */
#include <linux/mm.h>
#include <linux/hmm.h>
#include <linux/init.h>
#include <linux/rmap.h>
#include <linux/swap.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/mmzone.h>
#include <linux/pagemap.h>
#include <linux/swapops.h>
#include <linux/hugetlb.h>
#include <linux/memremap.h>
#include <linux/jump_label.h>
#include <linux/dma-mapping.h>
#include <linux/mmu_notifier.h>
#include <linux/memory_hotplug.h>

#define PA_SECTION_SIZE (1UL << PA_SECTION_SHIFT)

#if IS_ENABLED(CONFIG_HMM_MIRROR)
static const struct mmu_notifier_ops hmm_mmu_notifier_ops;

static inline struct hmm *mm_get_hmm(struct mm_struct *mm)
{
	struct hmm *hmm = READ_ONCE(mm->hmm);

	if (hmm && kref_get_unless_zero(&hmm->kref))
		return hmm;

	return NULL;
}

/**
 * hmm_get_or_create - register HMM against an mm (HMM internal)
 *
 * @mm: mm struct to attach to
 * Returns: returns an HMM object, either by referencing the existing
 *          (per-process) object, or by creating a new one.
 *
 * This is not intended to be used directly by device drivers. If mm already
 * has an HMM struct then it get a reference on it and returns it. Otherwise
 * it allocates an HMM struct, initializes it, associate it with the mm and
 * returns it.
 */
static struct hmm *hmm_get_or_create(struct mm_struct *mm)
{
	struct hmm *hmm = mm_get_hmm(mm);
	bool cleanup = false;

	if (hmm)
		return hmm;

	hmm = kmalloc(sizeof(*hmm), GFP_KERNEL);
	if (!hmm)
		return NULL;
	init_waitqueue_head(&hmm->wq);
	INIT_LIST_HEAD(&hmm->mirrors);
	init_rwsem(&hmm->mirrors_sem);
	hmm->mmu_notifier.ops = NULL;
	INIT_LIST_HEAD(&hmm->ranges);
	mutex_init(&hmm->lock);
	kref_init(&hmm->kref);
	hmm->notifiers = 0;
	hmm->dead = false;
	hmm->mm = mm;

	spin_lock(&mm->page_table_lock);
	if (!mm->hmm)
		mm->hmm = hmm;
	else
		cleanup = true;
	spin_unlock(&mm->page_table_lock);

	if (cleanup)
		goto error;

	/*
	 * We should only get here if hold the mmap_sem in write mode ie on
	 * registration of first mirror through hmm_mirror_register()
	 */
	hmm->mmu_notifier.ops = &hmm_mmu_notifier_ops;
	if (__mmu_notifier_register(&hmm->mmu_notifier, mm))
		goto error_mm;

	return hmm;

error_mm:
	spin_lock(&mm->page_table_lock);
	if (mm->hmm == hmm)
		mm->hmm = NULL;
	spin_unlock(&mm->page_table_lock);
error:
	kfree(hmm);
	return NULL;
}

static void hmm_free(struct kref *kref)
{
	struct hmm *hmm = container_of(kref, struct hmm, kref);
	struct mm_struct *mm = hmm->mm;

	mmu_notifier_unregister_no_release(&hmm->mmu_notifier, mm);

	spin_lock(&mm->page_table_lock);
	if (mm->hmm == hmm)
		mm->hmm = NULL;
	spin_unlock(&mm->page_table_lock);

	kfree(hmm);
}

static inline void hmm_put(struct hmm *hmm)
{
	kref_put(&hmm->kref, hmm_free);
}

void hmm_mm_destroy(struct mm_struct *mm)
{
	struct hmm *hmm;

	spin_lock(&mm->page_table_lock);
	hmm = mm_get_hmm(mm);
	mm->hmm = NULL;
	if (hmm) {
		hmm->mm = NULL;
		hmm->dead = true;
		spin_unlock(&mm->page_table_lock);
		hmm_put(hmm);
		return;
	}

	spin_unlock(&mm->page_table_lock);
}

static void hmm_release(struct mmu_notifier *mn, struct mm_struct *mm)
{
	struct hmm *hmm = mm_get_hmm(mm);
	struct hmm_mirror *mirror;
	struct hmm_range *range;

	/* Report this HMM as dying. */
	hmm->dead = true;

	/* Wake-up everyone waiting on any range. */
	mutex_lock(&hmm->lock);
	list_for_each_entry(range, &hmm->ranges, list) {
		range->valid = false;
	}
	wake_up_all(&hmm->wq);
	mutex_unlock(&hmm->lock);

	down_write(&hmm->mirrors_sem);
	mirror = list_first_entry_or_null(&hmm->mirrors, struct hmm_mirror,
					  list);
	while (mirror) {
		list_del_init(&mirror->list);
		if (mirror->ops->release) {
			/*
			 * Drop mirrors_sem so callback can wait on any pending
			 * work that might itself trigger mmu_notifier callback
			 * and thus would deadlock with us.
			 */
			up_write(&hmm->mirrors_sem);
			mirror->ops->release(mirror);
			down_write(&hmm->mirrors_sem);
		}
		mirror = list_first_entry_or_null(&hmm->mirrors,
						  struct hmm_mirror, list);
	}
	up_write(&hmm->mirrors_sem);

	hmm_put(hmm);
}

static int hmm_invalidate_range_start(struct mmu_notifier *mn,
			const struct mmu_notifier_range *nrange)
{
	struct hmm *hmm = mm_get_hmm(nrange->mm);
	struct hmm_mirror *mirror;
	struct hmm_update update;
	struct hmm_range *range;
	int ret = 0;

	VM_BUG_ON(!hmm);

	update.start = nrange->start;
	update.end = nrange->end;
	update.event = HMM_UPDATE_INVALIDATE;
	update.blockable = mmu_notifier_range_blockable(nrange);

	if (mmu_notifier_range_blockable(nrange))
		mutex_lock(&hmm->lock);
	else if (!mutex_trylock(&hmm->lock)) {
		ret = -EAGAIN;
		goto out;
	}
	hmm->notifiers++;
	list_for_each_entry(range, &hmm->ranges, list) {
		if (update.end < range->start || update.start >= range->end)
			continue;

		range->valid = false;
	}
	mutex_unlock(&hmm->lock);

	if (mmu_notifier_range_blockable(nrange))
		down_read(&hmm->mirrors_sem);
	else if (!down_read_trylock(&hmm->mirrors_sem)) {
		ret = -EAGAIN;
		goto out;
	}
	list_for_each_entry(mirror, &hmm->mirrors, list) {
		int ret;

		ret = mirror->ops->sync_cpu_device_pagetables(mirror, &update);
		if (!update.blockable && ret == -EAGAIN) {
			up_read(&hmm->mirrors_sem);
			ret = -EAGAIN;
			goto out;
		}
	}
	up_read(&hmm->mirrors_sem);

out:
	hmm_put(hmm);
	return ret;
}

static void hmm_invalidate_range_end(struct mmu_notifier *mn,
			const struct mmu_notifier_range *nrange)
{
	struct hmm *hmm = mm_get_hmm(nrange->mm);

	VM_BUG_ON(!hmm);

	mutex_lock(&hmm->lock);
	hmm->notifiers--;
	if (!hmm->notifiers) {
		struct hmm_range *range;

		list_for_each_entry(range, &hmm->ranges, list) {
			if (range->valid)
				continue;
			range->valid = true;
		}
		wake_up_all(&hmm->wq);
	}
	mutex_unlock(&hmm->lock);

	hmm_put(hmm);
}

static const struct mmu_notifier_ops hmm_mmu_notifier_ops = {
	.release		= hmm_release,
	.invalidate_range_start	= hmm_invalidate_range_start,
	.invalidate_range_end	= hmm_invalidate_range_end,
};

/*
 * hmm_mirror_register() - register a mirror against an mm
 *
 * @mirror: new mirror struct to register
 * @mm: mm to register against
 *
 * To start mirroring a process address space, the device driver must register
 * an HMM mirror struct.
 *
 * THE mm->mmap_sem MUST BE HELD IN WRITE MODE !
 */
int hmm_mirror_register(struct hmm_mirror *mirror, struct mm_struct *mm)
{
	/* Sanity check */
	if (!mm || !mirror || !mirror->ops)
		return -EINVAL;

	mirror->hmm = hmm_get_or_create(mm);
	if (!mirror->hmm)
		return -ENOMEM;

	down_write(&mirror->hmm->mirrors_sem);
	list_add(&mirror->list, &mirror->hmm->mirrors);
	up_write(&mirror->hmm->mirrors_sem);

	return 0;
}
EXPORT_SYMBOL(hmm_mirror_register);

/*
 * hmm_mirror_unregister() - unregister a mirror
 *
 * @mirror: new mirror struct to register
 *
 * Stop mirroring a process address space, and cleanup.
 */
void hmm_mirror_unregister(struct hmm_mirror *mirror)
{
	struct hmm *hmm = READ_ONCE(mirror->hmm);

	if (hmm == NULL)
		return;

	down_write(&hmm->mirrors_sem);
	list_del_init(&mirror->list);
	/* To protect us against double unregister ... */
	mirror->hmm = NULL;
	up_write(&hmm->mirrors_sem);

	hmm_put(hmm);
}
EXPORT_SYMBOL(hmm_mirror_unregister);

struct hmm_vma_walk {
	struct hmm_range	*range;
	struct dev_pagemap	*pgmap;
	unsigned long		last;
	bool			fault;
	bool			block;
};

static int hmm_vma_do_fault(struct mm_walk *walk, unsigned long addr,
			    bool write_fault, uint64_t *pfn)
{
	unsigned int flags = FAULT_FLAG_ALLOW_RETRY | FAULT_FLAG_REMOTE;
	struct hmm_vma_walk *hmm_vma_walk = walk->private;
	struct hmm_range *range = hmm_vma_walk->range;
	struct vm_area_struct *vma = walk->vma;
	vm_fault_t ret;

	flags |= hmm_vma_walk->block ? 0 : FAULT_FLAG_ALLOW_RETRY;
	flags |= write_fault ? FAULT_FLAG_WRITE : 0;
	ret = handle_mm_fault(vma, addr, flags);
	if (ret & VM_FAULT_RETRY)
		return -EAGAIN;
	if (ret & VM_FAULT_ERROR) {
		*pfn = range->values[HMM_PFN_ERROR];
		return -EFAULT;
	}

	return -EBUSY;
}

static int hmm_pfns_bad(unsigned long addr,
			unsigned long end,
			struct mm_walk *walk)
{
	struct hmm_vma_walk *hmm_vma_walk = walk->private;
	struct hmm_range *range = hmm_vma_walk->range;
	uint64_t *pfns = range->pfns;
	unsigned long i;

	i = (addr - range->start) >> PAGE_SHIFT;
	for (; addr < end; addr += PAGE_SIZE, i++)
		pfns[i] = range->values[HMM_PFN_ERROR];

	return 0;
}

/*
 * hmm_vma_walk_hole() - handle a range lacking valid pmd or pte(s)
 * @start: range virtual start address (inclusive)
 * @end: range virtual end address (exclusive)
 * @fault: should we fault or not ?
 * @write_fault: write fault ?
 * @walk: mm_walk structure
 * Returns: 0 on success, -EBUSY after page fault, or page fault error
 *
 * This function will be called whenever pmd_none() or pte_none() returns true,
 * or whenever there is no page directory covering the virtual address range.
 */
static int hmm_vma_walk_hole_(unsigned long addr, unsigned long end,
			      bool fault, bool write_fault,
			      struct mm_walk *walk)
{
	struct hmm_vma_walk *hmm_vma_walk = walk->private;
	struct hmm_range *range = hmm_vma_walk->range;
	uint64_t *pfns = range->pfns;
	unsigned long i, page_size;

	hmm_vma_walk->last = addr;
	page_size = hmm_range_page_size(range);
	i = (addr - range->start) >> range->page_shift;

	for (; addr < end; addr += page_size, i++) {
		pfns[i] = range->values[HMM_PFN_NONE];
		if (fault || write_fault) {
			int ret;

			ret = hmm_vma_do_fault(walk, addr, write_fault,
					       &pfns[i]);
			if (ret != -EBUSY)
				return ret;
		}
	}

	return (fault || write_fault) ? -EBUSY : 0;
}

static inline void hmm_pte_need_fault(const struct hmm_vma_walk *hmm_vma_walk,
				      uint64_t pfns, uint64_t cpu_flags,
				      bool *fault, bool *write_fault)
{
	struct hmm_range *range = hmm_vma_walk->range;

	if (!hmm_vma_walk->fault)
		return;

	/*
	 * So we not only consider the individual per page request we also
	 * consider the default flags requested for the range. The API can
	 * be use in 2 fashions. The first one where the HMM user coalesce
	 * multiple page fault into one request and set flags per pfns for
	 * of those faults. The second one where the HMM user want to pre-
	 * fault a range with specific flags. For the latter one it is a
	 * waste to have the user pre-fill the pfn arrays with a default
	 * flags value.
	 */
	pfns = (pfns & range->pfn_flags_mask) | range->default_flags;

	/* We aren't ask to do anything ... */
	if (!(pfns & range->flags[HMM_PFN_VALID]))
		return;
	/* If this is device memory than only fault if explicitly requested */
	if ((cpu_flags & range->flags[HMM_PFN_DEVICE_PRIVATE])) {
		/* Do we fault on device memory ? */
		if (pfns & range->flags[HMM_PFN_DEVICE_PRIVATE]) {
			*write_fault = pfns & range->flags[HMM_PFN_WRITE];
			*fault = true;
		}
		return;
	}

	/* If CPU page table is not valid then we need to fault */
	*fault = !(cpu_flags & range->flags[HMM_PFN_VALID]);
	/* Need to write fault ? */
	if ((pfns & range->flags[HMM_PFN_WRITE]) &&
	    !(cpu_flags & range->flags[HMM_PFN_WRITE])) {
		*write_fault = true;
		*fault = true;
	}
}

static void hmm_range_need_fault(const struct hmm_vma_walk *hmm_vma_walk,
				 const uint64_t *pfns, unsigned long npages,
				 uint64_t cpu_flags, bool *fault,
				 bool *write_fault)
{
	unsigned long i;

	if (!hmm_vma_walk->fault) {
		*fault = *write_fault = false;
		return;
	}

	*fault = *write_fault = false;
	for (i = 0; i < npages; ++i) {
		hmm_pte_need_fault(hmm_vma_walk, pfns[i], cpu_flags,
				   fault, write_fault);
		if ((*write_fault))
			return;
	}
}

static int hmm_vma_walk_hole(unsigned long addr, unsigned long end,
			     struct mm_walk *walk)
{
	struct hmm_vma_walk *hmm_vma_walk = walk->private;
	struct hmm_range *range = hmm_vma_walk->range;
	bool fault, write_fault;
	unsigned long i, npages;
	uint64_t *pfns;

	i = (addr - range->start) >> PAGE_SHIFT;
	npages = (end - addr) >> PAGE_SHIFT;
	pfns = &range->pfns[i];
	hmm_range_need_fault(hmm_vma_walk, pfns, npages,
			     0, &fault, &write_fault);
	return hmm_vma_walk_hole_(addr, end, fault, write_fault, walk);
}

static inline uint64_t pmd_to_hmm_pfn_flags(struct hmm_range *range, pmd_t pmd)
{
	if (pmd_protnone(pmd))
		return 0;
	return pmd_write(pmd) ? range->flags[HMM_PFN_VALID] |
				range->flags[HMM_PFN_WRITE] :
				range->flags[HMM_PFN_VALID];
}

static inline uint64_t pud_to_hmm_pfn_flags(struct hmm_range *range, pud_t pud)
{
	if (!pud_present(pud))
		return 0;
	return pud_write(pud) ? range->flags[HMM_PFN_VALID] |
				range->flags[HMM_PFN_WRITE] :
				range->flags[HMM_PFN_VALID];
}

static int hmm_vma_handle_pmd(struct mm_walk *walk,
			      unsigned long addr,
			      unsigned long end,
			      uint64_t *pfns,
			      pmd_t pmd)
{
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	struct hmm_vma_walk *hmm_vma_walk = walk->private;
	struct hmm_range *range = hmm_vma_walk->range;
	unsigned long pfn, npages, i;
	bool fault, write_fault;
	uint64_t cpu_flags;

	npages = (end - addr) >> PAGE_SHIFT;
	cpu_flags = pmd_to_hmm_pfn_flags(range, pmd);
	hmm_range_need_fault(hmm_vma_walk, pfns, npages, cpu_flags,
			     &fault, &write_fault);

	if (pmd_protnone(pmd) || fault || write_fault)
		return hmm_vma_walk_hole_(addr, end, fault, write_fault, walk);

	pfn = pmd_pfn(pmd) + pte_index(addr);
	for (i = 0; addr < end; addr += PAGE_SIZE, i++, pfn++) {
		if (pmd_devmap(pmd)) {
			hmm_vma_walk->pgmap = get_dev_pagemap(pfn,
					      hmm_vma_walk->pgmap);
			if (unlikely(!hmm_vma_walk->pgmap))
				return -EBUSY;
		}
		pfns[i] = hmm_device_entry_from_pfn(range, pfn) | cpu_flags;
	}
	if (hmm_vma_walk->pgmap) {
		put_dev_pagemap(hmm_vma_walk->pgmap);
		hmm_vma_walk->pgmap = NULL;
	}
	hmm_vma_walk->last = end;
	return 0;
#else
	/* If THP is not enabled then we should never reach that code ! */
	return -EINVAL;
#endif
}

static inline uint64_t pte_to_hmm_pfn_flags(struct hmm_range *range, pte_t pte)
{
	if (pte_none(pte) || !pte_present(pte))
		return 0;
	return pte_write(pte) ? range->flags[HMM_PFN_VALID] |
				range->flags[HMM_PFN_WRITE] :
				range->flags[HMM_PFN_VALID];
}

static int hmm_vma_handle_pte(struct mm_walk *walk, unsigned long addr,
			      unsigned long end, pmd_t *pmdp, pte_t *ptep,
			      uint64_t *pfn)
{
	struct hmm_vma_walk *hmm_vma_walk = walk->private;
	struct hmm_range *range = hmm_vma_walk->range;
	struct vm_area_struct *vma = walk->vma;
	bool fault, write_fault;
	uint64_t cpu_flags;
	pte_t pte = *ptep;
	uint64_t orig_pfn = *pfn;

	*pfn = range->values[HMM_PFN_NONE];
	fault = write_fault = false;

	if (pte_none(pte)) {
		hmm_pte_need_fault(hmm_vma_walk, orig_pfn, 0,
				   &fault, &write_fault);
		if (fault || write_fault)
			goto fault;
		return 0;
	}

	if (!pte_present(pte)) {
		swp_entry_t entry = pte_to_swp_entry(pte);

		if (!non_swap_entry(entry)) {
			if (fault || write_fault)
				goto fault;
			return 0;
		}

		/*
		 * This is a special swap entry, ignore migration, use
		 * device and report anything else as error.
		 */
		if (is_device_private_entry(entry)) {
			cpu_flags = range->flags[HMM_PFN_VALID] |
				range->flags[HMM_PFN_DEVICE_PRIVATE];
			cpu_flags |= is_write_device_private_entry(entry) ?
				range->flags[HMM_PFN_WRITE] : 0;
			hmm_pte_need_fault(hmm_vma_walk, orig_pfn, cpu_flags,
					   &fault, &write_fault);
			if (fault || write_fault)
				goto fault;
			*pfn = hmm_device_entry_from_pfn(range,
					    swp_offset(entry));
			*pfn |= cpu_flags;
			return 0;
		}

		if (is_migration_entry(entry)) {
			if (fault || write_fault) {
				pte_unmap(ptep);
				hmm_vma_walk->last = addr;
				migration_entry_wait(vma->vm_mm,
						     pmdp, addr);
				return -EBUSY;
			}
			return 0;
		}

		/* Report error for everything else */
		*pfn = range->values[HMM_PFN_ERROR];
		return -EFAULT;
	} else {
		cpu_flags = pte_to_hmm_pfn_flags(range, pte);
		hmm_pte_need_fault(hmm_vma_walk, orig_pfn, cpu_flags,
				   &fault, &write_fault);
	}

	if (fault || write_fault)
		goto fault;

	if (pte_devmap(pte)) {
		hmm_vma_walk->pgmap = get_dev_pagemap(pte_pfn(pte),
					      hmm_vma_walk->pgmap);
		if (unlikely(!hmm_vma_walk->pgmap))
			return -EBUSY;
	} else if (IS_ENABLED(CONFIG_ARCH_HAS_PTE_SPECIAL) && pte_special(pte)) {
		*pfn = range->values[HMM_PFN_SPECIAL];
		return -EFAULT;
	}

	*pfn = hmm_device_entry_from_pfn(range, pte_pfn(pte)) | cpu_flags;
	return 0;

fault:
	if (hmm_vma_walk->pgmap) {
		put_dev_pagemap(hmm_vma_walk->pgmap);
		hmm_vma_walk->pgmap = NULL;
	}
	pte_unmap(ptep);
	/* Fault any virtual address we were asked to fault */
	return hmm_vma_walk_hole_(addr, end, fault, write_fault, walk);
}

static int hmm_vma_walk_pmd(pmd_t *pmdp,
			    unsigned long start,
			    unsigned long end,
			    struct mm_walk *walk)
{
	struct hmm_vma_walk *hmm_vma_walk = walk->private;
	struct hmm_range *range = hmm_vma_walk->range;
	struct vm_area_struct *vma = walk->vma;
	uint64_t *pfns = range->pfns;
	unsigned long addr = start, i;
	pte_t *ptep;
	pmd_t pmd;


again:
	pmd = READ_ONCE(*pmdp);
	if (pmd_none(pmd))
		return hmm_vma_walk_hole(start, end, walk);

	if (pmd_huge(pmd) && (range->vma->vm_flags & VM_HUGETLB))
		return hmm_pfns_bad(start, end, walk);

	if (thp_migration_supported() && is_pmd_migration_entry(pmd)) {
		bool fault, write_fault;
		unsigned long npages;
		uint64_t *pfns;

		i = (addr - range->start) >> PAGE_SHIFT;
		npages = (end - addr) >> PAGE_SHIFT;
		pfns = &range->pfns[i];

		hmm_range_need_fault(hmm_vma_walk, pfns, npages,
				     0, &fault, &write_fault);
		if (fault || write_fault) {
			hmm_vma_walk->last = addr;
			pmd_migration_entry_wait(vma->vm_mm, pmdp);
			return -EBUSY;
		}
		return 0;
	} else if (!pmd_present(pmd))
		return hmm_pfns_bad(start, end, walk);

	if (pmd_devmap(pmd) || pmd_trans_huge(pmd)) {
		/*
		 * No need to take pmd_lock here, even if some other threads
		 * is splitting the huge pmd we will get that event through
		 * mmu_notifier callback.
		 *
		 * So just read pmd value and check again its a transparent
		 * huge or device mapping one and compute corresponding pfn
		 * values.
		 */
		pmd = pmd_read_atomic(pmdp);
		barrier();
		if (!pmd_devmap(pmd) && !pmd_trans_huge(pmd))
			goto again;

		i = (addr - range->start) >> PAGE_SHIFT;
		return hmm_vma_handle_pmd(walk, addr, end, &pfns[i], pmd);
	}

	/*
	 * We have handled all the valid case above ie either none, migration,
	 * huge or transparent huge. At this point either it is a valid pmd
	 * entry pointing to pte directory or it is a bad pmd that will not
	 * recover.
	 */
	if (pmd_bad(pmd))
		return hmm_pfns_bad(start, end, walk);

	ptep = pte_offset_map(pmdp, addr);
	i = (addr - range->start) >> PAGE_SHIFT;
	for (; addr < end; addr += PAGE_SIZE, ptep++, i++) {
		int r;

		r = hmm_vma_handle_pte(walk, addr, end, pmdp, ptep, &pfns[i]);
		if (r) {
			/* hmm_vma_handle_pte() did unmap pte directory */
			hmm_vma_walk->last = addr;
			return r;
		}
	}
	if (hmm_vma_walk->pgmap) {
		/*
		 * We do put_dev_pagemap() here and not in hmm_vma_handle_pte()
		 * so that we can leverage get_dev_pagemap() optimization which
		 * will not re-take a reference on a pgmap if we already have
		 * one.
		 */
		put_dev_pagemap(hmm_vma_walk->pgmap);
		hmm_vma_walk->pgmap = NULL;
	}
	pte_unmap(ptep - 1);

	hmm_vma_walk->last = addr;
	return 0;
}

static int hmm_vma_walk_pud(pud_t *pudp,
			    unsigned long start,
			    unsigned long end,
			    struct mm_walk *walk)
{
	struct hmm_vma_walk *hmm_vma_walk = walk->private;
	struct hmm_range *range = hmm_vma_walk->range;
	unsigned long addr = start, next;
	pmd_t *pmdp;
	pud_t pud;
	int ret;

again:
	pud = READ_ONCE(*pudp);
	if (pud_none(pud))
		return hmm_vma_walk_hole(start, end, walk);

	if (pud_huge(pud) && pud_devmap(pud)) {
		unsigned long i, npages, pfn;
		uint64_t *pfns, cpu_flags;
		bool fault, write_fault;

		if (!pud_present(pud))
			return hmm_vma_walk_hole(start, end, walk);

		i = (addr - range->start) >> PAGE_SHIFT;
		npages = (end - addr) >> PAGE_SHIFT;
		pfns = &range->pfns[i];

		cpu_flags = pud_to_hmm_pfn_flags(range, pud);
		hmm_range_need_fault(hmm_vma_walk, pfns, npages,
				     cpu_flags, &fault, &write_fault);
		if (fault || write_fault)
			return hmm_vma_walk_hole_(addr, end, fault,
						write_fault, walk);

#ifdef CONFIG_HUGETLB_PAGE
		pfn = pud_pfn(pud) + ((addr & ~PUD_MASK) >> PAGE_SHIFT);
		for (i = 0; i < npages; ++i, ++pfn) {
			hmm_vma_walk->pgmap = get_dev_pagemap(pfn,
					      hmm_vma_walk->pgmap);
			if (unlikely(!hmm_vma_walk->pgmap))
				return -EBUSY;
			pfns[i] = hmm_device_entry_from_pfn(range, pfn) |
				  cpu_flags;
		}
		if (hmm_vma_walk->pgmap) {
			put_dev_pagemap(hmm_vma_walk->pgmap);
			hmm_vma_walk->pgmap = NULL;
		}
		hmm_vma_walk->last = end;
		return 0;
#else
		return -EINVAL;
#endif
	}

	split_huge_pud(walk->vma, pudp, addr);
	if (pud_none(*pudp))
		goto again;

	pmdp = pmd_offset(pudp, addr);
	do {
		next = pmd_addr_end(addr, end);
		ret = hmm_vma_walk_pmd(pmdp, addr, next, walk);
		if (ret)
			return ret;
	} while (pmdp++, addr = next, addr != end);

	return 0;
}

static int hmm_vma_walk_hugetlb_entry(pte_t *pte, unsigned long hmask,
				      unsigned long start, unsigned long end,
				      struct mm_walk *walk)
{
#ifdef CONFIG_HUGETLB_PAGE
	unsigned long addr = start, i, pfn, mask, size, pfn_inc;
	struct hmm_vma_walk *hmm_vma_walk = walk->private;
	struct hmm_range *range = hmm_vma_walk->range;
	struct vm_area_struct *vma = walk->vma;
	struct hstate *h = hstate_vma(vma);
	uint64_t orig_pfn, cpu_flags;
	bool fault, write_fault;
	spinlock_t *ptl;
	pte_t entry;
	int ret = 0;

	size = 1UL << huge_page_shift(h);
	mask = size - 1;
	if (range->page_shift != PAGE_SHIFT) {
		/* Make sure we are looking at full page. */
		if (start & mask)
			return -EINVAL;
		if (end < (start + size))
			return -EINVAL;
		pfn_inc = size >> PAGE_SHIFT;
	} else {
		pfn_inc = 1;
		size = PAGE_SIZE;
	}


	ptl = huge_pte_lock(hstate_vma(walk->vma), walk->mm, pte);
	entry = huge_ptep_get(pte);

	i = (start - range->start) >> range->page_shift;
	orig_pfn = range->pfns[i];
	range->pfns[i] = range->values[HMM_PFN_NONE];
	cpu_flags = pte_to_hmm_pfn_flags(range, entry);
	fault = write_fault = false;
	hmm_pte_need_fault(hmm_vma_walk, orig_pfn, cpu_flags,
			   &fault, &write_fault);
	if (fault || write_fault) {
		ret = -ENOENT;
		goto unlock;
	}

	pfn = pte_pfn(entry) + ((start & mask) >> range->page_shift);
	for (; addr < end; addr += size, i++, pfn += pfn_inc)
		range->pfns[i] = hmm_device_entry_from_pfn(range, pfn) |
				 cpu_flags;
	hmm_vma_walk->last = end;

unlock:
	spin_unlock(ptl);

	if (ret == -ENOENT)
		return hmm_vma_walk_hole_(addr, end, fault, write_fault, walk);

	return ret;
#else /* CONFIG_HUGETLB_PAGE */
	return -EINVAL;
#endif
}

static void hmm_pfns_clear(struct hmm_range *range,
			   uint64_t *pfns,
			   unsigned long addr,
			   unsigned long end)
{
	for (; addr < end; addr += PAGE_SIZE, pfns++)
		*pfns = range->values[HMM_PFN_NONE];
}

/*
 * hmm_range_register() - start tracking change to CPU page table over a range
 * @range: range
 * @mm: the mm struct for the range of virtual address
 * @start: start virtual address (inclusive)
 * @end: end virtual address (exclusive)
 * @page_shift: expect page shift for the range
 * Returns 0 on success, -EFAULT if the address space is no longer valid
 *
 * Track updates to the CPU page table see include/linux/hmm.h
 */
int hmm_range_register(struct hmm_range *range,
		       struct mm_struct *mm,
		       unsigned long start,
		       unsigned long end,
		       unsigned page_shift)
{
	unsigned long mask = ((1UL << page_shift) - 1UL);

	range->valid = false;
	range->hmm = NULL;

	if ((start & mask) || (end & mask))
		return -EINVAL;
	if (start >= end)
		return -EINVAL;

	range->page_shift = page_shift;
	range->start = start;
	range->end = end;

	range->hmm = hmm_get_or_create(mm);
	if (!range->hmm)
		return -EFAULT;

	/* Check if hmm_mm_destroy() was call. */
	if (range->hmm->mm == NULL || range->hmm->dead) {
		hmm_put(range->hmm);
		return -EFAULT;
	}

	/* Initialize range to track CPU page table update */
	mutex_lock(&range->hmm->lock);

	list_add_rcu(&range->list, &range->hmm->ranges);

	/*
	 * If there are any concurrent notifiers we have to wait for them for
	 * the range to be valid (see hmm_range_wait_until_valid()).
	 */
	if (!range->hmm->notifiers)
		range->valid = true;
	mutex_unlock(&range->hmm->lock);

	return 0;
}
EXPORT_SYMBOL(hmm_range_register);

/*
 * hmm_range_unregister() - stop tracking change to CPU page table over a range
 * @range: range
 *
 * Range struct is used to track updates to the CPU page table after a call to
 * hmm_range_register(). See include/linux/hmm.h for how to use it.
 */
void hmm_range_unregister(struct hmm_range *range)
{
	/* Sanity check this really should not happen. */
	if (range->hmm == NULL || range->end <= range->start)
		return;

	mutex_lock(&range->hmm->lock);
	list_del_rcu(&range->list);
	mutex_unlock(&range->hmm->lock);

	/* Drop reference taken by hmm_range_register() */
	range->valid = false;
	hmm_put(range->hmm);
	range->hmm = NULL;
}
EXPORT_SYMBOL(hmm_range_unregister);

/*
 * hmm_range_snapshot() - snapshot CPU page table for a range
 * @range: range
 * Returns: -EINVAL if invalid argument, -ENOMEM out of memory, -EPERM invalid
 *          permission (for instance asking for write and range is read only),
 *          -EAGAIN if you need to retry, -EFAULT invalid (ie either no valid
 *          vma or it is illegal to access that range), number of valid pages
 *          in range->pfns[] (from range start address).
 *
 * This snapshots the CPU page table for a range of virtual addresses. Snapshot
 * validity is tracked by range struct. See in include/linux/hmm.h for example
 * on how to use.
 */
long hmm_range_snapshot(struct hmm_range *range)
{
	const unsigned long device_vma = VM_IO | VM_PFNMAP | VM_MIXEDMAP;
	unsigned long start = range->start, end;
	struct hmm_vma_walk hmm_vma_walk;
	struct hmm *hmm = range->hmm;
	struct vm_area_struct *vma;
	struct mm_walk mm_walk;

	/* Check if hmm_mm_destroy() was call. */
	if (hmm->mm == NULL || hmm->dead)
		return -EFAULT;

	do {
		/* If range is no longer valid force retry. */
		if (!range->valid)
			return -EAGAIN;

		vma = find_vma(hmm->mm, start);
		if (vma == NULL || (vma->vm_flags & device_vma))
			return -EFAULT;

		if (is_vm_hugetlb_page(vma)) {
			struct hstate *h = hstate_vma(vma);

			if (huge_page_shift(h) != range->page_shift &&
			    range->page_shift != PAGE_SHIFT)
				return -EINVAL;
		} else {
			if (range->page_shift != PAGE_SHIFT)
				return -EINVAL;
		}

		if (!(vma->vm_flags & VM_READ)) {
			/*
			 * If vma do not allow read access, then assume that it
			 * does not allow write access, either. HMM does not
			 * support architecture that allow write without read.
			 */
			hmm_pfns_clear(range, range->pfns,
				range->start, range->end);
			return -EPERM;
		}

		range->vma = vma;
		hmm_vma_walk.pgmap = NULL;
		hmm_vma_walk.last = start;
		hmm_vma_walk.fault = false;
		hmm_vma_walk.range = range;
		mm_walk.private = &hmm_vma_walk;
		end = min(range->end, vma->vm_end);

		mm_walk.vma = vma;
		mm_walk.mm = vma->vm_mm;
		mm_walk.pte_entry = NULL;
		mm_walk.test_walk = NULL;
		mm_walk.hugetlb_entry = NULL;
		mm_walk.pud_entry = hmm_vma_walk_pud;
		mm_walk.pmd_entry = hmm_vma_walk_pmd;
		mm_walk.pte_hole = hmm_vma_walk_hole;
		mm_walk.hugetlb_entry = hmm_vma_walk_hugetlb_entry;

		walk_page_range(start, end, &mm_walk);
		start = end;
	} while (start < range->end);

	return (hmm_vma_walk.last - range->start) >> PAGE_SHIFT;
}
EXPORT_SYMBOL(hmm_range_snapshot);

/*
 * hmm_range_fault() - try to fault some address in a virtual address range
 * @range: range being faulted
 * @block: allow blocking on fault (if true it sleeps and do not drop mmap_sem)
 * Returns: number of valid pages in range->pfns[] (from range start
 *          address). This may be zero. If the return value is negative,
 *          then one of the following values may be returned:
 *
 *           -EINVAL  invalid arguments or mm or virtual address are in an
 *                    invalid vma (for instance device file vma).
 *           -ENOMEM: Out of memory.
 *           -EPERM:  Invalid permission (for instance asking for write and
 *                    range is read only).
 *           -EAGAIN: If you need to retry and mmap_sem was drop. This can only
 *                    happens if block argument is false.
 *           -EBUSY:  If the the range is being invalidated and you should wait
 *                    for invalidation to finish.
 *           -EFAULT: Invalid (ie either no valid vma or it is illegal to access
 *                    that range), number of valid pages in range->pfns[] (from
 *                    range start address).
 *
 * This is similar to a regular CPU page fault except that it will not trigger
 * any memory migration if the memory being faulted is not accessible by CPUs
 * and caller does not ask for migration.
 *
 * On error, for one virtual address in the range, the function will mark the
 * corresponding HMM pfn entry with an error flag.
 */
long hmm_range_fault(struct hmm_range *range, bool block)
{
	const unsigned long device_vma = VM_IO | VM_PFNMAP | VM_MIXEDMAP;
	unsigned long start = range->start, end;
	struct hmm_vma_walk hmm_vma_walk;
	struct hmm *hmm = range->hmm;
	struct vm_area_struct *vma;
	struct mm_walk mm_walk;
	int ret;

	/* Check if hmm_mm_destroy() was call. */
	if (hmm->mm == NULL || hmm->dead)
		return -EFAULT;

	do {
		/* If range is no longer valid force retry. */
		if (!range->valid) {
			up_read(&hmm->mm->mmap_sem);
			return -EAGAIN;
		}

		vma = find_vma(hmm->mm, start);
		if (vma == NULL || (vma->vm_flags & device_vma))
			return -EFAULT;

		if (is_vm_hugetlb_page(vma)) {
			if (huge_page_shift(hstate_vma(vma)) !=
			    range->page_shift &&
			    range->page_shift != PAGE_SHIFT)
				return -EINVAL;
		} else {
			if (range->page_shift != PAGE_SHIFT)
				return -EINVAL;
		}

		if (!(vma->vm_flags & VM_READ)) {
			/*
			 * If vma do not allow read access, then assume that it
			 * does not allow write access, either. HMM does not
			 * support architecture that allow write without read.
			 */
			hmm_pfns_clear(range, range->pfns,
				range->start, range->end);
			return -EPERM;
		}

		range->vma = vma;
		hmm_vma_walk.pgmap = NULL;
		hmm_vma_walk.last = start;
		hmm_vma_walk.fault = true;
		hmm_vma_walk.block = block;
		hmm_vma_walk.range = range;
		mm_walk.private = &hmm_vma_walk;
		end = min(range->end, vma->vm_end);

		mm_walk.vma = vma;
		mm_walk.mm = vma->vm_mm;
		mm_walk.pte_entry = NULL;
		mm_walk.test_walk = NULL;
		mm_walk.hugetlb_entry = NULL;
		mm_walk.pud_entry = hmm_vma_walk_pud;
		mm_walk.pmd_entry = hmm_vma_walk_pmd;
		mm_walk.pte_hole = hmm_vma_walk_hole;
		mm_walk.hugetlb_entry = hmm_vma_walk_hugetlb_entry;

		do {
			ret = walk_page_range(start, end, &mm_walk);
			start = hmm_vma_walk.last;

			/* Keep trying while the range is valid. */
		} while (ret == -EBUSY && range->valid);

		if (ret) {
			unsigned long i;

			i = (hmm_vma_walk.last - range->start) >> PAGE_SHIFT;
			hmm_pfns_clear(range, &range->pfns[i],
				hmm_vma_walk.last, range->end);
			return ret;
		}
		start = end;

	} while (start < range->end);

	return (hmm_vma_walk.last - range->start) >> PAGE_SHIFT;
}
EXPORT_SYMBOL(hmm_range_fault);

/**
 * hmm_range_dma_map() - hmm_range_fault() and dma map page all in one.
 * @range: range being faulted
 * @device: device against to dma map page to
 * @daddrs: dma address of mapped pages
 * @block: allow blocking on fault (if true it sleeps and do not drop mmap_sem)
 * Returns: number of pages mapped on success, -EAGAIN if mmap_sem have been
 *          drop and you need to try again, some other error value otherwise
 *
 * Note same usage pattern as hmm_range_fault().
 */
long hmm_range_dma_map(struct hmm_range *range,
		       struct device *device,
		       dma_addr_t *daddrs,
		       bool block)
{
	unsigned long i, npages, mapped;
	long ret;

	ret = hmm_range_fault(range, block);
	if (ret <= 0)
		return ret ? ret : -EBUSY;

	npages = (range->end - range->start) >> PAGE_SHIFT;
	for (i = 0, mapped = 0; i < npages; ++i) {
		enum dma_data_direction dir = DMA_TO_DEVICE;
		struct page *page;

		/*
		 * FIXME need to update DMA API to provide invalid DMA address
		 * value instead of a function to test dma address value. This
		 * would remove lot of dumb code duplicated accross many arch.
		 *
		 * For now setting it to 0 here is good enough as the pfns[]
		 * value is what is use to check what is valid and what isn't.
		 */
		daddrs[i] = 0;

		page = hmm_device_entry_to_page(range, range->pfns[i]);
		if (page == NULL)
			continue;

		/* Check if range is being invalidated */
		if (!range->valid) {
			ret = -EBUSY;
			goto unmap;
		}

		/* If it is read and write than map bi-directional. */
		if (range->pfns[i] & range->flags[HMM_PFN_WRITE])
			dir = DMA_BIDIRECTIONAL;

		daddrs[i] = dma_map_page(device, page, 0, PAGE_SIZE, dir);
		if (dma_mapping_error(device, daddrs[i])) {
			ret = -EFAULT;
			goto unmap;
		}

		mapped++;
	}

	return mapped;

unmap:
	for (npages = i, i = 0; (i < npages) && mapped; ++i) {
		enum dma_data_direction dir = DMA_TO_DEVICE;
		struct page *page;

		page = hmm_device_entry_to_page(range, range->pfns[i]);
		if (page == NULL)
			continue;

		if (dma_mapping_error(device, daddrs[i]))
			continue;

		/* If it is read and write than map bi-directional. */
		if (range->pfns[i] & range->flags[HMM_PFN_WRITE])
			dir = DMA_BIDIRECTIONAL;

		dma_unmap_page(device, daddrs[i], PAGE_SIZE, dir);
		mapped--;
	}

	return ret;
}
EXPORT_SYMBOL(hmm_range_dma_map);

/**
 * hmm_range_dma_unmap() - unmap range of that was map with hmm_range_dma_map()
 * @range: range being unmapped
 * @vma: the vma against which the range (optional)
 * @device: device against which dma map was done
 * @daddrs: dma address of mapped pages
 * @dirty: dirty page if it had the write flag set
 * Returns: number of page unmapped on success, -EINVAL otherwise
 *
 * Note that caller MUST abide by mmu notifier or use HMM mirror and abide
 * to the sync_cpu_device_pagetables() callback so that it is safe here to
 * call set_page_dirty(). Caller must also take appropriate locks to avoid
 * concurrent mmu notifier or sync_cpu_device_pagetables() to make progress.
 */
long hmm_range_dma_unmap(struct hmm_range *range,
			 struct vm_area_struct *vma,
			 struct device *device,
			 dma_addr_t *daddrs,
			 bool dirty)
{
	unsigned long i, npages;
	long cpages = 0;

	/* Sanity check. */
	if (range->end <= range->start)
		return -EINVAL;
	if (!daddrs)
		return -EINVAL;
	if (!range->pfns)
		return -EINVAL;

	npages = (range->end - range->start) >> PAGE_SHIFT;
	for (i = 0; i < npages; ++i) {
		enum dma_data_direction dir = DMA_TO_DEVICE;
		struct page *page;

		page = hmm_device_entry_to_page(range, range->pfns[i]);
		if (page == NULL)
			continue;

		/* If it is read and write than map bi-directional. */
		if (range->pfns[i] & range->flags[HMM_PFN_WRITE]) {
			dir = DMA_BIDIRECTIONAL;

			/*
			 * See comments in function description on why it is
			 * safe here to call set_page_dirty()
			 */
			if (dirty)
				set_page_dirty(page);
		}

		/* Unmap and clear pfns/dma address */
		dma_unmap_page(device, daddrs[i], PAGE_SIZE, dir);
		range->pfns[i] = range->values[HMM_PFN_NONE];
		/* FIXME see comments in hmm_vma_dma_map() */
		daddrs[i] = 0;
		cpages++;
	}

	return cpages;
}
EXPORT_SYMBOL(hmm_range_dma_unmap);
#endif /* IS_ENABLED(CONFIG_HMM_MIRROR) */


#if IS_ENABLED(CONFIG_DEVICE_PRIVATE) ||  IS_ENABLED(CONFIG_DEVICE_PUBLIC)
struct page *hmm_vma_alloc_locked_page(struct vm_area_struct *vma,
				       unsigned long addr)
{
	struct page *page;

	page = alloc_page_vma(GFP_HIGHUSER, vma, addr);
	if (!page)
		return NULL;
	lock_page(page);
	return page;
}
EXPORT_SYMBOL(hmm_vma_alloc_locked_page);


static void hmm_devmem_ref_release(struct percpu_ref *ref)
{
	struct hmm_devmem *devmem;

	devmem = container_of(ref, struct hmm_devmem, ref);
	complete(&devmem->completion);
}

static void hmm_devmem_ref_exit(void *data)
{
	struct percpu_ref *ref = data;
	struct hmm_devmem *devmem;

	devmem = container_of(ref, struct hmm_devmem, ref);
	wait_for_completion(&devmem->completion);
	percpu_ref_exit(ref);
}

static void hmm_devmem_ref_kill(struct percpu_ref *ref)
{
	percpu_ref_kill(ref);
}

static vm_fault_t hmm_devmem_fault(struct vm_area_struct *vma,
			    unsigned long addr,
			    const struct page *page,
			    unsigned int flags,
			    pmd_t *pmdp)
{
	struct hmm_devmem *devmem = page->pgmap->data;

	return devmem->ops->fault(devmem, vma, addr, page, flags, pmdp);
}

static void hmm_devmem_free(struct page *page, void *data)
{
	struct hmm_devmem *devmem = data;

	page->mapping = NULL;

	devmem->ops->free(devmem, page);
}

/*
 * hmm_devmem_add() - hotplug ZONE_DEVICE memory for device memory
 *
 * @ops: memory event device driver callback (see struct hmm_devmem_ops)
 * @device: device struct to bind the resource too
 * @size: size in bytes of the device memory to add
 * Returns: pointer to new hmm_devmem struct ERR_PTR otherwise
 *
 * This function first finds an empty range of physical address big enough to
 * contain the new resource, and then hotplugs it as ZONE_DEVICE memory, which
 * in turn allocates struct pages. It does not do anything beyond that; all
 * events affecting the memory will go through the various callbacks provided
 * by hmm_devmem_ops struct.
 *
 * Device driver should call this function during device initialization and
 * is then responsible of memory management. HMM only provides helpers.
 */
struct hmm_devmem *hmm_devmem_add(const struct hmm_devmem_ops *ops,
				  struct device *device,
				  unsigned long size)
{
	struct hmm_devmem *devmem;
	resource_size_t addr;
	void *result;
	int ret;

	dev_pagemap_get_ops();

	devmem = devm_kzalloc(device, sizeof(*devmem), GFP_KERNEL);
	if (!devmem)
		return ERR_PTR(-ENOMEM);

	init_completion(&devmem->completion);
	devmem->pfn_first = -1UL;
	devmem->pfn_last = -1UL;
	devmem->resource = NULL;
	devmem->device = device;
	devmem->ops = ops;

	ret = percpu_ref_init(&devmem->ref, &hmm_devmem_ref_release,
			      0, GFP_KERNEL);
	if (ret)
		return ERR_PTR(ret);

	ret = devm_add_action_or_reset(device, hmm_devmem_ref_exit, &devmem->ref);
	if (ret)
		return ERR_PTR(ret);

	size = ALIGN(size, PA_SECTION_SIZE);
	addr = min((unsigned long)iomem_resource.end,
		   (1UL << MAX_PHYSMEM_BITS) - 1);
	addr = addr - size + 1UL;

	/*
	 * FIXME add a new helper to quickly walk resource tree and find free
	 * range
	 *
	 * FIXME what about ioport_resource resource ?
	 */
	for (; addr > size && addr >= iomem_resource.start; addr -= size) {
		ret = region_intersects(addr, size, 0, IORES_DESC_NONE);
		if (ret != REGION_DISJOINT)
			continue;

		devmem->resource = devm_request_mem_region(device, addr, size,
							   dev_name(device));
		if (!devmem->resource)
			return ERR_PTR(-ENOMEM);
		break;
	}
	if (!devmem->resource)
		return ERR_PTR(-ERANGE);

	devmem->resource->desc = IORES_DESC_DEVICE_PRIVATE_MEMORY;
	devmem->pfn_first = devmem->resource->start >> PAGE_SHIFT;
	devmem->pfn_last = devmem->pfn_first +
			   (resource_size(devmem->resource) >> PAGE_SHIFT);
	devmem->page_fault = hmm_devmem_fault;

	devmem->pagemap.type = MEMORY_DEVICE_PRIVATE;
	devmem->pagemap.res = *devmem->resource;
	devmem->pagemap.page_free = hmm_devmem_free;
	devmem->pagemap.altmap_valid = false;
	devmem->pagemap.ref = &devmem->ref;
	devmem->pagemap.data = devmem;
	devmem->pagemap.kill = hmm_devmem_ref_kill;

	result = devm_memremap_pages(devmem->device, &devmem->pagemap);
	if (IS_ERR(result))
		return result;
	return devmem;
}
EXPORT_SYMBOL_GPL(hmm_devmem_add);

struct hmm_devmem *hmm_devmem_add_resource(const struct hmm_devmem_ops *ops,
					   struct device *device,
					   struct resource *res)
{
	struct hmm_devmem *devmem;
	void *result;
	int ret;

	if (res->desc != IORES_DESC_DEVICE_PUBLIC_MEMORY)
		return ERR_PTR(-EINVAL);

	dev_pagemap_get_ops();

	devmem = devm_kzalloc(device, sizeof(*devmem), GFP_KERNEL);
	if (!devmem)
		return ERR_PTR(-ENOMEM);

	init_completion(&devmem->completion);
	devmem->pfn_first = -1UL;
	devmem->pfn_last = -1UL;
	devmem->resource = res;
	devmem->device = device;
	devmem->ops = ops;

	ret = percpu_ref_init(&devmem->ref, &hmm_devmem_ref_release,
			      0, GFP_KERNEL);
	if (ret)
		return ERR_PTR(ret);

	ret = devm_add_action_or_reset(device, hmm_devmem_ref_exit,
			&devmem->ref);
	if (ret)
		return ERR_PTR(ret);

	devmem->pfn_first = devmem->resource->start >> PAGE_SHIFT;
	devmem->pfn_last = devmem->pfn_first +
			   (resource_size(devmem->resource) >> PAGE_SHIFT);
	devmem->page_fault = hmm_devmem_fault;

	devmem->pagemap.type = MEMORY_DEVICE_PUBLIC;
	devmem->pagemap.res = *devmem->resource;
	devmem->pagemap.page_free = hmm_devmem_free;
	devmem->pagemap.altmap_valid = false;
	devmem->pagemap.ref = &devmem->ref;
	devmem->pagemap.data = devmem;
	devmem->pagemap.kill = hmm_devmem_ref_kill;

	result = devm_memremap_pages(devmem->device, &devmem->pagemap);
	if (IS_ERR(result))
		return result;
	return devmem;
}
EXPORT_SYMBOL_GPL(hmm_devmem_add_resource);

/*
 * A device driver that wants to handle multiple devices memory through a
 * single fake device can use hmm_device to do so. This is purely a helper
 * and it is not needed to make use of any HMM functionality.
 */
#define HMM_DEVICE_MAX 256

static DECLARE_BITMAP(hmm_device_mask, HMM_DEVICE_MAX);
static DEFINE_SPINLOCK(hmm_device_lock);
static struct class *hmm_device_class;
static dev_t hmm_device_devt;

static void hmm_device_release(struct device *device)
{
	struct hmm_device *hmm_device;

	hmm_device = container_of(device, struct hmm_device, device);
	spin_lock(&hmm_device_lock);
	clear_bit(hmm_device->minor, hmm_device_mask);
	spin_unlock(&hmm_device_lock);

	kfree(hmm_device);
}

struct hmm_device *hmm_device_new(void *drvdata)
{
	struct hmm_device *hmm_device;

	hmm_device = kzalloc(sizeof(*hmm_device), GFP_KERNEL);
	if (!hmm_device)
		return ERR_PTR(-ENOMEM);

	spin_lock(&hmm_device_lock);
	hmm_device->minor = find_first_zero_bit(hmm_device_mask, HMM_DEVICE_MAX);
	if (hmm_device->minor >= HMM_DEVICE_MAX) {
		spin_unlock(&hmm_device_lock);
		kfree(hmm_device);
		return ERR_PTR(-EBUSY);
	}
	set_bit(hmm_device->minor, hmm_device_mask);
	spin_unlock(&hmm_device_lock);

	dev_set_name(&hmm_device->device, "hmm_device%d", hmm_device->minor);
	hmm_device->device.devt = MKDEV(MAJOR(hmm_device_devt),
					hmm_device->minor);
	hmm_device->device.release = hmm_device_release;
	dev_set_drvdata(&hmm_device->device, drvdata);
	hmm_device->device.class = hmm_device_class;
	device_initialize(&hmm_device->device);

	return hmm_device;
}
EXPORT_SYMBOL(hmm_device_new);

void hmm_device_put(struct hmm_device *hmm_device)
{
	put_device(&hmm_device->device);
}
EXPORT_SYMBOL(hmm_device_put);

static int __init hmm_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&hmm_device_devt, 0,
				  HMM_DEVICE_MAX,
				  "hmm_device");
	if (ret)
		return ret;

	hmm_device_class = class_create(THIS_MODULE, "hmm_device");
	if (IS_ERR(hmm_device_class)) {
		unregister_chrdev_region(hmm_device_devt, HMM_DEVICE_MAX);
		return PTR_ERR(hmm_device_class);
	}
	return 0;
}

device_initcall(hmm_init);
#endif /* CONFIG_DEVICE_PRIVATE || CONFIG_DEVICE_PUBLIC */
