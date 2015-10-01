/*-
 * Copyright (c) 2004 Poul-Henning Kamp
 * Copyright (c) 1994,1997 John S. Dyson
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
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

/*
 * this file contains a new buffer I/O scheme implementing a coherent
 * VM object and buffer cache scheme.  Pains have been taken to make
 * sure that the performance degradation associated with schemes such
 * as this is not realized.
 *
 * Author:  John S. Dyson
 * Significant help during the development and debugging phases
 * had been provided by David Greenman, also of the FreeBSD core team.
 *
 * see man buf(9) for more info.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/devicestat.h>
#include <sys/eventhandler.h>
#include <sys/fail.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/rwlock.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/vmem.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>
#include <sys/watchdog.h>
#include <geom/geom.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/swap_pager.h>
#include "opt_compat.h"
#include "opt_swap.h"

static MALLOC_DEFINE(M_BIOBUF, "biobuf", "BIO buffer");

struct	bio_ops bioops;		/* I/O operation notification */

struct	buf_ops buf_ops_bio = {
	.bop_name	=	"buf_ops_bio",
	.bop_write	=	bufwrite,
	.bop_strategy	=	bufstrategy,
	.bop_sync	=	bufsync,
	.bop_bdflush	=	bufbdflush,
};

static struct buf *buf;		/* buffer header pool */
extern struct buf *swbuf;	/* Swap buffer header pool. */
caddr_t unmapped_buf;

/* Used below and for softdep flushing threads in ufs/ffs/ffs_softdep.c */
struct proc *bufdaemonproc;

static int inmem(struct vnode *vp, daddr_t blkno);
static void vm_hold_free_pages(struct buf *bp, int newbsize);
static void vm_hold_load_pages(struct buf *bp, vm_offset_t from,
		vm_offset_t to);
static void vfs_page_set_valid(struct buf *bp, vm_ooffset_t off, vm_page_t m);
static void vfs_page_set_validclean(struct buf *bp, vm_ooffset_t off,
		vm_page_t m);
static void vfs_clean_pages_dirty_buf(struct buf *bp);
static void vfs_setdirty_locked_object(struct buf *bp);
static void vfs_vmio_invalidate(struct buf *bp);
static void vfs_vmio_truncate(struct buf *bp, int npages);
static void vfs_vmio_extend(struct buf *bp, int npages, int size);
static int vfs_bio_clcheck(struct vnode *vp, int size,
		daddr_t lblkno, daddr_t blkno);
static int buf_flush(struct vnode *vp, int);
static int flushbufqueues(struct vnode *, int, int);
static void buf_daemon(void);
static void bremfreel(struct buf *bp);
static __inline void bd_wakeup(void);
static int sysctl_runningspace(SYSCTL_HANDLER_ARGS);
#if defined(COMPAT_FREEBSD4) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD7)
static int sysctl_bufspace(SYSCTL_HANDLER_ARGS);
#endif

int vmiodirenable = TRUE;
SYSCTL_INT(_vfs, OID_AUTO, vmiodirenable, CTLFLAG_RW, &vmiodirenable, 0,
    "Use the VM system for directory writes");
long runningbufspace;
SYSCTL_LONG(_vfs, OID_AUTO, runningbufspace, CTLFLAG_RD, &runningbufspace, 0,
    "Amount of presently outstanding async buffer io");
static long bufspace;
#if defined(COMPAT_FREEBSD4) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD7)
SYSCTL_PROC(_vfs, OID_AUTO, bufspace, CTLTYPE_LONG|CTLFLAG_MPSAFE|CTLFLAG_RD,
    &bufspace, 0, sysctl_bufspace, "L", "Virtual memory used for buffers");
#else
SYSCTL_LONG(_vfs, OID_AUTO, bufspace, CTLFLAG_RD, &bufspace, 0,
    "Physical memory used for buffers");
#endif
static long bufkvaspace;
SYSCTL_LONG(_vfs, OID_AUTO, bufkvaspace, CTLFLAG_RD, &bufkvaspace, 0,
    "Kernel virtual memory used for buffers");
static long maxbufspace;
SYSCTL_LONG(_vfs, OID_AUTO, maxbufspace, CTLFLAG_RD, &maxbufspace, 0,
    "Maximum allowed value of bufspace (including buf_daemon)");
static long bufmallocspace;
SYSCTL_LONG(_vfs, OID_AUTO, bufmallocspace, CTLFLAG_RD, &bufmallocspace, 0,
    "Amount of malloced memory for buffers");
static long maxbufmallocspace;
SYSCTL_LONG(_vfs, OID_AUTO, maxmallocbufspace, CTLFLAG_RW, &maxbufmallocspace, 0,
    "Maximum amount of malloced memory for buffers");
static long lobufspace;
SYSCTL_LONG(_vfs, OID_AUTO, lobufspace, CTLFLAG_RD, &lobufspace, 0,
    "Minimum amount of buffers we want to have");
long hibufspace;
SYSCTL_LONG(_vfs, OID_AUTO, hibufspace, CTLFLAG_RD, &hibufspace, 0,
    "Maximum allowed value of bufspace (excluding buf_daemon)");
static int bufreusecnt;
SYSCTL_INT(_vfs, OID_AUTO, bufreusecnt, CTLFLAG_RW, &bufreusecnt, 0,
    "Number of times we have reused a buffer");
static int buffreekvacnt;
SYSCTL_INT(_vfs, OID_AUTO, buffreekvacnt, CTLFLAG_RW, &buffreekvacnt, 0,
    "Number of times we have freed the KVA space from some buffer");
static int bufdefragcnt;
SYSCTL_INT(_vfs, OID_AUTO, bufdefragcnt, CTLFLAG_RW, &bufdefragcnt, 0,
    "Number of times we have had to repeat buffer allocation to defragment");
static long lorunningspace;
SYSCTL_PROC(_vfs, OID_AUTO, lorunningspace, CTLTYPE_LONG | CTLFLAG_MPSAFE |
    CTLFLAG_RW, &lorunningspace, 0, sysctl_runningspace, "L",
    "Minimum preferred space used for in-progress I/O");
static long hirunningspace;
SYSCTL_PROC(_vfs, OID_AUTO, hirunningspace, CTLTYPE_LONG | CTLFLAG_MPSAFE |
    CTLFLAG_RW, &hirunningspace, 0, sysctl_runningspace, "L",
    "Maximum amount of space to use for in-progress I/O");
int dirtybufferflushes;
SYSCTL_INT(_vfs, OID_AUTO, dirtybufferflushes, CTLFLAG_RW, &dirtybufferflushes,
    0, "Number of bdwrite to bawrite conversions to limit dirty buffers");
int bdwriteskip;
SYSCTL_INT(_vfs, OID_AUTO, bdwriteskip, CTLFLAG_RW, &bdwriteskip,
    0, "Number of buffers supplied to bdwrite with snapshot deadlock risk");
int altbufferflushes;
SYSCTL_INT(_vfs, OID_AUTO, altbufferflushes, CTLFLAG_RW, &altbufferflushes,
    0, "Number of fsync flushes to limit dirty buffers");
static int recursiveflushes;
SYSCTL_INT(_vfs, OID_AUTO, recursiveflushes, CTLFLAG_RW, &recursiveflushes,
    0, "Number of flushes skipped due to being recursive");
static int numdirtybuffers;
SYSCTL_INT(_vfs, OID_AUTO, numdirtybuffers, CTLFLAG_RD, &numdirtybuffers, 0,
    "Number of buffers that are dirty (has unwritten changes) at the moment");
static int lodirtybuffers;
SYSCTL_INT(_vfs, OID_AUTO, lodirtybuffers, CTLFLAG_RW, &lodirtybuffers, 0,
    "How many buffers we want to have free before bufdaemon can sleep");
static int hidirtybuffers;
SYSCTL_INT(_vfs, OID_AUTO, hidirtybuffers, CTLFLAG_RW, &hidirtybuffers, 0,
    "When the number of dirty buffers is considered severe");
int dirtybufthresh;
SYSCTL_INT(_vfs, OID_AUTO, dirtybufthresh, CTLFLAG_RW, &dirtybufthresh,
    0, "Number of bdwrite to bawrite conversions to clear dirty buffers");
static int numfreebuffers;
SYSCTL_INT(_vfs, OID_AUTO, numfreebuffers, CTLFLAG_RD, &numfreebuffers, 0,
    "Number of free buffers");
static int lofreebuffers;
SYSCTL_INT(_vfs, OID_AUTO, lofreebuffers, CTLFLAG_RW, &lofreebuffers, 0,
   "XXX Unused");
static int hifreebuffers;
SYSCTL_INT(_vfs, OID_AUTO, hifreebuffers, CTLFLAG_RW, &hifreebuffers, 0,
   "XXX Complicatedly unused");
static int getnewbufcalls;
SYSCTL_INT(_vfs, OID_AUTO, getnewbufcalls, CTLFLAG_RW, &getnewbufcalls, 0,
   "Number of calls to getnewbuf");
static int getnewbufrestarts;
SYSCTL_INT(_vfs, OID_AUTO, getnewbufrestarts, CTLFLAG_RW, &getnewbufrestarts, 0,
    "Number of times getnewbuf has had to restart a buffer aquisition");
static int mappingrestarts;
SYSCTL_INT(_vfs, OID_AUTO, mappingrestarts, CTLFLAG_RW, &mappingrestarts, 0,
    "Number of times getblk has had to restart a buffer mapping for "
    "unmapped buffer");
static int flushbufqtarget = 100;
SYSCTL_INT(_vfs, OID_AUTO, flushbufqtarget, CTLFLAG_RW, &flushbufqtarget, 0,
    "Amount of work to do in flushbufqueues when helping bufdaemon");
static long notbufdflushes;
SYSCTL_LONG(_vfs, OID_AUTO, notbufdflushes, CTLFLAG_RD, &notbufdflushes, 0,
    "Number of dirty buffer flushes done by the bufdaemon helpers");
static long barrierwrites;
SYSCTL_LONG(_vfs, OID_AUTO, barrierwrites, CTLFLAG_RW, &barrierwrites, 0,
    "Number of barrier writes");
SYSCTL_INT(_vfs, OID_AUTO, unmapped_buf_allowed, CTLFLAG_RD,
    &unmapped_buf_allowed, 0,
    "Permit the use of the unmapped i/o");

/*
 * Lock for the non-dirty bufqueues
 */
static struct mtx_padalign bqclean;

/*
 * Lock for the dirty queue.
 */
static struct mtx_padalign bqdirty;

/*
 * This lock synchronizes access to bd_request.
 */
static struct mtx_padalign bdlock;

/*
 * This lock protects the runningbufreq and synchronizes runningbufwakeup and
 * waitrunningbufspace().
 */
static struct mtx_padalign rbreqlock;

/*
 * Lock that protects needsbuffer and the sleeps/wakeups surrounding it.
 */
static struct rwlock_padalign nblock;

/*
 * Lock that protects bdirtywait.
 */
static struct mtx_padalign bdirtylock;

/*
 * Wakeup point for bufdaemon, as well as indicator of whether it is already
 * active.  Set to 1 when the bufdaemon is already "on" the queue, 0 when it
 * is idling.
 */
static int bd_request;

/*
 * Request for the buf daemon to write more buffers than is indicated by
 * lodirtybuf.  This may be necessary to push out excess dependencies or
 * defragment the address space where a simple count of the number of dirty
 * buffers is insufficient to characterize the demand for flushing them.
 */
static int bd_speedupreq;

/*
 * bogus page -- for I/O to/from partially complete buffers
 * this is a temporary solution to the problem, but it is not
 * really that bad.  it would be better to split the buffer
 * for input in the case of buffers partially already in memory,
 * but the code is intricate enough already.
 */
vm_page_t bogus_page;

/*
 * Synchronization (sleep/wakeup) variable for active buffer space requests.
 * Set when wait starts, cleared prior to wakeup().
 * Used in runningbufwakeup() and waitrunningbufspace().
 */
static int runningbufreq;

/* 
 * Synchronization (sleep/wakeup) variable for buffer requests.
 * Can contain the VFS_BIO_NEED flags defined below; setting/clearing is done
 * by and/or.
 * Used in numdirtywakeup(), bufspacewakeup(), bufcountadd(), bwillwrite(),
 * getnewbuf(), and getblk().
 */
static volatile int needsbuffer;

/*
 * Synchronization for bwillwrite() waiters.
 */
static int bdirtywait;

/*
 * Definitions for the buffer free lists.
 */
#define BUFFER_QUEUES	4	/* number of free buffer queues */

#define QUEUE_NONE	0	/* on no queue */
#define QUEUE_CLEAN	1	/* non-B_DELWRI buffers */
#define QUEUE_DIRTY	2	/* B_DELWRI buffers */
#define QUEUE_EMPTY	3	/* empty buffer headers */
#define QUEUE_SENTINEL	1024	/* not an queue index, but mark for sentinel */

/* Queues for free buffers with various properties */
static TAILQ_HEAD(bqueues, buf) bufqueues[BUFFER_QUEUES] = { { 0 } };
#ifdef INVARIANTS
static int bq_len[BUFFER_QUEUES];
#endif

/*
 * Single global constant for BUF_WMESG, to avoid getting multiple references.
 * buf_wmesg is referred from macros.
 */
const char *buf_wmesg = BUF_WMESG;

#define VFS_BIO_NEED_ANY	0x01	/* any freeable buffer */
#define VFS_BIO_NEED_FREE	0x04	/* wait for free bufs, hi hysteresis */
#define VFS_BIO_NEED_BUFSPACE	0x08	/* wait for buf space, lo hysteresis */

