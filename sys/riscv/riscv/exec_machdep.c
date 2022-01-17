/*-
 * Copyright (c) 2014 Andrew Turner
 * Copyright (c) 2015-2017 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/ucontext.h>

#include <machine/cpu.h>
#include <machine/kdb.h>
#include <machine/pcb.h>
#include <machine/pte.h>
#include <machine/reg.h>
#include <machine/riscvreg.h>
#include <machine/sbi.h>
#include <machine/trap.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#ifdef FPE
#include <machine/fpe.h>
#endif

static void get_fpcontext(struct thread *td, mcontext_t *mcp);
static void set_fpcontext(struct thread *td, mcontext_t *mcp);

int
fill_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *frame;

	frame = td->td_frame;
	regs->sepc = frame->tf_sepc;
	regs->sstatus = frame->tf_sstatus;
	regs->ra = frame->tf_ra;
	regs->sp = frame->tf_sp;
	regs->gp = frame->tf_gp;
	regs->tp = frame->tf_tp;

	memcpy(regs->t, frame->tf_t, sizeof(regs->t));
	memcpy(regs->s, frame->tf_s, sizeof(regs->s));
	memcpy(regs->a, frame->tf_a, sizeof(regs->a));

	return (0);
}

int
set_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *frame;

	frame = td->td_frame;
	frame->tf_sepc = regs->sepc;
	frame->tf_ra = regs->ra;
	frame->tf_sp = regs->sp;
	frame->tf_gp = regs->gp;
	frame->tf_tp = regs->tp;

	memcpy(frame->tf_t, regs->t, sizeof(frame->tf_t));
	memcpy(frame->tf_s, regs->s, sizeof(frame->tf_s));
	memcpy(frame->tf_a, regs->a, sizeof(frame->tf_a));

	return (0);
}

int
fill_fpregs(struct thread *td, struct fpreg *regs)
{
#ifdef FPE
	struct pcb *pcb;

	pcb = td->td_pcb;

	if ((pcb->pcb_fpflags & PCB_FP_STARTED) != 0) {
		/*
		 * If we have just been running FPE instructions we will
		 * need to save the state to memcpy it below.
		 */
		if (td == curthread)
			fpe_state_save(td);

		memcpy(regs->fp_x, pcb->pcb_x, sizeof(regs->fp_x));
		regs->fp_fcsr = pcb->pcb_fcsr;
	} else
#endif
		memset(regs, 0, sizeof(*regs));

	return (0);
}

int
set_fpregs(struct thread *td, struct fpreg *regs)
{
#ifdef FPE
	struct trapframe *frame;
	struct pcb *pcb;

	frame = td->td_frame;
	pcb = td->td_pcb;

	memcpy(pcb->pcb_x, regs->fp_x, sizeof(regs->fp_x));
	pcb->pcb_fcsr = regs->fp_fcsr;
	pcb->pcb_fpflags |= PCB_FP_STARTED;
	frame->tf_sstatus &= ~SSTATUS_FS_MASK;
	frame->tf_sstatus |= SSTATUS_FS_CLEAN;
#endif

	return (0);
}

int
fill_dbregs(struct thread *td, struct dbreg *regs)
{

	panic("fill_dbregs");
}

int
set_dbregs(struct thread *td, struct dbreg *regs)
{

	panic("set_dbregs");
}

void
exec_setregs(struct thread *td, struct image_params *imgp, uintptr_t stack)
{
	struct trapframe *tf;
	struct pcb *pcb;

	tf = td->td_frame;
	pcb = td->td_pcb;

	memset(tf, 0, sizeof(struct trapframe));

	tf->tf_a[0] = stack;
	tf->tf_sp = STACKALIGN(stack);
	tf->tf_ra = imgp->entry_addr;
	tf->tf_sepc = imgp->entry_addr;

	pcb->pcb_fpflags &= ~PCB_FP_STARTED;
}

/* Sanity check these are the same size, they will be memcpy'd to and from */
CTASSERT(sizeof(((struct trapframe *)0)->tf_a) ==
    sizeof((struct gpregs *)0)->gp_a);
CTASSERT(sizeof(((struct trapframe *)0)->tf_s) ==
    sizeof((struct gpregs *)0)->gp_s);
CTASSERT(sizeof(((struct trapframe *)0)->tf_t) ==
    sizeof((struct gpregs *)0)->gp_t);
CTASSERT(sizeof(((struct trapframe *)0)->tf_a) ==
    sizeof((struct reg *)0)->a);
CTASSERT(sizeof(((struct trapframe *)0)->tf_s) ==
    sizeof((struct reg *)0)->s);
CTASSERT(sizeof(((struct trapframe *)0)->tf_t) ==
    sizeof((struct reg *)0)->t);

