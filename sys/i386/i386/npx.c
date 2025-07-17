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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/cdefs.h>
#include "opt_cpu.h"
#include "opt_isa.h"
#include "opt_npx.h"

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
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <machine/bus.h>
#include <sys/rman.h>
#ifdef NPX_DEBUG
#include <sys/syslog.h>
#endif
#include <sys/signalvar.h>
#include <vm/uma.h>

#include <machine/asmacros.h>
#include <machine/cputypes.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/psl.h>
#include <machine/resource.h>
#include <machine/specialreg.h>
#include <machine/segments.h>
#include <machine/ucontext.h>
#include <x86/ifunc.h>

#include <machine/intr_machdep.h>

#ifdef DEV_ISA
#include <isa/isavar.h>
#endif

/*
 * 387 and 287 Numeric Coprocessor Extension (NPX) Driver.
 */

#define	fldcw(cw)		__asm __volatile("fldcw %0" : : "m" (cw))
#define	fnclex()		__asm __volatile("fnclex")
#define	fninit()		__asm __volatile("fninit")
#define	fnsave(addr)		__asm __volatile("fnsave %0" : "=m" (*(addr)))
#define	fnstcw(addr)		__asm __volatile("fnstcw %0" : "=m" (*(addr)))
#define	fnstsw(addr)		__asm __volatile("fnstsw %0" : "=am" (*(addr)))
#define	fp_divide_by_0()	__asm __volatile( \
				    "fldz; fld1; fdiv %st,%st(1); fnop")
#define	frstor(addr)		__asm __volatile("frstor %0" : : "m" (*(addr)))
#define	fxrstor(addr)		__asm __volatile("fxrstor %0" : : "m" (*(addr)))
#define	fxsave(addr)		__asm __volatile("fxsave %0" : "=m" (*(addr)))
#define	ldmxcsr(csr)		__asm __volatile("ldmxcsr %0" : : "m" (csr))
#define	stmxcsr(addr)		__asm __volatile("stmxcsr %0" : : "m" (*(addr)))

static __inline void
xrstor(char *addr, uint64_t mask)
{
	uint32_t low, hi;

	low = mask;
	hi = mask >> 32;
	__asm __volatile("xrstor %0" : : "m" (*addr), "a" (low), "d" (hi));
}

static __inline void
xsave(char *addr, uint64_t mask)
{
	uint32_t low, hi;

	low = mask;
	hi = mask >> 32;
	__asm __volatile("xsave %0" : "=m" (*addr) : "a" (low), "d" (hi) :
	    "memory");
}

static __inline void
xsaveopt(char *addr, uint64_t mask)
{
	uint32_t low, hi;

	low = mask;
	hi = mask >> 32;
	__asm __volatile("xsaveopt %0" : "=m" (*addr) : "a" (low), "d" (hi) :
	    "memory");
}

#define GET_FPU_CW(thread) \
	(cpu_fxsr ? \
		(thread)->td_pcb->pcb_save->sv_xmm.sv_env.en_cw : \
		(thread)->td_pcb->pcb_save->sv_87.sv_env.en_cw)
#define GET_FPU_SW(thread) \
	(cpu_fxsr ? \
		(thread)->td_pcb->pcb_save->sv_xmm.sv_env.en_sw : \
		(thread)->td_pcb->pcb_save->sv_87.sv_env.en_sw)
#define SET_FPU_CW(savefpu, value) do { \
	if (cpu_fxsr) \
		(savefpu)->sv_xmm.sv_env.en_cw = (value); \
	else \
		(savefpu)->sv_87.sv_env.en_cw = (value); \
} while (0)

CTASSERT(sizeof(union savefpu) == 512);
CTASSERT(sizeof(struct xstate_hdr) == 64);
CTASSERT(sizeof(struct savefpu_ymm) == 832);

/*
 * This requirement is to make it easier for asm code to calculate
 * offset of the fpu save area from the pcb address. FPU save area
 * must be 64-byte aligned.
 */
CTASSERT(sizeof(struct pcb) % XSAVE_AREA_ALIGN == 0);

/*
 * Ensure the copy of XCR0 saved in a core is contained in the padding
 * area.
 */
CTASSERT(X86_XSTATE_XCR0_OFFSET >= offsetof(struct savexmm, sv_pad) &&
    X86_XSTATE_XCR0_OFFSET + sizeof(uint64_t) <= sizeof(struct savexmm));

static	void	fpu_clean_state(void);

static	void	fpurstor(union savefpu *);

int	hw_float;

SYSCTL_INT(_hw, HW_FLOATINGPT, floatingpoint, CTLFLAG_RD,
    &hw_float, 0, "Floating point instructions executed in hardware");

int lazy_fpu_switch = 0;
SYSCTL_INT(_hw, OID_AUTO, lazy_fpu_switch, CTLFLAG_RWTUN | CTLFLAG_NOFETCH,
    &lazy_fpu_switch, 0,
    "Lazily load FPU context after context switch");

u_int cpu_fxsr;		/* SSE enabled */
int use_xsave;
uint64_t xsave_mask;
static	uma_zone_t fpu_save_area_zone;
static	union savefpu *npx_initialstate;

static struct xsave_area_elm_descr {
	u_int	offset;
	u_int	size;
} *xsave_area_desc;

static	volatile u_int		npx_traps_while_probing;

