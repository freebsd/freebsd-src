/*-
 * Copyright (c) 1992 Terrence R. Lambert.
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	from: @(#)machdep.c	7.4 (Berkeley) 6/3/91
 *	from: src/sys/i386/i386/machdep.c,v 1.385.2.3 2000/05/10 02:04:46 obrien
 *	JNPR: pm_machdep.c,v 1.9.2.1 2007/08/16 15:59:10 girish
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysent.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/ucontext.h>
#include <sys/lock.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/ptrace.h>
#include <sys/syslog.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <machine/reg.h>
#include <machine/md_var.h>
#include <machine/sigframe.h>
#include <machine/vmparam.h>
#include <sys/vnode.h>
#include <fs/pseudofs/pseudofs.h>
#include <fs/procfs/procfs.h>

#define	UCONTEXT_MAGIC	0xACEDBADE

/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored
 * at top to call routine, followed by kcall
 * to sigreturn routine below.	After sigreturn
 * resets the signal mask, the stack, and the
 * frame pointer, it returns to the user
 * specified pc, psl.
 */
void
sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct proc *p;
	struct thread *td;
	struct trapframe *regs;
	struct sigacts *psp;
	struct sigframe sf, *sfp;
	int sig;
	int oonstack;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	sig = ksi->ksi_signo;
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);

	regs = td->td_frame;
	oonstack = sigonstack(regs->sp);

	/* save user context */
	bzero(&sf, sizeof(struct sigframe));
	sf.sf_uc.uc_sigmask = *mask;
	sf.sf_uc.uc_stack = td->td_sigstk;
	sf.sf_uc.uc_mcontext.mc_onstack = (oonstack) ? 1 : 0;
	sf.sf_uc.uc_mcontext.mc_pc = regs->pc;
	sf.sf_uc.uc_mcontext.mullo = regs->mullo;
	sf.sf_uc.uc_mcontext.mulhi = regs->mulhi;
	sf.sf_uc.uc_mcontext.mc_tls = td->td_md.md_tls;
	sf.sf_uc.uc_mcontext.mc_regs[0] = UCONTEXT_MAGIC;  /* magic number */
	bcopy((void *)&regs->ast, (void *)&sf.sf_uc.uc_mcontext.mc_regs[1],
	    sizeof(sf.sf_uc.uc_mcontext.mc_regs) - sizeof(register_t));
	sf.sf_uc.uc_mcontext.mc_fpused = td->td_md.md_flags & MDTD_FPUSED;
	if (sf.sf_uc.uc_mcontext.mc_fpused) {
		/* if FPU has current state, save it first */
		if (td == PCPU_GET(fpcurthread))
			MipsSaveCurFPState(td);
		bcopy((void *)&td->td_frame->f0,
		    (void *)sf.sf_uc.uc_mcontext.mc_fpregs,
		    sizeof(sf.sf_uc.uc_mcontext.mc_fpregs));
	}

	/* Allocate and validate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sfp = (struct sigframe *)((vm_offset_t)(td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size - sizeof(struct sigframe))
		    & ~(sizeof(__int64_t) - 1));
	} else
		sfp = (struct sigframe *)((vm_offset_t)(regs->sp - 
		    sizeof(struct sigframe)) & ~(sizeof(__int64_t) - 1));

	/* Translate the signal if appropriate */
	if (p->p_sysent->sv_sigtbl) {
		if (sig <= p->p_sysent->sv_sigsize)
			sig = p->p_sysent->sv_sigtbl[_SIG_IDX(sig)];
	}

	/* Build the argument list for the signal handler. */
	regs->a0 = sig;
	regs->a2 = (register_t)(intptr_t)&sfp->sf_uc;
	if (SIGISMEMBER(psp->ps_siginfo, sig)) {
		/* Signal handler installed with SA_SIGINFO. */
		regs->a1 = (register_t)(intptr_t)&sfp->sf_si;
		/* sf.sf_ahu.sf_action = (__siginfohandler_t *)catcher; */

		/* fill siginfo structure */
		sf.sf_si.si_signo = sig;
		sf.sf_si.si_code = ksi->ksi_code;
		sf.sf_si.si_addr = (void*)(intptr_t)regs->badvaddr;
	} else {
		/* Old FreeBSD-style arguments. */
		regs->a1 = ksi->ksi_code;
		regs->a3 = regs->badvaddr;
		/* sf.sf_ahu.sf_handler = catcher; */
	}

	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	/*
	 * Copy the sigframe out to the user's stack.
	 */
	if (copyout(&sf, sfp, sizeof(struct sigframe)) != 0) {
		/*
		 * Something is wrong with the stack pointer.
		 * ...Kill the process.
		 */
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	regs->pc = (register_t)(intptr_t)catcher;
	regs->t9 = (register_t)(intptr_t)catcher;
	regs->sp = (register_t)(intptr_t)sfp;
	/*
	 * Signal trampoline code is at base of user stack.
	 */
	regs->ra = (register_t)(intptr_t)PS_STRINGS - *(p->p_sysent->sv_szsigcode);
	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}

