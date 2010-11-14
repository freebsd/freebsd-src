/*-
 * Copyright (c) 2009, Oleksandr Tymoshenko <gonzo@FreeBSD.org>
 * Copyright (c) 2009, Luiz Otavio O Souza.
 * Copyright (c) 2010, Andrew Thompson <thompsa@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * GPIO driver for Gateworks Avilia
 */

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

#include <machine/bus.h>
#include <machine/resource.h>
#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>

#include "gpio_if.h"

#define GPIO_SET_BITS(sc, reg, bits)	\
	GPIO_CONF_WRITE_4(sc, reg, GPIO_CONF_READ_4(sc, (reg)) | (bits))

#define GPIO_CLEAR_BITS(sc, reg, bits)	\
	GPIO_CONF_WRITE_4(sc, reg, GPIO_CONF_READ_4(sc, (reg)) & ~(bits))

struct avila_gpio_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_gpio_ioh;
	uint32_t		sc_valid;
	struct gpio_pin		sc_pins[IXP4XX_GPIO_PINS];
};

struct avila_gpio_pin {
	const char *name;
	int pin;
	int caps;
};

#define	GPIO_PIN_IO	(GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)
static struct avila_gpio_pin avila_gpio_pins[] = {
	{ "GPIO0", 0, GPIO_PIN_IO },
	{ "GPIO1", 1, GPIO_PIN_IO },
	{ "GPIO2", 2, GPIO_PIN_IO },
	{ "GPIO3", 3, GPIO_PIN_IO },
	{ "GPIO4", 4, GPIO_PIN_IO },
	/*
	 * The following pins are connected to system devices and should not
	 * really be frobbed.
	 */
#if 0
	{ "SER_ENA", 5, GPIO_PIN_IO },
	{ "I2C_SCL", 6, GPIO_PIN_IO },
	{ "I2C_SDA", 7, GPIO_PIN_IO },
	{ "PCI_INTD", 8, GPIO_PIN_IO },
	{ "PCI_INTC", 9, GPIO_PIN_IO },
	{ "PCI_INTB", 10, GPIO_PIN_IO },
	{ "PCI_INTA", 11, GPIO_PIN_IO },
	{ "ATA_INT", 12, GPIO_PIN_IO },
	{ "PCI_RST", 13, GPIO_PIN_IO },
	{ "PCI_CLK", 14, GPIO_PIN_OUTPUT },
	{ "EX_CLK", 15, GPIO_PIN_OUTPUT },
#endif
};
#undef GPIO_PIN_IO

/*
 * Helpers
 */
static void avila_gpio_pin_configure(struct avila_gpio_softc *sc, 
    struct gpio_pin *pin, uint32_t flags);
static int  avila_gpio_pin_flags(struct avila_gpio_softc *sc, uint32_t pin);

/*
 * Driver stuff
 */
static int avila_gpio_probe(device_t dev);
static int avila_gpio_attach(device_t dev);
static int avila_gpio_detach(device_t dev);

/*
 * GPIO interface
 */
static int avila_gpio_pin_max(device_t dev, int *maxpin);
static int avila_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps);
static int avila_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t
    *flags);
static int avila_gpio_pin_getname(device_t dev, uint32_t pin, char *name);
static int avila_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags);
static int avila_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value);
static int avila_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val);
static int avila_gpio_pin_toggle(device_t dev, uint32_t pin);

static int
avila_gpio_pin_flags(struct avila_gpio_softc *sc, uint32_t pin)
{
	uint32_t v;

	v = GPIO_CONF_READ_4(sc, IXP425_GPIO_GPINR) & (1 << pin);

	return (v ? GPIO_PIN_INPUT : GPIO_PIN_OUTPUT);
}

static void
avila_gpio_pin_configure(struct avila_gpio_softc *sc, struct gpio_pin *pin,
    unsigned int flags)
{
	uint32_t mask;

	mask = 1 << pin->gp_pin;

	/*
	 * Manage input/output
	 */
	if (flags & (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)) {
		IXP4XX_GPIO_LOCK(sc);
		pin->gp_flags &= ~(GPIO_PIN_INPUT|GPIO_PIN_OUTPUT);
		if (flags & GPIO_PIN_OUTPUT) {
			pin->gp_flags |= GPIO_PIN_OUTPUT;
			GPIO_CLEAR_BITS(sc, IXP425_GPIO_GPOER, mask);
		}
		else {
			pin->gp_flags |= GPIO_PIN_INPUT;
			GPIO_SET_BITS(sc, IXP425_GPIO_GPOER, mask);
		}
		IXP4XX_GPIO_UNLOCK(sc);
	}
}

static int
avila_gpio_pin_max(device_t dev, int *maxpin)
{

	*maxpin = IXP4XX_GPIO_PINS - 1;
	return (0);
}

static int
avila_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct avila_gpio_softc *sc = device_get_softc(dev);

	if (pin >= IXP4XX_GPIO_PINS || !(sc->sc_valid & (1 << pin)))
		return (EINVAL);

	*caps = sc->sc_pins[pin].gp_caps;
	return (0);
}

