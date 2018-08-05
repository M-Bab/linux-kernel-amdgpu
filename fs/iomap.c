/*
 * Copyright (C) 2010 Red Hat, Inc.
 * Copyright (c) 2016 Christoph Hellwig.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
#include <linux/module.h>
#include <linux/compiler.h>
#include <linux/fs.h>
#include <linux/iomap.h>
#include <linux/uaccess.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/pagemap.h>
#include <linux/pagevec.h>
#include <linux/file.h>
#include <linux/uio.h>
#include <linux/backing-dev.h>
#include <linux/buffer_head.h>
#include <linux/task_io_accounting_ops.h>
#include <linux/dax.h>
#include <linux/sched/signal.h>
#include <linux/swap.h>

#include "internal.h"

/*
 * Execute a iomap write on a segment of the mapping that spans a
 * contiguous range of pages that have identical block mapping state.
 *
 * This avoids the need to map pages individually, do individual allocations
 * for each page and most importantly avoid the need for filesystem specific
 * locking per page. Instead, all the operations are amortised over the entire
 * range of pages. It is assumed that the filesystems will lock whatever
 * resources they require in the iomap_begin call, and release them in the
 * iomap_end call.
 */
loff_t
iomap_apply(struct inode *inode, loff_t pos, loff_t length, unsigned flags,
		const struct iomap_ops *ops, void *data, iomap_actor_t actor)
{
	struct iomap iomap = { 0 };
	loff_t written = 0, ret;

	/*
	 * Need to map a range from start position for length bytes. This can
	 * span multiple pages - it is only guaranteed to return a range of a
	 * single type of pages (e.g. all into a hole, all mapped or all
	 * unwritten). Failure at this point has nothing to undo.
	 *
	 * If allocation is required for this range, reserve the space now so
	 * that the allocation is guaranteed to succeed later on. Once we copy
	 * the data into the page cache pages, then we cannot fail otherwise we
	 * expose transient stale data. If the reserve fails, we can safely
	 * back out at this point as there is nothing to undo.
	 */
	ret = ops->iomap_begin(inode, pos, length, flags, &iomap);
	if (ret)
		return ret;
	if (WARN_ON(iomap.offset > pos))
		return -EIO;
	if (WARN_ON(iomap.length == 0))
		return -EIO;

	/*
	 * Cut down the length to the one actually provided by the filesystem,
	 * as it might not be able to give us the whole size that we requested.
	 */
	if (iomap.offset + iomap.length < pos + length)
		length = iomap.offset + iomap.length - pos;

	/*
	 * Now that we have guaranteed that the space allocation will succeed.
	 * we can do the copy-in page by page without having to worry about
	 * failures exposing transient data.
	 */
	written = actor(inode, pos, length, data, &iomap);

	/*
	 * Now the data has been copied, commit the range we've copied.  This
	 * should not fail unless the filesystem has had a fatal error.
	 */
	if (ops->iomap_end) {
		ret = ops->iomap_end(inode, pos, length,
				     written > 0 ? written : 0,
				     flags, &iomap);
	}

	return written ? written : ret;
}

static sector_t
iomap_sector(struct iomap *iomap, loff_t pos)
{
	return (iomap->addr + pos - iomap->offset) >> SECTOR_SHIFT;
}

static void
iomap_write_failed(struct inode *inode, loff_t pos, unsigned len)
{
	loff_t i_size = i_size_read(inode);

	/*
	 * Only truncate newly allocated pages beyoned EOF, even if the
	 * write started inside the existing inode size.
	 */
	if (pos + len > i_size)
		truncate_pagecache_range(inode, max(pos, i_size), pos + len);
}

static int
iomap_write_begin(struct inode *inode, loff_t pos, unsigned len, unsigned flags,
		struct page **pagep, struct iomap *iomap)
{
	pgoff_t index = pos >> PAGE_SHIFT;
	struct page *page;
	int status = 0;

	BUG_ON(pos + len > iomap->offset + iomap->length);

	if (fatal_signal_pending(current))
		return -EINTR;

	page = grab_cache_page_write_begin(inode->i_mapping, index, flags);
	if (!page)
		return -ENOMEM;

	status = __block_write_begin_int(page, pos, len, NULL, iomap);
	if (unlikely(status)) {
		unlock_page(page);
		put_page(page);
		page = NULL;

		iomap_write_failed(inode, pos, len);
	}

	*pagep = page;
	return status;
}

static int
iomap_write_end(struct inode *inode, loff_t pos, unsigned len,
		unsigned copied, struct page *page)
{
	int ret;

	ret = generic_write_end(NULL, inode->i_mapping, pos, len,
			copied, page, NULL);
	if (ret < len)
		iomap_write_failed(inode, pos, len);
	return ret;
}

static loff_t
iomap_write_actor(struct inode *inode, loff_t pos, loff_t length, void *data,
		struct iomap *iomap)
{
	struct iov_iter *i = data;
	long status = 0;
	ssize_t written = 0;
	unsigned int flags = AOP_FLAG_NOFS;

