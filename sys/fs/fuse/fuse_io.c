/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2007-2009 Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of Google Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (C) 2005 Csaba Henk.
 * All rights reserved.
 *
 * Copyright (c) 2019 The FreeBSD Foundation
 *
 * Portions of this software were developed by BFF Storage Systems, LLC under
 * sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/sx.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <sys/filedesc.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>

#include "fuse.h"
#include "fuse_file.h"
#include "fuse_node.h"
#include "fuse_internal.h"
#include "fuse_ipc.h"
#include "fuse_io.h"

/* 
 * Set in a struct buf to indicate that the write came from the buffer cache
 * and the originating cred and pid are no longer known.
 */
#define B_FUSEFS_WRITE_CACHE B_FS_FLAG1

SDT_PROVIDER_DECLARE(fusefs);
/* 
 * Fuse trace probe:
 * arg0: verbosity.  Higher numbers give more verbose messages
 * arg1: Textual message
 */
SDT_PROBE_DEFINE2(fusefs, , io, trace, "int", "char*");

static int
fuse_inval_buf_range(struct vnode *vp, off_t filesize, off_t start, off_t end);
static int 
fuse_read_directbackend(struct vnode *vp, struct uio *uio,
    struct ucred *cred, struct fuse_filehandle *fufh);
static int 
fuse_read_biobackend(struct vnode *vp, struct uio *uio, int ioflag,
    struct ucred *cred, struct fuse_filehandle *fufh, pid_t pid);
static int 
fuse_write_directbackend(struct vnode *vp, struct uio *uio,
    struct ucred *cred, struct fuse_filehandle *fufh, off_t filesize,
    int ioflag, bool pages);
static int 
fuse_write_biobackend(struct vnode *vp, struct uio *uio,
    struct ucred *cred, struct fuse_filehandle *fufh, int ioflag, pid_t pid);

/* Invalidate a range of cached data, whether dirty of not */
static int
fuse_inval_buf_range(struct vnode *vp, off_t filesize, off_t start, off_t end)
{
	struct buf *bp;
	daddr_t left_lbn, end_lbn, right_lbn;
	off_t new_filesize;
	int iosize, left_on, right_on, right_blksize;

	iosize = fuse_iosize(vp);
	left_lbn = start / iosize;
	end_lbn = howmany(end, iosize);
	left_on = start & (iosize - 1);
	if (left_on != 0) {
		bp = getblk(vp, left_lbn, iosize, PCATCH, 0, 0);
		if ((bp->b_flags & B_CACHE) != 0 && bp->b_dirtyend >= left_on) {
			/* 
			 * Flush the dirty buffer, because we don't have a
			 * byte-granular way to record which parts of the
			 * buffer are valid.
			 */
			bwrite(bp);
			if (bp->b_error)
				return (bp->b_error);
		} else {
			brelse(bp);
		}
	}
	right_on = end & (iosize - 1);
	if (right_on != 0) {
		right_lbn = end / iosize;
		new_filesize = MAX(filesize, end);
		right_blksize = MIN(iosize, new_filesize - iosize * right_lbn);
		bp = getblk(vp, right_lbn, right_blksize, PCATCH, 0, 0);
		if ((bp->b_flags & B_CACHE) != 0 && bp->b_dirtyoff < right_on) {
			/* 
			 * Flush the dirty buffer, because we don't have a
			 * byte-granular way to record which parts of the
			 * buffer are valid.
			 */
			bwrite(bp);
			if (bp->b_error)
				return (bp->b_error);
		} else {
			brelse(bp);
		}
	}

	v_inval_buf_range(vp, left_lbn, end_lbn, iosize);
	return (0);
}

SDT_PROBE_DEFINE5(fusefs, , io, io_dispatch, "struct vnode*", "struct uio*",
		"int", "struct ucred*", "struct fuse_filehandle*");
SDT_PROBE_DEFINE4(fusefs, , io, io_dispatch_filehandles_closed, "struct vnode*",
    "struct uio*", "int", "struct ucred*");
int
fuse_io_dispatch(struct vnode *vp, struct uio *uio, int ioflag,
    struct ucred *cred, pid_t pid)
{
	struct fuse_filehandle *fufh;
	int err, directio;
	int fflag;
	bool closefufh = false;

	MPASS(vp->v_type == VREG || vp->v_type == VDIR);

