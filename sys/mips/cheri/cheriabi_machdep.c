/*-
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1987, 1990, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989, 1990 William Jolitz
 * Copyright (c) 1992 Terrence R. Lambert.
 * Copyright (c) 1994 John Dyson
 * Copyright (c) 2015 SRI International
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#include "opt_compat.h"
#include "opt_ddb.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/sysent.h>
#include <sys/signal.h>
#include <sys/proc.h>
#include <sys/imgact_elf.h>
#include <sys/imgact.h>
#include <sys/mman.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/ucontext.h>

/* Required by cheriabi_fill_uap.h */
#include <sys/capsicum.h>
#include <sys/linker.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/mqueue.h>
#include <sys/poll.h>
#include <sys/procctl.h>
#include <sys/resource.h>
#include <sys/sched.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/timeffc.h>
#include <sys/timex.h>
#include <sys/uuid.h>
#include <netinet/sctp.h>

#include <machine/cheri.h>
#include <machine/cpuinfo.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/sigframe.h>
#include <machine/sysarch.h>
#include <machine/tls.h>

#include <sys/cheriabi.h>

#include <compat/cheriabi/cheriabi.h>
#include <compat/cheriabi/cheriabi_proto.h>
#include <compat/cheriabi/cheriabi_syscall.h>
#include <compat/cheriabi/cheriabi_sysargmap.h>
#include <compat/cheriabi/cheriabi_util.h>

#include <compat/cheriabi/cheriabi_signal.h>
#include <compat/cheriabi/cheriabi_aio.h>
#include <compat/cheriabi/cheriabi_ioctl.h>
#include <compat/cheriabi/cheriabi_fill_uap.h>
#include <compat/cheriabi/cheriabi_fill_uap_manual.h>
#include <compat/cheriabi/cheriabi_dispatch_fill_uap.h>

#include <ddb/ddb.h>
#include <sys/kdb.h>

#define	DELAYBRANCH(x)	((int)(x) < 0)
#define	UCONTEXT_MAGIC	0xACEDBADE

static int	cheriabi_fetch_syscall_args(struct thread *td,
		    struct syscall_args *sa);
static void	cheriabi_set_syscall_retval(struct thread *td, int error);
static void	cheriabi_sendsig(sig_t, ksiginfo_t *, sigset_t *);
static void	cheriabi_exec_setregs(struct thread *, struct image_params *,
		    u_long);

extern const char *cheriabi_syscallnames[];

struct sysentvec elf_freebsd_cheriabi_sysvec = {
	.sv_size	= CHERIABI_SYS_MAXSYSCALL,
	.sv_table	= cheriabi_sysent,
	.sv_mask	= 0,
	.sv_errsize	= 0,
	.sv_errtbl	= NULL,
	.sv_fixup	= cheriabi_elf_fixup,
	.sv_sendsig	= cheriabi_sendsig,
	.sv_sigcode	= cheri_sigcode,
	.sv_szsigcode	= &szcheri_sigcode,
	.sv_name	= "FreeBSD-CHERI ELF64",
	.sv_coredump	= __elfN(coredump),
	.sv_imgact_try	= NULL,
	.sv_minsigstksz	= MINSIGSTKSZ,	/* XXXBD: or something bigger? */
	.sv_pagesize	= PAGE_SIZE,
	.sv_minuser	= VM_MIN_ADDRESS,
	.sv_maxuser	= VM_MAXUSER_ADDRESS,
	.sv_usrstack	= USRSTACK,
	.sv_psstrings	= CHERIABI_PS_STRINGS,
	.sv_stackprot	= VM_PROT_ALL,
	.sv_copyout_strings = cheriabi_copyout_strings,
	.sv_setregs	= cheriabi_exec_setregs,
	.sv_fixlimit	= NULL,
	.sv_maxssiz	= NULL,
	.sv_flags	= SV_ABI_FREEBSD | SV_LP64 | SV_CHERI,
	.sv_set_syscall_retval = cheriabi_set_syscall_retval,
	.sv_fetch_syscall_args = cheriabi_fetch_syscall_args,
	.sv_syscallnames = cheriabi_syscallnames,
	.sv_schedtail	= NULL,
};
INIT_SYSENTVEC(cheriabi_sysent, &elf_freebsd_cheriabi_sysvec);

static Elf64_Brandinfo freebsd_cheriabi_brand_info = {
	.brand		= ELFOSABI_FREEBSD,
	.machine	= EM_MIPS_CHERI,
	.compat_3_brand	= "FreeBSD",
	.emul_path	= NULL,
	.interp_path	= "/libexec/ld-elf.so.1",
	.sysvec		= &elf_freebsd_cheriabi_sysvec,
	.interp_newpath = NULL,
	.flags		= 0
};

