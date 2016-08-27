#-
# Copyright (c) 2015 Landon Fuller <landon@landonf.org>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
# USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/bhnd/bhnd_types.h>

INTERFACE bhnd_bus;

#
# bhnd(4) bus interface
#

HEADER {
	/* forward declarations */
	struct bhnd_board_info;
	struct bhnd_core_info;
	struct bhnd_chipid;
	struct bhnd_devinfo;
	struct bhnd_resource;
}

CODE {
	#include <sys/systm.h>

	#include <dev/bhnd/bhndvar.h>
	
	static struct bhnd_chipid *
	bhnd_bus_null_get_chipid(device_t dev, device_t child)
	{
		panic("bhnd_bus_get_chipid unimplemented");
	}

	static bhnd_attach_type
	bhnd_bus_null_get_attach_type(device_t dev, device_t child)
	{
		panic("bhnd_bus_get_attach_type unimplemented");
	}
	
	static bhnd_clksrc
	bhnd_bus_null_pwrctl_get_clksrc(device_t dev, device_t child,
	    bhnd_clock clock)
	{
		return (BHND_CLKSRC_UNKNOWN);
	}

	static int
	bhnd_bus_null_pwrctl_gate_clock(device_t dev, device_t child,
	    bhnd_clock clock)
	{
		return (ENODEV);
	}

	static int
	bhnd_bus_null_pwrctl_ungate_clock(device_t dev, device_t child,
	    bhnd_clock clock)
	{
		return (ENODEV);
	}

	static int
	bhnd_bus_null_read_board_info(device_t dev, device_t child,
	    struct bhnd_board_info *info)
	{
		panic("bhnd_bus_read_boardinfo unimplemented");
	}
	
	static void
	bhnd_bus_null_child_added(device_t dev, device_t child)
	{
	}

	static int
	bhnd_bus_null_alloc_pmu(device_t dev, device_t child)
	{
		panic("bhnd_bus_alloc_pmu unimplemented");
	}

	static int
	bhnd_bus_null_release_pmu(device_t dev, device_t child)
	{
		panic("bhnd_bus_release_pmu unimplemented");
	}

	static int
	bhnd_bus_null_request_clock(device_t dev, device_t child,
	    bhnd_clock clock)
	{
		panic("bhnd_bus_request_clock unimplemented");
	}

	static int
	bhnd_bus_null_enable_clocks(device_t dev, device_t child,
	    uint32_t clocks)
	{
		panic("bhnd_bus_enable_clocks unimplemented");
	}
	
	static int
	bhnd_bus_null_request_ext_rsrc(device_t dev, device_t child,
	    u_int rsrc)
	{
		panic("bhnd_bus_request_ext_rsrc unimplemented");
	}

	static int
	bhnd_bus_null_release_ext_rsrc(device_t dev, device_t child,
	    u_int rsrc)
	{
		panic("bhnd_bus_release_ext_rsrc unimplemented");
	}

	static uint32_t
	bhnd_bus_null_read_config(device_t dev, device_t child,
	    bus_size_t offset, u_int width)
	{
		panic("bhnd_bus_null_read_config unimplemented");
	}

	static void
	bhnd_bus_null_write_config(device_t dev, device_t child,
	    bus_size_t offset, uint32_t val, u_int width)
	{
		panic("bhnd_bus_null_write_config unimplemented");
	}

	static device_t
	bhnd_bus_null_find_hostb_device(device_t dev)
	{
		panic("bhnd_bus_find_hostb_device unimplemented");
	}

	static bool
	bhnd_bus_null_is_hw_disabled(device_t dev, device_t child)
	{
		panic("bhnd_bus_is_hw_disabled unimplemented");
	}
	
	static int
	bhnd_bus_null_get_probe_order(device_t dev, device_t child)
	{
		panic("bhnd_bus_get_probe_order unimplemented");
	}

	static int
	bhnd_bus_null_get_port_rid(device_t dev, device_t child,
	    bhnd_port_type port_type, u_int port, u_int region)
	{
		return (-1);
	}
	
	static int
	bhnd_bus_null_decode_port_rid(device_t dev, device_t child, int type,
	    int rid, bhnd_port_type *port_type, u_int *port, u_int *region)
	{
		return (ENOENT);
	}

	static int
	bhnd_bus_null_get_region_addr(device_t dev, device_t child, 
	    bhnd_port_type type, u_int port, u_int region, bhnd_addr_t *addr,
	    bhnd_size_t *size)
	{
		return (ENOENT);
	}
	
	static int
	bhnd_bus_null_get_nvram_var(device_t dev, device_t child,
	    const char *name, void *buf, size_t *size, bhnd_nvram_type type)
	{
		return (ENODEV);
	}

}

