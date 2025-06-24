/*-
 * Copyright (c) 2004 Tim J. Robbins
 * Copyright (c) 2003 Peter Wemm
 * Copyright (c) 2002 Doug Rabson
 * Copyright (c) 1998-1999 Andrew Gallatin
 * Copyright (c) 1994-1996 Søren Schmidt
 * All rights reserved.
 * Copyright (c) 2013, 2021 Dmitry Chagin <dchagin@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define	__ELF_WORD_SIZE	64

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/stddef.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>

#include <vm/pmap.h>
#include <vm/vm.h>
#include <vm/vm_param.h>

#include <machine/md_var.h>
#include <machine/trap.h>

#include <x86/linux/linux_x86.h>
#include <amd64/linux/linux.h>
#include <amd64/linux/linux_proto.h>
#include <compat/linux/linux_elf.h>
#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_fork.h>
#include <compat/linux/linux_ioctl.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_vdso.h>

#include <x86/linux/linux_x86_sigframe.h>

_Static_assert(sizeof(struct l_fpstate) ==
    sizeof(__typeof(((mcontext_t *)0)->mc_fpstate)),
    "fxsave area size incorrect");

MODULE_VERSION(linux64, 1);

#define	LINUX_VDSOPAGE_SIZE	PAGE_SIZE * 2
#define	LINUX_VDSOPAGE_LA48	(VM_MAXUSER_ADDRESS_LA48 - \
				    LINUX_VDSOPAGE_SIZE)
#define	LINUX_SHAREDPAGE_LA48	(LINUX_VDSOPAGE_LA48 - PAGE_SIZE)
				/*
				 * PAGE_SIZE - the size
				 * of the native SHAREDPAGE
				 */
#define	LINUX_USRSTACK_LA48	LINUX_SHAREDPAGE_LA48
#define	LINUX_PS_STRINGS_LA48	(LINUX_USRSTACK_LA48 - \
				    sizeof(struct ps_strings))

static int linux_szsigcode;
static vm_object_t linux_vdso_obj;
static char *linux_vdso_mapping;
extern char _binary_linux_vdso_so_o_start;
extern char _binary_linux_vdso_so_o_end;
static vm_offset_t linux_vdso_base;

extern struct sysent linux_sysent[LINUX_SYS_MAXSYSCALL];
extern const char *linux_syscallnames[];

SET_DECLARE(linux_ioctl_handler_set, struct linux_ioctl_handler);

static void	linux_vdso_install(const void *param);
static void	linux_vdso_deinstall(const void *param);
static void	linux_vdso_reloc(char *mapping, Elf_Addr offset);
static void	linux_set_syscall_retval(struct thread *td, int error);
static int	linux_fetch_syscall_args(struct thread *td);
static void	linux_exec_setregs(struct thread *td, struct image_params *imgp,
		    uintptr_t stack);
static void	linux_exec_sysvec_init(void *param);
static int	linux_on_exec_vmspace(struct proc *p,
		    struct image_params *imgp);
static void	linux_set_fork_retval(struct thread *td);
static int	linux_vsyscall(struct thread *td);

LINUX_VDSO_SYM_INTPTR(linux_rt_sigcode);
LINUX_VDSO_SYM_CHAR(linux_platform);
LINUX_VDSO_SYM_INTPTR(kern_timekeep_base);
LINUX_VDSO_SYM_INTPTR(kern_tsc_selector);
LINUX_VDSO_SYM_INTPTR(kern_cpu_selector);

/*
 * According to the Intel x86 ISA 64-bit syscall
 * saves %rip to %rcx and rflags to %r11. Registers on syscall entry:
 * %rax  system call number
 * %rcx  return address
 * %r11  saved rflags
 * %rdi  arg1
 * %rsi  arg2
 * %rdx  arg3
 * %r10  arg4
 * %r8   arg5
 * %r9   arg6
 *
 * Then FreeBSD fast_syscall() move registers:
 * %rcx -> trapframe.tf_rip
 * %r10 -> trapframe.tf_rcx
 */
static int
linux_fetch_syscall_args(struct thread *td)
{
	struct proc *p;
	struct trapframe *frame;
	struct syscall_args *sa;

	p = td->td_proc;
	frame = td->td_frame;
	sa = &td->td_sa;

	sa->args[0] = frame->tf_rdi;
	sa->args[1] = frame->tf_rsi;
	sa->args[2] = frame->tf_rdx;
	sa->args[3] = frame->tf_rcx;
	sa->args[4] = frame->tf_r8;
	sa->args[5] = frame->tf_r9;
	sa->code = frame->tf_rax;
	sa->original_code = sa->code;

	if (sa->code >= p->p_sysent->sv_size)
		/* nosys */
		sa->callp = &nosys_sysent;
	else
		sa->callp = &p->p_sysent->sv_table[sa->code];

	/* Restore r10 earlier to avoid doing this multiply times. */
	frame->tf_r10 = frame->tf_rcx;
	/* Restore %rcx for machine context. */
	frame->tf_rcx = frame->tf_rip;

	td->td_retval[0] = 0;
	return (0);
}