	do {
		struct page *page;
		unsigned long offset;	/* Offset into pagecache page */
		unsigned long bytes;	/* Bytes to write to page */
		size_t copied;		/* Bytes copied from user */

		offset = (pos & (PAGE_SIZE - 1));
		bytes = min_t(unsigned long, PAGE_SIZE - offset,
						iov_iter_count(i));
again:
		if (bytes > length)
			bytes = length;

		/*
		 * Bring in the user page that we will copy from _first_.
		 * Otherwise there's a nasty deadlock on copying from the
		 * same page as we're writing to, without it being marked
		 * up-to-date.
		 *
		 * Not only is this an optimisation, but it is also required
		 * to check that the address is actually valid, when atomic
		 * usercopies are used, below.
		 */
		if (unlikely(iov_iter_fault_in_readable(i, bytes))) {
			status = -EFAULT;
			break;
		}

		status = iomap_write_begin(inode, pos, bytes, flags, &page,
				iomap);
		if (unlikely(status))
			break;

		if (mapping_writably_mapped(inode->i_mapping))
			flush_dcache_page(page);

		copied = iov_iter_copy_from_user_atomic(page, i, offset, bytes);

		flush_dcache_page(page);

		status = iomap_write_end(inode, pos, bytes, copied, page);
		if (unlikely(status < 0))
			break;
		copied = status;

		cond_resched();

		iov_iter_advance(i, copied);
		if (unlikely(copied == 0)) {
			/*
			 * If we were unable to copy any data at all, we must
			 * fall back to a single segment length write.
			 *
			 * If we didn't fallback here, we could livelock
			 * because not all segments in the iov can be copied at
			 * once without a pagefault.
			 */
			bytes = min_t(unsigned long, PAGE_SIZE - offset,
						iov_iter_single_seg_count(i));
			goto again;
		}
		pos += copied;
		written += copied;
		length -= copied;

		balance_dirty_pages_ratelimited(inode->i_mapping);
	} while (iov_iter_count(i) && length);

	return written ? written : status;
}

ssize_t
iomap_file_buffered_write(struct kiocb *iocb, struct iov_iter *iter,
		const struct iomap_ops *ops)
{
	struct inode *inode = iocb->ki_filp->f_mapping->host;
	loff_t pos = iocb->ki_pos, ret = 0, written = 0;

	while (iov_iter_count(iter)) {
		ret = iomap_apply(inode, pos, iov_iter_count(iter),
				IOMAP_WRITE, ops, iter, iomap_write_actor);
		if (ret <= 0)
			break;
		pos += ret;
		written += ret;
	}

	return written ? written : ret;
}
EXPORT_SYMBOL_GPL(iomap_file_buffered_write);

static struct page *
__iomap_read_page(struct inode *inode, loff_t offset)
{
	struct address_space *mapping = inode->i_mapping;
	struct page *page;

	page = read_mapping_page(mapping, offset >> PAGE_SHIFT, NULL);
	if (IS_ERR(page))
		return page;
	if (!PageUptodate(page)) {
		put_page(page);
		return ERR_PTR(-EIO);
	}
	return page;
}

static loff_t
iomap_dirty_actor(struct inode *inode, loff_t pos, loff_t length, void *data,
		struct iomap *iomap)
{
	long status = 0;
	ssize_t written = 0;

	do {
		struct page *page, *rpage;
		unsigned long offset;	/* Offset into pagecache page */
		unsigned long bytes;	/* Bytes to write to page */

		offset = (pos & (PAGE_SIZE - 1));
		bytes = min_t(loff_t, PAGE_SIZE - offset, length);

		rpage = __iomap_read_page(inode, pos);
		if (IS_ERR(rpage))
			return PTR_ERR(rpage);

		status = iomap_write_begin(inode, pos, bytes,
					   AOP_FLAG_NOFS, &page, iomap);
		put_page(rpage);
		if (unlikely(status))
			return status;

		WARN_ON_ONCE(!PageUptodate(page));

		status = iomap_write_end(inode, pos, bytes, bytes, page);
		if (unlikely(status <= 0)) {
			if (WARN_ON_ONCE(status == 0))
				return -EIO;
			return status;
		}

		cond_resched();

		pos += status;
		written += status;
		length -= status;

		balance_dirty_pages_ratelimited(inode->i_mapping);
	} while (length);

	return written;
}

