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
#include "bhnd_bus_if.h"

extern devclass_t bhnd_devclass;
extern devclass_t bhnd_hostb_devclass;
extern devclass_t bhnd_nvram_devclass;

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

/**
 * A bhnd(4) core descriptor.
 */
struct bhnd_core_info {
	uint16_t	vendor;		/**< vendor */
	uint16_t	device;		/**< device */
	uint16_t	hwrev;		/**< hardware revision */
	u_int		core_idx;	/**< bus-assigned core index */
	int		unit;		/**< bus-assigned core unit */
};


/**
 * A hardware revision match descriptor.
 */
struct bhnd_hwrev_match {
	uint16_t	start;	/**< first revision, or BHND_HWREV_INVALID
					     to match on any revision. */
	uint16_t	end;	/**< last revision, or BHND_HWREV_INVALID
					     to match on any revision. */
};

/** 
 * Wildcard hardware revision match descriptor.
 */
#define	BHND_HWREV_ANY		{ BHND_HWREV_INVALID, BHND_HWREV_INVALID }
#define	BHND_HWREV_IS_ANY(_m)	\
	((_m)->start == BHND_HWREV_INVALID && (_m)->end == BHND_HWREV_INVALID)

/**
 * Hardware revision match descriptor for an inclusive range.
 * 
 * @param _start The first applicable hardware revision.
 * @param _end The last applicable hardware revision, or BHND_HWREV_INVALID
 * to match on any revision.
 */
#define	BHND_HWREV_RANGE(_start, _end)	{ _start, _end }

/**
 * Hardware revision match descriptor for a single revision.
 * 
 * @param _hwrev The hardware revision to match on.
 */
#define	BHND_HWREV_EQ(_hwrev)	BHND_HWREV_RANGE(_hwrev, _hwrev)

/**
 * Hardware revision match descriptor for any revision equal to or greater
 * than @p _start.
 * 
 * @param _start The first hardware revision to match on.
 */
#define	BHND_HWREV_GTE(_start)	BHND_HWREV_RANGE(_start, BHND_HWREV_INVALID)

/**
 * Hardware revision match descriptor for any revision equal to or less
 * than @p _end.
 * 
 * @param _end The last hardware revision to match on.
 */
#define	BHND_HWREV_LTE(_end)	BHND_HWREV_RANGE(0, _end)


/** A core match descriptor. */
struct bhnd_core_match {
	uint16_t		vendor;	/**< required JEP106 device vendor or BHND_MFGID_INVALID. */
	uint16_t		device;	/**< required core ID or BHND_COREID_INVALID */
	struct bhnd_hwrev_match	hwrev;	/**< matching revisions. */
	bhnd_devclass_t		class;	/**< required class or BHND_DEVCLASS_INVALID */
	int			unit;	/**< required core unit, or -1 */
};

/**
 * Core match descriptor matching against the given @p _vendor, @p _device,
 * and @p _hwrev match descriptors.
 */
#define	BHND_CORE_MATCH(_vendor, _device, _hwrev)	\
	{ _vendor, _device, _hwrev, BHND_DEVCLASS_INVALID, -1 }

/** 
 * Wildcard core match descriptor.
 */
#define	BHND_CORE_MATCH_ANY			\
	{					\
		.vendor = BHND_MFGID_INVALID,	\
		.device = BHND_COREID_INVALID,	\
		.hwrev = BHND_HWREV_ANY,	\
		.class = BHND_DEVCLASS_INVALID,	\
		.unit = -1			\
	}

/**
 * Device quirk table descriptor.
 */
struct bhnd_device_quirk {
	struct bhnd_hwrev_match	 hwrev;		/**< applicable hardware revisions */
	uint32_t		 quirks;	/**< quirk flags */
};
#define	BHND_DEVICE_QUIRK_END		{ BHND_HWREV_ANY, 0 }
#define	BHND_DEVICE_QUIRK_IS_END(_q)	\
	(BHND_HWREV_IS_ANY(&(_q)->hwrev) && (_q)->quirks == 0)

enum {
	BHND_DF_ANY	= 0,
	BHND_DF_HOSTB	= (1<<0)	/**< core is serving as the bus'
					  *  host bridge */
};

/** Device probe table descriptor */
struct bhnd_device {
	const struct bhnd_core_match	 core;			/**< core match descriptor */ 
	const char			*desc;			/**< device description, or NULL. */
	const struct bhnd_device_quirk	*quirks_table;		/**< quirks table for this device, or NULL */
	uint32_t			 device_flags;		/**< required BHND_DF_* flags */
};

