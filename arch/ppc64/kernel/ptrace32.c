/*
 *  linux/arch/ppc64/kernel/ptrace32.c
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

#ifdef CONFIG_ALTIVEC
/*
 * Get contents of AltiVec register state in task TASK
 */
static inline int get_vrregs32(unsigned long data, struct task_struct *task)
{
	if(copy_to_user((void *)data,&task->thread.vr[0],
			offsetof(struct thread_struct,vrsave)-
			offsetof(struct thread_struct,vr[0])))
		return -EFAULT;
	data+=offsetof(struct thread_struct,vrsave[1])-
		offsetof(struct thread_struct,vr[0]);
	if (put_user(task->thread.vrsave[1],((u32 *)data)))
		return -EFAULT;
	return 0;
}

/*
 * Write contents of AltiVec register state into task TASK.
 */
static inline int set_vrregs32(struct task_struct *task, unsigned long data)
{
	if(copy_from_user(&task->thread.vr[0],(void *)data,
			offsetof(struct thread_struct,vrsave)-
			  offsetof(struct thread_struct,vr[0])))
		return -EFAULT;
	data+=offsetof(struct thread_struct,vrsave[1])-
		offsetof(struct thread_struct,vr[0]);
	if (get_user(task->thread.vrsave[1],((u32 *)data)))
		return -EFAULT;
	return 0;
}
#endif

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
 * (Put DATA into task TASK's register REGNO.)
 */
static inline int put_reg(struct task_struct *task, int regno, unsigned long data)
{
	if (regno < PT_SOFTE) 
  {
		if (regno == PT_MSR)
			data = (data & MSR_DEBUGCHANGE) | (task->thread.regs->msr & ~MSR_DEBUGCHANGE);
		((unsigned long *)task->thread.regs)[regno] = data;
		return 0;
	}
	return -EIO;
}

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

int sys32_ptrace(long request, long pid, unsigned long addr, unsigned long data)
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
	/* Read word at location ADDR */
	/* when I and D space are separate, these will need to be fixed. */
	case PTRACE_PEEKTEXT: /* read word at location addr. */ 
	case PTRACE_PEEKDATA: 
	{
		unsigned int  tmp_mem_value;
		int copied;

		copied = access_process_vm(child, addr, &tmp_mem_value, sizeof(tmp_mem_value), 0);
		ret = -EIO;
		if (copied != sizeof(tmp_mem_value))
			break;
		ret = put_user(tmp_mem_value, (u32*)data);  // copy 4 bytes of data into the user location specified by the 8 byte pointer in "data".
		break;
	}

	/*
	 * Read 4 bytes of the other process' storage
	 *  data is a pointer specifying where the user wants the
	 *	4 bytes copied into
	 *  addr is a pointer in the user's storage that contains an 8 byte
	 *	address in the other process of the 4 bytes that is to be read
	 * (this is run in a 32-bit process looking at a 64-bit process)
	 * when I and D space are separate, these will need to be fixed.
	 */
	case PPC_PTRACE_PEEKTEXT_3264:
	case PPC_PTRACE_PEEKDATA_3264: 
	{
		u32  tmp_mem_value;
		int  copied;
		u32* addrOthers;

		ret = -EIO;

		/* Get the addr in the other process that we want to read */
		if (get_user(addrOthers, (u32**)addr) != 0)
			break;

		copied = access_process_vm(child, (u64)addrOthers, &tmp_mem_value, sizeof(tmp_mem_value), 0);
		if (copied != sizeof(tmp_mem_value))
			break;
		ret = put_user(tmp_mem_value, (u32*)data);  // copy 4 bytes of data into the user location specified by the 8 byte pointer in "data".
		break;
	}

	/* Read a register (specified by ADDR) out of the "user area" */
	case PTRACE_PEEKUSR: {
		int index;
		unsigned int reg32bits;
		unsigned long tmp_reg_value;

		ret = -EIO;
		/* convert to index and check */
		index = (unsigned long) addr >> 2;
		if ((addr & 3) || (index > PT_FPSCR32))
			break;

		if (index < PT_FPR0) {
			tmp_reg_value = get_reg(child,  index);
		} else {
			if (child->thread.regs->msr & MSR_FP)
				giveup_fpu(child);
			/*
			 * the user space code considers the floating point
			 * to be an array of unsigned int (32 bits) - the
			 * index passed in is based on this assumption.
			 */
			tmp_reg_value = ((unsigned int *)child->thread.fpr)[index - PT_FPR0];
		}
		reg32bits = tmp_reg_value;
		ret = put_user(reg32bits, (u32*)data);  // copy 4 bytes of data into the user location specified by the 8 byte pointer in "data".
		break;
	}
  
	/*
	 * Read 4 bytes out of the other process' pt_regs area
	 *  data is a pointer specifying where the user wants the
	 *	4 bytes copied into
	 *  addr is the offset into the other process' pt_regs structure
	 *	that is to be read
	 * (this is run in a 32-bit process looking at a 64-bit process)
	 */
	case PPC_PTRACE_PEEKUSR_3264: {
		u32 index;
		u32 reg32bits;
		u64 tmp_reg_value;
		u32 numReg;
		u32 part;

		ret = -EIO;
		/* Determine which register the user wants */
		index = (u64)addr >> 2;  /* Divide addr by 4 */
		numReg = index / 2;
		/* Determine which part of the register the user wants */
		if (index % 2)
			part = 1;  /* want the 2nd half of the register (right-most). */
		else
			part = 0;  /* want the 1st half of the register (left-most). */

		/* Validate the input - check to see if address is on the wrong boundary or beyond the end of the user area */
		if ((addr & 3) || numReg > PT_FPSCR)
			break;

		if (numReg >= PT_FPR0) {
			if (child->thread.regs->msr & MSR_FP)
				giveup_fpu(child);
		        if (numReg == PT_FPSCR) 
			        tmp_reg_value = ((unsigned long *)child->thread.fpscr);
		        else 
			        tmp_reg_value = ((unsigned long *)child->thread.fpr)[numReg - PT_FPR0];
		} else { /* register within PT_REGS struct */
		    tmp_reg_value = get_reg(child, numReg);
		} 
                reg32bits = ((u32*)&tmp_reg_value)[part];
		ret = put_user(reg32bits, (u32*)data);  /* copy 4 bytes of data into the user location specified by the 8 byte pointer in "data". */
		break;
	}

	/* Write the word at location ADDR */
	/* If I and D space are separate, this will have to be fixed. */
	case PTRACE_POKETEXT: /* write the word at location addr. */
	case PTRACE_POKEDATA: {
		unsigned int tmp_value_to_write;
		tmp_value_to_write =  data;
		ret = 0;
		if (access_process_vm(child, addr, &tmp_value_to_write, sizeof(tmp_value_to_write), 1) == sizeof(tmp_value_to_write))
			break;
		ret = -EIO;
		break;
	}

	/*
	 * Write 4 bytes into the other process' storage
	 *  data is the 4 bytes that the user wants written
	 *  addr is a pointer in the user's storage that contains an
	 *	8 byte address in the other process where the 4 bytes
	 *	that is to be written
	 * (this is run in a 32-bit process looking at a 64-bit process)
	 * when I and D space are separate, these will need to be fixed.
	 */
	case PPC_PTRACE_POKETEXT_3264:
	case PPC_PTRACE_POKEDATA_3264:
	{
		u32  tmp_value_to_write = data;
		u32* addrOthers;
		int  bytesWritten;

		/* Get the addr in the other process that we want to write into */
		ret = -EIO;
		if (get_user(addrOthers,(u32**)addr) != 0)
			break;
		ret = 0;
		bytesWritten = access_process_vm(child, (u64)addrOthers, &tmp_value_to_write, sizeof(tmp_value_to_write), 1);
		if (bytesWritten == sizeof(tmp_value_to_write))
			break;
		ret = -EIO;
		break;
	}

	/* Write DATA into location ADDR within the USER area  */
	case PTRACE_POKEUSR: {
		unsigned long index;

		ret = -EIO;
		/* convert to index and check */
		index = (unsigned long) addr >> 2;
		if ((addr & 3) || (index > PT_FPSCR32))
			break;

		if (index == PT_ORIG_R3)
			break;
		if (index < PT_FPR0) {
			ret = put_reg(child, index, data);
		} else {
			if (child->thread.regs->msr & MSR_FP)
				giveup_fpu(child);
			/*
			 * the user space code considers the floating point
			 * to be an array of unsigned int (32 bits) - the
			 * index passed in is based on this assumption.
			 */
			((unsigned int *)child->thread.fpr)[index - PT_FPR0] = data;
			ret = 0;
		}
		break;
	}

	/*
	 * Write 4 bytes into the other process' pt_regs area
	 *  data is the 4 bytes that the user wants written
	 *  addr is the offset into the other process' pt_regs structure
	 *	that is to be written into
	 * (this is run in a 32-bit process looking at a 64-bit process)
	 */
	case PPC_PTRACE_POKEUSR_3264: {
		u32 index;
		u32 numReg;

		ret = -EIO;
		/* Determine which register the user wants */
		index = (u64)addr >> 2;  /* Divide addr by 4 */
		numReg = index / 2;
		/*
		 * Validate the input - check to see if address is on the
		 * wrong boundary or beyond the end of the user area
		 */
		if ((addr & 3) || (numReg > PT_FPSCR))
			break;
		/* Insure it is a register we let them change */
		if ((numReg == PT_ORIG_R3)
				|| ((numReg > PT_CCR) && (numReg < PT_FPR0)))
			break;
		if (numReg >= PT_FPR0) {
			if (child->thread.regs->msr & MSR_FP)
				giveup_fpu(child);
		}
		if (numReg == PT_MSR)
			data = (data & MSR_DEBUGCHANGE)
				| (child->thread.regs->msr & ~MSR_DEBUGCHANGE);
		((u32*)child->thread.regs)[index] = data;
		ret = 0;
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
		unsigned int *tmp = (unsigned int *)addr;

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
		unsigned int *tmp = (unsigned int *)addr;

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
		unsigned int *tmp = (unsigned int *)addr;

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
		unsigned int *tmp = (unsigned int *)addr;

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
		ret = get_vrregs32((unsigned long)data, child);
		break;

	case PTRACE_SETVRREGS:
		/* Set the child altivec register state. */
		/* this is to clear the MSR_VEC bit to force a reload
		 * of register state from memory */
		if (child->thread.regs->msr & MSR_VEC)
			giveup_altivec(child);
		ret = set_vrregs32(child,(unsigned long)data);
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
