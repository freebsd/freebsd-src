/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
#include <sys/limits.h>
#include <sys/proc.h>
#include <sys/imgact_elf.h>
#include <sys/kernel.h>

#include <machine/armreg.h>
#include <machine/elf.h>
#include <machine/frame.h>
#include <machine/md_var.h>
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

struct fpu_kern_ctx {
	struct vfp_state	*prev;
#define	FPU_KERN_CTX_DUMMY	0x01	/* avoided save for the kern thread */
#define	FPU_KERN_CTX_INUSE	0x02
	uint32_t	 flags;
	struct vfp_state	 state;
};

/*
 * About .fpu directives in this file...
 *
 * We should need simply .fpu vfpv3, but clang 3.5 has a quirk where setting
 * vfpv3 doesn't imply that vfp2 features are also available -- both have to be
 * explicitly set to get all the features of both.  This is probably a bug in
 * clang, so it may get fixed and require changes here some day.  Other changes
 * are probably coming in clang too, because there is email and open PRs
 * indicating they want to completely disable the ability to use .fpu and
 * similar directives in inline asm.  That would be catastrophic for us,
 * hopefully they come to their senses.  There was also some discusion of a new
 * syntax such as .push fpu=vfpv3; ...; .pop fpu; and that would be ideal for
 * us, better than what we have now really.
 *
 * For gcc, each .fpu directive completely overrides the prior directive, unlike
 * with clang, but luckily on gcc saying v3 implies all the v2 features as well.
 */

#define fmxr(reg, val) \
    __asm __volatile("	.fpu vfpv2\n .fpu vfpv3\n"			\
		     "	vmsr	" __STRING(reg) ", %0"   :: "r"(val));

#define fmrx(reg) \
({ u_int val = 0;\
    __asm __volatile(" .fpu vfpv2\n .fpu vfpv3\n"			\
		     "	vmrs	%0, " __STRING(reg) : "=r"(val));	\
    val; \
})

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

static void
vfp_enable(void)
{
	uint32_t fpexc;

	fpexc = fmrx(fpexc);
	fmxr(fpexc, fpexc | VFPEXC_EN);
	isb();
}

static void
vfp_disable(void)
{
	uint32_t fpexc;

	fpexc = fmrx(fpexc);
	fmxr(fpexc, fpexc & ~VFPEXC_EN);
	isb();
}

	/* called for each cpu */
