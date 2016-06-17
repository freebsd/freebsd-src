/*
 * Copyright (c) 2000-2004 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.	 Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

#include "xfs.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_sb.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_trans.h"
#include "xfs_dmapi.h"
#include "xfs_mount.h"
#include "xfs_bmap_btree.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_alloc.h"
#include "xfs_btree.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_error.h"
#include "xfs_rw.h"
#include "xfs_iomap.h"
#include <linux/iobuf.h>

STATIC void xfs_count_page_state(struct page *, int *, int *, int *);
STATIC void xfs_convert_page(struct inode *, struct page *,
				xfs_iomap_t *, void *, int, int);

#if defined(XFS_RW_TRACE)
void
xfs_page_trace(
	int		tag,
	struct inode	*inode,
	struct page	*page,
	int		mask)
{
	xfs_inode_t	*ip;
	bhv_desc_t	*bdp;
	vnode_t		*vp = LINVFS_GET_VP(inode);
	loff_t		isize = i_size_read(inode);
	loff_t		offset = page->index << PAGE_CACHE_SHIFT;
	int		delalloc = -1, unmapped = -1, unwritten = -1;

	if (page_has_buffers(page))
		xfs_count_page_state(page, &delalloc, &unmapped, &unwritten);

	bdp = vn_bhv_lookup(VN_BHV_HEAD(vp), &xfs_vnodeops);
	ip = XFS_BHVTOI(bdp);
	if (!ip->i_rwtrace)
		return;

	ktrace_enter(ip->i_rwtrace,
		(void *)((unsigned long)tag),
		(void *)ip,
		(void *)inode,
		(void *)page,
		(void *)((unsigned long)mask),
		(void *)((unsigned long)((ip->i_d.di_size >> 32) & 0xffffffff)),
		(void *)((unsigned long)(ip->i_d.di_size & 0xffffffff)),
		(void *)((unsigned long)((isize >> 32) & 0xffffffff)),
		(void *)((unsigned long)(isize & 0xffffffff)),
		(void *)((unsigned long)((offset >> 32) & 0xffffffff)),
		(void *)((unsigned long)(offset & 0xffffffff)),
		(void *)((unsigned long)delalloc),
		(void *)((unsigned long)unmapped),
		(void *)((unsigned long)unwritten),
		(void *)NULL,
		(void *)NULL);
}
#else
#define xfs_page_trace(tag, inode, page, mask)
#endif

void
linvfs_unwritten_done(
	struct buffer_head	*bh,
	int			uptodate)
{
	page_buf_t		*pb = (page_buf_t *)bh->b_private;

	ASSERT(buffer_unwritten(bh));
	bh->b_end_io = NULL;
	clear_buffer_unwritten(bh);
	if (!uptodate)
		pagebuf_ioerror(pb, EIO);
	if (atomic_dec_and_test(&pb->pb_io_remaining) == 1) {
		pagebuf_iodone(pb, 1, 1);
	}
	end_buffer_io_async(bh, uptodate);
}

/*
 * Issue transactions to convert a buffer range from unwritten
 * to written extents.
 */
STATIC void
linvfs_unwritten_convert(
	xfs_buf_t	*bp)
{
	vnode_t		*vp = XFS_BUF_FSPRIVATE(bp, vnode_t *);
	int		error;

	BUG_ON(atomic_read(&bp->pb_hold) < 1);
	VOP_BMAP(vp, XFS_BUF_OFFSET(bp), XFS_BUF_SIZE(bp),
			BMAPI_UNWRITTEN, NULL, NULL, error);
	XFS_BUF_SET_FSPRIVATE(bp, NULL);
	XFS_BUF_CLR_IODONE_FUNC(bp);
	XFS_BUF_UNDATAIO(bp);
	iput(LINVFS_GET_IP(vp));
	pagebuf_iodone(bp, 0, 0);
}

STATIC int
xfs_map_blocks(
	struct inode		*inode,
	loff_t			offset,
	ssize_t			count,
	xfs_iomap_t		*iomapp,
	int			flags)
{
	vnode_t			*vp = LINVFS_GET_VP(inode);
	int			error, niomaps = 1;

	if (((flags & (BMAPI_DIRECT|BMAPI_SYNC)) == BMAPI_DIRECT) &&
	    (offset >= i_size_read(inode)))
		count = max_t(ssize_t, count, XFS_WRITE_IO_LOG);
retry:
	VOP_BMAP(vp, offset, count, flags, iomapp, &niomaps, error);
	if ((error == EAGAIN) || (error == EIO))
		return -error;
	if (unlikely((flags & (BMAPI_WRITE|BMAPI_DIRECT)) ==
					(BMAPI_WRITE|BMAPI_DIRECT) && niomaps &&
					(iomapp->iomap_flags & IOMAP_DELAY))) {
		flags = BMAPI_ALLOCATE;
		goto retry;
	}
	if (flags & (BMAPI_WRITE|BMAPI_ALLOCATE)) {
		VMODIFY(vp);
	}
	return -error;
}

