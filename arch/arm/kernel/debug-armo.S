/*
 *  linux/arch/arm/kernel/debug-armo.S
 *
 *  Copyright (C) 1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  26-bit debugging code
 */
#include <linux/linkage.h>

		.macro	addruart,rx
		mov	\rx, #0x03000000
		orr	\rx, \rx, #0x00010000
		orr	\rx, \rx, #0x00000fe0
		.endm

		.macro	senduart,rd,rx
		strb	\rd, [\rx]
		.endm

		.macro	busyuart,rd,rx
1002:		ldrb	\rd, [\rx, #0x14]
		and	\rd, \rd, #0x60
		teq	\rd, #0x60
		bne	1002b
		.endm

		.macro	waituart,rd,rx
1001:		ldrb	\rd, [\rx, #0x18]
		tst	\rd, #0x10
		beq	1001b
		.endm

		.text
/*
 * Useful debugging routines
 */
ENTRY(printhex8)
		mov	r1, #8
		b	printhex

ENTRY(printhex4)
		mov	r1, #4
		b	printhex

ENTRY(printhex2)
		mov	r1, #2
printhex:	ldr	r2, =hexbuf
		add	r3, r2, r1
		mov	r1, #0
		strb	r1, [r3]
1:		and	r1, r0, #15
		mov	r0, r0, lsr #4
		cmp	r1, #10
		addlt	r1, r1, #'0'
		addge	r1, r1, #'a' - 10
		strb	r1, [r3, #-1]!
		teq	r3, r2
		bne	1b
		mov	r0, r2
		b	printascii

		.ltorg

ENTRY(printascii)
		addruart r3
		b	2f
1:		waituart r2, r3
		senduart r1, r3
		busyuart r2, r3
		teq	r1, #'\n'
		moveq	r1, #'\r'
		beq	1b
2:		teq	r0, #0
		ldrneb	r1, [r0], #1
		teqne	r1, #0
		bne	1b
		mov	pc, lr

ENTRY(printch)
		addruart r3
		mov	r1, r0
		mov	r0, #0
		b	1b

		.bss
hexbuf:		.space 16
