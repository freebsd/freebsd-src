/*
 * Product specific probe and attach routines for:
 * 	Buslogic BT74x SCSI controllers
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
 *	$FreeBSD$
 */

#include "eisa.h"
#if NEISA > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <machine/clock.h>
#include <i386/eisa/eisaconf.h>
#include <i386/scsi/btreg.h>

#define EISA_DEVICE_ID_BUSLOGIC_74X_B	0x0ab34201
#define EISA_DEVICE_ID_BUSLOGIC_74X_C	0x0ab34202
#define	EISA_DEVICE_ID_AMI_4801		0x05a94801

#define BT_IOSIZE		0x04		/* Move to central header */
#define BT_EISA_IOSIZE		0x100
#define	BT_EISA_SLOT_OFFSET	0xc00

#define EISA_IOCONF			0x08C
#define		PORTADDR		0x07
#define			PORT_330	0x00
#define			PORT_334	0x01
#define			PORT_230	0x02
#define			PORT_234	0x03
#define			PORT_130	0x04
#define			PORT_134	0x05
#define		IRQ_CHANNEL		0xe0
#define			INT_11		0x40
#define			INT_10		0x20
#define			INT_15		0xa0
#define			INT_12		0x60
#define			INT_14		0x80
#define			INT_9		0x00

#define EISA_IRQ_TYPE                   0x08D
#define       LEVEL                     0x40

/* Definitions for the AMI Series 48 controler */
#define	AMI_EISA_IOSIZE			0x500	/* Two separate ranges?? */
#define	AMI_EISA_SLOT_OFFSET		0x800
#define	AMI_EISA_IOCONF			0x000
#define		AMI_DMA_CHANNEL		0x03
#define		AMI_IRQ_CHANNEL		0x1c
#define			AMI_INT_15	0x14
#define			AMI_INT_14	0x10
#define			AMI_INT_12	0x0c
#define			AMI_INT_11	0x00
#define			AMI_INT_10	0x08
#define			AMI_INT_9	0x04
#define		AMI_BIOS_ADDR		0xe0

#define	AMI_EISA_IOCONF1		0x001
#define		AMI_PORTADDR		0x0e
#define			AMI_PORT_334	0x08
#define			AMI_PORT_330	0x00
#define			AMI_PORT_234	0x0c
#define			AMI_PORT_230	0x04
#define			AMI_PORT_134	0x0a
#define			AMI_PORT_130	0x02
#define		AMI_IRQ_LEVEL		0x01


#define	AMI_MISC2_OPTIONS		0x49E
#define		AMI_ENABLE_ISA_DMA	0x08

static int	bt_eisa_probe __P((void));
static int	bt_eisa_attach __P((struct eisa_device *e_dev));

struct eisa_driver bt_eisa_driver = {
					"bt",
					bt_eisa_probe,
					bt_eisa_attach,
					/*shutdown*/NULL,
					&bt_unit
				      };

DATA_SET (eisadriver_set, bt_eisa_driver);

static char   *bt_match __P((eisa_id_t type));

static  char*
bt_match(type)
	eisa_id_t type;
{
	switch(type) {
		case EISA_DEVICE_ID_BUSLOGIC_74X_B:
			return ("Buslogic 74xB SCSI host adapter");
			break;
		case EISA_DEVICE_ID_BUSLOGIC_74X_C:
			return ("Buslogic 74xC SCSI host adapter");
			break;
		case EISA_DEVICE_ID_AMI_4801:
			return ("AMI Series 48 SCSI host adapter");
			break;
		default:
			break;
	}
	return (NULL);
}