/**
 * Return the active host bridge core for the bhnd bus, if any.
 *
 * @param dev The bhnd bus device.
 *
 * @retval device_t if a hostb device exists
 * @retval NULL if no hostb device is found.
 */
METHOD device_t find_hostb_device {
	device_t dev;
} DEFAULT bhnd_bus_null_find_hostb_device;

/**
 * Return true if the hardware components required by @p child are unpopulated
 * or otherwise unusable.
 *
 * In some cases, enumerated devices may have pins that are left floating, or
 * the hardware may otherwise be non-functional; this method allows a parent
 * device to explicitly specify if a successfully enumerated @p child should
 * be disabled.
 *
 * @param dev The device whose child is being examined.
 * @param child The child device.
 */
METHOD bool is_hw_disabled {
	device_t dev;
	device_t child;
} DEFAULT bhnd_bus_null_is_hw_disabled;

/**
 * Return the probe (and attach) order for @p child. 
 *
 * All devices on the bhnd(4) bus will be probed, attached, or resumed in
 * ascending order; they will be suspended, shutdown, and detached in
 * descending order.
 *
 * The following device methods will be dispatched in ascending probe order
 * by the bus:
 *
 * - DEVICE_PROBE()
 * - DEVICE_ATTACH()
 * - DEVICE_RESUME()
 *
 * The following device methods will be dispatched in descending probe order
 * by the bus:
 *
 * - DEVICE_SHUTDOWN()
 * - DEVICE_DETACH()
 * - DEVICE_SUSPEND()
 *
 * @param dev The device whose child is being examined.
 * @param child The child device.
 *
 * Refer to BHND_PROBE_* and BHND_PROBE_ORDER_* for the standard set of
 * priorities.
 */
METHOD int get_probe_order {
	device_t dev;
	device_t child;
} DEFAULT bhnd_bus_null_get_probe_order;

/**
 * Return the BHND chip identification for the parent bus.
 *
 * @param dev The device whose child is being examined.
 * @param child The child device.
 */
METHOD const struct bhnd_chipid * get_chipid {
	device_t dev;
	device_t child;
} DEFAULT bhnd_bus_null_get_chipid;

/**
 * Return the BHND attachment type of the parent bus.
 *
 * @param dev The device whose child is being examined.
 * @param child The child device.
 *
 * @retval BHND_ATTACH_ADAPTER if the bus is resident on a bridged adapter,
 * such as a WiFi chipset.
 * @retval BHND_ATTACH_NATIVE if the bus provides hardware services (clock,
 * CPU, etc) to a directly attached native host.
 */
METHOD bhnd_attach_type get_attach_type {
	device_t dev;
	device_t child;
} DEFAULT bhnd_bus_null_get_attach_type;

/**
 * Attempt to read the BHND board identification from the parent bus.
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
METHOD int read_board_info {
	device_t dev;
	device_t child;
	struct bhnd_board_info *info;
} DEFAULT bhnd_bus_null_read_board_info;

/**
 * Allocate and zero-initialize a buffer suitably sized and aligned for a
 * bhnd_devinfo structure.
 *
 * @param dev The bhnd bus device.
 *
 * @retval non-NULL	success
 * @retval NULL		allocation failed
 */
METHOD struct bhnd_devinfo * alloc_devinfo {
	device_t dev;
};

/**
 * Release memory previously allocated for @p devinfo.
 *
 * @param dev The bhnd bus device.
 * @param dinfo A devinfo buffer previously allocated via
 * BHND_BUS_ALLOC_DEVINFO().
 */
METHOD void free_devinfo {
	device_t dev;
	struct bhnd_devinfo *dinfo;
};

/**
 * Notify a bhnd bus that a child was added.
 *
 * This method must be called by concrete bhnd(4) driver impementations
 * after @p child's bus state is fully initialized.
 *
 * @param dev The bhnd bus whose child is being added.
 * @param child The child added to @p dev.
 */
