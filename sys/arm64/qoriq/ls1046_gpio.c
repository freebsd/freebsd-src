/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Alstom Group.
 * Copyright (c) 2020 Semihalf.
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

#include <sys/param.h>

#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>
#include <machine/bus.h>

#include "gpio_if.h"

/* constants */
enum {
	DIRECTION  = 0x0,
	OPEN_DRAIN = 0x4,
	DATA       = 0x8,
	INT_EV     = 0xC,
	INT_MASK   = 0x10,
	INT_CTRL   = 0x14
};

#define	PIN_COUNT 32
#define	DEFAULT_CAPS				\
	(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT |	\
	GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL)
#define	GPIO(n) (1 << (31 - (n)))

struct gpio_res {
	int mem_rid;
	struct resource *mem_res;
};

/* software context */
struct gpio_softc {
	device_t           dev;
	device_t           busdev;
	struct gpio_res    res;
	struct gpio_pin    setup[PIN_COUNT];
	struct mtx         mutex;
};

#define	QORIQ_GPIO_LOCK(_sc)          mtx_lock_spin(&(_sc)->mutex)
#define	QORIQ_GPIO_UNLOCK(_sc)        mtx_unlock_spin(&(_sc)->mutex)
#define	QORIQ_GPIO_ASSERT_LOCKED(_sc) mtx_assert(&(_sc)->mutex, MA_OWNED)

/* prototypes */
/* helpers */
static int qoriq_make_gpio_res(device_t, struct gpio_res*);
static uint32_t qoriq_gpio_reg_read(device_t, uint32_t);
static void qoriq_gpio_reg_write(device_t, uint32_t, uint32_t);
static void qoriq_gpio_reg_set(device_t, uint32_t, uint32_t);
static void qoriq_gpio_reg_clear(device_t, uint32_t, uint32_t);
static void qoriq_gpio_out_en(device_t, uint32_t, uint8_t);
static void qoriq_gpio_value_set(device_t, uint32_t, uint8_t);
static uint32_t qoriq_gpio_value_get(device_t, uint32_t);
static void qoriq_gpio_open_drain_set(device_t, uint32_t, uint8_t);
static int qoriq_gpio_configure(device_t, uint32_t, uint32_t);

/* GPIO API */
static int qoriq_gpio_probe(device_t);
static int qoriq_gpio_attach(device_t);
static device_t qoriq_gpio_get_bus(device_t);
static int qoriq_gpio_pin_max(device_t, int*);
static int qoriq_gpio_pin_getname(device_t, uint32_t, char*);
static int qoriq_gpio_pin_getflags(device_t, uint32_t, uint32_t*);
static int qoriq_gpio_pin_setflags(device_t, uint32_t, uint32_t);
static int qoriq_gpio_pin_getcaps(device_t, uint32_t, uint32_t*);
static int qoriq_gpio_pin_get(device_t, uint32_t, uint32_t*);
static int qoriq_gpio_pin_set(device_t, uint32_t, uint32_t);
static int qoriq_gpio_pin_toggle(device_t, uint32_t);
static int qoriq_gpio_map_gpios(device_t, phandle_t, phandle_t,
    int, pcell_t*, uint32_t*, uint32_t*);
static int qoriq_gpio_pin_access_32(device_t, uint32_t, uint32_t, uint32_t,
    uint32_t*);
static int qoriq_gpio_pin_config_32(device_t, uint32_t, uint32_t, uint32_t*);

static device_method_t qoriq_gpio_methods[] = {
	DEVMETHOD(device_probe,		qoriq_gpio_probe),
	DEVMETHOD(device_attach,	qoriq_gpio_attach),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus,		qoriq_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,		qoriq_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname,	qoriq_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags,	qoriq_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,	qoriq_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_getcaps,	qoriq_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_get,		qoriq_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		qoriq_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	qoriq_gpio_pin_toggle),
	DEVMETHOD(gpio_map_gpios,	qoriq_gpio_map_gpios),
	DEVMETHOD(gpio_pin_access_32,	qoriq_gpio_pin_access_32),
	DEVMETHOD(gpio_pin_config_32,	qoriq_gpio_pin_config_32),

	DEVMETHOD_END
};

