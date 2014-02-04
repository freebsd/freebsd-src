/*-
 * Copyright (c) 2011 Jakub Wojciech Klama <jceel@FreeBSD.org>
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

/*
 * GPIO on LPC32x0 consist of 4 ports:
 * - Port0 with 8 input/output pins
 * - Port1 with 24 input/output pins
 * - Port2 with 13 input/output pins
 * - Port3 with:
 *   - 26 input pins (GPI_00..GPI_09 + GPI_15..GPI_23 + GPI_25 + GPI_27..GPI_28)
 *   - 24 output pins (GPO_00..GPO_23)
 *   - 6 input/output pins (GPIO_00..GPIO_05)
 *
 * Pins are mapped to logical pin number as follows:
 * [0..9] -> GPI_00..GPI_09 		(port 3)
 * [10..18] -> GPI_15..GPI_23 		(port 3)
 * [19] -> GPI_25			(port 3)
 * [20..21] -> GPI_27..GPI_28		(port 3)
 * [22..45] -> GPO_00..GPO_23		(port 3)
 * [46..51] -> GPIO_00..GPIO_05		(port 3)
 * [52..64] -> P2.0..P2.12		(port 2)
 * [65..88] -> P1.0..P1.23		(port 1)
 * [89..96] -> P0.0..P0.7		(port 0)
 *
 */


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/intr.h>
#include <machine/fdt.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/lpc/lpcreg.h>
#include <arm/lpc/lpcvar.h>

#include "gpio_if.h"

struct lpc_gpio_softc
{
	device_t		lg_dev;
	struct resource *	lg_res;
	bus_space_tag_t		lg_bst;
	bus_space_handle_t	lg_bsh;
};

struct lpc_gpio_pinmap
{
	int			lp_start_idx;
	int			lp_pin_count;
	int			lp_port;
	int			lp_start_bit;
	int			lp_flags;
};

static const struct lpc_gpio_pinmap lpc_gpio_pins[] = {
	{ 0,	10,	3,	0,	GPIO_PIN_INPUT },
	{ 10,	9,	3,	15,	GPIO_PIN_INPUT },
	{ 19,	1,	3,	25,	GPIO_PIN_INPUT },
	{ 20,	2,	3,	27,	GPIO_PIN_INPUT },
	{ 22,	24,	3,	0,	GPIO_PIN_OUTPUT },
	/*
	 * -1 below is to mark special case for Port3 GPIO pins, as they
	 * have other bits in Port 3 registers as inputs and as outputs
	 */
	{ 46,	6,	3,	-1,	GPIO_PIN_INPUT | GPIO_PIN_OUTPUT },
	{ 52,	13,	2,	0,	GPIO_PIN_INPUT | GPIO_PIN_OUTPUT },
	{ 65,	24,	1,	0,	GPIO_PIN_INPUT | GPIO_PIN_OUTPUT },
	{ 89,	8,	0,	0,	GPIO_PIN_INPUT | GPIO_PIN_OUTPUT },
	{ -1,	-1,	-1,	-1,	-1 },
};

#define	LPC_GPIO_NPINS				\
    (LPC_GPIO_P0_COUNT + LPC_GPIO_P1_COUNT +	\
    LPC_GPIO_P2_COUNT + LPC_GPIO_P3_COUNT)

#define	LPC_GPIO_PIN_IDX(_map, _idx)	\
    (_idx - _map->lp_start_idx)

#define	LPC_GPIO_PIN_BIT(_map, _idx)	\
    (_map->lp_start_bit + LPC_GPIO_PIN_IDX(_map, _idx))

static int lpc_gpio_probe(device_t);
static int lpc_gpio_attach(device_t);
static int lpc_gpio_detach(device_t);

static int lpc_gpio_pin_max(device_t, int *);
static int lpc_gpio_pin_getcaps(device_t, uint32_t, uint32_t *);
static int lpc_gpio_pin_getflags(device_t, uint32_t, uint32_t *);
static int lpc_gpio_pin_setflags(device_t, uint32_t, uint32_t);
static int lpc_gpio_pin_getname(device_t, uint32_t, char *);
static int lpc_gpio_pin_get(device_t, uint32_t, uint32_t *);
static int lpc_gpio_pin_set(device_t, uint32_t, uint32_t);
static int lpc_gpio_pin_toggle(device_t, uint32_t);

static const struct lpc_gpio_pinmap *lpc_gpio_get_pinmap(int);

static struct lpc_gpio_softc *lpc_gpio_sc = NULL;

