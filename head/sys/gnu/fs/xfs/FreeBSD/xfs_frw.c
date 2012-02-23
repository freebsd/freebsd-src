/*
 * Copyright (c) 2000-2003,2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_bit.h"
#include "xfs_log.h"
#include "xfs_inum.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_alloc.h"
#include "xfs_dmapi.h"
#include "xfs_quota.h"
#include "xfs_mount.h"
#include "xfs_bmap_btree.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_attr_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_bmap.h"
#include "xfs_btree.h"
#include "xfs_ialloc.h"
#include "xfs_rtalloc.h"
#include "xfs_error.h"
#include "xfs_itable.h"
#include "xfs_rw.h"
#include "xfs_acl.h"
#include "xfs_cap.h"
#include "xfs_mac.h"
#include "xfs_attr.h"
#include "xfs_inode_item.h"
#include "xfs_buf_item.h"
#include "xfs_utils.h"
#include "xfs_iomap.h"

#if defined(XFS_RW_TRACE)
void
xfs_rw_enter_trace(
	int		tag,
	xfs_iocore_t	*io,
	const char	*buf,
	size_t		size,
	loff_t		offset,
	int		ioflags)
{
	xfs_inode_t	*ip = XFS_IO_INODE(io);

	if (ip->i_rwtrace == NULL)
		return;
	ktrace_enter(ip->i_rwtrace,
		(void *)(unsigned long)tag,
		(void *)ip,
		(void *)((unsigned long)((ip->i_d.di_size >> 32) & 0xffffffff)),
		(void *)((unsigned long)(ip->i_d.di_size & 0xffffffff)),
		(void *)(__psint_t)buf,
		(void *)((unsigned long)size),
		(void *)((unsigned long)((offset >> 32) & 0xffffffff)),
		(void *)((unsigned long)(offset & 0xffffffff)),
		(void *)((unsigned long)ioflags),
		(void *)((unsigned long)((io->io_new_size >> 32) & 0xffffffff)),
		(void *)((unsigned long)(io->io_new_size & 0xffffffff)),
		(void *)NULL,
		(void *)NULL,
		(void *)NULL,
		(void *)NULL,
		(void *)NULL);
}

void
xfs_inval_cached_trace(
	xfs_iocore_t	*io,
	xfs_off_t	offset,
	xfs_off_t	len,
	xfs_off_t	first,
	xfs_off_t	last)
{
	xfs_inode_t	*ip = XFS_IO_INODE(io);

	if (ip->i_rwtrace == NULL)
		return;
	ktrace_enter(ip->i_rwtrace,
		(void *)(__psint_t)XFS_INVAL_CACHED,
		(void *)ip,
		(void *)((unsigned long)((offset >> 32) & 0xffffffff)),
		(void *)((unsigned long)(offset & 0xffffffff)),
		(void *)((unsigned long)((len >> 32) & 0xffffffff)),
		(void *)((unsigned long)(len & 0xffffffff)),
		(void *)((unsigned long)((first >> 32) & 0xffffffff)),
		(void *)((unsigned long)(first & 0xffffffff)),
		(void *)((unsigned long)((last >> 32) & 0xffffffff)),
		(void *)((unsigned long)(last & 0xffffffff)),
		(void *)NULL,
		(void *)NULL,
		(void *)NULL,
		(void *)NULL,
		(void *)NULL,
		(void *)NULL);
}
#endif

/*
 *	xfs_iozero
 *
 *	xfs_iozero clears the specified range of buffer supplied,
 *	and marks all the affected blocks as valid and modified.  If
 *	an affected block is not allocated, it will be allocated.  If
 *	an affected block is not completely overwritten, and is not
 *	valid before the operation, it will be read from disk before
 *	being partially zeroed.
 */
