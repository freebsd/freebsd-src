/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/domainset.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/tslog.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/signalvar.h>
#include <vm/uma.h>

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
#include <x86/ifunc.h>

/*
 * Floating point support.
 */

#define	fldcw(cw)		__asm __volatile("fldcw %0" : : "m" (cw))
#define	fnclex()		__asm __volatile("fnclex")
#define	fninit()		__asm __volatile("fninit")
#define	fnstcw(addr)		__asm __volatile("fnstcw %0" : "=m" (*(addr)))
#define	fnstsw(addr)		__asm __volatile("fnstsw %0" : "=am" (*(addr)))
#define	fxrstor(addr)		__asm __volatile("fxrstor %0" : : "m" (*(addr)))
#define	fxsave(addr)		__asm __volatile("fxsave %0" : "=m" (*(addr)))
#define	ldmxcsr(csr)		__asm __volatile("ldmxcsr %0" : : "m" (csr))
#define	stmxcsr(addr)		__asm __volatile("stmxcsr %0" : "=m" (*(addr)))

static __inline void
xrstor32(char *addr, uint64_t mask)
{
	uint32_t low, hi;

	low = mask;
	hi = mask >> 32;
	__asm __volatile("xrstor %0" : : "m" (*addr), "a" (low), "d" (hi));
}

static __inline void
xrstor64(char *addr, uint64_t mask)
{
	uint32_t low, hi;

	low = mask;
	hi = mask >> 32;
	__asm __volatile("xrstor64 %0" : : "m" (*addr), "a" (low), "d" (hi));
}

static __inline void
xsave32(char *addr, uint64_t mask)
{
	uint32_t low, hi;

	low = mask;
	hi = mask >> 32;
	__asm __volatile("xsave %0" : "=m" (*addr) : "a" (low), "d" (hi) :
	    "memory");
}

static __inline void
xsave64(char *addr, uint64_t mask)
{
	uint32_t low, hi;

	low = mask;
	hi = mask >> 32;
	__asm __volatile("xsave64 %0" : "=m" (*addr) : "a" (low), "d" (hi) :
	    "memory");
}

static __inline void
xsaveopt32(char *addr, uint64_t mask)
{
	uint32_t low, hi;

	low = mask;
	hi = mask >> 32;
	__asm __volatile("xsaveopt %0" : "=m" (*addr) : "a" (low), "d" (hi) :
	    "memory");
}

static __inline void
xsaveopt64(char *addr, uint64_t mask)
{
	uint32_t low, hi;

	low = mask;
	hi = mask >> 32;
	__asm __volatile("xsaveopt64 %0" : "=m" (*addr) : "a" (low), "d" (hi) :
	    "memory");
}

CTASSERT(sizeof(struct savefpu) == 512);
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
CTASSERT(X86_XSTATE_XCR0_OFFSET >= offsetof(struct savefpu, sv_pad) &&
    X86_XSTATE_XCR0_OFFSET + sizeof(uint64_t) <= sizeof(struct savefpu));

static	void	fpu_clean_state(void);

SYSCTL_INT(_hw, HW_FLOATINGPT, floatingpoint, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 1, "Floating point instructions executed in hardware");

int use_xsave;			/* non-static for cpu_switch.S */
uint64_t xsave_mask;		/* the same */
static	uint64_t xsave_mask_supervisor;
static	uint64_t xsave_extensions;
static	uma_zone_t fpu_save_area_zone;
static	struct savefpu *fpu_initialstate;

static struct xsave_area_elm_descr {
	u_int	offset;
	u_int	size;
	u_int	flags;
} *xsave_area_desc;

static void
fpusave_xsaveopt64(void *addr)
{
	xsaveopt64((char *)addr, xsave_mask);
}

static void
fpusave_xsaveopt3264(void *addr)
{
	if (SV_CURPROC_FLAG(SV_ILP32))
		xsaveopt32((char *)addr, xsave_mask);
	else
		xsaveopt64((char *)addr, xsave_mask);
}

