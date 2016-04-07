/*-
 * Copyright (c) 2012, 2013 The FreeBSD Foundation
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

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/gpio.h>
#include <sys/proc.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/resource.h>

#include <dev/fdt/fdt_common.h>
#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "gpio_if.h"

#ifdef ARM_INTRNG
#include "pic_if.h"
#endif

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

#ifdef ARM_INTRNG
struct gpio_irqsrc {
	struct intr_irqsrc	gi_isrc;
	u_int			gi_irq;
	enum intr_polarity	gi_pol;
	enum intr_trigger	gi_trig;
};
#endif

struct imx51_gpio_softc {
	device_t		dev;
	device_t		sc_busdev;
	struct mtx		sc_mtx;
	struct resource		*sc_res[3]; /* 1 x mem, 2 x IRQ */
	void			*gpio_ih[2];
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	int			gpio_npins;
	struct gpio_pin		gpio_pins[NGPIO];
#ifdef ARM_INTRNG
	struct gpio_irqsrc 	gpio_pic_irqsrc[NGPIO];
#endif
};

static struct ofw_compat_data compat_data[] = {
	{"fsl,imx6q-gpio",  1},
	{"fsl,imx53-gpio",  1},
	{"fsl,imx51-gpio",  1},
	{NULL,	            0}
};

static struct resource_spec imx_gpio_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },
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

/*
 * GPIO interface
 */
static device_t imx51_gpio_get_bus(device_t);
static int imx51_gpio_pin_max(device_t, int *);
static int imx51_gpio_pin_getcaps(device_t, uint32_t, uint32_t *);
static int imx51_gpio_pin_getflags(device_t, uint32_t, uint32_t *);
static int imx51_gpio_pin_getname(device_t, uint32_t, char *);
static int imx51_gpio_pin_setflags(device_t, uint32_t, uint32_t);
static int imx51_gpio_pin_set(device_t, uint32_t, unsigned int);
static int imx51_gpio_pin_get(device_t, uint32_t, unsigned int *);
static int imx51_gpio_pin_toggle(device_t, uint32_t pin);

#ifdef ARM_INTRNG
static int
gpio_pic_map_fdt(device_t dev, u_int ncells, pcell_t *cells, u_int *irqp,
    enum intr_polarity *polp, enum intr_trigger *trigp)
{
	struct imx51_gpio_softc *sc;
	u_int irq, tripol;
	enum intr_polarity pol;
	enum intr_trigger trig;

	sc = device_get_softc(dev);

	/*
	 * From devicetree/bindings/gpio/fsl-imx-gpio.txt:
	 *  #interrupt-cells:  2. The first cell is the GPIO number. The second
	 *  cell bits[3:0] is used to specify trigger type and level flags:
	 *    1 = low-to-high edge triggered.
	 *    2 = high-to-low edge triggered.
	 *    4 = active high level-sensitive.
	 *    8 = active low level-sensitive.
         * We can do any single one of these modes, but nothing in combo.
	 */

	if (ncells != 2) {
		device_printf(sc->dev, "Invalid #interrupt-cells");
		return (EINVAL);
	}

	irq = cells[0];
	tripol = cells[1];
	if (irq >= sc->gpio_npins) {
		device_printf(sc->dev, "Invalid interrupt number %d", irq);
		return (EINVAL);
	}
	switch (tripol) {
	case 1:
		trig = INTR_TRIGGER_EDGE;
		pol  = INTR_POLARITY_HIGH;
		break;
	case 2:
		trig = INTR_TRIGGER_EDGE;
		pol  = INTR_POLARITY_LOW;
		break;
	case 4:
		trig = INTR_TRIGGER_LEVEL;
		pol  = INTR_POLARITY_HIGH;
		break;
	case 8:
		trig = INTR_TRIGGER_LEVEL;
		pol  = INTR_POLARITY_LOW;
		break;
	default:
		device_printf(sc->dev, "unsupported trigger/polarity 0x%2x\n",
		    tripol);
		return (ENOTSUP);
	}
	*irqp = irq;
	if (polp != NULL)
		*polp = pol;
	if (trigp != NULL)
		*trigp = trig;
	return (0);
}

static int
gpio_pic_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	int error;
	u_int irq;
	struct imx51_gpio_softc *sc;

	if (data->type != INTR_MAP_DATA_FDT)
		return (ENOTSUP);

	error = gpio_pic_map_fdt(dev, data->fdt.ncells, data->fdt.cells, &irq,
	    NULL, NULL);
	if (error == 0) {
		sc = device_get_softc(dev);
		*isrcp = &sc->gpio_pic_irqsrc[irq].gi_isrc;
	}
	return (error);
}