static int
sysctl_runningspace(SYSCTL_HANDLER_ARGS)
{
	long value;
	int error;

	value = *(long *)arg1;
	error = sysctl_handle_long(oidp, &value, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	mtx_lock(&rbreqlock);
	if (arg1 == &hirunningspace) {
		if (value < lorunningspace)
			error = EINVAL;
		else
			hirunningspace = value;
	} else {
		KASSERT(arg1 == &lorunningspace,
		    ("%s: unknown arg1", __func__));
		if (value > hirunningspace)
			error = EINVAL;
		else
			lorunningspace = value;
	}
	mtx_unlock(&rbreqlock);
	return (error);
}

#if defined(COMPAT_FREEBSD4) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD7)
static int
sysctl_bufspace(SYSCTL_HANDLER_ARGS)
{
	long lvalue;
	int ivalue;

	if (sizeof(int) == sizeof(long) || req->oldlen >= sizeof(long))
		return (sysctl_handle_long(oidp, arg1, arg2, req));
	lvalue = *(long *)arg1;
	if (lvalue > INT_MAX)
		/* On overflow, still write out a long to trigger ENOMEM. */
		return (sysctl_handle_long(oidp, &lvalue, 0, req));
	ivalue = lvalue;
	return (sysctl_handle_int(oidp, &ivalue, 0, req));
}
#endif

/*
 *	bqlock:
 *
 *	Return the appropriate queue lock based on the index.
 */
static inline struct mtx *
bqlock(int qindex)
{

	if (qindex == QUEUE_DIRTY)
		return (struct mtx *)(&bqdirty);
	return (struct mtx *)(&bqclean);
}

/*
 *	bdirtywakeup:
 *
 *	Wakeup any bwillwrite() waiters.
 */
static void
bdirtywakeup(void)
{
	mtx_lock(&bdirtylock);
	if (bdirtywait) {
		bdirtywait = 0;
		wakeup(&bdirtywait);
	}
	mtx_unlock(&bdirtylock);
}

/*
 *	bdirtysub:
 *
 *	Decrement the numdirtybuffers count by one and wakeup any
 *	threads blocked in bwillwrite().
 */
static void
bdirtysub(void)
{

	if (atomic_fetchadd_int(&numdirtybuffers, -1) ==
	    (lodirtybuffers + hidirtybuffers) / 2)
		bdirtywakeup();
}

/*
 *	bdirtyadd:
 *
 *	Increment the numdirtybuffers count by one and wakeup the buf 
 *	daemon if needed.
 */
static void
bdirtyadd(void)
{

	/*
	 * Only do the wakeup once as we cross the boundary.  The
	 * buf daemon will keep running until the condition clears.
	 */
	if (atomic_fetchadd_int(&numdirtybuffers, 1) ==
	    (lodirtybuffers + hidirtybuffers) / 2)
		bd_wakeup();
}

/*
 *	bufspacewakeup:
 *
 *	Called when buffer space is potentially available for recovery.
 *	getnewbuf() will block on this flag when it is unable to free 
 *	sufficient buffer space.  Buffer space becomes recoverable when 
 *	bp's get placed back in the queues.
 */
static __inline void
bufspacewakeup(void)
{
	int need_wakeup, on;

	/*
	 * If someone is waiting for bufspace, wake them up.  Even
	 * though we may not have freed the kva space yet, the waiting
	 * process will be able to now.
	 */
	rw_rlock(&nblock);
	for (;;) {
		need_wakeup = 0;
		on = needsbuffer;
		if ((on & VFS_BIO_NEED_BUFSPACE) == 0)
			break;
		need_wakeup = 1;
		if (atomic_cmpset_rel_int(&needsbuffer, on,
		    on & ~VFS_BIO_NEED_BUFSPACE))
			break;
	}
	if (need_wakeup)
		wakeup(__DEVOLATILE(void *, &needsbuffer));
	rw_runlock(&nblock);
}

/*
 *	bufspaceadjust:
 *
 *	Adjust the reported bufspace for a KVA managed buffer, possibly
 * 	waking any waiters.
 */
static void
bufspaceadjust(struct buf *bp, int bufsize)
{
	int diff;

	KASSERT((bp->b_flags & B_MALLOC) == 0,
	    ("bufspaceadjust: malloc buf %p", bp));
	diff = bufsize - bp->b_bufsize;
	if (diff < 0) {
		atomic_subtract_long(&bufspace, -diff);
		bufspacewakeup();
	} else
		atomic_add_long(&bufspace, diff);
	bp->b_bufsize = bufsize;
}

/*
 *	bufmallocadjust:
 *
 *	Adjust the reported bufspace for a malloc managed buffer, possibly
 *	waking any waiters.
 */
static void
bufmallocadjust(struct buf *bp, int bufsize)
{
	int diff;

	KASSERT((bp->b_flags & B_MALLOC) != 0,
	    ("bufmallocadjust: non-malloc buf %p", bp));
	diff = bufsize - bp->b_bufsize;
	if (diff < 0) {
		atomic_subtract_long(&bufmallocspace, -diff);
		bufspacewakeup();
	} else
		atomic_add_long(&bufmallocspace, diff);
	bp->b_bufsize = bufsize;
}

/*
 *	runningwakeup:
 *
 *	Wake up processes that are waiting on asynchronous writes to fall
 *	below lorunningspace.
 */
static void
runningwakeup(void)
{

	mtx_lock(&rbreqlock);
	if (runningbufreq) {
		runningbufreq = 0;
		wakeup(&runningbufreq);
	}
	mtx_unlock(&rbreqlock);
}

/*
 *	runningbufwakeup:
 *
 *	Decrement the outstanding write count according.
 */
void
runningbufwakeup(struct buf *bp)
{
	long space, bspace;

	bspace = bp->b_runningbufspace;
	if (bspace == 0)
		return;
	space = atomic_fetchadd_long(&runningbufspace, -bspace);
	KASSERT(space >= bspace, ("runningbufspace underflow %ld %ld",
	    space, bspace));
	bp->b_runningbufspace = 0;
	/*
	 * Only acquire the lock and wakeup on the transition from exceeding
	 * the threshold to falling below it.
	 */
	if (space < lorunningspace)
		return;
	if (space - bspace > lorunningspace)
		return;
	runningwakeup();
}

/*
 *	bufcountadd:
 *
 *	Called when a buffer has been added to one of the free queues to
 *	account for the buffer and to wakeup anyone waiting for free buffers.
 *	This typically occurs when large amounts of metadata are being handled
 *	by the buffer cache ( else buffer space runs out first, usually ).
 */
static __inline void
bufcountadd(struct buf *bp)
{
	int mask, need_wakeup, old, on;

	KASSERT((bp->b_flags & B_INFREECNT) == 0,
	    ("buf %p already counted as free", bp));
	bp->b_flags |= B_INFREECNT;
	old = atomic_fetchadd_int(&numfreebuffers, 1);
	KASSERT(old >= 0 && old < nbuf,
	    ("numfreebuffers climbed to %d", old + 1));
	mask = VFS_BIO_NEED_ANY;
	if (numfreebuffers >= hifreebuffers)
		mask |= VFS_BIO_NEED_FREE;
	rw_rlock(&nblock);
	for (;;) {
		need_wakeup = 0;
		on = needsbuffer;
		if (on == 0)
			break;
		need_wakeup = 1;
		if (atomic_cmpset_rel_int(&needsbuffer, on, on & ~mask))
			break;
	}
	if (need_wakeup)
		wakeup(__DEVOLATILE(void *, &needsbuffer));
	rw_runlock(&nblock);
}

/*
 *	bufcountsub:
 *
 *	Decrement the numfreebuffers count as needed.
 */
static void
bufcountsub(struct buf *bp)
{
	int old;

	/*
	 * Fixup numfreebuffers count.  If the buffer is invalid or not
	 * delayed-write, the buffer was free and we must decrement
	 * numfreebuffers.
	 */
	if ((bp->b_flags & B_INVAL) || (bp->b_flags & B_DELWRI) == 0) {
		KASSERT((bp->b_flags & B_INFREECNT) != 0,
		    ("buf %p not counted in numfreebuffers", bp));
		bp->b_flags &= ~B_INFREECNT;
		old = atomic_fetchadd_int(&numfreebuffers, -1);
		KASSERT(old > 0, ("numfreebuffers dropped to %d", old - 1));
	}
}

/*
 *	waitrunningbufspace()
 *
 *	runningbufspace is a measure of the amount of I/O currently
 *	running.  This routine is used in async-write situations to
 *	prevent creating huge backups of pending writes to a device.
 *	Only asynchronous writes are governed by this function.
 *
 *	This does NOT turn an async write into a sync write.  It waits  
 *	for earlier writes to complete and generally returns before the
 *	caller's write has reached the device.
 */
void
waitrunningbufspace(void)
{

	mtx_lock(&rbreqlock);
	while (runningbufspace > hirunningspace) {
		runningbufreq = 1;
		msleep(&runningbufreq, &rbreqlock, PVM, "wdrain", 0);
	}
	mtx_unlock(&rbreqlock);
}


/*
 *	vfs_buf_test_cache:
 *
 *	Called when a buffer is extended.  This function clears the B_CACHE
 *	bit if the newly extended portion of the buffer does not contain
 *	valid data.
 */
static __inline void
vfs_buf_test_cache(struct buf *bp, vm_ooffset_t foff, vm_offset_t off,
    vm_offset_t size, vm_page_t m)
{

	VM_OBJECT_ASSERT_LOCKED(m->object);
	if (bp->b_flags & B_CACHE) {
		int base = (foff + off) & PAGE_MASK;
		if (vm_page_is_valid(m, base, size) == 0)
			bp->b_flags &= ~B_CACHE;
	}
}

/* Wake up the buffer daemon if necessary */
static __inline void
bd_wakeup(void)
{

	mtx_lock(&bdlock);
	if (bd_request == 0) {
		bd_request = 1;
		wakeup(&bd_request);
	}
	mtx_unlock(&bdlock);
}

/*
 * bd_speedup - speedup the buffer cache flushing code
 */
void
bd_speedup(void)
{
	int needwake;

	mtx_lock(&bdlock);
	needwake = 0;
	if (bd_speedupreq == 0 || bd_request == 0)
		needwake = 1;
	bd_speedupreq = 1;
	bd_request = 1;
	if (needwake)
		wakeup(&bd_request);
	mtx_unlock(&bdlock);
}

#ifndef NSWBUF_MIN
#define	NSWBUF_MIN	16
#endif

#ifdef __i386__
#define	TRANSIENT_DENOM	5
#else
#define	TRANSIENT_DENOM 10
#endif

/*
 * Calculating buffer cache scaling values and reserve space for buffer
 * headers.  This is called during low level kernel initialization and
 * may be called more then once.  We CANNOT write to the memory area
 * being reserved at this time.
 */
caddr_t
kern_vfs_bio_buffer_alloc(caddr_t v, long physmem_est)
{
	int tuned_nbuf;
	long maxbuf, maxbuf_sz, buf_sz,	biotmap_sz;

	/*
	 * physmem_est is in pages.  Convert it to kilobytes (assumes
	 * PAGE_SIZE is >= 1K)
	 */
	physmem_est = physmem_est * (PAGE_SIZE / 1024);

	/*
	 * The nominal buffer size (and minimum KVA allocation) is BKVASIZE.
	 * For the first 64MB of ram nominally allocate sufficient buffers to
	 * cover 1/4 of our ram.  Beyond the first 64MB allocate additional
	 * buffers to cover 1/10 of our ram over 64MB.  When auto-sizing
	 * the buffer cache we limit the eventual kva reservation to
	 * maxbcache bytes.
	 *
	 * factor represents the 1/4 x ram conversion.
	 */
	if (nbuf == 0) {
		int factor = 4 * BKVASIZE / 1024;

		nbuf = 50;
		if (physmem_est > 4096)
			nbuf += min((physmem_est - 4096) / factor,
			    65536 / factor);
		if (physmem_est > 65536)
			nbuf += min((physmem_est - 65536) * 2 / (factor * 5),
			    32 * 1024 * 1024 / (factor * 5));

		if (maxbcache && nbuf > maxbcache / BKVASIZE)
			nbuf = maxbcache / BKVASIZE;
		tuned_nbuf = 1;
	} else
		tuned_nbuf = 0;

	/* XXX Avoid unsigned long overflows later on with maxbufspace. */
	maxbuf = (LONG_MAX / 3) / BKVASIZE;
	if (nbuf > maxbuf) {
		if (!tuned_nbuf)
			printf("Warning: nbufs lowered from %d to %ld\n", nbuf,
			    maxbuf);
		nbuf = maxbuf;
	}

	/*
	 * Ideal allocation size for the transient bio submap is 10%
	 * of the maximal space buffer map.  This roughly corresponds
	 * to the amount of the buffer mapped for typical UFS load.
	 *
	 * Clip the buffer map to reserve space for the transient
	 * BIOs, if its extent is bigger than 90% (80% on i386) of the
	 * maximum buffer map extent on the platform.
	 *
	 * The fall-back to the maxbuf in case of maxbcache unset,
	 * allows to not trim the buffer KVA for the architectures
	 * with ample KVA space.
	 */
	if (bio_transient_maxcnt == 0 && unmapped_buf_allowed) {
		maxbuf_sz = maxbcache != 0 ? maxbcache : maxbuf * BKVASIZE;
		buf_sz = (long)nbuf * BKVASIZE;
		if (buf_sz < maxbuf_sz / TRANSIENT_DENOM *
		    (TRANSIENT_DENOM - 1)) {
			/*
			 * There is more KVA than memory.  Do not
			 * adjust buffer map size, and assign the rest
			 * of maxbuf to transient map.
			 */
			biotmap_sz = maxbuf_sz - buf_sz;
		} else {
			/*
			 * Buffer map spans all KVA we could afford on
			 * this platform.  Give 10% (20% on i386) of
			 * the buffer map to the transient bio map.
			 */
			biotmap_sz = buf_sz / TRANSIENT_DENOM;
			buf_sz -= biotmap_sz;
		}
		if (biotmap_sz / INT_MAX > MAXPHYS)
			bio_transient_maxcnt = INT_MAX;
		else
			bio_transient_maxcnt = biotmap_sz / MAXPHYS;
		/*
		 * Artifically limit to 1024 simultaneous in-flight I/Os
		 * using the transient mapping.
		 */
		if (bio_transient_maxcnt > 1024)
			bio_transient_maxcnt = 1024;
		if (tuned_nbuf)
			nbuf = buf_sz / BKVASIZE;
	}

	/*
	 * swbufs are used as temporary holders for I/O, such as paging I/O.
	 * We have no less then 16 and no more then 256.
	 */
	nswbuf = min(nbuf / 4, 256);
	TUNABLE_INT_FETCH("kern.nswbuf", &nswbuf);
	if (nswbuf < NSWBUF_MIN)
		nswbuf = NSWBUF_MIN;

	/*
	 * Reserve space for the buffer cache buffers
	 */
	swbuf = (void *)v;
	v = (caddr_t)(swbuf + nswbuf);
	buf = (void *)v;
	v = (caddr_t)(buf + nbuf);

	return(v);
}

/* Initialize the buffer subsystem.  Called before use of any buffers. */
void
bufinit(void)
{
	struct buf *bp;
	int i;

	CTASSERT(MAXBCACHEBUF >= MAXBSIZE);
	mtx_init(&bqclean, "bufq clean lock", NULL, MTX_DEF);
	mtx_init(&bqdirty, "bufq dirty lock", NULL, MTX_DEF);
	mtx_init(&rbreqlock, "runningbufspace lock", NULL, MTX_DEF);
	rw_init(&nblock, "needsbuffer lock");
	mtx_init(&bdlock, "buffer daemon lock", NULL, MTX_DEF);
	mtx_init(&bdirtylock, "dirty buf lock", NULL, MTX_DEF);

	/* next, make a null set of free lists */
	for (i = 0; i < BUFFER_QUEUES; i++)
		TAILQ_INIT(&bufqueues[i]);

	unmapped_buf = (caddr_t)kva_alloc(MAXPHYS);

	/* finally, initialize each buffer header and stick on empty q */
	for (i = 0; i < nbuf; i++) {
		bp = &buf[i];
		bzero(bp, sizeof *bp);
		bp->b_flags = B_INVAL | B_INFREECNT;
		bp->b_rcred = NOCRED;
		bp->b_wcred = NOCRED;
		bp->b_qindex = QUEUE_EMPTY;
		bp->b_xflags = 0;
		bp->b_data = bp->b_kvabase = unmapped_buf;
		LIST_INIT(&bp->b_dep);
		BUF_LOCKINIT(bp);
		TAILQ_INSERT_TAIL(&bufqueues[QUEUE_EMPTY], bp, b_freelist);
#ifdef INVARIANTS
		bq_len[QUEUE_EMPTY]++;
#endif
	}

	/*
	 * maxbufspace is the absolute maximum amount of buffer space we are 
	 * allowed to reserve in KVM and in real terms.  The absolute maximum
	 * is nominally used by buf_daemon.  hibufspace is the nominal maximum
	 * used by most other processes.  The differential is required to 
	 * ensure that buf_daemon is able to run when other processes might 
	 * be blocked waiting for buffer space.
	 *
	 * maxbufspace is based on BKVASIZE.  Allocating buffers larger then
	 * this may result in KVM fragmentation which is not handled optimally
	 * by the system.
	 */
	maxbufspace = (long)nbuf * BKVASIZE;
	hibufspace = lmax(3 * maxbufspace / 4, maxbufspace - MAXBCACHEBUF * 10);
	lobufspace = hibufspace - MAXBCACHEBUF;

	/*
	 * Note: The 16 MiB upper limit for hirunningspace was chosen
	 * arbitrarily and may need further tuning. It corresponds to
	 * 128 outstanding write IO requests (if IO size is 128 KiB),
	 * which fits with many RAID controllers' tagged queuing limits.
	 * The lower 1 MiB limit is the historical upper limit for
	 * hirunningspace.
	 */
	hirunningspace = lmax(lmin(roundup(hibufspace / 64, MAXBCACHEBUF),
	    16 * 1024 * 1024), 1024 * 1024);
	lorunningspace = roundup((hirunningspace * 2) / 3, MAXBCACHEBUF);

/*
 * Limit the amount of malloc memory since it is wired permanently into
 * the kernel space.  Even though this is accounted for in the buffer
 * allocation, we don't want the malloced region to grow uncontrolled.
 * The malloc scheme improves memory utilization significantly on average
 * (small) directories.
 */
	maxbufmallocspace = hibufspace / 20;

/*
 * Reduce the chance of a deadlock occuring by limiting the number
 * of delayed-write dirty buffers we allow to stack up.
 */
	hidirtybuffers = nbuf / 4 + 20;
	dirtybufthresh = hidirtybuffers * 9 / 10;
	numdirtybuffers = 0;
/*
 * To support extreme low-memory systems, make sure hidirtybuffers cannot
 * eat up all available buffer space.  This occurs when our minimum cannot
 * be met.  We try to size hidirtybuffers to 3/4 our buffer space assuming
 * BKVASIZE'd buffers.
 */
	while ((long)hidirtybuffers * BKVASIZE > 3 * hibufspace / 4) {
		hidirtybuffers >>= 1;
	}
	lodirtybuffers = hidirtybuffers / 2;

/*
 * Try to keep the number of free buffers in the specified range,
 * and give special processes (e.g. like buf_daemon) access to an 
 * emergency reserve.
 */
	lofreebuffers = nbuf / 18 + 5;
	hifreebuffers = 2 * lofreebuffers;
	numfreebuffers = nbuf;

	bogus_page = vm_page_alloc(NULL, 0, VM_ALLOC_NOOBJ |
	    VM_ALLOC_NORMAL | VM_ALLOC_WIRED);
}

#ifdef INVARIANTS
static inline void
vfs_buf_check_mapped(struct buf *bp)
{

	KASSERT(bp->b_kvabase != unmapped_buf,
	    ("mapped buf: b_kvabase was not updated %p", bp));
	KASSERT(bp->b_data != unmapped_buf,
	    ("mapped buf: b_data was not updated %p", bp));
	KASSERT(bp->b_data < unmapped_buf || bp->b_data >= unmapped_buf +
	    MAXPHYS, ("b_data + b_offset unmapped %p", bp));
}

static inline void
vfs_buf_check_unmapped(struct buf *bp)
{

	KASSERT(bp->b_data == unmapped_buf,
	    ("unmapped buf: corrupted b_data %p", bp));
}

#define	BUF_CHECK_MAPPED(bp) vfs_buf_check_mapped(bp)
#define	BUF_CHECK_UNMAPPED(bp) vfs_buf_check_unmapped(bp)
#else
#define	BUF_CHECK_MAPPED(bp) do {} while (0)
#define	BUF_CHECK_UNMAPPED(bp) do {} while (0)
#endif

static int
isbufbusy(struct buf *bp)
{
	if (((bp->b_flags & (B_INVAL | B_PERSISTENT)) == 0 &&
	    BUF_ISLOCKED(bp)) ||
	    ((bp->b_flags & (B_DELWRI | B_INVAL)) == B_DELWRI))
		return (1);
	return (0);
}

/*
 * Shutdown the system cleanly to prepare for reboot, halt, or power off.
 */
void
bufshutdown(int show_busybufs)
{
	static int first_buf_printf = 1;
	struct buf *bp;
	int iter, nbusy, pbusy;
#ifndef PREEMPTION
	int subiter;
#endif

	/* 
	 * Sync filesystems for shutdown
	 */
	wdog_kern_pat(WD_LASTVAL);
	sys_sync(curthread, NULL);

	/*
	 * With soft updates, some buffers that are
	 * written will be remarked as dirty until other
	 * buffers are written.
	 */
	for (iter = pbusy = 0; iter < 20; iter++) {
		nbusy = 0;
		for (bp = &buf[nbuf]; --bp >= buf; )
			if (isbufbusy(bp))
				nbusy++;
		if (nbusy == 0) {
			if (first_buf_printf)
				printf("All buffers synced.");
			break;
		}
		if (first_buf_printf) {
			printf("Syncing disks, buffers remaining... ");
			first_buf_printf = 0;
		}
		printf("%d ", nbusy);
		if (nbusy < pbusy)
			iter = 0;
		pbusy = nbusy;

		wdog_kern_pat(WD_LASTVAL);
		sys_sync(curthread, NULL);

#ifdef PREEMPTION
		/*
		 * Drop Giant and spin for a while to allow
		 * interrupt threads to run.
		 */
		DROP_GIANT();
		DELAY(50000 * iter);
		PICKUP_GIANT();
#else
		/*
		 * Drop Giant and context switch several times to
		 * allow interrupt threads to run.
		 */
		DROP_GIANT();
		for (subiter = 0; subiter < 50 * iter; subiter++) {
			thread_lock(curthread);
			mi_switch(SW_VOL, NULL);
			thread_unlock(curthread);
			DELAY(1000);
		}
		PICKUP_GIANT();
#endif
	}
	printf("\n");
	/*
	 * Count only busy local buffers to prevent forcing 
	 * a fsck if we're just a client of a wedged NFS server
	 */
	nbusy = 0;
	for (bp = &buf[nbuf]; --bp >= buf; ) {
		if (isbufbusy(bp)) {
#if 0
/* XXX: This is bogus.  We should probably have a BO_REMOTE flag instead */
			if (bp->b_dev == NULL) {
				TAILQ_REMOVE(&mountlist,
				    bp->b_vp->v_mount, mnt_list);
				continue;
			}
#endif
			nbusy++;
			if (show_busybufs > 0) {
				printf(
	    "%d: buf:%p, vnode:%p, flags:%0x, blkno:%jd, lblkno:%jd, buflock:",
				    nbusy, bp, bp->b_vp, bp->b_flags,
				    (intmax_t)bp->b_blkno,
				    (intmax_t)bp->b_lblkno);
				BUF_LOCKPRINTINFO(bp);
				if (show_busybufs > 1)
					vn_printf(bp->b_vp,
					    "vnode content: ");
			}
		}
	}
	if (nbusy) {
		/*
		 * Failed to sync all blocks. Indicate this and don't
		 * unmount filesystems (thus forcing an fsck on reboot).
		 */
		printf("Giving up on %d buffers\n", nbusy);
		DELAY(5000000);	/* 5 seconds */
	} else {
		if (!first_buf_printf)
			printf("Final sync complete\n");
		/*
		 * Unmount filesystems
		 */
		if (panicstr == 0)
			vfs_unmountall();
	}
	swapoff_all();
	DELAY(100000);		/* wait for console output to finish */
}

static void
bpmap_qenter(struct buf *bp)
{

	BUF_CHECK_MAPPED(bp);

	/*
	 * bp->b_data is relative to bp->b_offset, but
	 * bp->b_offset may be offset into the first page.
	 */
	bp->b_data = (caddr_t)trunc_page((vm_offset_t)bp->b_data);
	pmap_qenter((vm_offset_t)bp->b_data, bp->b_pages, bp->b_npages);
	bp->b_data = (caddr_t)((vm_offset_t)bp->b_data |
	    (vm_offset_t)(bp->b_offset & PAGE_MASK));
}

/*
 *	binsfree:
 *
 *	Insert the buffer into the appropriate free list.
 */
static void
binsfree(struct buf *bp, int qindex)
{
	struct mtx *olock, *nlock;

	BUF_ASSERT_XLOCKED(bp);

	nlock = bqlock(qindex);
	/* Handle delayed bremfree() processing. */
	if (bp->b_flags & B_REMFREE) {
		olock = bqlock(bp->b_qindex);
		mtx_lock(olock);
		bremfreel(bp);
		if (olock != nlock) {
			mtx_unlock(olock);
			mtx_lock(nlock);
		}
	} else
		mtx_lock(nlock);

	if (bp->b_qindex != QUEUE_NONE)
		panic("binsfree: free buffer onto another queue???");

	bp->b_qindex = qindex;
	if (bp->b_flags & B_AGE)
		TAILQ_INSERT_HEAD(&bufqueues[bp->b_qindex], bp, b_freelist);
	else
		TAILQ_INSERT_TAIL(&bufqueues[bp->b_qindex], bp, b_freelist);
#ifdef INVARIANTS
	bq_len[bp->b_qindex]++;
#endif
	mtx_unlock(nlock);

	/*
	 * Something we can maybe free or reuse.
	 */
	if (bp->b_bufsize && !(bp->b_flags & B_DELWRI))
		bufspacewakeup();

	if ((bp->b_flags & B_INVAL) || !(bp->b_flags & B_DELWRI))
		bufcountadd(bp);
}

/*
 *	bremfree:
 *
 *	Mark the buffer for removal from the appropriate free list.
 *	
 */
void
bremfree(struct buf *bp)
{

	CTR3(KTR_BUF, "bremfree(%p) vp %p flags %X", bp, bp->b_vp, bp->b_flags);
	KASSERT((bp->b_flags & B_REMFREE) == 0,
	    ("bremfree: buffer %p already marked for delayed removal.", bp));
	KASSERT(bp->b_qindex != QUEUE_NONE,
	    ("bremfree: buffer %p not on a queue.", bp));
	BUF_ASSERT_XLOCKED(bp);

	bp->b_flags |= B_REMFREE;
	bufcountsub(bp);
}

/*
 *	bremfreef:
 *
 *	Force an immediate removal from a free list.  Used only in nfs when
 *	it abuses the b_freelist pointer.
 */
void
bremfreef(struct buf *bp)
{
	struct mtx *qlock;

	qlock = bqlock(bp->b_qindex);
	mtx_lock(qlock);
	bremfreel(bp);
	mtx_unlock(qlock);
}

/*
 *	bremfreel:
 *
 *	Removes a buffer from the free list, must be called with the
 *	correct qlock held.
 */
static void
bremfreel(struct buf *bp)
{

	CTR3(KTR_BUF, "bremfreel(%p) vp %p flags %X",
	    bp, bp->b_vp, bp->b_flags);
	KASSERT(bp->b_qindex != QUEUE_NONE,
	    ("bremfreel: buffer %p not on a queue.", bp));
	BUF_ASSERT_XLOCKED(bp);
	mtx_assert(bqlock(bp->b_qindex), MA_OWNED);

	TAILQ_REMOVE(&bufqueues[bp->b_qindex], bp, b_freelist);
#ifdef INVARIANTS
	KASSERT(bq_len[bp->b_qindex] >= 1, ("queue %d underflow",
	    bp->b_qindex));
	bq_len[bp->b_qindex]--;
#endif
	bp->b_qindex = QUEUE_NONE;
	/*
	 * If this was a delayed bremfree() we only need to remove the buffer
	 * from the queue and return the stats are already done.
	 */
	if (bp->b_flags & B_REMFREE) {
		bp->b_flags &= ~B_REMFREE;
		return;
	}
	bufcountsub(bp);
}

/*
 *	bufkvafree:
 *
 *	Free the kva allocation for a buffer.
 *
 */
static void
bufkvafree(struct buf *bp)
{

#ifdef INVARIANTS
	if (bp->b_kvasize == 0) {
		KASSERT(bp->b_kvabase == unmapped_buf &&
		    bp->b_data == unmapped_buf,
		    ("Leaked KVA space on %p", bp));
	} else if (buf_mapped(bp))
		BUF_CHECK_MAPPED(bp);
	else
		BUF_CHECK_UNMAPPED(bp);
#endif
	if (bp->b_kvasize == 0)
		return;

	vmem_free(buffer_arena, (vm_offset_t)bp->b_kvabase, bp->b_kvasize);
	atomic_subtract_long(&bufkvaspace, bp->b_kvasize);
	atomic_add_int(&buffreekvacnt, 1);
	bp->b_data = bp->b_kvabase = unmapped_buf;
	bp->b_kvasize = 0;
}

/*
 *	bufkvaalloc:
 *
 *	Allocate the buffer KVA and set b_kvasize and b_kvabase.
 */
static int
bufkvaalloc(struct buf *bp, int maxsize, int gbflags)
{
	vm_offset_t addr;
	int error;

	KASSERT((gbflags & GB_UNMAPPED) == 0 || (gbflags & GB_KVAALLOC) != 0,
	    ("Invalid gbflags 0x%x in %s", gbflags, __func__));

	bufkvafree(bp);

	addr = 0;
	error = vmem_alloc(buffer_arena, maxsize, M_BESTFIT | M_NOWAIT, &addr);
	if (error != 0) {
		/*
		 * Buffer map is too fragmented.  Request the caller
		 * to defragment the map.
		 */
		atomic_add_int(&bufdefragcnt, 1);
		return (error);
	}
	bp->b_kvabase = (caddr_t)addr;
	bp->b_kvasize = maxsize;
	atomic_add_long(&bufkvaspace, bp->b_kvasize);
	if ((gbflags & GB_UNMAPPED) != 0) {
		bp->b_data = unmapped_buf;
		BUF_CHECK_UNMAPPED(bp);
	} else {
		bp->b_data = bp->b_kvabase;
		BUF_CHECK_MAPPED(bp);
	}
	return (0);
}

/*
 * Attempt to initiate asynchronous I/O on read-ahead blocks.  We must
 * clear BIO_ERROR and B_INVAL prior to initiating I/O . If B_CACHE is set,
 * the buffer is valid and we do not have to do anything.
 */
void
breada(struct vnode * vp, daddr_t * rablkno, int * rabsize,
    int cnt, struct ucred * cred)
{
	struct buf *rabp;
	int i;

	for (i = 0; i < cnt; i++, rablkno++, rabsize++) {
		if (inmem(vp, *rablkno))
			continue;
		rabp = getblk(vp, *rablkno, *rabsize, 0, 0, 0);

		if ((rabp->b_flags & B_CACHE) == 0) {
			if (!TD_IS_IDLETHREAD(curthread))
				curthread->td_ru.ru_inblock++;
			rabp->b_flags |= B_ASYNC;
			rabp->b_flags &= ~B_INVAL;
			rabp->b_ioflags &= ~BIO_ERROR;
			rabp->b_iocmd = BIO_READ;
			if (rabp->b_rcred == NOCRED && cred != NOCRED)
				rabp->b_rcred = crhold(cred);
			vfs_busy_pages(rabp, 0);
			BUF_KERNPROC(rabp);
			rabp->b_iooffset = dbtob(rabp->b_blkno);
			bstrategy(rabp);
		} else {
			brelse(rabp);
		}
	}
}

/*
 * Entry point for bread() and breadn() via #defines in sys/buf.h.
 *
 * Get a buffer with the specified data.  Look in the cache first.  We
 * must clear BIO_ERROR and B_INVAL prior to initiating I/O.  If B_CACHE
 * is set, the buffer is valid and we do not have to do anything, see
 * getblk(). Also starts asynchronous I/O on read-ahead blocks.
 */
int
breadn_flags(struct vnode *vp, daddr_t blkno, int size, daddr_t *rablkno,
    int *rabsize, int cnt, struct ucred *cred, int flags, struct buf **bpp)
{
	struct buf *bp;
	int rv = 0, readwait = 0;

	CTR3(KTR_BUF, "breadn(%p, %jd, %d)", vp, blkno, size);
	/*
	 * Can only return NULL if GB_LOCK_NOWAIT flag is specified.
	 */
	*bpp = bp = getblk(vp, blkno, size, 0, 0, flags);
	if (bp == NULL)
		return (EBUSY);

	/* if not found in cache, do some I/O */
	if ((bp->b_flags & B_CACHE) == 0) {
		if (!TD_IS_IDLETHREAD(curthread))
			curthread->td_ru.ru_inblock++;
		bp->b_iocmd = BIO_READ;
		bp->b_flags &= ~B_INVAL;
		bp->b_ioflags &= ~BIO_ERROR;
		if (bp->b_rcred == NOCRED && cred != NOCRED)
			bp->b_rcred = crhold(cred);
		vfs_busy_pages(bp, 0);
		bp->b_iooffset = dbtob(bp->b_blkno);
		bstrategy(bp);
		++readwait;
	}

	breada(vp, rablkno, rabsize, cnt, cred);

	if (readwait) {
		rv = bufwait(bp);
	}
	return (rv);
}

/*
 * Write, release buffer on completion.  (Done by iodone
 * if async).  Do not bother writing anything if the buffer
 * is invalid.
 *
 * Note that we set B_CACHE here, indicating that buffer is
 * fully valid and thus cacheable.  This is true even of NFS
 * now so we set it generally.  This could be set either here 
 * or in biodone() since the I/O is synchronous.  We put it
 * here.
 */
int
bufwrite(struct buf *bp)
{
	int oldflags;
	struct vnode *vp;
	long space;
	int vp_md;

	CTR3(KTR_BUF, "bufwrite(%p) vp %p flags %X", bp, bp->b_vp, bp->b_flags);
	if ((bp->b_bufobj->bo_flag & BO_DEAD) != 0) {
		bp->b_flags |= B_INVAL | B_RELBUF;
		bp->b_flags &= ~B_CACHE;
		brelse(bp);
		return (ENXIO);
	}
	if (bp->b_flags & B_INVAL) {
		brelse(bp);
		return (0);
	}

	if (bp->b_flags & B_BARRIER)
		barrierwrites++;

	oldflags = bp->b_flags;

	BUF_ASSERT_HELD(bp);

	if (bp->b_pin_count > 0)
		bunpin_wait(bp);

	KASSERT(!(bp->b_vflags & BV_BKGRDINPROG),
	    ("FFS background buffer should not get here %p", bp));

	vp = bp->b_vp;
	if (vp)
		vp_md = vp->v_vflag & VV_MD;
	else
		vp_md = 0;

	/*
	 * Mark the buffer clean.  Increment the bufobj write count
	 * before bundirty() call, to prevent other thread from seeing
	 * empty dirty list and zero counter for writes in progress,
	 * falsely indicating that the bufobj is clean.
	 */
	bufobj_wref(bp->b_bufobj);
	bundirty(bp);

	bp->b_flags &= ~B_DONE;
	bp->b_ioflags &= ~BIO_ERROR;
	bp->b_flags |= B_CACHE;
	bp->b_iocmd = BIO_WRITE;

	vfs_busy_pages(bp, 1);

	/*
	 * Normal bwrites pipeline writes
	 */
	bp->b_runningbufspace = bp->b_bufsize;
	space = atomic_fetchadd_long(&runningbufspace, bp->b_runningbufspace);

	if (!TD_IS_IDLETHREAD(curthread))
		curthread->td_ru.ru_oublock++;
	if (oldflags & B_ASYNC)
		BUF_KERNPROC(bp);
	bp->b_iooffset = dbtob(bp->b_blkno);
	bstrategy(bp);

	if ((oldflags & B_ASYNC) == 0) {
		int rtval = bufwait(bp);
		brelse(bp);
		return (rtval);
	} else if (space > hirunningspace) {
		/*
		 * don't allow the async write to saturate the I/O
		 * system.  We will not deadlock here because
		 * we are blocking waiting for I/O that is already in-progress
		 * to complete. We do not block here if it is the update
		 * or syncer daemon trying to clean up as that can lead
		 * to deadlock.
		 */
		if ((curthread->td_pflags & TDP_NORUNNINGBUF) == 0 && !vp_md)
			waitrunningbufspace();
	}

	return (0);
}

void
bufbdflush(struct bufobj *bo, struct buf *bp)
{
	struct buf *nbp;

	if (bo->bo_dirty.bv_cnt > dirtybufthresh + 10) {
		(void) VOP_FSYNC(bp->b_vp, MNT_NOWAIT, curthread);
		altbufferflushes++;
	} else if (bo->bo_dirty.bv_cnt > dirtybufthresh) {
		BO_LOCK(bo);
		/*
		 * Try to find a buffer to flush.
		 */
		TAILQ_FOREACH(nbp, &bo->bo_dirty.bv_hd, b_bobufs) {
			if ((nbp->b_vflags & BV_BKGRDINPROG) ||
			    BUF_LOCK(nbp,
				     LK_EXCLUSIVE | LK_NOWAIT, NULL))
				continue;
			if (bp == nbp)
				panic("bdwrite: found ourselves");
			BO_UNLOCK(bo);
			/* Don't countdeps with the bo lock held. */
			if (buf_countdeps(nbp, 0)) {
				BO_LOCK(bo);
				BUF_UNLOCK(nbp);
				continue;
			}
			if (nbp->b_flags & B_CLUSTEROK) {
				vfs_bio_awrite(nbp);
			} else {
				bremfree(nbp);
				bawrite(nbp);
			}
			dirtybufferflushes++;
			break;
		}
		if (nbp == NULL)
			BO_UNLOCK(bo);
	}
}

/*
 * Delayed write. (Buffer is marked dirty).  Do not bother writing
 * anything if the buffer is marked invalid.
 *
 * Note that since the buffer must be completely valid, we can safely
 * set B_CACHE.  In fact, we have to set B_CACHE here rather then in
 * biodone() in order to prevent getblk from writing the buffer
 * out synchronously.
 */
void
bdwrite(struct buf *bp)
{
	struct thread *td = curthread;
	struct vnode *vp;
	struct bufobj *bo;

	CTR3(KTR_BUF, "bdwrite(%p) vp %p flags %X", bp, bp->b_vp, bp->b_flags);
	KASSERT(bp->b_bufobj != NULL, ("No b_bufobj %p", bp));
	KASSERT((bp->b_flags & B_BARRIER) == 0,
	    ("Barrier request in delayed write %p", bp));
	BUF_ASSERT_HELD(bp);

	if (bp->b_flags & B_INVAL) {
		brelse(bp);
		return;
	}

	/*
	 * If we have too many dirty buffers, don't create any more.
	 * If we are wildly over our limit, then force a complete
	 * cleanup. Otherwise, just keep the situation from getting
	 * out of control. Note that we have to avoid a recursive
	 * disaster and not try to clean up after our own cleanup!
	 */
	vp = bp->b_vp;
	bo = bp->b_bufobj;
	if ((td->td_pflags & (TDP_COWINPROGRESS|TDP_INBDFLUSH)) == 0) {
		td->td_pflags |= TDP_INBDFLUSH;
		BO_BDFLUSH(bo, bp);
		td->td_pflags &= ~TDP_INBDFLUSH;
	} else
		recursiveflushes++;

	bdirty(bp);
	/*
	 * Set B_CACHE, indicating that the buffer is fully valid.  This is
	 * true even of NFS now.
	 */
	bp->b_flags |= B_CACHE;

	/*
	 * This bmap keeps the system from needing to do the bmap later,
	 * perhaps when the system is attempting to do a sync.  Since it
	 * is likely that the indirect block -- or whatever other datastructure
	 * that the filesystem needs is still in memory now, it is a good
	 * thing to do this.  Note also, that if the pageout daemon is
	 * requesting a sync -- there might not be enough memory to do
	 * the bmap then...  So, this is important to do.
	 */
	if (vp->v_type != VCHR && bp->b_lblkno == bp->b_blkno) {
		VOP_BMAP(vp, bp->b_lblkno, NULL, &bp->b_blkno, NULL, NULL);
	}

	/*
	 * Set the *dirty* buffer range based upon the VM system dirty
	 * pages.
	 *
	 * Mark the buffer pages as clean.  We need to do this here to
	 * satisfy the vnode_pager and the pageout daemon, so that it
	 * thinks that the pages have been "cleaned".  Note that since
	 * the pages are in a delayed write buffer -- the VFS layer
	 * "will" see that the pages get written out on the next sync,
	 * or perhaps the cluster will be completed.
	 */
	vfs_clean_pages_dirty_buf(bp);
	bqrelse(bp);

	/*
	 * note: we cannot initiate I/O from a bdwrite even if we wanted to,
	 * due to the softdep code.
	 */
}

/*
 *	bdirty:
 *
 *	Turn buffer into delayed write request.  We must clear BIO_READ and
 *	B_RELBUF, and we must set B_DELWRI.  We reassign the buffer to 
 *	itself to properly update it in the dirty/clean lists.  We mark it
 *	B_DONE to ensure that any asynchronization of the buffer properly
 *	clears B_DONE ( else a panic will occur later ).  
 *
 *	bdirty() is kinda like bdwrite() - we have to clear B_INVAL which
 *	might have been set pre-getblk().  Unlike bwrite/bdwrite, bdirty()
 *	should only be called if the buffer is known-good.
 *
 *	Since the buffer is not on a queue, we do not update the numfreebuffers
 *	count.
 *
 *	The buffer must be on QUEUE_NONE.
 */
void
bdirty(struct buf *bp)
{

	CTR3(KTR_BUF, "bdirty(%p) vp %p flags %X",
	    bp, bp->b_vp, bp->b_flags);
	KASSERT(bp->b_bufobj != NULL, ("No b_bufobj %p", bp));
	KASSERT(bp->b_flags & B_REMFREE || bp->b_qindex == QUEUE_NONE,
	    ("bdirty: buffer %p still on queue %d", bp, bp->b_qindex));
	BUF_ASSERT_HELD(bp);
	bp->b_flags &= ~(B_RELBUF);
	bp->b_iocmd = BIO_WRITE;

	if ((bp->b_flags & B_DELWRI) == 0) {
		bp->b_flags |= /* XXX B_DONE | */ B_DELWRI;
		reassignbuf(bp);
		bdirtyadd();
	}
}

/*
 *	bundirty:
 *
 *	Clear B_DELWRI for buffer.
 *
 *	Since the buffer is not on a queue, we do not update the numfreebuffers
 *	count.
 *	
 *	The buffer must be on QUEUE_NONE.
 */

void
bundirty(struct buf *bp)
{

	CTR3(KTR_BUF, "bundirty(%p) vp %p flags %X", bp, bp->b_vp, bp->b_flags);
	KASSERT(bp->b_bufobj != NULL, ("No b_bufobj %p", bp));
	KASSERT(bp->b_flags & B_REMFREE || bp->b_qindex == QUEUE_NONE,
	    ("bundirty: buffer %p still on queue %d", bp, bp->b_qindex));
	BUF_ASSERT_HELD(bp);

	if (bp->b_flags & B_DELWRI) {
		bp->b_flags &= ~B_DELWRI;
		reassignbuf(bp);
		bdirtysub();
	}
	/*
	 * Since it is now being written, we can clear its deferred write flag.
	 */
	bp->b_flags &= ~B_DEFERRED;
}

/*
 *	bawrite:
 *
 *	Asynchronous write.  Start output on a buffer, but do not wait for
 *	it to complete.  The buffer is released when the output completes.
 *
 *	bwrite() ( or the VOP routine anyway ) is responsible for handling 
 *	B_INVAL buffers.  Not us.
 */
void
bawrite(struct buf *bp)
{

	bp->b_flags |= B_ASYNC;
	(void) bwrite(bp);
}

/*
 *	babarrierwrite:
 *
 *	Asynchronous barrier write.  Start output on a buffer, but do not
 *	wait for it to complete.  Place a write barrier after this write so
 *	that this buffer and all buffers written before it are committed to
 *	the disk before any buffers written after this write are committed
 *	to the disk.  The buffer is released when the output completes.
 */
void
babarrierwrite(struct buf *bp)
{

	bp->b_flags |= B_ASYNC | B_BARRIER;
	(void) bwrite(bp);
}

/*
 *	bbarrierwrite:
 *
 *	Synchronous barrier write.  Start output on a buffer and wait for
 *	it to complete.  Place a write barrier after this write so that
 *	this buffer and all buffers written before it are committed to 
 *	the disk before any buffers written after this write are committed
 *	to the disk.  The buffer is released when the output completes.
 */
int
bbarrierwrite(struct buf *bp)
{

	bp->b_flags |= B_BARRIER;
	return (bwrite(bp));
}

/*
 *	bwillwrite:
 *
 *	Called prior to the locking of any vnodes when we are expecting to
 *	write.  We do not want to starve the buffer cache with too many
 *	dirty buffers so we block here.  By blocking prior to the locking
 *	of any vnodes we attempt to avoid the situation where a locked vnode
 *	prevents the various system daemons from flushing related buffers.
 */
void
bwillwrite(void)
{

	if (numdirtybuffers >= hidirtybuffers) {
		mtx_lock(&bdirtylock);
		while (numdirtybuffers >= hidirtybuffers) {
			bdirtywait = 1;
			msleep(&bdirtywait, &bdirtylock, (PRIBIO + 4),
			    "flswai", 0);
		}
		mtx_unlock(&bdirtylock);
	}
}

/*
 * Return true if we have too many dirty buffers.
 */
int
buf_dirty_count_severe(void)
{

	return(numdirtybuffers >= hidirtybuffers);
}

/*
 *	brelse:
 *
 *	Release a busy buffer and, if requested, free its resources.  The
 *	buffer will be stashed in the appropriate bufqueue[] allowing it
 *	to be accessed later as a cache entity or reused for other purposes.
 */
void
brelse(struct buf *bp)
{
	int qindex;

	CTR3(KTR_BUF, "brelse(%p) vp %p flags %X",
	    bp, bp->b_vp, bp->b_flags);
	KASSERT(!(bp->b_flags & (B_CLUSTER|B_PAGING)),
	    ("brelse: inappropriate B_PAGING or B_CLUSTER bp %p", bp));
	KASSERT((bp->b_flags & B_VMIO) != 0 || (bp->b_flags & B_NOREUSE) == 0,
	    ("brelse: non-VMIO buffer marked NOREUSE"));

	if (BUF_LOCKRECURSED(bp)) {
		/*
		 * Do not process, in particular, do not handle the
		 * B_INVAL/B_RELBUF and do not release to free list.
		 */
		BUF_UNLOCK(bp);
		return;
	}

	if (bp->b_flags & B_MANAGED) {
		bqrelse(bp);
		return;
	}

	if ((bp->b_vflags & (BV_BKGRDINPROG | BV_BKGRDERR)) == BV_BKGRDERR) {
		BO_LOCK(bp->b_bufobj);
		bp->b_vflags &= ~BV_BKGRDERR;
		BO_UNLOCK(bp->b_bufobj);
		bdirty(bp);
	}
	if (bp->b_iocmd == BIO_WRITE && (bp->b_ioflags & BIO_ERROR) &&
	    bp->b_error == EIO && !(bp->b_flags & B_INVAL)) {
		/*
		 * Failed write, redirty.  Must clear BIO_ERROR to prevent
		 * pages from being scrapped.  If the error is anything
		 * other than an I/O error (EIO), assume that retrying
		 * is futile.
		 */
		bp->b_ioflags &= ~BIO_ERROR;
		bdirty(bp);
	} else if ((bp->b_flags & (B_NOCACHE | B_INVAL)) ||
	    (bp->b_ioflags & BIO_ERROR) || (bp->b_bufsize <= 0)) {
		/*
		 * Either a failed I/O or we were asked to free or not
		 * cache the buffer.
		 */
		bp->b_flags |= B_INVAL;
		if (!LIST_EMPTY(&bp->b_dep))
			buf_deallocate(bp);
		if (bp->b_flags & B_DELWRI)
			bdirtysub();
		bp->b_flags &= ~(B_DELWRI | B_CACHE);
		if ((bp->b_flags & B_VMIO) == 0) {
			allocbuf(bp, 0);
			if (bp->b_vp)
				brelvp(bp);
		}
	}

	/*
	 * We must clear B_RELBUF if B_DELWRI is set.  If vfs_vmio_truncate() 
	 * is called with B_DELWRI set, the underlying pages may wind up
	 * getting freed causing a previous write (bdwrite()) to get 'lost'
	 * because pages associated with a B_DELWRI bp are marked clean.
	 * 
	 * We still allow the B_INVAL case to call vfs_vmio_truncate(), even
	 * if B_DELWRI is set.
	 */
	if (bp->b_flags & B_DELWRI)
		bp->b_flags &= ~B_RELBUF;

	/*
	 * VMIO buffer rundown.  It is not very necessary to keep a VMIO buffer
	 * constituted, not even NFS buffers now.  Two flags effect this.  If
	 * B_INVAL, the struct buf is invalidated but the VM object is kept
	 * around ( i.e. so it is trivial to reconstitute the buffer later ).
	 *
	 * If BIO_ERROR or B_NOCACHE is set, pages in the VM object will be
	 * invalidated.  BIO_ERROR cannot be set for a failed write unless the
	 * buffer is also B_INVAL because it hits the re-dirtying code above.
	 *
	 * Normally we can do this whether a buffer is B_DELWRI or not.  If
	 * the buffer is an NFS buffer, it is tracking piecemeal writes or
	 * the commit state and we cannot afford to lose the buffer. If the
	 * buffer has a background write in progress, we need to keep it
	 * around to prevent it from being reconstituted and starting a second
	 * background write.
	 */
	if ((bp->b_flags & B_VMIO) && (bp->b_flags & B_NOCACHE ||
	    (bp->b_ioflags & BIO_ERROR && bp->b_iocmd == BIO_READ)) &&
	    !(bp->b_vp->v_mount != NULL &&
	    (bp->b_vp->v_mount->mnt_vfc->vfc_flags & VFCF_NETWORK) != 0 &&
	    !vn_isdisk(bp->b_vp, NULL) && (bp->b_flags & B_DELWRI))) {
		vfs_vmio_invalidate(bp);
		allocbuf(bp, 0);
	}

	if ((bp->b_flags & (B_INVAL | B_RELBUF)) != 0 ||
	    (bp->b_flags & (B_DELWRI | B_NOREUSE)) == B_NOREUSE) {
		allocbuf(bp, 0);
		bp->b_flags &= ~B_NOREUSE;
		if (bp->b_vp != NULL)
			brelvp(bp);
	}
			
	/*
	 * If the buffer has junk contents signal it and eventually
	 * clean up B_DELWRI and diassociate the vnode so that gbincore()
	 * doesn't find it.
	 */
	if (bp->b_bufsize == 0 || (bp->b_ioflags & BIO_ERROR) != 0 ||
	    (bp->b_flags & (B_INVAL | B_NOCACHE | B_RELBUF)) != 0)
		bp->b_flags |= B_INVAL;
	if (bp->b_flags & B_INVAL) {
		if (bp->b_flags & B_DELWRI)
			bundirty(bp);
		if (bp->b_vp)
			brelvp(bp);
	}

	/* buffers with no memory */
	if (bp->b_bufsize == 0) {
		bp->b_xflags &= ~(BX_BKGRDWRITE | BX_ALTDATA);
		if (bp->b_vflags & BV_BKGRDINPROG)
			panic("losing buffer 1");
		bufkvafree(bp);
		qindex = QUEUE_EMPTY;
		bp->b_flags |= B_AGE;
	/* buffers with junk contents */
	} else if (bp->b_flags & (B_INVAL | B_NOCACHE | B_RELBUF) ||
	    (bp->b_ioflags & BIO_ERROR)) {
		bp->b_xflags &= ~(BX_BKGRDWRITE | BX_ALTDATA);
		if (bp->b_vflags & BV_BKGRDINPROG)
			panic("losing buffer 2");
		qindex = QUEUE_CLEAN;
		bp->b_flags |= B_AGE;
	/* remaining buffers */
	} else if (bp->b_flags & B_DELWRI)
		qindex = QUEUE_DIRTY;
	else
		qindex = QUEUE_CLEAN;

	binsfree(bp, qindex);

	bp->b_flags &= ~(B_ASYNC | B_NOCACHE | B_AGE | B_RELBUF | B_DIRECT);
	if ((bp->b_flags & B_DELWRI) == 0 && (bp->b_xflags & BX_VNDIRTY))
		panic("brelse: not dirty");
	/* unlock */
	BUF_UNLOCK(bp);
}

/*
 * Release a buffer back to the appropriate queue but do not try to free
 * it.  The buffer is expected to be used again soon.
 *
 * bqrelse() is used by bdwrite() to requeue a delayed write, and used by
 * biodone() to requeue an async I/O on completion.  It is also used when
 * known good buffers need to be requeued but we think we may need the data
 * again soon.
 *
 * XXX we should be able to leave the B_RELBUF hint set on completion.
 */
void
bqrelse(struct buf *bp)
{
	int qindex;

	CTR3(KTR_BUF, "bqrelse(%p) vp %p flags %X", bp, bp->b_vp, bp->b_flags);
	KASSERT(!(bp->b_flags & (B_CLUSTER|B_PAGING)),
	    ("bqrelse: inappropriate B_PAGING or B_CLUSTER bp %p", bp));

	if (BUF_LOCKRECURSED(bp)) {
		/* do not release to free list */
		BUF_UNLOCK(bp);
		return;
	}
	bp->b_flags &= ~(B_ASYNC | B_NOCACHE | B_AGE | B_RELBUF);

	if (bp->b_flags & B_MANAGED) {
		if (bp->b_flags & B_REMFREE)
			bremfreef(bp);
		goto out;
	}

	/* buffers with stale but valid contents */
	if ((bp->b_flags & B_DELWRI) != 0 || (bp->b_vflags & (BV_BKGRDINPROG |
	    BV_BKGRDERR)) == BV_BKGRDERR) {
		BO_LOCK(bp->b_bufobj);
		bp->b_vflags &= ~BV_BKGRDERR;
		BO_UNLOCK(bp->b_bufobj);
		qindex = QUEUE_DIRTY;
	} else {
		if ((bp->b_flags & B_DELWRI) == 0 &&
		    (bp->b_xflags & BX_VNDIRTY))
			panic("bqrelse: not dirty");
		if ((bp->b_flags & B_NOREUSE) != 0) {
			brelse(bp);
			return;
		}
		qindex = QUEUE_CLEAN;
	}
	binsfree(bp, qindex);

out:
	/* unlock */
	BUF_UNLOCK(bp);
}

/*
 * Complete I/O to a VMIO backed page.  Validate the pages as appropriate,
 * restore bogus pages.
 */
static void
vfs_vmio_iodone(struct buf *bp)
{
	vm_ooffset_t foff;
	vm_page_t m;
	vm_object_t obj;
	struct vnode *vp;
	int bogus, i, iosize;

	obj = bp->b_bufobj->bo_object;
	KASSERT(obj->paging_in_progress >= bp->b_npages,
	    ("vfs_vmio_iodone: paging in progress(%d) < b_npages(%d)",
	    obj->paging_in_progress, bp->b_npages));

	vp = bp->b_vp;
	KASSERT(vp->v_holdcnt > 0,
	    ("vfs_vmio_iodone: vnode %p has zero hold count", vp));
	KASSERT(vp->v_object != NULL,
	    ("vfs_vmio_iodone: vnode %p has no vm_object", vp));

	foff = bp->b_offset;
	KASSERT(bp->b_offset != NOOFFSET,
	    ("vfs_vmio_iodone: bp %p has no buffer offset", bp));

	bogus = 0;
	iosize = bp->b_bcount - bp->b_resid;
	VM_OBJECT_WLOCK(obj);
	for (i = 0; i < bp->b_npages; i++) {
		int resid;

		resid = ((foff + PAGE_SIZE) & ~(off_t)PAGE_MASK) - foff;
		if (resid > iosize)
			resid = iosize;

		/*
		 * cleanup bogus pages, restoring the originals
		 */
		m = bp->b_pages[i];
		if (m == bogus_page) {
			bogus = 1;
			m = vm_page_lookup(obj, OFF_TO_IDX(foff));
			if (m == NULL)
				panic("biodone: page disappeared!");
			bp->b_pages[i] = m;
		} else if ((bp->b_iocmd == BIO_READ) && resid > 0) {
			/*
			 * In the write case, the valid and clean bits are
			 * already changed correctly ( see bdwrite() ), so we 
			 * only need to do this here in the read case.
			 */
			KASSERT((m->dirty & vm_page_bits(foff & PAGE_MASK,
			    resid)) == 0, ("vfs_vmio_iodone: page %p "
			    "has unexpected dirty bits", m));
			vfs_page_set_valid(bp, foff, m);
		}
		KASSERT(OFF_TO_IDX(foff) == m->pindex,
		    ("vfs_vmio_iodone: foff(%jd)/pindex(%ju) mismatch",
		    (intmax_t)foff, (uintmax_t)m->pindex));

		vm_page_sunbusy(m);
		vm_object_pip_subtract(obj, 1);
		foff = (foff + PAGE_SIZE) & ~(off_t)PAGE_MASK;
		iosize -= resid;
	}
	vm_object_pip_wakeupn(obj, 0);
	VM_OBJECT_WUNLOCK(obj);
	if (bogus && buf_mapped(bp)) {
		BUF_CHECK_MAPPED(bp);
		pmap_qenter(trunc_page((vm_offset_t)bp->b_data),
		    bp->b_pages, bp->b_npages);
	}
}

/*
 * Unwire a page held by a buf and place it on the appropriate vm queue.
 */
static void
vfs_vmio_unwire(struct buf *bp, vm_page_t m)
{
	bool freed;

	vm_page_lock(m);
	if (vm_page_unwire(m, PQ_NONE)) {
		/*
		 * Determine if the page should be freed before adding
		 * it to the inactive queue.
		 */
		if (m->valid == 0) {
			freed = !vm_page_busied(m);
			if (freed)
				vm_page_free(m);
		} else if ((bp->b_flags & B_DIRECT) != 0)
			freed = vm_page_try_to_free(m);
		else
			freed = false;
		if (!freed) {
			/*
			 * If the page is unlikely to be reused, let the
			 * VM know.  Otherwise, maintain LRU page
			 * ordering and put the page at the tail of the
			 * inactive queue.
			 */
			if ((bp->b_flags & B_NOREUSE) != 0)
				vm_page_deactivate_noreuse(m);
			else
				vm_page_deactivate(m);
		}
	}
	vm_page_unlock(m);
}

/*
 * Perform page invalidation when a buffer is released.  The fully invalid
 * pages will be reclaimed later in vfs_vmio_truncate().
 */
static void
vfs_vmio_invalidate(struct buf *bp)
{
	vm_object_t obj;
	vm_page_t m;
	int i, resid, poffset, presid;

	if (buf_mapped(bp)) {
		BUF_CHECK_MAPPED(bp);
		pmap_qremove(trunc_page((vm_offset_t)bp->b_data), bp->b_npages);
	} else
		BUF_CHECK_UNMAPPED(bp);
	/*
	 * Get the base offset and length of the buffer.  Note that 
	 * in the VMIO case if the buffer block size is not
	 * page-aligned then b_data pointer may not be page-aligned.
	 * But our b_pages[] array *IS* page aligned.
	 *
	 * block sizes less then DEV_BSIZE (usually 512) are not 
	 * supported due to the page granularity bits (m->valid,
	 * m->dirty, etc...). 
	 *
	 * See man buf(9) for more information
	 */
	obj = bp->b_bufobj->bo_object;
	resid = bp->b_bufsize;
	poffset = bp->b_offset & PAGE_MASK;
	VM_OBJECT_WLOCK(obj);
	for (i = 0; i < bp->b_npages; i++) {
		m = bp->b_pages[i];
		if (m == bogus_page)
			panic("vfs_vmio_invalidate: Unexpected bogus page.");
		bp->b_pages[i] = NULL;

		presid = resid > (PAGE_SIZE - poffset) ?
		    (PAGE_SIZE - poffset) : resid;
		KASSERT(presid >= 0, ("brelse: extra page"));
		while (vm_page_xbusied(m)) {
			vm_page_lock(m);
			VM_OBJECT_WUNLOCK(obj);
			vm_page_busy_sleep(m, "mbncsh");
			VM_OBJECT_WLOCK(obj);
		}
		if (pmap_page_wired_mappings(m) == 0)
			vm_page_set_invalid(m, poffset, presid);
		vfs_vmio_unwire(bp, m);
		resid -= presid;
		poffset = 0;
	}
	VM_OBJECT_WUNLOCK(obj);
	bp->b_npages = 0;
}

/*
 * Page-granular truncation of an existing VMIO buffer.
 */
static void
vfs_vmio_truncate(struct buf *bp, int desiredpages)
{
	vm_object_t obj;
	vm_page_t m;
	int i;

	if (bp->b_npages == desiredpages)
		return;

	if (buf_mapped(bp)) {
		BUF_CHECK_MAPPED(bp);
		pmap_qremove((vm_offset_t)trunc_page((vm_offset_t)bp->b_data) +
		    (desiredpages << PAGE_SHIFT), bp->b_npages - desiredpages);
	} else
		BUF_CHECK_UNMAPPED(bp);
	obj = bp->b_bufobj->bo_object;
	if (obj != NULL)
		VM_OBJECT_WLOCK(obj);
	for (i = desiredpages; i < bp->b_npages; i++) {
		m = bp->b_pages[i];
		KASSERT(m != bogus_page, ("allocbuf: bogus page found"));
		bp->b_pages[i] = NULL;
		vfs_vmio_unwire(bp, m);
	}
	if (obj != NULL)
		VM_OBJECT_WUNLOCK(obj);
	bp->b_npages = desiredpages;
}

/*
 * Byte granular extension of VMIO buffers.
 */
static void
vfs_vmio_extend(struct buf *bp, int desiredpages, int size)
{
	/*
	 * We are growing the buffer, possibly in a 
	 * byte-granular fashion.
	 */
	vm_object_t obj;
	vm_offset_t toff;
	vm_offset_t tinc;
	vm_page_t m;

	/*
	 * Step 1, bring in the VM pages from the object, allocating
	 * them if necessary.  We must clear B_CACHE if these pages
	 * are not valid for the range covered by the buffer.
	 */
	obj = bp->b_bufobj->bo_object;
	VM_OBJECT_WLOCK(obj);
	while (bp->b_npages < desiredpages) {
		/*
		 * We must allocate system pages since blocking
		 * here could interfere with paging I/O, no
		 * matter which process we are.
		 *
		 * Only exclusive busy can be tested here.
		 * Blocking on shared busy might lead to
		 * deadlocks once allocbuf() is called after
		 * pages are vfs_busy_pages().
		 */
		m = vm_page_grab(obj, OFF_TO_IDX(bp->b_offset) + bp->b_npages,
		    VM_ALLOC_NOBUSY | VM_ALLOC_SYSTEM |
		    VM_ALLOC_WIRED | VM_ALLOC_IGN_SBUSY |
		    VM_ALLOC_COUNT(desiredpages - bp->b_npages));
		if (m->valid == 0)
			bp->b_flags &= ~B_CACHE;
		bp->b_pages[bp->b_npages] = m;
		++bp->b_npages;
	}

	/*
	 * Step 2.  We've loaded the pages into the buffer,
	 * we have to figure out if we can still have B_CACHE
	 * set.  Note that B_CACHE is set according to the
	 * byte-granular range ( bcount and size ), not the
	 * aligned range ( newbsize ).
	 *
	 * The VM test is against m->valid, which is DEV_BSIZE
	 * aligned.  Needless to say, the validity of the data
	 * needs to also be DEV_BSIZE aligned.  Note that this
	 * fails with NFS if the server or some other client
	 * extends the file's EOF.  If our buffer is resized, 
	 * B_CACHE may remain set! XXX
	 */
	toff = bp->b_bcount;
	tinc = PAGE_SIZE - ((bp->b_offset + toff) & PAGE_MASK);
	while ((bp->b_flags & B_CACHE) && toff < size) {
		vm_pindex_t pi;

		if (tinc > (size - toff))
			tinc = size - toff;
		pi = ((bp->b_offset & PAGE_MASK) + toff) >> PAGE_SHIFT;
		m = bp->b_pages[pi];
		vfs_buf_test_cache(bp, bp->b_offset, toff, tinc, m);
		toff += tinc;
		tinc = PAGE_SIZE;
	}
	VM_OBJECT_WUNLOCK(obj);

	/*
	 * Step 3, fixup the KVA pmap.
	 */
	if (buf_mapped(bp))
		bpmap_qenter(bp);
	else
		BUF_CHECK_UNMAPPED(bp);
}

/*
 * Check to see if a block at a particular lbn is available for a clustered
 * write.
 */
static int
vfs_bio_clcheck(struct vnode *vp, int size, daddr_t lblkno, daddr_t blkno)
{
	struct buf *bpa;
	int match;

	match = 0;

	/* If the buf isn't in core skip it */
	if ((bpa = gbincore(&vp->v_bufobj, lblkno)) == NULL)
		return (0);

	/* If the buf is busy we don't want to wait for it */
	if (BUF_LOCK(bpa, LK_EXCLUSIVE | LK_NOWAIT, NULL) != 0)
		return (0);

	/* Only cluster with valid clusterable delayed write buffers */
	if ((bpa->b_flags & (B_DELWRI | B_CLUSTEROK | B_INVAL)) !=
	    (B_DELWRI | B_CLUSTEROK))
		goto done;

	if (bpa->b_bufsize != size)
		goto done;

	/*
	 * Check to see if it is in the expected place on disk and that the
	 * block has been mapped.
	 */
	if ((bpa->b_blkno != bpa->b_lblkno) && (bpa->b_blkno == blkno))
		match = 1;
done:
	BUF_UNLOCK(bpa);
	return (match);
}

/*
 *	vfs_bio_awrite:
 *
 *	Implement clustered async writes for clearing out B_DELWRI buffers.
 *	This is much better then the old way of writing only one buffer at
 *	a time.  Note that we may not be presented with the buffers in the 
 *	correct order, so we search for the cluster in both directions.
 */
int
vfs_bio_awrite(struct buf *bp)
{
	struct bufobj *bo;
	int i;
	int j;
	daddr_t lblkno = bp->b_lblkno;
	struct vnode *vp = bp->b_vp;
	int ncl;
	int nwritten;
	int size;
	int maxcl;
	int gbflags;

	bo = &vp->v_bufobj;
	gbflags = (bp->b_data == unmapped_buf) ? GB_UNMAPPED : 0;
	/*
	 * right now we support clustered writing only to regular files.  If
	 * we find a clusterable block we could be in the middle of a cluster
	 * rather then at the beginning.
	 */
	if ((vp->v_type == VREG) && 
	    (vp->v_mount != 0) && /* Only on nodes that have the size info */
	    (bp->b_flags & (B_CLUSTEROK | B_INVAL)) == B_CLUSTEROK) {

		size = vp->v_mount->mnt_stat.f_iosize;
		maxcl = MAXPHYS / size;

		BO_RLOCK(bo);
		for (i = 1; i < maxcl; i++)
			if (vfs_bio_clcheck(vp, size, lblkno + i,
			    bp->b_blkno + ((i * size) >> DEV_BSHIFT)) == 0)
				break;

		for (j = 1; i + j <= maxcl && j <= lblkno; j++) 
			if (vfs_bio_clcheck(vp, size, lblkno - j,
			    bp->b_blkno - ((j * size) >> DEV_BSHIFT)) == 0)
				break;
		BO_RUNLOCK(bo);
		--j;
		ncl = i + j;
		/*
		 * this is a possible cluster write
		 */
		if (ncl != 1) {
			BUF_UNLOCK(bp);
			nwritten = cluster_wbuild(vp, size, lblkno - j, ncl,
			    gbflags);
			return (nwritten);
		}
	}
	bremfree(bp);
	bp->b_flags |= B_ASYNC;
	/*
	 * default (old) behavior, writing out only one block
	 *
	 * XXX returns b_bufsize instead of b_bcount for nwritten?
	 */
	nwritten = bp->b_bufsize;
	(void) bwrite(bp);

	return (nwritten);
}

/*
 * Ask the bufdaemon for help, or act as bufdaemon itself, when a
 * locked vnode is supplied.
 */
static void
getnewbuf_bufd_help(struct vnode *vp, int gbflags, int slpflag, int slptimeo,
    int defrag)
{
	struct thread *td;
	char *waitmsg;
	int error, fl, flags, norunbuf;

	mtx_assert(&bqclean, MA_OWNED);

	if (defrag) {
		flags = VFS_BIO_NEED_BUFSPACE;
		waitmsg = "nbufkv";
	} else if (bufspace >= hibufspace) {
		waitmsg = "nbufbs";
		flags = VFS_BIO_NEED_BUFSPACE;
	} else {
		waitmsg = "newbuf";
		flags = VFS_BIO_NEED_ANY;
	}
	atomic_set_int(&needsbuffer, flags);
	mtx_unlock(&bqclean);

	bd_speedup();	/* heeeelp */
	if ((gbflags & GB_NOWAIT_BD) != 0)
		return;

	td = curthread;
	rw_wlock(&nblock);
	while ((needsbuffer & flags) != 0) {
		if (vp != NULL && vp->v_type != VCHR &&
		    (td->td_pflags & TDP_BUFNEED) == 0) {
			rw_wunlock(&nblock);
			/*
			 * getblk() is called with a vnode locked, and
			 * some majority of the dirty buffers may as
			 * well belong to the vnode.  Flushing the
			 * buffers there would make a progress that
			 * cannot be achieved by the buf_daemon, that
			 * cannot lock the vnode.
			 */
			norunbuf = ~(TDP_BUFNEED | TDP_NORUNNINGBUF) |
			    (td->td_pflags & TDP_NORUNNINGBUF);

			/*
			 * Play bufdaemon.  The getnewbuf() function
			 * may be called while the thread owns lock
			 * for another dirty buffer for the same
			 * vnode, which makes it impossible to use
			 * VOP_FSYNC() there, due to the buffer lock
			 * recursion.
			 */
			td->td_pflags |= TDP_BUFNEED | TDP_NORUNNINGBUF;
			fl = buf_flush(vp, flushbufqtarget);
			td->td_pflags &= norunbuf;
			rw_wlock(&nblock);
			if (fl != 0)
				continue;
			if ((needsbuffer & flags) == 0)
				break;
		}
		error = rw_sleep(__DEVOLATILE(void *, &needsbuffer), &nblock,
		    (PRIBIO + 4) | slpflag, waitmsg, slptimeo);
		if (error != 0)
			break;
	}
	rw_wunlock(&nblock);
}

static void
getnewbuf_reuse_bp(struct buf *bp, int qindex)
{

	CTR6(KTR_BUF, "getnewbuf(%p) vp %p flags %X kvasize %d bufsize %d "
	    "queue %d (recycling)", bp, bp->b_vp, bp->b_flags,
	     bp->b_kvasize, bp->b_bufsize, qindex);
	mtx_assert(&bqclean, MA_NOTOWNED);

	/*
	 * Note: we no longer distinguish between VMIO and non-VMIO
	 * buffers.
	 */
	KASSERT((bp->b_flags & (B_DELWRI | B_NOREUSE)) == 0,
	    ("invalid buffer %p flags %#x found in queue %d", bp, bp->b_flags,
	    qindex));

	/*
	 * When recycling a clean buffer we have to truncate it and
	 * release the vnode.
	 */
	if (qindex == QUEUE_CLEAN) {
		allocbuf(bp, 0);
		if (bp->b_vp != NULL)
			brelvp(bp);
	}

	/*
	 * Get the rest of the buffer freed up.  b_kva* is still valid
	 * after this operation.
	 */
	if (bp->b_rcred != NOCRED) {
		crfree(bp->b_rcred);
		bp->b_rcred = NOCRED;
	}
	if (bp->b_wcred != NOCRED) {
		crfree(bp->b_wcred);
		bp->b_wcred = NOCRED;
	}
	if (!LIST_EMPTY(&bp->b_dep))
		buf_deallocate(bp);
	if (bp->b_vflags & BV_BKGRDINPROG)
		panic("losing buffer 3");
	KASSERT(bp->b_vp == NULL, ("bp: %p still has vnode %p.  qindex: %d",
	    bp, bp->b_vp, qindex));
	KASSERT((bp->b_xflags & (BX_VNCLEAN|BX_VNDIRTY)) == 0,
	    ("bp: %p still on a buffer list. xflags %X", bp, bp->b_xflags));
	KASSERT(bp->b_npages == 0,
	    ("bp: %p still has %d vm pages\n", bp, bp->b_npages));

	bp->b_flags = 0;
	bp->b_ioflags = 0;
	bp->b_xflags = 0;
	KASSERT((bp->b_flags & B_INFREECNT) == 0,
	    ("buf %p still counted as free?", bp));
	bp->b_vflags = 0;
	bp->b_vp = NULL;
	bp->b_blkno = bp->b_lblkno = 0;
	bp->b_offset = NOOFFSET;
	bp->b_iodone = 0;
	bp->b_error = 0;
	bp->b_resid = 0;
	bp->b_bcount = 0;
	bp->b_npages = 0;
	bp->b_dirtyoff = bp->b_dirtyend = 0;
	bp->b_bufobj = NULL;
	bp->b_pin_count = 0;
	bp->b_data = bp->b_kvabase;
	bp->b_fsprivate1 = NULL;
	bp->b_fsprivate2 = NULL;
	bp->b_fsprivate3 = NULL;

	LIST_INIT(&bp->b_dep);
}

static struct buf *
getnewbuf_scan(int maxsize, int defrag, int unmapped, int metadata)
{
	struct buf *bp, *nbp;
	int nqindex, qindex, pass;

	KASSERT(!unmapped || !defrag, ("both unmapped and defrag"));

	pass = 0;
restart:
	if (pass != 0)
		atomic_add_int(&getnewbufrestarts, 1);

	nbp = NULL;
	mtx_lock(&bqclean);
	/*
	 * If we're not defragging or low on bufspace attempt to make a new
	 * buf from a header.
	 */
	if (defrag == 0 && bufspace + maxsize < hibufspace) {
		nqindex = QUEUE_EMPTY;
		nbp = TAILQ_FIRST(&bufqueues[nqindex]);
	}
	/*
	 * All available buffers might be clean or we need to start recycling.
	 */
	if (nbp == NULL) {
		nqindex = QUEUE_CLEAN;
		nbp = TAILQ_FIRST(&bufqueues[QUEUE_CLEAN]);
	}

	/*
	 * Run scan, possibly freeing data and/or kva mappings on the fly
	 * depending.
	 */
	while ((bp = nbp) != NULL) {
		qindex = nqindex;

		/*
		 * Calculate next bp (we can only use it if we do not
		 * release the bqlock)
		 */
		if ((nbp = TAILQ_NEXT(bp, b_freelist)) == NULL) {
			switch (qindex) {
			case QUEUE_EMPTY:
				nqindex = QUEUE_CLEAN;
				nbp = TAILQ_FIRST(&bufqueues[nqindex]);
				if (nbp != NULL)
					break;
				/* FALLTHROUGH */
			case QUEUE_CLEAN:
				if (metadata && pass == 0) {
					pass = 1;
					nqindex = QUEUE_EMPTY;
					nbp = TAILQ_FIRST(&bufqueues[nqindex]);
				}
				/*
				 * nbp is NULL. 
				 */
				break;
			}
		}
		/*
		 * If we are defragging then we need a buffer with 
		 * b_kvasize != 0.  This situation occurs when we
		 * have many unmapped bufs.
		 */
		if (defrag && bp->b_kvasize == 0)
			continue;

		/*
		 * Start freeing the bp.  This is somewhat involved.  nbp
		 * remains valid only for QUEUE_EMPTY[KVA] bp's.
		 */
		if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT, NULL) != 0)
			continue;
		/*
		 * BKGRDINPROG can only be set with the buf and bufobj
		 * locks both held.  We tolerate a race to clear it here.
		 */
		if (bp->b_vflags & BV_BKGRDINPROG) {
			BUF_UNLOCK(bp);
			continue;
		}

		/*
		 * Requeue the background write buffer with error.
		 */
		if ((bp->b_vflags & BV_BKGRDERR) != 0) {
			bremfreel(bp);
			mtx_unlock(&bqclean);
			bqrelse(bp);
			continue;
		}

		KASSERT(bp->b_qindex == qindex,
		    ("getnewbuf: inconsistent queue %d bp %p", qindex, bp));

		bremfreel(bp);
		mtx_unlock(&bqclean);

		/*
		 * NOTE:  nbp is now entirely invalid.  We can only restart
		 * the scan from this point on.
		 */
		getnewbuf_reuse_bp(bp, qindex);
		mtx_assert(&bqclean, MA_NOTOWNED);

		/*
		 * If we are defragging then free the buffer.
		 */
		if (defrag) {
			bp->b_flags |= B_INVAL;
			brelse(bp);
			defrag = 0;
			goto restart;
		}

		/*
		 * Notify any waiters for the buffer lock about
		 * identity change by freeing the buffer.
		 */
		if (qindex == QUEUE_CLEAN && BUF_LOCKWAITERS(bp)) {
			bp->b_flags |= B_INVAL;
			brelse(bp);
			goto restart;
		}

		if (metadata)
			break;

		/*
		 * If we are overcomitted then recover the buffer and its
		 * KVM space.  This occurs in rare situations when multiple
		 * processes are blocked in getnewbuf() or allocbuf().
		 */
		if (bufspace >= hibufspace && bp->b_kvasize != 0) {
			bp->b_flags |= B_INVAL;
			brelse(bp);
			goto restart;
		}
		break;
	}
	return (bp);
}

