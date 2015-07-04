/*-
 * Copyright (c) 2015 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
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
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/proc.h>

#include <machine/armreg.h>
#include <machine/pcb.h>
#include <machine/vfp.h>

/* Sanity check we can store all the VFP registers */
CTASSERT(sizeof(((struct pcb *)0)->pcb_vfp) == 16 * 32);

static void
vfp_enable(void)
{
	uint32_t cpacr;

	cpacr = READ_SPECIALREG(cpacr_el1);
	cpacr = (cpacr & ~CPACR_FPEN_MASK) | CPACR_FPEN_TRAP_NONE;
	WRITE_SPECIALREG(cpacr_el1, cpacr);
	isb();
}

static void
vfp_disable(void)
{
	uint32_t cpacr;

	cpacr = READ_SPECIALREG(cpacr_el1);
	cpacr = (cpacr & ~CPACR_FPEN_MASK) | CPACR_FPEN_TRAP_ALL1;
	WRITE_SPECIALREG(cpacr_el1, cpacr);
	isb();
}

/*
 * Called when the thread is dying. If the thread was the last to use the
 * VFP unit mark it as unused to tell the kernel the fp state is unowned.
 * Ensure the VFP unit is off so we get an exception on the next access.
 */
void
vfp_discard(struct thread *td)
{

	if (PCPU_GET(fpcurthread) == td)
		PCPU_SET(fpcurthread, NULL);

	vfp_disable();
}

void
vfp_save_state(struct thread *td)
{
	__int128_t *vfp_state;
	uint64_t fpcr, fpsr;
	uint32_t cpacr;

	critical_enter();
	/*
	 * Only store the registers if the VFP is enabled,
	 * i.e. return if we are trapping on FP access.
	 */
	cpacr = READ_SPECIALREG(cpacr_el1);
	if ((cpacr & CPACR_FPEN_MASK) == CPACR_FPEN_TRAP_NONE) {
		KASSERT(PCPU_GET(fpcurthread) == td,
		    ("Storing an invalid VFP state"));

		vfp_state = td->td_pcb->pcb_vfp;
		__asm __volatile(
		    "mrs	%0, fpcr		\n"
		    "mrs	%1, fpsr		\n"
		    "stp	q0,  q1,  [%2, #16 *  0]\n"
		    "stp	q2,  q3,  [%2, #16 *  2]\n"
		    "stp	q4,  q5,  [%2, #16 *  4]\n"
		    "stp	q6,  q7,  [%2, #16 *  6]\n"
		    "stp	q8,  q9,  [%2, #16 *  8]\n"
		    "stp	q10, q11, [%2, #16 * 10]\n"
		    "stp	q12, q13, [%2, #16 * 12]\n"
		    "stp	q14, q15, [%2, #16 * 14]\n"
		    "stp	q16, q17, [%2, #16 * 16]\n"
		    "stp	q18, q19, [%2, #16 * 18]\n"
		    "stp	q20, q21, [%2, #16 * 20]\n"
		    "stp	q22, q23, [%2, #16 * 22]\n"
		    "stp	q24, q25, [%2, #16 * 24]\n"
		    "stp	q26, q27, [%2, #16 * 26]\n"
		    "stp	q28, q29, [%2, #16 * 28]\n"
		    "stp	q30, q31, [%2, #16 * 30]\n"
		    : "=&r"(fpcr), "=&r"(fpsr) : "r"(vfp_state));

		td->td_pcb->pcb_fpcr = fpcr;
		td->td_pcb->pcb_fpsr = fpsr;

		dsb(ish);
		vfp_disable();
	}
	critical_exit();
}

void
vfp_restore_state(void)
{
	__int128_t *vfp_state;
	uint64_t fpcr, fpsr;
	struct pcb *curpcb;
	u_int cpu;

	critical_enter();

	cpu = PCPU_GET(cpuid);
	curpcb = curthread->td_pcb;
	curpcb->pcb_fpflags |= PCB_FP_STARTED;

	vfp_enable();

	/*
	 * If the previous thread on this cpu to use the VFP was not the
	 * current threas, or the current thread last used it on a different
	 * cpu we need to restore the old state.
	 */
	if (PCPU_GET(fpcurthread) != curthread || cpu != curpcb->pcb_vfpcpu) {

		vfp_state = curthread->td_pcb->pcb_vfp;
		fpcr = curthread->td_pcb->pcb_fpcr;
		fpsr = curthread->td_pcb->pcb_fpsr;

		__asm __volatile(
		    "ldp	q0,  q1,  [%2, #16 *  0]\n"
		    "ldp	q2,  q3,  [%2, #16 *  2]\n"
		    "ldp	q4,  q5,  [%2, #16 *  4]\n"
		    "ldp	q6,  q7,  [%2, #16 *  6]\n"
		    "ldp	q8,  q9,  [%2, #16 *  8]\n"
		    "ldp	q10, q11, [%2, #16 * 10]\n"
		    "ldp	q12, q13, [%2, #16 * 12]\n"
		    "ldp	q14, q15, [%2, #16 * 14]\n"
		    "ldp	q16, q17, [%2, #16 * 16]\n"
		    "ldp	q18, q19, [%2, #16 * 18]\n"
		    "ldp	q20, q21, [%2, #16 * 20]\n"
		    "ldp	q22, q23, [%2, #16 * 22]\n"
		    "ldp	q24, q25, [%2, #16 * 24]\n"
		    "ldp	q26, q27, [%2, #16 * 26]\n"
		    "ldp	q28, q29, [%2, #16 * 28]\n"
		    "ldp	q30, q31, [%2, #16 * 30]\n"
		    "msr	fpcr, %0		\n"
		    "msr	fpsr, %1		\n"
		    : : "r"(fpcr), "r"(fpsr), "r"(vfp_state));

		PCPU_SET(fpcurthread, curthread);
		curpcb->pcb_vfpcpu = cpu;
	}

	critical_exit();
}

void
vfp_init(void)
{
	uint64_t pfr;

	/* Check if there is a vfp unit present */
	pfr = READ_SPECIALREG(id_aa64pfr0_el1);
	if ((pfr & ID_AA64PFR0_FP_MASK) == ID_AA64PFR0_FP_NONE)
		return;

	/* Disable to be enabled when it's used */
	vfp_disable();
}

SYSINIT(vfp, SI_SUB_CPU, SI_ORDER_ANY, vfp_init, NULL);

#endif
