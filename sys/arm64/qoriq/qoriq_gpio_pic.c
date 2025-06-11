/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Alstom Group.
 * Copyright (c) 2021 Semihalf.
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/stdarg.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/gpio/qoriq_gpio.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dt-bindings/interrupt-controller/irq.h>

#include "gpio_if.h"
#include "pic_if.h"

struct qoriq_gpio_pic_irqsrc {
	struct intr_irqsrc isrc;
	int pin;
};

struct qoriq_gpio_pic_softc {
	struct qoriq_gpio_softc base;

	struct resource *res_irq;
	void *irq_cookie;
	struct qoriq_gpio_pic_irqsrc isrcs[MAXPIN + 1];
	struct intr_map_data_gpio gdata;
};

#define RD4(sc, off) bus_read_4((sc)->base.sc_mem, (off))
#define WR4(sc, off, data) bus_write_4((sc)->base.sc_mem, (off), (data))

static device_probe_t qoriq_gpio_pic_probe;
static device_attach_t qoriq_gpio_pic_attach;
static device_detach_t qoriq_gpio_pic_detach;

static void
qoriq_gpio_pic_set_intr(struct qoriq_gpio_pic_softc *sc, int pin, bool enable)
{
	uint32_t reg;

	reg = RD4(sc, GPIO_GPIMR);
	if (enable)
		reg |= BIT(31 - pin);
	else
		reg &= ~BIT(31 - pin);
	WR4(sc, GPIO_GPIMR, reg);
}

static void
qoriq_gpio_pic_ack_intr(struct qoriq_gpio_pic_softc *sc, int pin)
{
	uint32_t reg;

	reg = BIT(31 - pin);
	WR4(sc, GPIO_GPIER, reg);
}

static int
qoriq_gpio_pic_intr(void *arg)
{
	struct qoriq_gpio_pic_softc *sc;
	struct trapframe *tf;
	uint32_t status;
	int pin;

	sc = (struct qoriq_gpio_pic_softc *)arg;
	tf = curthread->td_intr_frame;

	status = RD4(sc, GPIO_GPIER);
	status &= RD4(sc, GPIO_GPIMR);
	while (status != 0) {
		pin = ffs(status) - 1;
		status &= ~BIT(pin);
		pin = 31 - pin;

		if (intr_isrc_dispatch(&sc->isrcs[pin].isrc, tf) != 0) {
			GPIO_LOCK(&sc->base);
			qoriq_gpio_pic_set_intr(sc, pin, false);
			qoriq_gpio_pic_ack_intr(sc, pin);
			GPIO_UNLOCK(&sc->base);
			device_printf(sc->base.dev,
			    "Masking spurious pin interrupt %d\n",
			    pin);
		}
	}

	return (FILTER_HANDLED);
}

static void
qoriq_gpio_pic_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct qoriq_gpio_pic_softc *sc;
	struct qoriq_gpio_pic_irqsrc *qisrc;

	sc = device_get_softc(dev);
	qisrc = (struct qoriq_gpio_pic_irqsrc *)isrc;

	GPIO_LOCK(&sc->base);
	qoriq_gpio_pic_set_intr(sc, qisrc->pin, false);
	GPIO_UNLOCK(&sc->base);
}

static void
qoriq_gpio_pic_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct qoriq_gpio_pic_softc *sc;
	struct qoriq_gpio_pic_irqsrc *qisrc;

	sc = device_get_softc(dev);
	qisrc = (struct qoriq_gpio_pic_irqsrc *)isrc;

	GPIO_LOCK(&sc->base);
	qoriq_gpio_pic_set_intr(sc, qisrc->pin, true);
	GPIO_UNLOCK(&sc->base);
}

