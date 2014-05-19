/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
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
#include <sys/signalvar.h>
#include <sys/syscallsubr.h>
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
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/pcb.h>

#include <i386/linux/linux.h>
#include <i386/linux/linux_proto.h>
#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_futex.h>
#include <compat/linux/linux_ioctl.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_util.h>

MODULE_VERSION(linux, 1);

MALLOC_DEFINE(M_LINUX, "linux", "Linux mode structures");

#if BYTE_ORDER == LITTLE_ENDIAN
#define SHELLMAGIC      0x2123 /* #! */
#else
#define SHELLMAGIC      0x2321
#endif

/*
 * Allow the sendsig functions to use the ldebug() facility
 * even though they are not syscalls themselves. Map them
 * to syscall 0. This is slightly less bogus than using
 * ldebug(sigreturn).
 */
#define	LINUX_SYS_linux_rt_sendsig	0
#define	LINUX_SYS_linux_sendsig		0

#define	LINUX_PS_STRINGS	(LINUX_USRSTACK - sizeof(struct ps_strings))

extern char linux_sigcode[];
extern int linux_szsigcode;

extern struct sysent linux_sysent[LINUX_SYS_MAXSYSCALL];

SET_DECLARE(linux_ioctl_handler_set, struct linux_ioctl_handler);
SET_DECLARE(linux_device_handler_set, struct linux_device_handler);

static int	linux_fixup(register_t **stack_base,
		    struct image_params *iparams);
static int	elf_linux_fixup(register_t **stack_base,
		    struct image_params *iparams);
static void     linux_sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask);
static void	exec_linux_setregs(struct thread *td,
		    struct image_params *imgp, u_long stack);
static register_t *linux_copyout_strings(struct image_params *imgp);
static boolean_t linux_trans_osrel(const Elf_Note *note, int32_t *osrel);

static int linux_szplatform;
const char *linux_platform;

static eventhandler_tag linux_exit_tag;
static eventhandler_tag linux_exec_tag;

/*
 * Linux syscalls return negative errno's, we do positive and map them
 * Reference:
 *   FreeBSD: src/sys/sys/errno.h
 *   Linux:   linux-2.6.17.8/include/asm-generic/errno-base.h
 *            linux-2.6.17.8/include/asm-generic/errno.h
 */
static int bsd_to_linux_errno[ELAST + 1] = {
	-0,  -1,  -2,  -3,  -4,  -5,  -6,  -7,  -8,  -9,
	-10, -35, -12, -13, -14, -15, -16, -17, -18, -19,
	-20, -21, -22, -23, -24, -25, -26, -27, -28, -29,
	-30, -31, -32, -33, -34, -11,-115,-114, -88, -89,
	-90, -91, -92, -93, -94, -95, -96, -97, -98, -99,
	-100,-101,-102,-103,-104,-105,-106,-107,-108,-109,
	-110,-111, -40, -36,-112,-113, -39, -11, -87,-122,
	-116, -66,  -6,  -6,  -6,  -6,  -6, -37, -38,  -9,
	  -6,  -6, -43, -42, -75,-125, -84, -95, -16, -74,
	 -72, -67, -71
};

int bsd_to_linux_signal[LINUX_SIGTBLSZ] = {
	LINUX_SIGHUP, LINUX_SIGINT, LINUX_SIGQUIT, LINUX_SIGILL,
	LINUX_SIGTRAP, LINUX_SIGABRT, 0, LINUX_SIGFPE,
	LINUX_SIGKILL, LINUX_SIGBUS, LINUX_SIGSEGV, LINUX_SIGSYS,
	LINUX_SIGPIPE, LINUX_SIGALRM, LINUX_SIGTERM, LINUX_SIGURG,
	LINUX_SIGSTOP, LINUX_SIGTSTP, LINUX_SIGCONT, LINUX_SIGCHLD,
	LINUX_SIGTTIN, LINUX_SIGTTOU, LINUX_SIGIO, LINUX_SIGXCPU,
	LINUX_SIGXFSZ, LINUX_SIGVTALRM, LINUX_SIGPROF, LINUX_SIGWINCH,
	0, LINUX_SIGUSR1, LINUX_SIGUSR2
};

