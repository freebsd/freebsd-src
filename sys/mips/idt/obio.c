/*-
 * Copyright (c) 2007, Oleksandr Tymoshenko <gonzo@freebsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
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

#include <mips/idt/idtreg.h>
#include <mips/idt/obiovar.h>

#define ICU_REG_READ(o) \
    *((volatile uint32_t *)MIPS_PHYS_TO_KSEG1(IDT_BASE_ICU + (o)))
#define ICU_REG_WRITE(o,v) (ICU_REG_READ(o)) = (v)

#define GPIO_REG_READ(o) \
    *((volatile uint32_t *)MIPS_PHYS_TO_KSEG1(IDT_BASE_GPIO + (o)))
#define GPIO_REG_WRITE(o,v) (GPIO_REG_READ(o)) = (v)

static int	obio_activate_resource(device_t, device_t, int, int,
		    struct resource *);
static device_t	obio_add_child(device_t, int, const char *, int);
static struct resource *
		obio_alloc_resource(device_t, device_t, int, int *, u_long,
		    u_long, u_long, u_int);
static int	obio_attach(device_t);
static int	obio_deactivate_resource(device_t, device_t, int, int,
		    struct resource *);
static struct resource_list *
		obio_get_resource_list(device_t, device_t);
static void	obio_hinted_child(device_t, const char *, int);
static int	obio_intr(void *);
static int	obio_probe(device_t);
static int	obio_release_resource(device_t, device_t, int, int,
		    struct resource *);
static int	obio_setup_intr(device_t, device_t, struct resource *, int,
		    driver_filter_t *, driver_intr_t *, void *, void **);
static int	obio_teardown_intr(device_t, device_t, struct resource *,
		    void *);

static void obio_mask_irq(unsigned int irq)
{
	int ip_bit, mask, mask_register;

	/* mask IRQ */
	mask_register = ICU_IRQ_MASK_REG(irq);
	ip_bit = ICU_IP_BIT(irq);

	mask = ICU_REG_READ(mask_register);
	ICU_REG_WRITE(mask_register, mask | ip_bit);
}

static void obio_unmask_irq(unsigned int irq)
{
	int ip_bit, mask, mask_register;

	/* unmask IRQ */
	mask_register = ICU_IRQ_MASK_REG(irq);
	ip_bit = ICU_IP_BIT(irq);

	mask = ICU_REG_READ(mask_register);
	ICU_REG_WRITE(mask_register, mask & ~ip_bit);
}

static int
obio_probe(device_t dev)
{

	return (0);
}

static int
obio_attach(device_t dev)
{
	struct obio_softc *sc = device_get_softc(dev);
	int rid, irq;

	sc->oba_mem_rman.rm_type = RMAN_ARRAY;
	sc->oba_mem_rman.rm_descr = "OBIO memeory";
	if (rman_init(&sc->oba_mem_rman) != 0 ||
	    rman_manage_region(&sc->oba_mem_rman, OBIO_MEM_START,
	        OBIO_MEM_START + OBIO_MEM_SIZE) != 0)
		panic("obio_attach: failed to set up I/O rman");

	sc->oba_irq_rman.rm_type = RMAN_ARRAY;
	sc->oba_irq_rman.rm_descr = "OBIO IRQ";

	if (rman_init(&sc->oba_irq_rman) != 0 ||
	    rman_manage_region(&sc->oba_irq_rman, IRQ_BASE, IRQ_END) != 0)
		panic("obio_attach: failed to set up IRQ rman");

	/* Hook up our interrupt handlers. We should handle IRQ0..IRQ4*/
	for(irq = 0; irq < 5; irq++) {
		if ((sc->sc_irq[irq] = bus_alloc_resource(dev, SYS_RES_IRQ, 
		    &rid, irq, irq, 1, RF_SHAREABLE | RF_ACTIVE)) == NULL) {
			device_printf(dev, "unable to allocate IRQ resource\n");
			return (ENXIO);
		}

		if ((bus_setup_intr(dev, sc->sc_irq[irq], INTR_TYPE_MISC, 
		    obio_intr, NULL, sc, &sc->sc_ih[irq]))) {
			device_printf(dev,
			    "WARNING: unable to register interrupt handler\n");
			return (ENXIO);
		}
	}

	bus_generic_probe(dev);
	bus_enumerate_hinted_children(dev);
	bus_generic_attach(dev);

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

	isdefault = (start == 0UL && end == ~0UL);
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

	/* XXX: should we mask/unmask IRQ here? */
	return (BUS_ACTIVATE_RESOURCE(device_get_parent(bus), child,
		type, rid, r));
}

static int
obio_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	/* XXX: should we mask/unmask IRQ here? */
	return (BUS_DEACTIVATE_RESOURCE(device_get_parent(bus), child,
		type, rid, r));
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
	int irq, ip_bit, error, mask, mask_register;

	irq = rman_get_start(ires);

	if (irq >= NIRQS)
		panic("%s: bad irq %d", __func__, irq);

	event = sc->sc_eventstab[irq];
	if (event == NULL) {
		error = intr_event_create(&event, (void *)irq, 0,
		    (mask_fn)obio_mask_irq, (mask_fn)obio_unmask_irq,
		    NULL, NULL,
		    "obio intr%d:", irq);

		sc->sc_eventstab[irq] = event;
	}

	intr_event_add_handler(event, device_get_nameunit(child), filt,
	    handler, arg, intr_priority(flags), flags, cookiep);

	/* unmask IRQ */
	mask_register = ICU_IRQ_MASK_REG(irq);
	ip_bit = ICU_IP_BIT(irq);

	mask = ICU_REG_READ(mask_register);
	ICU_REG_WRITE(mask_register, mask & ~ip_bit);

	return (0);
}

