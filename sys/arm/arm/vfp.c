/*-
 * Copyright (c) 2014 Ian Lepore <ian@freebsd.org>
 * Copyright (c) 2012 Mark Tinguely
 *
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef VFP
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>

#include <machine/armreg.h>
#include <machine/frame.h>
#include <machine/fp.h>
#include <machine/pcb.h>
#include <machine/undefined.h>
#include <machine/vfp.h>

/* function prototypes */
static int vfp_bounce(u_int, u_int, struct trapframe *, int);
static void vfp_restore(struct vfp_state *);

extern int vfp_exists;
static struct undefined_handler vfp10_uh, vfp11_uh;
/* If true the VFP unit has 32 double registers, otherwise it has 16 */
static int is_d32;

/* The VFMXR command using coprocessor commands */
#define fmxr(reg, val) \
    __asm __volatile("mcr p10, 7, %0, " __STRING(reg) " , c0, 0" :: "r"(val));

/* The VFMRX command using coprocessor commands */
#define fmrx(reg) \
({ u_int val = 0;\
    __asm __volatile("mrc p10, 7, %0, " __STRING(reg) " , c0, 0" : "=r"(val));\
    val; \
})

/*
 * Work around an issue with GCC where the asm it generates is not unified
 * syntax and fails to assemble because it expects the ldcleq instruction in the
 * form ldc<c>l, not in the UAL form ldcl<c>, and similar for stcleq.
 */
#ifdef __clang__
#define	LDCLNE  "ldclne "
#define	STCLNE  "stclne "
#else
#define	LDCLNE  "ldcnel "
#define	STCLNE  "stcnel "
#endif

static u_int
get_coprocessorACR(void)
{
	u_int val;
	__asm __volatile("mrc p15, 0, %0, c1, c0, 2" : "=r" (val) : : "cc");
	return val;
}

static void
set_coprocessorACR(u_int val)
{
	__asm __volatile("mcr p15, 0, %0, c1, c0, 2\n\t"
	 : : "r" (val) : "cc");
	isb();
}


	/* called for each cpu */
void
vfp_init(void)
{
	u_int fpsid, fpexc, tmp;
	u_int coproc, vfp_arch;

	coproc = get_coprocessorACR();
	coproc |= COPROC10 | COPROC11;
	set_coprocessorACR(coproc);
	
	fpsid = fmrx(VFPSID);		/* read the vfp system id */
	fpexc = fmrx(VFPEXC);		/* read the vfp exception reg */

	if (!(fpsid & VFPSID_HARDSOFT_IMP)) {
		vfp_exists = 1;
		is_d32 = 0;
		PCPU_SET(vfpsid, fpsid);	/* save the VFPSID */

		vfp_arch =
		    (fpsid & VFPSID_SUBVERSION2_MASK) >> VFPSID_SUBVERSION_OFF;

		if (vfp_arch >= VFP_ARCH3) {
			tmp = fmrx(VMVFR0);
			PCPU_SET(vfpmvfr0, tmp);

			if ((tmp & VMVFR0_RB_MASK) == 2)
				is_d32 = 1;

			tmp = fmrx(VMVFR1);
			PCPU_SET(vfpmvfr1, tmp);
		}

		/* initialize the coprocess 10 and 11 calls
		 * These are called to restore the registers and enable
		 * the VFP hardware.
		 */
		if (vfp10_uh.uh_handler == NULL) {
			vfp10_uh.uh_handler = vfp_bounce;
			vfp11_uh.uh_handler = vfp_bounce;
			install_coproc_handler_static(10, &vfp10_uh);
			install_coproc_handler_static(11, &vfp11_uh);
		}
	}
}

SYSINIT(vfp, SI_SUB_CPU, SI_ORDER_ANY, vfp_init, NULL);


/* start VFP unit, restore the vfp registers from the PCB  and retry
 * the instruction
 */
