/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/smp.h>
#include <sys/devctl.h>
#include <sys/eventhandler.h>
#include <sys/fcntl.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/libkern.h>
#include <sys/limits.h>
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
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <sys/vnode.h>
#include <vm/uma.h>

#include <geom/geom.h>

#include <machine/stdarg.h>

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

#define	VFS_MOUNTARG_SIZE_MAX	(1024 * 64)

static int	vfs_domount(struct thread *td, const char *fstype, char *fspath,
		    uint64_t fsflags, bool jail_export,
		    struct vfsoptlist **optlist);
static void	free_mntarg(struct mntarg *ma);

static int	usermount = 0;
SYSCTL_INT(_vfs, OID_AUTO, usermount, CTLFLAG_RW, &usermount, 0,
    "Unprivileged users may mount and unmount file systems");

static bool	default_autoro = false;
SYSCTL_BOOL(_vfs, OID_AUTO, default_autoro, CTLFLAG_RW, &default_autoro, 0,
    "Retry failed r/w mount as r/o if no explicit ro/rw option is specified");

static bool	recursive_forced_unmount = false;
SYSCTL_BOOL(_vfs, OID_AUTO, recursive_forced_unmount, CTLFLAG_RW,
    &recursive_forced_unmount, 0, "Recursively unmount stacked upper mounts"
    " when a file system is forcibly unmounted");

static SYSCTL_NODE(_vfs, OID_AUTO, deferred_unmount,
    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "deferred unmount controls");

static unsigned int	deferred_unmount_retry_limit = 10;
SYSCTL_UINT(_vfs_deferred_unmount, OID_AUTO, retry_limit, CTLFLAG_RW,
    &deferred_unmount_retry_limit, 0,
    "Maximum number of retries for deferred unmount failure");

static int	deferred_unmount_retry_delay_hz;
SYSCTL_INT(_vfs_deferred_unmount, OID_AUTO, retry_delay_hz, CTLFLAG_RW,
    &deferred_unmount_retry_delay_hz, 0,
    "Delay in units of [1/kern.hz]s when retrying a failed deferred unmount");

static int	deferred_unmount_total_retries = 0;
SYSCTL_INT(_vfs_deferred_unmount, OID_AUTO, total_retries, CTLFLAG_RD,
    &deferred_unmount_total_retries, 0,
    "Total number of retried deferred unmounts");

MALLOC_DEFINE(M_MOUNT, "mount", "vfs mount structure");
MALLOC_DEFINE(M_STATFS, "statfs", "statfs structure");
static uma_zone_t mount_zone;

/* List of mounted filesystems. */
struct mntlist mountlist = TAILQ_HEAD_INITIALIZER(mountlist);

/* For any iteration/modification of mountlist */
struct mtx_padalign __exclusive_cache_line mountlist_mtx;

EVENTHANDLER_LIST_DEFINE(vfs_mounted);
EVENTHANDLER_LIST_DEFINE(vfs_unmounted);

static void vfs_deferred_unmount(void *arg, int pending);
static struct timeout_task deferred_unmount_task;
static struct mtx deferred_unmount_lock;
MTX_SYSINIT(deferred_unmount, &deferred_unmount_lock, "deferred_unmount",
    MTX_DEF);
static STAILQ_HEAD(, mount) deferred_unmount_list =
    STAILQ_HEAD_INITIALIZER(deferred_unmount_list);
TASKQUEUE_DEFINE_THREAD(deferred_unmount);

static void mount_devctl_event(const char *type, struct mount *mp, bool donew);

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
	mtx_init(&mp->mnt_listmtx, "struct mount vlist mtx", NULL, MTX_DEF);
	lockinit(&mp->mnt_explock, PVFS, "explock", 0, 0);
	mp->mnt_pcpu = uma_zalloc_pcpu(pcpu_zone_16, M_WAITOK | M_ZERO);
	mp->mnt_ref = 0;
	mp->mnt_vfs_ops = 1;
	mp->mnt_rootvnode = NULL;
	return (0);
}

static void
mount_fini(void *mem, int size)
{
	struct mount *mp;

	mp = (struct mount *)mem;
	uma_zfree_pcpu(pcpu_zone_16, mp->mnt_pcpu);
	lockdestroy(&mp->mnt_explock);
	mtx_destroy(&mp->mnt_listmtx);
	mtx_destroy(&mp->mnt_mtx);
}

