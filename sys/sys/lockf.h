/*
 * Copyright (c) 1991, 1993
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
 *	@(#)lockf.h	8.1 (Berkeley) 6/11/93
 * $FreeBSD$
 */

#ifndef _SYS_LOCKF_H_
#define	_SYS_LOCKF_H_

#include <sys/queue.h>

struct vop_advlock_args;

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_LOCKF);
#endif

/*
 * The lockf structure is a kernel structure which contains the information
 * associated with a byte range lock.  The lockf structures are linked into
 * the inode structure. Locks are sorted by the starting byte of the lock for
 * efficiency.
 */
TAILQ_HEAD(locklist, lockf);

struct lockf {
	short	lf_flags;	    /* Semantics: F_POSIX, F_FLOCK, F_WAIT */
	short	lf_type;	    /* Lock type: F_RDLCK, F_WRLCK */
	off_t	lf_start;	    /* Byte # of the start of the lock */
	off_t	lf_end;		    /* Byte # of the end of the lock (-1=EOF) */
	caddr_t	lf_id;		    /* Id of the resource holding the lock */
	struct	lockf **lf_head;    /* Back pointer to the head of the locf list */
	struct	inode *lf_inode;    /* Back pointer to the inode */
	struct	lockf *lf_next;	    /* Pointer to the next lock on this inode */
	struct	locklist lf_blkhd;  /* List of requests blocked on this lock */
	TAILQ_ENTRY(lockf) lf_block;/* A request waiting for a lock */
};

/* Maximum length of sleep chains to traverse to try and detect deadlock. */
#define MAXDEPTH 50

int	 lf_advlock __P((struct vop_advlock_args *, struct lockf **, u_quad_t));

#ifdef LOCKF_DEBUG
void	lf_print __P((char *, struct lockf *));
void	lf_printlist __P((char *, struct lockf *));
#endif

#endif /* !_SYS_LOCKF_H_ */
