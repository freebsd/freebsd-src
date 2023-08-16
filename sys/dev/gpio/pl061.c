/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Amazon.com, Inc. or its affiliates.
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

#include <sys/cdefs.h>
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
#include <sys/interrupt.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/resource.h>

#include <dev/gpio/gpiobusvar.h>

#include "pl061.h"

#include "gpio_if.h"
#include "pic_if.h"

#define	PL061_LOCK(_sc)			mtx_lock_spin(&(_sc)->sc_mtx)
#define	PL061_UNLOCK(_sc)		mtx_unlock_spin(&(_sc)->sc_mtx)
#define	PL061_LOCK_DESTROY(_sc)		mtx_destroy(&(_sc)->sc_mtx)
#define	PL061_ASSERT_LOCKED(_sc)	mtx_assert(&(_sc)->sc_mtx, MA_OWNED)
#define	PL061_ASSERT_UNLOCKED(_sc)	mtx_assert(&(_sc)->sc_mtx, MA_NOTOWNED)

#if 0
#define dprintf(fmt, args...) do { 	\
	printf(fmt, ##args);	  	\
} while (0)
#else
#define dprintf(fmt, args...)
#endif

#define PL061_PIN_TO_ADDR(pin)  (1 << (pin + 2))
#define PL061_DATA		0x3FC
#define PL061_DIR		0x400
#define PL061_INTSENSE  	0x404
#define PL061_INTBOTHEDGES	0x408
#define PL061_INTEVENT		0x40C
#define PL061_INTMASK		0x410
#define PL061_RAWSTATUS		0x414
#define PL061_STATUS		0x418
#define PL061_INTCLR		0x41C
#define PL061_MODECTRL		0x420

#define PL061_ALLOWED_CAPS     (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT | GPIO_INTR_EDGE_BOTH | \
				GPIO_INTR_EDGE_RISING | GPIO_INTR_EDGE_FALLING | \
				GPIO_INTR_LEVEL_HIGH | GPIO_INTR_LEVEL_LOW )

#define PIC_INTR_ISRC(sc, irq) (&(sc->sc_isrcs[irq].isrc))

static device_t
pl061_get_bus(device_t dev)
{
	struct pl061_softc *sc;

	sc = device_get_softc(dev);
	return (sc->sc_busdev);
}

static int
pl061_pin_max(device_t dev, int *maxpin)
{
	*maxpin = PL061_NUM_GPIO - 1;
	return (0);
}

static int
pl061_pin_getname(device_t dev, uint32_t pin, char *name)
{
	if (pin >= PL061_NUM_GPIO)
		return (EINVAL);

	snprintf(name, GPIOMAXNAME, "p%u", pin);
	name[GPIOMAXNAME - 1] = '\0';

	return (0);
}

static int
pl061_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct pl061_softc *sc;
	uint8_t mask = 1 << pin;

	sc = device_get_softc(dev);
	if (pin >= PL061_NUM_GPIO)
		return (EINVAL);

	PL061_LOCK(sc);
	*flags = 0;

	if (mask & bus_read_1(sc->sc_mem_res, PL061_DIR))
		*flags |= GPIO_PIN_OUTPUT;
	else
		*flags |= GPIO_PIN_INPUT;

	PL061_UNLOCK(sc);
	return (0);
}

static int
pl061_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	if (pin >= PL061_NUM_GPIO)
		return (EINVAL);

	*caps = PL061_ALLOWED_CAPS;

	return (0);
}

static void
mask_and_set(struct pl061_softc *sc, long a, uint8_t m, uint8_t b)
{
	uint8_t tmp;

	tmp = bus_read_1(sc->sc_mem_res, a);
	tmp &= ~m;
	tmp |= b;
	bus_write_1(sc->sc_mem_res, a, tmp);
	dprintf("%s: writing %#x to register %#lx\n", __func__, tmp, a);
}

static int
pl061_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct pl061_softc *sc;
	uint8_t mask = 1 << pin;
	const uint32_t in_out = (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT);

	sc = device_get_softc(dev);
	if (pin >= PL061_NUM_GPIO)
		return (EINVAL);

	if (flags & ~PL061_ALLOWED_CAPS)
		return (EINVAL);

	/* can't be both input and output */
	if ((flags & in_out) == in_out)
		return (EINVAL);


	PL061_LOCK(sc);
	mask_and_set(sc, PL061_DIR, mask, flags & GPIO_PIN_OUTPUT ? mask : 0);
	PL061_UNLOCK(sc);
	return (0);
}

