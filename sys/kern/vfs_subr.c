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
 *	@(#)vfs_subr.c	8.31 (Berkeley) 5/26/95
 */

/*
 * External virtual filesystem routines
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/event.h>
#include <sys/eventhandler.h>
#include <sys/extattr.h>
#include <sys/fcntl.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/reboot.h>
#include <sys/sleepqueue.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <machine/stdarg.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>
#include <vm/uma.h>

static MALLOC_DEFINE(M_NETADDR, "Export Host", "Export host address structure");

static void	delmntque(struct vnode *vp);
static void	insmntque(struct vnode *vp, struct mount *mp);
static void	vlruvp(struct vnode *vp);
static int	flushbuflist(struct bufv *bufv, int flags, struct bufobj *bo,
		    int slpflag, int slptimeo);
static void	syncer_shutdown(void *arg, int howto);
static int	vtryrecycle(struct vnode *vp);
static void	vx_lock(struct vnode *vp);
static void	vx_unlock(struct vnode *vp);
static void	vbusy(struct vnode *vp);
static void	vdropl(struct vnode *vp);
static void	vholdl(struct vnode *);

/*
 * Enable Giant pushdown based on whether or not the vm is mpsafe in this
 * build.  Without mpsafevm the buffer cache can not run Giant free.
 */
#if defined(__alpha__) || defined(__amd64__) || defined(__i386__)
int mpsafe_vfs = 1;
#else
int mpsafe_vfs;
#endif
TUNABLE_INT("debug.mpsafevfs", &mpsafe_vfs);
SYSCTL_INT(_debug, OID_AUTO, mpsafevfs, CTLFLAG_RD, &mpsafe_vfs, 0,
    "MPSAFE VFS");

/*
 * Number of vnodes in existence.  Increased whenever getnewvnode()
 * allocates a new vnode, never decreased.
 */
static unsigned long	numvnodes;

SYSCTL_LONG(_vfs, OID_AUTO, numvnodes, CTLFLAG_RD, &numvnodes, 0, "");

/*
 * Conversion tables for conversion from vnode types to inode formats
 * and back.
 */
enum vtype iftovt_tab[16] = {
	VNON, VFIFO, VCHR, VNON, VDIR, VNON, VBLK, VNON,
	VREG, VNON, VLNK, VNON, VSOCK, VNON, VNON, VBAD,
};
int vttoif_tab[9] = {
	0, S_IFREG, S_IFDIR, S_IFBLK, S_IFCHR, S_IFLNK,
	S_IFSOCK, S_IFIFO, S_IFMT,
};

/*
 * List of vnodes that are ready for recycling.
 */
static TAILQ_HEAD(freelst, vnode) vnode_free_list;

/*
 * Minimum number of free vnodes.  If there are fewer than this free vnodes,
 * getnewvnode() will return a newly allocated vnode.
 */
static u_long wantfreevnodes = 25;
SYSCTL_LONG(_vfs, OID_AUTO, wantfreevnodes, CTLFLAG_RW, &wantfreevnodes, 0, "");
/* Number of vnodes in the free list. */
static u_long freevnodes;
SYSCTL_LONG(_vfs, OID_AUTO, freevnodes, CTLFLAG_RD, &freevnodes, 0, "");

/*
 * Various variables used for debugging the new implementation of
 * reassignbuf().
 * XXX these are probably of (very) limited utility now.
 */
static int reassignbufcalls;
SYSCTL_INT(_vfs, OID_AUTO, reassignbufcalls, CTLFLAG_RW, &reassignbufcalls, 0, "");
static int nameileafonly;
SYSCTL_INT(_vfs, OID_AUTO, nameileafonly, CTLFLAG_RW, &nameileafonly, 0, "");

/*
 * Cache for the mount type id assigned to NFS.  This is used for
 * special checks in nfs/nfs_nqlease.c and vm/vnode_pager.c.
 */
int	nfs_mount_type = -1;

/* To keep more than one thread at a time from running vfs_getnewfsid */
static struct mtx mntid_mtx;

/*
 * Lock for any access to the following:
 *	vnode_free_list
 *	numvnodes
 *	freevnodes
 */
static struct mtx vnode_free_list_mtx;

/* Publicly exported FS */
struct nfs_public nfs_pub;

/* Zone for allocation of new vnodes - used exclusively by getnewvnode() */
static uma_zone_t vnode_zone;
static uma_zone_t vnodepoll_zone;

/* Set to 1 to print out reclaim of active vnodes */
int	prtactive;

/*
 * The workitem queue.
 *
 * It is useful to delay writes of file data and filesystem metadata
 * for tens of seconds so that quickly created and deleted files need
 * not waste disk bandwidth being created and removed. To realize this,
 * we append vnodes to a "workitem" queue. When running with a soft
 * updates implementation, most pending metadata dependencies should
 * not wait for more than a few seconds. Thus, mounted on block devices
 * are delayed only about a half the time that file data is delayed.
 * Similarly, directory updates are more critical, so are only delayed
 * about a third the time that file data is delayed. Thus, there are
 * SYNCER_MAXDELAY queues that are processed round-robin at a rate of
 * one each second (driven off the filesystem syncer process). The
 * syncer_delayno variable indicates the next queue that is to be processed.
 * Items that need to be processed soon are placed in this queue:
 *
 *	syncer_workitem_pending[syncer_delayno]
 *
 * A delay of fifteen seconds is done by placing the request fifteen
 * entries later in the queue:
 *
 *	syncer_workitem_pending[(syncer_delayno + 15) & syncer_mask]
 *
 */
static int syncer_delayno;
static long syncer_mask;
LIST_HEAD(synclist, bufobj);
static struct synclist *syncer_workitem_pending;
/*
 * The sync_mtx protects:
 *	bo->bo_synclist
 *	sync_vnode_count
 *	syncer_delayno
 *	syncer_state
 *	syncer_workitem_pending
 *	syncer_worklist_len
 *	rushjob
 */
static struct mtx sync_mtx;

#define SYNCER_MAXDELAY		32
static int syncer_maxdelay = SYNCER_MAXDELAY;	/* maximum delay time */
static int syncdelay = 30;		/* max time to delay syncing data */
static int filedelay = 30;		/* time to delay syncing files */
SYSCTL_INT(_kern, OID_AUTO, filedelay, CTLFLAG_RW, &filedelay, 0, "");
static int dirdelay = 29;		/* time to delay syncing directories */
SYSCTL_INT(_kern, OID_AUTO, dirdelay, CTLFLAG_RW, &dirdelay, 0, "");
static int metadelay = 28;		/* time to delay syncing metadata */
SYSCTL_INT(_kern, OID_AUTO, metadelay, CTLFLAG_RW, &metadelay, 0, "");
static int rushjob;		/* number of slots to run ASAP */
static int stat_rush_requests;	/* number of times I/O speeded up */
SYSCTL_INT(_debug, OID_AUTO, rush_requests, CTLFLAG_RW, &stat_rush_requests, 0, "");

/*
 * When shutting down the syncer, run it at four times normal speed.
 */
#define SYNCER_SHUTDOWN_SPEEDUP		4
static int sync_vnode_count;
static int syncer_worklist_len;
static enum { SYNCER_RUNNING, SYNCER_SHUTTING_DOWN, SYNCER_FINAL_DELAY }
    syncer_state;

/*
 * Number of vnodes we want to exist at any one time.  This is mostly used
 * to size hash tables in vnode-related code.  It is normally not used in
 * getnewvnode(), as wantfreevnodes is normally nonzero.)
 *
 * XXX desiredvnodes is historical cruft and should not exist.
 */
int desiredvnodes;
SYSCTL_INT(_kern, KERN_MAXVNODES, maxvnodes, CTLFLAG_RW,
    &desiredvnodes, 0, "Maximum number of vnodes");
static int minvnodes;
SYSCTL_INT(_kern, OID_AUTO, minvnodes, CTLFLAG_RW,
    &minvnodes, 0, "Minimum number of vnodes");
static int vnlru_nowhere;
SYSCTL_INT(_debug, OID_AUTO, vnlru_nowhere, CTLFLAG_RW,
    &vnlru_nowhere, 0, "Number of times the vnlru process ran without success");

/* Hook for calling soft updates. */
int (*softdep_process_worklist_hook)(struct mount *);

/*
 * Initialize the vnode management data structures.
 */
#ifndef	MAXVNODES_MAX
#define	MAXVNODES_MAX	100000
#endif
static void
vntblinit(void *dummy __unused)
{

	/*
	 * Desiredvnodes is a function of the physical memory size and
	 * the kernel's heap size.  Specifically, desiredvnodes scales
	 * in proportion to the physical memory size until two fifths
	 * of the kernel's heap size is consumed by vnodes and vm
	 * objects.
	 */
	desiredvnodes = min(maxproc + cnt.v_page_count / 4, 2 * vm_kmem_size /
	    (5 * (sizeof(struct vm_object) + sizeof(struct vnode))));
	if (desiredvnodes > MAXVNODES_MAX) {
		if (bootverbose)
			printf("Reducing kern.maxvnodes %d -> %d\n",
			    desiredvnodes, MAXVNODES_MAX);
		desiredvnodes = MAXVNODES_MAX;
	}
	minvnodes = desiredvnodes / 4;
	mtx_init(&mountlist_mtx, "mountlist", NULL, MTX_DEF);
	mtx_init(&mntid_mtx, "mntid", NULL, MTX_DEF);
	TAILQ_INIT(&vnode_free_list);
	mtx_init(&vnode_free_list_mtx, "vnode_free_list", NULL, MTX_DEF);
	vnode_zone = uma_zcreate("VNODE", sizeof (struct vnode), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	vnodepoll_zone = uma_zcreate("VNODEPOLL", sizeof (struct vpollinfo),
	      NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	/*
	 * Initialize the filesystem syncer.
	 */
	syncer_workitem_pending = hashinit(syncer_maxdelay, M_VNODE,
		&syncer_mask);
	syncer_maxdelay = syncer_mask + 1;
	mtx_init(&sync_mtx, "Syncer mtx", NULL, MTX_DEF);
}
SYSINIT(vfs, SI_SUB_VFS, SI_ORDER_FIRST, vntblinit, NULL)


/*
 * Mark a mount point as busy. Used to synchronize access and to delay
 * unmounting. Interlock is not released on failure.
 */
int
vfs_busy(mp, flags, interlkp, td)
	struct mount *mp;
	int flags;
	struct mtx *interlkp;
	struct thread *td;
{
	int lkflags;

	MNT_ILOCK(mp);
	if (mp->mnt_kern_flag & MNTK_UNMOUNT) {
		if (flags & LK_NOWAIT) {
			MNT_IUNLOCK(mp);
			return (ENOENT);
		}
		if (interlkp)
			mtx_unlock(interlkp);
		mp->mnt_kern_flag |= MNTK_MWAIT;
		/*
		 * Since all busy locks are shared except the exclusive
		 * lock granted when unmounting, the only place that a
		 * wakeup needs to be done is at the release of the
		 * exclusive lock at the end of dounmount.
		 */
		msleep(mp, MNT_MTX(mp), PVFS|PDROP, "vfs_busy", 0);
		if (interlkp)
			mtx_lock(interlkp);
		return (ENOENT);
	}
	if (interlkp)
		mtx_unlock(interlkp);
	lkflags = LK_SHARED | LK_NOPAUSE | LK_INTERLOCK;
	if (lockmgr(&mp->mnt_lock, lkflags, MNT_MTX(mp), td))
		panic("vfs_busy: unexpected lock failure");
	return (0);
}

/*
 * Free a busy filesystem.
 */
void
vfs_unbusy(mp, td)
	struct mount *mp;
	struct thread *td;
{

	lockmgr(&mp->mnt_lock, LK_RELEASE, NULL, td);
}

/*
 * Lookup a mount point by filesystem identifier.
 */
struct mount *
vfs_getvfs(fsid)
	fsid_t *fsid;
{
	struct mount *mp;

	mtx_lock(&mountlist_mtx);
	TAILQ_FOREACH(mp, &mountlist, mnt_list) {
		if (mp->mnt_stat.f_fsid.val[0] == fsid->val[0] &&
		    mp->mnt_stat.f_fsid.val[1] == fsid->val[1]) {
			mtx_unlock(&mountlist_mtx);
			return (mp);
		}
	}
	mtx_unlock(&mountlist_mtx);
	return ((struct mount *) 0);
}

/*
 * Check if a user can access priveledged mount options.
 */
int
vfs_suser(struct mount *mp, struct thread *td)
{
	int error;

	if ((mp->mnt_flag & MNT_USER) == 0 ||
	    mp->mnt_cred->cr_uid != td->td_ucred->cr_uid) {
		if ((error = suser(td)) != 0)
			return (error);
	}
	return (0);
}

/*
 * Get a new unique fsid.  Try to make its val[0] unique, since this value
 * will be used to create fake device numbers for stat().  Also try (but
 * not so hard) make its val[0] unique mod 2^16, since some emulators only
 * support 16-bit device numbers.  We end up with unique val[0]'s for the
 * first 2^16 calls and unique val[0]'s mod 2^16 for the first 2^8 calls.
 *
 * Keep in mind that several mounts may be running in parallel.  Starting
 * the search one past where the previous search terminated is both a
 * micro-optimization and a defense against returning the same fsid to
 * different mounts.
 */
void
vfs_getnewfsid(mp)
	struct mount *mp;
{
	static u_int16_t mntid_base;
	fsid_t tfsid;
	int mtype;

	mtx_lock(&mntid_mtx);
	mtype = mp->mnt_vfc->vfc_typenum;
	tfsid.val[1] = mtype;
	mtype = (mtype & 0xFF) << 24;
	for (;;) {
		tfsid.val[0] = makedev(255,
		    mtype | ((mntid_base & 0xFF00) << 8) | (mntid_base & 0xFF));
		mntid_base++;
		if (vfs_getvfs(&tfsid) == NULL)
			break;
	}
	mp->mnt_stat.f_fsid.val[0] = tfsid.val[0];
	mp->mnt_stat.f_fsid.val[1] = tfsid.val[1];
	mtx_unlock(&mntid_mtx);
}

/*
 * Knob to control the precision of file timestamps:
 *
 *   0 = seconds only; nanoseconds zeroed.
 *   1 = seconds and nanoseconds, accurate within 1/HZ.
 *   2 = seconds and nanoseconds, truncated to microseconds.
 * >=3 = seconds and nanoseconds, maximum precision.
 */
enum { TSP_SEC, TSP_HZ, TSP_USEC, TSP_NSEC };

static int timestamp_precision = TSP_SEC;
SYSCTL_INT(_vfs, OID_AUTO, timestamp_precision, CTLFLAG_RW,
    &timestamp_precision, 0, "");

/*
 * Get a current timestamp.
 */
void
vfs_timestamp(tsp)
	struct timespec *tsp;
{
	struct timeval tv;

	switch (timestamp_precision) {
	case TSP_SEC:
		tsp->tv_sec = time_second;
		tsp->tv_nsec = 0;
		break;
	case TSP_HZ:
		getnanotime(tsp);
		break;
	case TSP_USEC:
		microtime(&tv);
		TIMEVAL_TO_TIMESPEC(&tv, tsp);
		break;
	case TSP_NSEC:
	default:
		nanotime(tsp);
		break;
	}
}

/*
 * Set vnode attributes to VNOVAL
 */
void
vattr_null(vap)
	struct vattr *vap;
{

	vap->va_type = VNON;
	vap->va_size = VNOVAL;
	vap->va_bytes = VNOVAL;
	vap->va_mode = VNOVAL;
	vap->va_nlink = VNOVAL;
	vap->va_uid = VNOVAL;
	vap->va_gid = VNOVAL;
	vap->va_fsid = VNOVAL;
	vap->va_fileid = VNOVAL;
	vap->va_blocksize = VNOVAL;
	vap->va_rdev = VNOVAL;
	vap->va_atime.tv_sec = VNOVAL;
	vap->va_atime.tv_nsec = VNOVAL;
	vap->va_mtime.tv_sec = VNOVAL;
	vap->va_mtime.tv_nsec = VNOVAL;
	vap->va_ctime.tv_sec = VNOVAL;
	vap->va_ctime.tv_nsec = VNOVAL;
	vap->va_birthtime.tv_sec = VNOVAL;
	vap->va_birthtime.tv_nsec = VNOVAL;
	vap->va_flags = VNOVAL;
	vap->va_gen = VNOVAL;
	vap->va_vaflags = 0;
}

/*
 * This routine is called when we have too many vnodes.  It attempts
 * to free <count> vnodes and will potentially free vnodes that still
 * have VM backing store (VM backing store is typically the cause
 * of a vnode blowout so we want to do this).  Therefore, this operation
 * is not considered cheap.
 *
 * A number of conditions may prevent a vnode from being reclaimed.
 * the buffer cache may have references on the vnode, a directory
 * vnode may still have references due to the namei cache representing
 * underlying files, or the vnode may be in active use.   It is not
 * desireable to reuse such vnodes.  These conditions may cause the
 * number of vnodes to reach some minimum value regardless of what
 * you set kern.maxvnodes to.  Do not set kern.maxvnodes too low.
 */
static int
vlrureclaim(struct mount *mp)
{
	struct vnode *vp;
	int done;
	int trigger;
	int usevnodes;
	int count;

	/*
	 * Calculate the trigger point, don't allow user
	 * screwups to blow us up.   This prevents us from
	 * recycling vnodes with lots of resident pages.  We
	 * aren't trying to free memory, we are trying to
	 * free vnodes.
	 */
	usevnodes = desiredvnodes;
	if (usevnodes <= 0)
		usevnodes = 1;
	trigger = cnt.v_page_count * 2 / usevnodes;

	done = 0;
	MNT_ILOCK(mp);
	count = mp->mnt_nvnodelistsize / 10 + 1;
	while (count && (vp = TAILQ_FIRST(&mp->mnt_nvnodelist)) != NULL) {
		TAILQ_REMOVE(&mp->mnt_nvnodelist, vp, v_nmntvnodes);
		TAILQ_INSERT_TAIL(&mp->mnt_nvnodelist, vp, v_nmntvnodes);

		if (vp->v_type != VNON &&
		    vp->v_type != VBAD &&
		    VI_TRYLOCK(vp)) {
			if (VMIGHTFREE(vp) &&           /* critical path opt */
			    (vp->v_object == NULL ||
			    vp->v_object->resident_page_count < trigger)) {
				MNT_IUNLOCK(mp);
				vgonel(vp, curthread);
				done++;
				MNT_ILOCK(mp);
			} else
				VI_UNLOCK(vp);
		}
		--count;
	}
	MNT_IUNLOCK(mp);
	return done;
}

/*
 * Attempt to recycle vnodes in a context that is always safe to block.
 * Calling vlrurecycle() from the bowels of filesystem code has some
 * interesting deadlock problems.
 */
static struct proc *vnlruproc;
static int vnlruproc_sig;

static void
vnlru_proc(void)
{
	struct mount *mp, *nmp;
	int done;
	struct proc *p = vnlruproc;
	struct thread *td = FIRST_THREAD_IN_PROC(p);

	mtx_lock(&Giant);

	EVENTHANDLER_REGISTER(shutdown_pre_sync, kproc_shutdown, p,
	    SHUTDOWN_PRI_FIRST);

	for (;;) {
		kthread_suspend_check(p);
		mtx_lock(&vnode_free_list_mtx);
		if (numvnodes - freevnodes <= desiredvnodes * 9 / 10) {
			vnlruproc_sig = 0;
			wakeup(&vnlruproc_sig);
			msleep(vnlruproc, &vnode_free_list_mtx,
			    PVFS|PDROP, "vlruwt", hz);
			continue;
		}
		mtx_unlock(&vnode_free_list_mtx);
		done = 0;
		mtx_lock(&mountlist_mtx);
		for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nmp) {
			if (vfs_busy(mp, LK_NOWAIT, &mountlist_mtx, td)) {
				nmp = TAILQ_NEXT(mp, mnt_list);
				continue;
			}
			done += vlrureclaim(mp);
			mtx_lock(&mountlist_mtx);
			nmp = TAILQ_NEXT(mp, mnt_list);
			vfs_unbusy(mp, td);
		}
		mtx_unlock(&mountlist_mtx);
		if (done == 0) {
#if 0
			/* These messages are temporary debugging aids */
			if (vnlru_nowhere < 5)
				printf("vnlru process getting nowhere..\n");
			else if (vnlru_nowhere == 5)
				printf("vnlru process messages stopped.\n");
#endif
			vnlru_nowhere++;
			tsleep(vnlruproc, PPAUSE, "vlrup", hz * 3);
		}
	}
}

static struct kproc_desc vnlru_kp = {
	"vnlru",
	vnlru_proc,
	&vnlruproc
};
SYSINIT(vnlru, SI_SUB_KTHREAD_UPDATE, SI_ORDER_FIRST, kproc_start, &vnlru_kp)


/*
 * Routines having to do with the management of the vnode table.
 */

/*
 * Check to see if a free vnode can be recycled. If it can,
 * recycle it and return it with the vnode interlock held.
 */
static int
vtryrecycle(struct vnode *vp)
{
	struct thread *td = curthread;
	vm_object_t object;
	struct mount *vnmp;
	int error;

	/* Don't recycle if we can't get the interlock */
	if (!VI_TRYLOCK(vp))
		return (EWOULDBLOCK);
	if (!VCANRECYCLE(vp)) {
		VI_UNLOCK(vp);
		return (EBUSY);
	}
	/*
	 * This vnode may found and locked via some other list, if so we
	 * can't recycle it yet.
	 */
	if (vn_lock(vp, LK_INTERLOCK | LK_EXCLUSIVE | LK_NOWAIT, td) != 0)
		return (EWOULDBLOCK);
	/*
	 * Don't recycle if its filesystem is being suspended.
	 */
	if (vn_start_write(vp, &vnmp, V_NOWAIT) != 0) {
		VOP_UNLOCK(vp, 0, td);
		return (EBUSY);
	}

	/*
	 * Don't recycle if we still have cached pages.
	 */
	object = vp->v_object;
	if (object != NULL) {
		VM_OBJECT_LOCK(object);
		if (object->resident_page_count ||
		    object->ref_count) {
			VM_OBJECT_UNLOCK(object);
			error = EBUSY;
			goto done;
		}
		VM_OBJECT_UNLOCK(object);
	}
	if (LIST_FIRST(&vp->v_cache_src)) {
		/*
		 * note: nameileafonly sysctl is temporary,
		 * for debugging only, and will eventually be
		 * removed.
		 */
		if (nameileafonly > 0) {
			/*
			 * Do not reuse namei-cached directory
			 * vnodes that have cached
			 * subdirectories.
			 */
			if (cache_leaf_test(vp) < 0) {
				error = EISDIR;
				goto done;
			}
		} else if (nameileafonly < 0 ||
			    vmiodirenable == 0) {
			/*
			 * Do not reuse namei-cached directory
			 * vnodes if nameileafonly is -1 or
			 * if VMIO backing for directories is
			 * turned off (otherwise we reuse them
			 * too quickly).
			 */
			error = EBUSY;
			goto done;
		}
	}
	/*
	 * If we got this far, we need to acquire the interlock and see if
	 * anyone picked up this vnode from another list.  If not, we will
	 * mark it with XLOCK via vgonel() so that anyone who does find it
	 * will skip over it.
	 */
	VI_LOCK(vp);
	if (!VCANRECYCLE(vp)) {
		VI_UNLOCK(vp);
		error = EBUSY;
		goto done;
	}
	mtx_lock(&vnode_free_list_mtx);
	TAILQ_REMOVE(&vnode_free_list, vp, v_freelist);
	vp->v_iflag &= ~VI_FREE;
	mtx_unlock(&vnode_free_list_mtx);
	vp->v_iflag |= VI_DOOMED;
	if ((vp->v_type != VBAD) || (vp->v_data != NULL)) {
		VOP_UNLOCK(vp, 0, td);
		vgonel(vp, td);
	} else
		VOP_UNLOCK(vp, LK_INTERLOCK, td);
	vn_finished_write(vnmp);
	return (0);
done:
	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(vnmp);
	return (error);
}

/*
 * Return the next vnode from the free list.
 */
int
getnewvnode(tag, mp, vops, vpp)
	const char *tag;
	struct mount *mp;
	struct vop_vector *vops;
	struct vnode **vpp;
{
	struct vnode *vp = NULL;
	struct vpollinfo *pollinfo = NULL;
	struct bufobj *bo;

	mtx_lock(&vnode_free_list_mtx);

	/*
	 * Try to reuse vnodes if we hit the max.  This situation only
	 * occurs in certain large-memory (2G+) situations.  We cannot
	 * attempt to directly reclaim vnodes due to nasty recursion
	 * problems.
	 */
	while (numvnodes - freevnodes > desiredvnodes) {
		if (vnlruproc_sig == 0) {
			vnlruproc_sig = 1;      /* avoid unnecessary wakeups */
			wakeup(vnlruproc);
		}
		msleep(&vnlruproc_sig, &vnode_free_list_mtx, PVFS,
		    "vlruwk", hz);
	}

	/*
	 * Attempt to reuse a vnode already on the free list, allocating
	 * a new vnode if we can't find one or if we have not reached a
	 * good minimum for good LRU performance.
	 */

	if (freevnodes >= wantfreevnodes && numvnodes >= minvnodes) {
		int error;
		int count;

		for (count = 0; count < freevnodes; count++) {
			vp = TAILQ_FIRST(&vnode_free_list);
			TAILQ_REMOVE(&vnode_free_list, vp, v_freelist);
			TAILQ_INSERT_TAIL(&vnode_free_list, vp, v_freelist);
			mtx_unlock(&vnode_free_list_mtx);
			error = vtryrecycle(vp);
			mtx_lock(&vnode_free_list_mtx);
			if (error == 0)
				break;
			vp = NULL;
		}
	}
	if (vp) {
		freevnodes--;
		bo = &vp->v_bufobj;
		mtx_unlock(&vnode_free_list_mtx);

#ifdef INVARIANTS
		{
			if (vp->v_data)
				printf("cleaned vnode isn't, "
				       "address %p, inode %p\n",
				       vp, vp->v_data);
			if (bo->bo_numoutput)
				panic("%p: Clean vnode has pending I/O's", vp);
			if (vp->v_usecount != 0)
				panic("%p: Non-zero use count", vp);
			if (vp->v_writecount != 0)
				panic("%p: Non-zero write count", vp);
		}
#endif
		if ((pollinfo = vp->v_pollinfo) != NULL) {
			/*
			 * To avoid lock order reversals, the call to
			 * uma_zfree() must be delayed until the vnode
			 * interlock is released.
			 */
			vp->v_pollinfo = NULL;
		}
#ifdef MAC
		mac_destroy_vnode(vp);
#endif
		vp->v_iflag = 0;
		vp->v_vflag = 0;
		vp->v_lastw = 0;
		vp->v_lasta = 0;
		vp->v_cstart = 0;
		vp->v_clen = 0;
		bzero(&vp->v_un, sizeof vp->v_un);
		lockdestroy(vp->v_vnlock);
		lockinit(vp->v_vnlock, PVFS, tag, VLKTIMEOUT, LK_NOPAUSE);
		VNASSERT(bo->bo_clean.bv_cnt == 0, vp,
		    ("cleanbufcnt not 0"));
		VNASSERT(bo->bo_clean.bv_root == NULL, vp,
		    ("cleanblkroot not NULL"));
		VNASSERT(bo->bo_dirty.bv_cnt == 0, vp,
		    ("dirtybufcnt not 0"));
		VNASSERT(bo->bo_dirty.bv_root == NULL, vp,
		    ("dirtyblkroot not NULL"));
	} else {
		numvnodes++;
		mtx_unlock(&vnode_free_list_mtx);

		vp = (struct vnode *) uma_zalloc(vnode_zone, M_WAITOK|M_ZERO);
		mtx_init(&vp->v_interlock, "vnode interlock", NULL, MTX_DEF);
		vp->v_dd = vp;
		bo = &vp->v_bufobj;
		bo->__bo_vnode = vp;
		bo->bo_mtx = &vp->v_interlock;
		vp->v_vnlock = &vp->v_lock;
		lockinit(vp->v_vnlock, PVFS, tag, VLKTIMEOUT, LK_NOPAUSE);
		cache_purge(vp);		/* Sets up v_id. */
		LIST_INIT(&vp->v_cache_src);
		TAILQ_INIT(&vp->v_cache_dst);
	}

	TAILQ_INIT(&bo->bo_clean.bv_hd);
	TAILQ_INIT(&bo->bo_dirty.bv_hd);
	bo->bo_ops = &buf_ops_bio;
	bo->bo_private = vp;
	vp->v_type = VNON;
	vp->v_tag = tag;
	vp->v_op = vops;
	*vpp = vp;
	vp->v_usecount = 1;
	vp->v_data = 0;
	if (pollinfo != NULL) {
		knlist_destroy(&pollinfo->vpi_selinfo.si_note);
		mtx_destroy(&pollinfo->vpi_lock);
		uma_zfree(vnodepoll_zone, pollinfo);
	}
#ifdef MAC
	mac_init_vnode(vp);
	if (mp != NULL && (mp->mnt_flag & MNT_MULTILABEL) == 0)
		mac_associate_vnode_singlelabel(mp, vp);
	else if (mp == NULL)
		printf("NULL mp in getnewvnode()\n");
#endif
	delmntque(vp);
	if (mp != NULL) {
		insmntque(vp, mp);
		bo->bo_bsize = mp->mnt_stat.f_iosize;
	}

	return (0);
}

/*
 * Delete from old mount point vnode list, if on one.
 */
static void
delmntque(struct vnode *vp)
{
	struct mount *mp;

	if (vp->v_mount == NULL)
		return;
	mp = vp->v_mount;
	MNT_ILOCK(mp);
	vp->v_mount = NULL;
	VNASSERT(mp->mnt_nvnodelistsize > 0, vp,
		("bad mount point vnode list size"));
	TAILQ_REMOVE(&mp->mnt_nvnodelist, vp, v_nmntvnodes);
	mp->mnt_nvnodelistsize--;
	MNT_IUNLOCK(mp);
}

/*
 * Insert into list of vnodes for the new mount point, if available.
 */
static void
insmntque(struct vnode *vp, struct mount *mp)
{

	vp->v_mount = mp;
	VNASSERT(mp != NULL, vp, ("Don't call insmntque(foo, NULL)"));
	MNT_ILOCK(vp->v_mount);
	TAILQ_INSERT_TAIL(&mp->mnt_nvnodelist, vp, v_nmntvnodes);
	mp->mnt_nvnodelistsize++;
	MNT_IUNLOCK(vp->v_mount);
}

/*
 * Flush out and invalidate all buffers associated with a bufobj
 * Called with the underlying object locked.
 */
int
bufobj_invalbuf(struct bufobj *bo, int flags, struct thread *td, int slpflag, int slptimeo)
{
	int error;

	BO_LOCK(bo);
	if (flags & V_SAVE) {
		error = bufobj_wwait(bo, slpflag, slptimeo);
		if (error) {
			BO_UNLOCK(bo);
			return (error);
		}
		if (bo->bo_dirty.bv_cnt > 0) {
			BO_UNLOCK(bo);
			if ((error = BO_SYNC(bo, MNT_WAIT, td)) != 0)
				return (error);
			/*
			 * XXX We could save a lock/unlock if this was only
			 * enabled under INVARIANTS
			 */
			BO_LOCK(bo);
			if (bo->bo_numoutput > 0 || bo->bo_dirty.bv_cnt > 0)
				panic("vinvalbuf: dirty bufs");
		}
	}
	/*
	 * If you alter this loop please notice that interlock is dropped and
	 * reacquired in flushbuflist.  Special care is needed to ensure that
	 * no race conditions occur from this.
	 */
	do {
		error = flushbuflist(&bo->bo_clean,
		    flags, bo, slpflag, slptimeo);
		if (error == 0)
			error = flushbuflist(&bo->bo_dirty,
			    flags, bo, slpflag, slptimeo);
		if (error != 0 && error != EAGAIN) {
			BO_UNLOCK(bo);
			return (error);
		}
	} while (error != 0);

	/*
	 * Wait for I/O to complete.  XXX needs cleaning up.  The vnode can
	 * have write I/O in-progress but if there is a VM object then the
	 * VM object can also have read-I/O in-progress.
	 */
	do {
		bufobj_wwait(bo, 0, 0);
		BO_UNLOCK(bo);
		if (bo->bo_object != NULL) {
			VM_OBJECT_LOCK(bo->bo_object);
			vm_object_pip_wait(bo->bo_object, "bovlbx");
			VM_OBJECT_UNLOCK(bo->bo_object);
		}
		BO_LOCK(bo);
	} while (bo->bo_numoutput > 0);
	BO_UNLOCK(bo);

	/*
	 * Destroy the copy in the VM cache, too.
	 */
	if (bo->bo_object != NULL) {
		VM_OBJECT_LOCK(bo->bo_object);
		vm_object_page_remove(bo->bo_object, 0, 0,
			(flags & V_SAVE) ? TRUE : FALSE);
		VM_OBJECT_UNLOCK(bo->bo_object);
	}

#ifdef INVARIANTS
	BO_LOCK(bo);
	if ((flags & (V_ALT | V_NORMAL)) == 0 &&
	    (bo->bo_dirty.bv_cnt > 0 || bo->bo_clean.bv_cnt > 0))
		panic("vinvalbuf: flush failed");
	BO_UNLOCK(bo);
#endif
	return (0);
}

/*
 * Flush out and invalidate all buffers associated with a vnode.
 * Called with the underlying object locked.
 */
int
vinvalbuf(struct vnode *vp, int flags, struct thread *td, int slpflag, int slptimeo)
{

	ASSERT_VOP_LOCKED(vp, "vinvalbuf");
	return (bufobj_invalbuf(&vp->v_bufobj, flags, td, slpflag, slptimeo));
}

/*
 * Flush out buffers on the specified list.
 *
 */
static int
flushbuflist(bufv, flags, bo, slpflag, slptimeo)
	struct bufv *bufv;
	int flags;
	struct bufobj *bo;
	int slpflag, slptimeo;
{
	struct buf *bp, *nbp;
	int retval, error;

	ASSERT_BO_LOCKED(bo);

	retval = 0;
	TAILQ_FOREACH_SAFE(bp, &bufv->bv_hd, b_bobufs, nbp) {
		if (((flags & V_NORMAL) && (bp->b_xflags & BX_ALTDATA)) ||
		    ((flags & V_ALT) && (bp->b_xflags & BX_ALTDATA) == 0)) {
			continue;
		}
		retval = EAGAIN;
		error = BUF_TIMELOCK(bp,
		    LK_EXCLUSIVE | LK_SLEEPFAIL | LK_INTERLOCK, BO_MTX(bo),
		    "flushbuf", slpflag, slptimeo);
		if (error) {
			BO_LOCK(bo);
			return (error != ENOLCK ? error : EAGAIN);
		}
		if (bp->b_bufobj != bo) {	/* XXX: necessary ? */
			BO_LOCK(bo);
			return (EAGAIN);
		}
		/*
		 * XXX Since there are no node locks for NFS, I
		 * believe there is a slight chance that a delayed
		 * write will occur while sleeping just above, so
		 * check for it.
		 */
		if (((bp->b_flags & (B_DELWRI | B_INVAL)) == B_DELWRI) &&
		    (flags & V_SAVE)) {
			bremfree(bp);
			bp->b_flags |= B_ASYNC;
			bwrite(bp);
			BO_LOCK(bo);
			return (EAGAIN);	/* XXX: why not loop ? */
		}
		bremfree(bp);
		bp->b_flags |= (B_INVAL | B_NOCACHE | B_RELBUF);
		bp->b_flags &= ~B_ASYNC;
		brelse(bp);
		BO_LOCK(bo);
	}
	return (retval);
}

/*
 * Truncate a file's buffer and pages to a specified length.  This
 * is in lieu of the old vinvalbuf mechanism, which performed unneeded
 * sync activity.
 */
int
vtruncbuf(struct vnode *vp, struct ucred *cred, struct thread *td, off_t length, int blksize)
{
	struct buf *bp, *nbp;
	int anyfreed;
	int trunclbn;
	struct bufobj *bo;

	/*
	 * Round up to the *next* lbn.
	 */
	trunclbn = (length + blksize - 1) / blksize;

	ASSERT_VOP_LOCKED(vp, "vtruncbuf");
restart:
	VI_LOCK(vp);
	bo = &vp->v_bufobj;
	anyfreed = 1;
	for (;anyfreed;) {
		anyfreed = 0;
		TAILQ_FOREACH_SAFE(bp, &bo->bo_clean.bv_hd, b_bobufs, nbp) {
			if (bp->b_lblkno < trunclbn)
				continue;
			if (BUF_LOCK(bp,
			    LK_EXCLUSIVE | LK_SLEEPFAIL | LK_INTERLOCK,
			    VI_MTX(vp)) == ENOLCK)
				goto restart;

			bremfree(bp);
			bp->b_flags |= (B_INVAL | B_RELBUF);
			bp->b_flags &= ~B_ASYNC;
			brelse(bp);
			anyfreed = 1;

			if (nbp != NULL &&
			    (((nbp->b_xflags & BX_VNCLEAN) == 0) ||
			    (nbp->b_vp != vp) ||
			    (nbp->b_flags & B_DELWRI))) {
				goto restart;
			}
			VI_LOCK(vp);
		}

		TAILQ_FOREACH_SAFE(bp, &bo->bo_dirty.bv_hd, b_bobufs, nbp) {
			if (bp->b_lblkno < trunclbn)
				continue;
			if (BUF_LOCK(bp,
			    LK_EXCLUSIVE | LK_SLEEPFAIL | LK_INTERLOCK,
			    VI_MTX(vp)) == ENOLCK)
				goto restart;
			bremfree(bp);
			bp->b_flags |= (B_INVAL | B_RELBUF);
			bp->b_flags &= ~B_ASYNC;
			brelse(bp);
			anyfreed = 1;
			if (nbp != NULL &&
			    (((nbp->b_xflags & BX_VNDIRTY) == 0) ||
			    (nbp->b_vp != vp) ||
			    (nbp->b_flags & B_DELWRI) == 0)) {
				goto restart;
			}
			VI_LOCK(vp);
		}
	}

	if (length > 0) {
restartsync:
		TAILQ_FOREACH_SAFE(bp, &bo->bo_dirty.bv_hd, b_bobufs, nbp) {
			if (bp->b_lblkno > 0)
				continue;
			/*
			 * Since we hold the vnode lock this should only
			 * fail if we're racing with the buf daemon.
			 */
			if (BUF_LOCK(bp,
			    LK_EXCLUSIVE | LK_SLEEPFAIL | LK_INTERLOCK,
			    VI_MTX(vp)) == ENOLCK) {
				goto restart;
			}
			VNASSERT((bp->b_flags & B_DELWRI), vp,
			    ("buf(%p) on dirty queue without DELWRI", bp));

			bremfree(bp);
			bawrite(bp);
			VI_LOCK(vp);
			goto restartsync;
		}
	}

	bufobj_wwait(bo, 0, 0);
	VI_UNLOCK(vp);
	vnode_pager_setsize(vp, length);

	return (0);
}

/*
 * buf_splay() - splay tree core for the clean/dirty list of buffers in
 * 		 a vnode.
 *
 *	NOTE: We have to deal with the special case of a background bitmap
 *	buffer, a situation where two buffers will have the same logical
 *	block offset.  We want (1) only the foreground buffer to be accessed
 *	in a lookup and (2) must differentiate between the foreground and
 *	background buffer in the splay tree algorithm because the splay
 *	tree cannot normally handle multiple entities with the same 'index'.
 *	We accomplish this by adding differentiating flags to the splay tree's
 *	numerical domain.
 */
static
struct buf *
buf_splay(daddr_t lblkno, b_xflags_t xflags, struct buf *root)
{
	struct buf dummy;
	struct buf *lefttreemax, *righttreemin, *y;

	if (root == NULL)
		return (NULL);
	lefttreemax = righttreemin = &dummy;
	for (;;) {
		if (lblkno < root->b_lblkno ||
		    (lblkno == root->b_lblkno &&
		    (xflags & BX_BKGRDMARKER) < (root->b_xflags & BX_BKGRDMARKER))) {
			if ((y = root->b_left) == NULL)
				break;
			if (lblkno < y->b_lblkno) {
				/* Rotate right. */
				root->b_left = y->b_right;
				y->b_right = root;
				root = y;
				if ((y = root->b_left) == NULL)
					break;
			}
			/* Link into the new root's right tree. */
			righttreemin->b_left = root;
			righttreemin = root;
		} else if (lblkno > root->b_lblkno ||
		    (lblkno == root->b_lblkno &&
		    (xflags & BX_BKGRDMARKER) > (root->b_xflags & BX_BKGRDMARKER))) {
			if ((y = root->b_right) == NULL)
				break;
			if (lblkno > y->b_lblkno) {
				/* Rotate left. */
				root->b_right = y->b_left;
				y->b_left = root;
				root = y;
				if ((y = root->b_right) == NULL)
					break;
			}
			/* Link into the new root's left tree. */
			lefttreemax->b_right = root;
			lefttreemax = root;
		} else {
			break;
		}
		root = y;
	}
	/* Assemble the new root. */
	lefttreemax->b_right = root->b_left;
	righttreemin->b_left = root->b_right;
	root->b_left = dummy.b_right;
	root->b_right = dummy.b_left;
	return (root);
}

static void
buf_vlist_remove(struct buf *bp)
{
	struct buf *root;
	struct bufv *bv;

	KASSERT(bp->b_bufobj != NULL, ("No b_bufobj %p", bp));
	ASSERT_BO_LOCKED(bp->b_bufobj);
	if (bp->b_xflags & BX_VNDIRTY) 
		bv = &bp->b_bufobj->bo_dirty;
	else
		bv = &bp->b_bufobj->bo_clean;
	if (bp != bv->bv_root) {
		root = buf_splay(bp->b_lblkno, bp->b_xflags, bv->bv_root);
		KASSERT(root == bp, ("splay lookup failed in remove"));
	}
	if (bp->b_left == NULL) {
		root = bp->b_right;
	} else {
		root = buf_splay(bp->b_lblkno, bp->b_xflags, bp->b_left);
		root->b_right = bp->b_right;
	}
	bv->bv_root = root;
	TAILQ_REMOVE(&bv->bv_hd, bp, b_bobufs);
	bv->bv_cnt--;
	bp->b_xflags &= ~(BX_VNDIRTY | BX_VNCLEAN);
}

/*
 * Add the buffer to the sorted clean or dirty block list using a
 * splay tree algorithm.
 *
 * NOTE: xflags is passed as a constant, optimizing this inline function!
 */
static void
buf_vlist_add(struct buf *bp, struct bufobj *bo, b_xflags_t xflags)
{
	struct buf *root;
	struct bufv *bv;

	ASSERT_BO_LOCKED(bo);
	bp->b_xflags |= xflags;
	if (xflags & BX_VNDIRTY)
		bv = &bo->bo_dirty;
	else
		bv = &bo->bo_clean;

	root = buf_splay(bp->b_lblkno, bp->b_xflags, bv->bv_root);
	if (root == NULL) {
		bp->b_left = NULL;
		bp->b_right = NULL;
		TAILQ_INSERT_TAIL(&bv->bv_hd, bp, b_bobufs);
	} else if (bp->b_lblkno < root->b_lblkno ||
	    (bp->b_lblkno == root->b_lblkno &&
	    (bp->b_xflags & BX_BKGRDMARKER) < (root->b_xflags & BX_BKGRDMARKER))) {
		bp->b_left = root->b_left;
		bp->b_right = root;
		root->b_left = NULL;
		TAILQ_INSERT_BEFORE(root, bp, b_bobufs);
	} else {
		bp->b_right = root->b_right;
		bp->b_left = root;
		root->b_right = NULL;
		TAILQ_INSERT_AFTER(&bv->bv_hd, root, bp, b_bobufs);
	}
	bv->bv_cnt++;
	bv->bv_root = bp;
}

/*
 * Lookup a buffer using the splay tree.  Note that we specifically avoid
 * shadow buffers used in background bitmap writes.
 *
 * This code isn't quite efficient as it could be because we are maintaining
 * two sorted lists and do not know which list the block resides in.
 *
 * During a "make buildworld" the desired buffer is found at one of
 * the roots more than 60% of the time.  Thus, checking both roots
 * before performing either splay eliminates unnecessary splays on the
 * first tree splayed.
 */
struct buf *
gbincore(struct bufobj *bo, daddr_t lblkno)
{
	struct buf *bp;

	ASSERT_BO_LOCKED(bo);
	if ((bp = bo->bo_clean.bv_root) != NULL &&
	    bp->b_lblkno == lblkno && !(bp->b_xflags & BX_BKGRDMARKER))
		return (bp);
	if ((bp = bo->bo_dirty.bv_root) != NULL &&
	    bp->b_lblkno == lblkno && !(bp->b_xflags & BX_BKGRDMARKER))
		return (bp);
	if ((bp = bo->bo_clean.bv_root) != NULL) {
		bo->bo_clean.bv_root = bp = buf_splay(lblkno, 0, bp);
		if (bp->b_lblkno == lblkno && !(bp->b_xflags & BX_BKGRDMARKER))
			return (bp);
	}
	if ((bp = bo->bo_dirty.bv_root) != NULL) {
		bo->bo_dirty.bv_root = bp = buf_splay(lblkno, 0, bp);
		if (bp->b_lblkno == lblkno && !(bp->b_xflags & BX_BKGRDMARKER))
			return (bp);
	}
	return (NULL);
}

/*
 * Associate a buffer with a vnode.
 */
void
bgetvp(struct vnode *vp, struct buf *bp)
{

	VNASSERT(bp->b_vp == NULL, bp->b_vp, ("bgetvp: not free"));

	CTR3(KTR_BUF, "bgetvp(%p) vp %p flags %X", bp, vp, bp->b_flags);
	VNASSERT((bp->b_xflags & (BX_VNDIRTY|BX_VNCLEAN)) == 0, vp,
	    ("bgetvp: bp already attached! %p", bp));

	ASSERT_VI_LOCKED(vp, "bgetvp");
	vholdl(vp);
	bp->b_vp = vp;
	bp->b_bufobj = &vp->v_bufobj;
	/*
	 * Insert onto list for new vnode.
	 */
	buf_vlist_add(bp, &vp->v_bufobj, BX_VNCLEAN);
}

/*
 * Disassociate a buffer from a vnode.
 */
void
brelvp(struct buf *bp)
{
	struct bufobj *bo;
	struct vnode *vp;

	CTR3(KTR_BUF, "brelvp(%p) vp %p flags %X", bp, bp->b_vp, bp->b_flags);
	KASSERT(bp->b_vp != NULL, ("brelvp: NULL"));

	/*
	 * Delete from old vnode list, if on one.
	 */
	vp = bp->b_vp;		/* XXX */
	bo = bp->b_bufobj;
	BO_LOCK(bo);
	if (bp->b_xflags & (BX_VNDIRTY | BX_VNCLEAN))
		buf_vlist_remove(bp);
	if ((bo->bo_flag & BO_ONWORKLST) && bo->bo_dirty.bv_cnt == 0) {
		bo->bo_flag &= ~BO_ONWORKLST;
		mtx_lock(&sync_mtx);
		LIST_REMOVE(bo, bo_synclist);
 		syncer_worklist_len--;
		mtx_unlock(&sync_mtx);
	}
	vdropl(vp);
	bp->b_vp = NULL;
	bp->b_bufobj = NULL;
	BO_UNLOCK(bo);
}

/*
 * Add an item to the syncer work queue.
 */
static void
vn_syncer_add_to_worklist(struct bufobj *bo, int delay)
{
	int slot;

	ASSERT_BO_LOCKED(bo);

	mtx_lock(&sync_mtx);
	if (bo->bo_flag & BO_ONWORKLST)
		LIST_REMOVE(bo, bo_synclist);
	else {
		bo->bo_flag |= BO_ONWORKLST;
 		syncer_worklist_len++;
	}

	if (delay > syncer_maxdelay - 2)
		delay = syncer_maxdelay - 2;
	slot = (syncer_delayno + delay) & syncer_mask;

	LIST_INSERT_HEAD(&syncer_workitem_pending[slot], bo, bo_synclist);
	mtx_unlock(&sync_mtx);
}

static int
sysctl_vfs_worklist_len(SYSCTL_HANDLER_ARGS)
{
	int error, len;

	mtx_lock(&sync_mtx);
	len = syncer_worklist_len - sync_vnode_count;
	mtx_unlock(&sync_mtx);
	error = SYSCTL_OUT(req, &len, sizeof(len));
	return (error);
}

SYSCTL_PROC(_vfs, OID_AUTO, worklist_len, CTLTYPE_INT | CTLFLAG_RD, NULL, 0,
    sysctl_vfs_worklist_len, "I", "Syncer thread worklist length");

struct  proc *updateproc;
static void sched_sync(void);
static struct kproc_desc up_kp = {
	"syncer",
	sched_sync,
	&updateproc
};
SYSINIT(syncer, SI_SUB_KTHREAD_UPDATE, SI_ORDER_FIRST, kproc_start, &up_kp)

static int
sync_vnode(struct bufobj *bo, struct thread *td)
{
	struct vnode *vp;
	struct mount *mp;

	vp = bo->__bo_vnode; 	/* XXX */
	if (VOP_ISLOCKED(vp, NULL) != 0)
		return (1);
	if (VI_TRYLOCK(vp) == 0)
		return (1);
	/*
	 * We use vhold in case the vnode does not
	 * successfully sync.  vhold prevents the vnode from
	 * going away when we unlock the sync_mtx so that
	 * we can acquire the vnode interlock.
	 */
	vholdl(vp);
	mtx_unlock(&sync_mtx);
	VI_UNLOCK(vp);
	if (vn_start_write(vp, &mp, V_NOWAIT) != 0) {
		vdrop(vp);
		mtx_lock(&sync_mtx);
		return (1);
	}
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	(void) VOP_FSYNC(vp, MNT_LAZY, td);
	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(mp);
	VI_LOCK(vp);
	if ((bo->bo_flag & BO_ONWORKLST) != 0) {
		/*
		 * Put us back on the worklist.  The worklist
		 * routine will remove us from our current
		 * position and then add us back in at a later
		 * position.
		 */
		vn_syncer_add_to_worklist(bo, syncdelay);
	}
	vdropl(vp);
	VI_UNLOCK(vp);
	mtx_lock(&sync_mtx);
	return (0);
}

/*
 * System filesystem synchronizer daemon.
 */
static void
sched_sync(void)
{
	struct synclist *next;
	struct synclist *slp;
	struct bufobj *bo;
	long starttime;
	struct thread *td = FIRST_THREAD_IN_PROC(updateproc);
	static int dummychan;
	int last_work_seen;
	int net_worklist_len;
	int syncer_final_iter;
	int first_printf;
	int error;

	mtx_lock(&Giant);
	last_work_seen = 0;
	syncer_final_iter = 0;
	first_printf = 1;
	syncer_state = SYNCER_RUNNING;
	starttime = time_second;

	EVENTHANDLER_REGISTER(shutdown_pre_sync, syncer_shutdown, td->td_proc,
	    SHUTDOWN_PRI_LAST);

	for (;;) {
		mtx_lock(&sync_mtx);
		if (syncer_state == SYNCER_FINAL_DELAY &&
		    syncer_final_iter == 0) {
			mtx_unlock(&sync_mtx);
			kthread_suspend_check(td->td_proc);
			mtx_lock(&sync_mtx);
		}
		net_worklist_len = syncer_worklist_len - sync_vnode_count;
		if (syncer_state != SYNCER_RUNNING &&
		    starttime != time_second) {
			if (first_printf) {
				printf("\nSyncing disks, vnodes remaining...");
				first_printf = 0;
			}
			printf("%d ", net_worklist_len);
		}
		starttime = time_second;

		/*
		 * Push files whose dirty time has expired.  Be careful
		 * of interrupt race on slp queue.
		 *
		 * Skip over empty worklist slots when shutting down.
		 */
		do {
			slp = &syncer_workitem_pending[syncer_delayno];
			syncer_delayno += 1;
			if (syncer_delayno == syncer_maxdelay)
				syncer_delayno = 0;
			next = &syncer_workitem_pending[syncer_delayno];
			/*
			 * If the worklist has wrapped since the
			 * it was emptied of all but syncer vnodes, 
			 * switch to the FINAL_DELAY state and run
			 * for one more second.
			 */
			if (syncer_state == SYNCER_SHUTTING_DOWN &&
			    net_worklist_len == 0 &&
			    last_work_seen == syncer_delayno) {
				syncer_state = SYNCER_FINAL_DELAY;
				syncer_final_iter = SYNCER_SHUTDOWN_SPEEDUP;
			}
		} while (syncer_state != SYNCER_RUNNING && LIST_EMPTY(slp) &&
		    syncer_worklist_len > 0);

		/*
		 * Keep track of the last time there was anything
		 * on the worklist other than syncer vnodes.
		 * Return to the SHUTTING_DOWN state if any
		 * new work appears.
		 */
		if (net_worklist_len > 0 || syncer_state == SYNCER_RUNNING)
			last_work_seen = syncer_delayno;
		if (net_worklist_len > 0 && syncer_state == SYNCER_FINAL_DELAY)
			syncer_state = SYNCER_SHUTTING_DOWN;
		while ((bo = LIST_FIRST(slp)) != NULL) {
			error = sync_vnode(bo, td);
			if (error == 1) {
				LIST_REMOVE(bo, bo_synclist);
				LIST_INSERT_HEAD(next, bo, bo_synclist);
				continue;
			}
		}
		if (syncer_state == SYNCER_FINAL_DELAY && syncer_final_iter > 0)
			syncer_final_iter--;
		mtx_unlock(&sync_mtx);

		/*
		 * Do soft update processing.
		 */
		if (softdep_process_worklist_hook != NULL)
			(*softdep_process_worklist_hook)(NULL);

		/*
		 * The variable rushjob allows the kernel to speed up the
		 * processing of the filesystem syncer process. A rushjob
		 * value of N tells the filesystem syncer to process the next
		 * N seconds worth of work on its queue ASAP. Currently rushjob
		 * is used by the soft update code to speed up the filesystem
		 * syncer process when the incore state is getting so far
		 * ahead of the disk that the kernel memory pool is being
		 * threatened with exhaustion.
		 */
		mtx_lock(&sync_mtx);
		if (rushjob > 0) {
			rushjob -= 1;
			mtx_unlock(&sync_mtx);
			continue;
		}
		mtx_unlock(&sync_mtx);
		/*
		 * Just sleep for a short period if time between
		 * iterations when shutting down to allow some I/O
		 * to happen.
		 *
		 * If it has taken us less than a second to process the
		 * current work, then wait. Otherwise start right over
		 * again. We can still lose time if any single round
		 * takes more than two seconds, but it does not really
		 * matter as we are just trying to generally pace the
		 * filesystem activity.
		 */
		if (syncer_state != SYNCER_RUNNING)
			tsleep(&dummychan, PPAUSE, "syncfnl",
			    hz / SYNCER_SHUTDOWN_SPEEDUP);
		else if (time_second == starttime)
			tsleep(&lbolt, PPAUSE, "syncer", 0);
	}
}

/*
 * Request the syncer daemon to speed up its work.
 * We never push it to speed up more than half of its
 * normal turn time, otherwise it could take over the cpu.
 */
int
speedup_syncer()
{
	struct thread *td;
	int ret = 0;

	td = FIRST_THREAD_IN_PROC(updateproc);
	sleepq_remove(td, &lbolt);
	mtx_lock(&sync_mtx);
	if (rushjob < syncdelay / 2) {
		rushjob += 1;
		stat_rush_requests += 1;
		ret = 1;
	}
	mtx_unlock(&sync_mtx);
	return (ret);
}

/*
 * Tell the syncer to speed up its work and run though its work
 * list several times, then tell it to shut down.
 */
static void
syncer_shutdown(void *arg, int howto)
{
	struct thread *td;

	if (howto & RB_NOSYNC)
		return;
	td = FIRST_THREAD_IN_PROC(updateproc);
	sleepq_remove(td, &lbolt);
	mtx_lock(&sync_mtx);
	syncer_state = SYNCER_SHUTTING_DOWN;
	rushjob = 0;
	mtx_unlock(&sync_mtx);
	kproc_shutdown(arg, howto);
}

/*
 * Reassign a buffer from one vnode to another.
 * Used to assign file specific control information
 * (indirect blocks) to the vnode to which they belong.
 */
void
reassignbuf(struct buf *bp)
{
	struct vnode *vp;
	struct bufobj *bo;
	int delay;

	vp = bp->b_vp;
	bo = bp->b_bufobj;
	++reassignbufcalls;

	CTR3(KTR_BUF, "reassignbuf(%p) vp %p flags %X",
	    bp, bp->b_vp, bp->b_flags);
	/*
	 * B_PAGING flagged buffers cannot be reassigned because their vp
	 * is not fully linked in.
	 */
	if (bp->b_flags & B_PAGING)
		panic("cannot reassign paging buffer");

	/*
	 * Delete from old vnode list, if on one.
	 */
	VI_LOCK(vp);
	if (bp->b_xflags & (BX_VNDIRTY | BX_VNCLEAN))
		buf_vlist_remove(bp);
	/*
	 * If dirty, put on list of dirty buffers; otherwise insert onto list
	 * of clean buffers.
	 */
	if (bp->b_flags & B_DELWRI) {
		if ((bo->bo_flag & BO_ONWORKLST) == 0) {
			switch (vp->v_type) {
			case VDIR:
				delay = dirdelay;
				break;
			case VCHR:
				delay = metadelay;
				break;
			default:
				delay = filedelay;
			}
			vn_syncer_add_to_worklist(bo, delay);
		}
		buf_vlist_add(bp, bo, BX_VNDIRTY);
	} else {
		buf_vlist_add(bp, bo, BX_VNCLEAN);

		if ((bo->bo_flag & BO_ONWORKLST) && bo->bo_dirty.bv_cnt == 0) {
			mtx_lock(&sync_mtx);
			LIST_REMOVE(bo, bo_synclist);
 			syncer_worklist_len--;
			mtx_unlock(&sync_mtx);
			bo->bo_flag &= ~BO_ONWORKLST;
		}
	}
	VI_UNLOCK(vp);
}

static void
v_incr_usecount(struct vnode *vp, int delta)
{

	vp->v_usecount += delta;
	if (vp->v_type == VCHR && vp->v_rdev != NULL) {
		dev_lock();
		vp->v_rdev->si_usecount += delta;
		dev_unlock();
	}
}

/*
 * Grab a particular vnode from the free list, increment its
 * reference count and lock it. The vnode lock bit is set if the
 * vnode is being eliminated in vgone. The process is awakened
 * when the transition is completed, and an error returned to
 * indicate that the vnode is no longer usable (possibly having
 * been changed to a new filesystem type).
 */
int
vget(vp, flags, td)
	struct vnode *vp;
	int flags;
	struct thread *td;
{
	int error;

	/*
	 * If the vnode is in the process of being cleaned out for
	 * another use, we wait for the cleaning to finish and then
	 * return failure. Cleaning is determined by checking that
	 * the VI_XLOCK flag is set.
	 */
	if ((flags & LK_INTERLOCK) == 0)
		VI_LOCK(vp);
	if (vp->v_iflag & VI_XLOCK && vp->v_vxthread != curthread) {
		if ((flags & LK_NOWAIT) == 0) {
			vx_waitl(vp);
			VI_UNLOCK(vp);
			return (ENOENT);
		}
		VI_UNLOCK(vp);
		return (EBUSY);
	}

	v_incr_usecount(vp, 1);

	if (VSHOULDBUSY(vp))
		vbusy(vp);
	if (flags & LK_TYPE_MASK) {
		if ((error = vn_lock(vp, flags | LK_INTERLOCK, td)) != 0) {
			/*
			 * must expand vrele here because we do not want
			 * to call VOP_INACTIVE if the reference count
			 * drops back to zero since it was never really
			 * active. We must remove it from the free list
			 * before sleeping so that multiple processes do
			 * not try to recycle it.
			 */
			VI_LOCK(vp);
			v_incr_usecount(vp, -1);
			if (VSHOULDFREE(vp))
				vfree(vp);
			else
				vlruvp(vp);
			VI_UNLOCK(vp);
		}
		return (error);
	}
	VI_UNLOCK(vp);
	return (0);
}

/*
 * Increase the reference count of a vnode.
 */
void
vref(struct vnode *vp)
{

	VI_LOCK(vp);
	v_incr_usecount(vp, 1);
	VI_UNLOCK(vp);
}

/*
 * Return reference count of a vnode.
 *
 * The results of this call are only guaranteed when some mechanism other
 * than the VI lock is used to stop other processes from gaining references
 * to the vnode.  This may be the case if the caller holds the only reference.
 * This is also useful when stale data is acceptable as race conditions may
 * be accounted for by some other means.
 */
int
vrefcnt(struct vnode *vp)
{
	int usecnt;

	VI_LOCK(vp);
	usecnt = vp->v_usecount;
	VI_UNLOCK(vp);

	return (usecnt);
}


/*
 * Vnode put/release.
 * If count drops to zero, call inactive routine and return to freelist.
 */
void
vrele(vp)
	struct vnode *vp;
{
	struct thread *td = curthread;	/* XXX */

	KASSERT(vp != NULL, ("vrele: null vp"));

	VI_LOCK(vp);

	/* Skip this v_writecount check if we're going to panic below. */
	VNASSERT(vp->v_writecount < vp->v_usecount || vp->v_usecount < 1, vp,
	    ("vrele: missed vn_close"));

	if (vp->v_usecount > 1 || ((vp->v_iflag & VI_DOINGINACT) &&
	    vp->v_usecount == 1)) {
		v_incr_usecount(vp, -1);
		VI_UNLOCK(vp);

		return;
	}

	if (vp->v_usecount == 1) {
		v_incr_usecount(vp, -1);
		/*
		 * We must call VOP_INACTIVE with the node locked. Mark
		 * as VI_DOINGINACT to avoid recursion.
		 */
		if (vn_lock(vp, LK_EXCLUSIVE | LK_INTERLOCK, td) == 0) {
			VI_LOCK(vp);
			VNASSERT((vp->v_iflag & VI_DOINGINACT) == 0, vp,
			    ("vrele: recursed on VI_DOINGINACT"));
			vp->v_iflag |= VI_DOINGINACT;
			VI_UNLOCK(vp);
			VOP_INACTIVE(vp, td);
			VI_LOCK(vp);
			VNASSERT(vp->v_iflag & VI_DOINGINACT, vp,
			    ("vrele: lost VI_DOINGINACT"));
			vp->v_iflag &= ~VI_DOINGINACT;
		} else
			VI_LOCK(vp);
		if (VSHOULDFREE(vp))
			vfree(vp);
		else
			vlruvp(vp);
		VI_UNLOCK(vp);

	} else {
#ifdef DIAGNOSTIC
		vprint("vrele: negative ref count", vp);
#endif
		VI_UNLOCK(vp);
		panic("vrele: negative ref cnt");
	}
}

/*
 * Release an already locked vnode.  This give the same effects as
 * unlock+vrele(), but takes less time and avoids releasing and
 * re-aquiring the lock (as vrele() aquires the lock internally.)
 */
void
vput(vp)
	struct vnode *vp;
{
	struct thread *td = curthread;	/* XXX */

	KASSERT(vp != NULL, ("vput: null vp"));
	VI_LOCK(vp);
	/* Skip this v_writecount check if we're going to panic below. */
	VNASSERT(vp->v_writecount < vp->v_usecount || vp->v_usecount < 1, vp,
	    ("vput: missed vn_close"));

	if (vp->v_usecount > 1 || ((vp->v_iflag & VI_DOINGINACT) &&
	    vp->v_usecount == 1)) {
		v_incr_usecount(vp, -1);
		VOP_UNLOCK(vp, LK_INTERLOCK, td);
		return;
	}

	if (vp->v_usecount == 1) {
		v_incr_usecount(vp, -1);
		/*
		 * We must call VOP_INACTIVE with the node locked, so
		 * we just need to release the vnode mutex. Mark as
		 * as VI_DOINGINACT to avoid recursion.
		 */
		VNASSERT((vp->v_iflag & VI_DOINGINACT) == 0, vp,
		    ("vput: recursed on VI_DOINGINACT"));
		vp->v_iflag |= VI_DOINGINACT;
		VI_UNLOCK(vp);
		VOP_INACTIVE(vp, td);
		VI_LOCK(vp);
		VNASSERT(vp->v_iflag & VI_DOINGINACT, vp,
		    ("vput: lost VI_DOINGINACT"));
		vp->v_iflag &= ~VI_DOINGINACT;
		if (VSHOULDFREE(vp))
			vfree(vp);
		else
			vlruvp(vp);
		VI_UNLOCK(vp);

	} else {
#ifdef DIAGNOSTIC
		vprint("vput: negative ref count", vp);
#endif
		panic("vput: negative ref cnt");
	}
}

/*
 * Somebody doesn't want the vnode recycled.
 */
void
vhold(struct vnode *vp)
{

	VI_LOCK(vp);
	vholdl(vp);
	VI_UNLOCK(vp);
}

static void
vholdl(struct vnode *vp)
{

	vp->v_holdcnt++;
	if (VSHOULDBUSY(vp))
		vbusy(vp);
}

/*
 * Note that there is one less who cares about this vnode.  vdrop() is the
 * opposite of vhold().
 */
void
vdrop(struct vnode *vp)
{

	VI_LOCK(vp);
	vdropl(vp);
	VI_UNLOCK(vp);
}

static void
vdropl(struct vnode *vp)
{

	if (vp->v_holdcnt <= 0)
		panic("vdrop: holdcnt");
	vp->v_holdcnt--;
	if (VSHOULDFREE(vp))
		vfree(vp);
	else
		vlruvp(vp);
}

/*
 * Remove any vnodes in the vnode table belonging to mount point mp.
 *
 * If FORCECLOSE is not specified, there should not be any active ones,
 * return error if any are found (nb: this is a user error, not a
 * system error). If FORCECLOSE is specified, detach any active vnodes
 * that are found.
 *
 * If WRITECLOSE is set, only flush out regular file vnodes open for
 * writing.
 *
 * SKIPSYSTEM causes any vnodes marked VV_SYSTEM to be skipped.
 *
 * `rootrefs' specifies the base reference count for the root vnode
 * of this filesystem. The root vnode is considered busy if its
 * v_usecount exceeds this value. On a successful return, vflush(, td)
 * will call vrele() on the root vnode exactly rootrefs times.
 * If the SKIPSYSTEM or WRITECLOSE flags are specified, rootrefs must
 * be zero.
 */
#ifdef DIAGNOSTIC
static int busyprt = 0;		/* print out busy vnodes */
SYSCTL_INT(_debug, OID_AUTO, busyprt, CTLFLAG_RW, &busyprt, 0, "");
#endif

int
vflush(mp, rootrefs, flags, td)
	struct mount *mp;
	int rootrefs;
	int flags;
	struct thread *td;
{
	struct vnode *vp, *nvp, *rootvp = NULL;
	struct vattr vattr;
	int busy = 0, error;

	if (rootrefs > 0) {
		KASSERT((flags & (SKIPSYSTEM | WRITECLOSE)) == 0,
		    ("vflush: bad args"));
		/*
		 * Get the filesystem root vnode. We can vput() it
		 * immediately, since with rootrefs > 0, it won't go away.
		 */
		if ((error = VFS_ROOT(mp, &rootvp, td)) != 0)
			return (error);
		vput(rootvp);

	}
	MNT_ILOCK(mp);
loop:
	MNT_VNODE_FOREACH(vp, mp, nvp) {

		VI_LOCK(vp);
		MNT_IUNLOCK(mp);
		error = vn_lock(vp, LK_INTERLOCK | LK_EXCLUSIVE, td);
		if (error) {
			MNT_ILOCK(mp);
			goto loop;
		}
		/*
		 * Skip over a vnodes marked VV_SYSTEM.
		 */
		if ((flags & SKIPSYSTEM) && (vp->v_vflag & VV_SYSTEM)) {
			VOP_UNLOCK(vp, 0, td);
			MNT_ILOCK(mp);
			continue;
		}
		/*
		 * If WRITECLOSE is set, flush out unlinked but still open
		 * files (even if open only for reading) and regular file
		 * vnodes open for writing.
		 */
		if (flags & WRITECLOSE) {
			error = VOP_GETATTR(vp, &vattr, td->td_ucred, td);
			VI_LOCK(vp);

			if ((vp->v_type == VNON ||
			    (error == 0 && vattr.va_nlink > 0)) &&
			    (vp->v_writecount == 0 || vp->v_type != VREG)) {
				VOP_UNLOCK(vp, LK_INTERLOCK, td);
				MNT_ILOCK(mp);
				continue;
			}
		} else
			VI_LOCK(vp);

		VOP_UNLOCK(vp, 0, td);

		/*
		 * With v_usecount == 0, all we need to do is clear out the
		 * vnode data structures and we are done.
		 */
		if (vp->v_usecount == 0) {
			vgonel(vp, td);
			MNT_ILOCK(mp);
			continue;
		}

		/*
		 * If FORCECLOSE is set, forcibly close the vnode. For block
		 * or character devices, revert to an anonymous device. For
		 * all other files, just kill them.
		 */
		if (flags & FORCECLOSE) {
			VNASSERT(vp->v_type != VCHR && vp->v_type != VBLK, vp,
			    ("device VNODE %p is FORCECLOSED", vp));
			vgonel(vp, td);
			MNT_ILOCK(mp);
			continue;
		}
#ifdef DIAGNOSTIC
		if (busyprt)
			vprint("vflush: busy vnode", vp);
#endif
		VI_UNLOCK(vp);
		MNT_ILOCK(mp);
		busy++;
	}
	MNT_IUNLOCK(mp);
	if (rootrefs > 0 && (flags & FORCECLOSE) == 0) {
		/*
		 * If just the root vnode is busy, and if its refcount
		 * is equal to `rootrefs', then go ahead and kill it.
		 */
		VI_LOCK(rootvp);
		KASSERT(busy > 0, ("vflush: not busy"));
		VNASSERT(rootvp->v_usecount >= rootrefs, rootvp,
		    ("vflush: usecount %d < rootrefs %d",
		     rootvp->v_usecount, rootrefs));
		if (busy == 1 && rootvp->v_usecount == rootrefs) {
			vgonel(rootvp, td);
			busy = 0;
		} else
			VI_UNLOCK(rootvp);
	}
	if (busy)
		return (EBUSY);
	for (; rootrefs > 0; rootrefs--)
		vrele(rootvp);
	return (0);
}

/*
 * This moves a now (likely recyclable) vnode to the end of the
 * mountlist.  XXX However, it is temporarily disabled until we
 * can clean up ffs_sync() and friends, which have loop restart
 * conditions which this code causes to operate O(N^2).
 */
static void
vlruvp(struct vnode *vp)
{
#if 0
	struct mount *mp;

	if ((mp = vp->v_mount) != NULL) {
		MNT_ILOCK(mp);
		TAILQ_REMOVE(&mp->mnt_nvnodelist, vp, v_nmntvnodes);
		TAILQ_INSERT_TAIL(&mp->mnt_nvnodelist, vp, v_nmntvnodes);
		MNT_IUNLOCK(mp);
	}
#endif
}

static void
vx_lock(struct vnode *vp)
{

	ASSERT_VI_LOCKED(vp, "vx_lock");

	/*
	 * Prevent the vnode from being recycled or brought into use while we
	 * clean it out.
	 */
	if (vp->v_iflag & VI_XLOCK)
		panic("vx_lock: deadlock");
	vp->v_iflag |= VI_XLOCK;
	vp->v_vxthread = curthread;
}

static void
vx_unlock(struct vnode *vp)
{
	ASSERT_VI_LOCKED(vp, "vx_unlock");
	vp->v_iflag &= ~VI_XLOCK;
	vp->v_vxthread = NULL;
	if (vp->v_iflag & VI_XWANT) {
		vp->v_iflag &= ~VI_XWANT;
		wakeup(vp);
	}
}

int
vx_wait(struct vnode *vp)
{
	int locked;

	ASSERT_VI_UNLOCKED(vp, "vx_wait");
	VI_LOCK(vp);
	locked = vx_waitl(vp);
	VI_UNLOCK(vp);
	return (locked);
}

int
vx_waitl(struct vnode *vp)
{
	int locked = 0;

	ASSERT_VI_LOCKED(vp, "vx_wait");
	while (vp->v_iflag & VI_XLOCK) {
		locked = 1;
		vp->v_iflag |= VI_XWANT;
		msleep(vp, VI_MTX(vp), PINOD, "vxwait", 0);
	}
	return (locked);
}

/*
 * Recycle an unused vnode to the front of the free list.
 * Release the passed interlock if the vnode will be recycled.
 */
int
vrecycle(struct vnode *vp, struct thread *td)
{

	VI_LOCK(vp);
	if (vp->v_usecount == 0) {
		vgonel(vp, td);
		return (1);
	}
	VI_UNLOCK(vp);
	return (0);
}

/*
 * Eliminate all activity associated with a vnode
 * in preparation for reuse.
 */
void
vgone(struct vnode *vp)
{
	struct thread *td = curthread;	/* XXX */

	VI_LOCK(vp);
	vgonel(vp, td);
}

/*
 * vgone, with the vp interlock held.
 */
void
vgonel(struct vnode *vp, struct thread *td)
{
	int active;

	/*
	 * If a vgone (or vclean) is already in progress,
	 * wait until it is done and return.
	 */
	ASSERT_VI_LOCKED(vp, "vgonel");
	if (vx_waitl(vp)) {
		VI_UNLOCK(vp);
		return;
	}

	vx_lock(vp);

	/*
	 * Check to see if the vnode is in use. If so we have to reference it
	 * before we clean it out so that its count cannot fall to zero and
	 * generate a race against ourselves to recycle it.
	 */
	if ((active = vp->v_usecount))
		v_incr_usecount(vp, 1);

	/*
	 * Even if the count is zero, the VOP_INACTIVE routine may still
	 * have the object locked while it cleans it out. The VOP_LOCK
	 * ensures that the VOP_INACTIVE routine is done with its work.
	 * For active vnodes, it ensures that no other activity can
	 * occur while the underlying object is being cleaned out.
	 */
	VOP_LOCK(vp, LK_DRAIN | LK_INTERLOCK, td);

	/*
	 * Clean out any buffers associated with the vnode.
	 * If the flush fails, just toss the buffers.
	 */
	if (!TAILQ_EMPTY(&vp->v_bufobj.bo_dirty.bv_hd));
		(void) vn_write_suspend_wait(vp, NULL, V_WAIT);
	if (vinvalbuf(vp, V_SAVE, td, 0, 0) != 0)
		vinvalbuf(vp, 0, td, 0, 0);

	/*
	 * Any other processes trying to obtain this lock must first
	 * wait for VXLOCK to clear, then call the new lock operation.
	 */
	VOP_UNLOCK(vp, 0, td);

	/*
	 * If purging an active vnode, it must be closed and
	 * deactivated before being reclaimed. Note that the
	 * VOP_INACTIVE will unlock the vnode.
	 */
	if (active) {
		VOP_CLOSE(vp, FNONBLOCK, NOCRED, td);
		VI_LOCK(vp);
		if ((vp->v_iflag & VI_DOINGINACT) == 0) {
			VNASSERT((vp->v_iflag & VI_DOINGINACT) == 0, vp,
			    ("vclean: recursed on VI_DOINGINACT"));
			vp->v_iflag |= VI_DOINGINACT;
			VI_UNLOCK(vp);
			if (vn_lock(vp, LK_EXCLUSIVE | LK_NOWAIT, td) != 0)
				panic("vclean: cannot relock.");
			VOP_INACTIVE(vp, td);
			VI_LOCK(vp);
			VNASSERT(vp->v_iflag & VI_DOINGINACT, vp,
			    ("vclean: lost VI_DOINGINACT"));
			vp->v_iflag &= ~VI_DOINGINACT;
		}
		VI_UNLOCK(vp);
	}
	/*
	 * Reclaim the vnode.
	 */
	if (VOP_RECLAIM(vp, td))
		panic("vclean: cannot reclaim");

	VNASSERT(vp->v_object == NULL, vp,
	    ("vop_reclaim left v_object vp=%p, tag=%s", vp, vp->v_tag));

	if (active) {
		/*
		 * Inline copy of vrele() since VOP_INACTIVE
		 * has already been called.
		 */
		VI_LOCK(vp);
		v_incr_usecount(vp, -1);
		if (vp->v_usecount <= 0) {
#ifdef INVARIANTS
			if (vp->v_usecount < 0 || vp->v_writecount != 0) {
				vprint("vclean: bad ref count", vp);
				panic("vclean: ref cnt");
			}
#endif
			if (VSHOULDFREE(vp))
				vfree(vp);
		}
		VI_UNLOCK(vp);
	}
	/*
	 * Delete from old mount point vnode list.
	 */
	delmntque(vp);
	cache_purge(vp);
	VI_LOCK(vp);
	if (VSHOULDFREE(vp))
		vfree(vp);

	/*
	 * Done with purge, reset to the standard lock and
	 * notify sleepers of the grim news.
	 */
	vp->v_vnlock = &vp->v_lock;
	vp->v_op = &dead_vnodeops;
	vp->v_tag = "none";

	VI_UNLOCK(vp);

	/*
	 * If special device, remove it from special device alias list
	 * if it is on one.
	 */
	VI_LOCK(vp);

	/*
	 * If it is on the freelist and not already at the head,
	 * move it to the head of the list. The test of the
	 * VDOOMED flag and the reference count of zero is because
	 * it will be removed from the free list by getnewvnode,
	 * but will not have its reference count incremented until
	 * after calling vgone. If the reference count were
	 * incremented first, vgone would (incorrectly) try to
	 * close the previous instance of the underlying object.
	 */
	if (vp->v_usecount == 0 && !(vp->v_iflag & VI_DOOMED)) {
		mtx_lock(&vnode_free_list_mtx);
		if (vp->v_iflag & VI_FREE) {
			TAILQ_REMOVE(&vnode_free_list, vp, v_freelist);
		} else {
			vp->v_iflag |= VI_FREE;
			freevnodes++;
		}
		TAILQ_INSERT_HEAD(&vnode_free_list, vp, v_freelist);
		mtx_unlock(&vnode_free_list_mtx);
	}

	vp->v_type = VBAD;
	vx_unlock(vp);
	VI_UNLOCK(vp);
}

/*
 * Calculate the total number of references to a special device.
 */
int
vcount(vp)
	struct vnode *vp;
{
	int count;

	dev_lock();
	count = vp->v_rdev->si_usecount;
	dev_unlock();
	return (count);
}

/*
 * Same as above, but using the struct cdev *as argument
 */
int
count_dev(dev)
	struct cdev *dev;
{
	int count;

	dev_lock();
	count = dev->si_usecount;
	dev_unlock();
	return(count);
}

/*
 * Print out a description of a vnode.
 */
static char *typename[] =
{"VNON", "VREG", "VDIR", "VBLK", "VCHR", "VLNK", "VSOCK", "VFIFO", "VBAD"};

void
vn_printf(struct vnode *vp, const char *fmt, ...)
{
	va_list ap;
	char buf[96];

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("%p: ", (void *)vp);
	printf("tag %s, type %s\n", vp->v_tag, typename[vp->v_type]);
	printf("    usecount %d, writecount %d, refcount %d mountedhere %p\n",
	    vp->v_usecount, vp->v_writecount, vp->v_holdcnt, vp->v_mountedhere);
	buf[0] = '\0';
	buf[1] = '\0';
	if (vp->v_vflag & VV_ROOT)
		strcat(buf, "|VV_ROOT");
	if (vp->v_vflag & VV_TEXT)
		strcat(buf, "|VV_TEXT");
	if (vp->v_vflag & VV_SYSTEM)
		strcat(buf, "|VV_SYSTEM");
	if (vp->v_iflag & VI_XLOCK)
		strcat(buf, "|VI_XLOCK");
	if (vp->v_iflag & VI_XWANT)
		strcat(buf, "|VI_XWANT");
	if (vp->v_iflag & VI_DOOMED)
		strcat(buf, "|VI_DOOMED");
	if (vp->v_iflag & VI_FREE)
		strcat(buf, "|VI_FREE");
	printf("    flags (%s)\n", buf + 1);
	if (mtx_owned(VI_MTX(vp)))
		printf(" VI_LOCKed");
	if (vp->v_object != NULL);
		printf("    v_object %p\n", vp->v_object);
	printf("    ");
	lockmgr_printinfo(vp->v_vnlock);
	printf("\n");
	if (vp->v_data != NULL)
		VOP_PRINT(vp);
}

#ifdef DDB
#include <ddb/ddb.h>
/*
 * List all of the locked vnodes in the system.
 * Called when debugging the kernel.
 */
DB_SHOW_COMMAND(lockedvnods, lockedvnodes)
{
	struct mount *mp, *nmp;
	struct vnode *vp;

	/*
	 * Note: because this is DDB, we can't obey the locking semantics
	 * for these structures, which means we could catch an inconsistent
	 * state and dereference a nasty pointer.  Not much to be done
	 * about that.
	 */
	printf("Locked vnodes\n");
	for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nmp) {
		nmp = TAILQ_NEXT(mp, mnt_list);
		TAILQ_FOREACH(vp, &mp->mnt_nvnodelist, v_nmntvnodes) {
			if (VOP_ISLOCKED(vp, NULL))
				vprint("", vp);
		}
		nmp = TAILQ_NEXT(mp, mnt_list);
	}
}
#endif

/*
 * Fill in a struct xvfsconf based on a struct vfsconf.
 */
static void
vfsconf2x(struct vfsconf *vfsp, struct xvfsconf *xvfsp)
{

	strcpy(xvfsp->vfc_name, vfsp->vfc_name);
	xvfsp->vfc_typenum = vfsp->vfc_typenum;
	xvfsp->vfc_refcount = vfsp->vfc_refcount;
	xvfsp->vfc_flags = vfsp->vfc_flags;
	/*
	 * These are unused in userland, we keep them
	 * to not break binary compatibility.
	 */
	xvfsp->vfc_vfsops = NULL;
	xvfsp->vfc_next = NULL;
}

/*
 * Top level filesystem related information gathering.
 */
static int
sysctl_vfs_conflist(SYSCTL_HANDLER_ARGS)
{
	struct vfsconf *vfsp;
	struct xvfsconf xvfsp;
	int error;

	error = 0;
	TAILQ_FOREACH(vfsp, &vfsconf, vfc_list) {
		vfsconf2x(vfsp, &xvfsp);
		error = SYSCTL_OUT(req, &xvfsp, sizeof xvfsp);
		if (error)
			break;
	}
	return (error);
}

SYSCTL_PROC(_vfs, OID_AUTO, conflist, CTLFLAG_RD, NULL, 0, sysctl_vfs_conflist,
    "S,xvfsconf", "List of all configured filesystems");

#ifndef BURN_BRIDGES
static int	sysctl_ovfs_conf(SYSCTL_HANDLER_ARGS);

static int
vfs_sysctl(SYSCTL_HANDLER_ARGS)
{
	int *name = (int *)arg1 - 1;	/* XXX */
	u_int namelen = arg2 + 1;	/* XXX */
	struct vfsconf *vfsp;
	struct xvfsconf xvfsp;

	printf("WARNING: userland calling deprecated sysctl, "
	    "please rebuild world\n");

#if 1 || defined(COMPAT_PRELITE2)
	/* Resolve ambiguity between VFS_VFSCONF and VFS_GENERIC. */
	if (namelen == 1)
		return (sysctl_ovfs_conf(oidp, arg1, arg2, req));
#endif

	switch (name[1]) {
	case VFS_MAXTYPENUM:
		if (namelen != 2)
			return (ENOTDIR);
		return (SYSCTL_OUT(req, &maxvfsconf, sizeof(int)));
	case VFS_CONF:
		if (namelen != 3)
			return (ENOTDIR);	/* overloaded */
		TAILQ_FOREACH(vfsp, &vfsconf, vfc_list)
			if (vfsp->vfc_typenum == name[2])
				break;
		if (vfsp == NULL)
			return (EOPNOTSUPP);
		vfsconf2x(vfsp, &xvfsp);
		return (SYSCTL_OUT(req, &xvfsp, sizeof(xvfsp)));
	}
	return (EOPNOTSUPP);
}

static SYSCTL_NODE(_vfs, VFS_GENERIC, generic, CTLFLAG_RD | CTLFLAG_SKIP,
	vfs_sysctl, "Generic filesystem");

#if 1 || defined(COMPAT_PRELITE2)

static int
sysctl_ovfs_conf(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct vfsconf *vfsp;
	struct ovfsconf ovfs;

	TAILQ_FOREACH(vfsp, &vfsconf, vfc_list) {
		ovfs.vfc_vfsops = vfsp->vfc_vfsops;	/* XXX used as flag */
		strcpy(ovfs.vfc_name, vfsp->vfc_name);
		ovfs.vfc_index = vfsp->vfc_typenum;
		ovfs.vfc_refcount = vfsp->vfc_refcount;
		ovfs.vfc_flags = vfsp->vfc_flags;
		error = SYSCTL_OUT(req, &ovfs, sizeof ovfs);
		if (error)
			return error;
	}
	return 0;
}

#endif /* 1 || COMPAT_PRELITE2 */
#endif /* !BURN_BRIDGES */

#define KINFO_VNODESLOP		10
#ifdef notyet
/*
 * Dump vnode list (via sysctl).
 */
/* ARGSUSED */
static int
sysctl_vnode(SYSCTL_HANDLER_ARGS)
{
	struct xvnode *xvn;
	struct thread *td = req->td;
	struct mount *mp;
	struct vnode *vp;
	int error, len, n;

	/*
	 * Stale numvnodes access is not fatal here.
	 */
	req->lock = 0;
	len = (numvnodes + KINFO_VNODESLOP) * sizeof *xvn;
	if (!req->oldptr)
		/* Make an estimate */
		return (SYSCTL_OUT(req, 0, len));

	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);
	xvn = malloc(len, M_TEMP, M_ZERO | M_WAITOK);
	n = 0;
	mtx_lock(&mountlist_mtx);
	TAILQ_FOREACH(mp, &mountlist, mnt_list) {
		if (vfs_busy(mp, LK_NOWAIT, &mountlist_mtx, td))
			continue;
		MNT_ILOCK(mp);
		TAILQ_FOREACH(vp, &mp->mnt_nvnodelist, v_nmntvnodes) {
			if (n == len)
				break;
			vref(vp);
			xvn[n].xv_size = sizeof *xvn;
			xvn[n].xv_vnode = vp;
#define XV_COPY(field) xvn[n].xv_##field = vp->v_##field
			XV_COPY(usecount);
			XV_COPY(writecount);
			XV_COPY(holdcnt);
			XV_COPY(id);
			XV_COPY(mount);
			XV_COPY(numoutput);
			XV_COPY(type);
#undef XV_COPY
			xvn[n].xv_flag = vp->v_vflag;

			switch (vp->v_type) {
			case VREG:
			case VDIR:
			case VLNK:
				break;
			case VBLK:
			case VCHR:
				if (vp->v_rdev == NULL) {
					vrele(vp);
					continue;
				}
				xvn[n].xv_dev = dev2udev(vp->v_rdev);
				break;
			case VSOCK:
				xvn[n].xv_socket = vp->v_socket;
				break;
			case VFIFO:
				xvn[n].xv_fifo = vp->v_fifoinfo;
				break;
			case VNON:
			case VBAD:
			default:
				/* shouldn't happen? */
				vrele(vp);
				continue;
			}
			vrele(vp);
			++n;
		}
		MNT_IUNLOCK(mp);
		mtx_lock(&mountlist_mtx);
		vfs_unbusy(mp, td);
		if (n == len)
			break;
	}
	mtx_unlock(&mountlist_mtx);

	error = SYSCTL_OUT(req, xvn, n * sizeof *xvn);
	free(xvn, M_TEMP);
	return (error);
}

