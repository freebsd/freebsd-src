/*-
 * Copyright (c) 1982, 1986, 1991 The Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)types.h	7.17 (Berkeley) 5/6/91
 *	$Id: types.h,v 1.2 1993/10/16 17:18:12 rgrimes Exp $
 */

#ifndef _TYPES_H_
#define	_TYPES_H_

typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned int	u_int;
typedef	unsigned long	u_long;
typedef	unsigned short	ushort;		/* Sys V compatibility */

typedef	char *	caddr_t;		/* core address */
typedef	long	daddr_t;		/* disk address */
typedef	short	dev_t;			/* device number */
typedef	u_long	ino_t;			/* inode number */
typedef	long	off_t;			/* file offset (should be a quad) */
typedef	u_short	nlink_t;		/* link count */
typedef	long	swblk_t;		/* swap offset */
typedef	long	segsz_t;		/* segment size */
typedef	u_short	uid_t;			/* user id */
typedef	u_short	gid_t;			/* group id */
typedef	short	pid_t;			/* process id */
typedef	u_short	mode_t;			/* permissions */
typedef u_long	fixpt_t;		/* fixed point number */

#ifndef _POSIX_SOURCE
typedef	struct	_uquad	{ u_long val[2]; } u_quad;
typedef	struct	_quad	{   long val[2]; } quad;
typedef	long *	qaddr_t;	/* should be typedef quad * qaddr_t; */

#define	major(x)	((int)(((u_int)(x) >> 8)&0xff))	/* major number */
#define	minor(x)	((int)((x)&0xff))		/* minor number */
#define	makedev(x,y)	((dev_t)(((x)<<8) | (y)))	/* create dev_t */
#endif

#include <machine/ansi.h>
#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
#include <machine/types.h>
#endif

#ifdef	_CLOCK_T_
typedef	_CLOCK_T_	clock_t;
#undef	_CLOCK_T_
#endif

#ifdef	_SIZE_T_
typedef	_SIZE_T_	size_t;
#undef	_SIZE_T_
#endif

#ifdef	_TIME_T_
typedef	_TIME_T_	time_t;
#undef	_TIME_T_
#endif

#ifndef _POSIX_SOURCE
#define	NBBY	8		/* number of bits in a byte */

/*
 * Select uses bit masks of file descriptors in longs.  These macros
 * manipulate such bit fields (the filesystem macros use chars).
 * FD_SETSIZE may be defined by the user, but the default here should
 * be enough for most uses.
 */
#ifndef	FD_SETSIZE
#define	FD_SETSIZE	256
#endif

typedef long	fd_mask;
#define NFDBITS	(sizeof(fd_mask) * NBBY)	/* bits per mask */

#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif

typedef	struct fd_set {
	fd_mask	fds_bits[howmany(FD_SETSIZE, NFDBITS)];
} fd_set;

#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define	FD_ZERO(p)	bzero((char *)(p), sizeof(*(p)))

#if defined(__STDC__) && defined(KERNEL)
/*
 * Forward structure declarations for function prototypes.
 * We include the common structures that cross subsystem boundaries here;
 * others are mostly used in the same place that the structure is defined.
 */
struct	proc;
struct	pgrp;
struct	ucred;
struct	rusage;
struct	file;
struct	buf;
struct	tty;
struct	uio;
#endif

#endif /* !_POSIX_SOURCE */
#endif /* !_TYPES_H_ */
