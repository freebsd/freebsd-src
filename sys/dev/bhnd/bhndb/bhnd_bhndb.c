/*-
 * Copyright (c) 2015-2016 Landon Fuller <landon@landonf.org>
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
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>

#include <dev/bhnd/bhnd_ids.h>
#include <dev/bhnd/bhnd.h>

#include "bhndbvar.h"

/*
 * bhnd(4) driver mix-in providing a shared common methods for
 * bhnd devices attached via a bhndb bridge.
 */

static int
bhnd_bhndb_read_board_info(device_t dev, device_t child,
    struct bhnd_board_info *info)
{
	int	error;

	/* Initialize with NVRAM-derived values */
	if ((error = bhnd_bus_generic_read_board_info(dev, child, info)))
		return (error);

	/* Let the bridge fill in any additional data */
	return (BHNDB_POPULATE_BOARD_INFO(device_get_parent(dev), dev, info));
}

static bhnd_attach_type
bhnd_bhndb_get_attach_type(device_t dev, device_t child)
{
	/* It's safe to assume that a bridged device is always an adapter */
	return (BHND_ATTACH_ADAPTER);
}

static device_method_t bhnd_bhndb_methods[] = {
	/* BHND interface */
	DEVMETHOD(bhnd_bus_get_attach_type,	bhnd_bhndb_get_attach_type),
	DEVMETHOD(bhnd_bus_read_board_info,	bhnd_bhndb_read_board_info),

	DEVMETHOD_END
};

DEFINE_CLASS_0(bhnd, bhnd_bhndb_driver, bhnd_bhndb_methods, 0);
