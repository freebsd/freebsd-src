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
#include <sys/ipl.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/user.h>
#include <sys/dkstat.h>

#include <machine/smp.h>
#include <machine/atomic.h>
#include <machine/globaldata.h>
#include <machine/pmap.h>
#include <machine/rpb.h>
#include <machine/clock.h>

#define CHECKSTATE_USER	0
#define CHECKSTATE_SYS	1
#define CHECKSTATE_INTR	2

volatile u_int	stopped_cpus;
volatile u_int	started_cpus;
volatile u_int	checkstate_probed_cpus;
volatile u_int	checkstate_need_ast;

/* Set to 1 once we're ready to let the APs out of the pen. */
static volatile int aps_ready = 0;

static struct mtx ap_boot_mtx;

struct proc*	checkstate_curproc[MAXCPU];
int		checkstate_cpustate[MAXCPU];
u_long		checkstate_pc[MAXCPU];
volatile u_int	resched_cpus;
void (*cpustop_restartfunc) __P((void));
int		mp_ncpus;

volatile int	smp_started;
int		boot_cpu_id;
u_int32_t	all_cpus;

static struct globaldata	*cpuid_to_globaldata[MAXCPU];

int smp_active = 0;	/* are the APs allowed to run? */
SYSCTL_INT(_machdep, OID_AUTO, smp_active, CTLFLAG_RW, &smp_active, 0, "");

static int smp_cpus = 1;	/* how many cpu's running */
SYSCTL_INT(_machdep, OID_AUTO, smp_cpus, CTLFLAG_RD, &smp_cpus, 0, "");

/* Enable forwarding of a signal to a process running on a different CPU */
static int forward_signal_enabled = 1;
SYSCTL_INT(_machdep, OID_AUTO, forward_signal_enabled, CTLFLAG_RW,
	   &forward_signal_enabled, 0, "");

/* Enable forwarding of roundrobin to all other cpus */
static int forward_roundrobin_enabled = 1;
SYSCTL_INT(_machdep, OID_AUTO, forward_roundrobin_enabled, CTLFLAG_RW,
	   &forward_roundrobin_enabled, 0, "");

/*
 * Communicate with a console running on a secondary processor.
 * Return 1 on failure.
 */
static int
smp_send_secondary_command(const char *command, int cpuid)
{
	u_int64_t mask = 1L << cpuid;
	struct pcs *cpu = LOCATE_PCS(hwrpb, cpuid);
	int i, len;

	/*
	 * Sanity check.
	 */
	len = strlen(command);
	if (len > sizeof(cpu->pcs_buffer.rxbuf)) {
		printf("smp_send_secondary_command: command '%s' too long\n",
		       command);
		return 0;
	}

	/*
	 * Wait for the rx bit to clear.
	 */
	for (i = 0; i < 100000; i++) {
		if (!(hwrpb->rpb_rxrdy & mask))
			break;
		DELAY(10);
	}
	if (hwrpb->rpb_rxrdy & mask)
		return 0;

	/*
	 * Write the command into the processor's buffer.
	 */
	bcopy(command, cpu->pcs_buffer.rxbuf, len);
	cpu->pcs_buffer.rxlen = len;

	/*
	 * Set the bit in the rxrdy mask and let the secondary try to
	 * handle the command.
	 */
	atomic_set_64(&hwrpb->rpb_rxrdy, mask);

	/*
	 * Wait for the rx bit to clear.
	 */
	for (i = 0; i < 100000; i++) {
		if (!(hwrpb->rpb_rxrdy & mask))
			break;
		DELAY(10);
	}
	if (hwrpb->rpb_rxrdy & mask)
		return 0;

	return 1;
}

