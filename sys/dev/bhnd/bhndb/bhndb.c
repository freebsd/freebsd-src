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

/*
 * Abstract BHND Bridge Device Driver
 * 
 * Provides generic support for bridging from a parent bus (such as PCI) to
 * a BHND-compatible bus (e.g. bcma or siba).
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/bhnd/bhndvar.h>
#include <dev/bhnd/bhndreg.h>

#include <dev/bhnd/bhnd_erom.h>

#include <dev/bhnd/cores/chipc/chipcreg.h>
#include <dev/bhnd/nvram/bhnd_nvram.h>

#include "bhnd_chipc_if.h"
#include "bhnd_nvram_if.h"

#include "bhndbvar.h"
#include "bhndb_bus_if.h"
#include "bhndb_hwdata.h"
#include "bhndb_private.h"

/* Debugging flags */
static u_long bhndb_debug = 0;
TUNABLE_ULONG("hw.bhndb.debug", &bhndb_debug);

enum {
	BHNDB_DEBUG_PRIO = 1 << 0,
};

#define	BHNDB_DEBUG(_type)	(BHNDB_DEBUG_ ## _type & bhndb_debug)

static int			 bhndb_find_hostb_core(struct bhndb_softc *sc,
				     bhnd_erom_t *erom,
				     struct bhnd_core_info *core);

static bhnd_erom_class_t	*bhndb_probe_erom_class(struct bhndb_softc *sc,
				    struct bhnd_chipid *cid);

static int			 bhndb_init_full_config(struct bhndb_softc *sc,
				     bhnd_erom_class_t *eromcls);

static struct bhnd_core_info	*bhndb_get_bridge_core(struct bhndb_softc *sc);

static bool			 bhndb_hw_matches(struct bhndb_softc *sc,
				     struct bhnd_core_info *cores, u_int ncores,
				     const struct bhndb_hw *hw);

static int			 bhndb_init_region_cfg(struct bhndb_softc *sc,
				     bhnd_erom_t *erom,
				     struct bhndb_resources *r,
				     struct bhnd_core_info *cores, u_int ncores,
				     const struct bhndb_hw_priority *table);

static int			 bhndb_find_hwspec(struct bhndb_softc *sc,
				     struct bhnd_core_info *cores, u_int ncores,
				     const struct bhndb_hw **hw);

bhndb_addrspace			 bhndb_get_addrspace(struct bhndb_softc *sc,
				     device_t child);

static struct rman		*bhndb_get_rman(struct bhndb_softc *sc,
				     device_t child, int type);

static int			 bhndb_init_child_resource(struct resource *r,
				     struct resource *parent,
				     bhnd_size_t offset,
				     bhnd_size_t size);

static int			 bhndb_activate_static_region(
				     struct bhndb_softc *sc,
				     struct bhndb_region *region, 
				     device_t child, int type, int rid,
				     struct resource *r);

static int			 bhndb_try_activate_resource(
				     struct bhndb_softc *sc, device_t child,
				     int type, int rid, struct resource *r,
				     bool *indirect);

static inline struct bhndb_dw_alloc *bhndb_io_resource(struct bhndb_softc *sc,
					bus_addr_t addr, bus_size_t size,
					bus_size_t *offset);


/**
 * Default bhndb(4) implementation of DEVICE_PROBE().
 * 
 * This function provides the default bhndb implementation of DEVICE_PROBE(),
 * and is compatible with bhndb(4) bridges attached via bhndb_attach_bridge().
 */
int
bhndb_generic_probe(device_t dev)
{
	return (BUS_PROBE_NOWILDCARD);
}

static void
bhndb_probe_nomatch(device_t dev, device_t child)
{
	const char *name;

	name = device_get_name(child);
	if (name == NULL)
		name = "unknown device";

	device_printf(dev, "<%s> (no driver attached)\n", name);
}

static int
bhndb_print_child(device_t dev, device_t child)
{
	struct bhndb_softc	*sc;
	struct resource_list	*rl;
	int			 retval = 0;

	sc = device_get_softc(dev);

	retval += bus_print_child_header(dev, child);

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	if (rl != NULL) {
		retval += resource_list_print_type(rl, "mem", SYS_RES_MEMORY,
		    "%#jx");
		retval += resource_list_print_type(rl, "irq", SYS_RES_IRQ,
		    "%jd");
	}

	retval += bus_print_child_domain(dev, child);
	retval += bus_print_child_footer(dev, child);

	return (retval);
}

static int
bhndb_child_pnpinfo_str(device_t bus, device_t child, char *buf,
    size_t buflen)
{
	*buf = '\0';
	return (0);
}

static int
bhndb_child_location_str(device_t dev, device_t child, char *buf,
    size_t buflen)
{
	struct bhndb_softc *sc;

	sc = device_get_softc(dev);

	snprintf(buf, buflen, "base=0x%llx",
	    (unsigned long long) sc->chipid.enum_addr);
	return (0);
}

/**
 * Return the bridge core info. Will panic if the bridge core info has not yet
 * been populated during full bridge configuration.
 * 
 * @param sc BHNDB device state.
 */
static struct bhnd_core_info *
bhndb_get_bridge_core(struct bhndb_softc *sc)
{
	if (!sc->have_br_core)
		panic("bridge not yet fully configured; no bridge core!");

	return (&sc->bridge_core);
}

/**
 * Return true if @p cores matches the @p hw specification.
 *
 * @param sc BHNDB device state.
 * @param cores A device table to match against.
 * @param ncores The number of cores in @p cores.
 * @param hw The hardware description to be matched against.
 */
static bool
bhndb_hw_matches(struct bhndb_softc *sc, struct bhnd_core_info *cores,
    u_int ncores, const struct bhndb_hw *hw)
{
	for (u_int i = 0; i < hw->num_hw_reqs; i++) {
		const struct bhnd_core_match	*match;
		bool				 found;

		match =  &hw->hw_reqs[i];
		found = false;

		for (u_int d = 0; d < ncores; d++) {
			struct bhnd_core_info *core = &cores[d];
			
			if (BHNDB_IS_CORE_DISABLED(sc->dev, sc->bus_dev, core))
				continue;

			if (!bhnd_core_matches(core, match))
				continue;

			found = true;
			break;
		}

		if (!found)
			return (false);
	}

	return (true);
}

/**
 * Initialize the region maps and priority configuration in @p br using
 * the priority @p table and the set of cores enumerated by @p erom.
 * 
 * @param sc The bhndb device state.
 * @param br The resource state to be configured.
 * @param erom EROM parser used to enumerate @p cores.
 * @param cores All cores enumerated on the bridged bhnd bus.
 * @param ncores The length of @p cores.
 * @param table Hardware priority table to be used to determine the relative
 * priorities of per-core port resources.
 */
