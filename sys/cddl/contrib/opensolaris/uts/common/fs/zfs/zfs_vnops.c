/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/* Portions Copyright 2007 Jeremy Teo */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/resource.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/kmem.h>
#include <sys/taskq.h>
#include <sys/uio.h>
#include <sys/atomic.h>
#include <sys/namei.h>
#include <sys/mman.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/unistd.h>
#include <sys/zfs_vfsops.h>
#include <sys/zfs_dir.h>
#include <sys/zfs_acl.h>
#include <sys/zfs_ioctl.h>
#include <sys/fs/zfs.h>
#include <sys/dmu.h>
#include <sys/spa.h>
#include <sys/txg.h>
#include <sys/dbuf.h>
#include <sys/zap.h>
#include <sys/dirent.h>
#include <sys/policy.h>
#include <sys/sunddi.h>
#include <sys/filio.h>
#include <sys/zfs_ctldir.h>
#include <sys/dnlc.h>
#include <sys/zfs_rlock.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/sf_buf.h>
#include <sys/sched.h>

/*
 * Programming rules.
 *
 * Each vnode op performs some logical unit of work.  To do this, the ZPL must
 * properly lock its in-core state, create a DMU transaction, do the work,
 * record this work in the intent log (ZIL), commit the DMU transaction,
 * and wait the the intent log to commit if it's is a synchronous operation.
 * Morover, the vnode ops must work in both normal and log replay context.
 * The ordering of events is important to avoid deadlocks and references
 * to freed memory.  The example below illustrates the following Big Rules:
 *
 *  (1) A check must be made in each zfs thread for a mounted file system.
 *	This is done avoiding races using ZFS_ENTER(zfsvfs).
 *	A ZFS_EXIT(zfsvfs) is needed before all returns.
 *
 *  (2)	VN_RELE() should always be the last thing except for zil_commit()
 *	(if necessary) and ZFS_EXIT(). This is for 3 reasons:
 *	First, if it's the last reference, the vnode/znode
 *	can be freed, so the zp may point to freed memory.  Second, the last
 *	reference will call zfs_zinactive(), which may induce a lot of work --
 *	pushing cached pages (which acquires range locks) and syncing out
 *	cached atime changes.  Third, zfs_zinactive() may require a new tx,
 *	which could deadlock the system if you were already holding one.
 *
 *  (3)	All range locks must be grabbed before calling dmu_tx_assign(),
 *	as they can span dmu_tx_assign() calls.
 *
 *  (4)	Always pass zfsvfs->z_assign as the second argument to dmu_tx_assign().
 *	In normal operation, this will be TXG_NOWAIT.  During ZIL replay,
 *	it will be a specific txg.  Either way, dmu_tx_assign() never blocks.
 *	This is critical because we don't want to block while holding locks.
 *	Note, in particular, that if a lock is sometimes acquired before
 *	the tx assigns, and sometimes after (e.g. z_lock), then failing to
 *	use a non-blocking assign can deadlock the system.  The scenario:
 *
 *	Thread A has grabbed a lock before calling dmu_tx_assign().
 *	Thread B is in an already-assigned tx, and blocks for this lock.
 *	Thread A calls dmu_tx_assign(TXG_WAIT) and blocks in txg_wait_open()
 *	forever, because the previous txg can't quiesce until B's tx commits.
 *
 *	If dmu_tx_assign() returns ERESTART and zfsvfs->z_assign is TXG_NOWAIT,
 *	then drop all locks, call dmu_tx_wait(), and try again.
 *
 *  (5)	If the operation succeeded, generate the intent log entry for it
 *	before dropping locks.  This ensures that the ordering of events
 *	in the intent log matches the order in which they actually occurred.
 *
 *  (6)	At the end of each vnode op, the DMU tx must always commit,
 *	regardless of whether there were any errors.
 *
 *  (7)	After dropping all locks, invoke zil_commit(zilog, seq, foid)
 *	to ensure that synchronous semantics are provided when necessary.
 *
 * In general, this is how things should be ordered in each vnode op:
 *
 *	ZFS_ENTER(zfsvfs);		// exit if unmounted
 * top:
 *	zfs_dirent_lock(&dl, ...)	// lock directory entry (may VN_HOLD())
 *	rw_enter(...);			// grab any other locks you need
 *	tx = dmu_tx_create(...);	// get DMU tx
 *	dmu_tx_hold_*();		// hold each object you might modify
 *	error = dmu_tx_assign(tx, zfsvfs->z_assign);	// try to assign
 *	if (error) {
 *		rw_exit(...);		// drop locks
 *		zfs_dirent_unlock(dl);	// unlock directory entry
 *		VN_RELE(...);		// release held vnodes
 *		if (error == ERESTART && zfsvfs->z_assign == TXG_NOWAIT) {
 *			dmu_tx_wait(tx);
 *			dmu_tx_abort(tx);
 *			goto top;
 *		}
 *		dmu_tx_abort(tx);	// abort DMU tx
 *		ZFS_EXIT(zfsvfs);	// finished in zfs
 *		return (error);		// really out of space
 *	}
 *	error = do_real_work();		// do whatever this VOP does
 *	if (error == 0)
 *		zfs_log_*(...);		// on success, make ZIL entry
 *	dmu_tx_commit(tx);		// commit DMU tx -- error or not
 *	rw_exit(...);			// drop locks
 *	zfs_dirent_unlock(dl);		// unlock directory entry
 *	VN_RELE(...);			// release held vnodes
 *	zil_commit(zilog, seq, foid);	// synchronous when necessary
 *	ZFS_EXIT(zfsvfs);		// finished in zfs
 *	return (error);			// done, report error
 */
/* ARGSUSED */
static int
zfs_open(vnode_t **vpp, int flag, cred_t *cr)
{
	znode_t	*zp = VTOZ(*vpp);

	/* Keep a count of the synchronous opens in the znode */
	if (flag & (FSYNC | FDSYNC))
		atomic_inc_32(&zp->z_sync_cnt);
	return (0);
}

/* ARGSUSED */
static int
zfs_close(vnode_t *vp, int flag, int count, offset_t offset, cred_t *cr)
{
	znode_t	*zp = VTOZ(vp);

	/* Decrement the synchronous opens in the znode */
	if (flag & (FSYNC | FDSYNC))
		atomic_dec_32(&zp->z_sync_cnt);

	/*
	 * Clean up any locks held by this process on the vp.
	 */
	cleanlocks(vp, ddi_get_pid(), 0);
	cleanshares(vp, ddi_get_pid());

	return (0);
}

/*
 * Lseek support for finding holes (cmd == _FIO_SEEK_HOLE) and
 * data (cmd == _FIO_SEEK_DATA). "off" is an in/out parameter.
 */
static int
zfs_holey(vnode_t *vp, u_long cmd, offset_t *off)
{
	znode_t	*zp = VTOZ(vp);
	uint64_t noff = (uint64_t)*off; /* new offset */
	uint64_t file_sz;
	int error;
	boolean_t hole;

	file_sz = zp->z_phys->zp_size;
	if (noff >= file_sz)  {
		return (ENXIO);
	}

	if (cmd == _FIO_SEEK_HOLE)
		hole = B_TRUE;
	else
		hole = B_FALSE;

	error = dmu_offset_next(zp->z_zfsvfs->z_os, zp->z_id, hole, &noff);

	/* end of file? */
	if ((error == ESRCH) || (noff > file_sz)) {
		/*
		 * Handle the virtual hole at the end of file.
		 */
		if (hole) {
			*off = file_sz;
			return (0);
		}
		return (ENXIO);
	}

	if (noff < *off)
		return (error);
	*off = noff;
	return (error);
}

/* ARGSUSED */
static int
zfs_ioctl(vnode_t *vp, u_long com, intptr_t data, int flag, cred_t *cred,
    int *rvalp)
{
	offset_t off;
	int error;
	zfsvfs_t *zfsvfs;

	switch (com) {
	    case _FIOFFS:
		return (0);

		/*
		 * The following two ioctls are used by bfu.  Faking out,
		 * necessary to avoid bfu errors.
		 */
	    case _FIOGDIO:
	    case _FIOSDIO:
		return (0);

	    case _FIO_SEEK_DATA:
	    case _FIO_SEEK_HOLE:
		if (ddi_copyin((void *)data, &off, sizeof (off), flag))
			return (EFAULT);

		zfsvfs = VTOZ(vp)->z_zfsvfs;
		ZFS_ENTER(zfsvfs);

		/* offset parameter is in/out */
		error = zfs_holey(vp, com, &off);
		ZFS_EXIT(zfsvfs);
		if (error)
			return (error);
		if (ddi_copyout(&off, (void *)data, sizeof (off), flag))
			return (EFAULT);
		return (0);
	}
	return (ENOTTY);
}

/*
 * When a file is memory mapped, we must keep the IO data synchronized
 * between the DMU cache and the memory mapped pages.  What this means:
 *
 * On Write:	If we find a memory mapped page, we write to *both*
 *		the page and the dmu buffer.
 *
 * NOTE: We will always "break up" the IO into PAGESIZE uiomoves when
 *	the file is memory mapped.
 */
static int
mappedwrite(vnode_t *vp, int nbytes, uio_t *uio, dmu_tx_t *tx)
{
	znode_t *zp = VTOZ(vp);
	objset_t *os = zp->z_zfsvfs->z_os;
	vm_object_t obj;
	vm_page_t m;
	struct sf_buf *sf;
	int64_t start, off;
	int len = nbytes;
	int error = 0;
	uint64_t dirbytes;

	ASSERT(vp->v_mount != NULL);
	obj = vp->v_object;
	ASSERT(obj != NULL);

	start = uio->uio_loffset;
	off = start & PAGEOFFSET;
	dirbytes = 0;
	VM_OBJECT_LOCK(obj);
	for (start &= PAGEMASK; len > 0; start += PAGESIZE) {
		uint64_t bytes = MIN(PAGESIZE - off, len);
		uint64_t fsize;

again:
		if ((m = vm_page_lookup(obj, OFF_TO_IDX(start))) != NULL &&
		    vm_page_is_valid(m, (vm_offset_t)off, bytes)) {
			uint64_t woff;
			caddr_t va;

			if (vm_page_sleep_if_busy(m, FALSE, "zfsmwb"))
				goto again;
			fsize = obj->un_pager.vnp.vnp_size;
			vm_page_busy(m);
			vm_page_lock_queues();
			vm_page_undirty(m);
			vm_page_unlock_queues();
			VM_OBJECT_UNLOCK(obj);
			if (dirbytes > 0) {
				error = dmu_write_uio(os, zp->z_id, uio,
				    dirbytes, tx);
				dirbytes = 0;
			}
			if (error == 0) {
				sched_pin();
				sf = sf_buf_alloc(m, SFB_CPUPRIVATE);
				va = (caddr_t)sf_buf_kva(sf);
				woff = uio->uio_loffset - off;
				error = uiomove(va + off, bytes, UIO_WRITE, uio);
				/*
				 * The uiomove() above could have been partially
				 * successful, that's why we call dmu_write()
				 * below unconditionally. The page was marked
				 * non-dirty above and we would lose the changes
				 * without doing so. If the uiomove() failed
				 * entirely, well, we just write what we got
				 * before one more time.
				 */
				dmu_write(os, zp->z_id, woff,
				    MIN(PAGESIZE, fsize - woff), va, tx);
				sf_buf_free(sf);
				sched_unpin();
			}
			VM_OBJECT_LOCK(obj);
			vm_page_wakeup(m);
		} else {
			if (__predict_false(obj->cache != NULL)) {
				vm_page_cache_free(obj, OFF_TO_IDX(start),
				    OFF_TO_IDX(start) + 1);
			}
			dirbytes += bytes;
		}
		len -= bytes;
		off = 0;
		if (error)
			break;
	}
	VM_OBJECT_UNLOCK(obj);
	if (error == 0 && dirbytes > 0)
		error = dmu_write_uio(os, zp->z_id, uio, dirbytes, tx);
	return (error);
}

/*
 * When a file is memory mapped, we must keep the IO data synchronized
 * between the DMU cache and the memory mapped pages.  What this means:
 *
 * On Read:	We "read" preferentially from memory mapped pages,
 *		else we default from the dmu buffer.
 *
 * NOTE: We will always "break up" the IO into PAGESIZE uiomoves when
 *	the file is memory mapped.
 */
static int
mappedread(vnode_t *vp, int nbytes, uio_t *uio)
{
	znode_t *zp = VTOZ(vp);
	objset_t *os = zp->z_zfsvfs->z_os;
	vm_object_t obj;
	vm_page_t m;
	struct sf_buf *sf;
	int64_t start, off;
	caddr_t va;
	int len = nbytes;
	int error = 0;
	uint64_t dirbytes;

	ASSERT(vp->v_mount != NULL);
	obj = vp->v_object;
	ASSERT(obj != NULL);

	start = uio->uio_loffset;
	off = start & PAGEOFFSET;
	dirbytes = 0;
	VM_OBJECT_LOCK(obj);
	for (start &= PAGEMASK; len > 0; start += PAGESIZE) {
		uint64_t bytes = MIN(PAGESIZE - off, len);

again:
		if ((m = vm_page_lookup(obj, OFF_TO_IDX(start))) != NULL &&
		    vm_page_is_valid(m, (vm_offset_t)off, bytes)) {
			if (vm_page_sleep_if_busy(m, FALSE, "zfsmrb"))
				goto again;
			vm_page_busy(m);
			VM_OBJECT_UNLOCK(obj);
			if (dirbytes > 0) {
				error = dmu_read_uio(os, zp->z_id, uio,
				    dirbytes);
				dirbytes = 0;
			}
			if (error == 0) {
				sched_pin();
				sf = sf_buf_alloc(m, SFB_CPUPRIVATE);
				va = (caddr_t)sf_buf_kva(sf);
				error = uiomove(va + off, bytes, UIO_READ, uio);
				sf_buf_free(sf);
				sched_unpin();
			}
			VM_OBJECT_LOCK(obj);
			vm_page_wakeup(m);
		} else if (m != NULL && uio->uio_segflg == UIO_NOCOPY) {
			/*
			 * The code below is here to make sendfile(2) work
			 * correctly with ZFS. As pointed out by ups@
			 * sendfile(2) should be changed to use VOP_GETPAGES(),
			 * but it pessimize performance of sendfile/UFS, that's
			 * why I handle this special case in ZFS code.
			 */
			if (vm_page_sleep_if_busy(m, FALSE, "zfsmrb"))
				goto again;
			vm_page_busy(m);
			VM_OBJECT_UNLOCK(obj);
			if (dirbytes > 0) {
				error = dmu_read_uio(os, zp->z_id, uio,
				    dirbytes);
				dirbytes = 0;
			}
			if (error == 0) {
				sched_pin();
				sf = sf_buf_alloc(m, SFB_CPUPRIVATE);
				va = (caddr_t)sf_buf_kva(sf);
				error = dmu_read(os, zp->z_id, start + off,
				    bytes, (void *)(va + off));
				sf_buf_free(sf);
				sched_unpin();
			}
			VM_OBJECT_LOCK(obj);
			vm_page_wakeup(m);
			if (error == 0)
				uio->uio_resid -= bytes;
		} else {
			dirbytes += bytes;
		}
		len -= bytes;
		off = 0;
		if (error)
			break;
	}
	VM_OBJECT_UNLOCK(obj);
	if (error == 0 && dirbytes > 0)
		error = dmu_read_uio(os, zp->z_id, uio, dirbytes);
	return (error);
}