static void
fpusave_xsave64(void *addr)
{
	xsave64((char *)addr, xsave_mask);
}

static void
fpusave_xsave3264(void *addr)
{
	if (SV_CURPROC_FLAG(SV_ILP32))
		xsave32((char *)addr, xsave_mask);
	else
		xsave64((char *)addr, xsave_mask);
}

static void
fpurestore_xrstor64(void *addr)
{
	xrstor64((char *)addr, xsave_mask);
}

static void
fpurestore_xrstor3264(void *addr)
{
	if (SV_CURPROC_FLAG(SV_ILP32))
		xrstor32((char *)addr, xsave_mask);
	else
		xrstor64((char *)addr, xsave_mask);
}

static void
fpusave_fxsave(void *addr)
{

	fxsave((char *)addr);
}

static void
fpurestore_fxrstor(void *addr)
{

	fxrstor((char *)addr);
}

DEFINE_IFUNC(, void, fpusave, (void *))
{
	u_int cp[4];

	if (!use_xsave)
		return (fpusave_fxsave);
	cpuid_count(0xd, 0x1, cp);
	if ((cp[0] & CPUID_EXTSTATE_XSAVEOPT) != 0) {
		return ((cpu_stdext_feature & CPUID_STDEXT_NFPUSG) != 0 ?
		    fpusave_xsaveopt64 : fpusave_xsaveopt3264);
	}
	return ((cpu_stdext_feature & CPUID_STDEXT_NFPUSG) != 0 ?
	    fpusave_xsave64 : fpusave_xsave3264);
}

DEFINE_IFUNC(, void, fpurestore, (void *))
{
	if (!use_xsave)
		return (fpurestore_fxrstor);
	return ((cpu_stdext_feature & CPUID_STDEXT_NFPUSG) != 0 ?
	    fpurestore_xrstor64 : fpurestore_xrstor3264);
}

void
fpususpend(void *addr)
{
	u_long cr0;

	cr0 = rcr0();
	fpu_enable();
	fpusave(addr);
	load_cr0(cr0);
}

void
fpuresume(void *addr)
{
	u_long cr0;

	cr0 = rcr0();
	fpu_enable();
	fninit();
	if (use_xsave)
		load_xcr(XCR0, xsave_mask);
	fpurestore(addr);
	load_cr0(cr0);
}

/*
 * Enable XSAVE if supported and allowed by user.
 * Calculate the xsave_mask.
 */
static void
fpuinit_bsp1(void)
{
	u_int cp[4];
	uint64_t xsave_mask_user;
	bool old_wp;

	if (!use_xsave)
		return;
	cpuid_count(0xd, 0x0, cp);
	xsave_mask = XFEATURE_ENABLED_X87 | XFEATURE_ENABLED_SSE;
	if ((cp[0] & xsave_mask) != xsave_mask)
		panic("CPU0 does not support X87 or SSE: %x", cp[0]);
	xsave_mask = ((uint64_t)cp[3] << 32) | cp[0];
	xsave_mask_user = xsave_mask;
	TUNABLE_ULONG_FETCH("hw.xsave_mask", &xsave_mask_user);
	xsave_mask_user |= XFEATURE_ENABLED_X87 | XFEATURE_ENABLED_SSE;
	xsave_mask &= xsave_mask_user;
	if ((xsave_mask & XFEATURE_AVX512) != XFEATURE_AVX512)
		xsave_mask &= ~XFEATURE_AVX512;
	if ((xsave_mask & XFEATURE_MPX) != XFEATURE_MPX)
		xsave_mask &= ~XFEATURE_MPX;

	cpuid_count(0xd, 0x1, cp);
	if ((cp[0] & CPUID_EXTSTATE_XSAVEOPT) != 0) {
		/*
		 * Patch the XSAVE instruction in the cpu_switch code
		 * to XSAVEOPT.  We assume that XSAVE encoding used
		 * REX byte, and set the bit 4 of the r/m byte.
		 *
		 * It seems that some BIOSes give control to the OS
		 * with CR0.WP already set, making the kernel text
		 * read-only before cpu_startup().
		 */
		old_wp = disable_wp();
		ctx_switch_xsave32[3] |= 0x10;
		ctx_switch_xsave[3] |= 0x10;
		restore_wp(old_wp);
	}
	xsave_mask_supervisor = ((uint64_t)cp[3] << 32) | cp[2];
}

