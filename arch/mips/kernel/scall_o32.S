/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1997, 1998, 1999, 2000, 2001 by Ralf Baechle
 * Copyright (C) 2001 MIPS Technologies, Inc.
 */
#include <linux/config.h>
#include <linux/errno.h>
#include <asm/asm.h>
#include <asm/current.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>
#include <asm/isadep.h>
#include <asm/sysmips.h>
#include <asm/unistd.h>

/* Highest syscall used of any syscall flavour */
#define MAX_SYSCALL_NO	__NR_O32_Linux + __NR_O32_Linux_syscalls

	.align  5
NESTED(handle_sys, PT_SIZE, sp)
	.set	noat
	SAVE_SOME
	STI
	.set	at

	lw	t1, PT_EPC(sp)		# skip syscall on return

	sltiu	t0, v0, MAX_SYSCALL_NO + 1 # check syscall number
	addiu	t1, 4			# skip to next instruction
	beqz	t0, illegal_syscall
	sw	t1, PT_EPC(sp)

	/* XXX Put both in one cacheline, should save a bit. */
	sll	t0, v0, 2
	lw	t2, sys_call_table(t0)	# syscall routine
	lbu	t3, sys_narg_table(v0)	# number of arguments
	beqz	t2, illegal_syscall

	subu	t0, t3, 5		# 5 or more arguments?
	sw	a3, PT_R26(sp)		# save a3 for syscall restarting
	bgez	t0, stackargs

stack_done:
        sw      a3, PT_R26(sp)          # save for syscall restart
	lw	t0, TASK_PTRACE($28)	# syscall tracing enabled?
	andi	t0, _PT_TRACESYS
	bnez	t0, trace_a_syscall

	jalr	t2			# Do The Real Thing (TM)

	li	t0, -EMAXERRNO - 1	# error?
	sltu	t0, t0, v0
	sw	t0, PT_R7(sp)		# set error flag
	beqz	t0, 1f

	negu	v0			# error
	sw	v0, PT_R0(sp)		# set flag for syscall restarting
1:	sw	v0, PT_R2(sp)		# result

fast_ret_from_sys_call:
ret_from_schedule:
	mfc0	t0, CP0_STATUS		# need_resched and signals atomic test
	ori	t0, t0, 1
	xori	t0, t0, 1
	mtc0	t0, CP0_STATUS
	SSNOP; SSNOP; SSNOP

	lw	t2, TASK_NEED_RESCHED($28)
	lw	v0, TASK_SIGPENDING($28)
	bnez	t2, reschedule
	bnez	v0, signal_return
restore_all:
	RESTORE_SOME
	RESTORE_SP_AND_RET

/* ------------------------------------------------------------------------ */

FEXPORT(ret_from_fork)
	move	a0, v0			# prev
	jal	schedule_tail
	lw	t0, TASK_PTRACE($28)	# syscall tracing enabled?
	andi	t0, _PT_TRACESYS
	bnez	t0, tracesys_exit

static_ret_from_sys_call:
	RESTORE_STATIC
	j	fast_ret_from_sys_call

/* ------------------------------------------------------------------------ */

/* ret_from_sys_call should be here but is in entry.S.  */

/* ------------------------------------------------------------------------ */

/* Put this behind restore_all for the sake of the branch prediction.  */
signal_return:
	.type	signal_return, @function

	mfc0	t0, CP0_STATUS
	ori	t0, t0, 1
	mtc0	t0, CP0_STATUS

	SAVE_STATIC
	move	a0, zero
	move	a1, sp
	jal	do_signal
	RESTORE_STATIC
	b	restore_all

/* ------------------------------------------------------------------------ */

reschedule:
	jal	schedule
	b	ret_from_schedule

/* ------------------------------------------------------------------------ */

trace_a_syscall:
	SAVE_STATIC
	sw	t2, PT_R1(sp)
	jal	syscall_trace
	lw	t2, PT_R1(sp)

	lw	a0, PT_R4(sp)		# Restore argument registers
	lw	a1, PT_R5(sp)
	lw	a2, PT_R6(sp)
	lw	a3, PT_R7(sp)
	jalr	t2

	li	t0, -EMAXERRNO - 1	# error?
	sltu	t0, t0, v0
	sw	t0, PT_R7(sp)		# set error flag
	beqz	t0, 1f

	negu	v0			# error
	sw	v0, PT_R0(sp)		# set flag for syscall restarting
1:	sw	v0, PT_R2(sp)		# result

tracesys_exit:
	jal	syscall_trace
	j	static_ret_from_sys_call

/* ------------------------------------------------------------------------ */

	/*
	 * More than four arguments.  Try to deal with it by copying the
	 * stack arguments from the user stack to the kernel stack.
	 * This Sucks (TM).
	 */
stackargs:
	lw	t0, PT_R29(sp)		# get old user stack pointer
	subu	t3, 4
	sll	t1, t3, 2		# stack valid?

	addu	t1, t0			# end address
	or	t0, t1
	bltz	t0, bad_stack		# -> sp is bad

	lw	t0, PT_R29(sp)		# get old user stack pointer
	PTR_LA	t1, 3f			# copy 1 to 2 arguments
	sll	t3, t3, 4
	subu	t1, t3
	jr	t1

	/* Ok, copy the args from the luser stack to the kernel stack */
	/*
	 * I know Ralf doesn't like nops but this avoids code
	 * duplication for R3000 targets (and this is the
	 * only place where ".set reorder" doesn't help).
	 * Harald.
	 */
	.set    push
	.set    noreorder
	.set	nomacro
