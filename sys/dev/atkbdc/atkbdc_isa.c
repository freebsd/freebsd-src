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
 *
 * $FreeBSD$
 */

#include "opt_kbd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
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
	int flags;	/* configuration flags */
	int irq;	/* ISA IRQ mask */
	u_int32_t vendorid;
	u_int32_t serial;
	u_int32_t logicalid;
	u_int32_t compatid;
} atkbdc_device_t;

/* kbdc */
devclass_t atkbdc_devclass;

static int	atkbdc_probe(device_t dev);
static int	atkbdc_attach(device_t dev);
static int	atkbdc_print_child(device_t bus, device_t dev);
static int	atkbdc_read_ivar(device_t bus, device_t dev, int index,
				 uintptr_t *val);
static int	atkbdc_write_ivar(device_t bus, device_t dev, int index,
				  uintptr_t val);

static device_method_t atkbdc_methods[] = {
	DEVMETHOD(device_probe,		atkbdc_probe),
	DEVMETHOD(device_attach,	atkbdc_attach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	DEVMETHOD(bus_print_child,	atkbdc_print_child),
	DEVMETHOD(bus_read_ivar,	atkbdc_read_ivar),
	DEVMETHOD(bus_write_ivar,	atkbdc_write_ivar),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
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
	int		error;
	int		rid;

	/* check PnP IDs */
	if (ISA_PNP_PROBE(device_get_parent(dev), dev, atkbdc_ids) == ENXIO)
		return ENXIO;

	device_set_desc(dev, "Keyboard controller (i8042)");

	rid = 0;
	port0 = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1,
				   RF_ACTIVE);
	if (port0 == NULL)
		return ENXIO;
	/* XXX */
	if (bus_get_resource_start(dev, SYS_RES_IOPORT, 1) <= 0) {
		bus_set_resource(dev, SYS_RES_IOPORT, 1,
				 rman_get_start(port0) + KBD_STATUS_PORT, 1);
	}
	rid = 1;
	port1 = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1,
				   RF_ACTIVE);
	if (port1 == NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, 0, port0);
		return ENXIO;
	}

	error = atkbdc_probe_unit(device_get_unit(dev), port0, port1);

	bus_release_resource(dev, SYS_RES_IOPORT, 0, port0);
	bus_release_resource(dev, SYS_RES_IOPORT, 1, port1);

	return error;
}

static void
atkbdc_add_device(device_t dev, const char *name, int unit)
{
	atkbdc_device_t	*kdev;
	device_t	child;
	int		t;

	if (resource_int_value(name, unit, "disabled", &t) == 0 && t != 0)
		return;

	kdev = malloc(sizeof(struct atkbdc_device), M_ATKBDDEV,
		M_NOWAIT | M_ZERO);
	if (!kdev)
		return;

	if (resource_int_value(name, unit, "irq", &t) == 0)
		kdev->irq = t;
	else
		kdev->irq = -1;

	if (resource_int_value(name, unit, "flags", &t) == 0)
		kdev->flags = t;
	else
		kdev->flags = 0;

	child = device_add_child(dev, name, unit);
	device_set_ivars(child, kdev);
}

static int
atkbdc_attach(device_t dev)
{
	atkbdc_softc_t	*sc;
	int		unit, dunit;
	int		error;
	int		rid;
	int		i;
	const char	*name, *dname;

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
	sc->port0 = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1,
				       RF_ACTIVE);
	if (sc->port0 == NULL)
		return ENXIO;
	rid = 1;
	sc->port1 = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1,
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

	/*
	 * Add all devices configured to be attached to atkbdc0.
	 */
	name = device_get_nameunit(dev);
	i = 0;
	while ((resource_find_match(&i, &dname, &dunit, "at", name)) == 0)
		atkbdc_add_device(dev, dname, dunit);

	/*
	 * and atkbdc?
	 */
	name = device_get_name(dev);
	i = 0;
	while ((resource_find_match(&i, &dname, &dunit, "at", name)) == 0)
		atkbdc_add_device(dev, dname, dunit);

	bus_generic_attach(dev);

	return 0;
}

static int
atkbdc_print_child(device_t bus, device_t dev)
{
	atkbdc_device_t *kbdcdev;
	int retval = 0;

	kbdcdev = (atkbdc_device_t *)device_get_ivars(dev);

	retval += bus_print_child_header(bus, dev);
	if (kbdcdev->flags != 0)
		retval += printf(" flags 0x%x", kbdcdev->flags);
	if (kbdcdev->irq != -1)
		retval += printf(" irq %d", kbdcdev->irq);
	retval += bus_print_child_footer(bus, dev);

	return (retval);
}

static int
atkbdc_read_ivar(device_t bus, device_t dev, int index, uintptr_t *val)
{
	atkbdc_device_t *ivar;

	ivar = (atkbdc_device_t *)device_get_ivars(dev);
	switch (index) {
	case KBDC_IVAR_IRQ:
		*val = (u_long)ivar->irq;
		break;
	case KBDC_IVAR_FLAGS:
		*val = (u_long)ivar->flags;
		break;
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
	case KBDC_IVAR_IRQ:
		ivar->irq = (int)val;
		break;
	case KBDC_IVAR_FLAGS:
		ivar->flags = (int)val;
		break;
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

DRIVER_MODULE(atkbdc, isa, atkbdc_driver, atkbdc_devclass, 0, 0);