/*
 * Calculate the fpu save area size.
 */
static void
fpuinit_bsp2(void)
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
		cpu_max_ext_state_size = sizeof(struct savefpu);
}

/*
 * Initialize the floating point unit.
 */
void
fpuinit(void)
{
	register_t saveintr;
	uint64_t cr4;
	u_int mxcsr;
	u_short control;

	TSENTER();
	if (IS_BSP())
		fpuinit_bsp1();

	if (use_xsave) {
		cr4 = rcr4();

		/*
		 * Revert enablement of PKRU if user disabled its
		 * saving on context switches by clearing the bit in
		 * the xsave mask.  Also redundantly clear the bit in
		 * cpu_stdext_feature2 to prevent pmap from ever
		 * trying to set the page table bits.
		 */
		if ((cpu_stdext_feature2 & CPUID_STDEXT2_PKU) != 0 &&
		    (xsave_mask & XFEATURE_ENABLED_PKRU) == 0) {
			cr4 &= ~CR4_PKE;
			cpu_stdext_feature2 &= ~CPUID_STDEXT2_PKU;
		}

		load_cr4(cr4 | CR4_XSAVE);
		load_xcr(XCR0, xsave_mask);
	}

	/*
	 * XCR0 shall be set up before CPU can report the save area size.
	 */
	if (IS_BSP())
		fpuinit_bsp2();

	/*
	 * It is too early for critical_enter() to work on AP.
	 */
	saveintr = intr_disable();
	fpu_enable();
	fninit();
	control = __INITIAL_FPUCW__;
	fldcw(control);
	mxcsr = __INITIAL_MXCSR__;
	ldmxcsr(mxcsr);
	fpu_disable();
	intr_restore(saveintr);
	TSEXIT();
}

/*
 * On the boot CPU we generate a clean state that is used to
 * initialize the floating point unit when it is first used by a
 * process.
 */
static void
fpuinitstate(void *arg __unused)
{
	uint64_t *xstate_bv;
	register_t saveintr;
	int cp[4], i, max_ext_n;

	/* Do potentially blocking operations before disabling interrupts. */
	fpu_save_area_zone = uma_zcreate("FPU_save_area",
	    cpu_max_ext_state_size, NULL, NULL, NULL, NULL,
	    XSAVE_AREA_ALIGN - 1, 0);
	fpu_initialstate = uma_zalloc(fpu_save_area_zone, M_WAITOK | M_ZERO);
	if (use_xsave) {
		max_ext_n = flsl(xsave_mask | xsave_mask_supervisor);
		xsave_area_desc = malloc(max_ext_n * sizeof(struct
		    xsave_area_elm_descr), M_DEVBUF, M_WAITOK | M_ZERO);
	}

	cpu_thread_alloc(&thread0);

	saveintr = intr_disable();
	fpu_enable();

	fpusave_fxsave(fpu_initialstate);
	if (fpu_initialstate->sv_env.en_mxcsr_mask)
		cpu_mxcsr_mask = fpu_initialstate->sv_env.en_mxcsr_mask;
	else
		cpu_mxcsr_mask = 0xFFBF;

	/*
	 * The fninit instruction does not modify XMM registers or x87
	 * registers (MM/ST).  The fpusave call dumped the garbage
	 * contained in the registers after reset to the initial state
	 * saved.  Clear XMM and x87 registers file image to make the
	 * startup program state and signal handler XMM/x87 register
	 * content predictable.
	 */
	bzero(fpu_initialstate->sv_fp, sizeof(fpu_initialstate->sv_fp));
	bzero(fpu_initialstate->sv_xmm, sizeof(fpu_initialstate->sv_xmm));

	/*
	 * Create a table describing the layout of the CPU Extended
	 * Save Area.  See Intel SDM rev. 075 Vol. 1 13.4.1 "Legacy
	 * Region of an XSAVE Area" for the source of offsets/sizes.
	 */
	if (use_xsave) {
		cpuid_count(0xd, 1, cp);
		xsave_extensions = cp[0];

		xstate_bv = (uint64_t *)((char *)(fpu_initialstate + 1) +
		    offsetof(struct xstate_hdr, xstate_bv));
		*xstate_bv = XFEATURE_ENABLED_X87 | XFEATURE_ENABLED_SSE;

		/* x87 state */
		xsave_area_desc[0].offset = 0;
		xsave_area_desc[0].size = 160;
		/* XMM */
		xsave_area_desc[1].offset = 160;
		xsave_area_desc[1].size = 416 - 160;

		for (i = 2; i < max_ext_n; i++) {
			cpuid_count(0xd, i, cp);
			xsave_area_desc[i].size = cp[0];
			xsave_area_desc[i].offset = cp[1];
			xsave_area_desc[i].flags = cp[2];
		}
	}

	fpu_disable();
	intr_restore(saveintr);
}
/* EFIRT needs this to be initialized before we can enter our EFI environment */
SYSINIT(fpuinitstate, SI_SUB_CPU, SI_ORDER_ANY, fpuinitstate, NULL);

