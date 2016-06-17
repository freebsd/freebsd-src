/*
 * include/asm-i386/i387.h
 *
 * Copyright (C) 1994 Linus Torvalds
 *
 * Pentium III FXSR, SSE support
 * General FPU state handling cleanups
 *	Gareth Hughes <gareth@valinux.com>, May 2000
 */

#ifndef __ASM_I386_I387_H
#define __ASM_I386_I387_H

#include <linux/sched.h>
#include <asm/processor.h>
#include <asm/sigcontext.h>
#include <asm/user.h>

extern void init_fpu(void);
/*
 * FPU lazy state save handling...
 */
extern void save_init_fpu( struct task_struct *tsk );
extern void restore_fpu( struct task_struct *tsk );

extern void kernel_fpu_begin(void);
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

/*
 * FPU state interaction...
 */
extern unsigned short get_fpu_cwd( struct task_struct *tsk );
extern unsigned short get_fpu_swd( struct task_struct *tsk );
extern unsigned short get_fpu_twd( struct task_struct *tsk );
extern unsigned short get_fpu_mxcsr( struct task_struct *tsk );

extern void set_fpu_cwd( struct task_struct *tsk, unsigned short cwd );
extern void set_fpu_swd( struct task_struct *tsk, unsigned short swd );
extern void set_fpu_twd( struct task_struct *tsk, unsigned short twd );
extern void set_fpu_mxcsr( struct task_struct *tsk, unsigned short mxcsr );

#define load_mxcsr( val ) do { \
	unsigned long __mxcsr = ((unsigned long)(val) & 0xffbf); \
	asm volatile( "ldmxcsr %0" : : "m" (__mxcsr) ); \
} while (0)

/*
 * Signal frame handlers...
 */
extern int save_i387( struct _fpstate *buf );
extern int restore_i387( struct _fpstate *buf );

/*
 * ptrace request handers...
 */
extern int get_fpregs( struct user_i387_struct *buf,
		       struct task_struct *tsk );
extern int set_fpregs( struct task_struct *tsk,
		       struct user_i387_struct *buf );

extern int get_fpxregs( struct user_fxsr_struct *buf,
			struct task_struct *tsk );
extern int set_fpxregs( struct task_struct *tsk,
			struct user_fxsr_struct *buf );
extern void load_empty_fpu(struct task_struct *);

/*
 * FPU state for core dumps...
 */
extern int dump_fpu( struct pt_regs *regs,
		     struct user_i387_struct *fpu );
extern int dump_extended_fpu( struct pt_regs *regs,
			      struct user_fxsr_struct *fpu );

#endif /* __ASM_I386_I387_H */