/*
 * Finds the corresponding mapping in block @map array of the
 * given @offset within a @page.
 */
STATIC xfs_iomap_t *
xfs_offset_to_map(
	struct page		*page,
	xfs_iomap_t		*iomapp,
	unsigned long		offset)
{
	loff_t			full_offset;	/* offset from start of file */

	ASSERT(offset < PAGE_CACHE_SIZE);

	full_offset = page->index;		/* NB: using 64bit number */
	full_offset <<= PAGE_CACHE_SHIFT;	/* offset from file start */
	full_offset += offset;			/* offset from page start */

	if (full_offset < iomapp->iomap_offset)
		return NULL;
	if (iomapp->iomap_offset + iomapp->iomap_bsize > full_offset)
		return iomapp;
	return NULL;
}

STATIC void
xfs_map_at_offset(
	struct page		*page,
	struct buffer_head	*bh,
	unsigned long		offset,
	int			block_bits,
	xfs_iomap_t		*iomapp)
{
	xfs_daddr_t		bn;
	loff_t			delta;
	int			sector_shift;

	ASSERT(!(iomapp->iomap_flags & IOMAP_HOLE));
	ASSERT(!(iomapp->iomap_flags & IOMAP_DELAY));
	ASSERT(iomapp->iomap_bn != IOMAP_DADDR_NULL);

	delta = page->index;
	delta <<= PAGE_CACHE_SHIFT;
	delta += offset;
	delta -= iomapp->iomap_offset;
	delta >>= block_bits;

	sector_shift = block_bits - BBSHIFT;
	bn = iomapp->iomap_bn >> sector_shift;
	bn += delta;
	ASSERT((bn << sector_shift) >= iomapp->iomap_bn);

	lock_buffer(bh);
	bh->b_blocknr = bn;
	bh->b_dev = iomapp->iomap_target->pbr_kdev;
	set_buffer_mapped(bh);
	clear_buffer_delay(bh);
}

/*
 * Look for a page at index which is unlocked and contains our
 * unwritten extent flagged buffers at its head.  Returns page
 * locked and with an extra reference count, and length of the
 * unwritten extent component on this page that we can write,
 * in units of filesystem blocks.
 */
STATIC struct page *
xfs_probe_unwritten_page(
	struct address_space	*mapping,
	unsigned long		index,
	xfs_iomap_t		*iomapp,
	page_buf_t		*pb,
	unsigned long		max_offset,
	unsigned long		*fsbs,
	unsigned int            bbits)
{
	struct page		*page;

	page = find_trylock_page(mapping, index);
	if (!page)
		return 0;

	if (page->mapping && page_has_buffers(page)) {
		struct buffer_head	*bh, *head;
		unsigned long		p_offset = 0;

		*fsbs = 0;
		bh = head = page_buffers(page);
		do {
			if (!buffer_unwritten(bh))
				break;
			if (!xfs_offset_to_map(page, iomapp, p_offset))
				break;
			if (p_offset >= max_offset)
				break;
			xfs_map_at_offset(page, bh, p_offset, bbits, iomapp);
			set_buffer_unwritten_io(bh);
			bh->b_private = pb;
			p_offset += bh->b_size;
			(*fsbs)++;
		} while ((bh = bh->b_this_page) != head);

		if (p_offset)
			return page;
	}

	unlock_page(page);
	return NULL;
}

/*
 * Look for a page at index which is unlocked and not mapped
 * yet - clustering for mmap write case.
 */
STATIC unsigned int
xfs_probe_unmapped_page(
	struct address_space	*mapping,
	unsigned long		index,
	unsigned int		pg_offset)
{
	struct page		*page;
	int			ret = 0;

	page = find_trylock_page(mapping, index);
	if (!page)
		return 0;

	if (page->mapping && PageDirty(page)) {
		if (page_has_buffers(page)) {
			struct buffer_head	*bh, *head;

			bh = head = page_buffers(page);
			do {
				if (buffer_mapped(bh) || !buffer_uptodate(bh))
					break;
				ret += bh->b_size;
				if (ret >= pg_offset)
					break;
			} while ((bh = bh->b_this_page) != head);
		} else
			ret = PAGE_CACHE_SIZE;
	}

	unlock_page(page);
	return ret;
}

STATIC unsigned int
xfs_probe_unmapped_cluster(
	struct inode		*inode,
	struct page		*startpage,
	struct buffer_head	*bh,
	struct buffer_head	*head)
{
	unsigned long		tindex, tlast, tloff;
	unsigned int		len, total = 0;
	struct address_space	*mapping = inode->i_mapping;

	/* First sum forwards in this page */
	do {
		if (buffer_mapped(bh))
			break;
		total += bh->b_size;
	} while ((bh = bh->b_this_page) != head);

