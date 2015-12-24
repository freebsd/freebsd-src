/*	$NetBSD: obio.c,v 1.11 2003/07/15 00:25:05 lukem Exp $	*/

/*-
 * Copyright (c) 2001, 2002, 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <mips/rt305x/rt305xreg.h>
#include <mips/rt305x/obiovar.h>
#include <mips/rt305x/rt305x_icvar.h>

/* MIPS HW interrupts of IRQ/FIQ respectively */
#define RT305X_INTR		0
#define RT305X_FAST_INTR	1

/* Interrupt levels */
#define INTR_IRQ 0
#define INTR_FIQ 1


int irq_priorities[NIRQS] = {
	INTR_IRQ,	/* SYSCTL */
	INTR_FIQ,	/* TIMER0 */
	INTR_FIQ,	/* WDTIMER */
	INTR_IRQ,	/* Illegal Access */
	INTR_IRQ,	/* PCM */
	INTR_IRQ,	/* UART */
	INTR_IRQ,	/* GPIO */
	INTR_FIQ,	/* GDMA */
	INTR_IRQ,	/* NAND */
	INTR_IRQ,	/* Perfomance Counter */
	INTR_IRQ,	/* I2S */
	INTR_IRQ,	/* unknown */
	INTR_IRQ,	/* UARTLITE */
	INTR_IRQ,	/* unknown */
	INTR_IRQ,	/* unknown */
	INTR_IRQ,	/* unknown */
	INTR_IRQ,	/* unknown */
	INTR_IRQ,	/* EtherNet Switch */
	INTR_FIQ,	/* OTG */
	INTR_IRQ,	/* unknown */
	INTR_IRQ,	/* unknown */
	INTR_IRQ,	/* unknown */
	INTR_IRQ,	/* unknown */
	INTR_IRQ,	/* unknown */
	INTR_IRQ,	/* unknown */
	INTR_IRQ,	/* unknown */
	INTR_IRQ,	/* unknown */
	INTR_IRQ,	/* unknown */
	INTR_IRQ,	/* unknown */
	INTR_IRQ,	/* unknown */
	INTR_IRQ,	/* unknown */
	INTR_IRQ,	/* unknown */
};


#define REG_READ(o) *((volatile uint32_t *)MIPS_PHYS_TO_KSEG1(INTCTL_BASE + (o)))
#define REG_WRITE(o,v) (REG_READ(o)) = (v)

static int	obio_activate_resource(device_t, device_t, int, int,
		    struct resource *);
static device_t	obio_add_child(device_t, u_int, const char *, int);
static struct resource *
		obio_alloc_resource(device_t, device_t, int, int *, u_long,
		    u_long, u_long, u_int);
static int	obio_attach(device_t);
static int	obio_deactivate_resource(device_t, device_t, int, int,
		    struct resource *);
static struct resource_list *
		obio_get_resource_list(device_t, device_t);
static void	obio_add_res_child(device_t, const char *, int, long, int, int);
static void	obio_hinted_child(device_t, const char *, int);
static int	obio_intr(void *);
static int	obio_probe(device_t);
static int	obio_release_resource(device_t, device_t, int, int,
		    struct resource *);
static int	obio_setup_intr(device_t, device_t, struct resource *, int,
		    driver_filter_t *, driver_intr_t *, void *, void **);
static int	obio_teardown_intr(device_t, device_t, struct resource *,
		    void *);

static void 
obio_mask_irq(void *source)
{
	int irq;
	uint32_t irqmask;

	irq = (int)source;
	irqmask = 1 << irq;

	/* disable IRQ */
	rt305x_ic_set(IC_INT_DIS, irqmask);
}

static void 
obio_unmask_irq(void *source)
{
	int irq;
	uint32_t irqmask;

	irq = (int)source;
	irqmask = 1 << irq;

	/* enable IRQ */
	rt305x_ic_set(IC_INT_ENA, irqmask);

}


static int
obio_probe(device_t dev)
{

	return (BUS_PROBE_NOWILDCARD);
}

