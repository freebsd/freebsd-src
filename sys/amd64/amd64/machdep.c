/*-
 * Copyright (c) 2003 Peter Wemm.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_atalk.h"
#include "opt_atpic.h"
#include "opt_compat.h"
#include "opt_cpu.h"
#include "opt_ddb.h"
#include "opt_inet.h"
#include "opt_ipx.h"
#include "opt_isa.h"
#include "opt_kstack_pages.h"
#include "opt_maxmem.h"
#include "opt_msgbuf.h"
#include "opt_perfmon.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/cons.h>
#include <sys/cpu.h>
#include <sys/eventhandler.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/ucontext.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vm_param.h>

#ifdef DDB
#ifndef KDB
#error KDB must be enabled in order for DDB to work!
#endif
#endif
#include <ddb/ddb.h>

#include <net/netisr.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/cputypes.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/pc/bios.h>
#include <machine/pcb.h>
#include <machine/proc.h>
#include <machine/reg.h>
#include <machine/sigframe.h>
#include <machine/specialreg.h>
#ifdef PERFMON
#include <machine/perfmon.h>
#endif
#include <machine/tss.h>
#ifdef SMP
#include <machine/smp.h>
#endif

#include <amd64/isa/icu.h>

#include <isa/isareg.h>
#include <isa/rtc.h>

/* Sanity check for __curthread() */
CTASSERT(offsetof(struct pcpu, pc_curthread) == 0);

extern u_int64_t hammer_time(u_int64_t, u_int64_t);
extern void dblfault_handler(void);

extern void printcpuinfo(void);	/* XXX header file */
extern void identify_cpu(void);
extern void panicifcpuunsupported(void);

#define	CS_SECURE(cs)		(ISPL(cs) == SEL_UPL)
#define	EFL_SECURE(ef, oef)	((((ef) ^ (oef)) & ~PSL_USERCHANGE) == 0)

static void cpu_startup(void *);
static void get_fpcontext(struct thread *td, mcontext_t *mcp);
static int  set_fpcontext(struct thread *td, const mcontext_t *mcp);
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL)

#ifdef DDB
extern vm_offset_t ksym_start, ksym_end;
#endif

int	_udatasel, _ucodesel, _ucode32sel;

int cold = 1;

long Maxmem = 0;
long realmem = 0;

vm_paddr_t phys_avail[20];

/* must be 2 less so 0 0 can signal end of chunks */
#define PHYS_AVAIL_ARRAY_END ((sizeof(phys_avail) / sizeof(vm_offset_t)) - 2)

struct kva_md_info kmi;

static struct trapframe proc0_tf;
struct region_descriptor r_gdt, r_idt;

struct pcpu __pcpu[MAXCPU];

struct mtx icu_lock;

struct mem_range_softc mem_range_softc;

static void
cpu_startup(dummy)
	void *dummy;
{
	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	startrtclock();
	printcpuinfo();
	panicifcpuunsupported();
#ifdef PERFMON
	perfmon_init();
#endif
	printf("real memory  = %ju (%ju MB)\n", ptoa((uintmax_t)Maxmem),
	    ptoa((uintmax_t)Maxmem) / 1048576);
	realmem = Maxmem;
	/*
	 * Display any holes after the first chunk of extended memory.
	 */
	if (bootverbose) {
		int indx;

		printf("Physical memory chunk(s):\n");
		for (indx = 0; phys_avail[indx + 1] != 0; indx += 2) {
			vm_paddr_t size;

			size = phys_avail[indx + 1] - phys_avail[indx];
			printf(
			    "0x%016jx - 0x%016jx, %ju bytes (%ju pages)\n",
			    (uintmax_t)phys_avail[indx],
			    (uintmax_t)phys_avail[indx + 1] - 1,
			    (uintmax_t)size, (uintmax_t)size / PAGE_SIZE);
		}
	}

	vm_ksubmap_init(&kmi);

	printf("avail memory = %ju (%ju MB)\n",
	    ptoa((uintmax_t)cnt.v_free_count),
	    ptoa((uintmax_t)cnt.v_free_count) / 1048576);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
	vm_pager_bufferinit();

	cpu_setregs();
}

/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored
 * at top to call routine, followed by kcall
 * to sigreturn routine below.  After sigreturn
 * resets the signal mask, the stack, and the
 * frame pointer, it returns to the user
 * specified pc, psl.
 */
void
sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig;
	sigset_t *mask;
	u_long code;
{
	struct sigframe sf, *sfp;
	struct proc *p;
	struct thread *td;
	struct sigacts *psp;
	char *sp;
	struct trapframe *regs;
	int oonstack;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	regs = td->td_frame;
	oonstack = sigonstack(regs->tf_rsp);

	/* Save user context. */
	bzero(&sf, sizeof(sf));
	sf.sf_uc.uc_sigmask = *mask;
	sf.sf_uc.uc_stack = td->td_sigstk;
	sf.sf_uc.uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK)
	    ? ((oonstack) ? SS_ONSTACK : 0) : SS_DISABLE;
	sf.sf_uc.uc_mcontext.mc_onstack = (oonstack) ? 1 : 0;
	bcopy(regs, &sf.sf_uc.uc_mcontext.mc_rdi, sizeof(*regs));
	sf.sf_uc.uc_mcontext.mc_len = sizeof(sf.sf_uc.uc_mcontext); /* magic */
	get_fpcontext(td, &sf.sf_uc.uc_mcontext);
	fpstate_drop(td);

	/* Allocate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sp = td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size - sizeof(struct sigframe);
#if defined(COMPAT_43)
		td->td_sigstk.ss_flags |= SS_ONSTACK;
#endif
	} else
		sp = (char *)regs->tf_rsp - sizeof(struct sigframe) - 128;
	/* Align to 16 bytes. */
	sfp = (struct sigframe *)((unsigned long)sp & ~0xFul);

	/* Translate the signal if appropriate. */
	if (p->p_sysent->sv_sigtbl && sig <= p->p_sysent->sv_sigsize)
		sig = p->p_sysent->sv_sigtbl[_SIG_IDX(sig)];

	/* Build the argument list for the signal handler. */
	regs->tf_rdi = sig;			/* arg 1 in %rdi */
	regs->tf_rdx = (register_t)&sfp->sf_uc;	/* arg 3 in %rdx */
	if (SIGISMEMBER(psp->ps_siginfo, sig)) {
		/* Signal handler installed with SA_SIGINFO. */
		regs->tf_rsi = (register_t)&sfp->sf_si;	/* arg 2 in %rsi */
		sf.sf_ahu.sf_action = (__siginfohandler_t *)catcher;

		/* Fill in POSIX parts */
		sf.sf_si.si_signo = sig;
		sf.sf_si.si_code = code;
		regs->tf_rcx = regs->tf_addr;	/* arg 4 in %rcx */
	} else {
		/* Old FreeBSD-style arguments. */
		regs->tf_rsi = code;		/* arg 2 in %rsi */
		regs->tf_rcx = regs->tf_addr;	/* arg 4 in %rcx */
		sf.sf_ahu.sf_handler = catcher;
	}
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	/*
	 * Copy the sigframe out to the user's stack.
	 */
	if (copyout(&sf, sfp, sizeof(*sfp)) != 0) {
#ifdef DEBUG
		printf("process %ld has trashed its stack\n", (long)p->p_pid);
#endif
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	regs->tf_rsp = (long)sfp;
	regs->tf_rip = PS_STRINGS - *(p->p_sysent->sv_szsigcode);
	regs->tf_rflags &= ~PSL_T;
	regs->tf_cs = _ucodesel;
	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}