alias_for_inthand_t probetrap;
__asm("								\n\
	.text							\n\
	.p2align 2,0x90						\n\
	.type	" __XSTRING(CNAME(probetrap)) ",@function	\n\
" __XSTRING(CNAME(probetrap)) ":				\n\
	ss							\n\
	incl	" __XSTRING(CNAME(npx_traps_while_probing)) "	\n\
	fnclex							\n\
	iret							\n\
");

/*
 * Determine if an FPU is present and how to use it.
 */
static int
npx_probe(void)
{
	struct gate_descriptor save_idt_npxtrap;
	u_short control, status;

	/*
	 * Modern CPUs all have an FPU that uses the INT16 interface
	 * and provide a simple way to verify that, so handle the
	 * common case right away.
	 */
	if (cpu_feature & CPUID_FPU) {
		hw_float = 1;
		return (1);
	}

	save_idt_npxtrap = idt[IDT_MF];
	setidt(IDT_MF, probetrap, SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));

	/*
	 * Don't trap while we're probing.
	 */
	fpu_enable();

	/*
	 * Finish resetting the coprocessor, if any.  If there is an error
	 * pending, then we may get a bogus IRQ13, but npx_intr() will handle
	 * it OK.  Bogus halts have never been observed, but we enabled
	 * IRQ13 and cleared the BUSY# latch early to handle them anyway.
	 */
	fninit();

	/*
	 * Don't use fwait here because it might hang.
	 * Don't use fnop here because it usually hangs if there is no FPU.
	 */
	DELAY(1000);		/* wait for any IRQ13 */
#ifdef DIAGNOSTIC
	if (npx_traps_while_probing != 0)
		printf("fninit caused %u bogus npx trap(s)\n",
		       npx_traps_while_probing);
#endif
	/*
	 * Check for a status of mostly zero.
	 */
	status = 0x5a5a;
	fnstsw(&status);
	if ((status & 0xb8ff) == 0) {
		/*
		 * Good, now check for a proper control word.
		 */
		control = 0x5a5a;
		fnstcw(&control);
		if ((control & 0x1f3f) == 0x033f) {
			/*
			 * We have an npx, now divide by 0 to see if exception
			 * 16 works.
			 */
			control &= ~(1 << 2);	/* enable divide by 0 trap */
			fldcw(control);
			npx_traps_while_probing = 0;
			fp_divide_by_0();
			if (npx_traps_while_probing != 0) {
				/*
				 * Good, exception 16 works.
				 */
				hw_float = 1;
				goto cleanup;
			}
			printf(
	"FPU does not use exception 16 for error reporting\n");
			goto cleanup;
		}
	}

	/*
	 * Probe failed.  Floating point simply won't work.
	 * Notify user and disable FPU/MMX/SSE instruction execution.
	 */
	printf("WARNING: no FPU!\n");
	__asm __volatile("smsw %%ax; orb %0,%%al; lmsw %%ax" : :
	    "n" (CR0_EM | CR0_MP) : "ax");

cleanup:
	idt[IDT_MF] = save_idt_npxtrap;
	return (hw_float);
}

static void
fpusave_xsaveopt(union savefpu *addr)
{

	xsaveopt((char *)addr, xsave_mask);
}

static void
fpusave_xsave(union savefpu *addr)
{

	xsave((char *)addr, xsave_mask);
}

static void
fpusave_fxsave(union savefpu *addr)
{

	fxsave((char *)addr);
}

static void
fpusave_fnsave(union savefpu *addr)
{

	fnsave((char *)addr);
}

DEFINE_IFUNC(, void, fpusave, (union savefpu *))
{
	u_int cp[4];

	if (use_xsave) {
		cpuid_count(0xd, 0x1, cp);
		return ((cp[0] & CPUID_EXTSTATE_XSAVEOPT) != 0 ?
		    fpusave_xsaveopt : fpusave_xsave);
	}
	if (cpu_fxsr)
		return (fpusave_fxsave);
	return (fpusave_fnsave);
}

/*
 * Enable XSAVE if supported and allowed by user.
 * Calculate the xsave_mask.
 */
static void
npxinit_bsp1(void)
{
	u_int cp[4];
	uint64_t xsave_mask_user;

	TUNABLE_INT_FETCH("hw.lazy_fpu_switch", &lazy_fpu_switch);
	if (!use_xsave)
		return;
	cpuid_count(0xd, 0x0, cp);
	xsave_mask = XFEATURE_ENABLED_X87 | XFEATURE_ENABLED_SSE;
	if ((cp[0] & xsave_mask) != xsave_mask)
		panic("CPU0 does not support X87 or SSE: %x", cp[0]);
	xsave_mask = ((uint64_t)cp[3] << 32) | cp[0];
	xsave_mask_user = xsave_mask;
	TUNABLE_QUAD_FETCH("hw.xsave_mask", &xsave_mask_user);
	xsave_mask_user |= XFEATURE_ENABLED_X87 | XFEATURE_ENABLED_SSE;
	xsave_mask &= xsave_mask_user;
	if ((xsave_mask & XFEATURE_AVX512) != XFEATURE_AVX512)
		xsave_mask &= ~XFEATURE_AVX512;
	if ((xsave_mask & XFEATURE_MPX) != XFEATURE_MPX)
		xsave_mask &= ~XFEATURE_MPX;
}