#define	lpc_gpio_read_4(_sc, _reg) \
    bus_space_read_4(_sc->lg_bst, _sc->lg_bsh, _reg)
#define	lpc_gpio_write_4(_sc, _reg, _val) \
    bus_space_write_4(_sc->lg_bst, _sc->lg_bsh, _reg, _val)
#define	lpc_gpio_get_4(_sc, _test, _reg1, _reg2) \
    lpc_gpio_read_4(_sc, ((_test) ? _reg1 : _reg2))
#define	lpc_gpio_set_4(_sc, _test, _reg1, _reg2, _val) \
    lpc_gpio_write_4(_sc, ((_test) ? _reg1 : _reg2), _val)

static int
lpc_gpio_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "lpc,gpio"))
		return (ENXIO);

	device_set_desc(dev, "LPC32x0 GPIO");
	return (BUS_PROBE_DEFAULT);
}

static int
lpc_gpio_attach(device_t dev)
{
	struct lpc_gpio_softc *sc = device_get_softc(dev);
	int rid;

	sc->lg_dev = dev;

	rid = 0;
	sc->lg_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->lg_res) {
		device_printf(dev, "cannot allocate memory window\n");
		return (ENXIO);
	}

	sc->lg_bst = rman_get_bustag(sc->lg_res);
	sc->lg_bsh = rman_get_bushandle(sc->lg_res);

	lpc_gpio_sc = sc;

	device_add_child(dev, "gpioc", device_get_unit(dev));
	device_add_child(dev, "gpiobus", device_get_unit(dev));

	return (bus_generic_attach(dev));
}

static int
lpc_gpio_detach(device_t dev)
{
	return (EBUSY);
}

static int
lpc_gpio_pin_max(device_t dev, int *npins)
{
	*npins = LPC_GPIO_NPINS - 1;
	return (0);
}

static int
lpc_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	const struct lpc_gpio_pinmap *map;

	if (pin > LPC_GPIO_NPINS)
		return (ENODEV);

	map = lpc_gpio_get_pinmap(pin);

	*caps = map->lp_flags;
	return (0);
}

static int
lpc_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct lpc_gpio_softc *sc = device_get_softc(dev);
	const struct lpc_gpio_pinmap *map;
	uint32_t state;
	int dir;

	if (pin > LPC_GPIO_NPINS)
		return (ENODEV);

	map = lpc_gpio_get_pinmap(pin);

	/* Check whether it's bidirectional pin */
	if ((map->lp_flags & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) != 
	    (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) {
		*flags = map->lp_flags;
		return (0);
	}

	switch (map->lp_port) {
	case 0:
		state = lpc_gpio_read_4(sc, LPC_GPIO_P0_DIR_STATE);
		dir = (state & (1 << LPC_GPIO_PIN_BIT(map, pin)));
		break;
	case 1:
		state = lpc_gpio_read_4(sc, LPC_GPIO_P1_DIR_STATE);
		dir = (state & (1 << LPC_GPIO_PIN_BIT(map, pin)));
		break;
	case 2:
		state = lpc_gpio_read_4(sc, LPC_GPIO_P2_DIR_STATE);
		dir = (state & (1 << LPC_GPIO_PIN_BIT(map, pin)));
		break;
	case 3:
		state = lpc_gpio_read_4(sc, LPC_GPIO_P2_DIR_STATE);
		dir = (state & (1 << (25 + LPC_GPIO_PIN_IDX(map, pin))));
		break;
	default:
		panic("unknown GPIO port");
	}

	*flags = dir ? GPIO_PIN_OUTPUT : GPIO_PIN_INPUT;

	return (0);
}