	/* If we reached the end of the page, sum forwards in
	 * following pages.
	 */
	if (bh == head) {
		tlast = i_size_read(inode) >> PAGE_CACHE_SHIFT;
		/* Prune this back to avoid pathological behavior */
		tloff = min(tlast, startpage->index + 64);
		for (tindex = startpage->index + 1; tindex < tloff; tindex++) {
			len = xfs_probe_unmapped_page(mapping, tindex,
							PAGE_CACHE_SIZE);
			if (!len)
				return total;
			total += len;
		}
		if (tindex == tlast &&
		    (tloff = i_size_read(inode) & (PAGE_CACHE_SIZE - 1))) {
			total += xfs_probe_unmapped_page(mapping,
							tindex, tloff);
		}
	}
	return total;
}

/*
 * Probe for a given page (index) in the inode and test if it is delayed
 * and without unwritten buffers.  Returns page locked and with an extra
 * reference count.
 */
STATIC struct page *
xfs_probe_delalloc_page(
	struct inode		*inode,
	unsigned long		index)
{
	struct page		*page;

	page = find_trylock_page(inode->i_mapping, index);
	if (!page)
		return NULL;

	if (page->mapping && page_has_buffers(page)) {
		struct buffer_head	*bh, *head;
		int			acceptable = 0;

		bh = head = page_buffers(page);
		do {
			if (buffer_unwritten(bh)) {
				acceptable = 0;
				break;
			} else if (buffer_delay(bh)) {
				acceptable = 1;
			}
		} while ((bh = bh->b_this_page) != head);

		if (acceptable)
			return page;
	}

	unlock_page(page);
	return NULL;
}

STATIC int
xfs_map_unwritten(
	struct inode		*inode,
	struct page		*start_page,
	struct buffer_head	*head,
	struct buffer_head	*curr,
	unsigned long		p_offset,
	int			block_bits,
	xfs_iomap_t		*iomapp,
	int			startio,
	int			all_bh)
{
	struct buffer_head	*bh = curr;
	xfs_iomap_t		*tmp;
	page_buf_t		*pb;
	loff_t			offset, size;
	unsigned long		nblocks = 0;

	offset = start_page->index;
	offset <<= PAGE_CACHE_SHIFT;
	offset += p_offset;

	/* get an "empty" pagebuf to manage IO completion
	 * Proper values will be set before returning */
	pb = pagebuf_lookup(iomapp->iomap_target, 0, 0, 0);
	if (!pb)
		return -EAGAIN;

	/* Take a reference to the inode to prevent it from
	 * being reclaimed while we have outstanding unwritten
	 * extent IO on it.
	 */
	if ((igrab(inode)) != inode) {
		pagebuf_free(pb);
		return -EAGAIN;
	}

	/* Set the count to 1 initially, this will stop an I/O
	 * completion callout which happens before we have started
	 * all the I/O from calling pagebuf_iodone too early.
	 */
	atomic_set(&pb->pb_io_remaining, 1);

	/* First map forwards in the page consecutive buffers
	 * covering this unwritten extent
	 */
	do {
		if (!buffer_unwritten(bh))
			break;
		tmp = xfs_offset_to_map(start_page, iomapp, p_offset);
		if (!tmp)
			break;
		xfs_map_at_offset(start_page, bh, p_offset, block_bits, iomapp);
		set_buffer_unwritten_io(bh);
		bh->b_private = pb;
		p_offset += bh->b_size;
		nblocks++;
	} while ((bh = bh->b_this_page) != head);

	atomic_add(nblocks, &pb->pb_io_remaining);

	/* If we reached the end of the page, map forwards in any
	 * following pages which are also covered by this extent.
	 */
	if (bh == head) {
		struct address_space	*mapping = inode->i_mapping;
		unsigned long		tindex, tloff, tlast, bs;
		unsigned int		bbits = inode->i_blkbits;
		struct page		*page;

		tlast = i_size_read(inode) >> PAGE_CACHE_SHIFT;
		tloff = (iomapp->iomap_offset + iomapp->iomap_bsize) >> PAGE_CACHE_SHIFT;
		tloff = min(tlast, tloff);
		for (tindex = start_page->index + 1; tindex < tloff; tindex++) {
			page = xfs_probe_unwritten_page(mapping,
						tindex, iomapp, pb,
						PAGE_CACHE_SIZE, &bs, bbits);
			if (!page)
				break;
			nblocks += bs;
			atomic_add(bs, &pb->pb_io_remaining);
			xfs_convert_page(inode, page, iomapp, pb,
							startio, all_bh);
			/* stop if converting the next page might add
			 * enough blocks that the corresponding byte
			 * count won't fit in our ulong page buf length */
			if (nblocks >= ((ULONG_MAX - PAGE_SIZE) >> block_bits))
				goto enough;
		}

		if (tindex == tlast &&
		    (tloff = (i_size_read(inode) & (PAGE_CACHE_SIZE - 1)))) {
			page = xfs_probe_unwritten_page(mapping,
							tindex, iomapp, pb,
							tloff, &bs, bbits);
			if (page) {
				nblocks += bs;
				atomic_add(bs, &pb->pb_io_remaining);
				xfs_convert_page(inode, page, iomapp, pb,
							startio, all_bh);
				if (nblocks >= ((ULONG_MAX - PAGE_SIZE) >> block_bits))
					goto enough;
			}
		}
	}

enough:
	size = nblocks;		/* NB: using 64bit number here */
	size <<= block_bits;	/* convert fsb's to byte range */

	XFS_BUF_DATAIO(pb);
	XFS_BUF_ASYNC(pb);
	XFS_BUF_SET_SIZE(pb, size);
	XFS_BUF_SET_COUNT(pb, size);
	XFS_BUF_SET_OFFSET(pb, offset);
	XFS_BUF_SET_FSPRIVATE(pb, LINVFS_GET_VP(inode));
	XFS_BUF_SET_IODONE_FUNC(pb, linvfs_unwritten_convert);

	if (atomic_dec_and_test(&pb->pb_io_remaining) == 1) {
		pagebuf_iodone(pb, 1, 1);
	}

	return 0;
}