/*
 * Calculate the fpu save area size.
 */
static void
npxinit_bsp2(void)
{
	u_int cp[4];

	if (use_xsave) {
		cpuid_count(0xd, 0x0, cp);
		cpu_max_ext_state_size = cp[1];

		/*
		 * Reload the cpu_feature2, since we enabled OSXSAVE.
		 */
		do_cpuid(1, cp);
		cpu_feature2 = cp[2];
	} else
		cpu_max_ext_state_size = sizeof(union savefpu);
}

/*
 * Initialize floating point unit.
 */
void
npxinit(bool bsp)
{
	static union savefpu dummy;
	register_t saveintr;
	u_int mxcsr;
	u_short control;

	if (bsp) {
		if (!npx_probe())
			return;
		npxinit_bsp1();
	}

	if (use_xsave) {
		load_cr4(rcr4() | CR4_XSAVE);
		load_xcr(XCR0, xsave_mask);
	}

	/*
	 * XCR0 shall be set up before CPU can report the save area size.
	 */
	if (bsp)
		npxinit_bsp2();

	/*
	 * fninit has the same h/w bugs as fnsave.  Use the detoxified
	 * fnsave to throw away any junk in the fpu.  fpusave() initializes
	 * the fpu.
	 *
	 * It is too early for critical_enter() to work on AP.
	 */
	saveintr = intr_disable();
	fpu_enable();
	if (cpu_fxsr)
		fninit();
	else
		fnsave(&dummy);
	control = __INITIAL_NPXCW__;
	fldcw(control);
	if (cpu_fxsr) {
		mxcsr = __INITIAL_MXCSR__;
		ldmxcsr(mxcsr);
	}
	fpu_disable();
	intr_restore(saveintr);
}

/*
 * On the boot CPU we generate a clean state that is used to
 * initialize the floating point unit when it is first used by a
 * process.
 */
static void
npxinitstate(void *arg __unused)
{
	uint64_t *xstate_bv;
	register_t saveintr;
	int cp[4], i, max_ext_n;

	if (!hw_float)
		return;

	/* Do potentially blocking operations before disabling interrupts. */
	fpu_save_area_zone = uma_zcreate("FPU_save_area",
	    cpu_max_ext_state_size, NULL, NULL, NULL, NULL,
	    XSAVE_AREA_ALIGN - 1, 0);
	npx_initialstate = uma_zalloc(fpu_save_area_zone, M_WAITOK | M_ZERO);
	if (use_xsave) {
		if (xsave_mask >> 32 != 0)
			max_ext_n = fls(xsave_mask >> 32) + 32;
		else
			max_ext_n = fls(xsave_mask);
		xsave_area_desc = malloc(max_ext_n * sizeof(struct
		    xsave_area_elm_descr), M_DEVBUF, M_WAITOK | M_ZERO);
	}

	saveintr = intr_disable();
	fpu_enable();

	if (cpu_fxsr)
		fpusave_fxsave(npx_initialstate);
	else
		fpusave_fnsave(npx_initialstate);
	if (cpu_fxsr) {
		if (npx_initialstate->sv_xmm.sv_env.en_mxcsr_mask)
			cpu_mxcsr_mask = 
			    npx_initialstate->sv_xmm.sv_env.en_mxcsr_mask;
		else
			cpu_mxcsr_mask = 0xFFBF;

		/*
		 * The fninit instruction does not modify XMM
		 * registers or x87 registers (MM/ST).  The fpusave
		 * call dumped the garbage contained in the registers
		 * after reset to the initial state saved.  Clear XMM
		 * and x87 registers file image to make the startup
		 * program state and signal handler XMM/x87 register
		 * content predictable.
		 */
		bzero(npx_initialstate->sv_xmm.sv_fp,
		    sizeof(npx_initialstate->sv_xmm.sv_fp));
		bzero(npx_initialstate->sv_xmm.sv_xmm,
		    sizeof(npx_initialstate->sv_xmm.sv_xmm));

	} else
		bzero(npx_initialstate->sv_87.sv_ac,
		    sizeof(npx_initialstate->sv_87.sv_ac));

	/*
	 * Create a table describing the layout of the CPU Extended
	 * Save Area.  See Intel SDM rev. 075 Vol. 1 13.4.1 "Legacy
	 * Region of an XSAVE Area" for the source of offsets/sizes.
	 * Note that 32bit XSAVE does not use %xmm8-%xmm15, see
	 * 10.5.1.2 and 13.5.2 "SSE State".
	 */
	if (use_xsave) {
		xstate_bv = (uint64_t *)((char *)(npx_initialstate + 1) +
		    offsetof(struct xstate_hdr, xstate_bv));
		*xstate_bv = XFEATURE_ENABLED_X87 | XFEATURE_ENABLED_SSE;

		/* x87 state */
		xsave_area_desc[0].offset = 0;
		xsave_area_desc[0].size = 160;
		/* XMM */
		xsave_area_desc[1].offset = 160;
		xsave_area_desc[1].size = 288 - 160;

		for (i = 2; i < max_ext_n; i++) {
			cpuid_count(0xd, i, cp);
			xsave_area_desc[i].offset = cp[1];
			xsave_area_desc[i].size = cp[0];
		}
	}

	fpu_disable();
	intr_restore(saveintr);
}
SYSINIT(npxinitstate, SI_SUB_CPU, SI_ORDER_ANY, npxinitstate, NULL);

