/*-
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
 * GPIO driver for Gateworks Cambria
 *
 * Note:
 * The Cambria PLD does not set the i2c ack bit after each write, if we used the
 * regular iicbus interface it would abort the xfer after the address byte
 * times out and not write our latch. To get around this we grab the iicbus and
 * then do our own bit banging. This is a compromise to changing all the iicbb
 * device methods to allow a flag to be passed down and is similir to how Linux
 * does it.
 *
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

#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>
#include <arm/xscale/ixp425/ixdp425reg.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include "iicbb_if.h"
#include "gpio_if.h"

#define	IIC_M_WR	0	/* write operation */
#define	PLD_ADDR	0xac	/* slave address */

#define	I2C_DELAY	10

#define	GPIO_CONF_CLR(sc, reg, mask)	\
	GPIO_CONF_WRITE_4(sc, reg, GPIO_CONF_READ_4(sc, reg) &~ (mask))
#define	GPIO_CONF_SET(sc, reg, mask)	\
	GPIO_CONF_WRITE_4(sc, reg, GPIO_CONF_READ_4(sc, reg) | (mask))

#define	GPIO_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	GPIO_UNLOCK(_sc)	mtx_unlock(&(_sc)->sc_mtx)
#define	GPIO_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->sc_mtx, MA_OWNED)

#define	GPIO_PINS		5
struct cambria_gpio_softc {
	device_t		sc_dev;
	device_t		sc_busdev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_gpio_ioh;
        struct mtx		sc_mtx;
	struct gpio_pin		sc_pins[GPIO_PINS];
	uint8_t			sc_latch;
	uint8_t			sc_val;
};

struct cambria_gpio_pin {
	const char *name;
	int pin;
	int flags;
};

extern struct ixp425_softc *ixp425_softc;

static struct cambria_gpio_pin cambria_gpio_pins[GPIO_PINS] = {
	{ "PLD0", 0, GPIO_PIN_OUTPUT },
	{ "PLD1", 1, GPIO_PIN_OUTPUT },
	{ "PLD2", 2, GPIO_PIN_OUTPUT },
	{ "PLD3", 3, GPIO_PIN_OUTPUT },
	{ "PLD4", 4, GPIO_PIN_OUTPUT },
};

/*
 * Helpers
 */
static int cambria_gpio_read(struct cambria_gpio_softc *, uint32_t, unsigned int *);
static int cambria_gpio_write(struct cambria_gpio_softc *);

/*
 * Driver stuff
 */
static int cambria_gpio_probe(device_t dev);
static int cambria_gpio_attach(device_t dev);
static int cambria_gpio_detach(device_t dev);

/*
 * GPIO interface
 */
static device_t cambria_gpio_get_bus(device_t);
static int cambria_gpio_pin_max(device_t dev, int *maxpin);
static int cambria_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps);
static int cambria_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t
    *flags);
static int cambria_gpio_pin_getname(device_t dev, uint32_t pin, char *name);
static int cambria_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags);
static int cambria_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value);
static int cambria_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val);
static int cambria_gpio_pin_toggle(device_t dev, uint32_t pin);

static int
i2c_getsda(struct cambria_gpio_softc *sc)
{
	uint32_t reg;

	IXP4XX_GPIO_LOCK();
	GPIO_CONF_SET(sc, IXP425_GPIO_GPOER, GPIO_I2C_SDA_BIT);

	reg = GPIO_CONF_READ_4(sc, IXP425_GPIO_GPINR);
	IXP4XX_GPIO_UNLOCK();
	return (reg & GPIO_I2C_SDA_BIT);
}

static void
i2c_setsda(struct cambria_gpio_softc *sc, int val)
{

	IXP4XX_GPIO_LOCK();
	GPIO_CONF_CLR(sc, IXP425_GPIO_GPOUTR, GPIO_I2C_SDA_BIT);
	if (val)
		GPIO_CONF_SET(sc, IXP425_GPIO_GPOER, GPIO_I2C_SDA_BIT);
	else
		GPIO_CONF_CLR(sc, IXP425_GPIO_GPOER, GPIO_I2C_SDA_BIT);
	IXP4XX_GPIO_UNLOCK();
	DELAY(I2C_DELAY);
}

static void
i2c_setscl(struct cambria_gpio_softc *sc, int val)
{

	IXP4XX_GPIO_LOCK();
	GPIO_CONF_CLR(sc, IXP425_GPIO_GPOUTR, GPIO_I2C_SCL_BIT);
	if (val)
		GPIO_CONF_SET(sc, IXP425_GPIO_GPOER, GPIO_I2C_SCL_BIT);
	else
		GPIO_CONF_CLR(sc, IXP425_GPIO_GPOER, GPIO_I2C_SCL_BIT);
	IXP4XX_GPIO_UNLOCK();
	DELAY(I2C_DELAY);
}

