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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)npx.c	7.2 (Berkeley) 5/12/91
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_debug_npx.h"
#include "opt_isa.h"

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
#ifdef NPX_DEBUG
#include <sys/syslog.h>
#endif
#include <sys/signalvar.h>
#include <sys/user.h>

#include <machine/cputypes.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/psl.h>
#include <machine/resource.h>
#include <machine/specialreg.h>
#include <machine/segments.h>
#include <machine/ucontext.h>

#include <amd64/isa/intr_machdep.h>
#ifdef DEV_ISA
#include <isa/isavar.h>
#endif

/*
 * 387 and 287 Numeric Coprocessor Extension (NPX) Driver.
 */

#if defined(__GNUC__) && !defined(lint)

#define	fldcw(addr)		__asm("fldcw %0" : : "m" (*(addr)))
#define	fnclex()		__asm("fnclex")
#define	fninit()		__asm("fninit")
#define	fnstcw(addr)		__asm __volatile("fnstcw %0" : "=m" (*(addr)))
#define	fnstsw(addr)		__asm __volatile("fnstsw %0" : "=m" (*(addr)))
#define	fxrstor(addr)		__asm("fxrstor %0" : : "m" (*(addr)))
#define	fxsave(addr)		__asm __volatile("fxsave %0" : "=m" (*(addr)))
#define	start_emulating()	__asm("smsw %%ax; orb %0,%%al; lmsw %%ax" \
				      : : "n" (CR0_TS) : "ax")
#define	stop_emulating()	__asm("clts")

#else	/* not __GNUC__ */

void	fldcw(caddr_t addr);
void	fnclex(void);
void	fninit(void);
void	fnstcw(caddr_t addr);
void	fnstsw(caddr_t addr);
void	fxsave(caddr_t addr);
void	fxrstor(caddr_t addr);
void	start_emulating(void);
void	stop_emulating(void);

#endif	/* __GNUC__ */

#define GET_FPU_CW(thread) ((thread)->td_pcb->pcb_save.sv_env.en_cw)
#define GET_FPU_SW(thread) ((thread)->td_pcb->pcb_save.sv_env.en_sw)

typedef u_char bool_t;

static	int	npx_attach(device_t dev);
static	void	npx_identify(driver_t *driver, device_t parent);
static	int	npx_probe(device_t dev);

int	hw_float = 1;
SYSCTL_INT(_hw,HW_FLOATINGPT, floatingpoint,
	CTLFLAG_RD, &hw_float, 0, 
	"Floatingpoint instructions executed in hardware");

static	struct savefpu		npx_cleanstate;
static	bool_t			npx_cleanstate_ready;

/*
 * Identify routine.  Create a connection point on our parent for probing.
 */
static void
npx_identify(driver, parent)
	driver_t *driver;
	device_t parent;
{
	device_t child;

	child = BUS_ADD_CHILD(parent, 0, "npx", 0);
	if (child == NULL)
		panic("npx_identify");
}

/*
 * Probe routine.  Initialize cr0 to give correct behaviour for [f]wait
 * whether the device exists or not (XXX should be elsewhere).
 * Modify device struct if npx doesn't need to use interrupts.
 * Return 0 if device exists.
 */
static int
npx_probe(dev)
	device_t dev;
{

	/*
	 * Partially reset the coprocessor, if any.  Some BIOS's don't reset
	 * it after a warm boot.
	 */
	outb(0xf1, 0);		/* full reset on some systems, NOP on others */
	outb(0xf0, 0);		/* clear BUSY# latch */
	/*
	 * Prepare to trap all ESC (i.e., NPX) instructions and all WAIT
	 * instructions.  We must set the CR0_MP bit and use the CR0_TS
	 * bit to control the trap, because setting the CR0_EM bit does
	 * not cause WAIT instructions to trap.  It's important to trap
	 * WAIT instructions - otherwise the "wait" variants of no-wait
	 * control instructions would degenerate to the "no-wait" variants
	 * after FP context switches but work correctly otherwise.  It's
	 * particularly important to trap WAITs when there is no NPX -
	 * otherwise the "wait" variants would always degenerate.
	 *
	 * Try setting CR0_NE to get correct error reporting on 486DX's.
	 * Setting it should fail or do nothing on lesser processors.
	 */
	load_cr0(rcr0() | CR0_MP | CR0_NE);
	/*
	 * But don't trap while we're probing.
	 */
	stop_emulating();
	/*
	 * Finish resetting the coprocessor.
	 */
	fninit();

	device_set_desc(dev, "math processor");

	return (0);
}

