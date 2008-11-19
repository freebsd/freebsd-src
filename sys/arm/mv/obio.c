/*-
 * Copyright (c) 2006 Benno Rice.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * from: FreeBSD: //depot/projects/arm/src/sys/arm/xscale/pxa2x0/pxa2x0_obio.c, rev 1
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <arm/mv/mvvar.h>
#include <arm/mv/mvreg.h>

static void	mbus_identify(driver_t *, device_t);
static int	mbus_probe(device_t);
static int	mbus_attach(device_t);

static void
mbus_identify(driver_t *driver, device_t parent)
{

	BUS_ADD_CHILD(parent, 0, "mbus", 0);
}

static int
mbus_probe(device_t dev)
{

	device_set_desc(dev, "Marvell Internal Bus (Mbus)");
	return (0);
}

static int
mbus_attach(device_t dev)
{
	struct		obio_softc *sc;
	struct		obio_device *od;
	int		i;
	device_t	child;

	sc = device_get_softc(dev);

	sc->obio_bst = obio_tag;

	sc->obio_mem.rm_type = RMAN_ARRAY;
	sc->obio_mem.rm_descr = "Marvell OBIO Memory";
	if (rman_init(&sc->obio_mem) != 0)
		panic("mbus_attach: failed to init obio mem rman");
	if (rman_manage_region(&sc->obio_mem, 0, ~0) != 0)
		panic("mbus_attach: failed to set up obio mem rman");

	sc->obio_irq.rm_type = RMAN_ARRAY;
	sc->obio_irq.rm_descr = "Marvell OBIO IRQ";
	if (rman_init(&sc->obio_irq) != 0)
		panic("mbus_attach: failed to init obio irq rman");
	if (rman_manage_region(&sc->obio_irq, 0, NIRQ - 1) != 0)
		panic("mbus_attach: failed to set up obio irq rman");

	sc->obio_gpio.rm_type = RMAN_ARRAY;
	sc->obio_gpio.rm_descr = "Marvell GPIO";
	if (rman_init(&sc->obio_gpio) != 0)
		panic("mbus_attach: failed to init gpio rman");
	if (rman_manage_region(&sc->obio_gpio, 0, MV_GPIO_MAX_NPINS - 1) != 0)
		panic("mbus_attach: failed to set up gpio rman");

	for (od = obio_devices; od->od_name != NULL; od++) {
		if (soc_power_ctrl_get(od->od_pwr_mask) != od->od_pwr_mask)
			continue;

		resource_list_init(&od->od_resources);

		resource_list_add(&od->od_resources, SYS_RES_MEMORY, 0,
		    od->od_base, od->od_base + od->od_size, od->od_size);

		for (i = 0; od->od_irqs[i] != -1; i++) {
			resource_list_add(&od->od_resources, SYS_RES_IRQ, i,
			    od->od_irqs[i], od->od_irqs[i], 1);
		}

		for (i = 0; od->od_gpio[i] != -1; i++) {
			resource_list_add(&od->od_resources, SYS_RES_GPIO, i,
			    od->od_gpio[i], od->od_gpio[i], 1);
		}

		child = device_add_child(dev, od->od_name, -1);
		device_set_ivars(child, od);
	}

	bus_generic_probe(dev);
	bus_generic_attach(dev);

	return (0);
}

static int
mbus_print_child(device_t dev, device_t child)
{
	struct	obio_device *od;
	int	rv;

	od = (struct obio_device *)device_get_ivars(child);
	if (od == NULL)
		panic("Unknown device on %s", device_get_nameunit(dev));

	rv = 0;

	rv += bus_print_child_header(dev, child);

	rv += resource_list_print_type(&od->od_resources, "at mem",
	    SYS_RES_MEMORY, "0x%08lx");
	rv += resource_list_print_type(&od->od_resources, "irq",
	    SYS_RES_IRQ, "%ld");
	rv += resource_list_print_type(&od->od_resources, "gpio",
	    SYS_RES_GPIO, "%ld");

	if (device_get_flags(child))
		rv += printf(" flags %#x", device_get_flags(child));
	rv += bus_print_child_footer(dev, child);

	return (rv);
}

static int
mbus_setup_intr(device_t dev, device_t child, struct resource *ires, int flags,
    driver_filter_t *filt, driver_intr_t *intr, void *arg, void **cookiep)
{
	struct	obio_softc *sc;
	int error;

	sc = (struct obio_softc *)device_get_softc(dev);

	if (rman_is_region_manager(ires, &sc->obio_gpio)) {
		error = mv_gpio_setup_intrhandler(
		    device_get_nameunit(child), filt, intr, arg,
		    rman_get_start(ires), flags, cookiep);

		if (error != 0)
			return (error);

		mv_gpio_intr_unmask(rman_get_start(ires));
		return (0);
	}

	BUS_SETUP_INTR(device_get_parent(dev), child, ires, flags, filt, intr, arg,
	    cookiep);
	arm_unmask_irq(rman_get_start(ires));
	return (0);
}

static int
mbus_teardown_intr(device_t dev, device_t child, struct resource *ires, void *cookie)
{

	return (BUS_TEARDOWN_INTR(device_get_parent(dev), child, ires, cookie));
}

static int
mbus_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct	obio_device *od;

	od = (struct obio_device *)device_get_ivars(child);

	switch (which) {
	case MBUS_IVAR_BASE:
		*((u_long *)result) = od->od_base;
		break;

	default:
		return (ENOENT);
	}

	return (0);
}

static struct resource_list *
mbus_get_resource_list(device_t dev, device_t child)
{
	struct	obio_device *od;

	od = (struct obio_device *)device_get_ivars(child);

	if (od == NULL)
		return (NULL);

	return (&od->od_resources);
}

static struct resource *
mbus_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct	obio_softc *sc;
	struct	obio_device *od;
	struct	resource *rv;
	struct	resource_list *rl;
	struct	resource_list_entry *rle = NULL;
	struct	rman *rm;
	int	needactivate;

	sc = (struct obio_softc *)device_get_softc(dev);

	if (type == SYS_RES_IRQ && IS_GPIO_IRQ(start)) {
		type = SYS_RES_GPIO;
		start = IRQ2GPIO(start);
	}

	switch (type) {
	case SYS_RES_IRQ:
		rm = &sc->obio_irq;
		break;
	case SYS_RES_MEMORY:
		rm = &sc->obio_mem;
		break;
	case SYS_RES_GPIO:
		rm = &sc->obio_gpio;
		break;
	default:
		return (NULL);
	}

	if (device_get_parent(child) == dev) {
		od = (struct obio_device *)device_get_ivars(child);
		rl = &od->od_resources;
		rle = resource_list_find(rl, type, *rid);

		if (rle->res != NULL)
			panic("mbus_alloc_resource: resource is busy");
	}

	if (rle) {
		start = rle->start;
		end = rle->end;
		count = rle->count;
	}

	needactivate = flags & RF_ACTIVE;
	flags &= ~RF_ACTIVE;
	rv = rman_reserve_resource(rm, start, end, count, flags, child);

	if (rv == NULL)
		return (NULL);

	if (rle)
		rle->res = rv;

	rman_set_rid(rv, *rid);

	if (type == SYS_RES_MEMORY) {
		rman_set_bustag(rv, sc->obio_bst);
		rman_set_bushandle(rv, start);
	}

	if (needactivate)
		if (bus_activate_resource(child, type, *rid, rv)) {
			rman_release_resource(rv);
			return (NULL);
		}

	return (rv);
}

static int
mbus_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct	obio_device *od;
	struct	resource_list *rl;
	struct	resource_list_entry *rle;

	if (device_get_parent(child) == dev) {
		od = (struct obio_device *)device_get_ivars(child);
		rl = &od->od_resources;
		rle = resource_list_find(rl, type, rid);

		if (!rle)
			panic("mbus_release_resource: can't find resource");

		if (!rle->res)
			panic("mbus_release_resource: resource entry is not busy");

		r = rle->res;
		rle->res = NULL;
	}

	rman_release_resource(r);

	return (0);
}

static int
mbus_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{

	return (rman_activate_resource(r));
}

static device_t
mbus_add_child(device_t bus, int order, const char *name, int unit)
{
	struct obio_device *od;
	device_t child;

	od = malloc(sizeof(struct obio_device), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!od)
		return (0);

	resource_list_init(&od->od_resources);
	od->od_name = name;

	child = device_add_child_ordered(bus, order, name, unit);
	device_set_ivars(child, od);

	return (child);
}

static device_method_t mbus_methods[] = {
	DEVMETHOD(device_identify,	mbus_identify),
	DEVMETHOD(device_probe,		mbus_probe),
	DEVMETHOD(device_attach,	mbus_attach),

	DEVMETHOD(bus_add_child,        mbus_add_child),
	DEVMETHOD(bus_print_child,	mbus_print_child),

	DEVMETHOD(bus_read_ivar,	mbus_read_ivar),
	DEVMETHOD(bus_setup_intr,	mbus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	mbus_teardown_intr),

	DEVMETHOD(bus_set_resource,		bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource_list,	mbus_get_resource_list),
	DEVMETHOD(bus_alloc_resource,		mbus_alloc_resource),
	DEVMETHOD(bus_release_resource,		mbus_release_resource),
	DEVMETHOD(bus_activate_resource,	mbus_activate_resource),

	{0, 0}
};

static driver_t mbus_driver = {
	"mbus",
	mbus_methods,
	sizeof(struct obio_softc),
};

static devclass_t mbus_devclass;

DRIVER_MODULE(mbus, nexus, mbus_driver, mbus_devclass, 0, 0);
