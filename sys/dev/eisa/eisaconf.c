/*
 * EISA bus probe and attach routines 
 *
 * Copyright (c) 1995, 1996 Justin T. Gibbs.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND  
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/eisa/eisaconf.c,v 1.55 2000/01/14 07:13:57 peter Exp $
 */

#include "opt_eisa.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/limits.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/eisa/eisaconf.h>

typedef struct resvaddr {
        u_long	addr;				/* start address */
        u_long	size;				/* size of reserved area */
	int	flags;
	struct resource *res;			/* resource manager handle */
	LIST_ENTRY(resvaddr) links;		/* List links */
} resvaddr_t;

LIST_HEAD(resvlist, resvaddr);

struct irq_node {
	int	irq_no;
	int	irq_trigger;
	void	*idesc;
	TAILQ_ENTRY(irq_node) links;
};

TAILQ_HEAD(irqlist, irq_node);

struct eisa_ioconf {
	int		slot;
	struct resvlist	ioaddrs;	/* list of reserved I/O ranges */
	struct resvlist maddrs;		/* list of reserved memory ranges */
	struct irqlist	irqs;		/* list of reserved irqs */
};

/* To be replaced by the "super device" generic device structure... */
struct eisa_device {
	eisa_id_t		id;
	struct eisa_ioconf	ioconf;
};


/* Global variable, so UserConfig can change it. */
#define MAX_COL		79
#ifndef EISA_SLOTS
#define EISA_SLOTS 10   /* PCI clashes with higher ones.. fix later */
#endif
int num_eisa_slots = EISA_SLOTS;

static devclass_t eisa_devclass;

static void eisa_reg_print (device_t, char *, char *, int *);
static struct irq_node * eisa_find_irq(struct eisa_device *e_dev, int rid);
static struct resvaddr * eisa_find_maddr(struct eisa_device *e_dev, int rid);
static struct resvaddr * eisa_find_ioaddr(struct eisa_device *e_dev, int rid);

static int
mainboard_probe(device_t dev)
{
	char *idstring;
	eisa_id_t id = eisa_get_id(dev);

	if (eisa_get_slot(dev) != 0)
		return (ENXIO);

	idstring = (char *)malloc(8 + sizeof(" (System Board)") + 1,
				  M_DEVBUF, M_NOWAIT);
	if (idstring == NULL) {
		panic("Eisa probe unable to malloc");
	}
	sprintf(idstring, "%c%c%c%03x%01x (System Board)",
		EISA_MFCTR_CHAR0(id),
		EISA_MFCTR_CHAR1(id),
		EISA_MFCTR_CHAR2(id),
		EISA_PRODUCT_ID(id),
		EISA_REVISION_ID(id));
	device_set_desc(dev, idstring);

	return (0);
}

static int
mainboard_attach(device_t dev)
{
	return (0);
}

static device_method_t mainboard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mainboard_probe),
	DEVMETHOD(device_attach,	mainboard_attach),

	{ 0, 0 }
};

static driver_t mainboard_driver = {
	"mainboard",
	mainboard_methods,
	1,
};

static devclass_t mainboard_devclass;

DRIVER_MODULE(mainboard, eisa, mainboard_driver, mainboard_devclass, 0, 0);
		