/*
 *	getnewbuf:
 *
 *	Find and initialize a new buffer header, freeing up existing buffers
 *	in the bufqueues as necessary.  The new buffer is returned locked.
 *
 *	Important:  B_INVAL is not set.  If the caller wishes to throw the
 *	buffer away, the caller must set B_INVAL prior to calling brelse().
 *
 *	We block if:
 *		We have insufficient buffer headers
 *		We have insufficient buffer space
 *		buffer_arena is too fragmented ( space reservation fails )
 *		If we have to flush dirty buffers ( but we try to avoid this )
 */
static struct buf *
getnewbuf(struct vnode *vp, int slpflag, int slptimeo, int size, int maxsize,
    int gbflags)
{
	struct buf *bp;
	int defrag, metadata;

	KASSERT((gbflags & (GB_UNMAPPED | GB_KVAALLOC)) != GB_KVAALLOC,
	    ("GB_KVAALLOC only makes sense with GB_UNMAPPED"));
	if (!unmapped_buf_allowed)
		gbflags &= ~(GB_UNMAPPED | GB_KVAALLOC);

	defrag = 0;
	if (vp == NULL || (vp->v_vflag & (VV_MD | VV_SYSTEM)) != 0 ||
	    vp->v_type == VCHR)
		metadata = 1;
	else
		metadata = 0;
	/*
	 * We can't afford to block since we might be holding a vnode lock,
	 * which may prevent system daemons from running.  We deal with
	 * low-memory situations by proactively returning memory and running
	 * async I/O rather then sync I/O.
	 */
	atomic_add_int(&getnewbufcalls, 1);
restart:
	bp = getnewbuf_scan(maxsize, defrag, (gbflags & (GB_UNMAPPED |
	    GB_KVAALLOC)) == GB_UNMAPPED, metadata);
	if (bp != NULL)
		defrag = 0;

	/*
	 * If we exhausted our list, sleep as appropriate.  We may have to
	 * wakeup various daemons and write out some dirty buffers.
	 *
	 * Generally we are sleeping due to insufficient buffer space.
	 */
	if (bp == NULL) {
		mtx_assert(&bqclean, MA_OWNED);
		getnewbuf_bufd_help(vp, gbflags, slpflag, slptimeo, defrag);
		mtx_assert(&bqclean, MA_NOTOWNED);
	} else if ((gbflags & (GB_UNMAPPED | GB_KVAALLOC)) == GB_UNMAPPED) {
		mtx_assert(&bqclean, MA_NOTOWNED);

		bufkvafree(bp);
		atomic_add_int(&bufreusecnt, 1);
	} else {
		mtx_assert(&bqclean, MA_NOTOWNED);

		/*
		 * We finally have a valid bp.  We aren't quite out of the
		 * woods, we still have to reserve kva space. In order to
		 * keep fragmentation sane we only allocate kva in BKVASIZE
		 * chunks.
		 */
		maxsize = (maxsize + BKVAMASK) & ~BKVAMASK;

		if (maxsize != bp->b_kvasize &&
		    bufkvaalloc(bp, maxsize, gbflags)) {
			defrag = 1;
			bp->b_flags |= B_INVAL;
			brelse(bp);
			goto restart;
		} else if ((gbflags & (GB_UNMAPPED | GB_KVAALLOC)) ==
		    (GB_UNMAPPED | GB_KVAALLOC)) {
			bp->b_data = unmapped_buf;
			BUF_CHECK_UNMAPPED(bp);
		}
		atomic_add_int(&bufreusecnt, 1);
	}
	return (bp);
}

