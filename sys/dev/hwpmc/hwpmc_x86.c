/*-
 * Copyright (c) 2005, Joseph Koshy
 * Copyright (c) 2007 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by A. Joseph Koshy under
 * sponsorship from the FreeBSD Foundation and Google, Inc.
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/pmc.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/apicreg.h>
#include <machine/pmc_mdep.h>
#include <machine/md_var.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

extern volatile lapic_t *lapic;

void
pmc_x86_lapic_enable_pmc_interrupt(void)
{
	uint32_t value;

	value =  lapic->lvt_pcint;
	value &= ~APIC_LVT_M;
	lapic->lvt_pcint = value;
}

/*
 * Attempt to walk a user call stack using a too-simple algorithm.
 * In the general case we need unwind information associated with
 * the executable to be able to walk the user stack.
 *
 * We are handed a trap frame laid down at the time the PMC interrupt
 * was taken.  If the application is using frame pointers, the saved
 * PC value could be:
 * a. at the beginning of a function before the stack frame is laid
 *    down,
 * b. just before a 'ret', after the stack frame has been taken off,
 * c. somewhere else in the function with a valid stack frame being
 *    present,
 *
 * If the application is not using frame pointers, this algorithm will
 * fail to yield an interesting call chain.
 *
 * TODO: figure out a way to use unwind information.
 */

int
pmc_save_user_callchain(uintptr_t *cc, int nframes, struct trapframe *tf)
{
	int n;
	uint32_t instr;
	uintptr_t fp, oldfp, pc, r, sp;

	KASSERT(TRAPF_USERMODE(tf), ("[x86,%d] Not a user trap frame tf=%p",
	    __LINE__, (void *) tf));

	pc = PMC_TRAPFRAME_TO_PC(tf);
	oldfp = fp = PMC_TRAPFRAME_TO_FP(tf);
	sp = PMC_TRAPFRAME_TO_SP(tf);

	*cc++ = pc; n = 1;

	r = fp + sizeof(uintptr_t); /* points to return address */

	if (!PMC_IN_USERSPACE(pc))
		return (n);

	if (copyin((void *) pc, &instr, sizeof(instr)) != 0)
		return (n);

	if (PMC_AT_FUNCTION_PROLOGUE_PUSH_BP(instr) ||
	    PMC_AT_FUNCTION_EPILOGUE_RET(instr)) { /* ret */
		if (copyin((void *) sp, &pc, sizeof(pc)) != 0)
			return (n);
	} else if (PMC_AT_FUNCTION_PROLOGUE_MOV_SP_BP(instr)) {
		sp += sizeof(uintptr_t);
		if (copyin((void *) sp, &pc, sizeof(pc)) != 0)
			return (n);
	} else if (copyin((void *) r, &pc, sizeof(pc)) != 0 ||
	    copyin((void *) fp, &fp, sizeof(fp) != 0))
		return (n);

	for (; n < nframes;) {
		if (pc == 0 || !PMC_IN_USERSPACE(pc))
			break;

		*cc++ = pc; n++;

		if (fp < oldfp)
			break;

		r = fp + sizeof(uintptr_t); /* address of return address */
		oldfp = fp;

		if (copyin((void *) r, &pc, sizeof(pc)) != 0 ||
		    copyin((void *) fp, &fp, sizeof(fp)) != 0)
			break;
	}

	return (n);
}

/*
 * Walking the kernel call stack.
 *
 * We are handed the trap frame laid down at the time the PMC
 * interrupt was taken.  The saved PC could be:
 * a. in the lowlevel trap handler, meaning that there isn't a C stack
 *    to traverse,
 * b. at the beginning of a function before the stack frame is laid
 *    down,
 * c. just before a 'ret', after the stack frame has been taken off,
 * d. somewhere else in a function with a valid stack frame being
 *    present.
 *
 * In case (d), the previous frame pointer is at [%ebp]/[%rbp] and
 * the return address is at [%ebp+4]/[%rbp+8].
 *
 * For cases (b) and (c), the return address is at [%esp]/[%rsp] and
 * the frame pointer doesn't need to be changed when going up one
 * level in the stack.
 *
 * For case (a), we check if the PC lies in low-level trap handling
 * code, and if so we terminate our trace.
 */

int
pmc_save_kernel_callchain(uintptr_t *cc, int nframes, struct trapframe *tf)
{
	int n;
	uint32_t instr;
	uintptr_t fp, pc, r, sp, stackstart, stackend;
	struct thread *td;

	KASSERT(TRAPF_USERMODE(tf) == 0,("[x86,%d] not a kernel backtrace",
	    __LINE__));

	pc = PMC_TRAPFRAME_TO_PC(tf);
	fp = PMC_TRAPFRAME_TO_FP(tf);
	sp = PMC_TRAPFRAME_TO_SP(tf);

	*cc++ = pc;
	r = fp + sizeof(uintptr_t); /* points to return address */

	if ((td = curthread) == NULL)
		return (1);

	if (nframes <= 1)
		return (1);

	stackstart = (uintptr_t) td->td_kstack;
	stackend = (uintptr_t) td->td_kstack + td->td_kstack_pages * PAGE_SIZE;

	if (PMC_IN_TRAP_HANDLER(pc) ||
	    !PMC_IN_KERNEL(pc) || !PMC_IN_KERNEL(r) ||
	    !PMC_IN_KERNEL_STACK(sp, stackstart, stackend) ||
	    !PMC_IN_KERNEL_STACK(fp, stackstart, stackend))
		return (1);

	instr = *(uint32_t *) pc;

	/*
	 * Determine whether the interrupted function was in the
	 * processing of either laying down its stack frame or taking
	 * it off.
	 *
	 * If we haven't started laying down a stack frame, or are
	 * just about to return, then our caller's address is at
	 * *sp, and we don't have a frame to unwind.
	 */
	if (PMC_AT_FUNCTION_PROLOGUE_PUSH_BP(instr) ||
	    PMC_AT_FUNCTION_EPILOGUE_RET(instr))
		pc = *(uintptr_t *) sp;
	else if (PMC_AT_FUNCTION_PROLOGUE_MOV_SP_BP(instr)) {
		/*
		 * The code was midway through laying down a frame.
		 * At this point sp[0] has a frame back pointer,
		 * and the caller's address is therefore at sp[1].
		 */
		sp += sizeof(uintptr_t);
		if (!PMC_IN_KERNEL_STACK(sp, stackstart, stackend))
			return (1);
		pc = *(uintptr_t *) sp;
	} else {
		/*
		 * Not in the function prologue or epilogue.
		 */
		pc = *(uintptr_t *) r;
		fp = *(uintptr_t *) fp;
	}

	for (n = 1; n < nframes; n++) {
		*cc++ = pc;

		if (PMC_IN_TRAP_HANDLER(pc))
			break;

		r = fp + sizeof(uintptr_t);
		if (!PMC_IN_KERNEL_STACK(fp, stackstart, stackend) ||
		    !PMC_IN_KERNEL(r))
			break;
		pc = *(uintptr_t *) r;
		fp = *(uintptr_t *) fp;
	}

	return (n);
}

