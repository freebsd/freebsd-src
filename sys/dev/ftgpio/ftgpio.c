/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016-2023 Stormshield
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

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <machine/bus.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/superio/superio.h>

#include "gpio_if.h"

#define GPIO_LOCK_INIT(_sc)	mtx_init(&(_sc)->mtx,	\
		device_get_nameunit(dev), NULL, MTX_DEF)
#define GPIO_LOCK_DESTROY(_sc)		mtx_destroy(&(_sc)->mtx)
#define GPIO_LOCK(_sc)		mtx_lock(&(_sc)->mtx)
#define GPIO_UNLOCK(_sc)	mtx_unlock(&(_sc)->mtx)
#define GPIO_ASSERT_LOCKED(_sc)	mtx_assert(&(_sc)->mtx, MA_OWNED)
#define GPIO_ASSERT_UNLOCKED(_sc)	mtx_assert(&(_sc)->mtx, MA_NOTOWNED)

/* Global register set */
#define GPIO4_ENABLE 0x28
#define GPIO3_ENABLE 0x29
#define FULL_UR5_UR6 0x2A
#define GPIO1_ENABLE 0x2B
#define GPIO2_ENABLE 0x2C

/* Logical Device Numbers. */
#define FTGPIO_LDN_GPIO			0x06

#define FTGPIO_MAX_GROUP 6
#define FTGPIO_MAX_PIN   52

#define FTGPIO_IS_VALID_PIN(_p)  ((_p) >= 0 && (_p) <= FTGPIO_MAX_PIN)
#define FTGPIO_PIN_GETINDEX(_p) ((_p) & 7)
#define FTGPIO_PIN_GETGROUP(_p) ((_p) >> 3)

#define FTGPIO_GPIO_CAPS (GPIO_PIN_INPUT  | GPIO_PIN_OUTPUT    | GPIO_PIN_INVIN | \
                          GPIO_PIN_INVOUT | GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL)

#define GET_BIT(_v, _b) (((_v) >> (_b)) & 1)

#define FTGPIO_VERBOSE_PRINTF(dev, ...)         \
	do {                                        \
		if (__predict_false(bootverbose))       \
			device_printf(dev, __VA_ARGS__);    \
	} while (0)

/*
 * Note that the values are important.
 * They match actual register offsets.
 * See p71 and p72 of F81865's datasheet.
 */
#define REG_OUTPUT_ENABLE         0 /* Not for GPIO0 */
#define REG_OUTPUT_DATA           1
#define REG_PIN_STATUS            2
#define REG_DRIVE_ENABLE          3
#define REG_MODE_SELECT_1         4 /* Only for GPIO0 */
#define REG_MODE_SELECT_2         5 /* Only for GPIO0 */
#define REG_PULSE_WIDTH_SELECT_1  6 /* Only for GPIO0 */
#define REG_PULSE_WIDTH_SELECT_2  7 /* Only for GPIO0 */
#define REG_INTERRUPT_ENABLE      8 /* Only for GPIO0 */
#define REG_INTERRUPT_STATUS      9 /* Only for GPIO0 */

struct ftgpio_device {
	uint16_t    devid;
	const char *descr;
} ftgpio_devices[] = {
	{
		.devid = 0x0704,
		.descr = "Fintek F81865",
	},
};

struct ftgpio_softc {
	device_t			dev;
	device_t			busdev;
	struct mtx			mtx;
	struct gpio_pin		pins[FTGPIO_MAX_PIN + 1];
};

static uint8_t
ftgpio_group_get_ioreg(struct ftgpio_softc *sc, uint8_t reg, unsigned group)
{
	uint8_t ioreg;

	KASSERT((group == 0 && REG_OUTPUT_DATA <= reg && reg <= REG_INTERRUPT_STATUS) || \
	        (group >= 1 && reg <= REG_DRIVE_ENABLE),
		("%s: invalid register %u for group %u", __func__, reg, group));
	ioreg = (((0xf - group) << 4) + reg);
	return (ioreg);
}

static uint8_t
ftgpio_group_get_output(struct ftgpio_softc *sc, unsigned group)
{
	uint8_t ioreg, val;

	ioreg = ftgpio_group_get_ioreg(sc, REG_OUTPUT_DATA, group);
	val   = superio_read(sc->dev, ioreg);
	FTGPIO_VERBOSE_PRINTF(sc->dev, "group GPIO%u output is 0x%x (ioreg=0x%x)\n",
		group, val, ioreg);
	return (val);
}

