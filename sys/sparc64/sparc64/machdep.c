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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>
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

int physmem = 0;
int cold = 1;
long dumplo;
int Maxmem = 0;

u_long debug_mask;

struct mtx Giant;
struct mtx sched_lock;

struct globaldata __globaldata;
/*
 * This needs not be aligned as the other user areas, provided that process 0
 * does not have an fp state (which it doesn't normally).
 * This constraint is only here for debugging.
 */
char user0[UPAGES * PAGE_SIZE] __attribute__ ((aligned (64)));
struct user *proc0paddr;

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
	char name[32];
	char type[8];
	u_int clock;
	caddr_t p;

	root = OF_peer(0);
	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		OF_getprop(child, "device_type", type, sizeof(type));
		if (strcmp(type, "cpu") == 0)
			break;
	}
	if (child == 0)
		panic("cpu_startup: no cpu\n");
	OF_getprop(child, "name", name, sizeof(name));
	OF_getprop(child, "clock-frequency", &clock, sizeof(clock));

	tick_tc.tc_get_timecount = tick_get_timecount;
	tick_tc.tc_poll_pps = NULL;
	tick_tc.tc_counter_mask = ~0u;
	tick_tc.tc_frequency = clock;
	tick_tc.tc_name = "tick";
	tc_init(&tick_tc);

	p = name;
	if (bcmp(p, "SUNW,", 5) == 0)
		p += 5;
	printf("CPU: %s Processor (%d.%02d MHz CPU)\n", p,
	    (clock + 4999) / 1000000, ((clock + 4999) / 10000) % 100);
#if 0
	ver = rdpr(ver);
	printf("manuf: %#lx impl: %#lx mask: %#lx maxtl: %#lx maxwin: %#lx\n",
	    VER_MANUF(ver), VER_IMPL(ver), VER_MASK(ver), VER_MAXTL(ver),
	    VER_MAXWIN(ver));
#endif

	vm_ksubmap_init(&kmi);

	bufinit();
	vm_pager_bufferinit();

	globaldata_register(globalp);
#if 0
	tick_start(clock, tick_hardclock);
#endif
}

unsigned
tick_get_timecount(struct timecounter *tc)
{
	return ((unsigned)rd(tick));
}

