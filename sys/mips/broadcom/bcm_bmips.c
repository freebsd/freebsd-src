/*-
 * Copyright (c) 2016 Landon Fuller <landonf@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/bhnd/bhnd.h>

#include "bcm_bmipsreg.h"

/*
 * BMIPS32 and BMIPS3300 core driver.
 *
 * These cores are only found on siba(4) chipsets, allowing
 * us to assume the availability of siba interrupt registers.
 */

static const struct bhnd_device bcm_bmips_devs[] = {
	BHND_DEVICE(BCM, MIPS33, NULL, NULL, BHND_DF_SOC),
	BHND_DEVICE_END
};

struct bcm_bmips_softc {
	device_t		 dev;
	struct resource		*mem_res;
	int			 mem_rid;
};

static int
bcm_bmips_probe(device_t dev)
{
	const struct bhnd_device	*id;

	id = bhnd_device_lookup(dev, bcm_bmips_devs,
	    sizeof(bcm_bmips_devs[0]));
	if (id == NULL)
		return (ENXIO);

	bhnd_set_default_core_desc(dev);
	return (BUS_PROBE_DEFAULT);
}

static int
bcm_bmips_attach(device_t dev)
{
	struct bcm_bmips_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	/* Allocate bus resources */
	sc->mem_rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->mem_rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL)
		return (ENXIO);

	return (0);
}

static int
bcm_bmips_detach(device_t dev)
{
	struct bcm_bmips_softc *sc;

	sc = device_get_softc(dev);

	bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid, sc->mem_res);

	return (0);
}

static device_method_t bcm_bmips_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			bcm_bmips_probe),
	DEVMETHOD(device_attach,		bcm_bmips_attach),
	DEVMETHOD(device_detach,		bcm_bmips_detach),
	
	DEVMETHOD_END
};

static devclass_t bcm_mips_devclass;

DEFINE_CLASS_0(bcm_mips, bcm_bmips_driver, bcm_bmips_methods, sizeof(struct bcm_bmips_softc));
EARLY_DRIVER_MODULE(bcm_bmips, bhnd, bcm_bmips_driver, bcm_mips_devclass, 0, 0, BUS_PASS_CPU + BUS_PASS_ORDER_EARLY);

MODULE_VERSION(bcm_bmips, 1);
MODULE_DEPEND(bcm_bmips, bhnd, 1, 1, 1);
