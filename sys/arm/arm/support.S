/*-
 * Copyright (c) 2004 Olivier Houchard
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

#include <machine/asm.h>
#include <machine/asmacros.h>
__FBSDID("$FreeBSD$");

#include "assym.s"

ENTRY(casuptr)
	mov	r1, r2
	bl	suword
/*
 * New experimental definitions of IRQdisable and IRQenable
 * These keep FIQ's enabled since FIQ's are special.
 */

#define IRQdisable \
	mrs	r14, cpsr ; \
	orr	r14, r14, #(I32_bit) ; \
	msr	cpsr_c, r14 ; \

#define IRQenable \
	mrs	r14, cpsr ; \
	bic	r14, r14, #(I32_bit) ; \
	msr	cpsr_c, r14 ; \

/*
 * These are used for switching the translation table/DACR.
 * Since the vector page can be invalid for a short time, we must
 * disable both regular IRQs *and* FIQs.
 *
 * XXX: This is not necessary if the vector table is relocated.
 */
#define IRQdisableALL \
	mrs	r14, cpsr ; \
	orr	r14, r14, #(I32_bit) ; \
	msr	cpsr_all, r14

#define IRQenableALL \
	mrs	r14, cpsr ; \
	bic	r14, r14, #(I32_bit) ; \
	msr	cpsr_all, r14

ENTRY(disable_intr)
	IRQdisableALL
ENTRY(enable_intr)
	IRQenableALL

/*
 * memset: Sets a block of memory to the specified value
 *
 * On entry:
 *   r0 - dest address
 *   r1 - byte to write
 *   r2 - number of bytes to write
 *
 * On exit:
 *   r0 - dest address
 */
/* LINTSTUB: Func: void bzero(void *, size_t) */
ENTRY(bzero)
	mov	r3, #0x00
	b	do_memset

/* LINTSTUB: Func: void *memset(void *, int, size_t) */
ENTRY(memset)
	and	r3, r1, #0xff		/* We deal with bytes */
	mov	r1, r2
do_memset:
	cmp	r1, #0x04		/* Do we have less than 4 bytes */
	mov	ip, r0
	blt	.Lmemset_lessthanfour

	/* Ok first we will word align the address */
	ands	r2, ip, #0x03		/* Get the bottom two bits */
	bne	.Lmemset_wordunaligned	/* The address is not word aligned */

	/* We are now word aligned */
.Lmemset_wordaligned:
	orr	r3, r3, r3, lsl #8	/* Extend value to 16-bits */
#ifdef __XSCALE__
	tst	ip, #0x04		/* Quad-align for Xscale */
#else
	cmp	r1, #0x10
#endif
	orr	r3, r3, r3, lsl #16	/* Extend value to 32-bits */
#ifdef __XSCALE__
	subne	r1, r1, #0x04		/* Quad-align if necessary */
	strne	r3, [ip], #0x04
	cmp	r1, #0x10
#endif
	blt	.Lmemset_loop4		/* If less than 16 then use words */
	mov	r2, r3			/* Duplicate data */
	cmp	r1, #0x80		/* If < 128 then skip the big loop */
	blt	.Lmemset_loop32

	/* Do 128 bytes at a time */
.Lmemset_loop128:
	subs	r1, r1, #0x80
#ifdef __XSCALE__
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
#else
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
#endif
	bgt	.Lmemset_loop128
	moveq	pc, lr			/* Zero length so just exit */

	add	r1, r1, #0x80		/* Adjust for extra sub */

	/* Do 32 bytes at a time */
.Lmemset_loop32:
	subs	r1, r1, #0x20
#ifdef __XSCALE__
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
#else
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
#endif
	bgt	.Lmemset_loop32
	moveq	pc, lr			/* Zero length so just exit */

	adds	r1, r1, #0x10		/* Partially adjust for extra sub */

	/* Deal with 16 bytes or more */
#ifdef __XSCALE__
	strged	r2, [ip], #0x08
	strged	r2, [ip], #0x08
#else
	stmgeia	ip!, {r2-r3}
	stmgeia	ip!, {r2-r3}
#endif
	moveq	pc, lr			/* Zero length so just exit */

	addlt	r1, r1, #0x10		/* Possibly adjust for extra sub */

	/* We have at least 4 bytes so copy as words */
.Lmemset_loop4:
	subs	r1, r1, #0x04
	strge	r3, [ip], #0x04
	bgt	.Lmemset_loop4
	moveq	pc, lr			/* Zero length so just exit */

#ifdef __XSCALE__
	/* Compensate for 64-bit alignment check */
	adds	r1, r1, #0x04
	moveq	pc, lr
	cmp	r1, #2
#else
	cmp	r1, #-2
#endif

	strb	r3, [ip], #0x01		/* Set 1 byte */
	strgeb	r3, [ip], #0x01		/* Set another byte */
	strgtb	r3, [ip]		/* and a third */
	mov	pc, lr			/* Exit */

.Lmemset_wordunaligned:
	rsb	r2, r2, #0x004
	strb	r3, [ip], #0x01		/* Set 1 byte */
	cmp	r2, #0x02
	strgeb	r3, [ip], #0x01		/* Set another byte */
	sub	r1, r1, r2
	strgtb	r3, [ip], #0x01		/* and a third */
	cmp	r1, #0x04		/* More than 4 bytes left? */
	bge	.Lmemset_wordaligned	/* Yup */

.Lmemset_lessthanfour:
	cmp	r1, #0x00
	moveq	pc, lr			/* Zero length so exit */
	strb	r3, [ip], #0x01		/* Set 1 byte */
	cmp	r1, #0x02
	strgeb	r3, [ip], #0x01		/* Set another byte */
	strgtb	r3, [ip]		/* and a third */
	mov	pc, lr			/* Exit */
