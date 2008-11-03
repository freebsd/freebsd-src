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
#include <sys/fcntl.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/priv.h>
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
#include <vm/uma.h>

#include <geom/geom.h>

#include <machine/stdarg.h>

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

#include "opt_rootdevname.h"
#include "opt_mac.h"

#define	ROOTNAME		"root_device"
#define	VFS_MOUNTARG_SIZE_MAX	(1024 * 64)

static int	vfs_domount(struct thread *td, const char *fstype,
		    char *fspath, int fsflags, void *fsdata);
static int	vfs_mountroot_ask(void);
static int	vfs_mountroot_try(const char *mountfrom);
static void	free_mntarg(struct mntarg *ma);
static int	vfs_getopt_pos(struct vfsoptlist *opts, const char *name);

static int	usermount = 0;
SYSCTL_INT(_vfs, OID_AUTO, usermount, CTLFLAG_RW, &usermount, 0,
    "Unprivileged users may mount and unmount file systems");

MALLOC_DEFINE(M_MOUNT, "mount", "vfs mount structure");
MALLOC_DEFINE(M_VNODE_MARKER, "vnodemarker", "vnode marker");
static uma_zone_t mount_zone;

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
	"errmsg",
	"fstype",
	"fspath",
	"ro",
	"rw",
	"nosuid",
	"noexec",
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
void
vfs_freeopts(struct vfsoptlist *opts)
{
	struct vfsopt *opt;

	while (!TAILQ_EMPTY(opts)) {
		opt = TAILQ_FIRST(opts);
		vfs_freeopt(opts, opt);
	}
	free(opts, M_MOUNT);
}