static int
lpc_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct lpc_gpio_softc *sc = device_get_softc(dev);
	const struct lpc_gpio_pinmap *map;
	uint32_t dir, state;

	if (pin > LPC_GPIO_NPINS)
		return (ENODEV);

	map = lpc_gpio_get_pinmap(pin);

	/* Check whether it's bidirectional pin */
	if ((map->lp_flags & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) != 
	    (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT))
		return (ENOTSUP);
	
	if (flags & GPIO_PIN_INPUT)
		dir = 0;

	if (flags & GPIO_PIN_OUTPUT)
		dir = 1;

	switch (map->lp_port) {
	case 0:
		state = (1 << LPC_GPIO_PIN_IDX(map, pin));
		lpc_gpio_set_4(sc, dir, LPC_GPIO_P0_DIR_SET, 
		    LPC_GPIO_P0_DIR_CLR, state);
		break;
	case 1:
		state = (1 << LPC_GPIO_PIN_IDX(map, pin));
		lpc_gpio_set_4(sc, dir, LPC_GPIO_P1_DIR_SET, 
		    LPC_GPIO_P0_DIR_CLR, state);
		break;
	case 2:
		state = (1 << LPC_GPIO_PIN_IDX(map, pin));
		lpc_gpio_set_4(sc, dir, LPC_GPIO_P2_DIR_SET, 
		    LPC_GPIO_P0_DIR_CLR, state);
		break;
	case 3:
		state = (1 << (25 + (pin - map->lp_start_idx)));
		lpc_gpio_set_4(sc, dir, LPC_GPIO_P2_DIR_SET, 
		    LPC_GPIO_P0_DIR_CLR, state);
		break;
	}

	return (0);
}

static int
lpc_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	const struct lpc_gpio_pinmap *map;
	int idx;

	map = lpc_gpio_get_pinmap(pin);
	idx = LPC_GPIO_PIN_IDX(map, pin);

	switch (map->lp_port) {
	case 0:
	case 1:
	case 2:
		snprintf(name, GPIOMAXNAME - 1, "P%d.%d", map->lp_port, 
		    map->lp_start_bit + LPC_GPIO_PIN_IDX(map, pin));
		break;
	case 3:
		if (map->lp_start_bit == -1) {
			snprintf(name, GPIOMAXNAME - 1, "GPIO_%02d", idx);
			break;
		}

		snprintf(name, GPIOMAXNAME - 1, "GP%c_%02d",
		    (map->lp_flags & GPIO_PIN_INPUT) ? 'I' : 'O',
		    map->lp_start_bit + idx);
		break;
	}

	return (0);
}

static int
lpc_gpio_pin_get(device_t dev, uint32_t pin, uint32_t *value)
{
	struct lpc_gpio_softc *sc = device_get_softc(dev);
	const struct lpc_gpio_pinmap *map;
	uint32_t state, flags;
	int dir;

	map = lpc_gpio_get_pinmap(pin);

	if (lpc_gpio_pin_getflags(dev, pin, &flags))
		return (ENXIO);

	if (flags & GPIO_PIN_OUTPUT)
		dir = 1;

	if (flags & GPIO_PIN_INPUT)
		dir = 0;

	switch (map->lp_port) {
	case 0:
		state = lpc_gpio_get_4(sc, dir, LPC_GPIO_P0_OUTP_STATE,
		    LPC_GPIO_P0_INP_STATE);
		*value = !!(state & (1 << LPC_GPIO_PIN_BIT(map, pin)));
	case 1:
		state = lpc_gpio_get_4(sc, dir, LPC_GPIO_P1_OUTP_STATE,
		    LPC_GPIO_P1_INP_STATE);
		*value = !!(state & (1 << LPC_GPIO_PIN_BIT(map, pin)));
	case 2:
		state = lpc_gpio_read_4(sc, LPC_GPIO_P2_INP_STATE);
		*value = !!(state & (1 << LPC_GPIO_PIN_BIT(map, pin)));
	case 3:
		state = lpc_gpio_get_4(sc, dir, LPC_GPIO_P3_OUTP_STATE,
		    LPC_GPIO_P3_INP_STATE);
		if (map->lp_start_bit == -1) {
			if (dir)
				*value = !!(state & (1 << (25 + 
				    LPC_GPIO_PIN_IDX(map, pin))));
			else
				*value = !!(state & (1 << (10 + 
				    LPC_GPIO_PIN_IDX(map, pin))));
		}

		*value = !!(state & (1 << LPC_GPIO_PIN_BIT(map, pin)));
	}

	return (0);
}

