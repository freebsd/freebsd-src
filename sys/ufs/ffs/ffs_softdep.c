/*
 * Copyright 1998, 2000 Marshall Kirk McKusick. All Rights Reserved.
 *
 * The soft updates code is derived from the appendix of a University
 * of Michigan technical report (Gregory R. Ganger and Yale N. Patt,
 * "Soft Updates: A Solution to the Metadata Update Problem in File
 * Systems", CSE-TR-254-95, August 1995).
 *
 * Further information about soft updates can be obtained from:
 *
 *	Marshall Kirk McKusick		http://www.mckusick.com/softdep/
 *	1614 Oxford Street		mckusick@mckusick.com
 *	Berkeley, CA 94709-1608		+1-510-843-9542
 *	USA
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY MARSHALL KIRK MCKUSICK ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL MARSHALL KIRK MCKUSICK BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)ffs_softdep.c	9.59 (McKusick) 6/21/00
 * $FreeBSD$
 */

/*
 * For now we want the safety net that the DIAGNOSTIC and DEBUG flags provide.
 */
#ifndef DIAGNOSTIC
#define DIAGNOSTIC
#endif
#ifndef DEBUG
#define DEBUG
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ffs/fs.h>
#include <ufs/ffs/softdep.h>
#include <ufs/ffs/ffs_extern.h>
#include <ufs/ufs/ufs_extern.h>

/*
 * These definitions need to be adapted to the system to which
 * this file is being ported.
 */
/*
 * malloc types defined for the softdep system.
 */
MALLOC_DEFINE(M_PAGEDEP, "pagedep","File page dependencies");
MALLOC_DEFINE(M_INODEDEP, "inodedep","Inode dependencies");
MALLOC_DEFINE(M_NEWBLK, "newblk","New block allocation");
MALLOC_DEFINE(M_BMSAFEMAP, "bmsafemap","Block or frag allocated from cyl group map");
MALLOC_DEFINE(M_ALLOCDIRECT, "allocdirect","Block or frag dependency for an inode");
MALLOC_DEFINE(M_INDIRDEP, "indirdep","Indirect block dependencies");
MALLOC_DEFINE(M_ALLOCINDIR, "allocindir","Block dependency for an indirect block");
MALLOC_DEFINE(M_FREEFRAG, "freefrag","Previously used frag for an inode");
MALLOC_DEFINE(M_FREEBLKS, "freeblks","Blocks freed from an inode");
MALLOC_DEFINE(M_FREEFILE, "freefile","Inode deallocated");
MALLOC_DEFINE(M_DIRADD, "diradd","New directory entry");
MALLOC_DEFINE(M_MKDIR, "mkdir","New directory");
MALLOC_DEFINE(M_DIRREM, "dirrem","Directory entry deleted");

#define	D_PAGEDEP	0
#define	D_INODEDEP	1
#define	D_NEWBLK	2
#define	D_BMSAFEMAP	3
#define	D_ALLOCDIRECT	4
#define	D_INDIRDEP	5
#define	D_ALLOCINDIR	6
#define	D_FREEFRAG	7
#define	D_FREEBLKS	8
#define	D_FREEFILE	9
#define	D_DIRADD	10
#define	D_MKDIR		11
#define	D_DIRREM	12
#define D_LAST		D_DIRREM

/* 
 * translate from workitem type to memory type
 * MUST match the defines above, such that memtype[D_XXX] == M_XXX
 */
static struct malloc_type *memtype[] = {
	M_PAGEDEP,
	M_INODEDEP,
	M_NEWBLK,
	M_BMSAFEMAP,
	M_ALLOCDIRECT,
	M_INDIRDEP,
	M_ALLOCINDIR,
	M_FREEFRAG,
	M_FREEBLKS,
	M_FREEFILE,
	M_DIRADD,
	M_MKDIR,
	M_DIRREM
};

#define DtoM(type) (memtype[type])

/*
 * Names of malloc types.
 */
#define TYPENAME(type)  \
	((unsigned)(type) < D_LAST ? memtype[type]->ks_shortdesc : "???")
/*
 * End system adaptaion definitions.
 */

/*
 * Internal function prototypes.
 */
static	void softdep_error __P((char *, int));
static	void drain_output __P((struct vnode *, int));
static	int getdirtybuf __P((struct buf **, int));
static	void clear_remove __P((struct proc *));
static	void clear_inodedeps __P((struct proc *));
static	int flush_pagedep_deps __P((struct vnode *, struct mount *,
	    struct diraddhd *));
static	int flush_inodedep_deps __P((struct fs *, ino_t));
static	int handle_written_filepage __P((struct pagedep *, struct buf *));
static  void diradd_inode_written __P((struct diradd *, struct inodedep *));
static	int handle_written_inodeblock __P((struct inodedep *, struct buf *));
static	void handle_allocdirect_partdone __P((struct allocdirect *));
static	void handle_allocindir_partdone __P((struct allocindir *));
static	void initiate_write_filepage __P((struct pagedep *, struct buf *));
static	void handle_written_mkdir __P((struct mkdir *, int));
static	void initiate_write_inodeblock __P((struct inodedep *, struct buf *));
static	void handle_workitem_freefile __P((struct freefile *));
static	void handle_workitem_remove __P((struct dirrem *));
static	struct dirrem *newdirrem __P((struct buf *, struct inode *,
	    struct inode *, int, struct dirrem **));
static	void free_diradd __P((struct diradd *));
static	void free_allocindir __P((struct allocindir *, struct inodedep *));
static	int indir_trunc __P((struct inode *, ufs_daddr_t, int, ufs_lbn_t,
	    long *));
static	void deallocate_dependencies __P((struct buf *, struct inodedep *));
static	void free_allocdirect __P((struct allocdirectlst *,
	    struct allocdirect *, int));
static	int check_inode_unwritten __P((struct inodedep *));
static	int free_inodedep __P((struct inodedep *));
static	void handle_workitem_freeblocks __P((struct freeblks *));
static	void merge_inode_lists __P((struct inodedep *));
static	void setup_allocindir_phase2 __P((struct buf *, struct inode *,
	    struct allocindir *));
static	struct allocindir *newallocindir __P((struct inode *, int, ufs_daddr_t,
	    ufs_daddr_t));
static	void handle_workitem_freefrag __P((struct freefrag *));
static	struct freefrag *newfreefrag __P((struct inode *, ufs_daddr_t, long));
static	void allocdirect_merge __P((struct allocdirectlst *,
	    struct allocdirect *, struct allocdirect *));
static	struct bmsafemap *bmsafemap_lookup __P((struct buf *));
static	int newblk_lookup __P((struct fs *, ufs_daddr_t, int,
	    struct newblk **));
static	int inodedep_lookup __P((struct fs *, ino_t, int, struct inodedep **));
static	int pagedep_lookup __P((struct inode *, ufs_lbn_t, int,
	    struct pagedep **));
static	void pause_timer __P((void *));
static	int request_cleanup __P((int, int));
static	void add_to_worklist __P((struct worklist *));

/*
 * Exported softdep operations.
 */
static	void softdep_disk_io_initiation __P((struct buf *));
static	void softdep_disk_write_complete __P((struct buf *));
static	void softdep_deallocate_dependencies __P((struct buf *));
static	void softdep_move_dependencies __P((struct buf *, struct buf *));
static	int softdep_count_dependencies __P((struct buf *bp, int));

struct bio_ops bioops = {
	softdep_disk_io_initiation,		/* io_start */
	softdep_disk_write_complete,		/* io_complete */
	softdep_deallocate_dependencies,	/* io_deallocate */
	softdep_move_dependencies,		/* io_movedeps */
	softdep_count_dependencies,		/* io_countdeps */
};

/*
 * Locking primitives.
 *
 * For a uniprocessor, all we need to do is protect against disk
 * interrupts. For a multiprocessor, this lock would have to be
 * a mutex. A single mutex is used throughout this file, though
 * finer grain locking could be used if contention warranted it.
 *
 * For a multiprocessor, the sleep call would accept a lock and
 * release it after the sleep processing was complete. In a uniprocessor
 * implementation there is no such interlock, so we simple mark
 * the places where it needs to be done with the `interlocked' form
 * of the lock calls. Since the uniprocessor sleep already interlocks
 * the spl, there is nothing that really needs to be done.
 */
#ifndef /* NOT */ DEBUG
static struct lockit {
	int	lkt_spl;
} lk = { 0 };
#define ACQUIRE_LOCK(lk)		(lk)->lkt_spl = splbio()
#define FREE_LOCK(lk)			splx((lk)->lkt_spl)
#define ACQUIRE_LOCK_INTERLOCKED(lk)
#define FREE_LOCK_INTERLOCKED(lk)

#else /* DEBUG */
static struct lockit {
	int	lkt_spl;
	pid_t	lkt_held;
} lk = { 0, -1 };
static int lockcnt;

static	void acquire_lock __P((struct lockit *));
static	void free_lock __P((struct lockit *));
static	void acquire_lock_interlocked __P((struct lockit *));
static	void free_lock_interlocked __P((struct lockit *));

#define ACQUIRE_LOCK(lk)		acquire_lock(lk)
#define FREE_LOCK(lk)			free_lock(lk)
#define ACQUIRE_LOCK_INTERLOCKED(lk)	acquire_lock_interlocked(lk)
#define FREE_LOCK_INTERLOCKED(lk)	free_lock_interlocked(lk)

static void
acquire_lock(lk)
	struct lockit *lk;
{

	if (lk->lkt_held != -1) {
		if (lk->lkt_held == CURPROC->p_pid)
			panic("softdep_lock: locking against myself");
		else
			panic("softdep_lock: lock held by %d", lk->lkt_held);
	}
	lk->lkt_spl = splbio();
	lk->lkt_held = CURPROC->p_pid;
	lockcnt++;
}

static void
free_lock(lk)
	struct lockit *lk;
{

	if (lk->lkt_held == -1)
		panic("softdep_unlock: lock not held");
	lk->lkt_held = -1;
	splx(lk->lkt_spl);
}

static void
acquire_lock_interlocked(lk)
	struct lockit *lk;
{

	if (lk->lkt_held != -1) {
		if (lk->lkt_held == CURPROC->p_pid)
			panic("softdep_lock_interlocked: locking against self");
		else
			panic("softdep_lock_interlocked: lock held by %d",
			    lk->lkt_held);
	}
	lk->lkt_held = CURPROC->p_pid;
	lockcnt++;
}

static void
free_lock_interlocked(lk)
	struct lockit *lk;
{

	if (lk->lkt_held == -1)
		panic("softdep_unlock_interlocked: lock not held");
	lk->lkt_held = -1;
}
#endif /* DEBUG */

/*
 * Place holder for real semaphores.
 */
struct sema {
	int	value;
	pid_t	holder;
	char	*name;
	int	prio;
	int	timo;
};
static	void sema_init __P((struct sema *, char *, int, int));
static	int sema_get __P((struct sema *, struct lockit *));
static	void sema_release __P((struct sema *));

static void
sema_init(semap, name, prio, timo)
	struct sema *semap;
	char *name;
	int prio, timo;
{

	semap->holder = -1;
	semap->value = 0;
	semap->name = name;
	semap->prio = prio;
	semap->timo = timo;
}

static int
sema_get(semap, interlock)
	struct sema *semap;
	struct lockit *interlock;
{

	if (semap->value++ > 0) {
		if (interlock != NULL)
			FREE_LOCK_INTERLOCKED(interlock);
		tsleep((caddr_t)semap, semap->prio, semap->name, semap->timo);
		if (interlock != NULL) {
			ACQUIRE_LOCK_INTERLOCKED(interlock);
			FREE_LOCK(interlock);
		}
		return (0);
	}
	semap->holder = CURPROC->p_pid;
	if (interlock != NULL)
		FREE_LOCK(interlock);
	return (1);
}

static void
sema_release(semap)
	struct sema *semap;
{

	if (semap->value <= 0 || semap->holder != CURPROC->p_pid)
		panic("sema_release: not held");
	if (--semap->value > 0) {
		semap->value = 0;
		wakeup(semap);
	}
	semap->holder = -1;
}

/*
 * Worklist queue management.
 * These routines require that the lock be held.
 */
#ifndef /* NOT */ DEBUG
#define WORKLIST_INSERT(head, item) do {	\
	(item)->wk_state |= ONWORKLIST;		\
	LIST_INSERT_HEAD(head, item, wk_list);	\
} while (0)
#define WORKLIST_REMOVE(item) do {		\
	(item)->wk_state &= ~ONWORKLIST;	\
	LIST_REMOVE(item, wk_list);		\
} while (0)
#define WORKITEM_FREE(item, type) FREE(item, DtoM(type))

#else /* DEBUG */
static	void worklist_insert __P((struct workhead *, struct worklist *));
static	void worklist_remove __P((struct worklist *));
static	void workitem_free __P((struct worklist *, int));

#define WORKLIST_INSERT(head, item) worklist_insert(head, item)
#define WORKLIST_REMOVE(item) worklist_remove(item)
#define WORKITEM_FREE(item, type) workitem_free((struct worklist *)item, type)

static void
worklist_insert(head, item)
	struct workhead *head;
	struct worklist *item;
{

	if (lk.lkt_held == -1)
		panic("worklist_insert: lock not held");
	if (item->wk_state & ONWORKLIST)
		panic("worklist_insert: already on list");
	item->wk_state |= ONWORKLIST;
	LIST_INSERT_HEAD(head, item, wk_list);
}

static void
worklist_remove(item)
	struct worklist *item;
{

	if (lk.lkt_held == -1)
		panic("worklist_remove: lock not held");
	if ((item->wk_state & ONWORKLIST) == 0)
		panic("worklist_remove: not on list");
	item->wk_state &= ~ONWORKLIST;
	LIST_REMOVE(item, wk_list);
}

static void
workitem_free(item, type)
	struct worklist *item;
	int type;
{

	if (item->wk_state & ONWORKLIST)
		panic("workitem_free: still on list");
	if (item->wk_type != type)
		panic("workitem_free: type mismatch");
	FREE(item, DtoM(type));
}
#endif /* DEBUG */

/*
 * Workitem queue management
 */
static struct workhead softdep_workitem_pending;
static int softdep_worklist_busy;
static int max_softdeps;	/* maximum number of structs before slowdown */
static int tickdelay = 2;	/* number of ticks to pause during slowdown */
static int proc_waiting;	/* tracks whether we have a timeout posted */
static struct proc *filesys_syncer; /* proc of filesystem syncer process */
static int req_clear_inodedeps;	/* syncer process flush some inodedeps */
#define FLUSH_INODES	1
static int req_clear_remove;	/* syncer process flush some freeblks */
#define FLUSH_REMOVE	2
/*
 * runtime statistics
 */
static int stat_blk_limit_push;	/* number of times block limit neared */
static int stat_ino_limit_push;	/* number of times inode limit neared */
static int stat_blk_limit_hit;	/* number of times block slowdown imposed */
static int stat_ino_limit_hit;	/* number of times inode slowdown imposed */
static int stat_indir_blk_ptrs;	/* bufs redirtied as indir ptrs not written */
static int stat_inode_bitmap;	/* bufs redirtied as inode bitmap not written */
static int stat_direct_blk_ptrs;/* bufs redirtied as direct ptrs not written */
static int stat_dir_entry;	/* bufs redirtied as dir entry cannot write */
#ifdef DEBUG
#include <vm/vm.h>
#include <sys/sysctl.h>
SYSCTL_INT(_debug, OID_AUTO, max_softdeps, CTLFLAG_RW, &max_softdeps, 0, "");
SYSCTL_INT(_debug, OID_AUTO, tickdelay, CTLFLAG_RW, &tickdelay, 0, "");
SYSCTL_INT(_debug, OID_AUTO, blk_limit_push, CTLFLAG_RW, &stat_blk_limit_push, 0,"");
SYSCTL_INT(_debug, OID_AUTO, ino_limit_push, CTLFLAG_RW, &stat_ino_limit_push, 0,"");
SYSCTL_INT(_debug, OID_AUTO, blk_limit_hit, CTLFLAG_RW, &stat_blk_limit_hit, 0, "");
SYSCTL_INT(_debug, OID_AUTO, ino_limit_hit, CTLFLAG_RW, &stat_ino_limit_hit, 0, "");
SYSCTL_INT(_debug, OID_AUTO, indir_blk_ptrs, CTLFLAG_RW, &stat_indir_blk_ptrs, 0, "");
SYSCTL_INT(_debug, OID_AUTO, inode_bitmap, CTLFLAG_RW, &stat_inode_bitmap, 0, "");
SYSCTL_INT(_debug, OID_AUTO, direct_blk_ptrs, CTLFLAG_RW, &stat_direct_blk_ptrs, 0, "");
SYSCTL_INT(_debug, OID_AUTO, dir_entry, CTLFLAG_RW, &stat_dir_entry, 0, "");
#endif /* DEBUG */

/*
 * Add an item to the end of the work queue.
 * This routine requires that the lock be held.
 * This is the only routine that adds items to the list.
 * The following routine is the only one that removes items
 * and does so in order from first to last.
 */
static void
add_to_worklist(wk)
	struct worklist *wk;
{
	static struct worklist *worklist_tail;

	if (wk->wk_state & ONWORKLIST)
		panic("add_to_worklist: already on list");
	wk->wk_state |= ONWORKLIST;
	if (LIST_FIRST(&softdep_workitem_pending) == NULL)
		LIST_INSERT_HEAD(&softdep_workitem_pending, wk, wk_list);
	else
		LIST_INSERT_AFTER(worklist_tail, wk, wk_list);
	worklist_tail = wk;
}

/*
 * Process that runs once per second to handle items in the background queue.
 *
 * Note that we ensure that everything is done in the order in which they
 * appear in the queue. The code below depends on this property to ensure
 * that blocks of a file are freed before the inode itself is freed. This
 * ordering ensures that no new <vfsid, inum, lbn> triples will be generated
 * until all the old ones have been purged from the dependency lists.
 */
int 
softdep_process_worklist(matchmnt)
	struct mount *matchmnt;
{
	struct proc *p = CURPROC;
	struct worklist *wk;
	struct mount *mp;
	int matchcnt, loopcount;

	/*
	 * Record the process identifier of our caller so that we can give
	 * this process preferential treatment in request_cleanup below.
	 */
	filesys_syncer = p;
	matchcnt = 0;
	/*
	 * There is no danger of having multiple processes run this
	 * code. It is single threaded solely so that softdep_flushfiles
	 * (below) can get an accurate count of the number of items
	 * related to its mount point that are in the list.
	 */
	if (softdep_worklist_busy && matchmnt == NULL)
		return (-1);
	/*
	 * If requested, try removing inode or removal dependencies.
	 */
	if (req_clear_inodedeps) {
		clear_inodedeps(p);
		req_clear_inodedeps = 0;
		wakeup(&proc_waiting);
	}
	if (req_clear_remove) {
		clear_remove(p);
		req_clear_remove = 0;
		wakeup(&proc_waiting);
	}
	ACQUIRE_LOCK(&lk);
	loopcount = 1;
	while ((wk = LIST_FIRST(&softdep_workitem_pending)) != 0) {
		WORKLIST_REMOVE(wk);
		FREE_LOCK(&lk);
		switch (wk->wk_type) {

		case D_DIRREM:
			/* removal of a directory entry */
			mp = WK_DIRREM(wk)->dm_mnt;
			if (vn_write_suspend_wait(NULL, mp, V_NOWAIT))
				panic("%s: dirrem on suspended filesystem",
					"softdep_process_worklist");
			if (mp == matchmnt)
				matchcnt += 1;
			handle_workitem_remove(WK_DIRREM(wk));
			break;

		case D_FREEBLKS:
			/* releasing blocks and/or fragments from a file */
			mp = WK_FREEBLKS(wk)->fb_mnt;
			if (vn_write_suspend_wait(NULL, mp, V_NOWAIT))
				panic("%s: freeblks on suspended filesystem",
					"softdep_process_worklist");
			if (mp == matchmnt)
				matchcnt += 1;
			handle_workitem_freeblocks(WK_FREEBLKS(wk));
			break;

		case D_FREEFRAG:
			/* releasing a fragment when replaced as a file grows */
			mp = WK_FREEFRAG(wk)->ff_mnt;
			if (vn_write_suspend_wait(NULL, mp, V_NOWAIT))
				panic("%s: freefrag on suspended filesystem",
					"softdep_process_worklist");
			if (mp == matchmnt)
				matchcnt += 1;
			handle_workitem_freefrag(WK_FREEFRAG(wk));
			break;

		case D_FREEFILE:
			/* releasing an inode when its link count drops to 0 */
			mp = WK_FREEFILE(wk)->fx_mnt;
			if (vn_write_suspend_wait(NULL, mp, V_NOWAIT))
				panic("%s: freefile on suspended filesystem",
					"softdep_process_worklist");
			if (mp == matchmnt)
				matchcnt += 1;
			handle_workitem_freefile(WK_FREEFILE(wk));
			break;

		default:
			panic("%s_process_worklist: Unknown type %s",
			    "softdep", TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
		if (softdep_worklist_busy && matchmnt == NULL)
			return (-1);
		/*
		 * If requested, try removing inode or removal dependencies.
		 */
		if (req_clear_inodedeps) {
			clear_inodedeps(p);
			req_clear_inodedeps = 0;
			wakeup(&proc_waiting);
		}
		if (req_clear_remove) {
			clear_remove(p);
			req_clear_remove = 0;
			wakeup(&proc_waiting);
		}
		/*
		 * We do not generally want to stop for buffer space, but if
		 * we are really being a buffer hog, we will stop and wait.
		 */
		if (loopcount++ % 128 == 0)
			bwillwrite();
		ACQUIRE_LOCK(&lk);
	}
	FREE_LOCK(&lk);
	return (matchcnt);
}

/*
 * Move dependencies from one buffer to another.
 */
static void
softdep_move_dependencies(oldbp, newbp)
	struct buf *oldbp;
	struct buf *newbp;
{
	struct worklist *wk, *wktail;

	if (LIST_FIRST(&newbp->b_dep) != NULL)
		panic("softdep_move_dependencies: need merge code");
	wktail = 0;
	ACQUIRE_LOCK(&lk);
	while ((wk = LIST_FIRST(&oldbp->b_dep)) != NULL) {
		LIST_REMOVE(wk, wk_list);
		if (wktail == 0)
			LIST_INSERT_HEAD(&newbp->b_dep, wk, wk_list);
		else
			LIST_INSERT_AFTER(wktail, wk, wk_list);
		wktail = wk;
	}
	FREE_LOCK(&lk);
}

/*
 * Purge the work list of all items associated with a particular mount point.
 */
int
softdep_flushworklist(oldmnt, countp, p)
	struct mount *oldmnt;
	int *countp;
	struct proc *p;
{
	struct vnode *devvp;
	int count, error = 0;

	/*
	 * Await our turn to clear out the queue.
	 */
	while (softdep_worklist_busy)
		tsleep(&lbolt, PRIBIO, "softflush", 0);
	softdep_worklist_busy = 1;
	/*
	 * Alternately flush the block device associated with the mount
	 * point and process any dependencies that the flushing
	 * creates. We continue until no more worklist dependencies
	 * are found.
	 */
	*countp = 0;
	devvp = VFSTOUFS(oldmnt)->um_devvp;
	while ((count = softdep_process_worklist(oldmnt)) > 0) {
		*countp += count;
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, p);
		error = VOP_FSYNC(devvp, p->p_ucred, MNT_WAIT, p);
		VOP_UNLOCK(devvp, 0, p);
		if (error)
			break;
	}
	softdep_worklist_busy = 0;
	return (error);
}

/*
 * Flush all vnodes and worklist items associated with a specified mount point.
 */
int
softdep_flushfiles(oldmnt, flags, p)
	struct mount *oldmnt;
	int flags;
	struct proc *p;
{
	int error, count, loopcnt;