	fflag = (uio->uio_rw == UIO_READ) ? FREAD : FWRITE;
	err = fuse_filehandle_getrw(vp, fflag, &fufh, cred, pid);
	if (err == EBADF && vnode_mount(vp)->mnt_flag & MNT_EXPORTED) {
		/* 
		 * nfsd will do I/O without first doing VOP_OPEN.  We
		 * must implicitly open the file here
		 */
		err = fuse_filehandle_open(vp, fflag, &fufh, curthread, cred);
		closefufh = true;
	}
	else if (err) {
		SDT_PROBE4(fusefs, , io, io_dispatch_filehandles_closed,
			vp, uio, ioflag, cred);
		printf("FUSE: io dispatch: filehandles are closed\n");
		return err;
	}
	if (err)
		goto out;
	SDT_PROBE5(fusefs, , io, io_dispatch, vp, uio, ioflag, cred, fufh);

	/*
         * Ideally, when the daemon asks for direct io at open time, the
         * standard file flag should be set according to this, so that would
         * just change the default mode, which later on could be changed via
         * fcntl(2).
         * But this doesn't work, the O_DIRECT flag gets cleared at some point
         * (don't know where). So to make any use of the Fuse direct_io option,
         * we hardwire it into the file's private data (similarly to Linux,
         * btw.).
         */
	directio = (ioflag & IO_DIRECT) || !fsess_opt_datacache(vnode_mount(vp));

	switch (uio->uio_rw) {
	case UIO_READ:
		if (directio) {
			SDT_PROBE2(fusefs, , io, trace, 1,
				"direct read of vnode");
			err = fuse_read_directbackend(vp, uio, cred, fufh);
		} else {
			SDT_PROBE2(fusefs, , io, trace, 1,
				"buffered read of vnode");
			err = fuse_read_biobackend(vp, uio, ioflag, cred, fufh,
				pid);
		}
		break;
	case UIO_WRITE:
		fuse_vnode_update(vp, FN_MTIMECHANGE | FN_CTIMECHANGE);
		if (directio) {
			off_t start, end, filesize;
			bool pages = (ioflag & IO_VMIO) != 0;

			SDT_PROBE2(fusefs, , io, trace, 1,
				"direct write of vnode");

			err = fuse_vnode_size(vp, &filesize, cred, curthread);
			if (err)
				goto out;

			start = uio->uio_offset;
			end = start + uio->uio_resid;
			if (!pages) {
				err = fuse_inval_buf_range(vp, filesize, start,
				    end);
				if (err)
					return (err);
			}
			err = fuse_write_directbackend(vp, uio, cred, fufh,
				filesize, ioflag, pages);
		} else {
			SDT_PROBE2(fusefs, , io, trace, 1,
				"buffered write of vnode");
			if (!fsess_opt_writeback(vnode_mount(vp)))
				ioflag |= IO_SYNC;
			err = fuse_write_biobackend(vp, uio, cred, fufh, ioflag,
				pid);
		}
		fuse_internal_clear_suid_on_write(vp, cred, uio->uio_td);
		break;
	default:
		panic("uninterpreted mode passed to fuse_io_dispatch");
	}

out:
	if (closefufh)
		fuse_filehandle_close(vp, fufh, curthread, cred);

	return (err);
}

SDT_PROBE_DEFINE4(fusefs, , io, read_bio_backend_start, "int", "int", "int", "int");
SDT_PROBE_DEFINE2(fusefs, , io, read_bio_backend_feed, "int", "struct buf*");
SDT_PROBE_DEFINE4(fusefs, , io, read_bio_backend_end, "int", "ssize_t", "int",
		"struct buf*");
static int
fuse_read_biobackend(struct vnode *vp, struct uio *uio, int ioflag,
    struct ucred *cred, struct fuse_filehandle *fufh, pid_t pid)
{
	struct buf *bp;
	struct mount *mp;
	struct fuse_data *data;
	daddr_t lbn, nextlbn;
	int bcount, nextsize;
	int err, n = 0, on = 0, seqcount;
	off_t filesize;

	const int biosize = fuse_iosize(vp);
	mp = vnode_mount(vp);
	data = fuse_get_mpdata(mp);

	if (uio->uio_offset < 0)
		return (EINVAL);

	seqcount = ioflag >> IO_SEQSHIFT;

	err = fuse_vnode_size(vp, &filesize, cred, curthread);
	if (err)
		return err;