static void
vfs_mount_init(void *dummy __unused)
{
	TIMEOUT_TASK_INIT(taskqueue_deferred_unmount, &deferred_unmount_task,
	    0, vfs_deferred_unmount, NULL);
	deferred_unmount_retry_delay_hz = hz;
	mount_zone = uma_zcreate("Mountpoints", sizeof(struct mount), NULL,
	    NULL, mount_init, mount_fini, UMA_ALIGN_CACHE, UMA_ZONE_NOFREE);
	mtx_init(&mountlist_mtx, "mountlist", NULL, MTX_DEF);
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
 * the last occurrence of it.
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
#ifndef _SYS_SYSPROTO_H_
struct nmount_args {
	struct iovec *iovp;
	unsigned int iovcnt;
	int flags;
};
#endif
int
sys_nmount(struct thread *td, struct nmount_args *uap)
{
	struct uio *auio;
	int error;
	u_int iovcnt;
	uint64_t flags;

	/*
	 * Mount flags are now 64-bits. On 32-bit archtectures only
	 * 32-bits are passed in, but from here on everything handles
	 * 64-bit flags correctly.
	 */
	flags = uap->flags;

	AUDIT_ARG_FFLAGS(flags);
	CTR4(KTR_VFS, "%s: iovp %p with iovcnt %d and flags %d", __func__,
	    uap->iovp, uap->iovcnt, flags);

	/*
	 * Filter out MNT_ROOTFS.  We do not want clients of nmount() in
	 * userspace to set this flag, but we must filter it out if we want
	 * MNT_UPDATE on the root file system to work.
	 * MNT_ROOTFS should only be set by the kernel when mounting its
	 * root file system.
	 */
	flags &= ~MNT_ROOTFS;

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
	error = vfs_donmount(td, flags, auio);

	free(auio, M_IOV);
	return (error);
}

/*
 * ---------------------------------------------------------------------
 * Various utility functions
 */

/*
 * Get a reference on a mount point from a vnode.
 *
 * The vnode is allowed to be passed unlocked and race against dooming. Note in
 * such case there are no guarantees the referenced mount point will still be
 * associated with it after the function returns.
 */
struct mount *
vfs_ref_from_vp(struct vnode *vp)
{
	struct mount *mp;
	struct mount_pcpu *mpcpu;

	mp = atomic_load_ptr(&vp->v_mount);
	if (__predict_false(mp == NULL)) {
		return (mp);
	}
	if (vfs_op_thread_enter(mp, mpcpu)) {
		if (__predict_true(mp == vp->v_mount)) {
			vfs_mp_count_add_pcpu(mpcpu, ref, 1);
			vfs_op_thread_exit(mp, mpcpu);
		} else {
			vfs_op_thread_exit(mp, mpcpu);
			mp = NULL;
		}
	} else {
		MNT_ILOCK(mp);
		if (mp == vp->v_mount) {
			MNT_REF(mp);
			MNT_IUNLOCK(mp);
		} else {
			MNT_IUNLOCK(mp);
			mp = NULL;
		}
	}
	return (mp);
}

void
vfs_ref(struct mount *mp)
{
	struct mount_pcpu *mpcpu;

	CTR2(KTR_VFS, "%s: mp %p", __func__, mp);
	if (vfs_op_thread_enter(mp, mpcpu)) {
		vfs_mp_count_add_pcpu(mpcpu, ref, 1);
		vfs_op_thread_exit(mp, mpcpu);
		return;
	}

	MNT_ILOCK(mp);
	MNT_REF(mp);
	MNT_IUNLOCK(mp);
}

/*
 * Register ump as an upper mount of the mount associated with
 * vnode vp.  This registration will be tracked through
 * mount_upper_node upper, which should be allocated by the
 * caller and stored in per-mount data associated with mp.
 *
 * If successful, this function will return the mount associated
 * with vp, and will ensure that it cannot be unmounted until
 * ump has been unregistered as one of its upper mounts.
 * 
 * Upon failure this function will return NULL.
 */
struct mount *
vfs_register_upper_from_vp(struct vnode *vp, struct mount *ump,
    struct mount_upper_node *upper)
{
	struct mount *mp;

	mp = atomic_load_ptr(&vp->v_mount);
	if (mp == NULL)
		return (NULL);
	MNT_ILOCK(mp);
	if (mp != vp->v_mount ||
	    ((mp->mnt_kern_flag & (MNTK_UNMOUNT | MNTK_RECURSE)) != 0)) {
		MNT_IUNLOCK(mp);
		return (NULL);
	}
	KASSERT(ump != mp, ("upper and lower mounts are identical"));
	upper->mp = ump;
	MNT_REF(mp);
	TAILQ_INSERT_TAIL(&mp->mnt_uppers, upper, mnt_upper_link);
	MNT_IUNLOCK(mp);
	return (mp);
}

/*
 * Register upper mount ump to receive vnode unlink/reclaim
 * notifications from lower mount mp. This registration will
 * be tracked through mount_upper_node upper, which should be
 * allocated by the caller and stored in per-mount data
 * associated with mp.
 *
 * ump must already be registered as an upper mount of mp
 * through a call to vfs_register_upper_from_vp().
 */
void
vfs_register_for_notification(struct mount *mp, struct mount *ump,
    struct mount_upper_node *upper)
{
	upper->mp = ump;
	MNT_ILOCK(mp);
	TAILQ_INSERT_TAIL(&mp->mnt_notify, upper, mnt_upper_link);
	MNT_IUNLOCK(mp);
}

static void
vfs_drain_upper_locked(struct mount *mp)
{
	mtx_assert(MNT_MTX(mp), MA_OWNED);
	while (mp->mnt_upper_pending != 0) {
		mp->mnt_kern_flag |= MNTK_UPPER_WAITER;
		msleep(&mp->mnt_uppers, MNT_MTX(mp), 0, "mntupw", 0);
	}
}

/*
 * Undo a previous call to vfs_register_for_notification().
 * The mount represented by upper must be currently registered
 * as an upper mount for mp.
 */
void
vfs_unregister_for_notification(struct mount *mp,
    struct mount_upper_node *upper)
{
	MNT_ILOCK(mp);
	vfs_drain_upper_locked(mp);
	TAILQ_REMOVE(&mp->mnt_notify, upper, mnt_upper_link);
	MNT_IUNLOCK(mp);
}

/*
 * Undo a previous call to vfs_register_upper_from_vp().
 * This must be done before mp can be unmounted.
 */
void
vfs_unregister_upper(struct mount *mp, struct mount_upper_node *upper)
{
	MNT_ILOCK(mp);
	KASSERT((mp->mnt_kern_flag & MNTK_UNMOUNT) == 0,
	    ("registered upper with pending unmount"));
	vfs_drain_upper_locked(mp);
	TAILQ_REMOVE(&mp->mnt_uppers, upper, mnt_upper_link);
	if ((mp->mnt_kern_flag & MNTK_TASKQUEUE_WAITER) != 0 &&
	    TAILQ_EMPTY(&mp->mnt_uppers)) {
		mp->mnt_kern_flag &= ~MNTK_TASKQUEUE_WAITER;
		wakeup(&mp->mnt_taskqueue_link);
	}
	MNT_REL(mp);
	MNT_IUNLOCK(mp);
}

void
vfs_rel(struct mount *mp)
{
	struct mount_pcpu *mpcpu;

	CTR2(KTR_VFS, "%s: mp %p", __func__, mp);
	if (vfs_op_thread_enter(mp, mpcpu)) {
		vfs_mp_count_sub_pcpu(mpcpu, ref, 1);
		vfs_op_thread_exit(mp, mpcpu);
		return;
	}

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
	mp->mnt_kern_flag = 0;
	mp->mnt_flag = 0;
	mp->mnt_rootvnode = NULL;
	mp->mnt_vnodecovered = NULL;
	mp->mnt_op = NULL;
	mp->mnt_vfc = NULL;
	TAILQ_INIT(&mp->mnt_nvnodelist);
	mp->mnt_nvnodelistsize = 0;
	TAILQ_INIT(&mp->mnt_lazyvnodelist);
	mp->mnt_lazyvnodelistsize = 0;
	MPPASS(mp->mnt_ref == 0 && mp->mnt_lockref == 0 &&
	    mp->mnt_writeopcount == 0, mp);
	MPASSERT(mp->mnt_vfs_ops == 1, mp,
	    ("vfs_ops should be 1 but %d found", mp->mnt_vfs_ops));
	(void) vfs_busy(mp, MBF_NOWAIT);
	atomic_add_acq_int(&vfsp->vfc_refcount, 1);
	mp->mnt_op = vfsp->vfc_vfsops;
	mp->mnt_vfc = vfsp;
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
	mp->mnt_upper_pending = 0;
	TAILQ_INIT(&mp->mnt_uppers);
	TAILQ_INIT(&mp->mnt_notify);
	mp->mnt_taskqueue_flags = 0;
	mp->mnt_unmount_retries = 0;
	return (mp);
}

/*
 * Destroy the mount struct previously allocated by vfs_mount_alloc().
 */
void
vfs_mount_destroy(struct mount *mp)
{

	MPPASS(mp->mnt_vfs_ops != 0, mp);

	vfs_assert_mount_counters(mp);

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
	MPPASS(mp->mnt_writeopcount == 0, mp);
	MPPASS(mp->mnt_secondary_writes == 0, mp);
	atomic_subtract_rel_int(&mp->mnt_vfc->vfc_refcount, 1);
	if (!TAILQ_EMPTY(&mp->mnt_nvnodelist)) {
		struct vnode *vp;

		TAILQ_FOREACH(vp, &mp->mnt_nvnodelist, v_nmntvnodes)
			vn_printf(vp, "dangling vnode ");
		panic("unmount: dangling vnode");
	}
	KASSERT(mp->mnt_upper_pending == 0, ("mnt_upper_pending"));
	KASSERT(TAILQ_EMPTY(&mp->mnt_uppers), ("mnt_uppers"));
	KASSERT(TAILQ_EMPTY(&mp->mnt_notify), ("mnt_notify"));
	MPPASS(mp->mnt_nvnodelistsize == 0, mp);
	MPPASS(mp->mnt_lazyvnodelistsize == 0, mp);
	MPPASS(mp->mnt_lockref == 0, mp);
	MNT_IUNLOCK(mp);

	MPASSERT(mp->mnt_vfs_ops == 1, mp,
	    ("vfs_ops should be 1 but %d found", mp->mnt_vfs_ops));

	MPASSERT(mp->mnt_rootvnode == NULL, mp,
	    ("mount point still has a root vnode %p", mp->mnt_rootvnode));

	if (mp->mnt_vnodecovered != NULL)
		vrele(mp->mnt_vnodecovered);
#ifdef MAC
	mac_mount_destroy(mp);
#endif
	if (mp->mnt_opt != NULL)
		vfs_freeopts(mp->mnt_opt);
	if (mp->mnt_exjail != NULL) {
		atomic_subtract_int(&mp->mnt_exjail->cr_prison->pr_exportcnt,
		    1);
		crfree(mp->mnt_exjail);
	}
	if (mp->mnt_export != NULL) {
		vfs_free_addrlist(mp->mnt_export);
		free(mp->mnt_export, M_MOUNT);
	}
	crfree(mp->mnt_cred);
	uma_zfree(mount_zone, mp);
}

static bool
vfs_should_downgrade_to_ro_mount(uint64_t fsflags, int error)
{
	/* This is an upgrade of an exisiting mount. */
	if ((fsflags & MNT_UPDATE) != 0)
		return (false);
	/* This is already an R/O mount. */
	if ((fsflags & MNT_RDONLY) != 0)
		return (false);

	switch (error) {
	case ENODEV:	/* generic, geom, ... */
	case EACCES:	/* cam/scsi, ... */
	case EROFS:	/* md, mmcsd, ... */
		/*
		 * These errors can be returned by the storage layer to signal
		 * that the media is read-only.  No harm in the R/O mount
		 * attempt if the error was returned for some other reason.
		 */
		return (true);
	default:
		return (false);
	}
}

int
vfs_donmount(struct thread *td, uint64_t fsflags, struct uio *fsoptions)
{
	struct vfsoptlist *optlist;
	struct vfsopt *opt, *tmp_opt;
	char *fstype, *fspath, *errmsg;
	int error, fstypelen, fspathlen, errmsg_len, errmsg_pos;
	bool autoro, has_nonexport, jail_export;

	errmsg = fspath = NULL;
	errmsg_len = fspathlen = 0;
	errmsg_pos = -1;
	autoro = default_autoro;

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
	if (error || fstypelen <= 0 || fstype[fstypelen - 1] != '\0') {
		error = EINVAL;
		if (errmsg != NULL)
			strncpy(errmsg, "Invalid fstype", errmsg_len);
		goto bail;
	}
	fspathlen = 0;
	error = vfs_getopt(optlist, "fspath", (void **)&fspath, &fspathlen);
	if (error || fspathlen <= 0 || fspath[fspathlen - 1] != '\0') {
		error = EINVAL;
		if (errmsg != NULL)
			strncpy(errmsg, "Invalid fspath", errmsg_len);
		goto bail;
	}

	/*
	 * Check to see that "export" is only used with the "update", "fstype",
	 * "fspath", "from" and "errmsg" options when in a vnet jail.
	 * These are the ones used to set/update exports by mountd(8).
	 * If only the above options are set in a jail that can run mountd(8),
	 * then the jail_export argument of vfs_domount() will be true.
	 * When jail_export is true, the vfs_suser() check does not cause
	 * failure, but limits the update to exports only.
	 * This allows mountd(8) running within the vnet jail
	 * to export file systems visible within the jail, but
	 * mounted outside of the jail.
	 */
	/*
	 * We need to see if we have the "update" option
	 * before we call vfs_domount(), since vfs_domount() has special
	 * logic based on MNT_UPDATE.  This is very important
	 * when we want to update the root filesystem.
	 */
	has_nonexport = false;
	jail_export = false;
	TAILQ_FOREACH_SAFE(opt, optlist, link, tmp_opt) {
		int do_freeopt = 0;

		if (jailed(td->td_ucred) &&
		    strcmp(opt->name, "export") != 0 &&
		    strcmp(opt->name, "update") != 0 &&
		    strcmp(opt->name, "fstype") != 0 &&
		    strcmp(opt->name, "fspath") != 0 &&
		    strcmp(opt->name, "from") != 0 &&
		    strcmp(opt->name, "errmsg") != 0)
			has_nonexport = true;
		if (strcmp(opt->name, "update") == 0) {
			fsflags |= MNT_UPDATE;
			do_freeopt = 1;
		}
		else if (strcmp(opt->name, "async") == 0)
			fsflags |= MNT_ASYNC;
		else if (strcmp(opt->name, "force") == 0) {
			fsflags |= MNT_FORCE;
			do_freeopt = 1;
		}
		else if (strcmp(opt->name, "reload") == 0) {
			fsflags |= MNT_RELOAD;
			do_freeopt = 1;
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
			autoro = false;
		}
		else if (strcmp(opt->name, "rw") == 0) {
			fsflags &= ~MNT_RDONLY;
			autoro = false;
		}
		else if (strcmp(opt->name, "ro") == 0) {
			fsflags |= MNT_RDONLY;
			autoro = false;
		}
		else if (strcmp(opt->name, "rdonly") == 0) {
			free(opt->name, M_MOUNT);
			opt->name = strdup("ro", M_MOUNT);
			fsflags |= MNT_RDONLY;
			autoro = false;
		}
		else if (strcmp(opt->name, "autoro") == 0) {
			do_freeopt = 1;
			autoro = true;
		}
		else if (strcmp(opt->name, "suiddir") == 0)
			fsflags |= MNT_SUIDDIR;
		else if (strcmp(opt->name, "sync") == 0)
			fsflags |= MNT_SYNCHRONOUS;
		else if (strcmp(opt->name, "union") == 0)
			fsflags |= MNT_UNION;
		else if (strcmp(opt->name, "export") == 0) {
			fsflags |= MNT_EXPORTED;
			jail_export = true;
		} else if (strcmp(opt->name, "automounted") == 0) {
			fsflags |= MNT_AUTOMOUNTED;
			do_freeopt = 1;
		} else if (strcmp(opt->name, "nocover") == 0) {
			fsflags |= MNT_NOCOVER;
			do_freeopt = 1;
		} else if (strcmp(opt->name, "cover") == 0) {
			fsflags &= ~MNT_NOCOVER;
			do_freeopt = 1;
		} else if (strcmp(opt->name, "emptydir") == 0) {
			fsflags |= MNT_EMPTYDIR;
			do_freeopt = 1;
		} else if (strcmp(opt->name, "noemptydir") == 0) {
			fsflags &= ~MNT_EMPTYDIR;
			do_freeopt = 1;
		}
		if (do_freeopt)
			vfs_freeopt(optlist, opt);
	}

	/*
	 * Be ultra-paranoid about making sure the type and fspath
	 * variables will fit in our mp buffers, including the
	 * terminating NUL.
	 */
	if (fstypelen > MFSNAMELEN || fspathlen > MNAMELEN) {
		error = ENAMETOOLONG;
		goto bail;
	}

	/*
	 * If has_nonexport is true or the caller is not running within a
	 * vnet prison that can run mountd(8), set jail_export false.
	 */
	if (has_nonexport || !jailed(td->td_ucred) ||
	    !prison_check_nfsd(td->td_ucred))
		jail_export = false;

	error = vfs_domount(td, fstype, fspath, fsflags, jail_export, &optlist);
	if (error == ENODEV) {
		error = EINVAL;
		if (errmsg != NULL)
			strncpy(errmsg, "Invalid fstype", errmsg_len);
		goto bail;
	}

	/*
	 * See if we can mount in the read-only mode if the error code suggests
	 * that it could be possible and the mount options allow for that.
	 * Never try it if "[no]{ro|rw}" has been explicitly requested and not
	 * overridden by "autoro".
	 */
	if (autoro && vfs_should_downgrade_to_ro_mount(fsflags, error)) {
		printf("%s: R/W mount failed, possibly R/O media,"
		    " trying R/O mount\n", __func__);
		fsflags |= MNT_RDONLY;
		error = vfs_domount(td, fstype, fspath, fsflags, jail_export,
		    &optlist);
	}
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
sys_mount(struct thread *td, struct mount_args *uap)
{
	char *fstype;
	struct vfsconf *vfsp = NULL;
	struct mntarg *ma = NULL;
	uint64_t flags;
	int error;

	/*
	 * Mount flags are now 64-bits. On 32-bit architectures only
	 * 32-bits are passed in, but from here on everything handles
	 * 64-bit flags correctly.
	 */
	flags = uap->flags;

	AUDIT_ARG_FFLAGS(flags);

	/*
	 * Filter out MNT_ROOTFS.  We do not want clients of mount() in
	 * userspace to set this flag, but we must filter it out if we want
	 * MNT_UPDATE on the root file system to work.
	 * MNT_ROOTFS should only be set by the kernel when mounting its
	 * root file system.
	 */
	flags &= ~MNT_ROOTFS;

	fstype = malloc(MFSNAMELEN, M_TEMP, M_WAITOK);
	error = copyinstr(uap->type, fstype, MFSNAMELEN, NULL);
	if (error) {
		free(fstype, M_TEMP);
		return (error);
	}

	AUDIT_ARG_TEXT(fstype);
	vfsp = vfs_byname_kld(fstype, td, &error);
	free(fstype, M_TEMP);
	if (vfsp == NULL)
		return (EINVAL);
	if (((vfsp->vfc_flags & VFCF_SBDRY) != 0 &&
	    vfsp->vfc_vfsops_sd->vfs_cmount == NULL) ||
	    ((vfsp->vfc_flags & VFCF_SBDRY) == 0 &&
	    vfsp->vfc_vfsops->vfs_cmount == NULL))
		return (EOPNOTSUPP);

	ma = mount_argsu(ma, "fstype", uap->type, MFSNAMELEN);
	ma = mount_argsu(ma, "fspath", uap->path, MNAMELEN);
	ma = mount_argb(ma, flags & MNT_RDONLY, "noro");
	ma = mount_argb(ma, !(flags & MNT_NOSUID), "nosuid");
	ma = mount_argb(ma, !(flags & MNT_NOEXEC), "noexec");

	if ((vfsp->vfc_flags & VFCF_SBDRY) != 0)
		return (vfsp->vfc_vfsops_sd->vfs_cmount(ma, uap->data, flags));
	return (vfsp->vfc_vfsops->vfs_cmount(ma, uap->data, flags));
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
	uint64_t fsflags,		/* Flags common to all filesystems. */
	struct vfsoptlist **optlist	/* Options local to the filesystem. */
	)
{
	struct vattr va;
	struct mount *mp;
	struct vnode *newdp, *rootvp;
	int error, error1;
	bool unmounted;

	ASSERT_VOP_ELOCKED(vp, __func__);
	KASSERT((fsflags & MNT_UPDATE) == 0, ("MNT_UPDATE shouldn't be here"));

	/*
	 * If the jail of the calling thread lacks permission for this type of
	 * file system, or is trying to cover its own root, deny immediately.
	 */
	if (jailed(td->td_ucred) && (!prison_allow(td->td_ucred,
	    vfsp->vfc_prison_flag) || vp == td->td_ucred->cr_prison->pr_root)) {
		vput(vp);
		return (EPERM);
	}

	/*
	 * If the user is not root, ensure that they own the directory
	 * onto which we are attempting to mount.
	 */
	error = VOP_GETATTR(vp, &va, td->td_ucred);
	if (error == 0 && va.va_uid != td->td_ucred->cr_uid)
		error = priv_check_cred(td->td_ucred, PRIV_VFS_ADMIN);
	if (error == 0)
		error = vinvalbuf(vp, V_SAVE, 0, 0);
	if (vfsp->vfc_flags & VFCF_FILEMOUNT) {
		if (error == 0 && vp->v_type != VDIR && vp->v_type != VREG)
			error = EINVAL;
		/*
		 * For file mounts, ensure that there is only one hardlink to the file.
		 */
		if (error == 0 && vp->v_type == VREG && va.va_nlink != 1)
			error = EINVAL;
	} else {
		if (error == 0 && vp->v_type != VDIR)
			error = ENOTDIR;
	}
	if (error == 0 && (fsflags & MNT_EMPTYDIR) != 0)
		error = vn_dir_check_empty(vp);
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
	vn_seqc_write_begin(vp);
	VOP_UNLOCK(vp);

	/* Allocate and initialize the filesystem. */
	mp = vfs_mount_alloc(vp, vfsp, fspath, td->td_ucred);
	/* XXXMAC: pass to vfs_mount_alloc? */
	mp->mnt_optnew = *optlist;
	/* Set the mount level flags. */
	mp->mnt_flag = (fsflags &
	    (MNT_UPDATEMASK | MNT_ROOTFS | MNT_RDONLY | MNT_FORCE));

	/*
	 * Mount the filesystem.
	 * XXX The final recipients of VFS_MOUNT just overwrite the ndp they
	 * get.  No freeing of cn_pnbuf.
	 */
	error1 = 0;
	unmounted = true;
	if ((error = VFS_MOUNT(mp)) != 0 ||
	    (error1 = VFS_STATFS(mp, &mp->mnt_stat)) != 0 ||
	    (error1 = VFS_ROOT(mp, LK_EXCLUSIVE, &newdp)) != 0) {
		rootvp = NULL;
		if (error1 != 0) {
			MPASS(error == 0);
			rootvp = vfs_cache_root_clear(mp);
			if (rootvp != NULL) {
				vhold(rootvp);
				vrele(rootvp);
			}
			(void)vn_start_write(NULL, &mp, V_WAIT);
			MNT_ILOCK(mp);
			mp->mnt_kern_flag |= MNTK_UNMOUNT | MNTK_UNMOUNTF;
			MNT_IUNLOCK(mp);
			VFS_PURGE(mp);
			error = VFS_UNMOUNT(mp, 0);
			vn_finished_write(mp);
			if (error != 0) {
				printf(
		    "failed post-mount (%d): rollback unmount returned %d\n",
				    error1, error);
				unmounted = false;
			}
			error = error1;
		}
		vfs_unbusy(mp);
		mp->mnt_vnodecovered = NULL;
		if (unmounted) {
			/* XXXKIB wait for mnt_lockref drain? */
			vfs_mount_destroy(mp);
		}
		VI_LOCK(vp);
		vp->v_iflag &= ~VI_MOUNT;
		VI_UNLOCK(vp);
		if (rootvp != NULL) {
			vn_seqc_write_end(rootvp);
			vdrop(rootvp);
		}
		vn_seqc_write_end(vp);
		vrele(vp);
		return (error);
	}
	vn_seqc_write_begin(newdp);
	VOP_UNLOCK(newdp);

	if (mp->mnt_opt != NULL)
		vfs_freeopts(mp->mnt_opt);
	mp->mnt_opt = mp->mnt_optnew;
	*optlist = NULL;

	/*
	 * Prevent external consumers of mount options from reading mnt_optnew.
	 */
	mp->mnt_optnew = NULL;

	MNT_ILOCK(mp);
	if ((mp->mnt_flag & MNT_ASYNC) != 0 &&
	    (mp->mnt_kern_flag & MNTK_NOASYNC) == 0)
		mp->mnt_kern_flag |= MNTK_ASYNC;
	else
		mp->mnt_kern_flag &= ~MNTK_ASYNC;
	MNT_IUNLOCK(mp);

	/*
	 * VIRF_MOUNTPOINT and v_mountedhere need to be set under the
	 * vp lock to satisfy vfs_lookup() requirements.
	 */
	VOP_LOCK(vp, LK_EXCLUSIVE | LK_RETRY);
	VI_LOCK(vp);
	vn_irflag_set_locked(vp, VIRF_MOUNTPOINT);
	vp->v_mountedhere = mp;
	VI_UNLOCK(vp);
	VOP_UNLOCK(vp);
	cache_purge(vp);

	/*
	 * We need to lock both vnodes.
	 *
	 * Use vn_lock_pair to avoid establishing an ordering between vnodes
	 * from different filesystems.
	 */
	vn_lock_pair(vp, false, LK_EXCLUSIVE, newdp, false, LK_EXCLUSIVE);

	VI_LOCK(vp);
	vp->v_iflag &= ~VI_MOUNT;
	VI_UNLOCK(vp);
	/* Place the new filesystem at the end of the mount list. */
	mtx_lock(&mountlist_mtx);
	TAILQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	mtx_unlock(&mountlist_mtx);
	vfs_event_signal(NULL, VQ_MOUNT, 0);
	VOP_UNLOCK(vp);
	EVENTHANDLER_DIRECT_INVOKE(vfs_mounted, mp, newdp, td);
	VOP_UNLOCK(newdp);
	mount_devctl_event("MOUNT", mp, false);
	mountcheckdirs(vp, newdp);
	vn_seqc_write_end(vp);
	vn_seqc_write_end(newdp);
	vrele(newdp);
	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		vfs_allocate_syncvnode(mp);
	vfs_op_exit(mp);
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
	uint64_t fsflags,		/* Flags common to all filesystems. */
	bool jail_export,		/* Got export option in vnet prison. */
	struct vfsoptlist **optlist	/* Options local to the filesystem. */
	)
{
	struct export_args export;
	struct o2export_args o2export;
	struct vnode *rootvp;
	void *bufp;
	struct mount *mp;
	int error, export_error, i, len, fsid_up_len;
	uint64_t flag;
	gid_t *grps;
	fsid_t *fsid_up;
	bool vfs_suser_failed;

	ASSERT_VOP_ELOCKED(vp, __func__);
	KASSERT((fsflags & MNT_UPDATE) != 0, ("MNT_UPDATE should be here"));
	mp = vp->v_mount;

	if ((vp->v_vflag & VV_ROOT) == 0) {
		if (vfs_copyopt(*optlist, "export", &export, sizeof(export))
		    == 0)
			error = EXDEV;
		else
			error = EINVAL;
		vput(vp);
		return (error);
	}

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
	/*
	 * For the case of mountd(8) doing exports in a jail, the vfs_suser()
	 * call does not cause failure.  vfs_domount() has already checked
	 * that "root" is doing this and vfs_suser() will fail when
	 * the file system has been mounted outside the jail.
	 * jail_export set true indicates that "export" is not mixed
	 * with other options that change mount behaviour.
	 */
	vfs_suser_failed = false;
	error = vfs_suser(mp, td);
	if (jail_export && error != 0) {
		error = 0;
		vfs_suser_failed = true;
	}
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
	VOP_UNLOCK(vp);

	rootvp = NULL;
	vfs_op_enter(mp);
	vn_seqc_write_begin(vp);

	if (vfs_getopt(*optlist, "fsid", (void **)&fsid_up,
	    &fsid_up_len) == 0) {
		if (fsid_up_len != sizeof(*fsid_up)) {
			error = EINVAL;
			goto end;
		}
		if (fsidcmp(&fsid_up, &mp->mnt_stat.f_fsid) != 0) {
			error = ENOENT;
			goto end;
		}
		vfs_deleteopt(*optlist, "fsid");
	}

	MNT_ILOCK(mp);
	if ((mp->mnt_kern_flag & MNTK_UNMOUNT) != 0) {
		MNT_IUNLOCK(mp);
		error = EBUSY;
		goto end;
	}
	if (vfs_suser_failed) {
		KASSERT((fsflags & (MNT_EXPORTED | MNT_UPDATE)) ==
		    (MNT_EXPORTED | MNT_UPDATE),
		    ("%s: jailed export did not set expected fsflags",
		     __func__));
		/*
		 * For this case, only MNT_UPDATE and
		 * MNT_EXPORTED have been set in fsflags
		 * by the options.  Only set MNT_UPDATE,
		 * since that is the one that would be set
		 * when set in fsflags, below.
		 */
		mp->mnt_flag |= MNT_UPDATE;
	} else {
		mp->mnt_flag &= ~MNT_UPDATEMASK;
		mp->mnt_flag |= fsflags & (MNT_RELOAD | MNT_FORCE | MNT_UPDATE |
		    MNT_SNAPSHOT | MNT_ROOTFS | MNT_UPDATEMASK | MNT_RDONLY);
		if ((mp->mnt_flag & MNT_ASYNC) == 0)
			mp->mnt_kern_flag &= ~MNTK_ASYNC;
	}
	rootvp = vfs_cache_root_clear(mp);
	MNT_IUNLOCK(mp);
	mp->mnt_optnew = *optlist;
	vfs_mergeopts(mp->mnt_optnew, mp->mnt_opt);

	/*
	 * Mount the filesystem.
	 * XXX The final recipients of VFS_MOUNT just overwrite the ndp they
	 * get.  No freeing of cn_pnbuf.
	 */
	/*
	 * For the case of mountd(8) doing exports from within a vnet jail,
	 * "from" is typically not set correctly such that VFS_MOUNT() will
	 * return ENOENT. It is not obvious that VFS_MOUNT() ever needs to be
	 * called when mountd is doing exports, but this check only applies to
	 * the specific case where it is running inside a vnet jail, to
	 * avoid any POLA violation.
	 */
	error = 0;
	if (!jail_export)
		error = VFS_MOUNT(mp);

	export_error = 0;
	/* Process the export option. */
	if (error == 0 && vfs_getopt(mp->mnt_optnew, "export", &bufp,
	    &len) == 0) {
		/* Assume that there is only 1 ABI for each length. */
		switch (len) {
		case (sizeof(struct oexport_args)):
			bzero(&o2export, sizeof(o2export));
			/* FALLTHROUGH */
		case (sizeof(o2export)):
			bcopy(bufp, &o2export, len);
			export.ex_flags = (uint64_t)o2export.ex_flags;
			export.ex_root = o2export.ex_root;
			export.ex_uid = o2export.ex_anon.cr_uid;
			export.ex_groups = NULL;
			export.ex_ngroups = o2export.ex_anon.cr_ngroups;
			if (export.ex_ngroups > 0) {
				if (export.ex_ngroups <= XU_NGROUPS) {
					export.ex_groups = malloc(
					    export.ex_ngroups * sizeof(gid_t),
					    M_TEMP, M_WAITOK);
					for (i = 0; i < export.ex_ngroups; i++)
						export.ex_groups[i] =
						  o2export.ex_anon.cr_groups[i];
				} else
					export_error = EINVAL;
			} else if (export.ex_ngroups < 0)
				export_error = EINVAL;
			export.ex_addr = o2export.ex_addr;
			export.ex_addrlen = o2export.ex_addrlen;
			export.ex_mask = o2export.ex_mask;
			export.ex_masklen = o2export.ex_masklen;
			export.ex_indexfile = o2export.ex_indexfile;
			export.ex_numsecflavors = o2export.ex_numsecflavors;
			if (export.ex_numsecflavors < MAXSECFLAVORS) {
				for (i = 0; i < export.ex_numsecflavors; i++)
					export.ex_secflavors[i] =
					    o2export.ex_secflavors[i];
			} else
				export_error = EINVAL;
			if (export_error == 0)
				export_error = vfs_export(mp, &export, true);
			free(export.ex_groups, M_TEMP);
			break;
		case (sizeof(export)):
			bcopy(bufp, &export, len);
			grps = NULL;
			if (export.ex_ngroups > 0) {
				if (export.ex_ngroups <= NGROUPS_MAX) {
					grps = malloc(export.ex_ngroups *
					    sizeof(gid_t), M_TEMP, M_WAITOK);
					export_error = copyin(export.ex_groups,
					    grps, export.ex_ngroups *
					    sizeof(gid_t));
					if (export_error == 0)
						export.ex_groups = grps;
				} else
					export_error = EINVAL;
			} else if (export.ex_ngroups == 0)
				export.ex_groups = NULL;
			else
				export_error = EINVAL;
			if (export_error == 0)
				export_error = vfs_export(mp, &export, true);
			free(grps, M_TEMP);
			break;
		default:
			export_error = EINVAL;
			break;
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
	if ((mp->mnt_flag & MNT_ASYNC) != 0 &&
	    (mp->mnt_kern_flag & MNTK_NOASYNC) == 0)
		mp->mnt_kern_flag |= MNTK_ASYNC;
	else
		mp->mnt_kern_flag &= ~MNTK_ASYNC;
	MNT_IUNLOCK(mp);

	if (error != 0)
		goto end;

	mount_devctl_event("REMOUNT", mp, true);
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
	vfs_op_exit(mp);
	if (rootvp != NULL) {
		vn_seqc_write_end(rootvp);
		vrele(rootvp);
	}
	vn_seqc_write_end(vp);
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
	uint64_t fsflags,		/* Flags common to all filesystems. */
	bool jail_export,		/* Got export option in vnet prison. */
	struct vfsoptlist **optlist	/* Options local to the filesystem. */
	)
{
	struct vfsconf *vfsp;
	struct nameidata nd;
	struct vnode *vp;
	char *pathbuf;
	int error;

	/*
	 * Be ultra-paranoid about making sure the type and fspath
	 * variables will fit in our mp buffers, including the
	 * terminating NUL.
	 */
	if (strlen(fstype) >= MFSNAMELEN || strlen(fspath) >= MNAMELEN)
		return (ENAMETOOLONG);

	if (jail_export) {
		error = priv_check(td, PRIV_NFS_DAEMON);
		if (error)
			return (error);
	} else if (jailed(td->td_ucred) || usermount == 0) {
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
		if (fsflags & MNT_ROOTFS) {
			if ((vfsp = vfs_byname(fstype)) == NULL)
				return (ENODEV);
		} else {
			if ((vfsp = vfs_byname_kld(fstype, td, &error)) == NULL)
				return (error);
		}
	}

	/*
	 * Get vnode to be covered or mount point's vnode in case of MNT_UPDATE.
	 */
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | AUDITVNODE1 | WANTPARENT,
	    UIO_SYSSPACE, fspath);
	error = namei(&nd);
	if (error != 0)
		return (error);
	vp = nd.ni_vp;
	/*
	 * Don't allow stacking file mounts to work around problems with the way
	 * that namei sets nd.ni_dvp to vp_crossmp for these.
	 */
	if (vp->v_type == VREG)
		fsflags |= MNT_NOCOVER;
	if ((fsflags & MNT_UPDATE) == 0) {
		if ((vp->v_vflag & VV_ROOT) != 0 &&
		    (fsflags & MNT_NOCOVER) != 0) {
			vput(vp);
			error = EBUSY;
			goto out;
		}
		pathbuf = malloc(MNAMELEN, M_TEMP, M_WAITOK);
		strcpy(pathbuf, fspath);
		/*
		 * Note: we allow any vnode type here. If the path sanity check
		 * succeeds, the type will be validated in vfs_domount_first
		 * above.
		 */
		if (vp->v_type == VDIR)
			error = vn_path_to_global_path(td, vp, pathbuf,
			    MNAMELEN);
		else
			error = vn_path_to_global_path_hardlink(td, vp,
			    nd.ni_dvp, pathbuf, MNAMELEN,
			    nd.ni_cnd.cn_nameptr, nd.ni_cnd.cn_namelen);
		if (error == 0) {
			error = vfs_domount_first(td, vfsp, pathbuf, vp,
			    fsflags, optlist);
		}
		free(pathbuf, M_TEMP);
	} else
		error = vfs_domount_update(td, vp, fsflags, jail_export,
		    optlist);

out:
	NDFREE_PNBUF(&nd);
	vrele(nd.ni_dvp);

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
sys_unmount(struct thread *td, struct unmount_args *uap)
{

	return (kern_unmount(td, uap->path, uap->flags));
}

int
kern_unmount(struct thread *td, const char *path, int flags)
{
	struct nameidata nd;
	struct mount *mp;
	char *fsidbuf, *pathbuf;
	fsid_t fsid;
	int error;

	AUDIT_ARG_VALUE(flags);
	if (jailed(td->td_ucred) || usermount == 0) {
		error = priv_check(td, PRIV_VFS_UNMOUNT);
		if (error)
			return (error);
	}

	if (flags & MNT_BYFSID) {
		fsidbuf = malloc(MNAMELEN, M_TEMP, M_WAITOK);
		error = copyinstr(path, fsidbuf, MNAMELEN, NULL);
		if (error) {
			free(fsidbuf, M_TEMP);
			return (error);
		}

		AUDIT_ARG_TEXT(fsidbuf);
		/* Decode the filesystem ID. */
		if (sscanf(fsidbuf, "FSID:%d:%d", &fsid.val[0], &fsid.val[1]) != 2) {
			free(fsidbuf, M_TEMP);
			return (EINVAL);
		}

		mp = vfs_getvfs(&fsid);
		free(fsidbuf, M_TEMP);
		if (mp == NULL) {
			return (ENOENT);
		}
	} else {
		pathbuf = malloc(MNAMELEN, M_TEMP, M_WAITOK);
		error = copyinstr(path, pathbuf, MNAMELEN, NULL);
		if (error) {
			free(pathbuf, M_TEMP);
			return (error);
		}

		/*
		 * Try to find global path for path argument.
		 */
		NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | AUDITVNODE1,
		    UIO_SYSSPACE, pathbuf);
		if (namei(&nd) == 0) {
			NDFREE_PNBUF(&nd);
			error = vn_path_to_global_path(td, nd.ni_vp, pathbuf,
			    MNAMELEN);
			if (error == 0)
				vput(nd.ni_vp);
		}
		mtx_lock(&mountlist_mtx);
		TAILQ_FOREACH_REVERSE(mp, &mountlist, mntlist, mnt_list) {
			if (strcmp(mp->mnt_stat.f_mntonname, pathbuf) == 0) {
				vfs_ref(mp);
				break;
			}
		}
		mtx_unlock(&mountlist_mtx);
		free(pathbuf, M_TEMP);
		if (mp == NULL) {
			/*
			 * Previously we returned ENOENT for a nonexistent path and
			 * EINVAL for a non-mountpoint.  We cannot tell these apart
			 * now, so in the !MNT_BYFSID case return the more likely
			 * EINVAL for compatibility.
			 */
			return (EINVAL);
		}
	}

	/*
	 * Don't allow unmounting the root filesystem.
	 */
	if (mp->mnt_flag & MNT_ROOTFS) {
		vfs_rel(mp);
		return (EINVAL);
	}
	error = dounmount(mp, flags, td);
	return (error);
}

/*
 * Return error if any of the vnodes, ignoring the root vnode
 * and the syncer vnode, have non-zero usecount.
 *
 * This function is purely advisory - it can return false positives
 * and negatives.
 */
static int
vfs_check_usecounts(struct mount *mp)
{
	struct vnode *vp, *mvp;

	MNT_VNODE_FOREACH_ALL(vp, mp, mvp) {
		if ((vp->v_vflag & VV_ROOT) == 0 && vp->v_type != VNON &&
		    vp->v_usecount != 0) {
			VI_UNLOCK(vp);
			MNT_VNODE_FOREACH_ALL_ABORT(mp, mvp);
			return (EBUSY);
		}
		VI_UNLOCK(vp);
	}

	return (0);
}

static void
dounmount_cleanup(struct mount *mp, struct vnode *coveredvp, int mntkflags)
{

	mtx_assert(MNT_MTX(mp), MA_OWNED);
	mp->mnt_kern_flag &= ~mntkflags;
	if ((mp->mnt_kern_flag & MNTK_MWAIT) != 0) {
		mp->mnt_kern_flag &= ~MNTK_MWAIT;
		wakeup(mp);
	}
	vfs_op_exit_locked(mp);
	MNT_IUNLOCK(mp);
	if (coveredvp != NULL) {
		VOP_UNLOCK(coveredvp);
		vdrop(coveredvp);
	}
	vn_finished_write(mp);
	vfs_rel(mp);
}

/*
 * There are various reference counters associated with the mount point.
 * Normally it is permitted to modify them without taking the mnt ilock,
 * but this behavior can be temporarily disabled if stable value is needed
 * or callers are expected to block (e.g. to not allow new users during
 * forced unmount).
 */
void
vfs_op_enter(struct mount *mp)
{
	struct mount_pcpu *mpcpu;
	int cpu;

	MNT_ILOCK(mp);
	mp->mnt_vfs_ops++;
	if (mp->mnt_vfs_ops > 1) {
		MNT_IUNLOCK(mp);
		return;
	}
	vfs_op_barrier_wait(mp);
	CPU_FOREACH(cpu) {
		mpcpu = vfs_mount_pcpu_remote(mp, cpu);

		mp->mnt_ref += mpcpu->mntp_ref;
		mpcpu->mntp_ref = 0;

		mp->mnt_lockref += mpcpu->mntp_lockref;
		mpcpu->mntp_lockref = 0;

		mp->mnt_writeopcount += mpcpu->mntp_writeopcount;
		mpcpu->mntp_writeopcount = 0;
	}
	MPASSERT(mp->mnt_ref > 0 && mp->mnt_lockref >= 0 &&
	    mp->mnt_writeopcount >= 0, mp,
	    ("invalid count(s): ref %d lockref %d writeopcount %d",
	    mp->mnt_ref, mp->mnt_lockref, mp->mnt_writeopcount));
	MNT_IUNLOCK(mp);
	vfs_assert_mount_counters(mp);
}

void
vfs_op_exit_locked(struct mount *mp)
{

	mtx_assert(MNT_MTX(mp), MA_OWNED);

	MPASSERT(mp->mnt_vfs_ops > 0, mp,
	    ("invalid vfs_ops count %d", mp->mnt_vfs_ops));
	MPASSERT(mp->mnt_vfs_ops > 1 ||
	    (mp->mnt_kern_flag & (MNTK_UNMOUNT | MNTK_SUSPEND)) == 0, mp,
	    ("vfs_ops too low %d in unmount or suspend", mp->mnt_vfs_ops));
	mp->mnt_vfs_ops--;
}

void
vfs_op_exit(struct mount *mp)
{

	MNT_ILOCK(mp);
	vfs_op_exit_locked(mp);
	MNT_IUNLOCK(mp);
}

struct vfs_op_barrier_ipi {
	struct mount *mp;
	struct smp_rendezvous_cpus_retry_arg srcra;
};

static void
vfs_op_action_func(void *arg)
{
	struct vfs_op_barrier_ipi *vfsopipi;
	struct mount *mp;

	vfsopipi = __containerof(arg, struct vfs_op_barrier_ipi, srcra);
	mp = vfsopipi->mp;

	if (!vfs_op_thread_entered(mp))
		smp_rendezvous_cpus_done(arg);
}

static void
vfs_op_wait_func(void *arg, int cpu)
{
	struct vfs_op_barrier_ipi *vfsopipi;
	struct mount *mp;
	struct mount_pcpu *mpcpu;

	vfsopipi = __containerof(arg, struct vfs_op_barrier_ipi, srcra);
	mp = vfsopipi->mp;

	mpcpu = vfs_mount_pcpu_remote(mp, cpu);
	while (atomic_load_int(&mpcpu->mntp_thread_in_ops))
		cpu_spinwait();
}

void
vfs_op_barrier_wait(struct mount *mp)
{
	struct vfs_op_barrier_ipi vfsopipi;

	vfsopipi.mp = mp;

	smp_rendezvous_cpus_retry(all_cpus,
	    smp_no_rendezvous_barrier,
	    vfs_op_action_func,
	    smp_no_rendezvous_barrier,
	    vfs_op_wait_func,
	    &vfsopipi.srcra);
}

#ifdef DIAGNOSTIC
void
vfs_assert_mount_counters(struct mount *mp)
{
	struct mount_pcpu *mpcpu;
	int cpu;

	if (mp->mnt_vfs_ops == 0)
		return;

	CPU_FOREACH(cpu) {
		mpcpu = vfs_mount_pcpu_remote(mp, cpu);
		if (mpcpu->mntp_ref != 0 ||
		    mpcpu->mntp_lockref != 0 ||
		    mpcpu->mntp_writeopcount != 0)
			vfs_dump_mount_counters(mp);
	}
}

void
vfs_dump_mount_counters(struct mount *mp)
{
	struct mount_pcpu *mpcpu;
	int ref, lockref, writeopcount;
	int cpu;

	printf("%s: mp %p vfs_ops %d\n", __func__, mp, mp->mnt_vfs_ops);

	printf("        ref : ");
	ref = mp->mnt_ref;
	CPU_FOREACH(cpu) {
		mpcpu = vfs_mount_pcpu_remote(mp, cpu);
		printf("%d ", mpcpu->mntp_ref);
		ref += mpcpu->mntp_ref;
	}
	printf("\n");
	printf("    lockref : ");
	lockref = mp->mnt_lockref;
	CPU_FOREACH(cpu) {
		mpcpu = vfs_mount_pcpu_remote(mp, cpu);
		printf("%d ", mpcpu->mntp_lockref);
		lockref += mpcpu->mntp_lockref;
	}
	printf("\n");
	printf("writeopcount: ");
	writeopcount = mp->mnt_writeopcount;
	CPU_FOREACH(cpu) {
		mpcpu = vfs_mount_pcpu_remote(mp, cpu);
		printf("%d ", mpcpu->mntp_writeopcount);
		writeopcount += mpcpu->mntp_writeopcount;
	}
	printf("\n");

	printf("counter       struct total\n");
	printf("ref             %-5d  %-5d\n", mp->mnt_ref, ref);
	printf("lockref         %-5d  %-5d\n", mp->mnt_lockref, lockref);
	printf("writeopcount    %-5d  %-5d\n", mp->mnt_writeopcount, writeopcount);

	panic("invalid counts on struct mount");
}
#endif

int
vfs_mount_fetch_counter(struct mount *mp, enum mount_counter which)
{
	struct mount_pcpu *mpcpu;
	int cpu, sum;

	switch (which) {
	case MNT_COUNT_REF:
		sum = mp->mnt_ref;
		break;
	case MNT_COUNT_LOCKREF:
		sum = mp->mnt_lockref;
		break;
	case MNT_COUNT_WRITEOPCOUNT:
		sum = mp->mnt_writeopcount;
		break;
	}

	CPU_FOREACH(cpu) {
		mpcpu = vfs_mount_pcpu_remote(mp, cpu);
		switch (which) {
		case MNT_COUNT_REF:
			sum += mpcpu->mntp_ref;
			break;
		case MNT_COUNT_LOCKREF:
			sum += mpcpu->mntp_lockref;
			break;
		case MNT_COUNT_WRITEOPCOUNT:
			sum += mpcpu->mntp_writeopcount;
			break;
		}
	}
	return (sum);
}

static bool
deferred_unmount_enqueue(struct mount *mp, uint64_t flags, bool requeue,
    int timeout_ticks)
{
	bool enqueued;

	enqueued = false;
	mtx_lock(&deferred_unmount_lock);
	if ((mp->mnt_taskqueue_flags & MNT_DEFERRED) == 0 || requeue) {
		mp->mnt_taskqueue_flags = flags | MNT_DEFERRED;
		STAILQ_INSERT_TAIL(&deferred_unmount_list, mp,
		    mnt_taskqueue_link);
		enqueued = true;
	}
	mtx_unlock(&deferred_unmount_lock);

	if (enqueued) {
		taskqueue_enqueue_timeout(taskqueue_deferred_unmount,
		    &deferred_unmount_task, timeout_ticks);
	}

	return (enqueued);
}

/*
 * Taskqueue handler for processing async/recursive unmounts
 */
static void
vfs_deferred_unmount(void *argi __unused, int pending __unused)
{
	STAILQ_HEAD(, mount) local_unmounts;
	uint64_t flags;
	struct mount *mp, *tmp;
	int error;
	unsigned int retries;
	bool unmounted;

	STAILQ_INIT(&local_unmounts);
	mtx_lock(&deferred_unmount_lock);
	STAILQ_CONCAT(&local_unmounts, &deferred_unmount_list); 
	mtx_unlock(&deferred_unmount_lock);

	STAILQ_FOREACH_SAFE(mp, &local_unmounts, mnt_taskqueue_link, tmp) {
		flags = mp->mnt_taskqueue_flags;
		KASSERT((flags & MNT_DEFERRED) != 0,
		    ("taskqueue unmount without MNT_DEFERRED"));
		error = dounmount(mp, flags, curthread);
		if (error != 0) {
			MNT_ILOCK(mp);
			unmounted = ((mp->mnt_kern_flag & MNTK_REFEXPIRE) != 0);
			MNT_IUNLOCK(mp);

			/*
			 * The deferred unmount thread is the only thread that
			 * modifies the retry counts, so locking/atomics aren't
			 * needed here.
			 */
			retries = (mp->mnt_unmount_retries)++;
			deferred_unmount_total_retries++;
			if (!unmounted && retries < deferred_unmount_retry_limit) {
				deferred_unmount_enqueue(mp, flags, true,
				    -deferred_unmount_retry_delay_hz);
			} else {
				if (retries >= deferred_unmount_retry_limit) {
					printf("giving up on deferred unmount "
					    "of %s after %d retries, error %d\n",
					    mp->mnt_stat.f_mntonname, retries, error);
				}
				vfs_rel(mp);
			}
		}
	}
}

/*
 * Do the actual filesystem unmount.
 */
int
dounmount(struct mount *mp, uint64_t flags, struct thread *td)
{
	struct mount_upper_node *upper;
	struct vnode *coveredvp, *rootvp;
	int error;
	uint64_t async_flag;
	int mnt_gen_r;
	unsigned int retries;

	KASSERT((flags & MNT_DEFERRED) == 0 ||
	    (flags & (MNT_RECURSE | MNT_FORCE)) == (MNT_RECURSE | MNT_FORCE),
	    ("MNT_DEFERRED requires MNT_RECURSE | MNT_FORCE"));

	/*
	 * If the caller has explicitly requested the unmount to be handled by
	 * the taskqueue and we're not already in taskqueue context, queue
	 * up the unmount request and exit.  This is done prior to any
	 * credential checks; MNT_DEFERRED should be used only for kernel-
	 * initiated unmounts and will therefore be processed with the
	 * (kernel) credentials of the taskqueue thread.  Still, callers
	 * should be sure this is the behavior they want.
	 */
	if ((flags & MNT_DEFERRED) != 0 &&
	    taskqueue_member(taskqueue_deferred_unmount, curthread) == 0) {
		if (!deferred_unmount_enqueue(mp, flags, false, 0))
			vfs_rel(mp);
		return (EINPROGRESS);
	}

	/*
	 * Only privileged root, or (if MNT_USER is set) the user that did the
	 * original mount is permitted to unmount this filesystem.
	 * This check should be made prior to queueing up any recursive
	 * unmounts of upper filesystems.  Those unmounts will be executed
	 * with kernel thread credentials and are expected to succeed, so
	 * we must at least ensure the originating context has sufficient
	 * privilege to unmount the base filesystem before proceeding with
	 * the uppers.
	 */
	error = vfs_suser(mp, td);
	if (error != 0) {
		KASSERT((flags & MNT_DEFERRED) == 0,
		    ("taskqueue unmount with insufficient privilege"));
		vfs_rel(mp);
		return (error);
	}

	if (recursive_forced_unmount && ((flags & MNT_FORCE) != 0))
		flags |= MNT_RECURSE;

	if ((flags & MNT_RECURSE) != 0) {
		KASSERT((flags & MNT_FORCE) != 0,
		    ("MNT_RECURSE requires MNT_FORCE"));

		MNT_ILOCK(mp);
		/*
		 * Set MNTK_RECURSE to prevent new upper mounts from being
		 * added, and note that an operation on the uppers list is in
		 * progress.  This will ensure that unregistration from the
		 * uppers list, and therefore any pending unmount of the upper
		 * FS, can't complete until after we finish walking the list.
		 */
		mp->mnt_kern_flag |= MNTK_RECURSE;
		mp->mnt_upper_pending++;
		TAILQ_FOREACH(upper, &mp->mnt_uppers, mnt_upper_link) {
			retries = upper->mp->mnt_unmount_retries;
			if (retries > deferred_unmount_retry_limit) {
				error = EBUSY;
				continue;
			}
			MNT_IUNLOCK(mp);

			vfs_ref(upper->mp);
			if (!deferred_unmount_enqueue(upper->mp, flags,
			    false, 0))
				vfs_rel(upper->mp);
			MNT_ILOCK(mp);
		}
		mp->mnt_upper_pending--;
		if ((mp->mnt_kern_flag & MNTK_UPPER_WAITER) != 0 &&
		    mp->mnt_upper_pending == 0) {
			mp->mnt_kern_flag &= ~MNTK_UPPER_WAITER;
			wakeup(&mp->mnt_uppers);
		}

		/*
		 * If we're not on the taskqueue, wait until the uppers list
		 * is drained before proceeding with unmount.  Otherwise, if
		 * we are on the taskqueue and there are still pending uppers,
		 * just re-enqueue on the end of the taskqueue.
		 */
		if ((flags & MNT_DEFERRED) == 0) {
			while (error == 0 && !TAILQ_EMPTY(&mp->mnt_uppers)) {
				mp->mnt_kern_flag |= MNTK_TASKQUEUE_WAITER;
				error = msleep(&mp->mnt_taskqueue_link,
				    MNT_MTX(mp), PCATCH, "umntqw", 0);
			}
			if (error != 0) {
				MNT_REL(mp);
				MNT_IUNLOCK(mp);
				return (error);
			}
		} else if (!TAILQ_EMPTY(&mp->mnt_uppers)) {
			MNT_IUNLOCK(mp);
			if (error == 0)
				deferred_unmount_enqueue(mp, flags, true, 0);
			return (error);
		}
		MNT_IUNLOCK(mp);
		KASSERT(TAILQ_EMPTY(&mp->mnt_uppers), ("mnt_uppers not empty"));
	}

	/* Allow the taskqueue to safely re-enqueue on failure */
	if ((flags & MNT_DEFERRED) != 0)
		vfs_ref(mp);

	if ((coveredvp = mp->mnt_vnodecovered) != NULL) {
		mnt_gen_r = mp->mnt_gen;
		VI_LOCK(coveredvp);
		vholdl(coveredvp);
		vn_lock(coveredvp, LK_EXCLUSIVE | LK_INTERLOCK | LK_RETRY);
		/*
		 * Check for mp being unmounted while waiting for the
		 * covered vnode lock.
		 */
		if (coveredvp->v_mountedhere != mp ||
		    coveredvp->v_mountedhere->mnt_gen != mnt_gen_r) {
			VOP_UNLOCK(coveredvp);
			vdrop(coveredvp);
			vfs_rel(mp);
			return (EBUSY);
		}
	}

	vfs_op_enter(mp);

	vn_start_write(NULL, &mp, V_WAIT);
	MNT_ILOCK(mp);
	if ((mp->mnt_kern_flag & MNTK_UNMOUNT) != 0 ||
	    (mp->mnt_flag & MNT_UPDATE) != 0 ||
	    !TAILQ_EMPTY(&mp->mnt_uppers)) {
		dounmount_cleanup(mp, coveredvp, 0);
		return (EBUSY);
	}
	mp->mnt_kern_flag |= MNTK_UNMOUNT;
	rootvp = vfs_cache_root_clear(mp);
	if (coveredvp != NULL)
		vn_seqc_write_begin(coveredvp);
	if (flags & MNT_NONBUSY) {
		MNT_IUNLOCK(mp);
		error = vfs_check_usecounts(mp);
		MNT_ILOCK(mp);
		if (error != 0) {
			vn_seqc_write_end(coveredvp);
			dounmount_cleanup(mp, coveredvp, MNTK_UNMOUNT);
			if (rootvp != NULL) {
				vn_seqc_write_end(rootvp);
				vrele(rootvp);
			}
			return (error);
		}
	}
	/* Allow filesystems to detect that a forced unmount is in progress. */
	if (flags & MNT_FORCE) {
		mp->mnt_kern_flag |= MNTK_UNMOUNTF;
		MNT_IUNLOCK(mp);
		/*
		 * Must be done after setting MNTK_UNMOUNTF and before
		 * waiting for mnt_lockref to become 0.
		 */
		VFS_PURGE(mp);
		MNT_ILOCK(mp);
	}
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

	/*
	 * We want to keep the vnode around so that we can vn_seqc_write_end
	 * after we are done with unmount. Downgrade our reference to a mere
	 * hold count so that we don't interefere with anything.
	 */
	if (rootvp != NULL) {
		vhold(rootvp);
		vrele(rootvp);
	}

	if (mp->mnt_flag & MNT_EXPUBLIC)
		vfs_setpublicfs(NULL, NULL, NULL);

	vfs_periodic(mp, MNT_WAIT);
	MNT_ILOCK(mp);
	async_flag = mp->mnt_flag & MNT_ASYNC;
	mp->mnt_flag &= ~MNT_ASYNC;
	mp->mnt_kern_flag &= ~MNTK_ASYNC;
	MNT_IUNLOCK(mp);
	vfs_deallocate_syncvnode(mp);
	error = VFS_UNMOUNT(mp, flags);
	vn_finished_write(mp);
	vfs_rel(mp);
	/*
	 * If we failed to flush the dirty blocks for this mount point,
	 * undo all the cdir/rdir and rootvnode changes we made above.
	 * Unless we failed to do so because the device is reporting that
	 * it doesn't exist anymore.
	 */
	if (error && error != ENXIO) {
		MNT_ILOCK(mp);
		if ((mp->mnt_flag & MNT_RDONLY) == 0) {
			MNT_IUNLOCK(mp);
			vfs_allocate_syncvnode(mp);
			MNT_ILOCK(mp);
		}
		mp->mnt_kern_flag &= ~(MNTK_UNMOUNT | MNTK_UNMOUNTF);
		mp->mnt_flag |= async_flag;
		if ((mp->mnt_flag & MNT_ASYNC) != 0 &&
		    (mp->mnt_kern_flag & MNTK_NOASYNC) == 0)
			mp->mnt_kern_flag |= MNTK_ASYNC;
		if (mp->mnt_kern_flag & MNTK_MWAIT) {
			mp->mnt_kern_flag &= ~MNTK_MWAIT;
			wakeup(mp);
		}
		vfs_op_exit_locked(mp);
		MNT_IUNLOCK(mp);
		if (coveredvp) {
			vn_seqc_write_end(coveredvp);
			VOP_UNLOCK(coveredvp);
			vdrop(coveredvp);
		}
		if (rootvp != NULL) {
			vn_seqc_write_end(rootvp);
			vdrop(rootvp);
		}
		return (error);
	}

	mtx_lock(&mountlist_mtx);
	TAILQ_REMOVE(&mountlist, mp, mnt_list);
	mtx_unlock(&mountlist_mtx);
	EVENTHANDLER_DIRECT_INVOKE(vfs_unmounted, mp, td);
	if (coveredvp != NULL) {
		VI_LOCK(coveredvp);
		vn_irflag_unset_locked(coveredvp, VIRF_MOUNTPOINT);
		coveredvp->v_mountedhere = NULL;
		vn_seqc_write_end_locked(coveredvp);
		VI_UNLOCK(coveredvp);
		VOP_UNLOCK(coveredvp);
		vdrop(coveredvp);
	}
	mount_devctl_event("UNMOUNT", mp, false);
	if (rootvp != NULL) {
		vn_seqc_write_end(rootvp);
		vdrop(rootvp);
	}
	vfs_event_signal(NULL, VQ_UNMOUNT, 0);
	if (rootvnode != NULL && mp == rootvnode->v_mount) {
		vrele(rootvnode);
		rootvnode = NULL;
	}
	if (mp == rootdevmp)
		rootdevmp = NULL;
	if ((flags & MNT_DEFERRED) != 0)
		vfs_rel(mp);
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
vfs_getopt(struct vfsoptlist *opts, const char *name, void **buf, int *len)
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

int
vfs_getopt_size(struct vfsoptlist *opts, const char *name, off_t *value)
{
	char *opt_value, *vtp;
	quad_t iv;
	int error, opt_len;

	error = vfs_getopt(opts, name, (void **)&opt_value, &opt_len);
	if (error != 0)
		return (error);
	if (opt_len == 0 || opt_value == NULL)
		return (EINVAL);
	if (opt_value[0] == '\0' || opt_value[opt_len - 1] != '\0')
		return (EINVAL);
	iv = strtoq(opt_value, &vtp, 0);
	if (vtp == opt_value || (vtp[0] != '\0' && vtp[1] != '\0'))
		return (EINVAL);
	if (iv < 0)
		return (EINVAL);
	switch (vtp[0]) {
	case 't': case 'T':
		iv *= 1024;
		/* FALLTHROUGH */
	case 'g': case 'G':
		iv *= 1024;
		/* FALLTHROUGH */
	case 'm': case 'M':
		iv *= 1024;
		/* FALLTHROUGH */
	case 'k': case 'K':
		iv *= 1024;
	case '\0':
		break;
	default:
		return (EINVAL);
	}
	*value = iv;

	return (0);
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
vfs_copyopt(struct vfsoptlist *opts, const char *name, void *dest, int len)
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

int
__vfs_statfs(struct mount *mp, struct statfs *sbp)
{
	/*
	 * Filesystems only fill in part of the structure for updates, we
	 * have to read the entirety first to get all content.
	 */
	if (sbp != &mp->mnt_stat)
		memcpy(sbp, &mp->mnt_stat, sizeof(*sbp));

	/*
	 * Set these in case the underlying filesystem fails to do so.
	 */
	sbp->f_version = STATFS_VERSION;
	sbp->f_namemax = NAME_MAX;
	sbp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	sbp->f_nvnodelistsize = mp->mnt_nvnodelistsize;

	return (mp->mnt_op->vfs_statfs(mp, sbp));
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
kernel_mount(struct mntarg *ma, uint64_t flags)
{
	struct uio auio;
	int error;

	KASSERT(ma != NULL, ("kernel_mount NULL ma"));
	KASSERT(ma->error != 0 || ma->v != NULL, ("kernel_mount NULL ma->v"));
	KASSERT(!(ma->len & 1), ("kernel_mount odd ma->len (%d)", ma->len));

	error = ma->error;
	if (error == 0) {
		auio.uio_iov = ma->v;
		auio.uio_iovcnt = ma->len;
		auio.uio_segflg = UIO_SYSSPACE;
		error = vfs_donmount(curthread, flags, &auio);
	}
	free_mntarg(ma);
	return (error);
}

/* Map from mount options to printable formats. */
static struct mntoptnames optnames[] = {
	MNTOPT_NAMES
};

#define DEVCTL_LEN 1024
static void
mount_devctl_event(const char *type, struct mount *mp, bool donew)
{
	const uint8_t *cp;
	struct mntoptnames *fp;
	struct sbuf sb;
	struct statfs *sfp = &mp->mnt_stat;
	char *buf;

	buf = malloc(DEVCTL_LEN, M_MOUNT, M_NOWAIT);
	if (buf == NULL)
		return;
	sbuf_new(&sb, buf, DEVCTL_LEN, SBUF_FIXEDLEN);
	sbuf_cpy(&sb, "mount-point=\"");
	devctl_safe_quote_sb(&sb, sfp->f_mntonname);
	sbuf_cat(&sb, "\" mount-dev=\"");
	devctl_safe_quote_sb(&sb, sfp->f_mntfromname);
	sbuf_cat(&sb, "\" mount-type=\"");
	devctl_safe_quote_sb(&sb, sfp->f_fstypename);
	sbuf_cat(&sb, "\" fsid=0x");
	cp = (const uint8_t *)&sfp->f_fsid.val[0];
	for (int i = 0; i < sizeof(sfp->f_fsid); i++)
		sbuf_printf(&sb, "%02x", cp[i]);
	sbuf_printf(&sb, " owner=%u flags=\"", sfp->f_owner);
	for (fp = optnames; fp->o_opt != 0; fp++) {
		if ((mp->mnt_flag & fp->o_opt) != 0) {
			sbuf_cat(&sb, fp->o_name);
			sbuf_putc(&sb, ';');
		}
	}
	sbuf_putc(&sb, '"');
	sbuf_finish(&sb);

	/*
	 * Options are not published because the form of the options depends on
	 * the file system and may include binary data. In addition, they don't
	 * necessarily provide enough useful information to be actionable when
	 * devd processes them.
	 */

	if (sbuf_error(&sb) == 0)
		devctl_notify("VFS", "FS", type, sbuf_data(&sb));
	sbuf_delete(&sb);
	free(buf, M_MOUNT);
}

/*
 * Force remount specified mount point to read-only.  The argument
 * must be busied to avoid parallel unmount attempts.
 *
 * Intended use is to prevent further writes if some metadata
 * inconsistency is detected.  Note that the function still flushes
 * all cached metadata and data for the mount point, which might be
 * not always suitable.
 */
int
vfs_remount_ro(struct mount *mp)
{
	struct vfsoptlist *opts;
	struct vfsopt *opt;
	struct vnode *vp_covered, *rootvp;
	int error;

	vfs_op_enter(mp);
	KASSERT(mp->mnt_lockref > 0,
	    ("vfs_remount_ro: mp %p is not busied", mp));
	KASSERT((mp->mnt_kern_flag & MNTK_UNMOUNT) == 0,
	    ("vfs_remount_ro: mp %p is being unmounted (and busy?)", mp));

	rootvp = NULL;
	vp_covered = mp->mnt_vnodecovered;
	error = vget(vp_covered, LK_EXCLUSIVE | LK_NOWAIT);
	if (error != 0) {
		vfs_op_exit(mp);
		return (error);
	}
	VI_LOCK(vp_covered);
	if ((vp_covered->v_iflag & VI_MOUNT) != 0) {
		VI_UNLOCK(vp_covered);
		vput(vp_covered);
		vfs_op_exit(mp);
		return (EBUSY);
	}
	vp_covered->v_iflag |= VI_MOUNT;
	VI_UNLOCK(vp_covered);
	vn_seqc_write_begin(vp_covered);

	MNT_ILOCK(mp);
	if ((mp->mnt_flag & MNT_RDONLY) != 0) {
		MNT_IUNLOCK(mp);
		error = EBUSY;
		goto out;
	}
	mp->mnt_flag |= MNT_UPDATE | MNT_FORCE | MNT_RDONLY;
	rootvp = vfs_cache_root_clear(mp);
	MNT_IUNLOCK(mp);

	opts = malloc(sizeof(struct vfsoptlist), M_MOUNT, M_WAITOK | M_ZERO);
	TAILQ_INIT(opts);
	opt = malloc(sizeof(struct vfsopt), M_MOUNT, M_WAITOK | M_ZERO);
	opt->name = strdup("ro", M_MOUNT);
	opt->value = NULL;
	TAILQ_INSERT_TAIL(opts, opt, link);
	vfs_mergeopts(opts, mp->mnt_opt);
	mp->mnt_optnew = opts;

	error = VFS_MOUNT(mp);

	if (error == 0) {
		MNT_ILOCK(mp);
		mp->mnt_flag &= ~(MNT_UPDATE | MNT_FORCE);
		MNT_IUNLOCK(mp);
		vfs_deallocate_syncvnode(mp);
		if (mp->mnt_opt != NULL)
			vfs_freeopts(mp->mnt_opt);
		mp->mnt_opt = mp->mnt_optnew;
	} else {
		MNT_ILOCK(mp);
		mp->mnt_flag &= ~(MNT_UPDATE | MNT_FORCE | MNT_RDONLY);
		MNT_IUNLOCK(mp);
		vfs_freeopts(mp->mnt_optnew);
	}
	mp->mnt_optnew = NULL;

out:
	vfs_op_exit(mp);
	VI_LOCK(vp_covered);
	vp_covered->v_iflag &= ~VI_MOUNT;
	VI_UNLOCK(vp_covered);
	vput(vp_covered);
	vn_seqc_write_end(vp_covered);
	if (rootvp != NULL) {
		vn_seqc_write_end(rootvp);
		vrele(rootvp);
	}
	return (error);
}

/*
 * Suspend write operations on all local writeable filesystems.  Does
 * full sync of them in the process.
 *
 * Iterate over the mount points in reverse order, suspending most
 * recently mounted filesystems first.  It handles a case where a
 * filesystem mounted from a md(4) vnode-backed device should be
 * suspended before the filesystem that owns the vnode.
 */
void
suspend_all_fs(void)
{
	struct mount *mp;
	int error;

	mtx_lock(&mountlist_mtx);
	TAILQ_FOREACH_REVERSE(mp, &mountlist, mntlist, mnt_list) {
		error = vfs_busy(mp, MBF_MNTLSTLOCK | MBF_NOWAIT);
		if (error != 0)
			continue;
		if ((mp->mnt_flag & (MNT_RDONLY | MNT_LOCAL)) != MNT_LOCAL ||
		    (mp->mnt_kern_flag & MNTK_SUSPEND) != 0) {
			mtx_lock(&mountlist_mtx);
			vfs_unbusy(mp);
			continue;
		}
		error = vfs_write_suspend(mp, 0);
		if (error == 0) {
			MNT_ILOCK(mp);
			MPASS((mp->mnt_kern_flag & MNTK_SUSPEND_ALL) == 0);
			mp->mnt_kern_flag |= MNTK_SUSPEND_ALL;
			MNT_IUNLOCK(mp);
			mtx_lock(&mountlist_mtx);
		} else {
			printf("suspend of %s failed, error %d\n",
			    mp->mnt_stat.f_mntonname, error);
			mtx_lock(&mountlist_mtx);
			vfs_unbusy(mp);
		}
	}
	mtx_unlock(&mountlist_mtx);
}

/*
 * Clone the mnt_exjail field to a new mount point.
 */
void
vfs_exjail_clone(struct mount *inmp, struct mount *outmp)
{
	struct ucred *cr;
	struct prison *pr;

	MNT_ILOCK(inmp);
	cr = inmp->mnt_exjail;
	if (cr != NULL) {
		crhold(cr);
		MNT_IUNLOCK(inmp);
		pr = cr->cr_prison;
		sx_slock(&allprison_lock);
		if (!prison_isalive(pr)) {
			sx_sunlock(&allprison_lock);
			crfree(cr);
			return;
		}
		MNT_ILOCK(outmp);
		if (outmp->mnt_exjail == NULL) {
			outmp->mnt_exjail = cr;
			atomic_add_int(&pr->pr_exportcnt, 1);
			cr = NULL;
		}
		MNT_IUNLOCK(outmp);
		sx_sunlock(&allprison_lock);
		if (cr != NULL)
			crfree(cr);
	} else
		MNT_IUNLOCK(inmp);
}

void
resume_all_fs(void)
{
	struct mount *mp;

	mtx_lock(&mountlist_mtx);
	TAILQ_FOREACH(mp, &mountlist, mnt_list) {
		if ((mp->mnt_kern_flag & MNTK_SUSPEND_ALL) == 0)
			continue;
		mtx_unlock(&mountlist_mtx);
		MNT_ILOCK(mp);
		MPASS((mp->mnt_kern_flag & MNTK_SUSPEND) != 0);
		mp->mnt_kern_flag &= ~MNTK_SUSPEND_ALL;
		MNT_IUNLOCK(mp);
		vfs_write_resume(mp, 0);
		mtx_lock(&mountlist_mtx);
		vfs_unbusy(mp);
	}
	mtx_unlock(&mountlist_mtx);
}