STATIC void
xfs_submit_page(
	struct page		*page,
	struct buffer_head	*bh_arr[],
	int			cnt)
{
	struct buffer_head	*bh;
	int			i;

	if (cnt) {
		for (i = 0; i < cnt; i++) {
			bh = bh_arr[i];
			set_buffer_async_io(bh);
			if (buffer_unwritten(bh))
				set_buffer_unwritten_io(bh);
			set_buffer_uptodate(bh);
			clear_buffer_dirty(bh);
		}

		for (i = 0; i < cnt; i++) {
			refile_buffer(bh_arr[i]);
			submit_bh(WRITE, bh_arr[i]);
		}
	} else
		unlock_page(page);
}

/*
 * Allocate & map buffers for page given the extent map. Write it out.
 * except for the original page of a writepage, this is called on
 * delalloc/unwritten pages only, for the original page it is possible
 * that the page has no mapping at all.
 */
STATIC void
xfs_convert_page(
	struct inode		*inode,
	struct page		*page,
	xfs_iomap_t		*iomapp,
	void			*private,
	int			startio,
	int			all_bh)
{
	struct buffer_head	*bh_arr[MAX_BUF_PER_PAGE], *bh, *head;
	xfs_iomap_t		*mp = iomapp, *tmp;
	unsigned long		end, offset, end_index;
	int			i = 0, index = 0;
	int			bbits = inode->i_blkbits;

	end_index = i_size_read(inode) >> PAGE_CACHE_SHIFT;
	if (page->index < end_index) {
		end = PAGE_CACHE_SIZE;
	} else {
		end = i_size_read(inode) & (PAGE_CACHE_SIZE-1);
	}
	bh = head = page_buffers(page);
	do {
		offset = i << bbits;
		if (!(PageUptodate(page) || buffer_uptodate(bh)))
			continue;
		if (buffer_mapped(bh) && all_bh &&
		    !buffer_unwritten(bh) && !buffer_delay(bh)) {
			if (startio && (offset < end)) {
				lock_buffer(bh);
				bh_arr[index++] = bh;
			}
			continue;
		}
		tmp = xfs_offset_to_map(page, mp, offset);
		if (!tmp)
			continue;
		ASSERT(!(tmp->iomap_flags & IOMAP_HOLE));
		ASSERT(!(tmp->iomap_flags & IOMAP_DELAY));

		/* If this is a new unwritten extent buffer (i.e. one
		 * that we haven't passed in private data for, we must
		 * now map this buffer too.
		 */
		if (buffer_unwritten(bh) && !bh->b_end_io) {
			ASSERT(tmp->iomap_flags & IOMAP_UNWRITTEN);
			xfs_map_unwritten(inode, page, head, bh,
					offset, bbits, tmp, startio, all_bh);
		} else if (! (buffer_unwritten(bh) && buffer_locked(bh))) {
			xfs_map_at_offset(page, bh, offset, bbits, tmp);
			if (buffer_unwritten(bh)) {
				set_buffer_unwritten_io(bh);
				bh->b_private = private;
				ASSERT(private);
			}
		}
		if (startio && (offset < end)) {
			bh_arr[index++] = bh;
		} else {
			unlock_buffer(bh);
			mark_buffer_dirty(bh);
		}
	} while (i++, (bh = bh->b_this_page) != head);

	if (startio) {
		xfs_submit_page(page, bh_arr, index);
	} else {
		unlock_page(page);
	}
}

/*
 * Convert & write out a cluster of pages in the same extent as defined
 * by mp and following the start page.
 */
STATIC void
xfs_cluster_write(
	struct inode		*inode,
	unsigned long		tindex,
	xfs_iomap_t		*iomapp,
	int			startio,
	int			all_bh)
{
	unsigned long		tlast;
	struct page		*page;

	tlast = (iomapp->iomap_offset + iomapp->iomap_bsize) >> PAGE_CACHE_SHIFT;
	for (; tindex < tlast; tindex++) {
		page = xfs_probe_delalloc_page(inode, tindex);
		if (!page)
			break;
		xfs_convert_page(inode, page, iomapp, NULL, startio, all_bh);
	}
}

