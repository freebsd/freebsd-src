/* $Id: fpu.c,v 1.1.1.1.2.1 2002/01/25 00:51:42 gniibe Exp $
 *
 * linux/arch/sh/kernel/fpu.c
 *
 * Save/restore floating point context for signal handlers.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1999, 2000  Kaz Kojima & Niibe Yutaka
 *
 * FIXME! These routines can be optimized in big endian case.
 */

#include <linux/sched.h>
#include <linux/signal.h>
#include <asm/processor.h>
#include <asm/io.h>

/* The PR (precision) bit in the FP Status Register must be clear when
 * an frchg instruction is executed, otherwise the instruction is undefined.
 * Executing frchg with PR set causes a trap on some SH4 implementations.
 */

#define FPSCR_RCHG 0x00000000


/*
 * Save FPU registers onto task structure.
 * Assume called with FPU enabled (SR.FD=0).
 */
void
save_fpu(struct task_struct *tsk)
{
	asm volatile("sts.l	fpul, @-%0\n\t"
		     "sts.l	fpscr, @-%0\n\t"
		     "lds	%1, fpscr\n\t"
		     "frchg\n\t"
		     "fmov.s	fr15, @-%0\n\t"
		     "fmov.s	fr14, @-%0\n\t"
		     "fmov.s	fr13, @-%0\n\t"
		     "fmov.s	fr12, @-%0\n\t"
		     "fmov.s	fr11, @-%0\n\t"
		     "fmov.s	fr10, @-%0\n\t"
		     "fmov.s	fr9, @-%0\n\t"
		     "fmov.s	fr8, @-%0\n\t"
		     "fmov.s	fr7, @-%0\n\t"
		     "fmov.s	fr6, @-%0\n\t"
		     "fmov.s	fr5, @-%0\n\t"
		     "fmov.s	fr4, @-%0\n\t"
		     "fmov.s	fr3, @-%0\n\t"
		     "fmov.s	fr2, @-%0\n\t"
		     "fmov.s	fr1, @-%0\n\t"
		     "fmov.s	fr0, @-%0\n\t"
		     "frchg\n\t"
		     "fmov.s	fr15, @-%0\n\t"
		     "fmov.s	fr14, @-%0\n\t"
		     "fmov.s	fr13, @-%0\n\t"
		     "fmov.s	fr12, @-%0\n\t"
		     "fmov.s	fr11, @-%0\n\t"
		     "fmov.s	fr10, @-%0\n\t"
		     "fmov.s	fr9, @-%0\n\t"
		     "fmov.s	fr8, @-%0\n\t"
		     "fmov.s	fr7, @-%0\n\t"
		     "fmov.s	fr6, @-%0\n\t"
		     "fmov.s	fr5, @-%0\n\t"
		     "fmov.s	fr4, @-%0\n\t"
		     "fmov.s	fr3, @-%0\n\t"
		     "fmov.s	fr2, @-%0\n\t"
		     "fmov.s	fr1, @-%0\n\t"
		     "fmov.s	fr0, @-%0\n\t"
		     "lds	%2, fpscr\n\t"
		     : /* no output */
		     : "r" ((char *)(&tsk->thread.fpu.hard.status)),
		       "r" (FPSCR_RCHG),
		       "r" (FPSCR_INIT)
		     : "memory");

	tsk->flags &= ~PF_USEDFPU;
	release_fpu();
}

