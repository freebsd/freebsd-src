/*-
 * Copyright (c) 1999 Kazutaka YOKOTA <yokota@zodiac.mech.utsunomiya-u.ac.jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_kbd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/kbd/atkbdcreg.h>

#include <isa/isareg.h>
#include <isa/isavar.h>

static MALLOC_DEFINE(M_ATKBDDEV, "atkbddev", "AT Keyboard device");

/* children */
typedef struct atkbdc_device {
	struct resource_list resources;
	int rid;
	u_int32_t vendorid;
	u_int32_t serial;
	u_int32_t logicalid;
	u_int32_t compatid;
} atkbdc_device_t;

/* kbdc */
static devclass_t atkbdc_devclass;

static int	atkbdc_probe(device_t dev);
static int	atkbdc_attach(device_t dev);
static device_t	atkbdc_add_child(device_t bus, int order, char *name,
				 int unit);
static int	atkbdc_print_child(device_t bus, device_t dev);
static int	atkbdc_read_ivar(device_t bus, device_t dev, int index,
				 uintptr_t *val);
static int	atkbdc_write_ivar(device_t bus, device_t dev, int index,
				  uintptr_t val);
static struct resource_list
		*atkbdc_get_resource_list (device_t bus, device_t dev);
static struct resource
		*atkbdc_alloc_resource(device_t bus, device_t dev, int type,
				       int *rid, u_long start, u_long end,
				       u_long count, u_int flags);
static int	atkbdc_release_resource(device_t bus, device_t dev, int type,
					int rid, struct resource *res);

static device_method_t atkbdc_methods[] = {
	DEVMETHOD(device_probe,		atkbdc_probe),
	DEVMETHOD(device_attach,	atkbdc_attach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	DEVMETHOD(bus_add_child,	atkbdc_add_child),
	DEVMETHOD(bus_print_child,	atkbdc_print_child),
	DEVMETHOD(bus_read_ivar,	atkbdc_read_ivar),
	DEVMETHOD(bus_write_ivar,	atkbdc_write_ivar),
	DEVMETHOD(bus_get_resource_list,atkbdc_get_resource_list),
	DEVMETHOD(bus_alloc_resource,	atkbdc_alloc_resource),
	DEVMETHOD(bus_release_resource,	atkbdc_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_delete_resource,	bus_generic_rl_delete_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	{ 0, 0 }
};

static driver_t atkbdc_driver = {
	ATKBDC_DRIVER_NAME,
	atkbdc_methods,
	sizeof(atkbdc_softc_t *),
};

static struct isa_pnp_id atkbdc_ids[] = {
	{ 0x0303d041, "Keyboard controller (i8042)" },	/* PNP0303 */
	{ 0 }
};

static int
atkbdc_probe(device_t dev)
{
	struct resource	*port0;
	struct resource	*port1;
	u_long		start;
	u_long		count;
	int		error;
	int		rid;

	/* check PnP IDs */
	if (ISA_PNP_PROBE(device_get_parent(dev), dev, atkbdc_ids) == ENXIO)
		return ENXIO;

	device_set_desc(dev, "Keyboard controller (i8042)");

	/*
	 * Adjust I/O port resources.
	 * The AT keyboard controller uses two ports (a command/data port
	 * 0x60 and a status port 0x64), which may be given to us in 
	 * one resource (0x60 through 0x64) or as two separate resources
	 * (0x60 and 0x64). Furthermore, /boot/device.hints may contain
	 * just one port, 0x60. We shall adjust resource settings 
	 * so that these two ports are available as two separate resources.
	 */
	device_quiet(dev);
	rid = 0;
	if (bus_get_resource(dev, SYS_RES_IOPORT, rid, &start, &count) != 0)
		return ENXIO;
	if (count > 1)	/* adjust the count */
		bus_set_resource(dev, SYS_RES_IOPORT, rid, start, 1);
	port0 = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid, RF_ACTIVE);
	if (port0 == NULL)
		return ENXIO;
	rid = 1;
	if (bus_get_resource(dev, SYS_RES_IOPORT, rid, NULL, NULL) != 0)
		bus_set_resource(dev, SYS_RES_IOPORT, 1,
				 start + KBD_STATUS_PORT, 1);
	port1 = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid, RF_ACTIVE);
	if (port1 == NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, 0, port0);
		return ENXIO;
	}
	device_verbose(dev);

	error = atkbdc_probe_unit(device_get_unit(dev), port0, port1);
	if (error == 0)
		bus_generic_probe(dev);

	bus_release_resource(dev, SYS_RES_IOPORT, 0, port0);
	bus_release_resource(dev, SYS_RES_IOPORT, 1, port1);

	return error;
}

