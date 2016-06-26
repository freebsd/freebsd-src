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

#include <dev/bhnd/cores/chipc/chipcreg.h>

#include "sibareg.h"
#include "sibavar.h"

int
siba_probe(device_t dev)
{
	device_set_desc(dev, "SIBA BHND bus");
	return (BUS_PROBE_DEFAULT);
}

int
siba_attach(device_t dev)
{
	struct siba_devinfo	*dinfo;
	struct siba_softc	*sc;
	device_t		*devs;
	int			 ndevs;
	int			 error;
	
	sc = device_get_softc(dev);
	sc->dev = dev;

	/* Fetch references to the siba SIBA_CFG* blocks for all
	 * registered devices */
	if ((error = device_get_children(dev, &devs, &ndevs)))
		return (error);

	for (int i = 0; i < ndevs; i++) {
		struct siba_addrspace	*addrspace;

		dinfo = device_get_ivars(devs[i]);

		KASSERT(!device_is_suspended(devs[i]),
		    ("siba(4) stateful suspend handling requires that devices "
		        "not be suspended before siba_attach()"));

		/* Fetch the core register address space */
		addrspace = siba_find_addrspace(dinfo, BHND_PORT_DEVICE, 0, 0);
		if (addrspace == NULL) {
			device_printf(dev,
			    "missing device registers for core %d\n", i);
			error = ENXIO;
			goto cleanup;
		}

		/*
		 * Map the per-core configuration blocks
		 */
		KASSERT(dinfo->core_id.num_cfg_blocks <= SIBA_MAX_CFG,
		    ("config block count %u out of range", 
		        dinfo->core_id.num_cfg_blocks));

		for (u_int cfgidx = 0; cfgidx < dinfo->core_id.num_cfg_blocks;
		    cfgidx++)
		{
			rman_res_t	r_start, r_count, r_end;

			/* Determine the config block's address range; configuration
			 * blocks are allocated starting at SIBA_CFG0_OFFSET,
			 * growing downwards. */
			r_start = addrspace->sa_base + SIBA_CFG0_OFFSET;
			r_start -= cfgidx * SIBA_CFG_SIZE;

			r_count = SIBA_CFG_SIZE;
			r_end = r_start + r_count - 1;

			/* Allocate the config resource */
			dinfo->cfg_rid[cfgidx] = 0;
			dinfo->cfg[cfgidx] = BHND_BUS_ALLOC_RESOURCE(dev, dev,
			    SYS_RES_MEMORY, &dinfo->cfg_rid[cfgidx], r_start,
			    r_end, r_count, RF_ACTIVE);
	
			if (dinfo->cfg[cfgidx] == NULL) {
			     device_printf(dev, "failed allocating CFG_%u for "
			     "core %d\n", cfgidx, i);
			     error = ENXIO;
			     goto cleanup;
			}
		}
	}

cleanup:
	free(devs, M_BHND);
	if (error)
		return (error);

	/* Delegate remainder to standard bhnd method implementation */
	return (bhnd_generic_attach(dev));
}

int
siba_detach(device_t dev)
{
	return (bhnd_generic_detach(dev));
}

int
siba_resume(device_t dev)
{
	return (bhnd_generic_resume(dev));
}

int
siba_suspend(device_t dev)
{
	return (bhnd_generic_suspend(dev));
}

static int
siba_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
	const struct siba_devinfo *dinfo;
	const struct bhnd_core_info *cfg;
	
	dinfo = device_get_ivars(child);
	cfg = &dinfo->core_id.core_info;
	
	switch (index) {
	case BHND_IVAR_VENDOR:
		*result = cfg->vendor;
		return (0);
	case BHND_IVAR_DEVICE:
		*result = cfg->device;
		return (0);
	case BHND_IVAR_HWREV:
		*result = cfg->hwrev;
		return (0);
	case BHND_IVAR_DEVICE_CLASS:
		*result = bhnd_core_class(cfg);
		return (0);
	case BHND_IVAR_VENDOR_NAME:
		*result = (uintptr_t) bhnd_vendor_name(cfg->vendor);
		return (0);
	case BHND_IVAR_DEVICE_NAME:
		*result = (uintptr_t) bhnd_core_name(cfg);
		return (0);
	case BHND_IVAR_CORE_INDEX:
		*result = cfg->core_idx;
		return (0);
	case BHND_IVAR_CORE_UNIT:
		*result = cfg->unit;
		return (0);
	default:
		return (ENOENT);
	}
}

static int
siba_write_ivar(device_t dev, device_t child, int index, uintptr_t value)
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
siba_get_resource_list(device_t dev, device_t child)
{
	struct siba_devinfo *dinfo = device_get_ivars(child);
	return (&dinfo->resources);
}

