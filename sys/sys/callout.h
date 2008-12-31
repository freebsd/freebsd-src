/*-
 * Copyright (c) 1990, 1993
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
 *	@(#)callout.h	8.2 (Berkeley) 1/21/94
 * $FreeBSD: src/sys/sys/callout.h,v 1.31.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _SYS_CALLOUT_H_
#define _SYS_CALLOUT_H_

#include <sys/queue.h>

struct mtx;

SLIST_HEAD(callout_list, callout);
TAILQ_HEAD(callout_tailq, callout);

struct callout {
	union {
		SLIST_ENTRY(callout) sle;
		TAILQ_ENTRY(callout) tqe;
	} c_links;
	int	c_time;				/* ticks to the event */
	void	*c_arg;				/* function argument */
	void	(*c_func)(void *);	/* function to call */
	struct mtx *c_mtx;			/* mutex to lock */
	int	c_flags;			/* state of this entry */
};

#define	CALLOUT_LOCAL_ALLOC	0x0001 /* was allocated from callfree */
#define	CALLOUT_ACTIVE		0x0002 /* callout is currently active */
#define	CALLOUT_PENDING		0x0004 /* callout is waiting for timeout */
#define	CALLOUT_MPSAFE		0x0008 /* callout handler is mp safe */
#define	CALLOUT_RETURNUNLOCKED	0x0010 /* handler returns with mtx unlocked */

struct callout_handle {
	struct callout *callout;
};

#ifdef _KERNEL
extern struct callout_list callfree;
extern struct callout *callout;
extern int ncallout;
extern struct callout_tailq *callwheel;
extern int callwheelsize, callwheelbits, callwheelmask, softticks;
extern struct mtx callout_lock;

#define	callout_active(c)	((c)->c_flags & CALLOUT_ACTIVE)
#define	callout_deactivate(c)	((c)->c_flags &= ~CALLOUT_ACTIVE)
#define	callout_drain(c)	_callout_stop_safe(c, 1)
void	callout_init(struct callout *, int);
void	callout_init_mtx(struct callout *, struct mtx *, int);
#define	callout_pending(c)	((c)->c_flags & CALLOUT_PENDING)
int	callout_reset(struct callout *, int, void (*)(void *), void *);
#define	callout_stop(c)		_callout_stop_safe(c, 0)
int	_callout_stop_safe(struct callout *, int);

#endif

#endif /* _SYS_CALLOUT_H_ */
