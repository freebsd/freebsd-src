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
 * $Id: atkbdc_isa.c,v 1.4 1999/05/08 21:59:29 dfr Exp $
 */

#include "atkbdc.h"
#include "opt_kbd.h"

#if NATKBDC > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <dev/kbd/atkbdcreg.h>

#include <isa/isareg.h>
#include <isa/isavar.h>

MALLOC_DEFINE(M_ATKBDDEV, "atkbddev", "AT Keyboard device");

/* children */
typedef struct atkbdc_device {
	int flags;	/* configuration flags */
	int port;	/* port number (same as the controller's) */
	int irq;	/* ISA IRQ mask */
} atkbdc_device_t;

/* kbdc */
devclass_t atkbdc_devclass;

static int	atkbdc_probe(device_t dev);
static int	atkbdc_attach(device_t dev);
static void	atkbdc_print_child(device_t bus, device_t dev);
static int	atkbdc_read_ivar(device_t bus, device_t dev, int index,
				 u_long *val);
static int	atkbdc_write_ivar(device_t bus, device_t dev, int index,
				  u_long val);

static device_method_t atkbdc_methods[] = {
	DEVMETHOD(device_probe,		atkbdc_probe),
	DEVMETHOD(device_attach,	atkbdc_attach),

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

static int
atkbdc_probe(device_t dev)
{
	atkbdc_softc_t *sc;
	int unit;
	int error;

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

	device_set_desc(dev, "keyboard controller (i8042)");

	error = atkbdc_probe_unit(sc, unit, isa_get_port(dev));
	if (error == 0)
		*(atkbdc_softc_t **)device_get_softc(dev) = sc;

	return error;
}

static void
atkbdc_add_device(device_t dev, const char *name, int unit)
{
	atkbdc_softc_t	*sc = *(atkbdc_softc_t **)device_get_softc(dev);
	atkbdc_device_t	*kdev;
	device_t	child;
	int		t;

	kdev = malloc(sizeof(struct atkbdc_device), M_ATKBDDEV, M_NOWAIT);
	if (!kdev)
		return;
	bzero(kdev, sizeof *kdev);

	kdev->port = sc->port;

	if (resource_int_value(name, unit, "irq", &t) == 0)
		kdev->irq = t;
	else
		kdev->irq = -1;

	if (resource_int_value(name, unit, "flags", &t) == 0)
		kdev->flags = t;
	else
		kdev->flags = 0;

	child = device_add_child(dev, name, unit, kdev);
}

static int
atkbdc_attach(device_t dev)
{
	atkbdc_softc_t	*sc;
	int		i;

	sc = *(atkbdc_softc_t **)device_get_softc(dev);
	if ((sc == NULL) || (sc->port <= 0))
		return ENXIO;

	/*
	 * Add all devices configured to be attached to atkbdc0.
	 */
	for (i = resource_query_string(-1, "at", "atkbdc0");
	     i != -1;
	     i = resource_query_string(i, "at", "atkbdc0")) {
		atkbdc_add_device(dev, resource_query_name(i),
				  resource_query_unit(i));
	}

	/*
	 * and atkbdc?
	 */
	for (i = resource_query_string(-1, "at", "atkbdc");
	     i != -1;
	     i = resource_query_string(i, "at", "atkbdc")) {
		atkbdc_add_device(dev, resource_query_name(i),
				  resource_query_unit(i));
	}

	bus_generic_attach(dev);

	return 0;
}

static void
atkbdc_print_child(device_t bus, device_t dev)
{
	atkbdc_device_t *kbdcdev;

	kbdcdev = (atkbdc_device_t *)device_get_ivars(dev);

	if (kbdcdev->flags != 0)
		printf(" flags 0x%x", kbdcdev->flags);
	if (kbdcdev->irq != -1)
		printf(" irq %d", kbdcdev->irq);

	printf(" on %s%d", device_get_name(bus), device_get_unit(bus));
}

static int
atkbdc_read_ivar(device_t bus, device_t dev, int index, u_long *val)
{
	atkbdc_device_t *ivar;

	ivar = (atkbdc_device_t *)device_get_ivars(dev);
	switch (index) {
	case KBDC_IVAR_PORT:
		*val = (u_long)ivar->port;
		break;
	case KBDC_IVAR_IRQ:
		*val = (u_long)ivar->irq;
		break;
	case KBDC_IVAR_FLAGS:
		*val = (u_long)ivar->flags;
		break;
	default:
		return ENOENT;
	}
	return 0;
}

static int
atkbdc_write_ivar(device_t bus, device_t dev, int index, u_long val)
{
	atkbdc_device_t *ivar;

	ivar = (atkbdc_device_t *)device_get_ivars(dev);
	switch (index) {
	case KBDC_IVAR_PORT:
		ivar->port = (int)val;
		break;
	case KBDC_IVAR_IRQ:
		ivar->irq = (int)val;
		break;
	case KBDC_IVAR_FLAGS:
		ivar->flags = (int)val;
		break;
	default:
		return ENOENT;
	}
	return 0;
}

DRIVER_MODULE(atkbdc, isa, atkbdc_driver, atkbdc_devclass, 0, 0);

#endif /* NATKBDC > 0 */
