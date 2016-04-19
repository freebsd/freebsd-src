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

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/bhnd/cores/chipc/chipcreg.h>

#include "bhndreg.h"
#include "bhndvar.h"

/* BHND core device description table. */
static const struct bhnd_core_desc {
	uint16_t	 vendor;
	uint16_t	 device;
	bhnd_devclass_t	 class;
	const char	*desc;
} bhnd_core_descs[] = {
	#define	BHND_CDESC(_mfg, _cid, _cls, _desc)		\
	    { BHND_MFGID_ ## _mfg, BHND_COREID_ ## _cid,	\
		BHND_DEVCLASS_ ## _cls, _desc }

	BHND_CDESC(BCM, CC,		CC,		"ChipCommon I/O Controller"),
	BHND_CDESC(BCM, ILINE20,	OTHER,		"iLine20 HPNA"),
	BHND_CDESC(BCM, SRAM,		RAM,		"SRAM"),
	BHND_CDESC(BCM, SDRAM,		RAM,		"SDRAM"),
	BHND_CDESC(BCM, PCI,		PCI,		"PCI Bridge"),
	BHND_CDESC(BCM, MIPS,		CPU,		"MIPS Core"),
	BHND_CDESC(BCM, ENET,		ENET_MAC,	"Fast Ethernet MAC"),
	BHND_CDESC(BCM, CODEC,		OTHER,		"V.90 Modem Codec"),
	BHND_CDESC(BCM, USB,		OTHER,		"USB 1.1 Device/Host Controller"),
	BHND_CDESC(BCM, ADSL,		OTHER,		"ADSL Core"),
	BHND_CDESC(BCM, ILINE100,	OTHER,		"iLine100 HPNA"),
	BHND_CDESC(BCM, IPSEC,		OTHER,		"IPsec Accelerator"),
	BHND_CDESC(BCM, UTOPIA,		OTHER,		"UTOPIA ATM Core"),
	BHND_CDESC(BCM, PCMCIA,		PCCARD,		"PCMCIA Bridge"),
	BHND_CDESC(BCM, SOCRAM,		RAM,		"Internal Memory"),
	BHND_CDESC(BCM, MEMC,		MEMC,		"MEMC SDRAM Controller"),
	BHND_CDESC(BCM, OFDM,		OTHER,		"OFDM PHY"),
	BHND_CDESC(BCM, EXTIF,		OTHER,		"External Interface"),
	BHND_CDESC(BCM, D11,		WLAN,		"802.11 MAC/PHY/Radio"),
	BHND_CDESC(BCM, APHY,		WLAN_PHY,	"802.11a PHY"),
	BHND_CDESC(BCM, BPHY,		WLAN_PHY,	"802.11b PHY"),
	BHND_CDESC(BCM, GPHY,		WLAN_PHY,	"802.11g PHY"),
	BHND_CDESC(BCM, MIPS33,		CPU,		"MIPS 3302 Core"),
	BHND_CDESC(BCM, USB11H,		OTHER,		"USB 1.1 Host Controller"),
	BHND_CDESC(BCM, USB11D,		OTHER,		"USB 1.1 Device Core"),
	BHND_CDESC(BCM, USB20H,		OTHER,		"USB 2.0 Host Controller"),
	BHND_CDESC(BCM, USB20D,		OTHER,		"USB 2.0 Device Core"),
	BHND_CDESC(BCM, SDIOH,		OTHER,		"SDIO Host Controller"),
	BHND_CDESC(BCM, ROBO,		OTHER,		"RoboSwitch"),
	BHND_CDESC(BCM, ATA100,		OTHER,		"Parallel ATA Controller"),
	BHND_CDESC(BCM, SATAXOR,	OTHER,		"SATA DMA/XOR Controller"),
	BHND_CDESC(BCM, GIGETH,		ENET_MAC,	"Gigabit Ethernet MAC"),
	BHND_CDESC(BCM, PCIE,		PCIE,		"PCIe Bridge"),
	BHND_CDESC(BCM, NPHY,		WLAN_PHY,	"802.11n 2x2 PHY"),
	BHND_CDESC(BCM, SRAMC,		MEMC,		"SRAM Controller"),
	BHND_CDESC(BCM, MINIMAC,	OTHER,		"MINI MAC/PHY"),
	BHND_CDESC(BCM, ARM11,		CPU,		"ARM1176 CPU"),
	BHND_CDESC(BCM, ARM7S,		CPU,		"ARM7TDMI-S CPU"),
	BHND_CDESC(BCM, LPPHY,		WLAN_PHY,	"802.11a/b/g PHY"),
	BHND_CDESC(BCM, PMU,		PMU,		"PMU"),
	BHND_CDESC(BCM, SSNPHY,		WLAN_PHY,	"802.11n Single-Stream PHY"),
	BHND_CDESC(BCM, SDIOD,		OTHER,		"SDIO Device Core"),
	BHND_CDESC(BCM, ARMCM3,		CPU,		"ARM Cortex-M3 CPU"),
	BHND_CDESC(BCM, HTPHY,		WLAN_PHY,	"802.11n 4x4 PHY"),
	BHND_CDESC(BCM, MIPS74K,	CPU,		"MIPS74k CPU"),
	BHND_CDESC(BCM, GMAC,		ENET_MAC,	"Gigabit MAC core"),
	BHND_CDESC(BCM, DMEMC,		MEMC,		"DDR1/DDR2 Memory Controller"),
	BHND_CDESC(BCM, PCIERC,		OTHER,		"PCIe Root Complex"),
	BHND_CDESC(BCM, OCP,		SOC_BRIDGE,	"OCP to OCP Bridge"),
	BHND_CDESC(BCM, SC,		OTHER,		"Shared Common Core"),
	BHND_CDESC(BCM, AHB,		SOC_BRIDGE,	"OCP to AHB Bridge"),
	BHND_CDESC(BCM, SPIH,		OTHER,		"SPI Host Controller"),
	BHND_CDESC(BCM, I2S,		OTHER,		"I2S Digital Audio Interface"),
	BHND_CDESC(BCM, DMEMS,		MEMC,		"SDR/DDR1 Memory Controller"),
	BHND_CDESC(BCM, UBUS_SHIM,	OTHER,		"BCM6362/UBUS WLAN SHIM"),
	BHND_CDESC(BCM, PCIE2,		PCIE,		"PCIe Bridge (Gen2)"),

	BHND_CDESC(ARM, APB_BRIDGE,	SOC_BRIDGE,	"BP135 AMBA3 AXI to APB Bridge"),
	BHND_CDESC(ARM, PL301,		SOC_ROUTER,	"PL301 AMBA3 Interconnect"),
	BHND_CDESC(ARM, EROM,		EROM,		"PL366 Device Enumeration ROM"),
	BHND_CDESC(ARM, OOB_ROUTER,	OTHER,		"PL367 OOB Interrupt Router"),
	BHND_CDESC(ARM, AXI_UNMAPPED,	OTHER,		"Unmapped Address Ranges"),

	BHND_CDESC(BCM, 4706_CC,	CC,		"ChipCommon I/O Controller"),
	BHND_CDESC(BCM, NS_PCIE2,	PCIE,		"PCIe Bridge (Gen2)"),
	BHND_CDESC(BCM, NS_DMA,		OTHER,		"DMA engine"),
	BHND_CDESC(BCM, NS_SDIO,	OTHER,		"SDIO 3.0 Host Controller"),
	BHND_CDESC(BCM, NS_USB20H,	OTHER,		"USB 2.0 Host Controller"),
	BHND_CDESC(BCM, NS_USB30H,	OTHER,		"USB 3.0 Host Controller"),
	BHND_CDESC(BCM, NS_A9JTAG,	OTHER,		"ARM Cortex A9 JTAG Interface"),
	BHND_CDESC(BCM, NS_DDR23_MEMC,	MEMC,		"Denali DDR2/DD3 Memory Controller"),
	BHND_CDESC(BCM, NS_ROM,		NVRAM,		"System ROM"),
	BHND_CDESC(BCM, NS_NAND,	NVRAM,		"NAND Flash Controller"),
	BHND_CDESC(BCM, NS_QSPI,	NVRAM,		"QSPI Flash Controller"),
	BHND_CDESC(BCM, NS_CC_B,	CC_B,		"ChipCommon B Auxiliary I/O Controller"),
	BHND_CDESC(BCM, 4706_SOCRAM,	RAM,		"Internal Memory"),
	BHND_CDESC(BCM, IHOST_ARMCA9,	CPU,		"ARM Cortex A9 CPU"),
	BHND_CDESC(BCM, 4706_GMAC_CMN,	ENET,		"Gigabit MAC (Common)"),
	BHND_CDESC(BCM, 4706_GMAC,	ENET_MAC,	"Gigabit MAC"),
	BHND_CDESC(BCM, AMEMC,		MEMC,		"Denali DDR1/DDR2 Memory Controller"),
#undef	BHND_CDESC

	/* Derived from inspection of the BCM4331 cores that provide PrimeCell
	 * IDs. Due to lack of documentation, the surmised device name/purpose
	 * provided here may be incorrect. */
	{ BHND_MFGID_ARM,	BHND_PRIMEID_EROM,	BHND_DEVCLASS_OTHER,
	    "PL364 Device Enumeration ROM" },
	{ BHND_MFGID_ARM,	BHND_PRIMEID_SWRAP,	BHND_DEVCLASS_OTHER,
	    "PL368 Device Management Interface" },
	{ BHND_MFGID_ARM,	BHND_PRIMEID_MWRAP,	BHND_DEVCLASS_OTHER,
	    "PL369 Device Management Interface" },

	{ 0, 0, 0, NULL }
};

/**
 * Return the name for a given JEP106 manufacturer ID.
 * 
 * @param vendor A JEP106 Manufacturer ID, including the non-standard ARM 4-bit
 * JEP106 continuation code.
 */
const char *
bhnd_vendor_name(uint16_t vendor)
{
	switch (vendor) {
	case BHND_MFGID_ARM:
		return "ARM";
	case BHND_MFGID_BCM:
		return "Broadcom";
	case BHND_MFGID_MIPS:
		return "MIPS";
	default:
		return "unknown";
	}
}

/**
 * Return the name of a port type.
 */
const char *
bhnd_port_type_name(bhnd_port_type port_type)
{
	switch (port_type) {
	case BHND_PORT_DEVICE:
		return ("device");
	case BHND_PORT_BRIDGE:
		return ("bridge");
	case BHND_PORT_AGENT:
		return ("agent");
	}
}


static const struct bhnd_core_desc *
bhnd_find_core_desc(uint16_t vendor, uint16_t device)
{
	for (u_int i = 0; bhnd_core_descs[i].desc != NULL; i++) {
		if (bhnd_core_descs[i].vendor != vendor)
			continue;
		
		if (bhnd_core_descs[i].device != device)
			continue;
		
		return (&bhnd_core_descs[i]);
	}
	
	return (NULL);
}

/**
 * Return a human-readable name for a BHND core.
 * 
 * @param vendor The core designer's JEDEC-106 Manufacturer ID
 * @param device The core identifier.
 */
const char *
bhnd_find_core_name(uint16_t vendor, uint16_t device)
{
	const struct bhnd_core_desc *desc;
	
	if ((desc = bhnd_find_core_desc(vendor, device)) == NULL)
		return ("unknown");

	return desc->desc;
}

/**
 * Return the device class for a BHND core.
 * 
 * @param vendor The core designer's JEDEC-106 Manufacturer ID
 * @param device The core identifier.
 */
bhnd_devclass_t
bhnd_find_core_class(uint16_t vendor, uint16_t device)
{
	const struct bhnd_core_desc *desc;
	
	if ((desc = bhnd_find_core_desc(vendor, device)) == NULL)
		return (BHND_DEVCLASS_OTHER);

	return desc->class;
}

/**
 * Return a human-readable name for a BHND core.
 * 
 * @param ci The core's info record.
 */
const char *
bhnd_core_name(const struct bhnd_core_info *ci)
{
	return bhnd_find_core_name(ci->vendor, ci->device);
}

/**
 * Return the device class for a BHND core.
 * 
 * @param ci The core's info record.
 */
bhnd_devclass_t
bhnd_core_class(const struct bhnd_core_info *ci)
{
	return bhnd_find_core_class(ci->vendor, ci->device);
}

/**
 * Initialize a core info record with data from from a bhnd-attached @p dev.
 * 
 * @param dev A bhnd device.
 * @param core The record to be initialized.
 */
struct bhnd_core_info
bhnd_get_core_info(device_t dev) {
	return (struct bhnd_core_info) {
		.vendor		= bhnd_get_vendor(dev),
		.device		= bhnd_get_device(dev),
		.hwrev		= bhnd_get_hwrev(dev),
		.core_idx	= bhnd_get_core_index(dev),
		.unit		= bhnd_get_core_unit(dev)
	};
}

/**
 * Find a @p class child device with @p unit on @p dev.
 * 
 * @param parent The bhnd-compatible bus to be searched.
 * @param class The device class to match on.
 * @param unit The device unit number; specify -1 to return the first match
 * regardless of unit number.
 * 
 * @retval device_t if a matching child device is found.
 * @retval NULL if no matching child device is found.
 */
device_t
bhnd_find_child(device_t dev, bhnd_devclass_t class, int unit)
{
	struct bhnd_core_match md = {
		.vendor = BHND_MFGID_INVALID,
		.device = BHND_COREID_INVALID,
		.hwrev.start = BHND_HWREV_INVALID,
		.hwrev.end = BHND_HWREV_INVALID,
		.class = class,
		.unit = unit
	};

	return bhnd_match_child(dev, &md);
}

/**
 * Find the first child device on @p dev that matches @p desc.
 * 
 * @param parent The bhnd-compatible bus to be searched.
 * @param desc A match descriptor.
 * 
 * @retval device_t if a matching child device is found.
 * @retval NULL if no matching child device is found.
 */
device_t
bhnd_match_child(device_t dev, const struct bhnd_core_match *desc)
{
	device_t	*devlistp;
	device_t	 match;
	int		 devcnt;
	int		 error;

	error = device_get_children(dev, &devlistp, &devcnt);
	if (error != 0)
		return (NULL);

	match = NULL;
	for (int i = 0; i < devcnt; i++) {
		device_t dev = devlistp[i];
		if (bhnd_device_matches(dev, desc)) {
			match = dev;
			goto done;
		}
	}

done:
	free(devlistp, M_TEMP);
	return match;
}

/**
 * Find the first core in @p cores that matches @p desc.
 * 
 * @param cores The table to search.
 * @param num_cores The length of @p cores.
 * @param desc A match descriptor.
 * 
 * @retval bhnd_core_info if a matching core is found.
 * @retval NULL if no matching core is found.
 */
const struct bhnd_core_info *
bhnd_match_core(const struct bhnd_core_info *cores, u_int num_cores,
    const struct bhnd_core_match *desc)
{
	for (u_int i = 0; i < num_cores; i++) {
		if (bhnd_core_matches(&cores[i], desc))
			return &cores[i];
	}

	return (NULL);
}


/**
 * Find the first core in @p cores with the given @p class.
 * 
 * @param cores The table to search.
 * @param num_cores The length of @p cores.
 * @param desc A match descriptor.
 * 
 * @retval bhnd_core_info if a matching core is found.
 * @retval NULL if no matching core is found.
 */
const struct bhnd_core_info *
bhnd_find_core(const struct bhnd_core_info *cores, u_int num_cores,
    bhnd_devclass_t class)
{
	struct bhnd_core_match md = {
		.vendor = BHND_MFGID_INVALID,
		.device = BHND_COREID_INVALID,
		.hwrev.start = BHND_HWREV_INVALID,
		.hwrev.end = BHND_HWREV_INVALID,
		.class = class,
		.unit = -1
	};

	return bhnd_match_core(cores, num_cores, &md);
}

/**
 * Return true if the @p core matches @p desc.
 * 
 * @param core A bhnd core descriptor.
 * @param desc A match descriptor to compare against @p core.
 * 
 * @retval true if @p core matches @p match
 * @retval false if @p core does not match @p match.
 */
bool
bhnd_core_matches(const struct bhnd_core_info *core,
    const struct bhnd_core_match *desc)
{
	if (desc->vendor != BHND_MFGID_INVALID &&
	    desc->vendor != core->vendor)
		return (false);

	if (desc->device != BHND_COREID_INVALID &&
	    desc->device != core->device)
		return (false);

	if (desc->unit != -1 && desc->unit != core->unit)
		return (false);

	if (!bhnd_hwrev_matches(core->hwrev, &desc->hwrev))
		return (false);
		
	if (desc->hwrev.end != BHND_HWREV_INVALID &&
	    desc->hwrev.end < core->hwrev)
		return (false);

	if (desc->class != BHND_DEVCLASS_INVALID &&
	    desc->class != bhnd_core_class(core))
		return (false);

	return true;
}

/**
 * Return true if the @p hwrev matches @p desc.
 * 
 * @param hwrev A bhnd hardware revision.
 * @param desc A match descriptor to compare against @p core.
 * 
 * @retval true if @p hwrev matches @p match
 * @retval false if @p hwrev does not match @p match.
 */
bool
bhnd_hwrev_matches(uint16_t hwrev, const struct bhnd_hwrev_match *desc)
{
	if (desc->start != BHND_HWREV_INVALID &&
	    desc->start > hwrev)
		return false;
		
	if (desc->end != BHND_HWREV_INVALID &&
	    desc->end < hwrev)
		return false;

	return true;
}

/**
 * Return true if the @p dev matches @p desc.
 * 
 * @param dev A bhnd device.
 * @param desc A match descriptor to compare against @p dev.
 * 
 * @retval true if @p dev matches @p match
 * @retval false if @p dev does not match @p match.
 */
bool
bhnd_device_matches(device_t dev, const struct bhnd_core_match *desc)
{
	struct bhnd_core_info ci = {
		.vendor = bhnd_get_vendor(dev),
		.device = bhnd_get_device(dev),
		.unit = bhnd_get_core_unit(dev),
		.hwrev = bhnd_get_hwrev(dev)
	};

	return bhnd_core_matches(&ci, desc);
}

/**
 * Search @p table for an entry matching @p dev.
 * 
 * @param dev A bhnd device to match against @p table.
 * @param table The device table to search.
 * @param entry_size The @p table entry size, in bytes.
 * 
 * @retval bhnd_device the first matching device, if any.
 * @retval NULL if no matching device is found in @p table.
 */
const struct bhnd_device *
bhnd_device_lookup(device_t dev, const struct bhnd_device *table,
    size_t entry_size)
{
	const struct bhnd_device *entry;

	for (entry = table; entry->desc != NULL; entry =
	    (const struct bhnd_device *) ((const char *) entry + entry_size))
	{
		/* match core info */
		if (!bhnd_device_matches(dev, &entry->core))
			continue;

		/* match device flags */
		if (entry->device_flags & BHND_DF_HOSTB) {
			if (!bhnd_is_hostb_device(dev))
				continue;
		}

		/* device found */
		return (entry);
	}

	/* not found */
	return (NULL);
}

/**
 * Scan @p table for all quirk flags applicable to @p dev.
 * 
 * @param dev A bhnd device to match against @p table.
 * @param table The device table to search.
 * @param entry_size The @p table entry size, in bytes.
 * 
 * @return returns all matching quirk flags.
 */
uint32_t
bhnd_device_quirks(device_t dev, const struct bhnd_device *table,
    size_t entry_size)
{
	const struct bhnd_device	*dent;
	const struct bhnd_device_quirk	*qtable, *qent;
	uint32_t			 quirks;
	uint16_t			 hwrev;

	hwrev = bhnd_get_hwrev(dev);
	quirks = 0;

	/* Find the quirk table */
	if ((dent = bhnd_device_lookup(dev, table, entry_size)) == NULL) {
		/* This is almost certainly a (caller) implementation bug */
		device_printf(dev, "quirk lookup did not match any device\n");
		return (0);
	}

	/* Quirks aren't a mandatory field */
	if ((qtable = dent->quirks_table) == NULL)
		return (0);

	/* Collect matching quirk entries */
	for (qent = qtable; !BHND_DEVICE_QUIRK_IS_END(qent); qent++) {
		if (bhnd_hwrev_matches(hwrev, &qent->hwrev))
			quirks |= qent->quirks;
	}

	return (quirks);
}


/**
 * Allocate bhnd(4) resources defined in @p rs from a parent bus.
 * 
 * @param dev The device requesting ownership of the resources.
 * @param rs A standard bus resource specification. This will be updated
 * with the allocated resource's RIDs.
 * @param res On success, the allocated bhnd resources.
 * 
 * @retval 0 success
 * @retval non-zero if allocation of any non-RF_OPTIONAL resource fails,
 * 		    all allocated resources will be released and a regular
 * 		    unix error code will be returned.
 */
int
bhnd_alloc_resources(device_t dev, struct resource_spec *rs,
    struct bhnd_resource **res)
{
	/* Initialize output array */
	for (u_int i = 0; rs[i].type != -1; i++)
		res[i] = NULL;

	for (u_int i = 0; rs[i].type != -1; i++) {
		res[i] = bhnd_alloc_resource_any(dev, rs[i].type, &rs[i].rid,
		    rs[i].flags);

		/* Clean up all allocations on failure */
		if (res[i] == NULL && !(rs[i].flags & RF_OPTIONAL)) {
			bhnd_release_resources(dev, rs, res);
			return (ENXIO);
		}
	}

	return (0);
};

/**
 * Release bhnd(4) resources defined in @p rs from a parent bus.
 * 
 * @param dev The device that owns the resources.
 * @param rs A standard bus resource specification previously initialized
 * by @p bhnd_alloc_resources.
 * @param res The bhnd resources to be released.
 */
void
bhnd_release_resources(device_t dev, const struct resource_spec *rs,
    struct bhnd_resource **res)
{
	for (u_int i = 0; rs[i].type != -1; i++) {
		if (res[i] == NULL)
			continue;

		bhnd_release_resource(dev, rs[i].type, rs[i].rid, res[i]);
		res[i] = NULL;
	}
}

/**
 * Parse the CHIPC_ID_* fields from the ChipCommon CHIPC_ID
 * register, returning its bhnd_chipid representation.
 * 
 * @param idreg The CHIPC_ID register value.
 * @param enum_addr The enumeration address to include in the result.
 *
 * @warning
 * On early siba(4) devices, the ChipCommon core does not provide
 * a valid CHIPC_ID_NUMCORE field. On these ChipCommon revisions
 * (see CHIPC_NCORES_MIN_HWREV()), this function will parse and return
 * an invalid `ncores` value.
 */
struct bhnd_chipid
bhnd_parse_chipid(uint32_t idreg, bhnd_addr_t enum_addr)
{
	struct bhnd_chipid result;

	/* Fetch the basic chip info */
	result.chip_id = CHIPC_GET_ATTR(idreg, ID_CHIP);
	result.chip_pkg = CHIPC_GET_ATTR(idreg, ID_PKG);
	result.chip_rev = CHIPC_GET_ATTR(idreg, ID_REV);
	result.chip_type = CHIPC_GET_ATTR(idreg, ID_BUS);
	result.ncores = CHIPC_GET_ATTR(idreg, ID_NUMCORE);

	result.enum_addr = enum_addr;

	return (result);
}

/**
 * Allocate the resource defined by @p rs via @p dev, use it
 * to read the ChipCommon ID register relative to @p chipc_offset,
 * then release the resource.
 * 
 * @param dev The device owning @p rs.
 * @param rs A resource spec that encompasses the ChipCommon register block.
 * @param chipc_offset The offset of the ChipCommon registers within @p rs.
 * @param[out] result the chip identification data.
 * 
 * @retval 0 success
 * @retval non-zero if the ChipCommon identification data could not be read.
 */
int
bhnd_read_chipid(device_t dev, struct resource_spec *rs,
    bus_size_t chipc_offset, struct bhnd_chipid *result)
{
	struct resource			*res;
	uint32_t			 reg;
	int				 error, rid, rtype;

	/* Allocate the ChipCommon window resource and fetch the chipid data */
	rid = rs->rid;
	rtype = rs->type;
	res = bus_alloc_resource_any(dev, rtype, &rid, RF_ACTIVE);
	if (res == NULL) {
		device_printf(dev,
		    "failed to allocate bhnd chipc resource\n");
		return (ENXIO);
	}

	/* Fetch the basic chip info */
	reg = bus_read_4(res, chipc_offset + CHIPC_ID);
	*result = bhnd_parse_chipid(reg, 0x0);

	/* Fetch the enum base address */
	error = 0;
	switch (result->chip_type) {
	case BHND_CHIPTYPE_SIBA:
		result->enum_addr = BHND_DEFAULT_CHIPC_ADDR;
		break;
	case BHND_CHIPTYPE_BCMA:
	case BHND_CHIPTYPE_BCMA_ALT:
		result->enum_addr = bus_read_4(res, chipc_offset +
		    CHIPC_EROMPTR);
		break;
	case BHND_CHIPTYPE_UBUS:
		device_printf(dev, "unsupported ubus/bcm63xx chip type");
		error = ENODEV;
		goto cleanup;
	default:
		device_printf(dev, "unknown chip type %hhu\n",
		    result->chip_type);
		error = ENODEV;
		goto cleanup;
	}

cleanup:
	/* Clean up */
	bus_release_resource(dev, rtype, rid, res);
	return (error);
}

/**
 * Using the bhnd(4) bus-level core information and a custom core name,
 * populate @p dev's device description.
 * 
 * @param dev A bhnd-bus attached device.
 * @param dev_name The core's name (e.g. "SDIO Device Core")
 */
void
bhnd_set_custom_core_desc(device_t dev, const char *dev_name)
{
	const char *vendor_name;
	char *desc;

	vendor_name = bhnd_get_vendor_name(dev);
	asprintf(&desc, M_BHND, "%s %s, rev %hhu", vendor_name, dev_name,
	    bhnd_get_hwrev(dev));

	if (desc != NULL) {
		device_set_desc_copy(dev, desc);
		free(desc, M_BHND);
	} else {
		device_set_desc(dev, dev_name);
	}
}

/**
 * Using the bhnd(4) bus-level core information, populate @p dev's device
 * description.
 * 
 * @param dev A bhnd-bus attached device.
 */
void
bhnd_set_default_core_desc(device_t dev)
{
	bhnd_set_custom_core_desc(dev, bhnd_get_device_name(dev));
}

/**
 * Helper function for implementing BHND_BUS_IS_HOSTB_DEVICE().
 * 
 * If a parent device is available, this implementation delegates the
 * request to the BHND_BUS_IS_HOSTB_DEVICE() method on the parent of @p dev.
 * 
 * If no parent device is available (i.e. on a the bus root), false
 * is returned.
 */
bool
bhnd_bus_generic_is_hostb_device(device_t dev, device_t child) {
	if (device_get_parent(dev) != NULL)
		return (BHND_BUS_IS_HOSTB_DEVICE(device_get_parent(dev),
		    child));

	return (false);
}

/**
 * Helper function for implementing BHND_BUS_IS_HW_DISABLED().
 * 
 * If a parent device is available, this implementation delegates the
 * request to the BHND_BUS_IS_HW_DISABLED() method on the parent of @p dev.
 * 
 * If no parent device is available (i.e. on a the bus root), the hardware
 * is assumed to be usable and false is returned.
 */
bool
bhnd_bus_generic_is_hw_disabled(device_t dev, device_t child)
{
	if (device_get_parent(dev) != NULL)
		return (BHND_BUS_IS_HW_DISABLED(device_get_parent(dev), child));

	return (false);
}

/**
 * Helper function for implementing BHND_BUS_GET_CHIPID().
 * 
 * This implementation delegates the request to the BHND_BUS_GET_CHIPID()
 * method on the parent of @p dev. If no parent exists, the implementation
 * will panic.
 */
const struct bhnd_chipid *
bhnd_bus_generic_get_chipid(device_t dev, device_t child)
{
	if (device_get_parent(dev) != NULL)
		return (BHND_BUS_GET_CHIPID(device_get_parent(dev), child));

	panic("missing BHND_BUS_GET_CHIPID()");
}

/**
 * Helper function for implementing BHND_BUS_ALLOC_RESOURCE().
 * 
 * This implementation of BHND_BUS_ALLOC_RESOURCE() delegates allocation
 * of the underlying resource to BUS_ALLOC_RESOURCE(), and activation
 * to @p dev's BHND_BUS_ACTIVATE_RESOURCE().
 */
struct bhnd_resource *
bhnd_bus_generic_alloc_resource(device_t dev, device_t child, int type,
	int *rid, rman_res_t start, rman_res_t end, rman_res_t count,
	u_int flags)
{
	struct bhnd_resource	*br;
	struct resource		*res;
	int			 error;

	br = NULL;
	res = NULL;

	/* Allocate the real bus resource (without activating it) */
	res = BUS_ALLOC_RESOURCE(dev, child, type, rid, start, end, count,
	    (flags & ~RF_ACTIVE));
	if (res == NULL)
		return (NULL);

	/* Allocate our bhnd resource wrapper. */
	br = malloc(sizeof(struct bhnd_resource), M_BHND, M_NOWAIT);
	if (br == NULL)
		goto failed;
	
	br->direct = false;
	br->res = res;

	/* Attempt activation */
	if (flags & RF_ACTIVE) {
		error = BHND_BUS_ACTIVATE_RESOURCE(dev, child, type, *rid, br);
		if (error)
			goto failed;
	}

	return (br);
	
failed:
	if (res != NULL)
		BUS_RELEASE_RESOURCE(dev, child, type, *rid, res);

	free(br, M_BHND);
	return (NULL);
}

/**
 * Helper function for implementing BHND_BUS_RELEASE_RESOURCE().
 * 
 * This implementation of BHND_BUS_RELEASE_RESOURCE() delegates release of
 * the backing resource to BUS_RELEASE_RESOURCE().
 */
int
bhnd_bus_generic_release_resource(device_t dev, device_t child, int type,
    int rid, struct bhnd_resource *r)
{
	int error;

	if ((error = BUS_RELEASE_RESOURCE(dev, child, type, rid, r->res)))
		return (error);

	free(r, M_BHND);
	return (0);
}


/**
 * Helper function for implementing BHND_BUS_ACTIVATE_RESOURCE().
 * 
 * This implementation of BHND_BUS_ACTIVATE_RESOURCE() simply calls the
 * BHND_BUS_ACTIVATE_RESOURCE() method of the parent of @p dev.
 */
int
bhnd_bus_generic_activate_resource(device_t dev, device_t child, int type,
    int rid, struct bhnd_resource *r)
{
	/* Try to delegate to the parent */
	if (device_get_parent(dev) != NULL)
		return (BHND_BUS_ACTIVATE_RESOURCE(device_get_parent(dev),
		    child, type, rid, r));

	return (EINVAL);
};

/**
 * Helper function for implementing BHND_BUS_DEACTIVATE_RESOURCE().
 * 
 * This implementation of BHND_BUS_ACTIVATE_RESOURCE() simply calls the
 * BHND_BUS_ACTIVATE_RESOURCE() method of the parent of @p dev.
 */
int
bhnd_bus_generic_deactivate_resource(device_t dev, device_t child,
    int type, int rid, struct bhnd_resource *r)
{
	if (device_get_parent(dev) != NULL)
		return (BHND_BUS_DEACTIVATE_RESOURCE(device_get_parent(dev),
		    child, type, rid, r));

	return (EINVAL);
};