void
smp_init_secondary(void)
{
	struct pcs *cpu;

	/* spin until all the AP's are ready */
	while (!aps_ready)
		/*spin*/ ;
 
	/*
	 * Record the globaldata pointer in the per-cpu system value.
	 */
	alpha_pal_wrval((u_int64_t) globalp);

	/*
	 * Point interrupt/exception vectors to our own.
	 */
	alpha_pal_wrent(XentInt, ALPHA_KENTRY_INT);
	alpha_pal_wrent(XentArith, ALPHA_KENTRY_ARITH);
	alpha_pal_wrent(XentMM, ALPHA_KENTRY_MM);
	alpha_pal_wrent(XentIF, ALPHA_KENTRY_IF);
	alpha_pal_wrent(XentUna, ALPHA_KENTRY_UNA);
	alpha_pal_wrent(XentSys, ALPHA_KENTRY_SYS);


	/* lower the ipl and take any pending machine check */
	mc_expected = 1;
	alpha_mb(); alpha_mb();
	alpha_pal_wrmces(7);
	(void)alpha_pal_swpipl(ALPHA_PSL_IPL_HIGH);
	mc_expected = 0;

        /*
         * Set curproc to our per-cpu idleproc so that mutexes have
         * something unique to lock with.
         */
        PCPU_SET(curproc, PCPU_GET(idleproc));
        PCPU_SET(spinlocks, NULL);

	/*
	 * Set flags in our per-CPU slot in the HWRPB.
	 */
	cpu = LOCATE_PCS(hwrpb, PCPU_GET(cpuid));
	cpu->pcs_flags &= ~PCS_BIP;
	cpu->pcs_flags |= PCS_RC;
	alpha_mb();

	/*
	 * XXX: doesn't idleproc already have a pcb from when it was
	 * kthread_create'd?
	 *
	 * cache idleproc's physical address.
	 */
	curproc->p_md.md_pcbpaddr = (struct pcb *)PCPU_GET(idlepcbphys);
	/*
	 * and make idleproc's trapframe pointer point to its
	 * stack pointer for sanity.
	 */
	curproc->p_md.md_tf =
	    (struct trapframe *)globalp->gd_idlepcb.apcb_ksp;

	mtx_lock_spin(&ap_boot_mtx);

	smp_cpus++;

	CTR0(KTR_SMP, "smp_init_secondary");

	PCPU_SET(other_cpus, all_cpus & ~(1 << PCPU_GET(cpuid)));

	printf("SMP: AP CPU #%d Launched!\n", PCPU_GET(cpuid));

	if (smp_cpus == mp_ncpus) {
		smp_started = 1;
		smp_active = 1;
		ipi_all(0);
	}

	mtx_unlock_spin(&ap_boot_mtx);

	while (smp_started == 0)
		alpha_mb(); /* nothing */

	microuptime(PCPU_PTR(switchtime));
	PCPU_SET(switchticks, ticks);

	/* ok, now grab sched_lock and enter the scheduler */
	(void)alpha_pal_swpipl(ALPHA_PSL_IPL_0);
	mtx_lock_spin(&sched_lock);
	cpu_throw();	/* doesn't return */

	panic("scheduler returned us to " __func__);
}

extern void smp_init_secondary_glue(void);

static int
smp_start_secondary(int cpuid)
{
	struct pcs *cpu = LOCATE_PCS(hwrpb, cpuid);
	struct pcs *bootcpu = LOCATE_PCS(hwrpb, hwrpb->rpb_primary_cpu_id);
	struct alpha_pcb *pcb = (struct alpha_pcb *) cpu->pcs_hwpcb;
	struct globaldata *globaldata;
	int i;
	size_t sz;

	if ((cpu->pcs_flags & PCS_PV) == 0) {
		printf("smp_start_secondary: cpu %d PALcode invalid\n", cpuid);
		return 0;
	}

	if (bootverbose)
		printf("smp_start_secondary: starting cpu %d\n", cpuid);

	sz = round_page(UPAGES * PAGE_SIZE);
	globaldata = malloc(sz, M_TEMP, M_NOWAIT);
	if (!globaldata) {
		printf("smp_start_secondary: can't allocate memory\n");
		return 0;
	}
	
	globaldata_init(globaldata, cpuid, sz);
	SLIST_INSERT_HEAD(&cpuhead, globaldata, gd_allcpu);

	/*
	 * Copy the idle pcb and setup the address to start executing.
	 * Use the pcb unique value to point the secondary at its globaldata
	 * structure.
	 */
	*pcb = globaldata->gd_idlepcb;
	pcb->apcb_unique = (u_int64_t)globaldata;
	hwrpb->rpb_restart = (u_int64_t) smp_init_secondary_glue;
	hwrpb->rpb_restart_val = (u_int64_t) smp_init_secondary_glue;
	hwrpb->rpb_checksum = hwrpb_checksum();

	/*
	 * Tell the cpu to start with the same PALcode as us.
	 */
	bcopy(&bootcpu->pcs_pal_rev, &cpu->pcs_pal_rev,
	      sizeof cpu->pcs_pal_rev);

	/*
	 * Set flags in cpu structure and push out write buffers to
	 * make sure the secondary sees it.
	 */
	cpu->pcs_flags |= PCS_CV|PCS_RC;
	cpu->pcs_flags &= ~PCS_BIP;
	alpha_mb();

	/*
	 * Fire it up and hope for the best.
	 */
	if (!smp_send_secondary_command("START\r\n", cpuid)) {
		printf("smp_start_secondary: can't send START command\n");
		free(globaldata, M_TEMP);
		return 0;
	}
	       
	/*
	 * Wait for the secondary to set the BIP flag in its structure.
	 */
	for (i = 0; i < 100000; i++) {
		if (cpu->pcs_flags & PCS_BIP)
			break;
		DELAY(10);
	}
	if (!(cpu->pcs_flags & PCS_BIP)) {
		printf("smp_start_secondary: secondary did not respond\n");
		free(globaldata, M_TEMP);
	}
	
	/*
	 * It worked (I think).
	 */
	if (bootverbose)
		printf("smp_start_secondary: cpu %d started\n", cpuid);
	return 1;
}

