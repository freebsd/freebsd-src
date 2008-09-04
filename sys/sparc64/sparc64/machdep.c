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
 *	from: FreeBSD: src/sys/i386/i386/machdep.c,v 1.477 2001/08/27
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_ddb.h"
#include "opt_kstack_pages.h"
#include "opt_msgbuf.h"

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/cons.h>
#include <sys/eventhandler.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/interrupt.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/signalvar.h>
#include <sys/smp.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/timetc.h>
#include <sys/ucontext.h>

#include <dev/ofw/openfirm.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vm_param.h>

#include <ddb/ddb.h>

#include <machine/bus.h>
#include <machine/cache.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/fp.h>
#include <machine/fsr.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/ofw_machdep.h>
#include <machine/ofw_mem.h>
#include <machine/pcb.h>
#include <machine/pmap.h>
#include <machine/pstate.h>
#include <machine/reg.h>
#include <machine/sigframe.h>
#include <machine/smp.h>
#include <machine/tick.h>
#include <machine/tlb.h>
#include <machine/tstate.h>
#include <machine/upa.h>
#include <machine/ver.h>

typedef int ofw_vec_t(void *);

#ifdef DDB
extern vm_offset_t ksym_start, ksym_end;
#endif

struct tlb_entry *kernel_tlbs;
int kernel_tlb_slots;

int cold = 1;
long Maxmem;
long realmem;

char pcpu0[PCPU_PAGES * PAGE_SIZE];
struct trapframe frame0;

vm_offset_t kstack0;
vm_paddr_t kstack0_phys;

struct kva_md_info kmi;

u_long ofw_vec;
u_long ofw_tba;

char sparc64_model[32];

static int cpu_use_vis = 1;

cpu_block_copy_t *cpu_block_copy;
cpu_block_zero_t *cpu_block_zero;

void sparc64_init(caddr_t mdp, u_long o1, u_long o2, u_long o3,
    ofw_vec_t *vec);
void sparc64_shutdown_final(void *dummy, int howto);

static void cpu_startup(void *);
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL);

CTASSERT((1 << INT_SHIFT) == sizeof(int));
CTASSERT((1 << PTR_SHIFT) == sizeof(char *));

CTASSERT(sizeof(struct reg) == 256);
CTASSERT(sizeof(struct fpreg) == 272);
CTASSERT(sizeof(struct __mcontext) == 512);

CTASSERT((sizeof(struct pcb) & (64 - 1)) == 0);
CTASSERT((offsetof(struct pcb, pcb_kfp) & (64 - 1)) == 0);
CTASSERT((offsetof(struct pcb, pcb_ufp) & (64 - 1)) == 0);
CTASSERT(sizeof(struct pcb) <= ((KSTACK_PAGES * PAGE_SIZE) / 8));

CTASSERT(sizeof(struct pcpu) <= ((PCPU_PAGES * PAGE_SIZE) / 2));

static void
cpu_startup(void *arg)
{
	vm_paddr_t physsz;
	int i;

	physsz = 0;
	for (i = 0; i < sparc64_nmemreg; i++)
		physsz += sparc64_memreg[i].mr_size;
	printf("real memory  = %lu (%lu MB)\n", physsz,
	    physsz / (1024 * 1024));
	realmem = (long)physsz / PAGE_SIZE;

	vm_ksubmap_init(&kmi);

	bufinit();
	vm_pager_bufferinit();

	EVENTHANDLER_REGISTER(shutdown_final, sparc64_shutdown_final, NULL,
	    SHUTDOWN_PRI_LAST);

	printf("avail memory = %lu (%lu MB)\n", cnt.v_free_count * PAGE_SIZE,
	    cnt.v_free_count / ((1024 * 1024) / PAGE_SIZE));

	if (bootverbose)
		printf("machine: %s\n", sparc64_model);

	cpu_identify(rdpr(ver), PCPU_GET(clock), curcpu);
}

void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t size)
{
	struct intr_request *ir;
	int i;

	pcpu->pc_irtail = &pcpu->pc_irhead;
	for (i = 0; i < IR_FREE; i++) {
		ir = &pcpu->pc_irpool[i];
		ir->ir_next = pcpu->pc_irfree;
		pcpu->pc_irfree = ir;
	}
}

void
spinlock_enter(void)
{
	struct thread *td;
	register_t pil;

	td = curthread;
	if (td->td_md.md_spinlock_count == 0) {
		pil = rdpr(pil);
		wrpr(pil, 0, PIL_TICK);
		td->td_md.md_saved_pil = pil;
	}
	td->td_md.md_spinlock_count++;
	critical_enter();
}