	for (err = 0, bp = NULL; uio->uio_resid > 0; bp = NULL) {
		if (fuse_isdeadfs(vp)) {
			err = ENXIO;
			break;
		}
		if (filesize - uio->uio_offset <= 0)
			break;
		lbn = uio->uio_offset / biosize;
		on = uio->uio_offset & (biosize - 1);

		if ((off_t)lbn * biosize >= filesize) {
			bcount = 0;
		} else if ((off_t)(lbn + 1) * biosize > filesize) {
			bcount = filesize - (off_t)lbn *biosize;
		} else {
			bcount = biosize;
		}
		nextlbn = lbn + 1;
		nextsize = MIN(biosize, filesize - nextlbn * biosize);

		SDT_PROBE4(fusefs, , io, read_bio_backend_start,
			biosize, (int)lbn, on, bcount);

		if (bcount < biosize) {
			/* If near EOF, don't do readahead */
			err = bread(vp, lbn, bcount, NOCRED, &bp);
		} else if ((vp->v_mount->mnt_flag & MNT_NOCLUSTERR) == 0) {
			/* Try clustered read */
			long totread = uio->uio_resid + on;
			seqcount = MIN(seqcount,
				data->max_readahead_blocks + 1);
			err = cluster_read(vp, filesize, lbn, bcount, NOCRED,
				totread, seqcount, 0, &bp);
		} else if (seqcount > 1 && data->max_readahead_blocks >= 1) {
			/* Try non-clustered readahead */
			err = breadn(vp, lbn, bcount, &nextlbn, &nextsize, 1,
				NOCRED, &bp);
		} else {
			/* Just read what was requested */
			err = bread(vp, lbn, bcount, NOCRED, &bp);
		}

		if (err) {
			brelse(bp);
			bp = NULL;
			break;
		}

		/*
	         * on is the offset into the current bp.  Figure out how many
	         * bytes we can copy out of the bp.  Note that bcount is
	         * NOT DEV_BSIZE aligned.
	         *
	         * Then figure out how many bytes we can copy into the uio.
	         */

		n = 0;
		if (on < bcount - bp->b_resid)
			n = MIN((unsigned)(bcount - bp->b_resid - on),
			    uio->uio_resid);
		if (n > 0) {
			SDT_PROBE2(fusefs, , io, read_bio_backend_feed, n, bp);
			err = uiomove(bp->b_data + on, n, uio);
		}
		vfs_bio_brelse(bp, ioflag);
		SDT_PROBE4(fusefs, , io, read_bio_backend_end, err,
			uio->uio_resid, n, bp);
		if (bp->b_resid > 0) {
			/* Short read indicates EOF */
			break;
		}
	}

	return (err);
}

SDT_PROBE_DEFINE1(fusefs, , io, read_directbackend_start,
	"struct fuse_read_in*");
SDT_PROBE_DEFINE3(fusefs, , io, read_directbackend_complete,
	"struct fuse_dispatcher*", "struct fuse_read_in*", "struct uio*");

static int
fuse_read_directbackend(struct vnode *vp, struct uio *uio,
    struct ucred *cred, struct fuse_filehandle *fufh)
{
	struct fuse_data *data;
	struct fuse_dispatcher fdi;
	struct fuse_read_in *fri;
	int err = 0;

	data = fuse_get_mpdata(vp->v_mount);

	if (uio->uio_resid == 0)
		return (0);

	fdisp_init(&fdi, 0);

	/*
         * XXX In "normal" case we use an intermediate kernel buffer for
         * transmitting data from daemon's context to ours. Eventually, we should
         * get rid of this. Anyway, if the target uio lives in sysspace (we are
         * called from pageops), and the input data doesn't need kernel-side
         * processing (we are not called from readdir) we can already invoke
         * an optimized, "peer-to-peer" I/O routine.
         */
	while (uio->uio_resid > 0) {
		fdi.iosize = sizeof(*fri);
		fdisp_make_vp(&fdi, FUSE_READ, vp, uio->uio_td, cred);
		fri = fdi.indata;
		fri->fh = fufh->fh_id;
		fri->offset = uio->uio_offset;
		fri->size = MIN(uio->uio_resid,
		    fuse_get_mpdata(vp->v_mount)->max_read);
		if (fuse_libabi_geq(data, 7, 9)) {
			/* See comment regarding FUSE_WRITE_LOCKOWNER */
			fri->read_flags = 0;
			fri->flags = fufh_type_2_fflags(fufh->fufh_type);
		}

		SDT_PROBE1(fusefs, , io, read_directbackend_start, fri);

		if ((err = fdisp_wait_answ(&fdi)))
			goto out;

		SDT_PROBE3(fusefs, , io, read_directbackend_complete,
			&fdi, fri, uio);

		if ((err = uiomove(fdi.answ, MIN(fri->size, fdi.iosize), uio)))
			break;
		if (fdi.iosize < fri->size) {
			/* 
			 * Short read.  Should only happen at EOF or with
			 * direct io.
			 */
			break;
		}
	}

out:
	fdisp_destroy(&fdi);
	return (err);
}

