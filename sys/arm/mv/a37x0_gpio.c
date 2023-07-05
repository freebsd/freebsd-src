/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2019, Rubicon Communications, LLC (Netgate)
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "gpio_if.h"
#include "syscon_if.h"

struct a37x0_gpio_softc {
	device_t		sc_busdev;
	int			sc_type;
	uint32_t		sc_max_pins;
	uint32_t		sc_npins;
	struct syscon		*syscon;
};

/* Memory regions. */
#define	A37X0_GPIO			0
#define	A37X0_INTR			1

/* North Bridge / South Bridge. */
#define	A37X0_NB_GPIO			1
#define	A37X0_SB_GPIO			2

#define	A37X0_GPIO_WRITE(_sc, _off, _val)		\
    SYSCON_WRITE_4((_sc)->syscon, (_off), (_val))
#define	A37X0_GPIO_READ(_sc, _off)			\
    SYSCON_READ_4((_sc)->syscon, (_off))

#define	A37X0_GPIO_BIT(_p)		(1U << ((_p) % 32))
#define	A37X0_GPIO_OUT_EN(_p)		(0x0 + ((_p) / 32) * 4)
#define	A37X0_GPIO_LATCH(_p)		(0x8 + ((_p) / 32) * 4)
#define	A37X0_GPIO_INPUT(_p)		(0x10 + ((_p) / 32) * 4)
#define	A37X0_GPIO_OUTPUT(_p)		(0x18 + ((_p) / 32) * 4)
#define	A37X0_GPIO_SEL			0x30

static struct ofw_compat_data compat_data[] = {
	{ "marvell,armada3710-nb-pinctrl",	A37X0_NB_GPIO },
	{ "marvell,armada3710-sb-pinctrl",	A37X0_SB_GPIO },
	{ NULL, 0 }
};

static phandle_t
a37x0_gpio_get_node(device_t bus, device_t dev)
{

	return (ofw_bus_get_node(bus));
}

static device_t
a37x0_gpio_get_bus(device_t dev)
{
	struct a37x0_gpio_softc *sc;

	sc = device_get_softc(dev);

	return (sc->sc_busdev);
}

static int
a37x0_gpio_pin_max(device_t dev, int *maxpin)
{
	struct a37x0_gpio_softc *sc;

	sc = device_get_softc(dev);
	*maxpin = sc->sc_npins - 1;

	return (0);
}

static int
a37x0_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct a37x0_gpio_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->sc_npins)
		return (EINVAL);
	snprintf(name, GPIOMAXNAME, "pin %d", pin);

	return (0);
}

static int
a37x0_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct a37x0_gpio_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->sc_npins)
		return (EINVAL);
	*caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT;

	return (0);
}

static int
a37x0_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct a37x0_gpio_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);
	if (pin >= sc->sc_npins)
		return (EINVAL);
	reg = A37X0_GPIO_READ(sc, A37X0_GPIO_OUT_EN(pin));
	if ((reg & A37X0_GPIO_BIT(pin)) != 0)
		*flags = GPIO_PIN_OUTPUT;
	else
		*flags = GPIO_PIN_INPUT;

	return (0);
}

static int
a37x0_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct a37x0_gpio_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);
	if (pin >= sc->sc_npins)
		return (EINVAL);

	reg = A37X0_GPIO_READ(sc, A37X0_GPIO_OUT_EN(pin));
	if (flags & GPIO_PIN_OUTPUT)
		reg |= A37X0_GPIO_BIT(pin);
	else
		reg &= ~A37X0_GPIO_BIT(pin);
	A37X0_GPIO_WRITE(sc, A37X0_GPIO_OUT_EN(pin), reg);

	return (0);
}

static int
a37x0_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct a37x0_gpio_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);
	if (pin >= sc->sc_npins)
		return (EINVAL);

	reg = A37X0_GPIO_READ(sc, A37X0_GPIO_OUT_EN(pin));
	if ((reg & A37X0_GPIO_BIT(pin)) != 0)
		reg = A37X0_GPIO_READ(sc, A37X0_GPIO_OUTPUT(pin));
	else
		reg = A37X0_GPIO_READ(sc, A37X0_GPIO_INPUT(pin));
	*val = ((reg & A37X0_GPIO_BIT(pin)) != 0) ? 1 : 0;

	return (0);
}

