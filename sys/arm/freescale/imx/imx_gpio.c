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
	struct arm_irqsrc 	*gpio_pic_irqsrc[NGPIO];
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
/*
 * this is teardown_intr
 */
static void
gpio_pic_disable_intr(device_t dev, struct arm_irqsrc *isrc)
{
	struct imx51_gpio_softc *sc;
	u_int irq;

	sc = device_get_softc(dev);
	irq = isrc->isrc_data;

	// XXX Not sure this is necessary
	mtx_lock_spin(&sc->sc_mtx);
	CLEAR4(sc, IMX_GPIO_IMR_REG, (1U << irq));
	WRITE4(sc, IMX_GPIO_ISR_REG, (1U << irq));
	mtx_unlock_spin(&sc->sc_mtx);
}

/*
 * this is mask_intr
 */
static void
gpio_pic_disable_source(device_t dev, struct arm_irqsrc *isrc)
{
	struct imx51_gpio_softc *sc;

	sc = device_get_softc(dev);

	mtx_lock_spin(&sc->sc_mtx);
	CLEAR4(sc, IMX_GPIO_IMR_REG, (1U << isrc->isrc_data));
	mtx_unlock_spin(&sc->sc_mtx);
}

/*
 * this is setup_intr
 */
static void
gpio_pic_enable_intr(device_t dev, struct arm_irqsrc *isrc)
{
	struct imx51_gpio_softc *sc;
	int icfg;
	u_int irq, reg, shift, wrk;

	sc = device_get_softc(dev);

	if (isrc->isrc_trig == INTR_TRIGGER_LEVEL) {
		if (isrc->isrc_pol == INTR_POLARITY_LOW)
			icfg = GPIO_ICR_COND_LOW;
		else
			icfg = GPIO_ICR_COND_HIGH;
	} else {
		if (isrc->isrc_pol == INTR_POLARITY_HIGH)
			icfg = GPIO_ICR_COND_FALL;
		else
			icfg = GPIO_ICR_COND_RISE;
	}

	irq = isrc->isrc_data;
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
}

/*
 * this is unmask_intr
 */
static void
gpio_pic_enable_source(device_t dev, struct arm_irqsrc *isrc)
{
	struct imx51_gpio_softc *sc;

	sc = device_get_softc(dev);

	mtx_lock_spin(&sc->sc_mtx);
	SET4(sc, IMX_GPIO_IMR_REG, (1U << isrc->isrc_data));
	mtx_unlock_spin(&sc->sc_mtx);
}

static void
gpio_pic_post_filter(device_t dev, struct arm_irqsrc *isrc)
{
	struct imx51_gpio_softc *sc;

	sc = device_get_softc(dev);

	arm_irq_memory_barrier(0);
        /* EOI.  W1C reg so no r-m-w, no locking needed. */
	WRITE4(sc, IMX_GPIO_ISR_REG, (1U << isrc->isrc_data));
}

static void
gpio_pic_post_ithread(device_t dev, struct arm_irqsrc *isrc)
{

	arm_irq_memory_barrier(0);
	gpio_pic_enable_source(dev, isrc);
}

static void
gpio_pic_pre_ithread(device_t dev, struct arm_irqsrc *isrc)
{

	gpio_pic_disable_source(dev, isrc);
}

/*
 * intrng calls this to make a new isrc known to us.
 */
static int
gpio_pic_register(device_t dev, struct arm_irqsrc *isrc, boolean_t *is_percpu)
{
	struct imx51_gpio_softc *sc;
	u_int irq, tripol;

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

	if (isrc->isrc_ncells != 2) {
		device_printf(sc->dev, "Invalid #interrupt-cells");
		return (EINVAL);
	}

	irq = isrc->isrc_cells[0];
	tripol = isrc->isrc_cells[1];
	if (irq >= sc->gpio_npins) {
		device_printf(sc->dev, "Invalid interrupt number %d", irq);
		return (EINVAL);
	}
	switch (tripol)
	{
	case 1:
		isrc->isrc_trig = INTR_TRIGGER_EDGE; 
		isrc->isrc_pol  = INTR_POLARITY_HIGH;
		break;
	case 2:
		isrc->isrc_trig = INTR_TRIGGER_EDGE; 
		isrc->isrc_pol  = INTR_POLARITY_LOW;
		break;
	case 4:
		isrc->isrc_trig = INTR_TRIGGER_LEVEL; 
		isrc->isrc_pol  = INTR_POLARITY_HIGH;
		break;
	case 8:
		isrc->isrc_trig = INTR_TRIGGER_LEVEL; 
		isrc->isrc_pol  = INTR_POLARITY_LOW;
		break;
	default:
		device_printf(sc->dev, "unsupported trigger/polarity 0x%2x\n",
		    tripol);
		return (ENOTSUP);
	}
	isrc->isrc_nspc_type = ARM_IRQ_NSPC_PLAIN;
	isrc->isrc_nspc_num = irq;

	/*
	 * 1. The link between ISRC and controller must be set atomically.
	 * 2. Just do things only once in rare case when consumers
	 *    of shared interrupt came here at the same moment.
	 */
	mtx_lock_spin(&sc->sc_mtx);
	if (sc->gpio_pic_irqsrc[irq] != NULL) {
		mtx_unlock_spin(&sc->sc_mtx);
		return (sc->gpio_pic_irqsrc[irq] == isrc ? 0 : EEXIST);
	}
	sc->gpio_pic_irqsrc[irq] = isrc;
	isrc->isrc_data = irq;
	mtx_unlock_spin(&sc->sc_mtx);

	arm_irq_set_name(isrc, "%s,%u", device_get_nameunit(sc->dev), irq);
	return (0);
}

