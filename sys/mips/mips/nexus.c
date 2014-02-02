/*-
 * Copyright 1998 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * This code implements a `root nexus' for MIPS Architecture
 * machines.  The function of the root nexus is to serve as an
 * attachment point for both processors and buses, and to manage
 * resources which are common to all of them.  In particular,
 * this code implements the core resource managers for interrupt
 * requests and memory address space.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/interrupt.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/intr_machdep.h>
#include <machine/pmap.h>
#include <machine/resource.h>
#include <machine/vmparam.h>

#include "opt_platform.h"

#ifdef FDT
#include <dev/ofw/ofw_nexus.h>
#endif

#undef NEXUS_DEBUG
#ifdef NEXUS_DEBUG
#define dprintf printf
#else 
#define dprintf(x, arg...)
#endif  /* NEXUS_DEBUG */

#define NUM_MIPS_IRQS	6

#ifndef FDT
static MALLOC_DEFINE(M_NEXUSDEV, "nexusdev", "Nexus device");

struct nexus_device {
	struct resource_list	nx_resources;
};

#define DEVTONX(dev)	((struct nexus_device *)device_get_ivars(dev))

static struct rman irq_rman;
static struct rman mem_rman;

static struct resource *
		nexus_alloc_resource(device_t, device_t, int, int *, u_long,
		    u_long, u_long, u_int);
static device_t	nexus_add_child(device_t, u_int, const char *, int);
static int	nexus_attach(device_t);
static void	nexus_delete_resource(device_t, device_t, int, int);
static struct resource_list *
		nexus_get_reslist(device_t, device_t);
static int	nexus_get_resource(device_t, device_t, int, int, u_long *,
		    u_long *);
static int	nexus_print_child(device_t, device_t);
static int	nexus_print_all_resources(device_t dev);
static int	nexus_probe(device_t);
static int	nexus_release_resource(device_t, device_t, int, int,
		    struct resource *);
static int	nexus_set_resource(device_t, device_t, int, int, u_long,
		    u_long);
#endif
static int	nexus_activate_resource(device_t, device_t, int, int,
		    struct resource *);
static int	nexus_deactivate_resource(device_t, device_t, int, int,
		    struct resource *);
static void	nexus_hinted_child(device_t, const char *, int);
static int	nexus_setup_intr(device_t dev, device_t child,
		    struct resource *res, int flags, driver_filter_t *filt,
		    driver_intr_t *intr, void *arg, void **cookiep);
static int	nexus_teardown_intr(device_t, device_t, struct resource *,
		    void *);

static device_method_t nexus_methods[] = {
#ifndef FDT
	/* Device interface */
	DEVMETHOD(device_probe,		nexus_probe),
	DEVMETHOD(device_attach,	nexus_attach),

	/* Bus interface */
	DEVMETHOD(bus_add_child,	nexus_add_child),
	DEVMETHOD(bus_alloc_resource,	nexus_alloc_resource),
	DEVMETHOD(bus_delete_resource,	nexus_delete_resource),
	DEVMETHOD(bus_get_resource,	nexus_get_resource),
	DEVMETHOD(bus_get_resource_list,	nexus_get_reslist),
	DEVMETHOD(bus_print_child,	nexus_print_child),
	DEVMETHOD(bus_release_resource,	nexus_release_resource),
	DEVMETHOD(bus_set_resource,	nexus_set_resource),
#endif
	DEVMETHOD(bus_setup_intr,	nexus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	nexus_teardown_intr),
	DEVMETHOD(bus_activate_resource,nexus_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	nexus_deactivate_resource),
	DEVMETHOD(bus_hinted_child,	nexus_hinted_child),

	{ 0, 0 }
};

#ifndef FDT
static driver_t nexus_driver = {
	"nexus",
	nexus_methods,
	1			/* no softc */
};
#else
DEFINE_CLASS_1(nexus, nexus_driver, nexus_methods,
    sizeof(struct ofw_nexus_softc), ofw_nexus_driver);
#endif
static devclass_t nexus_devclass;

#ifndef FDT
static int
nexus_probe(device_t dev)
{

	device_set_desc(dev, "MIPS32 root nexus");

	irq_rman.rm_start = 0;
	irq_rman.rm_end = NUM_MIPS_IRQS - 1;
	irq_rman.rm_type = RMAN_ARRAY;
	irq_rman.rm_descr = "Hardware IRQs";
	if (rman_init(&irq_rman) != 0 ||
	    rman_manage_region(&irq_rman, 0, NUM_MIPS_IRQS - 1) != 0) {
		panic("%s: irq_rman", __func__);
	}

	mem_rman.rm_start = 0;
	mem_rman.rm_end = ~0ul;
	mem_rman.rm_type = RMAN_ARRAY;
	mem_rman.rm_descr = "Memory addresses";
	if (rman_init(&mem_rman) != 0 ||
	    rman_manage_region(&mem_rman, 0, ~0) != 0) {
		panic("%s: mem_rman", __func__);
	}

	return (0);
}