static void
linux_set_syscall_retval(struct thread *td, int error)
{
	struct trapframe *frame;

	frame = td->td_frame;

	switch (error) {
	case 0:
		frame->tf_rax = td->td_retval[0];
		break;

	case ERESTART:
		/*
		 * Reconstruct pc, we know that 'syscall' is 2 bytes,
		 * lcall $X,y is 7 bytes, int 0x80 is 2 bytes.
		 * We saved this in tf_err.
		 *
		 */
		frame->tf_rip -= frame->tf_err;
		break;

	case EJUSTRETURN:
		break;

	default:
		frame->tf_rax = bsd_to_linux_errno(error);
		break;
	}

	/*
	 * Differently from FreeBSD native ABI, on Linux only %rcx
	 * and %r11 values are not preserved across the syscall.
	 * Require full context restore to get all registers except
	 * those two restored at return to usermode.
	 */
	set_pcb_flags(td->td_pcb, PCB_FULL_IRET);
}

static void
linux_set_fork_retval(struct thread *td)
{
	struct trapframe *frame = td->td_frame;

	frame->tf_rax = 0;
}

void
linux64_arch_copyout_auxargs(struct image_params *imgp, Elf_Auxinfo **pos)
{

	AUXARGS_ENTRY((*pos), LINUX_AT_SYSINFO_EHDR, linux_vdso_base);
	AUXARGS_ENTRY((*pos), LINUX_AT_HWCAP, cpu_feature);
	AUXARGS_ENTRY((*pos), LINUX_AT_HWCAP2, linux_x86_elf_hwcap2());
	AUXARGS_ENTRY((*pos), LINUX_AT_PLATFORM, PTROUT(linux_platform));
}

/*
 * Reset registers to default values on exec.
 */
static void
linux_exec_setregs(struct thread *td, struct image_params *imgp,
    uintptr_t stack)
{
	struct trapframe *regs;
	struct pcb *pcb;
	register_t saved_rflags;

	regs = td->td_frame;
	pcb = td->td_pcb;

	if (td->td_proc->p_md.md_ldt != NULL)
		user_ldt_free(td);

	pcb->pcb_fsbase = 0;
	pcb->pcb_gsbase = 0;
	clear_pcb_flags(pcb, PCB_32BIT | PCB_TLSBASE);
	pcb->pcb_initial_fpucw = __LINUX_NPXCW__;
	set_pcb_flags(pcb, PCB_FULL_IRET);

	saved_rflags = regs->tf_rflags & PSL_T;
	bzero((char *)regs, sizeof(struct trapframe));
	regs->tf_rip = imgp->entry_addr;
	regs->tf_rsp = stack;
	regs->tf_rflags = PSL_USER | saved_rflags;
	regs->tf_ss = _udatasel;
	regs->tf_cs = _ucodesel;
	regs->tf_ds = _udatasel;
	regs->tf_es = _udatasel;
	regs->tf_fs = _ufssel;
	regs->tf_gs = _ugssel;
	regs->tf_flags = TF_HASSEGS;

	x86_clear_dbregs(pcb);

	/*
	 * Drop the FP state if we hold it, so that the process gets a
	 * clean FP state if it uses the FPU again.
	 */
	fpstate_drop(td);
}

static int
linux_fxrstor(struct thread *td, mcontext_t *mcp, struct l_sigcontext *sc)
{
	struct savefpu *fp = (struct savefpu *)&mcp->mc_fpstate[0];
	int error;

	error = copyin(PTRIN(sc->sc_fpstate), fp, sizeof(mcp->mc_fpstate));
	if (error != 0)
		return (error);
	bzero(&fp->sv_pad[0], sizeof(fp->sv_pad));
	return (set_fpcontext(td, mcp, NULL, 0));
}

