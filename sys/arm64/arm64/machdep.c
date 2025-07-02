/*-
 * Copyright (c) 2014 Andrew Turner
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
 */

#include "opt_acpi.h"
#include "opt_kstack_pages.h"
#include "opt_platform.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/asan.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/cpu.h>
#include <sys/csan.h>
#include <sys/efi.h>
#include <sys/efi_map.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/limits.h>
#include <sys/linker.h>
#include <sys/msan.h>
#include <sys/msgbuf.h>
#include <sys/pcpu.h>
#include <sys/physmem.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/reg.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/ucontext.h>
#include <sys/vdso.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_pager.h>

#include <machine/armreg.h>
#include <machine/cpu.h>
#include <machine/cpu_feat.h>
#include <machine/debug_monitor.h>
#include <machine/hypervisor.h>
#include <machine/kdb.h>
#include <machine/machdep.h>
#include <machine/metadata.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/undefined.h>
#include <machine/vmparam.h>

#ifdef VFP
#include <machine/vfp.h>
#endif

#ifdef DEV_ACPI
#include <contrib/dev/acpica/include/acpi.h>
#include <machine/acpica_machdep.h>
#endif

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#endif

#include <dev/smbios/smbios.h>

_Static_assert(sizeof(struct pcb) == 1248, "struct pcb is incorrect size");
_Static_assert(offsetof(struct pcb, pcb_fpusaved) == 136,
    "pcb_fpusaved changed offset");
_Static_assert(offsetof(struct pcb, pcb_fpustate) == 192,
    "pcb_fpustate changed offset");

enum arm64_bus arm64_bus_method = ARM64_BUS_NONE;

/*
 * XXX: The .bss is assumed to be in the boot CPU NUMA domain. If not we
 * could relocate this, but will need to keep the same virtual address as
 * it's reverenced by the EARLY_COUNTER macro.
 */
struct pcpu pcpu0;

#if defined(PERTHREAD_SSP)
/*
 * The boot SSP canary. Will be replaced with a per-thread canary when
 * scheduling has started.
 */
uintptr_t boot_canary = 0x49a2d892bc05a0b1ul;
#endif

static struct trapframe proc0_tf;

int early_boot = 1;
int cold = 1;
static int boot_el;

struct kva_md_info kmi;

int64_t dczva_line_size;	/* The size of cache line the dc zva zeroes */
int has_pan;

#if defined(SOCDEV_PA)
/*
 * This is the virtual address used to access SOCDEV_PA. As it's set before
 * .bss is cleared we need to ensure it's preserved. To do this use
 * __read_mostly as it's only ever set once but read in the putc functions.
 */
uintptr_t socdev_va __read_mostly;
#endif

/*
 * Physical address of the EFI System Table. Stashed from the metadata hints
 * passed into the kernel and used by the EFI code to call runtime services.
 */
vm_paddr_t efi_systbl_phys;
static struct efi_map_header *efihdr;

/* pagezero_* implementations are provided in support.S */
void pagezero_simple(void *);
void pagezero_cache(void *);

/* pagezero_simple is default pagezero */
void (*pagezero)(void *p) = pagezero_simple;

int (*apei_nmi)(void);

#if defined(PERTHREAD_SSP_WARNING)
static void
print_ssp_warning(void *data __unused)
{
	printf("WARNING: Per-thread SSP is enabled but the compiler is too old to support it\n");
}
SYSINIT(ssp_warn, SI_SUB_COPYRIGHT, SI_ORDER_ANY, print_ssp_warning, NULL);
SYSINIT(ssp_warn2, SI_SUB_LAST, SI_ORDER_ANY, print_ssp_warning, NULL);
#endif

static bool
pan_check(const struct cpu_feat *feat __unused, u_int midr __unused)
{
	uint64_t id_aa64mfr1;

	id_aa64mfr1 = READ_SPECIALREG(id_aa64mmfr1_el1);
	return (ID_AA64MMFR1_PAN_VAL(id_aa64mfr1) != ID_AA64MMFR1_PAN_NONE);
}

