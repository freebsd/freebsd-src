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
#include "opt_acpi.h"
#include "opt_atpic.h"
#include "opt_cpu.h"
#include "opt_ddb.h"
#include "opt_inet.h"
#include "opt_isa.h"
#include "opt_kdb.h"
#include "opt_kstack_pages.h"
#include "opt_maxmem.h"
#include "opt_platform.h"
#include "opt_sched.h"
#ifdef __i386__
#include "opt_apic.h"
#endif

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/domainset.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/cputypes.h>
#include <machine/specialreg.h>
#include <machine/md_var.h>
#include <machine/tss.h>
#ifdef SMP
#include <machine/smp.h>
#endif
#ifdef CPU_ELAN
#include <machine/elan_mmcr.h>
#endif
#include <x86/acpica_machdep.h>
#include <x86/ifunc.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vm_param.h>

#include <isa/isareg.h>

#include <contrib/dev/acpica/include/acpi.h>

#define	STATE_RUNNING	0x0
#define	STATE_MWAIT	0x1
#define	STATE_SLEEPING	0x2

#ifdef SMP
static u_int	cpu_reset_proxyid;
static volatile u_int	cpu_reset_proxy_active;
#endif

char bootmethod[16];
SYSCTL_STRING(_machdep, OID_AUTO, bootmethod, CTLFLAG_RD, bootmethod, 0,
    "System firmware boot method");

struct msr_op_arg {
	u_int msr;
	int op;
	uint64_t arg1;
	uint64_t *res;
};

static void
x86_msr_op_one(void *argp)
{
	struct msr_op_arg *a;
	uint64_t v;

	a = argp;
	switch (a->op) {
	case MSR_OP_ANDNOT:
		v = rdmsr(a->msr);
		v &= ~a->arg1;
		wrmsr(a->msr, v);
		break;
	case MSR_OP_OR:
		v = rdmsr(a->msr);
		v |= a->arg1;
		wrmsr(a->msr, v);
		break;
	case MSR_OP_WRITE:
		wrmsr(a->msr, a->arg1);
		break;
	case MSR_OP_READ:
		v = rdmsr(a->msr);
		*a->res = v;
		break;
	}
}

#define	MSR_OP_EXMODE_MASK	0xf0000000
#define	MSR_OP_OP_MASK		0x000000ff
#define	MSR_OP_GET_CPUID(x)	(((x) & ~MSR_OP_EXMODE_MASK) >> 8)

void
x86_msr_op(u_int msr, u_int op, uint64_t arg1, uint64_t *res)
{
	struct thread *td;
	struct msr_op_arg a;
	cpuset_t set;
	u_int exmode;
	int bound_cpu, cpu, i, is_bound;

	a.op = op & MSR_OP_OP_MASK;
	MPASS(a.op == MSR_OP_ANDNOT || a.op == MSR_OP_OR ||
	    a.op == MSR_OP_WRITE || a.op == MSR_OP_READ);
	exmode = op & MSR_OP_EXMODE_MASK;
	MPASS(exmode == MSR_OP_LOCAL || exmode == MSR_OP_SCHED_ALL ||
	    exmode == MSR_OP_SCHED_ONE || exmode == MSR_OP_RENDEZVOUS_ALL ||
	    exmode == MSR_OP_RENDEZVOUS_ONE);
	a.msr = msr;
	a.arg1 = arg1;
	a.res = res;
	switch (exmode) {
	case MSR_OP_LOCAL:
		x86_msr_op_one(&a);
		break;
	case MSR_OP_SCHED_ALL:
		td = curthread;
		thread_lock(td);
		is_bound = sched_is_bound(td);
		bound_cpu = td->td_oncpu;
		CPU_FOREACH(i) {
			sched_bind(td, i);
			x86_msr_op_one(&a);
		}
		if (is_bound)
			sched_bind(td, bound_cpu);
		else
			sched_unbind(td);
		thread_unlock(td);
		break;
	case MSR_OP_SCHED_ONE:
		td = curthread;
		cpu = MSR_OP_GET_CPUID(op);
		thread_lock(td);
		is_bound = sched_is_bound(td);
		bound_cpu = td->td_oncpu;
		if (!is_bound || bound_cpu != cpu)
			sched_bind(td, cpu);
		x86_msr_op_one(&a);
		if (is_bound) {
			if (bound_cpu != cpu)
				sched_bind(td, bound_cpu);
		} else {
			sched_unbind(td);
		}
		thread_unlock(td);
		break;
	case MSR_OP_RENDEZVOUS_ALL:
		smp_rendezvous(smp_no_rendezvous_barrier, x86_msr_op_one,
		    smp_no_rendezvous_barrier, &a);
		break;
	case MSR_OP_RENDEZVOUS_ONE:
		cpu = MSR_OP_GET_CPUID(op);
		CPU_SETOF(cpu, &set);
		smp_rendezvous_cpus(set, smp_no_rendezvous_barrier,
		    x86_msr_op_one, smp_no_rendezvous_barrier, &a);
		break;
	}
}

/*
 * Automatically initialized per CPU errata in cpu_idle_tun below.
 */
bool mwait_cpustop_broken = false;
SYSCTL_BOOL(_machdep, OID_AUTO, mwait_cpustop_broken, CTLFLAG_RDTUN,
    &mwait_cpustop_broken, 0,
    "Can not reliably wake MONITOR/MWAIT cpus without interrupts");

/*
 * Flush the D-cache for non-DMA I/O so that the I-cache can
 * be made coherent later.
 */
void
cpu_flush_dcache(void *ptr, size_t len)
{
	/* Not applicable */
}

void
acpi_cpu_c1(void)
{

	__asm __volatile("sti; hlt");
}

/*
 * Use mwait to pause execution while waiting for an interrupt or
 * another thread to signal that there is more work.
 *
 * NOTE: Interrupts will cause a wakeup; however, this function does
 * not enable interrupt handling. The caller is responsible to enable
 * interrupts.
 */
void
acpi_cpu_idle_mwait(uint32_t mwait_hint)
{
	int *state;
	uint64_t v;

	/*
	 * A comment in Linux patch claims that 'CPUs run faster with
	 * speculation protection disabled. All CPU threads in a core
	 * must disable speculation protection for it to be
	 * disabled. Disable it while we are idle so the other
	 * hyperthread can run fast.'
	 *
	 * XXXKIB.  Software coordination mode should be supported,
	 * but all Intel CPUs provide hardware coordination.
	 */

	state = &PCPU_PTR(monitorbuf)->idle_state;
	KASSERT(atomic_load_int(state) == STATE_SLEEPING,
	    ("cpu_mwait_cx: wrong monitorbuf state"));
	atomic_store_int(state, STATE_MWAIT);
	if (PCPU_GET(ibpb_set) || hw_ssb_active) {
		v = rdmsr(MSR_IA32_SPEC_CTRL);
		wrmsr(MSR_IA32_SPEC_CTRL, v & ~(IA32_SPEC_CTRL_IBRS |
		    IA32_SPEC_CTRL_STIBP | IA32_SPEC_CTRL_SSBD));
	} else {
		v = 0;
	}
	cpu_monitor(state, 0, 0);
	if (atomic_load_int(state) == STATE_MWAIT)
		cpu_mwait(MWAIT_INTRBREAK, mwait_hint);

	/*
	 * SSB cannot be disabled while we sleep, or rather, if it was
	 * disabled, the sysctl thread will bind to our cpu to tweak
	 * MSR.
	 */
	if (v != 0)
		wrmsr(MSR_IA32_SPEC_CTRL, v);

	/*
	 * We should exit on any event that interrupts mwait, because
	 * that event might be a wanted interrupt.
	 */
	atomic_store_int(state, STATE_RUNNING);
}

