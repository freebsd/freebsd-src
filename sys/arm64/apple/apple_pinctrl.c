/*	$OpenBSD: aplpinctrl.c,v 1.4 2022/04/06 18:59:26 naddy Exp $	*/
/*
 * Copyright (c) 2021 Mark Kettenis <kettenis@openbsd.org>
 * Copyright (c) 2022 Kyle Evans <kevans@FreeBSD.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/resource.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/fdt/fdt_pinctrl.h>

#include "pic_if.h"
#include "gpio_if.h"

#define APPLE_PIN(pinmux) ((pinmux) & 0xffff)
#define APPLE_FUNC(pinmux) ((pinmux) >> 16)

#define GPIO_PIN(pin)		((pin) * 4)
#define  GPIO_PIN_GROUP_MASK	(7 << 16)
#define  GPIO_PIN_INPUT_ENABLE	(1 << 9)
#define  GPIO_PIN_FUNC_MASK	(3 << 5)
#define  GPIO_PIN_FUNC_SHIFT	5
#define  GPIO_PIN_MODE_MASK	(7 << 1)
#define  GPIO_PIN_MODE_INPUT	(0 << 1)
#define  GPIO_PIN_MODE_OUTPUT	(1 << 1)
#define  GPIO_PIN_MODE_IRQ_HI	(2 << 1)
#define  GPIO_PIN_MODE_IRQ_LO	(3 << 1)
#define  GPIO_PIN_MODE_IRQ_UP	(4 << 1)
#define  GPIO_PIN_MODE_IRQ_DN	(5 << 1)
#define  GPIO_PIN_MODE_IRQ_ANY	(6 << 1)
#define  GPIO_PIN_MODE_IRQ_OFF	(7 << 1)
#define  GPIO_PIN_DATA		(1 << 0)
#define GPIO_IRQ(grp, pin)	(0x800 + (grp) * 64 + ((pin) >> 5) * 4)

#define	APPLE_PINCTRL_DEFAULT_CAPS	\
	(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)

#define HREAD4(sc, reg)							\
	bus_read_4((sc)->sc_res[APPLE_PINCTRL_MEMRES], reg)
#define HWRITE4(sc, reg, val)						\
	bus_write_4((sc)->sc_res[APPLE_PINCTRL_MEMRES], reg, val)
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct apple_pinctrl_irqsrc {
	struct intr_irqsrc	isrc;
	int			irq;
	int			type;
};

enum {
	APPLE_PINCTRL_MEMRES = 0,
	APPLE_PINCTRL_IRQRES,
	APPLE_PINCTRL_NRES,
};

struct apple_pinctrl_softc {
	device_t		sc_dev;
	device_t		sc_busdev;
	struct mtx		sc_mtx;
	int			sc_ngpios;

	void			*sc_intrhand;
	struct resource		*sc_res[APPLE_PINCTRL_NRES];
	struct apple_pinctrl_irqsrc	*sc_irqs;
};

#define	APPLE_PINCTRL_LOCK(sc)		mtx_lock_spin(&(sc)->sc_mtx)
#define	APPLE_PINCTRL_UNLOCK(sc)	mtx_unlock_spin(&(sc)->sc_mtx)
#define	APPLE_PINCTRL_LOCK_ASSERT(sc)	mtx_assert(&(sc)->sc_mtx, MA_OWNED)

static struct ofw_compat_data compat_data[] = {
	{"apple,pinctrl",	1},
	{NULL,			0},
};

static struct resource_spec apple_pinctrl_res_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1,			0,	0 },
};

static int	apple_pinctrl_probe(device_t dev);
static int	apple_pinctrl_attach(device_t dev);
static int	apple_pinctrl_detach(device_t dev);

static int	apple_pinctrl_configure(device_t, phandle_t);
static phandle_t	apple_pinctrl_get_node(device_t, device_t);

static int
apple_pinctrl_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Apple Pinmux Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
apple_pinctrl_attach(device_t dev)
{
	pcell_t gpio_ranges[4];
	phandle_t node;
	struct apple_pinctrl_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	node = ofw_bus_get_node(dev);

	if (bus_alloc_resources(dev, apple_pinctrl_res_spec, sc->sc_res) != 0) {
		device_printf(dev, "cannot allocate device resources\n");
		return (ENXIO);
	}

	mtx_init(&sc->sc_mtx, "aapl gpio", "gpio", MTX_SPIN);

	error = OF_getencprop(node, "gpio-ranges", gpio_ranges,
	    sizeof(gpio_ranges));
	if (error == -1) {
		device_printf(dev, "failed to get gpio-ranges\n");
		goto error;
	}

	sc->sc_ngpios = gpio_ranges[3];
	if (sc->sc_ngpios == 0) {
		device_printf(dev, "no GPIOs\n");
		goto error;
	}

	fdt_pinctrl_register(dev, "pinmux");
	fdt_pinctrl_configure_tree(dev);

	if (OF_hasprop(node, "interrupt-controller")) {
		sc->sc_irqs = mallocarray(sc->sc_ngpios,
		    sizeof(*sc->sc_irqs), M_DEVBUF, M_ZERO | M_WAITOK);
		intr_pic_register(dev,
		    OF_xref_from_node(ofw_bus_get_node(dev)));
	}

	sc->sc_busdev = gpiobus_add_bus(dev);
	if (sc->sc_busdev == NULL) {
		device_printf(dev, "failed to attach gpiobus\n");
		goto error;
	}

	bus_attach_children(dev);
	return (0);
error:
	mtx_destroy(&sc->sc_mtx);
	bus_release_resources(dev, apple_pinctrl_res_spec, sc->sc_res);
	return (ENXIO);
}

static int
apple_pinctrl_detach(device_t dev)
{

	return (EBUSY);
}

static void
apple_pinctrl_pin_configure(struct apple_pinctrl_softc *sc, uint32_t pin,
    uint32_t flags)
{
	uint32_t reg;

	APPLE_PINCTRL_LOCK_ASSERT(sc);

	MPASS(pin < sc->sc_ngpios);

	reg = HREAD4(sc, GPIO_PIN(pin));
	reg &= ~GPIO_PIN_FUNC_MASK;
	reg &= ~GPIO_PIN_MODE_MASK;

	if ((flags & GPIO_PIN_PRESET_LOW) != 0)
		reg &= ~GPIO_PIN_DATA;
	else if ((flags & GPIO_PIN_PRESET_HIGH) != 0)
		reg |= GPIO_PIN_DATA;

	if ((flags & GPIO_PIN_INPUT) != 0)
		reg |= GPIO_PIN_MODE_INPUT;
	else if ((flags & GPIO_PIN_OUTPUT) != 0)
		reg |= GPIO_PIN_MODE_OUTPUT;

	HWRITE4(sc, GPIO_PIN(pin), reg);
}

static device_t
apple_pinctrl_get_bus(device_t dev)
{
	struct apple_pinctrl_softc *sc;

	sc = device_get_softc(dev);
	return (sc->sc_busdev);
}

static int
apple_pinctrl_pin_max(device_t dev, int *maxpin)
{
	struct apple_pinctrl_softc *sc;

	sc = device_get_softc(dev);
	*maxpin = sc->sc_ngpios - 1;
	return (0);
}

static int
apple_pinctrl_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct apple_pinctrl_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->sc_ngpios)
		return (EINVAL);

	snprintf(name, GPIOMAXNAME - 1, "gpio%c%d",
	    device_get_unit(dev) + 'a', pin);

	return (0);
}

static int
apple_pinctrl_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct apple_pinctrl_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);
	if (pin >= sc->sc_ngpios)
		return (EINVAL);

	*flags = 0;

	APPLE_PINCTRL_LOCK(sc);

	reg = HREAD4(sc, GPIO_PIN(pin));
	if ((reg & GPIO_PIN_MODE_INPUT) != 0)
		*flags |= GPIO_PIN_INPUT;
	else if ((reg & GPIO_PIN_MODE_OUTPUT) != 0)
		*flags |= GPIO_PIN_OUTPUT;

	APPLE_PINCTRL_UNLOCK(sc);

	return (0);
}

static int
apple_pinctrl_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{

	*caps = APPLE_PINCTRL_DEFAULT_CAPS;
	return (0);
}

static int
apple_pinctrl_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct apple_pinctrl_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->sc_ngpios)
		return (EINVAL);

	APPLE_PINCTRL_LOCK(sc);
	apple_pinctrl_pin_configure(sc, pin, flags);
	APPLE_PINCTRL_UNLOCK(sc);

	return (0);
}

static int
apple_pinctrl_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct apple_pinctrl_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);
	if (pin >= sc->sc_ngpios)
		return (EINVAL);

	APPLE_PINCTRL_LOCK(sc);
	reg = HREAD4(sc, GPIO_PIN(pin));
	*val = !!(reg & GPIO_PIN_DATA);
	APPLE_PINCTRL_UNLOCK(sc);

	return (0);
}

static int
apple_pinctrl_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct apple_pinctrl_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->sc_ngpios)
		return (EINVAL);

	APPLE_PINCTRL_LOCK(sc);
	if (value)
		HSET4(sc, GPIO_PIN(pin), GPIO_PIN_DATA);
	else
		HCLR4(sc, GPIO_PIN(pin), GPIO_PIN_DATA);
	device_printf(sc->sc_dev, "set pin %d to %x\n",
	    pin, HREAD4(sc, GPIO_PIN(pin)));
	APPLE_PINCTRL_UNLOCK(sc);
	return (0);
}


static int
apple_pinctrl_pin_toggle(device_t dev, uint32_t pin)
{
	struct apple_pinctrl_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);
	if (pin >= sc->sc_ngpios)
		return (EINVAL);

	APPLE_PINCTRL_LOCK(sc);
	reg = HREAD4(sc, GPIO_PIN(pin));
	if ((reg & GPIO_PIN_DATA) == 0)
		reg |= GPIO_PIN_DATA;
	else
		reg &= ~GPIO_PIN_DATA;
	HWRITE4(sc, GPIO_PIN(pin), reg);
	APPLE_PINCTRL_UNLOCK(sc);
	return (0);
}


static int
apple_pinctrl_pin_config_32(device_t dev, uint32_t first_pin, uint32_t num_pins,
    uint32_t *pin_flags)
{
	struct apple_pinctrl_softc *sc;
	uint32_t pin;

	sc = device_get_softc(dev);
	if (first_pin >= sc->sc_ngpios)
		return (EINVAL);

	/*
	 * The configuration for a bank of pins is scattered among several
	 * registers; we cannot g'tee to simultaneously change the state of all
	 * the pins in the flags array.  So just loop through the array
	 * configuring each pin for now.  If there was a strong need, it might
	 * be possible to support some limited simultaneous config, such as
	 * adjacent groups of 8 pins that line up the same as the config regs.
	 */
	APPLE_PINCTRL_LOCK(sc);
	for (pin = first_pin; pin < num_pins; ++pin) {
		if (pin_flags[pin] & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT))
			apple_pinctrl_pin_configure(sc, pin, pin_flags[pin]);
	}
	APPLE_PINCTRL_UNLOCK(sc);

	return (0);
}