void
vfs_deleteopt(struct vfsoptlist *opts, const char *name)
{
	struct vfsopt *opt, *temp;

	if (opts == NULL)
		return;
	TAILQ_FOREACH_SAFE(opt, opts, link, temp)  {
		if (strcmp(opt->name, name) == 0)
			vfs_freeopt(opts, opt);
	}
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
 *
 * XXX This function will keep a "nofoo" option in the
 *     new options if there is no matching "foo" option
 *     to be cancelled in the old options.  This is a bug
 *     if the option's canonical name is "foo".  E.g., "noro"
 *     shouldn't end up in the mount point's active options,
 *     but it can.
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
 * Mount a filesystem.
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

	AUDIT_ARG(fflags, uap->flags);

	/*
	 * Filter out MNT_ROOTFS.  We do not want clients of nmount() in
	 * userspace to set this flag, but we must filter it out if we want
	 * MNT_UPDATE on the root file system to work.
	 * MNT_ROOTFS should only be set in the kernel in vfs_mountroot_try().
	 */
	uap->flags &= ~MNT_ROOTFS;

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

void
vfs_ref(struct mount *mp)
{

	MNT_ILOCK(mp);
	MNT_REF(mp);
	MNT_IUNLOCK(mp);
}

void
vfs_rel(struct mount *mp)
{

	MNT_ILOCK(mp);
	MNT_REL(mp);
	MNT_IUNLOCK(mp);
}

static int
mount_init(void *mem, int size, int flags)
{
	struct mount *mp;

	mp = (struct mount *)mem;
	mtx_init(&mp->mnt_mtx, "struct mount mtx", NULL, MTX_DEF);
	lockinit(&mp->mnt_explock, PVFS, "explock", 0, 0);
	return (0);
}

static void
mount_fini(void *mem, int size)
{
	struct mount *mp;

	mp = (struct mount *)mem;
	lockdestroy(&mp->mnt_explock);
	mtx_destroy(&mp->mnt_mtx);
}

/*
 * Allocate and initialize the mount point struct.
 */
struct mount *
vfs_mount_alloc(struct vnode *vp, struct vfsconf *vfsp, const char *fspath,
    struct ucred *cred)
{
	struct mount *mp;

	mp = uma_zalloc(mount_zone, M_WAITOK);
	bzero(&mp->mnt_startzero,
	    __rangeof(struct mount, mnt_startzero, mnt_endzero));
	TAILQ_INIT(&mp->mnt_nvnodelist);
	mp->mnt_nvnodelistsize = 0;
	mp->mnt_ref = 0;
	(void) vfs_busy(mp, MBF_NOWAIT);
	mp->mnt_op = vfsp->vfc_vfsops;
	mp->mnt_vfc = vfsp;
	vfsp->vfc_refcount++;	/* XXX Unlocked */
	mp->mnt_stat.f_type = vfsp->vfc_typenum;
	mp->mnt_gen++;
	strlcpy(mp->mnt_stat.f_fstypename, vfsp->vfc_name, MFSNAMELEN);
	mp->mnt_vnodecovered = vp;
	mp->mnt_cred = crdup(cred);
	mp->mnt_stat.f_owner = cred->cr_uid;
	strlcpy(mp->mnt_stat.f_mntonname, fspath, MNAMELEN);
	mp->mnt_iosize_max = DFLTPHYS;
#ifdef MAC
	mac_mount_init(mp);
	mac_mount_create(cred, mp);
#endif
	arc4rand(&mp->mnt_hashseed, sizeof mp->mnt_hashseed, 0);
	return (mp);
}

/*
 * Destroy the mount struct previously allocated by vfs_mount_alloc().
 */
void
vfs_mount_destroy(struct mount *mp)
{

	MNT_ILOCK(mp);
	while (mp->mnt_ref)
		msleep(mp, MNT_MTX(mp), PVFS, "mntref", 0);
	if (mp->mnt_writeopcount > 0) {
		printf("Waiting for mount point write ops\n");
		while (mp->mnt_writeopcount > 0) {
			mp->mnt_kern_flag |= MNTK_SUSPEND;
			msleep(&mp->mnt_writeopcount,
			       MNT_MTX(mp),
			       PZERO, "mntdestroy2", 0);
		}
		printf("mount point write ops completed\n");
	}
	if (mp->mnt_secondary_writes > 0) {
		printf("Waiting for mount point secondary write ops\n");
		while (mp->mnt_secondary_writes > 0) {
			mp->mnt_kern_flag |= MNTK_SUSPEND;
			msleep(&mp->mnt_secondary_writes,
			       MNT_MTX(mp),
			       PZERO, "mntdestroy3", 0);
		}
		printf("mount point secondary write ops completed\n");
	}
	MNT_IUNLOCK(mp);
	mp->mnt_vfc->vfc_refcount--;
	if (!TAILQ_EMPTY(&mp->mnt_nvnodelist)) {
		struct vnode *vp;

		TAILQ_FOREACH(vp, &mp->mnt_nvnodelist, v_nmntvnodes)
			vprint("", vp);
		panic("unmount: dangling vnode");
	}
	MNT_ILOCK(mp);
	if (mp->mnt_kern_flag & MNTK_MWAIT)
		wakeup(mp);
	if (mp->mnt_writeopcount != 0)
		panic("vfs_mount_destroy: nonzero writeopcount");
	if (mp->mnt_secondary_writes != 0)
		panic("vfs_mount_destroy: nonzero secondary_writes");
	if (mp->mnt_nvnodelistsize != 0)
		panic("vfs_mount_destroy: nonzero nvnodelistsize");
	mp->mnt_writeopcount = -1000;
	mp->mnt_nvnodelistsize = -1000;
	mp->mnt_secondary_writes = -1000;
	MNT_IUNLOCK(mp);
#ifdef MAC
	mac_mount_destroy(mp);
#endif
	if (mp->mnt_opt != NULL)
		vfs_freeopts(mp->mnt_opt);
	crfree(mp->mnt_cred);
	uma_zfree(mount_zone, mp);
}

int
vfs_donmount(struct thread *td, int fsflags, struct uio *fsoptions)
{
	struct vfsoptlist *optlist;
	struct vfsopt *opt, *noro_opt, *tmp_opt;
	char *fstype, *fspath, *errmsg;
	int error, fstypelen, fspathlen, errmsg_len, errmsg_pos;
	int has_rw, has_noro;

	errmsg = fspath = NULL;
	errmsg_len = has_noro = has_rw = fspathlen = 0;
	errmsg_pos = -1;

	error = vfs_buildopts(fsoptions, &optlist);
	if (error)
		return (error);

	if (vfs_getopt(optlist, "errmsg", (void **)&errmsg, &errmsg_len) == 0)
		errmsg_pos = vfs_getopt_pos(optlist, "errmsg");

	/*
	 * We need these two options before the others,
	 * and they are mandatory for any filesystem.
	 * Ensure they are NUL terminated as well.
	 */
	fstypelen = 0;
	error = vfs_getopt(optlist, "fstype", (void **)&fstype, &fstypelen);
	if (error || fstype[fstypelen - 1] != '\0') {
		error = EINVAL;
		if (errmsg != NULL)
			strncpy(errmsg, "Invalid fstype", errmsg_len);
		goto bail;
	}
	fspathlen = 0;
	error = vfs_getopt(optlist, "fspath", (void **)&fspath, &fspathlen);
	if (error || fspath[fspathlen - 1] != '\0') {
		error = EINVAL;
		if (errmsg != NULL)
			strncpy(errmsg, "Invalid fspath", errmsg_len);
		goto bail;
	}

	/*
	 * We need to see if we have the "update" option
	 * before we call vfs_domount(), since vfs_domount() has special
	 * logic based on MNT_UPDATE.  This is very important
	 * when we want to update the root filesystem.
	 */
	TAILQ_FOREACH_SAFE(opt, optlist, link, tmp_opt) {
		if (strcmp(opt->name, "update") == 0) {
			fsflags |= MNT_UPDATE;
			vfs_freeopt(optlist, opt);
		}
		else if (strcmp(opt->name, "async") == 0)
			fsflags |= MNT_ASYNC;
		else if (strcmp(opt->name, "force") == 0) {
			fsflags |= MNT_FORCE;
			vfs_freeopt(optlist, opt);
		}
		else if (strcmp(opt->name, "reload") == 0) {
			fsflags |= MNT_RELOAD;
			vfs_freeopt(optlist, opt);
		}
		else if (strcmp(opt->name, "multilabel") == 0)
			fsflags |= MNT_MULTILABEL;
		else if (strcmp(opt->name, "noasync") == 0)
			fsflags &= ~MNT_ASYNC;
		else if (strcmp(opt->name, "noatime") == 0)
			fsflags |= MNT_NOATIME;
		else if (strcmp(opt->name, "atime") == 0) {
			free(opt->name, M_MOUNT);
			opt->name = strdup("nonoatime", M_MOUNT);
		}
		else if (strcmp(opt->name, "noclusterr") == 0)
			fsflags |= MNT_NOCLUSTERR;
		else if (strcmp(opt->name, "clusterr") == 0) {
			free(opt->name, M_MOUNT);
			opt->name = strdup("nonoclusterr", M_MOUNT);
		}
		else if (strcmp(opt->name, "noclusterw") == 0)
			fsflags |= MNT_NOCLUSTERW;
		else if (strcmp(opt->name, "clusterw") == 0) {
			free(opt->name, M_MOUNT);
			opt->name = strdup("nonoclusterw", M_MOUNT);
		}
		else if (strcmp(opt->name, "noexec") == 0)
			fsflags |= MNT_NOEXEC;
		else if (strcmp(opt->name, "exec") == 0) {
			free(opt->name, M_MOUNT);
			opt->name = strdup("nonoexec", M_MOUNT);
		}
		else if (strcmp(opt->name, "nosuid") == 0)
			fsflags |= MNT_NOSUID;
		else if (strcmp(opt->name, "suid") == 0) {
			free(opt->name, M_MOUNT);
			opt->name = strdup("nonosuid", M_MOUNT);
		}
		else if (strcmp(opt->name, "nosymfollow") == 0)
			fsflags |= MNT_NOSYMFOLLOW;
		else if (strcmp(opt->name, "symfollow") == 0) {
			free(opt->name, M_MOUNT);
			opt->name = strdup("nonosymfollow", M_MOUNT);
		}
		else if (strcmp(opt->name, "noro") == 0) {
			fsflags &= ~MNT_RDONLY;
			has_noro = 1;
		}
		else if (strcmp(opt->name, "rw") == 0) {
			fsflags &= ~MNT_RDONLY;
			has_rw = 1;
		}
		else if (strcmp(opt->name, "ro") == 0)
			fsflags |= MNT_RDONLY;
		else if (strcmp(opt->name, "rdonly") == 0) {
			free(opt->name, M_MOUNT);
			opt->name = strdup("ro", M_MOUNT);
			fsflags |= MNT_RDONLY;
		}
		else if (strcmp(opt->name, "suiddir") == 0)
			fsflags |= MNT_SUIDDIR;
		else if (strcmp(opt->name, "sync") == 0)
			fsflags |= MNT_SYNCHRONOUS;
		else if (strcmp(opt->name, "union") == 0)
			fsflags |= MNT_UNION;
	}

	/*
	 * If "rw" was specified as a mount option, and we
	 * are trying to update a mount-point from "ro" to "rw",
	 * we need a mount option "noro", since in vfs_mergeopts(),
	 * "noro" will cancel "ro", but "rw" will not do anything.
	 */
	if (has_rw && !has_noro) {
		noro_opt = malloc(sizeof(struct vfsopt), M_MOUNT, M_WAITOK);
		noro_opt->name = strdup("noro", M_MOUNT);
		noro_opt->value = NULL;
		noro_opt->len = 0;
		TAILQ_INSERT_TAIL(optlist, noro_opt, link);
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
	/* copyout the errmsg */
	if (errmsg_pos != -1 && ((2 * errmsg_pos + 1) < fsoptions->uio_iovcnt)
	    && errmsg_len > 0 && errmsg != NULL) {
		if (fsoptions->uio_segflg == UIO_SYSSPACE) {
			bcopy(errmsg,
			    fsoptions->uio_iov[2 * errmsg_pos + 1].iov_base,
			    fsoptions->uio_iov[2 * errmsg_pos + 1].iov_len);
		} else {
			copyout(errmsg,
			    fsoptions->uio_iov[2 * errmsg_pos + 1].iov_base,
			    fsoptions->uio_iov[2 * errmsg_pos + 1].iov_len);
		}
	}

	if (error != 0)
		vfs_freeopts(optlist);
	return (error);
}

/*
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

	AUDIT_ARG(fflags, uap->flags);

	/*
	 * Filter out MNT_ROOTFS.  We do not want clients of mount() in
	 * userspace to set this flag, but we must filter it out if we want
	 * MNT_UPDATE on the root file system to work.
	 * MNT_ROOTFS should only be set in the kernel in vfs_mountroot_try().
	 */
	uap->flags &= ~MNT_ROOTFS;

	fstype = malloc(MFSNAMELEN, M_TEMP, M_WAITOK);
	error = copyinstr(uap->type, fstype, MFSNAMELEN, NULL);
	if (error) {
		free(fstype, M_TEMP);
		return (error);
	}

	AUDIT_ARG(text, fstype);
	mtx_lock(&Giant);
	vfsp = vfs_byname_kld(fstype, td, &error);
	free(fstype, M_TEMP);
	if (vfsp == NULL) {
		mtx_unlock(&Giant);
		return (ENOENT);
	}
	if (vfsp->vfc_vfsops->vfs_cmount == NULL) {
		mtx_unlock(&Giant);
		return (EOPNOTSUPP);
	}

	ma = mount_argsu(ma, "fstype", uap->type, MNAMELEN);
	ma = mount_argsu(ma, "fspath", uap->path, MNAMELEN);
	ma = mount_argb(ma, uap->flags & MNT_RDONLY, "noro");
	ma = mount_argb(ma, !(uap->flags & MNT_NOSUID), "nosuid");
	ma = mount_argb(ma, !(uap->flags & MNT_NOEXEC), "noexec");

	error = vfsp->vfc_vfsops->vfs_cmount(ma, uap->data, uap->flags, td);
	mtx_unlock(&Giant);
	return (error);
}


/*
 * vfs_domount(): actually attempt a filesystem mount.
 */
static int
vfs_domount(
	struct thread *td,	/* Calling thread. */
	const char *fstype,	/* Filesystem type. */
	char *fspath,		/* Mount path. */
	int fsflags,		/* Flags common to all filesystems. */
	void *fsdata		/* Options local to the filesystem. */
	)
{
	struct vnode *vp;
	struct mount *mp;
	struct vfsconf *vfsp;
	struct oexport_args oexport;
	struct export_args export;
	int error, flag = 0;
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

	if (jailed(td->td_ucred) || usermount == 0) {
		if ((error = priv_check(td, PRIV_VFS_MOUNT)) != 0)
			return (error);
	}

	/*
	 * Do not allow NFS export or MNT_SUIDDIR by unprivileged users.
	 */
	if (fsflags & MNT_EXPORTED) {
		error = priv_check(td, PRIV_VFS_MOUNT_EXPORTED);
		if (error)
			return (error);
	}
	if (fsflags & MNT_SUIDDIR) {
		error = priv_check(td, PRIV_VFS_MOUNT_SUIDDIR);
		if (error)
			return (error);
	}
	/*
	 * Silently enforce MNT_NOSUID and MNT_USER for unprivileged users.
	 */
	if ((fsflags & (MNT_NOSUID | MNT_USER)) != (MNT_NOSUID | MNT_USER)) {
		if (priv_check(td, PRIV_VFS_MOUNT_NONUSER) != 0)
			fsflags |= MNT_NOSUID | MNT_USER;
	}

	/* Load KLDs before we lock the covered vnode to avoid reversals. */
	vfsp = NULL;
	if ((fsflags & MNT_UPDATE) == 0) {
		/* Don't try to load KLDs if we're mounting the root. */
		if (fsflags & MNT_ROOTFS)
			vfsp = vfs_byname(fstype);
		else
			vfsp = vfs_byname_kld(fstype, td, &error);
		if (vfsp == NULL)
			return (ENODEV);
		if (jailed(td->td_ucred) && !(vfsp->vfc_flags & VFCF_JAIL))
			return (EPERM);
	}
	/*
	 * Get vnode to be covered
	 */
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | AUDITVNODE1, UIO_SYSSPACE,
	    fspath, td);
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
		MNT_ILOCK(mp);
		flag = mp->mnt_flag;
		/*
		 * We only allow the filesystem to be reloaded if it
		 * is currently mounted read-only.
		 */
		if ((fsflags & MNT_RELOAD) &&
		    ((mp->mnt_flag & MNT_RDONLY) == 0)) {
			MNT_IUNLOCK(mp);
			vput(vp);
			return (EOPNOTSUPP);	/* Needs translation */
		}
		MNT_IUNLOCK(mp);
		/*
		 * Only privileged root, or (if MNT_USER is set) the user that
		 * did the original mount is permitted to update it.
		 */
		error = vfs_suser(mp, td);
		if (error) {
			vput(vp);
			return (error);
		}
		if (vfs_busy(mp, MBF_NOWAIT)) {
			vput(vp);
			return (EBUSY);
		}
		VI_LOCK(vp);
		if ((vp->v_iflag & VI_MOUNT) != 0 ||
		    vp->v_mountedhere != NULL) {
			VI_UNLOCK(vp);
			vfs_unbusy(mp);
			vput(vp);
			return (EBUSY);
		}
		vp->v_iflag |= VI_MOUNT;
		VI_UNLOCK(vp);
		MNT_ILOCK(mp);
		mp->mnt_flag |= fsflags &
		    (MNT_RELOAD | MNT_FORCE | MNT_UPDATE | MNT_SNAPSHOT | MNT_ROOTFS);
		MNT_IUNLOCK(mp);
		VOP_UNLOCK(vp, 0);
		mp->mnt_optnew = fsdata;
		vfs_mergeopts(mp->mnt_optnew, mp->mnt_opt);
	} else {
		/*
		 * If the user is not root, ensure that they own the directory
		 * onto which we are attempting to mount.
		 */
		error = VOP_GETATTR(vp, &va, td->td_ucred);
		if (error) {
			vput(vp);
			return (error);
		}
		if (va.va_uid != td->td_ucred->cr_uid) {
			error = priv_check_cred(td->td_ucred, PRIV_VFS_ADMIN,
			    0);
			if (error) {
				vput(vp);
				return (error);
			}
		}
		error = vinvalbuf(vp, V_SAVE, 0, 0);
		if (error != 0) {
			vput(vp);
			return (error);
		}
		if (vp->v_type != VDIR) {
			vput(vp);
			return (ENOTDIR);
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
		mp = vfs_mount_alloc(vp, vfsp, fspath, td->td_ucred);
		VOP_UNLOCK(vp, 0);

		/* XXXMAC: pass to vfs_mount_alloc? */
		mp->mnt_optnew = fsdata;
	}

	/*
	 * Set the mount level flags.
	 */
	MNT_ILOCK(mp);
	mp->mnt_flag = (mp->mnt_flag & ~MNT_UPDATEMASK) |
		(fsflags & (MNT_UPDATEMASK | MNT_FORCE | MNT_ROOTFS |
			    MNT_RDONLY));
	if ((mp->mnt_flag & MNT_ASYNC) == 0)
		mp->mnt_kern_flag &= ~MNTK_ASYNC;
	MNT_IUNLOCK(mp);
	/*
	 * Mount the filesystem.
	 * XXX The final recipients of VFS_MOUNT just overwrite the ndp they
	 * get.  No freeing of cn_pnbuf.
	 */
        error = VFS_MOUNT(mp, td);

	/*
	 * Process the export option only if we are
	 * updating mount options.
	 */
	if (!error && (fsflags & MNT_UPDATE)) {
		if (vfs_copyopt(mp->mnt_optnew, "export", &export,
		    sizeof(export)) == 0)
			error = vfs_export(mp, &export);
		else if (vfs_copyopt(mp->mnt_optnew, "export", &oexport,
			sizeof(oexport)) == 0) {
			export.ex_flags = oexport.ex_flags;
			export.ex_root = oexport.ex_root;
			export.ex_anon = oexport.ex_anon;
			export.ex_addr = oexport.ex_addr;
			export.ex_addrlen = oexport.ex_addrlen;
			export.ex_mask = oexport.ex_mask;
			export.ex_masklen = oexport.ex_masklen;
			export.ex_indexfile = oexport.ex_indexfile;
			export.ex_numsecflavors = 0;
			error = vfs_export(mp, &export);
		}
	}

	if (!error) {
		if (mp->mnt_opt != NULL)
			vfs_freeopts(mp->mnt_opt);
		mp->mnt_opt = mp->mnt_optnew;
		(void)VFS_STATFS(mp, &mp->mnt_stat, td);
	}
	/*
	 * Prevent external consumers of mount options from reading
	 * mnt_optnew.
	*/
	mp->mnt_optnew = NULL;
	if (mp->mnt_flag & MNT_UPDATE) {
		MNT_ILOCK(mp);
		if (error)
			mp->mnt_flag = (mp->mnt_flag & MNT_QUOTA) |
				(flag & ~MNT_QUOTA);
		else
			mp->mnt_flag &=	~(MNT_UPDATE | MNT_RELOAD |
					  MNT_FORCE | MNT_SNAPSHOT);
		if ((mp->mnt_flag & MNT_ASYNC) != 0 && mp->mnt_noasync == 0)
			mp->mnt_kern_flag |= MNTK_ASYNC;
		else
			mp->mnt_kern_flag &= ~MNTK_ASYNC;
		MNT_IUNLOCK(mp);
		if ((mp->mnt_flag & MNT_RDONLY) == 0) {
			if (mp->mnt_syncer == NULL)
				error = vfs_allocate_syncvnode(mp);
		} else {
			if (mp->mnt_syncer != NULL)
				vrele(mp->mnt_syncer);
			mp->mnt_syncer = NULL;
		}
		vfs_unbusy(mp);
		VI_LOCK(vp);
		vp->v_iflag &= ~VI_MOUNT;
		VI_UNLOCK(vp);
		vrele(vp);
		return (error);
	}
	MNT_ILOCK(mp);
	if ((mp->mnt_flag & MNT_ASYNC) != 0 && mp->mnt_noasync == 0)
		mp->mnt_kern_flag |= MNTK_ASYNC;
	else
		mp->mnt_kern_flag &= ~MNTK_ASYNC;
	MNT_IUNLOCK(mp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
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
		VOP_UNLOCK(vp, 0);
		if ((mp->mnt_flag & MNT_RDONLY) == 0)
			error = vfs_allocate_syncvnode(mp);
		vfs_unbusy(mp);
		if (error)
			vrele(vp);
	} else {
		VI_LOCK(vp);
		vp->v_iflag &= ~VI_MOUNT;
		VI_UNLOCK(vp);
		vfs_unbusy(mp);
		vfs_mount_destroy(mp);
		vput(vp);
	}
	return (error);
}