static void
pan_enable(const struct cpu_feat *feat __unused,
    cpu_feat_errata errata_status __unused, u_int *errata_list __unused,
    u_int errata_count __unused)
{
	has_pan = 1;

	/*
	 * This sets the PAN bit, stopping the kernel from accessing
	 * memory when userspace can also access it unless the kernel
	 * uses the userspace load/store instructions.
	 */
	WRITE_SPECIALREG(sctlr_el1,
	    READ_SPECIALREG(sctlr_el1) & ~SCTLR_SPAN);
	__asm __volatile(
	    ".arch_extension pan	\n"
	    "msr pan, #1		\n"
	    ".arch_extension nopan	\n");
}

static struct cpu_feat feat_pan = {
	.feat_name		= "FEAT_PAN",
	.feat_check		= pan_check,
	.feat_enable		= pan_enable,
	.feat_flags		= CPU_FEAT_EARLY_BOOT | CPU_FEAT_PER_CPU,
};
DATA_SET(cpu_feat_set, feat_pan);

bool
has_hyp(void)
{
	return (boot_el == CURRENTEL_EL_EL2);
}

bool
in_vhe(void)
{
	/* If we are currently in EL2 then must be in VHE */
	return ((READ_SPECIALREG(CurrentEL) & CURRENTEL_EL_MASK) ==
	    CURRENTEL_EL_EL2);
}

static void
cpu_startup(void *dummy)
{
	vm_paddr_t size;
	int i;

	printf("real memory  = %ju (%ju MB)\n", ptoa((uintmax_t)realmem),
	    ptoa((uintmax_t)realmem) / 1024 / 1024);

	if (bootverbose) {
		printf("Physical memory chunk(s):\n");
		for (i = 0; phys_avail[i + 1] != 0; i += 2) {
			size = phys_avail[i + 1] - phys_avail[i];
			printf("%#016jx - %#016jx, %ju bytes (%ju pages)\n",
			    (uintmax_t)phys_avail[i],
			    (uintmax_t)phys_avail[i + 1] - 1,
			    (uintmax_t)size, (uintmax_t)size / PAGE_SIZE);
		}
	}

	printf("avail memory = %ju (%ju MB)\n",
	    ptoa((uintmax_t)vm_free_count()),
	    ptoa((uintmax_t)vm_free_count()) / 1024 / 1024);

	undef_init();
	install_cpu_errata();

	vm_ksubmap_init(&kmi);
	bufinit();
	vm_pager_bufferinit();
}

SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL);

static void
late_ifunc_resolve(void *dummy __unused)
{
	link_elf_late_ireloc();
}
/* Late enough for cpu_feat to have completed */
SYSINIT(late_ifunc_resolve, SI_SUB_CONFIGURE, SI_ORDER_ANY,
    late_ifunc_resolve, NULL);

int
cpu_idle_wakeup(int cpu)
{

	return (0);
}

void
cpu_idle(int busy)
{

	spinlock_enter();
	if (!busy)
		cpu_idleclock();
	if (!sched_runnable())
		__asm __volatile(
		    "dsb sy \n"
		    "wfi    \n");
	if (!busy)
		cpu_activeclock();
	spinlock_exit();
}

void
cpu_halt(void)
{

	/* We should have shutdown by now, if not enter a low power sleep */
	intr_disable();
	while (1) {
		__asm __volatile("wfi");
	}
}

/*
 * Flush the D-cache for non-DMA I/O so that the I-cache can
 * be made coherent later.
 */
void
cpu_flush_dcache(void *ptr, size_t len)
{

	/* ARM64TODO TBD */
}

/* Get current clock frequency for the given CPU ID. */
int
cpu_est_clockrate(int cpu_id, uint64_t *rate)
{
	struct pcpu *pc;

	pc = pcpu_find(cpu_id);
	if (pc == NULL || rate == NULL)
		return (EINVAL);

	if (pc->pc_clock == 0)
		return (EOPNOTSUPP);

	*rate = pc->pc_clock;
	return (0);
}