/*
 * Free coprocessor (if we have it).
 */
void
npxexit(struct thread *td)
{

	critical_enter();
	if (curthread == PCPU_GET(fpcurthread)) {
		fpu_enable();
		fpusave(curpcb->pcb_save);
		fpu_disable();
		PCPU_SET(fpcurthread, NULL);
	}
	critical_exit();
#ifdef NPX_DEBUG
	if (hw_float) {
		u_int	masked_exceptions;

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
	}
#endif
}

int
npxformat(void)
{

	if (!hw_float)
		return (_MC_FPFMT_NODEV);
	if (cpu_fxsr)
		return (_MC_FPFMT_XMM);
	return (_MC_FPFMT_387);
}

/*
 * The following mechanism is used to ensure that the FPE_... value
 * that is passed as a trapcode to the signal handler of the user
 * process does not have more than one bit set.
 *
 * Multiple bits may be set if the user process modifies the control
 * word while a status word bit is already set.  While this is a sign
 * of bad coding, we have no choice than to narrow them down to one
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
 * Read the FP status and control words, then generate si_code value
 * for SIGFPE.  The error code chosen will be one of the
 * FPE_... macros.  It will be sent as the second argument to old
 * BSD-style signal handlers and as "siginfo_t->si_code" (second
 * argument) to SA_SIGINFO signal handlers.
 *
 * Some time ago, we cleared the x87 exceptions with FNCLEX there.
 * Clearing exceptions was necessary mainly to avoid IRQ13 bugs.  The
 * usermode code which understands the FPU hardware enough to enable
 * the exceptions, can also handle clearing the exception state in the
 * handler.  The only consequence of not clearing the exception is the
 * rethrow of the SIGFPE on return from the signal handler and
 * reexecution of the corresponding instruction.
 *
 * For XMM traps, the exceptions were never cleared.
 */
int
npxtrap_x87(void)
{
	u_short control, status;

	if (!hw_float) {
		printf(
	"npxtrap_x87: fpcurthread = %p, curthread = %p, hw_float = %d\n",
		       PCPU_GET(fpcurthread), curthread, hw_float);
		panic("npxtrap from nowhere");
	}
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
	critical_exit();
	return (fpetable[status & ((~control & 0x3f) | 0x40)]);
}

int
npxtrap_sse(void)
{
	u_int mxcsr;

	if (!hw_float) {
		printf(
	"npxtrap_sse: fpcurthread = %p, curthread = %p, hw_float = %d\n",
		       PCPU_GET(fpcurthread), curthread, hw_float);
		panic("npxtrap from nowhere");
	}
	critical_enter();
	if (PCPU_GET(fpcurthread) != curthread)
		mxcsr = curthread->td_pcb->pcb_save->sv_xmm.sv_env.en_mxcsr;
	else
		stmxcsr(&mxcsr);
	critical_exit();
	return (fpetable[(mxcsr & (~mxcsr >> 7)) & 0x3f]);
}

static void
restore_npx_curthread(struct thread *td, struct pcb *pcb)
{

	/*
	 * Record new context early in case frstor causes a trap.
	 */
	PCPU_SET(fpcurthread, td);

	fpu_enable();
	if (cpu_fxsr)
		fpu_clean_state();

	if ((pcb->pcb_flags & PCB_NPXINITDONE) == 0) {
		/*
		 * This is the first time this thread has used the FPU or
		 * the PCB doesn't contain a clean FPU state.  Explicitly
		 * load an initial state.
		 *
		 * We prefer to restore the state from the actual save
		 * area in PCB instead of directly loading from
		 * npx_initialstate, to ignite the XSAVEOPT
		 * tracking engine.
		 */
		bcopy(npx_initialstate, pcb->pcb_save, cpu_max_ext_state_size);
		fpurstor(pcb->pcb_save);
		if (pcb->pcb_initial_npxcw != __INITIAL_NPXCW__)
			fldcw(pcb->pcb_initial_npxcw);
		pcb->pcb_flags |= PCB_NPXINITDONE;
		if (PCB_USER_FPU(pcb))
			pcb->pcb_flags |= PCB_NPXUSERINITDONE;
	} else {
		fpurstor(pcb->pcb_save);
	}
}

/*
 * Implement device not available (DNA) exception
 *
 * It would be better to switch FP context here (if curthread != fpcurthread)
 * and not necessarily for every context switch, but it is too hard to
 * access foreign pcb's.
 */
int
npxdna(void)
{
	struct thread *td;

	if (!hw_float)
		return (0);
	td = curthread;
	critical_enter();

	KASSERT((curpcb->pcb_flags & PCB_NPXNOSAVE) == 0,
	    ("npxdna while in fpu_kern_enter(FPU_KERN_NOCTX)"));
	if (__predict_false(PCPU_GET(fpcurthread) == td)) {
		/*
		 * Some virtual machines seems to set %cr0.TS at
		 * arbitrary moments.  Silently clear the TS bit
		 * regardless of the eager/lazy FPU context switch
		 * mode.
		 */
		fpu_enable();
	} else {
		if (__predict_false(PCPU_GET(fpcurthread) != NULL)) {
			printf(
		    "npxdna: fpcurthread = %p (%d), curthread = %p (%d)\n",
			    PCPU_GET(fpcurthread),
			    PCPU_GET(fpcurthread)->td_proc->p_pid,
			    td, td->td_proc->p_pid);
			panic("npxdna");
		}
		restore_npx_curthread(td, td->td_pcb);
	}
	critical_exit();
	return (1);
}

