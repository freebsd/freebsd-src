/* 
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	from: @(#)kern_lock.c	7.4 (Berkeley) 4/21/91
 *	$Id: kern_lock.c,v 1.2 1993/10/16 16:20:13 rgrimes Exp $
 */

/*
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Avadis Tevanian, Jr., Michael Wayne Young
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 *	Locking primitives implementation
 */

#include "param.h"
#include "vm_param.h"
#include "lock.h"

/* XXX */
#include "proc.h"
typedef	int *thread_t;
#define	current_thread()	((thread_t)&curproc->p_thread)
/* XXX */

#if	NCPUS > 1

/*
 *	Module:		lock
 *	Function:
 *		Provide reader/writer sychronization.
 *	Implementation:
 *		Simple interlock on a bit.  Readers first interlock
 *		increment the reader count, then let go.  Writers hold
 *		the interlock (thus preventing further readers), and
 *		wait for already-accepted readers to go away.
 */

/*
 *	The simple-lock routines are the primitives out of which
 *	the lock package is built.  The implementation is left
 *	to the machine-dependent code.
 */

#ifdef	notdef
/*
 *	A sample implementation of simple locks.
 *	assumes:
 *		boolean_t test_and_set(boolean_t *)
 *			indivisibly sets the boolean to TRUE
 *			and returns its old value
 *		and that setting a boolean to FALSE is indivisible.
 */
/*
 *	simple_lock_init initializes a simple lock.  A simple lock
 *	may only be used for exclusive locks.
 */

void simple_lock_init(l)
	simple_lock_t	l;
{
	*(boolean_t *)l = FALSE;
}

void simple_lock(l)
	simple_lock_t	l;
{
	while (test_and_set((boolean_t *)l))
		continue;
}

void simple_unlock(l)
	simple_lock_t	l;
{
	*(boolean_t *)l = FALSE;
}

boolean_t simple_lock_try(l)
	simple_lock_t	l;
{
    	return (!test_and_set((boolean_t *)l));
}
#endif	notdef
#endif	NCPUS > 1

#if	NCPUS > 1
int lock_wait_time = 100;
#else	NCPUS > 1

	/*
	 * 	It is silly to spin on a uni-processor as if we
	 *	thought something magical would happen to the
	 *	want_write bit while we are executing.
	 */
int lock_wait_time = 0;
#endif	NCPUS > 1


/*
 *	Routine:	lock_init
 *	Function:
 *		Initialize a lock; required before use.
 *		Note that clients declare the "struct lock"
 *		variables and then initialize them, rather
 *		than getting a new one from this module.
 */
void lock_init(l, can_sleep)
	lock_t		l;
	boolean_t	can_sleep;
{
	bzero(l, sizeof(lock_data_t));
	simple_lock_init(&l->interlock);
	l->want_write = FALSE;
	l->want_upgrade = FALSE;
	l->read_count = 0;
	l->can_sleep = can_sleep;
	l->thread = (char *)-1;		/* XXX */
	l->recursion_depth = 0;
}

void lock_sleepable(l, can_sleep)
	lock_t		l;
	boolean_t	can_sleep;
{
	simple_lock(&l->interlock);
	l->can_sleep = can_sleep;
	simple_unlock(&l->interlock);
}


/*
 *	Sleep locks.  These use the same data structure and algorithm
 *	as the spin locks, but the process sleeps while it is waiting
 *	for the lock.  These work on uniprocessor systems.
 */

void lock_write(l)
	register lock_t	l;
{
	register int	i;

	simple_lock(&l->interlock);

	if (((thread_t)l->thread) == current_thread()) {
		/*
		 *	Recursive lock.
		 */
		l->recursion_depth++;
		simple_unlock(&l->interlock);
		return;
	}

	/*
	 *	Try to acquire the want_write bit.
	 */
	while (l->want_write) {
		if ((i = lock_wait_time) > 0) {
			simple_unlock(&l->interlock);
			while (--i > 0 && l->want_write)
				continue;
			simple_lock(&l->interlock);
		}

		if (l->can_sleep && l->want_write) {
			l->waiting = TRUE;
			thread_sleep((int) l, &l->interlock, FALSE);
			simple_lock(&l->interlock);
		}
	}
	l->want_write = TRUE;

	/* Wait for readers (and upgrades) to finish */

	while ((l->read_count != 0) || l->want_upgrade) {
		if ((i = lock_wait_time) > 0) {
			simple_unlock(&l->interlock);
			while (--i > 0 && (l->read_count != 0 ||
					l->want_upgrade))
				continue;
			simple_lock(&l->interlock);
		}

		if (l->can_sleep && (l->read_count != 0 || l->want_upgrade)) {
			l->waiting = TRUE;
			thread_sleep((int) l, &l->interlock, FALSE);
			simple_lock(&l->interlock);
		}
	}
	simple_unlock(&l->interlock);
}

void lock_done(l)
	register lock_t	l;
{
	simple_lock(&l->interlock);

	if (l->read_count != 0)
		l->read_count--;
	else
	if (l->recursion_depth != 0)
		l->recursion_depth--;
	else
	if (l->want_upgrade)
	 	l->want_upgrade = FALSE;
	else
	 	l->want_write = FALSE;

	if (l->waiting) {
		l->waiting = FALSE;
		thread_wakeup((int) l);
	}
	simple_unlock(&l->interlock);
}

