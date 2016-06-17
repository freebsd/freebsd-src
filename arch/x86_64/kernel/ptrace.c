/* ptrace.c */
/* By Ross Biro 1/23/92 */
/*
 * Pentium III FXSR, SSE support
 *	Gareth Hughes <gareth@valinux.com>, May 2000
 * 
 * x86-64 port 2000-2002 Andi Kleen
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
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/processor.h>
#include <asm/i387.h>
#include <asm/debugreg.h>

/*
 * does not yet catch signals sent when the child dies.
 * in exit.c or in signal.c.
 */

/* determines which flags the user has access to. */
/* 1 = access 0 = no access */
#define FLAG_MASK 0x44dd5UL

/* set's the trap flag. */
#define TRAP_FLAG 0x100UL

/*
 * eflags and offset of eflags on child stack..
 */
#define EFLAGS offsetof(struct pt_regs, eflags)
#define EFL_OFFSET ((int)(EFLAGS-sizeof(struct pt_regs)))

/*
 * this routine will get a word off of the processes privileged stack. 
 * the offset is how far from the base addr as stored in the TSS.  
 * this routine assumes that all the privileged stacks are in our
 * data space.
 */   
static inline unsigned long get_stack_long(struct task_struct *task, int offset)
{
	unsigned char *stack;

	stack = (unsigned char *)task->thread.rsp0;
	stack += offset;
	return (*((unsigned long *)stack));
}

/*
 * this routine will put a word on the processes privileged stack. 
 * the offset is how far from the base addr as stored in the TSS.  
 * this routine assumes that all the privileged stacks are in our
 * data space.
 */
static inline long put_stack_long(struct task_struct *task, int offset,
	unsigned long data)
{
	unsigned char * stack;

	stack = (unsigned char *) task->thread.rsp0;
	stack += offset;
	*(unsigned long *) stack = data;
	return 0;
}

/*
 * Called by kernel/ptrace.c when detaching..
 *
 * Make sure the single step bit is not set.
 */
void ptrace_disable(struct task_struct *child)
{ 
	long tmp;

	tmp = get_stack_long(child, EFL_OFFSET) & ~TRAP_FLAG;
	put_stack_long(child, EFL_OFFSET, tmp);
}

static int putreg(struct task_struct *child,
	unsigned long regno, unsigned long value)
{
	unsigned long tmp; 
	if (child->thread.flags & THREAD_IA32)
		value &= 0xffffffff;
	switch (regno) {
		case offsetof(struct user_regs_struct,fs):
			if (value && (value & 3) != 3)
				return -EIO;
			child->thread.fsindex = value & 0xffff; 
			return 0; 
		case offsetof(struct user_regs_struct,gs):
			if (value && (value & 3) != 3)
				return -EIO;
			child->thread.gsindex = value & 0xffff;
			return 0;
		case offsetof(struct user_regs_struct,ds):
			if (value && (value & 3) != 3)
				return -EIO;
			child->thread.ds = value & 0xffff;
			return 0;
		case offsetof(struct user_regs_struct,es): 
			if (value && (value & 3) != 3)
				return -EIO;
			child->thread.es = value & 0xffff;
			return 0;
		case offsetof(struct user_regs_struct,fs_base):
			if (!((value >> 48) == 0 || (value >> 48) == 0xffff))
				return -EIO; 
			child->thread.fs = value;
			return 0;
		case offsetof(struct user_regs_struct,gs_base):
			if (!((value >> 48) == 0 || (value >> 48) == 0xffff))
				return -EIO; 
			child->thread.gs = value;
			return 0;
		case offsetof(struct user_regs_struct, eflags):
			value &= FLAG_MASK;
			tmp = get_stack_long(child, EFL_OFFSET); 
			tmp &= ~FLAG_MASK; 
			value |= tmp;
			break;
		case offsetof(struct user_regs_struct,cs): 
			if ((value & 3) != 3)
				return -EIO;
			value &= 0xffff;
			break;
		case offsetof(struct user_regs_struct,ss):
			if ((value & 3) != 3)
				return -EIO;
			value &= 0xffff;
            break;
	}      
	put_stack_long(child, regno - sizeof(struct pt_regs), value);
	return 0;
}

