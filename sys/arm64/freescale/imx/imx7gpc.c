/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Oleksandr Tymoshenko <gonzo@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "pic_if.h"

struct imx7gpc_softc {
	device_t		dev;
	struct resource		*memres;
	device_t		parent;
};

static struct ofw_compat_data compat_data[] = {
	{ "fsl,imx7gpc",		1},
	{ "fsl,imx8mq-gpc",		1},
	{ NULL,				0}
};

static int
imx7gpc_activate_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct imx7gpc_softc *sc = device_get_softc(dev);

	return (PIC_ACTIVATE_INTR(sc->parent, isrc, res, data));
}

static void
imx7gpc_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct imx7gpc_softc *sc = device_get_softc(dev);

	PIC_DISABLE_INTR(sc->parent, isrc);
}

static void
imx7gpc_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct imx7gpc_softc *sc = device_get_softc(dev);

	PIC_ENABLE_INTR(sc->parent, isrc);
}

static int
imx7gpc_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	struct imx7gpc_softc *sc = device_get_softc(dev);

	return (PIC_MAP_INTR(sc->parent, data, isrcp));
}

static int
imx7gpc_deactivate_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct imx7gpc_softc *sc = device_get_softc(dev);

	return (PIC_DEACTIVATE_INTR(sc->parent, isrc, res, data));
}

static int
imx7gpc_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct imx7gpc_softc *sc = device_get_softc(dev);

	return (PIC_SETUP_INTR(sc->parent, isrc, res, data));
}

static int
imx7gpc_teardown_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct imx7gpc_softc *sc = device_get_softc(dev);

	return (PIC_TEARDOWN_INTR(sc->parent, isrc, res, data));
}

static void
imx7gpc_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct imx7gpc_softc *sc = device_get_softc(dev);

	PIC_PRE_ITHREAD(sc->parent, isrc);
}

static void
imx7gpc_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct imx7gpc_softc *sc = device_get_softc(dev);

	PIC_POST_ITHREAD(sc->parent, isrc);
}

static void
imx7gpc_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
	struct imx7gpc_softc *sc = device_get_softc(dev);

	PIC_POST_FILTER(sc->parent, isrc);
}

#ifdef SMP
static int
imx7gpc_bind_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct imx7gpc_softc *sc = device_get_softc(dev);

	return (PIC_BIND_INTR(sc->parent, isrc));
}
#endif

static int
imx7gpc_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "General Power Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
imx7gpc_attach(device_t dev)
{
	struct imx7gpc_softc *sc = device_get_softc(dev);
	phandle_t node;
	phandle_t parent_xref;
	int i, rv;

	sc->dev = dev;

	node = ofw_bus_get_node(dev);

	rv = OF_getencprop(node, "interrupt-parent", &parent_xref,
	    sizeof(parent_xref));
	if (rv <= 0) {
		device_printf(dev, "Can't read parent node property\n");
		return (ENXIO);
	}
	sc->parent = OF_device_from_xref(parent_xref);
	if (sc->parent == NULL) {
		device_printf(dev, "Can't find parent controller\n");
		return (ENXIO);
	}

	i = 0;
	sc->memres = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &i,
	    RF_ACTIVE);
	if (sc->memres == NULL) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* TODO: power up OTG domain and unmask all interrupts */

	if (intr_pic_register(dev, OF_xref_from_node(node)) == NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, i, sc->memres);
		device_printf(dev, "Cannot register PIC\n");
		return (ENXIO);
	}

	return (0);
}

static device_method_t imx7gpc_methods[] = {
	DEVMETHOD(device_probe,		imx7gpc_probe),
	DEVMETHOD(device_attach,	imx7gpc_attach),

	/* Interrupt controller interface */
	DEVMETHOD(pic_activate_intr,	imx7gpc_activate_intr),
	DEVMETHOD(pic_disable_intr,	imx7gpc_disable_intr),
	DEVMETHOD(pic_enable_intr,	imx7gpc_enable_intr),
	DEVMETHOD(pic_map_intr,		imx7gpc_map_intr),
	DEVMETHOD(pic_deactivate_intr,	imx7gpc_deactivate_intr),
	DEVMETHOD(pic_setup_intr,	imx7gpc_setup_intr),
	DEVMETHOD(pic_teardown_intr,	imx7gpc_teardown_intr),
	DEVMETHOD(pic_pre_ithread,	imx7gpc_pre_ithread),
	DEVMETHOD(pic_post_ithread,	imx7gpc_post_ithread),
	DEVMETHOD(pic_post_filter,	imx7gpc_post_filter),
#ifdef SMP
	DEVMETHOD(pic_bind_intr,	imx7gpc_bind_intr),
#endif

	DEVMETHOD_END
};

static driver_t imx7gpc_driver = {
	"imx7gpc",
	imx7gpc_methods,
	sizeof(struct imx7gpc_softc),
};

static devclass_t imx7gpc_devclass;

EARLY_DRIVER_MODULE(imx7gpc, ofwbus, imx7gpc_driver, imx7gpc_devclass, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
EARLY_DRIVER_MODULE(imx7gpc, simplebus, imx7gpc_driver, imx7gpc_devclass, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