void
spinlock_exit(void)
{
	struct thread *td;

	td = curthread;
	critical_exit();
	td->td_md.md_spinlock_count--;
	if (td->td_md.md_spinlock_count == 0)
		wrpr(pil, td->td_md.md_saved_pil, 0);
}

void
sparc64_init(caddr_t mdp, u_long o1, u_long o2, u_long o3, ofw_vec_t *vec)
{
	char type[8];
	char *env;
	struct pcpu *pc;
	vm_offset_t end;
	caddr_t kmdp;
	phandle_t child;
	phandle_t root;
	uint32_t portid;

	end = 0;
	kmdp = NULL;

	/*
	 * Find out what kind of CPU we have first, for anything that changes
	 * behaviour.
	 */
	cpu_impl = VER_IMPL(rdpr(ver));

	/*
	 * Do CPU-specific Initialization.
	 */
	if (cpu_impl >= CPU_IMPL_ULTRASPARCIII)
		cheetah_init();

	/*
	 * Clear (S)TICK timer (including NPT).
	 */
	tick_clear();

	/*
	 * UltraSparc II[e,i] based systems come up with the tick interrupt
	 * enabled and a handler that resets the tick counter, causing DELAY()
	 * to not work properly when used early in boot.
	 * UltraSPARC III based systems come up with the system tick interrupt
	 * enabled, causing an interrupt storm on startup since they are not
	 * handled.
	 */
	tick_stop();

	/*
	 * Initialize Open Firmware (needed for console).
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
			kernel_tlbs = (void *)preload_search_info(kmdp,
			    MODINFO_METADATA | MODINFOMD_DTLB);
		}
	}

	init_param1();

	/*
	 * Prime our per-CPU data page for use.  Note, we are using it for
	 * our stack, so don't pass the real size (PAGE_SIZE) to pcpu_init
	 * or it'll zero it out from under us.
	 */
	pc = (struct pcpu *)(pcpu0 + (PCPU_PAGES * PAGE_SIZE)) - 1;
	pcpu_init(pc, 0, sizeof(struct pcpu));
	pc->pc_addr = (vm_offset_t)pcpu0;
	pc->pc_mid = UPA_CR_GET_MID(ldxa(0, ASI_UPA_CONFIG_REG));
	pc->pc_tlb_ctx = TLB_CTX_USER_MIN;
	pc->pc_tlb_ctx_min = TLB_CTX_USER_MIN;
	pc->pc_tlb_ctx_max = TLB_CTX_USER_MAX;

	/*
	 * Determine the OFW node and frequency of the BSP (and ensure the
	 * BSP is in the device tree in the first place).
	 */
	pc->pc_node = 0;
	root = OF_peer(0);
	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		if (OF_getprop(child, "device_type", type, sizeof(type)) <= 0)
			continue;
		if (strcmp(type, "cpu") != 0)
			continue;
		if (OF_getprop(child, cpu_impl < CPU_IMPL_ULTRASPARCIII ?
		    "upa-portid" : "portid", &portid, sizeof(portid)) <= 0)
			continue;
		if (portid == pc->pc_mid) {
			pc->pc_node = child;
			break;
		}
	}
	if (pc->pc_node == 0)
		OF_exit();
	if (OF_getprop(child, "clock-frequency", &pc->pc_clock,
	    sizeof(pc->pc_clock)) <= 0)
		OF_exit();

	/*
	 * Provide a DELAY() that works before PCPU_REG is set.  We can't
	 * set PCPU_REG without also taking over the trap table or the
	 * firmware will overwrite it.  Unfortunately, it's way to early
	 * to also take over the trap table at this point.
	 */
	clock_boot = pc->pc_clock;
	delay_func = delay_boot;

	/*
	 * Initialize the console before printing anything.
	 * NB: the low-level console drivers require a working DELAY() at
	 * this point.
	 */
	cninit();

	/*
	 * Panic if there is no metadata.  Most likely the kernel was booted
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

	cache_init(pc);
	cache_enable();
	uma_set_align(pc->pc_cache.dc_linesize - 1);

	cpu_block_copy = bcopy;
	cpu_block_zero = bzero;
	getenv_int("machdep.use_vis", &cpu_use_vis);
	if (cpu_use_vis) {
		switch (cpu_impl) {
		case CPU_IMPL_SPARC64:
		case CPU_IMPL_ULTRASPARCI:
		case CPU_IMPL_ULTRASPARCII:
		case CPU_IMPL_ULTRASPARCIIi:
		case CPU_IMPL_ULTRASPARCIIe:
			cpu_block_copy = spitfire_block_copy;
			cpu_block_zero = spitfire_block_zero;
			break;
		}
	}

#ifdef SMP
	mp_init();
#endif

	/*
	 * Initialize virtual memory and calculate physmem.
	 */
	pmap_bootstrap(end);

	/*
	 * Initialize tunables.
	 */
	init_param2(physmem);
	env = getenv("kernelname");
	if (env != NULL) {
		strlcpy(kernelname, env, sizeof(kernelname));
		freeenv(env);
	}

	/*
	 * Initialize the interrupt tables.
	 */
	intr_init1();

	/*
	 * Initialize proc0, set kstack0, frame0, curthread and curpcb.
	 */
	proc_linkup0(&proc0, &thread0);
	proc0.p_md.md_sigtramp = NULL;
	proc0.p_md.md_utrap = NULL;
	thread0.td_kstack = kstack0;
	thread0.td_pcb = (struct pcb *)
	    (thread0.td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;
	frame0.tf_tstate = TSTATE_IE | TSTATE_PEF | TSTATE_PRIV;
	thread0.td_frame = &frame0;
	pc->pc_curthread = &thread0;
	pc->pc_curpcb = thread0.td_pcb;

	/*
	 * Initialize global registers.
	 */
	cpu_setregs(pc);

	/*
	 * Take over the trap table via the PROM.  Using the PROM for this
	 * is necessary in order to set obp-control-relinquished to true
	 * within the PROM so obtaining /virtual-memory/translations doesn't
	 * trigger a fatal reset error or worse things further down the road.
	 * XXX it should be possible to use this soley instead of writing
	 * %tba in cpu_setregs().  Doing so causes a hang however.
	 */
	sun4u_set_traptable(tl0_base);

	/*
	 * It's now safe to use the real DELAY().
	 */
	delay_func = delay_tick;

	/*
	 * Initialize the message buffer (after setting trap table).
	 */
	msgbufinit(msgbufp, MSGBUF_SIZE);

	mutex_init();
	intr_init2();

	/*
	 * Finish pmap initialization now that we're ready for mutexes.
	 */
	PMAP_LOCK_INIT(kernel_pmap);

	OF_getprop(root, "name", sparc64_model, sizeof(sparc64_model) - 1);

	kdb_init();

#ifdef KDB
	if (boothowto & RB_KDB)
		kdb_enter(KDB_WHY_BOOTFLAGS, "Boot flags requested debugger");
#endif
}