static int
obio_attach(device_t dev)
{
	struct obio_softc *sc = device_get_softc(dev);
	int rid;

	sc->oba_mem_rman.rm_type = RMAN_ARRAY;
	sc->oba_mem_rman.rm_descr = "OBIO memory";
	if (rman_init(&sc->oba_mem_rman) != 0 ||
	    rman_manage_region(&sc->oba_mem_rman, OBIO_MEM_START,
	        OBIO_MEM_END) != 0)
		panic("obio_attach: failed to set up I/O rman");

	sc->oba_irq_rman.rm_type = RMAN_ARRAY;
	sc->oba_irq_rman.rm_descr = "OBIO IRQ";
	if (rman_init(&sc->oba_irq_rman) != 0 ||
	    rman_manage_region(&sc->oba_irq_rman, 0, NIRQS-1) != 0)
		panic("obio_attach: failed to set up IRQ rman");

	/* Hook up our interrupt handler. */
	if ((sc->sc_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
	    RT305X_INTR, RT305X_INTR, 1,
	    RF_SHAREABLE | RF_ACTIVE)) == NULL) {
		device_printf(dev, "unable to allocate IRQ resource\n");
		return (ENXIO);
	}

	if ((bus_setup_intr(dev, sc->sc_irq, INTR_TYPE_MISC, obio_intr, NULL,
	    sc, &sc->sc_ih))) {
		device_printf(dev,
		    "WARNING: unable to register interrupt handler\n");
		return (ENXIO);
	}

	/* Hook up our FAST interrupt handler. */
	if ((sc->sc_fast_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
	    RT305X_FAST_INTR, RT305X_FAST_INTR, 1,
	    RF_SHAREABLE | RF_ACTIVE)) == NULL) {
		device_printf(dev, "unable to allocate IRQ resource\n");
		return (ENXIO);
	}

	if ((bus_setup_intr(dev, sc->sc_fast_irq, INTR_TYPE_MISC, obio_intr,
	    NULL, sc, &sc->sc_fast_ih))) {
		device_printf(dev,
		    "WARNING: unable to register interrupt handler\n");
		return (ENXIO);
	}

	/* disable all interrupts */
	rt305x_ic_set(IC_INT_DIS, IC_INT_MASK|IC_LINE_GLOBAL);

	bus_generic_probe(dev);

	obio_add_res_child(dev, "rt305x_sysctl", 0, 
	    SYSCTL_BASE, (SYSCTL_END - SYSCTL_BASE + 1),
	    IC_SYSCTL);
	obio_add_res_child(dev, "rt305x_ic", 0, 
	    INTCTL_BASE, (INTCTL_END - INTCTL_BASE + 1),
	    -1);
#ifdef notyet
	obio_add_res_child(dev, "timer",0, 
	    TIMER_BASE, (TIMER_END - TIMER_BASE  + 1),
	    IC_TIMER0);
	obio_add_res_child(dev, "rt305x_memc", 0,
	    MEMCTRL_BASE, (MEMCTRL_END - MEMCTRL_BASE + 1),
	    -1);
	obio_add_res_child(dev, "pcm", 	0, 
	    PCM_BASE, (PCM_END - PCM_BASE  + 1),
	    IC_PCM);
#endif
	obio_add_res_child(dev, "uart", 0, 
	    UART_BASE, (UART_END - UART_BASE + 1),
	    IC_UART);
	obio_add_res_child(dev, "gpio", 0, 
	    PIO_BASE, (PIO_END - PIO_BASE  + 1),
	    IC_PIO);
#ifdef notyet
	obio_add_res_child(dev, "rt305x_dma", 0,
	    GDMA_BASE, (GDMA_END - GDMA_BASE + 1),
	    IC_DMA);
	obio_add_res_child(dev, "rt305x_nandc", 0,
	    NANDFC_BASE, (NANDFC_END - NANDFC_BASE  + 1),
	    IC_NAND);
	obio_add_res_child(dev, "i2c", 	0,
	    I2C_BASE, (I2C_END - I2C_BASE  + 1),
	    -1);
	obio_add_res_child(dev, "i2s", 0,
	    I2S_BASE, (I2S_END - I2S_BASE  + 1),
	    IC_I2S);