static int
linux_xrstor(struct thread *td, mcontext_t *mcp, struct l_sigcontext *sc)
{
	struct savefpu *fp = (struct savefpu *)&mcp->mc_fpstate[0];
	char *xfpustate;
	struct proc *p;
	uint32_t magic2;
	int error;

	p = td->td_proc;
	mcp->mc_xfpustate_len = cpu_max_ext_state_size - sizeof(struct savefpu);

	/* Legacy region of an xsave area. */
	error = copyin(PTRIN(sc->sc_fpstate), fp, sizeof(mcp->mc_fpstate));
	if (error != 0)
		return (error);
	bzero(&fp->sv_pad[0], sizeof(fp->sv_pad));

	/* Extended region of an xsave area. */
	sc->sc_fpstate += sizeof(mcp->mc_fpstate);
	xfpustate = (char *)fpu_save_area_alloc();
	error = copyin(PTRIN(sc->sc_fpstate), xfpustate, mcp->mc_xfpustate_len);
	if (error != 0) {
		fpu_save_area_free((struct savefpu *)xfpustate);
		uprintf("pid %d (%s): linux xrstor failed\n", p->p_pid,
		    td->td_name);
		return (error);
	}

	/* Linux specific end of xsave area marker. */
	sc->sc_fpstate += mcp->mc_xfpustate_len;
	error = copyin(PTRIN(sc->sc_fpstate), &magic2, LINUX_FP_XSTATE_MAGIC2_SIZE);
	if (error != 0 || magic2 != LINUX_FP_XSTATE_MAGIC2) {
		fpu_save_area_free((struct savefpu *)xfpustate);
		uprintf("pid %d (%s): sigreturn magic2 0x%x error %d\n",
		    p->p_pid, td->td_name, magic2, error);
		return (error);
	}

	error = set_fpcontext(td, mcp, xfpustate, mcp->mc_xfpustate_len);
	fpu_save_area_free((struct savefpu *)xfpustate);
	if (error != 0) {
		uprintf("pid %d (%s): sigreturn set_fpcontext error %d\n",
		    p->p_pid, td->td_name, error);
	}
	return (error);
}

static int
linux_copyin_fpstate(struct thread *td, struct l_ucontext *uc)
{
	mcontext_t mc;

	bzero(&mc, sizeof(mc));
	mc.mc_ownedfp = _MC_FPOWNED_FPU;
	mc.mc_fpformat = _MC_FPFMT_XMM;

	if ((uc->uc_flags & LINUX_UC_FP_XSTATE) != 0)
		return (linux_xrstor(td, &mc, &uc->uc_mcontext));
	else
		return (linux_fxrstor(td, &mc, &uc->uc_mcontext));
}

/*
 * Copied from amd64/amd64/machdep.c
 */
int
linux_rt_sigreturn(struct thread *td, struct linux_rt_sigreturn_args *args)
{
	struct proc *p;
	struct l_rt_sigframe sf;
	struct l_sigcontext *context;
	struct trapframe *regs;
	unsigned long rflags;
	sigset_t bmask;
	int error;
	ksiginfo_t ksi;

	regs = td->td_frame;
	error = copyin((void *)regs->tf_rbx, &sf, sizeof(sf));
	if (error != 0)
		return (error);

	p = td->td_proc;
	context = &sf.sf_uc.uc_mcontext;
	rflags = context->sc_rflags;

	/*
	 * Don't allow users to change privileged or reserved flags.
	 */
	/*
	 * XXX do allow users to change the privileged flag PSL_RF.
	 * The cpu sets PSL_RF in tf_rflags for faults.  Debuggers
	 * should sometimes set it there too.  tf_rflags is kept in
	 * the signal context during signal handling and there is no
	 * other place to remember it, so the PSL_RF bit may be
	 * corrupted by the signal handler without us knowing.
	 * Corruption of the PSL_RF bit at worst causes one more or
	 * one less debugger trap, so allowing it is fairly harmless.
	 */
	if (!EFL_SECURE(rflags & ~PSL_RF, regs->tf_rflags & ~PSL_RF)) {
		uprintf("pid %d comm %s linux mangled rflags %#lx\n",
		    p->p_pid, p->p_comm, rflags);
		return (EINVAL);
	}

	/*
	 * Don't allow users to load a valid privileged %cs.  Let the
	 * hardware check for invalid selectors, excess privilege in
	 * other selectors, invalid %eip's and invalid %esp's.
	 */
	if (!CS_SECURE(context->sc_cs)) {
		uprintf("pid %d comm %s linux mangled cs %#x\n",
		    p->p_pid, p->p_comm, context->sc_cs);
		ksiginfo_init_trap(&ksi);
		ksi.ksi_signo = SIGBUS;
		ksi.ksi_code = BUS_OBJERR;
		ksi.ksi_trapno = T_PROTFLT;
		ksi.ksi_addr = (void *)regs->tf_rip;
		trapsignal(td, &ksi);
		return (EINVAL);
	}

	linux_to_bsd_sigset(&sf.sf_uc.uc_sigmask, &bmask);
	kern_sigprocmask(td, SIG_SETMASK, &bmask, NULL, 0);

	regs->tf_rdi    = context->sc_rdi;
	regs->tf_rsi    = context->sc_rsi;
	regs->tf_rdx    = context->sc_rdx;
	regs->tf_rbp    = context->sc_rbp;
	regs->tf_rbx    = context->sc_rbx;
	regs->tf_rcx    = context->sc_rcx;
	regs->tf_rax    = context->sc_rax;
	regs->tf_rip    = context->sc_rip;
	regs->tf_rsp    = context->sc_rsp;
	regs->tf_r8     = context->sc_r8;
	regs->tf_r9     = context->sc_r9;
	regs->tf_r10    = context->sc_r10;
	regs->tf_r11    = context->sc_r11;
	regs->tf_r12    = context->sc_r12;
	regs->tf_r13    = context->sc_r13;
	regs->tf_r14    = context->sc_r14;
	regs->tf_r15    = context->sc_r15;
	regs->tf_cs     = context->sc_cs;
	regs->tf_err    = context->sc_err;
	regs->tf_rflags = rflags;

	error = linux_copyin_fpstate(td, &sf.sf_uc);
	if (error != 0) {
		uprintf("pid %d comm %s linux can't restore fpu state %d\n",
		    p->p_pid, p->p_comm, error);
		return (error);
	}

	set_pcb_flags(td->td_pcb, PCB_FULL_IRET);
	return (EJUSTRETURN);
}

