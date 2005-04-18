/*-
 * Copyright (c) 1999-2004 Poul-Henning Kamp
 * Copyright (c) 1999 Michael Smith
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/conf.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/reboot.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/systm.h>
#include <sys/vnode.h>

#include <geom/geom.h>

#include <machine/stdarg.h>

#include "opt_rootdevname.h"
#include "opt_ddb.h"
#include "opt_mac.h"

#ifdef DDB
#include <ddb/ddb.h>
#endif

#define	ROOTNAME		"root_device"
#define	VFS_MOUNTARG_SIZE_MAX	(1024 * 64)

static int	vfs_domount(struct thread *td, const char *fstype,
		    char *fspath, int fsflags, void *fsdata);
static int	vfs_mount_alloc(struct vnode *dvp, struct vfsconf *vfsp,
		    const char *fspath, struct thread *td, struct mount **mpp);
static int	vfs_mountroot_ask(void);
static int	vfs_mountroot_try(const char *mountfrom);
static int	vfs_donmount(struct thread *td, int fsflags,
		    struct uio *fsoptions);
static void	free_mntarg(struct mntarg *ma);
static void	vfs_mount_destroy(struct mount *, struct thread *);

static int	usermount = 0;
SYSCTL_INT(_vfs, OID_AUTO, usermount, CTLFLAG_RW, &usermount, 0,
    "Unprivileged users may mount and unmount file systems");

MALLOC_DEFINE(M_MOUNT, "mount", "vfs mount structure");

/* List of mounted filesystems. */
struct mntlist mountlist = TAILQ_HEAD_INITIALIZER(mountlist);

/* For any iteration/modification of mountlist */
struct mtx mountlist_mtx;
MTX_SYSINIT(mountlist, &mountlist_mtx, "mountlist", MTX_DEF);

TAILQ_HEAD(vfsoptlist, vfsopt);
struct vfsopt {
	TAILQ_ENTRY(vfsopt) link;
	char	*name;
	void	*value;
	int	len;
};

/*
 * The vnode of the system's root (/ in the filesystem, without chroot
 * active.)
 */
struct vnode	*rootvnode;

/*
 * The root filesystem is detailed in the kernel environment variable
 * vfs.root.mountfrom, which is expected to be in the general format
 *
 * <vfsname>:[<path>]
 * vfsname   := the name of a VFS known to the kernel and capable
 *              of being mounted as root
 * path      := disk device name or other data used by the filesystem
 *              to locate its physical store
 */

/*
 * Global opts, taken by all filesystems
 */
static const char *global_opts[] = {
	"fstype",
	"fspath",
	"ro",
	"suid",
	"exec",
	NULL
};

/*
 * The root specifiers we will try if RB_CDROM is specified.
 */
static char *cdrom_rootdevnames[] = {
	"cd9660:cd0",
	"cd9660:acd0",
	NULL
};

/* legacy find-root code */
char		*rootdevnames[2] = {NULL, NULL};
#ifndef ROOTDEVNAME
#  define ROOTDEVNAME NULL
#endif
static const char	*ctrootdevname = ROOTDEVNAME;

/*
 * ---------------------------------------------------------------------
 * Functions for building and sanitizing the mount options
 */

/* Remove one mount option. */
static void
vfs_freeopt(struct vfsoptlist *opts, struct vfsopt *opt)
{

	TAILQ_REMOVE(opts, opt, link);
	free(opt->name, M_MOUNT);
	if (opt->value != NULL)
		free(opt->value, M_MOUNT);
#ifdef INVARIANTS
	else if (opt->len != 0)
		panic("%s: mount option with NULL value but length != 0",
		    __func__);
#endif
	free(opt, M_MOUNT);
}

/* Release all resources related to the mount options. */
static void
vfs_freeopts(struct vfsoptlist *opts)
{
	struct vfsopt *opt;

	while (!TAILQ_EMPTY(opts)) {
		opt = TAILQ_FIRST(opts);
		vfs_freeopt(opts, opt);
	}
	free(opts, M_MOUNT);
}

/*
 * Check if options are equal (with or without the "no" prefix).
 */
static int
vfs_equalopts(const char *opt1, const char *opt2)
{

	/* "opt" vs. "opt" or "noopt" vs. "noopt" */
	if (strcmp(opt1, opt2) == 0)
		return (1);
	/* "noopt" vs. "opt" */
	if (strncmp(opt1, "no", 2) == 0 && strcmp(opt1 + 2, opt2) == 0)
		return (1);
	/* "opt" vs. "noopt" */
	if (strncmp(opt2, "no", 2) == 0 && strcmp(opt1, opt2 + 2) == 0)
		return (1);
	return (0);
}

/*
 * If a mount option is specified several times,
 * (with or without the "no" prefix) only keep
 * the last occurence of it.
 */
static void
vfs_sanitizeopts(struct vfsoptlist *opts)
{
	struct vfsopt *opt, *opt2, *tmp;

	TAILQ_FOREACH_REVERSE(opt, opts, vfsoptlist, link) {
		opt2 = TAILQ_PREV(opt, vfsoptlist, link);
		while (opt2 != NULL) {
			if (vfs_equalopts(opt->name, opt2->name)) {
				tmp = TAILQ_PREV(opt2, vfsoptlist, link);
				vfs_freeopt(opts, opt2);
				opt2 = tmp;
			} else {
				opt2 = TAILQ_PREV(opt2, vfsoptlist, link);
			}
		}
	}
}

/*
 * Build a linked list of mount options from a struct uio.
 */
