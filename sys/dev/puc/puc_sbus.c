/*-
 * Copyright (c) 2003 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_puc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>

#include <dev/ofw/ofw_bus.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#define	PUC_ENTRAILS	1
#include <dev/puc/pucvar.h>

static int
puc_sbus_probe(device_t dev)
{
	const char *nm;

	nm = ofw_bus_get_name(dev);
	if (!strcmp(nm, "zs")) {
		device_set_desc(dev, "Zilog Z8530 dual channel SCC");
		return (0);
	}
	return (ENXIO);
}

static int
puc_sbus_attach(device_t dev)
{
	struct puc_device_description dd;
	int i;

	bzero(&dd, sizeof(dd));
	dd.name = device_get_desc(dev);
	for (i = 0; i < 2; i++) {
		dd.ports[i].type = PUC_PORT_TYPE_UART | PUC_PORT_UART_Z8530;
		dd.ports[i].bar = 0;
		dd.ports[i].offset = 4 - 4 * i;
		dd.ports[i].serialfreq = 0;
		dd.ports[i].flags = PUC_FLAGS_MEMORY;
		dd.ports[i].regshft = 1;
	}
	return (puc_attach(dev, &dd));
}

static device_method_t puc_sbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,             puc_sbus_probe),
	DEVMETHOD(device_attach,            puc_sbus_attach),

	DEVMETHOD(bus_alloc_resource,       puc_alloc_resource),
	DEVMETHOD(bus_release_resource,     puc_release_resource),
	DEVMETHOD(bus_get_resource,         puc_get_resource),
	DEVMETHOD(bus_read_ivar,            puc_read_ivar),
	DEVMETHOD(bus_setup_intr,           puc_setup_intr),
	DEVMETHOD(bus_teardown_intr,        puc_teardown_intr),
	DEVMETHOD(bus_print_child,          bus_generic_print_child),
	DEVMETHOD(bus_driver_added,         bus_generic_driver_added),
	{ 0, 0 }
};

static driver_t puc_sbus_driver = {
	"puc",
	puc_sbus_methods,
	sizeof(struct puc_softc),
};

DRIVER_MODULE(puc, sbus, puc_sbus_driver, puc_devclass, 0, 0);
