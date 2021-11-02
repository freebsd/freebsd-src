/*-
 * Copyright (c) 2020 Alstom Group.
 * Copyright (c) 2020 Semihalf.
 * Copyright (c) 2015 Justin Hibbits
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/gpio/qoriq_gpio.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "gpio_if.h"

static device_t
qoriq_gpio_get_bus(device_t dev)
{
	struct qoriq_gpio_softc *sc;

	sc = device_get_softc(dev);

	return (sc->busdev);
}

static int
qoriq_gpio_pin_max(device_t dev, int *maxpin)
{

	*maxpin = MAXPIN;
	return (0);
}

/* Get a specific pin's capabilities. */
static int
qoriq_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct qoriq_gpio_softc *sc;

	sc = device_get_softc(dev);

	if (!VALID_PIN(pin))
		return (EINVAL);

	GPIO_LOCK(sc);
	*caps = sc->sc_pins[pin].gp_caps;
	GPIO_UNLOCK(sc);

	return (0);
}

/* Get a specific pin's name. */
static int
qoriq_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{

	if (!VALID_PIN(pin))
		return (EINVAL);

	snprintf(name, GPIOMAXNAME, "qoriq_gpio%d.%d",
	    device_get_unit(dev), pin);
	name[GPIOMAXNAME-1] = '\0';

	return (0);
}

static int
qoriq_gpio_pin_configure(device_t dev, uint32_t pin, uint32_t flags)
{
	struct qoriq_gpio_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	if ((flags & sc->sc_pins[pin].gp_caps) != flags) {
		return (EINVAL);
	}

	if (flags & GPIO_PIN_INPUT) {
		reg = bus_read_4(sc->sc_mem, GPIO_GPDIR);
		reg &= ~(1 << (31 - pin));
		bus_write_4(sc->sc_mem, GPIO_GPDIR, reg);
	}
	else if (flags & GPIO_PIN_OUTPUT) {
		reg = bus_read_4(sc->sc_mem, GPIO_GPDIR);
		reg |= (1 << (31 - pin));
		bus_write_4(sc->sc_mem, GPIO_GPDIR, reg);
		reg = bus_read_4(sc->sc_mem, GPIO_GPODR);
		if (flags & GPIO_PIN_OPENDRAIN)
			reg |= (1 << (31 - pin));
		else
			reg &= ~(1 << (31 - pin));
		bus_write_4(sc->sc_mem, GPIO_GPODR, reg);
	}
	sc->sc_pins[pin].gp_flags = flags;

	return (0);
}

/* Set flags for the pin. */
static int
qoriq_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct qoriq_gpio_softc *sc = device_get_softc(dev);
	uint32_t ret;

	if (!VALID_PIN(pin))
		return (EINVAL);

	if ((flags & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) ==
	    (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT))
		return (EINVAL);

	GPIO_LOCK(sc);
	ret = qoriq_gpio_pin_configure(dev, pin, flags);
	GPIO_UNLOCK(sc);
	return (0);
}

static int
qoriq_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *pflags)
{
	struct qoriq_gpio_softc *sc;

	if (!VALID_PIN(pin))
		return (EINVAL);

	sc = device_get_softc(dev);

	GPIO_LOCK(sc);
	*pflags = sc->sc_pins[pin].gp_flags;
	GPIO_UNLOCK(sc);

	return (0);
}

/* Set a specific output pin's value. */
static int
qoriq_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct qoriq_gpio_softc *sc = device_get_softc(dev);
	uint32_t outvals;
	uint8_t pinbit;

	if (!VALID_PIN(pin) || value > 1)
		return (EINVAL);

	GPIO_LOCK(sc);
	pinbit = 31 - pin;

	outvals = bus_read_4(sc->sc_mem, GPIO_GPDAT);
	outvals &= ~(1 << pinbit);
	outvals |= (value << pinbit);
	bus_write_4(sc->sc_mem, GPIO_GPDAT, outvals);

	GPIO_UNLOCK(sc);

	return (0);
}

