/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) Andriy Gapon
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Driver for PCF8574 / PCF8574A 8-bit I/O expander
 * with quasi-bidirectional I/O.
 * There is no separate mode / configuration register.
 * Pins are set and queried via a single register.
 * Because of that we have to maintain the state in the driver
 * and assume that there is no outside meddling with the device.
 * See the datasheet for details.
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
#include <sys/sx.h>

#ifdef FDT
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include <dev/gpio/gpiobusvar.h>

#include "gpio_if.h"

#define	NUM_PINS	8
#define	PIN_CAPS	(GPIO_PIN_OUTPUT | GPIO_PIN_INPUT)

#define	dbg_dev_printf(dev, fmt, args...)	\
	if (bootverbose) device_printf(dev, fmt, ##args)

struct pcf8574_softc {
	device_t	dev;
	device_t	busdev;
	struct sx	lock;
	uint8_t		addr;
	uint8_t		output_mask;
	uint8_t		output_state;
};

#ifdef FDT
static struct ofw_compat_data compat_data[] = {
	{ "nxp,pcf8574",	1 },
	{ "nxp,pcf8574a",	1 },
	{ NULL,			0 }
};
#endif

static int
pcf8574_read(struct pcf8574_softc *sc, uint8_t *val)
{
	struct iic_msg msg;
	int error;

	msg.slave = sc->addr;
	msg.flags = IIC_M_RD;
	msg.len = 1;
	msg.buf = val;

	error = iicbus_transfer_excl(sc->dev, &msg, 1, IIC_WAIT);
	return (iic2errno(error));
}

static int
pcf8574_write(struct pcf8574_softc *sc, uint8_t val)
{
	struct iic_msg msg;
	int error;

	msg.slave = sc->addr;
	msg.flags = IIC_M_WR;
	msg.len = 1;
	msg.buf = &val;

	error = iicbus_transfer_excl(sc->dev, &msg, 1, IIC_WAIT);
	return (iic2errno(error));
}

static int
pcf8574_probe(device_t dev)
{

#ifdef FDT
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);
#endif
	device_set_desc(dev, "PCF8574 I/O expander");
	return (BUS_PROBE_DEFAULT);
}

static int
pcf8574_attach(device_t dev)
{
	struct pcf8574_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->addr = iicbus_get_addr(dev);

	/* Treat everything as input because there is no way to tell. */
	sc->output_mask = 0;
	sc->output_state = 0xff;

	/* Put the device to a safe, known state. */
	(void)pcf8574_write(sc, 0xff);

	sx_init(&sc->lock, "pcf8574");
	sc->busdev = gpiobus_attach_bus(dev);
	if (sc->busdev == NULL) {
		device_printf(dev, "Could not create busdev child\n");
		sx_destroy(&sc->lock);
		return (ENXIO);
	}
	return (0);
}

static int
pcf8574_detach(device_t dev)
{
	struct pcf8574_softc *sc;

	sc = device_get_softc(dev);

	if (sc->busdev != NULL)
		gpiobus_detach_bus(sc->busdev);

	sx_destroy(&sc->lock);
	return (0);
}

static device_t
pcf8574_get_bus(device_t dev)
{
	struct pcf8574_softc *sc;

	sc = device_get_softc(dev);
	return (sc->busdev);
}

static int
pcf8574_pin_max(device_t dev __unused, int *maxpin)
{
	*maxpin = NUM_PINS - 1;
	return (0);
}

static int
pcf8574_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{

	if (pin >= NUM_PINS)
		return (EINVAL);
	*caps = PIN_CAPS;
	return (0);
}

static int
pcf8574_pin_getflags(device_t dev, uint32_t pin, uint32_t *pflags)
{
	struct pcf8574_softc *sc;
	uint8_t val, stale;
	int error;

	sc = device_get_softc(dev);

	if (pin >= NUM_PINS)
		return (EINVAL);

	sx_xlock(&sc->lock);
	error = pcf8574_read(sc, &val);
	if (error != 0) {
		dbg_dev_printf(dev, "failed to read from device: %d\n",
		    error);
		sx_xunlock(&sc->lock);
		return (error);
	}

	/*
	 * Check for pins whose read value is one, but they are configured
	 * as outputs with low signal.  This is an impossible combination,
	 * so change their view to be inputs.
	 */
	stale = val & sc->output_mask & ~sc->output_state;
	sc->output_mask &= ~stale;
	sc->output_state |= stale;

	if ((sc->output_mask & (1 << pin)) != 0)
		*pflags = GPIO_PIN_OUTPUT;
	else
		*pflags = GPIO_PIN_INPUT;
	sx_xunlock(&sc->lock);

	return (0);
}

