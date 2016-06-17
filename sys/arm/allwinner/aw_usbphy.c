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
#include <dev/gpio/gpiobusvar.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>
#include <dev/extres/regulator/regulator.h>
#include <dev/extres/phy/phy.h>

#include "phy_if.h"

#define	USBPHY_NPHYS	4

static struct ofw_compat_data compat_data[] = {
	{ "allwinner,sun4i-a10-usb-phy",	1 },
	{ "allwinner,sun5i-a13-usb-phy",	1 },
	{ "allwinner,sun6i-a31-usb-phy",	1 },
	{ "allwinner,sun7i-a20-usb-phy",	1 },
	{ "allwinner,sun8i-a83t-usb-phy",	1 },
	{ "allwinner,sun8i-h3-usb-phy",		1 },
	{ NULL,					0 }
};

struct awusbphy_softc {
	regulator_t		reg[USBPHY_NPHYS];
	gpio_pin_t		id_det_pin;
	int			id_det_valid;
	gpio_pin_t		vbus_det_pin;
	int			vbus_det_valid;
};

static int
awusbphy_init(device_t dev)
{
	struct awusbphy_softc *sc;
	phandle_t node;
	char pname[20];
	int error, off;
	regulator_t reg;
	hwreset_t rst;
	clk_t clk;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

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

	/* Get regulators */
	for (off = 0; off < USBPHY_NPHYS; off++) {
		snprintf(pname, sizeof(pname), "usb%d_vbus-supply", off);
		if (regulator_get_by_ofw_property(dev, pname, &reg) == 0)
			sc->reg[off] = reg;
	}

	/* Get GPIOs */
	error = gpio_pin_get_by_ofw_property(dev, node, "usb0_id_det-gpios",
	    &sc->id_det_pin);
	if (error == 0)
		sc->id_det_valid = 1;
	error = gpio_pin_get_by_ofw_property(dev, node, "usb0_vbus_det-gpios",
	    &sc->vbus_det_pin);
	if (error == 0)
		sc->vbus_det_valid = 1;

	return (0);
}

static int
awusbphy_vbus_detect(device_t dev, int *val)
{
	struct awusbphy_softc *sc;
	bool active;
	int error;

	sc = device_get_softc(dev);

	if (sc->vbus_det_valid) {
		error = gpio_pin_is_active(sc->vbus_det_pin, &active);
		if (error != 0)
			return (error);
		*val = active;
		return (0);
	}

	*val = 1;
	return (0);
}

static int
awusbphy_phy_enable(device_t dev, int phy, bool enable)
{
	struct awusbphy_softc *sc;
	regulator_t reg;
	int error, vbus_det;

	if (phy < 0 || phy >= USBPHY_NPHYS)
		return (ERANGE);

	sc = device_get_softc(dev);

	/* Regulators are optional. If not found, return success. */
	reg = sc->reg[phy];
	if (reg == NULL)
		return (0);

	if (enable) {
		/* If an external vbus is detected, do not enable phy 0 */
		if (phy == 0) {
			error = awusbphy_vbus_detect(dev, &vbus_det);
			if (error == 0 && vbus_det == 1)
				return (0);
		} else
			error = 0;
		if (error == 0)
			error = regulator_enable(reg);
	} else
		error = regulator_disable(reg);
	if (error != 0) {
		device_printf(dev, "couldn't %s regulator for phy %d\n",
		    enable ? "enable" : "disable", phy);
		return (error);
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
	if (error) {
		device_printf(dev, "failed to initialize USB PHY, error %d\n",
		    error);
		return (error);
	}

	phy_register_provider(dev);

	return (error);
}

static device_method_t awusbphy_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		awusbphy_probe),
	DEVMETHOD(device_attach,	awusbphy_attach),

	/* PHY interface */
	DEVMETHOD(phy_enable,		awusbphy_phy_enable),

	DEVMETHOD_END
};

static driver_t awusbphy_driver = {
	"awusbphy",
	awusbphy_methods,
	sizeof(struct awusbphy_softc)
};

static devclass_t awusbphy_devclass;

EARLY_DRIVER_MODULE(awusbphy, simplebus, awusbphy_driver, awusbphy_devclass,
    0, 0, BUS_PASS_RESOURCE + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(awusbphy, 1);
