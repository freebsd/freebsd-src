/*-
 * Copyright (c) 2000 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/user.h>
#include <sys/dkstat.h>

#include <machine/atomic.h>
#include <machine/ipl.h>
#include <machine/globaldata.h>
#include <machine/pmap.h>
#include <machine/clock.h>

#define CHECKSTATE_USER	0
#define CHECKSTATE_SYS	1
#define CHECKSTATE_INTR	2

volatile u_int		checkstate_probed_cpus;
volatile u_int		checkstate_need_ast;
volatile u_int		checkstate_pending_ast;
struct proc*		checkstate_curproc[MAXCPU];
int			checkstate_cpustate[MAXCPU];
u_long			checkstate_pc[MAXCPU];
volatile u_int		resched_cpus;

int			boot_cpu_id;

/* Is forwarding of a interrupt to the CPU holding the ISR lock enabled ? */
int forward_irq_enabled = 1;
SYSCTL_INT(_machdep, OID_AUTO, forward_irq_enabled, CTLFLAG_RW,
	   &forward_irq_enabled, 0, "");

int
cpu_mp_probe()
{
	return (0);
}

void
cpu_mp_start()
{
}

void
cpu_mp_announce()
{
}

#define GD_TO_INDEX(pc, prof)				\
        ((int)(((u_quad_t)((pc) - (prof)->pr_off) *	\
            (u_quad_t)((prof)->pr_scale)) >> 16) & ~1)

static void
addupc_intr_forwarded(struct proc *p, int id, int *astmap)
{
	int i;
	struct uprof *prof;
	u_long pc;

	pc = checkstate_pc[id];
	prof = &p->p_stats->p_prof;
	if (pc >= prof->pr_off &&
	    (i = GD_TO_INDEX(pc, prof)) < prof->pr_size) {
		if ((p->p_sflag & PS_OWEUPC) == 0) {
			prof->pr_addr = pc;
			prof->pr_ticks = 1;
			p->p_sflag |= PS_OWEUPC;
		}
		*astmap |= (1 << id);
	}
}

static void
forwarded_statclock(int id, int pscnt, int *astmap)
{
	struct pstats *pstats;
	long rss;
	struct rusage *ru;
	struct vmspace *vm;
	int cpustate;
	struct proc *p;
#ifdef GPROF
	register struct gmonparam *g;
	int i;
#endif

	p = checkstate_curproc[id];
	cpustate = checkstate_cpustate[id];

	/* XXX */
	if (p->p_ithd)
		cpustate = CHECKSTATE_INTR;
	else if (p == cpuid_to_globaldata[id]->gd_idleproc)
		cpustate = CHECKSTATE_SYS;

	switch (cpustate) {
	case CHECKSTATE_USER:
		if (p->p_sflag & PS_PROFIL)
			addupc_intr_forwarded(p, id, astmap);
		if (pscnt > 1)
			return;
		p->p_uticks++;
		if (p->p_nice > NZERO)
			cp_time[CP_NICE]++;
		else
			cp_time[CP_USER]++;
		break;
	case CHECKSTATE_SYS:
#ifdef GPROF
		/*
		 * Kernel statistics are just like addupc_intr, only easier.
		 */
		g = &_gmonparam;
		if (g->state == GMON_PROF_ON) {
			i = checkstate_pc[id] - g->lowpc;
			if (i < g->textsize) {
				i /= HISTFRACTION * sizeof(*g->kcount);
				g->kcount[i]++;
			}
		}
#endif
		if (pscnt > 1)
			return;

		if (p == cpuid_to_globaldata[id]->gd_idleproc)
			cp_time[CP_IDLE]++;
		else {
			p->p_sticks++;
			cp_time[CP_SYS]++;
		}
		break;
	case CHECKSTATE_INTR:
	default:
#ifdef GPROF
		/*
		 * Kernel statistics are just like addupc_intr, only easier.
		 */
		g = &_gmonparam;
		if (g->state == GMON_PROF_ON) {
			i = checkstate_pc[id] - g->lowpc;
			if (i < g->textsize) {
				i /= HISTFRACTION * sizeof(*g->kcount);
				g->kcount[i]++;
			}
		}
#endif
		if (pscnt > 1)
			return;
		if (p)
			p->p_iticks++;
		cp_time[CP_INTR]++;
	}

	schedclock(p);
	
	/* Update resource usage integrals and maximums. */
	if ((pstats = p->p_stats) != NULL &&
	    (ru = &pstats->p_ru) != NULL &&
	    (vm = p->p_vmspace) != NULL) {
		ru->ru_ixrss += pgtok(vm->vm_tsize);
		ru->ru_idrss += pgtok(vm->vm_dsize);
		ru->ru_isrss += pgtok(vm->vm_ssize);
		rss = pgtok(vmspace_resident_count(vm));
		if (ru->ru_maxrss < rss)
			ru->ru_maxrss = rss;
	}
}