static device_t
siba_find_hostb_device(device_t dev)
{
	struct siba_softc *sc = device_get_softc(dev);

	/* This is set (or not) by the concrete siba driver subclass. */
	return (sc->hostb_dev);
}

static int
siba_reset_core(device_t dev, device_t child, uint16_t flags)
{
	struct siba_devinfo *dinfo;

	if (device_get_parent(child) != dev)
		BHND_BUS_RESET_CORE(device_get_parent(dev), child, flags);

	dinfo = device_get_ivars(child);

	/* Can't reset the core without access to the CFG0 registers */
	if (dinfo->cfg[0] == NULL)
		return (ENODEV);

	// TODO - perform reset

	return (ENXIO);
}

static int
siba_suspend_core(device_t dev, device_t child)
{
	struct siba_devinfo *dinfo;

	if (device_get_parent(child) != dev)
		BHND_BUS_SUSPEND_CORE(device_get_parent(dev), child);

	dinfo = device_get_ivars(child);

	/* Can't suspend the core without access to the CFG0 registers */
	if (dinfo->cfg[0] == NULL)
		return (ENODEV);

	// TODO - perform suspend

	return (ENXIO);
}


static u_int
siba_get_port_count(device_t dev, device_t child, bhnd_port_type type)
{
	struct siba_devinfo *dinfo;

	/* delegate non-bus-attached devices to our parent */
	if (device_get_parent(child) != dev)
		return (BHND_BUS_GET_PORT_COUNT(device_get_parent(dev), child,
		    type));

	dinfo = device_get_ivars(child);
	return (siba_addrspace_port_count(dinfo));
}

static u_int
siba_get_region_count(device_t dev, device_t child, bhnd_port_type type,
    u_int port)
{
	struct siba_devinfo	*dinfo;

	/* delegate non-bus-attached devices to our parent */
	if (device_get_parent(child) != dev)
		return (BHND_BUS_GET_REGION_COUNT(device_get_parent(dev), child,
		    type, port));

	dinfo = device_get_ivars(child);
	if (!siba_is_port_valid(dinfo, type, port))
		return (0);

	return (siba_addrspace_region_count(dinfo, port));
}

static int
siba_get_port_rid(device_t dev, device_t child, bhnd_port_type port_type,
    u_int port_num, u_int region_num)
{
	struct siba_devinfo	*dinfo;
	struct siba_addrspace	*addrspace;

	/* delegate non-bus-attached devices to our parent */
	if (device_get_parent(child) != dev)
		return (BHND_BUS_GET_PORT_RID(device_get_parent(dev), child,
		    port_type, port_num, region_num));

	dinfo = device_get_ivars(child);
	addrspace = siba_find_addrspace(dinfo, port_type, port_num, region_num);
	if (addrspace == NULL)
		return (-1);

	return (addrspace->sa_rid);
}

static int
siba_decode_port_rid(device_t dev, device_t child, int type, int rid,
    bhnd_port_type *port_type, u_int *port_num, u_int *region_num)
{
	struct siba_devinfo	*dinfo;

	/* delegate non-bus-attached devices to our parent */
	if (device_get_parent(child) != dev)
		return (BHND_BUS_DECODE_PORT_RID(device_get_parent(dev), child,
		    type, rid, port_type, port_num, region_num));

	dinfo = device_get_ivars(child);

	/* Ports are always memory mapped */
	if (type != SYS_RES_MEMORY)
		return (EINVAL);

	for (int i = 0; i < dinfo->core_id.num_addrspace; i++) {
		if (dinfo->addrspace[i].sa_rid != rid)
			continue;

		*port_type = BHND_PORT_DEVICE;
		*port_num = siba_addrspace_port(i);
		*region_num = siba_addrspace_region(i);
		return (0);
	}

	/* Not found */
	return (ENOENT);
}

static int
siba_get_region_addr(device_t dev, device_t child, bhnd_port_type port_type,
    u_int port_num, u_int region_num, bhnd_addr_t *addr, bhnd_size_t *size)
{
	struct siba_devinfo	*dinfo;
	struct siba_addrspace	*addrspace;

	/* delegate non-bus-attached devices to our parent */
	if (device_get_parent(child) != dev) {
		return (BHND_BUS_GET_REGION_ADDR(device_get_parent(dev), child,
		    port_type, port_num, region_num, addr, size));
	}

	dinfo = device_get_ivars(child);
	addrspace = siba_find_addrspace(dinfo, port_type, port_num, region_num);
	if (addrspace == NULL)
		return (ENOENT);

	*addr = addrspace->sa_base;
	*size = addrspace->sa_size - addrspace->sa_bus_reserved;
	return (0);
}


/**
 * Register all address space mappings for @p di.
 *
 * @param dev The siba bus device.
 * @param di The device info instance on which to register all address
 * space entries.
 * @param r A resource mapping the enumeration table block for @p di.
 */
