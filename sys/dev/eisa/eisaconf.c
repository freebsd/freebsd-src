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
 *	$Id: eisaconf.c,v 1.39 1999/04/19 06:57:33 peter Exp $
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

#include <i386/eisa/eisaconf.h>

#include <sys/interrupt.h>

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


/*
 * Local function declarations and static variables
 */
#if 0
static void eisa_reg_print __P((struct eisa_device *e_dev,
				char *string, char *separator));
static int eisa_add_resvaddr __P((struct eisa_device *e_dev,
				  struct resvlist *head, u_long	base,
				  u_long size, int flags));
static int eisa_reg_resvaddr __P((struct eisa_device *e_dev, 
				  struct resvlist *head, resvaddr_t *resvaddr,
				  int *reg_count));
#endif

#if 0
/*
 * Keep some state about what we've printed so far
 * to make probe output pretty.
 */
static struct {
	int	in_registration;/* reg_start has been called */
	int	num_interrupts;	
	int	num_ioaddrs;
	int	num_maddrs;
	int	column;		/* How much we have output so far. */
#define	MAX_COL 80
} reg_state;
#endif

/* Global variable, so UserConfig can change it. */
#ifndef EISA_SLOTS
#define EISA_SLOTS 10   /* PCI clashes with higher ones.. fix later */
#endif
int num_eisa_slots = EISA_SLOTS;

static devclass_t eisa_devclass;

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
	sprintf(idstring, "%c%c%c%x%x (System Board)",
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
	DRIVER_TYPE_MISC,
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
	int eisaBase = 0xc80;
	eisa_id_t eisa_id;

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

		/* Prepare an eisa_device_node for this slot */
		e_dev = (struct eisa_device *)malloc(sizeof(*e_dev),
						     M_DEVBUF, M_NOWAIT);
		if (!e_dev) {
			printf("eisa0: cannot malloc eisa_device");
			break; /* Try to attach what we have already */
		}
		bzero(e_dev, sizeof(*e_dev));

		e_dev->id = eisa_id;

		e_dev->ioconf.slot = slot; 

		/* Initialize our lists of reserved addresses */
		LIST_INIT(&(e_dev->ioconf.ioaddrs));
		LIST_INIT(&(e_dev->ioconf.maddrs));
		TAILQ_INIT(&(e_dev->ioconf.irqs));

		device_add_child(dev, NULL, -1, e_dev);
	}

	return 0;
}

static void
eisa_print_child(device_t dev, device_t child)
{
	/* XXX print resource descriptions? */
	printf(" at slot %d", eisa_get_slot(child));
	printf(" on %s", device_get_nameunit(dev));
}