/*
 * Register a struct globaldata.
 */
void
globaldata_register(struct globaldata *globaldata)
{
	cpuid_to_globaldata[globaldata->gd_cpuid] = globaldata;
}

struct globaldata *
globaldata_find(int cpuid)
{
	return cpuid_to_globaldata[cpuid];
}

/* Other stuff */

/* lock around the MP rendezvous */
static struct mtx smp_rv_mtx;

static void
init_locks(void)
{
	mtx_init(&smp_rv_mtx, "smp rendezvous", MTX_SPIN);
	mtx_init(&ap_boot_mtx, "ap boot", MTX_SPIN);
}

void
mp_start()
{
	int i;
	int cpuid = PCPU_GET(cpuid);

	init_locks();

	mp_ncpus = 1;

	all_cpus = 1 << cpuid;
	for (i = 0; i < hwrpb->rpb_pcs_cnt; i++) {
		struct pcs *pcsp;

		if (i == cpuid)
			continue;
		pcsp = (struct pcs *)((char *)hwrpb + hwrpb->rpb_pcs_off +
		    (i * hwrpb->rpb_pcs_size));
		if ((pcsp->pcs_flags & PCS_PP) == 0)
			continue;
		if ((pcsp->pcs_flags & PCS_PA) == 0) {
			if (bootverbose)
				printf("CPU %d not available.\n", i);
			continue;
		}
		if ((pcsp->pcs_flags & PCS_PV) == 0) {
			if (bootverbose)
				printf("CPU %d does not have valid PALcode.\n",
				    i);
			continue;
		}
		if (i > MAXCPU) {
			if (bootverbose) {
				printf("CPU %d not supported.", i);
				printf("  Only %d CPUs supported.\n", MAXCPU);
			}
			continue;
		}
		all_cpus |= 1 << i;
		mp_ncpus++;
	}
	PCPU_SET(other_cpus, all_cpus & ~(1<<cpuid));

	for (i = 0; i < hwrpb->rpb_pcs_cnt; i++) {
		if (i == cpuid)
			continue;
		if (all_cpus & 1 << i)
			smp_start_secondary(i);
	}
}

void
mp_announce()
{

	printf("SMP System Detected: %d usable CPUs\n", mp_ncpus);
}

void
smp_invltlb()
{
}

