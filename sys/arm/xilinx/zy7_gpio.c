/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 Thomas Skibo
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
 */

/*
 * A GPIO driver for Xilinx Zynq-7000.
 *
 * The GPIO peripheral on Zynq allows controlling 114 general purpose I/Os.
 *
 * Pins 53-0 are sent to the MIO.  Any MIO pins not used by a PS peripheral are
 * available as a GPIO pin.  Pins 64-127 are sent to the PL (FPGA) section of
 * Zynq as EMIO signals.
 *
 * The hardware provides a way to use IOs as interrupt sources but the
 * gpio framework doesn't seem to have hooks for this.
 *
 * Reference: Zynq-7000 All Programmable SoC Technical Reference Manual.
 * (v1.4) November 16, 2012.  Xilinx doc UG585.  GPIO is covered in
 * chater 14.  Register definitions are in appendix B.19.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "gpio_if.h"

#define	ZYNQ7_MAX_BANK		4
#define	ZYNQMP_MAX_BANK		6

/* Zynq 7000 */
#define	ZYNQ7_BANK0_PIN_MIN	0
#define	ZYNQ7_BANK0_NPIN	32
#define	ZYNQ7_BANK1_PIN_MIN	32
#define	ZYNQ7_BANK1_NPIN	22
#define	ZYNQ7_BANK2_PIN_MIN	64
#define	ZYNQ7_BANK2_NPIN	32
#define	ZYNQ7_BANK3_PIN_MIN	96
#define	ZYNQ7_BANK3_NPIN	32
#define	ZYNQ7_PIN_MIO_MIN	0
#define	ZYNQ7_PIN_MIO_MAX	54
#define	ZYNQ7_PIN_EMIO_MIN	64
#define	ZYNQ7_PIN_EMIO_MAX	118

/* ZynqMP */
#define	ZYNQMP_BANK0_PIN_MIN	0
#define	ZYNQMP_BANK0_NPIN	26
#define	ZYNQMP_BANK1_PIN_MIN	26
#define	ZYNQMP_BANK1_NPIN	26
#define	ZYNQMP_BANK2_PIN_MIN	52
#define	ZYNQMP_BANK2_NPIN	26
#define	ZYNQMP_BANK3_PIN_MIN	78
#define	ZYNQMP_BANK3_NPIN	32
#define	ZYNQMP_BANK4_PIN_MIN	110
#define	ZYNQMP_BANK4_NPIN	32
#define	ZYNQMP_BANK5_PIN_MIN	142
#define	ZYNQMP_BANK5_NPIN	32
#define	ZYNQMP_PIN_MIO_MIN	0
#define	ZYNQMP_PIN_MIO_MAX	77
#define	ZYNQMP_PIN_EMIO_MIN	78
#define	ZYNQMP_PIN_EMIO_MAX	174

#define	ZYNQ_BANK_NPIN(type, bank)	(ZYNQ##type##_BANK##bank##_NPIN)
#define	ZYNQ_BANK_PIN_MIN(type, bank)	(ZYNQ##type##_BANK##bank##_PIN_MIN)
#define	ZYNQ_BANK_PIN_MAX(type, bank)	(ZYNQ##type##_BANK##bank##_PIN_MIN + ZYNQ##type##_BANK##bank##_NPIN - 1)

#define	ZYNQ_PIN_IS_MIO(type, pin)	(pin >= ZYNQ##type##_PIN_MIO_MIN &&	\
	  pin <= ZYNQ##type##_PIN_MIO_MAX)
#define	ZYNQ_PIN_IS_EMIO(type, pin)	(pin >= ZYNQ##type##_PIN_EMIO_MIN &&	\
	  pin <= ZYNQ##type##_PIN_EMIO_MAX)

#define ZGPIO_LOCK(sc)			mtx_lock(&(sc)->sc_mtx)
#define	ZGPIO_UNLOCK(sc)		mtx_unlock(&(sc)->sc_mtx)
#define ZGPIO_LOCK_INIT(sc) \
	mtx_init(&(sc)->sc_mtx, device_get_nameunit((sc)->dev),	\
	    "gpio", MTX_DEF)
#define ZGPIO_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);

enum zynq_gpio_type {
	ZYNQ_7000 = 0,
	ZYNQMP,
};

