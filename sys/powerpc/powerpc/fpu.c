/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$NetBSD: fpu.c,v 1.5 2001/07/22 11:29:46 wiz Exp $
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/limits.h>

#include <machine/fpu.h>
#include <machine/pcb.h>
#include <machine/psl.h>
#include <machine/altivec.h>

static void
save_fpu_int(struct thread *td)
{
	register_t msr;
	struct	pcb *pcb;

	pcb = td->td_pcb;

	/*
	 * Temporarily re-enable floating-point during the save
	 */
	msr = mfmsr();
	if (pcb->pcb_flags & PCB_VSX)
		mtmsr(msr | PSL_FP | PSL_VSX);
	else
		mtmsr(msr | PSL_FP);

	/*
	 * Save the floating-point registers and FPSCR to the PCB
	 */
	if (pcb->pcb_flags & PCB_VSX) {
	#define SFP(n)   __asm ("stxvw4x " #n ", 0,%0" \
			:: "b"(&pcb->pcb_fpu.fpr[n]));
		SFP(0);		SFP(1);		SFP(2);		SFP(3);
		SFP(4);		SFP(5);		SFP(6);		SFP(7);
		SFP(8);		SFP(9);		SFP(10);	SFP(11);
		SFP(12);	SFP(13);	SFP(14);	SFP(15);
		SFP(16);	SFP(17);	SFP(18);	SFP(19);
		SFP(20);	SFP(21);	SFP(22);	SFP(23);
		SFP(24);	SFP(25);	SFP(26);	SFP(27);
		SFP(28);	SFP(29);	SFP(30);	SFP(31);
	#undef SFP
	} else {
	#define SFP(n)   __asm ("stfd " #n ", 0(%0)" \
			:: "b"(&pcb->pcb_fpu.fpr[n].fpr));
		SFP(0);		SFP(1);		SFP(2);		SFP(3);
		SFP(4);		SFP(5);		SFP(6);		SFP(7);
		SFP(8);		SFP(9);		SFP(10);	SFP(11);
		SFP(12);	SFP(13);	SFP(14);	SFP(15);
		SFP(16);	SFP(17);	SFP(18);	SFP(19);
		SFP(20);	SFP(21);	SFP(22);	SFP(23);
		SFP(24);	SFP(25);	SFP(26);	SFP(27);
		SFP(28);	SFP(29);	SFP(30);	SFP(31);
	#undef SFP
	}
	__asm __volatile ("mffs 0; stfd 0,0(%0)" :: "b"(&pcb->pcb_fpu.fpscr));

	/*
	 * Disable floating-point again
	 */
	isync();
	mtmsr(msr);
}

