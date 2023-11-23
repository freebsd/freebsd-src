/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 */

/*
 * External virtual filesystem routines
 */

#include <sys/cdefs.h>
#include "opt_ddb.h"
#include "opt_watchdog.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/asan.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/capsicum.h>
#include <sys/condvar.h>
#include <sys/conf.h>
#include <sys/counter.h>
#include <sys/dirent.h>
#include <sys/event.h>
#include <sys/eventhandler.h>
#include <sys/extattr.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/jail.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/ktr.h>
#include <sys/limits.h>
#include <sys/lockf.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/pctrie.h>
#include <sys/priv.h>
#include <sys/reboot.h>
#include <sys/refcount.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/sleepqueue.h>
#include <sys/smr.h>
#include <sys/smp.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>
#include <sys/watchdog.h>

#include <machine/stdarg.h>

#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>
#include <vm/uma.h>

#if defined(DEBUG_VFS_LOCKS) && (!defined(INVARIANTS) || !defined(WITNESS))
#error DEBUG_VFS_LOCKS requires INVARIANTS and WITNESS
#endif

#ifdef DDB
#include <ddb/ddb.h>
#endif

static void	delmntque(struct vnode *vp);
static int	flushbuflist(struct bufv *bufv, int flags, struct bufobj *bo,
		    int slpflag, int slptimeo);
static void	syncer_shutdown(void *arg, int howto);
static int	vtryrecycle(struct vnode *vp, bool isvnlru);
static void	v_init_counters(struct vnode *);
static void	vn_seqc_init(struct vnode *);
static void	vn_seqc_write_end_free(struct vnode *vp);
static void	vgonel(struct vnode *);
static bool	vhold_recycle_free(struct vnode *);
static void	vdropl_recycle(struct vnode *vp);
static void	vdrop_recycle(struct vnode *vp);
static void	vfs_knllock(void *arg);
static void	vfs_knlunlock(void *arg);
static void	vfs_knl_assert_lock(void *arg, int what);
static void	destroy_vpollinfo(struct vpollinfo *vi);
static int	v_inval_buf_range_locked(struct vnode *vp, struct bufobj *bo,
		    daddr_t startlbn, daddr_t endlbn);
static void	vnlru_recalc(void);

static SYSCTL_NODE(_vfs, OID_AUTO, vnode, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "vnode configuration and statistics");
static SYSCTL_NODE(_vfs_vnode, OID_AUTO, param, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "vnode configuration");
static SYSCTL_NODE(_vfs_vnode, OID_AUTO, stats, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "vnode statistics");
static SYSCTL_NODE(_vfs_vnode, OID_AUTO, vnlru, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "vnode recycling");

/*
 * Number of vnodes in existence.  Increased whenever getnewvnode()
 * allocates a new vnode, decreased in vdropl() for VIRF_DOOMED vnode.
 */
static u_long __exclusive_cache_line numvnodes;

SYSCTL_ULONG(_vfs, OID_AUTO, numvnodes, CTLFLAG_RD, &numvnodes, 0,
    "Number of vnodes in existence (legacy)");
SYSCTL_ULONG(_vfs_vnode_stats, OID_AUTO, count, CTLFLAG_RD, &numvnodes, 0,
    "Number of vnodes in existence");

static counter_u64_t vnodes_created;
SYSCTL_COUNTER_U64(_vfs, OID_AUTO, vnodes_created, CTLFLAG_RD, &vnodes_created,
    "Number of vnodes created by getnewvnode (legacy)");
SYSCTL_COUNTER_U64(_vfs_vnode_stats, OID_AUTO, created, CTLFLAG_RD, &vnodes_created,
    "Number of vnodes created by getnewvnode");

/*
 * Conversion tables for conversion from vnode types to inode formats
 * and back.
 */
__enum_uint8(vtype) iftovt_tab[16] = {
	VNON, VFIFO, VCHR, VNON, VDIR, VNON, VBLK, VNON,
	VREG, VNON, VLNK, VNON, VSOCK, VNON, VNON, VNON
};
int vttoif_tab[10] = {
	0, S_IFREG, S_IFDIR, S_IFBLK, S_IFCHR, S_IFLNK,
	S_IFSOCK, S_IFIFO, S_IFMT, S_IFMT
};

/*
 * List of allocates vnodes in the system.
 */
static TAILQ_HEAD(freelst, vnode) vnode_list;
static struct vnode *vnode_list_free_marker;
static struct vnode *vnode_list_reclaim_marker;

/*
 * "Free" vnode target.  Free vnodes are rarely completely free, but are
 * just ones that are cheap to recycle.  Usually they are for files which
 * have been stat'd but not read; these usually have inode and namecache
 * data attached to them.  This target is the preferred minimum size of a
 * sub-cache consisting mostly of such files. The system balances the size
 * of this sub-cache with its complement to try to prevent either from
 * thrashing while the other is relatively inactive.  The targets express
 * a preference for the best balance.
 *
 * "Above" this target there are 2 further targets (watermarks) related
 * to recyling of free vnodes.  In the best-operating case, the cache is
 * exactly full, the free list has size between vlowat and vhiwat above the
 * free target, and recycling from it and normal use maintains this state.
 * Sometimes the free list is below vlowat or even empty, but this state
 * is even better for immediate use provided the cache is not full.
 * Otherwise, vnlru_proc() runs to reclaim enough vnodes (usually non-free
 * ones) to reach one of these states.  The watermarks are currently hard-
 * coded as 4% and 9% of the available space higher.  These and the default
 * of 25% for wantfreevnodes are too large if the memory size is large.
 * E.g., 9% of 75% of MAXVNODES is more than 566000 vnodes to reclaim
 * whenever vnlru_proc() becomes active.
 */
static long wantfreevnodes;
static long __exclusive_cache_line freevnodes;
static long freevnodes_old;

static u_long recycles_count;
SYSCTL_ULONG(_vfs, OID_AUTO, recycles, CTLFLAG_RD | CTLFLAG_STATS, &recycles_count, 0,
    "Number of vnodes recycled to meet vnode cache targets (legacy)");
SYSCTL_ULONG(_vfs_vnode_vnlru, OID_AUTO, recycles, CTLFLAG_RD | CTLFLAG_STATS,
    &recycles_count, 0,
    "Number of vnodes recycled to meet vnode cache targets");

static u_long recycles_free_count;
SYSCTL_ULONG(_vfs, OID_AUTO, recycles_free, CTLFLAG_RD | CTLFLAG_STATS,
    &recycles_free_count, 0,
    "Number of free vnodes recycled to meet vnode cache targets (legacy)");
SYSCTL_ULONG(_vfs_vnode_vnlru, OID_AUTO, recycles_free, CTLFLAG_RD | CTLFLAG_STATS,
    &recycles_free_count, 0,
    "Number of free vnodes recycled to meet vnode cache targets");

static counter_u64_t direct_recycles_free_count;
SYSCTL_COUNTER_U64(_vfs_vnode_vnlru, OID_AUTO, direct_recycles_free, CTLFLAG_RD,
    &direct_recycles_free_count,
    "Number of free vnodes recycled by vn_alloc callers to meet vnode cache targets");

static counter_u64_t vnode_skipped_requeues;
SYSCTL_COUNTER_U64(_vfs_vnode_stats, OID_AUTO, skipped_requeues, CTLFLAG_RD, &vnode_skipped_requeues,
    "Number of times LRU requeue was skipped due to lock contention");

static u_long deferred_inact;
SYSCTL_ULONG(_vfs, OID_AUTO, deferred_inact, CTLFLAG_RD,
    &deferred_inact, 0, "Number of times inactive processing was deferred");

/* To keep more than one thread at a time from running vfs_getnewfsid */
static struct mtx mntid_mtx;

/*
 * Lock for any access to the following:
 *	vnode_list
 *	numvnodes
 *	freevnodes
 */
static struct mtx __exclusive_cache_line vnode_list_mtx;

/* Publicly exported FS */
struct nfs_public nfs_pub;

static uma_zone_t buf_trie_zone;
static smr_t buf_trie_smr;

/* Zone for allocation of new vnodes - used exclusively by getnewvnode() */
static uma_zone_t vnode_zone;
MALLOC_DEFINE(M_VNODEPOLL, "VN POLL", "vnode poll");

__read_frequently smr_t vfs_smr;

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
static struct cv sync_wakeup;

#define SYNCER_MAXDELAY		32
static int syncer_maxdelay = SYNCER_MAXDELAY;	/* maximum delay time */
static int syncdelay = 30;		/* max time to delay syncing data */
static int filedelay = 30;		/* time to delay syncing files */
SYSCTL_INT(_kern, OID_AUTO, filedelay, CTLFLAG_RW, &filedelay, 0,
    "Time to delay syncing files (in seconds)");
static int dirdelay = 29;		/* time to delay syncing directories */
SYSCTL_INT(_kern, OID_AUTO, dirdelay, CTLFLAG_RW, &dirdelay, 0,
    "Time to delay syncing directories (in seconds)");
static int metadelay = 28;		/* time to delay syncing metadata */
SYSCTL_INT(_kern, OID_AUTO, metadelay, CTLFLAG_RW, &metadelay, 0,
    "Time to delay syncing metadata (in seconds)");
static int rushjob;		/* number of slots to run ASAP */
static int stat_rush_requests;	/* number of times I/O speeded up */
SYSCTL_INT(_debug, OID_AUTO, rush_requests, CTLFLAG_RW, &stat_rush_requests, 0,
    "Number of times I/O speeded up (rush requests)");

#define	VDBATCH_SIZE 8
struct vdbatch {
	u_int index;
	struct mtx lock;
	struct vnode *tab[VDBATCH_SIZE];
};
DPCPU_DEFINE_STATIC(struct vdbatch, vd);

static void	vdbatch_dequeue(struct vnode *vp);

/*
 * When shutting down the syncer, run it at four times normal speed.
 */
#define SYNCER_SHUTDOWN_SPEEDUP		4
static int sync_vnode_count;
static int syncer_worklist_len;
static enum { SYNCER_RUNNING, SYNCER_SHUTTING_DOWN, SYNCER_FINAL_DELAY }
    syncer_state;

/* Target for maximum number of vnodes. */
u_long desiredvnodes;
static u_long gapvnodes;		/* gap between wanted and desired */
static u_long vhiwat;		/* enough extras after expansion */
static u_long vlowat;		/* minimal extras before expansion */
static bool vstir;		/* nonzero to stir non-free vnodes */
static volatile int vsmalltrigger = 8;	/* pref to keep if > this many pages */

static u_long vnlru_read_freevnodes(void);

/*
 * Note that no attempt is made to sanitize these parameters.
 */
