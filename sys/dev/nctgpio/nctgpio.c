/*-
 * Copyright (c) 2016 Daniel Wyatt <Daniel.Wyatt@gmail.com>
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
 *
 */

/*
 * Nuvoton GPIO driver.
 *
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/lock.h>

#include <sys/module.h>
#include <sys/gpio.h>

#include <machine/bus.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/superio/superio.h>

#include "gpio_if.h"

/* Logical Device Numbers. */
#define NCT_LDN_GPIO			0x07
#define NCT_LDN_GPIO_MODE		0x0f

/* Logical Device 7 */
#define NCT_LD7_GPIO0_IOR		0xe0
#define NCT_LD7_GPIO0_DAT		0xe1
#define NCT_LD7_GPIO0_INV		0xe2
#define NCT_LD7_GPIO0_DST		0xe3
#define NCT_LD7_GPIO1_IOR		0xe4
#define NCT_LD7_GPIO1_DAT		0xe5
#define NCT_LD7_GPIO1_INV		0xe6
#define NCT_LD7_GPIO1_DST		0xe7

/* Logical Device F */
#define NCT_LDF_GPIO0_OUTCFG		0xe0
#define NCT_LDF_GPIO1_OUTCFG		0xe1

/* Direct I/O port access. */
#define	NCT_IO_GSR			0
#define	NCT_IO_IOR			1
#define	NCT_IO_DAT			2
#define	NCT_IO_INV			3

#define NCT_MAX_PIN			15
#define NCT_IS_VALID_PIN(_p)	((_p) >= 0 && (_p) <= NCT_MAX_PIN)

#define NCT_PIN_BIT(_p)         (1 << ((_p) & 7))

#define NCT_GPIO_CAPS	(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT | \
	GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL | \
	GPIO_PIN_INVIN | GPIO_PIN_INVOUT)

/*
 * Note that the values are important.
 * They match actual register offsets.
 */
typedef enum {
	REG_IOR = 0,
	REG_DAT = 1,
	REG_INV = 2,
} reg_t;

struct nct_softc {
	device_t			dev;
	device_t			dev_f;
	device_t			busdev;
	struct mtx			mtx;
	struct resource			*iores;
	int				iorid;
	int				curgrp;
	struct {
		/* direction, 1: pin is input */
		uint8_t			ior[2];
		/* output value */
		uint8_t			out[2];
		/* whether out is valid */
		uint8_t			out_known[2];
		/* inversion, 1: pin is inverted */
		uint8_t			inv[2];
	} 				cache;
	struct gpio_pin			pins[NCT_MAX_PIN + 1];
};

#define GPIO_LOCK_INIT(_sc)	mtx_init(&(_sc)->mtx,		\
		device_get_nameunit(dev), NULL, MTX_DEF)
#define GPIO_LOCK_DESTROY(_sc)		mtx_destroy(&(_sc)->mtx)
#define GPIO_LOCK(_sc)		mtx_lock(&(_sc)->mtx)
#define GPIO_UNLOCK(_sc)	mtx_unlock(&(_sc)->mtx)
#define GPIO_ASSERT_LOCKED(_sc)	mtx_assert(&(_sc)->mtx, MA_OWNED)
#define GPIO_ASSERT_UNLOCKED(_sc)	mtx_assert(&(_sc)->mtx, MA_NOTOWNED)

struct nuvoton_vendor_device_id {
	uint16_t		chip_id;
	const char *		descr;
} nct_devs[] = {
	{
		.chip_id	= 0x1061,
		.descr		= "Nuvoton NCT5104D",
	},
	{
		.chip_id	= 0xc452,
		.descr		= "Nuvoton NCT5104D (PC-Engines APU)",
	},
	{
		.chip_id	= 0xc453,
		.descr		= "Nuvoton NCT5104D (PC-Engines APU3)",
	},
};

static void
nct_io_set_group(struct nct_softc *sc, int group)
{

	GPIO_ASSERT_LOCKED(sc);
	if (group != sc->curgrp) {
		bus_write_1(sc->iores, NCT_IO_GSR, group);
		sc->curgrp = group;
	}
}

