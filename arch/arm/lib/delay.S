/*
 *  linux/arch/arm/lib/delay.S
 *
 *  Copyright (C) 1995-2003 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/delay.h>

		.text

/*
 * See linux/include/asm-arm/delay.h for a description of the maths.
 */

#if NDELAY_FACTOR > 255
#error NDELAY_FACTOR out of range
#endif
#if UDELAY_FACTOR > 65535
#error UDELAY_FACTOR out of range
#endif

LC0:		.word	SYMBOL_NAME(loops_per_jiffy)

ENTRY(__const_delay)
		ldr	r3, LC0
		mov	r1, r0, lsr #11
		ldr	r2, [r3, #0]
		b	2f

ENTRY(__ndelay)
		mov	r2, #NDELAY_FACTOR
		b	1f

ENTRY(__udelay)
		mov	r2,     #UDELAY_FACTOR & 0xff00
		orr	r2, r2, #UDELAY_FACTOR & 0x00ff
1:		ldr	r3, LC0
		mul	r1, r0, r2
		ldr	r2, [r3, #0]
		mov	r1, r1, lsr #11
2:		mov	r2, r2, lsr #12
		mul	r0, r1, r2
		movs	r0, r0, lsr #5
		RETINSTR(moveq,pc,lr)

/*
 * loops = (r0 * 0x10c6 * HZ * loops_per_jiffy) / 2^32
 *
 * Oh, if only we had a cycle counter...
 */

ENTRY(__delay)
		subs	r0, r0, #1
		bcs	SYMBOL_NAME(__delay)
		RETINSTR(mov,pc,lr)
