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
#include <sys/limits.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include "bcma_eromreg.h"
#include "bcma_eromvar.h"

/*
 * BCMA Enumeration ROM (EROM) Table
 * 
 * Provides auto-discovery of BCMA cores on Broadcom's HND SoC.
 * 
 * The EROM core address can be found at BCMA_CC_EROM_ADDR within the
 * ChipCommon registers. The table itself is comprised of 32-bit
 * type-tagged entries, organized into an array of variable-length
 * core descriptor records.
 * 
 * The final core descriptor is followed by a 32-bit BCMA_EROM_TABLE_EOF (0xF)
 * marker.
 */

static const char	*erom_entry_type_name (uint8_t entry);
static int		 erom_read32(struct bcma_erom *erom, uint32_t *entry);
static int		 erom_skip32(struct bcma_erom *erom);

static int		 erom_skip_core(struct bcma_erom *erom);
static int		 erom_skip_mport(struct bcma_erom *erom);
static int		 erom_skip_sport_region(struct bcma_erom *erom);

static int		 erom_seek_next(struct bcma_erom *erom, uint8_t etype);

#define	EROM_LOG(erom, fmt, ...)	\
	device_printf(erom->dev, "erom[0x%llx]: " fmt, \
	    (unsigned long long) (erom->offset), ##__VA_ARGS__);

/**
 * Open an EROM table for reading.
 * 
 * @param[out] erom On success, will be populated with a valid EROM
 * read state.
 * @param r An active resource mapping the EROM core.
 * @param offset Offset of the EROM core within @p resource.
 *
 * @retval 0 success
 * @retval non-zero if the erom table could not be opened.
 */
int
bcma_erom_open(struct bcma_erom *erom, struct resource *r, bus_size_t offset)
{
	/* Initialize the EROM reader */
	erom->dev = rman_get_device(r);
	erom->r = r;
	erom->start = offset + BCMA_EROM_TABLE_START;
	erom->offset = 0;

	return (0);
}

/** Return the type name for an EROM entry */
static const char *
erom_entry_type_name (uint8_t entry)
{
	switch (BCMA_EROM_GET_ATTR(entry, ENTRY_TYPE)) {
	case BCMA_EROM_ENTRY_TYPE_CORE:
		return "core";
	case BCMA_EROM_ENTRY_TYPE_MPORT:
		return "mport";
	case BCMA_EROM_ENTRY_TYPE_REGION:
		return "region";
	default:
		return "unknown";
	}
}

/**
 * Return the current read position.
 */
bus_size_t
bcma_erom_tell(struct bcma_erom *erom)
{
	return (erom->offset);
}

/**
 * Seek to an absolute read position.
 */
void
bcma_erom_seek(struct bcma_erom *erom, bus_size_t offset)
{
	erom->offset = offset;
}

/**
 * Read a 32-bit entry value from the EROM table without advancing the
 * read position.
 * 
 * @param erom EROM read state.
 * @param entry Will contain the read result on success.
 * @retval 0 success
 * @retval ENOENT The end of the EROM table was reached.
 * @retval non-zero The read could not be completed.
 */
int
bcma_erom_peek32(struct bcma_erom *erom, uint32_t *entry)
{
	if (erom->offset >= BCMA_EROM_TABLE_SIZE) {
		EROM_LOG(erom, "BCMA EROM table missing terminating EOF\n");
		return (EINVAL);
	}

	*entry = bus_read_4(erom->r, erom->start + erom->offset);
	return (0);
}

/**
 * Read a 32-bit entry value from the EROM table.
 * 
 * @param erom EROM read state.
 * @param entry Will contain the read result on success.
 * @retval 0 success
 * @retval ENOENT The end of the EROM table was reached.
 * @retval non-zero The read could not be completed.
 */
static int
erom_read32(struct bcma_erom *erom, uint32_t *entry)
{
	int error;

	if ((error = bcma_erom_peek32(erom, entry)) == 0)
		erom->offset += 4;

	return (error);
}

/**
 * Read and discard 32-bit entry value from the EROM table.
 * 
 * @param erom EROM read state.
 * @retval 0 success
 * @retval ENOENT The end of the EROM table was reached.
 * @retval non-zero The read could not be completed.
 */
static int
erom_skip32(struct bcma_erom *erom)
{
	uint32_t	entry;

	return erom_read32(erom, &entry);
}

/**
 * Read and discard a core descriptor from the EROM table.
 * 
 * @param erom EROM read state.
 * @retval 0 success
 * @retval ENOENT The end of the EROM table was reached.
 * @retval non-zero The read could not be completed.
 */
static int
erom_skip_core(struct bcma_erom *erom)
{
	struct bcma_erom_core core;
	return (bcma_erom_parse_core(erom, &core));
}

/**
 * Read and discard a master port descriptor from the EROM table.
 * 
 * @param erom EROM read state.
 * @retval 0 success
 * @retval ENOENT The end of the EROM table was reached.
 * @retval non-zero The read could not be completed.
 */
static int
erom_skip_mport(struct bcma_erom *erom)
{
	struct bcma_erom_mport mp;
	return (bcma_erom_parse_mport(erom, &mp));
}

/**
 * Read and discard a port region descriptor from the EROM table.
 * 
 * @param erom EROM read state.
 * @retval 0 success
 * @retval ENOENT The end of the EROM table was reached.
 * @retval non-zero The read could not be completed.
 */
static int
erom_skip_sport_region(struct bcma_erom *erom)
{
	struct bcma_erom_sport_region r;
	return (bcma_erom_parse_sport_region(erom, &r));
}

/**
 * Seek to the next entry matching the given EROM entry type.
 * 
 * @param erom EROM read state.
 * @param etype  One of BCMA_EROM_ENTRY_TYPE_CORE,
 * BCMA_EROM_ENTRY_TYPE_MPORT, or BCMA_EROM_ENTRY_TYPE_REGION.
 * @retval 0 success
 * @retval ENOENT The end of the EROM table was reached.
 * @retval non-zero Reading or parsing the descriptor failed.
 */
static int
erom_seek_next(struct bcma_erom *erom, uint8_t etype)
{
	uint32_t			entry;
	int				error;

	/* Iterate until we hit an entry matching the requested type. */
	while (!(error = bcma_erom_peek32(erom, &entry))) {
		/* Handle EOF */
		if (entry == BCMA_EROM_TABLE_EOF)
			return (ENOENT);

		/* Invalid entry */
		if (!BCMA_EROM_GET_ATTR(entry, ENTRY_ISVALID))
			return (EINVAL);

		/* Entry type matches? */
		if (BCMA_EROM_GET_ATTR(entry, ENTRY_TYPE) == etype)
			return (0);

		/* Skip non-matching entry types. */
		switch (BCMA_EROM_GET_ATTR(entry, ENTRY_TYPE)) {
		case BCMA_EROM_ENTRY_TYPE_CORE:
			if ((error = erom_skip_core(erom)))
				return (error);

			break;

		case BCMA_EROM_ENTRY_TYPE_MPORT:
			if ((error = erom_skip_mport(erom)))
				return (error);

			break;
		
		case BCMA_EROM_ENTRY_TYPE_REGION:
			if ((error = erom_skip_sport_region(erom)))
				return (error);
			break;

		default:
			/* Unknown entry type! */
			return (EINVAL);	
		}
	}

	return (error);
}

/**
 * Return the read position to the start of the EROM table.
 * 
 * @param erom EROM read state.
 */
void
bcma_erom_reset(struct bcma_erom *erom)
{
	erom->offset = 0;
}

/**
 * Seek to the requested core entry.
 * 
 * @param erom EROM read state.
 * @param core_index Index of the core to seek to.
 * @retval 0 success
 * @retval ENOENT The end of the EROM table was reached before @p index was
 * found.
 * @retval non-zero Reading or parsing failed.
 */
int
bcma_erom_seek_core_index(struct bcma_erom *erom, u_int core_index)
{
	int error;

	/* Start search at top of EROM */
	bcma_erom_reset(erom);

	/* Skip core descriptors till we hit the requested entry */
	for (u_int i = 0; i < core_index; i++) {
		struct bcma_erom_core core;

		/* Read past the core descriptor */
		if ((error = bcma_erom_parse_core(erom, &core)))
			return (error);

		/* Seek to the next readable core entry */
		error = erom_seek_next(erom, BCMA_EROM_ENTRY_TYPE_CORE);
		if (error)
			return (error);
	}

	return (0);
}


/**
 * Read the next core descriptor from the EROM table.
 * 
 * @param erom EROM read state.
 * @param[out] core On success, will be populated with the parsed core
 * descriptor data.
 * @retval 0 success
 * @retval ENOENT The end of the EROM table was reached.
 * @retval non-zero Reading or parsing the core descriptor failed.
 */
int
bcma_erom_parse_core(struct bcma_erom *erom, struct bcma_erom_core *core)
{
	uint32_t	entry;
	int		error;

	/* Parse CoreDescA */
	if ((error = erom_read32(erom, &entry)))
		return (error);
	
	/* Handle EOF */
	if (entry == BCMA_EROM_TABLE_EOF)
		return (ENOENT);
	
	if (!BCMA_EROM_ENTRY_IS(entry, CORE)) {
		EROM_LOG(erom, "Unexpected EROM entry 0x%x (type=%s)\n",
                   entry, erom_entry_type_name(entry));
		
		return (EINVAL);
	}

	core->vendor = BCMA_EROM_GET_ATTR(entry, COREA_DESIGNER);
	core->device = BCMA_EROM_GET_ATTR(entry, COREA_ID);
	
	/* Parse CoreDescB */
	if ((error = erom_read32(erom, &entry)))
		return (error);

	if (!BCMA_EROM_ENTRY_IS(entry, CORE)) {
		return (EINVAL);
	}

	core->rev = BCMA_EROM_GET_ATTR(entry, COREB_REV);
	core->num_mport = BCMA_EROM_GET_ATTR(entry, COREB_NUM_MP);
	core->num_dport = BCMA_EROM_GET_ATTR(entry, COREB_NUM_DP);
	core->num_mwrap = BCMA_EROM_GET_ATTR(entry, COREB_NUM_WMP);
	core->num_swrap = BCMA_EROM_GET_ATTR(entry, COREB_NUM_WSP);

	return (0);
}

/**
 * Read the next master port descriptor from the EROM table.
 * 
 * @param erom EROM read state.
 * @param[out] mport On success, will be populated with the parsed
 * descriptor data.
 * @retval 0 success
 * @retval non-zero Reading or parsing the descriptor failed.
 */
int
bcma_erom_parse_mport(struct bcma_erom *erom,
    struct bcma_erom_mport *mport)
{
	uint32_t	entry;
	int		error;

	/* Parse the master port descriptor */
	if ((error = erom_read32(erom, &entry)))
		return (error);
	
	if (!BCMA_EROM_ENTRY_IS(entry, MPORT))
		return (EINVAL);

	mport->port_vid = BCMA_EROM_GET_ATTR(entry, MPORT_ID);
	mport->port_num = BCMA_EROM_GET_ATTR(entry, MPORT_NUM);

	return (0);
}

/**
 * Read the next slave port region descriptor from the EROM table.
 * 
 * @param erom EROM read state.
 * @param[out] mport On success, will be populated with the parsed
 * descriptor data.
 * @retval 0 success
 * @retval ENOENT The end of the region descriptor table was reached.
 * @retval non-zero Reading or parsing the descriptor failed.
 */
int
bcma_erom_parse_sport_region(struct bcma_erom *erom,
    struct bcma_erom_sport_region *region)
{
	uint32_t	entry;
	uint8_t		size_type;
	int		error;

	/* Peek at the region descriptor */
	if (bcma_erom_peek32(erom, &entry))
		return (EINVAL);

	/* A non-region entry signals the end of the region table */
	if (!BCMA_EROM_ENTRY_IS(entry, REGION)) {
		return (ENOENT);
	} else {
		erom_skip32(erom);
	}

	region->base_addr = BCMA_EROM_GET_ATTR(entry, REGION_BASE);
	region->region_type = BCMA_EROM_GET_ATTR(entry, REGION_TYPE);
	region->region_port = BCMA_EROM_GET_ATTR(entry, REGION_PORT);
	size_type = BCMA_EROM_GET_ATTR(entry, REGION_SIZE);

	/* If region address is 64-bit, fetch the high bits. */
	if (BCMA_EROM_GET_ATTR(entry, REGION_64BIT)) {
		if ((error = erom_read32(erom, &entry)))
			return (error);
		
		region->base_addr |= ((bhnd_addr_t) entry << 32);
	}

	/* Parse the region size; it's either encoded as the binary logarithm
	 * of the number of 4K pages (i.e. log2 n), or its encoded as a
	 * 32-bit/64-bit literal value directly following the current entry. */
	if (size_type == BCMA_EROM_REGION_SIZE_OTHER) {
		if ((error = erom_read32(erom, &entry)))
			return (error);

		region->size = BCMA_EROM_GET_ATTR(entry, RSIZE_VAL);

		if (BCMA_EROM_GET_ATTR(entry, RSIZE_64BIT)) {
			if ((error = erom_read32(erom, &entry)))
				return (error);
			region->size |= ((bhnd_size_t) entry << 32);
		}
	} else {
		region->size = BCMA_EROM_REGION_SIZE_BASE << size_type;
	}

	/* Verify that addr+size does not overflow. */
	if (region->size != 0 &&
	    BHND_ADDR_MAX - (region->size - 1) < region->base_addr)
	{
		EROM_LOG(erom, "%s%u: invalid address map %llx:%llx\n",
		    erom_entry_type_name(region->region_type),
		    region->region_port,
		    (unsigned long long) region->base_addr,
		    (unsigned long long) region->size);

		return (EINVAL);
	}

	return (0);
}

/**
 * Parse all cores descriptors from @p erom and return the array
 * in @p cores and the count in @p num_cores. The current EROM read position
 * is left unmodified.
 * 
 * The memory allocated for the table should be freed using
 * `free(*cores, M_BHND)`. @p cores and @p num_cores are not changed
 * when an error is returned.
 * 
 * @param erom EROM read state.
 * @param[out] cores the table of parsed core descriptors.
 * @param[out] num_cores the number of core records in @p cores.
 */
int
bcma_erom_get_core_info(struct bcma_erom *erom,
    struct bhnd_core_info **cores,
    u_int *num_cores)
{
	struct bhnd_core_info	*buffer;
	bus_size_t		 initial_offset;
	u_int			 count;
	int			 error;

	buffer = NULL;
	initial_offset = bcma_erom_tell(erom);

	/* Determine the core count */
	bcma_erom_reset(erom);
	for (count = 0, error = 0; !error; count++) {
		struct bcma_erom_core core;

		/* Seek to the first readable core entry */
		error = erom_seek_next(erom, BCMA_EROM_ENTRY_TYPE_CORE);
		if (error == ENOENT)
			break;
		else if (error)
			goto cleanup;
		
		/* Read past the core descriptor */
		if ((error = bcma_erom_parse_core(erom, &core)))
			goto cleanup;
	}

	/* Allocate our output buffer */
	buffer = malloc(sizeof(struct bhnd_core_info) * count, M_BHND,
	    M_NOWAIT);
	if (buffer == NULL) {
		error = ENOMEM;
		goto cleanup;
	}

	/* Parse all core descriptors */
	bcma_erom_reset(erom);
	for (u_int i = 0; i < count; i++) {
		struct bcma_erom_core core;

		/* Parse the core */
		error = erom_seek_next(erom, BCMA_EROM_ENTRY_TYPE_CORE);
		if (error)
			goto cleanup;

		error = bcma_erom_parse_core(erom, &core);
		if (error)
			goto cleanup;
		
		/* Convert to a bhnd info record */
		buffer[i].vendor = core.vendor;
		buffer[i].device = core.device;
		buffer[i].hwrev = core.rev;
		buffer[i].core_idx = i;
		buffer[i].unit = 0;

		/* Determine the unit number */
		for (u_int j = 0; j < i; j++) {
			if (buffer[i].vendor == buffer[j].vendor &&
			    buffer[i].device == buffer[j].device)
				buffer[i].unit++;
		}
	}

cleanup:
	if (!error) {
		*cores = buffer;
		*num_cores = count;
	} else {
		if (buffer != NULL)
			free(buffer, M_BHND);
	}

	/* Restore the initial position */
	bcma_erom_seek(erom, initial_offset);
	return (error);
}


/**
 * Register all MMIO region descriptors for the given slave port.
 * 
 * @param erom EROM read state.
 * @param corecfg Core info to be populated with the scanned port regions.
 * @param port_num Port index for which regions will be parsed.
 * @param region_type The region type to be parsed.
 * @param[out] offset The offset at which to perform parsing. On success, this
 * will be updated to point to the next EROM table entry.
 */
static int 
erom_corecfg_fill_port_regions(struct bcma_erom *erom,
    struct bcma_corecfg *corecfg, bcma_pid_t port_num,
    uint8_t region_type)
{
	struct bcma_sport	*sport;
	struct bcma_sport_list	*sports;
	bus_size_t		 entry_offset;
	int			 error;
	bhnd_port_type		 port_type;

	error = 0;
	
	/* Determine the port type for this region type. */
	switch (region_type) {
		case BCMA_EROM_REGION_TYPE_DEVICE:
			port_type = BHND_PORT_DEVICE;
			break;
		case BCMA_EROM_REGION_TYPE_BRIDGE:
			port_type = BHND_PORT_BRIDGE;
			break;
		case BCMA_EROM_REGION_TYPE_MWRAP:
		case BCMA_EROM_REGION_TYPE_SWRAP:
			port_type = BHND_PORT_AGENT;
			break;
		default:
			EROM_LOG(erom, "unsupported region type %hhx\n",
			    region_type);
			return (EINVAL);
	}

	/* Fetch the list to be populated */
	sports = bcma_corecfg_get_port_list(corecfg, port_type);
	
	/* Allocate a new port descriptor */
	sport = bcma_alloc_sport(port_num, port_type);
	if (sport == NULL)
		return (ENOMEM);

	/* Read all address regions defined for this port */
	for (bcma_rmid_t region_num = 0;; region_num++) {
		struct bcma_map			*map;
		struct bcma_erom_sport_region	 spr;

		/* No valid port definition should come anywhere near
		 * BCMA_RMID_MAX. */
		if (region_num == BCMA_RMID_MAX) {
			EROM_LOG(erom, "core%u %s%u: region count reached "
			    "upper limit of %u\n",
			    corecfg->core_info.core_idx,
			    bhnd_port_type_name(port_type),
			    port_num, BCMA_RMID_MAX);

			error = EINVAL;
			goto cleanup;
		}

		/* Parse the next region entry. */
		entry_offset = bcma_erom_tell(erom);
		error = bcma_erom_parse_sport_region(erom, &spr);
		if (error && error != ENOENT) {
			EROM_LOG(erom, "core%u %s%u.%u: invalid slave port "
			    "address region\n",
			    corecfg->core_info.core_idx,
			    bhnd_port_type_name(port_type),
			    port_num, region_num);
			goto cleanup;
		}

		/* ENOENT signals no further region entries */
		if (error == ENOENT) {
			/* No further entries */
			error = 0;
			break;
		} 
		
		/* A region or type mismatch also signals no further region
		 * entries */
		if (spr.region_port != port_num ||
		    spr.region_type != region_type)
		{
			/* We don't want to consume this entry */
			bcma_erom_seek(erom, entry_offset);

			error = 0;
			goto cleanup;
		}

		/*
		 * Create the map entry. 
		 */
		map = malloc(sizeof(struct bcma_map), M_BHND, M_NOWAIT);
		if (map == NULL) {
			error = ENOMEM;
			goto cleanup;
		}

		map->m_region_num = region_num;
		map->m_base = spr.base_addr;
		map->m_size = spr.size;
		map->m_rid = -1;

		/* Add the region map to the port */
		STAILQ_INSERT_TAIL(&sport->sp_maps, map, m_link);
		sport->sp_num_maps++;
	}

cleanup:
	/* Append the new port descriptor on success, or deallocate the
	 * partially parsed descriptor on failure. */
	if (error == 0) {
		STAILQ_INSERT_TAIL(sports, sport, sp_link);
	} else if (sport != NULL) {
		bcma_free_sport(sport);
	}

	return error;
}

/**
 * Parse the next core entry from the EROM table and produce a bcma_corecfg
 * to be owned by the caller.
 * 
 * @param erom EROM read state.
 * @param[out] result On success, the core's device info. The caller inherits
 * ownership of this allocation.
 * 
 * @return If successful, returns 0. If the end of the EROM table is hit,
 * ENOENT will be returned. On error, returns a non-zero error value.
 */
int
bcma_erom_parse_corecfg(struct bcma_erom *erom, struct bcma_corecfg **result)
{
	struct bcma_corecfg	*cfg;
	struct bcma_erom_core	 core;
	uint8_t			 first_region_type;
	bus_size_t		 initial_offset;
	u_int			 core_index;
	int			 core_unit;
	int			 error;

	cfg = NULL;
	initial_offset = bcma_erom_tell(erom);

	/* Parse the next core entry */
	if ((error = bcma_erom_parse_core(erom, &core)))
		return (error);

	/* Determine the core's index and unit numbers */
	bcma_erom_reset(erom);
	core_unit = 0;
	core_index = 0;
	for (; bcma_erom_tell(erom) != initial_offset; core_index++) {
		struct bcma_erom_core prev_core;

		/* Parse next core */
		if ((error = erom_seek_next(erom, BCMA_EROM_ENTRY_TYPE_CORE)))
			return (error);

		if ((error = bcma_erom_parse_core(erom, &prev_core)))
			return (error);

		/* Is earlier unit? */
		if (core.vendor == prev_core.vendor &&
		    core.device == prev_core.device)
		{
			core_unit++;
		}

		/* Seek to next core */
		if ((error = erom_seek_next(erom, BCMA_EROM_ENTRY_TYPE_CORE)))
			return (error);
	}

	/* We already parsed the core descriptor */
	if ((error = erom_skip_core(erom)))
		return (error);

	/* Allocate our corecfg */
	cfg = bcma_alloc_corecfg(core_index, core_unit, core.vendor,
	    core.device, core.rev);
	if (cfg == NULL)
		return (ENOMEM);
	
	/* These are 5-bit values in the EROM table, and should never be able
	 * to overflow BCMA_PID_MAX. */
	KASSERT(core.num_mport <= BCMA_PID_MAX, ("unsupported mport count"));
	KASSERT(core.num_dport <= BCMA_PID_MAX, ("unsupported dport count"));
	KASSERT(core.num_mwrap + core.num_swrap <= BCMA_PID_MAX,
	    ("unsupported wport count"));

	if (bootverbose) {
		EROM_LOG(erom, 
		    "core%u: %s %s (cid=%hx, rev=%hu, unit=%d)\n",
		    core_index,
		    bhnd_vendor_name(core.vendor),
		    bhnd_find_core_name(core.vendor, core.device), 
		    core.device, core.rev, core_unit);
	}

	cfg->num_master_ports = core.num_mport;
	cfg->num_dev_ports = 0;		/* determined below */
	cfg->num_bridge_ports = 0;	/* determined blow */
	cfg->num_wrapper_ports = core.num_mwrap + core.num_swrap;

	/* Parse Master Port Descriptors */
	for (uint8_t i = 0; i < core.num_mport; i++) {
		struct bcma_mport	*mport;
		struct bcma_erom_mport	 mpd;
	
		/* Parse the master port descriptor */
		error = bcma_erom_parse_mport(erom, &mpd);
		if (error)
			goto failed;

		/* Initialize a new bus mport structure */
		mport = malloc(sizeof(struct bcma_mport), M_BHND, M_NOWAIT);
		if (mport == NULL) {
			error = ENOMEM;
			goto failed;
		}
		
		mport->mp_vid = mpd.port_vid;
		mport->mp_num = mpd.port_num;

		/* Update dinfo */
		STAILQ_INSERT_TAIL(&cfg->master_ports, mport, mp_link);
	}
	

	/*
	 * Determine whether this is a bridge device; if so, we can
	 * expect the first sequence of address region descriptors to
	 * be of EROM_REGION_TYPE_BRIDGE instead of
	 * BCMA_EROM_REGION_TYPE_DEVICE.
	 * 
	 * It's unclear whether this is the correct mechanism by which we
	 * should detect/handle bridge devices, but this approach matches
	 * that of (some of) Broadcom's published drivers.
	 */
	if (core.num_dport > 0) {
		uint32_t entry;

		if ((error = bcma_erom_peek32(erom, &entry)))
			goto failed;

		if (BCMA_EROM_ENTRY_IS(entry, REGION) && 
		    BCMA_EROM_GET_ATTR(entry, REGION_TYPE) == BCMA_EROM_REGION_TYPE_BRIDGE)
		{
			first_region_type = BCMA_EROM_REGION_TYPE_BRIDGE;
			cfg->num_dev_ports = 0;
			cfg->num_bridge_ports = core.num_dport;
		} else {
			first_region_type = BCMA_EROM_REGION_TYPE_DEVICE;
			cfg->num_dev_ports = core.num_dport;
			cfg->num_bridge_ports = 0;
		}
	}
	
	/* Device/bridge port descriptors */
	for (uint8_t sp_num = 0; sp_num < core.num_dport; sp_num++) {
		error = erom_corecfg_fill_port_regions(erom, cfg, sp_num,
		    first_region_type);

		if (error)
			goto failed;
	}

	/* Wrapper (aka device management) descriptors (for master ports). */
	for (uint8_t sp_num = 0; sp_num < core.num_mwrap; sp_num++) {
		error = erom_corecfg_fill_port_regions(erom, cfg, sp_num,
		    BCMA_EROM_REGION_TYPE_MWRAP);

		if (error)
			goto failed;
	}

	
	/* Wrapper (aka device management) descriptors (for slave ports). */	
	for (uint8_t i = 0; i < core.num_swrap; i++) {
		/* Slave wrapper ports are not numbered distinctly from master
		 * wrapper ports. */

		/* 
		 * Broadcom DDR1/DDR2 Memory Controller
		 * (cid=82e, rev=1, unit=0, d/mw/sw = 2/0/1 ) ->
		 * bhnd0: erom[0xdc]: core6 agent0.0: mismatch got: 0x1 (0x2)
		 *
		 * ARM BP135 AMBA3 AXI to APB Bridge
		 * (cid=135, rev=0, unit=0, d/mw/sw = 1/0/1 ) ->
		 * bhnd0: erom[0x124]: core9 agent1.0: mismatch got: 0x0 (0x2)
		 *
		 * core.num_mwrap
		 * ===>
		 * (core.num_mwrap > 0) ?
		 *           core.num_mwrap :
		 *           ((core.vendor == BHND_MFGID_BCM) ? 1 : 0)
		 */
		uint8_t sp_num;
		sp_num = (core.num_mwrap > 0) ?
				core.num_mwrap :
				((core.vendor == BHND_MFGID_BCM) ? 1 : 0) + i;
		error = erom_corecfg_fill_port_regions(erom, cfg, sp_num,
		    BCMA_EROM_REGION_TYPE_SWRAP);

		if (error)
			goto failed;
	}

	*result = cfg;
	return (0);
	
failed:
	if (cfg != NULL)
		bcma_free_corecfg(cfg);

	return error;
}
