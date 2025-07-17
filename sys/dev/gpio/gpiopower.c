/*-
 * Copyright (c) 2016 Justin Hibbits
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


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/reboot.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>

#include <dev/gpio/gpiobusvar.h>

struct gpiopower_softc {
	gpio_pin_t	sc_pin;
	int		sc_rbmask;
	int		sc_hi_period;
	int		sc_lo_period;
	int		sc_timeout;
};

static void gpiopower_assert(device_t dev, int howto);

static int
gpiopower_probe(device_t dev)
{

	if (ofw_bus_is_compatible(dev, "gpio-poweroff")) {
		device_set_desc(dev, "GPIO poweroff control");
		return (0);
	} else if (ofw_bus_is_compatible(dev, "gpio-restart")) {
		device_set_desc(dev, "GPIO restart control");
		return (0);
	}

	return (ENXIO);
}

static int
gpiopower_attach(device_t dev)
{
	struct gpiopower_softc *sc;
	phandle_t node;
	uint32_t prop;

	sc = device_get_softc(dev);

	if ((node = ofw_bus_get_node(dev)) == -1)
		return (ENXIO);

	if (ofw_gpiobus_parse_gpios(dev, "gpios", &sc->sc_pin) <= 0) {
		device_printf(dev, "failed to map GPIO pin\n");
		return (ENXIO);
	}

	if (ofw_bus_is_compatible(dev, "gpio-poweroff"))
		sc->sc_rbmask = RB_HALT | RB_POWEROFF;
	else
		sc->sc_rbmask = 0;

	sc->sc_hi_period = 100000;
	sc->sc_lo_period = 100000;
	sc->sc_timeout = 1000000;

	if ((OF_getprop(node, "active-delay-ms", &prop, sizeof(prop))) > 0)
		sc->sc_hi_period = fdt32_to_cpu(prop) * 1000;
	if ((OF_getprop(node, "inactive-delay-ms", &prop, sizeof(prop))) > 0)
		sc->sc_lo_period = fdt32_to_cpu(prop) * 1000;
	if ((OF_getprop(node, "timeout-ms", &prop, sizeof(prop))) > 0)
		sc->sc_timeout = fdt32_to_cpu(prop) * 1000;

	EVENTHANDLER_REGISTER(shutdown_final, gpiopower_assert, dev,
	    SHUTDOWN_PRI_LAST);

	return (0);
}

static void
gpiopower_assert(device_t dev, int howto)
{
	struct gpiopower_softc *sc;
	int do_assert;

	sc = device_get_softc(dev);
	do_assert = sc->sc_rbmask ? (sc->sc_rbmask & howto) :
	    ((howto & RB_HALT) == 0);

	if (!do_assert)
		return;

	if (howto & RB_POWEROFF)
		device_printf(dev, "powering system off\n");
	else if ((howto & RB_HALT) == 0)
		device_printf(dev, "resetting system\n");
	else
		return;

	gpio_pin_setflags(sc->sc_pin, GPIO_PIN_OUTPUT);
	gpio_pin_set_active(sc->sc_pin, true);
	DELAY(sc->sc_hi_period);
	gpio_pin_set_active(sc->sc_pin, false);
	DELAY(sc->sc_lo_period);
	gpio_pin_set_active(sc->sc_pin, true);
	DELAY(sc->sc_timeout);

	device_printf(dev, "%s failed\n",
	    (howto & RB_POWEROFF) != 0 ? "power off" : "reset");
}

static device_method_t gpiopower_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gpiopower_probe),
	DEVMETHOD(device_attach,	gpiopower_attach),

	DEVMETHOD_END
};

static driver_t gpiopower_driver = {
	"gpiopower",
	gpiopower_methods,
	sizeof(struct gpiopower_softc)
};

DRIVER_MODULE(gpiopower, simplebus, gpiopower_driver, 0, 0);
MODULE_DEPEND(gpiopower, gpiobus, 1, 1, 1);