static int
bhndb_init_region_cfg(struct bhndb_softc *sc, bhnd_erom_t *erom,
    struct bhndb_resources *br, struct bhnd_core_info *cores, u_int ncores,
    const struct bhndb_hw_priority *table)
{
	const struct bhndb_hw_priority	*hp;
	bhnd_addr_t			 addr;
	bhnd_size_t			 size;
	size_t				 prio_low, prio_default, prio_high;
	int				 error;

	/* The number of port regions per priority band that must be accessible
	 * via dynamic register windows */
	prio_low = 0;
	prio_default = 0;
	prio_high = 0;

	/* 
	 * Register bridge regions covering all statically mapped ports.
	 */
	for (u_int i = 0; i < ncores; i++) {
		const struct bhndb_regwin	*regw;
		struct bhnd_core_info		*core;
		struct bhnd_core_match		 md;

		core = &cores[i];
		md = bhnd_core_get_match_desc(core);

		for (regw = br->cfg->register_windows;
		    regw->win_type != BHNDB_REGWIN_T_INVALID; regw++)
		{
			/* Only core windows are supported */
			if (regw->win_type != BHNDB_REGWIN_T_CORE)
				continue;

			/* Skip non-matching cores. */
			if (!bhndb_regwin_match_core(regw, core))
				continue;

			/* Fetch the base address of the mapped port */
			error = bhnd_erom_lookup_core_addr(erom, &md,
			    regw->d.core.port_type,
			    regw->d.core.port,
			    regw->d.core.region,
			    NULL,
			    &addr,
			    &size);
			if (error) {
				/* Skip non-applicable register windows */
				if (error == ENOENT)
					continue;

				return (error);
			}

			/*
			 * Always defer to the register window's size.
			 * 
			 * If the port size is smaller than the window size,
			 * this ensures that we fully utilize register windows
			 * larger than the referenced port.
			 * 
			 * If the port size is larger than the window size, this
			 * ensures that we do not directly map the allocations
			 * within the region to a too-small window.
			 */
			size = regw->win_size;

			/*
			 * Add to the bus region list.
			 * 
			 * The window priority for a statically mapped
			 * region is always HIGH.
			 */
			error = bhndb_add_resource_region(br, addr, size,
			    BHNDB_PRIORITY_HIGH, regw);
			if (error)
				return (error);
		}
	}

	/*
	 * Perform priority accounting and register bridge regions for all
	 * ports defined in the priority table
	 */
	for (u_int i = 0; i < ncores; i++) {
		struct bhndb_region	*region;
		struct bhnd_core_info	*core;
		struct bhnd_core_match	 md;

		core = &cores[i];
		md = bhnd_core_get_match_desc(core);

		/* 
		 * Skip priority accounting for cores that ...
		 */
		
		/* ... do not require bridge resources */
		if (BHNDB_IS_CORE_DISABLED(sc->dev, sc->bus_dev, core))
			continue;

		/* ... do not have a priority table entry */
		hp = bhndb_hw_priority_find_core(table, core);
		if (hp == NULL)
			continue;

		/* ... are explicitly disabled in the priority table. */
		if (hp->priority == BHNDB_PRIORITY_NONE)
			continue;

		/* Determine the number of dynamic windows required and
		 * register their bus_region entries. */
		for (u_int i = 0; i < hp->num_ports; i++) {
			const struct bhndb_port_priority *pp;

			pp = &hp->ports[i];

			/* Fetch the address+size of the mapped port. */
			error = bhnd_erom_lookup_core_addr(erom, &md,
			    pp->type, pp->port, pp->region,
			    NULL, &addr, &size);
			if (error) {
				/* Skip ports not defined on this device */
				if (error == ENOENT)
					continue;

				return (error);
			}

			/* Skip ports with an existing static mapping */
			region = bhndb_find_resource_region(br, addr, size);
			if (region != NULL && region->static_regwin != NULL)
				continue;

			/* Define a dynamic region for this port */
			error = bhndb_add_resource_region(br, addr, size,
			    pp->priority, NULL);
			if (error)
				return (error);

			/* Update port mapping counts */
			switch (pp->priority) {
			case BHNDB_PRIORITY_NONE:
				break;
			case BHNDB_PRIORITY_LOW:
				prio_low++;
				break;
			case BHNDB_PRIORITY_DEFAULT:
				prio_default++;
				break;
			case BHNDB_PRIORITY_HIGH:
				prio_high++;
				break;
			}
		}
	}

	/* Determine the minimum priority at which we'll allocate direct
	 * register windows from our dynamic pool */
	size_t prio_total = prio_low + prio_default + prio_high;
	if (prio_total <= br->dwa_count) {
		/* low+default+high priority regions get windows */
		br->min_prio = BHNDB_PRIORITY_LOW;

	} else if (prio_default + prio_high <= br->dwa_count) {
		/* default+high priority regions get windows */
		br->min_prio = BHNDB_PRIORITY_DEFAULT;

	} else {
		/* high priority regions get windows */
		br->min_prio = BHNDB_PRIORITY_HIGH;
	}

	if (BHNDB_DEBUG(PRIO)) {
		struct bhndb_region	*region;
		const char		*direct_msg, *type_msg;
		bhndb_priority_t	 prio, prio_min;

		prio_min = br->min_prio;
		device_printf(sc->dev, "min_prio: %d\n", prio_min);

		STAILQ_FOREACH(region, &br->bus_regions, link) {
			prio = region->priority;

			direct_msg = prio >= prio_min ? "direct" : "indirect";
			type_msg = region->static_regwin ? "static" : "dynamic";
	
			device_printf(sc->dev, "region 0x%llx+0x%llx priority "
			    "%u %s/%s\n",
			    (unsigned long long) region->addr, 
			    (unsigned long long) region->size,
			    region->priority,
			    direct_msg, type_msg);
		}
	}

	return (0);
}

/**
 * Find a hardware specification for @p dev.
 * 
 * @param sc The bhndb device state.
 * @param cores All cores enumerated on the bridged bhnd bus.
 * @param ncores The length of @p cores.
 * @param[out] hw On success, the matched hardware specification.
 * with @p dev.
 * 
 * @retval 0 success
 * @retval non-zero if an error occurs fetching device info for comparison.
 */
static int
bhndb_find_hwspec(struct bhndb_softc *sc, struct bhnd_core_info *cores,
    u_int ncores, const struct bhndb_hw **hw)
{
	const struct bhndb_hw	*next, *hw_table;

	/* Search for the first matching hardware config. */
	hw_table = BHNDB_BUS_GET_HARDWARE_TABLE(sc->parent_dev, sc->dev);
	for (next = hw_table; next->hw_reqs != NULL; next++) {
		if (!bhndb_hw_matches(sc, cores, ncores, next))
			continue;

		/* Found */
		*hw = next;
		return (0);
	}

	return (ENOENT);
}

/**
 * Helper function that must be called by subclass bhndb(4) drivers
 * when implementing DEVICE_ATTACH() before calling any bhnd(4) or bhndb(4)
 * APIs on the bridge device.
 * 
 * This function will add a bridged bhnd(4) child device with a device order of
 * BHND_PROBE_BUS. Any subclass bhndb(4) driver may use the BHND_PROBE_*
 * priority bands to add additional devices that will be attached in
 * their preferred order relative to the bridged bhnd(4) bus.
 * 
 * @param dev The bridge device to attach.
 * @param bridge_devclass The device class of the bridging core. This is used
 * to automatically detect the bridge core, and to disable additional bridge
 * cores (e.g. PCMCIA on a PCIe device).
 */
int
bhndb_attach(device_t dev, bhnd_devclass_t bridge_devclass)
{
	struct bhndb_devinfo		*dinfo;
	struct bhndb_softc		*sc;
	const struct bhndb_hwcfg	*cfg;
	bhnd_erom_class_t		*eromcls;
	int				 error;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->parent_dev = device_get_parent(dev);
	sc->bridge_class = bridge_devclass;

	BHNDB_LOCK_INIT(sc);
	
	/* Populate generic resource allocation state. */
	cfg = BHNDB_BUS_GET_GENERIC_HWCFG(sc->parent_dev, sc->dev);
	sc->bus_res = bhndb_alloc_resources(dev, sc->parent_dev, cfg);
	if (sc->bus_res == NULL) {
		return (ENXIO);
	}

	/* Allocate our host resources */
	if ((error = bhndb_alloc_host_resources(sc->bus_res)))
		goto failed;

	/* Probe for a usable EROM class for our bridged bhnd(4) bus and
	 * populate our chip identifier. */
	BHNDB_LOCK(sc);
	if ((eromcls = bhndb_probe_erom_class(sc, &sc->chipid)) == NULL) {
		BHNDB_UNLOCK(sc);

		device_printf(sc->dev, "device enumeration unsupported; no "
		    "compatible driver found\n");
		return (ENXIO);
	}
	BHNDB_UNLOCK(sc);

	/* Add our bridged bus device */
	sc->bus_dev = BUS_ADD_CHILD(dev, BHND_PROBE_BUS, "bhnd", -1);
	if (sc->bus_dev == NULL) {
		error = ENXIO;
		goto failed;
	}

	dinfo = device_get_ivars(sc->bus_dev);
	dinfo->addrspace = BHNDB_ADDRSPACE_BRIDGED;

	/* Enumerate the bridged device and fully initialize our bridged
	 * resource configuration */
	if ((error = bhndb_init_full_config(sc, eromcls))) {
		device_printf(sc->dev, "initializing full bridge "
		    "configuration failed: %d\n", error);
		goto failed;
	}

	return (0);

failed:
	BHNDB_LOCK_DESTROY(sc);

	if (sc->bus_res != NULL)
		bhndb_free_resources(sc->bus_res);

	return (error);
}


/**
 * Return a borrowed reference to the host resource mapping at least
 * BHND_DEFAULT_CORE_SIZE bytes at the first bus core, for use with
 * bhnd_erom_probe().
 * 
 * This may return a borrowed reference to a bhndb_dw_alloc-managed
 * resource; any additional resource mapping requests may invalidate this
 * borrowed reference.
 * 
 * @param sc BHNDB driver state.
 * @param[out] offset On success, the offset within the returned resource
 * at which the first bus core can be found.
 * 
 * @retval non-NULL success.
 * @retval NULL If no usable mapping could be found.
 */
