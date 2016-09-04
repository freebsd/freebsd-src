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
struct siba_erom_io;


static int			siba_eio_init(struct siba_erom_io *io,
				    device_t parent, struct bhnd_resource *res,
				    int rid, bus_size_t offset, u_int ncores);

static int			siba_eio_init_static(struct siba_erom_io *io,
				    bus_space_tag_t bst, bus_space_handle_t bsh,
				    bus_size_t offset, u_int ncores);

static uint32_t			siba_eio_read_4(struct siba_erom_io *io,
				    u_int core_idx, bus_size_t offset);

static struct siba_core_id	siba_eio_read_core_id(struct siba_erom_io *io,
				    u_int core_idx, int unit);

static int			siba_eio_read_chipid(struct siba_erom_io *io,
				    bus_addr_t enum_addr,
				    struct bhnd_chipid *cid);

/**
 * SIBA EROM generic I/O context
 */
struct siba_erom_io {
	u_int			 ncores;	/**< core count */
	bus_size_t	 	 offset;	/**< base read offset */

	/* resource state */
	device_t	 	 dev;		/**< parent dev to use for resource allocations,
						     or NULL if unavailable. */
	struct bhnd_resource	*res;		/**< memory resource, or NULL */
	int			 rid;		/**< memory resource ID */

	/* bus tag state */
	bus_space_tag_t		 bst;		/**< bus space tag */
	bus_space_handle_t	 bsh;		/**< bus space handle */
};

/**
 * SIBA EROM per-instance state.
 */
struct siba_erom {
	struct bhnd_erom	obj;
	struct siba_erom_io	io;	/**< i/o context */
};

