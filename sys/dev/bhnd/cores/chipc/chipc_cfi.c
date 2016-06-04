/*-
 * Copyright (c) 2016 Michael Zhilin <mizhka@gmail.com>
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/conf.h>

#include <machine/bus.h>

#include <dev/bhnd/bhnd_debug.h>
#include <dev/cfi/cfi_var.h>

#include "bhnd_chipc_if.h"
#include "chipc_slicer.h"
#include "chipcreg.h"
#include "chipcvar.h"

/*
 * **************************** PROTOTYPES ****************************
 */

static void	chipc_cfi_identify(driver_t *driver, device_t parent);
static int	chipc_cfi_probe(device_t dev);
static int	chipc_cfi_attach(device_t dev);

/*
 * **************************** IMPLEMENTATION ************************
 */

static void
chipc_cfi_identify(driver_t *driver, device_t parent)
{
	struct chipc_caps	*caps;

	if (device_find_child(parent, cfi_driver_name, -1) != NULL)
		return;

	caps = BHND_CHIPC_GET_CAPS(parent);
	if (caps == NULL)
		return;

	if (caps->flash_type != CHIPC_PFLASH_CFI)
		return;

	BUS_ADD_CHILD(parent, 0, cfi_driver_name, -1);
	return;
}

static int
chipc_cfi_probe(device_t dev)
{
	int			error;
	int			enabled;
	int			byteswap;
	uint32_t		flash_config;
	struct cfi_softc	*sc;

	sc = device_get_softc(dev);

	flash_config = BHND_CHIPC_GET_FLASH_CFG(device_get_parent(dev));

	enabled = (flash_config & CHIPC_CF_EN);
	byteswap = (flash_config & CHIPC_CF_BS);

	if (enabled == 0)
		device_disable(dev);

	BHND_DEBUG_DEV(dev, "trying attach flash enabled=%d swapbytes=%d",
	    enabled, byteswap);

	sc->sc_width = 0;
	error = cfi_probe(dev);
	if (error == 0)
		device_set_desc(dev, "ChipCommon CFI");
	return (error);
}

static int
chipc_cfi_attach(device_t dev)
{
	int	error;

	error = cfi_attach(dev);
	if (error)
		return (error);

	flash_register_slicer(chipc_slicer_cfi);
	return (0);
}

static device_method_t chipc_cfi_methods[] = {
	/* device interface */
	DEVMETHOD(device_identify,	chipc_cfi_identify),
	DEVMETHOD(device_probe,		chipc_cfi_probe),
	DEVMETHOD(device_attach,	chipc_cfi_attach),
	DEVMETHOD(device_detach,	cfi_detach),

	{0, 0}
};

static driver_t chipc_cfi_driver = {
	cfi_driver_name,
	chipc_cfi_methods,
	sizeof(struct cfi_softc),
};

DRIVER_MODULE(cfi, bhnd_chipc, chipc_cfi_driver, cfi_devclass, 0, 0);