/*
 *	buf_daemon:
 *
 *	buffer flushing daemon.  Buffers are normally flushed by the
 *	update daemon but if it cannot keep up this process starts to
 *	take the load in an attempt to prevent getnewbuf() from blocking.
 */

static struct kproc_desc buf_kp = {
	"bufdaemon",
	buf_daemon,
	&bufdaemonproc
};
SYSINIT(bufdaemon, SI_SUB_KTHREAD_BUF, SI_ORDER_FIRST, kproc_start, &buf_kp);

static int
buf_flush(struct vnode *vp, int target)
{
	int flushed;

	flushed = flushbufqueues(vp, target, 0);
	if (flushed == 0) {
		/*
		 * Could not find any buffers without rollback
		 * dependencies, so just write the first one
		 * in the hopes of eventually making progress.
		 */
		if (vp != NULL && target > 2)
			target /= 2;
		flushbufqueues(vp, target, 1);
	}
	return (flushed);
}

static void
buf_daemon()
{
	int lodirty;

	/*
	 * This process needs to be suspended prior to shutdown sync.
	 */
	EVENTHANDLER_REGISTER(shutdown_pre_sync, kproc_shutdown, bufdaemonproc,
	    SHUTDOWN_PRI_LAST);

	/*
	 * This process is allowed to take the buffer cache to the limit
	 */
	curthread->td_pflags |= TDP_NORUNNINGBUF | TDP_BUFNEED;
	mtx_lock(&bdlock);
	for (;;) {
		bd_request = 0;
		mtx_unlock(&bdlock);

		kproc_suspend_check(bufdaemonproc);
		lodirty = lodirtybuffers;
		if (bd_speedupreq) {
			lodirty = numdirtybuffers / 2;
			bd_speedupreq = 0;
		}
		/*
		 * Do the flush.  Limit the amount of in-transit I/O we
		 * allow to build up, otherwise we would completely saturate
		 * the I/O system.
		 */
		while (numdirtybuffers > lodirty) {
			if (buf_flush(NULL, numdirtybuffers - lodirty) == 0)
				break;
			kern_yield(PRI_USER);
		}

		/*
		 * Only clear bd_request if we have reached our low water
		 * mark.  The buf_daemon normally waits 1 second and
		 * then incrementally flushes any dirty buffers that have
		 * built up, within reason.
		 *
		 * If we were unable to hit our low water mark and couldn't
		 * find any flushable buffers, we sleep for a short period
		 * to avoid endless loops on unlockable buffers.
		 */
		mtx_lock(&bdlock);
		if (numdirtybuffers <= lodirtybuffers) {
			/*
			 * We reached our low water mark, reset the
			 * request and sleep until we are needed again.
			 * The sleep is just so the suspend code works.
			 */
			bd_request = 0;
			/*
			 * Do an extra wakeup in case dirty threshold
			 * changed via sysctl and the explicit transition
			 * out of shortfall was missed.
			 */
			bdirtywakeup();
			if (runningbufspace <= lorunningspace)
				runningwakeup();
			msleep(&bd_request, &bdlock, PVM, "psleep", hz);
		} else {
			/*
			 * We couldn't find any flushable dirty buffers but
			 * still have too many dirty buffers, we
			 * have to sleep and try again.  (rare)
			 */
			msleep(&bd_request, &bdlock, PVM, "qsleep", hz / 10);
		}
	}
}

