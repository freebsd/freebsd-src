/*-
 * Copyright (c) 2015-2016 The FreeBSD Foundation
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
#ifdef VFP
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/elf.h>
#include <sys/eventhandler.h>
#include <sys/limits.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/reg.h>
#include <sys/smp.h>

#include <vm/uma.h>

#include <machine/armreg.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/vfp.h>

/* Sanity check we can store all the VFP registers */
CTASSERT(sizeof(((struct pcb *)0)->pcb_fpustate.vfp_regs) == 16 * 32);

static MALLOC_DEFINE(M_FPUKERN_CTX, "fpukern_ctx",
    "Kernel contexts for VFP state");

struct fpu_kern_ctx {
	struct vfpstate	*prev;
#define	FPU_KERN_CTX_DUMMY	0x01	/* avoided save for the kern thread */
#define	FPU_KERN_CTX_INUSE	0x02
	uint32_t	 flags;
	struct vfpstate	 state;
};

static uma_zone_t fpu_save_area_zone;
static struct vfpstate *fpu_initialstate;

static u_int sve_max_vector_len;

static size_t
_sve_buf_size(u_int sve_len)
{
	size_t len;

	/* 32 vector registers */
	len = (size_t)sve_len * 32;
	/*
	 * 16 predicate registers and the fault fault register, each 1/8th
	 * the size of a vector register.
	 */
	len += ((size_t)sve_len * 17) / 8;
	/*
	 * FPSR and FPCR
	 */
	len += sizeof(uint64_t) * 2;

	return (len);
}

size_t
sve_max_buf_size(void)
{
	MPASS(sve_max_vector_len > 0);
	return (_sve_buf_size(sve_max_vector_len));
}

size_t
sve_buf_size(struct thread *td)
{
	struct pcb *pcb;

	pcb = td->td_pcb;
	MPASS(pcb->pcb_svesaved != NULL);
	MPASS(pcb->pcb_sve_len > 0);

	return (_sve_buf_size(pcb->pcb_sve_len));
}

static void *
sve_alloc(void)
{
	void *buf;

	buf = malloc(sve_max_buf_size(), M_FPUKERN_CTX, M_WAITOK | M_ZERO);

	return (buf);
}

static void
sve_free(void *buf)
{
	free(buf, M_FPUKERN_CTX);
}

void
vfp_enable(void)
{
	uint32_t cpacr;

	cpacr = READ_SPECIALREG(cpacr_el1);
	cpacr = (cpacr & ~CPACR_FPEN_MASK) | CPACR_FPEN_TRAP_NONE;
	WRITE_SPECIALREG(cpacr_el1, cpacr);
	isb();
}

static void
sve_enable(void)
{
	uint32_t cpacr;

	cpacr = READ_SPECIALREG(cpacr_el1);
	/* Enable FP */
	cpacr = (cpacr & ~CPACR_FPEN_MASK) | CPACR_FPEN_TRAP_NONE;
	/* Enable SVE */
	cpacr = (cpacr & ~CPACR_ZEN_MASK) | CPACR_ZEN_TRAP_NONE;
	WRITE_SPECIALREG(cpacr_el1, cpacr);
	isb();
}

void
vfp_disable(void)
{
	uint32_t cpacr;

	cpacr = READ_SPECIALREG(cpacr_el1);
	/* Disable FP */
	cpacr = (cpacr & ~CPACR_FPEN_MASK) | CPACR_FPEN_TRAP_ALL1;
	/* Disable SVE */
	cpacr = (cpacr & ~CPACR_ZEN_MASK) | CPACR_ZEN_TRAP_ALL1;
	WRITE_SPECIALREG(cpacr_el1, cpacr);
	isb();
}

/*
 * Called when the thread is dying or when discarding the kernel VFP state.
 * If the thread was the last to use the VFP unit mark it as unused to tell
 * the kernel the fp state is unowned. Ensure the VFP unit is off so we get
 * an exception on the next access.
 */
void
vfp_discard(struct thread *td)
{

#ifdef INVARIANTS
	if (td != NULL)
		CRITICAL_ASSERT(td);
#endif
	if (PCPU_GET(fpcurthread) == td)
		PCPU_SET(fpcurthread, NULL);

	vfp_disable();
}

void
vfp_store(struct vfpstate *state)
{
	__uint128_t *vfp_state;
	uint64_t fpcr, fpsr;

	vfp_state = state->vfp_regs;
	__asm __volatile(
	    ".arch_extension fp\n"
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
	    ".arch_extension nofp\n"
	    : "=&r"(fpcr), "=&r"(fpsr) : "r"(vfp_state));

	state->vfp_fpcr = fpcr;
	state->vfp_fpsr = fpsr;
}

void
vfp_restore(struct vfpstate *state)
{
	__uint128_t *vfp_state;
	uint64_t fpcr, fpsr;

	vfp_state = state->vfp_regs;
	fpcr = state->vfp_fpcr;
	fpsr = state->vfp_fpsr;

	__asm __volatile(
	    ".arch_extension fp\n"
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
	    ".arch_extension nofp\n"
	    : : "r"(fpcr), "r"(fpsr), "r"(vfp_state));
}