/*
 * Unmount a filesystem.
 *
 * Note: unmount takes a path to the vnode mounted on as argument, not
 * special file (as before).
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

	if (jailed(td->td_ucred) || usermount == 0) {
		error = priv_check(td, PRIV_VFS_UNMOUNT);
		if (error)
			return (error);
	}

	pathbuf = malloc(MNAMELEN, M_TEMP, M_WAITOK);
	error = copyinstr(uap->path, pathbuf, MNAMELEN, NULL);
	if (error) {
		free(pathbuf, M_TEMP);
		return (error);
	}
	AUDIT_ARG(upath, td, pathbuf, ARG_UPATH1);
	mtx_lock(&Giant);
	if (uap->flags & MNT_BYFSID) {
		/* Decode the filesystem ID. */
		if (sscanf(pathbuf, "FSID:%d:%d", &id0, &id1) != 2) {
			mtx_unlock(&Giant);
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
		mtx_unlock(&Giant);
		return ((uap->flags & MNT_BYFSID) ? ENOENT : EINVAL);
	}

	/*
	 * Don't allow unmounting the root filesystem.
	 */
	if (mp->mnt_flag & MNT_ROOTFS) {
		mtx_unlock(&Giant);
		return (EINVAL);
	}
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
	int mnt_gen_r;

	mtx_assert(&Giant, MA_OWNED);

	if ((coveredvp = mp->mnt_vnodecovered) != NULL) {
		mnt_gen_r = mp->mnt_gen;
		VI_LOCK(coveredvp);
		vholdl(coveredvp);
		vn_lock(coveredvp, LK_EXCLUSIVE | LK_INTERLOCK | LK_RETRY);
		vdrop(coveredvp);
		/*
		 * Check for mp being unmounted while waiting for the
		 * covered vnode lock.
		 */
		if (coveredvp->v_mountedhere != mp ||
		    coveredvp->v_mountedhere->mnt_gen != mnt_gen_r) {
			VOP_UNLOCK(coveredvp, 0);
			return (EBUSY);
		}
	}
	/*
	 * Only privileged root, or (if MNT_USER is set) the user that did the
	 * original mount is permitted to unmount this filesystem.
	 */
	error = vfs_suser(mp, td);
	if (error) {
		if (coveredvp)
			VOP_UNLOCK(coveredvp, 0);
		return (error);
	}

	MNT_ILOCK(mp);
	if (mp->mnt_kern_flag & MNTK_UNMOUNT) {
		MNT_IUNLOCK(mp);
		if (coveredvp)
			VOP_UNLOCK(coveredvp, 0);
		return (EBUSY);
	}
	mp->mnt_kern_flag |= MNTK_UNMOUNT | MNTK_NOINSMNTQ;
	/* Allow filesystems to detect that a forced unmount is in progress. */
	if (flags & MNT_FORCE)
		mp->mnt_kern_flag |= MNTK_UNMOUNTF;
	error = 0;
	if (mp->mnt_lockref) {
		if (flags & MNT_FORCE) {
			mp->mnt_kern_flag &= ~(MNTK_UNMOUNT | MNTK_NOINSMNTQ |
			    MNTK_UNMOUNTF);
			if (mp->mnt_kern_flag & MNTK_MWAIT) {
				mp->mnt_kern_flag &= ~MNTK_MWAIT;
				wakeup(mp);
			}
			MNT_IUNLOCK(mp);
			if (coveredvp)
				VOP_UNLOCK(coveredvp, 0);
			return (EBUSY);
		}
		mp->mnt_kern_flag |= MNTK_DRAINING;
		error = msleep(&mp->mnt_lockref, MNT_MTX(mp), PVFS,
		    "mount drain", 0);
	}
	MNT_IUNLOCK(mp);
	KASSERT(mp->mnt_lockref == 0,
	    ("%s: invalid lock refcount in the drain path @ %s:%d",
	    __func__, __FILE__, __LINE__));
	KASSERT(error == 0,
	    ("%s: invalid return value for msleep in the drain path @ %s:%d",
	    __func__, __FILE__, __LINE__));
	vn_start_write(NULL, &mp, V_WAIT);

	if (mp->mnt_flag & MNT_EXPUBLIC)
		vfs_setpublicfs(NULL, NULL, NULL);

	vfs_msync(mp, MNT_WAIT);
	MNT_ILOCK(mp);
	async_flag = mp->mnt_flag & MNT_ASYNC;
	mp->mnt_flag &= ~MNT_ASYNC;
	mp->mnt_kern_flag &= ~MNTK_ASYNC;
	MNT_IUNLOCK(mp);
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
	/*
	 * If we failed to flush the dirty blocks for this mount point,
	 * undo all the cdir/rdir and rootvnode changes we made above.
	 * Unless we failed to do so because the device is reporting that
	 * it doesn't exist anymore.
	 */
	if (error && error != ENXIO) {
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
		MNT_ILOCK(mp);
		mp->mnt_kern_flag &= ~MNTK_NOINSMNTQ;
		if ((mp->mnt_flag & MNT_RDONLY) == 0 && mp->mnt_syncer == NULL) {
			MNT_IUNLOCK(mp);
			(void) vfs_allocate_syncvnode(mp);
			MNT_ILOCK(mp);
		}
		mp->mnt_kern_flag &= ~(MNTK_UNMOUNT | MNTK_UNMOUNTF);
		mp->mnt_flag |= async_flag;
		if ((mp->mnt_flag & MNT_ASYNC) != 0 && mp->mnt_noasync == 0)
			mp->mnt_kern_flag |= MNTK_ASYNC;
		if (mp->mnt_kern_flag & MNTK_MWAIT) {
			mp->mnt_kern_flag &= ~MNTK_MWAIT;
			wakeup(mp);
		}
		MNT_IUNLOCK(mp);
		if (coveredvp)
			VOP_UNLOCK(coveredvp, 0);
		return (error);
	}
	mtx_lock(&mountlist_mtx);
	TAILQ_REMOVE(&mountlist, mp, mnt_list);
	mtx_unlock(&mountlist_mtx);
	if (coveredvp != NULL) {
		coveredvp->v_mountedhere = NULL;
		vput(coveredvp);
	}
	vfs_event_signal(NULL, VQ_UNMOUNT, 0);
	vfs_mount_destroy(mp);
	return (0);
}