static void
restore_fpu(struct task_struct *tsk)
{
	asm volatile("lds	%1, fpscr\n\t"
		     "fmov.s	@%0+, fr0\n\t"
		     "fmov.s	@%0+, fr1\n\t"
		     "fmov.s	@%0+, fr2\n\t"
		     "fmov.s	@%0+, fr3\n\t"
		     "fmov.s	@%0+, fr4\n\t"
		     "fmov.s	@%0+, fr5\n\t"
		     "fmov.s	@%0+, fr6\n\t"
		     "fmov.s	@%0+, fr7\n\t"
		     "fmov.s	@%0+, fr8\n\t"
		     "fmov.s	@%0+, fr9\n\t"
		     "fmov.s	@%0+, fr10\n\t"
		     "fmov.s	@%0+, fr11\n\t"
		     "fmov.s	@%0+, fr12\n\t"
		     "fmov.s	@%0+, fr13\n\t"
		     "fmov.s	@%0+, fr14\n\t"
		     "fmov.s	@%0+, fr15\n\t"
		     "frchg\n\t"
		     "fmov.s	@%0+, fr0\n\t"
		     "fmov.s	@%0+, fr1\n\t"
		     "fmov.s	@%0+, fr2\n\t"
		     "fmov.s	@%0+, fr3\n\t"
		     "fmov.s	@%0+, fr4\n\t"
		     "fmov.s	@%0+, fr5\n\t"
		     "fmov.s	@%0+, fr6\n\t"
		     "fmov.s	@%0+, fr7\n\t"
		     "fmov.s	@%0+, fr8\n\t"
		     "fmov.s	@%0+, fr9\n\t"
		     "fmov.s	@%0+, fr10\n\t"
		     "fmov.s	@%0+, fr11\n\t"
		     "fmov.s	@%0+, fr12\n\t"
		     "fmov.s	@%0+, fr13\n\t"
		     "fmov.s	@%0+, fr14\n\t"
		     "fmov.s	@%0+, fr15\n\t"
		     "frchg\n\t"
		     "lds.l	@%0+, fpscr\n\t"
		     "lds.l	@%0+, fpul\n\t"
		     : /* no output */
		     : "r" (&tsk->thread.fpu), "r" (FPSCR_RCHG)
		     : "memory");
}

/*
 * Load the FPU with signalling NANS.  This bit pattern we're using
 * has the property that no matter wether considered as single or as
 * double precission represents signaling NANS.  
 */

static void
fpu_init(void)
{
	asm volatile("lds	%0, fpul\n\t"
		     "lds	%1, fpscr\n\t"
		     "fsts	fpul, fr0\n\t"
		     "fsts	fpul, fr1\n\t"
		     "fsts	fpul, fr2\n\t"
		     "fsts	fpul, fr3\n\t"
		     "fsts	fpul, fr4\n\t"
		     "fsts	fpul, fr5\n\t"
		     "fsts	fpul, fr6\n\t"
		     "fsts	fpul, fr7\n\t"
		     "fsts	fpul, fr8\n\t"
		     "fsts	fpul, fr9\n\t"
		     "fsts	fpul, fr10\n\t"
		     "fsts	fpul, fr11\n\t"
		     "fsts	fpul, fr12\n\t"
		     "fsts	fpul, fr13\n\t"
		     "fsts	fpul, fr14\n\t"
		     "fsts	fpul, fr15\n\t"
		     "frchg\n\t"
		     "fsts	fpul, fr0\n\t"
		     "fsts	fpul, fr1\n\t"
		     "fsts	fpul, fr2\n\t"
		     "fsts	fpul, fr3\n\t"
		     "fsts	fpul, fr4\n\t"
		     "fsts	fpul, fr5\n\t"
		     "fsts	fpul, fr6\n\t"
		     "fsts	fpul, fr7\n\t"
		     "fsts	fpul, fr8\n\t"
		     "fsts	fpul, fr9\n\t"
		     "fsts	fpul, fr10\n\t"
		     "fsts	fpul, fr11\n\t"
		     "fsts	fpul, fr12\n\t"
		     "fsts	fpul, fr13\n\t"
		     "fsts	fpul, fr14\n\t"
		     "fsts	fpul, fr15\n\t"
		     "frchg\n\t"
		     "lds	%2, fpscr\n\t"
		     : /* no output */
		     : "r" (0), "r" (FPSCR_RCHG), "r" (FPSCR_INIT));
}

/**
 *	denormal_to_double - Given denormalized float number,
 *	                     store double float
 *
 *	@fpu: Pointer to sh_fpu_hard structure
 *	@n: Index to FP register
 */
static void
denormal_to_double (struct sh_fpu_hard_struct *fpu, int n)
{
	unsigned long du, dl;
	unsigned long x = fpu->fpul;
	int exp = 1023 - 126;

	if (x != 0 && (x & 0x7f800000) == 0) {
		du = (x & 0x80000000);
		while ((x & 0x00800000) == 0) {
			x <<= 1;
			exp--;
		}
		x &= 0x007fffff;
		du |= (exp << 20) | (x >> 3);
		dl = x << 29;

		fpu->fp_regs[n] = du;
		fpu->fp_regs[n+1] = dl;
	}
}