static struct resource *
bhndb_erom_chipc_resource(struct bhndb_softc *sc, bus_size_t *offset)
{
	const struct bhndb_hwcfg	*cfg;
	struct bhndb_dw_alloc		*dwa;
	struct resource			*res;
	const struct bhndb_regwin	*win;

	BHNDB_LOCK_ASSERT(sc, MA_OWNED);

	cfg = sc->bus_res->cfg;

	/* Find a static register window mapping ChipCommon. */
	win = bhndb_regwin_find_core(cfg->register_windows, BHND_DEVCLASS_CC,
	    0, BHND_PORT_DEVICE, 0, 0);
	if (win != NULL) {
		if (win->win_size < BHND_DEFAULT_CORE_SIZE) {
			device_printf(sc->dev,
			    "chipcommon register window too small\n");
			return (NULL);
		}

		res = bhndb_find_regwin_resource(sc->bus_res, win);
		if (res == NULL) {
			device_printf(sc->dev,
			    "chipcommon register window not allocated\n");
			return (NULL);
		}

		*offset = win->win_offset;
		return (res);
	}
	
	/* We'll need to fetch and configure a dynamic window. We can assume a
	 * device without a static ChipCommon mapping uses the default siba(4)
	 * base address. */
	dwa = bhndb_io_resource(sc, BHND_DEFAULT_CHIPC_ADDR,
	    BHND_DEFAULT_CORE_SIZE, offset);
	if (dwa != NULL)
		return (dwa->parent_res);

	device_printf(sc->dev, "unable to map chipcommon registers; no usable "
	    "register window found\n");
	return (NULL);
}

/**
 * Probe all supported EROM classes, returning the best matching class
 * (or NULL if not found), writing the probed chip identifier to @p cid.
 * 
 * @param sc BHNDB driver state.
 * @param cid On success, the bridged chipset's chip identifier.
 */
static bhnd_erom_class_t *
bhndb_probe_erom_class(struct bhndb_softc *sc, struct bhnd_chipid *cid)
{
	devclass_t			 bhndb_devclass;
	const struct bhnd_chipid	*hint;
	struct resource			*res;
	bus_size_t			 res_offset;
	driver_t			**drivers;
	int				 drv_count;
	bhnd_erom_class_t		*erom_cls;
	int				 prio, result;

	BHNDB_LOCK_ASSERT(sc, MA_OWNED);

	erom_cls = NULL;
	prio = 0;

	/* Let our parent device provide a chipid hint */
	hint = BHNDB_BUS_GET_CHIPID(sc->parent_dev, sc->dev);

	/* Fetch a borrowed reference to the resource mapping ChipCommon. */
	res = bhndb_erom_chipc_resource(sc, &res_offset);
	if (res == NULL)
		return (NULL);

	/* Fetch all available drivers */
	bhndb_devclass = device_get_devclass(sc->dev);
	if (devclass_get_drivers(bhndb_devclass, &drivers, &drv_count) != 0)
		return (NULL);

	/* Enumerate the drivers looking for the best available EROM class */
	for (int i = 0; i < drv_count; i++) {
		struct bhnd_chipid	 pcid;
		bhnd_erom_class_t	*cls;

		cls = bhnd_driver_get_erom_class(drivers[i]);
		if (cls == NULL)
			continue;

		kobj_class_compile(cls);

		/* Probe the bus */
		result = bhnd_erom_probe(cls, &BHND_DIRECT_RESOURCE(res),
		    res_offset, hint, &pcid);

		/* The parser did not match if an error was returned */
		if (result > 0)
			continue;
		
		/* Check for a new highest priority match */
		if (erom_cls == NULL || result > prio) {
			prio = result;

			*cid = pcid;
			erom_cls = cls;
		}

		/* Terminate immediately on BUS_PROBE_SPECIFIC */
		if (result == BUS_PROBE_SPECIFIC)
			break;
	}

	return (erom_cls);
}

/* ascending core index comparison used by bhndb_find_hostb_core() */ 
static int
compare_core_index(const void *lhs, const void *rhs)
{
	u_int left = ((const struct bhnd_core_info *)lhs)->core_idx;
	u_int right = ((const struct bhnd_core_info *)rhs)->core_idx;

	if (left < right)
		return (-1);
	else if (left > right)
		return (1);
	else
		return (0);
}

/**
 * Search @p erom for the core serving as the bhnd host bridge.
 * 
 * This function uses a heuristic valid on all known PCI/PCIe/PCMCIA-bridged
 * bhnd(4) devices to determine the hostb core:
 * 
 * - The core must have a Broadcom vendor ID.
 * - The core devclass must match the bridge type.
 * - The core must be the first device on the bus with the bridged device
 *   class.
 * 
 * @param sc BHNDB device state.
 * @param erom The device enumeration table parser to be used to fetch
 * core info.
 * @param[out] core If found, the matching core info.
 * 
 * @retval 0 success
 * @retval ENOENT not found
 * @retval non-zero if an error occured fetching core info.
 */
static int
bhndb_find_hostb_core(struct bhndb_softc *sc, bhnd_erom_t *erom,
    struct bhnd_core_info *core)
{
	struct bhnd_core_match	 md;
	struct bhnd_core_info	*cores;
	u_int			 ncores;
	int			 error;

	if ((error = bhnd_erom_get_core_table(erom, &cores, &ncores)))
		return (error);

	/* Set up a match descriptor for the required device class. */
	md = (struct bhnd_core_match) {
		BHND_MATCH_CORE_CLASS(sc->bridge_class),
		BHND_MATCH_CORE_UNIT(0)
	};

	/* Ensure the table is sorted by core index value, ascending;
	 * the host bridge must be the absolute first matching device on the
	 * bus. */
	qsort(cores, ncores, sizeof(*cores), compare_core_index);

	/* Find the hostb core */
	error = ENOENT;
	for (u_int i = 0; i < ncores; i++) {
		if (bhnd_core_matches(&cores[i], &md)) {
			/* Found! */
			*core = cores[i];
			error = 0;
			break;
		}
	}

	/* Clean up */
	bhnd_erom_free_core_table(erom, cores);

	return (error);
}

/**
 * Identify the bridged device and perform final bridge resource configuration
 * based on capabilities of the enumerated device.
 * 
 * Any bridged resources allocated using the generic brige hardware
 * configuration must be released prior to calling this function.
 */
static int
bhndb_init_full_config(struct bhndb_softc *sc, bhnd_erom_class_t *eromcls)
{
	struct bhnd_core_info		*cores;
	struct bhndb_resources		*br;
	const struct bhndb_hw_priority	*hwprio;
	bhnd_erom_t			*erom;
	const struct bhndb_hw		*hw;
	u_int				 ncores;
	int				 error;

	erom = NULL;
	cores = NULL;
	br = NULL;

	/* Allocate EROM parser instance */
	erom = bhnd_erom_alloc(eromcls, &sc->chipid, sc->bus_dev, 0);
	if (erom == NULL) {
		device_printf(sc->dev, "failed to allocate device enumeration "
		    "table parser\n");
		return (ENXIO);
	}

	/* Look for our host bridge core */
	if ((error = bhndb_find_hostb_core(sc, erom, &sc->bridge_core))) {
		device_printf(sc->dev, "no host bridge core found\n");
		goto cleanup;
	} else {
		sc->have_br_core = true;
	}

	/* Fetch the bridged device's core table */
	if ((error = bhnd_erom_get_core_table(erom, &cores, &ncores))) {
		device_printf(sc->dev, "error fetching core table: %d\n",
		    error);
		goto cleanup;
	}

	/* Find our full register window configuration */
	if ((error = bhndb_find_hwspec(sc, cores, ncores, &hw))) {
		device_printf(sc->dev, "unable to identify device, "
		    " using generic bridge resource definitions\n");
		error = 0;
		goto cleanup;
	}

	if (bootverbose || BHNDB_DEBUG(PRIO))
		device_printf(sc->dev, "%s resource configuration\n", hw->name);

	/* Allocate new bridge resource state using the discovered hardware
	 * configuration */
	br = bhndb_alloc_resources(sc->dev, sc->parent_dev, hw->cfg);
	if (br == NULL) {
		device_printf(sc->dev,
		    "failed to allocate new resource state\n");
		error = ENOMEM;
		goto cleanup;
	}

	/* Populate our resource priority configuration */
	hwprio = BHNDB_BUS_GET_HARDWARE_PRIO(sc->parent_dev, sc->dev);
	error = bhndb_init_region_cfg(sc, erom, br, cores, ncores, hwprio);
	if (error) {
		device_printf(sc->dev, "failed to initialize resource "
		    "priority configuration: %d\n", error);
		goto cleanup;
	}

	/* The EROM parser holds a reference to the resource state we're
	 * about to invalidate */
	bhnd_erom_free_core_table(erom, cores);
	bhnd_erom_free(erom);

	cores = NULL;
	erom = NULL;

	/* Replace existing resource state */
	bhndb_free_resources(sc->bus_res);
	sc->bus_res = br;

	/* Pointer is now owned by sc->bus_res */
	br = NULL;

	/* Re-allocate host resources */
	if ((error = bhndb_alloc_host_resources(sc->bus_res))) {
		device_printf(sc->dev, "failed to reallocate bridge host "
		    "resources: %d\n", error);
		goto cleanup;
	}

	return (0);

cleanup:
	if (cores != NULL)
		bhnd_erom_free_core_table(erom, cores);

	if (erom != NULL)
		bhnd_erom_free(erom);

	if (br != NULL)
		bhndb_free_resources(br);

	return (error);
}

