/*
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)lockf.h	7.1 (Berkeley) 2/1/91
 *	$Id: lockf.h,v 1.5 1993/12/19 00:55:40 wollman Exp $
 */

#ifndef _UFS_LOCKF_H_
#define _UFS_LOCKF_H_ 1

/*
 * The lockf structure is a kernel structure which contains all the
 * information associated with a byte range lock. The lockf structures
 * are linked into the inode structure. Locks are sorted by the starting
 * byte of the lock for efficiency.
 */
struct lockf {
	short	lf_flags;	 /* Lock semantics: F_POSIX, F_FLOCK, F_WAIT */
	short	lf_type;	 /* Lock type: F_RDLCK, F_WRLCK */
	off_t	lf_start;	 /* The byte # of the start of the lock */
	off_t	lf_end;		 /* The byte # of the end of the lock (-1=EOF)*/
	caddr_t	lf_id;		 /* The id of the resource holding the lock */
	struct	lockf **lf_head; /* Back pointer to the head of lockf list */
	struct	lockf *lf_next;	 /* A pointer to the next lock on this inode */
	struct	lockf *lf_block; /* The list of blocked locks */
};

/*
 * Maximum length of sleep chains to traverse to try and detect deadlock.
 */
#define MAXDEPTH 50

#ifdef	KERNEL
/*
 * Public lock manipulation routines
 */
extern int lf_advlock(struct lockf **, u_long, caddr_t, int, struct flock *,
		      int);
extern int lf_setlock(struct lockf *);
extern int lf_clearlock(struct lockf *);
extern int lf_getlock(struct lockf *, struct flock *);
extern struct lockf *lf_getblock(struct lockf *);
extern int lf_findoverlap(struct lockf *, struct lockf *, int, 
			  struct lockf ***, struct lockf **);

#ifdef	LOCKF_DEBUG
extern int lockf_debug;
#endif	LOCKF_DEBUG
#endif	KERNEL
#endif /* _UFS_LOCKF_H_ */
