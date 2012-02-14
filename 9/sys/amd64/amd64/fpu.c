/*-
 * Copyright (c) 1990 William Jolitz.
 * Copyright (c) 1991 The Regents of the University of California.
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
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)npx.c	7.2 (Berkeley) 5/12/91
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/signalvar.h>

#include <machine/cputypes.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/psl.h>
#include <machine/resource.h>
#include <machine/specialreg.h>
#include <machine/segments.h>
#include <machine/ucontext.h>

/*
 * Floating point support.
 */

#if defined(__GNUCLIKE_ASM) && !defined(lint)

#define	fldcw(cw)		__asm __volatile("fldcw %0" : : "m" (cw))
#define	fnclex()		__asm __volatile("fnclex")
#define	fninit()		__asm __volatile("fninit")
#define	fnstcw(addr)		__asm __volatile("fnstcw %0" : "=m" (*(addr)))
#define	fnstsw(addr)		__asm __volatile("fnstsw %0" : "=am" (*(addr)))
#define	fxrstor(addr)		__asm __volatile("fxrstor %0" : : "m" (*(addr)))
#define	fxsave(addr)		__asm __volatile("fxsave %0" : "=m" (*(addr)))
#define	ldmxcsr(csr)		__asm __volatile("ldmxcsr %0" : : "m" (csr))
#define	start_emulating()	__asm __volatile( \
				    "smsw %%ax; orb %0,%%al; lmsw %%ax" \
				    : : "n" (CR0_TS) : "ax")
#define	stop_emulating()	__asm __volatile("clts")

#else	/* !(__GNUCLIKE_ASM && !lint) */

void	fldcw(u_short cw);
void	fnclex(void);
void	fninit(void);
void	fnstcw(caddr_t addr);
void	fnstsw(caddr_t addr);
void	fxsave(caddr_t addr);
void	fxrstor(caddr_t addr);
void	ldmxcsr(u_int csr);
void	start_emulating(void);
void	stop_emulating(void);

#endif	/* __GNUCLIKE_ASM && !lint */

#define GET_FPU_CW(thread) ((thread)->td_pcb->pcb_save->sv_env.en_cw)
#define GET_FPU_SW(thread) ((thread)->td_pcb->pcb_save->sv_env.en_sw)

typedef u_char bool_t;

static	void	fpu_clean_state(void);

SYSCTL_INT(_hw, HW_FLOATINGPT, floatingpoint, CTLFLAG_RD,
    NULL, 1, "Floating point instructions executed in hardware");

static	struct savefpu		fpu_initialstate;

/*
 * Initialize the floating point unit.  On the boot CPU we generate a
 * clean state that is used to initialize the floating point unit when
 * it is first used by a process.
 */
void
fpuinit(void)
{
	register_t saveintr;
	u_int mxcsr;
	u_short control;

	/*
	 * It is too early for critical_enter() to work on AP.
	 */
	saveintr = intr_disable();
	stop_emulating();
	fninit();
	control = __INITIAL_FPUCW__;
	fldcw(control);
	mxcsr = __INITIAL_MXCSR__;
	ldmxcsr(mxcsr);
	if (PCPU_GET(cpuid) == 0) {
		fxsave(&fpu_initialstate);
		if (fpu_initialstate.sv_env.en_mxcsr_mask)
			cpu_mxcsr_mask = fpu_initialstate.sv_env.en_mxcsr_mask;
		else
			cpu_mxcsr_mask = 0xFFBF;
		bzero(fpu_initialstate.sv_fp, sizeof(fpu_initialstate.sv_fp));
		bzero(fpu_initialstate.sv_xmm, sizeof(fpu_initialstate.sv_xmm));
	}
	start_emulating();
	intr_restore(saveintr);
}

/*
 * Free coprocessor (if we have it).
 */
