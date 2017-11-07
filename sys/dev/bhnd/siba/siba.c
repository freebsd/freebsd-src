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
#include <dev/bhnd/cores/pmu/bhnd_pmu.h>

#include "sibareg.h"
#include "sibavar.h"

static bhnd_erom_class_t *
siba_get_erom_class(driver_t *driver)
{
	return (&siba_erom_parser);
}

int
siba_probe(device_t dev)
{
	device_set_desc(dev, "SIBA BHND bus");
	return (BUS_PROBE_DEFAULT);
}

/**
 * Default siba(4) bus driver implementation of DEVICE_ATTACH().
 * 
 * This implementation initializes internal siba(4) state and performs
 * bus enumeration, and must be called by subclassing drivers in
 * DEVICE_ATTACH() before any other bus methods.
 */
int
siba_attach(device_t dev)
{
	struct siba_softc	*sc;
	int			 error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	/* Enumerate children */
	if ((error = siba_add_children(dev))) {
		device_delete_children(dev);
		return (error);
	}

	return (0);
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
	case BHND_IVAR_PMU_INFO:
		*result = (uintptr_t) dinfo->pmu_info;
		return (0);
	default:
		return (ENOENT);
	}
}

static int
siba_write_ivar(device_t dev, device_t child, int index, uintptr_t value)
{
	struct siba_devinfo *dinfo;

	dinfo = device_get_ivars(child);

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
	case BHND_IVAR_PMU_INFO:
		dinfo->pmu_info = (struct bhnd_core_pmu_info *) value;
		return (0);
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

static int
siba_read_iost(device_t dev, device_t child, uint16_t *iost)
{
	uint32_t	tmhigh;
	int		error;

	error = bhnd_read_config(child, SIBA_CFG0_TMSTATEHIGH, &tmhigh, 4);
	if (error)
		return (error);

	*iost = (SIBA_REG_GET(tmhigh, TMH_SISF));
	return (0);
}

static int
siba_read_ioctl(device_t dev, device_t child, uint16_t *ioctl)
{
	uint32_t	ts_low;
	int		error;

	if ((error = bhnd_read_config(child, SIBA_CFG0_TMSTATELOW, &ts_low, 4)))
		return (error);

	*ioctl = (SIBA_REG_GET(ts_low, TML_SICF));
	return (0);
}

static int
siba_write_ioctl(device_t dev, device_t child, uint16_t value, uint16_t mask)
{
	struct siba_devinfo	*dinfo;
	struct bhnd_resource	*r;
	uint32_t		 ts_low, ts_mask;

	if (device_get_parent(child) != dev)
		return (EINVAL);

	/* Fetch CFG0 mapping */
	dinfo = device_get_ivars(child);
	if ((r = dinfo->cfg[0]) == NULL)
		return (ENODEV);

	/* Mask and set TMSTATELOW core flag bits */
	ts_mask = (mask << SIBA_TML_SICF_SHIFT) & SIBA_TML_SICF_MASK;
	ts_low = (value << SIBA_TML_SICF_SHIFT) & ts_mask;

	return (siba_write_target_state(child, dinfo, SIBA_CFG0_TMSTATELOW,
	    ts_low, ts_mask));
}

static bool
siba_is_hw_suspended(device_t dev, device_t child)
{
	uint32_t		ts_low;
	uint16_t		ioctl;
	int			error;

	/* Fetch target state */
	error = bhnd_read_config(child, SIBA_CFG0_TMSTATELOW, &ts_low, 4);
	if (error) {
		device_printf(child, "error reading HW reset state: %d\n",
		    error);
		return (true);
	}

	/* Is core held in RESET? */
	if (ts_low & SIBA_TML_RESET)
		return (true);

	/* Is core clocked? */
	ioctl = SIBA_REG_GET(ts_low, TML_SICF);
	if (!(ioctl & BHND_IOCTL_CLK_EN))
		return (true);

	return (false);
}

static int
siba_reset_hw(device_t dev, device_t child, uint16_t ioctl)
{
	struct siba_devinfo		*dinfo;
	struct bhnd_resource		*r;
	uint32_t			 ts_low, imstate;
	int				 error;

	if (device_get_parent(child) != dev)
		return (EINVAL);

	dinfo = device_get_ivars(child);

	/* Can't suspend the core without access to the CFG0 registers */
	if ((r = dinfo->cfg[0]) == NULL)
		return (ENODEV);

	/* We require exclusive control over BHND_IOCTL_CLK_EN and
	 * BHND_IOCTL_CLK_FORCE. */
	if (ioctl & (BHND_IOCTL_CLK_EN | BHND_IOCTL_CLK_FORCE))
		return (EINVAL);

	/* Place core into known RESET state */
	if ((error = BHND_BUS_SUSPEND_HW(dev, child)))
		return (error);

	/* Leaving the core in reset, set the caller's IOCTL flags and
	 * enable the core's clocks. */
	ts_low = (ioctl | BHND_IOCTL_CLK_EN | BHND_IOCTL_CLK_FORCE) <<
	    SIBA_TML_SICF_SHIFT;
	error = siba_write_target_state(child, dinfo, SIBA_CFG0_TMSTATELOW,
	    ts_low, SIBA_TML_SICF_MASK);
	if (error)
		return (error);

	/* Clear any target errors */
	if (bhnd_bus_read_4(r, SIBA_CFG0_TMSTATEHIGH) & SIBA_TMH_SERR) {
		error = siba_write_target_state(child, dinfo,
		    SIBA_CFG0_TMSTATEHIGH, 0, SIBA_TMH_SERR);
		if (error)
			return (error);
	}

	/* Clear any initiator errors */
	imstate = bhnd_bus_read_4(r, SIBA_CFG0_IMSTATE);
	if (imstate & (SIBA_IM_IBE|SIBA_IM_TO)) {
		error = siba_write_target_state(child, dinfo, SIBA_CFG0_IMSTATE,
		    0, SIBA_IM_IBE|SIBA_IM_TO);
		if (error)
			return (error);
	}

	/* Release from RESET while leaving clocks forced, ensuring the
	 * signal propagates throughout the core */
	error = siba_write_target_state(child, dinfo, SIBA_CFG0_TMSTATELOW,
	    0x0, SIBA_TML_RESET);
	if (error)
		return (error);

	/* The core should now be active; we can clear the BHND_IOCTL_CLK_FORCE
	 * bit and allow the core to manage clock gating. */
	error = siba_write_target_state(child, dinfo, SIBA_CFG0_TMSTATELOW,
	    0x0, (BHND_IOCTL_CLK_FORCE << SIBA_TML_SICF_SHIFT));
	if (error)
		return (error);

	return (0);
}

static int
siba_suspend_hw(device_t dev, device_t child)
{
	struct siba_devinfo		*dinfo;
	struct bhnd_core_pmu_info	*pm;
	struct bhnd_resource		*r;
	uint32_t			 idl, ts_low;
	uint16_t			 ioctl;
	int				 error;

	if (device_get_parent(child) != dev)
		return (EINVAL);

	dinfo = device_get_ivars(child);
	pm = dinfo->pmu_info;

	/* Can't suspend the core without access to the CFG0 registers */
	if ((r = dinfo->cfg[0]) == NULL)
		return (ENODEV);

	/* Already in RESET? */
	ts_low = bhnd_bus_read_4(r, SIBA_CFG0_TMSTATELOW);
	if (ts_low & SIBA_TML_RESET) {
		/* Clear IOCTL flags, ensuring the clock is disabled */
		return (siba_write_target_state(child, dinfo,
		    SIBA_CFG0_TMSTATELOW, 0x0, SIBA_TML_SICF_MASK));

		return (0);
	}

	/* If clocks are already disabled, we can put the core directly
	 * into RESET */
	ioctl = SIBA_REG_GET(ts_low, TML_SICF);
	if (!(ioctl & BHND_IOCTL_CLK_EN)) {
		/* Set RESET and clear IOCTL flags */
		return (siba_write_target_state(child, dinfo, 
		    SIBA_CFG0_TMSTATELOW,
		    SIBA_TML_RESET,
		    SIBA_TML_RESET | SIBA_TML_SICF_MASK));
	}

	/* Reject any further target backplane transactions */
	error = siba_write_target_state(child, dinfo, SIBA_CFG0_TMSTATELOW,
	    SIBA_TML_REJ, SIBA_TML_REJ);
	if (error)
		return (error);

	/* If this is an initiator core, we need to reject initiator
	 * transactions too. */
	idl = bhnd_bus_read_4(r, SIBA_CFG0_IDLOW);
	if (idl & SIBA_IDL_INIT) {
		error = siba_write_target_state(child, dinfo, SIBA_CFG0_IMSTATE,
		    SIBA_IM_RJ, SIBA_IM_RJ);
		if (error)
			return (error);
	}

	/* Put the core into RESET|REJECT, forcing clocks to ensure the RESET
	 * signal propagates throughout the core, leaving REJECT asserted. */
	ts_low = SIBA_TML_RESET;
	ts_low |= (BHND_IOCTL_CLK_EN | BHND_IOCTL_CLK_FORCE) <<
	    SIBA_TML_SICF_SHIFT;

	error = siba_write_target_state(child, dinfo, SIBA_CFG0_TMSTATELOW,
		ts_low, ts_low);
	if (error)
		return (error);

	/* Give RESET ample time */
	DELAY(10);

	/* Leaving core in reset, disable all clocks, clear REJ flags and
	 * IOCTL state */
	error = siba_write_target_state(child, dinfo, SIBA_CFG0_TMSTATELOW,
		SIBA_TML_RESET,
		SIBA_TML_RESET | SIBA_TML_REJ | SIBA_TML_SICF_MASK);
	if (error)
		return (error);

	/* Clear previously asserted initiator reject */
	if (idl & SIBA_IDL_INIT) {
		error = siba_write_target_state(child, dinfo, SIBA_CFG0_IMSTATE,
		    0, SIBA_IM_RJ);
		if (error)
			return (error);
	}

	/* Core is now in RESET, with clocks disabled and REJ not asserted.
	 * 
	 * We lastly need to inform the PMU, releasing any outstanding per-core
	 * PMU requests */	
	if (pm != NULL) {
		if ((error = BHND_PMU_CORE_RELEASE(pm->pm_pmu, pm)))
			return (error);
	}

	return (0);
}

static int
siba_read_config(device_t dev, device_t child, bus_size_t offset, void *value,
    u_int width)
{
	struct siba_devinfo	*dinfo;
	rman_res_t		 r_size;

	/* Must be directly attached */
	if (device_get_parent(child) != dev)
		return (EINVAL);

	/* CFG0 registers must be available */
	dinfo = device_get_ivars(child);
	if (dinfo->cfg[0] == NULL)
		return (ENODEV);

	/* Offset must fall within CFG0 */
	r_size = rman_get_size(dinfo->cfg[0]->res);
	if (r_size < offset || r_size - offset < width)
		return (EFAULT);

	switch (width) {
	case 1:
		*((uint8_t *)value) = bhnd_bus_read_1(dinfo->cfg[0], offset);
		return (0);
	case 2:
		*((uint16_t *)value) = bhnd_bus_read_2(dinfo->cfg[0], offset);
		return (0);
	case 4:
		*((uint32_t *)value) = bhnd_bus_read_4(dinfo->cfg[0], offset);
		return (0);
	default:
		return (EINVAL);
	}
}

static int
siba_write_config(device_t dev, device_t child, bus_size_t offset,
    const void *value, u_int width)
{
	struct siba_devinfo	*dinfo;
	struct bhnd_resource	*r;
	rman_res_t		 r_size;

	/* Must be directly attached */
	if (device_get_parent(child) != dev)
		return (EINVAL);

	/* CFG0 registers must be available */
	dinfo = device_get_ivars(child);
	if ((r = dinfo->cfg[0]) == NULL)
		return (ENODEV);

	/* Offset must fall within CFG0 */
	r_size = rman_get_size(r->res);
	if (r_size < offset || r_size - offset < width)
		return (EFAULT);

	switch (width) {
	case 1:
		bhnd_bus_write_1(r, offset, *(const uint8_t *)value);
		return (0);
	case 2:
		bhnd_bus_write_2(r, offset, *(const uint8_t *)value);
		return (0);
	case 4:
		bhnd_bus_write_4(r, offset, *(const uint8_t *)value);
		return (0);
	default:
		return (EINVAL);
	}
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
	return (siba_addrspace_port_count(dinfo->core_id.num_addrspace));
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
	if (!siba_is_port_valid(dinfo->core_id.num_addrspace, type, port))
		return (0);

	return (siba_addrspace_region_count(dinfo->core_id.num_addrspace,
	    port));
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
 * Default siba(4) bus driver implementation of BHND_BUS_GET_INTR_COUNT().
 * 
 * This implementation consults @p child's configuration block mapping,
 * returning SIBA_CORE_NUM_INTR if a valid CFG0 block is mapped.
 */
int
siba_get_intr_count(device_t dev, device_t child)
{
	struct siba_devinfo *dinfo;

	/* delegate non-bus-attached devices to our parent */
	if (device_get_parent(child) != dev)
		return (BHND_BUS_GET_INTR_COUNT(device_get_parent(dev), child));

	dinfo = device_get_ivars(child);

	/* We can get/set interrupt sbflags on any core with a valid cfg0
	 * block; whether the core actually makes use of it is another matter
	 * entirely */
	if (dinfo->cfg[0] == NULL)
		return (0);

	return (SIBA_CORE_NUM_INTR);
}

/**
 * Default siba(4) bus driver implementation of BHND_BUS_GET_CORE_IVEC().
 * 
 * This implementation consults @p child's CFG0 register block,
 * returning the interrupt flag assigned to @p child.
 */
int
siba_get_core_ivec(device_t dev, device_t child, u_int intr, uint32_t *ivec)
{
	struct siba_devinfo	*dinfo;
	uint32_t		 tpsflag;

	/* delegate non-bus-attached devices to our parent */
	if (device_get_parent(child) != dev)
		return (BHND_BUS_GET_CORE_IVEC(device_get_parent(dev), child,
		    intr, ivec));

	/* Must be a valid interrupt ID */
	if (intr >= siba_get_intr_count(dev, child))
		return (ENXIO);

	/* Fetch sbflag number */
	dinfo = device_get_ivars(child);
	tpsflag = bhnd_bus_read_4(dinfo->cfg[0], SIBA_CFG0_TPSFLAG);
	*ivec = SIBA_REG_GET(tpsflag, TPS_NUM0);

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
    struct bhnd_resource *r)
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
		adm = bhnd_bus_read_4(r, adm_offset);

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

/**
 * Map per-core configuration blocks for @p dinfo.
 *
 * @param dev The siba bus device.
 * @param dinfo The device info instance on which to map all per-core
 * configuration blocks.
 */
static int
siba_map_cfg_resources(device_t dev, struct siba_devinfo *dinfo)
{
	struct siba_addrspace	*addrspace;
	rman_res_t		 r_start, r_count, r_end;
	uint8_t			 num_cfg;

	num_cfg = dinfo->core_id.num_cfg_blocks;
	if (num_cfg > SIBA_MAX_CFG) {
		device_printf(dev, "config block count %hhu out of range\n",
		    num_cfg);
		return (ENXIO);
	}
	
	/* Fetch the core register address space */
	addrspace = siba_find_addrspace(dinfo, BHND_PORT_DEVICE, 0, 0);
	if (addrspace == NULL) {
		device_printf(dev, "missing device registers\n");
		return (ENXIO);
	}

	/*
	 * Map the per-core configuration blocks
	 */
	for (uint8_t i = 0; i < num_cfg; i++) {
		/* Determine the config block's address range; configuration
		 * blocks are allocated starting at SIBA_CFG0_OFFSET,
		 * growing downwards. */
		r_start = addrspace->sa_base + SIBA_CFG0_OFFSET;
		r_start -= i * SIBA_CFG_SIZE;

		r_count = SIBA_CFG_SIZE;
		r_end = r_start + r_count - 1;

		/* Allocate the config resource */
		dinfo->cfg_rid[i] = SIBA_CFG_RID(dinfo, i);
		dinfo->cfg[i] = BHND_BUS_ALLOC_RESOURCE(dev, dev,
		    SYS_RES_MEMORY, &dinfo->cfg_rid[i], r_start, r_end,
		    r_count, RF_ACTIVE);

		if (dinfo->cfg[i] == NULL) {
			device_printf(dev, "failed to allocate SIBA_CFG%hhu\n",
			    i);
			return (ENXIO);
		}
	}

	return (0);
}

static device_t
siba_add_child(device_t dev, u_int order, const char *name, int unit)
{
	struct siba_devinfo	*dinfo;
	device_t		 child;

	child = device_add_child_ordered(dev, order, name, unit);
	if (child == NULL)
		return (NULL);

	if ((dinfo = siba_alloc_dinfo(dev)) == NULL) {
		device_delete_child(dev, child);
		return (NULL);
	}

	device_set_ivars(child, dinfo);

	return (child);
}

static void
siba_child_deleted(device_t dev, device_t child)
{
	struct bhnd_softc	*sc;
	struct siba_devinfo	*dinfo;

	sc = device_get_softc(dev);

	/* Call required bhnd(4) implementation */
	bhnd_generic_child_deleted(dev, child);

	/* Free siba device info */
	if ((dinfo = device_get_ivars(child)) != NULL)
		siba_free_dinfo(dev, dinfo);

	device_set_ivars(child, NULL);
}

/**
 * Scan the core table and add all valid discovered cores to
 * the bus.
 * 
 * @param dev The siba bus device.
 */
int
siba_add_children(device_t dev)
{
	const struct bhnd_chipid	*chipid;
	struct siba_core_id		*cores;
	struct bhnd_resource		*r;
	device_t			*children;
	int				 rid;
	int				 error;

	cores = NULL;
	r = NULL;

	chipid = BHND_BUS_GET_CHIPID(dev, dev);

	/* Allocate our temporary core and device table */
	cores = malloc(sizeof(*cores) * chipid->ncores, M_BHND, M_WAITOK);
	children = malloc(sizeof(*children) * chipid->ncores, M_BHND,
	    M_WAITOK | M_ZERO);

	/*
	 * Add child devices for all discovered cores.
	 * 
	 * On bridged devices, we'll exhaust our available register windows if
	 * we map config blocks on unpopulated/disabled cores. To avoid this, we
	 * defer mapping of the per-core siba(4) config blocks until all cores
	 * have been enumerated and otherwise configured.
	 */
	for (u_int i = 0; i < chipid->ncores; i++) {
		struct siba_devinfo	*dinfo;
		uint32_t		 idhigh, idlow;
		rman_res_t		 r_count, r_end, r_start;

		/* Map the core's register block */
		rid = 0;
		r_start = SIBA_CORE_ADDR(i);
		r_count = SIBA_CORE_SIZE;
		r_end = r_start + SIBA_CORE_SIZE - 1;
		r = bhnd_alloc_resource(dev, SYS_RES_MEMORY, &rid, r_start,
		    r_end, r_count, RF_ACTIVE);
		if (r == NULL) {
			error = ENXIO;
			goto failed;
		}

		/* Read the core info */
		idhigh = bhnd_bus_read_4(r, SB0_REG_ABS(SIBA_CFG0_IDHIGH));
		idlow = bhnd_bus_read_4(r, SB0_REG_ABS(SIBA_CFG0_IDLOW));

		cores[i] = siba_parse_core_id(idhigh, idlow, i, 0);

		/* Determine and set unit number */
		for (u_int j = 0; j < i; j++) {
			struct bhnd_core_info *cur = &cores[i].core_info;
			struct bhnd_core_info *prev = &cores[j].core_info;

			if (prev->vendor == cur->vendor &&
			    prev->device == cur->device)
				cur->unit++;
		}

		/* Add the child device */
		children[i] = BUS_ADD_CHILD(dev, 0, NULL, -1);
		if (children[i] == NULL) {
			error = ENXIO;
			goto failed;
		}

		/* Initialize per-device bus info */
		if ((dinfo = device_get_ivars(children[i])) == NULL) {
			error = ENXIO;
			goto failed;
		}

		if ((error = siba_init_dinfo(dev, dinfo, &cores[i])))
			goto failed;

		/* Register the core's address space(s). */
		if ((error = siba_register_addrspaces(dev, dinfo, r)))
			goto failed;

		/* Unmap the core's register block */
		bhnd_release_resource(dev, SYS_RES_MEMORY, rid, r);
		r = NULL;

		/* If pins are floating or the hardware is otherwise
		 * unpopulated, the device shouldn't be used. */
		if (bhnd_is_hw_disabled(children[i]))
			device_disable(children[i]);
	}

	/* Map all valid core's config register blocks and perform interrupt
	 * assignment */
	for (u_int i = 0; i < chipid->ncores; i++) {
		struct siba_devinfo	*dinfo;
		device_t		 child;
		int			 nintr;

		child = children[i];

		/* Skip if core is disabled */
		if (bhnd_is_hw_disabled(child))
			continue;

		dinfo = device_get_ivars(child);

		/* Map the core's config blocks */
		if ((error = siba_map_cfg_resources(dev, dinfo)))
			goto failed;

		/* Assign interrupts */
		nintr = bhnd_get_intr_count(child);
		for (int rid = 0; rid < nintr; rid++) {
			error = BHND_BUS_ASSIGN_INTR(dev, child, rid);
			if (error) {
				device_printf(dev, "failed to assign interrupt "
				    "%d to core %u: %d\n", rid, i, error);
			}
		}

		/* Issue bus callback for fully initialized child. */
		BHND_BUS_CHILD_ADDED(dev, child);
	}

	free(cores, M_BHND);
	free(children, M_BHND);

	return (0);

failed:
	for (u_int i = 0; i < chipid->ncores; i++) {
		if (children[i] == NULL)
			continue;

		device_delete_child(dev, children[i]);
	}

	free(cores, M_BHND);
	free(children, M_BHND);

	if (r != NULL)
		bhnd_release_resource(dev, SYS_RES_MEMORY, rid, r);

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
	DEVMETHOD(bus_add_child,		siba_add_child),
	DEVMETHOD(bus_child_deleted,		siba_child_deleted),
	DEVMETHOD(bus_read_ivar,		siba_read_ivar),
	DEVMETHOD(bus_write_ivar,		siba_write_ivar),
	DEVMETHOD(bus_get_resource_list,	siba_get_resource_list),

	/* BHND interface */
	DEVMETHOD(bhnd_bus_get_erom_class,	siba_get_erom_class),
	DEVMETHOD(bhnd_bus_read_ioctl,		siba_read_ioctl),
	DEVMETHOD(bhnd_bus_write_ioctl,		siba_write_ioctl),
	DEVMETHOD(bhnd_bus_read_iost,		siba_read_iost),
	DEVMETHOD(bhnd_bus_is_hw_suspended,	siba_is_hw_suspended),
	DEVMETHOD(bhnd_bus_reset_hw,		siba_reset_hw),
	DEVMETHOD(bhnd_bus_suspend_hw,		siba_suspend_hw),
	DEVMETHOD(bhnd_bus_read_config,		siba_read_config),
	DEVMETHOD(bhnd_bus_write_config,	siba_write_config),
	DEVMETHOD(bhnd_bus_get_port_count,	siba_get_port_count),
	DEVMETHOD(bhnd_bus_get_region_count,	siba_get_region_count),
	DEVMETHOD(bhnd_bus_get_port_rid,	siba_get_port_rid),
	DEVMETHOD(bhnd_bus_decode_port_rid,	siba_decode_port_rid),
	DEVMETHOD(bhnd_bus_get_region_addr,	siba_get_region_addr),
	DEVMETHOD(bhnd_bus_get_intr_count,	siba_get_intr_count),
	DEVMETHOD(bhnd_bus_get_core_ivec,	siba_get_core_ivec),

	DEVMETHOD_END
};

DEFINE_CLASS_1(bhnd, siba_driver, siba_methods, sizeof(struct siba_softc), bhnd_driver);

MODULE_VERSION(siba, 1);
MODULE_DEPEND(siba, bhnd, 1, 1, 1);