STATIC int
xfs_iozero(
	xfs_vnode_t		*vp,	/* vnode			*/
	xfs_off_t		pos,	/* offset in file		*/
	size_t			count,	/* size of data to zero		*/
	xfs_off_t		end_size)	/* max file size to set */
{
	int			status;
	status = 0; /* XXXKAN: */
#ifdef XXXKAN
	unsigned		bytes;
	struct page		*page;
	struct address_space	*mapping;
	char			*kaddr;

	mapping = ip->i_mapping;
	do {
		unsigned long index, offset;

		offset = (pos & (PAGE_CACHE_SIZE -1)); /* Within page */
		index = pos >> PAGE_CACHE_SHIFT;
		bytes = PAGE_CACHE_SIZE - offset;
		if (bytes > count)
			bytes = count;

		status = -ENOMEM;
		page = grab_cache_page(mapping, index);
		if (!page)
			break;

		kaddr = kmap(page);
		status = mapping->a_ops->prepare_write(NULL, page, offset,
							offset + bytes);
		if (status) {
			goto unlock;
		}

		memset((void *) (kaddr + offset), 0, bytes);
		flush_dcache_page(page);
		status = mapping->a_ops->commit_write(NULL, page, offset,
							offset + bytes);
		if (!status) {
			pos += bytes;
			count -= bytes;
			if (pos > i_size_read(ip))
				i_size_write(ip, pos < end_size ? pos : end_size);
		}

unlock:
		kunmap(page);
		unlock_page(page);
		page_cache_release(page);
		if (status)
			break;
	} while (count);
#endif
	return (-status);
}

ssize_t			/* bytes read, or (-)  error */
xfs_read(
	bhv_desc_t      *bdp,
	uio_t		*uio,
	int		ioflags,
	cred_t          *credp)
{
	ssize_t		ret, size;
	xfs_fsize_t	n;
	xfs_inode_t	*ip;
	xfs_mount_t	*mp;

	ip = XFS_BHVTOI(bdp);
	mp = ip->i_mount;

	XFS_STATS_INC(xs_read_calls);

	if (unlikely(ioflags & IO_ISDIRECT)) {
		if (((__psint_t)buf & BBMASK) ||
		    (uio->uio_offset & mp->m_blockmask) ||
		    (uio->uio_resid & mp->m_blockmask)) {
			if (uio->uio_offset >= ip->i_d.di_size) {
				return (0);
			}
			return EINVAL;
		}
	}

	if (uio->uio_resid == 0)
		return 0;
	n = XFS_MAXIOFFSET(mp) - uio->uio_offset;
	if (n <= 0)
		return EFBIG;

	size = (n < uio->uio_resid)? n : uio->uio_resid;

	if (XFS_FORCED_SHUTDOWN(mp)) {
		return EIO;
	}

	xfs_ilock(ip, XFS_IOLOCK_SHARED);

#ifdef XXX
	if (DM_EVENT_ENABLED(BHV_TO_VNODE(bdp)->v_vfsp, ip, DM_EVENT_READ) &&
	    !(ioflags & IO_INVIS)) {
		int error;
		vrwlock_t locktype = VRWLOCK_READ;
		int dmflags = FILP_DELAY_FLAG(file) | DM_SEM_FLAG_RD(ioflags);

		error = XFS_SEND_DATA(mp, DM_EVENT_READ, BHV_TO_VNODE(bdp),
			uio->uio_offset, size, dmflags, &locktype);
		if (error) {
			xfs_iunlock(ip, XFS_IOLOCK_SHARED);
			return (error);
		}
	}
#endif

	ret = xfs_read_file(mp, ip, uio, ioflags);

	xfs_iunlock(ip, XFS_IOLOCK_SHARED);

	XFS_STATS_ADD(xs_read_bytes, ret);

	if (likely((ioflags & IO_INVIS) == 0)) {
		xfs_ichgtime(ip, XFS_ICHGTIME_ACC);
	}

	return ret;
}

/*
 * This routine is called to handle zeroing any space in the last
 * block of the file that is beyond the EOF.  We do this since the
 * size is being increased without writing anything to that block
 * and we don't want anyone to read the garbage on the disk.
 */