static int
vfs_buildopts(struct uio *auio, struct vfsoptlist **options)
{
	struct vfsoptlist *opts;
	struct vfsopt *opt;
	size_t memused;
	unsigned int i, iovcnt;
	int error, namelen, optlen;

	opts = malloc(sizeof(struct vfsoptlist), M_MOUNT, M_WAITOK);
	TAILQ_INIT(opts);
	memused = 0;
	iovcnt = auio->uio_iovcnt;
	for (i = 0; i < iovcnt; i += 2) {
		opt = malloc(sizeof(struct vfsopt), M_MOUNT, M_WAITOK);
		namelen = auio->uio_iov[i].iov_len;
		optlen = auio->uio_iov[i + 1].iov_len;
		opt->name = malloc(namelen, M_MOUNT, M_WAITOK);
		opt->value = NULL;
		opt->len = 0;

		/*
		 * Do this early, so jumps to "bad" will free the current
		 * option.
		 */
		TAILQ_INSERT_TAIL(opts, opt, link);
		memused += sizeof(struct vfsopt) + optlen + namelen;

		/*
		 * Avoid consuming too much memory, and attempts to overflow
		 * memused.
		 */
		if (memused > VFS_MOUNTARG_SIZE_MAX ||
		    optlen > VFS_MOUNTARG_SIZE_MAX ||
		    namelen > VFS_MOUNTARG_SIZE_MAX) {
			error = EINVAL;
			goto bad;
		}

		if (auio->uio_segflg == UIO_SYSSPACE) {
			bcopy(auio->uio_iov[i].iov_base, opt->name, namelen);
		} else {
			error = copyin(auio->uio_iov[i].iov_base, opt->name,
			    namelen);
			if (error)
				goto bad;
		}
		/* Ensure names are null-terminated strings. */
		if (opt->name[namelen - 1] != '\0') {
			error = EINVAL;
			goto bad;
		}
		if (optlen != 0) {
			opt->len = optlen;
			opt->value = malloc(optlen, M_MOUNT, M_WAITOK);
			if (auio->uio_segflg == UIO_SYSSPACE) {
				bcopy(auio->uio_iov[i + 1].iov_base, opt->value,
				    optlen);
			} else {
				error = copyin(auio->uio_iov[i + 1].iov_base,
				    opt->value, optlen);
				if (error)
					goto bad;
			}
		}
	}
	vfs_sanitizeopts(opts);
	*options = opts;
	return (0);
bad:
	vfs_freeopts(opts);
	return (error);
}

/*
 * Merge the old mount options with the new ones passed
 * in the MNT_UPDATE case.
 */
static void
vfs_mergeopts(struct vfsoptlist *toopts, struct vfsoptlist *opts)
{
	struct vfsopt *opt, *opt2, *new;

	TAILQ_FOREACH(opt, opts, link) {
		/*
		 * Check that this option hasn't been redefined
		 * nor cancelled with a "no" mount option.
		 */
		opt2 = TAILQ_FIRST(toopts);
		while (opt2 != NULL) {
			if (strcmp(opt2->name, opt->name) == 0)
				goto next;
			if (strncmp(opt2->name, "no", 2) == 0 &&
			    strcmp(opt2->name + 2, opt->name) == 0) {
				vfs_freeopt(toopts, opt2);
				goto next;
			}
			opt2 = TAILQ_NEXT(opt2, link);
		}
		/* We want this option, duplicate it. */
		new = malloc(sizeof(struct vfsopt), M_MOUNT, M_WAITOK);
		new->name = malloc(strlen(opt->name) + 1, M_MOUNT, M_WAITOK);
		strcpy(new->name, opt->name);
		if (opt->len != 0) {
			new->value = malloc(opt->len, M_MOUNT, M_WAITOK);
			bcopy(opt->value, new->value, opt->len);
		} else {
			new->value = NULL;
		}
		new->len = opt->len;
		TAILQ_INSERT_TAIL(toopts, new, link);
next:
		continue;
	}
}

/*
 * ---------------------------------------------------------------------
 * Mount a filesystem
 */
int
nmount(td, uap)
	struct thread *td;
	struct nmount_args /* {
		struct iovec *iovp;
		unsigned int iovcnt;
		int flags;
	} */ *uap;
{
	struct uio *auio;
	struct iovec *iov;
	unsigned int i;
	int error;
	u_int iovcnt;

	/* Kick out MNT_ROOTFS early as it is legal internally */
	if (uap->flags & MNT_ROOTFS)
		return (EINVAL);

	iovcnt = uap->iovcnt;
	/*
	 * Check that we have an even number of iovec's
	 * and that we have at least two options.
	 */
	if ((iovcnt & 1) || (iovcnt < 4))
		return (EINVAL);

	error = copyinuio(uap->iovp, iovcnt, &auio);
	if (error)
		return (error);
	iov = auio->uio_iov;
	for (i = 0; i < iovcnt; i++) {
		if (iov->iov_len > MMAXOPTIONLEN) {
			free(auio, M_IOV);
			return (EINVAL);
		}
		iov++;
	}
	error = vfs_donmount(td, uap->flags, auio);
	free(auio, M_IOV);
	return (error);
}

/*
 * ---------------------------------------------------------------------
 * Various utility functions
 */

/*
 * Allocate and initialize the mount point struct.
 */
static int
vfs_mount_alloc(struct vnode *vp, struct vfsconf *vfsp,
    const char *fspath, struct thread *td, struct mount **mpp)
{
	struct mount *mp;

	mp = malloc(sizeof(struct mount), M_MOUNT, M_WAITOK | M_ZERO);
	TAILQ_INIT(&mp->mnt_nvnodelist);
	mp->mnt_nvnodelistsize = 0;
	mtx_init(&mp->mnt_mtx, "struct mount mtx", NULL, MTX_DEF);
	lockinit(&mp->mnt_lock, PVFS, "vfslock", 0, 0);
	vfs_busy(mp, LK_NOWAIT, 0, td);
	mp->mnt_op = vfsp->vfc_vfsops;
	mp->mnt_vfc = vfsp;
	vfsp->vfc_refcount++;
	mp->mnt_stat.f_type = vfsp->vfc_typenum;
	mp->mnt_flag |= vfsp->vfc_flags & MNT_VISFLAGMASK;
	strlcpy(mp->mnt_stat.f_fstypename, vfsp->vfc_name, MFSNAMELEN);
	mp->mnt_vnodecovered = vp;
	mp->mnt_cred = crdup(td->td_ucred);
	mp->mnt_stat.f_owner = td->td_ucred->cr_uid;
	strlcpy(mp->mnt_stat.f_mntonname, fspath, MNAMELEN);
	mp->mnt_iosize_max = DFLTPHYS;
#ifdef MAC
	mac_init_mount(mp);
	mac_create_mount(td->td_ucred, mp);
#endif
	arc4rand(&mp->mnt_hashseed, sizeof mp->mnt_hashseed, 0);
	*mpp = mp;
	return (0);
}

/*
 * Destroy the mount struct previously allocated by vfs_mount_alloc().
 */
static void
vfs_mount_destroy(struct mount *mp, struct thread *td)
{

	mp->mnt_vfc->vfc_refcount--;
	if (!TAILQ_EMPTY(&mp->mnt_nvnodelist))
		panic("unmount: dangling vnode");
	vfs_unbusy(mp,td);
	lockdestroy(&mp->mnt_lock);
	MNT_ILOCK(mp);
	if (mp->mnt_kern_flag & MNTK_MWAIT)
		wakeup(mp);
	MNT_IUNLOCK(mp);
	mtx_destroy(&mp->mnt_mtx);
#ifdef MAC
	mac_destroy_mount(mp);
#endif
	if (mp->mnt_opt != NULL)
		vfs_freeopts(mp->mnt_opt);
	crfree(mp->mnt_cred);
	free(mp, M_MOUNT);
}

