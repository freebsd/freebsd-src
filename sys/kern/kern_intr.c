/*
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * $Id: kern_intr.c,v 1.2 1997/05/28 22:11:00 se Exp $
 *
 */

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#ifdef RESOURCE_CHECK
#include <sys/drvresource.h>
#endif /* RESOURCE_CHECK */

#include <i386/isa/icu.h>
#include <i386/isa/isa_device.h>
#include <sys/interrupt.h> /* XXX needs inthand2_t from isa_device.h */

#include <machine/ipl.h>

#include <stddef.h>

#include "vector.h"

/*
 * The interrupt multiplexer calls each of the handlers in turn,
 * and applies the associated interrupt mask to "cpl", which is
 * defined as a ".long" in /sys/i386/isa/icu.s
 */

static inline intrmask_t
splq(intrmask_t mask)
{
	intrmask_t tmp = cpl;
	cpl |= mask;
	return (tmp);
}

static void
intr_mux(void *arg)
{
	intrec *p = arg;

	while (p != NULL) {
		int oldspl = splq(p->mask);
		/* inthand2_t should take (void*) argument */
		p->handler(p->argument);
		splx(oldspl);
		p = p->next;
	}
}

/* XXX better use NHWI from <machine/ipl.h> for array size ??? */
static intrec *intreclist_head[ICU_LEN];

static intrec*
find_idesc(unsigned *maskptr, int irq)
{
	intrec *p = intreclist_head[irq];

	while (p && p->maskptr != maskptr)
		p = p->next;

	return (p);
}

static intrec**
find_pred(intrec *idesc, int irq)
{
	intrec **pp = &intreclist_head[irq];
	intrec *p = *pp;

	while (p != idesc) {
		if (p == NULL)
			return (NULL);
		pp = &p->next;
		p = *pp;
	}
	return (pp);
}

/*
 * Both the low level handler and the shared interrupt multiplexer
 * block out further interrupts as set in the handlers "mask", while
 * the handler is running. In fact *maskptr should be used for this
 * purpose, but since this requires one more pointer dereference on
 * each interrupt, we rather bother update "mask" whenever *maskptr
 * changes. The function "update_masks" should be called **after**
 * all manipulation of the linked list of interrupt handlers hung
 * off of intrdec_head[irq] is complete, since the chain of handlers
 * will both determine the *maskptr values and the instances of mask
 * that are fixed. This function should be called with the irq for
 * which a new handler has been add blocked, since the masks may not
 * yet know about the use of this irq for a device of a certain class.
 */

static void
update_mux_masks(void)
{
	int irq;
	for (irq = 0; irq < ICU_LEN; irq++) {
		intrec *idesc = intreclist_head[irq];
		while (idesc != NULL) {
			if (idesc->maskptr != NULL) {
				/* our copy of *maskptr may be stale, refresh */
				idesc->mask = *idesc->maskptr;
			}
			idesc = idesc->next;
		}
	}
}

static void
update_masks(intrmask_t *maskptr, int irq)
{
	intrmask_t mask = 1 << irq;

	if (maskptr == NULL)
		return;

	if (find_idesc(maskptr, irq) == NULL) {
		/* no reference to this maskptr was found in this irq's chain */
		if ((*maskptr & mask) == 0)
			return;
		/* the irq was included in the classes mask, remove it */
		INTRUNMASK(*maskptr, mask);
	} else {
		/* a reference to this maskptr was found in this irq's chain */
		if ((*maskptr & mask) != 0)
			return;
		/* put the irq into the classes mask */
		INTRMASK(*maskptr, mask);
	}
	/* we need to update all values in the intr_mask[irq] array */
	update_intr_masks();
	/* update mask in chains of the interrupt multiplex handler as well */
	update_mux_masks();
}

/*
 * Add interrupt handler to linked list hung off of intreclist_head[irq]
 * and install shared interrupt multiplex handler, if necessary
 */

static int
add_intrdesc(intrec *idesc)
{
	int irq = idesc->intr;

	intrec *head = intreclist_head[irq];

	if (head == NULL) {
		/* first handler for this irq, just install it */
		if (icu_setup(irq, idesc->handler, idesc->argument, 
			      idesc->maskptr, idesc->flags) != 0)
			return (-1);

		update_intrname(irq, idesc->devdata);
		/* keep reference */
		intreclist_head[irq] = idesc;
	} else {
		if ((idesc->flags & INTR_EXCL) != 0
		    || (head->flags & INTR_EXCL) != 0) {
			/*
			 * can't append new handler, if either list head or
			 * new handler do not allow interrupts to be shared
			 */
			printf("\tdevice combination doesn't support shared irq%d\n", 
			       irq);
			return (-1);
		}
		if (head->next == NULL) {
			/*
			 * second handler for this irq, replace device driver's
			 * handler by shared interrupt multiplexer function
			 */
			icu_unset(irq, head->handler);
			if (icu_setup(irq, intr_mux, head, 0, 0) != 0)
				return (-1);
			if (bootverbose)
				printf("\tusing shared irq%d.\n", irq);
			update_intrname(irq, -1);
		}
		/* just append to the end of the chain */
		while (head->next != NULL)
			head = head->next;
		head->next = idesc;
	}
	update_masks(idesc->maskptr, irq);
	return (0);
}

/*
 * Add the interrupt handler descriptor data structure created by an
 * earlier call of create_intr() to the linked list for its irq and
 * adjust the interrupt masks if necessary.
 *
 * This function effectively activates the handler.
 */

