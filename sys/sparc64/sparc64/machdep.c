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
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/ptrace.h>
#include <sys/signalvar.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/timetc.h>
#include <sys/user.h>
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

#include <machine/bootinfo.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/fp.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/pmap.h>
#include <machine/pstate.h>
#include <machine/reg.h>
#include <machine/sigframe.h>
#include <machine/tick.h>
#include <machine/tstate.h>
#include <machine/ver.h>

typedef int ofw_vec_t(void *);

extern char tl0_base[];

extern char _end[];

int physmem;
int cold = 1;
long dumplo;
int Maxmem;

u_long debug_mask;

struct mtx Giant;
struct mtx sched_lock;

static struct globaldata __globaldata;
/*
 * This needs not be aligned as the other user areas, provided that process 0
 * does not have an fp state (which it doesn't normally).
 * This constraint is only here for debugging.
 */
char kstack0[KSTACK_PAGES * PAGE_SIZE] __attribute__((aligned(64)));
static char uarea0[UAREA_PAGES * PAGE_SIZE] __attribute__((aligned(64)));
static struct trapframe frame0;
struct user *proc0uarea;
vm_offset_t proc0kstack;

struct kva_md_info kmi;

u_long ofw_vec;
u_long ofw_tba;

static struct timecounter tick_tc;

static timecounter_get_t tick_get_timecount;
void sparc64_init(struct bootinfo *bi, ofw_vec_t *vec);

static void cpu_startup(void *);
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL);

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

	cpu_identify(clock);

	vm_ksubmap_init(&kmi);

	bufinit();
	vm_pager_bufferinit();

	globaldata_register(globalp);

	intr_init();
	tick_start(clock, tick_hardclock);
}

unsigned
tick_get_timecount(struct timecounter *tc)
{
	return ((unsigned)rd(tick));
}