static int
vfp_bounce(u_int addr, u_int insn, struct trapframe *frame, int code)
{
	u_int cpu, fpexc;
	struct pcb *curpcb;

	if ((code & FAULT_USER) == 0)
		panic("undefined floating point instruction in supervisor mode");

	critical_enter();

	/*
	 * If the VFP is already on and we got an undefined instruction, then
	 * something tried to executate a truly invalid instruction that maps to
	 * the VFP.
	 */
	fpexc = fmrx(VFPEXC);
	if (fpexc & VFPEXC_EN) {
		/* kill the process - we do not handle emulation */
		critical_exit();
		killproc(curthread->td_proc, "vfp emulation");
		return 1;
	}

	/*
	 * If the last time this thread used the VFP it was on this core, and
	 * the last thread to use the VFP on this core was this thread, then the
	 * VFP state is valid, otherwise restore this thread's state to the VFP.
	 */
	fmxr(VFPEXC, fpexc | VFPEXC_EN);
	curpcb = curthread->td_pcb;
	cpu = PCPU_GET(cpu);
	if (curpcb->pcb_vfpcpu != cpu || curthread != PCPU_GET(fpcurthread)) {
		vfp_restore(&curpcb->pcb_vfpstate);
		curpcb->pcb_vfpcpu = cpu;
		PCPU_SET(fpcurthread, curthread);
	}

	critical_exit();
	return (0);
}

/*
 * Restore the given state to the VFP hardware.
 */
static void
vfp_restore(struct vfp_state *vfpsave)
{
	u_int vfpscr = 0;

	__asm __volatile("ldc	p10, c0, [%1], #128\n" /* d0-d15 */
			"cmp	%2, #0\n"		/* -D16 or -D32? */
			LDCLNE "p11, c0, [%1], #128\n"	/* d16-d31 */
			"addeq	%1, %1, #128\n"		/* skip missing regs */
			"ldr	%0, [%1]\n"		/* set old vfpscr */
			"mcr	p10, 7, %0, cr1, c0, 0\n"
			: "=&r" (vfpscr) : "r" (vfpsave), "r" (is_d32) : "cc");
}

/*
 * If the VFP is on, save its current state and turn it off if requested to do
 * so.  If the VFP is not on, does not change the values at *vfpsave.  Caller is
 * responsible for preventing a context switch while this is running.
 */
void
vfp_store(struct vfp_state *vfpsave, boolean_t disable_vfp)
{
	u_int tmp, vfpscr;

	tmp = fmrx(VFPEXC);		/* Is the vfp enabled? */
	if (tmp & VFPEXC_EN) {
		__asm __volatile(
			"stc	p11, c0, [%1], #128\n"  /* d0-d15 */
			"cmp	%2, #0\n"		/* -D16 or -D32? */
			STCLNE "p11, c0, [%1], #128\n"	/* d16-d31 */
			"addeq	%1, %1, #128\n"		/* skip missing regs */
			"mrc	p10, 7, %0, cr1, c0, 0\n" /* fmxr(VFPSCR) */
			"str	%0, [%1]\n"		/* save vfpscr */
			: "=&r" (vfpscr) : "r" (vfpsave), "r" (is_d32) : "cc");
		if (disable_vfp)
			fmxr(VFPEXC , tmp & ~VFPEXC_EN);
	}
}

/*
 * If the VFP hardware is on, the current thread was using it but now that
 * thread is dying.  Turn off the VFP and set pcpu fpcurthread to 0, to indicate
 * that the VFP hardware state does not belong to any thread.   Called only from
 * cpu_throw(), so we don't have to worry about a context switch here.
 */
void
vfp_discard()
{
	u_int tmp;

	tmp = fmrx(VFPEXC);
	if (tmp & VFPEXC_EN) {
		fmxr(VFPEXC, tmp & ~VFPEXC_EN);
		PCPU_SET(fpcurthread, 0);
	}
}

#endif