/*
 * ---------------------------------------------------------------------
 * Mounting of root filesystem
 *
 */

struct root_hold_token {
	const char			*who;
	LIST_ENTRY(root_hold_token)	list;
};

static LIST_HEAD(, root_hold_token)	root_holds =
    LIST_HEAD_INITIALIZER(&root_holds);

static int root_mount_complete;

/*
 * Hold root mount.
 */
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

/*
 * Release root mount.
 */
void
root_mount_rel(struct root_hold_token *h)
{

	mtx_lock(&mountlist_mtx);
	LIST_REMOVE(h, list);
	wakeup(&root_holds);
	mtx_unlock(&mountlist_mtx);
	free(h, M_DEVBUF);
}

/*
 * Wait for all subsystems to release root mount.
 */
static void
root_mount_prepare(void)
{
	struct root_hold_token *h;

	for (;;) {
		DROP_GIANT();
		g_waitidle();
		PICKUP_GIANT();
		mtx_lock(&mountlist_mtx);
		if (LIST_EMPTY(&root_holds)) {
			mtx_unlock(&mountlist_mtx);
			break;
		}
		printf("Root mount waiting for:");
		LIST_FOREACH(h, &root_holds, list)
			printf(" %s", h->who);
		printf("\n");
		msleep(&root_holds, &mountlist_mtx, PZERO | PDROP, "roothold",
		    hz);
	}
}