static int
gpio_pic_teardown_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct imx51_gpio_softc *sc;
	struct gpio_irqsrc *gi;

	sc = device_get_softc(dev);
	if (isrc->isrc_handlers == 0) {
		gi = (struct gpio_irqsrc *)isrc;
		gi->gi_pol = INTR_POLARITY_CONFORM;
		gi->gi_trig = INTR_TRIGGER_CONFORM;

		// XXX Not sure this is necessary
		mtx_lock_spin(&sc->sc_mtx);
		CLEAR4(sc, IMX_GPIO_IMR_REG, (1U << gi->gi_irq));
		WRITE4(sc, IMX_GPIO_ISR_REG, (1U << gi->gi_irq));
		mtx_unlock_spin(&sc->sc_mtx);
	}
	return (0);
}

static int
gpio_pic_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct imx51_gpio_softc *sc;
	struct gpio_irqsrc *gi;
	int error, icfg;
	u_int irq, reg, shift, wrk;
	enum intr_trigger trig;
	enum intr_polarity pol;

	sc = device_get_softc(dev);
	gi = (struct gpio_irqsrc *)isrc;

	/* Get config for interrupt. */
	if (data == NULL || data->type != INTR_MAP_DATA_FDT)
		return (ENOTSUP);
	error = gpio_pic_map_fdt(dev, data->fdt.ncells, data->fdt.cells, &irq,
	    &pol, &trig);
	if (error != 0)
		return (error);
	if (gi->gi_irq != irq)
		return (EINVAL);

	/* Compare config if this is not first setup. */
	if (isrc->isrc_handlers != 0) {
		if (pol != gi->gi_pol || trig != gi->gi_trig)
			return (EINVAL);
		else
			return (0);
	}

	gi->gi_pol = pol;
	gi->gi_trig = trig;

	if (trig == INTR_TRIGGER_LEVEL) {
		if (pol == INTR_POLARITY_LOW)
			icfg = GPIO_ICR_COND_LOW;
		else
			icfg = GPIO_ICR_COND_HIGH;
	} else {
		if (pol == INTR_POLARITY_HIGH)
			icfg = GPIO_ICR_COND_FALL;
		else
			icfg = GPIO_ICR_COND_RISE;
	}

	if (irq < 16) {
		reg = IMX_GPIO_ICR1_REG;
		shift = 2 * irq;
	} else {
		reg = IMX_GPIO_ICR2_REG;
		shift = 2 * (irq - 16);
	}

	mtx_lock_spin(&sc->sc_mtx);
	CLEAR4(sc, IMX_GPIO_IMR_REG, (1U << irq));
	WRITE4(sc, IMX_GPIO_ISR_REG, (1U << irq));
	wrk = READ4(sc, reg);
	wrk &= ~(0x03 << shift);
	wrk |= icfg << shift;
	WRITE4(sc, reg, wrk);
	mtx_unlock_spin(&sc->sc_mtx);
	return (0);
}

/*
 * this is mask_intr
 */
static void
gpio_pic_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct imx51_gpio_softc *sc;
	u_int irq;

	sc = device_get_softc(dev);
	irq = ((struct gpio_irqsrc *)isrc)->gi_irq;

	mtx_lock_spin(&sc->sc_mtx);
	CLEAR4(sc, IMX_GPIO_IMR_REG, (1U << irq));
	mtx_unlock_spin(&sc->sc_mtx);
}

/*
 * this is unmask_intr
 */
static void
gpio_pic_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct imx51_gpio_softc *sc;
	u_int irq;

	sc = device_get_softc(dev);
	irq = ((struct gpio_irqsrc *)isrc)->gi_irq;

	mtx_lock_spin(&sc->sc_mtx);
	SET4(sc, IMX_GPIO_IMR_REG, (1U << irq));
	mtx_unlock_spin(&sc->sc_mtx);
}

static void
gpio_pic_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
	struct imx51_gpio_softc *sc;
	u_int irq;

	sc = device_get_softc(dev);
	irq = ((struct gpio_irqsrc *)isrc)->gi_irq;

	arm_irq_memory_barrier(0);
        /* EOI.  W1C reg so no r-m-w, no locking needed. */
	WRITE4(sc, IMX_GPIO_ISR_REG, (1U << irq));
}

static void
gpio_pic_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{

	arm_irq_memory_barrier(0);
	gpio_pic_enable_intr(dev, isrc);
}

static void
gpio_pic_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{

	gpio_pic_disable_intr(dev, isrc);
}

