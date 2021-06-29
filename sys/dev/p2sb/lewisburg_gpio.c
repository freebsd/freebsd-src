/*-
 * Copyright (c) 2018 Stormshield
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

#include <sys/param.h>		/* defines used in kernel.h */
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>		/* types used in module initialization */
#include <sys/conf.h>		/* cdevsw struct */
#include <sys/uio.h>		/* uio struct */
#include <sys/malloc.h>
#include <sys/bus.h>		/* structs, prototypes for pci bus stuff and DEVMETHOD macros! */
#include <sys/gpio.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/gpio/gpiobusvar.h>

#include "gpio_if.h"

#include "lewisburg_gpiocm.h"

#define P2SB_GROUP_GPIO_MAX_PINS 24
struct lbggpio_softc
{
	device_t		sc_busdev;
	int groupid;
	int pins_off;
	int npins;
	char grpname;
	struct gpio_pin gpio_setup[P2SB_GROUP_GPIO_MAX_PINS];
};

static device_t
lbggpio_get_bus(device_t dev)
{
	struct lbggpio_softc *sc;

	sc = device_get_softc(dev);

	return (sc->sc_busdev);
}

static int
lbggpio_pin_max(device_t dev, int *maxpin)
{
	struct lbggpio_softc *sc;

	if (maxpin == NULL)
		return (EINVAL);

	sc = device_get_softc(dev);

	*maxpin = sc->npins - 1;

	return (0);
}

static int
lbggpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct lbggpio_softc *sc = device_get_softc(dev);

	if (name == NULL)
		return (EINVAL);

	if (pin >= sc->npins)
		return (EINVAL);

	strlcpy(name, sc->gpio_setup[pin].gp_name, GPIOMAXNAME);

	return (0);
}

static int
lbggpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct lbggpio_softc *sc = device_get_softc(dev);

	if (flags == NULL)
		return (EINVAL);

	if (pin >= sc->npins)
		return (EINVAL);

	*flags = sc->gpio_setup[pin].gp_flags;

	return (0);
}

static int
lbggpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct lbggpio_softc *sc = device_get_softc(dev);

	if (caps == NULL)
		return (EINVAL);

	if (pin >= sc->npins)
		return (EINVAL);

	*caps = sc->gpio_setup[pin].gp_caps;

	return (0);
}

static int
lbggpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct lbggpio_softc *sc = device_get_softc(dev);

	if (pin >= sc->npins)
		return (EINVAL);

	/* Check for unwanted flags. */
	if ((flags & sc->gpio_setup[pin].gp_caps) != flags)
		return (EINVAL);

	lbggpiocm_pin_setflags(device_get_parent(dev), dev, pin, flags);
	sc->gpio_setup[pin].gp_flags = flags;

	return (0);
}

static int
lbggpio_pin_get(device_t dev, uint32_t pin, uint32_t *value)
{
	struct lbggpio_softc *sc = device_get_softc(dev);

	if (value == NULL)
		return (EINVAL);

	if (pin >= sc->npins)
		return (EINVAL);

	return (lbggpiocm_pin_get(device_get_parent(dev), dev, pin, value));
}

static int
lbggpio_pin_set(device_t dev, uint32_t pin, uint32_t value)
{
	struct lbggpio_softc *sc = device_get_softc(dev);

	if (pin >= sc->npins)
		return (EINVAL);

	return (lbggpiocm_pin_set(device_get_parent(dev), dev, pin, value));
}

static int
lbggpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct lbggpio_softc *sc = device_get_softc(dev);

	if (pin >= sc->npins)
		return (EINVAL);

	return (lbggpiocm_pin_toggle(device_get_parent(dev), dev, pin));
}

static int
lbggpio_probe(device_t dev)
{
	struct lbggpio_softc *sc = device_get_softc(dev);
	/* X is a placeholder for the actual one letter group name. */
	static char desc[] = "LewisBurg GPIO Group X";

	sc->npins = lbggpiocm_get_group_npins(device_get_parent(dev), dev);
	sc->grpname = lbggpiocm_get_group_name(device_get_parent(dev), dev);
	if (sc->npins <= 0)
		return (ENXIO);

	desc[sizeof(desc)-2] = sc->grpname;
	device_set_desc_copy(dev, desc);
	return (BUS_PROBE_DEFAULT);
}

static int
lbggpio_attach(device_t dev)
{
	uint32_t i;
	struct lbggpio_softc *sc;

	sc = device_get_softc(dev);
	/* GPIO config */
	for (i = 0; i < sc->npins; ++i) {
		sc->gpio_setup[i].gp_pin = i;
		snprintf(sc->gpio_setup[i].gp_name,
		    sizeof(sc->gpio_setup[i].gp_name),
		    "GPIO %c%u", sc->grpname, i);
		sc->gpio_setup[i].gp_caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT;
	}

	/* support gpio */
	sc->sc_busdev = gpiobus_attach_bus(dev);
	if (sc->sc_busdev == NULL)
		return (ENXIO);

	return (0);
}

static int
lbggpio_detach(device_t dev)
{
	struct lbggpio_softc *sc;

	sc = device_get_softc(dev);

	if (sc->sc_busdev)
		gpiobus_detach_bus(dev);

	return (0);
}

static device_method_t lbggpio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		lbggpio_probe),
	DEVMETHOD(device_attach,	lbggpio_attach),
	DEVMETHOD(device_detach,	lbggpio_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus,		lbggpio_get_bus),
	DEVMETHOD(gpio_pin_max,		lbggpio_pin_max),
	DEVMETHOD(gpio_pin_getcaps,	lbggpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	lbggpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,	lbggpio_pin_setflags),
	DEVMETHOD(gpio_pin_getname,	lbggpio_pin_getname),
	DEVMETHOD(gpio_pin_set,		lbggpio_pin_set),
	DEVMETHOD(gpio_pin_get,		lbggpio_pin_get),
	DEVMETHOD(gpio_pin_toggle,	lbggpio_pin_toggle),

	DEVMETHOD_END
};

static driver_t lbggpio_driver = {
	"gpio",
	lbggpio_methods,
	sizeof(struct lbggpio_softc)
};

static devclass_t lbggpio_devclass;

DRIVER_MODULE(lbggpio, lbggpiocm, lbggpio_driver, lbggpio_devclass, NULL, NULL);
MODULE_DEPEND(lbggpio, gpiobus, 1, 1, 1);
