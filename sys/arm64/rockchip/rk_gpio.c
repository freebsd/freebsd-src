/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Emmanuel Vadot <manu@FreeBSD.org>
 * Copyright (c) 2021 Soren Schmidt <sos@deepcore.dk>
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/extres/clk/clk.h>

#include "gpio_if.h"
#include "pic_if.h"

#include "fdt_pinctrl_if.h"

enum gpio_regs {
	RK_GPIO_SWPORTA_DR = 1,	/* Data register */
	RK_GPIO_SWPORTA_DDR,	/* Data direction register */
	RK_GPIO_INTEN,		/* Interrupt enable register */
	RK_GPIO_INTMASK,	/* Interrupt mask register */
	RK_GPIO_INTTYPE_LEVEL,	/* Interrupt level register */
	RK_GPIO_INTTYPE_BOTH,	/* Both rise and falling edge */
	RK_GPIO_INT_POLARITY,	/* Interrupt polarity register */
	RK_GPIO_INT_STATUS,	/* Interrupt status register */
	RK_GPIO_INT_RAWSTATUS,	/* Raw Interrupt status register */
	RK_GPIO_DEBOUNCE,	/* Debounce enable register */
	RK_GPIO_PORTA_EOI,	/* Clear interrupt register */
	RK_GPIO_EXT_PORTA,	/* External port register */
	RK_GPIO_REGNUM
};

#define	RK_GPIO_LS_SYNC		0x60	/* Level sensitive syncronization enable register */

#define	RK_GPIO_DEFAULT_CAPS	(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT |	\
    GPIO_PIN_PULLUP | GPIO_PIN_PULLDOWN | GPIO_INTR_EDGE_BOTH | \
    GPIO_INTR_EDGE_RISING | GPIO_INTR_EDGE_FALLING | \
    GPIO_INTR_LEVEL_HIGH | GPIO_INTR_LEVEL_LOW)

#define	GPIO_FLAGS_PINCTRL	GPIO_PIN_PULLUP | GPIO_PIN_PULLDOWN
#define	RK_GPIO_MAX_PINS	32

struct pin_cached {
	uint8_t		is_gpio;
	uint32_t	flags;
};

struct rk_pin_irqsrc {
	struct intr_irqsrc	isrc;
	uint32_t		irq;
	uint32_t		mode;
};

struct rk_gpio_softc {
	device_t		sc_dev;
	device_t		sc_busdev;
	struct mtx		sc_mtx;
	struct resource		*sc_res[2];
	bus_space_tag_t		sc_bst;
	bus_space_handle_t	sc_bsh;
	clk_t			clk;
	device_t		pinctrl;
	uint32_t		swporta;
	uint32_t		swporta_ddr;
	uint32_t		version;
	struct pin_cached	pin_cached[RK_GPIO_MAX_PINS];
	uint8_t			regs[RK_GPIO_REGNUM];
	void			*ihandle;
	struct rk_pin_irqsrc	isrcs[RK_GPIO_MAX_PINS];
};

static struct ofw_compat_data compat_data[] = {
	{"rockchip,gpio-bank", 1},
	{NULL,             0}
};

static struct resource_spec rk_gpio_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

#define	RK_GPIO_VERSION		0x78
#define	RK_GPIO_TYPE_V1		0x00000000
#define	RK_GPIO_TYPE_V2		0x01000c2b
#define	RK_GPIO_ISRC(sc, irq)	(&(sc->isrcs[irq].isrc))

static int rk_gpio_detach(device_t dev);

#define	RK_GPIO_LOCK(_sc)		mtx_lock_spin(&(_sc)->sc_mtx)
#define	RK_GPIO_UNLOCK(_sc)		mtx_unlock_spin(&(_sc)->sc_mtx)
#define	RK_GPIO_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->sc_mtx, MA_OWNED)

#define	RK_GPIO_WRITE(_sc, _off, _val)		\
    bus_space_write_4(_sc->sc_bst, _sc->sc_bsh, _off, _val)
