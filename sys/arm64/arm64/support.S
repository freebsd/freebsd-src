/*-
 * Copyright (c) 2014 Andrew Turner
 * Copyright (c) 2014-2015 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Andrew Turner
 * under sponsorship from the FreeBSD Foundation
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
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#include <machine/setjmp.h>
#include <machine/param.h>
#include <machine/vmparam.h>

#include "assym.inc"

.macro check_user_access user_arg, limit, bad_addr_func
	ldr	x7, =(\limit)
	cmp	x\user_arg, x7
	b.cs	\bad_addr_func
.endm

/*
 * One of the fu* or su* functions failed, return -1.
 */
ENTRY(fsu_fault)
	SET_FAULT_HANDLER(xzr, x1)	/* Reset the handler function */
	EXIT_USER_ACCESS_CHECK(w0, x1)
fsu_fault_nopcb:
	mov	x0, #-1
	ret
END(fsu_fault)

/*
 * int casueword32(volatile uint32_t *, uint32_t, uint32_t *, uint32_t)
 */
ENTRY(casueword32)
	check_user_access 0, (VM_MAXUSER_ADDRESS-3), fsu_fault_nopcb
	adr	x6, fsu_fault		/* Load the fault handler */
	mov	w5, #1
	SET_FAULT_HANDLER(x6, x4)	/* And set it */
	ENTER_USER_ACCESS(w6, x4)
	ldxr	w4, [x0]		/* Load-exclusive the data */
	cmp	w4, w1			/* Compare */
	b.ne	1f			/* Not equal, exit */
	stxr	w5, w3, [x0]		/* Store the new data */
1:	EXIT_USER_ACCESS(w6)
	SET_FAULT_HANDLER(xzr, x6)	/* Reset the fault handler */
	str	w4, [x2]		/* Store the read data */
	mov	w0, w5			/* Result same as store status */
	ret				/* Return */
END(casueword32)

/*
 * int casueword(volatile u_long *, u_long, u_long *, u_long)
 */
ENTRY(casueword)
	check_user_access 0, (VM_MAXUSER_ADDRESS-7), fsu_fault_nopcb
	adr	x6, fsu_fault		/* Load the fault handler */
	mov	w5, #1
	SET_FAULT_HANDLER(x6, x4)	/* And set it */
	ENTER_USER_ACCESS(w6, x4)
	ldxr	x4, [x0]		/* Load-exclusive the data */
	cmp	x4, x1			/* Compare */
	b.ne	1f			/* Not equal, exit */
	stxr	w5, x3, [x0]		/* Store the new data */
1:	EXIT_USER_ACCESS(w6)
	SET_FAULT_HANDLER(xzr, x6)	/* Reset the fault handler */
	str	x4, [x2]		/* Store the read data */
	mov	w0, w5			/* Result same as store status */
	ret				/* Return */
END(casueword)

.macro fsudata insn, ret_reg, user_arg
	adr	x7, fsu_fault		/* Load the fault handler */
	SET_FAULT_HANDLER(x7, x6)	/* And set it */
	\insn	\ret_reg, [x\user_arg]	/* Try accessing the data */
	SET_FAULT_HANDLER(xzr, x6)	/* Reset the fault handler */
.endm

/*
 * int fubyte(volatile const void *)
 */
ENTRY(fubyte)
	check_user_access 0, (VM_MAXUSER_ADDRESS), fsu_fault_nopcb
	fsudata	ldtrb, w0, 0
	ret				/* Return */
END(fubyte)

/*
 * int fuword(volatile const void *)
 */
ENTRY(fuword16)
	check_user_access 0, (VM_MAXUSER_ADDRESS-1), fsu_fault_nopcb
	fsudata	ldtrh, w0, 0
	ret				/* Return */
END(fuword16)

/*
 * int32_t fueword32(volatile const void *, int32_t *)
 */