#define	_BHND_DEVICE(_device, _desc, _quirks, _flags, ...)	\
	{ BHND_CORE_MATCH(BHND_MFGID_BCM, BHND_COREID_ ## _device, \
	    BHND_HWREV_ANY), _desc, _quirks, _flags }

#define	BHND_DEVICE(_device, _desc, _quirks, ...)	\
	_BHND_DEVICE(_device, _desc, _quirks, ## __VA_ARGS__, 0)

#define	BHND_DEVICE_END			{ BHND_CORE_MATCH_ANY, NULL, NULL, 0 }

const char			*bhnd_vendor_name(uint16_t vendor);
const char			*bhnd_port_type_name(bhnd_port_type port_type);

const char 			*bhnd_find_core_name(uint16_t vendor,
				     uint16_t device);
bhnd_devclass_t			 bhnd_find_core_class(uint16_t vendor,
				     uint16_t device);

const char			*bhnd_core_name(const struct bhnd_core_info *ci);
bhnd_devclass_t			 bhnd_core_class(const struct bhnd_core_info *ci);


device_t			 bhnd_match_child(device_t dev,
				     const struct bhnd_core_match *desc);

device_t			 bhnd_find_child(device_t dev,
				     bhnd_devclass_t class, int unit);

const struct bhnd_core_info	*bhnd_match_core(
				     const struct bhnd_core_info *cores,
				     u_int num_cores,
				     const struct bhnd_core_match *desc);

const struct bhnd_core_info	*bhnd_find_core(
				     const struct bhnd_core_info *cores,
				     u_int num_cores, bhnd_devclass_t class);

bool				 bhnd_core_matches(
				     const struct bhnd_core_info *core,
				     const struct bhnd_core_match *desc);

bool				 bhnd_hwrev_matches(uint16_t hwrev,
				     const struct bhnd_hwrev_match *desc);

bool				 bhnd_device_matches(device_t dev,
				     const struct bhnd_core_match *desc);

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

int				 bhnd_read_chipid(device_t dev,
				     struct resource_spec *rs,
				     bus_size_t chipc_offset,
				     struct bhnd_chipid *result);

void				 bhnd_set_custom_core_desc(device_t dev,
				     const char *name);
void				 bhnd_set_default_core_desc(device_t dev);


bool				 bhnd_bus_generic_is_hostb_device(device_t dev,
				     device_t child);
bool				 bhnd_bus_generic_is_hw_disabled(device_t dev,
				     device_t child);
bool				 bhnd_bus_generic_is_region_valid(device_t dev,
				     device_t child, bhnd_port_type type,
				     u_int port, u_int region);
int				 bhnd_bus_generic_read_nvram_var(device_t dev,
				     device_t child, const char *name,
				     void *buf, size_t *size);
const struct bhnd_chipid	*bhnd_bus_generic_get_chipid(device_t dev,
				     device_t child);
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



/**
 * Return true if @p dev is serving as a host bridge for its parent bhnd
 * bus.
 *
 * @param dev A bhnd bus child device.
 */
static inline bool
bhnd_is_hostb_device(device_t dev) {
	return (BHND_BUS_IS_HOSTB_DEVICE(device_get_parent(dev), dev));
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
 * @retval non-zero an error occured while activating the resource.
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
 * @retval non-zero an error occured while activating the resource.
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
 * @retval non-zero an error occured while activating the resource.
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
	BHND_BUS_BARRIER(device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (l), (f))
#define bhnd_bus_read_1(r, o) \
    ((r)->direct) ? \
	bus_read_1((r)->res, (o)) : \
	BHND_BUS_READ_1(device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o))
#define bhnd_bus_write_1(r, o, v) \
    ((r)->direct) ? \
	bus_write_1((r)->res, (o), (v)) : \
	BHND_BUS_WRITE_1(device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (v))
#define bhnd_bus_read_2(r, o) \
    ((r)->direct) ? \
	bus_read_2((r)->res, (o)) : \
	BHND_BUS_READ_2(device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o))
#define bhnd_bus_write_2(r, o, v) \
    ((r)->direct) ? \
	bus_write_2((r)->res, (o), (v)) : \
	BHND_BUS_WRITE_2(device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (v))
#define bhnd_bus_read_4(r, o) \
    ((r)->direct) ? \
	bus_read_4((r)->res, (o)) : \
	BHND_BUS_READ_4(device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o))
#define bhnd_bus_write_4(r, o, v) \
    ((r)->direct) ? \
	bus_write_4((r)->res, (o), (v)) : \
	BHND_BUS_WRITE_4(device_get_parent(rman_get_device((r)->res)),	\
	    rman_get_device((r)->res), (r), (o), (v))

#endif /* _BHND_BHND_H_ */