offset_t zfs_read_chunk_size = 1024 * 1024; /* Tunable */

/*
 * Read bytes from specified file into supplied buffer.
 *
 *	IN:	vp	- vnode of file to be read from.
 *		uio	- structure supplying read location, range info,
 *			  and return buffer.
 *		ioflag	- SYNC flags; used to provide FRSYNC semantics.
 *		cr	- credentials of caller.
 *
 *	OUT:	uio	- updated offset and range, buffer filled.
 *
 *	RETURN:	0 if success
 *		error code if failure
 *
 * Side Effects:
 *	vp - atime updated if byte count > 0
 */
/* ARGSUSED */
static int
zfs_read(vnode_t *vp, uio_t *uio, int ioflag, cred_t *cr, caller_context_t *ct)
{
	znode_t		*zp = VTOZ(vp);
	zfsvfs_t	*zfsvfs = zp->z_zfsvfs;
	objset_t	*os = zfsvfs->z_os;
	ssize_t		n, nbytes;
	int		error;
	rl_t		*rl;

	ZFS_ENTER(zfsvfs);

	/*
	 * Validate file offset
	 */
	if (uio->uio_loffset < (offset_t)0) {
		ZFS_EXIT(zfsvfs);
		return (EINVAL);
	}

	/*
	 * Fasttrack empty reads
	 */
	if (uio->uio_resid == 0) {
		ZFS_EXIT(zfsvfs);
		return (0);
	}

	/*
	 * Check for mandatory locks
	 */
	if (MANDMODE((mode_t)zp->z_phys->zp_mode)) {
		if (error = chklock(vp, FREAD,
		    uio->uio_loffset, uio->uio_resid, uio->uio_fmode, ct)) {
			ZFS_EXIT(zfsvfs);
			return (error);
		}
	}

	/*
	 * If we're in FRSYNC mode, sync out this znode before reading it.
	 */
	if (ioflag & FRSYNC)
		zil_commit(zfsvfs->z_log, zp->z_last_itx, zp->z_id);

	/*
	 * Lock the range against changes.
	 */
	rl = zfs_range_lock(zp, uio->uio_loffset, uio->uio_resid, RL_READER);

	/*
	 * If we are reading past end-of-file we can skip
	 * to the end; but we might still need to set atime.
	 */
	if (uio->uio_loffset >= zp->z_phys->zp_size) {
		error = 0;
		goto out;
	}

	ASSERT(uio->uio_loffset < zp->z_phys->zp_size);
	n = MIN(uio->uio_resid, zp->z_phys->zp_size - uio->uio_loffset);

	while (n > 0) {
		nbytes = MIN(n, zfs_read_chunk_size -
		    P2PHASE(uio->uio_loffset, zfs_read_chunk_size));

		if (vn_has_cached_data(vp))
			error = mappedread(vp, nbytes, uio);
		else
			error = dmu_read_uio(os, zp->z_id, uio, nbytes);
		if (error)
			break;

		n -= nbytes;
	}

out:
	zfs_range_unlock(rl);

	ZFS_ACCESSTIME_STAMP(zfsvfs, zp);
	ZFS_EXIT(zfsvfs);
	return (error);
}

/*
 * Fault in the pages of the first n bytes specified by the uio structure.
 * 1 byte in each page is touched and the uio struct is unmodified.
 * Any error will exit this routine as this is only a best
 * attempt to get the pages resident. This is a copy of ufs_trans_touch().
 */
static void
zfs_prefault_write(ssize_t n, struct uio *uio)
{
	struct iovec *iov;
	ulong_t cnt, incr;
	caddr_t p;

	if (uio->uio_segflg != UIO_USERSPACE)
		return;

	iov = uio->uio_iov;

	while (n) {
		cnt = MIN(iov->iov_len, n);
		if (cnt == 0) {
			/* empty iov entry */
			iov++;
			continue;
		}
		n -= cnt;
		/*
		 * touch each page in this segment.
		 */
		p = iov->iov_base;
		while (cnt) {
			if (fubyte(p) == -1)
				return;
			incr = MIN(cnt, PAGESIZE);
			p += incr;
			cnt -= incr;
		}
		/*
		 * touch the last byte in case it straddles a page.
		 */
		p--;
		if (fubyte(p) == -1)
			return;
		iov++;
	}
}

/*
 * Write the bytes to a file.
 *
 *	IN:	vp	- vnode of file to be written to.
 *		uio	- structure supplying write location, range info,
 *			  and data buffer.
 *		ioflag	- IO_APPEND flag set if in append mode.
 *		cr	- credentials of caller.
 *
 *	OUT:	uio	- updated offset and range.
 *
 *	RETURN:	0 if success
 *		error code if failure
 *
 * Timestamps:
 *	vp - ctime|mtime updated if byte count > 0
 */
/* ARGSUSED */
static int
zfs_write(vnode_t *vp, uio_t *uio, int ioflag, cred_t *cr, caller_context_t *ct)
{
	znode_t		*zp = VTOZ(vp);
	rlim64_t	limit = MAXOFFSET_T;
	ssize_t		start_resid = uio->uio_resid;
	ssize_t		tx_bytes;
	uint64_t	end_size;
	dmu_tx_t	*tx;
	zfsvfs_t	*zfsvfs = zp->z_zfsvfs;
	zilog_t		*zilog = zfsvfs->z_log;
	offset_t	woff;
	ssize_t		n, nbytes;
	rl_t		*rl;
	int		max_blksz = zfsvfs->z_max_blksz;
	int		error;

	/*
	 * Fasttrack empty write
	 */
	n = start_resid;
	if (n == 0)
		return (0);

	if (limit == RLIM64_INFINITY || limit > MAXOFFSET_T)
		limit = MAXOFFSET_T;

	ZFS_ENTER(zfsvfs);

	/*
	 * Pre-fault the pages to ensure slow (eg NFS) pages
	 * don't hold up txg.
	 */
	zfs_prefault_write(n, uio);

	/*
	 * If in append mode, set the io offset pointer to eof.
	 */
	if (ioflag & IO_APPEND) {
		/*
		 * Range lock for a file append:
		 * The value for the start of range will be determined by
		 * zfs_range_lock() (to guarantee append semantics).
		 * If this write will cause the block size to increase,
		 * zfs_range_lock() will lock the entire file, so we must
		 * later reduce the range after we grow the block size.
		 */
		rl = zfs_range_lock(zp, 0, n, RL_APPEND);
		if (rl->r_len == UINT64_MAX) {
			/* overlocked, zp_size can't change */
			woff = uio->uio_loffset = zp->z_phys->zp_size;
		} else {
			woff = uio->uio_loffset = rl->r_off;
		}
	} else {
		woff = uio->uio_loffset;
		/*
		 * Validate file offset
		 */
		if (woff < 0) {
			ZFS_EXIT(zfsvfs);
			return (EINVAL);
		}

		/*
		 * If we need to grow the block size then zfs_range_lock()
		 * will lock a wider range than we request here.
		 * Later after growing the block size we reduce the range.
		 */
		rl = zfs_range_lock(zp, woff, n, RL_WRITER);
	}

	if (woff >= limit) {
		zfs_range_unlock(rl);
		ZFS_EXIT(zfsvfs);
		return (EFBIG);
	}

	if ((woff + n) > limit || woff > (limit - n))
		n = limit - woff;

	/*
	 * Check for mandatory locks
	 */
	if (MANDMODE((mode_t)zp->z_phys->zp_mode) &&
	    (error = chklock(vp, FWRITE, woff, n, uio->uio_fmode, ct)) != 0) {
		zfs_range_unlock(rl);
		ZFS_EXIT(zfsvfs);
		return (error);
	}
	end_size = MAX(zp->z_phys->zp_size, woff + n);

	/*
	 * Write the file in reasonable size chunks.  Each chunk is written
	 * in a separate transaction; this keeps the intent log records small
	 * and allows us to do more fine-grained space accounting.
	 */
	while (n > 0) {
		/*
		 * Start a transaction.
		 */
		woff = uio->uio_loffset;
		tx = dmu_tx_create(zfsvfs->z_os);
		dmu_tx_hold_bonus(tx, zp->z_id);
		dmu_tx_hold_write(tx, zp->z_id, woff, MIN(n, max_blksz));
		error = dmu_tx_assign(tx, zfsvfs->z_assign);
		if (error) {
			if (error == ERESTART &&
			    zfsvfs->z_assign == TXG_NOWAIT) {
				dmu_tx_wait(tx);
				dmu_tx_abort(tx);
				continue;
			}
			dmu_tx_abort(tx);
			break;
		}

		/*
		 * If zfs_range_lock() over-locked we grow the blocksize
		 * and then reduce the lock range.  This will only happen
		 * on the first iteration since zfs_range_reduce() will
		 * shrink down r_len to the appropriate size.
		 */
		if (rl->r_len == UINT64_MAX) {
			uint64_t new_blksz;

			if (zp->z_blksz > max_blksz) {
				ASSERT(!ISP2(zp->z_blksz));
				new_blksz = MIN(end_size, SPA_MAXBLOCKSIZE);
			} else {
				new_blksz = MIN(end_size, max_blksz);
			}
			zfs_grow_blocksize(zp, new_blksz, tx);
			zfs_range_reduce(rl, woff, n);
		}

		/*
		 * XXX - should we really limit each write to z_max_blksz?
		 * Perhaps we should use SPA_MAXBLOCKSIZE chunks?
		 */
		nbytes = MIN(n, max_blksz - P2PHASE(woff, max_blksz));

		if (woff + nbytes > zp->z_phys->zp_size)
			vnode_pager_setsize(vp, woff + nbytes);

		rw_enter(&zp->z_map_lock, RW_READER);

		tx_bytes = uio->uio_resid;
		if (vn_has_cached_data(vp)) {
			rw_exit(&zp->z_map_lock);
			error = mappedwrite(vp, nbytes, uio, tx);
		} else {
			error = dmu_write_uio(zfsvfs->z_os, zp->z_id,
			    uio, nbytes, tx);
			rw_exit(&zp->z_map_lock);
		}
		tx_bytes -= uio->uio_resid;

		/*
		 * If we made no progress, we're done.  If we made even
		 * partial progress, update the znode and ZIL accordingly.
		 */
		if (tx_bytes == 0) {
			dmu_tx_commit(tx);
			ASSERT(error != 0);
			break;
		}

		/*
		 * Clear Set-UID/Set-GID bits on successful write if not
		 * privileged and at least one of the excute bits is set.
		 *
		 * It would be nice to to this after all writes have
		 * been done, but that would still expose the ISUID/ISGID
		 * to another app after the partial write is committed.
		 */
		mutex_enter(&zp->z_acl_lock);
		if ((zp->z_phys->zp_mode & (S_IXUSR | (S_IXUSR >> 3) |
		    (S_IXUSR >> 6))) != 0 &&
		    (zp->z_phys->zp_mode & (S_ISUID | S_ISGID)) != 0 &&
		    secpolicy_vnode_setid_retain(cr,
		    (zp->z_phys->zp_mode & S_ISUID) != 0 &&
		    zp->z_phys->zp_uid == 0) != 0) {
			    zp->z_phys->zp_mode &= ~(S_ISUID | S_ISGID);
		}
		mutex_exit(&zp->z_acl_lock);

		/*
		 * Update time stamp.  NOTE: This marks the bonus buffer as
		 * dirty, so we don't have to do it again for zp_size.
		 */
		zfs_time_stamper(zp, CONTENT_MODIFIED, tx);

		/*
		 * Update the file size (zp_size) if it has changed;
		 * account for possible concurrent updates.
		 */
		while ((end_size = zp->z_phys->zp_size) < uio->uio_loffset)
			(void) atomic_cas_64(&zp->z_phys->zp_size, end_size,
			    uio->uio_loffset);
		zfs_log_write(zilog, tx, TX_WRITE, zp, woff, tx_bytes, ioflag);
		dmu_tx_commit(tx);

		if (error != 0)
			break;
		ASSERT(tx_bytes == nbytes);
		n -= nbytes;
	}

	zfs_range_unlock(rl);

	/*
	 * If we're in replay mode, or we made no progress, return error.
	 * Otherwise, it's at least a partial write, so it's successful.
	 */
	if (zfsvfs->z_assign >= TXG_INITIAL || uio->uio_resid == start_resid) {
		ZFS_EXIT(zfsvfs);
		return (error);
	}

	if (ioflag & (FSYNC | FDSYNC))
		zil_commit(zilog, zp->z_last_itx, zp->z_id);

	ZFS_EXIT(zfsvfs);
	return (0);
}