ENTRY(fueword32)
	check_user_access 0, (VM_MAXUSER_ADDRESS-3), fsu_fault_nopcb
	fsudata	ldtr, w0, 0
	str	w0, [x1]		/* Save the data in kernel space */
	mov	w0, #0			/* Success */
	ret				/* Return */
END(fueword32)

/*
 * long fueword(volatile const void *, int64_t *)
 * int64_t fueword64(volatile const void *, int64_t *)
 */
ENTRY(fueword)
EENTRY(fueword64)
	check_user_access 0, (VM_MAXUSER_ADDRESS-7), fsu_fault_nopcb
	fsudata	ldtr, x0, 0
	str	x0, [x1]		/* Save the data in kernel space */
	mov	x0, #0			/* Success */
	ret				/* Return */
EEND(fueword64)
END(fueword)

/*
 * int subyte(volatile void *, int)
 */
ENTRY(subyte)
	check_user_access 0, (VM_MAXUSER_ADDRESS), fsu_fault_nopcb
	fsudata	sttrb, w1, 0
	mov	x0, #0			/* Success */
	ret				/* Return */
END(subyte)

/*
 * int suword16(volatile void *, int)
 */
ENTRY(suword16)
	check_user_access 0, (VM_MAXUSER_ADDRESS-1), fsu_fault_nopcb
	fsudata	sttrh, w1, 0
	mov	x0, #0			/* Success */
	ret				/* Return */
END(suword16)

/*
 * int suword32(volatile void *, int)
 */
ENTRY(suword32)
	check_user_access 0, (VM_MAXUSER_ADDRESS-3), fsu_fault_nopcb
	fsudata	sttr, w1, 0
	mov	x0, #0			/* Success */
	ret				/* Return */
END(suword32)

/*
 * int suword(volatile void *, long)
 */
ENTRY(suword)
EENTRY(suword64)
	check_user_access 0, (VM_MAXUSER_ADDRESS-7), fsu_fault_nopcb
	fsudata	sttr, x1, 0
	mov	x0, #0			/* Success */
	ret				/* Return */
EEND(suword64)
END(suword)

ENTRY(setjmp)
	/* Store the stack pointer */
	mov	x8, sp
	str	x8, [x0], #8

	/* Store the general purpose registers and lr */
	stp	x19, x20, [x0], #16
	stp	x21, x22, [x0], #16
	stp	x23, x24, [x0], #16
	stp	x25, x26, [x0], #16
	stp	x27, x28, [x0], #16
	stp	x29, lr, [x0], #16

	/* Return value */
	mov	x0, #0
	ret
END(setjmp)

ENTRY(longjmp)
	/* Restore the stack pointer */
	ldr	x8, [x0], #8
	mov	sp, x8

	/* Restore the general purpose registers and lr */
	ldp	x19, x20, [x0], #16
	ldp	x21, x22, [x0], #16
	ldp	x23, x24, [x0], #16
	ldp	x25, x26, [x0], #16
	ldp	x27, x28, [x0], #16
	ldp	x29, lr, [x0], #16

	/* Load the return value */
	mov	x0, x1
	ret
END(longjmp)

/*
 * pagezero, simple implementation
 */
ENTRY(pagezero_simple)
	add	x1, x0, #PAGE_SIZE

1:
	stp	xzr, xzr, [x0], #0x10
	stp	xzr, xzr, [x0], #0x10
	stp	xzr, xzr, [x0], #0x10
	stp	xzr, xzr, [x0], #0x10
	cmp	x0, x1
	b.ne	1b
	ret

END(pagezero_simple)

/*
 * pagezero, cache assisted
 */
ENTRY(pagezero_cache)
	add	x1, x0, #PAGE_SIZE

	ldr	x2, =dczva_line_size
	ldr	x2, [x2]

1:
	dc	zva, x0
	add	x0, x0, x2
	cmp	x0, x1
	b.ne	1b
	ret

END(pagezero_cache)
