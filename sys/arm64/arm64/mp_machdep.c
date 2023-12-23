/*-
 * Copyright (c) 2015-2016 The FreeBSD Foundation
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

#include "opt_acpi.h"
#include "opt_ddb.h"
#include "opt_kstack_pages.h"
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/csan.h>
#include <sys/domainset.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>

#include <machine/machdep.h>
#include <machine/cpu.h>
#include <machine/debug_monitor.h>
#include <machine/intr.h>
#include <machine/smp.h>
#ifdef VFP
#include <machine/vfp.h>
#endif

#ifdef DEV_ACPI
#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>
#endif

#ifdef FDT
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_cpu.h>
#endif

#include <dev/psci/psci.h>

#include "pic_if.h"

#define	MP_BOOTSTACK_SIZE	(kstack_pages * PAGE_SIZE)

#define	MP_QUIRK_CPULIST	0x01	/* The list of cpus may be wrong, */
					/* don't panic if one fails to start */
static uint32_t mp_quirks;

#ifdef FDT
static struct {
	const char *compat;
	uint32_t quirks;
} fdt_quirks[] = {
	{ "arm,foundation-aarch64",	MP_QUIRK_CPULIST },
	{ "arm,fvp-base",		MP_QUIRK_CPULIST },
	/* This is incorrect in some DTS files */
	{ "arm,vfp-base",		MP_QUIRK_CPULIST },
	{ NULL, 0 },
};
#endif

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

static void ipi_ast(void *);
static void ipi_hardclock(void *);
static void ipi_preempt(void *);
static void ipi_rendezvous(void *);
static void ipi_stop(void *);

#ifdef FDT
static u_int fdt_cpuid;
#endif

void mpentry(unsigned long cpuid);
void init_secondary(uint64_t);

/* Synchronize AP startup. */
static struct mtx ap_boot_mtx;

/* Used to initialize the PCPU ahead of calling init_secondary(). */
void *bootpcpu;

/* Stacks for AP initialization, discarded once idle threads are started. */
void *bootstack;
static void *bootstacks[MAXCPU];

/* Count of started APs, used to synchronize access to bootstack. */
static volatile int aps_started;

/* Set to 1 once we're ready to let the APs out of the pen. */
static volatile int aps_ready;

/* Temporary variables for init_secondary()  */
static void *dpcpu[MAXCPU - 1];

static bool
is_boot_cpu(uint64_t target_cpu)
{

	return (PCPU_GET_MPIDR(cpuid_to_pcpu[0]) == (target_cpu & CPU_AFF_MASK));
}

