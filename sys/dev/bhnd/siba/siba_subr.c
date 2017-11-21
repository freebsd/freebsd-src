/*-
 * Copyright (c) 2015-2016 Landon Fuller <landon@landonf.org>
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

#include <dev/bhnd/bhndvar.h>

#include "sibareg.h"
#include "sibavar.h"

/**
 * Map a siba(4) OCP vendor code to its corresponding JEDEC JEP-106 vendor
 * code.
 * 
 * @param ocp_vendor An OCP vendor code.
 * @return The BHND_MFGID constant corresponding to @p ocp_vendor, or
 * BHND_MFGID_INVALID if the OCP vendor is unknown.
 */
uint16_t
siba_get_bhnd_mfgid(uint16_t ocp_vendor)
{
	switch (ocp_vendor) {
	case OCP_VENDOR_BCM:
		return (BHND_MFGID_BCM);
	default:
		return (BHND_MFGID_INVALID);
	}
}

/**
 * Parse the SIBA_IDH_* fields from the per-core identification
 * registers, returning a siba_core_id representation.
 * 
 * @param idhigh The SIBA_R0_IDHIGH register.
 * @param idlow The SIBA_R0_IDLOW register. 
 * @param core_id The core id (index) to include in the result.
 * @param unit The unit number to include in the result.
 */
struct siba_core_id	 
siba_parse_core_id(uint32_t idhigh, uint32_t idlow, u_int core_idx, int unit)
{

	uint16_t	ocp_vendor;
	uint8_t		sonics_rev;
	uint8_t		num_addrspace;
	uint8_t		num_cfg;

	ocp_vendor = SIBA_REG_GET(idhigh, IDH_VENDOR);
	sonics_rev = SIBA_REG_GET(idlow, IDL_SBREV);
	num_addrspace = SIBA_REG_GET(idlow, IDL_NRADDR) + 1 /* + enum block */;

	/* Determine the number of sonics config register blocks */
	num_cfg = SIBA_CFG_NUM_2_2;
	if (sonics_rev >= SIBA_IDL_SBREV_2_3)
		num_cfg = SIBA_CFG_NUM_2_3;

	return (struct siba_core_id) {
		.core_info	= {
			.vendor	= siba_get_bhnd_mfgid(ocp_vendor),
			.device	= SIBA_REG_GET(idhigh, IDH_DEVICE),
			.hwrev	= SIBA_IDH_CORE_REV(idhigh),
			.core_idx = core_idx,
			.unit	= unit
		},
		.sonics_vendor	= ocp_vendor,
		.sonics_rev	= sonics_rev,
		.num_addrspace	= num_addrspace,
		.num_cfg_blocks	= num_cfg
	};	
}

/**
 * Allocate and return a new empty device info structure.
 * 
 * @param bus The requesting bus device.
 * 
 * @retval NULL if allocation failed.
 */
struct siba_devinfo *
siba_alloc_dinfo(device_t bus)
{
	struct siba_devinfo *dinfo;
	
	dinfo = malloc(sizeof(struct siba_devinfo), M_BHND, M_NOWAIT|M_ZERO);
	if (dinfo == NULL)
		return NULL;

	for (u_int i = 0; i < nitems(dinfo->cfg); i++) {
		dinfo->cfg[i] = ((struct siba_cfg_block){
			.cb_base = 0,
			.cb_size = 0,
			.cb_rid = -1,
		});
		dinfo->cfg_res[i] = NULL;
		dinfo->cfg_rid[i] = -1;
	}

	resource_list_init(&dinfo->resources);

	dinfo->intr_en = false;

	return dinfo;
}

/**
 * Initialize a device info structure previously allocated via
 * siba_alloc_dinfo, copying the provided core id.
 * 
 * @param dev The requesting bus device.
 * @param dinfo The device info instance.
 * @param core Device core info.
 * 
 * @retval 0 success
 * @retval non-zero initialization failed.
 */
int
siba_init_dinfo(device_t dev, struct siba_devinfo *dinfo,
    const struct siba_core_id *core_id)
{
	dinfo->core_id = *core_id;
	return (0);
}

