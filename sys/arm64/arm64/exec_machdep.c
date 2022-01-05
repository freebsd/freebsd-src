/*-
 * Copyright (c) 2014 Andrew Turner
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/rwlock.h>
#include <sys/signalvar.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/ucontext.h>

#include <vm/vm.h>
#include <vm/vm_param.h>

#include <machine/armreg.h>
#include <machine/kdb.h>
#include <machine/md_var.h>
#include <machine/pcb.h>

#ifdef VFP
#include <machine/vfp.h>
#endif

static void get_fpcontext(struct thread *td, mcontext_t *mcp);
static void set_fpcontext(struct thread *td, mcontext_t *mcp);

int
fill_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *frame;

	frame = td->td_frame;
	regs->sp = frame->tf_sp;
	regs->lr = frame->tf_lr;
	regs->elr = frame->tf_elr;
	regs->spsr = frame->tf_spsr;

	memcpy(regs->x, frame->tf_x, sizeof(regs->x));

#ifdef COMPAT_FREEBSD32
	/*
	 * We may be called here for a 32bits process, if we're using a
	 * 64bits debugger. If so, put PC and SPSR where it expects it.
	 */
	if (SV_PROC_FLAG(td->td_proc, SV_ILP32)) {
		regs->x[15] = frame->tf_elr;
		regs->x[16] = frame->tf_spsr;
	}
#endif
	return (0);
}

int
set_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *frame;

	frame = td->td_frame;
	frame->tf_sp = regs->sp;
	frame->tf_lr = regs->lr;
	frame->tf_spsr &= ~PSR_FLAGS;

	memcpy(frame->tf_x, regs->x, sizeof(frame->tf_x));

#ifdef COMPAT_FREEBSD32
	if (SV_PROC_FLAG(td->td_proc, SV_ILP32)) {
		/*
		 * We may be called for a 32bits process if we're using
		 * a 64bits debugger. If so, get PC and SPSR from where
		 * it put it.
		 */
		frame->tf_elr = regs->x[15];
		frame->tf_spsr |= regs->x[16] & PSR_FLAGS;
	} else
#endif
	{
		frame->tf_elr = regs->elr;
		frame->tf_spsr |= regs->spsr & PSR_FLAGS;
	}
	return (0);
}

int
fill_fpregs(struct thread *td, struct fpreg *regs)
{
#ifdef VFP
	struct pcb *pcb;

	pcb = td->td_pcb;
	if ((pcb->pcb_fpflags & PCB_FP_STARTED) != 0) {
		/*
		 * If we have just been running VFP instructions we will
		 * need to save the state to memcpy it below.
		 */
		if (td == curthread)
			vfp_save_state(td, pcb);

		KASSERT(pcb->pcb_fpusaved == &pcb->pcb_fpustate,
		    ("Called fill_fpregs while the kernel is using the VFP"));
		memcpy(regs->fp_q, pcb->pcb_fpustate.vfp_regs,
		    sizeof(regs->fp_q));
		regs->fp_cr = pcb->pcb_fpustate.vfp_fpcr;
		regs->fp_sr = pcb->pcb_fpustate.vfp_fpsr;
	} else
#endif
		memset(regs, 0, sizeof(*regs));
	return (0);
}

int
set_fpregs(struct thread *td, struct fpreg *regs)
{
#ifdef VFP
	struct pcb *pcb;

	pcb = td->td_pcb;
	KASSERT(pcb->pcb_fpusaved == &pcb->pcb_fpustate,
	    ("Called set_fpregs while the kernel is using the VFP"));
	memcpy(pcb->pcb_fpustate.vfp_regs, regs->fp_q, sizeof(regs->fp_q));
	pcb->pcb_fpustate.vfp_fpcr = regs->fp_cr;
	pcb->pcb_fpustate.vfp_fpsr = regs->fp_sr;
#endif
	return (0);
}

int
fill_dbregs(struct thread *td, struct dbreg *regs)
{
	struct debug_monitor_state *monitor;
	int i;
	uint8_t debug_ver, nbkpts, nwtpts;

	memset(regs, 0, sizeof(*regs));

	extract_user_id_field(ID_AA64DFR0_EL1, ID_AA64DFR0_DebugVer_SHIFT,
	    &debug_ver);
	extract_user_id_field(ID_AA64DFR0_EL1, ID_AA64DFR0_BRPs_SHIFT,
	    &nbkpts);
	extract_user_id_field(ID_AA64DFR0_EL1, ID_AA64DFR0_WRPs_SHIFT,
	    &nwtpts);

	/*
	 * The BRPs field contains the number of breakpoints - 1. Armv8-A
	 * allows the hardware to provide 2-16 breakpoints so this won't
	 * overflow an 8 bit value. The same applies to the WRPs field.
	 */
	nbkpts++;
	nwtpts++;

	regs->db_debug_ver = debug_ver;
	regs->db_nbkpts = nbkpts;
	regs->db_nwtpts = nwtpts;

	monitor = &td->td_pcb->pcb_dbg_regs;
	if ((monitor->dbg_flags & DBGMON_ENABLED) != 0) {
		for (i = 0; i < nbkpts; i++) {
			regs->db_breakregs[i].dbr_addr = monitor->dbg_bvr[i];
			regs->db_breakregs[i].dbr_ctrl = monitor->dbg_bcr[i];
		}
		for (i = 0; i < nwtpts; i++) {
			regs->db_watchregs[i].dbw_addr = monitor->dbg_wvr[i];
			regs->db_watchregs[i].dbw_ctrl = monitor->dbg_wcr[i];
		}
	}

	return (0);
}