SYSCTL_PROC(_kern, KERN_VNODE, vnode, CTLTYPE_OPAQUE|CTLFLAG_RD,
	0, 0, sysctl_vnode, "S,xvnode", "");
#endif

/*
 * Unmount all filesystems. The list is traversed in reverse order
 * of mounting to avoid dependencies.
 */
void
vfs_unmountall()
{
	struct mount *mp;
	struct thread *td;
	int error;

	if (curthread != NULL)
		td = curthread;
	else
		td = FIRST_THREAD_IN_PROC(initproc); /* XXX XXX proc0? */
	/*
	 * Since this only runs when rebooting, it is not interlocked.
	 */
	while(!TAILQ_EMPTY(&mountlist)) {
		mp = TAILQ_LAST(&mountlist, mntlist);
		error = dounmount(mp, MNT_FORCE, td);
		if (error) {
			TAILQ_REMOVE(&mountlist, mp, mnt_list);
			printf("unmount of %s failed (",
			    mp->mnt_stat.f_mntonname);
			if (error == EBUSY)
				printf("BUSY)\n");
			else
				printf("%d)\n", error);
		} else {
			/* The unmount has removed mp from the mountlist */
		}
	}
}

/*
 * perform msync on all vnodes under a mount point
 * the mount point must be locked.
 */
