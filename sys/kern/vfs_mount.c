/*
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#define ROOTNAME	"root_device"

static void	checkdirs(struct vnode *olddp, struct vnode *newdp);
static int	vfs_nmount(struct thread *td, int, struct uio *);
static int	vfs_mountroot_try(char *mountfrom);
static int	vfs_mountroot_ask(void);
static void	gets(char *cp);

static int	usermount = 0;	/* if 1, non-root can mount fs. */
SYSCTL_INT(_vfs, OID_AUTO, usermount, CTLFLAG_RW, &usermount, 0, "");

MALLOC_DEFINE(M_MOUNT, "mount", "vfs mount structure");

/* List of mounted filesystems. */
struct mntlist mountlist = TAILQ_HEAD_INITIALIZER(mountlist);

/* For any iteration/modification of mountlist */
struct mtx mountlist_mtx;

/* For any iteration/modification of mnt_vnodelist */
struct mtx mntvnode_mtx;

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
	"cd9660:cd0a",
	"cd9660:acd0a",
	"cd9660:wcd0a",
	NULL
};

/* legacy find-root code */
char		*rootdevnames[2] = {NULL, NULL};
static int	setrootbyname(char *name);
dev_t		rootdev = NODEV;

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

	if (rootdev != NODEV)
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
 * If a mount option is specified several times,
 * (with or without the "no" prefix) only keep
 * the last occurence of it.
 */