/*
 * Free coprocessor (if we have it).
 */
void
fpuexit(struct thread *td)
{

	critical_enter();
	if (curthread == PCPU_GET(fpcurthread)) {
		fpu_enable();
		fpusave(curpcb->pcb_save);
		fpu_disable();
		PCPU_SET(fpcurthread, NULL);
	}
	critical_exit();
}

int
fpuformat(void)
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
fputrap_x87(void)
{
	struct savefpu *pcb_save;
	u_short control, status;

	critical_enter();

	/*
	 * Interrupt handling (for another interrupt) may have pushed the
	 * state to memory.  Fetch the relevant parts of the state from
	 * wherever they are.
	 */
	if (PCPU_GET(fpcurthread) != curthread) {
		pcb_save = curpcb->pcb_save;
		control = pcb_save->sv_env.en_cw;
		status = pcb_save->sv_env.en_sw;
	} else {
		fnstcw(&control);
		fnstsw(&status);
	}

	critical_exit();
	return (fpetable[status & ((~control & 0x3f) | 0x40)]);
}

int
fputrap_sse(void)
{
	u_int mxcsr;

	critical_enter();
	if (PCPU_GET(fpcurthread) != curthread)
		mxcsr = curpcb->pcb_save->sv_env.en_mxcsr;
	else
		stmxcsr(&mxcsr);
	critical_exit();
	return (fpetable[(mxcsr & (~mxcsr >> 7)) & 0x3f]);
}

static void
restore_fpu_curthread(struct thread *td)
{
	struct pcb *pcb;

	/*
	 * Record new context early in case frstor causes a trap.
	 */
	PCPU_SET(fpcurthread, td);

	fpu_enable();
	fpu_clean_state();
	pcb = td->td_pcb;

	if ((pcb->pcb_flags & PCB_FPUINITDONE) == 0) {
		/*
		 * This is the first time this thread has used the FPU or
		 * the PCB doesn't contain a clean FPU state.  Explicitly
		 * load an initial state.
		 *
		 * We prefer to restore the state from the actual save
		 * area in PCB instead of directly loading from
		 * fpu_initialstate, to ignite the XSAVEOPT
		 * tracking engine.
		 */
		bcopy(fpu_initialstate, pcb->pcb_save,
		    cpu_max_ext_state_size);
		fpurestore(pcb->pcb_save);
		if (pcb->pcb_initial_fpucw != __INITIAL_FPUCW__)
			fldcw(pcb->pcb_initial_fpucw);
		if (PCB_USER_FPU(pcb))
			set_pcb_flags(pcb, PCB_FPUINITDONE |
			    PCB_USERFPUINITDONE);
		else
			set_pcb_flags(pcb, PCB_FPUINITDONE);
	} else
		fpurestore(pcb->pcb_save);
}