static int
vfs_donmount(struct thread *td, int fsflags, struct uio *fsoptions)
{
	struct vfsoptlist *optlist;
	char *fstype, *fspath;
	int error, fstypelen, fspathlen;

	error = vfs_buildopts(fsoptions, &optlist);
	if (error)
		return (error);

	/*
	 * We need these two options before the others,
	 * and they are mandatory for any filesystem.
	 * Ensure they are NUL terminated as well.
	 */
	fstypelen = 0;
	error = vfs_getopt(optlist, "fstype", (void **)&fstype, &fstypelen);
	if (error || fstype[fstypelen - 1] != '\0') {
		error = EINVAL;
		goto bail;
	}
	fspathlen = 0;
	error = vfs_getopt(optlist, "fspath", (void **)&fspath, &fspathlen);
	if (error || fspath[fspathlen - 1] != '\0') {
		error = EINVAL;
		goto bail;
	}

	/*
	 * Be ultra-paranoid about making sure the type and fspath
	 * variables will fit in our mp buffers, including the
	 * terminating NUL.
	 */
	if (fstypelen >= MFSNAMELEN - 1 || fspathlen >= MNAMELEN - 1) {
		error = ENAMETOOLONG;
		goto bail;
	}

	mtx_lock(&Giant);
	error = vfs_domount(td, fstype, fspath, fsflags, optlist);
	mtx_unlock(&Giant);
bail:
	if (error)
		vfs_freeopts(optlist);
	return (error);
}

/*
 * ---------------------------------------------------------------------
 * Old mount API.
 */
#ifndef _SYS_SYSPROTO_H_
struct mount_args {
	char	*type;
	char	*path;
	int	flags;
	caddr_t	data;
};
#endif
/* ARGSUSED */
int
mount(td, uap)
	struct thread *td;
	struct mount_args /* {
		char *type;
		char *path;
		int flags;
		caddr_t data;
	} */ *uap;
{
	char *fstype;
	struct vfsconf *vfsp = NULL;
	struct mntarg *ma = NULL;
	int error;

	/* Kick out MNT_ROOTFS early as it is legal internally */
	uap->flags &= ~MNT_ROOTFS;

	if (uap->data == NULL)
		return (EINVAL);

	fstype = malloc(MFSNAMELEN, M_TEMP, M_WAITOK);
	error = copyinstr(uap->type, fstype, MFSNAMELEN, NULL);
	if (!error) {
		mtx_lock(&Giant);	/* XXX ? */
		vfsp = vfs_byname_kld(fstype, td, &error);
		mtx_unlock(&Giant);
	}
	free(fstype, M_TEMP);
	if (error)
		return (error);
	if (vfsp == NULL)
		return (ENOENT);
	if (vfsp->vfc_vfsops->vfs_cmount == NULL)
		return (EOPNOTSUPP);

	ma = mount_argsu(ma, "fstype", uap->type, MNAMELEN);
	ma = mount_argsu(ma, "fspath", uap->path, MNAMELEN);
	ma = mount_argb(ma, uap->flags & MNT_RDONLY, "noro");
	ma = mount_argb(ma, !(uap->flags & MNT_NOSUID), "nosuid");
	ma = mount_argb(ma, !(uap->flags & MNT_NOEXEC), "noexec");

	error = vfsp->vfc_vfsops->vfs_cmount(ma, uap->data, uap->flags, td);
	return (error);
}


/*
 * vfs_domount(): actually attempt a filesystem mount.
 */