#define	RK_GPIO_READ(_sc, _off)		\
    bus_space_read_4(_sc->sc_bst, _sc->sc_bsh, _off)

static int
rk_gpio_read_bit(struct rk_gpio_softc *sc, int reg, int bit)
{
	int offset = sc->regs[reg];
	uint32_t value;

	if (sc->version == RK_GPIO_TYPE_V1) {
		value = RK_GPIO_READ(sc, offset);
		value >>= bit;
	} else {
		value = RK_GPIO_READ(sc, bit > 15 ? offset + 4 : offset);
		value >>= (bit % 16);
	}
	return (value & 1);
}

static void
rk_gpio_write_bit(struct rk_gpio_softc *sc, int reg, int bit, int data)
{
	int offset = sc->regs[reg];
	uint32_t value;

	if (sc->version == RK_GPIO_TYPE_V1) {
		value = RK_GPIO_READ(sc, offset);
		if (data)
			value |= (1 << bit);
		else
			value &= ~(1 << bit);
		RK_GPIO_WRITE(sc, offset, value);
	} else {
		if (data)
			value = (1 << (bit % 16));
		else
			value = 0;
		value |= (1 << ((bit % 16) + 16));
		RK_GPIO_WRITE(sc, bit > 15 ? offset + 4 : offset, value);
	}
}

static uint32_t
rk_gpio_read_4(struct rk_gpio_softc *sc, int reg)
{
	int offset = sc->regs[reg];
	uint32_t value;

	if (sc->version == RK_GPIO_TYPE_V1)
		value = RK_GPIO_READ(sc, offset);
	else
		value = (RK_GPIO_READ(sc, offset) & 0xffff) |
		    (RK_GPIO_READ(sc, offset + 4) << 16);
	return (value);
}

static void
rk_gpio_write_4(struct rk_gpio_softc *sc, int reg, uint32_t value)
{
	int offset = sc->regs[reg];

	if (sc->version == RK_GPIO_TYPE_V1)
		RK_GPIO_WRITE(sc, offset, value);
	else {
		RK_GPIO_WRITE(sc, offset, (value & 0xffff) | 0xffff0000);
		RK_GPIO_WRITE(sc, offset + 4, (value >> 16) | 0xffff0000);
	}
}

static int
rk_gpio_intr(void *arg)
{
	struct rk_gpio_softc *sc = (struct rk_gpio_softc *)arg;;
	struct trapframe *tf = curthread->td_intr_frame;
	uint32_t status;

	RK_GPIO_LOCK(sc);
	status = rk_gpio_read_4(sc, RK_GPIO_INT_STATUS);
	rk_gpio_write_4(sc, RK_GPIO_PORTA_EOI, status);
	RK_GPIO_UNLOCK(sc);

	while (status) {
		int pin = ffs(status) - 1;

		status &= ~(1 << pin);
		if (intr_isrc_dispatch(RK_GPIO_ISRC(sc, pin), tf)) {
			device_printf(sc->sc_dev, "Interrupt pin=%d unhandled\n",
			    pin);
			continue;
		}

		if ((sc->version == RK_GPIO_TYPE_V1) &&
		    (sc->isrcs[pin].mode & GPIO_INTR_EDGE_BOTH)) {
			RK_GPIO_LOCK(sc);
			if (rk_gpio_read_bit(sc, RK_GPIO_EXT_PORTA, pin))
				rk_gpio_write_bit(sc, RK_GPIO_INT_POLARITY,
				    (1 << pin), 0);
			else
				rk_gpio_write_bit(sc, RK_GPIO_INT_POLARITY,
				    (1 << pin), 1);
			RK_GPIO_UNLOCK(sc);
		}
	}
	return (FILTER_HANDLED);
}

static int
rk_gpio_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "RockChip GPIO Bank controller");
	return (BUS_PROBE_DEFAULT);
}