static void
ftgpio_group_set_output(struct ftgpio_softc *sc, unsigned group, uint8_t group_value)
{
	uint8_t ioreg;

	ioreg = ftgpio_group_get_ioreg(sc, REG_OUTPUT_DATA, group);
	superio_write(sc->dev, ioreg, group_value);
	FTGPIO_VERBOSE_PRINTF(sc->dev, "set group GPIO%u output to 0x%x (ioreg=0x%x)\n",
		group, group_value, ioreg);
}

static uint8_t
ftgpio_group_get_status(struct ftgpio_softc *sc, unsigned group)
{
	uint8_t ioreg;

	ioreg = ftgpio_group_get_ioreg(sc, REG_PIN_STATUS, group);
	return (superio_read(sc->dev, ioreg));
}

static void
ftgpio_pin_write(struct ftgpio_softc *sc, uint32_t pin_num, bool pin_value)
{
	uint32_t pin_flags;
	uint8_t  val;
	unsigned group, index;

	GPIO_ASSERT_LOCKED(sc);
	index     = FTGPIO_PIN_GETINDEX(pin_num);
	group     = FTGPIO_PIN_GETGROUP(pin_num);
	pin_flags = sc->pins[pin_num].gp_flags;
	if ((pin_flags & (GPIO_PIN_OUTPUT)) == 0) {
		FTGPIO_VERBOSE_PRINTF(sc->dev, "pin %u<GPIO%u%u> is not configured for output\n",
			pin_num, group, index);
		return;
	}

	FTGPIO_VERBOSE_PRINTF(sc->dev, "set pin %u<GPIO%u%u> to %s\n",
		pin_num, group, index, (pin_value ? "on" : "off"));

	val = ftgpio_group_get_output(sc, group);
	if (!pin_value != !(pin_flags & GPIO_PIN_INVOUT))
		val |=  (1 << index);
	else
		val &= ~(1 << index);
	ftgpio_group_set_output(sc, group, val);
}

static bool
ftgpio_pin_read(struct ftgpio_softc *sc, uint32_t pin_num)
{
	uint32_t pin_flags;
	unsigned group, index;
	uint8_t  val;
	bool     pin_value;

	GPIO_ASSERT_LOCKED(sc);
	group     = FTGPIO_PIN_GETGROUP(pin_num);
	index     = FTGPIO_PIN_GETINDEX(pin_num);
	pin_flags = sc->pins[pin_num].gp_flags;
	if ((pin_flags & (GPIO_PIN_OUTPUT | GPIO_PIN_INPUT)) == 0) {
		FTGPIO_VERBOSE_PRINTF(sc->dev, "pin %u<GPIO%u%u> is not configured for input or output\n",
			pin_num, group, index);
		return (false);
	}

	if (pin_flags & GPIO_PIN_OUTPUT)
		val = ftgpio_group_get_output(sc, group);
	else
		val = ftgpio_group_get_status(sc, group);
	pin_value = GET_BIT(val, index);

	if (((pin_flags & (GPIO_PIN_OUTPUT|GPIO_PIN_INVOUT)) == (GPIO_PIN_OUTPUT|GPIO_PIN_INVOUT)) ||
	    ((pin_flags & (GPIO_PIN_INPUT |GPIO_PIN_INVIN )) == (GPIO_PIN_INPUT |GPIO_PIN_INVIN)))
		pin_value = !pin_value;
	FTGPIO_VERBOSE_PRINTF(sc->dev, "pin %u<GPIO%u%u> is %s\n",
		pin_num, group, index, (pin_value ? "on" : "off"));

	return (pin_value);
}

static void
ftgpio_pin_set_drive(struct ftgpio_softc *sc, uint32_t pin_num, bool pin_drive)
{
	unsigned group, index;
	uint8_t  group_drive, ioreg;

	index       = FTGPIO_PIN_GETINDEX(pin_num);
	group       = FTGPIO_PIN_GETGROUP(pin_num);
	ioreg		= ftgpio_group_get_ioreg(sc, REG_DRIVE_ENABLE, group);
	group_drive = superio_read(sc->dev, ioreg);
	FTGPIO_VERBOSE_PRINTF(sc->dev, "group GPIO%u drive is 0x%x (ioreg=0x%x)\n",
		group, group_drive, ioreg);

	if (pin_drive)
		group_drive |= (1 << index);   /* push pull */
	else
		group_drive &= ~(1 << index);  /* open drain */
	superio_write(sc->dev, ioreg, group_drive);
}