static int
sysctl_maxvnodes(SYSCTL_HANDLER_ARGS)
{
	u_long val;
	int error;

	val = desiredvnodes;
	error = sysctl_handle_long(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (val == desiredvnodes)
		return (0);
	mtx_lock(&vnode_list_mtx);
	desiredvnodes = val;
	wantfreevnodes = desiredvnodes / 4;
	vnlru_recalc();
	mtx_unlock(&vnode_list_mtx);
	/*
	 * XXX There is no protection against multiple threads changing
	 * desiredvnodes at the same time. Locking above only helps vnlru and
	 * getnewvnode.
	 */
	vfs_hash_changesize(desiredvnodes);
	cache_changesize(desiredvnodes);
	return (0);
}

SYSCTL_PROC(_kern, KERN_MAXVNODES, maxvnodes,
    CTLTYPE_ULONG | CTLFLAG_MPSAFE | CTLFLAG_RW, NULL, 0, sysctl_maxvnodes,
    "LU", "Target for maximum number of vnodes (legacy)");
SYSCTL_PROC(_vfs_vnode_param, OID_AUTO, limit,
    CTLTYPE_ULONG | CTLFLAG_MPSAFE | CTLFLAG_RW, NULL, 0, sysctl_maxvnodes,
    "LU", "Target for maximum number of vnodes");

static int
sysctl_freevnodes(SYSCTL_HANDLER_ARGS)
{
	u_long rfreevnodes;

	rfreevnodes = vnlru_read_freevnodes();
	return (sysctl_handle_long(oidp, &rfreevnodes, 0, req));
}

SYSCTL_PROC(_vfs, OID_AUTO, freevnodes,
    CTLTYPE_ULONG | CTLFLAG_MPSAFE | CTLFLAG_RD, NULL, 0, sysctl_freevnodes,
    "LU", "Number of \"free\" vnodes (legacy)");
SYSCTL_PROC(_vfs_vnode_stats, OID_AUTO, free,
    CTLTYPE_ULONG | CTLFLAG_MPSAFE | CTLFLAG_RD, NULL, 0, sysctl_freevnodes,
    "LU", "Number of \"free\" vnodes");

static int
sysctl_wantfreevnodes(SYSCTL_HANDLER_ARGS)
{
	u_long val;
	int error;

	val = wantfreevnodes;
	error = sysctl_handle_long(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (val == wantfreevnodes)
		return (0);
	mtx_lock(&vnode_list_mtx);
	wantfreevnodes = val;
	vnlru_recalc();
	mtx_unlock(&vnode_list_mtx);
	return (0);
}

SYSCTL_PROC(_vfs, OID_AUTO, wantfreevnodes,
    CTLTYPE_ULONG | CTLFLAG_MPSAFE | CTLFLAG_RW, NULL, 0, sysctl_wantfreevnodes,
    "LU", "Target for minimum number of \"free\" vnodes (legacy)");
SYSCTL_PROC(_vfs_vnode_param, OID_AUTO, wantfree,
    CTLTYPE_ULONG | CTLFLAG_MPSAFE | CTLFLAG_RW, NULL, 0, sysctl_wantfreevnodes,
    "LU", "Target for minimum number of \"free\" vnodes");

static int vnlru_nowhere;
SYSCTL_INT(_vfs_vnode_vnlru, OID_AUTO, failed_runs, CTLFLAG_RD | CTLFLAG_STATS,
    &vnlru_nowhere, 0, "Number of times the vnlru process ran without success");

static int
sysctl_try_reclaim_vnode(SYSCTL_HANDLER_ARGS)
{
	struct vnode *vp;
	struct nameidata nd;
	char *buf;
	unsigned long ndflags;
	int error;

	if (req->newptr == NULL)
		return (EINVAL);
	if (req->newlen >= PATH_MAX)
		return (E2BIG);

	buf = malloc(PATH_MAX, M_TEMP, M_WAITOK);
	error = SYSCTL_IN(req, buf, req->newlen);
	if (error != 0)
		goto out;

	buf[req->newlen] = '\0';

	ndflags = LOCKLEAF | NOFOLLOW | AUDITVNODE1;
	NDINIT(&nd, LOOKUP, ndflags, UIO_SYSSPACE, buf);
	if ((error = namei(&nd)) != 0)
		goto out;
	vp = nd.ni_vp;

	if (VN_IS_DOOMED(vp)) {
		/*
		 * This vnode is being recycled.  Return != 0 to let the caller
		 * know that the sysctl had no effect.  Return EAGAIN because a
		 * subsequent call will likely succeed (since namei will create
		 * a new vnode if necessary)
		 */
		error = EAGAIN;
		goto putvnode;
	}

	vgone(vp);
putvnode:
	vput(vp);
	NDFREE_PNBUF(&nd);
out:
	free(buf, M_TEMP);
	return (error);
}

static int
sysctl_ftry_reclaim_vnode(SYSCTL_HANDLER_ARGS)
{
	struct thread *td = curthread;
	struct vnode *vp;
	struct file *fp;
	int error;
	int fd;

	if (req->newptr == NULL)
		return (EBADF);

        error = sysctl_handle_int(oidp, &fd, 0, req);
        if (error != 0)
                return (error);
	error = getvnode(curthread, fd, &cap_fcntl_rights, &fp);
	if (error != 0)
		return (error);
	vp = fp->f_vnode;

	error = vn_lock(vp, LK_EXCLUSIVE);
	if (error != 0)
		goto drop;

	vgone(vp);
	VOP_UNLOCK(vp);
drop:
	fdrop(fp, td);
	return (error);
}

SYSCTL_PROC(_debug, OID_AUTO, try_reclaim_vnode,
    CTLTYPE_STRING | CTLFLAG_MPSAFE | CTLFLAG_WR, NULL, 0,
    sysctl_try_reclaim_vnode, "A", "Try to reclaim a vnode by its pathname");
SYSCTL_PROC(_debug, OID_AUTO, ftry_reclaim_vnode,
    CTLTYPE_INT | CTLFLAG_MPSAFE | CTLFLAG_WR, NULL, 0,
    sysctl_ftry_reclaim_vnode, "I",
    "Try to reclaim a vnode by its file descriptor");

/* Shift count for (uintptr_t)vp to initialize vp->v_hash. */
#define vnsz2log 8
#ifndef DEBUG_LOCKS
_Static_assert(sizeof(struct vnode) >= 1UL << vnsz2log &&
    sizeof(struct vnode) < 1UL << (vnsz2log + 1),
    "vnsz2log needs to be updated");
#endif

/*
 * Support for the bufobj clean & dirty pctrie.
 */
static void *
buf_trie_alloc(struct pctrie *ptree)
{
	return (uma_zalloc_smr(buf_trie_zone, M_NOWAIT));
}

static void
buf_trie_free(struct pctrie *ptree, void *node)
{
	uma_zfree_smr(buf_trie_zone, node);
}
PCTRIE_DEFINE_SMR(BUF, buf, b_lblkno, buf_trie_alloc, buf_trie_free,
    buf_trie_smr);

/*
 * Initialize the vnode management data structures.
 *
 * Reevaluate the following cap on the number of vnodes after the physical
 * memory size exceeds 512GB.  In the limit, as the physical memory size
 * grows, the ratio of the memory size in KB to vnodes approaches 64:1.
 */
#ifndef	MAXVNODES_MAX
#define	MAXVNODES_MAX	(512UL * 1024 * 1024 / 64)	/* 8M */
#endif

static MALLOC_DEFINE(M_VNODE_MARKER, "vnodemarker", "vnode marker");

static struct vnode *
vn_alloc_marker(struct mount *mp)
{
	struct vnode *vp;

	vp = malloc(sizeof(struct vnode), M_VNODE_MARKER, M_WAITOK | M_ZERO);
	vp->v_type = VMARKER;
	vp->v_mount = mp;

	return (vp);
}

static void
vn_free_marker(struct vnode *vp)
{

	MPASS(vp->v_type == VMARKER);
	free(vp, M_VNODE_MARKER);
}

#ifdef KASAN
static int
vnode_ctor(void *mem, int size, void *arg __unused, int flags __unused)
{
	kasan_mark(mem, size, roundup2(size, UMA_ALIGN_PTR + 1), 0);
	return (0);
}

static void
vnode_dtor(void *mem, int size, void *arg __unused)
{
	size_t end1, end2, off1, off2;

	_Static_assert(offsetof(struct vnode, v_vnodelist) <
	    offsetof(struct vnode, v_dbatchcpu),
	    "KASAN marks require updating");

	off1 = offsetof(struct vnode, v_vnodelist);
	off2 = offsetof(struct vnode, v_dbatchcpu);
	end1 = off1 + sizeof(((struct vnode *)NULL)->v_vnodelist);
	end2 = off2 + sizeof(((struct vnode *)NULL)->v_dbatchcpu);

	/*
	 * Access to the v_vnodelist and v_dbatchcpu fields are permitted even
	 * after the vnode has been freed.  Try to get some KASAN coverage by
	 * marking everything except those two fields as invalid.  Because
	 * KASAN's tracking is not byte-granular, any preceding fields sharing
	 * the same 8-byte aligned word must also be marked valid.
	 */

	/* Handle the area from the start until v_vnodelist... */
	off1 = rounddown2(off1, KASAN_SHADOW_SCALE);
	kasan_mark(mem, off1, off1, KASAN_UMA_FREED);

	/* ... then the area between v_vnodelist and v_dbatchcpu ... */
	off1 = roundup2(end1, KASAN_SHADOW_SCALE);
	off2 = rounddown2(off2, KASAN_SHADOW_SCALE);
	if (off2 > off1)
		kasan_mark((void *)((char *)mem + off1), off2 - off1,
		    off2 - off1, KASAN_UMA_FREED);

	/* ... and finally the area from v_dbatchcpu to the end. */
	off2 = roundup2(end2, KASAN_SHADOW_SCALE);
	kasan_mark((void *)((char *)mem + off2), size - off2, size - off2,
	    KASAN_UMA_FREED);
}
#endif /* KASAN */

/*
 * Initialize a vnode as it first enters the zone.
 */
static int
vnode_init(void *mem, int size, int flags)
{
	struct vnode *vp;

	vp = mem;
	bzero(vp, size);
	/*
	 * Setup locks.
	 */
	vp->v_vnlock = &vp->v_lock;
	mtx_init(&vp->v_interlock, "vnode interlock", NULL, MTX_DEF);
	/*
	 * By default, don't allow shared locks unless filesystems opt-in.
	 */
	lockinit(vp->v_vnlock, PVFS, "vnode", VLKTIMEOUT,
	    LK_NOSHARE | LK_IS_VNODE);
	/*
	 * Initialize bufobj.
	 */
	bufobj_init(&vp->v_bufobj, vp);
	/*
	 * Initialize namecache.
	 */
	cache_vnode_init(vp);
	/*
	 * Initialize rangelocks.
	 */
	rangelock_init(&vp->v_rl);

	vp->v_dbatchcpu = NOCPU;

	vp->v_state = VSTATE_DEAD;

	/*
	 * Check vhold_recycle_free for an explanation.
	 */
	vp->v_holdcnt = VHOLD_NO_SMR;
	vp->v_type = VNON;
	mtx_lock(&vnode_list_mtx);
	TAILQ_INSERT_BEFORE(vnode_list_free_marker, vp, v_vnodelist);
	mtx_unlock(&vnode_list_mtx);
	return (0);
}

/*
 * Free a vnode when it is cleared from the zone.
 */
static void
vnode_fini(void *mem, int size)
{
	struct vnode *vp;
	struct bufobj *bo;

	vp = mem;
	vdbatch_dequeue(vp);
	mtx_lock(&vnode_list_mtx);
	TAILQ_REMOVE(&vnode_list, vp, v_vnodelist);
	mtx_unlock(&vnode_list_mtx);
	rangelock_destroy(&vp->v_rl);
	lockdestroy(vp->v_vnlock);
	mtx_destroy(&vp->v_interlock);
	bo = &vp->v_bufobj;
	rw_destroy(BO_LOCKPTR(bo));

	kasan_mark(mem, size, size, 0);
}

/*
 * Provide the size of NFS nclnode and NFS fh for calculation of the
 * vnode memory consumption.  The size is specified directly to
 * eliminate dependency on NFS-private header.
 *
 * Other filesystems may use bigger or smaller (like UFS and ZFS)
 * private inode data, but the NFS-based estimation is ample enough.
 * Still, we care about differences in the size between 64- and 32-bit
 * platforms.
 *
 * Namecache structure size is heuristically
 * sizeof(struct namecache_ts) + CACHE_PATH_CUTOFF + 1.
 */
#ifdef _LP64
#define	NFS_NCLNODE_SZ	(528 + 64)
#define	NC_SZ		148
#else
#define	NFS_NCLNODE_SZ	(360 + 32)
#define	NC_SZ		92
#endif

static void
vntblinit(void *dummy __unused)
{
	struct vdbatch *vd;
	uma_ctor ctor;
	uma_dtor dtor;
	int cpu, physvnodes, virtvnodes;

	/*
	 * Desiredvnodes is a function of the physical memory size and the
	 * kernel's heap size.  Generally speaking, it scales with the
	 * physical memory size.  The ratio of desiredvnodes to the physical
	 * memory size is 1:16 until desiredvnodes exceeds 98,304.
	 * Thereafter, the
	 * marginal ratio of desiredvnodes to the physical memory size is
	 * 1:64.  However, desiredvnodes is limited by the kernel's heap
	 * size.  The memory required by desiredvnodes vnodes and vm objects
	 * must not exceed 1/10th of the kernel's heap size.
	 */
	physvnodes = maxproc + pgtok(vm_cnt.v_page_count) / 64 +
	    3 * min(98304 * 16, pgtok(vm_cnt.v_page_count)) / 64;
	virtvnodes = vm_kmem_size / (10 * (sizeof(struct vm_object) +
	    sizeof(struct vnode) + NC_SZ * ncsizefactor + NFS_NCLNODE_SZ));
	desiredvnodes = min(physvnodes, virtvnodes);
	if (desiredvnodes > MAXVNODES_MAX) {
		if (bootverbose)
			printf("Reducing kern.maxvnodes %lu -> %lu\n",
			    desiredvnodes, MAXVNODES_MAX);
		desiredvnodes = MAXVNODES_MAX;
	}
	wantfreevnodes = desiredvnodes / 4;
	mtx_init(&mntid_mtx, "mntid", NULL, MTX_DEF);
	TAILQ_INIT(&vnode_list);
	mtx_init(&vnode_list_mtx, "vnode_list", NULL, MTX_DEF);
	/*
	 * The lock is taken to appease WITNESS.
	 */
	mtx_lock(&vnode_list_mtx);
	vnlru_recalc();
	mtx_unlock(&vnode_list_mtx);
	vnode_list_free_marker = vn_alloc_marker(NULL);
	TAILQ_INSERT_HEAD(&vnode_list, vnode_list_free_marker, v_vnodelist);
	vnode_list_reclaim_marker = vn_alloc_marker(NULL);
	TAILQ_INSERT_HEAD(&vnode_list, vnode_list_reclaim_marker, v_vnodelist);

#ifdef KASAN
	ctor = vnode_ctor;
	dtor = vnode_dtor;
#else
	ctor = NULL;
	dtor = NULL;
#endif
	vnode_zone = uma_zcreate("VNODE", sizeof(struct vnode), ctor, dtor,
	    vnode_init, vnode_fini, UMA_ALIGN_PTR, UMA_ZONE_NOKASAN);
	uma_zone_set_smr(vnode_zone, vfs_smr);

	/*
	 * Preallocate enough nodes to support one-per buf so that
	 * we can not fail an insert.  reassignbuf() callers can not
	 * tolerate the insertion failure.
	 */
	buf_trie_zone = uma_zcreate("BUF TRIE", pctrie_node_size(),
	    NULL, NULL, pctrie_zone_init, NULL, UMA_ALIGN_PTR, 
	    UMA_ZONE_NOFREE | UMA_ZONE_SMR);
	buf_trie_smr = uma_zone_get_smr(buf_trie_zone);
	uma_prealloc(buf_trie_zone, nbuf);

	vnodes_created = counter_u64_alloc(M_WAITOK);
	direct_recycles_free_count = counter_u64_alloc(M_WAITOK);
	vnode_skipped_requeues = counter_u64_alloc(M_WAITOK);

	/*
	 * Initialize the filesystem syncer.
	 */
	syncer_workitem_pending = hashinit(syncer_maxdelay, M_VNODE,
	    &syncer_mask);
	syncer_maxdelay = syncer_mask + 1;
	mtx_init(&sync_mtx, "Syncer mtx", NULL, MTX_DEF);
	cv_init(&sync_wakeup, "syncer");

	CPU_FOREACH(cpu) {
		vd = DPCPU_ID_PTR((cpu), vd);
		bzero(vd, sizeof(*vd));
		mtx_init(&vd->lock, "vdbatch", NULL, MTX_DEF);
	}
}
SYSINIT(vfs, SI_SUB_VFS, SI_ORDER_FIRST, vntblinit, NULL);

/*
 * Mark a mount point as busy. Used to synchronize access and to delay
 * unmounting. Eventually, mountlist_mtx is not released on failure.
 *
 * vfs_busy() is a custom lock, it can block the caller.
 * vfs_busy() only sleeps if the unmount is active on the mount point.
 * For a mountpoint mp, vfs_busy-enforced lock is before lock of any
 * vnode belonging to mp.
 *
 * Lookup uses vfs_busy() to traverse mount points.
 * root fs			var fs
 * / vnode lock		A	/ vnode lock (/var)		D
 * /var vnode lock	B	/log vnode lock(/var/log)	E
 * vfs_busy lock	C	vfs_busy lock			F
 *
 * Within each file system, the lock order is C->A->B and F->D->E.
 *
 * When traversing across mounts, the system follows that lock order:
 *
 *        C->A->B
 *              |
 *              +->F->D->E
 *
 * The lookup() process for namei("/var") illustrates the process:
 *  1. VOP_LOOKUP() obtains B while A is held
 *  2. vfs_busy() obtains a shared lock on F while A and B are held
 *  3. vput() releases lock on B
 *  4. vput() releases lock on A
 *  5. VFS_ROOT() obtains lock on D while shared lock on F is held
 *  6. vfs_unbusy() releases shared lock on F
 *  7. vn_lock() obtains lock on deadfs vnode vp_crossmp instead of A.
 *     Attempt to lock A (instead of vp_crossmp) while D is held would
 *     violate the global order, causing deadlocks.
 *
 * dounmount() locks B while F is drained.  Note that for stacked
 * filesystems, D and B in the example above may be the same lock,
 * which introdues potential lock order reversal deadlock between
 * dounmount() and step 5 above.  These filesystems may avoid the LOR
 * by setting VV_CROSSLOCK on the covered vnode so that lock B will
 * remain held until after step 5.
 */
int
vfs_busy(struct mount *mp, int flags)
{
	struct mount_pcpu *mpcpu;

	MPASS((flags & ~MBF_MASK) == 0);
	CTR3(KTR_VFS, "%s: mp %p with flags %d", __func__, mp, flags);

	if (vfs_op_thread_enter(mp, mpcpu)) {
		MPASS((mp->mnt_kern_flag & MNTK_DRAINING) == 0);
		MPASS((mp->mnt_kern_flag & MNTK_UNMOUNT) == 0);
		MPASS((mp->mnt_kern_flag & MNTK_REFEXPIRE) == 0);
		vfs_mp_count_add_pcpu(mpcpu, ref, 1);
		vfs_mp_count_add_pcpu(mpcpu, lockref, 1);
		vfs_op_thread_exit(mp, mpcpu);
		if (flags & MBF_MNTLSTLOCK)
			mtx_unlock(&mountlist_mtx);
		return (0);
	}

	MNT_ILOCK(mp);
	vfs_assert_mount_counters(mp);
	MNT_REF(mp);
	/*
	 * If mount point is currently being unmounted, sleep until the
	 * mount point fate is decided.  If thread doing the unmounting fails,
	 * it will clear MNTK_UNMOUNT flag before waking us up, indicating
	 * that this mount point has survived the unmount attempt and vfs_busy
	 * should retry.  Otherwise the unmounter thread will set MNTK_REFEXPIRE
	 * flag in addition to MNTK_UNMOUNT, indicating that mount point is
	 * about to be really destroyed.  vfs_busy needs to release its
	 * reference on the mount point in this case and return with ENOENT,
	 * telling the caller the mount it tried to busy is no longer valid.
	 */
	while (mp->mnt_kern_flag & MNTK_UNMOUNT) {
		KASSERT(TAILQ_EMPTY(&mp->mnt_uppers),
		    ("%s: non-empty upper mount list with pending unmount",
		    __func__));
		if (flags & MBF_NOWAIT || mp->mnt_kern_flag & MNTK_REFEXPIRE) {
			MNT_REL(mp);
			MNT_IUNLOCK(mp);
			CTR1(KTR_VFS, "%s: failed busying before sleeping",
			    __func__);
			return (ENOENT);
		}
		if (flags & MBF_MNTLSTLOCK)
			mtx_unlock(&mountlist_mtx);
		mp->mnt_kern_flag |= MNTK_MWAIT;
		msleep(mp, MNT_MTX(mp), PVFS | PDROP, "vfs_busy", 0);
		if (flags & MBF_MNTLSTLOCK)
			mtx_lock(&mountlist_mtx);
		MNT_ILOCK(mp);
	}
	if (flags & MBF_MNTLSTLOCK)
		mtx_unlock(&mountlist_mtx);
	mp->mnt_lockref++;
	MNT_IUNLOCK(mp);
	return (0);
}

/*
 * Free a busy filesystem.
 */
void
vfs_unbusy(struct mount *mp)
{
	struct mount_pcpu *mpcpu;
	int c;

	CTR2(KTR_VFS, "%s: mp %p", __func__, mp);

	if (vfs_op_thread_enter(mp, mpcpu)) {
		MPASS((mp->mnt_kern_flag & MNTK_DRAINING) == 0);
		vfs_mp_count_sub_pcpu(mpcpu, lockref, 1);
		vfs_mp_count_sub_pcpu(mpcpu, ref, 1);
		vfs_op_thread_exit(mp, mpcpu);
		return;
	}

	MNT_ILOCK(mp);
	vfs_assert_mount_counters(mp);
	MNT_REL(mp);
	c = --mp->mnt_lockref;
	if (mp->mnt_vfs_ops == 0) {
		MPASS((mp->mnt_kern_flag & MNTK_DRAINING) == 0);
		MNT_IUNLOCK(mp);
		return;
	}
	if (c < 0)
		vfs_dump_mount_counters(mp);
	if (c == 0 && (mp->mnt_kern_flag & MNTK_DRAINING) != 0) {
		MPASS(mp->mnt_kern_flag & MNTK_UNMOUNT);
		CTR1(KTR_VFS, "%s: waking up waiters", __func__);
		mp->mnt_kern_flag &= ~MNTK_DRAINING;
		wakeup(&mp->mnt_lockref);
	}
	MNT_IUNLOCK(mp);
}

/*
 * Lookup a mount point by filesystem identifier.
 */
struct mount *
vfs_getvfs(fsid_t *fsid)
{
	struct mount *mp;

	CTR2(KTR_VFS, "%s: fsid %p", __func__, fsid);
	mtx_lock(&mountlist_mtx);
	TAILQ_FOREACH(mp, &mountlist, mnt_list) {
		if (fsidcmp(&mp->mnt_stat.f_fsid, fsid) == 0) {
			vfs_ref(mp);
			mtx_unlock(&mountlist_mtx);
			return (mp);
		}
	}
	mtx_unlock(&mountlist_mtx);
	CTR2(KTR_VFS, "%s: lookup failed for %p id", __func__, fsid);
	return ((struct mount *) 0);
}

/*
 * Lookup a mount point by filesystem identifier, busying it before
 * returning.
 *
 * To avoid congestion on mountlist_mtx, implement simple direct-mapped
 * cache for popular filesystem identifiers.  The cache is lockess, using
 * the fact that struct mount's are never freed.  In worst case we may
 * get pointer to unmounted or even different filesystem, so we have to
 * check what we got, and go slow way if so.
 */
struct mount *
vfs_busyfs(fsid_t *fsid)
{
#define	FSID_CACHE_SIZE	256
	typedef struct mount * volatile vmp_t;
	static vmp_t cache[FSID_CACHE_SIZE];
	struct mount *mp;
	int error;
	uint32_t hash;

	CTR2(KTR_VFS, "%s: fsid %p", __func__, fsid);
	hash = fsid->val[0] ^ fsid->val[1];
	hash = (hash >> 16 ^ hash) & (FSID_CACHE_SIZE - 1);
	mp = cache[hash];
	if (mp == NULL || fsidcmp(&mp->mnt_stat.f_fsid, fsid) != 0)
		goto slow;
	if (vfs_busy(mp, 0) != 0) {
		cache[hash] = NULL;
		goto slow;
	}
	if (fsidcmp(&mp->mnt_stat.f_fsid, fsid) == 0)
		return (mp);
	else
	    vfs_unbusy(mp);

slow:
	mtx_lock(&mountlist_mtx);
	TAILQ_FOREACH(mp, &mountlist, mnt_list) {
		if (fsidcmp(&mp->mnt_stat.f_fsid, fsid) == 0) {
			error = vfs_busy(mp, MBF_MNTLSTLOCK);
			if (error) {
				cache[hash] = NULL;
				mtx_unlock(&mountlist_mtx);
				return (NULL);
			}
			cache[hash] = mp;
			return (mp);
		}
	}
	CTR2(KTR_VFS, "%s: lookup failed for %p id", __func__, fsid);
	mtx_unlock(&mountlist_mtx);
	return ((struct mount *) 0);
}

/*
 * Check if a user can access privileged mount options.
 */
int
vfs_suser(struct mount *mp, struct thread *td)
{
	int error;

	if (jailed(td->td_ucred)) {
		/*
		 * If the jail of the calling thread lacks permission for
		 * this type of file system, deny immediately.
		 */
		if (!prison_allow(td->td_ucred, mp->mnt_vfc->vfc_prison_flag))
			return (EPERM);

		/*
		 * If the file system was mounted outside the jail of the
		 * calling thread, deny immediately.
		 */
		if (prison_check(td->td_ucred, mp->mnt_cred) != 0)
			return (EPERM);
	}

	/*
	 * If file system supports delegated administration, we don't check
	 * for the PRIV_VFS_MOUNT_OWNER privilege - it will be better verified
	 * by the file system itself.
	 * If this is not the user that did original mount, we check for
	 * the PRIV_VFS_MOUNT_OWNER privilege.
	 */
	if (!(mp->mnt_vfc->vfc_flags & VFCF_DELEGADMIN) &&
	    mp->mnt_cred->cr_uid != td->td_ucred->cr_uid) {
		if ((error = priv_check(td, PRIV_VFS_MOUNT_OWNER)) != 0)
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
vfs_getnewfsid(struct mount *mp)
{
	static uint16_t mntid_base;
	struct mount *nmp;
	fsid_t tfsid;
	int mtype;

	CTR2(KTR_VFS, "%s: mp %p", __func__, mp);
	mtx_lock(&mntid_mtx);
	mtype = mp->mnt_vfc->vfc_typenum;
	tfsid.val[1] = mtype;
	mtype = (mtype & 0xFF) << 24;
	for (;;) {
		tfsid.val[0] = makedev(255,
		    mtype | ((mntid_base & 0xFF00) << 8) | (mntid_base & 0xFF));
		mntid_base++;
		if ((nmp = vfs_getvfs(&tfsid)) == NULL)
			break;
		vfs_rel(nmp);
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

static int timestamp_precision = TSP_USEC;
SYSCTL_INT(_vfs, OID_AUTO, timestamp_precision, CTLFLAG_RW,
    &timestamp_precision, 0, "File timestamp precision (0: seconds, "
    "1: sec + ns accurate to 1/HZ, 2: sec + ns truncated to us, "
    "3+: sec + ns (max. precision))");

/*
 * Get a current timestamp.
 */
void
vfs_timestamp(struct timespec *tsp)
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
vattr_null(struct vattr *vap)
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
 * Try to reduce the total number of vnodes.
 *
 * This routine (and its user) are buggy in at least the following ways:
 * - all parameters were picked years ago when RAM sizes were significantly
 *   smaller
 * - it can pick vnodes based on pages used by the vm object, but filesystems
 *   like ZFS don't use it making the pick broken
 * - since ZFS has its own aging policy it gets partially combated by this one
 * - a dedicated method should be provided for filesystems to let them decide
 *   whether the vnode should be recycled
 *
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
 * desirable to reuse such vnodes.  These conditions may cause the
 * number of vnodes to reach some minimum value regardless of what
 * you set kern.maxvnodes to.  Do not set kern.maxvnodes too low.
 *
 * @param reclaim_nc_src Only reclaim directories with outgoing namecache
 * 			 entries if this argument is strue
 * @param trigger	 Only reclaim vnodes with fewer than this many resident
 *			 pages.
 * @param target	 How many vnodes to reclaim.
 * @return		 The number of vnodes that were reclaimed.
 */
static int
vlrureclaim(bool reclaim_nc_src, int trigger, u_long target)
{
	struct vnode *vp, *mvp;
	struct mount *mp;
	struct vm_object *object;
	u_long done;
	bool retried;

	mtx_assert(&vnode_list_mtx, MA_OWNED);

	retried = false;
	done = 0;

	mvp = vnode_list_reclaim_marker;
restart:
	vp = mvp;
	while (done < target) {
		vp = TAILQ_NEXT(vp, v_vnodelist);
		if (__predict_false(vp == NULL))
			break;

		if (__predict_false(vp->v_type == VMARKER))
			continue;

		/*
		 * If it's been deconstructed already, it's still
		 * referenced, or it exceeds the trigger, skip it.
		 * Also skip free vnodes.  We are trying to make space
		 * for more free vnodes, not reduce their count.
		 */
		if (vp->v_usecount > 0 || vp->v_holdcnt == 0 ||
		    (!reclaim_nc_src && !LIST_EMPTY(&vp->v_cache_src)))
			goto next_iter;

		if (vp->v_type == VBAD || vp->v_type == VNON)
			goto next_iter;

		object = atomic_load_ptr(&vp->v_object);
		if (object == NULL || object->resident_page_count > trigger) {
			goto next_iter;
		}

		/*
		 * Handle races against vnode allocation. Filesystems lock the
		 * vnode some time after it gets returned from getnewvnode,
		 * despite type and hold count being manipulated earlier.
		 * Resorting to checking v_mount restores guarantees present
		 * before the global list was reworked to contain all vnodes.
		 */
		if (!VI_TRYLOCK(vp))
			goto next_iter;
		if (__predict_false(vp->v_type == VBAD || vp->v_type == VNON)) {
			VI_UNLOCK(vp);
			goto next_iter;
		}
		if (vp->v_mount == NULL) {
			VI_UNLOCK(vp);
			goto next_iter;
		}
		vholdl(vp);
		VI_UNLOCK(vp);
		TAILQ_REMOVE(&vnode_list, mvp, v_vnodelist);
		TAILQ_INSERT_AFTER(&vnode_list, vp, mvp, v_vnodelist);
		mtx_unlock(&vnode_list_mtx);

		if (vn_start_write(vp, &mp, V_NOWAIT) != 0) {
			vdrop_recycle(vp);
			goto next_iter_unlocked;
		}
		if (VOP_LOCK(vp, LK_EXCLUSIVE|LK_NOWAIT) != 0) {
			vdrop_recycle(vp);
			vn_finished_write(mp);
			goto next_iter_unlocked;
		}

		VI_LOCK(vp);
		if (vp->v_usecount > 0 ||
		    (!reclaim_nc_src && !LIST_EMPTY(&vp->v_cache_src)) ||
		    (vp->v_object != NULL && vp->v_object->handle == vp &&
		    vp->v_object->resident_page_count > trigger)) {
			VOP_UNLOCK(vp);
			vdropl_recycle(vp);
			vn_finished_write(mp);
			goto next_iter_unlocked;
		}
		recycles_count++;
		vgonel(vp);
		VOP_UNLOCK(vp);
		vdropl_recycle(vp);
		vn_finished_write(mp);
		done++;
next_iter_unlocked:
		maybe_yield();
		mtx_lock(&vnode_list_mtx);
		goto restart;
next_iter:
		MPASS(vp->v_type != VMARKER);
		if (!should_yield())
			continue;
		TAILQ_REMOVE(&vnode_list, mvp, v_vnodelist);
		TAILQ_INSERT_AFTER(&vnode_list, vp, mvp, v_vnodelist);
		mtx_unlock(&vnode_list_mtx);
		kern_yield(PRI_USER);
		mtx_lock(&vnode_list_mtx);
		goto restart;
	}
	if (done == 0 && !retried) {
		TAILQ_REMOVE(&vnode_list, mvp, v_vnodelist);
		TAILQ_INSERT_HEAD(&vnode_list, mvp, v_vnodelist);
		retried = true;
		goto restart;
	}
	return (done);
}

static int max_free_per_call = 10000;
SYSCTL_INT(_debug, OID_AUTO, max_vnlru_free, CTLFLAG_RW, &max_free_per_call, 0,
    "limit on vnode free requests per call to the vnlru_free routine (legacy)");
SYSCTL_INT(_vfs_vnode_vnlru, OID_AUTO, max_free_per_call, CTLFLAG_RW,
    &max_free_per_call, 0,
    "limit on vnode free requests per call to the vnlru_free routine");

/*
 * Attempt to recycle requested amount of free vnodes.
 */
static int
vnlru_free_impl(int count, struct vfsops *mnt_op, struct vnode *mvp, bool isvnlru)
{
	struct vnode *vp;
	struct mount *mp;
	int ocount;
	bool retried;

	mtx_assert(&vnode_list_mtx, MA_OWNED);
	if (count > max_free_per_call)
		count = max_free_per_call;
	if (count == 0) {
		mtx_unlock(&vnode_list_mtx);
		return (0);
	}
	ocount = count;
	retried = false;
	vp = mvp;
	for (;;) {
		vp = TAILQ_NEXT(vp, v_vnodelist);
		if (__predict_false(vp == NULL)) {
			/*
			 * The free vnode marker can be past eligible vnodes:
			 * 1. if vdbatch_process trylock failed
			 * 2. if vtryrecycle failed
			 *
			 * If so, start the scan from scratch.
			 */
			if (!retried && vnlru_read_freevnodes() > 0) {
				TAILQ_REMOVE(&vnode_list, mvp, v_vnodelist);
				TAILQ_INSERT_HEAD(&vnode_list, mvp, v_vnodelist);
				vp = mvp;
				retried = true;
				continue;
			}

			/*
			 * Give up
			 */
			TAILQ_REMOVE(&vnode_list, mvp, v_vnodelist);
			TAILQ_INSERT_TAIL(&vnode_list, mvp, v_vnodelist);
			mtx_unlock(&vnode_list_mtx);
			break;
		}
		if (__predict_false(vp->v_type == VMARKER))
			continue;
		if (vp->v_holdcnt > 0)
			continue;
		/*
		 * Don't recycle if our vnode is from different type
		 * of mount point.  Note that mp is type-safe, the
		 * check does not reach unmapped address even if
		 * vnode is reclaimed.
		 */
		if (mnt_op != NULL && (mp = vp->v_mount) != NULL &&
		    mp->mnt_op != mnt_op) {
			continue;
		}
		if (__predict_false(vp->v_type == VBAD || vp->v_type == VNON)) {
			continue;
		}
		if (!vhold_recycle_free(vp))
			continue;
		TAILQ_REMOVE(&vnode_list, mvp, v_vnodelist);
		TAILQ_INSERT_AFTER(&vnode_list, vp, mvp, v_vnodelist);
		mtx_unlock(&vnode_list_mtx);
		/*
		 * FIXME: ignores the return value, meaning it may be nothing
		 * got recycled but it claims otherwise to the caller.
		 *
		 * Originally the value started being ignored in 2005 with
		 * 114a1006a8204aa156e1f9ad6476cdff89cada7f .
		 *
		 * Respecting the value can run into significant stalls if most
		 * vnodes belong to one file system and it has writes
		 * suspended.  In presence of many threads and millions of
		 * vnodes they keep contending on the vnode_list_mtx lock only
		 * to find vnodes they can't recycle.
		 *
		 * The solution would be to pre-check if the vnode is likely to
		 * be recycle-able, but it needs to happen with the
		 * vnode_list_mtx lock held. This runs into a problem where
		 * VOP_GETWRITEMOUNT (currently needed to find out about if
		 * writes are frozen) can take locks which LOR against it.
		 *
		 * Check nullfs for one example (null_getwritemount).
		 */
		vtryrecycle(vp, isvnlru);
		count--;
		if (count == 0) {
			break;
		}
		mtx_lock(&vnode_list_mtx);
		vp = mvp;
	}
	mtx_assert(&vnode_list_mtx, MA_NOTOWNED);
	return (ocount - count);
}

/*
 * XXX: returns without vnode_list_mtx locked!
 */
static int
vnlru_free_locked_direct(int count)
{
	int ret;

	mtx_assert(&vnode_list_mtx, MA_OWNED);
	ret = vnlru_free_impl(count, NULL, vnode_list_free_marker, false);
	mtx_assert(&vnode_list_mtx, MA_NOTOWNED);
	return (ret);
}

static int
vnlru_free_locked_vnlru(int count)
{
	int ret;

	mtx_assert(&vnode_list_mtx, MA_OWNED);
	ret = vnlru_free_impl(count, NULL, vnode_list_free_marker, true);
	mtx_assert(&vnode_list_mtx, MA_NOTOWNED);
	return (ret);
}

static int
vnlru_free_vnlru(int count)
{

	mtx_lock(&vnode_list_mtx);
	return (vnlru_free_locked_vnlru(count));
}

void
vnlru_free_vfsops(int count, struct vfsops *mnt_op, struct vnode *mvp)
{

	MPASS(mnt_op != NULL);
	MPASS(mvp != NULL);
	VNPASS(mvp->v_type == VMARKER, mvp);
	mtx_lock(&vnode_list_mtx);
	vnlru_free_impl(count, mnt_op, mvp, true);
	mtx_assert(&vnode_list_mtx, MA_NOTOWNED);
}

struct vnode *
vnlru_alloc_marker(void)
{
	struct vnode *mvp;

	mvp = vn_alloc_marker(NULL);
	mtx_lock(&vnode_list_mtx);
	TAILQ_INSERT_BEFORE(vnode_list_free_marker, mvp, v_vnodelist);
	mtx_unlock(&vnode_list_mtx);
	return (mvp);
}

void
vnlru_free_marker(struct vnode *mvp)
{
	mtx_lock(&vnode_list_mtx);
	TAILQ_REMOVE(&vnode_list, mvp, v_vnodelist);
	mtx_unlock(&vnode_list_mtx);
	vn_free_marker(mvp);
}

static void
vnlru_recalc(void)
{

	mtx_assert(&vnode_list_mtx, MA_OWNED);
	gapvnodes = imax(desiredvnodes - wantfreevnodes, 100);
	vhiwat = gapvnodes / 11; /* 9% -- just under the 10% in vlrureclaim() */
	vlowat = vhiwat / 2;
}

/*
 * Attempt to recycle vnodes in a context that is always safe to block.
 * Calling vlrurecycle() from the bowels of filesystem code has some
 * interesting deadlock problems.
 */
static struct proc *vnlruproc;
static int vnlruproc_sig;
static u_long vnlruproc_kicks;

SYSCTL_ULONG(_vfs_vnode_vnlru, OID_AUTO, kicks, CTLFLAG_RD, &vnlruproc_kicks, 0,
    "Number of times vnlru awakened due to vnode shortage");

#define VNLRU_COUNT_SLOP 100

/*
 * The main freevnodes counter is only updated when a counter local to CPU
 * diverges from 0 by more than VNLRU_FREEVNODES_SLOP. CPUs are conditionally
 * walked to compute a more accurate total.
 *
 * Note: the actual value at any given moment can still exceed slop, but it
 * should not be by significant margin in practice.
 */
#define VNLRU_FREEVNODES_SLOP 126

static void __noinline
vfs_freevnodes_rollup(int8_t *lfreevnodes)
{

	atomic_add_long(&freevnodes, *lfreevnodes);
	*lfreevnodes = 0;
	critical_exit();
}

static __inline void
vfs_freevnodes_inc(void)
{
	int8_t *lfreevnodes;

	critical_enter();
	lfreevnodes = PCPU_PTR(vfs_freevnodes);
	(*lfreevnodes)++;
	if (__predict_false(*lfreevnodes == VNLRU_FREEVNODES_SLOP))
		vfs_freevnodes_rollup(lfreevnodes);
	else
		critical_exit();
}

static __inline void
vfs_freevnodes_dec(void)
{
	int8_t *lfreevnodes;

	critical_enter();
	lfreevnodes = PCPU_PTR(vfs_freevnodes);
	(*lfreevnodes)--;
	if (__predict_false(*lfreevnodes == -VNLRU_FREEVNODES_SLOP))
		vfs_freevnodes_rollup(lfreevnodes);
	else
		critical_exit();
}

static u_long
vnlru_read_freevnodes(void)
{
	long slop, rfreevnodes, rfreevnodes_old;
	int cpu;

	rfreevnodes = atomic_load_long(&freevnodes);
	rfreevnodes_old = atomic_load_long(&freevnodes_old);

	if (rfreevnodes > rfreevnodes_old)
		slop = rfreevnodes - rfreevnodes_old;
	else
		slop = rfreevnodes_old - rfreevnodes;
	if (slop < VNLRU_FREEVNODES_SLOP)
		return (rfreevnodes >= 0 ? rfreevnodes : 0);
	CPU_FOREACH(cpu) {
		rfreevnodes += cpuid_to_pcpu[cpu]->pc_vfs_freevnodes;
	}
	atomic_store_long(&freevnodes_old, rfreevnodes);
	return (freevnodes_old >= 0 ? freevnodes_old : 0);
}

static bool
vnlru_under(u_long rnumvnodes, u_long limit)
{
	u_long rfreevnodes, space;

	if (__predict_false(rnumvnodes > desiredvnodes))
		return (true);

	space = desiredvnodes - rnumvnodes;
	if (space < limit) {
		rfreevnodes = vnlru_read_freevnodes();
		if (rfreevnodes > wantfreevnodes)
			space += rfreevnodes - wantfreevnodes;
	}
	return (space < limit);
}

static void
vnlru_kick_locked(void)
{

	mtx_assert(&vnode_list_mtx, MA_OWNED);
	if (vnlruproc_sig == 0) {
		vnlruproc_sig = 1;
		vnlruproc_kicks++;
		wakeup(vnlruproc);
	}
}

static void
vnlru_kick_cond(void)
{

	if (vnlru_read_freevnodes() > wantfreevnodes)
		return;

	if (vnlruproc_sig)
		return;
	mtx_lock(&vnode_list_mtx);
	vnlru_kick_locked();
	mtx_unlock(&vnode_list_mtx);
}

static void
vnlru_proc_sleep(void)
{

	if (vnlruproc_sig) {
		vnlruproc_sig = 0;
		wakeup(&vnlruproc_sig);
	}
	msleep(vnlruproc, &vnode_list_mtx, PVFS|PDROP, "vlruwt", hz);
}

/*
 * A lighter version of the machinery below.
 *
 * Tries to reach goals only by recycling free vnodes and does not invoke
 * uma_reclaim(UMA_RECLAIM_DRAIN).
 *
 * This works around pathological behavior in vnlru in presence of tons of free
 * vnodes, but without having to rewrite the machinery at this time. Said
 * behavior boils down to continuously trying to reclaim all kinds of vnodes
 * (cycling through all levels of "force") when the count is transiently above
 * limit. This happens a lot when all vnodes are used up and vn_alloc
 * speculatively increments the counter.
 *
 * Sample testcase: vnode limit 8388608, 20 separate directory trees each with
 * 1 million files in total and 20 find(1) processes stating them in parallel
 * (one per each tree).
 *
 * On a kernel with only stock machinery this needs anywhere between 60 and 120
 * seconds to execute (time varies *wildly* between runs). With the workaround
 * it consistently stays around 20 seconds [it got further down with later
 * changes].
 *
 * That is to say the entire thing needs a fundamental redesign (most notably
 * to accommodate faster recycling), the above only tries to get it ouf the way.
 *
 * Return values are:
 * -1 -- fallback to regular vnlru loop
 *  0 -- do nothing, go to sleep
 * >0 -- recycle this many vnodes
 */
static long
vnlru_proc_light_pick(void)
{
	u_long rnumvnodes, rfreevnodes;

	if (vstir || vnlruproc_sig == 1)
		return (-1);

	rnumvnodes = atomic_load_long(&numvnodes);
	rfreevnodes = vnlru_read_freevnodes();

	/*
	 * vnode limit might have changed and now we may be at a significant
	 * excess. Bail if we can't sort it out with free vnodes.
	 *
	 * Due to atomic updates the count can legitimately go above
	 * the limit for a short period, don't bother doing anything in
	 * that case.
	 */
	if (rnumvnodes > desiredvnodes + VNLRU_COUNT_SLOP + 10) {
		if (rnumvnodes - rfreevnodes >= desiredvnodes ||
		    rfreevnodes <= wantfreevnodes) {
			return (-1);
		}

		return (rnumvnodes - desiredvnodes);
	}

	/*
	 * Don't try to reach wantfreevnodes target if there are too few vnodes
	 * to begin with.
	 */
	if (rnumvnodes < wantfreevnodes) {
		return (0);
	}

	if (rfreevnodes < wantfreevnodes) {
		return (-1);
	}

	return (0);
}

static bool
vnlru_proc_light(void)
{
	long freecount;

	mtx_assert(&vnode_list_mtx, MA_NOTOWNED);

	freecount = vnlru_proc_light_pick();
	if (freecount == -1)
		return (false);

	if (freecount != 0) {
		vnlru_free_vnlru(freecount);
	}

	mtx_lock(&vnode_list_mtx);
	vnlru_proc_sleep();
	mtx_assert(&vnode_list_mtx, MA_NOTOWNED);
	return (true);
}

static u_long uma_reclaim_calls;
SYSCTL_ULONG(_vfs_vnode_vnlru, OID_AUTO, uma_reclaim_calls, CTLFLAG_RD | CTLFLAG_STATS,
    &uma_reclaim_calls, 0, "Number of calls to uma_reclaim");

static void
vnlru_proc(void)
{
	u_long rnumvnodes, rfreevnodes, target;
	unsigned long onumvnodes;
	int done, force, trigger, usevnodes;
	bool reclaim_nc_src, want_reread;

	EVENTHANDLER_REGISTER(shutdown_pre_sync, kproc_shutdown, vnlruproc,
	    SHUTDOWN_PRI_FIRST);

	force = 0;
	want_reread = false;
	for (;;) {
		kproc_suspend_check(vnlruproc);

		if (force == 0 && vnlru_proc_light())
			continue;

		mtx_lock(&vnode_list_mtx);
		rnumvnodes = atomic_load_long(&numvnodes);

		if (want_reread) {
			force = vnlru_under(numvnodes, vhiwat) ? 1 : 0;
			want_reread = false;
		}

		/*
		 * If numvnodes is too large (due to desiredvnodes being
		 * adjusted using its sysctl, or emergency growth), first
		 * try to reduce it by discarding free vnodes.
		 */
		if (rnumvnodes > desiredvnodes + 10) {
			vnlru_free_locked_vnlru(rnumvnodes - desiredvnodes);
			mtx_lock(&vnode_list_mtx);
			rnumvnodes = atomic_load_long(&numvnodes);
		}
		/*
		 * Sleep if the vnode cache is in a good state.  This is
		 * when it is not over-full and has space for about a 4%
		 * or 9% expansion (by growing its size or inexcessively
		 * reducing free vnode count).  Otherwise, try to reclaim
		 * space for a 10% expansion.
		 */
		if (vstir && force == 0) {
			force = 1;
			vstir = false;
		}
		if (force == 0 && !vnlru_under(rnumvnodes, vlowat)) {
			vnlru_proc_sleep();
			continue;
		}
		rfreevnodes = vnlru_read_freevnodes();

		onumvnodes = rnumvnodes;
		/*
		 * Calculate parameters for recycling.  These are the same
		 * throughout the loop to give some semblance of fairness.
		 * The trigger point is to avoid recycling vnodes with lots
		 * of resident pages.  We aren't trying to free memory; we
		 * are trying to recycle or at least free vnodes.
		 */
		if (rnumvnodes <= desiredvnodes)
			usevnodes = rnumvnodes - rfreevnodes;
		else
			usevnodes = rnumvnodes;
		if (usevnodes <= 0)
			usevnodes = 1;
		/*
		 * The trigger value is chosen to give a conservatively
		 * large value to ensure that it alone doesn't prevent
		 * making progress.  The value can easily be so large that
		 * it is effectively infinite in some congested and
		 * misconfigured cases, and this is necessary.  Normally
		 * it is about 8 to 100 (pages), which is quite large.
		 */
		trigger = vm_cnt.v_page_count * 2 / usevnodes;
		if (force < 2)
			trigger = vsmalltrigger;
		reclaim_nc_src = force >= 3;
		target = rnumvnodes * (int64_t)gapvnodes / imax(desiredvnodes, 1);
		target = target / 10 + 1;
		done = vlrureclaim(reclaim_nc_src, trigger, target);
		mtx_unlock(&vnode_list_mtx);
		/*
		 * Total number of vnodes can transiently go slightly above the
		 * limit (see vn_alloc_hard), no need to call uma_reclaim if
		 * this happens.
		 */
		if (onumvnodes + VNLRU_COUNT_SLOP + 1000 > desiredvnodes &&
		    numvnodes <= desiredvnodes) {
			uma_reclaim_calls++;
			uma_reclaim(UMA_RECLAIM_DRAIN);
		}
		if (done == 0) {
			if (force == 0 || force == 1) {
				force = 2;
				continue;
			}
			if (force == 2) {
				force = 3;
				continue;
			}
			want_reread = true;
			force = 0;
			vnlru_nowhere++;
			tsleep(vnlruproc, PPAUSE, "vlrup", hz * 3);
		} else {
			want_reread = true;
			kern_yield(PRI_USER);
		}
	}
}

static struct kproc_desc vnlru_kp = {
	"vnlru",
	vnlru_proc,
	&vnlruproc
};
SYSINIT(vnlru, SI_SUB_KTHREAD_UPDATE, SI_ORDER_FIRST, kproc_start,
    &vnlru_kp);

/*
 * Routines having to do with the management of the vnode table.
 */

/*
 * Try to recycle a freed vnode.
 */
static int
vtryrecycle(struct vnode *vp, bool isvnlru)
{
	struct mount *vnmp;

	CTR2(KTR_VFS, "%s: vp %p", __func__, vp);
	VNPASS(vp->v_holdcnt > 0, vp);
	/*
	 * This vnode may found and locked via some other list, if so we
	 * can't recycle it yet.
	 */
	if (VOP_LOCK(vp, LK_EXCLUSIVE | LK_NOWAIT) != 0) {
		CTR2(KTR_VFS,
		    "%s: impossible to recycle, vp %p lock is already held",
		    __func__, vp);
		vdrop_recycle(vp);
		return (EWOULDBLOCK);
	}
	/*
	 * Don't recycle if its filesystem is being suspended.
	 */
	if (vn_start_write(vp, &vnmp, V_NOWAIT) != 0) {
		VOP_UNLOCK(vp);
		CTR2(KTR_VFS,
		    "%s: impossible to recycle, cannot start the write for %p",
		    __func__, vp);
		vdrop_recycle(vp);
		return (EBUSY);
	}
	/*
	 * If we got this far, we need to acquire the interlock and see if
	 * anyone picked up this vnode from another list.  If not, we will
	 * mark it with DOOMED via vgonel() so that anyone who does find it
	 * will skip over it.
	 */
	VI_LOCK(vp);
	if (vp->v_usecount) {
		VOP_UNLOCK(vp);
		vdropl_recycle(vp);
		vn_finished_write(vnmp);
		CTR2(KTR_VFS,
		    "%s: impossible to recycle, %p is already referenced",
		    __func__, vp);
		return (EBUSY);
	}
	if (!VN_IS_DOOMED(vp)) {
		if (isvnlru)
			recycles_free_count++;
		else
			counter_u64_add(direct_recycles_free_count, 1);
		vgonel(vp);
	}
	VOP_UNLOCK(vp);
	vdropl_recycle(vp);
	vn_finished_write(vnmp);
	return (0);
}

/*
 * Allocate a new vnode.
 *
 * The operation never returns an error. Returning an error was disabled
 * in r145385 (dated 2005) with the following comment:
 *
 * XXX Not all VFS_VGET/ffs_vget callers check returns.
 *
 * Given the age of this commit (almost 15 years at the time of writing this
 * comment) restoring the ability to fail requires a significant audit of
 * all codepaths.
 *
 * The routine can try to free a vnode or stall for up to 1 second waiting for
 * vnlru to clear things up, but ultimately always performs a M_WAITOK allocation.
 */
static u_long vn_alloc_cyclecount;
static u_long vn_alloc_sleeps;

SYSCTL_ULONG(_vfs_vnode_stats, OID_AUTO, alloc_sleeps, CTLFLAG_RD, &vn_alloc_sleeps, 0,
    "Number of times vnode allocation blocked waiting on vnlru");

static struct vnode * __noinline
vn_alloc_hard(struct mount *mp, u_long rnumvnodes, bool bumped)
{
	u_long rfreevnodes;

	if (bumped) {
		if (rnumvnodes > desiredvnodes + VNLRU_COUNT_SLOP) {
			atomic_subtract_long(&numvnodes, 1);
			bumped = false;
		}
	}

	mtx_lock(&vnode_list_mtx);

	if (vn_alloc_cyclecount != 0) {
		rnumvnodes = atomic_load_long(&numvnodes);
		if (rnumvnodes + 1 < desiredvnodes) {
			vn_alloc_cyclecount = 0;
			mtx_unlock(&vnode_list_mtx);
			goto alloc;
		}

		rfreevnodes = vnlru_read_freevnodes();
		if (rfreevnodes < wantfreevnodes) {
			if (vn_alloc_cyclecount++ >= rfreevnodes) {
				vn_alloc_cyclecount = 0;
				vstir = true;
			}
		} else {
			vn_alloc_cyclecount = 0;
		}
	}

	/*
	 * Grow the vnode cache if it will not be above its target max after
	 * growing.  Otherwise, if there is at least one free vnode, try to
	 * reclaim 1 item from it before growing the cache (possibly above its
	 * target max if the reclamation failed or is delayed).
	 */
	if (vnlru_free_locked_direct(1) > 0)
		goto alloc;
	mtx_assert(&vnode_list_mtx, MA_NOTOWNED);
	if (mp == NULL || (mp->mnt_kern_flag & MNTK_SUSPEND) == 0) {
		/*
		 * Wait for space for a new vnode.
		 */
		if (bumped) {
			atomic_subtract_long(&numvnodes, 1);
			bumped = false;
		}
		mtx_lock(&vnode_list_mtx);
		vnlru_kick_locked();
		vn_alloc_sleeps++;
		msleep(&vnlruproc_sig, &vnode_list_mtx, PVFS, "vlruwk", hz);
		if (atomic_load_long(&numvnodes) + 1 > desiredvnodes &&
		    vnlru_read_freevnodes() > 1)
			vnlru_free_locked_direct(1);
		else
			mtx_unlock(&vnode_list_mtx);
	}
alloc:
	mtx_assert(&vnode_list_mtx, MA_NOTOWNED);
	if (!bumped)
		atomic_add_long(&numvnodes, 1);
	vnlru_kick_cond();
	return (uma_zalloc_smr(vnode_zone, M_WAITOK));
}

static struct vnode *
vn_alloc(struct mount *mp)
{
	u_long rnumvnodes;

	if (__predict_false(vn_alloc_cyclecount != 0))
		return (vn_alloc_hard(mp, 0, false));
	rnumvnodes = atomic_fetchadd_long(&numvnodes, 1) + 1;
	if (__predict_false(vnlru_under(rnumvnodes, vlowat))) {
		return (vn_alloc_hard(mp, rnumvnodes, true));
	}

	return (uma_zalloc_smr(vnode_zone, M_WAITOK));
}

static void
vn_free(struct vnode *vp)
{

	atomic_subtract_long(&numvnodes, 1);
	uma_zfree_smr(vnode_zone, vp);
}

/*
 * Allocate a new vnode.
 */
int
getnewvnode(const char *tag, struct mount *mp, struct vop_vector *vops,
    struct vnode **vpp)
{
	struct vnode *vp;
	struct thread *td;
	struct lock_object *lo;

	CTR3(KTR_VFS, "%s: mp %p with tag %s", __func__, mp, tag);

	KASSERT(vops->registered,
	    ("%s: not registered vector op %p\n", __func__, vops));
	cache_validate_vop_vector(mp, vops);

	td = curthread;
	if (td->td_vp_reserved != NULL) {
		vp = td->td_vp_reserved;
		td->td_vp_reserved = NULL;
	} else {
		vp = vn_alloc(mp);
	}
	counter_u64_add(vnodes_created, 1);

	vn_set_state(vp, VSTATE_UNINITIALIZED);

	/*
	 * Locks are given the generic name "vnode" when created.
	 * Follow the historic practice of using the filesystem
	 * name when they allocated, e.g., "zfs", "ufs", "nfs, etc.
	 *
	 * Locks live in a witness group keyed on their name. Thus,
	 * when a lock is renamed, it must also move from the witness
	 * group of its old name to the witness group of its new name.
	 *
	 * The change only needs to be made when the vnode moves
	 * from one filesystem type to another. We ensure that each
	 * filesystem use a single static name pointer for its tag so
	 * that we can compare pointers rather than doing a strcmp().
	 */
	lo = &vp->v_vnlock->lock_object;
#ifdef WITNESS
	if (lo->lo_name != tag) {
#endif
		lo->lo_name = tag;
#ifdef WITNESS
		WITNESS_DESTROY(lo);
		WITNESS_INIT(lo, tag);
	}
#endif
	/*
	 * By default, don't allow shared locks unless filesystems opt-in.
	 */
	vp->v_vnlock->lock_object.lo_flags |= LK_NOSHARE;
	/*
	 * Finalize various vnode identity bits.
	 */
	KASSERT(vp->v_object == NULL, ("stale v_object %p", vp));
	KASSERT(vp->v_lockf == NULL, ("stale v_lockf %p", vp));
	KASSERT(vp->v_pollinfo == NULL, ("stale v_pollinfo %p", vp));
	vp->v_type = VNON;
	vp->v_op = vops;
	vp->v_irflag = 0;
	v_init_counters(vp);
	vn_seqc_init(vp);
	vp->v_bufobj.bo_ops = &buf_ops_bio;
#ifdef DIAGNOSTIC
	if (mp == NULL && vops != &dead_vnodeops)
		printf("NULL mp in getnewvnode(9), tag %s\n", tag);
#endif
#ifdef MAC
	mac_vnode_init(vp);
	if (mp != NULL && (mp->mnt_flag & MNT_MULTILABEL) == 0)
		mac_vnode_associate_singlelabel(mp, vp);
#endif
	if (mp != NULL) {
		vp->v_bufobj.bo_bsize = mp->mnt_stat.f_iosize;
	}

	/*
	 * For the filesystems which do not use vfs_hash_insert(),
	 * still initialize v_hash to have vfs_hash_index() useful.
	 * E.g., nullfs uses vfs_hash_index() on the lower vnode for
	 * its own hashing.
	 */
	vp->v_hash = (uintptr_t)vp >> vnsz2log;

	*vpp = vp;
	return (0);
}

void
getnewvnode_reserve(void)
{
	struct thread *td;

	td = curthread;
	MPASS(td->td_vp_reserved == NULL);
	td->td_vp_reserved = vn_alloc(NULL);
}

void
getnewvnode_drop_reserve(void)
{
	struct thread *td;

	td = curthread;
	if (td->td_vp_reserved != NULL) {
		vn_free(td->td_vp_reserved);
		td->td_vp_reserved = NULL;
	}
}

static void __noinline
freevnode(struct vnode *vp)
{
	struct bufobj *bo;

	/*
	 * The vnode has been marked for destruction, so free it.
	 *
	 * The vnode will be returned to the zone where it will
	 * normally remain until it is needed for another vnode. We
	 * need to cleanup (or verify that the cleanup has already
	 * been done) any residual data left from its current use
	 * so as not to contaminate the freshly allocated vnode.
	 */
	CTR2(KTR_VFS, "%s: destroying the vnode %p", __func__, vp);
	/*
	 * Paired with vgone.
	 */
	vn_seqc_write_end_free(vp);

	bo = &vp->v_bufobj;
	VNASSERT(vp->v_data == NULL, vp, ("cleaned vnode isn't"));
	VNPASS(vp->v_holdcnt == VHOLD_NO_SMR, vp);
	VNASSERT(vp->v_usecount == 0, vp, ("Non-zero use count"));
	VNASSERT(vp->v_writecount == 0, vp, ("Non-zero write count"));
	VNASSERT(bo->bo_numoutput == 0, vp, ("Clean vnode has pending I/O's"));
	VNASSERT(bo->bo_clean.bv_cnt == 0, vp, ("cleanbufcnt not 0"));
	VNASSERT(pctrie_is_empty(&bo->bo_clean.bv_root), vp,
	    ("clean blk trie not empty"));
	VNASSERT(bo->bo_dirty.bv_cnt == 0, vp, ("dirtybufcnt not 0"));
	VNASSERT(pctrie_is_empty(&bo->bo_dirty.bv_root), vp,
	    ("dirty blk trie not empty"));
	VNASSERT(TAILQ_EMPTY(&vp->v_rl.rl_waiters), vp,
	    ("Dangling rangelock waiters"));
	VNASSERT((vp->v_iflag & (VI_DOINGINACT | VI_OWEINACT)) == 0, vp,
	    ("Leaked inactivation"));
	VI_UNLOCK(vp);
	cache_assert_no_entries(vp);

#ifdef MAC
	mac_vnode_destroy(vp);
#endif
	if (vp->v_pollinfo != NULL) {
		/*
		 * Use LK_NOWAIT to shut up witness about the lock. We may get
		 * here while having another vnode locked when trying to
		 * satisfy a lookup and needing to recycle.
		 */
		VOP_LOCK(vp, LK_EXCLUSIVE | LK_NOWAIT);
		destroy_vpollinfo(vp->v_pollinfo);
		VOP_UNLOCK(vp);
		vp->v_pollinfo = NULL;
	}
	vp->v_mountedhere = NULL;
	vp->v_unpcb = NULL;
	vp->v_rdev = NULL;
	vp->v_fifoinfo = NULL;
	vp->v_iflag = 0;
	vp->v_vflag = 0;
	bo->bo_flag = 0;
	vn_free(vp);
}

/*
 * Delete from old mount point vnode list, if on one.
 */
static void
delmntque(struct vnode *vp)
{
	struct mount *mp;

	VNPASS((vp->v_mflag & VMP_LAZYLIST) == 0, vp);

	mp = vp->v_mount;
	MNT_ILOCK(mp);
	VI_LOCK(vp);
	vp->v_mount = NULL;
	VNASSERT(mp->mnt_nvnodelistsize > 0, vp,
		("bad mount point vnode list size"));
	TAILQ_REMOVE(&mp->mnt_nvnodelist, vp, v_nmntvnodes);
	mp->mnt_nvnodelistsize--;
	MNT_REL(mp);
	MNT_IUNLOCK(mp);
	/*
	 * The caller expects the interlock to be still held.
	 */
	ASSERT_VI_LOCKED(vp, __func__);
}

static int
insmntque1_int(struct vnode *vp, struct mount *mp, bool dtr)
{

	KASSERT(vp->v_mount == NULL,
		("insmntque: vnode already on per mount vnode list"));
	VNASSERT(mp != NULL, vp, ("Don't call insmntque(foo, NULL)"));
	if ((mp->mnt_kern_flag & MNTK_UNLOCKED_INSMNTQUE) == 0) {
		ASSERT_VOP_ELOCKED(vp, "insmntque: non-locked vp");
	} else {
		KASSERT(!dtr,
		    ("%s: can't have MNTK_UNLOCKED_INSMNTQUE and cleanup",
		    __func__));
	}

	/*
	 * We acquire the vnode interlock early to ensure that the
	 * vnode cannot be recycled by another process releasing a
	 * holdcnt on it before we get it on both the vnode list
	 * and the active vnode list. The mount mutex protects only
	 * manipulation of the vnode list and the vnode freelist
	 * mutex protects only manipulation of the active vnode list.
	 * Hence the need to hold the vnode interlock throughout.
	 */
	MNT_ILOCK(mp);
	VI_LOCK(vp);
	if (((mp->mnt_kern_flag & MNTK_UNMOUNT) != 0 &&
	    ((mp->mnt_kern_flag & MNTK_UNMOUNTF) != 0 ||
	    mp->mnt_nvnodelistsize == 0)) &&
	    (vp->v_vflag & VV_FORCEINSMQ) == 0) {
		VI_UNLOCK(vp);
		MNT_IUNLOCK(mp);
		if (dtr) {
			vp->v_data = NULL;
			vp->v_op = &dead_vnodeops;
			vgone(vp);
			vput(vp);
		}
		return (EBUSY);
	}
	vp->v_mount = mp;
	MNT_REF(mp);
	TAILQ_INSERT_TAIL(&mp->mnt_nvnodelist, vp, v_nmntvnodes);
	VNASSERT(mp->mnt_nvnodelistsize >= 0, vp,
		("neg mount point vnode list size"));
	mp->mnt_nvnodelistsize++;
	VI_UNLOCK(vp);
	MNT_IUNLOCK(mp);
	return (0);
}

/*
 * Insert into list of vnodes for the new mount point, if available.
 * insmntque() reclaims the vnode on insertion failure, insmntque1()
 * leaves handling of the vnode to the caller.
 */
int
insmntque(struct vnode *vp, struct mount *mp)
{
	return (insmntque1_int(vp, mp, true));
}

int
insmntque1(struct vnode *vp, struct mount *mp)
{
	return (insmntque1_int(vp, mp, false));
}

/*
 * Flush out and invalidate all buffers associated with a bufobj
 * Called with the underlying object locked.
 */
int
bufobj_invalbuf(struct bufobj *bo, int flags, int slpflag, int slptimeo)
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
			do {
				error = BO_SYNC(bo, MNT_WAIT);
			} while (error == ERELOOKUP);
			if (error != 0)
				return (error);
			BO_LOCK(bo);
			if (bo->bo_numoutput > 0 || bo->bo_dirty.bv_cnt > 0) {
				BO_UNLOCK(bo);
				return (EBUSY);
			}
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
		if (error == 0 && !(flags & V_CLEANONLY))
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
		if ((flags & V_VMIO) == 0 && bo->bo_object != NULL) {
			BO_UNLOCK(bo);
			vm_object_pip_wait_unlocked(bo->bo_object, "bovlbx");
			BO_LOCK(bo);
		}
	} while (bo->bo_numoutput > 0);
	BO_UNLOCK(bo);

	/*
	 * Destroy the copy in the VM cache, too.
	 */
	if (bo->bo_object != NULL &&
	    (flags & (V_ALT | V_NORMAL | V_CLEANONLY | V_VMIO)) == 0) {
		VM_OBJECT_WLOCK(bo->bo_object);
		vm_object_page_remove(bo->bo_object, 0, 0, (flags & V_SAVE) ?
		    OBJPR_CLEANONLY : 0);
		VM_OBJECT_WUNLOCK(bo->bo_object);
	}

#ifdef INVARIANTS
	BO_LOCK(bo);
	if ((flags & (V_ALT | V_NORMAL | V_CLEANONLY | V_VMIO |
	    V_ALLOWCLEAN)) == 0 && (bo->bo_dirty.bv_cnt > 0 ||
	    bo->bo_clean.bv_cnt > 0))
		panic("vinvalbuf: flush failed");
	if ((flags & (V_ALT | V_NORMAL | V_CLEANONLY | V_VMIO)) == 0 &&
	    bo->bo_dirty.bv_cnt > 0)
		panic("vinvalbuf: flush dirty failed");
	BO_UNLOCK(bo);
#endif
	return (0);
}

/*
 * Flush out and invalidate all buffers associated with a vnode.
 * Called with the underlying object locked.
 */
int
vinvalbuf(struct vnode *vp, int flags, int slpflag, int slptimeo)
{

	CTR3(KTR_VFS, "%s: vp %p with flags %d", __func__, vp, flags);
	ASSERT_VOP_LOCKED(vp, "vinvalbuf");
	if (vp->v_object != NULL && vp->v_object->handle != vp)
		return (0);
	return (bufobj_invalbuf(&vp->v_bufobj, flags, slpflag, slptimeo));
}

/*
 * Flush out buffers on the specified list.
 *
 */
static int
flushbuflist(struct bufv *bufv, int flags, struct bufobj *bo, int slpflag,
    int slptimeo)
{
	struct buf *bp, *nbp;
	int retval, error;
	daddr_t lblkno;
	b_xflags_t xflags;

	ASSERT_BO_WLOCKED(bo);

	retval = 0;
	TAILQ_FOREACH_SAFE(bp, &bufv->bv_hd, b_bobufs, nbp) {
		/*
		 * If we are flushing both V_NORMAL and V_ALT buffers then
		 * do not skip any buffers. If we are flushing only V_NORMAL
		 * buffers then skip buffers marked as BX_ALTDATA. If we are
		 * flushing only V_ALT buffers then skip buffers not marked
		 * as BX_ALTDATA.
		 */
		if (((flags & (V_NORMAL | V_ALT)) != (V_NORMAL | V_ALT)) &&
		   (((flags & V_NORMAL) && (bp->b_xflags & BX_ALTDATA) != 0) ||
		    ((flags & V_ALT) && (bp->b_xflags & BX_ALTDATA) == 0))) {
			continue;
		}
		if (nbp != NULL) {
			lblkno = nbp->b_lblkno;
			xflags = nbp->b_xflags & (BX_VNDIRTY | BX_VNCLEAN);
		}
		retval = EAGAIN;
		error = BUF_TIMELOCK(bp,
		    LK_EXCLUSIVE | LK_SLEEPFAIL | LK_INTERLOCK, BO_LOCKPTR(bo),
		    "flushbuf", slpflag, slptimeo);
		if (error) {
			BO_LOCK(bo);
			return (error != ENOLCK ? error : EAGAIN);
		}
		KASSERT(bp->b_bufobj == bo,
		    ("bp %p wrong b_bufobj %p should be %p",
		    bp, bp->b_bufobj, bo));
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
		bp->b_flags |= (B_INVAL | B_RELBUF);
		bp->b_flags &= ~B_ASYNC;
		brelse(bp);
		BO_LOCK(bo);
		if (nbp == NULL)
			break;
		nbp = gbincore(bo, lblkno);
		if (nbp == NULL || (nbp->b_xflags & (BX_VNDIRTY | BX_VNCLEAN))
		    != xflags)
			break;			/* nbp invalid */
	}
	return (retval);
}

int
bnoreuselist(struct bufv *bufv, struct bufobj *bo, daddr_t startn, daddr_t endn)
{
	struct buf *bp;
	int error;
	daddr_t lblkno;

	ASSERT_BO_LOCKED(bo);

	for (lblkno = startn;;) {
again:
		bp = BUF_PCTRIE_LOOKUP_GE(&bufv->bv_root, lblkno);
		if (bp == NULL || bp->b_lblkno >= endn ||
		    bp->b_lblkno < startn)
			break;
		error = BUF_TIMELOCK(bp, LK_EXCLUSIVE | LK_SLEEPFAIL |
		    LK_INTERLOCK, BO_LOCKPTR(bo), "brlsfl", 0, 0);
		if (error != 0) {
			BO_RLOCK(bo);
			if (error == ENOLCK)
				goto again;
			return (error);
		}
		KASSERT(bp->b_bufobj == bo,
		    ("bp %p wrong b_bufobj %p should be %p",
		    bp, bp->b_bufobj, bo));
		lblkno = bp->b_lblkno + 1;
		if ((bp->b_flags & B_MANAGED) == 0)
			bremfree(bp);
		bp->b_flags |= B_RELBUF;
		/*
		 * In the VMIO case, use the B_NOREUSE flag to hint that the
		 * pages backing each buffer in the range are unlikely to be
		 * reused.  Dirty buffers will have the hint applied once
		 * they've been written.
		 */
		if ((bp->b_flags & B_VMIO) != 0)
			bp->b_flags |= B_NOREUSE;
		brelse(bp);
		BO_RLOCK(bo);
	}
	return (0);
}

/*
 * Truncate a file's buffer and pages to a specified length.  This
 * is in lieu of the old vinvalbuf mechanism, which performed unneeded
 * sync activity.
 */
int
vtruncbuf(struct vnode *vp, off_t length, int blksize)
{
	struct buf *bp, *nbp;
	struct bufobj *bo;
	daddr_t startlbn;

	CTR4(KTR_VFS, "%s: vp %p with block %d:%ju", __func__,
	    vp, blksize, (uintmax_t)length);

	/*
	 * Round up to the *next* lbn.
	 */
	startlbn = howmany(length, blksize);

	ASSERT_VOP_LOCKED(vp, "vtruncbuf");

	bo = &vp->v_bufobj;
restart_unlocked:
	BO_LOCK(bo);

	while (v_inval_buf_range_locked(vp, bo, startlbn, INT64_MAX) == EAGAIN)
		;

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
			    BO_LOCKPTR(bo)) == ENOLCK)
				goto restart_unlocked;

			VNASSERT((bp->b_flags & B_DELWRI), vp,
			    ("buf(%p) on dirty queue without DELWRI", bp));

			bremfree(bp);
			bawrite(bp);
			BO_LOCK(bo);
			goto restartsync;
		}
	}

	bufobj_wwait(bo, 0, 0);
	BO_UNLOCK(bo);
	vnode_pager_setsize(vp, length);

	return (0);
}

