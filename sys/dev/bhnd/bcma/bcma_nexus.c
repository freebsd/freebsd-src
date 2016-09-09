/*-
 * Copyright (c) 2016 Michael Zhilin <mizhka@gmail.com>
 * Copyright (c) 2015-2016 Landon Fuller <landon@freebsd.org>
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
 * 
 * $FreeBSD$
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

#include <dev/bhnd/bhnd_ids.h>
#include <dev/bhnd/bhnd_nexusvar.h>
#include <dev/bhnd/cores/chipc/chipcreg.h>

#include "bcmavar.h"
#include "bcma_eromreg.h"

/*
 * Supports bcma(4) attachment to a nexus bus.
 */

static int	bcma_nexus_attach(device_t);
static int	bcma_nexus_probe(device_t);

struct bcma_nexus_softc {
	struct bcma_softc		parent_sc;
	struct bhnd_chipid		bcma_cid;
};

static int
bcma_nexus_probe(device_t dev)
{
	struct bcma_nexus_softc	*sc;
	int			 error;

	sc = device_get_softc(dev);

	/* Read the ChipCommon info using the hints the kernel
	 * was compiled with. */
	if ((error = bhnd_nexus_read_chipid(dev, &sc->bcma_cid)))
		return (error);

	if (sc->bcma_cid.chip_type != BHND_CHIPTYPE_BCMA)
		return (ENXIO);

	if ((error = bcma_probe(dev)) > 0) {
		device_printf(dev, "error %d in probe\n", error);
		return (error);
	}

	/* Set device description */
	bhnd_set_default_bus_desc(dev, &sc->bcma_cid);

	return (0);
}

static int
bcma_nexus_attach(device_t dev)
{
	int error;

	/* Perform initial attach and enumerate our children. */
	if ((error = bcma_attach(dev)))
		goto failed;

	/* Delegate remainder to standard bhnd method implementation */
	if ((error = bhnd_generic_attach(dev)))
		goto failed;

	return (0);

failed:
	device_delete_children(dev);
	return (error);
}

static const struct bhnd_chipid *
bcma_nexus_get_chipid(device_t dev, device_t child) {
	struct bcma_nexus_softc	*sc = device_get_softc(dev);
	return (&sc->bcma_cid);
}

static device_method_t bcma_nexus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			bcma_nexus_probe),
	DEVMETHOD(device_attach,		bcma_nexus_attach),

	/* bhnd interface */
	DEVMETHOD(bhnd_bus_get_chipid,		bcma_nexus_get_chipid),

	DEVMETHOD_END
};

DEFINE_CLASS_2(bhnd, bcma_nexus_driver, bcma_nexus_methods,
    sizeof(struct bcma_nexus_softc), bhnd_nexus_driver, bcma_driver);

EARLY_DRIVER_MODULE(bcma_nexus, nexus, bcma_nexus_driver, bhnd_devclass, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
