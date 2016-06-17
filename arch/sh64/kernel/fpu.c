/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * arch/sh64/kernel/fpu.c
 *
 * Copyright (C) 2001  Manuela Cirronis, Paolo Alberelli
 *
 * Started from SH4 version:
 *   Copyright (C) 1999, 2000  Kaz Kojima & Niibe Yutaka
 *
 */

#include <linux/sched.h>
#include <linux/signal.h>
#include <asm/processor.h>
#include <asm/user.h>
#include <asm/io.h>

/*
 * Initially load the FPU with signalling NANS.  This bit pattern
 * has the property that no matter whether considered as single or as
 * double precision, it still represents a signalling NAN.  
 */
#define sNAN64		0xFFFFFFFFFFFFFFFFULL
#define sNAN32		0xFFFFFFFFUL

static union sh_fpu_union init_fpuregs = {
	.hard = {
	  .fp_regs = { [0 ... 63] = sNAN32 },
	  .fpscr = FPSCR_INIT
	}
};
#if 0
static struct sh_fpu_hard_struct init_fpuregs = {
	{ [0 ... 63] = sNAN32 },
	FPSCR_INIT
};
#endif

inline void fpsave(struct sh_fpu_hard_struct *fpregs)
{
	asm volatile("fst.p     %0, (0*8),"  __p(0)  "\n\t"
		     "fst.p     %0, (1*8),"  __p(2)  "\n\t"
		     "fst.p     %0, (2*8),"  __p(4)  "\n\t"
		     "fst.p     %0, (3*8),"  __p(6)  "\n\t"
		     "fst.p     %0, (4*8),"  __p(8)  "\n\t"
		     "fst.p     %0, (5*8),"  __p(10) "\n\t"
		     "fst.p     %0, (6*8),"  __p(12) "\n\t"
		     "fst.p     %0, (7*8),"  __p(14) "\n\t"
		     "fst.p     %0, (8*8),"  __p(16) "\n\t"
		     "fst.p     %0, (9*8),"  __p(18) "\n\t"
		     "fst.p     %0, (10*8)," __p(20) "\n\t"
		     "fst.p     %0, (11*8)," __p(22) "\n\t"
		     "fst.p     %0, (12*8)," __p(24) "\n\t"
		     "fst.p     %0, (13*8)," __p(26) "\n\t"
		     "fst.p     %0, (14*8)," __p(28) "\n\t"
		     "fst.p     %0, (15*8)," __p(30) "\n\t"
		     "fst.p     %0, (16*8)," __p(32) "\n\t"
		     "fst.p     %0, (17*8)," __p(34) "\n\t"
		     "fst.p     %0, (18*8)," __p(36) "\n\t"
		     "fst.p     %0, (19*8)," __p(38) "\n\t"
		     "fst.p     %0, (20*8)," __p(40) "\n\t"
		     "fst.p     %0, (21*8)," __p(42) "\n\t"
		     "fst.p     %0, (22*8)," __p(44) "\n\t"
		     "fst.p     %0, (23*8)," __p(46) "\n\t"
		     "fst.p     %0, (24*8)," __p(48) "\n\t"
		     "fst.p     %0, (25*8)," __p(50) "\n\t"
		     "fst.p     %0, (26*8)," __p(52) "\n\t"
		     "fst.p     %0, (27*8)," __p(54) "\n\t"
		     "fst.p     %0, (28*8)," __p(56) "\n\t"
		     "fst.p     %0, (29*8)," __p(58) "\n\t"
		     "fst.p     %0, (30*8)," __p(60) "\n\t"
		     "fst.p     %0, (31*8)," __p(62) "\n\t"

		     "_fgetscr  " __f(63) 	     "\n\t"
		     "fst.s     %0, (32*8)," __f(63) "\n\t"
		: /* no output */
		: "r" (fpregs)
		: "memory");
}


