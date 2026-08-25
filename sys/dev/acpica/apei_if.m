#-
#  SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 Netflix, Inc.
# Written by: John Baldwin <jhb@FreeBSD.org>
#

#include <sys/bus.h>

#include <contrib/dev/acpica/include/acpi.h>

/**
 * @defgroup APEI apei - KObj methods for ACPI platform error interface drivers
 * @brief A pair of methods to allocate/deallocate registers for APEI
 * @{
 */
INTERFACE apei;

/**
 * @brief Allocate a mapping for a resource.
 *
 * @param _bus	 parent device
 * @param _dev	 device requesting the mapping
 * @param _type	 resource type
 * @param _start start address
 * @param _count length
 *
 * @returns	resource mapping on success, or @c NULL on failure
 */
METHOD struct resource_map * map_register {
	device_t	_bus;
	device_t	_child;
	int		_type;
	rman_res_t	_start;
	rman_res_t	_count;
};

/**
 * @brief Deallocate a mapping for a resource.
 *
 * @param _bus	parent device
 * @param _dev	device releasing the mapping
 * @param _map	resource map
 */
METHOD int unmap_register {
	device_t	_bus;
	device_t	_child;
	struct resource_map *_map;
};
