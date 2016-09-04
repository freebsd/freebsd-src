#-
# Copyright (c) 2016 Landon Fuller <landon@landonf.org>
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

#include <sys/param.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/bhnd/bhnd.h>
#include <dev/bhnd/bhnd_erom_types.h>

INTERFACE bhnd_erom;

#
# bhnd(4) device enumeration.
#
# Provides a common parser interface to the incompatible device enumeration
# tables used by bhnd(4) buses.
#

/**
 * Probe to see if this device enumeration class supports the bhnd bus
 * mapped by the given resource, returning a standard newbus device probe
 * result (see BUS_PROBE_*) and the probed chip identification.
 *
 * @param	cls	The erom class to probe.
 * @param	res	A resource mapping the first bus core.
 * @param	offset	Offset to the first bus core within @p res.
 * @param	hint	Hint used to identify the device. If chipset supports
 *			standard chip identification registers within the first 
 *			core, this parameter should be NULL.
 * @param[out]	cid	On success, the probed chip identifier.
 *
 * @retval 0		if this is the only possible device enumeration
 *			parser for the probed bus.
 * @retval negative	if the probe succeeds, a negative value should be
 *			returned; the parser returning the highest negative
 *			value will be selected to handle device enumeration.
 * @retval ENXIO	If the bhnd bus type is not handled by this parser.
 * @retval positive	if an error occurs during probing, a regular unix error
 *			code should be returned.
 */
STATICMETHOD int probe {
	bhnd_erom_class_t		*cls;
	struct bhnd_resource		*res;
	bus_size_t			 offset;
	const struct bhnd_chipid	*hint;
	struct bhnd_chipid		*cid;
};

/**
 * Probe to see if this device enumeration class supports the bhnd bus
 * mapped at the given bus space tag and handle, returning a standard
 * newbus device probe result (see BUS_PROBE_*) and the probed
 * chip identification.
 *
 * @param	cls	The erom class to probe.
 * @param	bst	Bus space tag.
 * @param	bsh	Bus space handle mapping the first bus core.
 * @param	paddr	The physical address of the core mapped by @p bst and
 *			@p bsh.
 * @param	hint	Hint used to identify the device. If chipset supports
 *			standard chip identification registers within the first 
 *			core, this parameter should be NULL.
 * @param[out]	cid	On success, the probed chip identifier.
 *
 * @retval 0		if this is the only possible device enumeration
 *			parser for the probed bus.
 * @retval negative	if the probe succeeds, a negative value should be
 *			returned; the parser returning the highest negative
 *			value will be selected to handle device enumeration.
 * @retval ENXIO	If the bhnd bus type is not handled by this parser.
 * @retval positive	if an error occurs during probing, a regular unix error
 *			code should be returned.
 */
STATICMETHOD int probe_static {
	bhnd_erom_class_t		*cls;
	bus_space_tag_t 		 bst;
	bus_space_handle_t		 bsh;
	bus_addr_t			 paddr;
	const struct bhnd_chipid	*hint;
	struct bhnd_chipid		*cid;
};

/**
 * Initialize a device enumeration table parser.
 * 
 * @param erom		The erom parser to initialize.
 * @param cid		The device's chip identifier.
 * @param parent	The parent device from which EROM resources should
 *			be allocated.
 * @param rid		The resource id to be used when allocating the
 *			enumeration table.
 *
 * @retval 0		success
 * @retval non-zero	if an error occurs initializing the EROM parser,
 *			a regular unix error code will be returned.
 */
METHOD int init {
	bhnd_erom_t			*erom;
	const struct bhnd_chipid	*cid;
	device_t			 parent;
	int				 rid;
};

/**
 * Initialize an device enumeration table parser using the provided bus space
 * tag and handle.
 * 
 * @param erom	The erom parser to initialize.
 * @param cid	The device's chip identifier.
 * @param bst	Bus space tag.
 * @param bsh	Bus space handle mapping the full bus enumeration
 *		space.
 *
 * @retval 0		success
 * @retval non-zero	if an error occurs initializing the EROM parser,
 *			a regular unix error code will be returned.
 */
METHOD int init_static {
	bhnd_erom_t			*erom;
	const struct bhnd_chipid	*cid;
	bus_space_tag_t 		 bst;
	bus_space_handle_t		 bsh;
};

/**
 * Release all resources held by @p erom.
 * 
 * @param	erom	An erom parser instance previously initialized via
 *			BHND_EROM_INIT() or BHND_EROM_INIT_STATIC().
 */
METHOD void fini {
	bhnd_erom_t	*erom;
};

/**
 * Parse all cores descriptors, returning the array in @p cores and the count
 * in @p num_cores.
 * 
 * The memory allocated for the table must be freed via
 * BHND_EROM_FREE_CORE_TABLE().
 * 
 * @param	erom		The erom parser to be queried.
 * @param[out]	cores		The table of parsed core descriptors.
 * @param[out]	num_cores	The number of core records in @p cores.
 * 
 * @retval 0		success
 * @retval non-zero	if an error occurs, a regular unix error code will
 *			be returned.
 */
METHOD int get_core_table {
	bhnd_erom_t		*erom;
	struct bhnd_core_info	**cores;
	u_int			*num_cores;
};

/**
 * Free any memory allocated in a previous call to BHND_EROM_GET_CORE_TABLE().
 *
 * @param	erom		The erom parser instance.
 * @param	cores		A core table allocated by @p erom. 
 */
METHOD void free_core_table {
	bhnd_erom_t		*erom;
	struct bhnd_core_info	*cores;
};

/**
 * Locate the first core table entry in @p erom that matches @p desc.
 *
 * @param	erom	The erom parser to be queried.
 * @param	desc	A core match descriptor.
 * @param[out]	core	On success, the matching core info record.
 * 
 * @retval 0		success
 * @retval ENOENT	No core matching @p desc was found.
 * @retval non-zero	Reading or parsing failed.
 */
METHOD int lookup_core {
	bhnd_erom_t			*erom;
	const struct bhnd_core_match	*desc;
	struct bhnd_core_info		*core;
};

/**
 * Locate the first core table entry in @p erom that matches @p desc,
 * and return the specified port region's base address and size.
 *
 * If a core matching @p desc is not found, or the requested port region
 * is not mapped to the matching core, ENOENT is returned.
 *
 * @param	erom	The erom parser to be queried.
 * @param	desc	A core match descriptor.
 * @param	type	The port type to search for.
 * @param	port	The port to search for.
 * @param	region	The port region to search for.
 * @param[out]	core	If not NULL, will be populated with the matched core
 *			info record on success.
 * @param[out]	addr	On success, the base address of the port region.
 * @param[out]	size	On success, the total size of the port region.
 *
 * @retval 0		success
 * @retval ENOENT	No core matching @p desc was found.
 * @retval ENOENT	No port region matching @p type, @p port, and @p region
 *			was found.
 * @retval non-zero	Reading or parsing failed.
 */
METHOD int lookup_core_addr {
	bhnd_erom_t			*erom;
	const struct bhnd_core_match	*desc;
	bhnd_port_type			 type;
	u_int				 port;
	u_int				 region;
	struct bhnd_core_info		*core;
	bhnd_addr_t			*addr;
	bhnd_size_t			*size;
};
