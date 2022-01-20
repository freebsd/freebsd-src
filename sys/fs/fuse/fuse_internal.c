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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/module.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sdt.h>
#include <sys/sx.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <sys/filedesc.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/dirent.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/sysctl.h>
#include <sys/priv.h>

#include "fuse.h"
#include "fuse_file.h"
#include "fuse_internal.h"
#include "fuse_io.h"
#include "fuse_ipc.h"
#include "fuse_node.h"
#include "fuse_file.h"

SDT_PROVIDER_DECLARE(fusefs);
/* 
 * Fuse trace probe:
 * arg0: verbosity.  Higher numbers give more verbose messages
 * arg1: Textual message
 */
SDT_PROBE_DEFINE2(fusefs, , internal, trace, "int", "char*");

#ifdef ZERO_PAD_INCOMPLETE_BUFS
static int isbzero(void *buf, size_t len);

#endif

counter_u64_t fuse_lookup_cache_hits;
counter_u64_t fuse_lookup_cache_misses;

SYSCTL_COUNTER_U64(_vfs_fusefs_stats, OID_AUTO, lookup_cache_hits, CTLFLAG_RD,
    &fuse_lookup_cache_hits, "number of positive cache hits in lookup");

SYSCTL_COUNTER_U64(_vfs_fusefs_stats, OID_AUTO, lookup_cache_misses, CTLFLAG_RD,
    &fuse_lookup_cache_misses, "number of cache misses in lookup");

int
fuse_internal_get_cached_vnode(struct mount* mp, ino_t ino, int flags,
	struct vnode **vpp)
{
	struct bintime now;
	struct thread *td = curthread;
	uint64_t nodeid = ino;
	int error;

	*vpp = NULL;

	error = vfs_hash_get(mp, fuse_vnode_hash(nodeid), flags, td, vpp,
	    fuse_vnode_cmp, &nodeid);
	if (error)
		return error;
	/*
	 * Check the entry cache timeout.  We have to do this within fusefs
	 * instead of by using cache_enter_time/cache_lookup because those
	 * routines are only intended to work with pathnames, not inodes
	 */
	if (*vpp != NULL) {
		getbinuptime(&now);
		if (bintime_cmp(&(VTOFUD(*vpp)->entry_cache_timeout), &now, >)){
			counter_u64_add(fuse_lookup_cache_hits, 1);
			return 0;
		} else {
			/* Entry cache timeout */
			counter_u64_add(fuse_lookup_cache_misses, 1);
			cache_purge(*vpp);
			vput(*vpp);
			*vpp = NULL;
		}
	}
	return 0;
}

SDT_PROBE_DEFINE0(fusefs, , internal, access_vadmin);
/* Synchronously send a FUSE_ACCESS operation */
int
fuse_internal_access(struct vnode *vp,
    accmode_t mode,
    struct thread *td,
    struct ucred *cred)
{
	int err = 0;
	uint32_t mask = F_OK;
	int dataflags;
	struct mount *mp;
	struct fuse_dispatcher fdi;
	struct fuse_access_in *fai;
	struct fuse_data *data;

	mp = vnode_mount(vp);

	data = fuse_get_mpdata(mp);
	dataflags = data->dataflags;

	if (mode == 0)
		return 0;

	if (mode & VMODIFY_PERMS && vfs_isrdonly(mp)) {
		switch (vp->v_type) {
		case VDIR:
			/* FALLTHROUGH */
		case VLNK:
			/* FALLTHROUGH */
		case VREG:
			return EROFS;
		default:
			break;
		}
	}

	/* Unless explicitly permitted, deny everyone except the fs owner. */
	if (!(dataflags & FSESS_DAEMON_CAN_SPY)) {
		if (fuse_match_cred(data->daemoncred, cred))
			return EPERM;
	}

	if (dataflags & FSESS_DEFAULT_PERMISSIONS) {
		struct vattr va;

		fuse_internal_getattr(vp, &va, cred, td);
		return vaccess(vp->v_type, va.va_mode, va.va_uid,
		    va.va_gid, mode, cred);
	}

	if (mode & VADMIN) {
		/*
		 * The FUSE protocol doesn't have an equivalent of VADMIN, so
		 * it's a bug if we ever reach this point with that bit set.
		 */
		SDT_PROBE0(fusefs, , internal, access_vadmin);
	}

	if (fsess_not_impl(mp, FUSE_ACCESS))
		return 0;

	if ((mode & (VWRITE | VAPPEND)) != 0)
		mask |= W_OK;
	if ((mode & VREAD) != 0)
		mask |= R_OK;
	if ((mode & VEXEC) != 0)
		mask |= X_OK;

	fdisp_init(&fdi, sizeof(*fai));
	fdisp_make_vp(&fdi, FUSE_ACCESS, vp, td, cred);

	fai = fdi.indata;
	fai->mask = mask;

	err = fdisp_wait_answ(&fdi);
	fdisp_destroy(&fdi);

	if (err == ENOSYS) {
		fsess_set_notimpl(mp, FUSE_ACCESS);
		err = 0;
	}
	return err;
}

/*
 * Cache FUSE attributes from attr, in attribute cache associated with vnode
 * 'vp'.  Optionally, if argument 'vap' is not NULL, store a copy of the
 * converted attributes there as well.
 *
 * If the nominal attribute cache TTL is zero, do not cache on the 'vp' (but do
 * return the result to the caller).
 */