int
set_dbregs(struct thread *td, struct dbreg *regs)
{
	struct debug_monitor_state *monitor;
	uint64_t addr;
	uint32_t ctrl;
	int i;

	monitor = &td->td_pcb->pcb_dbg_regs;
	monitor->dbg_enable_count = 0;

	for (i = 0; i < DBG_BRP_MAX; i++) {
		addr = regs->db_breakregs[i].dbr_addr;
		ctrl = regs->db_breakregs[i].dbr_ctrl;

		/*
		 * Don't let the user set a breakpoint on a kernel or
		 * non-canonical user address.
		 */
		if (addr >= VM_MAXUSER_ADDRESS)
			return (EINVAL);

		/*
		 * The lowest 2 bits are ignored, so record the effective
		 * address.
		 */
		addr = rounddown2(addr, 4);

		/*
		 * Some control fields are ignored, and other bits reserved.
		 * Only unlinked, address-matching breakpoints are supported.
		 *
		 * XXX: fields that appear unvalidated, such as BAS, have
		 * constrained undefined behaviour. If the user mis-programs
		 * these, there is no risk to the system.
		 */
		ctrl &= DBG_BCR_EN | DBG_BCR_PMC | DBG_BCR_BAS;
		if ((ctrl & DBG_BCR_EN) != 0) {
			/* Only target EL0. */
			if ((ctrl & DBG_BCR_PMC) != DBG_BCR_PMC_EL0)
				return (EINVAL);

			monitor->dbg_enable_count++;
		}

		monitor->dbg_bvr[i] = addr;
		monitor->dbg_bcr[i] = ctrl;
	}

	for (i = 0; i < DBG_WRP_MAX; i++) {
		addr = regs->db_watchregs[i].dbw_addr;
		ctrl = regs->db_watchregs[i].dbw_ctrl;

		/*
		 * Don't let the user set a watchpoint on a kernel or
		 * non-canonical user address.
		 */
		if (addr >= VM_MAXUSER_ADDRESS)
			return (EINVAL);

		/*
		 * Some control fields are ignored, and other bits reserved.
		 * Only unlinked watchpoints are supported.
		 */
		ctrl &= DBG_WCR_EN | DBG_WCR_PAC | DBG_WCR_LSC | DBG_WCR_BAS |
		    DBG_WCR_MASK;

		if ((ctrl & DBG_WCR_EN) != 0) {
			/* Only target EL0. */
			if ((ctrl & DBG_WCR_PAC) != DBG_WCR_PAC_EL0)
				return (EINVAL);

			/* Must set at least one of the load/store bits. */
			if ((ctrl & DBG_WCR_LSC) == 0)
				return (EINVAL);

			/*
			 * When specifying the address range with BAS, the MASK
			 * field must be zero.
			 */
			if ((ctrl & DBG_WCR_BAS) != DBG_WCR_BAS_MASK &&
			    (ctrl & DBG_WCR_MASK) != 0)
				return (EINVAL);

			monitor->dbg_enable_count++;
		}
		monitor->dbg_wvr[i] = addr;
		monitor->dbg_wcr[i] = ctrl;
	}

	if (monitor->dbg_enable_count > 0)
		monitor->dbg_flags |= DBGMON_ENABLED;

	return (0);
}

#ifdef COMPAT_FREEBSD32
int
fill_regs32(struct thread *td, struct reg32 *regs)
{
	int i;
	struct trapframe *tf;

	tf = td->td_frame;
	for (i = 0; i < 13; i++)
		regs->r[i] = tf->tf_x[i];
	/* For arm32, SP is r13 and LR is r14 */
	regs->r_sp = tf->tf_x[13];
	regs->r_lr = tf->tf_x[14];
	regs->r_pc = tf->tf_elr;
	regs->r_cpsr = tf->tf_spsr;

	return (0);
}

