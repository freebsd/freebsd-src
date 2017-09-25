/*-
 * Copyright (c) 2014-2015 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

/* XXXRW: Can I drop some of these? */
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/regnum.h>
#include <machine/cpuregs.h>
#include <machine/pte.h>

#ifdef CPU_CHERI
#include <machine/cheriasm.h>
#include <machine/cherireg.h>
#endif

#include "assym.s"

.set noat
.set noreorder

/*
 * Implement CHERI memcpy() and bcopy() variants in assembly; C works fine in
 * the kernel most of the time, but copyincap() and copyoutcap() require that
 * no callee-save registers be trampled over due to fault-based error
 * handling.
 *
 * These routines attempt to handle only fully aligned access, and will throw
 * exceptions if that is not the case.  Likewise, they do not handle
 * overlapping copies, a typical requirement of bcopy() implementations.
 *
 * cheri_bcopy() accepts:
 * a0 - source pointer
 * a1 - destination pointer
 * a2 - length
 *
 * cheri_memcpy() has inverted source/destination pointers.
 */
LEAF(cheri_memcpy)
	move	v0, a0		/* Note cleverness: v0 will hold original... */
	move	a0, a1		/* ... value of a0 for cheri_memcpy(). */
	move	a1, v0

XLEAF(cheri_bcopy)
	beqz	a2, return	/* Return immediately if zero-length. */
	li	v1, -CHERICAP_SIZE	/* Handle under-aligned lengths by... */
	and	a2, a2, v1	/* .. truncating the length to capsize bytes. */
	daddu	a2, a1, a2	/* Branch delay: put end pointer in a2. */
loop:
	clc	CHERI_REG_CTEMP0, a0, 0(CHERI_REG_KDC)
	csc	CHERI_REG_CTEMP0, a1, 0(CHERI_REG_KDC)
	daddiu	a1, a1, CHERICAP_SIZE	/* Increment destination pointer. */
	bne	a1, a2, loop	/* Are we there yet?  If not, loop. */
	daddiu	a0, a0, CHERICAP_SIZE	/* Branch delay: increment src ptr. */
return:
	jr	ra
	nop
END(cheri_memcpy)

/*
 * Implement memcpy() and bcopy() variants that take capability
 * arguments, but DO NOT copy capabilities.  These are implemented in
 * assembly to support copyin and copyout variants.
 *
 *  XXX: These routines are inefficent and copy one byte at a time.
 *
 * memcpynocap_c() accepts:
 * c3 - destination ponter
 * c4 - source pointer
 * a0 - length
 *
 * bcopynocap_c() has inverted source/destination pointers.
 */
LEAF(bcopynocap_c)
	cmove	$c5, $c3
	cmove	$c3, $c4	/* $c3 now holds $dst for memcpynocap_c */
	cmove	$c4, $c5
XLEAF(memcpynocap_c)
	beqz	a0, bcnc_return	/* Return immediately if zero-length. */
	li	t0, 1		/* Bytes that will have been copied */
bcnc_loop:
	clbu	t1, t0, -1($c4)
	csb	t1, t0, -1($c3)
	bne	a0, t0, bcnc_loop /* Are we there yet?  If not, loop. */
	daddiu	t0, t0, 1
bcnc_return:
	jr	ra
	nop
END(bcopynocap_c)

LEAF(copystr_fc)
	move		t0, a1
	li		t1, 1
	beq		a1, zero, 4f
1:
	clbu		v0, zero, 0($c3)
	PTR_SUBU	a1, a1, 1
	beq		v0, zero, 2f
	sb		v0, 0(a0)		# each byte until NIL
	cincoffset	$c3, $c3, t1
	bne		a1, zero, 1b		# less than maxlen
	PTR_ADDU	a0, a0, 1
4:
	li		v0, ENAMETOOLONG	# run out of space
2:
	beq		a2, zero, 3f		# return num. of copied bytes
	PTR_SUBU	a1, t0, a1		# if the 4th arg was non-NULL
	PTR_S		a1, 0(a2)
3:
	j		ra			# v0 is 0 or ENAMETOOLONG
	nop
END(copystr_fc)

NESTED(copyinstr_fc, CALLFRAME_SIZ, ra)
	PTR_SUBU	sp, sp, CALLFRAME_SIZ
	.mask	0x80000000, (CALLFRAME_RA - CALLFRAME_SIZ)
	PTR_LA	v0, copyerr
	# XXX
	#blt	a0, zero, _C_LABEL(copyerr)  # make sure address is in user space
	REG_S	ra, CALLFRAME_RA(sp)
	GET_CPU_PCPU(v1)
	PTR_L	v1, PC_CURPCB(v1)
	PTR_LA	t9, _C_LABEL(copystr_fc)
	jalr	t9
	PTR_S	v0, U_PCB_ONFAULT(v1)
	REG_L	ra, CALLFRAME_RA(sp)
	GET_CPU_PCPU(v1)
	PTR_L	v1, PC_CURPCB(v1)
	PTR_S	zero, U_PCB_ONFAULT(v1)
	j	ra
	PTR_ADDU	sp, sp, CALLFRAME_SIZ
END(copyinstr_fc)
