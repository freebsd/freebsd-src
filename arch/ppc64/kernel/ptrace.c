/*
 *  linux/arch/ppc64/kernel/ptrace.c
 *
 *  PowerPC version
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Derived from "arch/m68k/kernel/ptrace.c"
 *  Copyright (C) 1994 by Hamish Macdonald
 *  Taken from linux/kernel/ptrace.c and modified for M680x0.
 *  linux/kernel/ptrace.c is by Ross Biro 1/23/92, edited by Linus Torvalds
 *
 * Modified by Cort Dougan (cort@hq.fsmlabs.com)
 * and Paul Mackerras (paulus@linuxcare.com.au).
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file README.legal in the main directory of
 * this archive for more details.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/user.h>

#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/system.h>

/*
 * Set of msr bits that gdb can change on behalf of a process.
 */
#define MSR_DEBUGCHANGE	(MSR_FE0 | MSR_SE | MSR_BE | MSR_FE1)

/*
 * does not yet catch signals sent when the child dies.
 * in exit.c or in signal.c.
 */

/*
 * Get contents of register REGNO in task TASK.
 */
static inline unsigned long get_reg(struct task_struct *task, int regno)
{
	if (regno < sizeof(struct pt_regs) / sizeof(unsigned long))
		return ((unsigned long *)task->thread.regs)[regno];
	return (0);
}

/*
 * Write contents of register REGNO in task TASK.
 */
static inline int put_reg(struct task_struct *task, int regno,
			  unsigned long data)
{
	if (regno < PT_SOFTE) {
		if (regno == PT_MSR)
			data = (data & MSR_DEBUGCHANGE)
				| (task->thread.regs->msr & ~MSR_DEBUGCHANGE);
		((unsigned long *)task->thread.regs)[regno] = data;
		return 0;
	}
	return -EIO;
}

#ifdef CONFIG_ALTIVEC
/*
 * Get contents of AltiVec register state in task TASK
 */
static inline int get_vrregs(unsigned long data, struct task_struct *task)
{
	return (copy_to_user((void *)data,&task->thread.vr[0],
			offsetof(struct thread_struct,vrsave[2])-
			     offsetof(struct thread_struct,vr[0])) ? -EFAULT : 0 );
}

/*
 * Write contents of AltiVec register state into task TASK.
 */
static inline int set_vrregs(struct task_struct *task, unsigned long data)
{
	return (copy_from_user(&task->thread.vr[0],(void *)data,
			offsetof(struct thread_struct,vrsave[2])-
			     offsetof(struct thread_struct,vr[0])) ? -EFAULT : 0 );
}
#endif

static inline void
set_single_step(struct task_struct *task)
{
	struct pt_regs *regs = task->thread.regs;
	if (regs != NULL)
		regs->msr |= MSR_SE;
}

static inline void
clear_single_step(struct task_struct *task)
{
	struct pt_regs *regs = task->thread.regs;
	if (regs != NULL)
		regs->msr &= ~MSR_SE;
}

/*
 * Called by kernel/ptrace.c when detaching..
 *
 * Make sure single step bits etc are not set.
 */
void ptrace_disable(struct task_struct *child)
{
	/* make sure the single step bit is not set. */
	clear_single_step(child);
}