static void
sve_store(void *state, u_int sve_len)
{
	vm_offset_t f_start, p_start, z_start;
	uint64_t fpcr, fpsr;

	/*
	 * Calculate the start of each register groups. There are three
	 * groups depending on size, with the First Fault Register (FFR)
	 * stored with the predicate registers as we use one of them to
	 * temporarily hold it.
	 *
	 *                 +-------------------------+-------------------+
	 *                 | Contents                | Register size     |
	 *      z_start -> +-------------------------+-------------------+
	 *                 |                         |                   |
	 *                 | 32 Z regs               | sve_len           |
	 *                 |                         |                   |
	 *      p_start -> +-------------------------+-------------------+
	 *                 |                         |                   |
	 *                 | 16 Predicate registers  | 1/8 size of Z reg |
	 *                 |  1 First Fault register |                   |
	 *                 |                         |                   |
	 *      f_start -> +-------------------------+-------------------+
	 *                 |                         |                   |
	 *                 | FPSR/FPCR               | 32 bit            |
	 *                 |                         |                   |
	 *                 +-------------------------+-------------------+
	 */
	z_start = (vm_offset_t)state;
	p_start = z_start + sve_len * 32;
	f_start = p_start + (sve_len / 8) * 17;

	__asm __volatile(
	    ".arch_extension sve				\n"
	    "str	z0, [%0, #0, MUL VL]			\n"
	    "str	z1, [%0, #1, MUL VL]			\n"
	    "str	z2, [%0, #2, MUL VL]			\n"
	    "str	z3, [%0, #3, MUL VL]			\n"
	    "str	z4, [%0, #4, MUL VL]			\n"
	    "str	z5, [%0, #5, MUL VL]			\n"
	    "str	z6, [%0, #6, MUL VL]			\n"
	    "str	z7, [%0, #7, MUL VL]			\n"
	    "str	z8, [%0, #8, MUL VL]			\n"
	    "str	z9, [%0, #9, MUL VL]			\n"
	    "str	z10, [%0, #10, MUL VL]			\n"
	    "str	z11, [%0, #11, MUL VL]			\n"
	    "str	z12, [%0, #12, MUL VL]			\n"
	    "str	z13, [%0, #13, MUL VL]			\n"
	    "str	z14, [%0, #14, MUL VL]			\n"
	    "str	z15, [%0, #15, MUL VL]			\n"
	    "str	z16, [%0, #16, MUL VL]			\n"
	    "str	z17, [%0, #17, MUL VL]			\n"
	    "str	z18, [%0, #18, MUL VL]			\n"
	    "str	z19, [%0, #19, MUL VL]			\n"
	    "str	z20, [%0, #20, MUL VL]			\n"
	    "str	z21, [%0, #21, MUL VL]			\n"
	    "str	z22, [%0, #22, MUL VL]			\n"
	    "str	z23, [%0, #23, MUL VL]			\n"
	    "str	z24, [%0, #24, MUL VL]			\n"
	    "str	z25, [%0, #25, MUL VL]			\n"
	    "str	z26, [%0, #26, MUL VL]			\n"
	    "str	z27, [%0, #27, MUL VL]			\n"
	    "str	z28, [%0, #28, MUL VL]			\n"
	    "str	z29, [%0, #29, MUL VL]			\n"
	    "str	z30, [%0, #30, MUL VL]			\n"
	    "str	z31, [%0, #31, MUL VL]			\n"
	    /* Store the predicate registers */
	    "str	p0, [%1, #0, MUL VL]			\n"
	    "str	p1, [%1, #1, MUL VL]			\n"
	    "str	p2, [%1, #2, MUL VL]			\n"
	    "str	p3, [%1, #3, MUL VL]			\n"
	    "str	p4, [%1, #4, MUL VL]			\n"
	    "str	p5, [%1, #5, MUL VL]			\n"
	    "str	p6, [%1, #6, MUL VL]			\n"
	    "str	p7, [%1, #7, MUL VL]			\n"
	    "str	p8, [%1, #8, MUL VL]			\n"
	    "str	p9, [%1, #9, MUL VL]			\n"
	    "str	p10, [%1, #10, MUL VL]			\n"
	    "str	p11, [%1, #11, MUL VL]			\n"
	    "str	p12, [%1, #12, MUL VL]			\n"
	    "str	p13, [%1, #13, MUL VL]			\n"
	    "str	p14, [%1, #14, MUL VL]			\n"
	    "str	p15, [%1, #15, MUL VL]			\n"
	    ".arch_extension nosve				\n"
	    : : "r"(z_start), "r"(p_start));

	/* Save the FFR if needed */
	/* TODO: Skip if in SME streaming mode (when supported) */
	__asm __volatile(
	    ".arch_extension sve				\n"
	    "rdffr	p0.b					\n"
	    "str	p0, [%0, #16, MUL VL]			\n"
	/*
	 * Load the old p0 value to ensure it is consistent if we enable
	 * without calling sve_restore, e.g. switch to a kernel thread and
	 * back.
	 */
	    "ldr	p0, [%0, #0, MUL VL]			\n"
	    ".arch_extension nosve				\n"
	    : : "r"(p_start));

	__asm __volatile(
	    ".arch_extension fp					\n"
	    "mrs	%0, fpsr				\n"
	    "mrs	%1, fpcr				\n"
	    "stp	%w0, %w1, [%2]				\n"
	    ".arch_extension nofp				\n"
	    : "=&r"(fpsr), "=&r"(fpcr) : "r"(f_start));
}