void
vfs_msync(struct mount *mp, int flags)
{
	struct vnode *vp, *nvp;
	struct vm_object *obj;
	int tries;

	tries = 5;
	MNT_ILOCK(mp);
loop:
	TAILQ_FOREACH_SAFE(vp, &mp->mnt_nvnodelist, v_nmntvnodes, nvp) {
		if (vp->v_mount != mp) {
			if (--tries > 0)
				goto loop;
			break;
		}

		VI_LOCK(vp);
		if (vp->v_iflag & VI_XLOCK) {
			VI_UNLOCK(vp);
			continue;
		}

		if ((vp->v_iflag & VI_OBJDIRTY) &&
		    (flags == MNT_WAIT || VOP_ISLOCKED(vp, NULL) == 0)) {
			MNT_IUNLOCK(mp);
			if (!vget(vp,
			    LK_EXCLUSIVE | LK_RETRY | LK_INTERLOCK,
			    curthread)) {
				if (vp->v_vflag & VV_NOSYNC) {	/* unlinked */
					vput(vp);
					MNT_ILOCK(mp);
					continue;
				}

				obj = vp->v_object;
				if (obj != NULL) {
					VM_OBJECT_LOCK(obj);
					vm_object_page_clean(obj, 0, 0,
					    flags == MNT_WAIT ?
					    OBJPC_SYNC : OBJPC_NOSYNC);
					VM_OBJECT_UNLOCK(obj);
				}
				vput(vp);
			}
			MNT_ILOCK(mp);
			if (TAILQ_NEXT(vp, v_nmntvnodes) != nvp) {
				if (--tries > 0)
					goto loop;
				break;
			}
		} else
			VI_UNLOCK(vp);
	}
	MNT_IUNLOCK(mp);
}