static int
fuse_write_directbackend(struct vnode *vp, struct uio *uio,
    struct ucred *cred, struct fuse_filehandle *fufh, off_t filesize,
    int ioflag, bool pages)
{
	struct fuse_vnode_data *fvdat = VTOFUD(vp);
	struct fuse_data *data;
	struct fuse_write_in *fwi;
	struct fuse_write_out *fwo;
	struct fuse_dispatcher fdi;
	size_t chunksize;
	void *fwi_data;
	off_t as_written_offset;
	int diff;
	int err = 0;
	bool direct_io = fufh->fuse_open_flags & FOPEN_DIRECT_IO;
	bool wrote_anything = false;
	uint32_t write_flags;

	data = fuse_get_mpdata(vp->v_mount);

	/* 
	 * Don't set FUSE_WRITE_LOCKOWNER in write_flags.  It can't be set
	 * accurately when using POSIX AIO, libfuse doesn't use it, and I'm not
	 * aware of any file systems that do.  It was an attempt to add
	 * Linux-style mandatory locking to the FUSE protocol, but mandatory
	 * locking is deprecated even on Linux.  See Linux commit
	 * f33321141b273d60cbb3a8f56a5489baad82ba5e .
	 */
	/*
	 * Set FUSE_WRITE_CACHE whenever we don't know the uid, gid, and/or pid
	 * that originated a write.  For example when writing from the
	 * writeback cache.  I don't know of a single file system that cares,
	 * but the protocol says we're supposed to do this.
	 */
	write_flags = !pages && (
		(ioflag & IO_DIRECT) ||
		!fsess_opt_datacache(vnode_mount(vp)) ||
		!fsess_opt_writeback(vnode_mount(vp))) ? 0 : FUSE_WRITE_CACHE;

	if (uio->uio_resid == 0)
		return (0);

	if (ioflag & IO_APPEND)
		uio_setoffset(uio, filesize);

	if (vn_rlimit_fsize(vp, uio, uio->uio_td))
		return (EFBIG);

	fdisp_init(&fdi, 0);

	while (uio->uio_resid > 0) {
		size_t sizeof_fwi;

		if (fuse_libabi_geq(data, 7, 9)) {
			sizeof_fwi = sizeof(*fwi);
		} else {
			sizeof_fwi = FUSE_COMPAT_WRITE_IN_SIZE;
		}

		chunksize = MIN(uio->uio_resid, data->max_write);

		fdi.iosize = sizeof_fwi + chunksize;
		fdisp_make_vp(&fdi, FUSE_WRITE, vp, uio->uio_td, cred);

		fwi = fdi.indata;
		fwi->fh = fufh->fh_id;
		fwi->offset = uio->uio_offset;
		fwi->size = chunksize;
		fwi->write_flags = write_flags;
		if (fuse_libabi_geq(data, 7, 9)) {
			fwi->flags = fufh_type_2_fflags(fufh->fufh_type);
		}
		fwi_data = (char *)fdi.indata + sizeof_fwi;

		if ((err = uiomove(fwi_data, chunksize, uio)))
			break;

retry:
		err = fdisp_wait_answ(&fdi);
		if (err == ERESTART || err == EINTR || err == EWOULDBLOCK) {
			/*
			 * Rewind the uio so dofilewrite will know it's
			 * incomplete
			 */
			uio->uio_resid += fwi->size;
			uio->uio_offset -= fwi->size;
			/* 
			 * Change ERESTART into EINTR because we can't rewind
			 * uio->uio_iov.  Basically, once uiomove(9) has been
			 * called, it's impossible to restart a syscall.
			 */
			if (err == ERESTART)
				err = EINTR;
			break;
		} else if (err) {
			break;
		} else {
			wrote_anything = true;
		}

		fwo = ((struct fuse_write_out *)fdi.answ);

		/* Adjust the uio in the case of short writes */
		diff = fwi->size - fwo->size;
		as_written_offset = uio->uio_offset - diff;

		if (as_written_offset - diff > filesize)
			fuse_vnode_setsize(vp, as_written_offset);
		if (as_written_offset - diff >= filesize)
			fvdat->flag &= ~FN_SIZECHANGE;

		if (diff < 0) {
			printf("WARNING: misbehaving FUSE filesystem "
				"wrote more data than we provided it\n");
			err = EINVAL;
			break;
		} else if (diff > 0) {
			/* Short write */
			if (!direct_io) {
				printf("WARNING: misbehaving FUSE filesystem: "
					"short writes are only allowed with "
					"direct_io\n");
			}
			if (ioflag & IO_DIRECT) {
				/* Return early */
				uio->uio_resid += diff;
				uio->uio_offset -= diff;
				break;
			} else {
				/* Resend the unwritten portion of data */
				fdi.iosize = sizeof_fwi + diff;
				/* Refresh fdi without clearing data buffer */
				fdisp_refresh_vp(&fdi, FUSE_WRITE, vp,
					uio->uio_td, cred);
				fwi = fdi.indata;
				MPASS2(fwi == fdi.indata, "FUSE dispatcher "
					"reallocated despite no increase in "
					"size?");
				void *src = (char*)fwi_data + fwo->size;
				memmove(fwi_data, src, diff);
				fwi->fh = fufh->fh_id;
				fwi->offset = as_written_offset;
				fwi->size = diff;
				fwi->write_flags = write_flags;
				goto retry;
			}
		}
	}

	fdisp_destroy(&fdi);

	if (wrote_anything)
		fuse_vnode_undirty_cached_timestamps(vp);

	return (err);
}