static phandle_t
apple_pinctrl_get_node(device_t dev, device_t bus)
{

	/* GPIO bus */
	return (ofw_bus_get_node(dev));
}

static int
apple_pinctrl_configure(device_t dev, phandle_t cfgxref)
{
	struct apple_pinctrl_softc *sc;
	pcell_t *pinmux;
	phandle_t node;
	ssize_t len;
	uint32_t reg;
	uint16_t pin, func;
	int i;

	sc = device_get_softc(dev);
	node = OF_node_from_xref(cfgxref);

	len = OF_getencprop_alloc(node, "pinmux", (void **)&pinmux);
	if (len <= 0)
		return (-1);

	APPLE_PINCTRL_LOCK(sc);
	for (i = 0; i < len / sizeof(pcell_t); i++) {
		pin = APPLE_PIN(pinmux[i]);
		func = APPLE_FUNC(pinmux[i]);
		reg = HREAD4(sc, GPIO_PIN(pin));
		reg &= ~GPIO_PIN_FUNC_MASK;
		reg |= (func << GPIO_PIN_FUNC_SHIFT) & GPIO_PIN_FUNC_MASK;
		HWRITE4(sc, GPIO_PIN(pin), reg);
	}
	APPLE_PINCTRL_UNLOCK(sc);

	OF_prop_free(pinmux);
	return 0;
}

