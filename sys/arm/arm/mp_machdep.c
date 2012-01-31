/*-
 * Copyright (c) 2011 Semihalf.
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
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/pcpu.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/ktr.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>

#include <machine/cpu.h>
#include <machine/smp.h>
#include <machine/pcb.h>
#include <machine/pte.h>
#include <machine/intr.h>

extern struct pcpu __pcpu[];
/* used to hold the AP's until we are ready to release them */
struct mtx ap_boot_mtx;
struct pcb stoppcbs[MAXCPU];

/* # of Applications processors */
volatile int mp_naps;

/* Set to 1 once we're ready to let the APs out of the pen. */
volatile int aps_ready = 0;

static int ipi_handler(void *arg);
void set_stackptrs(int cpu);

/* Temporary variables for init_secondary()  */
void *dpcpu;

/* Determine if we running MP machine */
int
cpu_mp_probe(void)
{
	CPU_SETOF(0, &all_cpus);

	return (platform_mp_probe());
}

/* Start Application Processor via platform specific function */
static int
start_ap(int cpu)
{
	int cpus, ms;

	cpus = mp_naps;

	dpcpu = (void *)kmem_alloc(kernel_map, DPCPU_SIZE);
	if (platform_mp_start_ap(cpu) != 0)
		return (-1);			/* could not start AP */

	for (ms = 0; ms < 5000; ++ms) {
		cpu_dcache_wbinv_all();
		if (mp_naps > cpus)
			return (0);		/* success */
		else
			DELAY(1000);
	}

	return (-2);
}

/* Initialize and fire up non-boot processors */
void
cpu_mp_start(void)
{
	int error, i;

	mtx_init(&ap_boot_mtx, "ap boot", NULL, MTX_SPIN);

	for (i = 1; i < mp_ncpus; i++) {
		error = start_ap(i);
		if (error) {
			printf("AP #%d failed to start\n", i);
			continue;
		}

		printf("AP #%d started\n", i);
		CPU_SET(i, &all_cpus);
	}
}

/* Introduce rest of cores to the world */
void
cpu_mp_announce(void)
{

}

extern vm_paddr_t pmap_pa;
void
init_secondary(int cpu)
{
	struct pcpu *pc;

	cpu_setup(NULL);

	setttb(pmap_pa);
	cpu_tlb_flushID();

	pc = &__pcpu[cpu];
	set_pcpu(pc);
	pcpu_init(pc, cpu, sizeof(struct pcpu));

	dpcpu_init(dpcpu, cpu);

	/* Provide stack pointers for other processor modes. */
	set_stackptrs(cpu);

	/* Signal our startup to BSP */
	mp_naps++;
	cpu_dcache_wbinv_all();

	/* Spin until the BSP releases the APs */
	while (!aps_ready)
		cpu_dcache_wbinv_all();

	/* Initialize curthread */
	KASSERT(PCPU_GET(idlethread) != NULL, ("no idle thread"));
	pc->pc_curthread = pc->pc_idlethread;
	pc->pc_curpcb = pc->pc_idlethread->td_pcb;

	mtx_lock_spin(&ap_boot_mtx);

	atomic_store_rel_int(&smp_cpus, 1);

	if (smp_cpus == mp_ncpus) {
		/* enable IPI's, tlb shootdown, freezes etc */
		atomic_store_rel_int(&smp_started, 1);
		smp_active = 1;
	}

	mtx_unlock_spin(&ap_boot_mtx);

	/* Enable ipi */
	arm_unmask_irq(0);
	enable_interrupts(I32_bit);

	cpu_dcache_wbinv_all();
	while (smp_started == 0)
		cpu_dcache_wbinv_all();

	/* Start per-CPU event timers. */
	cpu_initclocks_ap();

	CTR0(KTR_SMP, "go into scheduler");

	/* Enter the scheduler */
	sched_throw(NULL);

	panic("scheduler returned us to %s", __func__);
	/* NOTREACHED */
}