STATIC int				/* error (positive) */
xfs_zero_last_block(
	xfs_vnode_t	*vp,
	xfs_iocore_t	*io,
	xfs_fsize_t	isize,
	xfs_fsize_t	end_size)
{
	xfs_fileoff_t	last_fsb;
	xfs_mount_t	*mp;
	int		nimaps;
	int		zero_offset;
	int		zero_len;
	int		error = 0;
	xfs_bmbt_irec_t	imap;
	xfs_off_t	loff;

	ASSERT(ismrlocked(io->io_lock, MR_UPDATE) != 0);

	mp = io->io_mount;

	zero_offset = XFS_B_FSB_OFFSET(mp, isize);
	if (zero_offset == 0) {
		/*
		 * There are no extra bytes in the last block on disk to
		 * zero, so return.
		 */
		return 0;
	}

	last_fsb = XFS_B_TO_FSBT(mp, isize);
	nimaps = 1;
	error = XFS_BMAPI(mp, NULL, io, last_fsb, 1, 0, NULL, 0, &imap,
			  &nimaps, NULL, NULL);
	if (error) {
		return error;
	}
	ASSERT(nimaps > 0);
	/*
	 * If the block underlying isize is just a hole, then there
	 * is nothing to zero.
	 */
	if (imap.br_startblock == HOLESTARTBLOCK) {
		return 0;
	}
	/*
	 * Zero the part of the last block beyond the EOF, and write it
	 * out sync.  We need to drop the ilock while we do this so we
	 * don't deadlock when the buffer cache calls back to us.
	 */
	XFS_IUNLOCK(mp, io, XFS_ILOCK_EXCL| XFS_EXTSIZE_RD);
	loff = XFS_FSB_TO_B(mp, last_fsb);

	zero_len = mp->m_sb.sb_blocksize - zero_offset;

	error = xfs_iozero(vp, loff + zero_offset, zero_len, end_size);

	XFS_ILOCK(mp, io, XFS_ILOCK_EXCL|XFS_EXTSIZE_RD);
	ASSERT(error >= 0);
	return error;
}

/*
 * Zero any on disk space between the current EOF and the new,
 * larger EOF.  This handles the normal case of zeroing the remainder
 * of the last block in the file and the unusual case of zeroing blocks
 * out beyond the size of the file.  This second case only happens
 * with fixed size extents and when the system crashes before the inode
 * size was updated but after blocks were allocated.  If fill is set,
 * then any holes in the range are filled and zeroed.  If not, the holes
 * are left alone as holes.
 */

int					/* error (positive) */
xfs_zero_eof(
	xfs_vnode_t	*vp,
	xfs_iocore_t	*io,
	xfs_off_t	offset,		/* starting I/O offset */
	xfs_fsize_t	isize,		/* current inode size */
	xfs_fsize_t	end_size)	/* terminal inode size */
{
	xfs_fileoff_t	start_zero_fsb;
	xfs_fileoff_t	end_zero_fsb;
	xfs_fileoff_t	zero_count_fsb;
	xfs_fileoff_t	last_fsb;
	xfs_extlen_t	buf_len_fsb;
	xfs_mount_t	*mp;
	int		nimaps;
	int		error = 0;
	xfs_bmbt_irec_t	imap;

	ASSERT(ismrlocked(io->io_lock, MR_UPDATE));
	ASSERT(ismrlocked(io->io_iolock, MR_UPDATE));
	ASSERT(offset > isize);

	mp = io->io_mount;

	/*
	 * First handle zeroing the block on which isize resides.
	 * We only zero a part of that block so it is handled specially.
	 */
	error = xfs_zero_last_block(vp, io, isize, end_size);
	if (error) {
		ASSERT(ismrlocked(io->io_lock, MR_UPDATE));
		ASSERT(ismrlocked(io->io_iolock, MR_UPDATE));
		return error;
	}

	/*
	 * Calculate the range between the new size and the old
	 * where blocks needing to be zeroed may exist.  To get the
	 * block where the last byte in the file currently resides,
	 * we need to subtract one from the size and truncate back
	 * to a block boundary.  We subtract 1 in case the size is
	 * exactly on a block boundary.
	 */
	last_fsb = isize ? XFS_B_TO_FSBT(mp, isize - 1) : (xfs_fileoff_t)-1;
	start_zero_fsb = XFS_B_TO_FSB(mp, (xfs_ufsize_t)isize);
	end_zero_fsb = XFS_B_TO_FSBT(mp, offset - 1);
	ASSERT((xfs_sfiloff_t)last_fsb < (xfs_sfiloff_t)start_zero_fsb);
	if (last_fsb == end_zero_fsb) {
		/*
		 * The size was only incremented on its last block.
		 * We took care of that above, so just return.
		 */
		return 0;
	}

	ASSERT(start_zero_fsb <= end_zero_fsb);
	while (start_zero_fsb <= end_zero_fsb) {
		nimaps = 1;
		zero_count_fsb = end_zero_fsb - start_zero_fsb + 1;
		error = XFS_BMAPI(mp, NULL, io, start_zero_fsb, zero_count_fsb,
				  0, NULL, 0, &imap, &nimaps, NULL, NULL);
		if (error) {
			ASSERT(ismrlocked(io->io_lock, MR_UPDATE));
			ASSERT(ismrlocked(io->io_iolock, MR_UPDATE));
			return error;
		}
		ASSERT(nimaps > 0);

		if (imap.br_state == XFS_EXT_UNWRITTEN ||
		    imap.br_startblock == HOLESTARTBLOCK) {
			/*
			 * This loop handles initializing pages that were
			 * partially initialized by the code below this
			 * loop. It basically zeroes the part of the page
			 * that sits on a hole and sets the page as P_HOLE
			 * and calls remapf if it is a mapped file.
			 */
			start_zero_fsb = imap.br_startoff + imap.br_blockcount;
			ASSERT(start_zero_fsb <= (end_zero_fsb + 1));
			continue;
		}

		/*
		 * There are blocks in the range requested.
		 * Zero them a single write at a time.  We actually
		 * don't zero the entire range returned if it is
		 * too big and simply loop around to get the rest.
		 * That is not the most efficient thing to do, but it
		 * is simple and this path should not be exercised often.
		 */
		buf_len_fsb = XFS_FILBLKS_MIN(imap.br_blockcount,
					      mp->m_writeio_blocks << 8);
		/*
		 * Drop the inode lock while we're doing the I/O.
		 * We'll still have the iolock to protect us.
		 */
		XFS_IUNLOCK(mp, io, XFS_ILOCK_EXCL|XFS_EXTSIZE_RD);

		error = xfs_iozero(vp,
				   XFS_FSB_TO_B(mp, start_zero_fsb),
				   XFS_FSB_TO_B(mp, buf_len_fsb),
				   end_size);

		if (error) {
			goto out_lock;
		}

		start_zero_fsb = imap.br_startoff + buf_len_fsb;
		ASSERT(start_zero_fsb <= (end_zero_fsb + 1));

		XFS_ILOCK(mp, io, XFS_ILOCK_EXCL|XFS_EXTSIZE_RD);
	}

	return 0;

out_lock:

	XFS_ILOCK(mp, io, XFS_ILOCK_EXCL|XFS_EXTSIZE_RD);
	ASSERT(error >= 0);
	return error;
}