void
sparc64_init(struct bootinfo *bi, ofw_vec_t *vec)
{
	vm_offset_t off;
	u_long ps;

	/*
	 * Initialize openfirmware (needed for console).
	 */
	OF_init(vec);

	/*
	 * Initialize the console before printing anything.
	 */
	cninit();

	/*
	 * Check that the bootinfo struct is sane.
	 */
	if (bi->bi_version != BOOTINFO_VERSION)
		panic("sparc64_init: bootinfo version mismatch");
	if (bi->bi_metadata == 0)
		panic("sparc64_init: no loader metadata");
	boothowto = bi->bi_howto;
	kern_envp = (char *)bi->bi_envp;
	preload_metadata = (caddr_t)bi->bi_metadata;

	/*
	 * Initialize tunables.
	 */
	init_param();

#ifdef DDB
	kdb_init();
#endif

	/*
	 * Initialize virtual memory.
	 */
	pmap_bootstrap(bi->bi_end);

	/*
	 * Disable tick for now.
	 */
	tick_stop();

	/*
	 * Force trap level 1 and take over the trap table.
	 */
	wrpr(tl, 0, 1);
	wrpr(tba, tl0_base, 0);

	/*
	 * Map and initialize the message buffer (after setting trap table).
	 */
	for (off = 0; off < round_page(MSGBUF_SIZE); off += PAGE_SIZE)
		pmap_kenter((vm_offset_t)msgbufp + off, msgbuf_phys + off);
	msgbufinit(msgbufp, MSGBUF_SIZE);

	proc_linkup(&proc0);
	/*
	 * Initialize proc0 stuff (p_contested needs to be done early).
	 */
	proc0uarea = (struct user *)uarea0;
	proc0kstack = (vm_offset_t)kstack0;
	proc0.p_uarea = proc0uarea;
	proc0.p_stats = &proc0.p_uarea->u_stats;
	thread0 = &proc0.p_thread;
	thread0->td_kstack = proc0kstack;
	thread0->td_pcb = (struct pcb *)
	    (thread0->td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;
	frame0.tf_tstate = TSTATE_IE;
	thread0->td_frame = &frame0;
	LIST_INIT(&thread0->td_contested);

	/*
	 * Initialize the per-cpu pointer so we can set curproc.
	 */
	globalp = &__globaldata;

	/*
	 * Put the globaldata pointer in the alternate and interrupt %g7 also.
	 * globaldata is tied to %g7. We could therefore also use assignments to
	 * globaldata here.
	 * The alternate %g6 additionally points to a small per-cpu stack that
	 * is used to temporarily store global registers in special spill
	 * handlers.
	 */
	ps = rdpr(pstate);
	wrpr(pstate, ps, PSTATE_AG);
	__asm __volatile("mov %0, %%g6" : : "r"
	    (&__globaldata.gd_alt_stack[ALT_STACK_SIZE - 1]));
	__asm __volatile("mov %0, %%g7" : : "r" (&__globaldata));
	wrpr(pstate, ps, PSTATE_IG);
	__asm __volatile("mov %0, %%g6" : : "r" (&__globaldata.gd_iq));
	__asm __volatile("mov %0, %%g7" : : "r" (&intr_vectors));
	wrpr(pstate, ps, 0);

	/*
	 * Initialize curproc so that mutexes work.
	 */
	PCPU_SET(curthread, thread0);
	PCPU_SET(curpcb, thread0->td_pcb);
	PCPU_SET(spinlocks, NULL);

	/*
	 * Initialize mutexes.
	 */
	mtx_init(&sched_lock, "sched lock", MTX_SPIN | MTX_RECURSE);
	mtx_init(&Giant, "Giant", MTX_DEF | MTX_RECURSE);
	mtx_init(&proc0.p_mtx, "process lock", MTX_DEF);

	mtx_lock(&Giant);
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
	sf.sf_uc.uc_mcontext.mc_sp = tf->tf_sp;
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
	 * grow_stack() will return 0 if *sfp does not fit inside the stack
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
		sf.sf_si.si_addr = (void *)tf->tf_type;
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
#if 0
	if (!TSTATE_SECURE(uc.uc_mcontext.mc_tstate))
		return (EINVAL);
#endif

	tf = td->td_frame;
	bcopy(uc.uc_mcontext.mc_global, tf->tf_global,
	    sizeof(tf->tf_global));
	bcopy(uc.uc_mcontext.mc_out, tf->tf_out, sizeof(tf->tf_out));
	tf->tf_sp = uc.uc_mcontext.mc_sp;
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
	PROC_UNLOCK(p);
	CTR4(KTR_SIG, "sigreturn: return td=%p pc=%#lx sp=%#lx tstate=%#lx",
	     td, tf->tf_tpc, tf->tf_sp, tf->tf_tstate);
	return (EJUSTRETURN);
}

void
cpu_halt(void)
{

	OF_exit();
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
	struct pcb *pcb;
	struct frame *fp;
	u_long sp;

	/* Round the stack down to a multiple of 16 bytes. */
	sp = ((stack) / 16) * 16;
	pcb = td->td_pcb;
	/* XXX: honor the real number of windows... */
	bzero(pcb->pcb_rw, sizeof(pcb->pcb_rw));
	pcb->pcb_nsaved = 0;
	/* The inital window for the process (%cw = 0). */
	fp = (struct frame *)(td->td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;
	/* Make sure the frames that are frobbed are actually flushed. */
	__asm __volatile("flushw");
	mtx_lock_spin(&sched_lock);
	fp_init_thread(pcb);
	/* Setup state in the trap frame. */
	td->td_frame->tf_tstate = TSTATE_IE;
	td->td_frame->tf_tpc = entry;
	td->td_frame->tf_tnpc = entry + 4;
	td->td_frame->tf_pil = 0;
	/*
	 * Set up the registers for the user.
	 * The SCD (2.4.1) mandates:
	 * - the initial %fp should be 0
	 * - the initial %sp should point to the top frame, which should be
	 *   16-byte-aligned
	 * - %g1, if != 0, passes a function pointer which should be registered
	 *   with atexit().
	 */
	bzero(td->td_frame->tf_out, sizeof(td->td_frame->tf_out));
	bzero(td->td_frame->tf_global, sizeof(td->td_frame->tf_global));
	/* Set up user stack. */
	fp->f_fp = sp - SPOFF;
	td->td_frame->tf_out[0] = stack;
	td->td_frame->tf_out[1] = 0;
	td->td_frame->tf_out[2] = 0;
	td->td_frame->tf_out[3] = PS_STRINGS;
	td->td_frame->tf_out[6] = sp - SPOFF - sizeof(struct frame);
	td->td_retval[0] = td->td_frame->tf_out[0];
	td->td_retval[1] = td->td_frame->tf_out[1];
	wr(y, 0, 0);
	mtx_unlock_spin(&sched_lock);
}

void
Debugger(const char *msg)
{

	printf("Debugger(\"%s\")\n", msg);
	breakpoint();
}

int
fill_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *tf;
	struct pcb *pcb;

	tf = td->td_frame;
	pcb = td->td_pcb;
	bcopy(tf->tf_global, regs->r_global, sizeof(tf->tf_global));
	bcopy(tf->tf_out, regs->r_out, sizeof(tf->tf_out));
	regs->r_tstate = tf->tf_tstate;
	regs->r_pc = tf->tf_tpc;
	regs->r_npc = tf->tf_tnpc;
	regs->r_y = pcb->pcb_y;
	return (0);
}

int
set_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *tf;
	struct pcb *pcb;

	tf = td->td_frame;
	pcb = td->td_pcb;
	if (((regs->r_pc | regs->r_npc) & 3) != 0)
		return (EINVAL);
#if 0
	if (!TSTATE_SECURE(regs->r_tstate))
		return (EINVAL);
#endif
	bcopy(regs->r_global, tf->tf_global, sizeof(regs->r_global));
	bcopy(regs->r_out, tf->tf_out, sizeof(regs->r_out));
	tf->tf_tstate = regs->r_tstate;
	tf->tf_tpc = regs->r_pc;
	tf->tf_tnpc = regs->r_npc;
	pcb->pcb_y = regs->r_y;
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
	struct pcb *pcb;

	pcb = td->td_pcb;
	bcopy(pcb->pcb_fpstate.fp_fb, fpregs->fr_regs,
	    sizeof(pcb->pcb_fpstate.fp_fb));
	fpregs->fr_fsr = pcb->pcb_fpstate.fp_fsr;
	return (0);
}

int
set_fpregs(struct thread *td, struct fpreg *fpregs)
{
	struct pcb *pcb;

	pcb = td->td_pcb;
	bcopy(fpregs->fr_regs, pcb->pcb_fpstate.fp_fb,
	    sizeof(fpregs->fr_regs));
	pcb->pcb_fpstate.fp_fsr = fpregs->fr_fsr;
	return (0);
}