/*
 * Mark a vnode as free, putting it up for recycling.
 */
void
vfree(struct vnode *vp)
{

	ASSERT_VI_LOCKED(vp, "vfree");
	mtx_lock(&vnode_free_list_mtx);
	VNASSERT((vp->v_iflag & VI_FREE) == 0, vp, ("vnode already free"));
	if (vp->v_iflag & VI_AGE) {
		TAILQ_INSERT_HEAD(&vnode_free_list, vp, v_freelist);
	} else {
		TAILQ_INSERT_TAIL(&vnode_free_list, vp, v_freelist);
	}
	freevnodes++;
	mtx_unlock(&vnode_free_list_mtx);
	vp->v_iflag &= ~VI_AGE;
	vp->v_iflag |= VI_FREE;
}

/*
 * Opposite of vfree() - mark a vnode as in use.
 */
static void
vbusy(struct vnode *vp)
{

	ASSERT_VI_LOCKED(vp, "vbusy");
	VNASSERT((vp->v_iflag & VI_FREE) != 0, vp, ("vnode not free"));

	mtx_lock(&vnode_free_list_mtx);
	TAILQ_REMOVE(&vnode_free_list, vp, v_freelist);
	freevnodes--;
	mtx_unlock(&vnode_free_list_mtx);

	vp->v_iflag &= ~(VI_FREE|VI_AGE);
}