METHOD void child_added {
	device_t dev;
	device_t child;
} DEFAULT bhnd_bus_null_child_added;

/**
 * Reset the device's hardware core.
 *
 * @param dev The parent of @p child.
 * @param child The device to be reset.
 * @param flags Device-specific core flags to be supplied on reset.
 *
 * @retval 0 success
 * @retval non-zero error
 */
METHOD int reset_core {
	device_t dev;
	device_t child;
	uint16_t flags;
}

/**
 * Suspend a device hardware core.
 *
 * @param dev The parent of @p child.
 * @param child The device to be reset.
 *
 * @retval 0 success
 * @retval non-zero error
 */
METHOD int suspend_core {
	device_t dev;
	device_t child;
}

/**
 * If supported by the chipset, return the clock source for the given clock.
 *
 * This function is only supported on early PWRCTL-equipped chipsets
 * that expose clock management via their host bridge interface. Currently,
 * this includes PCI (not PCIe) devices, with ChipCommon core revisions 0-9.
 *
 * @param dev The parent of @p child.
 * @param child The bhnd device requesting a clock source.
 * @param clock The clock for which a clock source will be returned.
 *
 * @retval	bhnd_clksrc		The clock source for @p clock.
 * @retval	BHND_CLKSRC_UNKNOWN	If @p clock is unsupported, or its
 *					clock source is not known to the bus.
 */
METHOD bhnd_clksrc pwrctl_get_clksrc {
	device_t dev;
	device_t child;
	bhnd_clock clock;
} DEFAULT bhnd_bus_null_pwrctl_get_clksrc;

/**
 * If supported by the chipset, gate the clock source for @p clock
 *
 * This function is only supported on early PWRCTL-equipped chipsets
 * that expose clock management via their host bridge interface. Currently,
 * this includes PCI (not PCIe) devices, with ChipCommon core revisions 0-9.
 *
 * @param dev The parent of @p child.
 * @param child The bhnd device requesting clock gating.
 * @param clock The clock to be disabled.
 *
 * @retval 0 success
 * @retval ENODEV If bus-level clock source management is not supported.
 * @retval ENXIO If bus-level management of @p clock is not supported.
 */
METHOD int pwrctl_gate_clock {
	device_t dev;
	device_t child;
	bhnd_clock clock;
} DEFAULT bhnd_bus_null_pwrctl_gate_clock;

/**
 * If supported by the chipset, ungate the clock source for @p clock
 *
 * This function is only supported on early PWRCTL-equipped chipsets
 * that expose clock management via their host bridge interface. Currently,
 * this includes PCI (not PCIe) devices, with ChipCommon core revisions 0-9.
 *
 * @param dev The parent of @p child.
 * @param child The bhnd device requesting clock gating.
 * @param clock The clock to be enabled.
 *
 * @retval 0 success
 * @retval ENODEV If bus-level clock source management is not supported.
 * @retval ENXIO If bus-level management of @p clock is not supported.
 */
METHOD int pwrctl_ungate_clock {
	device_t dev;
	device_t child;
	bhnd_clock clock;
} DEFAULT bhnd_bus_null_pwrctl_ungate_clock;

/**
 * Allocate and enable per-core PMU request handling for @p child.
 *
 * The region containing the core's PMU register block (if any) must be
 * allocated via bus_alloc_resource(9) (or bhnd_alloc_resource) before
 * calling BHND_BUS_ALLOC_PMU(), and must not be released until after
 * calling BHND_BUS_RELEASE_PMU().
 *
 * @param dev The parent of @p child.
 * @param child The requesting bhnd device.
 */
METHOD int alloc_pmu {
	device_t dev;
	device_t child;
} DEFAULT bhnd_bus_null_alloc_pmu;

/**
 * Release per-core PMU resources allocated for @p child. Any
 * outstanding PMU requests are discarded.
 *
 * @param dev The parent of @p child.
 * @param child The requesting bhnd device.
 */
METHOD int release_pmu {
	device_t dev;
	device_t child;
} DEFAULT bhnd_bus_null_release_pmu;