static int
linux_fxsave(mcontext_t *mcp, void *ufp)
{
	struct l_fpstate *fx = (struct l_fpstate *)&mcp->mc_fpstate[0];

	bzero(&fx->reserved2[0], sizeof(fx->reserved2));
	return (copyout(fx, ufp, sizeof(*fx)));
}

static int
linux_xsave(mcontext_t *mcp, char *xfpusave, char *ufp)
{
	struct l_fpstate *fx = (struct l_fpstate *)&mcp->mc_fpstate[0];
	uint32_t magic2;
	int error;

	/* Legacy region of an xsave area. */
	fx->sw_reserved.magic1 = LINUX_FP_XSTATE_MAGIC1;
	fx->sw_reserved.xstate_size = mcp->mc_xfpustate_len + sizeof(*fx);
	fx->sw_reserved.extended_size = fx->sw_reserved.xstate_size +
	    LINUX_FP_XSTATE_MAGIC2_SIZE;
	fx->sw_reserved.xfeatures = xsave_mask;

	error = copyout(fx, ufp, sizeof(*fx));
	if (error != 0)
		return (error);
	ufp += sizeof(*fx);

	/* Extended region of an xsave area. */
	error = copyout(xfpusave, ufp, mcp->mc_xfpustate_len);
	if (error != 0)
		return (error);

	/* Linux specific end of xsave area marker. */
	ufp += mcp->mc_xfpustate_len;
	magic2 = LINUX_FP_XSTATE_MAGIC2;
	return (copyout(&magic2, ufp, LINUX_FP_XSTATE_MAGIC2_SIZE));
}

static int
linux_copyout_fpstate(struct thread *td, struct l_ucontext *uc, char **sp)
{
	size_t xfpusave_len;
	char *xfpusave;
	mcontext_t mc;
	char *ufp = *sp;

	get_fpcontext(td, &mc, &xfpusave, &xfpusave_len);
	KASSERT(mc.mc_fpformat != _MC_FPFMT_NODEV, ("fpu not present"));

	/* Room for fxsave area. */
	ufp -= sizeof(struct l_fpstate);
	if (xfpusave != NULL) {
		/* Room for xsave area. */
		ufp -= (xfpusave_len + LINUX_FP_XSTATE_MAGIC2_SIZE);
		uc->uc_flags |= LINUX_UC_FP_XSTATE;
	}
	*sp = ufp = (char *)((unsigned long)ufp & ~0x3Ful);

	if (xfpusave != NULL)
		return (linux_xsave(&mc, xfpusave, ufp));
	else
		return (linux_fxsave(&mc, ufp));
}

/*
 * copied from amd64/amd64/machdep.c
 *
 * Send an interrupt to process.
 */
