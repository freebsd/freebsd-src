/*-
 * Copyright (c) 1999 Matthew N. Dodd <winter@jurai.net>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/buslogic/bt_mca.c,v 1.11.6.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * Written using the bt_isa/bt_pci code as a reference.
 *
 * Thanks to Andy Farkas <andyf@speednet.com.au> for
 * testing and feedback.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/cpufunc.h>
#include <machine/md_var.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/mca/mca_busreg.h>
#include <dev/mca/mca_busvar.h>

#include <isa/isavar.h>

#include <dev/buslogic/btreg.h>

#include <cam/scsi/scsi_all.h>

static struct mca_ident bt_mca_devs[] = {
	{ 0x0708, "BusLogic 32 Bit Bus Master MCA-to-SCSI Host Adapter" },
	{ 0x0708, "BusTek BT-640A Micro Channel to SCSI Host Adapter" },
	{ 0x0708, "Storage Dimensions SDC3211B 32-bit SCSI Host Adapter" },
	{ 0x0709, "Storage Dimensions SDC3211F 32-bit FAST SCSI Host Adapter" },
	{ 0, NULL },
};

#define BT_MCA_IOPORT_POS1		MCA_ADP_POS(MCA_POS0)
#define BT_MCA_IOPORT_POS2		MCA_ADP_POS(MCA_POS1)
#define BT_MCA_IOPORT_MASK1		0x10
#define BT_MCA_IOPORT_MASK2		0x03
#define BT_MCA_IOPORT_SIZE		0x03
#define BT_MCA_IOPORT(pos)		(0x30 + \
					(((u_int32_t)pos &\
						BT_MCA_IOPORT_MASK2) << 8) + \
					(((u_int32_t)pos &\
						BT_MCA_IOPORT_MASK1) >> 2))

#define BT_MCA_IRQ_POS			MCA_ADP_POS(MCA_POS0)
#define BT_MCA_IRQ_MASK			0x0e
#define BT_MCA_IRQ(pos)			(((pos & BT_MCA_IRQ_MASK) >> 1) + 8)

#define BT_MCA_DRQ_POS			MCA_ADP_POS(MCA_POS3)
#define BT_MCA_DRQ_MASK			0x0f
#define BT_MCA_DRQ(pos)			(pos & BT_MCA_DRQ_MASK)

#define BT_MCA_SCSIID_POS		MCA_ADP_POS(MCA_POS2)
#define BT_MCA_SCSIID_MASK		0xe0
#define BT_MCA_SCSIID(pos)		((pos & BT_MCA_SCSIID_MASK) >> 5)

static	bus_dma_filter_t	btvlbouncefilter;
static	bus_dmamap_callback_t	btmapsensebuffers;

static void
bt_mca_release_resources (device_t dev)
{
	struct  bt_softc *	bt = device_get_softc(dev);

	if (bt->port)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, bt->port);
	if (bt->irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, bt->irq);
	if (bt->drq)
		bus_release_resource(dev, SYS_RES_DRQ, 0, bt->drq);

	bt_free_softc(dev);
}

#define BT_MCA_PROBE	0
#define BT_MCA_ATTACH	1

static int
bt_mca_alloc_resources(device_t dev, int mode)
{
	struct resource *	io = NULL;
	struct resource *	irq = NULL;
	struct resource *	drq = NULL;
	int			rid;

	rid = 0;
	io = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid, RF_ACTIVE);
	if (io == NULL) {
		printf("bt_mca_alloc_resources() failed to allocate IOPORT\n");
		return (ENOMEM);
	}

	if (mode == BT_MCA_ATTACH) {

		rid = 0;
		irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
		if (irq == NULL) {
			printf("bt_mca_alloc_resources() failed to allocate IRQ\n");
			goto bad;
		}
	
		rid = 0;
		drq = bus_alloc_resource_any(dev, SYS_RES_DRQ, &rid, RF_ACTIVE);
		if (drq == NULL) {
			printf("bt_mca_alloc_resources() failed to allocate DRQ\n");
			goto bad;
		}
	}
	
	bt_init_softc(dev, io, irq, drq);

	return (0);
bad:
	bt_mca_release_resources(dev);
	return (ENOMEM);
}

static int
bt_mca_probe (device_t dev)
{
	const char *		desc;
	mca_id_t		id = mca_get_id(dev);
	struct bt_probe_info	info;
	u_int32_t		iobase = 0;
	u_int32_t		iosize = 0;
	u_int8_t		drq = 0;
	u_int8_t		irq = 0;
	u_int8_t		pos;
	int			result;

	desc = mca_match_id(id, bt_mca_devs);
	if (!desc)
		return (ENXIO);
	device_set_desc(dev, desc);

	pos = (mca_pos_read(dev, BT_MCA_IOPORT_POS1) & BT_MCA_IOPORT_MASK1) |
	      (mca_pos_read(dev, BT_MCA_IOPORT_POS2) & BT_MCA_IOPORT_MASK2);
	iobase = BT_MCA_IOPORT(pos);
	iosize = BT_MCA_IOPORT_SIZE;

	pos = mca_pos_read(dev, BT_MCA_DRQ_POS);
	drq = BT_MCA_DRQ(pos);

	pos = mca_pos_read(dev, BT_MCA_IRQ_POS);
	irq = BT_MCA_IRQ(pos);

	bt_mark_probed_iop(iobase);

	mca_add_iospace(dev, iobase, iosize);

	/* And allocate them */
	bt_mca_alloc_resources(dev, BT_MCA_PROBE);
			
	if (bt_port_probe(dev, &info) != 0) {
		printf("bt_mca_probe: Probe failed for "
		       "card at slot %d\n", mca_get_slot(dev) + 1);
		result = ENXIO;
	} else {	       
		mca_add_drq(dev, drq);
		mca_add_irq(dev, irq);
		result = 0;    
	}	       
	bt_mca_release_resources(dev);

	return (result);
}