static int
pcf8574_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct pcf8574_softc *sc;
	int error;
	uint8_t val;
	bool update_needed;

	sc = device_get_softc(dev);

	if (pin >= NUM_PINS)
		return (EINVAL);
	if ((flags & ~PIN_CAPS) != 0)
		return (EINVAL);

	sx_xlock(&sc->lock);
	if ((flags & GPIO_PIN_OUTPUT) != 0) {
		sc->output_mask |= 1 << pin;
		update_needed = false;
	} else if ((flags & GPIO_PIN_INPUT) != 0) {
		sc->output_mask &= ~(1 << pin);
		sc->output_state |= 1 << pin;
		update_needed = true;
	} else {
		KASSERT(false, ("both input and output modes requested"));
		update_needed = false;
	}

	if (update_needed) {
		val = sc->output_state | ~sc->output_mask;
		error = pcf8574_write(sc, val);
		if (error != 0)
			dbg_dev_printf(dev, "failed to write to device: %d\n",
			    error);
	}
	sx_xunlock(&sc->lock);

	return (0);
}

static int
pcf8574_pin_getname(device_t dev, uint32_t pin, char *name)
{

	if (pin >= NUM_PINS)
		return (EINVAL);
	snprintf(name, GPIOMAXNAME, "P%d", pin);
	return (0);
}

static int
pcf8574_pin_get(device_t dev, uint32_t pin, unsigned int *on)
{
	struct pcf8574_softc *sc;
	uint8_t val;
	int error;

	sc = device_get_softc(dev);

	sx_xlock(&sc->lock);
	if ((sc->output_mask & (1 << pin)) != 0) {
		*on = (sc->output_state & (1 << pin)) != 0;
		sx_xunlock(&sc->lock);
		return (0);
	}

	error = pcf8574_read(sc, &val);
	if (error != 0) {
		dbg_dev_printf(dev, "failed to read from device: %d\n", error);
		sx_xunlock(&sc->lock);
		return (error);
	}
	sx_xunlock(&sc->lock);

	*on = (val & (1 << pin)) != 0;
	return (0);
}

static int
pcf8574_pin_set(device_t dev, uint32_t pin, unsigned int on)
{
	struct pcf8574_softc *sc;
	uint8_t val;
	int error;

	sc = device_get_softc(dev);

	if (pin >= NUM_PINS)
		return (EINVAL);

	sx_xlock(&sc->lock);

	if ((sc->output_mask & (1 << pin)) == 0) {
		sx_xunlock(&sc->lock);
		return (EINVAL);
	}

	/*
	 * Algorithm:
	 * - set all outputs to their recorded state;
	 * - set all inputs to the high state;
	 * - apply the requested change.
	 */
	val = sc->output_state | ~sc->output_mask;
	val &= ~(1 << pin);
	val |= (on != 0) << pin;

	error = pcf8574_write(sc, val);
	if (error != 0) {
		dbg_dev_printf(dev, "failed to write to device: %d\n", error);
		sx_xunlock(&sc->lock);
		return (error);
	}

	/*
	 * NB: we can record anything as "output" state of input pins.
	 * By convention and for convenience it will be recorded as 1.
	 */
	sc->output_state = val;
	sx_xunlock(&sc->lock);
	return (0);
}

static int
pcf8574_pin_toggle(device_t dev, uint32_t pin)
{
	struct pcf8574_softc *sc;
	uint8_t val;
	int error;

	sc = device_get_softc(dev);

	if (pin >= NUM_PINS)
		return (EINVAL);

	sx_xlock(&sc->lock);

	if ((sc->output_mask & (1 << pin)) == 0) {
		sx_xunlock(&sc->lock);
		return (EINVAL);
	}

	val = sc->output_state | ~sc->output_mask;
	val ^= 1 << pin;

	error = pcf8574_write(sc, val);
	if (error != 0) {
		dbg_dev_printf(dev, "failed to write to device: %d\n", error);
		sx_xunlock(&sc->lock);
		return (error);
	}

	sc->output_state = val;
	sx_xunlock(&sc->lock);
	return (0);
}

static device_method_t pcf8574_methods[] = {
	DEVMETHOD(device_probe,		pcf8574_probe),
	DEVMETHOD(device_attach,	pcf8574_attach),
	DEVMETHOD(device_detach,	pcf8574_detach),

	/* GPIO methods */
	DEVMETHOD(gpio_get_bus,		pcf8574_get_bus),
	DEVMETHOD(gpio_pin_max,		pcf8574_pin_max),
	DEVMETHOD(gpio_pin_getcaps,	pcf8574_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	pcf8574_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,	pcf8574_pin_setflags),
	DEVMETHOD(gpio_pin_getname,	pcf8574_pin_getname),
	DEVMETHOD(gpio_pin_get,		pcf8574_pin_get),
	DEVMETHOD(gpio_pin_set,		pcf8574_pin_set),
	DEVMETHOD(gpio_pin_toggle,	pcf8574_pin_toggle),

	DEVMETHOD_END
};

static driver_t pcf8574_driver = {
	"gpio",
	pcf8574_methods,
	sizeof(struct pcf8574_softc)
};

DRIVER_MODULE(pcf8574, iicbus, pcf8574_driver, 0, 0);
MODULE_DEPEND(pcf8574, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
MODULE_DEPEND(pcf8574, gpiobus, 1, 1, 1);
MODULE_VERSION(pcf8574, 1);
#ifdef FDT
IICBUS_FDT_PNP_INFO(compat_data);
#endif