#if 0
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

		p->p_sticks++;
		if (p == cpuid_to_globaldata[id]->gd_idleproc)
			cp_time[CP_IDLE]++;
		else
			cp_time[CP_SYS]++;
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
		KASSERT(p != NULL, ("NULL process in interrupt state"));
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
	
	mtx_lock_spin(&smp_rv_mtx);
	map = PCPU_GET(other_cpus) & ~stopped_cpus ;
	checkstate_probed_cpus = 0;
	if (map != 0)
		ipi_selected(map, IPI_CHECKSTATE);

	i = 0;
	while (checkstate_probed_cpus != map) {
		alpha_mb();
		/* spin */
		i++;
		if (i == 100000) {
#ifdef DIAGNOSTIC
			printf("forward_statclock: checkstate %x\n",
			       checkstate_probed_cpus);
#endif
			break;
		}
	}
	mtx_unlock_spin(&smp_rv_mtx);

	/*
	 * Step 2: walk through other processors processes, update ticks and 
	 * profiling info.
	 */
	
	map = 0;
	/* XXX: wrong, should walk bits in all_cpus */
	for (id = 0; id < mp_ncpus; id++) {
		if (id == PCPU_GET(cpuid))
			continue;
		if (((1 << id) & checkstate_probed_cpus) == 0)
			continue;
		forwarded_statclock(id, pscnt, &map);
	}
	if (map != 0) {
		mtx_lock_spin(&smp_rv_mtx);
		checkstate_need_ast |= map;
		ipi_selected(map, IPI_AST);
		i = 0;
		while ((checkstate_need_ast & map) != 0) {
			alpha_mb();
			/* spin */
			i++;
			if (i > 100000) { 
#ifdef DIAGNOSTIC
				printf("forward_statclock: dropped ast 0x%x\n",
				       checkstate_need_ast & map);
#endif
				break;
			}
		}
		mtx_unlock_spin(&smp_rv_mtx);
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
	
	mtx_lock_spin(&smp_rv_mtx);
	map = PCPU_GET(other_cpus) & ~stopped_cpus ;
	checkstate_probed_cpus = 0;
	if (map != 0)
		ipi_selected(map, IPI_CHECKSTATE);
	
	i = 0;
	while (checkstate_probed_cpus != map) {
		alpha_mb();
		/* spin */
		i++;
		if (i == 100000) {
#ifdef DIAGNOSTIC
			printf("forward_hardclock: checkstate %x\n",
			       checkstate_probed_cpus);
#endif
			breakpoint();
			break;
		}
	}
	mtx_unlock_spin(&smp_rv_mtx);

	/*
	 * Step 2: walk through other processors processes, update virtual 
	 * timer and profiling timer. If stathz == 0, also update ticks and 
	 * profiling info.
	 */
	
	map = 0;
	/* XXX: wrong, should walk bits in all_cpus */
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
		mtx_lock_spin(&smp_rv_mtx);
		checkstate_need_ast |= map;
		ipi_selected(map, IPI_AST);
		i = 0;
		while ((checkstate_need_ast & map) != 0) {
			alpha_mb();
			/* spin */
			i++;
			if (i > 100000) { 
#ifdef DIAGNOSTIC
				printf("forward_hardclock: dropped ast 0x%x\n",
				       checkstate_need_ast & map);
#endif
				break;
			}
		}
		mtx_unlock_spin(&smp_rv_mtx);
	}
}
#endif  /* 0 */

void
forward_signal(struct proc *p)
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

	CTR1(KTR_SMP, "forward_signal(%p)", p);

	if (!smp_started || cold || panicstr)
		return;
	if (!forward_signal_enabled)
		return;
	mtx_lock_spin(&sched_lock);
	while (1) {
		if (p->p_stat != SRUN) {
			mtx_unlock_spin(&sched_lock);
			return;
		}
		id = p->p_oncpu;
		mtx_unlock_spin(&sched_lock);
		if (id == 0xff)
			return;
		map = (1<<id);
		checkstate_need_ast |= map;
		ipi_selected(map, IPI_AST);
		i = 0;
		while ((checkstate_need_ast & map) != 0) {
			alpha_mb();
			/* spin */
			i++;
			if (i > 100000) { 
#if DIAGNOSTIC
				printf("forward_signal: dropped ast 0x%x\n",
				       checkstate_need_ast & map);
#endif
				break;
			}
		}
		mtx_lock_spin(&sched_lock);
		if (id == p->p_oncpu) {
			mtx_unlock_spin(&sched_lock);
			return;
		}
	}
}

void
forward_roundrobin(void)
{
	u_int map;
	int i;

	CTR0(KTR_SMP, "forward_roundrobin()");

	if (!smp_started || cold || panicstr)
		return;
	if (!forward_roundrobin_enabled)
		return;
	resched_cpus |= PCPU_GET(other_cpus);
	map = PCPU_GET(other_cpus) & ~stopped_cpus ;
	ipi_selected(map, IPI_AST);
	i = 0;
	while ((checkstate_need_ast & map) != 0) {
		alpha_mb();
		/* spin */
		i++;
		if (i > 100000) {
#if DIAGNOSTIC
			printf("forward_roundrobin: dropped ast 0x%x\n",
			       checkstate_need_ast & map);
#endif
			break;
		}
	}
}