static void
linux_rt_sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct l_rt_sigframe sf, *sfp;
	struct proc *p;
	struct thread *td;
	struct sigacts *psp;
	char *sp;
	struct trapframe *regs;
	int sig, code;
	int oonstack, issiginfo;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	sig = linux_translate_traps(ksi->ksi_signo, ksi->ksi_trapno);
	psp = p->p_sigacts;
	issiginfo = SIGISMEMBER(psp->ps_siginfo, sig);
	code = ksi->ksi_code;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	regs = td->td_frame;
	oonstack = sigonstack(regs->tf_rsp);

	LINUX_CTR4(rt_sendsig, "%p, %d, %p, %u",
	    catcher, sig, mask, code);

	bzero(&sf, sizeof(sf));
	sf.sf_uc.uc_stack.ss_sp = PTROUT(td->td_sigstk.ss_sp);
	sf.sf_uc.uc_stack.ss_size = td->td_sigstk.ss_size;
	sf.sf_uc.uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK)
	    ? ((oonstack) ? LINUX_SS_ONSTACK : 0) : LINUX_SS_DISABLE;

	/* Allocate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sp = (char *)td->td_sigstk.ss_sp + td->td_sigstk.ss_size;
	} else
		sp = (char *)regs->tf_rsp - 128;

	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	if (linux_copyout_fpstate(td, &sf.sf_uc, &sp) != 0) {
		uprintf("pid %d comm %s linux can't save fpu state, killing\n",
		    p->p_pid, p->p_comm);
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}
	sf.sf_uc.uc_mcontext.sc_fpstate = (register_t)sp;

	/* Make room, keeping the stack aligned. */
	sp -= sizeof(struct l_rt_sigframe);
	sfp = (struct l_rt_sigframe *)((unsigned long)sp & ~0xFul);

	/* Save user context. */
	bsd_to_linux_sigset(mask, &sf.sf_uc.uc_sigmask);
	sf.sf_uc.uc_mcontext.sc_mask   = sf.sf_uc.uc_sigmask;
	sf.sf_uc.uc_mcontext.sc_rdi    = regs->tf_rdi;
	sf.sf_uc.uc_mcontext.sc_rsi    = regs->tf_rsi;
	sf.sf_uc.uc_mcontext.sc_rdx    = regs->tf_rdx;
	sf.sf_uc.uc_mcontext.sc_rbp    = regs->tf_rbp;
	sf.sf_uc.uc_mcontext.sc_rbx    = regs->tf_rbx;
	sf.sf_uc.uc_mcontext.sc_rcx    = regs->tf_rcx;
	sf.sf_uc.uc_mcontext.sc_rax    = regs->tf_rax;
	sf.sf_uc.uc_mcontext.sc_rip    = regs->tf_rip;
	sf.sf_uc.uc_mcontext.sc_rsp    = regs->tf_rsp;
	sf.sf_uc.uc_mcontext.sc_r8     = regs->tf_r8;
	sf.sf_uc.uc_mcontext.sc_r9     = regs->tf_r9;
	sf.sf_uc.uc_mcontext.sc_r10    = regs->tf_r10;
	sf.sf_uc.uc_mcontext.sc_r11    = regs->tf_r11;
	sf.sf_uc.uc_mcontext.sc_r12    = regs->tf_r12;
	sf.sf_uc.uc_mcontext.sc_r13    = regs->tf_r13;
	sf.sf_uc.uc_mcontext.sc_r14    = regs->tf_r14;
	sf.sf_uc.uc_mcontext.sc_r15    = regs->tf_r15;
	sf.sf_uc.uc_mcontext.sc_cs     = regs->tf_cs;
	sf.sf_uc.uc_mcontext.sc_rflags = regs->tf_rflags;
	sf.sf_uc.uc_mcontext.sc_err    = regs->tf_err;
	sf.sf_uc.uc_mcontext.sc_trapno = bsd_to_linux_trapcode(code);
	sf.sf_uc.uc_mcontext.sc_cr2    = (register_t)ksi->ksi_addr;

	/* Translate the signal. */
	sig = bsd_to_linux_signal(sig);
	/* Fill in POSIX parts. */
	siginfo_to_lsiginfo(&ksi->ksi_info, &sf.sf_si, sig);

	/* Copy the sigframe out to the user's stack. */
	if (copyout(&sf, sfp, sizeof(*sfp)) != 0) {
		uprintf("pid %d comm %s has trashed its stack, killing\n",
		    p->p_pid, p->p_comm);
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	fpstate_drop(td);
	/* Build the argument list for the signal handler. */
	regs->tf_rdi = sig;			/* arg 1 in %rdi */
	regs->tf_rax = 0;
	if (issiginfo) {
		regs->tf_rsi = (register_t)&sfp->sf_si;	/* arg 2 in %rsi */
		regs->tf_rdx = (register_t)&sfp->sf_uc;	/* arg 3 in %rdx */
	} else {
		regs->tf_rsi = 0;
		regs->tf_rdx = 0;
	}
	regs->tf_rcx = (register_t)catcher;
	regs->tf_rsp = (long)sfp;
	regs->tf_rip = linux_rt_sigcode;
	regs->tf_rflags &= ~(PSL_T | PSL_D);
	regs->tf_cs = _ucodesel;
	set_pcb_flags(td->td_pcb, PCB_FULL_IRET);
	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}