static bool
ftgpio_pin_is_pushpull(struct ftgpio_softc *sc, uint32_t pin_num)
{
	unsigned group, index;
	uint8_t  group_drive, ioreg;
	bool is_pushpull;

	index       = FTGPIO_PIN_GETINDEX(pin_num);
	group       = FTGPIO_PIN_GETGROUP(pin_num);

	ioreg		= ftgpio_group_get_ioreg(sc, REG_DRIVE_ENABLE, group);
	group_drive = superio_read(sc->dev, ioreg);
	FTGPIO_VERBOSE_PRINTF(sc->dev, "group GPIO%u drive is 0x%x (ioreg=0x%x)\n",
		group, group_drive, ioreg);

	is_pushpull = group_drive & (1 << index);
	FTGPIO_VERBOSE_PRINTF(sc->dev, "pin %u<GPIO%u%u> drive is %s\n",
		pin_num, group, index, (is_pushpull ? "pushpull" : "opendrain"));

	return (is_pushpull);
}

static void
ftgpio_pin_set_io(struct ftgpio_softc *sc, uint32_t pin_num, bool pin_io)
{
	unsigned group, index;
	uint8_t  group_io, ioreg;

	index = FTGPIO_PIN_GETINDEX(pin_num);
	group = FTGPIO_PIN_GETGROUP(pin_num);
	FTGPIO_VERBOSE_PRINTF(sc->dev, "set pin %u<GPIO%u%u> io to %s\n",
		pin_num, group, index, (pin_io ? "output" : "input"));

	ioreg    = ftgpio_group_get_ioreg(sc, REG_OUTPUT_ENABLE, group);
	group_io = superio_read(sc->dev, ioreg);
	FTGPIO_VERBOSE_PRINTF(sc->dev, "group GPIO%u io is 0x%x (ioreg=0x%x)\n",
		group, group_io, ioreg);
	if (pin_io)
		group_io |=  (1 << index); /* output */
	else
		group_io &= ~(1 << index); /* input */
	superio_write(sc->dev, ioreg, group_io);
	FTGPIO_VERBOSE_PRINTF(sc->dev, "set group GPIO%u io to 0x%x (ioreg=0x%x)\n",
		group, group_io, ioreg);
}

static bool
ftgpio_pin_is_output(struct ftgpio_softc *sc, uint32_t pin_num)
{
	unsigned group, index;
	bool is_output;

	index = FTGPIO_PIN_GETINDEX(pin_num);
	group = FTGPIO_PIN_GETGROUP(pin_num);

	is_output = ftgpio_group_get_status(sc, group) & (1 << index);
	FTGPIO_VERBOSE_PRINTF(sc->dev, "pin %u<GPIO%u%u> io is %s\n",
		pin_num, group, index, (is_output ? "output" : "input"));
	return (is_output);
}

static int
ftgpio_pin_setflags(struct ftgpio_softc *sc, uint32_t pin_num, uint32_t pin_flags)
{
	/* check flags consistency */
	if ((pin_flags & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) ==
		(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT))
		return (EINVAL);

	if ((pin_flags & (GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL)) ==
		(GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL))
		return (EINVAL);

	if (pin_flags & GPIO_PIN_OPENDRAIN)
		ftgpio_pin_set_drive(sc, pin_num, 0 /* open drain */);
	else if (pin_flags & GPIO_PIN_PUSHPULL)
		ftgpio_pin_set_drive(sc, pin_num, 1 /* push pull */);

	if (pin_flags & GPIO_PIN_INPUT)
		ftgpio_pin_set_io(sc, pin_num, 0 /* input */);
	else if (pin_flags & GPIO_PIN_OUTPUT)
		ftgpio_pin_set_io(sc, pin_num, 1 /* output */);

	sc->pins[pin_num].gp_flags = pin_flags;

	return (0);
}

