/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 - 2000 by Ralf Baechle and others.
 * Copyright (C) 1999 Silicon Graphics, Inc.
 */
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/personality.h>
#include <linux/slab.h>
#include <linux/mman.h>
#include <linux/sys.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/init.h>
#include <linux/completion.h>

#include <asm/bootinfo.h>
#include <asm/cpu.h>
#include <asm/fpu.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/mipsregs.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/elf.h>
#include <asm/isadep.h>
#include <asm/inst.h>

ATTRIB_NORET void cpu_idle(void)
{
	/* endless idle loop with no priority at all */
	current->nice = 20;
	current->counter = -100;
	init_idle();

	while (1) {
		while (!current->need_resched)
			if (cpu_wait)
				(*cpu_wait)();
		schedule();
		check_pgt_cache();
	}
}

asmlinkage void ret_from_fork(void);

void start_thread(struct pt_regs * regs, unsigned long pc, unsigned long sp)
{
	regs->cp0_status &= ~(ST0_CU0|ST0_KSU|ST0_CU1);
       	regs->cp0_status |= KU_USER;
	current->used_math = 0;
	lose_fpu();
	regs->cp0_epc = pc;
	regs->regs[29] = sp;
	current->thread.current_ds = USER_DS;
}

void exit_thread(void)
{
}

void flush_thread(void)
{
}

int copy_thread(int nr, unsigned long clone_flags, unsigned long usp,
		 unsigned long unused,
                 struct task_struct * p, struct pt_regs * regs)
{
	struct pt_regs * childregs;
	long childksp;

	childksp = (unsigned long)p + KERNEL_STACK_SIZE - 32;

	if (is_fpu_owner()) {
		save_fp(p);
	}
	
	/* set up new TSS. */
	childregs = (struct pt_regs *) childksp - 1;
	*childregs = *regs;
	childregs->regs[7] = 0;	/* Clear error flag */
	if(current->personality == PER_LINUX) {
		childregs->regs[2] = 0;	/* Child gets zero as return value */
		regs->regs[2] = p->pid;
	} else {
		/* Under IRIX things are a little different. */
		childregs->regs[2] = 0;
		childregs->regs[3] = 1;
		regs->regs[2] = p->pid;
		regs->regs[3] = 0;
	}
	if (childregs->cp0_status & ST0_CU0) {
		childregs->regs[28] = (unsigned long) p;
		childregs->regs[29] = childksp;
		p->thread.current_ds = KERNEL_DS;
	} else {
		childregs->regs[29] = usp;
		p->thread.current_ds = USER_DS;
	}
	p->thread.reg29 = (unsigned long) childregs;
	p->thread.reg31 = (unsigned long) ret_from_fork;

	/*
	 * New tasks lose permission to use the fpu. This accelerates context
	 * switching for most programs since they don't use the fpu.
	 */
	p->thread.cp0_status = read_c0_status() &
                            ~(ST0_CU2|ST0_CU1|KU_MASK);
	childregs->cp0_status &= ~(ST0_CU2|ST0_CU1);

	return 0;
}

/* Fill in the fpu structure for a core dump.. */
int dump_fpu(struct pt_regs *regs, elf_fpregset_t *r)
{
	memcpy(r, &current->thread.fpu, sizeof(current->thread.fpu));
	return 1;
}

/*
 * Create a kernel thread
 */
int arch_kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	long retval;

	__asm__ __volatile__(
		"	.set	noreorder	\n"
		"	move    $6, $sp		\n"
		"	move    $4, %5		\n"
		"	li      $2, %1		\n"
		"	syscall			\n"
		"	beq     $6, $sp, 1f	\n"
		"	 subu    $sp, 32	\n"
		"	jalr    %4		\n"
		"	 move    $4, %3		\n"
		"	move    $4, $2		\n"
		"	li      $2, %2		\n"
		"	syscall			\n"
		"1:	addiu   $sp, 32		\n"
		"	move    %0, $2		\n"
		"	.set	reorder"
		: "=r" (retval)
		: "i" (__NR_clone), "i" (__NR_exit), "r" (arg), "r" (fn),
		  "r" (flags | CLONE_VM)
		 /*
		  * The called subroutine might have destroyed any of the
		  * at, result, argument or temporary registers ...
		  */
		: "$2", "$3", "$4", "$5", "$6", "$7", "$8",
		  "$9","$10","$11","$12","$13","$14","$15","$24","$25", "$31");

	return retval;
}

