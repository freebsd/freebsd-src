/*-
 * Copyright 2013 Philip Withnall.
 * Copyright 2009 Konstantin Belousov.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: head/lib/csu/mips/crt1_s.S 217383 2011-01-13 23:00:22Z kib $
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD: head/lib/csu/mips/crt1_s.S 217383 2011-01-13 23:00:22Z kib $");

	.text
	.balign	4
	.globl	__start
	.type	__start, @function
__start:
	.cfi_startproc
	.set push # save assembler options
	.set noreorder

#if defined(CHERI)
	/* Ensure the stack is 32-byte aligned so we can store capabilities
	 * on it. Do this before allocating the first stack frame. This is
	 * only necessary on CHERI. */
#ifndef _MIPS_CAP_ALIGN_MASK
#define	_MIPS_CAP_ALIGN_MASK	0xffffffffffffffe0
#endif
	li	t0, _MIPS_CAP_ALIGN_MASK
	and	sp, sp, t0
#endif

	/* Set up $gp as if this was a proper function. This is needed so we
	 * can load the address of _start1 below.
	 *
	 * __ABICALLS__ is defined if -mabicalls has been passed to GCC to
	 * get it to generate SVR4-style position independent code (PIC).
	 *
	 * NOTE: In order to move the AND instruction from above to the delay
	 * slot below, we assume none of the $gp setup code clobbers $t0. */
#if defined(__ABICALLS__) && (defined(__mips_o32) || defined(__mips_o64))
	SETUP_GP
#endif
	PTR_ADDU	sp, sp, -CALLFRAME_SIZ
	REG_S	ra, CALLFRAME_RA(sp)
#if defined(__ABICALLS__)
#if defined(__mips_o32) || defined(__mips_o64)
	SAVE_GP(CALLFRAME_GP)
#else
	SETUP_GP64(CALLFRAME_GP, __start)
#endif
#else /* if !__ABICALLS__ */
#if defined(__mips_n32) || defined(__mips_n64)
	REG_S	gp, CALLFRAME_GP(sp)
#endif
#endif

	/* Jump to the second part of __start(), implemented in C. */
	dla	$25, _start1
	jr	$25
	nop # delay slot

	.set pop # restore assembler options
	.cfi_endproc

	/* Explicitly set the size of this function. */
	.size __start, . - __start

	/* Hint that this object doesn't require an executable stack. */
	.section .note.GNU-stack,"",%progbits
