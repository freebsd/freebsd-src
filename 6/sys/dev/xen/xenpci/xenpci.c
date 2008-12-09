/*
 * Copyright (c) [year] [your name]
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
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/time.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <machine/stdarg.h>
#include <machine/xen/xen-os.h>
#include <xen/features.h>
#include <xen/hypervisor.h>
#include <xen/gnttab.h>
#include <xen/xen_intr.h>
#include <xen/interface/memory.h>
#include <xen/interface/hvm/params.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>

#include <dev/xen/xenpci/xenpcivar.h>

/*
 * These variables are used by the rest of the kernel to access the
 * hypervisor.
 */
char *hypercall_stubs;
shared_info_t *HYPERVISOR_shared_info;
static vm_paddr_t shared_info_pa;

/*
 * The softc is automatically allocated by the parent bus using the
 * size specified in the driver_t declaration below.
 */
#define DEVICE2SOFTC(dev) ((struct xenpci_softc *) device_get_softc(dev))

/* Function prototypes (these should all be static). */
static int xenpci_deallocate_resources(device_t device);
static int xenpci_allocate_resources(device_t device);
static int xenpci_attach(device_t device, struct xenpci_softc *scp);
static int xenpci_detach(device_t device, struct xenpci_softc *scp);

static int xenpci_alloc_space_int(struct xenpci_softc *scp, size_t sz,
    u_long *pa);

static devclass_t xenpci_devclass;

static int	xenpci_pci_probe(device_t);
static int	xenpci_pci_attach(device_t);
static int	xenpci_pci_detach(device_t);
static int	xenpci_pci_resume(device_t);

static device_method_t xenpci_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		xenpci_pci_probe),
	DEVMETHOD(device_attach,	xenpci_pci_attach),
	DEVMETHOD(device_detach,	xenpci_pci_detach),
	DEVMETHOD(device_resume,	xenpci_pci_resume),

	/* Bus interface */
	DEVMETHOD(bus_add_child,	bus_generic_add_child),

	{ 0, 0 }
};

static driver_t xenpci_pci_driver = {
	"xenpci",
	xenpci_pci_methods,
	sizeof(struct xenpci_softc),
};

DRIVER_MODULE(xenpci, pci, xenpci_pci_driver, xenpci_devclass, 0, 0);

static struct _pcsid
{
	u_int32_t	type;
	const char	*desc;
} pci_ids[] = {
	{ 0x00015853,	"Xen Platform Device"			},
	{ 0x00000000,	NULL					}
};

static int
xenpci_pci_probe (device_t device)
{
	u_int32_t	type = pci_get_devid(device);
	struct _pcsid	*ep = pci_ids;

	while (ep->type && ep->type != type)
		++ep;
	if (ep->desc) {
		device_set_desc(device, ep->desc);
		return (bus_generic_probe(device));
	} else
		return (ENXIO);
}

static int
xenpci_pci_attach(device_t device)
{
        int	error;
	struct xenpci_softc *scp = DEVICE2SOFTC(device);

        error = xenpci_attach(device, scp);
        if (error)
                xenpci_pci_detach(device);
        return (error);
}

static int
xenpci_pci_detach (device_t device)
{
	struct xenpci_softc *scp = DEVICE2SOFTC(device);

        return (xenpci_detach(device, scp));
}

static int
xenpci_pci_resume(device_t device)
{

	return (bus_generic_resume(device));
}

/*
 * Common Attachment sub-functions
 */
static uint32_t
xenpci_cpuid_base(void)
{
	uint32_t base, regs[4];

	for (base = 0x40000000; base < 0x40001000; base += 0x100) {
		do_cpuid(base, regs);
		if (!memcmp("XenVMMXenVMM", &regs[1], 12)
		    && (regs[0] - base) >= 2)
			return (base);
	}
	return (0);
}

static int
xenpci_init_hypercall_stubs(device_t device, struct xenpci_softc * scp)
{
	uint32_t base, regs[4];
	int i;

	base = xenpci_cpuid_base();
	if (!base) {
		device_printf(device, "Xen platform device but not Xen VMM\n");
		return (EINVAL);
	}

	if (bootverbose) {
		do_cpuid(base + 1, regs);
		device_printf(device, "Xen version %d.%d.\n",
		    regs[0] >> 16, regs[0] & 0xffff);
	}

	/*
	 * Find the hypercall pages.
	 */
	do_cpuid(base + 2, regs);
	
	hypercall_stubs = malloc(regs[0] * PAGE_SIZE, M_TEMP, M_WAITOK);

	for (i = 0; i < regs[0]; i++) {
		wrmsr(regs[1], vtophys(hypercall_stubs + i * PAGE_SIZE) + i);
	}

	return (0);
}

static void
xenpci_resume_hypercall_stubs(device_t device, struct xenpci_softc * scp)
{
	uint32_t base, regs[4];
	int i;

	base = xenpci_cpuid_base();

	do_cpuid(base + 2, regs);
	for (i = 0; i < regs[0]; i++) {
		wrmsr(regs[1], vtophys(hypercall_stubs + i * PAGE_SIZE) + i);
	}
}

static void
xenpci_set_callback(device_t device)
{
	int irq;
	uint64_t callback;
	struct xen_hvm_param xhp;

	irq = pci_get_irq(device);
	if (irq < 16) {
		callback = irq;
	} else {
		callback = (pci_get_intpin(device) - 1) & 3;
		callback |= pci_get_slot(device) << 11;
		callback |= 1ull << 56;
	}

	xhp.domid = DOMID_SELF;
	xhp.index = HVM_PARAM_CALLBACK_IRQ;
	xhp.value = callback;
	if (HYPERVISOR_hvm_op(HVMOP_set_param, &xhp))
		panic("Can't set evtchn callback");
}

