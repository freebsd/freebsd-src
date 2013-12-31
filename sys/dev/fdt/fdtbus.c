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
#include <dev/ofw/ofw_nexus.h>

#include "ofw_bus_if.h"

/*
 * Prototypes.
 */
static void fdtbus_identify(driver_t *, device_t);
static int fdtbus_probe(device_t);

static int fdtbus_activate_resource(device_t, device_t, int, int,
    struct resource *);
static int fdtbus_deactivate_resource(device_t, device_t, int, int,
    struct resource *);

/*
 * Bus interface definition.
 */
static device_method_t fdtbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	fdtbus_identify),
	DEVMETHOD(device_probe,		fdtbus_probe),

	/* Bus interface */
	DEVMETHOD(bus_activate_resource, fdtbus_activate_resource),
	DEVMETHOD(bus_deactivate_resource, fdtbus_deactivate_resource),
	DEVMETHOD(bus_config_intr,	bus_generic_config_intr),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	DEVMETHOD_END
};

devclass_t fdtbus_devclass;
DEFINE_CLASS_1(fdtbus, fdtbus_driver, fdtbus_methods,
    sizeof(struct ofw_nexus_softc), ofw_nexus_driver);
DRIVER_MODULE(fdtbus, nexus, fdtbus_driver, fdtbus_devclass, 0, 0);

static void
fdtbus_identify(driver_t *driver, device_t parent)
{

	if (device_find_child(parent, "fdtbus", -1) == NULL)
		BUS_ADD_CHILD(parent, 0, "fdtbus", -1);
}

static int
fdtbus_probe(device_t dev)
{

	device_set_desc(dev, "Flattened Device Tree");
	return (BUS_PROBE_NOWILDCARD);
}

static int
fdtbus_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{
	bus_space_handle_t p;
	int error;

	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		/* XXX endianess should be set based on SOC node */
		rman_set_bustag(res, fdtbus_bs_tag);
		rman_set_bushandle(res, rman_get_start(res));

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

