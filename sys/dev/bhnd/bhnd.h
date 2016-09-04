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
 * 
 * $FreeBSD$
 */

#ifndef _BHND_BHND_H_
#define _BHND_BHND_H_

#include <sys/types.h>
#include <sys/bus.h>

#include <machine/bus.h>

#include "bhnd_ids.h"
#include "bhnd_types.h"
#include "bhnd_erom_types.h"
#include "bhnd_debug.h"
#include "bhnd_bus_if.h"
#include "bhnd_match.h"

#include "nvram/bhnd_nvram.h"

extern devclass_t bhnd_devclass;
extern devclass_t bhnd_hostb_devclass;
extern devclass_t bhnd_nvram_devclass;

#define	BHND_CHIPID_MAX_NAMELEN	32	/**< maximum buffer required for a
					     bhnd_format_chip_id() */

/**
 * bhnd child instance variables
 */
enum bhnd_device_vars {
	BHND_IVAR_VENDOR,	/**< Designer's JEP-106 manufacturer ID. */
	BHND_IVAR_DEVICE,	/**< Part number */
	BHND_IVAR_HWREV,	/**< Core revision */
	BHND_IVAR_DEVICE_CLASS,	/**< Core class (@sa bhnd_devclass_t) */
	BHND_IVAR_VENDOR_NAME,	/**< Core vendor name */
	BHND_IVAR_DEVICE_NAME,	/**< Core name */
	BHND_IVAR_CORE_INDEX,	/**< Bus-assigned core number */
	BHND_IVAR_CORE_UNIT,	/**< Bus-assigned core unit number,
				     assigned sequentially (starting at 0) for
				     each vendor/device pair. */
};

/**
 * bhnd device probe priority bands.
 */
enum {
	BHND_PROBE_ROOT         = 0,    /**< Nexus or host bridge */
	BHND_PROBE_BUS		= 1000,	/**< Busses and bridges */
	BHND_PROBE_CPU		= 2000,	/**< CPU devices */
	BHND_PROBE_INTERRUPT	= 3000,	/**< Interrupt controllers. */
	BHND_PROBE_TIMER	= 4000,	/**< Timers and clocks. */
	BHND_PROBE_RESOURCE	= 5000,	/**< Resource discovery (including NVRAM/SPROM) */
	BHND_PROBE_DEFAULT	= 6000,	/**< Default device priority */
};

/**
 * Constants defining fine grained ordering within a BHND_PROBE_* priority band.
 * 
 * Example:
 * @code
 * BHND_PROBE_BUS + BHND_PROBE_ORDER_FIRST
 * @endcode
 */
enum {
	BHND_PROBE_ORDER_FIRST		= 0,
	BHND_PROBE_ORDER_EARLY		= 25,
	BHND_PROBE_ORDER_MIDDLE		= 50,
	BHND_PROBE_ORDER_LATE		= 75,
	BHND_PROBE_ORDER_LAST		= 100

};

/*
 * Simplified accessors for bhnd device ivars
 */
#define	BHND_ACCESSOR(var, ivar, type) \
	__BUS_ACCESSOR(bhnd, var, BHND, ivar, type)

BHND_ACCESSOR(vendor,		VENDOR,		uint16_t);
BHND_ACCESSOR(device,		DEVICE,		uint16_t);
BHND_ACCESSOR(hwrev,		HWREV,		uint8_t);
BHND_ACCESSOR(class,		DEVICE_CLASS,	bhnd_devclass_t);
BHND_ACCESSOR(vendor_name,	VENDOR_NAME,	const char *);
BHND_ACCESSOR(device_name,	DEVICE_NAME,	const char *);
BHND_ACCESSOR(core_index,	CORE_INDEX,	u_int);
BHND_ACCESSOR(core_unit,	CORE_UNIT,	int);

#undef	BHND_ACCESSOR

/**
 * A bhnd(4) board descriptor.
 */
struct bhnd_board_info {
	uint16_t	board_vendor;	/**< PCI-SIG vendor ID (even on non-PCI
					  *  devices).
					  *
					  *  On PCI devices, this will generally
					  *  be the subsystem vendor ID, but the
					  *  value may be overridden in device
					  *  NVRAM.
					  */
	uint16_t	board_type;	/**< Board type (See BHND_BOARD_*)
					  *
					  *  On PCI devices, this will generally
					  *  be the subsystem device ID, but the
					  *  value may be overridden in device
					  *  NVRAM.
					  */
	uint16_t	board_rev;	/**< Board revision. */
	uint8_t		board_srom_rev;	/**< Board SROM format revision */

	uint32_t	board_flags;	/**< Board flags (see BHND_BFL_*) */
	uint32_t	board_flags2;	/**< Board flags 2 (see BHND_BFL2_*) */
	uint32_t	board_flags3;	/**< Board flags 3 (see BHND_BFL3_*) */
};


/**
 * Chip Identification
 * 
 * This is read from the ChipCommon ID register; on earlier bhnd(4) devices
 * where ChipCommon is unavailable, known values must be supplied.
 */
struct bhnd_chipid {
	uint16_t	chip_id;	/**< chip id (BHND_CHIPID_*) */
	uint8_t		chip_rev;	/**< chip revision */
	uint8_t		chip_pkg;	/**< chip package (BHND_PKGID_*) */
	uint8_t		chip_type;	/**< chip type (BHND_CHIPTYPE_*) */

	bhnd_addr_t	enum_addr;	/**< chip_type-specific enumeration
					  *  address; either the siba(4) base
					  *  core register block, or the bcma(4)
					  *  EROM core address. */

