/* $Id: ptrace.c,v 1.1.1.1.2.3 2003/10/23 22:11:08 yoshii Exp $
 *
 * linux/arch/sh/kernel/ptrace.c
 *
 * Original x86 implementation:
 *	By Ross Biro 1/23/92
 *	edited by Linus Torvalds
 *
 * SuperH version:   Copyright (C) 1999, 2000  Kaz Kojima & Niibe Yutaka
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/user.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/processor.h>
#include <asm/mmu_context.h>

/*
 * does not yet catch signals sent when the child dies.
 * in exit.c or in signal.c.
 */

/*
 * This routine will get a word off of the process kernel stack.
 */
static inline int get_stack_long(struct task_struct *task, int offset)
{
	unsigned char *stack;

	stack = (unsigned char *)task + THREAD_SIZE - sizeof(struct pt_regs);
	stack += offset;
	return (*((int *)stack));
}

/*
 * This routine will put a word on the process kernel stack.
 */
static inline int put_stack_long(struct task_struct *task, int offset,
				 unsigned long data)
{
	unsigned char *stack;

	stack = (unsigned char *)task + THREAD_SIZE - sizeof(struct pt_regs);
	stack += offset;
	*(unsigned long *) stack = data;
	return 0;
}

static void
compute_next_pc(struct pt_regs *regs, unsigned short inst,
		unsigned long *pc1, unsigned long *pc2)
{
	int nib[4]
		= { (inst >> 12) & 0xf,
		    (inst >> 8) & 0xf,
		    (inst >> 4) & 0xf,
		    inst & 0xf};

	/* bra & bsr */
	if (nib[0] == 0xa || nib[0] == 0xb) {
		*pc1 = regs->pc + 4 + ((short) ((inst & 0xfff) << 4) >> 3);
		if(*pc1 == regs->pc + 2)
			*pc1 = regs->pc + 4;
		*pc2 = (unsigned long) -1;
		return;
	}

	/* bt & bf */
	if (nib[0] == 0x8 && (nib[1] == 0x9 || nib[1] == 0xb)) {
		*pc1 = regs->pc + 4 + ((char) (inst & 0xff) << 1);
		*pc2 = regs->pc + 2;
		return;
	}

	/* bt/s & bf/s */
	if (nib[0] == 0x8 && (nib[1] == 0xd || nib[1] == 0xf)) {
		*pc1 = regs->pc + 4 + ((char) (inst & 0xff) << 1);
		if(*pc1 == regs->pc + 2) {
			*pc1 = regs->pc + 4;
			*pc2 = (unsigned long) -1;
		}
		else
			*pc2 = regs->pc + 4;
		return;
	}

	/* jmp & jsr */
	if (nib[0] == 0x4 && nib[3] == 0xb
	    && (nib[2] == 0x0 || nib[2] == 0x2)) {
		*pc1 = regs->regs[nib[1]];
		if(*pc1 == regs->pc + 2)
			*pc1 = regs->pc + 4;
		*pc2 = (unsigned long) -1;
		return;
	}

	/* braf & bsrf */
	if (nib[0] == 0x0 && nib[3] == 0x3
	    && (nib[2] == 0x0 || nib[2] == 0x2)) {
		*pc1 = regs->pc + 4 + regs->regs[nib[1]];
		if(*pc1 == regs->pc + 2)
			*pc1 = regs->pc + 4;
		*pc2 = (unsigned long) -1;
		return;
	}

	if (inst == 0x000b) {
		*pc1 = regs->pr;
		if(*pc1 == regs->pc + 2)
			*pc1 = regs->pc + 4;
		*pc2 = (unsigned long) -1;
		return;
	}

	*pc1 = regs->pc + 2;
	*pc2 = (unsigned long) -1;
	return;
}

/*
 * Called by kernel/ptrace.c when detaching..
 *
 * Make sure single step bits etc are not set.
 */
void ptrace_disable(struct task_struct *child)
{
	/* nothing to do.. */
}

