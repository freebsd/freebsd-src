/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Warner Losh
 * Copyright (c) 2023 Stormshield
 * Copyright (c) 2023 Klara, Inc.
 */

#include <sys/syscall.h>

#define	STDOUT_FILENO	1
#define	SWP_MAGIC	0xffc0
#define	SWPB_MAGIC	0xc0c0

	.text
	.file "swp_test.S"
	.syntax unified
	.globl main
	.p2align 2
	.type main,%function
	.code 32

main:
	sub sp, #0x04
	/* r4 is our failed test counter */
	mov r4, #0
	/* r6 is our current teset counter */
	mov r6, #1

	movw r0, :lower16:.L.testheader
	movt r0, :upper16:.L.testheader
	ldr r1, =(.L.testheaderEnd - .L.testheader - 1)
	bl print

	/* eq */
	bl reset
	mov r1, #SWP_MAGIC
	cmp r1, r1
	swpeq r0, r1, [r0]
	bl expect_success

	/* Returned 0 (bad) or 1 (ok) */
	cmp r0, #0
	beq 1f

	/* !eq */
	bl reset
	mov r1, #SWP_MAGIC
	mov r2, #0
	cmp r1, r2
	swpeq r0, r1, [r0]
	bl expect_fail

	/* Don't care about the return of the second one, just print */
1:
	movw r0, :lower16:.L.eq
	movt r0, :upper16:.L.eq
	ldr r1, =(.L.eqEnd - .L.eq - 1)
	bl print_result
	add r6, r6, #1	/* Next test */

	/* cs */
	bl reset
	mov r1, #SWP_MAGIC
	movw r3, #0xffff
	movt r3, #0xffff
	/* Overflow */
	adds r2, r3, r3
	swpcs r0, r1, [r0]
	bl expect_success

	/* Returned 0 (bad) or 1 (ok) */
	cmp r0, #0
	beq 2f

	/* !cs */
	bl reset
	mov r1, #SWP_MAGIC
	mov r3, #0x00
	adds r2, r3, #0x08
	swpcs r0, r1, [r0]
	bl expect_fail

	/* Don't care about the return of the second one, just print */
2:
	movw r0, :lower16:.L.cs
	movt r0, :upper16:.L.cs
	ldr r1, =(.L.csEnd - .L.cs - 1)
	bl print_result
	add r6, r6, #1	/* Next test */

	/* mi */
	bl reset
	mov r1, #SWP_MAGIC
	mov r2, #0
	/* Underflow */
	subs r2, r2, #0x05
	swpmi r0, r1, [r0]
	bl expect_success

	/* Returned 0 (bad) or 1 (ok) */
	cmp r0, #0
	beq 3f

	/* !mi */
	bl reset
	mov r1, #SWP_MAGIC
	mov r2, #0x10
	subs r2, r2, #0x08
	swpmi r0, r1, [r0]
	bl expect_fail

	/* Don't care about the return of the second one, just print */
3:
	movw r0, :lower16:.L.mi
	movt r0, :upper16:.L.mi
	ldr r1, =(.L.miEnd - .L.mi - 1)
	bl print_result
	add r6, r6, #1	/* Next test */

	/* vs */
	bl reset
	mov r1, #SWP_MAGIC
	movw r3, #0xffff
	movt r3, #0x7fff
	/* Overflow */
	adds r2, r3, #0x10
	swpvs r0, r1, [r0]
	bl expect_success

	/* Returned 0 (bad) or 1 (ok) */
	cmp r0, #0
	beq 4f

	/* !vs */
	bl reset
	mov r1, #SWP_MAGIC
	mov r3, #0x00
	adds r2, r3, #0x08
	swpvs r0, r1, [r0]
	bl expect_fail

	/* Don't care about the return of the second one, just print */
4:
	movw r0, :lower16:.L.vs
	movt r0, :upper16:.L.vs
	ldr r1, =(.L.vsEnd - .L.vs - 1)
	bl print_result
	add r6, r6, #1	/* Next test */

	/* hi */
	bl reset
	mov r1, #SWP_MAGIC
	mov r2, #0x00
	mov r3, #0x01
	cmp r3, r2
	swphi r0, r1, [r0]
	bl expect_success

	/* Returned 0 (bad) or 1 (ok) */
	cmp r0, #0
	beq 5f

	/* !hi */
	bl reset
	mov r1, #SWP_MAGIC
	mov r2, #0x00
	mov r3, #0x01
	cmp r2, r3
	swphi r0, r1, [r0]
	bl expect_fail

	/* Don't care about the return of the second one, just print */
5:
	movw r0, :lower16:.L.hi
	movt r0, :upper16:.L.hi
	ldr r1, =(.L.hiEnd - .L.hi - 1)
	bl print_result
	add r6, r6, #1	/* Next test */

	/* ge */
	bl reset
	mov r1, #SWP_MAGIC
	mov r2, #0x01
	cmp r2, r2
	swpge r0, r1, [r0]
	bl expect_success

	/* Returned 0 (bad) or 1 (ok) */
	cmp r0, #0
	beq 6f

	/* !ge */
	bl reset
	mov r1, #SWP_MAGIC
	mov r2, #0x00
	mov r3, #0x01
	cmp r2, r3
	swpge r0, r1, [r0]
	bl expect_fail

	/* Don't care about the return of the second one, just print */
