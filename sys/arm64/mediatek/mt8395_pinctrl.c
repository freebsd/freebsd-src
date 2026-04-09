/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 UOS Project Contributors
 *
 * MediaTek MT8395 (Genio 1200) / MT8195 GPIO/Pinctrl Driver
 *
 * The MT8195/MT8395 has 202 GPIO pins managed by a single IOCFG
 * (IO Configuration) block. Each pin can be configured for:
 *   - GPIO direction and value
 *   - Pinmux function selection (up to 8 functions per pin)
 *   - Pull-up / pull-down (standard and RSEL for I2C)
 *   - Drive strength
 *   - Input enable / Schmitt trigger
 *
 * Compatible: "mediatek,mt8195-pinctrl"
 *             "mediatek,mt8395-pinctrl"
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/gpio/gpiobusvar.h>

/*
 * MT8195 IOCFG register layout.
 * Base: 0x10005000 (GPIO base from DTS)
 *
 * The pinctrl has multiple IOCFG sub-regions for different pin groups.
 * We use a simplified flat model here with the primary GPIO bank.
 */

/* GPIO direction registers (each bit = 1 GPIO, 0=input, 1=output) */
#define MT8195_GPIO_DIR_BASE		0x000
#define MT8195_GPIO_DIR_SET_BASE	0x010
#define MT8195_GPIO_DIR_CLR_BASE	0x020

/* GPIO output data registers */
#define MT8195_GPIO_DOUT_BASE		0x100
#define MT8195_GPIO_DOUT_SET_BASE	0x110
#define MT8195_GPIO_DOUT_CLR_BASE	0x120

/* GPIO input data registers (read-only) */
#define MT8195_GPIO_DIN_BASE		0x200

/* GPIO mode (pinmux function select) registers */
#define MT8195_GPIO_MODE_BASE		0x300

/* Register stride between banks (32 GPIOs per register) */
#define MT8195_GPIO_BANK_STRIDE		0x10
#define MT8195_GPIO_PINS_PER_REG	32

/* Total GPIO pins on MT8195/MT8395 */
#define MT8195_GPIO_NUM			202

/* Mode register: 4 bits per pin, 8 pins per register */
#define MT8195_GPIO_MODE_PINS_PER_REG	8
#define MT8195_GPIO_MODE_MASK		0x0F
#define MT8195_GPIO_MODE_SHIFT(pin)	(((pin) % MT8195_GPIO_MODE_PINS_PER_REG) * 4)
#define MT8195_GPIO_MODE_REG(pin)	(MT8195_GPIO_MODE_BASE + \
    ((pin) / MT8195_GPIO_MODE_PINS_PER_REG) * MT8195_GPIO_BANK_STRIDE)

/* Direction/data register helpers */
#define MT8195_GPIO_DIR_REG(pin)	(MT8195_GPIO_DIR_BASE + \
    ((pin) / MT8195_GPIO_PINS_PER_REG) * MT8195_GPIO_BANK_STRIDE)
#define MT8195_GPIO_DIR_SET_REG(pin)	(MT8195_GPIO_DIR_SET_BASE + \
    ((pin) / MT8195_GPIO_PINS_PER_REG) * MT8195_GPIO_BANK_STRIDE)
#define MT8195_GPIO_DIR_CLR_REG(pin)	(MT8195_GPIO_DIR_CLR_BASE + \
    ((pin) / MT8195_GPIO_PINS_PER_REG) * MT8195_GPIO_BANK_STRIDE)
#define MT8195_GPIO_DOUT_SET_REG(pin)	(MT8195_GPIO_DOUT_SET_BASE + \
    ((pin) / MT8195_GPIO_PINS_PER_REG) * MT8195_GPIO_BANK_STRIDE)
#define MT8195_GPIO_DOUT_CLR_REG(pin)	(MT8195_GPIO_DOUT_CLR_BASE + \
    ((pin) / MT8195_GPIO_PINS_PER_REG) * MT8195_GPIO_BANK_STRIDE)
#define MT8195_GPIO_DIN_REG(pin)	(MT8195_GPIO_DIN_BASE + \
    ((pin) / MT8195_GPIO_PINS_PER_REG) * MT8195_GPIO_BANK_STRIDE)