#endif
	obio_add_res_child(dev, "spi", 0, 
	    SPI_BASE, (SPI_END - SPI_BASE  + 1),
	    -1);
	obio_add_res_child(dev, "uart", 1,
	    UARTLITE_BASE, (UARTLITE_END - UARTLITE_BASE + 1),
	    IC_UARTLITE);
#if !defined(RT5350) && !defined(MT7620)
	obio_add_res_child(dev, "cfi", 	0,
	    FLASH_BASE, (FLASH_END - FLASH_BASE  + 1),
	    -1);
	obio_add_res_child(dev, "dwcotg", 0,
	    USB_OTG_BASE, (USB_OTG_END - USB_OTG_BASE  + 1),
	    IC_OTG);
#else
	obio_add_res_child(dev, "ehci", 0,
	    USB_OTG_BASE, (USB_OTG_END - USB_OTG_BASE  + 1),
	    IC_OTG);
	obio_add_res_child(dev, "ohci", 0,
	    USB_OHCI_BASE, (USB_OHCI_END - USB_OHCI_BASE + 1),
	    IC_OTG);
#endif
	obio_add_res_child(dev, "switch", 0,
	    ETHSW_BASE, (ETHSW_END - ETHSW_BASE  + 1),
	    IC_ETHSW);

	bus_enumerate_hinted_children(dev);
	bus_generic_attach(dev);

	/* enable IC */
	rt305x_ic_set(IC_INT_ENA, IC_LINE_GLOBAL);

	return (0);
}

static struct resource *
obio_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct obio_softc		*sc = device_get_softc(bus);
	struct obio_ivar		*ivar = device_get_ivars(child);
	struct resource			*rv;
	struct resource_list_entry	*rle;
	struct rman			*rm;
	int				 isdefault, needactivate, passthrough;

	isdefault = (start == 0UL && end == ~0UL && count == 1);
	needactivate = flags & RF_ACTIVE;
	passthrough = (device_get_parent(child) != bus);
	rle = NULL;

	if (passthrough)
		return (BUS_ALLOC_RESOURCE(device_get_parent(bus), child, type,
		    rid, start, end, count, flags));

	/*
	 * If this is an allocation of the "default" range for a given RID,
	 * and we know what the resources for this device are (ie. they aren't
	 * maintained by a child bus), then work out the start/end values.
	 */
	if (isdefault) {
		rle = resource_list_find(&ivar->resources, type, *rid);
		if (rle == NULL)
			return (NULL);
		if (rle->res != NULL) {
			panic("%s: resource entry is busy", __func__);
		}
		start = rle->start;
		end = rle->end;
		count = rle->count;
	}

	switch (type) {
	case SYS_RES_IRQ:
		rm = &sc->oba_irq_rman;
		break;
	case SYS_RES_MEMORY:
		rm = &sc->oba_mem_rman;
		break;
	default:
		printf("%s: unknown resource type %d\n", __func__, type);
		return (0);
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == 0) {
		printf("%s: could not reserve resource\n", __func__);
		return (0);
	}

	rman_set_rid(rv, *rid);

	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			printf("%s: could not activate resource\n", __func__);
			rman_release_resource(rv);
			return (0);
		}
	}

	return (rv);
}

static int
obio_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	/*
	 * If this is a memory resource, track the direct mapping
	 * in the uncached MIPS KSEG1 segment.
	 */
	if (type == SYS_RES_MEMORY) {
		void *vaddr;

		vaddr = (void *)MIPS_PHYS_TO_KSEG1((intptr_t)rman_get_start(r));
		rman_set_virtual(r, vaddr);
		rman_set_bustag(r, mips_bus_space_generic);
		rman_set_bushandle(r, (bus_space_handle_t)vaddr);
	}

	return (rman_activate_resource(r));
}

