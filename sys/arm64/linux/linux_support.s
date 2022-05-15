/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (C) 2018 Turing Robotic Industries Inc.
 * Copyright (C) 2022 Dmitry Chagin <dchagin@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#include <machine/param.h>
#include <machine/vmparam.h>

#include <sys/errno.h>

#include "assym.inc"

.macro check_user_access user_arg, limit, bad_addr_func
	ldr	x7, =(\limit)
	cmp	x\user_arg, x7
	b.cs	\bad_addr_func
.endm

futex_fault:
	SET_FAULT_HANDLER(xzr, x1)
	EXIT_USER_ACCESS_CHECK(w0, x1)
futex_fault_nopcb:
	mov	x0, #EFAULT
	ret

#define	LINUX_FUTEX_MAX_LOOPS	128

/*
 * int oparg, uint32_t *uaddr, int *oldval
 *
 * Return 0 on success, errno on failure,
 * EAGAIN is returned if LL/SC operation fails.
 *
 * XXX. VM_MAXUSER_ADDRESS is not applicable here, should be replaced
 * by something like LINUX_SHAREDPAGE.
 */

/* (int *)uaddr2 = oparg */
ENTRY(futex_xchgl)
	check_user_access 1, (VM_MAXUSER_ADDRESS-3), futex_fault_nopcb
	adr	x9, futex_fault		/* Load the fault handler */
	SET_FAULT_HANDLER(x9, x4)	/* And set it */
	ENTER_USER_ACCESS(w9, x4)
	mov	w5, #LINUX_FUTEX_MAX_LOOPS
	prfm	pstl1strm, [x1]
	mov	w6, w0			/* Save oparg */
1:	ldxr	w4, [x1]		/* Load oldval from uaddr */
	stlxr	w0, w6, [x1]		/* Store oparg to uaddr */
	cbz	w0, 3f			/* Exit on success */
	sub	w5, w5, w0		/* Dec loop counter, w0 is 1 */
	cbnz	w5, 1b			/* Loop */
	mov	x0, #EAGAIN		/* Store of newval failed */
3:	dmb	ish
	EXIT_USER_ACCESS(w9)
	SET_FAULT_HANDLER(xzr, x9)	/* Reset the fault handler */
	str w4, [x2]			/* Store oldval */
	ret
END(futex_xchgl)

/* (int *)uaddr2 += oparg */
ENTRY(futex_addl)
	check_user_access 1, (VM_MAXUSER_ADDRESS-3), futex_fault_nopcb
	adr	x9, futex_fault
	SET_FAULT_HANDLER(x9, x4)
	ENTER_USER_ACCESS(w9, x4)
	mov	w5, #LINUX_FUTEX_MAX_LOOPS
	prfm	pstl1strm, [x1]
	mov	w6, w0
1:	ldxr	w4, [x1]
	add	w3, w4, w6		/* oldval + oparg */
	stlxr	w0, w3, [x1]
	cbz	w0, 3f
	sub	w5, w5, w0
	cbnz	w5, 1b
	mov	x0, #EAGAIN
3:	dmb	ish
	EXIT_USER_ACCESS(w9)
	SET_FAULT_HANDLER(xzr, x9)
	str w4, [x2]
	ret
END(futex_addl)

/* (int *)uaddr2 |= oparg */
ENTRY(futex_orl)
	check_user_access 1, (VM_MAXUSER_ADDRESS-3), futex_fault_nopcb
	adr	x9, futex_fault
	SET_FAULT_HANDLER(x9, x4)
	ENTER_USER_ACCESS(w9, x4)
	mov	w5, #LINUX_FUTEX_MAX_LOOPS
	prfm	pstl1strm, [x1]
	mov	w6, w0
1:	ldxr	w4, [x1]
	orr	w3, w4, w6		/* oldavl |= oparg */
	stlxr	w0, w3, [x1]
	cbz	w0, 3f
	sub	w5, w5, w0
	cbnz	w5, 1b
	mov	x0, #EAGAIN
3:	dmb	ish
	EXIT_USER_ACCESS(w9)
	SET_FAULT_HANDLER(xzr, x9)
	str w4, [x2]
	ret
END(futex_orl)

/* (int *)uaddr2 &= oparg */
ENTRY(futex_andl)
	check_user_access 1, (VM_MAXUSER_ADDRESS-3), futex_fault_nopcb
	adr	x9, futex_fault
	SET_FAULT_HANDLER(x9, x4)
	ENTER_USER_ACCESS(w9, x4)
	mov	w5, #LINUX_FUTEX_MAX_LOOPS
	prfm	pstl1strm, [x1]
	mov	w6, w0
1:	ldxr	w4, [x1]
	and	w3, w4, w6		/* oldval &= oparg */
	stlxr	w0, w3, [x1]
	cbz	w0, 3f
	sub	w5, w5, w0
	cbnz	w5, 1b
	mov	x0, #EAGAIN
3:	dmb	ish
	EXIT_USER_ACCESS(w9)
	SET_FAULT_HANDLER(xzr, x9)
	str w4, [x2]
	ret
END(futex_andl)

/* (int *)uaddr2 ^= oparg */
ENTRY(futex_xorl)
	check_user_access 1, (VM_MAXUSER_ADDRESS-3), futex_fault_nopcb
	adr	x9, futex_fault
	SET_FAULT_HANDLER(x9, x4)
	ENTER_USER_ACCESS(w9, x4)
	mov	w5, #LINUX_FUTEX_MAX_LOOPS
	prfm	pstl1strm, [x1]
	mov	w6, w0
1:	ldxr	w4, [x1]
	eor	w3, w4, w6		/* oldval ^= oparg */
	stlxr	w0, w3, [x1]
	cbz	w0, 3f
	sub	w5, w5, w0
	cbnz	w5, 1b
	mov	x0, #EAGAIN
3:	dmb	ish
	EXIT_USER_ACCESS(w9)
	SET_FAULT_HANDLER(xzr, x9)
	str w4, [x2]
	ret
END(futex_xorl)
