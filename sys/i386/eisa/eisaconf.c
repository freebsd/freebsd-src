/*
 * EISA bus probe and attach routines 
 *
 * Copyright (c) 1995 Justin T. Gibbs.
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
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    Justin T. Gibbs.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 *	$Id: eisaconf.c,v 1.5 1995/11/09 07:14:11 gibbs Exp $
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/devconf.h>

#include "i386/isa/icu.h"	 /* Hmmm.  Interrupt stuff? */
#include "eisaconf.h"

struct eisa_device_node{
	struct	eisa_device dev;
	struct	eisa_device_node *next;
};

extern struct kern_devconf kdc_cpu0;
extern int bootverbose;
 
struct kern_devconf kdc_eisa0 = {
	0, 0, 0,                /* filled in by dev_attach */ 
	"eisa", 0, { MDDT_BUS, 0 },
	0, 0, 0, BUS_EXTERNALLEN,
	&kdc_cpu0,              /* parent is the CPU */
	0,                      /* no parentdata */
	DC_BUSY,                /* busses are always busy */
	"EISA bus",        
	DC_CLS_BUS              /* class */
};

/* 
 * This should probably be a list of "struct device" once it exists.
 * A struct device will incorperate ioconf and driver entry point data
 * regardless of how its attached to the system (via unions) as well
 * as more generic information that all device types should support (unit
 * number, if its installed, etc).
 */
static struct eisa_device_node *eisa_dev_list;
static struct eisa_device_node **eisa_dev_list_tail = &eisa_dev_list;
static u_long eisa_unit;

static struct eisa_driver mainboard_drv = {
				     "eisa",
				     NULL,
				     NULL,
				     NULL,
				     &eisa_unit
				   };
/*
** probe for EISA devices
*/
void
eisa_configure()
{
	int i,slot;
	char *id_string;
	struct eisa_device_node *dev_node;
	struct eisa_driver **e_drvp;
	struct eisa_driver *e_drv;
	struct eisa_device *e_dev;
	int eisaBase = 0xc80;
	eisa_id_t eisa_id;

	e_drvp = (struct eisa_driver**)eisadriver_set.ls_items;

	for (slot = 0; slot < EISA_SLOTS; eisaBase+=0x1000, slot++) {
		int id_size = sizeof(eisa_id);
		eisa_id = 0;
    		for( i = 0; i < id_size; i++ ) {
			outb(eisaBase,0xc80 + i); /*Some cards require priming*/
			eisa_id |= inb(eisaBase+i) << ((id_size-i-1)*CHAR_BIT);
		}
		if (eisa_id & 0x80000000)
			continue;  /* no EISA card in slot */

		/* Prepare an eisa_device_node for this slot */
		dev_node = (struct eisa_device_node *)malloc(sizeof(*dev_node),
							    M_DEVBUF, M_NOWAIT);
		if (!dev_node) {
			printf("eisa0: cannot malloc eisa_device_node");
			break; /* Try to attach what we have already */
		}
		bzero(dev_node, sizeof(*dev_node));
		e_dev = &(dev_node->dev);
		e_dev->id = eisa_id;
		/*
		 * Add an EISA ID based descriptive name incase we don't
		 * have a driver for it.  We do this now instead of after all
		 * probes because in the future, the eisa module will only
		 * be responsible for creating the list of devices in the system
		 * for the configuration manager to use.
		 */
		e_dev->full_name = (char *)malloc(10*sizeof(char),
						  M_DEVBUF, M_NOWAIT);
		if (!e_dev->full_name) {
			panic("Eisa probe unable to malloc");
		}
		sprintf(e_dev->full_name, "%c%c%c%x%x",
			EISA_MFCTR_CHAR0(e_dev->id),
			EISA_MFCTR_CHAR1(e_dev->id),
			EISA_MFCTR_CHAR2(e_dev->id),
			EISA_PRODUCT_ID(e_dev->id),
			EISA_REVISION_ID(e_dev->id));
		e_dev->ioconf.slot = slot; 
		/* Is iobase defined in any EISA specs? */
		e_dev->ioconf.iobase = eisaBase & 0xff00;
		*eisa_dev_list_tail = dev_node;
		eisa_dev_list_tail = &dev_node->next;
	}

	dev_node = eisa_dev_list;

	/*
	 * "Attach" the system board
	 */

	/* The first will be the motherboard in a true EISA system */
	if (dev_node && (dev_node->dev.ioconf.slot == 0)) {
		e_dev = &dev_node->dev;
		e_dev->driver = &mainboard_drv;
		e_dev->unit = (*e_dev->driver->unit)++; 
		id_string = e_dev->full_name;
		e_dev->full_name = (char *)malloc(strlen(e_dev->full_name)
						  + sizeof(" (System Board)")
						  + 1, M_DEVBUF, M_NOWAIT);
		if (!e_dev->full_name) {
			panic("Eisa probe unable to malloc");
		}
		sprintf(e_dev->full_name, "%s (System Board)", id_string);
		free(id_string, M_DEVBUF);

		printf("%s%ld: <%s>\n",
		       e_dev->driver->name,
		       e_dev->unit,
		       e_dev->full_name);

		/* Should set the iosize, but I don't have a spec handy */
		kdc_eisa0.kdc_parentdata = e_dev;
		dev_attach(&kdc_eisa0);
		printf("Probing for devices on the EISA bus\n");
		dev_node = dev_node->next;
	}

	if (!eisa_dev_list) {
		/*
		 * No devices.
		 */
		return;
    	}
	/*
	 * See what devices we recognize.
	 */
	while((e_drv = *e_drvp++)) {
		(*e_drv->probe)();
	}

	/*
	 * Attach the devices we found in slot order
	 */
	for (; dev_node; dev_node=dev_node->next) {
		e_dev = &dev_node->dev;
		e_drv = e_dev->driver;
		if (e_drv) {
			/*
			 * Determine the proper unit number for this device.
			 * Here we should look in the device table generated
			 * by config to see if this type of device is enabled
			 * either generically or for this particular address
			 * as well as determine if a reserved unit number
			 * should be used.  We should also ensure that the
			 * "next availible unit number" skips over "wired" unit
			 * numbers. This will be done after config is fixed or
			 * some other configuration method is chosen.
			 */
			e_dev->unit = (*e_drv->unit)++;
			if ((*e_drv->attach)(e_dev) < 0) {
				printf("%s0:%d <%s> attach failed\n",
					mainboard_drv.name, 
					e_dev->ioconf.slot,
					e_dev->full_name);
				continue;
			}
			e_dev->kdc->kdc_unit = e_dev->unit;
		}
		else {
			/* Announce unattached device */
			printf("%s0:%d <%s> unknown device\n",
				mainboard_drv.name,
				e_dev->ioconf.slot,
				e_dev->full_name);
		}
	}
}