static int
obio_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	return (rman_deactivate_resource(r));
}

static int
obio_release_resource(device_t dev, device_t child, int type,
    int rid, struct resource *r)
{
	struct resource_list *rl;
	struct resource_list_entry *rle;

	rl = obio_get_resource_list(dev, child);
	if (rl == NULL)
		return (EINVAL);
	rle = resource_list_find(rl, type, rid);
	if (rle == NULL)
		return (EINVAL);
	rman_release_resource(r);
	rle->res = NULL;

	return (0);
}

static int
obio_setup_intr(device_t dev, device_t child, struct resource *ires,
		int flags, driver_filter_t *filt, driver_intr_t *handler,
		void *arg, void **cookiep)
{
	struct obio_softc *sc = device_get_softc(dev);
	struct intr_event *event;
	int irq, error, priority;
	uint32_t irqmask;

	irq = rman_get_start(ires);

	if (irq >= NIRQS)
		panic("%s: bad irq %d", __func__, irq);

	event = sc->sc_eventstab[irq];
	if (event == NULL) {
		error = intr_event_create(&event, (void *)irq, 0, irq,
		    obio_mask_irq, obio_unmask_irq,
		    NULL, NULL, "obio intr%d:", irq);

		sc->sc_eventstab[irq] = event;
	}
	else
		panic("obio: Can't share IRQs");

	intr_event_add_handler(event, device_get_nameunit(child), filt,
	    handler, arg, intr_priority(flags), flags, cookiep);

	irqmask = 1 << irq;
	priority = irq_priorities[irq];

	if (priority == INTR_FIQ)
		rt305x_ic_set(IC_INTTYPE, rt305x_ic_get(IC_INTTYPE) | irqmask);
	else
		rt305x_ic_set(IC_INTTYPE, rt305x_ic_get(IC_INTTYPE) & ~irqmask);

	/* enable */
	obio_unmask_irq((void*)irq);

	return (0);
}

static int
obio_teardown_intr(device_t dev, device_t child, struct resource *ires,
    void *cookie)
{
	struct obio_softc *sc = device_get_softc(dev);
	int irq, result, priority;
	uint32_t irqmask;

	irq = rman_get_start(ires);
	if (irq >= NIRQS)
		panic("%s: bad irq %d", __func__, irq);

	if (sc->sc_eventstab[irq] == NULL)
		panic("Trying to teardown unoccupied IRQ");

	irqmask = (1 << irq);
	priority = irq_priorities[irq];

	if (priority == INTR_FIQ)
		rt305x_ic_set(IC_INTTYPE, rt305x_ic_get(IC_INTTYPE) & ~irqmask);
	else
		rt305x_ic_set(IC_INTTYPE, rt305x_ic_get(IC_INTTYPE) | irqmask);

	/* disable */
	obio_mask_irq((void*)irq);

	result = intr_event_remove_handler(cookie);
	if (!result) {
		sc->sc_eventstab[irq] = NULL;
	}

	return (result);
}

static int
obio_intr(void *arg)
{
	struct obio_softc *sc = arg;
	struct intr_event *event;
	uint32_t irqstat;
	int irq;

	irqstat = rt305x_ic_get(IC_IRQ0STAT);
	irqstat |= rt305x_ic_get(IC_IRQ1STAT);

	irq = 0;
	while (irqstat != 0) {
		if ((irqstat & 1) == 1) {
			event = sc->sc_eventstab[irq];
			if (!event || TAILQ_EMPTY(&event->ie_handlers))
				continue;

			/* TODO: pass frame as an argument*/
			/* TODO: log stray interrupt */
			intr_event_handle(event, NULL);
		}
		irq++;
		irqstat >>= 1;
	}

	return (FILTER_HANDLED);
}

