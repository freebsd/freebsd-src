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

#include <machine/ia64_cpu.h>

#ifndef LOCORE

#ifdef _KERNEL

#define	mtx_intr_enable(mutex)	do (mutex)->mtx_savecrit |= IA64_PSR_I; while (0)

#endif	/* _KERNEL */

#else	/* !LOCORE */

/*
 * Simple assembly macros to get and release non-recursive spin locks
 */
#define MTX_ENTER(lck, rPSR, rOLD, rNEW, rLCK)		\
	mov	rPSR=psr ;				\
	mov	rNEW=pcpup ;				\
	addl	rLCK=@ltoff(lck),gp ;;			\
	ld8	rLCK=[rLCK] ;;				\
	add	rLCK=MTX_LOCK,rLCK ;;			\
	rsm	psr.i ;					\
	mov	ar.ccv=MTX_UNOWNED ;			\
	add	rNEW=PC_CURTHREAD,rNEW ;;		\
	ld8	rNEW=[rNEW] ;;				\
1:	cmpxchg8.acq rOLD=[rLCK],rNEW,ar.ccv ;;		\
	cmp.eq	p1,p0=MTX_UNOWNED,rOLD ;;		\
(p1)	br.cond.spnt.few 1b ;;				\
	addl	rLCK=@ltoff(lck),gp ;;			\
	ld8	rLCK=[rLCK] ;;				\
	add	rLCK=MTX_SAVEINTR,rLCK ;;		\
	st4	[rLCK]=rPSR

#define MTX_EXIT(lck, rTMP, rLCK)			\
	mov	rTMP=MTX_UNOWNED ;			\
	addl	rLCK=@ltoff(lck),gp;;			\
	ld8	rLCK=[rLCK];;				\
	add	rLCK=MTX_LOCK,rLCK;;			\
	st8.rel	[rLCK]=rTMP,MTX_SAVEINTR-MTX_LOCK ;;	\
	ld4	rTMP=[rLCK] ;;				\
	mov	psr.l=rTMP ;;				\
	srlz.d

#endif	/* !LOCORE */

#endif	/* __MACHINE_MUTEX_H */
