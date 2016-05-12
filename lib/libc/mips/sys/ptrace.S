/*	$NetBSD: ptrace.S,v 1.9 2003/08/07 16:42:17 agc Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");
#include "SYS.h"

#if defined(LIBC_SCCS) && !defined(lint)
	ASMSTR("from: @(#)ptrace.s	8.1 (Berkeley) 6/4/93")
	ASMSTR("$NetBSD: ptrace.S,v 1.9 2003/08/07 16:42:17 agc Exp $")
#endif /* LIBC_SCCS and not lint */

NESTED_NOPROFILE(ptrace, CALLFRAME_SIZ, ra)
#ifndef __CHERI_PURE_CAPABILITY__
	.mask	0x80000000, (CALLFRAME_RA - CALLFRAME_SIZ)
	SETUP_GP
	PTR_SUBU	sp, sp, CALLFRAME_SIZ
	SETUP_GP64(CALLFRAME_GP, ptrace)
	SAVE_GP(CALLFRAME_GP)

	PTR_S		ra, CALLFRAME_RA(sp)

	PTR_LA		t9, _C_LABEL(__error)	# locate address of errno
	jalr		t9

	PTR_L		ra, CALLFRAME_RA(sp)
	INT_S		zero, 0(v0)		# update errno value

	li		v0, SYS_ptrace
	syscall

	# Load __cerror's address using our gp, then restore it.
	PTR_LA		t9, __cerror
	RESTORE_GP64
	PTR_ADDU	sp, sp, CALLFRAME_SIZ

	bne		a3, zero, 1f

	j		ra
1:	j		t9
#else
	/* CheriABI saves ra (for now) and fp */
	.mask	0xc0000000, (CALLFRAME_RA - CALLFRAME_SIZ)
	PTR_SUBU	sp, sp, CALLFRAME_SIZ

	csd		ra, sp, CALLFRAME_RA($c11)
	csd		s8, sp, CALLFRAME_FP($c11)
	csc		$c17, sp, CALLFRAME_C17($c11)
	move		s8, sp

	dla		t9, _C_LABEL(__error)   # locate address of errno
#ifdef PIC
	dla		t0, 1f
1:	cgetpcc		$c12
	cgetoffset	t1, $c12
	dsub		t0, t1, t0
	csetoffset	$c12, $c12, t0
	cincoffset	$c12, $c12, t9
#else
	cgetpccsetoffset $c12, t9
#endif
	cjalr		$c17, $c12
	nop

	move		sp, s8
	csw		zero, zero, 0($c3)	# update errno value

	li		v0, SYS_ptrace
	syscall

	clc		$c17, sp, CALLFRAME_C17($c11)
	cld		s8, sp, CALLFRAME_FP($c11)
	cld		ra, sp, CALLFRAME_RA($c11)
	PTR_ADDU	sp, sp, CALLFRAME_SIZ

	bne		a3, zero, err
	nop

	cjr		$c17
err:
	PTR_LA		t9, __cerror
	cgetpcc		$c12
	csetoffset	$c12, $c12, t9
	cjr		$c12
#endif
END(ptrace)