static void
obio_add_res_child(device_t bus, const char *dname, int dunit, 
    long maddr, int msize, int irq)
{
	device_t		child;
	int			result;

	child = BUS_ADD_CHILD(bus, 0, dname, dunit);

	result = bus_set_resource(child, SYS_RES_MEMORY, 0,
	    maddr, msize);
	if (result != 0)
		device_printf(bus, "warning: bus_set_resource() failed\n");

	if (irq != -1) {
		result = bus_set_resource(child, SYS_RES_IRQ, 0, irq, 1);
		if (result != 0)
			device_printf(bus,
			    "warning: bus_set_resource() failed\n");
	}
}

static void
obio_hinted_child(device_t bus, const char *dname, int dunit)
{
	long			maddr;
	int			msize;
	int			irq;

	/*
	 * Set hard-wired resources for hinted child using
	 * specific RIDs.
	 */
	resource_long_value(dname, dunit, "maddr", &maddr);
	resource_int_value(dname, dunit, "msize", &msize);


	if (resource_int_value(dname, dunit, "irq", &irq) == 0) irq = -1;

	obio_add_res_child(bus, dname, dunit, maddr, msize, irq);
}

static device_t
obio_add_child(device_t bus, u_int order, const char *name, int unit)
{
	device_t		child;
	struct obio_ivar	*ivar;

	ivar = malloc(sizeof(struct obio_ivar), M_DEVBUF, M_WAITOK | M_ZERO);
	if (ivar == NULL) {
		printf("Failed to allocate ivar\n");
		return (0);
	}
	resource_list_init(&ivar->resources);

	child = device_add_child_ordered(bus, order, name, unit);
	if (child == NULL) {
		printf("Can't add child %s%d ordered\n", name, unit);
		return (0);
	}

	device_set_ivars(child, ivar);

	return (child);
}

/*
 * Helper routine for bus_generic_rl_get_resource/bus_generic_rl_set_resource
 * Provides pointer to resource_list for these routines
 */
static struct resource_list *
obio_get_resource_list(device_t dev, device_t child)
{
	struct obio_ivar *ivar;

	ivar = device_get_ivars(child);
	return (&(ivar->resources));
}

static int
obio_print_all_resources(device_t dev)
{
	struct obio_ivar *ivar = device_get_ivars(dev);
	struct resource_list *rl = &ivar->resources;
	int retval = 0;

	if (STAILQ_FIRST(rl))
		retval += printf(" at");

	retval += resource_list_print_type(rl, "mem", SYS_RES_MEMORY, "%#lx");
	retval += resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%ld");

	return (retval);
}

static int
obio_print_child(device_t bus, device_t child)
{
	int retval = 0;

	retval += bus_print_child_header(bus, child);
	retval += obio_print_all_resources(child);
	if (device_get_flags(child))
		retval += printf(" flags %#x", device_get_flags(child));
	retval += printf(" on %s\n", device_get_nameunit(bus));

	return (retval);
}

static device_method_t obio_methods[] = {
	DEVMETHOD(bus_activate_resource,	obio_activate_resource),
	DEVMETHOD(bus_add_child,		obio_add_child),
	DEVMETHOD(bus_alloc_resource,		obio_alloc_resource),
	DEVMETHOD(bus_deactivate_resource,	obio_deactivate_resource),
	DEVMETHOD(bus_get_resource_list,	obio_get_resource_list),
	DEVMETHOD(bus_hinted_child,		obio_hinted_child),
	DEVMETHOD(bus_print_child,		obio_print_child),
	DEVMETHOD(bus_release_resource,		obio_release_resource),
	DEVMETHOD(bus_setup_intr,		obio_setup_intr),
	DEVMETHOD(bus_teardown_intr,		obio_teardown_intr),
	DEVMETHOD(device_attach,		obio_attach),
	DEVMETHOD(device_probe,			obio_probe),
        DEVMETHOD(bus_get_resource,		bus_generic_rl_get_resource),
        DEVMETHOD(bus_set_resource,		bus_generic_rl_set_resource),

	{0, 0},
};

static driver_t obio_driver = {
	"obio",
	obio_methods,
	sizeof(struct obio_softc),
};
static devclass_t obio_devclass;

DRIVER_MODULE(obio, nexus, obio_driver, obio_devclass, 0, 0);