#define	LINUX_VSYSCALL_START		(-10UL << 20)
#define	LINUX_VSYSCALL_SZ		1024

const unsigned long linux_vsyscall_vector[] = {
	LINUX_SYS_gettimeofday,
	LINUX_SYS_linux_time,
	LINUX_SYS_linux_getcpu,
};

static int
linux_vsyscall(struct thread *td)
{
	struct trapframe *frame;
	uint64_t retqaddr;
	int code, traced;
	int error;

	frame = td->td_frame;

	/* Check %rip for vsyscall area. */
	if (__predict_true(frame->tf_rip < LINUX_VSYSCALL_START))
		return (EINVAL);
	if ((frame->tf_rip & (LINUX_VSYSCALL_SZ - 1)) != 0)
		return (EINVAL);
	code = (frame->tf_rip - LINUX_VSYSCALL_START) / LINUX_VSYSCALL_SZ;
	if (code >= nitems(linux_vsyscall_vector))
		return (EINVAL);

	/*
	 * vsyscall called as callq *(%rax), so we must
	 * use return address from %rsp and also fixup %rsp.
	 */
	error = copyin((void *)frame->tf_rsp, &retqaddr, sizeof(retqaddr));
	if (error)
		return (error);

	frame->tf_rip = retqaddr;
	frame->tf_rax = linux_vsyscall_vector[code];
	frame->tf_rsp += 8;

	traced = (frame->tf_flags & PSL_T);

	amd64_syscall(td, traced);

	return (0);
}

struct sysentvec elf_linux_sysvec = {
	.sv_size	= LINUX_SYS_MAXSYSCALL,
	.sv_table	= linux_sysent,
	.sv_fixup	= __elfN(freebsd_fixup),
	.sv_sendsig	= linux_rt_sendsig,
	.sv_sigcode	= &_binary_linux_vdso_so_o_start,
	.sv_szsigcode	= &linux_szsigcode,
	.sv_name	= "Linux ELF64",
	.sv_coredump	= elf64_coredump,
	.sv_elf_core_osabi = ELFOSABI_NONE,
	.sv_elf_core_abi_vendor = LINUX_ABI_VENDOR,
	.sv_elf_core_prepare_notes = linux64_prepare_notes,
	.sv_minsigstksz	= LINUX_MINSIGSTKSZ,
	.sv_minuser	= VM_MIN_ADDRESS,
	.sv_maxuser	= VM_MAXUSER_ADDRESS_LA48,
	.sv_usrstack	= LINUX_USRSTACK_LA48,
	.sv_psstrings	= LINUX_PS_STRINGS_LA48,
	.sv_psstringssz	= sizeof(struct ps_strings),
	.sv_stackprot	= VM_PROT_ALL,
	.sv_copyout_auxargs = __linuxN(copyout_auxargs),
	.sv_copyout_strings = __linuxN(copyout_strings),
	.sv_setregs	= linux_exec_setregs,
	.sv_fixlimit	= NULL,
	.sv_maxssiz	= NULL,
	.sv_flags	= SV_ABI_LINUX | SV_LP64 | SV_SHP | SV_SIG_DISCIGN |
	    SV_SIG_WAITNDQ | SV_TIMEKEEP,
	.sv_set_syscall_retval = linux_set_syscall_retval,
	.sv_fetch_syscall_args = linux_fetch_syscall_args,
	.sv_syscallnames = linux_syscallnames,
	.sv_shared_page_base = LINUX_SHAREDPAGE_LA48,
	.sv_shared_page_len = PAGE_SIZE,
	.sv_schedtail	= linux_schedtail,
	.sv_thread_detach = linux_thread_detach,
	.sv_trap	= linux_vsyscall,
	.sv_hwcap	= NULL,
	.sv_hwcap2	= NULL,
	.sv_hwcap3	= NULL,
	.sv_hwcap4	= NULL,
	.sv_onexec	= linux_on_exec_vmspace,
	.sv_onexit	= linux_on_exit,
	.sv_ontdexit	= linux_thread_dtor,
	.sv_setid_allowed = &linux_setid_allowed_query,
	.sv_set_fork_retval = linux_set_fork_retval,
};

