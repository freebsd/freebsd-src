/*-
 * Copyright (c) 2016 Michael Zhilin <mizhka@gmail.com>
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
#include <sys/errno.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/bhnd/bhnd.h>

#include "bhnd_bus_if.h"
#include "bcmavar.h"
#include "bcma_eromreg.h"

#define	BCMA_NEXUS_EROM_RID	10

static int
bcma_nexus_probe(device_t dev)
{
	const struct bhnd_chipid *cid = BHND_BUS_GET_CHIPID(device_get_parent(dev), dev);

	/* Check bus type */
	if (cid->chip_type != BHND_CHIPTYPE_BCMA)
		return (ENXIO);

	/* Delegate to default probe implementation */
	return (bcma_probe(dev));
}

static int
bcma_nexus_attach(device_t dev)
{
	int 		 erom_rid;
	int 		 error;
	struct resource	*erom_res;
	const struct bhnd_chipid *cid = BHND_BUS_GET_CHIPID(device_get_parent(dev), dev);

  	erom_rid = BCMA_NEXUS_EROM_RID;
 	error = bus_set_resource(dev, SYS_RES_MEMORY, erom_rid, cid->enum_addr, BCMA_EROM_TABLE_SIZE);
 	if (error != 0) {
 		BHND_ERROR_DEV(dev, "failed to set EROM resource");
 		return (error);
 	}

 	/* Map the EROM resource and enumerate our children. */
 	BHND_DEBUG_DEV(dev, "erom enum address: %jx", cid->enum_addr);
 	erom_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &erom_rid, RF_ACTIVE);
 	if (erom_res == NULL) {
 		BHND_ERROR_DEV(dev, "failed to allocate EROM resource");
 		return (ENXIO);
 	}

 	BHND_DEBUG_DEV(dev, "erom scanning start address: %p", rman_get_virtual(erom_res));
 	error = bcma_add_children(dev, erom_res, BCMA_EROM_TABLE_START);

 	/* Clean up */
 	bus_release_resource(dev, SYS_RES_MEMORY, erom_rid, erom_res);
 	if (error)
 		return (error);

 	/* Call our superclass' implementation */
 	return (bcma_attach(dev));
}

static device_method_t bcma_nexus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			bcma_nexus_probe),
	DEVMETHOD(device_attach,		bcma_nexus_attach),
	DEVMETHOD_END
};

DEFINE_CLASS_1(bhnd, bcma_nexus_driver, bcma_nexus_methods, sizeof(struct bcma_softc), bcma_driver);
EARLY_DRIVER_MODULE(bcma_nexus, bhnd_soc, bcma_nexus_driver, bhnd_devclass,
    NULL, NULL, BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);

MODULE_VERSION(bcma_nexus, 1);
MODULE_DEPEND(bcma_nexus, bcma, 1, 1, 1);
MODULE_DEPEND(bcma_nexus, bhnd_soc, 1, 1, 1);