/*
** probe for EISA devices
*/
static int
eisa_probe(device_t dev)
{
	int i,slot;
	struct eisa_device *e_dev;
	device_t child;
	int eisaBase = 0xc80;
	eisa_id_t eisa_id;
	int devices_found = 0;

	device_set_desc(dev, "EISA bus");

	for (slot = 0; slot < num_eisa_slots; eisaBase+=0x1000, slot++) {
		int id_size = sizeof(eisa_id);
		eisa_id = 0;
    		for( i = 0; i < id_size; i++ ) {
			outb(eisaBase,0x80 + i); /*Some cards require priming*/
			eisa_id |= inb(eisaBase+i) << ((id_size-i-1)*CHAR_BIT);
		}
		if (eisa_id & 0x80000000)
			continue;  /* no EISA card in slot */

		devices_found++;

		/* Prepare an eisa_device_node for this slot */
		e_dev = (struct eisa_device *)malloc(sizeof(*e_dev),
						     M_DEVBUF, M_NOWAIT);
		if (!e_dev) {
			device_printf(dev, "cannot malloc eisa_device");
			break; /* Try to attach what we have already */
		}
		bzero(e_dev, sizeof(*e_dev));

		e_dev->id = eisa_id;

		e_dev->ioconf.slot = slot; 

		/* Initialize our lists of reserved addresses */
		LIST_INIT(&(e_dev->ioconf.ioaddrs));
		LIST_INIT(&(e_dev->ioconf.maddrs));
		TAILQ_INIT(&(e_dev->ioconf.irqs));

		child = device_add_child(dev, NULL, -1);
		device_set_ivars(child, e_dev);
	}

	/*
	 * EISA busses themselves are not easily detectable, the easiest way
	 * to tell if there is an eisa bus is if we found something - there
	 * should be a motherboard "card" there somewhere.
	 */
	return devices_found ? 0 : ENXIO;
}

static void
eisa_probe_nomatch(device_t dev, device_t child)
{
	u_int32_t	eisa_id = eisa_get_id(child);
	u_int8_t	slot = eisa_get_slot(child);

	device_printf(dev, "unknown card %c%c%c%03x%01x (0x%08x) at slot %d\n",
		EISA_MFCTR_CHAR0(eisa_id),
		EISA_MFCTR_CHAR1(eisa_id),
		EISA_MFCTR_CHAR2(eisa_id),
		EISA_PRODUCT_ID(eisa_id),
		EISA_REVISION_ID(eisa_id),
		eisa_id,
		slot);

	return;
}

static void
eisa_reg_print (dev, string, separator, column)
	device_t	dev;
	char *		string;
	char *		separator;
	int *		column;
{
	int length = strlen(string);

	length += (separator ? 2 : 1);

	if (((*column) + length) >= MAX_COL) {
		printf("\n");
		(*column) = 0;
	} else if ((*column) != 0) {
		if (separator) {
			printf("%c", *separator);
			(*column)++;
		}
		printf(" ");
		(*column)++;
	}

	if ((*column) == 0) {
		(*column) += device_printf(dev, "%s", string);
	} else {
		(*column) += printf("%s", string);
	}

	return;
}

static int
eisa_print_child(device_t dev, device_t child)
{
	char			buf[81];
	struct eisa_device *	e_dev = device_get_ivars(child);
	int			rid;
	struct irq_node *	irq;
	struct resvaddr *	resv;
	char			separator = ',';
	int			column = 0;
	int			retval = 0;

	if (device_get_desc(child)) {
		snprintf(buf, sizeof(buf), "<%s>", device_get_desc(child));
		eisa_reg_print(child, buf, NULL, &column);
	}

	rid = 0;
	while ((resv = eisa_find_ioaddr(e_dev, rid++))) {
		if ((resv->size == 1) ||
		    (resv->flags & RESVADDR_BITMASK)) {
			snprintf(buf, sizeof(buf), "%s%lx",
				((rid == 1) ? "at 0x" : "0x"),
				resv->addr);
		} else {
			snprintf(buf, sizeof(buf), "%s%lx-0x%lx",
				((rid == 1) ? "at 0x" : "0x"),
				resv->addr,
				(resv->addr + (resv->size - 1)));
		}
		eisa_reg_print(child, buf, 
			((rid == 2) ? &separator : NULL), &column);
	}

	rid = 0;
	while ((resv = eisa_find_maddr(e_dev, rid++))) {
		if ((resv->size == 1) ||
		    (resv->flags & RESVADDR_BITMASK)) {
			snprintf(buf, sizeof(buf), "%s%lx",
				((rid == 1) ? "at 0x" : "0x"),
				resv->addr);
		} else {
			snprintf(buf, sizeof(buf), "%s%lx-0x%lx",
				((rid == 1) ? "at 0x" : "0x"),
				resv->addr,
				(resv->addr + (resv->size - 1)));
		}
		eisa_reg_print(child, buf, 
			((rid == 2) ? &separator : NULL), &column);
	}

	rid = 0;
	while ((irq = eisa_find_irq(e_dev, rid++)) != NULL) {
		snprintf(buf, sizeof(buf), "irq %d (%s)", irq->irq_no,
			 (irq->irq_trigger ? "level" : "edge"));
		eisa_reg_print(child, buf, 
			((rid == 1) ? &separator : NULL), &column);
	}

	snprintf(buf, sizeof(buf), "on %s slot %d\n",
		device_get_nameunit(dev), eisa_get_slot(child));
	eisa_reg_print(child, buf, NULL, &column);

	return (retval);
}