static uint8_t
nct_io_read(struct nct_softc *sc, int group, uint8_t reg)
{
	nct_io_set_group(sc, group);
	return (bus_read_1(sc->iores, reg));
}

static void
nct_io_write(struct nct_softc *sc, int group, uint8_t reg, uint8_t val)
{
	nct_io_set_group(sc, group);
	return (bus_write_1(sc->iores, reg, val));
}

static uint8_t
nct_get_ioreg(struct nct_softc *sc, reg_t reg, int group)
{
	uint8_t ioreg;

	if (sc->iores != NULL)
		ioreg = NCT_IO_IOR + reg;
	else if (group == 0)
		ioreg = NCT_LD7_GPIO0_IOR + reg;
	else
		ioreg = NCT_LD7_GPIO1_IOR + reg;
	return (ioreg);
}

static uint8_t
nct_read_reg(struct nct_softc *sc, reg_t reg, int group)
{
	uint8_t ioreg;
	uint8_t val;

	ioreg = nct_get_ioreg(sc, reg, group);
	if (sc->iores != NULL)
		val = nct_io_read(sc, group, ioreg);
	else
		val = superio_read(sc->dev, ioreg);

	return (val);
}

#define GET_BIT(v, b)	(((v) >> (b)) & 1)
static bool
nct_get_pin_reg(struct nct_softc *sc, reg_t reg, uint32_t pin_num)
{
	uint8_t bit;
	uint8_t group;
	uint8_t val;

	KASSERT(NCT_IS_VALID_PIN(pin_num), ("%s: invalid pin number %d",
	    __func__, pin_num));

	group = pin_num >> 3;
	bit = pin_num & 7;
	val = nct_read_reg(sc, reg, group);
	return (GET_BIT(val, bit));
}

static int
nct_get_pin_cache(struct nct_softc *sc, uint32_t pin_num, uint8_t *cache)
{
	uint8_t bit;
	uint8_t group;
	uint8_t val;

	KASSERT(NCT_IS_VALID_PIN(pin_num), ("%s: invalid pin number %d",
	    __func__, pin_num));

	group = pin_num >> 3;
	bit = pin_num & 7;
	val = cache[group];
	return (GET_BIT(val, bit));
}

static void
nct_write_reg(struct nct_softc *sc, reg_t reg, int group, uint8_t val)
{
	uint8_t ioreg;

	ioreg = nct_get_ioreg(sc, reg, group);
	if (sc->iores != NULL)
		nct_io_write(sc, group, ioreg, val);
	else
		superio_write(sc->dev, ioreg, val);
}

static void
nct_set_pin_reg(struct nct_softc *sc, reg_t reg, uint32_t pin_num, bool val)
{
	uint8_t *cache;
	uint8_t bit;
	uint8_t bitval;
	uint8_t group;
	uint8_t mask;

	KASSERT(NCT_IS_VALID_PIN(pin_num),
	    ("%s: invalid pin number %d", __func__, pin_num));
	KASSERT(reg == REG_IOR || reg == REG_INV,
	    ("%s: unsupported register %d", __func__, reg));

	group = pin_num >> 3;
	bit = pin_num & 7;
	mask = (uint8_t)1 << bit;
	bitval = (uint8_t)val << bit;

	if (reg == REG_IOR)
		cache = &sc->cache.ior[group];
	else
		cache = &sc->cache.inv[group];
	if ((*cache & mask) == bitval)
		return;
	*cache &= ~mask;
	*cache |= bitval;
	nct_write_reg(sc, reg, group, *cache);
}

/*
 * Set a pin to input (val is true) or output (val is false) mode.
 */
static void
nct_set_pin_input(struct nct_softc *sc, uint32_t pin_num, bool val)
{
	nct_set_pin_reg(sc, REG_IOR, pin_num, val);
}

/*
 * Check whether a pin is configured as an input.
 */
static bool
nct_pin_is_input(struct nct_softc *sc, uint32_t pin_num)
{
	return (nct_get_pin_cache(sc, pin_num, sc->cache.ior));
}