/*
 * Device Not Available (DNA, #NM) exception handler.
 *
 * It would be better to switch FP context here (if curthread !=
 * fpcurthread) and not necessarily for every context switch, but it
 * is too hard to access foreign pcb's.
 */
void
fpudna(void)
{
	struct thread *td;

	td = curthread;
	/*
	 * This handler is entered with interrupts enabled, so context
	 * switches may occur before critical_enter() is executed.  If
	 * a context switch occurs, then when we regain control, our
	 * state will have been completely restored.  The CPU may
	 * change underneath us, but the only part of our context that
	 * lives in the CPU is CR0.TS and that will be "restored" by
	 * setting it on the new CPU.
	 */
	critical_enter();

	KASSERT((curpcb->pcb_flags & PCB_FPUNOSAVE) == 0,
	    ("fpudna while in fpu_kern_enter(FPU_KERN_NOCTX)"));
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
			panic(
		    "fpudna: fpcurthread = %p (%d), curthread = %p (%d)\n",
			    PCPU_GET(fpcurthread),
			    PCPU_GET(fpcurthread)->td_tid, td, td->td_tid);
		}
		restore_fpu_curthread(td);
	}
	critical_exit();
}

void fpu_activate_sw(struct thread *td); /* Called from the context switch */
void
fpu_activate_sw(struct thread *td)
{

	if ((td->td_pflags & TDP_KTHREAD) != 0 || !PCB_USER_FPU(td->td_pcb)) {
		PCPU_SET(fpcurthread, NULL);
		fpu_disable();
	} else if (PCPU_GET(fpcurthread) != td) {
		restore_fpu_curthread(td);
	}
}

void
fpudrop(void)
{
	struct thread *td;

	td = PCPU_GET(fpcurthread);
	KASSERT(td == curthread, ("fpudrop: fpcurthread != curthread"));
	CRITICAL_ASSERT(td);
	PCPU_SET(fpcurthread, NULL);
	clear_pcb_flags(td->td_pcb, PCB_FPUINITDONE);
	fpu_disable();
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
	uint64_t *xstate_bv, bit;
	char *sa;
	struct savefpu *s;
	uint32_t mxcsr, mxcsr_mask;
	int max_ext_n, i, owned;
	bool do_mxcsr;

	pcb = td->td_pcb;
	critical_enter();
	if ((pcb->pcb_flags & PCB_USERFPUINITDONE) == 0) {
		bcopy(fpu_initialstate, get_pcb_user_save_pcb(pcb),
		    cpu_max_ext_state_size);
		get_pcb_user_save_pcb(pcb)->sv_env.en_cw =
		    pcb->pcb_initial_fpucw;
		fpuuserinited(td);
		critical_exit();
		return (_MC_FPOWNED_PCB);
	}
	if (td == PCPU_GET(fpcurthread) && PCB_USER_FPU(pcb)) {
		fpusave(get_pcb_user_save_pcb(pcb));
		owned = _MC_FPOWNED_FPU;
	} else {
		owned = _MC_FPOWNED_PCB;
	}
	if (use_xsave) {
		/*
		 * Handle partially saved state.
		 */
		sa = (char *)get_pcb_user_save_pcb(pcb);
		xstate_bv = (uint64_t *)(sa + sizeof(struct savefpu) +
		    offsetof(struct xstate_hdr, xstate_bv));
		max_ext_n = flsl(xsave_mask);
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
				s = (struct savefpu *)sa;
				mxcsr = s->sv_env.en_mxcsr;
				mxcsr_mask = s->sv_env.en_mxcsr_mask;
				do_mxcsr = true;
			}
			bcopy((char *)fpu_initialstate +
			    xsave_area_desc[i].offset,
			    sa + xsave_area_desc[i].offset,
			    xsave_area_desc[i].size);
			if (do_mxcsr) {
				s->sv_env.en_mxcsr = mxcsr;
				s->sv_env.en_mxcsr_mask = mxcsr_mask;
			}
			*xstate_bv |= bit;
		}
	}
	critical_exit();
	return (owned);
}