	uint8_t		ncores;		/**< number of cores, if known. 0 if
					  *  not available. */
};

/**
 * A bhnd(4) core descriptor.
 */
struct bhnd_core_info {
	uint16_t	vendor;		/**< JEP-106 vendor (BHND_MFGID_*) */
	uint16_t	device;		/**< device */
	uint16_t	hwrev;		/**< hardware revision */
	u_int		core_idx;	/**< bus-assigned core index */
	int		unit;		/**< bus-assigned core unit */
};

/**
* A bhnd(4) bus resource.
* 
* This provides an abstract interface to per-core resources that may require
* bus-level remapping of address windows prior to access.
*/
struct bhnd_resource {
	struct resource	*res;		/**< the system resource. */
	bool		 direct;	/**< false if the resource requires
					 *   bus window remapping before it
					 *   is MMIO accessible. */
};

/** Wrap the active resource @p _r in a bhnd_resource structure */
#define	BHND_DIRECT_RESOURCE(_r)	((struct bhnd_resource) {	\
	.res = (_r),							\
	.direct = true,							\
})

/**
 * Device quirk table descriptor.
 */
struct bhnd_device_quirk {
	struct bhnd_device_match desc;		/**< device match descriptor */
	uint32_t		 quirks;	/**< quirk flags */
};

#define	BHND_CORE_QUIRK(_rev, _flags)		\
	{{ BHND_MATCH_CORE_REV(_rev) }, (_flags) }