static int
eisa_find_irq(struct eisa_device *e_dev, int rid)
{
	int i;
	struct irq_node *irq;

	for (i = 0, irq = TAILQ_FIRST(&e_dev->ioconf.irqs);
	     i < rid && irq;
	     i++, irq = TAILQ_NEXT(irq, links))
		;
	
	if (irq)
		return irq->irq_no;
	else
		return -1;
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

	switch (which) {
	case EISA_IVAR_SLOT:
		*result = e_dev->ioconf.slot;
		break;
		
	case EISA_IVAR_ID:
		*result = e_dev->id;
		break;

	case EISA_IVAR_IRQ:
		/* XXX only first irq */
		*result = eisa_find_irq(e_dev, 0);
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
			int irq = eisa_find_irq(e_dev, *rid);
			if (irq == -1)
				return 0;
			start = end = irq;
			count = 1;
		}
		break;

	case SYS_RES_MEMORY:
		if (isdefault) {
			struct resvaddr *resv;

			resv = eisa_find_maddr(e_dev, *rid);
			if (!resv)
				return 0;

			start = resv->addr;
			end = resv->size - 1;
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
			end = resv->size - 1;
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
		if (eisa_find_irq(e_dev, rid) == -1)
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

#if 0

/* Interrupt and I/O space registration facitlities */
void
eisa_reg_start(e_dev)
	struct eisa_device *e_dev;
{
	/*
	 * Announce the device.
	 */
	char *string;

	reg_state.in_registration = 1;
	reg_state.num_interrupts = 0;
	reg_state.num_ioaddrs = 0;
	reg_state.num_maddrs = 0;
	reg_state.column = 0;

	string = malloc(strlen(e_dev->full_name) + sizeof(" <>") + /*NULL*/1,
			M_TEMP, M_NOWAIT);
	if(!string) {
		printf("eisa0: cannot malloc device description string\n");
		return;
	}
	sprintf(string, " <%s>", e_dev->full_name);
	eisa_reg_print(e_dev, string, /*separator=*/NULL);
	free(string, M_TEMP);
}

/*
 * Output registration information mindfull of screen wrap.
 * Output an optional character separator before the string
 * if the line does not wrap.
 */
static void
eisa_reg_print(e_dev, string, separator)
	struct eisa_device *e_dev;
	char *string;
	char *separator;
{
	int len = strlen(string);

	if(separator)
		len++;

	if(reg_state.column + len > MAX_COL) {
		printf("\n");
		reg_state.column = 0;
	}
	else if(separator) {
		printf("%c", *separator);
		reg_state.column++;
	}

	if(reg_state.column == 0)
		reg_state.column += printf("%s%ld:%s",
					   e_dev->driver->name,
					   e_dev->unit,
					   string);
	else
		reg_state.column += printf("%s", string);
}

/* Interrupt and I/O space registration facitlities */
void
eisa_reg_end(e_dev)
	struct eisa_device *e_dev;
{
	if( reg_state.in_registration )
	{
		char string[25];

		snprintf(string, sizeof(string), " on %s0 slot %d",
			mainboard_drv.name,
			e_dev->ioconf.slot);
		eisa_reg_print(e_dev, string, NULL);
		printf("\n");
		reg_state.in_registration = 0;
	}
	else
		printf("eisa_reg_end called outside of a "
		       "registration session\n");
}

#endif /* 0 */

int
eisa_add_intr(dev, irq)
	device_t dev;
	int irq;
{
	struct eisa_device *e_dev = device_get_ivars(dev);
	struct	irq_node *irq_info;
 
	irq_info = (struct irq_node *)malloc(sizeof(*irq_info), M_DEVBUF,
					     M_NOWAIT);
	if (irq_info == NULL)
		return (1);

	irq_info->irq_no = irq;
	irq_info->idesc = NULL;
	TAILQ_INSERT_TAIL(&e_dev->ioconf.irqs, irq_info, links);
	return 0;
}

#if 0

int
eisa_reg_intr(e_dev, irq, func, arg, maskptr, shared)
	struct eisa_device *e_dev;
	int   irq;
	void (*func)(void *);
	void  *arg;
	u_int *maskptr;
	int   shared;
{
	char string[25];
	char separator = ',';

#if NOT_YET
	/* 
	 * Punt on conflict detection for the moment.
	 * I want to develop a generic routine to do
	 * this for all device types.
	 */
	int checkthese = CC_IRQ;
	if (haveseen_dev(dev, checkthese))
        	return 1;
#endif
	if (reg_state.in_registration) {
		/*
		 * Find the first instance of this irq that has a
		 * NULL idesc.
		 */
		struct irq_node *cur_irq;

		cur_irq = TAILQ_FIRST(&e_dev->ioconf.irqs);
		while (cur_irq != NULL) {
			if (cur_irq->irq_no == irq
			 && cur_irq->idesc == NULL) {
				/* XXX use cfg->devdata  */
                		void *dev_instance = (void *)-1;

				cur_irq->idesc = intr_create(dev_instance,
							     irq,
							     func,
							     arg,
							     maskptr, 0);
				break;
			}
			cur_irq = TAILQ_NEXT(cur_irq, links);
		}
 
		if (cur_irq == NULL || cur_irq->idesc == NULL)
			return (-1);
	} else {
		return EPERM;
	}

	snprintf(string, sizeof(string), " irq %d", irq);
	eisa_reg_print(e_dev, string, reg_state.num_interrupts ? 
				      &separator : NULL);
	reg_state.num_interrupts++;
	return (0);
}

int
eisa_release_intr(e_dev, irq, func)
	struct eisa_device *e_dev;
	int   irq;
	void  (*func)(void *);
{
	int	result;
	struct	irq_node *cur_irq;

	result = -1;
	cur_irq = TAILQ_FIRST(&e_dev->ioconf.irqs);
       	while (cur_irq != NULL) {
		if (cur_irq->irq_no == irq) {
			struct	irq_node *next_irq;

			next_irq = TAILQ_NEXT(cur_irq, links);
			if (cur_irq->idesc != NULL)
				intr_destroy(cur_irq->idesc);
			TAILQ_REMOVE(&e_dev->ioconf.irqs, cur_irq, links);
			free(cur_irq, M_DEVBUF);
			cur_irq = next_irq;
			result = 0;
		} else {
			cur_irq = TAILQ_NEXT(cur_irq, links);
		}
	}
	if (result != 0) {
		printf("%s%ld: Attempted to release an interrupt (%d) "
		       "it doesn't own\n", e_dev->driver->name,
			e_dev->unit, irq);
	}

	return (result);
}

int
eisa_enable_intr(e_dev, irq)
	struct eisa_device *e_dev;
	int irq;
{
	struct	irq_node *cur_irq;
	int	result;

	result = -1;
	cur_irq = TAILQ_FIRST(&e_dev->ioconf.irqs);
       	while (cur_irq != NULL) {
		if (cur_irq->irq_no == irq
		 && cur_irq->idesc != NULL) {
			result = intr_connect(cur_irq->idesc);
		}
		cur_irq = TAILQ_NEXT(cur_irq, links);
	}
	return (result);
}

#endif /* 0 */

static int
eisa_add_resvaddr(e_dev, head, base, size, flags)
	struct eisa_device *e_dev;
	struct resvlist *head;
	u_long	base;
	u_long	size;
	int	flags;
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
eisa_add_mspace(dev, mbase, msize, flags)
	device_t dev;
	u_long	mbase;
	u_long	msize;
	int	flags;
{
	struct eisa_device *e_dev = device_get_ivars(dev);
	return	eisa_add_resvaddr(e_dev, &(e_dev->ioconf.maddrs), mbase, msize,
				  flags);
}

int
eisa_add_iospace(dev, iobase, iosize, flags)
	device_t dev;
	u_long	iobase;
	u_long	iosize;
	int	flags;
{
	struct eisa_device *e_dev = device_get_ivars(dev);
	return	eisa_add_resvaddr(e_dev, &(e_dev->ioconf.ioaddrs), iobase,
				  iosize, flags);
}

#if 0

static int
eisa_reg_resvaddr(e_dev, head, resvaddr, reg_count)
	struct eisa_device *e_dev;
	struct resvlist *head;
	resvaddr_t *resvaddr;
	int *reg_count;
{
	if (reg_state.in_registration) {
		resvaddr_t *node;
		/*
		 * Ensure that this resvaddr is actually in the devices'
		 * reservation list.
		 */
		for(node = head->lh_first; node;
		    node = node->links.le_next) {
			if (node == resvaddr) {
				char buf[35];
				char separator = ',';
				char *string = buf;

				if (*reg_count == 0) {
					/* First time */
					string += sprintf(string, " at");
				}

				if (node->size == 1 
				  || (node->flags & RESVADDR_BITMASK))
					sprintf(string, " 0x%lx", node->addr);
				else
					sprintf(string, " 0x%lx-0x%lx",
						node->addr,
						node->addr + node->size - 1);
				eisa_reg_print(e_dev, buf,
						*reg_count ? &separator : NULL);
				(*reg_count)++;
				return (0);
			}
		}
		return (ENOENT);
	}
	return EPERM;
}

int
eisa_reg_mspace(e_dev, resvaddr)
	struct eisa_device *e_dev;
	resvaddr_t *resvaddr;
{
#ifdef NOT_YET
	/* 
	 * Punt on conflict detection for the moment.
	 * I want to develop a generic routine to do
	 * this for all device types.
	 */
	int checkthese = CC_MADDR;
	if (haveseen_dev(dev, checkthese))
		return -1;
#endif
	return (eisa_reg_resvaddr(e_dev, &(e_dev->ioconf.maddrs), resvaddr,
				  &(reg_state.num_maddrs)));
}
	
int
eisa_reg_iospace(e_dev, resvaddr)
	struct eisa_device *e_dev;
	resvaddr_t *resvaddr;
{
#ifdef NOT_YET
	/* 
	 * Punt on conflict detection for the moment.
	 * I want to develop a generic routine to do
	 * this for all device types.
	 */
	int checkthese = CC_IOADDR;
	if (haveseen_dev(dev, checkthese))
		return -1;
#endif
	return (eisa_reg_resvaddr(e_dev, &(e_dev->ioconf.ioaddrs), resvaddr,
				  &(reg_state.num_ioaddrs)));
}

#endif

static device_method_t eisa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		eisa_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	eisa_print_child),
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
	DRIVER_TYPE_MISC,
	1,			/* no softc */
};

DRIVER_MODULE(eisa, isab, eisa_driver, eisa_devclass, 0, 0);
DRIVER_MODULE(eisa, nexus, eisa_driver, eisa_devclass, 0, 0);