int linux_to_bsd_signal[LINUX_SIGTBLSZ] = {
	SIGHUP, SIGINT, SIGQUIT, SIGILL,
	SIGTRAP, SIGABRT, SIGBUS, SIGFPE,
	SIGKILL, SIGUSR1, SIGSEGV, SIGUSR2,
	SIGPIPE, SIGALRM, SIGTERM, SIGBUS,
	SIGCHLD, SIGCONT, SIGSTOP, SIGTSTP,
	SIGTTIN, SIGTTOU, SIGURG, SIGXCPU,
	SIGXFSZ, SIGVTALRM, SIGPROF, SIGWINCH,
	SIGIO, SIGURG, SIGSYS
};

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
    ((code)<sizeof(_bsd_to_linux_trapcode)/sizeof(*_bsd_to_linux_trapcode)? \
     _bsd_to_linux_trapcode[(code)]: \
     LINUX_T_UNKNOWN)

/*
 * If FreeBSD & Linux have a difference of opinion about what a trap
 * means, deal with it here.
 *
 * MPSAFE
 */
static int
translate_traps(int signal, int trap_code)
{
	if (signal != SIGBUS)
		return signal;
	switch (trap_code) {
	case T_PROTFLT:
	case T_TSSFLT:
	case T_DOUBLEFLT:
	case T_PAGEFLT:
		return SIGSEGV;
	default:
		return signal;
	}
}

static int
linux_fixup(register_t **stack_base, struct image_params *imgp)
{
	register_t *argv, *envp;

	argv = *stack_base;
	envp = *stack_base + (imgp->args->argc + 1);
	(*stack_base)--;
	suword(*stack_base, (intptr_t)(void *)envp);
	(*stack_base)--;
	suword(*stack_base, (intptr_t)(void *)argv);
	(*stack_base)--;
	suword(*stack_base, imgp->args->argc);
	return (0);
}

static int
elf_linux_fixup(register_t **stack_base, struct image_params *imgp)
{
	struct proc *p;
	Elf32_Auxargs *args;
	Elf32_Addr *uplatform;
	struct ps_strings *arginfo;
	register_t *pos;

	KASSERT(curthread->td_proc == imgp->proc,
	    ("unsafe elf_linux_fixup(), should be curproc"));

	p = imgp->proc;
	arginfo = (struct ps_strings *)p->p_sysent->sv_psstrings;
	uplatform = (Elf32_Addr *)((caddr_t)arginfo - linux_szplatform);
	args = (Elf32_Auxargs *)imgp->auxargs;
	pos = *stack_base + (imgp->args->argc + imgp->args->envc + 2);

	AUXARGS_ENTRY(pos, LINUX_AT_HWCAP, cpu_feature);

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
	AUXARGS_ENTRY(pos, AT_PAGESZ, args->pagesz);
	AUXARGS_ENTRY(pos, AT_FLAGS, args->flags);
	AUXARGS_ENTRY(pos, AT_ENTRY, args->entry);
	AUXARGS_ENTRY(pos, AT_BASE, args->base);
	AUXARGS_ENTRY(pos, LINUX_AT_SECURE, 0);
	AUXARGS_ENTRY(pos, AT_UID, imgp->proc->p_ucred->cr_ruid);
	AUXARGS_ENTRY(pos, AT_EUID, imgp->proc->p_ucred->cr_svuid);
	AUXARGS_ENTRY(pos, AT_GID, imgp->proc->p_ucred->cr_rgid);
	AUXARGS_ENTRY(pos, AT_EGID, imgp->proc->p_ucred->cr_svgid);
	AUXARGS_ENTRY(pos, LINUX_AT_PLATFORM, PTROUT(uplatform));
	if (args->execfd != -1)
		AUXARGS_ENTRY(pos, AT_EXECFD, args->execfd);
	AUXARGS_ENTRY(pos, AT_NULL, 0);

	free(imgp->auxargs, M_TEMP);
	imgp->auxargs = NULL;

	(*stack_base)--;
	suword(*stack_base, (register_t)imgp->args->argc);
	return (0);
}