/*
 * Invalidate the cached pages of a file's buffer within the range of block
 * numbers [startlbn, endlbn).
 */
void
v_inval_buf_range(struct vnode *vp, daddr_t startlbn, daddr_t endlbn,
    int blksize)
{
	struct bufobj *bo;
	off_t start, end;

	ASSERT_VOP_LOCKED(vp, "v_inval_buf_range");

	start = blksize * startlbn;
	end = blksize * endlbn;

	bo = &vp->v_bufobj;
	BO_LOCK(bo);
	MPASS(blksize == bo->bo_bsize);

	while (v_inval_buf_range_locked(vp, bo, startlbn, endlbn) == EAGAIN)
		;

	BO_UNLOCK(bo);
	vn_pages_remove(vp, OFF_TO_IDX(start), OFF_TO_IDX(end + PAGE_SIZE - 1));
}

static int
v_inval_buf_range_locked(struct vnode *vp, struct bufobj *bo,
    daddr_t startlbn, daddr_t endlbn)
{
	struct buf *bp, *nbp;
	bool anyfreed;

	ASSERT_VOP_LOCKED(vp, "v_inval_buf_range_locked");
	ASSERT_BO_LOCKED(bo);

	do {
		anyfreed = false;
		TAILQ_FOREACH_SAFE(bp, &bo->bo_clean.bv_hd, b_bobufs, nbp) {
			if (bp->b_lblkno < startlbn || bp->b_lblkno >= endlbn)
				continue;
			if (BUF_LOCK(bp,
			    LK_EXCLUSIVE | LK_SLEEPFAIL | LK_INTERLOCK,
			    BO_LOCKPTR(bo)) == ENOLCK) {
				BO_LOCK(bo);
				return (EAGAIN);
			}

			bremfree(bp);
			bp->b_flags |= B_INVAL | B_RELBUF;
			bp->b_flags &= ~B_ASYNC;
			brelse(bp);
			anyfreed = true;

			BO_LOCK(bo);
			if (nbp != NULL &&
			    (((nbp->b_xflags & BX_VNCLEAN) == 0) ||
			    nbp->b_vp != vp ||
			    (nbp->b_flags & B_DELWRI) != 0))
				return (EAGAIN);
		}

		TAILQ_FOREACH_SAFE(bp, &bo->bo_dirty.bv_hd, b_bobufs, nbp) {
			if (bp->b_lblkno < startlbn || bp->b_lblkno >= endlbn)
				continue;
			if (BUF_LOCK(bp,
			    LK_EXCLUSIVE | LK_SLEEPFAIL | LK_INTERLOCK,
			    BO_LOCKPTR(bo)) == ENOLCK) {
				BO_LOCK(bo);
				return (EAGAIN);
			}
			bremfree(bp);
			bp->b_flags |= B_INVAL | B_RELBUF;
			bp->b_flags &= ~B_ASYNC;
			brelse(bp);
			anyfreed = true;

			BO_LOCK(bo);
			if (nbp != NULL &&
			    (((nbp->b_xflags & BX_VNDIRTY) == 0) ||
			    (nbp->b_vp != vp) ||
			    (nbp->b_flags & B_DELWRI) == 0))
				return (EAGAIN);
		}
	} while (anyfreed);
	return (0);
}

