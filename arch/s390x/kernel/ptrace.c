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
#include <linux/config.h>
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
#ifdef CONFIG_S390_SUPPORT
#include "linux32.h"
#else
#define parent_31bit 0
#endif


void FixPerRegisters(struct task_struct *task)
{
	struct pt_regs *regs = __KSTK_PTREGS(task);
	per_struct *per_info=
			(per_struct *)&task->thread.per_info;

	per_info->control_regs.bits.em_instruction_fetch =
	        per_info->single_step | per_info->instruction_fetch;
	
	if (per_info->single_step) {
		per_info->control_regs.bits.starting_addr=0;
#ifdef CONFIG_S390_SUPPORT
		if (current->thread.flags & S390_FLAG_31BIT) {
			per_info->control_regs.bits.ending_addr=0x7fffffffUL;
	        }
		else 
#endif      
		{
		per_info->control_regs.bits.ending_addr=-1L;
		}
	} else {
		per_info->control_regs.bits.starting_addr=
		        per_info->starting_addr;
		per_info->control_regs.bits.ending_addr=
		        per_info->ending_addr;
	}
	/* if any of the control reg tracing bits are on 
	   we switch on per in the psw */
	if (per_info->control_regs.words.cr[0] & PER_EM_MASK)
		regs->psw.mask |= PSW_PER_MASK;
	else
		regs->psw.mask &= ~PSW_PER_MASK;
	if (per_info->control_regs.bits.storage_alt_space_ctl)
		task->thread.user_seg |= USER_STD_MASK;
	else
		task->thread.user_seg &= ~USER_STD_MASK;
}

void set_single_step(struct task_struct *task)
{
	per_struct *per_info= (per_struct *) &task->thread.per_info;	
	
	per_info->single_step = 1;  /* Single step */
	FixPerRegisters (task);
}

void clear_single_step(struct task_struct *task)
{
	per_struct *per_info= (per_struct *) &task->thread.per_info;

	per_info->single_step = 0;
	FixPerRegisters (task);
}

int ptrace_usercopy(addr_t realuseraddr, addr_t copyaddr, int len,
                    int tofromuser, int writeuser, unsigned long mask)
{
        unsigned long *realuserptr, *copyptr;
	unsigned long tempuser;
	int retval;

        retval = 0;
        realuserptr = (unsigned long *) realuseraddr;
        copyptr = (unsigned long *) copyaddr;

	if (writeuser && realuserptr == NULL)
		return 0;

	if (mask != -1L) {
		tempuser = *realuserptr;
		if (!writeuser) {
			tempuser &= mask;
			realuserptr = &tempuser;
		}
	}
	if (tofromuser) {
		if (writeuser) {
			retval = copy_from_user(realuserptr, copyptr, len);
		} else {
			if (realuserptr == NULL)
				retval = clear_user(copyptr, len);
			else
				retval = copy_to_user(copyptr,realuserptr,len);
		}      
		retval = retval ? -EFAULT : 0;
	} else {
		if (writeuser)
			memcpy(realuserptr, copyptr, len);
		else
			memcpy(copyptr, realuserptr, len);
	}
	if (mask != -1L && writeuser)
                *realuserptr = (*realuserptr & mask) | (tempuser & ~mask);
	return retval;
}

#ifdef CONFIG_S390_SUPPORT

typedef struct
{
	__u32 cr[3];
} per_cr_words32  __attribute__((packed));

typedef struct
{
	__u16          perc_atmid;          /* 0x096 */
	__u32          address;             /* 0x098 */
	__u8           access_id;           /* 0x0a1 */
} per_lowcore_words32  __attribute__((packed));

typedef struct
{
	union {
		per_cr_words32   words;
	} control_regs  __attribute__((packed));
	/*
	 * Use these flags instead of setting em_instruction_fetch
	 * directly they are used so that single stepping can be
	 * switched on & off while not affecting other tracing
	 */
	unsigned  single_step       : 1;
	unsigned  instruction_fetch : 1;
	unsigned                    : 30;
	/*
	 * These addresses are copied into cr10 & cr11 if single
	 * stepping is switched off
	 */
	__u32     starting_addr;
	__u32     ending_addr;
	union {
		per_lowcore_words32 words;
	} lowcore; 
} per_struct32 __attribute__((packed));

struct user_regs_struct32
{
	_psw_t32 psw;
	u32 gprs[NUM_GPRS];
	u32 acrs[NUM_ACRS];
	u32 orig_gpr2;
	s390_fp_regs fp_regs;
	/*
	 * These per registers are in here so that gdb can modify them
	 * itself as there is no "official" ptrace interface for hardware
	 * watchpoints. This is the way intel does it.
	 */
	per_struct32 per_info;
	u32  ieee_instruction_pointer; 
	/* Used to give failing instruction back to user for ieee exceptions */
};

