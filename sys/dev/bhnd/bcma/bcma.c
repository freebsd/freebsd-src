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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include "bcmavar.h"

#include "bcma_eromreg.h"
#include "bcma_eromvar.h"
#include <dev/bhnd/bhnd_core.h>

int
bcma_probe(device_t dev)
{
	device_set_desc(dev, "BCMA BHND bus");
	return (BUS_PROBE_DEFAULT);
}

int
bcma_attach(device_t dev)
{
	struct bcma_devinfo	*dinfo;
	device_t		*devs, child;
	int			 ndevs;
	int			 error;


	if ((error = device_get_children(dev, &devs, &ndevs)))
		return (error);

	/*
	 * Map our children's agent register block.
	 */
	for (int i = 0; i < ndevs; i++) {
		bhnd_addr_t	addr;
		bhnd_size_t	size;
		rman_res_t	r_start, r_count, r_end;

		child = devs[i];
		dinfo = device_get_ivars(child);

		KASSERT(!device_is_suspended(child),
		    ("bcma(4) stateful suspend handling requires that devices "
		        "not be suspended before bcma_attach()"));
		
		/* Verify that the agent register block exists and is
		 * mappable */
		if (bhnd_get_port_rid(child, BHND_PORT_AGENT, 0, 0) == -1)
			continue;

		/* Fetch the address of the agent register block */
		error = bhnd_get_region_addr(child, BHND_PORT_AGENT, 0, 0,
		    &addr, &size);
		if (error) {
			device_printf(dev, "failed fetching agent register "
			    "block address for core %d\n", i);
			goto cleanup;
		}

		/* Allocate the resource */
		r_start = addr;
		r_count = size;
		r_end = r_start + r_count - 1;

		dinfo->rid_agent = i + 1;
		dinfo->res_agent = BHND_BUS_ALLOC_RESOURCE(dev, dev,
		    SYS_RES_MEMORY, &dinfo->rid_agent, r_start, r_end, r_count,
		    RF_ACTIVE);
		if (dinfo->res_agent == NULL) {
			device_printf(dev, "failed allocating agent register "
			    "block for core %d\n", i);
			error = ENXIO;
			goto cleanup;
		}
	}

cleanup:
	free(devs, M_BHND);
	if (error)
		return (error);
	
	return (bhnd_generic_attach(dev));
}

int
bcma_detach(device_t dev)
{
	return (bhnd_generic_detach(dev));
}

static int
bcma_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
	const struct bcma_devinfo *dinfo;
	const struct bhnd_core_info *ci;
	
	dinfo = device_get_ivars(child);
	ci = &dinfo->corecfg->core_info;
	
	switch (index) {
	case BHND_IVAR_VENDOR:
		*result = ci->vendor;
		return (0);
	case BHND_IVAR_DEVICE:
		*result = ci->device;
		return (0);
	case BHND_IVAR_HWREV:
		*result = ci->hwrev;
		return (0);
	case BHND_IVAR_DEVICE_CLASS:
		*result = bhnd_core_class(ci);
		return (0);
	case BHND_IVAR_VENDOR_NAME:
		*result = (uintptr_t) bhnd_vendor_name(ci->vendor);
		return (0);
	case BHND_IVAR_DEVICE_NAME:
		*result = (uintptr_t) bhnd_core_name(ci);
		return (0);
	case BHND_IVAR_CORE_INDEX:
		*result = ci->core_idx;
		return (0);
	case BHND_IVAR_CORE_UNIT:
		*result = ci->unit;
		return (0);
	default:
		return (ENOENT);
	}
}

static int
bcma_write_ivar(device_t dev, device_t child, int index, uintptr_t value)
{
	switch (index) {
	case BHND_IVAR_VENDOR:
	case BHND_IVAR_DEVICE:
	case BHND_IVAR_HWREV:
	case BHND_IVAR_DEVICE_CLASS:
	case BHND_IVAR_VENDOR_NAME:
	case BHND_IVAR_DEVICE_NAME:
	case BHND_IVAR_CORE_INDEX:
	case BHND_IVAR_CORE_UNIT:
		return (EINVAL);
	default:
		return (ENOENT);
	}
}

static struct resource_list *
bcma_get_resource_list(device_t dev, device_t child)
{
	struct bcma_devinfo *dinfo = device_get_ivars(child);
	return (&dinfo->resources);
}

static device_t
bcma_find_hostb_device(device_t dev)
{
	struct bcma_softc *sc = device_get_softc(dev);

	/* This is set (or not) by the concrete bcma driver subclass. */
	return (sc->hostb_dev);
}