static void
sve_restore(void *state, u_int sve_len)
{
	vm_offset_t f_start, p_start, z_start;
	uint64_t fpcr, fpsr;

	/* See sve_store for the layout of the state buffer */
	z_start = (vm_offset_t)state;
	p_start = z_start + sve_len * 32;
	f_start = p_start + (sve_len / 8) * 17;

	__asm __volatile(
	    ".arch_extension sve				\n"
	    "ldr	p0, [%0, #16, MUL VL]			\n"
	    "wrffr	p0.b					\n"
	    ".arch_extension nosve				\n"
	    : : "r"(p_start));

	__asm __volatile(
	    ".arch_extension sve				\n"
	    "ldr	z0, [%0, #0, MUL VL]			\n"
	    "ldr	z1, [%0, #1, MUL VL]			\n"
	    "ldr	z2, [%0, #2, MUL VL]			\n"
	    "ldr	z3, [%0, #3, MUL VL]			\n"
	    "ldr	z4, [%0, #4, MUL VL]			\n"
	    "ldr	z5, [%0, #5, MUL VL]			\n"
	    "ldr	z6, [%0, #6, MUL VL]			\n"
	    "ldr	z7, [%0, #7, MUL VL]			\n"
	    "ldr	z8, [%0, #8, MUL VL]			\n"
	    "ldr	z9, [%0, #9, MUL VL]			\n"
	    "ldr	z10, [%0, #10, MUL VL]			\n"
	    "ldr	z11, [%0, #11, MUL VL]			\n"
	    "ldr	z12, [%0, #12, MUL VL]			\n"
	    "ldr	z13, [%0, #13, MUL VL]			\n"
	    "ldr	z14, [%0, #14, MUL VL]			\n"
	    "ldr	z15, [%0, #15, MUL VL]			\n"
	    "ldr	z16, [%0, #16, MUL VL]			\n"
	    "ldr	z17, [%0, #17, MUL VL]			\n"
	    "ldr	z18, [%0, #18, MUL VL]			\n"
	    "ldr	z19, [%0, #19, MUL VL]			\n"
	    "ldr	z20, [%0, #20, MUL VL]			\n"
	    "ldr	z21, [%0, #21, MUL VL]			\n"
	    "ldr	z22, [%0, #22, MUL VL]			\n"
	    "ldr	z23, [%0, #23, MUL VL]			\n"
	    "ldr	z24, [%0, #24, MUL VL]			\n"
	    "ldr	z25, [%0, #25, MUL VL]			\n"
	    "ldr	z26, [%0, #26, MUL VL]			\n"
	    "ldr	z27, [%0, #27, MUL VL]			\n"
	    "ldr	z28, [%0, #28, MUL VL]			\n"
	    "ldr	z29, [%0, #29, MUL VL]			\n"
	    "ldr	z30, [%0, #30, MUL VL]			\n"
	    "ldr	z31, [%0, #31, MUL VL]			\n"
	    /* Store the predicate registers */
	    "ldr	p0, [%1, #0, MUL VL]			\n"
	    "ldr	p1, [%1, #1, MUL VL]			\n"
	    "ldr	p2, [%1, #2, MUL VL]			\n"
	    "ldr	p3, [%1, #3, MUL VL]			\n"
	    "ldr	p4, [%1, #4, MUL VL]			\n"
	    "ldr	p5, [%1, #5, MUL VL]			\n"
	    "ldr	p6, [%1, #6, MUL VL]			\n"
	    "ldr	p7, [%1, #7, MUL VL]			\n"
	    "ldr	p8, [%1, #8, MUL VL]			\n"
	    "ldr	p9, [%1, #9, MUL VL]			\n"
	    "ldr	p10, [%1, #10, MUL VL]			\n"
	    "ldr	p11, [%1, #11, MUL VL]			\n"
	    "ldr	p12, [%1, #12, MUL VL]			\n"
	    "ldr	p13, [%1, #13, MUL VL]			\n"
	    "ldr	p14, [%1, #14, MUL VL]			\n"
	    "ldr	p15, [%1, #15, MUL VL]			\n"
	    ".arch_extension nosve				\n"
	    : : "r"(z_start), "r"(p_start));

	__asm __volatile(
	    ".arch_extension fp					\n"
	    "ldp	%w0, %w1, [%2]				\n"
	    "msr	fpsr, %0				\n"
	    "msr	fpcr, %1				\n"
	    ".arch_extension nofp				\n"
	    : "=&r"(fpsr), "=&r"(fpcr) : "r"(f_start));
}

/*
 * Sync the VFP registers to the SVE register state, e.g. in signal return
 * when userspace may have changed the vfp register values and expect them
 * to be used when the signal handler returns.
 */
void
vfp_to_sve_sync(struct thread *td)
{
	struct pcb *pcb;
	uint32_t *fpxr;

	pcb = td->td_pcb;
	if (pcb->pcb_svesaved == NULL)
		return;

	MPASS(pcb->pcb_fpusaved != NULL);

	/* Copy the VFP registers to the SVE region */
	for (int i = 0; i < nitems(pcb->pcb_fpusaved->vfp_regs); i++) {
		__uint128_t *sve_reg;

		sve_reg = (__uint128_t *)((uintptr_t)pcb->pcb_svesaved +
		    i * pcb->pcb_sve_len);
		*sve_reg = pcb->pcb_fpusaved->vfp_regs[i];
	}

	fpxr = (uint32_t *)((uintptr_t)pcb->pcb_svesaved +
	    (32 * pcb->pcb_sve_len) + (17 * pcb->pcb_sve_len / 8));
	fpxr[0] = pcb->pcb_fpusaved->vfp_fpsr;
	fpxr[1] = pcb->pcb_fpusaved->vfp_fpcr;
}

