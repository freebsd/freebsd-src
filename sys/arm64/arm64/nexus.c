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

#include "opt_acpi.h"
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/machdep.h>
#include <machine/pcb.h>
#include <machine/resource.h>
#include <machine/vmparam.h>

#ifdef FDT
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/openfirm.h>
#include "ofw_bus_if.h"
#endif
#ifdef DEV_ACPI
#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>
#include "acpi_bus_if.h"
#endif

extern struct bus_space memmap_bus;

static MALLOC_DEFINE(M_NEXUSDEV, "nexusdev", "Nexus device");

struct nexus_device {
	struct resource_list	nx_resources;
};

static int force_np;
SYSCTL_INT(_kern, OID_AUTO, force_nonposted, CTLFLAG_RDTUN, &force_np, 0,
    "Force all devices to use non-posted device memory");

#define DEVTONX(dev)	((struct nexus_device *)device_get_ivars(dev))

static struct rman mem_rman;
static struct rman irq_rman;

static	int nexus_attach(device_t);

#ifdef FDT
static device_probe_t	nexus_fdt_probe;
static device_attach_t	nexus_fdt_attach;
static bus_activate_resource_t nexus_fdt_activate_resource;
#endif
#ifdef DEV_ACPI
static device_probe_t		nexus_acpi_probe;
static device_attach_t		nexus_acpi_attach;
#endif

static bus_add_child_t		nexus_add_child;
static bus_print_child_t	nexus_print_child;

static bus_activate_resource_t	nexus_activate_resource;
static bus_alloc_resource_t	nexus_alloc_resource;
static bus_deactivate_resource_t nexus_deactivate_resource;
static bus_get_resource_list_t	nexus_get_reslist;
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

#ifdef FDT
static ofw_bus_map_intr_t	nexus_ofw_map_intr;
#endif

static device_method_t nexus_methods[] = {
	/* Bus interface */
	DEVMETHOD(bus_add_child,	nexus_add_child),
	DEVMETHOD(bus_print_child,	nexus_print_child),
	DEVMETHOD(bus_activate_resource, nexus_activate_resource),
	DEVMETHOD(bus_adjust_resource,	bus_generic_rman_adjust_resource),
	DEVMETHOD(bus_alloc_resource,	nexus_alloc_resource),
	DEVMETHOD(bus_deactivate_resource, nexus_deactivate_resource),
	DEVMETHOD(bus_delete_resource, bus_generic_rl_delete_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	DEVMETHOD(bus_get_resource_list, nexus_get_reslist),
	DEVMETHOD(bus_get_rman,		nexus_get_rman),
	DEVMETHOD(bus_map_resource,	nexus_map_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_rman_release_resource),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_unmap_resource,	nexus_unmap_resource),
#ifdef SMP
	DEVMETHOD(bus_bind_intr,	nexus_bind_intr),
#endif
	DEVMETHOD(bus_config_intr,	nexus_config_intr),
	DEVMETHOD(bus_describe_intr,	nexus_describe_intr),
	DEVMETHOD(bus_setup_intr,	nexus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	nexus_teardown_intr),
	DEVMETHOD(bus_get_bus_tag,	nexus_get_bus_tag),

	DEVMETHOD_END
};

static driver_t nexus_driver = {
	"nexus",
	nexus_methods,
	1			/* no softc */
};