	/*
	 * Alternately flush the vnodes associated with the mount
	 * point and process any dependencies that the flushing
	 * creates. In theory, this loop can happen at most twice,
	 * but we give it a few extra just to be sure.
	 */
	for (loopcnt = 10; loopcnt > 0; loopcnt--) {
		/*
		 * Do another flush in case any vnodes were brought in
		 * as part of the cleanup operations.
		 */
		if ((error = ffs_flushfiles(oldmnt, flags, p)) != 0)
			break;
		if ((error = softdep_flushworklist(oldmnt, &count, p)) != 0 ||
		    count == 0)
			break;
	}
	/*
	 * If we are unmounting then it is an error to fail. If we
	 * are simply trying to downgrade to read-only, then filesystem
	 * activity can keep us busy forever, so we just fail with EBUSY.
	 */
	if (loopcnt == 0) {
		if (oldmnt->mnt_kern_flag & MNTK_UNMOUNT)
			panic("softdep_flushfiles: looping");
		error = EBUSY;
	}
	return (error);
}

/*
 * Structure hashing.
 * 
 * There are three types of structures that can be looked up:
 *	1) pagedep structures identified by mount point, inode number,
 *	   and logical block.
 *	2) inodedep structures identified by mount point and inode number.
 *	3) newblk structures identified by mount point and
 *	   physical block number.
 *
 * The "pagedep" and "inodedep" dependency structures are hashed
 * separately from the file blocks and inodes to which they correspond.
 * This separation helps when the in-memory copy of an inode or
 * file block must be replaced. It also obviates the need to access
 * an inode or file page when simply updating (or de-allocating)
 * dependency structures. Lookup of newblk structures is needed to
 * find newly allocated blocks when trying to associate them with
 * their allocdirect or allocindir structure.
 *
 * The lookup routines optionally create and hash a new instance when
 * an existing entry is not found.
 */
#define DEPALLOC	0x0001	/* allocate structure if lookup fails */

/*
 * Structures and routines associated with pagedep caching.
 */
LIST_HEAD(pagedep_hashhead, pagedep) *pagedep_hashtbl;
u_long	pagedep_hash;		/* size of hash table - 1 */
#define	PAGEDEP_HASH(mp, inum, lbn) \
	(&pagedep_hashtbl[((((register_t)(mp)) >> 13) + (inum) + (lbn)) & \
	    pagedep_hash])
static struct sema pagedep_in_progress;

/*
 * Look up a pagedep. Return 1 if found, 0 if not found.
 * If not found, allocate if DEPALLOC flag is passed.
 * Found or allocated entry is returned in pagedeppp.
 * This routine must be called with splbio interrupts blocked.
 */
static int
pagedep_lookup(ip, lbn, flags, pagedeppp)
	struct inode *ip;
	ufs_lbn_t lbn;
	int flags;
	struct pagedep **pagedeppp;
{
	struct pagedep *pagedep;
	struct pagedep_hashhead *pagedephd;
	struct mount *mp;
	int i;

#ifdef DEBUG
	if (lk.lkt_held == -1)
		panic("pagedep_lookup: lock not held");
#endif
	mp = ITOV(ip)->v_mount;
	pagedephd = PAGEDEP_HASH(mp, ip->i_number, lbn);
top:
	for (pagedep = LIST_FIRST(pagedephd); pagedep;
	     pagedep = LIST_NEXT(pagedep, pd_hash))
		if (ip->i_number == pagedep->pd_ino &&
		    lbn == pagedep->pd_lbn &&
		    mp == pagedep->pd_mnt)
			break;
	if (pagedep) {
		*pagedeppp = pagedep;
		return (1);
	}
	if ((flags & DEPALLOC) == 0) {
		*pagedeppp = NULL;
		return (0);
	}
	if (sema_get(&pagedep_in_progress, &lk) == 0) {
		ACQUIRE_LOCK(&lk);
		goto top;
	}
	MALLOC(pagedep, struct pagedep *, sizeof(struct pagedep), M_PAGEDEP,
		M_WAITOK);
	bzero(pagedep, sizeof(struct pagedep));
	pagedep->pd_list.wk_type = D_PAGEDEP;
	pagedep->pd_mnt = mp;
	pagedep->pd_ino = ip->i_number;
	pagedep->pd_lbn = lbn;
	LIST_INIT(&pagedep->pd_dirremhd);
	LIST_INIT(&pagedep->pd_pendinghd);
	for (i = 0; i < DAHASHSZ; i++)
		LIST_INIT(&pagedep->pd_diraddhd[i]);
	ACQUIRE_LOCK(&lk);
	LIST_INSERT_HEAD(pagedephd, pagedep, pd_hash);
	sema_release(&pagedep_in_progress);
	*pagedeppp = pagedep;
	return (0);
}

/*
 * Structures and routines associated with inodedep caching.
 */
LIST_HEAD(inodedep_hashhead, inodedep) *inodedep_hashtbl;
static u_long	inodedep_hash;	/* size of hash table - 1 */
static long	num_inodedep;	/* number of inodedep allocated */
#define	INODEDEP_HASH(fs, inum) \
      (&inodedep_hashtbl[((((register_t)(fs)) >> 13) + (inum)) & inodedep_hash])
static struct sema inodedep_in_progress;

/*
 * Look up a inodedep. Return 1 if found, 0 if not found.
 * If not found, allocate if DEPALLOC flag is passed.
 * Found or allocated entry is returned in inodedeppp.
 * This routine must be called with splbio interrupts blocked.
 */
static int
inodedep_lookup(fs, inum, flags, inodedeppp)
	struct fs *fs;
	ino_t inum;
	int flags;
	struct inodedep **inodedeppp;
{
	struct inodedep *inodedep;
	struct inodedep_hashhead *inodedephd;
	int firsttry;

#ifdef DEBUG
	if (lk.lkt_held == -1)
		panic("inodedep_lookup: lock not held");
#endif
	firsttry = 1;
	inodedephd = INODEDEP_HASH(fs, inum);
top:
	for (inodedep = LIST_FIRST(inodedephd); inodedep;
	     inodedep = LIST_NEXT(inodedep, id_hash))
		if (inum == inodedep->id_ino && fs == inodedep->id_fs)
			break;
	if (inodedep) {
		*inodedeppp = inodedep;
		return (1);
	}
	if ((flags & DEPALLOC) == 0) {
		*inodedeppp = NULL;
		return (0);
	}
	/*
	 * If we are over our limit, try to improve the situation.
	 */
	if (num_inodedep > max_softdeps && firsttry && speedup_syncer() == 0 &&
	    request_cleanup(FLUSH_INODES, 1)) {
		firsttry = 0;
		goto top;
	}
	if (sema_get(&inodedep_in_progress, &lk) == 0) {
		ACQUIRE_LOCK(&lk);
		goto top;
	}
	num_inodedep += 1;
	MALLOC(inodedep, struct inodedep *, sizeof(struct inodedep),
		M_INODEDEP, M_WAITOK);
	inodedep->id_list.wk_type = D_INODEDEP;
	inodedep->id_fs = fs;
	inodedep->id_ino = inum;
	inodedep->id_state = ALLCOMPLETE;
	inodedep->id_nlinkdelta = 0;
	inodedep->id_savedino = NULL;
	inodedep->id_savedsize = -1;
	inodedep->id_buf = NULL;
	LIST_INIT(&inodedep->id_pendinghd);
	LIST_INIT(&inodedep->id_inowait);
	LIST_INIT(&inodedep->id_bufwait);
	TAILQ_INIT(&inodedep->id_inoupdt);
	TAILQ_INIT(&inodedep->id_newinoupdt);
	ACQUIRE_LOCK(&lk);
	LIST_INSERT_HEAD(inodedephd, inodedep, id_hash);
	sema_release(&inodedep_in_progress);
	*inodedeppp = inodedep;
	return (0);
}

/*
 * Structures and routines associated with newblk caching.
 */
LIST_HEAD(newblk_hashhead, newblk) *newblk_hashtbl;
u_long	newblk_hash;		/* size of hash table - 1 */
#define	NEWBLK_HASH(fs, inum) \
	(&newblk_hashtbl[((((register_t)(fs)) >> 13) + (inum)) & newblk_hash])
static struct sema newblk_in_progress;

/*
 * Look up a newblk. Return 1 if found, 0 if not found.
 * If not found, allocate if DEPALLOC flag is passed.
 * Found or allocated entry is returned in newblkpp.
 */
static int
newblk_lookup(fs, newblkno, flags, newblkpp)
	struct fs *fs;
	ufs_daddr_t newblkno;
	int flags;
	struct newblk **newblkpp;
{
	struct newblk *newblk;
	struct newblk_hashhead *newblkhd;

	newblkhd = NEWBLK_HASH(fs, newblkno);
top:
	for (newblk = LIST_FIRST(newblkhd); newblk;
	     newblk = LIST_NEXT(newblk, nb_hash))
		if (newblkno == newblk->nb_newblkno && fs == newblk->nb_fs)
			break;
	if (newblk) {
		*newblkpp = newblk;
		return (1);
	}
	if ((flags & DEPALLOC) == 0) {
		*newblkpp = NULL;
		return (0);
	}
	if (sema_get(&newblk_in_progress, 0) == 0)
		goto top;
	MALLOC(newblk, struct newblk *, sizeof(struct newblk),
		M_NEWBLK, M_WAITOK);
	newblk->nb_state = 0;
	newblk->nb_fs = fs;
	newblk->nb_newblkno = newblkno;
	LIST_INSERT_HEAD(newblkhd, newblk, nb_hash);
	sema_release(&newblk_in_progress);
	*newblkpp = newblk;
	return (0);
}

/*
 * Executed during filesystem system initialization before
 * mounting any file systems.
 */
void 
softdep_initialize()
{

	LIST_INIT(&mkdirlisthd);
	LIST_INIT(&softdep_workitem_pending);
	max_softdeps = desiredvnodes * 8;
	pagedep_hashtbl = hashinit(desiredvnodes / 5, M_PAGEDEP,
	    &pagedep_hash);
	sema_init(&pagedep_in_progress, "pagedep", PRIBIO, 0);
	inodedep_hashtbl = hashinit(desiredvnodes, M_INODEDEP, &inodedep_hash);
	sema_init(&inodedep_in_progress, "inodedep", PRIBIO, 0);
	newblk_hashtbl = hashinit(64, M_NEWBLK, &newblk_hash);
	sema_init(&newblk_in_progress, "newblk", PRIBIO, 0);
}

/*
 * Called at mount time to notify the dependency code that a
 * filesystem wishes to use it.
 */
int
softdep_mount(devvp, mp, fs, cred)
	struct vnode *devvp;
	struct mount *mp;
	struct fs *fs;
	struct ucred *cred;
{
	struct csum cstotal;
	struct cg *cgp;
	struct buf *bp;
	int error, cyl;

	mp->mnt_flag &= ~MNT_ASYNC;
	mp->mnt_flag |= MNT_SOFTDEP;
	/*
	 * When doing soft updates, the counters in the
	 * superblock may have gotten out of sync, so we have
	 * to scan the cylinder groups and recalculate them.
	 */
	if (fs->fs_clean != 0)
		return (0);
	bzero(&cstotal, sizeof cstotal);
	for (cyl = 0; cyl < fs->fs_ncg; cyl++) {
		if ((error = bread(devvp, fsbtodb(fs, cgtod(fs, cyl)),
		    fs->fs_cgsize, cred, &bp)) != 0) {
			brelse(bp);
			return (error);
		}
		cgp = (struct cg *)bp->b_data;
		cstotal.cs_nffree += cgp->cg_cs.cs_nffree;
		cstotal.cs_nbfree += cgp->cg_cs.cs_nbfree;
		cstotal.cs_nifree += cgp->cg_cs.cs_nifree;
		cstotal.cs_ndir += cgp->cg_cs.cs_ndir;
		fs->fs_cs(fs, cyl) = cgp->cg_cs;
		brelse(bp);
	}
#ifdef DEBUG
	if (bcmp(&cstotal, &fs->fs_cstotal, sizeof cstotal))
		printf("ffs_mountfs: superblock updated for soft updates\n");
#endif
	bcopy(&cstotal, &fs->fs_cstotal, sizeof cstotal);
	return (0);
}

/*
 * Protecting the freemaps (or bitmaps).
 * 
 * To eliminate the need to execute fsck before mounting a file system
 * after a power failure, one must (conservatively) guarantee that the
 * on-disk copy of the bitmaps never indicate that a live inode or block is
 * free.  So, when a block or inode is allocated, the bitmap should be
 * updated (on disk) before any new pointers.  When a block or inode is
 * freed, the bitmap should not be updated until all pointers have been
 * reset.  The latter dependency is handled by the delayed de-allocation
 * approach described below for block and inode de-allocation.  The former
 * dependency is handled by calling the following procedure when a block or
 * inode is allocated. When an inode is allocated an "inodedep" is created
 * with its DEPCOMPLETE flag cleared until its bitmap is written to disk.
 * Each "inodedep" is also inserted into the hash indexing structure so
 * that any additional link additions can be made dependent on the inode
 * allocation.
 * 
 * The ufs file system maintains a number of free block counts (e.g., per
 * cylinder group, per cylinder and per <cylinder, rotational position> pair)
 * in addition to the bitmaps.  These counts are used to improve efficiency
 * during allocation and therefore must be consistent with the bitmaps.
 * There is no convenient way to guarantee post-crash consistency of these
 * counts with simple update ordering, for two main reasons: (1) The counts
 * and bitmaps for a single cylinder group block are not in the same disk
 * sector.  If a disk write is interrupted (e.g., by power failure), one may
 * be written and the other not.  (2) Some of the counts are located in the
 * superblock rather than the cylinder group block. So, we focus our soft
 * updates implementation on protecting the bitmaps. When mounting a
 * filesystem, we recompute the auxiliary counts from the bitmaps.
 */

/*
 * Called just after updating the cylinder group block to allocate an inode.
 */
void
softdep_setup_inomapdep(bp, ip, newinum)
	struct buf *bp;		/* buffer for cylgroup block with inode map */
	struct inode *ip;	/* inode related to allocation */
	ino_t newinum;		/* new inode number being allocated */
{
	struct inodedep *inodedep;
	struct bmsafemap *bmsafemap;

	/*
	 * Create a dependency for the newly allocated inode.
	 * Panic if it already exists as something is seriously wrong.
	 * Otherwise add it to the dependency list for the buffer holding
	 * the cylinder group map from which it was allocated.
	 */
	ACQUIRE_LOCK(&lk);
	if (inodedep_lookup(ip->i_fs, newinum, DEPALLOC, &inodedep) != 0)
		panic("softdep_setup_inomapdep: found inode");
	inodedep->id_buf = bp;
	inodedep->id_state &= ~DEPCOMPLETE;
	bmsafemap = bmsafemap_lookup(bp);
	LIST_INSERT_HEAD(&bmsafemap->sm_inodedephd, inodedep, id_deps);
	FREE_LOCK(&lk);
}

/*
 * Called just after updating the cylinder group block to
 * allocate block or fragment.
 */
void
softdep_setup_blkmapdep(bp, fs, newblkno)
	struct buf *bp;		/* buffer for cylgroup block with block map */
	struct fs *fs;		/* filesystem doing allocation */
	ufs_daddr_t newblkno;	/* number of newly allocated block */
{
	struct newblk *newblk;
	struct bmsafemap *bmsafemap;

	/*
	 * Create a dependency for the newly allocated block.
	 * Add it to the dependency list for the buffer holding
	 * the cylinder group map from which it was allocated.
	 */
	if (newblk_lookup(fs, newblkno, DEPALLOC, &newblk) != 0)
		panic("softdep_setup_blkmapdep: found block");
	ACQUIRE_LOCK(&lk);
	newblk->nb_bmsafemap = bmsafemap = bmsafemap_lookup(bp);
	LIST_INSERT_HEAD(&bmsafemap->sm_newblkhd, newblk, nb_deps);
	FREE_LOCK(&lk);
}

/*
 * Find the bmsafemap associated with a cylinder group buffer.
 * If none exists, create one. The buffer must be locked when
 * this routine is called and this routine must be called with
 * splbio interrupts blocked.
 */
static struct bmsafemap *
bmsafemap_lookup(bp)
	struct buf *bp;
{
	struct bmsafemap *bmsafemap;
	struct worklist *wk;

#ifdef DEBUG
	if (lk.lkt_held == -1)
		panic("bmsafemap_lookup: lock not held");
#endif
	for (wk = LIST_FIRST(&bp->b_dep); wk; wk = LIST_NEXT(wk, wk_list))
		if (wk->wk_type == D_BMSAFEMAP)
			return (WK_BMSAFEMAP(wk));
	FREE_LOCK(&lk);
	MALLOC(bmsafemap, struct bmsafemap *, sizeof(struct bmsafemap),
		M_BMSAFEMAP, M_WAITOK);
	bmsafemap->sm_list.wk_type = D_BMSAFEMAP;
	bmsafemap->sm_list.wk_state = 0;
	bmsafemap->sm_buf = bp;
	LIST_INIT(&bmsafemap->sm_allocdirecthd);
	LIST_INIT(&bmsafemap->sm_allocindirhd);
	LIST_INIT(&bmsafemap->sm_inodedephd);
	LIST_INIT(&bmsafemap->sm_newblkhd);
	ACQUIRE_LOCK(&lk);
	WORKLIST_INSERT(&bp->b_dep, &bmsafemap->sm_list);
	return (bmsafemap);
}

/*
 * Direct block allocation dependencies.
 * 
 * When a new block is allocated, the corresponding disk locations must be
 * initialized (with zeros or new data) before the on-disk inode points to
 * them.  Also, the freemap from which the block was allocated must be
 * updated (on disk) before the inode's pointer. These two dependencies are
 * independent of each other and are needed for all file blocks and indirect
 * blocks that are pointed to directly by the inode.  Just before the
 * "in-core" version of the inode is updated with a newly allocated block
 * number, a procedure (below) is called to setup allocation dependency
 * structures.  These structures are removed when the corresponding
 * dependencies are satisfied or when the block allocation becomes obsolete
 * (i.e., the file is deleted, the block is de-allocated, or the block is a
 * fragment that gets upgraded).  All of these cases are handled in
 * procedures described later.
 * 
 * When a file extension causes a fragment to be upgraded, either to a larger
 * fragment or to a full block, the on-disk location may change (if the
 * previous fragment could not simply be extended). In this case, the old
 * fragment must be de-allocated, but not until after the inode's pointer has
 * been updated. In most cases, this is handled by later procedures, which
 * will construct a "freefrag" structure to be added to the workitem queue
 * when the inode update is complete (or obsolete).  The main exception to
 * this is when an allocation occurs while a pending allocation dependency
 * (for the same block pointer) remains.  This case is handled in the main
 * allocation dependency setup procedure by immediately freeing the
 * unreferenced fragments.
 */ 
void 
softdep_setup_allocdirect(ip, lbn, newblkno, oldblkno, newsize, oldsize, bp)
	struct inode *ip;	/* inode to which block is being added */
	ufs_lbn_t lbn;		/* block pointer within inode */
	ufs_daddr_t newblkno;	/* disk block number being added */
	ufs_daddr_t oldblkno;	/* previous block number, 0 unless frag */
	long newsize;		/* size of new block */
	long oldsize;		/* size of new block */
	struct buf *bp;		/* bp for allocated block */
{
	struct allocdirect *adp, *oldadp;
	struct allocdirectlst *adphead;
	struct bmsafemap *bmsafemap;
	struct inodedep *inodedep;
	struct pagedep *pagedep;
	struct newblk *newblk;