static int
gpio_pic_filter(void *arg)
{
	struct imx51_gpio_softc *sc;
	struct intr_irqsrc *isrc;
	uint32_t i, interrupts;

	sc = arg;
	mtx_lock_spin(&sc->sc_mtx);
	interrupts = READ4(sc, IMX_GPIO_ISR_REG) & READ4(sc, IMX_GPIO_IMR_REG);
	mtx_unlock_spin(&sc->sc_mtx);

	for (i = 0; interrupts != 0; i++, interrupts >>= 1) {
		if ((interrupts & 0x1) == 0)
			continue;
		isrc = &sc->gpio_pic_irqsrc[i].gi_isrc;
		if (intr_isrc_dispatch(isrc, curthread->td_intr_frame) != 0) {
			gpio_pic_disable_intr(sc->dev, isrc);
			gpio_pic_post_filter(sc->dev, isrc);
			device_printf(sc->dev, "Stray irq %u disabled\n", i);
		}
	}

	return (FILTER_HANDLED);
}

/*
 * Initialize our isrcs and register them with intrng.
 */
static int
gpio_pic_register_isrcs(struct imx51_gpio_softc *sc)
{
	int error;
	uint32_t irq;
	const char *name;

	name = device_get_nameunit(sc->dev);
	for (irq = 0; irq < NGPIO; irq++) {
		sc->gpio_pic_irqsrc[irq].gi_irq = irq;
		sc->gpio_pic_irqsrc[irq].gi_pol = INTR_POLARITY_CONFORM;
		sc->gpio_pic_irqsrc[irq].gi_trig = INTR_TRIGGER_CONFORM;

		error = intr_isrc_register(&sc->gpio_pic_irqsrc[irq].gi_isrc,
		    sc->dev, 0, "%s,%u", name, irq);
		if (error != 0) {
			/* XXX call intr_isrc_deregister() */
			device_printf(sc->dev, "%s failed", __func__);
			return (error);
		}
	}
	return (0);
}
#endif

/*
 *
 */
static void
imx51_gpio_pin_configure(struct imx51_gpio_softc *sc, struct gpio_pin *pin,
    unsigned int flags)
{
	u_int newflags;

	mtx_lock_spin(&sc->sc_mtx);

	/*
	 * Manage input/output; other flags not supported yet.
	 *
	 * Note that changes to pin->gp_flags must be acccumulated in newflags
	 * and stored with a single writeback to gp_flags at the end, to enable
	 * unlocked reads of that value elsewhere.
	 */
	if (flags & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) {
		newflags = pin->gp_flags & ~(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT);
		if (flags & GPIO_PIN_OUTPUT) {
			newflags |= GPIO_PIN_OUTPUT;
			SET4(sc, IMX_GPIO_OE_REG, (1U << pin->gp_pin));
		} else {
			newflags |= GPIO_PIN_INPUT;
			CLEAR4(sc, IMX_GPIO_OE_REG, (1U << pin->gp_pin));
		}
		pin->gp_flags = newflags;
	}

	mtx_unlock_spin(&sc->sc_mtx);
}

static device_t
imx51_gpio_get_bus(device_t dev)
{
	struct imx51_gpio_softc *sc;

	sc = device_get_softc(dev);

	return (sc->sc_busdev);
}

static int
imx51_gpio_pin_max(device_t dev, int *maxpin)
{
	struct imx51_gpio_softc *sc;

	sc = device_get_softc(dev);
	*maxpin = sc->gpio_npins - 1;

	return (0);
}

static int
imx51_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct imx51_gpio_softc *sc;

	sc = device_get_softc(dev);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	*caps = sc->gpio_pins[pin].gp_caps;

	return (0);
}

static int
imx51_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct imx51_gpio_softc *sc;

	sc = device_get_softc(dev);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	*flags = sc->gpio_pins[pin].gp_flags;

	return (0);
}

static int
imx51_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct imx51_gpio_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);

	mtx_lock_spin(&sc->sc_mtx);
	memcpy(name, sc->gpio_pins[pin].gp_name, GPIOMAXNAME);
	mtx_unlock_spin(&sc->sc_mtx);

	return (0);
}

static int
imx51_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct imx51_gpio_softc *sc;

	sc = device_get_softc(dev);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	imx51_gpio_pin_configure(sc, &sc->gpio_pins[pin], flags);

	return (0);
}

static int
imx51_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct imx51_gpio_softc *sc;

	sc = device_get_softc(dev);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	mtx_lock_spin(&sc->sc_mtx);
	if (value)
		SET4(sc, IMX_GPIO_DR_REG, (1U << pin));
	else
		CLEAR4(sc, IMX_GPIO_DR_REG, (1U << pin));
	mtx_unlock_spin(&sc->sc_mtx);

	return (0);
}

