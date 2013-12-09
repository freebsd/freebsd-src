/*-
 * Copyright (c) 2013 Ganbold Tsagaankhuu <ganbold@gmail.com>
 * Copyright (c) 2012 Oleksandr Tymoshenko <gonzo@freebsd.org>
 * Copyright (c) 2012 Luiz Otavio O Souza.
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
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/fdt.h>
#include <machine/intr.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "gpio_if.h"

#include "rk30xx_grf.h"
#include "rk30xx_pmu.h"

/*
 * RK3188 has 4 banks of gpio.
 * 32 pins per bank
 * PA0 - PA7 | PB0 - PB7
 * PC0 - PC7 | PD0 - PD7
 */

#define	RK30_GPIO_PINS		128
#define	RK30_GPIO_DEFAULT_CAPS	(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT | \
    GPIO_PIN_PULLUP | GPIO_PIN_PULLDOWN)

#define	RK30_GPIO_NONE			0
#define	RK30_GPIO_PULLUP		1
#define	RK30_GPIO_PULLDOWN		2

#define	RK30_GPIO_INPUT			0
#define	RK30_GPIO_OUTPUT		1

struct rk30_gpio_softc {
	device_t		sc_dev;
	struct mtx		sc_mtx;
	struct resource *	sc_mem_res;
	struct resource *	sc_irq_res;
	bus_space_tag_t		sc_bst;
	bus_space_handle_t	sc_bsh;
	void *			sc_intrhand;
	int			sc_gpio_npins;
	struct gpio_pin		sc_gpio_pins[RK30_GPIO_PINS];
};

static struct rk30_gpio_softc *rk30_gpio_sc = NULL;

typedef int (*gpios_phandler_t)(phandle_t, pcell_t *, int);

struct gpio_ctrl_entry {
	const char		*compat;
	gpios_phandler_t	handler;
};

int rk30_gpios_prop_handle(phandle_t ctrl, pcell_t *gpios, int len);
int platform_gpio_init(void);

struct gpio_ctrl_entry gpio_controllers[] = {
	{ "rockchip,rk30xx-gpio", &rk30_gpios_prop_handle },
	{ "rockchip,rk30xx-gpio", &rk30_gpios_prop_handle },
	{ "rockchip,rk30xx-gpio", &rk30_gpios_prop_handle },
	{ "rockchip,rk30xx-gpio", &rk30_gpios_prop_handle },
	{ NULL, NULL }
};

#define	RK30_GPIO_LOCK(_sc)		mtx_lock(&_sc->sc_mtx)
#define	RK30_GPIO_UNLOCK(_sc)		mtx_unlock(&_sc->sc_mtx)
#define	RK30_GPIO_LOCK_ASSERT(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED)

#define	RK30_GPIO_SWPORT_DR		0x00
#define	RK30_GPIO_SWPORT_DDR		0x04
#define	RK30_GPIO_INTEN			0x30
#define	RK30_GPIO_INTMASK		0x34
#define	RK30_GPIO_INTTYPE_LEVEL		0x38
#define	RK30_GPIO_INT_POLARITY		0x3c
#define	RK30_GPIO_INT_STATUS		0x40
#define	RK30_GPIO_INT_RAWSTATUS		0x44
#define	RK30_GPIO_DEBOUNCE		0x48
#define	RK30_GPIO_PORTS_EOI		0x4c
#define	RK30_GPIO_EXT_PORT		0x50
#define	RK30_GPIO_LS_SYNC		0x60

#define	RK30_GPIO_WRITE(_sc, _off, _val)		\
    bus_space_write_4(_sc->sc_bst, _sc->sc_bsh, _off, _val)
#define	RK30_GPIO_READ(_sc, _off)			\
    bus_space_read_4(_sc->sc_bst, _sc->sc_bsh, _off)

static uint32_t
rk30_gpio_get_function(struct rk30_gpio_softc *sc, uint32_t pin)
{
	uint32_t bank, func, offset;

	bank = pin / 32;
	pin = pin % 32;
	offset = 1 << pin;

	func = RK30_GPIO_READ(sc, RK30_GPIO_SWPORT_DDR);
	func &= offset;

	return (func);
}

static uint32_t
rk30_gpio_func_flag(uint32_t nfunc)
{

	switch (nfunc) {
	case RK30_GPIO_INPUT:
		return (GPIO_PIN_INPUT);
	case RK30_GPIO_OUTPUT:
		return (GPIO_PIN_OUTPUT);
	}
	return (0);
}

