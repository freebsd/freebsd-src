/*-
 * Copyright (c) 2006-2007 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
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
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/priv.h>
#include <sys/libkern.h>

MALLOC_DECLARE(M_MOUNT);

TAILQ_HEAD(vfsoptlist, vfsopt);
struct vfsopt {
	TAILQ_ENTRY(vfsopt) link;
	char	*name;
	void	*value;
	int	len;
};

void
vfs_setmntopt(vfs_t *vfsp, const char *name, const char *arg,
    int flags __unused)
{
	struct vfsopt *opt;
	size_t namesize;

	if (vfsp->mnt_opt == NULL) {
		vfsp->mnt_opt = malloc(sizeof(*vfsp->mnt_opt), M_MOUNT, M_WAITOK);
		TAILQ_INIT(vfsp->mnt_opt);
	}

	opt = malloc(sizeof(*opt), M_MOUNT, M_WAITOK);

	namesize = strlen(name) + 1;
	opt->name = malloc(namesize, M_MOUNT, M_WAITOK);
	strlcpy(opt->name, name, namesize);

	if (arg == NULL) {
		opt->value = NULL;
		opt->len = 0;
	} else {
		opt->len = strlen(arg) + 1;
		opt->value = malloc(opt->len, M_MOUNT, M_WAITOK);
		bcopy(arg, opt->value, opt->len);
	}
	/* TODO: Locking. */
	TAILQ_INSERT_TAIL(vfsp->mnt_opt, opt, link);
}

void
vfs_clearmntopt(vfs_t *vfsp, const char *name)
{
	struct vfsopt *opt;

	if (vfsp->mnt_opt == NULL)
		return;
	/* TODO: Locking. */
	TAILQ_FOREACH(opt, vfsp->mnt_opt, link) {
		if (strcmp(opt->name, name) == 0)
			break;
	}
	if (opt != NULL) {
		TAILQ_REMOVE(vfsp->mnt_opt, opt, link);
		free(opt->name, M_MOUNT);
		if (opt->value != NULL)
			free(opt->value, M_MOUNT);
		free(opt, M_MOUNT);
	}
}

int
vfs_optionisset(const vfs_t *vfsp, const char *opt, char **argp)
{
	struct vfsoptlist *opts = vfsp->mnt_opt;
	int error;

	if (opts == NULL)
		return (0);
	error = vfs_getopt(opts, opt, (void **)argp, NULL);
	return (error != 0 ? 0 : 1);
}

int
traverse(vnode_t **cvpp, int lktype)
{
	kthread_t *td = curthread;
	vnode_t *cvp;
	vnode_t *tvp;
	vfs_t *vfsp;
	int error;

	cvp = *cvpp;
	tvp = NULL;

	/*
	 * If this vnode is mounted on, then we transparently indirect
	 * to the vnode which is the root of the mounted file system.
	 * Before we do this we must check that an unmount is not in
	 * progress on this vnode.
	 */

	for (;;) {
		/*
		 * Reached the end of the mount chain?
		 */
		vfsp = vn_mountedvfs(cvp);
		if (vfsp == NULL)
			break;
		/*
		 * tvp is NULL for *cvpp vnode, which we can't unlock.
		 */
		if (tvp != NULL)
			vput(cvp);
		else
			vrele(cvp);

		/*
		 * The read lock must be held across the call to VFS_ROOT() to
		 * prevent a concurrent unmount from destroying the vfs.
		 */
		error = VFS_ROOT(vfsp, lktype, &tvp, td);
		if (error != 0)
			return (error);
		cvp = tvp;
	}

	*cvpp = cvp;
	return (0);
}

int
domount(kthread_t *td, vnode_t *vp, const char *fstype, char *fspath,
    char *fspec, int fsflags)
{
	struct mount *mp;
	struct vfsconf *vfsp;
	struct ucred *newcr, *oldcr;
	int error;

	/*
	 * Be ultra-paranoid about making sure the type and fspath
	 * variables will fit in our mp buffers, including the
	 * terminating NUL.
	 */
	if (strlen(fstype) >= MFSNAMELEN || strlen(fspath) >= MNAMELEN)
		return (ENAMETOOLONG);

	vfsp = vfs_byname_kld(fstype, td, &error);
	if (vfsp == NULL)
		return (ENODEV);

	if (vp->v_type != VDIR)
		return (ENOTDIR);
	VI_LOCK(vp);
	if ((vp->v_iflag & VI_MOUNT) != 0 ||
	    vp->v_mountedhere != NULL) {
		VI_UNLOCK(vp);
		return (EBUSY);
	}
	vp->v_iflag |= VI_MOUNT;
	VI_UNLOCK(vp);

	/*
	 * Allocate and initialize the filesystem.
	 */
	vn_lock(vp, LK_SHARED | LK_RETRY, td);
	mp = vfs_mount_alloc(vp, vfsp, fspath, td);
	VOP_UNLOCK(vp, 0, td);

	mp->mnt_optnew = NULL;
	vfs_setmntopt(mp, "from", fspec, 0);
	mp->mnt_optnew = mp->mnt_opt;
	mp->mnt_opt = NULL;

	/*
	 * Set the mount level flags.
	 * crdup() can sleep, so do it before acquiring a mutex.
	 */
	newcr = crdup(kcred);
	MNT_ILOCK(mp);
	if (fsflags & MNT_RDONLY)
		mp->mnt_flag |= MNT_RDONLY;
	mp->mnt_flag &=~ MNT_UPDATEMASK;
	mp->mnt_flag |= fsflags & (MNT_UPDATEMASK | MNT_FORCE | MNT_ROOTFS);
	/*
	 * Unprivileged user can trigger mounting a snapshot, but we don't want
	 * him to unmount it, so we switch to privileged credentials.
	 */
	oldcr = mp->mnt_cred;
	mp->mnt_cred = newcr;
	mp->mnt_stat.f_owner = mp->mnt_cred->cr_uid;
	MNT_IUNLOCK(mp);
	crfree(oldcr);
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
		(void)VFS_STATFS(mp, &mp->mnt_stat, td);
	}
	/*
	 * Prevent external consumers of mount options from reading
	 * mnt_optnew.
	*/
	mp->mnt_optnew = NULL;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	/*
	 * Put the new filesystem on the mount list after root.
	 */
#ifdef FREEBSD_NAMECACHE
	cache_purge(vp);
#endif
	if (!error) {
		vnode_t *mvp;

		VI_LOCK(vp);
		vp->v_iflag &= ~VI_MOUNT;
		VI_UNLOCK(vp);
		vp->v_mountedhere = mp;
		mtx_lock(&mountlist_mtx);
		TAILQ_INSERT_TAIL(&mountlist, mp, mnt_list);
		mtx_unlock(&mountlist_mtx);
		vfs_event_signal(NULL, VQ_MOUNT, 0);
		if (VFS_ROOT(mp, LK_EXCLUSIVE, &mvp, td))
			panic("mount: lost mount");
		mountcheckdirs(vp, mvp);
		vput(mvp);
		VOP_UNLOCK(vp, 0, td);
		if ((mp->mnt_flag & MNT_RDONLY) == 0)
			error = vfs_allocate_syncvnode(mp);
		vfs_unbusy(mp, td);
		if (error)
			vrele(vp);
		else
			vfs_mountedfrom(mp, fspec);
	} else {
		VI_LOCK(vp);
		vp->v_iflag &= ~VI_MOUNT;
		VI_UNLOCK(vp);
		VOP_UNLOCK(vp, 0, td);
		vfs_unbusy(mp, td);
		vfs_mount_destroy(mp);
	}
	return (error);
}
