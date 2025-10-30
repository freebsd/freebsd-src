/*-
 * Copyright (c) 2015 The FreeBSD Foundation
 * Copyright (c) 2016 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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

#include "opt_kstack_pages.h"
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/cpuset.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>

#include <machine/intr.h>
#include <machine/smp.h>
#include <machine/sbi.h>

#ifdef FDT
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_cpu.h>
#endif

#define	MP_BOOTSTACK_SIZE	(kstack_pages * PAGE_SIZE)

uint32_t __riscv_boot_ap[MAXCPU];

static enum {
	CPUS_UNKNOWN,
#ifdef FDT
	CPUS_FDT,
#endif
} cpu_enum_method;

static void ipi_ast(void *);
static void ipi_hardclock(void *);
static void ipi_preempt(void *);
static void ipi_rendezvous(void *);
static void ipi_stop(void *);

extern uint32_t boot_hart;
extern cpuset_t all_harts;

#ifdef INVARIANTS
static uint32_t cpu_reg[MAXCPU][2];
#endif

void mpentry(u_long hartid);
void init_secondary(uint64_t);

static struct mtx ap_boot_mtx;

/* Stacks for AP initialization, discarded once idle threads are started. */
void *bootstack;
static void *bootstacks[MAXCPU];

/* Count of started APs, used to synchronize access to bootstack. */
static volatile int aps_started;

/* Set to 1 once we're ready to let the APs out of the pen. */
static volatile int aps_ready;

/* Temporary variables for init_secondary()  */
void *dpcpu[MAXCPU - 1];

static void
release_aps(void *dummy __unused)
{
	cpuset_t mask;
	int i;

	if (mp_ncpus == 1)
		return;

	/* Setup the IPI handlers */
	intr_ipi_setup(IPI_AST, "ast", ipi_ast, NULL);
	intr_ipi_setup(IPI_PREEMPT, "preempt", ipi_preempt, NULL);
	intr_ipi_setup(IPI_RENDEZVOUS, "rendezvous", ipi_rendezvous, NULL);
	intr_ipi_setup(IPI_STOP, "stop", ipi_stop, NULL);
	intr_ipi_setup(IPI_STOP_HARD, "stop hard", ipi_stop, NULL);
	intr_ipi_setup(IPI_HARDCLOCK, "hardclock", ipi_hardclock, NULL);

	atomic_store_rel_int(&aps_ready, 1);

	/* Wake up the other CPUs */
	mask = all_harts;
	CPU_CLR(boot_hart, &mask);

	printf("Release APs\n");

	sbi_send_ipi(mask.__bits);

	for (i = 0; i < 2000; i++) {
		if (atomic_load_acq_int(&smp_started))
			return;
		DELAY(1000);
	}

	printf("APs not started\n");
}
SYSINIT(start_aps, SI_SUB_SMP, SI_ORDER_FIRST, release_aps, NULL);

void
init_secondary(uint64_t hart)
{
	struct pcpu *pcpup;
	u_int cpuid;

	/* Renumber this cpu */
	cpuid = hart;
	if (cpuid < boot_hart)
		cpuid += mp_maxid + 1;
	cpuid -= boot_hart;

	/* Setup the pcpu pointer */
	pcpup = &__pcpu[cpuid];
	__asm __volatile("mv tp, %0" :: "r"(pcpup));

	/* Workaround: make sure wfi doesn't halt the hart */
	csr_set(sie, SIE_SSIE);
	csr_set(sip, SIE_SSIE);

	/* Signal the BSP and spin until it has released all APs. */
	atomic_add_int(&aps_started, 1);
	while (!atomic_load_int(&aps_ready))
		__asm __volatile("wfi");

	/* Initialize curthread */
	KASSERT(PCPU_GET(idlethread) != NULL, ("no idle thread"));
	pcpup->pc_curthread = pcpup->pc_idlethread;
	schedinit_ap();

	/* Setup and enable interrupts */
	intr_pic_init_secondary();

#ifndef EARLY_AP_STARTUP
	/* Start per-CPU event timers. */
	cpu_initclocks_ap();
#endif

	/* Activate this hart in the kernel pmap. */
	CPU_SET_ATOMIC(hart, &kernel_pmap->pm_active);

	/* Activate process 0's pmap. */
	pmap_activate_boot(vmspace_pmap(proc0.p_vmspace));

	mtx_lock_spin(&ap_boot_mtx);

	atomic_add_rel_32(&smp_cpus, 1);

	if (smp_cpus == mp_ncpus) {
		/* enable IPI's, tlb shootdown, freezes etc */
		atomic_store_rel_int(&smp_started, 1);
	}

	mtx_unlock_spin(&ap_boot_mtx);

	if (bootverbose)
		printf("Secondary CPU %u fully online\n", cpuid);

	/* Enter the scheduler */
	sched_ap_entry();

	panic("scheduler returned us to init_secondary");
	/* NOTREACHED */
}