static int
bt_eisa_probe(void)
{
	u_long iobase;
	struct eisa_device *e_dev = NULL;
	int count;

	count = 0;
	while ((e_dev = eisa_match_dev(e_dev, bt_match))) {
		u_char ioconf;
		u_long port;
		int irq;

		iobase = (e_dev->ioconf.slot * EISA_SLOT_SIZE); 
		if(e_dev->id == EISA_DEVICE_ID_AMI_4801) {
			u_char ioconf1;
			iobase += AMI_EISA_SLOT_OFFSET;

			eisa_add_iospace(e_dev, iobase, AMI_EISA_IOSIZE,
					 RESVADDR_NONE);

			ioconf = inb(iobase + AMI_EISA_IOCONF);
			ioconf1 = inb(iobase + AMI_EISA_IOCONF1);
			/* Determine "ISA" I/O port */
			switch (ioconf1 & AMI_PORTADDR) {
				case AMI_PORT_330:
					port = 0x330;
					break;
				case AMI_PORT_334:
					port = 0x334;
					break;
				case AMI_PORT_230:
					port = 0x230;
					break;
				case AMI_PORT_234:
					port = 0x234;
					break;
				case AMI_PORT_134:
					port = 0x134;
					break;
				case AMI_PORT_130:
					port = 0x130;
					break;
				default:
					/* Disabled */
					printf("bt: AMI EISA Adapter at "
					       "slot %d has a disabled I/O "
					       "port.  Cannot attach.\n",
					       e_dev->ioconf.slot);
					continue;
			}

			eisa_add_iospace(e_dev, port, BT_IOSIZE, RESVADDR_NONE);

			/* Determine our IRQ */
			switch (ioconf & AMI_IRQ_CHANNEL) {
				case AMI_INT_11:
					irq = 11;
					break;
				case AMI_INT_10:
					irq = 10;
					break;
				case AMI_INT_15:
					irq = 15;
					break;
				case AMI_INT_12:
					irq = 12;
					break;
				case AMI_INT_14:
					irq = 14;
					break;
				case AMI_INT_9:
					irq = 9;
					break;
				default:
					/* Disabled */
					printf("bt: AMI EISA Adapter at "
					       "slot %d has its IRQ disabled. "
					       "Cannot attach.\n", 
						e_dev->ioconf.slot);
					continue;
			}
		}
		else {
			iobase += BT_EISA_SLOT_OFFSET;

			eisa_add_iospace(e_dev, iobase, BT_EISA_IOSIZE,
					RESVADDR_NONE);

			ioconf = inb(iobase + EISA_IOCONF);
			/* Determine "ISA" I/O port */
			switch (ioconf & PORTADDR) {
				case PORT_330:
					port = 0x330;
					break;
				case PORT_334:
					port = 0x334;
					break;
				case PORT_230:
					port = 0x230;
					break;
				case PORT_234:
					port = 0x234;
					break;
				case PORT_130:
					port = 0x130;
					break;
				case PORT_134:
					port = 0x134;
					break;
				default:
					/* Disabled */
					printf("bt: Buslogic EISA Adapter at "
					       "slot %d has a disabled I/O "
					       "port.  Cannot attach.\n",
					       e_dev->ioconf.slot);
					continue;
			}
			eisa_add_iospace(e_dev, port, BT_IOSIZE, RESVADDR_NONE);

			/* Determine our IRQ */
			switch (ioconf & IRQ_CHANNEL) {
				case INT_11:
					irq = 11;
					break;
				case INT_10:
					irq = 10;
					break;
				case INT_15:
					irq = 15;
					break;
				case INT_12:
					irq = 12;
					break;
				case INT_14:
					irq = 14;
					break;
				case INT_9:
					irq = 9;
					break;
				default:
					/* Disabled */
					printf("bt: Buslogic EISA Adapter at "
					       "slot %d has its IRQ disabled. "
					       "Cannot attach.\n", 
						e_dev->ioconf.slot);
					continue;
			}

		}
		eisa_add_intr(e_dev, irq);

		eisa_registerdev(e_dev, &bt_eisa_driver);

		count++;
	}
	return count;
}

static int
bt_eisa_attach(e_dev)
	struct eisa_device *e_dev;
{
	struct bt_data *bt;
	int unit = e_dev->unit;
	int irq = ffs(e_dev->ioconf.irq) - 1;
	resvaddr_t *ioport;
	resvaddr_t *eisa_ioport;
	u_char level_intr;

	/*
	 * The addresses are sorted in increasing order
	 * so we know the port to pass to the core bt
	 * driver comes first.
	 */
	ioport = e_dev->ioconf.ioaddrs.lh_first;

	if(!ioport)
		return -1;

	eisa_ioport = ioport->links.le_next;

	if(!eisa_ioport)
		return -1;

	if(e_dev->id == EISA_DEVICE_ID_AMI_4801)
		level_intr = inb(eisa_ioport->addr + AMI_EISA_IOCONF1)
				& AMI_IRQ_LEVEL;
	else
		level_intr = inb(eisa_ioport->addr + EISA_IRQ_TYPE)
                                & LEVEL;

	eisa_reg_start(e_dev);
	if(eisa_reg_iospace(e_dev, ioport))
		return -1;

	if(eisa_reg_iospace(e_dev, eisa_ioport))
		return -1;

	if(!(bt = bt_alloc(unit, ioport->addr)))
		return -1;

	if(eisa_reg_intr(e_dev, irq, bt_intr, (void *)bt, &bio_imask,
			 /*shared ==*/level_intr)) {
		bt_free(bt);
		return -1;
	}
	eisa_reg_end(e_dev);

	/*
	 * Now that we know we own the resources we need, do the full
	 * card initialization.
	 */
	if(bt_init(bt)){
		bt_free(bt);
		/*
		 * The board's IRQ line will not be left enabled
		 * if we can't intialize correctly, so its safe
		 * to release the irq.
		 */
		eisa_release_intr(e_dev, irq, bt_intr);
		return -1;
	}

	/* Attach sub-devices - always succeeds */
	bt_attach(bt);

	if(eisa_enable_intr(e_dev, irq)) {
		bt_free(bt);
		eisa_release_intr(e_dev, irq, bt_intr);
		return -1;
	}

	return 0;
}

#endif /* NEISA > 0 */