/*
 * Initalize per-vnode helper structure to hold poll-related state.
 */
void
v_addpollinfo(struct vnode *vp)
{
	struct vpollinfo *vi;

	vi = uma_zalloc(vnodepoll_zone, M_WAITOK);
	if (vp->v_pollinfo != NULL) {
		uma_zfree(vnodepoll_zone, vi);
		return;
	}
	vp->v_pollinfo = vi;
	mtx_init(&vp->v_pollinfo->vpi_lock, "vnode pollinfo", NULL, MTX_DEF);
	knlist_init(&vp->v_pollinfo->vpi_selinfo.si_note,
	    &vp->v_pollinfo->vpi_lock);
}

/*
 * Record a process's interest in events which might happen to
 * a vnode.  Because poll uses the historic select-style interface
 * internally, this routine serves as both the ``check for any
 * pending events'' and the ``record my interest in future events''
 * functions.  (These are done together, while the lock is held,
 * to avoid race conditions.)
 */
int
vn_pollrecord(vp, td, events)
	struct vnode *vp;
	struct thread *td;
	short events;
{

	if (vp->v_pollinfo == NULL)
		v_addpollinfo(vp);
	mtx_lock(&vp->v_pollinfo->vpi_lock);
	if (vp->v_pollinfo->vpi_revents & events) {
		/*
		 * This leaves events we are not interested
		 * in available for the other process which
		 * which presumably had requested them
		 * (otherwise they would never have been
		 * recorded).
		 */
		events &= vp->v_pollinfo->vpi_revents;
		vp->v_pollinfo->vpi_revents &= ~events;

		mtx_unlock(&vp->v_pollinfo->vpi_lock);
		return events;
	}
	vp->v_pollinfo->vpi_events |= events;
	selrecord(td, &vp->v_pollinfo->vpi_selinfo);
	mtx_unlock(&vp->v_pollinfo->vpi_lock);
	return 0;
}

