/*-
 * Copyright (c) 2015-2016 Landon Fuller <landon@freebsd.org>
 * Copyright (c) 2017 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Landon Fuller
 * under sponsorship from the FreeBSD Foundation.
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

/*
 * bhnd(4) driver mix-in providing shared common methods for
 * bhnd bus devices attached via a MIPS root nexus.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/bhnd/bhndvar.h>
#include <dev/bhnd/bhnd_ids.h>

#include "bcm_machdep.h"

#include "bhnd_nexusvar.h"


/**
 * Default bhnd_nexus implementation of BHND_BUS_GET_SERVICE_REGISTRY().
 */
static struct bhnd_service_registry *
bhnd_nexus_get_service_registry(device_t dev, device_t child)
{
	struct bcm_platform *bp = bcm_get_platform();
	return (&bp->services);
}

/**
 * Default bhnd_nexus implementation of BHND_BUS_ACTIVATE_RESOURCE().
 */
static int
bhnd_nexus_activate_resource(device_t dev, device_t child, int type, int rid,
    struct bhnd_resource *r)
{
	int error;

	/* Always direct */
	if ((error = bus_activate_resource(child, type, rid, r->res)))
		return (error);

	r->direct = true;
	return (0);
}

/**
 * Default bhnd_nexus implementation of BHND_BUS_DEACTIVATE_RESOURCE().
 */
static int
bhnd_nexus_deactivate_resource(device_t dev, device_t child,
    int type, int rid, struct bhnd_resource *r)
{
	int error;

	/* Always direct */
	KASSERT(r->direct, ("indirect resource delegated to bhnd_nexus\n"));

	if ((error = bus_deactivate_resource(child, type, rid, r->res)))
		return (error);

	r->direct = false;
	return (0);
}

/**
 * Default bhnd_nexus implementation of BHND_BUS_IS_HW_DISABLED().
 */
static bool
bhnd_nexus_is_hw_disabled(device_t dev, device_t child)
{
	struct bcm_platform	*bp;
	struct bhnd_chipid	*cid;

	bp = bcm_get_platform();
	cid = &bp->cid;

	/* The BCM4706 low-cost package leaves secondary GMAC cores
	 * floating */
	if (cid->chip_id == BHND_CHIPID_BCM4706 &&
	    cid->chip_pkg == BHND_PKGID_BCM4706L &&
	    bhnd_get_device(child) == BHND_COREID_4706_GMAC &&
	    bhnd_get_core_unit(child) != 0)
	{
		return (true);
	}

	return (false);
}

/**
 * Default bhnd_nexus implementation of BHND_BUS_AGET_ATTACH_TYPE().
 */
static bhnd_attach_type
bhnd_nexus_get_attach_type(device_t dev, device_t child)
{
	return (BHND_ATTACH_NATIVE);
}

/**
 * Default bhnd_nexus implementation of BHND_BUS_GET_CHIPID().
 */
static const struct bhnd_chipid *
bhnd_nexus_get_chipid(device_t dev, device_t child)
{
	return (&bcm_get_platform()->cid);
}

/**
 * Default bhnd_nexus implementation of BHND_BUS_GET_INTR_COUNT().
 */
static int
bhnd_nexus_get_intr_count(device_t dev, device_t child)
{
	// TODO: arch-specific interrupt handling.
	return (0);
}

/**
 * Default bhnd_nexus implementation of BHND_BUS_ASSIGN_INTR().
 */
static int
bhnd_nexus_assign_intr(device_t dev, device_t child, int rid)
{
	uint32_t	ivec;
	int		error;

	if ((error = bhnd_get_core_ivec(child, rid, &ivec)))
		return (error);

	return (bus_set_resource(child, SYS_RES_IRQ, rid, ivec, 1));
}

static device_method_t bhnd_nexus_methods[] = {
	/* bhnd interface */
	DEVMETHOD(bhnd_bus_get_service_registry,bhnd_nexus_get_service_registry),
	DEVMETHOD(bhnd_bus_register_provider,	bhnd_bus_generic_sr_register_provider),
	DEVMETHOD(bhnd_bus_deregister_provider,	bhnd_bus_generic_sr_deregister_provider),
	DEVMETHOD(bhnd_bus_retain_provider,	bhnd_bus_generic_sr_retain_provider),
	DEVMETHOD(bhnd_bus_release_provider,	bhnd_bus_generic_sr_release_provider),
	DEVMETHOD(bhnd_bus_activate_resource,	bhnd_nexus_activate_resource),
	DEVMETHOD(bhnd_bus_deactivate_resource, bhnd_nexus_deactivate_resource),
	DEVMETHOD(bhnd_bus_is_hw_disabled,	bhnd_nexus_is_hw_disabled),
	DEVMETHOD(bhnd_bus_get_attach_type,	bhnd_nexus_get_attach_type),
	DEVMETHOD(bhnd_bus_get_chipid,		bhnd_nexus_get_chipid),
	DEVMETHOD(bhnd_bus_get_intr_count,	bhnd_nexus_get_intr_count),
	DEVMETHOD(bhnd_bus_assign_intr,		bhnd_nexus_assign_intr),

	DEVMETHOD_END
};

DEFINE_CLASS_0(bhnd, bhnd_nexus_driver, bhnd_nexus_methods,
    sizeof(struct bhnd_softc));