#define	EROM_LOG(io, fmt, ...)	do {					\
	if (io->dev != NULL) {						\
		device_printf(io->dev, "%s: " fmt, __FUNCTION__,	\
		    ##__VA_ARGS__);					\
	} else {							\
		printf("%s: " fmt, __FUNCTION__, ##__VA_ARGS__);	\
	}								\
} while(0)

static int
siba_erom_probe_common(struct siba_erom_io *io, const struct bhnd_chipid *hint,
    struct bhnd_chipid *cid)
{
	uint32_t		idreg;
	int			error;

	/* Try using the provided hint. */
	if (hint != NULL) {
		struct siba_core_id sid;

		/* Validate bus type */
		if (hint->chip_type != BHND_CHIPTYPE_SIBA)
			return (ENXIO);

		/*
		 * Verify the first core's IDHIGH/IDLOW identification.
		 * 
		 * The core must be a Broadcom core, but must *not* be
		 * a chipcommon core; those shouldn't be hinted.
		 *
		 * The first core on EXTIF-equipped devices varies, but on the
		 * BCM4710, it's a SDRAM core (0x803).
		 */

		sid = siba_eio_read_core_id(io, 0, 0);

		if (sid.core_info.vendor != BHND_MFGID_BCM)
			return (ENXIO);

		if (sid.core_info.device == BHND_COREID_CC)
			return (EINVAL);

		*cid = *hint;
	} else {
		/* Validate bus type */
		idreg = siba_eio_read_4(io, 0, CHIPC_ID);
		if (CHIPC_GET_BITS(idreg, CHIPC_ID_BUS) != BHND_CHIPTYPE_SIBA)
			return (ENXIO);	

		/* Identify the chipset */
		if ((error = siba_eio_read_chipid(io, SIBA_ENUM_ADDR, cid)))
			return (error);

		/* Verify the chip type */
		if (cid->chip_type != BHND_CHIPTYPE_SIBA)
			return (ENXIO);
	}

	/*
	 * gcc hack: ensure bhnd_chipid.ncores cannot exceed SIBA_MAX_CORES
	 * without triggering build failure due to -Wtype-limits
	 *
	 * if (cid.ncores > SIBA_MAX_CORES)
	 *      return (EINVAL)
	 */
	_Static_assert((2^sizeof(cid->ncores)) <= SIBA_MAX_CORES,
	    "ncores could result in over-read of backing resource");

	return (0);
}

/* SIBA implementation of BHND_EROM_PROBE() */
static int
siba_erom_probe(bhnd_erom_class_t *cls, struct bhnd_resource *res,
    bus_size_t offset, const struct bhnd_chipid *hint,
    struct bhnd_chipid *cid)
{
	struct siba_erom_io	io;
	int			error, rid;

	rid = rman_get_rid(res->res);

	/* Initialize I/O context, assuming at least 1 core exists.  */
	if ((error = siba_eio_init(&io, NULL, res, rid, offset, 1)))
		return (error);

	return (siba_erom_probe_common(&io, hint, cid));
}

/* SIBA implementation of BHND_EROM_PROBE_STATIC() */
static int
siba_erom_probe_static(bhnd_erom_class_t *cls, bus_space_tag_t bst,
     bus_space_handle_t bsh, bus_addr_t paddr, const struct bhnd_chipid *hint,
     struct bhnd_chipid *cid)
{
	struct siba_erom_io	io;
	int			error;

	/* Initialize I/O context, assuming at least 1 core exists.  */
	if ((error = siba_eio_init_static(&io, bst, bsh, 0, 1)))
		return (error);

	return (siba_erom_probe_common(&io, hint, cid));
}

/* SIBA implementation of BHND_EROM_INIT() */
static int
siba_erom_init(bhnd_erom_t *erom, const struct bhnd_chipid *cid,
    device_t parent, int rid)
{
	struct siba_erom	*sc;
	struct bhnd_resource	*res;
	int			 error;
	
	sc = (struct siba_erom *)erom;

	/* Allocate backing resource */
	res = bhnd_alloc_resource(parent, SYS_RES_MEMORY, &rid,
	    cid->enum_addr, cid->enum_addr + SIBA_ENUM_SIZE -1, SIBA_ENUM_SIZE,
	    RF_ACTIVE|RF_SHAREABLE);
	if (res == NULL)
		return (ENOMEM);

	/* Initialize I/O context */
	error = siba_eio_init(&sc->io, parent, res, rid, 0x0, cid->ncores);
	if (error)
		bhnd_release_resource(parent, SYS_RES_MEMORY, rid, res);

	return (error);
}

/* SIBA implementation of BHND_EROM_INIT_STATIC() */
static int
siba_erom_init_static(bhnd_erom_t *erom, const struct bhnd_chipid *cid,
    bus_space_tag_t bst, bus_space_handle_t bsh)
{
	struct siba_erom	*sc;
	
	sc = (struct siba_erom *)erom;

	/* Initialize I/O context */
	return (siba_eio_init_static(&sc->io, bst, bsh, 0x0, cid->ncores));
}

/* SIBA implementation of BHND_EROM_FINI() */
static void
siba_erom_fini(bhnd_erom_t *erom)
{
	struct siba_erom *sc = (struct siba_erom *)erom;

	if (sc->io.res != NULL) {
		bhnd_release_resource(sc->io.dev, SYS_RES_MEMORY, sc->io.rid,
		    sc->io.res);

		sc->io.res = NULL;
		sc->io.rid = -1;
	}
}

/* Initialize siba_erom resource I/O context */
static int
siba_eio_init(struct siba_erom_io *io, device_t parent,
    struct bhnd_resource *res, int rid, bus_size_t offset, u_int ncores)
{
	io->dev = parent;
	io->res = res;
	io->rid = rid;
	io->offset = offset;
	io->ncores = ncores;

	return (0);
}

/* Initialize siba_erom bus space I/O context */
static int
siba_eio_init_static(struct siba_erom_io *io, bus_space_tag_t bst,
    bus_space_handle_t bsh, bus_size_t offset, u_int ncores)
{
	io->res = NULL;
	io->rid = -1;
	io->bst = bst;
	io->bsh = bsh;
	io->offset = offset;
	io->ncores = ncores;

	return (0);
}

/**
 * Read a 32-bit value from @p offset relative to the base address of
 * the given @p core_idx.
 * 
 * @param io EROM I/O context.
 * @param core_idx Core index.
 * @param offset Core register offset.
 */
static uint32_t
siba_eio_read_4(struct siba_erom_io *io, u_int core_idx, bus_size_t offset)
{
	bus_size_t core_offset;

	/* Sanity check core index and offset */
	if (core_idx >= io->ncores)
		panic("core index %u out of range (ncores=%u)", core_idx,
		    io->ncores);

	if (offset > SIBA_CORE_SIZE - sizeof(uint32_t))
		panic("invalid core offset %#jx", (uintmax_t)offset);

	/* Perform read */
	core_offset = io->offset + SIBA_CORE_OFFSET(core_idx) + offset;
	if (io->res != NULL)
		return (bhnd_bus_read_4(io->res, core_offset));
	else
		return (bus_space_read_4(io->bst, io->bsh, core_offset));
}

/**
 * Read and parse identification registers for the given @p core_index.
 * 
 * @param io EROM I/O context.
 * @param core_idx The core index.
 * @param unit The caller-specified unit number to be included in the return
 * value.
 */
static struct siba_core_id
siba_eio_read_core_id(struct siba_erom_io *io, u_int core_idx, int unit)
{
	uint32_t idhigh, idlow;

	idhigh = siba_eio_read_4(io, core_idx, SB0_REG_ABS(SIBA_CFG0_IDHIGH));
	idlow = siba_eio_read_4(io, core_idx, SB0_REG_ABS(SIBA_CFG0_IDLOW));

	return (siba_parse_core_id(idhigh, idlow, core_idx, unit));
}

/**
 * Read and parse the chip identification register from the ChipCommon core.
 * 
 * @param io EROM I/O context.
 * @param enum_addr The physical address mapped by @p io.
 * @param cid On success, the parsed chip identifier.
 */
static int
siba_eio_read_chipid(struct siba_erom_io *io, bus_addr_t enum_addr,
    struct bhnd_chipid *cid)
{
	struct siba_core_id	ccid;
	uint32_t		idreg;

	/* Identify the chipcommon core */
	ccid = siba_eio_read_core_id(io, 0, 0);
	if (ccid.core_info.vendor != BHND_MFGID_BCM ||
	    ccid.core_info.device != BHND_COREID_CC)
	{
		if (bootverbose) {
			EROM_LOG(io, "first core not chipcommon "
			    "(vendor=%#hx, core=%#hx)\n", ccid.core_info.vendor,
			    ccid.core_info.device);
		}
		return (ENXIO);
	}

	/* Identify the chipset */
	idreg = siba_eio_read_4(io, 0, CHIPC_ID);
	*cid = bhnd_parse_chipid(idreg, enum_addr);

	/* Fix up the core count in-place */
	return (bhnd_chipid_fixed_ncores(cid, ccid.core_info.hwrev,
	    &cid->ncores));
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
	for (u_int i = 0; i < sc->io.ncores; i++) {
		struct siba_core_id	sid;
		struct bhnd_core_info	ci;

		/* Read the core info */
		sid = siba_eio_read_core_id(&sc->io, i, 0);
		ci = sid.core_info;

		/* Check for initial match */
		if (!bhnd_core_matches(&ci, &imatch))
			continue;

		/* Re-scan preceding cores to determine the unit number. */
		for (u_int j = 0; j < i; j++) {
			sid = siba_eio_read_core_id(&sc->io, i, 0);

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
	sid = siba_eio_read_core_id(&sc->io, core.core_idx, core.unit);

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
	am = siba_eio_read_4(&sc->io, core.core_idx, am_offset);

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
	out = malloc(sizeof(*out) * sc->io.ncores, M_BHND, M_NOWAIT);
	if (out == NULL)
		return (ENOMEM);

	*cores = out;
	*num_cores = sc->io.ncores;

	/* Enumerate all cores. */
	for (u_int i = 0; i < sc->io.ncores; i++) {
		struct siba_core_id sid;

		/* Read the core info */
		sid = siba_eio_read_core_id(&sc->io, i, 0);
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
	KOBJMETHOD(bhnd_erom_probe,		siba_erom_probe),
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