static int
nexus_attach(device_t dev)
{

	mem_rman.rm_start = 0;
	mem_rman.rm_end = BUS_SPACE_MAXADDR;
	mem_rman.rm_type = RMAN_ARRAY;
	mem_rman.rm_descr = "I/O memory addresses";
	if (rman_init(&mem_rman) ||
	    rman_manage_region(&mem_rman, 0, BUS_SPACE_MAXADDR))
		panic("nexus_attach mem_rman");
	irq_rman.rm_start = 0;
	irq_rman.rm_end = ~0;
	irq_rman.rm_type = RMAN_ARRAY;
	irq_rman.rm_descr = "Interrupts";
	if (rman_init(&irq_rman) || rman_manage_region(&irq_rman, 0, ~0))
		panic("nexus_attach irq_rman");

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

	/* should we free this in nexus_child_detached? */
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

/*
 * Allocate a resource on behalf of child.  NB: child is usually going to be a
 * child of one of our descendants, not a direct child of nexus0.
 */
static struct resource *
nexus_alloc_resource(device_t bus, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct nexus_device *ndev = DEVTONX(child);
	struct resource_list_entry *rle;

	/*
	 * If this is an allocation of the "default" range for a given
	 * RID, and we know what the resources for this device are
	 * (ie. they aren't maintained by a child bus), then work out
	 * the start/end values.
	 */
	if (RMAN_IS_DEFAULT_RANGE(start, end) && (count == 1)) {
		if (device_get_parent(child) != bus || ndev == NULL)
			return (NULL);
		rle = resource_list_find(&ndev->nx_resources, type, *rid);
		if (rle == NULL)
			return (NULL);
		start = rle->start;
		end = rle->end;
		count = rle->count;
	}

	return (bus_generic_rman_alloc_resource(bus, child, type, rid, start,
	    end, count, flags));
}

static int
nexus_config_intr(device_t dev, int irq, enum intr_trigger trig,
    enum intr_polarity pol)
{

	/*
	 * On arm64 (due to INTRNG), ACPI interrupt configuration is 
	 * done in nexus_acpi_map_intr().
	 */
	return (0);
}

static int
nexus_setup_intr(device_t dev, device_t child, struct resource *res, int flags,
    driver_filter_t *filt, driver_intr_t *intr, void *arg, void **cookiep)
{
	int error;

	if ((rman_get_flags(res) & RF_SHAREABLE) == 0)
		flags |= INTR_EXCL;

	/* We depend here on rman_activate_resource() being idempotent. */
	error = rman_activate_resource(res);
	if (error)
		return (error);

	error = intr_setup_irq(child, res, filt, intr, arg, flags, cookiep);

	return (error);
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

static bus_space_tag_t
nexus_get_bus_tag(device_t bus __unused, device_t child __unused)
{

	return (&memmap_bus);
}

static int
nexus_activate_resource_flags(device_t bus, device_t child, int type, int rid,
    struct resource *r, int flags)
{
	struct resource_map_request args;
	struct resource_map map;
	int err, use_np;

	if ((err = rman_activate_resource(r)) != 0)
		return (err);

	/*
	 * If this is a memory resource, map it into the kernel.
	 */
	switch (type) {
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		if ((rman_get_flags(r) & RF_UNMAPPED) == 0) {
			resource_init_map_request(&args);
			use_np = (flags & BUS_SPACE_MAP_NONPOSTED) != 0 ||
			    force_np;
			if (!use_np)
				resource_int_value(device_get_name(child),
				    device_get_unit(child), "force_nonposted",
				    &use_np);
			if (use_np)
				args.memattr = VM_MEMATTR_DEVICE_NP;
			err = nexus_map_resource(bus, child, type, r, &args,
			    &map);
			if (err != 0) {
				rman_deactivate_resource(r);
				return (err);
			}

			rman_set_mapping(r, &map);
		}
		break;
	case SYS_RES_IRQ:
		err = intr_activate_irq(child, r);
		if (err != 0) {
			rman_deactivate_resource(r);
			return (err);
		}
	}
	return (0);
}

static int
nexus_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	return (nexus_activate_resource_flags(dev, child, type, rid, r, 0));
}

static struct resource_list *
nexus_get_reslist(device_t dev, device_t child)
{
	struct nexus_device *ndev = DEVTONX(child);

	return (&ndev->nx_resources);
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

static int
nexus_map_resource(device_t bus, device_t child, int type, struct resource *r,
    struct resource_map_request *argsp, struct resource_map *map)
{
	struct resource_map_request args;
	rman_res_t length, start;
	int error;

	/* Resources must be active to be mapped. */
	if ((rman_get_flags(r) & RF_ACTIVE) == 0)
		return (ENXIO);

	/* Mappings are only supported on I/O and memory resources. */
	switch (type) {
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		break;
	default:
		return (EINVAL);
	}

	resource_init_map_request(&args);
	error = resource_validate_map_request(r, argsp, &args, &start, &length);
	if (error)
		return (error);

	map->r_vaddr = pmap_mapdev_attr(start, length, args.memattr);
	map->r_bustag = &memmap_bus;
	map->r_size = length;

	/*
	 * The handle is the virtual address.
	 */
	map->r_bushandle = (bus_space_handle_t)map->r_vaddr;
	return (0);
}

static int
nexus_unmap_resource(device_t bus, device_t child, int type, struct resource *r,
    struct resource_map *map)
{

	switch (type) {
	case SYS_RES_MEMORY:
	case SYS_RES_IOPORT:
		pmap_unmapdev(map->r_vaddr, map->r_size);
		return (0);
	default:
		return (EINVAL);
	}
}

#ifdef FDT
static device_method_t nexus_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nexus_fdt_probe),
	DEVMETHOD(device_attach,	nexus_fdt_attach),

	/* Bus interface */
	DEVMETHOD(bus_activate_resource,	nexus_fdt_activate_resource),

	/* OFW interface */
	DEVMETHOD(ofw_bus_map_intr,	nexus_ofw_map_intr),

	DEVMETHOD_END,
};

