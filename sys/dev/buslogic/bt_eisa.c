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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bus.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/eisa/eisaconf.h>

#include <dev/buslogic/btreg.h>

#define EISA_DEVICE_ID_BUSLOGIC_74X_B	0x0ab34201
#define EISA_DEVICE_ID_BUSLOGIC_74X_C	0x0ab34202
#define EISA_DEVICE_ID_SDC3222B		0x0ab34281
#define EISA_DEVICE_ID_SDC3222F		0x0ab34781
#define EISA_DEVICE_ID_SDC3222WS	0x0ab34981
#define EISA_DEVICE_ID_SDC3222WB	0x0ab34982
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

static const char *bt_match(eisa_id_t type);

static int
bt_eisa_alloc_resources(device_t dev)
{
	struct	bt_softc *bt = device_get_softc(dev);
	int rid;
	struct resource *port;
	struct resource *irq;
	int shared;

	/*
	 * XXX assumes that the iospace ranges are sorted in increasing
	 * order.
	 */
	rid = 0;
	port = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				  0, ~0, 1, RF_ACTIVE);
	if (!port)
		return (ENOMEM);

	bt_init_softc(dev, port, 0, 0);

	if (eisa_get_irq(dev) != -1) {
		shared = bt->level_trigger_ints ? RF_SHAREABLE : 0;
		rid = 0;
		irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
					 0, ~0, 1, shared | RF_ACTIVE);
		if (!irq) {
			if (port)
				bus_release_resource(dev, SYS_RES_IOPORT,
						     0, port);
			return (ENOMEM);
		}
	} else
		irq = 0;
	bt->irq = irq;

	return (0);
}

static void
bt_eisa_release_resources(device_t dev)
{
	struct	bt_softc *bt = device_get_softc(dev);

	if (bt->port)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, bt->port);
	if (bt->irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, bt->irq);
	bt_free_softc(dev);
}

static const char*
bt_match(eisa_id_t type)
{
	switch(type) {
		case EISA_DEVICE_ID_BUSLOGIC_74X_B:
			return ("Buslogic 74xB SCSI host adapter");
		case EISA_DEVICE_ID_BUSLOGIC_74X_C:
			return ("Buslogic 74xC SCSI host adapter");
		case EISA_DEVICE_ID_SDC3222B:
			return ("Storage Dimensions SDC3222B SCSI host adapter");
		case EISA_DEVICE_ID_SDC3222F:
			return ("Storage Dimensions SDC3222F SCSI host adapter");
		case EISA_DEVICE_ID_SDC3222WS:
			return ("Storage Dimensions SDC3222WS SCSI host adapter");
		case EISA_DEVICE_ID_SDC3222WB:
			return ("Storage Dimensions SDC3222WB SCSI host adapter");
		case EISA_DEVICE_ID_AMI_4801:
			return ("AMI Series 48 SCSI host adapter");
		default:
			break;
	}
	return (NULL);
}

static int
bt_eisa_probe(device_t dev)
{
	const char *desc;
	u_long iobase;
	struct bt_probe_info info;
	u_long port;
	u_long iosize;
	u_int  ioconf;
	int    result;
	int    shared;

	desc = bt_match(eisa_get_id(dev));
	if (!desc)
		return (ENXIO);
	device_set_desc(dev, desc);

	iobase = (eisa_get_slot(dev) * EISA_SLOT_SIZE); 
	if (eisa_get_id(dev) == EISA_DEVICE_ID_AMI_4801) {
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
			       eisa_get_slot(dev));
			return (ENXIO);
		}
		shared = (inb(iobase + AMI_EISA_IOCONF1) & AMI_IRQ_LEVEL) ?
				EISA_TRIGGER_LEVEL : EISA_TRIGGER_EDGE;
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
			       eisa_get_slot(dev));
			return (ENXIO);
		}
		shared = (inb(iobase + EISA_IRQ_TYPE) & LEVEL) ?
				EISA_TRIGGER_LEVEL : EISA_TRIGGER_EDGE;
	}
	bt_mark_probed_iop(port);

	/* Tell parent where our resources are going to be */
	eisa_add_iospace(dev, iobase, iosize, RESVADDR_NONE);
	eisa_add_iospace(dev, port, BT_IOSIZE, RESVADDR_NONE);

	/* And allocate them */
	bt_eisa_alloc_resources(dev);

	if (bt_port_probe(dev, &info) != 0) {
		printf("bt_eisa_probe: Probe failed for "
		       "card at slot 0x%x\n", eisa_get_slot(dev));
		result = ENXIO;
	} else {
		eisa_add_intr(dev, info.irq, shared);
		result = 0;
	}
	bt_eisa_release_resources(dev);

	return (result);
}

static int
bt_eisa_attach(device_t dev)
{
	struct bt_softc *bt = device_get_softc(dev);

	/* Allocate resources */
	bt_eisa_alloc_resources(dev);

	/* Allocate a dmatag for our SCB DMA maps */
	/* XXX Should be a child of the PCI bus dma tag */
	if (bus_dma_tag_create( /* parent	*/ NULL,
				/* alignment	*/ 1,
				/* boundary	*/ 0,
				/* lowaddr	*/ BUS_SPACE_MAXADDR_32BIT,
				/* highaddr	*/ BUS_SPACE_MAXADDR,
				/* filter	*/ NULL,
				/* filterarg	*/ NULL,
				/* maxsize	*/ BUS_SPACE_MAXSIZE_32BIT,
				/* nsegments	*/ ~0,
				/* maxsegsz	*/ BUS_SPACE_MAXSIZE_32BIT,
				/* flags	*/ 0,
				/* lockfunc	*/ busdma_lock_mutex,
				/* lockarg,	*/ &Giant,
				&bt->parent_dmat) != 0) {
		bt_eisa_release_resources(dev);
		return -1;
	}

	/*
	 * Now that we know we own the resources we need, do the full
	 * card initialization.
	 */
	if (bt_probe(dev) || bt_fetch_adapter_info(dev) || bt_init(dev)) {
		bt_eisa_release_resources(dev);
		return -1;
	}

	/* Attach sub-devices - always succeeds (sets up intr) */
	bt_attach(dev);

	return 0;
}

static device_method_t bt_eisa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bt_eisa_probe),
	DEVMETHOD(device_attach,	bt_eisa_attach),

	{ 0, 0 }
};

static driver_t bt_eisa_driver = {
	"bt",
	bt_eisa_methods,
	sizeof(struct bt_softc),
};

static devclass_t bt_devclass;

DRIVER_MODULE(bt, eisa, bt_eisa_driver, bt_devclass, 0, 0);