SDT_PROBE_DEFINE6(fusefs, , io, write_biobackend_start, "int64_t", "int", "int",
		"struct uio*", "int", "bool");
SDT_PROBE_DEFINE2(fusefs, , io, write_biobackend_append_race, "long", "int");
SDT_PROBE_DEFINE2(fusefs, , io, write_biobackend_issue, "int", "struct buf*");

static int
fuse_write_biobackend(struct vnode *vp, struct uio *uio,
    struct ucred *cred, struct fuse_filehandle *fufh, int ioflag, pid_t pid)
{
	struct fuse_vnode_data *fvdat = VTOFUD(vp);
	struct buf *bp;
	daddr_t lbn;
	off_t filesize;
	int bcount;
	int n, on, seqcount, err = 0;
	bool last_page;

	const int biosize = fuse_iosize(vp);

	seqcount = ioflag >> IO_SEQSHIFT;

	KASSERT(uio->uio_rw == UIO_WRITE, ("fuse_write_biobackend mode"));
	if (vp->v_type != VREG)
		return (EIO);
	if (uio->uio_offset < 0)
		return (EINVAL);
	if (uio->uio_resid == 0)
		return (0);

	err = fuse_vnode_size(vp, &filesize, cred, curthread);
	if (err)
		return err;

	if (ioflag & IO_APPEND)
		uio_setoffset(uio, filesize);

	if (vn_rlimit_fsize(vp, uio, uio->uio_td))
		return (EFBIG);