/* Get current clock frequency for the given cpu id. */
int
cpu_est_clockrate(int cpu_id, uint64_t *rate)
{
	uint64_t tsc1, tsc2;
	uint64_t acnt, mcnt, perf;
	register_t reg;

	if (pcpu_find(cpu_id) == NULL || rate == NULL)
		return (EINVAL);
#ifdef __i386__
	if ((cpu_feature & CPUID_TSC) == 0)
		return (EOPNOTSUPP);
#endif

	/*
	 * If TSC is P-state invariant and APERF/MPERF MSRs do not exist,
	 * DELAY(9) based logic fails.
	 */
	if (tsc_is_invariant && !tsc_perf_stat)
		return (EOPNOTSUPP);

#ifdef SMP
	if (smp_cpus > 1) {
		/* Schedule ourselves on the indicated cpu. */
		thread_lock(curthread);
		sched_bind(curthread, cpu_id);
		thread_unlock(curthread);
	}
#endif

	/* Calibrate by measuring a short delay. */
	reg = intr_disable();
	if (tsc_is_invariant) {
		wrmsr(MSR_MPERF, 0);
		wrmsr(MSR_APERF, 0);
		tsc1 = rdtsc();
		DELAY(1000);
		mcnt = rdmsr(MSR_MPERF);
		acnt = rdmsr(MSR_APERF);
		tsc2 = rdtsc();
		intr_restore(reg);
		perf = 1000 * acnt / mcnt;
		*rate = (tsc2 - tsc1) * perf;
	} else {
		tsc1 = rdtsc();
		DELAY(1000);
		tsc2 = rdtsc();
		intr_restore(reg);
		*rate = (tsc2 - tsc1) * 1000;
	}

#ifdef SMP
	if (smp_cpus > 1) {
		thread_lock(curthread);
		sched_unbind(curthread);
		thread_unlock(curthread);
	}
#endif

	return (0);
}

/*
 * Shutdown the CPU as much as possible
 */
void
cpu_halt(void)
{
	for (;;)
		halt();
}

static void
cpu_reset_real(void)
{
	struct region_descriptor null_idt;
	int b;

	disable_intr();
#ifdef CPU_ELAN
	if (elan_mmcr != NULL)
		elan_mmcr->RESCFG = 1;
#endif
#ifdef __i386__
	if (cpu == CPU_GEODE1100) {
		/* Attempt Geode's own reset */
		outl(0xcf8, 0x80009044ul);
		outl(0xcfc, 0xf);
	}
#endif
#if !defined(BROKEN_KEYBOARD_RESET)
	/*
	 * Attempt to do a CPU reset via the keyboard controller,
	 * do not turn off GateA20, as any machine that fails
	 * to do the reset here would then end up in no man's land.
	 */
	outb(IO_KBD + 4, 0xFE);
	DELAY(500000);	/* wait 0.5 sec to see if that did it */
#endif

	/*
	 * Attempt to force a reset via the Reset Control register at
	 * I/O port 0xcf9.  Bit 2 forces a system reset when it
	 * transitions from 0 to 1.  Bit 1 selects the type of reset
	 * to attempt: 0 selects a "soft" reset, and 1 selects a
	 * "hard" reset.  We try a "hard" reset.  The first write sets
	 * bit 1 to select a "hard" reset and clears bit 2.  The
	 * second write forces a 0 -> 1 transition in bit 2 to trigger
	 * a reset.
	 */
	outb(0xcf9, 0x2);
	outb(0xcf9, 0x6);
	DELAY(500000);  /* wait 0.5 sec to see if that did it */

	/*
	 * Attempt to force a reset via the Fast A20 and Init register
	 * at I/O port 0x92.  Bit 1 serves as an alternate A20 gate.
	 * Bit 0 asserts INIT# when set to 1.  We are careful to only
	 * preserve bit 1 while setting bit 0.  We also must clear bit
	 * 0 before setting it if it isn't already clear.
	 */
	b = inb(0x92);
	if (b != 0xff) {
		if ((b & 0x1) != 0)
			outb(0x92, b & 0xfe);
		outb(0x92, b | 0x1);
		DELAY(500000);  /* wait 0.5 sec to see if that did it */
	}

	printf("No known reset method worked, attempting CPU shutdown\n");
	DELAY(1000000); /* wait 1 sec for printf to complete */

	/* Wipe the IDT. */
	null_idt.rd_limit = 0;
	null_idt.rd_base = 0;
	lidt(&null_idt);

	/* "good night, sweet prince .... <THUNK!>" */
	breakpoint();

	/* NOTREACHED */
	while(1);
}

#ifdef SMP
static void
cpu_reset_proxy(void)
{

	cpu_reset_proxy_active = 1;
	while (cpu_reset_proxy_active == 1)
		ia32_pause(); /* Wait for other cpu to see that we've started */

	printf("cpu_reset_proxy: Stopped CPU %d\n", cpu_reset_proxyid);
	DELAY(1000000);
	cpu_reset_real();
}
#endif

void
cpu_reset(void)
{
#ifdef SMP
	struct monitorbuf *mb;
	cpuset_t map;
	u_int cnt;

	if (smp_started) {
		map = all_cpus;
		CPU_CLR(PCPU_GET(cpuid), &map);
		CPU_ANDNOT(&map, &map, &stopped_cpus);
		if (!CPU_EMPTY(&map)) {
			printf("cpu_reset: Stopping other CPUs\n");
			stop_cpus(map);
		}

		if (PCPU_GET(cpuid) != 0) {
			cpu_reset_proxyid = PCPU_GET(cpuid);
			cpustop_restartfunc = cpu_reset_proxy;
			cpu_reset_proxy_active = 0;
			printf("cpu_reset: Restarting BSP\n");

			/* Restart CPU #0. */
			CPU_SETOF(0, &started_cpus);
			mb = &pcpu_find(0)->pc_monitorbuf;
			atomic_store_int(&mb->stop_state,
			    MONITOR_STOPSTATE_RUNNING);

			cnt = 0;
			while (cpu_reset_proxy_active == 0 && cnt < 10000000) {
				ia32_pause();
				cnt++;	/* Wait for BSP to announce restart */
			}
			if (cpu_reset_proxy_active == 0) {
				printf("cpu_reset: Failed to restart BSP\n");
			} else {
				cpu_reset_proxy_active = 2;
				while (1)
					ia32_pause();
				/* NOTREACHED */
			}
		}
	}
#endif
	cpu_reset_real();
	/* NOTREACHED */
}