static int
siba_register_addrspaces(device_t dev, struct siba_devinfo *di,
    struct resource *r)
{
	struct siba_core_id	*cid;
	uint32_t		 addr;
	uint32_t		 size;
	int			 error;

	cid = &di->core_id;


	/* Register the device address space entries */
	for (uint8_t i = 0; i < di->core_id.num_addrspace; i++) {
		uint32_t	adm;
		u_int		adm_offset;
		uint32_t	bus_reserved;

		/* Determine the register offset */
		adm_offset = siba_admatch_offset(i);
		if (adm_offset == 0) {
		    device_printf(dev, "addrspace %hhu is unsupported", i);
		    return (ENODEV);
		}

		/* Fetch the address match register value */
		adm = bus_read_4(r, adm_offset);

		/* Parse the value */
		if ((error = siba_parse_admatch(adm, &addr, &size))) {
			device_printf(dev, "failed to decode address "
			    " match register value 0x%x\n", adm);
			return (error);
		}

		/* If this is the device's core/enumeration addrespace,
		 * reserve the Sonics configuration register blocks for the
		 * use of our bus. */
		bus_reserved = 0;
		if (i == SIBA_CORE_ADDRSPACE)
			bus_reserved = cid->num_cfg_blocks * SIBA_CFG_SIZE;

		/* Append the region info */
		error = siba_append_dinfo_region(di, i, addr, size,
		    bus_reserved);
		if (error)
			return (error);
	}

	return (0);
}

static struct bhnd_devinfo *
siba_alloc_bhnd_dinfo(device_t dev)
{
	struct siba_devinfo *dinfo = siba_alloc_dinfo(dev);
	return ((struct bhnd_devinfo *)dinfo);
}

static void
siba_free_bhnd_dinfo(device_t dev, struct bhnd_devinfo *dinfo)
{
	siba_free_dinfo(dev, (struct siba_devinfo *)dinfo);
}

/**
 * Scan the core table and add all valid discovered cores to
 * the bus.
 * 
 * @param dev The siba bus device.
 * @param chipid The chip identifier, if the device does not provide a
 * ChipCommon core. Should o NULL otherwise.
 */