static void
buf_vlist_remove(struct buf *bp)
{
	struct bufv *bv;
	b_xflags_t flags;

	flags = bp->b_xflags;

	KASSERT(bp->b_bufobj != NULL, ("No b_bufobj %p", bp));
	ASSERT_BO_WLOCKED(bp->b_bufobj);
	KASSERT((flags & (BX_VNDIRTY | BX_VNCLEAN)) != 0 &&
	    (flags & (BX_VNDIRTY | BX_VNCLEAN)) != (BX_VNDIRTY | BX_VNCLEAN),
	    ("%s: buffer %p has invalid queue state", __func__, bp));

	if ((flags & BX_VNDIRTY) != 0)
		bv = &bp->b_bufobj->bo_dirty;
	else
		bv = &bp->b_bufobj->bo_clean;
	BUF_PCTRIE_REMOVE(&bv->bv_root, bp->b_lblkno);
	TAILQ_REMOVE(&bv->bv_hd, bp, b_bobufs);
	bv->bv_cnt--;
	bp->b_xflags &= ~(BX_VNDIRTY | BX_VNCLEAN);
}

/*
 * Add the buffer to the sorted clean or dirty block list.
 *
 * NOTE: xflags is passed as a constant, optimizing this inline function!
 */
static void
buf_vlist_add(struct buf *bp, struct bufobj *bo, b_xflags_t xflags)
{
	struct bufv *bv;
	struct buf *n;
	int error;

	ASSERT_BO_WLOCKED(bo);
	KASSERT((bo->bo_flag & BO_NOBUFS) == 0,
	    ("buf_vlist_add: bo %p does not allow bufs", bo));
	KASSERT((xflags & BX_VNDIRTY) == 0 || (bo->bo_flag & BO_DEAD) == 0,
	    ("dead bo %p", bo));
	KASSERT((bp->b_xflags & (BX_VNDIRTY|BX_VNCLEAN)) == 0,
	    ("buf_vlist_add: Buf %p has existing xflags %d", bp, bp->b_xflags));
	bp->b_xflags |= xflags;
	if (xflags & BX_VNDIRTY)
		bv = &bo->bo_dirty;
	else
		bv = &bo->bo_clean;

	/*
	 * Keep the list ordered.  Optimize empty list insertion.  Assume
	 * we tend to grow at the tail so lookup_le should usually be cheaper
	 * than _ge. 
	 */
	if (bv->bv_cnt == 0 ||
	    bp->b_lblkno > TAILQ_LAST(&bv->bv_hd, buflists)->b_lblkno)
		TAILQ_INSERT_TAIL(&bv->bv_hd, bp, b_bobufs);
	else if ((n = BUF_PCTRIE_LOOKUP_LE(&bv->bv_root, bp->b_lblkno)) == NULL)
		TAILQ_INSERT_HEAD(&bv->bv_hd, bp, b_bobufs);
	else
		TAILQ_INSERT_AFTER(&bv->bv_hd, n, bp, b_bobufs);
	error = BUF_PCTRIE_INSERT(&bv->bv_root, bp);
	if (error)
		panic("buf_vlist_add:  Preallocated nodes insufficient.");
	bv->bv_cnt++;
}

/*
 * Look up a buffer using the buffer tries.
 */
struct buf *
gbincore(struct bufobj *bo, daddr_t lblkno)
{
	struct buf *bp;

	ASSERT_BO_LOCKED(bo);
	bp = BUF_PCTRIE_LOOKUP(&bo->bo_clean.bv_root, lblkno);
	if (bp != NULL)
		return (bp);
	return (BUF_PCTRIE_LOOKUP(&bo->bo_dirty.bv_root, lblkno));
}

/*
 * Look up a buf using the buffer tries, without the bufobj lock.  This relies
 * on SMR for safe lookup, and bufs being in a no-free zone to provide type
 * stability of the result.  Like other lockless lookups, the found buf may
 * already be invalid by the time this function returns.
 */
struct buf *
gbincore_unlocked(struct bufobj *bo, daddr_t lblkno)
{
	struct buf *bp;

	ASSERT_BO_UNLOCKED(bo);
	bp = BUF_PCTRIE_LOOKUP_UNLOCKED(&bo->bo_clean.bv_root, lblkno);
	if (bp != NULL)
		return (bp);
	return (BUF_PCTRIE_LOOKUP_UNLOCKED(&bo->bo_dirty.bv_root, lblkno));
}

/*
 * Associate a buffer with a vnode.
 */
void
bgetvp(struct vnode *vp, struct buf *bp)
{
	struct bufobj *bo;

	bo = &vp->v_bufobj;
	ASSERT_BO_WLOCKED(bo);
	VNASSERT(bp->b_vp == NULL, bp->b_vp, ("bgetvp: not free"));

	CTR3(KTR_BUF, "bgetvp(%p) vp %p flags %X", bp, vp, bp->b_flags);
	VNASSERT((bp->b_xflags & (BX_VNDIRTY|BX_VNCLEAN)) == 0, vp,
	    ("bgetvp: bp already attached! %p", bp));

	vhold(vp);
	bp->b_vp = vp;
	bp->b_bufobj = bo;
	/*
	 * Insert onto list for new vnode.
	 */
	buf_vlist_add(bp, bo, BX_VNCLEAN);
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
	buf_vlist_remove(bp);
	if ((bo->bo_flag & BO_ONWORKLST) && bo->bo_dirty.bv_cnt == 0) {
		bo->bo_flag &= ~BO_ONWORKLST;
		mtx_lock(&sync_mtx);
		LIST_REMOVE(bo, bo_synclist);
		syncer_worklist_len--;
		mtx_unlock(&sync_mtx);
	}
	bp->b_vp = NULL;
	bp->b_bufobj = NULL;
	BO_UNLOCK(bo);
	vdrop(vp);
}

/*
 * Add an item to the syncer work queue.
 */
static void
vn_syncer_add_to_worklist(struct bufobj *bo, int delay)
{
	int slot;

	ASSERT_BO_WLOCKED(bo);

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

SYSCTL_PROC(_vfs, OID_AUTO, worklist_len,
    CTLTYPE_INT | CTLFLAG_MPSAFE| CTLFLAG_RD, NULL, 0,
    sysctl_vfs_worklist_len, "I", "Syncer thread worklist length");

static struct proc *updateproc;
static void sched_sync(void);
static struct kproc_desc up_kp = {
	"syncer",
	sched_sync,
	&updateproc
};
SYSINIT(syncer, SI_SUB_KTHREAD_UPDATE, SI_ORDER_FIRST, kproc_start, &up_kp);

static int
sync_vnode(struct synclist *slp, struct bufobj **bo, struct thread *td)
{
	struct vnode *vp;
	struct mount *mp;

	*bo = LIST_FIRST(slp);
	if (*bo == NULL)
		return (0);
	vp = bo2vnode(*bo);
	if (VOP_ISLOCKED(vp) != 0 || VI_TRYLOCK(vp) == 0)
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
		return (*bo == LIST_FIRST(slp));
	}
	MPASSERT(mp == NULL || (curthread->td_pflags & TDP_IGNSUSP) != 0 ||
	    (mp->mnt_kern_flag & MNTK_SUSPENDED) == 0, mp,
	    ("suspended mp syncing vp %p", vp));
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	(void) VOP_FSYNC(vp, MNT_LAZY, td);
	VOP_UNLOCK(vp);
	vn_finished_write(mp);
	BO_LOCK(*bo);
	if (((*bo)->bo_flag & BO_ONWORKLST) != 0) {
		/*
		 * Put us back on the worklist.  The worklist
		 * routine will remove us from our current
		 * position and then add us back in at a later
		 * position.
		 */
		vn_syncer_add_to_worklist(*bo, syncdelay);
	}
	BO_UNLOCK(*bo);
	vdrop(vp);
	mtx_lock(&sync_mtx);
	return (0);
}

static int first_printf = 1;

/*
 * System filesystem synchronizer daemon.
 */
