/*
 * Copyright (c) 2026 Justin Hibbits
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>

#include "opt_platform.h"

#include <powerpc/mpc85xx/mpc85xx.h>

#include "fman.h"

struct fman_muram_softc {
	struct resource *sc_mem;
	vmem_t sc_vmem;
};

static int
fman_muram_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "fsl,fman-muram"))
		return (ENXIO);

	device_set_desc(dev, "FMan MURAM");

	return (BUS_PROBE_DEFAULT);
}

static int
fman_muram_attach(device_t dev)
{
	struct fman_muram_softc *sc = device_get_softc(dev);

	sc->sc_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, 0,
	    RF_ACTIVE | RF_SHAREABLE);

	if (sc->sc_mem == NULL) {
		device_printf(dev, "cannot allocate memory\n");
		return (ENXIO);
	}
	sc->sc_vmem = vmem_create("MURAM", rman_get_bushandle(sc->sc_mem),
	    rman_get_size(sc->sc_mem), 
}

static device_method_t muram_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fman_muram_probe),
	DEVMETHOD(device_attach,	fman_muram_attach),
	DEVMETHOD(device_detach,	fman_muram_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(fman_muram, fman_muram_driver, muram_methods,
    sizeof(struct fman_muram_softc));
EARLY_DRIVER_MODULE(fman_muram, fman, fman_muram_driver, 0, 0,
    BUS_PASS_SUPPORTDEV);