void
fpuexit(struct thread *td)
{

	critical_enter();
	if (curthread == PCPU_GET(fpcurthread)) {
		stop_emulating();
		fxsave(PCPU_GET(curpcb)->pcb_save);
		start_emulating();
		PCPU_SET(fpcurthread, 0);
	}
	critical_exit();
}

int
fpuformat()
{

	return (_MC_FPFMT_XMM);
}

/* 
 * The following mechanism is used to ensure that the FPE_... value
 * that is passed as a trapcode to the signal handler of the user
 * process does not have more than one bit set.
 * 
 * Multiple bits may be set if the user process modifies the control
 * word while a status word bit is already set.  While this is a sign
 * of bad coding, we have no choise than to narrow them down to one
 * bit, since we must not send a trapcode that is not exactly one of
 * the FPE_ macros.
 *
 * The mechanism has a static table with 127 entries.  Each combination
 * of the 7 FPU status word exception bits directly translates to a
 * position in this table, where a single FPE_... value is stored.
 * This FPE_... value stored there is considered the "most important"
 * of the exception bits and will be sent as the signal code.  The
 * precedence of the bits is based upon Intel Document "Numerical
 * Applications", Chapter "Special Computational Situations".
 *
 * The macro to choose one of these values does these steps: 1) Throw
 * away status word bits that cannot be masked.  2) Throw away the bits
 * currently masked in the control word, assuming the user isn't
 * interested in them anymore.  3) Reinsert status word bit 7 (stack
 * fault) if it is set, which cannot be masked but must be presered.
 * 4) Use the remaining bits to point into the trapcode table.
 *
 * The 6 maskable bits in order of their preference, as stated in the
 * above referenced Intel manual:
 * 1  Invalid operation (FP_X_INV)
 * 1a   Stack underflow
 * 1b   Stack overflow
 * 1c   Operand of unsupported format
 * 1d   SNaN operand.
 * 2  QNaN operand (not an exception, irrelavant here)
 * 3  Any other invalid-operation not mentioned above or zero divide
 *      (FP_X_INV, FP_X_DZ)
 * 4  Denormal operand (FP_X_DNML)
 * 5  Numeric over/underflow (FP_X_OFL, FP_X_UFL)
 * 6  Inexact result (FP_X_IMP) 
 */