/**
 * Map an addrspace index to its corresponding bhnd(4) BHND_PORT_DEVICE port
 * number.
 * 
 * @param addrspace Address space index.
 */
u_int
siba_addrspace_device_port(u_int addrspace)
{
	/* The first addrspace is always mapped to device0; the remainder
	 * are mapped to device1 */
	if (addrspace == 0)
		return (0);
	else
		return (1);
}

/**
 * Map an addrspace index to its corresponding bhnd(4) BHND_PORT_DEVICE port
 * region number.
 * 
 * @param addrspace Address space index.
 */
u_int
siba_addrspace_device_region(u_int addrspace)
{
	/* The first addrspace is always mapped to device0.0; the remainder
	 * are mapped to device1.0 + (n - 1) */
	if (addrspace == 0)
		return (0);
	else
		return (addrspace - 1);
}

/**
 * Map an config block index to its corresponding bhnd(4) BHND_PORT_AGENT port
 * number.
 * 
 * @param cfg Config block index.
 */
u_int
siba_cfg_agent_port(u_int cfg)
{
	/* Always agent0 */
	return (0);
}

/**
 * Map an config block index to its corresponding bhnd(4) BHND_PORT_AGENT port
 * region number.
 * 
 * @param cfg Config block index.
 */
u_int
siba_cfg_agent_region(u_int cfg)
{
	/* Always agent0.<idx> */
	return (cfg);
}

/**
 * Return the number of bhnd(4) ports to advertise for the given
 * @p core_id and @p port_type.
 * 
 * Refer to the siba_addrspace_index() and siba_cfg_index() functions for
 * information on siba's mapping of bhnd(4) port and region identifiers.
 * 
 * @param core_id The siba core info.
 * @param port_type The bhnd(4) port type.
 */
u_int
siba_port_count(struct siba_core_id *core_id, bhnd_port_type port_type)
{
	switch (port_type) {
	case BHND_PORT_DEVICE:
		/* 0, 1, or 2 ports */
		return (min(core_id->num_addrspace, 2));

	case BHND_PORT_AGENT:
		/* One agent port maps all configuration blocks */
		if (core_id->num_cfg_blocks > 0)
			return (1);

		/* Do not advertise an agent port if there are no configuration
		 * register blocks */
		return (0);

	default:
		return (0);
	}
}

/**
 * Return true if @p port of @p port_type is defined by @p core_id, false
 * otherwise.
 * 
 * @param core_id The siba core info.
 * @param port_type The bhnd(4) port type.
 * @param port The bhnd(4) port number.
 */
bool
siba_is_port_valid(struct siba_core_id *core_id, bhnd_port_type port_type,
    u_int port)
{
	/* Verify the index against the port count */
	if (siba_port_count(core_id, port_type) <= port)
		return (false);

	return (true);
}

/**
 * Return the number of bhnd(4) regions to advertise for @p core_id on the
 * @p port of @p port_type.
 * 
 * @param core_id The siba core info.
 * @param port_type The bhnd(4) port type.
 */
u_int
siba_port_region_count(struct siba_core_id *core_id, bhnd_port_type port_type,
    u_int port)
{
	/* The port must exist */
	if (!siba_is_port_valid(core_id, port_type, port))
		return (0);

	switch (port_type) {
	case BHND_PORT_DEVICE:
		/* The first address space, if any, is mapped to device0.0 */
		if (port == 0)
			return (min(core_id->num_addrspace, 1));

		/* All remaining address spaces are mapped to device0.(n - 1) */
		if (port == 1 && core_id->num_addrspace >= 2)
			return (core_id->num_addrspace - 1);

		break;

	case BHND_PORT_AGENT:
		/* All config blocks are mapped to a single port */
		if (port == 0)
			return (core_id->num_cfg_blocks);

		break;

	default:
		break;
	}

	/* Validated above */
	panic("siba_is_port_valid() returned true for unknown %s.%u port",
	    bhnd_port_type_name(port_type), port);

}

