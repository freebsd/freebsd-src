/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2004 Tim J. Robbins
 * Copyright (c) 2003 Peter Wemm
 * Copyright (c) 2002 Doug Rabson
 * Copyright (c) 1998-1999 Andrew Gallatin
 * Copyright (c) 1994-1996 Søren Schmidt
 * All rights reserved.
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

#include "opt_compat.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef COMPAT_FREEBSD32
#error "Unable to compile Linux-emulator due to missing COMPAT_FREEBSD32 option!"
#endif

#define	__ELF_WORD_SIZE	32

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/fcntl.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/stddef.h>
#include <sys/signalvar.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/vnode.h>
#include <sys/eventhandler.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>

#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/specialreg.h>
#include <machine/trap.h>

#include <x86/linux/linux_x86.h>
#include <amd64/linux32/linux.h>
#include <amd64/linux32/linux32_proto.h>
#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_fork.h>
#include <compat/linux/linux_ioctl.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_vdso.h>

MODULE_VERSION(linux, 1);

#define	LINUX32_MAXUSER		((1ul << 32) - PAGE_SIZE)
#define	LINUX32_VDSOPAGE_SIZE	PAGE_SIZE * 2
#define	LINUX32_VDSOPAGE	(LINUX32_MAXUSER - LINUX32_VDSOPAGE_SIZE)
#define	LINUX32_SHAREDPAGE	(LINUX32_VDSOPAGE - PAGE_SIZE)
				/*
				 * PAGE_SIZE - the size
				 * of the native SHAREDPAGE
				 */
#define	LINUX32_USRSTACK	LINUX32_SHAREDPAGE

static int linux_szsigcode;
static vm_object_t linux_vdso_obj;
static char *linux_vdso_mapping;
extern char _binary_linux32_vdso_so_o_start;
extern char _binary_linux32_vdso_so_o_end;
static vm_offset_t linux_vdso_base;

extern struct sysent linux32_sysent[LINUX32_SYS_MAXSYSCALL];

SET_DECLARE(linux_ioctl_handler_set, struct linux_ioctl_handler);

static int	linux_fixup_elf(uintptr_t *stack_base,
		    struct image_params *iparams);
static int	linux_copyout_strings(struct image_params *imgp,
		    uintptr_t *stack_base);
static void     linux_sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask);
static void	linux_exec_setregs(struct thread *td,
				   struct image_params *imgp, uintptr_t stack);
static void	linux_exec_sysvec_init(void *param);
static int	linux_on_exec_vmspace(struct proc *p,
		    struct image_params *imgp);
static void	linux32_fixlimit(struct rlimit *rl, int which);
static bool	linux32_trans_osrel(const Elf_Note *note, int32_t *osrel);
static void	linux_vdso_install(const void *param);
static void	linux_vdso_deinstall(const void *param);
static void	linux_vdso_reloc(char *mapping, Elf_Addr offset);
static void	linux32_set_fork_retval(struct thread *td);
static void	linux32_set_syscall_retval(struct thread *td, int error);

#define LINUX_T_UNKNOWN  255
static int _bsd_to_linux_trapcode[] = {
	LINUX_T_UNKNOWN,	/* 0 */
	6,			/* 1  T_PRIVINFLT */
	LINUX_T_UNKNOWN,	/* 2 */
	3,			/* 3  T_BPTFLT */
	LINUX_T_UNKNOWN,	/* 4 */
	LINUX_T_UNKNOWN,	/* 5 */
	16,			/* 6  T_ARITHTRAP */
	254,			/* 7  T_ASTFLT */
	LINUX_T_UNKNOWN,	/* 8 */
	13,			/* 9  T_PROTFLT */
	1,			/* 10 T_TRCTRAP */
	LINUX_T_UNKNOWN,	/* 11 */
	14,			/* 12 T_PAGEFLT */
	LINUX_T_UNKNOWN,	/* 13 */
	17,			/* 14 T_ALIGNFLT */
	LINUX_T_UNKNOWN,	/* 15 */
	LINUX_T_UNKNOWN,	/* 16 */
	LINUX_T_UNKNOWN,	/* 17 */
	0,			/* 18 T_DIVIDE */
	2,			/* 19 T_NMI */
	4,			/* 20 T_OFLOW */
	5,			/* 21 T_BOUND */
	7,			/* 22 T_DNA */
	8,			/* 23 T_DOUBLEFLT */
	9,			/* 24 T_FPOPFLT */
	10,			/* 25 T_TSSFLT */
	11,			/* 26 T_SEGNPFLT */
	12,			/* 27 T_STKFLT */
	18,			/* 28 T_MCHK */
	19,			/* 29 T_XMMFLT */
	15			/* 30 T_RESERVED */
};
#define bsd_to_linux_trapcode(code) \
    ((code)<nitems(_bsd_to_linux_trapcode)? \
     _bsd_to_linux_trapcode[(code)]: \
     LINUX_T_UNKNOWN)

