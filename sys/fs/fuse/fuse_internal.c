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
#include <sys/module.h>
#include <sys/systm.h>
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
	int vtype;
	struct mount *mp;
	struct fuse_dispatcher fdi;
	struct fuse_access_in *fai;
	struct fuse_data *data;

	mp = vnode_mount(vp);
	vtype = vnode_vtype(vp);

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
		    va.va_gid, mode, cred, NULL);
	}

	if (!fsess_isimpl(mp, FUSE_ACCESS))
		return 0;

	if ((mode & (VWRITE | VAPPEND | VADMIN)) != 0)
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
fuse_internal_cache_attrs(struct vnode *vp, struct ucred *cred,
	struct fuse_attr *attr, uint64_t attr_valid, uint32_t attr_valid_nsec,
	struct vattr *vap)
{
	struct mount *mp;
	struct fuse_vnode_data *fvdat;
	struct fuse_data *data;
	struct vattr *vp_cache_at;

	mp = vnode_mount(vp);
	fvdat = VTOFUD(vp);
	data = fuse_get_mpdata(mp);
	if (!cred)
		cred = curthread->td_ucred;

	ASSERT_VOP_ELOCKED(vp, "fuse_internal_cache_attrs");

	fuse_validity_2_bintime(attr_valid, attr_valid_nsec,
		&fvdat->attr_cache_timeout);