/*
 * Wrapper for fpusave() called from context switch routines.
 *
 * npxsave() must be called with interrupts disabled, so that it clears
 * fpcurthread atomically with saving the state.  We require callers to do the
 * disabling, since most callers need to disable interrupts anyway to call
 * npxsave() atomically with checking fpcurthread.
 */
void
npxsave(union savefpu *addr)
{

	fpu_enable();
	fpusave(addr);
}

void npxswitch(struct thread *td, struct pcb *pcb);
void
npxswitch(struct thread *td, struct pcb *pcb)
{

	if (lazy_fpu_switch || (td->td_pflags & TDP_KTHREAD) != 0 ||
	    !PCB_USER_FPU(pcb)) {
		fpu_disable();
		PCPU_SET(fpcurthread, NULL);
	} else if (PCPU_GET(fpcurthread) != td) {
		restore_npx_curthread(td, pcb);
	}
}

/*
 * Unconditionally save the current co-processor state across suspend and
 * resume.
 */
void
npxsuspend(union savefpu *addr)
{
	register_t cr0;

	if (!hw_float)
		return;
	if (PCPU_GET(fpcurthread) == NULL) {
		bcopy(npx_initialstate, addr, cpu_max_ext_state_size);
		return;
	}
	cr0 = rcr0();
	fpu_enable();
	fpusave(addr);
	load_cr0(cr0);
}

void
npxresume(union savefpu *addr)
{
	register_t cr0;

	if (!hw_float)
		return;

	cr0 = rcr0();
	npxinit(false);
	fpu_enable();
	fpurstor(addr);
	load_cr0(cr0);
}

void
npxdrop(void)
{
	struct thread *td;

	/*
	 * Discard pending exceptions in the !cpu_fxsr case so that unmasked
	 * ones don't cause a panic on the next frstor.
	 */
	if (!cpu_fxsr)
		fnclex();

	td = PCPU_GET(fpcurthread);
	KASSERT(td == curthread, ("fpudrop: fpcurthread != curthread"));
	CRITICAL_ASSERT(td);
	PCPU_SET(fpcurthread, NULL);
	td->td_pcb->pcb_flags &= ~PCB_NPXINITDONE;
	fpu_disable();
}

/*
 * Get the user state of the FPU into pcb->pcb_user_save without
 * dropping ownership (if possible).  It returns the FPU ownership
 * status.
 */
int
npxgetregs(struct thread *td)
{
	struct pcb *pcb;
	uint64_t *xstate_bv, bit;
	char *sa;
	union savefpu *s;
	uint32_t mxcsr, mxcsr_mask;
	int max_ext_n, i;
	int owned;
	bool do_mxcsr;

	if (!hw_float)
		return (_MC_FPOWNED_NONE);

	pcb = td->td_pcb;
	critical_enter();
	if ((pcb->pcb_flags & PCB_NPXINITDONE) == 0) {
		bcopy(npx_initialstate, get_pcb_user_save_pcb(pcb),
		    cpu_max_ext_state_size);
		SET_FPU_CW(get_pcb_user_save_pcb(pcb), pcb->pcb_initial_npxcw);
		npxuserinited(td);
		critical_exit();
		return (_MC_FPOWNED_PCB);
	}
	if (td == PCPU_GET(fpcurthread)) {
		fpusave(get_pcb_user_save_pcb(pcb));
		if (!cpu_fxsr)
			/*
			 * fnsave initializes the FPU and destroys whatever
			 * context it contains.  Make sure the FPU owner
			 * starts with a clean state next time.
			 */
			npxdrop();
		owned = _MC_FPOWNED_FPU;
	} else {
		owned = _MC_FPOWNED_PCB;
	}
	if (use_xsave) {
		/*
		 * Handle partially saved state.
		 */
		sa = (char *)get_pcb_user_save_pcb(pcb);
		xstate_bv = (uint64_t *)(sa + sizeof(union savefpu) +
		    offsetof(struct xstate_hdr, xstate_bv));
		if (xsave_mask >> 32 != 0)
			max_ext_n = fls(xsave_mask >> 32) + 32;
		else
			max_ext_n = fls(xsave_mask);
		for (i = 0; i < max_ext_n; i++) {
			bit = 1ULL << i;
			if ((xsave_mask & bit) == 0 || (*xstate_bv & bit) != 0)
				continue;
			do_mxcsr = false;
			if (i == 0 && (*xstate_bv & (XFEATURE_ENABLED_SSE |
			    XFEATURE_ENABLED_AVX)) != 0) {
				/*
				 * x87 area was not saved by XSAVEOPT,
				 * but one of XMM or AVX was.  Then we need
				 * to preserve MXCSR from being overwritten
				 * with the default value.
				 */
				s = (union savefpu *)sa;
				mxcsr = s->sv_xmm.sv_env.en_mxcsr;
				mxcsr_mask = s->sv_xmm.sv_env.en_mxcsr_mask;
				do_mxcsr = true;
			}
			bcopy((char *)npx_initialstate +
			    xsave_area_desc[i].offset,
			    sa + xsave_area_desc[i].offset,
			    xsave_area_desc[i].size);
			if (do_mxcsr) {
				s->sv_xmm.sv_env.en_mxcsr = mxcsr;
				s->sv_xmm.sv_env.en_mxcsr_mask = mxcsr_mask;
			}
			*xstate_bv |= bit;
		}
	}
	critical_exit();
	return (owned);
}

