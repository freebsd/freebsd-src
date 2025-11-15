/*
 * Copyright (c) 2025 Stephen Hurd <shurd@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/syscon/syscon.h>

#include "syscon_if.h"

#define	GRF_SOC_CON10			0x0428
#define	 SOC_CON10_GPIOMUT		(1 << 1)
#define	 SOC_CON10_GPIOMUT_MASK		((1 << 1) << 16)
#define	 SOC_CON10_GPIOMUT_EN		(1 << 0)
#define	 SOC_CON10_GPIOMUT_EN_MASK	((1 << 0) << 16)

struct rk_grf_gpio_softc {
	device_t		sc_dev;
	device_t		sc_busdev;
	struct syscon		*sc_grf;
	bool			active_high;
};

static struct ofw_compat_data compat_data[] = {
	{"rockchip,rk3328-grf-gpio", 1},
	{NULL,             0}
};

static device_t
rk_grf_gpio_get_bus(device_t dev)
{
	struct rk_grf_gpio_softc *sc;

	sc = device_get_softc(dev);

	return (sc->sc_busdev);
}

static int
rk_grf_gpio_pin_max(device_t dev, int *maxpin)
{
	*maxpin = 1;
	return (0);
}

static int
rk_grf_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	if (pin)
		return (EINVAL);

	snprintf(name, GPIOMAXNAME, "GPIO_MUTE");

	return (0);
}

static int
rk_grf_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	if (pin)
		return (EINVAL);
	*flags = GPIO_PIN_OUTPUT;
	return (0);
}

static int
rk_grf_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	if (pin)
		return (EINVAL);
	if (flags != GPIO_PIN_OUTPUT)
		return (EINVAL);

	return (0);
}

static int
rk_grf_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	if (pin)
		return (EINVAL);

	*caps = GPIO_PIN_OUTPUT;
	return (0);
}

static int
rk_grf_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct rk_grf_gpio_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	if (pin)
		return (EINVAL);

	reg = SYSCON_READ_4(sc->sc_grf, GRF_SOC_CON10);
	if (reg & SOC_CON10_GPIOMUT)
		*val = 1;
	else
		*val = 0;

	return (0);
}

static int
rk_grf_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct rk_grf_gpio_softc *sc;
	uint32_t val;

	sc = device_get_softc(dev);

	if (pin)
		return (EINVAL);

	val = SOC_CON10_GPIOMUT_MASK;
	if (value)
		val |= SOC_CON10_GPIOMUT;
	SYSCON_WRITE_4(sc->sc_grf, GRF_SOC_CON10, val);

	return (0);
}

static int
rk_grf_gpio_map_gpios(device_t bus, phandle_t dev, phandle_t gparent, int gcells,
    pcell_t *gpios, uint32_t *pin, uint32_t *flags)
{
	if (gpios[0])
		return (EINVAL);

	/* The gpios are mapped as <pin flags> */
	*pin = 0;
	/* TODO: The only valid flags are active low or active high */
	*flags = GPIO_PIN_OUTPUT;
	return (0);
}

static int
rk_grf_gpio_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "RockChip General Register File GPIO (GPIO_MUTE)");
	return (BUS_PROBE_DEFAULT);
}

static int
rk_grf_gpio_attach(device_t dev)
{
	struct rk_grf_gpio_softc *sc;
	phandle_t parent_node, node;
	device_t pdev;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	node = ofw_bus_get_node(sc->sc_dev);
	if (!OF_hasprop(node, "gpio-controller"))
		return (ENXIO);
	pdev = device_get_parent(dev);
	parent_node = ofw_bus_get_node(pdev);
	if (syscon_get_by_ofw_node(dev, parent_node, &sc->sc_grf) != 0) {
		device_printf(dev, "cannot get parent syscon handle\n");
		return (ENXIO);
	}

	sc->sc_busdev = gpiobus_add_bus(dev);
	if (sc->sc_busdev == NULL) {
		return (ENXIO);
	}

	bus_attach_children(dev);
	return (0);
}

static int
rk_grf_gpio_detach(device_t dev)
{
	struct rk_grf_gpio_softc *sc;

	sc = device_get_softc(dev);

	if (sc->sc_busdev)
		gpiobus_detach_bus(dev);

	return(0);
}

static device_method_t rk_grf_gpio_methods[] = {
	DEVMETHOD(device_probe, rk_grf_gpio_probe),
	DEVMETHOD(device_attach, rk_grf_gpio_attach),
	DEVMETHOD(device_detach, rk_grf_gpio_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus,		rk_grf_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,		rk_grf_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname,	rk_grf_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags,	rk_grf_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,	rk_grf_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_getcaps,	rk_grf_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_get,		rk_grf_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		rk_grf_gpio_pin_set),
	DEVMETHOD(gpio_map_gpios,	rk_grf_gpio_map_gpios),

	DEVMETHOD_END
};

static driver_t rk_grf_gpio_driver = {
	"gpio",
	rk_grf_gpio_methods,
	sizeof(struct rk_grf_gpio_softc),
};

/*
 * GPIO driver is always a child of rk_grf driver and should be probed
 * and attached within rk_grf function. Due to this, bus pass order
 * must be same as bus pass order of rk_grf driver.
 */
EARLY_DRIVER_MODULE(rk_grf_gpio, simplebus, rk_grf_gpio_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
