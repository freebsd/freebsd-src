/*
 * r2300_switch.S: R2300 specific task switching code.
 *
 * Copyright (C) 1994, 1995, 1996, 1999 by Ralf Baechle
 * Copyright (C) 1994, 1995, 1996 by Andreas Busse
 *
 * Multi-cpu abstraction and macros for easier reading:
 * Copyright (C) 1996 David S. Miller (dm@engr.sgi.com)
 *
 * Further modifications to make this work:
 * Copyright (c) 1998-2000 Harald Koerfgen
 */
#include <linux/config.h>
#include <asm/asm.h>
#include <asm/cachectl.h>
#include <asm/current.h>
#include <asm/fpregdef.h>
#include <asm/mipsregs.h>
#include <asm/offset.h>
#include <asm/page.h>
#include <asm/pgtable-bits.h>
#include <asm/processor.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>

#include <asm/asmmacro.h>

	.set	mips1
	.align	5

#define PF_USEDFPU      0x00100000      /* task used FPU this quantum (SMP) */
#define ST_OFF (KERNEL_STACK_SIZE - 32 - PT_SIZE + PT_STATUS)

/*
 * [jsun] FPU context is saved if and only if the process has used FPU in 
 * the current run (PF_USEDFPU).  In any case, the CU1 bit for user space 
 * STATUS register should be 0, so that a process *always* starts its 
 * userland with FPU disabled after each context switch.
 *
 * FPU will be enabled as soon as the process accesses FPU again, through
 * do_cpu() trap.
 */

/*
 * task_struct *resume(task_struct *prev,
 *                     task_struct *next)
 */
LEAF(resume)
#ifndef CONFIG_CPU_HAS_LLSC
	sw      zero, ll_bit
#endif
	mfc0	t1, CP0_STATUS
	sw	t1, THREAD_STATUS(a0)
	CPU_SAVE_NONSCRATCH(a0)
	sw	ra, THREAD_REG31(a0)

	/* 
	 * check if we need to save FPU registers
	 */
	lw	t0, TASK_FLAGS(a0)
	li	t1, PF_USEDFPU
	and	t2, t0, t1
	beqz	t2, 1f
	nor	t1, zero, t1

	/*
	 * clear PF_USEDFPU bit in task flags
	 */
	and	t0, t0, t1
	sw	t0, TASK_FLAGS(a0)

	/*
	 * clear user-saved stack CU1 bit
	 */
	lw	t0, ST_OFF(a0)
	li	t1, ~ST0_CU1
	and	t0, t0, t1
	sw	t0, ST_OFF(a0)

	FPU_SAVE_SINGLE(a0, t0)			# clobbers t0

1:
	/*
	 * The order of restoring the registers takes care of the race
	 * updating $28, $29 and kernelsp without disabling ints.
	 */
	move	$28, a1
	CPU_RESTORE_NONSCRATCH($28)
	addiu	t0, $28, KERNEL_STACK_SIZE-32
	sw	t0, kernelsp
	mfc0	t1, CP0_STATUS		/* Do we really need this? */
	li	a3, 0xff00
	and	t1, a3
	lw	a2, THREAD_STATUS($28)
	nor	a3, $0, a3
	and	a2, a3
	or	a2, t1
	mtc0	a2, CP0_STATUS
	.set	noreorder
	jr	ra
	 move	v0, a0
	.set	reorder
	END(resume)

/*
 * Save a thread's fp context.
 */
LEAF(_save_fp)
	FPU_SAVE_SINGLE(a0, t1)			# clobbers t1
	jr	ra
	END(_save_fp)

/*
 * Restore a thread's fp context.
 */
LEAF(_restore_fp)
	FPU_RESTORE_SINGLE(a0, t1)		# clobbers t1
	jr	ra
	END(_restore_fp)

/*
 * Load the FPU with signalling NANS.  This bit pattern we're using has
 * the property that no matter wether considered as single or as double
 * precision represents signaling NANS.
 *
 * We initialize fcr31 to rounding to nearest, no exceptions.
 */

#define FPU_DEFAULT  0x00000000

LEAF(_init_fpu)
	mfc0	t0, CP0_STATUS
	li	t1, ST0_CU1
	or	t0, t1
	mtc0	t0, CP0_STATUS

	li	t1, FPU_DEFAULT
	ctc1	t1, fcr31

	li	t0, -1

	mtc1	t0, $f0
	mtc1	t0, $f1
	mtc1	t0, $f2
	mtc1	t0, $f3
	mtc1	t0, $f4
	mtc1	t0, $f5
	mtc1	t0, $f6
	mtc1	t0, $f7
	mtc1	t0, $f8
	mtc1	t0, $f9
	mtc1	t0, $f10
	mtc1	t0, $f11
	mtc1	t0, $f12
	mtc1	t0, $f13
	mtc1	t0, $f14
	mtc1	t0, $f15
	mtc1	t0, $f16
	mtc1	t0, $f17
	mtc1	t0, $f18
	mtc1	t0, $f19
	mtc1	t0, $f20
	mtc1	t0, $f21
	mtc1	t0, $f22
	mtc1	t0, $f23
	mtc1	t0, $f24
	mtc1	t0, $f25
	mtc1	t0, $f26
	mtc1	t0, $f27
	mtc1	t0, $f28
	mtc1	t0, $f29
	mtc1	t0, $f30
	.set	noreorder
	jr	ra
	 mtc1	t0, $f31
	.set	reorder
	END(_init_fpu)