static driver_t gpio_driver = {
	"gpio",
	qoriq_gpio_methods,
	sizeof(struct gpio_softc),
};

static devclass_t gpio_devclass;

DRIVER_MODULE(gpio, simplebus, gpio_driver, gpio_devclass, 0, 0);
MODULE_VERSION(gpio, 1);

/*
 * helpers
 */
static int
qoriq_make_gpio_res(device_t dev, struct gpio_res *out)
{

	out->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &out->mem_rid, RF_ACTIVE | RF_SHAREABLE);

	if(out->mem_res == NULL) {
		return (1);
	} else {
		return (0);
	}
}

static uint32_t
qoriq_gpio_reg_read(device_t dev, uint32_t reg)
{
	struct gpio_softc *sc = device_get_softc(dev);
	uint32_t result;

	QORIQ_GPIO_ASSERT_LOCKED(sc);
	result = bus_read_4(sc->res.mem_res, reg);
	return be32toh(result);
}

static void
qoriq_gpio_reg_write(device_t dev, uint32_t reg, uint32_t val)
{
	struct gpio_softc *sc = device_get_softc(dev);

	QORIQ_GPIO_ASSERT_LOCKED(sc);
	val = htobe32(val);

	bus_write_4(sc->res.mem_res, reg, val);
	bus_barrier(sc->res.mem_res, reg, 4, BUS_SPACE_BARRIER_READ
	    | BUS_SPACE_BARRIER_WRITE);
}

static void
qoriq_gpio_reg_set(device_t dev, uint32_t reg, uint32_t pin)
{
	uint32_t reg_val;

	reg_val = qoriq_gpio_reg_read(dev, reg);
	reg_val |= GPIO(pin);
	qoriq_gpio_reg_write(dev, reg, reg_val);
}

static void
qoriq_gpio_reg_clear(device_t dev, uint32_t reg, uint32_t pin)
{
	uint32_t reg_val;

	reg_val = qoriq_gpio_reg_read(dev, reg);
	reg_val &= ~(GPIO(pin));
	qoriq_gpio_reg_write(dev, reg, reg_val);
}

static void
qoriq_gpio_out_en(device_t dev, uint32_t pin, uint8_t enable)
{

	if (pin >= PIN_COUNT)
		return;

	if (enable) {
		qoriq_gpio_reg_set(dev, DIRECTION, pin);
	} else {
		qoriq_gpio_reg_clear(dev, DIRECTION, pin);
	}
}

static void
qoriq_gpio_value_set(device_t dev, uint32_t pin, uint8_t val)
{

	if (pin >= PIN_COUNT)
		return;

	if (val) {
		qoriq_gpio_reg_set(dev, DATA, pin);
	} else {
		qoriq_gpio_reg_clear(dev, DATA, pin);
	}
}

static uint32_t
qoriq_gpio_value_get(device_t dev, uint32_t pin)
{
	uint32_t reg_val;

	if (pin >= PIN_COUNT)
		return (0);

	reg_val = qoriq_gpio_reg_read(dev, DATA);
	return ((reg_val & GPIO(pin)) == 0 ? 0 : 1);
}

static void
qoriq_gpio_open_drain_set(device_t dev, uint32_t pin, uint8_t val)
{

	if (pin >= PIN_COUNT) {
		return;
	}

	if (val) {
		qoriq_gpio_reg_set(dev, OPEN_DRAIN, pin);
	} else {
		qoriq_gpio_reg_clear(dev, OPEN_DRAIN, pin);
	}
}

