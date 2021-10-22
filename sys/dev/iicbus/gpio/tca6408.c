/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Alstom Group.
 * Copyright (c) 2021 Semihalf.
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
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/ofw/ofw_bus.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include <dev/gpio/gpiobusvar.h>

#define BIT(x)				(1UL << (x))

#define TCA6408_READ_REG		0x0
#define TCA6408_WRITE_REG		0x1
#define TCA6408_POLARITY_REG		0x2
#define TCA6408_CONFIG_REG		0x3

#define PINS_NUM			8

#define PIN_CAPS			(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT \
    | GPIO_PIN_INVIN)

struct tca6408_softc {
	device_t	dev;
	device_t	bus_dev;
	struct mtx	mtx;
};

static int tca6408_read1(device_t dev, uint8_t offset, uint8_t *data);
static int tca6408_write1(device_t dev, uint8_t offset, uint8_t data);
static int tca6408_probe(device_t dev);
static int tca6408_attach(device_t dev);
static int tca6408_detach(device_t dev);
static device_t tca6408_get_bus(device_t dev);
static int tca6408_pin_max(device_t dev, int *pin);
static int tca6408_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags);
static int tca6408_pin_setflags(device_t dev, uint32_t pin, uint32_t flags);
static int tca6408_pin_getname(device_t dev, uint32_t pin, char *name);
static int tca6408_pin_get(device_t dev, uint32_t pin, unsigned int *val);
static int tca6408_pin_set(device_t dev, uint32_t pin, unsigned int val);
static int tca6408_pin_toggle(device_t dev, uint32_t pin);
static int tca6408_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps);

static device_method_t tca6408_methods[] = {
	DEVMETHOD(device_probe,		tca6408_probe),
	DEVMETHOD(device_attach,	tca6408_attach),
	DEVMETHOD(device_detach,	tca6408_detach),

	DEVMETHOD(gpio_get_bus,		tca6408_get_bus),
	DEVMETHOD(gpio_pin_max,		tca6408_pin_max),
	DEVMETHOD(gpio_pin_getflags,	tca6408_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,	tca6408_pin_setflags),
	DEVMETHOD(gpio_pin_getname,	tca6408_pin_getname),
	DEVMETHOD(gpio_pin_get,		tca6408_pin_get),
	DEVMETHOD(gpio_pin_set,		tca6408_pin_set),
	DEVMETHOD(gpio_pin_toggle,	tca6408_pin_toggle),
	DEVMETHOD(gpio_pin_getcaps,	tca6408_pin_getcaps),

	DEVMETHOD_END
};

static driver_t tca6408_driver = {
	"gpio",
	tca6408_methods,
	sizeof(struct tca6408_softc)
};

static struct ofw_compat_data tca6408_compat_data[] = {
	{ "ti,tca6408", 1 },
	{ NULL, 0}
};

devclass_t tca6408_devclass;

DRIVER_MODULE(tca6408, iicbus, tca6408_driver, tca6408_devclass, 0, 0);
IICBUS_FDT_PNP_INFO(tca6408_compat_data);

static int
tca6408_read1(device_t dev, uint8_t offset, uint8_t *data)
{
	int error;

	error = iicdev_readfrom(dev, offset, (void *) data, 1, IIC_WAIT);
	if (error != 0)
		device_printf(dev, "Failed to read from device\n");

	return (error);
}

static int
tca6408_write1(device_t dev, uint8_t offset, uint8_t data)
{
	int error;

	error = iicdev_writeto(dev, offset, (void *) &data, 1, IIC_WAIT);
	if (error != 0)
		device_printf(dev, "Failed to write to device\n");

	return (error);
}

static int
tca6408_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, tca6408_compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "TCA6408 GPIO expander");

	return (BUS_PROBE_DEFAULT);
}

static int
tca6408_attach(device_t dev)
{
	struct tca6408_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	mtx_init(&sc->mtx, "tca6408 gpio", "gpio", MTX_DEF);

	sc->bus_dev = gpiobus_attach_bus(dev);
	if (sc->bus_dev == NULL) {
		device_printf(dev, "Could not create busdev child\n");
		return (ENXIO);
	}

	OF_device_register_xref(OF_xref_from_node(ofw_bus_get_node(dev)), dev);

	return (0);
}

static int
tca6408_detach(device_t dev)
{
	struct tca6408_softc *sc;

	sc = device_get_softc(dev);

	if (sc->bus_dev != NULL)
		gpiobus_detach_bus(sc->bus_dev);

	mtx_destroy(&sc->mtx);

	return (0);
}

