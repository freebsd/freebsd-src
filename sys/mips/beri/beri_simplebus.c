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

#include "opt_platform.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include <dev/fdt/fdt_common.h>
#include "fdt_ic_if.h"
#include "ofw_bus_if.h"

#ifdef DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__);	\
    printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

static MALLOC_DEFINE(M_SIMPLEBUS, "simplebus", "simplebus devices information");

struct simplebus_softc {
	int	sc_addr_cells;
	int	sc_size_cells;
};

struct simplebus_devinfo {
	struct ofw_bus_devinfo	di_ofw;
	struct resource_list	di_res;

	/* Interrupts sense-level info for this device */
	struct fdt_sense_level	di_intr_sl[DI_MAX_INTR_NUM];
};

/*
 * Prototypes.
 */
static int simplebus_probe(device_t);
static int simplebus_attach(device_t);

static int simplebus_print_child(device_t, device_t);
static int simplebus_setup_intr(device_t, device_t, struct resource *, int,
    driver_filter_t *, driver_intr_t *, void *, void **);
static int simplebus_teardown_intr(device_t, device_t, struct resource *,
    void *);

static int simplebus_activate_resource(device_t, device_t, int, int,
    struct resource *);
static struct resource *simplebus_alloc_resource(device_t, device_t, int,
    int *, u_long, u_long, u_long, u_int);
static int simplebus_deactivate_resource(device_t, device_t, int, int,
    struct resource *);
static int simplebus_release_resource(device_t, device_t, int, int,
    struct resource *);
static device_t simplebus_get_interrupt_parent(device_t);
static struct resource_list *simplebus_get_resource_list(device_t, device_t);

static ofw_bus_get_devinfo_t simplebus_get_devinfo;

/*
 * Bus interface definition.
 */
static device_method_t simplebus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		simplebus_probe),
	DEVMETHOD(device_attach,	simplebus_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	simplebus_print_child),
	DEVMETHOD(bus_alloc_resource,	simplebus_alloc_resource),
	DEVMETHOD(bus_release_resource,	simplebus_release_resource),
	DEVMETHOD(bus_activate_resource, simplebus_activate_resource),
	DEVMETHOD(bus_deactivate_resource, simplebus_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	simplebus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	simplebus_teardown_intr),
	DEVMETHOD(bus_get_resource_list, simplebus_get_resource_list),

	/* OFW bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	simplebus_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	{ 0, 0 }
};

static driver_t simplebus_driver = {
	"simplebus",
	simplebus_methods,
	sizeof(struct simplebus_softc)
};

devclass_t simplebus_devclass;

DRIVER_MODULE(beri_simplebus, ofwbus, simplebus_driver, simplebus_devclass, 0,
    0);
DRIVER_MODULE(beri_simplebus, simplebus, simplebus_driver, simplebus_devclass,
    0, 0);

static int
simplebus_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "simple-bus"))
		return (ENXIO);

	device_set_desc(dev, "Flattened device tree simple bus (BERI version)");

	return (BUS_PROBE_SPECIFIC);
}

static int
simplebus_attach(device_t dev)
{
	device_t dev_child;
	struct simplebus_devinfo *di;
	struct simplebus_softc *sc;
	phandle_t dt_node, dt_child;

	sc = device_get_softc(dev);

	/*
	 * Walk simple-bus and add direct subordinates as our children.
	 */
	dt_node = ofw_bus_get_node(dev);
	for (dt_child = OF_child(dt_node); dt_child != 0;
	    dt_child = OF_peer(dt_child)) {

		/* Check and process 'status' property. */
		if (!(fdt_is_enabled(dt_child)))
			continue;

		if (!(fdt_pm_is_enabled(dt_child)))
			continue;

		di = malloc(sizeof(*di), M_SIMPLEBUS, M_WAITOK | M_ZERO);

		if (ofw_bus_gen_setup_devinfo(&di->di_ofw, dt_child) != 0) {
			free(di, M_SIMPLEBUS);
			device_printf(dev, "could not set up devinfo\n");
			continue;
		}

		resource_list_init(&di->di_res);
		if (fdt_reg_to_rl(dt_child, &di->di_res)) {
			device_printf(dev,
			    "%s: could not process 'reg' "
			    "property\n", di->di_ofw.obd_name);
			ofw_bus_gen_destroy_devinfo(&di->di_ofw);
			free(di, M_SIMPLEBUS);
			continue;
		}

		if (fdt_intr_to_rl(dev, dt_child, &di->di_res, di->di_intr_sl)) {
			device_printf(dev, "%s: could not process "
			    "'interrupts' property\n", di->di_ofw.obd_name);
			resource_list_free(&di->di_res);
			ofw_bus_gen_destroy_devinfo(&di->di_ofw);
			free(di, M_SIMPLEBUS);
			continue;
		}

		/* Add newbus device for this FDT node */
		dev_child = device_add_child(dev, NULL, -1);
		if (dev_child == NULL) {
			device_printf(dev, "could not add child: %s\n",
			    di->di_ofw.obd_name);
			resource_list_free(&di->di_res);
			ofw_bus_gen_destroy_devinfo(&di->di_ofw);
			free(di, M_SIMPLEBUS);
			continue;
		}
#ifdef DEBUG
		device_printf(dev, "added child: %s\n\n", di->di_ofw.obd_name);
#endif
		device_set_ivars(dev_child, di);
	}

	return (bus_generic_attach(dev));
}