/*
 * Build siginfo_t for SA thread
 */
void
cpu_thread_siginfo(int sig, u_long code, siginfo_t *si)
{
	struct proc *p;
	struct thread *td;
	struct trapframe *regs;

	td = curthread;
	p = td->td_proc;
	regs = td->td_frame;
	PROC_LOCK_ASSERT(p, MA_OWNED);

	bzero(si, sizeof(*si));
	si->si_signo = sig;
	si->si_code = code;
	si->si_addr = (void *)regs->tf_addr;
	/* XXXKSE fill other fields */
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * state to gain improper privileges.
 *
 * MPSAFE
 */
int
sigreturn(td, uap)
	struct thread *td;
	struct sigreturn_args /* {
		const __ucontext *sigcntxp;
	} */ *uap;
{
	ucontext_t uc;
	struct proc *p = td->td_proc;
	struct trapframe *regs;
	const ucontext_t *ucp;
	long rflags;
	int cs, error, ret;

	error = copyin(uap->sigcntxp, &uc, sizeof(uc));
	if (error != 0)
		return (error);
	ucp = &uc;
	regs = td->td_frame;
	rflags = ucp->uc_mcontext.mc_rflags;
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
		printf("sigreturn: rflags = 0x%lx\n", rflags);
		return (EINVAL);
	}

	/*
	 * Don't allow users to load a valid privileged %cs.  Let the
	 * hardware check for invalid selectors, excess privilege in
	 * other selectors, invalid %eip's and invalid %esp's.
	 */
	cs = ucp->uc_mcontext.mc_cs;
	if (!CS_SECURE(cs)) {
		printf("sigreturn: cs = 0x%x\n", cs);
		trapsignal(td, SIGBUS, T_PROTFLT);
		return (EINVAL);
	}

	ret = set_fpcontext(td, &ucp->uc_mcontext);
	if (ret != 0)
		return (ret);
	bcopy(&ucp->uc_mcontext.mc_rdi, regs, sizeof(*regs));

	PROC_LOCK(p);
#if defined(COMPAT_43)
	if (ucp->uc_mcontext.mc_onstack & 1)
		td->td_sigstk.ss_flags |= SS_ONSTACK;
	else
		td->td_sigstk.ss_flags &= ~SS_ONSTACK;
#endif

	td->td_sigmask = ucp->uc_sigmask;
	SIG_CANTMASK(td->td_sigmask);
	signotify(td);
	PROC_UNLOCK(p);
	td->td_pcb->pcb_flags |= PCB_FULLCTX;
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
 * Machine dependent boot() routine
 *
 * I haven't seen anything to put here yet
 * Possibly some stuff might be grafted back here from boot()
 */
void
cpu_boot(int howto)
{
}

/* Get current clock frequency for the given cpu id. */
int
cpu_est_clockrate(int cpu_id, uint64_t *rate)
{
	register_t reg;
	uint64_t tsc1, tsc2;

	if (pcpu_find(cpu_id) == NULL || rate == NULL)
		return (EINVAL);

	/* If we're booting, trust the rate calibrated moments ago. */
	if (cold) {
		*rate = tsc_freq;
		return (0);
	}

#ifdef SMP
	/* Schedule ourselves on the indicated cpu. */
	mtx_lock_spin(&sched_lock);
	sched_bind(curthread, cpu_id);
	mtx_unlock_spin(&sched_lock);
#endif

	/* Calibrate by measuring a short delay. */
	reg = intr_disable();
	tsc1 = rdtsc();
	DELAY(1000);
	tsc2 = rdtsc();
	intr_restore(reg);

#ifdef SMP
	mtx_lock_spin(&sched_lock);
	sched_unbind(curthread);
	mtx_unlock_spin(&sched_lock);
#endif

	/*
	 * Calculate the difference in readings, convert to Mhz, and
	 * subtract 0.5% of the total.  Empirical testing has shown that
	 * overhead in DELAY() works out to approximately this value.
	 */
	tsc2 -= tsc1;
	*rate = tsc2 * 1000 - tsc2 * 5;
	return (0);
}

/*
 * Shutdown the CPU as much as possible
 */
void
cpu_halt(void)
{
	for (;;)
		__asm__ ("hlt");
}

/*
 * Hook to idle the CPU when possible.  In the SMP case we default to
 * off because a halted cpu will not currently pick up a new thread in the
 * run queue until the next timer tick.  If turned on this will result in
 * approximately a 4.2% loss in real time performance in buildworld tests
 * (but improves user and sys times oddly enough), and saves approximately
 * 5% in power consumption on an idle machine (tests w/2xCPU 1.1GHz P3).
 *
 * XXX we need to have a cpu mask of idle cpus and generate an IPI or
 * otherwise generate some sort of interrupt to wake up cpus sitting in HLT.
 * Then we can have our cake and eat it too.
 *
 * XXX I'm turning it on for SMP as well by default for now.  It seems to
 * help lock contention somewhat, and this is critical for HTT. -Peter
 */
static int	cpu_idle_hlt = 1;
SYSCTL_INT(_machdep, OID_AUTO, cpu_idle_hlt, CTLFLAG_RW,
    &cpu_idle_hlt, 0, "Idle loop HLT enable");

static void
cpu_idle_default(void)
{
	/*
	 * we must absolutely guarentee that hlt is the
	 * absolute next instruction after sti or we
	 * introduce a timing window.
	 */
	__asm __volatile("sti; hlt");
}

/*
 * Note that we have to be careful here to avoid a race between checking
 * sched_runnable() and actually halting.  If we don't do this, we may waste
 * the time between calling hlt and the next interrupt even though there
 * is a runnable process.
 */
void
cpu_idle(void)
{

#ifdef SMP
	if (mp_grab_cpu_hlt())
		return;
#endif
	if (cpu_idle_hlt) {
		disable_intr();
  		if (sched_runnable())
			enable_intr();
		else
			(*cpu_idle_hook)();
	}
}

/* Other subsystems (e.g., ACPI) can hook this later. */
void (*cpu_idle_hook)(void) = cpu_idle_default;

/*
 * Clear registers on exec
 */
void
exec_setregs(td, entry, stack, ps_strings)
	struct thread *td;
	u_long entry;
	u_long stack;
	u_long ps_strings;
{
	struct trapframe *regs = td->td_frame;
	struct pcb *pcb = td->td_pcb;
	
	wrmsr(MSR_FSBASE, 0);
	wrmsr(MSR_KGSBASE, 0);	/* User value while we're in the kernel */
	pcb->pcb_fsbase = 0;
	pcb->pcb_gsbase = 0;
	load_ds(_udatasel);
	load_es(_udatasel);
	load_fs(_udatasel);
	load_gs(_udatasel);
	pcb->pcb_ds = _udatasel;
	pcb->pcb_es = _udatasel;
	pcb->pcb_fs = _udatasel;
	pcb->pcb_gs = _udatasel;

	bzero((char *)regs, sizeof(struct trapframe));
	regs->tf_rip = entry;
	regs->tf_rsp = ((stack - 8) & ~0xFul) + 8;
	regs->tf_rdi = stack;		/* argv */
	regs->tf_rflags = PSL_USER | (regs->tf_rflags & PSL_T);
	regs->tf_ss = _udatasel;
	regs->tf_cs = _ucodesel;

	/*
	 * Reset the hardware debug registers if they were in use.
	 * They won't have any meaning for the newly exec'd process.
	 */
	if (pcb->pcb_flags & PCB_DBREGS) {
		pcb->pcb_dr0 = 0;
		pcb->pcb_dr1 = 0;
		pcb->pcb_dr2 = 0;
		pcb->pcb_dr3 = 0;
		pcb->pcb_dr6 = 0;
		pcb->pcb_dr7 = 0;
		if (pcb == PCPU_GET(curpcb)) {
			/*
			 * Clear the debug registers on the running
			 * CPU, otherwise they will end up affecting
			 * the next process we switch to.
			 */
			reset_dbregs();
		}
		pcb->pcb_flags &= ~PCB_DBREGS;
	}

	/*
	 * Drop the FP state if we hold it, so that the process gets a
	 * clean FP state if it uses the FPU again.
	 */
	fpstate_drop(td);
}

void
cpu_setregs(void)
{
	register_t cr0;

	cr0 = rcr0();
	/*
	 * CR0_MP, CR0_NE and CR0_TS are also set by npx_probe() for the
	 * BSP.  See the comments there about why we set them.
	 */
	cr0 |= CR0_MP | CR0_NE | CR0_TS | CR0_WP | CR0_AM;
	load_cr0(cr0);
}

static int
sysctl_machdep_adjkerntz(SYSCTL_HANDLER_ARGS)
{
	int error;
	error = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2,
		req);
	if (!error && req->newptr)
		resettodr();
	return (error);
}