struct eisa_device *
eisa_match_dev(e_dev, match_func)
	struct eisa_device *e_dev;
	char* (*match_func)(eisa_id_t);
{
	struct eisa_device_node *e_node = eisa_dev_list;

	if (e_dev) {
		/* Start our search from the last successful match */
		e_node = ((struct eisa_device_node *)e_dev)->next;
	}

	for(; e_node; e_node = e_node->next) {
		char *result;
		if (e_node->dev.driver) {
			/* Already claimed */
			continue;
		}
		result = (*match_func)(e_node->dev.id);
		if (result) {
			free(e_node->dev.full_name, M_DEVBUF);
			e_node->dev.full_name = result;
			return (&(e_node->dev));
		}
	}
	return NULL;
}

/* Interrupt and I/O space registration facitlities */
void
eisa_reg_start(e_dev)
	struct eisa_device *e_dev;
{
	/*
	 * Announce the device.
	 */
	printf("%s%ld: <%s>",
		e_dev->driver->name,
		e_dev->unit,
		e_dev->full_name);
}

/* Interrupt and I/O space registration facitlities */
void
eisa_reg_end(e_dev)
	struct eisa_device *e_dev;
{
	/*
	 * The device should have called eisa_registerdev()
	 * during its probe.  So hopefully we can use the kdc
	 * to weed out ISA/VL devices that use EISA id registers.
	 */
	if (e_dev->kdc && (e_dev->kdc->kdc_parent == &kdc_isa0)) {
		printf(" on isa\n");
	}
	else {
		printf(" on %s0 slot %d\n",
			mainboard_drv.name,
			e_dev->ioconf.slot);
	}
}

int
eisa_add_intr(e_dev, irq)
	struct eisa_device *e_dev;
	int irq;
{
	e_dev->ioconf.irq |= 1ul << irq;
	return 0;
}