void
vfp_init(void)
{
	u_int fpsid, tmp;
	u_int coproc, vfp_arch;

	coproc = get_coprocessorACR();
	coproc |= COPROC10 | COPROC11;
	set_coprocessorACR(coproc);

	fpsid = fmrx(fpsid);		/* read the vfp system id */

	if (!(fpsid & VFPSID_HARDSOFT_IMP)) {
		vfp_exists = 1;
		is_d32 = 0;
		PCPU_SET(vfpsid, fpsid);	/* save the fpsid */
		elf_hwcap |= HWCAP_VFP;

		vfp_arch =
		    (fpsid & VFPSID_SUBVERSION2_MASK) >> VFPSID_SUBVERSION_OFF;

		if (vfp_arch >= VFP_ARCH3) {
			tmp = fmrx(mvfr0);
			PCPU_SET(vfpmvfr0, tmp);
			elf_hwcap |= HWCAP_VFPv3;

			if ((tmp & VMVFR0_RB_MASK) == 2) {
				elf_hwcap |= HWCAP_VFPD32;
				is_d32 = 1;
			} else
				elf_hwcap |= HWCAP_VFPv3D16;

			tmp = fmrx(mvfr1);
			PCPU_SET(vfpmvfr1, tmp);

			if (PCPU_GET(cpuid) == 0) {
				if ((tmp & VMVFR1_FZ_MASK) == 0x1) {
					/* Denormals arithmetic support */
					initial_fpscr &= ~VFPSCR_FZ;
					thread0.td_pcb->pcb_vfpstate.fpscr =
					    initial_fpscr;
				}
			}

			if ((tmp & VMVFR1_LS_MASK) >> VMVFR1_LS_OFF == 1 &&
			    (tmp & VMVFR1_I_MASK) >> VMVFR1_I_OFF == 1 &&
			    (tmp & VMVFR1_SP_MASK) >> VMVFR1_SP_OFF == 1)
				elf_hwcap |= HWCAP_NEON;
			if ((tmp & VMVFR1_FMAC_MASK) >>  VMVFR1_FMAC_OFF == 1)
				elf_hwcap |= HWCAP_VFPv4;
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
	ksiginfo_t ksi;

	if ((code & FAULT_USER) == 0)
		panic("undefined floating point instruction in supervisor mode");

	critical_enter();

	/*
	 * If the VFP is already on and we got an undefined instruction, then
	 * something tried to executate a truly invalid instruction that maps to
	 * the VFP.
	 */
	fpexc = fmrx(fpexc);
	if (fpexc & VFPEXC_EN) {
		/* Clear any exceptions */
		fmxr(fpexc, fpexc & ~(VFPEXC_EX | VFPEXC_FP2V));

		/* kill the process - we do not handle emulation */
		critical_exit();

		if (fpexc & VFPEXC_EX) {
			/* We have an exception, signal a SIGFPE */
			ksiginfo_init_trap(&ksi);
			ksi.ksi_signo = SIGFPE;
			if (fpexc & VFPEXC_UFC)
				ksi.ksi_code = FPE_FLTUND;
			else if (fpexc & VFPEXC_OFC)
				ksi.ksi_code = FPE_FLTOVF;
			else if (fpexc & VFPEXC_IOC)
				ksi.ksi_code = FPE_FLTINV;
			ksi.ksi_addr = (void *)addr;
			trapsignal(curthread, &ksi);
			return 0;
		}

		return 1;
	}

	/*
	 * If the last time this thread used the VFP it was on this core, and
	 * the last thread to use the VFP on this core was this thread, then the
	 * VFP state is valid, otherwise restore this thread's state to the VFP.
	 */
	fmxr(fpexc, fpexc | VFPEXC_EN);
	curpcb = curthread->td_pcb;
	cpu = PCPU_GET(cpuid);
	if (curpcb->pcb_vfpcpu != cpu || curthread != PCPU_GET(fpcurthread)) {
		vfp_restore(curpcb->pcb_vfpsaved);
		curpcb->pcb_vfpcpu = cpu;
		PCPU_SET(fpcurthread, curthread);
	}

	critical_exit();

	KASSERT(curpcb->pcb_vfpsaved == &curpcb->pcb_vfpstate,
	    ("Kernel VFP state in use when entering userspace"));

	return (0);
}

/*
 * Update the VFP state for a forked process or new thread. The PCB will
 * have been copied from the old thread.
 * The code is heavily based on arm64 logic.
 */
void
vfp_new_thread(struct thread *newtd, struct thread *oldtd, bool fork)
{
	struct pcb *newpcb;

	newpcb = newtd->td_pcb;

	/* Kernel threads start with clean VFP */
	if ((oldtd->td_pflags & TDP_KTHREAD) != 0) {
		newpcb->pcb_fpflags &=
		    ~(PCB_FP_STARTED | PCB_FP_KERN | PCB_FP_NOSAVE);
	} else {
		MPASS((newpcb->pcb_fpflags & (PCB_FP_KERN|PCB_FP_NOSAVE)) == 0);
		if (!fork) {
			newpcb->pcb_fpflags &= ~PCB_FP_STARTED;
		}
	}

	newpcb->pcb_vfpsaved = &newpcb->pcb_vfpstate;
	newpcb->pcb_vfpcpu = UINT_MAX;
}
/*
 * Restore the given state to the VFP hardware.
 */
static void
vfp_restore(struct vfp_state *vfpsave)
{
	uint32_t fpexc;

	/* On vfpv3 we may need to restore FPINST and FPINST2 */
	fpexc = vfpsave->fpexec;
	if (fpexc & VFPEXC_EX) {
		fmxr(fpinst, vfpsave->fpinst);
		if (fpexc & VFPEXC_FP2V)
			fmxr(fpinst2, vfpsave->fpinst2);
	}
	fmxr(fpscr, vfpsave->fpscr);

	__asm __volatile(
	    " .fpu	vfpv2\n"
	    " .fpu	vfpv3\n"
	    " vldmia	%0!, {d0-d15}\n"	/* d0-d15 */
	    " cmp	%1, #0\n"		/* -D16 or -D32? */
	    " vldmiane	%0!, {d16-d31}\n"	/* d16-d31 */
	    " addeq	%0, %0, #128\n"		/* skip missing regs */
	    : "+&r" (vfpsave) : "r" (is_d32) : "cc"
	    );

	fmxr(fpexc, fpexc);
}

/*
 * If the VFP is on, save its current state and turn it off if requested to do
 * so.  If the VFP is not on, does not change the values at *vfpsave.  Caller is
 * responsible for preventing a context switch while this is running.
 */
void
vfp_store(struct vfp_state *vfpsave, boolean_t disable_vfp)
{
	uint32_t fpexc;

	fpexc = fmrx(fpexc);		/* Is the vfp enabled? */
	if (fpexc & VFPEXC_EN) {
		vfpsave->fpexec = fpexc;
		vfpsave->fpscr = fmrx(fpscr);

		/* On vfpv3 we may need to save FPINST and FPINST2 */
		if (fpexc & VFPEXC_EX) {
			vfpsave->fpinst = fmrx(fpinst);
			if (fpexc & VFPEXC_FP2V)
				vfpsave->fpinst2 = fmrx(fpinst2);
			fpexc &= ~VFPEXC_EX;
		}

		__asm __volatile(
		    " .fpu	vfpv2\n"
		    " .fpu	vfpv3\n"
		    " vstmia	%0!, {d0-d15}\n"	/* d0-d15 */
		    " cmp	%1, #0\n"		/* -D16 or -D32? */
		    " vstmiane	%0!, {d16-d31}\n"	/* d16-d31 */
		    " addeq	%0, %0, #128\n"		/* skip missing regs */
		    : "+&r" (vfpsave) : "r" (is_d32) : "cc"
		    );

		if (disable_vfp)
			fmxr(fpexc , fpexc & ~VFPEXC_EN);
	}
}

/*
 * The current thread is dying.  If the state currently in the hardware belongs
 * to the current thread, set fpcurthread to NULL to indicate that the VFP
 * hardware state does not belong to any thread.  If the VFP is on, turn it off.
 */
void
vfp_discard(struct thread *td)
{
	u_int tmp;

	if (PCPU_GET(fpcurthread) == td)
		PCPU_SET(fpcurthread, NULL);

	tmp = fmrx(fpexc);
	if (tmp & VFPEXC_EN)
		fmxr(fpexc, tmp & ~VFPEXC_EN);
}

void
vfp_save_state(struct thread *td, struct pcb *pcb)
{
	int32_t fpexc;

	KASSERT(pcb != NULL, ("NULL vfp pcb"));
	KASSERT(td == NULL || td->td_pcb == pcb, ("Invalid vfp pcb"));

	/*
	 * savectx() will be called on panic with dumppcb as an argument,
	 * dumppcb doesn't have pcb_vfpsaved set, so set it to save
	 * the VFP registers.
	 */
	if (pcb->pcb_vfpsaved == NULL)
		pcb->pcb_vfpsaved = &pcb->pcb_vfpstate;

	if (td == NULL)
		td = curthread;

	critical_enter();
	/*
	 * Only store the registers if the VFP is enabled,
	 * i.e. return if we are trapping on FP access.
	 */
	fpexc = fmrx(fpexc);
	if (fpexc & VFPEXC_EN) {
		KASSERT(PCPU_GET(fpcurthread) == td,
		    ("Storing an invalid VFP state"));

		vfp_store(pcb->pcb_vfpsaved, true);
	}
	critical_exit();
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
	 * the the saved state points to the default user space.
	 */
	KASSERT((pcb->pcb_fpflags & PCB_FP_KERN) != 0 ||
	    pcb->pcb_vfpsaved == &pcb->pcb_vfpstate,
	    ("Mangled pcb_vfpsaved %x %p %p", pcb->pcb_fpflags, pcb->pcb_vfpsaved,
	     &pcb->pcb_vfpstate));
	ctx->flags = FPU_KERN_CTX_INUSE;
	vfp_save_state(curthread, pcb);
	ctx->prev = pcb->pcb_vfpsaved;
	pcb->pcb_vfpsaved = &ctx->state;
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
		pcb->pcb_vfpsaved = ctx->prev;
	}

	if (pcb->pcb_vfpsaved == &pcb->pcb_vfpstate) {
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
	KASSERT(pcb->pcb_vfpsaved == &pcb->pcb_vfpstate,
	    ("Mangled pcb_vfpsaved"));
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

#endif
