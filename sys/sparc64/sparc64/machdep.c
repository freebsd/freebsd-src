/*-
 * Copyright (c) 2001 Jake Burkholder.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 * 	from: FreeBSD: src/sys/i386/i386/machdep.c,v 1.477 2001/08/27
 * $FreeBSD$
 */

#include "opt_ddb.h"
#include "opt_msgbuf.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/interrupt.h>
#include <sys/ptrace.h>
#include <sys/signalvar.h>
#include <sys/smp.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/timetc.h>
#include <sys/user.h>
#include <sys/ucontext.h>
#include <sys/user.h>
#include <sys/ucontext.h>
#include <sys/exec.h>

#include <dev/ofw/openfirm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_pager.h>
#include <vm/vm_extern.h>

#include <ddb/ddb.h>

#include <machine/cache.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/fp.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/ofw_machdep.h>
#include <machine/smp.h>
#include <machine/pmap.h>
#include <machine/pstate.h>
#include <machine/reg.h>
#include <machine/sigframe.h>
#include <machine/tick.h>
#include <machine/tlb.h>
#include <machine/tstate.h>
#include <machine/upa.h>
#include <machine/ver.h>

typedef int ofw_vec_t(void *);

struct tte *kernel_ttes;
int kernel_tlb_slots;

int physmem;
int cold = 1;
long dumplo;
int Maxmem;

char pcpu0[PCPU_PAGES * PAGE_SIZE];
char uarea0[UAREA_PAGES * PAGE_SIZE];
struct trapframe frame0;

vm_offset_t kstack0;
vm_offset_t kstack0_phys;

struct kva_md_info kmi;

u_long ofw_vec;
u_long ofw_tba;

static struct timecounter tick_tc;

static timecounter_get_t tick_get_timecount;
void sparc64_init(caddr_t mdp, u_long o1, u_long o2, u_long o3,
		  ofw_vec_t *vec);
void sparc64_shutdown_final(void *dummy, int howto);

static void cpu_startup(void *);
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL);

CTASSERT((1 << INT_SHIFT) == sizeof(int));
CTASSERT((1 << PTR_SHIFT) == sizeof(char *));

CTASSERT(sizeof(struct pcpu) <= ((PCPU_PAGES * PAGE_SIZE) / 2));

static void
cpu_startup(void *arg)
{
	phandle_t child;
	phandle_t root;
	char type[8];
	u_int clock;

	root = OF_peer(0);
	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		OF_getprop(child, "device_type", type, sizeof(type));
		if (strcmp(type, "cpu") == 0)
			break;
	}
	if (child == 0)
		panic("cpu_startup: no cpu\n");
	OF_getprop(child, "clock-frequency", &clock, sizeof(clock));

	tick_tc.tc_get_timecount = tick_get_timecount;
	tick_tc.tc_poll_pps = NULL;
	tick_tc.tc_counter_mask = ~0u;
	tick_tc.tc_frequency = clock;
	tick_tc.tc_name = "tick";
	tc_init(&tick_tc);

	cpu_identify(rdpr(ver), clock, PCPU_GET(cpuid));
	cache_init(child);

	vm_ksubmap_init(&kmi);

	bufinit();
	vm_pager_bufferinit();

	tick_start(clock, tick_hardclock);

	EVENTHANDLER_REGISTER(shutdown_final, sparc64_shutdown_final, NULL,
	    SHUTDOWN_PRI_LAST);
}

void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t size)
{
}

unsigned
tick_get_timecount(struct timecounter *tc)
{
	return ((unsigned)rd(tick));
}