SYSCTL_PROC(_machdep, CPU_ADJKERNTZ, adjkerntz, CTLTYPE_INT|CTLFLAG_RW,
	&adjkerntz, 0, sysctl_machdep_adjkerntz, "I", "");

SYSCTL_INT(_machdep, CPU_DISRTCSET, disable_rtc_set,
	CTLFLAG_RW, &disable_rtc_set, 0, "");

SYSCTL_INT(_machdep, CPU_WALLCLOCK, wall_cmos_clock,
	CTLFLAG_RW, &wall_cmos_clock, 0, "");

/*
 * Initialize 386 and configure to run kernel
 */

/*
 * Initialize segments & interrupt table
 */

struct user_segment_descriptor gdt[NGDT * MAXCPU];/* global descriptor table */
static struct gate_descriptor idt0[NIDT];
struct gate_descriptor *idt = &idt0[0];	/* interrupt descriptor table */

static char dblfault_stack[PAGE_SIZE] __aligned(16);

struct amd64tss common_tss[MAXCPU];

/* software prototypes -- in more palatable form */
struct soft_segment_descriptor gdt_segs[] = {
/* GNULL_SEL	0 Null Descriptor */
{	0x0,			/* segment base address  */
	0x0,			/* length */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0,			/* long */
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
/* GCODE_SEL	1 Code Descriptor for kernel */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMERA,		/* segment type */
	SEL_KPL,		/* segment descriptor priority level */
	1,			/* segment descriptor present */
	1,			/* long */
	0,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
/* GDATA_SEL	2 Data Descriptor for kernel */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMRWA,		/* segment type */
	SEL_KPL,		/* segment descriptor priority level */
	1,			/* segment descriptor present */
	1,			/* long */
	0,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
/* GUCODE32_SEL	3 32 bit Code Descriptor for user */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMERA,		/* segment type */
	SEL_UPL,		/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0,			/* long */
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
/* GUDATA_SEL	4 32/64 bit Data Descriptor for user */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMRWA,		/* segment type */
	SEL_UPL,		/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0,			/* long */
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
/* GUCODE_SEL	5 64 bit Code Descriptor for user */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMERA,		/* segment type */
	SEL_UPL,		/* segment descriptor priority level */
	1,			/* segment descriptor present */
	1,			/* long */
	0,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
/* GPROC0_SEL	6 Proc 0 Tss Descriptor */
{
	0x0,			/* segment base address */
	sizeof(struct amd64tss)-1,/* length - all address space */
	SDT_SYSTSS,		/* segment type */
	SEL_KPL,		/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0,			/* long */
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
/* Actually, the TSS is a system descriptor which is double size */
{	0x0,			/* segment base address  */
	0x0,			/* length */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0,			/* long */
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
};

void
setidt(idx, func, typ, dpl, ist)
	int idx;
	inthand_t *func;
	int typ;
	int dpl;
	int ist;
{
	struct gate_descriptor *ip;

	ip = idt + idx;
	ip->gd_looffset = (uintptr_t)func;
	ip->gd_selector = GSEL(GCODE_SEL, SEL_KPL);
	ip->gd_ist = ist;
	ip->gd_xx = 0;
	ip->gd_type = typ;
	ip->gd_dpl = dpl;
	ip->gd_p = 1;
	ip->gd_hioffset = ((uintptr_t)func)>>16 ;
}

#define	IDTVEC(name)	__CONCAT(X,name)

extern inthand_t
	IDTVEC(div), IDTVEC(dbg), IDTVEC(nmi), IDTVEC(bpt), IDTVEC(ofl),
	IDTVEC(bnd), IDTVEC(ill), IDTVEC(dna), IDTVEC(fpusegm),
	IDTVEC(tss), IDTVEC(missing), IDTVEC(stk), IDTVEC(prot),
	IDTVEC(page), IDTVEC(mchk), IDTVEC(rsvd), IDTVEC(fpu), IDTVEC(align),
	IDTVEC(xmm), IDTVEC(dblfault),
	IDTVEC(fast_syscall), IDTVEC(fast_syscall32);

void
sdtossd(sd, ssd)
	struct user_segment_descriptor *sd;
	struct soft_segment_descriptor *ssd;
{

	ssd->ssd_base  = (sd->sd_hibase << 24) | sd->sd_lobase;
	ssd->ssd_limit = (sd->sd_hilimit << 16) | sd->sd_lolimit;
	ssd->ssd_type  = sd->sd_type;
	ssd->ssd_dpl   = sd->sd_dpl;
	ssd->ssd_p     = sd->sd_p;
	ssd->ssd_long  = sd->sd_long;
	ssd->ssd_def32 = sd->sd_def32;
	ssd->ssd_gran  = sd->sd_gran;
}

void
ssdtosd(ssd, sd)
	struct soft_segment_descriptor *ssd;
	struct user_segment_descriptor *sd;
{

	sd->sd_lobase = (ssd->ssd_base) & 0xffffff;
	sd->sd_hibase = (ssd->ssd_base >> 24) & 0xff;
	sd->sd_lolimit = (ssd->ssd_limit) & 0xffff;
	sd->sd_hilimit = (ssd->ssd_limit >> 16) & 0xf;
	sd->sd_type  = ssd->ssd_type;
	sd->sd_dpl   = ssd->ssd_dpl;
	sd->sd_p     = ssd->ssd_p;
	sd->sd_long  = ssd->ssd_long;
	sd->sd_def32 = ssd->ssd_def32;
	sd->sd_gran  = ssd->ssd_gran;
}

void
ssdtosyssd(ssd, sd)
	struct soft_segment_descriptor *ssd;
	struct system_segment_descriptor *sd;
{

	sd->sd_lobase = (ssd->ssd_base) & 0xffffff;
	sd->sd_hibase = (ssd->ssd_base >> 24) & 0xfffffffffful;
	sd->sd_lolimit = (ssd->ssd_limit) & 0xffff;
	sd->sd_hilimit = (ssd->ssd_limit >> 16) & 0xf;
	sd->sd_type  = ssd->ssd_type;
	sd->sd_dpl   = ssd->ssd_dpl;
	sd->sd_p     = ssd->ssd_p;
	sd->sd_gran  = ssd->ssd_gran;
}

#if !defined(DEV_ATPIC) && defined(DEV_ISA)
#include <isa/isavar.h>
u_int
isa_irq_pending(void)
{

	return (0);
}
#endif

#define PHYSMAP_SIZE	(2 * 8)

u_int basemem;

/*
 * Populate the (physmap) array with base/bound pairs describing the
 * available physical memory in the system, then test this memory and
 * build the phys_avail array describing the actually-available memory.
 *
 * If we cannot accurately determine the physical memory map, then use
 * value from the 0xE801 call, and failing that, the RTC.
 *
 * Total memory size may be set by the kernel environment variable
 * hw.physmem or the compile-time define MAXMEM.
 *
 * XXX first should be vm_paddr_t.
 */
static void
getmemsize(caddr_t kmdp, u_int64_t first)
{
	int i, physmap_idx, pa_indx;
	vm_paddr_t pa, physmap[PHYSMAP_SIZE];
	pt_entry_t *pte;
	char *cp;
	struct bios_smap *smapbase, *smap, *smapend;
	u_int32_t smapsize;
	quad_t dcons_addr, dcons_size;

	bzero(physmap, sizeof(physmap));
	basemem = 0;
	physmap_idx = 0;

	/*
	 * get memory map from INT 15:E820, kindly supplied by the loader.
	 *
	 * subr_module.c says:
	 * "Consumer may safely assume that size value precedes data."
	 * ie: an int32_t immediately precedes smap.
	 */
	smapbase = (struct bios_smap *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_SMAP);
	if (smapbase == NULL)
		panic("No BIOS smap info from loader!");

	smapsize = *((u_int32_t *)smapbase - 1);
	smapend = (struct bios_smap *)((uintptr_t)smapbase + smapsize);

	for (smap = smapbase; smap < smapend; smap++) {
		if (boothowto & RB_VERBOSE)
			printf("SMAP type=%02x base=%016lx len=%016lx\n",
			    smap->type, smap->base, smap->length);

		if (smap->type != 0x01)
			continue;

		if (smap->length == 0)
			continue;

		for (i = 0; i <= physmap_idx; i += 2) {
			if (smap->base < physmap[i + 1]) {
				if (boothowto & RB_VERBOSE)
					printf(
	"Overlapping or non-montonic memory region, ignoring second region\n");
				goto next_run;
			}
		}

		if (smap->base == physmap[physmap_idx + 1]) {
			physmap[physmap_idx + 1] += smap->length;
next_run:
			continue;
		}

		physmap_idx += 2;
		if (physmap_idx == PHYSMAP_SIZE) {
			printf(
		"Too many segments in the physical address map, giving up\n");
			break;
		}
		physmap[physmap_idx] = smap->base;
		physmap[physmap_idx + 1] = smap->base + smap->length;
	}

	/*
	 * Find the 'base memory' segment for SMP
	 */
	basemem = 0;
	for (i = 0; i <= physmap_idx; i += 2) {
		if (physmap[i] == 0x00000000) {
			basemem = physmap[i + 1] / 1024;
			break;
		}
	}
	if (basemem == 0)
		panic("BIOS smap did not include a basemem segment!");

#ifdef SMP
	/* make hole for AP bootstrap code */
	physmap[1] = mp_bootaddress(physmap[1] / 1024);
#endif

	/*
	 * Maxmem isn't the "maximum memory", it's one larger than the
	 * highest page of the physical address space.  It should be
	 * called something like "Maxphyspage".  We may adjust this
	 * based on ``hw.physmem'' and the results of the memory test.
	 */
	Maxmem = atop(physmap[physmap_idx + 1]);

#ifdef MAXMEM
	Maxmem = MAXMEM / 4;
#endif

	/*
	 * hw.physmem is a size in bytes; we also allow k, m, and g suffixes
	 * for the appropriate modifiers.  This overrides MAXMEM.
	 */
	cp = getenv("hw.physmem");
	if (cp != NULL) {
		u_int64_t AllowMem, sanity;
		char *ep;

		sanity = AllowMem = strtouq(cp, &ep, 0);
		if ((ep != cp) && (*ep != 0)) {
			switch(*ep) {
			case 'g':
			case 'G':
				AllowMem <<= 10;
			case 'm':
			case 'M':
				AllowMem <<= 10;
			case 'k':
			case 'K':
				AllowMem <<= 10;
				break;
			default:
				AllowMem = sanity = 0;
			}
			if (AllowMem < sanity)
				AllowMem = 0;
		}
		if (AllowMem == 0)
			printf("Ignoring invalid memory size of '%s'\n", cp);
		else
			Maxmem = atop(AllowMem);
		freeenv(cp);
	}

	if (atop(physmap[physmap_idx + 1]) != Maxmem &&
	    (boothowto & RB_VERBOSE))
		printf("Physical memory use set to %ldK\n", Maxmem * 4);

	/*
	 * If Maxmem has been increased beyond what the system has detected,
	 * extend the last memory segment to the new limit.
	 */
	if (atop(physmap[physmap_idx + 1]) < Maxmem)
		physmap[physmap_idx + 1] = ptoa((vm_paddr_t)Maxmem);

	/* call pmap initialization to make new kernel address space */
	pmap_bootstrap(&first);

	/*
	 * Size up each available chunk of physical memory.
	 */
	physmap[0] = PAGE_SIZE;		/* mask off page 0 */
	pa_indx = 0;
	phys_avail[pa_indx++] = physmap[0];
	phys_avail[pa_indx] = physmap[0];
	pte = CMAP1;

	/*
	 * Get dcons buffer address
	 */
	if (getenv_quad("dcons.addr", &dcons_addr) == 0 ||
	    getenv_quad("dcons.size", &dcons_size) == 0)
		dcons_addr = 0;

	/*
	 * physmap is in bytes, so when converting to page boundaries,
	 * round up the start address and round down the end address.
	 */
	for (i = 0; i <= physmap_idx; i += 2) {
		vm_paddr_t end;

		end = ptoa((vm_paddr_t)Maxmem);
		if (physmap[i + 1] < end)
			end = trunc_page(physmap[i + 1]);
		for (pa = round_page(physmap[i]); pa < end; pa += PAGE_SIZE) {
			int tmp, page_bad;
			int *ptr = (int *)CADDR1;

			/*
			 * block out kernel memory as not available.
			 */
			if (pa >= 0x100000 && pa < first)
				continue;

 			/*
 			 * block out dcons buffer
 			 */
 			if (dcons_addr > 0
 			    && pa >= trunc_page(dcons_addr)
 			    && pa < dcons_addr + dcons_size)
 				continue;

			page_bad = FALSE;

			/*
			 * map page into kernel: valid, read/write,non-cacheable
			 */
			*pte = pa | PG_V | PG_RW | PG_N;
			invltlb();

			tmp = *(int *)ptr;
			/*
			 * Test for alternating 1's and 0's
			 */
			*(volatile int *)ptr = 0xaaaaaaaa;
			if (*(volatile int *)ptr != 0xaaaaaaaa)
				page_bad = TRUE;
			/*
			 * Test for alternating 0's and 1's
			 */
			*(volatile int *)ptr = 0x55555555;
			if (*(volatile int *)ptr != 0x55555555)
				page_bad = TRUE;
			/*
			 * Test for all 1's
			 */
			*(volatile int *)ptr = 0xffffffff;
			if (*(volatile int *)ptr != 0xffffffff)
				page_bad = TRUE;
			/*
			 * Test for all 0's
			 */
			*(volatile int *)ptr = 0x0;
			if (*(volatile int *)ptr != 0x0)
				page_bad = TRUE;
			/*
			 * Restore original value.
			 */
			*(int *)ptr = tmp;

			/*
			 * Adjust array of valid/good pages.
			 */
			if (page_bad == TRUE)
				continue;
			/*
			 * If this good page is a continuation of the
			 * previous set of good pages, then just increase
			 * the end pointer. Otherwise start a new chunk.
			 * Note that "end" points one higher than end,
			 * making the range >= start and < end.
			 * If we're also doing a speculative memory
			 * test and we at or past the end, bump up Maxmem
			 * so that we keep going. The first bad page
			 * will terminate the loop.
			 */
			if (phys_avail[pa_indx] == pa) {
				phys_avail[pa_indx] += PAGE_SIZE;
			} else {
				pa_indx++;
				if (pa_indx == PHYS_AVAIL_ARRAY_END) {
					printf(
		"Too many holes in the physical address space, giving up\n");
					pa_indx--;
					break;
				}
				phys_avail[pa_indx++] = pa;	/* start */
				phys_avail[pa_indx] = pa + PAGE_SIZE; /* end */
			}
			physmem++;
		}
	}
	*pte = 0;
	invltlb();

	/*
	 * XXX
	 * The last chunk must contain at least one page plus the message
	 * buffer to avoid complicating other code (message buffer address
	 * calculation, etc.).
	 */
	while (phys_avail[pa_indx - 1] + PAGE_SIZE +
	    round_page(MSGBUF_SIZE) >= phys_avail[pa_indx]) {
		physmem -= atop(phys_avail[pa_indx] - phys_avail[pa_indx - 1]);
		phys_avail[pa_indx--] = 0;
		phys_avail[pa_indx--] = 0;
	}

	Maxmem = atop(phys_avail[pa_indx]);

	/* Trim off space for the message buffer. */
	phys_avail[pa_indx] -= round_page(MSGBUF_SIZE);

	avail_end = phys_avail[pa_indx];
}

u_int64_t
hammer_time(u_int64_t modulep, u_int64_t physfree)
{
	caddr_t kmdp;
	int gsel_tss, off, x;
	struct pcpu *pc;
	u_int64_t msr;
	char *env;

#ifdef DEV_ISA
	/* Preemptively mask the atpics and leave them shut down */
	outb(IO_ICU1 + ICU_IMR_OFFSET, 0xff);
	outb(IO_ICU2 + ICU_IMR_OFFSET, 0xff);
#else
#error "have you forgotten the isa device?";
#endif

	thread0.td_kstack = physfree + KERNBASE;
	bzero((void *)thread0.td_kstack, KSTACK_PAGES * PAGE_SIZE);
	physfree += KSTACK_PAGES * PAGE_SIZE;
	thread0.td_pcb = (struct pcb *)
	   (thread0.td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;

	/*
 	 * This may be done better later if it gets more high level
 	 * components in it. If so just link td->td_proc here.
	 */
	proc_linkup(&proc0, &ksegrp0, &thread0);

	preload_metadata = (caddr_t)(uintptr_t)(modulep + KERNBASE);
	preload_bootstrap_relocate(KERNBASE);
	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		kmdp = preload_search_by_type("elf64 kernel");
	boothowto = MD_FETCH(kmdp, MODINFOMD_HOWTO, int);
	kern_envp = MD_FETCH(kmdp, MODINFOMD_ENVP, char *) + KERNBASE;
#ifdef DDB
	ksym_start = MD_FETCH(kmdp, MODINFOMD_SSYM, uintptr_t);
	ksym_end = MD_FETCH(kmdp, MODINFOMD_ESYM, uintptr_t);
#endif

	/* Init basic tunables, hz etc */
	init_param1();

	/*
	 * make gdt memory segments
	 */
	gdt_segs[GPROC0_SEL].ssd_base = (uintptr_t)&common_tss[0];

	for (x = 0; x < NGDT; x++) {
		if (x != GPROC0_SEL && x != (GPROC0_SEL + 1))
			ssdtosd(&gdt_segs[x], &gdt[x]);
	}
	ssdtosyssd(&gdt_segs[GPROC0_SEL],
	    (struct system_segment_descriptor *)&gdt[GPROC0_SEL]);

	r_gdt.rd_limit = NGDT * sizeof(gdt[0]) - 1;
	r_gdt.rd_base =  (long) gdt;
	lgdt(&r_gdt);
	pc = &__pcpu[0];

	wrmsr(MSR_FSBASE, 0);		/* User value */
	wrmsr(MSR_GSBASE, (u_int64_t)pc);
	wrmsr(MSR_KGSBASE, 0);		/* User value while in the kernel */

	pcpu_init(pc, 0, sizeof(struct pcpu));
	PCPU_SET(prvspace, pc);
	PCPU_SET(curthread, &thread0);
	PCPU_SET(curpcb, thread0.td_pcb);
	PCPU_SET(tssp, &common_tss[0]);

	/*
	 * Initialize mutexes.
	 *
	 * icu_lock: in order to allow an interrupt to occur in a critical
	 * 	     section, to set pcpu->ipending (etc...) properly, we
	 *	     must be able to get the icu lock, so it can't be
	 *	     under witness.
	 */
	mutex_init();
	mtx_init(&clock_lock, "clk", NULL, MTX_SPIN);
	mtx_init(&icu_lock, "icu", NULL, MTX_SPIN | MTX_NOWITNESS);

	/* exceptions */
	for (x = 0; x < NIDT; x++)
		setidt(x, &IDTVEC(rsvd), SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_DE, &IDTVEC(div),  SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_DB, &IDTVEC(dbg),  SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_NMI, &IDTVEC(nmi),  SDT_SYSIGT, SEL_KPL, 0);
 	setidt(IDT_BP, &IDTVEC(bpt),  SDT_SYSIGT, SEL_UPL, 0);
	setidt(IDT_OF, &IDTVEC(ofl),  SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_BR, &IDTVEC(bnd),  SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_UD, &IDTVEC(ill),  SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_NM, &IDTVEC(dna),  SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_DF, &IDTVEC(dblfault), SDT_SYSIGT, SEL_KPL, 1);
	setidt(IDT_FPUGP, &IDTVEC(fpusegm),  SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_TS, &IDTVEC(tss),  SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_NP, &IDTVEC(missing),  SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_SS, &IDTVEC(stk),  SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_GP, &IDTVEC(prot),  SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_PF, &IDTVEC(page),  SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_MF, &IDTVEC(fpu),  SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_AC, &IDTVEC(align), SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_MC, &IDTVEC(mchk),  SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_XF, &IDTVEC(xmm), SDT_SYSIGT, SEL_KPL, 0);

	r_idt.rd_limit = sizeof(idt0) - 1;
	r_idt.rd_base = (long) idt;
	lidt(&r_idt);

	/*
	 * Initialize the console before we print anything out.
	 */
	cninit();

#ifdef DEV_ATPIC
	elcr_probe();
	atpic_startup();
#endif

	kdb_init();

#ifdef KDB
	if (boothowto & RB_KDB)
		kdb_enter("Boot flags requested debugger");
#endif

	identify_cpu();		/* Final stage of CPU initialization */
	initializecpu();	/* Initialize CPU registers */

	/* make an initial tss so cpu can get interrupt stack on syscall! */
	common_tss[0].tss_rsp0 = thread0.td_kstack + \
	    KSTACK_PAGES * PAGE_SIZE - sizeof(struct pcb);
	/* Ensure the stack is aligned to 16 bytes */
	common_tss[0].tss_rsp0 &= ~0xFul;
	PCPU_SET(rsp0, common_tss[0].tss_rsp0);

	/* doublefault stack space, runs on ist1 */
	common_tss[0].tss_ist1 = (long)&dblfault_stack[sizeof(dblfault_stack)];

	/* Set the IO permission bitmap (empty due to tss seg limit) */
	common_tss[0].tss_iobase = sizeof(struct amd64tss);

	gsel_tss = GSEL(GPROC0_SEL, SEL_KPL);
	ltr(gsel_tss);

	/* Set up the fast syscall stuff */
	msr = rdmsr(MSR_EFER) | EFER_SCE;
	wrmsr(MSR_EFER, msr);
	wrmsr(MSR_LSTAR, (u_int64_t)IDTVEC(fast_syscall));
	wrmsr(MSR_CSTAR, (u_int64_t)IDTVEC(fast_syscall32));
	msr = ((u_int64_t)GSEL(GCODE_SEL, SEL_KPL) << 32) |
	      ((u_int64_t)GSEL(GUCODE32_SEL, SEL_UPL) << 48);
	wrmsr(MSR_STAR, msr);
	wrmsr(MSR_SF_MASK, PSL_NT|PSL_T|PSL_I|PSL_C|PSL_D);

	getmemsize(kmdp, physfree);
	init_param2(physmem);

	/* now running on new page tables, configured,and u/iom is accessible */

	/* Map the message buffer. */
	for (off = 0; off < round_page(MSGBUF_SIZE); off += PAGE_SIZE)
		pmap_kenter((vm_offset_t)msgbufp + off, avail_end + off);

	msgbufinit(msgbufp, MSGBUF_SIZE);
	fpuinit();

	/* transfer to user mode */

	_ucodesel = GSEL(GUCODE_SEL, SEL_UPL);
	_udatasel = GSEL(GUDATA_SEL, SEL_UPL);
	_ucode32sel = GSEL(GUCODE32_SEL, SEL_UPL);

	/* setup proc 0's pcb */
	thread0.td_pcb->pcb_flags = 0; /* XXXKSE */
	thread0.td_pcb->pcb_cr3 = KPML4phys;
	thread0.td_frame = &proc0_tf;

        env = getenv("kernelname");
	if (env != NULL)
		strlcpy(kernelname, env, sizeof(kernelname));

	/* Location of kernel stack for locore */
	return ((u_int64_t)thread0.td_pcb);
}

void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t size)
{

	pcpu->pc_acpi_id = 0xffffffff;
}

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

	pcb->pcb_r12 = tf->tf_r12;
	pcb->pcb_r13 = tf->tf_r13;
	pcb->pcb_r14 = tf->tf_r14;
	pcb->pcb_r15 = tf->tf_r15;
	pcb->pcb_rbp = tf->tf_rbp;
	pcb->pcb_rbx = tf->tf_rbx;
	pcb->pcb_rip = tf->tf_rip;
	pcb->pcb_rsp = (ISPL(tf->tf_cs)) ? tf->tf_rsp : (long)(tf + 1) - 8;
}

int
ptrace_set_pc(struct thread *td, unsigned long addr)
{
	td->td_frame->tf_rip = addr;
	return (0);
}

int
ptrace_single_step(struct thread *td)
{
	td->td_frame->tf_rflags |= PSL_T;
	return (0);
}

int
ptrace_clear_single_step(struct thread *td)
{
	td->td_frame->tf_rflags &= ~PSL_T;
	return (0);
}

int
fill_regs(struct thread *td, struct reg *regs)
{
	struct pcb *pcb;
	struct trapframe *tp;

	tp = td->td_frame;
	regs->r_r15 = tp->tf_r15;
	regs->r_r14 = tp->tf_r14;
	regs->r_r13 = tp->tf_r13;
	regs->r_r12 = tp->tf_r12;
	regs->r_r11 = tp->tf_r11;
	regs->r_r10 = tp->tf_r10;
	regs->r_r9  = tp->tf_r9;
	regs->r_r8  = tp->tf_r8;
	regs->r_rdi = tp->tf_rdi;
	regs->r_rsi = tp->tf_rsi;
	regs->r_rbp = tp->tf_rbp;
	regs->r_rbx = tp->tf_rbx;
	regs->r_rdx = tp->tf_rdx;
	regs->r_rcx = tp->tf_rcx;
	regs->r_rax = tp->tf_rax;
	regs->r_rip = tp->tf_rip;
	regs->r_cs = tp->tf_cs;
	regs->r_rflags = tp->tf_rflags;
	regs->r_rsp = tp->tf_rsp;
	regs->r_ss = tp->tf_ss;
	pcb = td->td_pcb;
	return (0);
}

int
set_regs(struct thread *td, struct reg *regs)
{
	struct pcb *pcb;
	struct trapframe *tp;
	register_t rflags;

	tp = td->td_frame;
	rflags = regs->r_rflags & 0xffffffff;
	if (!EFL_SECURE(rflags, tp->tf_rflags) || !CS_SECURE(regs->r_cs))
		return (EINVAL);
	tp->tf_r15 = regs->r_r15;
	tp->tf_r14 = regs->r_r14;
	tp->tf_r13 = regs->r_r13;
	tp->tf_r12 = regs->r_r12;
	tp->tf_r11 = regs->r_r11;
	tp->tf_r10 = regs->r_r10;
	tp->tf_r9  = regs->r_r9;
	tp->tf_r8  = regs->r_r8;
	tp->tf_rdi = regs->r_rdi;
	tp->tf_rsi = regs->r_rsi;
	tp->tf_rbp = regs->r_rbp;
	tp->tf_rbx = regs->r_rbx;
	tp->tf_rdx = regs->r_rdx;
	tp->tf_rcx = regs->r_rcx;
	tp->tf_rax = regs->r_rax;
	tp->tf_rip = regs->r_rip;
	tp->tf_cs = regs->r_cs;
	tp->tf_rflags = rflags;
	tp->tf_rsp = regs->r_rsp;
	tp->tf_ss = regs->r_ss;
	pcb = td->td_pcb;
	return (0);
}

/* XXX check all this stuff! */
/* externalize from sv_xmm */
static void
fill_fpregs_xmm(struct savefpu *sv_xmm, struct fpreg *fpregs)
{
	struct envxmm *penv_fpreg = (struct envxmm *)&fpregs->fpr_env;
	struct envxmm *penv_xmm = &sv_xmm->sv_env;
	int i;

	/* pcb -> fpregs */
	bzero(fpregs, sizeof(*fpregs));

	/* FPU control/status */
	penv_fpreg->en_cw = penv_xmm->en_cw;
	penv_fpreg->en_sw = penv_xmm->en_sw;
	penv_fpreg->en_tw = penv_xmm->en_tw;
	penv_fpreg->en_opcode = penv_xmm->en_opcode;
	penv_fpreg->en_rip = penv_xmm->en_rip;
	penv_fpreg->en_rdp = penv_xmm->en_rdp;
	penv_fpreg->en_mxcsr = penv_xmm->en_mxcsr;
	penv_fpreg->en_mxcsr_mask = penv_xmm->en_mxcsr_mask;

	/* FPU registers */
	for (i = 0; i < 8; ++i)
		bcopy(sv_xmm->sv_fp[i].fp_acc.fp_bytes, fpregs->fpr_acc[i], 10);

	/* SSE registers */
	for (i = 0; i < 16; ++i)
		bcopy(sv_xmm->sv_xmm[i].xmm_bytes, fpregs->fpr_xacc[i], 16);
}

/* internalize from fpregs into sv_xmm */
static void
set_fpregs_xmm(struct fpreg *fpregs, struct savefpu *sv_xmm)
{
	struct envxmm *penv_xmm = &sv_xmm->sv_env;
	struct envxmm *penv_fpreg = (struct envxmm *)&fpregs->fpr_env;
	int i;

	/* fpregs -> pcb */
	/* FPU control/status */
	penv_xmm->en_cw = penv_fpreg->en_cw;
	penv_xmm->en_sw = penv_fpreg->en_sw;
	penv_xmm->en_tw = penv_fpreg->en_tw;
	penv_xmm->en_opcode = penv_fpreg->en_opcode;
	penv_xmm->en_rip = penv_fpreg->en_rip;
	penv_xmm->en_rdp = penv_fpreg->en_rdp;
	penv_xmm->en_mxcsr = penv_fpreg->en_mxcsr;
	penv_xmm->en_mxcsr_mask = penv_fpreg->en_mxcsr_mask;

	/* FPU registers */
	for (i = 0; i < 8; ++i)
		bcopy(fpregs->fpr_acc[i], sv_xmm->sv_fp[i].fp_acc.fp_bytes, 10);

	/* SSE registers */
	for (i = 0; i < 16; ++i)
		bcopy(fpregs->fpr_xacc[i], sv_xmm->sv_xmm[i].xmm_bytes, 16);
}

/* externalize from td->pcb */
int
fill_fpregs(struct thread *td, struct fpreg *fpregs)
{

	fill_fpregs_xmm(&td->td_pcb->pcb_save, fpregs);
	return (0);
}

/* internalize to td->pcb */
int
set_fpregs(struct thread *td, struct fpreg *fpregs)
{

	set_fpregs_xmm(fpregs, &td->td_pcb->pcb_save);
	return (0);
}

/*
 * Get machine context.
 */
int
get_mcontext(struct thread *td, mcontext_t *mcp, int flags)
{
	struct trapframe *tp;

	tp = td->td_frame;
	PROC_LOCK(curthread->td_proc);
	mcp->mc_onstack = sigonstack(tp->tf_rsp);
	PROC_UNLOCK(curthread->td_proc);
	mcp->mc_r15 = tp->tf_r15;
	mcp->mc_r14 = tp->tf_r14;
	mcp->mc_r13 = tp->tf_r13;
	mcp->mc_r12 = tp->tf_r12;
	mcp->mc_r11 = tp->tf_r11;
	mcp->mc_r10 = tp->tf_r10;
	mcp->mc_r9  = tp->tf_r9;
	mcp->mc_r8  = tp->tf_r8;
	mcp->mc_rdi = tp->tf_rdi;
	mcp->mc_rsi = tp->tf_rsi;
	mcp->mc_rbp = tp->tf_rbp;
	mcp->mc_rbx = tp->tf_rbx;
	mcp->mc_rcx = tp->tf_rcx;
	if (flags & GET_MC_CLEAR_RET) {
		mcp->mc_rax = 0;
		mcp->mc_rdx = 0;
	} else {
		mcp->mc_rax = tp->tf_rax;
		mcp->mc_rdx = tp->tf_rdx;
	}
	mcp->mc_rip = tp->tf_rip;
	mcp->mc_cs = tp->tf_cs;
	mcp->mc_rflags = tp->tf_rflags;
	mcp->mc_rsp = tp->tf_rsp;
	mcp->mc_ss = tp->tf_ss;
	mcp->mc_len = sizeof(*mcp);
	get_fpcontext(td, mcp);
	return (0);
}

/*
 * Set machine context.
 *
 * However, we don't set any but the user modifiable flags, and we won't
 * touch the cs selector.
 */
int
set_mcontext(struct thread *td, const mcontext_t *mcp)
{
	struct trapframe *tp;
	long rflags;
	int ret;

	tp = td->td_frame;
	if (mcp->mc_len != sizeof(*mcp))
		return (EINVAL);
	rflags = (mcp->mc_rflags & PSL_USERCHANGE) |
	    (tp->tf_rflags & ~PSL_USERCHANGE);
	ret = set_fpcontext(td, mcp);
	if (ret != 0)
		return (ret);
	tp->tf_r15 = mcp->mc_r15;
	tp->tf_r14 = mcp->mc_r14;
	tp->tf_r13 = mcp->mc_r13;
	tp->tf_r12 = mcp->mc_r12;
	tp->tf_r11 = mcp->mc_r11;
	tp->tf_r10 = mcp->mc_r10;
	tp->tf_r9  = mcp->mc_r9;
	tp->tf_r8  = mcp->mc_r8;
	tp->tf_rdi = mcp->mc_rdi;
	tp->tf_rsi = mcp->mc_rsi;
	tp->tf_rbp = mcp->mc_rbp;
	tp->tf_rbx = mcp->mc_rbx;
	tp->tf_rdx = mcp->mc_rdx;
	tp->tf_rcx = mcp->mc_rcx;
	tp->tf_rax = mcp->mc_rax;
	tp->tf_rip = mcp->mc_rip;
	tp->tf_rflags = rflags;
	tp->tf_rsp = mcp->mc_rsp;
	tp->tf_ss = mcp->mc_ss;
	td->td_pcb->pcb_flags |= PCB_FULLCTX;
	return (0);
}

static void
get_fpcontext(struct thread *td, mcontext_t *mcp)
{

	mcp->mc_ownedfp = fpugetregs(td, (struct savefpu *)&mcp->mc_fpstate);
	mcp->mc_fpformat = fpuformat();
}

static int
set_fpcontext(struct thread *td, const mcontext_t *mcp)
{

	if (mcp->mc_fpformat == _MC_FPFMT_NODEV)
		return (0);
	else if (mcp->mc_fpformat != _MC_FPFMT_XMM)
		return (EINVAL);
	else if (mcp->mc_ownedfp == _MC_FPOWNED_NONE)
		/* We don't care what state is left in the FPU or PCB. */
		fpstate_drop(td);
	else if (mcp->mc_ownedfp == _MC_FPOWNED_FPU ||
	    mcp->mc_ownedfp == _MC_FPOWNED_PCB) {
		/*
		 * XXX we violate the dubious requirement that fpusetregs()
		 * be called with interrupts disabled.
		 * XXX obsolete on trap-16 systems?
		 */
		fpusetregs(td, (struct savefpu *)&mcp->mc_fpstate);
	} else
		return (EINVAL);
	return (0);
}

void
fpstate_drop(struct thread *td)
{
	register_t s;

	s = intr_disable();
	if (PCPU_GET(fpcurthread) == td)
		fpudrop();
	/*
	 * XXX force a full drop of the fpu.  The above only drops it if we
	 * owned it.
	 *
	 * XXX I don't much like fpugetregs()'s semantics of doing a full
	 * drop.  Dropping only to the pcb matches fnsave's behaviour.
	 * We only need to drop to !PCB_INITDONE in sendsig().  But
	 * sendsig() is the only caller of fpugetregs()... perhaps we just
	 * have too many layers.
	 */
	curthread->td_pcb->pcb_flags &= ~PCB_FPUINITDONE;
	intr_restore(s);
}

int
fill_dbregs(struct thread *td, struct dbreg *dbregs)
{
	struct pcb *pcb;

	if (td == NULL) {
		dbregs->dr[0] = rdr0();
		dbregs->dr[1] = rdr1();
		dbregs->dr[2] = rdr2();
		dbregs->dr[3] = rdr3();
		dbregs->dr[6] = rdr6();
		dbregs->dr[7] = rdr7();
	} else {
		pcb = td->td_pcb;
		dbregs->dr[0] = pcb->pcb_dr0;
		dbregs->dr[1] = pcb->pcb_dr1;
		dbregs->dr[2] = pcb->pcb_dr2;
		dbregs->dr[3] = pcb->pcb_dr3;
		dbregs->dr[6] = pcb->pcb_dr6;
		dbregs->dr[7] = pcb->pcb_dr7;
	}
	dbregs->dr[4] = 0;
	dbregs->dr[5] = 0;
	dbregs->dr[8] = 0;
	dbregs->dr[9] = 0;
	dbregs->dr[10] = 0;
	dbregs->dr[11] = 0;
	dbregs->dr[12] = 0;
	dbregs->dr[13] = 0;
	dbregs->dr[14] = 0;
	dbregs->dr[15] = 0;
	return (0);
}

int
set_dbregs(struct thread *td, struct dbreg *dbregs)
{
	struct pcb *pcb;
	int i;
	u_int64_t mask1, mask2;

	if (td == NULL) {
		load_dr0(dbregs->dr[0]);
		load_dr1(dbregs->dr[1]);
		load_dr2(dbregs->dr[2]);
		load_dr3(dbregs->dr[3]);
		load_dr6(dbregs->dr[6]);
		load_dr7(dbregs->dr[7]);
	} else {
		/*
		 * Don't let an illegal value for dr7 get set.  Specifically,
		 * check for undefined settings.  Setting these bit patterns
		 * result in undefined behaviour and can lead to an unexpected
		 * TRCTRAP or a general protection fault right here.
		 */
		for (i = 0, mask1 = 0x3<<16, mask2 = 0x2<<16; i < 8;
		     i++, mask1 <<= 2, mask2 <<= 2)
			if ((dbregs->dr[7] & mask1) == mask2)
				return (EINVAL);

		pcb = td->td_pcb;

		/*
		 * Don't let a process set a breakpoint that is not within the
		 * process's address space.  If a process could do this, it
		 * could halt the system by setting a breakpoint in the kernel
		 * (if ddb was enabled).  Thus, we need to check to make sure
		 * that no breakpoints are being enabled for addresses outside
		 * process's address space, unless, perhaps, we were called by
		 * uid 0.
		 *
		 * XXX - what about when the watched area of the user's
		 * address space is written into from within the kernel
		 * ... wouldn't that still cause a breakpoint to be generated
		 * from within kernel mode?
		 */

		if (suser(td) != 0) {
			if (dbregs->dr[7] & 0x3) {
				/* dr0 is enabled */
				if (dbregs->dr[0] >= VM_MAXUSER_ADDRESS)
					return (EINVAL);
			}
			if (dbregs->dr[7] & 0x3<<2) {
				/* dr1 is enabled */
				if (dbregs->dr[1] >= VM_MAXUSER_ADDRESS)
					return (EINVAL);
			}
			if (dbregs->dr[7] & 0x3<<4) {
				/* dr2 is enabled */
				if (dbregs->dr[2] >= VM_MAXUSER_ADDRESS)
					return (EINVAL);
			}
			if (dbregs->dr[7] & 0x3<<6) {
				/* dr3 is enabled */
				if (dbregs->dr[3] >= VM_MAXUSER_ADDRESS)
					return (EINVAL);
			}
		}

		pcb->pcb_dr0 = dbregs->dr[0];
		pcb->pcb_dr1 = dbregs->dr[1];
		pcb->pcb_dr2 = dbregs->dr[2];
		pcb->pcb_dr3 = dbregs->dr[3];
		pcb->pcb_dr6 = dbregs->dr[6];
		pcb->pcb_dr7 = dbregs->dr[7];

		pcb->pcb_flags |= PCB_DBREGS;
	}

	return (0);
}

void
reset_dbregs(void)
{

	load_dr7(0);	/* Turn off the control bits first */
	load_dr0(0);
	load_dr1(0);
	load_dr2(0);
	load_dr3(0);
	load_dr6(0);
}

/*
 * Return > 0 if a hardware breakpoint has been hit, and the
 * breakpoint was in user space.  Return 0, otherwise.
 */
int
user_dbreg_trap(void)
{
        u_int64_t dr7, dr6; /* debug registers dr6 and dr7 */
        u_int64_t bp;       /* breakpoint bits extracted from dr6 */
        int nbp;            /* number of breakpoints that triggered */
        caddr_t addr[4];    /* breakpoint addresses */
        int i;
        
        dr7 = rdr7();
        if ((dr7 & 0x000000ff) == 0) {
                /*
                 * all GE and LE bits in the dr7 register are zero,
                 * thus the trap couldn't have been caused by the
                 * hardware debug registers
                 */
                return 0;
        }

        nbp = 0;
        dr6 = rdr6();
        bp = dr6 & 0x0000000f;

        if (!bp) {
                /*
                 * None of the breakpoint bits are set meaning this
                 * trap was not caused by any of the debug registers
                 */
                return 0;
        }

        /*
         * at least one of the breakpoints were hit, check to see
         * which ones and if any of them are user space addresses
         */

        if (bp & 0x01) {
                addr[nbp++] = (caddr_t)rdr0();
        }
        if (bp & 0x02) {
                addr[nbp++] = (caddr_t)rdr1();
        }
        if (bp & 0x04) {
                addr[nbp++] = (caddr_t)rdr2();
        }
        if (bp & 0x08) {
                addr[nbp++] = (caddr_t)rdr3();
        }

        for (i=0; i<nbp; i++) {
                if (addr[i] <
                    (caddr_t)VM_MAXUSER_ADDRESS) {
                        /*
                         * addr[i] is in user space
                         */
                        return nbp;
                }
        }

        /*
         * None of the breakpoints are in user space.
         */
        return 0;
}

#ifdef KDB

/*
 * Provide inb() and outb() as functions.  They are normally only
 * available as macros calling inlined functions, thus cannot be
 * called from the debugger.
 *
 * The actual code is stolen from <machine/cpufunc.h>, and de-inlined.
 */

#undef inb
#undef outb

/* silence compiler warnings */
u_char inb(u_int);
void outb(u_int, u_char);

u_char
inb(u_int port)
{
	u_char	data;
	/*
	 * We use %%dx and not %1 here because i/o is done at %dx and not at
	 * %edx, while gcc generates inferior code (movw instead of movl)
	 * if we tell it to load (u_short) port.
	 */
	__asm __volatile("inb %%dx,%0" : "=a" (data) : "d" (port));
	return (data);
}

void
outb(u_int port, u_char data)
{
	u_char	al;
	/*
	 * Use an unnecessary assignment to help gcc's register allocator.
	 * This make a large difference for gcc-1.40 and a tiny difference
	 * for gcc-2.6.0.  For gcc-1.40, al had to be ``asm("ax")'' for
	 * best results.  gcc-2.6.0 can't handle this.
	 */
	al = data;
	__asm __volatile("outb %0,%%dx" : : "a" (al), "d" (port));
}

#endif /* KDB */
