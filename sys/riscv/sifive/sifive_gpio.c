/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Jessica Clarke <jrtc27@FreeBSD.org>
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
 */

/* TODO: Provide interrupt controller interface */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/gpio.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>

/* Registers are 32-bit so can only fit 32 pins */
#define	SFGPIO_MAX_PINS		32

#define	SFGPIO_DEFAULT_CAPS	(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)

#define	SFGPIO_INPUT_VAL	0x0
#define	SFGPIO_INPUT_EN		0x4
#define	SFGPIO_OUTPUT_EN	0x8
#define	SFGPIO_OUTPUT_VAL	0xc
#define	SFGPIO_RISE_IE		0x18
#define	SFGPIO_RISE_IP		0x1c
#define	SFGPIO_FALL_IE		0x20
#define	SFGPIO_FALL_IP		0x24
#define	SFGPIO_HIGH_IE		0x28
#define	SFGPIO_HIGH_IP		0x2c
#define	SFGPIO_LOW_IE		0x30
#define	SFGPIO_LOW_IP		0x34

struct sfgpio_softc {
	device_t	dev;
	device_t	busdev;
	struct mtx	mtx;
	struct resource	*mem_res;
	int		mem_rid;
	struct resource	*irq_res;
	int		irq_rid;
	int		npins;
	struct gpio_pin	gpio_pins[SFGPIO_MAX_PINS];
};

#define	SFGPIO_LOCK(_sc)	mtx_lock(&(_sc)->mtx)
#define	SFGPIO_UNLOCK(_sc)	mtx_unlock(&(_sc)->mtx)

#define	SFGPIO_READ(_sc, _off)		\
    bus_read_4((_sc)->mem_res, (_off))
#define	SFGPIO_WRITE(_sc, _off, _val)	\
    bus_write_4((_sc)->mem_res, (_off), (_val))

static struct ofw_compat_data compat_data[] = {
	{ "sifive,gpio0",	1 },
	{ NULL,			0 },
};

static int
sfgpio_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "SiFive GPIO Controller");

	return (BUS_PROBE_DEFAULT);
}

static int
sfgpio_attach(device_t dev)
{
	struct sfgpio_softc *sc;
	phandle_t node;
	int error, i;
	pcell_t npins;
	uint32_t input_en, output_en;

	sc = device_get_softc(dev);
	sc->dev = dev;

	node = ofw_bus_get_node(dev);

	mtx_init(&sc->mtx, device_get_nameunit(sc->dev), NULL, MTX_DEF);

	sc->mem_rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->mem_rid, RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Cannot allocate memory resource\n");
		error = ENXIO;
		goto fail;
	}

	if (OF_getencprop(node, "ngpios", &npins, sizeof(npins)) <= 0) {
		/* Optional; defaults to 16 */
		npins = 16;
	} else if (npins > SFGPIO_MAX_PINS) {
		device_printf(dev, "Too many pins: %d\n", npins);
		error = ENXIO;
		goto fail;
	}
	sc->npins = npins;

	sc->irq_rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "Cannot allocate IRQ resource\n");
		error = ENXIO;
		goto fail;
	}

	input_en = SFGPIO_READ(sc, SFGPIO_INPUT_EN);
	output_en = SFGPIO_READ(sc, SFGPIO_OUTPUT_EN);
	for (i = 0; i < sc->npins; ++i) {
		sc->gpio_pins[i].gp_pin = i;
		sc->gpio_pins[i].gp_caps = SFGPIO_DEFAULT_CAPS;
		sc->gpio_pins[i].gp_flags =
		    ((input_en & (1u << i)) ? GPIO_PIN_INPUT : 0) |
		    ((output_en & (1u << i)) ? GPIO_PIN_OUTPUT : 0);
		snprintf(sc->gpio_pins[i].gp_name, GPIOMAXNAME, "GPIO%d", i);
		sc->gpio_pins[i].gp_name[GPIOMAXNAME - 1] = '\0';
	}

	sc->busdev = gpiobus_attach_bus(dev);
	if (sc->busdev == NULL) {
		device_printf(dev, "Cannot attach gpiobus\n");
		error = ENXIO;
		goto fail;
	}

	return (0);

