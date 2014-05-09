/*-
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
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
 */

/*
 * Samsung Exynos 5 Pad Control
 * Chapter 4, Exynos 5 Dual User's Manual Public Rev 1.00
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>
#include <sys/mutex.h>
#include <sys/gpio.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include "gpio_if.h"

#include <arm/samsung/exynos/exynos5_combiner.h>
#include <arm/samsung/exynos/exynos5_pad.h>

#define	GPIO_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	GPIO_UNLOCK(_sc)	mtx_unlock(&(_sc)->sc_mtx)

#define	DEFAULT_CAPS	(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)

#define	NPORTS	4
#define	NGRP	40
#define	NGPIO	253
#define	NINTS	16

#define	PIN_IN	0
#define	PIN_OUT	1

#define	READ4(_sc, _port, _reg)						\
	bus_space_read_4(_sc->bst[_port], _sc->bsh[_port], _reg)
#define	WRITE4(_sc, _port, _reg, _val)					\
	bus_space_write_4(_sc->bst[_port], _sc->bsh[_port], _reg, _val)

/*
 * GPIO interface
 */
static int pad_pin_max(device_t, int *);
static int pad_pin_getcaps(device_t, uint32_t, uint32_t *);
static int pad_pin_getname(device_t, uint32_t, char *);
static int pad_pin_getflags(device_t, uint32_t, uint32_t *);
static int pad_pin_setflags(device_t, uint32_t, uint32_t);
static int pad_pin_set(device_t, uint32_t, unsigned int);
static int pad_pin_get(device_t, uint32_t, unsigned int *);
static int pad_pin_toggle(device_t, uint32_t pin);

struct pad_softc {
	struct resource		*res[NPORTS+4];
	bus_space_tag_t		bst[NPORTS];
	bus_space_handle_t	bsh[NPORTS];
	struct mtx		sc_mtx;
	int			gpio_npins;
	struct gpio_pin		gpio_pins[NGPIO];
	void			*gpio_ih[NPORTS+4];
	device_t		dev;
};

struct pad_softc *gpio_sc;

static struct resource_spec pad_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_MEMORY,	1,	RF_ACTIVE },
	{ SYS_RES_MEMORY,	2,	RF_ACTIVE },
	{ SYS_RES_MEMORY,	3,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },
	{ SYS_RES_IRQ,		2,	RF_ACTIVE },
	{ SYS_RES_IRQ,		3,	RF_ACTIVE },
	{ -1, 0 }
};

struct pad_intr {
	uint32_t	enabled;
	void		(*ih) (void *);
	void		*ih_user;
};

static struct pad_intr intr_map[NGPIO];

struct interrupt_entry {
	int gpio_number;
	char *combiner_source_name;
};

struct interrupt_entry interrupt_table[NINTS] = {
	{ 147, "EINT[15]" },
	{ 146, "EINT[14]" },
	{ 145, "EINT[13]" },
	{ 144, "EINT[12]" },
	{ 143, "EINT[11]" },
	{ 142, "EINT[10]" },
	{ 141, "EINT[9]" },
	{ 140, "EINT[8]" },
	{ 139, "EINT[7]" },
	{ 138, "EINT[6]" },
	{ 137, "EINT[5]" },
	{ 136, "EINT[4]" },
	{ 135, "EINT[3]" },
	{ 134, "EINT[2]" },
	{ 133, "EINT[1]" },
	{ 132, "EINT[0]" },
};

struct gpio_bank {
	char		*name;
	uint32_t	port;
	uint32_t	con;
	uint32_t	ngpio;
	uint32_t	ext_int_grp;
	uint32_t	ext_con;
	uint32_t	ext_flt_con;
	uint32_t	mask;
	uint32_t	pend;
};

/*
 * 253 multi-functional input/output ports
 */

