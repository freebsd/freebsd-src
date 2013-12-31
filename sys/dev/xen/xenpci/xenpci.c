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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <machine/stdarg.h>

#include <xen/xen-os.h>
#include <xen/features.h>
#include <xen/hypervisor.h>
#include <xen/hvm.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/xen/xenpci/xenpcivar.h>

extern void xen_intr_handle_upcall(struct trapframe *trap_frame);

static device_t nexus;

/*
 * This is used to find our platform device instance.
 */
static devclass_t xenpci_devclass;

static int
xenpci_intr_filter(void *trap_frame)
{
	xen_intr_handle_upcall(trap_frame);
	return (FILTER_HANDLED);
}

static int
xenpci_irq_init(device_t device, struct xenpci_softc *scp)
{
	int error;

	error = BUS_SETUP_INTR(device_get_parent(device), device,
			       scp->res_irq, INTR_MPSAFE|INTR_TYPE_MISC,
			       xenpci_intr_filter, NULL, /*trap_frame*/NULL,
			       &scp->intr_cookie);
	if (error)
		return error;

#ifdef SMP
	/*
	 * When using the PCI event delivery callback we cannot assign
	 * events to specific vCPUs, so all events are delivered to vCPU#0 by
	 * Xen. Since the PCI interrupt can fire on any CPU by default, we
	 * need to bind it to vCPU#0 in order to ensure that
	 * xen_intr_handle_upcall always gets called on vCPU#0.
	 */
	error = BUS_BIND_INTR(device_get_parent(device), device,
	                      scp->res_irq, 0);
	if (error)
		return error;
#endif

	xen_hvm_set_callback(device);
	return (0);
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
	struct xenpci_softc *scp = device_get_softc(dev);
	devclass_t dc;
	int error;

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

	/*
	 * Hook the irq up to evtchn
	 */
	error = xenpci_irq_init(dev, scp);
	if (error) {
		device_printf(dev, "xenpci_irq_init failed(%d).\n",
			error);
		goto errexit;
	}

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

static int
xenpci_suspend(device_t dev)
{
	return (bus_generic_suspend(dev));
}

static int
xenpci_resume(device_t dev)
{
	xen_hvm_set_callback(dev);
	return (bus_generic_resume(dev));
}

static device_method_t xenpci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		xenpci_probe),
	DEVMETHOD(device_attach,	xenpci_attach),
	DEVMETHOD(device_detach,	xenpci_detach),
	DEVMETHOD(device_suspend,	xenpci_suspend),
	DEVMETHOD(device_resume,	xenpci_resume),

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