/*
 * Copied from kern/kern_exec.c
 */
static register_t *
linux_copyout_strings(struct image_params *imgp)
{
	int argc, envc;
	char **vectp;
	char *stringp, *destp;
	register_t *stack_base;
	struct ps_strings *arginfo;
	struct proc *p;

	/*
	 * Calculate string base and vector table pointers.
	 * Also deal with signal trampoline code for this exec type.
	 */
	p = imgp->proc;
	arginfo = (struct ps_strings *)p->p_sysent->sv_psstrings;
	destp = (caddr_t)arginfo - SPARE_USRSPACE - linux_szplatform -
	    roundup((ARG_MAX - imgp->args->stringspace), sizeof(char *));

	/*
	 * install LINUX_PLATFORM
	 */
	copyout(linux_platform, ((caddr_t)arginfo - linux_szplatform),
	    linux_szplatform);

	/*
	 * If we have a valid auxargs ptr, prepare some room
	 * on the stack.
	 */
	if (imgp->auxargs) {
		/*
		 * 'AT_COUNT*2' is size for the ELF Auxargs data. This is for
		 * lower compatibility.
		 */
		imgp->auxarg_size = (imgp->auxarg_size) ? imgp->auxarg_size :
		    (LINUX_AT_COUNT * 2);
		/*
		 * The '+ 2' is for the null pointers at the end of each of
		 * the arg and env vector sets,and imgp->auxarg_size is room
		 * for argument of Runtime loader.
		 */
		vectp = (char **)(destp - (imgp->args->argc +
		    imgp->args->envc + 2 + imgp->auxarg_size) * sizeof(char *));
	} else {
		/*
		 * The '+ 2' is for the null pointers at the end of each of
		 * the arg and env vector sets
		 */
		vectp = (char **)(destp - (imgp->args->argc + imgp->args->envc + 2) *
		    sizeof(char *));
	}

	/*
	 * vectp also becomes our initial stack base
	 */
	stack_base = (register_t *)vectp;

	stringp = imgp->args->begin_argv;
	argc = imgp->args->argc;
	envc = imgp->args->envc;

	/*
	 * Copy out strings - arguments and environment.
	 */
	copyout(stringp, destp, ARG_MAX - imgp->args->stringspace);

	/*
	 * Fill in "ps_strings" struct for ps, w, etc.
	 */
	suword(&arginfo->ps_argvstr, (long)(intptr_t)vectp);
	suword(&arginfo->ps_nargvstr, argc);

	/*
	 * Fill in argument portion of vector table.
	 */
	for (; argc > 0; --argc) {
		suword(vectp++, (long)(intptr_t)destp);
		while (*stringp++ != 0)
			destp++;
		destp++;
	}

	/* a null vector table pointer separates the argp's from the envp's */
	suword(vectp++, 0);

	suword(&arginfo->ps_envstr, (long)(intptr_t)vectp);
	suword(&arginfo->ps_nenvstr, envc);

	/*
	 * Fill in environment portion of vector table.
	 */
	for (; envc > 0; --envc) {
		suword(vectp++, (long)(intptr_t)destp);
		while (*stringp++ != 0)
			destp++;
		destp++;
	}

	/* end of vector table is a null pointer */
	suword(vectp, 0);

	return (stack_base);
}



extern int _ucodesel, _udatasel;
extern unsigned long linux_sznonrtsigcode;

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

	sig = ksi->ksi_signo;
	code = ksi->ksi_code;	
	PROC_LOCK_ASSERT(p, MA_OWNED);
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	regs = td->td_frame;
	oonstack = sigonstack(regs->tf_esp);

#ifdef DEBUG
	if (ldebug(rt_sendsig))
		printf(ARGS(rt_sendsig, "%p, %d, %p, %u"),
		    catcher, sig, (void*)mask, code);
