/* linux/arch/cris/entryoffsets.c
 *
 * Copyright (C) 2001 Axis Communications AB
 *
 * Generate structure offsets for use in entry.S.  No extra processing
 * needed more than compiling this file to assembly code.  Horrendous
 * assembly code will be generated, so don't look at that.
 *
 * Authors:	Hans-Peter Nilsson (hp@axis.com)
 */

/* There can be string constants fallout from inline functions, so we'd
   better make sure we don't assemble anything emitted from inclusions.  */
__asm__ (".if 0");

#include <linux/sched.h>
#include <linux/ptrace.h>
#include <asm/processor.h>

/* Exclude everything except the assembly by wrapping it in ".if 0".  */
#undef VAL
#define VAL(NAME, VALUE)			\
void NAME ## _fun (void)			\
 {						\
  __asm__ (".endif \n"				\
	   #NAME " = %0 \n"			\
	   ".if 0\n"				\
	   : : "i" (VALUE));			\
 }

#undef OF
#define OF(NAME, TYPE, MEMBER)			\
  VAL (NAME, offsetof (TYPE, MEMBER))

/* task_struct offsets.  */
OF (LTASK_SIGPENDING, struct task_struct, sigpending)
OF (LTASK_NEEDRESCHED, struct task_struct, need_resched)
OF (LTASK_PTRACE, struct task_struct, ptrace)
OF (LTASK_PID, struct task_struct, pid)

/* pt_regs offsets.  */
OF (LORIG_R10, struct pt_regs, orig_r10)
OF (LR13, struct pt_regs, r13)
OF (LR12, struct pt_regs, r12)
OF (LR11, struct pt_regs, r11)
OF (LR10, struct pt_regs, r10)
OF (LR9, struct pt_regs, r9)
OF (LMOF, struct pt_regs, mof)
OF (LDCCR, struct pt_regs, dccr)
OF (LSRP, struct pt_regs, srp)
OF (LIRP, struct pt_regs, irp)

/* thread_struct offsets.  */
OF (LTHREAD_KSP, struct thread_struct, ksp)
OF (LTHREAD_USP, struct thread_struct, usp)
OF (LTHREAD_DCCR, struct thread_struct, dccr)

/* linux/sched.h values - doesn't have an #ifdef __ASSEMBLY__ for these.  */
VAL (LCLONE_VM, CLONE_VM)

__asm__ (".endif");
