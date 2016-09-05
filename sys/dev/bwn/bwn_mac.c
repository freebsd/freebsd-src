/*-
 * Copyright (c) 2015 Landon Fuller <landon@landonf.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bwn.h"
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/bhnd/bhnd.h>
#include <dev/bhnd/bhnd_ids.h>

#include "bhnd_nvram_map.h"

struct bwn_softc {
	int			 mem_rid;
	struct bhnd_resource	*mem_res;

	int			 intr_rid;
	struct resource		*intr_res;
};

static const struct bwn_device {
	uint16_t	 vendor;
	uint16_t	 device;
} bwn_devices[] = {
	{ BHND_MFGID_BCM,	BHND_COREID_D11 },
	{ BHND_MFGID_INVALID,	BHND_COREID_INVALID }
};

static int
bwn_probe(device_t dev)
{
	const struct bwn_device	*id;

	for (id = bwn_devices; id->device != BHND_COREID_INVALID; id++)
	{
		if (bhnd_get_vendor(dev) == id->vendor &&
		    bhnd_get_device(dev) == id->device)
		{
			device_set_desc(dev, bhnd_get_device_name(dev));
			return (BUS_PROBE_DEFAULT);
		}
	}

	return (ENXIO);
}

static int
bwn_attach(device_t dev)
{
	struct bwn_softc	*sc;
	int			 error;

	sc = device_get_softc(dev);

	/* Allocate device resources */
	sc->mem_rid = 0;
	sc->mem_res = bhnd_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->mem_rid, RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "failed to allocate device registers\n");
		error = ENXIO;
		goto cleanup;
	}

	sc->intr_rid = 0;
	sc->intr_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->intr_rid,
	    RF_ACTIVE|RF_SHAREABLE);
	if (sc->intr_res == NULL) {
		device_printf(dev, "failed to allocate device interrupt\n");
		error = ENXIO;
		goto cleanup;
	}

	// TODO
	uint8_t	macaddr[6];
	error = bhnd_nvram_getvar_array(dev, BHND_NVAR_MACADDR, macaddr,
	    sizeof(macaddr), BHND_NVRAM_TYPE_UINT8);
	if (error)
		device_printf(dev, "error fetching macaddr: %d\n", error);
	else
		device_printf(dev, "got macaddr %6D\n", macaddr, ":");

	return (0);

cleanup:
	if (sc->mem_res != NULL)
		bhnd_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid,
		    sc->mem_res);

	if (sc->intr_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, sc->intr_rid,
		    sc->intr_res);

	return (error);
}

static int
bwn_detach(device_t dev)
{
	struct bwn_softc	*sc;

	sc = device_get_softc(dev);

	bhnd_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid, sc->mem_res);
	bus_release_resource(dev, SYS_RES_IRQ, sc->intr_rid, sc->intr_res);

	return (0);
}

static int
bwn_suspend(device_t dev)
{
	return (0);
}

static int
bwn_resume(device_t dev)
{
	return (0);
}

static device_method_t bwn_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bwn_probe),
	DEVMETHOD(device_attach,	bwn_attach),
	DEVMETHOD(device_detach,	bwn_detach),
	DEVMETHOD(device_suspend,	bwn_suspend),
	DEVMETHOD(device_resume,	bwn_resume),
	DEVMETHOD_END
};

static devclass_t bwn_devclass;

DEFINE_CLASS_0(bwn, bwn_driver, bwn_methods, sizeof(struct bwn_softc));
DRIVER_MODULE(bwn_mac, bhnd, bwn_driver, bwn_devclass, 0, 0);
MODULE_DEPEND(bwn_mac, bhnd, 1, 1, 1);
MODULE_VERSION(bwn_mac, 1);
