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
 *	from: @(#)lock.h	8.1 (Berkeley) 6/11/93
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
 * $Id: lock.h,v 1.4 1995/07/13 08:48:14 davidg Exp $
 */

/*
 *	Locking primitives definitions
 */

#ifndef	_LOCK_H_
#define	_LOCK_H_

/*
 *	The general lock structure.  Provides for multiple readers,
 *	upgrading from read to write, and sleeping until the lock
 *	can be gained.
 */
struct lock {
	/*
	 * Only the "interlock" field is used for hardware exclusion; other
	 * fields are modified with normal instructions after acquiring the
	 * interlock bit.
	 */
	boolean_t want_write;	/* Writer is waiting, or locked for write */
	boolean_t want_upgrade;	/* Read-to-write upgrade waiting */
	boolean_t waiting;	/* Someone is sleeping on lock */
	boolean_t can_sleep;	/* Can attempts to lock go to sleep */
	int read_count;		/* Number of accepted readers */
	struct proc *proc;	/* If recursive locking, process that has lock */
	int recursion_depth;	/* Depth of recursion */
};

typedef struct lock lock_data_t;
typedef struct lock *lock_t;

/* Sleep locks must work even if no multiprocessing. */

#define	lock_read_done(l)	lock_done(l)
#define	lock_write_done(l)	lock_done(l)

#ifdef KERNEL
void lock_clear_recursive __P((lock_t));
void lock_done __P((lock_t));
void lock_init __P((lock_t, boolean_t));
void lock_read __P((lock_t));
boolean_t lock_read_to_write __P((lock_t));
void lock_set_recursive __P((lock_t));
void lock_sleepable __P((lock_t, boolean_t));
boolean_t lock_try_read __P((lock_t));
boolean_t lock_try_read_to_write __P((lock_t));
boolean_t lock_try_write __P((lock_t));
void lock_write __P((lock_t));
void lock_write_to_read __P((lock_t));
#endif

#endif				/* !_LOCK_H_ */