/**
 * Default bhndb(4) implementation of DEVICE_DETACH().
 * 
 * This function detaches any child devices, and if successful, releases all
 * resources held by the bridge device.
 */
int
bhndb_generic_detach(device_t dev)
{
	struct bhndb_softc	*sc;
	int			 error;

	sc = device_get_softc(dev);
	
	/* Detach children */
	if ((error = bus_generic_detach(dev)))
		return (error);

	/* Clean up our driver state. */
	bhndb_free_resources(sc->bus_res);
	
	BHNDB_LOCK_DESTROY(sc);

	return (0);
}

/**
 * Default bhndb(4) implementation of DEVICE_SUSPEND().
 * 
 * This function calls bus_generic_suspend() (or implements equivalent
 * behavior).
 */
int
bhndb_generic_suspend(device_t dev)
{
	return (bus_generic_suspend(dev));
}

/**
 * Default bhndb(4) implementation of DEVICE_RESUME().
 * 
 * This function calls bus_generic_resume() (or implements equivalent
 * behavior).
 */
int
bhndb_generic_resume(device_t dev)
{
	struct bhndb_softc	*sc;
	struct bhndb_resources	*bus_res;
	struct bhndb_dw_alloc	*dwa;
	int			 error;

	sc = device_get_softc(dev);
	bus_res = sc->bus_res;

	/* Guarantee that all in-use dynamic register windows are mapped to
	 * their previously configured target address. */
	BHNDB_LOCK(sc);
	for (size_t i = 0; i < bus_res->dwa_count; i++) {
		dwa = &bus_res->dw_alloc[i];
	
		/* Skip regions that were not previously used */
		if (bhndb_dw_is_free(bus_res, dwa) && dwa->target == 0x0)
			continue;

		/* Otherwise, ensure the register window is correct before
		 * any children attempt MMIO */
		error = BHNDB_SET_WINDOW_ADDR(dev, dwa->win, dwa->target);
		if (error)
			break;
	}
	BHNDB_UNLOCK(sc);

	/* Error restoring hardware state; children cannot be safely resumed */
	if (error) {
		device_printf(dev, "Unable to restore hardware configuration; "
		    "cannot resume: %d\n", error);
		return (error);
	}

	return (bus_generic_resume(dev));
}

/**
 * Default implementation of BHNDB_SUSPEND_RESOURCE.
 */
static void
bhndb_suspend_resource(device_t dev, device_t child, int type,
    struct resource *r)
{
	struct bhndb_softc	*sc;
	struct bhndb_dw_alloc	*dwa;

	sc = device_get_softc(dev);

	/* Non-MMIO resources (e.g. IRQs) are handled solely by our parent */
	if (type != SYS_RES_MEMORY)
		return;

	BHNDB_LOCK(sc);
	dwa = bhndb_dw_find_resource(sc->bus_res, r);
	if (dwa == NULL) {
		BHNDB_UNLOCK(sc);
		return;
	}

	if (BHNDB_DEBUG(PRIO))
		device_printf(child, "suspend resource type=%d 0x%jx+0x%jx\n",
		    type, rman_get_start(r), rman_get_size(r));

	/* Release the resource's window reference */
	bhndb_dw_release(sc->bus_res, dwa, r);
	BHNDB_UNLOCK(sc);
}

/**
 * Default implementation of BHNDB_RESUME_RESOURCE.
 */
static int
bhndb_resume_resource(device_t dev, device_t child, int type,
    struct resource *r)
{
	struct bhndb_softc	*sc;

	sc = device_get_softc(dev);

	/* Non-MMIO resources (e.g. IRQs) are handled solely by our parent */
	if (type != SYS_RES_MEMORY)
		return (0);

	/* Inactive resources don't require reallocation of bridge resources */
	if (!(rman_get_flags(r) & RF_ACTIVE))
		return (0);

	if (BHNDB_DEBUG(PRIO))
		device_printf(child, "resume resource type=%d 0x%jx+0x%jx\n",
		    type, rman_get_start(r), rman_get_size(r));

	return (bhndb_try_activate_resource(sc, rman_get_device(r), type,
	    rman_get_rid(r), r, NULL));
}

/**
 * Default bhndb(4) implementation of BUS_READ_IVAR().
 */
static int
bhndb_read_ivar(device_t dev, device_t child, int index,
    uintptr_t *result)
{
	return (ENOENT);
}

/**
 * Default bhndb(4) implementation of BUS_WRITE_IVAR().
 */
static int
bhndb_write_ivar(device_t dev, device_t child, int index,
    uintptr_t value)
{
	return (ENOENT);
}

/**
 * Return the address space for the given @p child device.
 */
bhndb_addrspace
bhndb_get_addrspace(struct bhndb_softc *sc, device_t child)
{
	struct bhndb_devinfo	*dinfo;
	device_t		 imd_dev;

	/* Find the directly attached parent of the requesting device */
	imd_dev = child;
	while (imd_dev != NULL && device_get_parent(imd_dev) != sc->dev)
		imd_dev = device_get_parent(imd_dev);

	if (imd_dev == NULL)
		panic("bhndb address space request for non-child device %s\n",
		     device_get_nameunit(child));

	dinfo = device_get_ivars(imd_dev);
	return (dinfo->addrspace);
}

/**
 * Return the rman instance for a given resource @p type, if any.
 * 
 * @param sc The bhndb device state.
 * @param child The requesting child.
 * @param type The resource type (e.g. SYS_RES_MEMORY, SYS_RES_IRQ, ...)
 */
static struct rman *
bhndb_get_rman(struct bhndb_softc *sc, device_t child, int type)
{	
	switch (bhndb_get_addrspace(sc, child)) {
	case BHNDB_ADDRSPACE_NATIVE:
		switch (type) {
		case SYS_RES_MEMORY:
			return (&sc->bus_res->ht_mem_rman);
		case SYS_RES_IRQ:
			return (NULL);
		default:
			return (NULL);
		};
		
	case BHNDB_ADDRSPACE_BRIDGED:
		switch (type) {
		case SYS_RES_MEMORY:
			return (&sc->bus_res->br_mem_rman);
		case SYS_RES_IRQ:
			return (NULL);
		default:
			return (NULL);
		};
	}

	/* Quieten gcc */
	return (NULL);
}

/**
 * Default implementation of BUS_ADD_CHILD()
 */
static device_t
bhndb_add_child(device_t dev, u_int order, const char *name, int unit)
{
	struct bhndb_devinfo	*dinfo;
	device_t		 child;
	
	child = device_add_child_ordered(dev, order, name, unit);
	if (child == NULL)
		return (NULL);

	dinfo = malloc(sizeof(struct bhndb_devinfo), M_BHND, M_NOWAIT);
	if (dinfo == NULL) {
		device_delete_child(dev, child);
		return (NULL);
	}

	dinfo->addrspace = BHNDB_ADDRSPACE_NATIVE;
	resource_list_init(&dinfo->resources);

	device_set_ivars(child, dinfo);

	return (child);
}

/**
 * Default implementation of BUS_CHILD_DELETED().
 */
static void
bhndb_child_deleted(device_t dev, device_t child)
{
	struct bhndb_devinfo *dinfo = device_get_ivars(child);
	if (dinfo != NULL) {
		resource_list_free(&dinfo->resources);
		free(dinfo, M_BHND);
	}

	device_set_ivars(child, NULL);
}

/**
 * Default implementation of BHNDB_GET_CHIPID().
 */
static const struct bhnd_chipid *
bhndb_get_chipid(device_t dev, device_t child)
{
	struct bhndb_softc *sc = device_get_softc(dev);
	return (&sc->chipid);
}

/**
 * Default implementation of BHNDB_IS_CORE_DISABLED().
 */
static bool
bhndb_is_core_disabled(device_t dev, device_t child,
    struct bhnd_core_info *core)
{
	struct bhndb_softc	*sc;
	struct bhnd_core_info	*bridge_core;

	sc = device_get_softc(dev);

	/* Try to defer to the bhndb bus parent */
	if (BHNDB_BUS_IS_CORE_DISABLED(sc->parent_dev, dev, core))
		return (true);

	/* Otherwise, we treat bridge-capable cores as unpopulated if they're
	 * not the configured host bridge */
	bridge_core = bhndb_get_bridge_core(sc);
	if (BHND_DEVCLASS_SUPPORTS_HOSTB(bhnd_core_class(core)))
		return (!bhnd_cores_equal(core, bridge_core));

	/* Assume the core is populated */
	return (false);
}

/**
 * Default bhndb(4) implementation of BHNDB_GET_HOSTB_CORE().
 * 
 * This function uses a heuristic valid on all known PCI/PCIe/PCMCIA-bridged
 * bhnd(4) devices.
 */
static int
bhndb_get_hostb_core(device_t dev, device_t child, struct bhnd_core_info *core)
{
	struct bhndb_softc *sc = device_get_softc(dev);

	*core = *bhndb_get_bridge_core(sc);
	return (0);
}