	MALLOC(adp, struct allocdirect *, sizeof(struct allocdirect),
		M_ALLOCDIRECT, M_WAITOK);
	bzero(adp, sizeof(struct allocdirect));
	adp->ad_list.wk_type = D_ALLOCDIRECT;
	adp->ad_lbn = lbn;
	adp->ad_newblkno = newblkno;
	adp->ad_oldblkno = oldblkno;
	adp->ad_newsize = newsize;
	adp->ad_oldsize = oldsize;
	adp->ad_state = ATTACHED;
	if (newblkno == oldblkno)
		adp->ad_freefrag = NULL;
	else
		adp->ad_freefrag = newfreefrag(ip, oldblkno, oldsize);

	if (newblk_lookup(ip->i_fs, newblkno, 0, &newblk) == 0)
		panic("softdep_setup_allocdirect: lost block");

	ACQUIRE_LOCK(&lk);
	(void) inodedep_lookup(ip->i_fs, ip->i_number, DEPALLOC, &inodedep);
	adp->ad_inodedep = inodedep;

	if (newblk->nb_state == DEPCOMPLETE) {
		adp->ad_state |= DEPCOMPLETE;
		adp->ad_buf = NULL;
	} else {
		bmsafemap = newblk->nb_bmsafemap;
		adp->ad_buf = bmsafemap->sm_buf;
		LIST_REMOVE(newblk, nb_deps);
		LIST_INSERT_HEAD(&bmsafemap->sm_allocdirecthd, adp, ad_deps);
	}
	LIST_REMOVE(newblk, nb_hash);
	FREE(newblk, M_NEWBLK);

	WORKLIST_INSERT(&bp->b_dep, &adp->ad_list);
	if (lbn >= NDADDR) {
		/* allocating an indirect block */
		if (oldblkno != 0)
			panic("softdep_setup_allocdirect: non-zero indir");
	} else {
		/*
		 * Allocating a direct block.
		 *
		 * If we are allocating a directory block, then we must
		 * allocate an associated pagedep to track additions and
		 * deletions.
		 */
		if ((ip->i_mode & IFMT) == IFDIR &&
		    pagedep_lookup(ip, lbn, DEPALLOC, &pagedep) == 0)
			WORKLIST_INSERT(&bp->b_dep, &pagedep->pd_list);
	}
	/*
	 * The list of allocdirects must be kept in sorted and ascending
	 * order so that the rollback routines can quickly determine the
	 * first uncommitted block (the size of the file stored on disk
	 * ends at the end of the lowest committed fragment, or if there
	 * are no fragments, at the end of the highest committed block).
	 * Since files generally grow, the typical case is that the new
	 * block is to be added at the end of the list. We speed this
	 * special case by checking against the last allocdirect in the
	 * list before laboriously traversing the list looking for the
	 * insertion point.
	 */
	adphead = &inodedep->id_newinoupdt;
	oldadp = TAILQ_LAST(adphead, allocdirectlst);
	if (oldadp == NULL || oldadp->ad_lbn <= lbn) {
		/* insert at end of list */
		TAILQ_INSERT_TAIL(adphead, adp, ad_next);
		if (oldadp != NULL && oldadp->ad_lbn == lbn)
			allocdirect_merge(adphead, adp, oldadp);
		FREE_LOCK(&lk);
		return;
	}
	for (oldadp = TAILQ_FIRST(adphead); oldadp;
	     oldadp = TAILQ_NEXT(oldadp, ad_next)) {
		if (oldadp->ad_lbn >= lbn)
			break;
	}
	if (oldadp == NULL)
		panic("softdep_setup_allocdirect: lost entry");
	/* insert in middle of list */
	TAILQ_INSERT_BEFORE(oldadp, adp, ad_next);
	if (oldadp->ad_lbn == lbn)
		allocdirect_merge(adphead, adp, oldadp);
	FREE_LOCK(&lk);
}

/*
 * Replace an old allocdirect dependency with a newer one.
 * This routine must be called with splbio interrupts blocked.
 */
static void
allocdirect_merge(adphead, newadp, oldadp)
	struct allocdirectlst *adphead;	/* head of list holding allocdirects */
	struct allocdirect *newadp;	/* allocdirect being added */
	struct allocdirect *oldadp;	/* existing allocdirect being checked */
{
	struct freefrag *freefrag;

#ifdef DEBUG
	if (lk.lkt_held == -1)
		panic("allocdirect_merge: lock not held");
#endif
	if (newadp->ad_oldblkno != oldadp->ad_newblkno ||
	    newadp->ad_oldsize != oldadp->ad_newsize ||
	    newadp->ad_lbn >= NDADDR)
		panic("allocdirect_check: old %d != new %d || lbn %ld >= %d",
		    newadp->ad_oldblkno, oldadp->ad_newblkno, newadp->ad_lbn,
		    NDADDR);
	newadp->ad_oldblkno = oldadp->ad_oldblkno;
	newadp->ad_oldsize = oldadp->ad_oldsize;
	/*
	 * If the old dependency had a fragment to free or had never
	 * previously had a block allocated, then the new dependency
	 * can immediately post its freefrag and adopt the old freefrag.
	 * This action is done by swapping the freefrag dependencies.
	 * The new dependency gains the old one's freefrag, and the
	 * old one gets the new one and then immediately puts it on
	 * the worklist when it is freed by free_allocdirect. It is
	 * not possible to do this swap when the old dependency had a
	 * non-zero size but no previous fragment to free. This condition
	 * arises when the new block is an extension of the old block.
	 * Here, the first part of the fragment allocated to the new
	 * dependency is part of the block currently claimed on disk by
	 * the old dependency, so cannot legitimately be freed until the
	 * conditions for the new dependency are fulfilled.
	 */
	if (oldadp->ad_freefrag != NULL || oldadp->ad_oldblkno == 0) {
		freefrag = newadp->ad_freefrag;
		newadp->ad_freefrag = oldadp->ad_freefrag;
		oldadp->ad_freefrag = freefrag;
	}
	free_allocdirect(adphead, oldadp, 0);
}
		
/*
 * Allocate a new freefrag structure if needed.
 */
static struct freefrag *
newfreefrag(ip, blkno, size)
	struct inode *ip;
	ufs_daddr_t blkno;
	long size;
{
	struct freefrag *freefrag;
	struct fs *fs;

	if (blkno == 0)
		return (NULL);
	fs = ip->i_fs;
	if (fragnum(fs, blkno) + numfrags(fs, size) > fs->fs_frag)
		panic("newfreefrag: frag size");
	MALLOC(freefrag, struct freefrag *, sizeof(struct freefrag),
		M_FREEFRAG, M_WAITOK);
	freefrag->ff_list.wk_type = D_FREEFRAG;
	freefrag->ff_state = ip->i_uid & ~ONWORKLIST;	/* XXX - used below */
	freefrag->ff_inum = ip->i_number;
	freefrag->ff_mnt = ITOV(ip)->v_mount;
	freefrag->ff_devvp = ip->i_devvp;
	freefrag->ff_blkno = blkno;
	freefrag->ff_fragsize = size;
	return (freefrag);
}

/*
 * This workitem de-allocates fragments that were replaced during
 * file block allocation.
 */
static void 
handle_workitem_freefrag(freefrag)
	struct freefrag *freefrag;
{
	struct inode tip;

	tip.i_vnode = NULL;
	tip.i_fs = VFSTOUFS(freefrag->ff_mnt)->um_fs;
	tip.i_devvp = freefrag->ff_devvp;
	tip.i_dev = freefrag->ff_devvp->v_rdev;
	tip.i_number = freefrag->ff_inum;
	tip.i_uid = freefrag->ff_state & ~ONWORKLIST;	/* XXX - set above */
	ffs_blkfree(&tip, freefrag->ff_blkno, freefrag->ff_fragsize);
	FREE(freefrag, M_FREEFRAG);
}

/*
 * Indirect block allocation dependencies.
 * 
 * The same dependencies that exist for a direct block also exist when
 * a new block is allocated and pointed to by an entry in a block of
 * indirect pointers. The undo/redo states described above are also
 * used here. Because an indirect block contains many pointers that
 * may have dependencies, a second copy of the entire in-memory indirect
 * block is kept. The buffer cache copy is always completely up-to-date.
 * The second copy, which is used only as a source for disk writes,
 * contains only the safe pointers (i.e., those that have no remaining
 * update dependencies). The second copy is freed when all pointers
 * are safe. The cache is not allowed to replace indirect blocks with
 * pending update dependencies. If a buffer containing an indirect
 * block with dependencies is written, these routines will mark it
 * dirty again. It can only be successfully written once all the
 * dependencies are removed. The ffs_fsync routine in conjunction with
 * softdep_sync_metadata work together to get all the dependencies
 * removed so that a file can be successfully written to disk. Three
 * procedures are used when setting up indirect block pointer
 * dependencies. The division is necessary because of the organization
 * of the "balloc" routine and because of the distinction between file
 * pages and file metadata blocks.
 */

/*
 * Allocate a new allocindir structure.
 */
static struct allocindir *
newallocindir(ip, ptrno, newblkno, oldblkno)
	struct inode *ip;	/* inode for file being extended */
	int ptrno;		/* offset of pointer in indirect block */
	ufs_daddr_t newblkno;	/* disk block number being added */
	ufs_daddr_t oldblkno;	/* previous block number, 0 if none */
{
	struct allocindir *aip;

	MALLOC(aip, struct allocindir *, sizeof(struct allocindir),
		M_ALLOCINDIR, M_WAITOK);
	bzero(aip, sizeof(struct allocindir));
	aip->ai_list.wk_type = D_ALLOCINDIR;
	aip->ai_state = ATTACHED;
	aip->ai_offset = ptrno;
	aip->ai_newblkno = newblkno;
	aip->ai_oldblkno = oldblkno;
	aip->ai_freefrag = newfreefrag(ip, oldblkno, ip->i_fs->fs_bsize);
	return (aip);
}

/*
 * Called just before setting an indirect block pointer
 * to a newly allocated file page.
 */
void
softdep_setup_allocindir_page(ip, lbn, bp, ptrno, newblkno, oldblkno, nbp)
	struct inode *ip;	/* inode for file being extended */
	ufs_lbn_t lbn;		/* allocated block number within file */
	struct buf *bp;		/* buffer with indirect blk referencing page */
	int ptrno;		/* offset of pointer in indirect block */
	ufs_daddr_t newblkno;	/* disk block number being added */
	ufs_daddr_t oldblkno;	/* previous block number, 0 if none */
	struct buf *nbp;	/* buffer holding allocated page */
{
	struct allocindir *aip;
	struct pagedep *pagedep;

	aip = newallocindir(ip, ptrno, newblkno, oldblkno);
	ACQUIRE_LOCK(&lk);
	/*
	 * If we are allocating a directory page, then we must
	 * allocate an associated pagedep to track additions and
	 * deletions.
	 */
	if ((ip->i_mode & IFMT) == IFDIR &&
	    pagedep_lookup(ip, lbn, DEPALLOC, &pagedep) == 0)
		WORKLIST_INSERT(&nbp->b_dep, &pagedep->pd_list);
	WORKLIST_INSERT(&nbp->b_dep, &aip->ai_list);
	FREE_LOCK(&lk);
	setup_allocindir_phase2(bp, ip, aip);
}

/*
 * Called just before setting an indirect block pointer to a
 * newly allocated indirect block.
 */
void
softdep_setup_allocindir_meta(nbp, ip, bp, ptrno, newblkno)
	struct buf *nbp;	/* newly allocated indirect block */
	struct inode *ip;	/* inode for file being extended */
	struct buf *bp;		/* indirect block referencing allocated block */
	int ptrno;		/* offset of pointer in indirect block */
	ufs_daddr_t newblkno;	/* disk block number being added */
{
	struct allocindir *aip;

	aip = newallocindir(ip, ptrno, newblkno, 0);
	ACQUIRE_LOCK(&lk);
	WORKLIST_INSERT(&nbp->b_dep, &aip->ai_list);
	FREE_LOCK(&lk);
	setup_allocindir_phase2(bp, ip, aip);
}

/*
 * Called to finish the allocation of the "aip" allocated
 * by one of the two routines above.
 */
static void 
setup_allocindir_phase2(bp, ip, aip)
	struct buf *bp;		/* in-memory copy of the indirect block */
	struct inode *ip;	/* inode for file being extended */
	struct allocindir *aip;	/* allocindir allocated by the above routines */
{
	struct worklist *wk;
	struct indirdep *indirdep, *newindirdep;
	struct bmsafemap *bmsafemap;
	struct allocindir *oldaip;
	struct freefrag *freefrag;
	struct newblk *newblk;

	if (bp->b_lblkno >= 0)
		panic("setup_allocindir_phase2: not indir blk");
	for (indirdep = NULL, newindirdep = NULL; ; ) {
		ACQUIRE_LOCK(&lk);
		for (wk = LIST_FIRST(&bp->b_dep); wk;
		     wk = LIST_NEXT(wk, wk_list)) {
			if (wk->wk_type != D_INDIRDEP)
				continue;
			indirdep = WK_INDIRDEP(wk);
			break;
		}
		if (indirdep == NULL && newindirdep) {
			indirdep = newindirdep;
			WORKLIST_INSERT(&bp->b_dep, &indirdep->ir_list);
			newindirdep = NULL;
		}
		FREE_LOCK(&lk);
		if (indirdep) {
			if (newblk_lookup(ip->i_fs, aip->ai_newblkno, 0,
			    &newblk) == 0)
				panic("setup_allocindir: lost block");
			ACQUIRE_LOCK(&lk);
			if (newblk->nb_state == DEPCOMPLETE) {
				aip->ai_state |= DEPCOMPLETE;
				aip->ai_buf = NULL;
			} else {
				bmsafemap = newblk->nb_bmsafemap;
				aip->ai_buf = bmsafemap->sm_buf;
				LIST_REMOVE(newblk, nb_deps);
				LIST_INSERT_HEAD(&bmsafemap->sm_allocindirhd,
				    aip, ai_deps);
			}
			LIST_REMOVE(newblk, nb_hash);
			FREE(newblk, M_NEWBLK);
			aip->ai_indirdep = indirdep;
			/*
			 * Check to see if there is an existing dependency
			 * for this block. If there is, merge the old
			 * dependency into the new one.
			 */
			if (aip->ai_oldblkno == 0)
				oldaip = NULL;
			else
				for (oldaip=LIST_FIRST(&indirdep->ir_deplisthd);
				    oldaip; oldaip = LIST_NEXT(oldaip, ai_next))
					if (oldaip->ai_offset == aip->ai_offset)
						break;
			freefrag = NULL;
			if (oldaip != NULL) {
				if (oldaip->ai_newblkno != aip->ai_oldblkno)
					panic("setup_allocindir_phase2: blkno");
				aip->ai_oldblkno = oldaip->ai_oldblkno;
				freefrag = aip->ai_freefrag;
				aip->ai_freefrag = oldaip->ai_freefrag;
				oldaip->ai_freefrag = NULL;
				free_allocindir(oldaip, NULL);
			}
			LIST_INSERT_HEAD(&indirdep->ir_deplisthd, aip, ai_next);
			((ufs_daddr_t *)indirdep->ir_savebp->b_data)
			    [aip->ai_offset] = aip->ai_oldblkno;
			FREE_LOCK(&lk);
			if (freefrag != NULL)
				handle_workitem_freefrag(freefrag);
		}
		if (newindirdep) {
			if (indirdep->ir_savebp != NULL)
				brelse(newindirdep->ir_savebp);
			WORKITEM_FREE((caddr_t)newindirdep, D_INDIRDEP);
		}
		if (indirdep)
			break;
		MALLOC(newindirdep, struct indirdep *, sizeof(struct indirdep),
			M_INDIRDEP, M_WAITOK);
		newindirdep->ir_list.wk_type = D_INDIRDEP;
		newindirdep->ir_state = ATTACHED;
		LIST_INIT(&newindirdep->ir_deplisthd);
		LIST_INIT(&newindirdep->ir_donehd);
		if (bp->b_blkno == bp->b_lblkno) {
			VOP_BMAP(bp->b_vp, bp->b_lblkno, NULL, &bp->b_blkno,
				NULL, NULL);
		}
		newindirdep->ir_savebp =
		    getblk(ip->i_devvp, bp->b_blkno, bp->b_bcount, 0, 0);
		BUF_KERNPROC(newindirdep->ir_savebp);
		bcopy(bp->b_data, newindirdep->ir_savebp->b_data, bp->b_bcount);
	}
}

/*
 * Block de-allocation dependencies.
 * 
 * When blocks are de-allocated, the on-disk pointers must be nullified before
 * the blocks are made available for use by other files.  (The true
 * requirement is that old pointers must be nullified before new on-disk
 * pointers are set.  We chose this slightly more stringent requirement to
 * reduce complexity.) Our implementation handles this dependency by updating
 * the inode (or indirect block) appropriately but delaying the actual block
 * de-allocation (i.e., freemap and free space count manipulation) until
 * after the updated versions reach stable storage.  After the disk is
 * updated, the blocks can be safely de-allocated whenever it is convenient.
 * This implementation handles only the common case of reducing a file's
 * length to zero. Other cases are handled by the conventional synchronous
 * write approach.
 *
 * The ffs implementation with which we worked double-checks
 * the state of the block pointers and file size as it reduces
 * a file's length.  Some of this code is replicated here in our
 * soft updates implementation.  The freeblks->fb_chkcnt field is
 * used to transfer a part of this information to the procedure
 * that eventually de-allocates the blocks.
 *
 * This routine should be called from the routine that shortens
 * a file's length, before the inode's size or block pointers
 * are modified. It will save the block pointer information for
 * later release and zero the inode so that the calling routine
 * can release it.
 */
void
softdep_setup_freeblocks(ip, length)
	struct inode *ip;	/* The inode whose length is to be reduced */
	off_t length;		/* The new length for the file */
{
	struct freeblks *freeblks;
	struct inodedep *inodedep;
	struct allocdirect *adp;
	struct vnode *vp;
	struct buf *bp;
	struct fs *fs;
	int i, delay, error;

	fs = ip->i_fs;
	if (length != 0)
		panic("softde_setup_freeblocks: non-zero length");
	MALLOC(freeblks, struct freeblks *, sizeof(struct freeblks),
		M_FREEBLKS, M_WAITOK);
	bzero(freeblks, sizeof(struct freeblks));
	freeblks->fb_list.wk_type = D_FREEBLKS;
	freeblks->fb_uid = ip->i_uid;
	freeblks->fb_previousinum = ip->i_number;
	freeblks->fb_devvp = ip->i_devvp;
	freeblks->fb_mnt = ITOV(ip)->v_mount;
	freeblks->fb_oldsize = ip->i_size;
	freeblks->fb_newsize = length;
	freeblks->fb_chkcnt = ip->i_blocks;
	for (i = 0; i < NDADDR; i++) {
		freeblks->fb_dblks[i] = ip->i_db[i];
		ip->i_db[i] = 0;
	}
	for (i = 0; i < NIADDR; i++) {
		freeblks->fb_iblks[i] = ip->i_ib[i];
		ip->i_ib[i] = 0;
	}
	ip->i_blocks = 0;
	ip->i_size = 0;
	/*
	 * Push the zero'ed inode to to its disk buffer so that we are free
	 * to delete its dependencies below. Once the dependencies are gone
	 * the buffer can be safely released.
	 */
	if ((error = bread(ip->i_devvp,
	    fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
	    (int)fs->fs_bsize, NOCRED, &bp)) != 0)
		softdep_error("softdep_setup_freeblocks", error);
	*((struct dinode *)bp->b_data + ino_to_fsbo(fs, ip->i_number)) =
	    ip->i_din;
	/*
	 * Find and eliminate any inode dependencies.
	 */
	ACQUIRE_LOCK(&lk);
	(void) inodedep_lookup(fs, ip->i_number, DEPALLOC, &inodedep);
	if ((inodedep->id_state & IOSTARTED) != 0)
		panic("softdep_setup_freeblocks: inode busy");
	/*
	 * Because the file length has been truncated to zero, any
	 * pending block allocation dependency structures associated
	 * with this inode are obsolete and can simply be de-allocated.
	 * We must first merge the two dependency lists to get rid of
	 * any duplicate freefrag structures, then purge the merged list.
	 * If we still have a bitmap dependency, then the inode has never
	 * been written to disk, so we can free any fragments without delay.
	 */
	merge_inode_lists(inodedep);
	delay = (inodedep->id_state & DEPCOMPLETE);
	while ((adp = TAILQ_FIRST(&inodedep->id_inoupdt)) != 0)
		free_allocdirect(&inodedep->id_inoupdt, adp, delay);
	FREE_LOCK(&lk);
	bdwrite(bp);
	/*
	 * We must wait for any I/O in progress to finish so that
	 * all potential buffers on the dirty list will be visible.
	 * Once they are all there, walk the list and get rid of
	 * any dependencies.
	 */
	vp = ITOV(ip);
	ACQUIRE_LOCK(&lk);
	drain_output(vp, 1);
	while (getdirtybuf(&TAILQ_FIRST(&vp->v_dirtyblkhd), MNT_WAIT)) {
		bp = TAILQ_FIRST(&vp->v_dirtyblkhd);
		(void) inodedep_lookup(fs, ip->i_number, 0, &inodedep);
		deallocate_dependencies(bp, inodedep);
		bp->b_flags |= B_INVAL | B_NOCACHE;
		FREE_LOCK(&lk);
		brelse(bp);
		ACQUIRE_LOCK(&lk);
	}
	/*
	 * Add the freeblks structure to the list of operations that
	 * must await the zero'ed inode being written to disk. If we
	 * still have a bitmap dependency, then the inode has never been
	 * written to disk, so we can process the freeblks immediately.
	 * If the inodedep does not exist, then the zero'ed inode has
	 * been written and we can also proceed.
	 */
	if (inodedep_lookup(fs, ip->i_number, 0, &inodedep) == 0 ||
	    free_inodedep(inodedep) ||
	    (inodedep->id_state & DEPCOMPLETE) == 0) {
		FREE_LOCK(&lk);
		handle_workitem_freeblocks(freeblks);
	} else {
		WORKLIST_INSERT(&inodedep->id_bufwait, &freeblks->fb_list);
		FREE_LOCK(&lk);
	}
}

/*
 * Reclaim any dependency structures from a buffer that is about to
 * be reallocated to a new vnode. The buffer must be locked, thus,
 * no I/O completion operations can occur while we are manipulating
 * its associated dependencies. The mutex is held so that other I/O's
 * associated with related dependencies do not occur.
 */
static void
deallocate_dependencies(bp, inodedep)
	struct buf *bp;
	struct inodedep *inodedep;
{
	struct worklist *wk;
	struct indirdep *indirdep;
	struct allocindir *aip;
	struct pagedep *pagedep;
	struct dirrem *dirrem;
	struct diradd *dap;
	int i;