void
npxuserinited(struct thread *td)
{
	struct pcb *pcb;

	CRITICAL_ASSERT(td);
	pcb = td->td_pcb;
	if (PCB_USER_FPU(pcb))
		pcb->pcb_flags |= PCB_NPXINITDONE;
	pcb->pcb_flags |= PCB_NPXUSERINITDONE;
}

int
npxsetxstate(struct thread *td, char *xfpustate, size_t xfpustate_size)
{
	struct xstate_hdr *hdr, *ehdr;
	size_t len, max_len;
	uint64_t bv;

	/* XXXKIB should we clear all extended state in xstate_bv instead ? */
	if (xfpustate == NULL)
		return (0);
	if (!use_xsave)
		return (EOPNOTSUPP);

	len = xfpustate_size;
	if (len < sizeof(struct xstate_hdr))
		return (EINVAL);
	max_len = cpu_max_ext_state_size - sizeof(union savefpu);
	if (len > max_len)
		return (EINVAL);

	ehdr = (struct xstate_hdr *)xfpustate;
	bv = ehdr->xstate_bv;

	/*
	 * Avoid #gp.
	 */
	if (bv & ~xsave_mask)
		return (EINVAL);

	hdr = (struct xstate_hdr *)(get_pcb_user_save_td(td) + 1);

	hdr->xstate_bv = bv;
	bcopy(xfpustate + sizeof(struct xstate_hdr),
	    (char *)(hdr + 1), len - sizeof(struct xstate_hdr));

	return (0);
}

int
npxsetregs(struct thread *td, union savefpu *addr, char *xfpustate,
	size_t xfpustate_size)
{
	struct pcb *pcb;
	int error;

	if (!hw_float)
		return (ENXIO);

	if (cpu_fxsr)
		addr->sv_xmm.sv_env.en_mxcsr &= cpu_mxcsr_mask;
	pcb = td->td_pcb;
	error = 0;
	critical_enter();
	if (td == PCPU_GET(fpcurthread) && PCB_USER_FPU(pcb)) {
		error = npxsetxstate(td, xfpustate, xfpustate_size);
		if (error == 0) {
			if (!cpu_fxsr)
				fnclex();	/* As in npxdrop(). */
			bcopy(addr, get_pcb_user_save_td(td), sizeof(*addr));
			fpurstor(get_pcb_user_save_td(td));
			pcb->pcb_flags |= PCB_NPXUSERINITDONE | PCB_NPXINITDONE;
		}
	} else {
		error = npxsetxstate(td, xfpustate, xfpustate_size);
		if (error == 0) {
			bcopy(addr, get_pcb_user_save_td(td), sizeof(*addr));
			npxuserinited(td);
		}
	}
	critical_exit();
	return (error);
}

static void
npx_fill_fpregs_xmm1(struct savexmm *sv_xmm, struct save87 *sv_87)
{
	struct env87 *penv_87;
	struct envxmm *penv_xmm;
	struct fpacc87 *fx_reg;
	int i, st;
	uint64_t mantissa;
	uint16_t tw, exp;
	uint8_t ab_tw;

	penv_87 = &sv_87->sv_env;
	penv_xmm = &sv_xmm->sv_env;

	/* FPU control/status */
	penv_87->en_cw = penv_xmm->en_cw;
	penv_87->en_sw = penv_xmm->en_sw;
	penv_87->en_fip = penv_xmm->en_fip;
	penv_87->en_fcs = penv_xmm->en_fcs;
	penv_87->en_opcode = penv_xmm->en_opcode;
	penv_87->en_foo = penv_xmm->en_foo;
	penv_87->en_fos = penv_xmm->en_fos;

	/*
	 * FPU registers and tags.
	 * For ST(i), i = fpu_reg - top; we start with fpu_reg=7.
	 */
	st = 7 - ((penv_xmm->en_sw >> 11) & 7);
	ab_tw = penv_xmm->en_tw;
	tw = 0;
	for (i = 0x80; i != 0; i >>= 1) {
		sv_87->sv_ac[st] = sv_xmm->sv_fp[st].fp_acc;
		tw <<= 2;
		if (ab_tw & i) {
			/* Non-empty - we need to check ST(i) */
			fx_reg = &sv_xmm->sv_fp[st].fp_acc;
			/* The first 64 bits contain the mantissa. */
			mantissa = *((uint64_t *)fx_reg->fp_bytes);
			/*
			 * The final 16 bits contain the sign bit and the exponent.
			 * Mask the sign bit since it is of no consequence to these
			 * tests.
			 */
			exp = *((uint16_t *)&fx_reg->fp_bytes[8]) & 0x7fff;
			if (exp == 0) {
				if (mantissa == 0)
					tw |= 1; /* Zero */
				else
					tw |= 2; /* Denormal */
			} else if (exp == 0x7fff)
				tw |= 2; /* Infinity or NaN */
		} else
			tw |= 3; /* Empty */
		st = (st - 1) & 7;
	}
	penv_87->en_tw = tw;
}