static int
bcma_reset_core(device_t dev, device_t child, uint16_t flags)
{
	struct bcma_devinfo *dinfo;

	if (device_get_parent(child) != dev)
		BHND_BUS_RESET_CORE(device_get_parent(dev), child, flags);

	dinfo = device_get_ivars(child);

	/* Can't reset the core without access to the agent registers */
	if (dinfo->res_agent == NULL)
		return (ENODEV);

	/* Start reset */
	bhnd_bus_write_4(dinfo->res_agent, BHND_RESET_CF, BHND_RESET_CF_ENABLE);
	bhnd_bus_read_4(dinfo->res_agent, BHND_RESET_CF);
	DELAY(10);

	/* Disable clock */
	bhnd_bus_write_4(dinfo->res_agent, BHND_CF, flags);
	bhnd_bus_read_4(dinfo->res_agent, BHND_CF);
	DELAY(10);

	/* Enable clocks & force clock gating */
	bhnd_bus_write_4(dinfo->res_agent, BHND_CF, BHND_CF_CLOCK_EN |
	    BHND_CF_FGC | flags);
	bhnd_bus_read_4(dinfo->res_agent, BHND_CF);
	DELAY(10);

	/* Complete reset */
	bhnd_bus_write_4(dinfo->res_agent, BHND_RESET_CF, 0);
	bhnd_bus_read_4(dinfo->res_agent, BHND_RESET_CF);
	DELAY(10);

	/* Release force clock gating */
	bhnd_bus_write_4(dinfo->res_agent, BHND_CF, BHND_CF_CLOCK_EN | flags);
	bhnd_bus_read_4(dinfo->res_agent, BHND_CF);
	DELAY(10);

	return (0);
}

static int
bcma_suspend_core(device_t dev, device_t child)
{
	struct bcma_devinfo *dinfo;

	if (device_get_parent(child) != dev)
		BHND_BUS_SUSPEND_CORE(device_get_parent(dev), child);

	dinfo = device_get_ivars(child);

	/* Can't suspend the core without access to the agent registers */
	if (dinfo->res_agent == NULL)
		return (ENODEV);

	// TODO - perform suspend

	return (ENXIO);
}

static u_int
bcma_get_port_count(device_t dev, device_t child, bhnd_port_type type)
{
	struct bcma_devinfo *dinfo;

	/* delegate non-bus-attached devices to our parent */
	if (device_get_parent(child) != dev)
		return (BHND_BUS_GET_PORT_COUNT(device_get_parent(dev), child,
		    type));

	dinfo = device_get_ivars(child);
	switch (type) {
	case BHND_PORT_DEVICE:
		return (dinfo->corecfg->num_dev_ports);
	case BHND_PORT_BRIDGE:
		return (dinfo->corecfg->num_bridge_ports);
	case BHND_PORT_AGENT:
		return (dinfo->corecfg->num_wrapper_ports);
	default:
		device_printf(dev, "%s: unknown type (%d)\n",
		    __func__,
		    type);
		return (0);
	}
}

static u_int
bcma_get_region_count(device_t dev, device_t child, bhnd_port_type type,
    u_int port_num)
{
	struct bcma_devinfo	*dinfo;
	struct bcma_sport_list	*ports;
	struct bcma_sport	*port;

	/* delegate non-bus-attached devices to our parent */
	if (device_get_parent(child) != dev)
		return (BHND_BUS_GET_REGION_COUNT(device_get_parent(dev), child,
		    type, port_num));

	dinfo = device_get_ivars(child);
	ports = bcma_corecfg_get_port_list(dinfo->corecfg, type);
	
	STAILQ_FOREACH(port, ports, sp_link) {
		if (port->sp_num == port_num)
			return (port->sp_num_maps);
	}

	/* not found */
	return (0);
}

static int
bcma_get_port_rid(device_t dev, device_t child, bhnd_port_type port_type,
    u_int port_num, u_int region_num)
{
	struct bcma_devinfo	*dinfo;
	struct bcma_map		*map;
	struct bcma_sport_list	*ports;
	struct bcma_sport	*port;
	
	dinfo = device_get_ivars(child);
	ports = bcma_corecfg_get_port_list(dinfo->corecfg, port_type);

	STAILQ_FOREACH(port, ports, sp_link) {
		if (port->sp_num != port_num)
			continue;

		STAILQ_FOREACH(map, &port->sp_maps, m_link)
			if (map->m_region_num == region_num)
				return map->m_rid;
	}

	return -1;
}

static int
bcma_decode_port_rid(device_t dev, device_t child, int type, int rid,
    bhnd_port_type *port_type, u_int *port_num, u_int *region_num)
{
	struct bcma_devinfo	*dinfo;
	struct bcma_map		*map;
	struct bcma_sport_list	*ports;
	struct bcma_sport	*port;

	dinfo = device_get_ivars(child);

	/* Ports are always memory mapped */
	if (type != SYS_RES_MEMORY)
		return (EINVAL);

	/* Starting with the most likely device list, search all three port
	 * lists */
	bhnd_port_type types[] = {
	    BHND_PORT_DEVICE, 
	    BHND_PORT_AGENT,
	    BHND_PORT_BRIDGE
	};

	for (int i = 0; i < nitems(types); i++) {
		ports = bcma_corecfg_get_port_list(dinfo->corecfg, types[i]);

		STAILQ_FOREACH(port, ports, sp_link) {
			STAILQ_FOREACH(map, &port->sp_maps, m_link) {
				if (map->m_rid != rid)
					continue;

				*port_type = port->sp_type;
				*port_num = port->sp_num;
				*region_num = map->m_region_num;
				return (0);
			}
		}
	}

	return (ENOENT);
}