static int
bt_mca_attach (device_t dev)
{
	struct bt_softc *	bt = device_get_softc(dev);
	int			error = 0;

	/* Allocate resources */      
	if ((error = bt_mca_alloc_resources(dev, BT_MCA_ATTACH))) {
		device_printf(dev, "Unable to allocate resources in bt_mca_attach()\n");
		return (error);
	}

	isa_dmacascade(rman_get_start(bt->drq));

	/* Allocate a dmatag for our CCB DMA maps */
	if (bus_dma_tag_create( /* parent	*/ NULL,
				/* alignemnt	*/ 1,
				/* boundary	*/ 0,
				/* lowaddr	*/ BUS_SPACE_MAXADDR_24BIT,
				/* highaddr	*/ BUS_SPACE_MAXADDR,
				/* filter	*/ btvlbouncefilter,
				/* filterarg	*/ bt,
				/* maxsize	*/ BUS_SPACE_MAXSIZE_32BIT,
				/* nsegments	*/ ~0,
				/* maxsegsz	*/ BUS_SPACE_MAXSIZE_32BIT,
				/* flags	*/ 0,
				/* lockfunc	*/ busdma_lock_mutex,
				/* lockarg	*/ &Giant,
				&bt->parent_dmat) != 0) {
		bt_mca_release_resources(dev);
		return (ENOMEM);
	}

	if (bt_init(dev)) {
		bt_mca_release_resources(dev);
		return (ENOMEM);
	}

	/* DMA tag for our sense buffers */
	if (bus_dma_tag_create(	/* parent	*/ bt->parent_dmat,
				/* alignment	*/ 1, 
				/* boundary	*/ 0,
				/* lowaddr	*/ BUS_SPACE_MAXADDR,    
				/* highaddr	*/ BUS_SPACE_MAXADDR,   
				/* filter	*/ NULL,
				/* filterarg	*/ NULL,
				/* maxsize	*/ bt->max_ccbs *
						   sizeof(struct scsi_sense_data),
				/* nsegments	*/ 1,
				/* maxsegsz	*/ BUS_SPACE_MAXSIZE_32BIT,
				/* flags	*/ 0,
				/* lockfunc	*/ busdma_lock_mutex,
				/* lockarg	*/ &Giant,
				&bt->sense_dmat) != 0) {
		bt_mca_release_resources(dev);
		return (ENOMEM);
	}

	bt->init_level++;     

	/* Allocation of sense buffers */
	if (bus_dmamem_alloc(bt->sense_dmat,
			     (void **)&bt->sense_buffers,       
			     BUS_DMA_NOWAIT, &bt->sense_dmamap) != 0) {
		bt_mca_release_resources(dev);
		return (ENOMEM);
	}

	bt->init_level++;     

	/* And permanently map them */
	bus_dmamap_load(bt->sense_dmat, bt->sense_dmamap,       
			bt->sense_buffers,
			bt->max_ccbs * sizeof(*bt->sense_buffers),
			btmapsensebuffers, bt, /*flags*/0);     

	bt->init_level++;     

	if ((error = bt_attach(dev))) {
		bt_mca_release_resources(dev);
		return (error);
	}

	return (0);
}

/*
 * This code should be shared with the ISA
 * stubs as its exactly the same.
 */

#define BIOS_MAP_SIZE (16 * 1024)

static int
btvlbouncefilter(void *arg, bus_addr_t addr)
{
	struct bt_softc *bt;

	bt = (struct bt_softc *)arg;

	addr &= BUS_SPACE_MAXADDR_24BIT;

	if (addr == 0
	 || (addr >= bt->bios_addr
	  && addr < (bt->bios_addr + BIOS_MAP_SIZE)))
		return (1);
	return (0);
}

static void
btmapsensebuffers(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct bt_softc* bt;

	bt = (struct bt_softc*)arg;
	bt->sense_buffers_physbase = segs->ds_addr;
}

static device_method_t bt_mca_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bt_mca_probe),
	DEVMETHOD(device_attach,	bt_mca_attach),

	{ 0, 0 }
};

static driver_t bt_mca_driver = {
	"bt",
	bt_mca_methods,
	sizeof(struct bt_softc),
};

static devclass_t bt_devclass;

DRIVER_MODULE(bt, mca, bt_mca_driver, bt_devclass, 0, 0);
MODULE_DEPEND(bt, mca, 1, 1, 1);