void
npx_fill_fpregs_xmm(struct savexmm *sv_xmm, struct save87 *sv_87)
{

	bzero(sv_87, sizeof(*sv_87));
	npx_fill_fpregs_xmm1(sv_xmm, sv_87);
}

void
npx_set_fpregs_xmm(struct save87 *sv_87, struct savexmm *sv_xmm)
{
	struct env87 *penv_87;
	struct envxmm *penv_xmm;
	int i;

	penv_87 = &sv_87->sv_env;
	penv_xmm = &sv_xmm->sv_env;

	/* FPU control/status */
	penv_xmm->en_cw = penv_87->en_cw;
	penv_xmm->en_sw = penv_87->en_sw;
	penv_xmm->en_fip = penv_87->en_fip;
	penv_xmm->en_fcs = penv_87->en_fcs;
	penv_xmm->en_opcode = penv_87->en_opcode;
	penv_xmm->en_foo = penv_87->en_foo;
	penv_xmm->en_fos = penv_87->en_fos;

	/*
	 * FPU registers and tags.
	 * Abridged  /  Full translation (values in binary), see FXSAVE spec.
	 * 0		11
	 * 1		00, 01, 10
	 */
	penv_xmm->en_tw = 0;
	for (i = 0; i < 8; ++i) {
		sv_xmm->sv_fp[i].fp_acc = sv_87->sv_ac[i];
		if ((penv_87->en_tw & (3 << i * 2)) != (3 << i * 2))
			penv_xmm->en_tw |= 1 << i;
	}
}

void
npx_get_fsave(void *addr)
{
	struct thread *td;
	union savefpu *sv;

	td = curthread;
	npxgetregs(td);
	sv = get_pcb_user_save_td(td);
	if (cpu_fxsr)
		npx_fill_fpregs_xmm1(&sv->sv_xmm, addr);
	else
		bcopy(sv, addr, sizeof(struct env87) +
		    sizeof(struct fpacc87[8]));
}

int
npx_set_fsave(void *addr)
{
	union savefpu sv;
	int error;

	bzero(&sv, sizeof(sv));
	if (cpu_fxsr)
		npx_set_fpregs_xmm(addr, &sv.sv_xmm);
	else
		bcopy(addr, &sv, sizeof(struct env87) +
		    sizeof(struct fpacc87[8]));
	error = npxsetregs(curthread, &sv, NULL, 0);
	return (error);
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

static void
fpurstor(union savefpu *addr)
{

	if (use_xsave)
		xrstor((char *)addr, xsave_mask);
	else if (cpu_fxsr)
		fxrstor(addr);
	else
		frstor(addr);
}

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
	{ 0, 0 }
};

static driver_t npxisa_driver = {
	"npxisa",
	npxisa_methods,
	1,			/* no softc */
};

DRIVER_MODULE(npxisa, isa, npxisa_driver, 0, 0);
DRIVER_MODULE(npxisa, acpi, npxisa_driver, 0, 0);
ISA_PNP_INFO(npxisa_ids);
#endif /* DEV_ISA */

static MALLOC_DEFINE(M_FPUKERN_CTX, "fpukern_ctx",
    "Kernel contexts for FPU state");

#define	FPU_KERN_CTX_NPXINITDONE 0x01
#define	FPU_KERN_CTX_DUMMY	 0x02
#define	FPU_KERN_CTX_INUSE	 0x04

struct fpu_kern_ctx {
	union savefpu *prev;
	uint32_t flags;
	char hwstate1[];
};

struct fpu_kern_ctx *
fpu_kern_alloc_ctx(u_int flags)
{
	struct fpu_kern_ctx *res;
	size_t sz;

	sz = sizeof(struct fpu_kern_ctx) + XSAVE_AREA_ALIGN +
	    cpu_max_ext_state_size;
	res = malloc(sz, M_FPUKERN_CTX, ((flags & FPU_KERN_NOWAIT) ?
	    M_NOWAIT : M_WAITOK) | M_ZERO);
	return (res);
}

void
fpu_kern_free_ctx(struct fpu_kern_ctx *ctx)
{

	KASSERT((ctx->flags & FPU_KERN_CTX_INUSE) == 0, ("free'ing inuse ctx"));
	/* XXXKIB clear the memory ? */
	free(ctx, M_FPUKERN_CTX);
}

static union savefpu *
fpu_kern_ctx_savefpu(struct fpu_kern_ctx *ctx)
{
	vm_offset_t p;

	p = (vm_offset_t)&ctx->hwstate1;
	p = roundup2(p, XSAVE_AREA_ALIGN);
	return ((union savefpu *)p);
}