void
zfs_get_done(dmu_buf_t *db, void *vzgd)
{
	zgd_t *zgd = (zgd_t *)vzgd;
	rl_t *rl = zgd->zgd_rl;
	vnode_t *vp = ZTOV(rl->r_zp);
	int vfslocked;

	vfslocked = VFS_LOCK_GIANT(vp->v_vfsp);
	dmu_buf_rele(db, vzgd);
	zfs_range_unlock(rl);
	VN_RELE(vp);
	zil_add_vdev(zgd->zgd_zilog, DVA_GET_VDEV(BP_IDENTITY(zgd->zgd_bp)));
	kmem_free(zgd, sizeof (zgd_t));
	VFS_UNLOCK_GIANT(vfslocked);
}

/*
 * Get data to generate a TX_WRITE intent log record.
 */
int
zfs_get_data(void *arg, lr_write_t *lr, char *buf, zio_t *zio)
{
	zfsvfs_t *zfsvfs = arg;
	objset_t *os = zfsvfs->z_os;
	znode_t *zp;
	uint64_t off = lr->lr_offset;
	dmu_buf_t *db;
	rl_t *rl;
	zgd_t *zgd;
	int dlen = lr->lr_length;		/* length of user data */
	int error = 0;

	ASSERT(zio);
	ASSERT(dlen != 0);

	/*
	 * Nothing to do if the file has been removed
	 */
	if (zfs_zget(zfsvfs, lr->lr_foid, &zp) != 0)
		return (ENOENT);
	if (zp->z_unlinked) {
		VN_RELE(ZTOV(zp));
		return (ENOENT);
	}

	/*
	 * Write records come in two flavors: immediate and indirect.
	 * For small writes it's cheaper to store the data with the
	 * log record (immediate); for large writes it's cheaper to
	 * sync the data and get a pointer to it (indirect) so that
	 * we don't have to write the data twice.
	 */
	if (buf != NULL) { /* immediate write */
		rl = zfs_range_lock(zp, off, dlen, RL_READER);
		/* test for truncation needs to be done while range locked */
		if (off >= zp->z_phys->zp_size) {
			error = ENOENT;
			goto out;
		}
		VERIFY(0 == dmu_read(os, lr->lr_foid, off, dlen, buf));
	} else { /* indirect write */
		uint64_t boff; /* block starting offset */

		/*
		 * Have to lock the whole block to ensure when it's
		 * written out and it's checksum is being calculated
		 * that no one can change the data. We need to re-check
		 * blocksize after we get the lock in case it's changed!
		 */
		for (;;) {
			if (ISP2(zp->z_blksz)) {
				boff = P2ALIGN_TYPED(off, zp->z_blksz,
				    uint64_t);
			} else {
				boff = 0;
			}
			dlen = zp->z_blksz;
			rl = zfs_range_lock(zp, boff, dlen, RL_READER);
			if (zp->z_blksz == dlen)
				break;
			zfs_range_unlock(rl);
		}
		/* test for truncation needs to be done while range locked */
		if (off >= zp->z_phys->zp_size) {
			error = ENOENT;
			goto out;
		}
		zgd = (zgd_t *)kmem_alloc(sizeof (zgd_t), KM_SLEEP);
		zgd->zgd_rl = rl;
		zgd->zgd_zilog = zfsvfs->z_log;
		zgd->zgd_bp = &lr->lr_blkptr;
		VERIFY(0 == dmu_buf_hold(os, lr->lr_foid, boff, zgd, &db));
		ASSERT(boff == db->db_offset);
		lr->lr_blkoff = off - boff;
		error = dmu_sync(zio, db, &lr->lr_blkptr,
		    lr->lr_common.lrc_txg, zfs_get_done, zgd);
		ASSERT(error == EEXIST || lr->lr_length <= zp->z_blksz);
		if (error == 0) {
			zil_add_vdev(zfsvfs->z_log,
			    DVA_GET_VDEV(BP_IDENTITY(&lr->lr_blkptr)));
		}
		/*
		 * If we get EINPROGRESS, then we need to wait for a
		 * write IO initiated by dmu_sync() to complete before
		 * we can release this dbuf.  We will finish everything
		 * up in the zfs_get_done() callback.
		 */
		if (error == EINPROGRESS)
			return (0);
		dmu_buf_rele(db, zgd);
		kmem_free(zgd, sizeof (zgd_t));
	}
out:
	zfs_range_unlock(rl);
	VN_RELE(ZTOV(zp));
	return (error);
}

/*ARGSUSED*/
static int
zfs_access(vnode_t *vp, int mode, int flags, cred_t *cr)
{
	znode_t *zp = VTOZ(vp);
	zfsvfs_t *zfsvfs = zp->z_zfsvfs;
	int error;

	ZFS_ENTER(zfsvfs);
	error = zfs_zaccess_rwx(zp, mode, cr);
	ZFS_EXIT(zfsvfs);
	return (error);
}

/*
 * Lookup an entry in a directory, or an extended attribute directory.
 * If it exists, return a held vnode reference for it.
 *
 *	IN:	dvp	- vnode of directory to search.
 *		nm	- name of entry to lookup.
 *		pnp	- full pathname to lookup [UNUSED].
 *		flags	- LOOKUP_XATTR set if looking for an attribute.
 *		rdir	- root directory vnode [UNUSED].
 *		cr	- credentials of caller.
 *
 *	OUT:	vpp	- vnode of located entry, NULL if not found.
 *
 *	RETURN:	0 if success
 *		error code if failure
 *
 * Timestamps:
 *	NA
 */
/* ARGSUSED */
static int
zfs_lookup(vnode_t *dvp, char *nm, vnode_t **vpp, struct componentname *cnp,
    int nameiop, cred_t *cr, kthread_t *td)
{

	znode_t *zdp = VTOZ(dvp);
	zfsvfs_t *zfsvfs = zdp->z_zfsvfs;
	int	error;

	ZFS_ENTER(zfsvfs);

	*vpp = NULL;

#ifdef TODO
	if (flags & LOOKUP_XATTR) {
		/*
		 * If the xattr property is off, refuse the lookup request.
		 */
		if (!(zfsvfs->z_vfs->vfs_flag & VFS_XATTR)) {
			ZFS_EXIT(zfsvfs);
			return (EINVAL);
		}

		/*
		 * We don't allow recursive attributes..
		 * Maybe someday we will.
		 */
		if (zdp->z_phys->zp_flags & ZFS_XATTR) {
			ZFS_EXIT(zfsvfs);
			return (EINVAL);
		}

		if (error = zfs_get_xattrdir(VTOZ(dvp), vpp, cr, flags)) {
			ZFS_EXIT(zfsvfs);
			return (error);
		}

		/*
		 * Do we have permission to get into attribute directory?
		 */

		if (error = zfs_zaccess(VTOZ(*vpp), ACE_EXECUTE, cr)) {
			VN_RELE(*vpp);
		}

		ZFS_EXIT(zfsvfs);
		return (error);
	}
#endif	/* TODO */

	if (dvp->v_type != VDIR) {
		ZFS_EXIT(zfsvfs);
		return (ENOTDIR);
	}

	/*
	 * Check accessibility of directory.
	 */

	if (error = zfs_zaccess(zdp, ACE_EXECUTE, cr)) {
		ZFS_EXIT(zfsvfs);
		return (error);
	}

	if ((error = zfs_dirlook(zdp, nm, vpp)) == 0) {

		/*
		 * Convert device special files
		 */
		if (IS_DEVVP(*vpp)) {
			vnode_t	*svp;

			svp = specvp(*vpp, (*vpp)->v_rdev, (*vpp)->v_type, cr);
			VN_RELE(*vpp);
			if (svp == NULL)
				error = ENOSYS;
			else
				*vpp = svp;
		}
	}

	ZFS_EXIT(zfsvfs);

	/* Translate errors and add SAVENAME when needed. */
	if (cnp->cn_flags & ISLASTCN) {
		switch (nameiop) {
		case CREATE:
		case RENAME:
			if (error == ENOENT) {
				error = EJUSTRETURN;
				cnp->cn_flags |= SAVENAME;
				break;
			}
			/* FALLTHROUGH */
		case DELETE:
			if (error == 0)
				cnp->cn_flags |= SAVENAME;
			break;
		}
	}
	if (error == 0 && (nm[0] != '.' || nm[1] != '\0')) {
		int ltype = 0;

		if (cnp->cn_flags & ISDOTDOT) {
			ltype = VOP_ISLOCKED(dvp);
			VOP_UNLOCK(dvp, 0);
		}
		error = vn_lock(*vpp, cnp->cn_lkflags);
		if (cnp->cn_flags & ISDOTDOT)
			vn_lock(dvp, ltype | LK_RETRY);
		if (error != 0) {
			VN_RELE(*vpp);
			*vpp = NULL;
			return (error);
		}
	}

#ifdef FREEBSD_NAMECACHE
	/*
	 * Insert name into cache (as non-existent) if appropriate.
	 */
	if (error == ENOENT && (cnp->cn_flags & MAKEENTRY) && nameiop != CREATE)
		cache_enter(dvp, *vpp, cnp);
	/*
	 * Insert name into cache if appropriate.
	 */
	if (error == 0 && (cnp->cn_flags & MAKEENTRY)) {
		if (!(cnp->cn_flags & ISLASTCN) ||
		    (nameiop != DELETE && nameiop != RENAME)) {
			cache_enter(dvp, *vpp, cnp);
		}
	}
#endif

	return (error);
}

/*
 * Attempt to create a new entry in a directory.  If the entry
 * already exists, truncate the file if permissible, else return
 * an error.  Return the vp of the created or trunc'd file.
 *
 *	IN:	dvp	- vnode of directory to put new file entry in.
 *		name	- name of new file entry.
 *		vap	- attributes of new file.
 *		excl	- flag indicating exclusive or non-exclusive mode.
 *		mode	- mode to open file with.
 *		cr	- credentials of caller.
 *		flag	- large file flag [UNUSED].
 *
 *	OUT:	vpp	- vnode of created or trunc'd entry.
 *
 *	RETURN:	0 if success
 *		error code if failure
 *
 * Timestamps:
 *	dvp - ctime|mtime updated if new entry created
 *	 vp - ctime|mtime always, atime if new
 */
/* ARGSUSED */
static int
zfs_create(vnode_t *dvp, char *name, vattr_t *vap, int excl, int mode,
    vnode_t **vpp, cred_t *cr)
{
	znode_t		*zp, *dzp = VTOZ(dvp);
	zfsvfs_t	*zfsvfs = dzp->z_zfsvfs;
	zilog_t		*zilog = zfsvfs->z_log;
	objset_t	*os = zfsvfs->z_os;
	zfs_dirlock_t	*dl;
	dmu_tx_t	*tx;
	int		error;
	uint64_t	zoid;

	ZFS_ENTER(zfsvfs);

top:
	*vpp = NULL;

	if ((vap->va_mode & S_ISVTX) && secpolicy_vnode_stky_modify(cr))
		vap->va_mode &= ~S_ISVTX;

	if (*name == '\0') {
		/*
		 * Null component name refers to the directory itself.
		 */
		VN_HOLD(dvp);
		zp = dzp;
		dl = NULL;
		error = 0;
	} else {
		/* possible VN_HOLD(zp) */
		if (error = zfs_dirent_lock(&dl, dzp, name, &zp, 0)) {
			if (strcmp(name, "..") == 0)
				error = EISDIR;
			ZFS_EXIT(zfsvfs);
			return (error);
		}
	}

	zoid = zp ? zp->z_id : -1ULL;

	if (zp == NULL) {
		/*
		 * Create a new file object and update the directory
		 * to reference it.
		 */
		if (error = zfs_zaccess(dzp, ACE_ADD_FILE, cr)) {
			goto out;
		}

		/*
		 * We only support the creation of regular files in
		 * extended attribute directories.
		 */
		if ((dzp->z_phys->zp_flags & ZFS_XATTR) &&
		    (vap->va_type != VREG)) {
			error = EINVAL;
			goto out;
		}

		tx = dmu_tx_create(os);
		dmu_tx_hold_bonus(tx, DMU_NEW_OBJECT);
		dmu_tx_hold_bonus(tx, dzp->z_id);
		dmu_tx_hold_zap(tx, dzp->z_id, TRUE, name);
		if (dzp->z_phys->zp_flags & ZFS_INHERIT_ACE)
			dmu_tx_hold_write(tx, DMU_NEW_OBJECT,
			    0, SPA_MAXBLOCKSIZE);
		error = dmu_tx_assign(tx, zfsvfs->z_assign);
		if (error) {
			zfs_dirent_unlock(dl);
			if (error == ERESTART &&
			    zfsvfs->z_assign == TXG_NOWAIT) {
				dmu_tx_wait(tx);
				dmu_tx_abort(tx);
				goto top;
			}
			dmu_tx_abort(tx);
			ZFS_EXIT(zfsvfs);
			return (error);
		}
		zfs_mknode(dzp, vap, &zoid, tx, cr, 0, &zp, 0);
		ASSERT(zp->z_id == zoid);
		(void) zfs_link_create(dl, zp, tx, ZNEW);
		zfs_log_create(zilog, tx, TX_CREATE, dzp, zp, name);
		dmu_tx_commit(tx);
	} else {
		/*
		 * A directory entry already exists for this name.
		 */
		/*
		 * Can't truncate an existing file if in exclusive mode.
		 */
		if (excl == EXCL) {
			error = EEXIST;
			goto out;
		}
		/*
		 * Can't open a directory for writing.
		 */
		if ((ZTOV(zp)->v_type == VDIR) && (mode & S_IWRITE)) {
			error = EISDIR;
			goto out;
		}
		/*
		 * Verify requested access to file.
		 */
		if (mode && (error = zfs_zaccess_rwx(zp, mode, cr))) {
			goto out;
		}

		mutex_enter(&dzp->z_lock);
		dzp->z_seq++;
		mutex_exit(&dzp->z_lock);

		/*
		 * Truncate regular files if requested.
		 */
		if ((ZTOV(zp)->v_type == VREG) &&
		    (vap->va_mask & AT_SIZE) && (vap->va_size == 0)) {
			error = zfs_freesp(zp, 0, 0, mode, TRUE);
			if (error == ERESTART &&
			    zfsvfs->z_assign == TXG_NOWAIT) {
				/* NB: we already did dmu_tx_wait() */
				zfs_dirent_unlock(dl);
				VN_RELE(ZTOV(zp));
				goto top;
			}
		}
	}
out:
	if (dl)
		zfs_dirent_unlock(dl);

	if (error) {
		if (zp)
			VN_RELE(ZTOV(zp));
	} else {
		*vpp = ZTOV(zp);
		/*
		 * If vnode is for a device return a specfs vnode instead.
		 */
		if (IS_DEVVP(*vpp)) {
			struct vnode *svp;

			svp = specvp(*vpp, (*vpp)->v_rdev, (*vpp)->v_type, cr);
			VN_RELE(*vpp);
			if (svp == NULL) {
				error = ENOSYS;
			}
			*vpp = svp;
		}
	}

	ZFS_EXIT(zfsvfs);
	return (error);
}

