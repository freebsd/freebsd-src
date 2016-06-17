/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

#include <linux/time.h>
#include <linux/sched.h>
#include <asm/system.h>
#include <linux/interrupt.h>
#include <asm/current.h>

#include "mrlock.h"


#if USE_RW_WAIT_QUEUE_SPINLOCK
# define wq_write_lock	write_lock
#else
# define wq_write_lock	spin_lock
#endif

/*
 * We don't seem to need lock_type (only one supported), name, or
 * sequence. But, XFS will pass it so let's leave them here for now.
 */
/* ARGSUSED */
void
mrlock_init(mrlock_t *mrp, int lock_type, char *name, long sequence)
{
	mrp->mr_count = 0;
	mrp->mr_reads_waiting = 0;
	mrp->mr_writes_waiting = 0;
	init_waitqueue_head(&mrp->mr_readerq);
	init_waitqueue_head(&mrp->mr_writerq);
	mrp->mr_lock = SPIN_LOCK_UNLOCKED;
}

/*
 * Macros to lock/unlock the mrlock_t.
 */

#define MRLOCK(m)		spin_lock(&(m)->mr_lock);
#define MRUNLOCK(m)		spin_unlock(&(m)->mr_lock);


/*
 * lock_wait should never be called in an interrupt thread.
 *
 * mrlocks can sleep (i.e. call schedule) and so they can't ever
 * be called from an interrupt thread.
 *
 * threads that wake-up should also never be invoked from interrupt threads.
 *
 * But, waitqueue_lock is locked from interrupt threads - and we are
 * called with interrupts disabled, so it is all OK.
 */

/* ARGSUSED */
void
lock_wait(wait_queue_head_t *q, spinlock_t *lock, int rw)
{
	DECLARE_WAITQUEUE( wait, current );

	__set_current_state(TASK_UNINTERRUPTIBLE);

	wq_write_lock(&q->lock);
	if (rw) {
		__add_wait_queue_tail(q, &wait);
	} else {
		__add_wait_queue(q, &wait);
	}

	wq_write_unlock(&q->lock);
	spin_unlock(lock);

	schedule();

	wq_write_lock(&q->lock);
	__remove_wait_queue(q, &wait);
	wq_write_unlock(&q->lock);

	spin_lock(lock);

	/* return with lock held */
}

/* ARGSUSED */
void
mrfree(mrlock_t *mrp)
{
}

/* ARGSUSED */
void
mrlock(mrlock_t *mrp, int type, int flags)
{
	if (type == MR_ACCESS)
		mraccess(mrp);
	else
		mrupdate(mrp);
}

/* ARGSUSED */
void
mraccessf(mrlock_t *mrp, int flags)
{
	MRLOCK(mrp);
	if(mrp->mr_writes_waiting > 0) {
		mrp->mr_reads_waiting++;
		lock_wait(&mrp->mr_readerq, &mrp->mr_lock, 0);
		mrp->mr_reads_waiting--;
	}
	while (mrp->mr_count < 0) {
		mrp->mr_reads_waiting++;
		lock_wait(&mrp->mr_readerq, &mrp->mr_lock, 0);
		mrp->mr_reads_waiting--;
	}
	mrp->mr_count++;
	MRUNLOCK(mrp);
}

/* ARGSUSED */
void
mrupdatef(mrlock_t *mrp, int flags)
{
	MRLOCK(mrp);
	while(mrp->mr_count) {
		mrp->mr_writes_waiting++;
		lock_wait(&mrp->mr_writerq, &mrp->mr_lock, 1);
		mrp->mr_writes_waiting--;
	}

	mrp->mr_count = -1; /* writer on it */
	MRUNLOCK(mrp);
}

int
mrtryaccess(mrlock_t *mrp)
{
	MRLOCK(mrp);
	/*
	 * If anyone is waiting for update access or the lock is held for update
	 * fail the request.
	 */
	if(mrp->mr_writes_waiting > 0 || mrp->mr_count < 0) {
		MRUNLOCK(mrp);
		return 0;
	}
	mrp->mr_count++;
	MRUNLOCK(mrp);
	return 1;
}

int
mrtrypromote(mrlock_t *mrp)
{
	MRLOCK(mrp);

	if(mrp->mr_count == 1) { /* We are the only thread with the lock */
		mrp->mr_count = -1; /* writer on it */
		MRUNLOCK(mrp);
		return 1;
	}

	MRUNLOCK(mrp);
	return 0;
}

int
mrtryupdate(mrlock_t *mrp)
{
	MRLOCK(mrp);

	if(mrp->mr_count) {
		MRUNLOCK(mrp);
		return 0;
	}

	mrp->mr_count = -1; /* writer on it */
	MRUNLOCK(mrp);
	return 1;
}

static __inline__ void mrwake(mrlock_t *mrp)
{
	/*
	 * First, if the count is now 0, we need to wake-up anyone waiting.
	 */
	if (!mrp->mr_count) {
		if (mrp->mr_writes_waiting) {	/* Wake-up first writer waiting */
			wake_up(&mrp->mr_writerq);
		} else if (mrp->mr_reads_waiting) {	/* Wakeup any readers waiting */
			wake_up(&mrp->mr_readerq);
		}
	}
}

void
mraccunlock(mrlock_t *mrp)
{
	MRLOCK(mrp);
	mrp->mr_count--;
	mrwake(mrp);
	MRUNLOCK(mrp);
}

void
mrunlock(mrlock_t *mrp)
{
	MRLOCK(mrp);
	if (mrp->mr_count < 0) {
		mrp->mr_count = 0;
	} else {
		mrp->mr_count--;
	}
	mrwake(mrp);
	MRUNLOCK(mrp);
}

int
ismrlocked(mrlock_t *mrp, int type)	/* No need to lock since info can change */
{
	if (type == MR_ACCESS)
		return (mrp->mr_count > 0); /* Read lock */
	else if (type == MR_UPDATE)
		return (mrp->mr_count < 0); /* Write lock */
	else if (type == (MR_UPDATE | MR_ACCESS))
		return (mrp->mr_count);	/* Any type of lock held */
	else /* Any waiters */
		return (mrp->mr_reads_waiting | mrp->mr_writes_waiting);
}

/*
 * Demote from update to access. We better be the only thread with the
 * lock in update mode so it should be easy to set to 1.
 * Wake-up any readers waiting.
 */

void
mrdemote(mrlock_t *mrp)
{
	MRLOCK(mrp);
	mrp->mr_count = 1;
	if (mrp->mr_reads_waiting) {	/* Wakeup all readers waiting */
		wake_up(&mrp->mr_readerq);
	}
	MRUNLOCK(mrp);
}