static int
ftgpio_probe(device_t dev)
{
	uint16_t devid;
	int      i;

	if (superio_vendor(dev) != SUPERIO_VENDOR_FINTEK)
		return (ENXIO);
	if (superio_get_type(dev) != SUPERIO_DEV_GPIO)
		return (ENXIO);

	/*
	 * There are several GPIO devices, we attach only to one of them
	 * and use the rest without attaching.
	 */
	if (superio_get_ldn(dev) != FTGPIO_LDN_GPIO)
		return (ENXIO);

	devid = superio_devid(dev);
	for (i = 0; i < nitems(ftgpio_devices); i++) {
		if (devid == ftgpio_devices[i].devid) {
			device_set_desc(dev, ftgpio_devices[i].descr);
			return (BUS_PROBE_DEFAULT);
		}
	}
	return (ENXIO);
}

static int
ftgpio_attach(device_t dev)
{
	struct ftgpio_softc *sc;
	int                  i;

	sc		= device_get_softc(dev);
	sc->dev = dev;

	GPIO_LOCK_INIT(sc);
	GPIO_LOCK(sc);

	for (i = 0; i <= FTGPIO_MAX_PIN; i++) {
		struct gpio_pin *pin;

		pin           = &sc->pins[i];
		pin->gp_pin   = i;
		pin->gp_caps  = FTGPIO_GPIO_CAPS;
		pin->gp_flags = 0;

		if (ftgpio_pin_is_output(sc, i))
			pin->gp_flags |= GPIO_PIN_OUTPUT;
		else
			pin->gp_flags |= GPIO_PIN_INPUT;

		if (ftgpio_pin_is_pushpull(sc, i))
			pin->gp_flags |= GPIO_PIN_PUSHPULL;
		else
			pin->gp_flags |= GPIO_PIN_OPENDRAIN;

		snprintf(pin->gp_name, GPIOMAXNAME, "GPIO%u%u",
			FTGPIO_PIN_GETGROUP(i), FTGPIO_PIN_GETINDEX(i));
	}

	/* Enable all groups */
	superio_write(sc->dev, GPIO1_ENABLE, 0xFF);
	superio_write(sc->dev, GPIO2_ENABLE, 0xFF);
	superio_write(sc->dev, GPIO3_ENABLE, 0xFF);
	superio_write(sc->dev, GPIO4_ENABLE, 0xFF);
	superio_write(sc->dev, FULL_UR5_UR6, 0x0A);
	FTGPIO_VERBOSE_PRINTF(sc->dev, "groups GPIO1..GPIO6 enabled\n");

	GPIO_UNLOCK(sc);
	sc->busdev = gpiobus_add_bus(dev);
	if (sc->busdev == NULL) {
		GPIO_LOCK_DESTROY(sc);
		return (ENXIO);
	}

	bus_attach_children(dev);
	return (0);
}

static int
ftgpio_detach(device_t dev)
{
	struct ftgpio_softc *sc;

	sc = device_get_softc(dev);
	gpiobus_detach_bus(dev);
	GPIO_ASSERT_UNLOCKED(sc);
	GPIO_LOCK_DESTROY(sc);

	return (0);
}

static device_t
ftgpio_gpio_get_bus(device_t dev)
{
	struct ftgpio_softc *sc;

	sc = device_get_softc(dev);

	return (sc->busdev);
}

static int
ftgpio_gpio_pin_max(device_t dev, int *npins)
{
	*npins = FTGPIO_MAX_PIN;
	return (0);
}