static int
linux_on_exec_vmspace(struct proc *p, struct image_params *imgp)
{
	int error;

	error = linux_map_vdso(p, linux_vdso_obj, linux_vdso_base,
	    LINUX_VDSOPAGE_SIZE, imgp);
	if (error == 0)
		error = linux_on_exec(p, imgp);
	return (error);
}

/*
 * linux_vdso_install() and linux_exec_sysvec_init() must be called
 * after exec_sysvec_init() which is SI_SUB_EXEC (SI_ORDER_ANY).
 */
static void
linux_exec_sysvec_init(void *param)
{
	l_uintptr_t *ktimekeep_base, *ktsc_selector;
	struct sysentvec *sv;
	ptrdiff_t tkoff;

	sv = param;
	amd64_lower_shared_page(sv);
	/* Fill timekeep_base */
	exec_sysvec_init(sv);

	tkoff = kern_timekeep_base - linux_vdso_base;
	ktimekeep_base = (l_uintptr_t *)(linux_vdso_mapping + tkoff);
	*ktimekeep_base = sv->sv_shared_page_base + sv->sv_timekeep_offset;

	tkoff = kern_tsc_selector - linux_vdso_base;
	ktsc_selector = (l_uintptr_t *)(linux_vdso_mapping + tkoff);
	*ktsc_selector = linux_vdso_tsc_selector_idx();
	if (bootverbose)
		printf("Linux x86-64 vDSO tsc_selector: %lu\n", *ktsc_selector);

	tkoff = kern_cpu_selector - linux_vdso_base;
	ktsc_selector = (l_uintptr_t *)(linux_vdso_mapping + tkoff);
	*ktsc_selector = linux_vdso_cpu_selector_idx();
	if (bootverbose)
		printf("Linux x86-64 vDSO cpu_selector: %lu\n", *ktsc_selector);
}
SYSINIT(elf_linux_exec_sysvec_init, SI_SUB_EXEC + 1, SI_ORDER_ANY,
    linux_exec_sysvec_init, &elf_linux_sysvec);

static void
linux_vdso_install(const void *param)
{
	char *vdso_start = &_binary_linux_vdso_so_o_start;
	char *vdso_end = &_binary_linux_vdso_so_o_end;

	linux_szsigcode = vdso_end - vdso_start;
	MPASS(linux_szsigcode <= LINUX_VDSOPAGE_SIZE);

	linux_vdso_base = LINUX_VDSOPAGE_LA48;
	if (hw_lower_amd64_sharedpage != 0)
		linux_vdso_base -= PAGE_SIZE;

	__elfN(linux_vdso_fixup)(vdso_start, linux_vdso_base);

	linux_vdso_obj = __elfN(linux_shared_page_init)
	    (&linux_vdso_mapping, LINUX_VDSOPAGE_SIZE);
	bcopy(vdso_start, linux_vdso_mapping, linux_szsigcode);

	linux_vdso_reloc(linux_vdso_mapping, linux_vdso_base);
}
SYSINIT(elf_linux_vdso_init, SI_SUB_EXEC + 1, SI_ORDER_FIRST,
    linux_vdso_install, NULL);

static void
linux_vdso_deinstall(const void *param)
{

	__elfN(linux_shared_page_fini)(linux_vdso_obj,
	    linux_vdso_mapping, LINUX_VDSOPAGE_SIZE);
}
SYSUNINIT(elf_linux_vdso_uninit, SI_SUB_EXEC, SI_ORDER_FIRST,
    linux_vdso_deinstall, NULL);

