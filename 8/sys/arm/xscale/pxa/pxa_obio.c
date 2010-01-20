/*-
 * Copyright (c) 2006 Benno Rice.  All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <arm/xscale/pxa/pxavar.h>
#include <arm/xscale/pxa/pxareg.h>

static void	pxa_identify(driver_t *, device_t);
static int	pxa_probe(device_t);
static int	pxa_attach(device_t);

static int	pxa_print_child(device_t, device_t);

static int	pxa_setup_intr(device_t, device_t, struct resource *, int,
		    driver_filter_t *, driver_intr_t *, void *, void **);
static int	pxa_read_ivar(device_t, device_t, int, uintptr_t *);

static struct resource_list *	pxa_get_resource_list(device_t, device_t);
static struct resource *	pxa_alloc_resource(device_t, device_t, int,
				    int *, u_long, u_long, u_long, u_int);
static int			pxa_release_resource(device_t, device_t, int,
				    int, struct resource *);
static int			pxa_activate_resource(device_t, device_t,
				    int, int, struct resource *);

static struct resource *	pxa_alloc_gpio_irq(device_t, device_t, int,
				    int *, u_long, u_long, u_long, u_int);

struct obio_device {
	const char	*od_name;
	u_long		od_base;
	u_long		od_size;
	u_int		od_irqs[5];
	struct resource_list od_resources;
};

static struct obio_device obio_devices[] = {
	{ "icu", PXA2X0_INTCTL_BASE, PXA2X0_INTCTL_SIZE, { 0 } },
	{ "timer", PXA2X0_OST_BASE, PXA2X0_OST_SIZE, { PXA2X0_INT_OST0, PXA2X0_INT_OST1, PXA2X0_INT_OST2, PXA2X0_INT_OST3, 0 } },
	{ "dmac", PXA2X0_DMAC_BASE, PXA2X0_DMAC_SIZE, { PXA2X0_INT_DMA, 0 } },
	{ "gpio", PXA2X0_GPIO_BASE, PXA250_GPIO_SIZE, { PXA2X0_INT_GPIO0, PXA2X0_INT_GPIO1, PXA2X0_INT_GPION, 0 } },
	{ "uart", PXA2X0_FFUART_BASE, PXA2X0_FFUART_SIZE, { PXA2X0_INT_FFUART, 0 } },
	{ "uart", PXA2X0_BTUART_BASE, PXA2X0_BTUART_SIZE, { PXA2X0_INT_BTUART, 0 } },
	{ "uart", PXA2X0_STUART_BASE, PXA2X0_STUART_SIZE, { PXA2X0_INT_STUART, 0 } },
	{ "uart", PXA2X0_HWUART_BASE, PXA2X0_HWUART_SIZE, { PXA2X0_INT_HWUART, 0 } },
	{ "smi", PXA2X0_CS0_START, PXA2X0_CS_SIZE * 6, { 0 } },
	{ NULL, 0, 0, { 0 } }
};

void
pxa_identify(driver_t *driver, device_t parent)
{

	BUS_ADD_CHILD(parent, 0, "pxa", 0);
}

int
pxa_probe(device_t dev)
{

	device_set_desc(dev, "XScale PXA On-board IO");
	return (0);
}

int
pxa_attach(device_t dev)
{
	struct		obio_softc *sc;
	struct		obio_device *od;
	int		i;
	device_t	child;

	sc = device_get_softc(dev);

	sc->obio_bst = obio_tag;

	sc->obio_mem.rm_type = RMAN_ARRAY;
	sc->obio_mem.rm_descr = "PXA2X0 OBIO Memory";
	if (rman_init(&sc->obio_mem) != 0)
		panic("pxa_attach: failed to init obio mem rman");
	if (rman_manage_region(&sc->obio_mem, 0, PXA250_PERIPH_END) != 0)
		panic("pxa_attach: failed to set up obio mem rman");

	sc->obio_irq.rm_type = RMAN_ARRAY;
	sc->obio_irq.rm_descr = "PXA2X0 OBIO IRQ";
	if (rman_init(&sc->obio_irq) != 0)
		panic("pxa_attach: failed to init obio irq rman");
	if (rman_manage_region(&sc->obio_irq, 0, 31) != 0)
		panic("pxa_attach: failed to set up obio irq rman (main irqs)");
	if (rman_manage_region(&sc->obio_irq, IRQ_GPIO0, IRQ_GPIO_MAX) != 0)
		panic("pxa_attach: failed to set up obio irq rman (gpio irqs)");

	for (od = obio_devices; od->od_name != NULL; od++) {
		resource_list_init(&od->od_resources);

		resource_list_add(&od->od_resources, SYS_RES_MEMORY, 0,
		    od->od_base, od->od_base + od->od_size, od->od_size);

		for (i = 0; od->od_irqs[i] != 0; i++) {
			resource_list_add(&od->od_resources, SYS_RES_IRQ, i,
			    od->od_irqs[i], od->od_irqs[i], 1);
		}

		child = device_add_child(dev, od->od_name, -1);
		device_set_ivars(child, od);
	}

	bus_generic_probe(dev);
	bus_generic_attach(dev);

	return (0);
}

static int
pxa_print_child(device_t dev, device_t child)
{
	struct	obio_device *od;
	int	retval;

	od = (struct obio_device *)device_get_ivars(child);
	if (od == NULL)
		panic("Unknown device on pxa0");

	retval = 0;

	retval += bus_print_child_header(dev, child);

	retval += resource_list_print_type(&od->od_resources, "at mem",
	    SYS_RES_MEMORY, "0x%08lx");
	retval += resource_list_print_type(&od->od_resources, "irq",
	    SYS_RES_IRQ, "%ld");

	retval += bus_print_child_footer(dev, child);

	return (retval);
}

static int
pxa_setup_intr(device_t dev, device_t child, struct resource *irq, int flags,
    driver_filter_t *filter, driver_intr_t *ithread, void *arg, void **cookiep)
{
	struct	obio_softc *sc;

	sc = (struct obio_softc *)device_get_softc(dev);

	BUS_SETUP_INTR(device_get_parent(dev), child, irq, flags, filter,
	    ithread, arg, cookiep);
	arm_unmask_irq(rman_get_start(irq));
	return (0);
}

static int
pxa_teardown_intr(device_t dev, device_t child, struct resource *ires,
    void *cookie)
{
	return (BUS_TEARDOWN_INTR(device_get_parent(dev), child, ires, cookie));}

static int
pxa_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct	obio_device *od;

	od = (struct obio_device *)device_get_ivars(child);

	switch (which) {
	case PXA_IVAR_BASE:
		*((u_long *)result) = od->od_base;
		break;

	default:
		return (ENOENT);
	}

	return (0);
}

static struct resource_list *
pxa_get_resource_list(device_t dev, device_t child)
{
	struct	obio_device *od;

	od = (struct obio_device *)device_get_ivars(child);

	if (od == NULL)
		return (NULL);

	return (&od->od_resources);
}

static struct resource *
pxa_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct	obio_softc *sc;
	struct	obio_device *od;
	struct	resource *rv;
	struct	resource_list *rl;
	struct	resource_list_entry *rle;
	struct	rman *rm;
	int	needactivate;

	sc = (struct obio_softc *)device_get_softc(dev);
	od = (struct obio_device *)device_get_ivars(child);
	rl = &od->od_resources;

	rle = resource_list_find(rl, type, *rid);
	if (rle == NULL) {
		/* We can allocate GPIO-based IRQs lazily. */
		if (type == SYS_RES_IRQ)
			return (pxa_alloc_gpio_irq(dev, child, type, rid,
			    start, end, count, flags));
		return (NULL);
	}
	if (rle->res != NULL)
		panic("pxa_alloc_resource: resource is busy");

	switch (type) {
	case SYS_RES_IRQ:
		rm = &sc->obio_irq;
		break;

	case SYS_RES_MEMORY:
		rm = &sc->obio_mem;
		break;

	default:
		return (NULL);
	}

	needactivate = flags & RF_ACTIVE;
	flags &= ~RF_ACTIVE;
	rv = rman_reserve_resource(rm, rle->start, rle->end, rle->count, flags,
	    child);
	if (rv == NULL)
		return (NULL);
	rle->res = rv;
	rman_set_rid(rv, *rid);
	if (type == SYS_RES_MEMORY) {
		rman_set_bustag(rv, sc->obio_bst);
		rman_set_bushandle(rv, rle->start);
	}

	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			rman_release_resource(rv);
			return (NULL);
		}
	}

	return (rv);
}

