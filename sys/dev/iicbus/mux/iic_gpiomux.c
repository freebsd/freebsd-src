/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

/*
 * Driver for i2c bus muxes controlled by one or more gpio pins.
 *
 * This driver has #ifdef FDT sections in it, as if it supports both fdt and
 * hinted attachment, but there is currently no support for hinted attachment.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <dev/gpio/gpiobusvar.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/mux/iicmux.h>

#ifdef FDT
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

static struct ofw_compat_data compat_data[] = {
	{"i2c-mux-gpio",  true},
	{NULL,            false}
};
OFWBUS_PNP_INFO(compat_data);
SIMPLEBUS_PNP_INFO(compat_data);
#endif /* FDT */

#include <dev/iicbus/iiconf.h>
#include "iicmux.h"
#include "iicmux_if.h"

struct gpiomux_softc {
	struct iicmux_softc mux;
	int	idleidx;
	int	numpins;
	gpio_pin_t pins[IICMUX_MAX_BUSES];
};

#define IDLE_NOOP	(-1) /* When asked to idle the bus, do nothing. */

static int
gpiomux_bus_select(device_t dev, int busidx, struct iic_reqbus_data *rd)
{
	struct gpiomux_softc *sc = device_get_softc(dev);
	int i;

	/*
	 * The iicmux caller ensures busidx is between 0 and the number of buses
	 * we passed to iicmux_init_softc(), no need for validation here.  The
	 * bits in the index number are transcribed to the state of the pins,
	 * except when we're asked to idle the bus.  In that case, we transcribe
	 * sc->idleidx to the pins, unless that is IDLE_NOOP (leave the current
	 * bus selected), in which case we just bail.
	 */
	if (busidx == IICMUX_SELECT_IDLE) {
		if (sc->idleidx == IDLE_NOOP)
			return (0);
		busidx = sc->idleidx;
	}

	for (i = 0; i < sc->numpins; ++i)
		gpio_pin_set_active(sc->pins[i], busidx & (1u << i));

	return (0);
}

static int
gpiomux_probe(device_t dev)
{
	int rv;

	rv = ENXIO;

#ifdef FDT
	if (ofw_bus_status_okay(dev) &&
	    ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		rv = BUS_PROBE_DEFAULT;
#endif

	device_set_desc(dev, "I2C GPIO Mux");

	return (rv);
}

static void
gpiomux_release_pins(struct gpiomux_softc *sc)
{
	int i;

	for (i = 0; i < sc->numpins; ++i)
		gpio_pin_release(sc->pins[i]);
}

static int
gpiomux_attach(device_t dev)
{
	struct gpiomux_softc *sc = device_get_softc(dev);
	ssize_t len;
	device_t busdev;
	int err, i, idlebits, numchannels;
	pcell_t propval;
	phandle_t node;

	node = ofw_bus_get_node(dev);

	/*
	 * Locate the gpio pin(s) that control the mux hardware.  There can be
	 * multiple pins, but there must be at least one.
	 */
	for (i = 0; ; ++i) {
		err = gpio_pin_get_by_ofw_propidx(dev, node, "mux-gpios", i,
		    &sc->pins[i]);
		if (err != 0) {
			break;
		}
	}
	sc->numpins = i;
	if (sc->numpins == 0) {
		device_printf(dev, "cannot acquire pins listed in mux-gpios\n");
		if (err == 0)
			err = ENXIO;
		goto errexit;
	}
	numchannels = 1u << sc->numpins;
	if (numchannels > IICMUX_MAX_BUSES) {
		device_printf(dev, "too many mux-gpios pins for max %d buses\n",
		    IICMUX_MAX_BUSES);
		err = EINVAL;
		goto errexit;
	}

	/*
	 * We don't have a parent/child relationship to the upstream bus, we
	 * have to locate it via the i2c-parent property.  Explicitly tell the
	 * user which upstream we're associated with, since the normal attach
	 * message is going to mention only our actual parent.
	 */
	len = OF_getencprop(node, "i2c-parent", &propval, sizeof(propval));
	if (len != sizeof(propval)) {
		device_printf(dev, "cannot obtain i2c-parent property\n");
		err = ENXIO;
		goto errexit;
	}
	busdev = OF_device_from_xref((phandle_t)propval);
	if (busdev == NULL) {
		device_printf(dev,
		    "cannot find device referenced by i2c-parent property\n");
		err = ENXIO;
		goto errexit;
	}
	device_printf(dev, "upstream bus is %s\n", device_get_nameunit(busdev));

	/*
	 * If there is an idle-state property, that is the value we set the pins
	 * to when the bus is idle, otherwise idling the bus is a no-op
	 * (whichever bus was last accessed remains active).
	 */
	len = OF_getencprop(node, "idle-state", &propval, sizeof(propval));
	if (len == sizeof(propval)) {
		if ((int)propval >= numchannels) {
			device_printf(dev,
			    "idle-state property %d exceeds channel count\n",
			    propval);
		}
		sc->idleidx = (int)propval;
		idlebits = sc->idleidx;
	} else {
		sc->idleidx = IDLE_NOOP;
		idlebits = 0;
	}

	/* Preset the mux to the idle state to get things started. */
	for (i = 0; i < sc->numpins; ++i) {
		gpio_pin_setflags(sc->pins[i], GPIO_PIN_OUTPUT);
		gpio_pin_set_active(sc->pins[i], idlebits & (1u << i));
	}

	/* Init the core driver, have it add our child downstream buses. */
	if ((err = iicmux_attach(dev, busdev, numchannels)) == 0)
		bus_generic_attach(dev);

errexit:

	if (err != 0)
		gpiomux_release_pins(sc);

	return (err);
}

static int
gpiomux_detach(device_t dev)
{
	struct gpiomux_softc *sc = device_get_softc(dev);
	int err;

	if ((err = iicmux_detach(dev)) != 0)
		return (err);

	gpiomux_release_pins(sc);

	return (0);
}

static device_method_t gpiomux_methods[] = {
	/* device methods */
	DEVMETHOD(device_probe,			gpiomux_probe),
	DEVMETHOD(device_attach,		gpiomux_attach),
	DEVMETHOD(device_detach,		gpiomux_detach),

	/* iicmux methods */
	DEVMETHOD(iicmux_bus_select,		gpiomux_bus_select),

	DEVMETHOD_END
};

DEFINE_CLASS_1(iic_gpiomux, iic_gpiomux_driver, gpiomux_methods,
    sizeof(struct gpiomux_softc), iicmux_driver);
DRIVER_MODULE(iic_gpiomux, simplebus, iic_gpiomux_driver, 0, 0);
DRIVER_MODULE(iic_gpiomux, ofw_simplebus, iic_gpiomux_driver, 0, 0);

#ifdef FDT
DRIVER_MODULE(ofw_iicbus, iic_gpiomux, ofw_iicbus_driver, 0, 0);
#else
DRIVER_MODULE(iicbus, iic_gpiomux, iicbus_driver, 0, 0);
#endif

MODULE_DEPEND(iic_gpiomux, iicmux, 1, 1, 1);
MODULE_DEPEND(iic_gpiomux, iicbus, 1, 1, 1);
