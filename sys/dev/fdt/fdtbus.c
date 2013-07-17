/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>

#include "fdt_common.h"
#include "ofw_bus_if.h"

#ifdef DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__);	\
    printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

static MALLOC_DEFINE(M_FDTBUS, "fdtbus", "FDTbus devices information");

struct fdtbus_devinfo {
	phandle_t		di_node;
	char			*di_name;
	char			*di_type;
	char			*di_compat;
	struct resource_list	di_res;

	/* Interrupts sense-level info for this device */
	struct fdt_sense_level	di_intr_sl[DI_MAX_INTR_NUM];
};

struct fdtbus_softc {
	struct rman	sc_irq;
	struct rman	sc_mem;
};

/*
 * Prototypes.
 */
static void fdtbus_identify(driver_t *, device_t);
static int fdtbus_probe(device_t);
static int fdtbus_attach(device_t);

static int fdtbus_print_child(device_t, device_t);
static struct resource *fdtbus_alloc_resource(device_t, device_t, int,
    int *, u_long, u_long, u_long, u_int);
static int fdtbus_release_resource(device_t, device_t, int, int,
    struct resource *);
static int fdtbus_activate_resource(device_t, device_t, int, int,
    struct resource *);
static int fdtbus_deactivate_resource(device_t, device_t, int, int,
    struct resource *);
static int fdtbus_setup_intr(device_t, device_t, struct resource *, int,
    driver_filter_t *, driver_intr_t *, void *, void **);

static const char *fdtbus_ofw_get_name(device_t, device_t);
static phandle_t fdtbus_ofw_get_node(device_t, device_t);
static const char *fdtbus_ofw_get_type(device_t, device_t);
static const char *fdtbus_ofw_get_compat(device_t, device_t);

/*
 * Local routines.
 */
static void newbus_device_from_fdt_node(device_t, phandle_t);

/*
 * Bus interface definition.
 */
static device_method_t fdtbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	fdtbus_identify),
	DEVMETHOD(device_probe,		fdtbus_probe),
	DEVMETHOD(device_attach,	fdtbus_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	fdtbus_print_child),
	DEVMETHOD(bus_alloc_resource,	fdtbus_alloc_resource),
	DEVMETHOD(bus_release_resource,	fdtbus_release_resource),
	DEVMETHOD(bus_activate_resource, fdtbus_activate_resource),
	DEVMETHOD(bus_deactivate_resource, fdtbus_deactivate_resource),
	DEVMETHOD(bus_config_intr,	bus_generic_config_intr),
	DEVMETHOD(bus_setup_intr,	fdtbus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* OFW bus interface */
	DEVMETHOD(ofw_bus_get_node,	fdtbus_ofw_get_node),
	DEVMETHOD(ofw_bus_get_name,	fdtbus_ofw_get_name),
	DEVMETHOD(ofw_bus_get_type,	fdtbus_ofw_get_type),
	DEVMETHOD(ofw_bus_get_compat,	fdtbus_ofw_get_compat),

	{ 0, 0 }
};

static driver_t fdtbus_driver = {
	"fdtbus",
	fdtbus_methods,
	sizeof(struct fdtbus_softc)
};

devclass_t fdtbus_devclass;

DRIVER_MODULE(fdtbus, nexus, fdtbus_driver, fdtbus_devclass, 0, 0);

static void
fdtbus_identify(driver_t *driver, device_t parent)
{

	debugf("%s(driver=%p, parent=%p)\n", __func__, driver, parent);

	if (device_find_child(parent, "fdtbus", -1) == NULL)
		BUS_ADD_CHILD(parent, 0, "fdtbus", -1);
}

static int
fdtbus_probe(device_t dev)
{

	debugf("%s(dev=%p); pass=%u\n", __func__, dev, bus_current_pass);

	device_set_desc(dev, "FDT main bus");
	if (!bootverbose)
		device_quiet(dev);
	return (BUS_PROBE_DEFAULT);
}