static char fpetable[128] = {
	0,
	FPE_FLTINV,	/*  1 - INV */
	FPE_FLTUND,	/*  2 - DNML */
	FPE_FLTINV,	/*  3 - INV | DNML */
	FPE_FLTDIV,	/*  4 - DZ */
	FPE_FLTINV,	/*  5 - INV | DZ */
	FPE_FLTDIV,	/*  6 - DNML | DZ */
	FPE_FLTINV,	/*  7 - INV | DNML | DZ */
	FPE_FLTOVF,	/*  8 - OFL */
	FPE_FLTINV,	/*  9 - INV | OFL */
	FPE_FLTUND,	/*  A - DNML | OFL */
	FPE_FLTINV,	/*  B - INV | DNML | OFL */
	FPE_FLTDIV,	/*  C - DZ | OFL */
	FPE_FLTINV,	/*  D - INV | DZ | OFL */
	FPE_FLTDIV,	/*  E - DNML | DZ | OFL */
	FPE_FLTINV,	/*  F - INV | DNML | DZ | OFL */
	FPE_FLTUND,	/* 10 - UFL */
	FPE_FLTINV,	/* 11 - INV | UFL */
	FPE_FLTUND,	/* 12 - DNML | UFL */
	FPE_FLTINV,	/* 13 - INV | DNML | UFL */
	FPE_FLTDIV,	/* 14 - DZ | UFL */
	FPE_FLTINV,	/* 15 - INV | DZ | UFL */
	FPE_FLTDIV,	/* 16 - DNML | DZ | UFL */
	FPE_FLTINV,	/* 17 - INV | DNML | DZ | UFL */
	FPE_FLTOVF,	/* 18 - OFL | UFL */
	FPE_FLTINV,	/* 19 - INV | OFL | UFL */
	FPE_FLTUND,	/* 1A - DNML | OFL | UFL */
	FPE_FLTINV,	/* 1B - INV | DNML | OFL | UFL */
	FPE_FLTDIV,	/* 1C - DZ | OFL | UFL */
	FPE_FLTINV,	/* 1D - INV | DZ | OFL | UFL */
	FPE_FLTDIV,	/* 1E - DNML | DZ | OFL | UFL */
	FPE_FLTINV,	/* 1F - INV | DNML | DZ | OFL | UFL */
	FPE_FLTRES,	/* 20 - IMP */
	FPE_FLTINV,	/* 21 - INV | IMP */
	FPE_FLTUND,	/* 22 - DNML | IMP */
	FPE_FLTINV,	/* 23 - INV | DNML | IMP */
	FPE_FLTDIV,	/* 24 - DZ | IMP */
	FPE_FLTINV,	/* 25 - INV | DZ | IMP */
	FPE_FLTDIV,	/* 26 - DNML | DZ | IMP */
	FPE_FLTINV,	/* 27 - INV | DNML | DZ | IMP */
	FPE_FLTOVF,	/* 28 - OFL | IMP */
	FPE_FLTINV,	/* 29 - INV | OFL | IMP */
	FPE_FLTUND,	/* 2A - DNML | OFL | IMP */
	FPE_FLTINV,	/* 2B - INV | DNML | OFL | IMP */
	FPE_FLTDIV,	/* 2C - DZ | OFL | IMP */
	FPE_FLTINV,	/* 2D - INV | DZ | OFL | IMP */
	FPE_FLTDIV,	/* 2E - DNML | DZ | OFL | IMP */
	FPE_FLTINV,	/* 2F - INV | DNML | DZ | OFL | IMP */
	FPE_FLTUND,	/* 30 - UFL | IMP */
	FPE_FLTINV,	/* 31 - INV | UFL | IMP */
	FPE_FLTUND,	/* 32 - DNML | UFL | IMP */
	FPE_FLTINV,	/* 33 - INV | DNML | UFL | IMP */
	FPE_FLTDIV,	/* 34 - DZ | UFL | IMP */
	FPE_FLTINV,	/* 35 - INV | DZ | UFL | IMP */
	FPE_FLTDIV,	/* 36 - DNML | DZ | UFL | IMP */
	FPE_FLTINV,	/* 37 - INV | DNML | DZ | UFL | IMP */
	FPE_FLTOVF,	/* 38 - OFL | UFL | IMP */
	FPE_FLTINV,	/* 39 - INV | OFL | UFL | IMP */
	FPE_FLTUND,	/* 3A - DNML | OFL | UFL | IMP */
	FPE_FLTINV,	/* 3B - INV | DNML | OFL | UFL | IMP */
	FPE_FLTDIV,	/* 3C - DZ | OFL | UFL | IMP */
	FPE_FLTINV,	/* 3D - INV | DZ | OFL | UFL | IMP */
	FPE_FLTDIV,	/* 3E - DNML | DZ | OFL | UFL | IMP */
	FPE_FLTINV,	/* 3F - INV | DNML | DZ | OFL | UFL | IMP */
	FPE_FLTSUB,	/* 40 - STK */
	FPE_FLTSUB,	/* 41 - INV | STK */
	FPE_FLTUND,	/* 42 - DNML | STK */
	FPE_FLTSUB,	/* 43 - INV | DNML | STK */
	FPE_FLTDIV,	/* 44 - DZ | STK */
	FPE_FLTSUB,	/* 45 - INV | DZ | STK */
	FPE_FLTDIV,	/* 46 - DNML | DZ | STK */
	FPE_FLTSUB,	/* 47 - INV | DNML | DZ | STK */
	FPE_FLTOVF,	/* 48 - OFL | STK */
	FPE_FLTSUB,	/* 49 - INV | OFL | STK */
	FPE_FLTUND,	/* 4A - DNML | OFL | STK */
	FPE_FLTSUB,	/* 4B - INV | DNML | OFL | STK */
	FPE_FLTDIV,	/* 4C - DZ | OFL | STK */
	FPE_FLTSUB,	/* 4D - INV | DZ | OFL | STK */
	FPE_FLTDIV,	/* 4E - DNML | DZ | OFL | STK */
	FPE_FLTSUB,	/* 4F - INV | DNML | DZ | OFL | STK */
	FPE_FLTUND,	/* 50 - UFL | STK */
	FPE_FLTSUB,	/* 51 - INV | UFL | STK */
	FPE_FLTUND,	/* 52 - DNML | UFL | STK */
	FPE_FLTSUB,	/* 53 - INV | DNML | UFL | STK */
	FPE_FLTDIV,	/* 54 - DZ | UFL | STK */
	FPE_FLTSUB,	/* 55 - INV | DZ | UFL | STK */
	FPE_FLTDIV,	/* 56 - DNML | DZ | UFL | STK */
	FPE_FLTSUB,	/* 57 - INV | DNML | DZ | UFL | STK */
	FPE_FLTOVF,	/* 58 - OFL | UFL | STK */
	FPE_FLTSUB,	/* 59 - INV | OFL | UFL | STK */
	FPE_FLTUND,	/* 5A - DNML | OFL | UFL | STK */
	FPE_FLTSUB,	/* 5B - INV | DNML | OFL | UFL | STK */
	FPE_FLTDIV,	/* 5C - DZ | OFL | UFL | STK */
	FPE_FLTSUB,	/* 5D - INV | DZ | OFL | UFL | STK */
	FPE_FLTDIV,	/* 5E - DNML | DZ | OFL | UFL | STK */
	FPE_FLTSUB,	/* 5F - INV | DNML | DZ | OFL | UFL | STK */
	FPE_FLTRES,	/* 60 - IMP | STK */
	FPE_FLTSUB,	/* 61 - INV | IMP | STK */
	FPE_FLTUND,	/* 62 - DNML | IMP | STK */
	FPE_FLTSUB,	/* 63 - INV | DNML | IMP | STK */
	FPE_FLTDIV,	/* 64 - DZ | IMP | STK */
	FPE_FLTSUB,	/* 65 - INV | DZ | IMP | STK */
	FPE_FLTDIV,	/* 66 - DNML | DZ | IMP | STK */
	FPE_FLTSUB,	/* 67 - INV | DNML | DZ | IMP | STK */
	FPE_FLTOVF,	/* 68 - OFL | IMP | STK */
	FPE_FLTSUB,	/* 69 - INV | OFL | IMP | STK */
	FPE_FLTUND,	/* 6A - DNML | OFL | IMP | STK */
	FPE_FLTSUB,	/* 6B - INV | DNML | OFL | IMP | STK */
	FPE_FLTDIV,	/* 6C - DZ | OFL | IMP | STK */
	FPE_FLTSUB,	/* 6D - INV | DZ | OFL | IMP | STK */
	FPE_FLTDIV,	/* 6E - DNML | DZ | OFL | IMP | STK */
	FPE_FLTSUB,	/* 6F - INV | DNML | DZ | OFL | IMP | STK */
	FPE_FLTUND,	/* 70 - UFL | IMP | STK */
	FPE_FLTSUB,	/* 71 - INV | UFL | IMP | STK */
	FPE_FLTUND,	/* 72 - DNML | UFL | IMP | STK */
	FPE_FLTSUB,	/* 73 - INV | DNML | UFL | IMP | STK */
	FPE_FLTDIV,	/* 74 - DZ | UFL | IMP | STK */
	FPE_FLTSUB,	/* 75 - INV | DZ | UFL | IMP | STK */
	FPE_FLTDIV,	/* 76 - DNML | DZ | UFL | IMP | STK */
	FPE_FLTSUB,	/* 77 - INV | DNML | DZ | UFL | IMP | STK */
	FPE_FLTOVF,	/* 78 - OFL | UFL | IMP | STK */
	FPE_FLTSUB,	/* 79 - INV | OFL | UFL | IMP | STK */
	FPE_FLTUND,	/* 7A - DNML | OFL | UFL | IMP | STK */
	FPE_FLTSUB,	/* 7B - INV | DNML | OFL | UFL | IMP | STK */
	FPE_FLTDIV,	/* 7C - DZ | OFL | UFL | IMP | STK */
	FPE_FLTSUB,	/* 7D - INV | DZ | OFL | UFL | IMP | STK */
	FPE_FLTDIV,	/* 7E - DNML | DZ | OFL | UFL | IMP | STK */
	FPE_FLTSUB,	/* 7F - INV | DNML | DZ | OFL | UFL | IMP | STK */
};