static int
avila_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct avila_gpio_softc *sc = device_get_softc(dev);

	if (pin >= IXP4XX_GPIO_PINS || !(sc->sc_valid & (1 << pin)))
		return (EINVAL);

	IXP4XX_GPIO_LOCK(sc);
	/* refresh since we do not own all the pins */
	sc->sc_pins[pin].gp_flags = avila_gpio_pin_flags(sc, pin);
	*flags = sc->sc_pins[pin].gp_flags;
	IXP4XX_GPIO_UNLOCK(sc);

	return (0);
}

static int
avila_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct avila_gpio_softc *sc = device_get_softc(dev);

	if (pin >= IXP4XX_GPIO_PINS || !(sc->sc_valid & (1 << pin)))
		return (EINVAL);

	memcpy(name, sc->sc_pins[pin].gp_name, GPIOMAXNAME);
	return (0);
}

static int
avila_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct avila_gpio_softc *sc = device_get_softc(dev);
	uint32_t mask = 1 << pin;

	if (pin >= IXP4XX_GPIO_PINS || !(sc->sc_valid & mask))
		return (EINVAL);

	/* Filter out unwanted flags */
	if ((flags &= sc->sc_pins[pin].gp_caps) != flags)
		return (EINVAL);

	/* Can't mix input/output together */
	if ((flags & (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)) ==
	    (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT))
		return (EINVAL);

	avila_gpio_pin_configure(sc, &sc->sc_pins[pin], flags);
	return (0);
}

static int
avila_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct avila_gpio_softc *sc = device_get_softc(dev);
	uint32_t mask = 1 << pin;

	if (pin >= IXP4XX_GPIO_PINS || !(sc->sc_valid & mask))
		return (EINVAL);

	IXP4XX_GPIO_LOCK(sc);
	if (value)
		GPIO_SET_BITS(sc, IXP425_GPIO_GPOUTR, mask);
	else
		GPIO_CLEAR_BITS(sc, IXP425_GPIO_GPOUTR, mask);
	IXP4XX_GPIO_UNLOCK(sc);

	return (0);
}

static int
avila_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct avila_gpio_softc *sc = device_get_softc(dev);

	if (pin >= IXP4XX_GPIO_PINS || !(sc->sc_valid & (1 << pin)))
		return (EINVAL);

	IXP4XX_GPIO_LOCK(sc);
	*val = (GPIO_CONF_READ_4(sc, IXP425_GPIO_GPINR) & (1 << pin)) ? 1 : 0;
	IXP4XX_GPIO_UNLOCK(sc);

	return (0);
}

static int
avila_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct avila_gpio_softc *sc = device_get_softc(dev);
	uint32_t mask = 1 << pin;
	int res;

	if (pin >= IXP4XX_GPIO_PINS || !(sc->sc_valid & mask))
		return (EINVAL);

	IXP4XX_GPIO_LOCK(sc);
	res = GPIO_CONF_READ_4(sc, IXP425_GPIO_GPINR) & mask;
	if (res)
		GPIO_CLEAR_BITS(sc, IXP425_GPIO_GPOUTR, mask);
	else
		GPIO_SET_BITS(sc, IXP425_GPIO_GPOUTR, mask);
	IXP4XX_GPIO_UNLOCK(sc);

	return (0);
}

static int
avila_gpio_probe(device_t dev)
{

	device_set_desc(dev, "Gateworks Avila GPIO driver");
	return (0);
}

static int
avila_gpio_attach(device_t dev)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	struct avila_gpio_softc *sc = device_get_softc(dev);
	struct ixp425_softc *sa = device_get_softc(device_get_parent(dev));
	int i;

	sc->sc_dev = dev;
	sc->sc_iot = sa->sc_iot;
	sc->sc_gpio_ioh = sa->sc_gpio_ioh;

	for (i = 0; i < N(avila_gpio_pins); i++) {
		struct avila_gpio_pin *p = &avila_gpio_pins[i];

		strncpy(sc->sc_pins[p->pin].gp_name, p->name, GPIOMAXNAME);
		sc->sc_pins[p->pin].gp_pin = p->pin;
		sc->sc_pins[p->pin].gp_caps = p->caps;
		sc->sc_pins[p->pin].gp_flags = avila_gpio_pin_flags(sc, p->pin);
		sc->sc_valid |= 1 << p->pin;
	}

	device_add_child(dev, "gpioc", device_get_unit(dev));
	device_add_child(dev, "gpiobus", device_get_unit(dev));
	return (bus_generic_attach(dev));
#undef N
}

static int
avila_gpio_detach(device_t dev)
{

	bus_generic_detach(dev);

	return(0);
}

static device_method_t gpio_avila_methods[] = {
	DEVMETHOD(device_probe, avila_gpio_probe),
	DEVMETHOD(device_attach, avila_gpio_attach),
	DEVMETHOD(device_detach, avila_gpio_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_pin_max, avila_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname, avila_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags, avila_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps, avila_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags, avila_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get, avila_gpio_pin_get),
	DEVMETHOD(gpio_pin_set, avila_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle, avila_gpio_pin_toggle),
	{0, 0},
};

static driver_t gpio_avila_driver = {
	"gpio_avila",
	gpio_avila_methods,
	sizeof(struct avila_gpio_softc),
};
static devclass_t gpio_avila_devclass;

DRIVER_MODULE(gpio_avila, ixp, gpio_avila_driver, gpio_avila_devclass, 0, 0);