static int
fdtbus_attach(device_t dev)
{
	phandle_t root;
	phandle_t child;
	struct fdtbus_softc *sc;
	u_long start, end;
	int error;

	if ((root = OF_finddevice("/")) == -1)
		panic("fdtbus_attach: no root node.");

	sc = device_get_softc(dev);

	/*
	 * IRQ rman.
	 */
	start = 0;
	end = FDT_INTR_MAX - 1;
	sc->sc_irq.rm_start = start;
	sc->sc_irq.rm_end = end;
	sc->sc_irq.rm_type = RMAN_ARRAY;
	sc->sc_irq.rm_descr = "Interrupt request lines";
	if ((error = rman_init(&sc->sc_irq)) != 0) {
		device_printf(dev, "could not init IRQ rman, error = %d\n",
		    error);
		return (error);
	}
	if ((error = rman_manage_region(&sc->sc_irq, start, end)) != 0) {
		device_printf(dev, "could not manage IRQ region, error = %d\n",
		    error);
		return (error);
	}

	/*
	 * Mem-mapped I/O space rman.
	 */
	start = 0;
	end = ~0ul;
	sc->sc_mem.rm_start = start;
	sc->sc_mem.rm_end = end;
	sc->sc_mem.rm_type = RMAN_ARRAY;
	sc->sc_mem.rm_descr = "I/O memory";
	if ((error = rman_init(&sc->sc_mem)) != 0) {
		device_printf(dev, "could not init I/O mem rman, error = %d\n",
		    error);
		return (error);
	}
	if ((error = rman_manage_region(&sc->sc_mem, start, end)) != 0) {
		device_printf(dev, "could not manage I/O mem region, "
		    "error = %d\n", error);
		return (error);
	}

	/*
	 * Walk the FDT root node and add top-level devices as our children.
	 */
	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		/* Check and process 'status' property. */
		if (!(fdt_is_enabled(child)))
			continue;

		newbus_device_from_fdt_node(dev, child);
	}

	return (bus_generic_attach(dev));
}

static int
fdtbus_print_child(device_t dev, device_t child)
{
	struct fdtbus_devinfo *di;
	struct resource_list *rl;
	int rv;

	di = device_get_ivars(child);
	rl = &di->di_res;

	rv = 0;
	rv += bus_print_child_header(dev, child);
	rv += resource_list_print_type(rl, "mem", SYS_RES_MEMORY, "%#lx");
	rv += resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%ld");
	rv += bus_print_child_footer(dev, child);

	return (rv);
}

static void
newbus_device_destroy(device_t dev)
{
	struct fdtbus_devinfo *di;

	di = device_get_ivars(dev);
	if (di == NULL)
		return;

	free(di->di_name, M_OFWPROP);
	free(di->di_type, M_OFWPROP);
	free(di->di_compat, M_OFWPROP);

	resource_list_free(&di->di_res);
	free(di, M_FDTBUS);
}

static device_t
newbus_device_create(device_t dev_par, phandle_t node, char *name, char *type,
    char *compat)
{
	device_t child;
	struct fdtbus_devinfo *di;

	child = device_add_child(dev_par, NULL, -1);
	if (child == NULL) {
		free(name, M_OFWPROP);
		free(type, M_OFWPROP);
		free(compat, M_OFWPROP);
		return (NULL);
	}

	di = malloc(sizeof(*di), M_FDTBUS, M_WAITOK);
	di->di_node = node;
	di->di_name = name;
	di->di_type = type;
	di->di_compat = compat;

	resource_list_init(&di->di_res);

	if (fdt_reg_to_rl(node, &di->di_res)) {
		device_printf(child, "could not process 'reg' property\n");
		newbus_device_destroy(child);
		child = NULL;
		goto out;
	}

	if (fdt_intr_to_rl(node, &di->di_res, di->di_intr_sl)) {
		device_printf(child, "could not process 'interrupts' "
		    "property\n");
		newbus_device_destroy(child);
		child = NULL;
		goto out;
	}

	device_set_ivars(child, di);
	debugf("added child name='%s', node=%p\n", name, (void *)node);

out:
	return (child);
}