/*
 * Set a pin to inverted (val is true) or normal (val is false) mode.
 */
static void
nct_set_pin_inverted(struct nct_softc *sc, uint32_t pin_num, bool val)
{
	nct_set_pin_reg(sc, REG_INV, pin_num, val);
}

static bool
nct_pin_is_inverted(struct nct_softc *sc, uint32_t pin_num)
{
	return (nct_get_pin_cache(sc, pin_num, sc->cache.inv));
}

/*
 * Write a value to an output pin.
 * NB: the hardware remembers last output value across switching from
 * output mode to input mode and back.
 * Writes to a pin in input mode are not allowed here as they cannot
 * have any effect and would corrupt the output value cache.
 */
static void
nct_write_pin(struct nct_softc *sc, uint32_t pin_num, bool val)
{
	uint8_t bit;
	uint8_t group;

	KASSERT(!nct_pin_is_input(sc, pin_num), ("attempt to write input pin"));
	group = pin_num >> 3;
	bit = pin_num & 7;
	if (GET_BIT(sc->cache.out_known[group], bit) &&
	    GET_BIT(sc->cache.out[group], bit) == val) {
		/* The pin is already in requested state. */
		return;
	}
	sc->cache.out_known[group] |= 1 << bit;
	if (val)
		sc->cache.out[group] |= 1 << bit;
	else
		sc->cache.out[group] &= ~(1 << bit);
	nct_write_reg(sc, REG_DAT, group, sc->cache.out[group]);
}

/*
 * NB: state of an input pin cannot be cached, of course.
 * For an output we can either take the value from the cache if it's valid
 * or read the state from the hadrware and cache it.
 */
static bool
nct_read_pin(struct nct_softc *sc, uint32_t pin_num)
{
	uint8_t bit;
	uint8_t group;
	bool val;

	if (nct_pin_is_input(sc, pin_num))
		return (nct_get_pin_reg(sc, REG_DAT, pin_num));

	group = pin_num >> 3;
	bit = pin_num & 7;
	if (GET_BIT(sc->cache.out_known[group], bit))
		return (GET_BIT(sc->cache.out[group], bit));

	val = nct_get_pin_reg(sc, REG_DAT, pin_num);
	sc->cache.out_known[group] |= 1 << bit;
	if (val)
		sc->cache.out[group] |= 1 << bit;
	else
		sc->cache.out[group] &= ~(1 << bit);
	return (val);
}

static uint8_t
nct_outcfg_addr(uint32_t pin_num)
{
	KASSERT(NCT_IS_VALID_PIN(pin_num), ("%s: invalid pin number %d",
	    __func__, pin_num));
	if ((pin_num >> 3) == 0)
		return (NCT_LDF_GPIO0_OUTCFG);
	else
		return (NCT_LDF_GPIO1_OUTCFG);
}

/*
 * NB: PP/OD can be configured only via configuration registers.
 * Also, the registers are in a different logical device.
 * So, this is a special case.  No caching too.
 */
static void
nct_set_pin_opendrain(struct nct_softc *sc, uint32_t pin_num)
{
	uint8_t reg;
	uint8_t outcfg;

	reg = nct_outcfg_addr(pin_num);
	outcfg = superio_read(sc->dev_f, reg);
	outcfg |= NCT_PIN_BIT(pin_num);
	superio_write(sc->dev_f, reg, outcfg);
}

static void
nct_set_pin_pushpull(struct nct_softc *sc, uint32_t pin_num)
{
	uint8_t reg;
	uint8_t outcfg;

	reg = nct_outcfg_addr(pin_num);
	outcfg = superio_read(sc->dev_f, reg);
	outcfg &= ~NCT_PIN_BIT(pin_num);
	superio_write(sc->dev_f, reg, outcfg);
}

static bool
nct_pin_is_opendrain(struct nct_softc *sc, uint32_t pin_num)
{
	uint8_t reg;
	uint8_t outcfg;

	reg = nct_outcfg_addr(pin_num);
	outcfg = superio_read(sc->dev_f, reg);
	return (outcfg & NCT_PIN_BIT(pin_num));
}