/*
 *	flushbufqueues:
 *
 *	Try to flush a buffer in the dirty queue.  We must be careful to
 *	free up B_INVAL buffers instead of write them, which NFS is 
 *	particularly sensitive to.
 */
static int flushwithdeps = 0;
SYSCTL_INT(_vfs, OID_AUTO, flushwithdeps, CTLFLAG_RW, &flushwithdeps,
    0, "Number of buffers flushed with dependecies that require rollbacks");

static int
flushbufqueues(struct vnode *lvp, int target, int flushdeps)
{
	struct buf *sentinel;
	struct vnode *vp;
	struct mount *mp;
	struct buf *bp;
	int hasdeps;
	int flushed;
	int queue;
	int error;
	bool unlock;

	flushed = 0;
	queue = QUEUE_DIRTY;
	bp = NULL;
	sentinel = malloc(sizeof(struct buf), M_TEMP, M_WAITOK | M_ZERO);
	sentinel->b_qindex = QUEUE_SENTINEL;
	mtx_lock(&bqdirty);
	TAILQ_INSERT_HEAD(&bufqueues[queue], sentinel, b_freelist);
	mtx_unlock(&bqdirty);
	while (flushed != target) {
		maybe_yield();
		mtx_lock(&bqdirty);
		bp = TAILQ_NEXT(sentinel, b_freelist);
		if (bp != NULL) {
			TAILQ_REMOVE(&bufqueues[queue], sentinel, b_freelist);
			TAILQ_INSERT_AFTER(&bufqueues[queue], bp, sentinel,
			    b_freelist);
		} else {
			mtx_unlock(&bqdirty);
			break;
		}
		/*
		 * Skip sentinels inserted by other invocations of the
		 * flushbufqueues(), taking care to not reorder them.
		 *
		 * Only flush the buffers that belong to the
		 * vnode locked by the curthread.
		 */
		if (bp->b_qindex == QUEUE_SENTINEL || (lvp != NULL &&
		    bp->b_vp != lvp)) {
			mtx_unlock(&bqdirty);
 			continue;
		}
		error = BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT, NULL);
		mtx_unlock(&bqdirty);
		if (error != 0)
			continue;
		if (bp->b_pin_count > 0) {
			BUF_UNLOCK(bp);
			continue;
		}
		/*
		 * BKGRDINPROG can only be set with the buf and bufobj
		 * locks both held.  We tolerate a race to clear it here.
		 */
		if ((bp->b_vflags & BV_BKGRDINPROG) != 0 ||
		    (bp->b_flags & B_DELWRI) == 0) {
			BUF_UNLOCK(bp);
			continue;
		}
		if (bp->b_flags & B_INVAL) {
			bremfreef(bp);
			brelse(bp);
			flushed++;
			continue;
		}

		if (!LIST_EMPTY(&bp->b_dep) && buf_countdeps(bp, 0)) {
			if (flushdeps == 0) {
				BUF_UNLOCK(bp);
				continue;
			}
			hasdeps = 1;
		} else
			hasdeps = 0;
		/*
		 * We must hold the lock on a vnode before writing
		 * one of its buffers. Otherwise we may confuse, or
		 * in the case of a snapshot vnode, deadlock the
		 * system.
		 *
		 * The lock order here is the reverse of the normal
		 * of vnode followed by buf lock.  This is ok because
		 * the NOWAIT will prevent deadlock.
		 */
		vp = bp->b_vp;
		if (vn_start_write(vp, &mp, V_NOWAIT) != 0) {
			BUF_UNLOCK(bp);
			continue;
		}
		if (lvp == NULL) {
			unlock = true;
			error = vn_lock(vp, LK_EXCLUSIVE | LK_NOWAIT);
		} else {
			ASSERT_VOP_LOCKED(vp, "getbuf");
			unlock = false;
			error = VOP_ISLOCKED(vp) == LK_EXCLUSIVE ? 0 :
			    vn_lock(vp, LK_TRYUPGRADE);
		}
		if (error == 0) {
			CTR3(KTR_BUF, "flushbufqueue(%p) vp %p flags %X",
			    bp, bp->b_vp, bp->b_flags);
			if (curproc == bufdaemonproc) {
				vfs_bio_awrite(bp);
			} else {
				bremfree(bp);
				bwrite(bp);
				notbufdflushes++;
			}
			vn_finished_write(mp);
			if (unlock)
				VOP_UNLOCK(vp, 0);
			flushwithdeps += hasdeps;
			flushed++;

			/*
			 * Sleeping on runningbufspace while holding
			 * vnode lock leads to deadlock.
			 */
			if (curproc == bufdaemonproc &&
			    runningbufspace > hirunningspace)
				waitrunningbufspace();
			continue;
		}
		vn_finished_write(mp);
		BUF_UNLOCK(bp);
	}
	mtx_lock(&bqdirty);
	TAILQ_REMOVE(&bufqueues[queue], sentinel, b_freelist);
	mtx_unlock(&bqdirty);
	free(sentinel, M_TEMP);
	return (flushed);
}