static int
rk_gpio_attach(device_t dev)
{
	struct rk_gpio_softc *sc;
	phandle_t parent_node, node;
	int err, i;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->pinctrl = device_get_parent(dev);
	parent_node = ofw_bus_get_node(sc->pinctrl);

	node = ofw_bus_get_node(sc->sc_dev);
	if (!OF_hasprop(node, "gpio-controller"))
		return (ENXIO);

	mtx_init(&sc->sc_mtx, "rk gpio", "gpio", MTX_SPIN);

	if (bus_alloc_resources(dev, rk_gpio_spec, sc->sc_res)) {
		device_printf(dev, "could not allocate resources\n");
		bus_release_resources(dev, rk_gpio_spec, sc->sc_res);
		mtx_destroy(&sc->sc_mtx);
		return (ENXIO);
	}

	sc->sc_bst = rman_get_bustag(sc->sc_res[0]);
	sc->sc_bsh = rman_get_bushandle(sc->sc_res[0]);

	if (clk_get_by_ofw_index(dev, 0, 0, &sc->clk) != 0) {
		device_printf(dev, "Cannot get clock\n");
		rk_gpio_detach(dev);
		return (ENXIO);
	}
	err = clk_enable(sc->clk);
	if (err != 0) {
		device_printf(dev, "Could not enable clock %s\n",
		    clk_get_name(sc->clk));
		rk_gpio_detach(dev);
		return (ENXIO);
	}

	if ((err = bus_setup_intr(dev, sc->sc_res[1],
	    INTR_TYPE_MISC | INTR_MPSAFE, rk_gpio_intr, NULL,
	    sc, &sc->ihandle))) {
		device_printf(dev, "Can not setup IRQ\n");
		rk_gpio_detach(dev);
		return (ENXIO);
	}

	/*
	 * RK3568 has GPIO_VER_ID register, however both
	 * RK3328 and RK3399 doesn't have. So choose the
	 * version based on parent's compat string.
	 */
	if (ofw_bus_node_is_compatible(parent_node, "rockchip,rk3568-pinctrl"))
		sc->version = RK_GPIO_TYPE_V2;
	else
		sc->version = RK_GPIO_TYPE_V1;

	switch (sc->version) {
	case RK_GPIO_TYPE_V1:
		sc->regs[RK_GPIO_SWPORTA_DR] = 0x00;
		sc->regs[RK_GPIO_SWPORTA_DDR] = 0x04;
		sc->regs[RK_GPIO_INTEN] = 0x30;
		sc->regs[RK_GPIO_INTMASK] = 0x34;
		sc->regs[RK_GPIO_INTTYPE_LEVEL] = 0x38;
		sc->regs[RK_GPIO_INT_POLARITY] = 0x3c;
		sc->regs[RK_GPIO_INT_STATUS] = 0x40;
		sc->regs[RK_GPIO_INT_RAWSTATUS] = 0x44;
		sc->regs[RK_GPIO_DEBOUNCE] = 0x48;
		sc->regs[RK_GPIO_PORTA_EOI] = 0x4c;
		sc->regs[RK_GPIO_EXT_PORTA] = 0x50;
		break;
	case RK_GPIO_TYPE_V2:
		sc->regs[RK_GPIO_SWPORTA_DR] = 0x00;
		sc->regs[RK_GPIO_SWPORTA_DDR] = 0x08;
		sc->regs[RK_GPIO_INTEN] = 0x10;
		sc->regs[RK_GPIO_INTMASK] = 0x18;
		sc->regs[RK_GPIO_INTTYPE_LEVEL] = 0x20;
		sc->regs[RK_GPIO_INTTYPE_BOTH] = 0x30;
		sc->regs[RK_GPIO_INT_POLARITY] = 0x28;
		sc->regs[RK_GPIO_INT_STATUS] = 0x50;
		sc->regs[RK_GPIO_INT_RAWSTATUS] = 0x58;
		sc->regs[RK_GPIO_DEBOUNCE] = 0x38;
		sc->regs[RK_GPIO_PORTA_EOI] = 0x60;
		sc->regs[RK_GPIO_EXT_PORTA] = 0x70;
		break;
	default:
		device_printf(dev, "Unknown gpio version %08x\n", sc->version);
		rk_gpio_detach(dev);
		return (ENXIO);
	}

	for (i = 0; i < RK_GPIO_MAX_PINS; i++) {
		sc->isrcs[i].irq = i;
		sc->isrcs[i].mode = GPIO_INTR_CONFORM;
		if ((err = intr_isrc_register(RK_GPIO_ISRC(sc, i),
		    dev, 0, "%s", device_get_nameunit(dev)))) {
			device_printf(dev, "Can not register isrc %d\n", err);
			rk_gpio_detach(dev);
			return (ENXIO);
		}
	}

	if (intr_pic_register(dev, OF_xref_from_node(node)) == NULL) {
		device_printf(dev, "Can not register pic\n");
		rk_gpio_detach(dev);
		return (ENXIO);
	}

	sc->sc_busdev = gpiobus_attach_bus(dev);
	if (sc->sc_busdev == NULL) {
		rk_gpio_detach(dev);
		return (ENXIO);
	}

	/* Set the cached value to unknown */
	for (i = 0; i < RK_GPIO_MAX_PINS; i++)
		sc->pin_cached[i].is_gpio = 2;

	RK_GPIO_LOCK(sc);
	sc->swporta = rk_gpio_read_4(sc, RK_GPIO_SWPORTA_DR);
	sc->swporta_ddr = rk_gpio_read_4(sc, RK_GPIO_SWPORTA_DDR);
	RK_GPIO_UNLOCK(sc);

	return (0);
}

