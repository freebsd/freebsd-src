/*
 * Copyright (c) 2008, 2013 Citrix Systems, Inc.
 * Copyright (c) 2012 Spectra Logic Corporation
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS AS IS'' AND
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/pci/pcivar.h>

#include <machine/cpufunc.h>
#include <machine/cpu.h>
#include <machine/smp.h>

#include <x86/apicreg.h>

#include <xen/xen-os.h>
#include <xen/features.h>
#include <xen/gnttab.h>
#include <xen/hypervisor.h>
#include <xen/hvm.h>
#include <xen/xen_intr.h>

#include <xen/interface/hvm/params.h>
#include <xen/interface/vcpu.h>

/*--------------------------- Forward Declarations ---------------------------*/
#ifdef SMP
static driver_filter_t xen_smp_rendezvous_action;
static driver_filter_t xen_invltlb;
static driver_filter_t xen_invlpg;
static driver_filter_t xen_invlrng;
static driver_filter_t xen_invlcache;
#ifdef __i386__
static driver_filter_t xen_lazypmap;
#endif
static driver_filter_t xen_ipi_bitmap_handler;
static driver_filter_t xen_cpustop_handler;
static driver_filter_t xen_cpususpend_handler;
static driver_filter_t xen_cpustophard_handler;
#endif
static void xen_ipi_vectored(u_int vector, int dest);
static void xen_hvm_cpu_resume(void);
static void xen_hvm_cpu_init(void);

/*---------------------------- Extern Declarations ---------------------------*/
/* Variables used by mp_machdep to perform the MMU related IPIs */
extern volatile int smp_tlb_wait;
extern vm_offset_t smp_tlb_addr2;
#ifdef __i386__
extern vm_offset_t smp_tlb_addr1;
#else
extern struct invpcid_descr smp_tlb_invpcid;
extern uint64_t pcid_cr3;
extern int invpcid_works;
extern int pmap_pcid_enabled;
extern pmap_t smp_tlb_pmap;
#endif

#ifdef __i386__
extern void pmap_lazyfix_action(void);
#endif

/* Variables used by mp_machdep to perform the bitmap IPI */
extern volatile u_int cpu_ipi_pending[MAXCPU];

/*---------------------------------- Macros ----------------------------------*/
#define	IPI_TO_IDX(ipi) ((ipi) - APIC_IPI_INTS)

/*-------------------------------- Local Types -------------------------------*/
enum xen_hvm_init_type {
	XEN_HVM_INIT_COLD,
	XEN_HVM_INIT_CANCELLED_SUSPEND,
	XEN_HVM_INIT_RESUME
};

struct xen_ipi_handler
{
	driver_filter_t	*filter;
	const char	*description;
};

/*-------------------------------- Global Data -------------------------------*/
enum xen_domain_type xen_domain_type = XEN_NATIVE;

struct cpu_ops xen_hvm_cpu_ops = {
	.ipi_vectored	= lapic_ipi_vectored,
	.cpu_init	= xen_hvm_cpu_init,
	.cpu_resume	= xen_hvm_cpu_resume
};

static MALLOC_DEFINE(M_XENHVM, "xen_hvm", "Xen HVM PV Support");

#ifdef SMP
static struct xen_ipi_handler xen_ipis[] = 
{
	[IPI_TO_IDX(IPI_RENDEZVOUS)]	= { xen_smp_rendezvous_action,	"r"   },
	[IPI_TO_IDX(IPI_INVLTLB)]	= { xen_invltlb,		"itlb"},
	[IPI_TO_IDX(IPI_INVLPG)]	= { xen_invlpg,			"ipg" },
	[IPI_TO_IDX(IPI_INVLRNG)]	= { xen_invlrng,		"irg" },
	[IPI_TO_IDX(IPI_INVLCACHE)]	= { xen_invlcache,		"ic"  },
#ifdef __i386__
	[IPI_TO_IDX(IPI_LAZYPMAP)]	= { xen_lazypmap,		"lp"  },
#endif
	[IPI_TO_IDX(IPI_BITMAP_VECTOR)] = { xen_ipi_bitmap_handler,	"b"   },
	[IPI_TO_IDX(IPI_STOP)]		= { xen_cpustop_handler,	"st"  },
	[IPI_TO_IDX(IPI_SUSPEND)]	= { xen_cpususpend_handler,	"sp"  },
	[IPI_TO_IDX(IPI_STOP_HARD)]	= { xen_cpustophard_handler,	"sth" },
};
#endif