static void
sched_sync(void)
{
	struct synclist *next, *slp;
	struct bufobj *bo;
	long starttime;
	struct thread *td = curthread;
	int last_work_seen;
	int net_worklist_len;
	int syncer_final_iter;
	int error;

	last_work_seen = 0;
	syncer_final_iter = 0;
	syncer_state = SYNCER_RUNNING;
	starttime = time_uptime;
	td->td_pflags |= TDP_NORUNNINGBUF;

	EVENTHANDLER_REGISTER(shutdown_pre_sync, syncer_shutdown, td->td_proc,
	    SHUTDOWN_PRI_LAST);

	mtx_lock(&sync_mtx);
	for (;;) {
		if (syncer_state == SYNCER_FINAL_DELAY &&
		    syncer_final_iter == 0) {
			mtx_unlock(&sync_mtx);
			kproc_suspend_check(td->td_proc);
			mtx_lock(&sync_mtx);
		}
		net_worklist_len = syncer_worklist_len - sync_vnode_count;
		if (syncer_state != SYNCER_RUNNING &&
		    starttime != time_uptime) {
			if (first_printf) {
				printf("\nSyncing disks, vnodes remaining... ");
				first_printf = 0;
			}
			printf("%d ", net_worklist_len);
		}
		starttime = time_uptime;

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
		while (!LIST_EMPTY(slp)) {
			error = sync_vnode(slp, &bo, td);
			if (error == 1) {
				LIST_REMOVE(bo, bo_synclist);
				LIST_INSERT_HEAD(next, bo, bo_synclist);
				continue;
			}

			if (first_printf == 0) {
				/*
				 * Drop the sync mutex, because some watchdog
				 * drivers need to sleep while patting
				 */
				mtx_unlock(&sync_mtx);
				wdog_kern_pat(WD_LASTVAL);
				mtx_lock(&sync_mtx);
			}
		}
		if (syncer_state == SYNCER_FINAL_DELAY && syncer_final_iter > 0)
			syncer_final_iter--;
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
		if (rushjob > 0) {
			rushjob -= 1;
			continue;
		}
		/*
		 * Just sleep for a short period of time between
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
		if (syncer_state != SYNCER_RUNNING ||
		    time_uptime == starttime) {
			thread_lock(td);
			sched_prio(td, PPAUSE);
			thread_unlock(td);
		}
		if (syncer_state != SYNCER_RUNNING)
			cv_timedwait(&sync_wakeup, &sync_mtx,
			    hz / SYNCER_SHUTDOWN_SPEEDUP);
		else if (time_uptime == starttime)
			cv_timedwait(&sync_wakeup, &sync_mtx, hz);
	}
}

/*
 * Request the syncer daemon to speed up its work.
 * We never push it to speed up more than half of its
 * normal turn time, otherwise it could take over the cpu.
 */
int
speedup_syncer(void)
{
	int ret = 0;

	mtx_lock(&sync_mtx);
	if (rushjob < syncdelay / 2) {
		rushjob += 1;
		stat_rush_requests += 1;
		ret = 1;
	}
	mtx_unlock(&sync_mtx);
	cv_broadcast(&sync_wakeup);
	return (ret);
}

/*
 * Tell the syncer to speed up its work and run though its work
 * list several times, then tell it to shut down.
 */
static void
syncer_shutdown(void *arg, int howto)
{

	if (howto & RB_NOSYNC)
		return;
	mtx_lock(&sync_mtx);
	syncer_state = SYNCER_SHUTTING_DOWN;
	rushjob = 0;
	mtx_unlock(&sync_mtx);
	cv_broadcast(&sync_wakeup);
	kproc_shutdown(arg, howto);
}

void
syncer_suspend(void)
{

	syncer_shutdown(updateproc, 0);
}

void
syncer_resume(void)
{

	mtx_lock(&sync_mtx);
	first_printf = 1;
	syncer_state = SYNCER_RUNNING;
	mtx_unlock(&sync_mtx);
	cv_broadcast(&sync_wakeup);
	kproc_resume(updateproc);
}

/*
 * Move the buffer between the clean and dirty lists of its vnode.
 */
void
reassignbuf(struct buf *bp)
{
	struct vnode *vp;
	struct bufobj *bo;
	int delay;
#ifdef INVARIANTS
	struct bufv *bv;
#endif

	vp = bp->b_vp;
	bo = bp->b_bufobj;

	KASSERT((bp->b_flags & B_PAGING) == 0,
	    ("%s: cannot reassign paging buffer %p", __func__, bp));

	CTR3(KTR_BUF, "reassignbuf(%p) vp %p flags %X",
	    bp, bp->b_vp, bp->b_flags);

	BO_LOCK(bo);
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
#ifdef INVARIANTS
	bv = &bo->bo_clean;
	bp = TAILQ_FIRST(&bv->bv_hd);
	KASSERT(bp == NULL || bp->b_bufobj == bo,
	    ("bp %p wrong b_bufobj %p should be %p", bp, bp->b_bufobj, bo));
	bp = TAILQ_LAST(&bv->bv_hd, buflists);
	KASSERT(bp == NULL || bp->b_bufobj == bo,
	    ("bp %p wrong b_bufobj %p should be %p", bp, bp->b_bufobj, bo));
	bv = &bo->bo_dirty;
	bp = TAILQ_FIRST(&bv->bv_hd);
	KASSERT(bp == NULL || bp->b_bufobj == bo,
	    ("bp %p wrong b_bufobj %p should be %p", bp, bp->b_bufobj, bo));
	bp = TAILQ_LAST(&bv->bv_hd, buflists);
	KASSERT(bp == NULL || bp->b_bufobj == bo,
	    ("bp %p wrong b_bufobj %p should be %p", bp, bp->b_bufobj, bo));
#endif
	BO_UNLOCK(bo);
}

static void
v_init_counters(struct vnode *vp)
{

	VNASSERT(vp->v_type == VNON && vp->v_data == NULL && vp->v_iflag == 0,
	    vp, ("%s called for an initialized vnode", __FUNCTION__));
	ASSERT_VI_UNLOCKED(vp, __FUNCTION__);

	refcount_init(&vp->v_holdcnt, 1);
	refcount_init(&vp->v_usecount, 1);
}

/*
 * Get a usecount on a vnode.
 *
 * vget and vget_finish may fail to lock the vnode if they lose a race against
 * it being doomed. LK_RETRY can be passed in flags to lock it anyway.
 *
 * Consumers which don't guarantee liveness of the vnode can use SMR to
 * try to get a reference. Note this operation can fail since the vnode
 * may be awaiting getting freed by the time they get to it.
 */
enum vgetstate
vget_prep_smr(struct vnode *vp)
{
	enum vgetstate vs;

	VFS_SMR_ASSERT_ENTERED();

	if (refcount_acquire_if_not_zero(&vp->v_usecount)) {
		vs = VGET_USECOUNT;
	} else {
		if (vhold_smr(vp))
			vs = VGET_HOLDCNT;
		else
			vs = VGET_NONE;
	}
	return (vs);
}

enum vgetstate
vget_prep(struct vnode *vp)
{
	enum vgetstate vs;

	if (refcount_acquire_if_not_zero(&vp->v_usecount)) {
		vs = VGET_USECOUNT;
	} else {
		vhold(vp);
		vs = VGET_HOLDCNT;
	}
	return (vs);
}

void
vget_abort(struct vnode *vp, enum vgetstate vs)
{

	switch (vs) {
	case VGET_USECOUNT:
		vrele(vp);
		break;
	case VGET_HOLDCNT:
		vdrop(vp);
		break;
	default:
		__assert_unreachable();
	}
}

int
vget(struct vnode *vp, int flags)
{
	enum vgetstate vs;

	vs = vget_prep(vp);
	return (vget_finish(vp, flags, vs));
}

int
vget_finish(struct vnode *vp, int flags, enum vgetstate vs)
{
	int error;

	if ((flags & LK_INTERLOCK) != 0)
		ASSERT_VI_LOCKED(vp, __func__);
	else
		ASSERT_VI_UNLOCKED(vp, __func__);
	VNPASS(vs == VGET_HOLDCNT || vs == VGET_USECOUNT, vp);
	VNPASS(vp->v_holdcnt > 0, vp);
	VNPASS(vs == VGET_HOLDCNT || vp->v_usecount > 0, vp);

	error = vn_lock(vp, flags);
	if (__predict_false(error != 0)) {
		vget_abort(vp, vs);
		CTR2(KTR_VFS, "%s: impossible to lock vnode %p", __func__,
		    vp);
		return (error);
	}

	vget_finish_ref(vp, vs);
	return (0);
}

void
vget_finish_ref(struct vnode *vp, enum vgetstate vs)
{
	int old;

	VNPASS(vs == VGET_HOLDCNT || vs == VGET_USECOUNT, vp);
	VNPASS(vp->v_holdcnt > 0, vp);
	VNPASS(vs == VGET_HOLDCNT || vp->v_usecount > 0, vp);

	if (vs == VGET_USECOUNT)
		return;

	/*
	 * We hold the vnode. If the usecount is 0 it will be utilized to keep
	 * the vnode around. Otherwise someone else lended their hold count and
	 * we have to drop ours.
	 */
	old = atomic_fetchadd_int(&vp->v_usecount, 1);
	VNASSERT(old >= 0, vp, ("%s: wrong use count %d", __func__, old));
	if (old != 0) {
#ifdef INVARIANTS
		old = atomic_fetchadd_int(&vp->v_holdcnt, -1);
		VNASSERT(old > 1, vp, ("%s: wrong hold count %d", __func__, old));
#else
		refcount_release(&vp->v_holdcnt);
#endif
	}
}

void
vref(struct vnode *vp)
{
	enum vgetstate vs;

	CTR2(KTR_VFS, "%s: vp %p", __func__, vp);
	vs = vget_prep(vp);
	vget_finish_ref(vp, vs);
}

void
vrefact(struct vnode *vp)
{

	CTR2(KTR_VFS, "%s: vp %p", __func__, vp);
#ifdef INVARIANTS
	int old = atomic_fetchadd_int(&vp->v_usecount, 1);
	VNASSERT(old > 0, vp, ("%s: wrong use count %d", __func__, old));
#else
	refcount_acquire(&vp->v_usecount);
#endif
}

void
vlazy(struct vnode *vp)
{
	struct mount *mp;

	VNASSERT(vp->v_holdcnt > 0, vp, ("%s: vnode not held", __func__));

	if ((vp->v_mflag & VMP_LAZYLIST) != 0)
		return;
	/*
	 * We may get here for inactive routines after the vnode got doomed.
	 */
	if (VN_IS_DOOMED(vp))
		return;
	mp = vp->v_mount;
	mtx_lock(&mp->mnt_listmtx);
	if ((vp->v_mflag & VMP_LAZYLIST) == 0) {
		vp->v_mflag |= VMP_LAZYLIST;
		TAILQ_INSERT_TAIL(&mp->mnt_lazyvnodelist, vp, v_lazylist);
		mp->mnt_lazyvnodelistsize++;
	}
	mtx_unlock(&mp->mnt_listmtx);
}

static void
vunlazy(struct vnode *vp)
{
	struct mount *mp;

	ASSERT_VI_LOCKED(vp, __func__);
	VNPASS(!VN_IS_DOOMED(vp), vp);

	mp = vp->v_mount;
	mtx_lock(&mp->mnt_listmtx);
	VNPASS(vp->v_mflag & VMP_LAZYLIST, vp);
	/*
	 * Don't remove the vnode from the lazy list if another thread
	 * has increased the hold count. It may have re-enqueued the
	 * vnode to the lazy list and is now responsible for its
	 * removal.
	 */
	if (vp->v_holdcnt == 0) {
		vp->v_mflag &= ~VMP_LAZYLIST;
		TAILQ_REMOVE(&mp->mnt_lazyvnodelist, vp, v_lazylist);
		mp->mnt_lazyvnodelistsize--;
	}
	mtx_unlock(&mp->mnt_listmtx);
}

/*
 * This routine is only meant to be called from vgonel prior to dooming
 * the vnode.
 */
static void
vunlazy_gone(struct vnode *vp)
{
	struct mount *mp;

	ASSERT_VOP_ELOCKED(vp, __func__);
	ASSERT_VI_LOCKED(vp, __func__);
	VNPASS(!VN_IS_DOOMED(vp), vp);

	if (vp->v_mflag & VMP_LAZYLIST) {
		mp = vp->v_mount;
		mtx_lock(&mp->mnt_listmtx);
		VNPASS(vp->v_mflag & VMP_LAZYLIST, vp);
		vp->v_mflag &= ~VMP_LAZYLIST;
		TAILQ_REMOVE(&mp->mnt_lazyvnodelist, vp, v_lazylist);
		mp->mnt_lazyvnodelistsize--;
		mtx_unlock(&mp->mnt_listmtx);
	}
}

static void
vdefer_inactive(struct vnode *vp)
{

	ASSERT_VI_LOCKED(vp, __func__);
	VNPASS(vp->v_holdcnt > 0, vp);
	if (VN_IS_DOOMED(vp)) {
		vdropl(vp);
		return;
	}
	if (vp->v_iflag & VI_DEFINACT) {
		VNPASS(vp->v_holdcnt > 1, vp);
		vdropl(vp);
		return;
	}
	if (vp->v_usecount > 0) {
		vp->v_iflag &= ~VI_OWEINACT;
		vdropl(vp);
		return;
	}
	vlazy(vp);
	vp->v_iflag |= VI_DEFINACT;
	VI_UNLOCK(vp);
	atomic_add_long(&deferred_inact, 1);
}

static void
vdefer_inactive_unlocked(struct vnode *vp)
{

	VI_LOCK(vp);
	if ((vp->v_iflag & VI_OWEINACT) == 0) {
		vdropl(vp);
		return;
	}
	vdefer_inactive(vp);
}

enum vput_op { VRELE, VPUT, VUNREF };

/*
 * Handle ->v_usecount transitioning to 0.
 *
 * By releasing the last usecount we take ownership of the hold count which
 * provides liveness of the vnode, meaning we have to vdrop.
 *
 * For all vnodes we may need to perform inactive processing. It requires an
 * exclusive lock on the vnode, while it is legal to call here with only a
 * shared lock (or no locks). If locking the vnode in an expected manner fails,
 * inactive processing gets deferred to the syncer.
 *
 * XXX Some filesystems pass in an exclusively locked vnode and strongly depend
 * on the lock being held all the way until VOP_INACTIVE. This in particular
 * happens with UFS which adds half-constructed vnodes to the hash, where they
 * can be found by other code.
 */
static void
vput_final(struct vnode *vp, enum vput_op func)
{
	int error;
	bool want_unlock;

	CTR2(KTR_VFS, "%s: vp %p", __func__, vp);
	VNPASS(vp->v_holdcnt > 0, vp);

	VI_LOCK(vp);

	/*
	 * By the time we got here someone else might have transitioned
	 * the count back to > 0.
	 */
	if (vp->v_usecount > 0)
		goto out;

	/*
	 * If the vnode is doomed vgone already performed inactive processing
	 * (if needed).
	 */
	if (VN_IS_DOOMED(vp))
		goto out;

	if (__predict_true(VOP_NEED_INACTIVE(vp) == 0))
		goto out;

	if (vp->v_iflag & VI_DOINGINACT)
		goto out;

	/*
	 * Locking operations here will drop the interlock and possibly the
	 * vnode lock, opening a window where the vnode can get doomed all the
	 * while ->v_usecount is 0. Set VI_OWEINACT to let vgone know to
	 * perform inactive.
	 */
	vp->v_iflag |= VI_OWEINACT;
	want_unlock = false;
	error = 0;
	switch (func) {
	case VRELE:
		switch (VOP_ISLOCKED(vp)) {
		case LK_EXCLUSIVE:
			break;
		case LK_EXCLOTHER:
		case 0:
			want_unlock = true;
			error = vn_lock(vp, LK_EXCLUSIVE | LK_INTERLOCK);
			VI_LOCK(vp);
			break;
		default:
			/*
			 * The lock has at least one sharer, but we have no way
			 * to conclude whether this is us. Play it safe and
			 * defer processing.
			 */
			error = EAGAIN;
			break;
		}
		break;
	case VPUT:
		want_unlock = true;
		if (VOP_ISLOCKED(vp) != LK_EXCLUSIVE) {
			error = VOP_LOCK(vp, LK_UPGRADE | LK_INTERLOCK |
			    LK_NOWAIT);
			VI_LOCK(vp);
		}
		break;
	case VUNREF:
		if (VOP_ISLOCKED(vp) != LK_EXCLUSIVE) {
			error = VOP_LOCK(vp, LK_TRYUPGRADE | LK_INTERLOCK);
			VI_LOCK(vp);
		}
		break;
	}
	if (error == 0) {
		if (func == VUNREF) {
			VNASSERT((vp->v_vflag & VV_UNREF) == 0, vp,
			    ("recursive vunref"));
			vp->v_vflag |= VV_UNREF;
		}
		for (;;) {
			error = vinactive(vp);
			if (want_unlock)
				VOP_UNLOCK(vp);
			if (error != ERELOOKUP || !want_unlock)
				break;
			VOP_LOCK(vp, LK_EXCLUSIVE);
		}
		if (func == VUNREF)
			vp->v_vflag &= ~VV_UNREF;
		vdropl(vp);
	} else {
		vdefer_inactive(vp);
	}
	return;
out:
	if (func == VPUT)
		VOP_UNLOCK(vp);
	vdropl(vp);
}

/*
 * Decrement ->v_usecount for a vnode.
 *
 * Releasing the last use count requires additional processing, see vput_final
 * above for details.
 *
 * Comment above each variant denotes lock state on entry and exit.
 */

/*
 * in: any
 * out: same as passed in
 */
void
vrele(struct vnode *vp)
{

	ASSERT_VI_UNLOCKED(vp, __func__);
	if (!refcount_release(&vp->v_usecount))
		return;
	vput_final(vp, VRELE);
}

/*
 * in: locked
 * out: unlocked
 */
void
vput(struct vnode *vp)
{

	ASSERT_VOP_LOCKED(vp, __func__);
	ASSERT_VI_UNLOCKED(vp, __func__);
	if (!refcount_release(&vp->v_usecount)) {
		VOP_UNLOCK(vp);
		return;
	}
	vput_final(vp, VPUT);
}

/*
 * in: locked
 * out: locked
 */
void
vunref(struct vnode *vp)
{

	ASSERT_VOP_LOCKED(vp, __func__);
	ASSERT_VI_UNLOCKED(vp, __func__);
	if (!refcount_release(&vp->v_usecount))
		return;
	vput_final(vp, VUNREF);
}

void
vhold(struct vnode *vp)
{
	int old;

	CTR2(KTR_VFS, "%s: vp %p", __func__, vp);
	old = atomic_fetchadd_int(&vp->v_holdcnt, 1);
	VNASSERT(old >= 0 && (old & VHOLD_ALL_FLAGS) == 0, vp,
	    ("%s: wrong hold count %d", __func__, old));
	if (old == 0)
		vfs_freevnodes_dec();
}

void
vholdnz(struct vnode *vp)
{

	CTR2(KTR_VFS, "%s: vp %p", __func__, vp);
#ifdef INVARIANTS
	int old = atomic_fetchadd_int(&vp->v_holdcnt, 1);
	VNASSERT(old > 0 && (old & VHOLD_ALL_FLAGS) == 0, vp,
	    ("%s: wrong hold count %d", __func__, old));
#else
	atomic_add_int(&vp->v_holdcnt, 1);
#endif
}

/*
 * Grab a hold count unless the vnode is freed.
 *
 * Only use this routine if vfs smr is the only protection you have against
 * freeing the vnode.
 *
 * The code loops trying to add a hold count as long as the VHOLD_NO_SMR flag
 * is not set.  After the flag is set the vnode becomes immutable to anyone but
 * the thread which managed to set the flag.
 *
 * It may be tempting to replace the loop with:
 * count = atomic_fetchadd_int(&vp->v_holdcnt, 1);
 * if (count & VHOLD_NO_SMR) {
 *     backpedal and error out;
 * }
 *
 * However, while this is more performant, it hinders debugging by eliminating
 * the previously mentioned invariant.
 */
bool
vhold_smr(struct vnode *vp)
{
	int count;

	VFS_SMR_ASSERT_ENTERED();

	count = atomic_load_int(&vp->v_holdcnt);
	for (;;) {
		if (count & VHOLD_NO_SMR) {
			VNASSERT((count & ~VHOLD_NO_SMR) == 0, vp,
			    ("non-zero hold count with flags %d\n", count));
			return (false);
		}
		VNASSERT(count >= 0, vp, ("invalid hold count %d\n", count));
		if (atomic_fcmpset_int(&vp->v_holdcnt, &count, count + 1)) {
			if (count == 0)
				vfs_freevnodes_dec();
			return (true);
		}
	}
}

/*
 * Hold a free vnode for recycling.
 *
 * Note: vnode_init references this comment.
 *
 * Attempts to recycle only need the global vnode list lock and have no use for
 * SMR.
 *
 * However, vnodes get inserted into the global list before they get fully
 * initialized and stay there until UMA decides to free the memory. This in
 * particular means the target can be found before it becomes usable and after
 * it becomes recycled. Picking up such vnodes is guarded with v_holdcnt set to
 * VHOLD_NO_SMR.
 *
 * Note: the vnode may gain more references after we transition the count 0->1.
 */
static bool
vhold_recycle_free(struct vnode *vp)
{
	int count;

	mtx_assert(&vnode_list_mtx, MA_OWNED);

	count = atomic_load_int(&vp->v_holdcnt);
	for (;;) {
		if (count & VHOLD_NO_SMR) {
			VNASSERT((count & ~VHOLD_NO_SMR) == 0, vp,
			    ("non-zero hold count with flags %d\n", count));
			return (false);
		}
		VNASSERT(count >= 0, vp, ("invalid hold count %d\n", count));
		if (count > 0) {
			return (false);
		}
		if (atomic_fcmpset_int(&vp->v_holdcnt, &count, count + 1)) {
			vfs_freevnodes_dec();
			return (true);
		}
	}
}

static void __noinline
vdbatch_process(struct vdbatch *vd)
{
	struct vnode *vp;
	int i;

	mtx_assert(&vd->lock, MA_OWNED);
	MPASS(curthread->td_pinned > 0);
	MPASS(vd->index == VDBATCH_SIZE);

	/*
	 * Attempt to requeue the passed batch, but give up easily.
	 *
	 * Despite batching the mechanism is prone to transient *significant*
	 * lock contention, where vnode_list_mtx becomes the primary bottleneck
	 * if multiple CPUs get here (one real-world example is highly parallel
	 * do-nothing make , which will stat *tons* of vnodes). Since it is
	 * quasi-LRU (read: not that great even if fully honoured) just dodge
	 * the problem. Parties which don't like it are welcome to implement
	 * something better.
	 */
	critical_enter();
	if (mtx_trylock(&vnode_list_mtx)) {
		for (i = 0; i < VDBATCH_SIZE; i++) {
			vp = vd->tab[i];
			vd->tab[i] = NULL;
			TAILQ_REMOVE(&vnode_list, vp, v_vnodelist);
			TAILQ_INSERT_TAIL(&vnode_list, vp, v_vnodelist);
			MPASS(vp->v_dbatchcpu != NOCPU);
			vp->v_dbatchcpu = NOCPU;
		}
		mtx_unlock(&vnode_list_mtx);
	} else {
		counter_u64_add(vnode_skipped_requeues, 1);

		for (i = 0; i < VDBATCH_SIZE; i++) {
			vp = vd->tab[i];
			vd->tab[i] = NULL;
			MPASS(vp->v_dbatchcpu != NOCPU);
			vp->v_dbatchcpu = NOCPU;
		}
	}
	vd->index = 0;
	critical_exit();
}

static void
vdbatch_enqueue(struct vnode *vp)
{
	struct vdbatch *vd;

	ASSERT_VI_LOCKED(vp, __func__);
	VNPASS(!VN_IS_DOOMED(vp), vp);

	if (vp->v_dbatchcpu != NOCPU) {
		VI_UNLOCK(vp);
		return;
	}

	sched_pin();
	vd = DPCPU_PTR(vd);
	mtx_lock(&vd->lock);
	MPASS(vd->index < VDBATCH_SIZE);
	MPASS(vd->tab[vd->index] == NULL);
	/*
	 * A hack: we depend on being pinned so that we know what to put in
	 * ->v_dbatchcpu.
	 */
	vp->v_dbatchcpu = curcpu;
	vd->tab[vd->index] = vp;
	vd->index++;
	VI_UNLOCK(vp);
	if (vd->index == VDBATCH_SIZE)
		vdbatch_process(vd);
	mtx_unlock(&vd->lock);
	sched_unpin();
}

/*
 * This routine must only be called for vnodes which are about to be
 * deallocated. Supporting dequeue for arbitrary vndoes would require
 * validating that the locked batch matches.
 */
static void
vdbatch_dequeue(struct vnode *vp)
{
	struct vdbatch *vd;
	int i;
	short cpu;

	VNPASS(vp->v_type == VBAD || vp->v_type == VNON, vp);

	cpu = vp->v_dbatchcpu;
	if (cpu == NOCPU)
		return;

	vd = DPCPU_ID_PTR(cpu, vd);
	mtx_lock(&vd->lock);
	for (i = 0; i < vd->index; i++) {
		if (vd->tab[i] != vp)
			continue;
		vp->v_dbatchcpu = NOCPU;
		vd->index--;
		vd->tab[i] = vd->tab[vd->index];
		vd->tab[vd->index] = NULL;
		break;
	}
	mtx_unlock(&vd->lock);
	/*
	 * Either we dequeued the vnode above or the target CPU beat us to it.
	 */
	MPASS(vp->v_dbatchcpu == NOCPU);
}

/*
 * Drop the hold count of the vnode.
 *
 * It will only get freed if this is the last hold *and* it has been vgone'd.
 *
 * Because the vnode vm object keeps a hold reference on the vnode if
 * there is at least one resident non-cached page, the vnode cannot
 * leave the active list without the page cleanup done.
 */
static void __noinline
vdropl_final(struct vnode *vp)
{

	ASSERT_VI_LOCKED(vp, __func__);
	VNPASS(VN_IS_DOOMED(vp), vp);
	/*
	 * Set the VHOLD_NO_SMR flag.
	 *
	 * We may be racing against vhold_smr. If they win we can just pretend
	 * we never got this far, they will vdrop later.
	 */
	if (__predict_false(!atomic_cmpset_int(&vp->v_holdcnt, 0, VHOLD_NO_SMR))) {
		vfs_freevnodes_inc();
		VI_UNLOCK(vp);
		/*
		 * We lost the aforementioned race. Any subsequent access is
		 * invalid as they might have managed to vdropl on their own.
		 */
		return;
	}
	/*
	 * Don't bump freevnodes as this one is going away.
	 */
	freevnode(vp);
}

void
vdrop(struct vnode *vp)
{

	ASSERT_VI_UNLOCKED(vp, __func__);
	CTR2(KTR_VFS, "%s: vp %p", __func__, vp);
	if (refcount_release_if_not_last(&vp->v_holdcnt))
		return;
	VI_LOCK(vp);
	vdropl(vp);
}

static void __always_inline
vdropl_impl(struct vnode *vp, bool enqueue)
{

	ASSERT_VI_LOCKED(vp, __func__);
	CTR2(KTR_VFS, "%s: vp %p", __func__, vp);
	if (!refcount_release(&vp->v_holdcnt)) {
		VI_UNLOCK(vp);
		return;
	}
	VNPASS((vp->v_iflag & VI_OWEINACT) == 0, vp);
	VNPASS((vp->v_iflag & VI_DEFINACT) == 0, vp);
	if (VN_IS_DOOMED(vp)) {
		vdropl_final(vp);
		return;
	}

	vfs_freevnodes_inc();
	if (vp->v_mflag & VMP_LAZYLIST) {
		vunlazy(vp);
	}

	if (!enqueue) {
		VI_UNLOCK(vp);
		return;
	}

	/*
	 * Also unlocks the interlock. We can't assert on it as we
	 * released our hold and by now the vnode might have been
	 * freed.
	 */
	vdbatch_enqueue(vp);
}

void
vdropl(struct vnode *vp)
{

	vdropl_impl(vp, true);
}

/*
 * vdrop a vnode when recycling
 *
 * This is a special case routine only to be used when recycling, differs from
 * regular vdrop by not requeieing the vnode on LRU.
 *
 * Consider a case where vtryrecycle continuously fails with all vnodes (due to
 * e.g., frozen writes on the filesystem), filling the batch and causing it to
 * be requeued. Then vnlru will end up revisiting the same vnodes. This is a
 * loop which can last for as long as writes are frozen.
 */
static void
vdropl_recycle(struct vnode *vp)
{

	vdropl_impl(vp, false);
}

static void
vdrop_recycle(struct vnode *vp)
{

	VI_LOCK(vp);
	vdropl_recycle(vp);
}

/*
 * Call VOP_INACTIVE on the vnode and manage the DOINGINACT and OWEINACT
 * flags.  DOINGINACT prevents us from recursing in calls to vinactive.
 */
static int
vinactivef(struct vnode *vp)
{
	struct vm_object *obj;
	int error;

	ASSERT_VOP_ELOCKED(vp, "vinactive");
	ASSERT_VI_LOCKED(vp, "vinactive");
	VNPASS((vp->v_iflag & VI_DOINGINACT) == 0, vp);
	CTR2(KTR_VFS, "%s: vp %p", __func__, vp);
	vp->v_iflag |= VI_DOINGINACT;
	vp->v_iflag &= ~VI_OWEINACT;
	VI_UNLOCK(vp);
	/*
	 * Before moving off the active list, we must be sure that any
	 * modified pages are converted into the vnode's dirty
	 * buffers, since these will no longer be checked once the
	 * vnode is on the inactive list.
	 *
	 * The write-out of the dirty pages is asynchronous.  At the
	 * point that VOP_INACTIVE() is called, there could still be
	 * pending I/O and dirty pages in the object.
	 */
	if ((obj = vp->v_object) != NULL && (vp->v_vflag & VV_NOSYNC) == 0 &&
	    vm_object_mightbedirty(obj)) {
		VM_OBJECT_WLOCK(obj);
		vm_object_page_clean(obj, 0, 0, 0);
		VM_OBJECT_WUNLOCK(obj);
	}
	error = VOP_INACTIVE(vp);
	VI_LOCK(vp);
	VNPASS(vp->v_iflag & VI_DOINGINACT, vp);
	vp->v_iflag &= ~VI_DOINGINACT;
	return (error);
}

int
vinactive(struct vnode *vp)
{

	ASSERT_VOP_ELOCKED(vp, "vinactive");
	ASSERT_VI_LOCKED(vp, "vinactive");
	CTR2(KTR_VFS, "%s: vp %p", __func__, vp);

	if ((vp->v_iflag & VI_OWEINACT) == 0)
		return (0);
	if (vp->v_iflag & VI_DOINGINACT)
		return (0);
	if (vp->v_usecount > 0) {
		vp->v_iflag &= ~VI_OWEINACT;
		return (0);
	}
	return (vinactivef(vp));
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
SYSCTL_INT(_debug, OID_AUTO, busyprt, CTLFLAG_RW, &busyprt, 0, "Print out busy vnodes");
#endif

int
vflush(struct mount *mp, int rootrefs, int flags, struct thread *td)
{
	struct vnode *vp, *mvp, *rootvp = NULL;
	struct vattr vattr;
	int busy = 0, error;

	CTR4(KTR_VFS, "%s: mp %p with rootrefs %d and flags %d", __func__, mp,
	    rootrefs, flags);
	if (rootrefs > 0) {
		KASSERT((flags & (SKIPSYSTEM | WRITECLOSE)) == 0,
		    ("vflush: bad args"));
		/*
		 * Get the filesystem root vnode. We can vput() it
		 * immediately, since with rootrefs > 0, it won't go away.
		 */
		if ((error = VFS_ROOT(mp, LK_EXCLUSIVE, &rootvp)) != 0) {
			CTR2(KTR_VFS, "%s: vfs_root lookup failed with %d",
			    __func__, error);
			return (error);
		}
		vput(rootvp);
	}
loop:
	MNT_VNODE_FOREACH_ALL(vp, mp, mvp) {
		vholdl(vp);
		error = vn_lock(vp, LK_INTERLOCK | LK_EXCLUSIVE);
		if (error) {
			vdrop(vp);
			MNT_VNODE_FOREACH_ALL_ABORT(mp, mvp);
			goto loop;
		}
		/*
		 * Skip over a vnodes marked VV_SYSTEM.
		 */
		if ((flags & SKIPSYSTEM) && (vp->v_vflag & VV_SYSTEM)) {
			VOP_UNLOCK(vp);
			vdrop(vp);
			continue;
		}
		/*
		 * If WRITECLOSE is set, flush out unlinked but still open
		 * files (even if open only for reading) and regular file
		 * vnodes open for writing.
		 */
		if (flags & WRITECLOSE) {
			if (vp->v_object != NULL) {
				VM_OBJECT_WLOCK(vp->v_object);
				vm_object_page_clean(vp->v_object, 0, 0, 0);
				VM_OBJECT_WUNLOCK(vp->v_object);
			}
			do {
				error = VOP_FSYNC(vp, MNT_WAIT, td);
			} while (error == ERELOOKUP);
			if (error != 0) {
				VOP_UNLOCK(vp);
				vdrop(vp);
				MNT_VNODE_FOREACH_ALL_ABORT(mp, mvp);
				return (error);
			}
			error = VOP_GETATTR(vp, &vattr, td->td_ucred);
			VI_LOCK(vp);

			if ((vp->v_type == VNON ||
			    (error == 0 && vattr.va_nlink > 0)) &&
			    (vp->v_writecount <= 0 || vp->v_type != VREG)) {
				VOP_UNLOCK(vp);
				vdropl(vp);
				continue;
			}
		} else
			VI_LOCK(vp);
		/*
		 * With v_usecount == 0, all we need to do is clear out the
		 * vnode data structures and we are done.
		 *
		 * If FORCECLOSE is set, forcibly close the vnode.
		 */
		if (vp->v_usecount == 0 || (flags & FORCECLOSE)) {
			vgonel(vp);
		} else {
			busy++;
#ifdef DIAGNOSTIC
			if (busyprt)
				vn_printf(vp, "vflush: busy vnode ");
#endif
		}
		VOP_UNLOCK(vp);
		vdropl(vp);
	}
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
			VOP_LOCK(rootvp, LK_EXCLUSIVE|LK_INTERLOCK);
			vgone(rootvp);
			VOP_UNLOCK(rootvp);
			busy = 0;
		} else
			VI_UNLOCK(rootvp);
	}
	if (busy) {
		CTR2(KTR_VFS, "%s: failing as %d vnodes are busy", __func__,
		    busy);
		return (EBUSY);
	}
	for (; rootrefs > 0; rootrefs--)
		vrele(rootvp);
	return (0);
}

/*
 * Recycle an unused vnode.
 */
int
vrecycle(struct vnode *vp)
{
	int recycled;

	VI_LOCK(vp);
	recycled = vrecyclel(vp);
	VI_UNLOCK(vp);
	return (recycled);
}

/*
 * vrecycle, with the vp interlock held.
 */
int
vrecyclel(struct vnode *vp)
{
	int recycled;

	ASSERT_VOP_ELOCKED(vp, __func__);
	ASSERT_VI_LOCKED(vp, __func__);
	CTR2(KTR_VFS, "%s: vp %p", __func__, vp);
	recycled = 0;
	if (vp->v_usecount == 0) {
		recycled = 1;
		vgonel(vp);
	}
	return (recycled);
}

/*
 * Eliminate all activity associated with a vnode
 * in preparation for reuse.
 */
void
vgone(struct vnode *vp)
{
	VI_LOCK(vp);
	vgonel(vp);
	VI_UNLOCK(vp);
}

/*
 * Notify upper mounts about reclaimed or unlinked vnode.
 */
void
vfs_notify_upper(struct vnode *vp, enum vfs_notify_upper_type event)
{
	struct mount *mp;
	struct mount_upper_node *ump;

	mp = atomic_load_ptr(&vp->v_mount);
	if (mp == NULL)
		return;
	if (TAILQ_EMPTY(&mp->mnt_notify))
		return;

	MNT_ILOCK(mp);
	mp->mnt_upper_pending++;
	KASSERT(mp->mnt_upper_pending > 0,
	    ("%s: mnt_upper_pending %d", __func__, mp->mnt_upper_pending));
	TAILQ_FOREACH(ump, &mp->mnt_notify, mnt_upper_link) {
		MNT_IUNLOCK(mp);
		switch (event) {
		case VFS_NOTIFY_UPPER_RECLAIM:
			VFS_RECLAIM_LOWERVP(ump->mp, vp);
			break;
		case VFS_NOTIFY_UPPER_UNLINK:
			VFS_UNLINK_LOWERVP(ump->mp, vp);
			break;
		}
		MNT_ILOCK(mp);
	}
	mp->mnt_upper_pending--;
	if ((mp->mnt_kern_flag & MNTK_UPPER_WAITER) != 0 &&
	    mp->mnt_upper_pending == 0) {
		mp->mnt_kern_flag &= ~MNTK_UPPER_WAITER;
		wakeup(&mp->mnt_uppers);
	}
	MNT_IUNLOCK(mp);
}

/*
 * vgone, with the vp interlock held.
 */
static void
vgonel(struct vnode *vp)
{
	struct thread *td;
	struct mount *mp;
	vm_object_t object;
	bool active, doinginact, oweinact;

	ASSERT_VOP_ELOCKED(vp, "vgonel");
	ASSERT_VI_LOCKED(vp, "vgonel");
	VNASSERT(vp->v_holdcnt, vp,
	    ("vgonel: vp %p has no reference.", vp));
	CTR2(KTR_VFS, "%s: vp %p", __func__, vp);
	td = curthread;

	/*
	 * Don't vgonel if we're already doomed.
	 */
	if (VN_IS_DOOMED(vp)) {
		VNPASS(vn_get_state(vp) == VSTATE_DESTROYING || \
		    vn_get_state(vp) == VSTATE_DEAD, vp);
		return;
	}
	/*
	 * Paired with freevnode.
	 */
	vn_seqc_write_begin_locked(vp);
	vunlazy_gone(vp);
	vn_irflag_set_locked(vp, VIRF_DOOMED);
	vn_set_state(vp, VSTATE_DESTROYING);

	/*
	 * Check to see if the vnode is in use.  If so, we have to
	 * call VOP_CLOSE() and VOP_INACTIVE().
	 *
	 * It could be that VOP_INACTIVE() requested reclamation, in
	 * which case we should avoid recursion, so check
	 * VI_DOINGINACT.  This is not precise but good enough.
	 */
	active = vp->v_usecount > 0;
	oweinact = (vp->v_iflag & VI_OWEINACT) != 0;
	doinginact = (vp->v_iflag & VI_DOINGINACT) != 0;

	/*
	 * If we need to do inactive VI_OWEINACT will be set.
	 */
	if (vp->v_iflag & VI_DEFINACT) {
		VNASSERT(vp->v_holdcnt > 1, vp, ("lost hold count"));
		vp->v_iflag &= ~VI_DEFINACT;
		vdropl(vp);
	} else {
		VNASSERT(vp->v_holdcnt > 0, vp, ("vnode without hold count"));
		VI_UNLOCK(vp);
	}
	cache_purge_vgone(vp);
	vfs_notify_upper(vp, VFS_NOTIFY_UPPER_RECLAIM);

	/*
	 * If purging an active vnode, it must be closed and
	 * deactivated before being reclaimed.
	 */
	if (active)
		VOP_CLOSE(vp, FNONBLOCK, NOCRED, td);
	if (!doinginact) {
		do {
			if (oweinact || active) {
				VI_LOCK(vp);
				vinactivef(vp);
				oweinact = (vp->v_iflag & VI_OWEINACT) != 0;
				VI_UNLOCK(vp);
			}
		} while (oweinact);
	}
	if (vp->v_type == VSOCK)
		vfs_unp_reclaim(vp);

	/*
	 * Clean out any buffers associated with the vnode.
	 * If the flush fails, just toss the buffers.
	 */
	mp = NULL;
	if (!TAILQ_EMPTY(&vp->v_bufobj.bo_dirty.bv_hd))
		(void) vn_start_secondary_write(vp, &mp, V_WAIT);
	if (vinvalbuf(vp, V_SAVE, 0, 0) != 0) {
		while (vinvalbuf(vp, 0, 0, 0) != 0)
			;
	}

	BO_LOCK(&vp->v_bufobj);
	KASSERT(TAILQ_EMPTY(&vp->v_bufobj.bo_dirty.bv_hd) &&
	    vp->v_bufobj.bo_dirty.bv_cnt == 0 &&
	    TAILQ_EMPTY(&vp->v_bufobj.bo_clean.bv_hd) &&
	    vp->v_bufobj.bo_clean.bv_cnt == 0,
	    ("vp %p bufobj not invalidated", vp));

	/*
	 * For VMIO bufobj, BO_DEAD is set later, or in
	 * vm_object_terminate() after the object's page queue is
	 * flushed.
	 */
	object = vp->v_bufobj.bo_object;
	if (object == NULL)
		vp->v_bufobj.bo_flag |= BO_DEAD;
	BO_UNLOCK(&vp->v_bufobj);

	/*
	 * Handle the VM part.  Tmpfs handles v_object on its own (the
	 * OBJT_VNODE check).  Nullfs or other bypassing filesystems
	 * should not touch the object borrowed from the lower vnode
	 * (the handle check).
	 */
	if (object != NULL && object->type == OBJT_VNODE &&
	    object->handle == vp)
		vnode_destroy_vobject(vp);

	/*
	 * Reclaim the vnode.
	 */
	if (VOP_RECLAIM(vp))
		panic("vgone: cannot reclaim");
	if (mp != NULL)
		vn_finished_secondary_write(mp);
	VNASSERT(vp->v_object == NULL, vp,
	    ("vop_reclaim left v_object vp=%p", vp));
	/*
	 * Clear the advisory locks and wake up waiting threads.
	 */
	if (vp->v_lockf != NULL) {
		(void)VOP_ADVLOCKPURGE(vp);
		vp->v_lockf = NULL;
	}
	/*
	 * Delete from old mount point vnode list.
	 */
	if (vp->v_mount == NULL) {
		VI_LOCK(vp);
	} else {
		delmntque(vp);
		ASSERT_VI_LOCKED(vp, "vgonel 2");
	}
	/*
	 * Done with purge, reset to the standard lock and invalidate
	 * the vnode.
	 */
	vp->v_vnlock = &vp->v_lock;
	vp->v_op = &dead_vnodeops;
	vp->v_type = VBAD;
	vn_set_state(vp, VSTATE_DEAD);
}

/*
 * Print out a description of a vnode.
 */
static const char *const vtypename[] = {
	[VNON] = "VNON",
	[VREG] = "VREG",
	[VDIR] = "VDIR",
	[VBLK] = "VBLK",
	[VCHR] = "VCHR",
	[VLNK] = "VLNK",
	[VSOCK] = "VSOCK",
	[VFIFO] = "VFIFO",
	[VBAD] = "VBAD",
	[VMARKER] = "VMARKER",
};
_Static_assert(nitems(vtypename) == VLASTTYPE + 1,
    "vnode type name not added to vtypename");

static const char *const vstatename[] = {
	[VSTATE_UNINITIALIZED] = "VSTATE_UNINITIALIZED",
	[VSTATE_CONSTRUCTED] = "VSTATE_CONSTRUCTED",
	[VSTATE_DESTROYING] = "VSTATE_DESTROYING",
	[VSTATE_DEAD] = "VSTATE_DEAD",
};
_Static_assert(nitems(vstatename) == VLASTSTATE + 1,
    "vnode state name not added to vstatename");

_Static_assert((VHOLD_ALL_FLAGS & ~VHOLD_NO_SMR) == 0,
    "new hold count flag not added to vn_printf");

void
vn_printf(struct vnode *vp, const char *fmt, ...)
{
	va_list ap;
	char buf[256], buf2[16];
	u_long flags;
	u_int holdcnt;
	short irflag;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("%p: ", (void *)vp);
	printf("type %s state %s op %p\n", vtypename[vp->v_type],
	    vstatename[vp->v_state], vp->v_op);
	holdcnt = atomic_load_int(&vp->v_holdcnt);
	printf("    usecount %d, writecount %d, refcount %d seqc users %d",
	    vp->v_usecount, vp->v_writecount, holdcnt & ~VHOLD_ALL_FLAGS,
	    vp->v_seqc_users);
	switch (vp->v_type) {
	case VDIR:
		printf(" mountedhere %p\n", vp->v_mountedhere);
		break;
	case VCHR:
		printf(" rdev %p\n", vp->v_rdev);
		break;
	case VSOCK:
		printf(" socket %p\n", vp->v_unpcb);
		break;
	case VFIFO:
		printf(" fifoinfo %p\n", vp->v_fifoinfo);
		break;
	default:
		printf("\n");
		break;
	}
	buf[0] = '\0';
	buf[1] = '\0';
	if (holdcnt & VHOLD_NO_SMR)
		strlcat(buf, "|VHOLD_NO_SMR", sizeof(buf));
	printf("    hold count flags (%s)\n", buf + 1);

	buf[0] = '\0';
	buf[1] = '\0';
	irflag = vn_irflag_read(vp);
	if (irflag & VIRF_DOOMED)
		strlcat(buf, "|VIRF_DOOMED", sizeof(buf));
	if (irflag & VIRF_PGREAD)
		strlcat(buf, "|VIRF_PGREAD", sizeof(buf));
	if (irflag & VIRF_MOUNTPOINT)
		strlcat(buf, "|VIRF_MOUNTPOINT", sizeof(buf));
	if (irflag & VIRF_TEXT_REF)
		strlcat(buf, "|VIRF_TEXT_REF", sizeof(buf));
	flags = irflag & ~(VIRF_DOOMED | VIRF_PGREAD | VIRF_MOUNTPOINT | VIRF_TEXT_REF);
	if (flags != 0) {
		snprintf(buf2, sizeof(buf2), "|VIRF(0x%lx)", flags);
		strlcat(buf, buf2, sizeof(buf));
	}
	if (vp->v_vflag & VV_ROOT)
		strlcat(buf, "|VV_ROOT", sizeof(buf));
	if (vp->v_vflag & VV_ISTTY)
		strlcat(buf, "|VV_ISTTY", sizeof(buf));
	if (vp->v_vflag & VV_NOSYNC)
		strlcat(buf, "|VV_NOSYNC", sizeof(buf));
	if (vp->v_vflag & VV_ETERNALDEV)
		strlcat(buf, "|VV_ETERNALDEV", sizeof(buf));
	if (vp->v_vflag & VV_CACHEDLABEL)
		strlcat(buf, "|VV_CACHEDLABEL", sizeof(buf));
	if (vp->v_vflag & VV_VMSIZEVNLOCK)
		strlcat(buf, "|VV_VMSIZEVNLOCK", sizeof(buf));
	if (vp->v_vflag & VV_COPYONWRITE)
		strlcat(buf, "|VV_COPYONWRITE", sizeof(buf));
	if (vp->v_vflag & VV_SYSTEM)
		strlcat(buf, "|VV_SYSTEM", sizeof(buf));
	if (vp->v_vflag & VV_PROCDEP)
		strlcat(buf, "|VV_PROCDEP", sizeof(buf));
	if (vp->v_vflag & VV_DELETED)
		strlcat(buf, "|VV_DELETED", sizeof(buf));
	if (vp->v_vflag & VV_MD)
		strlcat(buf, "|VV_MD", sizeof(buf));
	if (vp->v_vflag & VV_FORCEINSMQ)
		strlcat(buf, "|VV_FORCEINSMQ", sizeof(buf));
	if (vp->v_vflag & VV_READLINK)
		strlcat(buf, "|VV_READLINK", sizeof(buf));
	flags = vp->v_vflag & ~(VV_ROOT | VV_ISTTY | VV_NOSYNC | VV_ETERNALDEV |
	    VV_CACHEDLABEL | VV_VMSIZEVNLOCK | VV_COPYONWRITE | VV_SYSTEM |
	    VV_PROCDEP | VV_DELETED | VV_MD | VV_FORCEINSMQ | VV_READLINK);
	if (flags != 0) {
		snprintf(buf2, sizeof(buf2), "|VV(0x%lx)", flags);
		strlcat(buf, buf2, sizeof(buf));
	}
	if (vp->v_iflag & VI_MOUNT)
		strlcat(buf, "|VI_MOUNT", sizeof(buf));
	if (vp->v_iflag & VI_DOINGINACT)
		strlcat(buf, "|VI_DOINGINACT", sizeof(buf));
	if (vp->v_iflag & VI_OWEINACT)
		strlcat(buf, "|VI_OWEINACT", sizeof(buf));
	if (vp->v_iflag & VI_DEFINACT)
		strlcat(buf, "|VI_DEFINACT", sizeof(buf));
	if (vp->v_iflag & VI_FOPENING)
		strlcat(buf, "|VI_FOPENING", sizeof(buf));
	flags = vp->v_iflag & ~(VI_MOUNT | VI_DOINGINACT |
	    VI_OWEINACT | VI_DEFINACT | VI_FOPENING);
	if (flags != 0) {
		snprintf(buf2, sizeof(buf2), "|VI(0x%lx)", flags);
		strlcat(buf, buf2, sizeof(buf));
	}
	if (vp->v_mflag & VMP_LAZYLIST)
		strlcat(buf, "|VMP_LAZYLIST", sizeof(buf));
	flags = vp->v_mflag & ~(VMP_LAZYLIST);
	if (flags != 0) {
		snprintf(buf2, sizeof(buf2), "|VMP(0x%lx)", flags);
		strlcat(buf, buf2, sizeof(buf));
	}
	printf("    flags (%s)", buf + 1);
	if (mtx_owned(VI_MTX(vp)))
		printf(" VI_LOCKed");
	printf("\n");
	if (vp->v_object != NULL)
		printf("    v_object %p ref %d pages %d "
		    "cleanbuf %d dirtybuf %d\n",
		    vp->v_object, vp->v_object->ref_count,
		    vp->v_object->resident_page_count,
		    vp->v_bufobj.bo_clean.bv_cnt,
		    vp->v_bufobj.bo_dirty.bv_cnt);
	printf("    ");
	lockmgr_printinfo(vp->v_vnlock);
	if (vp->v_data != NULL)
		VOP_PRINT(vp);
}

#ifdef DDB
/*
 * List all of the locked vnodes in the system.
 * Called when debugging the kernel.
 */
DB_SHOW_COMMAND_FLAGS(lockedvnods, lockedvnodes, DB_CMD_MEMSAFE)
{
	struct mount *mp;
	struct vnode *vp;

	/*
	 * Note: because this is DDB, we can't obey the locking semantics
	 * for these structures, which means we could catch an inconsistent
	 * state and dereference a nasty pointer.  Not much to be done
	 * about that.
	 */
	db_printf("Locked vnodes\n");
	TAILQ_FOREACH(mp, &mountlist, mnt_list) {
		TAILQ_FOREACH(vp, &mp->mnt_nvnodelist, v_nmntvnodes) {
			if (vp->v_type != VMARKER && VOP_ISLOCKED(vp))
				vn_printf(vp, "vnode ");
		}
	}
}

/*
 * Show details about the given vnode.
 */
DB_SHOW_COMMAND(vnode, db_show_vnode)
{
	struct vnode *vp;

	if (!have_addr)
		return;
	vp = (struct vnode *)addr;
	vn_printf(vp, "vnode ");
}

/*
 * Show details about the given mount point.
 */
DB_SHOW_COMMAND(mount, db_show_mount)
{
	struct mount *mp;
	struct vfsopt *opt;
	struct statfs *sp;
	struct vnode *vp;
	char buf[512];
	uint64_t mflags;
	u_int flags;

	if (!have_addr) {
		/* No address given, print short info about all mount points. */
		TAILQ_FOREACH(mp, &mountlist, mnt_list) {
			db_printf("%p %s on %s (%s)\n", mp,
			    mp->mnt_stat.f_mntfromname,
			    mp->mnt_stat.f_mntonname,
			    mp->mnt_stat.f_fstypename);
			if (db_pager_quit)
				break;
		}
		db_printf("\nMore info: show mount <addr>\n");
		return;
	}

	mp = (struct mount *)addr;
	db_printf("%p %s on %s (%s)\n", mp, mp->mnt_stat.f_mntfromname,
	    mp->mnt_stat.f_mntonname, mp->mnt_stat.f_fstypename);

	buf[0] = '\0';
	mflags = mp->mnt_flag;
#define	MNT_FLAG(flag)	do {						\
	if (mflags & (flag)) {						\
		if (buf[0] != '\0')					\
			strlcat(buf, ", ", sizeof(buf));		\
		strlcat(buf, (#flag) + 4, sizeof(buf));			\
		mflags &= ~(flag);					\
	}								\
} while (0)
	MNT_FLAG(MNT_RDONLY);
	MNT_FLAG(MNT_SYNCHRONOUS);
	MNT_FLAG(MNT_NOEXEC);
	MNT_FLAG(MNT_NOSUID);
	MNT_FLAG(MNT_NFS4ACLS);
	MNT_FLAG(MNT_UNION);
	MNT_FLAG(MNT_ASYNC);
	MNT_FLAG(MNT_SUIDDIR);
	MNT_FLAG(MNT_SOFTDEP);
	MNT_FLAG(MNT_NOSYMFOLLOW);
	MNT_FLAG(MNT_GJOURNAL);
	MNT_FLAG(MNT_MULTILABEL);
	MNT_FLAG(MNT_ACLS);
	MNT_FLAG(MNT_NOATIME);
	MNT_FLAG(MNT_NOCLUSTERR);
	MNT_FLAG(MNT_NOCLUSTERW);
	MNT_FLAG(MNT_SUJ);
	MNT_FLAG(MNT_EXRDONLY);
	MNT_FLAG(MNT_EXPORTED);
	MNT_FLAG(MNT_DEFEXPORTED);
	MNT_FLAG(MNT_EXPORTANON);
	MNT_FLAG(MNT_EXKERB);
	MNT_FLAG(MNT_EXPUBLIC);
	MNT_FLAG(MNT_LOCAL);
	MNT_FLAG(MNT_QUOTA);
	MNT_FLAG(MNT_ROOTFS);
	MNT_FLAG(MNT_USER);
	MNT_FLAG(MNT_IGNORE);
	MNT_FLAG(MNT_UPDATE);
	MNT_FLAG(MNT_DELEXPORT);
	MNT_FLAG(MNT_RELOAD);
	MNT_FLAG(MNT_FORCE);
	MNT_FLAG(MNT_SNAPSHOT);
	MNT_FLAG(MNT_BYFSID);
#undef MNT_FLAG
	if (mflags != 0) {
		if (buf[0] != '\0')
			strlcat(buf, ", ", sizeof(buf));
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    "0x%016jx", mflags);
	}
	db_printf("    mnt_flag = %s\n", buf);

	buf[0] = '\0';
	flags = mp->mnt_kern_flag;
#define	MNT_KERN_FLAG(flag)	do {					\
	if (flags & (flag)) {						\
		if (buf[0] != '\0')					\
			strlcat(buf, ", ", sizeof(buf));		\
		strlcat(buf, (#flag) + 5, sizeof(buf));			\
		flags &= ~(flag);					\
	}								\
} while (0)
	MNT_KERN_FLAG(MNTK_UNMOUNTF);
	MNT_KERN_FLAG(MNTK_ASYNC);
	MNT_KERN_FLAG(MNTK_SOFTDEP);
	MNT_KERN_FLAG(MNTK_NOMSYNC);
	MNT_KERN_FLAG(MNTK_DRAINING);
	MNT_KERN_FLAG(MNTK_REFEXPIRE);
	MNT_KERN_FLAG(MNTK_EXTENDED_SHARED);
	MNT_KERN_FLAG(MNTK_SHARED_WRITES);
	MNT_KERN_FLAG(MNTK_NO_IOPF);
	MNT_KERN_FLAG(MNTK_RECURSE);
	MNT_KERN_FLAG(MNTK_UPPER_WAITER);
	MNT_KERN_FLAG(MNTK_UNLOCKED_INSMNTQUE);
	MNT_KERN_FLAG(MNTK_USES_BCACHE);
	MNT_KERN_FLAG(MNTK_VMSETSIZE_BUG);
	MNT_KERN_FLAG(MNTK_FPLOOKUP);
	MNT_KERN_FLAG(MNTK_TASKQUEUE_WAITER);
	MNT_KERN_FLAG(MNTK_NOASYNC);
	MNT_KERN_FLAG(MNTK_UNMOUNT);
	MNT_KERN_FLAG(MNTK_MWAIT);
	MNT_KERN_FLAG(MNTK_SUSPEND);
	MNT_KERN_FLAG(MNTK_SUSPEND2);
	MNT_KERN_FLAG(MNTK_SUSPENDED);
	MNT_KERN_FLAG(MNTK_NULL_NOCACHE);
	MNT_KERN_FLAG(MNTK_LOOKUP_SHARED);
#undef MNT_KERN_FLAG
	if (flags != 0) {
		if (buf[0] != '\0')
			strlcat(buf, ", ", sizeof(buf));
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    "0x%08x", flags);
	}
	db_printf("    mnt_kern_flag = %s\n", buf);

	db_printf("    mnt_opt = ");
	opt = TAILQ_FIRST(mp->mnt_opt);
	if (opt != NULL) {
		db_printf("%s", opt->name);
		opt = TAILQ_NEXT(opt, link);
		while (opt != NULL) {
			db_printf(", %s", opt->name);
			opt = TAILQ_NEXT(opt, link);
		}
	}
	db_printf("\n");

	sp = &mp->mnt_stat;
	db_printf("    mnt_stat = { version=%u type=%u flags=0x%016jx "
	    "bsize=%ju iosize=%ju blocks=%ju bfree=%ju bavail=%jd files=%ju "
	    "ffree=%jd syncwrites=%ju asyncwrites=%ju syncreads=%ju "
	    "asyncreads=%ju namemax=%u owner=%u fsid=[%d, %d] }\n",
	    (u_int)sp->f_version, (u_int)sp->f_type, (uintmax_t)sp->f_flags,
	    (uintmax_t)sp->f_bsize, (uintmax_t)sp->f_iosize,
	    (uintmax_t)sp->f_blocks, (uintmax_t)sp->f_bfree,
	    (intmax_t)sp->f_bavail, (uintmax_t)sp->f_files,
	    (intmax_t)sp->f_ffree, (uintmax_t)sp->f_syncwrites,
	    (uintmax_t)sp->f_asyncwrites, (uintmax_t)sp->f_syncreads,
	    (uintmax_t)sp->f_asyncreads, (u_int)sp->f_namemax,
	    (u_int)sp->f_owner, (int)sp->f_fsid.val[0], (int)sp->f_fsid.val[1]);

	db_printf("    mnt_cred = { uid=%u ruid=%u",
	    (u_int)mp->mnt_cred->cr_uid, (u_int)mp->mnt_cred->cr_ruid);
	if (jailed(mp->mnt_cred))
		db_printf(", jail=%d", mp->mnt_cred->cr_prison->pr_id);
	db_printf(" }\n");
	db_printf("    mnt_ref = %d (with %d in the struct)\n",
	    vfs_mount_fetch_counter(mp, MNT_COUNT_REF), mp->mnt_ref);
	db_printf("    mnt_gen = %d\n", mp->mnt_gen);
	db_printf("    mnt_nvnodelistsize = %d\n", mp->mnt_nvnodelistsize);
	db_printf("    mnt_lazyvnodelistsize = %d\n",
	    mp->mnt_lazyvnodelistsize);
	db_printf("    mnt_writeopcount = %d (with %d in the struct)\n",
	    vfs_mount_fetch_counter(mp, MNT_COUNT_WRITEOPCOUNT), mp->mnt_writeopcount);
	db_printf("    mnt_iosize_max = %d\n", mp->mnt_iosize_max);
	db_printf("    mnt_hashseed = %u\n", mp->mnt_hashseed);
	db_printf("    mnt_lockref = %d (with %d in the struct)\n",
	    vfs_mount_fetch_counter(mp, MNT_COUNT_LOCKREF), mp->mnt_lockref);
	db_printf("    mnt_secondary_writes = %d\n", mp->mnt_secondary_writes);
	db_printf("    mnt_secondary_accwrites = %d\n",
	    mp->mnt_secondary_accwrites);
	db_printf("    mnt_gjprovider = %s\n",
	    mp->mnt_gjprovider != NULL ? mp->mnt_gjprovider : "NULL");
	db_printf("    mnt_vfs_ops = %d\n", mp->mnt_vfs_ops);

	db_printf("\n\nList of active vnodes\n");
	TAILQ_FOREACH(vp, &mp->mnt_nvnodelist, v_nmntvnodes) {
		if (vp->v_type != VMARKER && vp->v_holdcnt > 0) {
			vn_printf(vp, "vnode ");
			if (db_pager_quit)
				break;
		}
	}
	db_printf("\n\nList of inactive vnodes\n");
	TAILQ_FOREACH(vp, &mp->mnt_nvnodelist, v_nmntvnodes) {
		if (vp->v_type != VMARKER && vp->v_holdcnt == 0) {
			vn_printf(vp, "vnode ");
			if (db_pager_quit)
				break;
		}
	}
}
#endif	/* DDB */

/*
 * Fill in a struct xvfsconf based on a struct vfsconf.
 */
static int
vfsconf2x(struct sysctl_req *req, struct vfsconf *vfsp)
{
	struct xvfsconf xvfsp;

	bzero(&xvfsp, sizeof(xvfsp));
	strcpy(xvfsp.vfc_name, vfsp->vfc_name);
	xvfsp.vfc_typenum = vfsp->vfc_typenum;
	xvfsp.vfc_refcount = vfsp->vfc_refcount;
	xvfsp.vfc_flags = vfsp->vfc_flags;
	/*
	 * These are unused in userland, we keep them
	 * to not break binary compatibility.
	 */
	xvfsp.vfc_vfsops = NULL;
	xvfsp.vfc_next = NULL;
	return (SYSCTL_OUT(req, &xvfsp, sizeof(xvfsp)));
}

#ifdef COMPAT_FREEBSD32
struct xvfsconf32 {
	uint32_t	vfc_vfsops;
	char		vfc_name[MFSNAMELEN];
	int32_t		vfc_typenum;
	int32_t		vfc_refcount;
	int32_t		vfc_flags;
	uint32_t	vfc_next;
};

static int
vfsconf2x32(struct sysctl_req *req, struct vfsconf *vfsp)
{
	struct xvfsconf32 xvfsp;

	bzero(&xvfsp, sizeof(xvfsp));
	strcpy(xvfsp.vfc_name, vfsp->vfc_name);
	xvfsp.vfc_typenum = vfsp->vfc_typenum;
	xvfsp.vfc_refcount = vfsp->vfc_refcount;
	xvfsp.vfc_flags = vfsp->vfc_flags;
	return (SYSCTL_OUT(req, &xvfsp, sizeof(xvfsp)));
}
#endif

/*
 * Top level filesystem related information gathering.
 */
static int
sysctl_vfs_conflist(SYSCTL_HANDLER_ARGS)
{
	struct vfsconf *vfsp;
	int error;

	error = 0;
	vfsconf_slock();
	TAILQ_FOREACH(vfsp, &vfsconf, vfc_list) {
#ifdef COMPAT_FREEBSD32
		if (req->flags & SCTL_MASK32)
			error = vfsconf2x32(req, vfsp);
		else
#endif
			error = vfsconf2x(req, vfsp);
		if (error)
			break;
	}
	vfsconf_sunlock();
	return (error);
}

SYSCTL_PROC(_vfs, OID_AUTO, conflist, CTLTYPE_OPAQUE | CTLFLAG_RD |
    CTLFLAG_MPSAFE, NULL, 0, sysctl_vfs_conflist,
    "S,xvfsconf", "List of all configured filesystems");

#ifndef BURN_BRIDGES
static int	sysctl_ovfs_conf(SYSCTL_HANDLER_ARGS);

static int
vfs_sysctl(SYSCTL_HANDLER_ARGS)
{
	int *name = (int *)arg1 - 1;	/* XXX */
	u_int namelen = arg2 + 1;	/* XXX */
	struct vfsconf *vfsp;

	log(LOG_WARNING, "userland calling deprecated sysctl, "
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
		vfsconf_slock();
		TAILQ_FOREACH(vfsp, &vfsconf, vfc_list) {
			if (vfsp->vfc_typenum == name[2])
				break;
		}
		vfsconf_sunlock();
		if (vfsp == NULL)
			return (EOPNOTSUPP);
#ifdef COMPAT_FREEBSD32
		if (req->flags & SCTL_MASK32)
			return (vfsconf2x32(req, vfsp));
		else
#endif
			return (vfsconf2x(req, vfsp));
	}
	return (EOPNOTSUPP);
}

static SYSCTL_NODE(_vfs, VFS_GENERIC, generic, CTLFLAG_RD | CTLFLAG_SKIP |
    CTLFLAG_MPSAFE, vfs_sysctl,
    "Generic filesystem");

#if 1 || defined(COMPAT_PRELITE2)

static int
sysctl_ovfs_conf(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct vfsconf *vfsp;
	struct ovfsconf ovfs;

	vfsconf_slock();
	TAILQ_FOREACH(vfsp, &vfsconf, vfc_list) {
		bzero(&ovfs, sizeof(ovfs));
		ovfs.vfc_vfsops = vfsp->vfc_vfsops;	/* XXX used as flag */
		strcpy(ovfs.vfc_name, vfsp->vfc_name);
		ovfs.vfc_index = vfsp->vfc_typenum;
		ovfs.vfc_refcount = vfsp->vfc_refcount;
		ovfs.vfc_flags = vfsp->vfc_flags;
		error = SYSCTL_OUT(req, &ovfs, sizeof ovfs);
		if (error != 0) {
			vfsconf_sunlock();
			return (error);
		}
	}
	vfsconf_sunlock();
	return (0);
}

#endif /* 1 || COMPAT_PRELITE2 */
#endif /* !BURN_BRIDGES */

static void
unmount_or_warn(struct mount *mp)
{
	int error;

	error = dounmount(mp, MNT_FORCE, curthread);
	if (error != 0) {
		printf("unmount of %s failed (", mp->mnt_stat.f_mntonname);
		if (error == EBUSY)
			printf("BUSY)\n");
		else
			printf("%d)\n", error);
	}
}

/*
 * Unmount all filesystems. The list is traversed in reverse order
 * of mounting to avoid dependencies.
 */
void
vfs_unmountall(void)
{
	struct mount *mp, *tmp;

	CTR1(KTR_VFS, "%s: unmounting all filesystems", __func__);

	/*
	 * Since this only runs when rebooting, it is not interlocked.
	 */
	TAILQ_FOREACH_REVERSE_SAFE(mp, &mountlist, mntlist, mnt_list, tmp) {
		vfs_ref(mp);

		/*
		 * Forcibly unmounting "/dev" before "/" would prevent clean
		 * unmount of the latter.
		 */
		if (mp == rootdevmp)
			continue;

		unmount_or_warn(mp);
	}

	if (rootdevmp != NULL)
		unmount_or_warn(rootdevmp);
}

static void
vfs_deferred_inactive(struct vnode *vp, int lkflags)
{

	ASSERT_VI_LOCKED(vp, __func__);
	VNPASS((vp->v_iflag & VI_DEFINACT) == 0, vp);
	if ((vp->v_iflag & VI_OWEINACT) == 0) {
		vdropl(vp);
		return;
	}
	if (vn_lock(vp, lkflags) == 0) {
		VI_LOCK(vp);
		vinactive(vp);
		VOP_UNLOCK(vp);
		vdropl(vp);
		return;
	}
	vdefer_inactive_unlocked(vp);
}

static int
vfs_periodic_inactive_filter(struct vnode *vp, void *arg)
{

	return (vp->v_iflag & VI_DEFINACT);
}

static void __noinline
vfs_periodic_inactive(struct mount *mp, int flags)
{
	struct vnode *vp, *mvp;
	int lkflags;

	lkflags = LK_EXCLUSIVE | LK_INTERLOCK;
	if (flags != MNT_WAIT)
		lkflags |= LK_NOWAIT;

	MNT_VNODE_FOREACH_LAZY(vp, mp, mvp, vfs_periodic_inactive_filter, NULL) {
		if ((vp->v_iflag & VI_DEFINACT) == 0) {
			VI_UNLOCK(vp);
			continue;
		}
		vp->v_iflag &= ~VI_DEFINACT;
		vfs_deferred_inactive(vp, lkflags);
	}
}

static inline bool
vfs_want_msync(struct vnode *vp)
{
	struct vm_object *obj;

	/*
	 * This test may be performed without any locks held.
	 * We rely on vm_object's type stability.
	 */
	if (vp->v_vflag & VV_NOSYNC)
		return (false);
	obj = vp->v_object;
	return (obj != NULL && vm_object_mightbedirty(obj));
}

static int
vfs_periodic_msync_inactive_filter(struct vnode *vp, void *arg __unused)
{

	if (vp->v_vflag & VV_NOSYNC)
		return (false);
	if (vp->v_iflag & VI_DEFINACT)
		return (true);
	return (vfs_want_msync(vp));
}

static void __noinline
vfs_periodic_msync_inactive(struct mount *mp, int flags)
{
	struct vnode *vp, *mvp;
	struct vm_object *obj;
	int lkflags, objflags;
	bool seen_defer;

	lkflags = LK_EXCLUSIVE | LK_INTERLOCK;
	if (flags != MNT_WAIT) {
		lkflags |= LK_NOWAIT;
		objflags = OBJPC_NOSYNC;
	} else {
		objflags = OBJPC_SYNC;
	}

	MNT_VNODE_FOREACH_LAZY(vp, mp, mvp, vfs_periodic_msync_inactive_filter, NULL) {
		seen_defer = false;
		if (vp->v_iflag & VI_DEFINACT) {
			vp->v_iflag &= ~VI_DEFINACT;
			seen_defer = true;
		}
		if (!vfs_want_msync(vp)) {
			if (seen_defer)
				vfs_deferred_inactive(vp, lkflags);
			else
				VI_UNLOCK(vp);
			continue;
		}
		if (vget(vp, lkflags) == 0) {
			obj = vp->v_object;
			if (obj != NULL && (vp->v_vflag & VV_NOSYNC) == 0) {
				VM_OBJECT_WLOCK(obj);
				vm_object_page_clean(obj, 0, 0, objflags);
				VM_OBJECT_WUNLOCK(obj);
			}
			vput(vp);
			if (seen_defer)
				vdrop(vp);
		} else {
			if (seen_defer)
				vdefer_inactive_unlocked(vp);
		}
	}
}

void
vfs_periodic(struct mount *mp, int flags)
{

	CTR2(KTR_VFS, "%s: mp %p", __func__, mp);

	if ((mp->mnt_kern_flag & MNTK_NOMSYNC) != 0)
		vfs_periodic_inactive(mp, flags);
	else
		vfs_periodic_msync_inactive(mp, flags);
}

static void
destroy_vpollinfo_free(struct vpollinfo *vi)
{

	knlist_destroy(&vi->vpi_selinfo.si_note);
	mtx_destroy(&vi->vpi_lock);
	free(vi, M_VNODEPOLL);
}

static void
destroy_vpollinfo(struct vpollinfo *vi)
{

	knlist_clear(&vi->vpi_selinfo.si_note, 1);
	seldrain(&vi->vpi_selinfo);
	destroy_vpollinfo_free(vi);
}

/*
 * Initialize per-vnode helper structure to hold poll-related state.
 */
void
v_addpollinfo(struct vnode *vp)
{
	struct vpollinfo *vi;

	if (vp->v_pollinfo != NULL)
		return;
	vi = malloc(sizeof(*vi), M_VNODEPOLL, M_WAITOK | M_ZERO);
	mtx_init(&vi->vpi_lock, "vnode pollinfo", NULL, MTX_DEF);
	knlist_init(&vi->vpi_selinfo.si_note, vp, vfs_knllock,
	    vfs_knlunlock, vfs_knl_assert_lock);
	VI_LOCK(vp);
	if (vp->v_pollinfo != NULL) {
		VI_UNLOCK(vp);
		destroy_vpollinfo_free(vi);
		return;
	}
	vp->v_pollinfo = vi;
	VI_UNLOCK(vp);
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
vn_pollrecord(struct vnode *vp, struct thread *td, int events)
{

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
		return (events);
	}
	vp->v_pollinfo->vpi_events |= events;
	selrecord(td, &vp->v_pollinfo->vpi_selinfo);
	mtx_unlock(&vp->v_pollinfo->vpi_lock);
	return (0);
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
	.vop_close =	sync_close,
	.vop_fsync =	sync_fsync,
	.vop_getwritemount = vop_stdgetwritemount,
	.vop_inactive =	sync_inactive,
	.vop_need_inactive = vop_stdneed_inactive,
	.vop_reclaim =	sync_reclaim,
	.vop_lock1 =	vop_stdlock,
	.vop_unlock =	vop_stdunlock,
	.vop_islocked =	vop_stdislocked,
	.vop_fplookup_vexec = VOP_EAGAIN,
	.vop_fplookup_symlink = VOP_EAGAIN,
};
VFS_VOP_VECTOR_REGISTER(sync_vnodeops);

/*
 * Create a new filesystem syncer vnode for the specified mount point.
 */
void
vfs_allocate_syncvnode(struct mount *mp)
{
	struct vnode *vp;
	struct bufobj *bo;
	static long start, incr, next;
	int error;

	/* Allocate a new vnode */
	error = getnewvnode("syncer", mp, &sync_vnodeops, &vp);
	if (error != 0)
		panic("vfs_allocate_syncvnode: getnewvnode() failed");
	vp->v_type = VNON;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	vp->v_vflag |= VV_FORCEINSMQ;
	error = insmntque1(vp, mp);
	if (error != 0)
		panic("vfs_allocate_syncvnode: insmntque() failed");
	vp->v_vflag &= ~VV_FORCEINSMQ;
	vn_set_state(vp, VSTATE_CONSTRUCTED);
	VOP_UNLOCK(vp);
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
	bo = &vp->v_bufobj;
	BO_LOCK(bo);
	vn_syncer_add_to_worklist(bo, syncdelay > 0 ? next % syncdelay : 0);
	/* XXX - vn_syncer_add_to_worklist() also grabs and drops sync_mtx. */
	mtx_lock(&sync_mtx);
	sync_vnode_count++;
	if (mp->mnt_syncer == NULL) {
		mp->mnt_syncer = vp;
		vp = NULL;
	}
	mtx_unlock(&sync_mtx);
	BO_UNLOCK(bo);
	if (vp != NULL) {
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		vgone(vp);
		vput(vp);
	}
}

void
vfs_deallocate_syncvnode(struct mount *mp)
{
	struct vnode *vp;

	mtx_lock(&sync_mtx);
	vp = mp->mnt_syncer;
	if (vp != NULL)
		mp->mnt_syncer = NULL;
	mtx_unlock(&sync_mtx);
	if (vp != NULL)
		vrele(vp);
}

/*
 * Do a lazy sync of the filesystem.
 */
static int
sync_fsync(struct vop_fsync_args *ap)
{
	struct vnode *syncvp = ap->a_vp;
	struct mount *mp = syncvp->v_mount;
	int error, save;
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
	if (vfs_busy(mp, MBF_NOWAIT) != 0)
		return (0);
	VOP_UNLOCK(syncvp);
	save = curthread_pflags_set(TDP_SYNCIO);
	/*
	 * The filesystem at hand may be idle with free vnodes stored in the
	 * batch.  Return them instead of letting them stay there indefinitely.
	 */
	vfs_periodic(mp, MNT_NOWAIT);
	error = VFS_SYNC(mp, MNT_LAZY);
	curthread_pflags_restore(save);
	vn_lock(syncvp, LK_EXCLUSIVE | LK_RETRY);
	vfs_unbusy(mp);
	return (error);
}

/*
 * The syncer vnode is no referenced.
 */
static int
sync_inactive(struct vop_inactive_args *ap)
{

	vgone(ap->a_vp);
	return (0);
}

/*
 * The syncer vnode is no longer needed and is being decommissioned.
 *
 * Modifications to the worklist must be protected by sync_mtx.
 */
static int
sync_reclaim(struct vop_reclaim_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct bufobj *bo;

	bo = &vp->v_bufobj;
	BO_LOCK(bo);
	mtx_lock(&sync_mtx);
	if (vp->v_mount->mnt_syncer == vp)
		vp->v_mount->mnt_syncer = NULL;
	if (bo->bo_flag & BO_ONWORKLST) {
		LIST_REMOVE(bo, bo_synclist);
		syncer_worklist_len--;
		sync_vnode_count--;
		bo->bo_flag &= ~BO_ONWORKLST;
	}
	mtx_unlock(&sync_mtx);
	BO_UNLOCK(bo);

	return (0);
}

int
vn_need_pageq_flush(struct vnode *vp)
{
	struct vm_object *obj;

	obj = vp->v_object;
	return (obj != NULL && (vp->v_vflag & VV_NOSYNC) == 0 &&
	    vm_object_mightbedirty(obj));
}

/*
 * Check if vnode represents a disk device
 */
bool
vn_isdisk_error(struct vnode *vp, int *errp)
{
	int error;

	if (vp->v_type != VCHR) {
		error = ENOTBLK;
		goto out;
	}
	error = 0;
	dev_lock();
	if (vp->v_rdev == NULL)
		error = ENXIO;
	else if (vp->v_rdev->si_devsw == NULL)
		error = ENXIO;
	else if (!(vp->v_rdev->si_devsw->d_flags & D_DISK))
		error = ENOTBLK;
	dev_unlock();
out:
	*errp = error;
	return (error == 0);
}

bool
vn_isdisk(struct vnode *vp)
{
	int error;

	return (vn_isdisk_error(vp, &error));
}

/*
 * VOP_FPLOOKUP_VEXEC routines are subject to special circumstances, see
 * the comment above cache_fplookup for details.
 */
int
vaccess_vexec_smr(mode_t file_mode, uid_t file_uid, gid_t file_gid, struct ucred *cred)
{
	int error;

	VFS_SMR_ASSERT_ENTERED();

	/* Check the owner. */
	if (cred->cr_uid == file_uid) {
		if (file_mode & S_IXUSR)
			return (0);
		goto out_error;
	}

	/* Otherwise, check the groups (first match) */
	if (groupmember(file_gid, cred)) {
		if (file_mode & S_IXGRP)
			return (0);
		goto out_error;
	}

	/* Otherwise, check everyone else. */
	if (file_mode & S_IXOTH)
		return (0);
out_error:
	/*
	 * Permission check failed, but it is possible denial will get overwritten
	 * (e.g., when root is traversing through a 700 directory owned by someone
	 * else).
	 *
	 * vaccess() calls priv_check_cred which in turn can descent into MAC
	 * modules overriding this result. It's quite unclear what semantics
	 * are allowed for them to operate, thus for safety we don't call them
	 * from within the SMR section. This also means if any such modules
	 * are present, we have to let the regular lookup decide.
	 */
	error = priv_check_cred_vfs_lookup_nomac(cred);
	switch (error) {
	case 0:
		return (0);
	case EAGAIN:
		/*
		 * MAC modules present.
		 */
		return (EAGAIN);
	case EPERM:
		return (EACCES);
	default:
		return (error);
	}
}

/*
 * Common filesystem object access control check routine.  Accepts a
 * vnode's type, "mode", uid and gid, requested access mode, and credentials.
 * Returns 0 on success, or an errno on failure.
 */
int
vaccess(__enum_uint8(vtype) type, mode_t file_mode, uid_t file_uid, gid_t file_gid,
    accmode_t accmode, struct ucred *cred)
{
	accmode_t dac_granted;
	accmode_t priv_granted;

	KASSERT((accmode & ~(VEXEC | VWRITE | VREAD | VADMIN | VAPPEND)) == 0,
	    ("invalid bit in accmode"));
	KASSERT((accmode & VAPPEND) == 0 || (accmode & VWRITE),
	    ("VAPPEND without VWRITE"));

	/*
	 * Look for a normal, non-privileged way to access the file/directory
	 * as requested.  If it exists, go with that.
	 */

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

		if ((accmode & dac_granted) == accmode)
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

		if ((accmode & dac_granted) == accmode)
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
	if ((accmode & dac_granted) == accmode)
		return (0);

privcheck:
	/*
	 * Build a privilege mask to determine if the set of privileges
	 * satisfies the requirements when combined with the granted mask
	 * from above.  For each privilege, if the privilege is required,
	 * bitwise or the request type onto the priv_granted mask.
	 */
	priv_granted = 0;

	if (type == VDIR) {
		/*
		 * For directories, use PRIV_VFS_LOOKUP to satisfy VEXEC
		 * requests, instead of PRIV_VFS_EXEC.
		 */
		if ((accmode & VEXEC) && ((dac_granted & VEXEC) == 0) &&
		    !priv_check_cred(cred, PRIV_VFS_LOOKUP))
			priv_granted |= VEXEC;
	} else {
		/*
		 * Ensure that at least one execute bit is on. Otherwise,
		 * a privileged user will always succeed, and we don't want
		 * this to happen unless the file really is executable.
		 */
		if ((accmode & VEXEC) && ((dac_granted & VEXEC) == 0) &&
		    (file_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0 &&
		    !priv_check_cred(cred, PRIV_VFS_EXEC))
			priv_granted |= VEXEC;
	}

	if ((accmode & VREAD) && ((dac_granted & VREAD) == 0) &&
	    !priv_check_cred(cred, PRIV_VFS_READ))
		priv_granted |= VREAD;

	if ((accmode & VWRITE) && ((dac_granted & VWRITE) == 0) &&
	    !priv_check_cred(cred, PRIV_VFS_WRITE))
		priv_granted |= (VWRITE | VAPPEND);

	if ((accmode & VADMIN) && ((dac_granted & VADMIN) == 0) &&
	    !priv_check_cred(cred, PRIV_VFS_ADMIN))
		priv_granted |= VADMIN;

	if ((accmode & (priv_granted | dac_granted)) == accmode) {
		return (0);
	}

	return ((accmode & VADMIN) ? EPERM : EACCES);
}

/*
 * Credential check based on process requesting service, and per-attribute
 * permissions.
 */
int
extattr_check_cred(struct vnode *vp, int attrnamespace, struct ucred *cred,
    struct thread *td, accmode_t accmode)
{

	/*
	 * Kernel-invoked always succeeds.
	 */
	if (cred == NOCRED)
		return (0);

	/*
	 * Do not allow privileged processes in jail to directly manipulate
	 * system attributes.
	 */
	switch (attrnamespace) {
	case EXTATTR_NAMESPACE_SYSTEM:
		/* Potentially should be: return (EPERM); */
		return (priv_check_cred(cred, PRIV_VFS_EXTATTR_SYSTEM));
	case EXTATTR_NAMESPACE_USER:
		return (VOP_ACCESS(vp, accmode, cred, td));
	default:
		return (EPERM);
	}
}

#ifdef DEBUG_VFS_LOCKS
int vfs_badlock_ddb = 1;	/* Drop into debugger on violation. */
SYSCTL_INT(_debug, OID_AUTO, vfs_badlock_ddb, CTLFLAG_RW, &vfs_badlock_ddb, 0,
    "Drop into debugger on lock violation");

int vfs_badlock_mutex = 1;	/* Check for interlock across VOPs. */
SYSCTL_INT(_debug, OID_AUTO, vfs_badlock_mutex, CTLFLAG_RW, &vfs_badlock_mutex,
    0, "Check for interlock across VOPs");

int vfs_badlock_print = 1;	/* Print lock violations. */
SYSCTL_INT(_debug, OID_AUTO, vfs_badlock_print, CTLFLAG_RW, &vfs_badlock_print,
    0, "Print lock violations");

int vfs_badlock_vnode = 1;	/* Print vnode details on lock violations. */
SYSCTL_INT(_debug, OID_AUTO, vfs_badlock_vnode, CTLFLAG_RW, &vfs_badlock_vnode,
    0, "Print vnode details on lock violations");

#ifdef KDB
int vfs_badlock_backtrace = 1;	/* Print backtrace at lock violations. */
SYSCTL_INT(_debug, OID_AUTO, vfs_badlock_backtrace, CTLFLAG_RW,
    &vfs_badlock_backtrace, 0, "Print backtrace at lock violations");
#endif

static void
vfs_badlock(const char *msg, const char *str, struct vnode *vp)
{

#ifdef KDB
	if (vfs_badlock_backtrace)
		kdb_backtrace();
#endif
	if (vfs_badlock_vnode)
		vn_printf(vp, "vnode ");
	if (vfs_badlock_print)
		printf("%s: %p %s\n", str, (void *)vp, msg);
	if (vfs_badlock_ddb)
		kdb_enter(KDB_WHY_VFSLOCK, "lock violation");
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
	if (KERNEL_PANICKED() || vp == NULL)
		return;

#ifdef WITNESS
	if ((vp->v_irflag & VIRF_CROSSMP) == 0 &&
	    witness_is_owned(&vp->v_vnlock->lock_object) == -1)
#else
	int locked = VOP_ISLOCKED(vp);
	if (locked == 0 || locked == LK_EXCLOTHER)
#endif
		vfs_badlock("is not locked but should be", str, vp);
}

void
assert_vop_unlocked(struct vnode *vp, const char *str)
{
	if (KERNEL_PANICKED() || vp == NULL)
		return;

#ifdef WITNESS
	if ((vp->v_irflag & VIRF_CROSSMP) == 0 &&
	    witness_is_owned(&vp->v_vnlock->lock_object) == 1)
#else
	if (VOP_ISLOCKED(vp) == LK_EXCLUSIVE)
#endif
		vfs_badlock("is locked but should not be", str, vp);
}

void
assert_vop_elocked(struct vnode *vp, const char *str)
{
	if (KERNEL_PANICKED() || vp == NULL)
		return;

	if (VOP_ISLOCKED(vp) != LK_EXCLUSIVE)
		vfs_badlock("is not exclusive locked but should be", str, vp);
}
#endif /* DEBUG_VFS_LOCKS */

void
vop_rename_fail(struct vop_rename_args *ap)
{

	if (ap->a_tvp != NULL)
		vput(ap->a_tvp);
	if (ap->a_tdvp == ap->a_tvp)
		vrele(ap->a_tdvp);
	else
		vput(ap->a_tdvp);
	vrele(ap->a_fdvp);
	vrele(ap->a_fvp);
}

void
vop_rename_pre(void *ap)
{
	struct vop_rename_args *a = ap;

#ifdef DEBUG_VFS_LOCKS
	if (a->a_tvp)
		ASSERT_VI_UNLOCKED(a->a_tvp, "VOP_RENAME");
	ASSERT_VI_UNLOCKED(a->a_tdvp, "VOP_RENAME");
	ASSERT_VI_UNLOCKED(a->a_fvp, "VOP_RENAME");
	ASSERT_VI_UNLOCKED(a->a_fdvp, "VOP_RENAME");

	/* Check the source (from). */
	if (a->a_tdvp->v_vnlock != a->a_fdvp->v_vnlock &&
	    (a->a_tvp == NULL || a->a_tvp->v_vnlock != a->a_fdvp->v_vnlock))
		ASSERT_VOP_UNLOCKED(a->a_fdvp, "vop_rename: fdvp locked");
	if (a->a_tvp == NULL || a->a_tvp->v_vnlock != a->a_fvp->v_vnlock)
		ASSERT_VOP_UNLOCKED(a->a_fvp, "vop_rename: fvp locked");

	/* Check the target. */
	if (a->a_tvp)
		ASSERT_VOP_LOCKED(a->a_tvp, "vop_rename: tvp not locked");
	ASSERT_VOP_LOCKED(a->a_tdvp, "vop_rename: tdvp not locked");
#endif
	/*
	 * It may be tempting to add vn_seqc_write_begin/end calls here and
	 * in vop_rename_post but that's not going to work out since some
	 * filesystems relookup vnodes mid-rename. This is probably a bug.
	 *
	 * For now filesystems are expected to do the relevant calls after they
	 * decide what vnodes to operate on.
	 */
	if (a->a_tdvp != a->a_fdvp)
		vhold(a->a_fdvp);
	if (a->a_tvp != a->a_fvp)
		vhold(a->a_fvp);
	vhold(a->a_tdvp);
	if (a->a_tvp)
		vhold(a->a_tvp);
}

#ifdef DEBUG_VFS_LOCKS
void
vop_fplookup_vexec_debugpre(void *ap __unused)
{

	VFS_SMR_ASSERT_ENTERED();
}

void
vop_fplookup_vexec_debugpost(void *ap, int rc)
{
	struct vop_fplookup_vexec_args *a;
	struct vnode *vp;

	a = ap;
	vp = a->a_vp;

	VFS_SMR_ASSERT_ENTERED();
	if (rc == EOPNOTSUPP)
		VNPASS(VN_IS_DOOMED(vp), vp);
}

void
vop_fplookup_symlink_debugpre(void *ap __unused)
{

	VFS_SMR_ASSERT_ENTERED();
}

void
vop_fplookup_symlink_debugpost(void *ap __unused, int rc __unused)
{

	VFS_SMR_ASSERT_ENTERED();
}

static void
vop_fsync_debugprepost(struct vnode *vp, const char *name)
{
	if (vp->v_type == VCHR)
		;
	else if (MNT_EXTENDED_SHARED(vp->v_mount))
		ASSERT_VOP_LOCKED(vp, name);
	else
		ASSERT_VOP_ELOCKED(vp, name);
}

void
vop_fsync_debugpre(void *a)
{
	struct vop_fsync_args *ap;

	ap = a;
	vop_fsync_debugprepost(ap->a_vp, "fsync");
}

void
vop_fsync_debugpost(void *a, int rc __unused)
{
	struct vop_fsync_args *ap;

	ap = a;
	vop_fsync_debugprepost(ap->a_vp, "fsync");
}

void
vop_fdatasync_debugpre(void *a)
{
	struct vop_fdatasync_args *ap;

	ap = a;
	vop_fsync_debugprepost(ap->a_vp, "fsync");
}

void
vop_fdatasync_debugpost(void *a, int rc __unused)
{
	struct vop_fdatasync_args *ap;

	ap = a;
	vop_fsync_debugprepost(ap->a_vp, "fsync");
}

void
vop_strategy_debugpre(void *ap)
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

	if (!KERNEL_PANICKED() && !BUF_ISLOCKED(bp)) {
		if (vfs_badlock_print)
			printf(
			    "VOP_STRATEGY: bp is not locked but should be\n");
		if (vfs_badlock_ddb)
			kdb_enter(KDB_WHY_VFSLOCK, "lock violation");
	}
}

void
vop_lock_debugpre(void *ap)
{
	struct vop_lock1_args *a = ap;

	if ((a->a_flags & LK_INTERLOCK) == 0)
		ASSERT_VI_UNLOCKED(a->a_vp, "VOP_LOCK");
	else
		ASSERT_VI_LOCKED(a->a_vp, "VOP_LOCK");
}

void
vop_lock_debugpost(void *ap, int rc)
{
	struct vop_lock1_args *a = ap;

	ASSERT_VI_UNLOCKED(a->a_vp, "VOP_LOCK");
	if (rc == 0 && (a->a_flags & LK_EXCLOTHER) == 0)
		ASSERT_VOP_LOCKED(a->a_vp, "VOP_LOCK");
}

void
vop_unlock_debugpre(void *ap)
{
	struct vop_unlock_args *a = ap;
	struct vnode *vp = a->a_vp;

	VNPASS(vn_get_state(vp) != VSTATE_UNINITIALIZED, vp);
	ASSERT_VOP_LOCKED(vp, "VOP_UNLOCK");
}

void
vop_need_inactive_debugpre(void *ap)
{
	struct vop_need_inactive_args *a = ap;

	ASSERT_VI_LOCKED(a->a_vp, "VOP_NEED_INACTIVE");
}

void
vop_need_inactive_debugpost(void *ap, int rc)
{
	struct vop_need_inactive_args *a = ap;

	ASSERT_VI_LOCKED(a->a_vp, "VOP_NEED_INACTIVE");
}
#endif

void
vop_create_pre(void *ap)
{
	struct vop_create_args *a;
	struct vnode *dvp;

	a = ap;
	dvp = a->a_dvp;
	vn_seqc_write_begin(dvp);
}

void
vop_create_post(void *ap, int rc)
{
	struct vop_create_args *a;
	struct vnode *dvp;

	a = ap;
	dvp = a->a_dvp;
	vn_seqc_write_end(dvp);
	if (!rc)
		VFS_KNOTE_LOCKED(dvp, NOTE_WRITE);
}

void
vop_whiteout_pre(void *ap)
{
	struct vop_whiteout_args *a;
	struct vnode *dvp;

	a = ap;
	dvp = a->a_dvp;
	vn_seqc_write_begin(dvp);
}

void
vop_whiteout_post(void *ap, int rc)
{
	struct vop_whiteout_args *a;
	struct vnode *dvp;

	a = ap;
	dvp = a->a_dvp;
	vn_seqc_write_end(dvp);
}

void
vop_deleteextattr_pre(void *ap)
{
	struct vop_deleteextattr_args *a;
	struct vnode *vp;

	a = ap;
	vp = a->a_vp;
	vn_seqc_write_begin(vp);
}

void
vop_deleteextattr_post(void *ap, int rc)
{
	struct vop_deleteextattr_args *a;
	struct vnode *vp;

	a = ap;
	vp = a->a_vp;
	vn_seqc_write_end(vp);
	if (!rc)
		VFS_KNOTE_LOCKED(a->a_vp, NOTE_ATTRIB);
}

void
vop_link_pre(void *ap)
{
	struct vop_link_args *a;
	struct vnode *vp, *tdvp;

	a = ap;
	vp = a->a_vp;
	tdvp = a->a_tdvp;
	vn_seqc_write_begin(vp);
	vn_seqc_write_begin(tdvp);
}

void
vop_link_post(void *ap, int rc)
{
	struct vop_link_args *a;
	struct vnode *vp, *tdvp;

	a = ap;
	vp = a->a_vp;
	tdvp = a->a_tdvp;
	vn_seqc_write_end(vp);
	vn_seqc_write_end(tdvp);
	if (!rc) {
		VFS_KNOTE_LOCKED(vp, NOTE_LINK);
		VFS_KNOTE_LOCKED(tdvp, NOTE_WRITE);
	}
}

void
vop_mkdir_pre(void *ap)
{
	struct vop_mkdir_args *a;
	struct vnode *dvp;

	a = ap;
	dvp = a->a_dvp;
	vn_seqc_write_begin(dvp);
}

void
vop_mkdir_post(void *ap, int rc)
{
	struct vop_mkdir_args *a;
	struct vnode *dvp;

	a = ap;
	dvp = a->a_dvp;
	vn_seqc_write_end(dvp);
	if (!rc)
		VFS_KNOTE_LOCKED(dvp, NOTE_WRITE | NOTE_LINK);
}

#ifdef DEBUG_VFS_LOCKS
void
vop_mkdir_debugpost(void *ap, int rc)
{
	struct vop_mkdir_args *a;

	a = ap;
	if (!rc)
		cache_validate(a->a_dvp, *a->a_vpp, a->a_cnp);
}
#endif

void
vop_mknod_pre(void *ap)
{
	struct vop_mknod_args *a;
	struct vnode *dvp;

	a = ap;
	dvp = a->a_dvp;
	vn_seqc_write_begin(dvp);
}

void
vop_mknod_post(void *ap, int rc)
{
	struct vop_mknod_args *a;
	struct vnode *dvp;

	a = ap;
	dvp = a->a_dvp;
	vn_seqc_write_end(dvp);
	if (!rc)
		VFS_KNOTE_LOCKED(dvp, NOTE_WRITE);
}

void
vop_reclaim_post(void *ap, int rc)
{
	struct vop_reclaim_args *a;
	struct vnode *vp;

	a = ap;
	vp = a->a_vp;
	ASSERT_VOP_IN_SEQC(vp);
	if (!rc)
		VFS_KNOTE_LOCKED(vp, NOTE_REVOKE);
}

void
vop_remove_pre(void *ap)
{
	struct vop_remove_args *a;
	struct vnode *dvp, *vp;

	a = ap;
	dvp = a->a_dvp;
	vp = a->a_vp;
	vn_seqc_write_begin(dvp);
	vn_seqc_write_begin(vp);
}

void
vop_remove_post(void *ap, int rc)
{
	struct vop_remove_args *a;
	struct vnode *dvp, *vp;

	a = ap;
	dvp = a->a_dvp;
	vp = a->a_vp;
	vn_seqc_write_end(dvp);
	vn_seqc_write_end(vp);
	if (!rc) {
		VFS_KNOTE_LOCKED(dvp, NOTE_WRITE);
		VFS_KNOTE_LOCKED(vp, NOTE_DELETE);
	}
}

void
vop_rename_post(void *ap, int rc)
{
	struct vop_rename_args *a = ap;
	long hint;

	if (!rc) {
		hint = NOTE_WRITE;
		if (a->a_fdvp == a->a_tdvp) {
			if (a->a_tvp != NULL && a->a_tvp->v_type == VDIR)
				hint |= NOTE_LINK;
			VFS_KNOTE_UNLOCKED(a->a_fdvp, hint);
			VFS_KNOTE_UNLOCKED(a->a_tdvp, hint);
		} else {
			hint |= NOTE_EXTEND;
			if (a->a_fvp->v_type == VDIR)
				hint |= NOTE_LINK;
			VFS_KNOTE_UNLOCKED(a->a_fdvp, hint);

			if (a->a_fvp->v_type == VDIR && a->a_tvp != NULL &&
			    a->a_tvp->v_type == VDIR)
				hint &= ~NOTE_LINK;
			VFS_KNOTE_UNLOCKED(a->a_tdvp, hint);
		}

		VFS_KNOTE_UNLOCKED(a->a_fvp, NOTE_RENAME);
		if (a->a_tvp)
			VFS_KNOTE_UNLOCKED(a->a_tvp, NOTE_DELETE);
	}
	if (a->a_tdvp != a->a_fdvp)
		vdrop(a->a_fdvp);
	if (a->a_tvp != a->a_fvp)
		vdrop(a->a_fvp);
	vdrop(a->a_tdvp);
	if (a->a_tvp)
		vdrop(a->a_tvp);
}

void
vop_rmdir_pre(void *ap)
{
	struct vop_rmdir_args *a;
	struct vnode *dvp, *vp;

	a = ap;
	dvp = a->a_dvp;
	vp = a->a_vp;
	vn_seqc_write_begin(dvp);
	vn_seqc_write_begin(vp);
}

void
vop_rmdir_post(void *ap, int rc)
{
	struct vop_rmdir_args *a;
	struct vnode *dvp, *vp;

	a = ap;
	dvp = a->a_dvp;
	vp = a->a_vp;
	vn_seqc_write_end(dvp);
	vn_seqc_write_end(vp);
	if (!rc) {
		vp->v_vflag |= VV_UNLINKED;
		VFS_KNOTE_LOCKED(dvp, NOTE_WRITE | NOTE_LINK);
		VFS_KNOTE_LOCKED(vp, NOTE_DELETE);
	}
}

void
vop_setattr_pre(void *ap)
{
	struct vop_setattr_args *a;
	struct vnode *vp;

	a = ap;
	vp = a->a_vp;
	vn_seqc_write_begin(vp);
}

void
vop_setattr_post(void *ap, int rc)
{
	struct vop_setattr_args *a;
	struct vnode *vp;

	a = ap;
	vp = a->a_vp;
	vn_seqc_write_end(vp);
	if (!rc)
		VFS_KNOTE_LOCKED(vp, NOTE_ATTRIB);
}

void
vop_setacl_pre(void *ap)
{
	struct vop_setacl_args *a;
	struct vnode *vp;

	a = ap;
	vp = a->a_vp;
	vn_seqc_write_begin(vp);
}

void
vop_setacl_post(void *ap, int rc __unused)
{
	struct vop_setacl_args *a;
	struct vnode *vp;

	a = ap;
	vp = a->a_vp;
	vn_seqc_write_end(vp);
}

void
vop_setextattr_pre(void *ap)
{
	struct vop_setextattr_args *a;
	struct vnode *vp;

	a = ap;
	vp = a->a_vp;
	vn_seqc_write_begin(vp);
}

void
vop_setextattr_post(void *ap, int rc)
{
	struct vop_setextattr_args *a;
	struct vnode *vp;

	a = ap;
	vp = a->a_vp;
	vn_seqc_write_end(vp);
	if (!rc)
		VFS_KNOTE_LOCKED(vp, NOTE_ATTRIB);
}

void
vop_symlink_pre(void *ap)
{
	struct vop_symlink_args *a;
	struct vnode *dvp;

	a = ap;
	dvp = a->a_dvp;
	vn_seqc_write_begin(dvp);
}

void
vop_symlink_post(void *ap, int rc)
{
	struct vop_symlink_args *a;
	struct vnode *dvp;

	a = ap;
	dvp = a->a_dvp;
	vn_seqc_write_end(dvp);
	if (!rc)
		VFS_KNOTE_LOCKED(dvp, NOTE_WRITE);
}

void
vop_open_post(void *ap, int rc)
{
	struct vop_open_args *a = ap;

	if (!rc)
		VFS_KNOTE_LOCKED(a->a_vp, NOTE_OPEN);
}

void
vop_close_post(void *ap, int rc)
{
	struct vop_close_args *a = ap;

	if (!rc && (a->a_cred != NOCRED || /* filter out revokes */
	    !VN_IS_DOOMED(a->a_vp))) {
		VFS_KNOTE_LOCKED(a->a_vp, (a->a_fflag & FWRITE) != 0 ?
		    NOTE_CLOSE_WRITE : NOTE_CLOSE);
	}
}

void
vop_read_post(void *ap, int rc)
{
	struct vop_read_args *a = ap;

	if (!rc)
		VFS_KNOTE_LOCKED(a->a_vp, NOTE_READ);
}

void
vop_read_pgcache_post(void *ap, int rc)
{
	struct vop_read_pgcache_args *a = ap;

	if (!rc)
		VFS_KNOTE_UNLOCKED(a->a_vp, NOTE_READ);
}

void
vop_readdir_post(void *ap, int rc)
{
	struct vop_readdir_args *a = ap;

	if (!rc)
		VFS_KNOTE_LOCKED(a->a_vp, NOTE_READ);
}

static struct knlist fs_knlist;

static void
vfs_event_init(void *arg)
{
	knlist_init_mtx(&fs_knlist, NULL);
}
/* XXX - correct order? */
SYSINIT(vfs_knlist, SI_SUB_VFS, SI_ORDER_ANY, vfs_event_init, NULL);

void
vfs_event_signal(fsid_t *fsid, uint32_t event, intptr_t data __unused)
{

	KNOTE_UNLOCKED(&fs_knlist, event);
}

static int	filt_fsattach(struct knote *kn);
static void	filt_fsdetach(struct knote *kn);
static int	filt_fsevent(struct knote *kn, long hint);

struct filterops fs_filtops = {
	.f_isfd = 0,
	.f_attach = filt_fsattach,
	.f_detach = filt_fsdetach,
	.f_event = filt_fsevent
};

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

	kn->kn_fflags |= kn->kn_sfflags & hint;

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
		vfs_rel(mp);
		return (EINVAL);
	}
	VCTLTOREQ(&vc, req);
	error = VFS_SYSCTL(mp, vc.vc_op, req);
	vfs_rel(mp);
	return (error);
}