struct zynq_gpio_conf {
	char			*name;
	enum zynq_gpio_type	type;
	uint32_t		nbanks;
	uint32_t		maxpin;
	uint32_t		bank_min[ZYNQMP_MAX_BANK];
	uint32_t		bank_max[ZYNQMP_MAX_BANK];
};

struct zy7_gpio_softc {
	device_t		dev;
	device_t		busdev;
	struct mtx		sc_mtx;
	struct resource		*mem_res;	/* Memory resource */
	struct zynq_gpio_conf	*conf;
};

static struct zynq_gpio_conf z7_gpio_conf = {
	.name = "Zynq-7000 GPIO Controller",
	.type = ZYNQ_7000,
	.nbanks = ZYNQ7_MAX_BANK,
	.maxpin = ZYNQ7_PIN_EMIO_MAX,
	.bank_min[0] = ZYNQ_BANK_PIN_MIN(7, 0),
	.bank_max[0] = ZYNQ_BANK_PIN_MAX(7, 0),
	.bank_min[1] = ZYNQ_BANK_PIN_MIN(7, 1),
	.bank_max[1] = ZYNQ_BANK_PIN_MAX(7, 1),
	.bank_min[2] = ZYNQ_BANK_PIN_MIN(7, 2),
	.bank_max[2] = ZYNQ_BANK_PIN_MAX(7, 2),
	.bank_min[3] = ZYNQ_BANK_PIN_MIN(7, 3),
	.bank_max[3] = ZYNQ_BANK_PIN_MAX(7, 3),
};

static struct zynq_gpio_conf zynqmp_gpio_conf = {
	.name = "ZynqMP GPIO Controller",
	.type = ZYNQMP,
	.nbanks = ZYNQMP_MAX_BANK,
	.maxpin = ZYNQMP_PIN_EMIO_MAX,
	.bank_min[0] = ZYNQ_BANK_PIN_MIN(MP, 0),
	.bank_max[0] = ZYNQ_BANK_PIN_MAX(MP, 0),
	.bank_min[1] = ZYNQ_BANK_PIN_MIN(MP, 1),
	.bank_max[1] = ZYNQ_BANK_PIN_MAX(MP, 1),
	.bank_min[2] = ZYNQ_BANK_PIN_MIN(MP, 2),
	.bank_max[2] = ZYNQ_BANK_PIN_MAX(MP, 2),
	.bank_min[3] = ZYNQ_BANK_PIN_MIN(MP, 3),
	.bank_max[3] = ZYNQ_BANK_PIN_MAX(MP, 3),
	.bank_min[4] = ZYNQ_BANK_PIN_MIN(MP, 4),
	.bank_max[4] = ZYNQ_BANK_PIN_MAX(MP, 4),
	.bank_min[5] = ZYNQ_BANK_PIN_MIN(MP, 5),
	.bank_max[5] = ZYNQ_BANK_PIN_MAX(MP, 5),
};

static struct ofw_compat_data compat_data[] = {
	{"xlnx,zy7_gpio",		(uintptr_t)&z7_gpio_conf},
	{"xlnx,zynqmp-gpio-1.0",	(uintptr_t)&zynqmp_gpio_conf},
	{NULL, 0},
};

#define WR4(sc, off, val)	bus_write_4((sc)->mem_res, (off), (val))
#define RD4(sc, off)		bus_read_4((sc)->mem_res, (off))

/* Xilinx Zynq-7000 GPIO register definitions:
 */
#define ZY7_GPIO_MASK_DATA_LSW(b)	(0x0000+8*(b))	/* maskable wr lo */
#define ZY7_GPIO_MASK_DATA_MSW(b)	(0x0004+8*(b))	/* maskable wr hi */
#define ZY7_GPIO_DATA(b)		(0x0040+4*(b))	/* in/out data */
#define ZY7_GPIO_DATA_RO(b)		(0x0060+4*(b))	/* input data */

#define ZY7_GPIO_DIRM(b)		(0x0204+0x40*(b)) /* direction mode */
#define ZY7_GPIO_OEN(b)			(0x0208+0x40*(b)) /* output enable */
#define ZY7_GPIO_INT_MASK(b)		(0x020c+0x40*(b)) /* int mask */
#define ZY7_GPIO_INT_EN(b)		(0x0210+0x40*(b)) /* int enable */
#define ZY7_GPIO_INT_DIS(b)		(0x0214+0x40*(b)) /* int disable */
#define ZY7_GPIO_INT_STAT(b)		(0x0218+0x40*(b)) /* int status */
#define ZY7_GPIO_INT_TYPE(b)		(0x021c+0x40*(b)) /* int type */
#define ZY7_GPIO_INT_POLARITY(b)	(0x0220+0x40*(b)) /* int polarity */
#define ZY7_GPIO_INT_ANY(b)		(0x0224+0x40*(b)) /* any edge */