/**
 * Default bhndb(4) implementation of BUS_ALLOC_RESOURCE().
 */
static struct resource *
bhndb_alloc_resource(device_t dev, device_t child, int type,
    int *rid, rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct bhndb_softc		*sc;
	struct resource_list_entry	*rle;
	struct resource			*rv;
	struct rman			*rm;
	int				 error;
	bool				 passthrough, isdefault;

	sc = device_get_softc(dev);
	passthrough = (device_get_parent(child) != dev);
	isdefault = RMAN_IS_DEFAULT_RANGE(start, end);
	rle = NULL;

	/* Fetch the resource manager */
	rm = bhndb_get_rman(sc, child, type);
	if (rm == NULL) {
		/* Delegate to our parent device's bus; the requested
		 * resource type isn't handled locally. */
		return (BUS_ALLOC_RESOURCE(device_get_parent(sc->parent_dev),
		    child, type, rid,  start, end, count, flags));
	}

	/* Populate defaults */
	if (!passthrough && isdefault) {
		/* Fetch the resource list entry. */
		rle = resource_list_find(BUS_GET_RESOURCE_LIST(dev, child),
		    type, *rid);
		if (rle == NULL) {
			device_printf(dev,
			    "default resource %#x type %d for child %s "
			    "not found\n", *rid, type,
			    device_get_nameunit(child));
			
			return (NULL);
		}
		
		if (rle->res != NULL) {
			device_printf(dev,
			    "resource entry %#x type %d for child %s is busy\n",
			    *rid, type, device_get_nameunit(child));
			
			return (NULL);
		}

		start = rle->start;
		end = rle->end;
		count = ulmax(count, rle->count);
	}

	/* Validate resource addresses */
	if (start > end || count > ((end - start) + 1))
		return (NULL);

	/* Make our reservation */
	rv = rman_reserve_resource(rm, start, end, count, flags & ~RF_ACTIVE,
	    child);
	if (rv == NULL)
		return (NULL);
	
	rman_set_rid(rv, *rid);

	/* Activate */
	if (flags & RF_ACTIVE) {
		error = bus_activate_resource(child, type, *rid, rv);
		if (error) {
			device_printf(dev,
			    "failed to activate entry %#x type %d for "
				"child %s: %d\n",
			     *rid, type, device_get_nameunit(child), error);

			rman_release_resource(rv);

			return (NULL);
		}
	}

	/* Update child's resource list entry */
	if (rle != NULL) {
		rle->res = rv;
		rle->start = rman_get_start(rv);
		rle->end = rman_get_end(rv);
		rle->count = rman_get_size(rv);
	}

	return (rv);
}

/**
 * Default bhndb(4) implementation of BUS_RELEASE_RESOURCE().
 */
static int
bhndb_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct bhndb_softc		*sc;
	struct resource_list_entry	*rle;
	bool				 passthrough;
	int				 error;

	sc = device_get_softc(dev);
	passthrough = (device_get_parent(child) != dev);

	/* Delegate to our parent device's bus if the requested resource type
	 * isn't handled locally. */
	if (bhndb_get_rman(sc, child, type) == NULL) {
		return (BUS_RELEASE_RESOURCE(device_get_parent(sc->parent_dev),
		    child, type, rid, r));
	}

	/* Deactivate resources */
	if (rman_get_flags(r) & RF_ACTIVE) {
		error = BUS_DEACTIVATE_RESOURCE(dev, child, type, rid, r);
		if (error)
			return (error);
	}

	if ((error = rman_release_resource(r)))
		return (error);

	if (!passthrough) {
		/* Clean resource list entry */
		rle = resource_list_find(BUS_GET_RESOURCE_LIST(dev, child),
		    type, rid);
		if (rle != NULL)
			rle->res = NULL;
	}

	return (0);
}

/**
 * Default bhndb(4) implementation of BUS_ADJUST_RESOURCE().
 */
static int
bhndb_adjust_resource(device_t dev, device_t child, int type,
    struct resource *r, rman_res_t start, rman_res_t end)
{
	struct bhndb_softc		*sc;
	struct rman			*rm;
	rman_res_t			 mstart, mend;
	int				 error;
	
	sc = device_get_softc(dev);
	error = 0;

	/* Delegate to our parent device's bus if the requested resource type
	 * isn't handled locally. */
	rm = bhndb_get_rman(sc, child, type);
	if (rm == NULL) {
		return (BUS_ADJUST_RESOURCE(device_get_parent(sc->parent_dev),
		    child, type, r, start, end));
	}

	/* Verify basic constraints */
	if (end <= start)
		return (EINVAL);

	if (!rman_is_region_manager(r, rm))
		return (ENXIO);

	BHNDB_LOCK(sc);

	/* If not active, allow any range permitted by the resource manager */
	if (!(rman_get_flags(r) & RF_ACTIVE))
		goto done;

	/* Otherwise, the range is limited to the existing register window
	 * mapping */
	error = bhndb_find_resource_limits(sc->bus_res, r, &mstart, &mend);
	if (error)
		goto done;

	if (start < mstart || end > mend) {
		error = EINVAL;
		goto done;
	}

	/* Fall through */
done:
	if (!error)
		error = rman_adjust_resource(r, start, end);

	BHNDB_UNLOCK(sc);
	return (error);
}

/**
 * Initialize child resource @p r with a virtual address, tag, and handle
 * copied from @p parent, adjusted to contain only the range defined by
 * @p offsize and @p size.
 * 
 * @param r The register to be initialized.
 * @param parent The parent bus resource that fully contains the subregion.
 * @param offset The subregion offset within @p parent.
 * @param size The subregion size.
 * @p r.
 */
static int
bhndb_init_child_resource(struct resource *r,
    struct resource *parent, bhnd_size_t offset, bhnd_size_t size)
{
	bus_space_handle_t	bh, child_bh;
	bus_space_tag_t		bt;
	uintptr_t		vaddr;
	int			error;

	/* Fetch the parent resource's real bus values */
	vaddr = (uintptr_t) rman_get_virtual(parent);
	bt = rman_get_bustag(parent);
	bh = rman_get_bushandle(parent);

	/* Configure child resource with window-adjusted real bus values */
	vaddr += offset;
	error = bus_space_subregion(bt, bh, offset, size, &child_bh);
	if (error)
		return (error);

	rman_set_virtual(r, (void *) vaddr);
	rman_set_bustag(r, bt);
	rman_set_bushandle(r, child_bh);

	return (0);
}

/**
 * Attempt activation of a fixed register window mapping for @p child.
 * 
 * @param sc BHNDB device state.
 * @param region The static region definition capable of mapping @p r.
 * @param child A child requesting resource activation.
 * @param type Resource type.
 * @param rid Resource identifier.
 * @param r Resource to be activated.
 * 
 * @retval 0 if @p r was activated successfully
 * @retval ENOENT if no fixed register window was found.
 * @retval non-zero if @p r could not be activated.
 */
static int
bhndb_activate_static_region(struct bhndb_softc *sc,
    struct bhndb_region *region, device_t child, int type, int rid,
    struct resource *r)
{
	struct resource			*bridge_res;
	const struct bhndb_regwin	*win;
	bhnd_size_t			 parent_offset;
	rman_res_t			 r_start, r_size;
	int				 error;

	win = region->static_regwin;

	KASSERT(win != NULL && BHNDB_REGWIN_T_IS_STATIC(win->win_type),
	    ("can't activate non-static region"));

	r_start = rman_get_start(r);
	r_size = rman_get_size(r);

	/* Find the corresponding bridge resource */
	bridge_res = bhndb_find_regwin_resource(sc->bus_res, win);
	if (bridge_res == NULL)
		return (ENXIO);
	
	/* Calculate subregion offset within the parent resource */
	parent_offset = r_start - region->addr;
	parent_offset += win->win_offset;

	/* Configure resource with its real bus values. */
	error = bhndb_init_child_resource(r, bridge_res, parent_offset, r_size);
	if (error)
		return (error);

	/* Mark active */
	if ((error = rman_activate_resource(r)))
		return (error);

	return (0);
}

/**
 * Attempt to allocate/retain a dynamic register window for @p r, returning
 * the retained window.
 * 
 * @param sc The bhndb driver state.
 * @param r The resource for which a window will be retained.
 */
