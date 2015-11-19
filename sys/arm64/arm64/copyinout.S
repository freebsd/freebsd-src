/*-
 * Copyright (c) 2015 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
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

#include <sys/errno.h>

#include "assym.s"

/*
 * Fault handler for the copy{in,out} functions below.
 */
ENTRY(copyio_fault)
	SET_FAULT_HANDLER(xzr, x1) /* Clear the handler */
copyio_fault_nopcb:
	mov	x0, #EFAULT
	ret
END(copyio_fault)

/*
 * Copies from a kernel to user address
 *
 * int copyout(const void *kaddr, void *udaddr, size_t len)
 */
ENTRY(copyout)
	cbz	x2, 2f		/* If len == 0 then skip loop */
	add	x3, x1, x2
	ldr	x4, =VM_MAXUSER_ADDRESS
	cmp	x3, x4
	b.hi	copyio_fault_nopcb

	adr	x6, copyio_fault /* Get the handler address */
	SET_FAULT_HANDLER(x6, x7) /* Set the handler */

1:	ldrb	w4, [x0], #1	/* Load from kaddr */
	strb	w4, [x1], #1	/* Store in uaddr */
	sub	x2, x2, #1	/* len-- */
	cbnz	x2, 1b

	SET_FAULT_HANDLER(xzr, x7) /* Clear the handler */

2:	mov	x0, xzr		/* return 0 */
	ret
END(copyout)

/*
 * Copies from a user to kernel address
 *
 * int copyin(const void *uaddr, void *kdaddr, size_t len)
 */
ENTRY(copyin)
	cbz	x2, 2f		/* If len == 0 then skip loop */
	add	x3, x0, x2
	ldr	x4, =VM_MAXUSER_ADDRESS
	cmp	x3, x4
	b.hi	copyio_fault_nopcb

	adr	x6, copyio_fault /* Get the handler address */
	SET_FAULT_HANDLER(x6, x7) /* Set the handler */

1:	ldrb	w4, [x0], #1	/* Load from uaddr */
	strb	w4, [x1], #1	/* Store in kaddr */
	sub	x2, x2, #1	/* len-- */
	cbnz	x2, 1b

	SET_FAULT_HANDLER(xzr, x7) /* Clear the handler */

2:	mov	x0, xzr		/* return 0 */
	ret
END(copyin)

/*
 * Copies a string from a user to kernel address
 *
 * int copyinstr(const void *udaddr, void *kaddr, size_t len, size_t *done)
 */
ENTRY(copyinstr)
	mov	x5, xzr		/* count = 0 */
	mov	w4, #1		/* If zero return faulure */
	cbz	x2, 3f		/* If len == 0 then skip loop */
	ldr	x7, =VM_MAXUSER_ADDRESS

	adr	x6, copyio_fault /* Get the handler address */
	SET_FAULT_HANDLER(x6, x7) /* Set the handler */

1:	cmp	x0, x7
	b.cs	copyio_fault
	ldrb	w4, [x0], #1	/* Load from uaddr */
	strb	w4, [x1], #1	/* Store in kaddr */
	add	x5, x5, #1	/* count++ */
	cbz	w4, 2f		/* Break when NUL-terminated */
	sub	x2, x2, #1	/* len-- */
	cbnz	x2, 1b

2:	SET_FAULT_HANDLER(xzr, x7) /* Clear the handler */

3:	cbz	x3, 4f		/* Check if done != NULL */
	str	x5, [x3]	/* done = count */

4:	mov	w1, #ENAMETOOLONG /* Load ENAMETOOLONG to return if failed */
	cmp	w4, #0		/* Check if we saved the NUL-terminator */
	csel	w0, wzr, w1, eq	/* If so return success, else failure */
	ret
END(copyinstr)
