/*-
 * Copyright (c) 1997 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from BSDI $Id: mutex.h,v 2.7.2.35 2000/04/27 03:10:26 cp Exp $
 * $FreeBSD$
 */

#ifndef _MACHINE_MUTEX_H_
#define _MACHINE_MUTEX_H_

#ifndef LOCORE

#ifdef _KERNEL

#define	mtx_intr_enable(mutex)	(mutex)->mtx_savecrit = ALPHA_PSL_IPL_0

#endif	/* _KERNEL */

#else	/* !LOCORE */

/*
 * Simple assembly macros to get and release non-recursive spin locks
 *
 * XXX: These are presently unused and cannot be used right now. Need to be
 *	re-written (they are wrong). If you plan to use this and still see
 *	this message, know not to unless you fix them first! :-)
 */
#define MTX_ENTER(lck)				\
	ldiq	a0, ALPHA_PSL_IPL_HIGH;		\
	call_pal PAL_OSF1_swpipl;		\
1:	ldq_l	a0, lck+MTX_LOCK;		\
	cmpeq	a0, MTX_UNOWNED, a1;		\
	beq	a1, 1b;				\
	ldq	a0, PC_CURPROC(globalp);	\
	stq_c	a0, lck+MTX_LOCK;		\
	beq	a0, 1b;				\
	mb;					\
	stl	v0, lck+MTX_SAVEINTR

#define MTX_EXIT(lck)				\
	mb;					\
	ldiq	a0, MTX_UNOWNED;		\
	stq	a0, lck+MTX_LOCK;		\
	ldl	a0, lck+MTX_SAVEINTR;		\
	call_pal PAL_OSF1_swpipl

#endif	/* !LOCORE */

#endif	/* __MACHINE_MUTEX_H */