int
iomap_file_dirty(struct inode *inode, loff_t pos, loff_t len,
		const struct iomap_ops *ops)
{
	loff_t ret;

	while (len) {
		ret = iomap_apply(inode, pos, len, IOMAP_WRITE, ops, NULL,
				iomap_dirty_actor);
		if (ret <= 0)
			return ret;
		pos += ret;
		len -= ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(iomap_file_dirty);

static int iomap_zero(struct inode *inode, loff_t pos, unsigned offset,
		unsigned bytes, struct iomap *iomap)
{
	struct page *page;
	int status;

	status = iomap_write_begin(inode, pos, bytes, AOP_FLAG_NOFS, &page,
				   iomap);
	if (status)
		return status;

	zero_user(page, offset, bytes);
	mark_page_accessed(page);

	return iomap_write_end(inode, pos, bytes, bytes, page);
}

static int iomap_dax_zero(loff_t pos, unsigned offset, unsigned bytes,
		struct iomap *iomap)
{
	return __dax_zero_page_range(iomap->bdev, iomap->dax_dev,
			iomap_sector(iomap, pos & PAGE_MASK), offset, bytes);
}

static loff_t
iomap_zero_range_actor(struct inode *inode, loff_t pos, loff_t count,
		void *data, struct iomap *iomap)
{
	bool *did_zero = data;
	loff_t written = 0;
	int status;

	/* already zeroed?  we're done. */
	if (iomap->type == IOMAP_HOLE || iomap->type == IOMAP_UNWRITTEN)
	    	return count;

	do {
		unsigned offset, bytes;

		offset = pos & (PAGE_SIZE - 1); /* Within page */
		bytes = min_t(loff_t, PAGE_SIZE - offset, count);

		if (IS_DAX(inode))
			status = iomap_dax_zero(pos, offset, bytes, iomap);
		else
			status = iomap_zero(inode, pos, offset, bytes, iomap);
		if (status < 0)
			return status;

		pos += bytes;
		count -= bytes;
		written += bytes;
		if (did_zero)
			*did_zero = true;
	} while (count > 0);

	return written;
}

int
iomap_zero_range(struct inode *inode, loff_t pos, loff_t len, bool *did_zero,
		const struct iomap_ops *ops)
{
	loff_t ret;

	while (len > 0) {
		ret = iomap_apply(inode, pos, len, IOMAP_ZERO,
				ops, did_zero, iomap_zero_range_actor);
		if (ret <= 0)
			return ret;

		pos += ret;
		len -= ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(iomap_zero_range);

int
iomap_truncate_page(struct inode *inode, loff_t pos, bool *did_zero,
		const struct iomap_ops *ops)
{
	unsigned int blocksize = i_blocksize(inode);
	unsigned int off = pos & (blocksize - 1);

	/* Block boundary? Nothing to do */
	if (!off)
		return 0;
	return iomap_zero_range(inode, pos, blocksize - off, did_zero, ops);
}
EXPORT_SYMBOL_GPL(iomap_truncate_page);

static loff_t
iomap_page_mkwrite_actor(struct inode *inode, loff_t pos, loff_t length,
		void *data, struct iomap *iomap)
{
	struct page *page = data;
	int ret;

	ret = __block_write_begin_int(page, pos, length, NULL, iomap);
	if (ret)
		return ret;

	block_commit_write(page, 0, length);
	return length;
}

int iomap_page_mkwrite(struct vm_fault *vmf, const struct iomap_ops *ops)
{
	struct page *page = vmf->page;
	struct inode *inode = file_inode(vmf->vma->vm_file);
	unsigned long length;
	loff_t offset, size;
	ssize_t ret;

	lock_page(page);
	size = i_size_read(inode);
	if ((page->mapping != inode->i_mapping) ||
	    (page_offset(page) > size)) {
		/* We overload EFAULT to mean page got truncated */
		ret = -EFAULT;
		goto out_unlock;
	}

	/* page is wholly or partially inside EOF */
	if (((page->index + 1) << PAGE_SHIFT) > size)
		length = size & ~PAGE_MASK;
	else
		length = PAGE_SIZE;

	offset = page_offset(page);
	while (length > 0) {
		ret = iomap_apply(inode, offset, length,
				IOMAP_WRITE | IOMAP_FAULT, ops, page,
				iomap_page_mkwrite_actor);
		if (unlikely(ret <= 0))
			goto out_unlock;
		offset += ret;
		length -= ret;
	}

	set_page_dirty(page);
	wait_for_stable_page(page);
	return VM_FAULT_LOCKED;
out_unlock:
	unlock_page(page);
	return block_page_mkwrite_return(ret);
}
EXPORT_SYMBOL_GPL(iomap_page_mkwrite);

struct fiemap_ctx {
	struct fiemap_extent_info *fi;
	struct iomap prev;
};

static int iomap_to_fiemap(struct fiemap_extent_info *fi,
		struct iomap *iomap, u32 flags)
{
	switch (iomap->type) {
	case IOMAP_HOLE:
		/* skip holes */
		return 0;
	case IOMAP_DELALLOC:
		flags |= FIEMAP_EXTENT_DELALLOC | FIEMAP_EXTENT_UNKNOWN;
		break;
	case IOMAP_MAPPED:
		break;
	case IOMAP_UNWRITTEN:
		flags |= FIEMAP_EXTENT_UNWRITTEN;
		break;
	case IOMAP_INLINE:
		flags |= FIEMAP_EXTENT_DATA_INLINE;
		break;
	}

	if (iomap->flags & IOMAP_F_MERGED)
		flags |= FIEMAP_EXTENT_MERGED;
	if (iomap->flags & IOMAP_F_SHARED)
		flags |= FIEMAP_EXTENT_SHARED;

	return fiemap_fill_next_extent(fi, iomap->offset,
			iomap->addr != IOMAP_NULL_ADDR ? iomap->addr : 0,
			iomap->length, flags);
}

static loff_t
iomap_fiemap_actor(struct inode *inode, loff_t pos, loff_t length, void *data,
		struct iomap *iomap)
{
	struct fiemap_ctx *ctx = data;
	loff_t ret = length;

	if (iomap->type == IOMAP_HOLE)
		return length;

	ret = iomap_to_fiemap(ctx->fi, &ctx->prev, 0);
	ctx->prev = *iomap;
	switch (ret) {
	case 0:		/* success */
		return length;
	case 1:		/* extent array full */
		return 0;
	default:
		return ret;
	}
}

int iomap_fiemap(struct inode *inode, struct fiemap_extent_info *fi,
		loff_t start, loff_t len, const struct iomap_ops *ops)
{
	struct fiemap_ctx ctx;
	loff_t ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.fi = fi;
	ctx.prev.type = IOMAP_HOLE;

	ret = fiemap_check_flags(fi, FIEMAP_FLAG_SYNC);
	if (ret)
		return ret;

	if (fi->fi_flags & FIEMAP_FLAG_SYNC) {
		ret = filemap_write_and_wait(inode->i_mapping);
		if (ret)
			return ret;
	}

	while (len > 0) {
		ret = iomap_apply(inode, start, len, IOMAP_REPORT, ops, &ctx,
				iomap_fiemap_actor);
		/* inode with no (attribute) mapping will give ENOENT */
		if (ret == -ENOENT)
			break;
		if (ret < 0)
			return ret;
		if (ret == 0)
			break;

		start += ret;
		len -= ret;
	}

	if (ctx.prev.type != IOMAP_HOLE) {
		ret = iomap_to_fiemap(fi, &ctx.prev, FIEMAP_EXTENT_LAST);
		if (ret < 0)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(iomap_fiemap);

/*
 * Seek for SEEK_DATA / SEEK_HOLE within @page, starting at @lastoff.
 * Returns true if found and updates @lastoff to the offset in file.
 */
static bool
page_seek_hole_data(struct inode *inode, struct page *page, loff_t *lastoff,
		int whence)
{
	const struct address_space_operations *ops = inode->i_mapping->a_ops;
	unsigned int bsize = i_blocksize(inode), off;
	bool seek_data = whence == SEEK_DATA;
	loff_t poff = page_offset(page);

	if (WARN_ON_ONCE(*lastoff >= poff + PAGE_SIZE))
		return false;

	if (*lastoff < poff) {
		/*
		 * Last offset smaller than the start of the page means we found
		 * a hole:
		 */
		if (whence == SEEK_HOLE)
			return true;
		*lastoff = poff;
	}

	/*
	 * Just check the page unless we can and should check block ranges:
	 */
	if (bsize == PAGE_SIZE || !ops->is_partially_uptodate)
		return PageUptodate(page) == seek_data;

	lock_page(page);
	if (unlikely(page->mapping != inode->i_mapping))
		goto out_unlock_not_found;

	for (off = 0; off < PAGE_SIZE; off += bsize) {
		if ((*lastoff & ~PAGE_MASK) >= off + bsize)
			continue;
		if (ops->is_partially_uptodate(page, off, bsize) == seek_data) {
			unlock_page(page);
			return true;
		}
		*lastoff = poff + off + bsize;
	}

out_unlock_not_found:
	unlock_page(page);
	return false;
}

/*
 * Seek for SEEK_DATA / SEEK_HOLE in the page cache.
 *
 * Within unwritten extents, the page cache determines which parts are holes
 * and which are data: uptodate buffer heads count as data; everything else
 * counts as a hole.
 *
 * Returns the resulting offset on successs, and -ENOENT otherwise.
 */
static loff_t
page_cache_seek_hole_data(struct inode *inode, loff_t offset, loff_t length,
		int whence)
{
	pgoff_t index = offset >> PAGE_SHIFT;
	pgoff_t end = DIV_ROUND_UP(offset + length, PAGE_SIZE);
	loff_t lastoff = offset;
	struct pagevec pvec;

	if (length <= 0)
		return -ENOENT;

	pagevec_init(&pvec);

	do {
		unsigned nr_pages, i;

		nr_pages = pagevec_lookup_range(&pvec, inode->i_mapping, &index,
						end - 1);
		if (nr_pages == 0)
			break;

		for (i = 0; i < nr_pages; i++) {
			struct page *page = pvec.pages[i];

			if (page_seek_hole_data(inode, page, &lastoff, whence))
				goto check_range;
			lastoff = page_offset(page) + PAGE_SIZE;
		}
		pagevec_release(&pvec);
	} while (index < end);

	/* When no page at lastoff and we are not done, we found a hole. */
	if (whence != SEEK_HOLE)
		goto not_found;

check_range:
	if (lastoff < offset + length)
		goto out;
not_found:
	lastoff = -ENOENT;
out:
	pagevec_release(&pvec);
	return lastoff;
}


static loff_t
iomap_seek_hole_actor(struct inode *inode, loff_t offset, loff_t length,
		      void *data, struct iomap *iomap)
{
	switch (iomap->type) {
	case IOMAP_UNWRITTEN:
		offset = page_cache_seek_hole_data(inode, offset, length,
						   SEEK_HOLE);
		if (offset < 0)
			return length;
		/* fall through */
	case IOMAP_HOLE:
		*(loff_t *)data = offset;
		return 0;
	default:
		return length;
	}
}

loff_t
iomap_seek_hole(struct inode *inode, loff_t offset, const struct iomap_ops *ops)
{
	loff_t size = i_size_read(inode);
	loff_t length = size - offset;
	loff_t ret;

	/* Nothing to be found before or beyond the end of the file. */
	if (offset < 0 || offset >= size)
		return -ENXIO;

	while (length > 0) {
		ret = iomap_apply(inode, offset, length, IOMAP_REPORT, ops,
				  &offset, iomap_seek_hole_actor);
		if (ret < 0)
			return ret;
		if (ret == 0)
			break;

		offset += ret;
		length -= ret;
	}

	return offset;
}
EXPORT_SYMBOL_GPL(iomap_seek_hole);

static loff_t
iomap_seek_data_actor(struct inode *inode, loff_t offset, loff_t length,
		      void *data, struct iomap *iomap)
{
	switch (iomap->type) {
	case IOMAP_HOLE:
		return length;
	case IOMAP_UNWRITTEN:
		offset = page_cache_seek_hole_data(inode, offset, length,
						   SEEK_DATA);
		if (offset < 0)
			return length;
		/*FALLTHRU*/
	default:
		*(loff_t *)data = offset;
		return 0;
	}
}

loff_t
iomap_seek_data(struct inode *inode, loff_t offset, const struct iomap_ops *ops)
{
	loff_t size = i_size_read(inode);
	loff_t length = size - offset;
	loff_t ret;

	/* Nothing to be found before or beyond the end of the file. */
	if (offset < 0 || offset >= size)
		return -ENXIO;

	while (length > 0) {
		ret = iomap_apply(inode, offset, length, IOMAP_REPORT, ops,
				  &offset, iomap_seek_data_actor);
		if (ret < 0)
			return ret;
		if (ret == 0)
			break;

		offset += ret;
		length -= ret;
	}

	if (length <= 0)
		return -ENXIO;
	return offset;
}
EXPORT_SYMBOL_GPL(iomap_seek_data);

/*
 * Private flags for iomap_dio, must not overlap with the public ones in
 * iomap.h:
 */
#define IOMAP_DIO_WRITE_FUA	(1 << 28)
#define IOMAP_DIO_NEED_SYNC	(1 << 29)
#define IOMAP_DIO_WRITE		(1 << 30)
#define IOMAP_DIO_DIRTY		(1 << 31)

struct iomap_dio {
	struct kiocb		*iocb;
	iomap_dio_end_io_t	*end_io;
	loff_t			i_size;
	loff_t			size;
	atomic_t		ref;
	unsigned		flags;
	int			error;

	union {
		/* used during submission and for synchronous completion: */
		struct {
			struct iov_iter		*iter;
			struct task_struct	*waiter;
			struct request_queue	*last_queue;
			blk_qc_t		cookie;
		} submit;

		/* used for aio completion: */
		struct {
			struct work_struct	work;
		} aio;
	};
};

static ssize_t iomap_dio_complete(struct iomap_dio *dio)
{
	struct kiocb *iocb = dio->iocb;
	struct inode *inode = file_inode(iocb->ki_filp);
	loff_t offset = iocb->ki_pos;
	ssize_t ret;

	if (dio->end_io) {
		ret = dio->end_io(iocb,
				dio->error ? dio->error : dio->size,
				dio->flags);
	} else {
		ret = dio->error;
	}

	if (likely(!ret)) {
		ret = dio->size;
		/* check for short read */
		if (offset + ret > dio->i_size &&
		    !(dio->flags & IOMAP_DIO_WRITE))
			ret = dio->i_size - offset;
		iocb->ki_pos += ret;
	}

	/*
	 * Try again to invalidate clean pages which might have been cached by
	 * non-direct readahead, or faulted in by get_user_pages() if the source
	 * of the write was an mmap'ed region of the file we're writing.  Either
	 * one is a pretty crazy thing to do, so we don't support it 100%.  If
	 * this invalidation fails, tough, the write still worked...
	 *
	 * And this page cache invalidation has to be after dio->end_io(), as
	 * some filesystems convert unwritten extents to real allocations in
	 * end_io() when necessary, otherwise a racing buffer read would cache
	 * zeros from unwritten extents.
	 */
	if (!dio->error &&
	    (dio->flags & IOMAP_DIO_WRITE) && inode->i_mapping->nrpages) {
		int err;
		err = invalidate_inode_pages2_range(inode->i_mapping,
				offset >> PAGE_SHIFT,
				(offset + dio->size - 1) >> PAGE_SHIFT);
		if (err)
			dio_warn_stale_pagecache(iocb->ki_filp);
	}

	/*
	 * If this is a DSYNC write, make sure we push it to stable storage now
	 * that we've written data.
	 */
	if (ret > 0 && (dio->flags & IOMAP_DIO_NEED_SYNC))
		ret = generic_write_sync(iocb, ret);

	inode_dio_end(file_inode(iocb->ki_filp));
	kfree(dio);

	return ret;
}

static void iomap_dio_complete_work(struct work_struct *work)
{
	struct iomap_dio *dio = container_of(work, struct iomap_dio, aio.work);
	struct kiocb *iocb = dio->iocb;

	iocb->ki_complete(iocb, iomap_dio_complete(dio), 0);
}

/*
 * Set an error in the dio if none is set yet.  We have to use cmpxchg
 * as the submission context and the completion context(s) can race to
 * update the error.
 */
static inline void iomap_dio_set_error(struct iomap_dio *dio, int ret)
{
	cmpxchg(&dio->error, 0, ret);
}

static void iomap_dio_bio_end_io(struct bio *bio)
{
	struct iomap_dio *dio = bio->bi_private;
	bool should_dirty = (dio->flags & IOMAP_DIO_DIRTY);

	if (bio->bi_status)
		iomap_dio_set_error(dio, blk_status_to_errno(bio->bi_status));

	if (atomic_dec_and_test(&dio->ref)) {
		if (is_sync_kiocb(dio->iocb)) {
			struct task_struct *waiter = dio->submit.waiter;

			WRITE_ONCE(dio->submit.waiter, NULL);
			wake_up_process(waiter);
		} else if (dio->flags & IOMAP_DIO_WRITE) {
			struct inode *inode = file_inode(dio->iocb->ki_filp);

			INIT_WORK(&dio->aio.work, iomap_dio_complete_work);
			queue_work(inode->i_sb->s_dio_done_wq, &dio->aio.work);
		} else {
			iomap_dio_complete_work(&dio->aio.work);
		}
	}

	if (should_dirty) {
		bio_check_pages_dirty(bio);
	} else {
		struct bio_vec *bvec;
		int i;

		bio_for_each_segment_all(bvec, bio, i)
			put_page(bvec->bv_page);
		bio_put(bio);
	}
}

static blk_qc_t
iomap_dio_zero(struct iomap_dio *dio, struct iomap *iomap, loff_t pos,
		unsigned len)
{
	struct page *page = ZERO_PAGE(0);
	struct bio *bio;

	bio = bio_alloc(GFP_KERNEL, 1);
	bio_set_dev(bio, iomap->bdev);
	bio->bi_iter.bi_sector = iomap_sector(iomap, pos);
	bio->bi_private = dio;
	bio->bi_end_io = iomap_dio_bio_end_io;

	get_page(page);
	__bio_add_page(bio, page, len, 0);
	bio_set_op_attrs(bio, REQ_OP_WRITE, REQ_SYNC | REQ_IDLE);

	atomic_inc(&dio->ref);
	return submit_bio(bio);
}

static loff_t
iomap_dio_actor(struct inode *inode, loff_t pos, loff_t length,
		void *data, struct iomap *iomap)
{
	struct iomap_dio *dio = data;
	unsigned int blkbits = blksize_bits(bdev_logical_block_size(iomap->bdev));
	unsigned int fs_block_size = i_blocksize(inode), pad;
	unsigned int align = iov_iter_alignment(dio->submit.iter);
	struct iov_iter iter;
	struct bio *bio;
	bool need_zeroout = false;
	bool use_fua = false;
	int nr_pages, ret;
	size_t copied = 0;

	if ((pos | length | align) & ((1 << blkbits) - 1))
		return -EINVAL;

	switch (iomap->type) {
	case IOMAP_HOLE:
		if (WARN_ON_ONCE(dio->flags & IOMAP_DIO_WRITE))
			return -EIO;
		/*FALLTHRU*/
	case IOMAP_UNWRITTEN:
		if (!(dio->flags & IOMAP_DIO_WRITE)) {
			length = iov_iter_zero(length, dio->submit.iter);
			dio->size += length;
			return length;
		}
		dio->flags |= IOMAP_DIO_UNWRITTEN;
		need_zeroout = true;
		break;
	case IOMAP_MAPPED:
		if (iomap->flags & IOMAP_F_SHARED)
			dio->flags |= IOMAP_DIO_COW;
		if (iomap->flags & IOMAP_F_NEW) {
			need_zeroout = true;
		} else {
			/*
			 * Use a FUA write if we need datasync semantics, this
			 * is a pure data IO that doesn't require any metadata
			 * updates and the underlying device supports FUA. This
			 * allows us to avoid cache flushes on IO completion.
			 */
			if (!(iomap->flags & (IOMAP_F_SHARED|IOMAP_F_DIRTY)) &&
			    (dio->flags & IOMAP_DIO_WRITE_FUA) &&
			    blk_queue_fua(bdev_get_queue(iomap->bdev)))
				use_fua = true;
		}
		break;
	default:
		WARN_ON_ONCE(1);
		return -EIO;
	}

	/*
	 * Operate on a partial iter trimmed to the extent we were called for.
	 * We'll update the iter in the dio once we're done with this extent.
	 */
	iter = *dio->submit.iter;
	iov_iter_truncate(&iter, length);

	nr_pages = iov_iter_npages(&iter, BIO_MAX_PAGES);
	if (nr_pages <= 0)
		return nr_pages;

	if (need_zeroout) {
		/* zero out from the start of the block to the write offset */
		pad = pos & (fs_block_size - 1);
		if (pad)
			iomap_dio_zero(dio, iomap, pos - pad, pad);
	}

	do {
		size_t n;
		if (dio->error) {
			iov_iter_revert(dio->submit.iter, copied);
			return 0;
		}

		bio = bio_alloc(GFP_KERNEL, nr_pages);
		bio_set_dev(bio, iomap->bdev);
		bio->bi_iter.bi_sector = iomap_sector(iomap, pos);
		bio->bi_write_hint = dio->iocb->ki_hint;
		bio->bi_ioprio = dio->iocb->ki_ioprio;
		bio->bi_private = dio;
		bio->bi_end_io = iomap_dio_bio_end_io;

		ret = bio_iov_iter_get_pages(bio, &iter);
		if (unlikely(ret)) {
			bio_put(bio);
			return copied ? copied : ret;
		}

		n = bio->bi_iter.bi_size;
		if (dio->flags & IOMAP_DIO_WRITE) {
			bio->bi_opf = REQ_OP_WRITE | REQ_SYNC | REQ_IDLE;
			if (use_fua)
				bio->bi_opf |= REQ_FUA;
			else
				dio->flags &= ~IOMAP_DIO_WRITE_FUA;
			task_io_account_write(n);
		} else {
			bio->bi_opf = REQ_OP_READ;
			if (dio->flags & IOMAP_DIO_DIRTY)
				bio_set_pages_dirty(bio);
		}

		iov_iter_advance(dio->submit.iter, n);

		dio->size += n;
		pos += n;
		copied += n;

		nr_pages = iov_iter_npages(&iter, BIO_MAX_PAGES);

		atomic_inc(&dio->ref);

		dio->submit.last_queue = bdev_get_queue(iomap->bdev);
		dio->submit.cookie = submit_bio(bio);
	} while (nr_pages);

	if (need_zeroout) {
		/* zero out from the end of the write to the end of the block */
		pad = pos & (fs_block_size - 1);
		if (pad)
			iomap_dio_zero(dio, iomap, pos, fs_block_size - pad);
	}
	return copied;
}

/*
 * iomap_dio_rw() always completes O_[D]SYNC writes regardless of whether the IO
 * is being issued as AIO or not.  This allows us to optimise pure data writes
 * to use REQ_FUA rather than requiring generic_write_sync() to issue a
 * REQ_FLUSH post write. This is slightly tricky because a single request here
 * can be mapped into multiple disjoint IOs and only a subset of the IOs issued
 * may be pure data writes. In that case, we still need to do a full data sync
 * completion.
 */
ssize_t
iomap_dio_rw(struct kiocb *iocb, struct iov_iter *iter,
		const struct iomap_ops *ops, iomap_dio_end_io_t end_io)
{
	struct address_space *mapping = iocb->ki_filp->f_mapping;
	struct inode *inode = file_inode(iocb->ki_filp);
	size_t count = iov_iter_count(iter);
	loff_t pos = iocb->ki_pos, start = pos;
	loff_t end = iocb->ki_pos + count - 1, ret = 0;
	unsigned int flags = IOMAP_DIRECT;
	struct blk_plug plug;
	struct iomap_dio *dio;

	lockdep_assert_held(&inode->i_rwsem);

	if (!count)
		return 0;

	dio = kmalloc(sizeof(*dio), GFP_KERNEL);
	if (!dio)
		return -ENOMEM;

	dio->iocb = iocb;
	atomic_set(&dio->ref, 1);
	dio->size = 0;
	dio->i_size = i_size_read(inode);
	dio->end_io = end_io;
	dio->error = 0;
	dio->flags = 0;

	dio->submit.iter = iter;
	if (is_sync_kiocb(iocb)) {
		dio->submit.waiter = current;
		dio->submit.cookie = BLK_QC_T_NONE;
		dio->submit.last_queue = NULL;
	}

	if (iov_iter_rw(iter) == READ) {
		if (pos >= dio->i_size)
			goto out_free_dio;

		if (iter->type == ITER_IOVEC)
			dio->flags |= IOMAP_DIO_DIRTY;
	} else {
		flags |= IOMAP_WRITE;
		dio->flags |= IOMAP_DIO_WRITE;

		/* for data sync or sync, we need sync completion processing */
		if (iocb->ki_flags & IOCB_DSYNC)
			dio->flags |= IOMAP_DIO_NEED_SYNC;

		/*
		 * For datasync only writes, we optimistically try using FUA for
		 * this IO.  Any non-FUA write that occurs will clear this flag,
		 * hence we know before completion whether a cache flush is
		 * necessary.
		 */
		if ((iocb->ki_flags & (IOCB_DSYNC | IOCB_SYNC)) == IOCB_DSYNC)
			dio->flags |= IOMAP_DIO_WRITE_FUA;
	}

	if (iocb->ki_flags & IOCB_NOWAIT) {
		if (filemap_range_has_page(mapping, start, end)) {
			ret = -EAGAIN;
			goto out_free_dio;
		}
		flags |= IOMAP_NOWAIT;
	}

	ret = filemap_write_and_wait_range(mapping, start, end);
	if (ret)
		goto out_free_dio;

	/*
	 * Try to invalidate cache pages for the range we're direct
	 * writing.  If this invalidation fails, tough, the write will
	 * still work, but racing two incompatible write paths is a
	 * pretty crazy thing to do, so we don't support it 100%.
	 */
	ret = invalidate_inode_pages2_range(mapping,
			start >> PAGE_SHIFT, end >> PAGE_SHIFT);
	if (ret)
		dio_warn_stale_pagecache(iocb->ki_filp);
	ret = 0;

	if (iov_iter_rw(iter) == WRITE && !is_sync_kiocb(iocb) &&
	    !inode->i_sb->s_dio_done_wq) {
		ret = sb_init_dio_done_wq(inode->i_sb);
		if (ret < 0)
			goto out_free_dio;
	}

	inode_dio_begin(inode);

	blk_start_plug(&plug);
	do {
		ret = iomap_apply(inode, pos, count, flags, ops, dio,
				iomap_dio_actor);
		if (ret <= 0) {
			/* magic error code to fall back to buffered I/O */
			if (ret == -ENOTBLK)
				ret = 0;
			break;
		}
		pos += ret;

		if (iov_iter_rw(iter) == READ && pos >= dio->i_size)
			break;
	} while ((count = iov_iter_count(iter)) > 0);
	blk_finish_plug(&plug);

	if (ret < 0)
		iomap_dio_set_error(dio, ret);

	/*
	 * If all the writes we issued were FUA, we don't need to flush the
	 * cache on IO completion. Clear the sync flag for this case.
	 */
	if (dio->flags & IOMAP_DIO_WRITE_FUA)
		dio->flags &= ~IOMAP_DIO_NEED_SYNC;

	if (!atomic_dec_and_test(&dio->ref)) {
		if (!is_sync_kiocb(iocb))
			return -EIOCBQUEUED;

		for (;;) {
			set_current_state(TASK_UNINTERRUPTIBLE);
			if (!READ_ONCE(dio->submit.waiter))
				break;

			if (!(iocb->ki_flags & IOCB_HIPRI) ||
			    !dio->submit.last_queue ||
			    !blk_poll(dio->submit.last_queue,
					 dio->submit.cookie))
				io_schedule();
		}
		__set_current_state(TASK_RUNNING);
	}

	ret = iomap_dio_complete(dio);

	return ret;

out_free_dio:
	kfree(dio);
	return ret;
}
EXPORT_SYMBOL_GPL(iomap_dio_rw);

/* Swapfile activation */

#ifdef CONFIG_SWAP
struct iomap_swapfile_info {
	struct iomap iomap;		/* accumulated iomap */
	struct swap_info_struct *sis;
	uint64_t lowest_ppage;		/* lowest physical addr seen (pages) */
	uint64_t highest_ppage;		/* highest physical addr seen (pages) */
	unsigned long nr_pages;		/* number of pages collected */
	int nr_extents;			/* extent count */
};

/*
 * Collect physical extents for this swap file.  Physical extents reported to
 * the swap code must be trimmed to align to a page boundary.  The logical
 * offset within the file is irrelevant since the swapfile code maps logical
 * page numbers of the swap device to the physical page-aligned extents.
 */
static int iomap_swapfile_add_extent(struct iomap_swapfile_info *isi)
{
	struct iomap *iomap = &isi->iomap;
	unsigned long nr_pages;
	uint64_t first_ppage;
	uint64_t first_ppage_reported;
	uint64_t next_ppage;
	int error;

	/*
	 * Round the start up and the end down so that the physical
	 * extent aligns to a page boundary.
	 */
	first_ppage = ALIGN(iomap->addr, PAGE_SIZE) >> PAGE_SHIFT;
	next_ppage = ALIGN_DOWN(iomap->addr + iomap->length, PAGE_SIZE) >>
			PAGE_SHIFT;

	/* Skip too-short physical extents. */
	if (first_ppage >= next_ppage)
		return 0;
	nr_pages = next_ppage - first_ppage;

	/*
	 * Calculate how much swap space we're adding; the first page contains
	 * the swap header and doesn't count.  The mm still wants that first
	 * page fed to add_swap_extent, however.
	 */
	first_ppage_reported = first_ppage;
	if (iomap->offset == 0)
		first_ppage_reported++;
	if (isi->lowest_ppage > first_ppage_reported)
		isi->lowest_ppage = first_ppage_reported;
	if (isi->highest_ppage < (next_ppage - 1))
		isi->highest_ppage = next_ppage - 1;

	/* Add extent, set up for the next call. */
	error = add_swap_extent(isi->sis, isi->nr_pages, nr_pages, first_ppage);
	if (error < 0)
		return error;
	isi->nr_extents += error;
	isi->nr_pages += nr_pages;
	return 0;
}

/*
 * Accumulate iomaps for this swap file.  We have to accumulate iomaps because
 * swap only cares about contiguous page-aligned physical extents and makes no
 * distinction between written and unwritten extents.
 */
static loff_t iomap_swapfile_activate_actor(struct inode *inode, loff_t pos,
		loff_t count, void *data, struct iomap *iomap)
{
	struct iomap_swapfile_info *isi = data;
	int error;

	switch (iomap->type) {
	case IOMAP_MAPPED:
	case IOMAP_UNWRITTEN:
		/* Only real or unwritten extents. */
		break;
	case IOMAP_INLINE:
		/* No inline data. */
		pr_err("swapon: file is inline\n");
		return -EINVAL;
	default:
		pr_err("swapon: file has unallocated extents\n");
		return -EINVAL;
	}

	/* No uncommitted metadata or shared blocks. */
	if (iomap->flags & IOMAP_F_DIRTY) {
		pr_err("swapon: file is not committed\n");
		return -EINVAL;
	}
	if (iomap->flags & IOMAP_F_SHARED) {
		pr_err("swapon: file has shared extents\n");
		return -EINVAL;
	}

	/* Only one bdev per swap file. */
	if (iomap->bdev != isi->sis->bdev) {
		pr_err("swapon: file is on multiple devices\n");
		return -EINVAL;
	}

	if (isi->iomap.length == 0) {
		/* No accumulated extent, so just store it. */
		memcpy(&isi->iomap, iomap, sizeof(isi->iomap));
	} else if (isi->iomap.addr + isi->iomap.length == iomap->addr) {
		/* Append this to the accumulated extent. */
		isi->iomap.length += iomap->length;
	} else {
		/* Otherwise, add the retained iomap and store this one. */
		error = iomap_swapfile_add_extent(isi);
		if (error)
			return error;
		memcpy(&isi->iomap, iomap, sizeof(isi->iomap));
	}
	return count;
}

/*
 * Iterate a swap file's iomaps to construct physical extents that can be
 * passed to the swapfile subsystem.
 */
int iomap_swapfile_activate(struct swap_info_struct *sis,
		struct file *swap_file, sector_t *pagespan,
		const struct iomap_ops *ops)
{
	struct iomap_swapfile_info isi = {
		.sis = sis,
		.lowest_ppage = (sector_t)-1ULL,
	};
	struct address_space *mapping = swap_file->f_mapping;
	struct inode *inode = mapping->host;
	loff_t pos = 0;
	loff_t len = ALIGN_DOWN(i_size_read(inode), PAGE_SIZE);
	loff_t ret;

	/*
	 * Persist all file mapping metadata so that we won't have any
	 * IOMAP_F_DIRTY iomaps.
	 */
	ret = vfs_fsync(swap_file, 1);
	if (ret)
		return ret;

	while (len > 0) {
		ret = iomap_apply(inode, pos, len, IOMAP_REPORT,
				ops, &isi, iomap_swapfile_activate_actor);
		if (ret <= 0)
			return ret;

		pos += ret;
		len -= ret;
	}

	if (isi.iomap.length) {
		ret = iomap_swapfile_add_extent(&isi);
		if (ret)
			return ret;
	}

	*pagespan = 1 + isi.highest_ppage - isi.lowest_ppage;
	sis->max = isi.nr_pages;
	sis->pages = isi.nr_pages - 1;
	sis->highest_bit = isi.nr_pages - 1;
	return isi.nr_extents;
}
EXPORT_SYMBOL_GPL(iomap_swapfile_activate);
#endif /* CONFIG_SWAP */

static loff_t
iomap_bmap_actor(struct inode *inode, loff_t pos, loff_t length,
		void *data, struct iomap *iomap)
{
	sector_t *bno = data, addr;

	if (iomap->type == IOMAP_MAPPED) {
		addr = (pos - iomap->offset + iomap->addr) >> inode->i_blkbits;
		if (addr > INT_MAX)
			WARN(1, "would truncate bmap result\n");
		else
			*bno = addr;
	}
	return 0;
}

/* legacy ->bmap interface.  0 is the error return (!) */
sector_t
iomap_bmap(struct address_space *mapping, sector_t bno,
		const struct iomap_ops *ops)
{
	struct inode *inode = mapping->host;
	loff_t pos = bno << inode->i_blkbits;
	unsigned blocksize = i_blocksize(inode);

	if (filemap_write_and_wait(mapping))
		return 0;

	bno = 0;
	iomap_apply(inode, pos, blocksize, 0, ops, &bno, iomap_bmap_actor);
	return bno;
}
EXPORT_SYMBOL_GPL(iomap_bmap);
