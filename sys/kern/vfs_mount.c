/*-
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1999 Michael Smith
 * All rights reserved.
 * Copyright (c) 1999 Poul-Henning Kamp
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
#include <sys/cons.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/reboot.h>
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

static void	checkdirs(struct vnode *olddp, struct vnode *newdp);
static void	gets(char *cp);
static int	vfs_domount(struct thread *td, const char *fstype,
		    char *fspath, int fsflags, void *fsdata, int compat);
static int	vfs_mount_alloc(struct vnode *dvp, struct vfsconf *vfsp,
		    const char *fspath, struct thread *td, struct mount **mpp);
static int	vfs_mountroot_ask(void);
static int	vfs_mountroot_try(const char *mountfrom);
static int	vfs_donmount(struct thread *td, int fsflags,
		    struct uio *fsoptions);

static int	usermount = 0;
SYSCTL_INT(_vfs, OID_AUTO, usermount, CTLFLAG_RW, &usermount, 0,
    "Unprivileged users may mount and unmount file systems");

MALLOC_DEFINE(M_MOUNT, "mount", "vfs mount structure");

/* List of mounted filesystems. */
struct mntlist mountlist = TAILQ_HEAD_INITIALIZER(mountlist);

/* For any iteration/modification of mountlist */
struct mtx mountlist_mtx;

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
 * The root specifiers we will try if RB_CDROM is specified.
 */
static char *cdrom_rootdevnames[] = {
	"cd9660:cd0",
	"cd9660:acd0",
	NULL
};

/* legacy find-root code */
char		*rootdevnames[2] = {NULL, NULL};
struct cdev *rootdev = NULL;
#ifdef ROOTDEVNAME
const char	*ctrootdevname = ROOTDEVNAME;
#else
const char	*ctrootdevname = NULL;
#endif

/*
 * Has to be dynamic as the value of rootdev can change; however, it can't
 * change after the root is mounted, so a user process can't access this
 * sysctl until after the value is unchangeable.
 */
static int
sysctl_rootdev(SYSCTL_HANDLER_ARGS)
{
	int error;

	/* _RD prevents this from happening. */
	KASSERT(req->newptr == NULL, ("Attempt to change root device name"));

	if (rootdev != NULL)
		error = sysctl_handle_string(oidp, rootdev->si_name, 0, req);
	else
		error = sysctl_handle_string(oidp, "", 0, req);

	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, rootdev, CTLTYPE_STRING | CTLFLAG_RD,
    0, 0, sysctl_rootdev, "A", "Root file system device");

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
		opt->len = optlen;

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
 * New mount API.
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

int
kernel_mount(struct iovec *iovp, u_int iovcnt, int flags)
{
	struct uio auio;
	int error;

	/*
	 * Check that we have an even number of iovec's
	 * and that we have at least two options.
	 */
	if ((iovcnt & 1) || (iovcnt < 4))
		return (EINVAL);

	auio.uio_iov = iovp;
	auio.uio_iovcnt = iovcnt;
	auio.uio_segflg = UIO_SYSSPACE;

	error = vfs_donmount(curthread, flags, &auio);
	return (error);
}

int
kernel_vmount(int flags, ...)
{
	struct iovec *iovp;
	struct uio auio;
	va_list ap;
	u_int iovcnt, iovlen, len;
	const char *cp;
	char *buf, *pos;
	size_t n;
	int error, i;

	len = 0;
	va_start(ap, flags);
	for (iovcnt = 0; (cp = va_arg(ap, const char *)) != NULL; iovcnt++)
		len += strlen(cp) + 1;
	va_end(ap);

	if (iovcnt < 4 || iovcnt & 1)
		return (EINVAL);

	iovlen = iovcnt * sizeof (struct iovec);
	MALLOC(iovp, struct iovec *, iovlen, M_MOUNT, M_WAITOK);
	MALLOC(buf, char *, len, M_MOUNT, M_WAITOK);
	pos = buf;
	va_start(ap, flags);
	for (i = 0; i < iovcnt; i++) {
		cp = va_arg(ap, const char *);
		copystr(cp, pos, len - (pos - buf), &n);
		iovp[i].iov_base = pos;
		iovp[i].iov_len = n;
		pos += n;
	}
	va_end(ap);

	auio.uio_iov = iovp;
	auio.uio_iovcnt = iovcnt;
	auio.uio_segflg = UIO_SYSSPACE;

	error = vfs_donmount(curthread, flags, &auio);
	FREE(iovp, M_MOUNT);
	FREE(buf, M_MOUNT);
	return (error);
}

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
	lockinit(&mp->mnt_lock, PVFS, "vfslock", 0, LK_NOPAUSE);
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
	*mpp = mp;
	return (0);
}