static struct bhndb_dw_alloc *
bhndb_retain_dynamic_window(struct bhndb_softc *sc, struct resource *r)
{
	struct bhndb_dw_alloc	*dwa;
	rman_res_t		 r_start, r_size;
	int			 error;

	BHNDB_LOCK_ASSERT(sc, MA_OWNED);

	r_start = rman_get_start(r);
	r_size = rman_get_size(r);

	/* Look for an existing dynamic window we can reference */
	dwa = bhndb_dw_find_mapping(sc->bus_res, r_start, r_size);
	if (dwa != NULL) {
		if (bhndb_dw_retain(sc->bus_res, dwa, r) == 0)
			return (dwa);

		return (NULL);
	}

	/* Otherwise, try to reserve a free window */
	dwa = bhndb_dw_next_free(sc->bus_res);
	if (dwa == NULL) {
		/* No free windows */
		return (NULL);
	}

	/* Window must be large enough to map the entire resource */
	if (dwa->win->win_size < rman_get_size(r))
		return (NULL);

	/* Set the window target */
	error = bhndb_dw_set_addr(sc->dev, sc->bus_res, dwa, rman_get_start(r),
	    rman_get_size(r));
	if (error) {
		device_printf(sc->dev, "dynamic window initialization "
			"for 0x%llx-0x%llx failed: %d\n",
			(unsigned long long) r_start,
			(unsigned long long) r_start + r_size - 1,
			error);
		return (NULL);
	}

	/* Add our reservation */
	if (bhndb_dw_retain(sc->bus_res, dwa, r))
		return (NULL);

	return (dwa);
}

/**
 * Activate a resource using any viable static or dynamic register window.
 * 
 * @param sc The bhndb driver state.
 * @param child The child holding ownership of @p r.
 * @param type The type of the resource to be activated.
 * @param rid The resource ID of @p r.
 * @param r The resource to be activated
 * @param[out] indirect On error and if not NULL, will be set to 'true' if
 * the caller should instead use an indirect resource mapping.
 * 
 * @retval 0 success
 * @retval non-zero activation failed.
 */
static int
bhndb_try_activate_resource(struct bhndb_softc *sc, device_t child, int type,
    int rid, struct resource *r, bool *indirect)
{
	struct bhndb_region	*region;
	struct bhndb_dw_alloc	*dwa;
	bhndb_priority_t	 dw_priority;
	rman_res_t		 r_start, r_size;
	rman_res_t		 parent_offset;
	int			 error;

	BHNDB_LOCK_ASSERT(sc, MA_NOTOWNED);

	/* Only MMIO resources can be mapped via register windows */
	if (type != SYS_RES_MEMORY)
		return (ENXIO);
	
	if (indirect)
		*indirect = false;
	
	r_start = rman_get_start(r);
	r_size = rman_get_size(r);

	/* Activate native addrspace resources using the host address space */
	if (bhndb_get_addrspace(sc, child) == BHNDB_ADDRSPACE_NATIVE) {
		struct resource *parent;

		/* Find the bridge resource referenced by the child */
		parent = bhndb_find_resource_range(sc->bus_res, r_start,
		    r_size);
		if (parent == NULL) {
			device_printf(sc->dev, "host resource not found "
			     "for 0x%llx-0x%llx\n",
			     (unsigned long long) r_start,
			     (unsigned long long) r_start + r_size - 1);
			return (ENOENT);
		}

		/* Initialize child resource with the real bus values */
		error = bhndb_init_child_resource(r, parent,
		    r_start - rman_get_start(parent), r_size);
		if (error)
			return (error);

		/* Try to activate child resource */
		return (rman_activate_resource(r));
	}

	/* Default to low priority */
	dw_priority = BHNDB_PRIORITY_LOW;

	/* Look for a bus region matching the resource's address range */
	region = bhndb_find_resource_region(sc->bus_res, r_start, r_size);
	if (region != NULL)
		dw_priority = region->priority;

	/* Prefer static mappings over consuming a dynamic windows. */
	if (region && region->static_regwin) {
		error = bhndb_activate_static_region(sc, region, child, type,
		    rid, r);
		if (error)
			device_printf(sc->dev, "static window allocation "
			     "for 0x%llx-0x%llx failed\n",
			     (unsigned long long) r_start,
			     (unsigned long long) r_start + r_size - 1);
		return (error);
	}

	/* A dynamic window will be required; is this resource high enough
	 * priority to be reserved a dynamic window? */
	if (dw_priority < sc->bus_res->min_prio) {
		if (indirect)
			*indirect = true;

		return (ENOMEM);
	}

	/* Find and retain a usable window */
	BHNDB_LOCK(sc); {
		dwa = bhndb_retain_dynamic_window(sc, r);
	} BHNDB_UNLOCK(sc);

	if (dwa == NULL) {
		if (indirect)
			*indirect = true;
		return (ENOMEM);
	}

	/* Configure resource with its real bus values. */
	parent_offset = dwa->win->win_offset;
	parent_offset += r_start - dwa->target;

	error = bhndb_init_child_resource(r, dwa->parent_res, parent_offset,
	    dwa->win->win_size);
	if (error)
		goto failed;

	/* Mark active */
	if ((error = rman_activate_resource(r)))
		goto failed;

	return (0);

failed:
	/* Release our region allocation. */
	BHNDB_LOCK(sc);
	bhndb_dw_release(sc->bus_res, dwa, r);
	BHNDB_UNLOCK(sc);

	return (error);
}

/**
 * Default bhndb(4) implementation of BUS_ACTIVATE_RESOURCE().
 *
 * Maps resource activation requests to a viable static or dynamic
 * register window, if any.
 */
static int
bhndb_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct bhndb_softc *sc = device_get_softc(dev);

	/* Delegate directly to our parent device's bus if the requested
	 * resource type isn't handled locally. */
	if (bhndb_get_rman(sc, child, type) == NULL) {
		return (BUS_ACTIVATE_RESOURCE(device_get_parent(sc->parent_dev),
		    child, type, rid, r));
	}

	return (bhndb_try_activate_resource(sc, child, type, rid, r, NULL));
}

/**
 * Default bhndb(4) implementation of BUS_DEACTIVATE_RESOURCE().
 */
static int
bhndb_deactivate_resource(device_t dev, device_t child, int type,
    int rid, struct resource *r)
{
	struct bhndb_dw_alloc	*dwa;
	struct bhndb_softc	*sc;
	struct rman		*rm;
	int			 error;

	sc = device_get_softc(dev);

	/* Delegate directly to our parent device's bus if the requested
	 * resource type isn't handled locally. */
	rm = bhndb_get_rman(sc, child, type);
	if (rm == NULL) {
		return (BUS_DEACTIVATE_RESOURCE(
		    device_get_parent(sc->parent_dev), child, type, rid, r));
	}

	/* Mark inactive */
	if ((error = rman_deactivate_resource(r)))
		return (error);

	/* Free any dynamic window allocation. */
	if (bhndb_get_addrspace(sc, child) == BHNDB_ADDRSPACE_BRIDGED) {
		BHNDB_LOCK(sc);
		dwa = bhndb_dw_find_resource(sc->bus_res, r);
		if (dwa != NULL)
			bhndb_dw_release(sc->bus_res, dwa, r);
		BHNDB_UNLOCK(sc);
	}

	return (0);
}

/**
 * Default bhndb(4) implementation of BUS_GET_RESOURCE_LIST().
 */
static struct resource_list *
bhndb_get_resource_list(device_t dev, device_t child)
{
	struct bhndb_devinfo *dinfo = device_get_ivars(child);
	return (&dinfo->resources);
}

/**
 * Default bhndb(4) implementation of BHND_BUS_ACTIVATE_RESOURCE().
 *
 * For BHNDB_ADDRSPACE_NATIVE children, all resources may be assumed to
 * be activated by the bridge.
 * 
 * For BHNDB_ADDRSPACE_BRIDGED children, attempts to activate a static register
 * window, a dynamic register window, or configures @p r as an indirect
 * resource -- in that order.
 */
static int
bhndb_activate_bhnd_resource(device_t dev, device_t child,
    int type, int rid, struct bhnd_resource *r)
{
	struct bhndb_softc	*sc;
	struct bhndb_region	*region;
	rman_res_t		 r_start, r_size;
	int 			 error;
	bool			 indirect;

	KASSERT(!r->direct,
	    ("direct flag set on inactive resource"));
	
	KASSERT(!(rman_get_flags(r->res) & RF_ACTIVE),
	    ("RF_ACTIVE set on inactive resource"));

	sc = device_get_softc(dev);

	/* Delegate directly to BUS_ACTIVATE_RESOURCE() if the requested
	 * resource type isn't handled locally. */
	if (bhndb_get_rman(sc, child, type) == NULL) {
		error = BUS_ACTIVATE_RESOURCE(dev, child, type, rid, r->res);
		if (error == 0)
			r->direct = true;
		return (error);
	}

	r_start = rman_get_start(r->res);
	r_size = rman_get_size(r->res);

	/* Verify bridged address range's resource priority, and skip direct
	 * allocation if the priority is too low. */
	if (bhndb_get_addrspace(sc, child) == BHNDB_ADDRSPACE_BRIDGED) {
		bhndb_priority_t r_prio;

		region = bhndb_find_resource_region(sc->bus_res, r_start,
		    r_size);
		if (region != NULL)
			r_prio = region->priority;
		else
			r_prio = BHNDB_PRIORITY_NONE;

		/* If less than the minimum dynamic window priority, this
		 * resource should always be indirect. */
		if (r_prio < sc->bus_res->min_prio)
			return (0);
	}

	/* Attempt direct activation */
	error = bhndb_try_activate_resource(sc, child, type, rid, r->res,
	    &indirect);
	if (!error) {
		r->direct = true;
	} else if (indirect) {
		/* The request was valid, but no viable register window is
		 * available; indirection must be employed. */
		error = 0;
		r->direct = false;
	}

	if (BHNDB_DEBUG(PRIO) &&
	    bhndb_get_addrspace(sc, child) == BHNDB_ADDRSPACE_BRIDGED)
	{
		device_printf(child, "activated 0x%llx-0x%llx as %s "
		    "resource\n",
		    (unsigned long long) r_start, 
		    (unsigned long long) r_start + r_size - 1,
		    r->direct ? "direct" : "indirect");
	}

	return (error);
};