static void
smp_after_idle_runnable(void *arg __unused)
{
	int cpu;

	if (mp_ncpus == 1)
		return;

	KASSERT(smp_started != 0, ("%s: SMP not started yet", __func__));

	/*
	 * Wait for all APs to handle an interrupt.  After that, we know that
	 * the APs have entered the scheduler at least once, so the boot stacks
	 * are safe to free.
	 */
	smp_rendezvous(smp_no_rendezvous_barrier, NULL,
	    smp_no_rendezvous_barrier, NULL);

	for (cpu = 1; cpu <= mp_maxid; cpu++) {
		if (bootstacks[cpu] != NULL)
			kmem_free(bootstacks[cpu], MP_BOOTSTACK_SIZE);
	}
}
SYSINIT(smp_after_idle_runnable, SI_SUB_SMP, SI_ORDER_ANY,
    smp_after_idle_runnable, NULL);

static void
ipi_ast(void *dummy __unused)
{
	CTR0(KTR_SMP, "IPI_AST");
}

static void
ipi_preempt(void *dummy __unused)
{
	CTR1(KTR_SMP, "%s: IPI_PREEMPT", __func__);
	sched_preempt(curthread);
}

static void
ipi_rendezvous(void *dummy __unused)
{
	CTR0(KTR_SMP, "IPI_RENDEZVOUS");
	smp_rendezvous_action();
}

static void
ipi_stop(void *dummy __unused)
{
	u_int cpu;

	CTR0(KTR_SMP, "IPI_STOP");

	cpu = PCPU_GET(cpuid);
	savectx(&stoppcbs[cpu]);

	/* Indicate we are stopped */
	CPU_SET_ATOMIC(cpu, &stopped_cpus);

	/* Wait for restart */
	while (!CPU_ISSET(cpu, &started_cpus))
		cpu_spinwait();

	CPU_CLR_ATOMIC(cpu, &started_cpus);
	CPU_CLR_ATOMIC(cpu, &stopped_cpus);
	CTR0(KTR_SMP, "IPI_STOP (restart)");

	/*
	 * The kernel debugger might have set a breakpoint,
	 * so flush the instruction cache.
	 */
	fence_i();
}

static void
ipi_hardclock(void *dummy __unused)
{
	CTR1(KTR_SMP, "%s: IPI_HARDCLOCK", __func__);
	hardclockintr();
}

struct cpu_group *
cpu_topo(void)
{

	return (smp_topo_none());
}

/* Determine if we running MP machine */
int
cpu_mp_probe(void)
{

	return (mp_ncpus > 1);
}

#ifdef FDT
static bool
cpu_check_mmu(u_int id __unused, phandle_t node, u_int addr_size __unused,
    pcell_t *reg __unused)
{
	char type[32];

	/* Check if this hart supports MMU. */
	if (OF_getprop(node, "mmu-type", (void *)type, sizeof(type)) == -1 ||
	    strncmp(type, "riscv,none", 10) == 0)
		return (false);

	return (true);
}