void
enable_fpu(struct thread *td)
{
	register_t msr;
	struct	pcb *pcb;
	struct	trapframe *tf;

	pcb = td->td_pcb;
	tf = trapframe(td);

	/*
	 * Save the thread's FPU CPU number, and set the CPU's current
	 * FPU thread
	 */
	td->td_pcb->pcb_fpcpu = PCPU_GET(cpuid);
	PCPU_SET(fputhread, td);

	/*
	 * Enable the FPU for when the thread returns from the exception.
	 * If this is the first time the FPU has been used by the thread,
	 * initialise the FPU registers and FPSCR to 0, and set the flag
	 * to indicate that the FPU is in use.
	 */
	pcb->pcb_flags |= PCB_FPU;
	if (pcb->pcb_flags & PCB_VSX)
		tf->srr1 |= PSL_FP | PSL_VSX;
	else
		tf->srr1 |= PSL_FP;
	if (!(pcb->pcb_flags & PCB_FPREGS)) {
		memset(&pcb->pcb_fpu, 0, sizeof pcb->pcb_fpu);
		pcb->pcb_flags |= PCB_FPREGS;
	}

	/*
	 * Temporarily enable floating-point so the registers
	 * can be restored.
	 */
	msr = mfmsr();
	if (pcb->pcb_flags & PCB_VSX)
		mtmsr(msr | PSL_FP | PSL_VSX);
	else
		mtmsr(msr | PSL_FP);

	/*
	 * Load the floating point registers and FPSCR from the PCB.
	 * (A value of 0xff for mtfsf specifies that all 8 4-bit fields
	 * of the saved FPSCR are to be loaded from the FPU reg).
	 */
	__asm __volatile ("lfd 0,0(%0); mtfsf 0xff,0"
			  :: "b"(&pcb->pcb_fpu.fpscr));

	if (pcb->pcb_flags & PCB_VSX) {
	#define LFP(n)   __asm ("lxvw4x " #n ", 0,%0" \
			:: "b"(&pcb->pcb_fpu.fpr[n]));
		LFP(0);		LFP(1);		LFP(2);		LFP(3);
		LFP(4);		LFP(5);		LFP(6);		LFP(7);
		LFP(8);		LFP(9);		LFP(10);	LFP(11);
		LFP(12);	LFP(13);	LFP(14);	LFP(15);
		LFP(16);	LFP(17);	LFP(18);	LFP(19);
		LFP(20);	LFP(21);	LFP(22);	LFP(23);
		LFP(24);	LFP(25);	LFP(26);	LFP(27);
		LFP(28);	LFP(29);	LFP(30);	LFP(31);
	#undef LFP
	} else {
	#define LFP(n)   __asm ("lfd " #n ", 0(%0)" \
			:: "b"(&pcb->pcb_fpu.fpr[n].fpr));
		LFP(0);		LFP(1);		LFP(2);		LFP(3);
		LFP(4);		LFP(5);		LFP(6);		LFP(7);
		LFP(8);		LFP(9);		LFP(10);	LFP(11);
		LFP(12);	LFP(13);	LFP(14);	LFP(15);
		LFP(16);	LFP(17);	LFP(18);	LFP(19);
		LFP(20);	LFP(21);	LFP(22);	LFP(23);
		LFP(24);	LFP(25);	LFP(26);	LFP(27);
		LFP(28);	LFP(29);	LFP(30);	LFP(31);
	#undef LFP
	}

	isync();
	mtmsr(msr);
}

void
save_fpu(struct thread *td)
{
	struct	pcb *pcb;

	pcb = td->td_pcb;

	save_fpu_int(td);

	/*
	 * Clear the current fp thread and pcb's CPU id
	 * XXX should this be left clear to allow lazy save/restore ?
	 */
	pcb->pcb_fpcpu = INT_MAX;
	PCPU_SET(fputhread, NULL);
}

/*
 * Save fpu state without dropping ownership.  This will only save state if
 * the current fpu thread is `td'.
 */
void
save_fpu_nodrop(struct thread *td)
{

	if (td == PCPU_GET(fputhread))
		save_fpu_int(td);
}

/*
 * Clear Floating-Point Status and Control Register
 */
void
cleanup_fpscr(void)
{
	register_t msr;

	msr = mfmsr();
	mtmsr(msr | PSL_FP);
	mtfsf(0);

	isync();
	mtmsr(msr);
}

/*
 * Get the current fp exception
 */
u_int
get_fpu_exception(struct thread *td)
{
	register_t msr;
	u_int ucode;
	register_t reg;

	critical_enter();

	msr = mfmsr();
	mtmsr(msr | PSL_FP);

	reg = mffs();

	isync();
	mtmsr(msr);

	critical_exit();

	if (reg & FPSCR_ZX)
		ucode = FPE_FLTDIV;
	else if (reg & FPSCR_OX)
		ucode = FPE_FLTOVF;
	else if (reg & FPSCR_UX)
		ucode = FPE_FLTUND;
	else if (reg & FPSCR_XX)
		ucode = FPE_FLTRES;
	else
		ucode = FPE_FLTINV;

	return ucode;
}

void
enable_fpu_kern(void)
{
	register_t msr;

	msr = mfmsr() | PSL_FP;

	if (cpu_features & PPC_FEATURE_HAS_VSX)
		msr |= PSL_VSX;

	mtmsr(msr);
}

