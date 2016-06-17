/*
 *  arch/s390/kernel/ptrace.c
 *
 *  S390 version
 *    Copyright (C) 1999,2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Denis Joseph Barrow (djbarrow@de.ibm.com,barrow_dj@yahoo.com),
 *
 *  Based on PowerPC version 
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Derived from "arch/m68k/kernel/ptrace.c"
 *  Copyright (C) 1994 by Hamish Macdonald
 *  Taken from linux/kernel/ptrace.c and modified for M680x0.
 *  linux/kernel/ptrace.c is by Ross Biro 1/23/92, edited by Linus Torvalds
 *
 * Modified by Cort Dougan (cort@cs.nmt.edu) 
 *
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file README.legal in the main directory of
 * this archive for more details.
 */

#include <stddef.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/user.h>

#include <asm/segment.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/system.h>
#include <asm/uaccess.h>


void FixPerRegisters(struct task_struct *task)
{
	struct pt_regs *regs = __KSTK_PTREGS(task);
	per_struct *per_info=
			(per_struct *)&task->thread.per_info;

	per_info->control_regs.bits.em_instruction_fetch=
	per_info->single_step|per_info->instruction_fetch;
	
	if(per_info->single_step)
	{
		per_info->control_regs.bits.starting_addr=0;
		per_info->control_regs.bits.ending_addr=0x7fffffffUL;
	}
	else
	{
		per_info->control_regs.bits.starting_addr=
		per_info->starting_addr;
		per_info->control_regs.bits.ending_addr=
		per_info->ending_addr;
	}
	/* if any of the control reg tracing bits are on 
	   we switch on per in the psw */
	if(per_info->control_regs.words.cr[0]&PER_EM_MASK)
		regs->psw.mask |=PSW_PER_MASK;
	else
		regs->psw.mask &= ~PSW_PER_MASK;
	if (per_info->control_regs.bits.em_storage_alteration)
	{
		per_info->control_regs.bits.storage_alt_space_ctl=1;
		//((pgd_t *)__pa(task->mm->pgd))->pgd |= USER_STD_MASK;
	}
	else
	{
		per_info->control_regs.bits.storage_alt_space_ctl=0;
		//((pgd_t *)__pa(task->mm->pgd))->pgd &= ~USER_STD_MASK;
	}
}

void set_single_step(struct task_struct *task)
{
	per_struct *per_info=
			(per_struct *)&task->thread.per_info;	
	
	per_info->single_step=1;  /* Single step */
	FixPerRegisters(task);
}

void clear_single_step(struct task_struct *task)
{
	per_struct *per_info=
			(per_struct *)&task->thread.per_info;

	per_info->single_step=0;
	FixPerRegisters(task);
}

int ptrace_usercopy(addr_t realuseraddr,addr_t copyaddr,int len,int tofromuser,int writeuser,u32 mask)
{
	u32  tempuser;
	int  retval=0;
	
	if(writeuser&&realuseraddr==(addr_t)NULL)
		return(0);
	if(mask!=0xffffffff)
	{
		tempuser=*((u32 *)realuseraddr);
		if(!writeuser)
		{
			tempuser&=mask;
			realuseraddr=(addr_t)&tempuser;
		}
	}
	if(tofromuser)
	{
		if(writeuser)
		{
			retval=copy_from_user((void *)realuseraddr,(void *)copyaddr,len);
		}
		else
		{
			if(realuseraddr==(addr_t)NULL)
				retval=clear_user((void *)copyaddr,len);
			else
				retval=copy_to_user((void *)copyaddr,(void *)realuseraddr,len);
		}      
		retval = retval ? -EFAULT : 0;
	}
	else
	{
		if(writeuser)
			memcpy((void *)realuseraddr,(void *)copyaddr,len);
		else
			memcpy((void *)copyaddr,(void *)realuseraddr,len);
	}
	if(mask!=0xffffffff&&writeuser)
			(*((u32 *)realuseraddr))=(((*((u32 *)realuseraddr))&mask)|(tempuser&~mask));
	return(retval);
}