static struct gpio_bank gpio_map[] = {
	/* first 132 gpio */
	{ "gpa0", 0, 0x000, 8,  1, 0x700, 0x800, 0x900, 0xA00 },
	{ "gpa1", 0, 0x020, 6,  2, 0x704, 0x808, 0x904, 0xA04 },
	{ "gpa2", 0, 0x040, 8,  3, 0x708, 0x810, 0x908, 0xA08 },
	{ "gpb0", 0, 0x060, 5,  4, 0x70C, 0x818, 0x90C, 0xA0C },
	{ "gpb1", 0, 0x080, 5,  5, 0x710, 0x820, 0x910, 0xA10 },
	{ "gpb2", 0, 0x0A0, 4,  6, 0x714, 0x828, 0x914, 0xA14 },
	{ "gpb3", 0, 0x0C0, 4,  7, 0x718, 0x830, 0x918, 0xA18 },
	{ "gpc0", 0, 0x0E0, 7,  8, 0x71C, 0x838, 0x91C, 0xA1C },
	{ "gpc1", 0, 0x100, 4,  9, 0x720, 0x840, 0x920, 0xA20 },
	{ "gpc2", 0, 0x120, 7, 10, 0x724, 0x848, 0x924, 0xA24 },
	{ "gpc3", 0, 0x140, 7, 11, 0x728, 0x850, 0x928, 0xA28 },
	{ "gpd0", 0, 0x160, 4, 12, 0x72C, 0x858, 0x92C, 0xA2C },
	{ "gpd1", 0, 0x180, 8, 13, 0x730, 0x860, 0x930, 0xA30 },
	{ "gpy0", 0, 0x1A0, 6,  0,     0,     0,     0,     0 },
	{ "gpy1", 0, 0x1C0, 4,  0,     0,     0,     0,     0 },
	{ "gpy2", 0, 0x1E0, 6,  0,     0,     0,     0,     0 },
	{ "gpy3", 0, 0x200, 8,  0,     0,     0,     0,     0 },
	{ "gpy4", 0, 0x220, 8,  0,     0,     0,     0,     0 },
	{ "gpy5", 0, 0x240, 8,  0,     0,     0,     0,     0 },
	{ "gpy6", 0, 0x260, 8,  0,     0,     0,     0,     0 },
	{ "gpc4", 0, 0x2E0, 7, 30, 0x734, 0x868, 0x934, 0xA34 },

	/* next 32 */
	{ "gpx0", 0, 0xC00, 8, 40, 0xE00, 0xE80, 0xF00, 0xF40 },
	{ "gpx1", 0, 0xC20, 8, 41, 0xE04, 0xE88, 0xF04, 0xF44 },
	{ "gpx2", 0, 0xC40, 8, 42, 0xE08, 0xE90, 0xF08, 0xF48 },
	{ "gpx3", 0, 0xC60, 8, 43, 0xE0C, 0xE98, 0xF0C, 0xF4C },

	{ "gpe0", 1, 0x000, 8, 14, 0x700, 0x800, 0x900, 0xA00 },
	{ "gpe1", 1, 0x020, 2, 15, 0x704, 0x808, 0x904, 0xA04 },
	{ "gpf0", 1, 0x040, 4, 16, 0x708, 0x810, 0x908, 0xA08 },
	{ "gpf1", 1, 0x060, 4, 17, 0x70C, 0x818, 0x90C, 0xA0C },
	{ "gpg0", 1, 0x080, 8, 18, 0x710, 0x820, 0x910, 0xA10 },
	{ "gpg1", 1, 0x0A0, 8, 19, 0x714, 0x828, 0x914, 0xA14 },
	{ "gpg2", 1, 0x0C0, 2, 20, 0x718, 0x830, 0x918, 0xA18 },
	{ "gph0", 1, 0x0E0, 4, 21, 0x71C, 0x838, 0x91C, 0xA1C },
	{ "gph1", 1, 0x100, 8, 22, 0x720, 0x840, 0x920, 0xA20 },

	{ "gpv0", 2, 0x000, 8, 60, 0x700, 0x800, 0x900, 0xA00 },
	{ "gpv1", 2, 0x020, 8, 61, 0x704, 0x808, 0x904, 0xA04 },
	{ "gpv2", 2, 0x060, 8, 62, 0x708, 0x810, 0x908, 0xA08 },
	{ "gpv3", 2, 0x080, 8, 63, 0x70C, 0x818, 0x90C, 0xA0C },
	{ "gpv4", 2, 0x0C0, 2, 64, 0x710, 0x820, 0x910, 0xA10 },