void
disable_fpu(struct thread *td)
{
	register_t msr;
	struct pcb *pcb;
	struct trapframe *tf;

	pcb = td->td_pcb;
	tf = trapframe(td);

	/* Disable FPU in kernel (if enabled) */
	msr = mfmsr() & ~(PSL_FP | PSL_VSX);
	isync();
	mtmsr(msr);

	/*
	 * Disable FPU in userspace. It will be re-enabled when
	 * an FP or VSX instruction is executed.
	 */
	tf->srr1 &= ~(PSL_FP | PSL_VSX);
	pcb->pcb_flags &= ~(PCB_FPU | PCB_VSX);
}

#ifndef __SPE__
/*
 * XXX: Implement fpu_kern_alloc_ctx/fpu_kern_free_ctx once fpu_kern_enter and
 * fpu_kern_leave can handle !FPU_KERN_NOCTX.
 */
struct fpu_kern_ctx {
#define	FPU_KERN_CTX_DUMMY	0x01	/* avoided save for the kern thread */
#define	FPU_KERN_CTX_INUSE	0x02
	uint32_t	 flags;
};

void
fpu_kern_enter(struct thread *td, struct fpu_kern_ctx *ctx, u_int flags)
{
	struct pcb *pcb;

	pcb = td->td_pcb;

	KASSERT((flags & FPU_KERN_NOCTX) != 0 || ctx != NULL,
	    ("ctx is required when !FPU_KERN_NOCTX"));
	KASSERT(ctx == NULL || (ctx->flags & FPU_KERN_CTX_INUSE) == 0,
	    ("using inuse ctx"));
	KASSERT((pcb->pcb_flags & PCB_KERN_FPU_NOSAVE) == 0,
	    ("recursive fpu_kern_enter while in PCB_KERN_FPU_NOSAVE state"));

	if ((flags & FPU_KERN_NOCTX) != 0) {
		critical_enter();

		if (pcb->pcb_flags & PCB_FPU) {
			save_fpu(td);
			pcb->pcb_flags |= PCB_FPREGS;
		}
		enable_fpu_kern();

		if (pcb->pcb_flags & PCB_VEC) {
			save_vec(td);
			pcb->pcb_flags |= PCB_VECREGS;
		}
		enable_vec_kern();

		pcb->pcb_flags |= PCB_KERN_FPU | PCB_KERN_FPU_NOSAVE;
		return;
	}

	KASSERT(0, ("fpu_kern_enter with !FPU_KERN_NOCTX not implemented!"));
}

int
fpu_kern_leave(struct thread *td, struct fpu_kern_ctx *ctx)
{
	struct pcb *pcb;

	pcb = td->td_pcb;

	if ((pcb->pcb_flags & PCB_KERN_FPU_NOSAVE) != 0) {
		KASSERT(ctx == NULL, ("non-null ctx after FPU_KERN_NOCTX"));
		KASSERT(PCPU_GET(fpcurthread) == NULL,
		    ("non-NULL fpcurthread for PCB_FP_NOSAVE"));
		CRITICAL_ASSERT(td);

		/* Disable FPU, VMX, and VSX */
		disable_fpu(td);
		disable_vec(td);

		pcb->pcb_flags &= ~PCB_KERN_FPU_NOSAVE;

		critical_exit();
	} else {
		KASSERT(0, ("fpu_kern_leave with !FPU_KERN_NOCTX not implemented!"));
	}

	pcb->pcb_flags &= ~PCB_KERN_FPU;

	return 0;
}

int
is_fpu_kern_thread(u_int flags __unused)
{
	struct pcb *curpcb;

	if ((curthread->td_pflags & TDP_KTHREAD) == 0)
		return (0);
	curpcb = curthread->td_pcb;
	return ((curpcb->pcb_flags & PCB_KERN_FPU) != 0);
}

#endif /* !__SPE__ */