#ifdef GONE_IN_7
/*
 * Build siginfo_t for SA thread
 */
void
cpu_thread_siginfo(int sig, u_long code, siginfo_t *si)
{
	struct proc *p;
	struct thread *td;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);

	bzero(si, sizeof(*si));
	si->si_signo = sig;
	si->si_code = code;
	/* XXXKSE fill other fields */
}
#endif

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc as specified by
 * context left by sendsig.
 */
int
sys_sigreturn(struct thread *td, struct sigreturn_args *uap)
{
	ucontext_t uc;
	int error;

	error = copyin(uap->sigcntxp, &uc, sizeof(uc));
	if (error != 0)
	    return (error);

	error = set_mcontext(td, &uc.uc_mcontext);
	if (error != 0)
		return (error);

	kern_sigprocmask(td, SIG_SETMASK, &uc.uc_sigmask, NULL, 0);

	return (EJUSTRETURN);
}

int
ptrace_set_pc(struct thread *td, unsigned long addr)
{
	td->td_frame->pc = (register_t) addr;
	return 0;
}

static int
ptrace_read_int(struct thread *td, off_t addr, int *v)
{
	struct iovec iov;
	struct uio uio;

	PROC_LOCK_ASSERT(td->td_proc, MA_NOTOWNED);
	iov.iov_base = (caddr_t) v;
	iov.iov_len = sizeof(int);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)addr;
	uio.uio_resid = sizeof(int);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_td = td;
	return proc_rwmem(td->td_proc, &uio);
}

static int
ptrace_write_int(struct thread *td, off_t addr, int v)
{
	struct iovec iov;
	struct uio uio;

	PROC_LOCK_ASSERT(td->td_proc, MA_NOTOWNED);
	iov.iov_base = (caddr_t) &v;
	iov.iov_len = sizeof(int);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)addr;
	uio.uio_resid = sizeof(int);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	uio.uio_td = td;
	return proc_rwmem(td->td_proc, &uio);
}

int
ptrace_single_step(struct thread *td)
{
	unsigned va;
	struct trapframe *locr0 = td->td_frame;
	int i;
	int bpinstr = MIPS_BREAK_SSTEP;
	int curinstr;
	struct proc *p;

	p = td->td_proc;
	PROC_UNLOCK(p);
	/*
	 * Fetch what's at the current location.
	 */
	ptrace_read_int(td,  (off_t)locr0->pc, &curinstr);

	/* compute next address after current location */
	if(curinstr != 0) {
		va = MipsEmulateBranch(locr0, locr0->pc, locr0->fsr,
		    (uintptr_t)&curinstr);
	} else {
		va = locr0->pc + 4;
	}
	if (td->td_md.md_ss_addr) {
		printf("SS %s (%d): breakpoint already set at %x (va %x)\n",
		    p->p_comm, p->p_pid, td->td_md.md_ss_addr, va); /* XXX */
		return (EFAULT);
	}
	td->td_md.md_ss_addr = va;
	/*
	 * Fetch what's at the current location.
	 */
	ptrace_read_int(td, (off_t)va, &td->td_md.md_ss_instr);

	/*
	 * Store breakpoint instruction at the "next" location now.
	 */
	i = ptrace_write_int (td, va, bpinstr);

	/*
	 * The sync'ing of I & D caches is done by procfs_domem()
	 * through procfs_rwmem().
	 */

	PROC_LOCK(p);
	if (i < 0)
		return (EFAULT);
#if 0
	printf("SS %s (%d): breakpoint set at %x: %x (pc %x) br %x\n",
	    p->p_comm, p->p_pid, p->p_md.md_ss_addr,
	    p->p_md.md_ss_instr, locr0->pc, curinstr); /* XXX */
#endif
	return (0);
}