int sys_ptrace(long request, long pid, long addr, long data)
{
	struct task_struct *child;
	int ret = -EPERM;

	lock_kernel();
	if (request == PTRACE_TRACEME) {
		/* are we already being traced? */
		if (current->ptrace & PT_PTRACED)
			goto out;
		/* set the ptrace bit in the process flags. */
		current->ptrace |= PT_PTRACED;
		ret = 0;
		goto out;
	}
	ret = -ESRCH;
	read_lock(&tasklist_lock);
	child = find_task_by_pid(pid);
	if (child)
		get_task_struct(child);
	read_unlock(&tasklist_lock);
	if (!child)
		goto out;

	ret = -EPERM;
	if (pid == 1)		/* you may not mess with init */
		goto out_tsk;

	if (request == PTRACE_ATTACH) {
		ret = ptrace_attach(child);
		goto out_tsk;
	}

	ret = ptrace_check_attach(child, request == PTRACE_KILL);
	if (ret < 0)
		goto out_tsk;

	switch (request) {
	/* when I and D space are separate, these will need to be fixed. */
	case PTRACE_PEEKTEXT: /* read word at location addr. */ 
	case PTRACE_PEEKDATA: {
		unsigned long tmp;
		int copied;

		copied = access_process_vm(child, addr, &tmp, sizeof(tmp), 0);
		ret = -EIO;
		if (copied != sizeof(tmp))
			break;
		ret = put_user(tmp,(unsigned long *) data);
		break;
	}

	/* read the word at location addr in the USER area. */
	case PTRACE_PEEKUSR: {
		unsigned long index;
		unsigned long tmp;

		ret = -EIO;
		/* convert to index and check */
		index = (unsigned long) addr >> 3;
		if ((addr & 7) || (index > PT_FPSCR))
			break;

		if (index < PT_FPR0) {
			tmp = get_reg(child, (int)index);
		} else {
			if (child->thread.regs->msr & MSR_FP)
				giveup_fpu(child);
			tmp = ((unsigned long *)child->thread.fpr)[index - PT_FPR0];
		}
		ret = put_user(tmp,(unsigned long *) data);
		break;
	}

	/* If I and D space are separate, this will have to be fixed. */
	case PTRACE_POKETEXT: /* write the word at location addr. */
	case PTRACE_POKEDATA:
		ret = 0;
		if (access_process_vm(child, addr, &data, sizeof(data), 1)
				== sizeof(data))
			break;
		ret = -EIO;
		break;

	/* write the word at location addr in the USER area */
	case PTRACE_POKEUSR: {
		unsigned long index;

		ret = -EIO;
		/* convert to index and check */
		index = (unsigned long) addr >> 3;
		if ((addr & 7) || (index > PT_FPSCR))
			break;

		if (index == PT_ORIG_R3)
			break;
		if (index < PT_FPR0) {
			ret = put_reg(child, index, data);
		} else {
			if (child->thread.regs->msr & MSR_FP)
				giveup_fpu(child);
			((unsigned long *)child->thread.fpr)[index - PT_FPR0] = data;
			ret = 0;
		}
		break;
	}

	case PTRACE_SYSCALL: /* continue and stop at next (return from) syscall */
	case PTRACE_CONT: { /* restart after signal. */
		ret = -EIO;
		if ((unsigned long) data > _NSIG)
			break;
		if (request == PTRACE_SYSCALL)
			child->ptrace |= PT_TRACESYS;
		else
			child->ptrace &= ~PT_TRACESYS;
		child->exit_code = data;
		/* make sure the single step bit is not set. */
		clear_single_step(child);
		wake_up_process(child);
		ret = 0;
		break;
	}

	/*
	 * make the child exit.  Best I can do is send it a sigkill.
	 * perhaps it should be put in the status that it wants to
	 * exit.
	 */
	case PTRACE_KILL: {
		ret = 0;
		if (child->state == TASK_ZOMBIE)	/* already dead */
			break;
		child->exit_code = SIGKILL;
		/* make sure the single step bit is not set. */
		clear_single_step(child);
		wake_up_process(child);
		break;
	}

	case PTRACE_SINGLESTEP: {  /* set the trap flag. */
		ret = -EIO;
		if ((unsigned long) data > _NSIG)
			break;
		child->ptrace &= ~PT_TRACESYS;
		set_single_step(child);
		child->exit_code = data;
		/* give it a chance to run. */
		wake_up_process(child);
		ret = 0;
		break;
	}

	case PTRACE_DETACH:
		ret = ptrace_detach(child, data);
		break;

	case PPC_PTRACE_GETREGS: { /* Get GPRs 0 - 31. */
		int i;
		unsigned long *reg = &((unsigned long *)child->thread.regs)[0];
		unsigned long *tmp = (unsigned long *)addr;

		for (i = 0; i < 32; i++) {
			ret = put_user(*reg, tmp);
			if (ret)
				break;
			reg++;
			tmp++;
		}
		break;
	}

	case PPC_PTRACE_SETREGS: { /* Set GPRs 0 - 31. */
		int i;
		unsigned long *reg = &((unsigned long *)child->thread.regs)[0];
		unsigned long *tmp = (unsigned long *)addr;

		for (i = 0; i < 32; i++) {
			ret = get_user(*reg, tmp);
			if (ret)
				break;
			reg++;
			tmp++;
		}
		break;
	}

	case PPC_PTRACE_GETFPREGS: { /* Get FPRs 0 - 31. */
		int i;
		unsigned long *reg = &((unsigned long *)child->thread.fpr)[0];
		unsigned long *tmp = (unsigned long *)addr;

		if (child->thread.regs->msr & MSR_FP)
			giveup_fpu(child);

		for (i = 0; i < 32; i++) {
			ret = put_user(*reg, tmp);
			if (ret)
				break;
			reg++;
			tmp++;
		}
		break;
	}

	case PPC_PTRACE_SETFPREGS: { /* Get FPRs 0 - 31. */
		int i;
		unsigned long *reg = &((unsigned long *)child->thread.fpr)[0];
		unsigned long *tmp = (unsigned long *)addr;

		if (child->thread.regs->msr & MSR_FP)
			giveup_fpu(child);

		for (i = 0; i < 32; i++) {
			ret = get_user(*reg, tmp);
			if (ret)
				break;
			reg++;
			tmp++;
		}
		break;
	}
#ifdef CONFIG_ALTIVEC
	case PTRACE_GETVRREGS:
		/* Get the child altivec register state. */
		if (child->thread.regs->msr & MSR_VEC)
			giveup_altivec(child);
		ret = get_vrregs(data, child);
		break;

	case PTRACE_SETVRREGS:
		/* Set the child altivec register state. */
		/* this is to clear the MSR_VEC bit to force a reload
		 * of register state from memory */
		if (child->thread.regs->msr & MSR_VEC)
			giveup_altivec(child);
		ret = set_vrregs(child,data);
		break;
#endif

	default:
		ret = -EIO;
		break;
	}
out_tsk:
	free_task_struct(child);
out:
	unlock_kernel();
	return ret;
}

void syscall_trace(void)
{
  if ((current->ptrace & (PT_PTRACED|PT_TRACESYS))
			!= (PT_PTRACED|PT_TRACESYS))
		return;
	current->exit_code = SIGTRAP;
	current->state = TASK_STOPPED;
	notify_parent(current, SIGCHLD);
	schedule();
	/*
	 * this isn't the same as continuing with a signal, but it will do
	 * for normal use.  strace only continues with a signal if the
	 * stopping signal is not SIGTRAP.  -brl
	 */
	if (current->exit_code) {
		send_sig(current->exit_code, current, 1);
		current->exit_code = 0;
	}
}