static struct pmc_mdep *
pmc_intel_initialize(void)
{
	struct pmc_mdep *pmc_mdep;
	enum pmc_cputype cputype;
	int error, model;

	KASSERT(strcmp(cpu_vendor, "GenuineIntel") == 0,
	    ("[intel,%d] Initializing non-intel processor", __LINE__));

	PMCDBG(MDP,INI,0, "intel-initialize cpuid=0x%x", cpu_id);

	cputype = -1;

	switch (cpu_id & 0xF00) {
#if	defined(__i386__)
	case 0x500:		/* Pentium family processors */
		cputype = PMC_CPU_INTEL_P5;
		break;
	case 0x600:		/* Pentium Pro, Celeron, Pentium II & III */
		switch ((cpu_id & 0xF0) >> 4) { /* model number field */
		case 0x1:
			cputype = PMC_CPU_INTEL_P6;
			break;
		case 0x3: case 0x5:
			cputype = PMC_CPU_INTEL_PII;
			break;
		case 0x6:
			cputype = PMC_CPU_INTEL_CL;
			break;
		case 0x7: case 0x8: case 0xA: case 0xB:
			cputype = PMC_CPU_INTEL_PIII;
			break;
		case 0x9: case 0xD:
			cputype = PMC_CPU_INTEL_PM;
			break;
		}
		break;
#endif
#if	defined(__i386__) || defined(__amd64__)
	case 0xF00:		/* P4 */
		model = ((cpu_id & 0xF0000) >> 12) | ((cpu_id & 0xF0) >> 4);
		if (model >= 0 && model <= 6) /* known models */
			cputype = PMC_CPU_INTEL_PIV;
		break;
	}
#endif

	if ((int) cputype == -1) {
		printf("pmc: Unknown Intel CPU.\n");
		return NULL;
	}

	MALLOC(pmc_mdep, struct pmc_mdep *, sizeof(struct pmc_mdep),
	    M_PMC, M_WAITOK|M_ZERO);

	pmc_mdep->pmd_cputype 	    = cputype;
	pmc_mdep->pmd_nclass	    = 2;
	pmc_mdep->pmd_classes[0].pm_class    = PMC_CLASS_TSC;
	pmc_mdep->pmd_classes[0].pm_caps     = PMC_CAP_READ;
	pmc_mdep->pmd_classes[0].pm_width    = 64;
	pmc_mdep->pmd_nclasspmcs[0] = 1;

	error = 0;

	switch (cputype) {

#if	defined(__i386__) || defined(__amd64__)

		/*
		 * Intel Pentium 4 Processors, and P4/EMT64 processors.
		 */

	case PMC_CPU_INTEL_PIV:
		error = pmc_initialize_p4(pmc_mdep);
		break;
#endif

#if	defined(__i386__)
		/*
		 * P6 Family Processors
		 */

	case PMC_CPU_INTEL_P6:
	case PMC_CPU_INTEL_CL:
	case PMC_CPU_INTEL_PII:
	case PMC_CPU_INTEL_PIII:
	case PMC_CPU_INTEL_PM:

		error = pmc_initialize_p6(pmc_mdep);
		break;

		/*
		 * Intel Pentium PMCs.
		 */

	case PMC_CPU_INTEL_P5:
		error = pmc_initialize_p5(pmc_mdep);
		break;
#endif

	default:
		KASSERT(0,("[intel,%d] Unknown CPU type", __LINE__));
	}

	if (error) {
		FREE(pmc_mdep, M_PMC);
		pmc_mdep = NULL;
	}

	return pmc_mdep;
}


/*
 * Machine dependent initialization for x86 class platforms.
 */

struct pmc_mdep *
pmc_md_initialize()
{
	int i;
	struct pmc_mdep *md;

	/* determine the CPU kind */
	md = NULL;
	if (strcmp(cpu_vendor, "AuthenticAMD") == 0)
		md = pmc_amd_initialize();
	else if (strcmp(cpu_vendor, "GenuineIntel") == 0)
		md = pmc_intel_initialize();

	/* disallow sampling if we do not have an LAPIC */
	if (md != NULL && lapic == NULL)
		for (i = 1; i < md->pmd_nclass; i++)
			md->pmd_classes[i].pm_caps &= ~PMC_CAP_INTERRUPT;

	return md;
}