SYSCTL_PROC(_vfs, OID_AUTO, ctl, CTLTYPE_OPAQUE | CTLFLAG_MPSAFE | CTLFLAG_WR,
    NULL, 0, sysctl_vfs_ctl, "",
    "Sysctl by fsid");

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

static int	filt_vfsread(struct knote *kn, long hint);
static int	filt_vfswrite(struct knote *kn, long hint);
static int	filt_vfsvnode(struct knote *kn, long hint);
static void	filt_vfsdetach(struct knote *kn);
static struct filterops vfsread_filtops = {
	.f_isfd = 1,
	.f_detach = filt_vfsdetach,
	.f_event = filt_vfsread
};
static struct filterops vfswrite_filtops = {
	.f_isfd = 1,
	.f_detach = filt_vfsdetach,
	.f_event = filt_vfswrite
};
static struct filterops vfsvnode_filtops = {
	.f_isfd = 1,
	.f_detach = filt_vfsdetach,
	.f_event = filt_vfsvnode
};

static void
vfs_knllock(void *arg)
{
	struct vnode *vp = arg;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
}

static void
vfs_knlunlock(void *arg)
{
	struct vnode *vp = arg;

	VOP_UNLOCK(vp);
}

static void
vfs_knl_assert_lock(void *arg, int what)
{
#ifdef DEBUG_VFS_LOCKS
	struct vnode *vp = arg;

	if (what == LA_LOCKED)
		ASSERT_VOP_LOCKED(vp, "vfs_knl_assert_locked");
	else
		ASSERT_VOP_UNLOCKED(vp, "vfs_knl_assert_unlocked");
#endif
}