static struct irq_node *
eisa_find_irq(struct eisa_device *e_dev, int rid)
{
	int i;
	struct irq_node *irq;

	for (i = 0, irq = TAILQ_FIRST(&e_dev->ioconf.irqs);
	     i < rid && irq;
	     i++, irq = TAILQ_NEXT(irq, links))
		;
	
	if (irq)
		return (irq);
	else
		return (NULL);
}

static struct resvaddr *
eisa_find_maddr(struct eisa_device *e_dev, int rid)
{
	int i;
	struct resvaddr *resv;

	for (i = 0, resv = LIST_FIRST(&e_dev->ioconf.maddrs);
	     i < rid && resv;
	     i++, resv = LIST_NEXT(resv, links))
		;

	return resv;
}

static struct resvaddr *
eisa_find_ioaddr(struct eisa_device *e_dev, int rid)
{
	int i;
	struct resvaddr *resv;

	for (i = 0, resv = LIST_FIRST(&e_dev->ioconf.ioaddrs);
	     i < rid && resv;
	     i++, resv = LIST_NEXT(resv, links))
		;

	return resv;
}

static int
eisa_read_ivar(device_t dev, device_t child, int which, u_long *result)
{
	struct eisa_device *e_dev = device_get_ivars(child);
	struct irq_node *irq;

	switch (which) {
	case EISA_IVAR_SLOT:
		*result = e_dev->ioconf.slot;
		break;
		
	case EISA_IVAR_ID:
		*result = e_dev->id;
		break;

	case EISA_IVAR_IRQ:
		/* XXX only first irq */
		if ((irq = eisa_find_irq(e_dev, 0)) != NULL) {
			*result = irq->irq_no;
		} else {
			*result = -1;
		}
		break;

	default:
		return (ENOENT);
	}

	return (0);
}

static int
eisa_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	return (EINVAL);
}

static struct resource *
eisa_alloc_resource(device_t dev, device_t child, int type, int *rid,
		    u_long start, u_long end, u_long count, u_int flags)
{
	int isdefault;
	struct eisa_device *e_dev = device_get_ivars(child);
	struct resource *rv, **rvp = 0;

	isdefault = (device_get_parent(child) == dev
		     && start == 0UL && end == ~0UL && count == 1);

	switch (type) {
	case SYS_RES_IRQ:
		if (isdefault) {
			struct irq_node * irq = eisa_find_irq(e_dev, *rid);
			if (irq == NULL)
				return 0;
			start = end = irq->irq_no;
			count = 1;
			if (irq->irq_trigger == EISA_TRIGGER_LEVEL) {
				flags |= RF_SHAREABLE;
			} else {
				flags &= ~RF_SHAREABLE;
			}
		}
		break;

	case SYS_RES_MEMORY:
		if (isdefault) {
			struct resvaddr *resv;

			resv = eisa_find_maddr(e_dev, *rid);
			if (!resv)
				return 0;

			start = resv->addr;
			end = resv->addr + (resv->size - 1);
			count = resv->size;
			rvp = &resv->res;
		}
		break;

	case SYS_RES_IOPORT:
		if (isdefault) {
			struct resvaddr *resv;

			resv = eisa_find_ioaddr(e_dev, *rid);
			if (!resv)
				return 0;

			start = resv->addr;
			end = resv->addr + (resv->size - 1);
			count = resv->size;
			rvp = &resv->res;
		}
		break;

	default:
		return 0;
	}

	rv = BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
				 type, rid, start, end, count, flags);
	if (rvp)
		*rvp = rv;

	return rv;
}