int
set_regs32(struct thread *td, struct reg32 *regs)
{
	int i;
	struct trapframe *tf;

	tf = td->td_frame;
	for (i = 0; i < 13; i++)
		tf->tf_x[i] = regs->r[i];
	/* For arm 32, SP is r13 an LR is r14 */
	tf->tf_x[13] = regs->r_sp;
	tf->tf_x[14] = regs->r_lr;
	tf->tf_elr = regs->r_pc;
	tf->tf_spsr &= ~PSR_FLAGS;
	tf->tf_spsr |= regs->r_cpsr & PSR_FLAGS;

	return (0);
}

/* XXX fill/set dbregs/fpregs are stubbed on 32-bit arm. */
int
fill_fpregs32(struct thread *td, struct fpreg32 *regs)
{

	memset(regs, 0, sizeof(*regs));
	return (0);
}

int
set_fpregs32(struct thread *td, struct fpreg32 *regs)
{

	return (0);
}

int
fill_dbregs32(struct thread *td, struct dbreg32 *regs)
{

	memset(regs, 0, sizeof(*regs));
	return (0);
}

int
set_dbregs32(struct thread *td, struct dbreg32 *regs)
{

	return (0);
}
#endif

void
exec_setregs(struct thread *td, struct image_params *imgp, uintptr_t stack)
{
	struct trapframe *tf = td->td_frame;
	struct pcb *pcb = td->td_pcb;

	memset(tf, 0, sizeof(struct trapframe));

	tf->tf_x[0] = stack;
	tf->tf_sp = STACKALIGN(stack);
	tf->tf_lr = imgp->entry_addr;
	tf->tf_elr = imgp->entry_addr;

	td->td_pcb->pcb_tpidr_el0 = 0;
	td->td_pcb->pcb_tpidrro_el0 = 0;
	WRITE_SPECIALREG(tpidrro_el0, 0);
	WRITE_SPECIALREG(tpidr_el0, 0);

#ifdef VFP
	vfp_reset_state(td, pcb);
#endif

	/*
	 * Clear debug register state. It is not applicable to the new process.
	 */
	bzero(&pcb->pcb_dbg_regs, sizeof(pcb->pcb_dbg_regs));
}

/* Sanity check these are the same size, they will be memcpy'd to and from */
CTASSERT(sizeof(((struct trapframe *)0)->tf_x) ==
    sizeof((struct gpregs *)0)->gp_x);
CTASSERT(sizeof(((struct trapframe *)0)->tf_x) ==
    sizeof((struct reg *)0)->x);

int
get_mcontext(struct thread *td, mcontext_t *mcp, int clear_ret)
{
	struct trapframe *tf = td->td_frame;

	if (clear_ret & GET_MC_CLEAR_RET) {
		mcp->mc_gpregs.gp_x[0] = 0;
		mcp->mc_gpregs.gp_spsr = tf->tf_spsr & ~PSR_C;
	} else {
		mcp->mc_gpregs.gp_x[0] = tf->tf_x[0];
		mcp->mc_gpregs.gp_spsr = tf->tf_spsr;
	}

	memcpy(&mcp->mc_gpregs.gp_x[1], &tf->tf_x[1],
	    sizeof(mcp->mc_gpregs.gp_x[1]) * (nitems(mcp->mc_gpregs.gp_x) - 1));

	mcp->mc_gpregs.gp_sp = tf->tf_sp;
	mcp->mc_gpregs.gp_lr = tf->tf_lr;
	mcp->mc_gpregs.gp_elr = tf->tf_elr;
	get_fpcontext(td, mcp);

	return (0);
}

int
set_mcontext(struct thread *td, mcontext_t *mcp)
{
	struct trapframe *tf = td->td_frame;
	uint32_t spsr;

	spsr = mcp->mc_gpregs.gp_spsr;
	if ((spsr & PSR_M_MASK) != PSR_M_EL0t ||
	    (spsr & PSR_AARCH32) != 0 ||
	    (spsr & PSR_DAIF) != (td->td_frame->tf_spsr & PSR_DAIF))
		return (EINVAL); 

	memcpy(tf->tf_x, mcp->mc_gpregs.gp_x, sizeof(tf->tf_x));

	tf->tf_sp = mcp->mc_gpregs.gp_sp;
	tf->tf_lr = mcp->mc_gpregs.gp_lr;
	tf->tf_elr = mcp->mc_gpregs.gp_elr;
	tf->tf_spsr = mcp->mc_gpregs.gp_spsr;
	set_fpcontext(td, mcp);

	return (0);
}