/*
 * Remove an entry from a directory.
 *
 *	IN:	dvp	- vnode of directory to remove entry from.
 *		name	- name of entry to remove.
 *		cr	- credentials of caller.
 *
 *	RETURN:	0 if success
 *		error code if failure
 *
 * Timestamps:
 *	dvp - ctime|mtime
 *	 vp - ctime (if nlink > 0)
 */
static int
zfs_remove(vnode_t *dvp, char *name, cred_t *cr)
{
	znode_t		*zp, *dzp = VTOZ(dvp);
	znode_t		*xzp = NULL;
	vnode_t		*vp;
	zfsvfs_t	*zfsvfs = dzp->z_zfsvfs;
	zilog_t		*zilog = zfsvfs->z_log;
	uint64_t	acl_obj, xattr_obj;
	zfs_dirlock_t	*dl;
	dmu_tx_t	*tx;
	boolean_t	may_delete_now, delete_now = FALSE;
	boolean_t	unlinked;
	int		error;

	ZFS_ENTER(zfsvfs);

top:
	/*
	 * Attempt to lock directory; fail if entry doesn't exist.
	 */
	if (error = zfs_dirent_lock(&dl, dzp, name, &zp, ZEXISTS)) {
		ZFS_EXIT(zfsvfs);
		return (error);
	}

	vp = ZTOV(zp);

	if (error = zfs_zaccess_delete(dzp, zp, cr)) {
		goto out;
	}

	/*
	 * Need to use rmdir for removing directories.
	 */
	if (vp->v_type == VDIR) {
		error = EPERM;
		goto out;
	}

	vnevent_remove(vp);

	dnlc_remove(dvp, name);

	may_delete_now = FALSE;

	/*
	 * We may delete the znode now, or we may put it in the unlinked set;
	 * it depends on whether we're the last link, and on whether there are
	 * other holds on the vnode.  So we dmu_tx_hold() the right things to
	 * allow for either case.
	 */
	tx = dmu_tx_create(zfsvfs->z_os);
	dmu_tx_hold_zap(tx, dzp->z_id, FALSE, name);
	dmu_tx_hold_bonus(tx, zp->z_id);
	if (may_delete_now)
		dmu_tx_hold_free(tx, zp->z_id, 0, DMU_OBJECT_END);

	/* are there any extended attributes? */
	if ((xattr_obj = zp->z_phys->zp_xattr) != 0) {
		/* XXX - do we need this if we are deleting? */
		dmu_tx_hold_bonus(tx, xattr_obj);
	}

	/* are there any additional acls */
	if ((acl_obj = zp->z_phys->zp_acl.z_acl_extern_obj) != 0 &&
	    may_delete_now)
		dmu_tx_hold_free(tx, acl_obj, 0, DMU_OBJECT_END);

	/* charge as an update -- would be nice not to charge at all */
	dmu_tx_hold_zap(tx, zfsvfs->z_unlinkedobj, FALSE, NULL);

	error = dmu_tx_assign(tx, zfsvfs->z_assign);
	if (error) {
		zfs_dirent_unlock(dl);
		VN_RELE(vp);
		if (error == ERESTART && zfsvfs->z_assign == TXG_NOWAIT) {
			dmu_tx_wait(tx);
			dmu_tx_abort(tx);
			goto top;
		}
		dmu_tx_abort(tx);
		ZFS_EXIT(zfsvfs);
		return (error);
	}

	/*
	 * Remove the directory entry.
	 */
	error = zfs_link_destroy(dl, zp, tx, 0, &unlinked);

	if (error) {
		dmu_tx_commit(tx);
		goto out;
	}

	if (0 && unlinked) {
		VI_LOCK(vp);
		delete_now = may_delete_now &&
		    vp->v_count == 1 && !vn_has_cached_data(vp) &&
		    zp->z_phys->zp_xattr == xattr_obj &&
		    zp->z_phys->zp_acl.z_acl_extern_obj == acl_obj;
		VI_UNLOCK(vp);
	}

	if (delete_now) {
		if (zp->z_phys->zp_xattr) {
			error = zfs_zget(zfsvfs, zp->z_phys->zp_xattr, &xzp);
			ASSERT3U(error, ==, 0);
			ASSERT3U(xzp->z_phys->zp_links, ==, 2);
			dmu_buf_will_dirty(xzp->z_dbuf, tx);
			mutex_enter(&xzp->z_lock);
			xzp->z_unlinked = 1;
			xzp->z_phys->zp_links = 0;
			mutex_exit(&xzp->z_lock);
			zfs_unlinked_add(xzp, tx);
			zp->z_phys->zp_xattr = 0; /* probably unnecessary */
		}
		mutex_enter(&zp->z_lock);
		VI_LOCK(vp);
		vp->v_count--;
		ASSERT3U(vp->v_count, ==, 0);
		VI_UNLOCK(vp);
		mutex_exit(&zp->z_lock);
		zfs_znode_delete(zp, tx);
		VFS_RELE(zfsvfs->z_vfs);
	} else if (unlinked) {
		zfs_unlinked_add(zp, tx);
	}

	zfs_log_remove(zilog, tx, TX_REMOVE, dzp, name);

	dmu_tx_commit(tx);
out:
	zfs_dirent_unlock(dl);

	if (!delete_now) {
		VN_RELE(vp);
	} else if (xzp) {
		/* this rele delayed to prevent nesting transactions */
		VN_RELE(ZTOV(xzp));
	}

	ZFS_EXIT(zfsvfs);
	return (error);
}

/*
 * Create a new directory and insert it into dvp using the name
 * provided.  Return a pointer to the inserted directory.
 *
 *	IN:	dvp	- vnode of directory to add subdir to.
 *		dirname	- name of new directory.
 *		vap	- attributes of new directory.
 *		cr	- credentials of caller.
 *
 *	OUT:	vpp	- vnode of created directory.
 *
 *	RETURN:	0 if success
 *		error code if failure
 *
 * Timestamps:
 *	dvp - ctime|mtime updated
 *	 vp - ctime|mtime|atime updated
 */
static int
zfs_mkdir(vnode_t *dvp, char *dirname, vattr_t *vap, vnode_t **vpp, cred_t *cr)
{
	znode_t		*zp, *dzp = VTOZ(dvp);
	zfsvfs_t	*zfsvfs = dzp->z_zfsvfs;
	zilog_t		*zilog = zfsvfs->z_log;
	zfs_dirlock_t	*dl;
	uint64_t	zoid = 0;
	dmu_tx_t	*tx;
	int		error;

	ASSERT(vap->va_type == VDIR);

	ZFS_ENTER(zfsvfs);

	if (dzp->z_phys->zp_flags & ZFS_XATTR) {
		ZFS_EXIT(zfsvfs);
		return (EINVAL);
	}
top:
	*vpp = NULL;

	/*
	 * First make sure the new directory doesn't exist.
	 */
	if (error = zfs_dirent_lock(&dl, dzp, dirname, &zp, ZNEW)) {
		ZFS_EXIT(zfsvfs);
		return (error);
	}

	if (error = zfs_zaccess(dzp, ACE_ADD_SUBDIRECTORY, cr)) {
		zfs_dirent_unlock(dl);
		ZFS_EXIT(zfsvfs);
		return (error);
	}

	/*
	 * Add a new entry to the directory.
	 */
	tx = dmu_tx_create(zfsvfs->z_os);
	dmu_tx_hold_zap(tx, dzp->z_id, TRUE, dirname);
	dmu_tx_hold_zap(tx, DMU_NEW_OBJECT, FALSE, NULL);
	if (dzp->z_phys->zp_flags & ZFS_INHERIT_ACE)
		dmu_tx_hold_write(tx, DMU_NEW_OBJECT,
		    0, SPA_MAXBLOCKSIZE);
	error = dmu_tx_assign(tx, zfsvfs->z_assign);
	if (error) {
		zfs_dirent_unlock(dl);
		if (error == ERESTART && zfsvfs->z_assign == TXG_NOWAIT) {
			dmu_tx_wait(tx);
			dmu_tx_abort(tx);
			goto top;
		}
		dmu_tx_abort(tx);
		ZFS_EXIT(zfsvfs);
		return (error);
	}

	/*
	 * Create new node.
	 */
	zfs_mknode(dzp, vap, &zoid, tx, cr, 0, &zp, 0);

	/*
	 * Now put new name in parent dir.
	 */
	(void) zfs_link_create(dl, zp, tx, ZNEW);

	*vpp = ZTOV(zp);

	zfs_log_create(zilog, tx, TX_MKDIR, dzp, zp, dirname);
	dmu_tx_commit(tx);

	zfs_dirent_unlock(dl);

	ZFS_EXIT(zfsvfs);
	return (0);
}

/*
 * Remove a directory subdir entry.  If the current working
 * directory is the same as the subdir to be removed, the
 * remove will fail.
 *
 *	IN:	dvp	- vnode of directory to remove from.
 *		name	- name of directory to be removed.
 *		cwd	- vnode of current working directory.
 *		cr	- credentials of caller.
 *
 *	RETURN:	0 if success
 *		error code if failure
 *
 * Timestamps:
 *	dvp - ctime|mtime updated
 */
static int
zfs_rmdir(vnode_t *dvp, char *name, vnode_t *cwd, cred_t *cr)
{
	znode_t		*dzp = VTOZ(dvp);
	znode_t		*zp;
	vnode_t		*vp;
	zfsvfs_t	*zfsvfs = dzp->z_zfsvfs;
	zilog_t		*zilog = zfsvfs->z_log;
	zfs_dirlock_t	*dl;
	dmu_tx_t	*tx;
	int		error;

	ZFS_ENTER(zfsvfs);

top:
	zp = NULL;

	/*
	 * Attempt to lock directory; fail if entry doesn't exist.
	 */
	if (error = zfs_dirent_lock(&dl, dzp, name, &zp, ZEXISTS)) {
		ZFS_EXIT(zfsvfs);
		return (error);
	}

	vp = ZTOV(zp);

	if (error = zfs_zaccess_delete(dzp, zp, cr)) {
		goto out;
	}

	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}

	if (vp == cwd) {
		error = EINVAL;
		goto out;
	}

	vnevent_rmdir(vp);

	/*
	 * Grab a lock on the directory to make sure that noone is
	 * trying to add (or lookup) entries while we are removing it.
	 */
	rw_enter(&zp->z_name_lock, RW_WRITER);

	/*
	 * Grab a lock on the parent pointer to make sure we play well
	 * with the treewalk and directory rename code.
	 */
	rw_enter(&zp->z_parent_lock, RW_WRITER);

	tx = dmu_tx_create(zfsvfs->z_os);
	dmu_tx_hold_zap(tx, dzp->z_id, FALSE, name);
	dmu_tx_hold_bonus(tx, zp->z_id);
	dmu_tx_hold_zap(tx, zfsvfs->z_unlinkedobj, FALSE, NULL);
	error = dmu_tx_assign(tx, zfsvfs->z_assign);
	if (error) {
		rw_exit(&zp->z_parent_lock);
		rw_exit(&zp->z_name_lock);
		zfs_dirent_unlock(dl);
		VN_RELE(vp);
		if (error == ERESTART && zfsvfs->z_assign == TXG_NOWAIT) {
			dmu_tx_wait(tx);
			dmu_tx_abort(tx);
			goto top;
		}
		dmu_tx_abort(tx);
		ZFS_EXIT(zfsvfs);
		return (error);
	}

#ifdef FREEBSD_NAMECACHE
	cache_purge(dvp);
#endif

	error = zfs_link_destroy(dl, zp, tx, 0, NULL);

	if (error == 0)
		zfs_log_remove(zilog, tx, TX_RMDIR, dzp, name);

	dmu_tx_commit(tx);

	rw_exit(&zp->z_parent_lock);
	rw_exit(&zp->z_name_lock);
#ifdef FREEBSD_NAMECACHE
	cache_purge(vp);
#endif
out:
	zfs_dirent_unlock(dl);

	VN_RELE(vp);

	ZFS_EXIT(zfsvfs);
	return (error);
}

/*
 * Read as many directory entries as will fit into the provided
 * buffer from the given directory cursor position (specified in
 * the uio structure.
 *
 *	IN:	vp	- vnode of directory to read.
 *		uio	- structure supplying read location, range info,
 *			  and return buffer.
 *		cr	- credentials of caller.
 *
 *	OUT:	uio	- updated offset and range, buffer filled.
 *		eofp	- set to true if end-of-file detected.
 *
 *	RETURN:	0 if success
 *		error code if failure
 *
 * Timestamps:
 *	vp - atime updated
 *
 * Note that the low 4 bits of the cookie returned by zap is always zero.
 * This allows us to use the low range for "special" directory entries:
 * We use 0 for '.', and 1 for '..'.  If this is the root of the filesystem,
 * we use the offset 2 for the '.zfs' directory.
 */
