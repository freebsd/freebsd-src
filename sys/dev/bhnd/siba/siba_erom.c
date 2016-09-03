/*-
 * Copyright (c) 2015-2016 Landon Fuller <landonf@FreeBSD.org>
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

#include <machine/bus.h>

#include <dev/bhnd/bhnd_erom.h>

#include <dev/bhnd/cores/chipc/chipcreg.h>

#include "sibareg.h"
#include "sibavar.h"

struct siba_erom;

static int	siba_erom_init_static(bhnd_erom_t *erom, bus_space_tag_t bst,
		    bus_space_handle_t bsh);
static void	siba_erom_fini(bhnd_erom_t *erom);

static uint32_t	siba_erom_read_4(struct siba_erom *sc, u_int core_idx,
		    bus_size_t offset);
static int	siba_erom_read_chipid(struct siba_erom *sc,
		    bus_addr_t enum_addr, struct bhnd_chipid *cid);

struct siba_erom {
	struct bhnd_erom	 obj;
	u_int			 ncores;	/**< core count */

	/* resource state */
	device_t	 	 dev;		/**< parent dev to use for resource allocations,
						     or NULL if initialized with bst/bsh */
	struct bhnd_resource	*res;		/**< siba bus mapping, or NULL */
	int			 rid;		/**< siba bus maping resource ID */

	/* bus tag state */
	bus_space_tag_t		 bst;		/**< chipc bus tag */
	bus_space_handle_t	 bsh;		/**< chipc bus handle */
};