#define MT8195_GPIO_PIN_BIT(pin)	(1U << ((pin) % MT8195_GPIO_PINS_PER_REG))

struct mt8395_pinctrl_softc {
	device_t		 dev;
	device_t		 busdev;
	struct resource		*mem_res;
	struct mtx		 mtx;
};

#define	MT8395_PINCTRL_LOCK(sc)		mtx_lock(&(sc)->mtx)
#define	MT8395_PINCTRL_UNLOCK(sc)	mtx_unlock(&(sc)->mtx)

static inline uint32_t
mt8395_read4(struct mt8395_pinctrl_softc *sc, bus_size_t off)
{
	return (bus_read_4(sc->mem_res, off));
}

static inline void
mt8395_write4(struct mt8395_pinctrl_softc *sc, bus_size_t off, uint32_t val)
{
	bus_write_4(sc->mem_res, off, val);
}

/* ---------- GPIO bus interface implementation ---------- */

static int
mt8395_gpio_pin_max(device_t dev, int *maxpin)
{
	*maxpin = MT8195_GPIO_NUM - 1;
	return (0);
}

static int
mt8395_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	if (pin >= MT8195_GPIO_NUM)
		return (EINVAL);
	snprintf(name, GPIOMAXNAME, "GPIO%u", pin);
	return (0);
}

static int
mt8395_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	if (pin >= MT8195_GPIO_NUM)
		return (EINVAL);
	*caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT |
	    GPIO_PIN_PULLUP | GPIO_PIN_PULLDOWN;
	return (0);
}

static int
mt8395_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct mt8395_pinctrl_softc *sc;
	uint32_t reg;

	if (pin >= MT8195_GPIO_NUM)
		return (EINVAL);

	sc = device_get_softc(dev);
	MT8395_PINCTRL_LOCK(sc);
	reg = mt8395_read4(sc, MT8195_GPIO_DIR_REG(pin));
	MT8395_PINCTRL_UNLOCK(sc);

	if (reg & MT8195_GPIO_PIN_BIT(pin))
		*flags = GPIO_PIN_OUTPUT;
	else
		*flags = GPIO_PIN_INPUT;
	return (0);
}

static int
mt8395_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct mt8395_pinctrl_softc *sc;

	if (pin >= MT8195_GPIO_NUM)
		return (EINVAL);

	sc = device_get_softc(dev);
	MT8395_PINCTRL_LOCK(sc);
	if (flags & GPIO_PIN_OUTPUT) {
		/* Set direction to output using set register (atomic) */
		mt8395_write4(sc, MT8195_GPIO_DIR_SET_REG(pin),
		    MT8195_GPIO_PIN_BIT(pin));
	} else {
		/* Set direction to input using clear register (atomic) */
		mt8395_write4(sc, MT8195_GPIO_DIR_CLR_REG(pin),
		    MT8195_GPIO_PIN_BIT(pin));
	}
	MT8395_PINCTRL_UNLOCK(sc);
	return (0);
}

static int
mt8395_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct mt8395_pinctrl_softc *sc;
	uint32_t reg;

	if (pin >= MT8195_GPIO_NUM)
		return (EINVAL);

	sc = device_get_softc(dev);
	MT8395_PINCTRL_LOCK(sc);
	reg = mt8395_read4(sc, MT8195_GPIO_DIN_REG(pin));
	MT8395_PINCTRL_UNLOCK(sc);

	*val = (reg & MT8195_GPIO_PIN_BIT(pin)) ? 1 : 0;
	return (0);
}

static int
mt8395_gpio_pin_set(device_t dev, uint32_t pin, unsigned int val)
{
	struct mt8395_pinctrl_softc *sc;

	if (pin >= MT8195_GPIO_NUM)
		return (EINVAL);

	sc = device_get_softc(dev);
	MT8395_PINCTRL_LOCK(sc);
	if (val)
		mt8395_write4(sc, MT8195_GPIO_DOUT_SET_REG(pin),
		    MT8195_GPIO_PIN_BIT(pin));
	else
		mt8395_write4(sc, MT8195_GPIO_DOUT_CLR_REG(pin),
		    MT8195_GPIO_PIN_BIT(pin));
	MT8395_PINCTRL_UNLOCK(sc);
	return (0);
}