/** 
 * Request that @p clock (or faster) be routed to @p child.
 * 
 * A driver must ask the bhnd bus to allocate PMU request state
 * via BHND_BUS_ALLOC_PMU() before it can request clock resources.
 * 
 * Request multiplexing is managed by the bus.
 *
 * @param dev The parent of @p child.
 * @param child The bhnd device requesting @p clock.
 * @param clock The requested clock source. 
 *
 * @retval 0 success
 * @retval ENODEV If an unsupported clock was requested.
 * @retval ENXIO If the PMU has not been initialized or is otherwise unvailable.
 */
METHOD int request_clock {
	device_t dev;
	device_t child;
	bhnd_clock clock;
} DEFAULT bhnd_bus_null_request_clock;

/**
 * Request that @p clocks be powered on behalf of @p child.
 *
 * This will power on clock sources (e.g. XTAL, PLL, etc) required for
 * @p clocks and wait until they are ready, discarding any previous
 * requests by @p child.
 *
 * Request multiplexing is managed by the bus.
 * 
 * A driver must ask the bhnd bus to allocate PMU request state
 * via BHND_BUS_ALLOC_PMU() before it can request clock resources.
 *
 * @param dev The parent of @p child.
 * @param child The bhnd device requesting @p clock.
 * @param clock The requested clock source.
 *
 * @retval 0 success
 * @retval ENODEV If an unsupported clock was requested.
 * @retval ENXIO If the PMU has not been initialized or is otherwise unvailable.
 */
METHOD int enable_clocks {
	device_t dev;
	device_t child;
	uint32_t clocks;
} DEFAULT bhnd_bus_null_enable_clocks;

/**
 * Power up an external PMU-managed resource assigned to @p child.
 * 
 * A driver must ask the bhnd bus to allocate PMU request state
 * via BHND_BUS_ALLOC_PMU() before it can request PMU resources.
 *
 * @param dev The parent of @p child.
 * @param child The bhnd device requesting @p rsrc.
 * @param rsrc The core-specific external resource identifier.
 *
 * @retval 0 success
 * @retval ENODEV If the PMU does not support @p rsrc.
 * @retval ENXIO If the PMU has not been initialized or is otherwise unvailable.
 */
METHOD int request_ext_rsrc {
	device_t dev;
	device_t child;
	u_int rsrc;
} DEFAULT bhnd_bus_null_request_ext_rsrc;

/**
 * Power down an external PMU-managed resource assigned to @p child.
 * 
 * A driver must ask the bhnd bus to allocate PMU request state
 * via BHND_BUS_ALLOC_PMU() before it can request PMU resources.
 *
 * @param dev The parent of @p child.
 * @param child The bhnd device requesting @p rsrc.
 * @param rsrc The core-specific external resource number.
 *
 * @retval 0 success
 * @retval ENODEV If the PMU does not support @p rsrc.
 * @retval ENXIO If the PMU has not been initialized or is otherwise unvailable.
 */
METHOD int release_ext_rsrc {
	device_t dev;
	device_t child;
	u_int rsrc;
} DEFAULT bhnd_bus_null_release_ext_rsrc;

/**
 * Read @p width bytes at @p offset from the bus-specific agent/config
 * space of @p child.
 *
 * @param dev The parent of @p child.
 * @param child The bhnd device for which @p offset should be read.
 * @param offset The offset to be read.
 * @param width The size of the access. Must be 1, 2 or 4 bytes.
 *
 * The exact behavior of this method is bus-specific. In the case of
 * bcma(4), this method provides access to the first agent port of @p child.
 *
 * @note Device drivers should only use this API for functionality
 * that is not available via another bhnd(4) function.
 */
METHOD uint32_t read_config {
	device_t dev;
	device_t child;
	bus_size_t offset;
	u_int width;
} DEFAULT bhnd_bus_null_read_config;

/**
 * Read @p width bytes at @p offset from the bus-specific agent/config
 * space of @p child.
 *
 * @param dev The parent of @p child.
 * @param child The bhnd device for which @p offset should be read.
 * @param offset The offset to be written.
 * @param width The size of the access. Must be 1, 2 or 4 bytes.
 *
 * The exact behavior of this method is bus-specific. In the case of
 * bcma(4), this method provides access to the first agent port of @p child.
 *
 * @note Device drivers should only use this API for functionality
 * that is not available via another bhnd(4) function.
 */
