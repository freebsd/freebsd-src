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
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>

#include <dev/pci/pcivar.h>

#include <machine/_inttypes.h>
#include <machine/cpufunc.h>
#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/pc/bios.h>
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

	do_cpuid(hv_base + 1, regs);

	major = regs[0] >> 16;
	minor = regs[0] & 0xffff;
	printf("XEN: Hypervisor version %d.%d detected.\n", major, minor);

	hypervisor_quirks(major, minor);
}

/*
 * Translate linear to physical address when still running on the bootloader
 * created page-tables.
 */
static vm_paddr_t
early_init_vtop(void *addr)
{

	/*
	 * Using a KASSERT won't print anything, as this is before console
	 * initialization.
	 */
	if (__predict_false((uintptr_t)addr < KERNBASE)) {
		xc_printf("invalid linear address: %p\n", addr);
		halt();
	}

	return ((uintptr_t)addr - KERNBASE
#ifdef __amd64__
	    + kernphys - KERNLOAD
#endif
	    );
}

static int
map_shared_info(void)
{
	/*
	 * TODO shared info page should be mapped in an unpopulated (IOW:
	 * non-RAM) address.  But finding one at this point in boot is
	 * complicated, hence re-use a RAM address for the time being.  This
	 * sadly causes super-page shattering in the second stage translation
	 * page tables.
	 */
	static union {
		shared_info_t shared_info;
		uint8_t raw[PAGE_SIZE];
	} shared_page __attribute__((aligned(PAGE_SIZE)));
	static struct xen_add_to_physmap xatp = {
	    .domid = DOMID_SELF,
	    .space = XENMAPSPACE_shared_info,
	};
	int rc;

	_Static_assert(sizeof(shared_page) == PAGE_SIZE,
	    "invalid Xen shared_info struct size");

	if (xatp.gpfn == 0)
		xatp.gpfn = atop(early_init_vtop(&shared_page.shared_info));

	rc = HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp);
	if (rc != 0) {
		xc_printf("cannot map shared info page: %d\n", rc);
		HYPERVISOR_shared_info = NULL;
	} else if (HYPERVISOR_shared_info == NULL)
		HYPERVISOR_shared_info = &shared_page.shared_info;

	return (rc);
}