void
set_openfirm_callback(ofw_vec_t *vec)
{

	ofw_tba = rdpr(tba);
	ofw_vec = (u_long)vec;
}

void
sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct trapframe *tf;
	struct sigframe *sfp;
	struct sigacts *psp;
	struct sigframe sf;
	struct thread *td;
	struct frame *fp;
	struct proc *p;
	u_long sp;
	int oonstack;
	int sig;

	oonstack = 0;
	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	sig = ksi->ksi_signo;
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	tf = td->td_frame;
	sp = tf->tf_sp + SPOFF;
	oonstack = sigonstack(sp);

	CTR4(KTR_SIG, "sendsig: td=%p (%s) catcher=%p sig=%d", td, p->p_comm,
	    catcher, sig);

	/* Make sure we have a signal trampoline to return to. */
	if (p->p_md.md_sigtramp == NULL) {
		/*
		 * No signal trampoline... kill the process.
		 */
		CTR0(KTR_SIG, "sendsig: no sigtramp");
		printf("sendsig: %s is too old, rebuild it\n", p->p_comm);
		sigexit(td, sig);
		/* NOTREACHED */
	}

	/* Save user context. */
	bzero(&sf, sizeof(sf));
	get_mcontext(td, &sf.sf_uc.uc_mcontext, 0);
	sf.sf_uc.uc_sigmask = *mask;
	sf.sf_uc.uc_stack = td->td_sigstk;
	sf.sf_uc.uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK) ?
	    ((oonstack) ? SS_ONSTACK : 0) : SS_DISABLE;

	/* Allocate and validate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sfp = (struct sigframe *)(td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size - sizeof(struct sigframe));
	} else
		sfp = (struct sigframe *)sp - 1;
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	fp = (struct frame *)sfp - 1;

	/* Translate the signal if appropriate. */
	if (p->p_sysent->sv_sigtbl && sig <= p->p_sysent->sv_sigsize)
		sig = p->p_sysent->sv_sigtbl[_SIG_IDX(sig)];

	/* Build the argument list for the signal handler. */
	tf->tf_out[0] = sig;
	tf->tf_out[2] = (register_t)&sfp->sf_uc;
	tf->tf_out[4] = (register_t)catcher;
	if (SIGISMEMBER(psp->ps_siginfo, sig)) {
		/* Signal handler installed with SA_SIGINFO. */
		tf->tf_out[1] = (register_t)&sfp->sf_si;

		/* Fill in POSIX parts. */
		sf.sf_si = ksi->ksi_info;
		sf.sf_si.si_signo = sig; /* maybe a translated signal */
	} else {
		/* Old FreeBSD-style arguments. */
		tf->tf_out[1] = ksi->ksi_code;
		tf->tf_out[3] = (register_t)ksi->ksi_addr;
	}

	/* Copy the sigframe out to the user's stack. */
	if (rwindow_save(td) != 0 || copyout(&sf, sfp, sizeof(*sfp)) != 0 ||
	    suword(&fp->fr_in[6], tf->tf_out[6]) != 0) {
		/*
		 * Something is wrong with the stack pointer.
		 * ...Kill the process.
		 */
		CTR2(KTR_SIG, "sendsig: sigexit td=%p sfp=%p", td, sfp);
		PROC_LOCK(p);
		sigexit(td, SIGILL);
		/* NOTREACHED */
	}

	tf->tf_tpc = (u_long)p->p_md.md_sigtramp;
	tf->tf_tnpc = tf->tf_tpc + 4;
	tf->tf_sp = (u_long)fp - SPOFF;

	CTR3(KTR_SIG, "sendsig: return td=%p pc=%#lx sp=%#lx", td, tf->tf_tpc,
	    tf->tf_sp);

	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}

