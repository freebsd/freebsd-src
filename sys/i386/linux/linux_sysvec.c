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
 *    derived from this software withough specific prior written permission
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
 *
 *  $Id: linux_sysvec.c,v 1.14 1997/05/07 20:05:45 peter Exp $
 */

/* XXX we use functions that might not exist. */
#define	COMPAT_43	1

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sysent.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>
#include <sys/exec.h>
#include <sys/kernel.h>
#include <machine/cpu.h>
#include <machine/psl.h>

#include <i386/linux/linux.h>
#include <i386/linux/linux_proto.h>

int	linux_fixup __P((int **stack_base, struct image_params *iparams));
int	elf_linux_fixup __P((int **stack_base, struct image_params *iparams));
void	linux_prepsyscall __P((struct trapframe *tf, int *args, u_int *code, caddr_t *params));
void    linux_sendsig __P((sig_t catcher, int sig, int mask, u_long code));
static void linux_elf_init __P((void *dummy));

/*
 * Linux syscalls return negative errno's, we do positive and map them
 */
int bsd_to_linux_errno[ELAST] = {
  	-0,  -1,  -2,  -3,  -4,  -5,  -6,  -7,  -8,  -9,
 	-10, -35, -12, -13, -14, -15, -16, -17, -18, -19,
 	-20, -21, -22, -23, -24, -25, -26, -27, -28, -29,
 	-30, -31, -32, -33, -34, -11,-115,-114, -88, -89,
 	-90, -91, -92, -93, -94, -95, -96, -97, -98, -99,
	-100,-101,-102,-103,-104,-105,-106,-107,-108,-109,
	-110,-111, -40, -36,-112,-113, -39, -11, -87,-122,
	-116, -66,  -6,  -6,  -6,  -6,  -6, -37, -38,  -9,
  	-6, 
};

int bsd_to_linux_signal[NSIG] = {
	0, LINUX_SIGHUP, LINUX_SIGINT, LINUX_SIGQUIT,
	LINUX_SIGILL, LINUX_SIGTRAP, LINUX_SIGABRT, 0,
	LINUX_SIGFPE, LINUX_SIGKILL, LINUX_SIGBUS, LINUX_SIGSEGV, 
	0, LINUX_SIGPIPE, LINUX_SIGALRM, LINUX_SIGTERM,
	LINUX_SIGURG, LINUX_SIGSTOP, LINUX_SIGTSTP, LINUX_SIGCONT,	
	LINUX_SIGCHLD, LINUX_SIGTTIN, LINUX_SIGTTOU, LINUX_SIGIO, 
	LINUX_SIGXCPU, LINUX_SIGXFSZ, LINUX_SIGVTALRM, LINUX_SIGPROF, 
	LINUX_SIGWINCH, 0, LINUX_SIGUSR1, LINUX_SIGUSR2
};

int linux_to_bsd_signal[LINUX_NSIG] = {
	0, SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT, SIGEMT,
	SIGFPE, SIGKILL, SIGUSR1, SIGSEGV, SIGUSR2, SIGPIPE, SIGALRM, SIGTERM, 
	SIGBUS, SIGCHLD, SIGCONT, SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU, SIGIO,
	SIGXCPU, SIGXFSZ, SIGVTALRM, SIGPROF, SIGWINCH, SIGURG, SIGURG, 0
};

int linux_fixup(int **stack_base, struct image_params *imgp)
{
	int *argv, *envp;

	argv = *stack_base;
	envp = *stack_base + (imgp->argc + 1);
	(*stack_base)--;
	**stack_base = (int)envp;
	(*stack_base)--;
	**stack_base = (int)argv;
	(*stack_base)--;
	**stack_base = (int)imgp->argc;
	return 0;
}

