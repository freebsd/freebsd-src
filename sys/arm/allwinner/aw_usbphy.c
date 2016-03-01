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

#include "gpio_if.h"

#define	USBPHY_NUMOFF		3
#define	GPIO_POLARITY(flags)	(((flags) & 1) ? GPIO_PIN_LOW : GPIO_PIN_HIGH)

static struct ofw_compat_data compat_data[] = {
	{ "allwinner,sun4i-a10-usb-phy",	1 },
	{ "allwinner,sun5i-a13-usb-phy",	1 },
	{ "allwinner,sun6i-a31-usb-phy",	1 },
	{ "allwinner,sun7i-a20-usb-phy",	1 },
	{ NULL,					0 }
};

static int
awusbphy_gpio_set(device_t dev, phandle_t node, const char *pname)
{
	pcell_t gpio_prop[4];
	phandle_t gpio_node;
	device_t gpio_dev;
	uint32_t pin, flags;
	ssize_t len;
	int val;

	len = OF_getencprop(node, pname, gpio_prop, sizeof(gpio_prop));
	if (len == -1)
		return (0);

	if (len != sizeof(gpio_prop)) {
		device_printf(dev, "property %s length was %d, expected %d\n",
		    pname, len, sizeof(gpio_prop));
		return (ENXIO);
	}

	gpio_node = OF_node_from_xref(gpio_prop[0]);
	gpio_dev = OF_device_from_xref(gpio_prop[0]);
	if (gpio_dev == NULL) {
		device_printf(dev, "failed to get the GPIO device for %s\n",
		    pname);
		return (ENOENT);
	}

	if (GPIO_MAP_GPIOS(gpio_dev, node, gpio_node,
	    sizeof(gpio_prop) / sizeof(gpio_prop[0]) - 1, gpio_prop + 1,
	    &pin, &flags) != 0) {
		device_printf(dev, "failed to map the GPIO pin for %s\n",
		    pname);
		return (ENXIO);
	}

	val = GPIO_POLARITY(flags);

	GPIO_PIN_SETFLAGS(gpio_dev, pin, GPIO_PIN_OUTPUT);
	GPIO_PIN_SET(gpio_dev, pin, val);

	return (0);
}

static int
awusbphy_supply_set(device_t dev, const char *pname)
{
	phandle_t node, reg_node;
	pcell_t reg_xref;

	node = ofw_bus_get_node(dev);

	if (OF_getencprop(node, pname, &reg_xref, sizeof(reg_xref)) == -1)
		return (0);

	reg_node = OF_node_from_xref(reg_xref);

	return (awusbphy_gpio_set(dev, reg_node, "gpio"));
}

static int
awusbphy_init(device_t dev)
{
	char pname[20];
	phandle_t node;
	int error, off;

	node = ofw_bus_get_node(dev);

	for (off = 0; off < USBPHY_NUMOFF; off++) {
		snprintf(pname, sizeof(pname), "usb%d_id_det-gpio", off);
		error = awusbphy_gpio_set(dev, node, pname);
		if (error)
			return (error);

		snprintf(pname, sizeof(pname), "usb%d_vbus_det-gpio", off);
		error = awusbphy_gpio_set(dev, node, pname);
		if (error)
			return (error);

		snprintf(pname, sizeof(pname), "usb%d_vbus-supply", off);
		error = awusbphy_supply_set(dev, pname);
		if (error)
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