#define	EROM_LOG(sc, fmt, ...)	do {					\
	if (sc->dev != NULL) {						\
		device_printf(sc->dev, "%s: " fmt, __FUNCTION__,	\
		    ##__VA_ARGS__);					\
	} else {							\
		printf("%s: " fmt, __FUNCTION__, ##__VA_ARGS__);	\
	}								\
} while(0)

static uint32_t
siba_erom_read_4(struct siba_erom *sc, u_int core_idx, bus_size_t offset)
{
	bus_size_t core_offset;

	/* Sanity check core index and offset */
	if (core_idx >= sc->ncores)
		panic("core index %u out of range (ncores=%u)", core_idx,
		    sc->ncores);

	if (offset > SIBA_CORE_SIZE - sizeof(uint32_t))
		panic("invalid core offset %#jx", (uintmax_t)offset);

	/* Perform read */
	core_offset = SIBA_CORE_OFFSET(core_idx) + offset;
	if (sc->res != NULL)
		return (bhnd_bus_read_4(sc->res, core_offset));
	else
		return (bus_space_read_4(sc->bst, sc->bsh, core_offset));
}

/** Fetch and parse a siba core's identification registers */
static struct siba_core_id
siba_erom_parse_core_id(struct siba_erom *sc, u_int core_idx, int unit)
{
	uint32_t idhigh, idlow;

	idhigh = siba_erom_read_4(sc, core_idx, SB0_REG_ABS(SIBA_CFG0_IDHIGH));
	idlow = siba_erom_read_4(sc, core_idx, SB0_REG_ABS(SIBA_CFG0_IDLOW));

	return (siba_parse_core_id(idhigh, idlow, core_idx, unit));
}

/** Fetch and parse the chip identification register */
static int
siba_erom_read_chipid(struct siba_erom *sc, bus_addr_t enum_addr,
    struct bhnd_chipid *cid)
{
	struct siba_core_id	ccid;
	uint32_t		idreg;

	/* Identify the chipcommon core */
	ccid = siba_erom_parse_core_id(sc, 0, 0);
	if (ccid.core_info.vendor != BHND_MFGID_BCM ||
	    ccid.core_info.device != BHND_COREID_CC)
	{
		EROM_LOG(sc,
		    "first core not chipcommon (vendor=%#hx, core=%#hx)\n",
		    ccid.core_info.vendor, ccid.core_info.device);
		return (ENXIO);
	}

	/* Identify the chipset */
	idreg = siba_erom_read_4(sc, 0, CHIPC_ID);
	*cid = bhnd_parse_chipid(idreg, enum_addr);

	/* Fix up the core count in-place */
	return (bhnd_chipid_fixed_ncores(cid, ccid.core_info.hwrev,
	    &cid->ncores));
}

static int
siba_erom_init_common(struct siba_erom *sc)
{
	struct bhnd_chipid	cid;
	int			error;

	/* There's always at least one core */
	sc->ncores = 1;

	/* Identify the chipset */
	if ((error = siba_erom_read_chipid(sc, SIBA_ENUM_ADDR, &cid)))
		return (error);

	/* Verify the chip type */
	if (cid.chip_type != BHND_CHIPTYPE_SIBA)
		return (ENXIO);

	/*
	 * gcc hack: ensure bhnd_chipid.ncores cannot exceed SIBA_MAX_CORES
	 * without triggering build failure due to -Wtype-limits
	 *
	 * if (cid.ncores > SIBA_MAX_CORES)
	 *      return (EINVAL)
	 */
	_Static_assert((2^sizeof(cid.ncores)) <= SIBA_MAX_CORES,
	    "ncores could result in over-read of backing resource");

	/* Update our core count */
	sc->ncores = cid.ncores;

	return (0);
}

static int
siba_erom_init(bhnd_erom_t *erom, device_t parent, int rid,
    bus_addr_t enum_addr)
{
	struct siba_erom *sc = (struct siba_erom *)erom;

	sc->dev = parent;
	sc->rid = rid;

	sc->res = bhnd_alloc_resource(sc->dev, SYS_RES_MEMORY, &sc->rid,
	    enum_addr, enum_addr + SIBA_ENUM_SIZE -1, SIBA_ENUM_SIZE,
	    RF_ACTIVE|RF_SHAREABLE);
	if (sc->res == NULL)
		return (ENOMEM);

	return (siba_erom_init_common(sc));
}

static int
siba_erom_probe_static(bhnd_erom_class_t *cls, bus_space_tag_t bst,
     bus_space_handle_t bsh, bus_addr_t paddr, struct bhnd_chipid *cid)
{
	struct siba_erom 	sc;
	uint32_t		idreg;
	uint8_t			chip_type;
	int			error;

	idreg = bus_space_read_4(bst, bsh, CHIPC_ID);
	chip_type = CHIPC_GET_BITS(idreg, CHIPC_ID_BUS);

	if (chip_type != BHND_CHIPTYPE_SIBA)
		return (ENXIO);

	/* Initialize a static EROM instance that we can use to fetch
	 * the chip identifier */
	if ((error = siba_erom_init_static((bhnd_erom_t *)&sc, bst, bsh)))
		return (error);

	/* Try to read the chip ID, clean up the static instance */
	error = siba_erom_read_chipid(&sc, paddr, cid);
	siba_erom_fini((bhnd_erom_t *)&sc);
	if (error)
		return (error);

	return (BUS_PROBE_DEFAULT);
}

static int
siba_erom_init_static(bhnd_erom_t *erom, bus_space_tag_t bst,
     bus_space_handle_t bsh)
{
	struct siba_erom *sc = (struct siba_erom *)erom;

	sc->dev = NULL;
	sc->rid = -1;
	sc->res = NULL;
	sc->bst = bst;
	sc->bsh = bsh;

	return (siba_erom_init_common(sc));
}

static void
siba_erom_fini(bhnd_erom_t *erom)
{
	struct siba_erom *sc = (struct siba_erom *)erom;

	if (sc->res != NULL) {
		bhnd_release_resource(sc->dev, SYS_RES_MEMORY, sc->rid,
		    sc->res);

		sc->res = NULL;
		sc->rid = -1;
	}
}

static int
siba_erom_lookup_core(bhnd_erom_t *erom, const struct bhnd_core_match *desc,
    struct bhnd_core_info *core)
{
	struct siba_erom	*sc;
	struct bhnd_core_match	 imatch;

	sc = (struct siba_erom *)erom;

	/* We can't determine a core's unit number during the initial scan. */
	imatch = *desc;
	imatch.m.match.core_unit = 0;

	/* Locate the first matching core */
	for (u_int i = 0; i < sc->ncores; i++) {
		struct siba_core_id	sid;
		struct bhnd_core_info	ci;

		/* Read the core info */
		sid = siba_erom_parse_core_id(sc, i, 0);
		ci = sid.core_info;

		/* Check for initial match */
		if (!bhnd_core_matches(&ci, &imatch))
			continue;

		/* Re-scan preceding cores to determine the unit number. */
		for (u_int j = 0; j < i; j++) {
			sid = siba_erom_parse_core_id(sc, i, 0);

			/* Bump the unit number? */
			if (sid.core_info.vendor == ci.vendor &&
			    sid.core_info.device == ci.device)
				ci.unit++;
		}

		/* Check for full match against now-valid unit number */
		if (!bhnd_core_matches(&ci, desc))
			continue;

		/* Matching core found */
		*core = ci;
		return (0);
	}

	/* Not found */
	return (ENOENT);
}

static int
siba_erom_lookup_core_addr(bhnd_erom_t *erom, const struct bhnd_core_match *desc,
    bhnd_port_type type, u_int port, u_int region, struct bhnd_core_info *info,
    bhnd_addr_t *addr, bhnd_size_t *size)
{
	struct siba_erom	*sc;
	struct bhnd_core_info	 core;
	struct siba_core_id	 sid;
	uint32_t		 am, am_addr, am_size;
	u_int			 am_offset;
	u_int			 addrspace;
	int			 error;

	sc = (struct siba_erom *)erom;

	/* Locate the requested core */
	if ((error = siba_erom_lookup_core(erom, desc, &core)))
		return (error);

	/* Fetch full siba core ident */
	sid = siba_erom_parse_core_id(sc, core.core_idx, core.unit);

	/* Is port valid? */
	if (!siba_is_port_valid(sid.num_addrspace, type, port))
		return (ENOENT);

	/* Is region valid? */
	if (region >= siba_addrspace_region_count(sid.num_addrspace, port))
		return (ENOENT);

	/* Map the bhnd port values to a siba addrspace index */
	error = siba_addrspace_index(sid.num_addrspace, type, port, region,
	   &addrspace);
	if (error)
		return (error);

	/* Determine the register offset */
	am_offset = siba_admatch_offset(addrspace);
	if (am_offset == 0) {
		printf("addrspace %u is unsupported", addrspace);
		return (ENODEV);
	}

	/* Read and parse the address match register */
	am = siba_erom_read_4(sc, core.core_idx, am_offset);

	if ((error = siba_parse_admatch(am, &am_addr, &am_size))) {
		printf("failed to decode address match register value 0x%x\n",
		    am);
		return (error);
	}

	if (info != NULL)
		*info = core;

	*addr = am_addr;
	*size = am_size;

	return (0);
}

/* BHND_EROM_GET_CORE_TABLE() */
static int
siba_erom_get_core_table(bhnd_erom_t *erom, struct bhnd_core_info **cores,
    u_int *num_cores)
{
	struct siba_erom	*sc;
	struct bhnd_core_info	*out;

	sc = (struct siba_erom *)erom;

	/* Allocate our core array */
	out = malloc(sizeof(*out) * sc->ncores, M_BHND, M_NOWAIT);
	if (out == NULL)
		return (ENOMEM);

	*cores = out;
	*num_cores = sc->ncores;

	/* Enumerate all cores. */
	for (u_int i = 0; i < sc->ncores; i++) {
		struct siba_core_id sid;

		/* Read the core info */
		sid = siba_erom_parse_core_id(sc, i, 0);
		out[i] = sid.core_info;

		/* Determine unit number */
		for (u_int j = 0; j < i; j++) {
			if (out[j].vendor == out[i].vendor &&
			    out[j].device == out[i].device)
				out[i].unit++;
		}
	}

	return (0);
}

/* BHND_EROM_FREE_CORE_TABLE() */
static void
siba_erom_free_core_table(bhnd_erom_t *erom, struct bhnd_core_info *cores)
{
	free(cores, M_BHND);
}

static kobj_method_t siba_erom_methods[] = {
	KOBJMETHOD(bhnd_erom_probe_static,	siba_erom_probe_static),
	KOBJMETHOD(bhnd_erom_init,		siba_erom_init),
	KOBJMETHOD(bhnd_erom_init_static,	siba_erom_init_static),
	KOBJMETHOD(bhnd_erom_fini,		siba_erom_fini),
	KOBJMETHOD(bhnd_erom_get_core_table,	siba_erom_get_core_table),
	KOBJMETHOD(bhnd_erom_free_core_table,	siba_erom_free_core_table),
	KOBJMETHOD(bhnd_erom_lookup_core,	siba_erom_lookup_core),
	KOBJMETHOD(bhnd_erom_lookup_core_addr,	siba_erom_lookup_core_addr),

	KOBJMETHOD_END
};

BHND_EROM_DEFINE_CLASS(siba_erom, siba_erom_parser, siba_erom_methods, sizeof(struct siba_erom));
