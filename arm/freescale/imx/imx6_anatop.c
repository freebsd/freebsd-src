/*-
 * Copyright (c) 2013 Ian Lepore <ian@freebsd.org>
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

/*
 * Analog PLL and power regulator driver for Freescale i.MX6 family of SoCs.
 *
 * We don't really do anything with analog PLLs, but the registers for
 * controlling them belong to the same block as the power regulator registers.
 * Since the newbus hierarchy makes it hard for anyone other than us to get at
 * them, we just export a couple public functions to allow the imx6 CCM clock
 * driver to read and write those registers.
 *
 * We also don't do anything about power regulation yet, but when the need
 * arises, this would be the place for that code to live.
 *
 * I have no idea where the "anatop" name comes from.  It's in the standard DTS
 * source describing i.MX6 SoCs, and in the linux and u-boot code which comes
 * from Freescale, but it's not in the SoC manual.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>

#include <arm/freescale/imx/imx6_anatopreg.h>
#include <arm/freescale/imx/imx6_anatopvar.h>

struct imx6_anatop_softc {
	device_t	dev;
	struct resource	*mem_res;
};

static struct imx6_anatop_softc *imx6_anatop_sc;

uint32_t
imx6_anatop_read_4(bus_size_t offset)
{

	return (bus_read_4(imx6_anatop_sc->mem_res, offset));
}

void
imx6_anatop_write_4(bus_size_t offset, uint32_t value)
{

	bus_write_4(imx6_anatop_sc->mem_res, offset, value);
}

static int
imx6_anatop_detach(device_t dev)
{
	struct imx6_anatop_softc *sc;

	sc = device_get_softc(dev);

	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);

	return (0);
}

static int
imx6_anatop_attach(device_t dev)
{
	struct imx6_anatop_softc *sc;
	int err, rid;

	sc = device_get_softc(dev);

	/* Allocate bus_space resources. */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Cannot allocate memory resources\n");
		err = ENXIO;
		goto out;
	}

	imx6_anatop_sc = sc;
	err = 0;

out:

	if (err != 0)
		imx6_anatop_detach(dev);

	return (err);
}

static int
imx6_anatop_probe(device_t dev)
{

        if (ofw_bus_is_compatible(dev, "fsl,imx6q-anatop") == 0)
		return (ENXIO);

	device_set_desc(dev, "Freescale i.MX6 Analog PLLs and Power");

	return (BUS_PROBE_DEFAULT);
}

static device_method_t imx6_anatop_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,  imx6_anatop_probe),
	DEVMETHOD(device_attach, imx6_anatop_attach),
	DEVMETHOD(device_detach, imx6_anatop_detach),

	DEVMETHOD_END
};

static driver_t imx6_anatop_driver = {
	"imx6_anatop",
	imx6_anatop_methods,
	sizeof(struct imx6_anatop_softc)
};

static devclass_t imx6_anatop_devclass;

DRIVER_MODULE(imx6_anatop, simplebus, imx6_anatop_driver, imx6_anatop_devclass, 0, 0);