static void
rk30_gpio_set_function(struct rk30_gpio_softc *sc, uint32_t pin, uint32_t f)
{
	uint32_t bank, data, offset;

	/* Must be called with lock held. */
	RK30_GPIO_LOCK_ASSERT(sc);

	bank = pin / 32;
	pin = pin % 32;
	offset = 1 << pin;

	data = RK30_GPIO_READ(sc, RK30_GPIO_SWPORT_DDR);
	if (f)
		data |= offset;
	else
		data &= ~offset;
	RK30_GPIO_WRITE(sc, RK30_GPIO_SWPORT_DDR, data);
}

static void
rk30_gpio_set_pud(struct rk30_gpio_softc *sc, uint32_t pin, uint32_t state)
{
	uint32_t bank;

	bank = pin / 32;

	/* Must be called with lock held. */
	RK30_GPIO_LOCK_ASSERT(sc);

	if (bank == 0 && pin < 12)
		rk30_pmu_gpio_pud(pin, state);
	else
		rk30_grf_gpio_pud(bank, pin, state);
}

static void
rk30_gpio_pin_configure(struct rk30_gpio_softc *sc, struct gpio_pin *pin,
    unsigned int flags)
{

	RK30_GPIO_LOCK(sc);

	/*
	 * Manage input/output.
	 */
	if (flags & (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)) {
		pin->gp_flags &= ~(GPIO_PIN_INPUT|GPIO_PIN_OUTPUT);
		if (flags & GPIO_PIN_OUTPUT) {
			pin->gp_flags |= GPIO_PIN_OUTPUT;
			rk30_gpio_set_function(sc, pin->gp_pin,
			    RK30_GPIO_OUTPUT);
		} else {
			pin->gp_flags |= GPIO_PIN_INPUT;
			rk30_gpio_set_function(sc, pin->gp_pin,
			    RK30_GPIO_INPUT);
		}
	}

	/* Manage Pull-up/pull-down. */
	pin->gp_flags &= ~(GPIO_PIN_PULLUP|GPIO_PIN_PULLDOWN);
	if (flags & (GPIO_PIN_PULLUP|GPIO_PIN_PULLDOWN)) {
		if (flags & GPIO_PIN_PULLUP) {
			pin->gp_flags |= GPIO_PIN_PULLUP;
			rk30_gpio_set_pud(sc, pin->gp_pin, 
			    RK30_GPIO_PULLUP);
		} else {
			pin->gp_flags |= GPIO_PIN_PULLDOWN;
			rk30_gpio_set_pud(sc, pin->gp_pin, 
			    RK30_GPIO_PULLDOWN);
		}
	} else
		rk30_gpio_set_pud(sc, pin->gp_pin, RK30_GPIO_NONE);

	RK30_GPIO_UNLOCK(sc);
}

static int
rk30_gpio_pin_max(device_t dev, int *maxpin)
{

	*maxpin = RK30_GPIO_PINS - 1;
	return (0);
}

static int
rk30_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct rk30_gpio_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < sc->sc_gpio_npins; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->sc_gpio_npins)
		return (EINVAL);

	RK30_GPIO_LOCK(sc);
	*caps = sc->sc_gpio_pins[i].gp_caps;
	RK30_GPIO_UNLOCK(sc);

	return (0);
}

static int
rk30_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct rk30_gpio_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < sc->sc_gpio_npins; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->sc_gpio_npins)
		return (EINVAL);

	RK30_GPIO_LOCK(sc);
	*flags = sc->sc_gpio_pins[i].gp_flags;
	RK30_GPIO_UNLOCK(sc);

	return (0);
}

static int
rk30_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct rk30_gpio_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < sc->sc_gpio_npins; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->sc_gpio_npins)
		return (EINVAL);

	RK30_GPIO_LOCK(sc);
	memcpy(name, sc->sc_gpio_pins[i].gp_name, GPIOMAXNAME);
	RK30_GPIO_UNLOCK(sc);

	return (0);
}

static int
rk30_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct rk30_gpio_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < sc->sc_gpio_npins; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->sc_gpio_npins)
		return (EINVAL);

	/* Check for unwanted flags. */
	if ((flags & sc->sc_gpio_pins[i].gp_caps) != flags)
		return (EINVAL);

	/* Can't mix input/output together. */
	if ((flags & (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)) ==
	    (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT))
		return (EINVAL);

	/* Can't mix pull-up/pull-down together. */
	if ((flags & (GPIO_PIN_PULLUP|GPIO_PIN_PULLDOWN)) ==
	    (GPIO_PIN_PULLUP|GPIO_PIN_PULLDOWN))
		return (EINVAL);

	rk30_gpio_pin_configure(sc, &sc->sc_gpio_pins[i], flags);

	return (0);
}

