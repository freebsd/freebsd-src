/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2009 Oleksandr Tymoshenko <gonzo@freebsd.org>
 * Copyright (c) 2010 Luiz Otavio O Souza
 * Copyright (c) 2019 Ian Lepore <ian@freebsd.org>
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

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/iicbus/iiconf.h>

#include "gpiobus_if.h"
#include "iicbb_if.h"

#define	GPIOIIC_SCL_DFLT	0
#define	GPIOIIC_SDA_DFLT	1
#define	GPIOIIC_MIN_PINS	2

struct gpioiic_softc 
{
	device_t	dev;
	gpio_pin_t	sclpin;
	gpio_pin_t	sdapin;
};

#ifdef FDT

#include <dev/ofw/ofw_bus.h>

static struct ofw_compat_data compat_data[] = {
	{"i2c-gpio",  true}, /* Standard devicetree compat string */
	{"gpioiic",   true}, /* Deprecated old freebsd compat string */
	{NULL,        false}
};
OFWBUS_PNP_INFO(compat_data);
SIMPLEBUS_PNP_INFO(compat_data);

static phandle_t
gpioiic_get_node(device_t bus, device_t dev)
{

	/* Share our fdt node with iicbus so it can find its child nodes. */
	return (ofw_bus_get_node(bus));
}

static int
gpioiic_setup_fdt_pins(struct gpioiic_softc *sc)
{
	phandle_t node;
	int err;

	node = ofw_bus_get_node(sc->dev);

	/*
	 * Historically, we used the first two array elements of the gpios
	 * property.  The modern bindings specify separate scl-gpios and
	 * sda-gpios properties.  We cope with whichever is present.
	 */
	if (OF_hasprop(node, "gpios")) {
		if ((err = gpio_pin_get_by_ofw_idx(sc->dev, node,
		    GPIOIIC_SCL_DFLT, &sc->sclpin)) != 0) {
			device_printf(sc->dev, "invalid gpios property\n");
			return (err);
		}
		if ((err = gpio_pin_get_by_ofw_idx(sc->dev, node,
		    GPIOIIC_SDA_DFLT, &sc->sdapin)) != 0) {
			device_printf(sc->dev, "ivalid gpios property\n");
			return (err);
		}
	} else {
		if ((err = gpio_pin_get_by_ofw_property(sc->dev, node,
		    "scl-gpios", &sc->sclpin)) != 0) {
			device_printf(sc->dev, "missing scl-gpios property\n");
			return (err);
		}
		if ((err = gpio_pin_get_by_ofw_property(sc->dev, node,
		    "sda-gpios", &sc->sdapin)) != 0) {
			device_printf(sc->dev, "missing sda-gpios property\n");
			return (err);
		}
	}
	return (0);
}
#endif /* FDT */