/*
 * Sync the SVE registers to the VFP register state.
 */
void
sve_to_vfp_sync(struct thread *td)
{
	struct pcb *pcb;
	uint32_t *fpxr;

	pcb = td->td_pcb;
	if (pcb->pcb_svesaved == NULL)
		return;

	MPASS(pcb->pcb_fpusaved == &pcb->pcb_fpustate);

	/* Copy the SVE registers to the VFP saved state */
	for (int i = 0; i < nitems(pcb->pcb_fpusaved->vfp_regs); i++) {
		__uint128_t *sve_reg;

		sve_reg = (__uint128_t *)((uintptr_t)pcb->pcb_svesaved +
		    i * pcb->pcb_sve_len);
		pcb->pcb_fpusaved->vfp_regs[i] = *sve_reg;
	}

	fpxr = (uint32_t *)((uintptr_t)pcb->pcb_svesaved +
	    (32 * pcb->pcb_sve_len) + (17 * pcb->pcb_sve_len / 8));
	pcb->pcb_fpusaved->vfp_fpsr = fpxr[0];
	pcb->pcb_fpusaved->vfp_fpcr = fpxr[1];
}

static void
vfp_save_state_common(struct thread *td, struct pcb *pcb, bool full_save)
{
	uint32_t cpacr;
	bool save_sve;

	save_sve = false;

	critical_enter();
	/*
	 * Only store the registers if the VFP is enabled,
	 * i.e. return if we are trapping on FP access.
	 */
	cpacr = READ_SPECIALREG(cpacr_el1);
	if ((cpacr & CPACR_FPEN_MASK) != CPACR_FPEN_TRAP_NONE)
		goto done;

	KASSERT(PCPU_GET(fpcurthread) == td,
	    ("Storing an invalid VFP state"));

	/*
	 * Also save the SVE state. As SVE depends on the VFP being
	 * enabled we can rely on only needing to check this when
	 * the VFP unit has been enabled.
	 */
	if ((cpacr & CPACR_ZEN_MASK) == CPACR_ZEN_TRAP_NONE) {
		/* If SVE is enabled it should be valid */
		MPASS((pcb->pcb_fpflags & PCB_FP_SVEVALID) != 0);

		/*
		 * If we are switching while in a system call skip saving
		 * SVE registers. The ABI allows us to drop them over any
		 * system calls, however doing so is expensive in SVE
		 * heavy userspace code. This would require us to disable
		 * SVE for all system calls and trap the next use of them.
		 * As an optimisation only disable SVE on context switch.
		 */
		if (td->td_frame == NULL ||
		    (ESR_ELx_EXCEPTION(td->td_frame->tf_esr) != EXCP_SVC64 &&
		    td->td_sa.code != (u_int)-1))
			save_sve = true;
	}

	if (save_sve) {
		KASSERT(pcb->pcb_svesaved != NULL,
		    ("Storing to a NULL SVE state"));
		sve_store(pcb->pcb_svesaved, pcb->pcb_sve_len);
		if (full_save)
			sve_to_vfp_sync(td);
	} else {
		pcb->pcb_fpflags &= ~PCB_FP_SVEVALID;
		vfp_store(pcb->pcb_fpusaved);
	}
	dsb(ish);
	vfp_disable();

done:
	critical_exit();
}

void
vfp_save_state(struct thread *td, struct pcb *pcb)
{
	KASSERT(td != NULL, ("NULL vfp thread"));
	KASSERT(pcb != NULL, ("NULL vfp pcb"));
	KASSERT(td->td_pcb == pcb, ("Invalid vfp pcb"));

	vfp_save_state_common(td, pcb, true);
}

void
vfp_save_state_savectx(struct pcb *pcb)
{
	/*
	 * savectx() will be called on panic with dumppcb as an argument,
	 * dumppcb either has no pcb_fpusaved set or it was previously set
	 * to its own fpu state.
	 *
	 * In both cases we can set it here to the pcb fpu state.
	 */
	MPASS(pcb->pcb_fpusaved == NULL ||
	    pcb->pcb_fpusaved == &pcb->pcb_fpustate);
	pcb->pcb_fpusaved = &pcb->pcb_fpustate;

	vfp_save_state_common(curthread, pcb, true);
}

void
vfp_save_state_switch(struct thread *td)
{
	KASSERT(td != NULL, ("NULL vfp thread"));

	vfp_save_state_common(td, td->td_pcb, false);
}

/*
 * Update the VFP state for a forked process or new thread. The PCB will
 * have been copied from the old thread.
 */
