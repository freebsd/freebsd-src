/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	from: @(#)kern_lock.c	8.1 (Berkeley) 6/11/93
 *
 *
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
 *
 * $FreeBSD$
 */

/*
 *	Locking primitives implementation
 */

#include <sys/param.h>
#include <sys/systm.h>

/* XXX */
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/lock.h>

/*
 *	Routine:	lock_init
 *	Function:
 *		Initialize a lock; required before use.
 *		Note that clients declare the "struct lock"
 *		variables and then initialize them, rather
 *		than getting a new one from this module.
 */
void
lock_init(l, can_sleep)
	lock_t l;
	boolean_t can_sleep;
{
	l->want_write = 0;
	l->want_upgrade = 0;
	l->waiting = 0;
	l->can_sleep = can_sleep;
	l->read_count = 0;
	l->proc = NULL;
	l->recursion_depth = 0;
}

void
lock_sleepable(l, can_sleep)
	lock_t l;
	boolean_t can_sleep;
{
	l->can_sleep = can_sleep;
}


/*
 *	Sleep locks.  These use the same data structure and algorithm
 *	as the spin locks, but the process sleeps while it is waiting
 *	for the lock.  These work on uniprocessor systems.
 */

void
lock_write(l)
	register lock_t l;
{
	if (l->proc == curproc) {
		/*
		 * Recursive lock.
		 */
		l->recursion_depth++;
		return;
	}
	/*
	 * Try to acquire the want_write bit.
	 */
	while (l->want_write) {
		if (l->can_sleep && l->want_write) {
			l->waiting = TRUE;
			tsleep(l, PVM, "lckwt1", 0);
		}
	}
	l->want_write = TRUE;

	/* Wait for readers (and upgrades) to finish */

	while ((l->read_count != 0) || l->want_upgrade) {
		if (l->can_sleep && (l->read_count != 0 || l->want_upgrade)) {
			l->waiting = TRUE;
			tsleep(l, PVM, "lckwt2", 0);
		}
	}
}

void
lock_done(l)
	register lock_t l;
{
	if (l->read_count != 0)
		l->read_count--;
	else if (l->recursion_depth != 0)
		l->recursion_depth--;
	else if (l->want_upgrade)
		l->want_upgrade = FALSE;
	else
		l->want_write = FALSE;

	if (l->waiting) {
		l->waiting = FALSE;
		wakeup(l);
	}
}

void
lock_read(l)
	register lock_t l;
{
	if (l->proc == curproc) {
		/*
		 * Recursive lock.
		 */
		l->read_count++;
		return;
	}
	while (l->want_write || l->want_upgrade) {
		if (l->can_sleep && (l->want_write || l->want_upgrade)) {
			l->waiting = TRUE;
			tsleep(l, PVM, "lockrd", 0);
		}
	}

	l->read_count++;
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
boolean_t
lock_read_to_write(l)
	register lock_t l;
{
	l->read_count--;

	if (l->proc == curproc) {
		/*
		 * Recursive lock.
		 */
		l->recursion_depth++;
		return (FALSE);
	}
	if (l->want_upgrade) {
		/*
		 * Someone else has requested upgrade. Since we've released a
		 * read lock, wake him up.
		 */
		if (l->waiting) {
			l->waiting = FALSE;
			wakeup(l);
		}
		return (TRUE);
	}
	l->want_upgrade = TRUE;

	while (l->read_count != 0) {
		if (l->can_sleep && l->read_count != 0) {
			l->waiting = TRUE;
			tsleep(l, PVM, "lckrw", 0);
		}
	}

	return (FALSE);
}

void
lock_write_to_read(l)
	register lock_t l;
{
	l->read_count++;
	if (l->recursion_depth != 0)
		l->recursion_depth--;
	else if (l->want_upgrade)
		l->want_upgrade = FALSE;
	else
		l->want_write = FALSE;

	if (l->waiting) {
		l->waiting = FALSE;
		wakeup(l);
	}
}


/*
 *	Routine:	lock_try_write
 *	Function:
 *		Tries to get a write lock.
 *
 *		Returns FALSE if the lock is not held on return.
 */

boolean_t
lock_try_write(l)
	register lock_t l;
{
	if (l->proc == curproc) {
		/*
		 * Recursive lock
		 */
		l->recursion_depth++;
		return (TRUE);
	}
	if (l->want_write || l->want_upgrade || l->read_count) {
		/*
		 * Can't get lock.
		 */
		return (FALSE);
	}
	/*
	 * Have lock.
	 */

	l->want_write = TRUE;
	return (TRUE);
}

/*
 *	Routine:	lock_try_read
 *	Function:
 *		Tries to get a read lock.
 *
 *		Returns FALSE if the lock is not held on return.
 */

boolean_t
lock_try_read(l)
	register lock_t l;
{
	if (l->proc == curproc) {
		/*
		 * Recursive lock
		 */
		l->read_count++;
		return (TRUE);
	}
	if (l->want_write || l->want_upgrade) {
		return (FALSE);
	}
	l->read_count++;
	return (TRUE);
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
boolean_t
lock_try_read_to_write(l)
	register lock_t l;
{
	if (l->proc == curproc) {
		/*
		 * Recursive lock
		 */
		l->read_count--;
		l->recursion_depth++;
		return (TRUE);
	}
	if (l->want_upgrade) {
		return (FALSE);
	}
	l->want_upgrade = TRUE;
	l->read_count--;

	while (l->read_count != 0) {
		l->waiting = TRUE;
		tsleep(l, PVM, "lcktrw", 0);
	}

	return (TRUE);
}

/*
 *	Allow a process that has a lock for write to acquire it
 *	recursively (for read, write, or update).
 */
void
lock_set_recursive(l)
	lock_t l;
{
	if (!l->want_write) {
		panic("lock_set_recursive: don't have write lock");
	}
	l->proc = curproc;
}

/*
 *	Prevent a lock from being re-acquired.
 */
void
lock_clear_recursive(l)
	lock_t l;
{
	if (l->proc != curproc) {
		panic("lock_clear_recursive: wrong proc");
	}
	if (l->recursion_depth == 0)
		l->proc = NULL;
}