6:
	movw r0, :lower16:.L.ge
	movt r0, :upper16:.L.ge
	ldr r1, =(.L.geEnd - .L.ge - 1)
	bl print_result
	add r6, r6, #1	/* Next test */

	/* gt */
	bl reset
	mov r1, #SWP_MAGIC
	mov r2, #0x00
	mov r3, #0x01
	cmp r3, r2
	swpgt r0, r1, [r0]
	bl expect_success

	/* Returned 0 (bad) or 1 (ok) */
	cmp r0, #0
	beq 7f

	/* !ge */
	bl reset
	mov r1, #SWP_MAGIC
	mov r2, #0x00
	mov r3, #0x01
	cmp r2, r3
	swpgt r0, r1, [r0]
	bl expect_fail

	/* Don't care about the return of the second one, just print */
7:
	movw r0, :lower16:.L.gt
	movt r0, :upper16:.L.gt
	ldr r1, =(.L.gtEnd - .L.gt - 1)
	bl print_result
	add r6, r6, #1	/* Next test */

	mov r0, r4	/* retval */
	ldr r7, =SYS_exit
	swi 0

	.p2align 2
	.type print_result,%function
	.code 32
print_result:
	push {r4, r5, lr}
	/* Save the label, size for our result */
	mov r4, r0
	mov r5, r1

	movw r0, :lower16:.L.ok
	movt r0, :upper16:.L.ok
	ldr r1, =(.L.okEnd - .L.ok - 1)
	bl print
	mov r0, r6
	add r0, #0x30 /* "0" + test number */
	mov r1, #0x01
	str r0, [sp]
	mov r0, sp
	bl print
	movw r0, :lower16:.L.swp
	movt r0, :upper16:.L.swp
	ldr r1, =(.L.swpEnd - .L.swp - 1)
	bl print
	mov r0, r4
	mov r1, r5
	bl print
	movw r0, :lower16:.L.term
	movt r0, :upper16:.L.term
	ldr r1, =(.L.termEnd - .L.term - 1)
	bl print

	pop {r4, r5, lr}
	bx lr

	.p2align 2
	.type reset,%function
	.code 32
reset:
	/* Reset sp[0] and return the address used */
	mov r0, #0x03
	str r0, [sp]
	mov r0, sp
	bx lr

	.p2align 2
	.type expect_fail,%function
	.code 32
expect_fail:
	/* Just check the stack value */
	ldr r0, [sp]
	mov r1, #0x03
	cmp r0,  r1
	bne 1f

	/* Success (not swapped) */
	mov r0, #1
	bx lr

1:
	/* Fail (swapped) */
	/* Print the "not" part */
	movw r0, :lower16:.L.not
	movt r0, :upper16:.L.not
	ldr r1, =(.L.notEnd - .L.not - 1)
	push {lr}
	bl print
	pop {lr}

	/* Failed */
	add r4, r4, #1
	mov r0, #0
	bx lr

	.p2align 2
	.type expect_success,%function
	.code 32
expect_success:
	/* Old value should be 3 */
	cmp r0, #0x03
	beq 1f
	b 3f

1:
	/* Check stack value */
	ldr r0, [sp]
	mov r1, #SWP_MAGIC
	cmp r0, r1
	beq 2f
	b 3f

2:
	mov r0, #1
	bx lr

3:
	/* Print the "not" part */
	movw r0, :lower16:.L.not
	movt r0, :upper16:.L.not
	ldr r1, =(.L.notEnd - .L.not - 1)
	push {lr}
	bl print
	pop {lr}

	/* Failed */
	add r4, r4, #1
	mov r0, #0
	bx lr

	.p2align 2
	.type print,%function
	.code 32
print:
	/* r0 - string, r1 = size */
	mov r2, r1
	mov r1, r0
	ldr r0, =STDOUT_FILENO
	ldr r7, =SYS_write
	swi 0

	bx lr

.L.testheader:
	.asciz "1..7\n"
.L.testheaderEnd:
	.size .L.testheader, .L.testheaderEnd - .L.testheader

.L.not:
	.asciz "not "
.L.notEnd:
	.size .L.not, .L.notEnd - .L.not
.L.ok:
	.asciz "ok "
.L.okEnd:
	.size .L.ok, .L.okEnd - .L.ok
.L.swp:
	.asciz " - swp"
.L.swpEnd:
	.size .L.swp, .L.swpEnd - .L.swp
.L.eq:
	.asciz "eq"
.L.eqEnd:
	.size .L.eq, .L.eqEnd - .L.eq
.L.cs:
	.asciz "cs"
.L.csEnd:
	.size .L.cs, .L.csEnd - .L.cs
.L.mi:
	.asciz "mi"
.L.miEnd:
	.size .L.mi, .L.miEnd - .L.mi
.L.vs:
	.asciz "vs"
.L.vsEnd:
	.size .L.vs, .L.vsEnd - .L.vs
.L.hi:
	.asciz "hi"
.L.hiEnd:
	.size .L.hi, .L.hiEnd - .L.hi
.L.ge:
	.asciz "ge"
.L.geEnd:
	.size .L.ge, .L.geEnd - .L.ge
.L.gt:
	.asciz "gt"
.L.gtEnd:
	.size .L.gt, .L.gtEnd - .L.gt
.L.term:
	.asciz "\n"
.L.termEnd:
	.size .L.term, .L.termEnd - .L.term