void
vfp_new_thread(struct thread *newtd, struct thread *oldtd, bool fork)
{
	struct pcb *newpcb, *oldpcb;

	newpcb = newtd->td_pcb;
	oldpcb = oldtd->td_pcb;

	/* Kernel threads start with clean VFP */
	if ((oldtd->td_pflags & TDP_KTHREAD) != 0) {
		newpcb->pcb_fpflags &=
		    ~(PCB_FP_STARTED | PCB_FP_SVEVALID | PCB_FP_KERN |
		      PCB_FP_NOSAVE);
	} else {
		MPASS((newpcb->pcb_fpflags & (PCB_FP_KERN|PCB_FP_NOSAVE)) == 0);

		/*
		 * The only SVE register state to be guaranteed to be saved
		 * a system call is the lower bits of the Z registers as
		 * these are aliased with the existing FP registers. Because
		 * we can only create a new thread or fork through a system
		 * call it is safe to drop the SVE state in the new thread.
		 */
		newpcb->pcb_fpflags &= ~PCB_FP_SVEVALID;
		if (!fork) {
			newpcb->pcb_fpflags &= ~PCB_FP_STARTED;
		}
	}

	newpcb->pcb_svesaved = NULL;
	if (oldpcb->pcb_svesaved == NULL)
		newpcb->pcb_sve_len = sve_max_vector_len;
	else
		KASSERT(newpcb->pcb_sve_len == oldpcb->pcb_sve_len,
		    ("%s: pcb sve vector length differs: %x != %x", __func__,
		    newpcb->pcb_sve_len, oldpcb->pcb_sve_len));

	newpcb->pcb_fpusaved = &newpcb->pcb_fpustate;
	newpcb->pcb_vfpcpu = UINT_MAX;
}

/*
 * Reset the FP state to avoid leaking state from the parent process across
 * execve() (and to ensure that we get a consistent floating point environment
 * in every new process).
 */
void
vfp_reset_state(struct thread *td, struct pcb *pcb)
{
	/* Discard the threads VFP state before resetting it */
	critical_enter();
	vfp_discard(td);
	critical_exit();

	/*
	 * Clear the thread state. The VFP is disabled and is not the current
	 * VFP thread so we won't change any of these on context switch.
	 */
	bzero(&pcb->pcb_fpustate.vfp_regs, sizeof(pcb->pcb_fpustate.vfp_regs));
	KASSERT(pcb->pcb_fpusaved == &pcb->pcb_fpustate,
	    ("pcb_fpusaved should point to pcb_fpustate."));
	pcb->pcb_fpustate.vfp_fpcr = VFPCR_INIT;
	pcb->pcb_fpustate.vfp_fpsr = 0;
	/* XXX: Memory leak when using SVE between fork & exec? */
	pcb->pcb_svesaved = NULL;
	pcb->pcb_vfpcpu = UINT_MAX;
	pcb->pcb_fpflags = 0;
}

static void
vfp_restore_state_common(struct thread *td, int flags)
{
	struct pcb *curpcb;
	u_int cpu;
	bool restore_sve;

	KASSERT(td == curthread, ("%s: Called with non-current thread",
	    __func__));

	critical_enter();

	cpu = PCPU_GET(cpuid);
	curpcb = td->td_pcb;

	/*
	 * If SVE has been used and the base VFP state is in use then
	 * restore the SVE registers. A non-base VFP state should only
	 * be used by the kernel and SVE should onlu be used by userspace.
	 */
	restore_sve = false;
	if ((curpcb->pcb_fpflags & PCB_FP_SVEVALID) != 0 &&
	    curpcb->pcb_fpusaved == &curpcb->pcb_fpustate) {
		MPASS(curpcb->pcb_svesaved != NULL);
		/* SVE shouldn't be enabled in the kernel */
		MPASS((flags & PCB_FP_KERN) == 0);
		restore_sve = true;
	}

	if (restore_sve) {
		MPASS((curpcb->pcb_fpflags & PCB_FP_SVEVALID) != 0);
		sve_enable();
	} else {
		curpcb->pcb_fpflags |= PCB_FP_STARTED;
		vfp_enable();
	}

	/*
	 * If the previous thread on this cpu to use the VFP was not the
	 * current thread, or the current thread last used it on a different
	 * cpu we need to restore the old state.
	 */
	if (PCPU_GET(fpcurthread) != curthread || cpu != curpcb->pcb_vfpcpu) {
		/*
		 * The VFP registers are the lower 128 bits of the SVE
		 * registers. Use the SVE store state if it was previously
		 * enabled.
		 */
		if (restore_sve) {
			MPASS(td->td_pcb->pcb_svesaved != NULL);
			sve_restore(td->td_pcb->pcb_svesaved,
			    td->td_pcb->pcb_sve_len);
		} else {
			vfp_restore(td->td_pcb->pcb_fpusaved);
		}
		PCPU_SET(fpcurthread, td);
		curpcb->pcb_vfpcpu = cpu;
	}

	critical_exit();
}

void
vfp_restore_state(void)
{
	struct thread *td;

	td = curthread;
	vfp_restore_state_common(td, td->td_pcb->pcb_fpflags);
}