void
sparc64_init(struct bootinfo *bi, ofw_vec_t *vec)
{
	struct trapframe *tf;
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
	pmap_bootstrap(bi->bi_kpa, bi->bi_end);

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
	 * Initialize proc0 stuff (p_contested needs to be done early).
	 */
	LIST_INIT(&proc0.p_contested);
	proc0paddr = (struct user *)user0;
	proc0.p_addr = (struct user *)user0;
	proc0.p_stats = &proc0.p_addr->u_stats;
	tf = (struct trapframe *)(user0 + UPAGES * PAGE_SIZE -
	    sizeof(struct frame) - sizeof(*tf));
	proc0.p_frame = tf;
	tf->tf_tstate = 0;

	/*
	 * Initialize the per-cpu pointer so we can set curproc.
	 */
	globalp = &__globaldata;

	/*
	 * Setup pointers to interrupt data tables.
	 */
	globalp->gd_iq = &intr_queues[0];	/* XXX cpuno */
	globalp->gd_ivt = intr_vectors;

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
	__asm __volatile("mov %0, %%g7" : : "r" (&__globaldata));
	wrpr(pstate, ps, 0);

	/*
	 * Initialize curproc so that mutexes work.
	 */
	PCPU_SET(curproc, &proc0);
	PCPU_SET(curpcb, &((struct user *)user0)->u_pcb);
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
	struct proc *p;
	u_long sp;
	int oonstack;

	oonstack = 0;
	p = curproc;
	PROC_LOCK(p);
	psp = p->p_sigacts;
	tf = p->p_frame;
	sp = tf->tf_sp + SPOFF;
	oonstack = sigonstack(sp);

	CTR4(KTR_SIG, "sendsig: p=%p (%s) catcher=%p sig=%d", p, p->p_comm,
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
	if (vm_map_growstack(p, (u_long)sfp) != KERN_SUCCESS ||
	    !useracc((caddr_t)sfp, sizeof(*sfp), VM_PROT_WRITE)) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		CTR2(KTR_SIG, "sendsig: trashed stack p=%p sfp=%p", p, sfp);
		PROC_LOCK(p);
		SIGACTION(p, SIGILL) = SIG_DFL;
		SIGDELSET(p->p_sigignore, SIGILL);
		SIGDELSET(p->p_sigcatch, SIGILL);
		SIGDELSET(p->p_sigmask, SIGILL);
		psignal(p, SIGILL);
		PROC_UNLOCK(p);
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
	if (rwindow_save(p) != 0 || copyout(&sf, sfp, sizeof(*sfp)) != 0) {
		/*
		 * Something is wrong with the stack pointer.
		 * ...Kill the process.
		 */
		CTR2(KTR_SIG, "sendsig: sigexit p=%p sfp=%p", p, sfp);
		PROC_LOCK(p);
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}

	tf->tf_tpc = PS_STRINGS - *(p->p_sysent->sv_szsigcode);
	tf->tf_tnpc = tf->tf_tpc + 4;
	tf->tf_sp = (u_long)sfp - SPOFF;

	CTR3(KTR_SIG, "sendsig: return p=%p pc=%#lx sp=%#lx", p, tf->tf_tpc,
	    tf->tf_sp);
}

#ifndef	_SYS_SYSPROTO_H_
struct sigreturn_args {
	ucontext_t *ucp;
};
#endif

int
sigreturn(struct proc *p, struct sigreturn_args *uap)
{
	struct trapframe *tf;
	ucontext_t uc;

	if (rwindow_save(p)) {
		PROC_LOCK(p);
		sigexit(p, SIGILL);
	}

	CTR2(KTR_SIG, "sigreturn: p=%p ucp=%p", p, uap->sigcntxp);
	if (copyin(uap->sigcntxp, &uc, sizeof(uc)) != 0) {
		CTR1(KTR_SIG, "sigreturn: efault p=%p", p);
		return (EFAULT);
	}

	if (((uc.uc_mcontext.mc_tpc | uc.uc_mcontext.mc_tnpc) & 3) != 0)
		return (EINVAL);

	tf = p->p_frame;
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
	CTR4(KTR_SIG, "sigreturn: return p=%p pc=%#lx sp=%#lx tstate=%#lx",
	     p, tf->tf_tpc, tf->tf_sp, tf->tf_tstate);
	return (EJUSTRETURN);
}

void
cpu_halt(void)
{
	TODO;
}

int
ptrace_set_pc(struct proc *p, u_long addr)
{

	p->p_frame->tf_tpc = addr;
	p->p_frame->tf_tnpc = addr + 4;
	return (0);
}

int
ptrace_single_step(struct proc *p)
{
	TODO;
	return (0);
}

void
setregs(struct proc *p, u_long entry, u_long stack, u_long ps_strings)
{
	struct pcb *pcb;
	struct frame *fp;

	/* Round the stack down to a multiple of 16 bytes. */
	stack = ((stack) / 16) * 16;
	pcb = &p->p_addr->u_pcb;
	/* XXX: honor the real number of windows... */
	bzero(pcb->pcb_rw, sizeof(pcb->pcb_rw));
	/* The inital window for the process (%cw = 0). */
	fp = (struct frame *)((caddr_t)p->p_addr + UPAGES * PAGE_SIZE) - 1;
	/* Make sure the frames that are frobbed are actually flushed. */
	__asm __volatile("flushw");
	mtx_lock_spin(&sched_lock);
	fp_init_proc(pcb);
	/* Setup state in the trap frame. */
	p->p_frame->tf_tstate = TSTATE_IE;
	p->p_frame->tf_tpc = entry;
	p->p_frame->tf_tnpc = entry + 4;
	p->p_frame->tf_pil = 0;
	/*
	 * Set up the registers for the user.
	 * The SCD (2.4.1) mandates:
	 * - the initial %fp should be 0
	 * - the initial %sp should point to the top frame, which should be
	 *   16-byte-aligned
	 * - %g1, if != 0, passes a function pointer which should be registered
	 *   with atexit().
	 */
	bzero(p->p_frame->tf_out, sizeof(p->p_frame->tf_out));
	bzero(p->p_frame->tf_global, sizeof(p->p_frame->tf_global));
	/* Set up user stack. */
	fp->f_fp = stack - SPOFF;
	p->p_frame->tf_out[6] = stack - SPOFF - sizeof(struct frame);
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
fill_dbregs(struct proc *p, struct dbreg *dbregs)
{
	TODO;
	return (0);
}

int
set_dbregs(struct proc *p, struct dbreg *dbregs)
{
	TODO;
	return (0);
}

int
fill_regs(struct proc *p, struct reg *regs)
{
	TODO;
	return (0);
}

int
set_regs(struct proc *p, struct reg *regs)
{
	TODO;
	return (0);
}

int
fill_fpregs(struct proc *p, struct fpreg *fpregs)
{
	TODO;
	return (0);
}

int
set_fpregs(struct proc *p, struct fpreg *fpregs)
{
	TODO;
	return (0);
}
