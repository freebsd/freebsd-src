/*-
 * Copyright (c) 2016 Landon Fuller <landonf@FreeBSD.org>
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
#include <sys/kobj.h>

#include <dev/bhnd/bhndvar.h>
#include <dev/bhnd/bhnd_erom.h>

/**
 * Allocate and return a new device enumeration table parser.
 * 
 * @param cls		The parser class for which an instance will be
 *			allocated.
 * @param parent	The parent device from which EROM resources should
 *			be allocated.
 * @param rid		The resource ID to be used when allocating EROM
 *			resources.
 * @param cid		The device's chip identifier.
 *
 * @retval non-NULL	success
 * @retval NULL		if an error occured allocating or initializing the
 *			EROM parser.
 */
bhnd_erom_t *
bhnd_erom_alloc(bhnd_erom_class_t *cls, const struct bhnd_chipid *cid,
    device_t parent, int rid)
{
	bhnd_erom_t	*erom;
	int		 error;

	erom = (bhnd_erom_t *)kobj_create((kobj_class_t)cls, M_BHND,
	    M_WAITOK|M_ZERO);

	if ((error = BHND_EROM_INIT(erom, cid, parent, rid))) {
		printf("error initializing %s parser at %#jx with "
		    "rid %d: %d\n", cls->name, (uintmax_t)cid->enum_addr, rid,
		     error);

		kobj_delete((kobj_t)erom, M_BHND);
		return (NULL);
	}

	return (erom);
}

/**
 * Perform static initialization of aa device enumeration table parser using
 * the provided bus space tag and handle.
 * 
 * This may be used to initialize a caller-allocated erom instance state
 * during early boot, prior to malloc availability.
 * 
 * @param cls		The parser class for which an instance will be
 *			allocated.
 * @param erom		The erom parser instance to initialize.
 * @param esize		The total available number of bytes allocated for
 *			@p erom. If this is less than is required by @p cls,
 *			ENOMEM will be returned.
 * @param cid		The device's chip identifier.
 * @param bst		Bus space tag.
 * @param bsh		Bus space handle mapping the device enumeration
 *			space.
 *
 * @retval 0		success
 * @retval ENOMEM	if @p esize is smaller than required by @p cls.
 * @retval non-zero	if an error occurs initializing the EROM parser,
 *			a regular unix error code will be returned.
 */
int
bhnd_erom_init_static(bhnd_erom_class_t *cls, bhnd_erom_t *erom, size_t esize,
    const struct bhnd_chipid *cid, bus_space_tag_t bst, bus_space_handle_t bsh)
{
	kobj_class_t	kcls;

	kcls = (kobj_class_t)cls;

	/* Verify allocation size */
	if (kcls->size > esize)
		return (ENOMEM);

	/* Perform instance initialization */
	kobj_init_static((kobj_t)erom, kcls);
	return (BHND_EROM_INIT_STATIC(erom, cid, bst, bsh)); 
}

/**
 * Release any resources held by a @p erom parser previously
 * initialized via bhnd_erom_init_static().
 * 
 * @param	erom	An erom parser instance previously initialized via
 *			bhnd_erom_init_static().
 */
void
bhnd_erom_fini_static(bhnd_erom_t *erom)
{
	return (BHND_EROM_FINI(erom));
}

/**
 * Release all resources held by a @p erom parser previously
 * allocated via bhnd_erom_alloc().
 * 
 * @param	erom	An erom parser instance previously allocated via
 *			bhnd_erom_alloc().
 */
void
bhnd_erom_free(bhnd_erom_t *erom)
{
	BHND_EROM_FINI(erom);
	kobj_delete((kobj_t)erom, M_BHND);
}