#endif
	/*
	 * Allocate space for the signal handler context.
	 */
	if ((td->td_pflags & TDP_ALTSTACK) && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		fp = (struct l_rt_sigframe *)(td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size - sizeof(struct l_rt_sigframe));
	} else
		fp = (struct l_rt_sigframe *)regs->tf_esp - 1;
	mtx_unlock(&psp->ps_mtx);

	/*
	 * Build the argument list for the signal handler.
	 */
	if (p->p_sysent->sv_sigtbl)
		if (sig <= p->p_sysent->sv_sigsize)
			sig = p->p_sysent->sv_sigtbl[_SIG_IDX(sig)];

	bzero(&frame, sizeof(frame));

	frame.sf_handler = catcher;
	frame.sf_sig = sig;
	frame.sf_siginfo = &fp->sf_si;
	frame.sf_ucontext = &fp->sf_sc;

	/* Fill in POSIX parts */
	ksiginfo_to_lsiginfo(ksi, &frame.sf_si, sig);

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	frame.sf_sc.uc_flags = 0;		/* XXX ??? */
	frame.sf_sc.uc_link = NULL;		/* XXX ??? */

	frame.sf_sc.uc_stack.ss_sp = td->td_sigstk.ss_sp;
	frame.sf_sc.uc_stack.ss_size = td->td_sigstk.ss_size;
	frame.sf_sc.uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK)
	    ? ((oonstack) ? LINUX_SS_ONSTACK : 0) : LINUX_SS_DISABLE;
	PROC_UNLOCK(p);

	bsd_to_linux_sigset(mask, &frame.sf_sc.uc_sigmask);

	frame.sf_sc.uc_mcontext.sc_mask   = frame.sf_sc.uc_sigmask.__bits[0];
	frame.sf_sc.uc_mcontext.sc_gs     = rgs();
	frame.sf_sc.uc_mcontext.sc_fs     = regs->tf_fs;
	frame.sf_sc.uc_mcontext.sc_es     = regs->tf_es;
	frame.sf_sc.uc_mcontext.sc_ds     = regs->tf_ds;
	frame.sf_sc.uc_mcontext.sc_edi    = regs->tf_edi;
	frame.sf_sc.uc_mcontext.sc_esi    = regs->tf_esi;
	frame.sf_sc.uc_mcontext.sc_ebp    = regs->tf_ebp;
	frame.sf_sc.uc_mcontext.sc_ebx    = regs->tf_ebx;
	frame.sf_sc.uc_mcontext.sc_edx    = regs->tf_edx;
	frame.sf_sc.uc_mcontext.sc_ecx    = regs->tf_ecx;
	frame.sf_sc.uc_mcontext.sc_eax    = regs->tf_eax;
	frame.sf_sc.uc_mcontext.sc_eip    = regs->tf_eip;
	frame.sf_sc.uc_mcontext.sc_cs     = regs->tf_cs;
	frame.sf_sc.uc_mcontext.sc_eflags = regs->tf_eflags;
	frame.sf_sc.uc_mcontext.sc_esp_at_signal = regs->tf_esp;
	frame.sf_sc.uc_mcontext.sc_ss     = regs->tf_ss;
	frame.sf_sc.uc_mcontext.sc_err    = regs->tf_err;
	frame.sf_sc.uc_mcontext.sc_cr2    = (register_t)ksi->ksi_addr;
	frame.sf_sc.uc_mcontext.sc_trapno = bsd_to_linux_trapcode(code);

#ifdef DEBUG
	if (ldebug(rt_sendsig))
		printf(LMSG("rt_sendsig flags: 0x%x, sp: %p, ss: 0x%x, mask: 0x%x"),
		    frame.sf_sc.uc_stack.ss_flags, td->td_sigstk.ss_sp,
		    td->td_sigstk.ss_size, frame.sf_sc.uc_mcontext.sc_mask);
