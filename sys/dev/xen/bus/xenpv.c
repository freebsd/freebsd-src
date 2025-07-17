/*
 * Copyright (c) 2014 Roger Pau Monné <roger.pau@citrix.com>
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
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/pcpu.h>
#include <sys/rman.h>
#include <sys/smp.h>
#include <sys/limits.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>
#include <vm/vm_phys.h>

#include <xen/xen-os.h>
#include <xen/gnttab.h>

#include "xenmem_if.h"

/*
 * Allocate unused physical memory above 4GB in order to map memory
 * from foreign domains. We use memory starting at 4GB in order to
 * prevent clashes with MMIO/ACPI regions.
 *
 * Since this is not possible on i386 just use any available memory
 * chunk above 1MB and hope we don't clash with anything else.
 *
 * Other architectures better document MMIO regions and drivers more
 * reliably reserve them.  As such, allow using any unpopulated memory
 * region.
 */
#ifdef __amd64__
#define LOW_MEM_LIMIT	0x100000000ul
#elif defined(__i386__)
#define LOW_MEM_LIMIT	0x100000ul
#else
#define LOW_MEM_LIMIT	0
#endif

/*
 * Memory ranges available for creating external mappings (foreign or grant
 * pages for example).
 */
static struct rman unpopulated_mem = {
	.rm_end = ~0,
	.rm_type = RMAN_ARRAY,
	.rm_descr = "Xen scratch memory",
};

static void
xenpv_identify(driver_t *driver, device_t parent)
{
	if (!xen_domain())
		return;

	/* Make sure there's only one xenpv device. */
	if (devclass_get_device(devclass_find(driver->name), 0))
		return;

	/*
	 * The xenpv bus should be the last to attach in order
	 * to properly detect if an ISA bus has already been added.
	 */
	if (BUS_ADD_CHILD(parent, UINT_MAX, driver->name, 0) == NULL)
		panic("Unable to attach xenpv bus.");
}

static int
xenpv_probe(device_t dev)
{

	device_set_desc(dev, "Xen PV bus");
	return (BUS_PROBE_NOWILDCARD);
}

/* Dummy init for arches that don't have a specific implementation. */
int __weak_symbol
xen_arch_init_physmem(device_t dev, struct rman *mem)
{

	return (0);
}

static int
xenpv_attach(device_t dev)
{
	int error = rman_init(&unpopulated_mem);

	if (error != 0)
		return (error);

	error = xen_arch_init_physmem(dev, &unpopulated_mem);
	if (error != 0)
		return (error);

	/*
	 * Let our child drivers identify any child devices that they
	 * can find.  Once that is done attach any devices that we
	 * found.
	 */
	bus_identify_children(dev);
	bus_attach_children(dev);

	return (0);
}

static int
release_unpopulated_mem(device_t dev, struct resource *res)
{

	return (rman_is_region_manager(res, &unpopulated_mem) ?
	    rman_release_resource(res) : bus_release_resource(dev, res));
}

static struct resource *
xenpv_alloc_physmem(device_t dev, device_t child, int *res_id, size_t size)
{
	struct resource *res;
	vm_paddr_t phys_addr;
	void *virt_addr;
	int error;
	const unsigned int flags = RF_ACTIVE | RF_UNMAPPED |
	    RF_ALIGNMENT_LOG2(PAGE_SHIFT);

	KASSERT((size & PAGE_MASK) == 0, ("unaligned size requested"));
	size = round_page(size);

	/* Attempt to allocate from arch resource manager. */
	res = rman_reserve_resource(&unpopulated_mem, 0, ~0, size, flags,
	    child);
	if (res != NULL) {
		rman_set_rid(res, *res_id);
		rman_set_type(res, SYS_RES_MEMORY);
	} else {
		static bool warned = false;

		/* Fallback to generic MMIO allocator. */
		if (__predict_false(!warned)) {
			warned = true;
			device_printf(dev,
			    "unable to allocate from arch specific routine, "
			    "fall back to unused memory areas\n");
		}
		res = bus_alloc_resource(child, SYS_RES_MEMORY, res_id,
		    LOW_MEM_LIMIT, ~0, size, flags);
	}

	if (res == NULL) {
		device_printf(dev,
		    "failed to allocate Xen unpopulated memory\n");
		return (NULL);
	}

	phys_addr = rman_get_start(res);
	error = vm_phys_fictitious_reg_range(phys_addr, phys_addr + size,
	    VM_MEMATTR_XEN);
	if (error) {
		int error = release_unpopulated_mem(child, res);

		if (error != 0)
			device_printf(dev, "failed to release resource: %d\n",
			    error);

		return (NULL);
	}
	virt_addr = pmap_mapdev_attr(phys_addr, size, VM_MEMATTR_XEN);
	KASSERT(virt_addr != NULL, ("Failed to create linear mappings"));
	rman_set_virtual(res, virt_addr);

	return (res);
}

static int
xenpv_free_physmem(device_t dev, device_t child, int res_id, struct resource *res)
{
	vm_paddr_t phys_addr;
	void *virt_addr;
	size_t size;

	phys_addr = rman_get_start(res);
	size = rman_get_size(res);
	virt_addr = rman_get_virtual(res);

	pmap_unmapdev(virt_addr, size);
	vm_phys_fictitious_unreg_range(phys_addr, phys_addr + size);

	return (release_unpopulated_mem(child, res));
}

static device_method_t xenpv_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,		xenpv_identify),
	DEVMETHOD(device_probe,			xenpv_probe),
	DEVMETHOD(device_attach,		xenpv_attach),
	DEVMETHOD(device_suspend,		bus_generic_suspend),
	DEVMETHOD(device_resume,		bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_add_child,		bus_generic_add_child),
	DEVMETHOD(bus_alloc_resource,		bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,		bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),

	/* Interface to allocate memory for foreign mappings */
	DEVMETHOD(xenmem_alloc,			xenpv_alloc_physmem),
	DEVMETHOD(xenmem_free,			xenpv_free_physmem),

	DEVMETHOD_END
};

static driver_t xenpv_driver = {
	"xenpv",
	xenpv_methods,
	0,
};

DRIVER_MODULE(xenpv, nexus, xenpv_driver, 0, 0);

struct resource *
xenmem_alloc(device_t dev, int *res_id, size_t size)
{
	device_t parent;

	parent = device_get_parent(dev);
	if (parent == NULL)
		return (NULL);
	return (XENMEM_ALLOC(parent, dev, res_id, size));
}

int
xenmem_free(device_t dev, int res_id, struct resource *res)
{
	device_t parent;

	parent = device_get_parent(dev);
	if (parent == NULL)
		return (ENXIO);
	return (XENMEM_FREE(parent, dev, res_id, res));
}