static device_t
zy7_gpio_get_bus(device_t dev)
{
	struct zy7_gpio_softc *sc;

	sc = device_get_softc(dev);

	return (sc->busdev);
}

static int
zy7_gpio_pin_max(device_t dev, int *maxpin)
{
	struct zy7_gpio_softc *sc;

	sc = device_get_softc(dev);
	*maxpin = sc->conf->maxpin;
	return (0);
}

static inline bool
zy7_pin_valid(device_t dev, uint32_t pin)
{
	struct zy7_gpio_softc *sc;
	int i;
	bool found = false;

	sc = device_get_softc(dev);
	for (i = 0; i < sc->conf->nbanks; i++) {
		if (pin >= sc->conf->bank_min[i] && pin <= sc->conf->bank_max[i]) {
			found = true;
			break;
		}
	}

	return (found);
}

/* Get a specific pin's capabilities. */
static int
zy7_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{

	if (!zy7_pin_valid(dev, pin))
		return (EINVAL);

	*caps = (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT | GPIO_PIN_TRISTATE);

	return (0);
}

/* Get a specific pin's name. */
static int
zy7_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct zy7_gpio_softc *sc;
	uint32_t emio_min;
	bool is_mio;

	sc = device_get_softc(dev);
	if (!zy7_pin_valid(dev, pin))
		return (EINVAL);

	switch (sc->conf->type) {
	case ZYNQ_7000:
		is_mio = ZYNQ_PIN_IS_MIO(7, pin);
		emio_min = ZYNQ7_PIN_EMIO_MIN;
		break;
	case ZYNQMP:
		is_mio = ZYNQ_PIN_IS_MIO(MP, pin);
		emio_min = ZYNQMP_PIN_EMIO_MIN;
		break;
	default:
		return (EINVAL);
	}
	if (is_mio) {
		snprintf(name, GPIOMAXNAME, "MIO_%d", pin);
		name[GPIOMAXNAME - 1] = '\0';
	} else {
		snprintf(name, GPIOMAXNAME, "EMIO_%d", pin - emio_min);
		name[GPIOMAXNAME - 1] = '\0';
	}

	return (0);
}

/* Get a specific pin's current in/out/tri state. */
static int
zy7_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct zy7_gpio_softc *sc = device_get_softc(dev);

	if (!zy7_pin_valid(dev, pin))
		return (EINVAL);

	ZGPIO_LOCK(sc);

	if ((RD4(sc, ZY7_GPIO_DIRM(pin >> 5)) & (1 << (pin & 31))) != 0) {
		/* output */
		if ((RD4(sc, ZY7_GPIO_OEN(pin >> 5)) & (1 << (pin & 31))) == 0)
			*flags = (GPIO_PIN_OUTPUT | GPIO_PIN_TRISTATE);
		else
			*flags = GPIO_PIN_OUTPUT;
	} else
		/* input */
		*flags = GPIO_PIN_INPUT;

	ZGPIO_UNLOCK(sc);

	return (0);
}

/* Set a specific pin's in/out/tri state. */
static int
zy7_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct zy7_gpio_softc *sc = device_get_softc(dev);

	if (!zy7_pin_valid(dev, pin))
		return (EINVAL);

	ZGPIO_LOCK(sc);

	if ((flags & GPIO_PIN_OUTPUT) != 0) {
		/* Output.  Set or reset OEN too. */
		WR4(sc, ZY7_GPIO_DIRM(pin >> 5),
		    RD4(sc, ZY7_GPIO_DIRM(pin >> 5)) | (1 << (pin & 31)));

		if ((flags & GPIO_PIN_TRISTATE) != 0)
			WR4(sc, ZY7_GPIO_OEN(pin >> 5),
			    RD4(sc, ZY7_GPIO_OEN(pin >> 5)) &
			    ~(1 << (pin & 31)));
		else
			WR4(sc, ZY7_GPIO_OEN(pin >> 5),
			    RD4(sc, ZY7_GPIO_OEN(pin >> 5)) |
			    (1 << (pin & 31)));
	} else {
		/* Input.  Turn off OEN. */
		WR4(sc, ZY7_GPIO_DIRM(pin >> 5),
		    RD4(sc, ZY7_GPIO_DIRM(pin >> 5)) & ~(1 << (pin & 31)));
		WR4(sc, ZY7_GPIO_OEN(pin >> 5),
		    RD4(sc, ZY7_GPIO_OEN(pin >> 5)) & ~(1 << (pin & 31)));
	}
		
	ZGPIO_UNLOCK(sc);

	return (0);
}

