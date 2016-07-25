/*-
 * Copyright (c) 2015-2016 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
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
 */

#include "opt_kstack_pages.h"
#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
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

#include <machine/debug_monitor.h>
#include <machine/intr.h>
#include <machine/smp.h>
#ifdef VFP
#include <machine/vfp.h>
#endif

#ifdef FDT
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_cpu.h>
#endif

#include <dev/psci/psci.h>

#include "pic_if.h"

typedef void intr_ipi_send_t(void *, cpuset_t, u_int);
typedef void intr_ipi_handler_t(void *);

#define INTR_IPI_NAMELEN	(MAXCOMLEN + 1)
struct intr_ipi {
	intr_ipi_handler_t *	ii_handler;
	void *			ii_handler_arg;
	intr_ipi_send_t *	ii_send;
	void *			ii_send_arg;
	char			ii_name[INTR_IPI_NAMELEN];
	u_long *		ii_count;
};

static struct intr_ipi ipi_sources[INTR_IPI_COUNT];

static struct intr_ipi *intr_ipi_lookup(u_int);
static void intr_pic_ipi_setup(u_int, const char *, intr_ipi_handler_t *,
    void *);

boolean_t ofw_cpu_reg(phandle_t node, u_int, cell_t *);

extern struct pcpu __pcpu[];

static enum {
	CPUS_UNKNOWN,
#ifdef FDT
	CPUS_FDT,
#endif
} cpu_enum_method;

static device_identify_t arm64_cpu_identify;
static device_probe_t arm64_cpu_probe;
static device_attach_t arm64_cpu_attach;

static void ipi_ast(void *);
static void ipi_hardclock(void *);
static void ipi_preempt(void *);
static void ipi_rendezvous(void *);
static void ipi_stop(void *);

static int ipi_handler(void *arg);

struct mtx ap_boot_mtx;
struct pcb stoppcbs[MAXCPU];

#ifdef INVARIANTS
static uint32_t cpu_reg[MAXCPU][2];
#endif
static device_t cpu_list[MAXCPU];

/*
 * Not all systems boot from the first CPU in the device tree. To work around
 * this we need to find which CPU we have booted from so when we later
 * enable the secondary CPUs we skip this one.
 */
static int cpu0 = -1;

void mpentry(unsigned long cpuid);
void init_secondary(uint64_t);

uint8_t secondary_stacks[MAXCPU - 1][PAGE_SIZE * KSTACK_PAGES] __aligned(16);

/* Set to 1 once we're ready to let the APs out of the pen. */
volatile int aps_ready = 0;

/* Temporary variables for init_secondary()  */
void *dpcpu[MAXCPU - 1];

static device_method_t arm64_cpu_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	arm64_cpu_identify),
	DEVMETHOD(device_probe,		arm64_cpu_probe),
	DEVMETHOD(device_attach,	arm64_cpu_attach),

	DEVMETHOD_END
};

static devclass_t arm64_cpu_devclass;
static driver_t arm64_cpu_driver = {
	"arm64_cpu",
	arm64_cpu_methods,
	0
};

DRIVER_MODULE(arm64_cpu, cpu, arm64_cpu_driver, arm64_cpu_devclass, 0, 0);

static void
arm64_cpu_identify(driver_t *driver, device_t parent)
{

	if (device_find_child(parent, "arm64_cpu", -1) != NULL)
		return;
	if (BUS_ADD_CHILD(parent, 0, "arm64_cpu", -1) == NULL)
		device_printf(parent, "add child failed\n");
}

static int
arm64_cpu_probe(device_t dev)
{
	u_int cpuid;

	cpuid = device_get_unit(dev);
	if (cpuid >= MAXCPU || cpuid > mp_maxid)
		return (EINVAL);

	device_quiet(dev);
	return (0);
}