int
eisa_reg_intr(e_dev, irq, func, arg, maskptr, shared)
	struct eisa_device *e_dev;
	int   irq;
	void  (*func)(void *);
	void  *arg;
	u_int *maskptr;
	int   shared;
{
	int result;
	int s;

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
	s = splhigh();
	/*
	 * This should really go to a routine that can optionally
	 * handle shared interrupts.
	 */
	result = register_intr(irq,			/* isa irq	*/
			       0,			/* deviced??	*/
			       0,			/* flags?	*/
			       (inthand2_t*) func,	/* handler      */
			       maskptr,			/* mask pointer	*/
			       (int)arg);		/* handler arg  */

	if (result) {
		printf ("eisa_reg_int: result=%d\n", result);
		splx(s);
		return (result);
	};
	update_intr_masks();
	splx(s);

	e_dev->ioconf.irq |= 1ul << irq;
	printf(" irq %d", irq);
	return (0);
}

int
eisa_release_intr(e_dev, irq, func)
	struct eisa_device *e_dev;
	int   irq;
	void  (*func)(void *);
{
	int result;
	int s;
       	
	if (!(e_dev->ioconf.irq & (1ul << irq))) {
		printf("%s%ld: Attempted to release an interrupt (%d) "
		       "it doesn't own\n", e_dev->driver->name,
			e_dev->unit, irq);
		return (-1);
	}

	s = splhigh();
	INTRDIS ((1ul<<irq));

	result = unregister_intr (irq, (inthand2_t*)func);
        
	if (result)
		printf ("eisa_release_intr: result=%d\n", result);
  
	update_intr_masks();

	splx(s);
	return (result);
}

int
eisa_enable_intr(e_dev, irq)
	struct eisa_device *e_dev;
	int irq;
{
	int s;

	if (!(e_dev->ioconf.irq & (1ul << irq))) {
		printf("%s%ld: Attempted to enable an interrupt (%d) "
		       "it doesn't own\n", e_dev->driver->name,
			e_dev->unit, irq);
		return (-1);
	}
	s = splhigh();
	INTREN((1ul << irq));
	splx(s);
	return 0;
}

int
eisa_add_iospace(e_dev, iobase, iosize)
	struct eisa_device *e_dev;
	u_long	iobase;
	int	iosize;
{
	/*
	 * We should develop a scheme for storing the results of
	 * multiple calls to this function.
	 */
	e_dev->ioconf.iobase = iobase;
	e_dev->ioconf.iosize = iosize;
	return 0;
}

int
eisa_reg_iospace(e_dev, iobase, iosize)
	struct eisa_device *e_dev;
	u_long	iobase;
	int	iosize;
{
	/*
	 * We should develop a scheme for storing the results of
	 * multiple calls to this function.
	 */
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
	e_dev->ioconf.iobase = iobase;
	e_dev->ioconf.iosize = iosize;

	printf(" at 0x%lx-0x%lx", iobase, iobase + iosize - 1);
	return (0);
}

int
eisa_registerdev(e_dev, driver, kdc_template)
	struct eisa_device *e_dev;
	struct eisa_driver *driver;
	struct kern_devconf *kdc_template;
{
	e_dev->driver = driver;	/* Driver now owns this device */
	e_dev->kdc = (struct kern_devconf *)malloc(sizeof(struct kern_devconf),
						   M_DEVBUF, M_NOWAIT);
	if (!e_dev->kdc) {
		printf("WARNING: eisa_registerdev unable to malloc! "
		       "Device kdc will not be registerd\n");
		return 1;
	}
	bcopy(kdc_template, e_dev->kdc, sizeof(*kdc_template));
	e_dev->kdc->kdc_description = e_dev->full_name;
	e_dev->kdc->kdc_parentdata = e_dev;
	dev_attach(e_dev->kdc);
	return (0);
}

/*
 * Provide EISA-specific device information to user programs using the
 * hw.devconf interface.
 */
int
eisa_externalize(id, userp, maxlen)
	struct eisa_device *id;
	void *userp;
	size_t *maxlen;
{
	if (*maxlen < (sizeof *id)) {
		return ENOMEM;
	}
	*maxlen -= (sizeof *id);
	return (copyout(id, userp, sizeof *id));
}


int
eisa_generic_externalize(p, kdc, userp, l)
	struct proc *p;
	struct kern_devconf *kdc;
	void *userp;
	size_t l;
{
    return eisa_externalize(kdc->kdc_eisa, userp, &l);
}