static int
gpioiic_setup_hinted_pins(struct gpioiic_softc *sc)
{
	device_t busdev;
	const char *busname, *devname;
	int err, numpins, sclnum, sdanum, unit;

	devname = device_get_name(sc->dev);
	unit = device_get_unit(sc->dev);
	busdev = device_get_parent(sc->dev);

	/*
	 * If there is not an "at" hint naming our actual parent, then we
	 * weren't instantiated as a child of gpiobus via hints, and we thus
	 * can't access ivars that only exist for such children.
	 */
	if (resource_string_value(devname, unit, "at", &busname) != 0 ||
	    (strcmp(busname, device_get_nameunit(busdev)) != 0 &&
	     strcmp(busname, device_get_name(busdev)) != 0)) {
		return (ENOENT);
	}

	/* Make sure there were hints for at least two pins. */
	numpins = gpiobus_get_npins(sc->dev);
	if (numpins < GPIOIIC_MIN_PINS) {
#ifdef FDT
		/*
		 * Be silent when there are no hints on FDT systems; the FDT
		 * data will provide the pin config (we'll whine if it doesn't).
		 */
		if (numpins == 0) {
			return (ENOENT);
		}
#endif
		device_printf(sc->dev, 
		    "invalid pins hint; it must contain at least %d pins\n",
		    GPIOIIC_MIN_PINS);
		return (EINVAL);
	}

	/*
	 * Our parent bus has already parsed the pins hint and it will use that
	 * info when we call gpio_pin_get_by_child_index().  But we have to
	 * handle the scl/sda index hints that tell us which of the two pins is
	 * the clock and which is the data.  They're optional, but if present
	 * they must be a valid index (0 <= index < numpins).
	 */
	if ((err = resource_int_value(devname, unit, "scl", &sclnum)) != 0)
		sclnum = GPIOIIC_SCL_DFLT;
	else if (sclnum < 0 || sclnum >= numpins) {
		device_printf(sc->dev, "invalid scl hint %d\n", sclnum);
		return (EINVAL);
	}
	if ((err = resource_int_value(devname, unit, "sda", &sdanum)) != 0)
		sdanum = GPIOIIC_SDA_DFLT;
	else if (sdanum < 0 || sdanum >= numpins) {
		device_printf(sc->dev, "invalid sda hint %d\n", sdanum);
		return (EINVAL);
	}

	/* Allocate gpiobus_pin structs for the pins we found above. */
	if ((err = gpio_pin_get_by_child_index(sc->dev, sclnum,
	    &sc->sclpin)) != 0)
		return (err);
	if ((err = gpio_pin_get_by_child_index(sc->dev, sdanum,
	    &sc->sdapin)) != 0)
		return (err);

	return (0);
}

static void
gpioiic_setsda(device_t dev, int val)
{
	struct gpioiic_softc *sc = device_get_softc(dev);

	if (val) {
		gpio_pin_setflags(sc->sdapin, GPIO_PIN_INPUT);
	} else {
		gpio_pin_setflags(sc->sdapin,
		    GPIO_PIN_OUTPUT | GPIO_PIN_OPENDRAIN);
		gpio_pin_set_active(sc->sdapin, 0);
	}
}

static void
gpioiic_setscl(device_t dev, int val)
{
	struct gpioiic_softc *sc = device_get_softc(dev);

	if (val) {
		gpio_pin_setflags(sc->sclpin, GPIO_PIN_INPUT);
	} else {
		gpio_pin_setflags(sc->sclpin,
		    GPIO_PIN_OUTPUT | GPIO_PIN_OPENDRAIN);
		gpio_pin_set_active(sc->sclpin, 0);
	}
}

static int
gpioiic_getscl(device_t dev)
{
	struct gpioiic_softc *sc = device_get_softc(dev);
	bool val;

	gpio_pin_setflags(sc->sclpin, GPIO_PIN_INPUT);
	gpio_pin_is_active(sc->sclpin, &val);
	return (val);
}

static int
gpioiic_getsda(device_t dev)
{
	struct gpioiic_softc *sc = device_get_softc(dev);
	bool val;

	gpio_pin_setflags(sc->sdapin, GPIO_PIN_INPUT);
	gpio_pin_is_active(sc->sdapin, &val);
	return (val);
}

static int
gpioiic_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{
	struct gpioiic_softc *sc = device_get_softc(dev);

	/* Stop driving the bus pins. */
	gpio_pin_setflags(sc->sdapin, GPIO_PIN_INPUT);
	gpio_pin_setflags(sc->sclpin, GPIO_PIN_INPUT);

	/* Indicate that we have no slave address (master mode). */
	return (IIC_ENOADDR);
}

static void
gpioiic_cleanup(struct gpioiic_softc *sc)
{

	device_delete_children(sc->dev);

	if (sc->sclpin != NULL)
		gpio_pin_release(sc->sclpin);

	if (sc->sdapin != NULL)
		gpio_pin_release(sc->sdapin);
}

