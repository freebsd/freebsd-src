/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2007-2009 Google Inc. and Amit Singh
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

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/module.h>
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
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/fcntl.h>
#include <sys/priv.h>
#include <sys/buf.h>
#include <security/mac/mac_framework.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>

#include "fuse.h"
#include "fuse_node.h"
#include "fuse_internal.h"
#include "fuse_io.h"
#include "fuse_ipc.h"

SDT_PROVIDER_DECLARE(fusefs);
/* 
 * Fuse trace probe:
 * arg0: verbosity.  Higher numbers give more verbose messages
 * arg1: Textual message
 */
SDT_PROBE_DEFINE2(fusefs, , node, trace, "int", "char*");

MALLOC_DEFINE(M_FUSEVN, "fuse_vnode", "fuse vnode private data");

static int sysctl_fuse_cache_mode(SYSCTL_HANDLER_ARGS);

static counter_u64_t fuse_node_count;

SYSCTL_COUNTER_U64(_vfs_fusefs_stats, OID_AUTO, node_count, CTLFLAG_RD,
    &fuse_node_count, "Count of FUSE vnodes");

int	fuse_data_cache_mode = FUSE_CACHE_WT;

/*
 * OBSOLETE
 * This sysctl is no longer needed as of fuse protocol 7.23.  Now, individual
 * servers can select the cache behavior they need for each mountpoint:
 * - writethrough: the default
 * - writeback: set FUSE_WRITEBACK_CACHE in fuse_init_out.flags
 * - uncached: set FOPEN_DIRECT_IO for every file
 * The sysctl is retained primarily due to the enduring popularity of libfuse2,
 * which is frozen at protocol version 7.19.  As of 4-April-2024, 90% of
 * FreeBSD ports that use libfuse still bind to libfuse2.
 */
SYSCTL_PROC(_vfs_fusefs, OID_AUTO, data_cache_mode,
    CTLTYPE_INT | CTLFLAG_MPSAFE | CTLFLAG_RW,
    &fuse_data_cache_mode, 0, sysctl_fuse_cache_mode, "I",
    "Zero: disable caching of FUSE file data; One: write-through caching "
    "(default); Two: write-back caching (generally unsafe)");

static int
sysctl_fuse_cache_mode(SYSCTL_HANDLER_ARGS)
{
	int val, error;

	val = *(int *)arg1;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return (error);

	switch (val) {
	case FUSE_CACHE_UC:
	case FUSE_CACHE_WT:
	case FUSE_CACHE_WB:
		*(int *)arg1 = val;
		break;
	default:
		return (EDOM);
	}
	return (0);
}

static void
fuse_vnode_init(struct vnode *vp, struct fuse_vnode_data *fvdat,
    uint64_t nodeid, __enum_uint8(vtype) vtyp)
{
	fvdat->nid = nodeid;
	LIST_INIT(&fvdat->handles);

	vattr_null(&fvdat->cached_attrs);
	fvdat->cached_attrs.va_birthtime.tv_sec = -1;
	fvdat->cached_attrs.va_birthtime.tv_nsec = 0;
	fvdat->cached_attrs.va_fsid = VNOVAL;
	fvdat->cached_attrs.va_gen = 0;
	fvdat->cached_attrs.va_rdev = NODEV;

	if (nodeid == FUSE_ROOT_ID) {
		vp->v_vflag |= VV_ROOT;
	}
	vp->v_type = vtyp;
	vp->v_data = fvdat;
	cluster_init_vn(&fvdat->clusterw);
	timespecclear(&fvdat->last_local_modify);

	counter_u64_add(fuse_node_count, 1);
}

void
fuse_vnode_destroy(struct vnode *vp)
{
	struct fuse_vnode_data *fvdat = vp->v_data;

	vp->v_data = NULL;
	KASSERT(LIST_EMPTY(&fvdat->handles),
		("Destroying fuse vnode with open files!"));
	free(fvdat, M_FUSEVN);

	counter_u64_add(fuse_node_count, -1);
}

int
fuse_vnode_cmp(struct vnode *vp, void *nidp)
{
	return (VTOI(vp) != *((uint64_t *)nidp));
}

SDT_PROBE_DEFINE3(fusefs, , node, stale_vnode, "struct vnode*", "uint8_t",
		"uint64_t");
