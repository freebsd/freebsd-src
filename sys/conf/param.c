/*
 * Copyright (c) 1980, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)param.c	8.3 (Berkeley) 8/20/94
 * $FreeBSD: src/sys/conf/param.c,v 1.36.2.2 2000/08/04 22:31:06 peter Exp $
 */

#include <stddef.h>

#include "opt_param.h"

#include <sys/param.h>

/*
 * System parameter formulae.
 *
 * This file is copied into each directory where we compile
 * the kernel; it should be modified there to suit local taste
 * if necessary.
 *
 * Compiled with -DMAXUSERS=xx
 */

#ifndef HZ
#define	HZ 100
#endif
int	hz = HZ;
int	tick = 1000000 / HZ;
int	tickadj = howmany(30000, 60 * HZ);	/* can adjust 30ms in 60s */
#define	NPROC (20 + 16 * MAXUSERS)
#ifndef MAXFILES
#define MAXFILES (NPROC*2)
#endif
int	maxproc = NPROC;			/* maximum # of processes */
int	maxprocperuid = NPROC-1;		/* maximum # of processes per user */
int	maxfiles = MAXFILES;			/* system wide open files limit */
int	maxfilesperproc = MAXFILES;		/* per-process open files limit */
int	ncallout = 16 + NPROC + MAXFILES;	/* maximum # of timer events */
int	mbuf_wait = 32;				/* mbuf sleep time in ticks */

/* maximum # of sf_bufs (sendfile(2) zero-copy virtual buffers) */
#ifndef NSFBUFS
#define	NSFBUFS (512 + MAXUSERS * 16)
#endif
int	nsfbufs = NSFBUFS;

/*
 * These may be set to nonzero here or by patching.
 * If they are nonzero at bootstrap time then they are
 * initialized to values dependent on the memory size.
 */
#ifdef	NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif
int	nswbuf = 0;

/*
 * These have to be allocated somewhere; allocating
 * them here forces loader errors if this file is omitted
 * (if they've been externed everywhere else; hah!).
 */
struct	buf *swbuf;
