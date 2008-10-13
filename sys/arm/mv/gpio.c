/*-
 * Copyright (c) 2006 Benno Rice.
 * Copyright (C) 2008 MARVELL INTERNATIONAL LTD.
 * All rights reserved.
 *
 * Adapted and extended for Marvell SoCs by Semihalf.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * from: FreeBSD: //depot/projects/arm/src/sys/arm/xscale/pxa2x0/pxa2x0_gpio.c, rev 1
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/interrupt.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/queue.h>
#include <sys/timetc.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <arm/mv/mvvar.h>
#include <arm/mv/mvreg.h>

#define GPIO_MAX_INTR_COUNT	8
#define GPIO_PINS_PER_REG	32

struct mv_gpio_softc {
	struct resource *	res[GPIO_MAX_INTR_COUNT + 1];
	void			*ih_cookie[GPIO_MAX_INTR_COUNT];
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	uint8_t			pin_num;	/* number of GPIO pins */
	uint8_t			irq_num;	/* number of real IRQs occupied by GPIO controller */
	uint8_t			use_high;
};

extern struct resource_spec mv_gpio_spec[];

static struct mv_gpio_softc *mv_gpio_softc = NULL;
static uint32_t	gpio_setup[MV_GPIO_MAX_NPINS];

static int	mv_gpio_probe(device_t);
static int	mv_gpio_attach(device_t);
static void	mv_gpio_intr(void *);

static void	mv_gpio_intr_handler(int pin);
static uint32_t	mv_gpio_reg_read(uint32_t reg);
static void	mv_gpio_reg_write(uint32_t reg, uint32_t val);
static void	mv_gpio_reg_set(uint32_t reg, uint32_t val);
static void	mv_gpio_reg_clear(uint32_t reg, uint32_t val);

static void	mv_gpio_blink(uint32_t pin, uint8_t enable);
static void	mv_gpio_polarity(uint32_t pin, uint8_t enable);
static void	mv_gpio_level(uint32_t pin, uint8_t enable);
static void	mv_gpio_edge(uint32_t pin, uint8_t enable);
static void	mv_gpio_out_en(uint32_t pin, uint8_t enable);
static void	mv_gpio_int_ack(uint32_t pin);
static void	mv_gpio_value_set(uint32_t pin, uint8_t val);
static uint32_t	mv_gpio_value_get(uint32_t pin);

static device_method_t mv_gpio_methods[] = {
	DEVMETHOD(device_probe,		mv_gpio_probe),
	DEVMETHOD(device_attach,	mv_gpio_attach),
	{ 0, 0 }
};

static driver_t mv_gpio_driver = {
	"gpio",
	mv_gpio_methods,
	sizeof(struct mv_gpio_softc),
};

static devclass_t mv_gpio_devclass;

DRIVER_MODULE(gpio, mbus, mv_gpio_driver, mv_gpio_devclass, 0, 0);

static int
mv_gpio_probe(device_t dev)
{

	device_set_desc(dev, "Marvell Integrated GPIO Controller");
	return (0);
}

static int
mv_gpio_attach(device_t dev)
{
	int	error, i;
	struct	mv_gpio_softc *sc;
	uint32_t dev_id, rev_id;

	sc = (struct mv_gpio_softc *)device_get_softc(dev);

	if (mv_gpio_softc != NULL)
		return (ENXIO);
	mv_gpio_softc = sc;

	/* Get board id and revision */
	soc_id(&dev_id, &rev_id);

	if (dev_id == MV_DEV_88F5182 ||
	    dev_id == MV_DEV_88F5281 ||
	    dev_id == MV_DEV_MV78100) {
		sc->pin_num = 32;
		sc->irq_num = 4;
		sc->use_high = 0;

	} else if (dev_id == MV_DEV_88F6281) {
		sc->pin_num = 50;
		sc->irq_num = 7;
		sc->use_high = 1;

	} else {
		device_printf(dev, "unknown board id=0x%x\n", dev_id);
		return (ENXIO);
	}

	error = bus_alloc_resources(dev, mv_gpio_spec, sc->res);
	if (error) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	sc->bst = rman_get_bustag(sc->res[0]);
	sc->bsh = rman_get_bushandle(sc->res[0]);

	/* Disable and clear all interrupts */
	bus_space_write_4(sc->bst, sc->bsh, GPIO_INT_EDGE_MASK, 0);
	bus_space_write_4(sc->bst, sc->bsh, GPIO_INT_LEV_MASK, 0);
	bus_space_write_4(sc->bst, sc->bsh, GPIO_INT_CAUSE, 0);

	if (sc->use_high) {
		bus_space_write_4(sc->bst, sc->bsh,
		    GPIO_HI_INT_EDGE_MASK, 0);
		bus_space_write_4(sc->bst, sc->bsh,
		    GPIO_HI_INT_LEV_MASK, 0);
		bus_space_write_4(sc->bst, sc->bsh,
		    GPIO_HI_INT_CAUSE, 0);
	}

	for (i = 0; i < sc->irq_num; i++) {
		if (bus_setup_intr(dev, sc->res[1 + i],
		    INTR_TYPE_MISC | INTR_FAST,
		    (driver_filter_t *)mv_gpio_intr, NULL,
		    sc, &sc->ih_cookie[i]) != 0) {
			bus_release_resources(dev, mv_gpio_spec, sc->res);
			device_printf(dev, "could not set up intr %d\n", i);
			return (ENXIO);
		}
	}

	return (0);
}

