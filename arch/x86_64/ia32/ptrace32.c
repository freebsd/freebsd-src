/* 
 * 32bit ptrace for x86-64.
 *
 * Copyright 2001,2002 Andi Kleen, SuSE Labs.
 * Some parts copied from arch/i386/kernel/ptrace.c. See that file for earlier 
 * copyright.
 * 
 * This allows to access 64bit processes too; but there is no way to see the extended 
 * register contents.
 *
 * $Id: ptrace32.c,v 1.19 2004/01/29 03:31:13 ak Exp $
 */ 

#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/ptrace.h>
#include <asm/uaccess.h>
#include <asm/user32.h>
#include <asm/user.h>
#include <asm/errno.h>
#include <asm/debugreg.h>
#include <asm/i387.h>
#include <asm/fpu32.h>
#include <linux/mm.h>

/* determines which flags the user has access to. */
/* 1 = access 0 = no access */
#define FLAG_MASK 0x44dd5UL

#define R32(l,q) \
	case offsetof(struct user32, regs.l): stack[offsetof(struct pt_regs, q)/8] = val; break

static int putreg32(struct task_struct *child, unsigned regno, u32 val)
{
	int i;
	__u64 *stack = (__u64 *)(child->thread.rsp0 - sizeof(struct pt_regs)); 

	switch (regno) {
	case offsetof(struct user32, regs.fs):
        if (val && (val & 3) != 3) return -EIO;
        child->thread.fs = val & 0xffff; 
		break;
	case offsetof(struct user32, regs.gs):
	    if (val && (val & 3) != 3) return -EIO;
		child->thread.gs = val & 0xffff;
		break;
	case offsetof(struct user32, regs.ds):
		if (val && (val & 3) != 3) return -EIO; 
		child->thread.ds = val & 0xffff;
		break;
	case offsetof(struct user32, regs.es):
		child->thread.es = val & 0xffff;
		break;
    case offsetof(struct user32, regs.ss):
		if ((val & 3) != 3) return -EIO; 
        stack[offsetof(struct pt_regs, ss)/8] = val & 0xffff;
        break;
	case offsetof(struct user32, regs.cs):
		if ((val & 3) != 3) return -EIO;
		stack[offsetof(struct pt_regs, cs)/8] = val & 0xffff;
		break;

	R32(ebx, rbx); 
	R32(ecx, rcx);
	R32(edx, rdx);
	R32(edi, rdi);
	R32(esi, rsi);
	R32(ebp, rbp);
	R32(eax, rax);
	R32(orig_eax, orig_rax);
	R32(eip, rip);
	R32(esp, rsp);

	case offsetof(struct user32, regs.eflags): { 
		__u64 *flags = &stack[offsetof(struct pt_regs, eflags)/8];
		val &= FLAG_MASK;
		*flags = val | (*flags & ~FLAG_MASK);
		break;
	}

	case offsetof(struct user32, u_debugreg[4]): 
	case offsetof(struct user32, u_debugreg[5]):
		return -EIO;

	case offsetof(struct user32, u_debugreg[0]) ...
	     offsetof(struct user32, u_debugreg[3]):
	case offsetof(struct user32, u_debugreg[6]):
		child->thread.debugreg
			[(regno-offsetof(struct user32, u_debugreg[0]))/4] 
			= val; 
		break; 

	case offsetof(struct user32, u_debugreg[7]):
		val &= ~DR_CONTROL_RESERVED;
		/* You are not expected to understand this ... I don't neither. */
		for(i=0; i<4; i++)
			if ((0x5454 >> ((val >> (16 + 4*i)) & 0xf)) & 1)
			       return -EIO;
		child->thread.debugreg[7] = val; 
		break; 
		    
	default:
		if (regno > sizeof(struct user32) || (regno & 3))
			return -EIO;
	       
		/* Other dummy fields in the virtual user structure are ignored */ 
		break; 		
	}
	return 0;
}

#undef R32

#define R32(l,q) \
	case offsetof(struct user32, regs.l): *val = stack[offsetof(struct pt_regs, q)/8]; break

static int getreg32(struct task_struct *child, unsigned regno, u32 *val)
{
	__u64 *stack = (__u64 *)(child->thread.rsp0 - sizeof(struct pt_regs)); 

	switch (regno) {
	case offsetof(struct user32, regs.fs):
	        *val = child->thread.fs; 
		break;
	case offsetof(struct user32, regs.gs):
		*val = child->thread.gs;
		break;
	case offsetof(struct user32, regs.ds):
		*val = child->thread.ds;
		break;
	case offsetof(struct user32, regs.es):
		*val = child->thread.es;
		break;

	R32(cs, cs);
	R32(ss, ss);
	R32(ebx, rbx); 
	R32(ecx, rcx);
	R32(edx, rdx);
	R32(edi, rdi);
	R32(esi, rsi);
	R32(ebp, rbp);
	R32(eax, rax);
	R32(orig_eax, orig_rax);
	R32(eip, rip);
	R32(eflags, eflags);
	R32(esp, rsp);

	case offsetof(struct user32, u_debugreg[0]) ... offsetof(struct user32, u_debugreg[7]):
		*val = child->thread.debugreg[(regno-offsetof(struct user32, u_debugreg[0]))/4]; 
		break; 
		    
	default:
		if (regno > sizeof(struct user32) || (regno & 3))
			return -EIO;

		/* Other dummy fields in the virtual user structure are ignored */ 
		*val = 0;
		break; 		
	}
	return 0;
}

#undef R32

static struct task_struct *find_target(int request, int pid, int *err)
{ 
	struct task_struct *child;

	*err = -EPERM; 
	if (pid == 1)
		return NULL; 