/**
 * If non-zero, the hypervisor has been configured to use a direct
 * IDT event callback for interrupt injection.
 */
int xen_vector_callback_enabled;

/*------------------------------- Per-CPU Data -------------------------------*/
DPCPU_DEFINE(struct vcpu_info, vcpu_local_info);
DPCPU_DEFINE(struct vcpu_info *, vcpu_info);
#ifdef SMP
DPCPU_DEFINE(xen_intr_handle_t, ipi_handle[nitems(xen_ipis)]);
#endif

/*------------------ Hypervisor Access Shared Memory Regions -----------------*/
/** Hypercall table accessed via HYPERVISOR_*_op() methods. */
char *hypercall_stubs;
shared_info_t *HYPERVISOR_shared_info;

#ifdef SMP
/*---------------------------- XEN PV IPI Handlers ---------------------------*/
/*
 * This are C clones of the ASM functions found in apic_vector.s
 */
static int
xen_ipi_bitmap_handler(void *arg)
{
	struct trapframe *frame;

	frame = arg;
	ipi_bitmap_handler(*frame);
	return (FILTER_HANDLED);
}

static int
xen_smp_rendezvous_action(void *arg)
{
#ifdef COUNT_IPIS
	int cpu;

	cpu = PCPU_GET(cpuid);
	(*ipi_rendezvous_counts[cpu])++;
#endif /* COUNT_IPIS */

	smp_rendezvous_action();
	return (FILTER_HANDLED);
}

static int
xen_invltlb(void *arg)
{
#if defined(COUNT_XINVLTLB_HITS) || defined(COUNT_IPIS)
	int cpu;

	cpu = PCPU_GET(cpuid);
#ifdef COUNT_XINVLTLB_HITS
	xhits_gbl[cpu]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invltlb_counts[cpu])++;
#endif /* COUNT_IPIS */
#endif /* COUNT_XINVLTLB_HITS || COUNT_IPIS */

	invltlb();
	atomic_add_int(&smp_tlb_wait, 1);
	return (FILTER_HANDLED);
}

#ifdef __amd64__
static int
xen_invltlb_pcid(void *arg)
{
	uint64_t cr3;
#if defined(COUNT_XINVLTLB_HITS) || defined(COUNT_IPIS)
	int cpu;

	cpu = PCPU_GET(cpuid);
#ifdef COUNT_XINVLTLB_HITS
	xhits_gbl[cpu]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invltlb_counts[cpu])++;
#endif /* COUNT_IPIS */
#endif /* COUNT_XINVLTLB_HITS || COUNT_IPIS */

	cr3 = rcr3();
	if (smp_tlb_invpcid.pcid != (uint64_t)-1 &&
	    smp_tlb_invpcid.pcid != 0) {

		if (invpcid_works) {
			invpcid(&smp_tlb_invpcid, INVPCID_CTX);
		} else {
			/* Otherwise reload %cr3 twice. */
			if (cr3 != pcid_cr3) {
				load_cr3(pcid_cr3);
				cr3 |= CR3_PCID_SAVE;
			}
			load_cr3(cr3);
		}
	} else {
		invltlb_globpcid();
	}
	if (smp_tlb_pmap != NULL)
		CPU_CLR_ATOMIC(PCPU_GET(cpuid), &smp_tlb_pmap->pm_save);

	atomic_add_int(&smp_tlb_wait, 1);
	return (FILTER_HANDLED);
}
#endif

static int
xen_invlpg(void *arg)
{
#if defined(COUNT_XINVLTLB_HITS) || defined(COUNT_IPIS)
	int cpu;

	cpu = PCPU_GET(cpuid);
#ifdef COUNT_XINVLTLB_HITS
	xhits_pg[cpu]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invlpg_counts[cpu])++;
#endif /* COUNT_IPIS */
#endif /* COUNT_XINVLTLB_HITS || COUNT_IPIS */