/*
 * Root was mounted, share the good news.
 */
static void
root_mount_done(void)
{

	/*
	 * Use a mutex to prevent the wakeup being missed and waiting for
	 * an extra 1 second sleep.
	 */
	mtx_lock(&mountlist_mtx);
	root_mount_complete = 1;
	wakeup(&root_mount_complete);
	mtx_unlock(&mountlist_mtx);
}

/*
 * Return true if root is already mounted.
 */
int
root_mounted(void)
{

	/* No mutex is acquired here because int stores are atomic. */
	return (root_mount_complete);
}

/*
 * Wait until root is mounted.
 */
void
root_mount_wait(void)
{

	/*
	 * Panic on an obvious deadlock - the function can't be called from
	 * a thread which is doing the whole SYSINIT stuff.
	 */
	KASSERT(curthread->td_proc->p_pid != 0,
	    ("root_mount_wait: cannot be called from the swapper thread"));
	mtx_lock(&mountlist_mtx);
	while (!root_mount_complete) {
		msleep(&root_mount_complete, &mountlist_mtx, PZERO, "rootwait",
		    hz);
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
	FILEDESC_XLOCK(p->p_fd);

	if (p->p_fd->fd_cdir != NULL)
		vrele(p->p_fd->fd_cdir);
	p->p_fd->fd_cdir = rootvnode;
	VREF(rootvnode);

	if (p->p_fd->fd_rdir != NULL)
		vrele(p->p_fd->fd_rdir);
	p->p_fd->fd_rdir = rootvnode;
	VREF(rootvnode);

	FILEDESC_XUNLOCK(p->p_fd);

	VOP_UNLOCK(rootvnode, 0);

	EVENTHANDLER_INVOKE(mountroot);
}

/*
 * Mount /devfs as our root filesystem, but do not put it on the mountlist
 * yet.  Create a /dev -> / symlink so that absolute pathnames will lookup.
 */

static void
devfs_first(void)
{
	struct thread *td = curthread;
	struct vfsoptlist *opts;
	struct vfsconf *vfsp;
	struct mount *mp = NULL;
	int error;

	vfsp = vfs_byname("devfs");
	KASSERT(vfsp != NULL, ("Could not find devfs by name"));
	if (vfsp == NULL)
		return;

	mp = vfs_mount_alloc(NULLVP, vfsp, "/dev", td->td_ucred);

	error = VFS_MOUNT(mp, td);
	KASSERT(error == 0, ("VFS_MOUNT(devfs) failed %d", error));
	if (error)
		return;

	opts = malloc(sizeof(struct vfsoptlist), M_MOUNT, M_WAITOK);
	TAILQ_INIT(opts);
	mp->mnt_opt = opts;

	mtx_lock(&mountlist_mtx);
	TAILQ_INSERT_HEAD(&mountlist, mp, mnt_list);
	mtx_unlock(&mountlist_mtx);

	set_rootvnode(td);

	error = kern_symlink(td, "/", "dev", UIO_SYSSPACE);
	if (error)
		printf("kern_symlink /dev -> / returns %d\n", error);
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
	VI_UNLOCK(dvp);
	dvp->v_mountedhere = NULL;

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
	error = vinvalbuf(vp, V_SAVE, 0, 0);
	if (error) {
		vput(vp);
	}
	cache_purge(vp);
	mp->mnt_vnodecovered = vp;
	vp->v_mountedhere = mp;
	mtx_lock(&mountlist_mtx);
	TAILQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	mtx_unlock(&mountlist_mtx);
	VOP_UNLOCK(vp, 0);
	vput(dvp);
	vfs_unbusy(mp);

	/* Unlink the no longer needed /dev/dev -> / symlink */
	kern_unlink(td, "/dev/dev", UIO_SYSSPACE);
}

/*
 * Report errors during filesystem mounting.
 */
void
vfs_mount_error(struct mount *mp, const char *fmt, ...)
{
	struct vfsoptlist *moptlist = mp->mnt_optnew;
	va_list ap;
	int error, len;
	char *errmsg;

	error = vfs_getopt(moptlist, "errmsg", (void **)&errmsg, &len);
	if (error || errmsg == NULL || len <= 0)
		return;

	va_start(ap, fmt);
	vsnprintf(errmsg, (size_t)len, fmt, ap);
	va_end(ap);
}

/*
 * Find and mount the root filesystem
 */
void
vfs_mountroot(void)
{
	char *cp;
	int error, i, asked = 0;

	root_mount_prepare();

	mount_zone = uma_zcreate("Mountpoints", sizeof(struct mount),
	    NULL, NULL, mount_init, mount_fini,
	    UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	devfs_first();

	/*
	 * We are booted with instructions to prompt for the root filesystem.
	 */
	if (boothowto & RB_ASKNAME) {
		if (!vfs_mountroot_ask())
			goto mounted;
		asked = 1;
	}

	/*
	 * The root filesystem information is compiled in, and we are
	 * booted with instructions to use it.
	 */
	if (ctrootdevname != NULL && (boothowto & RB_DFLTROOT)) {
		if (!vfs_mountroot_try(ctrootdevname))
			goto mounted;
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
				goto mounted;
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
			goto mounted;
	}

	/*
	 * Try values that may have been computed by code during boot
	 */
	if (!vfs_mountroot_try(rootdevnames[0]))
		goto mounted;
	if (!vfs_mountroot_try(rootdevnames[1]))
		goto mounted;

	/*
	 * If we (still) have a compiled-in default, try it.
	 */
	if (ctrootdevname != NULL)
		if (!vfs_mountroot_try(ctrootdevname))
			goto mounted;
	/*
	 * Everything so far has failed, prompt on the console if we haven't
	 * already tried that.
	 */
	if (!asked)
		if (!vfs_mountroot_ask())
			goto mounted;

	panic("Root mount failed, startup aborted.");

mounted:
	root_mount_done();
}

/*
 * Mount (mountfrom) as the root filesystem.
 */
static int
vfs_mountroot_try(const char *mountfrom)
{
	struct mount	*mp;
	char		*vfsname, *path;
	time_t		timebase;
	int		error;
	char		patt[32];

	vfsname = NULL;
	path    = NULL;
	mp      = NULL;
	error   = EINVAL;

	if (mountfrom == NULL)
		return (error);		/* don't complain */
	printf("Trying to mount root from %s\n", mountfrom);

	/* parse vfs name and path */
	vfsname = malloc(MFSNAMELEN, M_MOUNT, M_WAITOK);
	path = malloc(MNAMELEN, M_MOUNT, M_WAITOK);
	vfsname[0] = path[0] = 0;
	sprintf(patt, "%%%d[a-z0-9]:%%%ds", MFSNAMELEN, MNAMELEN);
	if (sscanf(mountfrom, patt, vfsname, path) < 1)
		goto out;

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

		/*
		 * Iterate over all currently mounted file systems and use
		 * the time stamp found to check and/or initialize the RTC.
		 * Typically devfs has no time stamp and the only other FS
		 * is the actual / FS.
		 * Call inittodr() only once and pass it the largest of the
		 * timestamps we encounter.
		 */
		timebase = 0;
		do {
			if (mp->mnt_time > timebase)
				timebase = mp->mnt_time;
			mp = TAILQ_NEXT(mp, mnt_list);
		} while (mp != NULL);
		inittodr(timebase);

		devfs_fixup(curthread);
	}
out:
	free(path, M_MOUNT);
	free(vfsname, M_MOUNT);
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
#if defined(__amd64__) || defined(__i386__) || defined(__ia64__)
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
	char errmsg[255];
	const char **t, *p, *q;
	int ret = 0;

	TAILQ_FOREACH(opt, opts, link) {
		p = opt->name;
		q = NULL;
		if (p[0] == 'n' && p[1] == 'o')
			q = p + 2;
		for(t = global_opts; *t != NULL; t++) {
			if (strcmp(*t, p) == 0)
				break;
			if (q != NULL) {
				if (strcmp(*t, q) == 0)
					break;
			}
		}
		if (*t != NULL)
			continue;
		for(t = legal; *t != NULL; t++) {
			if (strcmp(*t, p) == 0)
				break;
			if (q != NULL) {
				if (strcmp(*t, q) == 0)
					break;
			}
		}
		if (*t != NULL)
			continue;
		snprintf(errmsg, sizeof(errmsg),
		    "mount option <%s> is unknown", p);
		printf("%s\n", errmsg);
		ret = EINVAL;
	}
	if (ret != 0) {
		TAILQ_FOREACH(opt, opts, link) {
			if (strcmp(opt->name, "errmsg") == 0) {
				strncpy((char *)opt->value, errmsg, opt->len);
			}
		}
	}
	return (ret);
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

static int
vfs_getopt_pos(struct vfsoptlist *opts, const char *name)
{
	struct vfsopt *opt;
	int i;

	if (opts == NULL)
		return (-1);

	i = 0;
	TAILQ_FOREACH(opt, opts, link) {
		if (strcmp(name, opt->name) == 0)
			return (i);
		++i;
	}
	return (-1);
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
	*error = ENOENT;
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
		if (opt->len == 0 || opt->value == NULL)
			return (0);
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
__mnt_vnode_next(struct vnode **mvp, struct mount *mp)
{
	struct vnode *vp;

	mtx_assert(MNT_MTX(mp), MA_OWNED);

	KASSERT((*mvp)->v_mount == mp, ("marker vnode mount list mismatch"));
	if ((*mvp)->v_yield++ == 500) {
		MNT_IUNLOCK(mp);
		(*mvp)->v_yield = 0;
		uio_yield();
		MNT_ILOCK(mp);
	}
	vp = TAILQ_NEXT(*mvp, v_nmntvnodes);
	while (vp != NULL && vp->v_type == VMARKER)
		vp = TAILQ_NEXT(vp, v_nmntvnodes);

	/* Check if we are done */
	if (vp == NULL) {
		__mnt_vnode_markerfree(mvp, mp);
		return (NULL);
	}
	TAILQ_REMOVE(&mp->mnt_nvnodelist, *mvp, v_nmntvnodes);
	TAILQ_INSERT_AFTER(&mp->mnt_nvnodelist, vp, *mvp, v_nmntvnodes);
	return (vp);
}

struct vnode *
__mnt_vnode_first(struct vnode **mvp, struct mount *mp)
{
	struct vnode *vp;

	mtx_assert(MNT_MTX(mp), MA_OWNED);

	vp = TAILQ_FIRST(&mp->mnt_nvnodelist);
	while (vp != NULL && vp->v_type == VMARKER)
		vp = TAILQ_NEXT(vp, v_nmntvnodes);

	/* Check if we are done */
	if (vp == NULL) {
		*mvp = NULL;
		return (NULL);
	}
	MNT_REF(mp);
	MNT_IUNLOCK(mp);
	*mvp = (struct vnode *) malloc(sizeof(struct vnode),
				       M_VNODE_MARKER,
				       M_WAITOK | M_ZERO);
	MNT_ILOCK(mp);
	(*mvp)->v_type = VMARKER;

	vp = TAILQ_FIRST(&mp->mnt_nvnodelist);
	while (vp != NULL && vp->v_type == VMARKER)
		vp = TAILQ_NEXT(vp, v_nmntvnodes);

	/* Check if we are done */
	if (vp == NULL) {
		MNT_IUNLOCK(mp);
		free(*mvp, M_VNODE_MARKER);
		MNT_ILOCK(mp);
		*mvp = NULL;
		MNT_REL(mp);
		return (NULL);
	}
	(*mvp)->v_mount = mp;
	TAILQ_INSERT_AFTER(&mp->mnt_nvnodelist, vp, *mvp, v_nmntvnodes);
	return (vp);
}


void
__mnt_vnode_markerfree(struct vnode **mvp, struct mount *mp)
{

	if (*mvp == NULL)
		return;

	mtx_assert(MNT_MTX(mp), MA_OWNED);

	KASSERT((*mvp)->v_mount == mp, ("marker vnode mount list mismatch"));
	TAILQ_REMOVE(&mp->mnt_nvnodelist, *mvp, v_nmntvnodes);
	MNT_IUNLOCK(mp);
	free(*mvp, M_VNODE_MARKER);
	MNT_ILOCK(mp);
	*mvp = NULL;
	MNT_REL(mp);
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

	sb = sbuf_new_auto();
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
 * If length is -1, treat value as a C string.
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
		ma = mount_arg(ma, cp, vp, (vp != NULL ? -1 : 0));
	}
	va_end(ap);

	error = kernel_mount(ma, flags);
	return (error);
}