/**
 *	ieee_fpe_handler - Handle denormalized number exception
 *
 *	@regs: Pointer to register structure
 *
 *	Returns 1 when it's handled (should not cause exception).
 */
static int
ieee_fpe_handler (struct pt_regs *regs)
{
	unsigned short insn = *(unsigned short *) regs->pc;
	unsigned short finsn;
	unsigned long nextpc;
	int nib[4] = {
		(insn >> 12) & 0xf,
		(insn >> 8) & 0xf,
		(insn >> 4) & 0xf,
		insn & 0xf};

	if (nib[0] == 0xb ||
	    (nib[0] == 0x4 && nib[2] == 0x0 && nib[3] == 0xb)) /* bsr & jsr */
		regs->pr = regs->pc + 4;
  
	if (nib[0] == 0xa || nib[0] == 0xb) { /* bra & bsr */
		nextpc = regs->pc + 4 + ((short) ((insn & 0xfff) << 4) >> 3);
		finsn = *(unsigned short *) (regs->pc + 2);
	} else if (nib[0] == 0x8 && nib[1] == 0xd) { /* bt/s */
		if (regs->sr & 1)
			nextpc = regs->pc + 4 + ((char) (insn & 0xff) << 1);
		else
			nextpc = regs->pc + 4;
		finsn = *(unsigned short *) (regs->pc + 2);
	} else if (nib[0] == 0x8 && nib[1] == 0xf) { /* bf/s */
		if (regs->sr & 1)
			nextpc = regs->pc + 4;
		else
			nextpc = regs->pc + 4 + ((char) (insn & 0xff) << 1);
		finsn = *(unsigned short *) (regs->pc + 2);
	} else if (nib[0] == 0x4 && nib[3] == 0xb &&
		 (nib[2] == 0x0 || nib[2] == 0x2)) { /* jmp & jsr */
		nextpc = regs->regs[nib[1]];
		finsn = *(unsigned short *) (regs->pc + 2);
	} else if (nib[0] == 0x0 && nib[3] == 0x3 &&
		 (nib[2] == 0x0 || nib[2] == 0x2)) { /* braf & bsrf */
		nextpc = regs->pc + 4 + regs->regs[nib[1]];
		finsn = *(unsigned short *) (regs->pc + 2);
	} else if (insn == 0x000b) { /* rts */
		nextpc = regs->pr;
		finsn = *(unsigned short *) (regs->pc + 2);
	} else {
		nextpc = regs->pc + 2;
		finsn = insn;
	}

	if ((finsn & 0xf1ff) == 0xf0ad) { /* fcnvsd */
		struct task_struct *tsk = current;

		save_fpu(tsk);
		if ((tsk->thread.fpu.hard.fpscr & (1 << 17))) {
			/* FPU error */
			denormal_to_double (&tsk->thread.fpu.hard,
					    (finsn >> 8) & 0xf);
			tsk->thread.fpu.hard.fpscr &=
				~(FPSCR_CAUSE_MASK | FPSCR_FLAG_MASK);
			grab_fpu();
			restore_fpu(tsk);
			tsk->flags |= PF_USEDFPU;
		} else {
			tsk->thread.trap_no = 11;
			tsk->thread.error_code = 0;
			force_sig(SIGFPE, tsk);
		}

		regs->pc = nextpc;
		return 1;
	}

	return 0;
}

asmlinkage void
do_fpu_error(unsigned long r4, unsigned long r5, unsigned long r6, unsigned long r7,
	     struct pt_regs regs)
{
	struct task_struct *tsk = current;

	if (ieee_fpe_handler (&regs))
		return;

	regs.pc += 2;
	save_fpu(tsk);
	tsk->thread.trap_no = 11;
	tsk->thread.error_code = 0;
	force_sig(SIGFPE, tsk);
}

asmlinkage void
do_fpu_state_restore(unsigned long r4, unsigned long r5, unsigned long r6,
		     unsigned long r7, struct pt_regs regs)
{
	struct task_struct *tsk = current;

	grab_fpu();
	if (!user_mode(&regs)) {
		printk(KERN_ERR "BUG: FPU is used in kernel mode.\n");
		return;
	}

	if (tsk->used_math) {
		/* Using the FPU again.  */
		restore_fpu(tsk);
	} else	{
		/* First time FPU user.  */
		fpu_init();
		tsk->used_math = 1;
	}
	tsk->flags |= PF_USEDFPU;
}