/*
 * Preserve the FP status word, clear FP exceptions, then generate a SIGFPE.
 *
 * Clearing exceptions is necessary mainly to avoid IRQ13 bugs.  We now
 * depend on longjmp() restoring a usable state.  Restoring the state
 * or examining it might fail if we didn't clear exceptions.
 *
 * The error code chosen will be one of the FPE_... macros. It will be
 * sent as the second argument to old BSD-style signal handlers and as
 * "siginfo_t->si_code" (second argument) to SA_SIGINFO signal handlers.
 *
 * XXX the FP state is not preserved across signal handlers.  So signal
 * handlers cannot afford to do FP unless they preserve the state or
 * longjmp() out.  Both preserving the state and longjmp()ing may be
 * destroyed by IRQ13 bugs.  Clearing FP exceptions is not an acceptable
 * solution for signals other than SIGFPE.
 */
int
fputrap()
{
	u_short control, status;

	critical_enter();

	/*
	 * Interrupt handling (for another interrupt) may have pushed the
	 * state to memory.  Fetch the relevant parts of the state from
	 * wherever they are.
	 */
	if (PCPU_GET(fpcurthread) != curthread) {
		control = GET_FPU_CW(curthread);
		status = GET_FPU_SW(curthread);
	} else {
		fnstcw(&control);
		fnstsw(&status);
	}

	if (PCPU_GET(fpcurthread) == curthread)
		fnclex();
	critical_exit();
	return (fpetable[status & ((~control & 0x3f) | 0x40)]);
}