/**
 * Map a bhnd(4) type/port/region triplet to its associated config block index,
 * if any.
 * 
 * We map config registers to port/region identifiers as follows:
 * 
 * 	[port].[region]	[cfg register block]
 * 	agent0.0	0
 * 	agent0.1	1
 * 
 * @param num_addrspace The number of available siba address spaces.
 * @param port_type The bhnd(4) port type.
 * @param port The bhnd(4) port number.
 * @param region The bhnd(4) port region.
 * @param addridx On success, the corresponding addrspace index.
 * 
 * @retval 0 success
 * @retval ENOENT if the given type/port/region cannot be mapped to a
 * siba config register block.
 */
int
siba_cfg_index(struct siba_core_id *core_id, bhnd_port_type port_type,
    u_int port, u_int region, u_int *cfgidx)
{
	/* Config blocks are mapped to agent ports */
	if (port_type != BHND_PORT_AGENT)
		return (ENOENT);

	/* Port must be valid */
	if (!siba_is_port_valid(core_id, port_type, port))
		return (ENOENT);

	if (region >= core_id->num_cfg_blocks)
		return (ENOENT);

	if (region >= SIBA_MAX_CFG)
		return (ENOENT);

	/* Found */
	*cfgidx = region;
	return (0);
}

/**
 * Map an bhnd(4) type/port/region triplet to its associated config block
 * entry, if any.
 *
 * The only supported port type is BHND_PORT_DEVICE.
 * 
 * @param dinfo The device info to search for a matching address space.
 * @param type The bhnd(4) port type.
 * @param port The bhnd(4) port number.
 * @param region The bhnd(4) port region.
 */
struct siba_cfg_block *
siba_find_cfg_block(struct siba_devinfo *dinfo, bhnd_port_type type, u_int port,
    u_int region)
{
	u_int	cfgidx;
	int	error;

	/* Map to addrspace index */
	error = siba_cfg_index(&dinfo->core_id, type, port, region, &cfgidx);
	if (error)
		return (NULL);

	/* Found */
	return (&dinfo->cfg[cfgidx]);
}

/**
 * Map a bhnd(4) type/port/region triplet to its associated address space
 * index, if any.
 * 
 * For compatibility with bcma(4), we map address spaces to port/region
 * identifiers as follows:
 * 
 * 	[port]		[addrspace]
 * 	device0.0	0
 * 	device1.0	1
 * 	device1.1	2
 * 	device1.2	3
 * 
 * @param core_id The siba core info.
 * @param port_type The bhnd(4) port type.
 * @param port The bhnd(4) port number.
 * @param region The bhnd(4) port region.
 * @param addridx On success, the corresponding addrspace index.
 * 
 * @retval 0 success
 * @retval ENOENT if the given type/port/region cannot be mapped to a
 * siba address space.
 */
int
siba_addrspace_index(struct siba_core_id *core_id, bhnd_port_type port_type,
    u_int port, u_int region, u_int *addridx)
{
	u_int idx;

	/* Address spaces are always device ports */
	if (port_type != BHND_PORT_DEVICE)
		return (ENOENT);

	/* Port must be valid */
	if (!siba_is_port_valid(core_id, port_type, port))
		return (ENOENT);
	
	if (port == 0)
		idx = region;
	else if (port == 1)
		idx = region + 1;
	else
		return (ENOENT);

	if (idx >= core_id->num_addrspace)
		return (ENOENT);

	/* Found */
	*addridx = idx;
	return (0);
}

/**
 * Map an bhnd(4) type/port/region triplet to its associated address space
 * entry, if any.
 *
 * The only supported port type is BHND_PORT_DEVICE.
 * 
 * @param dinfo The device info to search for a matching address space.
 * @param type The bhnd(4) port type.
 * @param port The bhnd(4) port number.
 * @param region The bhnd(4) port region.
 */