void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t size)
{

	pcpu->pc_acpi_id = 0xffffffff;
	pcpu->pc_mpidr = UINT64_MAX;
}

void
spinlock_enter(void)
{
	struct thread *td;
	register_t daif;

	td = curthread;
	if (td->td_md.md_spinlock_count == 0) {
		daif = intr_disable();
		td->td_md.md_spinlock_count = 1;
		td->td_md.md_saved_daif = daif;
		critical_enter();
	} else
		td->td_md.md_spinlock_count++;
}

void
spinlock_exit(void)
{
	struct thread *td;
	register_t daif;

	td = curthread;
	daif = td->td_md.md_saved_daif;
	td->td_md.md_spinlock_count--;
	if (td->td_md.md_spinlock_count == 0) {
		critical_exit();
		intr_restore(daif);
	}
}

/*
 * Construct a PCB from a trapframe. This is called from kdb_trap() where
 * we want to start a backtrace from the function that caused us to enter
 * the debugger. We have the context in the trapframe, but base the trace
 * on the PCB. The PCB doesn't have to be perfect, as long as it contains
 * enough for a backtrace.
 */
void
makectx(struct trapframe *tf, struct pcb *pcb)
{
	int i;

	/* NB: pcb_x[PCB_LR] is the PC, see PC_REGS() in db_machdep.h */
	for (i = 0; i < nitems(pcb->pcb_x); i++) {
		if (i == PCB_LR)
			pcb->pcb_x[i] = tf->tf_elr;
		else
			pcb->pcb_x[i] = tf->tf_x[i + PCB_X_START];
	}

	pcb->pcb_sp = tf->tf_sp;
}

static void
init_proc0(vm_offset_t kstack)
{
	struct pcpu *pcpup;

	pcpup = cpuid_to_pcpu[0];
	MPASS(pcpup != NULL);

	proc_linkup0(&proc0, &thread0);
	thread0.td_kstack = kstack;
	thread0.td_kstack_pages = KSTACK_PAGES;
#if defined(PERTHREAD_SSP)
	thread0.td_md.md_canary = boot_canary;
#endif
	thread0.td_pcb = (struct pcb *)(thread0.td_kstack +
	    thread0.td_kstack_pages * PAGE_SIZE) - 1;
	thread0.td_pcb->pcb_flags = 0;
	thread0.td_pcb->pcb_fpflags = 0;
	thread0.td_pcb->pcb_fpusaved = &thread0.td_pcb->pcb_fpustate;
	thread0.td_pcb->pcb_vfpcpu = UINT_MAX;
	thread0.td_frame = &proc0_tf;
	ptrauth_thread0(&thread0);
	pcpup->pc_curpcb = thread0.td_pcb;

	/*
	 * Unmask SError exceptions. They are used to signal a RAS failure,
	 * or other hardware error.
	 */
	serror_enable();
}

/*
 * Get an address to be used to write to kernel data that may be mapped
 * read-only, e.g. to patch kernel code.
 */
bool
arm64_get_writable_addr(void *addr, void **out)
{
	vm_paddr_t pa;

	/* Check if the page is writable */
	if (PAR_SUCCESS(arm64_address_translate_s1e1w((vm_offset_t)addr))) {
		*out = addr;
		return (true);
	}

	/*
	 * Find the physical address of the given page.
	 */
	if (!pmap_klookup((vm_offset_t)addr, &pa)) {
		return (false);
	}

	/*
	 * If it is within the DMAP region and is writable use that.
	 */
	if (PHYS_IN_DMAP_RANGE(pa)) {
		addr = (void *)PHYS_TO_DMAP(pa);
		if (PAR_SUCCESS(arm64_address_translate_s1e1w(
		    (vm_offset_t)addr))) {
			*out = addr;
			return (true);
		}
	}

	return (false);
}