fail:
	if (sc->busdev != NULL)
		gpiobus_detach_bus(dev);
	if (sc->irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid,
		    sc->irq_res);
	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid,
		    sc->mem_res);
	mtx_destroy(&sc->mtx);
	return (error);
}

static device_t
sfgpio_get_bus(device_t dev)
{
	struct sfgpio_softc *sc;

	sc = device_get_softc(dev);

	return (sc->busdev);
}

static int
sfgpio_pin_max(device_t dev, int *maxpin)
{
	struct sfgpio_softc *sc;

	sc = device_get_softc(dev);

	*maxpin = sc->npins - 1;

	return (0);
}

static int
sfgpio_pin_set(device_t dev, uint32_t pin, unsigned int val)
{
	struct sfgpio_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	if (pin >= sc->npins)
		return (EINVAL);

	SFGPIO_LOCK(sc);
	reg = SFGPIO_READ(sc, SFGPIO_OUTPUT_VAL);
	if (val)
		reg |= (1u << pin);
	else
		reg &= ~(1u << pin);
	SFGPIO_WRITE(sc, SFGPIO_OUTPUT_VAL, reg);
	SFGPIO_UNLOCK(sc);

	return (0);
}

static int
sfgpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct sfgpio_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	if (pin >= sc->npins)
		return (EINVAL);

	SFGPIO_LOCK(sc);
	if (sc->gpio_pins[pin].gp_flags & GPIO_PIN_OUTPUT)
		reg = SFGPIO_READ(sc, SFGPIO_OUTPUT_VAL);
	else
		reg = SFGPIO_READ(sc, SFGPIO_INPUT_VAL);
	*val = (reg & (1u << pin)) ? 1 : 0;
	SFGPIO_UNLOCK(sc);

	return (0);
}

static int
sfgpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct sfgpio_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	if (pin >= sc->npins)
		return (EINVAL);

	SFGPIO_LOCK(sc);
	reg = SFGPIO_READ(sc, SFGPIO_OUTPUT_VAL);
	reg ^= (1u << pin);
	SFGPIO_WRITE(sc, SFGPIO_OUTPUT_VAL, reg);
	SFGPIO_UNLOCK(sc);

	return (0);
}

static int
sfgpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct sfgpio_softc *sc;

	sc = device_get_softc(dev);

	if (pin >= sc->npins)
		return (EINVAL);

	SFGPIO_LOCK(sc);
	*caps = sc->gpio_pins[pin].gp_caps;
	SFGPIO_UNLOCK(sc);

	return (0);
}

static int
sfgpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct sfgpio_softc *sc;

	sc = device_get_softc(dev);

	if (pin >= sc->npins)
		return (EINVAL);

	SFGPIO_LOCK(sc);
	*flags = sc->gpio_pins[pin].gp_flags;
	SFGPIO_UNLOCK(sc);

	return (0);
}

static int
sfgpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct sfgpio_softc *sc;

	sc = device_get_softc(dev);

	if (pin >= sc->npins)
		return (EINVAL);

	SFGPIO_LOCK(sc);
	memcpy(name, sc->gpio_pins[pin].gp_name, GPIOMAXNAME);
	SFGPIO_UNLOCK(sc);

	return (0);
}

static int
sfgpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct sfgpio_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	if (pin >= sc->npins)
		return (EINVAL);

	SFGPIO_LOCK(sc);

	reg = SFGPIO_READ(sc, SFGPIO_INPUT_EN);
	if (flags & GPIO_PIN_INPUT) {
		reg |= (1u << pin);
		sc->gpio_pins[pin].gp_flags |= GPIO_PIN_INPUT;
	} else {
		reg &= ~(1u << pin);
		sc->gpio_pins[pin].gp_flags &= ~GPIO_PIN_INPUT;
	}
	SFGPIO_WRITE(sc, SFGPIO_INPUT_EN, reg);

	reg = SFGPIO_READ(sc, SFGPIO_OUTPUT_EN);
	if (flags & GPIO_PIN_OUTPUT) {
		reg |= (1u << pin);
		sc->gpio_pins[pin].gp_flags |= GPIO_PIN_OUTPUT;
	} else {
		reg &= ~(1u << pin);
		sc->gpio_pins[pin].gp_flags &= ~GPIO_PIN_OUTPUT;
	}
	SFGPIO_WRITE(sc, SFGPIO_OUTPUT_EN, reg);

	SFGPIO_UNLOCK(sc);

	return (0);
}