void
fuse_internal_cache_attrs(struct vnode *vp, struct fuse_attr *attr,
	uint64_t attr_valid, uint32_t attr_valid_nsec, struct vattr *vap,
	bool from_server)
{
	struct mount *mp;
	struct fuse_vnode_data *fvdat;
	struct fuse_data *data;
	struct vattr *vp_cache_at;

	mp = vnode_mount(vp);
	fvdat = VTOFUD(vp);
	data = fuse_get_mpdata(mp);

	ASSERT_VOP_ELOCKED(vp, "fuse_internal_cache_attrs");

	fuse_validity_2_bintime(attr_valid, attr_valid_nsec,
		&fvdat->attr_cache_timeout);

	if (vnode_isreg(vp) &&
	    fvdat->cached_attrs.va_size != VNOVAL &&
	    attr->size != fvdat->cached_attrs.va_size)
	{
		if ( data->cache_mode == FUSE_CACHE_WB &&
		    fvdat->flag & FN_SIZECHANGE)
		{
			const char *msg;

			/*
			 * The server changed the file's size even though we're
			 * using writeback cacheing and and we have outstanding
			 * dirty writes!  That's a server bug.
			 */
			if (fuse_libabi_geq(data, 7, 23)) {
				msg = "writeback cache incoherent!."
				    "To prevent data corruption, disable "
				    "the writeback cache according to your "
				    "FUSE server's documentation.";
			} else {
				msg = "writeback cache incoherent!."
				    "To prevent data corruption, disable "
				    "the writeback cache by setting "
				    "vfs.fusefs.data_cache_mode to 0 or 1.";
			}
			fuse_warn(data, FSESS_WARN_WB_CACHE_INCOHERENT, msg);
		}
		if (fuse_vnode_attr_cache_valid(vp) &&
		    data->cache_mode != FUSE_CACHE_UC)
		{
			/*
			 * The server changed the file's size even though we
			 * have it cached and our cache has not yet expired.
			 * That's a bug.
			 */
			fuse_warn(data, FSESS_WARN_CACHE_INCOHERENT,
			    "cache incoherent!  "
			    "To prevent "
			    "data corruption, disable the data cache "
			    "by mounting with -o direct_io, or as "
			    "directed otherwise by your FUSE server's "
			    "documentation.");
		}
	}

	/* Fix our buffers if the filesize changed without us knowing */
	if (vnode_isreg(vp) && attr->size != fvdat->cached_attrs.va_size) {
		(void)fuse_vnode_setsize(vp, attr->size, from_server);
		fvdat->cached_attrs.va_size = attr->size;
	}

	if (attr_valid > 0 || attr_valid_nsec > 0)
		vp_cache_at = &(fvdat->cached_attrs);
	else if (vap != NULL)
		vp_cache_at = vap;
	else
		return;

	vattr_null(vp_cache_at);
	vp_cache_at->va_fsid = mp->mnt_stat.f_fsid.val[0];
	vp_cache_at->va_fileid = attr->ino;
	vp_cache_at->va_mode = attr->mode & ~S_IFMT;
	vp_cache_at->va_nlink     = attr->nlink;
	vp_cache_at->va_uid       = attr->uid;
	vp_cache_at->va_gid       = attr->gid;
	vp_cache_at->va_rdev      = attr->rdev;
	vp_cache_at->va_size      = attr->size;
	/* XXX on i386, seconds are truncated to 32 bits */
	vp_cache_at->va_atime.tv_sec  = attr->atime;
	vp_cache_at->va_atime.tv_nsec = attr->atimensec;
	vp_cache_at->va_mtime.tv_sec  = attr->mtime;
	vp_cache_at->va_mtime.tv_nsec = attr->mtimensec;
	vp_cache_at->va_ctime.tv_sec  = attr->ctime;
	vp_cache_at->va_ctime.tv_nsec = attr->ctimensec;
	if (fuse_libabi_geq(data, 7, 9) && attr->blksize > 0)
		vp_cache_at->va_blocksize = attr->blksize;
	else
		vp_cache_at->va_blocksize = PAGE_SIZE;
	vp_cache_at->va_type = IFTOVT(attr->mode);
	vp_cache_at->va_bytes = attr->blocks * S_BLKSIZE;
	vp_cache_at->va_flags = 0;

	if (vap != vp_cache_at && vap != NULL)
		memcpy(vap, vp_cache_at, sizeof(*vap));
}

/* fsync */

int
fuse_internal_fsync_callback(struct fuse_ticket *tick, struct uio *uio)
{
	if (tick->tk_aw_ohead.error == ENOSYS) {
		fsess_set_notimpl(tick->tk_data->mp, fticket_opcode(tick));
	}
	return 0;
}

int
fuse_internal_fsync(struct vnode *vp,
    struct thread *td,
    int waitfor,
    bool datasync)
{
	struct fuse_fsync_in *ffsi = NULL;
	struct fuse_dispatcher fdi;
	struct fuse_filehandle *fufh;
	struct fuse_vnode_data *fvdat = VTOFUD(vp);
	struct mount *mp = vnode_mount(vp);
	int op = FUSE_FSYNC;
	int err = 0;

	if (fsess_not_impl(vnode_mount(vp),
	    (vnode_vtype(vp) == VDIR ? FUSE_FSYNCDIR : FUSE_FSYNC))) {
		return 0;
	}
	if (vnode_isdir(vp))
		op = FUSE_FSYNCDIR;

	if (fsess_not_impl(mp, op))
		return 0;

	fdisp_init(&fdi, sizeof(*ffsi));
	/*
	 * fsync every open file handle for this file, because we can't be sure
	 * which file handle the caller is really referring to.
	 */
	LIST_FOREACH(fufh, &fvdat->handles, next) {
		fdi.iosize = sizeof(*ffsi);
		if (ffsi == NULL)
			fdisp_make_vp(&fdi, op, vp, td, NULL);
		else
			fdisp_refresh_vp(&fdi, op, vp, td, NULL);
		ffsi = fdi.indata;
		ffsi->fh = fufh->fh_id;
		ffsi->fsync_flags = 0;

		if (datasync)
			ffsi->fsync_flags = FUSE_FSYNC_FDATASYNC;

		if (waitfor == MNT_WAIT) {
			err = fdisp_wait_answ(&fdi);
		} else {
			fuse_insert_callback(fdi.tick,
				fuse_internal_fsync_callback);
			fuse_insert_message(fdi.tick, false);
		}
		if (err == ENOSYS) {
			/* ENOSYS means "success, and don't call again" */
			fsess_set_notimpl(mp, op);
			err = 0;
			break;
		}
	}
	fdisp_destroy(&fdi);

	return err;
}

