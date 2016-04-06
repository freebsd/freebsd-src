/*-
 * Copyright (c) 2016 Jared McNeill <jmcneill@invisible.ca>
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
 *
 * $FreeBSD$
 */

/*
 * Allwinner USB PHY
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/gpio.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>
#include <dev/extres/regulator/regulator.h>

#define	USBPHY_NUMOFF		3

static struct ofw_compat_data compat_data[] = {
	{ "allwinner,sun4i-a10-usb-phy",	1 },
	{ "allwinner,sun5i-a13-usb-phy",	1 },
	{ "allwinner,sun6i-a31-usb-phy",	1 },
	{ "allwinner,sun7i-a20-usb-phy",	1 },
	{ NULL,					0 }
};

static int
awusbphy_init(device_t dev)
{
	char pname[20];
	int error, off;
	regulator_t reg;
	hwreset_t rst;
	clk_t clk;

	/* Enable clocks */
	for (off = 0; clk_get_by_ofw_index(dev, off, &clk) == 0; off++) {
		error = clk_enable(clk);
		if (error != 0) {
			device_printf(dev, "couldn't enable clock %s\n",
			    clk_get_name(clk));
			return (error);
		}
	}

	/* De-assert resets */
	for (off = 0; hwreset_get_by_ofw_idx(dev, off, &rst) == 0; off++) {
		error = hwreset_deassert(rst);
		if (error != 0) {
			device_printf(dev, "couldn't de-assert reset %d\n",
			    off);
			return (error);
		}
	}

	/* Enable regulator(s) */
	for (off = 0; off < USBPHY_NUMOFF; off++) {
		snprintf(pname, sizeof(pname), "usb%d_vbus-supply", off);
		if (regulator_get_by_ofw_property(dev, pname, &reg) != 0)
			continue;
		error = regulator_enable(reg);
		if (error != 0) {
			device_printf(dev, "couldn't enable regulator %s\n",
			    pname);
			return (error);
		}
	}

	return (0);
}

static int
awusbphy_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Allwinner USB PHY");
	return (BUS_PROBE_DEFAULT);
}

static int
awusbphy_attach(device_t dev)
{
	int error;

	error = awusbphy_init(dev);
	if (error)
		device_printf(dev, "failed to initialize USB PHY, error %d\n",
		    error);

	return (error);
}

static device_method_t awusbphy_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		awusbphy_probe),
	DEVMETHOD(device_attach,	awusbphy_attach),

	DEVMETHOD_END
};

static driver_t awusbphy_driver = {
	"awusbphy",
	awusbphy_methods,
	0,
};

static devclass_t awusbphy_devclass;

DRIVER_MODULE(awusbphy, simplebus, awusbphy_driver, awusbphy_devclass, 0, 0);
MODULE_VERSION(awusbphy, 1);