/*
 * When called the executing CPU will send an IPI to all other CPUs
 *  requesting that they halt execution.
 *
 * Usually (but not necessarily) called with 'other_cpus' as its arg.
 *
 *  - Signals all CPUs in map to stop.
 *  - Waits for each to stop.
 *
 * Returns:
 *  -1: error
 *   0: NA
 *   1: ok
 *
 * XXX FIXME: this is not MP-safe, needs a lock to prevent multiple CPUs
 *            from executing at same time.
 */
int
stop_cpus(u_int map)
{
	int i;

	if (!smp_started)
		return 0;

	CTR1(KTR_SMP, "stop_cpus(%x)", map);

	/* send the stop IPI to all CPUs in map */
	ipi_selected(map, IPI_STOP);
	
	i = 0;
	while ((stopped_cpus & map) != map) {
		/* spin */
		i++;
#ifdef DIAGNOSTIC
		if (i == 100000) {
			printf("timeout stopping cpus\n");
			break;
		}
#endif
		alpha_mb();
	}

	return 1;
}


/*
 * Called by a CPU to restart stopped CPUs. 
 *
 * Usually (but not necessarily) called with 'stopped_cpus' as its arg.
 *
 *  - Signals all CPUs in map to restart.
 *  - Waits for each to restart.
 *
 * Returns:
 *  -1: error
 *   0: NA
 *   1: ok
 */
int
restart_cpus(u_int map)
{
	if (!smp_started)
		return 0;

	CTR1(KTR_SMP, "restart_cpus(%x)", map);

	started_cpus = map;		/* signal other cpus to restart */
	alpha_mb();

	while ((stopped_cpus & map) != 0) /* wait for each to clear its bit */
		alpha_mb();

	return 1;
}

/*
 * All-CPU rendezvous.  CPUs are signalled, all execute the setup function 
 * (if specified), rendezvous, execute the action function (if specified),
 * rendezvous again, execute the teardown function (if specified), and then
 * resume.
 *
 * Note that the supplied external functions _must_ be reentrant and aware
 * that they are running in parallel and in an unknown lock context.
 */
static void (*smp_rv_setup_func)(void *arg);
static void (*smp_rv_action_func)(void *arg);
static void (*smp_rv_teardown_func)(void *arg);
static void *smp_rv_func_arg;
static volatile int smp_rv_waiters[2];

void
smp_rendezvous_action(void)
{
	/* setup function */
	if (smp_rv_setup_func != NULL)
		smp_rv_setup_func(smp_rv_func_arg);
	/* spin on entry rendezvous */
	atomic_add_int(&smp_rv_waiters[0], 1);
	while (smp_rv_waiters[0] < mp_ncpus)
		alpha_mb();
	/* action function */
	if (smp_rv_action_func != NULL)
		smp_rv_action_func(smp_rv_func_arg);
	/* spin on exit rendezvous */
	atomic_add_int(&smp_rv_waiters[1], 1);
	while (smp_rv_waiters[1] < mp_ncpus)
		alpha_mb();
	/* teardown function */
	if (smp_rv_teardown_func != NULL)
		smp_rv_teardown_func(smp_rv_func_arg);
}

void
smp_rendezvous(void (* setup_func)(void *), 
	       void (* action_func)(void *),
	       void (* teardown_func)(void *),
	       void *arg)
{

	if (!smp_started) {
		if (setup_func != NULL)
			setup_func(arg);
		if (action_func != NULL)
			action_func(arg);
		if (teardown_func != NULL)
			teardown_func(arg);
		return;
	}
		
	/* obtain rendezvous lock */
	mtx_lock_spin(&smp_rv_mtx);

	/* set static function pointers */
	smp_rv_setup_func = setup_func;
	smp_rv_action_func = action_func;
	smp_rv_teardown_func = teardown_func;
	smp_rv_func_arg = arg;
	smp_rv_waiters[0] = 0;
	smp_rv_waiters[1] = 0;

	/* signal other processors, which will enter the IPI with interrupts off */
	ipi_all_but_self(IPI_RENDEZVOUS);

	/* call executor function */
	CTR2(KTR_SMP, "smp_rv: calling action = %p, arg = %p\n", action_func,
	    arg);
	smp_rendezvous_action();

	/* release lock */
	mtx_unlock_spin(&smp_rv_mtx);
}

/*
 * send an IPI to a set of cpus.
 */
