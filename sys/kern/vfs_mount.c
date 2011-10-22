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
#include <sys/sbuf.h>
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

#define	VFS_MOUNTARG_SIZE_MAX	(1024 * 64)

static int	vfs_domount(struct thread *td, const char *fstype,
		    char *fspath, int fsflags, struct vfsoptlist **optlist);
static void	free_mntarg(struct mntarg *ma);

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

static void
vfs_mount_init(void *dummy __unused)
{

	mount_zone = uma_zcreate("Mountpoints", sizeof(struct mount), NULL,
	    NULL, mount_init, mount_fini, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
}
SYSINIT(vfs_mount, SI_SUB_VFS, SI_ORDER_ANY, vfs_mount_init, NULL);

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

static int
vfs_isopt_ro(const char *opt)
{

	if (strcmp(opt, "ro") == 0 || strcmp(opt, "rdonly") == 0 ||
	    strcmp(opt, "norw") == 0)
		return (1);
	return (0);
}

static int
vfs_isopt_rw(const char *opt)
{

	if (strcmp(opt, "rw") == 0 || strcmp(opt, "noro") == 0)
		return (1);
	return (0);
}

/*
 * Check if options are equal (with or without the "no" prefix).
 */
static int
vfs_equalopts(const char *opt1, const char *opt2)
{
	char *p;

	/* "opt" vs. "opt" or "noopt" vs. "noopt" */
	if (strcmp(opt1, opt2) == 0)
		return (1);
	/* "noopt" vs. "opt" */
	if (strncmp(opt1, "no", 2) == 0 && strcmp(opt1 + 2, opt2) == 0)
		return (1);
	/* "opt" vs. "noopt" */
	if (strncmp(opt2, "no", 2) == 0 && strcmp(opt1, opt2 + 2) == 0)
		return (1);
	while ((p = strchr(opt1, '.')) != NULL &&
	    !strncmp(opt1, opt2, ++p - opt1)) {
		opt2 += p - opt1;
		opt1 = p;
		/* "foo.noopt" vs. "foo.opt" */
		if (strncmp(opt1, "no", 2) == 0 && strcmp(opt1 + 2, opt2) == 0)
			return (1);
		/* "foo.opt" vs. "foo.noopt" */
		if (strncmp(opt2, "no", 2) == 0 && strcmp(opt1, opt2 + 2) == 0)
			return (1);
	}
	/* "ro" / "rdonly" / "norw" / "rw" / "noro" */
	if ((vfs_isopt_ro(opt1) || vfs_isopt_rw(opt1)) &&
	    (vfs_isopt_ro(opt2) || vfs_isopt_rw(opt2)))
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
int
vfs_buildopts(struct uio *auio, struct vfsoptlist **options)
{
	struct vfsoptlist *opts;
	struct vfsopt *opt;
	size_t memused, namelen, optlen;
	unsigned int i, iovcnt;
	int error;

	opts = malloc(sizeof(struct vfsoptlist), M_MOUNT, M_WAITOK);
	TAILQ_INIT(opts);
	memused = 0;
	iovcnt = auio->uio_iovcnt;
	for (i = 0; i < iovcnt; i += 2) {
		namelen = auio->uio_iov[i].iov_len;
		optlen = auio->uio_iov[i + 1].iov_len;
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

		opt = malloc(sizeof(struct vfsopt), M_MOUNT, M_WAITOK);
		opt->name = malloc(namelen, M_MOUNT, M_WAITOK);
		opt->value = NULL;
		opt->len = 0;
		opt->pos = i / 2;
		opt->seen = 0;

		/*
		 * Do this early, so jumps to "bad" will free the current
		 * option.
		 */
		TAILQ_INSERT_TAIL(opts, opt, link);

		if (auio->uio_segflg == UIO_SYSSPACE) {
			bcopy(auio->uio_iov[i].iov_base, opt->name, namelen);
		} else {
			error = copyin(auio->uio_iov[i].iov_base, opt->name,
			    namelen);
			if (error)
				goto bad;
		}
		/* Ensure names are null-terminated strings. */
		if (namelen == 0 || opt->name[namelen - 1] != '\0') {
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
 * XXX: This function will keep a "nofoo" option in the new
 * options.  E.g, if the option's canonical name is "foo",
 * "nofoo" ends up in the mount point's active options.
 */
static void
vfs_mergeopts(struct vfsoptlist *toopts, struct vfsoptlist *oldopts)
{
	struct vfsopt *opt, *new;

	TAILQ_FOREACH(opt, oldopts, link) {
		new = malloc(sizeof(struct vfsopt), M_MOUNT, M_WAITOK);
		new->name = strdup(opt->name, M_MOUNT);
		if (opt->len != 0) {
			new->value = malloc(opt->len, M_MOUNT, M_WAITOK);
			bcopy(opt->value, new->value, opt->len);
		} else
			new->value = NULL;
		new->len = opt->len;
		new->seen = opt->seen;
		TAILQ_INSERT_HEAD(toopts, new, link);
	}
	vfs_sanitizeopts(toopts);
}

/*
 * Mount a filesystem.
 */
int
sys_nmount(td, uap)
	struct thread *td;
	struct nmount_args /* {
		struct iovec *iovp;
		unsigned int iovcnt;
		int flags;
	} */ *uap;
{
	struct uio *auio;
	int error;
	u_int iovcnt;

	AUDIT_ARG_FFLAGS(uap->flags);
	CTR4(KTR_VFS, "%s: iovp %p with iovcnt %d and flags %d", __func__,
	    uap->iovp, uap->iovcnt, uap->flags);

	/*
	 * Filter out MNT_ROOTFS.  We do not want clients of nmount() in
	 * userspace to set this flag, but we must filter it out if we want
	 * MNT_UPDATE on the root file system to work.
	 * MNT_ROOTFS should only be set by the kernel when mounting its
	 * root file system.
	 */
	uap->flags &= ~MNT_ROOTFS;

	iovcnt = uap->iovcnt;
	/*
	 * Check that we have an even number of iovec's
	 * and that we have at least two options.
	 */
	if ((iovcnt & 1) || (iovcnt < 4)) {
		CTR2(KTR_VFS, "%s: failed for invalid iovcnt %d", __func__,
		    uap->iovcnt);
		return (EINVAL);
	}

	error = copyinuio(uap->iovp, iovcnt, &auio);
	if (error) {
		CTR2(KTR_VFS, "%s: failed for invalid uio op with %d errno",
		    __func__, error);
		return (error);
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

	CTR2(KTR_VFS, "%s: mp %p", __func__, mp);
	MNT_ILOCK(mp);
	MNT_REF(mp);
	MNT_IUNLOCK(mp);
}

void
vfs_rel(struct mount *mp)
{

	CTR2(KTR_VFS, "%s: mp %p", __func__, mp);
	MNT_ILOCK(mp);
	MNT_REL(mp);
	MNT_IUNLOCK(mp);
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
	mp->mnt_kern_flag |= MNTK_REFEXPIRE;
	if (mp->mnt_kern_flag & MNTK_MWAIT) {
		mp->mnt_kern_flag &= ~MNTK_MWAIT;
		wakeup(mp);
	}
	while (mp->mnt_ref)
		msleep(mp, MNT_MTX(mp), PVFS, "mntref", 0);
	KASSERT(mp->mnt_ref == 0,
	    ("%s: invalid refcount in the drain path @ %s:%d", __func__,
	    __FILE__, __LINE__));
	if (mp->mnt_writeopcount != 0)
		panic("vfs_mount_destroy: nonzero writeopcount");
	if (mp->mnt_secondary_writes != 0)
		panic("vfs_mount_destroy: nonzero secondary_writes");
	mp->mnt_vfc->vfc_refcount--;
	if (!TAILQ_EMPTY(&mp->mnt_nvnodelist)) {
		struct vnode *vp;

		TAILQ_FOREACH(vp, &mp->mnt_nvnodelist, v_nmntvnodes)
			vprint("", vp);
		panic("unmount: dangling vnode");
	}
	if (mp->mnt_nvnodelistsize != 0)
		panic("vfs_mount_destroy: nonzero nvnodelistsize");
	if (mp->mnt_lockref != 0)
		panic("vfs_mount_destroy: nonzero lock refcount");
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
	struct vfsopt *opt, *tmp_opt;
	char *fstype, *fspath, *errmsg;
	int error, fstypelen, fspathlen, errmsg_len, errmsg_pos;

	errmsg = fspath = NULL;
	errmsg_len = fspathlen = 0;
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
		else if (strcmp(opt->name, "noro") == 0)
			fsflags &= ~MNT_RDONLY;
		else if (strcmp(opt->name, "rw") == 0)
			fsflags &= ~MNT_RDONLY;
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
	 * Be ultra-paranoid about making sure the type and fspath
	 * variables will fit in our mp buffers, including the
	 * terminating NUL.
	 */
	if (fstypelen >= MFSNAMELEN - 1 || fspathlen >= MNAMELEN - 1) {
		error = ENAMETOOLONG;
		goto bail;
	}

	error = vfs_domount(td, fstype, fspath, fsflags, &optlist);
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

	if (optlist != NULL)
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
sys_mount(td, uap)
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

	AUDIT_ARG_FFLAGS(uap->flags);

	/*
	 * Filter out MNT_ROOTFS.  We do not want clients of mount() in
	 * userspace to set this flag, but we must filter it out if we want
	 * MNT_UPDATE on the root file system to work.
	 * MNT_ROOTFS should only be set by the kernel when mounting its
	 * root file system.
	 */
	uap->flags &= ~MNT_ROOTFS;

	fstype = malloc(MFSNAMELEN, M_TEMP, M_WAITOK);
	error = copyinstr(uap->type, fstype, MFSNAMELEN, NULL);
	if (error) {
		free(fstype, M_TEMP);
		return (error);
	}

	AUDIT_ARG_TEXT(fstype);
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

	error = vfsp->vfc_vfsops->vfs_cmount(ma, uap->data, uap->flags);
	mtx_unlock(&Giant);
	return (error);
}

/*
 * vfs_domount_first(): first file system mount (not update)
 */
static int
vfs_domount_first(
	struct thread *td,		/* Calling thread. */
	struct vfsconf *vfsp,		/* File system type. */
	char *fspath,			/* Mount path. */
	struct vnode *vp,		/* Vnode to be covered. */
	int fsflags,			/* Flags common to all filesystems. */
	struct vfsoptlist **optlist	/* Options local to the filesystem. */
	)
{
	struct vattr va;
	struct mount *mp;
	struct vnode *newdp;
	int error;

	mtx_assert(&Giant, MA_OWNED);
	ASSERT_VOP_ELOCKED(vp, __func__);
	KASSERT((fsflags & MNT_UPDATE) == 0, ("MNT_UPDATE shouldn't be here"));

	/*
	 * If the user is not root, ensure that they own the directory
	 * onto which we are attempting to mount.
	 */
	error = VOP_GETATTR(vp, &va, td->td_ucred);
	if (error == 0 && va.va_uid != td->td_ucred->cr_uid)
		error = priv_check_cred(td->td_ucred, PRIV_VFS_ADMIN, 0);
	if (error == 0)
		error = vinvalbuf(vp, V_SAVE, 0, 0);
	if (error == 0 && vp->v_type != VDIR)
		error = ENOTDIR;
	if (error == 0) {
		VI_LOCK(vp);
		if ((vp->v_iflag & VI_MOUNT) == 0 && vp->v_mountedhere == NULL)
			vp->v_iflag |= VI_MOUNT;
		else
			error = EBUSY;
		VI_UNLOCK(vp);
	}
	if (error != 0) {
		vput(vp);
		return (error);
	}
	VOP_UNLOCK(vp, 0);

	/* Allocate and initialize the filesystem. */
	mp = vfs_mount_alloc(vp, vfsp, fspath, td->td_ucred);
	/* XXXMAC: pass to vfs_mount_alloc? */
	mp->mnt_optnew = *optlist;
	/* Set the mount level flags. */
	mp->mnt_flag = (fsflags & (MNT_UPDATEMASK | MNT_ROOTFS | MNT_RDONLY));

	/*
	 * Mount the filesystem.
	 * XXX The final recipients of VFS_MOUNT just overwrite the ndp they
	 * get.  No freeing of cn_pnbuf.
	 */
	error = VFS_MOUNT(mp);
	if (error != 0) {
		vfs_unbusy(mp);
		vfs_mount_destroy(mp);
		VI_LOCK(vp);
		vp->v_iflag &= ~VI_MOUNT;
		VI_UNLOCK(vp);
		vrele(vp);
		return (error);
	}

	if (mp->mnt_opt != NULL)
		vfs_freeopts(mp->mnt_opt);
	mp->mnt_opt = mp->mnt_optnew;
	*optlist = NULL;
	(void)VFS_STATFS(mp, &mp->mnt_stat);

	/*
	 * Prevent external consumers of mount options from reading mnt_optnew.
	 */
	mp->mnt_optnew = NULL;

	MNT_ILOCK(mp);
	if ((mp->mnt_flag & MNT_ASYNC) != 0 && mp->mnt_noasync == 0)
		mp->mnt_kern_flag |= MNTK_ASYNC;
	else
		mp->mnt_kern_flag &= ~MNTK_ASYNC;
	MNT_IUNLOCK(mp);

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	cache_purge(vp);
	VI_LOCK(vp);
	vp->v_iflag &= ~VI_MOUNT;
	VI_UNLOCK(vp);
	vp->v_mountedhere = mp;
	/* Place the new filesystem at the end of the mount list. */
	mtx_lock(&mountlist_mtx);
	TAILQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	mtx_unlock(&mountlist_mtx);
	vfs_event_signal(NULL, VQ_MOUNT, 0);
	if (VFS_ROOT(mp, LK_EXCLUSIVE, &newdp))
		panic("mount: lost mount");
	VOP_UNLOCK(newdp, 0);
	VOP_UNLOCK(vp, 0);
	mountcheckdirs(vp, newdp);
	vrele(newdp);
	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		vfs_allocate_syncvnode(mp);
	vfs_unbusy(mp);
	return (0);
}

/*
 * vfs_domount_update(): update of mounted file system
 */
static int
vfs_domount_update(
	struct thread *td,		/* Calling thread. */
	struct vnode *vp,		/* Mount point vnode. */
	int fsflags,			/* Flags common to all filesystems. */
	struct vfsoptlist **optlist	/* Options local to the filesystem. */
	)
{
	struct oexport_args oexport;
	struct export_args export;
	struct mount *mp;
	int error, export_error, flag;

	mtx_assert(&Giant, MA_OWNED);
	ASSERT_VOP_ELOCKED(vp, __func__);
	KASSERT((fsflags & MNT_UPDATE) != 0, ("MNT_UPDATE should be here"));

	if ((vp->v_vflag & VV_ROOT) == 0) {
		vput(vp);
		return (EINVAL);
	}
	mp = vp->v_mount;
	/*
	 * We only allow the filesystem to be reloaded if it
	 * is currently mounted read-only.
	 */
	flag = mp->mnt_flag;
	if ((fsflags & MNT_RELOAD) != 0 && (flag & MNT_RDONLY) == 0) {
		vput(vp);
		return (EOPNOTSUPP);	/* Needs translation */
	}
	/*
	 * Only privileged root, or (if MNT_USER is set) the user that
	 * did the original mount is permitted to update it.
	 */
	error = vfs_suser(mp, td);
	if (error != 0) {
		vput(vp);
		return (error);
	}
	if (vfs_busy(mp, MBF_NOWAIT)) {
		vput(vp);
		return (EBUSY);
	}
	VI_LOCK(vp);
	if ((vp->v_iflag & VI_MOUNT) != 0 || vp->v_mountedhere != NULL) {
		VI_UNLOCK(vp);
		vfs_unbusy(mp);
		vput(vp);
		return (EBUSY);
	}
	vp->v_iflag |= VI_MOUNT;
	VI_UNLOCK(vp);
	VOP_UNLOCK(vp, 0);

	MNT_ILOCK(mp);
	mp->mnt_flag &= ~MNT_UPDATEMASK;
	mp->mnt_flag |= fsflags & (MNT_RELOAD | MNT_FORCE | MNT_UPDATE |
	    MNT_SNAPSHOT | MNT_ROOTFS | MNT_UPDATEMASK | MNT_RDONLY);
	if ((mp->mnt_flag & MNT_ASYNC) == 0)
		mp->mnt_kern_flag &= ~MNTK_ASYNC;
	MNT_IUNLOCK(mp);
	mp->mnt_optnew = *optlist;
	vfs_mergeopts(mp->mnt_optnew, mp->mnt_opt);

	/*
	 * Mount the filesystem.
	 * XXX The final recipients of VFS_MOUNT just overwrite the ndp they
	 * get.  No freeing of cn_pnbuf.
	 */
	error = VFS_MOUNT(mp);

	export_error = 0;
	if (error == 0) {
		/* Process the export option. */
		if (vfs_copyopt(mp->mnt_optnew, "export", &export,
		    sizeof(export)) == 0) {
			export_error = vfs_export(mp, &export);
		} else if (vfs_copyopt(mp->mnt_optnew, "export", &oexport,
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
			export_error = vfs_export(mp, &export);
		}
	}

	MNT_ILOCK(mp);
	if (error == 0) {
		mp->mnt_flag &=	~(MNT_UPDATE | MNT_RELOAD | MNT_FORCE |
		    MNT_SNAPSHOT);
	} else {
		/*
		 * If we fail, restore old mount flags. MNT_QUOTA is special,
		 * because it is not part of MNT_UPDATEMASK, but it could have
		 * changed in the meantime if quotactl(2) was called.
		 * All in all we want current value of MNT_QUOTA, not the old
		 * one.
		 */
		mp->mnt_flag = (mp->mnt_flag & MNT_QUOTA) | (flag & ~MNT_QUOTA);
	}
	if ((mp->mnt_flag & MNT_ASYNC) != 0 && mp->mnt_noasync == 0)
		mp->mnt_kern_flag |= MNTK_ASYNC;
	else
		mp->mnt_kern_flag &= ~MNTK_ASYNC;
	MNT_IUNLOCK(mp);

	if (error != 0)
		goto end;

	if (mp->mnt_opt != NULL)
		vfs_freeopts(mp->mnt_opt);
	mp->mnt_opt = mp->mnt_optnew;
	*optlist = NULL;
	(void)VFS_STATFS(mp, &mp->mnt_stat);
	/*
	 * Prevent external consumers of mount options from reading
	 * mnt_optnew.
	 */
	mp->mnt_optnew = NULL;

	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		vfs_allocate_syncvnode(mp);
	else
		vfs_deallocate_syncvnode(mp);
end:
	vfs_unbusy(mp);
	VI_LOCK(vp);
	vp->v_iflag &= ~VI_MOUNT;
	VI_UNLOCK(vp);
	vrele(vp);
	return (error != 0 ? error : export_error);
}

/*
 * vfs_domount(): actually attempt a filesystem mount.
 */
static int
vfs_domount(
	struct thread *td,		/* Calling thread. */
	const char *fstype,		/* Filesystem type. */
	char *fspath,			/* Mount path. */
	int fsflags,			/* Flags common to all filesystems. */
	struct vfsoptlist **optlist	/* Options local to the filesystem. */
	)
{
	struct vfsconf *vfsp;
	struct nameidata nd;
	struct vnode *vp;
	int error;

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
	 * Get vnode to be covered or mount point's vnode in case of MNT_UPDATE.
	 */
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | MPSAFE | AUDITVNODE1,
	    UIO_SYSSPACE, fspath, td);
	error = namei(&nd);
	if (error != 0)
		return (error);
	if (!NDHASGIANT(&nd))
		mtx_lock(&Giant);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;
	if ((fsflags & MNT_UPDATE) == 0) {
		error = vfs_domount_first(td, vfsp, fspath, vp, fsflags,
		    optlist);
	} else {
		error = vfs_domount_update(td, vp, fsflags, optlist);
	}
	mtx_unlock(&Giant);

	ASSERT_VI_UNLOCKED(vp, __func__);
	ASSERT_VOP_UNLOCKED(vp, __func__);

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
sys_unmount(td, uap)
	struct thread *td;
	register struct unmount_args /* {
		char *path;
		int flags;
	} */ *uap;
{
	struct mount *mp;
	char *pathbuf;
	int error, id0, id1;

	AUDIT_ARG_VALUE(uap->flags);
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
	mtx_lock(&Giant);
	if (uap->flags & MNT_BYFSID) {
		AUDIT_ARG_TEXT(pathbuf);
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
		AUDIT_ARG_UPATH1(td, pathbuf);
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
	vfs_deallocate_syncvnode(mp);
	/*
	 * For forced unmounts, move process cdir/rdir refs on the fs root
	 * vnode to the covered vnode.  For non-forced unmounts we want
	 * such references to cause an EBUSY error.
	 */
	if ((flags & MNT_FORCE) &&
	    VFS_ROOT(mp, LK_EXCLUSIVE, &fsrootvp) == 0) {
		if (mp->mnt_vnodecovered != NULL)
			mountcheckdirs(fsrootvp, mp->mnt_vnodecovered);
		if (fsrootvp == rootvnode) {
			vrele(rootvnode);
			rootvnode = NULL;
		}
		vput(fsrootvp);
	}
	if (((mp->mnt_flag & MNT_RDONLY) ||
	     (error = VFS_SYNC(mp, MNT_WAIT)) == 0) || (flags & MNT_FORCE) != 0)
		error = VFS_UNMOUNT(mp, flags);
	vn_finished_write(mp);
	/*
	 * If we failed to flush the dirty blocks for this mount point,
	 * undo all the cdir/rdir and rootvnode changes we made above.
	 * Unless we failed to do so because the device is reporting that
	 * it doesn't exist anymore.
	 */
	if (error && error != ENXIO) {
		if ((flags & MNT_FORCE) &&
		    VFS_ROOT(mp, LK_EXCLUSIVE, &fsrootvp) == 0) {
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
		if ((mp->mnt_flag & MNT_RDONLY) == 0) {
			MNT_IUNLOCK(mp);
			vfs_allocate_syncvnode(mp);
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

void
vfs_opterror(struct vfsoptlist *opts, const char *fmt, ...)
{
	va_list ap;
	int error, len;
	char *errmsg;

	error = vfs_getopt(opts, "errmsg", (void **)&errmsg, &len);
	if (error || errmsg == NULL || len <= 0)
		return;

	va_start(ap, fmt);
	vsnprintf(errmsg, (size_t)len, fmt, ap);
	va_end(ap);
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
		ret = EINVAL;
	}
	if (ret != 0) {
		TAILQ_FOREACH(opt, opts, link) {
			if (strcmp(opt->name, "errmsg") == 0) {
				strncpy((char *)opt->value, errmsg, opt->len);
				break;
			}
		}
		if (opt == NULL)
			printf("%s\n", errmsg);
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
			opt->seen = 1;
			if (len != NULL)
				*len = opt->len;
			if (buf != NULL)
				*buf = opt->value;
			return (0);
		}
	}
	return (ENOENT);
}

int
vfs_getopt_pos(struct vfsoptlist *opts, const char *name)
{
	struct vfsopt *opt;

	if (opts == NULL)
		return (-1);

	TAILQ_FOREACH(opt, opts, link) {
		if (strcmp(name, opt->name) == 0) {
			opt->seen = 1;
			return (opt->pos);
		}
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
		opt->seen = 1;
		if (opt->len == 0 ||
		    ((char *)opt->value)[opt->len - 1] != '\0') {
			*error = EINVAL;
			return (NULL);
		}
		return (opt->value);
	}
	*error = ENOENT;
	return (NULL);
}

int
vfs_flagopt(struct vfsoptlist *opts, const char *name, uint64_t *w,
	uint64_t val)
{
	struct vfsopt *opt;

	TAILQ_FOREACH(opt, opts, link) {
		if (strcmp(name, opt->name) == 0) {
			opt->seen = 1;
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
		opt->seen = 1;
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

int
vfs_setopt(struct vfsoptlist *opts, const char *name, void *value, int len)
{
	struct vfsopt *opt;

	TAILQ_FOREACH(opt, opts, link) {
		if (strcmp(name, opt->name) != 0)
			continue;
		opt->seen = 1;
		if (opt->value == NULL)
			opt->len = len;
		else {
			if (opt->len != len)
				return (EINVAL);
			bcopy(value, opt->value, len);
		}
		return (0);
	}
	return (ENOENT);
}

int
vfs_setopt_part(struct vfsoptlist *opts, const char *name, void *value, int len)
{
	struct vfsopt *opt;

	TAILQ_FOREACH(opt, opts, link) {
		if (strcmp(name, opt->name) != 0)
			continue;
		opt->seen = 1;
		if (opt->value == NULL)
			opt->len = len;
		else {
			if (opt->len < len)
				return (EINVAL);
			opt->len = len;
			bcopy(value, opt->value, len);
		}
		return (0);
	}
	return (ENOENT);
}

int
vfs_setopts(struct vfsoptlist *opts, const char *name, const char *value)
{
	struct vfsopt *opt;

	TAILQ_FOREACH(opt, opts, link) {
		if (strcmp(name, opt->name) != 0)
			continue;
		opt->seen = 1;
		if (opt->value == NULL)
			opt->len = strlen(value) + 1;
		else if (strlcpy(opt->value, value, opt->len) >= opt->len)
			return (EINVAL);
		return (0);
	}
	return (ENOENT);
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
			opt->seen = 1;
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
	if (should_yield()) {
		MNT_IUNLOCK(mp);
		kern_yield(PRI_UNCHANGED);
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
__vfs_statfs(struct mount *mp, struct statfs *sbp)
{
	int error;

	error = mp->mnt_op->vfs_statfs(mp, &mp->mnt_stat);
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

void
vfs_oexport_conv(const struct oexport_args *oexp, struct export_args *exp)
{

	bcopy(oexp, exp, sizeof(*oexp));
	exp->ex_numsecflavors = 0;
}
