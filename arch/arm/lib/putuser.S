/*
 *  linux/arch/arm/lib/putuser.S
 *
 *  Copyright (C) 2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Idea from x86 version, (C) Copyright 1998 Linus Torvalds
 *
 * These functions have a non-standard call interface to make
 * them more efficient, especially as they return an error
 * value in addition to the "real" return value.
 *
 * __put_user_X
 *
 * Inputs:	r0 contains the address
 *		r1, r2 contains the value
 * Outputs:	r0 is the error code
 *		lr corrupted
 *
 * No other registers must be altered.  (see include/asm-arm/uaccess.h
 * for specific ASM register usage).
 *
 * Note that ADDR_LIMIT is either 0 or 0xc0000000
 * Note also that it is intended that __put_user_bad is not global.
 */
#include <asm/constants.h>

	.global	__put_user_1
__put_user_1:
	bic	r2, sp, #0x1f00
	bic	r2, r2, #0x00ff
	ldr	r2, [r2, #TSK_ADDR_LIMIT]
	sub	r2, r2, #1
	cmp	r0, r2
1:	strlsbt	r1, [r0]
	movls	r0, #0
	movls	pc, lr
	b	__put_user_bad

	.global	__put_user_2
__put_user_2:
	bic	r2, sp, #0x1f00
	bic	r2, r2, #0x00ff
	ldr	r2, [r2, #TSK_ADDR_LIMIT]
	sub	r2, r2, #2
	cmp	r0, r2
2:	strlsbt	r1, [r0], #1
	movls	r1, r1, lsr #8
3:	strlsbt	r1, [r0]
	movls	r0, #0
	movls	pc, lr
	b	__put_user_bad

	.global	__put_user_4
__put_user_4:
	bic	r2, sp, #0x1f00
	bic	r2, r2, #0x00ff
	ldr	r2, [r2, #TSK_ADDR_LIMIT]
	sub	r2, r2, #4
	cmp	r0, r2
4:	strlst	r1, [r0]
	movls	r0, #0
	movls	pc, lr
	b	__put_user_bad

	.global	__put_user_8
__put_user_8:
	bic	ip, sp, #0x1f00
	bic	ip, ip, #0x00ff
	ldr	ip, [ip, #TSK_ADDR_LIMIT]
	sub	ip, ip, #8
	cmp	r0, ip
5:	strlst	r1, [r0], #4
6:	strlst	r2, [r0]
	movls	r0, #0
	movls	pc, lr

	/* fall through */

__put_user_bad:
	mov	r0, #-14
	mov	pc, lr

.section __ex_table, "a"
	.long	1b, __put_user_bad
	.long	2b, __put_user_bad
	.long	3b, __put_user_bad
	.long	4b, __put_user_bad
	.long	5b, __put_user_bad
	.long	6b, __put_user_bad
.previous