#ifdef __i386__
	invlpg(smp_tlb_addr1);
#else
	invlpg(smp_tlb_invpcid.addr);
#endif
	atomic_add_int(&smp_tlb_wait, 1);
	return (FILTER_HANDLED);
}

#ifdef __amd64__
static int
xen_invlpg_pcid(void *arg)
{
#if defined(COUNT_XINVLTLB_HITS) || defined(COUNT_IPIS)
	int cpu;

	cpu = PCPU_GET(cpuid);
#ifdef COUNT_XINVLTLB_HITS
	xhits_pg[cpu]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invlpg_counts[cpu])++;
#endif /* COUNT_IPIS */
#endif /* COUNT_XINVLTLB_HITS || COUNT_IPIS */

	if (invpcid_works) {
		invpcid(&smp_tlb_invpcid, INVPCID_ADDR);
	} else if (smp_tlb_invpcid.pcid == 0) {
		invlpg(smp_tlb_invpcid.addr);
	} else if (smp_tlb_invpcid.pcid == (uint64_t)-1) {
		invltlb_globpcid();
	} else {
		uint64_t cr3;

		/*
		 * PCID supported, but INVPCID is not.
		 * Temporarily switch to the target address
		 * space and do INVLPG.
		 */
		cr3 = rcr3();
		if (cr3 != pcid_cr3)
			load_cr3(pcid_cr3 | CR3_PCID_SAVE);
		invlpg(smp_tlb_invpcid.addr);
		load_cr3(cr3 | CR3_PCID_SAVE);
	}

	atomic_add_int(&smp_tlb_wait, 1);
	return (FILTER_HANDLED);
}
#endif

static inline void
invlpg_range(vm_offset_t start, vm_offset_t end)
{
	do {
		invlpg(start);
		start += PAGE_SIZE;
	} while (start < end);
}

static int
xen_invlrng(void *arg)
{
	vm_offset_t addr;
#if defined(COUNT_XINVLTLB_HITS) || defined(COUNT_IPIS)
	int cpu;

	cpu = PCPU_GET(cpuid);
#ifdef COUNT_XINVLTLB_HITS
	xhits_rng[cpu]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invlrng_counts[cpu])++;
#endif /* COUNT_IPIS */
#endif /* COUNT_XINVLTLB_HITS || COUNT_IPIS */

#ifdef __i386__
	addr = smp_tlb_addr1;
	invlpg_range(addr, smp_tlb_addr2);
#else
	addr = smp_tlb_invpcid.addr;
	if (pmap_pcid_enabled) {
		if (invpcid_works) {
			struct invpcid_descr d;

			d = smp_tlb_invpcid;
			do {
				invpcid(&d, INVPCID_ADDR);
				d.addr += PAGE_SIZE;
			} while (d.addr < smp_tlb_addr2);
		} else if (smp_tlb_invpcid.pcid == 0) {
			/*
			 * kernel pmap - use invlpg to invalidate
			 * global mapping.
			 */
			invlpg_range(addr, smp_tlb_addr2);
		} else if (smp_tlb_invpcid.pcid != (uint64_t)-1) {
			invltlb_globpcid();
			if (smp_tlb_pmap != NULL) {
				CPU_CLR_ATOMIC(PCPU_GET(cpuid),
				    &smp_tlb_pmap->pm_save);
			}
		} else {
			uint64_t cr3;

			cr3 = rcr3();
			if (cr3 != pcid_cr3)
				load_cr3(pcid_cr3 | CR3_PCID_SAVE);
			invlpg_range(addr, smp_tlb_addr2);
			load_cr3(cr3 | CR3_PCID_SAVE);
		}
	} else {
		invlpg_range(addr, smp_tlb_addr2);
	}
#endif

	atomic_add_int(&smp_tlb_wait, 1);
	return (FILTER_HANDLED);
}

static int
xen_invlcache(void *arg)
{
#ifdef COUNT_IPIS
	int cpu = PCPU_GET(cpuid);

	cpu = PCPU_GET(cpuid);
	(*ipi_invlcache_counts[cpu])++;
#endif /* COUNT_IPIS */

	wbinvd();
	atomic_add_int(&smp_tlb_wait, 1);
	return (FILTER_HANDLED);
}