	do {
		bool direct_append, extending;

		if (fuse_isdeadfs(vp)) {
			err = ENXIO;
			break;
		}
		lbn = uio->uio_offset / biosize;
		on = uio->uio_offset & (biosize - 1);
		n = MIN((unsigned)(biosize - on), uio->uio_resid);

again:
		/* Get or create a buffer for the write */
		direct_append = uio->uio_offset == filesize && n;
		if (uio->uio_offset + n < filesize) {
			extending = false;
			if ((off_t)(lbn + 1) * biosize < filesize) {
				/* Not the file's last block */
				bcount = biosize;
			} else {
				/* The file's last block */
				bcount = filesize - (off_t)lbn * biosize;
			}
		} else {
			extending = true;
			bcount = on + n;
		}
		if (howmany(((off_t)lbn * biosize + on + n - 1), PAGE_SIZE) >=
		    howmany(filesize, PAGE_SIZE))
			last_page = true;
		else
			last_page = false;
		if (direct_append) {
			/* 
			 * Take care to preserve the buffer's B_CACHE state so
			 * as not to cause an unnecessary read.
			 */
			bp = getblk(vp, lbn, on, PCATCH, 0, 0);
			if (bp != NULL) {
				uint32_t save = bp->b_flags & B_CACHE;
				allocbuf(bp, bcount);
				bp->b_flags |= save;
			}
		} else {
			bp = getblk(vp, lbn, bcount, PCATCH, 0, 0);
		}
		if (!bp) {
			err = EINTR;
			break;
		}
		if (extending) {
			/* 
			 * Extend file _after_ locking buffer so we won't race
			 * with other readers
			 */
			err = fuse_vnode_setsize(vp, uio->uio_offset + n);
			filesize = uio->uio_offset + n;
			fvdat->flag |= FN_SIZECHANGE;
			if (err) {
				brelse(bp);
				break;
			} 
		}

		SDT_PROBE6(fusefs, , io, write_biobackend_start,
			lbn, on, n, uio, bcount, direct_append);
		/*
	         * Issue a READ if B_CACHE is not set.  In special-append
	         * mode, B_CACHE is based on the buffer prior to the write
	         * op and is typically set, avoiding the read.  If a read
	         * is required in special append mode, the server will
	         * probably send us a short-read since we extended the file
	         * on our end, resulting in b_resid == 0 and, thusly,
	         * B_CACHE getting set.
	         *
	         * We can also avoid issuing the read if the write covers
	         * the entire buffer.  We have to make sure the buffer state
	         * is reasonable in this case since we will not be initiating
	         * I/O.  See the comments in kern/vfs_bio.c's getblk() for
	         * more information.
	         *
	         * B_CACHE may also be set due to the buffer being cached
	         * normally.
	         */

		if (on == 0 && n == bcount) {
			bp->b_flags |= B_CACHE;
			bp->b_flags &= ~B_INVAL;
			bp->b_ioflags &= ~BIO_ERROR;
		}
		if ((bp->b_flags & B_CACHE) == 0) {
			bp->b_iocmd = BIO_READ;
			vfs_busy_pages(bp, 0);
			fuse_io_strategy(vp, bp);
			if ((err = bp->b_error)) {
				brelse(bp);
				break;
			}
			if (bp->b_resid > 0) {
				/* 
				 * Short read indicates EOF.  Update file size
				 * from the server and try again.
				 */
				SDT_PROBE2(fusefs, , io, trace, 1,
					"Short read during a RMW");
				brelse(bp);
				err = fuse_vnode_size(vp, &filesize, cred,
				    curthread);
				if (err)
					break;
				else
					goto again;
			}
		}
		if (bp->b_wcred == NOCRED)
			bp->b_wcred = crhold(cred);

		/*
	         * If dirtyend exceeds file size, chop it down.  This should
	         * not normally occur but there is an append race where it
	         * might occur XXX, so we log it.
	         *
	         * If the chopping creates a reverse-indexed or degenerate
	         * situation with dirtyoff/end, we 0 both of them.
	         */
		if (bp->b_dirtyend > bcount) {
			SDT_PROBE2(fusefs, , io, write_biobackend_append_race,
			    (long)bp->b_blkno * biosize,
			    bp->b_dirtyend - bcount);
			bp->b_dirtyend = bcount;
		}
		if (bp->b_dirtyoff >= bp->b_dirtyend)
			bp->b_dirtyoff = bp->b_dirtyend = 0;

		/*
	         * If the new write will leave a contiguous dirty
	         * area, just update the b_dirtyoff and b_dirtyend,
	         * otherwise force a write rpc of the old dirty area.
	         *
	         * While it is possible to merge discontiguous writes due to
	         * our having a B_CACHE buffer ( and thus valid read data
	         * for the hole), we don't because it could lead to
	         * significant cache coherency problems with multiple clients,
	         * especially if locking is implemented later on.
	         *
	         * as an optimization we could theoretically maintain
	         * a linked list of discontinuous areas, but we would still
	         * have to commit them separately so there isn't much
	         * advantage to it except perhaps a bit of asynchronization.
	         */

		if (bp->b_dirtyend > 0 &&
		    (on > bp->b_dirtyend || (on + n) < bp->b_dirtyoff)) {
			/*
	                 * Yes, we mean it. Write out everything to "storage"
	                 * immediately, without hesitation. (Apart from other
	                 * reasons: the only way to know if a write is valid
	                 * if its actually written out.)
	                 */
			SDT_PROBE2(fusefs, , io, write_biobackend_issue, 0, bp);
			bwrite(bp);
			if (bp->b_error == EINTR) {
				err = EINTR;
				break;
			}
			goto again;
		}
		err = uiomove((char *)bp->b_data + on, n, uio);

		if (err) {
			bp->b_ioflags |= BIO_ERROR;
			bp->b_error = err;
			brelse(bp);
			break;
			/* TODO: vfs_bio_clrbuf like ffs_write does? */
		}
		/*
	         * Only update dirtyoff/dirtyend if not a degenerate
	         * condition.
	         */
		if (n) {
			if (bp->b_dirtyend > 0) {
				bp->b_dirtyoff = MIN(on, bp->b_dirtyoff);
				bp->b_dirtyend = MAX((on + n), bp->b_dirtyend);
			} else {
				bp->b_dirtyoff = on;
				bp->b_dirtyend = on + n;
			}
			vfs_bio_set_valid(bp, on, n);
		}

		vfs_bio_set_flags(bp, ioflag);

		bp->b_flags |= B_FUSEFS_WRITE_CACHE;
		if (ioflag & IO_SYNC) {
			SDT_PROBE2(fusefs, , io, write_biobackend_issue, 2, bp);
			if (!(ioflag & IO_VMIO))
				bp->b_flags &= ~B_FUSEFS_WRITE_CACHE;
			err = bwrite(bp);
		} else if (vm_page_count_severe() ||
			    buf_dirty_count_severe() ||
			    (ioflag & IO_ASYNC)) {
			bp->b_flags |= B_CLUSTEROK;
			SDT_PROBE2(fusefs, , io, write_biobackend_issue, 3, bp);
			bawrite(bp);
		} else if (on == 0 && n == bcount) {
			if ((vp->v_mount->mnt_flag & MNT_NOCLUSTERW) == 0) {
				bp->b_flags |= B_CLUSTEROK;
				SDT_PROBE2(fusefs, , io, write_biobackend_issue,
					4, bp);
				cluster_write(vp, bp, filesize, seqcount, 0);
			} else {
				SDT_PROBE2(fusefs, , io, write_biobackend_issue,
					5, bp);
				bawrite(bp);
			}
		} else if (ioflag & IO_DIRECT) {
			bp->b_flags |= B_CLUSTEROK;
			SDT_PROBE2(fusefs, , io, write_biobackend_issue, 6, bp);
			bawrite(bp);
		} else {
			bp->b_flags &= ~B_CLUSTEROK;
			SDT_PROBE2(fusefs, , io, write_biobackend_issue, 7, bp);
			bdwrite(bp);
		}
		if (err)
			break;
	} while (uio->uio_resid > 0 && n > 0);