void
fpuuserinited(struct thread *td)
{
	struct pcb *pcb;

	CRITICAL_ASSERT(td);
	pcb = td->td_pcb;
	if (PCB_USER_FPU(pcb))
		set_pcb_flags(pcb,
		    PCB_FPUINITDONE | PCB_USERFPUINITDONE);
	else
		set_pcb_flags(pcb, PCB_FPUINITDONE);
}

int
fpusetxstate(struct thread *td, char *xfpustate, size_t xfpustate_size)
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
	max_len = cpu_max_ext_state_size - sizeof(struct savefpu);
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

/*
 * Set the state of the FPU.
 */
int
fpusetregs(struct thread *td, struct savefpu *addr, char *xfpustate,
    size_t xfpustate_size)
{
	struct pcb *pcb;
	int error;

	addr->sv_env.en_mxcsr &= cpu_mxcsr_mask;
	pcb = td->td_pcb;
	error = 0;
	critical_enter();
	if (td == PCPU_GET(fpcurthread) && PCB_USER_FPU(pcb)) {
		error = fpusetxstate(td, xfpustate, xfpustate_size);
		if (error == 0) {
			bcopy(addr, get_pcb_user_save_td(td), sizeof(*addr));
			fpurestore(get_pcb_user_save_td(td));
			set_pcb_flags(pcb, PCB_FPUINITDONE |
			    PCB_USERFPUINITDONE);
		}
	} else {
		error = fpusetxstate(td, xfpustate, xfpustate_size);
		if (error == 0) {
			bcopy(addr, get_pcb_user_save_td(td), sizeof(*addr));
			fpuuserinited(td);
		}
	}
	critical_exit();
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
	{ 0, 0 }
};

static driver_t fpupnp_driver = {
	"fpupnp",
	fpupnp_methods,
	1,			/* no softc */
};

DRIVER_MODULE(fpupnp, acpi, fpupnp_driver, 0, 0);
ISA_PNP_INFO(fpupnp_ids);
#endif	/* DEV_ISA */

static MALLOC_DEFINE(M_FPUKERN_CTX, "fpukern_ctx",
    "Kernel contexts for FPU state");

#define	FPU_KERN_CTX_FPUINITDONE 0x01
#define	FPU_KERN_CTX_DUMMY	 0x02	/* avoided save for the kern thread */
#define	FPU_KERN_CTX_INUSE	 0x04

struct fpu_kern_ctx {
	struct savefpu *prev;
	uint32_t flags;
	char hwstate1[];
};

static inline size_t __pure2
fpu_kern_alloc_sz(u_int max_est)
{
	return (sizeof(struct fpu_kern_ctx) + XSAVE_AREA_ALIGN + max_est);
}

static inline int __pure2
fpu_kern_malloc_flags(u_int fpflags)
{
	return (((fpflags & FPU_KERN_NOWAIT) ? M_NOWAIT : M_WAITOK) | M_ZERO);
}

struct fpu_kern_ctx *
fpu_kern_alloc_ctx_domain(int domain, u_int flags)
{
	return (malloc_domainset(fpu_kern_alloc_sz(cpu_max_ext_state_size),
	    M_FPUKERN_CTX, DOMAINSET_PREF(domain),
	    fpu_kern_malloc_flags(flags)));
}

struct fpu_kern_ctx *
fpu_kern_alloc_ctx(u_int flags)
{
	return (malloc(fpu_kern_alloc_sz(cpu_max_ext_state_size),
	    M_FPUKERN_CTX, fpu_kern_malloc_flags(flags)));
}

void
fpu_kern_free_ctx(struct fpu_kern_ctx *ctx)
{

	KASSERT((ctx->flags & FPU_KERN_CTX_INUSE) == 0, ("free'ing inuse ctx"));
	/* XXXKIB clear the memory ? */
	free(ctx, M_FPUKERN_CTX);
}