void
sparc64_init(caddr_t mdp, u_long o1, u_long o2, u_long o3, ofw_vec_t *vec)
{
	struct pcpu *pc;
	vm_offset_t end;
	vm_offset_t off;
	caddr_t kmdp;

	end = 0;
	kmdp = NULL;

	/*
	 * Initialize openfirmware (needed for console).
	 */
	OF_init(vec);

	/*
	 * Parse metadata if present and fetch parameters.  Must be before the
	 * console is inited so cninit gets the right value of boothowto.
	 */
	if (mdp != NULL) {
		preload_metadata = mdp;
		kmdp = preload_search_by_type("elf kernel");
		if (kmdp != NULL) {
			boothowto = MD_FETCH(kmdp, MODINFOMD_HOWTO, int);
			kern_envp = MD_FETCH(kmdp, MODINFOMD_ENVP, char *);
			end = MD_FETCH(kmdp, MODINFOMD_KERNEND, vm_offset_t);
			kernel_tlb_slots = MD_FETCH(kmdp, MODINFOMD_DTLB_SLOTS,
			    int);
			kernel_ttes = (struct tte *)preload_search_info(kmdp,
			    MODINFO_METADATA | MODINFOMD_DTLB);
		}
	}

	/*
	 * Initialize the console before printing anything.
	 */
	cninit();

	/*
	 * Panic is there is no metadata.  Most likely the kernel was booted
	 * directly, instead of through loader(8).
	 */
	if (mdp == NULL || kmdp == NULL) {
		printf("sparc64_init: no loader metadata.\n"
		       "This probably means you are not using loader(8).\n");
		panic("sparc64_init");
	}

	/*
	 * Sanity check the kernel end, which is important.
	 */
	if (end == 0) {
		printf("sparc64_init: warning, kernel end not specified.\n"
		       "Attempting to continue anyway.\n");
		end = (vm_offset_t)_end;
	}

#ifdef DDB
	kdb_init();
#endif

#ifdef SMP
	mp_tramp = mp_tramp_alloc();
#endif

	/*
	 * Initialize virtual memory and calculate physmem.
	 */
	pmap_bootstrap(end);

	/*
	 * Initialize tunables.
	 */
	init_param1();
	init_param2(physmem);

	/*
	 * Disable tick for now.
	 */
	tick_stop();

	/*
	 * Initialize the interrupt tables.
	 */
	intr_init1();

	/*
	 * Initialize proc0 stuff (p_contested needs to be done early).
	 */
	proc_linkup(&proc0, &proc0.p_ksegrp, &proc0.p_kse, &thread0);
	proc0.p_md.md_utrap = NULL;
	proc0.p_uarea = (struct user *)uarea0;
	proc0.p_stats = &proc0.p_uarea->u_stats;
	thread0.td_kstack = kstack0;
	thread0.td_pcb = (struct pcb *)
	    (thread0.td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;
	frame0.tf_tstate = TSTATE_IE | TSTATE_PEF;
	thread0.td_frame = &frame0;

	/*
	 * Prime our per-cpu data page for use.  Note, we are using it for our
	 * stack, so don't pass the real size (PAGE_SIZE) to pcpu_init or
	 * it'll zero it out from under us.
	 */
	pc = (struct pcpu *)(pcpu0 + (PCPU_PAGES * PAGE_SIZE)) - 1;
	pcpu_init(pc, 0, sizeof(struct pcpu));
	pc->pc_curthread = &thread0;
	pc->pc_curpcb = thread0.td_pcb;
	pc->pc_mid = UPA_CR_GET_MID(ldxa(0, ASI_UPA_CONFIG_REG));
	pc->pc_addr = (vm_offset_t)pcpu0;
	pc->pc_tlb_ctx = TLB_CTX_USER_MIN;
	pc->pc_tlb_ctx_min = TLB_CTX_USER_MIN;
	pc->pc_tlb_ctx_max = TLB_CTX_USER_MAX;

	/*
	 * Initialize global registers.
	 */
	cpu_setregs(pc);

	/*
	 * Map and initialize the message buffer (after setting trap table).
	 */
	for (off = 0; off < round_page(MSGBUF_SIZE); off += PAGE_SIZE)
		pmap_kenter((vm_offset_t)msgbufp + off, msgbuf_phys + off);
	msgbufinit(msgbufp, MSGBUF_SIZE);

	mutex_init();
	intr_init2();
}

void
set_openfirm_callback(ofw_vec_t *vec)
{
	ofw_tba = rdpr(tba);
	ofw_vec = (u_long)vec;
}

void
sendsig(sig_t catcher, int sig, sigset_t *mask, u_long code)
{
	struct trapframe *tf;
	struct sigframe *sfp;
	struct sigacts *psp;
	struct sigframe sf;
	struct thread *td;
	struct frame *fp;
	struct proc *p;
	int oonstack;
	u_long sp;

	oonstack = 0;
	td = curthread;
	p = td->td_proc;
	psp = p->p_sigacts;
	tf = td->td_frame;
	sp = tf->tf_sp + SPOFF;
	oonstack = sigonstack(sp);

	CTR4(KTR_SIG, "sendsig: td=%p (%s) catcher=%p sig=%d", td, p->p_comm,
	    catcher, sig);

	/* Save user context. */
	bzero(&sf, sizeof(sf));
	sf.sf_uc.uc_sigmask = *mask;
	sf.sf_uc.uc_stack = p->p_sigstk;
	sf.sf_uc.uc_stack.ss_flags = (p->p_flag & P_ALTSTACK)
	    ? ((oonstack) ? SS_ONSTACK : 0) : SS_DISABLE;
	sf.sf_uc.uc_mcontext.mc_onstack = (oonstack) ? 1 : 0;
	bcopy(tf->tf_global, sf.sf_uc.uc_mcontext.mc_global,
	    sizeof (tf->tf_global));
	bcopy(tf->tf_out, sf.sf_uc.uc_mcontext.mc_out, sizeof (tf->tf_out));
	sf.sf_uc.uc_mcontext.mc_tpc = tf->tf_tpc;
	sf.sf_uc.uc_mcontext.mc_tnpc = tf->tf_tnpc;
	sf.sf_uc.uc_mcontext.mc_tstate = tf->tf_tstate;

	/* Allocate and validate space for the signal handler context. */
	if ((p->p_flag & P_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sfp = (struct sigframe *)(p->p_sigstk.ss_sp +
		    p->p_sigstk.ss_size - sizeof(struct sigframe));
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
		p->p_sigstk.ss_flags |= SS_ONSTACK;
#endif
	} else
		sfp = (struct sigframe *)sp - 1;
	PROC_UNLOCK(p);

	/*
	 * vm_map_growstack() will fail if *sfp does not fit inside the stack
	 * and the stack can not be grown.
	 * useracc() will return FALSE if access is denied.
	 */
	fp = (struct frame *)sfp - 1;
	if (vm_map_growstack(p, (u_long)fp) != KERN_SUCCESS ||
	    !useracc((caddr_t)fp, sizeof(*fp) + sizeof(*sfp), VM_PROT_WRITE)) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		CTR2(KTR_SIG, "sendsig: trashed stack td=%p sfp=%p", td, sfp);
		PROC_LOCK(p);
		SIGACTION(p, SIGILL) = SIG_DFL;
		SIGDELSET(p->p_sigignore, SIGILL);
		SIGDELSET(p->p_sigcatch, SIGILL);
		SIGDELSET(p->p_sigmask, SIGILL);
		psignal(p, SIGILL);
		return;
	}

	/* Translate the signal if appropriate. */
	if (p->p_sysent->sv_sigtbl && sig <= p->p_sysent->sv_sigsize)
		sig = p->p_sysent->sv_sigtbl[_SIG_IDX(sig)];

	/* Build the argument list for the signal handler. */
	tf->tf_out[0] = sig;
	tf->tf_out[2] = (register_t)&sfp->sf_uc;
	tf->tf_out[3] = tf->tf_type;
	tf->tf_out[4] = (register_t)catcher;
	PROC_LOCK(p);
	if (SIGISMEMBER(p->p_sigacts->ps_siginfo, sig)) {
		/* Signal handler installed with SA_SIGINFO. */
		tf->tf_out[1] = (register_t)&sfp->sf_si;
		
		/* Fill siginfo structure. */
		sf.sf_si.si_signo = sig;
		sf.sf_si.si_code = code;
		sf.sf_si.si_addr = (void *)tf->tf_sfar;
	} else {
		/* Old FreeBSD-style arguments. */
		tf->tf_out[1] = code;
	}
	PROC_UNLOCK(p);

	/* Copy the sigframe out to the user's stack. */
	if (rwindow_save(td) != 0 || copyout(&sf, sfp, sizeof(*sfp)) != 0 ||
	    suword(&fp->f_in[6], tf->tf_out[6]) != 0) {
		/*
		 * Something is wrong with the stack pointer.
		 * ...Kill the process.
		 */
		CTR2(KTR_SIG, "sendsig: sigexit td=%p sfp=%p", td, sfp);
		PROC_LOCK(p);
		sigexit(td, SIGILL);
		/* NOTREACHED */
	}

	tf->tf_tpc = PS_STRINGS - *(p->p_sysent->sv_szsigcode);
	tf->tf_tnpc = tf->tf_tpc + 4;
	tf->tf_sp = (u_long)fp - SPOFF;

	CTR3(KTR_SIG, "sendsig: return td=%p pc=%#lx sp=%#lx", td, tf->tf_tpc,
	    tf->tf_sp);

	PROC_LOCK(p);
}

/*
 * Stub to satisfy the reference to osigreturn in the syscall table.  This
 * is needed even for newer arches that don't support old signals because
 * the syscall table is machine-independent.
 */
int
osigreturn(struct thread *td, struct osigreturn_args *uap)
{

	return (nosys(td, (struct nosys_args *)uap));
}

#ifndef	_SYS_SYSPROTO_H_
struct sigreturn_args {
	ucontext_t *ucp;
};
#endif

int
sigreturn(struct thread *td, struct sigreturn_args *uap)
{
	struct trapframe *tf;
	struct proc *p;
	ucontext_t uc;

	p = td->td_proc;
	if (rwindow_save(td)) {
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	CTR2(KTR_SIG, "sigreturn: td=%p ucp=%p", td, uap->sigcntxp);
	if (copyin(uap->sigcntxp, &uc, sizeof(uc)) != 0) {
		CTR1(KTR_SIG, "sigreturn: efault td=%p", td);
		return (EFAULT);
	}

	if (((uc.uc_mcontext.mc_tpc | uc.uc_mcontext.mc_tnpc) & 3) != 0)
		return (EINVAL);
	if (!TSTATE_SECURE(uc.uc_mcontext.mc_tstate))
		return (EINVAL);

	tf = td->td_frame;
	bcopy(uc.uc_mcontext.mc_global, tf->tf_global,
	    sizeof(tf->tf_global));
	bcopy(uc.uc_mcontext.mc_out, tf->tf_out, sizeof(tf->tf_out));
	tf->tf_tpc = uc.uc_mcontext.mc_tpc;
	tf->tf_tnpc = uc.uc_mcontext.mc_tnpc;
	tf->tf_tstate = uc.uc_mcontext.mc_tstate;
	PROC_LOCK(p);
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	if (uc.uc_mcontext.mc_onstack & 1)
		p->p_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigstk.ss_flags &= ~SS_ONSTACK;
#endif

	p->p_sigmask = uc.uc_sigmask;
	SIG_CANTMASK(p->p_sigmask);
	signotify(p);
	PROC_UNLOCK(p);
	CTR4(KTR_SIG, "sigreturn: return td=%p pc=%#lx sp=%#lx tstate=%#lx",
	     td, tf->tf_tpc, tf->tf_sp, tf->tf_tstate);
	return (EJUSTRETURN);
}

/*
 * Exit the kernel and execute a firmware call that will not return, as
 * specified by the arguments.
 */
void
cpu_shutdown(void *args)
{

#ifdef SMP
	cpu_mp_shutdown();
#endif
	openfirmware_exit(args);
}

/*
 * Duplicate OF_exit() with a different firmware call function that restores
 * the trap table, otherwise a RED state exception is triggered in at least
 * some firmware versions.
 */
void
cpu_halt(void)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
	} args = {
		(cell_t)"exit",
		0,
		0
	};

	cpu_shutdown(&args);
}

