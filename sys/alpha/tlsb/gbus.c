/* $FreeBSD$ */
/* $NetBSD: gbus.c,v 1.8 1998/05/13 22:13:35 thorpej Exp $ */

/*
 * Copyright (c) 1997, 2000 by Matthew Jacob
 * NASA AMES Research Center.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Autoconfiguration and support routines for the Gbus: the internal
 * bus on AlphaServer CPU modules.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/rpb.h>
#include <machine/pte.h>

#include <alpha/tlsb/gbusreg.h>
#include <alpha/tlsb/gbusvar.h>

#include <alpha/tlsb/tlsbreg.h>
#include <alpha/tlsb/tlsbvar.h>

extern int	cputype;

#define KV(_addr)	((caddr_t)ALPHA_PHYS_TO_K0SEG((_addr)))

/*
 * The structure used to attach devices to the Gbus.
 */
struct gbus_device {
	const char*	gd_name;
	int		gd_offset;
};

#define DEVTOGBUS(dev)	((struct gbus_device*) device_get_ivars(dev))

struct gbus_device gbus_children[] = {
	{ "zsc",	GBUS_DUART0_OFFSET },
	{ "mcclock",	GBUS_CLOCK_OFFSET },
	{ NULL,		0 },
};

static devclass_t gbus_devclass;

/*
 * Device methods
 */
static int gbus_probe(device_t);
static int gbus_print_child(device_t, device_t);
static int gbus_read_ivar(device_t, device_t, int, u_long *);

static device_method_t gbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gbus_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	gbus_print_child),
	DEVMETHOD(bus_read_ivar,	gbus_read_ivar),
	DEVMETHOD(bus_write_ivar,	bus_generic_write_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	{ 0, 0 }
};

static driver_t gbus_driver = {
	"gbus", gbus_methods, 1
};

/*
 * At 'probe' time, we add all the devices which we know about to the
 * bus.  The generic attach routine will probe and attach them if they
 * are alive.
 */
static int
gbus_probe(device_t dev)
{
	device_t child;
	struct gbus_device *gdev;

	/*
	 * Make sure we're looking for a Gbus.
	 * A Gbus can only be a child of a TLSB CPU Node.
	 */
	if (!TLDEV_ISCPU(tlsb_get_dtype(device_get_parent(dev)))) {
		return ENXIO;
	}

	for (gdev = gbus_children; gdev->gd_name; gdev++) {
		child = device_add_child(dev, gdev->gd_name, -1);
		device_set_ivars(child, gdev);
	}

	return (0);
}

static int
gbus_print_child(device_t bus, device_t dev)
{
	struct gbus_device* gdev = DEVTOGBUS(dev);
	int retval = 0;

	retval += bus_print_child_header(bus, dev);
	retval += printf(" on %s offset 0x%x\n", device_get_nameunit(bus),
			 gdev->gd_offset);

	return (retval);
}

static int
gbus_read_ivar(device_t bus, device_t dev,
	       int index, u_long* result)
{
	struct gbus_device* gdev = DEVTOGBUS(dev);

	switch (index) {
	case GBUS_IVAR_OFFSET:
		*result = gdev->gd_offset;
		break;
	}
	return ENOENT;
}
DRIVER_MODULE(gbus, tlsbcpu, gbus_driver, gbus_devclass, 0, 0);