/* ARGSUSED */
static int
zfs_readdir(vnode_t *vp, uio_t *uio, cred_t *cr, int *eofp, int *ncookies, u_long **cookies)
{
	znode_t		*zp = VTOZ(vp);
	iovec_t		*iovp;
	dirent64_t	*odp;
	zfsvfs_t	*zfsvfs = zp->z_zfsvfs;
	objset_t	*os;
	caddr_t		outbuf;
	size_t		bufsize;
	zap_cursor_t	zc;
	zap_attribute_t	zap;
	uint_t		bytes_wanted;
	uint64_t	offset; /* must be unsigned; checks for < 1 */
	int		local_eof;
	int		outcount;
	int		error;
	uint8_t		prefetch;
	uint8_t		type;
	int		ncooks;
	u_long		*cooks = NULL;

	ZFS_ENTER(zfsvfs);

	/*
	 * If we are not given an eof variable,
	 * use a local one.
	 */
	if (eofp == NULL)
		eofp = &local_eof;

	/*
	 * Check for valid iov_len.
	 */
	if (uio->uio_iov->iov_len <= 0) {
		ZFS_EXIT(zfsvfs);
		return (EINVAL);
	}

	/*
	 * Quit if directory has been removed (posix)
	 */
	if ((*eofp = zp->z_unlinked) != 0) {
		ZFS_EXIT(zfsvfs);
		return (0);
	}

	error = 0;
	os = zfsvfs->z_os;
	offset = uio->uio_loffset;
	prefetch = zp->z_zn_prefetch;

	/*
	 * Initialize the iterator cursor.
	 */
	if (offset <= 3) {
		/*
		 * Start iteration from the beginning of the directory.
		 */
		zap_cursor_init(&zc, os, zp->z_id);
	} else {
		/*
		 * The offset is a serialized cursor.
		 */
		zap_cursor_init_serialized(&zc, os, zp->z_id, offset);
	}

	/*
	 * Get space to change directory entries into fs independent format.
	 */
	iovp = uio->uio_iov;
	bytes_wanted = iovp->iov_len;
	if (uio->uio_segflg != UIO_SYSSPACE || uio->uio_iovcnt != 1) {
		bufsize = bytes_wanted;
		outbuf = kmem_alloc(bufsize, KM_SLEEP);
		odp = (struct dirent64 *)outbuf;
	} else {
		bufsize = bytes_wanted;
		odp = (struct dirent64 *)iovp->iov_base;
	}

	if (ncookies != NULL) {
		/*
		 * Minimum entry size is dirent size and 1 byte for a file name.
		 */
		ncooks = uio->uio_resid / (sizeof(struct dirent) - sizeof(((struct dirent *)NULL)->d_name) + 1);
		cooks = malloc(ncooks * sizeof(u_long), M_TEMP, M_WAITOK);
		*cookies = cooks;
		*ncookies = ncooks;
	}

	/*
	 * Transform to file-system independent format
	 */
	outcount = 0;
	while (outcount < bytes_wanted) {
		ino64_t objnum;
		ushort_t reclen;

		/*
		 * Special case `.', `..', and `.zfs'.
		 */
		if (offset == 0) {
			(void) strcpy(zap.za_name, ".");
			objnum = zp->z_id;
			type = DT_DIR;
		} else if (offset == 1) {
			(void) strcpy(zap.za_name, "..");
			objnum = zp->z_phys->zp_parent;
			type = DT_DIR;
		} else if (offset == 2 && zfs_show_ctldir(zp)) {
			(void) strcpy(zap.za_name, ZFS_CTLDIR_NAME);
			objnum = ZFSCTL_INO_ROOT;
			type = DT_DIR;
		} else {
			/*
			 * Grab next entry.
			 */
			if (error = zap_cursor_retrieve(&zc, &zap)) {
				if ((*eofp = (error == ENOENT)) != 0)
					break;
				else
					goto update;
			}

			if (zap.za_integer_length != 8 ||
			    zap.za_num_integers != 1) {
				cmn_err(CE_WARN, "zap_readdir: bad directory "
				    "entry, obj = %lld, offset = %lld\n",
				    (u_longlong_t)zp->z_id,
				    (u_longlong_t)offset);
				error = ENXIO;
				goto update;
			}

			objnum = ZFS_DIRENT_OBJ(zap.za_first_integer);
			/*
			 * MacOS X can extract the object type here such as:
			 * uint8_t type = ZFS_DIRENT_TYPE(zap.za_first_integer);
			 */
			type = ZFS_DIRENT_TYPE(zap.za_first_integer);
		}
		reclen = DIRENT64_RECLEN(strlen(zap.za_name));

		/*
		 * Will this entry fit in the buffer?
		 */
		if (outcount + reclen > bufsize) {
			/*
			 * Did we manage to fit anything in the buffer?
			 */
			if (!outcount) {
				error = EINVAL;
				goto update;
			}
			break;
		}
		/*
		 * Add this entry:
		 */
		odp->d_ino = objnum;
		odp->d_reclen = reclen;
		odp->d_namlen = strlen(zap.za_name);
		(void) strlcpy(odp->d_name, zap.za_name, odp->d_namlen + 1);
		odp->d_type = type;
		outcount += reclen;
		odp = (dirent64_t *)((intptr_t)odp + reclen);

		ASSERT(outcount <= bufsize);

		/* Prefetch znode */
		if (prefetch)
			dmu_prefetch(os, objnum, 0, 0);

		/*
		 * Move to the next entry, fill in the previous offset.
		 */
		if (offset > 2 || (offset == 2 && !zfs_show_ctldir(zp))) {
			zap_cursor_advance(&zc);
			offset = zap_cursor_serialize(&zc);
		} else {
			offset += 1;
		}

		if (cooks != NULL) {
			*cooks++ = offset;
			ncooks--;
			KASSERT(ncooks >= 0, ("ncookies=%d", ncooks));
		}
	}
	zp->z_zn_prefetch = B_FALSE; /* a lookup will re-enable pre-fetching */

	/* Subtract unused cookies */
	if (ncookies != NULL)
		*ncookies -= ncooks;

	if (uio->uio_segflg == UIO_SYSSPACE && uio->uio_iovcnt == 1) {
		iovp->iov_base += outcount;
		iovp->iov_len -= outcount;
		uio->uio_resid -= outcount;
	} else if (error = uiomove(outbuf, (long)outcount, UIO_READ, uio)) {
		/*
		 * Reset the pointer.
		 */
		offset = uio->uio_loffset;
	}

update:
	zap_cursor_fini(&zc);
	if (uio->uio_segflg != UIO_SYSSPACE || uio->uio_iovcnt != 1)
		kmem_free(outbuf, bufsize);

	if (error == ENOENT)
		error = 0;

	ZFS_ACCESSTIME_STAMP(zfsvfs, zp);

	uio->uio_loffset = offset;
	ZFS_EXIT(zfsvfs);
	if (error != 0 && cookies != NULL) {
		free(*cookies, M_TEMP);
		*cookies = NULL;
		*ncookies = 0;
	}
	return (error);
}

static int
zfs_fsync(vnode_t *vp, int syncflag, cred_t *cr)
{
	znode_t	*zp = VTOZ(vp);
	zfsvfs_t *zfsvfs = zp->z_zfsvfs;

	ZFS_ENTER(zfsvfs);
	zil_commit(zfsvfs->z_log, zp->z_last_itx, zp->z_id);
	ZFS_EXIT(zfsvfs);
	return (0);
}

/*
 * Get the requested file attributes and place them in the provided
 * vattr structure.
 *
 *	IN:	vp	- vnode of file.
 *		vap	- va_mask identifies requested attributes.
 *		flags	- [UNUSED]
 *		cr	- credentials of caller.
 *
 *	OUT:	vap	- attribute values.
 *
 *	RETURN:	0 (always succeeds)
 */
/* ARGSUSED */
static int
zfs_getattr(vnode_t *vp, vattr_t *vap, int flags, cred_t *cr)
{
	znode_t *zp = VTOZ(vp);
	zfsvfs_t *zfsvfs = zp->z_zfsvfs;
	znode_phys_t *pzp = zp->z_phys;
	uint32_t blksize;
	u_longlong_t nblocks;
	int	error;

	ZFS_ENTER(zfsvfs);

	/*
	 * Return all attributes.  It's cheaper to provide the answer
	 * than to determine whether we were asked the question.
	 */
	mutex_enter(&zp->z_lock);

	vap->va_type = IFTOVT(pzp->zp_mode);
	vap->va_mode = pzp->zp_mode & ~S_IFMT;
	vap->va_uid = zp->z_phys->zp_uid;
	vap->va_gid = zp->z_phys->zp_gid;
	vap->va_nodeid = zp->z_id;
	vap->va_nlink = MIN(pzp->zp_links, UINT32_MAX);	/* nlink_t limit! */
	vap->va_size = pzp->zp_size;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_rdev = zfs_cmpldev(pzp->zp_rdev);
	vap->va_seq = zp->z_seq;
	vap->va_flags = 0;	/* FreeBSD: Reset chflags(2) flags. */

	ZFS_TIME_DECODE(&vap->va_atime, pzp->zp_atime);
	ZFS_TIME_DECODE(&vap->va_mtime, pzp->zp_mtime);
	ZFS_TIME_DECODE(&vap->va_ctime, pzp->zp_ctime);
	ZFS_TIME_DECODE(&vap->va_birthtime, pzp->zp_crtime);

	/*
	 * If ACL is trivial don't bother looking for ACE_READ_ATTRIBUTES.
	 * Also, if we are the owner don't bother, since owner should
	 * always be allowed to read basic attributes of file.
	 */
	if (!(zp->z_phys->zp_flags & ZFS_ACL_TRIVIAL) &&
	    (zp->z_phys->zp_uid != crgetuid(cr))) {
		if (error = zfs_zaccess(zp, ACE_READ_ATTRIBUTES, cr)) {
			mutex_exit(&zp->z_lock);
			ZFS_EXIT(zfsvfs);
			return (error);
		}
	}

	mutex_exit(&zp->z_lock);

	dmu_object_size_from_db(zp->z_dbuf, &blksize, &nblocks);
	vap->va_blksize = blksize;
	vap->va_bytes = nblocks << 9;	/* nblocks * 512 */

	if (zp->z_blksz == 0) {
		/*
		 * Block size hasn't been set; suggest maximal I/O transfers.
		 */
		vap->va_blksize = zfsvfs->z_max_blksz;
	}

	ZFS_EXIT(zfsvfs);
	return (0);
}

/*
 * Set the file attributes to the values contained in the
 * vattr structure.
 *
 *	IN:	vp	- vnode of file to be modified.
 *		vap	- new attribute values.
 *		flags	- ATTR_UTIME set if non-default time values provided.
 *		cr	- credentials of caller.
 *
 *	RETURN:	0 if success
 *		error code if failure
 *
 * Timestamps:
 *	vp - ctime updated, mtime updated if size changed.
 */