static void
get_fpcontext(struct thread *td, mcontext_t *mcp)
{
#ifdef VFP
	struct pcb *curpcb;

	critical_enter();

	curpcb = curthread->td_pcb;

	if ((curpcb->pcb_fpflags & PCB_FP_STARTED) != 0) {
		/*
		 * If we have just been running VFP instructions we will
		 * need to save the state to memcpy it below.
		 */
		vfp_save_state(td, curpcb);

		KASSERT(curpcb->pcb_fpusaved == &curpcb->pcb_fpustate,
		    ("Called get_fpcontext while the kernel is using the VFP"));
		KASSERT((curpcb->pcb_fpflags & ~PCB_FP_USERMASK) == 0,
		    ("Non-userspace FPU flags set in get_fpcontext"));
		memcpy(mcp->mc_fpregs.fp_q, curpcb->pcb_fpustate.vfp_regs,
		    sizeof(mcp->mc_fpregs.fp_q));
		mcp->mc_fpregs.fp_cr = curpcb->pcb_fpustate.vfp_fpcr;
		mcp->mc_fpregs.fp_sr = curpcb->pcb_fpustate.vfp_fpsr;
		mcp->mc_fpregs.fp_flags = curpcb->pcb_fpflags;
		mcp->mc_flags |= _MC_FP_VALID;
	}

	critical_exit();
#endif
}

static void
set_fpcontext(struct thread *td, mcontext_t *mcp)
{
#ifdef VFP
	struct pcb *curpcb;

	critical_enter();

	if ((mcp->mc_flags & _MC_FP_VALID) != 0) {
		curpcb = curthread->td_pcb;

		/*
		 * Discard any vfp state for the current thread, we
		 * are about to override it.
		 */
		vfp_discard(td);

		KASSERT(curpcb->pcb_fpusaved == &curpcb->pcb_fpustate,
		    ("Called set_fpcontext while the kernel is using the VFP"));
		memcpy(curpcb->pcb_fpustate.vfp_regs, mcp->mc_fpregs.fp_q,
		    sizeof(mcp->mc_fpregs.fp_q));
		curpcb->pcb_fpustate.vfp_fpcr = mcp->mc_fpregs.fp_cr;
		curpcb->pcb_fpustate.vfp_fpsr = mcp->mc_fpregs.fp_sr;
		curpcb->pcb_fpflags = mcp->mc_fpregs.fp_flags & PCB_FP_USERMASK;
	}

	critical_exit();
#endif
}

int
sys_sigreturn(struct thread *td, struct sigreturn_args *uap)
{
	ucontext_t uc;
	int error;

	if (copyin(uap->sigcntxp, &uc, sizeof(uc)))
		return (EFAULT);

	error = set_mcontext(td, &uc.uc_mcontext);
	if (error != 0)
		return (error);

	/* Restore signal mask. */
	kern_sigprocmask(td, SIG_SETMASK, &uc.uc_sigmask, NULL, 0);

	return (EJUSTRETURN);
}

void
sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct thread *td;
	struct proc *p;
	struct trapframe *tf;
	struct sigframe *fp, frame;
	struct sigacts *psp;
	int onstack, sig;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);

	sig = ksi->ksi_signo;
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);

	tf = td->td_frame;
	onstack = sigonstack(tf->tf_sp);

	CTR4(KTR_SIG, "sendsig: td=%p (%s) catcher=%p sig=%d", td, p->p_comm,
	    catcher, sig);

	/* Allocate and validate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) != 0 && !onstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		fp = (struct sigframe *)((uintptr_t)td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size);
#if defined(COMPAT_43)
		td->td_sigstk.ss_flags |= SS_ONSTACK;
#endif
	} else {
		fp = (struct sigframe *)td->td_frame->tf_sp;
	}

	/* Make room, keeping the stack aligned */
	fp--;
	fp = (struct sigframe *)STACKALIGN(fp);

	/* Fill in the frame to copy out */
	bzero(&frame, sizeof(frame));
	get_mcontext(td, &frame.sf_uc.uc_mcontext, 0);
	frame.sf_si = ksi->ksi_info;
	frame.sf_uc.uc_sigmask = *mask;
	frame.sf_uc.uc_stack = td->td_sigstk;
	frame.sf_uc.uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK) != 0 ?
	    (onstack ? SS_ONSTACK : 0) : SS_DISABLE;
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(td->td_proc);

	/* Copy the sigframe out to the user's stack. */
	if (copyout(&frame, fp, sizeof(*fp)) != 0) {
		/* Process has trashed its stack. Kill it. */
		CTR2(KTR_SIG, "sendsig: sigexit td=%p fp=%p", td, fp);
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	tf->tf_x[0]= sig;
	tf->tf_x[1] = (register_t)&fp->sf_si;
	tf->tf_x[2] = (register_t)&fp->sf_uc;

	tf->tf_elr = (register_t)catcher;
	tf->tf_sp = (register_t)fp;
	tf->tf_lr = (register_t)p->p_sysent->sv_sigcode_base;

	CTR3(KTR_SIG, "sendsig: return td=%p pc=%#x sp=%#x", td, tf->tf_elr,
	    tf->tf_sp);

	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}