static int
vfs_domount(
	struct thread *td,	/* Flags common to all filesystems. */
	const char *fstype,	/* Filesystem type. */
	char *fspath,		/* Mount path. */
	int fsflags,		/* Flags common to all filesystems. */
	void *fsdata		/* Options local to the filesystem. */
	)
{
	struct vnode *vp;
	struct mount *mp;
	struct vfsconf *vfsp;
	int error, flag = 0, kern_flag = 0;
	struct vattr va;
	struct nameidata nd;

	mtx_assert(&Giant, MA_OWNED);

	/*
	 * Be ultra-paranoid about making sure the type and fspath
	 * variables will fit in our mp buffers, including the
	 * terminating NUL.
	 */
	if (strlen(fstype) >= MFSNAMELEN || strlen(fspath) >= MNAMELEN)
		return (ENAMETOOLONG);

	if (jailed(td->td_ucred))
		return (EPERM);
	if (usermount == 0) {
		if ((error = suser(td)) != 0)
			return (error);
	}

	/*
	 * Do not allow NFS export or MNT_SUIDDIR by unprivileged users.
	 */
	if (fsflags & (MNT_EXPORTED | MNT_SUIDDIR)) {
		if ((error = suser(td)) != 0)
			return (error);
	}
	/*
	 * Silently enforce MNT_NOSUID and MNT_USER for
	 * unprivileged users.
	 */
	if (suser(td) != 0)
		fsflags |= MNT_NOSUID | MNT_USER;
	/*
	 * Get vnode to be covered
	 */
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, fspath, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;
	if (fsflags & MNT_UPDATE) {
		if ((vp->v_vflag & VV_ROOT) == 0) {
			vput(vp);
			return (EINVAL);
		}
		mp = vp->v_mount;
		flag = mp->mnt_flag;
		kern_flag = mp->mnt_kern_flag;
		/*
		 * We only allow the filesystem to be reloaded if it
		 * is currently mounted read-only.
		 */
		if ((fsflags & MNT_RELOAD) &&
		    ((mp->mnt_flag & MNT_RDONLY) == 0)) {
			vput(vp);
			return (EOPNOTSUPP);	/* Needs translation */
		}
		/*
		 * Only privileged root, or (if MNT_USER is set) the user that
		 * did the original mount is permitted to update it.
		 */
		error = vfs_suser(mp, td);
		if (error) {
			vput(vp);
			return (error);
		}
		if (vfs_busy(mp, LK_NOWAIT, 0, td)) {
			vput(vp);
			return (EBUSY);
		}
		VI_LOCK(vp);
		if ((vp->v_iflag & VI_MOUNT) != 0 ||
		    vp->v_mountedhere != NULL) {
			VI_UNLOCK(vp);
			vfs_unbusy(mp, td);
			vput(vp);
			return (EBUSY);
		}
		vp->v_iflag |= VI_MOUNT;
		VI_UNLOCK(vp);
		mp->mnt_flag |= fsflags &
		    (MNT_RELOAD | MNT_FORCE | MNT_UPDATE | MNT_SNAPSHOT | MNT_ROOTFS);
		VOP_UNLOCK(vp, 0, td);
		mp->mnt_optnew = fsdata;
		vfs_mergeopts(mp->mnt_optnew, mp->mnt_opt);
	} else {
		/*
		 * If the user is not root, ensure that they own the directory
		 * onto which we are attempting to mount.
		 */
		error = VOP_GETATTR(vp, &va, td->td_ucred, td);
		if (error) {
			vput(vp);
			return (error);
		}
		if (va.va_uid != td->td_ucred->cr_uid) {
			if ((error = suser(td)) != 0) {
				vput(vp);
				return (error);
			}
		}
		error = vinvalbuf(vp, V_SAVE, td, 0, 0);
		if (error != 0) {
			vput(vp);
			return (error);
		}
		if (vp->v_type != VDIR) {
			vput(vp);
			return (ENOTDIR);
		}
		vfsp = vfs_byname_kld(fstype, td, &error);
		if (vfsp == NULL) {
			vput(vp);
			return (error);
		}
		VI_LOCK(vp);
		if ((vp->v_iflag & VI_MOUNT) != 0 ||
		    vp->v_mountedhere != NULL) {
			VI_UNLOCK(vp);
			vput(vp);
			return (EBUSY);
		}
		vp->v_iflag |= VI_MOUNT;
		VI_UNLOCK(vp);

		/*
		 * Allocate and initialize the filesystem.
		 */
		error = vfs_mount_alloc(vp, vfsp, fspath, td, &mp);
		if (error) {
			vput(vp);
			return (error);
		}
		VOP_UNLOCK(vp, 0, td);

		/* XXXMAC: pass to vfs_mount_alloc? */
		mp->mnt_optnew = fsdata;
	}

	/*
	 * Set the mount level flags.
	 */
	if (fsflags & MNT_RDONLY)
		mp->mnt_flag |= MNT_RDONLY;
	mp->mnt_flag &=~ MNT_UPDATEMASK;
	mp->mnt_flag |= fsflags & (MNT_UPDATEMASK | MNT_FORCE | MNT_ROOTFS);
	/*
	 * Mount the filesystem.
	 * XXX The final recipients of VFS_MOUNT just overwrite the ndp they
	 * get.  No freeing of cn_pnbuf.
	 */
        error = VFS_MOUNT(mp, td);
	if (!error) {
		if (mp->mnt_opt != NULL)
			vfs_freeopts(mp->mnt_opt);
		mp->mnt_opt = mp->mnt_optnew;
		VFS_STATFS(mp, &mp->mnt_stat, td);
	}
	/*
	 * Prevent external consumers of mount options from reading
	 * mnt_optnew.
	*/
	mp->mnt_optnew = NULL;
	if (mp->mnt_flag & MNT_UPDATE) {
		mp->mnt_flag &=
		    ~(MNT_UPDATE | MNT_RELOAD | MNT_FORCE | MNT_SNAPSHOT);
		if (error) {
			mp->mnt_flag = flag;
			mp->mnt_kern_flag = kern_flag;
		}
		if ((mp->mnt_flag & MNT_RDONLY) == 0) {
			if (mp->mnt_syncer == NULL)
				error = vfs_allocate_syncvnode(mp);
		} else {
			if (mp->mnt_syncer != NULL)
				vrele(mp->mnt_syncer);
			mp->mnt_syncer = NULL;
		}
		vfs_unbusy(mp, td);
		VI_LOCK(vp);
		vp->v_iflag &= ~VI_MOUNT;
		VI_UNLOCK(vp);
		vrele(vp);
		return (error);
	}
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	/*
	 * Put the new filesystem on the mount list after root.
	 */
	cache_purge(vp);
	if (!error) {
		struct vnode *newdp;

		VI_LOCK(vp);
		vp->v_iflag &= ~VI_MOUNT;
		VI_UNLOCK(vp);
		vp->v_mountedhere = mp;
		mtx_lock(&mountlist_mtx);
		TAILQ_INSERT_TAIL(&mountlist, mp, mnt_list);
		mtx_unlock(&mountlist_mtx);
		vfs_event_signal(NULL, VQ_MOUNT, 0);
		if (VFS_ROOT(mp, LK_EXCLUSIVE, &newdp, td))
			panic("mount: lost mount");
		mountcheckdirs(vp, newdp);
		vput(newdp);
		VOP_UNLOCK(vp, 0, td);
		if ((mp->mnt_flag & MNT_RDONLY) == 0)
			error = vfs_allocate_syncvnode(mp);
		vfs_unbusy(mp, td);
		if (error)
			vrele(vp);
	} else {
		VI_LOCK(vp);
		vp->v_iflag &= ~VI_MOUNT;
		VI_UNLOCK(vp);
		vfs_mount_destroy(mp, td);
		vput(vp);
	}
	return (error);
}

/*
 * ---------------------------------------------------------------------
 * Unmount a filesystem.
 *
 * Note: unmount takes a path to the vnode mounted on as argument,
 * not special file (as before).
 */
#ifndef _SYS_SYSPROTO_H_
struct unmount_args {
	char	*path;
	int	flags;
};
#endif
/* ARGSUSED */
int
unmount(td, uap)
	struct thread *td;
	register struct unmount_args /* {
		char *path;
		int flags;
	} */ *uap;
{
	struct mount *mp;
	char *pathbuf;
	int error, id0, id1;

	if (jailed(td->td_ucred))
		return (EPERM);
	if (usermount == 0) {
		if ((error = suser(td)) != 0)
			return (error);
	}

	pathbuf = malloc(MNAMELEN, M_TEMP, M_WAITOK);
	error = copyinstr(uap->path, pathbuf, MNAMELEN, NULL);
	if (error) {
		free(pathbuf, M_TEMP);
		return (error);
	}
	if (uap->flags & MNT_BYFSID) {
		/* Decode the filesystem ID. */
		if (sscanf(pathbuf, "FSID:%d:%d", &id0, &id1) != 2) {
			free(pathbuf, M_TEMP);
			return (EINVAL);
		}

		mtx_lock(&mountlist_mtx);
		TAILQ_FOREACH_REVERSE(mp, &mountlist, mntlist, mnt_list) {
			if (mp->mnt_stat.f_fsid.val[0] == id0 &&
			    mp->mnt_stat.f_fsid.val[1] == id1)
				break;
		}
		mtx_unlock(&mountlist_mtx);
	} else {
		mtx_lock(&mountlist_mtx);
		TAILQ_FOREACH_REVERSE(mp, &mountlist, mntlist, mnt_list) {
			if (strcmp(mp->mnt_stat.f_mntonname, pathbuf) == 0)
				break;
		}
		mtx_unlock(&mountlist_mtx);
	}
	free(pathbuf, M_TEMP);
	if (mp == NULL) {
		/*
		 * Previously we returned ENOENT for a nonexistent path and
		 * EINVAL for a non-mountpoint.  We cannot tell these apart
		 * now, so in the !MNT_BYFSID case return the more likely
		 * EINVAL for compatibility.
		 */
		return ((uap->flags & MNT_BYFSID) ? ENOENT : EINVAL);
	}

	/*
	 * Only privileged root, or (if MNT_USER is set) the user that did the
	 * original mount is permitted to unmount this filesystem.
	 */
	error = vfs_suser(mp, td);
	if (error)
		return (error);

	/*
	 * Don't allow unmounting the root filesystem.
	 */
	if (mp->mnt_flag & MNT_ROOTFS)
		return (EINVAL);
	mtx_lock(&Giant);
	error = dounmount(mp, uap->flags, td);
	mtx_unlock(&Giant);
	return (error);
}

