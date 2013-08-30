/*
 * Copyright (c) 2008 Citrix Systems, Inc.
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

#include <dev/pci/pcivar.h>
#include <machine/cpufunc.h>

#include <xen/xen-os.h>
#include <xen/features.h>
#include <xen/gnttab.h>
#include <xen/hypervisor.h>
#include <xen/hvm.h>
#include <xen/xen_intr.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <xen/interface/hvm/params.h>
#include <xen/interface/vcpu.h>

static MALLOC_DEFINE(M_XENHVM, "xen_hvm", "Xen HVM PV Support");

DPCPU_DEFINE(struct vcpu_info, vcpu_local_info);
DPCPU_DEFINE(struct vcpu_info *, vcpu_info);

/*-------------------------------- Global Data -------------------------------*/
/**
 * If non-zero, the hypervisor has been configured to use a direct
 * IDT event callback for interrupt injection.
 */
int xen_vector_callback_enabled;

/*------------------ Hypervisor Access Shared Memory Regions -----------------*/
/** Hypercall table accessed via HYPERVISOR_*_op() methods. */
char *hypercall_stubs;
shared_info_t *HYPERVISOR_shared_info;
enum xen_domain_type xen_domain_type = XEN_NATIVE;

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
	if (!base)
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

	xhp.domid = DOMID_SELF;
	xhp.index = HVM_PARAM_CALLBACK_IRQ;
	if (xen_feature(XENFEAT_hvm_callback_vector)) {
		int error;

		xhp.value = HVM_CALLBACK_VECTOR(IDT_EVTCHN);
		error = HYPERVISOR_hvm_op(HVMOP_set_param, &xhp);
		if (error == 0) {
			xen_vector_callback_enabled = 1;
			return;
		}
		printf("Xen HVM callback vector registration failed (%d). "
		       "Falling back to emulated device interrupt\n",
		       error);
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

	if (HYPERVISOR_hvm_op(HVMOP_set_param, &xhp))
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

void
xen_hvm_suspend(void)
{
}

void
xen_hvm_resume(void)
{
	xen_hvm_init_hypercall_stubs();
	xen_hvm_init_shared_info_page();
}
 
static void
xen_hvm_init(void *dummy __unused)
{
	if (xen_hvm_init_hypercall_stubs() != 0)
		return;

	xen_domain_type = XEN_HVM_DOMAIN;
	setup_xen_features();
	xen_hvm_init_shared_info_page();
	xen_hvm_set_callback(NULL);
	xen_hvm_disable_emulated_devices();
} 

void xen_hvm_init_cpu(void)
{
	int cpu = PCPU_GET(acpi_id);
	struct vcpu_info *vcpu_info;
	struct vcpu_register_vcpu_info info;
	int rc;

	vcpu_info = DPCPU_PTR(vcpu_local_info);
	info.mfn = vtophys(vcpu_info) >> PAGE_SHIFT;
	info.offset = vtophys(vcpu_info) - trunc_page(vtophys(vcpu_info));

	rc = HYPERVISOR_vcpu_op(VCPUOP_register_vcpu_info, cpu, &info);
	if (rc) {
		DPCPU_SET(vcpu_info, &HYPERVISOR_shared_info->vcpu_info[cpu]);
	} else {
		DPCPU_SET(vcpu_info, vcpu_info);
	}
}

SYSINIT(xen_hvm_init, SI_SUB_HYPERVISOR, SI_ORDER_FIRST, xen_hvm_init, NULL);
SYSINIT(xen_hvm_init_cpu, SI_SUB_INTR, SI_ORDER_FIRST, xen_hvm_init_cpu, NULL);