void
makectx(struct trapframe *tf, struct pcb *pcb)
{

	pcb->pcb_regs.ra = tf->ra;
	pcb->pcb_regs.pc = tf->pc;
	pcb->pcb_regs.sp = tf->sp;
}

int
fill_regs(struct thread *td, struct reg *regs)
{
	memcpy(regs, td->td_frame, sizeof(struct reg));
	return (0);
}

int
set_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *f;
	register_t sr;

	f = (struct trapframe *) td->td_frame;
	/*
	 * Don't allow the user to change SR
	 */
	sr = f->sr;
	memcpy(td->td_frame, regs, sizeof(struct reg));
	f->sr = sr;
	return (0);
}

int
get_mcontext(struct thread *td, mcontext_t *mcp, int flags)
{
	struct trapframe *tp;

	tp = td->td_frame;
	PROC_LOCK(curthread->td_proc);
	mcp->mc_onstack = sigonstack(tp->sp);
	PROC_UNLOCK(curthread->td_proc);
	bcopy((void *)&td->td_frame->zero, (void *)&mcp->mc_regs,
	    sizeof(mcp->mc_regs));

	mcp->mc_fpused = td->td_md.md_flags & MDTD_FPUSED;
	if (mcp->mc_fpused) {
		bcopy((void *)&td->td_frame->f0, (void *)&mcp->mc_fpregs,
		    sizeof(mcp->mc_fpregs));
	}
	if (flags & GET_MC_CLEAR_RET) {
		mcp->mc_regs[V0] = 0;
		mcp->mc_regs[V1] = 0;
		mcp->mc_regs[A3] = 0;
	}

	mcp->mc_pc = td->td_frame->pc;
	mcp->mullo = td->td_frame->mullo;
	mcp->mulhi = td->td_frame->mulhi;
	mcp->mc_tls = td->td_md.md_tls;
	return (0);
}

int
set_mcontext(struct thread *td, const mcontext_t *mcp)
{
	struct trapframe *tp;

	tp = td->td_frame;
	bcopy((void *)&mcp->mc_regs, (void *)&td->td_frame->zero,
	    sizeof(mcp->mc_regs));

	td->td_md.md_flags = mcp->mc_fpused & MDTD_FPUSED;
	if (mcp->mc_fpused) {
		bcopy((void *)&mcp->mc_fpregs, (void *)&td->td_frame->f0,
		    sizeof(mcp->mc_fpregs));
	}
	td->td_frame->pc = mcp->mc_pc;
	td->td_frame->mullo = mcp->mullo;
	td->td_frame->mulhi = mcp->mulhi;
	td->td_md.md_tls = mcp->mc_tls;
	/* Dont let user to set any bits in Status and casue registers */

	return (0);
}

int
fill_fpregs(struct thread *td, struct fpreg *fpregs)
{
	if (td == PCPU_GET(fpcurthread))
		MipsSaveCurFPState(td);
	memcpy(fpregs, &td->td_frame->f0, sizeof(struct fpreg)); 
	return 0;
}

int
set_fpregs(struct thread *td, struct fpreg *fpregs)
{
	if (PCPU_GET(fpcurthread) == td)
		PCPU_SET(fpcurthread, (struct thread *)0);
	memcpy(&td->td_frame->f0, fpregs, sizeof(struct fpreg));
	return 0;
}


