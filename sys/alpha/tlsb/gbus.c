/* $Id */
/* $NetBSD: gbus.c,v 1.8 1998/05/13 22:13:35 thorpej Exp $ */

/*
 * Copyright (c) 1997 by Matthew Jacob
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
/* 	{ "zsc",	GBUS_DUART1_OFFSET },*/
	{ "mcclock",	GBUS_CLOCK_OFFSET },
	{ NULL,		0 },
};

static devclass_t gbus_devclass;

/*
 * Bus handlers.
 */
static bus_print_device_t	gbus_print_device;
static bus_read_ivar_t		gbus_read_ivar;

static bus_ops_t gbus_bus_ops = {
	gbus_print_device,
	gbus_read_ivar,
	null_write_ivar,
	null_map_intr,
};

static void
gbus_print_device(bus_t bus, device_t dev)
{
	struct gbus_device* gdev = DEVTOGBUS(dev);
	device_t gbusdev = bus_get_device(bus);

	printf(" at %s%d offset 0x%lx",
	       device_get_name(gbusdev), device_get_unit(gbusdev),
	       gdev->gd_offset);
}

static int
gbus_read_ivar(bus_t bus, device_t dev,
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

static driver_probe_t gbus_bus_probe;

static driver_t gbus_bus_driver = {
	"gbus",
	gbus_bus_probe,
	bus_generic_attach,
	bus_generic_detach,
	bus_generic_shutdown,
	DRIVER_TYPE_MISC,
	sizeof(struct bus),
	NULL,
};

/*
 * At 'probe' time, we add all the devices which we know about to the
 * bus.  The generic attach routine will probe and attach them if they
 * are alive.
 */
static int
gbus_bus_probe(bus_t parent, device_t dev)
{
	bus_t bus = device_get_softc(dev);
	struct gbus_device *gdev;

	/*
	 * Make sure we're looking for a Gbus.
	 * Right now, only Gbus could be a
	 * child of a TLSB CPU Node.
	 */
	if (!TLDEV_ISCPU(tlsb_get_dtype(dev)))
		return ENXIO;

	bus_init(bus, dev, &gbus_bus_ops);

	for (gdev = gbus_children; gdev->gd_name; gdev++)
		bus_add_device(bus, gdev->gd_name, -1, gdev);

	return 0;
}

DRIVER_MODULE(gbus, tlsb, gbus_bus_driver, gbus_devclass, 0, 0);