void
sparc64_shutdown_final(void *dummy, int howto)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
	} args = {
		(cell_t)"SUNW,power-off",
		0,
		0
	};

	/* Turn the power off? */
	if ((howto & RB_POWEROFF) != 0)
		cpu_shutdown(&args);
	/* In case of halt, return to the firmware */
	if ((howto & RB_HALT) != 0)
		cpu_halt();
}

int
ptrace_set_pc(struct thread *td, u_long addr)
{

	td->td_frame->tf_tpc = addr;
	td->td_frame->tf_tnpc = addr + 4;
	return (0);
}

int
ptrace_single_step(struct thread *td)
{
	TODO;
	return (0);
}

void
setregs(struct thread *td, u_long entry, u_long stack, u_long ps_strings)
{
	struct trapframe *tf;
	struct md_utrap *ut;
	struct pcb *pcb;
	u_long sp;

	/* XXX no cpu_exec */
	if ((ut = td->td_proc->p_md.md_utrap) != NULL) {
		ut->ut_refcnt--;
		if (ut->ut_refcnt == 0)
			free(ut, M_SUBPROC);
		td->td_proc->p_md.md_utrap = NULL;
	}

	pcb = td->td_pcb;
	sp = rounddown(stack, 16);
	tf = td->td_frame;
	fp_init_thread(td);
	bzero(pcb->pcb_rw, sizeof(pcb->pcb_rw));
	bzero(pcb->pcb_rwsp, sizeof(pcb->pcb_rwsp));
	pcb->pcb_nsaved = 0;
	bzero(tf, sizeof (*tf));
	tf->tf_out[0] = stack;
	tf->tf_out[3] = PS_STRINGS;
	tf->tf_out[6] = sp - SPOFF - sizeof(struct frame);
	tf->tf_tnpc = entry + 4;
	tf->tf_tpc = entry;
	tf->tf_tstate = TSTATE_IE | TSTATE_PEF | TSTATE_MM_TSO;

	td->td_retval[0] = tf->tf_out[0];
	td->td_retval[1] = tf->tf_out[1];
}