static int
gpioiic_probe(device_t dev)
{
	int rv;

	/*
	 * By default we only bid to attach if specifically added by our parent
	 * (usually via hint.gpioiic.#.at=busname).  On FDT systems we bid as
	 * the default driver based on being configured in the FDT data.
	 */
	rv = BUS_PROBE_NOWILDCARD;

#ifdef FDT
	if (ofw_bus_status_okay(dev) &&
	    ofw_bus_search_compatible(dev, compat_data)->ocd_data)
                rv = BUS_PROBE_DEFAULT;
#endif

	device_set_desc(dev, "GPIO I2C");

	return (rv);
}

static int
gpioiic_attach(device_t dev)
{
	struct gpioiic_softc *sc = device_get_softc(dev);
	int err;

	sc->dev = dev;

	/* Acquire our gpio pins. */
	err = gpioiic_setup_hinted_pins(sc);
#ifdef FDT
	if (err != 0)
		err = gpioiic_setup_fdt_pins(sc);
#endif
	if (err != 0) {
		device_printf(sc->dev, "no pins configured\n");
		gpioiic_cleanup(sc);
		return (ENXIO);
	}

	/*
	 * Say what we came up with for pin config.
	 * NB: in the !FDT case the controller driver might not be set up enough
	 * for GPIO_GET_BUS() to work.  Also, our parent is the only gpiobus
	 * that can provide our pins.
	 */
	device_printf(dev, "SCL pin: %s:%d, SDA pin: %s:%d\n",
#ifdef FDT
	    device_get_nameunit(GPIO_GET_BUS(sc->sclpin->dev)), sc->sclpin->pin,
	    device_get_nameunit(GPIO_GET_BUS(sc->sdapin->dev)), sc->sdapin->pin);
#else
	    device_get_nameunit(device_get_parent(dev)), sc->sclpin->pin,
	    device_get_nameunit(device_get_parent(dev)), sc->sdapin->pin);
#endif

	/* Add the bitbang driver as our only child; it will add iicbus. */
	device_add_child(sc->dev, "iicbb", -1);
	return (bus_generic_attach(dev));
}

static int
gpioiic_detach(device_t dev)
{
	struct gpioiic_softc *sc = device_get_softc(dev);
	int err;

	if ((err = bus_generic_detach(dev)) != 0)
		return (err);

	gpioiic_cleanup(sc);

	return (0);
}

static devclass_t gpioiic_devclass;

static device_method_t gpioiic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gpioiic_probe),
	DEVMETHOD(device_attach,	gpioiic_attach),
	DEVMETHOD(device_detach,	gpioiic_detach),

	/* iicbb interface */
	DEVMETHOD(iicbb_setsda,		gpioiic_setsda),
	DEVMETHOD(iicbb_setscl,		gpioiic_setscl),
	DEVMETHOD(iicbb_getsda,		gpioiic_getsda),
	DEVMETHOD(iicbb_getscl,		gpioiic_getscl),
	DEVMETHOD(iicbb_reset,		gpioiic_reset),

#ifdef FDT
	/* OFW bus interface */
	DEVMETHOD(ofw_bus_get_node,	gpioiic_get_node),
#endif

	DEVMETHOD_END
};

static driver_t gpioiic_driver = {
	"gpioiic",
	gpioiic_methods,
	sizeof(struct gpioiic_softc),
};

DRIVER_MODULE(gpioiic, gpiobus, gpioiic_driver, gpioiic_devclass, 0, 0);
DRIVER_MODULE(gpioiic, simplebus, gpioiic_driver, gpioiic_devclass, 0, 0);
DRIVER_MODULE(iicbb, gpioiic, iicbb_driver, iicbb_devclass, 0, 0);
MODULE_DEPEND(gpioiic, iicbb, IICBB_MINVER, IICBB_PREFVER, IICBB_MAXVER);
MODULE_DEPEND(gpioiic, gpiobus, 1, 1, 1);
