/* 
 * linux/arch/arm/boot/compressed/head-ftvpci.S
 * 
 * Copyright (C) 2000 FutureTV Labs Ltd.
 * 
 * Special startup code for FTV PCI board.
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

	.section        ".start", "ax"
ftv_start:
	mcr	p15, 0, r0, c7, c5, 0		@ flush I cache
	mrc	p15, 0, r0, c1, c0
	orr	r0, r0, #1 << 12
	mcr	p15, 0, r0, c1, c0		@ enable I cache
	mov	r0, #0
	mcreq	p15, 0, r0, c15, c1, 2		@ enable clock switching

	/* check to see if the kernel must be relocated */
	ldr	ip, =ftv_start
	adr	sl, ftv_start
	teq	ip, sl
	beq	2f				@ no need to copy

	/* in the wrong place -> presumably, executing out of ROM */
	sub	ip, ip, sl			@ displacement
	ldr	lr, =_start			@ destination
	sub	sp, lr, ip			@ source
	ldr	fp, =_edata			@ end of copied area
1:	ldmia	sp!, {r0, r1, r2, r3, r4, r5, r6, r10}
	stmia	lr!, {r0, r1, r2, r3, r4, r5, r6, r10}
	cmp	lr, fp
	ble	1b

2:
	mov	r8, #0
	mov	r7, #3
	b	1f
.ltorg
1:
	/* fall back into head.S */