SYSINIT(cheriabi, SI_SUB_EXEC, SI_ORDER_ANY,
    (sysinit_cfunc_t) elf64_insert_brand_entry,
    &freebsd_cheriabi_brand_info);

void
cheriabi_fetch_syscall_arg(struct thread *td, struct chericap *arg,
    int syscall_no, int argnum)
{
	struct trapframe *locr0 = td->td_frame;	 /* aka td->td_pcb->pcv_regs */
	struct cheri_frame *capreg = &td->td_pcb->pcb_cheriframe;
	struct chericap *arg_capp;
	struct sysentvec *se;
	int i, intreg_offset, ptrreg_offset, ptrmask, is_ptr_arg;
	register_t arg_reg;

	se = td->td_proc->p_sysent;

	KASSERT(syscall_no >= 0, ("Negative syscall number %d\n", syscall_no));
	KASSERT(syscall_no < se->sv_size,
	    ("Syscall number too large %d >= %d\n", syscall_no, se->sv_size));
	KASSERT(argnum >= 0, ("Negative argument number %d\n", argnum));
	KASSERT(argnum <= se->sv_table[syscall_no].sy_narg,
	    ("Argument number out of range %d > %d\n", argnum,
	    se->sv_table[syscall_no].sy_narg));

	ptrmask = CHERIABI_SYS_argmap[syscall_no].sam_ptrmask;
	/* XXX: O(1) possible with more bit twiddling. */
	intreg_offset = ptrreg_offset = -1;
	for (i = 0; i <= argnum; i++) {
		if (ptrmask & (1 << i)) {
			is_ptr_arg = 1;
			ptrreg_offset++;
		} else {
			is_ptr_arg = 0;
			intreg_offset++;
		}
	}

	if (is_ptr_arg) {
		switch (ptrreg_offset) {
		case 0:	arg_capp = &capreg->cf_c3;	break;
		case 1:	arg_capp = &capreg->cf_c4;	break;
		case 2:	arg_capp = &capreg->cf_c5;	break;
		case 3:	arg_capp = &capreg->cf_c6;	break;
		case 4:	arg_capp = &capreg->cf_c7;	break;
		case 5:	arg_capp = &capreg->cf_c8;	break;
		case 6:	arg_capp = &capreg->cf_c9;	break;
		case 7:	arg_capp = &capreg->cf_c10;	break;
		default:
			panic("%s: pointer argument %d out of range",
			    __func__, ptrreg_offset);
		}
		cheri_capability_copy(arg, arg_capp);
	} else {
		switch (intreg_offset) {
		case 0:	arg_reg = locr0->a0;	break;
		case 1:	arg_reg = locr0->a1;	break;
		case 2:	arg_reg = locr0->a2;	break;
		case 3:	arg_reg = locr0->a3;	break;
		case 4:	arg_reg = locr0->a4;	break;
		case 5:	arg_reg = locr0->a5;	break;
		case 6:	arg_reg = locr0->a6;	break;
		case 7:	arg_reg = locr0->a7;	break;
		default:
			panic("%s: integer argument %d out of range",
			    __func__, intreg_offset);
		}
		cheri_capability_set_null(arg);
		cheri_capability_setoffset(arg, arg_reg);
	}
}