	return (err);
}

int
fuse_io_strategy(struct vnode *vp, struct buf *bp)
{
	struct fuse_vnode_data *fvdat = VTOFUD(vp);
	struct fuse_filehandle *fufh;
	struct ucred *cred;
	struct uio *uiop;
	struct uio uio;
	struct iovec io;
	off_t filesize;
	int error = 0;
	int fflag;
	/* We don't know the true pid when we're dealing with the cache */
	pid_t pid = 0;

	const int biosize = fuse_iosize(vp);

	MPASS(vp->v_type == VREG || vp->v_type == VDIR);
	MPASS(bp->b_iocmd == BIO_READ || bp->b_iocmd == BIO_WRITE);

	fflag = bp->b_iocmd == BIO_READ ? FREAD : FWRITE;
	cred = bp->b_iocmd == BIO_READ ? bp->b_rcred : bp->b_wcred;
	error = fuse_filehandle_getrw(vp, fflag, &fufh, cred, pid);
	if (bp->b_iocmd == BIO_READ && error == EBADF) {
		/* 
		 * This may be a read-modify-write operation on a cached file
		 * opened O_WRONLY.  The FUSE protocol allows this.
		 */
		error = fuse_filehandle_get(vp, FWRITE, &fufh, cred, pid);
	}
	if (error) {
		printf("FUSE: strategy: filehandles are closed\n");
		bp->b_ioflags |= BIO_ERROR;
		bp->b_error = error;
		bufdone(bp);
		return (error);
	}

	uiop = &uio;
	uiop->uio_iov = &io;
	uiop->uio_iovcnt = 1;
	uiop->uio_segflg = UIO_SYSSPACE;
	uiop->uio_td = curthread;

	/*
         * clear BIO_ERROR and B_INVAL state prior to initiating the I/O.  We
         * do this here so we do not have to do it in all the code that
         * calls us.
         */
	bp->b_flags &= ~B_INVAL;
	bp->b_ioflags &= ~BIO_ERROR;

	KASSERT(!(bp->b_flags & B_DONE),
	    ("fuse_io_strategy: bp %p already marked done", bp));
	if (bp->b_iocmd == BIO_READ) {
		ssize_t left;

		io.iov_len = uiop->uio_resid = bp->b_bcount;
		io.iov_base = bp->b_data;
		uiop->uio_rw = UIO_READ;

		uiop->uio_offset = ((off_t)bp->b_lblkno) * biosize;
		error = fuse_read_directbackend(vp, uiop, cred, fufh);
		/* 
		 * Store the amount we failed to read in the buffer's private
		 * field, so callers can truncate the file if necessary'
		 */

		if (!error && uiop->uio_resid) {
			int nread = bp->b_bcount - uiop->uio_resid;
			left = uiop->uio_resid;
			bzero((char *)bp->b_data + nread, left);

			if ((fvdat->flag & FN_SIZECHANGE) == 0) {
				/*
				 * A short read with no error, when not using
				 * direct io, and when no writes are cached,
				 * indicates EOF caused by a server-side
				 * truncation.  Clear the attr cache so we'll
				 * pick up the new file size and timestamps.
				 *
				 * We must still bzero the remaining buffer so
				 * uninitialized data doesn't get exposed by a
				 * future truncate that extends the file.
				 * 
				 * To prevent lock order problems, we must
				 * truncate the file upstack, not here.
				 */
				SDT_PROBE2(fusefs, , io, trace, 1,
					"Short read of a clean file");
				fuse_vnode_clear_attr_cache(vp);
			} else {
				/*
				 * If dirty writes _are_ cached beyond EOF,
				 * that indicates a newly created hole that the
				 * server doesn't know about.  Those don't pose
				 * any problem.
				 * XXX: we don't currently track whether dirty
				 * writes are cached beyond EOF, before EOF, or
				 * both.
				 */
				SDT_PROBE2(fusefs, , io, trace, 1,
					"Short read of a dirty file");
				uiop->uio_resid = 0;
			}
		}
		if (error) {
			bp->b_ioflags |= BIO_ERROR;
			bp->b_error = error;
		}
	} else {
		/*
	         * Setup for actual write
	         */
		error = fuse_vnode_size(vp, &filesize, cred, curthread);
		if (error) {
			bp->b_ioflags |= BIO_ERROR;
			bp->b_error = error;
			bufdone(bp);
			return (error);
		}

		if ((off_t)bp->b_lblkno * biosize + bp->b_dirtyend > filesize)
			bp->b_dirtyend = filesize - 
				(off_t)bp->b_lblkno * biosize;

		if (bp->b_dirtyend > bp->b_dirtyoff) {
			io.iov_len = uiop->uio_resid = bp->b_dirtyend
			    - bp->b_dirtyoff;
			uiop->uio_offset = (off_t)bp->b_lblkno * biosize
			    + bp->b_dirtyoff;
			io.iov_base = (char *)bp->b_data + bp->b_dirtyoff;
			uiop->uio_rw = UIO_WRITE;

			bool pages = bp->b_flags & B_FUSEFS_WRITE_CACHE;
			error = fuse_write_directbackend(vp, uiop, cred, fufh,
				filesize, 0, pages);

			if (error == EINTR || error == ETIMEDOUT) {
				bp->b_flags &= ~(B_INVAL | B_NOCACHE);
				if ((bp->b_flags & B_PAGING) == 0) {
					bdirty(bp);
					bp->b_flags &= ~B_DONE;
				}
				if ((error == EINTR || error == ETIMEDOUT) &&
				    (bp->b_flags & B_ASYNC) == 0)
					bp->b_flags |= B_EINTR;
			} else {
				if (error) {
					bp->b_ioflags |= BIO_ERROR;
					bp->b_flags |= B_INVAL;
					bp->b_error = error;
				}
				bp->b_dirtyoff = bp->b_dirtyend = 0;
			}
		} else {
			bp->b_resid = 0;
			bufdone(bp);
			return (0);
		}
	}
	bp->b_resid = uiop->uio_resid;
	bufdone(bp);
	return (error);
}

