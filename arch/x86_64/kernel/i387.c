/*
 *  linux/arch/x86_64/kernel/i387.c
 *
 *  Copyright (C) 1994 Linus Torvalds
 *  Copyright (C) 2002 Andi Kleen, SuSE Labs
 *
 *  Pentium III FXSR, SSE support
 *  General FPU state handling cleanups
 *	Gareth Hughes <gareth@valinux.com>, May 2000
 * 
 *  x86-64 rework 2002 Andi Kleen. 
 *  Does direct fxsave in and out of user space now for signal handlers.
 *  All the FSAVE<->FXSAVE conversion code has been moved to the 32bit emulation,
 *  the 64bit user space sees a FXSAVE frame directly. 
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <asm/processor.h>
#include <asm/i387.h>
#include <asm/sigcontext.h>
#include <asm/user.h>
#include <asm/ptrace.h>
#include <asm/uaccess.h>

extern int exception_trace;

void init_fpu(struct task_struct *child)
{
	if (child->used_math) { 
		unlazy_fpu(child);
		return;
	}
	memset(&child->thread.i387.fxsave, 0, sizeof(struct i387_fxsave_struct));
	child->thread.i387.fxsave.cwd = 0x37f;
	child->thread.i387.fxsave.mxcsr = 0x1f80;
	child->used_math = 1;
}

/*
 * Signal frame handlers.
 */

int save_i387(struct _fpstate *buf)
{
	struct task_struct *tsk = current;
	int err = 0;

	{ 
		extern void bad_user_i387_struct(void); 
		if (sizeof(struct user_i387_struct) != sizeof(tsk->thread.i387.fxsave))
			bad_user_i387_struct();
	} 

	if (!tsk->used_math) 
		return 0; 
	tsk->used_math = 0; /* trigger finit */ 
	if (tsk->flags & PF_USEDFPU) { 
		err = save_i387_checking((struct i387_fxsave_struct *)buf);
		if (err) { 
			if (exception_trace) 
				printk("%s[%d] unaligned signal floating point context %p\n", 
					tsk->comm, tsk->pid, buf); 
			return err;
		}
		stts();
	} else { 
		if (__copy_to_user(buf, &tsk->thread.i387.fxsave, 
				   sizeof(struct i387_fxsave_struct)))
			return -1;
	} 
	return 1; 
}

/*
 * ptrace request handlers.
 */

int get_fpregs(struct user_i387_struct *buf, struct task_struct *tsk)
{
	init_fpu(tsk);
	return __copy_to_user((void *)buf, &tsk->thread.i387.fxsave,
			       sizeof(struct user_i387_struct)) ? -EFAULT : 0;
}

int set_fpregs(struct task_struct *tsk, struct user_i387_struct *buf)
{	
	if (__copy_from_user(&tsk->thread.i387.fxsave, buf, 
			     sizeof(struct user_i387_struct)))
		return -EFAULT;
	/* mxcsr bit 6 and 31-16 must be zero for security reasons. */
	tsk->thread.i387.fxsave.mxcsr &= 0xffbf;
	return 0;
}

/*
 * FPU state for core dumps.
 */

int dump_fpu( struct pt_regs *regs, struct user_i387_struct *fpu )
{
	struct task_struct *tsk = current;

	if (!tsk->used_math) 
		return 0;
	unlazy_fpu(tsk);

	memcpy(fpu, &tsk->thread.i387.fxsave, sizeof(struct user_i387_struct)); 
	return 1; 
}