ssize_t				/* bytes written, or (-) error */
xfs_write(
	bhv_desc_t      *bdp,
	uio_t		*uio,
	int		ioflag,
	cred_t          *credp)
{
	xfs_inode_t	*xip;
	xfs_mount_t	*mp;
	ssize_t		ret = 0;
	int		error = 0;
	xfs_fsize_t     isize, new_size;
	xfs_fsize_t	n, limit;
	xfs_fsize_t	size;
	xfs_iocore_t    *io;
	xfs_vnode_t	*vp;
	int		iolock;
	//int		eventsent = 0;
	vrwlock_t	locktype;
	xfs_off_t	offset_c;
	xfs_off_t	*offset;
	xfs_off_t	pos;

	XFS_STATS_INC(xs_write_calls);

	vp = BHV_TO_VNODE(bdp);
	xip = XFS_BHVTOI(bdp);

	io = &xip->i_iocore;
	mp = io->io_mount;

	if (XFS_FORCED_SHUTDOWN(xip->i_mount)) {
		return EIO;
	}

	size = uio->uio_resid;
	pos = offset_c = uio->uio_offset;
	offset = &offset_c;

	if (unlikely(ioflag & IO_ISDIRECT)) {
		if (((__psint_t)buf & BBMASK) ||
		    (*offset & mp->m_blockmask) ||
		    (size  & mp->m_blockmask)) {
			return EINVAL;
		}
		iolock = XFS_IOLOCK_SHARED;
		locktype = VRWLOCK_WRITE_DIRECT;
	} else {
		if (io->io_flags & XFS_IOCORE_RT)
			return EINVAL;
		iolock = XFS_IOLOCK_EXCL;
		locktype = VRWLOCK_WRITE;
	}

	iolock = XFS_IOLOCK_EXCL;
	locktype = VRWLOCK_WRITE;

	xfs_ilock(xip, XFS_ILOCK_EXCL|iolock);

	isize = xip->i_d.di_size;
	limit = XFS_MAXIOFFSET(mp);

	if (ioflag & O_APPEND)
		*offset = isize;

//start:
	n = limit - *offset;
	if (n <= 0) {
		xfs_iunlock(xip, XFS_ILOCK_EXCL|iolock);
		return EFBIG;
	}
	if (n < size)
		size = n;

	new_size = *offset + size;
	if (new_size > isize) {
		io->io_new_size = new_size;
	}

#ifdef RMC
	/* probably be a long time before if ever that we do dmapi */
	if ((DM_EVENT_ENABLED(vp->v_vfsp, xip, DM_EVENT_WRITE) &&
	    !(ioflags & IO_INVIS) && !eventsent)) {
		loff_t		savedsize = *offset;
		int dmflags = FILP_DELAY_FLAG(file) | DM_SEM_FLAG_RD(ioflags);

		xfs_iunlock(xip, XFS_ILOCK_EXCL);
		error = XFS_SEND_DATA(xip->i_mount, DM_EVENT_WRITE, vp,
				      *offset, size,
				      dmflags, &locktype);
		if (error) {
			if (iolock) xfs_iunlock(xip, iolock);
			return -error;
		}
		xfs_ilock(xip, XFS_ILOCK_EXCL);
		eventsent = 1;

		/*
		 * The iolock was dropped and reaquired in XFS_SEND_DATA
		 * so we have to recheck the size when appending.
		 * We will only "goto start;" once, since having sent the
		 * event prevents another call to XFS_SEND_DATA, which is
		 * what allows the size to change in the first place.
		 */
		if ((file->f_flags & O_APPEND) &&
		    savedsize != xip->i_d.di_size) {
			*offset = isize = xip->i_d.di_size;
			goto start;
		}
	}
#endif

	/*
	 * If the offset is beyond the size of the file, we have a couple
	 * of things to do. First, if there is already space allocated
	 * we need to either create holes or zero the disk or ...
	 *
	 * If there is a page where the previous size lands, we need
	 * to zero it out up to the new size.
	 */

	if (!(ioflag & IO_ISDIRECT) && (*offset > isize && isize)) {
		error = xfs_zero_eof(BHV_TO_VNODE(bdp), io, *offset,
			isize, *offset + size);
		if (error) {
			xfs_iunlock(xip, XFS_ILOCK_EXCL|iolock);
			return(-error);
		}
	}
	xfs_iunlock(xip, XFS_ILOCK_EXCL);

#if 0
	/*
	 * If we're writing the file then make sure to clear the
	 * setuid and setgid bits if the process is not being run
	 * by root.  This keeps people from modifying setuid and
	 * setgid binaries.
	 */

	if (((xip->i_d.di_mode & S_ISUID) ||
	    ((xip->i_d.di_mode & (S_ISGID | S_IXGRP)) ==
		(S_ISGID | S_IXGRP))) &&
	     !capable(CAP_FSETID)) {
		error = xfs_write_clear_setuid(xip);
		if (likely(!error))
			error = -remove_suid(file->f_dentry);
		if (unlikely(error)) {
			xfs_iunlock(xip, iolock);
			goto out_unlock_mutex;
		}
	}
#endif


//retry:
	if (unlikely(ioflag & IO_ISDIRECT)) {

#ifdef RMC
		xfs_off_t	pos = *offset;
		struct address_space *mapping = file->f_dentry->d_inode->i_mapping;
		struct inode    *inode = mapping->host;

		ret = precheck_file_write(file, inode, &size,  &pos);
		if (ret || size == 0)
			goto error;

		xfs_inval_cached_pages(vp, io, pos, 1, 1);
		inode->i_ctime = inode->i_mtime = CURRENT_TIME;
		/* mark_inode_dirty_sync(inode); - we do this later */

		xfs_rw_enter_trace(XFS_DIOWR_ENTER, io, buf, size, pos, ioflags);
		ret = generic_file_direct_IO(WRITE, file, (char *)buf, size, pos);
		xfs_inval_cached_pages(vp, io, pos, 1, 1);
		if (ret > 0)
			*offset += ret;
#endif
	} else {
		xfs_rw_enter_trace(XFS_WRITE_ENTER, io, buf, size, *offset, ioflags);
		ret = xfs_write_file(xip,uio,ioflag);
	}

	xfs_ichgtime(xip, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);


//error:
	if (ret <= 0) {
		if (iolock)
			xfs_rwunlock(bdp, locktype);
		return ret;
	}

	XFS_STATS_ADD(xs_write_bytes, ret);

	if (*offset > xip->i_d.di_size) {
		xfs_ilock(xip, XFS_ILOCK_EXCL);
		if (*offset > xip->i_d.di_size) {
			printf("xfs_write look at doing more here %s:%d\n",__FILE__,__LINE__);
#ifdef RMC
			struct inode	*inode = LINVFS_GET_IP(vp);
			i_size_write(inode, *offset);
			mark_inode_dirty_sync(inode);
#endif

			xip->i_d.di_size = *offset;
			xip->i_update_core = 1;
			xip->i_update_size = 1;
		}
		xfs_iunlock(xip, XFS_ILOCK_EXCL);
	}

	/* Handle various SYNC-type writes */
#if 0
//	if ((file->f_flags & O_SYNC) || IS_SYNC(inode)) {
#endif
	if (ioflag & IO_SYNC) {
		/*
		 * If we're treating this as O_DSYNC and we have not updated the
		 * size, force the log.
		 */
		if (!(mp->m_flags & XFS_MOUNT_OSYNCISOSYNC) &&
		    !(xip->i_update_size)) {
			xfs_inode_log_item_t	*iip = xip->i_itemp;

			/*
			 * If an allocation transaction occurred
			 * without extending the size, then we have to force
			 * the log up the proper point to ensure that the
			 * allocation is permanent.  We can't count on
			 * the fact that buffered writes lock out direct I/O
			 * writes - the direct I/O write could have extended
			 * the size nontransactionally, then finished before
			 * we started.  xfs_write_file will think that the file
			 * didn't grow but the update isn't safe unless the
			 * size change is logged.
			 *
			 * Force the log if we've committed a transaction
			 * against the inode or if someone else has and
			 * the commit record hasn't gone to disk (e.g.
			 * the inode is pinned).  This guarantees that
			 * all changes affecting the inode are permanent
			 * when we return.
			 */
			if (iip && iip->ili_last_lsn) {
				xfs_log_force(mp, iip->ili_last_lsn,
						XFS_LOG_FORCE | XFS_LOG_SYNC);
			} else if (xfs_ipincount(xip) > 0) {
				xfs_log_force(mp, (xfs_lsn_t)0,
						XFS_LOG_FORCE | XFS_LOG_SYNC);
			}

		} else {
			xfs_trans_t	*tp;

			/*
			 * O_SYNC or O_DSYNC _with_ a size update are handled
			 * the same way.
			 *
			 * If the write was synchronous then we need to make
			 * sure that the inode modification time is permanent.
			 * We'll have updated the timestamp above, so here
			 * we use a synchronous transaction to log the inode.
			 * It's not fast, but it's necessary.
			 *
			 * If this a dsync write and the size got changed
			 * non-transactionally, then we need to ensure that
			 * the size change gets logged in a synchronous
			 * transaction.
			 */

			tp = xfs_trans_alloc(mp, XFS_TRANS_WRITE_SYNC);
			if ((error = xfs_trans_reserve(tp, 0,
						      XFS_SWRITE_LOG_RES(mp),
						      0, 0, 0))) {
				/* Transaction reserve failed */
				xfs_trans_cancel(tp, 0);
			} else {
				/* Transaction reserve successful */
				xfs_ilock(xip, XFS_ILOCK_EXCL);
				xfs_trans_ijoin(tp, xip, XFS_ILOCK_EXCL);
				xfs_trans_ihold(tp, xip);
				xfs_trans_log_inode(tp, xip, XFS_ILOG_CORE);
				xfs_trans_set_sync(tp);
				error = xfs_trans_commit(tp, 0, NULL);
				xfs_iunlock(xip, XFS_ILOCK_EXCL);
			}
			if (error)
				goto out_unlock_internal;
		}

		xfs_rwunlock(bdp, locktype);
		return ret;

	} /* (ioflags & O_SYNC) */

out_unlock_internal:
	xfs_rwunlock(bdp, locktype);
#if 0
out_unlock_mutex:
	if (need_i_mutex)
		mutex_unlock(&inode->i_mutex);
#endif
 //out_nounlocks:
	return -error;
}