#endif

	if (copyout(&frame, fp, sizeof(frame)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
#ifdef DEBUG
		if (ldebug(rt_sendsig))
			printf(LMSG("rt_sendsig: bad stack %p, oonstack=%x"),
			    fp, oonstack);
#endif
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	/*
	 * Build context to run handler in.
	 */
	regs->tf_esp = (int)fp;
	regs->tf_eip = p->p_sysent->sv_sigcode_base + linux_sznonrtsigcode;
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
	int sig, code;
	int oonstack, i;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	psp = p->p_sigacts;
	sig = ksi->ksi_signo;
	code = ksi->ksi_code;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	if (SIGISMEMBER(psp->ps_siginfo, sig)) {
		/* Signal handler installed with SA_SIGINFO. */
		linux_rt_sendsig(catcher, ksi, mask);
		return;
	}
	regs = td->td_frame;
	oonstack = sigonstack(regs->tf_esp);

#ifdef DEBUG
	if (ldebug(sendsig))
		printf(ARGS(sendsig, "%p, %d, %p, %u"),
		    catcher, sig, (void*)mask, code);
#endif

	/*
	 * Allocate space for the signal handler context.
	 */
	if ((td->td_pflags & TDP_ALTSTACK) && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		fp = (struct l_sigframe *)(td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size - sizeof(struct l_sigframe));
	} else
		fp = (struct l_sigframe *)regs->tf_esp - 1;
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	/*
	 * Build the argument list for the signal handler.
	 */
	if (p->p_sysent->sv_sigtbl)
		if (sig <= p->p_sysent->sv_sigsize)
			sig = p->p_sysent->sv_sigtbl[_SIG_IDX(sig)];

	bzero(&frame, sizeof(frame));

	frame.sf_handler = catcher;
	frame.sf_sig = sig;

	bsd_to_linux_sigset(mask, &lmask);

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	frame.sf_sc.sc_mask   = lmask.__bits[0];
	frame.sf_sc.sc_gs     = rgs();
	frame.sf_sc.sc_fs     = regs->tf_fs;
	frame.sf_sc.sc_es     = regs->tf_es;
	frame.sf_sc.sc_ds     = regs->tf_ds;
	frame.sf_sc.sc_edi    = regs->tf_edi;
	frame.sf_sc.sc_esi    = regs->tf_esi;
	frame.sf_sc.sc_ebp    = regs->tf_ebp;
	frame.sf_sc.sc_ebx    = regs->tf_ebx;
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

	for (i = 0; i < (LINUX_NSIG_WORDS-1); i++)
		frame.sf_extramask[i] = lmask.__bits[i+1];

	if (copyout(&frame, fp, sizeof(frame)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	/*
	 * Build context to run handler in.
	 */
	regs->tf_esp = (int)fp;
	regs->tf_eip = p->p_sysent->sv_sigcode_base;
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
	l_sigset_t lmask;
	sigset_t bmask;
	int eflags, i;
	ksiginfo_t ksi;

	regs = td->td_frame;

#ifdef DEBUG
	if (ldebug(sigreturn))
		printf(ARGS(sigreturn, "%p"), (void *)args->sfp);
#endif
	/*
	 * The trampoline code hands us the sigframe.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	if (copyin(args->sfp, &frame, sizeof(frame)) != 0)
		return (EFAULT);

	/*
	 * Check for security violations.
	 */
#define	EFLAGS_SECURE(ef, oef)	((((ef) ^ (oef)) & ~PSL_USERCHANGE) == 0)
	eflags = frame.sf_sc.sc_eflags;
	if (!EFLAGS_SECURE(eflags, regs->tf_eflags))
		return(EINVAL);

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
		return(EINVAL);
	}

	lmask.__bits[0] = frame.sf_sc.sc_mask;
	for (i = 0; i < (LINUX_NSIG_WORDS-1); i++)
		lmask.__bits[i+1] = frame.sf_extramask[i];
	linux_to_bsd_sigset(&lmask, &bmask);
	kern_sigprocmask(td, SIG_SETMASK, &bmask, NULL, 0);

	/*
	 * Restore signal context.
	 */
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

#ifdef DEBUG
	if (ldebug(rt_sigreturn))
		printf(ARGS(rt_sigreturn, "%p"), (void *)args->ucp);
#endif
	/*
	 * The trampoline code hands us the ucontext.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	if (copyin(args->ucp, &uc, sizeof(uc)) != 0)
		return (EFAULT);

	context = &uc.uc_mcontext;

	/*
	 * Check for security violations.
	 */
#define	EFLAGS_SECURE(ef, oef)	((((ef) ^ (oef)) & ~PSL_USERCHANGE) == 0)
	eflags = context->sc_eflags;
	if (!EFLAGS_SECURE(eflags, regs->tf_eflags))
		return(EINVAL);

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
		return(EINVAL);
	}

	linux_to_bsd_sigset(&uc.uc_sigmask, &bmask);
	kern_sigprocmask(td, SIG_SETMASK, &bmask, NULL, 0);

	/*
	 * Restore signal context
	 */
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

	/*
	 * call sigaltstack & ignore results..
	 */
	lss = &uc.uc_stack;
	ss.ss_sp = lss->ss_sp;
	ss.ss_size = lss->ss_size;
	ss.ss_flags = linux_to_bsd_sigaltstack(lss->ss_flags);

#ifdef DEBUG
	if (ldebug(rt_sigreturn))
		printf(LMSG("rt_sigret flags: 0x%x, sp: %p, ss: 0x%x, mask: 0x%x"),
		    ss.ss_flags, ss.ss_sp, ss.ss_size, context->sc_mask);
#endif
	(void)kern_sigaltstack(td, &ss, NULL);

	return (EJUSTRETURN);
}

static int
linux_fetch_syscall_args(struct thread *td, struct syscall_args *sa)
{
	struct proc *p;
	struct trapframe *frame;

	p = td->td_proc;
	frame = td->td_frame;

	sa->code = frame->tf_eax;
	sa->args[0] = frame->tf_ebx;
	sa->args[1] = frame->tf_ecx;
	sa->args[2] = frame->tf_edx;
	sa->args[3] = frame->tf_esi;
	sa->args[4] = frame->tf_edi;
	sa->args[5] = frame->tf_ebp;	/* Unconfirmed */

	if (sa->code >= p->p_sysent->sv_size)
		sa->callp = &p->p_sysent->sv_table[0];
 	else
 		sa->callp = &p->p_sysent->sv_table[sa->code];
	sa->narg = sa->callp->sy_narg;

	td->td_retval[0] = 0;
	td->td_retval[1] = frame->tf_edx;

	return (0);
}

/*
 * If a linux binary is exec'ing something, try this image activator
 * first.  We override standard shell script execution in order to
 * be able to modify the interpreter path.  We only do this if a linux
 * binary is doing the exec, so we do not create an EXEC module for it.
 */
static int	exec_linux_imgact_try(struct image_params *iparams);

static int
exec_linux_imgact_try(struct image_params *imgp)
{
    const char *head = (const char *)imgp->image_header;
    char *rpath;
    int error = -1;

    /*
     * The interpreter for shell scripts run from a linux binary needs
     * to be located in /compat/linux if possible in order to recursively
     * maintain linux path emulation.
     */
    if (((const short *)head)[0] == SHELLMAGIC) {
	    /*
	     * Run our normal shell image activator.  If it succeeds attempt
	     * to use the alternate path for the interpreter.  If an alternate
	     * path is found, use our stringspace to store it.
	     */
	    if ((error = exec_shell_imgact(imgp)) == 0) {
		    linux_emul_convpath(FIRST_THREAD_IN_PROC(imgp->proc),
			imgp->interpreter_name, UIO_SYSSPACE, &rpath, 0, AT_FDCWD);
		    if (rpath != NULL)
			    imgp->args->fname_buf =
				imgp->interpreter_name = rpath;
	    }
    }
    return (error);
}

/*
 * exec_setregs may initialize some registers differently than Linux
 * does, thus potentially confusing Linux binaries. If necessary, we
 * override the exec_setregs default(s) here.
 */
static void
exec_linux_setregs(struct thread *td, struct image_params *imgp, u_long stack)
{
	struct pcb *pcb = td->td_pcb;

	exec_setregs(td, imgp, stack);

	/* Linux sets %gs to 0, we default to _udatasel */
	pcb->pcb_gs = 0;
	load_gs(0);

	pcb->pcb_initial_npxcw = __LINUX_NPXCW__;
}

static void
linux_get_machine(const char **dst)
{

	switch (cpu_class) {
	case CPUCLASS_686:
		*dst = "i686";
		break;
	case CPUCLASS_586:
		*dst = "i586";
		break;
	case CPUCLASS_486:
		*dst = "i486";
		break;
	default:
		*dst = "i386";
	}
}

struct sysentvec linux_sysvec = {
	.sv_size	= LINUX_SYS_MAXSYSCALL,
	.sv_table	= linux_sysent,
	.sv_mask	= 0,
	.sv_sigsize	= LINUX_SIGTBLSZ,
	.sv_sigtbl	= bsd_to_linux_signal,
	.sv_errsize	= ELAST + 1,
	.sv_errtbl	= bsd_to_linux_errno,
	.sv_transtrap	= translate_traps,
	.sv_fixup	= linux_fixup,
	.sv_sendsig	= linux_sendsig,
	.sv_sigcode	= linux_sigcode,
	.sv_szsigcode	= &linux_szsigcode,
	.sv_prepsyscall	= NULL,
	.sv_name	= "Linux a.out",
	.sv_coredump	= NULL,
	.sv_imgact_try	= exec_linux_imgact_try,
	.sv_minsigstksz	= LINUX_MINSIGSTKSZ,
	.sv_pagesize	= PAGE_SIZE,
	.sv_minuser	= VM_MIN_ADDRESS,
	.sv_maxuser	= VM_MAXUSER_ADDRESS,
	.sv_usrstack	= LINUX_USRSTACK,
	.sv_psstrings	= PS_STRINGS,
	.sv_stackprot	= VM_PROT_ALL,
	.sv_copyout_strings = exec_copyout_strings,
	.sv_setregs	= exec_linux_setregs,
	.sv_fixlimit	= NULL,
	.sv_maxssiz	= NULL,
	.sv_flags	= SV_ABI_LINUX | SV_AOUT | SV_IA32 | SV_ILP32,
	.sv_set_syscall_retval = cpu_set_syscall_retval,
	.sv_fetch_syscall_args = linux_fetch_syscall_args,
	.sv_syscallnames = NULL,
	.sv_shared_page_base = LINUX_SHAREDPAGE,
	.sv_shared_page_len = PAGE_SIZE,
	.sv_schedtail	= linux_schedtail,
};
INIT_SYSENTVEC(aout_sysvec, &linux_sysvec);

struct sysentvec elf_linux_sysvec = {
	.sv_size	= LINUX_SYS_MAXSYSCALL,
	.sv_table	= linux_sysent,
	.sv_mask	= 0,
	.sv_sigsize	= LINUX_SIGTBLSZ,
	.sv_sigtbl	= bsd_to_linux_signal,
	.sv_errsize	= ELAST + 1,
	.sv_errtbl	= bsd_to_linux_errno,
	.sv_transtrap	= translate_traps,
	.sv_fixup	= elf_linux_fixup,
	.sv_sendsig	= linux_sendsig,
	.sv_sigcode	= linux_sigcode,
	.sv_szsigcode	= &linux_szsigcode,
	.sv_prepsyscall	= NULL,
	.sv_name	= "Linux ELF",
	.sv_coredump	= elf32_coredump,
	.sv_imgact_try	= exec_linux_imgact_try,
	.sv_minsigstksz	= LINUX_MINSIGSTKSZ,
	.sv_pagesize	= PAGE_SIZE,
	.sv_minuser	= VM_MIN_ADDRESS,
	.sv_maxuser	= VM_MAXUSER_ADDRESS,
	.sv_usrstack	= LINUX_USRSTACK,
	.sv_psstrings	= LINUX_PS_STRINGS,
	.sv_stackprot	= VM_PROT_ALL,
	.sv_copyout_strings = linux_copyout_strings,
	.sv_setregs	= exec_linux_setregs,
	.sv_fixlimit	= NULL,
	.sv_maxssiz	= NULL,
	.sv_flags	= SV_ABI_LINUX | SV_IA32 | SV_ILP32 | SV_SHP,
	.sv_set_syscall_retval = cpu_set_syscall_retval,
	.sv_fetch_syscall_args = linux_fetch_syscall_args,
	.sv_syscallnames = NULL,
	.sv_shared_page_base = LINUX_SHAREDPAGE,
	.sv_shared_page_len = PAGE_SIZE,
	.sv_schedtail	= linux_schedtail,
};
INIT_SYSENTVEC(elf_sysvec, &elf_linux_sysvec);

static char GNU_ABI_VENDOR[] = "GNU";
static int GNULINUX_ABI_DESC = 0;

static boolean_t
linux_trans_osrel(const Elf_Note *note, int32_t *osrel)
{
	const Elf32_Word *desc;
	uintptr_t p;

	p = (uintptr_t)(note + 1);
	p += roundup2(note->n_namesz, sizeof(Elf32_Addr));

	desc = (const Elf32_Word *)p;
	if (desc[0] != GNULINUX_ABI_DESC)
		return (FALSE);

	/*
	 * For linux we encode osrel as follows (see linux_mib.c):
	 * VVVMMMIII (version, major, minor), see linux_mib.c.
	 */
	*osrel = desc[1] * 1000000 + desc[2] * 1000 + desc[3];

	return (TRUE);
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
	.emul_path	= "/compat/linux",
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
	.emul_path	= "/compat/linux",
	.interp_path	= "/lib/ld-linux.so.2",
	.sysvec		= &elf_linux_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &linux_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE
};

Elf32_Brandinfo *linux_brandlist[] = {
	&linux_brand,
	&linux_glibc2brand,
	NULL
};

static int
linux_elf_modevent(module_t mod, int type, void *data)
{
	Elf32_Brandinfo **brandinfo;
	int error;
	struct linux_ioctl_handler **lihp;
	struct linux_device_handler **ldhp;

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
			SET_FOREACH(ldhp, linux_device_handler_set)
				linux_device_register_handler(*ldhp);
			mtx_init(&emul_lock, "emuldata lock", NULL, MTX_DEF);
			sx_init(&emul_shared_lock, "emuldata->shared lock");
			LIST_INIT(&futex_list);
			mtx_init(&futex_mtx, "ftllk", NULL, MTX_DEF);
			linux_exit_tag = EVENTHANDLER_REGISTER(process_exit, linux_proc_exit,
			      NULL, 1000);
			linux_exec_tag = EVENTHANDLER_REGISTER(process_exec, linux_proc_exec,
			      NULL, 1000);
			linux_get_machine(&linux_platform);
			linux_szplatform = roundup(strlen(linux_platform) + 1,
			    sizeof(char *));
			linux_osd_jail_register();
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
			SET_FOREACH(ldhp, linux_device_handler_set)
				linux_device_unregister_handler(*ldhp);
			mtx_destroy(&emul_lock);
			sx_destroy(&emul_shared_lock);
			mtx_destroy(&futex_mtx);
			EVENTHANDLER_DEREGISTER(process_exit, linux_exit_tag);
			EVENTHANDLER_DEREGISTER(process_exec, linux_exec_tag);
			linux_osd_jail_deregister();
			if (bootverbose)
				printf("Linux ELF exec handler removed\n");
		} else
			printf("Could not deinstall ELF interpreter entry\n");
		break;
	default:
		return EOPNOTSUPP;
	}
	return error;
}

static moduledata_t linux_elf_mod = {
	"linuxelf",
	linux_elf_modevent,
	0
};

DECLARE_MODULE_TIED(linuxelf, linux_elf_mod, SI_SUB_EXEC, SI_ORDER_ANY);
