/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Scooter Morris at Genentech Inc.
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
 *	@(#)ufs_lockf.c	8.3 (Berkeley) 1/6/94
 * $Id: kern_lockf.c,v 1.7 1996/12/19 13:22:30 bde Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>

#include <sys/lockf.h>

/*
 * This variable controls the maximum number of processes that will
 * be checked in doing deadlock detection.
 */
int maxlockdepth = MAXDEPTH;

#ifdef LOCKF_DEBUG
int	lockf_debug = 0;
#endif

#define NOLOCKF (struct lockf *)0
#define SELF	0x1
#define OTHERS	0x2
static void	 lf_addblock __P((struct lockf *, struct lockf *));
static int	 lf_clearlock __P((struct lockf *));
static int	 lf_findoverlap __P((struct lockf *,
	    struct lockf *, int, struct lockf ***, struct lockf **));
static struct lockf *
	 lf_getblock __P((struct lockf *));
static int	 lf_getlock __P((struct lockf *, struct flock *));
static int	 lf_setlock __P((struct lockf *));
static void	 lf_split __P((struct lockf *, struct lockf *));
static void	 lf_wakelock __P((struct lockf *));

/*
 * Advisory record locking support
 */
int
lf_advlock(ap, head, size)
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		caddr_t  a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap;
	struct lockf **head;
	u_quad_t size;
{
	register struct flock *fl = ap->a_fl;
	register struct lockf *lock;
	off_t start, end;
	int error;

	/*
	 * Convert the flock structure into a start and end.
	 */
	switch (fl->l_whence) {

	case SEEK_SET:
	case SEEK_CUR:
		/*
		 * Caller is responsible for adding any necessary offset
		 * when SEEK_CUR is used.
		 */
		start = fl->l_start;
		break;

	case SEEK_END:
		start = size + fl->l_start;
		break;

	default:
		return (EINVAL);
	}
	if (start < 0)
		return (EINVAL);
	if (fl->l_len == 0)
		end = -1;
	else {
		end = start + fl->l_len - 1;
		if (end < start)
			return (EINVAL);
	}
	/*
	 * Avoid the common case of unlocking when inode has no locks.
	 */
	if (*head == (struct lockf *)0) {
		if (ap->a_op != F_SETLK) {
			fl->l_type = F_UNLCK;
			return (0);
		}
	}
	/*
	 * Create the lockf structure
	 */
	MALLOC(lock, struct lockf *, sizeof *lock, M_LOCKF, M_WAITOK);
	lock->lf_start = start;
	lock->lf_end = end;
	lock->lf_id = ap->a_id;
	lock->lf_head = head;
	lock->lf_type = fl->l_type;
	lock->lf_next = (struct lockf *)0;
	lock->lf_block = (struct lockf *)0;
	lock->lf_flags = ap->a_flags;
	/*
	 * Do the requested operation.
	 */
	switch(ap->a_op) {
	case F_SETLK:
		return (lf_setlock(lock));

	case F_UNLCK:
		error = lf_clearlock(lock);
		FREE(lock, M_LOCKF);
		return (error);

	case F_GETLK:
		error = lf_getlock(lock, fl);
		FREE(lock, M_LOCKF);
		return (error);

	default:
		free(lock, M_LOCKF);
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Set a byte-range lock.
 */
static int
lf_setlock(lock)
	register struct lockf *lock;
{
	register struct lockf *block;
	struct lockf **head = lock->lf_head;
	struct lockf **prev, *overlap, *ltmp;
	static char lockstr[] = "lockf";
	int ovcase, priority, needtolink, error;

#ifdef LOCKF_DEBUG
	if (lockf_debug & 1)
		lf_print("lf_setlock", lock);
#endif /* LOCKF_DEBUG */

	/*
	 * Set the priority
	 */
	priority = PLOCK;
	if (lock->lf_type == F_WRLCK)
		priority += 4;
	priority |= PCATCH;
	/*
	 * Scan lock list for this file looking for locks that would block us.
	 */
	while ((block = lf_getblock(lock))) {
		/*
		 * Free the structure and return if nonblocking.
		 */
		if ((lock->lf_flags & F_WAIT) == 0) {
			FREE(lock, M_LOCKF);
			return (EAGAIN);
		}
		/*
		 * We are blocked. Since flock style locks cover
		 * the whole file, there is no chance for deadlock.
		 * For byte-range locks we must check for deadlock.
		 *
		 * Deadlock detection is done by looking through the
		 * wait channels to see if there are any cycles that
		 * involve us. MAXDEPTH is set just to make sure we
		 * do not go off into neverland.
		 */
		if ((lock->lf_flags & F_POSIX) &&
		    (block->lf_flags & F_POSIX)) {
			register struct proc *wproc;
			register struct lockf *waitblock;
			int i = 0;

			/* The block is waiting on something */
			wproc = (struct proc *)block->lf_id;
			while (wproc->p_wchan &&
			       (wproc->p_wmesg == lockstr) &&
			       (i++ < maxlockdepth)) {
				waitblock = (struct lockf *)wproc->p_wchan;
				/* Get the owner of the blocking lock */
				waitblock = waitblock->lf_next;
				if ((waitblock->lf_flags & F_POSIX) == 0)
					break;
				wproc = (struct proc *)waitblock->lf_id;
				if (wproc == (struct proc *)lock->lf_id) {
					free(lock, M_LOCKF);
					return (EDEADLK);
				}
			}
		}
		/*
		 * For flock type locks, we must first remove
		 * any shared locks that we hold before we sleep
		 * waiting for an exclusive lock.
		 */
		if ((lock->lf_flags & F_FLOCK) &&
		    lock->lf_type == F_WRLCK) {
			lock->lf_type = F_UNLCK;
			(void) lf_clearlock(lock);
			lock->lf_type = F_WRLCK;
		}
		/*
		 * Add our lock to the blocked list and sleep until we're free.
		 * Remember who blocked us (for deadlock detection).
		 */
		lock->lf_next = block;
		lf_addblock(block, lock);
#ifdef LOCKF_DEBUG
		if (lockf_debug & 1) {
			lf_print("lf_setlock: blocking on", block);
			lf_printlist("lf_setlock", block);
		}
#endif /* LOCKF_DEBUG */
		if ((error = tsleep((caddr_t)lock, priority, lockstr, 0))) {
			/*
			 * Delete ourselves from the waiting to lock list.
			 */
			for (block = lock->lf_next;
			     block != NOLOCKF;
			     block = block->lf_block) {
				if (block->lf_block != lock)
					continue;
				block->lf_block = block->lf_block->lf_block;
				break;
			}
			free(lock, M_LOCKF);
			return (error);
		}
	}
	/*
	 * No blocks!!  Add the lock.  Note that we will
	 * downgrade or upgrade any overlapping locks this
	 * process already owns.
	 *
	 * Skip over locks owned by other processes.
	 * Handle any locks that overlap and are owned by ourselves.
	 */
	prev = head;
	block = *head;
	needtolink = 1;
	for (;;) {
		ovcase = lf_findoverlap(block, lock, SELF, &prev, &overlap);
		if (ovcase)
			block = overlap->lf_next;
		/*
		 * Six cases:
		 *	0) no overlap
		 *	1) overlap == lock
		 *	2) overlap contains lock
		 *	3) lock contains overlap
		 *	4) overlap starts before lock
		 *	5) overlap ends after lock
		 */
		switch (ovcase) {
		case 0: /* no overlap */
			if (needtolink) {
				*prev = lock;
				lock->lf_next = overlap;
			}
			break;

		case 1: /* overlap == lock */
			/*
			 * If downgrading lock, others may be
			 * able to acquire it.
			 */
			if (lock->lf_type == F_RDLCK &&
			    overlap->lf_type == F_WRLCK)
				lf_wakelock(overlap);
			overlap->lf_type = lock->lf_type;
			FREE(lock, M_LOCKF);
			lock = overlap; /* for debug output below */
			break;

		case 2: /* overlap contains lock */
			/*
			 * Check for common starting point and different types.
			 */
			if (overlap->lf_type == lock->lf_type) {
				free(lock, M_LOCKF);
				lock = overlap; /* for debug output below */
				break;
			}
			if (overlap->lf_start == lock->lf_start) {
				*prev = lock;
				lock->lf_next = overlap;
				overlap->lf_start = lock->lf_end + 1;
			} else
				lf_split(overlap, lock);
			lf_wakelock(overlap);
			break;

		case 3: /* lock contains overlap */
			/*
			 * If downgrading lock, others may be able to
			 * acquire it, otherwise take the list.
			 */
			if (lock->lf_type == F_RDLCK &&
			    overlap->lf_type == F_WRLCK) {
				lf_wakelock(overlap);
			} else {
				ltmp = lock->lf_block;
				lock->lf_block = overlap->lf_block;
				lf_addblock(lock, ltmp);
			}
			/*
			 * Add the new lock if necessary and delete the overlap.
			 */
			if (needtolink) {
				*prev = lock;
				lock->lf_next = overlap->lf_next;
				prev = &lock->lf_next;
				needtolink = 0;
			} else
				*prev = overlap->lf_next;
			free(overlap, M_LOCKF);
			continue;

		case 4: /* overlap starts before lock */
			/*
			 * Add lock after overlap on the list.
			 */
			lock->lf_next = overlap->lf_next;
			overlap->lf_next = lock;
			overlap->lf_end = lock->lf_start - 1;
			prev = &lock->lf_next;
			lf_wakelock(overlap);
			needtolink = 0;
			continue;

		case 5: /* overlap ends after lock */
			/*
			 * Add the new lock before overlap.
			 */
			if (needtolink) {
				*prev = lock;
				lock->lf_next = overlap;
			}
			overlap->lf_start = lock->lf_end + 1;
			lf_wakelock(overlap);
			break;
		}
		break;
	}
#ifdef LOCKF_DEBUG
	if (lockf_debug & 1) {
		lf_print("lf_setlock: got the lock", lock);
		lf_printlist("lf_setlock", lock);
	}
#endif /* LOCKF_DEBUG */
	return (0);
}

/*
 * Remove a byte-range lock on an inode.
 *
 * Generally, find the lock (or an overlap to that lock)
 * and remove it (or shrink it), then wakeup anyone we can.
 */
static int
lf_clearlock(unlock)
	register struct lockf *unlock;
{
	struct lockf **head = unlock->lf_head;
	register struct lockf *lf = *head;
	struct lockf *overlap, **prev;
	int ovcase;

	if (lf == NOLOCKF)
		return (0);
#ifdef LOCKF_DEBUG
	if (unlock->lf_type != F_UNLCK)
		panic("lf_clearlock: bad type");
	if (lockf_debug & 1)
		lf_print("lf_clearlock", unlock);
#endif /* LOCKF_DEBUG */
	prev = head;
	while ((ovcase = lf_findoverlap(lf, unlock, SELF, &prev, &overlap))) {
		/*
		 * Wakeup the list of locks to be retried.
		 */
		lf_wakelock(overlap);

		switch (ovcase) {

		case 1: /* overlap == lock */
			*prev = overlap->lf_next;
			FREE(overlap, M_LOCKF);
			break;

		case 2: /* overlap contains lock: split it */
			if (overlap->lf_start == unlock->lf_start) {
				overlap->lf_start = unlock->lf_end + 1;
				break;
			}
			lf_split(overlap, unlock);
			overlap->lf_next = unlock->lf_next;
			break;

		case 3: /* lock contains overlap */
			*prev = overlap->lf_next;
			lf = overlap->lf_next;
			free(overlap, M_LOCKF);
			continue;

		case 4: /* overlap starts before lock */
			overlap->lf_end = unlock->lf_start - 1;
			prev = &overlap->lf_next;
			lf = overlap->lf_next;
			continue;

		case 5: /* overlap ends after lock */
			overlap->lf_start = unlock->lf_end + 1;
			break;
		}
		break;
	}
#ifdef LOCKF_DEBUG
	if (lockf_debug & 1)
		lf_printlist("lf_clearlock", unlock);
#endif /* LOCKF_DEBUG */
	return (0);
}

/*
 * Check whether there is a blocking lock,
 * and if so return its process identifier.
 */
static int
lf_getlock(lock, fl)
	register struct lockf *lock;
	register struct flock *fl;
{
	register struct lockf *block;

#ifdef LOCKF_DEBUG
	if (lockf_debug & 1)
		lf_print("lf_getlock", lock);
#endif /* LOCKF_DEBUG */

	if ((block = lf_getblock(lock))) {
		fl->l_type = block->lf_type;
		fl->l_whence = SEEK_SET;
		fl->l_start = block->lf_start;
		if (block->lf_end == -1)
			fl->l_len = 0;
		else
			fl->l_len = block->lf_end - block->lf_start + 1;
		if (block->lf_flags & F_POSIX)
			fl->l_pid = ((struct proc *)(block->lf_id))->p_pid;
		else
			fl->l_pid = -1;
	} else {
		fl->l_type = F_UNLCK;
	}
	return (0);
}

/*
 * Walk the list of locks for an inode and
 * return the first blocking lock.
 */
static struct lockf *
lf_getblock(lock)
	register struct lockf *lock;
{
	struct lockf **prev, *overlap, *lf = *(lock->lf_head);
	int ovcase;

	prev = lock->lf_head;
	while ((ovcase = lf_findoverlap(lf, lock, OTHERS, &prev, &overlap))) {
		/*
		 * We've found an overlap, see if it blocks us
		 */
		if ((lock->lf_type == F_WRLCK || overlap->lf_type == F_WRLCK))
			return (overlap);
		/*
		 * Nope, point to the next one on the list and
		 * see if it blocks us
		 */
		lf = overlap->lf_next;
	}
	return (NOLOCKF);
}

/*
 * Walk the list of locks for an inode to
 * find an overlapping lock (if any).
 *
 * NOTE: this returns only the FIRST overlapping lock.  There
 *	 may be more than one.
 */
static int
lf_findoverlap(lf, lock, type, prev, overlap)
	register struct lockf *lf;
	struct lockf *lock;
	int type;
	struct lockf ***prev;
	struct lockf **overlap;
{
	off_t start, end;

	*overlap = lf;
	if (lf == NOLOCKF)
		return (0);
#ifdef LOCKF_DEBUG
	if (lockf_debug & 2)
		lf_print("lf_findoverlap: looking for overlap in", lock);
#endif /* LOCKF_DEBUG */
	start = lock->lf_start;
	end = lock->lf_end;
	while (lf != NOLOCKF) {
		if (((type & SELF) && lf->lf_id != lock->lf_id) ||
		    ((type & OTHERS) && lf->lf_id == lock->lf_id)) {
			*prev = &lf->lf_next;
			*overlap = lf = lf->lf_next;
			continue;
		}
#ifdef LOCKF_DEBUG
		if (lockf_debug & 2)
			lf_print("\tchecking", lf);
#endif /* LOCKF_DEBUG */
		/*
		 * OK, check for overlap
		 *
		 * Six cases:
		 *	0) no overlap
		 *	1) overlap == lock
		 *	2) overlap contains lock
		 *	3) lock contains overlap
		 *	4) overlap starts before lock
		 *	5) overlap ends after lock
		 */
		if ((lf->lf_end != -1 && start > lf->lf_end) ||
		    (end != -1 && lf->lf_start > end)) {
			/* Case 0 */
#ifdef LOCKF_DEBUG
			if (lockf_debug & 2)
				printf("no overlap\n");
#endif /* LOCKF_DEBUG */
			if ((type & SELF) && end != -1 && lf->lf_start > end)
				return (0);
			*prev = &lf->lf_next;
			*overlap = lf = lf->lf_next;
			continue;
		}
		if ((lf->lf_start == start) && (lf->lf_end == end)) {
			/* Case 1 */
#ifdef LOCKF_DEBUG
			if (lockf_debug & 2)
				printf("overlap == lock\n");
#endif /* LOCKF_DEBUG */
			return (1);
		}
		if ((lf->lf_start <= start) &&
		    (end != -1) &&
		    ((lf->lf_end >= end) || (lf->lf_end == -1))) {
			/* Case 2 */
#ifdef LOCKF_DEBUG
			if (lockf_debug & 2)
				printf("overlap contains lock\n");
#endif /* LOCKF_DEBUG */
			return (2);
		}
		if (start <= lf->lf_start &&
		           (end == -1 ||
			   (lf->lf_end != -1 && end >= lf->lf_end))) {
			/* Case 3 */
#ifdef LOCKF_DEBUG
			if (lockf_debug & 2)
				printf("lock contains overlap\n");
#endif /* LOCKF_DEBUG */
			return (3);
		}
		if ((lf->lf_start < start) &&
			((lf->lf_end >= start) || (lf->lf_end == -1))) {
			/* Case 4 */
#ifdef LOCKF_DEBUG
			if (lockf_debug & 2)
				printf("overlap starts before lock\n");
#endif /* LOCKF_DEBUG */
			return (4);
		}
		if ((lf->lf_start > start) &&
			(end != -1) &&
			((lf->lf_end > end) || (lf->lf_end == -1))) {
			/* Case 5 */
#ifdef LOCKF_DEBUG
			if (lockf_debug & 2)
				printf("overlap ends after lock\n");
#endif /* LOCKF_DEBUG */
			return (5);
		}
		panic("lf_findoverlap: default");
	}
	return (0);
}

/*
 * Add a lock to the end of the blocked list.
 */
static void
lf_addblock(blocklist, lock)
	struct lockf *blocklist;
	struct lockf *lock;
{
	register struct lockf *lf;

	if (lock == NOLOCKF)
		return;
#ifdef LOCKF_DEBUG
	if (lockf_debug & 2) {
		lf_print("addblock: adding", lock);
		lf_print("to blocked list of", blocklist);
	}
#endif /* LOCKF_DEBUG */
	if ((lf = blocklist->lf_block) == NOLOCKF) {
		blocklist->lf_block = lock;
		return;
	}
	while (lf->lf_block != NOLOCKF)
		lf = lf->lf_block;
	lf->lf_block = lock;
	return;
}

/*
 * Split a lock and a contained region into
 * two or three locks as necessary.
 */
static void
lf_split(lock1, lock2)
	register struct lockf *lock1;
	register struct lockf *lock2;
{
	register struct lockf *splitlock;

#ifdef LOCKF_DEBUG
	if (lockf_debug & 2) {
		lf_print("lf_split", lock1);
		lf_print("splitting from", lock2);
	}
#endif /* LOCKF_DEBUG */
	/*
	 * Check to see if spliting into only two pieces.
	 */
	if (lock1->lf_start == lock2->lf_start) {
		lock1->lf_start = lock2->lf_end + 1;
		lock2->lf_next = lock1;
		return;
	}
	if (lock1->lf_end == lock2->lf_end) {
		lock1->lf_end = lock2->lf_start - 1;
		lock2->lf_next = lock1->lf_next;
		lock1->lf_next = lock2;
		return;
	}
	/*
	 * Make a new lock consisting of the last part of
	 * the encompassing lock
	 */
	MALLOC(splitlock, struct lockf *, sizeof *splitlock, M_LOCKF, M_WAITOK);
	bcopy((caddr_t)lock1, (caddr_t)splitlock, sizeof *splitlock);
	splitlock->lf_start = lock2->lf_end + 1;
	splitlock->lf_block = NOLOCKF;
	lock1->lf_end = lock2->lf_start - 1;
	/*
	 * OK, now link it in
	 */
	splitlock->lf_next = lock1->lf_next;
	lock2->lf_next = splitlock;
	lock1->lf_next = lock2;
}

/*
 * Wakeup a blocklist
 */
static void
lf_wakelock(listhead)
	struct lockf *listhead;
{
        register struct lockf *blocklist, *wakelock;

	blocklist = listhead->lf_block;
	listhead->lf_block = NOLOCKF;
        while (blocklist != NOLOCKF) {
                wakelock = blocklist;
                blocklist = blocklist->lf_block;
		wakelock->lf_block = NOLOCKF;
		wakelock->lf_next = NOLOCKF;
#ifdef LOCKF_DEBUG
		if (lockf_debug & 2)
			lf_print("lf_wakelock: awakening", wakelock);
#endif /* LOCKF_DEBUG */
                wakeup((caddr_t)wakelock);
        }
}

#ifdef LOCKF_DEBUG
/*
 * Print out a lock.
 */
void
lf_print(tag, lock)
	char *tag;
	register struct lockf *lock;
{

	printf("%s: lock 0x%lx for ", tag, lock);
	if (lock->lf_flags & F_POSIX)
		printf("proc %d", ((struct proc *)(lock->lf_id))->p_pid);
	else
		printf("id 0x%x", lock->lf_id);
	printf(" in ino %d on dev <%d, %d>, %s, start %d, end %d",
		lock->lf_inode->i_number,
		major(lock->lf_inode->i_dev),
		minor(lock->lf_inode->i_dev),
		lock->lf_type == F_RDLCK ? "shared" :
		lock->lf_type == F_WRLCK ? "exclusive" :
		lock->lf_type == F_UNLCK ? "unlock" :
		"unknown", lock->lf_start, lock->lf_end);
	if (lock->lf_block)
		printf(" block 0x%x\n", lock->lf_block);
	else
		printf("\n");
}

void
lf_printlist(tag, lock)
	char *tag;
	struct lockf *lock;
{
	register struct lockf *lf;

	printf("%s: Lock list for ino %d on dev <%d, %d>:\n",
		tag, lock->lf_inode->i_number,
		major(lock->lf_inode->i_dev),
		minor(lock->lf_inode->i_dev));
	for (lf = lock->lf_inode->i_lockf; lf; lf = lf->lf_next) {
		printf("\tlock 0x%lx for ", lf);
		if (lf->lf_flags & F_POSIX)
			printf("proc %d", ((struct proc *)(lf->lf_id))->p_pid);
		else
			printf("id 0x%x", lf->lf_id);
		printf(", %s, start %d, end %d",
			lf->lf_type == F_RDLCK ? "shared" :
			lf->lf_type == F_WRLCK ? "exclusive" :
			lf->lf_type == F_UNLCK ? "unlock" :
			"unknown", lf->lf_start, lf->lf_end);
		if (lf->lf_block)
			printf(" block 0x%x\n", lf->lf_block);
		else
			printf("\n");
	}
}
#endif /* LOCKF_DEBUG */