	while ((wk = LIST_FIRST(&bp->b_dep)) != NULL) {
		switch (wk->wk_type) {

		case D_INDIRDEP:
			indirdep = WK_INDIRDEP(wk);
			/*
			 * None of the indirect pointers will ever be visible,
			 * so they can simply be tossed. GOINGAWAY ensures
			 * that allocated pointers will be saved in the buffer
			 * cache until they are freed. Note that they will
			 * only be able to be found by their physical address
			 * since the inode mapping the logical address will
			 * be gone. The save buffer used for the safe copy
			 * was allocated in setup_allocindir_phase2 using
			 * the physical address so it could be used for this
			 * purpose. Hence we swap the safe copy with the real
			 * copy, allowing the safe copy to be freed and holding
			 * on to the real copy for later use in indir_trunc.
			 */
			if (indirdep->ir_state & GOINGAWAY)
				panic("deallocate_dependencies: already gone");
			indirdep->ir_state |= GOINGAWAY;
			while ((aip = LIST_FIRST(&indirdep->ir_deplisthd)) != 0)
				free_allocindir(aip, inodedep);
			if (bp->b_lblkno >= 0 ||
			    bp->b_blkno != indirdep->ir_savebp->b_lblkno)
				panic("deallocate_dependencies: not indir");
			bcopy(bp->b_data, indirdep->ir_savebp->b_data,
			    bp->b_bcount);
			WORKLIST_REMOVE(wk);
			WORKLIST_INSERT(&indirdep->ir_savebp->b_dep, wk);
			continue;

		case D_PAGEDEP:
			pagedep = WK_PAGEDEP(wk);
			/*
			 * None of the directory additions will ever be
			 * visible, so they can simply be tossed.
			 */
			for (i = 0; i < DAHASHSZ; i++)
				while ((dap =
				    LIST_FIRST(&pagedep->pd_diraddhd[i])))
					free_diradd(dap);
			while ((dap = LIST_FIRST(&pagedep->pd_pendinghd)) != 0)
				free_diradd(dap);
			/*
			 * Copy any directory remove dependencies to the list
			 * to be processed after the zero'ed inode is written.
			 * If the inode has already been written, then they 
			 * can be dumped directly onto the work list.
			 */
			for (dirrem = LIST_FIRST(&pagedep->pd_dirremhd); dirrem;
			     dirrem = LIST_NEXT(dirrem, dm_next)) {
				LIST_REMOVE(dirrem, dm_next);
				dirrem->dm_dirinum = pagedep->pd_ino;
				if (inodedep == NULL ||
				    (inodedep->id_state & ALLCOMPLETE) ==
				     ALLCOMPLETE)
					add_to_worklist(&dirrem->dm_list);
				else
					WORKLIST_INSERT(&inodedep->id_bufwait,
					    &dirrem->dm_list);
			}
			WORKLIST_REMOVE(&pagedep->pd_list);
			LIST_REMOVE(pagedep, pd_hash);
			WORKITEM_FREE(pagedep, D_PAGEDEP);
			continue;

		case D_ALLOCINDIR:
			free_allocindir(WK_ALLOCINDIR(wk), inodedep);
			continue;

		case D_ALLOCDIRECT:
		case D_INODEDEP:
			panic("deallocate_dependencies: Unexpected type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */

		default:
			panic("deallocate_dependencies: Unknown type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
}

/*
 * Free an allocdirect. Generate a new freefrag work request if appropriate.
 * This routine must be called with splbio interrupts blocked.
 */
static void
free_allocdirect(adphead, adp, delay)
	struct allocdirectlst *adphead;
	struct allocdirect *adp;
	int delay;
{

#ifdef DEBUG
	if (lk.lkt_held == -1)
		panic("free_allocdirect: lock not held");
#endif
	if ((adp->ad_state & DEPCOMPLETE) == 0)
		LIST_REMOVE(adp, ad_deps);
	TAILQ_REMOVE(adphead, adp, ad_next);
	if ((adp->ad_state & COMPLETE) == 0)
		WORKLIST_REMOVE(&adp->ad_list);
	if (adp->ad_freefrag != NULL) {
		if (delay)
			WORKLIST_INSERT(&adp->ad_inodedep->id_bufwait,
			    &adp->ad_freefrag->ff_list);
		else
			add_to_worklist(&adp->ad_freefrag->ff_list);
	}
	WORKITEM_FREE(adp, D_ALLOCDIRECT);
}

/*
 * Prepare an inode to be freed. The actual free operation is not
 * done until the zero'ed inode has been written to disk.
 */
void
softdep_freefile(pvp, ino, mode)
		struct vnode *pvp;
		ino_t ino;
		int mode;
{
	struct inode *ip = VTOI(pvp);
	struct inodedep *inodedep;
	struct freefile *freefile;

	/*
	 * This sets up the inode de-allocation dependency.
	 */
	MALLOC(freefile, struct freefile *, sizeof(struct freefile),
		M_FREEFILE, M_WAITOK);
	freefile->fx_list.wk_type = D_FREEFILE;
	freefile->fx_list.wk_state = 0;
	freefile->fx_mode = mode;
	freefile->fx_oldinum = ino;
	freefile->fx_devvp = ip->i_devvp;
	freefile->fx_mnt = ITOV(ip)->v_mount;

	/*
	 * If the inodedep does not exist, then the zero'ed inode has
	 * been written to disk. If the allocated inode has never been
	 * written to disk, then the on-disk inode is zero'ed. In either
	 * case we can free the file immediately.
	 */
	ACQUIRE_LOCK(&lk);
	if (inodedep_lookup(ip->i_fs, ino, 0, &inodedep) == 0 ||
	    check_inode_unwritten(inodedep)) {
		FREE_LOCK(&lk);
		handle_workitem_freefile(freefile);
		return;
	}
	WORKLIST_INSERT(&inodedep->id_inowait, &freefile->fx_list);
	FREE_LOCK(&lk);
}

/*
 * Check to see if an inode has never been written to disk. If
 * so free the inodedep and return success, otherwise return failure.
 * This routine must be called with splbio interrupts blocked.
 *
 * If we still have a bitmap dependency, then the inode has never
 * been written to disk. Drop the dependency as it is no longer
 * necessary since the inode is being deallocated. We set the
 * ALLCOMPLETE flags since the bitmap now properly shows that the
 * inode is not allocated. Even if the inode is actively being
 * written, it has been rolled back to its zero'ed state, so we
 * are ensured that a zero inode is what is on the disk. For short
 * lived files, this change will usually result in removing all the
 * dependencies from the inode so that it can be freed immediately.
 */
static int
check_inode_unwritten(inodedep)
	struct inodedep *inodedep;
{

	if ((inodedep->id_state & DEPCOMPLETE) != 0 ||
	    LIST_FIRST(&inodedep->id_pendinghd) != NULL ||
	    LIST_FIRST(&inodedep->id_bufwait) != NULL ||
	    LIST_FIRST(&inodedep->id_inowait) != NULL ||
	    TAILQ_FIRST(&inodedep->id_inoupdt) != NULL ||
	    TAILQ_FIRST(&inodedep->id_newinoupdt) != NULL ||
	    inodedep->id_nlinkdelta != 0)
		return (0);
	inodedep->id_state |= ALLCOMPLETE;
	LIST_REMOVE(inodedep, id_deps);
	inodedep->id_buf = NULL;
	if (inodedep->id_state & ONWORKLIST)
		WORKLIST_REMOVE(&inodedep->id_list);
	if (inodedep->id_savedino != NULL) {
		FREE(inodedep->id_savedino, M_INODEDEP);
		inodedep->id_savedino = NULL;
	}
	if (free_inodedep(inodedep) == 0)
		panic("check_inode_unwritten: busy inode");
	return (1);
}

/*
 * Try to free an inodedep structure. Return 1 if it could be freed.
 */
static int
free_inodedep(inodedep)
	struct inodedep *inodedep;
{

	if ((inodedep->id_state & ONWORKLIST) != 0 ||
	    (inodedep->id_state & ALLCOMPLETE) != ALLCOMPLETE ||
	    LIST_FIRST(&inodedep->id_pendinghd) != NULL ||
	    LIST_FIRST(&inodedep->id_bufwait) != NULL ||
	    LIST_FIRST(&inodedep->id_inowait) != NULL ||
	    TAILQ_FIRST(&inodedep->id_inoupdt) != NULL ||
	    TAILQ_FIRST(&inodedep->id_newinoupdt) != NULL ||
	    inodedep->id_nlinkdelta != 0 || inodedep->id_savedino != NULL)
		return (0);
	LIST_REMOVE(inodedep, id_hash);
	WORKITEM_FREE(inodedep, D_INODEDEP);
	num_inodedep -= 1;
	return (1);
}

/*
 * This workitem routine performs the block de-allocation.
 * The workitem is added to the pending list after the updated
 * inode block has been written to disk.  As mentioned above,
 * checks regarding the number of blocks de-allocated (compared
 * to the number of blocks allocated for the file) are also
 * performed in this function.
 */
static void
handle_workitem_freeblocks(freeblks)
	struct freeblks *freeblks;
{
	struct inode tip;
	ufs_daddr_t bn;
	struct fs *fs;
	int i, level, bsize;
	long nblocks, blocksreleased = 0;
	int error, allerror = 0;
	ufs_lbn_t baselbns[NIADDR], tmpval;

	tip.i_fs = fs = VFSTOUFS(freeblks->fb_mnt)->um_fs;
	tip.i_number = freeblks->fb_previousinum;
	tip.i_devvp = freeblks->fb_devvp;
	tip.i_dev = freeblks->fb_devvp->v_rdev;
	tip.i_size = freeblks->fb_oldsize;
	tip.i_uid = freeblks->fb_uid;
	tip.i_vnode = NULL;
	tmpval = 1;
	baselbns[0] = NDADDR;
	for (i = 1; i < NIADDR; i++) {
		tmpval *= NINDIR(fs);
		baselbns[i] = baselbns[i - 1] + tmpval;
	}
	nblocks = btodb(fs->fs_bsize);
	blocksreleased = 0;
	/*
	 * Indirect blocks first.
	 */
	for (level = (NIADDR - 1); level >= 0; level--) {
		if ((bn = freeblks->fb_iblks[level]) == 0)
			continue;
		if ((error = indir_trunc(&tip, fsbtodb(fs, bn), level,
		    baselbns[level], &blocksreleased)) == 0)
			allerror = error;
		ffs_blkfree(&tip, bn, fs->fs_bsize);
		blocksreleased += nblocks;
	}
	/*
	 * All direct blocks or frags.
	 */
	for (i = (NDADDR - 1); i >= 0; i--) {
		if ((bn = freeblks->fb_dblks[i]) == 0)
			continue;
		bsize = blksize(fs, &tip, i);
		ffs_blkfree(&tip, bn, bsize);
		blocksreleased += btodb(bsize);
	}

#ifdef DIAGNOSTIC
	if (freeblks->fb_chkcnt != blocksreleased)
		panic("handle_workitem_freeblocks: block count");
	if (allerror)
		softdep_error("handle_workitem_freeblks", allerror);
#endif /* DIAGNOSTIC */
	WORKITEM_FREE(freeblks, D_FREEBLKS);
}

/*
 * Release blocks associated with the inode ip and stored in the indirect
 * block dbn. If level is greater than SINGLE, the block is an indirect block
 * and recursive calls to indirtrunc must be used to cleanse other indirect
 * blocks.
 */
static int
indir_trunc(ip, dbn, level, lbn, countp)
	struct inode *ip;
	ufs_daddr_t dbn;
	int level;
	ufs_lbn_t lbn;
	long *countp;
{
	struct buf *bp;
	ufs_daddr_t *bap;
	ufs_daddr_t nb;
	struct fs *fs;
	struct worklist *wk;
	struct indirdep *indirdep;
	int i, lbnadd, nblocks;
	int error, allerror = 0;

	fs = ip->i_fs;
	lbnadd = 1;
	for (i = level; i > 0; i--)
		lbnadd *= NINDIR(fs);
	/*
	 * Get buffer of block pointers to be freed. This routine is not
	 * called until the zero'ed inode has been written, so it is safe
	 * to free blocks as they are encountered. Because the inode has
	 * been zero'ed, calls to bmap on these blocks will fail. So, we
	 * have to use the on-disk address and the block device for the
	 * filesystem to look them up. If the file was deleted before its
	 * indirect blocks were all written to disk, the routine that set
	 * us up (deallocate_dependencies) will have arranged to leave
	 * a complete copy of the indirect block in memory for our use.
	 * Otherwise we have to read the blocks in from the disk.
	 */
	ACQUIRE_LOCK(&lk);
	if ((bp = incore(ip->i_devvp, dbn)) != NULL &&
	    (wk = LIST_FIRST(&bp->b_dep)) != NULL) {
		if (wk->wk_type != D_INDIRDEP ||
		    (indirdep = WK_INDIRDEP(wk))->ir_savebp != bp ||
		    (indirdep->ir_state & GOINGAWAY) == 0)
			panic("indir_trunc: lost indirdep");
		WORKLIST_REMOVE(wk);
		WORKITEM_FREE(indirdep, D_INDIRDEP);
		if (LIST_FIRST(&bp->b_dep) != NULL)
			panic("indir_trunc: dangling dep");
		FREE_LOCK(&lk);
	} else {
		FREE_LOCK(&lk);
		error = bread(ip->i_devvp, dbn, (int)fs->fs_bsize, NOCRED, &bp);
		if (error)
			return (error);
	}
	/*
	 * Recursively free indirect blocks.
	 */
	bap = (ufs_daddr_t *)bp->b_data;
	nblocks = btodb(fs->fs_bsize);
	for (i = NINDIR(fs) - 1; i >= 0; i--) {
		if ((nb = bap[i]) == 0)
			continue;
		if (level != 0) {
			if ((error = indir_trunc(ip, fsbtodb(fs, nb),
			     level - 1, lbn + (i * lbnadd), countp)) != 0)
				allerror = error;
		}
		ffs_blkfree(ip, nb, fs->fs_bsize);
		*countp += nblocks;
	}
	bp->b_flags |= B_INVAL | B_NOCACHE;
	brelse(bp);
	return (allerror);
}

/*
 * Free an allocindir.
 * This routine must be called with splbio interrupts blocked.
 */
static void
free_allocindir(aip, inodedep)
	struct allocindir *aip;
	struct inodedep *inodedep;
{
	struct freefrag *freefrag;

#ifdef DEBUG
	if (lk.lkt_held == -1)
		panic("free_allocindir: lock not held");
#endif
	if ((aip->ai_state & DEPCOMPLETE) == 0)
		LIST_REMOVE(aip, ai_deps);
	if (aip->ai_state & ONWORKLIST)
		WORKLIST_REMOVE(&aip->ai_list);
	LIST_REMOVE(aip, ai_next);
	if ((freefrag = aip->ai_freefrag) != NULL) {
		if (inodedep == NULL)
			add_to_worklist(&freefrag->ff_list);
		else
			WORKLIST_INSERT(&inodedep->id_bufwait,
			    &freefrag->ff_list);
	}
	WORKITEM_FREE(aip, D_ALLOCINDIR);
}

/*
 * Directory entry addition dependencies.
 * 
 * When adding a new directory entry, the inode (with its incremented link
 * count) must be written to disk before the directory entry's pointer to it.
 * Also, if the inode is newly allocated, the corresponding freemap must be
 * updated (on disk) before the directory entry's pointer. These requirements
 * are met via undo/redo on the directory entry's pointer, which consists
 * simply of the inode number.
 * 
 * As directory entries are added and deleted, the free space within a
 * directory block can become fragmented.  The ufs file system will compact
 * a fragmented directory block to make space for a new entry. When this
 * occurs, the offsets of previously added entries change. Any "diradd"
 * dependency structures corresponding to these entries must be updated with
 * the new offsets.
 */

/*
 * This routine is called after the in-memory inode's link
 * count has been incremented, but before the directory entry's
 * pointer to the inode has been set.
 */
void 
softdep_setup_directory_add(bp, dp, diroffset, newinum, newdirbp)
	struct buf *bp;		/* buffer containing directory block */
	struct inode *dp;	/* inode for directory */
	off_t diroffset;	/* offset of new entry in directory */
	long newinum;		/* inode referenced by new directory entry */
	struct buf *newdirbp;	/* non-NULL => contents of new mkdir */
{
	int offset;		/* offset of new entry within directory block */
	ufs_lbn_t lbn;		/* block in directory containing new entry */
	struct fs *fs;
	struct diradd *dap;
	struct pagedep *pagedep;
	struct inodedep *inodedep;
	struct mkdir *mkdir1, *mkdir2;

	/*
	 * Whiteouts have no dependencies.
	 */
	if (newinum == WINO) {
		if (newdirbp != NULL)
			bdwrite(newdirbp);
		return;
	}

	fs = dp->i_fs;
	lbn = lblkno(fs, diroffset);
	offset = blkoff(fs, diroffset);
	MALLOC(dap, struct diradd *, sizeof(struct diradd), M_DIRADD, M_WAITOK);
	bzero(dap, sizeof(struct diradd));
	dap->da_list.wk_type = D_DIRADD;
	dap->da_offset = offset;
	dap->da_newinum = newinum;
	dap->da_state = ATTACHED;
	if (newdirbp == NULL) {
		dap->da_state |= DEPCOMPLETE;
		ACQUIRE_LOCK(&lk);
	} else {
		dap->da_state |= MKDIR_BODY | MKDIR_PARENT;
		MALLOC(mkdir1, struct mkdir *, sizeof(struct mkdir), M_MKDIR,
		    M_WAITOK);
		mkdir1->md_list.wk_type = D_MKDIR;
		mkdir1->md_state = MKDIR_BODY;
		mkdir1->md_diradd = dap;
		MALLOC(mkdir2, struct mkdir *, sizeof(struct mkdir), M_MKDIR,
		    M_WAITOK);
		mkdir2->md_list.wk_type = D_MKDIR;
		mkdir2->md_state = MKDIR_PARENT;
		mkdir2->md_diradd = dap;
		/*
		 * Dependency on "." and ".." being written to disk.
		 */
		mkdir1->md_buf = newdirbp;
		ACQUIRE_LOCK(&lk);
		LIST_INSERT_HEAD(&mkdirlisthd, mkdir1, md_mkdirs);
		WORKLIST_INSERT(&newdirbp->b_dep, &mkdir1->md_list);
		FREE_LOCK(&lk);
		bdwrite(newdirbp);
		/*
		 * Dependency on link count increase for parent directory
		 */
		ACQUIRE_LOCK(&lk);
		if (inodedep_lookup(dp->i_fs, dp->i_number, 0, &inodedep) == 0
		    || (inodedep->id_state & ALLCOMPLETE) == ALLCOMPLETE) {
			dap->da_state &= ~MKDIR_PARENT;
			WORKITEM_FREE(mkdir2, D_MKDIR);
		} else {
			LIST_INSERT_HEAD(&mkdirlisthd, mkdir2, md_mkdirs);
			WORKLIST_INSERT(&inodedep->id_bufwait,&mkdir2->md_list);
		}
	}
	/*
	 * Link into parent directory pagedep to await its being written.
	 */
	if (pagedep_lookup(dp, lbn, DEPALLOC, &pagedep) == 0)
		WORKLIST_INSERT(&bp->b_dep, &pagedep->pd_list);
	dap->da_pagedep = pagedep;
	LIST_INSERT_HEAD(&pagedep->pd_diraddhd[DIRADDHASH(offset)], dap,
	    da_pdlist);
	/*
	 * Link into its inodedep. Put it on the id_bufwait list if the inode
	 * is not yet written. If it is written, do the post-inode write
	 * processing to put it on the id_pendinghd list.
	 */
	(void) inodedep_lookup(fs, newinum, DEPALLOC, &inodedep);
	if ((inodedep->id_state & ALLCOMPLETE) == ALLCOMPLETE)
		diradd_inode_written(dap, inodedep);
	else
		WORKLIST_INSERT(&inodedep->id_bufwait, &dap->da_list);
	FREE_LOCK(&lk);
}

/*
 * This procedure is called to change the offset of a directory
 * entry when compacting a directory block which must be owned
 * exclusively by the caller. Note that the actual entry movement
 * must be done in this procedure to ensure that no I/O completions
 * occur while the move is in progress.
 */
void 
softdep_change_directoryentry_offset(dp, base, oldloc, newloc, entrysize)
	struct inode *dp;	/* inode for directory */
	caddr_t base;		/* address of dp->i_offset */
	caddr_t oldloc;		/* address of old directory location */
	caddr_t newloc;		/* address of new directory location */
	int entrysize;		/* size of directory entry */
{
	int offset, oldoffset, newoffset;
	struct pagedep *pagedep;
	struct diradd *dap;
	ufs_lbn_t lbn;

	ACQUIRE_LOCK(&lk);
	lbn = lblkno(dp->i_fs, dp->i_offset);
	offset = blkoff(dp->i_fs, dp->i_offset);
	if (pagedep_lookup(dp, lbn, 0, &pagedep) == 0)
		goto done;
	oldoffset = offset + (oldloc - base);
	newoffset = offset + (newloc - base);
	for (dap = LIST_FIRST(&pagedep->pd_diraddhd[DIRADDHASH(oldoffset)]);
	     dap; dap = LIST_NEXT(dap, da_pdlist)) {
		if (dap->da_offset != oldoffset)
			continue;
		dap->da_offset = newoffset;
		if (DIRADDHASH(newoffset) == DIRADDHASH(oldoffset))
			break;
		LIST_REMOVE(dap, da_pdlist);
		LIST_INSERT_HEAD(&pagedep->pd_diraddhd[DIRADDHASH(newoffset)],
		    dap, da_pdlist);
		break;
	}
	if (dap == NULL) {
		for (dap = LIST_FIRST(&pagedep->pd_pendinghd);
		     dap; dap = LIST_NEXT(dap, da_pdlist)) {
			if (dap->da_offset == oldoffset) {
				dap->da_offset = newoffset;
				break;
			}
		}
	}
done:
	bcopy(oldloc, newloc, entrysize);
	FREE_LOCK(&lk);
}

/*
 * Free a diradd dependency structure. This routine must be called
 * with splbio interrupts blocked.
 */
static void
free_diradd(dap)
	struct diradd *dap;
{
	struct dirrem *dirrem;
	struct pagedep *pagedep;
	struct inodedep *inodedep;
	struct mkdir *mkdir, *nextmd;

#ifdef DEBUG
	if (lk.lkt_held == -1)
		panic("free_diradd: lock not held");
#endif
	WORKLIST_REMOVE(&dap->da_list);
	LIST_REMOVE(dap, da_pdlist);
	if ((dap->da_state & DIRCHG) == 0) {
		pagedep = dap->da_pagedep;
	} else {
		dirrem = dap->da_previous;
		pagedep = dirrem->dm_pagedep;
		dirrem->dm_dirinum = pagedep->pd_ino;
		add_to_worklist(&dirrem->dm_list);
	}
	if (inodedep_lookup(VFSTOUFS(pagedep->pd_mnt)->um_fs, dap->da_newinum,
	    0, &inodedep) != 0)
		(void) free_inodedep(inodedep);
	if ((dap->da_state & (MKDIR_PARENT | MKDIR_BODY)) != 0) {
		for (mkdir = LIST_FIRST(&mkdirlisthd); mkdir; mkdir = nextmd) {
			nextmd = LIST_NEXT(mkdir, md_mkdirs);
			if (mkdir->md_diradd != dap)
				continue;
			dap->da_state &= ~mkdir->md_state;
			WORKLIST_REMOVE(&mkdir->md_list);
			LIST_REMOVE(mkdir, md_mkdirs);
			WORKITEM_FREE(mkdir, D_MKDIR);
		}
		if ((dap->da_state & (MKDIR_PARENT | MKDIR_BODY)) != 0)
			panic("free_diradd: unfound ref");
	}
	WORKITEM_FREE(dap, D_DIRADD);
}

/*
 * Directory entry removal dependencies.
 * 
 * When removing a directory entry, the entry's inode pointer must be
 * zero'ed on disk before the corresponding inode's link count is decremented
 * (possibly freeing the inode for re-use). This dependency is handled by
 * updating the directory entry but delaying the inode count reduction until
 * after the directory block has been written to disk. After this point, the
 * inode count can be decremented whenever it is convenient.
 */

/*
 * This routine should be called immediately after removing
 * a directory entry.  The inode's link count should not be
 * decremented by the calling procedure -- the soft updates
 * code will do this task when it is safe.
 */
void 
softdep_setup_remove(bp, dp, ip, isrmdir)
	struct buf *bp;		/* buffer containing directory block */
	struct inode *dp;	/* inode for the directory being modified */
	struct inode *ip;	/* inode for directory entry being removed */
	int isrmdir;		/* indicates if doing RMDIR */
{
	struct dirrem *dirrem, *prevdirrem;

	/*
	 * Allocate a new dirrem if appropriate and ACQUIRE_LOCK.
	 */
	dirrem = newdirrem(bp, dp, ip, isrmdir, &prevdirrem);

	/*
	 * If the COMPLETE flag is clear, then there were no active
	 * entries and we want to roll back to a zeroed entry until
	 * the new inode is committed to disk. If the COMPLETE flag is
	 * set then we have deleted an entry that never made it to
	 * disk. If the entry we deleted resulted from a name change,
	 * then the old name still resides on disk. We cannot delete
	 * its inode (returned to us in prevdirrem) until the zeroed
	 * directory entry gets to disk. The new inode has never been
	 * referenced on the disk, so can be deleted immediately.
	 */
	if ((dirrem->dm_state & COMPLETE) == 0) {
		LIST_INSERT_HEAD(&dirrem->dm_pagedep->pd_dirremhd, dirrem,
		    dm_next);
		FREE_LOCK(&lk);
	} else {
		if (prevdirrem != NULL)
			LIST_INSERT_HEAD(&dirrem->dm_pagedep->pd_dirremhd,
			    prevdirrem, dm_next);
		dirrem->dm_dirinum = dirrem->dm_pagedep->pd_ino;
		FREE_LOCK(&lk);
		handle_workitem_remove(dirrem);
	}
}

/*
 * Allocate a new dirrem if appropriate and return it along with
 * its associated pagedep. Called without a lock, returns with lock.
 */
static long num_dirrem;		/* number of dirrem allocated */
static struct dirrem *
newdirrem(bp, dp, ip, isrmdir, prevdirremp)
	struct buf *bp;		/* buffer containing directory block */
	struct inode *dp;	/* inode for the directory being modified */
	struct inode *ip;	/* inode for directory entry being removed */
	int isrmdir;		/* indicates if doing RMDIR */
	struct dirrem **prevdirremp; /* previously referenced inode, if any */
{
	int offset;
	ufs_lbn_t lbn;
	struct diradd *dap;
	struct dirrem *dirrem;
	struct pagedep *pagedep;

	/*
	 * Whiteouts have no deletion dependencies.
	 */
	if (ip == NULL)
		panic("newdirrem: whiteout");
	/*
	 * If we are over our limit, try to improve the situation.
	 * Limiting the number of dirrem structures will also limit
	 * the number of freefile and freeblks structures.
	 */
	if (num_dirrem > max_softdeps / 2 && speedup_syncer() == 0)
		(void) request_cleanup(FLUSH_REMOVE, 0);
	num_dirrem += 1;
	MALLOC(dirrem, struct dirrem *, sizeof(struct dirrem),
		M_DIRREM, M_WAITOK);
	bzero(dirrem, sizeof(struct dirrem));
	dirrem->dm_list.wk_type = D_DIRREM;
	dirrem->dm_state = isrmdir ? RMDIR : 0;
	dirrem->dm_mnt = ITOV(ip)->v_mount;
	dirrem->dm_oldinum = ip->i_number;
	*prevdirremp = NULL;

	ACQUIRE_LOCK(&lk);
	lbn = lblkno(dp->i_fs, dp->i_offset);
	offset = blkoff(dp->i_fs, dp->i_offset);
	if (pagedep_lookup(dp, lbn, DEPALLOC, &pagedep) == 0)
		WORKLIST_INSERT(&bp->b_dep, &pagedep->pd_list);
	dirrem->dm_pagedep = pagedep;
	/*
	 * Check for a diradd dependency for the same directory entry.
	 * If present, then both dependencies become obsolete and can
	 * be de-allocated. Check for an entry on both the pd_dirraddhd
	 * list and the pd_pendinghd list.
	 */
	for (dap = LIST_FIRST(&pagedep->pd_diraddhd[DIRADDHASH(offset)]);
	     dap; dap = LIST_NEXT(dap, da_pdlist))
		if (dap->da_offset == offset)
			break;
	if (dap == NULL) {
		for (dap = LIST_FIRST(&pagedep->pd_pendinghd);
		     dap; dap = LIST_NEXT(dap, da_pdlist))
			if (dap->da_offset == offset)
				break;
		if (dap == NULL)
			return (dirrem);
	}
	/*
	 * Must be ATTACHED at this point.
	 */
	if ((dap->da_state & ATTACHED) == 0)
		panic("newdirrem: not ATTACHED");
	if (dap->da_newinum != ip->i_number)
		panic("newdirrem: inum %d should be %d",
		    ip->i_number, dap->da_newinum);
	/*
	 * If we are deleting a changed name that never made it to disk,
	 * then return the dirrem describing the previous inode (which
	 * represents the inode currently referenced from this entry on disk).
	 */
	if ((dap->da_state & DIRCHG) != 0) {
		*prevdirremp = dap->da_previous;
		dap->da_state &= ~DIRCHG;
		dap->da_pagedep = pagedep;
	}
	/*
	 * We are deleting an entry that never made it to disk.
	 * Mark it COMPLETE so we can delete its inode immediately.
	 */
	dirrem->dm_state |= COMPLETE;
	free_diradd(dap);
	return (dirrem);
}

/*
 * Directory entry change dependencies.
 * 
 * Changing an existing directory entry requires that an add operation
 * be completed first followed by a deletion. The semantics for the addition
 * are identical to the description of adding a new entry above except
 * that the rollback is to the old inode number rather than zero. Once
 * the addition dependency is completed, the removal is done as described
 * in the removal routine above.
 */

/*
 * This routine should be called immediately after changing
 * a directory entry.  The inode's link count should not be
 * decremented by the calling procedure -- the soft updates
 * code will perform this task when it is safe.
 */
void 
softdep_setup_directory_change(bp, dp, ip, newinum, isrmdir)
	struct buf *bp;		/* buffer containing directory block */
	struct inode *dp;	/* inode for the directory being modified */
	struct inode *ip;	/* inode for directory entry being removed */
	long newinum;		/* new inode number for changed entry */
	int isrmdir;		/* indicates if doing RMDIR */
{
	int offset;
	struct diradd *dap = NULL;
	struct dirrem *dirrem, *prevdirrem;
	struct pagedep *pagedep;
	struct inodedep *inodedep;

	offset = blkoff(dp->i_fs, dp->i_offset);

	/*
	 * Whiteouts do not need diradd dependencies.
	 */
	if (newinum != WINO) {
		MALLOC(dap, struct diradd *, sizeof(struct diradd),
		    M_DIRADD, M_WAITOK);
		bzero(dap, sizeof(struct diradd));
		dap->da_list.wk_type = D_DIRADD;
		dap->da_state = DIRCHG | ATTACHED | DEPCOMPLETE;
		dap->da_offset = offset;
		dap->da_newinum = newinum;
	}

	/*
	 * Allocate a new dirrem and ACQUIRE_LOCK.
	 */
	dirrem = newdirrem(bp, dp, ip, isrmdir, &prevdirrem);
	pagedep = dirrem->dm_pagedep;
	/*
	 * The possible values for isrmdir:
	 *	0 - non-directory file rename
	 *	1 - directory rename within same directory
	 *   inum - directory rename to new directory of given inode number
	 * When renaming to a new directory, we are both deleting and
	 * creating a new directory entry, so the link count on the new
	 * directory should not change. Thus we do not need the followup
	 * dirrem which is usually done in handle_workitem_remove. We set
	 * the DIRCHG flag to tell handle_workitem_remove to skip the 
	 * followup dirrem.
	 */
	if (isrmdir > 1)
		dirrem->dm_state |= DIRCHG;

	/*
	 * Whiteouts have no additional dependencies,
	 * so just put the dirrem on the correct list.
	 */
	if (newinum == WINO) {
		if ((dirrem->dm_state & COMPLETE) == 0) {
			LIST_INSERT_HEAD(&pagedep->pd_dirremhd, dirrem,
			    dm_next);
		} else {
			dirrem->dm_dirinum = pagedep->pd_ino;
			add_to_worklist(&dirrem->dm_list);
		}
		FREE_LOCK(&lk);
		return;
	}

	/*
	 * If the COMPLETE flag is clear, then there were no active
	 * entries and we want to roll back to the previous inode until
	 * the new inode is committed to disk. If the COMPLETE flag is
	 * set, then we have deleted an entry that never made it to disk.
	 * If the entry we deleted resulted from a name change, then the old
	 * inode reference still resides on disk. Any rollback that we do
	 * needs to be to that old inode (returned to us in prevdirrem). If
	 * the entry we deleted resulted from a create, then there is
	 * no entry on the disk, so we want to roll back to zero rather
	 * than the uncommitted inode. In either of the COMPLETE cases we
	 * want to immediately free the unwritten and unreferenced inode.
	 */
	if ((dirrem->dm_state & COMPLETE) == 0) {
		dap->da_previous = dirrem;
	} else {
		if (prevdirrem != NULL) {
			dap->da_previous = prevdirrem;
		} else {
			dap->da_state &= ~DIRCHG;
			dap->da_pagedep = pagedep;
		}
		dirrem->dm_dirinum = pagedep->pd_ino;
		add_to_worklist(&dirrem->dm_list);
	}
	/*
	 * Link into its inodedep. Put it on the id_bufwait list if the inode
	 * is not yet written. If it is written, do the post-inode write
	 * processing to put it on the id_pendinghd list.
	 */
	if (inodedep_lookup(dp->i_fs, newinum, DEPALLOC, &inodedep) == 0 ||
	    (inodedep->id_state & ALLCOMPLETE) == ALLCOMPLETE) {
		dap->da_state |= COMPLETE;
		LIST_INSERT_HEAD(&pagedep->pd_pendinghd, dap, da_pdlist);
		WORKLIST_INSERT(&inodedep->id_pendinghd, &dap->da_list);
	} else {
		LIST_INSERT_HEAD(&pagedep->pd_diraddhd[DIRADDHASH(offset)],
		    dap, da_pdlist);
		WORKLIST_INSERT(&inodedep->id_bufwait, &dap->da_list);
	}
	FREE_LOCK(&lk);
}

/*
 * Called whenever the link count on an inode is changed.
 * It creates an inode dependency so that the new reference(s)
 * to the inode cannot be committed to disk until the updated
 * inode has been written.
 */
void
softdep_change_linkcnt(ip)
	struct inode *ip;	/* the inode with the increased link count */
{
	struct inodedep *inodedep;

	ACQUIRE_LOCK(&lk);
	(void) inodedep_lookup(ip->i_fs, ip->i_number, DEPALLOC, &inodedep);
	if (ip->i_nlink < ip->i_effnlink)
		panic("softdep_change_linkcnt: bad delta");
	inodedep->id_nlinkdelta = ip->i_nlink - ip->i_effnlink;
	FREE_LOCK(&lk);
}

/*
 * This workitem decrements the inode's link count.
 * If the link count reaches zero, the file is removed.
 */
static void 
handle_workitem_remove(dirrem)
	struct dirrem *dirrem;
{
	struct proc *p = CURPROC;	/* XXX */
	struct inodedep *inodedep;
	struct vnode *vp;
	struct inode *ip;
	ino_t oldinum;
	int error;

	if ((error = VFS_VGET(dirrem->dm_mnt, dirrem->dm_oldinum, &vp)) != 0) {
		softdep_error("handle_workitem_remove: vget", error);
		return;
	}
	ip = VTOI(vp);
	ACQUIRE_LOCK(&lk);
	if ((inodedep_lookup(ip->i_fs, dirrem->dm_oldinum, 0, &inodedep)) == 0)
		panic("handle_workitem_remove: lost inodedep");
	/*
	 * Normal file deletion.
	 */
	if ((dirrem->dm_state & RMDIR) == 0) {
		ip->i_nlink--;
		ip->i_flag |= IN_CHANGE;
		if (ip->i_nlink < ip->i_effnlink)
			panic("handle_workitem_remove: bad file delta");
		inodedep->id_nlinkdelta = ip->i_nlink - ip->i_effnlink;
		FREE_LOCK(&lk);
		vput(vp);
		num_dirrem -= 1;
		WORKITEM_FREE(dirrem, D_DIRREM);
		return;
	}
	/*
	 * Directory deletion. Decrement reference count for both the
	 * just deleted parent directory entry and the reference for ".".
	 * Next truncate the directory to length zero. When the
	 * truncation completes, arrange to have the reference count on
	 * the parent decremented to account for the loss of "..".
	 */
	ip->i_nlink -= 2;
	ip->i_flag |= IN_CHANGE;
	if (ip->i_nlink < ip->i_effnlink)
		panic("handle_workitem_remove: bad dir delta");
	inodedep->id_nlinkdelta = ip->i_nlink - ip->i_effnlink;
	FREE_LOCK(&lk);
	if ((error = UFS_TRUNCATE(vp, (off_t)0, 0, p->p_ucred, p)) != 0)
		softdep_error("handle_workitem_remove: truncate", error);
	/*
	 * Rename a directory to a new parent. Since, we are both deleting
	 * and creating a new directory entry, the link count on the new
	 * directory should not change. Thus we skip the followup dirrem.
	 */
	if (dirrem->dm_state & DIRCHG) {
		vput(vp);
		num_dirrem -= 1;
		WORKITEM_FREE(dirrem, D_DIRREM);
		return;
	}
	/*
	 * If the inodedep does not exist, then the zero'ed inode has
	 * been written to disk. If the allocated inode has never been
	 * written to disk, then the on-disk inode is zero'ed. In either
	 * case we can remove the file immediately.
	 */
	ACQUIRE_LOCK(&lk);
	dirrem->dm_state = 0;
	oldinum = dirrem->dm_oldinum;
	dirrem->dm_oldinum = dirrem->dm_dirinum;
	if (inodedep_lookup(ip->i_fs, oldinum, 0, &inodedep) == 0 ||
	    check_inode_unwritten(inodedep)) {
		FREE_LOCK(&lk);
		vput(vp);
		handle_workitem_remove(dirrem);
		return;
	}
	WORKLIST_INSERT(&inodedep->id_inowait, &dirrem->dm_list);
	FREE_LOCK(&lk);
	vput(vp);
}

/*
 * Inode de-allocation dependencies.
 * 
 * When an inode's link count is reduced to zero, it can be de-allocated. We
 * found it convenient to postpone de-allocation until after the inode is
 * written to disk with its new link count (zero).  At this point, all of the
 * on-disk inode's block pointers are nullified and, with careful dependency
 * list ordering, all dependencies related to the inode will be satisfied and
 * the corresponding dependency structures de-allocated.  So, if/when the
 * inode is reused, there will be no mixing of old dependencies with new
 * ones.  This artificial dependency is set up by the block de-allocation
 * procedure above (softdep_setup_freeblocks) and completed by the
 * following procedure.
 */
static void 
handle_workitem_freefile(freefile)
	struct freefile *freefile;
{
	struct fs *fs;
	struct vnode vp;
	struct inode tip;
	struct inodedep *idp;
	int error;

	fs = VFSTOUFS(freefile->fx_mnt)->um_fs;
#ifdef DEBUG
	ACQUIRE_LOCK(&lk);
	if (inodedep_lookup(fs, freefile->fx_oldinum, 0, &idp))
		panic("handle_workitem_freefile: inodedep survived");
	FREE_LOCK(&lk);
#endif
	tip.i_devvp = freefile->fx_devvp;
	tip.i_dev = freefile->fx_devvp->v_rdev;
	tip.i_fs = fs;
	tip.i_vnode = &vp;
	vp.v_data = &tip;
	if ((error = ffs_freefile(&vp, freefile->fx_oldinum, freefile->fx_mode)) != 0)
		softdep_error("handle_workitem_freefile", error);
	WORKITEM_FREE(freefile, D_FREEFILE);
}

/*
 * Disk writes.
 * 
 * The dependency structures constructed above are most actively used when file
 * system blocks are written to disk.  No constraints are placed on when a
 * block can be written, but unsatisfied update dependencies are made safe by
 * modifying (or replacing) the source memory for the duration of the disk
 * write.  When the disk write completes, the memory block is again brought
 * up-to-date.
 *
 * In-core inode structure reclamation.
 * 
 * Because there are a finite number of "in-core" inode structures, they are
 * reused regularly.  By transferring all inode-related dependencies to the
 * in-memory inode block and indexing them separately (via "inodedep"s), we
 * can allow "in-core" inode structures to be reused at any time and avoid
 * any increase in contention.
 *
 * Called just before entering the device driver to initiate a new disk I/O.
 * The buffer must be locked, thus, no I/O completion operations can occur
 * while we are manipulating its associated dependencies.
 */
static void 
softdep_disk_io_initiation(bp)
	struct buf *bp;		/* structure describing disk write to occur */
{
	struct worklist *wk, *nextwk;
	struct indirdep *indirdep;

	/*
	 * We only care about write operations. There should never
	 * be dependencies for reads.
	 */
	if (bp->b_iocmd == BIO_READ)
		panic("softdep_disk_io_initiation: read");
	/*
	 * Do any necessary pre-I/O processing.
	 */
	for (wk = LIST_FIRST(&bp->b_dep); wk; wk = nextwk) {
		nextwk = LIST_NEXT(wk, wk_list);
		switch (wk->wk_type) {

		case D_PAGEDEP:
			initiate_write_filepage(WK_PAGEDEP(wk), bp);
			continue;

		case D_INODEDEP:
			initiate_write_inodeblock(WK_INODEDEP(wk), bp);
			continue;

		case D_INDIRDEP:
			indirdep = WK_INDIRDEP(wk);
			if (indirdep->ir_state & GOINGAWAY)
				panic("disk_io_initiation: indirdep gone");
			/*
			 * If there are no remaining dependencies, this
			 * will be writing the real pointers, so the
			 * dependency can be freed.
			 */
			if (LIST_FIRST(&indirdep->ir_deplisthd) == NULL) {
				indirdep->ir_savebp->b_flags |= B_INVAL | B_NOCACHE;
				brelse(indirdep->ir_savebp);
				/* inline expand WORKLIST_REMOVE(wk); */
				wk->wk_state &= ~ONWORKLIST;
				LIST_REMOVE(wk, wk_list);
				WORKITEM_FREE(indirdep, D_INDIRDEP);
				continue;
			}
			/*
			 * Replace up-to-date version with safe version.
			 */
			ACQUIRE_LOCK(&lk);
			indirdep->ir_state &= ~ATTACHED;
			indirdep->ir_state |= UNDONE;
			MALLOC(indirdep->ir_saveddata, caddr_t, bp->b_bcount,
			    M_INDIRDEP, M_WAITOK);
			bcopy(bp->b_data, indirdep->ir_saveddata, bp->b_bcount);
			bcopy(indirdep->ir_savebp->b_data, bp->b_data,
			    bp->b_bcount);
			FREE_LOCK(&lk);
			continue;

		case D_MKDIR:
		case D_BMSAFEMAP:
		case D_ALLOCDIRECT:
		case D_ALLOCINDIR:
			continue;

		default:
			panic("handle_disk_io_initiation: Unexpected type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
}

/*
 * Called from within the procedure above to deal with unsatisfied
 * allocation dependencies in a directory. The buffer must be locked,
 * thus, no I/O completion operations can occur while we are
 * manipulating its associated dependencies.
 */
static void
initiate_write_filepage(pagedep, bp)
	struct pagedep *pagedep;
	struct buf *bp;
{
	struct diradd *dap;
	struct direct *ep;
	int i;

	if (pagedep->pd_state & IOSTARTED) {
		/*
		 * This can only happen if there is a driver that does not
		 * understand chaining. Here biodone will reissue the call
		 * to strategy for the incomplete buffers.
		 */
		printf("initiate_write_filepage: already started\n");
		return;
	}
	pagedep->pd_state |= IOSTARTED;
	ACQUIRE_LOCK(&lk);
	for (i = 0; i < DAHASHSZ; i++) {
		for (dap = LIST_FIRST(&pagedep->pd_diraddhd[i]); dap;
		     dap = LIST_NEXT(dap, da_pdlist)) {
			ep = (struct direct *)
			    ((char *)bp->b_data + dap->da_offset);
			if (ep->d_ino != dap->da_newinum)
				panic("%s: dir inum %d != new %d",
				    "initiate_write_filepage",
				    ep->d_ino, dap->da_newinum);
			if (dap->da_state & DIRCHG)
				ep->d_ino = dap->da_previous->dm_oldinum;
			else
				ep->d_ino = 0;
			dap->da_state &= ~ATTACHED;
			dap->da_state |= UNDONE;
		}
	}
	FREE_LOCK(&lk);
}

/*
 * Called from within the procedure above to deal with unsatisfied
 * allocation dependencies in an inodeblock. The buffer must be
 * locked, thus, no I/O completion operations can occur while we
 * are manipulating its associated dependencies.
 */
static void 
initiate_write_inodeblock(inodedep, bp)
	struct inodedep *inodedep;
	struct buf *bp;			/* The inode block */
{
	struct allocdirect *adp, *lastadp;
	struct dinode *dp;
	struct fs *fs;
	ufs_lbn_t prevlbn = 0;
	int i, deplist;

	if (inodedep->id_state & IOSTARTED)
		panic("initiate_write_inodeblock: already started");
	inodedep->id_state |= IOSTARTED;
	fs = inodedep->id_fs;
	dp = (struct dinode *)bp->b_data +
	    ino_to_fsbo(fs, inodedep->id_ino);
	/*
	 * If the bitmap is not yet written, then the allocated
	 * inode cannot be written to disk.
	 */
	if ((inodedep->id_state & DEPCOMPLETE) == 0) {
		if (inodedep->id_savedino != NULL)
			panic("initiate_write_inodeblock: already doing I/O");
		MALLOC(inodedep->id_savedino, struct dinode *,
		    sizeof(struct dinode), M_INODEDEP, M_WAITOK);
		*inodedep->id_savedino = *dp;
		bzero((caddr_t)dp, sizeof(struct dinode));
		return;
	}
	/*
	 * If no dependencies, then there is nothing to roll back.
	 */
	inodedep->id_savedsize = dp->di_size;
	if (TAILQ_FIRST(&inodedep->id_inoupdt) == NULL)
		return;
	/*
	 * Set the dependencies to busy.
	 */
	ACQUIRE_LOCK(&lk);
	for (deplist = 0, adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp;
	     adp = TAILQ_NEXT(adp, ad_next)) {
#ifdef DIAGNOSTIC
		if (deplist != 0 && prevlbn >= adp->ad_lbn)
			panic("softdep_write_inodeblock: lbn order");
		prevlbn = adp->ad_lbn;
		if (adp->ad_lbn < NDADDR &&
		    dp->di_db[adp->ad_lbn] != adp->ad_newblkno)
			panic("%s: direct pointer #%ld mismatch %d != %d",
			    "softdep_write_inodeblock", adp->ad_lbn,
			    dp->di_db[adp->ad_lbn], adp->ad_newblkno);
		if (adp->ad_lbn >= NDADDR &&
		    dp->di_ib[adp->ad_lbn - NDADDR] != adp->ad_newblkno)
			panic("%s: indirect pointer #%ld mismatch %d != %d",
			    "softdep_write_inodeblock", adp->ad_lbn - NDADDR,
			    dp->di_ib[adp->ad_lbn - NDADDR], adp->ad_newblkno);
		deplist |= 1 << adp->ad_lbn;
		if ((adp->ad_state & ATTACHED) == 0)
			panic("softdep_write_inodeblock: Unknown state 0x%x",
			    adp->ad_state);
#endif /* DIAGNOSTIC */
		adp->ad_state &= ~ATTACHED;
		adp->ad_state |= UNDONE;
	}
	/*
	 * The on-disk inode cannot claim to be any larger than the last
	 * fragment that has been written. Otherwise, the on-disk inode
	 * might have fragments that were not the last block in the file
	 * which would corrupt the filesystem.
	 */
	for (lastadp = NULL, adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp;
	     lastadp = adp, adp = TAILQ_NEXT(adp, ad_next)) {
		if (adp->ad_lbn >= NDADDR)
			break;
		dp->di_db[adp->ad_lbn] = adp->ad_oldblkno;
		/* keep going until hitting a rollback to a frag */
		if (adp->ad_oldsize == 0 || adp->ad_oldsize == fs->fs_bsize)
			continue;
		dp->di_size = fs->fs_bsize * adp->ad_lbn + adp->ad_oldsize;
		for (i = adp->ad_lbn + 1; i < NDADDR; i++) {
#ifdef DIAGNOSTIC
			if (dp->di_db[i] != 0 && (deplist & (1 << i)) == 0)
				panic("softdep_write_inodeblock: lost dep1");
#endif /* DIAGNOSTIC */
			dp->di_db[i] = 0;
		}
		for (i = 0; i < NIADDR; i++) {
#ifdef DIAGNOSTIC
			if (dp->di_ib[i] != 0 &&
			    (deplist & ((1 << NDADDR) << i)) == 0)
				panic("softdep_write_inodeblock: lost dep2");
#endif /* DIAGNOSTIC */
			dp->di_ib[i] = 0;
		}
		FREE_LOCK(&lk);
		return;
	}
	/*
	 * If we have zero'ed out the last allocated block of the file,
	 * roll back the size to the last currently allocated block.
	 * We know that this last allocated block is a full-sized as
	 * we already checked for fragments in the loop above.
	 */
	if (lastadp != NULL &&
	    dp->di_size <= (lastadp->ad_lbn + 1) * fs->fs_bsize) {
		for (i = lastadp->ad_lbn; i >= 0; i--)
			if (dp->di_db[i] != 0)
				break;
		dp->di_size = (i + 1) * fs->fs_bsize;
	}
	/*
	 * The only dependencies are for indirect blocks.
	 *
	 * The file size for indirect block additions is not guaranteed.
	 * Such a guarantee would be non-trivial to achieve. The conventional
	 * synchronous write implementation also does not make this guarantee.
	 * Fsck should catch and fix discrepancies. Arguably, the file size
	 * can be over-estimated without destroying integrity when the file
	 * moves into the indirect blocks (i.e., is large). If we want to
	 * postpone fsck, we are stuck with this argument.
	 */
	for (; adp; adp = TAILQ_NEXT(adp, ad_next))
		dp->di_ib[adp->ad_lbn - NDADDR] = 0;
	FREE_LOCK(&lk);
}

/*
 * This routine is called during the completion interrupt
 * service routine for a disk write (from the procedure called
 * by the device driver to inform the file system caches of
 * a request completion).  It should be called early in this
 * procedure, before the block is made available to other
 * processes or other routines are called.
 */
static void 
softdep_disk_write_complete(bp)
	struct buf *bp;		/* describes the completed disk write */
{
	struct worklist *wk;
	struct workhead reattach;
	struct newblk *newblk;
	struct allocindir *aip;
	struct allocdirect *adp;
	struct indirdep *indirdep;
	struct inodedep *inodedep;
	struct bmsafemap *bmsafemap;

#ifdef DEBUG
	if (lk.lkt_held != -1)
		panic("softdep_disk_write_complete: lock is held");
	lk.lkt_held = -2;
#endif
	LIST_INIT(&reattach);
	while ((wk = LIST_FIRST(&bp->b_dep)) != NULL) {
		WORKLIST_REMOVE(wk);
		switch (wk->wk_type) {

		case D_PAGEDEP:
			if (handle_written_filepage(WK_PAGEDEP(wk), bp))
				WORKLIST_INSERT(&reattach, wk);
			continue;

		case D_INODEDEP:
			if (handle_written_inodeblock(WK_INODEDEP(wk), bp))
				WORKLIST_INSERT(&reattach, wk);
			continue;

		case D_BMSAFEMAP:
			bmsafemap = WK_BMSAFEMAP(wk);
			while ((newblk = LIST_FIRST(&bmsafemap->sm_newblkhd))) {
				newblk->nb_state |= DEPCOMPLETE;
				newblk->nb_bmsafemap = NULL;
				LIST_REMOVE(newblk, nb_deps);
			}
			while ((adp =
			   LIST_FIRST(&bmsafemap->sm_allocdirecthd))) {
				adp->ad_state |= DEPCOMPLETE;
				adp->ad_buf = NULL;
				LIST_REMOVE(adp, ad_deps);
				handle_allocdirect_partdone(adp);
			}
			while ((aip =
			    LIST_FIRST(&bmsafemap->sm_allocindirhd))) {
				aip->ai_state |= DEPCOMPLETE;
				aip->ai_buf = NULL;
				LIST_REMOVE(aip, ai_deps);
				handle_allocindir_partdone(aip);
			}
			while ((inodedep =
			     LIST_FIRST(&bmsafemap->sm_inodedephd)) != NULL) {
				inodedep->id_state |= DEPCOMPLETE;
				LIST_REMOVE(inodedep, id_deps);
				inodedep->id_buf = NULL;
			}
			WORKITEM_FREE(bmsafemap, D_BMSAFEMAP);
			continue;

		case D_MKDIR:
			handle_written_mkdir(WK_MKDIR(wk), MKDIR_BODY);
			continue;

		case D_ALLOCDIRECT:
			adp = WK_ALLOCDIRECT(wk);
			adp->ad_state |= COMPLETE;
			handle_allocdirect_partdone(adp);
			continue;

		case D_ALLOCINDIR:
			aip = WK_ALLOCINDIR(wk);
			aip->ai_state |= COMPLETE;
			handle_allocindir_partdone(aip);
			continue;

		case D_INDIRDEP:
			indirdep = WK_INDIRDEP(wk);
			if (indirdep->ir_state & GOINGAWAY)
				panic("disk_write_complete: indirdep gone");
			bcopy(indirdep->ir_saveddata, bp->b_data, bp->b_bcount);
			FREE(indirdep->ir_saveddata, M_INDIRDEP);
			indirdep->ir_saveddata = 0;
			indirdep->ir_state &= ~UNDONE;
			indirdep->ir_state |= ATTACHED;
			while ((aip = LIST_FIRST(&indirdep->ir_donehd)) != 0) {
				handle_allocindir_partdone(aip);
				if (aip == LIST_FIRST(&indirdep->ir_donehd))
					panic("disk_write_complete: not gone");
			}
			WORKLIST_INSERT(&reattach, wk);
			if ((bp->b_flags & B_DELWRI) == 0)
				stat_indir_blk_ptrs++;
			bdirty(bp);
			continue;

		default:
			panic("handle_disk_write_complete: Unknown type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
	/*
	 * Reattach any requests that must be redone.
	 */
	while ((wk = LIST_FIRST(&reattach)) != NULL) {
		WORKLIST_REMOVE(wk);
		WORKLIST_INSERT(&bp->b_dep, wk);
	}
#ifdef DEBUG
	if (lk.lkt_held != -2)
		panic("softdep_disk_write_complete: lock lost");
	lk.lkt_held = -1;
#endif
}

/*
 * Called from within softdep_disk_write_complete above. Note that
 * this routine is always called from interrupt level with further
 * splbio interrupts blocked.
 */
static void 
handle_allocdirect_partdone(adp)
	struct allocdirect *adp;	/* the completed allocdirect */
{
	struct allocdirect *listadp;
	struct inodedep *inodedep;
	long bsize, delay;

	if ((adp->ad_state & ALLCOMPLETE) != ALLCOMPLETE)
		return;
	if (adp->ad_buf != NULL)
		panic("handle_allocdirect_partdone: dangling dep");
	/*
	 * The on-disk inode cannot claim to be any larger than the last
	 * fragment that has been written. Otherwise, the on-disk inode
	 * might have fragments that were not the last block in the file
	 * which would corrupt the filesystem. Thus, we cannot free any
	 * allocdirects after one whose ad_oldblkno claims a fragment as
	 * these blocks must be rolled back to zero before writing the inode.
	 * We check the currently active set of allocdirects in id_inoupdt.
	 */
	inodedep = adp->ad_inodedep;
	bsize = inodedep->id_fs->fs_bsize;
	for (listadp = TAILQ_FIRST(&inodedep->id_inoupdt); listadp;
	     listadp = TAILQ_NEXT(listadp, ad_next)) {
		/* found our block */
		if (listadp == adp)
			break;
		/* continue if ad_oldlbn is not a fragment */
		if (listadp->ad_oldsize == 0 ||
		    listadp->ad_oldsize == bsize)
			continue;
		/* hit a fragment */
		return;
	}
	/*
	 * If we have reached the end of the current list without
	 * finding the just finished dependency, then it must be
	 * on the future dependency list. Future dependencies cannot
	 * be freed until they are moved to the current list.
	 */
	if (listadp == NULL) {
#ifdef DEBUG
		for (listadp = TAILQ_FIRST(&inodedep->id_newinoupdt); listadp;
		     listadp = TAILQ_NEXT(listadp, ad_next))
			/* found our block */
			if (listadp == adp)
				break;
		if (listadp == NULL)
			panic("handle_allocdirect_partdone: lost dep");
#endif /* DEBUG */
		return;
	}
	/*
	 * If we have found the just finished dependency, then free
	 * it along with anything that follows it that is complete.
	 * If the inode still has a bitmap dependency, then it has
	 * never been written to disk, hence the on-disk inode cannot
	 * reference the old fragment so we can free it without delay.
	 */
	delay = (inodedep->id_state & DEPCOMPLETE);
	for (; adp; adp = listadp) {
		listadp = TAILQ_NEXT(adp, ad_next);
		if ((adp->ad_state & ALLCOMPLETE) != ALLCOMPLETE)
			return;
		free_allocdirect(&inodedep->id_inoupdt, adp, delay);
	}
}

/*
 * Called from within softdep_disk_write_complete above. Note that
 * this routine is always called from interrupt level with further
 * splbio interrupts blocked.
 */
static void
handle_allocindir_partdone(aip)
	struct allocindir *aip;		/* the completed allocindir */
{
	struct indirdep *indirdep;

	if ((aip->ai_state & ALLCOMPLETE) != ALLCOMPLETE)
		return;
	if (aip->ai_buf != NULL)
		panic("handle_allocindir_partdone: dangling dependency");
	indirdep = aip->ai_indirdep;
	if (indirdep->ir_state & UNDONE) {
		LIST_REMOVE(aip, ai_next);
		LIST_INSERT_HEAD(&indirdep->ir_donehd, aip, ai_next);
		return;
	}
	((ufs_daddr_t *)indirdep->ir_savebp->b_data)[aip->ai_offset] =
	    aip->ai_newblkno;
	LIST_REMOVE(aip, ai_next);
	if (aip->ai_freefrag != NULL)
		add_to_worklist(&aip->ai_freefrag->ff_list);
	WORKITEM_FREE(aip, D_ALLOCINDIR);
}

/*
 * Called from within softdep_disk_write_complete above to restore
 * in-memory inode block contents to their most up-to-date state. Note
 * that this routine is always called from interrupt level with further
 * splbio interrupts blocked.
 */
static int 
handle_written_inodeblock(inodedep, bp)
	struct inodedep *inodedep;
	struct buf *bp;		/* buffer containing the inode block */
{
	struct worklist *wk, *filefree;
	struct allocdirect *adp, *nextadp;
	struct dinode *dp;
	int hadchanges;

	if ((inodedep->id_state & IOSTARTED) == 0)
		panic("handle_written_inodeblock: not started");
	inodedep->id_state &= ~IOSTARTED;
	inodedep->id_state |= COMPLETE;
	dp = (struct dinode *)bp->b_data +
	    ino_to_fsbo(inodedep->id_fs, inodedep->id_ino);
	/*
	 * If we had to rollback the inode allocation because of
	 * bitmaps being incomplete, then simply restore it.
	 * Keep the block dirty so that it will not be reclaimed until
	 * all associated dependencies have been cleared and the
	 * corresponding updates written to disk.
	 */
	if (inodedep->id_savedino != NULL) {
		*dp = *inodedep->id_savedino;
		FREE(inodedep->id_savedino, M_INODEDEP);
		inodedep->id_savedino = NULL;
		if ((bp->b_flags & B_DELWRI) == 0)
			stat_inode_bitmap++;
		bdirty(bp);
		return (1);
	}
	/*
	 * Roll forward anything that had to be rolled back before 
	 * the inode could be updated.
	 */
	hadchanges = 0;
	for (adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp; adp = nextadp) {
		nextadp = TAILQ_NEXT(adp, ad_next);
		if (adp->ad_state & ATTACHED)
			panic("handle_written_inodeblock: new entry");
		if (adp->ad_lbn < NDADDR) {
			if (dp->di_db[adp->ad_lbn] != adp->ad_oldblkno)
				panic("%s: %s #%ld mismatch %d != %d",
				    "handle_written_inodeblock",
				    "direct pointer", adp->ad_lbn,
				    dp->di_db[adp->ad_lbn], adp->ad_oldblkno);
			dp->di_db[adp->ad_lbn] = adp->ad_newblkno;
		} else {
			if (dp->di_ib[adp->ad_lbn - NDADDR] != 0)
				panic("%s: %s #%ld allocated as %d",
				    "handle_written_inodeblock",
				    "indirect pointer", adp->ad_lbn - NDADDR,
				    dp->di_ib[adp->ad_lbn - NDADDR]);
			dp->di_ib[adp->ad_lbn - NDADDR] = adp->ad_newblkno;
		}
		adp->ad_state &= ~UNDONE;
		adp->ad_state |= ATTACHED;
		hadchanges = 1;
	}
	if (hadchanges && (bp->b_flags & B_DELWRI) == 0)
		stat_direct_blk_ptrs++;
	/*
	 * Reset the file size to its most up-to-date value.
	 */
	if (inodedep->id_savedsize == -1)
		panic("handle_written_inodeblock: bad size");
	if (dp->di_size != inodedep->id_savedsize) {
		dp->di_size = inodedep->id_savedsize;
		hadchanges = 1;
	}
	inodedep->id_savedsize = -1;
	/*
	 * If there were any rollbacks in the inode block, then it must be
	 * marked dirty so that its will eventually get written back in
	 * its correct form.
	 */
	if (hadchanges)
		bdirty(bp);
	/*
	 * Process any allocdirects that completed during the update.
	 */
	if ((adp = TAILQ_FIRST(&inodedep->id_inoupdt)) != NULL)
		handle_allocdirect_partdone(adp);
	/*
	 * Process deallocations that were held pending until the
	 * inode had been written to disk. Freeing of the inode
	 * is delayed until after all blocks have been freed to
	 * avoid creation of new <vfsid, inum, lbn> triples
	 * before the old ones have been deleted.
	 */
	filefree = NULL;
	while ((wk = LIST_FIRST(&inodedep->id_bufwait)) != NULL) {
		WORKLIST_REMOVE(wk);
		switch (wk->wk_type) {

		case D_FREEFILE:
			/*
			 * We defer adding filefree to the worklist until
			 * all other additions have been made to ensure
			 * that it will be done after all the old blocks
			 * have been freed.
			 */
			if (filefree != NULL)
				panic("handle_written_inodeblock: filefree");
			filefree = wk;
			continue;

		case D_MKDIR:
			handle_written_mkdir(WK_MKDIR(wk), MKDIR_PARENT);
			continue;

		case D_DIRADD:
			diradd_inode_written(WK_DIRADD(wk), inodedep);
			continue;

		case D_FREEBLKS:
		case D_FREEFRAG:
		case D_DIRREM:
			add_to_worklist(wk);
			continue;

		default:
			panic("handle_written_inodeblock: Unknown type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
	if (filefree != NULL) {
		if (free_inodedep(inodedep) == 0)
			panic("handle_written_inodeblock: live inodedep");
		add_to_worklist(filefree);
		return (0);
	}

	/*
	 * If no outstanding dependencies, free it.
	 */
	if (free_inodedep(inodedep) || TAILQ_FIRST(&inodedep->id_inoupdt) == 0)
		return (0);
	return (hadchanges);
}

/*
 * Process a diradd entry after its dependent inode has been written.
 * This routine must be called with splbio interrupts blocked.
 */
static void
diradd_inode_written(dap, inodedep)
	struct diradd *dap;
	struct inodedep *inodedep;
{
	struct pagedep *pagedep;

	dap->da_state |= COMPLETE;
	if ((dap->da_state & ALLCOMPLETE) == ALLCOMPLETE) {
		if (dap->da_state & DIRCHG)
			pagedep = dap->da_previous->dm_pagedep;
		else
			pagedep = dap->da_pagedep;
		LIST_REMOVE(dap, da_pdlist);
		LIST_INSERT_HEAD(&pagedep->pd_pendinghd, dap, da_pdlist);
	}
	WORKLIST_INSERT(&inodedep->id_pendinghd, &dap->da_list);
}

/*
 * Handle the completion of a mkdir dependency.
 */
static void
handle_written_mkdir(mkdir, type)
	struct mkdir *mkdir;
	int type;
{
	struct diradd *dap;
	struct pagedep *pagedep;

	if (mkdir->md_state != type)
		panic("handle_written_mkdir: bad type");
	dap = mkdir->md_diradd;
	dap->da_state &= ~type;
	if ((dap->da_state & (MKDIR_PARENT | MKDIR_BODY)) == 0)
		dap->da_state |= DEPCOMPLETE;
	if ((dap->da_state & ALLCOMPLETE) == ALLCOMPLETE) {
		if (dap->da_state & DIRCHG)
			pagedep = dap->da_previous->dm_pagedep;
		else
			pagedep = dap->da_pagedep;
		LIST_REMOVE(dap, da_pdlist);
		LIST_INSERT_HEAD(&pagedep->pd_pendinghd, dap, da_pdlist);
	}
	LIST_REMOVE(mkdir, md_mkdirs);
	WORKITEM_FREE(mkdir, D_MKDIR);
}

/*
 * Called from within softdep_disk_write_complete above.
 * A write operation was just completed. Removed inodes can
 * now be freed and associated block pointers may be committed.
 * Note that this routine is always called from interrupt level
 * with further splbio interrupts blocked.
 */
static int 
handle_written_filepage(pagedep, bp)
	struct pagedep *pagedep;
	struct buf *bp;		/* buffer containing the written page */
{
	struct dirrem *dirrem;
	struct diradd *dap, *nextdap;
	struct direct *ep;
	int i, chgs;

	if ((pagedep->pd_state & IOSTARTED) == 0)
		panic("handle_written_filepage: not started");
	pagedep->pd_state &= ~IOSTARTED;
	/*
	 * Process any directory removals that have been committed.
	 */
	while ((dirrem = LIST_FIRST(&pagedep->pd_dirremhd)) != NULL) {
		LIST_REMOVE(dirrem, dm_next);
		dirrem->dm_dirinum = pagedep->pd_ino;
		add_to_worklist(&dirrem->dm_list);
	}
	/*
	 * Free any directory additions that have been committed.
	 */
	while ((dap = LIST_FIRST(&pagedep->pd_pendinghd)) != NULL)
		free_diradd(dap);
	/*
	 * Uncommitted directory entries must be restored.
	 */
	for (chgs = 0, i = 0; i < DAHASHSZ; i++) {
		for (dap = LIST_FIRST(&pagedep->pd_diraddhd[i]); dap;
		     dap = nextdap) {
			nextdap = LIST_NEXT(dap, da_pdlist);
			if (dap->da_state & ATTACHED)
				panic("handle_written_filepage: attached");
			ep = (struct direct *)
			    ((char *)bp->b_data + dap->da_offset);
			ep->d_ino = dap->da_newinum;
			dap->da_state &= ~UNDONE;
			dap->da_state |= ATTACHED;
			chgs = 1;
			/*
			 * If the inode referenced by the directory has
			 * been written out, then the dependency can be
			 * moved to the pending list.
			 */
			if ((dap->da_state & ALLCOMPLETE) == ALLCOMPLETE) {
				LIST_REMOVE(dap, da_pdlist);
				LIST_INSERT_HEAD(&pagedep->pd_pendinghd, dap,
				    da_pdlist);
			}
		}
	}
	/*
	 * If there were any rollbacks in the directory, then it must be
	 * marked dirty so that its will eventually get written back in
	 * its correct form.
	 */
	if (chgs) {
		if ((bp->b_flags & B_DELWRI) == 0)
			stat_dir_entry++;
		bdirty(bp);
	}
	/*
	 * If no dependencies remain, the pagedep will be freed.
	 * Otherwise it will remain to update the page before it
	 * is written back to disk.
	 */
	if (LIST_FIRST(&pagedep->pd_pendinghd) == 0) {
		for (i = 0; i < DAHASHSZ; i++)
			if (LIST_FIRST(&pagedep->pd_diraddhd[i]) != NULL)
				break;
		if (i == DAHASHSZ) {
			LIST_REMOVE(pagedep, pd_hash);
			WORKITEM_FREE(pagedep, D_PAGEDEP);
			return (0);
		}
	}
	return (1);
}

/*
 * Writing back in-core inode structures.
 * 
 * The file system only accesses an inode's contents when it occupies an
 * "in-core" inode structure.  These "in-core" structures are separate from
 * the page frames used to cache inode blocks.  Only the latter are
 * transferred to/from the disk.  So, when the updated contents of the
 * "in-core" inode structure are copied to the corresponding in-memory inode
 * block, the dependencies are also transferred.  The following procedure is
 * called when copying a dirty "in-core" inode to a cached inode block.
 */

/*
 * Called when an inode is loaded from disk. If the effective link count
 * differed from the actual link count when it was last flushed, then we
 * need to ensure that the correct effective link count is put back.
 */
void 
softdep_load_inodeblock(ip)
	struct inode *ip;	/* the "in_core" copy of the inode */
{
	struct inodedep *inodedep;

	/*
	 * Check for alternate nlink count.
	 */
	ip->i_effnlink = ip->i_nlink;
	ACQUIRE_LOCK(&lk);
	if (inodedep_lookup(ip->i_fs, ip->i_number, 0, &inodedep) == 0) {
		FREE_LOCK(&lk);
		return;
	}
	ip->i_effnlink -= inodedep->id_nlinkdelta;
	FREE_LOCK(&lk);
}

/*
 * This routine is called just before the "in-core" inode
 * information is to be copied to the in-memory inode block.
 * Recall that an inode block contains several inodes. If
 * the force flag is set, then the dependencies will be
 * cleared so that the update can always be made. Note that
 * the buffer is locked when this routine is called, so we
 * will never be in the middle of writing the inode block 
 * to disk.
 */
void 
softdep_update_inodeblock(ip, bp, waitfor)
	struct inode *ip;	/* the "in_core" copy of the inode */
	struct buf *bp;		/* the buffer containing the inode block */
	int waitfor;		/* nonzero => update must be allowed */
{
	struct inodedep *inodedep;
	struct worklist *wk;
	int error, gotit;

	/*
	 * If the effective link count is not equal to the actual link
	 * count, then we must track the difference in an inodedep while
	 * the inode is (potentially) tossed out of the cache. Otherwise,
	 * if there is no existing inodedep, then there are no dependencies
	 * to track.
	 */
	ACQUIRE_LOCK(&lk);
	if (inodedep_lookup(ip->i_fs, ip->i_number, 0, &inodedep) == 0) {
		if (ip->i_effnlink != ip->i_nlink)
			panic("softdep_update_inodeblock: bad link count");
		FREE_LOCK(&lk);
		return;
	}
	if (inodedep->id_nlinkdelta != ip->i_nlink - ip->i_effnlink)
		panic("softdep_update_inodeblock: bad delta");
	/*
	 * Changes have been initiated. Anything depending on these
	 * changes cannot occur until this inode has been written.
	 */
	inodedep->id_state &= ~COMPLETE;
	if ((inodedep->id_state & ONWORKLIST) == 0)
		WORKLIST_INSERT(&bp->b_dep, &inodedep->id_list);
	/*
	 * Any new dependencies associated with the incore inode must 
	 * now be moved to the list associated with the buffer holding
	 * the in-memory copy of the inode. Once merged process any
	 * allocdirects that are completed by the merger.
	 */
	merge_inode_lists(inodedep);
	if (TAILQ_FIRST(&inodedep->id_inoupdt) != NULL)
		handle_allocdirect_partdone(TAILQ_FIRST(&inodedep->id_inoupdt));
	/*
	 * Now that the inode has been pushed into the buffer, the
	 * operations dependent on the inode being written to disk
	 * can be moved to the id_bufwait so that they will be
	 * processed when the buffer I/O completes.
	 */
	while ((wk = LIST_FIRST(&inodedep->id_inowait)) != NULL) {
		WORKLIST_REMOVE(wk);
		WORKLIST_INSERT(&inodedep->id_bufwait, wk);
	}
	/*
	 * Newly allocated inodes cannot be written until the bitmap
	 * that allocates them have been written (indicated by
	 * DEPCOMPLETE being set in id_state). If we are doing a
	 * forced sync (e.g., an fsync on a file), we force the bitmap
	 * to be written so that the update can be done.
	 */
	if ((inodedep->id_state & DEPCOMPLETE) != 0 || waitfor == 0) {
		FREE_LOCK(&lk);
		return;
	}
	gotit = getdirtybuf(&inodedep->id_buf, MNT_WAIT);
	FREE_LOCK(&lk);
	if (gotit &&
	    (error = BUF_WRITE(inodedep->id_buf)) != 0)
		softdep_error("softdep_update_inodeblock: bwrite", error);
	if ((inodedep->id_state & DEPCOMPLETE) == 0)
		panic("softdep_update_inodeblock: update failed");
}

/*
 * Merge the new inode dependency list (id_newinoupdt) into the old
 * inode dependency list (id_inoupdt). This routine must be called
 * with splbio interrupts blocked.
 */
static void
merge_inode_lists(inodedep)
	struct inodedep *inodedep;
{
	struct allocdirect *listadp, *newadp;

	newadp = TAILQ_FIRST(&inodedep->id_newinoupdt);
	for (listadp = TAILQ_FIRST(&inodedep->id_inoupdt); listadp && newadp;) {
		if (listadp->ad_lbn < newadp->ad_lbn) {
			listadp = TAILQ_NEXT(listadp, ad_next);
			continue;
		}
		TAILQ_REMOVE(&inodedep->id_newinoupdt, newadp, ad_next);
		TAILQ_INSERT_BEFORE(listadp, newadp, ad_next);
		if (listadp->ad_lbn == newadp->ad_lbn) {
			allocdirect_merge(&inodedep->id_inoupdt, newadp,
			    listadp);
			listadp = newadp;
		}
		newadp = TAILQ_FIRST(&inodedep->id_newinoupdt);
	}
	while ((newadp = TAILQ_FIRST(&inodedep->id_newinoupdt)) != NULL) {
		TAILQ_REMOVE(&inodedep->id_newinoupdt, newadp, ad_next);
		TAILQ_INSERT_TAIL(&inodedep->id_inoupdt, newadp, ad_next);
	}
}

/*
 * If we are doing an fsync, then we must ensure that any directory
 * entries for the inode have been written after the inode gets to disk.
 */
int
softdep_fsync(vp)
	struct vnode *vp;	/* the "in_core" copy of the inode */
{
	struct inodedep *inodedep;
	struct pagedep *pagedep;
	struct worklist *wk;
	struct diradd *dap;
	struct mount *mnt;
	struct vnode *pvp;
	struct inode *ip;
	struct buf *bp;
	struct fs *fs;
	struct proc *p = CURPROC;		/* XXX */
	int error, flushparent;
	ino_t parentino;
	ufs_lbn_t lbn;

	ip = VTOI(vp);
	fs = ip->i_fs;
	ACQUIRE_LOCK(&lk);
	if (inodedep_lookup(fs, ip->i_number, 0, &inodedep) == 0) {
		FREE_LOCK(&lk);
		return (0);
	}
	if (LIST_FIRST(&inodedep->id_inowait) != NULL ||
	    LIST_FIRST(&inodedep->id_bufwait) != NULL ||
	    TAILQ_FIRST(&inodedep->id_inoupdt) != NULL ||
	    TAILQ_FIRST(&inodedep->id_newinoupdt) != NULL)
		panic("softdep_fsync: pending ops");
	for (error = 0, flushparent = 0; ; ) {
		if ((wk = LIST_FIRST(&inodedep->id_pendinghd)) == NULL)
			break;
		if (wk->wk_type != D_DIRADD)
			panic("softdep_fsync: Unexpected type %s",
			    TYPENAME(wk->wk_type));
		dap = WK_DIRADD(wk);
		/*
		 * Flush our parent if this directory entry
		 * has a MKDIR_PARENT dependency.
		 */
		if (dap->da_state & DIRCHG)
			pagedep = dap->da_previous->dm_pagedep;
		else
			pagedep = dap->da_pagedep;
		mnt = pagedep->pd_mnt;
		parentino = pagedep->pd_ino;
		lbn = pagedep->pd_lbn;
		if ((dap->da_state & (MKDIR_BODY | COMPLETE)) != COMPLETE)
			panic("softdep_fsync: dirty");
		flushparent = dap->da_state & MKDIR_PARENT;
		/*
		 * If we are being fsync'ed as part of vgone'ing this vnode,
		 * then we will not be able to release and recover the
		 * vnode below, so we just have to give up on writing its
		 * directory entry out. It will eventually be written, just
		 * not now, but then the user was not asking to have it
		 * written, so we are not breaking any promises.
		 */
		if (vp->v_flag & VXLOCK)
			break;
		/*
		 * We prevent deadlock by always fetching inodes from the
		 * root, moving down the directory tree. Thus, when fetching
		 * our parent directory, we must unlock ourselves before
		 * requesting the lock on our parent. See the comment in
		 * ufs_lookup for details on possible races.
		 */
		FREE_LOCK(&lk);
		VOP_UNLOCK(vp, 0, p);
		error = VFS_VGET(mnt, parentino, &pvp);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		if (error != 0)
			return (error);
		if (flushparent) {
			if ((error = UFS_UPDATE(pvp, 1)) != 0) {
				vput(pvp);
				return (error);
			}
		}
		/*
		 * Flush directory page containing the inode's name.
		 */
		error = bread(pvp, lbn, blksize(fs, VTOI(pvp), lbn), p->p_ucred,
		    &bp);
		if (error == 0)
			error = BUF_WRITE(bp);
		vput(pvp);
		if (error != 0)
			return (error);
		ACQUIRE_LOCK(&lk);
		if (inodedep_lookup(fs, ip->i_number, 0, &inodedep) == 0)
			break;
	}
	FREE_LOCK(&lk);
	return (0);
}

/*
 * Flush all the dirty bitmaps associated with the block device
 * before flushing the rest of the dirty blocks so as to reduce
 * the number of dependencies that will have to be rolled back.
 */
void
softdep_fsync_mountdev(vp)
	struct vnode *vp;
{
	struct buf *bp, *nbp;
	struct worklist *wk;

	if (!vn_isdisk(vp, NULL))
		panic("softdep_fsync_mountdev: vnode not a disk");
	ACQUIRE_LOCK(&lk);
	for (bp = TAILQ_FIRST(&vp->v_dirtyblkhd); bp; bp = nbp) {
		nbp = TAILQ_NEXT(bp, b_vnbufs);
		/* 
		 * If it is already scheduled, skip to the next buffer.
		 */
		if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT))
			continue;
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("softdep_fsync_mountdev: not dirty");
		/*
		 * We are only interested in bitmaps with outstanding
		 * dependencies.
		 */
		if ((wk = LIST_FIRST(&bp->b_dep)) == NULL ||
		    wk->wk_type != D_BMSAFEMAP ||
		    (bp->b_xflags & BX_BKGRDINPROG)) {
			BUF_UNLOCK(bp);
			continue;
		}
		bremfree(bp);
		FREE_LOCK(&lk);
		(void) bawrite(bp);
		ACQUIRE_LOCK(&lk);
		/*
		 * Since we may have slept during the I/O, we need 
		 * to start from a known point.
		 */
		nbp = TAILQ_FIRST(&vp->v_dirtyblkhd);
	}
	drain_output(vp, 1);
	FREE_LOCK(&lk);
}

/*
 * This routine is called when we are trying to synchronously flush a
 * file. This routine must eliminate any filesystem metadata dependencies
 * so that the syncing routine can succeed by pushing the dirty blocks
 * associated with the file. If any I/O errors occur, they are returned.
 */
int
softdep_sync_metadata(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct pagedep *pagedep;
	struct allocdirect *adp;
	struct allocindir *aip;
	struct buf *bp, *nbp;
	struct worklist *wk;
	int i, error, waitfor;

	/*
	 * Check whether this vnode is involved in a filesystem
	 * that is doing soft dependency processing.
	 */
	if (!vn_isdisk(vp, NULL)) {
		if (!DOINGSOFTDEP(vp))
			return (0);
	} else
		if (vp->v_specmountpoint == NULL ||
		    (vp->v_specmountpoint->mnt_flag & MNT_SOFTDEP) == 0)
			return (0);
	/*
	 * Ensure that any direct block dependencies have been cleared.
	 */
	ACQUIRE_LOCK(&lk);
	if ((error = flush_inodedep_deps(VTOI(vp)->i_fs, VTOI(vp)->i_number))) {
		FREE_LOCK(&lk);
		return (error);
	}
	/*
	 * For most files, the only metadata dependencies are the
	 * cylinder group maps that allocate their inode or blocks.
	 * The block allocation dependencies can be found by traversing
	 * the dependency lists for any buffers that remain on their
	 * dirty buffer list. The inode allocation dependency will
	 * be resolved when the inode is updated with MNT_WAIT.
	 * This work is done in two passes. The first pass grabs most
	 * of the buffers and begins asynchronously writing them. The
	 * only way to wait for these asynchronous writes is to sleep
	 * on the filesystem vnode which may stay busy for a long time
	 * if the filesystem is active. So, instead, we make a second
	 * pass over the dependencies blocking on each write. In the
	 * usual case we will be blocking against a write that we
	 * initiated, so when it is done the dependency will have been
	 * resolved. Thus the second pass is expected to end quickly.
	 */
	waitfor = MNT_NOWAIT;
top:
	if (getdirtybuf(&TAILQ_FIRST(&vp->v_dirtyblkhd), MNT_WAIT) == 0) {
		FREE_LOCK(&lk);
		return (0);
	}
	bp = TAILQ_FIRST(&vp->v_dirtyblkhd);
loop:
	/*
	 * As we hold the buffer locked, none of its dependencies
	 * will disappear.
	 */
	for (wk = LIST_FIRST(&bp->b_dep); wk;
	     wk = LIST_NEXT(wk, wk_list)) {
		switch (wk->wk_type) {

		case D_ALLOCDIRECT:
			adp = WK_ALLOCDIRECT(wk);
			if (adp->ad_state & DEPCOMPLETE)
				break;
			nbp = adp->ad_buf;
			if (getdirtybuf(&nbp, waitfor) == 0)
				break;
			FREE_LOCK(&lk);
			if (waitfor == MNT_NOWAIT) {
				bawrite(nbp);
			} else if ((error = BUF_WRITE(nbp)) != 0) {
				bawrite(bp);
				return (error);
			}
			ACQUIRE_LOCK(&lk);
			break;

		case D_ALLOCINDIR:
			aip = WK_ALLOCINDIR(wk);
			if (aip->ai_state & DEPCOMPLETE)
				break;
			nbp = aip->ai_buf;
			if (getdirtybuf(&nbp, waitfor) == 0)
				break;
			FREE_LOCK(&lk);
			if (waitfor == MNT_NOWAIT) {
				bawrite(nbp);
			} else if ((error = BUF_WRITE(nbp)) != 0) {
				bawrite(bp);
				return (error);
			}
			ACQUIRE_LOCK(&lk);
			break;

		case D_INDIRDEP:
		restart:
			for (aip = LIST_FIRST(&WK_INDIRDEP(wk)->ir_deplisthd);
			     aip; aip = LIST_NEXT(aip, ai_next)) {
				if (aip->ai_state & DEPCOMPLETE)
					continue;
				nbp = aip->ai_buf;
				if (getdirtybuf(&nbp, MNT_WAIT) == 0)
					goto restart;
				FREE_LOCK(&lk);
				if ((error = BUF_WRITE(nbp)) != 0) {
					bawrite(bp);
					return (error);
				}
				ACQUIRE_LOCK(&lk);
				goto restart;
			}
			break;

		case D_INODEDEP:
			if ((error = flush_inodedep_deps(WK_INODEDEP(wk)->id_fs,
			    WK_INODEDEP(wk)->id_ino)) != 0) {
				FREE_LOCK(&lk);
				bawrite(bp);
				return (error);
			}
			break;

		case D_PAGEDEP:
			/*
			 * We are trying to sync a directory that may
			 * have dependencies on both its own metadata
			 * and/or dependencies on the inodes of any
			 * recently allocated files. We walk its diradd
			 * lists pushing out the associated inode.
			 */
			pagedep = WK_PAGEDEP(wk);
			for (i = 0; i < DAHASHSZ; i++) {
				if (LIST_FIRST(&pagedep->pd_diraddhd[i]) == 0)
					continue;
				if ((error =
				    flush_pagedep_deps(vp, pagedep->pd_mnt,
						&pagedep->pd_diraddhd[i]))) {
					FREE_LOCK(&lk);
					bawrite(bp);
					return (error);
				}
			}
			break;

		case D_MKDIR:
			/*
			 * This case should never happen if the vnode has
			 * been properly sync'ed. However, if this function
			 * is used at a place where the vnode has not yet
			 * been sync'ed, this dependency can show up. So,
			 * rather than panic, just flush it.
			 */
			nbp = WK_MKDIR(wk)->md_buf;
			if (getdirtybuf(&nbp, waitfor) == 0)
				break;
			FREE_LOCK(&lk);
			if (waitfor == MNT_NOWAIT) {
				bawrite(nbp);
			} else if ((error = BUF_WRITE(nbp)) != 0) {
				bawrite(bp);
				return (error);
			}
			ACQUIRE_LOCK(&lk);
			break;

		case D_BMSAFEMAP:
			/*
			 * This case should never happen if the vnode has
			 * been properly sync'ed. However, if this function
			 * is used at a place where the vnode has not yet
			 * been sync'ed, this dependency can show up. So,
			 * rather than panic, just flush it.
			 */
			nbp = WK_BMSAFEMAP(wk)->sm_buf;
			if (getdirtybuf(&nbp, waitfor) == 0)
				break;
			FREE_LOCK(&lk);
			if (waitfor == MNT_NOWAIT) {
				bawrite(nbp);
			} else if ((error = BUF_WRITE(nbp)) != 0) {
				bawrite(bp);
				return (error);
			}
			ACQUIRE_LOCK(&lk);
			break;

		default:
			panic("softdep_sync_metadata: Unknown type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
	(void) getdirtybuf(&TAILQ_NEXT(bp, b_vnbufs), MNT_WAIT);
	nbp = TAILQ_NEXT(bp, b_vnbufs);
	FREE_LOCK(&lk);
	bawrite(bp);
	ACQUIRE_LOCK(&lk);
	if (nbp != NULL) {
		bp = nbp;
		goto loop;
	}
	/*
	 * We must wait for any I/O in progress to finish so that
	 * all potential buffers on the dirty list will be visible.
	 * Once they are all there, proceed with the second pass
	 * which will wait for the I/O as per above.
	 */
	drain_output(vp, 1);
	/*
	 * The brief unlock is to allow any pent up dependency
	 * processing to be done.
	 */
	if (waitfor == MNT_NOWAIT) {
		waitfor = MNT_WAIT;
		FREE_LOCK(&lk);
		ACQUIRE_LOCK(&lk);
		goto top;
	}

	/*
	 * If we have managed to get rid of all the dirty buffers,
	 * then we are done. For certain directories and block
	 * devices, we may need to do further work.
	 */
	if (TAILQ_FIRST(&vp->v_dirtyblkhd) == NULL) {
		FREE_LOCK(&lk);
		return (0);
	}

	FREE_LOCK(&lk);
	/*
	 * If we are trying to sync a block device, some of its buffers may
	 * contain metadata that cannot be written until the contents of some
	 * partially written files have been written to disk. The only easy
	 * way to accomplish this is to sync the entire filesystem (luckily
	 * this happens rarely).
	 */
	if (vn_isdisk(vp, NULL) && 
	    vp->v_specmountpoint && !VOP_ISLOCKED(vp, NULL) &&
	    (error = VFS_SYNC(vp->v_specmountpoint, MNT_WAIT, ap->a_cred,
	     ap->a_p)) != 0)
		return (error);
	return (0);
}

/*
 * Flush the dependencies associated with an inodedep.
 * Called with splbio blocked.
 */
static int
flush_inodedep_deps(fs, ino)
	struct fs *fs;
	ino_t ino;
{
	struct inodedep *inodedep;
	struct allocdirect *adp;
	int error, waitfor;
	struct buf *bp;

	/*
	 * This work is done in two passes. The first pass grabs most
	 * of the buffers and begins asynchronously writing them. The
	 * only way to wait for these asynchronous writes is to sleep
	 * on the filesystem vnode which may stay busy for a long time
	 * if the filesystem is active. So, instead, we make a second
	 * pass over the dependencies blocking on each write. In the
	 * usual case we will be blocking against a write that we
	 * initiated, so when it is done the dependency will have been
	 * resolved. Thus the second pass is expected to end quickly.
	 * We give a brief window at the top of the loop to allow
	 * any pending I/O to complete.
	 */
	for (waitfor = MNT_NOWAIT; ; ) {
		FREE_LOCK(&lk);
		ACQUIRE_LOCK(&lk);
		if (inodedep_lookup(fs, ino, 0, &inodedep) == 0)
			return (0);
		for (adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp;
		     adp = TAILQ_NEXT(adp, ad_next)) {
			if (adp->ad_state & DEPCOMPLETE)
				continue;
			bp = adp->ad_buf;
			if (getdirtybuf(&bp, waitfor) == 0) {
				if (waitfor == MNT_NOWAIT)
					continue;
				break;
			}
			FREE_LOCK(&lk);
			if (waitfor == MNT_NOWAIT) {
				bawrite(bp);
			} else if ((error = BUF_WRITE(bp)) != 0) {
				ACQUIRE_LOCK(&lk);
				return (error);
			}
			ACQUIRE_LOCK(&lk);
			break;
		}
		if (adp != NULL)
			continue;
		for (adp = TAILQ_FIRST(&inodedep->id_newinoupdt); adp;
		     adp = TAILQ_NEXT(adp, ad_next)) {
			if (adp->ad_state & DEPCOMPLETE)
				continue;
			bp = adp->ad_buf;
			if (getdirtybuf(&bp, waitfor) == 0) {
				if (waitfor == MNT_NOWAIT)
					continue;
				break;
			}
			FREE_LOCK(&lk);
			if (waitfor == MNT_NOWAIT) {
				bawrite(bp);
			} else if ((error = BUF_WRITE(bp)) != 0) {
				ACQUIRE_LOCK(&lk);
				return (error);
			}
			ACQUIRE_LOCK(&lk);
			break;
		}
		if (adp != NULL)
			continue;
		/*
		 * If pass2, we are done, otherwise do pass 2.
		 */
		if (waitfor == MNT_WAIT)
			break;
		waitfor = MNT_WAIT;
	}
	/*
	 * Try freeing inodedep in case all dependencies have been removed.
	 */
	if (inodedep_lookup(fs, ino, 0, &inodedep) != 0)
		(void) free_inodedep(inodedep);
	return (0);
}

/*
 * Eliminate a pagedep dependency by flushing out all its diradd dependencies.
 * Called with splbio blocked.
 */
static int
flush_pagedep_deps(pvp, mp, diraddhdp)
	struct vnode *pvp;
	struct mount *mp;
	struct diraddhd *diraddhdp;
{
	struct proc *p = CURPROC;	/* XXX */
	struct inodedep *inodedep;
	struct ufsmount *ump;
	struct diradd *dap;
	struct vnode *vp;
	int gotit, error = 0;
	struct buf *bp;
	ino_t inum;

	ump = VFSTOUFS(mp);
	while ((dap = LIST_FIRST(diraddhdp)) != NULL) {
		/*
		 * Flush ourselves if this directory entry
		 * has a MKDIR_PARENT dependency.
		 */
		if (dap->da_state & MKDIR_PARENT) {
			FREE_LOCK(&lk);
			if ((error = UFS_UPDATE(pvp, 1)) != 0)
				break;
			ACQUIRE_LOCK(&lk);
			/*
			 * If that cleared dependencies, go on to next.
			 */
			if (dap != LIST_FIRST(diraddhdp))
				continue;
			if (dap->da_state & MKDIR_PARENT)
				panic("flush_pagedep_deps: MKDIR_PARENT");
		}
		/*
		 * A newly allocated directory must have its "." and
		 * ".." entries written out before its name can be
		 * committed in its parent. We do not want or need
		 * the full semantics of a synchronous VOP_FSYNC as
		 * that may end up here again, once for each directory
		 * level in the filesystem. Instead, we push the blocks
		 * and wait for them to clear. We have to fsync twice
		 * because the first call may choose to defer blocks
		 * that still have dependencies, but deferral will
		 * happen at most once.
		 */
		inum = dap->da_newinum;
		if (dap->da_state & MKDIR_BODY) {
			FREE_LOCK(&lk);
			if ((error = VFS_VGET(mp, inum, &vp)) != 0)
				break;
			if ((error=VOP_FSYNC(vp, p->p_ucred, MNT_NOWAIT, p)) ||
			    (error=VOP_FSYNC(vp, p->p_ucred, MNT_NOWAIT, p))) {
				vput(vp);
				break;
			}
			drain_output(vp, 0);
			vput(vp);
			ACQUIRE_LOCK(&lk);
			/*
			 * If that cleared dependencies, go on to next.
			 */
			if (dap != LIST_FIRST(diraddhdp))
				continue;
			if (dap->da_state & MKDIR_BODY)
				panic("flush_pagedep_deps: MKDIR_BODY");
		}
		/*
		 * Flush the inode on which the directory entry depends.
		 * Having accounted for MKDIR_PARENT and MKDIR_BODY above,
		 * the only remaining dependency is that the updated inode
		 * count must get pushed to disk. The inode has already
		 * been pushed into its inode buffer (via VOP_UPDATE) at
		 * the time of the reference count change. So we need only
		 * locate that buffer, ensure that there will be no rollback
		 * caused by a bitmap dependency, then write the inode buffer.
		 */
		if (inodedep_lookup(ump->um_fs, inum, 0, &inodedep) == 0)
			panic("flush_pagedep_deps: lost inode");
		/*
		 * If the inode still has bitmap dependencies,
		 * push them to disk.
		 */
		if ((inodedep->id_state & DEPCOMPLETE) == 0) {
			gotit = getdirtybuf(&inodedep->id_buf, MNT_WAIT);
			FREE_LOCK(&lk);
			if (gotit &&
			    (error = BUF_WRITE(inodedep->id_buf)) != 0)
				break;
			ACQUIRE_LOCK(&lk);
			if (dap != LIST_FIRST(diraddhdp))
				continue;
		}
		/*
		 * If the inode is still sitting in a buffer waiting
		 * to be written, push it to disk.
		 */
		FREE_LOCK(&lk);
		if ((error = bread(ump->um_devvp,
		    fsbtodb(ump->um_fs, ino_to_fsba(ump->um_fs, inum)),
		    (int)ump->um_fs->fs_bsize, NOCRED, &bp)) != 0)
			break;
		if ((error = BUF_WRITE(bp)) != 0)
			break;
		ACQUIRE_LOCK(&lk);
		/*
		 * If we have failed to get rid of all the dependencies
		 * then something is seriously wrong.
		 */
		if (dap == LIST_FIRST(diraddhdp))
			panic("flush_pagedep_deps: flush failed");
	}
	if (error)
		ACQUIRE_LOCK(&lk);
	return (error);
}

/*
 * A large burst of file addition or deletion activity can drive the
 * memory load excessively high. Therefore we deliberately slow things
 * down and speed up the I/O processing if we find ourselves with too
 * many dependencies in progress.
 */
static int
request_cleanup(resource, islocked)
	int resource;
	int islocked;
{
	struct callout_handle handle;
	struct proc *p = CURPROC;

	/*
	 * We never hold up the filesystem syncer process.
	 */
	if (p == filesys_syncer)
		return (0);
	/*
	 * If we are resource constrained on inode dependencies, try
	 * flushing some dirty inodes. Otherwise, we are constrained
	 * by file deletions, so try accelerating flushes of directories
	 * with removal dependencies. We would like to do the cleanup
	 * here, but we probably hold an inode locked at this point and 
	 * that might deadlock against one that we try to clean. So,
	 * the best that we can do is request the syncer daemon to do
	 * the cleanup for us.
	 */
	switch (resource) {

	case FLUSH_INODES:
		stat_ino_limit_push += 1;
		req_clear_inodedeps = 1;
		break;

	case FLUSH_REMOVE:
		stat_blk_limit_push += 1;
		req_clear_remove = 1;
		break;

	default:
		panic("request_cleanup: unknown type");
	}
	/*
	 * Hopefully the syncer daemon will catch up and awaken us.
	 * We wait at most tickdelay before proceeding in any case.
	 */
	if (islocked == 0)
		ACQUIRE_LOCK(&lk);
	if (proc_waiting == 0) {
		proc_waiting = 1;
		handle = timeout(pause_timer, NULL,
		    tickdelay > 2 ? tickdelay : 2);
	}
	FREE_LOCK_INTERLOCKED(&lk);
	(void) tsleep((caddr_t)&proc_waiting, PPAUSE, "softupdate", 0);
	ACQUIRE_LOCK_INTERLOCKED(&lk);
	if (proc_waiting) {
		untimeout(pause_timer, NULL, handle);
		proc_waiting = 0;
	} else {
		switch (resource) {

		case FLUSH_INODES:
			stat_ino_limit_hit += 1;
			break;

		case FLUSH_REMOVE:
			stat_blk_limit_hit += 1;
			break;
		}
	}
	if (islocked == 0)
		FREE_LOCK(&lk);
	return (1);
}

/*
 * Awaken processes pausing in request_cleanup and clear proc_waiting
 * to indicate that there is no longer a timer running.
 */
void
pause_timer(arg)
	void *arg;
{

	proc_waiting = 0;
	wakeup(&proc_waiting);
}

/*
 * Flush out a directory with at least one removal dependency in an effort to
 * reduce the number of dirrem, freefile, and freeblks dependency structures.
 */
static void
clear_remove(p)
	struct proc *p;
{
	struct pagedep_hashhead *pagedephd;
	struct pagedep *pagedep;
	static int next = 0;
	struct mount *mp;
	struct vnode *vp;
	int error, cnt;
	ino_t ino;

	ACQUIRE_LOCK(&lk);
	for (cnt = 0; cnt < pagedep_hash; cnt++) {
		pagedephd = &pagedep_hashtbl[next++];
		if (next >= pagedep_hash)
			next = 0;
		for (pagedep = LIST_FIRST(pagedephd); pagedep;
		     pagedep = LIST_NEXT(pagedep, pd_hash)) {
			if (LIST_FIRST(&pagedep->pd_dirremhd) == NULL)
				continue;
			mp = pagedep->pd_mnt;
			ino = pagedep->pd_ino;
			FREE_LOCK(&lk);
			if (vn_start_write(NULL, &mp, V_NOWAIT) != 0)
				continue;
			if ((error = VFS_VGET(mp, ino, &vp)) != 0) {
				softdep_error("clear_remove: vget", error);
				vn_finished_write(mp);
				return;
			}
			if ((error = VOP_FSYNC(vp, p->p_ucred, MNT_NOWAIT, p)))
				softdep_error("clear_remove: fsync", error);
			drain_output(vp, 0);
			vput(vp);
			vn_finished_write(mp);
			return;
		}
	}
	FREE_LOCK(&lk);
}

/*
 * Clear out a block of dirty inodes in an effort to reduce
 * the number of inodedep dependency structures.
 */
static void
clear_inodedeps(p)
	struct proc *p;
{
	struct inodedep_hashhead *inodedephd;
	struct inodedep *inodedep;
	static int next = 0;
	struct mount *mp;
	struct vnode *vp;
	struct fs *fs;
	int error, cnt;
	ino_t firstino, lastino, ino;

	ACQUIRE_LOCK(&lk);
	/*
	 * Pick a random inode dependency to be cleared.
	 * We will then gather up all the inodes in its block 
	 * that have dependencies and flush them out.
	 */
	for (cnt = 0; cnt < inodedep_hash; cnt++) {
		inodedephd = &inodedep_hashtbl[next++];
		if (next >= inodedep_hash)
			next = 0;
		if ((inodedep = LIST_FIRST(inodedephd)) != NULL)
			break;
	}
	/*
	 * Ugly code to find mount point given pointer to superblock.
	 */
	fs = inodedep->id_fs;
	TAILQ_FOREACH(mp, &mountlist, mnt_list)
		if ((mp->mnt_flag & MNT_SOFTDEP) && fs == VFSTOUFS(mp)->um_fs)
			break;
	/*
	 * Find the last inode in the block with dependencies.
	 */
	firstino = inodedep->id_ino & ~(INOPB(fs) - 1);
	for (lastino = firstino + INOPB(fs) - 1; lastino > firstino; lastino--)
		if (inodedep_lookup(fs, lastino, 0, &inodedep) != 0)
			break;
	/*
	 * Asynchronously push all but the last inode with dependencies.
	 * Synchronously push the last inode with dependencies to ensure
	 * that the inode block gets written to free up the inodedeps.
	 */
	for (ino = firstino; ino <= lastino; ino++) {
		if (inodedep_lookup(fs, ino, 0, &inodedep) == 0)
			continue;
		FREE_LOCK(&lk);
		if (vn_start_write(NULL, &mp, V_NOWAIT) != 0)
			continue;
		if ((error = VFS_VGET(mp, ino, &vp)) != 0) {
			softdep_error("clear_inodedeps: vget", error);
			vn_finished_write(mp);
			return;
		}
		if (ino == lastino) {
			if ((error = VOP_FSYNC(vp, p->p_ucred, MNT_WAIT, p)))
				softdep_error("clear_inodedeps: fsync1", error);
		} else {
			if ((error = VOP_FSYNC(vp, p->p_ucred, MNT_NOWAIT, p)))
				softdep_error("clear_inodedeps: fsync2", error);
			drain_output(vp, 0);
		}
		vput(vp);
		vn_finished_write(mp);
		ACQUIRE_LOCK(&lk);
	}
	FREE_LOCK(&lk);
}

/*
 * Function to determine if the buffer has outstanding dependencies
 * that will cause a roll-back if the buffer is written. If wantcount
 * is set, return number of dependencies, otherwise just yes or no.
 */
static int
softdep_count_dependencies(bp, wantcount)
	struct buf *bp;
	int wantcount;
{
	struct worklist *wk;
	struct inodedep *inodedep;
	struct indirdep *indirdep;
	struct allocindir *aip;
	struct pagedep *pagedep;
	struct diradd *dap;
	int i, retval;

	retval = 0;
	ACQUIRE_LOCK(&lk);
	for (wk = LIST_FIRST(&bp->b_dep); wk; wk = LIST_NEXT(wk, wk_list)) {
		switch (wk->wk_type) {

		case D_INODEDEP:
			inodedep = WK_INODEDEP(wk);
			if ((inodedep->id_state & DEPCOMPLETE) == 0) {
				/* bitmap allocation dependency */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			if (TAILQ_FIRST(&inodedep->id_inoupdt)) {
				/* direct block pointer dependency */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			continue;

		case D_INDIRDEP:
			indirdep = WK_INDIRDEP(wk);
			for (aip = LIST_FIRST(&indirdep->ir_deplisthd);
			     aip; aip = LIST_NEXT(aip, ai_next)) {
				/* indirect block pointer dependency */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			continue;

		case D_PAGEDEP:
			pagedep = WK_PAGEDEP(wk);
			for (i = 0; i < DAHASHSZ; i++) {
				for (dap = LIST_FIRST(&pagedep->pd_diraddhd[i]);
				     dap; dap = LIST_NEXT(dap, da_pdlist)) {
					/* directory entry dependency */
					retval += 1;
					if (!wantcount)
						goto out;
				}
			}
			continue;

		case D_BMSAFEMAP:
		case D_ALLOCDIRECT:
		case D_ALLOCINDIR:
		case D_MKDIR:
			/* never a dependency on these blocks */
			continue;

		default:
			panic("softdep_check_for_rollback: Unexpected type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
out:
	FREE_LOCK(&lk);
	return retval;
}

/*
 * Acquire exclusive access to a buffer.
 * Must be called with splbio blocked.
 * Return 1 if buffer was acquired.
 */
static int
getdirtybuf(bpp, waitfor)
	struct buf **bpp;
	int waitfor;
{
	struct buf *bp;

	for (;;) {
		if ((bp = *bpp) == NULL)
			return (0);
		if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT) == 0) {
			if ((bp->b_xflags & BX_BKGRDINPROG) == 0)
				break;
			BUF_UNLOCK(bp);
			if (waitfor != MNT_WAIT)
				return (0);
			bp->b_xflags |= BX_BKGRDWAIT;
			FREE_LOCK_INTERLOCKED(&lk);
			tsleep(&bp->b_xflags, PRIBIO, "getbuf", 0);
			ACQUIRE_LOCK_INTERLOCKED(&lk);
			continue;
		}
		if (waitfor != MNT_WAIT)
			return (0);
		FREE_LOCK_INTERLOCKED(&lk);
		if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_SLEEPFAIL) != ENOLCK)
			panic("getdirtybuf: inconsistent lock");
		ACQUIRE_LOCK_INTERLOCKED(&lk);
	}
	if ((bp->b_flags & B_DELWRI) == 0) {
		BUF_UNLOCK(bp);
		return (0);
	}
	bremfree(bp);
	return (1);
}

/*
 * Wait for pending output on a vnode to complete.
 * Must be called with vnode locked.
 */
static void
drain_output(vp, islocked)
	struct vnode *vp;
	int islocked;
{

	if (!islocked)
		ACQUIRE_LOCK(&lk);
	while (vp->v_numoutput) {
		vp->v_flag |= VBWAIT;
		FREE_LOCK_INTERLOCKED(&lk);
		tsleep((caddr_t)&vp->v_numoutput, PRIBIO + 1, "drainvp", 0);
		ACQUIRE_LOCK_INTERLOCKED(&lk);
	}
	if (!islocked)
		FREE_LOCK(&lk);
}

/*
 * Called whenever a buffer that is being invalidated or reallocated
 * contains dependencies. This should only happen if an I/O error has
 * occurred. The routine is called with the buffer locked.
 */ 
static void
softdep_deallocate_dependencies(bp)
	struct buf *bp;
{

	if ((bp->b_ioflags & BIO_ERROR) == 0)
		panic("softdep_deallocate_dependencies: dangling deps");
	softdep_error(bp->b_vp->v_mount->mnt_stat.f_mntonname, bp->b_error);
	panic("softdep_deallocate_dependencies: unrecovered I/O error");
}

/*
 * Function to handle asynchronous write errors in the filesystem.
 */
void
softdep_error(func, error)
	char *func;
	int error;
{

	/* XXX should do something better! */
	printf("%s: got error %d while accessing filesystem\n", func, error);
}