/*
 * These bracket the sleeping functions..
 */
extern void scheduling_functions_start_here(void);
extern void scheduling_functions_end_here(void);
#define first_sched	((unsigned long) scheduling_functions_start_here)
#define last_sched	((unsigned long) scheduling_functions_end_here)

struct mips_frame_info schedule_frame;
static struct mips_frame_info schedule_timeout_frame;
static struct mips_frame_info sleep_on_frame;
static struct mips_frame_info sleep_on_timeout_frame;
static struct mips_frame_info wait_for_completion_frame;
static int mips_frame_info_initialized;
static int __init get_frame_info(struct mips_frame_info *info, void *func)
{
	int i;
	union mips_instruction *ip = (union mips_instruction *)func;
	info->pc_offset = -1;
	info->frame_offset = -1;
	for (i = 0; i < 128; i++, ip++) {
		/* if jal, jalr, jr, stop. */
		if (ip->j_format.opcode == jal_op ||
		    (ip->r_format.opcode == spec_op &&
		     (ip->r_format.func == jalr_op ||
		      ip->r_format.func == jr_op)))
			break;
		if (ip->i_format.opcode == sw_op &&
		    ip->i_format.rs == 29) {
			/* sw $ra, offset($sp) */
			if (ip->i_format.rt == 31) {
				if (info->pc_offset != -1)
					break;
				info->pc_offset =
					ip->i_format.simmediate / sizeof(long);
			}
			/* sw $s8, offset($sp) */
			if (ip->i_format.rt == 30) {
				if (info->frame_offset != -1)
					break;
				info->frame_offset =
					ip->i_format.simmediate / sizeof(long);
			}
		}
	}
	if (info->pc_offset == -1 || info->frame_offset == -1) {
		printk("Can't analyze prologue code at %p\n", func);
		info->pc_offset = -1;
		info->frame_offset = -1;
		return -1;
	}

	return 0;
}
void __init frame_info_init(void)
{
	mips_frame_info_initialized =
		!get_frame_info(&schedule_frame, schedule) &&
		!get_frame_info(&schedule_timeout_frame, schedule_timeout) &&
		!get_frame_info(&sleep_on_frame, sleep_on) &&
		!get_frame_info(&sleep_on_timeout_frame, sleep_on_timeout) &&
		!get_frame_info(&wait_for_completion_frame, wait_for_completion);
}

/* get_wchan - a maintenance nightmare^W^Wpain in the ass ...  */
unsigned long get_wchan(struct task_struct *p)
{
	unsigned long frame, pc;

	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	if (!mips_frame_info_initialized)
		return 0;
	pc = thread_saved_pc(&p->thread);
	if (pc < first_sched || pc >= last_sched) {
		return pc;
	}

	if (pc >= (unsigned long) sleep_on_timeout)
		goto schedule_timeout_caller;
	if (pc >= (unsigned long) sleep_on)
		goto schedule_caller;
	if (pc >= (unsigned long) interruptible_sleep_on_timeout)
		goto schedule_timeout_caller;
	if (pc >= (unsigned long)interruptible_sleep_on)
		goto schedule_caller;
	if (pc >= (unsigned long)wait_for_completion)
		goto schedule_caller;
	goto schedule_timeout_caller;

schedule_caller:
	frame = ((unsigned long *)p->thread.reg30)[schedule_frame.frame_offset];
	if (pc >= (unsigned long) sleep_on)
		pc = ((unsigned long *)frame)[sleep_on_frame.pc_offset];
	else
		pc = ((unsigned long *)frame)[wait_for_completion_frame.pc_offset];
	return pc;

schedule_timeout_caller:
	/*
	 * The schedule_timeout frame
	 */
	frame = ((unsigned long *)p->thread.reg30)[schedule_frame.frame_offset];

	/*
	 * frame now points to sleep_on_timeout's frame
	 */
	pc    = ((unsigned long *)frame)[schedule_timeout_frame.pc_offset];

	if (pc >= first_sched && pc < last_sched) {
		/* schedule_timeout called by [interruptible_]sleep_on_timeout */
		frame = ((unsigned long *)frame)[schedule_timeout_frame.frame_offset];
		pc    = ((unsigned long *)frame)[sleep_on_timeout_frame.pc_offset];
	}

	return pc;
}