/**
 * Default bhndb(4) implementation of BHND_BUS_DEACTIVATE_RESOURCE().
 */
static int
bhndb_deactivate_bhnd_resource(device_t dev, device_t child,
    int type, int rid, struct bhnd_resource *r)
{
	int error;

	/* Indirect resources don't require activation */
	if (!r->direct)
		return (0);

	KASSERT(rman_get_flags(r->res) & RF_ACTIVE,
	    ("RF_ACTIVE not set on direct resource"));

	/* Perform deactivation */
	error = BUS_DEACTIVATE_RESOURCE(dev, child, type, rid, r->res);
	if (!error)
		r->direct = false;

	return (error);
};

/**
 * Slow path for bhndb_io_resource().
 *
 * Iterates over the existing allocated dynamic windows looking for a viable
 * in-use region; the first matching region is returned.
 */
static struct bhndb_dw_alloc *
bhndb_io_resource_slow(struct bhndb_softc *sc, bus_addr_t addr,
    bus_size_t size, bus_size_t *offset)
{
	struct bhndb_resources	*br;
	struct bhndb_dw_alloc	*dwa;

	BHNDB_LOCK_ASSERT(sc, MA_OWNED);

	br = sc->bus_res;

	/* Search for an existing dynamic mapping of this address range.
	 * Static regions are not searched, as a statically mapped
	 * region would never be allocated as an indirect resource. */
	for (size_t i = 0; i < br->dwa_count; i++) {
		const struct bhndb_regwin *win;

		dwa = &br->dw_alloc[i];
		win = dwa->win;

		KASSERT(win->win_type == BHNDB_REGWIN_T_DYN,
			("invalid register window type"));

		/* Verify the range */
		if (addr < dwa->target)
			continue;

		if (addr + size > dwa->target + win->win_size)
			continue;

		/* Found */
		*offset = dwa->win->win_offset;
		*offset += addr - dwa->target;

		return (dwa);
	}

	/* not found */
	return (NULL);
}

/**
 * Return a borrowed reference to a bridge resource allocation record capable
 * of handling bus I/O requests of @p size at @p addr.
 * 
 * This will either return a  reference to an existing allocation
 * record mapping the requested space, or will configure and return a free
 * allocation record.
 * 
 * Will panic if a usable record cannot be found.
 * 
 * @param sc Bridge driver state.
 * @param addr The I/O target address.
 * @param size The size of the I/O operation to be performed at @p addr. 
 * @param[out] offset The offset within the returned resource at which
 * to perform the I/O request.
 */
static inline struct bhndb_dw_alloc *
bhndb_io_resource(struct bhndb_softc *sc, bus_addr_t addr, bus_size_t size,
    bus_size_t *offset)
{
	struct bhndb_resources	*br;
	struct bhndb_dw_alloc	*dwa;
	int			 error;

	BHNDB_LOCK_ASSERT(sc, MA_OWNED);

	br = sc->bus_res;

	/* Try to fetch a free window */
	dwa = bhndb_dw_next_free(br);

	/*
	 * If no dynamic windows are available, look for an existing
	 * region that maps the target range. 
	 * 
	 * If none are found, this is a child driver bug -- our window
	 * over-commit should only fail in the case where a child driver leaks
	 * resources, or perform operations out-of-order.
	 * 
	 * Broadcom HND chipsets are designed to not require register window
	 * swapping during execution; as long as the child devices are
	 * attached/detached correctly, using the hardware's required order
	 * of operations, there should always be a window available for the
	 * current operation.
	 */
	if (dwa == NULL) {
		dwa = bhndb_io_resource_slow(sc, addr, size, offset);
		if (dwa == NULL) {
			panic("register windows exhausted attempting to map "
			    "0x%llx-0x%llx\n", 
			    (unsigned long long) addr,
			    (unsigned long long) addr+size-1);
		}

		return (dwa);
	}

	/* Adjust the window if the I/O request won't fit in the current
	 * target range. */
	if (addr < dwa->target ||
	    addr > dwa->target + dwa->win->win_size ||
	    (dwa->target + dwa->win->win_size) - addr < size)
	{
		error = bhndb_dw_set_addr(sc->dev, sc->bus_res, dwa, addr,
		    size);
		if (error) {
		    panic("failed to set register window target mapping "
			    "0x%llx-0x%llx\n", 
			    (unsigned long long) addr,
			    (unsigned long long) addr+size-1);
		}
	}

	/* Calculate the offset and return */
	*offset = (addr - dwa->target) + dwa->win->win_offset;
	return (dwa);
}

/*
 * BHND_BUS_(READ|WRITE_* implementations
 */

/* bhndb_bus_(read|write) common implementation */
#define	BHNDB_IO_COMMON_SETUP(_io_size)				\
	struct bhndb_softc	*sc;				\
	struct bhndb_dw_alloc	*dwa;				\
	struct resource		*io_res;			\
	bus_size_t		 io_offset;			\
								\
	sc = device_get_softc(dev);				\
								\
	BHNDB_LOCK(sc);						\
	dwa = bhndb_io_resource(sc, rman_get_start(r->res) +	\
	    offset, _io_size, &io_offset);			\
	io_res = dwa->parent_res;				\
								\
	KASSERT(!r->direct,					\
	    ("bhnd_bus slow path used for direct resource"));	\
								\
	KASSERT(rman_get_flags(io_res) & RF_ACTIVE,		\
	    ("i/o resource is not active"));

#define	BHNDB_IO_COMMON_TEARDOWN()				\
	BHNDB_UNLOCK(sc);

/* Defines a bhndb_bus_read_* method implementation */
#define	BHNDB_IO_READ(_type, _name)				\
static _type							\
bhndb_bus_read_ ## _name (device_t dev, device_t child,		\
    struct bhnd_resource *r, bus_size_t offset)			\
{								\
	_type v;						\
	BHNDB_IO_COMMON_SETUP(sizeof(_type));			\
	v = bus_read_ ## _name (io_res, io_offset);		\
	BHNDB_IO_COMMON_TEARDOWN();				\
								\
	return (v);						\
}

/* Defines a bhndb_bus_write_* method implementation */
#define	BHNDB_IO_WRITE(_type, _name)				\
static void							\
bhndb_bus_write_ ## _name (device_t dev, device_t child,	\
    struct bhnd_resource *r, bus_size_t offset, _type value)	\
{								\
	BHNDB_IO_COMMON_SETUP(sizeof(_type));			\
	bus_write_ ## _name (io_res, io_offset, value);		\
	BHNDB_IO_COMMON_TEARDOWN();				\
}

/* Defines a bhndb_bus_(read|write|set)_(multi|region)_* method */
#define	BHNDB_IO_MISC(_type, _ptr, _op, _size)			\
static void							\
bhndb_bus_ ## _op ## _ ## _size (device_t dev,			\
    device_t child, struct bhnd_resource *r, bus_size_t offset,	\
    _type _ptr datap, bus_size_t count)				\
{								\
	BHNDB_IO_COMMON_SETUP(sizeof(_type) * count);		\
	bus_ ## _op ## _ ## _size (io_res, io_offset,		\
	    datap, count);					\
	BHNDB_IO_COMMON_TEARDOWN();				\
}

