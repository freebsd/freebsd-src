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

static int stack[8192/4 + 4];

#ifdef XCOFF_GLUE
asm("
	.text
	.globl	_entry
_entry:
	.long	_start,0,0
");
#endif

asm("
	.text
	.globl	_start
_start:
	li	8,0
	li	9,0x100
	mtctr	9
1:
	dcbf	0,8
	icbi	0,8
	addi	8,8,0x20
	bdnz	1b
	sync
	isync

	lis	1,stack@ha
	addi	1,1,stack@l
	addi	1,1,8192

	mfmsr	8
	li	0,0
	mtmsr	0
	isync

	mtibatu	0,0
	mtibatu	1,0
	mtibatu	2,0
	mtibatu	3,0
	mtdbatu	0,0
	mtdbatu	1,0
	mtdbatu	2,0
	mtdbatu	3,0

	li	9,0x12		/* BATL(0, BAT_M, BAT_PP_RW) */
	mtibatl	0,9
	mtdbatl	0,9
	li	9,0x1ffe	/* BATU(0, BAT_BL_256M, BAT_Vs) */
	mtibatu	0,9
	mtdbatu	0,9
	isync

	mtmsr	8
	isync

	b	startup
");

void
startup(void *vpd, int res, int (*openfirm)(void *), char *arg, int argl)
{
	main(openfirm);
}