asmlinkage int sys_ptrace(long request, long pid, long addr, long data)
{
	struct task_struct *child, *tsk = current;
	struct user * dummy = NULL;
	int ret;

	lock_kernel();
	ret = -EPERM;
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
	ret = -ESRCH;
	if (!(child->ptrace & PT_PTRACED))
		goto out_tsk;
	if (child->state != TASK_STOPPED) {
		if (request != PTRACE_KILL)
			goto out_tsk;
	}
	if (child->p_pptr != tsk)
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
		unsigned long tmp;

		ret = -EIO;
		if ((addr & 3) || addr < 0 || 
		    addr > sizeof(struct user) - 3)
			break;

		if (addr < sizeof(struct pt_regs))
			tmp = get_stack_long(child, addr);
		else if (addr >= (long) &dummy->fpu &&
			 addr < (long) &dummy->u_fpvalid) {
			if (!child->used_math) {
				if (addr == (long)&dummy->fpu.fpscr)
					tmp = FPSCR_INIT;
				else
					tmp = 0;
			} else
				tmp = ((long *)&child->thread.fpu)
					[(addr - (long)&dummy->fpu) >> 2];
		} else if (addr == (long) &dummy->u_fpvalid)
			tmp = child->used_math;
		else
			tmp = 0;
		ret = put_user(tmp, (unsigned long *)data);
		break;
	}

	/* when I and D space are separate, this will have to be fixed. */
	case PTRACE_POKETEXT: /* write the word at location addr. */
	case PTRACE_POKEDATA:
		ret = 0;
		if (access_process_vm(child, addr, &data, sizeof(data), 1) == sizeof(data))
			break;
		ret = -EIO;
		break;

	case PTRACE_POKEUSR: /* write the word at location addr in the USER area */
		ret = -EIO;
		if ((addr & 3) || addr < 0 || 
		    addr > sizeof(struct user) - 3)
			break;

		if (addr < sizeof(struct pt_regs))
			ret = put_stack_long(child, addr, data);
		else if (addr >= (long) &dummy->fpu &&
			 addr < (long) &dummy->u_fpvalid) {
			child->used_math = 1;
			((long *)&child->thread.fpu)
				[(addr - (long)&dummy->fpu) >> 2] = data;
			ret = 0;
		} else if (addr == (long) &dummy->u_fpvalid) {
			child->used_math = data?1:0;
			ret = 0;
		}
		break;

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
		wake_up_process(child);
		break;
	}

	case PTRACE_SINGLESTEP: {  /* set the trap flag. */
		long tmp, pc;
		struct pt_regs *dummy = NULL;
		struct pt_regs *regs;
		unsigned long nextpc1, nextpc2;
		unsigned short insn;

		ret = -EIO;
		if ((unsigned long) data > _NSIG)
			break;
		child->ptrace &= ~PT_TRACESYS;
		if ((child->ptrace & PT_DTRACE) == 0) {
			/* Spurious delayed TF traps may occur */
			child->ptrace |= PT_DTRACE;
		}

		/* Compute next pc.  */
		pc = get_stack_long(child, (long)&dummy->pc);
		regs = (struct pt_regs *)((unsigned long)child + THREAD_SIZE - sizeof(struct pt_regs));
		if (access_process_vm(child, pc&~3, &tmp, sizeof(tmp), 0) != sizeof(tmp))
			break;
 
#ifdef  __LITTLE_ENDIAN__
		if (pc & 3)
			insn = tmp >> 16;
		else
			insn = tmp & 0xffff;
#else
		if (pc & 3)
			insn = tmp & 0xffff;
		else
			insn = tmp >> 16;
#endif
		compute_next_pc (regs, insn, &nextpc1, &nextpc2);

		if (nextpc1 & 0x80000000)
			break;
		if (nextpc2 != (unsigned long) -1 && (nextpc2 & 0x80000000))
			break;

		/* Next scheduling will set up UBC */
		if (child->thread.ubc_pc1 == 0)
			ubc_usercnt += 1;
		child->thread.ubc_pc1 = nextpc1;
		child->thread.ubc_pc2 = nextpc2;

		child->exit_code = data;
		/* give it a chance to run. */
		wake_up_process(child);
		ret = 0;
		break;
	}

	case PTRACE_DETACH: /* detach a process that was attached. */
		ret = ptrace_detach(child, data);
		break;

	case PTRACE_SETOPTIONS: {
		if (data & PTRACE_O_TRACESYSGOOD)
			child->ptrace |= PT_TRACESYSGOOD;
		else
			child->ptrace &= ~PT_TRACESYSGOOD;
		ret = 0;
		break;
	}

#ifdef CONFIG_SH_DSP
	case PTRACE_GETDSPREGS: {
		unsigned long dp;

		ret = -EIO;
		dp = ((unsigned long) child) + THREAD_SIZE -
			 sizeof(struct pt_dspregs);
		if (*((int *) (dp - 4)) == SR_DSP) {
			copy_to_user(addr, (void *) dp,
				sizeof(struct pt_dspregs));
			ret = 0;
		}
		break;
	}

	case PTRACE_SETDSPREGS: {
		unsigned long dp;
		int i;

		ret = -EIO;
		dp = ((unsigned long) child) + THREAD_SIZE -
			 sizeof(struct pt_dspregs);
		if (*((int *) (dp - 4)) == SR_DSP) {
			copy_from_user((void *) dp, addr,
				sizeof(struct pt_dspregs));
			ret = 0;
		}
		break;
	}
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

asmlinkage void syscall_trace(void)
{
	struct task_struct *tsk = current;

	if ((tsk->ptrace & (PT_PTRACED|PT_TRACESYS))
	    != (PT_PTRACED|PT_TRACESYS))
		return;
	/* the 0x80 provides a way for the tracing parent to distinguish
	   between a syscall stop and SIGTRAP delivery */
	tsk->exit_code = SIGTRAP | ((current->ptrace & PT_TRACESYSGOOD)
				    ? 0x80 : 0);
	tsk->state = TASK_STOPPED;
	notify_parent(tsk, SIGCHLD);
	schedule();
	/*
	 * this isn't the same as continuing with a signal, but it will do
	 * for normal use.  strace only continues with a signal if the
	 * stopping signal is not SIGTRAP.  -brl
	 */
	if (tsk->exit_code) {
		send_sig(tsk->exit_code, tsk, 1);
		tsk->exit_code = 0;
	}
}