static int
ftgpio_gpio_pin_set(device_t dev, uint32_t pin_num, uint32_t pin_value)
{
	struct ftgpio_softc *sc;

	if (!FTGPIO_IS_VALID_PIN(pin_num))
		return (EINVAL);

	sc = device_get_softc(dev);
	GPIO_LOCK(sc);
	if ((sc->pins[pin_num].gp_flags & GPIO_PIN_OUTPUT) == 0) {
		GPIO_UNLOCK(sc);
		return (EINVAL);
	}
	ftgpio_pin_write(sc, pin_num, pin_value);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
ftgpio_gpio_pin_get(device_t dev, uint32_t pin_num, uint32_t *pin_value)
{
	struct ftgpio_softc *sc;

	if (!FTGPIO_IS_VALID_PIN(pin_num))
		return (EINVAL);

	if (pin_value == NULL)
		return (EINVAL);

	sc = device_get_softc(dev);
	GPIO_LOCK(sc);
	*pin_value = ftgpio_pin_read(sc, pin_num);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
ftgpio_gpio_pin_toggle(device_t dev, uint32_t pin_num)
{
	struct ftgpio_softc *sc;
	bool              pin_value;

	if (!FTGPIO_IS_VALID_PIN(pin_num))
		return (EINVAL);

	sc = device_get_softc(dev);
	GPIO_LOCK(sc);
	pin_value = ftgpio_pin_read(sc, pin_num);
	ftgpio_pin_write(sc, pin_num, !pin_value);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
ftgpio_gpio_pin_getname(device_t dev, uint32_t pin_num, char *pin_name)
{
	struct ftgpio_softc *sc;

	if (pin_name == NULL)
		return (EINVAL);

	if (!FTGPIO_IS_VALID_PIN(pin_num))
		return (EINVAL);

	sc = device_get_softc(dev);
	strlcpy(pin_name, sc->pins[pin_num].gp_name, GPIOMAXNAME);

	return (0);
}

static int
ftgpio_gpio_pin_getcaps(device_t dev, uint32_t pin_num, uint32_t *pin_caps)
{
	struct ftgpio_softc *sc;

	if (pin_caps == NULL)
		return (EINVAL);

	if (!FTGPIO_IS_VALID_PIN(pin_num))
		return (EINVAL);

	sc        = device_get_softc(dev);
	*pin_caps = sc->pins[pin_num].gp_caps;

	return (0);
}

static int
ftgpio_gpio_pin_getflags(device_t dev, uint32_t pin_num, uint32_t *pin_flags)
{
	struct ftgpio_softc *sc;

	if (pin_flags == NULL)
		return (EINVAL);

	if (!FTGPIO_IS_VALID_PIN(pin_num))
		return (EINVAL);

	sc         = device_get_softc(dev);
	*pin_flags = sc->pins[pin_num].gp_flags;

	return (0);
}

static int
ftgpio_gpio_pin_setflags(device_t dev, uint32_t pin_num, uint32_t pin_flags)
{
	struct ftgpio_softc *sc;
	int               ret;

	if (!FTGPIO_IS_VALID_PIN(pin_num)) {
		FTGPIO_VERBOSE_PRINTF(dev, "invalid pin number: %u\n", pin_num);
		return (EINVAL);
	}

	sc = device_get_softc(dev);

	/* Check for unwanted flags. */
	if ((pin_flags & sc->pins[pin_num].gp_caps) != pin_flags) {
		FTGPIO_VERBOSE_PRINTF(dev, "invalid pin flags 0x%x, vs caps 0x%x\n",
			pin_flags, sc->pins[pin_num].gp_caps);
		return (EINVAL);
	}

	GPIO_LOCK(sc);
	ret = ftgpio_pin_setflags(sc, pin_num, pin_flags);
	GPIO_UNLOCK(sc);

	return (ret);
}

static device_method_t ftgpio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,     ftgpio_probe),
	DEVMETHOD(device_attach,    ftgpio_attach),
	DEVMETHOD(device_detach,    ftgpio_detach),

	/* GPIO */
	DEVMETHOD(gpio_get_bus,         ftgpio_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,         ftgpio_gpio_pin_max),
	DEVMETHOD(gpio_pin_set,         ftgpio_gpio_pin_set),
	DEVMETHOD(gpio_pin_get,         ftgpio_gpio_pin_get),
	DEVMETHOD(gpio_pin_toggle,      ftgpio_gpio_pin_toggle),
	DEVMETHOD(gpio_pin_getname,     ftgpio_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getcaps,     ftgpio_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,    ftgpio_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,    ftgpio_gpio_pin_setflags),

	DEVMETHOD_END
};

static driver_t ftgpio_driver = {
	"gpio",
	ftgpio_methods,
	sizeof(struct ftgpio_softc)
};

DRIVER_MODULE(ftgpio, superio, ftgpio_driver, NULL,  NULL);
MODULE_DEPEND(ftgpio, gpiobus, 1, 1, 1);
MODULE_DEPEND(ftgpio, superio, 1, 1, 1);
MODULE_VERSION(ftgpio, 1);