/*
 * Calling this without startio set means we are being asked to make a dirty
 * page ready for freeing it's buffers.  When called with startio set then
 * we are coming from writepage.
 *
 * When called with startio set it is important that we write the WHOLE
 * page if possible.
 * The bh->b_state's cannot know if any of the blocks or which block for
 * that matter are dirty due to mmap writes, and therefore bh uptodate is
 * only vaild if the page itself isn't completely uptodate.  Some layers
 * may clear the page dirty flag prior to calling write page, under the
 * assumption the entire page will be written out; by not writing out the
 * whole page the page can be reused before all valid dirty data is
 * written out.  Note: in the case of a page that has been dirty'd by
 * mapwrite and but partially setup by block_prepare_write the
 * bh->b_states's will not agree and only ones setup by BPW/BCW will have
 * valid state, thus the whole page must be written out thing.
 */

STATIC int
xfs_page_state_convert(
	struct inode	*inode,
	struct page	*page,
	int		startio,
	int		unmapped) /* also implies page uptodate */
{
	struct buffer_head	*bh_arr[MAX_BUF_PER_PAGE], *bh, *head;
	xfs_iomap_t		*iomp, iomap;
	unsigned long		p_offset = 0, end_index;
	loff_t			offset;
	unsigned long long	end_offset;
	int			len, err, i, cnt = 0, uptodate = 1;
	int			flags = startio ? 0 : BMAPI_TRYLOCK;
	int			page_dirty = 1;


	/* Are we off the end of the file ? */
	end_index = i_size_read(inode) >> PAGE_CACHE_SHIFT;
	if (page->index >= end_index) {
		if ((page->index >= end_index + 1) ||
		    !(i_size_read(inode) & (PAGE_CACHE_SIZE - 1))) {
			err = -EIO;
			goto error;
		}
	}

	offset = (loff_t)page->index << PAGE_CACHE_SHIFT;
	end_offset = min_t(unsigned long long,
			offset + PAGE_CACHE_SIZE, i_size_read(inode));

	bh = head = page_buffers(page);
	iomp = NULL;

	len = bh->b_size;
	do {
		if (offset >= end_offset)
			break;
		if (!buffer_uptodate(bh))
			uptodate = 0;
		if (!(PageUptodate(page) || buffer_uptodate(bh)) && !startio)
			continue;

		if (iomp) {
			iomp = xfs_offset_to_map(page, &iomap, p_offset);
		}

		/*
		 * First case, map an unwritten extent and prepare for
		 * extent state conversion transaction on completion.
		 */
		if (buffer_unwritten(bh)) {
			if (!iomp) {
				err = xfs_map_blocks(inode, offset, len, &iomap,
						BMAPI_READ|BMAPI_IGNSTATE);
				if (err) {
					goto error;
				}
				iomp = xfs_offset_to_map(page, &iomap,
								p_offset);
			}
			if (iomp && startio) {
				if (!bh->b_end_io) {
					err = xfs_map_unwritten(inode, page,
							head, bh, p_offset,
							inode->i_blkbits, iomp,
							startio, unmapped);
					if (err) {
						goto error;
					}
				}
				bh_arr[cnt++] = bh;
				page_dirty = 0;
			}
		/*
		 * Second case, allocate space for a delalloc buffer.
		 * We can return EAGAIN here in the release page case.
		 */
		} else if (buffer_delay(bh)) {
			if (!iomp) {
				err = xfs_map_blocks(inode, offset, len, &iomap,
						BMAPI_ALLOCATE | flags);
				if (err) {
					goto error;
				}
				iomp = xfs_offset_to_map(page, &iomap,
								p_offset);
			}
			if (iomp) {
				xfs_map_at_offset(page, bh, p_offset,
						inode->i_blkbits, iomp);
				if (startio) {
					bh_arr[cnt++] = bh;
				} else {
					unlock_buffer(bh);
					mark_buffer_dirty(bh);
				}
				page_dirty = 0;
			}
		} else if ((buffer_uptodate(bh) || PageUptodate(page)) &&
			   (unmapped || startio)) {

			if (!buffer_mapped(bh)) {
				int	size;

				/*
				 * Getting here implies an unmapped buffer
				 * was found, and we are in a path where we
				 * need to write the whole page out.
				 */
				if (!iomp) {
					size = xfs_probe_unmapped_cluster(
							inode, page, bh, head);
					err = xfs_map_blocks(inode, offset,
							size, &iomap,
							BMAPI_WRITE|BMAPI_MMAP);
					if (err) {
						goto error;
					}
					iomp = xfs_offset_to_map(page, &iomap,
								     p_offset);
				}
				if (iomp) {
					xfs_map_at_offset(page,
							bh, p_offset,
							inode->i_blkbits, iomp);
					if (startio) {
						bh_arr[cnt++] = bh;
					} else {
						unlock_buffer(bh);
						mark_buffer_dirty(bh);
					}
					page_dirty = 0;
				}
			} else if (startio) {
				if (buffer_uptodate(bh) &&
				    !test_and_set_bit(BH_Lock, &bh->b_state)) {
					bh_arr[cnt++] = bh;
					page_dirty = 0;
				}
			}
		}
	} while (offset += len, p_offset += len,
		((bh = bh->b_this_page) != head));

	if (uptodate && bh == head)
		SetPageUptodate(page);

	if (startio)
		xfs_submit_page(page, bh_arr, cnt);

	if (iomp)
		xfs_cluster_write(inode, page->index + 1, iomp, startio, unmapped);

	return page_dirty;

error:
	for (i = 0; i < cnt; i++) {
		unlock_buffer(bh_arr[i]);
	}

	/*
	 * If it's delalloc and we have nowhere to put it,
	 * throw it away, unless the lower layers told
	 * us to try again.
	 */
	if (err != -EAGAIN) {
		if (!unmapped) {
			block_flushpage(page, 0);
		}
		ClearPageUptodate(page);
	}
	return err;
}