static int
cheriabi_fetch_syscall_args(struct thread *td, struct syscall_args *sa)
{
	struct trapframe *locr0 = td->td_frame;	 /* aka td->td_pcb->pcv_regs */
	struct sysentvec *se;
#ifdef OLD_ARG_HANDLING
	struct cheri_frame *capreg = &td->td_pcb->pcb_cheriframe;
	register_t intargs[8];
	uintptr_t ptrargs[8];
	u_int tag;
	int i, isaved, psaved, curint, curptr, nintargs, nptrargs;
#endif
	int error;

	error = 0;

	bzero(sa->args, sizeof(sa->args));

	/* compute next PC after syscall instruction */
	td->td_pcb->pcb_tpc = sa->trapframe->pc; /* Remember if restart */
	if (DELAYBRANCH(sa->trapframe->cause))	 /* Check BD bit */
		locr0->pc = MipsEmulateBranch(locr0, sa->trapframe->pc, 0, 0);
	else
		locr0->pc += sizeof(int);
	sa->code = locr0->v0;

	se = td->td_proc->p_sysent;
	if (se->sv_mask)
		sa->code &= se->sv_mask;

	if (sa->code >= se->sv_size)
		sa->callp = &se->sv_table[0];
	else
		sa->callp = &se->sv_table[sa->code];

	sa->narg = sa->callp->sy_narg;

#ifndef OLD_ARG_HANDLING
	error = cheriabi_dispatch_fill_uap(td, sa->code, sa->args);
#else

	intargs[0] = locr0->a0;
	intargs[1] = locr0->a1;
	intargs[2] = locr0->a2;
	intargs[3] = locr0->a3;
	intargs[4] = locr0->a4;
	intargs[5] = locr0->a5;
	intargs[6] = locr0->a6;
	intargs[7] = locr0->a7;
	isaved = 8;

#if defined(CPU_CHERI_CHERI0) || defined (CPU_CHERI_CHERI8) || defined(CPU_CHERI_CHERI16)
#error	CHERIABI does not support fewer than 8 argument registers
#endif
	/*
	 * XXXBD: We should ideally use a user capability rather than KDC
	 * to generate the pointers, but then we have to answer: which one?
	 *
	 * XXXRW: The kernel cannot distinguish between pointers with tags vs.
	 * untagged (possible) integers, which is problematic when a
	 * system-call argument is an intptr_t.  We used to just use CToPtr
	 * here, but this caused untagged integer arguments to be lost.  Now
	 * we pick one of CToPtr and CToInt based on the tag -- but this is
	 * not really ideal.  Instead, we'd prefer that the kernel could
	 * differentiate between the two explicitly using tagged capabilities,
	 * which we're not yet ready to do.
	 */
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &capreg->cf_c3, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (tag)
		CHERI_CTOPTR(ptrargs[0], CHERI_CR_CTEMP0, CHERI_CR_KDC);
	else
		CHERI_CTOINT(ptrargs[0], CHERI_CR_CTEMP0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &capreg->cf_c4, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (tag)
		CHERI_CTOPTR(ptrargs[1], CHERI_CR_CTEMP0, CHERI_CR_KDC);
	else
		CHERI_CTOINT(ptrargs[1], CHERI_CR_CTEMP0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &capreg->cf_c5, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (tag)
		CHERI_CTOPTR(ptrargs[2], CHERI_CR_CTEMP0, CHERI_CR_KDC);
	else
		CHERI_CTOINT(ptrargs[2], CHERI_CR_CTEMP0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &capreg->cf_c6, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (tag)
		CHERI_CTOPTR(ptrargs[3], CHERI_CR_CTEMP0, CHERI_CR_KDC);
	else
		CHERI_CTOINT(ptrargs[3], CHERI_CR_CTEMP0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &capreg->cf_c7, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (tag)
		CHERI_CTOPTR(ptrargs[4], CHERI_CR_CTEMP0, CHERI_CR_KDC);
	else
		CHERI_CTOINT(ptrargs[4], CHERI_CR_CTEMP0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &capreg->cf_c8, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (tag)
		CHERI_CTOPTR(ptrargs[5], CHERI_CR_CTEMP0, CHERI_CR_KDC);
	else
		CHERI_CTOINT(ptrargs[5], CHERI_CR_CTEMP0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &capreg->cf_c9, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (tag)
		CHERI_CTOPTR(ptrargs[6], CHERI_CR_CTEMP0, CHERI_CR_KDC);
	else
		CHERI_CTOINT(ptrargs[6], CHERI_CR_CTEMP0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &capreg->cf_c10, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (tag)
		CHERI_CTOPTR(ptrargs[7], CHERI_CR_CTEMP0, CHERI_CR_KDC);
	else
		CHERI_CTOINT(ptrargs[7], CHERI_CR_CTEMP0);
	psaved = 8;

#ifdef TRAP_DEBUG
	if (trap_debug)
		printf("SYSCALL #%d pid:%u\n", sa->code, td->td_proc->p_pid);
#endif

	nptrargs = bitcount(CHERIABI_SYS_argmap[sa->code].sam_ptrmask);
	nintargs = sa->narg - nptrargs;
	KASSERT(nintargs <= isaved,
	    ("SYSCALL #%u pid:%u, nintargs (%u) > isaved (%u).\n",
	     sa->code, td->td_proc->p_pid, nintargs, isaved));
	KASSERT(nptrargs <= psaved,
	    ("SYSCALL #%u pid:%u, nptrargs (%u) > psaved (%u).\n",
	     sa->code, td->td_proc->p_pid, nptrargs, psaved));

	/*
	 * Check each argument to see if it is a pointer and pop an argument
	 * off the appropriate list.
	 */
	curint = curptr = 0;
	for (i = 0; i < sa->narg; i++)
		sa->args[i] =
		    (CHERIABI_SYS_argmap[sa->code].sam_ptrmask & 1 << i) ?
		    ptrargs[curptr++] : intargs[curint++];
#endif /* OLD_ARG_HANDLING */

	td->td_retval[0] = 0;
	td->td_retval[1] = locr0->v1;

	return (error);
}

static void
cheriabi_set_syscall_retval(struct thread *td, int error)
{
	struct trapframe *locr0 = td->td_frame;
	struct cheri_frame *capreg = &td->td_pcb->pcb_cheriframe;
	register_t a0;
	unsigned int code;
	struct sysentvec *se;

	code = locr0->v0;
	a0 = locr0->a0;

	se = td->td_proc->p_sysent;
	/*
	 * When programs start up, they pass through the return path
	 * (maybe via execve?).  When this happens, code is an absurd
	 * and out of range value.
	 */
	if (code > se->sv_size)
		code = 0;

	switch (error) {
	case 0:
		locr0->v0 = td->td_retval[0];
		locr0->v1 = td->td_retval[1];
		locr0->a3 = 0;

		if (!CHERIABI_SYS_argmap[code].sam_return_ptr)
			break;

		switch (code) {
		case CHERIABI_SYS_cheriabi_mmap:
			cheriabi_mmap_set_retcap(td, &capreg->cf_c3,
			&capreg->cf_c3, locr0->a0, locr0->a1, locr0->a2);
			break;

		default:
			panic("%s: unsupported syscall (%u) returning pointer",
			    __func__, code);
		}
		break;
	case ERESTART:
		locr0->pc = td->td_pcb->pcb_tpc;
		break;

	case EJUSTRETURN:
		break;	/* nothing to do */

	default:
		locr0->v0 = error;
		locr0->a3 = 1;
	}
}

static int
cheriabi_set_mcontext(struct thread *td, mcontext_c_t *mcp)
{
	struct trapframe *tp;
	int tag;

	if (mcp->mc_regs[0] != UCONTEXT_MAGIC) {
		printf("mcp->mc_regs[0] != UCONTEXT_MAGIC\n");
		return (EINVAL);
	}

	cheri_memcpy(&td->td_pcb->pcb_cheriframe, &mcp->mc_cheriframe,
	    sizeof(td->td_pcb->pcb_cheriframe));

	tp = td->td_frame;
	bcopy((void *)&mcp->mc_regs, (void *)&td->td_frame->zero,
	    sizeof(mcp->mc_regs));

	td->td_md.md_flags = mcp->mc_fpused & MDTD_FPUSED;
	if (mcp->mc_fpused)
		bcopy((void *)&mcp->mc_fpregs, (void *)&td->td_frame->f0,
		    sizeof(mcp->mc_fpregs));
	td->td_frame->pc = mcp->mc_pc;
	td->td_frame->mullo = mcp->mullo;
	td->td_frame->mulhi = mcp->mulhi;

	cheri_capability_copy(&td->td_md.md_tls_cap, &mcp->mc_tls);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &mcp->mc_tls, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (tag)
		CHERI_CTOPTR(td->td_md.md_tls, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	else
		td->td_md.md_tls = NULL;

	/* Dont let user to set any bits in status and cause registers.  */

	return (0);
}

int
cheriabi_sigreturn(struct thread *td, struct cheriabi_sigreturn_args *uap)
{
	ucontext_c_t uc;
	int error;

	error = copyincap(uap->sigcntxp, &uc, sizeof(uc));
	if (error != 0)
		return (error);

	error = cheriabi_set_mcontext(td, &uc.uc_mcontext);
	if (error != 0)
		return (error);

	kern_sigprocmask(td, SIG_SETMASK, &uc.uc_sigmask, NULL, 0);

	return (EJUSTRETURN);
}

int
cheriabi_getcontext(struct thread *td, struct cheriabi_getcontext_args *uap)
{

	return (ENOSYS);
}

int
cheriabi_setcontext(struct thread *td, struct cheriabi_setcontext_args *uap)
{

	return (ENOSYS);
}

int
cheriabi_swapcontext(struct thread *td, struct cheriabi_swapcontext_args *uap)
{

	return (ENOSYS);
}

/*
 * The CheriABI version of sendsig(9) largely borrows from the MIPS version,
 * and it is important to keep them in sync.  It differs primarily in that it
 * must also be aware of user stack-handling ABIs, so is also sensitive to our
 * (fluctuating) design choices in how $c11 and $sp interact.  The current
 * design uses ($c11 + $sp) for stack-relative references, so early on we have
 * to calculate a 'relocated' version of $sp that we can then use for
 * MIPS-style access.
 *
 * This code, as with the CHERI-aware MIPS code, makes a privilege
 * determination in order to decide whether to trust the stack exposed by the
 * user code for the purposes of signal handling.  We must use the alternative
 * stack if there is any indication that using the user thread's stack state
 * might violate the userspace compartmentalisation model.
 */
static void
cheriabi_sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct proc *p;
	struct thread *td;
	struct trapframe *regs;
	struct cheri_frame *capreg;
	struct sigacts *psp;
	struct sigframe_c sf, *sfp;
	uintptr_t stackbase;
	vm_offset_t sp;
	int cheri_is_sandboxed;
	int sig;
	int oonstack;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	sig = ksi->ksi_signo;
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);

	regs = td->td_frame;
	capreg = &td->td_pcb->pcb_cheriframe;

	/*
	 * In CheriABI, $sp is $c11 relative, so calculate a relocation base
	 * that must be combined with regs->sp from this point onwards.
	 * Unfortunately, we won't retain bounds and permissions information
	 * (as is the case elsewhere in CheriABI).  While 'stackbase'
	 * suggests that $c11's offset isn't included, in practice it will be,
	 * although we may reasonably assume that it will be zero.
	 *
	 * If it turns out we will be delivering to the alternative signal
	 * stack, we'll recalculate stackbase later.
	 */
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC,
	    &td->td_pcb->pcb_cheriframe.cf_c11, 0);
	CHERI_CTOPTR(stackbase, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	oonstack = sigonstack(stackbase + regs->sp);

	/*
	 * CHERI affects signal delivery in the following ways:
	 *
	 * (1) Additional capability-coprocessor state is exposed via
	 *     extensions to the context frame placed on the stack.
	 *
	 * (2) If the user $pcc doesn't include CHERI_PERM_SYSCALL, then we
	 *     consider user state to be 'sandboxed' and therefore to require
	 *     special delivery handling which includes a domain-switch to the
	 *     thread's context-switch domain.  (This is done by
	 *     cheri_sendsig()).
	 *
	 * (3) If an alternative signal stack is not defined, and we are in a
	 *     'sandboxed' state, then we have two choices: (a) if the signal
	 *     is of type SA_SANDBOX_UNWIND, we will automatically unwind the
	 *     trusted stack by one frame; (b) otherwise, we will terminate
	 *     the process unconditionally.
	 */
	cheri_is_sandboxed = cheri_signal_sandboxed(td);

	/*
	 * We provide the ability to drop into the debugger in two different
	 * circumstances: (1) if the code running is sandboxed; and (2) if the
	 * fault is a CHERI protection fault.  Handle both here for the
	 * non-unwind case.  Do this before we rewrite any general-purpose or
	 * capability register state for the thread.
	 */
#if DDB
	if (cheri_is_sandboxed && security_cheri_debugger_on_sandbox_signal)
		kdb_enter(KDB_WHY_CHERI, "Signal delivery to CHERI sandbox");
	else if (sig == SIGPROT && security_cheri_debugger_on_sigprot)
		kdb_enter(KDB_WHY_CHERI,
		    "SIGPROT delivered outside sandbox");
#endif

	/*
	 * If a thread is running sandboxed, we can't rely on $sp which may
	 * not point at a valid stack in the ambient context, or even be
	 * maliciously manipulated.  We must therefore always use the
	 * alternative stack.  We are also therefore unable to tell whether we
	 * are on the alternative stack, so must clear 'oonstack' here.
	 *
	 * XXXRW: This requires significant further thinking; however, the net
	 * upshot is that it is not a good idea to do an object-capability
	 * invoke() from a signal handler, as with so many other things in
	 * life.
	 */
	if (cheri_is_sandboxed != 0)
		oonstack = 0;

	/* save user context */
	bzero(&sf, sizeof(sf));
	sf.sf_uc.uc_sigmask = *mask;
#if 0
	/*
	 * XXX-BD: stack_t type differs and we can't just fake a capabilty.
	 * We don't restore the value so what purpose does it serve?
	 */
	sf.sf_uc.uc_stack = td->td_sigstk;
#endif
	sf.sf_uc.uc_mcontext.mc_onstack = (oonstack) ? 1 : 0;
	sf.sf_uc.uc_mcontext.mc_pc = regs->pc;
	sf.sf_uc.uc_mcontext.mullo = regs->mullo;
	sf.sf_uc.uc_mcontext.mulhi = regs->mulhi;
	cheri_capability_copy(&sf.sf_uc.uc_mcontext.mc_tls,
	    &td->td_md.md_tls_cap);
	sf.sf_uc.uc_mcontext.mc_regs[0] = UCONTEXT_MAGIC;  /* magic number */
	bcopy((void *)&regs->ast, (void *)&sf.sf_uc.uc_mcontext.mc_regs[1],
	    sizeof(sf.sf_uc.uc_mcontext.mc_regs) - sizeof(register_t));
	sf.sf_uc.uc_mcontext.mc_fpused = td->td_md.md_flags & MDTD_FPUSED;
#if defined(CPU_HAVEFPU)
	if (sf.sf_uc.uc_mcontext.mc_fpused) {
		/* if FPU has current state, save it first */
		if (td == PCPU_GET(fpcurthread))
			MipsSaveCurFPState(td);
		bcopy((void *)&td->td_frame->f0,
		    (void *)sf.sf_uc.uc_mcontext.mc_fpregs,
		    sizeof(sf.sf_uc.uc_mcontext.mc_fpregs));
	}
#endif
	/* XXXRW: sf.sf_uc.uc_mcontext.sr seems never to be set? */
	sf.sf_uc.uc_mcontext.cause = regs->cause;
	cheri_memcpy(&sf.sf_uc.uc_mcontext.mc_cheriframe,
	    &td->td_pcb->pcb_cheriframe,
	    sizeof(struct cheri_frame));

	/*
	 * Allocate and validate space for the signal handler context.  For
	 * CheriABI purposes, 'sp' from this point forward is relocated
	 * relative to any pertinent stack capability.  For an alternative
	 * signal context, we need to recalculate stackbase for later use in
	 * calculating a new $sp for the signal-handling context.
	 *
	 * XXXRW: It seems like it would be nice to both the regular and
	 * alternative stack calculations in the same place.  However, we need
	 * oonstack sooner.  We should clean this up later.
	 */
	if ((td->td_pflags & TDP_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		stackbase = (vm_offset_t)td->td_sigstk.ss_sp;
		sp = (vm_offset_t)(stackbase + td->td_sigstk.ss_size);
	} else {
		/*
		 * Signals delivered when a CHERI sandbox is present must be
		 * delivered on the alternative stack rather than a local one.
		 * If an alternative stack isn't present, then terminate or
		 * risk leaking capabilities (and control) to the sandbox (or
		 * just crashing the sandbox).
		 */
		if (cheri_is_sandboxed) {
			mtx_unlock(&psp->ps_mtx);
			printf("pid %d, tid %d: signal in sandbox without "
			    "alternative stack defined\n", td->td_proc->p_pid,
			    td->td_tid);
			sigexit(td, SIGILL);
			/* NOTREACHED */
		}
		sp = (vm_offset_t)(stackbase + regs->sp);
	}
	sp -= sizeof(struct sigframe_c);
	/* For CHERI, keep the stack pointer capability aligned. */
	sp &= ~(CHERICAP_SIZE - 1);
	sfp = (void *)sp;

	/* Build the argument list for the signal handler. */
	regs->a0 = sig;
	if (SIGISMEMBER(psp->ps_siginfo, sig)) {
		/*
		 * Signal handler installed with SA_SIGINFO.
		 *
		 * XXXRW: We would ideally synthesise these from the
		 * user-originated stack capability, rather than KDC, to be on
		 * the safe side.
		 */
		cheri_capability_set(&capreg->cf_c3, CHERI_CAP_USER_DATA_PERMS,
		    CHERI_CAP_USER_DATA_OTYPE, (void *)(intptr_t)&sfp->sf_si,
		    sizeof(sfp->sf_si), 0);
		cheri_capability_set(&capreg->cf_c4, CHERI_CAP_USER_DATA_PERMS,
		    CHERI_CAP_USER_DATA_OTYPE, (void *)(intptr_t)&sfp->sf_uc,
		    sizeof(sfp->sf_uc), 0);
		/* sf.sf_ahu.sf_action = (__siginfohandler_t *)catcher; */

		/* fill siginfo structure */
		sf.sf_si.si_signo = sig;
		sf.sf_si.si_code = ksi->ksi_code;
		/*
		 * Write out badvaddr, but don't create a valid capability
		 * since that might allow privilege amplification.
		 *
		 * XXX-BD: This probably isn't the right method.
		 * XXX-BD: Do we want to set base or offset?
		 *
		 * XXXRW: I think there's some argument that anything
		 * receiving this signal is fairly privileged.  But we could
		 * generate a $c0-relative (or $pcc-relative) capability, if
		 * possible.  (Using versions if $c0 and $pcc for the
		 * signal-handling context rather than that which caused the
		 * signal).  I'd be tempted to deliver badvaddr as the offset
		 * of that capability.  If badvaddr is not in range, then we
		 * should just deliver an untagged NULL-derived version
		 * (perhaps)?
		 */
		*((uintptr_t *)&sf.sf_si.si_addr) =
		    (uintptr_t)(void *)regs->badvaddr;
	}
	/*
	 * XXX: No support for undocumented arguments to old style handlers.
	 */

	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	/*
	 * Copy the sigframe out to the user's stack.
	 */
	if (copyoutcap(&sf, (void *)sfp, sizeof(sf)) != 0) {
		/*
		 * Something is wrong with the stack pointer.
		 * ...Kill the process.
		 */
		PROC_LOCK(p);
		printf("pid %d, tid %d: could not copy out sigframe\n",
		    td->td_proc->p_pid, td->td_tid);
		sigexit(td, SIGILL);
		/* NOTREACHED */
	}

	/*
	 * Re-acquire process locks necessary to access suitable pcb fields.
	 * However, arguably, these operations should be atomic with the
	 * initial inspection of 'psp'.
	 */
	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);

	/*
	 * Install CHERI signal-delivery register state for handler to run
	 * in.  As we don't install this in the CHERI frame on the user stack,
	 * it will be (generally) be removed automatically on sigreturn().
	 */
	/* XXX-BD: this isn't quite right */
	cheri_sendsig(td);

	/*
	 * Note that $sp must be installed relative to $c11, so re-subtract
	 * the stack base here.
	 */
	regs->pc = (register_t)(intptr_t)catcher;
	regs->sp = (register_t)((intptr_t)sfp - stackbase);

	cheri_capability_copy(&capreg->cf_c12, &psp->ps_sigcap[_SIG_IDX(sig)]);
	cheri_capability_copy(&capreg->cf_c17,
	    &td->td_pcb->pcb_cherisignal.csig_sigcode);
}

static void
cheriabi_exec_setregs(struct thread *td, struct image_params *imgp, u_long stack)
{
	struct cheri_frame *cfp;
	struct cheri_signal *csigp;
	u_long stackbase, stacklen;

	bzero((caddr_t)td->td_frame, sizeof(struct trapframe));

	KASSERT(stack % sizeof(struct chericap) == 0,
	    ("CheriABI stack pointer not properly aligned"));

	/* XXX-BD: use MMAP defines, not DATA */
	cheri_capability_set(&td->td_proc->p_md.md_cheri_mmap_cap,
	    CHERI_CAP_USER_DATA_PERMS | CHERI_CAP_USER_CODE_PERMS, NULL,
	    CHERI_CAP_USER_DATA_BASE, CHERI_CAP_USER_DATA_LENGTH,
	    CHERI_CAP_USER_DATA_OFFSET);

	td->td_frame->pc = imgp->entry_addr;
	td->td_frame->sr = MIPS_SR_KSU_USER | MIPS_SR_EXL | MIPS_SR_INT_IE |
	    (mips_rd_status() & MIPS_SR_INT_MASK) |
	    MIPS_SR_PX | MIPS_SR_UX | MIPS_SR_KX | MIPS_SR_COP_2_BIT;
	cheri_exec_setregs(td, imgp->entry_addr);
	cheri_stack_init(td->td_pcb);

	/*
	 * Pass a pointer to the struct cheriabi_execdata at the top of the
	 * stack.
	 *
	 * XXXBD: should likely be read only
	 */
	cfp = &td->td_pcb->pcb_cheriframe;
	cheri_capability_set(&cfp->cf_c3, CHERI_CAP_USER_DATA_PERMS,
	    CHERI_CAP_USER_DATA_OTYPE, (void *)stack,
	    sizeof(struct cheriabi_execdata), 0);

	/*
	 * Restrict the stack capability to the maximum region allowed for
	 * this process and adjust sp accordingly.
	 *
	 * XXXBD: 8MB should be the process stack limit.
	 */
	CTASSERT(CHERI_CAP_USER_DATA_BASE == 0);
	stackbase = USRSTACK - (1024 * 1024 * 8);
	KASSERT(stack > stackbase,
	    ("top of stack 0x%lx is below stack base 0x%lx", stack, stackbase));
	stacklen = stack - stackbase;
	cheri_capability_set(&cfp->cf_c11, CHERI_CAP_USER_DATA_PERMS,
	    CHERI_CAP_USER_DATA_OTYPE, (void *)stackbase, stacklen, 0);
	td->td_frame->sp = stacklen;
	/*
	 * Also update the signal stack.  The default set in
	 * cheri_exec_setregs() covers the whole address space.
	 */
	csigp = &td->td_pcb->pcb_cherisignal;
	cheri_capability_set(&csigp->csig_c11, CHERI_CAP_USER_DATA_PERMS,
	    CHERI_CAP_USER_DATA_OTYPE, (void *)stackbase, stacklen, 0);

	td->td_md.md_flags &= ~MDTD_FPUSED;
	if (PCPU_GET(fpcurthread) == td)
		PCPU_SET(fpcurthread, (struct thread *)0);
	td->td_md.md_ss_addr = 0;

	td->td_md.md_tls_tcb_offset = TLS_TP_OFFSET + TLS_TCB_SIZE_C;
}

void
cheriabi_get_signal_stack_capability(struct thread *td, struct chericap *csig)
{

	cheri_capability_copy(csig, &td->td_pcb->pcb_cherisignal.csig_c11);
}

/*
 * Set a thread's signal stack capability.  If NULL is passed, restore
 * the default stack capability.
 */
void
cheriabi_set_signal_stack_capability(struct thread *td, struct chericap *csig)
{

	cheri_capability_copy(&td->td_pcb->pcb_cherisignal.csig_c11,
	    csig != NULL ? csig :
	    &td->td_pcb->pcb_cherisignal.csig_default_stack);
}

int
cheriabi_sysarch(struct thread *td, struct cheriabi_sysarch_args *uap)
{
	struct cheri_frame *capreg = &td->td_pcb->pcb_cheriframe;
	int error;
	int parms_from_cap = 1;
	uint64_t perms;
	size_t reqsize;
	register_t reqperms;

	/*
	 * The sysarch() fill_uap function is machine-independent so can not
	 * check the validity of the capabilty which becomes uap->parms.  As
	 * such, it makes no attempt to convert the result.  We need to
	 * perform those checks here.
	 */
	switch (uap->op) {
	case MIPS_SET_TLS:
		/*
		 * XXX-BD: not exactly clear what perms and size this
		 * should have, presumably somewhat flexible, at least in
		 * theory.
		 */
		reqsize = 0;
		reqperms = 0;
		break;

	case MIPS_GET_TLS:
	case CHERI_GET_STACK:
	case CHERI_GET_TYPECAP:
		reqsize = sizeof(struct chericap);
		reqperms = CHERI_PERM_STORE|CHERI_PERM_STORE_CAP;
		break;

	case CHERI_SET_STACK:
		reqsize = sizeof(struct chericap);
		reqperms = CHERI_PERM_LOAD|CHERI_PERM_LOAD_CAP;
		break;

	case CHERI_MMAP_GETPERM:
		reqsize = sizeof(uint64_t);
		reqperms = CHERI_PERM_STORE;
		break;

	case CHERI_MMAP_ANDPERM:
		reqsize = sizeof(uint64_t);
		reqperms = CHERI_PERM_LOAD|CHERI_PERM_STORE;
		break;

	case MIPS_GET_COUNT:
		parms_from_cap = 0;
		break;

	default:
		return (EINVAL);
	}
	if (parms_from_cap) {
		error = cheriabi_cap_to_ptr(&uap->parms, &capreg->cf_c3,
		    reqsize, CHERI_PERM_GLOBAL | reqperms, 0);
		if (error != 0)
			return (error);
	}

	switch (uap->op) {
	case MIPS_SET_TLS:
		cheri_capability_copy(&td->td_md.md_tls_cap, &capreg->cf_c3);
		td->td_md.md_tls = uap->parms;

		/* XXX: should support a crdhwr version */
		if (cpuinfo.userlocal_reg == true) {
			/*
			 * If there is an user local register implementation
			 * (ULRI) update it as well.  Add the TLS and TCB
			 * offsets so the value in this register is
			 * adjusted like in the case of the rdhwr trap()
			 * instruction handler.
			 *
			 * The user local register needs the TLS and TCB
			 * offsets because the compiler simply generates a
			 * 'rdhwr reg, $29' instruction to access thread local
			 * storage (i.e., variables with the '_thread'
			 * attribute).
			 */
			mips_wr_userlocal((unsigned long)(uap->parms +
			    td->td_md.md_tls_tcb_offset));
		}

		return (0);

	case MIPS_GET_TLS:
		error = copyoutcap(&td->td_md.md_tls_cap, uap->parms,
		    sizeof(struct chericap));
		return (error);

	case CHERI_MMAP_GETPERM:
		PROC_LOCK(td->td_proc);
		CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC,
		    &td->td_proc->p_md.md_cheri_mmap_cap, 0);
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		PROC_UNLOCK(td->td_proc);
		if (suword64(uap->parms, perms) != 0)
			return (EFAULT);
		return (0);

	case CHERI_MMAP_ANDPERM:
		perms = fuword64(uap->parms);
		if (perms == -1)
			return (EINVAL);
		PROC_LOCK(td->td_proc);
		CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC,
		    &td->td_proc->p_md.md_cheri_mmap_cap, 0);
		CHERI_CANDPERM(CHERI_CR_CTEMP0, CHERI_CR_CTEMP0,
		    (register_t)perms);
		CHERI_CSC(CHERI_CR_CTEMP0, CHERI_CR_KDC,
		    &td->td_proc->p_md.md_cheri_mmap_cap, 0);
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		PROC_UNLOCK(td->td_proc);
		if (suword64(uap->parms, perms) != 0)
			return (EFAULT);
		return (0);

	default:
		return (sysarch(td, (struct sysarch_args*)uap));
	}
}
