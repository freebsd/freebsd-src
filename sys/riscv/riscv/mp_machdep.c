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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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

boolean_t ofw_cpu_reg(phandle_t node, u_int, cell_t *);

uint32_t __riscv_boot_ap[MAXCPU];

static enum {
	CPUS_UNKNOWN,
#ifdef FDT
	CPUS_FDT,
#endif
} cpu_enum_method;

static device_identify_t riscv64_cpu_identify;
static device_probe_t riscv64_cpu_probe;
static device_attach_t riscv64_cpu_attach;

static int ipi_handler(void *);

struct pcb stoppcbs[MAXCPU];

extern uint32_t boot_hart;
extern cpuset_t all_harts;

#ifdef INVARIANTS
static uint32_t cpu_reg[MAXCPU][2];
#endif
static device_t cpu_list[MAXCPU];

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

static device_method_t riscv64_cpu_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	riscv64_cpu_identify),
	DEVMETHOD(device_probe,		riscv64_cpu_probe),
	DEVMETHOD(device_attach,	riscv64_cpu_attach),

	DEVMETHOD_END
};

static devclass_t riscv64_cpu_devclass;
static driver_t riscv64_cpu_driver = {
	"riscv64_cpu",
	riscv64_cpu_methods,
	0
};

DRIVER_MODULE(riscv64_cpu, cpu, riscv64_cpu_driver, riscv64_cpu_devclass, 0, 0);

static void
riscv64_cpu_identify(driver_t *driver, device_t parent)
{

	if (device_find_child(parent, "riscv64_cpu", -1) != NULL)
		return;
	if (BUS_ADD_CHILD(parent, 0, "riscv64_cpu", -1) == NULL)
		device_printf(parent, "add child failed\n");
}

static int
riscv64_cpu_probe(device_t dev)
{
	u_int cpuid;

	cpuid = device_get_unit(dev);
	if (cpuid >= MAXCPU || cpuid > mp_maxid)
		return (EINVAL);

	device_quiet(dev);
	return (0);
}

static int
riscv64_cpu_attach(device_t dev)
{
	const uint32_t *reg;
	size_t reg_size;
	u_int cpuid;
	int i;

	cpuid = device_get_unit(dev);

	if (cpuid >= MAXCPU || cpuid > mp_maxid)
		return (EINVAL);
	KASSERT(cpu_list[cpuid] == NULL, ("Already have cpu %u", cpuid));

	reg = cpu_get_cpuid(dev, &reg_size);
	if (reg == NULL)
		return (EINVAL);

	if (bootverbose) {
		device_printf(dev, "register <");
		for (i = 0; i < reg_size; i++)
			printf("%s%x", (i == 0) ? "" : " ", reg[i]);
		printf(">\n");
	}

	/* Set the device to start it later */
	cpu_list[cpuid] = dev;

	return (0);
}

