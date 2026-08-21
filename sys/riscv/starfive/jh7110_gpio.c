/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Jari Sihvola <jsihv@gmx.com>
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/resource.h>

#include <dev/clk/clk.h>
#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "gpio_if.h"

#define	PINCTRL_SYS		1
#define	PINCTRL_AON		2

#define	GPIO_PINS		64
#define	AON_GPIO_PINS		4
#define	GPIO_REGS		2

#define	GP0_DOEN_CFG		0x0
#define	AON_DOEN_CFG		0x0
#define	GP0_DOUT_CFG		0x40
#define	AON_DOUT_CFG		0x4
#define	GPIOEN			0xdc
#define	AON_GPIOEN		0xc
#define	GPIOE_0			0x100
#define	GPIOE_1			0x104
#define	GPIOE_AON		0x20
#define	GPIO_DIN_LOW		0x118
#define	GPIO_DIN_HIGH		0x11c
#define	GPIO_DIN_AON		0x2c
#define	IOMUX_SYSCFG_288	0x120
#define	IOMUX_AONCFG_52		0x34

#define	PAD_INPUT_EN		(1 << 0)
#define	SHIFT_DRIVESTRENGTH	1
#define	PAD_DRIVESTRENGTH	(0x3 << SHIFT_DRIVESTRENGTH)
#define	PAD_PULLUP		(1 << 3)
#define	PAD_PULLDOWN		(1 << 4)
#define	PAD_SLEW		(1 << 5)
#define	PAD_HYST		(1 << 6)
#define	PAD_POWERONSTART	(1 << 7)

#define	ENABLE_MASK		0x3f
#define	OUTPUT_MASK		0x01
#define	DATA_OUT_MASK		0x7f
#define	DIROUT_DISABLE		1

struct jh7110_gpio_softc {
	device_t		dev;
	device_t		busdev;
	struct mtx		mtx;
	struct resource		*res;
	clk_t			clk;
	int			pinctrl; /* which pinctrl controller */
	uint32_t		maxpin; /* pins on this controller */
	/* location of variable position fields for the two controllers */
	uint32_t		iomuxcfg;
	uint32_t		doutcfg;
	uint32_t		doencfg;
};

static struct ofw_compat_data compat_data[] = {
	{"starfive,jh7110-sys-pinctrl",	PINCTRL_SYS},
	{"starfive,jh7110-aon-pinctrl",	PINCTRL_AON},
	{NULL,				0}
};

static struct resource_spec jh7110_gpio_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE },
	{ -1, 0 }
};

#define	GPIO_RW_OFFSET(_val)		(_val & ~3)
#define	GPIO_SHIFT(_val)		((_val & 3) * 8)
#define	PAD_OFFSET(_val)		(_val * 4) /* 32 bits per pin (even though only 8 used) */

#define	JH7110_GPIO_LOCK(_sc)		mtx_lock(&(_sc)->mtx)
#define	JH7110_GPIO_UNLOCK(_sc)		mtx_unlock(&(_sc)->mtx)

#define	RD4(sc, reg)			bus_read_4((sc)->res, (reg))
#define	WR4(sc, reg, val)		bus_write_4((sc)->res, (reg), (val))

static device_t
jh7110_gpio_get_bus(device_t dev)
{
	struct jh7110_gpio_softc *sc;

	sc = device_get_softc(dev);

	return (sc->busdev);
}

static int
jh7110_gpio_pin_max(device_t dev, int *maxpin)
{
	struct jh7110_gpio_softc *sc;

	sc = device_get_softc(dev);

	*maxpin = sc->maxpin;

	return (0);
}

static int
jh7110_gpio_pin_get(device_t dev, uint32_t pin, uint32_t *val)
{
	struct jh7110_gpio_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	if (pin > sc->maxpin)
		return (EINVAL);

	JH7110_GPIO_LOCK(sc);
	if (sc->pinctrl == PINCTRL_AON) {
		reg = RD4(sc, GPIO_DIN_AON);
		*val = (reg >> pin) & 0x1;
	} else if (pin < GPIO_PINS / GPIO_REGS) {
		reg = RD4(sc, GPIO_DIN_LOW);
		*val = (reg >> pin) & 0x1;
	} else {
		reg = RD4(sc, GPIO_DIN_HIGH);
		*val = (reg >> (pin - GPIO_PINS / GPIO_REGS)) & 0x1;
	}
	JH7110_GPIO_UNLOCK(sc);

	return (0);
}