/*
 * Implement device not available (DNA) exception
 *
 * It would be better to switch FP context here (if curthread != fpcurthread)
 * and not necessarily for every context switch, but it is too hard to
 * access foreign pcb's.
 */

static int err_count = 0;

void
fpudna(void)
{
	struct pcb *pcb;

	critical_enter();
	if (PCPU_GET(fpcurthread) == curthread) {
		printf("fpudna: fpcurthread == curthread %d times\n",
		    ++err_count);
		stop_emulating();
		critical_exit();
		return;
	}
	if (PCPU_GET(fpcurthread) != NULL) {
		printf("fpudna: fpcurthread = %p (%d), curthread = %p (%d)\n",
		       PCPU_GET(fpcurthread),
		       PCPU_GET(fpcurthread)->td_proc->p_pid,
		       curthread, curthread->td_proc->p_pid);
		panic("fpudna");
	}
	stop_emulating();
	/*
	 * Record new context early in case frstor causes a trap.
	 */
	PCPU_SET(fpcurthread, curthread);
	pcb = PCPU_GET(curpcb);

	fpu_clean_state();

	if ((pcb->pcb_flags & PCB_FPUINITDONE) == 0) {
		/*
		 * This is the first time this thread has used the FPU or
		 * the PCB doesn't contain a clean FPU state.  Explicitly
		 * load an initial state.
		 */
		fxrstor(&fpu_initialstate);
		if (pcb->pcb_initial_fpucw != __INITIAL_FPUCW__)
			fldcw(pcb->pcb_initial_fpucw);
		if (PCB_USER_FPU(pcb))
			set_pcb_flags(pcb,
			    PCB_FPUINITDONE | PCB_USERFPUINITDONE);
		else
			set_pcb_flags(pcb, PCB_FPUINITDONE);
	} else
		fxrstor(pcb->pcb_save);
	critical_exit();
}