static int
lpc_gpio_pin_set(device_t dev, uint32_t pin, uint32_t value)
{
	struct lpc_gpio_softc *sc = device_get_softc(dev);
	const struct lpc_gpio_pinmap *map;
	uint32_t state, flags;

	map = lpc_gpio_get_pinmap(pin);

	if (lpc_gpio_pin_getflags(dev, pin, &flags))
		return (ENXIO);

	if ((flags & GPIO_PIN_OUTPUT) == 0)
		return (EINVAL);

	state = (1 << LPC_GPIO_PIN_BIT(map, pin));

	switch (map->lp_port) {
	case 0:
		lpc_gpio_set_4(sc, value, LPC_GPIO_P0_OUTP_SET,
		    LPC_GPIO_P0_OUTP_CLR, state);
		break;
	case 1:
		lpc_gpio_set_4(sc, value, LPC_GPIO_P1_OUTP_SET,
		    LPC_GPIO_P1_OUTP_CLR, state);
		break;
	case 2:
		lpc_gpio_set_4(sc, value, LPC_GPIO_P2_OUTP_SET,
		    LPC_GPIO_P2_OUTP_CLR, state);
		break;
	case 3:
		if (map->lp_start_bit == -1)
			state = (1 << (25 + LPC_GPIO_PIN_IDX(map, pin)));
		
		lpc_gpio_set_4(sc, value, LPC_GPIO_P3_OUTP_SET,
		    LPC_GPIO_P3_OUTP_CLR, state);
		break;
	}

	return (0);
}

static int
lpc_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	const struct lpc_gpio_pinmap *map;
	uint32_t flags;

	map = lpc_gpio_get_pinmap(pin);

	if (lpc_gpio_pin_getflags(dev, pin, &flags))
		return (ENXIO);

	if ((flags & GPIO_PIN_OUTPUT) == 0)
		return (EINVAL);
	
	panic("not implemented yet");

	return (0);

}

static const struct lpc_gpio_pinmap *
lpc_gpio_get_pinmap(int pin)
{
	const struct lpc_gpio_pinmap *map;

	for (map = &lpc_gpio_pins[0]; map->lp_start_idx != -1; map++) {
		if (pin >= map->lp_start_idx &&
		    pin < map->lp_start_idx + map->lp_pin_count)
			return map;
	}

	panic("pin number %d out of range", pin);
}

int
lpc_gpio_set_flags(device_t dev, int pin, int flags)
{
	if (lpc_gpio_sc == NULL)
		return (ENXIO);

	return lpc_gpio_pin_setflags(lpc_gpio_sc->lg_dev, pin, flags);
}

int
lpc_gpio_set_state(device_t dev, int pin, int state)
{
	if (lpc_gpio_sc == NULL)
		return (ENXIO);

	return lpc_gpio_pin_set(lpc_gpio_sc->lg_dev, pin, state); 
}

int
lpc_gpio_get_state(device_t dev, int pin, int *state)
{
	if (lpc_gpio_sc == NULL)
		return (ENXIO);

	return lpc_gpio_pin_get(lpc_gpio_sc->lg_dev, pin, state);
}

void
platform_gpio_init()
{
	/* Preset SPI devices CS pins to one */
	bus_space_write_4(fdtbus_bs_tag, 
	    LPC_GPIO_BASE, LPC_GPIO_P3_OUTP_SET,
	    1 << (SSD1289_CS_PIN - LPC_GPIO_GPO_00(0)) |
	    1 << (SSD1289_DC_PIN - LPC_GPIO_GPO_00(0)) |
	    1 << (ADS7846_CS_PIN - LPC_GPIO_GPO_00(0)));	
}

static device_method_t lpc_gpio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		lpc_gpio_probe),
	DEVMETHOD(device_attach,	lpc_gpio_attach),
	DEVMETHOD(device_detach,	lpc_gpio_detach),

	/* GPIO interface */
	DEVMETHOD(gpio_pin_max,		lpc_gpio_pin_max),
	DEVMETHOD(gpio_pin_getcaps,	lpc_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	lpc_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,	lpc_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_getname,	lpc_gpio_pin_getname),
	DEVMETHOD(gpio_pin_set,		lpc_gpio_pin_set),
	DEVMETHOD(gpio_pin_get,		lpc_gpio_pin_get),
	DEVMETHOD(gpio_pin_toggle,	lpc_gpio_pin_toggle),

	{ 0, 0 }
};

static devclass_t lpc_gpio_devclass;

static driver_t lpc_gpio_driver = {
	"lpcgpio",
	lpc_gpio_methods,
	sizeof(struct lpc_gpio_softc),
};

extern devclass_t gpiobus_devclass, gpioc_devclass;
extern driver_t gpiobus_driver, gpioc_driver;

DRIVER_MODULE(lpcgpio, simplebus, lpc_gpio_driver, lpc_gpio_devclass, 0, 0);
DRIVER_MODULE(gpiobus, lpcgpio, gpiobus_driver, gpiobus_devclass, 0, 0);
DRIVER_MODULE(gpioc, lpcgpio, gpioc_driver, gpioc_devclass, 0, 0);
MODULE_VERSION(lpcgpio, 1);
