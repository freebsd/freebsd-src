/*-
 * Copyright (c) 2012-2013 Robert N. M. Watson
 * Copyright (c) 2014 Ed Maste
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
#include <sys/bus.h>
#include <sys/fbio.h>				/* video_adapter_t */
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/terasic/pixelstream/pixelstream.h>

#include "fb_if.h"

static int
pixelstream_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "sri-cambridge,pixelstream")) {
		device_set_desc(dev, "Pixelstream HDMI");
		return (BUS_PROBE_DEFAULT);
	}
        return (ENXIO);
}

static int
pixelstream_fdt_attach(device_t dev)
{
	struct pixelstream_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->ps_dev = dev;
	sc->ps_unit = device_get_unit(dev);

	/*
	 * Pixelstream's FDT entry has a single memory resource for its control
	 * registers.
	 */
	sc->ps_reg_rid = 0;
	sc->ps_reg_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->ps_reg_rid, RF_ACTIVE);
	if (sc->ps_reg_res == NULL) {
		device_printf(dev, "couldn't map register memory\n");
		error = ENXIO;
		goto error;
	}
	if (rman_get_start(sc->ps_reg_res) % PAGE_SIZE != 0) {
		device_printf(dev, "improper register address\n");
		error = ENXIO;
		goto error;
	}
	device_printf(sc->ps_dev, "registers at mem %p-%p\n",
            (void *)rman_get_start(sc->ps_reg_res),
	    (void *)(rman_get_start(sc->ps_reg_res) +
	      rman_get_size(sc->ps_reg_res)));

	error = pixelstream_attach(sc);
	if (error == 0)
		return (0);
error:
	if (sc->ps_reg_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->ps_reg_rid,
		    sc->ps_reg_res);
	return (error);
}

static int
pixelstream_fdt_detach(device_t dev)
{
	struct pixelstream_softc *sc;

	sc = device_get_softc(dev);
	pixelstream_detach(sc);
	bus_release_resource(dev, SYS_RES_MEMORY, sc->ps_reg_rid,
	    sc->ps_reg_res);
	return (0);
}

static struct fb_info *
pixelstream_fb_getinfo(device_t dev)
{
        struct pixelstream_softc *sc;

        sc = device_get_softc(dev);
        return (&sc->ps_fb_info);
}

static device_method_t pixelstream_fdt_methods[] = {
	DEVMETHOD(device_probe,		pixelstream_fdt_probe),
	DEVMETHOD(device_attach,	pixelstream_fdt_attach),
	DEVMETHOD(device_detach,	pixelstream_fdt_detach),
	DEVMETHOD(fb_getinfo,		pixelstream_fb_getinfo),
	{ 0, 0 }
};

static driver_t pixelstream_fdt_driver = {
	"pixelstream",
	pixelstream_fdt_methods,
	sizeof(struct pixelstream_softc),
};

DRIVER_MODULE(pixelstream, simplebus, pixelstream_fdt_driver,
    pixelstream_devclass, 0, 0);