/*
 * Routine to create and manage a filesystem syncer vnode.
 */
#define sync_close ((int (*)(struct  vop_close_args *))nullop)
static int	sync_fsync(struct  vop_fsync_args *);
static int	sync_inactive(struct  vop_inactive_args *);
static int	sync_reclaim(struct  vop_reclaim_args *);

static struct vop_vector sync_vnodeops = {
	.vop_bypass =	VOP_EOPNOTSUPP,
	.vop_close =	sync_close,		/* close */
	.vop_fsync =	sync_fsync,		/* fsync */
	.vop_inactive =	sync_inactive,	/* inactive */
	.vop_reclaim =	sync_reclaim,	/* reclaim */
	.vop_lock =	vop_stdlock,	/* lock */
	.vop_unlock =	vop_stdunlock,	/* unlock */
	.vop_islocked =	vop_stdislocked,	/* islocked */
};

/*
 * Create a new filesystem syncer vnode for the specified mount point.
 */
int
vfs_allocate_syncvnode(mp)
	struct mount *mp;
{
	struct vnode *vp;
	static long start, incr, next;
	int error;

	/* Allocate a new vnode */
	if ((error = getnewvnode("syncer", mp, &sync_vnodeops, &vp)) != 0) {
		mp->mnt_syncer = NULL;
		return (error);
	}
	vp->v_type = VNON;
	/*
	 * Place the vnode onto the syncer worklist. We attempt to
	 * scatter them about on the list so that they will go off
	 * at evenly distributed times even if all the filesystems
	 * are mounted at once.
	 */
	next += incr;
	if (next == 0 || next > syncer_maxdelay) {
		start /= 2;
		incr /= 2;
		if (start == 0) {
			start = syncer_maxdelay / 2;
			incr = syncer_maxdelay;
		}
		next = start;
	}
	VI_LOCK(vp);
	vn_syncer_add_to_worklist(&vp->v_bufobj,
	    syncdelay > 0 ? next % syncdelay : 0);
	/* XXX - vn_syncer_add_to_worklist() also grabs and drops sync_mtx. */
	mtx_lock(&sync_mtx);
	sync_vnode_count++;
	mtx_unlock(&sync_mtx);
	VI_UNLOCK(vp);
	mp->mnt_syncer = vp;
	return (0);
}