/*
 * Attach routine - announce which it is, and wire into system
 */
static int
npx_attach(dev)
	device_t dev;
{
	register_t s;

	npxinit(__INITIAL_NPXCW__);

	if (npx_cleanstate_ready == 0) {
		s = intr_disable();
		stop_emulating();
		fxsave(&npx_cleanstate);
		start_emulating();
		npx_cleanstate_ready = 1;
		intr_restore(s);
	}
	return (0);		/* XXX unused */
}

/*
 * Initialize floating point unit.
 */
void
npxinit(control)
	u_short control;
{
	static struct savefpu dummy;
	register_t savecrit;

	/*
	 * fninit has the same h/w bugs as fnsave.  Use the detoxified
	 * fnsave to throw away any junk in the fpu.  npxsave() initializes
	 * the fpu and sets fpcurthread = NULL as important side effects.
	 */
	savecrit = intr_disable();
	npxsave(&dummy);
	stop_emulating();
	/* XXX npxsave() doesn't actually initialize the fpu in the SSE case. */
	fninit();
	fldcw(&control);
	start_emulating();
	intr_restore(savecrit);
}

/*
 * Free coprocessor (if we have it).
 */
void
npxexit(td)
	struct thread *td;
{
#ifdef NPX_DEBUG
	u_int	masked_exceptions;
#endif
	register_t savecrit;

	savecrit = intr_disable();
	if (curthread == PCPU_GET(fpcurthread))
		npxsave(&PCPU_GET(curpcb)->pcb_save);
	intr_restore(savecrit);
#ifdef NPX_DEBUG
	masked_exceptions = GET_FPU_CW(td) & GET_FPU_SW(td) & 0x7f;
	/*
	 * Log exceptions that would have trapped with the old
	 * control word (overflow, divide by 0, and invalid operand).
	 */
	if (masked_exceptions & 0x0d)
		log(LOG_ERR,
"pid %d (%s) exited with masked floating point exceptions 0x%02x\n",
		    td->td_proc->p_pid, td->td_proc->p_comm,
		    masked_exceptions);
#endif
}