int
vfs_kqfilter(struct vop_kqfilter_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct knote *kn = ap->a_kn;
	struct knlist *knl;

	KASSERT(vp->v_type != VFIFO || (kn->kn_filter != EVFILT_READ &&
	    kn->kn_filter != EVFILT_WRITE),
	    ("READ/WRITE filter on a FIFO leaked through"));
	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &vfsread_filtops;
		break;
	case EVFILT_WRITE:
		kn->kn_fop = &vfswrite_filtops;
		break;
	case EVFILT_VNODE:
		kn->kn_fop = &vfsvnode_filtops;
		break;
	default:
		return (EINVAL);
	}

	kn->kn_hook = (caddr_t)vp;

	v_addpollinfo(vp);
	if (vp->v_pollinfo == NULL)
		return (ENOMEM);
	knl = &vp->v_pollinfo->vpi_selinfo.si_note;
	vhold(vp);
	knlist_add(knl, kn, 0);

	return (0);
}

/*
 * Detach knote from vnode
 */
static void
filt_vfsdetach(struct knote *kn)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;

	KASSERT(vp->v_pollinfo != NULL, ("Missing v_pollinfo"));
	knlist_remove(&vp->v_pollinfo->vpi_selinfo.si_note, kn, 0);
	vdrop(vp);
}