/* ARGSUSED */
static int
zfs_setattr(vnode_t *vp, vattr_t *vap, int flags, cred_t *cr,
	caller_context_t *ct)
{
	struct znode	*zp = VTOZ(vp);
	znode_phys_t	*pzp = zp->z_phys;
	zfsvfs_t	*zfsvfs = zp->z_zfsvfs;
	zilog_t		*zilog = zfsvfs->z_log;
	dmu_tx_t	*tx;
	vattr_t		oldva;
	uint_t		mask = vap->va_mask;
	uint_t		saved_mask;
	int		trim_mask = 0;
	uint64_t	new_mode;
	znode_t		*attrzp;
	int		need_policy = FALSE;
	int		err;

	if (mask == 0)
		return (0);

	if (mask & AT_NOSET)
		return (EINVAL);

	if (mask & AT_SIZE && vp->v_type == VDIR)
		return (EISDIR);

	if (mask & AT_SIZE && vp->v_type != VREG && vp->v_type != VFIFO)
		return (EINVAL);

	ZFS_ENTER(zfsvfs);

top:
	attrzp = NULL;

	if (zfsvfs->z_vfs->vfs_flag & VFS_RDONLY) {
		ZFS_EXIT(zfsvfs);
		return (EROFS);
	}

	/*
	 * First validate permissions
	 */

	if (mask & AT_SIZE) {
		err = zfs_zaccess(zp, ACE_WRITE_DATA, cr);
		if (err) {
			ZFS_EXIT(zfsvfs);
			return (err);
		}
		/*
		 * XXX - Note, we are not providing any open
		 * mode flags here (like FNDELAY), so we may
		 * block if there are locks present... this
		 * should be addressed in openat().
		 */
		do {
			err = zfs_freesp(zp, vap->va_size, 0, 0, FALSE);
			/* NB: we already did dmu_tx_wait() if necessary */
		} while (err == ERESTART && zfsvfs->z_assign == TXG_NOWAIT);
		if (err) {
			ZFS_EXIT(zfsvfs);
			return (err);
		}
	}

	if (mask & (AT_ATIME|AT_MTIME))
		need_policy = zfs_zaccess_v4_perm(zp, ACE_WRITE_ATTRIBUTES, cr);

	if (mask & (AT_UID|AT_GID)) {
		int	idmask = (mask & (AT_UID|AT_GID));
		int	take_owner;
		int	take_group;

		/*
		 * NOTE: even if a new mode is being set,
		 * we may clear S_ISUID/S_ISGID bits.
		 */

		if (!(mask & AT_MODE))
			vap->va_mode = pzp->zp_mode;

		/*
		 * Take ownership or chgrp to group we are a member of
		 */

		take_owner = (mask & AT_UID) && (vap->va_uid == crgetuid(cr));
		take_group = (mask & AT_GID) && groupmember(vap->va_gid, cr);

		/*
		 * If both AT_UID and AT_GID are set then take_owner and
		 * take_group must both be set in order to allow taking
		 * ownership.
		 *
		 * Otherwise, send the check through secpolicy_vnode_setattr()
		 *
		 */

		if (((idmask == (AT_UID|AT_GID)) && take_owner && take_group) ||
		    ((idmask == AT_UID) && take_owner) ||
		    ((idmask == AT_GID) && take_group)) {
			if (zfs_zaccess_v4_perm(zp, ACE_WRITE_OWNER, cr) == 0) {
				/*
				 * Remove setuid/setgid for non-privileged users
				 */
				secpolicy_setid_clear(vap, cr);
				trim_mask = (mask & (AT_UID|AT_GID));
			} else {
				need_policy =  TRUE;
			}
		} else {
			need_policy =  TRUE;
		}
	}

	mutex_enter(&zp->z_lock);
	oldva.va_mode = pzp->zp_mode;
	oldva.va_uid = zp->z_phys->zp_uid;
	oldva.va_gid = zp->z_phys->zp_gid;
	mutex_exit(&zp->z_lock);

	if (mask & AT_MODE) {
		if (zfs_zaccess_v4_perm(zp, ACE_WRITE_ACL, cr) == 0) {
			err = secpolicy_setid_setsticky_clear(vp, vap,
			    &oldva, cr);
			if (err) {
				ZFS_EXIT(zfsvfs);
				return (err);
			}
			trim_mask |= AT_MODE;
		} else {
			need_policy = TRUE;
		}
	}

	if (need_policy) {
		/*
		 * If trim_mask is set then take ownership
		 * has been granted or write_acl is present and user
		 * has the ability to modify mode.  In that case remove
		 * UID|GID and or MODE from mask so that
		 * secpolicy_vnode_setattr() doesn't revoke it.
		 */

		if (trim_mask) {
			saved_mask = vap->va_mask;
			vap->va_mask &= ~trim_mask;

		}
		err = secpolicy_vnode_setattr(cr, vp, vap, &oldva, flags,
		    (int (*)(void *, int, cred_t *))zfs_zaccess_rwx, zp);
		if (err) {
			ZFS_EXIT(zfsvfs);
			return (err);
		}

		if (trim_mask)
			vap->va_mask |= saved_mask;
	}

	/*
	 * secpolicy_vnode_setattr, or take ownership may have
	 * changed va_mask
	 */
	mask = vap->va_mask;

	tx = dmu_tx_create(zfsvfs->z_os);
	dmu_tx_hold_bonus(tx, zp->z_id);

	if (mask & AT_MODE) {
		uint64_t pmode = pzp->zp_mode;

		new_mode = (pmode & S_IFMT) | (vap->va_mode & ~S_IFMT);

		if (zp->z_phys->zp_acl.z_acl_extern_obj)
			dmu_tx_hold_write(tx,
			    pzp->zp_acl.z_acl_extern_obj, 0, SPA_MAXBLOCKSIZE);
		else
			dmu_tx_hold_write(tx, DMU_NEW_OBJECT,
			    0, ZFS_ACL_SIZE(MAX_ACL_SIZE));
	}

	if ((mask & (AT_UID | AT_GID)) && zp->z_phys->zp_xattr != 0) {
		err = zfs_zget(zp->z_zfsvfs, zp->z_phys->zp_xattr, &attrzp);
		if (err) {
			dmu_tx_abort(tx);
			ZFS_EXIT(zfsvfs);
			return (err);
		}
		dmu_tx_hold_bonus(tx, attrzp->z_id);
	}

	err = dmu_tx_assign(tx, zfsvfs->z_assign);
	if (err) {
		if (attrzp)
			VN_RELE(ZTOV(attrzp));
		if (err == ERESTART && zfsvfs->z_assign == TXG_NOWAIT) {
			dmu_tx_wait(tx);
			dmu_tx_abort(tx);
			goto top;
		}
		dmu_tx_abort(tx);
		ZFS_EXIT(zfsvfs);
		return (err);
	}

	dmu_buf_will_dirty(zp->z_dbuf, tx);

	/*
	 * Set each attribute requested.
	 * We group settings according to the locks they need to acquire.
	 *
	 * Note: you cannot set ctime directly, although it will be
	 * updated as a side-effect of calling this function.
	 */

	mutex_enter(&zp->z_lock);

	if (mask & AT_MODE) {
		err = zfs_acl_chmod_setattr(zp, new_mode, tx);
		ASSERT3U(err, ==, 0);
	}

	if (attrzp)
		mutex_enter(&attrzp->z_lock);

	if (mask & AT_UID) {
		zp->z_phys->zp_uid = (uint64_t)vap->va_uid;
		if (attrzp) {
			attrzp->z_phys->zp_uid = (uint64_t)vap->va_uid;
		}
	}

	if (mask & AT_GID) {
		zp->z_phys->zp_gid = (uint64_t)vap->va_gid;
		if (attrzp)
			attrzp->z_phys->zp_gid = (uint64_t)vap->va_gid;
	}

	if (attrzp)
		mutex_exit(&attrzp->z_lock);

	if (mask & AT_ATIME)
		ZFS_TIME_ENCODE(&vap->va_atime, pzp->zp_atime);

	if (mask & AT_MTIME)
		ZFS_TIME_ENCODE(&vap->va_mtime, pzp->zp_mtime);

	if (mask & AT_SIZE)
		zfs_time_stamper_locked(zp, CONTENT_MODIFIED, tx);
	else if (mask != 0)
		zfs_time_stamper_locked(zp, STATE_CHANGED, tx);

	if (mask != 0)
		zfs_log_setattr(zilog, tx, TX_SETATTR, zp, vap, mask);

	mutex_exit(&zp->z_lock);

	if (attrzp)
		VN_RELE(ZTOV(attrzp));

	dmu_tx_commit(tx);

	ZFS_EXIT(zfsvfs);
	return (err);
}

typedef struct zfs_zlock {
	krwlock_t	*zl_rwlock;	/* lock we acquired */
	znode_t		*zl_znode;	/* znode we held */
	struct zfs_zlock *zl_next;	/* next in list */
} zfs_zlock_t;

/*
 * Drop locks and release vnodes that were held by zfs_rename_lock().
 */
static void
zfs_rename_unlock(zfs_zlock_t **zlpp)
{
	zfs_zlock_t *zl;

	while ((zl = *zlpp) != NULL) {
		if (zl->zl_znode != NULL)
			VN_RELE(ZTOV(zl->zl_znode));
		rw_exit(zl->zl_rwlock);
		*zlpp = zl->zl_next;
		kmem_free(zl, sizeof (*zl));
	}
}

/*
 * Search back through the directory tree, using the ".." entries.
 * Lock each directory in the chain to prevent concurrent renames.
 * Fail any attempt to move a directory into one of its own descendants.
 * XXX - z_parent_lock can overlap with map or grow locks
 */
static int
zfs_rename_lock(znode_t *szp, znode_t *tdzp, znode_t *sdzp, zfs_zlock_t **zlpp)
{
	zfs_zlock_t	*zl;
	znode_t		*zp = tdzp;
	uint64_t	rootid = zp->z_zfsvfs->z_root;
	uint64_t	*oidp = &zp->z_id;
	krwlock_t	*rwlp = &szp->z_parent_lock;
	krw_t		rw = RW_WRITER;

	/*
	 * First pass write-locks szp and compares to zp->z_id.
	 * Later passes read-lock zp and compare to zp->z_parent.
	 */
	do {
		if (!rw_tryenter(rwlp, rw)) {
			/*
			 * Another thread is renaming in this path.
			 * Note that if we are a WRITER, we don't have any
			 * parent_locks held yet.
			 */
			if (rw == RW_READER && zp->z_id > szp->z_id) {
				/*
				 * Drop our locks and restart
				 */
				zfs_rename_unlock(&zl);
				*zlpp = NULL;
				zp = tdzp;
				oidp = &zp->z_id;
				rwlp = &szp->z_parent_lock;
				rw = RW_WRITER;
				continue;
			} else {
				/*
				 * Wait for other thread to drop its locks
				 */
				rw_enter(rwlp, rw);
			}
		}

		zl = kmem_alloc(sizeof (*zl), KM_SLEEP);
		zl->zl_rwlock = rwlp;
		zl->zl_znode = NULL;
		zl->zl_next = *zlpp;
		*zlpp = zl;

		if (*oidp == szp->z_id)		/* We're a descendant of szp */
			return (EINVAL);

		if (*oidp == rootid)		/* We've hit the top */
			return (0);

		if (rw == RW_READER) {		/* i.e. not the first pass */
			int error = zfs_zget(zp->z_zfsvfs, *oidp, &zp);
			if (error)
				return (error);
			zl->zl_znode = zp;
		}
		oidp = &zp->z_phys->zp_parent;
		rwlp = &zp->z_parent_lock;
		rw = RW_READER;

	} while (zp->z_id != sdzp->z_id);

	return (0);
}

/*
 * Move an entry from the provided source directory to the target
 * directory.  Change the entry name as indicated.
 *
 *	IN:	sdvp	- Source directory containing the "old entry".
 *		snm	- Old entry name.
 *		tdvp	- Target directory to contain the "new entry".
 *		tnm	- New entry name.
 *		cr	- credentials of caller.
 *
 *	RETURN:	0 if success
 *		error code if failure
 *
 * Timestamps:
 *	sdvp,tdvp - ctime|mtime updated
 */
static int
zfs_rename(vnode_t *sdvp, char *snm, vnode_t *tdvp, char *tnm, cred_t *cr)
{
	znode_t		*tdzp, *szp, *tzp;
	znode_t		*sdzp = VTOZ(sdvp);
	zfsvfs_t	*zfsvfs = sdzp->z_zfsvfs;
	zilog_t		*zilog = zfsvfs->z_log;
	vnode_t		*realvp;
	zfs_dirlock_t	*sdl, *tdl;
	dmu_tx_t	*tx;
	zfs_zlock_t	*zl;
	int		cmp, serr, terr, error;

	ZFS_ENTER(zfsvfs);

	/*
	 * Make sure we have the real vp for the target directory.
	 */
	if (VOP_REALVP(tdvp, &realvp) == 0)
		tdvp = realvp;

	if (tdvp->v_vfsp != sdvp->v_vfsp) {
		ZFS_EXIT(zfsvfs);
		return (EXDEV);
	}

	tdzp = VTOZ(tdvp);
top:
	szp = NULL;
	tzp = NULL;
	zl = NULL;

	/*
	 * This is to prevent the creation of links into attribute space
	 * by renaming a linked file into/outof an attribute directory.
	 * See the comment in zfs_link() for why this is considered bad.
	 */
	if ((tdzp->z_phys->zp_flags & ZFS_XATTR) !=
	    (sdzp->z_phys->zp_flags & ZFS_XATTR)) {
		ZFS_EXIT(zfsvfs);
		return (EINVAL);
	}

	/*
	 * Lock source and target directory entries.  To prevent deadlock,
	 * a lock ordering must be defined.  We lock the directory with
	 * the smallest object id first, or if it's a tie, the one with
	 * the lexically first name.
	 */
	if (sdzp->z_id < tdzp->z_id) {
		cmp = -1;
	} else if (sdzp->z_id > tdzp->z_id) {
		cmp = 1;
	} else {
		cmp = strcmp(snm, tnm);
		if (cmp == 0) {
			/*
			 * POSIX: "If the old argument and the new argument
			 * both refer to links to the same existing file,
			 * the rename() function shall return successfully
			 * and perform no other action."
			 */
			ZFS_EXIT(zfsvfs);
			return (0);
		}
	}
	if (cmp < 0) {
		serr = zfs_dirent_lock(&sdl, sdzp, snm, &szp, ZEXISTS);
		terr = zfs_dirent_lock(&tdl, tdzp, tnm, &tzp, 0);
	} else {
		terr = zfs_dirent_lock(&tdl, tdzp, tnm, &tzp, 0);
		serr = zfs_dirent_lock(&sdl, sdzp, snm, &szp, ZEXISTS);
	}

	if (serr) {
		/*
		 * Source entry invalid or not there.
		 */
		if (!terr) {
			zfs_dirent_unlock(tdl);
			if (tzp)
				VN_RELE(ZTOV(tzp));
		}
		if (strcmp(snm, ".") == 0 || strcmp(snm, "..") == 0)
			serr = EINVAL;
		ZFS_EXIT(zfsvfs);
		return (serr);
	}
	if (terr) {
		zfs_dirent_unlock(sdl);
		VN_RELE(ZTOV(szp));
		if (strcmp(tnm, "..") == 0)
			terr = EINVAL;
		ZFS_EXIT(zfsvfs);
		return (terr);
	}

	/*
	 * Must have write access at the source to remove the old entry
	 * and write access at the target to create the new entry.
	 * Note that if target and source are the same, this can be
	 * done in a single check.
	 */

	if (error = zfs_zaccess_rename(sdzp, szp, tdzp, tzp, cr))
		goto out;

	if (ZTOV(szp)->v_type == VDIR) {
		/*
		 * Check to make sure rename is valid.
		 * Can't do a move like this: /usr/a/b to /usr/a/b/c/d
		 */
		if (error = zfs_rename_lock(szp, tdzp, sdzp, &zl))
			goto out;
	}

	/*
	 * Does target exist?
	 */
	if (tzp) {
		/*
		 * Source and target must be the same type.
		 */
		if (ZTOV(szp)->v_type == VDIR) {
			if (ZTOV(tzp)->v_type != VDIR) {
				error = ENOTDIR;
				goto out;
			}
		} else {
			if (ZTOV(tzp)->v_type == VDIR) {
				error = EISDIR;
				goto out;
			}
		}
		/*
		 * POSIX dictates that when the source and target
		 * entries refer to the same file object, rename
		 * must do nothing and exit without error.
		 */
		if (szp->z_id == tzp->z_id) {
			error = 0;
			goto out;
		}
	}

	vnevent_rename_src(ZTOV(szp));
	if (tzp)
		vnevent_rename_dest(ZTOV(tzp));

	tx = dmu_tx_create(zfsvfs->z_os);
	dmu_tx_hold_bonus(tx, szp->z_id);	/* nlink changes */
	dmu_tx_hold_bonus(tx, sdzp->z_id);	/* nlink changes */
	dmu_tx_hold_zap(tx, sdzp->z_id, FALSE, snm);
	dmu_tx_hold_zap(tx, tdzp->z_id, TRUE, tnm);
	if (sdzp != tdzp)
		dmu_tx_hold_bonus(tx, tdzp->z_id);	/* nlink changes */
	if (tzp)
		dmu_tx_hold_bonus(tx, tzp->z_id);	/* parent changes */
	dmu_tx_hold_zap(tx, zfsvfs->z_unlinkedobj, FALSE, NULL);
	error = dmu_tx_assign(tx, zfsvfs->z_assign);
	if (error) {
		if (zl != NULL)
			zfs_rename_unlock(&zl);
		zfs_dirent_unlock(sdl);
		zfs_dirent_unlock(tdl);
		VN_RELE(ZTOV(szp));
		if (tzp)
			VN_RELE(ZTOV(tzp));
		if (error == ERESTART && zfsvfs->z_assign == TXG_NOWAIT) {
			dmu_tx_wait(tx);
			dmu_tx_abort(tx);
			goto top;
		}
		dmu_tx_abort(tx);
		ZFS_EXIT(zfsvfs);
		return (error);
	}

	if (tzp)	/* Attempt to remove the existing target */
		error = zfs_link_destroy(tdl, tzp, tx, 0, NULL);

	if (error == 0) {
		error = zfs_link_create(tdl, szp, tx, ZRENAMING);
		if (error == 0) {
			error = zfs_link_destroy(sdl, szp, tx, ZRENAMING, NULL);
			ASSERT(error == 0);
			zfs_log_rename(zilog, tx, TX_RENAME, sdzp,
			    sdl->dl_name, tdzp, tdl->dl_name, szp);
		}
#ifdef FREEBSD_NAMECACHE
		if (error == 0) {
			cache_purge(sdvp);
			cache_purge(tdvp);
		}
#endif
	}

	dmu_tx_commit(tx);
out:
	if (zl != NULL)
		zfs_rename_unlock(&zl);

	zfs_dirent_unlock(sdl);
	zfs_dirent_unlock(tdl);

	VN_RELE(ZTOV(szp));
	if (tzp)
		VN_RELE(ZTOV(tzp));

	ZFS_EXIT(zfsvfs);

	return (error);
}

