/*-
 * Copyright (c) 2012 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Oleksandr Rybalko under sponsorship
 * from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.	Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2.	Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Freescale i.MX515 GPIO driver.
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
#include <machine/resource.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "gpio_if.h"

#define GPIO_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	GPIO_UNLOCK(_sc)	mtx_unlock(&(_sc)->sc_mtx)
#define GPIO_LOCK_INIT(_sc)	mtx_init(&_sc->sc_mtx, 			\
	    device_get_nameunit(_sc->sc_dev), "imx_gpio", MTX_DEF)
#define GPIO_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define GPIO_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define GPIO_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

#define	WRITE4(_sc, _r, _v)						\
	    bus_space_write_4((_sc)->sc_iot, (_sc)->sc_ioh, (_r), (_v))
#define	READ4(_sc, _r)							\
	    bus_space_read_4((_sc)->sc_iot, (_sc)->sc_ioh, (_r))
#define	SET4(_sc, _r, _m)						\
	    WRITE4((_sc), (_r), READ4((_sc), (_r)) | (_m))
#define	CLEAR4(_sc, _r, _m)						\
	    WRITE4((_sc), (_r), READ4((_sc), (_r)) & ~(_m))

/* Registers definition for Freescale i.MX515 GPIO controller */

#define	IMX_GPIO_DR_REG		0x000 /* Pin Data */
#define	IMX_GPIO_OE_REG		0x004 /* Set Pin Output */
#define	IMX_GPIO_PSR_REG	0x008 /* Pad Status */
#define	IMX_GPIO_ICR1_REG	0x00C /* Interrupt Configuration */
#define	IMX_GPIO_ICR2_REG	0x010 /* Interrupt Configuration */
#define		GPIO_ICR_COND_LOW	0
#define		GPIO_ICR_COND_HIGH	1
#define		GPIO_ICR_COND_RISE	2
#define		GPIO_ICR_COND_FALL	3
#define	IMX_GPIO_IMR_REG	0x014 /* Interrupt Mask Register */
#define	IMX_GPIO_ISR_REG	0x018 /* Interrupt Status Register */
#define	IMX_GPIO_EDGE_REG	0x01C /* Edge Detect Register */

#define	DEFAULT_CAPS	(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)
#define	NGPIO		32

struct imx51_gpio_softc {
	device_t		dev;
	struct mtx		sc_mtx;
	struct resource		*sc_res[11]; /* 1 x mem, 2 x IRQ, 8 x IRQ */
	void			*gpio_ih[11]; /* 1 ptr is not a big waste */
	int			sc_l_irq; /* Last irq resource */
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	int			gpio_npins;
	struct gpio_pin		gpio_pins[NGPIO];
};

static struct resource_spec imx_gpio_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },
	{ -1, 0 }
};

static struct resource_spec imx_gpio0irq_spec[] = {
	{ SYS_RES_IRQ,		2,	RF_ACTIVE },
	{ SYS_RES_IRQ,		3,	RF_ACTIVE },
	{ SYS_RES_IRQ,		4,	RF_ACTIVE },
	{ SYS_RES_IRQ,		5,	RF_ACTIVE },
	{ SYS_RES_IRQ,		6,	RF_ACTIVE },
	{ SYS_RES_IRQ,		7,	RF_ACTIVE },
	{ SYS_RES_IRQ,		8,	RF_ACTIVE },
	{ SYS_RES_IRQ,		9,	RF_ACTIVE },
	{ -1, 0 }
};

/*
 * Helpers
 */
static void imx51_gpio_pin_configure(struct imx51_gpio_softc *,
    struct gpio_pin *, uint32_t);

/*
 * Driver stuff
 */
static int imx51_gpio_probe(device_t);
static int imx51_gpio_attach(device_t);
static int imx51_gpio_detach(device_t);
static int imx51_gpio_intr(void *);

/*
 * GPIO interface
 */
static int imx51_gpio_pin_max(device_t, int *);
static int imx51_gpio_pin_getcaps(device_t, uint32_t, uint32_t *);
static int imx51_gpio_pin_getflags(device_t, uint32_t, uint32_t *);
static int imx51_gpio_pin_getname(device_t, uint32_t, char *);
static int imx51_gpio_pin_setflags(device_t, uint32_t, uint32_t);
static int imx51_gpio_pin_set(device_t, uint32_t, unsigned int);
static int imx51_gpio_pin_get(device_t, uint32_t, unsigned int *);
static int imx51_gpio_pin_toggle(device_t, uint32_t pin);

static void
imx51_gpio_pin_configure(struct imx51_gpio_softc *sc, struct gpio_pin *pin,
    unsigned int flags)
{

	GPIO_LOCK(sc);

	/*
	 * Manage input/output
	 */
	if (flags & (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)) {
		pin->gp_flags &= ~(GPIO_PIN_INPUT|GPIO_PIN_OUTPUT);
		if (flags & GPIO_PIN_OUTPUT) {
			pin->gp_flags |= GPIO_PIN_OUTPUT;
			SET4(sc, IMX_GPIO_OE_REG, (1 << pin->gp_pin));
		}
		else {
			pin->gp_flags |= GPIO_PIN_INPUT;
			CLEAR4(sc, IMX_GPIO_OE_REG, (1 << pin->gp_pin));
		}
	}

	GPIO_UNLOCK(sc);
}

static int
imx51_gpio_pin_max(device_t dev, int *maxpin)
{

	*maxpin = NGPIO - 1;
	return (0);
}