#define	BHND_CHIP_QUIRK(_chip, _rev, _flags)	\
	{{ BHND_CHIP_IR(BCM ## _chip, _rev) }, (_flags) }

#define	BHND_PKG_QUIRK(_chip, _pkg, _flags)	\
	{{ BHND_CHIP_IP(BCM ## _chip, BCM ## _chip ## _pkg) }, (_flags) }

#define	BHND_BOARD_QUIRK(_board, _flags)	\
	{{ BHND_MATCH_BOARD_TYPE(_board) },	\
	    (_flags) }

#define	BHND_DEVICE_QUIRK_END		{ { BHND_MATCH_ANY }, 0 }
#define	BHND_DEVICE_QUIRK_IS_END(_q)	\
	(((_q)->desc.m.match_flags == 0) && (_q)->quirks == 0)

enum {
	BHND_DF_ANY	= 0,
	BHND_DF_HOSTB	= (1<<0),	/**< core is serving as the bus' host
					  *  bridge. implies BHND_DF_ADAPTER */
	BHND_DF_SOC	= (1<<1),	/**< core is attached to a native
					     bus (BHND_ATTACH_NATIVE) */
	BHND_DF_ADAPTER	= (1<<2),	/**< core is attached to a bridged
					  *  adapter (BHND_ATTACH_ADAPTER) */
};

/** Device probe table descriptor */
struct bhnd_device {
	const struct bhnd_device_match	 core;		/**< core match descriptor */ 
	const char			*desc;		/**< device description, or NULL. */
	const struct bhnd_device_quirk	*quirks_table;	/**< quirks table for this device, or NULL */
	uint32_t			 device_flags;	/**< required BHND_DF_* flags */
};

#define	_BHND_DEVICE(_vendor, _device, _desc, _quirks,		\
     _flags, ...)						\
	{ { BHND_MATCH_CORE(BHND_MFGID_ ## _vendor,		\
	    BHND_COREID_ ## _device) }, _desc, _quirks,		\
	    _flags }

#define	BHND_DEVICE(_vendor, _device, _desc, _quirks, ...)	\
	_BHND_DEVICE(_vendor, _device, _desc, _quirks,		\
	    ## __VA_ARGS__, 0)

#define	BHND_DEVICE_END		{ { BHND_MATCH_ANY }, NULL, NULL, 0 }
#define	BHND_DEVICE_IS_END(_d)	\
	(BHND_MATCH_IS_ANY(&(_d)->core) && (_d)->desc == NULL)

const char			*bhnd_vendor_name(uint16_t vendor);
const char			*bhnd_port_type_name(bhnd_port_type port_type);
const char			*bhnd_nvram_src_name(bhnd_nvram_src nvram_src);

const char 			*bhnd_find_core_name(uint16_t vendor,
				     uint16_t device);
bhnd_devclass_t			 bhnd_find_core_class(uint16_t vendor,
				     uint16_t device);

const char			*bhnd_core_name(const struct bhnd_core_info *ci);
bhnd_devclass_t			 bhnd_core_class(const struct bhnd_core_info *ci);

int				 bhnd_format_chip_id(char *buffer, size_t size,
				     uint16_t chip_id);

device_t			 bhnd_match_child(device_t dev,
				     const struct bhnd_core_match *desc);

device_t			 bhnd_find_child(device_t dev,
				     bhnd_devclass_t class, int unit);

device_t			 bhnd_find_bridge_root(device_t dev,
				     devclass_t bus_class);

const struct bhnd_core_info	*bhnd_match_core(
				     const struct bhnd_core_info *cores,
				     u_int num_cores,
				     const struct bhnd_core_match *desc);

const struct bhnd_core_info	*bhnd_find_core(
				     const struct bhnd_core_info *cores,
				     u_int num_cores, bhnd_devclass_t class);

struct bhnd_core_match		 bhnd_core_get_match_desc(
				     const struct bhnd_core_info *core);

bool				 bhnd_cores_equal(
				     const struct bhnd_core_info *lhs,
				     const struct bhnd_core_info *rhs);

bool				 bhnd_core_matches(
				     const struct bhnd_core_info *core,
				     const struct bhnd_core_match *desc);

bool				 bhnd_chip_matches(
				     const struct bhnd_chipid *chipid,
				     const struct bhnd_chip_match *desc);

bool				 bhnd_board_matches(
				     const struct bhnd_board_info *info,
				     const struct bhnd_board_match *desc);

bool				 bhnd_hwrev_matches(uint16_t hwrev,
				     const struct bhnd_hwrev_match *desc);

bool				 bhnd_device_matches(device_t dev,
				     const struct bhnd_device_match *desc);

const struct bhnd_device	*bhnd_device_lookup(device_t dev,
				     const struct bhnd_device *table,
				     size_t entry_size);

uint32_t			 bhnd_device_quirks(device_t dev,
				     const struct bhnd_device *table,
				     size_t entry_size);

struct bhnd_core_info		 bhnd_get_core_info(device_t dev);

int				 bhnd_alloc_resources(device_t dev,
				     struct resource_spec *rs,
				     struct bhnd_resource **res);

void				 bhnd_release_resources(device_t dev,
				     const struct resource_spec *rs,
				     struct bhnd_resource **res);

struct bhnd_chipid		 bhnd_parse_chipid(uint32_t idreg,
				     bhnd_addr_t enum_addr);

int				 bhnd_chipid_fixed_ncores(
				     const struct bhnd_chipid *cid,
				     uint16_t chipc_hwrev, uint8_t *ncores);

int				 bhnd_read_chipid(device_t dev,
				     struct resource_spec *rs,
				     bus_size_t chipc_offset,
				     struct bhnd_chipid *result);

void				 bhnd_set_custom_core_desc(device_t dev,
				     const char *name);
void				 bhnd_set_default_core_desc(device_t dev);

void				 bhnd_set_default_bus_desc(device_t dev,
				     const struct bhnd_chipid *chip_id);

int				 bhnd_nvram_getvar_str(device_t dev,
				     const char *name, char *buf, size_t len,
				     size_t *rlen);

int				 bhnd_nvram_getvar_uint(device_t dev,
				     const char *name, void *value, int width);
int				 bhnd_nvram_getvar_uint8(device_t dev,
				     const char *name, uint8_t *value);
int				 bhnd_nvram_getvar_uint16(device_t dev,
				     const char *name, uint16_t *value);
int				 bhnd_nvram_getvar_uint32(device_t dev,
				     const char *name, uint32_t *value);

int				 bhnd_nvram_getvar_int(device_t dev,
				     const char *name, void *value, int width);
int				 bhnd_nvram_getvar_int8(device_t dev,
				     const char *name, int8_t *value);
int				 bhnd_nvram_getvar_int16(device_t dev,
				     const char *name, int16_t *value);
int				 bhnd_nvram_getvar_int32(device_t dev,
				     const char *name, int32_t *value);

int				 bhnd_nvram_getvar_array(device_t dev,
				     const char *name, void *buf, size_t count,
				     bhnd_nvram_type type);

bool				 bhnd_bus_generic_is_hw_disabled(device_t dev,
				     device_t child);
bool				 bhnd_bus_generic_is_region_valid(device_t dev,
				     device_t child, bhnd_port_type type,
				     u_int port, u_int region);
int				 bhnd_bus_generic_get_nvram_var(device_t dev,
				     device_t child, const char *name,
				     void *buf, size_t *size,
				     bhnd_nvram_type type);
const struct bhnd_chipid	*bhnd_bus_generic_get_chipid(device_t dev,
				     device_t child);
int				 bhnd_bus_generic_read_board_info(device_t dev,
				     device_t child,
				     struct bhnd_board_info *info);
struct bhnd_resource		*bhnd_bus_generic_alloc_resource (device_t dev,
				     device_t child, int type, int *rid,
				     rman_res_t start, rman_res_t end,
				     rman_res_t count, u_int flags);
int				 bhnd_bus_generic_release_resource (device_t dev,
				     device_t child, int type, int rid,
				     struct bhnd_resource *r);
int				 bhnd_bus_generic_activate_resource (device_t dev,
				     device_t child, int type, int rid,
				     struct bhnd_resource *r);
int				 bhnd_bus_generic_deactivate_resource (device_t dev,
				     device_t child, int type, int rid,
				     struct bhnd_resource *r);
bhnd_attach_type		 bhnd_bus_generic_get_attach_type(device_t dev,
				     device_t child);

/**
 * Return the bhnd(4) bus driver's device enumeration parser class
 *
 * @param driver A bhnd bus driver instance.
 */
static inline bhnd_erom_class_t *
bhnd_driver_get_erom_class(driver_t *driver)
{
	return (BHND_BUS_GET_EROM_CLASS(driver));
}

/**
 * Return the active host bridge core for the bhnd bus, if any, or NULL if
 * not found.
 *
 * @param dev A bhnd bus device.
 */
static inline device_t
bhnd_find_hostb_device(device_t dev) {
	return (BHND_BUS_FIND_HOSTB_DEVICE(dev));
}

/**
 * Return true if the hardware components required by @p dev are known to be
 * unpopulated or otherwise unusable.
 *
 * In some cases, enumerated devices may have pins that are left floating, or
 * the hardware may otherwise be non-functional; this method allows a parent
 * device to explicitly specify if a successfully enumerated @p dev should
 * be disabled.
 *
 * @param dev A bhnd bus child device.
 */
static inline bool
bhnd_is_hw_disabled(device_t dev) {
	return (BHND_BUS_IS_HW_DISABLED(device_get_parent(dev), dev));
}

/**
 * Return the BHND chip identification info for the bhnd bus.
 *
 * @param dev A bhnd bus child device.
 */
static inline const struct bhnd_chipid *
bhnd_get_chipid(device_t dev) {
	return (BHND_BUS_GET_CHIPID(device_get_parent(dev), dev));
};

/**
 * If supported by the chipset, return the clock source for the given clock.
 *
 * This function is only supported on early PWRCTL-equipped chipsets
 * that expose clock management via their host bridge interface. Currently,
 * this includes PCI (not PCIe) devices, with ChipCommon core revisions 0-9.
 *
 * @param dev A bhnd bus child device.
 * @param clock The clock for which a clock source will be returned.
 *
 * @retval	bhnd_clksrc		The clock source for @p clock.
 * @retval	BHND_CLKSRC_UNKNOWN	If @p clock is unsupported, or its
 *					clock source is not known to the bus.
 */
static inline bhnd_clksrc
bhnd_pwrctl_get_clksrc(device_t dev, bhnd_clock clock)
{
	return (BHND_BUS_PWRCTL_GET_CLKSRC(device_get_parent(dev), dev, clock));
}

/**
 * If supported by the chipset, gate @p clock
 *
 * This function is only supported on early PWRCTL-equipped chipsets
 * that expose clock management via their host bridge interface. Currently,
 * this includes PCI (not PCIe) devices, with ChipCommon core revisions 0-9.
 *
 * @param dev A bhnd bus child device.
 * @param clock The clock to be disabled.
 *
 * @retval 0 success
 * @retval ENODEV If bus-level clock source management is not supported.
 * @retval ENXIO If bus-level management of @p clock is not supported.
 */
static inline int
bhnd_pwrctl_gate_clock(device_t dev, bhnd_clock clock)
{
	return (BHND_BUS_PWRCTL_GATE_CLOCK(device_get_parent(dev), dev, clock));
}

/**
 * If supported by the chipset, ungate @p clock
 *
 * This function is only supported on early PWRCTL-equipped chipsets
 * that expose clock management via their host bridge interface. Currently,
 * this includes PCI (not PCIe) devices, with ChipCommon core revisions 0-9.
 *
 * @param dev A bhnd bus child device.
 * @param clock The clock to be enabled.
 *
 * @retval 0 success
 * @retval ENODEV If bus-level clock source management is not supported.
 * @retval ENXIO If bus-level management of @p clock is not supported.
 */
static inline int
bhnd_pwrctl_ungate_clock(device_t dev, bhnd_clock clock)
{
	return (BHND_BUS_PWRCTL_UNGATE_CLOCK(device_get_parent(dev), dev,
	    clock));
}

/**
 * Return the BHND attachment type of the parent bhnd bus.
 *
 * @param dev A bhnd bus child device.
 *
 * @retval BHND_ATTACH_ADAPTER if the bus is resident on a bridged adapter,
 * such as a WiFi chipset.
 * @retval BHND_ATTACH_NATIVE if the bus provides hardware services (clock,
 * CPU, etc) to a directly attached native host.
 */
static inline bhnd_attach_type
bhnd_get_attach_type (device_t dev) {
	return (BHND_BUS_GET_ATTACH_TYPE(device_get_parent(dev), dev));
}

/**
 * Attempt to read the BHND board identification from the bhnd bus.
 *
 * This relies on NVRAM access, and will fail if a valid NVRAM device cannot
 * be found, or is not yet attached.
 *
 * @param dev The parent of @p child.
 * @param child The bhnd device requesting board info.
 * @param[out] info On success, will be populated with the bhnd(4) device's
 * board information.
 *
 * @retval 0 success
 * @retval ENODEV	No valid NVRAM source could be found.
 * @retval non-zero	If reading @p name otherwise fails, a regular unix
 *			error code will be returned.
 */
static inline int
bhnd_read_board_info(device_t dev, struct bhnd_board_info *info)
{
	return (BHND_BUS_READ_BOARD_INFO(device_get_parent(dev), dev, info));
}

/**
 * Allocate and enable per-core PMU request handling for @p child.
 *
 * The region containing the core's PMU register block (if any) must be
 * allocated via bus_alloc_resource(9) (or bhnd_alloc_resource) before
 * calling bhnd_alloc_pmu(), and must not be released until after
 * calling bhnd_release_pmu().
 *
 * @param dev The parent of @p child.
 * @param child The requesting bhnd device.
 * 
 * @retval 0           success
 * @retval non-zero    If allocating PMU request state otherwise fails, a
 *                     regular unix error code will be returned.
 */
static inline int
bhnd_alloc_pmu(device_t dev)
{
	return (BHND_BUS_ALLOC_PMU(device_get_parent(dev), dev));
}

/**
 * Release any per-core PMU resources allocated for @p child. Any outstanding
 * PMU requests are are discarded.
 *
 * @param dev The parent of @p child.
 * @param child The requesting bhnd device.
 * 
 * @retval 0           success
 * @retval non-zero    If releasing PMU request state otherwise fails, a
 *                     regular unix error code will be returned, and
 *                     the core state will be left unmodified.
 */
static inline int
bhnd_release_pmu(device_t dev)
{
	return (BHND_BUS_RELEASE_PMU(device_get_parent(dev), dev));
}

/** 
 * Request that @p clock (or faster) be routed to @p dev.
 * 
 * A driver must ask the bhnd bus to allocate clock request state
 * via bhnd_alloc_pmu() before it can request clock resources.
 * 
 * Request multiplexing is managed by the bus.
 *
 * @param dev The bhnd(4) device to which @p clock should be routed.
 * @param clock The requested clock source. 
 *
 * @retval 0 success
 * @retval ENODEV If an unsupported clock was requested.
 * @retval ENXIO If the PMU has not been initialized or is otherwise unvailable.
 */
static inline int
bhnd_request_clock(device_t dev, bhnd_clock clock)
{
	return (BHND_BUS_REQUEST_CLOCK(device_get_parent(dev), dev, clock));
}

/**
 * Request that @p clocks be powered on behalf of @p dev.
 *
 * This will power any clock sources (e.g. XTAL, PLL, etc) required for
 * @p clocks and wait until they are ready, discarding any previous
 * requests by @p dev.
 *
 * Request multiplexing is managed by the bus.
 * 
 * A driver must ask the bhnd bus to allocate clock request state
 * via bhnd_alloc_pmu() before it can request clock resources.
 *
 * @param dev The requesting bhnd(4) device.
 * @param clocks The clock(s) to be enabled.
 *
 * @retval 0 success
 * @retval ENODEV If an unsupported clock was requested.
 * @retval ENXIO If the PMU has not been initialized or is otherwise unvailable.
 */
static inline int
bhnd_enable_clocks(device_t dev, uint32_t clocks)
{
	return (BHND_BUS_ENABLE_CLOCKS(device_get_parent(dev), dev, clocks));
}

/**
 * Power up an external PMU-managed resource assigned to @p dev.
 * 
 * A driver must ask the bhnd bus to allocate PMU request state
 * via bhnd_alloc_pmu() before it can request PMU resources.
 *
 * @param dev The requesting bhnd(4) device.
 * @param rsrc The core-specific external resource identifier.
 *
 * @retval 0 success
 * @retval ENODEV If the PMU does not support @p rsrc.
 * @retval ENXIO If the PMU has not been initialized or is otherwise unvailable.
 */
static inline int
bhnd_request_ext_rsrc(device_t dev, u_int rsrc)
{
	return (BHND_BUS_REQUEST_EXT_RSRC(device_get_parent(dev), dev, rsrc));
}

/**
 * Power down an external PMU-managed resource assigned to @p dev.
 * 
 * A driver must ask the bhnd bus to allocate PMU request state
 * via bhnd_alloc_pmu() before it can request PMU resources.
 *
 * @param dev The requesting bhnd(4) device.
 * @param rsrc The core-specific external resource identifier.
 *
 * @retval 0 success
 * @retval ENODEV If the PMU does not support @p rsrc.
 * @retval ENXIO If the PMU has not been initialized or is otherwise unvailable.
 */
static inline int
bhnd_release_ext_rsrc(device_t dev, u_int rsrc)
{
	return (BHND_BUS_RELEASE_EXT_RSRC(device_get_parent(dev), dev, rsrc));
}


/**
 * Read @p width bytes at @p offset from the bus-specific agent/config
 * space of @p dev.
 *
 * @param dev The bhnd device for which @p offset should be read.
 * @param offset The offset to be read.
 * @param width The size of the access. Must be 1, 2 or 4 bytes.
 *
 * The exact behavior of this method is bus-specific. In the case of
 * bcma(4), this method provides access to the first agent port of @p child.
 *
 * @note Device drivers should only use this API for functionality
 * that is not available via another bhnd(4) function.
 */
static inline uint32_t
bhnd_read_config(device_t dev, bus_size_t offset, u_int width)
{
	return (BHND_BUS_READ_CONFIG(device_get_parent(dev), dev, offset,
	    width));
}

/**
 * Read @p width bytes at @p offset from the bus-specific agent/config
 * space of @p dev.
 *
 * @param dev The bhnd device for which @p offset should be read.
 * @param offset The offset to be written.
 * @param width The size of the access. Must be 1, 2 or 4 bytes.
 *
 * The exact behavior of this method is bus-specific. In the case of
 * bcma(4), this method provides access to the first agent port of @p child.
 *
 * @note Device drivers should only use this API for functionality
 * that is not available via another bhnd(4) function.
 */
static inline void
bhnd_write_config(device_t dev, bus_size_t offset, uint32_t val, u_int width)
{
	BHND_BUS_WRITE_CONFIG(device_get_parent(dev), dev, offset, val, width);
}

/**
 * Read an NVRAM variable, coerced to the requested @p type.
 *
 * @param 		dev	A bhnd bus child device.
 * @param		name	The NVRAM variable name.
 * @param[out]		buf	A buffer large enough to hold @p len bytes. On
 *				success, the requested value will be written to
 *				this buffer. This argment may be NULL if
 *				the value is not desired.
 * @param[in,out]	len	The maximum capacity of @p buf. On success,
 *				will be set to the actual size of the requested
 *				value.
 * @param		type	The desired data representation to be written
 *				to @p buf.
 * 
 * @retval 0		success
 * @retval ENOENT	The requested variable was not found.
 * @retval ENODEV	No valid NVRAM source could be found.
 * @retval ENOMEM	If a buffer of @p size is too small to hold the
 *			requested value.
 * @retval EOPNOTSUPP	If the value cannot be coerced to @p type.
 * @retval ERANGE	If value coercion would overflow @p type.
 * @retval non-zero	If reading @p name otherwise fails, a regular unix
 *			error code will be returned.
 */
static inline int
bhnd_nvram_getvar(device_t dev, const char *name, void *buf, size_t *len,
     bhnd_nvram_type type)
{
	return (BHND_BUS_GET_NVRAM_VAR(device_get_parent(dev), dev, name, buf,
	    len, type));
}

/**
 * Allocate a resource from a device's parent bhnd(4) bus.
 * 
 * @param dev The device requesting resource ownership.
 * @param type The type of resource to allocate. This may be any type supported
 * by the standard bus APIs.
 * @param rid The bus-specific handle identifying the resource being allocated.
 * @param start The start address of the resource.
 * @param end The end address of the resource.
 * @param count The size of the resource.
 * @param flags The flags for the resource to be allocated. These may be any
 * values supported by the standard bus APIs.
 * 
 * To request the resource's default addresses, pass @p start and
 * @p end values of @c 0 and @c ~0, respectively, and
 * a @p count of @c 1.
 * 
 * @retval NULL The resource could not be allocated.
 * @retval resource The allocated resource.
 */
static inline struct bhnd_resource *
bhnd_alloc_resource(device_t dev, int type, int *rid, rman_res_t start,
    rman_res_t end, rman_res_t count, u_int flags)
{
	return BHND_BUS_ALLOC_RESOURCE(device_get_parent(dev), dev, type, rid,
	    start, end, count, flags);
}


/**
 * Allocate a resource from a device's parent bhnd(4) bus, using the
 * resource's default start, end, and count values.
 * 
 * @param dev The device requesting resource ownership.
 * @param type The type of resource to allocate. This may be any type supported
 * by the standard bus APIs.
 * @param rid The bus-specific handle identifying the resource being allocated.
 * @param flags The flags for the resource to be allocated. These may be any
 * values supported by the standard bus APIs.
 * 
 * @retval NULL The resource could not be allocated.
 * @retval resource The allocated resource.
 */
static inline struct bhnd_resource *
bhnd_alloc_resource_any(device_t dev, int type, int *rid, u_int flags)
{
	return bhnd_alloc_resource(dev, type, rid, 0, ~0, 1, flags);
}

/**
 * Activate a previously allocated bhnd resource.
 *
 * @param dev The device holding ownership of the allocated resource.
 * @param type The type of the resource. 
 * @param rid The bus-specific handle identifying the resource.
 * @param r A pointer to the resource returned by bhnd_alloc_resource or
 * BHND_BUS_ALLOC_RESOURCE.
 * 
 * @retval 0 success
 * @retval non-zero an error occurred while activating the resource.
 */
static inline int
bhnd_activate_resource(device_t dev, int type, int rid,
   struct bhnd_resource *r)
{
	return BHND_BUS_ACTIVATE_RESOURCE(device_get_parent(dev), dev, type,
	    rid, r);
}

/**
 * Deactivate a previously activated bhnd resource.
 *
 * @param dev The device holding ownership of the activated resource.
 * @param type The type of the resource. 
 * @param rid The bus-specific handle identifying the resource.
 * @param r A pointer to the resource returned by bhnd_alloc_resource or
 * BHND_BUS_ALLOC_RESOURCE.
 * 
 * @retval 0 success
 * @retval non-zero an error occurred while activating the resource.
 */
static inline int
bhnd_deactivate_resource(device_t dev, int type, int rid,
   struct bhnd_resource *r)
{
	return BHND_BUS_DEACTIVATE_RESOURCE(device_get_parent(dev), dev, type,
	    rid, r);
}

/**
 * Free a resource allocated by bhnd_alloc_resource().
 *
 * @param dev The device holding ownership of the resource.
 * @param type The type of the resource. 
 * @param rid The bus-specific handle identifying the resource.
 * @param r A pointer to the resource returned by bhnd_alloc_resource or
 * BHND_ALLOC_RESOURCE.
 * 
 * @retval 0 success
 * @retval non-zero an error occurred while activating the resource.
 */
static inline int
bhnd_release_resource(device_t dev, int type, int rid,
   struct bhnd_resource *r)
{
	return BHND_BUS_RELEASE_RESOURCE(device_get_parent(dev), dev, type,
	    rid, r);
}

/**
 * Return true if @p region_num is a valid region on @p port_num of
 * @p type attached to @p dev.
 *
 * @param dev A bhnd bus child device.
 * @param type The port type being queried.
 * @param port_num The port number being queried.
 * @param region_num The region number being queried.
 */
static inline bool
bhnd_is_region_valid(device_t dev, bhnd_port_type type, u_int port_num,
    u_int region_num)
{
	return (BHND_BUS_IS_REGION_VALID(device_get_parent(dev), dev, type,
	    port_num, region_num));
}

/**
 * Return the number of ports of type @p type attached to @p def.
 *
 * @param dev A bhnd bus child device.
 * @param type The port type being queried.
 */
static inline u_int
bhnd_get_port_count(device_t dev, bhnd_port_type type) {
	return (BHND_BUS_GET_PORT_COUNT(device_get_parent(dev), dev, type));
}

/**
 * Return the number of memory regions mapped to @p child @p port of
 * type @p type.
 *
 * @param dev A bhnd bus child device.
 * @param port The port number being queried.
 * @param type The port type being queried.
 */
static inline u_int
bhnd_get_region_count(device_t dev, bhnd_port_type type, u_int port) {
	return (BHND_BUS_GET_REGION_COUNT(device_get_parent(dev), dev, type,
	    port));
}

/**
 * Return the resource-ID for a memory region on the given device port.
 *
 * @param dev A bhnd bus child device.
 * @param type The port type.
 * @param port The port identifier.
 * @param region The identifier of the memory region on @p port.
 * 
 * @retval int The RID for the given @p port and @p region on @p device.
 * @retval -1 No such port/region found.
 */
static inline int
bhnd_get_port_rid(device_t dev, bhnd_port_type type, u_int port, u_int region)
{
	return BHND_BUS_GET_PORT_RID(device_get_parent(dev), dev, type, port,
	    region);
}

/**
 * Decode a port / region pair on @p dev defined by @p rid.
 *
 * @param dev A bhnd bus child device.
 * @param type The resource type.
 * @param rid The resource identifier.
 * @param[out] port_type The decoded port type.
 * @param[out] port The decoded port identifier.
 * @param[out] region The decoded region identifier.
 *
 * @retval 0 success
 * @retval non-zero No matching port/region found.
 */
static inline int
bhnd_decode_port_rid(device_t dev, int type, int rid, bhnd_port_type *port_type,
    u_int *port, u_int *region)
{
	return BHND_BUS_DECODE_PORT_RID(device_get_parent(dev), dev, type, rid,
	    port_type, port, region);
}

/**
 * Get the address and size of @p region on @p port.
 *
 * @param dev A bhnd bus child device.
 * @param port_type The port type.
 * @param port The port identifier.
 * @param region The identifier of the memory region on @p port.
 * @param[out] region_addr The region's base address.
 * @param[out] region_size The region's size.
 *
 * @retval 0 success
 * @retval non-zero No matching port/region found.
 */
static inline int
bhnd_get_region_addr(device_t dev, bhnd_port_type port_type, u_int port,
    u_int region, bhnd_addr_t *region_addr, bhnd_size_t *region_size)
{
	return BHND_BUS_GET_REGION_ADDR(device_get_parent(dev), dev, port_type,
	    port, region, region_addr, region_size);
}

/*
 * bhnd bus-level equivalents of the bus_(read|write|set|barrier|...)
 * macros (compatible with bhnd_resource).
 *
 * Generated with bhnd/tools/bus_macro.sh
 */
#define bhnd_bus_barrier(r, o, l, f) \
    ((r)->direct) ? \
	bus_barrier((r)->res, (o), (l), (f)) : \
	BHND_BUS_BARRIER( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (l), (f))
#define bhnd_bus_read_1(r, o) \
    ((r)->direct) ? \
	bus_read_1((r)->res, (o)) : \
	BHND_BUS_READ_1( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o))
#define bhnd_bus_read_multi_1(r, o, d, c) \
    ((r)->direct) ? \
	bus_read_multi_1((r)->res, (o), (d), (c)) : \
	BHND_BUS_READ_MULTI_1( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_read_region_1(r, o, d, c) \
    ((r)->direct) ? \
	bus_read_region_1((r)->res, (o), (d), (c)) : \
	BHND_BUS_READ_REGION_1( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_write_1(r, o, v) \
    ((r)->direct) ? \
	bus_write_1((r)->res, (o), (v)) : \
	BHND_BUS_WRITE_1( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (v))
#define bhnd_bus_write_multi_1(r, o, d, c) \
    ((r)->direct) ? \
	bus_write_multi_1((r)->res, (o), (d), (c)) : \
	BHND_BUS_WRITE_MULTI_1( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_write_region_1(r, o, d, c) \
    ((r)->direct) ? \
	bus_write_region_1((r)->res, (o), (d), (c)) : \
	BHND_BUS_WRITE_REGION_1( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_read_stream_1(r, o) \
    ((r)->direct) ? \
	bus_read_stream_1((r)->res, (o)) : \
	BHND_BUS_READ_STREAM_1( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o))
#define bhnd_bus_read_multi_stream_1(r, o, d, c) \
    ((r)->direct) ? \
	bus_read_multi_stream_1((r)->res, (o), (d), (c)) : \
	BHND_BUS_READ_MULTI_STREAM_1( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_read_region_stream_1(r, o, d, c) \
    ((r)->direct) ? \
	bus_read_region_stream_1((r)->res, (o), (d), (c)) : \
	BHND_BUS_READ_REGION_STREAM_1( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_write_stream_1(r, o, v) \
    ((r)->direct) ? \
	bus_write_stream_1((r)->res, (o), (v)) : \
	BHND_BUS_WRITE_STREAM_1( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (v))
#define bhnd_bus_write_multi_stream_1(r, o, d, c) \
    ((r)->direct) ? \
	bus_write_multi_stream_1((r)->res, (o), (d), (c)) : \
	BHND_BUS_WRITE_MULTI_STREAM_1( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_write_region_stream_1(r, o, d, c) \
    ((r)->direct) ? \
	bus_write_region_stream_1((r)->res, (o), (d), (c)) : \
	BHND_BUS_WRITE_REGION_STREAM_1( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_set_multi_1(r, o, v, c) \
    ((r)->direct) ? \
	bus_set_multi_1((r)->res, (o), (v), (c)) : \
	BHND_BUS_SET_MULTI_1( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (v), (c))
#define bhnd_bus_set_region_1(r, o, v, c) \
    ((r)->direct) ? \
	bus_set_region_1((r)->res, (o), (v), (c)) : \
	BHND_BUS_SET_REGION_1( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (v), (c))
#define bhnd_bus_read_2(r, o) \
    ((r)->direct) ? \
	bus_read_2((r)->res, (o)) : \
	BHND_BUS_READ_2( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o))
#define bhnd_bus_read_multi_2(r, o, d, c) \
    ((r)->direct) ? \
	bus_read_multi_2((r)->res, (o), (d), (c)) : \
	BHND_BUS_READ_MULTI_2( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_read_region_2(r, o, d, c) \
    ((r)->direct) ? \
	bus_read_region_2((r)->res, (o), (d), (c)) : \
	BHND_BUS_READ_REGION_2( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_write_2(r, o, v) \
    ((r)->direct) ? \
	bus_write_2((r)->res, (o), (v)) : \
	BHND_BUS_WRITE_2( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (v))
#define bhnd_bus_write_multi_2(r, o, d, c) \
    ((r)->direct) ? \
	bus_write_multi_2((r)->res, (o), (d), (c)) : \
	BHND_BUS_WRITE_MULTI_2( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_write_region_2(r, o, d, c) \
    ((r)->direct) ? \
	bus_write_region_2((r)->res, (o), (d), (c)) : \
	BHND_BUS_WRITE_REGION_2( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_read_stream_2(r, o) \
    ((r)->direct) ? \
	bus_read_stream_2((r)->res, (o)) : \
	BHND_BUS_READ_STREAM_2( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o))
#define bhnd_bus_read_multi_stream_2(r, o, d, c) \
    ((r)->direct) ? \
	bus_read_multi_stream_2((r)->res, (o), (d), (c)) : \
	BHND_BUS_READ_MULTI_STREAM_2( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_read_region_stream_2(r, o, d, c) \
    ((r)->direct) ? \
	bus_read_region_stream_2((r)->res, (o), (d), (c)) : \
	BHND_BUS_READ_REGION_STREAM_2( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_write_stream_2(r, o, v) \
    ((r)->direct) ? \
	bus_write_stream_2((r)->res, (o), (v)) : \
	BHND_BUS_WRITE_STREAM_2( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (v))
#define bhnd_bus_write_multi_stream_2(r, o, d, c) \
    ((r)->direct) ? \
	bus_write_multi_stream_2((r)->res, (o), (d), (c)) : \
	BHND_BUS_WRITE_MULTI_STREAM_2( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_write_region_stream_2(r, o, d, c) \
    ((r)->direct) ? \
	bus_write_region_stream_2((r)->res, (o), (d), (c)) : \
	BHND_BUS_WRITE_REGION_STREAM_2( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_set_multi_2(r, o, v, c) \
    ((r)->direct) ? \
	bus_set_multi_2((r)->res, (o), (v), (c)) : \
	BHND_BUS_SET_MULTI_2( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (v), (c))
#define bhnd_bus_set_region_2(r, o, v, c) \
    ((r)->direct) ? \
	bus_set_region_2((r)->res, (o), (v), (c)) : \
	BHND_BUS_SET_REGION_2( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (v), (c))
#define bhnd_bus_read_4(r, o) \
    ((r)->direct) ? \
	bus_read_4((r)->res, (o)) : \
	BHND_BUS_READ_4( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o))
#define bhnd_bus_read_multi_4(r, o, d, c) \
    ((r)->direct) ? \
	bus_read_multi_4((r)->res, (o), (d), (c)) : \
	BHND_BUS_READ_MULTI_4( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_read_region_4(r, o, d, c) \
    ((r)->direct) ? \
	bus_read_region_4((r)->res, (o), (d), (c)) : \
	BHND_BUS_READ_REGION_4( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_write_4(r, o, v) \
    ((r)->direct) ? \
	bus_write_4((r)->res, (o), (v)) : \
	BHND_BUS_WRITE_4( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (v))
#define bhnd_bus_write_multi_4(r, o, d, c) \
    ((r)->direct) ? \
	bus_write_multi_4((r)->res, (o), (d), (c)) : \
	BHND_BUS_WRITE_MULTI_4( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_write_region_4(r, o, d, c) \
    ((r)->direct) ? \
	bus_write_region_4((r)->res, (o), (d), (c)) : \
	BHND_BUS_WRITE_REGION_4( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_read_stream_4(r, o) \
    ((r)->direct) ? \
	bus_read_stream_4((r)->res, (o)) : \
	BHND_BUS_READ_STREAM_4( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o))
#define bhnd_bus_read_multi_stream_4(r, o, d, c) \
    ((r)->direct) ? \
	bus_read_multi_stream_4((r)->res, (o), (d), (c)) : \
	BHND_BUS_READ_MULTI_STREAM_4( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_read_region_stream_4(r, o, d, c) \
    ((r)->direct) ? \
	bus_read_region_stream_4((r)->res, (o), (d), (c)) : \
	BHND_BUS_READ_REGION_STREAM_4( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_write_stream_4(r, o, v) \
    ((r)->direct) ? \
	bus_write_stream_4((r)->res, (o), (v)) : \
	BHND_BUS_WRITE_STREAM_4( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (v))
#define bhnd_bus_write_multi_stream_4(r, o, d, c) \
    ((r)->direct) ? \
	bus_write_multi_stream_4((r)->res, (o), (d), (c)) : \
	BHND_BUS_WRITE_MULTI_STREAM_4( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_write_region_stream_4(r, o, d, c) \
    ((r)->direct) ? \
	bus_write_region_stream_4((r)->res, (o), (d), (c)) : \
	BHND_BUS_WRITE_REGION_STREAM_4( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (d), (c))
#define bhnd_bus_set_multi_4(r, o, v, c) \
    ((r)->direct) ? \
	bus_set_multi_4((r)->res, (o), (v), (c)) : \
	BHND_BUS_SET_MULTI_4( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (v), (c))
#define bhnd_bus_set_region_4(r, o, v, c) \
    ((r)->direct) ? \
	bus_set_region_4((r)->res, (o), (v), (c)) : \
	BHND_BUS_SET_REGION_4( \
	    device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (v), (c))

#endif /* _BHND_BHND_H_ */