static int
pl061_pin_get(device_t dev, uint32_t pin, uint32_t *value)
{
	struct pl061_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= PL061_NUM_GPIO)
		return (EINVAL);

	PL061_LOCK(sc);
	if (bus_read_1(sc->sc_mem_res, PL061_PIN_TO_ADDR(pin)))
		*value = GPIO_PIN_HIGH;
	else
		*value = GPIO_PIN_LOW;
	PL061_UNLOCK(sc);

	return (0);
}

static int
pl061_pin_set(device_t dev, uint32_t pin, uint32_t value)
{
	struct pl061_softc *sc;
	uint8_t d = (value == GPIO_PIN_HIGH) ? 0xff : 0x00;

	sc = device_get_softc(dev);
	if (pin >= PL061_NUM_GPIO)
		return (EINVAL);

	PL061_LOCK(sc);
	bus_write_1(sc->sc_mem_res, PL061_PIN_TO_ADDR(pin), d);
	PL061_UNLOCK(sc);

	return (0);
}

static int
pl061_pin_toggle(device_t dev, uint32_t pin)
{
	struct pl061_softc *sc;
	uint8_t d;

	sc = device_get_softc(dev);
	if (pin >= PL061_NUM_GPIO)
		return (EINVAL);

	PL061_LOCK(sc);
	d = ~bus_read_1(sc->sc_mem_res, PL061_PIN_TO_ADDR(pin));
	bus_write_1(sc->sc_mem_res, PL061_PIN_TO_ADDR(pin), d);
	PL061_UNLOCK(sc);

	return (0);
}

static void
pl061_pic_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct pl061_softc *sc;
	uint8_t mask;

	sc = device_get_softc(dev);
	mask = 1 << ((struct pl061_pin_irqsrc *)isrc)->irq;

	dprintf("%s: calling disable interrupt %#x\n", __func__, mask);
	PL061_LOCK(sc);
	mask_and_set(sc, PL061_INTMASK, mask, 0);
	PL061_UNLOCK(sc);
}



static void
pl061_pic_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct pl061_softc *sc;
	uint8_t mask;

	sc = device_get_softc(dev);
	mask = 1 << ((struct pl061_pin_irqsrc *)isrc)->irq;


	dprintf("%s: calling enable interrupt %#x\n", __func__, mask);
	PL061_LOCK(sc);
	mask_and_set(sc, PL061_INTMASK, mask, mask);
	PL061_UNLOCK(sc);
}

static int
pl061_pic_map_intr(device_t dev, struct intr_map_data *data,
	struct intr_irqsrc **isrcp)
{
	struct pl061_softc *sc;
	struct intr_map_data_gpio *gdata;
	uint32_t irq;

	sc = device_get_softc(dev);
	if (data->type != INTR_MAP_DATA_GPIO)
		return (ENOTSUP);

	gdata = (struct intr_map_data_gpio *)data;
	irq = gdata->gpio_pin_num;
	if (irq >= PL061_NUM_GPIO) {
		device_printf(dev, "invalid interrupt number %u\n", irq);
		return (EINVAL);
	}

	dprintf("%s: calling map interrupt %u\n", __func__, irq);
	*isrcp = PIC_INTR_ISRC(sc, irq);

	return (0);
}

static int
pl061_pic_setup_intr(device_t dev, struct intr_irqsrc *isrc,
	struct resource *res, struct intr_map_data *data)
{
	struct pl061_softc *sc;
	struct intr_map_data_gpio *gdata;
	struct pl061_pin_irqsrc *irqsrc;
	uint32_t mode;
	uint8_t mask;

	if (data == NULL)
		return (ENOTSUP);

	sc = device_get_softc(dev);
	gdata = (struct intr_map_data_gpio *)data;
	irqsrc = (struct pl061_pin_irqsrc *)isrc;

	mode = gdata->gpio_intr_mode;
	mask = 1 << gdata->gpio_pin_num;

	dprintf("%s: calling setup interrupt %u mode %#x\n", __func__,
	    irqsrc->irq, mode);
	if (irqsrc->irq != gdata->gpio_pin_num) {
		dprintf("%s: interrupts don't match\n", __func__);
		return (EINVAL);
	}

