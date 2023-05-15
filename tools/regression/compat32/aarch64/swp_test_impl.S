/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Warner Losh
 * Copyright (c) 2023 Stormshield
 * Copyright (c) 2023 Klara, Inc.
 */

#include <sys/syscall.h>

#define	STDOUT_FILENO	1

#define	MUTEX_LOCKED	0x01
#define	MUTEX_UNLOCKED	0x00

#define	STACK_SIZE	4096
#define	TLS_SIZE	4096

	.text
	.file "swp_test.S"
	.syntax unified
	.globl main
	.p2align 2
	.type main,%function
	.code 32

main:
	/*
	 * Stack slots:
	 * 0 - Sync word
	 * 1 - Thread id
	 * 2 - Shared word
	 */
	sub sp, sp, #12

	/* Print a message */
	movw r0, :lower16:.L.mainmsg
	movt r0, :upper16:.L.mainmsg
	ldr r1, =(.L.mainmsgEnd - .L.mainmsg - 1)
	bl print

	/* Create two secondary threads */
	mov r0, #1
	str r0, [sp, #4]	/* Thread ID */
	movw r0, :lower16:secondary_thread
	movt r0, :upper16:secondary_thread
	mov r1, sp
	movw r2, :lower16:stack1
	movt r2, :upper16:stack1
	movw r3, :lower16:tls1
	movt r3, :upper16:tls1
	bl create_thr

1:
	/*
	 * Wait for the first new thread to ack its existence by
	 * incrementing the thread id.
	 */
	ldr r0, [sp, #4]
	cmp r0, #1
	bne 2f
	ldr r7, =SYS_sched_yield
	swi 0
	b 1b

2:
	/* Create thread #2 */
	movw r0, :lower16:secondary_thread
	movt r0, :upper16:secondary_thread
	mov r1, sp
	movw r2, :lower16:stack2
	movt r2, :upper16:stack2
	movw r3, :lower16:tls2
	movt r3, :upper16:tls2
	bl create_thr

3:
	/*
	 * Wait for the first new thread to ack its existence by
	 * incrementing the thread id.
	 */
	ldr r0, [sp, #4]
	cmp r0, #2
	bne 4f
	ldr r7, =SYS_sched_yield
	swi 0
	b 3b

	/* Loop */
4:
	mov r0, sp
	mov r1, #0	/* Thread loop */
	add r2, sp, #8
	bl thread_loop
	b 4b

	/* UNREACHABLE */
	mov r0, #0
	ldr r7, =SYS_exit
	swi 0

	.p2align 2
	.type secondary_thread,%function
	.code 32
secondary_thread:
	/*
	 * On entry, r0 is where we stashed our sync word and
	 * ack word (thread ID).
	 *
	 * Stash the sync word in r4, thread ID in r5.
	 */
	mov r4, r0
	ldr r5, [r0, #4]

	/* Print a message */
	movw r0, :lower16:.L.secondarymsg
	movt r0, :upper16:.L.secondarymsg
	ldr r1, =(.L.secondarymsgEnd - .L.secondarymsg - 1)
	bl print

	/* Acknowledge that we started */
	add r0, r5, #1
	str r0, [r4, #4]

1:
	mov r0, r4
	mov r1, r5
	add r2, r4, #8
	bl thread_loop
	b 1b

	.p2align 2
	.type thread_loop,%function
	.code 32
thread_loop:
	push {r4, r5, r6, r7, r8, lr}

	/*
	 * r0 == sync word
	 * r1 == thread ID
	 * r2 == shared word
	 */
	mov r4, r0
	mov r5, r1
	mov r6, r2
	bl lock_mutex_swp
	str r5, [r6] /* Write the thread ID */
	bl random_cycles

	# Save off the now cycle count */
	mov r8, r0

	/* Print the thread ID and cycle count */
	mov r0, r5
	mov r1, #0
	bl printnum

	/* Separator */
	movw r0, :lower16:.L.idsep
	movt r0, :upper16:.L.idsep
	ldr r1, =(.L.idsepEnd - .L.idsep - 1)
	bl print

	/* Cycle count */
	mov r0, r8
	mov r1, #1
	bl printnum

1:
	ldr r0, [r6]
	cmp r0, r5	/* Check against the thread ID */
	bne 2f
	str r5, [r6]

	/*
	 * Check if the count hit 0, otherwise go again.
	 */
	cmp r8, #0
	beq 3f
	sub r8, r8, #1
	b 1b

2:
	/* exit(1) */
	mov r0, #1
	ldr r7, =SYS_exit
	swi 0

3:
	mov r0, r4
	bl unlock_mutex_swp

	/*
	 * Yield to lower the chance that we end up re-acquiring, the other two
	 * threads are still actively trying to acquire the lock.
	 */
	ldr r7, =SYS_sched_yield
	swi 0

	pop {r4, r5, r6, r7, r8, lr}
	bx lr

	.p2align 2
	.type random_cycles,%function
	.code 32
random_cycles:
	/* Return a random number < 4k */
	sub sp, sp, #4
	mov r0, sp
	mov r1, #4
	mov r2, #0
	ldr r7, =SYS_getrandom
	swi 0

	/*
	 * Just truncate the result of getrandom(2)
	 * to put us within range.  Naive, but functional.
	 */
	ldr r0, [sp]
	mov r1, #0xfff
	and r0, r0, r1
	add sp, sp, #4
	bx lr

	/*
	 * lock_mutex_swp and unlock_mutex_swp lifted from
	 * ARM documentation on SWP/SWPB.
	 */
	.p2align 2
	.type lock_mutex_swp,%function
	.code 32
lock_mutex_swp:
	mov r2, #MUTEX_LOCKED
	swp r1, r2, [r0]	/* Swap in lock value. */
	cmp r1, r2		/* Check if we were locked already. */
	beq lock_mutex_swp	/* Retry if so */
	bx lr			/* Return locked */

	.p2align 2
	.type unlock_mutex_swp,%function
	.code 32
unlock_mutex_swp:
	mov r1, #MUTEX_UNLOCKED
	str r1, [r0]		/* Move in unlocked */
	bx lr

	.p2align 2
	.type create_thr,%function
	.code 32
create_thr:
	/*
	 * r0 == start_func
	 * r1 == arg
	 * r2 == stack_base
	 * r3 == tls_base
	 */
	sub sp, sp, #56
	str r0, [sp, #4] 	/* start_func */
	str r1, [sp, #8]	/* arg */
	str r2, [sp, #12]	/* stack_base */
	mov r0, #STACK_SIZE
	str r0, [sp, #16]	/* stack_size */
	str r3, [sp, #20]	/* tls_base */
	mov r0, #TLS_SIZE
	str r0, [sp, #24]	/* tls_size */
	mov r0, #0
	str r0, [sp, #28]
	str r0, [sp, #32]
	str r0, [sp, #36]
	str r0, [sp, #40]

	add r0, sp, #4	/* &thrp */
	mov r1, #52	/* sizeof(thrp) */
	ldr r7, =SYS_thr_new
	swi 0

	add sp, sp, #56
	bx lr

	.p2align 2
	.type printnum,%function
	.code 32
printnum:
	push {r4, r5, r6, r7, r8, r10, lr}
	sub sp, #4

	/* 1000000000 */
	movw r6, #0xca00
	movt r6, #0x3b9a

	udiv r5, r0, r6
	cmp r5, #9
	bhi abort

	/* r4 is our accumulator */
	mov r4, r0
	/* r5 to be used as our "significant bit" */
	mov r5, #0
	/* r10 is "output_newline" */
	mov r10, r1

1:
	cmp r6, #0
	beq 4f

	/* Divide by current place */
	udiv r0, r4, r6
	/* Significant already? print anyways */
	cmp r5, #0
	bne 2f

	/*
	 * Not significant, maybe print.  If we made it all the way to 1, we
	 * need to just print the 0 anyways.
	 */
	cmp r6, #1
	beq 2f

	cmp r0, #0
	bne 2f
	b 3f	/* Proceed */

	/* Print */
2:
	mov r5, #1
	mov r8, r0
	add r0, r0, #0x30
	str r0, [sp]
	mov r0, sp
	mov r1, #1
	bl print

	/* Multiply back into place and subtract from accumulator */
	mul r0, r8, r6
	sub r4, r4, r0

3:
	mov r3, #10
	udiv r6, r6, r3
	b 1b

4:
	cmp r10, #0
	beq 5f

	/* newline */
	mov r0, #0x0a
	str r0, [sp]
	mov r0, sp
	mov r1, #1
	bl print

5:
	add sp, sp, #4
	pop {r4, r5, r6, r7, r8, r10, lr}
	bx lr

abort:
	movw r0, :lower16:.L.badnum
	movt r0, :upper16:.L.badnum
	ldr r1, =(.L.badnumEnd - .L.badnum - 1)
	bl print

	mov r0, #1
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

.L.mainmsg:
	.asciz "Main thread\n"
.L.mainmsgEnd:
	.size .L.mainmsg, .L.mainmsgEnd - .L.mainmsg
.L.secondarymsg:
	.asciz "Secondary thread\n"
.L.secondarymsgEnd:
	.size .L.secondarymsg, .L.secondarymsgEnd - .L.secondarymsg
.L.badnum:
	.asciz "Bad number\n"
.L.badnumEnd:
	.size .L.badnum, .L.badnumEnd - .L.badnum
.L.idsep:
	.asciz " - cycles "
.L.idsepEnd:
	.size .L.idsep, .L.idsepEnd - .L.idsep

	.type stack1,%object
	.local stack1
	.comm stack1,STACK_SIZE,1
	.type tls1,%object
	.local tls1
	.comm tls1,TLS_SIZE,1

	.type stack2,%object
	.local stack2
	.comm stack2,STACK_SIZE,1
	.type tls2,%object
	.local tls2
	.comm tls2,TLS_SIZE,1