static int
rk_gpio_detach(device_t dev)
{
	struct rk_gpio_softc *sc;

	sc = device_get_softc(dev);

	if (sc->sc_busdev)
		gpiobus_detach_bus(dev);
	bus_release_resources(dev, rk_gpio_spec, sc->sc_res);
	mtx_destroy(&sc->sc_mtx);
	clk_disable(sc->clk);

	return(0);
}

static device_t
rk_gpio_get_bus(device_t dev)
{
	struct rk_gpio_softc *sc;

	sc = device_get_softc(dev);

	return (sc->sc_busdev);
}

static int
rk_gpio_pin_max(device_t dev, int *maxpin)
{

	/* Each bank have always 32 pins */
	/* XXX not true*/
	*maxpin = 31;
	return (0);
}

static int
rk_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct rk_gpio_softc *sc;
	uint32_t bank;

	sc = device_get_softc(dev);

	if (pin >= 32)
		return (EINVAL);

	bank = pin / 8;
	pin = pin - (bank * 8);
	RK_GPIO_LOCK(sc);
	snprintf(name, GPIOMAXNAME, "P%c%d", bank + 'A', pin);
	RK_GPIO_UNLOCK(sc);

	return (0);
}

static int
rk_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct rk_gpio_softc *sc;
	int rv;

	sc = device_get_softc(dev);

	if (__predict_false(sc->pin_cached[pin].is_gpio != 1)) {
		rv = FDT_PINCTRL_IS_GPIO(sc->pinctrl, dev, pin, (bool *)&sc->pin_cached[pin].is_gpio);
		if (rv != 0)
			return (rv);
		if (sc->pin_cached[pin].is_gpio == 0)
			return (EINVAL);
	}
	*flags = 0;
	rv = FDT_PINCTRL_GET_FLAGS(sc->pinctrl, dev, pin, flags);
	if (rv != 0)
		return (rv);
	sc->pin_cached[pin].flags = *flags;

	if (sc->swporta_ddr & (1 << pin))
		*flags |= GPIO_PIN_OUTPUT;
	else
		*flags |= GPIO_PIN_INPUT;

	return (0);
}

static int
rk_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{

	if (pin >= RK_GPIO_MAX_PINS)
		return EINVAL;

	*caps = RK_GPIO_DEFAULT_CAPS;
	return (0);
}