#define BETTER_CLOCK_DIAGNOSTIC

void
forward_statclock(int pscnt)
{
	int map;
	int id;
	int i;

	/* Kludge. We don't yet have separate locks for the interrupts
	 * and the kernel. This means that we cannot let the other processors
	 * handle complex interrupts while inhibiting them from entering
	 * the kernel in a non-interrupt context.
	 *
	 * What we can do, without changing the locking mechanisms yet,
	 * is letting the other processors handle a very simple interrupt
	 * (wich determines the processor states), and do the main
	 * work ourself.
	 */

	CTR1(KTR_SMP, "forward_statclock(%d)", pscnt);

	if (!smp_started || cold || panicstr)
		return;

	/* Step 1: Probe state   (user, cpu, interrupt, spinlock, idle ) */
	
	map = PCPU_GET(other_cpus) & ~stopped_cpus ;
	checkstate_probed_cpus = 0;
	if (map != 0)
		ipi_selected(map, IPI_CHECKSTATE);

	i = 0;
	while (checkstate_probed_cpus != map) {
		/* spin */
		i++;
		if (i == 100000) {
#ifdef BETTER_CLOCK_DIAGNOSTIC
			printf("forward_statclock: checkstate %x\n",
			       checkstate_probed_cpus);
#endif
			break;
		}
	}

	/*
	 * Step 2: walk through other processors processes, update ticks and 
	 * profiling info.
	 */
	
	map = 0;
	for (id = 0; id < mp_ncpus; id++) {
		if (id == PCPU_GET(cpuid))
			continue;
		if (((1 << id) & checkstate_probed_cpus) == 0)
			continue;
		forwarded_statclock(id, pscnt, &map);
	}
	if (map != 0) {
		checkstate_need_ast |= map;
		ipi_selected(map, IPI_AST);
		i = 0;
		while ((checkstate_need_ast & map) != 0) {
			/* spin */
			i++;
			if (i > 100000) { 
#ifdef BETTER_CLOCK_DIAGNOSTIC
				printf("forward_statclock: dropped ast 0x%x\n",
				       checkstate_need_ast & map);
#endif
				break;
			}
		}
	}
}

void
forward_hardclock(int pscnt)
{
	int map;
	int id;
	struct proc *p;
	struct pstats *pstats;
	int i;

	/* Kludge. We don't yet have separate locks for the interrupts
	 * and the kernel. This means that we cannot let the other processors
	 * handle complex interrupts while inhibiting them from entering
	 * the kernel in a non-interrupt context.
	 *
	 * What we can do, without changing the locking mechanisms yet,
	 * is letting the other processors handle a very simple interrupt
	 * (wich determines the processor states), and do the main
	 * work ourself.
	 */

	CTR1(KTR_SMP, "forward_hardclock(%d)", pscnt);

	if (!smp_started || cold || panicstr)
		return;

	/* Step 1: Probe state   (user, cpu, interrupt, spinlock, idle) */
	
	map = PCPU_GET(other_cpus) & ~stopped_cpus ;
	checkstate_probed_cpus = 0;
	if (map != 0)
		ipi_selected(map, IPI_CHECKSTATE);
	
	i = 0;
	while (checkstate_probed_cpus != map) {
		/* spin */
		i++;
		if (i == 100000) {
#ifdef BETTER_CLOCK_DIAGNOSTIC
			printf("forward_hardclock: checkstate %x\n",
			       checkstate_probed_cpus);
#endif
			breakpoint();
			break;
		}
	}

	/*
	 * Step 2: walk through other processors processes, update virtual 
	 * timer and profiling timer. If stathz == 0, also update ticks and 
	 * profiling info.
	 */
	
	map = 0;
	for (id = 0; id < mp_ncpus; id++) {
		if (id == PCPU_GET(cpuid))
			continue;
		if (((1 << id) & checkstate_probed_cpus) == 0)
			continue;
		p = checkstate_curproc[id];
		if (p) {
			pstats = p->p_stats;
			if (checkstate_cpustate[id] == CHECKSTATE_USER &&
			    timevalisset(&pstats->p_timer[ITIMER_VIRTUAL].it_value) &&
			    itimerdecr(&pstats->p_timer[ITIMER_VIRTUAL], tick) == 0) {
				p->p_sflag |= PS_ALRMPEND;
				map |= (1 << id);
			}
			if (timevalisset(&pstats->p_timer[ITIMER_PROF].it_value) &&
			    itimerdecr(&pstats->p_timer[ITIMER_PROF], tick) == 0) {
				p->p_sflag |= PS_PROFPEND;
				map |= (1 << id);
			}
		}
		if (stathz == 0) {
			forwarded_statclock( id, pscnt, &map);
		}
	}
	if (map != 0) {
		checkstate_need_ast |= map;
		ipi_selected(map, IPI_AST);
		i = 0;
		while ((checkstate_need_ast & map) != 0) {
			/* spin */
			i++;
			if (i > 100000) { 
#ifdef BETTER_CLOCK_DIAGNOSTIC
				printf("forward_hardclock: dropped ast 0x%x\n",
				       checkstate_need_ast & map);
#endif
				break;
			}
		}
	}
}