bool
sve_restore_state(struct thread *td)
{
	struct pcb *curpcb;
	void *svesaved;
	uint64_t cpacr;

	KASSERT(td == curthread, ("%s: Called with non-current thread",
	    __func__));

	curpcb = td->td_pcb;

	/* The SVE state should alias the base VFP state */
	MPASS(curpcb->pcb_fpusaved == &curpcb->pcb_fpustate);

	/* SVE not enabled, tell the caller to raise a fault */
	if (curpcb->pcb_sve_len == 0) {
		/*
		 * The init pcb is created before we read the vector length.
		 * Set it to the default length.
		 */
		if (sve_max_vector_len == 0)
			return (false);

		MPASS(curpcb->pcb_svesaved == NULL);
		curpcb->pcb_sve_len = sve_max_vector_len;
	}

	if (curpcb->pcb_svesaved == NULL) {
		/* SVE should be disabled so will be invalid */
		MPASS((curpcb->pcb_fpflags & PCB_FP_SVEVALID) == 0);

		/*
		 * Allocate the SVE buffer of this thread.
		 * Enable interrupts so the allocation can sleep
		 */
		svesaved = sve_alloc();

		critical_enter();

		/* Restore the VFP state if needed */
		cpacr = READ_SPECIALREG(cpacr_el1);
		if ((cpacr & CPACR_FPEN_MASK) != CPACR_FPEN_TRAP_NONE) {
			vfp_restore_state_common(td, curpcb->pcb_fpflags);
		}

		/*
		 * Set the flags after enabling the VFP as the SVE saved
		 * state will be invalid.
		 */
		curpcb->pcb_svesaved = svesaved;
		curpcb->pcb_fpflags |= PCB_FP_SVEVALID;
		sve_enable();

		critical_exit();
	} else {
		vfp_restore_state_common(td, curpcb->pcb_fpflags);

		/* Enable SVE if it wasn't previously enabled */
		if ((curpcb->pcb_fpflags & PCB_FP_SVEVALID) == 0) {
			critical_enter();
			sve_enable();
			curpcb->pcb_fpflags |= PCB_FP_SVEVALID;
			critical_exit();
		}
	}

	return (true);
}

void
vfp_init_secondary(void)
{
	uint64_t pfr;

	/* Check if there is a vfp unit present */
	pfr = READ_SPECIALREG(id_aa64pfr0_el1);
	if ((pfr & ID_AA64PFR0_FP_MASK) == ID_AA64PFR0_FP_NONE)
		return;

	/* Disable to be enabled when it's used */
	vfp_disable();
}

static void
vfp_init(const void *dummy __unused)
{
	uint64_t pfr;

	/* Check if there is a vfp unit present */
	pfr = READ_SPECIALREG(id_aa64pfr0_el1);
	if ((pfr & ID_AA64PFR0_FP_MASK) == ID_AA64PFR0_FP_NONE)
		return;

	fpu_save_area_zone = uma_zcreate("VFP_save_area",
	    sizeof(struct vfpstate), NULL, NULL, NULL, NULL,
	    _Alignof(struct vfpstate) - 1, 0);
	fpu_initialstate = uma_zalloc(fpu_save_area_zone, M_WAITOK | M_ZERO);

	/* Ensure the VFP is enabled before accessing it in vfp_store */
	vfp_enable();
	vfp_store(fpu_initialstate);

	/* Disable to be enabled when it's used */
	vfp_disable();

	/* Zero the VFP registers but keep fpcr and fpsr */
	bzero(fpu_initialstate->vfp_regs, sizeof(fpu_initialstate->vfp_regs));

	thread0.td_pcb->pcb_fpusaved->vfp_fpcr = VFPCR_INIT;
}

SYSINIT(vfp, SI_SUB_CPU, SI_ORDER_ANY, vfp_init, NULL);

static void
sve_thread_dtor(void *arg __unused, struct thread *td)
{
	sve_free(td->td_pcb->pcb_svesaved);
}

static void
sve_pcpu_read(void *arg)
{
	u_int *len;
	uint64_t vl;

	len = arg;

	/* Enable SVE to read zcr_el1 and VFP for rdvl */
	sve_enable();

	/* Set the longest vector length */
	WRITE_SPECIALREG(ZCR_EL1_REG, ZCR_LEN_MASK);
	isb();

	/* Read the real vector length */
	__asm __volatile(
	    ".arch_extension sve	\n"
	    "rdvl	%0, #1		\n"
	    ".arch_extension nosve	\n"
	    : "=&r"(vl));

	vfp_disable();

	len[PCPU_GET(cpuid)] = vl;
}

static void
sve_init(const void *dummy __unused)
{
	u_int *len_list;
	uint64_t reg;
	int i;

	if (!get_kernel_reg(ID_AA64PFR0_EL1, &reg))
		return;

	if (ID_AA64PFR0_SVE_VAL(reg) == ID_AA64PFR0_SVE_NONE)
		return;

	len_list = malloc(sizeof(*len_list) * (mp_maxid + 1), M_TEMP,
	    M_WAITOK | M_ZERO);
	smp_rendezvous(NULL, sve_pcpu_read, NULL, len_list);

	sve_max_vector_len = ZCR_LEN_BYTES(ZCR_LEN_MASK);
	CPU_FOREACH(i) {
		if (bootverbose)
			printf("CPU%d SVE vector length: %u\n", i, len_list[i]);
		sve_max_vector_len = MIN(sve_max_vector_len, len_list[i]);
	}
	free(len_list, M_TEMP);

	if (bootverbose)
		printf("SVE with %u byte vectors\n", sve_max_vector_len);

	if (sve_max_vector_len > 0) {
		EVENTHANDLER_REGISTER(thread_dtor, sve_thread_dtor, NULL,
		    EVENTHANDLER_PRI_ANY);
	}
}
SYSINIT(sve, SI_SUB_SMP, SI_ORDER_ANY, sve_init, NULL);

