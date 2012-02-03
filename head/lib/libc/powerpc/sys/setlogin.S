/*-
 * Copyright (c) 2002 Peter Grehan.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*      $NetBSD: setlogin.S,v 1.3 1998/11/24 11:14:57 tsubai Exp $      */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");
	
#include "SYS.h"

	.globl  CNAME(_logname_valid)		/* in _getlogin() */

SYSCALL(setlogin)
#ifdef PIC
	mflr	%r10
	bl	_GLOBAL_OFFSET_TABLE_@local-4
	mflr	%r4
	lwz	%r4,CNAME(_logname_valid)@got(%r4)
	li	%r5,%r0
	stw	%r5,0(%r4)
	mtlr	%r10
#else
	lis	%r4,CNAME(_logname_valid)@ha
	li	%r5,0
	stw	%r5,CNAME(_logname_valid)@l(%r4)
#endif
	blr

	.section .note.GNU-stack,"",%progbits
