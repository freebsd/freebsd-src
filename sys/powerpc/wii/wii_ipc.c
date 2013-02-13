/*-
 * Copyright (C) 2012 Margarida Gouveia
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/platform.h>
#include <machine/intr_machdep.h>
#include <machine/resource.h>

#include <powerpc/wii/wii_ipcreg.h>

/*
 * Driver to interface with the Wii's IOS. IOS are small "microkernels" that run
 * on the Broadway GPU and provide access to system services like USB.
 */
static int	wiiipc_probe(device_t);
static int	wiiipc_attach(device_t);

struct wiiipc_softc {
};

static device_method_t wiiipc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		wiiipc_probe),
	DEVMETHOD(device_attach,	wiiipc_attach),

	DEVMETHOD_END
};

static driver_t wiiipc_driver = {
	"wiiipc",
	wiiipc_methods,
	sizeof(struct wiiipc_softc)
};

static devclass_t wiiipc_devclass;

DRIVER_MODULE(wiiipc, wiibus, wiiipc_driver, wiiipc_devclass, 0, 0);

static int
wiiipc_probe(device_t dev)
{
        device_set_desc(dev, "Nintendo Wii IOS IPC");

        return (BUS_PROBE_NOWILDCARD);
}

static int
wiiipc_attach(device_t dev)
{
	struct wiiipc_softc *sc;

	sc = device_get_softc(dev);
#ifdef notyet
	sc->sc_dev = dev;

	sc->sc_rrid = 0;
	sc->sc_rres = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->sc_rrid, RF_ACTIVE);
	if (sc->sc_rres == NULL) {
		device_printf(dev, "could not alloc mem resource\n");
		return (ENXIO);
	}
	sc->sc_bt = rman_get_bustag(sc->sc_rres);
	sc->sc_bh = rman_get_bushandle(sc->sc_rres);
#endif

	return (0);
}