	/* Fix our buffers if the filesize changed without us knowing */
	if (vnode_isreg(vp) && attr->size != fvdat->cached_attrs.va_size) {
		(void)fuse_vnode_setsize(vp, cred, attr->size);
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

	if (!fsess_isimpl(vnode_mount(vp),
	    (vnode_vtype(vp) == VDIR ? FUSE_FSYNCDIR : FUSE_FSYNC))) {
		return 0;
	}
	if (vnode_isdir(vp))
		op = FUSE_FSYNCDIR;

	if (!fsess_isimpl(mp, op))
		return 0;

	fdisp_init(&fdi, sizeof(*ffsi));
	/*
	 * fsync every open file handle for this file, because we can't be sure
	 * which file handle the caller is really referring to.
	 */
	LIST_FOREACH(fufh, &fvdat->handles, next) {
		if (ffsi == NULL)
			fdisp_make_vp(&fdi, op, vp, td, NULL);
		else
			fdisp_refresh_vp(&fdi, op, vp, td, NULL);
		ffsi = fdi.indata;
		ffsi->fh = fufh->fh_id;
		ffsi->fsync_flags = 0;

		if (datasync)
			ffsi->fsync_flags = 1;

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

/* mknod */
int
fuse_internal_mknod(struct vnode *dvp, struct vnode **vpp,
	struct componentname *cnp, struct vattr *vap)
{
	struct fuse_mknod_in fmni;

	fmni.mode = MAKEIMODE(vap->va_type, vap->va_mode);
	fmni.rdev = vap->va_rdev;
	return (fuse_internal_newentry(dvp, vpp, cnp, FUSE_MKNOD, &fmni,
	    sizeof(fmni), vap->va_type));
}

/* readdir */

int
fuse_internal_readdir(struct vnode *vp,
    struct uio *uio,
    off_t startoff,
    struct fuse_filehandle *fufh,
    struct fuse_iov *cookediov,
    int *ncookies,
    u_long *cookies)
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
    u_long **cookiesp)
{
	int err = 0;
	int bytesavail;
	size_t freclen;

	struct dirent *de;
	struct fuse_dirent *fudge;
	u_long *cookies;

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
		bytesavail = GENERIC_DIRSIZ((struct pseudo_dirent *)
					    &fudge->namelen);

		if (bytesavail > uio_resid(uio)) {
			/* Out of space for the dir so we are done. */
			err = -1;
			break;
		}
		/*
		 * Don't start to copy the directory entries out until
		 * the requested offset in the directory is found.
		 */
		if (*fnd_start != 0) {
			fiov_adjust(cookediov, bytesavail);
			bzero(cookediov->base, bytesavail);

			de = (struct dirent *)cookediov->base;
			de->d_fileno = fudge->ino;
			de->d_reclen = bytesavail;
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
	int err = 0;

	fdisp_init(&fdi, cnp->cn_namelen + 1);
	fdisp_make_vp(&fdi, op, dvp, cnp->cn_thread, cnp->cn_cred);

	memcpy(fdi.indata, cnp->cn_nameptr, cnp->cn_namelen);
	((char *)fdi.indata)[cnp->cn_namelen] = '\0';

	err = fdisp_wait_answ(&fdi);
	fdisp_destroy(&fdi);
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
	fdisp_make_vp(&fdi, FUSE_RENAME, fdvp, tcnp->cn_thread, tcnp->cn_cred);

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

	fdisp_make(fdip, op, mp, dnid, cnp->cn_thread, cnp->cn_cred);
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
		fuse_internal_forget_send(mp, cnp->cn_thread, cnp->cn_cred,
		    feo->nodeid, 1);
		return err;
	}

	/* 
	 * Purge the parent's attribute cache because the daemon should've
	 * updated its mtime and ctime
	 */
	fuse_vnode_clear_attr_cache(dvp);

	fuse_internal_cache_attrs(*vpp, NULL, &feo->attr, feo->attr_valid,
		feo->attr_valid_nsec, NULL);

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
	struct fuse_attr_out *fao;
	off_t old_filesize = fvdat->cached_attrs.va_size;
	enum vtype vtyp;
	int err;

	fdisp_init(&fdi, 0);
	if ((err = fdisp_simple_putget_vp(&fdi, FUSE_GETATTR, vp, td, cred))) {
		if (err == ENOENT)
			fuse_internal_vnode_disappear(vp);
		goto out;
	}

	fao = (struct fuse_attr_out *)fdi.answ;
	vtyp = IFTOVT(fao->attr.mode);
	if (fvdat->flag & FN_SIZECHANGE)
		fao->attr.size = old_filesize;
	fuse_internal_cache_attrs(vp, NULL, &fao->attr, fao->attr_valid,
		fao->attr_valid_nsec, vap);
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
	bintime_clear(&fvdat->attr_cache_timeout);
	cache_purge(vp);
}

/* fuse start/stop */

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

	/* XXX: Do we want to check anything further besides this? */
	if (fiio->major < 7) {
		SDT_PROBE2(fusefs, , internal, trace, 1,
			"userpace version too low");
		err = EPROTONOSUPPORT;
		goto out;
	}
	data->fuse_libabi_major = fiio->major;
	data->fuse_libabi_minor = fiio->minor;

	if (fuse_libabi_geq(data, 7, 5)) {
		if (fticket_resp(tick)->len == sizeof(struct fuse_init_out)) {
			data->max_write = fiio->max_write;
			if (fiio->flags & FUSE_ASYNC_READ)
				data->dataflags |= FSESS_ASYNC_READ;
			if (fiio->flags & FUSE_POSIX_LOCKS)
				data->dataflags |= FSESS_POSIX_LOCKS;
			if (fiio->flags & FUSE_EXPORT_SUPPORT)
				data->dataflags |= FSESS_EXPORT_SUPPORT;
		} else {
			err = EINVAL;
		}
	} else {
		/* Old fix values */
		data->max_write = 4096;
	}

out:
	if (err) {
		fdata_set_dead(data);
	}
	FUSE_LOCK();
	data->dataflags |= FSESS_INITED;
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
	 * fusefs currently doesn't do any readahead other than fetching whole
	 * buffer cache block sized regions at once.  So the max readahead is
	 * the size of a buffer cache block.
	 */
	fiii->max_readahead = maxbcachebuf;
	fiii->flags = FUSE_ASYNC_READ | FUSE_POSIX_LOCKS | FUSE_EXPORT_SUPPORT ;

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
	struct fuse_dispatcher fdi;
	struct fuse_setattr_in *fsai;
	struct mount *mp;
	pid_t pid = td->td_proc->p_pid;
	struct fuse_data *data;
	int dataflags;
	int err = 0;
	enum vtype vtyp;
	int sizechanged = -1;
	uint64_t newsize = 0;

	mp = vnode_mount(vp);
	data = fuse_get_mpdata(mp);
	dataflags = data->dataflags;

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
		sizechanged = 1;
		newsize = vap->va_size;
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
	}
	if (vap->va_mtime.tv_sec != VNOVAL) {
		fsai->mtime = vap->va_mtime.tv_sec;
		fsai->mtimensec = vap->va_mtime.tv_nsec;
		fsai->valid |= FATTR_MTIME;
		if (vap->va_vaflags & VA_UTIMES_NULL)
			fsai->valid |= FATTR_MTIME_NOW;
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
			 * There's nothing really we can do, so let us just
			 * force an internal revocation and tell the caller to
			 * try again, if interested.
	                 */
			fuse_internal_vnode_disappear(vp);
			err = EAGAIN;
		}
	}
	if (err == 0) {
		struct fuse_attr_out *fao = (struct fuse_attr_out*)fdi.answ;
		fuse_internal_cache_attrs(vp, cred, &fao->attr, fao->attr_valid,
			fao->attr_valid_nsec, NULL);
	}

out:
	fdisp_destroy(&fdi);
	return err;
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
