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
 * This code implements a `root nexus' for Arm Architecture
 * machines.  The function of the root nexus is to serve as an
 * attachment point for both processors and buses, and to manage
 * resources which are common to all of them.  In particular,
 * this code implements the core resource managers for interrupt
 * requests and I/O memory address space.
 */

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/pcb.h>
#include <machine/intr.h>
#include <machine/resource.h>
#include <machine/vmparam.h>

#include <arm/arm/nexusvar.h>

#ifdef FDT
#include <machine/fdt.h>
#include <dev/ofw/ofw_bus_subr.h>
#include "ofw_bus_if.h"
#endif

static MALLOC_DEFINE(M_NEXUSDEV, "nexusdev", "Nexus device");

struct nexus_device {
	struct resource_list	nx_resources;
};

#define DEVTONX(dev)	((struct nexus_device *)device_get_ivars(dev))

static struct rman mem_rman;
static struct rman irq_rman;

static device_probe_t		nexus_probe;
static device_attach_t		nexus_attach;

static bus_add_child_t		nexus_add_child;
static bus_print_child_t	nexus_print_child;

static bus_activate_resource_t	nexus_activate_resource;
static bus_deactivate_resource_t nexus_deactivate_resource;
static bus_get_rman_t		nexus_get_rman;
static bus_map_resource_t	nexus_map_resource;
static bus_unmap_resource_t	nexus_unmap_resource;

#ifdef SMP
static bus_bind_intr_t		nexus_bind_intr;
#endif
static bus_config_intr_t	nexus_config_intr;
static bus_describe_intr_t	nexus_describe_intr;
static bus_setup_intr_t		nexus_setup_intr;
static bus_teardown_intr_t	nexus_teardown_intr;

static bus_get_bus_tag_t	nexus_get_bus_tag;
static bus_get_dma_tag_t	nexus_get_dma_tag;

#ifdef FDT
static ofw_bus_map_intr_t	nexus_ofw_map_intr;
#endif

/*
 * Normally NULL (which results in defaults which are handled in
 * busdma_machdep), platform init code can use nexus_set_dma_tag() to set this
 * to a tag that will be inherited by all busses and devices on the platform.
 */
static bus_dma_tag_t nexus_dma_tag;

static device_method_t nexus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nexus_probe),
	DEVMETHOD(device_attach,	nexus_attach),

	/* Bus interface */
	DEVMETHOD(bus_add_child,	nexus_add_child),
	DEVMETHOD(bus_print_child,	nexus_print_child),
	DEVMETHOD(bus_activate_resource, nexus_activate_resource),
	DEVMETHOD(bus_adjust_resource,	bus_generic_rman_adjust_resource),
	DEVMETHOD(bus_alloc_resource,	bus_generic_rman_alloc_resource),
	DEVMETHOD(bus_deactivate_resource, nexus_deactivate_resource),
	DEVMETHOD(bus_get_rman,		nexus_get_rman),
	DEVMETHOD(bus_map_resource,	nexus_map_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_rman_release_resource),
	DEVMETHOD(bus_unmap_resource,	nexus_unmap_resource),
#ifdef SMP
	DEVMETHOD(bus_bind_intr,	nexus_bind_intr),
#endif
	DEVMETHOD(bus_config_intr,	nexus_config_intr),
	DEVMETHOD(bus_describe_intr,	nexus_describe_intr),
	DEVMETHOD(bus_setup_intr,	nexus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	nexus_teardown_intr),
	DEVMETHOD(bus_get_bus_tag,	nexus_get_bus_tag),
	DEVMETHOD(bus_get_dma_tag,	nexus_get_dma_tag),
#ifdef FDT
	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_map_intr,	nexus_ofw_map_intr),
#endif
	DEVMETHOD_END
};

static driver_t nexus_driver = {
	"nexus",
	nexus_methods,
	1			/* no softc */
};

EARLY_DRIVER_MODULE(nexus, root, nexus_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_EARLY);

static int
nexus_probe(device_t dev)
{

	device_quiet(dev);	/* suppress attach message for neatness */

	return (BUS_PROBE_DEFAULT);
}

static int
nexus_attach(device_t dev)
{

	mem_rman.rm_start = 0;
	mem_rman.rm_end = BUS_SPACE_MAXADDR;
	mem_rman.rm_type = RMAN_ARRAY;
	mem_rman.rm_descr = "I/O memory addresses";
	if (rman_init(&mem_rman) ||
	    rman_manage_region(&mem_rman, 0, BUS_SPACE_MAXADDR))
		panic("nexus_probe mem_rman");
	irq_rman.rm_start = 0;
	irq_rman.rm_end = ~0;
	irq_rman.rm_type = RMAN_ARRAY;
	irq_rman.rm_descr = "Interrupts";
	if (rman_init(&irq_rman) || rman_manage_region(&irq_rman, 0, ~0))
		panic("nexus_attach irq_rman");

	/* First, add ofwbus0. */
	device_add_child(dev, "ofwbus", 0);

	/*
	 * Next, deal with the children we know about already.
	 */
	bus_generic_probe(dev);
	bus_generic_attach(dev);

	return (0);
}

static int
nexus_print_child(device_t bus, device_t child)
{
	int retval = 0;

	retval += bus_print_child_header(bus, child);
	retval += printf("\n");

	return (retval);
}

static device_t
nexus_add_child(device_t bus, u_int order, const char *name, int unit)
{
	device_t child;
	struct nexus_device *ndev;

	ndev = malloc(sizeof(struct nexus_device), M_NEXUSDEV, M_NOWAIT|M_ZERO);
	if (!ndev)
		return (0);
	resource_list_init(&ndev->nx_resources);

	child = device_add_child_ordered(bus, order, name, unit);

	device_set_ivars(child, ndev);

	return (child);
}

static struct rman *
nexus_get_rman(device_t bus, int type, u_int flags)
{
	switch (type) {
	case SYS_RES_IRQ:
		return (&irq_rman);
	case SYS_RES_MEMORY:
	case SYS_RES_IOPORT:
		return (&mem_rman);
	default:
		return (NULL);
	}
}

static bus_space_tag_t
nexus_get_bus_tag(device_t bus __unused, device_t child __unused)
{

#ifdef FDT
	return (fdtbus_bs_tag);
#else
	return ((void *)1);
#endif
}

static bus_dma_tag_t
nexus_get_dma_tag(device_t dev, device_t child)
{

	return (nexus_dma_tag);
}

void
nexus_set_dma_tag(bus_dma_tag_t tag)
{

	nexus_dma_tag = tag;
}

static int
nexus_config_intr(device_t dev, int irq, enum intr_trigger trig,
    enum intr_polarity pol)
{
	int ret = ENODEV;

	device_printf(dev, "bus_config_intr is obsolete and not supported!\n");
	ret = EOPNOTSUPP;
	return (ret);
}

static int
nexus_setup_intr(device_t dev, device_t child, struct resource *res, int flags,
    driver_filter_t *filt, driver_intr_t *intr, void *arg, void **cookiep)
{

	if ((rman_get_flags(res) & RF_SHAREABLE) == 0)
		flags |= INTR_EXCL;

	return (intr_setup_irq(child, res, filt, intr, arg, flags, cookiep));
}

static int
nexus_teardown_intr(device_t dev, device_t child, struct resource *r, void *ih)
{

	return (intr_teardown_irq(child, r, ih));
}

static int
nexus_describe_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie, const char *descr)
{

	return (intr_describe_irq(child, irq, cookie, descr));
}

#ifdef SMP
static int
nexus_bind_intr(device_t dev, device_t child, struct resource *irq, int cpu)
{

	return (intr_bind_irq(child, irq, cpu));
}
#endif

static int
nexus_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	int err;

	switch (type) {
	case SYS_RES_MEMORY:
	case SYS_RES_IOPORT:
		return (bus_generic_rman_activate_resource(bus, child, type,
		    rid, r));
	case SYS_RES_IRQ:
		err = rman_activate_resource(r);
		if (err != 0)
			return (err);
		err = intr_activate_irq(child, r);
		if (err != 0) {
			rman_deactivate_resource(r);
			return (err);
		}
		return (0);
	default:
		return (EINVAL);
	}
}

static int
nexus_map_resource(device_t bus, device_t child, int type, struct resource *r,
    struct resource_map_request *argsp, struct resource_map *map)
{
	struct resource_map_request args;
	rman_res_t length, start;
	int error;

	/* Resources must be active to be mapped. */
	if (!(rman_get_flags(r) & RF_ACTIVE))
		return (ENXIO);

	switch (type) {
	case SYS_RES_MEMORY:
	case SYS_RES_IOPORT:
		break;
	default:
		return (EINVAL);
	}

	resource_init_map_request(&args);
	error = resource_validate_map_request(r, argsp, &args, &start, &length);
	if (error)
		return (error);

#ifdef FDT
	error = bus_space_map(fdtbus_bs_tag, start, length, 0,
	    &map->r_bushandle);
	if (error)
		return (error);
	map->r_bustag = fdtbus_bs_tag;
	map->r_vaddr = (void *)map->r_bushandle;
#else
	map->r_vaddr = pmap_mapdev(start, length);
	if (map->r_vaddr == NULL)
		return (ENOMEM);
	map->r_bustag = (void *)1;
	map->r_bushandle = (bus_space_handle_t)map->r_vaddr;
#endif
	map->r_size = length;
	return (0);
}

static int
nexus_unmap_resource(device_t bus, device_t child, int type, struct resource *r,
    struct resource_map *map)
{

	switch (type) {
	case SYS_RES_MEMORY:
	case SYS_RES_IOPORT:
#ifdef FDT
		bus_space_unmap(map->r_bustag, map->r_bushandle, map->r_size);
#else
		pmap_unmapdev(map->r_vaddr, map->r_size);
#endif
		return (0);
	default:
		return (EINVAL);
	}
}

static int
nexus_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	int error;

	switch (type) {
	case SYS_RES_MEMORY:
	case SYS_RES_IOPORT:
		return (bus_generic_rman_deactivate_resource(bus, child, type,
		    rid, r));
	case SYS_RES_IRQ:
		error = rman_deactivate_resource(r);
		if (error)
			return (error);
		intr_deactivate_irq(child, r);
		return (0);
	default:
		return (EINVAL);
	}
}

#ifdef FDT
static int
nexus_ofw_map_intr(device_t dev, device_t child, phandle_t iparent, int icells,
    pcell_t *intr)
{
	u_int irq;
	struct intr_map_data_fdt *fdt_data;
	size_t len;

	len = sizeof(*fdt_data) + icells * sizeof(pcell_t);
	fdt_data = (struct intr_map_data_fdt *)intr_alloc_map_data(
	    INTR_MAP_DATA_FDT, len, M_WAITOK | M_ZERO);
	fdt_data->iparent = iparent;
	fdt_data->ncells = icells;
	memcpy(fdt_data->cells, intr, icells * sizeof(pcell_t));
	irq = intr_map_irq(NULL, iparent, (struct intr_map_data *)fdt_data);
	return (irq);
}
#endif /* FDT */