static void
mv_gpio_intr(void *arg)
{
	uint32_t	int_cause, gpio_val;
	uint32_t	int_cause_hi, gpio_val_hi = 0;
	int		i;

	int_cause = mv_gpio_reg_read(GPIO_INT_CAUSE);
	gpio_val = mv_gpio_reg_read(GPIO_DATA_IN);
	gpio_val &= int_cause;
	if (mv_gpio_softc->use_high) {
		int_cause_hi = mv_gpio_reg_read(GPIO_HI_INT_CAUSE);
		gpio_val_hi = mv_gpio_reg_read(GPIO_HI_DATA_IN);
		gpio_val_hi &= int_cause_hi;
	}

	i = 0;
	while (gpio_val != 0) {
		if (gpio_val & 1)
			mv_gpio_intr_handler(i);
		gpio_val >>= 1;
		i++;
	}

	if (mv_gpio_softc->use_high) {
		i = 0;
		while (gpio_val_hi != 0) {
			if (gpio_val_hi & 1)
				mv_gpio_intr_handler(i + GPIO_PINS_PER_REG);
			gpio_val_hi >>= 1;
			i++;
		}
	}
}

/*
 * GPIO interrupt handling
 */

static struct intr_event	*gpio_events[MV_GPIO_MAX_NPINS];

int
mv_gpio_setup_intrhandler(const char *name, driver_filter_t *filt,
    void (*hand)(void *), void *arg, int pin, int flags, void **cookiep)
{
	struct	intr_event *event;
	int	error;

	if (pin < 0 || pin >= mv_gpio_softc->pin_num)
		return (ENXIO);
	event = gpio_events[pin];
	if (event == NULL) {
		error = intr_event_create(&event, (void *)pin, 0, pin,
		    (void (*)(void *))mv_gpio_intr_mask,
		    (void (*)(void *))mv_gpio_intr_unmask,
		    (void (*)(void *))mv_gpio_int_ack,
		    NULL,
		    "gpio%d:", pin);
		if (error != 0)
			return (error);
		gpio_events[pin] = event;
	}

	intr_event_add_handler(event, name, filt, hand, arg, intr_priority(flags),
	    flags, cookiep);
	return (0);
}

void
mv_gpio_intr_mask(int pin)
{

	if (pin >= mv_gpio_softc->pin_num)
		return;

	if (gpio_setup[pin] & MV_GPIO_EDGE)
		mv_gpio_edge(pin, 0);
	else
		mv_gpio_level(pin, 0);
}

void
mv_gpio_intr_unmask(int pin)
{

	if (pin >= mv_gpio_softc->pin_num)
		return;

	if (gpio_setup[pin] & MV_GPIO_EDGE)
		mv_gpio_edge(pin, 1);
	else
		mv_gpio_level(pin, 1);
}

static void
mv_gpio_intr_handler(int pin)
{
	struct	intr_event *event;

	event = gpio_events[pin];
	if (event == NULL || TAILQ_EMPTY(&event->ie_handlers))
		return;

	intr_event_handle(event, NULL);
}

int
mv_gpio_configure(uint32_t pin, uint32_t flags, uint32_t mask)
{

	if (pin >= mv_gpio_softc->pin_num)
		return (EINVAL);

	if (mask & MV_GPIO_BLINK)
		mv_gpio_blink(pin, flags & MV_GPIO_BLINK);
	if (mask & MV_GPIO_POLARITY)
		mv_gpio_polarity(pin, flags & MV_GPIO_POLARITY);
	if (mask & MV_GPIO_EDGE)
		mv_gpio_edge(pin, flags & MV_GPIO_EDGE);
	if (mask & MV_GPIO_LEVEL)
		mv_gpio_level(pin, flags & MV_GPIO_LEVEL);

	gpio_setup[pin] &= ~(mask);
	gpio_setup[pin] |= (flags & mask);

	return (0);
}

void
mv_gpio_out(uint32_t pin, uint8_t val, uint8_t enable)
{

	mv_gpio_value_set(pin, val);
	mv_gpio_out_en(pin, enable);
}

uint8_t
mv_gpio_in(uint32_t pin)
{

	return (mv_gpio_value_get(pin));
}

static uint32_t
mv_gpio_reg_read(uint32_t reg)
{

	return (bus_space_read_4(mv_gpio_softc->bst,
	    mv_gpio_softc->bsh, reg));
}