static struct savefpu *
fpu_kern_ctx_savefpu(struct fpu_kern_ctx *ctx)
{
	vm_offset_t p;

	p = (vm_offset_t)&ctx->hwstate1;
	p = roundup2(p, XSAVE_AREA_ALIGN);
	return ((struct savefpu *)p);
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
	KASSERT((pcb->pcb_flags & PCB_FPUNOSAVE) == 0,
	    ("recursive fpu_kern_enter while in PCB_FPUNOSAVE state"));

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
		 * PCB_FPUNOSAVE state is supposed to never need to
		 * save FPU context at all.
		 */
		fpurestore(fpu_initialstate);
		set_pcb_flags(pcb, PCB_KERNFPU | PCB_FPUNOSAVE |
		    PCB_FPUINITDONE);
		return;
	}
	if ((flags & FPU_KERN_KTHR) != 0 && is_fpu_kern_thread(0)) {
		ctx->flags = FPU_KERN_CTX_DUMMY | FPU_KERN_CTX_INUSE;
		return;
	}
	critical_enter();
	KASSERT(!PCB_USER_FPU(pcb) || pcb->pcb_save ==
	    get_pcb_user_save_pcb(pcb), ("mangled pcb_save"));
	ctx->flags = FPU_KERN_CTX_INUSE;
	if ((pcb->pcb_flags & PCB_FPUINITDONE) != 0)
		ctx->flags |= FPU_KERN_CTX_FPUINITDONE;
	fpuexit(td);
	ctx->prev = pcb->pcb_save;
	pcb->pcb_save = fpu_kern_ctx_savefpu(ctx);
	set_pcb_flags(pcb, PCB_KERNFPU);
	clear_pcb_flags(pcb, PCB_FPUINITDONE);
	critical_exit();
}