static unsigned long getreg(struct task_struct *child, unsigned long regno)
{
	unsigned long val; 
	switch (regno) {
		case offsetof(struct user_regs_struct, fs):
			return child->thread.fsindex;
		case offsetof(struct user_regs_struct, gs):
			return child->thread.gsindex;
		case offsetof(struct user_regs_struct, ds):
			return child->thread.ds;
		case offsetof(struct user_regs_struct, es):
			return child->thread.es; 
		case offsetof(struct user_regs_struct, fs_base):
			return child->thread.fs;
		case offsetof(struct user_regs_struct, gs_base):
			return child->thread.gs;
		default:
			regno = regno - sizeof(struct pt_regs);
			val = get_stack_long(child, regno);
			if (child->thread.flags & THREAD_IA32) 
				val &= 0xffffffff;
			return val;
	}

}

asmlinkage long sys_ptrace(long request, long pid, long addr, long data)
{
	struct task_struct *child;
	struct user * dummy = NULL;
	long i, ret;

	/* This lock_kernel fixes a subtle race with suid exec */
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
		unsigned long tmp;

		ret = -EIO;
		if ((addr & 7) || addr < 0 || 
		    addr > sizeof(struct user) - 7)
			break;

		tmp = 0;  /* Default return condition */
		if(addr < sizeof(struct user_regs_struct))
			tmp = getreg(child, addr);
		if(addr >= (long) &dummy->u_debugreg[0] &&
		   addr <= (long) &dummy->u_debugreg[7]){
			addr -= (long) &dummy->u_debugreg[0];
			addr = addr >> 3;
			tmp = child->thread.debugreg[addr];
		}
		ret = put_user(tmp,(unsigned long *) data);
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
		if ((addr & 7) || addr < 0 || 
		    addr > sizeof(struct user) - 7)
			break;

		if (addr < sizeof(struct user_regs_struct)) {
			ret = putreg(child, addr, data);
			break;
		}
		/* We need to be very careful here.  We implicitly
		   want to modify a portion of the task_struct, and we
		   have to be selective about what portions we allow someone
		   to modify. */

		  ret = -EIO;
		  if(addr >= (long) &dummy->u_debugreg[0] &&
		     addr <= (long) &dummy->u_debugreg[7]){

			  if(addr == (long) &dummy->u_debugreg[4]) break;
			  if(addr == (long) &dummy->u_debugreg[5]) break;
			  if(addr < (long) &dummy->u_debugreg[4] &&
			     ((unsigned long) data) >= TASK_SIZE-3) break;
			  
			  if (addr == (long) &dummy->u_debugreg[6]) { 
				  if (data >> 32) 
					  goto out_tsk;
			  }

			  if(addr == (long) &dummy->u_debugreg[7]) {
				  data &= ~DR_CONTROL_RESERVED;
				  for(i=0; i<4; i++)
					  if ((0x5454 >> ((data >> (16 + 4*i)) & 0xf)) & 1)
						  goto out_tsk;
			  }

			  addr -= (long) &dummy->u_debugreg;
			  addr = addr >> 3;
			  child->thread.debugreg[addr] = data;
			  ret = 0;
		  }
		  break;

	case PTRACE_SYSCALL: /* continue and stop at next (return from) syscall */
	case PTRACE_CONT: { /* restart after signal. */
		long tmp;

		ret = -EIO;
		if ((unsigned long) data > _NSIG)
			break;
		if (request == PTRACE_SYSCALL)
			child->ptrace |= PT_TRACESYS;
		else
			child->ptrace &= ~PT_TRACESYS;
		child->exit_code = data;
	/* make sure the single step bit is not set. */
		tmp = get_stack_long(child, EFL_OFFSET);
		tmp &= ~TRAP_FLAG;
		put_stack_long(child, EFL_OFFSET,tmp);
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
		long tmp;

		ret = 0;
		if (child->state == TASK_ZOMBIE)	/* already dead */
			break;
		child->exit_code = SIGKILL;
		/* make sure the single step bit is not set. */
		tmp = get_stack_long(child, EFL_OFFSET) & ~TRAP_FLAG;
		put_stack_long(child, EFL_OFFSET, tmp);
		wake_up_process(child);
		break;
	}

	case PTRACE_SINGLESTEP: {  /* set the trap flag. */
		long tmp;

		ret = -EIO;
		if ((unsigned long) data > _NSIG)
			break;
		child->ptrace &= ~PT_TRACESYS;
		if ((child->ptrace & PT_DTRACE) == 0) {
			/* Spurious delayed TF traps may occur */
			child->ptrace |= PT_DTRACE;
		}
		tmp = get_stack_long(child, EFL_OFFSET) | TRAP_FLAG;
		put_stack_long(child, EFL_OFFSET, tmp);
		child->exit_code = data;
		/* give it a chance to run. */
		wake_up_process(child);
		ret = 0;
		break;
	}

	case PTRACE_DETACH:
		/* detach a process that was attached. */
		ret = ptrace_detach(child, data);
		break;

	case PTRACE_GETREGS: { /* Get all gp regs from the child. */
	  	if (!access_ok(VERIFY_WRITE, (unsigned *)data, FRAME_SIZE)) {
			ret = -EIO;
			break;
		}
		for ( i = 0; i < sizeof(struct user_regs_struct); i += sizeof(long) ) {
			__put_user(getreg(child, i),(unsigned long *) data);
			data += sizeof(long);
		}
		ret = 0;
		break;
	}

	case PTRACE_SETREGS: { /* Set all gp regs in the child. */
		unsigned long tmp;
	  	if (!access_ok(VERIFY_READ, (unsigned *)data, FRAME_SIZE)) {
			ret = -EIO;
			break;
		}
		for ( i = 0; i < sizeof(struct user_regs_struct); i += sizeof(long) ) {
			__get_user(tmp, (unsigned long *) data);
			putreg(child, i, tmp);
			data += sizeof(long);
		}
		ret = 0;
		break;
	}

	case PTRACE_GETFPREGS: { /* Get the child extended FPU state. */
		if (!access_ok(VERIFY_WRITE, (unsigned *)data,
			       sizeof(struct user_i387_struct))) {
			ret = -EIO;
			break;
		}
		ret = get_fpregs((struct user_i387_struct *)data, child);
		break;
	}

	case PTRACE_SETFPREGS: { /* Set the child extended FPU state. */
		if (!access_ok(VERIFY_READ, (unsigned *)data,
			       sizeof(struct user_i387_struct))) {
			ret = -EIO;
			break;
		}
		unlazy_fpu(child);
		ret = set_fpregs(child, (struct user_i387_struct *)data);
		if (!ret) 
			child->used_math = 1;
		break;
	}

	case PTRACE_SETOPTIONS: {
		if (data & PTRACE_O_TRACESYSGOOD)
			child->ptrace |= PT_TRACESYSGOOD;
		else
			child->ptrace &= ~PT_TRACESYSGOOD;
		ret = 0;
		break;
	}

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

asmlinkage void syscall_trace(struct pt_regs *regs)
{
	if ((current->ptrace & (PT_PTRACED|PT_TRACESYS)) !=
			(PT_PTRACED|PT_TRACESYS))
		return;
	
	current->exit_code = SIGTRAP | ((current->ptrace & PT_TRACESYSGOOD)
					? 0x80 : 0);
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