static int
jh7110_gpio_pin_set(device_t dev, uint32_t pin, uint32_t val)
{
	struct jh7110_gpio_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	if (pin > sc->maxpin)
		return (EINVAL);

	JH7110_GPIO_LOCK(sc);
	reg = RD4(sc, sc->doutcfg + GPIO_RW_OFFSET(pin));
	reg &= ~(DATA_OUT_MASK << GPIO_SHIFT(pin));
	if (val != 0)
		reg |= 0x1 << GPIO_SHIFT(pin);
	WR4(sc, sc->doutcfg + GPIO_RW_OFFSET(pin), reg);
	JH7110_GPIO_UNLOCK(sc);

	return (0);
}

static int
jh7110_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct jh7110_gpio_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	if (pin > sc->maxpin)
		return (EINVAL);

	JH7110_GPIO_LOCK(sc);
	reg = RD4(sc, sc->doutcfg + GPIO_RW_OFFSET(pin));
	if ((reg & 0x1 << GPIO_SHIFT(pin)) != 0) {
		reg &= ~(DATA_OUT_MASK << GPIO_SHIFT(pin));
	} else {
		reg &= ~(DATA_OUT_MASK << GPIO_SHIFT(pin));
		reg |= 0x1 << GPIO_SHIFT(pin);
	}
	WR4(sc, sc->doutcfg + GPIO_RW_OFFSET(pin), reg);
	JH7110_GPIO_UNLOCK(sc);

	return (0);
}

static int
jh7110_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct jh7110_gpio_softc *sc;

	sc = device_get_softc(dev);

	if (pin > sc->maxpin)
		return (EINVAL);

	*caps = (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT | GPIO_PIN_PULLUP | GPIO_PIN_PULLDOWN);

	return (0);
}

static int
jh7110_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct jh7110_gpio_softc *sc;

	sc = device_get_softc(dev);

	if (pin > sc->maxpin)
		return (EINVAL);

	snprintf(name, GPIOMAXNAME, "%sGPIO%d", sc->pinctrl == PINCTRL_SYS ?
	  "" : "R", pin);

	return (0);
}

static int
jh7110_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct jh7110_gpio_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	if (pin > sc->maxpin)
		return (EINVAL);

	/* Reading the direction */
	*flags = 0;
	JH7110_GPIO_LOCK(sc);
	reg = RD4(sc, sc->doencfg + GPIO_RW_OFFSET(pin));
	if ((reg & ENABLE_MASK << GPIO_SHIFT(pin)) == 0)
		*flags |= GPIO_PIN_OUTPUT;
	else
		*flags |= GPIO_PIN_INPUT;
	reg = RD4(sc, sc->iomuxcfg + PAD_OFFSET(pin));
	if (reg & PAD_PULLUP)
		*flags |= GPIO_PIN_PULLUP;
	if (reg & PAD_PULLDOWN)
		*flags |= GPIO_PIN_PULLDOWN;
	JH7110_GPIO_UNLOCK(sc);

	return (0);
}

static int
jh7110_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct jh7110_gpio_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	if (pin > sc->maxpin)
		return (EINVAL);

	/* Setting the direction, enable or disable output, configuring pads */

	JH7110_GPIO_LOCK(sc);

	if ((flags & GPIO_PIN_INPUT) != 0) {
		reg = RD4(sc, sc->iomuxcfg + PAD_OFFSET(pin));
		reg |= (PAD_INPUT_EN | PAD_HYST);
		reg &= ~(PAD_DRIVESTRENGTH | PAD_SLEW);
		if ((flags & GPIO_PIN_PULLUP) != 0)
			reg |= PAD_PULLUP;
		else
			reg &= ~PAD_PULLUP;
		if ((flags & GPIO_PIN_PULLDOWN) != 0)
			reg |= PAD_PULLDOWN;
		else
			reg &= ~PAD_PULLDOWN;
		WR4(sc, sc->iomuxcfg + PAD_OFFSET(pin), reg);
	}

	reg = RD4(sc, sc->doencfg + GPIO_RW_OFFSET(pin));
	reg &= ~(ENABLE_MASK << GPIO_SHIFT(pin));
	if ((flags & GPIO_PIN_INPUT) != 0) {
		reg |= DIROUT_DISABLE << GPIO_SHIFT(pin);
	}
	WR4(sc, sc->doencfg + GPIO_RW_OFFSET(pin), reg);

	if ((flags & GPIO_PIN_OUTPUT) != 0) {
		reg = RD4(sc, sc->doutcfg + GPIO_RW_OFFSET(pin));
		/*
		 * Clear the output selection but maintain the current (from
		 * last time as output) state.
		 */
		reg &= ~((ENABLE_MASK - OUTPUT_MASK) << GPIO_SHIFT(pin));
		WR4(sc, sc->doutcfg + GPIO_RW_OFFSET(pin), reg);

		reg = RD4(sc, sc->iomuxcfg + PAD_OFFSET(pin));
		reg &= ~(PAD_PULLUP | PAD_PULLDOWN | PAD_HYST);
		WR4(sc, sc->iomuxcfg + PAD_OFFSET(pin), reg);
	}

	JH7110_GPIO_UNLOCK(sc);

	return (0);
}