/*
 * Insert the indicated symbolic reference entry into the directory.
 *
 *	IN:	dvp	- Directory to contain new symbolic link.
 *		link	- Name for new symlink entry.
 *		vap	- Attributes of new entry.
 *		target	- Target path of new symlink.
 *		cr	- credentials of caller.
 *
 *	RETURN:	0 if success
 *		error code if failure
 *
 * Timestamps:
 *	dvp - ctime|mtime updated
 */
static int
zfs_symlink(vnode_t *dvp, vnode_t **vpp, char *name, vattr_t *vap, char *link, cred_t *cr, kthread_t *td)
{
	znode_t		*zp, *dzp = VTOZ(dvp);
	zfs_dirlock_t	*dl;
	dmu_tx_t	*tx;
	zfsvfs_t	*zfsvfs = dzp->z_zfsvfs;
	zilog_t		*zilog = zfsvfs->z_log;
	uint64_t	zoid;
	int		len = strlen(link);
	int		error;

	ASSERT(vap->va_type == VLNK);

	ZFS_ENTER(zfsvfs);
top:
	if (error = zfs_zaccess(dzp, ACE_ADD_FILE, cr)) {
		ZFS_EXIT(zfsvfs);
		return (error);
	}

	if (len > MAXPATHLEN) {
		ZFS_EXIT(zfsvfs);
		return (ENAMETOOLONG);
	}

	/*
	 * Attempt to lock directory; fail if entry already exists.
	 */
	if (error = zfs_dirent_lock(&dl, dzp, name, &zp, ZNEW)) {
		ZFS_EXIT(zfsvfs);
		return (error);
	}

	tx = dmu_tx_create(zfsvfs->z_os);
	dmu_tx_hold_write(tx, DMU_NEW_OBJECT, 0, MAX(1, len));
	dmu_tx_hold_bonus(tx, dzp->z_id);
	dmu_tx_hold_zap(tx, dzp->z_id, TRUE, name);
	if (dzp->z_phys->zp_flags & ZFS_INHERIT_ACE)
		dmu_tx_hold_write(tx, DMU_NEW_OBJECT, 0, SPA_MAXBLOCKSIZE);
	error = dmu_tx_assign(tx, zfsvfs->z_assign);
	if (error) {
		zfs_dirent_unlock(dl);
		if (error == ERESTART && zfsvfs->z_assign == TXG_NOWAIT) {
			dmu_tx_wait(tx);
			dmu_tx_abort(tx);
			goto top;
		}
		dmu_tx_abort(tx);
		ZFS_EXIT(zfsvfs);
		return (error);
	}

	dmu_buf_will_dirty(dzp->z_dbuf, tx);

	/*
	 * Create a new object for the symlink.
	 * Put the link content into bonus buffer if it will fit;
	 * otherwise, store it just like any other file data.
	 */
	zoid = 0;
	if (sizeof (znode_phys_t) + len <= dmu_bonus_max()) {
		zfs_mknode(dzp, vap, &zoid, tx, cr, 0, &zp, len);
		if (len != 0)
			bcopy(link, zp->z_phys + 1, len);
	} else {
		dmu_buf_t *dbp;

		zfs_mknode(dzp, vap, &zoid, tx, cr, 0, &zp, 0);

		/*
		 * Nothing can access the znode yet so no locking needed
		 * for growing the znode's blocksize.
		 */
		zfs_grow_blocksize(zp, len, tx);

		VERIFY(0 == dmu_buf_hold(zfsvfs->z_os, zoid, 0, FTAG, &dbp));
		dmu_buf_will_dirty(dbp, tx);

		ASSERT3U(len, <=, dbp->db_size);
		bcopy(link, dbp->db_data, len);
		dmu_buf_rele(dbp, FTAG);
	}
	zp->z_phys->zp_size = len;

	/*
	 * Insert the new object into the directory.
	 */
	(void) zfs_link_create(dl, zp, tx, ZNEW);
out:
	if (error == 0) {
		zfs_log_symlink(zilog, tx, TX_SYMLINK, dzp, zp, name, link);
		*vpp = ZTOV(zp);
	}

	dmu_tx_commit(tx);

	zfs_dirent_unlock(dl);

	ZFS_EXIT(zfsvfs);
	return (error);
}

/*
 * Return, in the buffer contained in the provided uio structure,
 * the symbolic path referred to by vp.
 *
 *	IN:	vp	- vnode of symbolic link.
 *		uoip	- structure to contain the link path.
 *		cr	- credentials of caller.
 *
 *	OUT:	uio	- structure to contain the link path.
 *
 *	RETURN:	0 if success
 *		error code if failure
 *
 * Timestamps:
 *	vp - atime updated
 */
/* ARGSUSED */
static int
zfs_readlink(vnode_t *vp, uio_t *uio, cred_t *cr)
{
	znode_t		*zp = VTOZ(vp);
	zfsvfs_t	*zfsvfs = zp->z_zfsvfs;
	size_t		bufsz;
	int		error;

	ZFS_ENTER(zfsvfs);

	bufsz = (size_t)zp->z_phys->zp_size;
	if (bufsz + sizeof (znode_phys_t) <= zp->z_dbuf->db_size) {
		error = uiomove(zp->z_phys + 1,
		    MIN((size_t)bufsz, uio->uio_resid), UIO_READ, uio);
	} else {
		dmu_buf_t *dbp;
		error = dmu_buf_hold(zfsvfs->z_os, zp->z_id, 0, FTAG, &dbp);
		if (error) {
			ZFS_EXIT(zfsvfs);
			return (error);
		}
		error = uiomove(dbp->db_data,
		    MIN((size_t)bufsz, uio->uio_resid), UIO_READ, uio);
		dmu_buf_rele(dbp, FTAG);
	}

	ZFS_ACCESSTIME_STAMP(zfsvfs, zp);
	ZFS_EXIT(zfsvfs);
	return (error);
}

/*
 * Insert a new entry into directory tdvp referencing svp.
 *
 *	IN:	tdvp	- Directory to contain new entry.
 *		svp	- vnode of new entry.
 *		name	- name of new entry.
 *		cr	- credentials of caller.
 *
 *	RETURN:	0 if success
 *		error code if failure
 *
 * Timestamps:
 *	tdvp - ctime|mtime updated
 *	 svp - ctime updated
 */
/* ARGSUSED */
static int
zfs_link(vnode_t *tdvp, vnode_t *svp, char *name, cred_t *cr)
{
	znode_t		*dzp = VTOZ(tdvp);
	znode_t		*tzp, *szp;
	zfsvfs_t	*zfsvfs = dzp->z_zfsvfs;
	zilog_t		*zilog = zfsvfs->z_log;
	zfs_dirlock_t	*dl;
	dmu_tx_t	*tx;
	vnode_t		*realvp;
	int		error;

	ASSERT(tdvp->v_type == VDIR);

	ZFS_ENTER(zfsvfs);

	if (VOP_REALVP(svp, &realvp) == 0)
		svp = realvp;

	if (svp->v_vfsp != tdvp->v_vfsp) {
		ZFS_EXIT(zfsvfs);
		return (EXDEV);
	}

	szp = VTOZ(svp);
top:
	/*
	 * We do not support links between attributes and non-attributes
	 * because of the potential security risk of creating links
	 * into "normal" file space in order to circumvent restrictions
	 * imposed in attribute space.
	 */
	if ((szp->z_phys->zp_flags & ZFS_XATTR) !=
	    (dzp->z_phys->zp_flags & ZFS_XATTR)) {
		ZFS_EXIT(zfsvfs);
		return (EINVAL);
	}

	/*
	 * POSIX dictates that we return EPERM here.
	 * Better choices include ENOTSUP or EISDIR.
	 */
	if (svp->v_type == VDIR) {
		ZFS_EXIT(zfsvfs);
		return (EPERM);
	}

	if ((uid_t)szp->z_phys->zp_uid != crgetuid(cr) &&
	    secpolicy_basic_link(cr) != 0) {
		ZFS_EXIT(zfsvfs);
		return (EPERM);
	}

	if (error = zfs_zaccess(dzp, ACE_ADD_FILE, cr)) {
		ZFS_EXIT(zfsvfs);
		return (error);
	}

	/*
	 * Attempt to lock directory; fail if entry already exists.
	 */
	if (error = zfs_dirent_lock(&dl, dzp, name, &tzp, ZNEW)) {
		ZFS_EXIT(zfsvfs);
		return (error);
	}

	tx = dmu_tx_create(zfsvfs->z_os);
	dmu_tx_hold_bonus(tx, szp->z_id);
	dmu_tx_hold_zap(tx, dzp->z_id, TRUE, name);
	error = dmu_tx_assign(tx, zfsvfs->z_assign);
	if (error) {
		zfs_dirent_unlock(dl);
		if (error == ERESTART && zfsvfs->z_assign == TXG_NOWAIT) {
			dmu_tx_wait(tx);
			dmu_tx_abort(tx);
			goto top;
		}
		dmu_tx_abort(tx);
		ZFS_EXIT(zfsvfs);
		return (error);
	}

	error = zfs_link_create(dl, szp, tx, 0);

	if (error == 0)
		zfs_log_link(zilog, tx, TX_LINK, dzp, szp, name);

	dmu_tx_commit(tx);

	zfs_dirent_unlock(dl);

	ZFS_EXIT(zfsvfs);
	return (error);
}

void
zfs_inactive(vnode_t *vp, cred_t *cr)
{
	znode_t	*zp = VTOZ(vp);
	zfsvfs_t *zfsvfs = zp->z_zfsvfs;
	int error;

	rw_enter(&zfsvfs->z_um_lock, RW_READER);
	if (zfsvfs->z_unmounted2) {
		ASSERT(zp->z_dbuf_held == 0);

		mutex_enter(&zp->z_lock);
		VI_LOCK(vp);
		vp->v_count = 0; /* count arrives as 1 */
		VI_UNLOCK(vp);
		if (zp->z_dbuf == NULL) {
			mutex_exit(&zp->z_lock);
			zfs_znode_free(zp);
		} else {
			mutex_exit(&zp->z_lock);
		}
		rw_exit(&zfsvfs->z_um_lock);
		VFS_RELE(zfsvfs->z_vfs);
		return;
	}

	if (zp->z_atime_dirty && zp->z_unlinked == 0) {
		dmu_tx_t *tx = dmu_tx_create(zfsvfs->z_os);

		dmu_tx_hold_bonus(tx, zp->z_id);
		error = dmu_tx_assign(tx, TXG_WAIT);
		if (error) {
			dmu_tx_abort(tx);
		} else {
			dmu_buf_will_dirty(zp->z_dbuf, tx);
			mutex_enter(&zp->z_lock);
			zp->z_atime_dirty = 0;
			mutex_exit(&zp->z_lock);
			dmu_tx_commit(tx);
		}
	}

	zfs_zinactive(zp);
	rw_exit(&zfsvfs->z_um_lock);
}

CTASSERT(sizeof(struct zfid_short) <= sizeof(struct fid));
CTASSERT(sizeof(struct zfid_long) <= sizeof(struct fid));

