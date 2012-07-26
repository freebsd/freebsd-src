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
 * $FreeBSD$
 */

#ifndef _SYS_CALLOUT_H_
#define _SYS_CALLOUT_H_

#include <sys/_callout.h>

#define	CALLOUT_LOCAL_ALLOC	0x0001 /* was allocated from callfree */
#define	CALLOUT_ACTIVE		0x0002 /* callout is currently active */
#define	CALLOUT_PENDING		0x0004 /* callout is waiting for timeout */
#define	CALLOUT_MPSAFE		0x0008 /* callout handler is mp safe */
#define	CALLOUT_RETURNUNLOCKED	0x0010 /* handler returns with mtx unlocked */
#define	CALLOUT_SHAREDLOCK	0x0020 /* callout lock held in shared mode */
#define	CALLOUT_DFRMIGRATION	0x0040 /* callout in deferred migration mode */
#define	CALLOUT_PROCESSED	0x0080 /* callout in wheel or processing list? */
#define	CALLOUT_DIRECT 		0x0100 /* allow exec from hw int context */

#define	C_DIRECT_EXEC		0x0001 /* direct execution of callout */
#define	C_P1S			0x0002 /* fields related to precision */ 
#define	C_P500MS		0x0006 	
#define	C_P250MS		0x000a 	
#define	C_P125MS		0x000e
#define	C_P64MS			0x0012
#define	C_P32MS			0x0016
#define	C_P16MS			0x001a
#define	C_P8MS			0x001e
#define	C_P4MS			0x0022
#define	C_P2MS			0x0026
#define	C_P1MS			0x002a
#define	C_P500US		0x002e
#define	C_P250US		0x0032
#define	C_P125US		0x0036
#define	C_P64US			0x003a
#define	C_P32US			0x003e
#define	C_P16US			0x0042
#define	C_P8US			0x0046
#define	C_P4US			0x004a
#define	C_P2US			0x004e
#define	PRECISION_BITS		7	
#define	PRECISION_RANGE		((1 << PRECISION_BITS) - 1)	

struct callout_handle {
	struct callout *callout;
};

#ifdef _KERNEL
extern int ncallout;

#define	callout_active(c)	((c)->c_flags & CALLOUT_ACTIVE)
#define	callout_deactivate(c)	((c)->c_flags &= ~CALLOUT_ACTIVE)
#define	callout_drain(c)	_callout_stop_safe(c, 1)
void	callout_init(struct callout *, int);
void	_callout_init_lock(struct callout *, struct lock_object *, int);
#define	callout_init_mtx(c, mtx, flags)					\
	_callout_init_lock((c), ((mtx) != NULL) ? &(mtx)->lock_object :	\
	    NULL, (flags))
#define	callout_init_rw(c, rw, flags)					\
	_callout_init_lock((c), ((rw) != NULL) ? &(rw)->lock_object :	\
	   NULL, (flags))
#define	callout_pending(c)	((c)->c_flags & CALLOUT_PENDING)
int	_callout_reset_on(struct callout *, struct bintime *, int,
	    void (*)(void *), void *, int, int);
#define	callout_reset_on(c, to_ticks, fn, arg, cpu)			\
    _callout_reset_on((c), (NULL), (to_ticks), (fn), (arg), (cpu),	\
        (0))
#define callout_reset_flags_on(c, to_ticks, fn, arg, cpu, flags)	\
    _callout_reset_on((c), (NULL), (to_ticks), (fn), (arg), (cpu),	\
        (flags))			
#define callout_reset_bt_on(c, bt, fn, arg, cpu, flags)			\
    _callout_reset_on((c), (bt), (0), (fn), (arg), (cpu), (flags)) 
#define	callout_reset(c, on_tick, fn, arg)				\
    callout_reset_on((c), (on_tick), (fn), (arg), (c)->c_cpu)
#define	callout_reset_curcpu(c, on_tick, fn, arg)			\
    callout_reset_on((c), (on_tick), (fn), (arg), PCPU_GET(cpuid))
int	callout_schedule(struct callout *, int);
int	callout_schedule_on(struct callout *, int, int);
#define	callout_schedule_curcpu(c, on_tick)				\
    callout_schedule_on((c), (on_tick), PCPU_GET(cpuid))
#define	callout_stop(c)		_callout_stop_safe(c, 0)
int	_callout_stop_safe(struct callout *, int);
void	callout_process(struct bintime *);
extern void (*callout_new_inserted)(int cpu, struct bintime bt);

#endif

#endif /* _SYS_CALLOUT_H_ */