METHOD void write_config {
	device_t dev;
	device_t child;
	bus_size_t offset;
	uint32_t val;
	u_int width;
} DEFAULT bhnd_bus_null_write_config;

/**
 * Allocate a bhnd resource.
 *
 * This method's semantics are functionally identical to the bus API of the same
 * name; refer to BUS_ALLOC_RESOURCE for complete documentation.
 */
METHOD struct bhnd_resource * alloc_resource {
	device_t dev;
	device_t child;
	int type;
	int *rid;
	rman_res_t start;
	rman_res_t end;
	rman_res_t count;
	u_int flags;
} DEFAULT bhnd_bus_generic_alloc_resource;

/**
 * Release a bhnd resource.
 *
 * This method's semantics are functionally identical to the bus API of the same
 * name; refer to BUS_RELEASE_RESOURCE for complete documentation.
 */
METHOD int release_resource {
	device_t dev;
	device_t child;
	int type;
	int rid;
	struct bhnd_resource *res;
} DEFAULT bhnd_bus_generic_release_resource;

/**
 * Activate a bhnd resource.
 *
 * This method's semantics are functionally identical to the bus API of the same
 * name; refer to BUS_ACTIVATE_RESOURCE for complete documentation.
 */
METHOD int activate_resource {
	device_t dev;
        device_t child;
	int type;
        int rid;
        struct bhnd_resource *r;
} DEFAULT bhnd_bus_generic_activate_resource;

/**
 * Deactivate a bhnd resource.
 *
 * This method's semantics are functionally identical to the bus API of the same
 * name; refer to BUS_DEACTIVATE_RESOURCE for complete documentation.
 */
METHOD int deactivate_resource {
        device_t dev;
        device_t child;
        int type;
	int rid;
        struct bhnd_resource *r;
} DEFAULT bhnd_bus_generic_deactivate_resource;

/**
 * Return true if @p region_num is a valid region on @p port_num of
 * @p type attached to @p child.
 *
 * @param dev The device whose child is being examined.
 * @param child The child device.
 * @param type The port type being queried.
 * @param port_num The port number being queried.
 * @param region_num The region number being queried.
 */
METHOD bool is_region_valid {
	device_t dev;
	device_t child;
	bhnd_port_type type;
	u_int port_num;
	u_int region_num;
};

/**
 * Return the number of ports of type @p type attached to @p child.
 *
 * @param dev The device whose child is being examined.
 * @param child The child device.
 * @param type The port type being queried.
 */
METHOD u_int get_port_count {
	device_t dev;
	device_t child;
	bhnd_port_type type;
};

/**
 * Return the number of memory regions mapped to @p child @p port of
 * type @p type.
 *
 * @param dev The device whose child is being examined.
 * @param child The child device.
 * @param port The port number being queried.
 * @param type The port type being queried.
 */
METHOD u_int get_region_count {
	device_t dev;
	device_t child;
	bhnd_port_type type;
	u_int port;
};

/**
 * Return the SYS_RES_MEMORY resource-ID for a port/region pair attached to
 * @p child.
 *
 * @param dev The bus device.
 * @param child The bhnd child.
 * @param port_type The port type.
 * @param port_num The index of the child interconnect port.
 * @param region_num The index of the port-mapped address region.
 *
 * @retval -1 No such port/region found.
 */
METHOD int get_port_rid {
	device_t dev;
	device_t child;
	bhnd_port_type port_type;
	u_int port_num;
	u_int region_num;
} DEFAULT bhnd_bus_null_get_port_rid;


/**
 * Decode a port / region pair on @p child defined by @p type and @p rid.
 *
 * @param dev The bus device.
 * @param child The bhnd child.
 * @param type The resource type.
 * @param rid The resource ID.
 * @param[out] port_type The port's type.
 * @param[out] port The port identifier.
 * @param[out] region The identifier of the memory region on @p port.
 * 
 * @retval 0 success
 * @retval non-zero No matching type/rid found.
 */
METHOD int decode_port_rid {
	device_t dev;
	device_t child;
	int type;
	int rid;
	bhnd_port_type *port_type;
	u_int *port;
	u_int *region;
} DEFAULT bhnd_bus_null_decode_port_rid;