void lock_read(l)
	register lock_t	l;
{
	register int	i;

	simple_lock(&l->interlock);

	if (((thread_t)l->thread) == current_thread()) {
		/*
		 *	Recursive lock.
		 */
		l->read_count++;
		simple_unlock(&l->interlock);
		return;
	}

	while (l->want_write || l->want_upgrade) {
		if ((i = lock_wait_time) > 0) {
			simple_unlock(&l->interlock);
			while (--i > 0 && (l->want_write || l->want_upgrade))
				continue;
			simple_lock(&l->interlock);
		}

		if (l->can_sleep && (l->want_write || l->want_upgrade)) {
			l->waiting = TRUE;
			thread_sleep((int) l, &l->interlock, FALSE);
			simple_lock(&l->interlock);
		}
	}

	l->read_count++;
	simple_unlock(&l->interlock);
}

/*
 *	Routine:	lock_read_to_write
 *	Function:
 *		Improves a read-only lock to one with
 *		write permission.  If another reader has
 *		already requested an upgrade to a write lock,
 *		no lock is held upon return.
 *
 *		Returns TRUE if the upgrade *failed*.
 */
boolean_t lock_read_to_write(l)
	register lock_t	l;
{
	register int	i;

	simple_lock(&l->interlock);

	l->read_count--;

	if (((thread_t)l->thread) == current_thread()) {
		/*
		 *	Recursive lock.
		 */
		l->recursion_depth++;
		simple_unlock(&l->interlock);
		return(FALSE);
	}

	if (l->want_upgrade) {
		/*
		 *	Someone else has requested upgrade.
		 *	Since we've released a read lock, wake
		 *	him up.
		 */
		if (l->waiting) {
			l->waiting = FALSE;
			thread_wakeup((int) l);
		}

		simple_unlock(&l->interlock);
		return (TRUE);
	}

	l->want_upgrade = TRUE;

	while (l->read_count != 0) {
		if ((i = lock_wait_time) > 0) {
			simple_unlock(&l->interlock);
			while (--i > 0 && l->read_count != 0)
				continue;
			simple_lock(&l->interlock);
		}

		if (l->can_sleep && l->read_count != 0) {
			l->waiting = TRUE;
			thread_sleep((int) l, &l->interlock, FALSE);
			simple_lock(&l->interlock);
		}
	}

	simple_unlock(&l->interlock);
	return (FALSE);
}

void lock_write_to_read(l)
	register lock_t	l;
{
	simple_lock(&l->interlock);

	l->read_count++;
	if (l->recursion_depth != 0)
		l->recursion_depth--;
	else
	if (l->want_upgrade)
		l->want_upgrade = FALSE;
	else
	 	l->want_write = FALSE;

	if (l->waiting) {
		l->waiting = FALSE;
		thread_wakeup((int) l);
	}

	simple_unlock(&l->interlock);
}


/*
 *	Routine:	lock_try_write
 *	Function:
 *		Tries to get a write lock.
 *
 *		Returns FALSE if the lock is not held on return.
 */

boolean_t lock_try_write(l)
	register lock_t	l;
{

	simple_lock(&l->interlock);

	if (((thread_t)l->thread) == current_thread()) {
		/*
		 *	Recursive lock
		 */
		l->recursion_depth++;
		simple_unlock(&l->interlock);
		return(TRUE);
	}

	if (l->want_write || l->want_upgrade || l->read_count) {
		/*
		 *	Can't get lock.
		 */
		simple_unlock(&l->interlock);
		return(FALSE);
	}

	/*
	 *	Have lock.
	 */

	l->want_write = TRUE;
	simple_unlock(&l->interlock);
	return(TRUE);
}

/*
 *	Routine:	lock_try_read
 *	Function:
 *		Tries to get a read lock.
 *
 *		Returns FALSE if the lock is not held on return.
 */

boolean_t lock_try_read(l)
	register lock_t	l;
{
	simple_lock(&l->interlock);

	if (((thread_t)l->thread) == current_thread()) {
		/*
		 *	Recursive lock
		 */
		l->read_count++;
		simple_unlock(&l->interlock);
		return(TRUE);
	}

	if (l->want_write || l->want_upgrade) {
		simple_unlock(&l->interlock);
		return(FALSE);
	}

	l->read_count++;
	simple_unlock(&l->interlock);
	return(TRUE);
}

/*
 *	Routine:	lock_try_read_to_write
 *	Function:
 *		Improves a read-only lock to one with
 *		write permission.  If another reader has
 *		already requested an upgrade to a write lock,
 *		the read lock is still held upon return.
 *
 *		Returns FALSE if the upgrade *failed*.
 */
boolean_t lock_try_read_to_write(l)
	register lock_t	l;
{

	simple_lock(&l->interlock);

	if (((thread_t)l->thread) == current_thread()) {
		/*
		 *	Recursive lock
		 */
		l->read_count--;
		l->recursion_depth++;
		simple_unlock(&l->interlock);
		return(TRUE);
	}

	if (l->want_upgrade) {
		simple_unlock(&l->interlock);
		return(FALSE);
	}
	l->want_upgrade = TRUE;
	l->read_count--;

	while (l->read_count != 0) {
		l->waiting = TRUE;
		thread_sleep((int) l, &l->interlock, FALSE);
		simple_lock(&l->interlock);
	}

	simple_unlock(&l->interlock);
	return(TRUE);
}

/*
 *	Allow a process that has a lock for write to acquire it
 *	recursively (for read, write, or update).
 */
void lock_set_recursive(l)
	lock_t		l;
{
	simple_lock(&l->interlock);
	if (!l->want_write) {
		panic("lock_set_recursive: don't have write lock");
	}
	l->thread = (char *) current_thread();
	simple_unlock(&l->interlock);
}

/*
 *	Prevent a lock from being re-acquired.
 */
void lock_clear_recursive(l)
	lock_t		l;
{
	simple_lock(&l->interlock);
	if (((thread_t) l->thread) != current_thread()) {
		panic("lock_clear_recursive: wrong thread");
	}
	if (l->recursion_depth == 0)
		l->thread = (char *)-1;		/* XXX */
	simple_unlock(&l->interlock);
}
