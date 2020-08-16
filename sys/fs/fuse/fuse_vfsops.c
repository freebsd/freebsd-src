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
#include <sys/buf.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/capsicum.h>
#include <sys/conf.h>
#include <sys/filedesc.h>
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

#include "fuse.h"
#include "fuse_node.h"
#include "fuse_ipc.h"
#include "fuse_internal.h"

#include <sys/priv.h>
#include <security/mac/mac_framework.h>

SDT_PROVIDER_DECLARE(fusefs);
/* 
 * Fuse trace probe:
 * arg0: verbosity.  Higher numbers give more verbose messages
 * arg1: Textual message
 */
SDT_PROBE_DEFINE2(fusefs, , vfsops, trace, "int", "char*");

/* This will do for privilege types for now */
#ifndef PRIV_VFS_FUSE_ALLOWOTHER
#define PRIV_VFS_FUSE_ALLOWOTHER PRIV_VFS_MOUNT_NONUSER
#endif
#ifndef PRIV_VFS_FUSE_MOUNT_NONUSER
#define PRIV_VFS_FUSE_MOUNT_NONUSER PRIV_VFS_MOUNT_NONUSER
#endif
#ifndef PRIV_VFS_FUSE_SYNC_UNMOUNT
#define PRIV_VFS_FUSE_SYNC_UNMOUNT PRIV_VFS_MOUNT_NONUSER
#endif

static vfs_fhtovp_t fuse_vfsop_fhtovp;
static vfs_mount_t fuse_vfsop_mount;
static vfs_unmount_t fuse_vfsop_unmount;
static vfs_root_t fuse_vfsop_root;
static vfs_statfs_t fuse_vfsop_statfs;
static vfs_vget_t fuse_vfsop_vget;

struct vfsops fuse_vfsops = {
	.vfs_fhtovp = fuse_vfsop_fhtovp,
	.vfs_mount = fuse_vfsop_mount,
	.vfs_unmount = fuse_vfsop_unmount,
	.vfs_root = fuse_vfsop_root,
	.vfs_statfs = fuse_vfsop_statfs,
	.vfs_vget = fuse_vfsop_vget,
};

static int fuse_enforce_dev_perms = 0;

SYSCTL_INT(_vfs_fusefs, OID_AUTO, enforce_dev_perms, CTLFLAG_RW,
    &fuse_enforce_dev_perms, 0,
    "enforce fuse device permissions for secondary mounts");

MALLOC_DEFINE(M_FUSEVFS, "fuse_filesystem", "buffer for fuse vfs layer");

static int
fuse_getdevice(const char *fspec, struct thread *td, struct cdev **fdevp)
{
	struct nameidata nd, *ndp = &nd;
	struct vnode *devvp;
	struct cdev *fdev;
	int err;

	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible disk device.
	 */

	NDINIT(ndp, LOOKUP, FOLLOW, UIO_SYSSPACE, fspec, td);
	if ((err = namei(ndp)) != 0)
		return err;
	NDFREE(ndp, NDF_ONLY_PNBUF);
	devvp = ndp->ni_vp;

	if (devvp->v_type != VCHR) {
		vrele(devvp);
		return ENXIO;
	}
	fdev = devvp->v_rdev;
	dev_ref(fdev);

	if (fuse_enforce_dev_perms) {
		/*
	         * Check if mounter can open the fuse device.
	         *
	         * This has significance only if we are doing a secondary mount
	         * which doesn't involve actually opening fuse devices, but we
	         * still want to enforce the permissions of the device (in
	         * order to keep control over the circle of fuse users).
	         *
	         * (In case of primary mounts, we are either the superuser so
	         * we can do anything anyway, or we can mount only if the
	         * device is already opened by us, ie. we are permitted to open
	         * the device.)
	         */
#if 0
#ifdef MAC
		err = mac_check_vnode_open(td->td_ucred, devvp, VREAD | VWRITE);
		if (!err)
#endif
#endif /* 0 */
			err = VOP_ACCESS(devvp, VREAD | VWRITE, td->td_ucred, td);
		if (err) {
			vrele(devvp);
			dev_rel(fdev);
			return err;
		}
	}
	/*
	 * according to coda code, no extra lock is needed --
	 * although in sys/vnode.h this field is marked "v"
	 */
	vrele(devvp);

	if (!fdev->si_devsw ||
	    strcmp("fuse", fdev->si_devsw->d_name)) {
		dev_rel(fdev);
		return ENXIO;
	}
	*fdevp = fdev;

	return 0;
}

