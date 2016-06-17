/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Low level exception handling
 *
 * Copyright (C) 1994 - 2000, 2001 Ralf Baechle
 * Copyright (C) 1999, 2000 Silicon Graphics
 * Copyright (C) 2001 MIPS Technologies, Inc.
 */
#include <asm/asm.h>
#include <asm/regdef.h>
#include <asm/mipsregs.h>
#include <asm/offset.h>
#include <asm/stackframe.h>

#define KU_USER 0x10

		.text
		.align	5
FEXPORT(ret_from_fork)
		move	a0, v0			# prev
		jal	schedule_tail
		ld	t0, TASK_PTRACE($28)	# syscall tracing enabled?
		andi	t0, _PT_TRACESYS
		bnez	t0, tracesys_exit
		j	ret_from_sys_call

tracesys_exit:
		jal	syscall_trace
		b	ret_from_sys_call

EXPORT(ret_from_irq)
EXPORT(ret_from_exception)
		ld	t0, PT_STATUS(sp)	# returning to kernel mode?
		andi	t0, t0, KU_USER
		bnez	t0, ret_from_sys_call
		j	restore_all

reschedule:	jal	schedule

FEXPORT(ret_from_sys_call)
		mfc0	t0, CP0_STATUS	# need_resched and signals atomic test
		ori	t0, t0, 1
		xori	t0, t0, 1
		mtc0	t0, CP0_STATUS
		SSNOP; SSNOP; SSNOP

		ld	v0, TASK_NEED_RESCHED($28)
		lw	v1, TASK_SIGPENDING($28)
		bnez	v0, reschedule
		bnez	v1, signal_return

restore_all:	.set	noat
		RESTORE_ALL
		eret
		.set	at

signal_return:	.type	signal_return, @function

		mfc0	t0, CP0_STATUS
		ori	t0, t0, 1
		mtc0	t0, CP0_STATUS

		move	a0, zero
		move	a1, sp
		jal	do_signal
		b	restore_all

/*
 * Common spurious interrupt handler.
 */
		.text
		.align  5
LEAF(spurious_interrupt)
		/*
		 * Someone tried to fool us by sending an interrupt but we
		 * couldn't find a cause for it.
		 */
		lui     t1, %hi(irq_err_count)
1:		ll      t0, %lo(irq_err_count)(t1)
		addiu   t0, 1
		sc      t0, %lo(irq_err_count)(t1)
		beqz	t0, 1b
		j	ret_from_irq
		END(spurious_interrupt)