1:	lw	t1, 20(t0)		# argument #6 from usp
	nop
	sw	t1, 20(sp)
	nop
2:	lw	t1, 16(t0)		# argument #5 from usp
	nop
	sw	t1, 16(sp)
	nop
3:	.set	pop

	j	stack_done		# go back

	.section __ex_table,"a"
	PTR	1b,bad_stack
	PTR	2b,bad_stack
	.previous

/* ------------------------------------------------------------------------ */

	/*
	 * The stackpointer for a call with more than 4 arguments is bad.
	 * We probably should handle this case a bit more drastic.
	 */
bad_stack:
	negu	v0			# error
	sw	v0, PT_R0(sp)
	sw	v0, PT_R2(sp)
	li	t0, 1			# set error flag
	sw	t0, PT_R7(sp)
	j	fast_ret_from_sys_call

/* ------------------------------------------------------------------------ */

	/*
	 * The system call does not exist in this kernel
	 */
illegal_syscall:
	lw	t0, TASK_PTRACE($28)	# syscall tracing enabled?
	andi	t0, _PT_TRACESYS
	beqz	t0, 1f

	SAVE_STATIC
	jal	syscall_trace
	li	t0, _PT_TRACESYS

1:	li	v0, ENOSYS		# error
	sw	v0, PT_R0(sp)		# set flag for syscall restarting
	sw	v0, PT_R2(sp)
	li	t1, 1			# set error flag
	sw	t1, PT_R7(sp)
	bnez	t0, tracesys_exit

	j	fast_ret_from_sys_call
END(handle_sys)

LEAF(mips_atomic_set)
	andi	v0, a1, 3		# must be word aligned
	bnez	v0, bad_alignment

	lw	v1, THREAD_CURDS($28)	# in legal address range?
	addiu	a0, a1, 4
	or	a0, a0, a1
	and	a0, a0, v1
	bltz	a0, bad_address

#ifdef CONFIG_CPU_HAS_LLSC
	/* Ok, this is the ll/sc case.  World is sane :-)  */
1:	ll	v0, (a1)
	move	a0, a2
2:	sc	a0, (a1)
	beqz	a0, 1b

	.section __ex_table,"a"
	PTR	1b, bad_stack
	PTR	2b, bad_stack
	.previous
#else
	sw	a1, 16(sp)
	sw	a2, 20(sp)

	move	a0, sp
	move	a2, a1
	li	a1, 1
	jal	do_page_fault

	lw	a1, 16(sp)
	lw	a2, 20(sp)

	/*
	 * At this point the page should be readable and writable unless
	 * there was no more memory available.
	 */
1:	lw	v0, (a1)
2:	sw	a2, (a1)

	.section __ex_table,"a"
	PTR	1b, no_mem
	PTR	2b, no_mem
	.previous
#endif

	sw	zero, PT_R7(sp)		# success
	sw	v0, PT_R2(sp)		# result

	/* Success, so skip usual error handling garbage.  */
	lw	t0, TASK_PTRACE($28)	# syscall tracing enabled?
	andi	t0, _PT_TRACESYS
	beqz	t0, fast_ret_from_sys_call

	SAVE_STATIC
	jal	syscall_trace
	j	static_ret_from_sys_call

no_mem:	li	v0, -ENOMEM
	jr	ra

bad_address:
	li	v0, -EFAULT
	jr	ra

bad_alignment:
	li	v0, -EINVAL
	jr	ra
END(mips_atomic_set)

LEAF(sys_sysmips)
	beq	a0, MIPS_ATOMIC_SET, mips_atomic_set
	j	_sys_sysmips
END(sys_sysmips)

LEAF(sys_syscall)
	lw	t0, PT_R29(sp)		# user sp

	sltu	v0, a0, __NR_O32_Linux + __NR_O32_Linux_syscalls + 1
	beqz	v0, enosys

	sll	v0, a0, 2
	la	v1, sys_syscall
	lw	t2, sys_call_table(v0)	# function pointer
	lbu	t4, sys_narg_table(a0)	# number of arguments

	li	v0, -EINVAL
	beq	t2, v1, out		# do not recurse

	beqz	t2, enosys		# null function pointer?

	andi	v0, t0, 0x3		# unaligned stack pointer?
	bnez	v0, sigsegv

	addu	v0, t0, 16		# v0 = usp + 16
	addu	t1, v0, 12		# 3 32-bit arguments
	lw	v1, THREAD_CURDS($28)
	or	v0, v0, t1
	and	v1, v1, v0
	bltz	v1, efault

	move	a0, a1			# shift argument registers
	move	a1, a2
	move	a2, a3

1:	lw	a3, 16(t0)
2:	lw	t3, 20(t0)
3:	lw	t4, 24(t0)

	.section	__ex_table, "a"
	.word	1b, efault
	.word	2b, efault
	.word	3b, efault
	.previous

	sw	t3, 16(sp)		# put into new stackframe
	sw	t4, 20(sp)

	bnez	t4, 1f			# zero arguments?
	addu	a0, sp, 32		# then pass sp in a0
1:

	sw	t3, 16(sp)
	sw	v1, 20(sp)
	jr	t2
	/* Unreached */

enosys:	li	v0, -ENOSYS
	b	out

sigsegv:
	li	a0, _SIGSEGV
	move	a1, $28
	jal	force_sig
	/* Fall through */

efault:	li	v0, -EFAULT

out:	jr	ra
END(sys_syscall)
