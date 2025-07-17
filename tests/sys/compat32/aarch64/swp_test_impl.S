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

	movw r0, :lower16:.L.testheader
	movt r0, :upper16:.L.testheader
	ldr r1, =(.L.testheaderEnd - .L.testheader - 1)
	bl print

	/* Target address */
	mov r0, #0x03
	str r0, [sp]
	mov r0, sp

	/* Load value */
	mov r1, #SWP_MAGIC

	/* swp it */
	swp r0, r1, [r0]

	/* Old value should be 3 */
	cmp r0, #0x03
	bne 1f

	/* Check stack value */
	ldr r0, [sp]
	mov r1, #SWP_MAGIC
	cmp r0, r1
	bne 1f
	b 2f

1:
	/* Denote the failed test */
	add r4, #1
	/* "No" part of the notification */
	movw r0, :lower16:.L.boknot
	movt r0, :upper16:.L.boknot
	ldr r1, =(.L.boknotEnd - .L.boknot - 1)
	bl print

2:
	/* Notify */
	movw r0, :lower16:.L.ok1
	movt r0, :upper16:.L.ok1
	ldr r1, =(.L.ok1End - .L.ok1 - 1)
	bl print

	movw r5, #SWPB_MAGIC
	movt r5, #SWPB_MAGIC

	/* Using r6 as our accumulator */
	mov r6, sp
	/* Simplify the loop */
	sub r6, #1
3:
	/* Restore our magic value every time */
	str r5, [sp]
	/* Move on to the next byte */
	add r6, #1

	/* swp it in */
	mov r0, r6
	mov r1, #3
	swpb r0, r1, [r0]

	/* Check the old value */
	cmp r0, #0xc0
	bne 6f

	/* Check the stack value */
	ldrb r0, [r6]
	cmp r0, #0x03
	bne 6f

	/* Just loop over the rest of the word and check those values. */
	mov r1, r6
	sub r1, sp

	mov r0, #0x00
4:
	cmp r0, r1
	beq 5f

	/* Check the next byte */
	ldrb r3, [sp, r0]
	cmp r3, #0xc0
	bne 6f

5:
	add r0, #0x01
	cmp r0, #0x04
	/* Hit the end, this one succeeded */
	beq 7f

	/* Move on to the next byte */
	b 4b

6:
	/* Denote the failed test */
	add r4, #1
	/* "No" part of the notification */
	movw r0, :lower16:.L.boknot
	movt r0, :upper16:.L.boknot
	ldr r1, =(.L.boknotEnd - .L.boknot - 1)
	bl print

	/* FALLTHROUGH */
7:
	/* "ok" part */
	movw r0, :lower16:.L.bok
	movt r0, :upper16:.L.bok
	ldr r1, =(.L.bokEnd - .L.bok - 1)
	bl print

	/* test number */
	mov r0, r6
	sub r0, sp
	add r0, #0x32 /* "0" + 2 because we start at test 2. */
	mov r1, #0x01
	str r0, [sp]
	mov r0, sp
	bl print

	/* boklabel */
	movw r0, :lower16:.L.boklabel
	movt r0, :upper16:.L.boklabel
	ldr r1, =(.L.boklabelEnd - .L.boklabel - 1)
	bl print

	/* index */
	mov r0, r6
	sub r0, sp
	add r0, #0x30 /* "0" */
	str r0, [sp]
	mov r0, sp
	mov r1, #0x01
	bl print

	/* bokterm */
	movw r0, :lower16:.L.bokterm
	movt r0, :upper16:.L.bokterm
	ldr r1, =(.L.boktermEnd - .L.bokterm - 1)
	bl print

	mov r0, sp
	add r0, #0x3
	cmp r0, r6
	bne 3b

	mov r0, r4	/* retval */
	ldr r7, =SYS_exit
	swi 0

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
	.asciz "1..5\n"
.L.testheaderEnd:
	.size .L.testheader, .L.testheaderEnd - .L.testheader

	/* Maybe not the most efficient, but meh. */
.L.ok1:
	.asciz "ok 1 - swp\n"
.L.ok1End:
	.size .L.ok1, .L.ok1End - .L.ok1

.L.boknot:
	.asciz "not "
.L.boknotEnd:
	.size .L.boknot, .L.boknotEnd - .L.boknot
.L.bok:
	.asciz "ok "
.L.bokEnd:
	.size .L.bok, .L.bokEnd - .L.bok
.L.boklabel:
	.asciz " - swpb["
.L.boklabelEnd:
	.size .L.boklabel, .L.boklabelEnd - .L.boklabel
.L.bokterm:
	.asciz "]\n"
.L.boktermEnd:
	.size .L.bokterm, .L.boktermEnd - .L.bokterm