static device_t
newbus_pci_create(device_t dev_par, phandle_t dt_node, u_long par_base,
    u_long par_size)
{
	pcell_t reg[3 + 2];
	device_t dev_child;
	u_long start, end, count;
	struct fdtbus_devinfo *di;
	char *name, *type, *compat;
	int len;

	OF_getprop_alloc(dt_node, "device_type", 1, (void **)&type);
	if (!(type != NULL && strcmp(type, "pci") == 0)) {
		/* Only process 'pci' subnodes. */
		free(type, M_OFWPROP);
		return (NULL);
	}

	OF_getprop_alloc(dt_node, "name", 1, (void **)&name);
	OF_getprop_alloc(OF_parent(dt_node), "compatible", 1,
	    (void **)&compat);

	dev_child = device_add_child(dev_par, NULL, -1);
	if (dev_child == NULL) {
		free(name, M_OFWPROP);
		free(type, M_OFWPROP);
		free(compat, M_OFWPROP);
		return (NULL);
	}

	di = malloc(sizeof(*di), M_FDTBUS, M_WAITOK);
	di->di_node = dt_node;
	di->di_name = name;
	di->di_type = type;
	di->di_compat = compat;

	resource_list_init(&di->di_res);

	/*
	 * Produce and set SYS_RES_MEMORY resources.
	 */
	start = 0;
	count = 0;

	len = OF_getprop(dt_node, "reg", &reg, sizeof(reg));
	if (len > 0) {
		if (fdt_data_verify((void *)&reg[1], 2) != 0) {
			device_printf(dev_child, "'reg' address value out of "
			    "range\n");
			newbus_device_destroy(dev_child);
			dev_child = NULL;
			goto out;
		}
		start = fdt_data_get((void *)&reg[1], 2);

		if (fdt_data_verify((void *)&reg[3], 2) != 0) {
			device_printf(dev_child, "'reg' size value out of "
			    "range\n");
			newbus_device_destroy(dev_child);
			dev_child = NULL;
			goto out;
		}
		count = fdt_data_get((void *)&reg[3], 2);
	}

	/* Calculate address range relative to base. */
	par_base &= 0x000ffffful;
	start &= 0x000ffffful;
	start += par_base + fdt_immr_va;
	if (count == 0)
		count = par_size;
	end = start + count - 1;

	debugf("start = 0x%08lx, end = 0x%08lx, count = 0x%08lx\n",
	    start, end, count);

	if (count > par_size) {
		device_printf(dev_child, "'reg' size value out of range\n");
		newbus_device_destroy(dev_child);
		dev_child = NULL;
		goto out;
	}

	resource_list_add(&di->di_res, SYS_RES_MEMORY, 0, start, end, count);

	/*
	 * Set SYS_RES_IRQ resources.
	 */
	if (fdt_intr_to_rl(OF_parent(dt_node), &di->di_res, di->di_intr_sl)) {
		device_printf(dev_child, "could not process 'interrupts' "
		    "property\n");
		newbus_device_destroy(dev_child);
		dev_child = NULL;
		goto out;
	}

	device_set_ivars(dev_child, di);
	debugf("added child name='%s', node=%p\n", name,
	    (void *)dt_node);

out:
	return (dev_child);
}

static void 
pci_from_fdt_node(device_t dev_par, phandle_t dt_node, char *name,
    char *type, char *compat)
{
	u_long reg_base, reg_size;
	phandle_t dt_child;

	/*
	 * Retrieve 'reg' property.
	 */
	if (fdt_regsize(dt_node, &reg_base, &reg_size) != 0) {
		device_printf(dev_par, "could not retrieve 'reg' prop\n");
		return;
	}

	/*
	 * Walk the PCI node and instantiate newbus devices representing
	 * logical resources (bridges / ports).
	 */
	for (dt_child = OF_child(dt_node); dt_child != 0;
	    dt_child = OF_peer(dt_child)) {

		if (!(fdt_is_enabled(dt_child)))
			continue;

		newbus_pci_create(dev_par, dt_child, reg_base, reg_size);
	}
}

/*
 * These FDT nodes do not need a corresponding newbus device object.
 */
static char *fdt_devices_skip[] = {
	"aliases",
	"chosen",
	"memory",
	NULL
};

static void
newbus_device_from_fdt_node(device_t dev_par, phandle_t node)
{
	char *name, *type, *compat;
	device_t child;
	int i;

	OF_getprop_alloc(node, "name", 1, (void **)&name);
	OF_getprop_alloc(node, "device_type", 1, (void **)&type);
	OF_getprop_alloc(node, "compatible", 1, (void **)&compat);

	for (i = 0; fdt_devices_skip[i] != NULL; i++)
		if (name != NULL && strcmp(name, fdt_devices_skip[i]) == 0) {
			debugf("skipping instantiating FDT device='%s'\n",
			    name);
			return;
		}

	child = newbus_device_create(dev_par, node, name, type, compat);
	if (type != NULL && strcmp(type, "pci") == 0)
		pci_from_fdt_node(child, node, name, type, compat);
}