void
Debugger(const char *msg)
{

	printf("Debugger(\"%s\")\n", msg);
	critical_enter();
	breakpoint();
	critical_exit();
}

int
fill_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *tf;

	tf = td->td_frame;
	bcopy(tf->tf_global, regs->r_global, sizeof(tf->tf_global));
	bcopy(tf->tf_out, regs->r_out, sizeof(tf->tf_out));
	regs->r_npc = tf->tf_tnpc;
	regs->r_pc = tf->tf_tpc;
	regs->r_tstate = tf->tf_tstate;
	regs->r_y = tf->tf_y;
	return (0);
}

int
set_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *tf;

	tf = td->td_frame;
	if (((regs->r_pc | regs->r_npc) & 3) != 0)
		return (EINVAL);
	if (!TSTATE_SECURE(regs->r_tstate))
		return (EINVAL);
	bcopy(regs->r_global, tf->tf_global, sizeof(regs->r_global));
	bcopy(regs->r_out, tf->tf_out, sizeof(regs->r_out));
	tf->tf_tnpc = regs->r_npc;
	tf->tf_tpc = regs->r_pc;
	tf->tf_tstate = regs->r_tstate;
	tf->tf_y = regs->r_y;
	return (0);
}

int
fill_dbregs(struct thread *td, struct dbreg *dbregs)
{

	return (ENOSYS);
}

int
set_dbregs(struct thread *td, struct dbreg *dbregs)
{

	return (ENOSYS);
}

int
fill_fpregs(struct thread *td, struct fpreg *fpregs)
{
	struct trapframe *tf;
	struct pcb *pcb;

	pcb = td->td_pcb;
	tf = td->td_frame;
	bcopy(pcb->pcb_fpstate.fp_fb, fpregs->fr_regs,
	    sizeof(pcb->pcb_fpstate.fp_fb));
	fpregs->fr_fprs = tf->tf_fprs;
	fpregs->fr_fsr = tf->tf_fsr;
	return (0);
}

int
set_fpregs(struct thread *td, struct fpreg *fpregs)
{
	struct trapframe *tf;
	struct pcb *pcb;

	pcb = td->td_pcb;
	tf = td->td_frame;
	bcopy(fpregs->fr_regs, pcb->pcb_fpstate.fp_fb,
	    sizeof(fpregs->fr_regs));
	tf->tf_fprs = fpregs->fr_fprs;
	tf->tf_fsr = fpregs->fr_fsr;
	return (0);
}