/* Asynchronous invalidation */
SDT_PROBE_DEFINE3(fusefs, , internal, invalidate_entry,
	"struct vnode*", "struct fuse_notify_inval_entry_out*", "char*");
int
fuse_internal_invalidate_entry(struct mount *mp, struct uio *uio)
{
	struct fuse_notify_inval_entry_out fnieo;
	struct componentname cn;
	struct vnode *dvp, *vp;
	char name[PATH_MAX];
	int err;

	if ((err = uiomove(&fnieo, sizeof(fnieo), uio)) != 0)
		return (err);

	if (fnieo.namelen >= sizeof(name))
		return (EINVAL);

	if ((err = uiomove(name, fnieo.namelen, uio)) != 0)
		return (err);
	name[fnieo.namelen] = '\0';
	/* fusefs does not cache "." or ".." entries */
	if (strncmp(name, ".", sizeof(".")) == 0 ||
	    strncmp(name, "..", sizeof("..")) == 0)
		return (0);

	if (fnieo.parent == FUSE_ROOT_ID)
		err = VFS_ROOT(mp, LK_SHARED, &dvp);
	else
		err = fuse_internal_get_cached_vnode( mp, fnieo.parent,
			LK_SHARED, &dvp);
	SDT_PROBE3(fusefs, , internal, invalidate_entry, dvp, &fnieo, name);
	/* 
	 * If dvp is not in the cache, then it must've been reclaimed.  And
	 * since fuse_vnop_reclaim does a cache_purge, name's entry must've
	 * been invalidated already.  So we can safely return if dvp == NULL
	 */
	if (err != 0 || dvp == NULL)
		return (err);
	/*
	 * XXX we can't check dvp's generation because the FUSE invalidate
	 * entry message doesn't include it.  Worse case is that we invalidate
	 * an entry that didn't need to be invalidated.
	 */

	cn.cn_nameiop = LOOKUP;
	cn.cn_flags = 0;	/* !MAKEENTRY means free cached entry */
	cn.cn_cred = curthread->td_ucred;
	cn.cn_lkflags = LK_SHARED;
	cn.cn_pnbuf = NULL;
	cn.cn_nameptr = name;
	cn.cn_namelen = fnieo.namelen;
	err = cache_lookup(dvp, &vp, &cn, NULL, NULL);
	MPASS(err == 0);
	fuse_vnode_clear_attr_cache(dvp);
	vput(dvp);
	return (0);
}

SDT_PROBE_DEFINE2(fusefs, , internal, invalidate_inode,
	"struct vnode*", "struct fuse_notify_inval_inode_out *");
int
fuse_internal_invalidate_inode(struct mount *mp, struct uio *uio)
{
	struct fuse_notify_inval_inode_out fniio;
	struct vnode *vp;
	int err;

	if ((err = uiomove(&fniio, sizeof(fniio), uio)) != 0)
		return (err);

	if (fniio.ino == FUSE_ROOT_ID)
		err = VFS_ROOT(mp, LK_EXCLUSIVE, &vp);
	else
		err = fuse_internal_get_cached_vnode(mp, fniio.ino, LK_SHARED,
			&vp);
	SDT_PROBE2(fusefs, , internal, invalidate_inode, vp, &fniio);
	if (err != 0 || vp == NULL)
		return (err);
	/*
	 * XXX we can't check vp's generation because the FUSE invalidate
	 * entry message doesn't include it.  Worse case is that we invalidate
	 * an inode that didn't need to be invalidated.
	 */

	/* 
	 * Flush and invalidate buffers if off >= 0.  Technically we only need
	 * to flush and invalidate the range of offsets [off, off + len), but
	 * for simplicity's sake we do everything.
	 */
	if (fniio.off >= 0)
		fuse_io_invalbuf(vp, curthread);
	fuse_vnode_clear_attr_cache(vp);
	vput(vp);
	return (0);
}

/* mknod */
int
fuse_internal_mknod(struct vnode *dvp, struct vnode **vpp,
	struct componentname *cnp, struct vattr *vap)
{
	struct fuse_data *data;
	struct fuse_mknod_in fmni;
	size_t insize;

	data = fuse_get_mpdata(dvp->v_mount);

	fmni.mode = MAKEIMODE(vap->va_type, vap->va_mode);
	fmni.rdev = vap->va_rdev;
	if (fuse_libabi_geq(data, 7, 12)) {
		insize = sizeof(fmni);
		fmni.umask = curthread->td_proc->p_pd->pd_cmask;
	} else {
		insize = FUSE_COMPAT_MKNOD_IN_SIZE;
	}
	return (fuse_internal_newentry(dvp, vpp, cnp, FUSE_MKNOD, &fmni,
	    insize, vap->va_type));
}

/* readdir */