/*
 * Map the passed in VA in EFI space to a void * using the efi memory table to
 * find the PA and return it in the DMAP, if it exists. We're used between the
 * calls to pmap_bootstrap() and physmem_init_kernel_globals() to parse CFG
 * tables We assume that either the entry you are mapping fits within its page,
 * or if it spills to the next page, that's contiguous in PA and in the DMAP.
 * All observed tables obey the first part of this precondition.
 */
struct early_map_data
{
	vm_offset_t va;
	vm_offset_t pa;
};

static void
efi_early_map_entry(struct efi_md *p, void *argp)
{
	struct early_map_data *emdp = argp;
	vm_offset_t s, e;

	if (emdp->pa != 0)
		return;
	if ((p->md_attr & EFI_MD_ATTR_RT) == 0)
		return;
	s = p->md_virt;
	e = p->md_virt + p->md_pages * EFI_PAGE_SIZE;
	if (emdp->va < s  || emdp->va >= e)
		return;
	emdp->pa = p->md_phys + (emdp->va - p->md_virt);
}

static void *
efi_early_map(vm_offset_t va)
{
	struct early_map_data emd = { .va = va };

	efi_map_foreach_entry(efihdr, efi_early_map_entry, &emd);
	if (emd.pa == 0)
		return NULL;
	return (void *)PHYS_TO_DMAP(emd.pa);
}


/*
 * When booted via kexec from Linux, the prior kernel will pass in reserved
 * memory areas in an EFI config table. We need to find that table and walk
 * through it excluding the memory ranges in it. btw, this is called too early
 * for the printf to do anything (unless EARLY_PRINTF is defined) since msgbufp
 * isn't initialized, let alone a console, but breakpoints in printf help
 * diagnose rare failures.
 */
static void
exclude_efi_memreserve(vm_paddr_t efi_systbl_phys)
{
	struct efi_systbl *systbl;
	efi_guid_t efi_memreserve = LINUX_EFI_MEMRESERVE_TABLE;

	systbl = (struct efi_systbl *)PHYS_TO_DMAP(efi_systbl_phys);
	if (systbl == NULL) {
		printf("can't map systbl\n");
		return;
	}
	if (systbl->st_hdr.th_sig != EFI_SYSTBL_SIG) {
		printf("Bad signature for systbl %#lx\n", systbl->st_hdr.th_sig);
		return;
	}

	/*
	 * We don't yet have the pmap system booted enough to create a pmap for
	 * the efi firmware's preferred address space from the GetMemoryMap()
	 * table. The st_cfgtbl is a VA in this space, so we need to do the
	 * mapping ourselves to a kernel VA with efi_early_map. We assume that
	 * the cfgtbl entries don't span a page. Other pointers are PAs, as
	 * noted below.
	 */
	if (systbl->st_cfgtbl == 0)	/* Failsafe st_entries should == 0 in this case */
		return;
	for (int i = 0; i < systbl->st_entries; i++) {
		struct efi_cfgtbl *cfgtbl;
		struct linux_efi_memreserve *mr;

		cfgtbl = efi_early_map(systbl->st_cfgtbl + i * sizeof(*cfgtbl));
		if (cfgtbl == NULL)
			panic("Can't map the config table entry %d\n", i);
		if (memcmp(&cfgtbl->ct_guid, &efi_memreserve, sizeof(efi_guid_t)) != 0)
			continue;

		/*
		 * cfgtbl points are either VA or PA, depending on the GUID of
		 * the table. memreserve GUID pointers are PA and not converted
		 * after a SetVirtualAddressMap(). The list's mr_next pointer
		 * is also a PA.
		 */
		mr = (struct linux_efi_memreserve *)PHYS_TO_DMAP(
			(vm_offset_t)cfgtbl->ct_data);
		while (true) {
			for (int j = 0; j < mr->mr_count; j++) {
				struct linux_efi_memreserve_entry *mre;

				mre = &mr->mr_entry[j];
				physmem_exclude_region(mre->mre_base, mre->mre_size,
				    EXFLAG_NODUMP | EXFLAG_NOALLOC);
			}
			if (mr->mr_next == 0)
				break;
			mr = (struct linux_efi_memreserve *)PHYS_TO_DMAP(mr->mr_next);
		};
	}

}