/*
 * Destroy the mount struct previously allocated by vfs_mount_alloc().
 */
void
vfs_mount_destroy(struct mount *mp, struct thread *td)
{

	mp->mnt_vfc->vfc_refcount--;
	if (!TAILQ_EMPTY(&mp->mnt_nvnodelist))
		panic("unmount: dangling vnode");
	vfs_unbusy(mp,td);
	lockdestroy(&mp->mnt_lock);
	mtx_destroy(&mp->mnt_mtx);
	if (mp->mnt_kern_flag & MNTK_MWAIT)
		wakeup(mp);
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

	error = vfs_domount(td, fstype, fspath, fsflags, optlist, 0);
bail:
	if (error)
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
	char *fspath;
	int error;

	fstype = malloc(MFSNAMELEN, M_TEMP, M_WAITOK);
	fspath = malloc(MNAMELEN, M_TEMP, M_WAITOK);

	/*
	 * vfs_mount() actually takes a kernel string for `type' and
	 * `path' now, so extract them.
	 */
	error = copyinstr(uap->type, fstype, MFSNAMELEN, NULL);
	if (error == 0)
		error = copyinstr(uap->path, fspath, MNAMELEN, NULL);
	if (error == 0)
		error = vfs_domount(td, fstype, fspath, uap->flags,
		    uap->data, 1);
	free(fstype, M_TEMP);
	free(fspath, M_TEMP);
	return (error);
}

/*
 * vfs_mount(): actually attempt a filesystem mount.
 *
 * This routine is designed to be a "generic" entry point for routines
 * that wish to mount a filesystem. All parameters except `fsdata' are
 * pointers into kernel space. `fsdata' is currently still a pointer
 * into userspace.
 */