int
fuse_internal_readdir(struct vnode *vp,
    struct uio *uio,
    off_t startoff,
    struct fuse_filehandle *fufh,
    struct fuse_iov *cookediov,
    int *ncookies,
    uint64_t *cookies)
{
	int err = 0;
	struct fuse_dispatcher fdi;
	struct fuse_read_in *fri = NULL;
	int fnd_start;

	if (uio_resid(uio) == 0)
		return 0;
	fdisp_init(&fdi, 0);

	/*
	 * Note that we DO NOT have a UIO_SYSSPACE here (so no need for p2p
	 * I/O).
	 */

	/*
	 * fnd_start is set non-zero once the offset in the directory gets
	 * to the startoff.  This is done because directories must be read
	 * from the beginning (offset == 0) when fuse_vnop_readdir() needs
	 * to do an open of the directory.
	 * If it is not set non-zero here, it will be set non-zero in
	 * fuse_internal_readdir_processdata() when uio_offset == startoff.
	 */
	fnd_start = 0;
	if (uio->uio_offset == startoff)
		fnd_start = 1;
	while (uio_resid(uio) > 0) {
		fdi.iosize = sizeof(*fri);
		if (fri == NULL)
			fdisp_make_vp(&fdi, FUSE_READDIR, vp, NULL, NULL);
		else
			fdisp_refresh_vp(&fdi, FUSE_READDIR, vp, NULL, NULL);

		fri = fdi.indata;
		fri->fh = fufh->fh_id;
		fri->offset = uio_offset(uio);
		fri->size = MIN(uio->uio_resid,
		    fuse_get_mpdata(vp->v_mount)->max_read);

		if ((err = fdisp_wait_answ(&fdi)))
			break;
		if ((err = fuse_internal_readdir_processdata(uio, startoff,
		    &fnd_start, fri->size, fdi.answ, fdi.iosize, cookediov,
		    ncookies, &cookies)))
			break;
	}

	fdisp_destroy(&fdi);
	return ((err == -1) ? 0 : err);
}

/*
 * Return -1 to indicate that this readdir is finished, 0 if it copied
 * all the directory data read in and it may be possible to read more
 * and greater than 0 for a failure.
 */
int
fuse_internal_readdir_processdata(struct uio *uio,
    off_t startoff,
    int *fnd_start,
    size_t reqsize,
    void *buf,
    size_t bufsize,
    struct fuse_iov *cookediov,
    int *ncookies,
    uint64_t **cookiesp)
{
	int err = 0;
	int oreclen;
	size_t freclen;

	struct dirent *de;
	struct fuse_dirent *fudge;
	uint64_t *cookies;

	cookies = *cookiesp;
	if (bufsize < FUSE_NAME_OFFSET)
		return -1;
	for (;;) {
		if (bufsize < FUSE_NAME_OFFSET) {
			err = -1;
			break;
		}
		fudge = (struct fuse_dirent *)buf;
		freclen = FUSE_DIRENT_SIZE(fudge);

		if (bufsize < freclen) {
			/*
			 * This indicates a partial directory entry at the
			 * end of the directory data.
			 */
			err = -1;
			break;
		}
#ifdef ZERO_PAD_INCOMPLETE_BUFS
		if (isbzero(buf, FUSE_NAME_OFFSET)) {
			err = -1;
			break;
		}
#endif

		if (!fudge->namelen || fudge->namelen > MAXNAMLEN) {
			err = EINVAL;
			break;
		}
		oreclen = GENERIC_DIRSIZ((struct pseudo_dirent *)
					    &fudge->namelen);

		if (oreclen > uio_resid(uio)) {
			/* Out of space for the dir so we are done. */
			err = -1;
			break;
		}
		/*
		 * Don't start to copy the directory entries out until
		 * the requested offset in the directory is found.
		 */
		if (*fnd_start != 0) {
			fiov_adjust(cookediov, oreclen);
			bzero(cookediov->base, oreclen);

			de = (struct dirent *)cookediov->base;
			de->d_fileno = fudge->ino;
			de->d_off = fudge->off;
			de->d_reclen = oreclen;
			de->d_type = fudge->type;
			de->d_namlen = fudge->namelen;
			memcpy((char *)cookediov->base + sizeof(struct dirent) -
			       MAXNAMLEN - 1,
			       (char *)buf + FUSE_NAME_OFFSET, fudge->namelen);
			dirent_terminate(de);

			err = uiomove(cookediov->base, cookediov->len, uio);
			if (err)
				break;
			if (cookies != NULL) {
				if (*ncookies == 0) {
					err = -1;
					break;
				}
				*cookies = fudge->off;
				cookies++;
				(*ncookies)--;
			}
		} else if (startoff == fudge->off)
			*fnd_start = 1;
		buf = (char *)buf + freclen;
		bufsize -= freclen;
		uio_setoffset(uio, fudge->off);
	}
	*cookiesp = cookies;

	return err;
}

/* remove */

int
fuse_internal_remove(struct vnode *dvp,
    struct vnode *vp,
    struct componentname *cnp,
    enum fuse_opcode op)
{
	struct fuse_dispatcher fdi;
	nlink_t nlink;
	int err = 0;

	fdisp_init(&fdi, cnp->cn_namelen + 1);
	fdisp_make_vp(&fdi, op, dvp, curthread, cnp->cn_cred);

	memcpy(fdi.indata, cnp->cn_nameptr, cnp->cn_namelen);
	((char *)fdi.indata)[cnp->cn_namelen] = '\0';

	err = fdisp_wait_answ(&fdi);
	fdisp_destroy(&fdi);

	if (err)
		return (err);