#ifdef __i386__
static int
xen_lazypmap(void *arg)
{

	pmap_lazyfix_action();
	return (FILTER_HANDLED);
}
#endif

static int
xen_cpustop_handler(void *arg)
{

	cpustop_handler();
	return (FILTER_HANDLED);
}

static int
xen_cpususpend_handler(void *arg)
{

	cpususpend_handler();
	return (FILTER_HANDLED);
}

static int
xen_cpustophard_handler(void *arg)
{

	ipi_nmi_handler();
	return (FILTER_HANDLED);
}

/* Xen PV IPI sender */
static void
xen_ipi_vectored(u_int vector, int dest)
{
	xen_intr_handle_t *ipi_handle;
	int ipi_idx, to_cpu, self;

	ipi_idx = IPI_TO_IDX(vector);
	if (ipi_idx > nitems(xen_ipis))
		panic("IPI out of range");

	switch(dest) {
	case APIC_IPI_DEST_SELF:
		ipi_handle = DPCPU_GET(ipi_handle);
		xen_intr_signal(ipi_handle[ipi_idx]);
		break;
	case APIC_IPI_DEST_ALL:
		CPU_FOREACH(to_cpu) {
			ipi_handle = DPCPU_ID_GET(to_cpu, ipi_handle);
			xen_intr_signal(ipi_handle[ipi_idx]);
		}
		break;
	case APIC_IPI_DEST_OTHERS:
		self = PCPU_GET(cpuid);
		CPU_FOREACH(to_cpu) {
			if (to_cpu != self) {
				ipi_handle = DPCPU_ID_GET(to_cpu, ipi_handle);
				xen_intr_signal(ipi_handle[ipi_idx]);
			}
		}
		break;
	default:
		to_cpu = apic_cpuid(dest);
		ipi_handle = DPCPU_ID_GET(to_cpu, ipi_handle);
		xen_intr_signal(ipi_handle[ipi_idx]);
		break;
	}
}

/* XEN diverged cpu operations */
static void
xen_hvm_cpu_resume(void)
{
	u_int cpuid = PCPU_GET(cpuid);

	/*
	 * Reset pending bitmap IPIs, because Xen doesn't preserve pending
	 * event channels on migration.
	 */
	cpu_ipi_pending[cpuid] = 0;

	/* register vcpu_info area */
	xen_hvm_cpu_init();
}

static void
xen_cpu_ipi_init(int cpu)
{
	xen_intr_handle_t *ipi_handle;
	const struct xen_ipi_handler *ipi;
	device_t dev;
	int idx, rc;

	ipi_handle = DPCPU_ID_GET(cpu, ipi_handle);
	dev = pcpu_find(cpu)->pc_device;
	KASSERT((dev != NULL), ("NULL pcpu device_t"));

	for (ipi = xen_ipis, idx = 0; idx < nitems(xen_ipis); ipi++, idx++) {

		if (ipi->filter == NULL) {
			ipi_handle[idx] = NULL;
			continue;
		}

		rc = xen_intr_alloc_and_bind_ipi(dev, cpu, ipi->filter,
		    INTR_TYPE_TTY, &ipi_handle[idx]);
		if (rc != 0)
			panic("Unable to allocate a XEN IPI port");
		xen_intr_describe(ipi_handle[idx], "%s", ipi->description);
	}
}

static void
xen_setup_cpus(void)
{
	int i;

	if (!xen_hvm_domain() || !xen_vector_callback_enabled)
		return;

#ifdef __amd64__
	if (pmap_pcid_enabled) {
		xen_ipis[IPI_TO_IDX(IPI_INVLTLB)].filter = xen_invltlb_pcid;
		xen_ipis[IPI_TO_IDX(IPI_INVLPG)].filter = xen_invlpg_pcid;
	}
#endif
	CPU_FOREACH(i)
		xen_cpu_ipi_init(i);

	/* Set the xen pv ipi ops to replace the native ones */
	cpu_ops.ipi_vectored = xen_ipi_vectored;
}
#endif