#ifdef FDT
static void
try_load_dtb(void)
{
	vm_offset_t dtbp;

	dtbp = MD_FETCH(preload_kmdp, MODINFOMD_DTBP, vm_offset_t);
#if defined(FDT_DTB_STATIC)
	/*
	 * In case the device tree blob was not retrieved (from metadata) try
	 * to use the statically embedded one.
	 */
	if (dtbp == 0)
		dtbp = (vm_offset_t)&fdt_static_dtb;
#endif

	if (dtbp == (vm_offset_t)NULL) {
#ifndef TSLOG
		printf("ERROR loading DTB\n");
#endif
		return;
	}

	if (!OF_install(OFW_FDT, 0))
		panic("Cannot install FDT");

	if (OF_init((void *)dtbp) != 0)
		panic("OF_init failed with the found device tree");

	parse_fdt_bootargs();
}
#endif

static bool
bus_probe(void)
{
	bool has_acpi, has_fdt;
	char *order, *env;

	has_acpi = has_fdt = false;

#ifdef FDT
	has_fdt = (OF_peer(0) != 0);
#endif
#ifdef DEV_ACPI
	has_acpi = (AcpiOsGetRootPointer() != 0);
#endif

	env = kern_getenv("kern.cfg.order");
	if (env != NULL) {
		order = env;
		while (order != NULL) {
			if (has_acpi &&
			    strncmp(order, "acpi", 4) == 0 &&
			    (order[4] == ',' || order[4] == '\0')) {
				arm64_bus_method = ARM64_BUS_ACPI;
				break;
			}
			if (has_fdt &&
			    strncmp(order, "fdt", 3) == 0 &&
			    (order[3] == ',' || order[3] == '\0')) {
				arm64_bus_method = ARM64_BUS_FDT;
				break;
			}
			order = strchr(order, ',');
			if (order != NULL)
				order++;	/* Skip comma */
		}
		freeenv(env);

		/* If we set the bus method it is valid */
		if (arm64_bus_method != ARM64_BUS_NONE)
			return (true);
	}
	/* If no order or an invalid order was set use the default */
	if (arm64_bus_method == ARM64_BUS_NONE) {
		if (has_acpi)
			arm64_bus_method = ARM64_BUS_ACPI;
		else if (has_fdt)
			arm64_bus_method = ARM64_BUS_FDT;
	}

	/*
	 * If no option was set the default is valid, otherwise we are
	 * setting one to get cninit() working, then calling panic to tell
	 * the user about the invalid bus setup.
	 */
	return (env == NULL);
}

static void
cache_setup(void)
{
	int dczva_line_shift;
	uint32_t dczid_el0;

	identify_cache(READ_SPECIALREG(ctr_el0));

	dczid_el0 = READ_SPECIALREG(dczid_el0);

	/* Check if dc zva is not prohibited */
	if (dczid_el0 & DCZID_DZP)
		dczva_line_size = 0;
	else {
		/* Same as with above calculations */
		dczva_line_shift = DCZID_BS_SIZE(dczid_el0);
		dczva_line_size = sizeof(int) << dczva_line_shift;

		/* Change pagezero function */
		pagezero = pagezero_cache;
	}
}