static struct resource *
fdtbus_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct fdtbus_softc *sc;
	struct resource *res;
	struct rman *rm;
	struct fdtbus_devinfo *di;
	struct resource_list_entry *rle;
	int needactivate;

	/*
	 * Request for the default allocation with a given rid: use resource
	 * list stored in the local device info.
	 */
	if ((start == 0UL) && (end == ~0UL)) {
		if ((di = device_get_ivars(child)) == NULL)
			return (NULL);

		if (type == SYS_RES_IOPORT)
			type = SYS_RES_MEMORY;

		rle = resource_list_find(&di->di_res, type, *rid);
		if (rle == NULL) {
			device_printf(bus, "no default resources for "
			    "rid = %d, type = %d\n", *rid, type);
			return (NULL);
		}
		start = rle->start;
		end = rle->end;
		count = rle->count;
	}

	sc = device_get_softc(bus);

	needactivate = flags & RF_ACTIVE;
	flags &= ~RF_ACTIVE;

	switch (type) {
	case SYS_RES_IRQ:
		rm = &sc->sc_irq;
		break;

	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		rm = &sc->sc_mem;
		break;

	default:
		return (NULL);
	}

	res = rman_reserve_resource(rm, start, end, count, flags, child);
	if (res == NULL) {
		device_printf(bus, "failed to reserve resource %#lx - %#lx "
		    "(%#lx)\n", start, end, count);
		return (NULL);
	}

	rman_set_rid(res, *rid);

	if (type == SYS_RES_IOPORT || type == SYS_RES_MEMORY) {
		/* XXX endianess should be set based on SOC node */
		rman_set_bustag(res, fdtbus_bs_tag);
		rman_set_bushandle(res, rman_get_start(res));
	}

	if (needactivate)
		if (bus_activate_resource(child, type, *rid, res)) {
			device_printf(child, "resource activation failed\n");
			rman_release_resource(res);
			return (NULL);
		}

	return (res);
}

static int
fdtbus_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{
	int err;

	if (rman_get_flags(res) & RF_ACTIVE) {
		err = bus_deactivate_resource(child, type, rid, res);
		if (err)
			return (err);
	}

	return (rman_release_resource(res));
}

static int
fdtbus_setup_intr(device_t bus, device_t child, struct resource *res,
    int flags, driver_filter_t *filter, driver_intr_t *ihand, void *arg,
    void **cookiep)
{
	struct fdtbus_devinfo *di;
	enum intr_trigger trig;
	enum intr_polarity pol;
	int error, rid;

	if (res == NULL)
		return (EINVAL);

	/*
	 * We are responsible for configuring the interrupts of our direct
	 * children.
	 */
	if (device_get_parent(child) == bus) {
		di = device_get_ivars(child);
		if (di == NULL)
			return (ENXIO);

		rid = rman_get_rid(res);
		if (rid >= DI_MAX_INTR_NUM)
			return (ENOENT);

		trig = di->di_intr_sl[rid].trig;
		pol = di->di_intr_sl[rid].pol;
		if (trig != INTR_TRIGGER_CONFORM ||
		    pol != INTR_POLARITY_CONFORM) {
			error = bus_generic_config_intr(bus,
			    rman_get_start(res), trig, pol);
			if (error)
				return (error);
		}
	}

	error = bus_generic_setup_intr(bus, child, res, flags, filter, ihand,
	    arg, cookiep);
	return (error);
}

static int
fdtbus_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{
	bus_space_handle_t p;
	int error;

	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		error = bus_space_map(rman_get_bustag(res),
		    rman_get_bushandle(res), rman_get_size(res), 0, &p);
		if (error)
			return (error);
		rman_set_bushandle(res, p);
	}

	return (rman_activate_resource(res));
}

static int
fdtbus_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{

	return (rman_deactivate_resource(res));
}

static const char *
fdtbus_ofw_get_name(device_t bus, device_t dev)
{
	struct fdtbus_devinfo *di;

	return ((di = device_get_ivars(dev)) == NULL ? NULL : di->di_name);
}

static phandle_t
fdtbus_ofw_get_node(device_t bus, device_t dev)
{
	struct fdtbus_devinfo *di;

	return ((di = device_get_ivars(dev)) == NULL ? 0 : di->di_node);
}

static const char *
fdtbus_ofw_get_type(device_t bus, device_t dev)
{
	struct fdtbus_devinfo *di;

	return ((di = device_get_ivars(dev)) == NULL ? NULL : di->di_type);
}

static const char *
fdtbus_ofw_get_compat(device_t bus, device_t dev)
{
	struct fdtbus_devinfo *di;

	return ((di = device_get_ivars(dev)) == NULL ? NULL : di->di_compat);
}