struct siba_addrspace *
siba_find_addrspace(struct siba_devinfo *dinfo, bhnd_port_type type, u_int port,
    u_int region)
{
	u_int	addridx;
	int	error;

	/* Map to addrspace index */
	error = siba_addrspace_index(&dinfo->core_id, type, port, region,
	    &addridx);
	if (error)
		return (NULL);

	/* Found */
	if (addridx >= SIBA_MAX_ADDRSPACE)
		return (NULL);

	return (&dinfo->addrspace[addridx]);
}

/**
 * Append an address space entry to @p dinfo.
 * 
 * @param dinfo The device info entry to update.
 * @param addridx The address space index.
 * @param base The mapping's base address.
 * @param size The mapping size.
 * @param bus_reserved Number of bytes to reserve in @p size for bus use
 * when registering the resource list entry. This is used to reserve bus
 * access to the core's SIBA_CFG* register blocks.
 * 
 * @retval 0 success
 * @retval non-zero An error occurred appending the entry.
 */
int
siba_append_dinfo_region(struct siba_devinfo *dinfo, uint8_t addridx,
    uint32_t base, uint32_t size, uint32_t bus_reserved)
{
	struct siba_addrspace	*sa;
	rman_res_t		 r_size;

	/* Verify that base + size will not overflow */
	if (size > 0 && UINT32_MAX - (size - 1) < base)
		return (ERANGE);

	/* Verify that size - bus_reserved will not underflow */
	if (size < bus_reserved)
		return (ERANGE);

	/* Must not be 0-length */
	if (size == 0)
		return (EINVAL);

	/* Must not exceed addrspace array size */
	if (addridx >= nitems(dinfo->addrspace))
		return (EINVAL);

	/* Initialize new addrspace entry */
	sa = &dinfo->addrspace[addridx];
	sa->sa_base = base;
	sa->sa_size = size;
	sa->sa_bus_reserved = bus_reserved;

	/* Populate the resource list */
	r_size = size - bus_reserved;
	sa->sa_rid = resource_list_add_next(&dinfo->resources, SYS_RES_MEMORY,
	    base, base + (r_size - 1), r_size);

	return (0);
}

/**
 * Deallocate the given device info structure and any associated resources.
 * 
 * @param dev The requesting bus device.
 * @param child The siba child device.
 * @param dinfo Device info associated with @p child to be deallocated.
 */
void
siba_free_dinfo(device_t dev, device_t child, struct siba_devinfo *dinfo)
{
	resource_list_free(&dinfo->resources);

	/* Free all mapped configuration blocks */
	for (u_int i = 0; i < nitems(dinfo->cfg); i++) {
		if (dinfo->cfg_res[i] == NULL)
			continue;

		bhnd_release_resource(dev, SYS_RES_MEMORY, dinfo->cfg_rid[i],
		    dinfo->cfg_res[i]);

		dinfo->cfg_res[i] = NULL;
		dinfo->cfg_rid[i] = -1;
	}

	/* Unmap the core's interrupt */
	if (dinfo->intr_en && dinfo->intr.mapped) {
		BHND_BUS_UNMAP_INTR(dev, child, dinfo->intr.irq);
		dinfo->intr.mapped = false;
	}

	free(dinfo, M_BHND);
}

/**
 * Return the core-enumeration-relative offset for the @p addrspace
 * SIBA_R0_ADMATCH* register.
 * 
 * @param addrspace The address space index.
 * 
 * @retval non-zero success
 * @retval 0 the given @p addrspace index is not supported.
 */
u_int
siba_admatch_offset(uint8_t addrspace)
{
	switch (addrspace) {
	case 0:
		return SB0_REG_ABS(SIBA_CFG0_ADMATCH0);
	case 1:
		return SB0_REG_ABS(SIBA_CFG0_ADMATCH1);
	case 2:
		return SB0_REG_ABS(SIBA_CFG0_ADMATCH2);
	case 3:
		return SB0_REG_ABS(SIBA_CFG0_ADMATCH3);
	default:
		return (0);
	}
}

/**
 * Parse a SIBA_R0_ADMATCH* register.
 * 
 * @param addrspace The address space index.
 * @param am The address match register value to be parsed.
 * @param[out] addr The parsed address.
 * @param[out] size The parsed size.
 * 
 * @retval 0 success
 * @retval non-zero a parse error occurred.
 */