static int
eisa_release_resource(device_t dev, device_t child, int type, int rid,
		      struct resource *r)
{
	int rv;
	struct eisa_device *e_dev = device_get_ivars(child);
	struct resvaddr *resv = 0;

	switch (type) {
	case SYS_RES_IRQ:
		if (eisa_find_irq(e_dev, rid) == NULL)
			return EINVAL;
		break;

	case SYS_RES_MEMORY:
		if (device_get_parent(child) == dev)
			resv = eisa_find_maddr(e_dev, rid);
		break;


	case SYS_RES_IOPORT:
		if (device_get_parent(child) == dev)
			resv = eisa_find_ioaddr(e_dev, rid);
		break;

	default:
		return (ENOENT);
	}

	rv = BUS_RELEASE_RESOURCE(device_get_parent(dev), child, type, rid, r);

	if (rv == 0) {
		if (resv)
			resv->res = 0;
	}

	return rv;
}

int
eisa_add_intr(device_t dev, int irq, int trigger)
{
	struct eisa_device *e_dev = device_get_ivars(dev);
	struct irq_node *irq_info;
 
	irq_info = (struct irq_node *)malloc(sizeof(*irq_info), M_DEVBUF,
					     M_NOWAIT);
	if (irq_info == NULL)
		return (1);

	irq_info->irq_no = irq;
	irq_info->irq_trigger = trigger;
	irq_info->idesc = NULL;
	TAILQ_INSERT_TAIL(&e_dev->ioconf.irqs, irq_info, links);
	return 0;
}

static int
eisa_add_resvaddr(struct eisa_device *e_dev, struct resvlist *head, u_long base,
		  u_long size, int flags)
{
	resvaddr_t *reservation;

	reservation = (resvaddr_t *)malloc(sizeof(resvaddr_t),
					   M_DEVBUF, M_NOWAIT);
	if(!reservation)
		return (ENOMEM);

	reservation->addr = base;
	reservation->size = size;
	reservation->flags = flags;

	if (!head->lh_first) {
		LIST_INSERT_HEAD(head, reservation, links);
	}
	else {
		resvaddr_t *node;
		for(node = head->lh_first; node; node = node->links.le_next) {
			if (node->addr > reservation->addr) {
				/*
				 * List is sorted in increasing
				 * address order.
				 */
				LIST_INSERT_BEFORE(node, reservation, links);
				break;
			}

			if (node->addr == reservation->addr) {
				/*
				 * If the entry we want to add
				 * matches any already in here,
				 * fail.
				 */
				free(reservation, M_DEVBUF);
				return (EEXIST);
			}

			if (!node->links.le_next) {
				LIST_INSERT_AFTER(node, reservation, links);
				break;
			}
		}
	}
	return (0);
}

int
eisa_add_mspace(device_t dev, u_long mbase, u_long msize, int flags)
{
	struct eisa_device *e_dev = device_get_ivars(dev);

	return	eisa_add_resvaddr(e_dev, &(e_dev->ioconf.maddrs), mbase, msize,
				  flags);
}

int
eisa_add_iospace(device_t dev, u_long iobase, u_long iosize, int flags)
{
	struct eisa_device *e_dev = device_get_ivars(dev);

	return	eisa_add_resvaddr(e_dev, &(e_dev->ioconf.ioaddrs), iobase,
				  iosize, flags);
}

static device_method_t eisa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		eisa_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	eisa_print_child),
	DEVMETHOD(bus_probe_nomatch,	eisa_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	eisa_read_ivar),
	DEVMETHOD(bus_write_ivar,	eisa_write_ivar),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	DEVMETHOD(bus_alloc_resource,	eisa_alloc_resource),
	DEVMETHOD(bus_release_resource,	eisa_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	{ 0, 0 }
};

static driver_t eisa_driver = {
	"eisa",
	eisa_methods,
	1,			/* no softc */
};

DRIVER_MODULE(eisa, isab, eisa_driver, eisa_devclass, 0, 0);
DRIVER_MODULE(eisa, nexus, eisa_driver, eisa_devclass, 0, 0);