static int
qoriq_gpio_configure(device_t dev, uint32_t pin, uint32_t flags)
{
	struct gpio_softc *sc = device_get_softc(dev);
	uint32_t newflags;

	if (pin >= PIN_COUNT) {
		return (EINVAL);
	}

	/*
	 * Pin cannot function as input and output at the same time.
	 * The same applies to open-drain and push-pull functionality.
	 */
	if (((flags & GPIO_PIN_INPUT) && (flags & GPIO_PIN_OUTPUT))
	    || ((flags & GPIO_PIN_OPENDRAIN) && (flags & GPIO_PIN_PUSHPULL))) {
		return (EINVAL);
	}

	QORIQ_GPIO_ASSERT_LOCKED(sc);

	if (flags & GPIO_PIN_INPUT) {
		newflags = GPIO_PIN_INPUT;
		qoriq_gpio_out_en(dev, pin, 0);
	}

	if (flags & GPIO_PIN_OUTPUT) {
		newflags = GPIO_PIN_OUTPUT;
		qoriq_gpio_out_en(dev, pin, 1);

		if (flags & GPIO_PIN_OPENDRAIN) {
			newflags |= GPIO_PIN_OPENDRAIN;
			qoriq_gpio_open_drain_set(dev, pin, 1);
		} else {
			newflags |= GPIO_PIN_PUSHPULL;
			qoriq_gpio_open_drain_set(dev, pin, 0);
		}
	}

	sc->setup[pin].gp_flags = newflags;

	return (0);
}

/* GPIO API */
static int
qoriq_gpio_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev)) {
		return (ENXIO);
	}

	if (!ofw_bus_is_compatible(dev, "fsl,qoriq-gpio")) {
		return (ENXIO);
	}

	device_set_desc(dev, "Integrated GPIO Controller");
	return (0);
}

static int
qoriq_gpio_attach(device_t dev)
{
	struct gpio_softc *sc = device_get_softc(dev);
	int i;

	if(qoriq_make_gpio_res(dev, &sc->res) != 0) {
		return (ENXIO);
	}

	for(i = 0; i < PIN_COUNT; i++) {
		sc->setup[i].gp_caps = DEFAULT_CAPS;
	}

	sc->dev = dev;

	sc->busdev = gpiobus_attach_bus(dev);
	if(sc->busdev == NULL) {
		return (ENXIO);
	}

	return (0);
}

static device_t
qoriq_gpio_get_bus(device_t dev)
{
	struct gpio_softc *softc = device_get_softc(dev);

	return (softc->busdev);
}

static int
qoriq_gpio_pin_max(device_t dev, int *maxpin)
{

	if(maxpin == NULL) {
		return (EINVAL);
	}

	*maxpin = PIN_COUNT - 1;
	return (0);
}

static int
qoriq_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{

	if(name == NULL || pin >= PIN_COUNT) {
		return (EINVAL);
	}

	snprintf(name, GPIOMAXNAME, "pin %d", pin);

	return (0);
}

static int
qoriq_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *pflags)
{
	struct gpio_softc *sc = device_get_softc(dev);

	if (pflags == NULL || pin >= PIN_COUNT) {
		return (EINVAL);
	}

	QORIQ_GPIO_LOCK(sc);
	*pflags = sc->setup[pin].gp_flags;
	QORIQ_GPIO_UNLOCK(sc);

	return (0);
}

static int
qoriq_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct gpio_softc *sc = device_get_softc(dev);
	int ret;

	if (pin >= PIN_COUNT)
		return (EINVAL);

	/* Check for unwanted flags. */
	QORIQ_GPIO_LOCK(sc);
	if ((flags & sc->setup[pin].gp_caps) != flags) {
		QORIQ_GPIO_UNLOCK(sc);
		return (EINVAL);
	}

	ret = qoriq_gpio_configure(dev, pin, flags);

	QORIQ_GPIO_UNLOCK(sc);
	return (ret);
}

static int
qoriq_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct gpio_softc *sc = device_get_softc(dev);

	if (caps == NULL || pin >= PIN_COUNT) {
		return (EINVAL);
	}

	QORIQ_GPIO_LOCK(sc);
	*caps = sc->setup[pin].gp_caps;
	QORIQ_GPIO_UNLOCK(sc);

	return (0);
}