	/* 
	 * Access the cached nlink even if the attr cached has expired.  If
	 * it's inaccurate, the worst that will happen is:
	 * 1) We'll recycle the vnode even though the file has another link we
	 *    don't know about, costing a bit of cpu time, or
	 * 2) We won't recycle the vnode even though all of its links are gone.
	 *    It will linger around until vnlru reclaims it, costing a bit of
	 *    temporary memory.
	 */
	nlink = VTOFUD(vp)->cached_attrs.va_nlink--;

	/* 
	 * Purge the parent's attribute cache because the daemon
	 * should've updated its mtime and ctime.
	 */
	fuse_vnode_clear_attr_cache(dvp);

	/* NB: nlink could be zero if it was never cached */
	if (nlink <= 1 || vnode_vtype(vp) == VDIR) {
		fuse_internal_vnode_disappear(vp);
	} else {
		cache_purge(vp);
		fuse_vnode_update(vp, FN_CTIMECHANGE);
	}

	return err;
}

/* rename */

int
fuse_internal_rename(struct vnode *fdvp,
    struct componentname *fcnp,
    struct vnode *tdvp,
    struct componentname *tcnp)
{
	struct fuse_dispatcher fdi;
	struct fuse_rename_in *fri;
	int err = 0;

	fdisp_init(&fdi, sizeof(*fri) + fcnp->cn_namelen + tcnp->cn_namelen + 2);
	fdisp_make_vp(&fdi, FUSE_RENAME, fdvp, curthread, tcnp->cn_cred);

	fri = fdi.indata;
	fri->newdir = VTOI(tdvp);
	memcpy((char *)fdi.indata + sizeof(*fri), fcnp->cn_nameptr,
	    fcnp->cn_namelen);
	((char *)fdi.indata)[sizeof(*fri) + fcnp->cn_namelen] = '\0';
	memcpy((char *)fdi.indata + sizeof(*fri) + fcnp->cn_namelen + 1,
	    tcnp->cn_nameptr, tcnp->cn_namelen);
	((char *)fdi.indata)[sizeof(*fri) + fcnp->cn_namelen +
	    tcnp->cn_namelen + 1] = '\0';

	err = fdisp_wait_answ(&fdi);
	fdisp_destroy(&fdi);
	return err;
}

/* strategy */

/* entity creation */

void
fuse_internal_newentry_makerequest(struct mount *mp,
    uint64_t dnid,
    struct componentname *cnp,
    enum fuse_opcode op,
    void *buf,
    size_t bufsize,
    struct fuse_dispatcher *fdip)
{
	fdip->iosize = bufsize + cnp->cn_namelen + 1;

	fdisp_make(fdip, op, mp, dnid, curthread, cnp->cn_cred);
	memcpy(fdip->indata, buf, bufsize);
	memcpy((char *)fdip->indata + bufsize, cnp->cn_nameptr, cnp->cn_namelen);
	((char *)fdip->indata)[bufsize + cnp->cn_namelen] = '\0';
}

int
fuse_internal_newentry_core(struct vnode *dvp,
    struct vnode **vpp,
    struct componentname *cnp,
    enum vtype vtyp,
    struct fuse_dispatcher *fdip)
{
	int err = 0;
	struct fuse_entry_out *feo;
	struct mount *mp = vnode_mount(dvp);

	if ((err = fdisp_wait_answ(fdip))) {
		return err;
	}
	feo = fdip->answ;

	if ((err = fuse_internal_checkentry(feo, vtyp))) {
		return err;
	}
	err = fuse_vnode_get(mp, feo, feo->nodeid, dvp, vpp, cnp, vtyp);
	if (err) {
		fuse_internal_forget_send(mp, curthread, cnp->cn_cred,
		    feo->nodeid, 1);
		return err;
	}

	/* 
	 * Purge the parent's attribute cache because the daemon should've
	 * updated its mtime and ctime
	 */
	fuse_vnode_clear_attr_cache(dvp);

	fuse_internal_cache_attrs(*vpp, &feo->attr, feo->attr_valid,
		feo->attr_valid_nsec, NULL, true);

	return err;
}

int
fuse_internal_newentry(struct vnode *dvp,
    struct vnode **vpp,
    struct componentname *cnp,
    enum fuse_opcode op,
    void *buf,
    size_t bufsize,
    enum vtype vtype)
{
	int err;
	struct fuse_dispatcher fdi;
	struct mount *mp = vnode_mount(dvp);

	fdisp_init(&fdi, 0);
	fuse_internal_newentry_makerequest(mp, VTOI(dvp), cnp, op, buf,
	    bufsize, &fdi);
	err = fuse_internal_newentry_core(dvp, vpp, cnp, vtype, &fdi);
	fdisp_destroy(&fdi);

	return err;
}

/* entity destruction */

int
fuse_internal_forget_callback(struct fuse_ticket *ftick, struct uio *uio)
{
	fuse_internal_forget_send(ftick->tk_data->mp, curthread, NULL,
	    ((struct fuse_in_header *)ftick->tk_ms_fiov.base)->nodeid, 1);

	return 0;
}

void
fuse_internal_forget_send(struct mount *mp,
    struct thread *td,
    struct ucred *cred,
    uint64_t nodeid,
    uint64_t nlookup)
{

	struct fuse_dispatcher fdi;
	struct fuse_forget_in *ffi;

	/*
         * KASSERT(nlookup > 0, ("zero-times forget for vp #%llu",
         *         (long long unsigned) nodeid));
         */

	fdisp_init(&fdi, sizeof(*ffi));
	fdisp_make(&fdi, FUSE_FORGET, mp, nodeid, td, cred);

	ffi = fdi.indata;
	ffi->nlookup = nlookup;

	fuse_insert_message(fdi.tick, false);
	fdisp_destroy(&fdi);
}