void
fpudrop()
{
	struct thread *td;

	td = PCPU_GET(fpcurthread);
	KASSERT(td == curthread, ("fpudrop: fpcurthread != curthread"));
	CRITICAL_ASSERT(td);
	PCPU_SET(fpcurthread, NULL);
	clear_pcb_flags(td->td_pcb, PCB_FPUINITDONE);
	start_emulating();
}

/*
 * Get the user state of the FPU into pcb->pcb_user_save without
 * dropping ownership (if possible).  It returns the FPU ownership
 * status.
 */
int
fpugetregs(struct thread *td)
{
	struct pcb *pcb;

	pcb = td->td_pcb;
	if ((pcb->pcb_flags & PCB_USERFPUINITDONE) == 0) {
		bcopy(&fpu_initialstate, &pcb->pcb_user_save,
		    sizeof(fpu_initialstate));
		pcb->pcb_user_save.sv_env.en_cw = pcb->pcb_initial_fpucw;
		fpuuserinited(td);
		return (_MC_FPOWNED_PCB);
	}
	critical_enter();
	if (td == PCPU_GET(fpcurthread) && PCB_USER_FPU(pcb)) {
		fxsave(&pcb->pcb_user_save);
		critical_exit();
		return (_MC_FPOWNED_FPU);
	} else {
		critical_exit();
		return (_MC_FPOWNED_PCB);
	}
}

void
fpuuserinited(struct thread *td)
{
	struct pcb *pcb;

	pcb = td->td_pcb;
	if (PCB_USER_FPU(pcb))
		set_pcb_flags(pcb,
		    PCB_FPUINITDONE | PCB_USERFPUINITDONE);
	else
		set_pcb_flags(pcb, PCB_FPUINITDONE);
}

/*
 * Set the state of the FPU.
 */
void
fpusetregs(struct thread *td, struct savefpu *addr)
{
	struct pcb *pcb;

	pcb = td->td_pcb;
	critical_enter();
	if (td == PCPU_GET(fpcurthread) && PCB_USER_FPU(pcb)) {
		fxrstor(addr);
		critical_exit();
		set_pcb_flags(pcb, PCB_FPUINITDONE | PCB_USERFPUINITDONE);
	} else {
		critical_exit();
		bcopy(addr, &td->td_pcb->pcb_user_save, sizeof(*addr));
		fpuuserinited(td);
	}
}

/*
 * On AuthenticAMD processors, the fxrstor instruction does not restore
 * the x87's stored last instruction pointer, last data pointer, and last
 * opcode values, except in the rare case in which the exception summary
 * (ES) bit in the x87 status word is set to 1.
 *
 * In order to avoid leaking this information across processes, we clean
 * these values by performing a dummy load before executing fxrstor().
 */
static void
fpu_clean_state(void)
{
	static float dummy_variable = 0.0;
	u_short status;

	/*
	 * Clear the ES bit in the x87 status word if it is currently
	 * set, in order to avoid causing a fault in the upcoming load.
	 */
	fnstsw(&status);
	if (status & 0x80)
		fnclex();

	/*
	 * Load the dummy variable into the x87 stack.  This mangles
	 * the x87 stack, but we don't care since we're about to call
	 * fxrstor() anyway.
	 */
	__asm __volatile("ffree %%st(7); flds %0" : : "m" (dummy_variable));
}

/*
 * This really sucks.  We want the acpi version only, but it requires
 * the isa_if.h file in order to get the definitions.
 */