static int
qoriq_gpio_pin_get(device_t dev, uint32_t pin, uint32_t *value)
{
	struct gpio_softc *sc = device_get_softc(dev);

	if (value == NULL || pin >= PIN_COUNT) {
		return (EINVAL);
	}

	QORIQ_GPIO_LOCK(sc);
	*value = qoriq_gpio_value_get(dev, pin);
	QORIQ_GPIO_UNLOCK(sc);
	return (0);
}

static int
qoriq_gpio_pin_set(device_t dev, uint32_t pin, uint32_t value)
{
	struct gpio_softc *sc = device_get_softc(dev);

	if (pin >= PIN_COUNT) {
		return (EINVAL);
	}

	QORIQ_GPIO_LOCK(sc);
	qoriq_gpio_value_set(dev, pin, value);
	QORIQ_GPIO_UNLOCK(sc);
	return (0);
}

static int
qoriq_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct gpio_softc *sc;
	uint32_t value;

	if (pin >= PIN_COUNT) {
		return (EINVAL);
	}

	sc = device_get_softc(dev);

	QORIQ_GPIO_LOCK(sc);
	value = qoriq_gpio_reg_read(dev, DATA);
	if (value & (1 << pin))
		value &= ~(1 << pin);
	else
		value |= (1 << pin);
	qoriq_gpio_reg_write(dev, DATA, value);
	QORIQ_GPIO_UNLOCK(sc);

	return (0);
}

static int
qoriq_gpio_map_gpios(device_t bus, phandle_t dev, phandle_t gparent, int gcells,
    pcell_t *gpios, uint32_t *pin, uint32_t *flags)
{
	struct gpio_softc *sc = device_get_softc(bus);
	int err;

	if (gpios[0] >= PIN_COUNT)
		return (EINVAL);

	QORIQ_GPIO_LOCK(sc);
	err = qoriq_gpio_configure(bus, gpios[0], gpios[1]);
	QORIQ_GPIO_UNLOCK(sc);

	if(err == 0) {
		*pin = gpios[0];
		*flags = gpios[1];
	}

	return (err);
}

static int
qoriq_gpio_pin_access_32(device_t dev, uint32_t first_pin, uint32_t clear_pins,
    uint32_t change_pins, uint32_t *orig_pins)
{
	struct gpio_softc *sc;
	uint32_t hwstate;

	sc = device_get_softc(dev);

	if (first_pin != 0)
		return (EINVAL);

	QORIQ_GPIO_LOCK(sc);
	hwstate = qoriq_gpio_reg_read(dev, DATA);
	qoriq_gpio_reg_write(dev, DATA, (hwstate & ~clear_pins) ^ change_pins);
	QORIQ_GPIO_UNLOCK(sc);

	if (orig_pins != NULL)
		*orig_pins = hwstate;

	return (0);
}

static int
qoriq_gpio_pin_config_32(device_t dev, uint32_t first_pin, uint32_t num_pins,
    uint32_t *pin_flags)
{
	uint32_t dir, odr, mask, reg;
	struct gpio_softc *sc;
	uint32_t newflags[32];
	int i;

	if (first_pin != 0 || num_pins > PIN_COUNT)
		return (EINVAL);

	sc = device_get_softc(dev);

	dir = 0;
	odr = 0;
	mask = 0;

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

	QORIQ_GPIO_LOCK(sc);
	reg = qoriq_gpio_reg_read(dev, DIRECTION);
	reg &= ~mask;
	reg |= dir;
	qoriq_gpio_reg_write(dev, DIRECTION, reg);
	reg = qoriq_gpio_reg_read(dev, OPEN_DRAIN);
	reg &= ~mask;
	reg |= odr;
	qoriq_gpio_reg_write(dev, OPEN_DRAIN, reg);
	for (i = 0; i < num_pins; i++) {
		sc->setup[i].gp_flags = newflags[i];
	}
	QORIQ_GPIO_UNLOCK(sc);

	return (0);
}