int
fuse_io_flushbuf(struct vnode *vp, int waitfor, struct thread *td)
{

	return (vn_fsync_buf(vp, waitfor));
}

/*
 * Flush and invalidate all dirty buffers. If another process is already
 * doing the flush, just wait for completion.
 */
int
fuse_io_invalbuf(struct vnode *vp, struct thread *td)
{
	struct fuse_vnode_data *fvdat = VTOFUD(vp);
	int error = 0;

	if (VN_IS_DOOMED(vp))
		return 0;

	ASSERT_VOP_ELOCKED(vp, "fuse_io_invalbuf");

	while (fvdat->flag & FN_FLUSHINPROG) {
		struct proc *p = td->td_proc;

		if (vp->v_mount->mnt_kern_flag & MNTK_UNMOUNTF)
			return EIO;
		fvdat->flag |= FN_FLUSHWANT;
		tsleep(&fvdat->flag, PRIBIO + 2, "fusevinv", 2 * hz);
		error = 0;
		if (p != NULL) {
			PROC_LOCK(p);
			if (SIGNOTEMPTY(p->p_siglist) ||
			    SIGNOTEMPTY(td->td_siglist))
				error = EINTR;
			PROC_UNLOCK(p);
		}
		if (error == EINTR)
			return EINTR;
	}
	fvdat->flag |= FN_FLUSHINPROG;

	if (vp->v_bufobj.bo_object != NULL) {
		VM_OBJECT_WLOCK(vp->v_bufobj.bo_object);
		vm_object_page_clean(vp->v_bufobj.bo_object, 0, 0, OBJPC_SYNC);
		VM_OBJECT_WUNLOCK(vp->v_bufobj.bo_object);
	}
	error = vinvalbuf(vp, V_SAVE, PCATCH, 0);
	while (error) {
		if (error == ERESTART || error == EINTR) {
			fvdat->flag &= ~FN_FLUSHINPROG;
			if (fvdat->flag & FN_FLUSHWANT) {
				fvdat->flag &= ~FN_FLUSHWANT;
				wakeup(&fvdat->flag);
			}
			return EINTR;
		}
		error = vinvalbuf(vp, V_SAVE, PCATCH, 0);
	}
	fvdat->flag &= ~FN_FLUSHINPROG;
	if (fvdat->flag & FN_FLUSHWANT) {
		fvdat->flag &= ~FN_FLUSHWANT;
		wakeup(&fvdat->flag);
	}
	return (error);
}
