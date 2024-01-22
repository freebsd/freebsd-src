/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>

#include <dev/pci/pcivar.h>

#include <machine/cpufunc.h>
#include <machine/cpu.h>
#include <machine/smp.h>

#include <x86/apicreg.h>

#include <xen/xen-os.h>
#include <xen/error.h>
#include <xen/features.h>
#include <xen/gnttab.h>
#include <xen/hypervisor.h>
#include <xen/hvm.h>
#include <xen/xen_intr.h>

#include <contrib/xen/arch-x86/cpuid.h>
#include <contrib/xen/hvm/params.h>
#include <contrib/xen/vcpu.h>

/*--------------------------- Forward Declarations ---------------------------*/
static void xen_hvm_cpu_init(void);

/*-------------------------------- Global Data -------------------------------*/
#ifdef SMP
struct cpu_ops xen_hvm_cpu_ops = {
	.cpu_init	= xen_hvm_cpu_init,
	.cpu_resume	= xen_hvm_cpu_init
};
#endif

static MALLOC_DEFINE(M_XENHVM, "xen_hvm", "Xen HVM PV Support");

/**
 * If non-zero, the hypervisor has been configured to use a direct
 * IDT event callback for interrupt injection.
 */
int xen_vector_callback_enabled;

/**
 * Signal whether the vector injected for the event channel upcall requires to
 * be EOI'ed on the local APIC.
 */
bool xen_evtchn_needs_ack;

/*------------------------------- Per-CPU Data -------------------------------*/
DPCPU_DECLARE(struct vcpu_info *, vcpu_info);

/*------------------------------ Sysctl tunables -----------------------------*/
int xen_disable_pv_disks = 0;
int xen_disable_pv_nics = 0;
TUNABLE_INT("hw.xen.disable_pv_disks", &xen_disable_pv_disks);
TUNABLE_INT("hw.xen.disable_pv_nics", &xen_disable_pv_nics);

/*---------------------- XEN Hypervisor Probe and Setup ----------------------*/

void xen_emergency_print(const char *str, size_t size)
{
	outsb(XEN_HVM_DEBUGCONS_IOPORT, str, size);
}

uint32_t xen_cpuid_base;

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

static void
hypervisor_quirks(unsigned int major, unsigned int minor)
{
#ifdef SMP
	if (((major < 4) || (major == 4 && minor <= 5)) &&
	    msix_disable_migration == -1) {
		/*
		 * Xen hypervisors prior to 4.6.0 do not properly
		 * handle updates to enabled MSI-X table entries,
		 * so disable MSI-X interrupt migration in that
		 * case.
		 */
		if (bootverbose)
			printf(
"Disabling MSI-X interrupt migration due to Xen hypervisor bug.\n"
"Set machdep.msix_disable_migration=0 to forcefully enable it.\n");
		msix_disable_migration = 1;
	}
#endif
}

static void
hypervisor_version(void)
{
	uint32_t regs[4];
	int major, minor;

	do_cpuid(xen_cpuid_base + 1, regs);

	major = regs[0] >> 16;
	minor = regs[0] & 0xffff;
	printf("XEN: Hypervisor version %d.%d detected.\n", major, minor);

	hypervisor_quirks(major, minor);
}

/*
 * Allocate and fill in the hypcall page.
 */
int
xen_hvm_init_hypercall_stubs(enum xen_hvm_init_type init_type)
{
	uint32_t regs[4];

	if (xen_cpuid_base != 0)
		/* Already setup. */
		goto out;

	xen_cpuid_base = xen_hvm_cpuid_base();
	if (xen_cpuid_base == 0)
		return (ENXIO);

	/*
	 * Find the hypercall pages.
	 */
	do_cpuid(xen_cpuid_base + 2, regs);
	if (regs[0] != 1)
		return (EINVAL);

	wrmsr(regs[1], (init_type == XEN_HVM_INIT_EARLY)
	    ? (vm_paddr_t)((uintptr_t)&hypercall_page - KERNBASE)
	    : vtophys(&hypercall_page));

out:
	hypervisor_version();
	return (0);
}