static void
i2c_sendstart(struct cambria_gpio_softc *sc)
{
	i2c_setsda(sc, 1);
	i2c_setscl(sc, 1);
	i2c_setsda(sc, 0);
	i2c_setscl(sc, 0);
}

static void
i2c_sendstop(struct cambria_gpio_softc *sc)
{
	i2c_setscl(sc, 1);
	i2c_setsda(sc, 1);
	i2c_setscl(sc, 0);
	i2c_setsda(sc, 0);
}

static void
i2c_sendbyte(struct cambria_gpio_softc *sc, u_char data)
{
	int i;

	for (i=7; i>=0; i--) {
		i2c_setsda(sc, data & (1<<i));
		i2c_setscl(sc, 1);
		i2c_setscl(sc, 0);
	}
	i2c_setscl(sc, 1);
	i2c_getsda(sc);
	i2c_setscl(sc, 0);
}

static u_char
i2c_readbyte(struct cambria_gpio_softc *sc)
{
	int i;
	unsigned char data=0;

	for (i=7; i>=0; i--)
	{
		i2c_setscl(sc, 1);
		if (i2c_getsda(sc))
			data |= (1<<i);
		i2c_setscl(sc, 0);
	}
	return data;
}

static int
cambria_gpio_read(struct cambria_gpio_softc *sc, uint32_t pin, unsigned int *val)
{
	device_t dev = sc->sc_dev;
	int error;

	error = iicbus_request_bus(device_get_parent(dev), dev,
	    IIC_DONTWAIT);
	if (error)
		return (error);

	i2c_sendstart(sc);
	i2c_sendbyte(sc, PLD_ADDR | LSB);
	*val = (i2c_readbyte(sc) & (1 << pin)) != 0;
	i2c_sendstop(sc);

	iicbus_release_bus(device_get_parent(dev), dev);

	return (0);
}

static int
cambria_gpio_write(struct cambria_gpio_softc *sc)
{
	device_t dev = sc->sc_dev;
	int error;

	error = iicbus_request_bus(device_get_parent(dev), dev,
	    IIC_DONTWAIT);
	if (error)
		return (error);

	i2c_sendstart(sc);
	i2c_sendbyte(sc, PLD_ADDR & ~LSB);
	i2c_sendbyte(sc, sc->sc_latch);
	i2c_sendstop(sc);

	iicbus_release_bus(device_get_parent(dev), dev);

	return (0);
}

static device_t
cambria_gpio_get_bus(device_t dev)
{
	struct cambria_gpio_softc *sc;

	sc = device_get_softc(dev);

	return (sc->sc_busdev);
}

static int
cambria_gpio_pin_max(device_t dev, int *maxpin)
{

	*maxpin = GPIO_PINS - 1;
	return (0);
}

static int
cambria_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct cambria_gpio_softc *sc = device_get_softc(dev);

	if (pin >= GPIO_PINS)
		return (EINVAL);

	*caps = sc->sc_pins[pin].gp_caps;
	return (0);
}

static int
cambria_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct cambria_gpio_softc *sc = device_get_softc(dev);

	if (pin >= GPIO_PINS)
		return (EINVAL);

	*flags = sc->sc_pins[pin].gp_flags;
	return (0);
}

static int
cambria_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct cambria_gpio_softc *sc = device_get_softc(dev);

	if (pin >= GPIO_PINS)
		return (EINVAL);

	memcpy(name, sc->sc_pins[pin].gp_name, GPIOMAXNAME);
	return (0);
}

static int
cambria_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct cambria_gpio_softc *sc = device_get_softc(dev);
	int error;
	uint8_t mask;

	mask = 1 << pin;

	if (pin >= GPIO_PINS)
		return (EINVAL);

	GPIO_LOCK(sc);
	sc->sc_pins[pin].gp_flags = flags;

	/*
	 * Writing a logical one sets the signal high and writing a logical
	 * zero sets the signal low. To configure a digital I/O signal as an
	 * input, a logical one must first be written to the data bit to
	 * three-state the associated output.
	 */
	if (flags & GPIO_PIN_INPUT || sc->sc_val & mask)
		sc->sc_latch |= mask; /* input or output & high */
	else
		sc->sc_latch &= ~mask;
	error = cambria_gpio_write(sc);
	GPIO_UNLOCK(sc);

	return (error);
}