/*---------------------- XEN Hypervisor Probe and Setup ----------------------*/
static uint32_t
xen_hvm_cpuid_base(void)
{
	uint32_t base, regs[4];

	for (base = 0x40000000; base < 0x40010000; base += 0x100) {
		do_cpuid(base, regs);
		if (!memcmp("XenVMMXenVMM", &regs[1], 12)
		    && (regs[0] - base) >= 2)
			return (base);
	}
	return (0);
}

/*
 * Allocate and fill in the hypcall page.
 */
static int
xen_hvm_init_hypercall_stubs(void)
{
	uint32_t base, regs[4];
	int i;

	base = xen_hvm_cpuid_base();
	if (base == 0)
		return (ENXIO);

	if (hypercall_stubs == NULL) {
		do_cpuid(base + 1, regs);
		printf("XEN: Hypervisor version %d.%d detected.\n",
		    regs[0] >> 16, regs[0] & 0xffff);
	}

	/*
	 * Find the hypercall pages.
	 */
	do_cpuid(base + 2, regs);
	
	if (hypercall_stubs == NULL) {
		size_t call_region_size;

		call_region_size = regs[0] * PAGE_SIZE;
		hypercall_stubs = malloc(call_region_size, M_XENHVM, M_NOWAIT);
		if (hypercall_stubs == NULL)
			panic("Unable to allocate Xen hypercall region");
	}

	for (i = 0; i < regs[0]; i++)
		wrmsr(regs[1], vtophys(hypercall_stubs + i * PAGE_SIZE) + i);

	return (0);
}

static void
xen_hvm_init_shared_info_page(void)
{
	struct xen_add_to_physmap xatp;

	if (HYPERVISOR_shared_info == NULL) {
		HYPERVISOR_shared_info = malloc(PAGE_SIZE, M_XENHVM, M_NOWAIT);
		if (HYPERVISOR_shared_info == NULL)
			panic("Unable to allocate Xen shared info page");
	}

	xatp.domid = DOMID_SELF;
	xatp.idx = 0;
	xatp.space = XENMAPSPACE_shared_info;
	xatp.gpfn = vtophys(HYPERVISOR_shared_info) >> PAGE_SHIFT;
	if (HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp))
		panic("HYPERVISOR_memory_op failed");
}

/*
 * Tell the hypervisor how to contact us for event channel callbacks.
 */
void
xen_hvm_set_callback(device_t dev)
{
	struct xen_hvm_param xhp;
	int irq;

	if (xen_vector_callback_enabled)
		return;

	xhp.domid = DOMID_SELF;
	xhp.index = HVM_PARAM_CALLBACK_IRQ;
	if (xen_feature(XENFEAT_hvm_callback_vector) != 0) {
		int error;

		xhp.value = HVM_CALLBACK_VECTOR(IDT_EVTCHN);
		error = HYPERVISOR_hvm_op(HVMOP_set_param, &xhp);
		if (error == 0) {
			xen_vector_callback_enabled = 1;
			return;
		}
		printf("Xen HVM callback vector registration failed (%d). "
		    "Falling back to emulated device interrupt\n", error);
	}
	xen_vector_callback_enabled = 0;
	if (dev == NULL) {
		/*
		 * Called from early boot or resume.
		 * xenpci will invoke us again later.
		 */
		return;
	}

	irq = pci_get_irq(dev);
	if (irq < 16) {
		xhp.value = HVM_CALLBACK_GSI(irq);
	} else {
		u_int slot;
		u_int pin;

		slot = pci_get_slot(dev);
		pin = pci_get_intpin(dev) - 1;
		xhp.value = HVM_CALLBACK_PCI_INTX(slot, pin);
	}

	if (HYPERVISOR_hvm_op(HVMOP_set_param, &xhp) != 0)
		panic("Can't set evtchn callback");
}

#define	XEN_MAGIC_IOPORT 0x10
enum {
	XMI_MAGIC			 = 0x49d2,
	XMI_UNPLUG_IDE_DISKS		 = 0x01,
	XMI_UNPLUG_NICS			 = 0x02,
	XMI_UNPLUG_IDE_EXCEPT_PRI_MASTER = 0x04
};