static int
rk_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct rk_gpio_softc *sc;
	int rv;

	sc = device_get_softc(dev);

	if (pin >= RK_GPIO_MAX_PINS)
		return (EINVAL);

	if (__predict_false(sc->pin_cached[pin].is_gpio != 1)) {
		rv = FDT_PINCTRL_IS_GPIO(sc->pinctrl, dev, pin, (bool *)&sc->pin_cached[pin].is_gpio);
		if (rv != 0)
			return (rv);
		if (sc->pin_cached[pin].is_gpio == 0)
			return (EINVAL);
	}

	if (__predict_false((flags & GPIO_PIN_INPUT) && ((flags & GPIO_FLAGS_PINCTRL) != sc->pin_cached[pin].flags))) {
		rv = FDT_PINCTRL_SET_FLAGS(sc->pinctrl, dev, pin, flags);
		sc->pin_cached[pin].flags = flags & GPIO_FLAGS_PINCTRL;
		if (rv != 0)
			return (rv);
	}

	RK_GPIO_LOCK(sc);
	if (flags & GPIO_PIN_INPUT)
		sc->swporta_ddr &= ~(1 << pin);
	else if (flags & GPIO_PIN_OUTPUT)
		sc->swporta_ddr |= (1 << pin);

	rk_gpio_write_4(sc, RK_GPIO_SWPORTA_DDR, sc->swporta_ddr);
	RK_GPIO_UNLOCK(sc);

	return (0);
}

static int
rk_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct rk_gpio_softc *sc;

	sc = device_get_softc(dev);

	if (pin >= RK_GPIO_MAX_PINS)
		return (EINVAL);

	RK_GPIO_LOCK(sc);
	*val = rk_gpio_read_bit(sc, RK_GPIO_EXT_PORTA, pin);
	RK_GPIO_UNLOCK(sc);

	return (0);
}

static int
rk_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct rk_gpio_softc *sc;

	sc = device_get_softc(dev);

	if (pin >= RK_GPIO_MAX_PINS)
		return (EINVAL);

	RK_GPIO_LOCK(sc);
	if (value)
		sc->swporta |= (1 << pin);
	else
		sc->swporta &= ~(1 << pin);
	rk_gpio_write_4(sc, RK_GPIO_SWPORTA_DR, sc->swporta);
	RK_GPIO_UNLOCK(sc);

	return (0);
}

static int
rk_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct rk_gpio_softc *sc;

	sc = device_get_softc(dev);

	if (pin >= RK_GPIO_MAX_PINS)
		return (EINVAL);

	RK_GPIO_LOCK(sc);
	if (sc->swporta & (1 << pin))
		sc->swporta &= ~(1 << pin);
	else
		sc->swporta |= (1 << pin);
	rk_gpio_write_4(sc, RK_GPIO_SWPORTA_DR, sc->swporta);
	RK_GPIO_UNLOCK(sc);

	return (0);
}

static int
rk_gpio_pin_access_32(device_t dev, uint32_t first_pin, uint32_t clear_pins,
    uint32_t change_pins, uint32_t *orig_pins)
{
	struct rk_gpio_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	RK_GPIO_LOCK(sc);
	reg = rk_gpio_read_4(sc, RK_GPIO_SWPORTA_DR);
	if (orig_pins)
		*orig_pins = reg;
	sc->swporta = reg;

	if ((clear_pins | change_pins) != 0) {
		reg = (reg & ~clear_pins) ^ change_pins;
		rk_gpio_write_4(sc, RK_GPIO_SWPORTA_DR, reg);
	}
	RK_GPIO_UNLOCK(sc);

	return (0);
}