/*
 * Check to see if a block is currently memory resident.
 */
struct buf *
incore(struct bufobj *bo, daddr_t blkno)
{
	struct buf *bp;

	BO_RLOCK(bo);
	bp = gbincore(bo, blkno);
	BO_RUNLOCK(bo);
	return (bp);
}

/*
 * Returns true if no I/O is needed to access the
 * associated VM object.  This is like incore except
 * it also hunts around in the VM system for the data.
 */

static int
inmem(struct vnode * vp, daddr_t blkno)
{
	vm_object_t obj;
	vm_offset_t toff, tinc, size;
	vm_page_t m;
	vm_ooffset_t off;

	ASSERT_VOP_LOCKED(vp, "inmem");

	if (incore(&vp->v_bufobj, blkno))
		return 1;
	if (vp->v_mount == NULL)
		return 0;
	obj = vp->v_object;
	if (obj == NULL)
		return (0);

	size = PAGE_SIZE;
	if (size > vp->v_mount->mnt_stat.f_iosize)
		size = vp->v_mount->mnt_stat.f_iosize;
	off = (vm_ooffset_t)blkno * (vm_ooffset_t)vp->v_mount->mnt_stat.f_iosize;

	VM_OBJECT_RLOCK(obj);
	for (toff = 0; toff < vp->v_mount->mnt_stat.f_iosize; toff += tinc) {
		m = vm_page_lookup(obj, OFF_TO_IDX(off + toff));
		if (!m)
			goto notinmem;
		tinc = size;
		if (tinc > PAGE_SIZE - ((toff + off) & PAGE_MASK))
			tinc = PAGE_SIZE - ((toff + off) & PAGE_MASK);
		if (vm_page_is_valid(m,
		    (vm_offset_t) ((toff + off) & PAGE_MASK), tinc) == 0)
			goto notinmem;
	}
	VM_OBJECT_RUNLOCK(obj);
	return 1;

notinmem:
	VM_OBJECT_RUNLOCK(obj);
	return (0);
}

/*
 * Set the dirty range for a buffer based on the status of the dirty
 * bits in the pages comprising the buffer.  The range is limited
 * to the size of the buffer.
 *
 * Tell the VM system that the pages associated with this buffer
 * are clean.  This is used for delayed writes where the data is
 * going to go to disk eventually without additional VM intevention.
 *
 * Note that while we only really need to clean through to b_bcount, we
 * just go ahead and clean through to b_bufsize.
 */
static void
vfs_clean_pages_dirty_buf(struct buf *bp)
{
	vm_ooffset_t foff, noff, eoff;
	vm_page_t m;
	int i;

	if ((bp->b_flags & B_VMIO) == 0 || bp->b_bufsize == 0)
		return;

	foff = bp->b_offset;
	KASSERT(bp->b_offset != NOOFFSET,
	    ("vfs_clean_pages_dirty_buf: no buffer offset"));

	VM_OBJECT_WLOCK(bp->b_bufobj->bo_object);
	vfs_drain_busy_pages(bp);
	vfs_setdirty_locked_object(bp);
	for (i = 0; i < bp->b_npages; i++) {
		noff = (foff + PAGE_SIZE) & ~(off_t)PAGE_MASK;
		eoff = noff;
		if (eoff > bp->b_offset + bp->b_bufsize)
			eoff = bp->b_offset + bp->b_bufsize;
		m = bp->b_pages[i];
		vfs_page_set_validclean(bp, foff, m);
		/* vm_page_clear_dirty(m, foff & PAGE_MASK, eoff - foff); */
		foff = noff;
	}
	VM_OBJECT_WUNLOCK(bp->b_bufobj->bo_object);
}

static void
vfs_setdirty_locked_object(struct buf *bp)
{
	vm_object_t object;
	int i;

	object = bp->b_bufobj->bo_object;
	VM_OBJECT_ASSERT_WLOCKED(object);

	/*
	 * We qualify the scan for modified pages on whether the
	 * object has been flushed yet.
	 */
	if ((object->flags & OBJ_MIGHTBEDIRTY) != 0) {
		vm_offset_t boffset;
		vm_offset_t eoffset;

		/*
		 * test the pages to see if they have been modified directly
		 * by users through the VM system.
		 */
		for (i = 0; i < bp->b_npages; i++)
			vm_page_test_dirty(bp->b_pages[i]);

		/*
		 * Calculate the encompassing dirty range, boffset and eoffset,
		 * (eoffset - boffset) bytes.
		 */

		for (i = 0; i < bp->b_npages; i++) {
			if (bp->b_pages[i]->dirty)
				break;
		}
		boffset = (i << PAGE_SHIFT) - (bp->b_offset & PAGE_MASK);

		for (i = bp->b_npages - 1; i >= 0; --i) {
			if (bp->b_pages[i]->dirty) {
				break;
			}
		}
		eoffset = ((i + 1) << PAGE_SHIFT) - (bp->b_offset & PAGE_MASK);

		/*
		 * Fit it to the buffer.
		 */

		if (eoffset > bp->b_bcount)
			eoffset = bp->b_bcount;

		/*
		 * If we have a good dirty range, merge with the existing
		 * dirty range.
		 */

		if (boffset < eoffset) {
			if (bp->b_dirtyoff > boffset)
				bp->b_dirtyoff = boffset;
			if (bp->b_dirtyend < eoffset)
				bp->b_dirtyend = eoffset;
		}
	}
}

/*
 * Allocate the KVA mapping for an existing buffer.
 * If an unmapped buffer is provided but a mapped buffer is requested, take
 * also care to properly setup mappings between pages and KVA.
 */
static void
bp_unmapped_get_kva(struct buf *bp, daddr_t blkno, int size, int gbflags)
{
	struct buf *scratch_bp;
	int bsize, maxsize, need_mapping, need_kva;
	off_t offset;

	need_mapping = bp->b_data == unmapped_buf &&
	    (gbflags & GB_UNMAPPED) == 0;
	need_kva = bp->b_kvabase == unmapped_buf &&
	    bp->b_data == unmapped_buf &&
	    (gbflags & GB_KVAALLOC) != 0;
	if (!need_mapping && !need_kva)
		return;

	BUF_CHECK_UNMAPPED(bp);

	if (need_mapping && bp->b_kvabase != unmapped_buf) {
		/*
		 * Buffer is not mapped, but the KVA was already
		 * reserved at the time of the instantiation.  Use the
		 * allocated space.
		 */
		goto has_addr;
	}

	/*
	 * Calculate the amount of the address space we would reserve
	 * if the buffer was mapped.
	 */
	bsize = vn_isdisk(bp->b_vp, NULL) ? DEV_BSIZE : bp->b_bufobj->bo_bsize;
	KASSERT(bsize != 0, ("bsize == 0, check bo->bo_bsize"));
	offset = blkno * bsize;
	maxsize = size + (offset & PAGE_MASK);
	maxsize = imax(maxsize, bsize);

mapping_loop:
	if (bufkvaalloc(bp, maxsize, gbflags)) {
		/*
		 * Request defragmentation. getnewbuf() returns us the
		 * allocated space by the scratch buffer KVA.
		 */
		scratch_bp = getnewbuf(bp->b_vp, 0, 0, size, maxsize, gbflags |
		    (GB_UNMAPPED | GB_KVAALLOC));
		if (scratch_bp == NULL) {
			if ((gbflags & GB_NOWAIT_BD) != 0) {
				/*
				 * XXXKIB: defragmentation cannot
				 * succeed, not sure what else to do.
				 */
				panic("GB_NOWAIT_BD and GB_UNMAPPED %p", bp);
			}
			atomic_add_int(&mappingrestarts, 1);
			goto mapping_loop;
		}
		KASSERT(scratch_bp->b_kvabase != unmapped_buf,
		    ("scratch bp has no KVA %p", scratch_bp));
		/* Grab pointers. */
		bp->b_kvabase = scratch_bp->b_kvabase;
		bp->b_kvasize = scratch_bp->b_kvasize;
		bp->b_data = scratch_bp->b_data;

		/* Get rid of the scratch buffer. */
		scratch_bp->b_kvasize = 0;
		scratch_bp->b_flags |= B_INVAL;
		scratch_bp->b_data = scratch_bp->b_kvabase = unmapped_buf;
		brelse(scratch_bp);
	}
has_addr:
	if (need_mapping) {
		/* b_offset is handled by bpmap_qenter. */
		bp->b_data = bp->b_kvabase;
		BUF_CHECK_MAPPED(bp);
		bpmap_qenter(bp);
	}
}

/*
 *	getblk:
 *
 *	Get a block given a specified block and offset into a file/device.
 *	The buffers B_DONE bit will be cleared on return, making it almost
 * 	ready for an I/O initiation.  B_INVAL may or may not be set on 
 *	return.  The caller should clear B_INVAL prior to initiating a
 *	READ.
 *
 *	For a non-VMIO buffer, B_CACHE is set to the opposite of B_INVAL for
 *	an existing buffer.
 *
 *	For a VMIO buffer, B_CACHE is modified according to the backing VM.
 *	If getblk()ing a previously 0-sized invalid buffer, B_CACHE is set
 *	and then cleared based on the backing VM.  If the previous buffer is
 *	non-0-sized but invalid, B_CACHE will be cleared.
 *
 *	If getblk() must create a new buffer, the new buffer is returned with
 *	both B_INVAL and B_CACHE clear unless it is a VMIO buffer, in which
 *	case it is returned with B_INVAL clear and B_CACHE set based on the
 *	backing VM.
 *
 *	getblk() also forces a bwrite() for any B_DELWRI buffer whos
 *	B_CACHE bit is clear.
 *	
 *	What this means, basically, is that the caller should use B_CACHE to
 *	determine whether the buffer is fully valid or not and should clear
 *	B_INVAL prior to issuing a read.  If the caller intends to validate
 *	the buffer by loading its data area with something, the caller needs
 *	to clear B_INVAL.  If the caller does this without issuing an I/O, 
 *	the caller should set B_CACHE ( as an optimization ), else the caller
 *	should issue the I/O and biodone() will set B_CACHE if the I/O was
 *	a write attempt or if it was a successfull read.  If the caller 
 *	intends to issue a READ, the caller must clear B_INVAL and BIO_ERROR
 *	prior to issuing the READ.  biodone() will *not* clear B_INVAL.
 */
struct buf *
getblk(struct vnode *vp, daddr_t blkno, int size, int slpflag, int slptimeo,
    int flags)
{
	struct buf *bp;
	struct bufobj *bo;
	int bsize, error, maxsize, vmio;
	off_t offset;

	CTR3(KTR_BUF, "getblk(%p, %ld, %d)", vp, (long)blkno, size);
	KASSERT((flags & (GB_UNMAPPED | GB_KVAALLOC)) != GB_KVAALLOC,
	    ("GB_KVAALLOC only makes sense with GB_UNMAPPED"));
	ASSERT_VOP_LOCKED(vp, "getblk");
	if (size > MAXBCACHEBUF)
		panic("getblk: size(%d) > MAXBCACHEBUF(%d)\n", size,
		    MAXBCACHEBUF);
	if (!unmapped_buf_allowed)
		flags &= ~(GB_UNMAPPED | GB_KVAALLOC);

	bo = &vp->v_bufobj;
loop:
	BO_RLOCK(bo);
	bp = gbincore(bo, blkno);
	if (bp != NULL) {
		int lockflags;
		/*
		 * Buffer is in-core.  If the buffer is not busy nor managed,
		 * it must be on a queue.
		 */
		lockflags = LK_EXCLUSIVE | LK_SLEEPFAIL | LK_INTERLOCK;

		if (flags & GB_LOCK_NOWAIT)
			lockflags |= LK_NOWAIT;

		error = BUF_TIMELOCK(bp, lockflags,
		    BO_LOCKPTR(bo), "getblk", slpflag, slptimeo);

		/*
		 * If we slept and got the lock we have to restart in case
		 * the buffer changed identities.
		 */
		if (error == ENOLCK)
			goto loop;
		/* We timed out or were interrupted. */
		else if (error)
			return (NULL);
		/* If recursed, assume caller knows the rules. */
		else if (BUF_LOCKRECURSED(bp))
			goto end;

		/*
		 * The buffer is locked.  B_CACHE is cleared if the buffer is 
		 * invalid.  Otherwise, for a non-VMIO buffer, B_CACHE is set
		 * and for a VMIO buffer B_CACHE is adjusted according to the
		 * backing VM cache.
		 */
		if (bp->b_flags & B_INVAL)
			bp->b_flags &= ~B_CACHE;
		else if ((bp->b_flags & (B_VMIO | B_INVAL)) == 0)
			bp->b_flags |= B_CACHE;
		if (bp->b_flags & B_MANAGED)
			MPASS(bp->b_qindex == QUEUE_NONE);
		else
			bremfree(bp);

		/*
		 * check for size inconsistencies for non-VMIO case.
		 */
		if (bp->b_bcount != size) {
			if ((bp->b_flags & B_VMIO) == 0 ||
			    (size > bp->b_kvasize)) {
				if (bp->b_flags & B_DELWRI) {
					/*
					 * If buffer is pinned and caller does
					 * not want sleep  waiting for it to be
					 * unpinned, bail out
					 * */
					if (bp->b_pin_count > 0) {
						if (flags & GB_LOCK_NOWAIT) {
							bqrelse(bp);
							return (NULL);
						} else {
							bunpin_wait(bp);
						}
					}
					bp->b_flags |= B_NOCACHE;
					bwrite(bp);
				} else {
					if (LIST_EMPTY(&bp->b_dep)) {
						bp->b_flags |= B_RELBUF;
						brelse(bp);
					} else {
						bp->b_flags |= B_NOCACHE;
						bwrite(bp);
					}
				}
				goto loop;
			}
		}

		/*
		 * Handle the case of unmapped buffer which should
		 * become mapped, or the buffer for which KVA
		 * reservation is requested.
		 */
		bp_unmapped_get_kva(bp, blkno, size, flags);

		/*
		 * If the size is inconsistant in the VMIO case, we can resize
		 * the buffer.  This might lead to B_CACHE getting set or
		 * cleared.  If the size has not changed, B_CACHE remains
		 * unchanged from its previous state.
		 */
		allocbuf(bp, size);

		KASSERT(bp->b_offset != NOOFFSET, 
		    ("getblk: no buffer offset"));

		/*
		 * A buffer with B_DELWRI set and B_CACHE clear must
		 * be committed before we can return the buffer in
		 * order to prevent the caller from issuing a read
		 * ( due to B_CACHE not being set ) and overwriting
		 * it.
		 *
		 * Most callers, including NFS and FFS, need this to
		 * operate properly either because they assume they
		 * can issue a read if B_CACHE is not set, or because
		 * ( for example ) an uncached B_DELWRI might loop due 
		 * to softupdates re-dirtying the buffer.  In the latter
		 * case, B_CACHE is set after the first write completes,
		 * preventing further loops.
		 * NOTE!  b*write() sets B_CACHE.  If we cleared B_CACHE
		 * above while extending the buffer, we cannot allow the
		 * buffer to remain with B_CACHE set after the write
		 * completes or it will represent a corrupt state.  To
		 * deal with this we set B_NOCACHE to scrap the buffer
		 * after the write.
		 *
		 * We might be able to do something fancy, like setting
		 * B_CACHE in bwrite() except if B_DELWRI is already set,
		 * so the below call doesn't set B_CACHE, but that gets real
		 * confusing.  This is much easier.
		 */

		if ((bp->b_flags & (B_CACHE|B_DELWRI)) == B_DELWRI) {
			bp->b_flags |= B_NOCACHE;
			bwrite(bp);
			goto loop;
		}
		bp->b_flags &= ~B_DONE;
	} else {
		/*
		 * Buffer is not in-core, create new buffer.  The buffer
		 * returned by getnewbuf() is locked.  Note that the returned
		 * buffer is also considered valid (not marked B_INVAL).
		 */
		BO_RUNLOCK(bo);
		/*
		 * If the user does not want us to create the buffer, bail out
		 * here.
		 */
		if (flags & GB_NOCREAT)
			return NULL;
		if (numfreebuffers == 0 && TD_IS_IDLETHREAD(curthread))
			return NULL;

		bsize = vn_isdisk(vp, NULL) ? DEV_BSIZE : bo->bo_bsize;
		KASSERT(bsize != 0, ("bsize == 0, check bo->bo_bsize"));
		offset = blkno * bsize;
		vmio = vp->v_object != NULL;
		if (vmio) {
			maxsize = size + (offset & PAGE_MASK);
		} else {
			maxsize = size;
			/* Do not allow non-VMIO notmapped buffers. */
			flags &= ~(GB_UNMAPPED | GB_KVAALLOC);
		}
		maxsize = imax(maxsize, bsize);

		bp = getnewbuf(vp, slpflag, slptimeo, size, maxsize, flags);
		if (bp == NULL) {
			if (slpflag || slptimeo)
				return NULL;
			goto loop;
		}

		/*
		 * This code is used to make sure that a buffer is not
		 * created while the getnewbuf routine is blocked.
		 * This can be a problem whether the vnode is locked or not.
		 * If the buffer is created out from under us, we have to
		 * throw away the one we just created.
		 *
		 * Note: this must occur before we associate the buffer
		 * with the vp especially considering limitations in
		 * the splay tree implementation when dealing with duplicate
		 * lblkno's.
		 */
		BO_LOCK(bo);
		if (gbincore(bo, blkno)) {
			BO_UNLOCK(bo);
			bp->b_flags |= B_INVAL;
			brelse(bp);
			goto loop;
		}

		/*
		 * Insert the buffer into the hash, so that it can
		 * be found by incore.
		 */
		bp->b_blkno = bp->b_lblkno = blkno;
		bp->b_offset = offset;
		bgetvp(vp, bp);
		BO_UNLOCK(bo);

		/*
		 * set B_VMIO bit.  allocbuf() the buffer bigger.  Since the
		 * buffer size starts out as 0, B_CACHE will be set by
		 * allocbuf() for the VMIO case prior to it testing the
		 * backing store for validity.
		 */

		if (vmio) {
			bp->b_flags |= B_VMIO;
			KASSERT(vp->v_object == bp->b_bufobj->bo_object,
			    ("ARGH! different b_bufobj->bo_object %p %p %p\n",
			    bp, vp->v_object, bp->b_bufobj->bo_object));
		} else {
			bp->b_flags &= ~B_VMIO;
			KASSERT(bp->b_bufobj->bo_object == NULL,
			    ("ARGH! has b_bufobj->bo_object %p %p\n",
			    bp, bp->b_bufobj->bo_object));
			BUF_CHECK_MAPPED(bp);
		}

		allocbuf(bp, size);
		bp->b_flags &= ~B_DONE;
	}
	CTR4(KTR_BUF, "getblk(%p, %ld, %d) = %p", vp, (long)blkno, size, bp);
	BUF_ASSERT_HELD(bp);
end:
	KASSERT(bp->b_bufobj == bo,
	    ("bp %p wrong b_bufobj %p should be %p", bp, bp->b_bufobj, bo));
	return (bp);
}