static int
simplebus_print_child(device_t dev, device_t child)
{
	device_t ip;
	struct simplebus_devinfo *di;
	struct resource_list *rl;
	int rv;

	di = device_get_ivars(child);
	rl = &di->di_res;

	rv = 0;
	rv += bus_print_child_header(dev, child);
	rv += resource_list_print_type(rl, "mem", SYS_RES_MEMORY, "%#lx");
	rv += resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%ld");
	if ((ip = simplebus_get_interrupt_parent(child)) != NULL)
		rv += printf(" (%s)", device_get_nameunit(ip));
	rv += bus_print_child_footer(dev, child);

	return (rv);
}

static struct resource *
simplebus_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	device_t ic;
	struct simplebus_devinfo *di;
	struct resource_list_entry *rle;

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
			if (bootverbose)
				device_printf(bus, "no default resources for "
				    "rid = %d, type = %d\n", *rid, type);
			return (NULL);
		}
		start = rle->start;
		end = rle->end;
		count = rle->count;
	}

	if (type == SYS_RES_IRQ &&
	    (ic = simplebus_get_interrupt_parent(child)) != NULL)
		return(FDT_IC_ALLOC_INTR(ic, child, rid, start, flags));

	return (bus_generic_alloc_resource(bus, child, type, rid, start, end,
	    count, flags));
}

static int
simplebus_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	device_t ic;

	if (type == SYS_RES_IRQ &&
	    (ic = simplebus_get_interrupt_parent(child)) != NULL)
		return (FDT_IC_ACTIVATE_INTR(ic, r));

	return (bus_generic_activate_resource(dev, child, type, rid, r));
}

static int
simplebus_deactivate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	device_t ic;

	if (type == SYS_RES_IRQ &&
	    (ic = simplebus_get_interrupt_parent(child)) != NULL)
		return (FDT_IC_DEACTIVATE_INTR(ic, r));

	return (bus_generic_deactivate_resource(dev, child, type, rid, r));
}

static int
simplebus_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	device_t ic;

	if (type == SYS_RES_IRQ &&
	    (ic = simplebus_get_interrupt_parent(child)) != NULL)
		return (FDT_IC_RELEASE_INTR(ic, r));

	return (bus_generic_release_resource(dev, child, type, rid, r));
}

static struct resource_list *
simplebus_get_resource_list(device_t bus, device_t child)
{
	struct simplebus_devinfo *di;

	di = device_get_ivars(child);
	return (&di->di_res);
}

static device_t
simplebus_get_interrupt_parent(device_t dev)
{
	struct simplebus_devinfo *di;
	struct fdt_ic *ic;
	device_t ip;
	phandle_t ph, iph;

	ip = NULL;

	di = device_get_ivars(dev);
	if (di == NULL)
		return (NULL);

	if (OF_getencprop(di->di_ofw.obd_node, "interrupt-parent", &iph,
	    sizeof(iph)) > 0) {
		ph = OF_node_from_xref(iph);
		SLIST_FOREACH(ic, &fdt_ic_list_head, fdt_ics) {
			if (ic->iph == ph) {
				ip = ic->dev;
				break;
			}
		}
	}
	return (ip);
}

static int
simplebus_setup_intr(device_t bus, device_t child, struct resource *res,
    int flags, driver_filter_t *filter, driver_intr_t *ihand, void *arg,
    void **cookiep)
{
	struct simplebus_devinfo *di;
	device_t ic;
	enum intr_trigger trig;
	enum intr_polarity pol;
	int error, irq, rid;

	di = device_get_ivars(child);
	if (di == NULL)
		return (ENXIO);

	if (res == NULL)
		return (EINVAL);

	rid = rman_get_rid(res);
	if (rid >= DI_MAX_INTR_NUM)
		return (ENOENT);

	ic = simplebus_get_interrupt_parent(child);

	trig = di->di_intr_sl[rid].trig;
	pol = di->di_intr_sl[rid].pol;
	if (trig != INTR_TRIGGER_CONFORM || pol != INTR_POLARITY_CONFORM) {
		irq = rman_get_start(res);
		if (ic != NULL)
			error = FDT_IC_CONFIG_INTR(ic, irq, trig, pol);
		else
			error = bus_generic_config_intr(bus, irq, trig, pol);
		if (error)
			return (error);
	}

	if (ic != NULL)
		error = FDT_IC_SETUP_INTR(ic, child, res, flags, filter,
		    ihand, arg, cookiep);
	else
		error = bus_generic_setup_intr(bus, child, res, flags, filter,
		    ihand, arg, cookiep);
	return (error);
}

static int
simplebus_teardown_intr(device_t bus, device_t child, struct resource *res,
    void *cookie)
{
	device_t ic;

	if ((ic = simplebus_get_interrupt_parent(child)) != NULL)
		return (FDT_IC_TEARDOWN_INTR(ic, child, res, cookie));

	return (bus_generic_teardown_intr(bus, child, res, cookie));
}

static const struct ofw_bus_devinfo *
simplebus_get_devinfo(device_t bus, device_t child)
{
	struct simplebus_devinfo *di;

	di = device_get_ivars(child);
	return (&di->di_ofw);
}