static int
rk_gpio_pin_config_32(device_t dev, uint32_t first_pin, uint32_t num_pins,
    uint32_t *pin_flags)
{
	struct rk_gpio_softc *sc;
	uint32_t reg, set, mask, flags;
	int i;

	sc = device_get_softc(dev);

	if (first_pin != 0 || num_pins > 32)
		return (EINVAL);

	set = 0;
	mask = 0;
	for (i = 0; i < num_pins; i++) {
		mask = (mask << 1) | 1;
		flags = pin_flags[i];
		if (flags & GPIO_PIN_INPUT) {
			set &= ~(1 << i);
		} else if (flags & GPIO_PIN_OUTPUT) {
			set |= (1 << i);
		}
	}

	RK_GPIO_LOCK(sc);
	reg = rk_gpio_read_4(sc, RK_GPIO_SWPORTA_DDR);
	reg &= ~mask;
	reg |= set;
	rk_gpio_write_4(sc, RK_GPIO_SWPORTA_DDR, reg);
	sc->swporta_ddr = reg;
	RK_GPIO_UNLOCK(sc);

	return (0);
}

static int
rk_gpio_map_gpios(device_t bus, phandle_t dev, phandle_t gparent, int gcells,
    pcell_t *gpios, uint32_t *pin, uint32_t *flags)
{

	/* The gpios are mapped as <pin flags> */
	*pin = gpios[0];
	*flags = gpios[1];
	return (0);
}

static phandle_t
rk_gpio_get_node(device_t bus, device_t dev)
{

	/* We only have one child, the GPIO bus, which needs our own node. */
	return (ofw_bus_get_node(bus));
}

static int
rk_pic_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	struct rk_gpio_softc *sc = device_get_softc(dev);
	struct intr_map_data_gpio *gdata;
	uint32_t irq;

	if (data->type != INTR_MAP_DATA_GPIO) {
		device_printf(dev, "Wrong type\n");
		return (ENOTSUP);
	}
	gdata = (struct intr_map_data_gpio *)data;
	irq = gdata->gpio_pin_num;
	if (irq >= RK_GPIO_MAX_PINS) {
		device_printf(dev, "Invalid interrupt %u\n", irq);
		return (EINVAL);
	}
	*isrcp = RK_GPIO_ISRC(sc, irq);
	return (0);
}

static int
rk_pic_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct rk_gpio_softc *sc = device_get_softc(dev);
	struct rk_pin_irqsrc *rkisrc = (struct rk_pin_irqsrc *)isrc;
	struct intr_map_data_gpio *gdata;
	uint32_t mode;
	uint8_t pin;

	if (!data) {
		device_printf(dev, "No map data\n");
		return (ENOTSUP);
	}
	gdata = (struct intr_map_data_gpio *)data;
	mode = gdata->gpio_intr_mode;
	pin = gdata->gpio_pin_num;

	if (rkisrc->irq != gdata->gpio_pin_num) {
		device_printf(dev, "Interrupts don't match\n");
		return (EINVAL);
	}

	if (isrc->isrc_handlers != 0) {
		device_printf(dev, "Handler already attached\n");
		return (rkisrc->mode == mode ? 0 : EINVAL);
	}
	rkisrc->mode = mode;

	RK_GPIO_LOCK(sc);

	switch (mode & GPIO_INTR_MASK) {
	case GPIO_INTR_EDGE_RISING:
		rk_gpio_write_bit(sc, RK_GPIO_SWPORTA_DDR, pin, 0);
		rk_gpio_write_bit(sc, RK_GPIO_INTTYPE_LEVEL, pin, 1);
		rk_gpio_write_bit(sc, RK_GPIO_INT_POLARITY, pin, 1);
		break;
	case GPIO_INTR_EDGE_FALLING:
		rk_gpio_write_bit(sc, RK_GPIO_SWPORTA_DDR, pin, 0);
		rk_gpio_write_bit(sc, RK_GPIO_INTTYPE_LEVEL, pin, 1);
		rk_gpio_write_bit(sc, RK_GPIO_INT_POLARITY, pin, 0);
		break;
	case GPIO_INTR_EDGE_BOTH:
		rk_gpio_write_bit(sc, RK_GPIO_SWPORTA_DDR, pin, 0);
		rk_gpio_write_bit(sc, RK_GPIO_INTTYPE_LEVEL, pin, 1);
		if (sc->version == RK_GPIO_TYPE_V1) {
			if (rk_gpio_read_bit(sc, RK_GPIO_EXT_PORTA, pin))
				rk_gpio_write_bit(sc, RK_GPIO_INT_POLARITY,
				    pin, 0);
			else
				rk_gpio_write_bit(sc, RK_GPIO_INT_POLARITY,
				    pin, 1);
		} else
			rk_gpio_write_bit(sc, RK_GPIO_INTTYPE_BOTH, pin, 1);
		break;
	case GPIO_INTR_LEVEL_HIGH:
		rk_gpio_write_bit(sc, RK_GPIO_SWPORTA_DDR, pin, 0);
		rk_gpio_write_bit(sc, RK_GPIO_INTTYPE_LEVEL, pin, 0);
		rk_gpio_write_bit(sc, RK_GPIO_INT_POLARITY, pin, 1);
		break;
	case GPIO_INTR_LEVEL_LOW:
		rk_gpio_write_bit(sc, RK_GPIO_SWPORTA_DDR, pin, 0);
		rk_gpio_write_bit(sc, RK_GPIO_INTTYPE_LEVEL, pin, 0);
		rk_gpio_write_bit(sc, RK_GPIO_INT_POLARITY, pin, 0);
		break;
	default:
		rk_gpio_write_bit(sc, RK_GPIO_INTMASK, pin, 1);
		rk_gpio_write_bit(sc, RK_GPIO_INTEN, pin, 0);
		RK_GPIO_UNLOCK(sc);
		return (EINVAL);
	}
	rk_gpio_write_bit(sc, RK_GPIO_DEBOUNCE, pin, 1);
	rk_gpio_write_bit(sc, RK_GPIO_INTMASK, pin, 0);
	rk_gpio_write_bit(sc, RK_GPIO_INTEN, pin, 1);
	RK_GPIO_UNLOCK(sc);

	return (0);
}

