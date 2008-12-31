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
 * $FreeBSD: src/sys/powerpc/include/mutex.h,v 1.28.24.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _MACHINE_MUTEX_H_
#define _MACHINE_MUTEX_H_

#ifdef LOCORE

/*
 * Simple assembly macros to get and release non-recursive spin locks
 */
#define MTX_ENTER(lck)				\
	mfmsr	r10;				\ /* disable interrupts */
	rlwinm	r0, r10, 0, 17, 15;		\
	mtmsr	r0;				\
1:	li	r11, MTX_LOCK;			\ /* MTX_LOCK offset */
	lwarx	r0, r11, lck;			\ /* load current lock value */
	cmplwi	r0, r1, MTX_UNOWNED;		\ /* compare with unowned */
	beq	1;				\ /* if owned, loop */
	lwz	r0, PC_CURPROC(pcpup);		\ /* load curproc */
	stwcx.	r0, r11, lck;			\ /* attempt to store */
	beq	1;				\ /* loop if failed */
	sync;					\ /* sync */
	eieio;					\ /* sync */
	stw	r10, MTX_SAVEINTR(lck)		  /* save flags */

#define MTX_EXIT(lck)				\
	sync;					\ /* sync */
	eieio;					\ /* sync */
	li	r0, MTX_UNOWNED;		\ /* load in unowned */
	stw	r0, MTX_LOCK(lck);		\ /* store to lock */
	lwz	r0, MTX_SAVEINTR(lck);		\ /* load saved flags */
	mtmsr	r0				  /* enable interrupts */

#endif	/* !LOCORE */

#endif	/* __MACHINE_MUTEX_H */