/*
 * Get an empty, disassociated buffer of given size.  The buffer is initially
 * set to B_INVAL.
 */
struct buf *
geteblk(int size, int flags)
{
	struct buf *bp;
	int maxsize;

	maxsize = (size + BKVAMASK) & ~BKVAMASK;
	while ((bp = getnewbuf(NULL, 0, 0, size, maxsize, flags)) == NULL) {
		if ((flags & GB_NOWAIT_BD) &&
		    (curthread->td_pflags & TDP_BUFNEED) != 0)
			return (NULL);
	}
	allocbuf(bp, size);
	bp->b_flags |= B_INVAL;	/* b_dep cleared by getnewbuf() */
	BUF_ASSERT_HELD(bp);
	return (bp);
}

/*
 * Truncate the backing store for a non-vmio buffer.
 */
static void
vfs_nonvmio_truncate(struct buf *bp, int newbsize)
{

	if (bp->b_flags & B_MALLOC) {
		/*
		 * malloced buffers are not shrunk
		 */
		if (newbsize == 0) {
			bufmallocadjust(bp, 0);
			free(bp->b_data, M_BIOBUF);
			bp->b_data = bp->b_kvabase;
			bp->b_flags &= ~B_MALLOC;
		}
		return;
	}
	vm_hold_free_pages(bp, newbsize);
	bufspaceadjust(bp, newbsize);
}

/*
 * Extend the backing for a non-VMIO buffer.
 */
static void
vfs_nonvmio_extend(struct buf *bp, int newbsize)
{
	caddr_t origbuf;
	int origbufsize;

	/*
	 * We only use malloced memory on the first allocation.
	 * and revert to page-allocated memory when the buffer
	 * grows.
	 *
	 * There is a potential smp race here that could lead
	 * to bufmallocspace slightly passing the max.  It
	 * is probably extremely rare and not worth worrying
	 * over.
	 */
	if (bp->b_bufsize == 0 && newbsize <= PAGE_SIZE/2 &&
	    bufmallocspace < maxbufmallocspace) {
		bp->b_data = malloc(newbsize, M_BIOBUF, M_WAITOK);
		bp->b_flags |= B_MALLOC;
		bufmallocadjust(bp, newbsize);
		return;
	}

	/*
	 * If the buffer is growing on its other-than-first
	 * allocation then we revert to the page-allocation
	 * scheme.
	 */
	origbuf = NULL;
	origbufsize = 0;
	if (bp->b_flags & B_MALLOC) {
		origbuf = bp->b_data;
		origbufsize = bp->b_bufsize;
		bp->b_data = bp->b_kvabase;
		bufmallocadjust(bp, 0);
		bp->b_flags &= ~B_MALLOC;
		newbsize = round_page(newbsize);
	}
	vm_hold_load_pages(bp, (vm_offset_t) bp->b_data + bp->b_bufsize,
	    (vm_offset_t) bp->b_data + newbsize);
	if (origbuf != NULL) {
		bcopy(origbuf, bp->b_data, origbufsize);
		free(origbuf, M_BIOBUF);
	}
	bufspaceadjust(bp, newbsize);
}

/*
 * This code constitutes the buffer memory from either anonymous system
 * memory (in the case of non-VMIO operations) or from an associated
 * VM object (in the case of VMIO operations).  This code is able to
 * resize a buffer up or down.
 *
 * Note that this code is tricky, and has many complications to resolve
 * deadlock or inconsistant data situations.  Tread lightly!!! 
 * There are B_CACHE and B_DELWRI interactions that must be dealt with by 
 * the caller.  Calling this code willy nilly can result in the loss of data.
 *
 * allocbuf() only adjusts B_CACHE for VMIO buffers.  getblk() deals with
 * B_CACHE for the non-VMIO case.
 */
int
allocbuf(struct buf *bp, int size)
{
	int newbsize;

	BUF_ASSERT_HELD(bp);

	if (bp->b_bcount == size)
		return (1);

	if (bp->b_kvasize != 0 && bp->b_kvasize < size)
		panic("allocbuf: buffer too small");

	newbsize = (size + DEV_BSIZE - 1) & ~(DEV_BSIZE - 1);
	if ((bp->b_flags & B_VMIO) == 0) {
		if ((bp->b_flags & B_MALLOC) == 0)
			newbsize = round_page(newbsize);
		/*
		 * Just get anonymous memory from the kernel.  Don't
		 * mess with B_CACHE.
		 */
		if (newbsize < bp->b_bufsize)
			vfs_nonvmio_truncate(bp, newbsize);
		else if (newbsize > bp->b_bufsize)
			vfs_nonvmio_extend(bp, newbsize);
	} else {
		int desiredpages;

		desiredpages = (size == 0) ? 0 :
		    num_pages((bp->b_offset & PAGE_MASK) + newbsize);

		if (bp->b_flags & B_MALLOC)
			panic("allocbuf: VMIO buffer can't be malloced");
		/*
		 * Set B_CACHE initially if buffer is 0 length or will become
		 * 0-length.
		 */
		if (size == 0 || bp->b_bufsize == 0)
			bp->b_flags |= B_CACHE;

		if (newbsize < bp->b_bufsize)
			vfs_vmio_truncate(bp, desiredpages);
		/* XXX This looks as if it should be newbsize > b_bufsize */
		else if (size > bp->b_bcount)
			vfs_vmio_extend(bp, desiredpages, size);
		bufspaceadjust(bp, newbsize);
	}
	bp->b_bcount = size;		/* requested buffer size. */
	return (1);
}

extern int inflight_transient_maps;

void
biodone(struct bio *bp)
{
	struct mtx *mtxp;
	void (*done)(struct bio *);
	vm_offset_t start, end;

	if ((bp->bio_flags & BIO_TRANSIENT_MAPPING) != 0) {
		bp->bio_flags &= ~BIO_TRANSIENT_MAPPING;
		bp->bio_flags |= BIO_UNMAPPED;
		start = trunc_page((vm_offset_t)bp->bio_data);
		end = round_page((vm_offset_t)bp->bio_data + bp->bio_length);
		bp->bio_data = unmapped_buf;
		pmap_qremove(start, OFF_TO_IDX(end - start));
		vmem_free(transient_arena, start, end - start);
		atomic_add_int(&inflight_transient_maps, -1);
	}
	done = bp->bio_done;
	if (done == NULL) {
		mtxp = mtx_pool_find(mtxpool_sleep, bp);
		mtx_lock(mtxp);
		bp->bio_flags |= BIO_DONE;
		wakeup(bp);
		mtx_unlock(mtxp);
	} else {
		bp->bio_flags |= BIO_DONE;
		done(bp);
	}
}

/*
 * Wait for a BIO to finish.
 */
int
biowait(struct bio *bp, const char *wchan)
{
	struct mtx *mtxp;

	mtxp = mtx_pool_find(mtxpool_sleep, bp);
	mtx_lock(mtxp);
	while ((bp->bio_flags & BIO_DONE) == 0)
		msleep(bp, mtxp, PRIBIO, wchan, 0);
	mtx_unlock(mtxp);
	if (bp->bio_error != 0)
		return (bp->bio_error);
	if (!(bp->bio_flags & BIO_ERROR))
		return (0);
	return (EIO);
}

void
biofinish(struct bio *bp, struct devstat *stat, int error)
{
	
	if (error) {
		bp->bio_error = error;
		bp->bio_flags |= BIO_ERROR;
	}
	if (stat != NULL)
		devstat_end_transaction_bio(stat, bp);
	biodone(bp);
}

/*
 *	bufwait:
 *
 *	Wait for buffer I/O completion, returning error status.  The buffer
 *	is left locked and B_DONE on return.  B_EINTR is converted into an EINTR
 *	error and cleared.
 */
int
bufwait(struct buf *bp)
{
	if (bp->b_iocmd == BIO_READ)
		bwait(bp, PRIBIO, "biord");
	else
		bwait(bp, PRIBIO, "biowr");
	if (bp->b_flags & B_EINTR) {
		bp->b_flags &= ~B_EINTR;
		return (EINTR);
	}
	if (bp->b_ioflags & BIO_ERROR) {
		return (bp->b_error ? bp->b_error : EIO);
	} else {
		return (0);
	}
}

/*
 *	bufdone:
 *
 *	Finish I/O on a buffer, optionally calling a completion function.
 *	This is usually called from an interrupt so process blocking is
 *	not allowed.
 *
 *	biodone is also responsible for setting B_CACHE in a B_VMIO bp.
 *	In a non-VMIO bp, B_CACHE will be set on the next getblk() 
 *	assuming B_INVAL is clear.
 *
 *	For the VMIO case, we set B_CACHE if the op was a read and no
 *	read error occured, or if the op was a write.  B_CACHE is never
 *	set if the buffer is invalid or otherwise uncacheable.
 *
 *	biodone does not mess with B_INVAL, allowing the I/O routine or the
 *	initiator to leave B_INVAL set to brelse the buffer out of existance
 *	in the biodone routine.
 */
void
bufdone(struct buf *bp)
{
	struct bufobj *dropobj;
	void    (*biodone)(struct buf *);

	CTR3(KTR_BUF, "bufdone(%p) vp %p flags %X", bp, bp->b_vp, bp->b_flags);
	dropobj = NULL;

	KASSERT(!(bp->b_flags & B_DONE), ("biodone: bp %p already done", bp));
	BUF_ASSERT_HELD(bp);

	runningbufwakeup(bp);
	if (bp->b_iocmd == BIO_WRITE)
		dropobj = bp->b_bufobj;
	/* call optional completion function if requested */
	if (bp->b_iodone != NULL) {
		biodone = bp->b_iodone;
		bp->b_iodone = NULL;
		(*biodone) (bp);
		if (dropobj)
			bufobj_wdrop(dropobj);
		return;
	}

	bufdone_finish(bp);

	if (dropobj)
		bufobj_wdrop(dropobj);
}

void
bufdone_finish(struct buf *bp)
{
	BUF_ASSERT_HELD(bp);

	if (!LIST_EMPTY(&bp->b_dep))
		buf_complete(bp);

	if (bp->b_flags & B_VMIO) {
		/*
		 * Set B_CACHE if the op was a normal read and no error
		 * occured.  B_CACHE is set for writes in the b*write()
		 * routines.
		 */
		if (bp->b_iocmd == BIO_READ &&
		    !(bp->b_flags & (B_INVAL|B_NOCACHE)) &&
		    !(bp->b_ioflags & BIO_ERROR))
			bp->b_flags |= B_CACHE;
		vfs_vmio_iodone(bp);
	}

	/*
	 * For asynchronous completions, release the buffer now. The brelse
	 * will do a wakeup there if necessary - so no need to do a wakeup
	 * here in the async case. The sync case always needs to do a wakeup.
	 */
	if (bp->b_flags & B_ASYNC) {
		if ((bp->b_flags & (B_NOCACHE | B_INVAL | B_RELBUF)) ||
		    (bp->b_ioflags & BIO_ERROR))
			brelse(bp);
		else
			bqrelse(bp);
	} else
		bdone(bp);
}

/*
 * This routine is called in lieu of iodone in the case of
 * incomplete I/O.  This keeps the busy status for pages
 * consistant.
 */
void
vfs_unbusy_pages(struct buf *bp)
{
	int i;
	vm_object_t obj;
	vm_page_t m;

	runningbufwakeup(bp);
	if (!(bp->b_flags & B_VMIO))
		return;

	obj = bp->b_bufobj->bo_object;
	VM_OBJECT_WLOCK(obj);
	for (i = 0; i < bp->b_npages; i++) {
		m = bp->b_pages[i];
		if (m == bogus_page) {
			m = vm_page_lookup(obj, OFF_TO_IDX(bp->b_offset) + i);
			if (!m)
				panic("vfs_unbusy_pages: page missing\n");
			bp->b_pages[i] = m;
			if (buf_mapped(bp)) {
				BUF_CHECK_MAPPED(bp);
				pmap_qenter(trunc_page((vm_offset_t)bp->b_data),
				    bp->b_pages, bp->b_npages);
			} else
				BUF_CHECK_UNMAPPED(bp);
		}
		vm_object_pip_subtract(obj, 1);
		vm_page_sunbusy(m);
	}
	vm_object_pip_wakeupn(obj, 0);
	VM_OBJECT_WUNLOCK(obj);
}

/*
 * vfs_page_set_valid:
 *
 *	Set the valid bits in a page based on the supplied offset.   The
 *	range is restricted to the buffer's size.
 *
 *	This routine is typically called after a read completes.
 */
static void
vfs_page_set_valid(struct buf *bp, vm_ooffset_t off, vm_page_t m)
{
	vm_ooffset_t eoff;

	/*
	 * Compute the end offset, eoff, such that [off, eoff) does not span a
	 * page boundary and eoff is not greater than the end of the buffer.
	 * The end of the buffer, in this case, is our file EOF, not the
	 * allocation size of the buffer.
	 */
	eoff = (off + PAGE_SIZE) & ~(vm_ooffset_t)PAGE_MASK;
	if (eoff > bp->b_offset + bp->b_bcount)
		eoff = bp->b_offset + bp->b_bcount;

	/*
	 * Set valid range.  This is typically the entire buffer and thus the
	 * entire page.
	 */
	if (eoff > off)
		vm_page_set_valid_range(m, off & PAGE_MASK, eoff - off);
}

/*
 * vfs_page_set_validclean:
 *
 *	Set the valid bits and clear the dirty bits in a page based on the
 *	supplied offset.   The range is restricted to the buffer's size.
 */
static void
vfs_page_set_validclean(struct buf *bp, vm_ooffset_t off, vm_page_t m)
{
	vm_ooffset_t soff, eoff;

	/*
	 * Start and end offsets in buffer.  eoff - soff may not cross a
	 * page boundry or cross the end of the buffer.  The end of the
	 * buffer, in this case, is our file EOF, not the allocation size
	 * of the buffer.
	 */
	soff = off;
	eoff = (off + PAGE_SIZE) & ~(off_t)PAGE_MASK;
	if (eoff > bp->b_offset + bp->b_bcount)
		eoff = bp->b_offset + bp->b_bcount;

	/*
	 * Set valid range.  This is typically the entire buffer and thus the
	 * entire page.
	 */
	if (eoff > soff) {
		vm_page_set_validclean(
		    m,
		   (vm_offset_t) (soff & PAGE_MASK),
		   (vm_offset_t) (eoff - soff)
		);
	}
}

/*
 * Ensure that all buffer pages are not exclusive busied.  If any page is
 * exclusive busy, drain it.
 */
void
vfs_drain_busy_pages(struct buf *bp)
{
	vm_page_t m;
	int i, last_busied;

	VM_OBJECT_ASSERT_WLOCKED(bp->b_bufobj->bo_object);
	last_busied = 0;
	for (i = 0; i < bp->b_npages; i++) {
		m = bp->b_pages[i];
		if (vm_page_xbusied(m)) {
			for (; last_busied < i; last_busied++)
				vm_page_sbusy(bp->b_pages[last_busied]);
			while (vm_page_xbusied(m)) {
				vm_page_lock(m);
				VM_OBJECT_WUNLOCK(bp->b_bufobj->bo_object);
				vm_page_busy_sleep(m, "vbpage");
				VM_OBJECT_WLOCK(bp->b_bufobj->bo_object);
			}
		}
	}
	for (i = 0; i < last_busied; i++)
		vm_page_sunbusy(bp->b_pages[i]);
}

/*
 * This routine is called before a device strategy routine.
 * It is used to tell the VM system that paging I/O is in
 * progress, and treat the pages associated with the buffer
 * almost as being exclusive busy.  Also the object paging_in_progress
 * flag is handled to make sure that the object doesn't become
 * inconsistant.
 *
 * Since I/O has not been initiated yet, certain buffer flags
 * such as BIO_ERROR or B_INVAL may be in an inconsistant state
 * and should be ignored.
 */