static int
a37x0_gpio_pin_set(device_t dev, uint32_t pin, unsigned int val)
{
	struct a37x0_gpio_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);
	if (pin >= sc->sc_npins)
		return (EINVAL);

	reg = A37X0_GPIO_READ(sc, A37X0_GPIO_OUTPUT(pin));
	if (val != 0)
		reg |= A37X0_GPIO_BIT(pin);
	else
		reg &= ~A37X0_GPIO_BIT(pin);
	A37X0_GPIO_WRITE(sc, A37X0_GPIO_OUTPUT(pin), reg);

	return (0);
}

static int
a37x0_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct a37x0_gpio_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);
	if (pin >= sc->sc_npins)
		return (EINVAL);

	reg = A37X0_GPIO_READ(sc, A37X0_GPIO_OUT_EN(pin));
	if ((reg & A37X0_GPIO_BIT(pin)) == 0)
		return (EINVAL);
	reg = A37X0_GPIO_READ(sc, A37X0_GPIO_OUTPUT(pin));
	reg ^= A37X0_GPIO_BIT(pin);
	A37X0_GPIO_WRITE(sc, A37X0_GPIO_OUTPUT(pin), reg);

	return (0);
}

static int
a37x0_gpio_probe(device_t dev)
{
	const char *desc;
	struct a37x0_gpio_softc *sc;

	if (!OF_hasprop(ofw_bus_get_node(dev), "gpio-controller"))
		return (ENXIO);

	sc = device_get_softc(dev);
	sc->sc_type = ofw_bus_search_compatible(
	    device_get_parent(dev), compat_data)->ocd_data;
	switch (sc->sc_type) {
	case A37X0_NB_GPIO:
		sc->sc_max_pins = 36;
		desc = "Armada 37x0 North Bridge GPIO Controller";
		break;
	case A37X0_SB_GPIO:
		sc->sc_max_pins = 30;
		desc = "Armada 37x0 South Bridge GPIO Controller";
		break;
	default:
		return (ENXIO);
	}
	device_set_desc(dev, desc);

	return (BUS_PROBE_DEFAULT);
}

static int
a37x0_gpio_attach(device_t dev)
{
	int err, ncells;
	pcell_t *ranges;
	struct a37x0_gpio_softc *sc;

	sc = device_get_softc(dev);

	err = syscon_get_handle_default(dev, &sc->syscon);
	if (err != 0) {
		device_printf(dev, "Cannot get syscon handle from parent\n");
		return (ENXIO);
	}

	/* Read and verify the "gpio-ranges" property. */
	ncells = OF_getencprop_alloc(ofw_bus_get_node(dev), "gpio-ranges",
	    (void **)&ranges);
	if (ncells == -1)
		return (ENXIO);
	if (ncells != sizeof(*ranges) * 4 || ranges[1] != 0 || ranges[2] != 0) {
		OF_prop_free(ranges);
		return (ENXIO);
	}
	sc->sc_npins = ranges[3];
	OF_prop_free(ranges);

	/* Check the number of pins in the DTS vs HW capabilities. */
	if (sc->sc_npins > sc->sc_max_pins)
		return (ENXIO);

	sc->sc_busdev = gpiobus_attach_bus(dev);
	if (sc->sc_busdev == NULL)
		return (ENXIO);

	return (0);
}

static int
a37x0_gpio_detach(device_t dev)
{

	return (EBUSY);
}

static device_method_t a37x0_gpio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		a37x0_gpio_probe),
	DEVMETHOD(device_attach,	a37x0_gpio_attach),
	DEVMETHOD(device_detach,	a37x0_gpio_detach),

	/* GPIO interface */
	DEVMETHOD(gpio_get_bus,		a37x0_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,		a37x0_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname,	a37x0_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getcaps,	a37x0_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	a37x0_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,	a37x0_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get,		a37x0_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		a37x0_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	a37x0_gpio_pin_toggle),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	a37x0_gpio_get_node),

	DEVMETHOD_END
};

static driver_t a37x0_gpio_driver = {
	"gpio",
	a37x0_gpio_methods,
	sizeof(struct a37x0_gpio_softc),
};

EARLY_DRIVER_MODULE(a37x0_gpio, simple_mfd, a37x0_gpio_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LAST);