int
memory_mapping_mode(vm_paddr_t pa)
{
	struct efi_md *map, *p;
	size_t efisz;
	int ndesc, i;

	if (efihdr == NULL)
		return (VM_MEMATTR_WRITE_BACK);

	/*
	 * Memory map data provided by UEFI via the GetMemoryMap
	 * Boot Services API.
	 */
	efisz = (sizeof(struct efi_map_header) + 0xf) & ~0xf;
	map = (struct efi_md *)((uint8_t *)efihdr + efisz);

	if (efihdr->descriptor_size == 0)
		return (VM_MEMATTR_WRITE_BACK);
	ndesc = efihdr->memory_size / efihdr->descriptor_size;

	for (i = 0, p = map; i < ndesc; i++,
	    p = efi_next_descriptor(p, efihdr->descriptor_size)) {
		if (pa < p->md_phys ||
		    pa >= p->md_phys + p->md_pages * EFI_PAGE_SIZE)
			continue;
		if (p->md_type == EFI_MD_TYPE_IOMEM ||
		    p->md_type == EFI_MD_TYPE_IOPORT)
			return (VM_MEMATTR_DEVICE);
		else if ((p->md_attr & EFI_MD_ATTR_WB) != 0 ||
		    p->md_type == EFI_MD_TYPE_RECLAIM)
			return (VM_MEMATTR_WRITE_BACK);
		else if ((p->md_attr & EFI_MD_ATTR_WT) != 0)
			return (VM_MEMATTR_WRITE_THROUGH);
		else if ((p->md_attr & EFI_MD_ATTR_WC) != 0)
			return (VM_MEMATTR_WRITE_COMBINING);
		break;
	}

	return (VM_MEMATTR_DEVICE);
}

#ifdef FDT
static void
fdt_physmem_hardware_region_cb(const struct mem_region *mr, void *arg __unused)
{
	physmem_hardware_region(mr->mr_start, mr->mr_size);
}

static void
fdt_physmem_exclude_region_cb(const struct mem_region *mr, void *arg __unused)
{
	physmem_exclude_region(mr->mr_start, mr->mr_size,
	    EXFLAG_NODUMP | EXFLAG_NOALLOC);
}
#endif