/*ARGSUSED*/
static int
filt_vfsread(struct knote *kn, long hint)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;
	off_t size;
	int res;

	/*
	 * filesystem is gone, so set the EOF flag and schedule
	 * the knote for deletion.
	 */
	if (hint == NOTE_REVOKE || (hint == 0 && vp->v_type == VBAD)) {
		VI_LOCK(vp);
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		VI_UNLOCK(vp);
		return (1);
	}

	if (vn_getsize_locked(vp, &size, curthread->td_ucred) != 0)
		return (0);

	VI_LOCK(vp);
	kn->kn_data = size - kn->kn_fp->f_offset;
	res = (kn->kn_sfflags & NOTE_FILE_POLL) != 0 || kn->kn_data != 0;
	VI_UNLOCK(vp);
	return (res);
}

/*ARGSUSED*/
static int
filt_vfswrite(struct knote *kn, long hint)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;

	VI_LOCK(vp);

	/*
	 * filesystem is gone, so set the EOF flag and schedule
	 * the knote for deletion.
	 */
	if (hint == NOTE_REVOKE || (hint == 0 && vp->v_type == VBAD))
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);

	kn->kn_data = 0;
	VI_UNLOCK(vp);
	return (1);
}

static int
filt_vfsvnode(struct knote *kn, long hint)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;
	int res;

	VI_LOCK(vp);
	if (kn->kn_sfflags & hint)
		kn->kn_fflags |= hint;
	if (hint == NOTE_REVOKE || (hint == 0 && vp->v_type == VBAD)) {
		kn->kn_flags |= EV_EOF;
		VI_UNLOCK(vp);
		return (1);
	}
	res = (kn->kn_fflags != 0);
	VI_UNLOCK(vp);
	return (res);
}

int
vfs_read_dirent(struct vop_readdir_args *ap, struct dirent *dp, off_t off)
{
	int error;

	if (dp->d_reclen > ap->a_uio->uio_resid)
		return (ENAMETOOLONG);
	error = uiomove(dp, dp->d_reclen, ap->a_uio);
	if (error) {
		if (ap->a_ncookies != NULL) {
			if (ap->a_cookies != NULL)
				free(ap->a_cookies, M_TEMP);
			ap->a_cookies = NULL;
			*ap->a_ncookies = 0;
		}
		return (error);
	}
	if (ap->a_ncookies == NULL)
		return (0);

	KASSERT(ap->a_cookies,
	    ("NULL ap->a_cookies value with non-NULL ap->a_ncookies!"));

	*ap->a_cookies = realloc(*ap->a_cookies,
	    (*ap->a_ncookies + 1) * sizeof(uint64_t), M_TEMP, M_WAITOK | M_ZERO);
	(*ap->a_cookies)[*ap->a_ncookies] = off;
	*ap->a_ncookies += 1;
	return (0);
}

/*
 * The purpose of this routine is to remove granularity from accmode_t,
 * reducing it into standard unix access bits - VEXEC, VREAD, VWRITE,
 * VADMIN and VAPPEND.
 *
 * If it returns 0, the caller is supposed to continue with the usual
 * access checks using 'accmode' as modified by this routine.  If it
 * returns nonzero value, the caller is supposed to return that value
 * as errno.
 *
 * Note that after this routine runs, accmode may be zero.
 */
int
vfs_unixify_accmode(accmode_t *accmode)
{
	/*
	 * There is no way to specify explicit "deny" rule using
	 * file mode or POSIX.1e ACLs.
	 */
	if (*accmode & VEXPLICIT_DENY) {
		*accmode = 0;
		return (0);
	}

	/*
	 * None of these can be translated into usual access bits.
	 * Also, the common case for NFSv4 ACLs is to not contain
	 * either of these bits. Caller should check for VWRITE
	 * on the containing directory instead.
	 */
	if (*accmode & (VDELETE_CHILD | VDELETE))
		return (EPERM);

	if (*accmode & VADMIN_PERMS) {
		*accmode &= ~VADMIN_PERMS;
		*accmode |= VADMIN;
	}

	/*
	 * There is no way to deny VREAD_ATTRIBUTES, VREAD_ACL
	 * or VSYNCHRONIZE using file mode or POSIX.1e ACL.
	 */
	*accmode &= ~(VSTAT_PERMS | VSYNCHRONIZE);

	return (0);
}

/*
 * Clear out a doomed vnode (if any) and replace it with a new one as long
 * as the fs is not being unmounted. Return the root vnode to the caller.
 */
static int __noinline
vfs_cache_root_fallback(struct mount *mp, int flags, struct vnode **vpp)
{
	struct vnode *vp;
	int error;

restart:
	if (mp->mnt_rootvnode != NULL) {
		MNT_ILOCK(mp);
		vp = mp->mnt_rootvnode;
		if (vp != NULL) {
			if (!VN_IS_DOOMED(vp)) {
				vrefact(vp);
				MNT_IUNLOCK(mp);
				error = vn_lock(vp, flags);
				if (error == 0) {
					*vpp = vp;
					return (0);
				}
				vrele(vp);
				goto restart;
			}
			/*
			 * Clear the old one.
			 */
			mp->mnt_rootvnode = NULL;
		}
		MNT_IUNLOCK(mp);
		if (vp != NULL) {
			vfs_op_barrier_wait(mp);
			vrele(vp);
		}
	}
	error = VFS_CACHEDROOT(mp, flags, vpp);
	if (error != 0)
		return (error);
	if (mp->mnt_vfs_ops == 0) {
		MNT_ILOCK(mp);
		if (mp->mnt_vfs_ops != 0) {
			MNT_IUNLOCK(mp);
			return (0);
		}
		if (mp->mnt_rootvnode == NULL) {
			vrefact(*vpp);
			mp->mnt_rootvnode = *vpp;
		} else {
			if (mp->mnt_rootvnode != *vpp) {
				if (!VN_IS_DOOMED(mp->mnt_rootvnode)) {
					panic("%s: mismatch between vnode returned "
					    " by VFS_CACHEDROOT and the one cached "
					    " (%p != %p)",
					    __func__, *vpp, mp->mnt_rootvnode);
				}
			}
		}
		MNT_IUNLOCK(mp);
	}
	return (0);
}

int
vfs_cache_root(struct mount *mp, int flags, struct vnode **vpp)
{
	struct mount_pcpu *mpcpu;
	struct vnode *vp;
	int error;

	if (!vfs_op_thread_enter(mp, mpcpu))
		return (vfs_cache_root_fallback(mp, flags, vpp));
	vp = atomic_load_ptr(&mp->mnt_rootvnode);
	if (vp == NULL || VN_IS_DOOMED(vp)) {
		vfs_op_thread_exit(mp, mpcpu);
		return (vfs_cache_root_fallback(mp, flags, vpp));
	}
	vrefact(vp);
	vfs_op_thread_exit(mp, mpcpu);
	error = vn_lock(vp, flags);
	if (error != 0) {
		vrele(vp);
		return (vfs_cache_root_fallback(mp, flags, vpp));
	}
	*vpp = vp;
	return (0);
}

struct vnode *
vfs_cache_root_clear(struct mount *mp)
{
	struct vnode *vp;

	/*
	 * ops > 0 guarantees there is nobody who can see this vnode
	 */
	MPASS(mp->mnt_vfs_ops > 0);
	vp = mp->mnt_rootvnode;
	if (vp != NULL)
		vn_seqc_write_begin(vp);
	mp->mnt_rootvnode = NULL;
	return (vp);
}

void
vfs_cache_root_set(struct mount *mp, struct vnode *vp)
{

	MPASS(mp->mnt_vfs_ops > 0);
	vrefact(vp);
	mp->mnt_rootvnode = vp;
}

/*
 * These are helper functions for filesystems to traverse all
 * their vnodes.  See MNT_VNODE_FOREACH_ALL() in sys/mount.h.
 *
 * This interface replaces MNT_VNODE_FOREACH.
 */

struct vnode *
__mnt_vnode_next_all(struct vnode **mvp, struct mount *mp)
{
	struct vnode *vp;

	maybe_yield();
	MNT_ILOCK(mp);
	KASSERT((*mvp)->v_mount == mp, ("marker vnode mount list mismatch"));
	for (vp = TAILQ_NEXT(*mvp, v_nmntvnodes); vp != NULL;
	    vp = TAILQ_NEXT(vp, v_nmntvnodes)) {
		/* Allow a racy peek at VIRF_DOOMED to save a lock acquisition. */
		if (vp->v_type == VMARKER || VN_IS_DOOMED(vp))
			continue;
		VI_LOCK(vp);
		if (VN_IS_DOOMED(vp)) {
			VI_UNLOCK(vp);
			continue;
		}
		break;
	}
	if (vp == NULL) {
		__mnt_vnode_markerfree_all(mvp, mp);
		/* MNT_IUNLOCK(mp); -- done in above function */
		mtx_assert(MNT_MTX(mp), MA_NOTOWNED);
		return (NULL);
	}
	TAILQ_REMOVE(&mp->mnt_nvnodelist, *mvp, v_nmntvnodes);
	TAILQ_INSERT_AFTER(&mp->mnt_nvnodelist, vp, *mvp, v_nmntvnodes);
	MNT_IUNLOCK(mp);
	return (vp);
}

struct vnode *
__mnt_vnode_first_all(struct vnode **mvp, struct mount *mp)
{
	struct vnode *vp;

	*mvp = vn_alloc_marker(mp);
	MNT_ILOCK(mp);
	MNT_REF(mp);

	TAILQ_FOREACH(vp, &mp->mnt_nvnodelist, v_nmntvnodes) {
		/* Allow a racy peek at VIRF_DOOMED to save a lock acquisition. */
		if (vp->v_type == VMARKER || VN_IS_DOOMED(vp))
			continue;
		VI_LOCK(vp);
		if (VN_IS_DOOMED(vp)) {
			VI_UNLOCK(vp);
			continue;
		}
		break;
	}
	if (vp == NULL) {
		MNT_REL(mp);
		MNT_IUNLOCK(mp);
		vn_free_marker(*mvp);
		*mvp = NULL;
		return (NULL);
	}
	TAILQ_INSERT_AFTER(&mp->mnt_nvnodelist, vp, *mvp, v_nmntvnodes);
	MNT_IUNLOCK(mp);
	return (vp);
}

void
__mnt_vnode_markerfree_all(struct vnode **mvp, struct mount *mp)
{

	if (*mvp == NULL) {
		MNT_IUNLOCK(mp);
		return;
	}

	mtx_assert(MNT_MTX(mp), MA_OWNED);

	KASSERT((*mvp)->v_mount == mp, ("marker vnode mount list mismatch"));
	TAILQ_REMOVE(&mp->mnt_nvnodelist, *mvp, v_nmntvnodes);
	MNT_REL(mp);
	MNT_IUNLOCK(mp);
	vn_free_marker(*mvp);
	*mvp = NULL;
}

/*
 * These are helper functions for filesystems to traverse their
 * lazy vnodes.  See MNT_VNODE_FOREACH_LAZY() in sys/mount.h
 */
static void
mnt_vnode_markerfree_lazy(struct vnode **mvp, struct mount *mp)
{

	KASSERT((*mvp)->v_mount == mp, ("marker vnode mount list mismatch"));

	MNT_ILOCK(mp);
	MNT_REL(mp);
	MNT_IUNLOCK(mp);
	vn_free_marker(*mvp);
	*mvp = NULL;
}

/*
 * Relock the mp mount vnode list lock with the vp vnode interlock in the
 * conventional lock order during mnt_vnode_next_lazy iteration.
 *
 * On entry, the mount vnode list lock is held and the vnode interlock is not.
 * The list lock is dropped and reacquired.  On success, both locks are held.
 * On failure, the mount vnode list lock is held but the vnode interlock is
 * not, and the procedure may have yielded.
 */
static bool
mnt_vnode_next_lazy_relock(struct vnode *mvp, struct mount *mp,
    struct vnode *vp)
{

	VNASSERT(mvp->v_mount == mp && mvp->v_type == VMARKER &&
	    TAILQ_NEXT(mvp, v_lazylist) != NULL, mvp,
	    ("%s: bad marker", __func__));
	VNASSERT(vp->v_mount == mp && vp->v_type != VMARKER, vp,
	    ("%s: inappropriate vnode", __func__));
	ASSERT_VI_UNLOCKED(vp, __func__);
	mtx_assert(&mp->mnt_listmtx, MA_OWNED);

	TAILQ_REMOVE(&mp->mnt_lazyvnodelist, mvp, v_lazylist);
	TAILQ_INSERT_BEFORE(vp, mvp, v_lazylist);

	/*
	 * Note we may be racing against vdrop which transitioned the hold
	 * count to 0 and now waits for the ->mnt_listmtx lock. This is fine,
	 * if we are the only user after we get the interlock we will just
	 * vdrop.
	 */
	vhold(vp);
	mtx_unlock(&mp->mnt_listmtx);
	VI_LOCK(vp);
	if (VN_IS_DOOMED(vp)) {
		VNPASS((vp->v_mflag & VMP_LAZYLIST) == 0, vp);
		goto out_lost;
	}
	VNPASS(vp->v_mflag & VMP_LAZYLIST, vp);
	/*
	 * There is nothing to do if we are the last user.
	 */
	if (!refcount_release_if_not_last(&vp->v_holdcnt))
		goto out_lost;
	mtx_lock(&mp->mnt_listmtx);
	return (true);
out_lost:
	vdropl(vp);
	maybe_yield();
	mtx_lock(&mp->mnt_listmtx);
	return (false);
}

static struct vnode *
mnt_vnode_next_lazy(struct vnode **mvp, struct mount *mp, mnt_lazy_cb_t *cb,
    void *cbarg)
{
	struct vnode *vp;

	mtx_assert(&mp->mnt_listmtx, MA_OWNED);
	KASSERT((*mvp)->v_mount == mp, ("marker vnode mount list mismatch"));
restart:
	vp = TAILQ_NEXT(*mvp, v_lazylist);
	while (vp != NULL) {
		if (vp->v_type == VMARKER) {
			vp = TAILQ_NEXT(vp, v_lazylist);
			continue;
		}
		/*
		 * See if we want to process the vnode. Note we may encounter a
		 * long string of vnodes we don't care about and hog the list
		 * as a result. Check for it and requeue the marker.
		 */
		VNPASS(!VN_IS_DOOMED(vp), vp);
		if (!cb(vp, cbarg)) {
			if (!should_yield()) {
				vp = TAILQ_NEXT(vp, v_lazylist);
				continue;
			}
			TAILQ_REMOVE(&mp->mnt_lazyvnodelist, *mvp,
			    v_lazylist);
			TAILQ_INSERT_AFTER(&mp->mnt_lazyvnodelist, vp, *mvp,
			    v_lazylist);
			mtx_unlock(&mp->mnt_listmtx);
			kern_yield(PRI_USER);
			mtx_lock(&mp->mnt_listmtx);
			goto restart;
		}
		/*
		 * Try-lock because this is the wrong lock order.
		 */
		if (!VI_TRYLOCK(vp) &&
		    !mnt_vnode_next_lazy_relock(*mvp, mp, vp))
			goto restart;
		KASSERT(vp->v_type != VMARKER, ("locked marker %p", vp));
		KASSERT(vp->v_mount == mp || vp->v_mount == NULL,
		    ("alien vnode on the lazy list %p %p", vp, mp));
		VNPASS(vp->v_mount == mp, vp);
		VNPASS(!VN_IS_DOOMED(vp), vp);
		break;
	}
	TAILQ_REMOVE(&mp->mnt_lazyvnodelist, *mvp, v_lazylist);

	/* Check if we are done */
	if (vp == NULL) {
		mtx_unlock(&mp->mnt_listmtx);
		mnt_vnode_markerfree_lazy(mvp, mp);
		return (NULL);
	}
	TAILQ_INSERT_AFTER(&mp->mnt_lazyvnodelist, vp, *mvp, v_lazylist);
	mtx_unlock(&mp->mnt_listmtx);
	ASSERT_VI_LOCKED(vp, "lazy iter");
	return (vp);
}

struct vnode *
__mnt_vnode_next_lazy(struct vnode **mvp, struct mount *mp, mnt_lazy_cb_t *cb,
    void *cbarg)
{

	maybe_yield();
	mtx_lock(&mp->mnt_listmtx);
	return (mnt_vnode_next_lazy(mvp, mp, cb, cbarg));
}

struct vnode *
__mnt_vnode_first_lazy(struct vnode **mvp, struct mount *mp, mnt_lazy_cb_t *cb,
    void *cbarg)
{
	struct vnode *vp;

	if (TAILQ_EMPTY(&mp->mnt_lazyvnodelist))
		return (NULL);

	*mvp = vn_alloc_marker(mp);
	MNT_ILOCK(mp);
	MNT_REF(mp);
	MNT_IUNLOCK(mp);

	mtx_lock(&mp->mnt_listmtx);
	vp = TAILQ_FIRST(&mp->mnt_lazyvnodelist);
	if (vp == NULL) {
		mtx_unlock(&mp->mnt_listmtx);
		mnt_vnode_markerfree_lazy(mvp, mp);
		return (NULL);
	}
	TAILQ_INSERT_BEFORE(vp, *mvp, v_lazylist);
	return (mnt_vnode_next_lazy(mvp, mp, cb, cbarg));
}

void
__mnt_vnode_markerfree_lazy(struct vnode **mvp, struct mount *mp)
{

	if (*mvp == NULL)
		return;

	mtx_lock(&mp->mnt_listmtx);
	TAILQ_REMOVE(&mp->mnt_lazyvnodelist, *mvp, v_lazylist);
	mtx_unlock(&mp->mnt_listmtx);
	mnt_vnode_markerfree_lazy(mvp, mp);
}

int
vn_dir_check_exec(struct vnode *vp, struct componentname *cnp)
{

	if ((cnp->cn_flags & NOEXECCHECK) != 0) {
		cnp->cn_flags &= ~NOEXECCHECK;
		return (0);
	}

	return (VOP_ACCESS(vp, VEXEC, cnp->cn_cred, curthread));
}

/*
 * Do not use this variant unless you have means other than the hold count
 * to prevent the vnode from getting freed.
 */
void
vn_seqc_write_begin_locked(struct vnode *vp)
{

	ASSERT_VI_LOCKED(vp, __func__);
	VNPASS(vp->v_holdcnt > 0, vp);
	VNPASS(vp->v_seqc_users >= 0, vp);
	vp->v_seqc_users++;
	if (vp->v_seqc_users == 1)
		seqc_sleepable_write_begin(&vp->v_seqc);
}

void
vn_seqc_write_begin(struct vnode *vp)
{

	VI_LOCK(vp);
	vn_seqc_write_begin_locked(vp);
	VI_UNLOCK(vp);
}

void
vn_seqc_write_end_locked(struct vnode *vp)
{

	ASSERT_VI_LOCKED(vp, __func__);
	VNPASS(vp->v_seqc_users > 0, vp);
	vp->v_seqc_users--;
	if (vp->v_seqc_users == 0)
		seqc_sleepable_write_end(&vp->v_seqc);
}

void
vn_seqc_write_end(struct vnode *vp)
{

	VI_LOCK(vp);
	vn_seqc_write_end_locked(vp);
	VI_UNLOCK(vp);
}

/*
 * Special case handling for allocating and freeing vnodes.
 *
 * The counter remains unchanged on free so that a doomed vnode will
 * keep testing as in modify as long as it is accessible with SMR.
 */
static void
vn_seqc_init(struct vnode *vp)
{

	vp->v_seqc = 0;
	vp->v_seqc_users = 0;
}

static void
vn_seqc_write_end_free(struct vnode *vp)
{

	VNPASS(seqc_in_modify(vp->v_seqc), vp);
	VNPASS(vp->v_seqc_users == 1, vp);
}

void
vn_irflag_set_locked(struct vnode *vp, short toset)
{
	short flags;

	ASSERT_VI_LOCKED(vp, __func__);
	flags = vn_irflag_read(vp);
	VNASSERT((flags & toset) == 0, vp,
	    ("%s: some of the passed flags already set (have %d, passed %d)\n",
	    __func__, flags, toset));
	atomic_store_short(&vp->v_irflag, flags | toset);
}

void
vn_irflag_set(struct vnode *vp, short toset)
{

	VI_LOCK(vp);
	vn_irflag_set_locked(vp, toset);
	VI_UNLOCK(vp);
}

void
vn_irflag_set_cond_locked(struct vnode *vp, short toset)
{
	short flags;

	ASSERT_VI_LOCKED(vp, __func__);
	flags = vn_irflag_read(vp);
	atomic_store_short(&vp->v_irflag, flags | toset);
}

void
vn_irflag_set_cond(struct vnode *vp, short toset)
{

	VI_LOCK(vp);
	vn_irflag_set_cond_locked(vp, toset);
	VI_UNLOCK(vp);
}

void
vn_irflag_unset_locked(struct vnode *vp, short tounset)
{
	short flags;

	ASSERT_VI_LOCKED(vp, __func__);
	flags = vn_irflag_read(vp);
	VNASSERT((flags & tounset) == tounset, vp,
	    ("%s: some of the passed flags not set (have %d, passed %d)\n",
	    __func__, flags, tounset));
	atomic_store_short(&vp->v_irflag, flags & ~tounset);
}

void
vn_irflag_unset(struct vnode *vp, short tounset)
{

	VI_LOCK(vp);
	vn_irflag_unset_locked(vp, tounset);
	VI_UNLOCK(vp);
}

int
vn_getsize_locked(struct vnode *vp, off_t *size, struct ucred *cred)
{
	struct vattr vattr;
	int error;

	ASSERT_VOP_LOCKED(vp, __func__);
	error = VOP_GETATTR(vp, &vattr, cred);
	if (__predict_true(error == 0)) {
		if (vattr.va_size <= OFF_MAX)
			*size = vattr.va_size;
		else
			error = EFBIG;
	}
	return (error);
}

int
vn_getsize(struct vnode *vp, off_t *size, struct ucred *cred)
{
	int error;

	VOP_LOCK(vp, LK_SHARED);
	error = vn_getsize_locked(vp, size, cred);
	VOP_UNLOCK(vp);
	return (error);
}

#ifdef INVARIANTS
void
vn_set_state_validate(struct vnode *vp, __enum_uint8(vstate) state)
{

	switch (vp->v_state) {
	case VSTATE_UNINITIALIZED:
		switch (state) {
		case VSTATE_CONSTRUCTED:
		case VSTATE_DESTROYING:
			return;
		default:
			break;
		}
		break;
	case VSTATE_CONSTRUCTED:
		ASSERT_VOP_ELOCKED(vp, __func__);
		switch (state) {
		case VSTATE_DESTROYING:
			return;
		default:
			break;
		}
		break;
	case VSTATE_DESTROYING:
		ASSERT_VOP_ELOCKED(vp, __func__);
		switch (state) {
		case VSTATE_DEAD:
			return;
		default:
			break;
		}
		break;
	case VSTATE_DEAD:
		switch (state) {
		case VSTATE_UNINITIALIZED:
			return;
		default:
			break;
		}
		break;
	}

	vn_printf(vp, "invalid state transition %d -> %d\n", vp->v_state, state);
	panic("invalid state transition %d -> %d\n", vp->v_state, state);
}
#endif