static int
arm64_cpu_attach(device_t dev)
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
	int cpu, i;

	intr_pic_ipi_setup(IPI_AST, "ast", ipi_ast, NULL);
	intr_pic_ipi_setup(IPI_PREEMPT, "preempt", ipi_preempt, NULL);
	intr_pic_ipi_setup(IPI_RENDEZVOUS, "rendezvous", ipi_rendezvous, NULL);
	intr_pic_ipi_setup(IPI_STOP, "stop", ipi_stop, NULL);
	intr_pic_ipi_setup(IPI_STOP_HARD, "stop hard", ipi_stop, NULL);
	intr_pic_ipi_setup(IPI_HARDCLOCK, "hardclock", ipi_hardclock, NULL);

	atomic_store_rel_int(&aps_ready, 1);
	/* Wake up the other CPUs */
	__asm __volatile("sev");

	printf("Release APs\n");

	for (i = 0; i < 2000; i++) {
		if (smp_started) {
			for (cpu = 0; cpu <= mp_maxid; cpu++) {
				if (CPU_ABSENT(cpu))
					continue;
				print_cpu_features(cpu);
			}
			return;
		}
		DELAY(1000);
	}

	printf("APs not started\n");
}
SYSINIT(start_aps, SI_SUB_SMP, SI_ORDER_FIRST, release_aps, NULL);

void
init_secondary(uint64_t cpu)
{
	struct pcpu *pcpup;

	pcpup = &__pcpu[cpu];
	/*
	 * Set the pcpu pointer with a backup in tpidr_el1 to be
	 * loaded when entering the kernel from userland.
	 */
	__asm __volatile(
	    "mov x18, %0 \n"
	    "msr tpidr_el1, %0" :: "r"(pcpup));

	/* Spin until the BSP releases the APs */
	while (!aps_ready)
		__asm __volatile("wfe");

	/* Initialize curthread */
	KASSERT(PCPU_GET(idlethread) != NULL, ("no idle thread"));
	pcpup->pc_curthread = pcpup->pc_idlethread;
	pcpup->pc_curpcb = pcpup->pc_idlethread->td_pcb;

	/*
	 * Identify current CPU. This is necessary to setup
	 * affinity registers and to provide support for
	 * runtime chip identification.
	 */
	identify_cpu();

	intr_pic_init_secondary();

	/* Start per-CPU event timers. */
	cpu_initclocks_ap();

#ifdef VFP
	vfp_init();
#endif

	dbg_monitor_init();

	/* Enable interrupts */
	intr_enable();

	mtx_lock_spin(&ap_boot_mtx);

	atomic_add_rel_32(&smp_cpus, 1);

	if (smp_cpus == mp_ncpus) {
		/* enable IPI's, tlb shootdown, freezes etc */
		atomic_store_rel_int(&smp_started, 1);
	}

	mtx_unlock_spin(&ap_boot_mtx);

	/* Enter the scheduler */
	sched_throw(NULL);

	panic("scheduler returned us to init_secondary");
	/* NOTREACHED */
}

/*
 *  Send IPI thru interrupt controller.
 */
static void
pic_ipi_send(void *arg, cpuset_t cpus, u_int ipi)
{

	KASSERT(intr_irq_root_dev != NULL, ("%s: no root attached", __func__));
	PIC_IPI_SEND(intr_irq_root_dev, arg, cpus, ipi);
}

/*
 *  Setup IPI handler on interrupt controller.
 *
 *  Not SMP coherent.
 */
static void
intr_pic_ipi_setup(u_int ipi, const char *name, intr_ipi_handler_t *hand,
    void *arg)
{
	struct intr_irqsrc *isrc;
	struct intr_ipi *ii;
	int error;

	KASSERT(intr_irq_root_dev != NULL, ("%s: no root attached", __func__));
	KASSERT(hand != NULL, ("%s: ipi %u no handler", __func__, ipi));

	error = PIC_IPI_SETUP(intr_irq_root_dev, ipi, &isrc);
	if (error != 0)
		return;

	isrc->isrc_handlers++;

	ii = intr_ipi_lookup(ipi);
	KASSERT(ii->ii_count == NULL, ("%s: ipi %u reused", __func__, ipi));

	ii->ii_handler = hand;
	ii->ii_handler_arg = arg;
	ii->ii_send = pic_ipi_send;
	ii->ii_send_arg = isrc;
	strlcpy(ii->ii_name, name, INTR_IPI_NAMELEN);
	ii->ii_count = intr_ipi_setup_counters(name);
}

static void
intr_ipi_send(cpuset_t cpus, u_int ipi)
{
	struct intr_ipi *ii;

	ii = intr_ipi_lookup(ipi);
	if (ii->ii_count == NULL)
		panic("%s: not setup IPI %u", __func__, ipi);

	ii->ii_send(ii->ii_send_arg, cpus, ipi);
}

