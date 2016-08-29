/*-
 * Copyright (c) 2016 Landon Fuller <landonf@FreeBSD.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <machine/bus.h>

#include <dev/bhnd/bhnd.h>
#include <dev/bhnd/bcma/bcma_eromvar.h>

#include "bcm_machdep.h"

#define	BCMFC_ERR(fmt, ...)	printf("%s: " fmt, __FUNCTION__, ##__VA_ARGS__)

int
bcm_find_core_bcma(struct bhnd_chipid *chipid, bhnd_devclass_t devclass,
    int unit, struct bhnd_core_info *info, uintptr_t *addr)
{
	struct bcma_erom		erom;
	struct bcma_erom_core		core;
	struct bcma_erom_sport_region	region;
	bhnd_devclass_t			core_class;
	int				error;

	error = bhnd_erom_bus_space_open(&erom, NULL, mips_bus_space_generic,
	    (bus_space_handle_t) BCM_SOC_ADDR(chipid->enum_addr, 0), 0);
	if (error) {
		BCMFC_ERR("erom open failed: %d\n", error);
		return (error);
	}

	for (u_long core_index = 0; core_index < ULONG_MAX; core_index++) {
		/* Fetch next core record */
		if ((error = bcma_erom_seek_next_core(&erom)))
			return (error);

		if ((error = bcma_erom_parse_core(&erom, &core))) {
			BCMFC_ERR("core parse failed: %d\n", error);
			return (error);
		}

		/* Check for match */
		core_class = bhnd_find_core_class(core.vendor,
		    core.device);
		if (core_class != devclass)
			continue;

		/* Provide the basic core info */
		if (info != NULL)
			bcma_erom_to_core_info(&core, core_index, 0, info);

		/* Provide the core's device0.0 port address */
		error = bcma_erom_seek_core_sport_region(&erom, core_index,
		    BHND_PORT_DEVICE, 0, 0);
		if (error) {
			BCMFC_ERR("sport not found: %d\n", error);
			return (error);
		}

		if ((error = bcma_erom_parse_sport_region(&erom, &region))) {
			BCMFC_ERR("sport parse failed: %d\n", error);
			return (error);
		}

		if (addr != NULL)
			*addr = region.base_addr;

		return (0);
	}

	/* Not found */
	return (ENOENT);
}