static bool
get_arm64_sve(struct regset *rs, struct thread *td, void *buf,
    size_t *sizep)
{
	struct svereg_header *header;
	struct pcb *pcb;
	size_t buf_size;
	uint16_t sve_flags;

	pcb = td->td_pcb;

	/* If there is no SVE support in HW then we don't support NT_ARM_SVE */
	if (pcb->pcb_sve_len == 0)
		return (false);

	sve_flags = 0;
	if ((pcb->pcb_fpflags & PCB_FP_SVEVALID) == 0) {
		/* If SVE hasn't been used yet provide the VFP registers */
		buf_size = sizeof(struct fpreg);
		sve_flags |= SVEREG_FLAG_FP;
	} else {
		/* We have SVE registers */
		buf_size = sve_buf_size(td);
		sve_flags |= SVEREG_FLAG_SVE;
		KASSERT(pcb->pcb_svesaved != NULL, ("%s: no saved sve",
		    __func__));
	}

	if (buf != NULL) {
		KASSERT(*sizep == sizeof(struct svereg_header) + buf_size,
		    ("%s: invalid size", __func__));

		if (td == curthread && (pcb->pcb_fpflags & PCB_FP_STARTED) != 0)
			vfp_save_state(td, pcb);

		header = buf;
		memset(header, 0, sizeof(*header));

		header->sve_size = sizeof(struct svereg_header) + buf_size;
		header->sve_maxsize = sizeof(struct svereg_header) +
		    sve_max_buf_size();
		header->sve_vec_len = pcb->pcb_sve_len;
		header->sve_max_vec_len = sve_max_vector_len;
		header->sve_flags = sve_flags;

		if ((sve_flags & SVEREG_FLAG_REGS_MASK) == SVEREG_FLAG_FP) {
			struct fpreg *fpregs;

			fpregs = (void *)(&header[1]);
			memcpy(fpregs->fp_q, pcb->pcb_fpustate.vfp_regs,
			    sizeof(fpregs->fp_q));
			fpregs->fp_cr = pcb->pcb_fpustate.vfp_fpcr;
			fpregs->fp_sr = pcb->pcb_fpustate.vfp_fpsr;
		} else {
			memcpy((void *)(&header[1]), pcb->pcb_svesaved,
			    buf_size);
		}
	}
	*sizep = sizeof(struct svereg_header) + buf_size;

	return (true);
}

static bool
set_arm64_sve(struct regset *rs, struct thread *td, void *buf, size_t size)
{
	struct svereg_header *header;
	struct pcb *pcb;
	size_t buf_size;
	uint16_t sve_flags;

	pcb = td->td_pcb;

	/* If there is no SVE support in HW then we don't support NT_ARM_SVE */
	if (pcb->pcb_sve_len == 0)
		return (false);

	sve_flags = 0;
	if ((pcb->pcb_fpflags & PCB_FP_SVEVALID) == 0) {
		/*
		 * If the SVE state is invalid it provide the FP registers.
		 * This may be beause it hasn't been used, or it has but
		 * was switched out in a system call.
		 */
		buf_size = sizeof(struct fpreg);
		sve_flags |= SVEREG_FLAG_FP;
	} else {
		/* We have SVE registers */
		MPASS(pcb->pcb_svesaved != NULL);
		buf_size = sve_buf_size(td);
		sve_flags |= SVEREG_FLAG_SVE;
		KASSERT(pcb->pcb_svesaved != NULL, ("%s: no saved sve",
		    __func__));
	}

	if (size != sizeof(struct svereg_header) + buf_size)
		return (false);

	header = buf;
	/* Sanity checks on the header */
	if (header->sve_size != sizeof(struct svereg_header) + buf_size)
		return (false);

	if (header->sve_maxsize != sizeof(struct svereg_header) +
	    sve_max_buf_size())
		return (false);

	if (header->sve_vec_len != pcb->pcb_sve_len)
		return (false);

	if (header->sve_max_vec_len != sve_max_vector_len)
		return (false);

	if (header->sve_flags != sve_flags)
		return (false);

	if ((sve_flags & SVEREG_FLAG_REGS_MASK) == SVEREG_FLAG_FP) {
		struct fpreg *fpregs;

		fpregs = (void *)(&header[1]);
		memcpy(pcb->pcb_fpustate.vfp_regs, fpregs->fp_q,
		    sizeof(fpregs->fp_q));
		pcb->pcb_fpustate.vfp_fpcr = fpregs->fp_cr;
		pcb->pcb_fpustate.vfp_fpsr = fpregs->fp_sr;
	} else {
		/* Restore the SVE registers */
		memcpy(pcb->pcb_svesaved, (void *)(&header[1]), buf_size);
	}

	return (true);
}

static struct regset regset_arm64_sve = {
	.note = NT_ARM_SVE,
	.get = get_arm64_sve,
	.set = set_arm64_sve,
};
ELF_REGSET(regset_arm64_sve);

struct fpu_kern_ctx *
fpu_kern_alloc_ctx(u_int flags)
{
	struct fpu_kern_ctx *res;
	size_t sz;

	sz = sizeof(struct fpu_kern_ctx);
	res = malloc(sz, M_FPUKERN_CTX, ((flags & FPU_KERN_NOWAIT) ?
	    M_NOWAIT : M_WAITOK) | M_ZERO);
	return (res);
}