static int
xenpci_attach(device_t device, struct xenpci_softc * scp)
{
	struct xen_add_to_physmap xatp;
	vm_offset_t shared_va;

	if (xenpci_allocate_resources(device))
		goto errexit;

	scp->phys_next = rman_get_start(scp->res_memory);

	if (xenpci_init_hypercall_stubs(device, scp))
		goto errexit;

	setup_xen_features();

	xenpci_alloc_space_int(scp, PAGE_SIZE, &shared_info_pa); 

	xatp.domid = DOMID_SELF;
	xatp.idx = 0;
	xatp.space = XENMAPSPACE_shared_info;
	xatp.gpfn = shared_info_pa >> PAGE_SHIFT;
	if (HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp))
		panic("HYPERVISOR_memory_op failed");

	shared_va = kmem_alloc_nofault(kernel_map, PAGE_SIZE);
	pmap_kenter(shared_va, shared_info_pa);
	HYPERVISOR_shared_info = (void *) shared_va;

	/*
	 * Hook the irq up to evtchn
	 */
	xenpci_irq_init(device, scp);
	xenpci_set_callback(device);

	return (bus_generic_attach(device));

errexit:
	/*
	 * Undo anything we may have done.
	 */
	xenpci_detach(device, scp);
	return (ENXIO);
}

static int
xenpci_detach(device_t device, struct xenpci_softc *scp)
{
	device_t parent = device_get_parent(device);

	/*
	 * Take our interrupt handler out of the list of handlers
	 * that can handle this irq.
	 */
	if (scp->intr_cookie != NULL) {
		if (BUS_TEARDOWN_INTR(parent, device,
			scp->res_irq, scp->intr_cookie) != 0)
				printf("intr teardown failed.. continuing\n");
		scp->intr_cookie = NULL;
	}

	/*
	 * Deallocate any system resources we may have
	 * allocated on behalf of this driver.
	 */
	return xenpci_deallocate_resources(device);
}

static int
xenpci_allocate_resources(device_t device)
{
	int error;
	struct xenpci_softc *scp = DEVICE2SOFTC(device);

	scp->res_irq = bus_alloc_resource_any(device, SYS_RES_IRQ,
			&scp->rid_irq, RF_SHAREABLE|RF_ACTIVE);
	if (scp->res_irq == NULL)
		goto errexit;

	scp->rid_ioport = PCIR_BAR(0);
	scp->res_ioport = bus_alloc_resource_any(device, SYS_RES_IOPORT,
			&scp->rid_ioport, RF_ACTIVE);
	if (scp->res_ioport == NULL)
		goto errexit;

	scp->rid_memory = PCIR_BAR(1);
	scp->res_memory = bus_alloc_resource_any(device, SYS_RES_MEMORY,
			&scp->rid_memory, RF_ACTIVE);
	if (scp->res_memory == NULL)
		goto errexit;
	return (0);

errexit:
	error = ENXIO;
	/* Cleanup anything we may have assigned. */
	xenpci_deallocate_resources(device);
	return (ENXIO); /* For want of a better idea. */
}

static int
xenpci_deallocate_resources(device_t device)
{
	struct xenpci_softc *scp = DEVICE2SOFTC(device);

	if (scp->res_irq != 0) {
		bus_deactivate_resource(device, SYS_RES_IRQ,
			scp->rid_irq, scp->res_irq);
		bus_release_resource(device, SYS_RES_IRQ,
			scp->rid_irq, scp->res_irq);
		scp->res_irq = 0;
	}
	if (scp->res_ioport != 0) {
		bus_deactivate_resource(device, SYS_RES_IOPORT,
			scp->rid_ioport, scp->res_ioport);
		bus_release_resource(device, SYS_RES_IOPORT,
			scp->rid_ioport, scp->res_ioport);
		scp->res_ioport = 0;
	}
	if (scp->res_memory != 0) {
		bus_deactivate_resource(device, SYS_RES_MEMORY,
			scp->rid_memory, scp->res_memory);
		bus_release_resource(device, SYS_RES_MEMORY,
			scp->rid_memory, scp->res_memory);
		scp->res_memory = 0;
	}

	return (0);
}

static int
xenpci_alloc_space_int(struct xenpci_softc *scp, size_t sz,
    vm_paddr_t *pa)
{

	if (scp->phys_next + sz > rman_get_end(scp->res_memory)) {
		return (ENOMEM);
	}

	*pa = scp->phys_next;
	scp->phys_next += sz;

	return (0);
}

int
xenpci_alloc_space(size_t sz, vm_paddr_t *pa)
{
	device_t device = devclass_get_device(xenpci_devclass, 0);

	if (device) {
		return (xenpci_alloc_space_int(DEVICE2SOFTC(device),
			sz, pa));
	} else {
		return (ENOMEM);
	}
}

void
xenpci_resume()
{
	device_t device = devclass_get_device(xenpci_devclass, 0);
	struct xenpci_softc *scp = DEVICE2SOFTC(device);
	struct xen_add_to_physmap xatp;

	xenpci_resume_hypercall_stubs(device, scp);

	xatp.domid = DOMID_SELF;
	xatp.idx = 0;
	xatp.space = XENMAPSPACE_shared_info;
	xatp.gpfn = shared_info_pa >> PAGE_SHIFT;
	if (HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp))
		panic("HYPERVISOR_memory_op failed");

	pmap_kenter((vm_offset_t) HYPERVISOR_shared_info, shared_info_pa);

	xenpci_set_callback(device);

	gnttab_resume();
	irq_resume();
}

