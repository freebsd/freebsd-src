/*
 * Product specific probe and attach routines for:
 * 	27/284X and aic7770 motherboard SCSI controllers
 *
 * Copyright (c) 1995 Justin T. Gibbs
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
 *	$Id: aic7770.c,v 1.17 1995/11/05 04:42:47 gibbs Exp $
 */

#include "eisa.h"
#if NEISA > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/devconf.h>
#include <sys/kernel.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <machine/clock.h>
#include <i386/eisa/eisaconf.h>
#include <i386/scsi/aic7xxx.h>
#include <dev/aic7xxx/aic7xxx_reg.h>

#define EISA_DEVICE_ID_ADAPTEC_AIC7770	0x04907770
#define EISA_DEVICE_ID_ADAPTEC_274x	0x04907771
#define EISA_DEVICE_ID_ADAPTEC_284xB	0x04907756 /* BIOS enabled */
#define EISA_DEVICE_ID_ADAPTEC_284x	0x04907757 /* BIOS disabled*/

#define AHC_EISA_IOSIZE	0x100
#define INTDEF		0x5cul		/* Interrupt Definition Register */

int	aic7770probe __P((void));
int	aic7770_attach __P((struct eisa_device *e_dev));

struct eisa_driver ahc_eisa_driver = {
					"ahc",
					aic7770probe,
					aic7770_attach,
					/*shutdown*/NULL,
					&ahc_unit
				      };

DATA_SET (eisadriver_set, ahc_eisa_driver);

static struct kern_devconf kdc_aic7770 = {
	0, 0, 0,                /* filled in by dev_attach */
	"ahc", 0, { MDDT_EISA, 0, "bio" },
	eisa_generic_externalize, 0, 0, EISA_EXTERNALLEN,
	&kdc_eisa0,		/* parent */
	0,			/* parentdata */
	DC_UNCONFIGURED,	/* always start out here */
	NULL,
	DC_CLS_MISC		/* host adapters aren't special */
};

static  char*
aic7770_match(type)
	eisa_id_t type;
{
	switch(type) {
		case EISA_DEVICE_ID_ADAPTEC_AIC7770:
			return ("Adaptec aic7770 SCSI host adapter");
			break;
		case EISA_DEVICE_ID_ADAPTEC_274x:
			return ("Adaptec 274X SCSI host adapter");
			break;
		case EISA_DEVICE_ID_ADAPTEC_284xB:
		case EISA_DEVICE_ID_ADAPTEC_284x:
			return ("Adaptec 284X SCSI host adapter");
			break;
		default:
			break;
	}
	return (NULL);
}

int
aic7770probe(void)
{
	u_long iobase;
	char intdef;
	u_long irq;
	struct eisa_device *e_dev = NULL;
	int count;

	count = 0;
	while ((e_dev = eisa_match_dev(e_dev, aic7770_match))) {
		iobase = e_dev->ioconf.iobase;
		ahc_reset(iobase);

		eisa_add_iospace(e_dev, iobase, AHC_EISA_IOSIZE);
		intdef = inb(INTDEF + iobase);
		switch (intdef & 0xf) {
			case 9: 
				irq = 9;
				break;
			case 10:
				irq = 10;
				break;
			case 11:
				irq = 11;
				break;  
			case 12:
				irq = 12;
				break;
			case 14:
				irq = 14;
				break;
			case 15:
				irq = 15;
				break;
			default:
				printf("aic7770 at slot %d: illegal "
				       "irq setting %d\n", e_dev->ioconf.slot,
					intdef);
				continue;
		}
		eisa_add_intr(e_dev, irq);
		eisa_registerdev(e_dev, &ahc_eisa_driver, &kdc_aic7770);
		if(e_dev->id == EISA_DEVICE_ID_ADAPTEC_284xB
		   || e_dev->id == EISA_DEVICE_ID_ADAPTEC_284x) {
			/* Our real parent is the isa bus.  Say so */
			e_dev->kdc->kdc_parent = &kdc_isa0;
		}
		count++;
	}
	return count;
}

int
aic7770_attach(e_dev)
	struct eisa_device *e_dev;
{
	ahc_type type;
	struct ahc_data *ahc;
	int unit = e_dev->unit;
	int irq = ffs(e_dev->ioconf.irq) - 1;

	switch(e_dev->id) {
		case EISA_DEVICE_ID_ADAPTEC_AIC7770:
			type = AHC_AIC7770;
			break;
		case EISA_DEVICE_ID_ADAPTEC_274x:
			type = AHC_274;
			break;          
		case EISA_DEVICE_ID_ADAPTEC_284xB:
		case EISA_DEVICE_ID_ADAPTEC_284x:
			type = AHC_284;
			break;
		default: 
			printf("aic7770_attach: Unknown device type!\n");
			return -1;
			break;
	}

	if(!(ahc = ahc_alloc(unit, e_dev->ioconf.iobase, type, AHC_FNONE)))
		return -1;

	eisa_reg_start(e_dev);
	if(eisa_reg_iospace(e_dev, e_dev->ioconf.iobase, AHC_EISA_IOSIZE)) {
		ahc_free(ahc);
		return -1;
	}

	/*
	 * The IRQMS bit enables level sensitive interrupts only allow
	 * IRQ sharing if its set.
	 */
	if(eisa_reg_intr(e_dev, irq, ahc_eisa_intr, (void *)ahc, &bio_imask,
			 /*shared ==*/ahc->pause & IRQMS)) {
		ahc_free(ahc);
		return -1;
	}
	eisa_reg_end(e_dev);

	/*
	 * Now that we know we own the resources we need, do the full
	 * card initialization.
	 */
	if(ahc_init(unit)){
		ahc_free(ahc);
		/*
		 * The board's IRQ line will not be left enabled
		 * if we can't intialize correctly, so its safe
		 * to release the irq.
		 */
		eisa_release_intr(e_dev, irq, ahc_eisa_intr);
		return -1;
	}

	e_dev->kdc->kdc_state = DC_BUSY; /* host adapters always busy */

	/* Attach sub-devices - always succeeds */
	ahc_attach(unit);

	return(eisa_enable_intr(e_dev, irq));
}

#endif /* NEISA > 0 */