static device_method_t apple_pinctrl_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		apple_pinctrl_probe),
	DEVMETHOD(device_attach,	apple_pinctrl_attach),
	DEVMETHOD(device_detach,	apple_pinctrl_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus,		apple_pinctrl_get_bus),
	DEVMETHOD(gpio_pin_max,		apple_pinctrl_pin_max),
	DEVMETHOD(gpio_pin_getname,	apple_pinctrl_pin_getname),
	DEVMETHOD(gpio_pin_getflags,	apple_pinctrl_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps,	apple_pinctrl_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags,	apple_pinctrl_pin_setflags),
	DEVMETHOD(gpio_pin_get,		apple_pinctrl_pin_get),
	DEVMETHOD(gpio_pin_set,		apple_pinctrl_pin_set),
	DEVMETHOD(gpio_pin_toggle,	apple_pinctrl_pin_toggle),
	DEVMETHOD(gpio_pin_config_32,	apple_pinctrl_pin_config_32),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,		apple_pinctrl_get_node),

        /* fdt_pinctrl interface */
	DEVMETHOD(fdt_pinctrl_configure,	apple_pinctrl_configure),

	DEVMETHOD_END
};

static driver_t apple_pinctrl_driver = {
	"gpio",
	apple_pinctrl_methods,
	sizeof(struct apple_pinctrl_softc),
};

EARLY_DRIVER_MODULE(apple_pinctrl, simplebus, apple_pinctrl_driver,
    0, 0, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