static int
imx51_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct imx51_gpio_softc *sc;
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
imx51_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct imx51_gpio_softc *sc;
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
imx51_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct imx51_gpio_softc *sc;
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
imx51_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct imx51_gpio_softc *sc;
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

	imx51_gpio_pin_configure(sc, &sc->gpio_pins[i], flags);


	return (0);
}

static int
imx51_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct imx51_gpio_softc *sc;
	int i;

	sc = device_get_softc(dev);
	for (i = 0; i < sc->gpio_npins; i++) {
		if (sc->gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	if (value)
		SET4(sc, IMX_GPIO_DR_REG, (1 << i));
	else
		CLEAR4(sc, IMX_GPIO_DR_REG, (1 << i));
	GPIO_UNLOCK(sc);

	return (0);
}

static int
imx51_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct imx51_gpio_softc *sc;
	int i;

	sc = device_get_softc(dev);
	for (i = 0; i < sc->gpio_npins; i++) {
		if (sc->gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	*val = (READ4(sc, IMX_GPIO_DR_REG) >> i) & 1;
	GPIO_UNLOCK(sc);

	return (0);
}

static int
imx51_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct imx51_gpio_softc *sc;
	int i;

	sc = device_get_softc(dev);
	for (i = 0; i < sc->gpio_npins; i++) {
		if (sc->gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	WRITE4(sc, IMX_GPIO_DR_REG,
	    (READ4(sc, IMX_GPIO_DR_REG) ^ (1 << i)));
	GPIO_UNLOCK(sc);

	return (0);
}

static int
imx51_gpio_intr(void *arg)
{
	struct imx51_gpio_softc *sc;
	uint32_t input, value;

	sc = arg;
	input = READ4(sc, IMX_GPIO_ISR_REG);
	value = input & READ4(sc, IMX_GPIO_IMR_REG);
	WRITE4(sc, IMX_GPIO_DR_REG, input);

	if (!value)
		goto intr_done;

	/* TODO: interrupt handling */

intr_done:
	return (FILTER_HANDLED);
}

static int
imx51_gpio_probe(device_t dev)
{

	if (ofw_bus_is_compatible(dev, "fsl,imx51-gpio")) {
		device_set_desc(dev, "i.MX515 GPIO Controller");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
imx51_gpio_attach(device_t dev)
{
	struct imx51_gpio_softc *sc;
	int i, irq;

	sc = device_get_softc(dev);
	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	if (bus_alloc_resources(dev, imx_gpio_spec, sc->sc_res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	sc->dev = dev;
	sc->gpio_npins = NGPIO;
	sc->sc_l_irq = 2;
	sc->sc_iot = rman_get_bustag(sc->sc_res[0]);
	sc->sc_ioh = rman_get_bushandle(sc->sc_res[0]);

	if (bus_alloc_resources(dev, imx_gpio0irq_spec, &sc->sc_res[3]) == 0) {
		/*
		 * First GPIO unit able to serve +8 interrupts for 8 first
		 * pins.
		 */
		sc->sc_l_irq = 10;
	}

	for (irq = 1; irq <= sc->sc_l_irq; irq ++) {
		if ((bus_setup_intr(dev, sc->sc_res[irq], INTR_TYPE_MISC,
		    imx51_gpio_intr, NULL, sc, &sc->gpio_ih[irq]))) {
			device_printf(dev,
			    "WARNING: unable to register interrupt handler\n");
			return (ENXIO);
		}
	}

	for (i = 0; i < sc->gpio_npins; i++) {
 		sc->gpio_pins[i].gp_pin = i;
 		sc->gpio_pins[i].gp_caps = DEFAULT_CAPS;
 		sc->gpio_pins[i].gp_flags =
 		    (READ4(sc, IMX_GPIO_OE_REG) & (1 << i)) ? GPIO_PIN_OUTPUT:
 		    GPIO_PIN_INPUT;
 		snprintf(sc->gpio_pins[i].gp_name, GPIOMAXNAME,
 		    "imx_gpio%d.%d", device_get_unit(dev), i);
	}

	device_add_child(dev, "gpioc", device_get_unit(dev));
	device_add_child(dev, "gpiobus", device_get_unit(dev));

	return (bus_generic_attach(dev));
}

static int
imx51_gpio_detach(device_t dev)
{
	struct imx51_gpio_softc *sc;

	sc = device_get_softc(dev);

	KASSERT(mtx_initialized(&sc->sc_mtx), ("gpio mutex not initialized"));

	bus_generic_detach(dev);

	if (sc->sc_res[3])
		bus_release_resources(dev, imx_gpio0irq_spec, &sc->sc_res[3]);

	if (sc->sc_res[0])
		bus_release_resources(dev, imx_gpio_spec, sc->sc_res);

	mtx_destroy(&sc->sc_mtx);

	return(0);
}

static device_method_t imx51_gpio_methods[] = {
	DEVMETHOD(device_probe,		imx51_gpio_probe),
	DEVMETHOD(device_attach,	imx51_gpio_attach),
	DEVMETHOD(device_detach,	imx51_gpio_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_pin_max,		imx51_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname,	imx51_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags,	imx51_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps,	imx51_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags,	imx51_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get,		imx51_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		imx51_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	imx51_gpio_pin_toggle),
	{0, 0},
};

static driver_t imx51_gpio_driver = {
	"gpio",
	imx51_gpio_methods,
	sizeof(struct imx51_gpio_softc),
};
static devclass_t imx51_gpio_devclass;

DRIVER_MODULE(imx51_gpio, simplebus, imx51_gpio_driver, imx51_gpio_devclass,
    0, 0);
