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
 *	from: @(#)lock.h	7.3 (Berkeley) 4/21/91
 *	$Id
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
 *	Locking primitives definitions
 */

#ifndef	_LOCK_H_
#define	_LOCK_H_

#define	NCPUS	1		/* XXX */

/*
 *	A simple spin lock.
 */

struct slock {
	int		lock_data;	/* in general 1 bit is sufficient */
};

typedef struct slock	simple_lock_data_t;
typedef struct slock	*simple_lock_t;

/*
 *	The general lock structure.  Provides for multiple readers,
 *	upgrading from read to write, and sleeping until the lock
 *	can be gained.
 */

struct lock {
#ifdef	vax
	/*
	 *	Efficient VAX implementation -- see field description below.
	 */
	unsigned int	read_count:16,
			want_upgrade:1,
			want_write:1,
			waiting:1,
			can_sleep:1,
			:0;

	simple_lock_data_t	interlock;
#else	vax
#ifdef	ns32000
	/*
	 *	Efficient ns32000 implementation --
	 *	see field description below.
	 */
	simple_lock_data_t	interlock;
	unsigned int	read_count:16,
			want_upgrade:1,
			want_write:1,
			waiting:1,
			can_sleep:1,
			:0;

#else	ns32000
	/*	Only the "interlock" field is used for hardware exclusion;
	 *	other fields are modified with normal instructions after
	 *	acquiring the interlock bit.
	 */
	simple_lock_data_t
			interlock;	/* Interlock for remaining fields */
	boolean_t	want_write;	/* Writer is waiting, or locked for write */
	boolean_t	want_upgrade;	/* Read-to-write upgrade waiting */
	boolean_t	waiting;	/* Someone is sleeping on lock */
	boolean_t	can_sleep;	/* Can attempts to lock go to sleep */
	int		read_count;	/* Number of accepted readers */
#endif	/* ns32000 */
#endif	/* vax */
	char		*thread;	/* Thread that has lock, if recursive locking allowed */
					/* (should be thread_t, but but we then have mutually
					   recursive definitions) */
	int		recursion_depth;/* Depth of recursion */
};

typedef struct lock	lock_data_t;
typedef struct lock	*lock_t;

#if	NCPUS > 1
void		simple_lock_init();
void		simple_lock();
void		simple_unlock();
boolean_t	simple_lock_try();
#else	NCPUS > 1
/*
 *	No multiprocessor locking is necessary.
 */
#define simple_lock_init(l)
#define simple_lock(l)
#define simple_unlock(l)
#define simple_lock_try(l)	(1)	/* always succeeds */
#endif	/* NCPUS > 1 */

/* Sleep locks must work even if no multiprocessing */

void		lock_init();
void		lock_sleepable();
void		lock_write();
void		lock_read();
void		lock_done();
boolean_t	lock_read_to_write();
void		lock_write_to_read();
boolean_t	lock_try_write();
boolean_t	lock_try_read();
boolean_t	lock_try_read_to_write();

#define	lock_read_done(l)	lock_done(l)
#define	lock_write_done(l)	lock_done(l)

void		lock_set_recursive();
void		lock_clear_recursive();

#endif /* !_LOCK_H_ */