/* Defines a complete set of read/write methods */
#define	BHNDB_IO_METHODS(_type, _size)				\
	BHNDB_IO_READ(_type, _size)				\
	BHNDB_IO_WRITE(_type, _size)				\
								\
	BHNDB_IO_READ(_type, stream_ ## _size)			\
	BHNDB_IO_WRITE(_type, stream_ ## _size)			\
								\
	BHNDB_IO_MISC(_type, *, read_multi, _size)		\
	BHNDB_IO_MISC(_type, *, write_multi, _size)		\
								\
	BHNDB_IO_MISC(_type, *, read_multi_stream, _size)	\
	BHNDB_IO_MISC(_type, *, write_multi_stream, _size)	\
								\
	BHNDB_IO_MISC(_type,  , set_multi, _size)		\
	BHNDB_IO_MISC(_type,  , set_region, _size)		\
	BHNDB_IO_MISC(_type, *, read_region, _size)		\
	BHNDB_IO_MISC(_type, *, write_region, _size)		\
								\
	BHNDB_IO_MISC(_type, *, read_region_stream, _size)	\
	BHNDB_IO_MISC(_type, *, write_region_stream, _size)

BHNDB_IO_METHODS(uint8_t, 1);
BHNDB_IO_METHODS(uint16_t, 2);
BHNDB_IO_METHODS(uint32_t, 4);

/**
 * Default bhndb(4) implementation of BHND_BUS_BARRIER().
 */
static void 
bhndb_bus_barrier(device_t dev, device_t child, struct bhnd_resource *r,
    bus_size_t offset, bus_size_t length, int flags)
{
	BHNDB_IO_COMMON_SETUP(length);

	bus_barrier(io_res, io_offset + offset, length, flags);

	BHNDB_IO_COMMON_TEARDOWN();
}

/**
 * Default bhndb(4) implementation of BUS_GET_DMA_TAG().
 */
static bus_dma_tag_t
bhndb_get_dma_tag(device_t dev, device_t child)
{
	// TODO
	return (NULL);
}

static device_method_t bhndb_methods[] = {
	/* Device interface */ \
	DEVMETHOD(device_probe,			bhndb_generic_probe),
	DEVMETHOD(device_detach,		bhndb_generic_detach),
	DEVMETHOD(device_shutdown,		bus_generic_shutdown),
	DEVMETHOD(device_suspend,		bhndb_generic_suspend),
	DEVMETHOD(device_resume,		bhndb_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_probe_nomatch,		bhndb_probe_nomatch),
	DEVMETHOD(bus_print_child,		bhndb_print_child),
	DEVMETHOD(bus_child_pnpinfo_str,	bhndb_child_pnpinfo_str),
	DEVMETHOD(bus_child_location_str,	bhndb_child_location_str),
	DEVMETHOD(bus_add_child,		bhndb_add_child),
	DEVMETHOD(bus_child_deleted,		bhndb_child_deleted),

	DEVMETHOD(bus_alloc_resource,		bhndb_alloc_resource),
	DEVMETHOD(bus_release_resource,		bhndb_release_resource),
	DEVMETHOD(bus_activate_resource,	bhndb_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bhndb_deactivate_resource),

	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,		bus_generic_teardown_intr),
	DEVMETHOD(bus_config_intr,		bus_generic_config_intr),
	DEVMETHOD(bus_bind_intr,		bus_generic_bind_intr),
	DEVMETHOD(bus_describe_intr,		bus_generic_describe_intr),

	DEVMETHOD(bus_get_dma_tag,		bhndb_get_dma_tag),

	DEVMETHOD(bus_adjust_resource,		bhndb_adjust_resource),
	DEVMETHOD(bus_set_resource,		bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource,		bus_generic_rl_get_resource),
	DEVMETHOD(bus_delete_resource,		bus_generic_rl_delete_resource),
	DEVMETHOD(bus_get_resource_list,	bhndb_get_resource_list),

	DEVMETHOD(bus_read_ivar,		bhndb_read_ivar),
	DEVMETHOD(bus_write_ivar,		bhndb_write_ivar),

	/* BHNDB interface */
	DEVMETHOD(bhndb_get_chipid,		bhndb_get_chipid),
	DEVMETHOD(bhndb_is_core_disabled,	bhndb_is_core_disabled),
	DEVMETHOD(bhndb_get_hostb_core,		bhndb_get_hostb_core),
	DEVMETHOD(bhndb_suspend_resource,	bhndb_suspend_resource),
	DEVMETHOD(bhndb_resume_resource,	bhndb_resume_resource),

	/* BHND interface */
	DEVMETHOD(bhnd_bus_get_chipid,		bhndb_get_chipid),
	DEVMETHOD(bhnd_bus_activate_resource,	bhndb_activate_bhnd_resource),
	DEVMETHOD(bhnd_bus_deactivate_resource,	bhndb_deactivate_bhnd_resource),
	DEVMETHOD(bhnd_bus_get_nvram_var,	bhnd_bus_generic_get_nvram_var),
	DEVMETHOD(bhnd_bus_read_1,		bhndb_bus_read_1),
	DEVMETHOD(bhnd_bus_read_2,		bhndb_bus_read_2),
	DEVMETHOD(bhnd_bus_read_4,		bhndb_bus_read_4),
	DEVMETHOD(bhnd_bus_write_1,		bhndb_bus_write_1),
	DEVMETHOD(bhnd_bus_write_2,		bhndb_bus_write_2),
	DEVMETHOD(bhnd_bus_write_4,		bhndb_bus_write_4),

	DEVMETHOD(bhnd_bus_read_stream_1,	bhndb_bus_read_stream_1),
	DEVMETHOD(bhnd_bus_read_stream_2,	bhndb_bus_read_stream_2),
	DEVMETHOD(bhnd_bus_read_stream_4,	bhndb_bus_read_stream_4),
	DEVMETHOD(bhnd_bus_write_stream_1,	bhndb_bus_write_stream_1),
	DEVMETHOD(bhnd_bus_write_stream_2,	bhndb_bus_write_stream_2),
	DEVMETHOD(bhnd_bus_write_stream_4,	bhndb_bus_write_stream_4),

	DEVMETHOD(bhnd_bus_read_multi_1,	bhndb_bus_read_multi_1),
	DEVMETHOD(bhnd_bus_read_multi_2,	bhndb_bus_read_multi_2),
	DEVMETHOD(bhnd_bus_read_multi_4,	bhndb_bus_read_multi_4),
	DEVMETHOD(bhnd_bus_write_multi_1,	bhndb_bus_write_multi_1),
	DEVMETHOD(bhnd_bus_write_multi_2,	bhndb_bus_write_multi_2),
	DEVMETHOD(bhnd_bus_write_multi_4,	bhndb_bus_write_multi_4),
	
	DEVMETHOD(bhnd_bus_read_multi_stream_1,	bhndb_bus_read_multi_stream_1),
	DEVMETHOD(bhnd_bus_read_multi_stream_2,	bhndb_bus_read_multi_stream_2),
	DEVMETHOD(bhnd_bus_read_multi_stream_4,	bhndb_bus_read_multi_stream_4),
	DEVMETHOD(bhnd_bus_write_multi_stream_1,bhndb_bus_write_multi_stream_1),
	DEVMETHOD(bhnd_bus_write_multi_stream_2,bhndb_bus_write_multi_stream_2),
	DEVMETHOD(bhnd_bus_write_multi_stream_4,bhndb_bus_write_multi_stream_4),

	DEVMETHOD(bhnd_bus_set_multi_1,		bhndb_bus_set_multi_1),
	DEVMETHOD(bhnd_bus_set_multi_2,		bhndb_bus_set_multi_2),
	DEVMETHOD(bhnd_bus_set_multi_4,		bhndb_bus_set_multi_4),
	DEVMETHOD(bhnd_bus_set_region_1,	bhndb_bus_set_region_1),
	DEVMETHOD(bhnd_bus_set_region_2,	bhndb_bus_set_region_2),
	DEVMETHOD(bhnd_bus_set_region_4,	bhndb_bus_set_region_4),

	DEVMETHOD(bhnd_bus_read_region_1,	bhndb_bus_read_region_1),
	DEVMETHOD(bhnd_bus_read_region_2,	bhndb_bus_read_region_2),
	DEVMETHOD(bhnd_bus_read_region_4,	bhndb_bus_read_region_4),
	DEVMETHOD(bhnd_bus_write_region_1,	bhndb_bus_write_region_1),
	DEVMETHOD(bhnd_bus_write_region_2,	bhndb_bus_write_region_2),
	DEVMETHOD(bhnd_bus_write_region_4,	bhndb_bus_write_region_4),

	DEVMETHOD(bhnd_bus_read_region_stream_1,bhndb_bus_read_region_stream_1),
	DEVMETHOD(bhnd_bus_read_region_stream_2,bhndb_bus_read_region_stream_2),
	DEVMETHOD(bhnd_bus_read_region_stream_4,bhndb_bus_read_region_stream_4),
	DEVMETHOD(bhnd_bus_write_region_stream_1,bhndb_bus_write_region_stream_1),
	DEVMETHOD(bhnd_bus_write_region_stream_2,bhndb_bus_write_region_stream_2),
	DEVMETHOD(bhnd_bus_write_region_stream_4,bhndb_bus_write_region_stream_4),

	DEVMETHOD(bhnd_bus_barrier,		bhndb_bus_barrier),

	DEVMETHOD_END
};

devclass_t bhndb_devclass;

DEFINE_CLASS_0(bhndb, bhndb_driver, bhndb_methods, sizeof(struct bhndb_softc));

MODULE_VERSION(bhndb, 1);
MODULE_DEPEND(bhndb, bhnd, 1, 1, 1);