void
ipi_selected(u_int32_t cpus, u_int64_t ipi)
{
	struct globaldata *globaldata;

	CTR2(KTR_SMP, "ipi_selected: cpus: %x ipi: %lx", cpus, ipi);
	alpha_mb();
	while (cpus) {
		int cpuid = ffs(cpus) - 1;
		cpus &= ~(1 << cpuid);

		globaldata = cpuid_to_globaldata[cpuid];
		if (globaldata) {
			atomic_set_64(&globaldata->gd_pending_ipis, ipi);
			alpha_mb();
			CTR1(KTR_SMP, "calling alpha_pal_wripir(%d)", cpuid);
			alpha_pal_wripir(cpuid);
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
	u_int64_t ipis = atomic_readandclear_64(PCPU_PTR(pending_ipis));
	u_int64_t ipi;
	int cpumask;

	cpumask = 1 << PCPU_GET(cpuid);

	CTR1(KTR_SMP, "smp_handle_ipi(), ipis=%lx", ipis);
	while (ipis) {
		/*
		 * Find the lowest set bit.
		 */
		ipi = ipis & ~(ipis - 1);
		ipis &= ~ipi;
		switch (ipi) {
		case IPI_INVLTLB:
			CTR0(KTR_SMP, "IPI_NVLTLB");
			ALPHA_TBIA();
			break;

		case IPI_RENDEZVOUS:
			CTR0(KTR_SMP, "IPI_RENDEZVOUS");
			smp_rendezvous_action();
			break;

		case IPI_AST:
			CTR0(KTR_SMP, "IPI_AST");
			atomic_clear_int(&checkstate_need_ast, cpumask);
			mtx_lock_spin(&sched_lock);
			curproc->p_sflag |= PS_ASTPENDING;
			mtx_unlock_spin(&sched_lock);
			break;

		case IPI_CHECKSTATE:
			CTR0(KTR_SMP, "IPI_CHECKSTATE");
			if (frame->tf_regs[FRAME_PS] & ALPHA_PSL_USERMODE)
				checkstate_cpustate[PCPU_GET(cpuid)] =
				    CHECKSTATE_USER;
			else if (curproc->p_intr_nesting_level == 1)
				checkstate_cpustate[PCPU_GET(cpuid)] =
				    CHECKSTATE_SYS;
			else
				checkstate_cpustate[PCPU_GET(cpuid)] =
				    CHECKSTATE_INTR;
			checkstate_curproc[PCPU_GET(cpuid)] = curproc;
			atomic_set_int(&checkstate_probed_cpus, cpumask);
			break;

		case IPI_STOP:
			CTR0(KTR_SMP, "IPI_STOP");
			atomic_set_int(&stopped_cpus, cpumask);
			while ((started_cpus & cpumask) == 0)
				alpha_mb();
			atomic_clear_int(&started_cpus, cpumask);
			atomic_clear_int(&stopped_cpus, cpumask);
			break;
		}
	}

	/*
	 * Dump console messages to the console.  XXX - we need to handle
	 * requests to provide PALcode to secondaries and to start up new
	 * secondaries that are added to the system on the fly.
	 */
	if (PCPU_GET(cpuid) == hwrpb->rpb_primary_cpu_id) {
		u_int cpuid;
		u_int64_t txrdy;
#ifdef DIAGNOSTIC
		struct pcs *cpu;
		char buf[81];
#endif

		alpha_mb();
		while (hwrpb->rpb_txrdy != 0) {
			cpuid = ffs(hwrpb->rpb_txrdy) - 1;
#ifdef DIAGNOSTIC
			cpu = LOCATE_PCS(hwrpb, cpuid);
			bcopy(&cpu->pcs_buffer.txbuf, buf,
			    cpu->pcs_buffer.txlen);
			buf[cpu->pcs_buffer.txlen] = '\0';
			printf("SMP From CPU%d: %s\n", cpuid, buf);
#endif
			do {
				txrdy = hwrpb->rpb_txrdy;
			} while (atomic_cmpset_64(&hwrpb->rpb_txrdy, txrdy,
			    txrdy & ~(1 << cpuid)) == 0);
		}
	}
}

static void
release_aps(void *dummy __unused)
{
	if (bootverbose)
		printf(__func__ ": releasing secondary CPUs\n");
	atomic_store_rel_int(&aps_ready, 1);
}

SYSINIT(start_aps, SI_SUB_SMP, SI_ORDER_FIRST, release_aps, NULL);
