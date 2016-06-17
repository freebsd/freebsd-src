/*
 * include/asm-x86_64/i387.h
 *
 * Copyright (C) 1994 Linus Torvalds
 *
 * Pentium III FXSR, SSE support
 * General FPU state handling cleanups
 *	Gareth Hughes <gareth@valinux.com>, May 2000
 * x86-64 work by Andi Kleen 2002
 */

#ifndef __ASM_X86_64_I387_H
#define __ASM_X86_64_I387_H

#include <linux/sched.h>
#include <asm/processor.h>
#include <asm/sigcontext.h>
#include <asm/user.h>
#include <asm/uaccess.h>

extern void init_fpu(struct task_struct *child);
extern int save_i387(struct _fpstate *buf);

/*
 * FPU lazy state save handling...
 */

#define kernel_fpu_end() stts()

#define unlazy_fpu( tsk ) do { \
	if ( tsk->flags & PF_USEDFPU ) \
		save_init_fpu( tsk ); \
} while (0)

#define clear_fpu( tsk ) do { \
	if ( tsk->flags & PF_USEDFPU ) { \
		asm volatile("fwait"); \
		tsk->flags &= ~PF_USEDFPU; \
		stts(); \
	} \
} while (0)

#define load_mxcsr( val ) do { \
		unsigned long __mxcsr = ((unsigned long)(val) & 0xffbf); \
		asm volatile( "ldmxcsr %0" : : "m" (__mxcsr) ); \
} while (0)

/*
 * ptrace request handers...
 */
extern int get_fpregs( struct user_i387_struct *buf,
		       struct task_struct *tsk );
extern int set_fpregs( struct task_struct *tsk,
		       struct user_i387_struct *buf );

/*
 * FPU state for core dumps...
 */
extern int dump_fpu( struct pt_regs *regs,
		     struct user_i387_struct *fpu );

/* 
 * i387 state interaction
 */
#define get_fpu_mxcsr(t) ((t)->thread.i387.fxsave.mxcsr)
#define get_fpu_cwd(t) ((t)->thread.i387.fxsave.cwd)
#define get_fpu_fxsr_twd(t) ((t)->thread.i387.fxsave.twd)
#define get_fpu_swd(t) ((t)->thread.i387.fxsave.swd)
#define set_fpu_cwd(t,val) ((t)->thread.i387.fxsave.cwd = (val))
#define set_fpu_swd(t,val) ((t)->thread.i387.fxsave.swd = (val))
#define set_fpu_fxsr_twd(t,val) ((t)->thread.i387.fxsave.twd = (val))
#define set_fpu_mxcsr(t,val) ((t)->thread.i387.fxsave.mxcsr = (val)&0xffbf)

static inline int restore_fpu_checking(struct i387_fxsave_struct *fx) 
{ 
	int err;
	asm volatile("1:  rex64 ; fxrstor (%[fx])\n\t"
		     "2:\n"
		     ".section .fixup,\"ax\"\n"
		     "3:  movl $-1,%[err]\n"
		     "    jmp  2b\n"
		     ".previous\n"
		     ".section __ex_table,\"a\"\n"
		     "   .align 8\n"
		     "   .quad  1b,3b\n"
		     ".previous"
		     : [err] "=r" (err)
		     : [fx] "r" (fx), "0" (0)); 
	if (unlikely(err))
		init_fpu(current);
	return err;
} 

static inline int save_i387_checking(struct i387_fxsave_struct *fx) 
{ 
	int err;
	asm volatile("1:  rex64 ; fxsave (%[fx])\n\t"
		     "2:\n"
		     ".section .fixup,\"ax\"\n"
		     "3:  movl $-1,%[err]\n"
		     "    jmp  2b\n"
		     ".previous\n"
		     ".section __ex_table,\"a\"\n"
		     "   .align 8\n"
		     "   .quad  1b,3b\n"
		     ".previous"
		     : [err] "=r" (err)
		     : [fx] "r" (fx), "0" (0)); 
	if (unlikely(err))
		__clear_user(fx, sizeof(struct i387_fxsave_struct));
	return err;
} 

static inline void kernel_fpu_begin(void)
{
	struct task_struct *tsk = current;
	if (tsk->flags & PF_USEDFPU) {
		asm volatile("rex64 ; fxsave %0 ; fnclex"
			      : "=m" (tsk->thread.i387.fxsave));
		tsk->flags &= ~PF_USEDFPU;
		return;
	}
	clts();
}

static inline void save_init_fpu( struct task_struct *tsk )
{
	asm volatile( "fxsave %0 ; fnclex"
		      : "=m" (tsk->thread.i387.fxsave));
	tsk->flags &= ~PF_USEDFPU;
	stts();
}

/* 
 * This restores directly out of user space. Exceptions are handled.
 */ 
static inline int restore_i387(struct _fpstate *buf)
{
	return restore_fpu_checking((struct i387_fxsave_struct *)buf);
}

#endif /* __ASM_X86_64_I387_H */
