/*-
 * Copyright (c) 2014-2015 Robert N. M. Watson
 * Copyright (c) 2017 SRI International
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

/*
 * Copy a string from one capabilty to another up to a maximum length.
 * Return the length by reference.
 *
 * copystr_c() accepts:
 * c3 - source pointer
 * c4 - destination pointer
 * c5 - pointer length copied (size_t)
 * a0 - maximum length
 */

LEAF(copystr_c)
	li		v0, 0	/* Hopefully we succeed */
	beqz		a0, cstr_return_toolong /* Return if zero-length. */	
	li		t1, 1	/* Length we will have copied in loop */
cstr_loop:
	clbu		t0, t1, -1($c3)
	beq		t0, zero, cstr_return_nul
	csb		t0, t1, -1($c4)
	bne		t1, a0, cstr_loop
	daddiu		t1, t1, 1

cstr_return_toolong:
	li		v0, ENAMETOOLONG
	daddiu		t1, t1, -1	/* Count is 1 too high */
cstr_return_nul:	/* Store length read if c5 is non-NULL */
	cgetdefault	$c3
	ctoptr		t0, $c5, $c3
	beq	 	t0, zero, cstr_return
	nop
	csd		t1, zero, 0($c5)	/* Store bytes copied */
cstr_return:
	jr	ra
	nop
END(copystr_c)