#define FUSE_FLAGOPT(fnam, fval) do {				\
	vfs_flagopt(opts, #fnam, &mntopts, fval);		\
	vfs_flagopt(opts, "__" #fnam, &__mntopts, fval);	\
} while (0)

SDT_PROBE_DEFINE1(fusefs, , vfsops, mntopts, "uint64_t");
SDT_PROBE_DEFINE4(fusefs, , vfsops, mount_err, "char*", "struct fuse_data*",
	"struct mount*", "int");

static int
fuse_vfs_remount(struct mount *mp, struct thread *td, uint64_t mntopts,
	uint32_t max_read, int daemon_timeout)
{
	int err = 0;
	struct fuse_data *data = fuse_get_mpdata(mp);
	/* Don't allow these options to be changed */
	const static unsigned long long cant_update_opts = 
		MNT_USER;	/* Mount owner must be the user running the daemon */

	FUSE_LOCK();

	if ((mp->mnt_flag ^ data->mnt_flag) & cant_update_opts) {
		err = EOPNOTSUPP;
		SDT_PROBE4(fusefs, , vfsops, mount_err,
			"Can't change these mount options during remount",
			data, mp, err);
		goto out;
	}
	if (((data->dataflags ^ mntopts) & FSESS_MNTOPTS_MASK) ||
	     (data->max_read != max_read) ||
	     (data->daemon_timeout != daemon_timeout)) {
		// TODO: allow changing options where it makes sense
		err = EOPNOTSUPP;
		SDT_PROBE4(fusefs, , vfsops, mount_err,
			"Can't change fuse mount options during remount",
			data, mp, err);
		goto out;
	}

	if (fdata_get_dead(data)) {
		err = ENOTCONN;
		SDT_PROBE4(fusefs, , vfsops, mount_err,
			"device is dead during mount", data, mp, err);
		goto out;
	}

	/* Sanity + permission checks */
	if (!data->daemoncred)
		panic("fuse daemon found, but identity unknown");
	if (mntopts & FSESS_DAEMON_CAN_SPY)
		err = priv_check(td, PRIV_VFS_FUSE_ALLOWOTHER);
	if (err == 0 && td->td_ucred->cr_uid != data->daemoncred->cr_uid)
		/* are we allowed to do the first mount? */
		err = priv_check(td, PRIV_VFS_FUSE_MOUNT_NONUSER);

out:
	FUSE_UNLOCK();
	return err;
}

static int
fuse_vfsop_fhtovp(struct mount *mp, struct fid *fhp, int flags,
	struct vnode **vpp)
{
	struct fuse_fid *ffhp = (struct fuse_fid *)fhp;
	struct fuse_vnode_data *fvdat;
	struct vnode *nvp;
	int error;

	if (!(fuse_get_mpdata(mp)->dataflags & FSESS_EXPORT_SUPPORT))
		return EOPNOTSUPP;

	error = VFS_VGET(mp, ffhp->nid, LK_EXCLUSIVE, &nvp);
	if (error) {
		*vpp = NULLVP;
		return (error);
	}
	fvdat = VTOFUD(nvp);
	if (fvdat->generation != ffhp->gen ) {
		vput(nvp);
		*vpp = NULLVP;
		return (ESTALE);
	}
	*vpp = nvp;
	vnode_create_vobject(*vpp, 0, curthread);
	return (0);
}

static int
fuse_vfsop_mount(struct mount *mp)
{
	int err;

	uint64_t mntopts, __mntopts;
	uint32_t max_read;
	int daemon_timeout;
	int fd;

	struct cdev *fdev;
	struct fuse_data *data = NULL;
	struct thread *td;
	struct file *fp, *fptmp;
	char *fspec, *subtype;
	struct vfsoptlist *opts;

	subtype = NULL;
	max_read = ~0;
	err = 0;
	mntopts = 0;
	__mntopts = 0;
	td = curthread;

	/* Get the new options passed to mount */
	opts = mp->mnt_optnew;

	if (!opts)
		return EINVAL;

	/* `fspath' contains the mount point (eg. /mnt/fuse/sshfs); REQUIRED */
	if (!vfs_getopts(opts, "fspath", &err))
		return err;

	/*
	 * With the help of underscored options the mount program
	 * can inform us from the flags it sets by default
	 */
	FUSE_FLAGOPT(allow_other, FSESS_DAEMON_CAN_SPY);
	FUSE_FLAGOPT(push_symlinks_in, FSESS_PUSH_SYMLINKS_IN);
	FUSE_FLAGOPT(default_permissions, FSESS_DEFAULT_PERMISSIONS);
	FUSE_FLAGOPT(intr, FSESS_INTR);

	(void)vfs_scanopt(opts, "max_read=", "%u", &max_read);
	if (vfs_scanopt(opts, "timeout=", "%u", &daemon_timeout) == 1) {
		if (daemon_timeout < FUSE_MIN_DAEMON_TIMEOUT)
			daemon_timeout = FUSE_MIN_DAEMON_TIMEOUT;
		else if (daemon_timeout > FUSE_MAX_DAEMON_TIMEOUT)
			daemon_timeout = FUSE_MAX_DAEMON_TIMEOUT;
	} else {
		daemon_timeout = FUSE_DEFAULT_DAEMON_TIMEOUT;
	}
	subtype = vfs_getopts(opts, "subtype=", &err);

	SDT_PROBE1(fusefs, , vfsops, mntopts, mntopts);

	if (mp->mnt_flag & MNT_UPDATE) {
		return fuse_vfs_remount(mp, td, mntopts, max_read,
			daemon_timeout);
	}

	/* `from' contains the device name (eg. /dev/fuse0); REQUIRED */
	fspec = vfs_getopts(opts, "from", &err);
	if (!fspec)
		return err;

	/* `fd' contains the filedescriptor for this session; REQUIRED */
	if (vfs_scanopt(opts, "fd", "%d", &fd) != 1)
		return EINVAL;

	err = fuse_getdevice(fspec, td, &fdev);
	if (err != 0)
		return err;

	err = fget(td, fd, &cap_read_rights, &fp);
	if (err != 0) {
		SDT_PROBE2(fusefs, , vfsops, trace, 1,
			"invalid or not opened device");
		goto out;
	}
	fptmp = td->td_fpop;
	td->td_fpop = fp;
	err = devfs_get_cdevpriv((void **)&data);
	td->td_fpop = fptmp;
	fdrop(fp, td);
	FUSE_LOCK();

	if (err != 0 || data == NULL) {
		err = ENXIO;
		SDT_PROBE4(fusefs, , vfsops, mount_err,
			"invalid or not opened device", data, mp, err);
		FUSE_UNLOCK();
		goto out;
	}
	if (fdata_get_dead(data)) {
		err = ENOTCONN;
		SDT_PROBE4(fusefs, , vfsops, mount_err,
			"device is dead during mount", data, mp, err);
		FUSE_UNLOCK();
		goto out;
	}
	/* Sanity + permission checks */
	if (!data->daemoncred)
		panic("fuse daemon found, but identity unknown");
	if (mntopts & FSESS_DAEMON_CAN_SPY)
		err = priv_check(td, PRIV_VFS_FUSE_ALLOWOTHER);
	if (err == 0 && td->td_ucred->cr_uid != data->daemoncred->cr_uid)
		/* are we allowed to do the first mount? */
		err = priv_check(td, PRIV_VFS_FUSE_MOUNT_NONUSER);
	if (err) {
		FUSE_UNLOCK();
		goto out;
	}
	data->ref++;
	data->mp = mp;
	data->dataflags |= mntopts;
	data->max_read = max_read;
	data->daemon_timeout = daemon_timeout;
	data->mnt_flag = mp->mnt_flag & MNT_UPDATEMASK;
	FUSE_UNLOCK();

	vfs_getnewfsid(mp);
	MNT_ILOCK(mp);
	mp->mnt_data = data;
	/* 
	 * FUSE file systems can be either local or remote, but the kernel
	 * can't tell the difference.
	 */
	mp->mnt_flag &= ~MNT_LOCAL;
	mp->mnt_kern_flag |= MNTK_USES_BCACHE;
	/* 
	 * Disable nullfs cacheing because it can consume too many resources in
	 * the FUSE server.
	 */
	mp->mnt_kern_flag |= MNTK_NULL_NOCACHE;
	MNT_IUNLOCK(mp);
	/* We need this here as this slot is used by getnewvnode() */
	mp->mnt_stat.f_iosize = maxbcachebuf;
	if (subtype) {
		strlcat(mp->mnt_stat.f_fstypename, ".", MFSNAMELEN);
		strlcat(mp->mnt_stat.f_fstypename, subtype, MFSNAMELEN);
	}
	memset(mp->mnt_stat.f_mntfromname, 0, MNAMELEN);
	strlcpy(mp->mnt_stat.f_mntfromname, fspec, MNAMELEN);
	mp->mnt_iosize_max = MAXPHYS;

	/* Now handshaking with daemon */
	fuse_internal_send_init(data, td);

out:
	if (err) {
		FUSE_LOCK();
		if (data != NULL && data->mp == mp) {
			/*
			 * Destroy device only if we acquired reference to
			 * it
			 */
			SDT_PROBE4(fusefs, , vfsops, mount_err,
				"mount failed, destroy device", data, mp, err);
			data->mp = NULL;
			mp->mnt_data = NULL;
			fdata_trydestroy(data);
		}
		FUSE_UNLOCK();
		dev_rel(fdev);
	}
	return err;
}

static int
fuse_vfsop_unmount(struct mount *mp, int mntflags)
{
	int err = 0;
	int flags = 0;

	struct cdev *fdev;
	struct fuse_data *data;
	struct fuse_dispatcher fdi;
	struct thread *td = curthread;

	if (mntflags & MNT_FORCE) {
		flags |= FORCECLOSE;
	}
	data = fuse_get_mpdata(mp);
	if (!data) {
		panic("no private data for mount point?");
	}
	/* There is 1 extra root vnode reference (mp->mnt_data). */
	FUSE_LOCK();
	if (data->vroot != NULL) {
		struct vnode *vroot = data->vroot;

		data->vroot = NULL;
		FUSE_UNLOCK();
		vrele(vroot);
	} else
		FUSE_UNLOCK();
	err = vflush(mp, 0, flags, td);
	if (err) {
		return err;
	}
	if (fdata_get_dead(data)) {
		goto alreadydead;
	}
	if (fsess_isimpl(mp, FUSE_DESTROY)) {
		fdisp_init(&fdi, 0);
		fdisp_make(&fdi, FUSE_DESTROY, mp, 0, td, NULL);

		(void)fdisp_wait_answ(&fdi);
		fdisp_destroy(&fdi);
	}

	fdata_set_dead(data);

alreadydead:
	FUSE_LOCK();
	data->mp = NULL;
	fdev = data->fdev;
	fdata_trydestroy(data);
	FUSE_UNLOCK();

	MNT_ILOCK(mp);
	mp->mnt_data = NULL;
	MNT_IUNLOCK(mp);

	dev_rel(fdev);

	return 0;
}

SDT_PROBE_DEFINE1(fusefs, , vfsops, invalidate_without_export,
	"struct mount*");
static int
fuse_vfsop_vget(struct mount *mp, ino_t ino, int flags, struct vnode **vpp)
{
	struct fuse_data *data = fuse_get_mpdata(mp);
	uint64_t nodeid = ino;
	struct thread *td = curthread;
	struct fuse_dispatcher fdi;
	struct fuse_entry_out *feo;
	struct fuse_vnode_data *fvdat;
	const char dot[] = ".";
	off_t filesize;
	enum vtype vtyp;
	int error;

	if (!(data->dataflags & FSESS_EXPORT_SUPPORT)) {
		/*
		 * Unreachable unless you do something stupid, like export a
		 * nullfs mount of a fusefs file system.
		 */
		SDT_PROBE1(fusefs, , vfsops, invalidate_without_export, mp);
		return (EOPNOTSUPP);
	}

	error = fuse_internal_get_cached_vnode(mp, ino, flags, vpp);
	if (error || *vpp != NULL)
		return error;

	/* Do a LOOKUP, using nodeid as the parent and "." as filename */
	fdisp_init(&fdi, sizeof(dot));
	fdisp_make(&fdi, FUSE_LOOKUP, mp, nodeid, td, td->td_ucred);
	memcpy(fdi.indata, dot, sizeof(dot));
	error = fdisp_wait_answ(&fdi);

	if (error)
		return error;

	feo = (struct fuse_entry_out *)fdi.answ;
	if (feo->nodeid == 0) {
		/* zero nodeid means ENOENT and cache it */
		error = ENOENT;
		goto out;
	}

	vtyp = IFTOVT(feo->attr.mode);
	error = fuse_vnode_get(mp, feo, nodeid, NULL, vpp, NULL, vtyp);
	if (error)
		goto out;
	filesize = feo->attr.size;

	/*
	 * In the case where we are looking up a FUSE node represented by an
	 * existing cached vnode, and the true size reported by FUSE_LOOKUP
	 * doesn't match the vnode's cached size, then any cached writes beyond
	 * the file's current size are lost.
	 *
	 * We can get here:
	 * * following attribute cache expiration, or
	 * * due a bug in the daemon, or
	 */
	fvdat = VTOFUD(*vpp);
	if (vnode_isreg(*vpp) &&
	    filesize != fvdat->cached_attrs.va_size &&
	    fvdat->flag & FN_SIZECHANGE) {
		printf("%s: WB cache incoherent on %s!\n", __func__,
		    vnode_mount(*vpp)->mnt_stat.f_mntonname);

		fvdat->flag &= ~FN_SIZECHANGE;
	}

	fuse_internal_cache_attrs(*vpp, &feo->attr, feo->attr_valid,
		feo->attr_valid_nsec, NULL);
	fuse_validity_2_bintime(feo->entry_valid, feo->entry_valid_nsec,
		&fvdat->entry_cache_timeout);
out:
	fdisp_destroy(&fdi);
	return error;
}

static int
fuse_vfsop_root(struct mount *mp, int lkflags, struct vnode **vpp)
{
	struct fuse_data *data = fuse_get_mpdata(mp);
	int err = 0;

	if (data->vroot != NULL) {
		err = vget(data->vroot, lkflags);
		if (err == 0)
			*vpp = data->vroot;
	} else {
		err = fuse_vnode_get(mp, NULL, FUSE_ROOT_ID, NULL, vpp, NULL,
		    VDIR);
		if (err == 0) {
			FUSE_LOCK();
			MPASS(data->vroot == NULL || data->vroot == *vpp);
			if (data->vroot == NULL) {
				SDT_PROBE2(fusefs, , vfsops, trace, 1,
					"new root vnode");
				data->vroot = *vpp;
				FUSE_UNLOCK();
				vref(*vpp);
			} else if (data->vroot != *vpp) {
				SDT_PROBE2(fusefs, , vfsops, trace, 1,
					"root vnode race");
				FUSE_UNLOCK();
				VOP_UNLOCK(*vpp);
				vrele(*vpp);
				vrecycle(*vpp);
				*vpp = data->vroot;
			} else
				FUSE_UNLOCK();
		}
	}
	return err;
}

static int
fuse_vfsop_statfs(struct mount *mp, struct statfs *sbp)
{
	struct fuse_dispatcher fdi;
	int err = 0;

	struct fuse_statfs_out *fsfo;
	struct fuse_data *data;

	data = fuse_get_mpdata(mp);

	if (!(data->dataflags & FSESS_INITED))
		goto fake;

	fdisp_init(&fdi, 0);
	fdisp_make(&fdi, FUSE_STATFS, mp, FUSE_ROOT_ID, NULL, NULL);
	err = fdisp_wait_answ(&fdi);
	if (err) {
		fdisp_destroy(&fdi);
		if (err == ENOTCONN) {
			/*
	                 * We want to seem a legitimate fs even if the daemon
	                 * is stiff dead... (so that, eg., we can still do path
	                 * based unmounting after the daemon dies).
	                 */
			goto fake;
		}
		return err;
	}
	fsfo = fdi.answ;

	sbp->f_blocks = fsfo->st.blocks;
	sbp->f_bfree = fsfo->st.bfree;
	sbp->f_bavail = fsfo->st.bavail;
	sbp->f_files = fsfo->st.files;
	sbp->f_ffree = fsfo->st.ffree;	/* cast from uint64_t to int64_t */
	sbp->f_namemax = fsfo->st.namelen;
	sbp->f_bsize = fsfo->st.frsize;	/* cast from uint32_t to uint64_t */

	fdisp_destroy(&fdi);
	return 0;

fake:
	sbp->f_blocks = 0;
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = 0;
	sbp->f_ffree = 0;
	sbp->f_namemax = 0;
	sbp->f_bsize = S_BLKSIZE;

	return 0;
}