static int
rk30_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct rk30_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank, offset, data;
	int i;

	for (i = 0; i < sc->sc_gpio_npins; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->sc_gpio_npins)
		return (EINVAL);

	bank = pin / 32;
	pin = pin % 32;
	offset = 1 << pin;

	RK30_GPIO_LOCK(sc);
	data = RK30_GPIO_READ(sc, RK30_GPIO_SWPORT_DDR);
	data |= offset;
	RK30_GPIO_WRITE(sc, RK30_GPIO_SWPORT_DDR, data);

	data = RK30_GPIO_READ(sc, RK30_GPIO_SWPORT_DR);
	if (value)
		data |= offset;
	else
		data &= ~offset;
	RK30_GPIO_WRITE(sc, RK30_GPIO_SWPORT_DR, data);
	RK30_GPIO_UNLOCK(sc);

	return (0);
}

static int
rk30_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct rk30_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank, offset, reg_data;
	int i;

	for (i = 0; i < sc->sc_gpio_npins; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->sc_gpio_npins)
		return (EINVAL);

	bank = pin / 32;
	pin = pin % 32;
	offset = 1 << pin;

	RK30_GPIO_LOCK(sc);
	reg_data = RK30_GPIO_READ(sc, RK30_GPIO_EXT_PORT);
	RK30_GPIO_UNLOCK(sc);
	*val = (reg_data & offset) ? 1 : 0;

	return (0);
}

static int
rk30_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct rk30_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank, data, offset;
	int i;

	for (i = 0; i < sc->sc_gpio_npins; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->sc_gpio_npins)
		return (EINVAL);

	bank = pin / 32;
	pin = pin % 32;
	offset = 1 << pin;

	RK30_GPIO_LOCK(sc);
	data = RK30_GPIO_READ(sc, RK30_GPIO_SWPORT_DDR);
	if (data & offset)
		data &= ~offset;
	else
		data |= offset;
	RK30_GPIO_WRITE(sc, RK30_GPIO_SWPORT_DDR, data);

	data = RK30_GPIO_READ(sc, RK30_GPIO_SWPORT_DR);
	if (data & offset)
		data &= ~offset;
	else
		data |= offset;
	RK30_GPIO_WRITE(sc, RK30_GPIO_SWPORT_DR, data);
	RK30_GPIO_UNLOCK(sc);

	return (0);
}

static int
rk30_gpio_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "rockchip,rk30xx-gpio"))
		return (ENXIO);

	device_set_desc(dev, "Rockchip RK30XX GPIO controller");
	return (BUS_PROBE_DEFAULT);
}

static int
rk30_gpio_attach(device_t dev)
{
	struct rk30_gpio_softc *sc = device_get_softc(dev);
	uint32_t func;
	int i, rid;
	phandle_t gpio;

	if (rk30_gpio_sc)
		return (ENXIO);

	sc->sc_dev = dev;

	mtx_init(&sc->sc_mtx, "rk30 gpio", "gpio", MTX_DEF);

	rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->sc_mem_res) {
		device_printf(dev, "cannot allocate memory window\n");
		return (ENXIO);
	}

	sc->sc_bst = rman_get_bustag(sc->sc_mem_res);
	sc->sc_bsh = rman_get_bushandle(sc->sc_mem_res);

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (!sc->sc_irq_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
		device_printf(dev, "cannot allocate interrupt\n");
		return (ENXIO);
	}

	/* Find our node. */
	gpio = ofw_bus_get_node(sc->sc_dev);

	if (!OF_hasprop(gpio, "gpio-controller"))
		/* Node is not a GPIO controller. */
		goto fail;

	/* Initialize the software controlled pins. */
	for (i = 0; i < RK30_GPIO_PINS; i++) {
		snprintf(sc->sc_gpio_pins[i].gp_name, GPIOMAXNAME,
		    "pin %d", i);
		func = rk30_gpio_get_function(sc, i);
		sc->sc_gpio_pins[i].gp_pin = i;
		sc->sc_gpio_pins[i].gp_caps = RK30_GPIO_DEFAULT_CAPS;
		sc->sc_gpio_pins[i].gp_flags = rk30_gpio_func_flag(func);
	}
	sc->sc_gpio_npins = i;

	device_add_child(dev, "gpioc", device_get_unit(dev));
	device_add_child(dev, "gpiobus", device_get_unit(dev));

	rk30_gpio_sc = sc;

	platform_gpio_init();
	
	return (bus_generic_attach(dev));