bool
cpu_mwait_usable(void)
{

	return ((cpu_feature2 & CPUID2_MON) != 0 && ((cpu_mon_mwait_flags &
	    (CPUID5_MON_MWAIT_EXT | CPUID5_MWAIT_INTRBREAK)) ==
	    (CPUID5_MON_MWAIT_EXT | CPUID5_MWAIT_INTRBREAK)));
}

void (*cpu_idle_hook)(sbintime_t) = NULL;	/* ACPI idle hook. */

int cpu_amdc1e_bug = 0;			/* AMD C1E APIC workaround required. */

static int	idle_mwait = 1;		/* Use MONITOR/MWAIT for short idle. */
SYSCTL_INT(_machdep, OID_AUTO, idle_mwait, CTLFLAG_RWTUN, &idle_mwait,
    0, "Use MONITOR/MWAIT for short idle");

static bool
cpu_idle_enter(int *statep, int newstate)
{
	KASSERT(atomic_load_int(statep) == STATE_RUNNING,
	    ("%s: state %d", __func__, atomic_load_int(statep)));

	/*
	 * A fence is needed to prevent reordering of the load in
	 * sched_runnable() with this store to the idle state word.  Without it,
	 * cpu_idle_wakeup() can observe the state as STATE_RUNNING after having
	 * added load to the queue, and elide an IPI.  Then, sched_runnable()
	 * can observe tdq_load == 0, so the CPU ends up idling with pending
	 * work.  tdq_notify() similarly ensures that a prior update to tdq_load
	 * is visible before calling cpu_idle_wakeup().
	 */
	atomic_store_int(statep, newstate);
#if defined(SCHED_ULE) && defined(SMP)
	atomic_thread_fence_seq_cst();
#endif

	/*
	 * Since we may be in a critical section from cpu_idle(), if
	 * an interrupt fires during that critical section we may have
	 * a pending preemption.  If the CPU halts, then that thread
	 * may not execute until a later interrupt awakens the CPU.
	 * To handle this race, check for a runnable thread after
	 * disabling interrupts and immediately return if one is
	 * found.  Also, we must absolutely guarentee that hlt is
	 * the next instruction after sti.  This ensures that any
	 * interrupt that fires after the call to disable_intr() will
	 * immediately awaken the CPU from hlt.  Finally, please note
	 * that on x86 this works fine because of interrupts enabled only
	 * after the instruction following sti takes place, while IF is set
	 * to 1 immediately, allowing hlt instruction to acknowledge the
	 * interrupt.
	 */
	disable_intr();
	if (sched_runnable()) {
		enable_intr();
		atomic_store_int(statep, STATE_RUNNING);
		return (false);
	} else {
		return (true);
	}
}

static void
cpu_idle_exit(int *statep)
{
	atomic_store_int(statep, STATE_RUNNING);
}

static void
cpu_idle_acpi(sbintime_t sbt)
{
	int *state;

	state = &PCPU_PTR(monitorbuf)->idle_state;
	if (cpu_idle_enter(state, STATE_SLEEPING)) {
		if (cpu_idle_hook)
			cpu_idle_hook(sbt);
		else
			acpi_cpu_c1();
		cpu_idle_exit(state);
	}
}

static void
cpu_idle_hlt(sbintime_t sbt)
{
	int *state;

	state = &PCPU_PTR(monitorbuf)->idle_state;
	if (cpu_idle_enter(state, STATE_SLEEPING)) {
		acpi_cpu_c1();
		atomic_store_int(state, STATE_RUNNING);
	}
}

static void
cpu_idle_mwait(sbintime_t sbt)
{
	int *state;

	state = &PCPU_PTR(monitorbuf)->idle_state;
	if (cpu_idle_enter(state, STATE_MWAIT)) {
		cpu_monitor(state, 0, 0);
		if (atomic_load_int(state) == STATE_MWAIT)
			__asm __volatile("sti; mwait" : : "a" (MWAIT_C1), "c" (0));
		else
			enable_intr();
		cpu_idle_exit(state);
	}
}

static void
cpu_idle_spin(sbintime_t sbt)
{
	int *state;
	int i;

	state = &PCPU_PTR(monitorbuf)->idle_state;
	atomic_store_int(state, STATE_RUNNING);

	/*
	 * The sched_runnable() call is racy but as long as there is
	 * a loop missing it one time will have just a little impact if any 
	 * (and it is much better than missing the check at all).
	 */
	for (i = 0; i < 1000; i++) {
		if (sched_runnable())
			return;
		cpu_spinwait();
	}
}

void (*cpu_idle_fn)(sbintime_t) = cpu_idle_acpi;

void
cpu_idle(int busy)
{
	uint64_t msr;
	sbintime_t sbt = -1;

	CTR1(KTR_SPARE2, "cpu_idle(%d)", busy);

	/* If we are busy - try to use fast methods. */
	if (busy) {
		if ((cpu_feature2 & CPUID2_MON) && idle_mwait) {
			cpu_idle_mwait(busy);
			goto out;
		}
	}

	/* If we have time - switch timers into idle mode. */
	if (!busy) {
		critical_enter();
		sbt = cpu_idleclock();
	}

	/* Apply AMD APIC timer C1E workaround. */
	if (cpu_amdc1e_bug && cpu_disable_c3_sleep) {
		msr = rdmsr(MSR_AMDK8_IPM);
		if ((msr & (AMDK8_SMIONCMPHALT | AMDK8_C1EONCMPHALT)) != 0)
			wrmsr(MSR_AMDK8_IPM, msr & ~(AMDK8_SMIONCMPHALT |
			    AMDK8_C1EONCMPHALT));
	}

	/* Call main idle method. */
	cpu_idle_fn(sbt);

	/* Switch timers back into active mode. */
	if (!busy) {
		cpu_activeclock();
		critical_exit();
	}
out:
	CTR1(KTR_SPARE2, "cpu_idle(%d) done", busy);
}

static int cpu_idle_apl31_workaround;
SYSCTL_INT(_machdep, OID_AUTO, idle_apl31, CTLFLAG_RWTUN | CTLFLAG_NOFETCH,
    &cpu_idle_apl31_workaround, 0,
    "Apollo Lake APL31 MWAIT bug workaround");

int
cpu_idle_wakeup(int cpu)
{
	struct monitorbuf *mb;
	int *state;

	mb = &pcpu_find(cpu)->pc_monitorbuf;
	state = &mb->idle_state;
	switch (atomic_load_int(state)) {
	case STATE_SLEEPING:
		return (0);
	case STATE_MWAIT:
		atomic_store_int(state, STATE_RUNNING);
		return (cpu_idle_apl31_workaround ? 0 : 1);
	case STATE_RUNNING:
		return (1);
	default:
		panic("bad monitor state");
		return (1);
	}
}

/*
 * Ordered by speed/power consumption.
 */