int
siba_add_children(device_t dev, const struct bhnd_chipid *chipid)
{
	struct bhnd_chipid	 ccid;
	struct bhnd_core_info	*cores;
	struct siba_devinfo	*dinfo;
	struct resource		*r;
	int			 rid;
	int			 error;

	dinfo = NULL;
	cores = NULL;
	r = NULL;
	
	/*
	 * Try to determine the number of device cores via the ChipCommon
	 * identification registers.
	 * 
	 * A small number of very early devices do not include a ChipCommon
	 * core, in which case our caller must supply the chip identification
	 * information via a non-NULL chipid parameter.
	 */
	if (chipid == NULL) {
		uint32_t	idhigh, ccreg;
		uint16_t	vendor, device;
		uint8_t		ccrev;

		/* Map the first core's register block. If the ChipCommon core
		 * exists, it will always be the first core. */
		rid = 0;
		r = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
		    SIBA_CORE_ADDR(0), SIBA_CORE_SIZE, 
		    SIBA_CORE_ADDR(0) + SIBA_CORE_SIZE - 1,
		    RF_ACTIVE);

		/* Identify the core */
		idhigh = bus_read_4(r, SB0_REG_ABS(SIBA_CFG0_IDHIGH));
		vendor = SIBA_REG_GET(idhigh, IDH_VENDOR);
		device = SIBA_REG_GET(idhigh, IDH_DEVICE);
		ccrev = SIBA_IDH_CORE_REV(idhigh);

		if (vendor != OCP_VENDOR_BCM || device != BHND_COREID_CC) {
			device_printf(dev,
			    "cannot identify device: no chipcommon core "
			    "found\n");
			error = ENXIO;
			goto cleanup;
		}

		/* Identify the chipset */
		ccreg = bus_read_4(r, CHIPC_ID);
		ccid = bhnd_parse_chipid(ccreg, SIBA_ENUM_ADDR);

		if (!CHIPC_NCORES_MIN_HWREV(ccrev)) {
			switch (ccid.chip_id) {
			case BHND_CHIPID_BCM4306:
				ccid.ncores = 6;
				break;
			case BHND_CHIPID_BCM4704:
				ccid.ncores = 9;
				break;
			case BHND_CHIPID_BCM5365:
				/*
				* BCM5365 does support ID_NUMCORE in at least
				* some of its revisions, but for unknown
				* reasons, Broadcom's drivers always exclude
				* the ChipCommon revision (0x5) used by BCM5365
				* from the set of revisions supporting
				* ID_NUMCORE, and instead supply a fixed value.
				* 
				* Presumably, at least some of these devices
				* shipped with a broken ID_NUMCORE value.
				*/
				ccid.ncores = 7;
				break;
			default:
				device_printf(dev, "unable to determine core "
				    "count for unrecognized chipset 0x%hx\n",
				    ccid.chip_id);
				error = ENXIO;
				goto cleanup;
			}
		}

		chipid = &ccid;
		bus_release_resource(dev, SYS_RES_MEMORY, rid, r);
	}

	/* Allocate our temporary core table and enumerate all cores */
	cores = malloc(sizeof(*cores) * chipid->ncores, M_BHND, M_NOWAIT);
	if (cores == NULL)
		return (ENOMEM);

	/* Add all cores. */
	for (u_int i = 0; i < chipid->ncores; i++) {
		struct siba_core_id	 cid;
		device_t		 child;
		uint32_t		 idhigh, idlow;
		rman_res_t		 r_count, r_end, r_start;

		/* Map the core's register block */
		rid = 0;
		r_start = SIBA_CORE_ADDR(i);
		r_count = SIBA_CORE_SIZE;
		r_end = r_start + SIBA_CORE_SIZE - 1;
		r = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, r_start,
		    r_end, r_count, RF_ACTIVE);
		if (r == NULL) {
			error = ENXIO;
			goto cleanup;
		}

		/* Add the child device */
		child = BUS_ADD_CHILD(dev, 0, NULL, -1);
		if (child == NULL) {
			error = ENXIO;
			goto cleanup;
		}
		
		/* Read the core info */
		idhigh = bus_read_4(r, SB0_REG_ABS(SIBA_CFG0_IDHIGH));
		idlow = bus_read_4(r, SB0_REG_ABS(SIBA_CFG0_IDLOW));

		cid = siba_parse_core_id(idhigh, idlow, i, 0);
		cores[i] = cid.core_info;

		/* Determine unit number */
		for (u_int j = 0; j < i; j++) {
			if (cores[j].vendor == cores[i].vendor &&
			    cores[j].device == cores[i].device)
				cores[i].unit++;
		}

		/* Initialize per-device bus info */
		if ((dinfo = device_get_ivars(child)) == NULL) {
			error = ENXIO;
			goto cleanup;
		}

		if ((error = siba_init_dinfo(dev, dinfo, &cid)))
			goto cleanup;

		/* Register the core's address space(s). */
		if ((error = siba_register_addrspaces(dev, dinfo, r)))
			goto cleanup;

		/* If pins are floating or the hardware is otherwise
		 * unpopulated, the device shouldn't be used. */
		if (bhnd_is_hw_disabled(child))
			device_disable(child);
				
		/* Release our resource */
		bus_release_resource(dev, SYS_RES_MEMORY, rid, r);
		r = NULL;
	}
	
cleanup:
	if (cores != NULL)
		free(cores, M_BHND);

	if (r != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, rid, r);

	return (error);
}

static device_method_t siba_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			siba_probe),
	DEVMETHOD(device_attach,		siba_attach),
	DEVMETHOD(device_detach,		siba_detach),
	DEVMETHOD(device_resume,		siba_resume),
	DEVMETHOD(device_suspend,		siba_suspend),
	
	/* Bus interface */
	DEVMETHOD(bus_read_ivar,		siba_read_ivar),
	DEVMETHOD(bus_write_ivar,		siba_write_ivar),
	DEVMETHOD(bus_get_resource_list,	siba_get_resource_list),

	/* BHND interface */
	DEVMETHOD(bhnd_bus_find_hostb_device,	siba_find_hostb_device),
	DEVMETHOD(bhnd_bus_alloc_devinfo,	siba_alloc_bhnd_dinfo),
	DEVMETHOD(bhnd_bus_free_devinfo,	siba_free_bhnd_dinfo),
	DEVMETHOD(bhnd_bus_reset_core,		siba_reset_core),
	DEVMETHOD(bhnd_bus_suspend_core,	siba_suspend_core),
	DEVMETHOD(bhnd_bus_get_port_count,	siba_get_port_count),
	DEVMETHOD(bhnd_bus_get_region_count,	siba_get_region_count),
	DEVMETHOD(bhnd_bus_get_port_rid,	siba_get_port_rid),
	DEVMETHOD(bhnd_bus_decode_port_rid,	siba_decode_port_rid),
	DEVMETHOD(bhnd_bus_get_region_addr,	siba_get_region_addr),

	DEVMETHOD_END
};

DEFINE_CLASS_1(bhnd, siba_driver, siba_methods, sizeof(struct siba_softc), bhnd_driver);

MODULE_VERSION(siba, 1);
MODULE_DEPEND(siba, bhnd, 1, 1, 1);