/*
 * Do the actual filesystem unmount.
 */
int
dounmount(mp, flags, td)
	struct mount *mp;
	int flags;
	struct thread *td;
{
	struct vnode *coveredvp, *fsrootvp;
	int error;
	int async_flag;

	mtx_assert(&Giant, MA_OWNED);

	MNT_ILOCK(mp);
	if (mp->mnt_kern_flag & MNTK_UNMOUNT) {
		MNT_IUNLOCK(mp);
		return (EBUSY);
	}
	mp->mnt_kern_flag |= MNTK_UNMOUNT;
	/* Allow filesystems to detect that a forced unmount is in progress. */
	if (flags & MNT_FORCE)
		mp->mnt_kern_flag |= MNTK_UNMOUNTF;
	error = lockmgr(&mp->mnt_lock, LK_DRAIN | LK_INTERLOCK |
	    ((flags & MNT_FORCE) ? 0 : LK_NOWAIT), MNT_MTX(mp), td);
	if (error) {
		MNT_ILOCK(mp);
		mp->mnt_kern_flag &= ~(MNTK_UNMOUNT | MNTK_UNMOUNTF);
		if (mp->mnt_kern_flag & MNTK_MWAIT)
			wakeup(mp);
		MNT_IUNLOCK(mp);
		return (error);
	}
	vn_start_write(NULL, &mp, V_WAIT);

	if (mp->mnt_flag & MNT_EXPUBLIC)
		vfs_setpublicfs(NULL, NULL, NULL);

	vfs_msync(mp, MNT_WAIT);
	async_flag = mp->mnt_flag & MNT_ASYNC;
	mp->mnt_flag &= ~MNT_ASYNC;
	cache_purgevfs(mp);	/* remove cache entries for this file sys */
	if (mp->mnt_syncer != NULL)
		vrele(mp->mnt_syncer);
	/*
	 * For forced unmounts, move process cdir/rdir refs on the fs root
	 * vnode to the covered vnode.  For non-forced unmounts we want
	 * such references to cause an EBUSY error.
	 */
	if ((flags & MNT_FORCE) &&
	    VFS_ROOT(mp, LK_EXCLUSIVE, &fsrootvp, td) == 0) {
		if (mp->mnt_vnodecovered != NULL)
			mountcheckdirs(fsrootvp, mp->mnt_vnodecovered);
		if (fsrootvp == rootvnode) {
			vrele(rootvnode);
			rootvnode = NULL;
		}
		vput(fsrootvp);
	}
	if (((mp->mnt_flag & MNT_RDONLY) ||
	     (error = VFS_SYNC(mp, MNT_WAIT, td)) == 0) ||
	    (flags & MNT_FORCE)) {
		error = VFS_UNMOUNT(mp, flags, td);
	}
	vn_finished_write(mp);
	if (error) {
		/* Undo cdir/rdir and rootvnode changes made above. */
		if ((flags & MNT_FORCE) &&
		    VFS_ROOT(mp, LK_EXCLUSIVE, &fsrootvp, td) == 0) {
			if (mp->mnt_vnodecovered != NULL)
				mountcheckdirs(mp->mnt_vnodecovered, fsrootvp);
			if (rootvnode == NULL) {
				rootvnode = fsrootvp;
				vref(rootvnode);
			}
			vput(fsrootvp);
		}
		if ((mp->mnt_flag & MNT_RDONLY) == 0 && mp->mnt_syncer == NULL)
			(void) vfs_allocate_syncvnode(mp);
		MNT_ILOCK(mp);
		mp->mnt_kern_flag &= ~(MNTK_UNMOUNT | MNTK_UNMOUNTF);
		mp->mnt_flag |= async_flag;
		lockmgr(&mp->mnt_lock, LK_RELEASE, NULL, td);
		if (mp->mnt_kern_flag & MNTK_MWAIT)
			wakeup(mp);
		MNT_IUNLOCK(mp);
		return (error);
	}
	mtx_lock(&mountlist_mtx);
	TAILQ_REMOVE(&mountlist, mp, mnt_list);
	if ((coveredvp = mp->mnt_vnodecovered) != NULL)
		coveredvp->v_mountedhere = NULL;
	mtx_unlock(&mountlist_mtx);
	vfs_event_signal(NULL, VQ_UNMOUNT, 0);
	vfs_mount_destroy(mp, td);
	if (coveredvp != NULL)
		vrele(coveredvp);
	return (0);
}

/*
 * ---------------------------------------------------------------------
 * Mounting of root filesystem
 *
 */

struct root_hold_token {
	const char 			*who;
	LIST_ENTRY(root_hold_token)	list;
};

static LIST_HEAD(, root_hold_token)	root_holds =
    LIST_HEAD_INITIALIZER(&root_holds);

struct root_hold_token *
root_mount_hold(const char *identifier)
{
	struct root_hold_token *h;

	h = malloc(sizeof *h, M_DEVBUF, M_ZERO | M_WAITOK);
	h->who = identifier;
	mtx_lock(&mountlist_mtx);
	LIST_INSERT_HEAD(&root_holds, h, list);
	mtx_unlock(&mountlist_mtx);
	return (h);
}

void
root_mount_rel(struct root_hold_token *h)
{

	mtx_lock(&mountlist_mtx);
	LIST_REMOVE(h, list);
	wakeup(&root_holds);
	mtx_unlock(&mountlist_mtx);
	free(h, M_DEVBUF);
}

static void
root_mount_wait(void)
{
	struct root_hold_token *h;

	mtx_lock(&mountlist_mtx);
	while (!LIST_EMPTY(&root_holds)) {
		printf("Root mount waiting for:");
		LIST_FOREACH(h, &root_holds, list)
			printf(" %s", h->who);
		printf("\n");
		msleep(&root_holds, &mountlist_mtx, PZERO, "roothold", hz);
	}
	mtx_unlock(&mountlist_mtx);
}

