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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>

#include <dev/bhnd/bhnd_ids.h>
#include <dev/bhnd/bhndb/bhndbvar.h>
#include <dev/bhnd/bhndb/bhndb_hwdata.h>

#include "bcmavar.h"

#include "bcma_eromreg.h"
#include "bcma_eromvar.h"

/*
 * Supports attachment of bcma(4) bus devices via a bhndb bridge.
 */

static int
bcma_bhndb_probe(device_t dev)
{
	const struct bhnd_chipid *cid;

	/* Check bus type */
	cid = BHNDB_GET_CHIPID(device_get_parent(dev), dev);
	if (cid->chip_type != BHND_CHIPTYPE_BCMA)
		return (ENXIO);

	/* Delegate to default probe implementation */
	return (bcma_probe(dev));
}

static int
bcma_bhndb_attach(device_t dev)
{
	struct bcma_softc		*sc;
	const struct bhnd_chipid	*cid;
	struct resource			*erom_res;
	int				 error;
	int				 rid;

	sc = device_get_softc(dev);

	/* Map the EROM resource and enumerate our children. */
	cid = BHNDB_GET_CHIPID(device_get_parent(dev), dev);
	rid = 0;
	erom_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, cid->enum_addr,
		cid->enum_addr + BCMA_EROM_TABLE_SIZE, BCMA_EROM_TABLE_SIZE,
		RF_ACTIVE);
	if (erom_res == NULL) {
		device_printf(dev, "failed to allocate EROM resource\n");
		return (ENXIO);
	}

	error = bcma_add_children(dev, erom_res, BCMA_EROM_TABLE_START);

	/* Clean up */
	bus_release_resource(dev, SYS_RES_MEMORY, rid, erom_res);
	if (error)
		return (error);

	/* Initialize full bridge configuration */
	error = BHNDB_INIT_FULL_CONFIG(device_get_parent(dev), dev,
	    bhndb_bcma_priority_table);
	if (error)
		return (error);

	/* Ask our parent bridge to find the corresponding bridge core */
	sc->hostb_dev = BHNDB_FIND_HOSTB_DEVICE(device_get_parent(dev), dev);

	/* Call our superclass' implementation */
	return (bcma_attach(dev));
}

static int
bcma_bhndb_suspend_child(device_t dev, device_t child)
{
	struct bcma_devinfo	*dinfo;
	int			 error;

	if (device_get_parent(child) != dev)
		BUS_SUSPEND_CHILD(device_get_parent(dev), child);
	
	if (device_is_suspended(child))
		return (EBUSY);

	dinfo = device_get_ivars(child);

	/* Suspend the child */
	if ((error = bhnd_generic_br_suspend_child(dev, child)))
		return (error);

	/* Suspend child's agent resource  */
	if (dinfo->res_agent != NULL)
		BHNDB_SUSPEND_RESOURCE(device_get_parent(dev), dev,
		    SYS_RES_MEMORY, dinfo->res_agent->res);
	
	return (0);
}

static int
bcma_bhndb_resume_child(device_t dev, device_t child)
{
	struct bcma_devinfo	*dinfo;
	int			 error;

	if (device_get_parent(child) != dev)
		BUS_SUSPEND_CHILD(device_get_parent(dev), child);

	if (!device_is_suspended(child))
		return (EBUSY);

	dinfo = device_get_ivars(child);

	/* Resume child's agent resource  */
	if (dinfo->res_agent != NULL) {
		error = BHNDB_RESUME_RESOURCE(device_get_parent(dev), dev,
		    SYS_RES_MEMORY, dinfo->res_agent->res);
		if (error)
			return (error);
	}

	/* Resume the child */
	if ((error = bhnd_generic_br_resume_child(dev, child))) {
		/* On failure, re-suspend the agent resource */
		if (dinfo->res_agent != NULL) {
			BHNDB_SUSPEND_RESOURCE(device_get_parent(dev), dev,
			    SYS_RES_MEMORY, dinfo->res_agent->res);
		}

		return (error);
	}

	return (0);
}

static device_method_t bcma_bhndb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			bcma_bhndb_probe),
	DEVMETHOD(device_attach,		bcma_bhndb_attach),

	/* Bus interface */
	DEVMETHOD(bus_suspend_child,		bcma_bhndb_suspend_child),
	DEVMETHOD(bus_resume_child,		bcma_bhndb_resume_child),

	DEVMETHOD_END
};

DEFINE_CLASS_2(bhnd, bcma_bhndb_driver, bcma_bhndb_methods,
    sizeof(struct bcma_softc), bhnd_bhndb_driver, bcma_driver);

DRIVER_MODULE(bcma_bhndb, bhndb, bcma_bhndb_driver, bhnd_devclass, NULL, NULL);
 
MODULE_VERSION(bcma_bhndb, 1);
MODULE_DEPEND(bcma_bhndb, bcma, 1, 1, 1);
MODULE_DEPEND(bcma_bhndb, bhnd, 1, 1, 1);
MODULE_DEPEND(bcma_bhndb, bhndb, 1, 1, 1);