void
vfs_busy_pages(struct buf *bp, int clear_modify)
{
	int i, bogus;
	vm_object_t obj;
	vm_ooffset_t foff;
	vm_page_t m;

	if (!(bp->b_flags & B_VMIO))
		return;

	obj = bp->b_bufobj->bo_object;
	foff = bp->b_offset;
	KASSERT(bp->b_offset != NOOFFSET,
	    ("vfs_busy_pages: no buffer offset"));
	VM_OBJECT_WLOCK(obj);
	vfs_drain_busy_pages(bp);
	if (bp->b_bufsize != 0)
		vfs_setdirty_locked_object(bp);
	bogus = 0;
	for (i = 0; i < bp->b_npages; i++) {
		m = bp->b_pages[i];

		if ((bp->b_flags & B_CLUSTER) == 0) {
			vm_object_pip_add(obj, 1);
			vm_page_sbusy(m);
		}
		/*
		 * When readying a buffer for a read ( i.e
		 * clear_modify == 0 ), it is important to do
		 * bogus_page replacement for valid pages in 
		 * partially instantiated buffers.  Partially 
		 * instantiated buffers can, in turn, occur when
		 * reconstituting a buffer from its VM backing store
		 * base.  We only have to do this if B_CACHE is
		 * clear ( which causes the I/O to occur in the
		 * first place ).  The replacement prevents the read
		 * I/O from overwriting potentially dirty VM-backed
		 * pages.  XXX bogus page replacement is, uh, bogus.
		 * It may not work properly with small-block devices.
		 * We need to find a better way.
		 */
		if (clear_modify) {
			pmap_remove_write(m);
			vfs_page_set_validclean(bp, foff, m);
		} else if (m->valid == VM_PAGE_BITS_ALL &&
		    (bp->b_flags & B_CACHE) == 0) {
			bp->b_pages[i] = bogus_page;
			bogus++;
		}
		foff = (foff + PAGE_SIZE) & ~(off_t)PAGE_MASK;
	}
	VM_OBJECT_WUNLOCK(obj);
	if (bogus && buf_mapped(bp)) {
		BUF_CHECK_MAPPED(bp);
		pmap_qenter(trunc_page((vm_offset_t)bp->b_data),
		    bp->b_pages, bp->b_npages);
	}
}

/*
 *	vfs_bio_set_valid:
 *
 *	Set the range within the buffer to valid.  The range is
 *	relative to the beginning of the buffer, b_offset.  Note that
 *	b_offset itself may be offset from the beginning of the first
 *	page.
 */
void   
vfs_bio_set_valid(struct buf *bp, int base, int size)
{
	int i, n;
	vm_page_t m;

	if (!(bp->b_flags & B_VMIO))
		return;

	/*
	 * Fixup base to be relative to beginning of first page.
	 * Set initial n to be the maximum number of bytes in the
	 * first page that can be validated.
	 */
	base += (bp->b_offset & PAGE_MASK);
	n = PAGE_SIZE - (base & PAGE_MASK);

	VM_OBJECT_WLOCK(bp->b_bufobj->bo_object);
	for (i = base / PAGE_SIZE; size > 0 && i < bp->b_npages; ++i) {
		m = bp->b_pages[i];
		if (n > size)
			n = size;
		vm_page_set_valid_range(m, base & PAGE_MASK, n);
		base += n;
		size -= n;
		n = PAGE_SIZE;
	}
	VM_OBJECT_WUNLOCK(bp->b_bufobj->bo_object);
}

/*
 *	vfs_bio_clrbuf:
 *
 *	If the specified buffer is a non-VMIO buffer, clear the entire
 *	buffer.  If the specified buffer is a VMIO buffer, clear and
 *	validate only the previously invalid portions of the buffer.
 *	This routine essentially fakes an I/O, so we need to clear
 *	BIO_ERROR and B_INVAL.
 *
 *	Note that while we only theoretically need to clear through b_bcount,
 *	we go ahead and clear through b_bufsize.
 */
void
vfs_bio_clrbuf(struct buf *bp) 
{
	int i, j, mask, sa, ea, slide;

	if ((bp->b_flags & (B_VMIO | B_MALLOC)) != B_VMIO) {
		clrbuf(bp);
		return;
	}
	bp->b_flags &= ~B_INVAL;
	bp->b_ioflags &= ~BIO_ERROR;
	VM_OBJECT_WLOCK(bp->b_bufobj->bo_object);
	if ((bp->b_npages == 1) && (bp->b_bufsize < PAGE_SIZE) &&
	    (bp->b_offset & PAGE_MASK) == 0) {
		if (bp->b_pages[0] == bogus_page)
			goto unlock;
		mask = (1 << (bp->b_bufsize / DEV_BSIZE)) - 1;
		VM_OBJECT_ASSERT_WLOCKED(bp->b_pages[0]->object);
		if ((bp->b_pages[0]->valid & mask) == mask)
			goto unlock;
		if ((bp->b_pages[0]->valid & mask) == 0) {
			pmap_zero_page_area(bp->b_pages[0], 0, bp->b_bufsize);
			bp->b_pages[0]->valid |= mask;
			goto unlock;
		}
	}
	sa = bp->b_offset & PAGE_MASK;
	slide = 0;
	for (i = 0; i < bp->b_npages; i++, sa = 0) {
		slide = imin(slide + PAGE_SIZE, bp->b_offset + bp->b_bufsize);
		ea = slide & PAGE_MASK;
		if (ea == 0)
			ea = PAGE_SIZE;
		if (bp->b_pages[i] == bogus_page)
			continue;
		j = sa / DEV_BSIZE;
		mask = ((1 << ((ea - sa) / DEV_BSIZE)) - 1) << j;
		VM_OBJECT_ASSERT_WLOCKED(bp->b_pages[i]->object);
		if ((bp->b_pages[i]->valid & mask) == mask)
			continue;
		if ((bp->b_pages[i]->valid & mask) == 0)
			pmap_zero_page_area(bp->b_pages[i], sa, ea - sa);
		else {
			for (; sa < ea; sa += DEV_BSIZE, j++) {
				if ((bp->b_pages[i]->valid & (1 << j)) == 0) {
					pmap_zero_page_area(bp->b_pages[i],
					    sa, DEV_BSIZE);
				}
			}
		}
		bp->b_pages[i]->valid |= mask;
	}
unlock:
	VM_OBJECT_WUNLOCK(bp->b_bufobj->bo_object);
	bp->b_resid = 0;
}

void
vfs_bio_bzero_buf(struct buf *bp, int base, int size)
{
	vm_page_t m;
	int i, n;

	if (buf_mapped(bp)) {
		BUF_CHECK_MAPPED(bp);
		bzero(bp->b_data + base, size);
	} else {
		BUF_CHECK_UNMAPPED(bp);
		n = PAGE_SIZE - (base & PAGE_MASK);
		for (i = base / PAGE_SIZE; size > 0 && i < bp->b_npages; ++i) {
			m = bp->b_pages[i];
			if (n > size)
				n = size;
			pmap_zero_page_area(m, base & PAGE_MASK, n);
			base += n;
			size -= n;
			n = PAGE_SIZE;
		}
	}
}

/*
 * vm_hold_load_pages and vm_hold_free_pages get pages into
 * a buffers address space.  The pages are anonymous and are
 * not associated with a file object.
 */
static void
vm_hold_load_pages(struct buf *bp, vm_offset_t from, vm_offset_t to)
{
	vm_offset_t pg;
	vm_page_t p;
	int index;

	BUF_CHECK_MAPPED(bp);

	to = round_page(to);
	from = round_page(from);
	index = (from - trunc_page((vm_offset_t)bp->b_data)) >> PAGE_SHIFT;

	for (pg = from; pg < to; pg += PAGE_SIZE, index++) {
tryagain:
		/*
		 * note: must allocate system pages since blocking here
		 * could interfere with paging I/O, no matter which
		 * process we are.
		 */
		p = vm_page_alloc(NULL, 0, VM_ALLOC_SYSTEM | VM_ALLOC_NOOBJ |
		    VM_ALLOC_WIRED | VM_ALLOC_COUNT((to - pg) >> PAGE_SHIFT));
		if (p == NULL) {
			VM_WAIT;
			goto tryagain;
		}
		pmap_qenter(pg, &p, 1);
		bp->b_pages[index] = p;
	}
	bp->b_npages = index;
}

/* Return pages associated with this buf to the vm system */
static void
vm_hold_free_pages(struct buf *bp, int newbsize)
{
	vm_offset_t from;
	vm_page_t p;
	int index, newnpages;

	BUF_CHECK_MAPPED(bp);

	from = round_page((vm_offset_t)bp->b_data + newbsize);
	newnpages = (from - trunc_page((vm_offset_t)bp->b_data)) >> PAGE_SHIFT;
	if (bp->b_npages > newnpages)
		pmap_qremove(from, bp->b_npages - newnpages);
	for (index = newnpages; index < bp->b_npages; index++) {
		p = bp->b_pages[index];
		bp->b_pages[index] = NULL;
		if (vm_page_sbusied(p))
			printf("vm_hold_free_pages: blkno: %jd, lblkno: %jd\n",
			    (intmax_t)bp->b_blkno, (intmax_t)bp->b_lblkno);
		p->wire_count--;
		vm_page_free(p);
		atomic_subtract_int(&vm_cnt.v_wire_count, 1);
	}
	bp->b_npages = newnpages;
}

/*
 * Map an IO request into kernel virtual address space.
 *
 * All requests are (re)mapped into kernel VA space.
 * Notice that we use b_bufsize for the size of the buffer
 * to be mapped.  b_bcount might be modified by the driver.
 *
 * Note that even if the caller determines that the address space should
 * be valid, a race or a smaller-file mapped into a larger space may
 * actually cause vmapbuf() to fail, so all callers of vmapbuf() MUST
 * check the return value.
 *
 * This function only works with pager buffers.
 */
int
vmapbuf(struct buf *bp, int mapbuf)
{
	vm_prot_t prot;
	int pidx;

	if (bp->b_bufsize < 0)
		return (-1);
	prot = VM_PROT_READ;
	if (bp->b_iocmd == BIO_READ)
		prot |= VM_PROT_WRITE;	/* Less backwards than it looks */
	if ((pidx = vm_fault_quick_hold_pages(&curproc->p_vmspace->vm_map,
	    (vm_offset_t)bp->b_data, bp->b_bufsize, prot, bp->b_pages,
	    btoc(MAXPHYS))) < 0)
		return (-1);
	bp->b_npages = pidx;
	bp->b_offset = ((vm_offset_t)bp->b_data) & PAGE_MASK;
	if (mapbuf || !unmapped_buf_allowed) {
		pmap_qenter((vm_offset_t)bp->b_kvabase, bp->b_pages, pidx);
		bp->b_data = bp->b_kvabase + bp->b_offset;
	} else
		bp->b_data = unmapped_buf;
	return(0);
}

/*
 * Free the io map PTEs associated with this IO operation.
 * We also invalidate the TLB entries and restore the original b_addr.
 *
 * This function only works with pager buffers.
 */
void
vunmapbuf(struct buf *bp)
{
	int npages;

	npages = bp->b_npages;
	if (buf_mapped(bp))
		pmap_qremove(trunc_page((vm_offset_t)bp->b_data), npages);
	vm_page_unhold_pages(bp->b_pages, npages);

	bp->b_data = unmapped_buf;
}

void
bdone(struct buf *bp)
{
	struct mtx *mtxp;

	mtxp = mtx_pool_find(mtxpool_sleep, bp);
	mtx_lock(mtxp);
	bp->b_flags |= B_DONE;
	wakeup(bp);
	mtx_unlock(mtxp);
}

void
bwait(struct buf *bp, u_char pri, const char *wchan)
{
	struct mtx *mtxp;

	mtxp = mtx_pool_find(mtxpool_sleep, bp);
	mtx_lock(mtxp);
	while ((bp->b_flags & B_DONE) == 0)
		msleep(bp, mtxp, pri, wchan, 0);
	mtx_unlock(mtxp);
}

int
bufsync(struct bufobj *bo, int waitfor)
{

	return (VOP_FSYNC(bo->__bo_vnode, waitfor, curthread));
}

void
bufstrategy(struct bufobj *bo, struct buf *bp)
{
	int i = 0;
	struct vnode *vp;

	vp = bp->b_vp;
	KASSERT(vp == bo->bo_private, ("Inconsistent vnode bufstrategy"));
	KASSERT(vp->v_type != VCHR && vp->v_type != VBLK,
	    ("Wrong vnode in bufstrategy(bp=%p, vp=%p)", bp, vp));
	i = VOP_STRATEGY(vp, bp);
	KASSERT(i == 0, ("VOP_STRATEGY failed bp=%p vp=%p", bp, bp->b_vp));
}

void
bufobj_wrefl(struct bufobj *bo)
{

	KASSERT(bo != NULL, ("NULL bo in bufobj_wref"));
	ASSERT_BO_WLOCKED(bo);
	bo->bo_numoutput++;
}

void
bufobj_wref(struct bufobj *bo)
{

	KASSERT(bo != NULL, ("NULL bo in bufobj_wref"));
	BO_LOCK(bo);
	bo->bo_numoutput++;
	BO_UNLOCK(bo);
}

void
bufobj_wdrop(struct bufobj *bo)
{

	KASSERT(bo != NULL, ("NULL bo in bufobj_wdrop"));
	BO_LOCK(bo);
	KASSERT(bo->bo_numoutput > 0, ("bufobj_wdrop non-positive count"));
	if ((--bo->bo_numoutput == 0) && (bo->bo_flag & BO_WWAIT)) {
		bo->bo_flag &= ~BO_WWAIT;
		wakeup(&bo->bo_numoutput);
	}
	BO_UNLOCK(bo);
}

int
bufobj_wwait(struct bufobj *bo, int slpflag, int timeo)
{
	int error;

	KASSERT(bo != NULL, ("NULL bo in bufobj_wwait"));
	ASSERT_BO_WLOCKED(bo);
	error = 0;
	while (bo->bo_numoutput) {
		bo->bo_flag |= BO_WWAIT;
		error = msleep(&bo->bo_numoutput, BO_LOCKPTR(bo),
		    slpflag | (PRIBIO + 1), "bo_wwait", timeo);
		if (error)
			break;
	}
	return (error);
}

void
bpin(struct buf *bp)
{
	struct mtx *mtxp;

	mtxp = mtx_pool_find(mtxpool_sleep, bp);
	mtx_lock(mtxp);
	bp->b_pin_count++;
	mtx_unlock(mtxp);
}

void
bunpin(struct buf *bp)
{
	struct mtx *mtxp;

	mtxp = mtx_pool_find(mtxpool_sleep, bp);
	mtx_lock(mtxp);
	if (--bp->b_pin_count == 0)
		wakeup(bp);
	mtx_unlock(mtxp);
}

void
bunpin_wait(struct buf *bp)
{
	struct mtx *mtxp;

	mtxp = mtx_pool_find(mtxpool_sleep, bp);
	mtx_lock(mtxp);
	while (bp->b_pin_count > 0)
		msleep(bp, mtxp, PRIBIO, "bwunpin", 0);
	mtx_unlock(mtxp);
}

/*
 * Set bio_data or bio_ma for struct bio from the struct buf.
 */
void
bdata2bio(struct buf *bp, struct bio *bip)
{

	if (!buf_mapped(bp)) {
		KASSERT(unmapped_buf_allowed, ("unmapped"));
		bip->bio_ma = bp->b_pages;
		bip->bio_ma_n = bp->b_npages;
		bip->bio_data = unmapped_buf;
		bip->bio_ma_offset = (vm_offset_t)bp->b_offset & PAGE_MASK;
		bip->bio_flags |= BIO_UNMAPPED;
		KASSERT(round_page(bip->bio_ma_offset + bip->bio_length) /
		    PAGE_SIZE == bp->b_npages,
		    ("Buffer %p too short: %d %lld %d", bp, bip->bio_ma_offset,
		    (long long)bip->bio_length, bip->bio_ma_n));
	} else {
		bip->bio_data = bp->b_data;
		bip->bio_ma = NULL;
	}
}

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>

/* DDB command to show buffer data */
DB_SHOW_COMMAND(buffer, db_show_buffer)
{
	/* get args */
	struct buf *bp = (struct buf *)addr;

	if (!have_addr) {
		db_printf("usage: show buffer <addr>\n");
		return;
	}

	db_printf("buf at %p\n", bp);
	db_printf("b_flags = 0x%b, b_xflags=0x%b, b_vflags=0x%b\n",
	    (u_int)bp->b_flags, PRINT_BUF_FLAGS, (u_int)bp->b_xflags,
	    PRINT_BUF_XFLAGS, (u_int)bp->b_vflags, PRINT_BUF_VFLAGS);
	db_printf(
	    "b_error = %d, b_bufsize = %ld, b_bcount = %ld, b_resid = %ld\n"
	    "b_bufobj = (%p), b_data = %p, b_blkno = %jd, b_lblkno = %jd, "
	    "b_dep = %p\n",
	    bp->b_error, bp->b_bufsize, bp->b_bcount, bp->b_resid,
	    bp->b_bufobj, bp->b_data, (intmax_t)bp->b_blkno,
	    (intmax_t)bp->b_lblkno, bp->b_dep.lh_first);
	db_printf("b_kvabase = %p, b_kvasize = %d\n",
	    bp->b_kvabase, bp->b_kvasize);
	if (bp->b_npages) {
		int i;
		db_printf("b_npages = %d, pages(OBJ, IDX, PA): ", bp->b_npages);
		for (i = 0; i < bp->b_npages; i++) {
			vm_page_t m;
			m = bp->b_pages[i];
			db_printf("(%p, 0x%lx, 0x%lx)", (void *)m->object,
			    (u_long)m->pindex, (u_long)VM_PAGE_TO_PHYS(m));
			if ((i + 1) < bp->b_npages)
				db_printf(",");
		}
		db_printf("\n");
	}
	db_printf(" ");
	BUF_LOCKPRINTINFO(bp);
}

DB_SHOW_COMMAND(lockedbufs, lockedbufs)
{
	struct buf *bp;
	int i;

	for (i = 0; i < nbuf; i++) {
		bp = &buf[i];
		if (BUF_ISLOCKED(bp)) {
			db_show_buffer((uintptr_t)bp, 1, 0, NULL);
			db_printf("\n");
		}
	}
}

DB_SHOW_COMMAND(vnodebufs, db_show_vnodebufs)
{
	struct vnode *vp;
	struct buf *bp;

	if (!have_addr) {
		db_printf("usage: show vnodebufs <addr>\n");
		return;
	}
	vp = (struct vnode *)addr;
	db_printf("Clean buffers:\n");
	TAILQ_FOREACH(bp, &vp->v_bufobj.bo_clean.bv_hd, b_bobufs) {
		db_show_buffer((uintptr_t)bp, 1, 0, NULL);
		db_printf("\n");
	}
	db_printf("Dirty buffers:\n");
	TAILQ_FOREACH(bp, &vp->v_bufobj.bo_dirty.bv_hd, b_bobufs) {
		db_show_buffer((uintptr_t)bp, 1, 0, NULL);
		db_printf("\n");
	}
}

DB_COMMAND(countfreebufs, db_coundfreebufs)
{
	struct buf *bp;
	int i, used = 0, nfree = 0;

	if (have_addr) {
		db_printf("usage: countfreebufs\n");
		return;
	}

	for (i = 0; i < nbuf; i++) {
		bp = &buf[i];
		if ((bp->b_flags & B_INFREECNT) != 0)
			nfree++;
		else
			used++;
	}

	db_printf("Counted %d free, %d used (%d tot)\n", nfree, used,
	    nfree + used);
	db_printf("numfreebuffers is %d\n", numfreebuffers);
}
#endif /* DDB */
