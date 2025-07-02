/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1994-1996 Søren Schmidt
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

#define __ELF_WORD_SIZE	32

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/fcntl.h>
#include <sys/imgact.h>
#include <sys/imgact_aout.h>
#include <sys/imgact_elf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/stddef.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>

#include <vm/pmap.h>
#include <vm/vm.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>

#include <machine/cpu.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/trap.h>

#include <x86/linux/linux_x86.h>
#include <i386/linux/linux.h>
#include <i386/linux/linux_proto.h>
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

MODULE_VERSION(linux, 1);

#define	LINUX_VDSOPAGE_SIZE	PAGE_SIZE * 2
#define	LINUX_VDSOPAGE		(VM_MAXUSER_ADDRESS - LINUX_VDSOPAGE_SIZE)
#define	LINUX_SHAREDPAGE	(LINUX_VDSOPAGE - PAGE_SIZE)
				/*
				 * PAGE_SIZE - the size
				 * of the native SHAREDPAGE
				 */
#define	LINUX_USRSTACK		LINUX_SHAREDPAGE
#define	LINUX_PS_STRINGS	(LINUX_USRSTACK - sizeof(struct ps_strings))

static int linux_szsigcode;
static vm_object_t linux_vdso_obj;
static char *linux_vdso_mapping;
extern char _binary_linux_vdso_so_o_start;
extern char _binary_linux_vdso_so_o_end;
static vm_offset_t linux_vdso_base;

extern struct sysent linux_sysent[LINUX_SYS_MAXSYSCALL];
extern const char *linux_syscallnames[];

SET_DECLARE(linux_ioctl_handler_set, struct linux_ioctl_handler);

static int	linux_fixup(uintptr_t *stack_base,
		    struct image_params *iparams);
static void     linux_sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask);
static void	linux_exec_setregs(struct thread *td,
		    struct image_params *imgp, uintptr_t stack);
static void	linux_exec_sysvec_init(void *param);
static int	linux_on_exec_vmspace(struct proc *p,
		    struct image_params *imgp);
static void	linux_set_fork_retval(struct thread *td);
static void	linux_vdso_install(const void *param);
static void	linux_vdso_deinstall(const void *param);
static void	linux_vdso_reloc(char *mapping, Elf_Addr offset);

LINUX_VDSO_SYM_CHAR(linux_platform);
LINUX_VDSO_SYM_INTPTR(__kernel_vsyscall);
LINUX_VDSO_SYM_INTPTR(linux_vdso_sigcode);
LINUX_VDSO_SYM_INTPTR(linux_vdso_rt_sigcode);
LINUX_VDSO_SYM_INTPTR(kern_timekeep_base);
LINUX_VDSO_SYM_INTPTR(kern_tsc_selector);
LINUX_VDSO_SYM_INTPTR(kern_cpu_selector);

static int
linux_fixup(uintptr_t *stack_base, struct image_params *imgp)
{
	register_t *base, *argv, *envp;

	base = (register_t *)*stack_base;
	argv = base;
	envp = base + (imgp->args->argc + 1);
	base--;
	if (suword(base, (intptr_t)envp) != 0)
		return (EFAULT);
	base--;
	if (suword(base, (intptr_t)argv) != 0)
		return (EFAULT);
	base--;
	if (suword(base, imgp->args->argc) != 0)
		return (EFAULT);
	*stack_base = (uintptr_t)base;
	return (0);
}

void
linux32_arch_copyout_auxargs(struct image_params *imgp, Elf_Auxinfo **pos)
{

	AUXARGS_ENTRY((*pos), LINUX_AT_SYSINFO_EHDR, linux_vdso_base);
	AUXARGS_ENTRY((*pos), LINUX_AT_SYSINFO, __kernel_vsyscall);
	AUXARGS_ENTRY((*pos), LINUX_AT_HWCAP, cpu_feature);
	AUXARGS_ENTRY((*pos), LINUX_AT_HWCAP2, linux_x86_elf_hwcap2());
	AUXARGS_ENTRY((*pos), LINUX_AT_PLATFORM, PTROUT(linux_platform));
}