int
fpu_kern_leave(struct thread *td, struct fpu_kern_ctx *ctx)
{
	struct pcb *pcb;

	pcb = td->td_pcb;

	if ((pcb->pcb_flags & PCB_FPUNOSAVE) != 0) {
		KASSERT(ctx == NULL, ("non-null ctx after FPU_KERN_NOCTX"));
		KASSERT(PCPU_GET(fpcurthread) == NULL,
		    ("non-NULL fpcurthread for PCB_FPUNOSAVE"));
		CRITICAL_ASSERT(td);

		clear_pcb_flags(pcb,  PCB_FPUNOSAVE | PCB_FPUINITDONE);
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
			fpudrop();
		pcb->pcb_save = ctx->prev;
	}

	if (pcb->pcb_save == get_pcb_user_save_pcb(pcb)) {
		if ((pcb->pcb_flags & PCB_USERFPUINITDONE) != 0) {
			set_pcb_flags(pcb, PCB_FPUINITDONE);
			if ((pcb->pcb_flags & PCB_KERNFPU_THR) == 0)
				clear_pcb_flags(pcb, PCB_KERNFPU);
		} else if ((pcb->pcb_flags & PCB_KERNFPU_THR) == 0)
			clear_pcb_flags(pcb, PCB_FPUINITDONE | PCB_KERNFPU);
	} else {
		if ((ctx->flags & FPU_KERN_CTX_FPUINITDONE) != 0)
			set_pcb_flags(pcb, PCB_FPUINITDONE);
		else
			clear_pcb_flags(pcb, PCB_FPUINITDONE);
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

	set_pcb_flags(curpcb, PCB_KERNFPU | PCB_KERNFPU_THR);
	return (0);
}

int
is_fpu_kern_thread(u_int flags)
{

	if ((curthread->td_pflags & TDP_KTHREAD) == 0)
		return (0);
	return ((curpcb->pcb_flags & PCB_KERNFPU_THR) != 0);
}

/*
 * FPU save area alloc/free/init utility routines
 */
struct savefpu *
fpu_save_area_alloc(void)
{

	return (uma_zalloc(fpu_save_area_zone, M_WAITOK));
}

void
fpu_save_area_free(struct savefpu *fsa)
{

	uma_zfree(fpu_save_area_zone, fsa);
}

void
fpu_save_area_reset(struct savefpu *fsa)
{

	bcopy(fpu_initialstate, fsa, cpu_max_ext_state_size);
}

static __inline void
xsave_extfeature_check(uint64_t feature, bool supervisor)
{
#ifdef INVARIANTS
	uint64_t mask;

	mask = supervisor ? xsave_mask_supervisor : xsave_mask;
	KASSERT((feature & (feature - 1)) == 0,
	    ("%s: invalid XFEATURE 0x%lx", __func__, feature));
	KASSERT(ilog2(feature) <= ilog2(mask),
	    ("%s: unsupported %s XFEATURE 0x%lx", __func__,
	    supervisor ? "supervisor" : "user", feature));
#endif
}

static __inline void
xsave_extstate_bv_check(uint64_t xstate_bv, bool supervisor)
{
#ifdef INVARIANTS
	uint64_t mask;

	mask = supervisor ? xsave_mask_supervisor : xsave_mask;
	KASSERT(xstate_bv != 0 && ilog2(xstate_bv) <= ilog2(mask),
	    ("%s: invalid XSTATE_BV 0x%lx", __func__, xstate_bv));
#endif
}

/*
 * Returns whether the XFEATURE 'feature' is supported as a user state
 * or supervisor state component.
 */
bool
xsave_extfeature_supported(uint64_t feature, bool supervisor)
{
	int idx;
	uint64_t mask;

	KASSERT(use_xsave, ("%s: XSAVE not supported", __func__));
	xsave_extfeature_check(feature, supervisor);

	mask = supervisor ? xsave_mask_supervisor : xsave_mask;
	if ((mask & feature) == 0)
		return (false);
	idx = ilog2(feature);
	return (((xsave_area_desc[idx].flags & CPUID_EXTSTATE_SUPERVISOR) != 0) ==
	    supervisor);
}

/*
 * Returns whether the given XSAVE extension is supported.
 */
bool
xsave_extension_supported(uint64_t extension)
{
	KASSERT(use_xsave, ("%s: XSAVE not supported", __func__));

	return ((xsave_extensions & extension) != 0);
}

/*
 * Returns offset for XFEATURE 'feature' given the requested feature bitmap
 * 'xstate_bv', and extended region format ('compact').
 */
size_t
xsave_area_offset(uint64_t xstate_bv, uint64_t feature,
    bool compact, bool supervisor)
{
	int i, idx;
	size_t offs;
	struct xsave_area_elm_descr *xep;

	KASSERT(use_xsave, ("%s: XSAVE not supported", __func__));
	xsave_extstate_bv_check(xstate_bv, supervisor);
	xsave_extfeature_check(feature, supervisor);

	idx = ilog2(feature);
	if (!compact)
		return (xsave_area_desc[idx].offset);
	offs = sizeof(struct savefpu) + sizeof(struct xstate_hdr);
	xstate_bv &= ~(XFEATURE_ENABLED_X87 | XFEATURE_ENABLED_SSE);
	while ((i = ffs(xstate_bv) - 1) > 0 && i < idx) {
		xep = &xsave_area_desc[i];
		if ((xep->flags & CPUID_EXTSTATE_ALIGNED) != 0)
			offs = roundup2(offs, 64);
		offs += xep->size;
		xstate_bv &= ~((uint64_t)1 << i);
	}

	return (offs);
}

/*
 * Returns the XSAVE area size for the requested feature bitmap
 * 'xstate_bv' and extended region format ('compact').
 */
size_t
xsave_area_size(uint64_t xstate_bv, bool compact, bool supervisor)
{
	int last_idx;

	KASSERT(use_xsave, ("%s: XSAVE not supported", __func__));
	xsave_extstate_bv_check(xstate_bv, supervisor);

	last_idx = ilog2(xstate_bv);

	return (xsave_area_offset(xstate_bv, (uint64_t)1 << last_idx, compact, supervisor) +
	    xsave_area_desc[last_idx].size);
}

size_t
xsave_area_hdr_offset(void)
{
	return (sizeof(struct savefpu));
}