static device_t
tca6408_get_bus(device_t dev)
{
	struct tca6408_softc *sc;

	sc = device_get_softc(dev);

	return (sc->bus_dev);
}

static int
tca6408_pin_max(device_t dev, int *pin)
{

	if (pin == NULL)
		return (EINVAL);

	*pin = PINS_NUM - 1;

	return (0);
}

static int
tca6408_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct tca6408_softc *sc;
	uint8_t buffer;
	int error;

	if (pin >= PINS_NUM || flags == NULL)
		return (EINVAL);

	sc = device_get_softc(dev);

	error = tca6408_read1(dev, TCA6408_CONFIG_REG, &buffer);
	if (error != 0)
		return (error);

	*flags = (buffer & BIT(pin)) ? GPIO_PIN_INPUT : GPIO_PIN_OUTPUT;

	error = tca6408_read1(dev, TCA6408_POLARITY_REG, &buffer);
	if (error != 0)
		return (error);

	if (buffer & BIT(pin))
		*flags |= ((*flags & GPIO_PIN_INPUT)
		    ? GPIO_PIN_INVIN : GPIO_PIN_INVOUT);

	return (0);
}

static int
tca6408_pin_setflags(device_t dev, uint32_t pin, unsigned int flags)
{
	uint8_t config_buf, inv_buf;
	struct tca6408_softc *sc;
	int error;

	if (pin >= PINS_NUM)
		return (EINVAL);

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);

	error = tca6408_read1(dev, TCA6408_CONFIG_REG, &config_buf);
	if (error != 0)
		goto fail;

	error = tca6408_read1(dev, TCA6408_POLARITY_REG, &inv_buf);
	if (error != 0)
		goto fail;

	if (flags & GPIO_PIN_INPUT)
		config_buf |= BIT(pin);
	else if (flags & GPIO_PIN_OUTPUT)
		config_buf &= ~BIT(pin);

	if (flags & GPIO_PIN_INVIN)
		inv_buf |= BIT(pin);
	else if (flags & GPIO_PIN_INVOUT)
		inv_buf &= ~BIT(pin);

	error = tca6408_write1(dev, TCA6408_CONFIG_REG, config_buf);
	if (error != 0)
		goto fail;

	error = tca6408_write1(dev, TCA6408_POLARITY_REG, inv_buf);

fail:
	mtx_unlock(&sc->mtx);

	return (error);
}

static int
tca6408_pin_getname(device_t dev, uint32_t pin, char *name)
{

	if (pin >= PINS_NUM)
		return (EINVAL);

	snprintf(name, GPIOMAXNAME, "tca6408_gpio_pin%d\n", pin);

	return (0);
}

static int
tca6408_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	uint8_t buffer;
	int error;

	if (pin >= PINS_NUM || val == NULL)
		return (EINVAL);

	error = tca6408_read1(dev, TCA6408_READ_REG, &buffer);
	if (error != 0)
		return (error);

	*val = buffer & BIT(pin);

	return (0);
}

static int
tca6408_pin_set(device_t dev, uint32_t pin, uint32_t val)
{
	struct tca6408_softc *sc;
	uint8_t buffer;
	int error;

	if (pin >= PINS_NUM)
		return (EINVAL);

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);

	error = tca6408_read1(dev, TCA6408_WRITE_REG, &buffer);
	if (error != 0)
		goto fail;

	if (val != 0)
		buffer |= BIT(pin);
	else
		buffer &= ~BIT(pin);

	error = tca6408_write1(dev, TCA6408_WRITE_REG, buffer);

fail:
	mtx_unlock(&sc->mtx);

	return (error);
}

static int
tca6408_pin_toggle(device_t dev, uint32_t pin)
{
	struct tca6408_softc *sc;
	uint8_t buffer;
	int error;

	if (pin >= PINS_NUM)
		return (EINVAL);

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);

	error = tca6408_read1(dev, TCA6408_WRITE_REG, &buffer);
	if (error)
		goto fail;

	buffer ^= BIT(pin);
	error = tca6408_write1(dev, TCA6408_WRITE_REG, buffer);

fail:
	mtx_unlock(&sc->mtx);

	return (error);
}

static int
tca6408_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{

	if (pin >= PINS_NUM)
		return (EINVAL);

	*caps = PIN_CAPS;

	return (0);
}