/*
 * send an IPI to a set of cpus.
 */
void
ipi_selected(u_int32_t cpus, u_int64_t ipi)
{
	struct globaldata *globaldata;

	CTR2(KTR_SMP, "ipi_selected: cpus: %x ipi: %lx", cpus, ipi);
	ia64_mf();
	while (cpus) {
		int cpuid = ffs(cpus) - 1;
		cpus &= ~(1 << cpuid);

		globaldata = globaldata_find(cpuid);
		if (globaldata) {
			atomic_set_64(&globaldata->gd_pending_ipis, ipi);
			ia64_mf();
#if 0
			CTR1(KTR_SMP, "calling alpha_pal_wripir(%d)", cpuid);
			alpha_pal_wripir(cpuid);
#endif
		}
	}
}

/*
 * send an IPI INTerrupt containing 'vector' to all CPUs, including myself
 */
void
ipi_all(u_int64_t ipi)
{
	ipi_selected(all_cpus, ipi);
}

/*
 * send an IPI to all CPUs EXCEPT myself
 */
void
ipi_all_but_self(u_int64_t ipi)
{
	ipi_selected(PCPU_GET(other_cpus), ipi);
}

/*
 * send an IPI to myself
 */
void
ipi_self(u_int64_t ipi)
{
	ipi_selected(1 << PCPU_GET(cpuid), ipi);
}

/*
 * Handle an IPI sent to this processor.
 */
void
smp_handle_ipi(struct trapframe *frame)
{
	u_int64_t ipis;
	u_int64_t ipi;
	int cpuid = PCPU_GET(cpuid);

	do {
		ipis = PCPU_GET(pending_ipis);
	} while (atomic_cmpset_64(PCPU_PTR(pending_ipis), ipis, 0));

	CTR1(KTR_SMP, "smp_handle_ipi(), ipis=%lx", ipis);
	while (ipis) {
		/*
		 * Find the lowest set bit.
		 */
		ipi = ipis & ~(ipis - 1);
		switch (ipi) {
		case IPI_INVLTLB:
			break;

		case IPI_RENDEZVOUS:
			CTR0(KTR_SMP, "IPI_RENDEZVOUS");
			smp_rendezvous_action();
			break;

		case IPI_AST:
			CTR0(KTR_SMP, "IPI_AST");
			atomic_clear_int(&checkstate_need_ast, 1<<cpuid);
			atomic_set_int(&checkstate_pending_ast, 1<<cpuid);
			break;

		case IPI_CHECKSTATE:
			CTR0(KTR_SMP, "IPI_CHECKSTATE");
			if ((frame->tf_cr_ipsr & IA64_PSR_CPL)
			    == IA64_PSR_CPL_USER)
				checkstate_cpustate[cpuid] = CHECKSTATE_USER;
			else if (curproc->p_intr_nesting_level == 1)
				checkstate_cpustate[cpuid] = CHECKSTATE_SYS;
			else
				checkstate_cpustate[cpuid] = CHECKSTATE_INTR;
			checkstate_curproc[cpuid] = PCPU_GET(curproc);
			atomic_set_int(&checkstate_probed_cpus, 1<<cpuid);
			break;

		case IPI_STOP:
			CTR0(KTR_SMP, "IPI_STOP");
			atomic_set_int(&stopped_cpus, 1<<cpuid);
			while ((started_cpus & (1<<cpuid)) == 0)
				ia64_mf();
			atomic_clear_int(&started_cpus, 1<<cpuid);
			atomic_clear_int(&stopped_cpus, 1<<cpuid);
			break;
		}
	}
}