static void
linux_vdso_reloc(char *mapping, Elf_Addr offset)
{
	const Elf_Ehdr *ehdr;
	const Elf_Shdr *shdr;
	Elf64_Addr *where, val;
	Elf_Size rtype, symidx;
	const Elf_Rela *rela;
	Elf_Addr addr, addend;
	int relacnt;
	int i, j;

	MPASS(offset != 0);

	relacnt = 0;
	ehdr = (const Elf_Ehdr *)mapping;
	shdr = (const Elf_Shdr *)(mapping + ehdr->e_shoff);
	for (i = 0; i < ehdr->e_shnum; i++)
	{
		switch (shdr[i].sh_type) {
		case SHT_REL:
			printf("Linux x86_64 vDSO: unexpected Rel section\n");
			break;
		case SHT_RELA:
			rela = (const Elf_Rela *)(mapping + shdr[i].sh_offset);
			relacnt = shdr[i].sh_size / sizeof(*rela);
		}
	}

	for (j = 0; j < relacnt; j++, rela++) {
		where = (Elf_Addr *)(mapping + rela->r_offset);
		addend = rela->r_addend;
		rtype = ELF_R_TYPE(rela->r_info);
		symidx = ELF_R_SYM(rela->r_info);

		switch (rtype) {
		case R_X86_64_NONE:	/* none */
			break;

		case R_X86_64_RELATIVE:	/* B + A */
			addr = (Elf_Addr)(offset + addend);
			val = addr;
			if (*where != val)
				*where = val;
			break;
		case R_X86_64_IRELATIVE:
			printf("Linux x86_64 vDSO: unexpected ifunc relocation, "
			    "symbol index %ld\n", symidx);
			break;
		default:
			printf("Linux x86_64 vDSO: unexpected relocation type %ld, "
			    "symbol index %ld\n", rtype, symidx);
		}
	}
}

static Elf_Brandnote linux64_brandnote = {
	.hdr.n_namesz	= sizeof(GNU_ABI_VENDOR),
	.hdr.n_descsz	= 16,
	.hdr.n_type	= 1,
	.vendor		= GNU_ABI_VENDOR,
	.flags		= BN_TRANSLATE_OSREL,
	.trans_osrel	= linux_trans_osrel
};

static Elf64_Brandinfo linux_glibc2brand = {
	.brand		= ELFOSABI_LINUX,
	.machine	= EM_X86_64,
	.compat_3_brand	= "Linux",
	.interp_path	= "/lib64/ld-linux-x86-64.so.2",
	.sysvec		= &elf_linux_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &linux64_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE
};

static Elf64_Brandinfo linux_glibc2brandshort = {
	.brand		= ELFOSABI_LINUX,
	.machine	= EM_X86_64,
	.compat_3_brand	= "Linux",
	.interp_path	= "/lib64/ld-linux.so.2",
	.sysvec		= &elf_linux_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &linux64_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE
};

static Elf64_Brandinfo linux_muslbrand = {
	.brand		= ELFOSABI_LINUX,
	.machine	= EM_X86_64,
	.compat_3_brand	= "Linux",
	.interp_path	= "/lib/ld-musl-x86_64.so.1",
	.sysvec		= &elf_linux_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &linux64_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE |
			    LINUX_BI_FUTEX_REQUEUE
};

static Elf64_Brandinfo *linux_brandlist[] = {
	&linux_glibc2brand,
	&linux_glibc2brandshort,
	&linux_muslbrand,
	NULL
};

static int
linux64_elf_modevent(module_t mod, int type, void *data)
{
	Elf64_Brandinfo **brandinfo;
	int error;
	struct linux_ioctl_handler **lihp;

	error = 0;

	switch(type) {
	case MOD_LOAD:
		for (brandinfo = &linux_brandlist[0]; *brandinfo != NULL;
		     ++brandinfo)
			if (elf64_insert_brand_entry(*brandinfo) < 0)
				error = EINVAL;
		if (error == 0) {
			SET_FOREACH(lihp, linux_ioctl_handler_set)
				linux_ioctl_register_handler(*lihp);
			stclohz = (stathz ? stathz : hz);
			if (bootverbose)
				printf("Linux x86-64 ELF exec handler installed\n");
		} else
			printf("cannot insert Linux x86-64 ELF brand handler\n");
		break;
	case MOD_UNLOAD:
		for (brandinfo = &linux_brandlist[0]; *brandinfo != NULL;
		     ++brandinfo)
			if (elf64_brand_inuse(*brandinfo))
				error = EBUSY;
		if (error == 0) {
			for (brandinfo = &linux_brandlist[0];
			     *brandinfo != NULL; ++brandinfo)
				if (elf64_remove_brand_entry(*brandinfo) < 0)
					error = EINVAL;
		}
		if (error == 0) {
			SET_FOREACH(lihp, linux_ioctl_handler_set)
				linux_ioctl_unregister_handler(*lihp);
			if (bootverbose)
				printf("Linux x86_64 ELF exec handler removed\n");
		} else
			printf("Could not deinstall Linux x86_64 ELF interpreter entry\n");
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (error);
}

static moduledata_t linux64_elf_mod = {
	"linux64elf",
	linux64_elf_modevent,
	0
};

DECLARE_MODULE_TIED(linux64elf, linux64_elf_mod, SI_SUB_EXEC, SI_ORDER_ANY);
MODULE_DEPEND(linux64elf, linux_common, 1, 1, 1);
FEATURE(linux64, "Linux 64bit support");