	{ "gpz",  3, 0x000, 7, 50, 0x700, 0x800, 0x900, 0xA00 },
};

static int
get_bank(int gpio_number, struct gpio_bank *bank, int *pin_shift)
{
	int ngpio;
	int i;
	int n;

	n = 0;
	for (i = 0; i < NGRP; i++) {
		ngpio = gpio_map[i].ngpio;

		if ((n + ngpio) >= gpio_number) {
			*bank = gpio_map[i];
			*pin_shift = (gpio_number - n);
			return (0);
		};

		n += ngpio;
	};

	return (-1);
}

static int
port_intr(void *arg)
{
	struct port_softc *sc;

	sc = arg;

	return (FILTER_HANDLED);
}

static void
ext_intr(void *arg)
{
	struct pad_softc *sc;
	void (*ih) (void *);
	void *ih_user;
	int ngpio;
	int found;
	int reg;
	int i,j;
	int n,k;

	sc = arg;

	n = 0;
	for (i = 0; i < NGRP; i++) {
		found = 0;
		ngpio = gpio_map[i].ngpio;

		if (gpio_map[i].pend == 0) {
			n += ngpio;
			continue;
		}

		reg = READ4(sc, gpio_map[i].port, gpio_map[i].pend);

		for (j = 0; j < ngpio; j++) {
			if (reg & (1 << j)) {
				found = 1;

				k = (n + j);
				if (intr_map[k].enabled == 1) {
					ih = intr_map[k].ih;
					ih_user = intr_map[k].ih_user;
					ih(ih_user);
				}
			}
		}

		if (found) {
			/* ACK */
			WRITE4(sc, gpio_map[i].port, gpio_map[i].pend, reg);
		}

		n += ngpio;
	}
}

int
pad_setup_intr(int gpio_number, void (*ih)(void *), void *ih_user)
{
	struct interrupt_entry *entry;
	struct pad_intr *pad_irq;
	struct gpio_bank bank;
	struct pad_softc *sc;
	int pin_shift;
	int reg;
	int i;

	sc = gpio_sc;

	if (sc == NULL) {
		device_printf(sc->dev, "Error: pad is not attached\n");
		return (-1);
	}

	if (get_bank(gpio_number, &bank, &pin_shift) != 0)
		return (-1);

	entry = NULL;
	for (i = 0; i < NINTS; i++)
		if (interrupt_table[i].gpio_number == gpio_number)
			entry = &interrupt_table[i];

	if (entry == NULL) {
		device_printf(sc->dev, "Cant find interrupt source for %d\n",
		    gpio_number);
		return (-1);
	}

#if 0
	printf("Request interrupt name %s\n", entry->combiner_source_name);
#endif

	pad_irq = &intr_map[gpio_number];
	pad_irq->enabled = 1;
	pad_irq->ih = ih;
	pad_irq->ih_user = ih_user;

	/* Setup port as external interrupt source */
	reg = READ4(sc, bank.port, bank.con);
	reg |= (0xf << (pin_shift * 4));
#if 0
	printf("writing 0x%08x to 0x%08x\n", reg, bank.con);
#endif
	WRITE4(sc, bank.port, bank.con, reg);

	/*
	 * Configure interrupt pin
	 *
	 * 0x0 = Sets Low level
	 * 0x1 = Sets High level
	 * 0x2 = Triggers Falling edge
	 * 0x3 = Triggers Rising edge
	 * 0x4 = Triggers Both edge
	 *
	 * TODO: add parameter. For now configure as 0x0
	 */
	reg = READ4(sc, bank.port, bank.ext_con);
	reg &= ~(0x7 << (pin_shift * 4));
	WRITE4(sc, bank.port, bank.ext_con, reg);

	/* Unmask */
	reg = READ4(sc, bank.port, bank.mask);
	reg &= ~(1 << pin_shift);
	WRITE4(sc, bank.port, bank.mask, reg);

	combiner_setup_intr(entry->combiner_source_name, ext_intr, sc);

	return (0);
}

