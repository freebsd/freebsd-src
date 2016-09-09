/*-
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


/*
 * bhnd(4) driver mix-in providing shared common methods for
 * bhnd bus devices attached via a root nexus.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/bhnd/bhnd_ids.h>
#include <dev/bhnd/cores/chipc/chipcreg.h>

#include "bhnd_nexusvar.h"

static const struct resource_spec bhnd_nexus_res_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },	/* chipc registers */
	{ -1,			0,	0 }
};

/**
 * Map ChipCommon's register block and read the chip identifier data.
 * 
 * @param dev A bhnd_nexus device.
 * @param chipid On success, will be populated with the chip identifier.
 * @retval 0 success
 * @retval non-zero An error occurred reading the chip identifier..
 */
int
bhnd_nexus_read_chipid(device_t dev, struct bhnd_chipid *chipid)
{
	struct resource_spec	rspec[nitems(bhnd_nexus_res_spec)];
	int			error;

	memcpy(rspec, bhnd_nexus_res_spec, sizeof(rspec));
	error = bhnd_read_chipid(dev, rspec, 0, chipid);
	if (error)
		device_printf(dev, "error %d reading chip ID\n", error);

	return (error);
}

static bool
bhnd_nexus_is_hw_disabled(device_t dev, device_t child)
{
	return (false);
}

static bhnd_attach_type
bhnd_nexus_get_attach_type(device_t dev, device_t child)
{
	return (BHND_ATTACH_NATIVE);
}

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

static int
bhnd_nexus_get_intr_count(device_t dev, device_t child)
{
	// TODO: arch-specific interrupt handling.
	return (0);
}

static device_method_t bhnd_nexus_methods[] = {
	/* bhnd interface */
	DEVMETHOD(bhnd_bus_activate_resource,	bhnd_nexus_activate_resource),
	DEVMETHOD(bhnd_bus_deactivate_resource, bhnd_nexus_deactivate_resource),
	DEVMETHOD(bhnd_bus_is_hw_disabled,	bhnd_nexus_is_hw_disabled),
	DEVMETHOD(bhnd_bus_get_attach_type,	bhnd_nexus_get_attach_type),

	DEVMETHOD(bhnd_bus_get_intr_count,	bhnd_nexus_get_intr_count),

	DEVMETHOD_END
};

DEFINE_CLASS_0(bhnd, bhnd_nexus_driver, bhnd_nexus_methods,
    sizeof(struct bhnd_softc));
