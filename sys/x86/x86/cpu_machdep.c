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

#include "opt_atpic.h"
#include "opt_compat.h"
#include "opt_cpu.h"
#include "opt_ddb.h"
#include "opt_inet.h"
#include "opt_isa.h"
#include "opt_kstack_pages.h"
#include "opt_maxmem.h"
#include "opt_mp_watchdog.h"
#include "opt_perfmon.h"
#include "opt_platform.h"
#ifdef __i386__
#include "opt_npx.h"
#include "opt_apic.h"
#include "opt_xbox.h"
#endif

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#ifdef SMP
#include <sys/smp.h>
#endif
#include <sys/sysctl.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/cputypes.h>
#include <machine/specialreg.h>
#include <machine/md_var.h>
#include <machine/mp_watchdog.h>
#ifdef PERFMON
#include <machine/perfmon.h>
#endif
#include <machine/tss.h>
#ifdef SMP
#include <machine/smp.h>
#endif
#include <x86/acpica_machdep.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vm_param.h>

#define	STATE_RUNNING	0x0
#define	STATE_MWAIT	0x1
#define	STATE_SLEEPING	0x2

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

void
acpi_cpu_idle_mwait(uint32_t mwait_hint)
{
	int *state;

	/*
	 * XXXKIB.  Software coordination mode should be supported,
	 * but all Intel CPUs provide hardware coordination.
	 */

	state = (int *)PCPU_PTR(monitorbuf);
	KASSERT(*state == STATE_SLEEPING,
		("cpu_mwait_cx: wrong monitorbuf state"));
	*state = STATE_MWAIT;
	cpu_monitor(state, 0, 0);
	if (*state == STATE_MWAIT)
		cpu_mwait(MWAIT_INTRBREAK, mwait_hint);

	/*
	 * We should exit on any event that interrupts mwait, because
	 * that event might be a wanted interrupt.
	 */
	*state = STATE_RUNNING;
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

bool
cpu_mwait_usable(void)
{

	return ((cpu_feature2 & CPUID2_MON) != 0 && ((cpu_mon_mwait_flags &
	    (CPUID5_MON_MWAIT_EXT | CPUID5_MWAIT_INTRBREAK)) ==
	    (CPUID5_MON_MWAIT_EXT | CPUID5_MWAIT_INTRBREAK)));
}

void (*cpu_idle_hook)(sbintime_t) = NULL;	/* ACPI idle hook. */
static int	cpu_ident_amdc1e = 0;	/* AMD C1E supported. */
static int	idle_mwait = 1;		/* Use MONITOR/MWAIT for short idle. */
SYSCTL_INT(_machdep, OID_AUTO, idle_mwait, CTLFLAG_RWTUN, &idle_mwait,
    0, "Use MONITOR/MWAIT for short idle");

#ifndef PC98
static void
cpu_idle_acpi(sbintime_t sbt)
{
	int *state;

	state = (int *)PCPU_PTR(monitorbuf);
	*state = STATE_SLEEPING;

	/* See comments in cpu_idle_hlt(). */
	disable_intr();
	if (sched_runnable())
		enable_intr();
	else if (cpu_idle_hook)
		cpu_idle_hook(sbt);
	else
		acpi_cpu_c1();
	*state = STATE_RUNNING;
}
#endif /* !PC98 */

static void
cpu_idle_hlt(sbintime_t sbt)
{
	int *state;

	state = (int *)PCPU_PTR(monitorbuf);
	*state = STATE_SLEEPING;

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
	if (sched_runnable())
		enable_intr();
	else
		acpi_cpu_c1();
	*state = STATE_RUNNING;
}

static void
cpu_idle_mwait(sbintime_t sbt)
{
	int *state;

	state = (int *)PCPU_PTR(monitorbuf);
	*state = STATE_MWAIT;

	/* See comments in cpu_idle_hlt(). */
	disable_intr();
	if (sched_runnable()) {
		enable_intr();
		*state = STATE_RUNNING;
		return;
	}
	cpu_monitor(state, 0, 0);
	if (*state == STATE_MWAIT)
		__asm __volatile("sti; mwait" : : "a" (MWAIT_C1), "c" (0));
	else
		enable_intr();
	*state = STATE_RUNNING;
}

static void
cpu_idle_spin(sbintime_t sbt)
{
	int *state;
	int i;

	state = (int *)PCPU_PTR(monitorbuf);
	*state = STATE_RUNNING;

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

/*
 * C1E renders the local APIC timer dead, so we disable it by
 * reading the Interrupt Pending Message register and clearing
 * both C1eOnCmpHalt (bit 28) and SmiOnCmpHalt (bit 27).
 * 
 * Reference:
 *   "BIOS and Kernel Developer's Guide for AMD NPT Family 0Fh Processors"
 *   #32559 revision 3.00+
 */
#define	MSR_AMDK8_IPM		0xc0010055
#define	AMDK8_SMIONCMPHALT	(1ULL << 27)
#define	AMDK8_C1EONCMPHALT	(1ULL << 28)
#define	AMDK8_CMPHALT		(AMDK8_SMIONCMPHALT | AMDK8_C1EONCMPHALT)

void
cpu_probe_amdc1e(void)
{

	/*
	 * Detect the presence of C1E capability mostly on latest
	 * dual-cores (or future) k8 family.
	 */
	if (cpu_vendor_id == CPU_VENDOR_AMD &&
	    (cpu_id & 0x00000f00) == 0x00000f00 &&
	    (cpu_id & 0x0fff0000) >=  0x00040000) {
		cpu_ident_amdc1e = 1;
	}
}

#if defined(__i386__) && defined(PC98)
void (*cpu_idle_fn)(sbintime_t) = cpu_idle_hlt;
#else
void (*cpu_idle_fn)(sbintime_t) = cpu_idle_acpi;
#endif

void
cpu_idle(int busy)
{
	uint64_t msr;
	sbintime_t sbt = -1;

	CTR2(KTR_SPARE2, "cpu_idle(%d) at %d",
	    busy, curcpu);
#ifdef MP_WATCHDOG
	ap_watchdog(PCPU_GET(cpuid));
#endif

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
	if (cpu_ident_amdc1e && cpu_disable_c3_sleep) {
		msr = rdmsr(MSR_AMDK8_IPM);
		if (msr & AMDK8_CMPHALT)
			wrmsr(MSR_AMDK8_IPM, msr & ~AMDK8_CMPHALT);
	}

	/* Call main idle method. */
	cpu_idle_fn(sbt);

	/* Switch timers back into active mode. */
	if (!busy) {
		cpu_activeclock();
		critical_exit();
	}
out:
	CTR2(KTR_SPARE2, "cpu_idle(%d) at %d done",
	    busy, curcpu);
}

int
cpu_idle_wakeup(int cpu)
{
	struct pcpu *pcpu;
	int *state;

	pcpu = pcpu_find(cpu);
	state = (int *)pcpu->pc_monitorbuf;
	/*
	 * This doesn't need to be atomic since missing the race will
	 * simply result in unnecessary IPIs.
	 */
	if (*state == STATE_SLEEPING)
		return (0);
	if (*state == STATE_MWAIT)
		*state = STATE_RUNNING;
	return (1);
}

/*
 * Ordered by speed/power consumption.
 */
struct {
	void	*id_fn;
	char	*id_name;
} idle_tbl[] = {
	{ cpu_idle_spin, "spin" },
	{ cpu_idle_mwait, "mwait" },
	{ cpu_idle_hlt, "hlt" },
#if !defined(__i386__) || !defined(PC98)
	{ cpu_idle_acpi, "acpi" },
#endif
	{ NULL, NULL }
};

static int
idle_sysctl_available(SYSCTL_HANDLER_ARGS)
{
	char *avail, *p;
	int error;
	int i;

	avail = malloc(256, M_TEMP, M_WAITOK);
	p = avail;
	for (i = 0; idle_tbl[i].id_name != NULL; i++) {
		if (strstr(idle_tbl[i].id_name, "mwait") &&
		    (cpu_feature2 & CPUID2_MON) == 0)
			continue;
#if !defined(__i386__) || !defined(PC98)
		if (strcmp(idle_tbl[i].id_name, "acpi") == 0 &&
		    cpu_idle_hook == NULL)
			continue;
#endif
		p += sprintf(p, "%s%s", p != avail ? ", " : "",
		    idle_tbl[i].id_name);
	}
	error = sysctl_handle_string(oidp, avail, 0, req);
	free(avail, M_TEMP);
	return (error);
}

SYSCTL_PROC(_machdep, OID_AUTO, idle_available, CTLTYPE_STRING | CTLFLAG_RD,
    0, 0, idle_sysctl_available, "A", "list of available idle functions");

static int
idle_sysctl(SYSCTL_HANDLER_ARGS)
{
	char buf[16];
	int error;
	char *p;
	int i;

	p = "unknown";
	for (i = 0; idle_tbl[i].id_name != NULL; i++) {
		if (idle_tbl[i].id_fn == cpu_idle_fn) {
			p = idle_tbl[i].id_name;
			break;
		}
	}
	strncpy(buf, p, sizeof(buf));
	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	for (i = 0; idle_tbl[i].id_name != NULL; i++) {
		if (strstr(idle_tbl[i].id_name, "mwait") &&
		    (cpu_feature2 & CPUID2_MON) == 0)
			continue;
#if !defined(__i386__) || !defined(PC98)
		if (strcmp(idle_tbl[i].id_name, "acpi") == 0 &&
		    cpu_idle_hook == NULL)
			continue;
#endif
		if (strcmp(idle_tbl[i].id_name, buf))
			continue;
		cpu_idle_fn = idle_tbl[i].id_fn;
		return (0);
	}
	return (EINVAL);
}

SYSCTL_PROC(_machdep, OID_AUTO, idle, CTLTYPE_STRING | CTLFLAG_RW, 0, 0,
    idle_sysctl, "A", "currently selected idle function");