static int
bcma_get_region_addr(device_t dev, device_t child, bhnd_port_type port_type,
    u_int port_num, u_int region_num, bhnd_addr_t *addr, bhnd_size_t *size)
{
	struct bcma_devinfo	*dinfo;
	struct bcma_map		*map;
	struct bcma_sport_list	*ports;
	struct bcma_sport	*port;
	
	dinfo = device_get_ivars(child);
	ports = bcma_corecfg_get_port_list(dinfo->corecfg, port_type);

	/* Search the port list */
	STAILQ_FOREACH(port, ports, sp_link) {
		if (port->sp_num != port_num)
			continue;

		STAILQ_FOREACH(map, &port->sp_maps, m_link) {
			if (map->m_region_num != region_num)
				continue;

			/* Found! */
			*addr = map->m_base;
			*size = map->m_size;
			return (0);
		}
	}

	return (ENOENT);
}

static struct bhnd_devinfo *
bcma_alloc_bhnd_dinfo(device_t dev)
{
	struct bcma_devinfo *dinfo = bcma_alloc_dinfo(dev);
	return ((struct bhnd_devinfo *)dinfo);
}

static void
bcma_free_bhnd_dinfo(device_t dev, struct bhnd_devinfo *dinfo)
{
	bcma_free_dinfo(dev, (struct bcma_devinfo *)dinfo);
}

/**
 * Scan a device enumeration ROM table, adding all valid discovered cores to
 * the bus.
 * 
 * @param bus The bcma bus.
 * @param erom_res An active resource mapping the EROM core.
 * @param erom_offset Base offset of the EROM core's register mapping.
 */
int
bcma_add_children(device_t bus, struct resource *erom_res, bus_size_t erom_offset)
{
	struct bcma_erom	 erom;
	struct bcma_corecfg	*corecfg;
	struct bcma_devinfo	*dinfo;
	device_t		 child;
	int			 error;

	corecfg = NULL;

	/* Initialize our reader */
	error = bcma_erom_open(&erom, erom_res, erom_offset);
	if (error)
		return (error);

	/* Add all cores. */
	while (!error) {
		/* Parse next core */
		error = bcma_erom_parse_corecfg(&erom, &corecfg);
		if (error && error == ENOENT) {
			return (0);
		} else if (error) {
			goto failed;
		}

		/* Add the child device */
		child = BUS_ADD_CHILD(bus, 0, NULL, -1);
		if (child == NULL) {
			error = ENXIO;
			goto failed;
		}

		/* Initialize device ivars */
		dinfo = device_get_ivars(child);
		if ((error = bcma_init_dinfo(bus, dinfo, corecfg)))
			goto failed;

		/* The dinfo instance now owns the corecfg value */
		corecfg = NULL;

		/* If pins are floating or the hardware is otherwise
		 * unpopulated, the device shouldn't be used. */
		if (bhnd_is_hw_disabled(child))
			device_disable(child);
	}

	/* Hit EOF parsing cores? */
	if (error == ENOENT)
		return (0);
	
failed:
	if (corecfg != NULL)
		bcma_free_corecfg(corecfg);

	return (error);
}


static device_method_t bcma_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			bcma_probe),
	DEVMETHOD(device_attach,		bcma_attach),
	DEVMETHOD(device_detach,		bcma_detach),
	
	/* Bus interface */
	DEVMETHOD(bus_read_ivar,		bcma_read_ivar),
	DEVMETHOD(bus_write_ivar,		bcma_write_ivar),
	DEVMETHOD(bus_get_resource_list,	bcma_get_resource_list),

	/* BHND interface */
	DEVMETHOD(bhnd_bus_find_hostb_device,	bcma_find_hostb_device),
	DEVMETHOD(bhnd_bus_alloc_devinfo,	bcma_alloc_bhnd_dinfo),
	DEVMETHOD(bhnd_bus_free_devinfo,	bcma_free_bhnd_dinfo),
	DEVMETHOD(bhnd_bus_reset_core,		bcma_reset_core),
	DEVMETHOD(bhnd_bus_suspend_core,	bcma_suspend_core),
	DEVMETHOD(bhnd_bus_get_port_count,	bcma_get_port_count),
	DEVMETHOD(bhnd_bus_get_region_count,	bcma_get_region_count),
	DEVMETHOD(bhnd_bus_get_port_rid,	bcma_get_port_rid),
	DEVMETHOD(bhnd_bus_decode_port_rid,	bcma_decode_port_rid),
	DEVMETHOD(bhnd_bus_get_region_addr,	bcma_get_region_addr),

	DEVMETHOD_END
};

DEFINE_CLASS_1(bhnd, bcma_driver, bcma_methods, sizeof(struct bcma_softc), bhnd_driver);
MODULE_VERSION(bcma, 1);
MODULE_DEPEND(bcma, bhnd, 1, 1, 1);
