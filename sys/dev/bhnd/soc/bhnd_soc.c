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
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/resource.h>

#include <dev/bhnd/bhnd_debug.h>
#include <dev/bhnd/bhndvar.h>
#include <dev/bhnd/bhndreg.h>
#include <dev/bhnd/bhndb/bhndb.h>
#include <dev/bhnd/soc/bhnd_soc.h>

#include "bhndb_if.h"

/*
 * **************************** VARIABLES *************************************
 */

struct resource_spec bhnd_soc_default_rspec = {SYS_RES_MEMORY, 0, RF_ACTIVE};

/*
 * **************************** PROTOTYPES ************************************
 */

static int	bhnd_soc_attach_bus(device_t dev, struct bhnd_soc_softc* sc);
static int	bhnd_soc_probe(device_t dev);
static int	bhnd_soc_attach(device_t dev);
int		bhnd_soc_attach_by_class(device_t parent, device_t *child,
		    int unit, devclass_t child_devclass);

/*
 * **************************** IMPLEMENTATION ********************************
 */

int
bhnd_soc_attach_by_class(device_t parent, device_t *child, int unit,
		devclass_t child_devclass)
{
	int error;
	struct bhnd_soc_devinfo* devinfo;

	*child = device_add_child(parent, devclass_get_name(child_devclass),
		unit);
	if (*child == NULL)
		return (ENXIO);

	devinfo = malloc(sizeof(struct bhnd_soc_devinfo*), M_BHND, M_NOWAIT);
	resource_list_init(&devinfo->resources);

	for (int i = 0; i < BHND_SOC_MAXNUM_CORES; i++)
		resource_list_add(&devinfo->resources, SYS_RES_MEMORY, i,
				BHND_SOC_RAM_OFFSET, BHND_SOC_RAM_SIZE, 1);

	device_set_ivars(*child, devinfo);

	error = device_probe_and_attach(*child);
	if (error && device_delete_child(parent, *child))
		BHND_ERROR_DEV(parent, "failed to detach bhndb child");

	return (error);
}

static int
bhnd_soc_attach_bus(device_t dev, struct bhnd_soc_softc* sc)
{
	int error;

	error = bhnd_read_chipid(dev, &bhnd_soc_default_rspec,
			BHND_DEFAULT_CHIPC_ADDR, &sc->chipid);

	if (error) {
		return (error);
	}

	return (bhnd_soc_attach_by_class(dev, &(sc->bus), -1, bhnd_devclass));
}

static int
bhnd_soc_probe(device_t dev)
{
	return (BUS_PROBE_GENERIC);
}

static int
bhnd_soc_attach(device_t dev)
{
	struct bhnd_soc_softc* sc;
	sc = device_get_softc(dev);
	sc->dev = dev;
	return (bhnd_soc_attach_bus(dev,sc));
}

static const struct bhnd_chipid *
bhnd_soc_get_chipid (device_t dev, device_t child)
{
	struct bhnd_soc_softc* sc;
	sc = device_get_softc(dev);
	return (&sc->chipid);
}

static struct resource_list *
bhnd_soc_get_rl(device_t dev, device_t child)
{
	struct bhnd_soc_devinfo *dinfo;
	dinfo = device_get_ivars(child);
	return (&dinfo->resources);
}

static struct bhnd_resource *
bhnd_soc_alloc_resource(device_t dev, device_t child, int type, int *rid,
		rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct bhnd_soc_softc	*sc;
	struct bhnd_resource	*br;
	int error;

	sc = device_get_softc(dev);

	/* Allocate resource wrapper */
	br = malloc(sizeof(struct bhnd_resource), M_BHND, M_NOWAIT|M_ZERO);
	if (br == NULL)
		return (NULL);

	BHND_TRACE_DEV(child,"trying to allocate resource %d: %jx-%jx (%ju)",
			*rid, start, end, count);

	/* Configure */
	br->direct = true;
	br->res = bus_alloc_resource(child, type, rid, start, end, count,
	    flags & ~RF_ACTIVE);
	if (br->res == NULL) {
		BHND_ERROR_DEV(child, "can't allocate resource %d: %jx-%jx (%ju)",
				*rid, start, end, count);
		goto failed;
	}

	if (flags & RF_ACTIVE) {
		BHND_TRACE_DEV(child, "trying to activate resource: %d", *rid);
		error = bhnd_activate_resource(child, type, *rid, br);
		if (error) {
			BHND_ERROR_DEV(child, "can't activate BHND resource %d:"
					"%jx-%jx (%ju) with error: %d",
					*rid, start, end, count, error);
			goto failed;
		}
	}

	return (br);

failed:
	if (br->res != NULL)
		bus_release_resource(child, type, *rid, br->res);

	free(br, M_BHND);
	return (NULL);
}

static int
bhnd_soc_activate_resource(device_t dev, device_t child, int type, int rid,
		struct bhnd_resource *r)
{
	int error;

	/*
	 * Fallback to direct
	 */
	error = bus_activate_resource(child, type, rid, r->res);
	if (error) {
		BHND_ERROR_DEV(child, "can't activate resource %d, error: %d",
				rman_get_rid(r->res), error);
		return (error);
	}
	r->direct = true;
	return (0);
}

static bool
bhnd_soc_is_hw_disabled(device_t dev, device_t child)
{
	return false;
}

static bhnd_attach_type
bhnd_soc_get_attach_type(device_t dev, device_t child)
{
	return (BHND_ATTACH_NATIVE);
}

/*
 * **************************** DRIVER METADATA ****************************
 */

static device_method_t bhnd_soc_methods[] = {
		//device interface
	DEVMETHOD(device_probe,		bhnd_soc_probe),
	DEVMETHOD(device_attach,	bhnd_soc_attach),
	//resources
	DEVMETHOD(bus_alloc_resource,	bus_generic_rl_alloc_resource),
	DEVMETHOD(bus_delete_resource,	bus_generic_rl_delete_resource),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_rl_release_resource),
	//intr
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_config_intr,	bus_generic_config_intr),
	DEVMETHOD(bus_bind_intr,	bus_generic_bind_intr),
	DEVMETHOD(bus_describe_intr,	bus_generic_describe_intr),

	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
	//resource list
	DEVMETHOD(bus_get_resource_list,	bhnd_soc_get_rl),

	//bhnd - BCMA allocates agent resources
	DEVMETHOD(bhnd_bus_alloc_resource,	bhnd_soc_alloc_resource),
	DEVMETHOD(bhnd_bus_activate_resource,	bhnd_soc_activate_resource),
	DEVMETHOD(bhnd_bus_is_hw_disabled,	bhnd_soc_is_hw_disabled),
	DEVMETHOD(bhnd_bus_get_chipid,		bhnd_soc_get_chipid),
	DEVMETHOD(bhnd_bus_get_attach_type,	bhnd_soc_get_attach_type),

	DEVMETHOD_END
};

devclass_t bhnd_soc_devclass;

DEFINE_CLASS_0(bhnd_soc, bhnd_soc_driver, bhnd_soc_methods,
		sizeof(struct bhnd_soc_softc));
EARLY_DRIVER_MODULE(bhnd_soc, nexus, bhnd_soc_driver, bhnd_soc_devclass, NULL,
    NULL, BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