struct linux32_ps_strings {
	u_int32_t ps_argvstr;	/* first of 0 or more argument strings */
	u_int ps_nargvstr;	/* the number of argument strings */
	u_int32_t ps_envstr;	/* first of 0 or more environment strings */
	u_int ps_nenvstr;	/* the number of environment strings */
};
#define	LINUX32_PS_STRINGS	(LINUX32_USRSTACK - \
				    sizeof(struct linux32_ps_strings))

LINUX_VDSO_SYM_INTPTR(__kernel_vsyscall);
LINUX_VDSO_SYM_INTPTR(__kernel_sigreturn);
LINUX_VDSO_SYM_INTPTR(__kernel_rt_sigreturn);
LINUX_VDSO_SYM_INTPTR(kern_timekeep_base);
LINUX_VDSO_SYM_INTPTR(kern_tsc_selector);
LINUX_VDSO_SYM_CHAR(linux_platform);

/*
 * If FreeBSD & Linux have a difference of opinion about what a trap
 * means, deal with it here.
 *
 * MPSAFE
 */
static int
linux_translate_traps(int signal, int trap_code)
{
	if (signal != SIGBUS)
		return (signal);
	switch (trap_code) {
	case T_PROTFLT:
	case T_TSSFLT:
	case T_DOUBLEFLT:
	case T_PAGEFLT:
		return (SIGSEGV);
	default:
		return (signal);
	}
}

static int
linux_copyout_auxargs(struct image_params *imgp, uintptr_t base)
{
	Elf32_Auxargs *args;
	Elf32_Auxinfo *argarray, *pos;
	int error, issetugid;

	args = (Elf32_Auxargs *)imgp->auxargs;
	argarray = pos = malloc(LINUX_AT_COUNT * sizeof(*pos), M_TEMP,
	    M_WAITOK | M_ZERO);

	issetugid = imgp->proc->p_flag & P_SUGID ? 1 : 0;
	AUXARGS_ENTRY(pos, LINUX_AT_SYSINFO, __kernel_vsyscall);
	AUXARGS_ENTRY(pos, LINUX_AT_SYSINFO_EHDR, linux_vdso_base);
	AUXARGS_ENTRY(pos, LINUX_AT_HWCAP, cpu_feature);
	AUXARGS_ENTRY(pos, AT_PAGESZ, args->pagesz);

	/*
	 * Do not export AT_CLKTCK when emulating Linux kernel prior to 2.4.0,
	 * as it has appeared in the 2.4.0-rc7 first time.
	 * Being exported, AT_CLKTCK is returned by sysconf(_SC_CLK_TCK),
	 * glibc falls back to the hard-coded CLK_TCK value when aux entry
	 * is not present.
	 * Also see linux_times() implementation.
	 */
	if (linux_kernver(curthread) >= LINUX_KERNVER_2004000)
		AUXARGS_ENTRY(pos, LINUX_AT_CLKTCK, stclohz);
	AUXARGS_ENTRY(pos, AT_PHDR, args->phdr);
	AUXARGS_ENTRY(pos, AT_PHENT, args->phent);
	AUXARGS_ENTRY(pos, AT_PHNUM, args->phnum);
	AUXARGS_ENTRY(pos, AT_BASE, args->base);
	AUXARGS_ENTRY(pos, AT_FLAGS, args->flags);
	AUXARGS_ENTRY(pos, AT_ENTRY, args->entry);
	AUXARGS_ENTRY(pos, AT_UID, imgp->proc->p_ucred->cr_ruid);
	AUXARGS_ENTRY(pos, AT_EUID, imgp->proc->p_ucred->cr_svuid);
	AUXARGS_ENTRY(pos, AT_GID, imgp->proc->p_ucred->cr_rgid);
	AUXARGS_ENTRY(pos, AT_EGID, imgp->proc->p_ucred->cr_svgid);
	AUXARGS_ENTRY(pos, LINUX_AT_SECURE, issetugid);
	AUXARGS_ENTRY(pos, LINUX_AT_RANDOM, PTROUT(imgp->canary));
	AUXARGS_ENTRY(pos, LINUX_AT_HWCAP2, 0);
	if (imgp->execpathp != 0)
		AUXARGS_ENTRY(pos, LINUX_AT_EXECFN, PTROUT(imgp->execpathp));
	if (args->execfd != -1)
		AUXARGS_ENTRY(pos, AT_EXECFD, args->execfd);
	AUXARGS_ENTRY(pos, LINUX_AT_PLATFORM, PTROUT(linux_platform));
	AUXARGS_ENTRY(pos, AT_NULL, 0);

	free(imgp->auxargs, M_TEMP);
	imgp->auxargs = NULL;
	KASSERT(pos - argarray <= LINUX_AT_COUNT, ("Too many auxargs"));

	error = copyout(argarray, (void *)base,
	    sizeof(*argarray) * LINUX_AT_COUNT);
	free(argarray, M_TEMP);
	return (error);
}

static int
linux_fixup_elf(uintptr_t *stack_base, struct image_params *imgp)
{
	Elf32_Addr *base;

	base = (Elf32_Addr *)*stack_base;
	base--;
	if (suword32(base, (uint32_t)imgp->args->argc) == -1)
		return (EFAULT);
	*stack_base = (uintptr_t)base;
	return (0);
}