fail:
	if (sc->sc_irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
	if (sc->sc_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
	return (ENXIO);
}

static int
rk30_gpio_detach(device_t dev)
{

	return (EBUSY);
}

static device_method_t rk30_gpio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk30_gpio_probe),
	DEVMETHOD(device_attach,	rk30_gpio_attach),
	DEVMETHOD(device_detach,	rk30_gpio_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_pin_max,		rk30_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname,	rk30_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags,	rk30_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps,	rk30_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags,	rk30_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get,		rk30_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		rk30_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	rk30_gpio_pin_toggle),

	DEVMETHOD_END
};

static devclass_t rk30_gpio_devclass;

static driver_t rk30_gpio_driver = {
	"gpio",
	rk30_gpio_methods,
	sizeof(struct rk30_gpio_softc),
};

DRIVER_MODULE(rk30_gpio, simplebus, rk30_gpio_driver, rk30_gpio_devclass, 0, 0);

int
rk30_gpios_prop_handle(phandle_t ctrl, pcell_t *gpios, int len)
{
	struct rk30_gpio_softc *sc;
	pcell_t gpio_cells;
	int inc, t, tuples, tuple_size;
	int dir, flags, pin, i;
	u_long gpio_ctrl, size;

	sc = rk30_gpio_sc;
	if (sc == NULL)
		return ENXIO;

	if (OF_getprop(ctrl, "#gpio-cells", &gpio_cells, sizeof(pcell_t)) < 0)
		return (ENXIO);

	gpio_cells = fdt32_to_cpu(gpio_cells);
	if (gpio_cells != 2)
		return (ENXIO);

	tuple_size = gpio_cells * sizeof(pcell_t) + sizeof(phandle_t);
	tuples = len / tuple_size;

	if (fdt_regsize(ctrl, &gpio_ctrl, &size))
		return (ENXIO);

	/*
	 * Skip controller reference, since controller's phandle is given
	 * explicitly (in a function argument).
	 */
	inc = sizeof(ihandle_t) / sizeof(pcell_t);
	gpios += inc;
	for (t = 0; t < tuples; t++) {
		pin = fdt32_to_cpu(gpios[0]);
		dir = fdt32_to_cpu(gpios[1]);
		flags = fdt32_to_cpu(gpios[2]);

		for (i = 0; i < sc->sc_gpio_npins; i++) {
			if (sc->sc_gpio_pins[i].gp_pin == pin)
				break;
		}
		if (i >= sc->sc_gpio_npins)
			return (EINVAL);

		rk30_gpio_pin_configure(sc, &sc->sc_gpio_pins[i], flags);

		if (dir == 1) {
			/* Input. */
			rk30_gpio_pin_set(sc->sc_dev, pin, RK30_GPIO_INPUT);
		} else {
			/* Output. */
			rk30_gpio_pin_set(sc->sc_dev, pin, RK30_GPIO_OUTPUT);
		}
		gpios += gpio_cells + inc;
	}

	return (0);
}

#define	MAX_PINS_PER_NODE	5
#define	GPIOS_PROP_CELLS	4

int
platform_gpio_init(void)
{
	phandle_t child, parent, root, ctrl;
	pcell_t gpios[MAX_PINS_PER_NODE * GPIOS_PROP_CELLS];
	struct gpio_ctrl_entry *e;
	int len, rv;

	root = OF_finddevice("/");
	len = 0;
	parent = root;

	/* Traverse through entire tree to find nodes with 'gpios' prop */
	for (child = OF_child(parent); child != 0; child = OF_peer(child)) {

		/* Find a 'leaf'. Start the search from this node. */
		while (OF_child(child)) {
			parent = child;
			child = OF_child(child);
		}
		if ((len = OF_getproplen(child, "gpios")) > 0) {

			if (len > sizeof(gpios))
				return (ENXIO);

			/* Get 'gpios' property. */
			OF_getprop(child, "gpios", &gpios, len);

			e = (struct gpio_ctrl_entry *)&gpio_controllers;

			/* Find and call a handler. */
			for (; e->compat; e++) {
				/*
				 * First cell of 'gpios' property should
				 * contain a ref. to a node defining GPIO
				 * controller.
				 */
				ctrl = OF_xref_phandle(fdt32_to_cpu(gpios[0]));

				if (fdt_is_compatible(ctrl, e->compat))
					/* Call a handler. */
					if ((rv = e->handler(ctrl,
					    (pcell_t *)&gpios, len)))
						return (rv);
			}
		}

		if (OF_peer(child) == 0) {
			/* No more siblings. */
			child = parent;
			parent = OF_parent(child);
		}
	}
	return (0);
}