struct user32 {
                                  /* We start with the registers, to mimic the way that "memory" is returned
                                   from the ptrace(3,...) function.  */
  struct user_regs_struct32 regs; /* Where the registers are actually stored */
                                  /* The rest of this junk is to help gdb figure out what goes where */
  u32 u_tsize;	                  /* Text segment size (pages). */
  u32 u_dsize;	                  /* Data segment size (pages). */
  u32 u_ssize;	                  /* Stack segment size (pages). */
  u32 start_code;                 /* Starting virtual address of text. */
  u32 start_stack;	          /* Starting virtual address of stack area.
				   This is actually the bottom of the stack,
				   the top of the stack is always found in the
				   esp register.  */
  s32 signal;     		  /* Signal that caused the core dump. */
  u32 u_ar0;                      /* Used by gdb to help find the values for */
				  /* the registers. */
  u32 magic;		          /* To uniquely identify a core file */
  char u_comm[32];		  /* User command that was responsible */
};


#define PT32_PSWMASK  0x0
#define PT32_PSWADDR  0x04
#define PT32_GPR0     0x08
#define PT32_GPR15    0x44
#define PT32_ACR0     0x48
#define PT32_ACR15    0x84
#define PT32_ORIGGPR2 0x88
#define PT32_FPC      0x90
#define PT32_FPR0_HI  0x98
#define PT32_FPR15_LO 0x114
#define PT32_CR_9     0x118
#define PT32_CR_11    0x120
#define PT32_IEEE_IP  0x13C
#define PT32_LASTOFF  PT32_IEEE_IP
#define PT32_ENDREGS  0x140-1
#define U32OFFSETOF(member) offsetof(struct user32,regs.member)
#define U64OFFSETOF(member) offsetof(struct user,regs.member)
#define U6432DIFF(member) (U64OFFSETOF(member) - U32OFFSETOF(member))
#define PT_SINGLE_STEP   (PT_CR_11+8)
#define PT32_SINGLE_STEP (PT32_CR_11+4)

#endif /* CONFIG_S390_SUPPORT */

