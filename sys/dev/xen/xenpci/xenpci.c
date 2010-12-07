/*
 * Copyright (c) 2008 Citrix Systems, Inc.
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
__FBSDID("$FreeBSD: head/sys/dev/xen/xenpci/xenpci.c 214077 2010-10-19 20:53:30Z gibbs $");

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
static device_t nexus;

/*
 * This is used to find our platform device instance.
 */
static devclass_t xenpci_devclass;

/*
 * Return the CPUID base address for Xen functions.
 */
static uint32_t
xenpci_cpuid_base(void)
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
xenpci_init_hypercall_stubs(device_t dev, struct xenpci_softc * scp)
{
	uint32_t base, regs[4];
	int i;

	base = xenpci_cpuid_base();
	if (!base) {
		device_printf(dev, "Xen platform device but not Xen VMM\n");
		return (EINVAL);
	}

	if (bootverbose) {
		do_cpuid(base + 1, regs);
		device_printf(dev, "Xen version %d.%d.\n",
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

/*
 * After a resume, re-initialise the hypercall page.
 */
static void
xenpci_resume_hypercall_stubs(device_t dev, struct xenpci_softc * scp)
{
	uint32_t base, regs[4];
	int i;

	base = xenpci_cpuid_base();

	do_cpuid(base + 2, regs);
	for (i = 0; i < regs[0]; i++) {
		wrmsr(regs[1], vtophys(hypercall_stubs + i * PAGE_SIZE) + i);
	}
}

/*
 * Tell the hypervisor how to contact us for event channel callbacks.
 */
static void
xenpci_set_callback(device_t dev)
{
	int irq;
	uint64_t callback;
	struct xen_hvm_param xhp;

	irq = pci_get_irq(dev);
	if (irq < 16) {
		callback = irq;
	} else {
		callback = (pci_get_intpin(dev) - 1) & 3;
		callback |= pci_get_slot(dev) << 11;
		callback |= 1ull << 56;
	}

	xhp.domid = DOMID_SELF;
	xhp.index = HVM_PARAM_CALLBACK_IRQ;
	xhp.value = callback;
	if (HYPERVISOR_hvm_op(HVMOP_set_param, &xhp))
		panic("Can't set evtchn callback");
}


/*
 * Deallocate anything allocated by xenpci_allocate_resources.
 */
static int
xenpci_deallocate_resources(device_t dev)
{
	struct xenpci_softc *scp = device_get_softc(dev);

	if (scp->res_irq != 0) {
		bus_deactivate_resource(dev, SYS_RES_IRQ,
			scp->rid_irq, scp->res_irq);
		bus_release_resource(dev, SYS_RES_IRQ,
			scp->rid_irq, scp->res_irq);
		scp->res_irq = 0;
	}
	if (scp->res_memory != 0) {
		bus_deactivate_resource(dev, SYS_RES_MEMORY,
			scp->rid_memory, scp->res_memory);
		bus_release_resource(dev, SYS_RES_MEMORY,
			scp->rid_memory, scp->res_memory);
		scp->res_memory = 0;
	}

	return (0);
}

/*
 * Allocate irq and memory resources.
 */
static int
xenpci_allocate_resources(device_t dev)
{
	struct xenpci_softc *scp = device_get_softc(dev);

	scp->res_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ,
			&scp->rid_irq, RF_SHAREABLE|RF_ACTIVE);
	if (scp->res_irq == NULL) {
		printf("xenpci Could not allocate irq.\n");
		goto errexit;
	}

	scp->rid_memory = PCIR_BAR(1);
	scp->res_memory = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
			&scp->rid_memory, RF_ACTIVE);
	if (scp->res_memory == NULL) {
		printf("xenpci Could not allocate memory bar.\n");
		goto errexit;
	}

	scp->phys_next = rman_get_start(scp->res_memory);

	return (0);

errexit:
	/* Cleanup anything we may have assigned. */
	xenpci_deallocate_resources(dev);
	return (ENXIO); /* For want of a better idea. */
}

/*
 * Allocate a physical address range from our mmio region.
 */
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

/*
 * Allocate a physical address range from our mmio region.
 */
int
xenpci_alloc_space(size_t sz, vm_paddr_t *pa)
{
	device_t dev = devclass_get_device(xenpci_devclass, 0);

	if (dev) {
		return (xenpci_alloc_space_int(device_get_softc(dev),
			sz, pa));
	} else {
		return (ENOMEM);
	}
}

static struct resource *
xenpci_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	return (BUS_ALLOC_RESOURCE(nexus, child, type, rid, start,
	    end, count, flags));
}