#ifndef	_SYS_SYSPROTO_H_
struct sigreturn_args {
	ucontext_t *ucp;
};
#endif

/*
 * MPSAFE
 */
int
sigreturn(struct thread *td, struct sigreturn_args *uap)
{
	struct proc *p;
	mcontext_t *mc;
	ucontext_t uc;
	int error;

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

	mc = &uc.uc_mcontext;
	error = set_mcontext(td, mc);
	if (error != 0)
		return (error);

	PROC_LOCK(p);
	td->td_sigmask = uc.uc_sigmask;
	SIG_CANTMASK(td->td_sigmask);
	signotify(td);
	PROC_UNLOCK(p);

	CTR4(KTR_SIG, "sigreturn: return td=%p pc=%#lx sp=%#lx tstate=%#lx",
	    td, mc->mc_tpc, mc->mc_sp, mc->mc_tstate);
	return (EJUSTRETURN);
}

#ifdef COMPAT_FREEBSD4
int
freebsd4_sigreturn(struct thread *td, struct freebsd4_sigreturn_args *uap)
{

	return sigreturn(td, (struct sigreturn_args *)uap);
}
#endif

/*
 * Construct a PCB from a trapframe. This is called from kdb_trap() where
 * we want to start a backtrace from the function that caused us to enter
 * the debugger. We have the context in the trapframe, but base the trace
 * on the PCB. The PCB doesn't have to be perfect, as long as it contains
 * enough for a backtrace.
 */
void
makectx(struct trapframe *tf, struct pcb *pcb)
{

	pcb->pcb_pc = tf->tf_tpc;
	pcb->pcb_sp = tf->tf_sp;
}

int
get_mcontext(struct thread *td, mcontext_t *mc, int flags)
{
	struct trapframe *tf;
	struct pcb *pcb;

	tf = td->td_frame;
	pcb = td->td_pcb;
	bcopy(tf, mc, sizeof(*tf));
	if (flags & GET_MC_CLEAR_RET) {
		mc->mc_out[0] = 0;
		mc->mc_out[1] = 0;
	}
	mc->mc_flags = _MC_VERSION;
	critical_enter();
	if ((tf->tf_fprs & FPRS_FEF) != 0) {
		savefpctx(pcb->pcb_ufp);
		tf->tf_fprs &= ~FPRS_FEF;
		pcb->pcb_flags |= PCB_FEF;
	}
	if ((pcb->pcb_flags & PCB_FEF) != 0) {
		bcopy(pcb->pcb_ufp, mc->mc_fp, sizeof(mc->mc_fp));
		mc->mc_fprs |= FPRS_FEF;
	}
	critical_exit();
	return (0);
}