static void
xen_hvm_disable_emulated_devices(void)
{
	if (inw(XEN_MAGIC_IOPORT) != XMI_MAGIC)
		return;

	if (bootverbose)
		printf("XEN: Disabling emulated block and network devices\n");
	outw(XEN_MAGIC_IOPORT, XMI_UNPLUG_IDE_DISKS|XMI_UNPLUG_NICS);
}

static void
xen_hvm_init(enum xen_hvm_init_type init_type)
{
	int error;
	int i;

	if (init_type == XEN_HVM_INIT_CANCELLED_SUSPEND)
		return;

	error = xen_hvm_init_hypercall_stubs();

	switch (init_type) {
	case XEN_HVM_INIT_COLD:
		if (error != 0)
			return;

		setup_xen_features();
		cpu_ops = xen_hvm_cpu_ops;
 		vm_guest = VM_GUEST_XEN;
		break;
	case XEN_HVM_INIT_RESUME:
		if (error != 0)
			panic("Unable to init Xen hypercall stubs on resume");

		/* Clear stale vcpu_info. */
		CPU_FOREACH(i)
			DPCPU_ID_SET(i, vcpu_info, NULL);
		break;
	default:
		panic("Unsupported HVM initialization type");
	}

	xen_vector_callback_enabled = 0;
	xen_domain_type = XEN_HVM_DOMAIN;
	xen_hvm_init_shared_info_page();
	xen_hvm_set_callback(NULL);
	xen_hvm_disable_emulated_devices();
} 

void
xen_hvm_suspend(void)
{
}

void
xen_hvm_resume(bool suspend_cancelled)
{

	xen_hvm_init(suspend_cancelled ?
	    XEN_HVM_INIT_CANCELLED_SUSPEND : XEN_HVM_INIT_RESUME);

	/* Register vcpu_info area for CPU#0. */
	xen_hvm_cpu_init();
}
 
static void
xen_hvm_sysinit(void *arg __unused)
{
	xen_hvm_init(XEN_HVM_INIT_COLD);
}

static void
xen_set_vcpu_id(void)
{
	struct pcpu *pc;
	int i;

	/* Set vcpu_id to acpi_id */
	CPU_FOREACH(i) {
		pc = pcpu_find(i);
		pc->pc_vcpu_id = pc->pc_acpi_id;
		if (bootverbose)
			printf("XEN: CPU %u has VCPU ID %u\n",
			       i, pc->pc_vcpu_id);
	}
}

static void
xen_hvm_cpu_init(void)
{
	struct vcpu_register_vcpu_info info;
	struct vcpu_info *vcpu_info;
	int cpu, rc;

	if (!xen_domain())
		return;

	if (DPCPU_GET(vcpu_info) != NULL) {
		/*
		 * vcpu_info is already set.  We're resuming
		 * from a failed migration and our pre-suspend
		 * configuration is still valid.
		 */
		return;
	}

	vcpu_info = DPCPU_PTR(vcpu_local_info);
	cpu = PCPU_GET(vcpu_id);
	info.mfn = vtophys(vcpu_info) >> PAGE_SHIFT;
	info.offset = vtophys(vcpu_info) - trunc_page(vtophys(vcpu_info));

	rc = HYPERVISOR_vcpu_op(VCPUOP_register_vcpu_info, cpu, &info);
	if (rc != 0)
		DPCPU_SET(vcpu_info, &HYPERVISOR_shared_info->vcpu_info[cpu]);
	else
		DPCPU_SET(vcpu_info, vcpu_info);
}

SYSINIT(xen_hvm_init, SI_SUB_HYPERVISOR, SI_ORDER_FIRST, xen_hvm_sysinit, NULL);
#ifdef SMP
SYSINIT(xen_setup_cpus, SI_SUB_SMP, SI_ORDER_FIRST, xen_setup_cpus, NULL);
#endif
SYSINIT(xen_hvm_cpu_init, SI_SUB_INTR, SI_ORDER_FIRST, xen_hvm_cpu_init, NULL);
SYSINIT(xen_set_vcpu_id, SI_SUB_CPU, SI_ORDER_ANY, xen_set_vcpu_id, NULL);
