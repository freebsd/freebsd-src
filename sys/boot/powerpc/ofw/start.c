/* $FreeBSD$ */
/* $NetBSD: Locore.c,v 1.7 2000/08/20 07:04:59 tsubai Exp $ */

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stand.h>
#include "libofw.h"

void startup(void *, int, int (*)(void *), char *, int);

#ifdef XCOFF_GLUE
asm("				\n\
	.text			\n\
	.globl	_entry		\n\
_entry:				\n\
	.long	_start,0,0	\n\
");
#endif

__asm("				\n\
	.data			\n\
stack:				\n\
	.space	16388		\n\
				\n\
	.text			\n\
	.globl	_start		\n\
_start:				\n\
	li	%r8,0		\n\
	li	%r9,0x100	\n\
	mtctr	%r9		\n\
1:				\n\
	dcbf	%r0,%r8		\n\
	icbi	%r0,%r8		\n\
	addi	%r8,%r8,0x20	\n\
	bdnz	1b		\n\
	sync			\n\
	isync			\n\
				\n\
	lis	%r1,stack@ha	\n\
	addi	%r1,%r1,stack@l	\n\
	addi	%r1,%r1,8192	\n\
				\n\
	mfmsr	%r8		\n\
	li	%r0,0		\n\
	mtmsr	%r0		\n\
	isync			\n\
				\n\
	mtibatu	0,%r0		\n\
	mtibatu	1,%r0		\n\
	mtibatu	2,%r0		\n\
	mtibatu	3,%r0		\n\
	mtdbatu	0,%r0		\n\
	mtdbatu	1,%r0		\n\
	mtdbatu	2,%r0		\n\
	mtdbatu	3,%r0		\n\
				\n\
	li	%r9,0x12     /* BATL(0, BAT_M, BAT_PP_RW) */ \n\
	mtibatl	0,%r9		\n\
	mtdbatl	0,%r9		\n\
	li	%r9,0x1ffe   /* BATU(0, BAT_BL_256M, BAT_Vs) */ \n\
	mtibatu	0,%r9		\n\
	mtdbatu	0,%r9		\n\
	isync			\n\
				\n\
	mtmsr	%r8		\n\
	isync			\n\
				\n\
	b	startup		\n\
");

void
startup(void *vpd, int res, int (*openfirm)(void *), char *arg, int argl)
{
	main(openfirm);
}