/* Fetch the vnode's attributes from the daemon*/
int
fuse_internal_do_getattr(struct vnode *vp, struct vattr *vap,
	struct ucred *cred, struct thread *td)
{
	struct fuse_dispatcher fdi;
	struct fuse_vnode_data *fvdat = VTOFUD(vp);
	struct fuse_getattr_in *fgai;
	struct fuse_attr_out *fao;
	off_t old_filesize = fvdat->cached_attrs.va_size;
	struct timespec old_atime = fvdat->cached_attrs.va_atime;
	struct timespec old_ctime = fvdat->cached_attrs.va_ctime;
	struct timespec old_mtime = fvdat->cached_attrs.va_mtime;
	enum vtype vtyp;
	int err;

	ASSERT_VOP_LOCKED(vp, __func__);

	fdisp_init(&fdi, sizeof(*fgai));
	fdisp_make_vp(&fdi, FUSE_GETATTR, vp, td, cred);
	fgai = fdi.indata;
	/* 
	 * We could look up a file handle and set it in fgai->fh, but that
	 * involves extra runtime work and I'm unaware of any file systems that
	 * care.
	 */
	fgai->getattr_flags = 0;
	if ((err = fdisp_wait_answ(&fdi))) {
		if (err == ENOENT)
			fuse_internal_vnode_disappear(vp);
		goto out;
	}

	fao = (struct fuse_attr_out *)fdi.answ;
	vtyp = IFTOVT(fao->attr.mode);
	if (fvdat->flag & FN_SIZECHANGE)
		fao->attr.size = old_filesize;
	if (fvdat->flag & FN_ATIMECHANGE) {
		fao->attr.atime = old_atime.tv_sec;
		fao->attr.atimensec = old_atime.tv_nsec;
	}
	if (fvdat->flag & FN_CTIMECHANGE) {
		fao->attr.ctime = old_ctime.tv_sec;
		fao->attr.ctimensec = old_ctime.tv_nsec;
	}
	if (fvdat->flag & FN_MTIMECHANGE) {
		fao->attr.mtime = old_mtime.tv_sec;
		fao->attr.mtimensec = old_mtime.tv_nsec;
	}
	fuse_internal_cache_attrs(vp, &fao->attr, fao->attr_valid,
		fao->attr_valid_nsec, vap, true);
	if (vtyp != vnode_vtype(vp)) {
		fuse_internal_vnode_disappear(vp);
		err = ENOENT;
	}

out:
	fdisp_destroy(&fdi);
	return err;
}

/* Read a vnode's attributes from cache or fetch them from the fuse daemon */
int
fuse_internal_getattr(struct vnode *vp, struct vattr *vap, struct ucred *cred,
	struct thread *td)
{
	struct vattr *attrs;

	if ((attrs = VTOVA(vp)) != NULL) {
		*vap = *attrs;	/* struct copy */
		return 0;
	}

	return fuse_internal_do_getattr(vp, vap, cred, td);
}

void
fuse_internal_vnode_disappear(struct vnode *vp)
{
	struct fuse_vnode_data *fvdat = VTOFUD(vp);

	ASSERT_VOP_ELOCKED(vp, "fuse_internal_vnode_disappear");
	fvdat->flag |= FN_REVOKED;
	cache_purge(vp);
}

/* fuse start/stop */

SDT_PROBE_DEFINE2(fusefs, , internal, init_done,
	"struct fuse_data*", "struct fuse_init_out*");
int
fuse_internal_init_callback(struct fuse_ticket *tick, struct uio *uio)
{
	int err = 0;
	struct fuse_data *data = tick->tk_data;
	struct fuse_init_out *fiio;

	if ((err = tick->tk_aw_ohead.error)) {
		goto out;
	}
	if ((err = fticket_pull(tick, uio))) {
		goto out;
	}
	fiio = fticket_resp(tick)->base;

	data->fuse_libabi_major = fiio->major;
	data->fuse_libabi_minor = fiio->minor;
	if (!fuse_libabi_geq(data, 7, 4)) {
		/* 
		 * With a little work we could support servers as old as 7.1.
		 * But there would be little payoff.
		 */
		SDT_PROBE2(fusefs, , internal, trace, 1,
			"userpace version too low");
		err = EPROTONOSUPPORT;
		goto out;
	}

	if (fuse_libabi_geq(data, 7, 5)) {
		if (fticket_resp(tick)->len == sizeof(struct fuse_init_out) ||
		    fticket_resp(tick)->len == FUSE_COMPAT_22_INIT_OUT_SIZE) {
			data->max_write = fiio->max_write;
			if (fiio->flags & FUSE_ASYNC_READ)
				data->dataflags |= FSESS_ASYNC_READ;
			if (fiio->flags & FUSE_POSIX_LOCKS)
				data->dataflags |= FSESS_POSIX_LOCKS;
			if (fiio->flags & FUSE_EXPORT_SUPPORT)
				data->dataflags |= FSESS_EXPORT_SUPPORT;
			if (fiio->flags & FUSE_NO_OPEN_SUPPORT)
				data->dataflags |= FSESS_NO_OPEN_SUPPORT;
			if (fiio->flags & FUSE_NO_OPENDIR_SUPPORT)
				data->dataflags |= FSESS_NO_OPENDIR_SUPPORT;
			/* 
			 * Don't bother to check FUSE_BIG_WRITES, because it's
			 * redundant with max_write
			 */
			/* 
			 * max_background and congestion_threshold are not
			 * implemented
			 */
		} else {
			err = EINVAL;
		}
	} else {
		/* Old fixed values */
		data->max_write = 4096;
	}

	if (fuse_libabi_geq(data, 7, 6))
		data->max_readahead_blocks = fiio->max_readahead / maxbcachebuf;

	if (!fuse_libabi_geq(data, 7, 7))
		fsess_set_notimpl(data->mp, FUSE_INTERRUPT);

	if (!fuse_libabi_geq(data, 7, 8)) {
		fsess_set_notimpl(data->mp, FUSE_BMAP);
		fsess_set_notimpl(data->mp, FUSE_DESTROY);
	}

	if (!fuse_libabi_geq(data, 7, 19)) {
		fsess_set_notimpl(data->mp, FUSE_FALLOCATE);
	}

	if (fuse_libabi_geq(data, 7, 23) && fiio->time_gran >= 1 &&
	    fiio->time_gran <= 1000000000)
		data->time_gran = fiio->time_gran;
	else
		data->time_gran = 1;

	if (!fuse_libabi_geq(data, 7, 23))
		data->cache_mode = fuse_data_cache_mode;
	else if (fiio->flags & FUSE_WRITEBACK_CACHE)
		data->cache_mode = FUSE_CACHE_WB;
	else
		data->cache_mode = FUSE_CACHE_WT;

	if (!fuse_libabi_geq(data, 7, 24))
		fsess_set_notimpl(data->mp, FUSE_LSEEK);

	if (!fuse_libabi_geq(data, 7, 28))
		fsess_set_notimpl(data->mp, FUSE_COPY_FILE_RANGE);

out:
	if (err) {
		fdata_set_dead(data);
	}
	FUSE_LOCK();
	data->dataflags |= FSESS_INITED;
	SDT_PROBE2(fusefs, , internal, init_done, data, fiio);
	wakeup(&data->ticketer);
	FUSE_UNLOCK();

	return 0;
}