/* Set a specific output pin's value. */
static int
zy7_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct zy7_gpio_softc *sc = device_get_softc(dev);

	if (!zy7_pin_valid(dev, pin) || value > 1)
		return (EINVAL);

	/* Fancy register tricks allow atomic set or reset. */
	if ((pin & 16) != 0)
		WR4(sc, ZY7_GPIO_MASK_DATA_MSW(pin >> 5),
		    (0xffff0000 ^ (0x10000 << (pin & 15))) |
		    (value << (pin & 15)));
	else
		WR4(sc, ZY7_GPIO_MASK_DATA_LSW(pin >> 5),
		    (0xffff0000 ^ (0x10000 << (pin & 15))) |
		    (value << (pin & 15)));

	return (0);
}

/* Get a specific pin's input value. */
static int
zy7_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *value)
{
	struct zy7_gpio_softc *sc = device_get_softc(dev);

	if (!zy7_pin_valid(dev, pin))
		return (EINVAL);

	*value = (RD4(sc, ZY7_GPIO_DATA_RO(pin >> 5)) >> (pin & 31)) & 1;

	return (0);
}

/* Toggle a pin's output value. */
static int
zy7_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct zy7_gpio_softc *sc = device_get_softc(dev);

	if (!zy7_pin_valid(dev, pin))
		return (EINVAL);

	ZGPIO_LOCK(sc);

	WR4(sc, ZY7_GPIO_DATA(pin >> 5),
	    RD4(sc, ZY7_GPIO_DATA(pin >> 5)) ^ (1 << (pin & 31)));

	ZGPIO_UNLOCK(sc);

	return (0);
}

static int
zy7_gpio_probe(device_t dev)
{
	struct zynq_gpio_conf *conf;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	conf = (struct zynq_gpio_conf *)ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	if (conf == 0)
		return (ENXIO);

	device_set_desc(dev, conf->name);
	return (0);
}

static int zy7_gpio_detach(device_t dev);

static int
zy7_gpio_attach(device_t dev)
{
	struct zy7_gpio_softc *sc = device_get_softc(dev);
	int rid;

	sc->dev = dev;
	sc->conf = (struct zynq_gpio_conf *)ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	ZGPIO_LOCK_INIT(sc);

	/* Allocate memory. */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev,
		     SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Can't allocate memory for device");
		zy7_gpio_detach(dev);
		return (ENOMEM);
	}

	sc->busdev = gpiobus_attach_bus(dev);
	if (sc->busdev == NULL) {
		zy7_gpio_detach(dev);
		return (ENOMEM);
	}

	return (0);
}

static int
zy7_gpio_detach(device_t dev)
{
	struct zy7_gpio_softc *sc = device_get_softc(dev);

	gpiobus_detach_bus(dev);

	if (sc->mem_res != NULL) {
		/* Release memory resource. */
		bus_release_resource(dev, SYS_RES_MEMORY,
				     rman_get_rid(sc->mem_res), sc->mem_res);
	}

	ZGPIO_LOCK_DESTROY(sc);

	return (0);
}

static device_method_t zy7_gpio_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, 	zy7_gpio_probe),
	DEVMETHOD(device_attach, 	zy7_gpio_attach),
	DEVMETHOD(device_detach, 	zy7_gpio_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus, 	zy7_gpio_get_bus),
	DEVMETHOD(gpio_pin_max, 	zy7_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname, 	zy7_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags, 	zy7_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps, 	zy7_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags, 	zy7_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get, 	zy7_gpio_pin_get),
	DEVMETHOD(gpio_pin_set, 	zy7_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle, 	zy7_gpio_pin_toggle),

	DEVMETHOD_END
};

static driver_t zy7_gpio_driver = {
	"gpio",
	zy7_gpio_methods,
	sizeof(struct zy7_gpio_softc),
};

EARLY_DRIVER_MODULE(zy7_gpio, simplebus, zy7_gpio_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