int
intr_connect(intrec *idesc)
{
	int errcode = -1;
	int irq;

#ifdef RESOURCE_CHECK
	int resflag;
#endif /* RESOURCE_CHECK */

	if (idesc == NULL)
		return (-1);

	irq = idesc->intr;
#ifdef RESOURCE_CHECK
	resflag = (idesc->flags & INTR_EXCL) ? RESF_NONE : RESF_SHARED;
	if (resource_claim(idesc->devdata, REST_INT, resflag, irq, irq) == 0)
#endif /* RESOURCE_CHECK */
	{
		/* block this irq */
		intrmask_t oldspl = splq(1 << irq);

		/* add irq to class selected by maskptr */
		errcode = add_intrdesc(idesc);
		splx(oldspl);
	}
	if (errcode != 0)
		printf("\tintr_connect(irq%d) failed, result=%d\n", 
		       irq, errcode);

	return (errcode);
}

/*
 * Remove the interrupt handler descriptor data connected created by an
 * earlier call of intr_connect() from the linked list and adjust the
 * interrupt masks if necessary.
 *
 * This function deactivates the handler.
 */

int
intr_disconnect(intrec *idesc)
{
	intrec **hook, *head;
	int irq;
	int errcode = 0;

	if (idesc == NULL)
		return (-1);

	irq = idesc->intr;

	/* find pointer that keeps the reference to this interrupt descriptor */
	hook = find_pred(idesc, irq);
	if (hook == NULL)
		return (-1);

	/* make copy of original list head, the line after may overwrite it */
	head = intreclist_head[irq];

	/* unlink: make predecessor point to idesc->next instead of to idesc */
	*hook = idesc->next;

	/* now check whether the element we removed was the list head */
	if (idesc == head) {
		intrmask_t oldspl = splq(1 << irq);

		/* we want to remove the list head, which was known to intr_mux */
		icu_unset(irq, intr_mux);

		/* check whether the new list head is the only element on list */
		head = intreclist_head[irq];
		if (head != NULL) {
			if (head->next != NULL) {
				/* install the multiplex handler with new list head as argument */
				errcode = icu_setup(irq, intr_mux, head, 0, 0);
				if (errcode == 0)
					update_intrname(irq, -1);
			} else {
				/* install the one remaining handler for this irq */
				errcode = icu_setup(irq, head->handler,
						    head->argument,
						    head->maskptr, head->flags);
				if (errcode == 0)
					update_intrname(irq, head->devdata);
			}
		}
		splx(oldspl);
	}
	update_masks(idesc->maskptr, irq);
#ifdef RESOURCE_CHECK
	resource_free(idesc->devdata);
#endif /* RESOURCE_CHECK */
	return (0);
}

/*
 * Create an interrupt handler descriptor data structure, which later can
 * be activated or deactivated at will by calls of [dis]connect(intrec*).
 *
 * The dev_instance pointer is required for resource management, and will
 * only be passed through to resource_claim().
 *
 * The interrupt handler takes an argument of type (void*), which is not
 * what is currently used for ISA devices. But since the unit number passed
 * to an ISA interrupt handler can be stored in a (void*) variable, this
 * causes no problems. Eventually all the ISA interrupt handlers should be
 * modified to accept the pointer to their private data, too, instead of
 * an integer index.
 *
 * There will be functions that derive a driver and unit name from a
 * dev_instance variable, and those functions will be used to maintain the
 * interrupt counter label array referenced by systat and vmstat to report
 * device interrupt rates (->update_intrlabels).
 */

intrec *
intr_create(void *dev_instance, int irq, inthand2_t handler, void *arg,
	     intrmask_t *maskptr, int flags)
{
	intrec *idesc;

	if (ICU_LEN > 8 * sizeof *maskptr) {
		printf("create_intr: ICU_LEN of %d too high for %d bit intrmask\n",
		       ICU_LEN, 8 * sizeof *maskptr);
		return (NULL);
	}
	if ((unsigned)irq >= ICU_LEN) {
		printf("create_intr: requested irq%d too high, limit is %d\n",
		       irq, ICU_LEN -1);
		return (NULL);
	}

	idesc = malloc(sizeof *idesc, M_DEVBUF, M_WAITOK);
	if (idesc) {
		idesc->next     = NULL;
		bzero(idesc, sizeof *idesc);

		idesc->devdata  = dev_instance;
		idesc->handler  = handler;
		idesc->argument = arg;
		idesc->maskptr  = maskptr;
		idesc->intr     = irq;
		idesc->flags    = flags;
	}
	return (idesc);
}

/*
 * Return the memory held by the interrupt handler descriptor data structure
 * to the system. Make sure, the handler is not actively used anymore, before.
 */

int
intr_destroy(intrec *rec)
{
	if (intr_disconnect(rec) != 0)
		return (-1);
	free(rec, M_DEVBUF);
	return (0);
}

/*
 * Emulate the register_intr() call previously defined as low level function.
 * That function (now icu_setup()) may no longer be directly called, since 
 * a conflict between an ISA and PCI interrupt might go by unnocticed, else.
 */

int
register_intr(int intr, int device_id, u_int flags,
	      inthand2_t handler, u_int *maskptr, int unit)
{
	/* XXX modify to include isa_device instead of device_id */
	intrec *idesc;

	flags |= INTR_EXCL;
	idesc = intr_create((void *)device_id, intr, handler, 
			    (void*)unit, maskptr, flags);
	return (intr_connect(idesc));
}

/*
 * Emulate the old unregister_intr() low level function. 
 * Make sure there is just one interrupt, that it was 
 * registered as non-shared, and that the handlers match.
 */

int
unregister_intr(int intr, inthand2_t handler)
{
	intrec *p = intreclist_head[intr];

	if (p != NULL && (p->flags & INTR_EXCL) != 0 && p->handler == handler)
		return (intr_destroy(p));
	return (EINVAL);
}