static int
atkbdc_attach(device_t dev)
{
	atkbdc_softc_t	*sc;
	int		unit;
	int		error;
	int		rid;

	unit = device_get_unit(dev);
	sc = *(atkbdc_softc_t **)device_get_softc(dev);
	if (sc == NULL) {
		/*
		 * We have to maintain two copies of the kbdc_softc struct,
		 * as the low-level console needs to have access to the
		 * keyboard controller before kbdc is probed and attached. 
		 * kbdc_soft[] contains the default entry for that purpose.
		 * See atkbdc.c. XXX
		 */
		sc = atkbdc_get_softc(unit);
		if (sc == NULL)
			return ENOMEM;
	}

	rid = 0;
	sc->port0 = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid,
					   RF_ACTIVE);
	if (sc->port0 == NULL)
		return ENXIO;
	rid = 1;
	sc->port1 = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid,
					   RF_ACTIVE);
	if (sc->port1 == NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, 0, sc->port0);
		return ENXIO;
	}

	error = atkbdc_attach_unit(unit, sc, sc->port0, sc->port1);
	if (error) {
		bus_release_resource(dev, SYS_RES_IOPORT, 0, sc->port0);
		bus_release_resource(dev, SYS_RES_IOPORT, 1, sc->port1);
		return error;
	}
	*(atkbdc_softc_t **)device_get_softc(dev) = sc;

	bus_generic_attach(dev);

	return 0;
}

static device_t
atkbdc_add_child(device_t bus, int order, char *name, int unit)
{
	atkbdc_device_t	*ivar;
	device_t	child;
	int		t;

	ivar = malloc(sizeof(struct atkbdc_device), M_ATKBDDEV,
		M_NOWAIT | M_ZERO);
	if (!ivar)
		return NULL;

	child = device_add_child_ordered(bus, order, name, unit);
	if (child == NULL) {
		free(ivar, M_ATKBDDEV);
		return child;
	}

	resource_list_init(&ivar->resources);
	ivar->rid = order;

	/*
	 * If the device is not created by the PnP BIOS or ACPI,
	 * refer to device hints for IRQ.
	 */
	if (ISA_PNP_PROBE(device_get_parent(bus), bus, atkbdc_ids) != 0) {
		if (resource_int_value(name, unit, "irq", &t) != 0)
			t = -1;
	} else {
		t = bus_get_resource_start(bus, SYS_RES_IRQ, ivar->rid);
	}
	if (t > 0)
		resource_list_add(&ivar->resources, SYS_RES_IRQ, ivar->rid,
				  t, t, 1);

	if (resource_disabled(name, unit))
		device_disable(child);

	device_set_ivars(child, ivar);

	return child;
}

static int
atkbdc_print_child(device_t bus, device_t dev)
{
	atkbdc_device_t *kbdcdev;
	u_long irq;
	int flags;
	int retval = 0;

	kbdcdev = (atkbdc_device_t *)device_get_ivars(dev);

	retval += bus_print_child_header(bus, dev);
	flags = device_get_flags(dev);
	if (flags != 0)
		retval += printf(" flags 0x%x", flags);
	irq = bus_get_resource_start(dev, SYS_RES_IRQ, kbdcdev->rid);
	if (irq != 0)
		retval += printf(" irq %ld", irq);
	retval += bus_print_child_footer(bus, dev);

	return (retval);
}

static int
atkbdc_read_ivar(device_t bus, device_t dev, int index, uintptr_t *val)
{
	atkbdc_device_t *ivar;

	ivar = (atkbdc_device_t *)device_get_ivars(dev);
	switch (index) {
	case KBDC_IVAR_VENDORID:
		*val = (u_long)ivar->vendorid;
		break;
	case KBDC_IVAR_SERIAL:
		*val = (u_long)ivar->serial;
		break;
	case KBDC_IVAR_LOGICALID:
		*val = (u_long)ivar->logicalid;
		break;
	case KBDC_IVAR_COMPATID:
		*val = (u_long)ivar->compatid;
		break;
	default:
		return ENOENT;
	}
	return 0;
}

static int
atkbdc_write_ivar(device_t bus, device_t dev, int index, uintptr_t val)
{
	atkbdc_device_t *ivar;

	ivar = (atkbdc_device_t *)device_get_ivars(dev);
	switch (index) {
	case KBDC_IVAR_VENDORID:
		ivar->vendorid = (u_int32_t)val;
		break;
	case KBDC_IVAR_SERIAL:
		ivar->serial = (u_int32_t)val;
		break;
	case KBDC_IVAR_LOGICALID:
		ivar->logicalid = (u_int32_t)val;
		break;
	case KBDC_IVAR_COMPATID:
		ivar->compatid = (u_int32_t)val;
		break;
	default:
		return ENOENT;
	}
	return 0;
}

static struct resource_list
*atkbdc_get_resource_list (device_t bus, device_t dev)
{
	atkbdc_device_t *ivar;

	ivar = (atkbdc_device_t *)device_get_ivars(dev);
	return &ivar->resources;
}

static struct resource
*atkbdc_alloc_resource(device_t bus, device_t dev, int type, int *rid,
		       u_long start, u_long end, u_long count, u_int flags)
{
	atkbdc_device_t *ivar;

	ivar = (atkbdc_device_t *)device_get_ivars(dev);
	return resource_list_alloc(&ivar->resources, bus, dev, type, rid,
				   start, end, count, flags);
}

static int
atkbdc_release_resource(device_t bus, device_t dev, int type, int rid,
			struct resource *res)
{
	atkbdc_device_t *ivar;

	ivar = (atkbdc_device_t *)device_get_ivars(dev);
	return resource_list_release(&ivar->resources, bus, dev, type, rid,
				     res);
}

DRIVER_MODULE(atkbdc, isa, atkbdc_driver, atkbdc_devclass, 0, 0);
DRIVER_MODULE(atkbdc, acpi, atkbdc_driver, atkbdc_devclass, 0, 0);
