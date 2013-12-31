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
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>

#include <machine/cpu.h>
#include <machine/smp.h>
#include <machine/pcb.h>
#include <machine/pte.h>
#include <machine/intr.h>
#include <machine/vmparam.h>
#ifdef VFP
#include <machine/vfp.h>
#endif
#ifdef CPU_MV_PJ4B
#include <arm/mv/mvwin.h>
#include <dev/fdt/fdt_common.h>
#endif

#include "opt_smp.h"

void *temp_pagetable;
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
void *dpcpu[MAXCPU - 1];

/* Determine if we running MP machine */
int
cpu_mp_probe(void)
{
	CPU_SETOF(0, &all_cpus);

	return (platform_mp_probe());
}

/* Start Application Processor via platform specific function */
static int
check_ap(void)
{
	uint32_t ms;

	for (ms = 0; ms < 2000; ++ms) {
		if ((mp_naps + 1) == mp_ncpus)
			return (0);		/* success */
		else
			DELAY(1000);
	}

	return (-2);
}

extern unsigned char _end[];

/* Initialize and fire up non-boot processors */
void
cpu_mp_start(void)
{
	int error, i;
	vm_offset_t temp_pagetable_va;
	vm_paddr_t addr, addr_end;

	mtx_init(&ap_boot_mtx, "ap boot", NULL, MTX_SPIN);

	/* Reserve memory for application processors */
	for(i = 0; i < (mp_ncpus - 1); i++)
		dpcpu[i] = (void *)kmem_malloc(kernel_arena, DPCPU_SIZE,
		    M_WAITOK | M_ZERO);
	temp_pagetable_va = (vm_offset_t)contigmalloc(L1_TABLE_SIZE,
	    M_TEMP, 0, 0x0, 0xffffffff, L1_TABLE_SIZE, 0);
	addr = KERNPHYSADDR;
	addr_end = (vm_offset_t)&_end - KERNVIRTADDR + KERNPHYSADDR;
	addr_end &= ~L1_S_OFFSET;
	addr_end += L1_S_SIZE;
	bzero((void *)temp_pagetable_va,  L1_TABLE_SIZE);
	for (addr = KERNPHYSADDR; addr <= addr_end; addr += L1_S_SIZE) { 
		((int *)(temp_pagetable_va))[addr >> L1_S_SHIFT] =
		    L1_TYPE_S|L1_SHARED|L1_S_C|L1_S_AP(AP_KRW)|L1_S_DOM(PMAP_DOMAIN_KERNEL)|addr;
		((int *)(temp_pagetable_va))[(addr -
			KERNPHYSADDR + KERNVIRTADDR) >> L1_S_SHIFT] = 
		    L1_TYPE_S|L1_SHARED|L1_S_C|L1_S_AP(AP_KRW)|L1_S_DOM(PMAP_DOMAIN_KERNEL)|addr;
	}

#if defined(CPU_MV_PJ4B)
	/* Add ARMADAXP registers required for snoop filter initialization */
	((int *)(temp_pagetable_va))[MV_BASE >> L1_S_SHIFT] =
	    L1_TYPE_S|L1_SHARED|L1_S_B|L1_S_AP(AP_KRW)|fdt_immr_pa;
#endif

	temp_pagetable = (void*)(vtophys(temp_pagetable_va));
	cpu_idcache_wbinv_all();
	cpu_l2cache_wbinv_all();

	/* Initialize boot code and start up processors */
	platform_mp_start_ap();

	/*  Check if ap's started properly */
	error = check_ap();
	if (error)
		printf("WARNING: Some AP's failed to start\n");
	else
		for (i = 1; i < mp_ncpus; i++)
			CPU_SET(i, &all_cpus);

	contigfree((void *)temp_pagetable_va, L1_TABLE_SIZE, M_TEMP);
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
	uint32_t loop_counter;
	int start = 0, end = 0;

	cpu_setup(NULL);
	setttb(pmap_pa);
	cpu_tlb_flushID();

	pc = &__pcpu[cpu];
	set_pcpu(pc);

	/*
	 * pcpu_init() updates queue, so it should not be executed in parallel
	 * on several cores
	 */
	while(mp_naps < (cpu - 1))
		;

	pcpu_init(pc, cpu, sizeof(struct pcpu));
	dpcpu_init(dpcpu[cpu - 1], cpu);

	/* Provide stack pointers for other processor modes. */
	set_stackptrs(cpu);

	/* Signal our startup to BSP */
	atomic_add_rel_32(&mp_naps, 1);

	/* Spin until the BSP releases the APs */
	while (!aps_ready)
		;

	/* Initialize curthread */
	KASSERT(PCPU_GET(idlethread) != NULL, ("no idle thread"));
	pc->pc_curthread = pc->pc_idlethread;
	pc->pc_curpcb = pc->pc_idlethread->td_pcb;
#ifdef VFP
	pc->pc_cpu = cpu;

	vfp_init();
#endif

	mtx_lock_spin(&ap_boot_mtx);

	atomic_add_rel_32(&smp_cpus, 1);

	if (smp_cpus == mp_ncpus) {
		/* enable IPI's, tlb shootdown, freezes etc */
		atomic_store_rel_int(&smp_started, 1);
		smp_active = 1;
	}

	mtx_unlock_spin(&ap_boot_mtx);

	/* Enable ipi */
#ifdef IPI_IRQ_START
	start = IPI_IRQ_START;
#ifdef IPI_IRQ_END
  	end = IPI_IRQ_END;
#else
	end = IPI_IRQ_START;
#endif
#endif
				
	for (int i = start; i <= end; i++)
		arm_unmask_irq(i);
	enable_interrupts(I32_bit);

	loop_counter = 0;
	while (smp_started == 0) {
		DELAY(100);
		loop_counter++;
		if (loop_counter == 1000)
			CTR0(KTR_SMP, "AP still wait for smp_started");
	}
	/* Start per-CPU event timers. */
	cpu_initclocks_ap();

	CTR0(KTR_SMP, "go into scheduler");
	platform_mp_init_secondary();

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

	ipi = pic_ipi_get((int)arg);

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
		ipi = pic_ipi_get(-1);
	}

	return (FILTER_HANDLED);
}

static void
release_aps(void *dummy __unused)
{
	uint32_t loop_counter;
	int start = 0, end = 0;

	if (mp_ncpus == 1)
		return;
#ifdef IPI_IRQ_START
	start = IPI_IRQ_START;
#ifdef IPI_IRQ_END
	end = IPI_IRQ_END;
#else
	end = IPI_IRQ_START;
#endif
#endif

	for (int i = start; i <= end; i++) {
		/*
		 * IPI handler
		 */
		/* 
		 * Use 0xdeadbeef as the argument value for irq 0,
		 * if we used 0, the intr code will give the trap frame
		 * pointer instead.
		 */
		arm_setup_irqhandler("ipi", ipi_handler, NULL, (void *)i, i,
		    INTR_TYPE_MISC | INTR_EXCL, NULL);

		/* Enable ipi */
		arm_unmask_irq(i);
	}
	atomic_store_rel_int(&aps_ready, 1);

	printf("Release APs\n");

	for (loop_counter = 0; loop_counter < 2000; loop_counter++) {
		if (smp_started)
			return;
		DELAY(1000);
	}
	printf("AP's not started\n");
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

	CPU_ZERO(&cpus);
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
