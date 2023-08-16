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

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>

#include <arm64/coresight/coresight.h>
#include <arm64/coresight/coresight_funnel.h>

#include "coresight_if.h"

#define	FUNNEL_DEBUG
#undef FUNNEL_DEBUG
        
#ifdef FUNNEL_DEBUG
#define	dprintf(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define	dprintf(fmt, ...)
#endif

static struct resource_spec funnel_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

static int
funnel_init(device_t dev)
{
	struct funnel_softc *sc;

	sc = device_get_softc(dev);
	if (sc->hwtype == HWTYPE_STATIC_FUNNEL)
		return (0);

	/* Unlock Coresight */
	bus_write_4(sc->res, CORESIGHT_LAR, CORESIGHT_UNLOCK);
	dprintf("Device ID: %x\n", bus_read_4(sc->res, FUNNEL_DEVICEID));

	return (0);
}

static int
funnel_enable(device_t dev, struct endpoint *endp,
    struct coresight_event *event)
{
	struct funnel_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);
	if (sc->hwtype == HWTYPE_STATIC_FUNNEL)
		return (0);

	reg = bus_read_4(sc->res, FUNNEL_FUNCTL);
	reg &= ~(FUNCTL_HOLDTIME_MASK);
	reg |= (7 << FUNCTL_HOLDTIME_SHIFT);
	reg |= (1 << endp->reg);
	bus_write_4(sc->res, FUNNEL_FUNCTL, reg);

	return (0);
}

static void
funnel_disable(device_t dev, struct endpoint *endp,
    struct coresight_event *event)
{
	struct funnel_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);
	if (sc->hwtype == HWTYPE_STATIC_FUNNEL)
		return;

	reg = bus_read_4(sc->res, FUNNEL_FUNCTL);
	reg &= ~(1 << endp->reg);
	bus_write_4(sc->res, FUNNEL_FUNCTL, reg);
}

int
funnel_attach(device_t dev)
{
	struct coresight_desc desc;
	struct funnel_softc *sc;

	sc = device_get_softc(dev);
	if (sc->hwtype == HWTYPE_FUNNEL &&
	    bus_alloc_resources(dev, funnel_spec, &sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		return (ENXIO);
	}

	desc.pdata = sc->pdata;
	desc.dev = dev;
	desc.dev_type = CORESIGHT_FUNNEL;
	coresight_register(&desc);

	return (0);
}

static device_method_t funnel_methods[] = {
	/* Coresight interface */
	DEVMETHOD(coresight_init,	funnel_init),
	DEVMETHOD(coresight_enable,	funnel_enable),
	DEVMETHOD(coresight_disable,	funnel_disable),
	DEVMETHOD_END
};

DEFINE_CLASS_0(funnel, funnel_driver, funnel_methods,
    sizeof(struct funnel_softc));