/*
 * Initiate IO on given buffer.
 */
int
xfs_buf_iorequest(struct xfs_buf *bp)
{
	bp->b_flags &= ~(B_INVAL|B_DONE);
	bp->b_ioflags &= ~BIO_ERROR;

	if (bp->b_flags & B_ASYNC)
		BUF_KERNPROC(bp);

	if (bp->b_vp == NULL) {
		if (bp->b_iocmd == BIO_WRITE) {
			bp->b_flags &= ~(B_DELWRI | B_DEFERRED);
			bufobj_wref(bp->b_bufobj);
		}

		bp->b_iooffset = (bp->b_blkno << BBSHIFT);
		bstrategy(bp);
	} else {
		if (bp->b_iocmd == BIO_WRITE) {
			/* Mark the buffer clean */
			bundirty(bp);
			bufobj_wref(bp->b_bufobj);
			vfs_busy_pages(bp, 1);
		} else if (bp->b_iocmd == BIO_READ) {
			vfs_busy_pages(bp, 0);
		}
		bp->b_iooffset = dbtob(bp->b_blkno);
		bstrategy(bp);
	}
	return 0;
}

/*
 * All xfs metadata buffers except log state machine buffers
 * get this attached as their b_bdstrat callback function.
 * This is so that we can catch a buffer
 * after prematurely unpinning it to forcibly shutdown the filesystem.
 */