#include "opt_isa.h"
#ifdef DEV_ISA
#include <isa/isavar.h>
/*
 * This sucks up the legacy ISA support assignments from PNPBIOS/ACPI.
 */
static struct isa_pnp_id fpupnp_ids[] = {
	{ 0x040cd041, "Legacy ISA coprocessor support" }, /* PNP0C04 */
	{ 0 }
};

static int
fpupnp_probe(device_t dev)
{
	int result;

	result = ISA_PNP_PROBE(device_get_parent(dev), dev, fpupnp_ids);
	if (result <= 0)
		device_quiet(dev);
	return (result);
}

static int
fpupnp_attach(device_t dev)
{

	return (0);
}

static device_method_t fpupnp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fpupnp_probe),
	DEVMETHOD(device_attach,	fpupnp_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	
	{ 0, 0 }
};

static driver_t fpupnp_driver = {
	"fpupnp",
	fpupnp_methods,
	1,			/* no softc */
};

static devclass_t fpupnp_devclass;

DRIVER_MODULE(fpupnp, acpi, fpupnp_driver, fpupnp_devclass, 0, 0);
#endif	/* DEV_ISA */

int
fpu_kern_enter(struct thread *td, struct fpu_kern_ctx *ctx, u_int flags)
{
	struct pcb *pcb;

	pcb = td->td_pcb;
	KASSERT(!PCB_USER_FPU(pcb) || pcb->pcb_save == &pcb->pcb_user_save,
	    ("mangled pcb_save"));
	ctx->flags = 0;
	if ((pcb->pcb_flags & PCB_FPUINITDONE) != 0)
		ctx->flags |= FPU_KERN_CTX_FPUINITDONE;
	fpuexit(td);
	ctx->prev = pcb->pcb_save;
	pcb->pcb_save = &ctx->hwstate;
	set_pcb_flags(pcb, PCB_KERNFPU);
	clear_pcb_flags(pcb, PCB_FPUINITDONE);
	return (0);
}

int
fpu_kern_leave(struct thread *td, struct fpu_kern_ctx *ctx)
{
	struct pcb *pcb;

	pcb = td->td_pcb;
	critical_enter();
	if (curthread == PCPU_GET(fpcurthread))
		fpudrop();
	critical_exit();
	pcb->pcb_save = ctx->prev;
	if (pcb->pcb_save == &pcb->pcb_user_save) {
		if ((pcb->pcb_flags & PCB_USERFPUINITDONE) != 0) {
			set_pcb_flags(pcb, PCB_FPUINITDONE);
			clear_pcb_flags(pcb, PCB_KERNFPU);
		} else
			clear_pcb_flags(pcb, PCB_FPUINITDONE | PCB_KERNFPU);
	} else {
		if ((ctx->flags & FPU_KERN_CTX_FPUINITDONE) != 0)
			set_pcb_flags(pcb, PCB_FPUINITDONE);
		else
			clear_pcb_flags(pcb, PCB_FPUINITDONE);
		KASSERT(!PCB_USER_FPU(pcb), ("unpaired fpu_kern_leave"));
	}
	return (0);
}

int
fpu_kern_thread(u_int flags)
{
	struct pcb *pcb;

	pcb = PCPU_GET(curpcb);
	KASSERT((curthread->td_pflags & TDP_KTHREAD) != 0,
	    ("Only kthread may use fpu_kern_thread"));
	KASSERT(pcb->pcb_save == &pcb->pcb_user_save, ("mangled pcb_save"));
	KASSERT(PCB_USER_FPU(pcb), ("recursive call"));

	set_pcb_flags(pcb, PCB_KERNFPU);
	return (0);
}

int
is_fpu_kern_thread(u_int flags)
{

	if ((curthread->td_pflags & TDP_KTHREAD) == 0)
		return (0);
	return ((PCPU_GET(curpcb)->pcb_flags & PCB_KERNFPU) != 0);
}