static int
nct_probe(device_t dev)
{
	int j;
	uint16_t chipid;

	if (superio_vendor(dev) != SUPERIO_VENDOR_NUVOTON)
		return (ENXIO);
	if (superio_get_type(dev) != SUPERIO_DEV_GPIO)
		return (ENXIO);

	/*
	 * There are several GPIO devices, we attach only to one of them
	 * and use the rest without attaching.
	 */
	if (superio_get_ldn(dev) != NCT_LDN_GPIO)
		return (ENXIO);

	chipid = superio_devid(dev);
	for (j = 0; j < nitems(nct_devs); j++) {
		if (chipid == nct_devs[j].chip_id) {
			device_set_desc(dev, "Nuvoton GPIO controller");
			return (BUS_PROBE_DEFAULT);
		}
	}
	return (ENXIO);
}

static int
nct_attach(device_t dev)
{
	struct nct_softc *sc;
	device_t dev_8;
	uint16_t iobase;
	int err;
	int i;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->dev_f = superio_find_dev(device_get_parent(dev), SUPERIO_DEV_GPIO,
	    NCT_LDN_GPIO_MODE);
	if (sc->dev_f == NULL) {
		device_printf(dev, "failed to find LDN F\n");
		return (ENXIO);
	}

	/*
	 * As strange as it may seem, I/O port base is configured in the
	 * Logical Device 8 which is primarily used for WDT, but also plays
	 * a role in GPIO configuration.
	 */
	iobase = 0;
	dev_8 = superio_find_dev(device_get_parent(dev), SUPERIO_DEV_WDT, 8);
	if (dev_8 != NULL)
		iobase = superio_get_iobase(dev_8);
	if (iobase != 0 && iobase != 0xffff) {
		sc->curgrp = -1;
		sc->iorid = 0;
		err = bus_set_resource(dev, SYS_RES_IOPORT, sc->iorid,
		    iobase, 7);
		if (err == 0) {
			sc->iores = bus_alloc_resource_any(dev, SYS_RES_IOPORT,
			    &sc->iorid, RF_ACTIVE);
			if (sc->iores == NULL) {
				device_printf(dev, "can't map i/o space, "
				    "iobase=0x%04x\n", iobase);
			}
		} else {
			device_printf(dev,
			    "failed to set io port resource at 0x%x\n", iobase);
		}
	}

	/* Enable gpio0 and gpio1. */
	superio_dev_enable(dev, 0x03);

	GPIO_LOCK_INIT(sc);
	GPIO_LOCK(sc);

	sc->cache.inv[0] = nct_read_reg(sc, REG_INV, 0);
	sc->cache.inv[1] = nct_read_reg(sc, REG_INV, 1);
	sc->cache.ior[0] = nct_read_reg(sc, REG_IOR, 0);
	sc->cache.ior[1] = nct_read_reg(sc, REG_IOR, 1);

	/*
	 * Caching input values is meaningless as an input can be changed at any
	 * time by an external agent.  But outputs are controlled by this
	 * driver, so it can cache their state.  Also, the hardware remembers
	 * the output state of a pin when the pin is switched to input mode and
	 * then back to output mode.  So, the cache stays valid.
	 * The only problem is with pins that are in input mode at the attach
	 * time.  For them the output state is not known until it is set by the
	 * driver for the first time.
	 * 'out' and 'out_known' bits form a tri-state output cache:
	 * |-----+-----------+---------|
	 * | out | out_known | cache   |
	 * |-----+-----------+---------|
	 * |   X |         0 | invalid |
	 * |   0 |         1 |       0 |
	 * |   1 |         1 |       1 |
	 * |-----+-----------+---------|
	 */
	sc->cache.out[0] = nct_read_reg(sc, REG_DAT, 0);
	sc->cache.out[1] = nct_read_reg(sc, REG_DAT, 1);
	sc->cache.out_known[0] = ~sc->cache.ior[0];
	sc->cache.out_known[1] = ~sc->cache.ior[1];

	for (i = 0; i <= NCT_MAX_PIN; i++) {
		struct gpio_pin *pin;

		pin = &sc->pins[i];
		pin->gp_pin = i;
		pin->gp_caps = NCT_GPIO_CAPS;
		pin->gp_flags = 0;

		snprintf(pin->gp_name, GPIOMAXNAME, "GPIO%02o", i);
		pin->gp_name[GPIOMAXNAME - 1] = '\0';

		if (nct_pin_is_input(sc, i))
			pin->gp_flags |= GPIO_PIN_INPUT;
		else
			pin->gp_flags |= GPIO_PIN_OUTPUT;

		if (nct_pin_is_opendrain(sc, i))
			pin->gp_flags |= GPIO_PIN_OPENDRAIN;
		else
			pin->gp_flags |= GPIO_PIN_PUSHPULL;

		if (nct_pin_is_inverted(sc, i))
			pin->gp_flags |= (GPIO_PIN_INVIN | GPIO_PIN_INVOUT);
	}
	GPIO_UNLOCK(sc);

	sc->busdev = gpiobus_attach_bus(dev);
	if (sc->busdev == NULL) {
		GPIO_LOCK_DESTROY(sc);
		return (ENXIO);
	}

	return (0);
}