static void
linux_rt_sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct sigacts *psp;
	struct trapframe *regs;
	struct l_rt_sigframe *fp, frame;
	int oonstack;
	int sig;
	int code;

	sig = ksi->ksi_signo;
	code = ksi->ksi_code;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	regs = td->td_frame;
	oonstack = sigonstack(regs->tf_rsp);

	/* Allocate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		fp = (struct l_rt_sigframe *)((uintptr_t)td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size - sizeof(struct l_rt_sigframe));
	} else
		fp = (struct l_rt_sigframe *)regs->tf_rsp - 1;
	mtx_unlock(&psp->ps_mtx);

	/* Build the argument list for the signal handler. */
	sig = bsd_to_linux_signal(sig);

	bzero(&frame, sizeof(frame));

	frame.sf_handler = PTROUT(catcher);
	frame.sf_sig = sig;
	frame.sf_siginfo = PTROUT(&fp->sf_si);
	frame.sf_ucontext = PTROUT(&fp->sf_sc);

	/* Fill in POSIX parts. */
	siginfo_to_lsiginfo(&ksi->ksi_info, &frame.sf_si, sig);

	/*
	 * Build the signal context to be used by sigreturn and libgcc unwind.
	 */
	frame.sf_sc.uc_flags = 0;		/* XXX ??? */
	frame.sf_sc.uc_link = 0;		/* XXX ??? */

	frame.sf_sc.uc_stack.ss_sp = PTROUT(td->td_sigstk.ss_sp);
	frame.sf_sc.uc_stack.ss_size = td->td_sigstk.ss_size;
	frame.sf_sc.uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK)
	    ? ((oonstack) ? LINUX_SS_ONSTACK : 0) : LINUX_SS_DISABLE;
	PROC_UNLOCK(p);

	bsd_to_linux_sigset(mask, &frame.sf_sc.uc_sigmask);

	frame.sf_sc.uc_mcontext.sc_mask   = frame.sf_sc.uc_sigmask.__mask;
	frame.sf_sc.uc_mcontext.sc_edi    = regs->tf_rdi;
	frame.sf_sc.uc_mcontext.sc_esi    = regs->tf_rsi;
	frame.sf_sc.uc_mcontext.sc_ebp    = regs->tf_rbp;
	frame.sf_sc.uc_mcontext.sc_ebx    = regs->tf_rbx;
	frame.sf_sc.uc_mcontext.sc_esp    = regs->tf_rsp;
	frame.sf_sc.uc_mcontext.sc_edx    = regs->tf_rdx;
	frame.sf_sc.uc_mcontext.sc_ecx    = regs->tf_rcx;
	frame.sf_sc.uc_mcontext.sc_eax    = regs->tf_rax;
	frame.sf_sc.uc_mcontext.sc_eip    = regs->tf_rip;
	frame.sf_sc.uc_mcontext.sc_cs     = regs->tf_cs;
	frame.sf_sc.uc_mcontext.sc_gs     = regs->tf_gs;
	frame.sf_sc.uc_mcontext.sc_fs     = regs->tf_fs;
	frame.sf_sc.uc_mcontext.sc_es     = regs->tf_es;
	frame.sf_sc.uc_mcontext.sc_ds     = regs->tf_ds;
	frame.sf_sc.uc_mcontext.sc_eflags = regs->tf_rflags;
	frame.sf_sc.uc_mcontext.sc_esp_at_signal = regs->tf_rsp;
	frame.sf_sc.uc_mcontext.sc_ss     = regs->tf_ss;
	frame.sf_sc.uc_mcontext.sc_err    = regs->tf_err;
	frame.sf_sc.uc_mcontext.sc_cr2    = (u_int32_t)(uintptr_t)ksi->ksi_addr;
	frame.sf_sc.uc_mcontext.sc_trapno = bsd_to_linux_trapcode(code);

	if (copyout(&frame, fp, sizeof(frame)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	/* Build context to run handler in. */
	regs->tf_rsp = PTROUT(fp);
	regs->tf_rip = __kernel_rt_sigreturn;
	regs->tf_rflags &= ~(PSL_T | PSL_D);
	regs->tf_cs = _ucode32sel;
	regs->tf_ss = _udatasel;
	regs->tf_ds = _udatasel;
	regs->tf_es = _udatasel;
	regs->tf_fs = _ufssel;
	regs->tf_gs = _ugssel;
	regs->tf_flags = TF_HASSEGS;
	set_pcb_flags(td->td_pcb, PCB_FULL_IRET);
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
	int oonstack;
	int sig, code;

	sig = ksi->ksi_signo;
	code = ksi->ksi_code;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	if (SIGISMEMBER(psp->ps_siginfo, sig)) {
		/* Signal handler installed with SA_SIGINFO. */
		linux_rt_sendsig(catcher, ksi, mask);
		return;
	}

	regs = td->td_frame;
	oonstack = sigonstack(regs->tf_rsp);

	/* Allocate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		fp = (struct l_sigframe *)((uintptr_t)td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size - sizeof(struct l_sigframe));
	} else
		fp = (struct l_sigframe *)regs->tf_rsp - 1;
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	/* Build the argument list for the signal handler. */
	sig = bsd_to_linux_signal(sig);

	bzero(&frame, sizeof(frame));

	frame.sf_handler = PTROUT(catcher);
	frame.sf_sig = sig;

	bsd_to_linux_sigset(mask, &lmask);

	/* Build the signal context to be used by sigreturn. */
	frame.sf_sc.sc_mask   = lmask.__mask;
	frame.sf_sc.sc_gs     = regs->tf_gs;
	frame.sf_sc.sc_fs     = regs->tf_fs;
	frame.sf_sc.sc_es     = regs->tf_es;
	frame.sf_sc.sc_ds     = regs->tf_ds;
	frame.sf_sc.sc_edi    = regs->tf_rdi;
	frame.sf_sc.sc_esi    = regs->tf_rsi;
	frame.sf_sc.sc_ebp    = regs->tf_rbp;
	frame.sf_sc.sc_ebx    = regs->tf_rbx;
	frame.sf_sc.sc_esp    = regs->tf_rsp;
	frame.sf_sc.sc_edx    = regs->tf_rdx;
	frame.sf_sc.sc_ecx    = regs->tf_rcx;
	frame.sf_sc.sc_eax    = regs->tf_rax;
	frame.sf_sc.sc_eip    = regs->tf_rip;
	frame.sf_sc.sc_cs     = regs->tf_cs;
	frame.sf_sc.sc_eflags = regs->tf_rflags;
	frame.sf_sc.sc_esp_at_signal = regs->tf_rsp;
	frame.sf_sc.sc_ss     = regs->tf_ss;
	frame.sf_sc.sc_err    = regs->tf_err;
	frame.sf_sc.sc_cr2    = (u_int32_t)(uintptr_t)ksi->ksi_addr;
	frame.sf_sc.sc_trapno = bsd_to_linux_trapcode(code);

	frame.sf_extramask[0] = lmask.__mask;

	if (copyout(&frame, fp, sizeof(frame)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	/* Build context to run handler in. */
	regs->tf_rsp = PTROUT(fp);
	regs->tf_rip = __kernel_sigreturn;
	regs->tf_rflags &= ~(PSL_T | PSL_D);
	regs->tf_cs = _ucode32sel;
	regs->tf_ss = _udatasel;
	regs->tf_ds = _udatasel;
	regs->tf_es = _udatasel;
	regs->tf_fs = _ufssel;
	regs->tf_gs = _ugssel;
	regs->tf_flags = TF_HASSEGS;
	set_pcb_flags(td->td_pcb, PCB_FULL_IRET);
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
	sigset_t bmask;
	l_sigset_t lmask;
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
	eflags = frame.sf_sc.sc_eflags;
	if (!EFL_SECURE(eflags, regs->tf_rflags))
		return(EINVAL);

	/*
	 * Don't allow users to load a valid privileged %cs.  Let the
	 * hardware check for invalid selectors, excess privilege in
	 * other selectors, invalid %eip's and invalid %esp's.
	 */
	if (!CS_SECURE(frame.sf_sc.sc_cs)) {
		ksiginfo_init_trap(&ksi);
		ksi.ksi_signo = SIGBUS;
		ksi.ksi_code = BUS_OBJERR;
		ksi.ksi_trapno = T_PROTFLT;
		ksi.ksi_addr = (void *)regs->tf_rip;
		trapsignal(td, &ksi);
		return(EINVAL);
	}

	lmask.__mask = frame.sf_sc.sc_mask;
	lmask.__mask = frame.sf_extramask[0];
	linux_to_bsd_sigset(&lmask, &bmask);
	kern_sigprocmask(td, SIG_SETMASK, &bmask, NULL, 0);

	/* Restore signal context. */
	regs->tf_rdi    = frame.sf_sc.sc_edi;
	regs->tf_rsi    = frame.sf_sc.sc_esi;
	regs->tf_rbp    = frame.sf_sc.sc_ebp;
	regs->tf_rbx    = frame.sf_sc.sc_ebx;
	regs->tf_rdx    = frame.sf_sc.sc_edx;
	regs->tf_rcx    = frame.sf_sc.sc_ecx;
	regs->tf_rax    = frame.sf_sc.sc_eax;
	regs->tf_rip    = frame.sf_sc.sc_eip;
	regs->tf_cs     = frame.sf_sc.sc_cs;
	regs->tf_ds     = frame.sf_sc.sc_ds;
	regs->tf_es     = frame.sf_sc.sc_es;
	regs->tf_fs     = frame.sf_sc.sc_fs;
	regs->tf_gs     = frame.sf_sc.sc_gs;
	regs->tf_rflags = eflags;
	regs->tf_rsp    = frame.sf_sc.sc_esp_at_signal;
	regs->tf_ss     = frame.sf_sc.sc_ss;
	set_pcb_flags(td->td_pcb, PCB_FULL_IRET);

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
	eflags = context->sc_eflags;
	if (!EFL_SECURE(eflags, regs->tf_rflags))
		return(EINVAL);

	/*
	 * Don't allow users to load a valid privileged %cs.  Let the
	 * hardware check for invalid selectors, excess privilege in
	 * other selectors, invalid %eip's and invalid %esp's.
	 */
	if (!CS_SECURE(context->sc_cs)) {
		ksiginfo_init_trap(&ksi);
		ksi.ksi_signo = SIGBUS;
		ksi.ksi_code = BUS_OBJERR;
		ksi.ksi_trapno = T_PROTFLT;
		ksi.ksi_addr = (void *)regs->tf_rip;
		trapsignal(td, &ksi);
		return(EINVAL);
	}

	linux_to_bsd_sigset(&uc.uc_sigmask, &bmask);
	kern_sigprocmask(td, SIG_SETMASK, &bmask, NULL, 0);

	/*
	 * Restore signal context
	 */
	regs->tf_gs	= context->sc_gs;
	regs->tf_fs	= context->sc_fs;
	regs->tf_es	= context->sc_es;
	regs->tf_ds	= context->sc_ds;
	regs->tf_rdi    = context->sc_edi;
	regs->tf_rsi    = context->sc_esi;
	regs->tf_rbp    = context->sc_ebp;
	regs->tf_rbx    = context->sc_ebx;
	regs->tf_rdx    = context->sc_edx;
	regs->tf_rcx    = context->sc_ecx;
	regs->tf_rax    = context->sc_eax;
	regs->tf_rip    = context->sc_eip;
	regs->tf_cs     = context->sc_cs;
	regs->tf_rflags = eflags;
	regs->tf_rsp    = context->sc_esp_at_signal;
	regs->tf_ss     = context->sc_ss;
	set_pcb_flags(td->td_pcb, PCB_FULL_IRET);

	/*
	 * call sigaltstack & ignore results..
	 */
	lss = &uc.uc_stack;
	ss.ss_sp = PTRIN(lss->ss_sp);
	ss.ss_size = lss->ss_size;
	ss.ss_flags = linux_to_bsd_sigaltstack(lss->ss_flags);

	(void)kern_sigaltstack(td, &ss, NULL);

	return (EJUSTRETURN);
}

static int
linux32_fetch_syscall_args(struct thread *td)
{
	struct proc *p;
	struct trapframe *frame;
	struct syscall_args *sa;

	p = td->td_proc;
	frame = td->td_frame;
	sa = &td->td_sa;

	sa->args[0] = frame->tf_rbx;
	sa->args[1] = frame->tf_rcx;
	sa->args[2] = frame->tf_rdx;
	sa->args[3] = frame->tf_rsi;
	sa->args[4] = frame->tf_rdi;
	sa->args[5] = frame->tf_rbp;	/* Unconfirmed */
	sa->code = frame->tf_rax;
	sa->original_code = sa->code;

	if (sa->code >= p->p_sysent->sv_size)
		/* nosys */
		sa->callp = &p->p_sysent->sv_table[p->p_sysent->sv_size - 1];
	else
		sa->callp = &p->p_sysent->sv_table[sa->code];

	td->td_retval[0] = 0;
	td->td_retval[1] = frame->tf_rdx;

	return (0);
}

static void
linux32_set_syscall_retval(struct thread *td, int error)
{
	struct trapframe *frame = td->td_frame;

	cpu_set_syscall_retval(td, error);

	if (__predict_false(error != 0)) {
		if (error != ERESTART && error != EJUSTRETURN)
			frame->tf_rax = bsd_to_linux_errno(error);
	}
}

static void
linux32_set_fork_retval(struct thread *td)
{
	struct trapframe *frame = td->td_frame;

	frame->tf_rax = 0;
}

/*
 * Clear registers on exec
 * XXX copied from ia32_signal.c.
 */
static void
linux_exec_setregs(struct thread *td, struct image_params *imgp,
    uintptr_t stack)
{
	struct trapframe *regs = td->td_frame;
	struct pcb *pcb = td->td_pcb;
	register_t saved_rflags;

	regs = td->td_frame;
	pcb = td->td_pcb;

	if (td->td_proc->p_md.md_ldt != NULL)
		user_ldt_free(td);

	critical_enter();
	wrmsr(MSR_FSBASE, 0);
	wrmsr(MSR_KGSBASE, 0);	/* User value while we're in the kernel */
	pcb->pcb_fsbase = 0;
	pcb->pcb_gsbase = 0;
	critical_exit();
	pcb->pcb_initial_fpucw = __LINUX_NPXCW__;

	saved_rflags = regs->tf_rflags & PSL_T;
	bzero((char *)regs, sizeof(struct trapframe));
	regs->tf_rip = imgp->entry_addr;
	regs->tf_rsp = stack;
	regs->tf_rflags = PSL_USER | saved_rflags;
	regs->tf_gs = _ugssel;
	regs->tf_fs = _ufssel;
	regs->tf_es = _udatasel;
	regs->tf_ds = _udatasel;
	regs->tf_ss = _udatasel;
	regs->tf_flags = TF_HASSEGS;
	regs->tf_cs = _ucode32sel;
	regs->tf_rbx = (register_t)imgp->ps_strings;

	x86_clear_dbregs(pcb);

	fpstate_drop(td);

	/* Do full restore on return so that we can change to a different %cs */
	set_pcb_flags(pcb, PCB_32BIT | PCB_FULL_IRET);
}

/*
 * XXX copied from ia32_sysvec.c.
 */
static int
linux_copyout_strings(struct image_params *imgp, uintptr_t *stack_base)
{
	int argc, envc, error;
	u_int32_t *vectp;
	char *stringp;
	uintptr_t destp, ustringp;
	struct linux32_ps_strings *arginfo;
	char canary[LINUX_AT_RANDOM_LEN];
	size_t execpath_len;

	arginfo = (struct linux32_ps_strings *)PROC_PS_STRINGS(imgp->proc);
	destp = (uintptr_t)arginfo;

	if (imgp->execpath != NULL && imgp->auxargs != NULL) {
		execpath_len = strlen(imgp->execpath) + 1;
		destp -= execpath_len;
		destp = rounddown2(destp, sizeof(uint32_t));
		imgp->execpathp = (void *)destp;
		error = copyout(imgp->execpath, imgp->execpathp, execpath_len);
		if (error != 0)
			return (error);
	}

	/* Prepare the canary for SSP. */
	arc4rand(canary, sizeof(canary), 0);
	destp -= roundup(sizeof(canary), sizeof(uint32_t));
	imgp->canary = (void *)destp;
	error = copyout(canary, imgp->canary, sizeof(canary));
	if (error != 0)
		return (error);

	/* Allocate room for the argument and environment strings. */
	destp -= ARG_MAX - imgp->args->stringspace;
	destp = rounddown2(destp, sizeof(uint32_t));
	ustringp = destp;

	if (imgp->auxargs) {
		/*
		 * Allocate room on the stack for the ELF auxargs
		 * array.  It has LINUX_AT_COUNT entries.
		 */
		destp -= LINUX_AT_COUNT * sizeof(Elf32_Auxinfo);
		destp = rounddown2(destp, sizeof(uint32_t));
	}

	vectp = (uint32_t *)destp;

	/*
	 * Allocate room for the argv[] and env vectors including the
	 * terminating NULL pointers.
	 */
	vectp -= imgp->args->argc + 1 + imgp->args->envc + 1;

	/* vectp also becomes our initial stack base. */
	*stack_base = (uintptr_t)vectp;

	stringp = imgp->args->begin_argv;
	argc = imgp->args->argc;
	envc = imgp->args->envc;

	/* Copy out strings - arguments and environment. */
	error = copyout(stringp, (void *)ustringp,
	    ARG_MAX - imgp->args->stringspace);
	if (error != 0)
		return (error);

	/* Fill in "ps_strings" struct for ps, w, etc. */
	if (suword32(&arginfo->ps_argvstr, (uint32_t)(intptr_t)vectp) != 0 ||
	    suword32(&arginfo->ps_nargvstr, argc) != 0)
		return (EFAULT);

	/* Fill in argument portion of vector table. */
	for (; argc > 0; --argc) {
		if (suword32(vectp++, ustringp) != 0)
			return (EFAULT);
		while (*stringp++ != 0)
			ustringp++;
		ustringp++;
	}

	/* A null vector table pointer separates the argp's from the envp's. */
	if (suword32(vectp++, 0) != 0)
		return (EFAULT);

	if (suword32(&arginfo->ps_envstr, (uint32_t)(intptr_t)vectp) != 0 ||
	    suword32(&arginfo->ps_nenvstr, envc) != 0)
		return (EFAULT);

	/* Fill in environment portion of vector table. */
	for (; envc > 0; --envc) {
		if (suword32(vectp++, ustringp) != 0)
			return (EFAULT);
		while (*stringp++ != 0)
			ustringp++;
		ustringp++;
	}

	/* The end of the vector table is a null pointer. */
	if (suword32(vectp, 0) != 0)
		return (EFAULT);

	if (imgp->auxargs) {
		vectp++;
		error = imgp->sysent->sv_copyout_auxargs(imgp,
		    (uintptr_t)vectp);
		if (error != 0)
			return (error);
	}

	return (0);
}

static SYSCTL_NODE(_compat, OID_AUTO, linux32, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "32-bit Linux emulation");

static u_long	linux32_maxdsiz = LINUX32_MAXDSIZ;
SYSCTL_ULONG(_compat_linux32, OID_AUTO, maxdsiz, CTLFLAG_RW,
    &linux32_maxdsiz, 0, "");
static u_long	linux32_maxssiz = LINUX32_MAXSSIZ;
SYSCTL_ULONG(_compat_linux32, OID_AUTO, maxssiz, CTLFLAG_RW,
    &linux32_maxssiz, 0, "");
static u_long	linux32_maxvmem = LINUX32_MAXVMEM;
SYSCTL_ULONG(_compat_linux32, OID_AUTO, maxvmem, CTLFLAG_RW,
    &linux32_maxvmem, 0, "");

static void
linux32_fixlimit(struct rlimit *rl, int which)
{

	switch (which) {
	case RLIMIT_DATA:
		if (linux32_maxdsiz != 0) {
			if (rl->rlim_cur > linux32_maxdsiz)
				rl->rlim_cur = linux32_maxdsiz;
			if (rl->rlim_max > linux32_maxdsiz)
				rl->rlim_max = linux32_maxdsiz;
		}
		break;
	case RLIMIT_STACK:
		if (linux32_maxssiz != 0) {
			if (rl->rlim_cur > linux32_maxssiz)
				rl->rlim_cur = linux32_maxssiz;
			if (rl->rlim_max > linux32_maxssiz)
				rl->rlim_max = linux32_maxssiz;
		}
		break;
	case RLIMIT_VMEM:
		if (linux32_maxvmem != 0) {
			if (rl->rlim_cur > linux32_maxvmem)
				rl->rlim_cur = linux32_maxvmem;
			if (rl->rlim_max > linux32_maxvmem)
				rl->rlim_max = linux32_maxvmem;
		}
		break;
	}
}

struct sysentvec elf_linux_sysvec = {
	.sv_size	= LINUX32_SYS_MAXSYSCALL,
	.sv_table	= linux32_sysent,
	.sv_transtrap	= linux_translate_traps,
	.sv_fixup	= linux_fixup_elf,
	.sv_sendsig	= linux_sendsig,
	.sv_sigcode	= &_binary_linux32_vdso_so_o_start,
	.sv_szsigcode	= &linux_szsigcode,
	.sv_name	= "Linux ELF32",
	.sv_coredump	= elf32_coredump,
	.sv_elf_core_osabi = ELFOSABI_NONE,
	.sv_elf_core_abi_vendor = LINUX_ABI_VENDOR,
	.sv_elf_core_prepare_notes = linux32_prepare_notes,
	.sv_imgact_try	= linux_exec_imgact_try,
	.sv_minsigstksz	= LINUX_MINSIGSTKSZ,
	.sv_minuser	= VM_MIN_ADDRESS,
	.sv_maxuser	= LINUX32_MAXUSER,
	.sv_usrstack	= LINUX32_USRSTACK,
	.sv_psstrings	= LINUX32_PS_STRINGS,
	.sv_psstringssz	= sizeof(struct linux32_ps_strings),
	.sv_stackprot	= VM_PROT_ALL,
	.sv_copyout_auxargs = linux_copyout_auxargs,
	.sv_copyout_strings = linux_copyout_strings,
	.sv_setregs	= linux_exec_setregs,
	.sv_fixlimit	= linux32_fixlimit,
	.sv_maxssiz	= &linux32_maxssiz,
	.sv_flags	= SV_ABI_LINUX | SV_ILP32 | SV_IA32 | SV_SHP |
	    SV_SIG_DISCIGN | SV_SIG_WAITNDQ | SV_TIMEKEEP,
	.sv_set_syscall_retval = linux32_set_syscall_retval,
	.sv_fetch_syscall_args = linux32_fetch_syscall_args,
	.sv_syscallnames = NULL,
	.sv_shared_page_base = LINUX32_SHAREDPAGE,
	.sv_shared_page_len = PAGE_SIZE,
	.sv_schedtail	= linux_schedtail,
	.sv_thread_detach = linux_thread_detach,
	.sv_trap	= NULL,
	.sv_onexec	= linux_on_exec_vmspace,
	.sv_onexit	= linux_on_exit,
	.sv_ontdexit	= linux_thread_dtor,
	.sv_setid_allowed = &linux_setid_allowed_query,
	.sv_set_fork_retval = linux32_set_fork_retval,
};

static int
linux_on_exec_vmspace(struct proc *p, struct image_params *imgp)
{
	int error;

	error = linux_map_vdso(p, linux_vdso_obj, linux_vdso_base,
	    LINUX32_VDSOPAGE_SIZE, imgp);
	if (error == 0)
		linux_on_exec(p, imgp);
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
	*ktimekeep_base = sv->sv_timekeep_base;

	tkoff = kern_tsc_selector - linux_vdso_base;
	ktsc_selector = (l_uintptr_t *)(linux_vdso_mapping + tkoff);
	*ktsc_selector = linux_vdso_tsc_selector_idx();
	if (bootverbose)
		printf("Linux i386 vDSO tsc_selector: %u\n", *ktsc_selector);
}
SYSINIT(elf_linux_exec_sysvec_init, SI_SUB_EXEC + 1, SI_ORDER_ANY,
    linux_exec_sysvec_init, &elf_linux_sysvec);

static void
linux_vdso_install(const void *param)
{
	char *vdso_start = &_binary_linux32_vdso_so_o_start;
	char *vdso_end = &_binary_linux32_vdso_so_o_end;

	linux_szsigcode = vdso_end - vdso_start;
	MPASS(linux_szsigcode <= LINUX32_VDSOPAGE_SIZE);

	linux_vdso_base = LINUX32_VDSOPAGE;

	__elfN(linux_vdso_fixup)(vdso_start, linux_vdso_base);

	linux_vdso_obj = __elfN(linux_shared_page_init)
	    (&linux_vdso_mapping, LINUX32_VDSOPAGE_SIZE);
	bcopy(vdso_start, linux_vdso_mapping, linux_szsigcode);

	linux_vdso_reloc(linux_vdso_mapping, linux_vdso_base);
}
SYSINIT(elf_linux_vdso_init, SI_SUB_EXEC + 1, SI_ORDER_FIRST,
    linux_vdso_install, NULL);

static void
linux_vdso_deinstall(const void *param)
{

	__elfN(linux_shared_page_fini)(linux_vdso_obj,
	    linux_vdso_mapping, LINUX32_VDSOPAGE_SIZE);
}
SYSUNINIT(elf_linux_vdso_uninit, SI_SUB_EXEC, SI_ORDER_FIRST,
    linux_vdso_deinstall, NULL);

static void
linux_vdso_reloc(char *mapping, Elf_Addr offset)
{
	const Elf_Shdr *shdr;
	const Elf_Rel *rel;
	const Elf_Ehdr *ehdr;
	Elf32_Addr *where;
	Elf_Size rtype, symidx;
	Elf32_Addr addr, addend;
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
		where = (Elf32_Addr *)(mapping + rel->r_offset);
		addend = *where;
		rtype = ELF_R_TYPE(rel->r_info);
		symidx = ELF_R_SYM(rel->r_info);

		switch (rtype) {
		case R_386_NONE:	/* none */
			break;

		case R_386_RELATIVE:	/* B + A */
			addr = (Elf32_Addr)PTROUT(offset + addend);
			if (*where != addr)
				*where = addr;
			break;

		case R_386_IRELATIVE:
			printf("Linux i386 vDSO: unexpected ifunc relocation, "
			    "symbol index %ld\n", (intmax_t)symidx);
			break;
		default:
			printf("Linux i386 vDSO: unexpected relocation type %ld, "
			    "symbol index %ld\n", (intmax_t)rtype, (intmax_t)symidx);
		}
	}
}

static char GNU_ABI_VENDOR[] = "GNU";
static int GNULINUX_ABI_DESC = 0;

static bool
linux32_trans_osrel(const Elf_Note *note, int32_t *osrel)
{
	const Elf32_Word *desc;
	uintptr_t p;

	p = (uintptr_t)(note + 1);
	p += roundup2(note->n_namesz, sizeof(Elf32_Addr));

	desc = (const Elf32_Word *)p;
	if (desc[0] != GNULINUX_ABI_DESC)
		return (false);

	/*
	 * For Linux we encode osrel using the Linux convention of
	 * 	(version << 16) | (major << 8) | (minor)
	 * See macro in linux_mib.h
	 */
	*osrel = LINUX_KERNVER(desc[1], desc[2], desc[3]);

	return (true);
}

static Elf_Brandnote linux32_brandnote = {
	.hdr.n_namesz	= sizeof(GNU_ABI_VENDOR),
	.hdr.n_descsz	= 16,	/* XXX at least 16 */
	.hdr.n_type	= 1,
	.vendor		= GNU_ABI_VENDOR,
	.flags		= BN_TRANSLATE_OSREL,
	.trans_osrel	= linux32_trans_osrel
};

static Elf32_Brandinfo linux_brand = {
	.brand		= ELFOSABI_LINUX,
	.machine	= EM_386,
	.compat_3_brand	= "Linux",
	.emul_path	= linux_emul_path,
	.interp_path	= "/lib/ld-linux.so.1",
	.sysvec		= &elf_linux_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &linux32_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE
};

static Elf32_Brandinfo linux_glibc2brand = {
	.brand		= ELFOSABI_LINUX,
	.machine	= EM_386,
	.compat_3_brand	= "Linux",
	.emul_path	= linux_emul_path,
	.interp_path	= "/lib/ld-linux.so.2",
	.sysvec		= &elf_linux_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &linux32_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE
};

static Elf32_Brandinfo linux_muslbrand = {
	.brand		= ELFOSABI_LINUX,
	.machine	= EM_386,
	.compat_3_brand	= "Linux",
	.emul_path	= linux_emul_path,
	.interp_path	= "/lib/ld-musl-i386.so.1",
	.sysvec		= &elf_linux_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &linux32_brandnote,
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
				linux32_ioctl_register_handler(*lihp);
			stclohz = (stathz ? stathz : hz);
			if (bootverbose)
				printf("Linux i386 ELF exec handler installed\n");
		} else
			printf("cannot insert Linux i386 ELF brand handler\n");
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
				linux32_ioctl_unregister_handler(*lihp);
			if (bootverbose)
				printf("Linux i386 ELF exec handler removed\n");
		} else
			printf("Could not deinstall Linux i386 ELF interpreter entry\n");
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
MODULE_DEPEND(linuxelf, linux_common, 1, 1, 1);
FEATURE(linux, "Linux 32bit support");