static void
ipi_ast(void *dummy __unused)
{

	CTR0(KTR_SMP, "IPI_AST");
}

static void
ipi_hardclock(void *dummy __unused)
{

	CTR1(KTR_SMP, "%s: IPI_HARDCLOCK", __func__);
	hardclockintr();
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

	/* ARM64TODO: Read the u bit of mpidr_el1 to determine this */
	return (1);
}

#ifdef FDT
static boolean_t
cpu_init_fdt(u_int id, phandle_t node, u_int addr_size, pcell_t *reg)
{
	uint64_t target_cpu;
	struct pcpu *pcpup;
	vm_paddr_t pa;
	u_int cpuid;
	int err;

	/* Check we are able to start this cpu */
	if (id > mp_maxid)
		return (0);

	KASSERT(id < MAXCPU, ("Too many CPUs"));

	KASSERT(addr_size == 1 || addr_size == 2, ("Invalid register size"));
#ifdef INVARIANTS
	cpu_reg[id][0] = reg[0];
	if (addr_size == 2)
		cpu_reg[id][1] = reg[1];
#endif

	/* We are already running on cpu 0 */
	if (id == cpu0)
		return (1);

	/*
	 * Rotate the CPU IDs to put the boot CPU as CPU 0. We keep the other
	 * CPUs ordered as the are likely grouped into clusters so it can be
	 * useful to keep that property, e.g. for the GICv3 driver to send
	 * an IPI to all CPUs in the cluster.
	 */
	cpuid = id;
	if (cpuid < cpu0)
		cpuid += mp_maxid + 1;
	cpuid -= cpu0;

	pcpup = &__pcpu[cpuid];
	pcpu_init(pcpup, cpuid, sizeof(struct pcpu));

	dpcpu[cpuid - 1] = (void *)kmem_malloc(kernel_arena, DPCPU_SIZE,
	    M_WAITOK | M_ZERO);
	dpcpu_init(dpcpu[cpuid - 1], cpuid);

	target_cpu = reg[0];
	if (addr_size == 2) {
		target_cpu <<= 32;
		target_cpu |= reg[1];
	}

	printf("Starting CPU %u (%lx)\n", cpuid, target_cpu);
	pa = pmap_extract(kernel_pmap, (vm_offset_t)mpentry);

	err = psci_cpu_on(target_cpu, pa, cpuid);
	if (err != PSCI_RETVAL_SUCCESS) {
		/* Panic here if INVARIANTS are enabled */
		KASSERT(0, ("Failed to start CPU %u (%lx)\n", id,
		    target_cpu));

		pcpu_destroy(pcpup);
		kmem_free(kernel_arena, (vm_offset_t)dpcpu[cpuid - 1],
		    DPCPU_SIZE);
		dpcpu[cpuid - 1] = NULL;
		/* Notify the user that the CPU failed to start */
		printf("Failed to start CPU %u (%lx)\n", id, target_cpu);
	} else
		CPU_SET(cpuid, &all_cpus);

	return (1);
}
#endif