void
fuse_internal_send_init(struct fuse_data *data, struct thread *td)
{
	struct fuse_init_in *fiii;
	struct fuse_dispatcher fdi;

	fdisp_init(&fdi, sizeof(*fiii));
	fdisp_make(&fdi, FUSE_INIT, data->mp, 0, td, NULL);
	fiii = fdi.indata;
	fiii->major = FUSE_KERNEL_VERSION;
	fiii->minor = FUSE_KERNEL_MINOR_VERSION;
	/* 
	 * fusefs currently reads ahead no more than one cache block at a time.
	 * See fuse_read_biobackend
	 */
	fiii->max_readahead = maxbcachebuf;
	/*
	 * Unsupported features:
	 * FUSE_FILE_OPS: No known FUSE server or client supports it
	 * FUSE_ATOMIC_O_TRUNC: our VFS cannot support it
	 * FUSE_DONT_MASK: unlike Linux, FreeBSD always applies the umask, even
	 *	when default ACLs are in use.
	 * FUSE_SPLICE_WRITE, FUSE_SPLICE_MOVE, FUSE_SPLICE_READ: FreeBSD
	 *	doesn't have splice(2).
	 * FUSE_FLOCK_LOCKS: not yet implemented
	 * FUSE_HAS_IOCTL_DIR: not yet implemented
	 * FUSE_AUTO_INVAL_DATA: not yet implemented
	 * FUSE_DO_READDIRPLUS: not yet implemented
	 * FUSE_READDIRPLUS_AUTO: not yet implemented
	 * FUSE_ASYNC_DIO: not yet implemented
	 * FUSE_PARALLEL_DIROPS: not yet implemented
	 * FUSE_HANDLE_KILLPRIV: not yet implemented
	 * FUSE_POSIX_ACL: not yet implemented
	 * FUSE_ABORT_ERROR: not yet implemented
	 * FUSE_CACHE_SYMLINKS: not yet implemented
	 * FUSE_MAX_PAGES: not yet implemented
	 */
	fiii->flags = FUSE_ASYNC_READ | FUSE_POSIX_LOCKS | FUSE_EXPORT_SUPPORT
		| FUSE_BIG_WRITES | FUSE_WRITEBACK_CACHE
		| FUSE_NO_OPEN_SUPPORT | FUSE_NO_OPENDIR_SUPPORT;

	fuse_insert_callback(fdi.tick, fuse_internal_init_callback);
	fuse_insert_message(fdi.tick, false);
	fdisp_destroy(&fdi);
}

/* 
 * Send a FUSE_SETATTR operation with no permissions checks.  If cred is NULL,
 * send the request with root credentials
 */
int fuse_internal_setattr(struct vnode *vp, struct vattr *vap,
	struct thread *td, struct ucred *cred)
{
	struct fuse_vnode_data *fvdat;
	struct fuse_dispatcher fdi;
	struct fuse_setattr_in *fsai;
	struct mount *mp;
	pid_t pid = td->td_proc->p_pid;
	struct fuse_data *data;
	int err = 0;
	enum vtype vtyp;

	ASSERT_VOP_ELOCKED(vp, __func__);

	mp = vnode_mount(vp);
	fvdat = VTOFUD(vp);
	data = fuse_get_mpdata(mp);

	fdisp_init(&fdi, sizeof(*fsai));
	fdisp_make_vp(&fdi, FUSE_SETATTR, vp, td, cred);
	if (!cred) {
		fdi.finh->uid = 0;
		fdi.finh->gid = 0;
	}
	fsai = fdi.indata;
	fsai->valid = 0;