static int
nexus_attach(device_t dev)
{

	bus_generic_probe(dev);
	bus_enumerate_hinted_children(dev);
	bus_generic_attach(dev);

	return (0);
}

static int
nexus_print_child(device_t bus, device_t child)
{
	int retval = 0;

	retval += bus_print_child_header(bus, child);
	retval += nexus_print_all_resources(child);
	if (device_get_flags(child))
		retval += printf(" flags %#x", device_get_flags(child));
	retval += printf(" on %s\n", device_get_nameunit(bus));

	return (retval);
}

static int
nexus_print_all_resources(device_t dev)
{
	struct nexus_device *ndev = DEVTONX(dev);
	struct resource_list *rl = &ndev->nx_resources;
	int retval = 0;

	if (STAILQ_FIRST(rl))
		retval += printf(" at");

	retval += resource_list_print_type(rl, "mem", SYS_RES_MEMORY, "%#lx");
	retval += resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%ld");

	return (retval);
}

static device_t
nexus_add_child(device_t bus, u_int order, const char *name, int unit)
{
	device_t	child;
	struct nexus_device *ndev;

	ndev = malloc(sizeof(struct nexus_device), M_NEXUSDEV, M_NOWAIT|M_ZERO);
	if (!ndev)
		return (0);
	resource_list_init(&ndev->nx_resources);

	child = device_add_child_ordered(bus, order, name, unit);
	if (child == NULL) {
		device_printf(bus, "failed to add child: %s%d\n", name, unit);
		return (0);
	}

	/* should we free this in nexus_child_detached? */
	device_set_ivars(child, ndev);

	return (child);
}

/*
 * Allocate a resource on behalf of child.  NB: child is usually going to be a
 * child of one of our descendants, not a direct child of nexus0.
 * (Exceptions include footbridge.)
 */
static struct resource *
nexus_alloc_resource(device_t bus, device_t child, int type, int *rid,
	u_long start, u_long end, u_long count, u_int flags)
{
	struct nexus_device		*ndev = DEVTONX(child);
	struct resource			*rv;
	struct resource_list_entry	*rle;
	struct rman			*rm;
	int				 isdefault, needactivate, passthrough;

	dprintf("%s: entry (%p, %p, %d, %p, %p, %p, %ld, %d)\n",
	    __func__, bus, child, type, rid, (void *)(intptr_t)start,
	    (void *)(intptr_t)end, count, flags);
	dprintf("%s: requested rid is %d\n", __func__, *rid);

	isdefault = (start == 0UL && end == ~0UL && count == 1);
	needactivate = flags & RF_ACTIVE;
	passthrough = (device_get_parent(child) != bus);
	rle = NULL;

	/*
	 * If this is an allocation of the "default" range for a given RID,
	 * and we know what the resources for this device are (ie. they aren't
	 * maintained by a child bus), then work out the start/end values.
	 */
	if (isdefault) {
		rle = resource_list_find(&ndev->nx_resources, type, *rid);
		if (rle == NULL)
			return (NULL);
		if (rle->res != NULL) {
			panic("%s: resource entry is busy", __func__);
		}
		start = rle->start;
		end = rle->end;
		count = rle->count;
	}

	switch (type) {
	case SYS_RES_IRQ:
		rm = &irq_rman;
		break;
	case SYS_RES_MEMORY:
		rm = &mem_rman;
		break;
	default:
		printf("%s: unknown resource type %d\n", __func__, type);
		return (0);
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == 0) {
		printf("%s: could not reserve resource for %s\n", __func__,
		    device_get_nameunit(child));
		return (0);
	}

	rman_set_rid(rv, *rid);

	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			printf("%s: could not activate resource\n", __func__);
			rman_release_resource(rv);
			return (0);
		}
	}

	return (rv);
}

static struct resource_list *
nexus_get_reslist(device_t dev, device_t child)
{
	struct nexus_device *ndev = DEVTONX(child);

	return (&ndev->nx_resources);
}

static int
nexus_set_resource(device_t dev, device_t child, int type, int rid,
    u_long start, u_long count)
{
	struct nexus_device		*ndev = DEVTONX(child);
	struct resource_list		*rl = &ndev->nx_resources;
	struct resource_list_entry	*rle;

	dprintf("%s: entry (%p, %p, %d, %d, %p, %ld)\n",
	    __func__, dev, child, type, rid, (void *)(intptr_t)start, count);

	rle = resource_list_add(rl, type, rid, start, start + count - 1,
	    count);
	if (rle == NULL)
		return (ENXIO);

	return (0);
}