int elf_linux_fixup(int **stack_base, struct image_params *imgp)
{
	Elf32_Auxargs *args = (Elf32_Auxargs *)imgp->auxargs;
	int *pos;
             
	pos = *stack_base + (imgp->argc + imgp->envc + 2);  
    
	if (args->trace) {
		AUXARGS_ENTRY(pos, AT_DEBUG, 1);
	}
	if (args->execfd != -1) {
		AUXARGS_ENTRY(pos, AT_EXECFD, args->execfd);
	}       
	AUXARGS_ENTRY(pos, AT_PHDR, args->phdr);
	AUXARGS_ENTRY(pos, AT_PHENT, args->phent);
	AUXARGS_ENTRY(pos, AT_PHNUM, args->phnum);
	AUXARGS_ENTRY(pos, AT_PAGESZ, args->pagesz);
	AUXARGS_ENTRY(pos, AT_FLAGS, args->flags);
	AUXARGS_ENTRY(pos, AT_ENTRY, args->entry);
	AUXARGS_ENTRY(pos, AT_BASE, args->base);
	AUXARGS_ENTRY(pos, AT_UID, imgp->proc->p_cred->p_ruid);
	AUXARGS_ENTRY(pos, AT_EUID, imgp->proc->p_cred->p_svuid);
	AUXARGS_ENTRY(pos, AT_GID, imgp->proc->p_cred->p_rgid);
	AUXARGS_ENTRY(pos, AT_EGID, imgp->proc->p_cred->p_svgid);
	AUXARGS_ENTRY(pos, AT_NULL, 0);
	
	free(imgp->auxargs, M_TEMP);      
	imgp->auxargs = NULL;

	(*stack_base)--;
	**stack_base = (int)imgp->argc;
	return 0;
}

extern int _ucodesel, _udatasel;

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