void
fpu_kern_free_ctx(struct fpu_kern_ctx *ctx)
{

	KASSERT((ctx->flags & FPU_KERN_CTX_INUSE) == 0, ("free'ing inuse ctx"));
	/* XXXAndrew clear the memory ? */
	free(ctx, M_FPUKERN_CTX);
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
	KASSERT((pcb->pcb_fpflags & PCB_FP_NOSAVE) == 0,
	    ("recursive fpu_kern_enter while in PCB_FP_NOSAVE state"));

	if ((flags & FPU_KERN_NOCTX) != 0) {
		critical_enter();
		if (curthread == PCPU_GET(fpcurthread)) {
			vfp_save_state(curthread, pcb);
		}
		PCPU_SET(fpcurthread, NULL);

		vfp_enable();
		pcb->pcb_fpflags |= PCB_FP_KERN | PCB_FP_NOSAVE |
		    PCB_FP_STARTED;
		return;
	}

	if ((flags & FPU_KERN_KTHR) != 0 && is_fpu_kern_thread(0)) {
		ctx->flags = FPU_KERN_CTX_DUMMY | FPU_KERN_CTX_INUSE;
		return;
	}
	/*
	 * Check either we are already using the VFP in the kernel, or
	 * the saved state points to the default user space.
	 */
	KASSERT((pcb->pcb_fpflags & PCB_FP_KERN) != 0 ||
	    pcb->pcb_fpusaved == &pcb->pcb_fpustate,
	    ("Mangled pcb_fpusaved %x %p %p", pcb->pcb_fpflags, pcb->pcb_fpusaved, &pcb->pcb_fpustate));
	ctx->flags = FPU_KERN_CTX_INUSE;
	vfp_save_state(curthread, pcb);
	ctx->prev = pcb->pcb_fpusaved;
	pcb->pcb_fpusaved = &ctx->state;
	pcb->pcb_fpflags |= PCB_FP_KERN;
	pcb->pcb_fpflags &= ~PCB_FP_STARTED;

	return;
}

int
fpu_kern_leave(struct thread *td, struct fpu_kern_ctx *ctx)
{
	struct pcb *pcb;

	pcb = td->td_pcb;

	if ((pcb->pcb_fpflags & PCB_FP_NOSAVE) != 0) {
		KASSERT(ctx == NULL, ("non-null ctx after FPU_KERN_NOCTX"));
		KASSERT(PCPU_GET(fpcurthread) == NULL,
		    ("non-NULL fpcurthread for PCB_FP_NOSAVE"));
		CRITICAL_ASSERT(td);

		vfp_disable();
		pcb->pcb_fpflags &= ~(PCB_FP_NOSAVE | PCB_FP_STARTED);
		critical_exit();
	} else {
		KASSERT((ctx->flags & FPU_KERN_CTX_INUSE) != 0,
		    ("FPU context not inuse"));
		ctx->flags &= ~FPU_KERN_CTX_INUSE;

		if (is_fpu_kern_thread(0) &&
		    (ctx->flags & FPU_KERN_CTX_DUMMY) != 0)
			return (0);
		KASSERT((ctx->flags & FPU_KERN_CTX_DUMMY) == 0, ("dummy ctx"));
		critical_enter();
		vfp_discard(td);
		critical_exit();
		pcb->pcb_fpflags &= ~PCB_FP_STARTED;
		pcb->pcb_fpusaved = ctx->prev;
	}

	if (pcb->pcb_fpusaved == &pcb->pcb_fpustate) {
		pcb->pcb_fpflags &= ~PCB_FP_KERN;
	} else {
		KASSERT((pcb->pcb_fpflags & PCB_FP_KERN) != 0,
		    ("unpaired fpu_kern_leave"));
	}

	return (0);
}

int
fpu_kern_thread(u_int flags __unused)
{
	struct pcb *pcb = curthread->td_pcb;

	KASSERT((curthread->td_pflags & TDP_KTHREAD) != 0,
	    ("Only kthread may use fpu_kern_thread"));
	KASSERT(pcb->pcb_fpusaved == &pcb->pcb_fpustate,
	    ("Mangled pcb_fpusaved"));
	KASSERT((pcb->pcb_fpflags & PCB_FP_KERN) == 0,
	    ("Thread already setup for the VFP"));
	pcb->pcb_fpflags |= PCB_FP_KERN;
	return (0);
}

int
is_fpu_kern_thread(u_int flags __unused)
{
	struct pcb *curpcb;

	if ((curthread->td_pflags & TDP_KTHREAD) == 0)
		return (0);
	curpcb = curthread->td_pcb;
	return ((curpcb->pcb_fpflags & PCB_FP_KERN) != 0);
}

/*
 * FPU save area alloc/free/init utility routines
 */
struct vfpstate *
fpu_save_area_alloc(void)
{
	return (uma_zalloc(fpu_save_area_zone, M_WAITOK));
}

void
fpu_save_area_free(struct vfpstate *fsa)
{
	uma_zfree(fpu_save_area_zone, fsa);
}

void
fpu_save_area_reset(struct vfpstate *fsa)
{
	memcpy(fsa, fpu_initialstate, sizeof(*fsa));
}
#endif