static const struct {
	void	*id_fn;
	const char *id_name;
	int	id_cpuid2_flag;
} idle_tbl[] = {
	{ .id_fn = cpu_idle_spin, .id_name = "spin" },
	{ .id_fn = cpu_idle_mwait, .id_name = "mwait",
	    .id_cpuid2_flag = CPUID2_MON },
	{ .id_fn = cpu_idle_hlt, .id_name = "hlt" },
	{ .id_fn = cpu_idle_acpi, .id_name = "acpi" },
};

static int
idle_sysctl_available(SYSCTL_HANDLER_ARGS)
{
	char *avail, *p;
	int error;
	int i;

	avail = malloc(256, M_TEMP, M_WAITOK);
	p = avail;
	for (i = 0; i < nitems(idle_tbl); i++) {
		if (idle_tbl[i].id_cpuid2_flag != 0 &&
		    (cpu_feature2 & idle_tbl[i].id_cpuid2_flag) == 0)
			continue;
		if (strcmp(idle_tbl[i].id_name, "acpi") == 0 &&
		    cpu_idle_hook == NULL)
			continue;
		p += sprintf(p, "%s%s", p != avail ? ", " : "",
		    idle_tbl[i].id_name);
	}
	error = sysctl_handle_string(oidp, avail, 0, req);
	free(avail, M_TEMP);
	return (error);
}

SYSCTL_PROC(_machdep, OID_AUTO, idle_available,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
    0, 0, idle_sysctl_available, "A",
    "list of available idle functions");

static bool
cpu_idle_selector(const char *new_idle_name)
{
	int i;

	for (i = 0; i < nitems(idle_tbl); i++) {
		if (idle_tbl[i].id_cpuid2_flag != 0 &&
		    (cpu_feature2 & idle_tbl[i].id_cpuid2_flag) == 0)
			continue;
		if (strcmp(idle_tbl[i].id_name, "acpi") == 0 &&
		    cpu_idle_hook == NULL)
			continue;
		if (strcmp(idle_tbl[i].id_name, new_idle_name))
			continue;
		cpu_idle_fn = idle_tbl[i].id_fn;
		if (bootverbose)
			printf("CPU idle set to %s\n", idle_tbl[i].id_name);
		return (true);
	}
	return (false);
}

static int
cpu_idle_sysctl(SYSCTL_HANDLER_ARGS)
{
	char buf[16];
	const char *p;
	int error, i;

	p = "unknown";
	for (i = 0; i < nitems(idle_tbl); i++) {
		if (idle_tbl[i].id_fn == cpu_idle_fn) {
			p = idle_tbl[i].id_name;
			break;
		}
	}
	strncpy(buf, p, sizeof(buf));
	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	return (cpu_idle_selector(buf) ? 0 : EINVAL);
}

SYSCTL_PROC(_machdep, OID_AUTO, idle,
    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE,
    0, 0, cpu_idle_sysctl, "A",
    "currently selected idle function");

static void
cpu_idle_tun(void *unused __unused)
{
	char tunvar[16];

	if (TUNABLE_STR_FETCH("machdep.idle", tunvar, sizeof(tunvar)))
		cpu_idle_selector(tunvar);
	else if (cpu_vendor_id == CPU_VENDOR_AMD &&
	    CPUID_TO_FAMILY(cpu_id) == 0x17 && CPUID_TO_MODEL(cpu_id) == 0x1) {
		/* Ryzen erratas 1057, 1109. */
		cpu_idle_selector("hlt");
		idle_mwait = 0;
		mwait_cpustop_broken = true;
	}

	if (cpu_vendor_id == CPU_VENDOR_INTEL &&
	    CPUID_TO_FAMILY(cpu_id) == 0x6 && CPUID_TO_MODEL(cpu_id) == 0x5c) {
		/*
		 * Apollo Lake errata APL31 (public errata APL30).
		 * Stores to the armed address range may not trigger
		 * MWAIT to resume execution.  OS needs to use
		 * interrupts to wake processors from MWAIT-induced
		 * sleep states.
		 */
		cpu_idle_apl31_workaround = 1;
		mwait_cpustop_broken = true;
	}
	TUNABLE_INT_FETCH("machdep.idle_apl31", &cpu_idle_apl31_workaround);
}
SYSINIT(cpu_idle_tun, SI_SUB_CPU, SI_ORDER_MIDDLE, cpu_idle_tun, NULL);

static int panic_on_nmi = 0xff;
SYSCTL_INT(_machdep, OID_AUTO, panic_on_nmi, CTLFLAG_RWTUN,
    &panic_on_nmi, 0,
    "Panic on NMI: 1 = H/W failure; 2 = unknown; 0xff = all");
int nmi_is_broadcast = 1;
SYSCTL_INT(_machdep, OID_AUTO, nmi_is_broadcast, CTLFLAG_RWTUN,
    &nmi_is_broadcast, 0,
    "Chipset NMI is broadcast");
int (*apei_nmi)(void);

void
nmi_call_kdb(u_int cpu, u_int type, struct trapframe *frame)
{
	bool claimed = false;

#ifdef DEV_ISA
	/* machine/parity/power fail/"kitchen sink" faults */
	if (isa_nmi(frame->tf_err)) {
		claimed = true;
		if ((panic_on_nmi & 1) != 0)
			panic("NMI indicates hardware failure");
	}
#endif /* DEV_ISA */

	/* ACPI Platform Error Interfaces callback. */
	if (apei_nmi != NULL && (*apei_nmi)())
		claimed = true;

	/*
	 * NMIs can be useful for debugging.  They can be hooked up to a
	 * pushbutton, usually on an ISA, PCI, or PCIe card.  They can also be
	 * generated by an IPMI BMC, either manually or in response to a
	 * watchdog timeout.  For example, see the "power diag" command in
	 * ports/sysutils/ipmitool.  They can also be generated by a
	 * hypervisor; see "bhyvectl --inject-nmi".
	 */

#ifdef KDB
	if (!claimed && (panic_on_nmi & 2) != 0) {
		if (debugger_on_panic) {
			printf("NMI/cpu%d ... going to debugger\n", cpu);
			claimed = kdb_trap(type, 0, frame);
		}
	}
#endif /* KDB */

	if (!claimed && panic_on_nmi != 0)
		panic("NMI");
}

void
nmi_handle_intr(u_int type, struct trapframe *frame)
{

#ifdef SMP
	if (nmi_is_broadcast) {
		nmi_call_kdb_smp(type, frame);
		return;
	}
#endif
	nmi_call_kdb(PCPU_GET(cpuid), type, frame);
}

static int hw_ibrs_active;
int hw_ibrs_ibpb_active;
int hw_ibrs_disable = 1;

SYSCTL_INT(_hw, OID_AUTO, ibrs_active, CTLFLAG_RD, &hw_ibrs_active, 0,
    "Indirect Branch Restricted Speculation active");