	if (vap->va_uid != (uid_t)VNOVAL) {
		fsai->uid = vap->va_uid;
		fsai->valid |= FATTR_UID;
	}
	if (vap->va_gid != (gid_t)VNOVAL) {
		fsai->gid = vap->va_gid;
		fsai->valid |= FATTR_GID;
	}
	if (vap->va_size != VNOVAL) {
		struct fuse_filehandle *fufh = NULL;

		/*Truncate to a new value. */
		fsai->size = vap->va_size;
		fsai->valid |= FATTR_SIZE;

		fuse_filehandle_getrw(vp, FWRITE, &fufh, cred, pid);
		if (fufh) {
			fsai->fh = fufh->fh_id;
			fsai->valid |= FATTR_FH;
		}
		VTOFUD(vp)->flag &= ~FN_SIZECHANGE;
	}
	if (vap->va_atime.tv_sec != VNOVAL) {
		fsai->atime = vap->va_atime.tv_sec;
		fsai->atimensec = vap->va_atime.tv_nsec;
		fsai->valid |= FATTR_ATIME;
		if (vap->va_vaflags & VA_UTIMES_NULL)
			fsai->valid |= FATTR_ATIME_NOW;
	} else if (fvdat->flag & FN_ATIMECHANGE) {
		fsai->atime = fvdat->cached_attrs.va_atime.tv_sec;
		fsai->atimensec = fvdat->cached_attrs.va_atime.tv_nsec;
		fsai->valid |= FATTR_ATIME;
	}
	if (vap->va_mtime.tv_sec != VNOVAL) {
		fsai->mtime = vap->va_mtime.tv_sec;
		fsai->mtimensec = vap->va_mtime.tv_nsec;
		fsai->valid |= FATTR_MTIME;
		if (vap->va_vaflags & VA_UTIMES_NULL)
			fsai->valid |= FATTR_MTIME_NOW;
	} else if (fvdat->flag & FN_MTIMECHANGE) {
		fsai->mtime = fvdat->cached_attrs.va_mtime.tv_sec;
		fsai->mtimensec = fvdat->cached_attrs.va_mtime.tv_nsec;
		fsai->valid |= FATTR_MTIME;
	}
	if (fuse_libabi_geq(data, 7, 23) && fvdat->flag & FN_CTIMECHANGE) {
		fsai->ctime = fvdat->cached_attrs.va_ctime.tv_sec;
		fsai->ctimensec = fvdat->cached_attrs.va_ctime.tv_nsec;
		fsai->valid |= FATTR_CTIME;
	}
	if (vap->va_mode != (mode_t)VNOVAL) {
		fsai->mode = vap->va_mode & ALLPERMS;
		fsai->valid |= FATTR_MODE;
	}
	if (!fsai->valid) {
		goto out;
	}

	if ((err = fdisp_wait_answ(&fdi)))
		goto out;
	vtyp = IFTOVT(((struct fuse_attr_out *)fdi.answ)->attr.mode);

	if (vnode_vtype(vp) != vtyp) {
		if (vnode_vtype(vp) == VNON && vtyp != VNON) {
			SDT_PROBE2(fusefs, , internal, trace, 1, "FUSE: Dang! "
				"vnode_vtype is VNON and vtype isn't.");
		} else {
			/*
	                 * STALE vnode, ditch
	                 *
			 * The vnode has changed its type "behind our back".
			 * This probably means that the file got deleted and
			 * recreated on the server, with the same inode.
			 * There's nothing really we can do, so let us just
			 * return ENOENT.  After all, the entry must not have
			 * existed in the recent past.  If the user tries
			 * again, it will work.
	                 */
			fuse_internal_vnode_disappear(vp);
			err = ENOENT;
		}
	}
	if (err == 0) {
		struct fuse_attr_out *fao = (struct fuse_attr_out*)fdi.answ;
		fuse_vnode_undirty_cached_timestamps(vp, true);
		fuse_internal_cache_attrs(vp, &fao->attr, fao->attr_valid,
			fao->attr_valid_nsec, NULL, false);
		getnanouptime(&fvdat->last_local_modify);
	}

out:
	fdisp_destroy(&fdi);
	return err;
}

/*
 * FreeBSD clears the SUID and SGID bits on any write by a non-root user.
 */
void
fuse_internal_clear_suid_on_write(struct vnode *vp, struct ucred *cred,
	struct thread *td)
{
	struct fuse_data *data;
	struct mount *mp;
	struct vattr va;
	int dataflags;

	mp = vnode_mount(vp);
	data = fuse_get_mpdata(mp);
	dataflags = data->dataflags;

	ASSERT_VOP_LOCKED(vp, __func__);

	if (dataflags & FSESS_DEFAULT_PERMISSIONS) {
		if (priv_check_cred(cred, PRIV_VFS_RETAINSUGID)) {
			fuse_internal_getattr(vp, &va, cred, td);
			if (va.va_mode & (S_ISUID | S_ISGID)) {
				mode_t mode = va.va_mode & ~(S_ISUID | S_ISGID);
				/* Clear all vattr fields except mode */
				vattr_null(&va);
				va.va_mode = mode;

				/*
				 * Ignore fuse_internal_setattr's return value,
				 * because at this point the write operation has
				 * already succeeded and we don't want to return
				 * failing status for that.
				 */
				(void)fuse_internal_setattr(vp, &va, td, NULL);
			}
		}
	}
}

#ifdef ZERO_PAD_INCOMPLETE_BUFS
static int
isbzero(void *buf, size_t len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (((char *)buf)[i])
			return (0);
	}

	return (1);
}

#endif

void
fuse_internal_init(void)
{
	fuse_lookup_cache_misses = counter_u64_alloc(M_WAITOK);
	fuse_lookup_cache_hits = counter_u64_alloc(M_WAITOK);
}

void
fuse_internal_destroy(void)
{
	counter_u64_free(fuse_lookup_cache_hits);
	counter_u64_free(fuse_lookup_cache_misses);
}
