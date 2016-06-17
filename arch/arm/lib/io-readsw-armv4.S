/*
 *  linux/arch/arm/lib/io-readsw-armv4.S
 *
 *  Copyright (C) 1995-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/hardware.h>

.insw_bad_alignment:
		adr	r0, .insw_bad_align_msg
		mov	r2, lr
		b	SYMBOL_NAME(panic)
.insw_bad_align_msg:
		.asciz	"insw: bad buffer alignment (0x%p, lr=0x%08lX)\n"
		.align

.insw_align:	tst	r1, #1
		bne	.insw_bad_alignment

		ldrh	r3, [r0]
		strh	r3, [r1], #2

		subs	r2, r2, #1
		RETINSTR(moveq, pc, lr)

ENTRY(__raw_readsw)
		teq	r2, #0		@ do we have to check for the zero len?
		moveq	pc, lr
		tst	r1, #3
		bne	.insw_align

		stmfd	sp!, {r4, r5, lr}

		subs	r2, r2, #8
		bmi	.no_insw_8

.insw_8_lp:	ldrh	r3, [r0]
		ldrh	r4, [r0]
		orr	r3, r3, r4, lsl #16

		ldrh	r4, [r0]
		ldrh	r5, [r0]
		orr	r4, r4, r5, lsl #16

		ldrh	r5, [r0]
		ldrh	ip, [r0]
		orr	r5, r5, ip, lsl #16

		ldrh	ip, [r0]
		ldrh	lr, [r0]
		orr	ip, ip, lr, lsl #16

		stmia	r1!, {r3 - r5, ip}

		subs	r2, r2, #8
		bpl	.insw_8_lp

		tst	r2, #7
		LOADREGS(eqfd, sp!, {r4, r5, pc})

.no_insw_8:	tst	r2, #4
		beq	.no_insw_4

		ldrh	r3, [r0]
		ldrh	r4, [r0]
		orr	r3, r3, r4, lsl #16

		ldrh	r4, [r0]
		ldrh	ip, [r0]
		orr	r4, r4, ip, lsl #16

		stmia	r1!, {r3, r4}

.no_insw_4:	tst	r2, #2
		beq	.no_insw_2

		ldrh	r3, [r0]
		ldrh	ip, [r0]
		orr	r3, r3, ip, lsl #16

		str	r3, [r1], #4

.no_insw_2:	tst	r2, #1
		ldrneh	r3, [r0]
		strneh	r3, [r1]

		LOADREGS(fd, sp!, {r4, r5, pc})