static void
release_aps(void *dummy __unused)
{
	cpuset_t mask;
	int i;

	if (mp_ncpus == 1)
		return;

	/* Setup the IPI handler */
	riscv_setup_ipihandler(ipi_handler);

	atomic_store_rel_int(&aps_ready, 1);

	/* Wake up the other CPUs */
	mask = all_harts;
	CPU_CLR(boot_hart, &mask);

	printf("Release APs\n");

	sbi_send_ipi(mask.__bits);

	for (i = 0; i < 2000; i++) {
		if (smp_started)
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

	/*
	 * Identify current CPU. This is necessary to setup
	 * affinity registers and to provide support for
	 * runtime chip identification.
	 */
	identify_cpu();

	/* Enable software interrupts */
	riscv_unmask_ipi();

#ifndef EARLY_AP_STARTUP
	/* Start per-CPU event timers. */
	cpu_initclocks_ap();
#endif

	/* Enable external (PLIC) interrupts */
	csr_set(sie, SIE_SEIE);

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

	/*
	 * Assert that smp_after_idle_runnable condition is reasonable.
	 */
	MPASS(PCPU_GET(curpcb) == NULL);

	/* Enter the scheduler */
	sched_throw(NULL);

	panic("scheduler returned us to init_secondary");
	/* NOTREACHED */
}

static void
smp_after_idle_runnable(void *arg __unused)
{
	struct pcpu *pc;
	int cpu;

	for (cpu = 1; cpu <= mp_maxid; cpu++) {
		if (bootstacks[cpu] != NULL) {
			pc = pcpu_find(cpu);
			while (atomic_load_ptr(&pc->pc_curpcb) == NULL)
				cpu_spinwait();
			kmem_free((vm_offset_t)bootstacks[cpu], PAGE_SIZE);
		}
	}
}
SYSINIT(smp_after_idle_runnable, SI_SUB_SMP, SI_ORDER_ANY,
    smp_after_idle_runnable, NULL);

static int
ipi_handler(void *arg)
{
	u_int ipi_bitmap;
	u_int cpu, ipi;
	int bit;

	csr_clear(sip, SIP_SSIP);

	cpu = PCPU_GET(cpuid);

	mb();

	ipi_bitmap = atomic_readandclear_int(PCPU_PTR(pending_ipis));
	if (ipi_bitmap == 0)
		return (FILTER_HANDLED);

	while ((bit = ffs(ipi_bitmap))) {
		bit = (bit - 1);
		ipi = (1 << bit);
		ipi_bitmap &= ~ipi;

		mb();

		switch (ipi) {
		case IPI_AST:
			CTR0(KTR_SMP, "IPI_AST");
			break;
		case IPI_PREEMPT:
			CTR1(KTR_SMP, "%s: IPI_PREEMPT", __func__);
			sched_preempt(curthread);
			break;
		case IPI_RENDEZVOUS:
			CTR0(KTR_SMP, "IPI_RENDEZVOUS");
			smp_rendezvous_action();
			break;
		case IPI_STOP:
		case IPI_STOP_HARD:
			CTR0(KTR_SMP, (ipi == IPI_STOP) ? "IPI_STOP" : "IPI_STOP_HARD");
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
			break;
		case IPI_HARDCLOCK:
			CTR1(KTR_SMP, "%s: IPI_HARDCLOCK", __func__);
			hardclockintr();
			break;
		default:
			panic("Unknown IPI %#0x on cpu %d", ipi, curcpu);
		}
	}

	return (FILTER_HANDLED);
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
static boolean_t
cpu_init_fdt(u_int id, phandle_t node, u_int addr_size, pcell_t *reg)
{
	struct pcpu *pcpup;
	vm_paddr_t start_addr;
	uint64_t hart;
	u_int cpuid;
	int naps;
	int error;

	/* Check if this hart supports MMU. */
	if (OF_getproplen(node, "mmu-type") < 0)
		return (0);

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
		return (1);

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
		return (0);

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
			return (0);
		}
	}

	pcpup = &__pcpu[cpuid];
	pcpu_init(pcpup, cpuid, sizeof(struct pcpu));
	pcpup->pc_hart = hart;

	dpcpu[cpuid - 1] = (void *)kmem_malloc(DPCPU_SIZE, M_WAITOK | M_ZERO);
	dpcpu_init(dpcpu[cpuid - 1], cpuid);

	bootstacks[cpuid] = (void *)kmem_malloc(PAGE_SIZE, M_WAITOK | M_ZERO);

	naps = atomic_load_int(&aps_started);
	bootstack = (char *)bootstacks[cpuid] + PAGE_SIZE;

	printf("Starting CPU %u (hart %lx)\n", cpuid, hart);
	atomic_store_32(&__riscv_boot_ap[hart], 1);

	/* Wait for the AP to switch to its boot stack. */
	while (atomic_load_int(&aps_started) < naps + 1)
		cpu_spinwait();

	CPU_SET(cpuid, &all_cpus);
	CPU_SET(hart, &all_harts);

	return (1);
}
#endif

/* Initialize and fire up non-boot processors */
void
cpu_mp_start(void)
{

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
}

/* Introduce rest of cores to the world */
void
cpu_mp_announce(void)
{
}

static boolean_t
cpu_check_mmu(u_int id, phandle_t node, u_int addr_size, pcell_t *reg)
{

	/* Check if this hart supports MMU. */
	if (OF_getproplen(node, "mmu-type") < 0)
		return (0);

	return (1);
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