static int
nexus_get_resource(device_t dev, device_t child, int type, int rid,
    u_long *startp, u_long *countp)
{
	struct nexus_device		*ndev = DEVTONX(child);
	struct resource_list		*rl = &ndev->nx_resources;
	struct resource_list_entry	*rle;

	rle = resource_list_find(rl, type, rid);
	if (!rle)
		return(ENOENT);
	if (startp)
		*startp = rle->start;
	if (countp)
		*countp = rle->count;
	return (0);
}

static void
nexus_delete_resource(device_t dev, device_t child, int type, int rid)
{
	struct nexus_device	*ndev = DEVTONX(child);
	struct resource_list	*rl = &ndev->nx_resources;

	dprintf("%s: entry\n", __func__);

	resource_list_delete(rl, type, rid);
}

static int
nexus_release_resource(device_t bus, device_t child, int type, int rid,
		       struct resource *r)
{
	dprintf("%s: entry\n", __func__);

	if (rman_get_flags(r) & RF_ACTIVE) {
		int error = bus_deactivate_resource(child, type, rid, r);
		if (error)
			return error;
	}

	return (rman_release_resource(r));
}
#endif

static int
nexus_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	void *vaddr;
	vm_paddr_t paddr;
	vm_size_t psize;

	/*
	 * If this is a memory resource, use pmap_mapdev to map it.
	 */
	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		paddr = rman_get_start(r);
		psize = rman_get_size(r);
		vaddr = pmap_mapdev(paddr, psize);

		rman_set_virtual(r, vaddr);
		rman_set_bustag(r, mips_bus_space_generic);
		rman_set_bushandle(r, (bus_space_handle_t)(uintptr_t)vaddr);
	}

	return (rman_activate_resource(r));
}

static int
nexus_deactivate_resource(device_t bus, device_t child, int type, int rid,
			  struct resource *r)
{
	vm_offset_t va;
	
	if (type == SYS_RES_MEMORY) {
		va = (vm_offset_t)rman_get_virtual(r);
		pmap_unmapdev(va, rman_get_size(r));
	}

	return (rman_deactivate_resource(r));
}

static int
nexus_setup_intr(device_t dev, device_t child, struct resource *res, int flags,
    driver_filter_t *filt, driver_intr_t *intr, void *arg, void **cookiep)
{
	register_t s;
	int irq;

	s = intr_disable();
	irq = rman_get_start(res);
	if (irq >= NUM_MIPS_IRQS) {
		intr_restore(s);
		return (0);
	}

	cpu_establish_hardintr(device_get_nameunit(child), filt, intr, arg,
	    irq, flags, cookiep);
	intr_restore(s);
	return (0);
}

static int
nexus_teardown_intr(device_t dev, device_t child, struct resource *r, void *ih)
{

	printf("Unimplemented %s at %s:%d\n", __func__, __FILE__, __LINE__);
	return (0);
}

static void
nexus_hinted_child(device_t bus, const char *dname, int dunit)
{
	device_t child;
	long	maddr;
	int	msize;
	int	order;
	int	result;
	int	irq;
	int	mem_hints_count;

	if ((resource_int_value(dname, dunit, "order", &order)) != 0)
		order = 1000;
	child = BUS_ADD_CHILD(bus, order, dname, dunit);
	if (child == NULL)
		return;

	/*
	 * Set hard-wired resources for hinted child using
	 * specific RIDs.
	 */
	mem_hints_count = 0;
	if (resource_long_value(dname, dunit, "maddr", &maddr) == 0)
		mem_hints_count++;
	if (resource_int_value(dname, dunit, "msize", &msize) == 0)
		mem_hints_count++;

	/* check if all info for mem resource has been provided */
	if ((mem_hints_count > 0) && (mem_hints_count < 2)) {
		printf("Either maddr or msize hint is missing for %s%d\n",
		    dname, dunit);
	} 
	else if (mem_hints_count) {
		dprintf("%s: discovered hinted child %s at maddr %p(%d)\n",
		    __func__, device_get_nameunit(child),
		    (void *)(intptr_t)maddr, msize);

		result = bus_set_resource(child, SYS_RES_MEMORY, 0, maddr, 
		    msize);
		if (result != 0) {
			device_printf(bus, 
			    "warning: bus_set_resource() failed\n");
		}
	}

	if (resource_int_value(dname, dunit, "irq", &irq) == 0) {
		result = bus_set_resource(child, SYS_RES_IRQ, 0, irq, 1);
		if (result != 0)
			device_printf(bus,
			    "warning: bus_set_resource() failed\n");
	}
}

DRIVER_MODULE(nexus, root, nexus_driver, nexus_devclass, 0, 0);