static struct intr_map_data_gpio*
qoriq_gpio_pic_convert_map_data(struct qoriq_gpio_pic_softc *sc, struct intr_map_data *data)
{
	struct intr_map_data_gpio *gdata;
	struct intr_map_data_fdt *daf;

	switch (data->type) {
	case INTR_MAP_DATA_GPIO:
		gdata = (struct intr_map_data_gpio *)data;
		break;
	case INTR_MAP_DATA_FDT:
		daf = (struct intr_map_data_fdt *)data;
		if (daf->ncells != 2)
			return (NULL);

		gdata = &sc->gdata;
		gdata->gpio_pin_num = daf->cells[0];
		switch (daf->cells[1]) {
		case IRQ_TYPE_LEVEL_LOW:
			gdata->gpio_intr_mode = GPIO_INTR_LEVEL_LOW;
			break;
		case IRQ_TYPE_LEVEL_HIGH:
			gdata->gpio_intr_mode = GPIO_INTR_LEVEL_HIGH;
			break;
		case IRQ_TYPE_EDGE_RISING:
			gdata->gpio_intr_mode = GPIO_INTR_EDGE_RISING;
			break;
		case IRQ_TYPE_EDGE_FALLING:
			gdata->gpio_intr_mode = GPIO_INTR_EDGE_FALLING;
			break;
		case IRQ_TYPE_EDGE_BOTH:
			gdata->gpio_intr_mode = GPIO_INTR_EDGE_BOTH;
			break;
		default:
			return (NULL);
		}
		break;
	default:
		return (NULL);
	}

	return (gdata);
}


static int
qoriq_gpio_pic_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	struct qoriq_gpio_pic_softc *sc;
	struct intr_map_data_gpio *gdata;
	int pin;

	sc = device_get_softc(dev);

	gdata = qoriq_gpio_pic_convert_map_data(sc, data);
	if (gdata == NULL)
		return (EINVAL);

	pin = gdata->gpio_pin_num;
	if (pin > MAXPIN)
		return (EINVAL);

	*isrcp = &sc->isrcs[pin].isrc;
	return (0);
}

static int
qoriq_gpio_pic_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct qoriq_gpio_pic_softc *sc;
	struct intr_map_data_gpio *gdata;
	struct qoriq_gpio_pic_irqsrc *qisrc;
	bool falling;
	uint32_t reg;

	sc = device_get_softc(dev);
	qisrc = (struct qoriq_gpio_pic_irqsrc *)isrc;

	gdata = qoriq_gpio_pic_convert_map_data(sc, data);
	if (gdata == NULL)
		return (EINVAL);

	if (gdata->gpio_intr_mode & GPIO_INTR_EDGE_BOTH)
		falling = false;
	else if (gdata->gpio_intr_mode & GPIO_INTR_EDGE_FALLING)
		falling = true;
	else
		return (EOPNOTSUPP);

	GPIO_LOCK(&sc->base);
	reg = RD4(sc, GPIO_GPICR);
	if (falling)
		reg |= BIT(31 - qisrc->pin);
	else
		reg &= ~BIT(31 - qisrc->pin);
	WR4(sc, GPIO_GPICR, reg);
	GPIO_UNLOCK(&sc->base);

	return (0);
}

static int
qoriq_gpio_pic_teardown_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct qoriq_gpio_pic_softc *sc;
	struct qoriq_gpio_pic_irqsrc *qisrc;

	sc = device_get_softc(dev);
	qisrc = (struct qoriq_gpio_pic_irqsrc *)isrc;

	if (isrc->isrc_handlers > 0)
		return (0);

	GPIO_LOCK(&sc->base);
	qoriq_gpio_pic_set_intr(sc, qisrc->pin, false);
	GPIO_UNLOCK(&sc->base);
	return (0);
}

static void
qoriq_gpio_pic_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
	struct qoriq_gpio_pic_softc *sc;
	struct qoriq_gpio_pic_irqsrc *qisrc;

	sc = device_get_softc(dev);
	qisrc = (struct qoriq_gpio_pic_irqsrc *)isrc;

	GPIO_LOCK(&sc->base);
	qoriq_gpio_pic_ack_intr(sc, qisrc->pin);
	GPIO_UNLOCK(&sc->base);
}


static void
qoriq_gpio_pic_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct qoriq_gpio_pic_softc *sc;
	struct qoriq_gpio_pic_irqsrc *qisrc;

	sc = device_get_softc(dev);
	qisrc = (struct qoriq_gpio_pic_irqsrc *)isrc;

	GPIO_LOCK(&sc->base);
	qoriq_gpio_pic_ack_intr(sc, qisrc->pin);
	qoriq_gpio_pic_set_intr(sc, qisrc->pin, true);
	GPIO_UNLOCK(&sc->base);
}