static int
fuse_vnode_alloc(struct mount *mp,
    struct thread *td,
    uint64_t nodeid,
    __enum_uint8(vtype) vtyp,
    struct vnode **vpp)
{
	struct fuse_data *data;
	struct fuse_vnode_data *fvdat;
	struct vnode *vp2;
	int err = 0;

	data = fuse_get_mpdata(mp);
	if (vtyp == VNON) {
		return EINVAL;
	}
	*vpp = NULL;
	err = vfs_hash_get(mp, fuse_vnode_hash(nodeid), LK_EXCLUSIVE, td, vpp,
	    fuse_vnode_cmp, &nodeid);
	if (err)
		return (err);

	if (*vpp) {
		if ((*vpp)->v_type == vtyp) {
			/* Reuse a vnode that hasn't yet been reclaimed */
			MPASS((*vpp)->v_data != NULL);
			MPASS(VTOFUD(*vpp)->nid == nodeid);
			SDT_PROBE2(fusefs, , node, trace, 1,
				"vnode taken from hash");
			return (0);
		} else {
			/*
			 * The inode changed types!  If we get here, we can't
			 * tell whether the inode's entry cache had expired
			 * yet.  So this could be the result of a buggy server,
			 * but more likely the server just reused an inode
			 * number following an entry cache expiration.
			 */
			SDT_PROBE3(fusefs, , node, stale_vnode, *vpp, vtyp,
				nodeid);
			fuse_internal_vnode_disappear(*vpp);
			vgone(*vpp);
			lockmgr((*vpp)->v_vnlock, LK_RELEASE, NULL);
		}
	}
	fvdat = malloc(sizeof(*fvdat), M_FUSEVN, M_WAITOK | M_ZERO);
	switch (vtyp) {
	case VFIFO:
		err = getnewvnode("fuse", mp, &fuse_fifoops, vpp);
		break;
	default:
		err = getnewvnode("fuse", mp, &fuse_vnops, vpp);
		break;
	}
	if (err) {
		free(fvdat, M_FUSEVN);
		return (err);
	}
	lockmgr((*vpp)->v_vnlock, LK_EXCLUSIVE, NULL);
	fuse_vnode_init(*vpp, fvdat, nodeid, vtyp);
	err = insmntque(*vpp, mp);
	ASSERT_VOP_ELOCKED(*vpp, "fuse_vnode_alloc");
	if (err) {
		lockmgr((*vpp)->v_vnlock, LK_RELEASE, NULL);
		free(fvdat, M_FUSEVN);
		*vpp = NULL;
		return (err);
	}
	/* Disallow async reads for fifos because UFS does.  I don't know why */
	if (data->dataflags & FSESS_ASYNC_READ && vtyp != VFIFO)
		VN_LOCK_ASHARE(*vpp);

	vn_set_state(*vpp, VSTATE_CONSTRUCTED);
	err = vfs_hash_insert(*vpp, fuse_vnode_hash(nodeid), LK_EXCLUSIVE,
	    td, &vp2, fuse_vnode_cmp, &nodeid);
	if (err) {
		lockmgr((*vpp)->v_vnlock, LK_RELEASE, NULL);
		free(fvdat, M_FUSEVN);
		*vpp = NULL;
		return (err);
	}
	if (vp2 != NULL) {
		*vpp = vp2;
		return (0);
	}

	ASSERT_VOP_ELOCKED(*vpp, "fuse_vnode_alloc");

	return (0);
}