static void
vfs_sanitizeopts(struct vfsoptlist *opts)
{
	struct vfsopt *opt, *opt2, *tmp;
	int noopt;

	TAILQ_FOREACH_REVERSE(opt, opts, vfsoptlist, link) {
		if (strncmp(opt->name, "no", 2) == 0)
			noopt = 1;
		else
			noopt = 0;
		opt2 = TAILQ_PREV(opt, vfsoptlist, link);
		while (opt2 != NULL) {
			if (strcmp(opt2->name, opt->name) == 0 ||
			    (noopt && strcmp(opt->name + 2, opt2->name) == 0) ||
			    (!noopt && strncmp(opt2->name, "no", 2) == 0 &&
			    strcmp(opt2->name + 2, opt->name) == 0)) {
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
	unsigned int i, iovcnt;
	int error, namelen, optlen;

	iovcnt = auio->uio_iovcnt;
	opts = malloc(sizeof(struct vfsoptlist), M_MOUNT, M_WAITOK);
	TAILQ_INIT(opts);
	for (i = 0; i < iovcnt; i += 2) {
		opt = malloc(sizeof(struct vfsopt), M_MOUNT, M_WAITOK);
		namelen = auio->uio_iov[i].iov_len;
		optlen = auio->uio_iov[i + 1].iov_len;
		opt->name = malloc(namelen, M_MOUNT, M_WAITOK);
		opt->value = NULL;
		if (auio->uio_segflg == UIO_SYSSPACE) {
			bcopy(auio->uio_iov[i].iov_base, opt->name, namelen);
		} else {
			error = copyin(auio->uio_iov[i].iov_base, opt->name,
			    namelen);
			if (error)
				goto bad;
		}
		opt->len = optlen;
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
		TAILQ_INSERT_TAIL(opts, opt, link);
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
	struct uio auio;
	struct iovec *iov, *needfree;
	struct iovec aiov[UIO_SMALLIOV];
	unsigned int i;
	int error;
	u_int iovlen, iovcnt;

	iovcnt = uap->iovcnt;
	iovlen = iovcnt * sizeof (struct iovec);
	/*
	 * Check that we have an even number of iovec's
	 * and that we have at least two options.
	 */
	if ((iovcnt & 1) || (iovcnt < 4) || (iovcnt > UIO_MAXIOV))
		return (EINVAL);

	if (iovcnt > UIO_SMALLIOV) {
		MALLOC(iov, struct iovec *, iovlen, M_IOV, M_WAITOK);
		needfree = iov;
	} else {
		iov = aiov;
		needfree = NULL;
	}
	auio.uio_iov = iov;
	auio.uio_iovcnt = iovcnt;
	auio.uio_segflg = UIO_USERSPACE;
	if ((error = copyin(uap->iovp, iov, iovlen)))
		goto finish;

	for (i = 0; i < iovcnt; i++) {
		if (iov->iov_len > MMAXOPTIONLEN) {
			error = EINVAL;
			goto finish;
		}
		iov++;
	}
	error = vfs_nmount(td, uap->flags, &auio);
finish:
	if (needfree != NULL)
		free(needfree, M_TEMP);
	return (error);
}

int
kernel_mount(iovp, iovcnt, flags)
	struct iovec *iovp;
	unsigned int iovcnt;
	int flags;
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

	error = vfs_nmount(curthread, flags, &auio);
	return (error);
}

int
kernel_vmount(int flags, ...)
{
	struct iovec *iovp;
	struct uio auio;
	va_list ap;
	unsigned int iovcnt, iovlen, len;
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

	error = vfs_nmount(curthread, flags, &auio);
	FREE(iovp, M_MOUNT);
	FREE(buf, M_MOUNT);
	return (error);
}

/*
 * vfs_nmount(): actually attempt a filesystem mount.
 */
static int
vfs_nmount(td, fsflags, fsoptions)
	struct thread *td;
	int fsflags;		/* Flags common to all filesystems. */
	struct uio *fsoptions;	/* Options local to the filesystem. */
{
	linker_file_t lf;
	struct vnode *vp;
	struct mount *mp;
	struct vfsconf *vfsp;
	struct vfsoptlist *optlist;
	char *fstype, *fspath;
	int error, flag = 0, kern_flag = 0;
	int fstypelen, fspathlen;
	struct vattr va;
	struct nameidata nd;

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
		goto bad;
	}
	fspathlen = 0;
	error = vfs_getopt(optlist, "fspath", (void **)&fspath, &fspathlen);
	if (error || fspath[fspathlen - 1] != '\0') {
		error = EINVAL;
		goto bad;
	}

	/*
	 * Be ultra-paranoid about making sure the type and fspath
	 * variables will fit in our mp buffers, including the
	 * terminating NUL.
	 */
	if (fstypelen >= MFSNAMELEN - 1 || fspathlen >= MNAMELEN - 1) {
		error = ENAMETOOLONG;
		goto bad;
	}

	if (usermount == 0) {
	       	error = suser(td);
		if (error)
			goto bad;
	}
	/*
	 * Do not allow NFS export by non-root users.
	 */
	if (fsflags & MNT_EXPORTED) {
		error = suser(td);
		if (error)
			goto bad;
	}
	/*
	 * Silently enforce MNT_NOSUID and MNT_NODEV for non-root users.
	 */
	if (suser(td)) 
		fsflags |= MNT_NOSUID | MNT_NODEV;
	/*
	 * Get vnode to be covered
	 */
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, fspath, td);
	if ((error = namei(&nd)) != 0)
		goto bad;
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;
	if (fsflags & MNT_UPDATE) {
		if ((vp->v_vflag & VV_ROOT) == 0) {
			vput(vp);
			error = EINVAL;
			goto bad;
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
			error = EOPNOTSUPP;	/* Needs translation */
			goto bad;
		}
		/*
		 * Only root, or the user that did the original mount is
		 * permitted to update it.
		 */
		if (mp->mnt_cred->cr_uid != td->td_ucred->cr_uid) {
			error = suser(td);
			if (error) {
				vput(vp);
				goto bad;
			}
		}
		if (vfs_busy(mp, LK_NOWAIT, 0, td)) {
			vput(vp);
			error = EBUSY;
			goto bad;
		}
		VI_LOCK(vp);
		if ((vp->v_iflag & VI_MOUNT) != 0 ||
		    vp->v_mountedhere != NULL) {
			VI_UNLOCK(vp);
			vfs_unbusy(mp, td);
			vput(vp);
			error = EBUSY;
			goto bad;
		}
		vp->v_iflag |= VI_MOUNT;
		VI_UNLOCK(vp);
		mp->mnt_flag |= fsflags &
		    (MNT_RELOAD | MNT_FORCE | MNT_UPDATE | MNT_SNAPSHOT);
		VOP_UNLOCK(vp, 0, td);
		mp->mnt_optnew = optlist;
		vfs_mergeopts(mp->mnt_optnew, mp->mnt_opt);
		goto update;
	}
	/*
	 * If the user is not root, ensure that they own the directory
	 * onto which we are attempting to mount.
	 */
	error = VOP_GETATTR(vp, &va, td->td_ucred, td);
	if (error) {
		vput(vp);
		goto bad;
	}
	if (va.va_uid != td->td_ucred->cr_uid) {
		error = suser(td);
		if (error) {
			vput(vp);
			goto bad;
		}
	}
	if ((error = vinvalbuf(vp, V_SAVE, td->td_ucred, td, 0, 0)) != 0) {
		vput(vp);
		goto bad;
	}
	if (vp->v_type != VDIR) {
		vput(vp);
		error = ENOTDIR;
		goto bad;
	}
	for (vfsp = vfsconf; vfsp; vfsp = vfsp->vfc_next)
		if (!strcmp(vfsp->vfc_name, fstype))
			break;
	if (vfsp == NULL) {
		/* Only load modules for root (very important!). */
		error = suser(td);
		if (error) {
			vput(vp);
			goto bad;
		}
		error = securelevel_gt(td->td_ucred, 0);
		if (error) {
			vput(vp);
			goto bad;
		}
		error = linker_load_module(NULL, fstype, NULL, NULL, &lf);
		if (error || lf == NULL) {
			vput(vp);
			if (lf == NULL)
				error = ENODEV;
			goto bad;
		}
		lf->userrefs++;
		/* Look up again to see if the VFS was loaded. */
		for (vfsp = vfsconf; vfsp; vfsp = vfsp->vfc_next)
			if (!strcmp(vfsp->vfc_name, fstype))
				break;
		if (vfsp == NULL) {
			lf->userrefs--;
			linker_file_unload(lf);
			vput(vp);
			error = ENODEV;
			goto bad;
		}
	}
	VI_LOCK(vp);
	if ((vp->v_iflag & VI_MOUNT) != 0 ||
	    vp->v_mountedhere != NULL) {
		VI_UNLOCK(vp);
		vput(vp);
		error = EBUSY;
		goto bad;
	}
	vp->v_iflag |= VI_MOUNT;
	VI_UNLOCK(vp);

	/*
	 * Allocate and initialize the filesystem.
	 */
	mp = malloc(sizeof(struct mount), M_MOUNT, M_WAITOK | M_ZERO);
	TAILQ_INIT(&mp->mnt_nvnodelist);
	TAILQ_INIT(&mp->mnt_reservedvnlist);
	mp->mnt_nvnodelistsize = 0;
	lockinit(&mp->mnt_lock, PVFS, "vfslock", 0, LK_NOPAUSE);
	(void)vfs_busy(mp, LK_NOWAIT, 0, td);
	mp->mnt_op = vfsp->vfc_vfsops;
	mp->mnt_vfc = vfsp;
	vfsp->vfc_refcount++;
	mp->mnt_stat.f_type = vfsp->vfc_typenum;
	mp->mnt_flag |= vfsp->vfc_flags & MNT_VISFLAGMASK;
	strlcpy(mp->mnt_stat.f_fstypename, fstype, MFSNAMELEN);
	mp->mnt_vnodecovered = vp;
	mp->mnt_cred = crdup(td->td_ucred);
	mp->mnt_stat.f_owner = td->td_ucred->cr_uid;
	strlcpy(mp->mnt_stat.f_mntonname, fspath, MNAMELEN);
	mp->mnt_iosize_max = DFLTPHYS;
#ifdef MAC
	mac_init_mount(mp);
	mac_create_mount(td->td_ucred, mp);
#endif
	VOP_UNLOCK(vp, 0, td);
	mp->mnt_optnew = optlist;	/* XXXMAC: should this be above? */

update:
	/*
	 * Check if the fs implements the new VFS_NMOUNT()
	 * function, since the new system call was used.
	 */
	if (mp->mnt_op->vfs_mount != NULL) {
		printf("%s doesn't support the new mount syscall\n",
		    mp->mnt_vfc->vfc_name);
		VI_LOCK(vp);
		vp->v_iflag &= ~VI_MOUNT;
		VI_UNLOCK(vp);
		if (mp->mnt_flag & MNT_UPDATE)
			vfs_unbusy(mp, td);
		else {
			mp->mnt_vfc->vfc_refcount--;
			vfs_unbusy(mp, td);
#ifdef MAC
			mac_destroy_mount(mp);
#endif
			crfree(mp->mnt_cred);
			free(mp, M_MOUNT);
		}
		vrele(vp);
		error = EOPNOTSUPP;
		goto bad;
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
	error = VFS_NMOUNT(mp, &nd, td);
	if (!error) {
		if (mp->mnt_opt != NULL)
			vfs_freeopts(mp->mnt_opt);
		mp->mnt_opt = mp->mnt_optnew;
	}
	/*
	 * Prevent external consumers of mount
	 * options to read mnt_optnew.
	 */
	mp->mnt_optnew = NULL;
	if (mp->mnt_flag & MNT_UPDATE) {
		if (mp->mnt_kern_flag & MNTK_WANTRDWR)
			mp->mnt_flag &= ~MNT_RDONLY;
		mp->mnt_flag &=~
		    (MNT_UPDATE | MNT_RELOAD | MNT_FORCE | MNT_SNAPSHOT);
		mp->mnt_kern_flag &=~ MNTK_WANTRDWR;
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
		if (VFS_ROOT(mp, &newdp))
			panic("mount: lost mount");
		checkdirs(vp, newdp);
		vput(newdp);
		VOP_UNLOCK(vp, 0, td);
		if ((mp->mnt_flag & MNT_RDONLY) == 0)
			error = vfs_allocate_syncvnode(mp);
		vfs_unbusy(mp, td);
		if ((error = VFS_START(mp, 0, td)) != 0) {
			vrele(vp);
			goto bad;
		}
	} else {
		VI_LOCK(vp);
		vp->v_iflag &= ~VI_MOUNT;
		VI_UNLOCK(vp);
		mp->mnt_vfc->vfc_refcount--;
		vfs_unbusy(mp, td);
#ifdef MAC
		mac_destroy_mount(mp);
#endif
		crfree(mp->mnt_cred);
		free(mp, M_MOUNT);
		vput(vp);
		goto bad;
	}
	return (0);
bad:
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
		error = vfs_mount(td, fstype, fspath, uap->flags, uap->data);
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

	if (usermount == 0) {
		error = suser(td);
		if (error)
			return (error);
	}
	/*
	 * Do not allow NFS export by non-root users.
	 */
	if (fsflags & MNT_EXPORTED) {
		error = suser(td);
		if (error)
			return (error);
	}
	/*
	 * Silently enforce MNT_NOSUID and MNT_NODEV for non-root users.
	 */
	if (suser(td)) 
		fsflags |= MNT_NOSUID | MNT_NODEV;
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
		 * Only root, or the user that did the original mount is
		 * permitted to update it.
		 */
		if (mp->mnt_cred->cr_uid != td->td_ucred->cr_uid) {
			error = suser(td);
			if (error) {
				vput(vp);
				return (error);
			}
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
		goto update;
	}
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
		error = suser(td);
		if (error) {
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
	for (vfsp = vfsconf; vfsp; vfsp = vfsp->vfc_next)
		if (!strcmp(vfsp->vfc_name, fstype))
			break;
	if (vfsp == NULL) {
		/* Only load modules for root (very important!). */
		error = suser(td);
		if (error) {
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
		for (vfsp = vfsconf; vfsp; vfsp = vfsp->vfc_next)
			if (!strcmp(vfsp->vfc_name, fstype))
				break;
		if (vfsp == NULL) {
			lf->userrefs--;
			linker_file_unload(lf);
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
	mp = malloc(sizeof(struct mount), M_MOUNT, M_WAITOK | M_ZERO);
	TAILQ_INIT(&mp->mnt_nvnodelist);
	TAILQ_INIT(&mp->mnt_reservedvnlist);
	mp->mnt_nvnodelistsize = 0;
	lockinit(&mp->mnt_lock, PVFS, "vfslock", 0, LK_NOPAUSE);
	(void)vfs_busy(mp, LK_NOWAIT, 0, td);
	mp->mnt_op = vfsp->vfc_vfsops;
	mp->mnt_vfc = vfsp;
	vfsp->vfc_refcount++;
	mp->mnt_stat.f_type = vfsp->vfc_typenum;
	mp->mnt_flag |= vfsp->vfc_flags & MNT_VISFLAGMASK;
	strlcpy(mp->mnt_stat.f_fstypename, fstype, MFSNAMELEN);
	mp->mnt_vnodecovered = vp;
	mp->mnt_cred = crdup(td->td_ucred);
	mp->mnt_stat.f_owner = td->td_ucred->cr_uid;
	strlcpy(mp->mnt_stat.f_mntonname, fspath, MNAMELEN);
	mp->mnt_iosize_max = DFLTPHYS;
#ifdef MAC
	mac_init_mount(mp);
	mac_create_mount(td->td_ucred, mp);
#endif
	VOP_UNLOCK(vp, 0, td);
update:
	/*
	 * Check if the fs implements the old VFS_MOUNT()
	 * function, since the old system call was used.
	 */
	if (mp->mnt_op->vfs_mount == NULL) {
		printf("%s doesn't support the old mount syscall\n",
		    mp->mnt_vfc->vfc_name);
		VI_LOCK(vp);
		vp->v_iflag &= ~VI_MOUNT;
		VI_UNLOCK(vp);
		if (mp->mnt_flag & MNT_UPDATE)
			vfs_unbusy(mp, td);
		else {
			mp->mnt_vfc->vfc_refcount--;
			vfs_unbusy(mp, td);
#ifdef MAC
			mac_destroy_mount(mp);
#endif
			crfree(mp->mnt_cred);
			free(mp, M_MOUNT);
		}
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
	error = VFS_MOUNT(mp, fspath, fsdata, &nd, td);
	if (mp->mnt_flag & MNT_UPDATE) {
		if (mp->mnt_kern_flag & MNTK_WANTRDWR)
			mp->mnt_flag &= ~MNT_RDONLY;
		mp->mnt_flag &=~
		    (MNT_UPDATE | MNT_RELOAD | MNT_FORCE | MNT_SNAPSHOT);
		mp->mnt_kern_flag &=~ MNTK_WANTRDWR;
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
		if (VFS_ROOT(mp, &newdp))
			panic("mount: lost mount");
		checkdirs(vp, newdp);
		vput(newdp);
		VOP_UNLOCK(vp, 0, td);
		if ((mp->mnt_flag & MNT_RDONLY) == 0)
			error = vfs_allocate_syncvnode(mp);
		vfs_unbusy(mp, td);
		if ((error = VFS_START(mp, 0, td)) != 0)
			vrele(vp);
	} else {
		VI_LOCK(vp);
		vp->v_iflag &= ~VI_MOUNT;
		VI_UNLOCK(vp);
		mp->mnt_vfc->vfc_refcount--;
		vfs_unbusy(mp, td);
#ifdef MAC
		mac_destroy_mount(mp);
#endif
		crfree(mp->mnt_cred);
		free(mp, M_MOUNT);
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
		TAILQ_FOREACH_REVERSE(mp, &mountlist, mntlist, mnt_list)
			if (mp->mnt_stat.f_fsid.val[0] == id0 &&
			    mp->mnt_stat.f_fsid.val[1] == id1)
				break;
		mtx_unlock(&mountlist_mtx);
	} else {
		mtx_lock(&mountlist_mtx);
		TAILQ_FOREACH_REVERSE(mp, &mountlist, mntlist, mnt_list)
			if (strcmp(mp->mnt_stat.f_mntonname, pathbuf) == 0)
				break;
		mtx_unlock(&mountlist_mtx);
	}
	free(pathbuf, M_TEMP);
	if (mp == NULL)
		return (ENOENT);

	/*
	 * Only root, or the user that did the original mount is
	 * permitted to unmount this filesystem.
	 */
	if (mp->mnt_cred->cr_uid != td->td_ucred->cr_uid) {
		error = suser(td);
		if (error)
			return (error);
	}

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
	mp->mnt_flag &=~ MNT_ASYNC;
	cache_purgevfs(mp);	/* remove cache entries for this file sys */
	if (mp->mnt_syncer != NULL)
		vrele(mp->mnt_syncer);
	/* Move process cdir/rdir refs on fs root to underlying vnode. */
	if (VFS_ROOT(mp, &fsrootvp) == 0) {
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
		if (VFS_ROOT(mp, &fsrootvp) == 0) {
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
	crfree(mp->mnt_cred);
	mtx_lock(&mountlist_mtx);
	TAILQ_REMOVE(&mountlist, mp, mnt_list);
	if ((coveredvp = mp->mnt_vnodecovered) != NULL)
		coveredvp->v_mountedhere = NULL;
	mp->mnt_vfc->vfc_refcount--;
	if (!TAILQ_EMPTY(&mp->mnt_nvnodelist))
		panic("unmount: dangling vnode");
	lockmgr(&mp->mnt_lock, LK_RELEASE | LK_INTERLOCK, &mountlist_mtx, td);
	lockdestroy(&mp->mnt_lock);
	if (coveredvp != NULL)
		vrele(coveredvp);
	if (mp->mnt_kern_flag & MNTK_MWAIT)
		wakeup(mp);
#ifdef MAC
	mac_destroy_mount(mp);
#endif
	if (mp->mnt_op->vfs_mount == NULL)
		vfs_freeopts(mp->mnt_opt);
	free(mp, M_MOUNT);
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

	if (fstypename == NULL)
		return (ENODEV);
	for (vfsp = vfsconf; vfsp; vfsp = vfsp->vfc_next)
		if (!strcmp(vfsp->vfc_name, fstypename))
			break;
	if (vfsp == NULL)
		return (ENODEV);
	mp = malloc((u_long)sizeof(struct mount), M_MOUNT, M_WAITOK | M_ZERO);
	lockinit(&mp->mnt_lock, PVFS, "vfslock", 0, LK_NOPAUSE);
	(void)vfs_busy(mp, LK_NOWAIT, 0, td);
	TAILQ_INIT(&mp->mnt_nvnodelist);
	TAILQ_INIT(&mp->mnt_reservedvnlist);
	mp->mnt_nvnodelistsize = 0;
	mp->mnt_vfc = vfsp;
	mp->mnt_op = vfsp->vfc_vfsops;
	mp->mnt_flag = MNT_RDONLY;
	mp->mnt_vnodecovered = NULLVP;
	mp->mnt_cred = crdup(td->td_ucred);
	vfsp->vfc_refcount++;
	mp->mnt_iosize_max = DFLTPHYS;
	mp->mnt_stat.f_type = vfsp->vfc_typenum;
	mp->mnt_flag |= vfsp->vfc_flags & MNT_VISFLAGMASK;
	strlcpy(mp->mnt_stat.f_fstypename, vfsp->vfc_name, MFSNAMELEN);
	mp->mnt_stat.f_mntonname[0] = '/';
	mp->mnt_stat.f_mntonname[1] = 0;
	strlcpy(mp->mnt_stat.f_mntfromname, devname, MNAMELEN);
#ifdef MAC
	mac_init_mount(mp);
	mac_create_mount(td->td_ucred, mp);
#endif
	*mpp = mp;
	return (0);
}

/*
 * Find and mount the root filesystem
 */
void
vfs_mountroot(void)
{
	char		*cp;
	int		i, error;

	g_waitidle();	

	/* 
	 * The root filesystem information is compiled in, and we are
	 * booted with instructions to use it.
	 */
#ifdef ROOTDEVNAME
	if ((boothowto & RB_DFLTROOT) && 
	    !vfs_mountroot_try(ROOTDEVNAME))
		return;
#endif
	/* 
	 * We are booted with instructions to prompt for the root filesystem,
	 * or to use the compiled-in default when it doesn't exist.
	 */
	if (boothowto & (RB_DFLTROOT | RB_ASKNAME)) {
		if (!vfs_mountroot_ask())
			return;
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
	if ((cp = getenv("vfs.root.mountfrom")) != NULL) {
		error = vfs_mountroot_try(cp);
		freeenv(cp);
		if (!error)
			return;
	}

	/* 
	 * Try values that may have been computed by the machine-dependant
	 * legacy code.
	 */
	if (!vfs_mountroot_try(rootdevnames[0]))
		return;
	if (!vfs_mountroot_try(rootdevnames[1]))
		return;

	/*
	 * If we have a compiled-in default, and haven't already tried it, try
	 * it now.
	 */
#ifdef ROOTDEVNAME
	if (!(boothowto & RB_DFLTROOT))
		if (!vfs_mountroot_try(ROOTDEVNAME))
			return;
#endif

	/* 
	 * Everything so far has failed, prompt on the console if we haven't
	 * already tried that.
	 */
	if (!(boothowto & (RB_DFLTROOT | RB_ASKNAME)) && !vfs_mountroot_ask())
		return;
	panic("Root mount failed, startup aborted.");
}

/*
 * Mount (mountfrom) as the root filesystem.
 */
static int
vfs_mountroot_try(char *mountfrom)
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
		return(error);		/* don't complain */

	s = splcam();			/* Overkill, but annoying without it */
	printf("Mounting root from %s\n", mountfrom);
	splx(s);

	/* parse vfs name and path */
	vfsname = malloc(MFSNAMELEN, M_MOUNT, M_WAITOK);
	path = malloc(MNAMELEN, M_MOUNT, M_WAITOK);
	vfsname[0] = path[0] = 0;
	sprintf(patt, "%%%d[a-z0-9]:%%%zds", MFSNAMELEN, MNAMELEN);
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
	mp->mnt_flag |= MNT_ROOTFS;

	/* do our best to set rootdev */
	if ((path[0] != 0) && setrootbyname(path))
		printf("setrootbyname failed\n");

	/* If the root device is a type "memory disk", mount RW */
	if (rootdev != NODEV && devsw(rootdev) != NULL) {
		devname = devtoname(rootdev);
		if (devname[0] == 'm' && devname[1] == 'd')
			mp->mnt_flag &= ~MNT_RDONLY;
	}

	/* 
	 * Set the mount path to be something useful, because the
	 * filesystem code isn't responsible now for initialising
	 * f_mntonname unless they want to override the default
	 * (which is `path'.)
	 */
	strlcpy(mp->mnt_stat.f_mntonname, "/", MNAMELEN);

	error = VFS_MOUNT(mp, NULL, NULL, NULL, curthread);

done:
	if (vfsname != NULL)
		free(vfsname, M_MOUNT);
	if (path != NULL)
		free(path, M_MOUNT);
	if (error != 0) {
		if (mp != NULL) {
			vfs_unbusy(mp, curthread);
#ifdef MAC
			mac_destroy_mount(mp);
#endif
			crfree(mp->mnt_cred);
			free(mp, M_MOUNT);
		}
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
	return(error);
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
		if (name[0] == 0)
			return(1);
		if (name[0] == '?') {
			printf("\nList of GEOM managed disk devices:\n  ");
			g_dev_print();
			continue;
		}
		if (!vfs_mountroot_try(name))
			return(0);
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
 * Convert a given name to the dev_t of the disk-like device
 * it refers to.
 */
dev_t
getdiskbyname(char *name) {
	char *cp;
	dev_t dev;

	cp = name;
	if (!bcmp(cp, "/dev/", 5))
		cp += 5;

	dev = NODEV;
	EVENTHANDLER_INVOKE(dev_clone, cp, strlen(cp), &dev);
	return (dev);
}

/*
 * Set rootdev to match (name), given that we expect it to
 * refer to a disk-like device.
 */
static int
setrootbyname(char *name)
{
	dev_t diskdev;

	diskdev = getdiskbyname(name);
	if (diskdev != NODEV) {
		rootdev = diskdev;
		return (0);
	}

	return (1);
}

/* Show the dev_t for a disk specified by name */
#ifdef DDB
DB_SHOW_COMMAND(disk, db_getdiskbyname)
{
	dev_t dev;

	if (modif[0] == '\0') {
		db_error("usage: show disk/devicename");
		return;
	}
	dev = getdiskbyname(modif);
	if (dev != NODEV)
		db_printf("dev_t = %p\n", dev);
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