int copy_user(struct task_struct *task,saddr_t useraddr,addr_t copyaddr,int len,int tofromuser,int writingtouser)
{
	int copylen=0,copymax;
	addr_t  realuseraddr;
	saddr_t enduseraddr=useraddr+len;
	
	u32 mask;

	if (useraddr < 0 || enduseraddr > sizeof(struct user)||
	   (useraddr < PT_ENDREGS && (useraddr&3))||
	   (enduseraddr < PT_ENDREGS && (enduseraddr&3)))
		return (-EIO);
	while(len>0)
	{
		mask=0xffffffff;
		if(useraddr<PT_FPC)
		{
			realuseraddr=((addr_t) __KSTK_PTREGS(task)) + useraddr;
			if(useraddr<PT_PSWMASK)
			{
				copymax=PT_PSWMASK;
			}
			else if(useraddr<(PT_PSWMASK+4))
			{
				copymax=(PT_PSWMASK+4);
				if(writingtouser)
					mask=PSW_MASK_DEBUGCHANGE;
			}
			else if(useraddr<(PT_PSWADDR+4))
			{
				copymax=PT_PSWADDR+4;
				mask=PSW_ADDR_DEBUGCHANGE;
			}
			else
				copymax=PT_FPC;
			
		}
		else if(useraddr<(PT_FPR15_LO+4))
		{
			copymax=(PT_FPR15_LO+4);
			realuseraddr=(addr_t)&(((u8 *)&task->thread.fp_regs)[useraddr-PT_FPC]);
		}
		else if(useraddr<sizeof(struct user_regs_struct))
		{
			copymax=sizeof(struct user_regs_struct);
			realuseraddr=(addr_t)&(((u8 *)&task->thread.per_info)[useraddr-PT_CR_9]);
		}
		else 
		{
			copymax=sizeof(struct user);
			realuseraddr=(addr_t)NULL;
		}
		copylen=copymax-useraddr;
		copylen=(copylen>len ? len:copylen);
		if(ptrace_usercopy(realuseraddr,copyaddr,copylen,tofromuser,writingtouser,mask))
			return (-EIO);
		copyaddr+=copylen;
		len-=copylen;
		useraddr+=copylen;
	}
	FixPerRegisters(task);
	return(0);
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

asmlinkage int sys_ptrace(long request, long pid, long addr, long data)
{
	struct task_struct *child;
	int ret = -EPERM;
	unsigned long tmp;
	int copied;
	ptrace_area   parea; 

	lock_kernel();
	if (request == PTRACE_TRACEME) 
	{
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
	if (request == PTRACE_ATTACH) 
	{
		ret = ptrace_attach(child);
		goto out_tsk;
	}
	ret = -ESRCH;
	// printk("child=%lX child->flags=%lX",child,child->flags);
	/* I added child!=current line so we can get the */
	/* ieee_instruction_pointer from the user structure DJB */
	if(child!=current)
	{
		if (!(child->ptrace & PT_PTRACED))
			goto out_tsk;
		if (child->state != TASK_STOPPED) 
		{
			if (request != PTRACE_KILL)
				goto out_tsk;
		}
		if (child->p_pptr != current)
			goto out_tsk;
	}
	switch (request) 
	{
		/* If I and D space are separate, these will need to be fixed. */
	case PTRACE_PEEKTEXT: /* read word at location addr. */ 
	case PTRACE_PEEKDATA: 
		copied = access_process_vm(child,ADDR_BITS_REMOVE(addr), &tmp, sizeof(tmp), 0);
		ret = -EIO;
		if (copied != sizeof(tmp))
			break;
		ret = put_user(tmp,(unsigned long *) data);
		break;

		/* read the word at location addr in the USER area. */
	case PTRACE_PEEKUSR:
		ret=copy_user(child,addr,data,sizeof(unsigned long),1,0);
		break;

		/* If I and D space are separate, this will have to be fixed. */
	case PTRACE_POKETEXT: /* write the word at location addr. */
	case PTRACE_POKEDATA:
		ret = 0;
		if (access_process_vm(child,ADDR_BITS_REMOVE(addr), &data, sizeof(data), 1) == sizeof(data))
			break;
		ret = -EIO;
		break;

	case PTRACE_POKEUSR: /* write the word at location addr in the USER area */
		ret=copy_user(child,addr,(addr_t)&data,sizeof(unsigned long),0,1);
		break;

	case PTRACE_SYSCALL: 	/* continue and stop at next (return from) syscall */
	case PTRACE_CONT: 	 /* restart after signal. */
		ret = -EIO;
		if ((unsigned long) data >= _NSIG)
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

/*
 * make the child exit.  Best I can do is send it a sigkill. 
 * perhaps it should be put in the status that it wants to 
 * exit.
 */
	case PTRACE_KILL:
		ret = 0;
		if (child->state == TASK_ZOMBIE) /* already dead */
			break;
		child->exit_code = SIGKILL;
		clear_single_step(child);
		wake_up_process(child);
		/* make sure the single step bit is not set. */
		break;

	case PTRACE_SINGLESTEP:  /* set the trap flag. */
		ret = -EIO;
		if ((unsigned long) data >= _NSIG)
			break;
		child->ptrace &= ~PT_TRACESYS;
		child->exit_code = data;
		set_single_step(child);
		/* give it a chance to run. */
		wake_up_process(child);
		ret = 0;
		break;

	case PTRACE_DETACH:  /* detach a process that was attached. */
		ret = ptrace_detach(child, data);
		break;
	case PTRACE_PEEKUSR_AREA:
	case PTRACE_POKEUSR_AREA:
		if(copy_from_user(&parea,(void *)addr,sizeof(parea))==0)  
			ret=copy_user(child,parea.kernel_addr,parea.process_addr,
				      parea.len,1,(request==PTRACE_POKEUSR_AREA));
		else ret = -EFAULT;
		break;
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
	lock_kernel();
	if ((current->ptrace & (PT_PTRACED|PT_TRACESYS))
	    != (PT_PTRACED|PT_TRACESYS))
		goto out;
	current->exit_code = SIGTRAP;
	set_current_state(TASK_STOPPED);
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
 out:
	unlock_kernel();
}