	if (isrc->isrc_handlers != 0) {
		dprintf("%s: handler already attached\n", __func__);
		return (irqsrc->mode == mode ? 0 : EINVAL);
	}
	irqsrc->mode = mode;

	PL061_LOCK(sc);

	if (mode & GPIO_INTR_EDGE_BOTH) {
		mask_and_set(sc, PL061_INTBOTHEDGES, mask, mask);
		mask_and_set(sc, PL061_INTSENSE, mask, 0);
	} else if (mode & GPIO_INTR_EDGE_RISING) {
		mask_and_set(sc, PL061_INTBOTHEDGES, mask, 0);
		mask_and_set(sc, PL061_INTSENSE, mask, 0);
		mask_and_set(sc, PL061_INTEVENT, mask, mask);
	} else if (mode & GPIO_INTR_EDGE_FALLING) {
		mask_and_set(sc, PL061_INTBOTHEDGES, mask, 0);
		mask_and_set(sc, PL061_INTSENSE, mask, 0);
		mask_and_set(sc, PL061_INTEVENT, mask, 0);
	} else if (mode & GPIO_INTR_LEVEL_HIGH) {
		mask_and_set(sc, PL061_INTBOTHEDGES, mask, 0);
		mask_and_set(sc, PL061_INTSENSE, mask, mask);
		mask_and_set(sc, PL061_INTEVENT, mask, mask);
	} else if (mode & GPIO_INTR_LEVEL_LOW) {
		mask_and_set(sc, PL061_INTBOTHEDGES, mask, 0);
		mask_and_set(sc, PL061_INTSENSE, mask, mask);
		mask_and_set(sc, PL061_INTEVENT, mask, 0);
	}
	PL061_UNLOCK(sc);
	return (0);
}

static int
pl061_pic_teardown_intr(device_t dev, struct intr_irqsrc *isrc,
	struct resource *res, struct intr_map_data *data)
{
	struct pl061_softc *sc;
	struct pl061_pin_irqsrc *irqsrc;
	uint8_t mask;

	irqsrc = (struct pl061_pin_irqsrc *)isrc;
	mask = 1 << irqsrc->irq;
	dprintf("%s: calling teardown interrupt %#x\n", __func__, mask);

	sc = device_get_softc(dev);
	if (isrc->isrc_handlers == 0) {
		irqsrc->mode = GPIO_INTR_CONFORM;
		PL061_LOCK(sc);
		mask_and_set(sc, PL061_INTMASK, mask, 0);
		PL061_UNLOCK(sc);
	}
	return (0);
}

static void
pl061_pic_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
	struct pl061_softc *sc;
	uint8_t mask;

	sc = device_get_softc(dev);
	mask = 1 << ((struct pl061_pin_irqsrc *)isrc)->irq;
	dprintf("%s: calling post filter %#x\n", __func__, mask);

	bus_write_1(sc->sc_mem_res, PL061_INTCLR, mask);
}

static void
pl061_pic_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct pl061_softc *sc;
	uint8_t mask;

	sc = device_get_softc(dev);
	mask = 1 << ((struct pl061_pin_irqsrc *)isrc)->irq;
	dprintf("%s: calling post ithread %#x\n", __func__, mask);
	bus_write_1(sc->sc_mem_res, PL061_INTCLR, mask);

	pl061_pic_enable_intr(dev, isrc);
}

static void
pl061_pic_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	pl061_pic_disable_intr(dev, isrc);
}

static int
pl061_intr(void *arg)
{
	struct pl061_softc *sc;
	struct trapframe *tf;
	uint8_t status;
	int pin;

	sc = (struct pl061_softc *)arg;
	tf = curthread->td_intr_frame;

	status = bus_read_1(sc->sc_mem_res, PL061_STATUS);

	while (status != 0) {
		pin = ffs(status) - 1;
		status &= ~(1 << pin);

		if (intr_isrc_dispatch(PIC_INTR_ISRC(sc, pin), tf) != 0)
			device_printf(sc->sc_dev, "spurious interrupt %d\n",
			    pin);

		dprintf("got IRQ on %d\n", pin);

	}
	return (FILTER_HANDLED);
}

