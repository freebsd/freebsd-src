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
 * $Id: param.c,v 1.27 1998/05/15 20:10:54 wollman Exp $
 */

#include "opt_sysvipc.h"
#include "opt_param.h"

#include <sys/param.h>

#ifdef SYSVSHM
#include <machine/vmparam.h>
#include <sys/shm.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVMSG
#include <sys/msg.h>
#endif

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
#define MAXFILES (NPROC*2)
int	maxproc = NPROC;			/* maximum # of processes */
int	maxprocperuid = NPROC-1;		/* maximum # of processes per user */
int	maxfiles = MAXFILES;			/* system wide open files limit */
int	maxfilesperproc = MAXFILES;		/* per-process open files limit */
int	ncallout = 16 + NPROC + MAXFILES;	/* maximum # of timer events */

/* maximum # of mbuf clusters */
#ifndef NMBCLUSTERS
#define	NMBCLUSTERS (512 + MAXUSERS * 16)
#endif
int	nmbclusters = NMBCLUSTERS;

#if MAXFILES > NMBCLUSTERS
#define	MAXSOCKETS MAXFILES
#else
#define	MAXSOCKETS NMBCLUSTERS
#endif
int	maxsockets = MAXSOCKETS;

/* allocate 1/4th amount of virtual address space for mbufs XXX */
int	nmbufs = NMBCLUSTERS * 4;

int	fscale = FSCALE;	/* kernel uses `FSCALE', user uses `fscale' */

/*
 * Values in support of System V compatible shared memory.	XXX
 */
#ifdef SYSVSHM
#ifndef SHMMAX
#define	SHMMAX	(SHMMAXPGS*PAGE_SIZE)
#endif
#ifndef SHMMIN
#define	SHMMIN	1
#endif
#ifndef SHMMNI
#define	SHMMNI	32			/* <= SHMMMNI in shm.h */
#endif
#ifndef SHMSEG
#define	SHMSEG	8
#endif
#ifndef SHMALL
#define	SHMALL	(SHMMAXPGS)
#endif

struct	shminfo shminfo = {
	SHMMAX,
	SHMMIN,
	SHMMNI,
	SHMSEG,
	SHMALL
};
#endif

/*
 * Values in support of System V compatible semaphores.
 */

#ifdef SYSVSEM

struct seminfo seminfo = {
                SEMMAP,         /* # of entries in semaphore map */
                SEMMNI,         /* # of semaphore identifiers */
                SEMMNS,         /* # of semaphores in system */
                SEMMNU,         /* # of undo structures in system */
                SEMMSL,         /* max # of semaphores per id */
                SEMOPM,         /* max # of operations per semop call */
                SEMUME,         /* max # of undo entries per process */
                SEMUSZ,         /* size in bytes of undo structure */
                SEMVMX,         /* semaphore maximum value */
                SEMAEM          /* adjust on exit max value */
};
#endif

/*
 * Values in support of System V compatible messages.
 */

#ifdef SYSVMSG

struct msginfo msginfo = {
                MSGMAX,         /* max chars in a message */
                MSGMNI,         /* # of message queue identifiers */
                MSGMNB,         /* max chars in a queue */
                MSGTQL,         /* max messages in system */
                MSGSSZ,         /* size of a message segment */
                		/* (must be small power of 2 greater than 4) */
                MSGSEG          /* number of message segments */
};
#endif

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