static void
release_aps(void *dummy __unused)
{
	int i, started;

	/* Only release CPUs if they exist */
	if (mp_ncpus == 1)
		return;

	intr_pic_ipi_setup(IPI_AST, "ast", ipi_ast, NULL);
	intr_pic_ipi_setup(IPI_PREEMPT, "preempt", ipi_preempt, NULL);
	intr_pic_ipi_setup(IPI_RENDEZVOUS, "rendezvous", ipi_rendezvous, NULL);
	intr_pic_ipi_setup(IPI_STOP, "stop", ipi_stop, NULL);
	intr_pic_ipi_setup(IPI_STOP_HARD, "stop hard", ipi_stop, NULL);
	intr_pic_ipi_setup(IPI_HARDCLOCK, "hardclock", ipi_hardclock, NULL);

	atomic_store_rel_int(&aps_ready, 1);
	/* Wake up the other CPUs */
	__asm __volatile(
	    "dsb ishst	\n"
	    "sev	\n"
	    ::: "memory");

	printf("Release APs...");

	started = 0;
	for (i = 0; i < 2000; i++) {
		if (atomic_load_acq_int(&smp_started) != 0) {
			printf("done\n");
			return;
		}
		/*
		 * Don't time out while we are making progress. Some large
		 * systems can take a while to start all CPUs.
		 */
		if (smp_cpus > started) {
			i = 0;
			started = smp_cpus;
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
	pmap_t pmap0;
	uint64_t mpidr;

	ptrauth_mp_start(cpu);

	/*
	 * Verify that the value passed in 'cpu' argument (aka context_id) is
	 * valid. Some older U-Boot based PSCI implementations are buggy,
	 * they can pass random value in it.
	 */
	mpidr = READ_SPECIALREG(mpidr_el1) & CPU_AFF_MASK;
	if (cpu >= MAXCPU || cpuid_to_pcpu[cpu] == NULL ||
	    PCPU_GET_MPIDR(cpuid_to_pcpu[cpu]) != mpidr) {
		for (cpu = 0; cpu < mp_maxid; cpu++)
			if (cpuid_to_pcpu[cpu] != NULL &&
			    PCPU_GET_MPIDR(cpuid_to_pcpu[cpu]) == mpidr)
				break;
		if ( cpu >= MAXCPU)
			panic("MPIDR for this CPU is not in pcpu table");
	}

	/*
	 * Identify current CPU. This is necessary to setup
	 * affinity registers and to provide support for
	 * runtime chip identification.
	 *
	 * We need this before signalling the CPU is ready to
	 * let the boot CPU use the results.
	 */
	pcpup = cpuid_to_pcpu[cpu];
	pcpup->pc_midr = get_midr();
	identify_cpu(cpu);

	/* Ensure the stores in identify_cpu have completed */
	atomic_thread_fence_acq_rel();

	/* Signal the BSP and spin until it has released all APs. */
	atomic_add_int(&aps_started, 1);
	while (!atomic_load_int(&aps_ready))
		__asm __volatile("wfe");

	/* Initialize curthread */
	KASSERT(PCPU_GET(idlethread) != NULL, ("no idle thread"));
	pcpup->pc_curthread = pcpup->pc_idlethread;
	schedinit_ap();

	/* Initialize curpmap to match TTBR0's current setting. */
	pmap0 = vmspace_pmap(&vmspace0);
	KASSERT(pmap_to_ttbr0(pmap0) == READ_SPECIALREG(ttbr0_el1),
	    ("pmap0 doesn't match cpu %ld's ttbr0", cpu));
	pcpup->pc_curpmap = pmap0;

	install_cpu_errata();

	intr_pic_init_secondary();

	/* Start per-CPU event timers. */
	cpu_initclocks_ap();

#ifdef VFP
	vfp_init_secondary();
#endif

	dbg_init();
	pan_enable();

	mtx_lock_spin(&ap_boot_mtx);
	atomic_add_rel_32(&smp_cpus, 1);
	if (smp_cpus == mp_ncpus) {
		/* enable IPI's, tlb shootdown, freezes etc */
		atomic_store_rel_int(&smp_started, 1);
	}
	mtx_unlock_spin(&ap_boot_mtx);

	kcsan_cpu_init(cpu);

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

	for (cpu = 1; cpu < mp_ncpus; cpu++) {
		if (bootstacks[cpu] != NULL)
			kmem_free(bootstacks[cpu], MP_BOOTSTACK_SIZE);
	}
}
SYSINIT(smp_after_idle_runnable, SI_SUB_SMP, SI_ORDER_ANY,
    smp_after_idle_runnable, NULL);

/*
 *  Send IPI thru interrupt controller.
 */
static void
pic_ipi_send(void *arg, cpuset_t cpus, u_int ipi)
{

	KASSERT(intr_irq_root_dev != NULL, ("%s: no root attached", __func__));

	/*
	 * Ensure that this CPU's stores will be visible to IPI
	 * recipients before starting to send the interrupts.
	 */
	dsb(ishst);

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

	PIC_ENABLE_INTR(intr_irq_root_dev, isrc);
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

#ifdef DDB
	dbg_register_sync(NULL);
#endif

	CPU_CLR_ATOMIC(cpu, &started_cpus);
	CPU_CLR_ATOMIC(cpu, &stopped_cpus);
	CTR0(KTR_SMP, "IPI_STOP (restart)");
}

struct cpu_group *
cpu_topo(void)
{
	struct cpu_group *dom, *root;
	int i;

	root = smp_topo_alloc(1);
	dom = smp_topo_alloc(vm_ndomains);

	root->cg_parent = NULL;
	root->cg_child = dom;
	CPU_COPY(&all_cpus, &root->cg_mask);
	root->cg_count = mp_ncpus;
	root->cg_children = vm_ndomains;
	root->cg_level = CG_SHARE_NONE;
	root->cg_flags = 0;

	/*
	 * Redundant layers will be collapsed by the caller so we don't need a
	 * special case for a single domain.
	 */
	for (i = 0; i < vm_ndomains; i++, dom++) {
		dom->cg_parent = root;
		dom->cg_child = NULL;
		CPU_COPY(&cpuset_domain[i], &dom->cg_mask);
		dom->cg_count = CPU_COUNT(&dom->cg_mask);
		dom->cg_children = 0;
		dom->cg_level = CG_SHARE_L3;
		dom->cg_flags = 0;
	}

	return (root);
}

/* Determine if we running MP machine */
int
cpu_mp_probe(void)
{

	/* ARM64TODO: Read the u bit of mpidr_el1 to determine this */
	return (1);
}

static int
enable_cpu_psci(uint64_t target_cpu, vm_paddr_t entry, u_int cpuid)
{
	int err;

	err = psci_cpu_on(target_cpu, entry, cpuid);
	if (err != PSCI_RETVAL_SUCCESS) {
		/*
		 * Panic here if INVARIANTS are enabled and PSCI failed to
		 * start the requested CPU.  psci_cpu_on() returns PSCI_MISSING
		 * to indicate we are unable to use it to start the given CPU.
		 */
		KASSERT(err == PSCI_MISSING ||
		    (mp_quirks & MP_QUIRK_CPULIST) == MP_QUIRK_CPULIST,
		    ("Failed to start CPU %u (%lx), error %d\n",
		    cpuid, target_cpu, err));
		return (EINVAL);
	}

	return (0);
}

static int
enable_cpu_spin(uint64_t cpu, vm_paddr_t entry, vm_paddr_t release_paddr)
{
	vm_paddr_t *release_addr;

	release_addr = pmap_mapdev(release_paddr, sizeof(*release_addr));
	if (release_addr == NULL)
		return (ENOMEM);

	*release_addr = entry;
	pmap_unmapdev(release_addr, sizeof(*release_addr));

	__asm __volatile(
	    "dsb sy	\n"
	    "sev	\n"
	    ::: "memory");

	return (0);
}

/*
 * Starts a given CPU. If the CPU is already running, i.e. it is the boot CPU,
 * do nothing. Returns true if the CPU is present and running.
 */
static bool
start_cpu(u_int cpuid, uint64_t target_cpu, int domain, vm_paddr_t release_addr)
{
	struct pcpu *pcpup;
	vm_size_t size;
	vm_paddr_t pa;
	int err, naps;

	/* Check we are able to start this cpu */
	if (cpuid > mp_maxid)
		return (false);

	/* Skip boot CPU */
	if (is_boot_cpu(target_cpu))
		return (true);

	KASSERT(cpuid < MAXCPU, ("Too many CPUs"));

	size = round_page(sizeof(*pcpup) + DPCPU_SIZE);
	pcpup = kmem_malloc_domainset(DOMAINSET_PREF(domain), size,
	    M_WAITOK | M_ZERO);
	pmap_disable_promotion((vm_offset_t)pcpup, size);
	pcpu_init(pcpup, cpuid, sizeof(struct pcpu));
	pcpup->pc_mpidr = target_cpu & CPU_AFF_MASK;
	bootpcpu = pcpup;

	dpcpu[cpuid - 1] = (void *)(pcpup + 1);
	dpcpu_init(dpcpu[cpuid - 1], cpuid);

	bootstacks[cpuid] = kmem_malloc_domainset(DOMAINSET_PREF(domain),
	    MP_BOOTSTACK_SIZE, M_WAITOK | M_ZERO);

	naps = atomic_load_int(&aps_started);
	bootstack = (char *)bootstacks[cpuid] + MP_BOOTSTACK_SIZE;

	printf("Starting CPU %u (%lx)\n", cpuid, target_cpu);
	pa = pmap_extract(kernel_pmap, (vm_offset_t)mpentry);

	/*
	 * A limited set of hardware we support can only do spintables and
	 * remain useful, due to lack of EL3.  Thus, we'll usually fall into the
	 * PSCI branch here.
	 */
	MPASS(release_addr == 0 || !psci_present);
	if (release_addr != 0)
		err = enable_cpu_spin(target_cpu, pa, release_addr);
	else
		err = enable_cpu_psci(target_cpu, pa, cpuid);

	if (err != 0) {
		pcpu_destroy(pcpup);
		dpcpu[cpuid - 1] = NULL;
		kmem_free(bootstacks[cpuid], MP_BOOTSTACK_SIZE);
		kmem_free(pcpup, size);
		bootstacks[cpuid] = NULL;
		mp_ncpus--;
		return (false);
	}

	/* Wait for the AP to switch to its boot stack. */
	while (atomic_load_int(&aps_started) < naps + 1)
		cpu_spinwait();
	CPU_SET(cpuid, &all_cpus);

	return (true);
}

#ifdef DEV_ACPI
static void
madt_handler(ACPI_SUBTABLE_HEADER *entry, void *arg)
{
	ACPI_MADT_GENERIC_INTERRUPT *intr;
	u_int *cpuid;
	u_int id;
	int domain;

	switch(entry->Type) {
	case ACPI_MADT_TYPE_GENERIC_INTERRUPT:
		intr = (ACPI_MADT_GENERIC_INTERRUPT *)entry;
		cpuid = arg;

		if (is_boot_cpu(intr->ArmMpidr))
			id = 0;
		else
			id = *cpuid;

		domain = 0;
#ifdef NUMA
		if (vm_ndomains > 1)
			domain = acpi_pxm_get_cpu_locality(intr->Uid);
#endif
		if (start_cpu(id, intr->ArmMpidr, domain, 0)) {
			MPASS(cpuid_to_pcpu[id] != NULL);
			cpuid_to_pcpu[id]->pc_acpi_id = intr->Uid;
			/*
			 * Don't increment for the boot CPU, its CPU ID is
			 * reserved.
			 */
			if (!is_boot_cpu(intr->ArmMpidr))
				(*cpuid)++;
		}

		break;
	default:
		break;
	}
}

static void
cpu_init_acpi(void)
{
	ACPI_TABLE_MADT *madt;
	vm_paddr_t physaddr;
	u_int cpuid;

	physaddr = acpi_find_table(ACPI_SIG_MADT);
	if (physaddr == 0)
		return;

	madt = acpi_map_table(physaddr, ACPI_SIG_MADT);
	if (madt == NULL) {
		printf("Unable to map the MADT, not starting APs\n");
		return;
	}
	/* Boot CPU is always 0 */
	cpuid = 1;
	acpi_walk_subtables(madt + 1, (char *)madt + madt->Header.Length,
	    madt_handler, &cpuid);

	acpi_unmap_table(madt);

#if MAXMEMDOM > 1
	acpi_pxm_set_cpu_locality();
#endif
}
#endif

#ifdef FDT
/*
 * Failure is indicated by failing to populate *release_addr.
 */
static void
populate_release_addr(phandle_t node, vm_paddr_t *release_addr)
{
	pcell_t buf[2];

	if (OF_getencprop(node, "cpu-release-addr", buf, sizeof(buf)) !=
	    sizeof(buf))
		return;

	*release_addr = (((uintptr_t)buf[0] << 32) | buf[1]);
}

static bool
start_cpu_fdt(u_int id, phandle_t node, u_int addr_size, pcell_t *reg)
{
	uint64_t target_cpu;
	vm_paddr_t release_addr;
	char *enable_method;
	int domain;
	int cpuid;

	target_cpu = reg[0];
	if (addr_size == 2) {
		target_cpu <<= 32;
		target_cpu |= reg[1];
	}

	if (is_boot_cpu(target_cpu))
		cpuid = 0;
	else
		cpuid = fdt_cpuid;

	/*
	 * If PSCI is present, we'll always use that -- the cpu_on method is
	 * mandated in both v0.1 and v0.2.  We'll check the enable-method if
	 * we don't have PSCI and use spin table if it's provided.
	 */
	release_addr = 0;
	if (!psci_present && cpuid != 0) {
		if (OF_getprop_alloc(node, "enable-method",
		    (void **)&enable_method) <= 0)
			return (false);

		if (strcmp(enable_method, "spin-table") != 0) {
			OF_prop_free(enable_method);
			return (false);
		}

		OF_prop_free(enable_method);
		populate_release_addr(node, &release_addr);
		if (release_addr == 0) {
			printf("Failed to fetch release address for CPU %u",
			    cpuid);
			return (false);
		}
	}

	if (!start_cpu(cpuid, target_cpu, 0, release_addr))
		return (false);

	/*
	 * Don't increment for the boot CPU, its CPU ID is reserved.
	 */
	if (!is_boot_cpu(target_cpu))
		fdt_cpuid++;

	/* Try to read the numa node of this cpu */
	if (vm_ndomains == 1 ||
	    OF_getencprop(node, "numa-node-id", &domain, sizeof(domain)) <= 0)
		domain = 0;
	cpuid_to_pcpu[cpuid]->pc_domain = domain;
	if (domain < MAXMEMDOM)
		CPU_SET(cpuid, &cpuset_domain[domain]);
	return (true);
}
static void
cpu_init_fdt(void)
{
	phandle_t node;
	int i;

	node = OF_peer(0);
	for (i = 0; fdt_quirks[i].compat != NULL; i++) {
		if (ofw_bus_node_is_compatible(node,
		    fdt_quirks[i].compat) != 0) {
			mp_quirks = fdt_quirks[i].quirks;
		}
	}
	fdt_cpuid = 1;
	ofw_cpu_early_foreach(start_cpu_fdt, true);
}
#endif

/* Initialize and fire up non-boot processors */
void
cpu_mp_start(void)
{
	uint64_t mpidr;

	mtx_init(&ap_boot_mtx, "ap boot", NULL, MTX_SPIN);

	/* CPU 0 is always boot CPU. */
	CPU_SET(0, &all_cpus);
	mpidr = READ_SPECIALREG(mpidr_el1) & CPU_AFF_MASK;
	cpuid_to_pcpu[0]->pc_mpidr = mpidr;

	cpu_desc_init();

	switch(arm64_bus_method) {
#ifdef DEV_ACPI
	case ARM64_BUS_ACPI:
		mp_quirks = MP_QUIRK_CPULIST;
		cpu_init_acpi();
		break;
#endif
#ifdef FDT
	case ARM64_BUS_FDT:
		cpu_init_fdt();
		break;
#endif
	default:
		break;
	}
}

/* Introduce rest of cores to the world */
void
cpu_mp_announce(void)
{
}

#ifdef DEV_ACPI
static void
cpu_count_acpi_handler(ACPI_SUBTABLE_HEADER *entry, void *arg)
{
	u_int *cores = arg;

	switch(entry->Type) {
	case ACPI_MADT_TYPE_GENERIC_INTERRUPT:
		(*cores)++;
		break;
	default:
		break;
	}
}

static u_int
cpu_count_acpi(void)
{
	ACPI_TABLE_MADT *madt;
	vm_paddr_t physaddr;
	u_int cores;

	physaddr = acpi_find_table(ACPI_SIG_MADT);
	if (physaddr == 0)
		return (0);

	madt = acpi_map_table(physaddr, ACPI_SIG_MADT);
	if (madt == NULL) {
		printf("Unable to map the MADT, not starting APs\n");
		return (0);
	}

	cores = 0;
	acpi_walk_subtables(madt + 1, (char *)madt + madt->Header.Length,
	    cpu_count_acpi_handler, &cores);

	acpi_unmap_table(madt);

	return (cores);
}
#endif

void
cpu_mp_setmaxid(void)
{
	int cores;

	mp_ncpus = 1;
	mp_maxid = 0;

	switch(arm64_bus_method) {
#ifdef DEV_ACPI
	case ARM64_BUS_ACPI:
		cores = cpu_count_acpi();
		if (cores > 0) {
			cores = MIN(cores, MAXCPU);
			if (bootverbose)
				printf("Found %d CPUs in the ACPI tables\n",
				    cores);
			mp_ncpus = cores;
			mp_maxid = cores - 1;
		}
		break;
#endif
#ifdef FDT
	case ARM64_BUS_FDT:
		cores = ofw_cpu_early_foreach(NULL, false);
		if (cores > 0) {
			cores = MIN(cores, MAXCPU);
			if (bootverbose)
				printf("Found %d CPUs in the device tree\n",
				    cores);
			mp_ncpus = cores;
			mp_maxid = cores - 1;
		}
		break;
#endif
	default:
		if (bootverbose)
			printf("No CPU data, limiting to 1 core\n");
		break;
	}

	if (TUNABLE_INT_FETCH("hw.ncpu", &cores)) {
		if (cores > 0 && cores < mp_ncpus) {
			mp_ncpus = cores;
			mp_maxid = cores - 1;
		}
	}
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
intr_ipi_dispatch(u_int ipi)
{
	struct intr_ipi *ii;

	ii = intr_ipi_lookup(ipi);
	if (ii->ii_count == NULL)
		panic("%s: not setup IPI %u", __func__, ipi);

	intr_ipi_increment_count(ii->ii_count, PCPU_GET(cpuid));

	ii->ii_handler(ii->ii_handler_arg);
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
