/*
 * Product specific probe and attach routines for:
 * 	Buslogic BT74x SCSI controllers
 *
 * Copyright (c) 1995, 1998, 1999 Justin T. Gibbs
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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
 *	$Id: bt_eisa.c,v 1.1.2.1 1999/03/08 21:38:03 gibbs Exp $
 */

#include "eisa.h"
#if NEISA > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <i386/eisa/eisaconf.h>

#include <dev/buslogic/btreg.h>

#define EISA_DEVICE_ID_BUSLOGIC_74X_B	0x0ab34201
#define EISA_DEVICE_ID_BUSLOGIC_74X_C	0x0ab34202
#define EISA_DEVICE_ID_SDC3222F		0x0ab34781
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

static int	bt_eisa_probe(void);
static int	bt_eisa_attach(struct eisa_device *e_dev);

static struct eisa_driver bt_eisa_driver = {
	"bt",
	bt_eisa_probe,
	bt_eisa_attach,
	/*shutdown*/NULL,
	&bt_unit
};

DATA_SET (eisadriver_set, bt_eisa_driver);

static const char *bt_match(eisa_id_t type);

static const char*
bt_match(eisa_id_t type)
{
	switch(type) {
		case EISA_DEVICE_ID_BUSLOGIC_74X_B:
			return ("Buslogic 74xB SCSI host adapter");
			break;
		case EISA_DEVICE_ID_BUSLOGIC_74X_C:
			return ("Buslogic 74xC SCSI host adapter");
			break;
		case EISA_DEVICE_ID_SDC3222F:
			return ("Storage Dimensions SDC3222F SCSI host adapter");
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
		struct bt_softc *bt;
		struct bt_probe_info info;
		u_long port;
		u_long iosize;
		u_int  ioconf;

		iobase = (e_dev->ioconf.slot * EISA_SLOT_SIZE); 
		if (e_dev->id == EISA_DEVICE_ID_AMI_4801) {
			u_int ioconf1;

			iobase += AMI_EISA_SLOT_OFFSET;
			iosize = AMI_EISA_IOSIZE;
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
		} else {
			iobase += BT_EISA_SLOT_OFFSET;
			iosize = BT_EISA_IOSIZE;

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
		}
		bt_mark_probed_iop(port);

		/* Allocate a softc for use during probing */
		bt = bt_alloc(BT_TEMP_UNIT, I386_BUS_SPACE_IO, port);

		if (bt == NULL) {
			printf("bt_eisa_probe: Could not allocate softc for "
			       "card at slot 0x%x\n", e_dev->ioconf.slot);
			continue;
		}

		if (bt_port_probe(bt, &info) != 0) {
			printf("bt_eisa_probe: Probe failed for "
			       "card at slot 0x%x\n", e_dev->ioconf.slot);
		} else {
			eisa_add_iospace(e_dev, iobase, iosize, RESVADDR_NONE);
			eisa_add_iospace(e_dev, port, BT_IOSIZE, RESVADDR_NONE);
			eisa_add_intr(e_dev, info.irq);

			eisa_registerdev(e_dev, &bt_eisa_driver);

			count++;
		}
		bt_free(bt);
	}
	return count;
}

static int
bt_eisa_attach(struct eisa_device *e_dev)
{
	struct bt_softc *bt;
	int unit = e_dev->unit;
	int irq;
	resvaddr_t *ioport;
	resvaddr_t *eisa_ioport;

	if (TAILQ_FIRST(&e_dev->ioconf.irqs) == NULL)
		return (-1);

	irq = TAILQ_FIRST(&e_dev->ioconf.irqs)->irq_no;

	/*
	 * The addresses are sorted in increasing order
	 * so we know the port to pass to the core bt
	 * driver comes first.
	 */
	ioport = e_dev->ioconf.ioaddrs.lh_first;

	if (ioport == NULL)
		return -1;

	eisa_ioport = ioport->links.le_next;

	if (eisa_ioport == NULL)
		return -1;

	eisa_reg_start(e_dev);
	if (eisa_reg_iospace(e_dev, ioport))
		return -1;

	if (eisa_reg_iospace(e_dev, eisa_ioport))
		return -1;

	if ((bt = bt_alloc(unit, I386_BUS_SPACE_IO, ioport->addr)) == NULL)
		return -1;

	/* Allocate a dmatag for our SCB DMA maps */
	/* XXX Should be a child of the PCI bus dma tag */
	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/0, /*boundary*/0,
			       /*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       /*maxsize*/BUS_SPACE_MAXSIZE_32BIT,
			       /*nsegments*/BUS_SPACE_UNRESTRICTED,
			       /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			       /*flags*/0, &bt->parent_dmat) != 0) {
		bt_free(bt);
		return -1;
	}

	if (eisa_reg_intr(e_dev, irq, bt_intr, (void *)bt, &cam_imask,
			  /*shared ==*/bt->level_trigger_ints ? 1 : 0)) {
		bt_free(bt);
		return -1;
	}
	eisa_reg_end(e_dev);

	/*
	 * Now that we know we own the resources we need, do the full
	 * card initialization.
	 */
	if (bt_probe(bt) || bt_fetch_adapter_info(bt) || bt_init(bt)) {
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

	if (eisa_enable_intr(e_dev, irq)) {
		bt_free(bt);
		eisa_release_intr(e_dev, irq, bt_intr);
		return -1;
	}

	return 0;
}

#endif /* NEISA > 0 */