void
linux_sendsig(sig_t catcher, int sig, int mask, u_long code)
{
	register struct proc *p = curproc;
	register struct trapframe *regs;
	struct linux_sigframe *fp, frame;
	struct sigacts *psp = p->p_sigacts;
	int oonstack;

	regs = p->p_md.md_regs;
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;

#ifdef DEBUG
	printf("Linux-emul(%d): linux_sendsig(%8x, %d, %d, %ld)\n",
		p->p_pid, catcher, sig, mask, code);
#endif
	/*
	 * Allocate space for the signal handler context.
	 */
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct linux_sigframe *)(psp->ps_sigstk.ss_sp +
		    psp->ps_sigstk.ss_size - sizeof(struct linux_sigframe));
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else {
		fp = (struct linux_sigframe *)regs->tf_esp - 1;
	}

	/*
	 * grow() will return FALSE if the fp will not fit inside the stack
	 *	and the stack can not be grown. useracc will return FALSE
	 *	if access is denied.
	 */
	if ((grow(p, (int)fp) == FALSE) ||
	    (useracc((caddr_t)fp, sizeof (struct linux_sigframe), B_WRITE) == FALSE)) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		SIGACTION(p, SIGILL) = SIG_DFL;
		sig = sigmask(SIGILL);
		p->p_sigignore &= ~sig;
		p->p_sigcatch &= ~sig;
		p->p_sigmask &= ~sig;
		psignal(p, SIGILL);
		return;
	}

	/*
	 * Build the argument list for the signal handler.
	 */
	if (p->p_sysent->sv_sigtbl) {
		if (sig < p->p_sysent->sv_sigsize)
			sig = p->p_sysent->sv_sigtbl[sig];
		else
			sig = p->p_sysent->sv_sigsize + 1;
	}

	frame.sf_handler = catcher;
	frame.sf_sig = sig;

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	frame.sf_sc.sc_mask   = mask;
	__asm("movl %%gs,%w0" : "=r" (frame.sf_sc.sc_gs));
	__asm("movl %%fs,%w0" : "=r" (frame.sf_sc.sc_fs));
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
	frame.sf_sc.sc_trapno = code;	/* XXX ???? */

	if (copyout(&frame, fp, sizeof(frame)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}

	/*
	 * Build context to run handler in.
	 */
	regs->tf_esp = (int)fp;
	regs->tf_eip = (int)(((char *)PS_STRINGS) - *(p->p_sysent->sv_szsigcode));
	regs->tf_eflags &= ~PSL_VM;
	regs->tf_cs = _ucodesel;
	regs->tf_ds = _udatasel;
	regs->tf_es = _udatasel;
	regs->tf_ss = _udatasel;
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
linux_sigreturn(p, args, retval)
	struct proc *p;
	struct linux_sigreturn_args *args;
	int *retval;
{
	struct linux_sigcontext *scp, context;
	register struct trapframe *regs;
	int eflags;

	regs = p->p_md.md_regs;

#ifdef DEBUG
	printf("Linux-emul(%d): linux_sigreturn(%8x)\n", p->p_pid, args->scp);
#endif
	/*
	 * The trampoline code hands us the context.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	scp = args->scp;
	if (copyin((caddr_t)scp, &context, sizeof(*scp)) != 0)
		return (EFAULT);

	/*
	 * Check for security violations.
	 */
#define	EFLAGS_SECURE(ef, oef)	((((ef) ^ (oef)) & ~PSL_USERCHANGE) == 0)
	eflags = context.sc_eflags;
	/*
	 * XXX do allow users to change the privileged flag PSL_RF.  The
	 * cpu sets PSL_RF in tf_eflags for faults.  Debuggers should
	 * sometimes set it there too.  tf_eflags is kept in the signal
	 * context during signal handling and there is no other place
	 * to remember it, so the PSL_RF bit may be corrupted by the
	 * signal handler without us knowing.  Corruption of the PSL_RF
	 * bit at worst causes one more or one less debugger trap, so
	 * allowing it is fairly harmless.
	 */
	if (!EFLAGS_SECURE(eflags & ~PSL_RF, regs->tf_eflags & ~PSL_RF)) {
    		return(EINVAL);
	}

	/*
	 * Don't allow users to load a valid privileged %cs.  Let the
	 * hardware check for invalid selectors, excess privilege in
	 * other selectors, invalid %eip's and invalid %esp's.
	 */
#define	CS_SECURE(cs)	(ISPL(cs) == SEL_UPL)
	if (!CS_SECURE(context.sc_cs)) {
		trapsignal(p, SIGBUS, T_PROTFLT);
		return(EINVAL);
	}

	p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = context.sc_mask &~
		(sigmask(SIGKILL)|sigmask(SIGCONT)|sigmask(SIGSTOP));
	/*
	 * Restore signal context.
	 */
	/* %fs and %gs were restored by the trampoline. */
	regs->tf_es     = context.sc_es;
	regs->tf_ds     = context.sc_ds;
	regs->tf_edi    = context.sc_edi;
	regs->tf_esi    = context.sc_esi;
	regs->tf_ebp    = context.sc_ebp;
	regs->tf_ebx    = context.sc_ebx;
	regs->tf_edx    = context.sc_edx;
	regs->tf_ecx    = context.sc_ecx;
	regs->tf_eax    = context.sc_eax;
	regs->tf_eip    = context.sc_eip;
	regs->tf_cs     = context.sc_cs;
	regs->tf_eflags = eflags;
	regs->tf_esp    = context.sc_esp_at_signal;
	regs->tf_ss     = context.sc_ss;

	return (EJUSTRETURN);
}

void
linux_prepsyscall(struct trapframe *tf, int *args, u_int *code, caddr_t *params)
{
	args[0] = tf->tf_ebx;
	args[1] = tf->tf_ecx;
	args[2] = tf->tf_edx;
	args[3] = tf->tf_esi;
	args[4] = tf->tf_edi;
	*params = NULL;		/* no copyin */
}

struct sysentvec linux_sysvec = {
	LINUX_SYS_MAXSYSCALL,
	linux_sysent,
	0xff,
	NSIG,
	bsd_to_linux_signal,
	ELAST, 
	bsd_to_linux_errno,
	linux_fixup,
	linux_sendsig,
	linux_sigcode,	
	&linux_szsigcode,
	linux_prepsyscall,
	"Linux a.out"
};

struct sysentvec elf_linux_sysvec = {
        LINUX_SYS_MAXSYSCALL,
        linux_sysent,
        0xff,
        NSIG,
        bsd_to_linux_signal,
        ELAST,
        bsd_to_linux_errno,
        elf_linux_fixup,
        linux_sendsig,
        linux_sigcode,
        &linux_szsigcode,
        linux_prepsyscall,
	"Linux ELF"
};

/*
 * Installed either via SYSINIT() or via LKM stubs.
 */
Elf32_Brandinfo linux_brand = {
					"Linux",
					"/compat/linux",
					"/lib/ld-linux.so.1",
					&elf_linux_sysvec
				 };

#ifndef LKM
/*
 * XXX: this is WRONG, it needs to be SI_SUB_EXEC, but this is just at the
 * "proof of concept" stage and will be fixed shortly
 */
static void
linux_elf_init(dummy)
	void *dummy;
{
	if (elf_insert_brand_entry(&linux_brand) < 0)
		printf("cannot insert Linux elf brand handler\n");
	else if (bootverbose)
		printf("Linux-ELF exec handler installed\n");
}

SYSINIT(linuxelf, SI_SUB_VFS, SI_ORDER_ANY, linux_elf_init, NULL);
#endif