static void
mv_gpio_reg_write(uint32_t reg, uint32_t val)
{

	bus_space_write_4(mv_gpio_softc->bst,
	    mv_gpio_softc->bsh, reg, val);
}

static void
mv_gpio_reg_set(uint32_t reg, uint32_t pin)
{
	uint32_t reg_val;

	reg_val = mv_gpio_reg_read(reg);
	reg_val |= GPIO(pin);
	mv_gpio_reg_write(reg, reg_val);
}

static void
mv_gpio_reg_clear(uint32_t reg, uint32_t pin)
{
	uint32_t reg_val;

	reg_val = mv_gpio_reg_read(reg);
	reg_val &= ~(GPIO(pin));
	mv_gpio_reg_write(reg, reg_val);
}

static void
mv_gpio_out_en(uint32_t pin, uint8_t enable)
{
	uint32_t reg;

	if (pin >= mv_gpio_softc->pin_num)
		return;

	if (pin >= GPIO_PINS_PER_REG) {
		reg = GPIO_HI_DATA_OUT_EN_CTRL;
		pin -= GPIO_PINS_PER_REG;
	} else
		reg = GPIO_DATA_OUT_EN_CTRL;

	if (enable)
		mv_gpio_reg_clear(reg, pin);
	else
		mv_gpio_reg_set(reg, pin);
}

static void
mv_gpio_blink(uint32_t pin, uint8_t enable)
{
	uint32_t reg;

	if (pin >= mv_gpio_softc->pin_num)
		return;

	if (pin >= GPIO_PINS_PER_REG) {
		reg = GPIO_HI_BLINK_EN;
		pin -= GPIO_PINS_PER_REG;
	} else
		reg = GPIO_BLINK_EN;

	if (enable)
		mv_gpio_reg_set(reg, pin);
	else
		mv_gpio_reg_clear(reg, pin);
}

static void
mv_gpio_polarity(uint32_t pin, uint8_t enable)
{
	uint32_t reg;

	if (pin >= mv_gpio_softc->pin_num)
		return;

	if (pin >= GPIO_PINS_PER_REG) {
		reg = GPIO_HI_DATA_IN_POLAR;
		pin -= GPIO_PINS_PER_REG;
	} else
		reg = GPIO_DATA_IN_POLAR;

	if (enable)
		mv_gpio_reg_set(reg, pin);
	else
		mv_gpio_reg_clear(reg, pin);
}

static void
mv_gpio_level(uint32_t pin, uint8_t enable)
{
	uint32_t reg;

	if (pin >= mv_gpio_softc->pin_num)
		return;

	if (pin >= GPIO_PINS_PER_REG) {
		reg = GPIO_HI_INT_LEV_MASK;
		pin -= GPIO_PINS_PER_REG;
	} else
		reg = GPIO_INT_LEV_MASK;

	if (enable)
		mv_gpio_reg_set(reg, pin);
	else
		mv_gpio_reg_clear(reg, pin);
}

static void
mv_gpio_edge(uint32_t pin, uint8_t enable)
{
	uint32_t reg;

	if (pin >= mv_gpio_softc->pin_num)
		return;

	if (pin >= GPIO_PINS_PER_REG) {
		reg = GPIO_HI_INT_EDGE_MASK;
		pin -= GPIO_PINS_PER_REG;
	} else
		reg = GPIO_INT_EDGE_MASK;

	if (enable)
		mv_gpio_reg_set(reg, pin);
	else
		mv_gpio_reg_clear(reg, pin);
}

static void
mv_gpio_int_ack(uint32_t pin)
{
	uint32_t reg;

	if (pin >= mv_gpio_softc->pin_num)
		return;

	if (pin >= GPIO_PINS_PER_REG) {
		reg = GPIO_HI_INT_CAUSE;
		pin -= GPIO_PINS_PER_REG;
	} else
		reg = GPIO_INT_CAUSE;

	mv_gpio_reg_clear(reg, pin);
}

static uint32_t
mv_gpio_value_get(uint32_t pin)
{
	uint32_t reg, reg_val;

	if (pin >= mv_gpio_softc->pin_num)
		return (0);

	if (pin >= GPIO_PINS_PER_REG) {
		reg = GPIO_HI_DATA_IN;
		pin -= GPIO_PINS_PER_REG;
	} else
		reg = GPIO_DATA_IN;

	reg_val = mv_gpio_reg_read(reg);

	return (reg_val & GPIO(pin));
}

static void
mv_gpio_value_set(uint32_t pin, uint8_t val)
{
	uint32_t reg;

	if (pin >= mv_gpio_softc->pin_num)
		return;

	if (pin >= GPIO_PINS_PER_REG) {
		reg = GPIO_HI_DATA_OUT;
		pin -= GPIO_PINS_PER_REG;
	} else
		reg = GPIO_DATA_OUT;

	if (val)
		mv_gpio_reg_set(reg, pin);
	else
		mv_gpio_reg_clear(reg, pin);
}