int
siba_parse_admatch(uint32_t am, uint32_t *addr, uint32_t *size)
{
	u_int		am_type;
	
	/* Negative encoding is not supported. This is not used on any
	 * currently known devices*/
	if (am & SIBA_AM_ADNEG)
		return (EINVAL);
	
	/* Extract the base address and size */
	am_type = SIBA_REG_GET(am, AM_TYPE);
	switch (am_type) {
	case 0:
		*addr = am & SIBA_AM_BASE0_MASK;
		*size = 1 << (SIBA_REG_GET(am, AM_ADINT0) + 1);
		break;
	case 1:
		*addr = am & SIBA_AM_BASE1_MASK;
		*size = 1 << (SIBA_REG_GET(am, AM_ADINT1) + 1);
		break;
	case 2:
		*addr = am & SIBA_AM_BASE2_MASK;
		*size = 1 << (SIBA_REG_GET(am, AM_ADINT2) + 1);
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

/**
 * Write @p value to @p dev's CFG0 target/initiator state register and
 * wait for completion.
 * 
 * @param dev The siba(4) child device.
 * @param reg The state register to write (e.g. SIBA_CFG0_TMSTATELOW,
 *    SIBA_CFG0_IMSTATE)
 * @param value The value to write to @p reg.
 * @param mask The mask of bits to be included from @p value.
 * 
 * @retval 0 success.
 * @retval ENODEV if SIBA_CFG0 is not mapped by @p dinfo.
 * @retval ETIMEDOUT if a timeout occurs prior to SIBA_TMH_BUSY clearing.
 */
int
siba_write_target_state(device_t dev, struct siba_devinfo *dinfo,
    bus_size_t reg, uint32_t value, uint32_t mask)
{
	struct bhnd_resource	*r;
	uint32_t		 rval;

	/* Must have a CFG0 block */
	if ((r = dinfo->cfg_res[0]) == NULL)
		return (ENODEV);

	/* Verify the register offset falls within CFG register block */
	if (reg > SIBA_CFG_SIZE-4)
		return (EFAULT);

	for (int i = 0; i < 300; i += 10) {
		rval = bhnd_bus_read_4(r, reg);
		rval &= ~mask;
		rval |= (value & mask);

		bhnd_bus_write_4(r, reg, rval);
		bhnd_bus_read_4(r, reg); /* read-back */
		DELAY(1);

		/* If the write has completed, wait for target busy state
		 * to clear */
		rval = bhnd_bus_read_4(r, reg);
		if ((rval & mask) == (value & mask))
			return (siba_wait_target_busy(dev, dinfo, 100000));

		DELAY(10);
	}

	return (ETIMEDOUT);
}

/**
 * Spin for up to @p usec waiting for SIBA_TMH_BUSY to clear in
 * @p dev's SIBA_CFG0_TMSTATEHIGH register.
 * 
 * @param dev The siba(4) child device to wait on.
 * @param dinfo The @p dev's device info
 * 
 * @retval 0 if SIBA_TMH_BUSY is cleared prior to the @p usec timeout.
 * @retval ENODEV if SIBA_CFG0 is not mapped by @p dinfo.
 * @retval ETIMEDOUT if a timeout occurs prior to SIBA_TMH_BUSY clearing.
 */
int
siba_wait_target_busy(device_t dev, struct siba_devinfo *dinfo, int usec)
{
	struct bhnd_resource	*r;
	uint32_t		 ts_high;

	if ((r = dinfo->cfg_res[0]) == NULL)
		return (ENODEV);

	for (int i = 0; i < usec; i += 10) {
		ts_high = bhnd_bus_read_4(r, SIBA_CFG0_TMSTATEHIGH);
		if (!(ts_high & SIBA_TMH_BUSY))
			return (0);

		DELAY(10);
	}

	device_printf(dev, "SIBA_TMH_BUSY wait timeout\n");
	return (ETIMEDOUT);
}