int copy_user(struct task_struct *task,saddr_t useraddr, addr_t copyaddr,
              int len, int tofromuser, int writingtouser)
{
	int copylen=0,copymax;
	addr_t  realuseraddr;
	saddr_t enduseraddr;
	unsigned long mask;
#ifdef CONFIG_S390_SUPPORT
	int     parent_31bit=current->thread.flags & S390_FLAG_31BIT;
	int     skip;
#endif
	enduseraddr=useraddr+len;
	if ((useraddr<0||useraddr&3||enduseraddr&3)||
#ifdef CONFIG_S390_SUPPORT
	    (parent_31bit && enduseraddr > sizeof(struct user32)) ||
#endif
	    enduseraddr > sizeof(struct user))
		return (-EIO);

#ifdef CONFIG_S390_SUPPORT
	if(parent_31bit)
	{
		if(useraddr != PT32_PSWMASK)
		{
			if (useraddr == PT32_PSWADDR)
				useraddr = PT_PSWADDR+4;
			else if(useraddr <= PT32_GPR15)
				useraddr = ((useraddr-PT32_GPR0)*2) + PT_GPR0+4;
			else if(useraddr <= PT32_ACR15)
				useraddr += PT_ACR0-PT32_ACR0;
			else if(useraddr == PT32_ORIGGPR2)
				useraddr = PT_ORIGGPR2+4;
			else if(useraddr <= PT32_FPR15_LO)
				useraddr += PT_FPR0-PT32_FPR0_HI;
			else if(useraddr <= PT32_CR_11)
				useraddr = ((useraddr-PT32_CR_9)*2) + PT_CR_9+4;
			else if(useraddr ==  PT32_SINGLE_STEP)
				useraddr = PT_SINGLE_STEP; 
			else if(useraddr <= U32OFFSETOF(per_info.ending_addr))	
				useraddr = (((useraddr-U32OFFSETOF(per_info.starting_addr)))*2) + 
					U64OFFSETOF(per_info.starting_addr)+4;
			else if( useraddr == U32OFFSETOF(per_info.lowcore.words.perc_atmid))
				useraddr = U64OFFSETOF(per_info.lowcore.words.perc_atmid);
			else if( useraddr == U32OFFSETOF(per_info.lowcore.words.address))
				useraddr = U64OFFSETOF(per_info.lowcore.words.address)+4;
			else if(useraddr == U32OFFSETOF(per_info.lowcore.words.access_id))
				useraddr = U64OFFSETOF(per_info.lowcore.words.access_id);
			else if(useraddr == PT32_IEEE_IP)
				useraddr = PT_IEEE_IP+4;
		}
	}
#endif /* CONFIG_S390_SUPPORT */

	while(len>0)
	{
#ifdef CONFIG_S390_SUPPORT
		skip=0;
#endif
		mask=PSW_ADDR_MASK;
		if(useraddr<PT_FPC)
		{
			realuseraddr=((addr_t) __KSTK_PTREGS(task)) + useraddr;
			if(useraddr<(PT_PSWMASK+8))
			{
				if(parent_31bit)
				{
					copymax=PT_PSWMASK+4;
#ifdef CONFIG_S390_SUPPORT
					skip=8;
#endif
				}
				else
				{
					copymax=PT_PSWMASK+8;
				}
				if(writingtouser)
					mask=PSW_MASK_DEBUGCHANGE;
			}
			else if(useraddr<(PT_PSWADDR+8))
			{
				copymax=PT_PSWADDR+8;
				mask=PSW_ADDR_DEBUGCHANGE;
#ifdef CONFIG_S390_SUPPORT
				if(parent_31bit)
					skip=4;
#endif

			}
			else
			{
#ifdef CONFIG_S390_SUPPORT
				if(parent_31bit && useraddr <= PT_GPR15+4)
				{
					copymax=useraddr+4;
					if(useraddr<PT_GPR15+4)
						skip=4;
				}
				else
#endif
					copymax=PT_FPC;
			}
		}
		else if(useraddr<(PT_FPR15+sizeof(freg_t)))
		{
			copymax=(PT_FPR15+sizeof(freg_t));
			realuseraddr=(addr_t)&(((u8 *)&task->thread.fp_regs)[useraddr-PT_FPC]);
		}
		else if(useraddr<sizeof(struct user_regs_struct))
		{
#ifdef CONFIG_S390_SUPPORT
			if( parent_31bit && useraddr <= PT_IEEE_IP+4)
			{
				switch(useraddr)
				{
				case PT_CR_11+4:
				case U64OFFSETOF(per_info.ending_addr)+4:
				case U64OFFSETOF(per_info.lowcore.words.address)+4:
					copymax=useraddr+4;
					break;
				case  PT_SINGLE_STEP:
				case  U64OFFSETOF(per_info.lowcore.words.perc_atmid):
					/* We copy 2 bytes in excess for the atmid member this also gets around */
					/* alignment for this member in 32 bit */
					skip=8;
					copymax=useraddr+4;
					break;
				default: 
					copymax=useraddr+4;
					skip=4;
				}
			}
			else
#endif
			{
				copymax=sizeof(struct user_regs_struct);
			}
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
		useraddr+=copylen
#if CONFIG_S390_SUPPORT
			+skip
#endif
			;
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

typedef struct
{
__u32	len;
__u32	kernel_addr;
__u32	process_addr;
} ptrace_area_emu31;


asmlinkage int sys_ptrace(long request, long pid, long addr, long data)
{
	struct task_struct *child;
	int ret = -EPERM;
	int copied;
#ifdef CONFIG_S390_SUPPORT
	int           parent_31bit;
	int           sizeof_parent_long;
	u8            *dataptr;
#else
#define sizeof_parent_long 8
#define dataptr (u8 *)&data
#endif
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
#ifdef CONFIG_S390_SUPPORT
	parent_31bit=(current->thread.flags & S390_FLAG_31BIT);
	sizeof_parent_long=(parent_31bit ? 4:8);
	dataptr=&(((u8 *)&data)[parent_31bit ? 4:0]);
#endif
	switch (request) 
	{
		/* If I and D space are separate, these will need to be fixed. */
	case PTRACE_PEEKTEXT: /* read word at location addr. */ 
	case PTRACE_PEEKDATA: 
	{
		u8 tmp[8];
		copied = access_process_vm(child, addr, tmp, sizeof_parent_long, 0);
		ret = -EIO;
		if (copied != sizeof_parent_long)
			break;
		ret = copy_to_user((void *)data,tmp,sizeof_parent_long);
		ret = ret ? -EFAULT : 0;
		break;
	
	}
		/* read the word at location addr in the USER area. */
	case PTRACE_PEEKUSR:
		ret=copy_user(child,addr,data,sizeof_parent_long,1,0);
		break;

		/* If I and D space are separate, this will have to be fixed. */
	case PTRACE_POKETEXT: /* write the word at location addr. */
	case PTRACE_POKEDATA:
		ret = 0;
		if (access_process_vm(child, addr,dataptr, sizeof_parent_long, 1) == sizeof_parent_long)
			break;
		ret = -EIO;
		break;

	case PTRACE_POKEUSR: /* write the word at location addr in the USER area */
		ret=copy_user(child,addr,(addr_t)dataptr,sizeof_parent_long,0,1);
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
		if(parent_31bit)
		{
			ptrace_area_emu31   parea; 
			if(copy_from_user(&parea,(void *)addr,sizeof(parea))==0)
				ret=copy_user(child,parea.kernel_addr,parea.process_addr,
					      parea.len,1,(request==PTRACE_POKEUSR_AREA));
			else ret = -EFAULT;
		}
		else
		{
			ptrace_area   parea; 
			if(copy_from_user(&parea,(void *)addr,sizeof(parea))==0)
				ret=copy_user(child,parea.kernel_addr,parea.process_addr,
					      parea.len,1,(request==PTRACE_POKEUSR_AREA));
			else ret = -EFAULT;
		}
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