static bool
cpu_init_fdt(u_int id, phandle_t node, u_int addr_size, pcell_t *reg)
{
	struct pcpu *pcpup;
	vm_paddr_t start_addr;
	uint64_t hart;
	u_int cpuid;
	int naps;
	int error;

	if (!cpu_check_mmu(id, node, addr_size, reg))
		return (false);

	KASSERT(id < MAXCPU, ("Too many CPUs"));

	KASSERT(addr_size == 1 || addr_size == 2, ("Invalid register size"));
#ifdef INVARIANTS
	cpu_reg[id][0] = reg[0];
	if (addr_size == 2)
		cpu_reg[id][1] = reg[1];
#endif

	hart = reg[0];
	if (addr_size == 2) {
		hart <<= 32;
		hart |= reg[1];
	}

	KASSERT(hart < MAXCPU, ("Too many harts."));

	/* We are already running on this cpu */
	if (hart == boot_hart)
		return (true);

	/*
	 * Rotate the CPU IDs to put the boot CPU as CPU 0.
	 * We keep the other CPUs ordered.
	 */
	cpuid = hart;
	if (cpuid < boot_hart)
		cpuid += mp_maxid + 1;
	cpuid -= boot_hart;

	/* Check if we are able to start this cpu */
	if (cpuid > mp_maxid)
		return (false);

	/*
	 * Depending on the SBI implementation, APs are waiting either in
	 * locore.S or to be activated explicitly, via SBI call.
	 */
	if (sbi_probe_extension(SBI_EXT_ID_HSM) != 0) {
		start_addr = pmap_kextract((vm_offset_t)mpentry);
		error = sbi_hsm_hart_start(hart, start_addr, 0);
		if (error != 0) {
			mp_ncpus--;

			/* Send a warning to the user and continue. */
			printf("AP %u (hart %lu) failed to start, error %d\n",
			    cpuid, hart, error);
			return (false);
		}
	}

	pcpup = &__pcpu[cpuid];
	pcpu_init(pcpup, cpuid, sizeof(struct pcpu));
	pcpup->pc_hart = hart;

	dpcpu[cpuid - 1] = kmem_malloc(DPCPU_SIZE, M_WAITOK | M_ZERO);
	dpcpu_init(dpcpu[cpuid - 1], cpuid);

	bootstacks[cpuid] = kmem_malloc(MP_BOOTSTACK_SIZE, M_WAITOK | M_ZERO);

	naps = atomic_load_int(&aps_started);
	bootstack = (char *)bootstacks[cpuid] + MP_BOOTSTACK_SIZE;

	if (bootverbose)
		printf("Starting CPU %u (hart %lx)\n", cpuid, hart);
	atomic_store_32(&__riscv_boot_ap[hart], 1);

	/* Wait for the AP to switch to its boot stack. */
	while (atomic_load_int(&aps_started) < naps + 1)
		cpu_spinwait();

	CPU_SET(cpuid, &all_cpus);
	CPU_SET(hart, &all_harts);

	return (true);
}
#endif

/* Initialize and fire up non-boot processors */
void
cpu_mp_start(void)
{
	u_int cpu;

	mtx_init(&ap_boot_mtx, "ap boot", NULL, MTX_SPIN);

	CPU_SET(0, &all_cpus);
	CPU_SET(boot_hart, &all_harts);

	switch(cpu_enum_method) {
#ifdef FDT
	case CPUS_FDT:
		ofw_cpu_early_foreach(cpu_init_fdt, true);
		break;
#endif
	case CPUS_UNKNOWN:
		break;
	}

	CPU_FOREACH(cpu) {
		/* Already identified. */
		if (cpu == 0)
			continue;

		identify_cpu(cpu);
	}
}

/* Introduce rest of cores to the world */
void
cpu_mp_announce(void)
{
	u_int cpu;

	CPU_FOREACH(cpu) {
		/* Already announced. */
		if (cpu == 0)
			continue;

		printcpuinfo(cpu);
	}
}

void
cpu_mp_setmaxid(void)
{
	int cores;

#ifdef FDT
	cores = ofw_cpu_early_foreach(cpu_check_mmu, true);
	if (cores > 0) {
		cores = MIN(cores, MAXCPU);
		if (bootverbose)
			printf("Found %d CPUs in the device tree\n", cores);
		mp_ncpus = cores;
		mp_maxid = cores - 1;
		cpu_enum_method = CPUS_FDT;
	} else
#endif
	{
		if (bootverbose)
			printf("No CPU data, limiting to 1 core\n");
		mp_ncpus = 1;
		mp_maxid = 0;
	}

	if (TUNABLE_INT_FETCH("hw.ncpu", &cores)) {
		if (cores > 0 && cores < mp_ncpus) {
			mp_ncpus = cores;
			mp_maxid = cores - 1;
		}
	}
}

void
ipi_all_but_self(u_int ipi)
{
	cpuset_t other_cpus;

	other_cpus = all_cpus;
	CPU_CLR(PCPU_GET(cpuid), &other_cpus);

	CTR2(KTR_SMP, "%s: ipi: %x", __func__, ipi);
	intr_ipi_send(other_cpus, ipi);
}

void
ipi_cpu(int cpu, u_int ipi)
{
	cpuset_t cpus;

	CPU_ZERO(&cpus);
	CPU_SET(cpu, &cpus);

	CTR3(KTR_SMP, "%s: cpu: %d, ipi: %x", __func__, cpu, ipi);
	intr_ipi_send(cpus, ipi);
}

void
ipi_selected(cpuset_t cpus, u_int ipi)
{
	CTR1(KTR_SMP, "ipi_selected: ipi: %x", ipi);
	intr_ipi_send(cpus, ipi);
}