	*err = -ESRCH;
	read_lock(&tasklist_lock);
	child = find_task_by_pid(pid);
	if (child)
		get_task_struct(child);
	read_unlock(&tasklist_lock);
	if (child) { 
		*err = -EPERM;
		if (pid == 1) 
			goto out;
		*err = ptrace_check_attach(child, request == PTRACE_KILL); 
		if (*err < 0) 
				goto out;
		return child; 
	} 

 out:
	free_task_struct(child);
	return NULL; 
	
} 

extern asmlinkage long sys_ptrace(long request, long pid, unsigned long addr, unsigned long data);

asmlinkage long sys32_ptrace(long request, u32 pid, u32 addr, u32 data)
{
	struct task_struct *child;
	struct pt_regs *childregs; 
	int ret;
	__u32 val;

	switch (request) { 
	case PTRACE_TRACEME:
	case PTRACE_ATTACH:
	case PTRACE_SYSCALL:
	case PTRACE_CONT:
	case PTRACE_KILL:
	case PTRACE_SINGLESTEP:
	case PTRACE_DETACH:
	case PTRACE_SETOPTIONS:
		ret = sys_ptrace(request, pid, addr, data); 
		return ret;

	case PTRACE_PEEKTEXT:
	case PTRACE_PEEKDATA:
	case PTRACE_POKEDATA:
	case PTRACE_POKETEXT:
	case PTRACE_POKEUSR:       
	case PTRACE_PEEKUSR:
	case PTRACE_GETREGS:
	case PTRACE_SETREGS:
	case PTRACE_SETFPREGS:
	case PTRACE_GETFPREGS:
	case PTRACE_SETFPXREGS:
	case PTRACE_GETFPXREGS:
		break;
		
	default:
		return -EIO;
	} 

	child = find_target(request, pid, &ret);
	if (!child)
		return ret;
	
	childregs = (struct pt_regs *)(child->thread.rsp0 - sizeof(struct pt_regs)); 

	switch (request) {
	case PTRACE_PEEKDATA:
	case PTRACE_PEEKTEXT:
		ret = 0;
		if (access_process_vm(child, addr, &val, sizeof(u32), 0)!=sizeof(u32))
			ret = -EIO;
		else
			ret = put_user(val, (unsigned int *)(u64)data); 
		break; 

	case PTRACE_POKEDATA:
	case PTRACE_POKETEXT:
		ret = 0;
		if (access_process_vm(child, addr, &data, sizeof(u32), 1)!=sizeof(u32))
			ret = -EIO; 
		break;

	case PTRACE_PEEKUSR:
		ret = getreg32(child, addr, &val);
		if (ret == 0)
			ret = put_user(val, (__u32 *)(unsigned long) data);
		break;

	case PTRACE_POKEUSR:
		ret = putreg32(child, addr, data);
		break;

	case PTRACE_GETREGS: { /* Get all gp regs from the child. */
		int i;
	  	if (!access_ok(VERIFY_WRITE, (unsigned *)(unsigned long)data, 16*4)) {
			ret = -EIO;
			break;
		}
		ret = 0;
		for ( i = 0; i <= 16*4 ; i += sizeof(__u32) ) {
			getreg32(child, i, &val);
			ret |= __put_user(val,(u32 *) (unsigned long) data);
			data += sizeof(u32);
		}
		break;
	}

	case PTRACE_SETREGS: { /* Set all gp regs in the child. */
		unsigned long tmp;
		int i;
	  	if (!access_ok(VERIFY_READ, (unsigned *)(unsigned long)data, 16*4)) {
			ret = -EIO;
			break;
		}
		ret = 0; 
		for ( i = 0; i <= 16*4; i += sizeof(u32) ) {
			ret |= __get_user(tmp, (u32 *) (unsigned long) data);
			putreg32(child, i, tmp);
			data += sizeof(u32);
		}
		break;
	}

	case PTRACE_GETFPREGS:
		ret = -EIO; 
		if (!access_ok(VERIFY_READ, (void *)(u64)data, 
			       sizeof(struct user_i387_struct)))
			break;
		save_i387_ia32(child, (void *)(u64)data, childregs, 1);
		ret = 0; 
		break;

	case PTRACE_SETFPREGS:
		ret = -EIO;
		if (!access_ok(VERIFY_WRITE, (void *)(u64)data, 
			       sizeof(struct user_i387_struct)))
			break;
		ret = 0;
		/* don't check EFAULT to be bug-to-bug compatible to i386 */
		restore_i387_ia32(child, (void *)(u64)data, 1);
		break;

	case PTRACE_GETFPXREGS: { 
		struct user32_fxsr_struct *u = (void *)(u64)data; 
		init_fpu(child); 
		ret = -EIO;
		if (!access_ok(VERIFY_WRITE, u, sizeof(*u)))
			break;
			ret = -EFAULT;
		if (__copy_to_user(u, &child->thread.i387.fxsave, sizeof(*u)))
			break;
		ret = __put_user(childregs->cs, &u->fcs);
		ret |= __put_user(child->thread.ds, &u->fos); 
		break; 
	} 
	case PTRACE_SETFPXREGS: { 
		struct user32_fxsr_struct *u = (void *)(u64)data; 
		unlazy_fpu(child);
		ret = -EIO;
		if (!access_ok(VERIFY_READ, u, sizeof(*u)))
			break;
		/* no checking to be bug-to-bug compatible with i386 */
		__copy_from_user(&child->thread.i387.fxsave, u, sizeof(*u));
		child->used_math = 1;
	        child->thread.i387.fxsave.mxcsr &= 0xffbf;
		ret = 0; 
		break; 
	} 

	default:
		ret = -EINVAL;
		break;
	}

	free_task_struct(child);
	return ret;
}