/**
 * Get the address and size of @p region on @p port.
 *
 * @param dev The bus device.
 * @param child The bhnd child.
 * @param port_type The port type.
 * @param port The port identifier.
 * @param region The identifier of the memory region on @p port.
 * @param[out] region_addr The region's base address.
 * @param[out] region_size The region's size.
 *
 * @retval 0 success
 * @retval non-zero No matching port/region found.
 */
METHOD int get_region_addr {
	device_t dev;
	device_t child;
	bhnd_port_type port_type;
	u_int port;
	u_int region;
	bhnd_addr_t *region_addr;
	bhnd_size_t *region_size;
} DEFAULT bhnd_bus_null_get_region_addr;

/**
 * Read an NVRAM variable.
 * 
 * It is the responsibility of the bus to delegate this request to
 * the appropriate NVRAM child device, or to a parent bus implementation.
 *
 * @param		dev	The bus device.
 * @param		child	The requesting device.
 * @param		name	The NVRAM variable name.
 * @param[out]		buf	On success, the requested value will be written
 *				to this buffer. This argment may be NULL if
 *				the value is not desired.
 * @param[in,out]	size	The capacity of @p buf. On success, will be set
 *				to the actual size of the requested value.
 * @param		type	The data type to be written to @p buf.
 *
 * @retval 0		success
 * @retval ENOENT	The requested variable was not found.
 * @retval ENOMEM	If @p buf is non-NULL and a buffer of @p size is too
 *			small to hold the requested value.
 * @retval ENODEV	No valid NVRAM source could be found.
 * @retval EFTYPE	If the @p name's data type cannot be coerced to @p type.
 * @retval ERANGE	If value coercion would overflow @p type.
 * @retval non-zero	If reading @p name otherwise fails, a regular unix
 *			error code will be returned.
 */
METHOD int get_nvram_var {
	device_t	 dev;
	device_t	 child;
	const char	*name;
	void		*buf;
	size_t		*size;
	bhnd_nvram_type	 type;
} DEFAULT bhnd_bus_null_get_nvram_var;


/** An implementation of bus_read_1() compatible with bhnd_resource */
METHOD uint8_t read_1 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
}

/** An implementation of bus_read_2() compatible with bhnd_resource */
METHOD uint16_t read_2 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
}

/** An implementation of bus_read_4() compatible with bhnd_resource */
METHOD uint32_t read_4 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
}

/** An implementation of bus_write_1() compatible with bhnd_resource */
METHOD void write_1 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint8_t value;
}

/** An implementation of bus_write_2() compatible with bhnd_resource */
METHOD void write_2 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint16_t value;
}

/** An implementation of bus_write_4() compatible with bhnd_resource */
METHOD void write_4 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint32_t value;
}

/** An implementation of bus_read_stream_1() compatible with bhnd_resource */
METHOD uint8_t read_stream_1 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
}

/** An implementation of bus_read_stream_2() compatible with bhnd_resource */
METHOD uint16_t read_stream_2 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
}

/** An implementation of bus_read_stream_4() compatible with bhnd_resource */
METHOD uint32_t read_stream_4 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
}

/** An implementation of bus_write_stream_1() compatible with bhnd_resource */
METHOD void write_stream_1 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint8_t value;
}

/** An implementation of bus_write_stream_2() compatible with bhnd_resource */
METHOD void write_stream_2 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint16_t value;
}

/** An implementation of bus_write_stream_4() compatible with bhnd_resource */
METHOD void write_stream_4 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint32_t value;
}

/** An implementation of bus_read_multi_1() compatible with bhnd_resource */
METHOD void read_multi_1 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint8_t *datap;
	bus_size_t count;
}

/** An implementation of bus_read_multi_2() compatible with bhnd_resource */
METHOD void read_multi_2 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint16_t *datap;
	bus_size_t count;
}

/** An implementation of bus_read_multi_4() compatible with bhnd_resource */
METHOD void read_multi_4 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint32_t *datap;
	bus_size_t count;
}

/** An implementation of bus_write_multi_1() compatible with bhnd_resource */
METHOD void write_multi_1 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint8_t *datap;
	bus_size_t count;
}

/** An implementation of bus_write_multi_2() compatible with bhnd_resource */
METHOD void write_multi_2 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint16_t *datap;
	bus_size_t count;
}

/** An implementation of bus_write_multi_4() compatible with bhnd_resource */
METHOD void write_multi_4 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint32_t *datap;
	bus_size_t count;
}