static int
pxa_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct	obio_device *od;
	struct	resource_list *rl;
	struct	resource_list_entry *rle;

	od = (struct obio_device *)device_get_ivars(child);
	rl = &od->od_resources;

	if (type == SYS_RES_IOPORT)
		type = SYS_RES_MEMORY;

	rle = resource_list_find(rl, type, rid);

	if (!rle)
		panic("pxa_release_resource: can't find resource");
	if (!rle->res)
		panic("pxa_release_resource: resource entry is not busy");

	rman_release_resource(rle->res);
	rle->res = NULL;

	return (0);
}

static int
pxa_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{

	return (rman_activate_resource(r));
}

static device_method_t pxa_methods[] = {
	DEVMETHOD(device_identify,	pxa_identify),
	DEVMETHOD(device_probe,		pxa_probe),
	DEVMETHOD(device_attach,	pxa_attach),

	DEVMETHOD(bus_print_child,	pxa_print_child),

	DEVMETHOD(bus_read_ivar,	pxa_read_ivar),
	DEVMETHOD(bus_setup_intr,	pxa_setup_intr),
	DEVMETHOD(bus_teardown_intr,	pxa_teardown_intr),

	DEVMETHOD(bus_get_resource_list,	pxa_get_resource_list),
	DEVMETHOD(bus_alloc_resource,		pxa_alloc_resource),
	DEVMETHOD(bus_release_resource,		pxa_release_resource),
	DEVMETHOD(bus_activate_resource,	pxa_activate_resource),

	{0, 0}
};

static driver_t pxa_driver = {
	"pxa",
	pxa_methods,
	sizeof(struct obio_softc),
};

static devclass_t pxa_devclass;

DRIVER_MODULE(pxa, nexus, pxa_driver, pxa_devclass, 0, 0);

static struct resource *
pxa_alloc_gpio_irq(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct	obio_softc *sc;
	struct	obio_device *od;
	struct	resource_list *rl;
	struct	resource_list_entry *rle;
	struct	resource *rv;
	struct	rman *rm;
	int	needactivate;

	sc = device_get_softc(dev);
	od = device_get_ivars(child);
	rl = &od->od_resources;
	rm = &sc->obio_irq;

	needactivate = flags & RF_ACTIVE;
	flags &= ~RF_ACTIVE;
	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == NULL)
		return (NULL);

	resource_list_add(rl, type, *rid, start, end, count);
	rle = resource_list_find(rl, type, *rid);
	if (rle == NULL)
		panic("pxa_alloc_gpio_irq: unexpectedly can't find resource");

	rle->res = rv;
	rle->start = rman_get_start(rv);
	rle->end = rman_get_end(rv);
	rle->count = count;

	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			rman_release_resource(rv);
			return (NULL);
		}
	}

	if (bootverbose)
		device_printf(dev, "lazy allocation of irq %ld for %s\n",
		    start, device_get_nameunit(child));

	return (rv);
}