int
fuse_vnode_get(struct mount *mp,
    struct fuse_entry_out *feo,
    uint64_t nodeid,
    struct vnode *dvp,
    struct vnode **vpp,
    struct componentname *cnp,
    __enum_uint8(vtype) vtyp)
{
	struct thread *td = curthread;
	bool exportable = fuse_get_mpdata(mp)->dataflags & FSESS_EXPORT_SUPPORT;

	/* 
	 * feo should only be NULL for the root directory, which (when libfuse
	 * is used) always has generation 0
	 */
	uint64_t generation = feo ? feo->generation : 0;
	int err = 0;

	if (dvp != NULL && VTOFUD(dvp)->nid == nodeid) {
		fuse_warn(fuse_get_mpdata(mp), FSESS_WARN_ILLEGAL_INODE,
			"Assigned same inode to both parent and child.");
		return EIO;
	}
	if (feo && feo->nodeid != feo->attr.ino && exportable) {
		/*
		 * NFS servers (both kernelspace and userspace) rely on
		 * VFS_VGET to lookup inodes.  But that's only possible if the
		 * file's inode number matches its nodeid, which isn't
		 * necessarily the case for FUSE.  If they don't match, then we
		 * can complete the current operation, but future VFS_VGET
		 * operations will almost certainly return spurious results.
		 * Warn the operator.
		 *
		 * But only warn the operator if the file system reports
		 * NFS-compatibility, because that's the only time that this
		 * matters, and dumb fuse servers abound.
		 */
		fuse_warn(fuse_get_mpdata(mp), FSESS_WARN_INODE_MISMATCH,
		    "file has different inode number and nodeid.");
	}

	err = fuse_vnode_alloc(mp, td, nodeid, vtyp, vpp);
	if (err) {
		return err;
	}
	if (dvp != NULL) {
		MPASS(cnp && (cnp->cn_flags & ISDOTDOT) == 0);
		MPASS(cnp &&
			!(cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.'));
		fuse_vnode_setparent(*vpp, dvp);
	}
	if (dvp != NULL && cnp != NULL && (cnp->cn_flags & MAKEENTRY) != 0 &&
	    feo != NULL &&
	    (feo->entry_valid != 0 || feo->entry_valid_nsec != 0)) {
		struct timespec timeout;

		ASSERT_VOP_LOCKED(*vpp, "fuse_vnode_get");
		ASSERT_VOP_LOCKED(dvp, "fuse_vnode_get");

		fuse_validity_2_timespec(feo, &timeout);
		cache_enter_time(dvp, *vpp, cnp, &timeout, NULL);
	}

	VTOFUD(*vpp)->generation = generation;
	/*
	 * In userland, libfuse uses cached lookups for dot and dotdot entries,
	 * thus it does not really bump the nlookup counter for forget.
	 * Follow the same semantic and avoid the bump in order to keep
	 * nlookup counters consistent.
	 */
	if (cnp == NULL || ((cnp->cn_flags & ISDOTDOT) == 0 &&
	    (cnp->cn_namelen != 1 || cnp->cn_nameptr[0] != '.')))
		VTOFUD(*vpp)->nlookup++;

	return 0;
}

/*
 * Called for every fusefs vnode open to initialize the vnode (not
 * fuse_filehandle) for use
 */
void
fuse_vnode_open(struct vnode *vp, int32_t fuse_open_flags, struct thread *td)
{
	if (vnode_vtype(vp) == VREG)
		vnode_create_vobject(vp, VNODE_NO_SIZE, td);
}

int
fuse_vnode_savesize(struct vnode *vp, struct ucred *cred, pid_t pid)
{
	struct fuse_vnode_data *fvdat = VTOFUD(vp);
	struct thread *td = curthread;
	struct fuse_filehandle *fufh = NULL;
	struct fuse_dispatcher fdi;
	struct fuse_setattr_in *fsai;
	int err = 0;

	ASSERT_VOP_ELOCKED(vp, "fuse_io_extend");

	if (fuse_isdeadfs(vp)) {
		return EBADF;
	}
	if (vnode_vtype(vp) == VDIR) {
		return EISDIR;
	}
	if (vfs_isrdonly(vnode_mount(vp))) {
		return EROFS;
	}
	if (cred == NULL) {
		cred = td->td_ucred;
	}
	fdisp_init(&fdi, sizeof(*fsai));
	fdisp_make_vp(&fdi, FUSE_SETATTR, vp, td, cred);
	fsai = fdi.indata;
	fsai->valid = 0;

	/* Truncate to a new value. */
	MPASS((fvdat->flag & FN_SIZECHANGE) != 0);
	fsai->size = fvdat->cached_attrs.va_size;
	fsai->valid |= FATTR_SIZE;

	fuse_filehandle_getrw(vp, FWRITE, &fufh, cred, pid);
	if (fufh) {
		fsai->fh = fufh->fh_id;
		fsai->valid |= FATTR_FH;
	}
	err = fdisp_wait_answ(&fdi);
	fdisp_destroy(&fdi);
	if (err == 0) {
		getnanouptime(&fvdat->last_local_modify);
		fvdat->flag &= ~FN_SIZECHANGE;
	}

	return err;
}

/*
 * Adjust the vnode's size to a new value.
 *
 * If the new value came from the server, such as from a FUSE_GETATTR
 * operation, set `from_server` true.  But if it came from a local operation,
 * such as write(2) or truncate(2), set `from_server` false.
 */
int
fuse_vnode_setsize(struct vnode *vp, off_t newsize, bool from_server)
{
	struct fuse_vnode_data *fvdat = VTOFUD(vp);
	struct vattr *attrs;
	off_t oldsize;
	size_t iosize;
	struct buf *bp = NULL;
	int err = 0;

	ASSERT_VOP_ELOCKED(vp, "fuse_vnode_setsize");

	iosize = fuse_iosize(vp);
	oldsize = fvdat->cached_attrs.va_size;
	fvdat->cached_attrs.va_size = newsize;
	if ((attrs = VTOVA(vp)) != NULL)
		attrs->va_size = newsize;

	if (newsize < oldsize) {
		daddr_t lbn;

		err = vtruncbuf(vp, newsize, fuse_iosize(vp));
		if (err)
			goto out;
		if (newsize % iosize == 0)
			goto out;
		/* 
		 * Zero the contents of the last partial block.
		 * Sure seems like vtruncbuf should do this for us.
		 */

		lbn = newsize / iosize;
		bp = getblk(vp, lbn, iosize, PCATCH, 0, 0);
		if (!bp) {
			err = EINTR;
			goto out;
		}
		if (!(bp->b_flags & B_CACHE))
			goto out;	/* Nothing to do */
		MPASS(bp->b_flags & B_VMIO);
		vfs_bio_clrbuf(bp);
		bp->b_dirtyend = MIN(bp->b_dirtyend, newsize - lbn * iosize);
	} else if (from_server && newsize > oldsize && oldsize != VNOVAL) {
		/*
		 * The FUSE server changed the file size behind our back.  We
		 * should invalidate the entire cache.
		 */
		daddr_t end_lbn;

		end_lbn = howmany(newsize, iosize);
		v_inval_buf_range(vp, 0, end_lbn, iosize);
	}
out:
	if (bp)
		brelse(bp);
	vnode_pager_setsize(vp, newsize);
	return err;
}

/* Get the current, possibly dirty, size of the file */
int
fuse_vnode_size(struct vnode *vp, off_t *filesize, struct ucred *cred,
	struct thread *td)
{
	struct fuse_vnode_data *fvdat = VTOFUD(vp);
	int error = 0;

	if (!(fvdat->flag & FN_SIZECHANGE) &&
		(!fuse_vnode_attr_cache_valid(vp) ||
		  fvdat->cached_attrs.va_size == VNOVAL)) 
		error = fuse_internal_do_getattr(vp, NULL, cred, td);

	if (!error)
		*filesize = fvdat->cached_attrs.va_size;

	return error;
}

void
fuse_vnode_undirty_cached_timestamps(struct vnode *vp, bool atime)
{
	struct fuse_vnode_data *fvdat = VTOFUD(vp);

	fvdat->flag &= ~(FN_MTIMECHANGE | FN_CTIMECHANGE);
	if (atime)
		fvdat->flag &= ~FN_ATIMECHANGE;
}

/* Update a fuse file's cached timestamps */
void
fuse_vnode_update(struct vnode *vp, int flags)
{
	struct fuse_vnode_data *fvdat = VTOFUD(vp);
	struct mount *mp = vnode_mount(vp);
	struct fuse_data *data = fuse_get_mpdata(mp);
	struct timespec ts;

	vfs_timestamp(&ts);

	if (data->time_gran > 1)
		ts.tv_nsec = rounddown(ts.tv_nsec, data->time_gran);

	if (mp->mnt_flag & MNT_NOATIME)
		flags &= ~FN_ATIMECHANGE;

	if (flags & FN_ATIMECHANGE)
		fvdat->cached_attrs.va_atime = ts;
	if (flags & FN_MTIMECHANGE)
		fvdat->cached_attrs.va_mtime = ts;
	if (flags & FN_CTIMECHANGE)
		fvdat->cached_attrs.va_ctime = ts;

	fvdat->flag |= flags;
}

void
fuse_node_init(void)
{
	fuse_node_count = counter_u64_alloc(M_WAITOK);
}

void
fuse_node_destroy(void)
{
	counter_u64_free(fuse_node_count);
}