static int
xenpci_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	return (BUS_RELEASE_RESOURCE(nexus, child, type, rid, r));
}

static int
xenpci_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	return (BUS_ACTIVATE_RESOURCE(nexus, child, type, rid, r));
}

static int
xenpci_deactivate_resource(device_t dev, device_t child, int type,
    int rid, struct resource *r)
{
	return (BUS_DEACTIVATE_RESOURCE(nexus, child, type, rid, r));
}

/*
 * Called very early in the resume sequence - reinitialise the various
 * bits of Xen machinery including the hypercall page and the shared
 * info page.
 */
void
xenpci_resume()
{
	device_t dev = devclass_get_device(xenpci_devclass, 0);
	struct xenpci_softc *scp = device_get_softc(dev);
	struct xen_add_to_physmap xatp;

	xenpci_resume_hypercall_stubs(dev, scp);

	xatp.domid = DOMID_SELF;
	xatp.idx = 0;
	xatp.space = XENMAPSPACE_shared_info;
	xatp.gpfn = shared_info_pa >> PAGE_SHIFT;
	if (HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp))
		panic("HYPERVISOR_memory_op failed");

	pmap_kenter((vm_offset_t) HYPERVISOR_shared_info, shared_info_pa);

	xenpci_set_callback(dev);

	gnttab_resume();
	irq_resume();
}

/*
 * Probe - just check device ID.
 */
static int
xenpci_probe(device_t dev)
{

	if (pci_get_devid(dev) != 0x00015853)
		return (ENXIO);

	device_set_desc(dev, "Xen Platform Device");
	return (bus_generic_probe(dev));
}

/*
 * Attach - find resources and talk to Xen.
 */
static int
xenpci_attach(device_t dev)
{
	int error;
	struct xenpci_softc *scp = device_get_softc(dev);
	struct xen_add_to_physmap xatp;
	vm_offset_t shared_va;
	devclass_t dc;

	/*
	 * Find and record nexus0.  Since we are not really on the
	 * PCI bus, all resource operations are directed to nexus
	 * instead of through our parent.
	 */
	if ((dc = devclass_find("nexus"))  == 0
	 || (nexus = devclass_get_device(dc, 0)) == 0) {
		device_printf(dev, "unable to find nexus.");
		return (ENOENT);
	}

	error = xenpci_allocate_resources(dev);
	if (error) {
		device_printf(dev, "xenpci_allocate_resources failed(%d).\n",
		    error);
		goto errexit;
	}

	error = xenpci_init_hypercall_stubs(dev, scp);
	if (error) {
		device_printf(dev, "xenpci_init_hypercall_stubs failed(%d).\n",
		    error);
		goto errexit;
	}

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
	xenpci_irq_init(dev, scp);
	xenpci_set_callback(dev);

	return (bus_generic_attach(dev));

errexit:
	/*
	 * Undo anything we may have done.
	 */
	xenpci_deallocate_resources(dev);
	return (error);
}

/*
 * Detach - reverse anything done by attach.
 */
static int
xenpci_detach(device_t dev)
{
	struct xenpci_softc *scp = device_get_softc(dev);
	device_t parent = device_get_parent(dev);

	/*
	 * Take our interrupt handler out of the list of handlers
	 * that can handle this irq.
	 */
	if (scp->intr_cookie != NULL) {
		if (BUS_TEARDOWN_INTR(parent, dev,
		    scp->res_irq, scp->intr_cookie) != 0)
			device_printf(dev,
			    "intr teardown failed.. continuing\n");
		scp->intr_cookie = NULL;
	}

	/*
	 * Deallocate any system resources we may have
	 * allocated on behalf of this driver.
	 */
	return (xenpci_deallocate_resources(dev));
}

static device_method_t xenpci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		xenpci_probe),
	DEVMETHOD(device_attach,	xenpci_attach),
	DEVMETHOD(device_detach,	xenpci_detach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_add_child,	bus_generic_add_child),
	DEVMETHOD(bus_alloc_resource,   xenpci_alloc_resource),
	DEVMETHOD(bus_release_resource, xenpci_release_resource),
	DEVMETHOD(bus_activate_resource, xenpci_activate_resource),
	DEVMETHOD(bus_deactivate_resource, xenpci_deactivate_resource),

	{ 0, 0 }
};

static driver_t xenpci_driver = {
	"xenpci",
	xenpci_methods,
	sizeof(struct xenpci_softc),
};

DRIVER_MODULE(xenpci, pci, xenpci_driver, xenpci_devclass, 0, 0);
