/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2003-2012 Broadcom Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "iicbus_if.h"
#include "iicoc.h"

static int
iicoc_detach(device_t dev)
{
	struct iicoc_softc *sc;

	sc = device_get_softc(dev);
	device_delete_children(dev);
	bus_generic_detach(dev);
	bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid, &sc->mem_res);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

/*
 * We add all the devices which we know about.
 * The generic attach routine will attach them if they are alive.
 */
static int
iicoc_attach(device_t dev)
{
	int bus;
	struct iicoc_softc *sc;

	sc = device_get_softc(dev);
	bus = device_get_unit(dev);

	sc->dev = dev;
	mtx_init(&sc->sc_mtx, "iicoc", "iicoc", MTX_DEF);
	sc->mem_rid = 0;
	sc->mem_res = bus_alloc_resource_anywhere(dev,
	    SYS_RES_MEMORY, &sc->mem_rid, 0x100, RF_ACTIVE);

	if (sc->mem_res == NULL) {
		device_printf(dev, "Could not allocate bus resource.\n");
		return (-1);
	}
	iicoc_init(dev);
	sc->iicbus = device_add_child(dev, "iicbus", -1);
	if (sc->iicbus == NULL) {
		device_printf(dev, "Could not allocate iicbus instance.\n");
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid,
		    &sc->mem_res);
		mtx_destroy(&sc->sc_mtx);
		return (-1);
	}
	bus_generic_attach(dev);

	return (0);
}

static int
iicoc_probe(device_t dev)
{
	struct iicoc_softc *sc;

	sc = device_get_softc(dev);
	if ((pci_get_vendor(dev) == 0x184e) &&
	    (pci_get_device(dev) == 0x1011)) {
		sc->clockfreq = XLP_I2C_CLKFREQ;
		sc->i2cfreq = XLP_I2C_FREQ;
		sc->reg_shift = 2;
		device_set_desc(dev, "Netlogic XLP I2C Controller");
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static device_method_t iicoc_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe, iicoc_probe),
	DEVMETHOD(device_attach, iicoc_attach),
	DEVMETHOD(device_detach, iicoc_detach),

	/* iicbus interface */
	DEVMETHOD(iicbus_callback, iicbus_null_callback),
	DEVMETHOD(iicbus_repeated_start, iicoc_iicbus_repeated_start),
	DEVMETHOD(iicbus_start, iicoc_iicbus_start),
	DEVMETHOD(iicbus_stop, iicoc_iicbus_stop),
	DEVMETHOD(iicbus_reset, iicoc_iicbus_reset),
	DEVMETHOD(iicbus_write, iicoc_iicbus_write),
	DEVMETHOD(iicbus_read, iicoc_iicbus_read),
	DEVMETHOD(iicbus_transfer, iicbus_transfer_gen),

	DEVMETHOD_END
};

static driver_t iicoc_driver = {
	"iicoc",
	iicoc_methods,
	sizeof(struct iicoc_softc),
};

DRIVER_MODULE(iicoc, pci, iicoc_driver, iicoc_devclass, 0, 0);