static inline void
fpload(struct sh_fpu_hard_struct *fpregs)
{
	asm volatile("fld.p     %0, (0*8),"  __p(0)  "\n\t"
		     "fld.p     %0, (1*8),"  __p(2)  "\n\t"
		     "fld.p     %0, (2*8),"  __p(4)  "\n\t"
		     "fld.p     %0, (3*8),"  __p(6)  "\n\t"
		     "fld.p     %0, (4*8),"  __p(8)  "\n\t"
		     "fld.p     %0, (5*8),"  __p(10) "\n\t"
		     "fld.p     %0, (6*8),"  __p(12) "\n\t"
		     "fld.p     %0, (7*8),"  __p(14) "\n\t"
		     "fld.p     %0, (8*8),"  __p(16) "\n\t"
		     "fld.p     %0, (9*8),"  __p(18) "\n\t"
		     "fld.p     %0, (10*8)," __p(20) "\n\t"
		     "fld.p     %0, (11*8)," __p(22) "\n\t"
		     "fld.p     %0, (12*8)," __p(24) "\n\t"
		     "fld.p     %0, (13*8)," __p(26) "\n\t"
		     "fld.p     %0, (14*8)," __p(28) "\n\t"
		     "fld.p     %0, (15*8)," __p(30) "\n\t"
		     "fld.p     %0, (16*8)," __p(32) "\n\t"
		     "fld.p     %0, (17*8)," __p(34) "\n\t"
		     "fld.p     %0, (18*8)," __p(36) "\n\t"
		     "fld.p     %0, (19*8)," __p(38) "\n\t"
		     "fld.p     %0, (20*8)," __p(40) "\n\t"
		     "fld.p     %0, (21*8)," __p(42) "\n\t"
		     "fld.p     %0, (22*8)," __p(44) "\n\t"
		     "fld.p     %0, (23*8)," __p(46) "\n\t"
		     "fld.p     %0, (24*8)," __p(48) "\n\t"
		     "fld.p     %0, (25*8)," __p(50) "\n\t"
		     "fld.p     %0, (26*8)," __p(52) "\n\t"
		     "fld.p     %0, (27*8)," __p(54) "\n\t"
		     "fld.p     %0, (28*8)," __p(56) "\n\t"
		     "fld.p     %0, (29*8)," __p(58) "\n\t"
		     "fld.p     %0, (30*8)," __p(60) "\n\t"

		     "fld.s     %0, (32*8)," __f(63) "\n\t"
		     "_fputscr  " __f(63) 	     "\n\t"

	     	     "fld.p     %0, (31*8)," __p(62) "\n\t"
		: /* no output */
		: "r" (fpregs) );
}

void fpinit(struct sh_fpu_hard_struct *fpregs)
{
	*fpregs = init_fpuregs.hard;
}

asmlinkage void
do_fpu_error(unsigned long ex, struct pt_regs *regs)
{
	struct task_struct *tsk = current;

	regs->pc += 4;

	tsk->thread.trap_no = 11;
	tsk->thread.error_code = 0;
	force_sig(SIGFPE, tsk);
}


asmlinkage void
do_fpu_state_restore(unsigned long ex, struct pt_regs *regs)
{
	void die(const char * str, struct pt_regs * regs, long err);

#if 0
printk("do_fpu_state_restore (pid %d, used_math %d, last_used_math pid %d)\n",
       current->pid, current->used_math,
       last_task_used_math ? last_task_used_math->pid : -1);
#endif

	if (! user_mode(regs))
		die("FPU used in kernel", regs, ex);

	regs->sr &= ~SR_FD;

	if (last_task_used_math == current)
		return;

	grab_fpu();
	if (last_task_used_math != NULL) {
		/* Other processes fpu state, save away */
		fpsave(&last_task_used_math->thread.fpu.hard);
        }
        last_task_used_math = current;
        if (current->used_math) {
                fpload(&current->thread.fpu.hard);
        } else {
		/* First time FPU user.  */
		fpload(&init_fpuregs.hard);
                current->used_math = 1;
        }
	release_fpu();
}