/*
 * Clear registers on exec
 * $sp is set to the stack pointer passed in.  $pc is set to the entry
 * point given by the exec_package passed in, as is $t9 (used for PIC
 * code by the MIPS elf abi).
 */
void
exec_setregs(struct thread *td, struct image_params *imgp, u_long stack)
{

	bzero((caddr_t)td->td_frame, sizeof(struct trapframe));

	/*
	 * The stack pointer has to be aligned to accommodate the largest
	 * datatype at minimum.  This probably means it should be 16-byte
	 * aligned, but for now we're 8-byte aligning it.
	 */
	td->td_frame->sp = ((register_t) stack) & ~(sizeof(__int64_t) - 1);

	/*
	 * If we're running o32 or n32 programs but have 64-bit registers,
	 * GCC may use stack-relative addressing near the top of user
	 * address space that, due to sign extension, will yield an
	 * invalid address.  For instance, if sp is 0x7fffff00 then GCC
	 * might do something like this to load a word from 0x7ffffff0:
	 *
	 * 	addu	sp, sp, 32768
	 * 	lw	t0, -32528(sp)
	 *
	 * On systems with 64-bit registers, sp is sign-extended to
	 * 0xffffffff80007f00 and the load is instead done from
	 * 0xffffffff7ffffff0.
	 *
	 * To prevent this, we subtract 64K from the stack pointer here.
	 *
	 * For consistency, we should just always do this unless we're
	 * running n64 programs.  For now, since we don't support
	 * COMPAT_FREEBSD32 on n64 kernels, we just do it unless we're
	 * running n64 kernels.
	 */
#if !defined(__mips_n64)
	td->td_frame->sp -= 65536;
#endif

	td->td_frame->pc = imgp->entry_addr & ~3;
	td->td_frame->t9 = imgp->entry_addr & ~3; /* abicall req */
	td->td_frame->sr = MIPS_SR_KSU_USER | MIPS_SR_EXL | MIPS_SR_INT_IE |
	    (mips_rd_status() & MIPS_SR_INT_MASK);
#if defined(__mips_n32) 
	td->td_frame->sr |= MIPS_SR_PX;
#elif  defined(__mips_n64)
	td->td_frame->sr |= MIPS_SR_PX | MIPS_SR_UX | MIPS_SR_KX;
#endif
	/*
	 * FREEBSD_DEVELOPERS_FIXME:
	 * Setup any other CPU-Specific registers (Not MIPS Standard)
	 * and/or bits in other standard MIPS registers (if CPU-Specific)
	 *  that are needed.
	 */

	/*
	 * Set up arguments for the rtld-capable crt0:
	 *	a0	stack pointer
	 *	a1	rtld cleanup (filled in by dynamic loader)
	 *	a2	rtld object (filled in by dynamic loader)
	 *	a3	ps_strings
	 */
	td->td_frame->a0 = (register_t) stack;
	td->td_frame->a1 = 0;
	td->td_frame->a2 = 0;
	td->td_frame->a3 = (register_t)imgp->ps_strings;

	td->td_md.md_flags &= ~MDTD_FPUSED;
	if (PCPU_GET(fpcurthread) == td)
	    PCPU_SET(fpcurthread, (struct thread *)0);
	td->td_md.md_ss_addr = 0;
}

int
ptrace_clear_single_step(struct thread *td)
{
	int i;
	struct proc *p;

	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (!td->td_md.md_ss_addr)
		return EINVAL;

	/*
	 * Restore original instruction and clear BP
	 */
	i = ptrace_write_int (td, td->td_md.md_ss_addr, td->td_md.md_ss_instr);

	/* The sync'ing of I & D caches is done by procfs_domem(). */

	if (i < 0) {
		log(LOG_ERR, "SS %s %d: can't restore instruction at %x: %x\n",
		    p->p_comm, p->p_pid, td->td_md.md_ss_addr,
		    td->td_md.md_ss_instr);
	}
	td->td_md.md_ss_addr = 0;
	return 0;
}