static int
jh7110_gpio_probe(device_t dev)
{
	struct jh7110_gpio_softc *sc;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	sc = device_get_softc(dev);

	if ((sc->pinctrl = ofw_bus_search_compatible(dev, compat_data)->ocd_data) == 0)
		return (ENXIO);

	device_set_desc(dev, "StarFive JH7110 GPIO controller");

	return (BUS_PROBE_DEFAULT);
}

static int
jh7110_gpio_detach(device_t dev)
{
	struct jh7110_gpio_softc *sc;

	sc = device_get_softc(dev);

	bus_release_resources(dev, jh7110_gpio_spec, &sc->res);
	if (sc->busdev != NULL)
		gpiobus_detach_bus(dev);
	if (sc->clk != NULL)
		clk_release(sc->clk);
	mtx_destroy(&sc->mtx);

	return (0);
}

static int
jh7110_gpio_attach(device_t dev)
{
	struct jh7110_gpio_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	mtx_init(&sc->mtx, device_get_nameunit(sc->dev), NULL, MTX_DEF);

	if (bus_alloc_resources(dev, jh7110_gpio_spec, &sc->res) != 0) {
		device_printf(dev, "Could not allocate resources\n");
		bus_release_resources(dev, jh7110_gpio_spec, &sc->res);
		mtx_destroy(&sc->mtx);
		return (ENXIO);
	}

	sc->clk = NULL;
	if (clk_get_by_ofw_index(dev, 0, 0, &sc->clk) != 0 &&
	    sc->pinctrl != PINCTRL_AON) {
		device_printf(dev, "Cannot get clock\n");
		jh7110_gpio_detach(dev);
		return (ENXIO);
	}

	if (sc->clk != NULL && clk_enable(sc->clk) != 0) {
		device_printf(dev, "Could not enable clock %s\n",
		    clk_get_name(sc->clk));
		jh7110_gpio_detach(dev);
		return (ENXIO);
	}

	/* Reset GPIO interrupts and set register offsets. */
	if (sc->pinctrl == PINCTRL_SYS) {
		WR4(sc, GPIOE_0, 0);
		WR4(sc, GPIOE_1, 0);
		WR4(sc, GPIOEN, 1);
		sc->maxpin = GPIO_PINS - 1;
		sc->iomuxcfg = IOMUX_SYSCFG_288;
		sc->doutcfg = GP0_DOUT_CFG;
		sc->doencfg = GP0_DOEN_CFG;
	} else {
		WR4(sc, GPIOE_AON, 0);
		WR4(sc, AON_GPIOEN, 1);
		sc->maxpin = AON_GPIO_PINS - 1;
		sc->iomuxcfg = IOMUX_AONCFG_52;
		sc->doutcfg = AON_DOUT_CFG;
		sc->doencfg = AON_DOEN_CFG;
	}

	sc->busdev = gpiobus_add_bus(dev);
	if (sc->busdev == NULL) {
		device_printf(dev, "Cannot attach gpiobus\n");
		jh7110_gpio_detach(dev);
		return (ENXIO);
	}

	bus_attach_children(dev);
	return (0);
}

static phandle_t
jh7110_gpio_get_node(device_t bus, device_t dev)
{
	return (ofw_bus_get_node(bus));
}

static device_method_t jh7110_gpio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		jh7110_gpio_probe),
	DEVMETHOD(device_attach,	jh7110_gpio_attach),
	DEVMETHOD(device_detach,	jh7110_gpio_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus,		jh7110_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,		jh7110_gpio_pin_max),
	DEVMETHOD(gpio_pin_get,		jh7110_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		jh7110_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	jh7110_gpio_pin_toggle),
	DEVMETHOD(gpio_pin_getflags,	jh7110_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,	jh7110_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_getcaps,	jh7110_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getname,	jh7110_gpio_pin_getname),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	jh7110_gpio_get_node),

	DEVMETHOD_END
};

DEFINE_CLASS_0(gpio, jh7110_gpio_driver, jh7110_gpio_methods,
    sizeof(struct jh7110_gpio_softc));
EARLY_DRIVER_MODULE(jh7110_gpio, simplebus, jh7110_gpio_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
MODULE_DEPEND(jh7110_gpio, gpiobus, 1, 1, 1);