void
fpu_kern_enter(struct thread *td, struct fpu_kern_ctx *ctx, u_int flags)
{
	struct pcb *pcb;

	pcb = td->td_pcb;
	KASSERT((flags & FPU_KERN_NOCTX) != 0 || ctx != NULL,
	    ("ctx is required when !FPU_KERN_NOCTX"));
	KASSERT(ctx == NULL || (ctx->flags & FPU_KERN_CTX_INUSE) == 0,
	    ("using inuse ctx"));
	KASSERT((pcb->pcb_flags & PCB_NPXNOSAVE) == 0,
	    ("recursive fpu_kern_enter while in PCB_NPXNOSAVE state"));

	if ((flags & FPU_KERN_NOCTX) != 0) {
		critical_enter();
		fpu_enable();
		if (curthread == PCPU_GET(fpcurthread)) {
			fpusave(curpcb->pcb_save);
			PCPU_SET(fpcurthread, NULL);
		} else {
			KASSERT(PCPU_GET(fpcurthread) == NULL,
			    ("invalid fpcurthread"));
		}

		/*
		 * This breaks XSAVEOPT tracker, but
		 * PCB_NPXNOSAVE state is supposed to never need to
		 * save FPU context at all.
		 */
		fpurstor(npx_initialstate);
		pcb->pcb_flags |= PCB_KERNNPX | PCB_NPXNOSAVE | PCB_NPXINITDONE;
		return;
	}
	if ((flags & FPU_KERN_KTHR) != 0 && is_fpu_kern_thread(0)) {
		ctx->flags = FPU_KERN_CTX_DUMMY | FPU_KERN_CTX_INUSE;
		return;
	}
	pcb = td->td_pcb;
	critical_enter();
	KASSERT(!PCB_USER_FPU(pcb) || pcb->pcb_save ==
	    get_pcb_user_save_pcb(pcb), ("mangled pcb_save"));
	ctx->flags = FPU_KERN_CTX_INUSE;
	if ((pcb->pcb_flags & PCB_NPXINITDONE) != 0)
		ctx->flags |= FPU_KERN_CTX_NPXINITDONE;
	npxexit(td);
	ctx->prev = pcb->pcb_save;
	pcb->pcb_save = fpu_kern_ctx_savefpu(ctx);
	pcb->pcb_flags |= PCB_KERNNPX;
	pcb->pcb_flags &= ~PCB_NPXINITDONE;
	critical_exit();
}

int
fpu_kern_leave(struct thread *td, struct fpu_kern_ctx *ctx)
{
	struct pcb *pcb;

	pcb = td->td_pcb;

	if ((pcb->pcb_flags & PCB_NPXNOSAVE) != 0) {
		KASSERT(ctx == NULL, ("non-null ctx after FPU_KERN_NOCTX"));
		KASSERT(PCPU_GET(fpcurthread) == NULL,
		    ("non-NULL fpcurthread for PCB_NPXNOSAVE"));
		CRITICAL_ASSERT(td);

		pcb->pcb_flags &= ~(PCB_NPXNOSAVE | PCB_NPXINITDONE);
		fpu_disable();
	} else {
		KASSERT((ctx->flags & FPU_KERN_CTX_INUSE) != 0,
		    ("leaving not inuse ctx"));
		ctx->flags &= ~FPU_KERN_CTX_INUSE;

		if (is_fpu_kern_thread(0) &&
		    (ctx->flags & FPU_KERN_CTX_DUMMY) != 0)
			return (0);
		KASSERT((ctx->flags & FPU_KERN_CTX_DUMMY) == 0,
		    ("dummy ctx"));
		critical_enter();
		if (curthread == PCPU_GET(fpcurthread))
			npxdrop();
		pcb->pcb_save = ctx->prev;
	}

	if (pcb->pcb_save == get_pcb_user_save_pcb(pcb)) {
		if ((pcb->pcb_flags & PCB_NPXUSERINITDONE) != 0) {
			pcb->pcb_flags |= PCB_NPXINITDONE;
			if ((pcb->pcb_flags & PCB_KERNNPX_THR) == 0)
				pcb->pcb_flags &= ~PCB_KERNNPX;
		} else if ((pcb->pcb_flags & PCB_KERNNPX_THR) == 0)
			pcb->pcb_flags &= ~(PCB_NPXINITDONE | PCB_KERNNPX);
	} else {
		if ((ctx->flags & FPU_KERN_CTX_NPXINITDONE) != 0)
			pcb->pcb_flags |= PCB_NPXINITDONE;
		else
			pcb->pcb_flags &= ~PCB_NPXINITDONE;
		KASSERT(!PCB_USER_FPU(pcb), ("unpaired fpu_kern_leave"));
	}
	critical_exit();
	return (0);
}

int
fpu_kern_thread(u_int flags)
{

	KASSERT((curthread->td_pflags & TDP_KTHREAD) != 0,
	    ("Only kthread may use fpu_kern_thread"));
	KASSERT(curpcb->pcb_save == get_pcb_user_save_pcb(curpcb),
	    ("mangled pcb_save"));
	KASSERT(PCB_USER_FPU(curpcb), ("recursive call"));

	curpcb->pcb_flags |= PCB_KERNNPX | PCB_KERNNPX_THR;
	return (0);
}

int
is_fpu_kern_thread(u_int flags)
{

	if ((curthread->td_pflags & TDP_KTHREAD) == 0)
		return (0);
	return ((curpcb->pcb_flags & PCB_KERNNPX_THR) != 0);
}

/*
 * FPU save area alloc/free/init utility routines
 */
union savefpu *
fpu_save_area_alloc(void)
{

	return (uma_zalloc(fpu_save_area_zone, M_WAITOK));
}

void
fpu_save_area_free(union savefpu *fsa)
{

	uma_zfree(fpu_save_area_zone, fsa);
}

void
fpu_save_area_reset(union savefpu *fsa)
{

	bcopy(npx_initialstate, fsa, cpu_max_ext_state_size);
}
