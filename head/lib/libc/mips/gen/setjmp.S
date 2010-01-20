/*	$NetBSD: setjmp.S,v 1.17 2005/09/17 11:49:39 tsutsui Exp $	*/

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
#include <machine/regnum.h>

#if defined(LIBC_SCCS) && !defined(lint)
	ASMSTR("from: @(#)setjmp.s	8.1 (Berkeley) 6/4/93")
	ASMSTR("$NetBSD: setjmp.S,v 1.17 2005/09/17 11:49:39 tsutsui Exp $")
#endif /* LIBC_SCCS and not lint */

#ifdef __ABICALLS__
	.abicalls
#endif

/*
 * C library -- setjmp, longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	setjmp(a)
 * by restoring registers from the stack,
 * and a struct sigcontext, see <signal.h>
 */

#define SETJMP_FRAME_SIZE	(CALLFRAME_SIZ + (SZREG * 2))


NESTED(setjmp, SETJMP_FRAME_SIZE, ra)
	.mask	0x80000000, (CALLFRAME_RA - CALLFRAME_SIZ)
	.set	noreorder
#ifdef __ABICALLS__
	.cpload	t9
#endif
	subu	sp, sp, SETJMP_FRAME_SIZE	# allocate stack frame
#ifdef __ABICALLS__
	.cprestore 16
#endif
	sw	ra, CALLFRAME_RA(sp)		# save RA
	sw	a0, CALLFRAME_SIZ(sp)		# store env

	/* Get the signal mask. */
	addu	a2, a0, _JB_SIGMASK * SZREG	# &oenv
	li	a0, 1				# SIG_SETBLOCK
	move	a1, zero			# &env == 0
	la	t9, _C_LABEL(sigprocmask)	# get current signal mask
	jal	t9
	nop

	lw	a0, CALLFRAME_SIZ(sp)		# restore env pointer
	lw	ra, CALLFRAME_RA(sp)		# restore RA
	addu	sp, sp, SETJMP_FRAME_SIZE	# pop stack frame

	li	v0, _JB_MAGIC_SETJMP
	sw	v0, (_JB_MAGIC  * SZREG)(a0)
	sw	ra, (_JB_REG_RA * SZREG)(a0)
	sw	s0, (_JB_REG_S0 * SZREG)(a0)
	sw	s1, (_JB_REG_S1 * SZREG)(a0)
	sw	s2, (_JB_REG_S2 * SZREG)(a0)
	sw	s3, (_JB_REG_S3 * SZREG)(a0)
	sw	s4, (_JB_REG_S4 * SZREG)(a0)
	sw	s5, (_JB_REG_S5 * SZREG)(a0)
	sw	s6, (_JB_REG_S6 * SZREG)(a0)
	sw	s7, (_JB_REG_S7 * SZREG)(a0)
	sw	sp, (_JB_REG_SP * SZREG)(a0)
	sw	s8, (_JB_REG_S8 * SZREG)(a0)

	move	v0, zero
	j	ra
END(setjmp)

#define LONGJMP_FRAME_SIZE	(CALLFRAME_SIZ + (SZREG * 2))

NESTED(longjmp, LONGJMP_FRAME_SIZE, ra)
	.mask	0x80000000, (CALLFRAME_RA - CALLFRAME_SIZ)
	.set	noreorder
#ifdef __ABICALLS__
	.cpload	t9
#endif
	subu	sp, sp, LONGJMP_FRAME_SIZE	# allocate stack frame
#ifdef __ABICALLS__
	.cprestore 16
#endif
	sw	ra, CALLFRAME_RA(sp)		# save RA
	lw	v0, (_JB_MAGIC  * SZREG)(a0)
	li	t0, _JB_MAGIC_SETJMP
	bne	v0, t0, botch		# jump if error
	nop

	sw	a0, CALLFRAME_SIZ(sp)		# save env
	sw	a1, (CALLFRAME_SIZ + SZREG)(sp)	# save return value

	# set sigmask
	addu	a1, a0, _JB_SIGMASK * SZREG	# &set
	move	a2, zero			# &oset == NULL
	li	a0, 3				# SIG_SETMASK
	la	t9,_C_LABEL(sigprocmask)	# set current signal mask
	jal	t9
	nop

	lw	a0, CALLFRAME_SIZ(sp)		# restore env
	lw	a1, (CALLFRAME_SIZ + SZREG)(sp)	# restore return value

	lw	ra, (_JB_REG_RA * SZREG)(a0)
	lw	s0, (_JB_REG_S0 * SZREG)(a0)
	lw	s1, (_JB_REG_S1 * SZREG)(a0)
	lw	s2, (_JB_REG_S2 * SZREG)(a0)
	lw	s3, (_JB_REG_S3 * SZREG)(a0)
	lw	s4, (_JB_REG_S4 * SZREG)(a0)
	lw	s5, (_JB_REG_S5 * SZREG)(a0)
	lw	s6, (_JB_REG_S6 * SZREG)(a0)
	lw	s7, (_JB_REG_S7 * SZREG)(a0)
	lw	sp, (_JB_REG_SP * SZREG)(a0)
	lw	s8, (_JB_REG_S8 * SZREG)(a0)
	move	v0, a1
	j	ra
	nop

botch:
	la	t9, _C_LABEL(longjmperror)
	jal	t9
	nop

	la	t9, _C_LABEL(abort)
	jal	t9
	nop
END(longjmp)