static int
zfs_fid(vnode_t *vp, fid_t *fidp)
{
	znode_t		*zp = VTOZ(vp);
	zfsvfs_t	*zfsvfs = zp->z_zfsvfs;
	uint32_t	gen = (uint32_t)zp->z_phys->zp_gen;
	uint64_t	object = zp->z_id;
	zfid_short_t	*zfid;
	int		size, i;

	ZFS_ENTER(zfsvfs);

	size = (zfsvfs->z_parent != zfsvfs) ? LONG_FID_LEN : SHORT_FID_LEN;
	fidp->fid_len = size;

	zfid = (zfid_short_t *)fidp;

	zfid->zf_len = size;

	for (i = 0; i < sizeof (zfid->zf_object); i++)
		zfid->zf_object[i] = (uint8_t)(object >> (8 * i));

	/* Must have a non-zero generation number to distinguish from .zfs */
	if (gen == 0)
		gen = 1;
	for (i = 0; i < sizeof (zfid->zf_gen); i++)
		zfid->zf_gen[i] = (uint8_t)(gen >> (8 * i));

	if (size == LONG_FID_LEN) {
		uint64_t	objsetid = dmu_objset_id(zfsvfs->z_os);
		zfid_long_t	*zlfid;

		zlfid = (zfid_long_t *)fidp;

		for (i = 0; i < sizeof (zlfid->zf_setid); i++)
			zlfid->zf_setid[i] = (uint8_t)(objsetid >> (8 * i));

		/* XXX - this should be the generation number for the objset */
		for (i = 0; i < sizeof (zlfid->zf_setgen); i++)
			zlfid->zf_setgen[i] = 0;
	}

	ZFS_EXIT(zfsvfs);
	return (0);
}

static int
zfs_pathconf(vnode_t *vp, int cmd, ulong_t *valp, cred_t *cr)
{
	znode_t		*zp, *xzp;
	zfsvfs_t	*zfsvfs;
	zfs_dirlock_t	*dl;
	int		error;

	switch (cmd) {
	case _PC_LINK_MAX:
		*valp = INT_MAX;
		return (0);

	case _PC_FILESIZEBITS:
		*valp = 64;
		return (0);

#if 0
	case _PC_XATTR_EXISTS:
		zp = VTOZ(vp);
		zfsvfs = zp->z_zfsvfs;
		ZFS_ENTER(zfsvfs);
		*valp = 0;
		error = zfs_dirent_lock(&dl, zp, "", &xzp,
		    ZXATTR | ZEXISTS | ZSHARED);
		if (error == 0) {
			zfs_dirent_unlock(dl);
			if (!zfs_dirempty(xzp))
				*valp = 1;
			VN_RELE(ZTOV(xzp));
		} else if (error == ENOENT) {
			/*
			 * If there aren't extended attributes, it's the
			 * same as having zero of them.
			 */
			error = 0;
		}
		ZFS_EXIT(zfsvfs);
		return (error);
#endif

	case _PC_ACL_EXTENDED:
		*valp = 0;	/* TODO */
		return (0);

	case _PC_MIN_HOLE_SIZE:
		*valp = (int)SPA_MINBLOCKSIZE;
		return (0);

	default:
		return (EOPNOTSUPP);
	}
}

#ifdef TODO
/*ARGSUSED*/
static int
zfs_getsecattr(vnode_t *vp, vsecattr_t *vsecp, int flag, cred_t *cr)
{
	znode_t *zp = VTOZ(vp);
	zfsvfs_t *zfsvfs = zp->z_zfsvfs;
	int error;

	ZFS_ENTER(zfsvfs);
	error = zfs_getacl(zp, vsecp, cr);
	ZFS_EXIT(zfsvfs);

	return (error);
}
#endif	/* TODO */

#ifdef TODO
/*ARGSUSED*/
static int
zfs_setsecattr(vnode_t *vp, vsecattr_t *vsecp, int flag, cred_t *cr)
{
	znode_t *zp = VTOZ(vp);
	zfsvfs_t *zfsvfs = zp->z_zfsvfs;
	int error;

	ZFS_ENTER(zfsvfs);
	error = zfs_setacl(zp, vsecp, cr);
	ZFS_EXIT(zfsvfs);
	return (error);
}
#endif	/* TODO */

static int
zfs_freebsd_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	vnode_t	*vp = ap->a_vp;
	znode_t *zp = VTOZ(vp);
	int error;

	error = zfs_open(&vp, ap->a_mode, ap->a_cred);
	if (error == 0)
		vnode_create_vobject(vp, zp->z_phys->zp_size, ap->a_td);
	return (error);
}

static int
zfs_freebsd_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{

	return (zfs_close(ap->a_vp, ap->a_fflag, 0, 0, ap->a_cred));
}

static int
zfs_freebsd_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long a_command;
		caddr_t a_data;
		int a_fflag;
		struct ucred *cred;
		struct thread *td;
	} */ *ap;
{

	return (zfs_ioctl(ap->a_vp, ap->a_command, (intptr_t)ap->a_data,
	    ap->a_fflag, ap->a_cred, NULL));
}

static int
zfs_freebsd_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{

	return (zfs_read(ap->a_vp, ap->a_uio, ap->a_ioflag, ap->a_cred, NULL));
}

static int
zfs_freebsd_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{

	return (zfs_write(ap->a_vp, ap->a_uio, ap->a_ioflag, ap->a_cred, NULL));
}

static int
zfs_freebsd_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{

	return (zfs_access(ap->a_vp, ap->a_mode, 0, ap->a_cred));
}

static int
zfs_freebsd_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	char nm[NAME_MAX + 1];

	ASSERT(cnp->cn_namelen < sizeof(nm));
	strlcpy(nm, cnp->cn_nameptr, MIN(cnp->cn_namelen + 1, sizeof(nm)));

	return (zfs_lookup(ap->a_dvp, nm, ap->a_vpp, cnp, cnp->cn_nameiop,
	    cnp->cn_cred, cnp->cn_thread));
}

static int
zfs_freebsd_create(ap)
	struct vop_create_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	vattr_t *vap = ap->a_vap;
	int mode;

	ASSERT(cnp->cn_flags & SAVENAME);

	vattr_init_mask(vap);
	mode = vap->va_mode & ALLPERMS;

	return (zfs_create(ap->a_dvp, cnp->cn_nameptr, vap, !EXCL, mode,
	    ap->a_vpp, cnp->cn_cred));
}

static int
zfs_freebsd_remove(ap)
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{

	ASSERT(ap->a_cnp->cn_flags & SAVENAME);

	return (zfs_remove(ap->a_dvp, ap->a_cnp->cn_nameptr,
	    ap->a_cnp->cn_cred));
}

static int
zfs_freebsd_mkdir(ap)
	struct vop_mkdir_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	vattr_t *vap = ap->a_vap;

	ASSERT(ap->a_cnp->cn_flags & SAVENAME);

	vattr_init_mask(vap);

	return (zfs_mkdir(ap->a_dvp, ap->a_cnp->cn_nameptr, vap, ap->a_vpp,
	    ap->a_cnp->cn_cred));
}

static int
zfs_freebsd_rmdir(ap)
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;

	ASSERT(cnp->cn_flags & SAVENAME);

	return (zfs_rmdir(ap->a_dvp, cnp->cn_nameptr, NULL, cnp->cn_cred));
}

static int
zfs_freebsd_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		int *a_ncookies;
		u_long **a_cookies;
	} */ *ap;
{

	return (zfs_readdir(ap->a_vp, ap->a_uio, ap->a_cred, ap->a_eofflag,
	    ap->a_ncookies, ap->a_cookies));
}

static int
zfs_freebsd_fsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		int a_waitfor;
		struct thread *a_td;
	} */ *ap;
{

	vop_stdfsync(ap);
	return (zfs_fsync(ap->a_vp, 0, ap->a_td->td_ucred));
}

static int
zfs_freebsd_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
	} */ *ap;
{

	return (zfs_getattr(ap->a_vp, ap->a_vap, 0, ap->a_cred));
}

static int
zfs_freebsd_setattr(ap)
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
	} */ *ap;
{
	vattr_t *vap = ap->a_vap;

	/* No support for FreeBSD's chflags(2). */
	if (vap->va_flags != VNOVAL)
		return (EOPNOTSUPP);

	vattr_init_mask(vap);
	vap->va_mask &= ~AT_NOSET;

	return (zfs_setattr(ap->a_vp, vap, 0, ap->a_cred, NULL));
}

static int
zfs_freebsd_rename(ap)
	struct vop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap;
{
	vnode_t *fdvp = ap->a_fdvp;
	vnode_t *fvp = ap->a_fvp;
	vnode_t *tdvp = ap->a_tdvp;
	vnode_t *tvp = ap->a_tvp;
	int error;

	ASSERT(ap->a_fcnp->cn_flags & SAVENAME);
	ASSERT(ap->a_tcnp->cn_flags & SAVENAME);

	error = zfs_rename(fdvp, ap->a_fcnp->cn_nameptr, tdvp,
	    ap->a_tcnp->cn_nameptr, ap->a_fcnp->cn_cred);

	if (tdvp == tvp)
		VN_RELE(tdvp);
	else
		VN_URELE(tdvp);
	if (tvp)
		VN_URELE(tvp);
	VN_RELE(fdvp);
	VN_RELE(fvp);

	return (error);
}

static int
zfs_freebsd_symlink(ap)
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	vattr_t *vap = ap->a_vap;

	ASSERT(cnp->cn_flags & SAVENAME);

	vap->va_type = VLNK;	/* FreeBSD: Syscall only sets va_mode. */
	vattr_init_mask(vap);

	return (zfs_symlink(ap->a_dvp, ap->a_vpp, cnp->cn_nameptr, vap,
	    ap->a_target, cnp->cn_cred, cnp->cn_thread));
}

static int
zfs_freebsd_readlink(ap)
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap;
{

	return (zfs_readlink(ap->a_vp, ap->a_uio, ap->a_cred));
}

static int
zfs_freebsd_link(ap)
	struct vop_link_args /* {
		struct vnode *a_tdvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;

	ASSERT(cnp->cn_flags & SAVENAME);

	return (zfs_link(ap->a_tdvp, ap->a_vp, cnp->cn_nameptr, cnp->cn_cred));
}

static int
zfs_freebsd_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap;
{
	vnode_t *vp = ap->a_vp;

	zfs_inactive(vp, ap->a_td->td_ucred);
	return (0);
}

static int
zfs_freebsd_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap;
{
	vnode_t	*vp = ap->a_vp;
	znode_t	*zp = VTOZ(vp);
	zfsvfs_t *zfsvfs;
	int rele = 1;

	ASSERT(zp != NULL);

	/*
	 * Destroy the vm object and flush associated pages.
	 */
	vnode_destroy_vobject(vp);

	mutex_enter(&zp->z_lock);
	ASSERT(zp->z_phys);
	ASSERT(zp->z_dbuf_held);
	zfsvfs = zp->z_zfsvfs;
	if (!zp->z_unlinked) {
		zp->z_dbuf_held = 0;
		ZTOV(zp) = NULL;
		mutex_exit(&zp->z_lock);
		dmu_buf_rele(zp->z_dbuf, NULL);
	} else {
		mutex_exit(&zp->z_lock);
	}
	VI_LOCK(vp);
	if (vp->v_count > 0)
		rele = 0;
	vp->v_data = NULL;
	ASSERT(vp->v_holdcnt >= 1);
	VI_UNLOCK(vp);
	if (!zp->z_unlinked && rele)
		VFS_RELE(zfsvfs->z_vfs);
	return (0);
}

static int
zfs_freebsd_fid(ap)
	struct vop_fid_args /* {
		struct vnode *a_vp;
		struct fid *a_fid;
	} */ *ap;
{

	return (zfs_fid(ap->a_vp, (void *)ap->a_fid));
}

static int
zfs_freebsd_pathconf(ap)
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap;
{
	ulong_t val;
	int error;

	error = zfs_pathconf(ap->a_vp, ap->a_name, &val, curthread->td_ucred);
	if (error == 0)
		*ap->a_retval = val;
	else if (error == EOPNOTSUPP)
		error = vop_stdpathconf(ap);
	return (error);
}

struct vop_vector zfs_vnodeops;
struct vop_vector zfs_fifoops;

struct vop_vector zfs_vnodeops = {
	.vop_default =	&default_vnodeops,
	.vop_inactive =	zfs_freebsd_inactive,
	.vop_reclaim =	zfs_freebsd_reclaim,
	.vop_access =	zfs_freebsd_access,
#ifdef FREEBSD_NAMECACHE
	.vop_lookup =	vfs_cache_lookup,
	.vop_cachedlookup = zfs_freebsd_lookup,
#else
	.vop_lookup =	zfs_freebsd_lookup,
#endif
	.vop_getattr =	zfs_freebsd_getattr,
	.vop_setattr =	zfs_freebsd_setattr,
	.vop_create =	zfs_freebsd_create,
	.vop_mknod =	zfs_freebsd_create,
	.vop_mkdir =	zfs_freebsd_mkdir,
	.vop_readdir =	zfs_freebsd_readdir,
	.vop_fsync =	zfs_freebsd_fsync,
	.vop_open =	zfs_freebsd_open,
	.vop_close =	zfs_freebsd_close,
	.vop_rmdir =	zfs_freebsd_rmdir,
	.vop_ioctl =	zfs_freebsd_ioctl,
	.vop_link =	zfs_freebsd_link,
	.vop_symlink =	zfs_freebsd_symlink,
	.vop_readlink =	zfs_freebsd_readlink,
	.vop_read =	zfs_freebsd_read,
	.vop_write =	zfs_freebsd_write,
	.vop_remove =	zfs_freebsd_remove,
	.vop_rename =	zfs_freebsd_rename,
	.vop_pathconf =	zfs_freebsd_pathconf,
	.vop_bmap =	VOP_EOPNOTSUPP,
	.vop_fid =	zfs_freebsd_fid,
};

struct vop_vector zfs_fifoops = {
	.vop_default =	&fifo_specops,
	.vop_fsync =	VOP_PANIC,
	.vop_access =	zfs_freebsd_access,
	.vop_getattr =	zfs_freebsd_getattr,
	.vop_inactive =	zfs_freebsd_inactive,
	.vop_read =	VOP_PANIC,
	.vop_reclaim =	zfs_freebsd_reclaim,
	.vop_setattr =	zfs_freebsd_setattr,
	.vop_write =	VOP_PANIC,
	.vop_fid =	zfs_freebsd_fid,
};