int
xfs_bdstrat_cb(struct xfs_buf *bp)
{
	xfs_mount_t	*mp;

	mp = XFS_BUF_FSPRIVATE3(bp, xfs_mount_t *);
	if (!XFS_FORCED_SHUTDOWN(mp)) {
		xfs_buf_iorequest(bp);
		return 0;
	} else {
		xfs_buftrace("XFS__BDSTRAT IOERROR", bp);
		/*
		 * Metadata write that didn't get logged but
		 * written delayed anyway. These aren't associated
		 * with a transaction, and can be ignored.
		 */
		if (XFS_BUF_IODONE_FUNC(bp) == NULL &&
		    (XFS_BUF_ISREAD(bp)) == 0)
			return (xfs_bioerror_relse(bp));
		else
			return (xfs_bioerror(bp));
	}
}


int
xfs_bmap(bhv_desc_t	*bdp,
	xfs_off_t	offset,
	ssize_t		count,
	int		flags,
	xfs_iomap_t	*iomapp,
	int		*niomaps)
{
	xfs_inode_t	*ip = XFS_BHVTOI(bdp);
	xfs_iocore_t	*io = &ip->i_iocore;

	ASSERT((ip->i_d.di_mode & S_IFMT) == S_IFREG);
	ASSERT(((ip->i_d.di_flags & XFS_DIFLAG_REALTIME) != 0) ==
	       ((ip->i_iocore.io_flags & XFS_IOCORE_RT) != 0));

	return xfs_iomap(io, offset, count, flags, iomapp, niomaps);
}

