/*-
 * Copyright (c) 2018-2020 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * This software was developed by BAE Systems, the University of Cambridge
 * Computer Laboratory, and Memorial University under DARPA/AFRL contract
 * FA8650-15-C-7558 ("CADETS"), as part of the DARPA Transparent Computing
 * (TC) research program.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>

#include <arm64/coresight/coresight.h>
#include <arm64/coresight/coresight_replicator.h>

#include "coresight_if.h"

static struct resource_spec replicator_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

static int
replicator_init(device_t dev)
{
	struct replicator_softc *sc;

	sc = device_get_softc(dev);

	/* Unlock Coresight */
	bus_write_4(sc->res, CORESIGHT_LAR, CORESIGHT_UNLOCK);

	return (0);
}

static int
replicator_enable(device_t dev, struct endpoint *endp,
    struct coresight_event *event)
{
	struct replicator_softc *sc;

	sc = device_get_softc(dev);

	/* Enable the port. Keep the other port disabled */
	if (endp->reg == 0) {
		bus_write_4(sc->res, REPLICATOR_IDFILTER0, 0x00);
		bus_write_4(sc->res, REPLICATOR_IDFILTER1, 0xff);
	} else {
		bus_write_4(sc->res, REPLICATOR_IDFILTER0, 0xff);
		bus_write_4(sc->res, REPLICATOR_IDFILTER1, 0x00);
	}

	return (0);
}

static void
replicator_disable(device_t dev, struct endpoint *endp,
    struct coresight_event *event)
{
	struct replicator_softc *sc;

	sc = device_get_softc(dev);

	bus_write_4(sc->res, REPLICATOR_IDFILTER0, 0xff);
	bus_write_4(sc->res, REPLICATOR_IDFILTER1, 0xff);
}

int
replicator_attach(device_t dev)
{
	struct replicator_softc *sc;
	struct coresight_desc desc;

	sc = device_get_softc(dev);

	if (bus_alloc_resources(dev, replicator_spec, &sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		return (ENXIO);
	}

	desc.pdata = sc->pdata;
	desc.dev = dev;
	desc.dev_type = CORESIGHT_DYNAMIC_REPLICATOR;
	coresight_register(&desc);

	return (0);
}

static device_method_t replicator_methods[] = {
	/* Coresight interface */
	DEVMETHOD(coresight_init,	replicator_init),
	DEVMETHOD(coresight_enable,	replicator_enable),
	DEVMETHOD(coresight_disable,	replicator_disable),
	DEVMETHOD_END
};

DEFINE_CLASS_0(replicator, replicator_driver, replicator_methods,
    sizeof(struct replicator_softc));