static void
fixup_console(void)
{
	struct xen_platform_op op = {
		.cmd = XENPF_get_dom0_console,
	};
	xenpf_dom0_console_t *console = &op.u.dom0_console;
	union {
		struct efi_fb efi;
		struct vbe_fb vbe;
	} *fb = NULL;
	int size;

	size = HYPERVISOR_platform_op(&op);
	if (size < 0) {
		xc_printf("Failed to get video console info: %d\n", size);
		return;
	}

	switch (console->video_type) {
	case XEN_VGATYPE_VESA_LFB:
		fb = (__typeof__ (fb))preload_search_info(preload_kmdp,
		    MODINFO_METADATA | MODINFOMD_VBE_FB);

		if (fb == NULL) {
			xc_printf("No VBE FB in kernel metadata\n");
			return;
		}

		_Static_assert(offsetof(struct vbe_fb, fb_bpp) ==
		    offsetof(struct efi_fb, fb_mask_reserved) +
		    sizeof(fb->efi.fb_mask_reserved),
		    "Bad structure overlay\n");
		fb->vbe.fb_bpp = console->u.vesa_lfb.bits_per_pixel;
		/* FALLTHROUGH */
	case XEN_VGATYPE_EFI_LFB:
		if (fb == NULL) {
			fb = (__typeof__ (fb))preload_search_info(preload_kmdp,
			    MODINFO_METADATA | MODINFOMD_EFI_FB);
			if (fb == NULL) {
				xc_printf("No EFI FB in kernel metadata\n");
				return;
			}
		}

		fb->efi.fb_addr = console->u.vesa_lfb.lfb_base;
		if (size >
		    offsetof(xenpf_dom0_console_t, u.vesa_lfb.ext_lfb_base))
			fb->efi.fb_addr |=
			    (uint64_t)console->u.vesa_lfb.ext_lfb_base << 32;
		fb->efi.fb_size = console->u.vesa_lfb.lfb_size << 16;
		fb->efi.fb_height = console->u.vesa_lfb.height;
		fb->efi.fb_width = console->u.vesa_lfb.width;
		fb->efi.fb_stride = (console->u.vesa_lfb.bytes_per_line << 3) /
		    console->u.vesa_lfb.bits_per_pixel;
#define FBMASK(c) \
    ((~0u << console->u.vesa_lfb.c ## _pos) & \
    (~0u >> (32 - console->u.vesa_lfb.c ## _pos - \
    console->u.vesa_lfb.c ## _size)))
		fb->efi.fb_mask_red = FBMASK(red);
		fb->efi.fb_mask_green = FBMASK(green);
		fb->efi.fb_mask_blue = FBMASK(blue);
		fb->efi.fb_mask_reserved = FBMASK(rsvd);
#undef FBMASK
		break;

	default:
		xc_printf("Video console type unsupported\n");
		return;
	}
}

/* Early initialization when running as a Xen guest. */
void
xen_early_init(void)
{
	uint32_t regs[4];
	int rc;

	if (hv_high < hv_base + 2) {
		xc_printf("Invalid maximum leaves for hv_base\n");
		vm_guest = VM_GUEST_VM;
		return;
	}

	/* Find the hypercall pages. */
	do_cpuid(hv_base + 2, regs);
	if (regs[0] != 1) {
		xc_printf("Invalid number of hypercall pages %u\n",
		    regs[0]);
		vm_guest = VM_GUEST_VM;
		return;
	}

	wrmsr(regs[1], early_init_vtop(&hypercall_page));

	rc = map_shared_info();
	if (rc != 0) {
		vm_guest = VM_GUEST_VM;
		return;
	}

	if (xen_initial_domain())
	    /* Fixup video console information in case Xen changed the mode. */
	    fixup_console();
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
	unsigned int i;

	if (!xen_domain() ||
	    init_type == XEN_HVM_INIT_CANCELLED_SUSPEND)
		return;

	hypervisor_version();

	switch (init_type) {
	case XEN_HVM_INIT_LATE:
		setup_xen_features();
#ifdef SMP
		cpu_ops = xen_hvm_cpu_ops;
#endif
		break;
	case XEN_HVM_INIT_RESUME:
		/* Clear stale vcpu_info. */
		CPU_FOREACH(i)
			DPCPU_ID_SET(i, vcpu_info, NULL);

		if (map_shared_info() != 0)
			panic("cannot map Xen shared info page");

		break;
	default:
		panic("Unsupported HVM initialization type");
	}

	xen_vector_callback_enabled = 0;
	xen_evtchn_needs_ack = false;
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
	KASSERT(hv_base != 0, ("Invalid base Xen CPUID leaf"));
	cpuid_count(hv_base + 4, 0, regs);
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

	KASSERT(hv_base != 0, ("Invalid base Xen CPUID leaf"));
	cpuid_count(hv_base + 4, 0, regs);

	return (regs[0] & XEN_HVM_CPUID_IOMMU_MAPPINGS);
}

int
xen_arch_init_physmem(device_t dev, struct rman *mem)
{
	static struct bios_smap smap[128];
	struct xen_memory_map memmap = {
		.nr_entries = nitems(smap),
	};
	unsigned int i;
	int error;

	set_xen_guest_handle(memmap.buffer, smap);
	error = HYPERVISOR_memory_op(XENMEM_memory_map, &memmap);
	if (error != 0)
		return (0);

	/*
	 * Fill with UNUSABLE regions, as it's always fine to use those for
	 * foreign mappings, they will never be populated.
	 */
	for (i = 0; i < memmap.nr_entries; i++) {
		const vm_paddr_t max_phys = cpu_getmaxphyaddr();
		vm_paddr_t start, end;

		if (smap[i].type != SMAP_TYPE_ACPI_ERROR)
			continue;

		start = round_page(smap[i].base);
		/* In 32bit mode we possibly need to truncate the addresses. */
		end = MIN(trunc_page(smap[i].base + smap[i].length) - 1,
		    max_phys);

		if (start >= end)
			continue;

		if (bootverbose != 0)
			device_printf(dev,
			    "scratch mapping region @ [%016jx, %016jx]\n",
			    start, end);

		error = rman_manage_region(mem, start, end);
		if (error != 0)
			device_printf(dev,
			    "unable to add scratch region [%016jx, %016jx]: %d\n",
			    start, end, error);
	}

	return (0);
}