static int
obio_teardown_intr(device_t dev, device_t child, struct resource *ires,
    void *cookie)
{
	struct obio_softc *sc = device_get_softc(dev);
	int irq, result;
	uint32_t mask_register, mask, ip_bit;

	irq = rman_get_start(ires);
	if (irq >= NIRQS)
		panic("%s: bad irq %d", __func__, irq);

	if (sc->sc_eventstab[irq] == NULL)
		panic("Trying to teardown unoccupied IRQ");

	/* mask IRQ */
	mask_register = ICU_IRQ_MASK_REG(irq);
	ip_bit = ICU_IP_BIT(irq);

	mask = ICU_REG_READ(mask_register);
	ICU_REG_WRITE(mask_register, mask | ip_bit);

	result = intr_event_remove_handler(cookie);
	if (!result)
		sc->sc_eventstab[irq] = NULL;

	return (result);
}

static int
obio_intr(void *arg)
{
	struct obio_softc *sc = arg;
	struct intr_event *event;
	struct intr_handler *ih;
	uint32_t irqstat, ipend, imask, xpend;
	int irq, thread, group, i, ret;

	irqstat = 0;
	irq = 0;
	for (group = 2; group <= 6; group++) {
		ipend = ICU_REG_READ(ICU_GROUP_IPEND_REG(group));
		imask = ICU_REG_READ(ICU_GROUP_MASK_REG(group));
		xpend = ipend;
		ipend &= ~imask;

		while ((i = fls(xpend)) != 0) {
			xpend &= ~(1 << (i - 1));
			irq = IP_IRQ(group, i - 1);
		}
	
		while ((i = fls(ipend)) != 0) {
			ipend &= ~(1 << (i - 1));
			irq = IP_IRQ(group, i - 1);
			event = sc->sc_eventstab[irq];
			thread = 0;
#ifndef INTR_FILTER
			obio_mask_irq(irq);
#endif
			if (!event || TAILQ_EMPTY(&event->ie_handlers)) {
#ifdef INTR_FILTER
				obio_unmask_irq(irq);
#endif
				continue;
			}

#ifdef INTR_FILTER
			/* TODO: frame instead of NULL? */
			intr_event_handle(event, NULL);
			/* XXX: Log stray IRQs */
#else
			/* Execute fast handlers. */
			TAILQ_FOREACH(ih, &event->ie_handlers,
			    ih_next) {
				if (ih->ih_filter == NULL)
					thread = 1;
				else
					ret = ih->ih_filter(ih->ih_argument);
				/*
				 * Wrapper handler special case: see
				 * intr_execute_handlers() in
				 * i386/intr_machdep.c
				 */
				if (!thread) {
					if (ret == FILTER_SCHEDULE_THREAD)
						thread = 1;
				}
			}

			/* Schedule thread if needed. */
			if (thread)
				intr_event_schedule_thread(event);
			else
				obio_unmask_irq(irq);
		}
	}
#endif
#if 0
	ipend = ICU_REG_READ(ICU_IPEND2);
	printf("ipend2 = %08x!\n", ipend);

	ipend = ICU_REG_READ(ICU_IPEND3);
	printf("ipend3 = %08x!\n", ipend);

	ipend = ICU_REG_READ(ICU_IPEND4);
	printf("ipend4 = %08x!\n", ipend);
	ipend = ICU_REG_READ(ICU_IPEND5);
	printf("ipend5 = %08x!\n", ipend);

	ipend = ICU_REG_READ(ICU_IPEND6);
	printf("ipend6 = %08x!\n", ipend);
#endif
	while (irqstat != 0) {
		if ((irqstat & 1) == 1) {
		}

		irq++;
		irqstat >>= 1;
	}

	return (FILTER_HANDLED);
}

static void
obio_hinted_child(device_t bus, const char *dname, int dunit)
{
	device_t		child;
	long			maddr;
	int			msize;
	int			irq;
	int			result;

	child = BUS_ADD_CHILD(bus, 0, dname, dunit);

	/*
	 * Set hard-wired resources for hinted child using
	 * specific RIDs.
	 */
	resource_long_value(dname, dunit, "maddr", &maddr);
	resource_int_value(dname, dunit, "msize", &msize);


	result = bus_set_resource(child, SYS_RES_MEMORY, 0,
	    maddr, msize);
	if (result != 0)
		device_printf(bus, "warning: bus_set_resource() failed\n");

	if (resource_int_value(dname, dunit, "irq", &irq) == 0) {
		result = bus_set_resource(child, SYS_RES_IRQ, 0, irq, 1);
		if (result != 0)
			device_printf(bus,
			    "warning: bus_set_resource() failed\n");
	}
}

static device_t
obio_add_child(device_t bus, int order, const char *name, int unit)
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

static device_method_t obio_methods[] = {
	DEVMETHOD(bus_activate_resource,	obio_activate_resource),
	DEVMETHOD(bus_add_child,		obio_add_child),
	DEVMETHOD(bus_alloc_resource,		obio_alloc_resource),
	DEVMETHOD(bus_deactivate_resource,	obio_deactivate_resource),
	DEVMETHOD(bus_get_resource_list,	obio_get_resource_list),
	DEVMETHOD(bus_hinted_child,		obio_hinted_child),
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