/*
 * Wrapper around bdstrat so that we can stop data
 * from going to disk in case we are shutting down the filesystem.
 * Typically user data goes thru this path; one of the exceptions
 * is the superblock.
 */
int
xfsbdstrat(
	struct xfs_mount	*mp,
	struct xfs_buf		*bp)
{
	ASSERT(mp);
	if (!XFS_FORCED_SHUTDOWN(mp)) {

		xfs_buf_iorequest(bp);
		return 0;
	}

	xfs_buftrace("XFSBDSTRAT IOERROR", bp);
	return (xfs_bioerror_relse(bp));
}

/*
 * If the underlying (data/log/rt) device is readonly, there are some
 * operations that cannot proceed.
 */
int
xfs_dev_is_read_only(
	xfs_mount_t		*mp,
	char			*message)
{
	if (xfs_readonly_buftarg(mp->m_ddev_targp) ||
	    xfs_readonly_buftarg(mp->m_logdev_targp) ||
	    (mp->m_rtdev_targp && xfs_readonly_buftarg(mp->m_rtdev_targp))) {
		cmn_err(CE_NOTE,
			"XFS: %s required on read-only device.", message);
		cmn_err(CE_NOTE,
			"XFS: write access unavailable, cannot proceed.");
		return EROFS;
	}
	return 0;
}