/* Initialize and fire up non-boot processors */
void
cpu_mp_start(void)
{

	mtx_init(&ap_boot_mtx, "ap boot", NULL, MTX_SPIN);

	CPU_SET(0, &all_cpus);

	switch(cpu_enum_method) {
#ifdef FDT
	case CPUS_FDT:
		KASSERT(cpu0 >= 0, ("Current CPU was not found"));
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
cpu_find_cpu0_fdt(u_int id, phandle_t node, u_int addr_size, pcell_t *reg)
{
	uint64_t mpidr_fdt, mpidr_reg;

	if (cpu0 < 0) {
		mpidr_fdt = reg[0];
		if (addr_size == 2) {
			mpidr_fdt <<= 32;
			mpidr_fdt |= reg[1];
		}

		mpidr_reg = READ_SPECIALREG(mpidr_el1);

		if ((mpidr_reg & 0xff00fffffful) == mpidr_fdt)
			cpu0 = id;
	}

	return (TRUE);
}

void
cpu_mp_setmaxid(void)
{
#ifdef FDT
	int cores;

	cores = ofw_cpu_early_foreach(cpu_find_cpu0_fdt, false);
	if (cores > 0) {
		cores = MIN(cores, MAXCPU);
		if (bootverbose)
			printf("Found %d CPUs in the device tree\n", cores);
		mp_ncpus = cores;
		mp_maxid = cores - 1;
		cpu_enum_method = CPUS_FDT;
		return;
	}
#endif

	if (bootverbose)
		printf("No CPU data, limiting to 1 core\n");
	mp_ncpus = 1;
	mp_maxid = 0;
}

/*
 *  Lookup IPI source.
 */
static struct intr_ipi *
intr_ipi_lookup(u_int ipi)
{

	if (ipi >= INTR_IPI_COUNT)
		panic("%s: no such IPI %u", __func__, ipi);

	return (&ipi_sources[ipi]);
}

/*
 *  interrupt controller dispatch function for IPIs. It should
 *  be called straight from the interrupt controller, when associated
 *  interrupt source is learned. Or from anybody who has an interrupt
 *  source mapped.
 */
void
intr_ipi_dispatch(u_int ipi, struct trapframe *tf)
{
	void *arg;
	struct intr_ipi *ii;

	ii = intr_ipi_lookup(ipi);
	if (ii->ii_count == NULL)
		panic("%s: not setup IPI %u", __func__, ipi);

	intr_ipi_increment_count(ii->ii_count, PCPU_GET(cpuid));

	/*
	 * Supply ipi filter with trapframe argument
	 * if none is registered.
	 */
	arg = ii->ii_handler_arg != NULL ? ii->ii_handler_arg : tf;
	ii->ii_handler(arg);
}

#ifdef notyet
/*
 *  Map IPI into interrupt controller.
 *
 *  Not SMP coherent.
 */
static int
ipi_map(struct intr_irqsrc *isrc, u_int ipi)
{
	boolean_t is_percpu;
	int error;

	if (ipi >= INTR_IPI_COUNT)
		panic("%s: no such IPI %u", __func__, ipi);

	KASSERT(intr_irq_root_dev != NULL, ("%s: no root attached", __func__));

	isrc->isrc_type = INTR_ISRCT_NAMESPACE;
	isrc->isrc_nspc_type = INTR_IRQ_NSPC_IPI;
	isrc->isrc_nspc_num = ipi_next_num;

	error = PIC_REGISTER(intr_irq_root_dev, isrc, &is_percpu);
	if (error == 0) {
		isrc->isrc_dev = intr_irq_root_dev;
		ipi_next_num++;
	}
	return (error);
}

/*
 *  Setup IPI handler to interrupt source.
 *
 *  Note that there could be more ways how to send and receive IPIs
 *  on a platform like fast interrupts for example. In that case,
 *  one can call this function with ASIF_NOALLOC flag set and then
 *  call intr_ipi_dispatch() when appropriate.
 *
 *  Not SMP coherent.
 */
int
intr_ipi_set_handler(u_int ipi, const char *name, intr_ipi_filter_t *filter,
    void *arg, u_int flags)
{
	struct intr_irqsrc *isrc;
	int error;

	if (filter == NULL)
		return(EINVAL);

	isrc = intr_ipi_lookup(ipi);
	if (isrc->isrc_ipifilter != NULL)
		return (EEXIST);

	if ((flags & AISHF_NOALLOC) == 0) {
		error = ipi_map(isrc, ipi);
		if (error != 0)
			return (error);
	}

	isrc->isrc_ipifilter = filter;
	isrc->isrc_arg = arg;
	isrc->isrc_handlers = 1;
	isrc->isrc_count = intr_ipi_setup_counters(name);
	isrc->isrc_index = 0; /* it should not be used in IPI case */

	if (isrc->isrc_dev != NULL) {
		PIC_ENABLE_INTR(isrc->isrc_dev, isrc);
		PIC_ENABLE_SOURCE(isrc->isrc_dev, isrc);
	}
	return (0);
}
#endif

/* Sending IPI */
void
ipi_all_but_self(u_int ipi)
{
	cpuset_t cpus;

	cpus = all_cpus;
	CPU_CLR(PCPU_GET(cpuid), &cpus);
	CTR2(KTR_SMP, "%s: ipi: %x", __func__, ipi);
	intr_ipi_send(cpus, ipi);
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

	CTR2(KTR_SMP, "%s: ipi: %x", __func__, ipi);
	intr_ipi_send(cpus, ipi);
}