static int
nct_detach(device_t dev)
{
	struct nct_softc *sc;

	sc = device_get_softc(dev);
	gpiobus_detach_bus(dev);

	if (sc->iores != NULL)
		bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->iores);
	GPIO_ASSERT_UNLOCKED(sc);
	GPIO_LOCK_DESTROY(sc);

	return (0);
}

static device_t
nct_gpio_get_bus(device_t dev)
{
	struct nct_softc *sc;

	sc = device_get_softc(dev);

	return (sc->busdev);
}

static int
nct_gpio_pin_max(device_t dev, int *npins)
{
	*npins = NCT_MAX_PIN;

	return (0);
}

static int
nct_gpio_pin_set(device_t dev, uint32_t pin_num, uint32_t pin_value)
{
	struct nct_softc *sc;

	if (!NCT_IS_VALID_PIN(pin_num))
		return (EINVAL);

	sc = device_get_softc(dev);
	GPIO_LOCK(sc);
	if ((sc->pins[pin_num].gp_flags & GPIO_PIN_OUTPUT) == 0) {
		GPIO_UNLOCK(sc);
		return (EINVAL);
	}
	nct_write_pin(sc, pin_num, pin_value);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
nct_gpio_pin_get(device_t dev, uint32_t pin_num, uint32_t *pin_value)
{
	struct nct_softc *sc;

	if (!NCT_IS_VALID_PIN(pin_num))
		return (EINVAL);

	sc = device_get_softc(dev);
	GPIO_ASSERT_UNLOCKED(sc);
	GPIO_LOCK(sc);
	*pin_value = nct_read_pin(sc, pin_num);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
nct_gpio_pin_toggle(device_t dev, uint32_t pin_num)
{
	struct nct_softc *sc;

	if (!NCT_IS_VALID_PIN(pin_num))
		return (EINVAL);

	sc = device_get_softc(dev);
	GPIO_ASSERT_UNLOCKED(sc);
	GPIO_LOCK(sc);
	if ((sc->pins[pin_num].gp_flags & GPIO_PIN_OUTPUT) == 0) {
		GPIO_UNLOCK(sc);
		return (EINVAL);
	}
	if (nct_read_pin(sc, pin_num))
		nct_write_pin(sc, pin_num, 0);
	else
		nct_write_pin(sc, pin_num, 1);

	GPIO_UNLOCK(sc);

	return (0);
}

static int
nct_gpio_pin_getcaps(device_t dev, uint32_t pin_num, uint32_t *caps)
{
	struct nct_softc *sc;

	if (!NCT_IS_VALID_PIN(pin_num))
		return (EINVAL);

	sc = device_get_softc(dev);
	GPIO_ASSERT_UNLOCKED(sc);
	GPIO_LOCK(sc);
	*caps = sc->pins[pin_num].gp_caps;
	GPIO_UNLOCK(sc);

	return (0);
}

static int
nct_gpio_pin_getflags(device_t dev, uint32_t pin_num, uint32_t *flags)
{
	struct nct_softc *sc;

	if (!NCT_IS_VALID_PIN(pin_num))
		return (EINVAL);

	sc = device_get_softc(dev);
	GPIO_ASSERT_UNLOCKED(sc);
	GPIO_LOCK(sc);
	*flags = sc->pins[pin_num].gp_flags;
	GPIO_UNLOCK(sc);

	return (0);
}

static int
nct_gpio_pin_getname(device_t dev, uint32_t pin_num, char *name)
{
	struct nct_softc *sc;

	if (!NCT_IS_VALID_PIN(pin_num))
		return (EINVAL);

	sc = device_get_softc(dev);
	GPIO_ASSERT_UNLOCKED(sc);
	GPIO_LOCK(sc);
	memcpy(name, sc->pins[pin_num].gp_name, GPIOMAXNAME);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
nct_gpio_pin_setflags(device_t dev, uint32_t pin_num, uint32_t flags)
{
	struct nct_softc *sc;
	struct gpio_pin *pin;

	if (!NCT_IS_VALID_PIN(pin_num))
		return (EINVAL);

	sc = device_get_softc(dev);
	pin = &sc->pins[pin_num];
	if ((flags & pin->gp_caps) != flags)
		return (EINVAL);

	if ((flags & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) ==
		(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) {
			return (EINVAL);
	}
	if ((flags & (GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL)) ==
		(GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL)) {
			return (EINVAL);
	}
	if ((flags & (GPIO_PIN_INVIN | GPIO_PIN_INVOUT)) ==
		(GPIO_PIN_INVIN | GPIO_PIN_INVOUT)) {
			return (EINVAL);
	}

	GPIO_ASSERT_UNLOCKED(sc);
	GPIO_LOCK(sc);
	if ((flags & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) != 0) {
		nct_set_pin_input(sc, pin_num, (flags & GPIO_PIN_INPUT) != 0);
		pin->gp_flags &= ~(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT);
		pin->gp_flags |= flags & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT);
	}
	if ((flags & (GPIO_PIN_INVIN | GPIO_PIN_INVOUT)) != 0) {
		nct_set_pin_inverted(sc, pin_num,
		    (flags & GPIO_PIN_INVIN) != 0);
		pin->gp_flags &= ~(GPIO_PIN_INVIN | GPIO_PIN_INVOUT);
		pin->gp_flags |= flags & (GPIO_PIN_INVIN | GPIO_PIN_INVOUT);
	}
	if ((flags & (GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL)) != 0) {
		if (flags & GPIO_PIN_OPENDRAIN)
			nct_set_pin_opendrain(sc, pin_num);
		else
			nct_set_pin_pushpull(sc, pin_num);
		pin->gp_flags &= ~(GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL);
		pin->gp_flags |=
		    flags & (GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL);
	}
	GPIO_UNLOCK(sc);

	return (0);
}

static device_method_t nct_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nct_probe),
	DEVMETHOD(device_attach,	nct_attach),
	DEVMETHOD(device_detach,	nct_detach),

	/* GPIO */
	DEVMETHOD(gpio_get_bus,		nct_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,		nct_gpio_pin_max),
	DEVMETHOD(gpio_pin_get,		nct_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		nct_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	nct_gpio_pin_toggle),
	DEVMETHOD(gpio_pin_getname,	nct_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getcaps,	nct_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	nct_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,	nct_gpio_pin_setflags),

	DEVMETHOD_END
};

static driver_t nct_driver = {
	"gpio",
	nct_methods,
	sizeof(struct nct_softc)
};

DRIVER_MODULE(nctgpio, superio, nct_driver, NULL, NULL);
MODULE_DEPEND(nctgpio, gpiobus, 1, 1, 1);
MODULE_DEPEND(nctgpio, superio, 1, 1, 1);
MODULE_VERSION(nctgpio, 1);

