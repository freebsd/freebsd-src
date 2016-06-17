/*
 *  linux/arch/arm/lib/findbit.S
 *
 *  Copyright (C) 1995-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
                .text

/*
 * Purpose  : Find a 'zero' bit
 * Prototype: int find_first_zero_bit(void *addr, int maxbit);
 */
ENTRY(find_first_zero_bit)
		mov	r2, #0
.bytelp:	ldrb	r3, [r0, r2, lsr #3]
		eors	r3, r3, #0xff		@ invert bits
		bne	.found			@ any now set - found zero bit
		add	r2, r2, #8		@ next bit pointer
		cmp	r2, r1			@ any more?
		bcc	.bytelp
		add	r0, r1, #1		@ no free bits
		RETINSTR(mov,pc,lr)

/*
 * Purpose  : Find next 'zero' bit
 * Prototype: int find_next_zero_bit(void *addr, int maxbit, int offset)
 */
ENTRY(find_next_zero_bit)
		ands	ip, r2, #7
		beq	.bytelp			@ If new byte, goto old routine
		ldrb	r3, [r0, r2, lsr#3]
		eor	r3, r3, #0xff		@ now looking for a 1 bit
		movs	r3, r3, lsr ip		@ shift off unused bits
		orreq	r2, r2, #7		@ if zero, then no bits here
		addeq	r2, r2, #1		@ align bit pointer
		beq	.bytelp			@ loop for next bit

/*
 * One or more bits in the LSB of r3 are assumed to be set.
 */
.found:		tst	r3, #0x0f
		addeq	r2, r2, #4
		movne	r3, r3, lsl #4
		tst	r3, #0x30
		addeq	r2, r2, #2
		movne	r3, r3, lsl #2
		tst	r3, #0x40
		addeq	r2, r2, #1
		mov	r0, r2
		RETINSTR(mov,pc,lr)