static int
imx51_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct imx51_gpio_softc *sc;

	sc = device_get_softc(dev);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	*val = (READ4(sc, IMX_GPIO_DR_REG) >> pin) & 1;

	return (0);
}

static int
imx51_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct imx51_gpio_softc *sc;

	sc = device_get_softc(dev);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	mtx_lock_spin(&sc->sc_mtx);
	WRITE4(sc, IMX_GPIO_DR_REG,
	    (READ4(sc, IMX_GPIO_DR_REG) ^ (1U << pin)));
	mtx_unlock_spin(&sc->sc_mtx);

	return (0);
}

static int
imx51_gpio_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data != 0) {
		device_set_desc(dev, "Freescale i.MX GPIO Controller");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
imx51_gpio_attach(device_t dev)
{
	struct imx51_gpio_softc *sc;
	int i, irq, unit;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->gpio_npins = NGPIO;

	mtx_init(&sc->sc_mtx, device_get_nameunit(sc->dev), NULL, MTX_SPIN);

	if (bus_alloc_resources(dev, imx_gpio_spec, sc->sc_res)) {
		device_printf(dev, "could not allocate resources\n");
		bus_release_resources(dev, imx_gpio_spec, sc->sc_res);
		mtx_destroy(&sc->sc_mtx);
		return (ENXIO);
	}

	sc->sc_iot = rman_get_bustag(sc->sc_res[0]);
	sc->sc_ioh = rman_get_bushandle(sc->sc_res[0]);
	/*
	 * Mask off all interrupts in hardware, then set up interrupt handling.
	 */
	WRITE4(sc, IMX_GPIO_IMR_REG, 0);
	for (irq = 0; irq < 2; irq++) {
#ifdef ARM_INTRNG
		if ((bus_setup_intr(dev, sc->sc_res[1 + irq], INTR_TYPE_CLK,
		    gpio_pic_filter, NULL, sc, &sc->gpio_ih[irq]))) {
			device_printf(dev,
			    "WARNING: unable to register interrupt handler\n");
			imx51_gpio_detach(dev);
			return (ENXIO);
		}
#endif		
	}

	unit = device_get_unit(dev);
	for (i = 0; i < sc->gpio_npins; i++) {
 		sc->gpio_pins[i].gp_pin = i;
 		sc->gpio_pins[i].gp_caps = DEFAULT_CAPS;
 		sc->gpio_pins[i].gp_flags =
 		    (READ4(sc, IMX_GPIO_OE_REG) & (1U << i)) ? GPIO_PIN_OUTPUT :
 		    GPIO_PIN_INPUT;
 		snprintf(sc->gpio_pins[i].gp_name, GPIOMAXNAME,
 		    "imx_gpio%d.%d", unit, i);
	}

#ifdef ARM_INTRNG
	gpio_pic_register_isrcs(sc);
	intr_pic_register(dev, OF_xref_from_node(ofw_bus_get_node(dev)));
#endif
	sc->sc_busdev = gpiobus_attach_bus(dev);
	
	if (sc->sc_busdev == NULL) {
		imx51_gpio_detach(dev);
		return (ENXIO);
	}

	return (0);
}

static int
imx51_gpio_detach(device_t dev)
{
	int irq;
	struct imx51_gpio_softc *sc;

	sc = device_get_softc(dev);

	gpiobus_detach_bus(dev);
	for (irq = 1; irq <= 2; irq++) {
		if (sc->gpio_ih[irq])
			bus_teardown_intr(dev, sc->sc_res[irq], sc->gpio_ih[irq]);
	}
	bus_release_resources(dev, imx_gpio_spec, sc->sc_res);
	mtx_destroy(&sc->sc_mtx);

	return(0);
}

static device_method_t imx51_gpio_methods[] = {
	DEVMETHOD(device_probe,		imx51_gpio_probe),
	DEVMETHOD(device_attach,	imx51_gpio_attach),
	DEVMETHOD(device_detach,	imx51_gpio_detach),

#ifdef ARM_INTRNG
	/* Interrupt controller interface */
	DEVMETHOD(pic_disable_intr,	gpio_pic_disable_intr),
	DEVMETHOD(pic_enable_intr,	gpio_pic_enable_intr),
	DEVMETHOD(pic_map_intr,		gpio_pic_map_intr),
	DEVMETHOD(pic_setup_intr,	gpio_pic_setup_intr),
	DEVMETHOD(pic_teardown_intr,	gpio_pic_teardown_intr),
	DEVMETHOD(pic_post_filter,	gpio_pic_post_filter),
	DEVMETHOD(pic_post_ithread,	gpio_pic_post_ithread),
	DEVMETHOD(pic_pre_ithread,	gpio_pic_pre_ithread),
#endif

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus,		imx51_gpio_get_bus),
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