static int
pad_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "exynos,pad"))
		return (ENXIO);

	device_set_desc(dev, "Exynos Pad Control");
	return (BUS_PROBE_DEFAULT);
}

static int
pad_attach(device_t dev)
{
	struct gpio_bank bank;
	struct pad_softc *sc;
	int pin_shift;
	int reg;
	int i;

	sc = device_get_softc(dev);
	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	if (bus_alloc_resources(dev, pad_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* Memory interface */

	for (i = 0; i < NPORTS; i++) {
		sc->bst[i] = rman_get_bustag(sc->res[i]);
		sc->bsh[i] = rman_get_bushandle(sc->res[i]);
	};

	sc->dev = dev;
	sc->gpio_npins = NGPIO;

	gpio_sc = sc;

	for (i = 0; i < NPORTS; i++) {
		if ((bus_setup_intr(dev, sc->res[NPORTS + i],
			    INTR_TYPE_BIO | INTR_MPSAFE, port_intr,
			    NULL, sc, &sc->gpio_ih[i]))) {
			device_printf(dev,
			    "ERROR: Unable to register interrupt handler\n");
			return (ENXIO);
		}
	};

	for (i = 0; i < sc->gpio_npins; i++) {
		sc->gpio_pins[i].gp_pin = i;
		sc->gpio_pins[i].gp_caps = DEFAULT_CAPS;

		if (get_bank(i, &bank, &pin_shift) != 0)
			continue;

		pin_shift *= 4;

		reg = READ4(sc, bank.port, bank.con);
		if (reg & (PIN_OUT << pin_shift))
			sc->gpio_pins[i].gp_flags = GPIO_PIN_OUTPUT;
		else
			sc->gpio_pins[i].gp_flags = GPIO_PIN_INPUT;

		/* TODO: add other pin statuses */

		snprintf(sc->gpio_pins[i].gp_name, GPIOMAXNAME,
		    "pad%d.%d", device_get_unit(dev), i);
	}

	device_add_child(dev, "gpioc", device_get_unit(dev));
	device_add_child(dev, "gpiobus", device_get_unit(dev));

	return (bus_generic_attach(dev));
}

static int
pad_pin_max(device_t dev, int *maxpin)
{

	*maxpin = NGPIO - 1;
	return (0);
}

static int
pad_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct pad_softc *sc;
	int i;

	sc = device_get_softc(dev);
	for (i = 0; i < sc->gpio_npins; i++) {
		if (sc->gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	memcpy(name, sc->gpio_pins[i].gp_name, GPIOMAXNAME);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
pad_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct pad_softc *sc;
	int i;

	sc = device_get_softc(dev);
	for (i = 0; i < sc->gpio_npins; i++) {
		if (sc->gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	*caps = sc->gpio_pins[i].gp_caps;
	GPIO_UNLOCK(sc);

	return (0);
}

static int
pad_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct pad_softc *sc;
	int i;

	sc = device_get_softc(dev);
	for (i = 0; i < sc->gpio_npins; i++) {
		if (sc->gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	*flags = sc->gpio_pins[i].gp_flags;
	GPIO_UNLOCK(sc);

	return (0);
}

static int
pad_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct gpio_bank bank;
	struct pad_softc *sc;
	int pin_shift;
	int i;

	sc = device_get_softc(dev);
	for (i = 0; i < sc->gpio_npins; i++) {
		if (sc->gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->gpio_npins)
		return (EINVAL);

	if (get_bank(pin, &bank, &pin_shift) != 0)
		return (EINVAL);

	GPIO_LOCK(sc);
	if (READ4(sc, bank.port, bank.con + 0x4) & (1 << pin_shift))
		*val = 1;
	else
		*val = 0;
	GPIO_UNLOCK(sc);

	return (0);
}

static int
pad_pin_toggle(device_t dev, uint32_t pin)
{
	struct gpio_bank bank;
	struct pad_softc *sc;
	int pin_shift;
	int reg;
	int i;

	sc = device_get_softc(dev);
	for (i = 0; i < sc->gpio_npins; i++) {
		if (sc->gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->gpio_npins)
		return (EINVAL);

	if (get_bank(pin, &bank, &pin_shift) != 0)
		return (EINVAL);

	GPIO_LOCK(sc);
	reg = READ4(sc, bank.port, bank.con + 0x4);
	if (reg & (1 << pin_shift))
		reg &= ~(1 << pin_shift);
	else
		reg |= (1 << pin_shift);
	WRITE4(sc, bank.port, bank.con + 0x4, reg);
	GPIO_UNLOCK(sc);

	return (0);
}


static void
pad_pin_configure(struct pad_softc *sc, struct gpio_pin *pin,
    unsigned int flags)
{
	struct gpio_bank bank;
	int pin_shift;
	int reg;

	GPIO_LOCK(sc);

	/*
	 * Manage input/output
	 */
	if (flags & (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)) {
		pin->gp_flags &= ~(GPIO_PIN_INPUT|GPIO_PIN_OUTPUT);

		if (get_bank(pin->gp_pin, &bank, &pin_shift) != 0)
			return;

		pin_shift *= 4;

#if 0
		printf("bank is 0x%08x pin_shift %d\n", bank.con, pin_shift);
#endif

		if (flags & GPIO_PIN_OUTPUT) {
			pin->gp_flags |= GPIO_PIN_OUTPUT;
			reg = READ4(sc, bank.port, bank.con);
			reg &= ~(0xf << pin_shift);
			reg |= (PIN_OUT << pin_shift);
			WRITE4(sc, bank.port, bank.con, reg);
		} else {
			pin->gp_flags |= GPIO_PIN_INPUT;
			reg = READ4(sc, bank.port, bank.con);
			reg &= ~(0xf << pin_shift);
			WRITE4(sc, bank.port, bank.con, reg);
		}
	}

	GPIO_UNLOCK(sc);
}


static int
pad_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct pad_softc *sc;
	int i;

	sc = device_get_softc(dev);
	for (i = 0; i < sc->gpio_npins; i++) {
		if (sc->gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->gpio_npins)
		return (EINVAL);

	/* Check for unwanted flags. */
	if ((flags & sc->gpio_pins[i].gp_caps) != flags)
		return (EINVAL);

	/* Can't mix input/output together */
	if ((flags & (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)) ==
	    (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT))
		return (EINVAL);

	pad_pin_configure(sc, &sc->gpio_pins[i], flags);

	return (0);
}

static int
pad_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct pad_softc *sc;
	struct gpio_bank bank;
	int pin_shift;
	int reg;
	int i;

	sc = device_get_softc(dev);
	for (i = 0; i < sc->gpio_npins; i++) {
		if (sc->gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->gpio_npins)
		return (EINVAL);

	if (get_bank(pin, &bank, &pin_shift) != 0)
		return (EINVAL);

	GPIO_LOCK(sc);
	reg = READ4(sc, bank.port, bank.con + 0x4);
	reg &= ~(PIN_OUT << pin_shift);
	if (value)
		reg |= (PIN_OUT << pin_shift);
	WRITE4(sc, bank.port, bank.con + 0x4, reg);
	GPIO_UNLOCK(sc);

	return (0);
}

static device_method_t pad_methods[] = {
	DEVMETHOD(device_probe,		pad_probe),
	DEVMETHOD(device_attach,	pad_attach),

	/* GPIO protocol */
	DEVMETHOD(gpio_pin_max,		pad_pin_max),
	DEVMETHOD(gpio_pin_getname,	pad_pin_getname),
	DEVMETHOD(gpio_pin_getcaps,	pad_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	pad_pin_getflags),
	DEVMETHOD(gpio_pin_get,		pad_pin_get),
	DEVMETHOD(gpio_pin_toggle,	pad_pin_toggle),
	DEVMETHOD(gpio_pin_setflags,	pad_pin_setflags),
	DEVMETHOD(gpio_pin_set,		pad_pin_set),
	{ 0, 0 }
};

static driver_t pad_driver = {
	"gpio",
	pad_methods,
	sizeof(struct pad_softc),
};

static devclass_t pad_devclass;

DRIVER_MODULE(pad, simplebus, pad_driver, pad_devclass, 0, 0);