static void
set_rootvnode(struct thread *td)
{
	struct proc *p;

	if (VFS_ROOT(TAILQ_FIRST(&mountlist), LK_EXCLUSIVE, &rootvnode, td))
		panic("Cannot find root vnode");

	p = td->td_proc;
	FILEDESC_LOCK(p->p_fd);

	if (p->p_fd->fd_cdir != NULL)
		vrele(p->p_fd->fd_cdir);
	p->p_fd->fd_cdir = rootvnode;
	VREF(rootvnode);

	if (p->p_fd->fd_rdir != NULL)
		vrele(p->p_fd->fd_rdir);
	p->p_fd->fd_rdir = rootvnode;
	VREF(rootvnode);

	FILEDESC_UNLOCK(p->p_fd);

	VOP_UNLOCK(rootvnode, 0, td);
}

/*
 * Mount /devfs as our root filesystem, but do not put it on the mountlist
 * yet.  Create a /dev -> / symlink so that absolute pathnames will lookup.
 */

static struct mount *
devfs_first(void)
{
	struct thread *td = curthread;
	struct vfsconf *vfsp;
	struct mount *mp = NULL;
	int error;

	vfsp = vfs_byname("devfs");
	KASSERT(vfsp != NULL, ("Could not find devfs by name"));
	if (vfsp == NULL) 
		return(NULL);

	error = vfs_mount_alloc(NULLVP, vfsp, "/dev", td, &mp);
	KASSERT(error == 0, ("vfs_mount_alloc failed %d", error));
	if (error)
		return (NULL);

	error = VFS_MOUNT(mp, curthread);
	KASSERT(error == 0, ("VFS_MOUNT(devfs) failed %d", error));
	if (error)
		return (NULL);

	mtx_lock(&mountlist_mtx);
	TAILQ_INSERT_HEAD(&mountlist, mp, mnt_list);
	mtx_unlock(&mountlist_mtx);

	set_rootvnode(td);

	error = kern_symlink(td, "/", "dev", UIO_SYSSPACE);
	if (error)
		printf("kern_symlink /dev -> / returns %d\n", error);

	return (mp);
}

/*
 * Surgically move our devfs to be mounted on /dev.
 */

static void
devfs_fixup(struct thread *td)
{
	struct nameidata nd;
	int error;
	struct vnode *vp, *dvp;
	struct mount *mp;

	/* Remove our devfs mount from the mountlist and purge the cache */
	mtx_lock(&mountlist_mtx);
	mp = TAILQ_FIRST(&mountlist);
	TAILQ_REMOVE(&mountlist, mp, mnt_list);
	mtx_unlock(&mountlist_mtx);
	cache_purgevfs(mp);

	VFS_ROOT(mp, LK_EXCLUSIVE, &dvp, td);
	VI_LOCK(dvp);
	dvp->v_iflag &= ~VI_MOUNT;
	dvp->v_mountedhere = NULL;
	VI_UNLOCK(dvp);

	/* Set up the real rootvnode, and purge the cache */
	TAILQ_FIRST(&mountlist)->mnt_vnodecovered = NULL;
	set_rootvnode(td);
	cache_purgevfs(rootvnode->v_mount);

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, "/dev", td);
	error = namei(&nd);
	if (error) {
		printf("Lookup of /dev for devfs, error: %d\n", error);
		return;
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;
	if (vp->v_type != VDIR) {
		vput(vp);
	}
	error = vinvalbuf(vp, V_SAVE, td, 0, 0);
	if (error) {
		vput(vp);
	}
	cache_purge(vp);
	mp->mnt_vnodecovered = vp;
	vp->v_mountedhere = mp;
	mtx_lock(&mountlist_mtx);
	TAILQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	mtx_unlock(&mountlist_mtx);
	VOP_UNLOCK(vp, 0, td);
	vfs_unbusy(mp, td);
	vput(dvp);

	/* Unlink the no longer needed /dev/dev -> / symlink */
	kern_unlink(td, "/dev/dev", UIO_SYSSPACE);
}

/*
 * Find and mount the root filesystem
 */
void
vfs_mountroot(void)
{
	char *cp;
	int error, i, asked = 0;
	struct mount *mp;

	root_mount_wait();

	mp = devfs_first();

	/*
	 * We are booted with instructions to prompt for the root filesystem.
	 */
	if (boothowto & RB_ASKNAME) {
		if (!vfs_mountroot_ask())
			return;
		asked = 1;
	}

	/*
	 * The root filesystem information is compiled in, and we are
	 * booted with instructions to use it.
	 */
	if (ctrootdevname != NULL && (boothowto & RB_DFLTROOT)) {
		if (!vfs_mountroot_try(ctrootdevname))
			return;
		ctrootdevname = NULL;
	}

	/*
	 * We've been given the generic "use CDROM as root" flag.  This is
	 * necessary because one media may be used in many different
	 * devices, so we need to search for them.
	 */
	if (boothowto & RB_CDROM) {
		for (i = 0; cdrom_rootdevnames[i] != NULL; i++) {
			if (!vfs_mountroot_try(cdrom_rootdevnames[i]))
				return;
		}
	}

	/*
	 * Try to use the value read by the loader from /etc/fstab, or
	 * supplied via some other means.  This is the preferred
	 * mechanism.
	 */
	cp = getenv("vfs.root.mountfrom");
	if (cp != NULL) {
		error = vfs_mountroot_try(cp);
		freeenv(cp);
		if (!error)
			return;
	}

	/*
	 * Try values that may have been computed by code during boot
	 */
	if (!vfs_mountroot_try(rootdevnames[0]))
		return;
	if (!vfs_mountroot_try(rootdevnames[1]))
		return;

	/*
	 * If we (still) have a compiled-in default, try it.
	 */
	if (ctrootdevname != NULL)
		if (!vfs_mountroot_try(ctrootdevname))
			return;
	/*
	 * Everything so far has failed, prompt on the console if we haven't
	 * already tried that.
	 */
	if (!asked)
		if (!vfs_mountroot_ask())
			return;

	panic("Root mount failed, startup aborted.");
}

/*
 * Mount (mountfrom) as the root filesystem.
 */
