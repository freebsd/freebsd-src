/*-
 * Copyright (c) 2015 Oleksandr Tymoshenko <gonzo@freebsd.org>
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
 * HDMI core module
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

#include <dev/videomode/videomode.h>

#include <arm/freescale/imx/imx_ccmvar.h>
#include <arm/freescale/imx/imx_iomuxvar.h>
#include <arm/freescale/imx/imx_iomuxreg.h>

#include <dev/hdmi/dwc_hdmi.h>

#include "hdmi_if.h"

struct imx_hdmi_softc {
	struct dwc_hdmi_softc	base;
	phandle_t		i2c_xref;
};

static struct ofw_compat_data compat_data[] = {
	{"fsl,imx6dl-hdmi", 1},
	{"fsl,imx6q-hdmi",  1},
	{NULL,	            0}
};

static device_t
imx_hdmi_get_i2c_dev(device_t dev)
{
	struct imx_hdmi_softc *sc;

	sc = device_get_softc(dev);

	if (sc->i2c_xref == 0)
		return (NULL);

	return (OF_device_from_xref(sc->i2c_xref));
}

static int
imx_hdmi_detach(device_t dev)
{
	struct imx_hdmi_softc *sc;

	sc = device_get_softc(dev);

	if (sc->base.sc_mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->base.sc_mem_rid, sc->base.sc_mem_res);

	return (0);
}

static int
imx_hdmi_attach(device_t dev)
{
	struct imx_hdmi_softc *sc;
	int err;
	uint32_t gpr3;
	phandle_t node, i2c_xref;

	sc = device_get_softc(dev);
	sc->base.sc_dev = dev;
	sc->base.sc_get_i2c_dev = imx_hdmi_get_i2c_dev;
	err = 0;

	/* Allocate memory resources. */
	sc->base.sc_mem_rid = 0;
	sc->base.sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->base.sc_mem_rid, RF_ACTIVE);
	if (sc->base.sc_mem_res == NULL) {
		device_printf(dev, "Cannot allocate memory resources\n");
		err = ENXIO;
		goto out;
	}

	node = ofw_bus_get_node(dev);
	if (OF_getencprop(node, "ddc-i2c-bus", &i2c_xref, sizeof(i2c_xref)) == -1)
		sc->i2c_xref = 0;
	else
		sc->i2c_xref = i2c_xref;

	imx_ccm_hdmi_enable();

	gpr3 = imx_iomux_gpr_get(IOMUXC_GPR3);
	gpr3 &= ~(IOMUXC_GPR3_HDMI_MASK);
	gpr3 |= IOMUXC_GPR3_HDMI_IPU1_DI0;
	imx_iomux_gpr_set(IOMUXC_GPR3, gpr3);

	return (dwc_hdmi_init(dev));

out:
	imx_hdmi_detach(dev);

	return (err);
}

static int
imx_hdmi_probe(device_t dev)
{

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Freescale i.MX6 HDMI core");

	return (BUS_PROBE_DEFAULT);
}

static device_method_t imx_hdmi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,  imx_hdmi_probe),
	DEVMETHOD(device_attach, imx_hdmi_attach),
	DEVMETHOD(device_detach, imx_hdmi_detach),

	/* HDMI methods */
	DEVMETHOD(hdmi_get_edid,	dwc_hdmi_get_edid),
	DEVMETHOD(hdmi_set_videomode,	dwc_hdmi_set_videomode),

	DEVMETHOD_END
};

static driver_t imx_hdmi_driver = {
	"hdmi",
	imx_hdmi_methods,
	sizeof(struct imx_hdmi_softc)
};

static devclass_t imx_hdmi_devclass;

DRIVER_MODULE(hdmi, simplebus, imx_hdmi_driver, imx_hdmi_devclass, 0, 0);