/*
 * Do a lazy sync of the filesystem.
 */
static int
sync_fsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *syncvp = ap->a_vp;
	struct mount *mp = syncvp->v_mount;
	struct thread *td = ap->a_td;
	int error, asyncflag;
	struct bufobj *bo;

	/*
	 * We only need to do something if this is a lazy evaluation.
	 */
	if (ap->a_waitfor != MNT_LAZY)
		return (0);

	/*
	 * Move ourselves to the back of the sync list.
	 */
	bo = &syncvp->v_bufobj;
	BO_LOCK(bo);
	vn_syncer_add_to_worklist(bo, syncdelay);
	BO_UNLOCK(bo);

	/*
	 * Walk the list of vnodes pushing all that are dirty and
	 * not already on the sync list.
	 */
	mtx_lock(&mountlist_mtx);
	if (vfs_busy(mp, LK_EXCLUSIVE | LK_NOWAIT, &mountlist_mtx, td) != 0) {
		mtx_unlock(&mountlist_mtx);
		return (0);
	}
	if (vn_start_write(NULL, &mp, V_NOWAIT) != 0) {
		vfs_unbusy(mp, td);
		return (0);
	}
	asyncflag = mp->mnt_flag & MNT_ASYNC;
	mp->mnt_flag &= ~MNT_ASYNC;
	vfs_msync(mp, MNT_NOWAIT);
	error = VFS_SYNC(mp, MNT_LAZY, td);
	if (asyncflag)
		mp->mnt_flag |= MNT_ASYNC;
	vn_finished_write(mp);
	vfs_unbusy(mp, td);
	return (error);
}

/*
 * The syncer vnode is no referenced.
 */
static int
sync_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap;
{

	VOP_UNLOCK(ap->a_vp, 0, ap->a_td);
	vgone(ap->a_vp);
	return (0);
}

/*
 * The syncer vnode is no longer needed and is being decommissioned.
 *
 * Modifications to the worklist must be protected by sync_mtx.
 */
static int
sync_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct bufobj *bo;

	VI_LOCK(vp);
	bo = &vp->v_bufobj;
	vp->v_mount->mnt_syncer = NULL;
	if (bo->bo_flag & BO_ONWORKLST) {
		mtx_lock(&sync_mtx);
		LIST_REMOVE(bo, bo_synclist);
 		syncer_worklist_len--;
		sync_vnode_count--;
		mtx_unlock(&sync_mtx);
		bo->bo_flag &= ~BO_ONWORKLST;
	}
	VI_UNLOCK(vp);

	return (0);
}

/*
 * Check if vnode represents a disk device
 */
int
vn_isdisk(vp, errp)
	struct vnode *vp;
	int *errp;
{
	int error;

	error = 0;
	dev_lock();
	if (vp->v_type != VCHR)
		error = ENOTBLK;
	else if (vp->v_rdev == NULL)
		error = ENXIO;
	else if (vp->v_rdev->si_devsw == NULL)
		error = ENXIO;
	else if (!(vp->v_rdev->si_devsw->d_flags & D_DISK))
		error = ENOTBLK;
	dev_unlock();
	if (errp != NULL)
		*errp = error;
	return (error == 0);
}

/*
 * Free data allocated by namei(); see namei(9) for details.
 */
void
NDFREE(ndp, flags)
     struct nameidata *ndp;
     const u_int flags;
{

	if (!(flags & NDF_NO_FREE_PNBUF) &&
	    (ndp->ni_cnd.cn_flags & HASBUF)) {
		uma_zfree(namei_zone, ndp->ni_cnd.cn_pnbuf);
		ndp->ni_cnd.cn_flags &= ~HASBUF;
	}
	if (!(flags & NDF_NO_DVP_UNLOCK) &&
	    (ndp->ni_cnd.cn_flags & LOCKPARENT) &&
	    ndp->ni_dvp != ndp->ni_vp)
		VOP_UNLOCK(ndp->ni_dvp, 0, ndp->ni_cnd.cn_thread);
	if (!(flags & NDF_NO_DVP_RELE) &&
	    (ndp->ni_cnd.cn_flags & (LOCKPARENT|WANTPARENT))) {
		vrele(ndp->ni_dvp);
		ndp->ni_dvp = NULL;
	}
	if (!(flags & NDF_NO_VP_UNLOCK) &&
	    (ndp->ni_cnd.cn_flags & LOCKLEAF) && ndp->ni_vp)
		VOP_UNLOCK(ndp->ni_vp, 0, ndp->ni_cnd.cn_thread);
	if (!(flags & NDF_NO_VP_RELE) &&
	    ndp->ni_vp) {
		vrele(ndp->ni_vp);
		ndp->ni_vp = NULL;
	}
	if (!(flags & NDF_NO_STARTDIR_RELE) &&
	    (ndp->ni_cnd.cn_flags & SAVESTART)) {
		vrele(ndp->ni_startdir);
		ndp->ni_startdir = NULL;
	}
}

/*
 * Common filesystem object access control check routine.  Accepts a
 * vnode's type, "mode", uid and gid, requested access mode, credentials,
 * and optional call-by-reference privused argument allowing vaccess()
 * to indicate to the caller whether privilege was used to satisfy the
 * request (obsoleted).  Returns 0 on success, or an errno on failure.
 */