/* Get a specific pin's input value. */
static int
qoriq_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *value)
{
	struct qoriq_gpio_softc *sc = device_get_softc(dev);

	if (!VALID_PIN(pin))
		return (EINVAL);

	*value = (bus_read_4(sc->sc_mem, GPIO_GPDAT) >> (31 - pin)) & 1;

	return (0);
}

/* Toggle a pin's output value. */
static int
qoriq_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct qoriq_gpio_softc *sc = device_get_softc(dev);
	uint32_t val;

	if (!VALID_PIN(pin))
		return (EINVAL);

	GPIO_LOCK(sc);

	val = bus_read_4(sc->sc_mem, GPIO_GPDAT);
	val ^= (1 << (31 - pin));
	bus_write_4(sc->sc_mem, GPIO_GPDAT, val);

	GPIO_UNLOCK(sc);

	return (0);
}

static struct ofw_compat_data gpio_matches[] = {
    {"fsl,pq3-gpio", 1},
    {"fsl,mpc8572-gpio", 1},
    {0, 0}
};

static int
qoriq_gpio_probe(device_t dev)
{

	if (ofw_bus_search_compatible(dev, gpio_matches)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Freescale QorIQ GPIO driver");

	return (0);
}

static int
qoriq_gpio_pin_access_32(device_t dev, uint32_t first_pin, uint32_t clear_pins,
    uint32_t change_pins, uint32_t *orig_pins)
{
	struct qoriq_gpio_softc *sc;
	uint32_t hwstate;

	sc = device_get_softc(dev);

	if (first_pin != 0)
		return (EINVAL);

	GPIO_LOCK(sc);
	hwstate = bus_read_4(sc->sc_mem, GPIO_GPDAT);
	bus_write_4(sc->sc_mem, GPIO_GPDAT,
	    (hwstate & ~clear_pins) ^ change_pins);
	GPIO_UNLOCK(sc);

	if (orig_pins != NULL)
		*orig_pins = hwstate;

	return (0);
}

static int
qoriq_gpio_pin_config_32(device_t dev, uint32_t first_pin, uint32_t num_pins,
    uint32_t *pin_flags)
{
	uint32_t dir, odr, mask, reg;
	struct qoriq_gpio_softc *sc;
	uint32_t newflags[32];
	int i;

	if (first_pin != 0 || !VALID_PIN(num_pins))
		return (EINVAL);

	sc = device_get_softc(dev);

	dir = odr = mask = 0;

	for (i = 0; i < num_pins; i++) {
		newflags[i] = 0;
		mask |= (1 << i);

		if (pin_flags[i] & GPIO_PIN_INPUT) {
			newflags[i] = GPIO_PIN_INPUT;
			dir &= ~(1 << i);
		} else {
			newflags[i] = GPIO_PIN_OUTPUT;
			dir |= (1 << i);

			if (pin_flags[i] & GPIO_PIN_OPENDRAIN) {
				newflags[i] |= GPIO_PIN_OPENDRAIN;
				odr |= (1 << i);
			} else {
				newflags[i] |= GPIO_PIN_PUSHPULL;
				odr &= ~(1 << i);
			}
		}
	}

	GPIO_LOCK(sc);

	reg = (bus_read_4(sc->sc_mem, GPIO_GPDIR) & ~mask) | dir;
	bus_write_4(sc->sc_mem, GPIO_GPDIR, reg);

	reg = (bus_read_4(sc->sc_mem, GPIO_GPODR) & ~mask) | odr;
	bus_write_4(sc->sc_mem, GPIO_GPODR, reg);

	for (i = 0; i < num_pins; i++)
		sc->sc_pins[i].gp_flags = newflags[i];

	GPIO_UNLOCK(sc);

	return (0);
}

static int
qoriq_gpio_map_gpios(device_t bus, phandle_t dev, phandle_t gparent, int gcells,
    pcell_t *gpios, uint32_t *pin, uint32_t *flags)
{
	struct qoriq_gpio_softc *sc;
	int err;

	if (!VALID_PIN(gpios[0]))
		return (EINVAL);

	sc = device_get_softc(bus);
	GPIO_LOCK(sc);
	err = qoriq_gpio_pin_configure(bus, gpios[0], gpios[1]);
	GPIO_UNLOCK(sc);

	if (err == 0) {
		*pin = gpios[0];
		*flags = gpios[1];
	}

	return (err);
}

int
qoriq_gpio_attach(device_t dev)
{
	struct qoriq_gpio_softc *sc = device_get_softc(dev);
	int i, rid;

	sc->dev = dev;

	GPIO_LOCK_INIT(sc);

	/* Allocate memory. */
	rid = 0;
	sc->sc_mem = bus_alloc_resource_any(dev,
		     SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->sc_mem == NULL) {
		device_printf(dev, "Can't allocate memory for device output port");
		qoriq_gpio_detach(dev);
		return (ENOMEM);
	}

	for (i = 0; i <= MAXPIN; i++)
		sc->sc_pins[i].gp_caps = DEFAULT_CAPS;

	sc->busdev = gpiobus_attach_bus(dev);
	if (sc->busdev == NULL) {
		qoriq_gpio_detach(dev);
		return (ENOMEM);
	}
	/*
	 * Enable the GPIO Input Buffer for all GPIOs.
	 * This is safe on devices without a GPIBE register, because those
	 * devices ignore writes and read 0's in undefined portions of the map.
	 */
	if (ofw_bus_is_compatible(dev, "fsl,qoriq-gpio"))
		bus_write_4(sc->sc_mem, GPIO_GPIBE, 0xffffffff);

	OF_device_register_xref(OF_xref_from_node(ofw_bus_get_node(dev)), dev);

	return (0);
}

int
qoriq_gpio_detach(device_t dev)
{
	struct qoriq_gpio_softc *sc = device_get_softc(dev);

	gpiobus_detach_bus(dev);

	if (sc->sc_mem != NULL) {
		/* Release output port resource. */
		bus_release_resource(dev, SYS_RES_MEMORY,
				     rman_get_rid(sc->sc_mem), sc->sc_mem);
	}

	GPIO_LOCK_DESTROY(sc);

	return (0);
}

static device_method_t qoriq_gpio_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, 	qoriq_gpio_probe),
	DEVMETHOD(device_attach, 	qoriq_gpio_attach),
	DEVMETHOD(device_detach, 	qoriq_gpio_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus, 	qoriq_gpio_get_bus),
	DEVMETHOD(gpio_pin_max, 	qoriq_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname, 	qoriq_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getcaps, 	qoriq_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_get, 	qoriq_gpio_pin_get),
	DEVMETHOD(gpio_pin_set, 	qoriq_gpio_pin_set),
	DEVMETHOD(gpio_pin_getflags, 	qoriq_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags, 	qoriq_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_toggle, 	qoriq_gpio_pin_toggle),

	DEVMETHOD(gpio_map_gpios,	qoriq_gpio_map_gpios),
	DEVMETHOD(gpio_pin_access_32,	qoriq_gpio_pin_access_32),
	DEVMETHOD(gpio_pin_config_32,	qoriq_gpio_pin_config_32),

	DEVMETHOD_END
};

static devclass_t qoriq_gpio_devclass;

DEFINE_CLASS_0(gpio, qoriq_gpio_driver, qoriq_gpio_methods,
    sizeof(struct qoriq_gpio_softc));

EARLY_DRIVER_MODULE(qoriq_gpio, simplebus, qoriq_gpio_driver,
    qoriq_gpio_devclass, NULL, NULL,
    BUS_PASS_RESOURCE + BUS_PASS_ORDER_MIDDLE);