static int
cambria_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct cambria_gpio_softc *sc = device_get_softc(dev);
	int error;
	uint8_t mask;

	mask = 1 << pin;

	if (pin >= GPIO_PINS)
		return (EINVAL);
	GPIO_LOCK(sc);
	if (value)
		sc->sc_val |= mask;
	else
		sc->sc_val &= ~mask;

	if (sc->sc_pins[pin].gp_flags != GPIO_PIN_OUTPUT) {
		/* just save, altering the latch will disable input */
		GPIO_UNLOCK(sc);
		return (0);
	}

	if (value)
		sc->sc_latch |= mask;
	else
		sc->sc_latch &= ~mask;
	error = cambria_gpio_write(sc);
	GPIO_UNLOCK(sc);

	return (error);
}

static int
cambria_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct cambria_gpio_softc *sc = device_get_softc(dev);
	int error = 0;

	if (pin >= GPIO_PINS)
		return (EINVAL);

	GPIO_LOCK(sc);
	if (sc->sc_pins[pin].gp_flags == GPIO_PIN_OUTPUT)
		*val = (sc->sc_latch & (1 << pin)) ? 1 : 0;
	else
		error = cambria_gpio_read(sc, pin, val);
	GPIO_UNLOCK(sc);

	return (error);
}

static int
cambria_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct cambria_gpio_softc *sc = device_get_softc(dev);
	int error = 0;

	if (pin >= GPIO_PINS)
		return (EINVAL);

	GPIO_LOCK(sc);
	sc->sc_val ^= (1 << pin);
	if (sc->sc_pins[pin].gp_flags == GPIO_PIN_OUTPUT) {
		sc->sc_latch ^= (1 << pin);
		error = cambria_gpio_write(sc);
	}
	GPIO_UNLOCK(sc);

	return (error);
}

static int
cambria_gpio_probe(device_t dev)
{

	device_set_desc(dev, "Gateworks Cambria GPIO driver");
	return (0);
}

static int
cambria_gpio_attach(device_t dev)
{
	struct cambria_gpio_softc *sc = device_get_softc(dev);
	int pin;

	sc->sc_dev = dev;
	sc->sc_iot = ixp425_softc->sc_iot;
	sc->sc_gpio_ioh = ixp425_softc->sc_gpio_ioh;

	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	for (pin = 0; pin < GPIO_PINS; pin++) {
		struct cambria_gpio_pin *p = &cambria_gpio_pins[pin];

		strncpy(sc->sc_pins[pin].gp_name, p->name, GPIOMAXNAME);
		sc->sc_pins[pin].gp_pin = pin;
		sc->sc_pins[pin].gp_caps = GPIO_PIN_INPUT|GPIO_PIN_OUTPUT;
		sc->sc_pins[pin].gp_flags = 0;
		cambria_gpio_pin_setflags(dev, pin, p->flags);
	}

	sc->sc_busdev = gpiobus_attach_bus(dev);
	if (sc->sc_busdev == NULL) {
		mtx_destroy(&sc->sc_mtx);
		return (ENXIO);
	}

	return (0);
}

static int
cambria_gpio_detach(device_t dev)
{
	struct cambria_gpio_softc *sc = device_get_softc(dev);

	KASSERT(mtx_initialized(&sc->sc_mtx), ("gpio mutex not initialized"));

	gpiobus_detach_bus(dev);
	mtx_destroy(&sc->sc_mtx);

	return(0);
}

static device_method_t cambria_gpio_methods[] = {
	DEVMETHOD(device_probe, cambria_gpio_probe),
	DEVMETHOD(device_attach, cambria_gpio_attach),
	DEVMETHOD(device_detach, cambria_gpio_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus, cambria_gpio_get_bus),
	DEVMETHOD(gpio_pin_max, cambria_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname, cambria_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags, cambria_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps, cambria_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags, cambria_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get, cambria_gpio_pin_get),
	DEVMETHOD(gpio_pin_set, cambria_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle, cambria_gpio_pin_toggle),
	{0, 0},
};

static driver_t cambria_gpio_driver = {
	"gpio",
	cambria_gpio_methods,
	sizeof(struct cambria_gpio_softc),
};
static devclass_t cambria_gpio_devclass;

DRIVER_MODULE(gpio_cambria, iicbus, cambria_gpio_driver, cambria_gpio_devclass, 0, 0);
MODULE_VERSION(gpio_cambria, 1);
MODULE_DEPEND(gpio_cambria, iicbus, 1, 1, 1);