static int
ipi_handler(void *arg)
{
	u_int	cpu, ipi;

	cpu = PCPU_GET(cpuid);

	ipi = pic_ipi_get();

	while ((ipi != 0x3ff)) {
		switch (ipi) {
		case IPI_RENDEZVOUS:
			CTR0(KTR_SMP, "IPI_RENDEZVOUS");
			smp_rendezvous_action();
			break;

		case IPI_AST:
			CTR0(KTR_SMP, "IPI_AST");
			break;

		case IPI_STOP:
		case IPI_STOP_HARD:
			/*
			 * IPI_STOP_HARD is mapped to IPI_STOP so it is not
			 * necessary to add it in the switch.
			 */
			CTR0(KTR_SMP, "IPI_STOP or IPI_STOP_HARD");

			savectx(&stoppcbs[cpu]);

			/* Indicate we are stopped */
			CPU_SET_ATOMIC(cpu, &stopped_cpus);

			/* Wait for restart */
			while (!CPU_ISSET(cpu, &started_cpus))
				cpu_spinwait();

			CPU_CLR_ATOMIC(cpu, &started_cpus);
			CPU_CLR_ATOMIC(cpu, &stopped_cpus);
			CTR0(KTR_SMP, "IPI_STOP (restart)");
			break;
		case IPI_PREEMPT:
			CTR1(KTR_SMP, "%s: IPI_PREEMPT", __func__);
			sched_preempt(curthread);
			break;
		case IPI_HARDCLOCK:
			CTR1(KTR_SMP, "%s: IPI_HARDCLOCK", __func__);
			hardclockintr();
			break;
		case IPI_TLB:
			CTR1(KTR_SMP, "%s: IPI_TLB", __func__);
			cpufuncs.cf_tlb_flushID();
			break;
		default:
			panic("Unknown IPI 0x%0x on cpu %d", ipi, curcpu);
		}

		pic_ipi_clear(ipi);
		ipi = pic_ipi_get();
	}

	return (FILTER_HANDLED);
}

static void
release_aps(void *dummy __unused)
{

	if (mp_ncpus == 1)
		return;

	/*
	 * IPI handler
	 */
	arm_setup_irqhandler("ipi", ipi_handler, NULL, NULL, 0,
	    INTR_TYPE_MISC | INTR_EXCL, NULL);

	/* Enable ipi */
	arm_unmask_irq(0);

	atomic_store_rel_int(&aps_ready, 1);

	printf("Release APs\n");

	while (smp_started == 0) {
		DELAY(5000);
	}
}

SYSINIT(start_aps, SI_SUB_SMP, SI_ORDER_FIRST, release_aps, NULL);

struct cpu_group *
cpu_topo(void)
{

	return (smp_topo_1level(CG_SHARE_L2, 1, 0));
}

void
cpu_mp_setmaxid(void)
{

	platform_mp_setmaxid();
}

/* Sending IPI */
void
ipi_all_but_self(u_int ipi)
{
	cpuset_t other_cpus;

	other_cpus = all_cpus;
	CPU_CLR(PCPU_GET(cpuid), &other_cpus);
	CTR2(KTR_SMP, "%s: ipi: %x", __func__, ipi);
	platform_ipi_send(other_cpus, ipi);
}

void
ipi_cpu(int cpu, u_int ipi)
{
	cpuset_t cpus;

	CPU_SET(cpu, &cpus);

	CTR3(KTR_SMP, "%s: cpu: %d, ipi: %x", __func__, cpu, ipi);
	platform_ipi_send(cpus, ipi);
}

void
ipi_selected(cpuset_t cpus, u_int ipi)
{

	CTR2(KTR_SMP, "%s: ipi: %x", __func__, ipi);
	platform_ipi_send(cpus, ipi);
}

void
tlb_broadcast(int ipi)
{

	if (smp_started)
		ipi_all_but_self(ipi);
}
