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
 * 
 * $FreeBSD$
 */

#ifndef _BHND_EROM_BHND_EROM_H_
#define _BHND_EROM_BHND_EROM_H_

#include <sys/param.h>
#include <sys/kobj.h>
#include <sys/linker_set.h>

#include <dev/bhnd/bhnd.h>
#include <dev/bhnd/bhnd_erom_types.h>

#include "bhnd_erom_if.h"

bhnd_erom_t			*bhnd_erom_alloc(bhnd_erom_class_t *cls,
				     device_t parent, int rid,
				     bus_addr_t enum_addr);

int				 bhnd_erom_init_static(bhnd_erom_class_t *cls,
				     bhnd_erom_t *erom, size_t esize,
				     bus_space_tag_t bst,
				     bus_space_handle_t bsh);

void				 bhnd_erom_fini_static(bhnd_erom_t *erom);

void				 bhnd_erom_free(bhnd_erom_t *erom);

/**
 * Abstract bhnd_erom instance state. Must be first member of all subclass
 * instances.
 */
struct bhnd_erom {
	KOBJ_FIELDS;
};


/** Number of additional bytes to reserve for statically allocated
 *  bhnd_erom instances. */
#define	BHND_EROM_STATIC_BYTES	64

/**
 * A bhnd_erom instance structure large enough to statically allocate
 * any known bhnd_erom subclass.
 * 
 * The maximum size of subclasses is verified statically in
 * BHND_EROM_DEFINE_CLASS(), and at runtime in bhnd_erom_init_static().
 */
struct bhnd_erom_static {
	struct bhnd_erom	obj;
	uint8_t			idata[BHND_EROM_STATIC_BYTES];
};

/** Registered EROM parser class instances. */
SET_DECLARE(bhnd_erom_class_set, bhnd_erom_class_t);

#define	BHND_EROM_DEFINE_CLASS(name, classvar, methods, size)	\
	DEFINE_CLASS_0(name, classvar, methods, size);		\
	BHND_EROM_CLASS_DEF(classvar);				\
	_Static_assert(size <= sizeof(struct bhnd_erom_static),	\
	    "cannot statically allocate instance data; "	\
	        "increase BHND_EROM_STATIC_BYTES");

#define	BHND_EROM_CLASS_DEF(classvar)	DATA_SET(bhnd_erom_class_set, classvar)


/**
 * Probe to see if this device enumeration class supports the bhnd bus
 * mapped at the given bus space tag and handle, returning a standard
 * newbus device probe result (see BUS_PROBE_*) and the probed
 * chip identification.
 *
 * @param	cls	The parser class to be probed.
 * @param	bst	Bus space tag.
 * @param	bsh	Bus space handle mapping the EXTIF or ChipCommon core.
 * @param	paddr	The physical address of the core mapped by @p bst and
 *			@p bsh.
 * @param[out]	cid	On success, the probed chip identifier.
 *
 * @retval 0		if this is the only possible device enumeration
 *			parser for the probed bus.
 * @retval negative	if the probe succeeds, a negative value should be
 *			returned; the parser returning the lowest value will
 *			be selected to handle device enumeration.
 * @retval ENXIO	If the bhnd bus type is not handled by this parser.
 * @retval positive	if an error occurs during probing, a regular unix error
 *			code should be returned.
 */
static inline int
bhnd_erom_probe_static(bhnd_erom_class_t *cls, bus_space_tag_t bst,
    bus_space_handle_t bsh, bus_addr_t paddr, struct bhnd_chipid *cid)
{
	return (BHND_EROM_PROBE_STATIC(cls, bst, bsh, paddr, cid));
}

/**
 * Parse all cores descriptors in @p erom, returning the array in @p cores and
 * the count in @p num_cores.
 * 
 * The memory allocated for the table must be freed via
 * bhnd_erom_free_core_table().
 * 
 * @param	erom		The erom parser to be queried.
 * @param[out]	cores		The table of parsed core descriptors.
 * @param[out]	num_cores	The number of core records in @p cores.
 * 
 * @retval 0		success
 * @retval non-zero	if an error occurs, a regular unix error code will
 *			be returned.
 */
static inline int
bhnd_erom_get_core_table(bhnd_erom_t *erom, struct bhnd_core_info **cores,
    u_int *num_cores)
{
	return (BHND_EROM_GET_CORE_TABLE(erom, cores, num_cores));
}

/**
 * Free any memory allocated in a previous call to BHND_EROM_GET_CORE_TABLE().
 *
 * @param	erom		The erom parser instance.
 * @param	cores		A core table allocated by @p erom. 
 */
static inline void
bhnd_erom_free_core_table(bhnd_erom_t *erom, struct bhnd_core_info *cores)
{
	return (BHND_EROM_FREE_CORE_TABLE(erom, cores));
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
static inline int
bhnd_erom_lookup_core(bhnd_erom_t *erom, const struct bhnd_core_match *desc,
    struct bhnd_core_info *core)
{
	return (BHND_EROM_LOOKUP_CORE(erom, desc, core));
}

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
static inline int
bhnd_erom_lookup_core_addr(bhnd_erom_t *erom, const struct bhnd_core_match *desc,
    bhnd_port_type type, u_int port, u_int region, struct bhnd_core_info *core,
    bhnd_addr_t *addr, bhnd_size_t *size)
{
	return (BHND_EROM_LOOKUP_CORE_ADDR(erom, desc, type, port, region,
	    core, addr, size));
};

#endif /* _BHND_EROM_BHND_EROM_H_ */