int
vaccess(type, file_mode, file_uid, file_gid, acc_mode, cred, privused)
	enum vtype type;
	mode_t file_mode;
	uid_t file_uid;
	gid_t file_gid;
	mode_t acc_mode;
	struct ucred *cred;
	int *privused;
{
	mode_t dac_granted;
#ifdef CAPABILITIES
	mode_t cap_granted;
#endif

	/*
	 * Look for a normal, non-privileged way to access the file/directory
	 * as requested.  If it exists, go with that.
	 */

	if (privused != NULL)
		*privused = 0;

	dac_granted = 0;

	/* Check the owner. */
	if (cred->cr_uid == file_uid) {
		dac_granted |= VADMIN;
		if (file_mode & S_IXUSR)
			dac_granted |= VEXEC;
		if (file_mode & S_IRUSR)
			dac_granted |= VREAD;
		if (file_mode & S_IWUSR)
			dac_granted |= (VWRITE | VAPPEND);

		if ((acc_mode & dac_granted) == acc_mode)
			return (0);

		goto privcheck;
	}

	/* Otherwise, check the groups (first match) */
	if (groupmember(file_gid, cred)) {
		if (file_mode & S_IXGRP)
			dac_granted |= VEXEC;
		if (file_mode & S_IRGRP)
			dac_granted |= VREAD;
		if (file_mode & S_IWGRP)
			dac_granted |= (VWRITE | VAPPEND);

		if ((acc_mode & dac_granted) == acc_mode)
			return (0);

		goto privcheck;
	}

	/* Otherwise, check everyone else. */
	if (file_mode & S_IXOTH)
		dac_granted |= VEXEC;
	if (file_mode & S_IROTH)
		dac_granted |= VREAD;
	if (file_mode & S_IWOTH)
		dac_granted |= (VWRITE | VAPPEND);
	if ((acc_mode & dac_granted) == acc_mode)
		return (0);

privcheck:
	if (!suser_cred(cred, SUSER_ALLOWJAIL)) {
		/* XXX audit: privilege used */
		if (privused != NULL)
			*privused = 1;
		return (0);
	}

#ifdef CAPABILITIES
	/*
	 * Build a capability mask to determine if the set of capabilities
	 * satisfies the requirements when combined with the granted mask
	 * from above.
	 * For each capability, if the capability is required, bitwise
	 * or the request type onto the cap_granted mask.
	 */
	cap_granted = 0;

	if (type == VDIR) {
		/*
		 * For directories, use CAP_DAC_READ_SEARCH to satisfy
		 * VEXEC requests, instead of CAP_DAC_EXECUTE.
		 */
		if ((acc_mode & VEXEC) && ((dac_granted & VEXEC) == 0) &&
		    !cap_check(cred, NULL, CAP_DAC_READ_SEARCH, SUSER_ALLOWJAIL))
			cap_granted |= VEXEC;
	} else {
		if ((acc_mode & VEXEC) && ((dac_granted & VEXEC) == 0) &&
		    !cap_check(cred, NULL, CAP_DAC_EXECUTE, SUSER_ALLOWJAIL))
			cap_granted |= VEXEC;
	}

	if ((acc_mode & VREAD) && ((dac_granted & VREAD) == 0) &&
	    !cap_check(cred, NULL, CAP_DAC_READ_SEARCH, SUSER_ALLOWJAIL))
		cap_granted |= VREAD;

	if ((acc_mode & VWRITE) && ((dac_granted & VWRITE) == 0) &&
	    !cap_check(cred, NULL, CAP_DAC_WRITE, SUSER_ALLOWJAIL))
		cap_granted |= (VWRITE | VAPPEND);

	if ((acc_mode & VADMIN) && ((dac_granted & VADMIN) == 0) &&
	    !cap_check(cred, NULL, CAP_FOWNER, SUSER_ALLOWJAIL))
		cap_granted |= VADMIN;

	if ((acc_mode & (cap_granted | dac_granted)) == acc_mode) {
		/* XXX audit: privilege used */
		if (privused != NULL)
			*privused = 1;
		return (0);
	}
#endif

	return ((acc_mode & VADMIN) ? EPERM : EACCES);
}

/*
 * Credential check based on process requesting service, and per-attribute
 * permissions.
 */
int
extattr_check_cred(struct vnode *vp, int attrnamespace,
    struct ucred *cred, struct thread *td, int access)
{

	/*
	 * Kernel-invoked always succeeds.
	 */
	if (cred == NOCRED)
		return (0);

	/*
	 * Do not allow privileged processes in jail to directly
	 * manipulate system attributes.
	 *
	 * XXX What capability should apply here?
	 * Probably CAP_SYS_SETFFLAG.
	 */
	switch (attrnamespace) {
	case EXTATTR_NAMESPACE_SYSTEM:
		/* Potentially should be: return (EPERM); */
		return (suser_cred(cred, 0));
	case EXTATTR_NAMESPACE_USER:
		return (VOP_ACCESS(vp, access, cred, td));
	default:
		return (EPERM);
	}
}

#ifdef DEBUG_VFS_LOCKS
/*
 * This only exists to supress warnings from unlocked specfs accesses.  It is
 * no longer ok to have an unlocked VFS.
 */
#define	IGNORE_LOCK(vp) ((vp)->v_type == VCHR || (vp)->v_type == VBAD)

int vfs_badlock_ddb = 1;	/* Drop into debugger on violation. */
SYSCTL_INT(_debug, OID_AUTO, vfs_badlock_ddb, CTLFLAG_RW, &vfs_badlock_ddb, 0, "");

int vfs_badlock_mutex = 1;	/* Check for interlock across VOPs. */
SYSCTL_INT(_debug, OID_AUTO, vfs_badlock_mutex, CTLFLAG_RW, &vfs_badlock_mutex, 0, "");

int vfs_badlock_print = 1;	/* Print lock violations. */
SYSCTL_INT(_debug, OID_AUTO, vfs_badlock_print, CTLFLAG_RW, &vfs_badlock_print, 0, "");

#ifdef KDB
int vfs_badlock_backtrace = 1;	/* Print backtrace at lock violations. */
SYSCTL_INT(_debug, OID_AUTO, vfs_badlock_backtrace, CTLFLAG_RW, &vfs_badlock_backtrace, 0, "");
#endif

static void
vfs_badlock(const char *msg, const char *str, struct vnode *vp)
{

#ifdef KDB
	if (vfs_badlock_backtrace)
		kdb_backtrace();
#endif
	if (vfs_badlock_print)
		printf("%s: %p %s\n", str, (void *)vp, msg);
	if (vfs_badlock_ddb)
		kdb_enter("lock violation");
}

void
assert_vi_locked(struct vnode *vp, const char *str)
{

	if (vfs_badlock_mutex && !mtx_owned(VI_MTX(vp)))
		vfs_badlock("interlock is not locked but should be", str, vp);
}

void
assert_vi_unlocked(struct vnode *vp, const char *str)
{

	if (vfs_badlock_mutex && mtx_owned(VI_MTX(vp)))
		vfs_badlock("interlock is locked but should not be", str, vp);
}

void
assert_vop_locked(struct vnode *vp, const char *str)
{

	if (vp && !IGNORE_LOCK(vp) && VOP_ISLOCKED(vp, NULL) == 0 &&
	  !((vp->v_iflag & VI_XLOCK) && vp->v_vxthread == curthread))
		vfs_badlock("is not locked but should be", str, vp);
}

void
assert_vop_unlocked(struct vnode *vp, const char *str)
{

	if (vp && !IGNORE_LOCK(vp) &&
	    VOP_ISLOCKED(vp, curthread) == LK_EXCLUSIVE)
		vfs_badlock("is locked but should not be", str, vp);
}

#if 0
void
assert_vop_elocked(struct vnode *vp, const char *str)
{

	if (vp && !IGNORE_LOCK(vp) &&
	    VOP_ISLOCKED(vp, curthread) != LK_EXCLUSIVE)
		vfs_badlock("is not exclusive locked but should be", str, vp);
}

void
assert_vop_elocked_other(struct vnode *vp, const char *str)
{

	if (vp && !IGNORE_LOCK(vp) &&
	    VOP_ISLOCKED(vp, curthread) != LK_EXCLOTHER)
		vfs_badlock("is not exclusive locked by another thread",
		    str, vp);
}

void
assert_vop_slocked(struct vnode *vp, const char *str)
{

	if (vp && !IGNORE_LOCK(vp) &&
	    VOP_ISLOCKED(vp, curthread) != LK_SHARED)
		vfs_badlock("is not locked shared but should be", str, vp);
}
#endif /* 0 */

void
vop_rename_pre(void *ap)
{
	struct vop_rename_args *a = ap;

	if (a->a_tvp)
		ASSERT_VI_UNLOCKED(a->a_tvp, "VOP_RENAME");
	ASSERT_VI_UNLOCKED(a->a_tdvp, "VOP_RENAME");
	ASSERT_VI_UNLOCKED(a->a_fvp, "VOP_RENAME");
	ASSERT_VI_UNLOCKED(a->a_fdvp, "VOP_RENAME");

	/* Check the source (from). */
	if (a->a_tdvp != a->a_fdvp)
		ASSERT_VOP_UNLOCKED(a->a_fdvp, "vop_rename: fdvp locked");
	if (a->a_tvp != a->a_fvp)
		ASSERT_VOP_UNLOCKED(a->a_fvp, "vop_rename: tvp locked");

	/* Check the target. */
	if (a->a_tvp)
		ASSERT_VOP_LOCKED(a->a_tvp, "vop_rename: tvp not locked");
	ASSERT_VOP_LOCKED(a->a_tdvp, "vop_rename: tdvp not locked");
}

void
vop_strategy_pre(void *ap)
{
	struct vop_strategy_args *a;
	struct buf *bp;

	a = ap;
	bp = a->a_bp;

	/*
	 * Cluster ops lock their component buffers but not the IO container.
	 */
	if ((bp->b_flags & B_CLUSTER) != 0)
		return;

	if (BUF_REFCNT(bp) < 1) {
		if (vfs_badlock_print)
			printf(
			    "VOP_STRATEGY: bp is not locked but should be\n");
		if (vfs_badlock_ddb)
			kdb_enter("lock violation");
	}
}

void
vop_lookup_pre(void *ap)
{
	struct vop_lookup_args *a;
	struct vnode *dvp;

	a = ap;
	dvp = a->a_dvp;
	ASSERT_VI_UNLOCKED(dvp, "VOP_LOOKUP");
	ASSERT_VOP_LOCKED(dvp, "VOP_LOOKUP");
}

void
vop_lookup_post(void *ap, int rc)
{
	struct vop_lookup_args *a;
	struct componentname *cnp;
	struct vnode *dvp;
	struct vnode *vp;
	int flags;

	a = ap;
	dvp = a->a_dvp;
	cnp = a->a_cnp;
	vp = *(a->a_vpp);
	flags = cnp->cn_flags;

	ASSERT_VI_UNLOCKED(dvp, "VOP_LOOKUP");

	/*
	 * If this is the last path component for this lookup and LOCKPARENT
	 * is set, OR if there is an error the directory has to be locked.
	 */
	if ((flags & LOCKPARENT) && (flags & ISLASTCN))
		ASSERT_VOP_LOCKED(dvp, "VOP_LOOKUP (LOCKPARENT)");
	else if (rc != 0)
		ASSERT_VOP_LOCKED(dvp, "VOP_LOOKUP (error)");
	else if (dvp != vp)
		ASSERT_VOP_UNLOCKED(dvp, "VOP_LOOKUP (dvp)");
	if (flags & PDIRUNLOCK)
		ASSERT_VOP_UNLOCKED(dvp, "VOP_LOOKUP (PDIRUNLOCK)");
}

void
vop_lock_pre(void *ap)
{
	struct vop_lock_args *a = ap;

	if (a->a_vp->v_iflag & VI_XLOCK &&
	    a->a_vp->v_vxthread != curthread) {
		vprint("vop_lock_pre:", a->a_vp);
		panic("vop_lock_pre: locked while xlock held.\n");
	}
	if ((a->a_flags & LK_INTERLOCK) == 0)
		ASSERT_VI_UNLOCKED(a->a_vp, "VOP_LOCK");
	else
		ASSERT_VI_LOCKED(a->a_vp, "VOP_LOCK");
}

void
vop_lock_post(void *ap, int rc)
{
	struct vop_lock_args *a = ap;

	ASSERT_VI_UNLOCKED(a->a_vp, "VOP_LOCK");
	if (rc == 0)
		ASSERT_VOP_LOCKED(a->a_vp, "VOP_LOCK");
}

void
vop_unlock_pre(void *ap)
{
	struct vop_unlock_args *a = ap;

	if (a->a_flags & LK_INTERLOCK)
		ASSERT_VI_LOCKED(a->a_vp, "VOP_UNLOCK");
	ASSERT_VOP_LOCKED(a->a_vp, "VOP_UNLOCK");
}

void
vop_unlock_post(void *ap, int rc)
{
	struct vop_unlock_args *a = ap;

	if (a->a_flags & LK_INTERLOCK)
		ASSERT_VI_UNLOCKED(a->a_vp, "VOP_UNLOCK");
}
#endif /* DEBUG_VFS_LOCKS */

static struct knlist fs_knlist;

static void
vfs_event_init(void *arg)
{
	knlist_init(&fs_knlist, NULL);
}
/* XXX - correct order? */
SYSINIT(vfs_knlist, SI_SUB_VFS, SI_ORDER_ANY, vfs_event_init, NULL);

void
vfs_event_signal(fsid_t *fsid, u_int32_t event, intptr_t data __unused)
{

	KNOTE_UNLOCKED(&fs_knlist, event);
}

static int	filt_fsattach(struct knote *kn);
static void	filt_fsdetach(struct knote *kn);
static int	filt_fsevent(struct knote *kn, long hint);

struct filterops fs_filtops =
	{ 0, filt_fsattach, filt_fsdetach, filt_fsevent };

static int
filt_fsattach(struct knote *kn)
{

	kn->kn_flags |= EV_CLEAR;
	knlist_add(&fs_knlist, kn, 0);
	return (0);
}

static void
filt_fsdetach(struct knote *kn)
{

	knlist_remove(&fs_knlist, kn, 0);
}

static int
filt_fsevent(struct knote *kn, long hint)
{

	kn->kn_fflags |= hint;
	return (kn->kn_fflags != 0);
}

static int
sysctl_vfs_ctl(SYSCTL_HANDLER_ARGS)
{
	struct vfsidctl vc;
	int error;
	struct mount *mp;

	error = SYSCTL_IN(req, &vc, sizeof(vc));
	if (error)
		return (error);
	if (vc.vc_vers != VFS_CTL_VERS1)
		return (EINVAL);
	mp = vfs_getvfs(&vc.vc_fsid);
	if (mp == NULL)
		return (ENOENT);
	/* ensure that a specific sysctl goes to the right filesystem. */
	if (strcmp(vc.vc_fstypename, "*") != 0 &&
	    strcmp(vc.vc_fstypename, mp->mnt_vfc->vfc_name) != 0) {
		return (EINVAL);
	}
	VCTLTOREQ(&vc, req);
	return (VFS_SYSCTL(mp, vc.vc_op, req));
}

SYSCTL_PROC(_vfs, OID_AUTO, ctl, CTLFLAG_WR,
        NULL, 0, sysctl_vfs_ctl, "", "Sysctl by fsid");

/*
 * Function to initialize a va_filerev field sensibly.
 * XXX: Wouldn't a random number make a lot more sense ??
 */
u_quad_t
init_va_filerev(void)
{
	struct bintime bt;

	getbinuptime(&bt);
	return (((u_quad_t)bt.sec << 32LL) | (bt.frac >> 32LL));
}