STATIC int
linvfs_get_block_core(
	struct inode		*inode,
	long			iblock,
	struct buffer_head	*bh_result,
	int			create,
	int			direct,
	bmapi_flags_t		flags)
{
	vnode_t			*vp = LINVFS_GET_VP(inode);
	xfs_iomap_t		iomap;
	int			retpbbm = 1;
	int			error;
	ssize_t			size;
	loff_t			offset = (loff_t)iblock << inode->i_blkbits;

	/* If we are doing writes at the end of the file,
	 * allocate in chunks
	 */
	if (create && (offset >= i_size_read(inode)) /* && !(flags & BMAPI_SYNC) */)
		size = 1 << XFS_WRITE_IO_LOG;
	else
		size = 1 << inode->i_blkbits;

	VOP_BMAP(vp, offset, size,
		create ? flags : BMAPI_READ, &iomap, &retpbbm, error);
	if (error)
		return -error;

	if (retpbbm == 0)
		return 0;

	if (iomap.iomap_bn != IOMAP_DADDR_NULL) {
		xfs_daddr_t		bn;
		loff_t			delta;

		/* For unwritten extents do not report a disk address on
		 * the read case (treat as if we're reading into a hole).
		 */
		if (create || !(iomap.iomap_flags & IOMAP_UNWRITTEN)) {
			delta = offset - iomap.iomap_offset;
			delta >>= inode->i_blkbits;

			bn = iomap.iomap_bn >> (inode->i_blkbits - BBSHIFT);
			bn += delta;

			bh_result->b_blocknr = bn;
			set_buffer_mapped(bh_result);
		}
		if (create && (iomap.iomap_flags & IOMAP_UNWRITTEN)) {
			set_buffer_unwritten(bh_result);
			set_buffer_delay(bh_result);
		}
	}

	/* If this is a realtime file, data might be on a new device */
	bh_result->b_dev = iomap.iomap_target->pbr_kdev;

	/* If we previously allocated a block out beyond eof and
	 * we are now coming back to use it then we will need to
	 * flag it as new even if it has a disk address.
	 */
	if (create &&
	    ((!buffer_mapped(bh_result) && !buffer_uptodate(bh_result)) ||
	     (offset >= i_size_read(inode)))) {
		set_buffer_new(bh_result);
	}

	if (iomap.iomap_flags & IOMAP_DELAY) {
		if (unlikely(direct))
			BUG();
		if (create) {
			set_buffer_mapped(bh_result);
		}
		set_buffer_delay(bh_result);
	}

	return 0;
}

int
linvfs_get_block(
	struct inode		*inode,
	long			iblock,
	struct buffer_head	*bh_result,
	int			create)
{
	return linvfs_get_block_core(inode, iblock, bh_result,
					create, 0, BMAPI_WRITE);
}

STATIC int
linvfs_get_block_sync(
	struct inode		*inode,
	long			iblock,
	struct buffer_head	*bh_result,
	int			create)
{
	return linvfs_get_block_core(inode, iblock, bh_result,
					create, 0, BMAPI_SYNC|BMAPI_WRITE);
}

STATIC int
linvfs_get_block_direct(
	struct inode		*inode,
	long			iblock,
	struct buffer_head	*bh_result,
	int			create)
{
	return linvfs_get_block_core(inode, iblock, bh_result,
					create, 1, BMAPI_WRITE|BMAPI_DIRECT);
}

STATIC int
linvfs_bmap(
	struct address_space	*mapping,
	long			block)
{
	struct inode		*inode = (struct inode *)mapping->host;
	vnode_t			*vp = LINVFS_GET_VP(inode);
	int			error;

	vn_trace_entry(vp, "linvfs_bmap", (inst_t *)__return_address);

	VOP_RWLOCK(vp, VRWLOCK_READ);
	VOP_FLUSH_PAGES(vp, (xfs_off_t)0, -1, 0, FI_REMAPF, error);
	VOP_RWUNLOCK(vp, VRWLOCK_READ);
	return generic_block_bmap(mapping, block, linvfs_get_block_direct);
}