int
set_mcontext(struct thread *td, const mcontext_t *mc)
{
	struct trapframe *tf;
	struct pcb *pcb;
	uint64_t wstate;

	if (!TSTATE_SECURE(mc->mc_tstate) ||
	    (mc->mc_flags & ((1L << _MC_VERSION_BITS) - 1)) != _MC_VERSION)
		return (EINVAL);
	tf = td->td_frame;
	pcb = td->td_pcb;
	/* Make sure the windows are spilled first. */
	flushw();
	wstate = tf->tf_wstate;
	bcopy(mc, tf, sizeof(*tf));
	tf->tf_wstate = wstate;
	if ((mc->mc_fprs & FPRS_FEF) != 0) {
		tf->tf_fprs = 0;
		bcopy(mc->mc_fp, pcb->pcb_ufp, sizeof(pcb->pcb_ufp));
		pcb->pcb_flags |= PCB_FEF;
	}
	return (0);
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

/* Get current clock frequency for the given CPU ID. */
int
cpu_est_clockrate(int cpu_id, uint64_t *rate)
{
	struct pcpu *pc;

	pc = pcpu_find(cpu_id);
	if (pc == NULL || rate == NULL)
		return (EINVAL);
	*rate = pc->pc_clock;
	return (0);
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

void
cpu_idle(int busy)
{

	/* Insert code to halt (until next interrupt) for the idle loop. */
}

int
cpu_idle_wakeup(int cpu)
{

	return (0);
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

	/* TODO; */
	return (0);
}

int
ptrace_clear_single_step(struct thread *td)
{

	/* TODO; */
	return (0);
}

void
exec_setregs(struct thread *td, u_long entry, u_long stack, u_long ps_strings)
{
	struct trapframe *tf;
	struct pcb *pcb;
	struct proc *p;
	u_long sp;

	/* XXX no cpu_exec */
	p = td->td_proc;
	p->p_md.md_sigtramp = NULL;
	if (p->p_md.md_utrap != NULL) {
		utrap_free(p->p_md.md_utrap);
		p->p_md.md_utrap = NULL;
	}

	pcb = td->td_pcb;
	tf = td->td_frame;
	sp = rounddown(stack, 16);
	bzero(pcb, sizeof(*pcb));
	bzero(tf, sizeof(*tf));
	tf->tf_out[0] = stack;
	tf->tf_out[3] = p->p_sysent->sv_psstrings;
	tf->tf_out[6] = sp - SPOFF - sizeof(struct frame);
	tf->tf_tnpc = entry + 4;
	tf->tf_tpc = entry;
	tf->tf_tstate = TSTATE_IE | TSTATE_PEF | TSTATE_MM_TSO;

	td->td_retval[0] = tf->tf_out[0];
	td->td_retval[1] = tf->tf_out[1];
}

int
fill_regs(struct thread *td, struct reg *regs)
{

	bcopy(td->td_frame, regs, sizeof(*regs));
	return (0);
}

int
set_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *tf;

	if (!TSTATE_SECURE(regs->r_tstate))
		return (EINVAL);
	tf = td->td_frame;
	regs->r_wstate = tf->tf_wstate;
	bcopy(regs, tf, sizeof(*regs));
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
	bcopy(pcb->pcb_ufp, fpregs->fr_regs, sizeof(fpregs->fr_regs));
	fpregs->fr_fsr = tf->tf_fsr;
	fpregs->fr_gsr = tf->tf_gsr;
	return (0);
}

int
set_fpregs(struct thread *td, struct fpreg *fpregs)
{
	struct trapframe *tf;
	struct pcb *pcb;

	pcb = td->td_pcb;
	tf = td->td_frame;
	tf->tf_fprs &= ~FPRS_FEF;
	bcopy(fpregs->fr_regs, pcb->pcb_ufp, sizeof(pcb->pcb_ufp));
	tf->tf_fsr = fpregs->fr_fsr;
	tf->tf_gsr = fpregs->fr_gsr;
	return (0);
}

struct md_utrap *
utrap_alloc(void)
{
	struct md_utrap *ut;

	ut = malloc(sizeof(struct md_utrap), M_SUBPROC, M_WAITOK | M_ZERO);
	ut->ut_refcnt = 1;
	return (ut);
}

void
utrap_free(struct md_utrap *ut)
{
	int refcnt;

	if (ut == NULL)
		return;
	mtx_pool_lock(mtxpool_sleep, ut);
	ut->ut_refcnt--;
	refcnt = ut->ut_refcnt;
	mtx_pool_unlock(mtxpool_sleep, ut);
	if (refcnt == 0)
		free(ut, M_SUBPROC);
}

struct md_utrap *
utrap_hold(struct md_utrap *ut)
{

	if (ut == NULL)
		return (NULL);
	mtx_pool_lock(mtxpool_sleep, ut);
	ut->ut_refcnt++;
	mtx_pool_unlock(mtxpool_sleep, ut);
	return (ut);
}