static int
gpio_pic_unregister(device_t dev, struct arm_irqsrc *isrc)
{
	struct imx51_gpio_softc *sc;
	u_int irq;

	sc = device_get_softc(dev);

	mtx_lock_spin(&sc->sc_mtx);
	irq = isrc->isrc_data;
	if (sc->gpio_pic_irqsrc[irq] != isrc) {
		mtx_unlock_spin(&sc->sc_mtx);
		return (sc->gpio_pic_irqsrc[irq] == NULL ? 0 : EINVAL);
	}
	sc->gpio_pic_irqsrc[irq] = NULL;
	isrc->isrc_data = 0;
	mtx_unlock_spin(&sc->sc_mtx);

	arm_irq_set_name(isrc, "");
	return (0);
}

static int
gpio_pic_filter(void *arg)
{
	struct imx51_gpio_softc *sc;
	uint32_t i, interrupts;

	sc = arg;
	mtx_lock_spin(&sc->sc_mtx);
	interrupts = READ4(sc, IMX_GPIO_ISR_REG) & READ4(sc, IMX_GPIO_IMR_REG);
	mtx_unlock_spin(&sc->sc_mtx);

	for (i = 0; interrupts != 0; i++, interrupts >>= 1) {
		if ((interrupts & 0x1) == 0)
			continue;
		if (sc->gpio_pic_irqsrc[i])
			arm_irq_dispatch(sc->gpio_pic_irqsrc[i], curthread->td_intr_frame);
		else
			device_printf(sc->dev, "spurious interrupt %d\n", i);
	}

	return (FILTER_HANDLED);
}
#endif

/*
 *
 */
static void
imx51_gpio_pin_configure(struct imx51_gpio_softc *sc, struct gpio_pin *pin,
    unsigned int flags)
{

	mtx_lock_spin(&sc->sc_mtx);

	/*
	 * Manage input/output
	 */
	if (flags & (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)) {
		pin->gp_flags &= ~(GPIO_PIN_INPUT|GPIO_PIN_OUTPUT);
		if (flags & GPIO_PIN_OUTPUT) {
			pin->gp_flags |= GPIO_PIN_OUTPUT;
			SET4(sc, IMX_GPIO_OE_REG, (1U << pin->gp_pin));
		}
		else {
			pin->gp_flags |= GPIO_PIN_INPUT;
			CLEAR4(sc, IMX_GPIO_OE_REG, (1U << pin->gp_pin));
		}
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
	int i;

	sc = device_get_softc(dev);
	for (i = 0; i < sc->gpio_npins; i++) {
		if (sc->gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->gpio_npins)
		return (EINVAL);

	mtx_lock_spin(&sc->sc_mtx);
	*caps = sc->gpio_pins[i].gp_caps;
	mtx_unlock_spin(&sc->sc_mtx);

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

	mtx_lock_spin(&sc->sc_mtx);
	*flags = sc->gpio_pins[i].gp_flags;
	mtx_unlock_spin(&sc->sc_mtx);

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

	mtx_lock_spin(&sc->sc_mtx);
	memcpy(name, sc->gpio_pins[i].gp_name, GPIOMAXNAME);
	mtx_unlock_spin(&sc->sc_mtx);

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

	mtx_lock_spin(&sc->sc_mtx);
	if (value)
		SET4(sc, IMX_GPIO_DR_REG, (1U << i));
	else
		CLEAR4(sc, IMX_GPIO_DR_REG, (1U << i));
	mtx_unlock_spin(&sc->sc_mtx);

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

	mtx_lock_spin(&sc->sc_mtx);
	*val = (READ4(sc, IMX_GPIO_DR_REG) >> i) & 1;
	mtx_unlock_spin(&sc->sc_mtx);

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

	mtx_lock_spin(&sc->sc_mtx);
	WRITE4(sc, IMX_GPIO_DR_REG,
	    (READ4(sc, IMX_GPIO_DR_REG) ^ (1U << i)));
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
	arm_pic_register(dev, OF_xref_from_node(ofw_bus_get_node(dev)));
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
	DEVMETHOD(pic_disable_source,	gpio_pic_disable_source),
	DEVMETHOD(pic_enable_intr,	gpio_pic_enable_intr),
	DEVMETHOD(pic_enable_source,	gpio_pic_enable_source),
	DEVMETHOD(pic_post_filter,	gpio_pic_post_filter),
	DEVMETHOD(pic_post_ithread,	gpio_pic_post_ithread),
	DEVMETHOD(pic_pre_ithread,	gpio_pic_pre_ithread),
	DEVMETHOD(pic_register,		gpio_pic_register),
	DEVMETHOD(pic_unregister,	gpio_pic_unregister),
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