static void
qoriq_gpio_pic_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct qoriq_gpio_pic_softc *sc;
	struct qoriq_gpio_pic_irqsrc *qisrc;

	sc = device_get_softc(dev);
	qisrc = (struct qoriq_gpio_pic_irqsrc *)isrc;

	GPIO_LOCK(&sc->base);
	qoriq_gpio_pic_set_intr(sc, qisrc->pin, false);
	GPIO_UNLOCK(&sc->base);

}
static int
qoriq_gpio_pic_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "fsl,qoriq-gpio"))
		return (ENXIO);

	device_set_desc(dev, "Freescale QorIQ GPIO PIC driver");

	return (BUS_PROBE_DEFAULT);
}

static int
qoriq_gpio_pic_attach(device_t dev)
{
	struct qoriq_gpio_pic_softc *sc;
	int error, rid, i;
	const char *name;
	intptr_t xref;

	sc = device_get_softc(dev);

	error = qoriq_gpio_attach(dev);
	if (error != 0)
		return (error);

	rid = 0;
	sc->res_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->res_irq == NULL) {
		device_printf(dev, "Can't allocate interrupt resource.\n");
		error = ENOMEM;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->res_irq, INTR_TYPE_MISC | INTR_MPSAFE,
	    qoriq_gpio_pic_intr, NULL, sc, &sc->irq_cookie);
	if (error != 0) {
		device_printf(dev, "Failed to setup interrupt.\n");
		goto fail;
	}

	name = device_get_nameunit(dev);
	for (i = 0; i <= MAXPIN; i++) {
		sc->isrcs[i].pin = i;
		error = intr_isrc_register(&sc->isrcs[i].isrc,
		    dev, 0, "%s,%u", name, i);
		if (error != 0)
			goto fail;
	}

	xref = OF_xref_from_node(ofw_bus_get_node(dev));
	if (intr_pic_register(dev, xref) == NULL) {
		error = ENXIO;
		goto fail;
	}

	/* ACK and mask all interrupts. */
	WR4(sc, GPIO_GPIER, 0xffffffff);
	WR4(sc, GPIO_GPIMR, 0);

	return (0);
fail:
	qoriq_gpio_pic_detach(dev);
	return (error);
}

static int
qoriq_gpio_pic_detach(device_t dev)
{
	struct qoriq_gpio_pic_softc *sc;

	sc = device_get_softc(dev);

	if (sc->irq_cookie != NULL)
		bus_teardown_intr(dev, sc->res_irq, sc->irq_cookie);

	if (sc->res_irq != NULL)
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->res_irq), sc->res_irq);

	return (qoriq_gpio_detach(dev));
}


static device_method_t qoriq_gpio_pic_methods[] = {
	DEVMETHOD(device_probe,		qoriq_gpio_pic_probe),
	DEVMETHOD(device_attach,	qoriq_gpio_pic_attach),
	DEVMETHOD(device_detach,	qoriq_gpio_pic_detach),

	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),

	DEVMETHOD(pic_disable_intr,	qoriq_gpio_pic_disable_intr),
	DEVMETHOD(pic_enable_intr,	qoriq_gpio_pic_enable_intr),
	DEVMETHOD(pic_map_intr,		qoriq_gpio_pic_map_intr),
	DEVMETHOD(pic_setup_intr,	qoriq_gpio_pic_setup_intr),
	DEVMETHOD(pic_teardown_intr,	qoriq_gpio_pic_teardown_intr),
	DEVMETHOD(pic_post_filter,	qoriq_gpio_pic_post_filter),
	DEVMETHOD(pic_post_ithread,	qoriq_gpio_pic_post_ithread),
	DEVMETHOD(pic_pre_ithread,	qoriq_gpio_pic_pre_ithread),

	DEVMETHOD_END
};

DEFINE_CLASS_1(gpio, qoriq_gpio_pic_driver, qoriq_gpio_pic_methods,
    sizeof(struct qoriq_gpio_pic_softc), qoriq_gpio_driver);
EARLY_DRIVER_MODULE(qoriq_gpio_pic, simplebus, qoriq_gpio_pic_driver, NULL, NULL,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