SYSCTL_NODE(_machdep_mitigations, OID_AUTO, ibrs,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Indirect Branch Restricted Speculation active");

SYSCTL_INT(_machdep_mitigations_ibrs, OID_AUTO, active, CTLFLAG_RD,
    &hw_ibrs_active, 0, "Indirect Branch Restricted Speculation active");

void
hw_ibrs_recalculate(bool for_all_cpus)
{
	if ((cpu_ia32_arch_caps & IA32_ARCH_CAP_IBRS_ALL) != 0) {
		x86_msr_op(MSR_IA32_SPEC_CTRL, (for_all_cpus ?
		    MSR_OP_RENDEZVOUS_ALL : MSR_OP_LOCAL) |
		    (hw_ibrs_disable != 0 ? MSR_OP_ANDNOT : MSR_OP_OR),
		    IA32_SPEC_CTRL_IBRS, NULL);
		hw_ibrs_active = hw_ibrs_disable == 0;
		hw_ibrs_ibpb_active = 0;
	} else {
		hw_ibrs_active = hw_ibrs_ibpb_active = (cpu_stdext_feature3 &
		    CPUID_STDEXT3_IBPB) != 0 && !hw_ibrs_disable;
	}
}

static int
hw_ibrs_disable_handler(SYSCTL_HANDLER_ARGS)
{
	int error, val;

	val = hw_ibrs_disable;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	hw_ibrs_disable = val != 0;
	hw_ibrs_recalculate(true);
	return (0);
}
SYSCTL_PROC(_hw, OID_AUTO, ibrs_disable, CTLTYPE_INT | CTLFLAG_RWTUN |
    CTLFLAG_NOFETCH | CTLFLAG_MPSAFE, NULL, 0, hw_ibrs_disable_handler, "I",
    "Disable Indirect Branch Restricted Speculation");

SYSCTL_PROC(_machdep_mitigations_ibrs, OID_AUTO, disable, CTLTYPE_INT |
    CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE, NULL, 0,
    hw_ibrs_disable_handler, "I",
    "Disable Indirect Branch Restricted Speculation");

int hw_ssb_active;
int hw_ssb_disable;

SYSCTL_INT(_hw, OID_AUTO, spec_store_bypass_disable_active, CTLFLAG_RD,
    &hw_ssb_active, 0,
    "Speculative Store Bypass Disable active");

SYSCTL_NODE(_machdep_mitigations, OID_AUTO, ssb,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Speculative Store Bypass Disable active");

SYSCTL_INT(_machdep_mitigations_ssb, OID_AUTO, active, CTLFLAG_RD,
    &hw_ssb_active, 0, "Speculative Store Bypass Disable active");

static void
hw_ssb_set(bool enable, bool for_all_cpus)
{

	if ((cpu_stdext_feature3 & CPUID_STDEXT3_SSBD) == 0) {
		hw_ssb_active = 0;
		return;
	}
	hw_ssb_active = enable;
	x86_msr_op(MSR_IA32_SPEC_CTRL,
	    (enable ? MSR_OP_OR : MSR_OP_ANDNOT) |
	    (for_all_cpus ? MSR_OP_SCHED_ALL : MSR_OP_LOCAL),
	    IA32_SPEC_CTRL_SSBD, NULL);
}

void
hw_ssb_recalculate(bool all_cpus)
{

	switch (hw_ssb_disable) {
	default:
		hw_ssb_disable = 0;
		/* FALLTHROUGH */
	case 0: /* off */
		hw_ssb_set(false, all_cpus);
		break;
	case 1: /* on */
		hw_ssb_set(true, all_cpus);
		break;
	case 2: /* auto */
		hw_ssb_set((cpu_ia32_arch_caps & IA32_ARCH_CAP_SSB_NO) != 0 ?
		    false : true, all_cpus);
		break;
	}
}

static int
hw_ssb_disable_handler(SYSCTL_HANDLER_ARGS)
{
	int error, val;

	val = hw_ssb_disable;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	hw_ssb_disable = val;
	hw_ssb_recalculate(true);
	return (0);
}
SYSCTL_PROC(_hw, OID_AUTO, spec_store_bypass_disable, CTLTYPE_INT |
    CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE, NULL, 0,
    hw_ssb_disable_handler, "I",
    "Speculative Store Bypass Disable (0 - off, 1 - on, 2 - auto)");

SYSCTL_PROC(_machdep_mitigations_ssb, OID_AUTO, disable, CTLTYPE_INT |
    CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE, NULL, 0,
    hw_ssb_disable_handler, "I",
    "Speculative Store Bypass Disable (0 - off, 1 - on, 2 - auto)");

int hw_mds_disable;

/*
 * Handler for Microarchitectural Data Sampling issues.  Really not a
 * pointer to C function: on amd64 the code must not change any CPU
 * architectural state except possibly %rflags. Also, it is always
 * called with interrupts disabled.
 */
void mds_handler_void(void);
void mds_handler_verw(void);
void mds_handler_ivb(void);
void mds_handler_bdw(void);
void mds_handler_skl_sse(void);
void mds_handler_skl_avx(void);
void mds_handler_skl_avx512(void);
void mds_handler_silvermont(void);
void (*mds_handler)(void) = mds_handler_void;

static int
sysctl_hw_mds_disable_state_handler(SYSCTL_HANDLER_ARGS)
{
	const char *state;

	if (mds_handler == mds_handler_void)
		state = "inactive";
	else if (mds_handler == mds_handler_verw)
		state = "VERW";
	else if (mds_handler == mds_handler_ivb)
		state = "software IvyBridge";
	else if (mds_handler == mds_handler_bdw)
		state = "software Broadwell";
	else if (mds_handler == mds_handler_skl_sse)
		state = "software Skylake SSE";
	else if (mds_handler == mds_handler_skl_avx)
		state = "software Skylake AVX";
	else if (mds_handler == mds_handler_skl_avx512)
		state = "software Skylake AVX512";
	else if (mds_handler == mds_handler_silvermont)
		state = "software Silvermont";
	else
		state = "unknown";
	return (SYSCTL_OUT(req, state, strlen(state)));
}

SYSCTL_PROC(_hw, OID_AUTO, mds_disable_state,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_hw_mds_disable_state_handler, "A",
    "Microarchitectural Data Sampling Mitigation state");

SYSCTL_NODE(_machdep_mitigations, OID_AUTO, mds,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Microarchitectural Data Sampling Mitigation state");

SYSCTL_PROC(_machdep_mitigations_mds, OID_AUTO, state,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_hw_mds_disable_state_handler, "A",
    "Microarchitectural Data Sampling Mitigation state");

_Static_assert(__offsetof(struct pcpu, pc_mds_tmp) % 64 == 0, "MDS AVX512");

void
hw_mds_recalculate(void)
{
	struct pcpu *pc;
	vm_offset_t b64;
	u_long xcr0;
	int i;

	/*
	 * Allow user to force VERW variant even if MD_CLEAR is not
	 * reported.  For instance, hypervisor might unknowingly
	 * filter the cap out.
	 * For the similar reasons, and for testing, allow to enable
	 * mitigation even when MDS_NO cap is set.
	 */
	if (cpu_vendor_id != CPU_VENDOR_INTEL || hw_mds_disable == 0 ||
	    ((cpu_ia32_arch_caps & IA32_ARCH_CAP_MDS_NO) != 0 &&
	    hw_mds_disable == 3)) {
		mds_handler = mds_handler_void;
	} else if (((cpu_stdext_feature3 & CPUID_STDEXT3_MD_CLEAR) != 0 &&
	    hw_mds_disable == 3) || hw_mds_disable == 1) {
		mds_handler = mds_handler_verw;
	} else if (CPUID_TO_FAMILY(cpu_id) == 0x6 &&
	    (CPUID_TO_MODEL(cpu_id) == 0x2e || CPUID_TO_MODEL(cpu_id) == 0x1e ||
	    CPUID_TO_MODEL(cpu_id) == 0x1f || CPUID_TO_MODEL(cpu_id) == 0x1a ||
	    CPUID_TO_MODEL(cpu_id) == 0x2f || CPUID_TO_MODEL(cpu_id) == 0x25 ||
	    CPUID_TO_MODEL(cpu_id) == 0x2c || CPUID_TO_MODEL(cpu_id) == 0x2d ||
	    CPUID_TO_MODEL(cpu_id) == 0x2a || CPUID_TO_MODEL(cpu_id) == 0x3e ||
	    CPUID_TO_MODEL(cpu_id) == 0x3a) &&
	    (hw_mds_disable == 2 || hw_mds_disable == 3)) {
		/*
		 * Nehalem, SandyBridge, IvyBridge
		 */
		CPU_FOREACH(i) {
			pc = pcpu_find(i);
			if (pc->pc_mds_buf == NULL) {
				pc->pc_mds_buf = malloc_domainset(672, M_TEMP,
				    DOMAINSET_PREF(pc->pc_domain), M_WAITOK);
				bzero(pc->pc_mds_buf, 16);
			}
		}
		mds_handler = mds_handler_ivb;
	} else if (CPUID_TO_FAMILY(cpu_id) == 0x6 &&
	    (CPUID_TO_MODEL(cpu_id) == 0x3f || CPUID_TO_MODEL(cpu_id) == 0x3c ||
	    CPUID_TO_MODEL(cpu_id) == 0x45 || CPUID_TO_MODEL(cpu_id) == 0x46 ||
	    CPUID_TO_MODEL(cpu_id) == 0x56 || CPUID_TO_MODEL(cpu_id) == 0x4f ||
	    CPUID_TO_MODEL(cpu_id) == 0x47 || CPUID_TO_MODEL(cpu_id) == 0x3d) &&
	    (hw_mds_disable == 2 || hw_mds_disable == 3)) {
		/*
		 * Haswell, Broadwell
		 */
		CPU_FOREACH(i) {
			pc = pcpu_find(i);
			if (pc->pc_mds_buf == NULL) {
				pc->pc_mds_buf = malloc_domainset(1536, M_TEMP,
				    DOMAINSET_PREF(pc->pc_domain), M_WAITOK);
				bzero(pc->pc_mds_buf, 16);
			}
		}
		mds_handler = mds_handler_bdw;
	} else if (CPUID_TO_FAMILY(cpu_id) == 0x6 &&
	    ((CPUID_TO_MODEL(cpu_id) == 0x55 && (cpu_id &
	    CPUID_STEPPING) <= 5) ||
	    CPUID_TO_MODEL(cpu_id) == 0x4e || CPUID_TO_MODEL(cpu_id) == 0x5e ||
	    (CPUID_TO_MODEL(cpu_id) == 0x8e && (cpu_id &
	    CPUID_STEPPING) <= 0xb) ||
	    (CPUID_TO_MODEL(cpu_id) == 0x9e && (cpu_id &
	    CPUID_STEPPING) <= 0xc)) &&
	    (hw_mds_disable == 2 || hw_mds_disable == 3)) {
		/*
		 * Skylake, KabyLake, CoffeeLake, WhiskeyLake,
		 * CascadeLake
		 */
		CPU_FOREACH(i) {
			pc = pcpu_find(i);
			if (pc->pc_mds_buf == NULL) {
				pc->pc_mds_buf = malloc_domainset(6 * 1024,
				    M_TEMP, DOMAINSET_PREF(pc->pc_domain),
				    M_WAITOK);
				b64 = (vm_offset_t)malloc_domainset(64 + 63,
				    M_TEMP, DOMAINSET_PREF(pc->pc_domain),
				    M_WAITOK);
				pc->pc_mds_buf64 = (void *)roundup2(b64, 64);
				bzero(pc->pc_mds_buf64, 64);
			}
		}
		xcr0 = rxcr(0);
		if ((xcr0 & XFEATURE_ENABLED_ZMM_HI256) != 0 &&
		    (cpu_stdext_feature & CPUID_STDEXT_AVX512DQ) != 0)
			mds_handler = mds_handler_skl_avx512;
		else if ((xcr0 & XFEATURE_ENABLED_AVX) != 0 &&
		    (cpu_feature2 & CPUID2_AVX) != 0)
			mds_handler = mds_handler_skl_avx;
		else
			mds_handler = mds_handler_skl_sse;
	} else if (CPUID_TO_FAMILY(cpu_id) == 0x6 &&
	    ((CPUID_TO_MODEL(cpu_id) == 0x37 ||
	    CPUID_TO_MODEL(cpu_id) == 0x4a ||
	    CPUID_TO_MODEL(cpu_id) == 0x4c ||
	    CPUID_TO_MODEL(cpu_id) == 0x4d ||
	    CPUID_TO_MODEL(cpu_id) == 0x5a ||
	    CPUID_TO_MODEL(cpu_id) == 0x5d ||
	    CPUID_TO_MODEL(cpu_id) == 0x6e ||
	    CPUID_TO_MODEL(cpu_id) == 0x65 ||
	    CPUID_TO_MODEL(cpu_id) == 0x75 ||
	    CPUID_TO_MODEL(cpu_id) == 0x1c ||
	    CPUID_TO_MODEL(cpu_id) == 0x26 ||
	    CPUID_TO_MODEL(cpu_id) == 0x27 ||
	    CPUID_TO_MODEL(cpu_id) == 0x35 ||
	    CPUID_TO_MODEL(cpu_id) == 0x36 ||
	    CPUID_TO_MODEL(cpu_id) == 0x7a))) {
		/* Silvermont, Airmont */
		CPU_FOREACH(i) {
			pc = pcpu_find(i);
			if (pc->pc_mds_buf == NULL)
				pc->pc_mds_buf = malloc(256, M_TEMP, M_WAITOK);
		}
		mds_handler = mds_handler_silvermont;
	} else {
		hw_mds_disable = 0;
		mds_handler = mds_handler_void;
	}
}

static void
hw_mds_recalculate_boot(void *arg __unused)
{

	hw_mds_recalculate();
}
SYSINIT(mds_recalc, SI_SUB_SMP, SI_ORDER_ANY, hw_mds_recalculate_boot, NULL);

static int
sysctl_mds_disable_handler(SYSCTL_HANDLER_ARGS)
{
	int error, val;

	val = hw_mds_disable;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val < 0 || val > 3)
		return (EINVAL);
	hw_mds_disable = val;
	hw_mds_recalculate();
	return (0);
}

SYSCTL_PROC(_hw, OID_AUTO, mds_disable, CTLTYPE_INT |
    CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_mds_disable_handler, "I",
    "Microarchitectural Data Sampling Mitigation "
    "(0 - off, 1 - on VERW, 2 - on SW, 3 - on AUTO)");

SYSCTL_PROC(_machdep_mitigations_mds, OID_AUTO, disable, CTLTYPE_INT |
    CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_mds_disable_handler, "I",
    "Microarchitectural Data Sampling Mitigation "
    "(0 - off, 1 - on VERW, 2 - on SW, 3 - on AUTO)");

/*
 * Intel Transactional Memory Asynchronous Abort Mitigation
 * CVE-2019-11135
 */
int x86_taa_enable;
int x86_taa_state;
enum {
	TAA_NONE	= 0,	/* No mitigation enabled */
	TAA_TSX_DISABLE	= 1,	/* Disable TSX via MSR */
	TAA_VERW	= 2,	/* Use VERW mitigation */
	TAA_AUTO	= 3,	/* Automatically select the mitigation */

	/* The states below are not selectable by the operator */

	TAA_TAA_UC	= 4,	/* Mitigation present in microcode */
	TAA_NOT_PRESENT	= 5	/* TSX is not present */
};

static void
taa_set(bool enable, bool all)
{

	x86_msr_op(MSR_IA32_TSX_CTRL,
	    (enable ? MSR_OP_OR : MSR_OP_ANDNOT) |
	    (all ? MSR_OP_RENDEZVOUS_ALL : MSR_OP_LOCAL),
	    IA32_TSX_CTRL_RTM_DISABLE | IA32_TSX_CTRL_TSX_CPUID_CLEAR,
	    NULL);
}

void
x86_taa_recalculate(void)
{
	static int taa_saved_mds_disable = 0;
	int taa_need = 0, taa_state = 0;
	int mds_disable = 0, need_mds_recalc = 0;

	/* Check CPUID.07h.EBX.HLE and RTM for the presence of TSX */
	if ((cpu_stdext_feature & CPUID_STDEXT_HLE) == 0 ||
	    (cpu_stdext_feature & CPUID_STDEXT_RTM) == 0) {
		/* TSX is not present */
		x86_taa_state = TAA_NOT_PRESENT;
		return;
	}

	/* Check to see what mitigation options the CPU gives us */
	if (cpu_ia32_arch_caps & IA32_ARCH_CAP_TAA_NO) {
		/* CPU is not suseptible to TAA */
		taa_need = TAA_TAA_UC;
	} else if (cpu_ia32_arch_caps & IA32_ARCH_CAP_TSX_CTRL) {
		/*
		 * CPU can turn off TSX.  This is the next best option
		 * if TAA_NO hardware mitigation isn't present
		 */
		taa_need = TAA_TSX_DISABLE;
	} else {
		/* No TSX/TAA specific remedies are available. */
		if (x86_taa_enable == TAA_TSX_DISABLE) {
			if (bootverbose)
				printf("TSX control not available\n");
			return;
		} else
			taa_need = TAA_VERW;
	}

	/* Can we automatically take action, or are we being forced? */
	if (x86_taa_enable == TAA_AUTO)
		taa_state = taa_need;
	else
		taa_state = x86_taa_enable;

	/* No state change, nothing to do */
	if (taa_state == x86_taa_state) {
		if (bootverbose)
			printf("No TSX change made\n");
		return;
	}

	/* Does the MSR need to be turned on or off? */
	if (taa_state == TAA_TSX_DISABLE)
		taa_set(true, true);
	else if (x86_taa_state == TAA_TSX_DISABLE)
		taa_set(false, true);

	/* Does MDS need to be set to turn on VERW? */
	if (taa_state == TAA_VERW) {
		taa_saved_mds_disable = hw_mds_disable;
		mds_disable = hw_mds_disable = 1;
		need_mds_recalc = 1;
	} else if (x86_taa_state == TAA_VERW) {
		mds_disable = hw_mds_disable = taa_saved_mds_disable;
		need_mds_recalc = 1;
	}
	if (need_mds_recalc) {
		hw_mds_recalculate();
		if (mds_disable != hw_mds_disable) {
			if (bootverbose)
				printf("Cannot change MDS state for TAA\n");
			/* Don't update our state */
			return;
		}
	}

	x86_taa_state = taa_state;
	return;
}

static void
taa_recalculate_boot(void * arg __unused)
{

	x86_taa_recalculate();
}
SYSINIT(taa_recalc, SI_SUB_SMP, SI_ORDER_ANY, taa_recalculate_boot, NULL);

SYSCTL_NODE(_machdep_mitigations, OID_AUTO, taa,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "TSX Asynchronous Abort Mitigation");

static int
sysctl_taa_handler(SYSCTL_HANDLER_ARGS)
{
	int error, val;

	val = x86_taa_enable;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val < TAA_NONE || val > TAA_AUTO)
		return (EINVAL);
	x86_taa_enable = val;
	x86_taa_recalculate();
	return (0);
}

SYSCTL_PROC(_machdep_mitigations_taa, OID_AUTO, enable, CTLTYPE_INT |
    CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_taa_handler, "I",
    "TAA Mitigation enablement control "
    "(0 - off, 1 - disable TSX, 2 - VERW, 3 - on AUTO)");

static int
sysctl_taa_state_handler(SYSCTL_HANDLER_ARGS)
{
	const char *state;

	switch (x86_taa_state) {
	case TAA_NONE:
		state = "inactive";
		break;
	case TAA_TSX_DISABLE:
		state = "TSX disabled";
		break;
	case TAA_VERW:
		state = "VERW";
		break;
	case TAA_TAA_UC:
		state = "Mitigated in microcode";
		break;
	case TAA_NOT_PRESENT:
		state = "TSX not present";
		break;
	default:
		state = "unknown";
	}

	return (SYSCTL_OUT(req, state, strlen(state)));
}

SYSCTL_PROC(_machdep_mitigations_taa, OID_AUTO, state,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_taa_state_handler, "A",
    "TAA Mitigation state");

int __read_frequently cpu_flush_rsb_ctxsw;
SYSCTL_INT(_machdep_mitigations, OID_AUTO, flush_rsb_ctxsw,
    CTLFLAG_RW | CTLFLAG_NOFETCH, &cpu_flush_rsb_ctxsw, 0,
    "Flush Return Stack Buffer on context switch");

SYSCTL_NODE(_machdep_mitigations, OID_AUTO, rngds,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "MCU Optimization, disable RDSEED mitigation");

int x86_rngds_mitg_enable = 1;
void
x86_rngds_mitg_recalculate(bool all_cpus)
{
	if ((cpu_stdext_feature3 & CPUID_STDEXT3_MCUOPT) == 0)
		return;
	x86_msr_op(MSR_IA32_MCU_OPT_CTRL,
	    (x86_rngds_mitg_enable ? MSR_OP_OR : MSR_OP_ANDNOT) |
	    (all_cpus ? MSR_OP_RENDEZVOUS_ALL : MSR_OP_LOCAL),
	    IA32_RNGDS_MITG_DIS, NULL);
}

static int
sysctl_rngds_mitg_enable_handler(SYSCTL_HANDLER_ARGS)
{
	int error, val;

	val = x86_rngds_mitg_enable;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	x86_rngds_mitg_enable = val;
	x86_rngds_mitg_recalculate(true);
	return (0);
}
SYSCTL_PROC(_machdep_mitigations_rngds, OID_AUTO, enable, CTLTYPE_INT |
    CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_rngds_mitg_enable_handler, "I",
    "MCU Optimization, disabling RDSEED mitigation control "
    "(0 - mitigation disabled (RDSEED optimized), 1 - mitigation enabled)");

static int
sysctl_rngds_state_handler(SYSCTL_HANDLER_ARGS)
{
	const char *state;

	if ((cpu_stdext_feature3 & CPUID_STDEXT3_MCUOPT) == 0) {
		state = "Not applicable";
	} else if (x86_rngds_mitg_enable == 0) {
		state = "RDSEED not serialized";
	} else {
		state = "Mitigated";
	}
	return (SYSCTL_OUT(req, state, strlen(state)));
}
SYSCTL_PROC(_machdep_mitigations_rngds, OID_AUTO, state,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_rngds_state_handler, "A",
    "MCU Optimization state");


/*
 * Zenbleed.
 *
 * No corresponding errata is publicly listed.  AMD has issued a security
 * bulletin (AMD-SB-7008), entitled "Cross-Process Information Leak".  This
 * document lists (as of August 2023) platform firmware's availability target
 * dates, with most being November/December 2023.  It will then be up to
 * motherboard manufacturers to produce corresponding BIOS updates, which will
 * happen with an inevitable lag.  Additionally, for a variety of reasons,
 * operators might not be able to apply them everywhere due.  On the side of
 * standalone CPU microcodes, no plans for availability have been published so
 * far.  However, a developer appearing to be an AMD employee has hardcoded in
 * Linux revision numbers of future microcodes that are presumed to fix the
 * vulnerability.
 *
 * Given the stability issues encountered with early microcode releases for Rome
 * (the only microcode publicly released so far) and the absence of official
 * communication on standalone CPU microcodes, we have opted instead for
 * matching by default all AMD Zen2 processors which, according to the
 * vulnerability's discoverer, are all affected (see
 * https://lock.cmpxchg8b.com/zenbleed.html).  This policy, also adopted by
 * OpenBSD, may be overriden using the tunable/sysctl
 * 'machdep.mitigations.zenbleed.enable'.  We might revise it later depending on
 * official statements, microcode updates' public availability and community
 * assessment that they actually fix the vulnerability without any instability
 * side effects.
 */

SYSCTL_NODE(_machdep_mitigations, OID_AUTO, zenbleed,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Zenbleed OS-triggered prevention (via chicken bit)");

/* 2 is auto, see below. */
int zenbleed_enable = 2;

void
zenbleed_sanitize_enable(void)
{
	/* Default to auto (2). */
	if (zenbleed_enable < 0 || zenbleed_enable > 2)
		zenbleed_enable = 2;
}

static bool
zenbleed_chicken_bit_applicable(void)
{
	/* Concerns only bare-metal AMD Zen2 processors. */
	return (cpu_vendor_id == CPU_VENDOR_AMD &&
	    CPUID_TO_FAMILY(cpu_id) == 0x17 &&
	    CPUID_TO_MODEL(cpu_id) >= 0x30 &&
	    vm_guest == VM_GUEST_NO);
}

static bool
zenbleed_chicken_bit_should_enable(void)
{
	/*
	 * Obey tunable/sysctl.
	 *
	 * As explained above, currently, the automatic setting (2) and the "on"
	 * one (1) have the same effect.  In the future, we might additionally
	 * check for specific microcode revisions as part of the automatic
	 * determination.
	 */
	return (zenbleed_enable != 0);
}

void
zenbleed_check_and_apply(bool all_cpus)
{
	bool set;

	if (!zenbleed_chicken_bit_applicable())
		return;

	set = zenbleed_chicken_bit_should_enable();

	x86_msr_op(MSR_DE_CFG,
	    (set ? MSR_OP_OR : MSR_OP_ANDNOT) |
	    (all_cpus ? MSR_OP_RENDEZVOUS_ALL : MSR_OP_LOCAL),
	    DE_CFG_ZEN2_FP_BACKUP_FIX_BIT, NULL);
}

static int
sysctl_zenbleed_enable_handler(SYSCTL_HANDLER_ARGS)
{
	int error, val;

	val = zenbleed_enable;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	zenbleed_enable = val;
	zenbleed_sanitize_enable();
	zenbleed_check_and_apply(true);
	return (0);
}
SYSCTL_PROC(_machdep_mitigations_zenbleed, OID_AUTO, enable, CTLTYPE_INT |
    CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_zenbleed_enable_handler, "I",
    "Enable Zenbleed OS-triggered mitigation (chicken bit) "
    "(0: Force disable, 1: Force enable, 2: Automatic determination)");

static int
sysctl_zenbleed_state_handler(SYSCTL_HANDLER_ARGS)
{
	const char *state;

	if (!zenbleed_chicken_bit_applicable())
		state = "Not applicable";
	else if (zenbleed_chicken_bit_should_enable())
		state = "Mitigation enabled";
	else
		state = "Mitigation disabled";
	return (SYSCTL_OUT(req, state, strlen(state)));
}
SYSCTL_PROC(_machdep_mitigations_zenbleed, OID_AUTO, state,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_zenbleed_state_handler, "A",
    "Zenbleed OS-triggered mitigation (chicken bit) state");


/*
 * Enable and restore kernel text write permissions.
 * Callers must ensure that disable_wp()/restore_wp() are executed
 * without rescheduling on the same core.
 */
bool
disable_wp(void)
{
	u_int cr0;

	cr0 = rcr0();
	if ((cr0 & CR0_WP) == 0)
		return (false);
	load_cr0(cr0 & ~CR0_WP);
	return (true);
}

void
restore_wp(bool old_wp)
{

	if (old_wp)
		load_cr0(rcr0() | CR0_WP);
}

bool
acpi_get_fadt_bootflags(uint16_t *flagsp)
{
#ifdef DEV_ACPI
	ACPI_TABLE_FADT *fadt;
	vm_paddr_t physaddr;

	physaddr = acpi_find_table(ACPI_SIG_FADT);
	if (physaddr == 0)
		return (false);
	fadt = acpi_map_table(physaddr, ACPI_SIG_FADT);
	if (fadt == NULL)
		return (false);
	*flagsp = fadt->BootFlags;
	acpi_unmap_table(fadt);
	return (true);
#else
	return (false);
#endif
}

DEFINE_IFUNC(, uint64_t, rdtsc_ordered, (void))
{
	bool cpu_is_amd = cpu_vendor_id == CPU_VENDOR_AMD ||
	    cpu_vendor_id == CPU_VENDOR_HYGON;

	if ((amd_feature & AMDID_RDTSCP) != 0)
		return (rdtscp);
	else if ((cpu_feature & CPUID_SSE2) != 0)
		return (cpu_is_amd ? rdtsc_ordered_mfence :
		    rdtsc_ordered_lfence);
	else
		return (rdtsc);
}