void
initarm(struct arm64_bootparams *abp)
{
	struct efi_fb *efifb;
	struct pcpu *pcpup;
	char *env;
#ifdef FDT
	phandle_t root;
	char dts_version[255];
#endif
	vm_offset_t lastaddr;
	bool valid;

	TSRAW(&thread0, TS_ENTER, __func__, NULL);

	boot_el = abp->boot_el;

	/* Parse loader or FDT boot parameters. Determine last used address. */
	lastaddr = parse_boot_param(abp);

	identify_cpu(0);
	identify_hypervisor_smbios();

	update_special_regs(0);

	/* Set the pcpu data, this is needed by pmap_bootstrap */
	pcpup = &pcpu0;
	pcpu_init(pcpup, 0, sizeof(struct pcpu));

	/*
	 * Set the pcpu pointer with a backup in tpidr_el1 to be
	 * loaded when entering the kernel from userland.
	 */
	__asm __volatile(
	    "mov x18, %0 \n"
	    "msr tpidr_el1, %0" :: "r"(pcpup));

	/* locore.S sets sp_el0 to &thread0 so no need to set it here. */
	PCPU_SET(curthread, &thread0);
	PCPU_SET(midr, get_midr());

	link_elf_ireloc();
#ifdef FDT
	try_load_dtb();
#endif

	efi_systbl_phys = MD_FETCH(preload_kmdp, MODINFOMD_FW_HANDLE,
	    vm_paddr_t);

	/* Load the physical memory ranges */
	efihdr = (struct efi_map_header *)preload_search_info(preload_kmdp,
	    MODINFO_METADATA | MODINFOMD_EFI_MAP);
	if (efihdr != NULL)
		efi_map_add_entries(efihdr);
#ifdef FDT
	else {
		/* Grab physical memory regions information from device tree. */
		if (fdt_foreach_mem_region(fdt_physmem_hardware_region_cb,
		    NULL) != 0)
			panic("Cannot get physical memory regions");
	}
	fdt_foreach_reserved_mem(fdt_physmem_exclude_region_cb, NULL);
#endif

	/* Exclude the EFI framebuffer from our view of physical memory. */
	efifb = (struct efi_fb *)preload_search_info(preload_kmdp,
	    MODINFO_METADATA | MODINFOMD_EFI_FB);
	if (efifb != NULL)
		physmem_exclude_region(efifb->fb_addr, efifb->fb_size,
		    EXFLAG_NOALLOC);

	/* Do basic tuning, hz etc */
	init_param1();

	cache_setup();

	/*
	 * Perform a staged bootstrap of virtual memory.
	 *
	 * - First we create the DMAP region. This allows it to be used in
	 *   later bootstrapping.
	 * - Next exclude memory that is needed in the DMAP region, but must
	 *   not be used by FreeBSD.
	 * - Lastly complete the bootstrapping. It may use the physical
	 *   memory map so any excluded memory must be marked as such before
	 *   pmap_bootstrap() is called.
	 */
	pmap_bootstrap_dmap(lastaddr - KERNBASE);
	/*
	 * Exclude EFI entries needed in the DMAP, e.g. EFI_MD_TYPE_RECLAIM
	 * may contain the ACPI tables but shouldn't be used by the kernel
	 */
	if (efihdr != NULL)
		efi_map_exclude_entries(efihdr);
	/*  Do the same for reserve entries in the EFI MEMRESERVE table */
	if (efi_systbl_phys != 0)
		exclude_efi_memreserve(efi_systbl_phys);
	/* Continue bootstrapping pmap */
	pmap_bootstrap();

	/*
	 * We carefully bootstrap the sanitizer map after we've excluded
	 * absolutely everything else that could impact phys_avail.  There's not
	 * always enough room for the initial shadow map after the kernel, so
	 * we'll end up searching for segments that we can safely use.  Those
	 * segments also get excluded from phys_avail.
	 */
#if defined(KASAN) || defined(KMSAN)
	pmap_bootstrap_san();
#endif

	physmem_init_kernel_globals();

	valid = bus_probe();

	cninit();
	set_ttbr0(abp->kern_ttbr0);
	cpu_tlb_flushID();

	if (!valid)
		panic("Invalid bus configuration: %s",
		    kern_getenv("kern.cfg.order"));

	/* Detect early CPU feature support */
	enable_cpu_feat(CPU_FEAT_EARLY_BOOT);

	/*
	 * Dump the boot metadata. We have to wait for cninit() since console
	 * output is required. If it's grossly incorrect the kernel will never
	 * make it this far.
	 */
	if (getenv_is_true("debug.dump_modinfo_at_boot"))
		preload_dump();

	init_proc0(abp->kern_stack);
	msgbufinit(msgbufp, msgbufsize);
	mutex_init();
	init_param2(physmem);

	dbg_init();
	kdb_init();
#ifdef KDB
	if ((boothowto & RB_KDB) != 0)
		kdb_enter(KDB_WHY_BOOTFLAGS, "Boot flags requested debugger");
#endif

	kcsan_cpu_init(0);
	kasan_init();
	kmsan_init();

	env = kern_getenv("kernelname");
	if (env != NULL)
		strlcpy(kernelname, env, sizeof(kernelname));

#ifdef FDT
	if (arm64_bus_method == ARM64_BUS_FDT) {
		root = OF_finddevice("/");
		if (OF_getprop(root, "freebsd,dts-version", dts_version, sizeof(dts_version)) > 0) {
			if (strcmp(LINUX_DTS_VERSION, dts_version) != 0)
				printf("WARNING: DTB version is %s while kernel expects %s, "
				    "please update the DTB in the ESP\n",
				    dts_version,
				    LINUX_DTS_VERSION);
		} else {
			printf("WARNING: Cannot find freebsd,dts-version property, "
			    "cannot check DTB compliance\n");
		}
	}
#endif

	if (boothowto & RB_VERBOSE) {
		if (efihdr != NULL)
			efi_map_print_entries(efihdr);
		physmem_print_tables();
	}

	early_boot = 0;

	if (bootverbose && kstack_pages != KSTACK_PAGES)
		printf("kern.kstack_pages = %d ignored for thread0\n",
		    kstack_pages);

	TSEXIT();
}

void
dbg_init(void)
{

	/* Clear OS lock */
	WRITE_SPECIALREG(oslar_el1, 0);

	/* This permits DDB to use debug registers for watchpoints. */
	dbg_monitor_init();

	/* TODO: Eventually will need to initialize debug registers here. */
}