static int
mt8395_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct mt8395_pinctrl_softc *sc;
	uint32_t reg;

	if (pin >= MT8195_GPIO_NUM)
		return (EINVAL);

	sc = device_get_softc(dev);
	MT8395_PINCTRL_LOCK(sc);
	reg = mt8395_read4(sc, MT8195_GPIO_DIN_REG(pin));
	if (reg & MT8195_GPIO_PIN_BIT(pin))
		mt8395_write4(sc, MT8195_GPIO_DOUT_CLR_REG(pin),
		    MT8195_GPIO_PIN_BIT(pin));
	else
		mt8395_write4(sc, MT8195_GPIO_DOUT_SET_REG(pin),
		    MT8195_GPIO_PIN_BIT(pin));
	MT8395_PINCTRL_UNLOCK(sc);
	return (0);
}

/*
 * Pinmux configuration: set a pin's alternate function.
 * mode: 0 = GPIO, 1-7 = peripheral functions per TRM pinmux table.
 */
static int
mt8395_pinmux_set(device_t dev, uint32_t pin, uint32_t mode)
{
	struct mt8395_pinctrl_softc *sc;
	uint32_t reg, shift;

	if (pin >= MT8195_GPIO_NUM || mode > 15)
		return (EINVAL);

	sc = device_get_softc(dev);
	shift = MT8195_GPIO_MODE_SHIFT(pin);

	MT8395_PINCTRL_LOCK(sc);
	reg = mt8395_read4(sc, MT8195_GPIO_MODE_REG(pin));
	reg &= ~(MT8195_GPIO_MODE_MASK << shift);
	reg |= (mode & MT8195_GPIO_MODE_MASK) << shift;
	mt8395_write4(sc, MT8195_GPIO_MODE_REG(pin), reg);
	MT8395_PINCTRL_UNLOCK(sc);
	return (0);
}

/* ---------- Device attach/detach ---------- */

static struct ofw_compat_data mt8395_pinctrl_compat[] = {
	{ "mediatek,mt8195-pinctrl", 1 },
	{ "mediatek,mt8395-pinctrl", 1 },
	{ NULL, 0 },
};

static int
mt8395_pinctrl_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, mt8395_pinctrl_compat)->ocd_data == 0)
		return (ENXIO);
	device_set_desc(dev, "MediaTek MT8395 GPIO/Pinctrl");
	return (BUS_PROBE_DEFAULT);
}

static int
mt8395_pinctrl_attach(device_t dev)
{
	struct mt8395_pinctrl_softc *sc;
	int rid;

	sc = device_get_softc(dev);
	sc->dev = dev;

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Cannot allocate memory resource\n");
		return (ENXIO);
	}

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	sc->busdev = gpiobus_attach_bus(dev);
	if (sc->busdev == NULL) {
		device_printf(dev, "Cannot attach gpiobus\n");
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);
		mtx_destroy(&sc->mtx);
		return (ENXIO);
	}

	device_printf(dev, "MT8395 GPIO/Pinctrl: %d pins\n", MT8195_GPIO_NUM);
	return (0);
}

static int
mt8395_pinctrl_detach(device_t dev)
{
	struct mt8395_pinctrl_softc *sc;

	sc = device_get_softc(dev);
	gpiobus_detach_bus(dev);
	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);
	mtx_destroy(&sc->mtx);
	return (0);
}

static device_method_t mt8395_pinctrl_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,    mt8395_pinctrl_probe),
	DEVMETHOD(device_attach,   mt8395_pinctrl_attach),
	DEVMETHOD(device_detach,   mt8395_pinctrl_detach),

	/* GPIO interface */
	DEVMETHOD(gpio_pin_max,    mt8395_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname, mt8395_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getcaps, mt8395_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags, mt8395_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags, mt8395_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get,    mt8395_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,    mt8395_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle, mt8395_gpio_pin_toggle),

	DEVMETHOD_END
};

static driver_t mt8395_pinctrl_driver = {
	"gpio",
	mt8395_pinctrl_methods,
	sizeof(struct mt8395_pinctrl_softc),
};

EARLY_DRIVER_MODULE(mt8395_pinctrl, simplebus, mt8395_pinctrl_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LAST);
MODULE_VERSION(mt8395_pinctrl, 1);