int
npxformat()
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
npxtrap()
{
	register_t savecrit;
	u_short control, status;

	savecrit = intr_disable();

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
	intr_restore(savecrit);
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

int
npxdna()
{
	struct pcb *pcb;
	register_t s;
	u_short control;

	if (PCPU_GET(fpcurthread) == curthread) {
		printf("npxdna: fpcurthread == curthread %d times\n",
		    ++err_count);
		stop_emulating();
		return (1);
	}
	if (PCPU_GET(fpcurthread) != NULL) {
		printf("npxdna: fpcurthread = %p (%d), curthread = %p (%d)\n",
		       PCPU_GET(fpcurthread),
		       PCPU_GET(fpcurthread)->td_proc->p_pid,
		       curthread, curthread->td_proc->p_pid);
		panic("npxdna");
	}
	s = intr_disable();
	stop_emulating();
	/*
	 * Record new context early in case frstor causes an IRQ13.
	 */
	PCPU_SET(fpcurthread, curthread);
	pcb = PCPU_GET(curpcb);

	if ((pcb->pcb_flags & PCB_NPXINITDONE) == 0) {
		/*
		 * This is the first time this thread has used the FPU or
		 * the PCB doesn't contain a clean FPU state.  Explicitly
		 * initialize the FPU and load the default control word.
		 */
		fninit();
		control = __INITIAL_NPXCW__;
		fldcw(&control);
		pcb->pcb_flags |= PCB_NPXINITDONE;
	} else {
		/*
		 * The following frstor may cause a trap when the state
		 * being restored has a pending error.  The error will
		 * appear to have been triggered by the current (npx) user
		 * instruction even when that instruction is a no-wait
		 * instruction that should not trigger an error (e.g.,
		 * instructions are broken the same as frstor, so our
		 * treatment does not amplify the breakage.
		 */
		fxrstor(&pcb->pcb_save);
	}
	intr_restore(s);

	return (1);
}

/*
 * Wrapper for fnsave instruction, partly to handle hardware bugs.  When npx
 * exceptions are reported via IRQ13, spurious IRQ13's may be triggered by
 * no-wait npx instructions.  See the Intel application note AP-578 for
 * details.  This doesn't cause any additional complications here.  IRQ13's
 * are inherently asynchronous unless the CPU is frozen to deliver them --
 * one that started in userland may be delivered many instructions later,
 * after the process has entered the kernel.  It may even be delivered after
 * the fnsave here completes.  A spurious IRQ13 for the fnsave is handled in
 * the same way as a very-late-arriving non-spurious IRQ13 from user mode:
 * it is normally ignored at first because we set fpcurthread to NULL; it is
 * normally retriggered in npxdna() after return to user mode.
 *
 * npxsave() must be called with interrupts disabled, so that it clears
 * fpcurthread atomically with saving the state.  We require callers to do the
 * disabling, since most callers need to disable interrupts anyway to call
 * npxsave() atomically with checking fpcurthread.
 *
 * A previous version of npxsave() went to great lengths to excecute fnsave
 * with interrupts enabled in case executing it froze the CPU.  This case
 * can't happen, at least for Intel CPU/NPX's.  Spurious IRQ13's don't imply
 * spurious freezes.
 */
void
npxsave(addr)
	struct savefpu *addr;
{

	stop_emulating();
	fxsave(addr);

	start_emulating();
	PCPU_SET(fpcurthread, NULL);
}

/*
 * This should be called with interrupts disabled and only when the owning
 * FPU thread is non-null.
 */
void
npxdrop()
{
	struct thread *td;

	td = PCPU_GET(fpcurthread);
	PCPU_SET(fpcurthread, NULL);
	td->td_pcb->pcb_flags &= ~PCB_NPXINITDONE;
	start_emulating();
}

/*
 * Get the state of the FPU without dropping ownership (if possible).
 * It returns the FPU ownership status.
 */
int
npxgetregs(td, addr)
	struct thread *td;
	struct savefpu *addr;
{
	register_t s;

	if ((td->td_pcb->pcb_flags & PCB_NPXINITDONE) == 0) {
		if (npx_cleanstate_ready)
			bcopy(&npx_cleanstate, addr, sizeof(npx_cleanstate));
		else
			bzero(addr, sizeof(*addr));
		return (_MC_FPOWNED_NONE);
	}
	s = intr_disable();
	if (td == PCPU_GET(fpcurthread)) {
		fxsave(addr);
		intr_restore(s);
		return (_MC_FPOWNED_FPU);
	} else {
		intr_restore(s);
		bcopy(&td->td_pcb->pcb_save, addr, sizeof(*addr));
		return (_MC_FPOWNED_PCB);
	}
}

/*
 * Set the state of the FPU.
 */
void
npxsetregs(td, addr)
	struct thread *td;
	struct savefpu *addr;
{
	register_t s;

	s = intr_disable();
	if (td == PCPU_GET(fpcurthread)) {
		fxrstor(addr);
		intr_restore(s);
	} else {
		intr_restore(s);
		bcopy(addr, &td->td_pcb->pcb_save, sizeof(*addr));
	}
	curthread->td_pcb->pcb_flags |= PCB_NPXINITDONE;
}

static device_method_t npx_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	npx_identify),
	DEVMETHOD(device_probe,		npx_probe),
	DEVMETHOD(device_attach,	npx_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	
	{ 0, 0 }
};

static driver_t npx_driver = {
	"npx",
	npx_methods,
	1,			/* no softc */
};

static devclass_t npx_devclass;

/*
 * We prefer to attach to the root nexus so that the usual case (exception 16)
 * doesn't describe the processor as being `on isa'.
 */
DRIVER_MODULE(npx, nexus, npx_driver, npx_devclass, 0, 0);

#ifdef DEV_ISA
/*
 * This sucks up the legacy ISA support assignments from PNPBIOS/ACPI.
 */
static struct isa_pnp_id npxisa_ids[] = {
	{ 0x040cd041, "Legacy ISA coprocessor support" }, /* PNP0C04 */
	{ 0 }
};

static int
npxisa_probe(device_t dev)
{
	int result;
	if ((result = ISA_PNP_PROBE(device_get_parent(dev), dev, npxisa_ids)) <= 0) {
		device_quiet(dev);
	}
	return(result);
}

static int
npxisa_attach(device_t dev)
{
	return (0);
}

static device_method_t npxisa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		npxisa_probe),
	DEVMETHOD(device_attach,	npxisa_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	
	{ 0, 0 }
};

static driver_t npxisa_driver = {
	"npxisa",
	npxisa_methods,
	1,			/* no softc */
};

static devclass_t npxisa_devclass;

DRIVER_MODULE(npxisa, isa, npxisa_driver, npxisa_devclass, 0, 0);
DRIVER_MODULE(npxisa, acpi, npxisa_driver, npxisa_devclass, 0, 0);
#endif /* DEV_ISA */