int
vfs_mount(td, fstype, fspath, fsflags, fsdata)
	struct thread *td;
	const char *fstype;
	char *fspath;
	int fsflags;
	void *fsdata;
{

	return (vfs_domount(td, fstype, fspath, fsflags, fsdata, 1));
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
	void *fsdata,		/* Options local to the filesystem. */
	int compat		/* Invocation from compat syscall. */
	)
{
	linker_file_t lf;
	struct vnode *vp;
	struct mount *mp;
	struct vfsconf *vfsp;
	int error, flag = 0, kern_flag = 0;
	struct vattr va;
	struct nameidata nd;

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
	 * Silently enforce MNT_NODEV, MNT_NOSUID and MNT_USER for
	 * unprivileged users.
	 */
	if (suser(td) != 0)
		fsflags |= MNT_NODEV | MNT_NOSUID | MNT_USER;
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
		    (MNT_RELOAD | MNT_FORCE | MNT_UPDATE | MNT_SNAPSHOT);
		VOP_UNLOCK(vp, 0, td);
		if (compat == 0) {
			mp->mnt_optnew = fsdata;
			vfs_mergeopts(mp->mnt_optnew, mp->mnt_opt);
		}
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
		if ((error = vinvalbuf(vp, V_SAVE, td->td_ucred, td, 0, 0)) != 0) {
			vput(vp);
			return (error);
		}
		if (vp->v_type != VDIR) {
			vput(vp);
			return (ENOTDIR);
		}
		vfsp = vfs_byname(fstype);
		if (vfsp == NULL) {
			/* Only load modules for root (very important!). */
			if ((error = suser(td)) != 0) {
				vput(vp);
				return (error);
			}
			error = securelevel_gt(td->td_ucred, 0);
			if (error) {
				vput(vp);
				return (error);
			}
			error = linker_load_module(NULL, fstype, NULL, NULL, &lf);
			if (error || lf == NULL) {
				vput(vp);
				if (lf == NULL)
					error = ENODEV;
				return (error);
			}
			lf->userrefs++;
			/* Look up again to see if the VFS was loaded. */
			vfsp = vfs_byname(fstype);
			if (vfsp == NULL) {
				lf->userrefs--;
				linker_file_unload(lf, LINKER_UNLOAD_FORCE);
				vput(vp);
				return (ENODEV);
			}
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
		if (compat == 0)
			mp->mnt_optnew = fsdata;
	}
	/*
	 * Check if the fs implements the type VFS_[O]MOUNT()
	 * function we are looking for.
	 */
	if ((compat == 0) == (mp->mnt_op->vfs_omount != NULL)) {
		printf("%s doesn't support the %s mount syscall\n",
		    mp->mnt_vfc->vfc_name, compat ? "old" : "new");
		VI_LOCK(vp);
		vp->v_iflag &= ~VI_MOUNT;
		VI_UNLOCK(vp);
		if (mp->mnt_flag & MNT_UPDATE)
			vfs_unbusy(mp, td);
		else
			vfs_mount_destroy(mp, td);
		vrele(vp);
		return (EOPNOTSUPP);
	}

	/*
	 * Set the mount level flags.
	 */
	if (fsflags & MNT_RDONLY)
		mp->mnt_flag |= MNT_RDONLY;
	else if (mp->mnt_flag & MNT_RDONLY)
		mp->mnt_kern_flag |= MNTK_WANTRDWR;
	mp->mnt_flag &=~ MNT_UPDATEMASK;
	mp->mnt_flag |= fsflags & (MNT_UPDATEMASK | MNT_FORCE);
	/*
	 * Mount the filesystem.
	 * XXX The final recipients of VFS_MOUNT just overwrite the ndp they
	 * get.  No freeing of cn_pnbuf.
	 */
	if (compat)
	    error = VFS_OMOUNT(mp, fspath, fsdata, td);
	else
	    error = VFS_MOUNT(mp, td);
	if (!error) {
		if (mp->mnt_opt != NULL)
			vfs_freeopts(mp->mnt_opt);
		mp->mnt_opt = mp->mnt_optnew;
	}
	/*
	 * Prevent external consumers of mount options from reading
	 * mnt_optnew.
	*/
	mp->mnt_optnew = NULL;
	if (mp->mnt_flag & MNT_UPDATE) {
		if (mp->mnt_kern_flag & MNTK_WANTRDWR)
			mp->mnt_flag &= ~MNT_RDONLY;
		mp->mnt_flag &=
		    ~(MNT_UPDATE | MNT_RELOAD | MNT_FORCE | MNT_SNAPSHOT);
		mp->mnt_kern_flag &= ~MNTK_WANTRDWR;
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
		if (VFS_ROOT(mp, &newdp, td))
			panic("mount: lost mount");
		checkdirs(vp, newdp);
		vput(newdp);
		VOP_UNLOCK(vp, 0, td);
		if ((mp->mnt_flag & MNT_RDONLY) == 0)
			error = vfs_allocate_syncvnode(mp);
		vfs_unbusy(mp, td);
		if (error || (error = VFS_START(mp, 0, td)) != 0)
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
 * Scan all active processes to see if any of them have a current
 * or root directory of `olddp'. If so, replace them with the new
 * mount point.
 */
static void
checkdirs(olddp, newdp)
	struct vnode *olddp, *newdp;
{
	struct filedesc *fdp;
	struct proc *p;
	int nrele;

	if (vrefcnt(olddp) == 1)
		return;
	sx_slock(&allproc_lock);
	LIST_FOREACH(p, &allproc, p_list) {
		mtx_lock(&fdesc_mtx);
		fdp = p->p_fd;
		if (fdp == NULL) {
			mtx_unlock(&fdesc_mtx);
			continue;
		}
		nrele = 0;
		FILEDESC_LOCK(fdp);
		if (fdp->fd_cdir == olddp) {
			VREF(newdp);
			fdp->fd_cdir = newdp;
			nrele++;
		}
		if (fdp->fd_rdir == olddp) {
			VREF(newdp);
			fdp->fd_rdir = newdp;
			nrele++;
		}
		FILEDESC_UNLOCK(fdp);
		mtx_unlock(&fdesc_mtx);
		while (nrele--)
			vrele(olddp);
	}
	sx_sunlock(&allproc_lock);
	if (rootvnode == olddp) {
		vrele(rootvnode);
		VREF(newdp);
		rootvnode = newdp;
	}
}

/*
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
	return (dounmount(mp, uap->flags, td));
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

	mtx_lock(&mountlist_mtx);
	if (mp->mnt_kern_flag & MNTK_UNMOUNT) {
		mtx_unlock(&mountlist_mtx);
		return (EBUSY);
	}
	mp->mnt_kern_flag |= MNTK_UNMOUNT;
	/* Allow filesystems to detect that a forced unmount is in progress. */
	if (flags & MNT_FORCE)
		mp->mnt_kern_flag |= MNTK_UNMOUNTF;
	error = lockmgr(&mp->mnt_lock, LK_DRAIN | LK_INTERLOCK |
	    ((flags & MNT_FORCE) ? 0 : LK_NOWAIT), &mountlist_mtx, td);
	if (error) {
		mp->mnt_kern_flag &= ~(MNTK_UNMOUNT | MNTK_UNMOUNTF);
		if (mp->mnt_kern_flag & MNTK_MWAIT)
			wakeup(mp);
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
	if ((flags & MNT_FORCE) && VFS_ROOT(mp, &fsrootvp, td) == 0) {
		if (mp->mnt_vnodecovered != NULL)
			checkdirs(fsrootvp, mp->mnt_vnodecovered);
		if (fsrootvp == rootvnode) {
			vrele(rootvnode);
			rootvnode = NULL;
		}
		vput(fsrootvp);
	}
	if (((mp->mnt_flag & MNT_RDONLY) ||
	     (error = VFS_SYNC(mp, MNT_WAIT, td->td_ucred, td)) == 0) ||
	    (flags & MNT_FORCE)) {
		error = VFS_UNMOUNT(mp, flags, td);
	}
	vn_finished_write(mp);
	if (error) {
		/* Undo cdir/rdir and rootvnode changes made above. */
		if ((flags & MNT_FORCE) && VFS_ROOT(mp, &fsrootvp, td) == 0) {
			if (mp->mnt_vnodecovered != NULL)
				checkdirs(mp->mnt_vnodecovered, fsrootvp);
			if (rootvnode == NULL) {
				rootvnode = fsrootvp;
				vref(rootvnode);
			}
			vput(fsrootvp);
		}
		if ((mp->mnt_flag & MNT_RDONLY) == 0 && mp->mnt_syncer == NULL)
			(void) vfs_allocate_syncvnode(mp);
		mtx_lock(&mountlist_mtx);
		mp->mnt_kern_flag &= ~(MNTK_UNMOUNT | MNTK_UNMOUNTF);
		mp->mnt_flag |= async_flag;
		lockmgr(&mp->mnt_lock, LK_RELEASE | LK_INTERLOCK,
		    &mountlist_mtx, td);
		if (mp->mnt_kern_flag & MNTK_MWAIT)
			wakeup(mp);
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
 * Lookup a filesystem type, and if found allocate and initialize
 * a mount structure for it.
 *
 * Devname is usually updated by mount(8) after booting.
 */
int
vfs_rootmountalloc(fstypename, devname, mpp)
	char *fstypename;
	char *devname;
	struct mount **mpp;
{
	struct thread *td = curthread;	/* XXX */
	struct vfsconf *vfsp;
	struct mount *mp;
	int error;

	if (fstypename == NULL)
		return (ENODEV);
	vfsp = vfs_byname(fstypename);
	if (vfsp == NULL)
		return (ENODEV);
	error = vfs_mount_alloc(NULLVP, vfsp, "/", td, &mp);
	if (error)
		return (error);
	mp->mnt_flag |= MNT_RDONLY | MNT_ROOTFS;
	strlcpy(mp->mnt_stat.f_mntfromname, devname, MNAMELEN);
	*mpp = mp;
	return (0);
}

/*
 * Find and mount the root filesystem
 */
void
vfs_mountroot(void)
{
	char *cp;
	int error, i, asked = 0;


	/*
	 * Wait for GEOM to settle down
	 */
	g_waitidle();

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
	const char	*devname;
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
	printf("Mounting root from %s\n", mountfrom);
	splx(s);

	/* parse vfs name and path */
	vfsname = malloc(MFSNAMELEN, M_MOUNT, M_WAITOK);
	path = malloc(MNAMELEN, M_MOUNT, M_WAITOK);
	vfsname[0] = path[0] = 0;
	sprintf(patt, "%%%d[a-z0-9]:%%%ds", MFSNAMELEN, MNAMELEN);
	if (sscanf(mountfrom, patt, vfsname, path) < 1)
		goto done;

	/* allocate a root mount */
	error = vfs_rootmountalloc(vfsname, path[0] != 0 ? path : ROOTNAME,
	    &mp);
	if (error != 0) {
		printf("Can't allocate root mount for filesystem '%s': %d\n",
		       vfsname, error);
		goto done;
	}

	/*
	 * do our best to set rootdev
	 * XXX: This does not belong here!
	 */
	if (path[0] != '\0') {
		struct cdev *diskdev;
		diskdev = getdiskbyname(path);
		if (diskdev != NULL)
			rootdev = diskdev;
		else
			printf("setrootbyname failed\n");
	}

	/* If the root device is a type "memory disk", mount RW */
	if (rootdev != NULL && devsw(rootdev) != NULL) {
		devname = devtoname(rootdev);
		if (devname[0] == 'm' && devname[1] == 'd')
			mp->mnt_flag &= ~MNT_RDONLY;
	}

	error = VFS_OMOUNT(mp, NULL, NULL, curthread);

done:
	if (vfsname != NULL)
		free(vfsname, M_MOUNT);
	if (path != NULL)
		free(path, M_MOUNT);
	if (error != 0) {
		if (mp != NULL)
			vfs_mount_destroy(mp, curthread);
		printf("Root mount failed: %d\n", error);
	} else {

		/* register with list of mounted filesystems */
		mtx_lock(&mountlist_mtx);
		TAILQ_INSERT_HEAD(&mountlist, mp, mnt_list);
		mtx_unlock(&mountlist_mtx);

		/* sanity check system clock against root fs timestamp */
		inittodr(mp->mnt_time);
		vfs_unbusy(mp, curthread);
		error = VFS_START(mp, 0, curthread);
	}
	return (error);
}

/*
 * Spin prompting on the console for a suitable root filesystem
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
		gets(name);
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
 * Local helper function for vfs_mountroot_ask.
 */
static void
gets(char *cp)
{
	char *lp;
	int c;

	lp = cp;
	for (;;) {
		printf("%c", c = cngetc() & 0177);
		switch (c) {
		case -1:
		case '\n':
		case '\r':
			*lp++ = '\0';
			return;
		case '\b':
		case '\177':
			if (lp > cp) {
				printf(" \b");
				lp--;
			}
			continue;
		case '#':
			lp--;
			if (lp < cp)
				lp = cp;
			continue;
		case '@':
		case 'u' & 037:
			lp = cp;
			printf("%c", '\n');
			continue;
		default:
			*lp++ = c;
		}
	}
}

/*
 * Convert a given name to the cdev pointer of the device, which is probably
 * but not by definition, a disk.  Mount a DEVFS (on nothing), look the name
 * up, extract the cdev from the vnode and unmount it again.  Unfortunately
 * we cannot use the vnode directly (because we unmount the DEVFS again)
 * so the filesystems still have to do the bdevvp() stunt.
 */
struct cdev *
getdiskbyname(char *name)
{
	char *cp = name;
	struct cdev *dev = NULL;
	struct thread *td = curthread;
	struct vfsconf *vfsp;
	struct mount *mp = NULL;
	struct vnode *vroot = NULL;
	struct nameidata nid;
	int error;

	if (!bcmp(cp, "/dev/", 5))
		cp += 5;

	do {
		vfsp = vfs_byname("devfs");
		if (vfsp == NULL)
			break;
		error = vfs_mount_alloc(NULLVP, vfsp, "/dev", td, &mp);
		if (error)
			break;
		mp->mnt_flag |= MNT_RDONLY;

		error = VFS_MOUNT(mp, curthread);
		if (error)
			break;
		VFS_START(mp, 0, td);
		VFS_ROOT(mp, &vroot, td);
		VOP_UNLOCK(vroot, 0, td);

		NDINIT(&nid, LOOKUP, NOCACHE|FOLLOW,
		    UIO_SYSSPACE, cp, curthread);
		nid.ni_startdir = vroot;
		nid.ni_pathlen = strlen(cp);
		nid.ni_cnd.cn_cred = curthread->td_ucred;
		nid.ni_cnd.cn_nameptr = cp;

		error = lookup(&nid);
		if (error)
			break;
		dev = vn_todev (nid.ni_vp);
		NDFREE(&nid, 0);
	} while (0);

	if (vroot != NULL)
		VFS_UNMOUNT(mp, 0, td);
	if (mp != NULL)
		vfs_mount_destroy(mp, td);
  	return (dev);
}

/* Show the struct cdev *for a disk specified by name */
#ifdef DDB
DB_SHOW_COMMAND(disk, db_getdiskbyname)
{
	struct cdev *dev;

	if (modif[0] == '\0') {
		db_error("usage: show disk/devicename");
		return;
	}
	dev = getdiskbyname(modif);
	if (dev != NULL)
		db_printf("struct cdev *= %p\n", dev);
	else
		db_printf("No disk device matched.\n");
}
#endif

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