static int
rk_pic_teardown_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct rk_gpio_softc *sc = device_get_softc(dev);
	struct rk_pin_irqsrc *irqsrc;

	irqsrc = (struct rk_pin_irqsrc *)isrc;

	if (isrc->isrc_handlers == 0) {
		irqsrc->mode = GPIO_INTR_CONFORM;
		RK_GPIO_LOCK(sc);
		rk_gpio_write_bit(sc, RK_GPIO_INTEN, irqsrc->irq, 0);
		rk_gpio_write_bit(sc, RK_GPIO_INTMASK, irqsrc->irq, 0);
		rk_gpio_write_bit(sc, RK_GPIO_DEBOUNCE, irqsrc->irq, 0);
		RK_GPIO_UNLOCK(sc);
	}
	return (0);
}

static device_method_t rk_gpio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk_gpio_probe),
	DEVMETHOD(device_attach,	rk_gpio_attach),
	DEVMETHOD(device_detach,	rk_gpio_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus,		rk_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,		rk_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname,	rk_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags,	rk_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps,	rk_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags,	rk_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get,		rk_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		rk_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	rk_gpio_pin_toggle),
	DEVMETHOD(gpio_pin_access_32,	rk_gpio_pin_access_32),
	DEVMETHOD(gpio_pin_config_32,	rk_gpio_pin_config_32),
	DEVMETHOD(gpio_map_gpios,	rk_gpio_map_gpios),

	/* Interrupt controller interface */
	DEVMETHOD(pic_map_intr,		rk_pic_map_intr),
	DEVMETHOD(pic_setup_intr,	rk_pic_setup_intr),
	DEVMETHOD(pic_teardown_intr,	rk_pic_teardown_intr),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	rk_gpio_get_node),

	DEVMETHOD_END
};

static driver_t rk_gpio_driver = {
	"gpio",
	rk_gpio_methods,
	sizeof(struct rk_gpio_softc),
};

/*
 * GPIO driver is always a child of rk_pinctrl driver and should be probed
 * and attached within rk_pinctrl_attach function. Due to this, bus pass order
 * must be same as bus pass order of rk_pinctrl driver.
 */
EARLY_DRIVER_MODULE(rk_gpio, simplebus, rk_gpio_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