int
get_mcontext(struct thread *td, mcontext_t *mcp, int clear_ret)
{
	struct trapframe *tf = td->td_frame;

	memcpy(mcp->mc_gpregs.gp_t, tf->tf_t, sizeof(mcp->mc_gpregs.gp_t));
	memcpy(mcp->mc_gpregs.gp_s, tf->tf_s, sizeof(mcp->mc_gpregs.gp_s));
	memcpy(mcp->mc_gpregs.gp_a, tf->tf_a, sizeof(mcp->mc_gpregs.gp_a));

	if (clear_ret & GET_MC_CLEAR_RET) {
		mcp->mc_gpregs.gp_a[0] = 0;
		mcp->mc_gpregs.gp_t[0] = 0; /* clear syscall error */
	}

	mcp->mc_gpregs.gp_ra = tf->tf_ra;
	mcp->mc_gpregs.gp_sp = tf->tf_sp;
	mcp->mc_gpregs.gp_gp = tf->tf_gp;
	mcp->mc_gpregs.gp_tp = tf->tf_tp;
	mcp->mc_gpregs.gp_sepc = tf->tf_sepc;
	mcp->mc_gpregs.gp_sstatus = tf->tf_sstatus;
	get_fpcontext(td, mcp);

	return (0);
}

int
set_mcontext(struct thread *td, mcontext_t *mcp)
{
	struct trapframe *tf;

	tf = td->td_frame;

	/*
	 * Permit changes to the USTATUS bits of SSTATUS.
	 *
	 * Ignore writes to read-only bits (SD, XS).
	 *
	 * Ignore writes to the FS field as set_fpcontext() will set
	 * it explicitly.
	 */
	if (((mcp->mc_gpregs.gp_sstatus ^ tf->tf_sstatus) &
	    ~(SSTATUS_SD | SSTATUS_XS_MASK | SSTATUS_FS_MASK | SSTATUS_UPIE |
	    SSTATUS_UIE)) != 0)
		return (EINVAL);

	memcpy(tf->tf_t, mcp->mc_gpregs.gp_t, sizeof(tf->tf_t));
	memcpy(tf->tf_s, mcp->mc_gpregs.gp_s, sizeof(tf->tf_s));
	memcpy(tf->tf_a, mcp->mc_gpregs.gp_a, sizeof(tf->tf_a));

	tf->tf_ra = mcp->mc_gpregs.gp_ra;
	tf->tf_sp = mcp->mc_gpregs.gp_sp;
	tf->tf_gp = mcp->mc_gpregs.gp_gp;
	tf->tf_sepc = mcp->mc_gpregs.gp_sepc;
	tf->tf_sstatus = mcp->mc_gpregs.gp_sstatus;
	set_fpcontext(td, mcp);

	return (0);
}

static void
get_fpcontext(struct thread *td, mcontext_t *mcp)
{
#ifdef FPE
	struct pcb *curpcb;

	critical_enter();

	curpcb = curthread->td_pcb;

	KASSERT(td->td_pcb == curpcb, ("Invalid fpe pcb"));

	if ((curpcb->pcb_fpflags & PCB_FP_STARTED) != 0) {
		/*
		 * If we have just been running FPE instructions we will
		 * need to save the state to memcpy it below.
		 */
		fpe_state_save(td);

		KASSERT((curpcb->pcb_fpflags & ~PCB_FP_USERMASK) == 0,
		    ("Non-userspace FPE flags set in get_fpcontext"));
		memcpy(mcp->mc_fpregs.fp_x, curpcb->pcb_x,
		    sizeof(mcp->mc_fpregs.fp_x));
		mcp->mc_fpregs.fp_fcsr = curpcb->pcb_fcsr;
		mcp->mc_fpregs.fp_flags = curpcb->pcb_fpflags;
		mcp->mc_flags |= _MC_FP_VALID;
	}

	critical_exit();
#endif
}

static void
set_fpcontext(struct thread *td, mcontext_t *mcp)
{
#ifdef FPE
	struct pcb *curpcb;
#endif

	td->td_frame->tf_sstatus &= ~SSTATUS_FS_MASK;
	td->td_frame->tf_sstatus |= SSTATUS_FS_OFF;

#ifdef FPE
	critical_enter();

	if ((mcp->mc_flags & _MC_FP_VALID) != 0) {
		curpcb = curthread->td_pcb;
		/* FPE usage is enabled, override registers. */
		memcpy(curpcb->pcb_x, mcp->mc_fpregs.fp_x,
		    sizeof(mcp->mc_fpregs.fp_x));
		curpcb->pcb_fcsr = mcp->mc_fpregs.fp_fcsr;
		curpcb->pcb_fpflags = mcp->mc_fpregs.fp_flags & PCB_FP_USERMASK;
		td->td_frame->tf_sstatus |= SSTATUS_FS_CLEAN;
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
	struct sigframe *fp, frame;
	struct sysentvec *sysent;
	struct trapframe *tf;
	struct sigacts *psp;
	struct thread *td;
	struct proc *p;
	int onstack;
	int sig;

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

	tf->tf_a[0] = sig;
	tf->tf_a[1] = (register_t)&fp->sf_si;
	tf->tf_a[2] = (register_t)&fp->sf_uc;

	tf->tf_sepc = (register_t)catcher;
	tf->tf_sp = (register_t)fp;

	sysent = p->p_sysent;
	if (sysent->sv_sigcode_base != 0)
		tf->tf_ra = (register_t)sysent->sv_sigcode_base;
	else
		tf->tf_ra = (register_t)(PROC_PS_STRINGS(p) -
		    *(sysent->sv_szsigcode));

	CTR3(KTR_SIG, "sendsig: return td=%p pc=%#x sp=%#x", td, tf->tf_sepc,
	    tf->tf_sp);

	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}