/** An implementation of bus_read_multi_stream_1() compatible
 *  bhnd_resource */
METHOD void read_multi_stream_1 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint8_t *datap;
	bus_size_t count;
}

/** An implementation of bus_read_multi_stream_2() compatible
 *  bhnd_resource */
METHOD void read_multi_stream_2 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint16_t *datap;
	bus_size_t count;
}

/** An implementation of bus_read_multi_stream_4() compatible
 *  bhnd_resource */
METHOD void read_multi_stream_4 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint32_t *datap;
	bus_size_t count;
}

/** An implementation of bus_write_multi_stream_1() compatible
 *  bhnd_resource */
METHOD void write_multi_stream_1 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint8_t *datap;
	bus_size_t count;
}

/** An implementation of bus_write_multi_stream_2() compatible with
 *  bhnd_resource */
METHOD void write_multi_stream_2 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint16_t *datap;
	bus_size_t count;
}

/** An implementation of bus_write_multi_stream_4() compatible with
 *  bhnd_resource */
METHOD void write_multi_stream_4 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint32_t *datap;
	bus_size_t count;
}

/** An implementation of bus_set_multi_1() compatible with bhnd_resource */
METHOD void set_multi_1 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint8_t value;
	bus_size_t count;
}

/** An implementation of bus_set_multi_2() compatible with bhnd_resource */
METHOD void set_multi_2 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint16_t value;
	bus_size_t count;
}

/** An implementation of bus_set_multi_4() compatible with bhnd_resource */
METHOD void set_multi_4 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint32_t value;
	bus_size_t count;
}

/** An implementation of bus_set_region_1() compatible with bhnd_resource */
METHOD void set_region_1 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint8_t value;
	bus_size_t count;
}

/** An implementation of bus_set_region_2() compatible with bhnd_resource */
METHOD void set_region_2 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint16_t value;
	bus_size_t count;
}

/** An implementation of bus_set_region_4() compatible with bhnd_resource */
METHOD void set_region_4 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint32_t value;
	bus_size_t count;
}

/** An implementation of bus_read_region_1() compatible with bhnd_resource */
METHOD void read_region_1 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint8_t *datap;
	bus_size_t count;
}

/** An implementation of bus_read_region_2() compatible with bhnd_resource */
METHOD void read_region_2 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint16_t *datap;
	bus_size_t count;
}

/** An implementation of bus_read_region_4() compatible with bhnd_resource */
METHOD void read_region_4 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint32_t *datap;
	bus_size_t count;
}

/** An implementation of bus_read_region_stream_1() compatible with
  * bhnd_resource */
METHOD void read_region_stream_1 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint8_t *datap;
	bus_size_t count;
}

/** An implementation of bus_read_region_stream_2() compatible with
  * bhnd_resource */
METHOD void read_region_stream_2 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint16_t *datap;
	bus_size_t count;
}

/** An implementation of bus_read_region_stream_4() compatible with
  * bhnd_resource */
METHOD void read_region_stream_4 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint32_t *datap;
	bus_size_t count;
}

/** An implementation of bus_write_region_1() compatible with bhnd_resource */
METHOD void write_region_1 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint8_t *datap;
	bus_size_t count;
}

/** An implementation of bus_write_region_2() compatible with bhnd_resource */
METHOD void write_region_2 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint16_t *datap;
	bus_size_t count;
}

/** An implementation of bus_write_region_4() compatible with bhnd_resource */
METHOD void write_region_4 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint32_t *datap;
	bus_size_t count;
}

/** An implementation of bus_write_region_stream_1() compatible with
  * bhnd_resource */
METHOD void write_region_stream_1 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint8_t *datap;
	bus_size_t count;
}

/** An implementation of bus_write_region_stream_2() compatible with
  * bhnd_resource */
METHOD void write_region_stream_2 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint16_t *datap;
	bus_size_t count;
}

/** An implementation of bus_write_region_stream_4() compatible with
  * bhnd_resource */
METHOD void write_region_stream_4 {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	uint32_t *datap;
	bus_size_t count;
}

/** An implementation of bus_barrier() compatible with bhnd_resource */
METHOD void barrier {
	device_t dev;
	device_t child;
	struct bhnd_resource *r;
	bus_size_t offset;
	bus_size_t length;
	int flags;
}
