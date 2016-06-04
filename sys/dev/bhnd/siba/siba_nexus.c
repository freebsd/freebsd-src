/*-
 * Copyright (c) 2015-2016 Landon Fuller <landon@freebsd.org>
 * Copyright (c) 2007 Bruce M. Simpson.
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/bhnd/bhnd_ids.h>
#include <dev/bhnd/bhnd_nexusvar.h>
#include <dev/bhnd/cores/chipc/chipcreg.h>

#include "sibavar.h"

/*
 * Supports siba(4) attachment to a MIPS nexus bus.
 * 
 * Derived from Bruce M. Simpson' original siba(4) driver.
 */

struct siba_nexus_softc {
	struct siba_softc		parent_sc;
	struct bhnd_chipid		siba_cid;
};

static int
siba_nexus_probe(device_t dev)
{
	struct siba_nexus_softc	*sc;
	int			 error;

	sc = device_get_softc(dev);

	/* Read the ChipCommon info using the hints the kernel
	 * was compiled with. */
	if ((error = bhnd_nexus_read_chipid(dev, &sc->siba_cid)))
		return (error);

	if (sc->siba_cid.chip_type != BHND_CHIPTYPE_SIBA)
		return (ENXIO);

	if ((error = siba_probe(dev)) > 0) {
		device_printf(dev, "error %d in probe\n", error);
		return (error);
	}

	return (0);
}

static int
siba_nexus_attach(device_t dev)
{
	struct siba_nexus_softc	*sc;
	int error;

	sc = device_get_softc(dev);

	/* Enumerate the bus. */
	if ((error = siba_add_children(dev, NULL))) {
		device_printf(dev, "error %d enumerating children\n", error);
		return (error);
	}

	return (siba_attach(dev));
}

static const struct bhnd_chipid *
siba_nexus_get_chipid(device_t dev, device_t child) {
	struct siba_nexus_softc	*sc = device_get_softc(dev);
	return (&sc->siba_cid);
}

static device_method_t siba_nexus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			siba_nexus_probe),
	DEVMETHOD(device_attach,		siba_nexus_attach),

	/* bhnd interface */
	DEVMETHOD(bhnd_bus_get_chipid,		siba_nexus_get_chipid),

	DEVMETHOD_END
};

DEFINE_CLASS_2(bhnd, siba_nexus_driver, siba_nexus_methods,
    sizeof(struct siba_nexus_softc), bhnd_nexus_driver, siba_driver);

DRIVER_MODULE(siba_nexus, nexus, siba_nexus_driver, bhnd_devclass, 0, 0);