static int
vfs_mountroot_try(const char *mountfrom)
{
        struct mount	*mp;
	char		*vfsname, *path;
	int		error;
	char		patt[32];
	int		s;

	vfsname = NULL;
	path    = NULL;
	mp      = NULL;
	error   = EINVAL;

	if (mountfrom == NULL)
		return (error);		/* don't complain */

	s = splcam();			/* Overkill, but annoying without it */
	printf("Trying to mount root from %s\n", mountfrom);
	splx(s);

	/* parse vfs name and path */
	vfsname = malloc(MFSNAMELEN, M_MOUNT, M_WAITOK);
	path = malloc(MNAMELEN, M_MOUNT, M_WAITOK);
	vfsname[0] = path[0] = 0;
	sprintf(patt, "%%%d[a-z0-9]:%%%ds", MFSNAMELEN, MNAMELEN);
	if (sscanf(mountfrom, patt, vfsname, path) < 1) {
		free(path, M_MOUNT);
		free(vfsname, M_MOUNT);
		return (error);
	}

	if (path[0] == '\0')
		strcpy(path, ROOTNAME);

	error = kernel_vmount(
	    MNT_RDONLY | MNT_ROOTFS,
	    "fstype", vfsname,
	    "fspath", "/",
	    "from", path,
	    NULL);
	if (error == 0) {
		/*
		 * We mount devfs prior to mounting the / FS, so the first
		 * entry will typically be devfs.
		 */
		mp = TAILQ_FIRST(&mountlist);
		KASSERT(mp != NULL, ("%s: mountlist is empty", __func__));
		vfs_unbusy(mp, curthread);

		/*
		 * Iterate over all currently mounted file systems and use
		 * the time stamp found to check and/or initialize the RTC.
		 * Typically devfs has no time stamp and the only other FS
		 * is the actual / FS.
		 */
		do {
			if (mp->mnt_time != 0)
				inittodr(mp->mnt_time);
			mp = TAILQ_NEXT(mp, mnt_list);
		} while (mp != NULL);

		devfs_fixup(curthread);
	}
	return (error);
}

/*
 * ---------------------------------------------------------------------
 * Interactive root filesystem selection code.
 */

static int
vfs_mountroot_ask(void)
{
	char name[128];

	for(;;) {
		printf("\nManual root filesystem specification:\n");
		printf("  <fstype>:<device>  Mount <device> using filesystem <fstype>\n");
#if defined(__i386__) || defined(__ia64__)
		printf("                       eg. ufs:da0s1a\n");
#else
		printf("                       eg. ufs:/dev/da0a\n");
#endif
		printf("  ?                  List valid disk boot devices\n");
		printf("  <empty line>       Abort manual input\n");
		printf("\nmountroot> ");
		gets(name, sizeof(name), 1);
		if (name[0] == '\0')
			return (1);
		if (name[0] == '?') {
			printf("\nList of GEOM managed disk devices:\n  ");
			g_dev_print();
			continue;
		}
		if (!vfs_mountroot_try(name))
			return (0);
	}
}

/*
 * ---------------------------------------------------------------------
 * Functions for querying mount options/arguments from filesystems.
 */

/*
 * Check that no unknown options are given
 */
int
vfs_filteropt(struct vfsoptlist *opts, const char **legal)
{
	struct vfsopt *opt;
	const char **t, *p;
	

	TAILQ_FOREACH(opt, opts, link) {
		p = opt->name;
		if (p[0] == 'n' && p[1] == 'o')
			p += 2;
		for(t = global_opts; *t != NULL; t++)
			if (!strcmp(*t, p))
				break;
		if (*t != NULL)
			continue;
		for(t = legal; *t != NULL; t++)
			if (!strcmp(*t, p))
				break;
		if (*t != NULL)
			continue;
		printf("mount option <%s> is unknown\n", p);
		return (EINVAL);
	}
	return (0);
}

/*
 * Get a mount option by its name.
 *
 * Return 0 if the option was found, ENOENT otherwise.
 * If len is non-NULL it will be filled with the length
 * of the option. If buf is non-NULL, it will be filled
 * with the address of the option.
 */
int
vfs_getopt(opts, name, buf, len)
	struct vfsoptlist *opts;
	const char *name;
	void **buf;
	int *len;
{
	struct vfsopt *opt;

	KASSERT(opts != NULL, ("vfs_getopt: caller passed 'opts' as NULL"));

	TAILQ_FOREACH(opt, opts, link) {
		if (strcmp(name, opt->name) == 0) {
			if (len != NULL)
				*len = opt->len;
			if (buf != NULL)
				*buf = opt->value;
			return (0);
		}
	}
	return (ENOENT);
}

char *
vfs_getopts(struct vfsoptlist *opts, const char *name, int *error)
{
	struct vfsopt *opt;

	*error = 0;
	TAILQ_FOREACH(opt, opts, link) {
		if (strcmp(name, opt->name) != 0)
			continue;
		if (((char *)opt->value)[opt->len - 1] != '\0') {
			*error = EINVAL;
			return (NULL);
		}
		return (opt->value);
	}
	return (NULL);
}

int
vfs_flagopt(struct vfsoptlist *opts, const char *name, u_int *w, u_int val)
{
	struct vfsopt *opt;

	TAILQ_FOREACH(opt, opts, link) {
		if (strcmp(name, opt->name) == 0) {
			if (w != NULL)
				*w |= val;
			return (1);
		}
	}
	if (w != NULL)
		*w &= ~val;
	return (0);
}

int
vfs_scanopt(struct vfsoptlist *opts, const char *name, const char *fmt, ...)
{
	va_list ap;
	struct vfsopt *opt;
	int ret;

	KASSERT(opts != NULL, ("vfs_getopt: caller passed 'opts' as NULL"));

	TAILQ_FOREACH(opt, opts, link) {
		if (strcmp(name, opt->name) != 0)
			continue;
		if (((char *)opt->value)[opt->len - 1] != '\0')
			return (0);
		va_start(ap, fmt);
		ret = vsscanf(opt->value, fmt, ap);
		va_end(ap);
		return (ret);
	}
	return (0);
}

/*
 * Find and copy a mount option.
 *
 * The size of the buffer has to be specified
 * in len, if it is not the same length as the
 * mount option, EINVAL is returned.
 * Returns ENOENT if the option is not found.
 */
int
vfs_copyopt(opts, name, dest, len)
	struct vfsoptlist *opts;
	const char *name;
	void *dest;
	int len;
{
	struct vfsopt *opt;

	KASSERT(opts != NULL, ("vfs_copyopt: caller passed 'opts' as NULL"));

	TAILQ_FOREACH(opt, opts, link) {
		if (strcmp(name, opt->name) == 0) {
			if (len != opt->len)
				return (EINVAL);
			bcopy(opt->value, dest, opt->len);
			return (0);
		}
	}
	return (ENOENT);
}

/*
 * This is a helper function for filesystems to traverse their
 * vnodes.  See MNT_VNODE_FOREACH() in sys/mount.h
 */

struct vnode *
__mnt_vnode_next(struct vnode **nvp, struct mount *mp)
{
	struct vnode *vp;

	mtx_assert(&mp->mnt_mtx, MA_OWNED);

	vp = *nvp;
	/* Check if we are done */
	if (vp == NULL)
		return (NULL);
	/* If our next vnode is no longer ours, start over */
	if (vp->v_mount != mp) 
		vp = TAILQ_FIRST(&mp->mnt_nvnodelist);
	/* Save pointer to next vnode in list */
	if (vp != NULL)
		*nvp = TAILQ_NEXT(vp, v_nmntvnodes);
	else
		*nvp = NULL;
	return (vp);
}