static void
linux_rt_sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct sigacts *psp;
	struct trapframe *regs;
	struct l_rt_sigframe *fp, frame;
	int sig, code;
	int oonstack;

	sig = linux_translate_traps(ksi->ksi_signo, ksi->ksi_trapno);
	code = ksi->ksi_code;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	regs = td->td_frame;
	oonstack = sigonstack(regs->tf_esp);

	/* Allocate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		fp = (struct l_rt_sigframe *)((uintptr_t)td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size - sizeof(struct l_rt_sigframe));
	} else
		fp = (struct l_rt_sigframe *)regs->tf_esp - 1;
	mtx_unlock(&psp->ps_mtx);

	/* Build the argument list for the signal handler. */
	sig = bsd_to_linux_signal(sig);

	bzero(&frame, sizeof(frame));

	frame.sf_sig = sig;
	frame.sf_siginfo = PTROUT(&fp->sf_si);
	frame.sf_ucontext = PTROUT(&fp->sf_uc);

	/* Fill in POSIX parts. */
	siginfo_to_lsiginfo(&ksi->ksi_info, &frame.sf_si, sig);

	/* Build the signal context to be used by sigreturn. */
	frame.sf_uc.uc_stack.ss_sp = PTROUT(td->td_sigstk.ss_sp);
	frame.sf_uc.uc_stack.ss_size = td->td_sigstk.ss_size;
	frame.sf_uc.uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK)
	    ? ((oonstack) ? LINUX_SS_ONSTACK : 0) : LINUX_SS_DISABLE;
	PROC_UNLOCK(p);

	bsd_to_linux_sigset(mask, &frame.sf_uc.uc_sigmask);

	frame.sf_uc.uc_mcontext.sc_mask   = frame.sf_uc.uc_sigmask.__mask;
	frame.sf_uc.uc_mcontext.sc_gs     = rgs();
	frame.sf_uc.uc_mcontext.sc_fs     = regs->tf_fs;
	frame.sf_uc.uc_mcontext.sc_es     = regs->tf_es;
	frame.sf_uc.uc_mcontext.sc_ds     = regs->tf_ds;
	frame.sf_uc.uc_mcontext.sc_edi    = regs->tf_edi;
	frame.sf_uc.uc_mcontext.sc_esi    = regs->tf_esi;
	frame.sf_uc.uc_mcontext.sc_ebp    = regs->tf_ebp;
	frame.sf_uc.uc_mcontext.sc_ebx    = regs->tf_ebx;
	frame.sf_uc.uc_mcontext.sc_esp    = regs->tf_esp;
	frame.sf_uc.uc_mcontext.sc_edx    = regs->tf_edx;
	frame.sf_uc.uc_mcontext.sc_ecx    = regs->tf_ecx;
	frame.sf_uc.uc_mcontext.sc_eax    = regs->tf_eax;
	frame.sf_uc.uc_mcontext.sc_eip    = regs->tf_eip;
	frame.sf_uc.uc_mcontext.sc_cs     = regs->tf_cs;
	frame.sf_uc.uc_mcontext.sc_eflags = regs->tf_eflags;
	frame.sf_uc.uc_mcontext.sc_esp_at_signal = regs->tf_esp;
	frame.sf_uc.uc_mcontext.sc_ss     = regs->tf_ss;
	frame.sf_uc.uc_mcontext.sc_err    = regs->tf_err;
	frame.sf_uc.uc_mcontext.sc_cr2    = (register_t)ksi->ksi_addr;
	frame.sf_uc.uc_mcontext.sc_trapno = bsd_to_linux_trapcode(code);

	if (copyout(&frame, fp, sizeof(frame)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	/* Build context to run handler in. */
	regs->tf_esp = PTROUT(fp);
	regs->tf_eip = linux_vdso_rt_sigcode;
	regs->tf_edi = PTROUT(catcher);
	regs->tf_eflags &= ~(PSL_T | PSL_VM | PSL_D);
	regs->tf_cs = _ucodesel;
	regs->tf_ds = _udatasel;
	regs->tf_es = _udatasel;
	regs->tf_fs = _udatasel;
	regs->tf_ss = _udatasel;
	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}

/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored
 * in u. to call routine, followed by kcall
 * to sigreturn routine below.  After sigreturn
 * resets the signal mask, the stack, and the
 * frame pointer, it returns to the user
 * specified pc, psl.
 */
static void
linux_sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct sigacts *psp;
	struct trapframe *regs;
	struct l_sigframe *fp, frame;
	l_sigset_t lmask;
	int sig;
	int oonstack;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	psp = p->p_sigacts;
	sig = linux_translate_traps(ksi->ksi_signo, ksi->ksi_trapno);
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	if (SIGISMEMBER(psp->ps_siginfo, sig)) {
		/* Signal handler installed with SA_SIGINFO. */
		linux_rt_sendsig(catcher, ksi, mask);
		return;
	}
	regs = td->td_frame;
	oonstack = sigonstack(regs->tf_esp);

	/* Allocate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		fp = (struct l_sigframe *)((uintptr_t)td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size - sizeof(struct l_sigframe));
	} else
		fp = (struct l_sigframe *)regs->tf_esp - 1;
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	/* Build the argument list for the signal handler. */
	sig = bsd_to_linux_signal(sig);

	bzero(&frame, sizeof(frame));

	frame.sf_sig = sig;
	frame.sf_sigmask = *mask;
	bsd_to_linux_sigset(mask, &lmask);

	/* Build the signal context to be used by sigreturn. */
	frame.sf_sc.sc_mask   = lmask.__mask;
	frame.sf_sc.sc_gs     = rgs();
	frame.sf_sc.sc_fs     = regs->tf_fs;
	frame.sf_sc.sc_es     = regs->tf_es;
	frame.sf_sc.sc_ds     = regs->tf_ds;
	frame.sf_sc.sc_edi    = regs->tf_edi;
	frame.sf_sc.sc_esi    = regs->tf_esi;
	frame.sf_sc.sc_ebp    = regs->tf_ebp;
	frame.sf_sc.sc_ebx    = regs->tf_ebx;
	frame.sf_sc.sc_esp    = regs->tf_esp;
	frame.sf_sc.sc_edx    = regs->tf_edx;
	frame.sf_sc.sc_ecx    = regs->tf_ecx;
	frame.sf_sc.sc_eax    = regs->tf_eax;
	frame.sf_sc.sc_eip    = regs->tf_eip;
	frame.sf_sc.sc_cs     = regs->tf_cs;
	frame.sf_sc.sc_eflags = regs->tf_eflags;
	frame.sf_sc.sc_esp_at_signal = regs->tf_esp;
	frame.sf_sc.sc_ss     = regs->tf_ss;
	frame.sf_sc.sc_err    = regs->tf_err;
	frame.sf_sc.sc_cr2    = (register_t)ksi->ksi_addr;
	frame.sf_sc.sc_trapno = bsd_to_linux_trapcode(ksi->ksi_trapno);

	if (copyout(&frame, fp, sizeof(frame)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	/* Build context to run handler in. */
	regs->tf_esp = PTROUT(fp);
	regs->tf_eip = linux_vdso_sigcode;
	regs->tf_edi = PTROUT(catcher);
	regs->tf_eflags &= ~(PSL_T | PSL_VM | PSL_D);
	regs->tf_cs = _ucodesel;
	regs->tf_ds = _udatasel;
	regs->tf_es = _udatasel;
	regs->tf_fs = _udatasel;
	regs->tf_ss = _udatasel;
	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * psl to gain improper privileges or to cause
 * a machine fault.
 */
int
linux_sigreturn(struct thread *td, struct linux_sigreturn_args *args)
{
	struct l_sigframe frame;
	struct trapframe *regs;
	int eflags;
	ksiginfo_t ksi;

	regs = td->td_frame;

	/*
	 * The trampoline code hands us the sigframe.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	if (copyin(args->sfp, &frame, sizeof(frame)) != 0)
		return (EFAULT);

	/* Check for security violations. */
#define	EFLAGS_SECURE(ef, oef)	((((ef) ^ (oef)) & ~PSL_USERCHANGE) == 0)
	eflags = frame.sf_sc.sc_eflags;
	if (!EFLAGS_SECURE(eflags, regs->tf_eflags))
		return (EINVAL);

	/*
	 * Don't allow users to load a valid privileged %cs.  Let the
	 * hardware check for invalid selectors, excess privilege in
	 * other selectors, invalid %eip's and invalid %esp's.
	 */
#define	CS_SECURE(cs)	(ISPL(cs) == SEL_UPL)
	if (!CS_SECURE(frame.sf_sc.sc_cs)) {
		ksiginfo_init_trap(&ksi);
		ksi.ksi_signo = SIGBUS;
		ksi.ksi_code = BUS_OBJERR;
		ksi.ksi_trapno = T_PROTFLT;
		ksi.ksi_addr = (void *)regs->tf_eip;
		trapsignal(td, &ksi);
		return (EINVAL);
	}

	kern_sigprocmask(td, SIG_SETMASK, &frame.sf_sigmask, NULL, 0);

	/* Restore signal context. */
	/* %gs was restored by the trampoline. */
	regs->tf_fs     = frame.sf_sc.sc_fs;
	regs->tf_es     = frame.sf_sc.sc_es;
	regs->tf_ds     = frame.sf_sc.sc_ds;
	regs->tf_edi    = frame.sf_sc.sc_edi;
	regs->tf_esi    = frame.sf_sc.sc_esi;
	regs->tf_ebp    = frame.sf_sc.sc_ebp;
	regs->tf_ebx    = frame.sf_sc.sc_ebx;
	regs->tf_edx    = frame.sf_sc.sc_edx;
	regs->tf_ecx    = frame.sf_sc.sc_ecx;
	regs->tf_eax    = frame.sf_sc.sc_eax;
	regs->tf_eip    = frame.sf_sc.sc_eip;
	regs->tf_cs     = frame.sf_sc.sc_cs;
	regs->tf_eflags = eflags;
	regs->tf_esp    = frame.sf_sc.sc_esp_at_signal;
	regs->tf_ss     = frame.sf_sc.sc_ss;

	return (EJUSTRETURN);
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by rt_sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * psl to gain improper privileges or to cause
 * a machine fault.
 */
int
linux_rt_sigreturn(struct thread *td, struct linux_rt_sigreturn_args *args)
{
	struct l_ucontext uc;
	struct l_sigcontext *context;
	sigset_t bmask;
	l_stack_t *lss;
	stack_t ss;
	struct trapframe *regs;
	int eflags;
	ksiginfo_t ksi;

	regs = td->td_frame;

	/*
	 * The trampoline code hands us the ucontext.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	if (copyin(args->ucp, &uc, sizeof(uc)) != 0)
		return (EFAULT);

	context = &uc.uc_mcontext;

	/* Check for security violations. */
#define	EFLAGS_SECURE(ef, oef)	((((ef) ^ (oef)) & ~PSL_USERCHANGE) == 0)
	eflags = context->sc_eflags;
	if (!EFLAGS_SECURE(eflags, regs->tf_eflags))
		return (EINVAL);

	/*
	 * Don't allow users to load a valid privileged %cs.  Let the
	 * hardware check for invalid selectors, excess privilege in
	 * other selectors, invalid %eip's and invalid %esp's.
	 */
#define	CS_SECURE(cs)	(ISPL(cs) == SEL_UPL)
	if (!CS_SECURE(context->sc_cs)) {
		ksiginfo_init_trap(&ksi);
		ksi.ksi_signo = SIGBUS;
		ksi.ksi_code = BUS_OBJERR;
		ksi.ksi_trapno = T_PROTFLT;
		ksi.ksi_addr = (void *)regs->tf_eip;
		trapsignal(td, &ksi);
		return (EINVAL);
	}

	linux_to_bsd_sigset(&uc.uc_sigmask, &bmask);
	kern_sigprocmask(td, SIG_SETMASK, &bmask, NULL, 0);

	/* Restore signal context. */
	/* %gs was restored by the trampoline. */
	regs->tf_fs     = context->sc_fs;
	regs->tf_es     = context->sc_es;
	regs->tf_ds     = context->sc_ds;
	regs->tf_edi    = context->sc_edi;
	regs->tf_esi    = context->sc_esi;
	regs->tf_ebp    = context->sc_ebp;
	regs->tf_ebx    = context->sc_ebx;
	regs->tf_edx    = context->sc_edx;
	regs->tf_ecx    = context->sc_ecx;
	regs->tf_eax    = context->sc_eax;
	regs->tf_eip    = context->sc_eip;
	regs->tf_cs     = context->sc_cs;
	regs->tf_eflags = eflags;
	regs->tf_esp    = context->sc_esp_at_signal;
	regs->tf_ss     = context->sc_ss;

	/* Call sigaltstack & ignore results. */
	lss = &uc.uc_stack;
	ss.ss_sp = PTRIN(lss->ss_sp);
	ss.ss_size = lss->ss_size;
	ss.ss_flags = linux_to_bsd_sigaltstack(lss->ss_flags);

	(void)kern_sigaltstack(td, &ss, NULL);

	return (EJUSTRETURN);
}

static int
linux_fetch_syscall_args(struct thread *td)
{
	struct proc *p;
	struct trapframe *frame;
	struct syscall_args *sa;

	p = td->td_proc;
	frame = td->td_frame;
	sa = &td->td_sa;

	sa->code = frame->tf_eax;
	sa->original_code = sa->code;
	sa->args[0] = frame->tf_ebx;
	sa->args[1] = frame->tf_ecx;
	sa->args[2] = frame->tf_edx;
	sa->args[3] = frame->tf_esi;
	sa->args[4] = frame->tf_edi;
	sa->args[5] = frame->tf_ebp;

	if (sa->code >= p->p_sysent->sv_size)
		/* nosys */
		sa->callp = &nosys_sysent;
	else
		sa->callp = &p->p_sysent->sv_table[sa->code];

	td->td_retval[0] = 0;
	td->td_retval[1] = frame->tf_edx;

	return (0);
}

static void
linux_set_syscall_retval(struct thread *td, int error)
{
	struct trapframe *frame = td->td_frame;

	cpu_set_syscall_retval(td, error);

	if (__predict_false(error != 0)) {
		if (error != ERESTART && error != EJUSTRETURN)
			frame->tf_eax = bsd_to_linux_errno(error);
	}
}

static void
linux_set_fork_retval(struct thread *td)
{
	struct trapframe *frame = td->td_frame;

	frame->tf_eax = 0;
}

/*
 * exec_setregs may initialize some registers differently than Linux
 * does, thus potentially confusing Linux binaries. If necessary, we
 * override the exec_setregs default(s) here.
 */
static void
linux_exec_setregs(struct thread *td, struct image_params *imgp,
    uintptr_t stack)
{
	struct pcb *pcb = td->td_pcb;

	exec_setregs(td, imgp, stack);

	/* Linux sets %gs to 0, we default to _udatasel. */
	pcb->pcb_gs = 0;
	load_gs(0);

	pcb->pcb_initial_npxcw = __LINUX_NPXCW__;
}

struct sysentvec linux_sysvec = {
	.sv_size	= LINUX_SYS_MAXSYSCALL,
	.sv_table	= linux_sysent,
	.sv_fixup	= linux_fixup,
	.sv_sendsig	= linux_sendsig,
	.sv_sigcode	= &_binary_linux_vdso_so_o_start,
	.sv_szsigcode	= &linux_szsigcode,
	.sv_name	= "Linux a.out",
	.sv_coredump	= NULL,
	.sv_minsigstksz	= LINUX_MINSIGSTKSZ,
	.sv_minuser	= VM_MIN_ADDRESS,
	.sv_maxuser	= VM_MAXUSER_ADDRESS,
	.sv_usrstack	= LINUX_USRSTACK,
	.sv_psstrings	= PS_STRINGS,
	.sv_psstringssz	= sizeof(struct ps_strings),
	.sv_stackprot	= VM_PROT_ALL,
	.sv_copyout_strings = exec_copyout_strings,
	.sv_setregs	= linux_exec_setregs,
	.sv_fixlimit	= NULL,
	.sv_maxssiz	= NULL,
	.sv_flags	= SV_ABI_LINUX | SV_AOUT | SV_ILP32 |
	    SV_SIG_DISCIGN | SV_SIG_WAITNDQ,
	.sv_set_syscall_retval = linux_set_syscall_retval,
	.sv_fetch_syscall_args = linux_fetch_syscall_args,
	.sv_syscallnames = linux_syscallnames,
	.sv_schedtail	= linux_schedtail,
	.sv_thread_detach = linux_thread_detach,
	.sv_trap	= NULL,
	.sv_hwcap	= NULL,
	.sv_hwcap2	= NULL,
	.sv_onexec	= linux_on_exec_vmspace,
	.sv_onexit	= linux_on_exit,
	.sv_ontdexit	= linux_thread_dtor,
	.sv_setid_allowed = &linux_setid_allowed_query,
	.sv_set_fork_retval = linux_set_fork_retval,
};
INIT_SYSENTVEC(aout_sysvec, &linux_sysvec);

struct sysentvec elf_linux_sysvec = {
	.sv_size	= LINUX_SYS_MAXSYSCALL,
	.sv_table	= linux_sysent,
	.sv_fixup	= __elfN(freebsd_fixup),
	.sv_sendsig	= linux_sendsig,
	.sv_sigcode	= &_binary_linux_vdso_so_o_start,
	.sv_szsigcode	= &linux_szsigcode,
	.sv_name	= "Linux ELF32",
	.sv_coredump	= elf32_coredump,
	.sv_elf_core_osabi = ELFOSABI_NONE,
	.sv_elf_core_abi_vendor = LINUX_ABI_VENDOR,
	.sv_elf_core_prepare_notes = __linuxN(prepare_notes),
	.sv_minsigstksz	= LINUX_MINSIGSTKSZ,
	.sv_minuser	= VM_MIN_ADDRESS,
	.sv_maxuser	= VM_MAXUSER_ADDRESS,
	.sv_usrstack	= LINUX_USRSTACK,
	.sv_psstrings	= LINUX_PS_STRINGS,
	.sv_psstringssz	= sizeof(struct ps_strings),
	.sv_stackprot	= VM_PROT_ALL,
	.sv_copyout_auxargs = __linuxN(copyout_auxargs),
	.sv_copyout_strings = __linuxN(copyout_strings),
	.sv_setregs	= linux_exec_setregs,
	.sv_fixlimit	= NULL,
	.sv_maxssiz	= NULL,
	.sv_flags	= SV_ABI_LINUX | SV_ILP32 | SV_SHP |
	    SV_SIG_DISCIGN | SV_SIG_WAITNDQ | SV_TIMEKEEP,
	.sv_set_syscall_retval = linux_set_syscall_retval,
	.sv_fetch_syscall_args = linux_fetch_syscall_args,
	.sv_syscallnames = NULL,
	.sv_shared_page_base = LINUX_SHAREDPAGE,
	.sv_shared_page_len = PAGE_SIZE,
	.sv_schedtail	= linux_schedtail,
	.sv_thread_detach = linux_thread_detach,
	.sv_trap	= NULL,
	.sv_hwcap	= NULL,
	.sv_hwcap2	= NULL,
	.sv_onexec	= linux_on_exec_vmspace,
	.sv_onexit	= linux_on_exit,
	.sv_ontdexit	= linux_thread_dtor,
	.sv_setid_allowed = &linux_setid_allowed_query,
	.sv_set_fork_retval = linux_set_fork_retval,
};

static int
linux_on_exec_vmspace(struct proc *p, struct image_params *imgp)
{
	int error = 0;

	if (SV_PROC_FLAG(p, SV_SHP) != 0)
		error = linux_map_vdso(p, linux_vdso_obj,
		    linux_vdso_base, LINUX_VDSOPAGE_SIZE, imgp);
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
	/* Fill timekeep_base */
	exec_sysvec_init(sv);

	tkoff = kern_timekeep_base - linux_vdso_base;
	ktimekeep_base = (l_uintptr_t *)(linux_vdso_mapping + tkoff);
	*ktimekeep_base = sv->sv_shared_page_base + sv->sv_timekeep_offset;

	tkoff = kern_tsc_selector - linux_vdso_base;
	ktsc_selector = (l_uintptr_t *)(linux_vdso_mapping + tkoff);
	*ktsc_selector = linux_vdso_tsc_selector_idx();
	if (bootverbose)
		printf("Linux i386 vDSO tsc_selector: %u\n", *ktsc_selector);

	tkoff = kern_cpu_selector - linux_vdso_base;
	ktsc_selector = (l_uintptr_t *)(linux_vdso_mapping + tkoff);
	*ktsc_selector = linux_vdso_cpu_selector_idx();
	if (bootverbose)
		printf("Linux i386 vDSO cpu_selector: %u\n", *ktsc_selector);
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

	linux_vdso_base = LINUX_VDSOPAGE;

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
	const Elf_Shdr *shdr;
	const Elf_Rel *rel;
	const Elf_Ehdr *ehdr;
	Elf_Addr *where;
	Elf_Size rtype, symidx;
	Elf_Addr addr, addend;
	int i, relcnt;

	MPASS(offset != 0);

	relcnt = 0;
	ehdr = (const Elf_Ehdr *)mapping;
	shdr = (const Elf_Shdr *)(mapping + ehdr->e_shoff);
	for (i = 0; i < ehdr->e_shnum; i++)
	{
		switch (shdr[i].sh_type) {
		case SHT_REL:
			rel = (const Elf_Rel *)(mapping + shdr[i].sh_offset);
			relcnt = shdr[i].sh_size / sizeof(*rel);
			break;
		case SHT_RELA:
			printf("Linux i386 vDSO: unexpected Rela section\n");
			break;
		}
	}

	for (i = 0; i < relcnt; i++, rel++) {
		where = (Elf_Addr *)(mapping + rel->r_offset);
		addend = *where;
		rtype = ELF_R_TYPE(rel->r_info);
		symidx = ELF_R_SYM(rel->r_info);

		switch (rtype) {
		case R_386_NONE:	/* none */
			break;

		case R_386_RELATIVE:	/* B + A */
			addr = (Elf_Addr)PTROUT(offset + addend);
			if (*where != addr)
				*where = addr;
			break;

		case R_386_IRELATIVE:
			printf("Linux i386 vDSO: unexpected ifunc relocation, "
			    "symbol index %d\n", symidx);
			break;
		default:
			printf("Linux i386 vDSO: unexpected relocation type %d, "
			    "symbol index %d\n", rtype, symidx);
		}
	}
}

static Elf_Brandnote linux_brandnote = {
	.hdr.n_namesz	= sizeof(GNU_ABI_VENDOR),
	.hdr.n_descsz	= 16,	/* XXX at least 16 */
	.hdr.n_type	= 1,
	.vendor		= GNU_ABI_VENDOR,
	.flags		= BN_TRANSLATE_OSREL,
	.trans_osrel	= linux_trans_osrel
};

static Elf32_Brandinfo linux_brand = {
	.brand		= ELFOSABI_LINUX,
	.machine	= EM_386,
	.compat_3_brand	= "Linux",
	.interp_path	= "/lib/ld-linux.so.1",
	.sysvec		= &elf_linux_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &linux_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE
};

static Elf32_Brandinfo linux_glibc2brand = {
	.brand		= ELFOSABI_LINUX,
	.machine	= EM_386,
	.compat_3_brand	= "Linux",
	.interp_path	= "/lib/ld-linux.so.2",
	.sysvec		= &elf_linux_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &linux_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE
};

static Elf32_Brandinfo linux_muslbrand = {
	.brand		= ELFOSABI_LINUX,
	.machine	= EM_386,
	.compat_3_brand	= "Linux",
	.interp_path	= "/lib/ld-musl-i386.so.1",
	.sysvec		= &elf_linux_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &linux_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE |
			    LINUX_BI_FUTEX_REQUEUE
};

Elf32_Brandinfo *linux_brandlist[] = {
	&linux_brand,
	&linux_glibc2brand,
	&linux_muslbrand,
	NULL
};

static int
linux_elf_modevent(module_t mod, int type, void *data)
{
	Elf32_Brandinfo **brandinfo;
	int error;
	struct linux_ioctl_handler **lihp;

	error = 0;

	switch(type) {
	case MOD_LOAD:
		for (brandinfo = &linux_brandlist[0]; *brandinfo != NULL;
		     ++brandinfo)
			if (elf32_insert_brand_entry(*brandinfo) < 0)
				error = EINVAL;
		if (error == 0) {
			SET_FOREACH(lihp, linux_ioctl_handler_set)
				linux_ioctl_register_handler(*lihp);
			linux_dev_shm_create();
			linux_osd_jail_register();
			linux_netlink_register();
			stclohz = (stathz ? stathz : hz);
			if (bootverbose)
				printf("Linux ELF exec handler installed\n");
		} else
			printf("cannot insert Linux ELF brand handler\n");
		break;
	case MOD_UNLOAD:
		for (brandinfo = &linux_brandlist[0]; *brandinfo != NULL;
		     ++brandinfo)
			if (elf32_brand_inuse(*brandinfo))
				error = EBUSY;
		if (error == 0) {
			for (brandinfo = &linux_brandlist[0];
			     *brandinfo != NULL; ++brandinfo)
				if (elf32_remove_brand_entry(*brandinfo) < 0)
					error = EINVAL;
		}
		if (error == 0) {
			SET_FOREACH(lihp, linux_ioctl_handler_set)
				linux_ioctl_unregister_handler(*lihp);
			linux_netlink_deregister();
			linux_dev_shm_destroy();
			linux_osd_jail_deregister();
			if (bootverbose)
				printf("Linux ELF exec handler removed\n");
		} else
			printf("Could not deinstall ELF interpreter entry\n");
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (error);
}

static moduledata_t linux_elf_mod = {
	"linuxelf",
	linux_elf_modevent,
	0
};

DECLARE_MODULE_TIED(linuxelf, linux_elf_mod, SI_SUB_EXEC, SI_ORDER_ANY);
MODULE_DEPEND(linuxelf, netlink, 1, 1, 1);
FEATURE(linux, "Linux 32bit support");