STATIC int
linvfs_readpage(
	struct file		*unused,
	struct page		*page)
{
	return block_read_full_page(page, linvfs_get_block);
}

STATIC void
xfs_count_page_state(
	struct page		*page,
	int			*delalloc,
	int			*unmapped,
	int			*unwritten)
{
	struct buffer_head	*bh, *head;

	*delalloc = *unmapped = *unwritten = 0;

	bh = head = page_buffers(page);
	do {
		if (buffer_uptodate(bh) && !buffer_mapped(bh))
			(*unmapped) = 1;
		else if (buffer_unwritten(bh) && !buffer_delay(bh))
			clear_buffer_unwritten(bh);
		else if (buffer_unwritten(bh))
			(*unwritten) = 1;
		else if (buffer_delay(bh))
			(*delalloc) = 1;
	} while ((bh = bh->b_this_page) != head);
}


/*
 * writepage: Called from one of two places:
 *
 * 1. we are flushing a delalloc buffer head.
 *
 * 2. we are writing out a dirty page. Typically the page dirty
 *    state is cleared before we get here. In this case is it
 *    conceivable we have no buffer heads.
 *
 * For delalloc space on the page we need to allocate space and
 * flush it. For unmapped buffer heads on the page we should
 * allocate space if the page is uptodate. For any other dirty
 * buffer heads on the page we should flush them.
 *
 * If we detect that a transaction would be required to flush
 * the page, we have to check the process flags first, if we
 * are already in a transaction or disk I/O during allocations
 * is off, we need to fail the writepage and redirty the page.
 */

STATIC int
linvfs_writepage(
	struct page		*page)
{
	int			error;
	int			need_trans;
	int			delalloc, unmapped, unwritten;
	struct inode		*inode = page->mapping->host;
	xfs_pflags_t		pflags;

	xfs_page_trace(XFS_WRITEPAGE_ENTER, inode, page, 0);

	/*
	 * We need a transaction if:
	 *  1. There are delalloc buffers on the page
	 *  2. The page is uptodate and we have unmapped buffers
	 *  3. The page is uptodate and we have no buffers
	 *  4. There are unwritten buffers on the page
	 */

	if (!page_has_buffers(page)) {
		unmapped = 1;
		need_trans = 1;
	} else {
		xfs_count_page_state(page, &delalloc, &unmapped, &unwritten);
		if (!PageUptodate(page))
			unmapped = 0;
		need_trans = delalloc + unmapped + unwritten;
	}

	/*
	 * If we need a transaction and the process flags say
	 * we are already in a transaction, or no IO is allowed
	 * then mark the page dirty again and leave the page
	 * as is.
	 */

	if ((PFLAGS_TEST_FSTRANS() || PFLAGS_TEST_NOIO()) && need_trans)
		goto out_fail;

	/*
	 * Delay hooking up buffer heads until we have
	 * made our go/no-go decision.
	 */
	if (!page_has_buffers(page))
		create_empty_buffers(page, inode->i_dev, 1 << inode->i_blkbits);

	/*
	 * Convert delayed allocate, unwritten or unmapped space
	 * to real space and flush out to disk.
	 */
	if (need_trans)
		PFLAGS_SET_NOIO(&pflags);
	error = xfs_page_state_convert(inode, page, 1, unmapped);
	if (need_trans)
		PFLAGS_RESTORE(&pflags);
	if (error == -EAGAIN)
		goto out_fail;

	if (unlikely(error < 0)) {
		unlock_page(page);
		return error;
	}

	return 0;

out_fail:
	SetPageDirty(page);
	unlock_page(page);
	return 0;
}

/*
 * Called to move a page into cleanable state - and from there
 * to be released. Possibly the page is already clean. We always
 * have buffer heads in this call.
 *
 * Returns 0 if the page is ok to release, 1 otherwise.
 *
 * Possible scenarios are:
 *
 * 1. We are being called to release a page which has been written
 *    to via regular I/O. buffer heads will be dirty and possibly
 *    delalloc. If no delalloc buffer heads in this case then we
 *    can just return zero.
 *
 * 2. We are called to release a page which has been written via
 *    mmap, all we need to do is ensure there is no delalloc
 *    state in the buffer heads, if not we can let the caller
 *    free them and we should come back later via writepage.
 */
STATIC int
linvfs_release_page(
	struct page		*page,
	int			gfp_mask)
{
	struct inode		*inode = page->mapping->host;
	int			dirty, delalloc, unmapped, unwritten;

	xfs_page_trace(XFS_RELEASEPAGE_ENTER, inode, page, gfp_mask);

	xfs_count_page_state(page, &delalloc, &unmapped, &unwritten);
	if (!delalloc && !unwritten)
		return 1;

	if (!(gfp_mask & __GFP_FS))
		return 0;

	/* If we are already inside a transaction or the thread cannot
	 * do I/O, we cannot release this page.
	 */
	if (PFLAGS_TEST_FSTRANS() || PFLAGS_TEST_NOIO())
		return 0;

	/*
	 * Convert delalloc space to real space, do not flush the
	 * data out to disk, that will be done by the caller.
	 * Never need to allocate space here - we will always
	 * come back to writepage in that case.
	 */
	dirty = xfs_page_state_convert(inode, page, 0, 0);
	return (dirty == 0 && !unwritten) ? 1 : 0;
}