#define nexus_baseclasses nexus_fdt_baseclasses
DEFINE_CLASS_1(nexus, nexus_fdt_driver, nexus_fdt_methods, 1, nexus_driver);
#undef nexus_baseclasses

EARLY_DRIVER_MODULE(nexus_fdt, root, nexus_fdt_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_FIRST);

static int
nexus_fdt_probe(device_t dev)
{

	if (arm64_bus_method != ARM64_BUS_FDT)
		return (ENXIO);

	device_quiet(dev);
	return (BUS_PROBE_DEFAULT);
}

static int
nexus_fdt_attach(device_t dev)
{

	nexus_add_child(dev, 10, "ofwbus", 0);
	return (nexus_attach(dev));
}

static int
nexus_fdt_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	phandle_t node, parent;
	int flags;

	flags = 0;
	switch (type) {
	case SYS_RES_MEMORY:
	case SYS_RES_IOPORT:
		/*
		 * If the fdt parent has the nonposted-mmio property we
		 * need to use non-posted IO to access the device. When
		 * we find this property set the BUS_SPACE_MAP_NONPOSTED
		 * flag to be passed to bus_space_map.
		 */
		node = ofw_bus_get_node(child);
		if (node != -1) {
			parent = OF_parent(node);
			if (parent != 0 &&
			    OF_hasprop(parent, "nonposted-mmio")) {
				flags |= BUS_SPACE_MAP_NONPOSTED;
			}
		}
		break;
	default:
		break;
	}

	return (nexus_activate_resource_flags(bus, child, type, rid, r, flags));
}

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
#endif

#ifdef DEV_ACPI
static int nexus_acpi_map_intr(device_t dev, device_t child, u_int irq, int trig, int pol);

static device_method_t nexus_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nexus_acpi_probe),
	DEVMETHOD(device_attach,	nexus_acpi_attach),

	/* ACPI interface */
	DEVMETHOD(acpi_bus_map_intr,	nexus_acpi_map_intr),

	DEVMETHOD_END,
};

#define nexus_baseclasses nexus_acpi_baseclasses
DEFINE_CLASS_1(nexus, nexus_acpi_driver, nexus_acpi_methods, 1,
    nexus_driver);
#undef nexus_baseclasses

EARLY_DRIVER_MODULE(nexus_acpi, root, nexus_acpi_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_FIRST);

static int
nexus_acpi_probe(device_t dev)
{

	if (arm64_bus_method != ARM64_BUS_ACPI || acpi_identify() != 0)
		return (ENXIO);

	device_quiet(dev);
	return (BUS_PROBE_LOW_PRIORITY);
}

static int
nexus_acpi_attach(device_t dev)
{

	nexus_add_child(dev, 10, "acpi", 0);
	return (nexus_attach(dev));
}

static int
nexus_acpi_map_intr(device_t dev, device_t child, u_int irq, int trig, int pol)
{
	struct intr_map_data_acpi *acpi_data;
	size_t len;

	len = sizeof(*acpi_data);
	acpi_data = (struct intr_map_data_acpi *)intr_alloc_map_data(
	    INTR_MAP_DATA_ACPI, len, M_WAITOK | M_ZERO);
	acpi_data->irq = irq;
	acpi_data->pol = pol;
	acpi_data->trig = trig;

	/*
	 * TODO: This will only handle a single interrupt controller.
	 * ACPI will map multiple controllers into a single virtual IRQ
	 * space. Each controller has a System Vector Base to hold the
	 * first irq it handles in this space. As such the correct way
	 * to handle interrupts with ACPI is to search through the
	 * controllers for the largest base value that is no larger than
	 * the IRQ value.
	 */
	irq = intr_map_irq(NULL, ACPI_INTR_XREF,
	    (struct intr_map_data *)acpi_data);
	return (irq);
}
#endif