static int
sfgpio_pin_access_32(device_t dev, uint32_t first_pin, uint32_t clear_pins,
    uint32_t change_pins, uint32_t *orig_pins)
{
	struct sfgpio_softc *sc;
	uint32_t reg;

	if (first_pin != 0)
		return (EINVAL);

	sc = device_get_softc(dev);

	SFGPIO_LOCK(sc);

	reg = SFGPIO_READ(sc, SFGPIO_OUTPUT_VAL);

	if (orig_pins != NULL)
		/* Only input_val is implicitly masked by input_en */
		*orig_pins = SFGPIO_READ(sc, SFGPIO_INPUT_VAL) |
		     (reg & SFGPIO_READ(sc, SFGPIO_OUTPUT_EN));

	if ((clear_pins | change_pins) != 0)
		SFGPIO_WRITE(sc, SFGPIO_OUTPUT_VAL,
		    (reg & ~clear_pins) ^ change_pins);

	SFGPIO_UNLOCK(sc);

	return (0);
}

static int
sfgpio_pin_config_32(device_t dev, uint32_t first_pin, uint32_t num_pins,
    uint32_t *pin_flags)
{
	struct sfgpio_softc *sc;
	uint32_t ireg, oreg;
	int i;

	sc = device_get_softc(dev);

	if (first_pin != 0 || num_pins > sc->npins)
		return (EINVAL);

	SFGPIO_LOCK(sc);

	ireg = SFGPIO_READ(sc, SFGPIO_INPUT_EN);
	oreg = SFGPIO_READ(sc, SFGPIO_OUTPUT_EN);
	for (i = 0; i < num_pins; ++i) {
		if (pin_flags[i] & GPIO_PIN_INPUT) {
			ireg |= (1u << i);
			oreg &= ~(1u << i);
			sc->gpio_pins[i].gp_flags |= GPIO_PIN_INPUT;
			sc->gpio_pins[i].gp_flags &= ~GPIO_PIN_OUTPUT;
		} else if (pin_flags[i] & GPIO_PIN_OUTPUT) {
			ireg &= ~(1u << i);
			oreg |= (1u << i);
			sc->gpio_pins[i].gp_flags &= ~GPIO_PIN_INPUT;
			sc->gpio_pins[i].gp_flags |= GPIO_PIN_OUTPUT;
		}
	}
	SFGPIO_WRITE(sc, SFGPIO_INPUT_EN, ireg);
	SFGPIO_WRITE(sc, SFGPIO_OUTPUT_EN, oreg);

	SFGPIO_UNLOCK(sc);

	return (0);
}

static phandle_t
sfgpio_get_node(device_t bus, device_t dev)
{
	return (ofw_bus_get_node(bus));
}

static device_method_t sfgpio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sfgpio_probe),
	DEVMETHOD(device_attach,	sfgpio_attach),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus,		sfgpio_get_bus),
	DEVMETHOD(gpio_pin_max,		sfgpio_pin_max),
	DEVMETHOD(gpio_pin_set,		sfgpio_pin_set),
	DEVMETHOD(gpio_pin_get,		sfgpio_pin_get),
	DEVMETHOD(gpio_pin_toggle,	sfgpio_pin_toggle),
	DEVMETHOD(gpio_pin_getcaps,	sfgpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	sfgpio_pin_getflags),
	DEVMETHOD(gpio_pin_getname,	sfgpio_pin_getname),
	DEVMETHOD(gpio_pin_setflags,	sfgpio_pin_setflags),
	DEVMETHOD(gpio_pin_access_32,	sfgpio_pin_access_32),
	DEVMETHOD(gpio_pin_config_32,	sfgpio_pin_config_32),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	sfgpio_get_node),

	DEVMETHOD_END
};

DEFINE_CLASS_0(gpio, sfgpio_driver, sfgpio_methods,
    sizeof(struct sfgpio_softc));
EARLY_DRIVER_MODULE(gpio, simplebus, sfgpio_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
MODULE_DEPEND(sfgpio, gpiobus, 1, 1, 1);