STATIC int
linvfs_prepare_write(
	struct file		*file,
	struct page		*page,
	unsigned int		from,
	unsigned int		to)
{
	if (file && (file->f_flags & O_SYNC)) {
		return block_prepare_write(page, from, to,
						linvfs_get_block_sync);
	} else {
		return block_prepare_write(page, from, to,
						linvfs_get_block);
	}
}

/*
 * Initiate I/O on a kiobuf of user memory
 */
STATIC int
linvfs_direct_IO(
	int			rw,
	struct inode		*inode,
	struct kiobuf		*iobuf,
	unsigned long		blocknr,
	int			blocksize)
{
	struct page		**maplist;
	size_t			page_offset;
	page_buf_t		*pb;
	xfs_iomap_t		iomap;
	int			error = 0;
	int			pb_flags, map_flags, pg_index = 0;
	size_t			length, total;
	loff_t			offset, map_size;
	size_t			size;
	vnode_t			*vp = LINVFS_GET_VP(inode);

	/* Note - although the iomap could have a 64-bit size,
	 * kiobuf->length is only an int, so the min(map_size, length)
	 * test will keep us from overflowing the pagebuf size_t size.
	 */
	total = length = iobuf->length;
	offset = blocknr;
	offset <<= inode->i_blkbits;

	maplist = iobuf->maplist;
	page_offset = iobuf->offset;

	map_flags = (rw ? BMAPI_WRITE : BMAPI_READ) | BMAPI_DIRECT;
	pb_flags = (rw ? PBF_WRITE : PBF_READ) | PBF_FORCEIO | PBF_DIRECTIO;
	while (length) {
		error = xfs_map_blocks(inode, offset, length, &iomap, map_flags);
		if (error)
			break;
		BUG_ON(iomap.iomap_flags & IOMAP_DELAY);

		map_size = iomap.iomap_bsize - iomap.iomap_delta;
		size = (size_t)min(map_size, (loff_t)length);

		if ((iomap.iomap_flags & IOMAP_HOLE) ||
		    ((iomap.iomap_flags & IOMAP_UNWRITTEN) && rw == READ)) {
			size_t	zero_len = size;

			if (rw == WRITE)
				break;

			/* Need to zero it all */
			while (zero_len) {
				struct page	*page;
				size_t		pg_len;

				pg_len = min((size_t)
						(PAGE_CACHE_SIZE - page_offset),
						zero_len);

				page = maplist[pg_index];

				memset(kmap(page) + page_offset, 0, pg_len);
				flush_dcache_page(page);
				kunmap(page);

				zero_len -= pg_len;
				if ((pg_len + page_offset) == PAGE_CACHE_SIZE) {
					pg_index++;
					page_offset = 0;
				} else {
					page_offset = (page_offset + pg_len) &
							~PAGE_CACHE_MASK;
				}
			}
		} else {
			int	pg_count;

			pg_count = (size + page_offset + PAGE_CACHE_SIZE - 1)
					>> PAGE_CACHE_SHIFT;
			if ((pb = pagebuf_lookup(iomap.iomap_target, offset,
						size, pb_flags)) == NULL) {
				error = -ENOMEM;
				break;
			}
			/* Need to hook up pagebuf to kiobuf pages */
			pb->pb_pages = &maplist[pg_index];
			pb->pb_offset = page_offset;
			pb->pb_page_count = pg_count;
			pb->pb_bn = iomap.iomap_bn + (iomap.iomap_delta >> BBSHIFT);

			error = pagebuf_iostart(pb, pb_flags);
			if (!error && (iomap.iomap_flags & IOMAP_UNWRITTEN)) {
				VOP_BMAP(vp, XFS_BUF_OFFSET(pb),
					XFS_BUF_SIZE(pb),
					BMAPI_UNWRITTEN, NULL, NULL, error);
			}

			pagebuf_rele(pb);

			if (error) {
				if (error > 0)
					error = -error;
				break;
			}

			page_offset = (page_offset + size) & ~PAGE_CACHE_MASK;
			if (page_offset)
				pg_count--;
			pg_index += pg_count;
		}

		offset += size;
		length -= size;
	}

	return (error ? error : (int)(total - length));
}


struct address_space_operations linvfs_aops = {
	.readpage		= linvfs_readpage,
	.writepage		= linvfs_writepage,
	.sync_page		= block_sync_page,
	.releasepage		= linvfs_release_page,
	.prepare_write		= linvfs_prepare_write,
	.commit_write		= generic_commit_write,
	.bmap			= linvfs_bmap,
	.direct_IO		= linvfs_direct_IO,
};