static void
xen_hvm_init_shared_info_page(void)
{
	struct xen_add_to_physmap xatp;

	if (xen_pv_domain()) {
		/*
		 * Already setup in the PV case, shared_info is passed inside
		 * of the start_info struct at start of day.
		 */
		return;
	}

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

static int
set_percpu_callback(unsigned int vcpu)
{
	struct xen_hvm_evtchn_upcall_vector vec;
	int error;

	vec.vcpu = vcpu;
	vec.vector = IDT_EVTCHN;
	error = HYPERVISOR_hvm_op(HVMOP_set_evtchn_upcall_vector, &vec);

	return (error != 0 ? xen_translate_error(error) : 0);
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

		error = set_percpu_callback(0);
		if (error == 0) {
			xen_evtchn_needs_ack = true;
			/* Trick toolstack to think we are enlightened */
			xhp.value = 1;
		} else
			xhp.value = HVM_CALLBACK_VECTOR(IDT_EVTCHN);
		error = HYPERVISOR_hvm_op(HVMOP_set_param, &xhp);
		if (error == 0) {
			xen_vector_callback_enabled = 1;
			return;
		} else if (xen_evtchn_needs_ack)
			panic("Unable to setup fake HVM param: %d", error);

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
	u_short disable_devs = 0;

	if (xen_pv_domain()) {
		/*
		 * No emulated devices in the PV case, so no need to unplug
		 * anything.
		 */
		if (xen_disable_pv_disks != 0 || xen_disable_pv_nics != 0)
			printf("PV devices cannot be disabled in PV guests\n");
		return;
	}

	if (inw(XEN_MAGIC_IOPORT) != XMI_MAGIC)
		return;

	if (xen_disable_pv_disks == 0) {
		if (bootverbose)
			printf("XEN: disabling emulated disks\n");
		disable_devs |= XMI_UNPLUG_IDE_DISKS;
	}
	if (xen_disable_pv_nics == 0) {
		if (bootverbose)
			printf("XEN: disabling emulated nics\n");
		disable_devs |= XMI_UNPLUG_NICS;
	}

	if (disable_devs != 0)
		outw(XEN_MAGIC_IOPORT, disable_devs);
}

static void
xen_hvm_init(enum xen_hvm_init_type init_type)
{
	int error;
	int i;

	if (!xen_domain() ||
	    init_type == XEN_HVM_INIT_CANCELLED_SUSPEND)
		return;

	error = xen_hvm_init_hypercall_stubs(init_type);

	switch (init_type) {
	case XEN_HVM_INIT_LATE:
		if (error != 0)
			return;

		setup_xen_features();
#ifdef SMP
		cpu_ops = xen_hvm_cpu_ops;
#endif
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
	xen_evtchn_needs_ack = false;
	xen_hvm_set_callback(NULL);

	/*
	 * On (PV)HVM domains we need to request the hypervisor to
	 * fill the shared info page, for PVH guest the shared_info page
	 * is passed inside the start_info struct and is already set, so this
	 * functions are no-ops.
	 */
	xen_hvm_init_shared_info_page();
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
	xen_hvm_init(XEN_HVM_INIT_LATE);
}
SYSINIT(xen_hvm_init, SI_SUB_HYPERVISOR, SI_ORDER_FIRST, xen_hvm_sysinit, NULL);

static void
xen_hvm_cpu_init(void)
{
	uint32_t regs[4];
	int rc;

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

	/*
	 * Set vCPU ID. If available fetch the ID from CPUID, if not just use
	 * the ACPI ID.
	 */
	KASSERT(xen_cpuid_base != 0, ("Invalid base Xen CPUID leaf"));
	cpuid_count(xen_cpuid_base + 4, 0, regs);
	KASSERT((regs[0] & XEN_HVM_CPUID_VCPU_ID_PRESENT) ||
	    !xen_pv_domain(),
	    ("Xen PV domain without vcpu_id in cpuid"));
	PCPU_SET(vcpu_id, (regs[0] & XEN_HVM_CPUID_VCPU_ID_PRESENT) ?
	    regs[1] : PCPU_GET(acpi_id));

	if (xen_evtchn_needs_ack && !IS_BSP()) {
		/*
		 * Setup the per-vpcu event channel upcall vector. This is only
		 * required when using the new HVMOP_set_evtchn_upcall_vector
		 * hypercall, which allows using a different vector for each
		 * vCPU. Note that FreeBSD uses the same vector for all vCPUs
		 * because it's not dynamically allocated.
		 */
		rc = set_percpu_callback(PCPU_GET(vcpu_id));
		if (rc != 0)
			panic("Event channel upcall vector setup failed: %d",
			    rc);
	}

	xen_setup_vcpu_info();
}
SYSINIT(xen_hvm_cpu_init, SI_SUB_INTR, SI_ORDER_FIRST, xen_hvm_cpu_init, NULL);

bool
xen_has_iommu_maps(void)
{
	uint32_t regs[4];

	KASSERT(xen_cpuid_base != 0, ("Invalid base Xen CPUID leaf"));
	cpuid_count(xen_cpuid_base + 4, 0, regs);

	return (regs[0] & XEN_HVM_CPUID_IOMMU_MAPPINGS);
}