#ifdef DDB
#include <ddb/ddb.h>

DB_SHOW_COMMAND(specialregs, db_show_spregs)
{
#define	PRINT_REG(reg)	\
    db_printf(__STRING(reg) " = %#016lx\n", READ_SPECIALREG(reg))

	PRINT_REG(actlr_el1);
	PRINT_REG(afsr0_el1);
	PRINT_REG(afsr1_el1);
	PRINT_REG(aidr_el1);
	PRINT_REG(amair_el1);
	PRINT_REG(ccsidr_el1);
	PRINT_REG(clidr_el1);
	PRINT_REG(contextidr_el1);
	PRINT_REG(cpacr_el1);
	PRINT_REG(csselr_el1);
	PRINT_REG(ctr_el0);
	PRINT_REG(currentel);
	PRINT_REG(daif);
	PRINT_REG(dczid_el0);
	PRINT_REG(elr_el1);
	PRINT_REG(esr_el1);
	PRINT_REG(far_el1);
#if 0
	/* ARM64TODO: Enable VFP before reading floating-point registers */
	PRINT_REG(fpcr);
	PRINT_REG(fpsr);
#endif
	PRINT_REG(id_aa64afr0_el1);
	PRINT_REG(id_aa64afr1_el1);
	PRINT_REG(id_aa64dfr0_el1);
	PRINT_REG(id_aa64dfr1_el1);
	PRINT_REG(id_aa64isar0_el1);
	PRINT_REG(id_aa64isar1_el1);
	PRINT_REG(id_aa64pfr0_el1);
	PRINT_REG(id_aa64pfr1_el1);
	PRINT_REG(id_afr0_el1);
	PRINT_REG(id_dfr0_el1);
	PRINT_REG(id_isar0_el1);
	PRINT_REG(id_isar1_el1);
	PRINT_REG(id_isar2_el1);
	PRINT_REG(id_isar3_el1);
	PRINT_REG(id_isar4_el1);
	PRINT_REG(id_isar5_el1);
	PRINT_REG(id_mmfr0_el1);
	PRINT_REG(id_mmfr1_el1);
	PRINT_REG(id_mmfr2_el1);
	PRINT_REG(id_mmfr3_el1);
#if 0
	/* Missing from llvm */
	PRINT_REG(id_mmfr4_el1);
#endif
	PRINT_REG(id_pfr0_el1);
	PRINT_REG(id_pfr1_el1);
	PRINT_REG(isr_el1);
	PRINT_REG(mair_el1);
	PRINT_REG(midr_el1);
	PRINT_REG(mpidr_el1);
	PRINT_REG(mvfr0_el1);
	PRINT_REG(mvfr1_el1);
	PRINT_REG(mvfr2_el1);
	PRINT_REG(revidr_el1);
	PRINT_REG(sctlr_el1);
	PRINT_REG(sp_el0);
	PRINT_REG(spsel);
	PRINT_REG(spsr_el1);
	PRINT_REG(tcr_el1);
	PRINT_REG(tpidr_el0);
	PRINT_REG(tpidr_el1);
	PRINT_REG(tpidrro_el0);
	PRINT_REG(ttbr0_el1);
	PRINT_REG(ttbr1_el1);
	PRINT_REG(vbar_el1);
#undef PRINT_REG
}

DB_SHOW_COMMAND(vtop, db_show_vtop)
{
	uint64_t phys;

	if (have_addr) {
		phys = arm64_address_translate_s1e1r(addr);
		db_printf("EL1 physical address reg (read):  0x%016lx\n", phys);
		phys = arm64_address_translate_s1e1w(addr);
		db_printf("EL1 physical address reg (write): 0x%016lx\n", phys);
		phys = arm64_address_translate_s1e0r(addr);
		db_printf("EL0 physical address reg (read):  0x%016lx\n", phys);
		phys = arm64_address_translate_s1e0w(addr);
		db_printf("EL0 physical address reg (write): 0x%016lx\n", phys);
	} else
		db_printf("show vtop <virt_addr>\n");
}
#endif