int
__vfs_statfs(struct mount *mp, struct statfs *sbp, struct thread *td)
{
	int error;

	error = mp->mnt_op->vfs_statfs(mp, &mp->mnt_stat, td);
	if (sbp != &mp->mnt_stat)
		*sbp = mp->mnt_stat;
	return (error);
}

void
vfs_mountedfrom(struct mount *mp, const char *from)
{

	bzero(mp->mnt_stat.f_mntfromname, sizeof mp->mnt_stat.f_mntfromname);
	strlcpy(mp->mnt_stat.f_mntfromname, from,
	    sizeof mp->mnt_stat.f_mntfromname);
}

/*
 * ---------------------------------------------------------------------
 * This is the api for building mount args and mounting filesystems from
 * inside the kernel.
 *
 * The API works by accumulation of individual args.  First error is
 * latched.
 *
 * XXX: should be documented in new manpage kernel_mount(9)
 */

/* A memory allocation which must be freed when we are done */
struct mntaarg {
	SLIST_ENTRY(mntaarg)	next;
};

/* The header for the mount arguments */
struct mntarg {
	struct iovec *v;
	int len;
	int error;
	SLIST_HEAD(, mntaarg)	list;
};

/*
 * Add a boolean argument.
 *
 * flag is the boolean value.
 * name must start with "no".
 */
struct mntarg *
mount_argb(struct mntarg *ma, int flag, const char *name)
{

	KASSERT(name[0] == 'n' && name[1] == 'o',
	    ("mount_argb(...,%s): name must start with 'no'", name));

	return (mount_arg(ma, name + (flag ? 2 : 0), NULL, 0));
}

/*
 * Add an argument printf style
 */
struct mntarg *
mount_argf(struct mntarg *ma, const char *name, const char *fmt, ...)
{
	va_list ap;
	struct mntaarg *maa;
	struct sbuf *sb;
	int len;

	if (ma == NULL) {
		ma = malloc(sizeof *ma, M_MOUNT, M_WAITOK | M_ZERO);
		SLIST_INIT(&ma->list);
	}
	if (ma->error)
		return (ma);

	ma->v = realloc(ma->v, sizeof *ma->v * (ma->len + 2),
	    M_MOUNT, M_WAITOK);
	ma->v[ma->len].iov_base = (void *)(uintptr_t)name;
	ma->v[ma->len].iov_len = strlen(name) + 1;
	ma->len++;

	sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	va_start(ap, fmt);
	sbuf_vprintf(sb, fmt, ap);
	va_end(ap);
	sbuf_finish(sb);
	len = sbuf_len(sb) + 1;
	maa = malloc(sizeof *maa + len, M_MOUNT, M_WAITOK | M_ZERO);
	SLIST_INSERT_HEAD(&ma->list, maa, next);
	bcopy(sbuf_data(sb), maa + 1, len);
	sbuf_delete(sb);

	ma->v[ma->len].iov_base = maa + 1;
	ma->v[ma->len].iov_len = len;
	ma->len++;

	return (ma);
}

/*
 * Add an argument which is a userland string.
 */
struct mntarg *
mount_argsu(struct mntarg *ma, const char *name, const void *val, int len)
{
	struct mntaarg *maa;
	char *tbuf;

	if (val == NULL)
		return (ma);
	if (ma == NULL) {
		ma = malloc(sizeof *ma, M_MOUNT, M_WAITOK | M_ZERO);
		SLIST_INIT(&ma->list);
	}
	if (ma->error)
		return (ma);
	maa = malloc(sizeof *maa + len, M_MOUNT, M_WAITOK | M_ZERO);
	SLIST_INSERT_HEAD(&ma->list, maa, next);
	tbuf = (void *)(maa + 1);
	ma->error = copyinstr(val, tbuf, len, NULL);
	return (mount_arg(ma, name, tbuf, -1));
}

/*
 * Plain argument.
 *
 * If length is -1, use printf.
 */
struct mntarg *
mount_arg(struct mntarg *ma, const char *name, const void *val, int len)
{

	if (ma == NULL) {
		ma = malloc(sizeof *ma, M_MOUNT, M_WAITOK | M_ZERO);
		SLIST_INIT(&ma->list);
	}
	if (ma->error)
		return (ma);

	ma->v = realloc(ma->v, sizeof *ma->v * (ma->len + 2),
	    M_MOUNT, M_WAITOK);
	ma->v[ma->len].iov_base = (void *)(uintptr_t)name;
	ma->v[ma->len].iov_len = strlen(name) + 1;
	ma->len++;

	ma->v[ma->len].iov_base = (void *)(uintptr_t)val;
	if (len < 0)
		ma->v[ma->len].iov_len = strlen(val) + 1;
	else
		ma->v[ma->len].iov_len = len;
	ma->len++;
	return (ma);
}

/*
 * Free a mntarg structure
 */
static void
free_mntarg(struct mntarg *ma)
{
	struct mntaarg *maa;

	while (!SLIST_EMPTY(&ma->list)) {
		maa = SLIST_FIRST(&ma->list);
		SLIST_REMOVE_HEAD(&ma->list, next);
		free(maa, M_MOUNT);
	}
	free(ma->v, M_MOUNT);
	free(ma, M_MOUNT);
}

/*
 * Mount a filesystem
 */
int
kernel_mount(struct mntarg *ma, int flags)
{
	struct uio auio;
	int error;

	KASSERT(ma != NULL, ("kernel_mount NULL ma"));
	KASSERT(ma->v != NULL, ("kernel_mount NULL ma->v"));
	KASSERT(!(ma->len & 1), ("kernel_mount odd ma->len (%d)", ma->len));

	auio.uio_iov = ma->v;
	auio.uio_iovcnt = ma->len;
	auio.uio_segflg = UIO_SYSSPACE;

	error = ma->error;
	if (!error)
		error = vfs_donmount(curthread, flags, &auio);
	free_mntarg(ma);
	return (error);
}

/*
 * A printflike function to mount a filesystem.
 */
int
kernel_vmount(int flags, ...)
{
	struct mntarg *ma = NULL;
	va_list ap;
	const char *cp;
	const void *vp;
	int error;

	va_start(ap, flags);
	for (;;) {
		cp = va_arg(ap, const char *);
		if (cp == NULL)
			break;
		vp = va_arg(ap, const void *);
		ma = mount_arg(ma, cp, vp, -1);
	}
	va_end(ap);

	error = kernel_mount(ma, flags);
	return (error);
}