int
pl061_attach(device_t dev)
{
	struct pl061_softc *sc;
	int ret;
	int irq;
	const char *name;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	sc->sc_mem_rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->sc_mem_rid, RF_ACTIVE);
	if (sc->sc_mem_res == NULL) {
		device_printf(dev, "can't allocate memory resource\n");
		return (ENXIO);
	}

	sc->sc_irq_rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &sc->sc_irq_rid, RF_ACTIVE);

	if (sc->sc_irq_res == NULL) {
		device_printf(dev, "can't allocate IRQ resource\n");
		goto free_mem;
	}

	ret = bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    pl061_intr, NULL, sc, &sc->sc_irq_hdlr);
	if (ret) {
		device_printf(dev, "can't setup IRQ\n");
		goto free_pic;
	}

	name = device_get_nameunit(dev);

	for (irq = 0; irq < PL061_NUM_GPIO; irq++) {
		if (bootverbose) {
			device_printf(dev,
			    "trying to register pin %d name %s\n", irq, name);
		}
		sc->sc_isrcs[irq].irq = irq;
		sc->sc_isrcs[irq].mode = GPIO_INTR_CONFORM;
		ret = intr_isrc_register(PIC_INTR_ISRC(sc, irq), dev, 0,
		    "%s", name);
		if (ret) {
			device_printf(dev, "can't register isrc %d\n", ret);
			goto free_isrc;
		}
	}

	sc->sc_busdev = gpiobus_attach_bus(dev);
	if (sc->sc_busdev == NULL) {
		device_printf(dev, "couldn't attach gpio bus\n");
		goto free_isrc;
	}

	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), "pl061", MTX_SPIN);

	return (0);

free_isrc:
	/*
	 * XXX isrc_release_counters() not implemented
	 * for (irq = 0; irq < PL061_NUM_GPIO; irq++)
	 *	intr_isrc_deregister(PIC_INTR_ISRC(sc, irq));
	*/
	bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irq_rid,
	    sc->sc_irq_res);
free_pic:
        /*
	 * XXX intr_pic_deregister: not implemented
         * intr_pic_deregister(dev, 0);
         */

free_mem:
	bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_mem_rid,
	    sc->sc_mem_res);

	return (ENXIO);

}

int
pl061_detach(device_t dev)
{
	struct pl061_softc *sc;
	sc = device_get_softc(dev);

	if (sc->sc_busdev)
		gpiobus_detach_bus(dev);

	if (sc->sc_irq_hdlr != NULL)
		bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_irq_hdlr);

	if (sc->sc_irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irq_rid,
		    sc->sc_irq_res);

	if (sc->sc_mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_mem_rid,
		    sc->sc_mem_res);
	PL061_LOCK_DESTROY(sc);
	return (0);
}

static device_method_t pl061_methods[] = {
	/* Device interface */
	DEVMETHOD(device_attach,	pl061_attach),
	DEVMETHOD(device_detach,	pl061_detach),

	/* Bus interface */
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus,		pl061_get_bus),
	DEVMETHOD(gpio_pin_max,		pl061_pin_max),
	DEVMETHOD(gpio_pin_getname,	pl061_pin_getname),
	DEVMETHOD(gpio_pin_getflags,	pl061_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps,	pl061_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags,	pl061_pin_setflags),
	DEVMETHOD(gpio_pin_get,		pl061_pin_get),
	DEVMETHOD(gpio_pin_set,		pl061_pin_set),
	DEVMETHOD(gpio_pin_toggle,	pl061_pin_toggle),

	/* Interrupt controller interface */
	DEVMETHOD(pic_disable_intr,	pl061_pic_disable_intr),
	DEVMETHOD(pic_enable_intr,	pl061_pic_enable_intr),
	DEVMETHOD(pic_map_intr,		pl061_pic_map_intr),
	DEVMETHOD(pic_setup_intr,	pl061_pic_setup_intr),
	DEVMETHOD(pic_teardown_intr,	pl061_pic_teardown_intr),
	DEVMETHOD(pic_post_filter,	pl061_pic_post_filter),
	DEVMETHOD(pic_post_ithread,	pl061_pic_post_ithread),
	DEVMETHOD(pic_pre_ithread,	pl061_pic_pre_ithread),

	DEVMETHOD_END
};

DEFINE_CLASS_0(gpio, pl061_driver, pl061_methods, sizeof(struct pl061_softc));
