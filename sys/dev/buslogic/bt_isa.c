/*
 * Product specific probe and attach routines for:
 *      Buslogic BT-54X and BT-445 cards
 *
 * Copyright (c) 1998, 1999 Justin T. Gibbs
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
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
 * $FreeBSD: src/sys/dev/buslogic/bt_isa.c,v 1.18 1999/10/12 21:35:43 dfr Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <isa/isavar.h>
#include <dev/buslogic/btreg.h>

#include <cam/scsi/scsi_all.h>

static	bus_dma_filter_t btvlbouncefilter;
static	bus_dmamap_callback_t btmapsensebuffers;

static int
bt_isa_alloc_resources(device_t dev, u_long portstart, u_long portend)
{
	int rid;
	struct resource *port;
	struct resource *irq;
	struct resource *drq;

	rid = 0;
	port = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				  portstart, portend, BT_NREGS, RF_ACTIVE);
	if (!port)
		return (ENOMEM);

	if (isa_get_irq(dev) != -1) {
		rid = 0;
		irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
					 0, ~0, 1, RF_ACTIVE);
		if (!irq) {
			if (port)
				bus_release_resource(dev, SYS_RES_IOPORT,
						     0, port);
			return (ENOMEM);
		}
	} else
		irq = 0;

	if (isa_get_drq(dev) != -1) {
		rid = 0;
		drq = bus_alloc_resource(dev, SYS_RES_DRQ, &rid,
					 0, ~0, 1, RF_ACTIVE);
		if (!drq) {
			if (port)
				bus_release_resource(dev, SYS_RES_IOPORT,
						     0, port);
			if (irq)
				bus_release_resource(dev, SYS_RES_IRQ,
						     0, irq);
			return (ENOMEM);
		}
	} else
		drq = 0;

	bt_init_softc(dev, port, irq, drq);

	return (0);
}

static void
bt_isa_release_resources(device_t dev)
{
	struct	bt_softc *bt = device_get_softc(dev);

	if (bt->port)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, bt->port);
	if (bt->irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, bt->irq);
	if (bt->drq)
		bus_release_resource(dev, SYS_RES_DRQ, 0, bt->drq);
	bt_free_softc(dev);
}

/*
 * Check if the device can be found at the port given
 * and if so, set it up ready for further work
 * as an argument, takes the isa_device structure from
 * autoconf.c
 */
static int
bt_isa_probe(device_t dev)
{
	/*
	 * find unit and check we have that many defined
	 */
	int	port_index;
        int	max_port_index;

	/* No pnp support */
	if (isa_get_vendorid(dev))
		return (ENXIO);

	port_index = 0;
	max_port_index = BT_NUM_ISAPORTS - 1;
	/*
	 * Bound our board search if the user has
	 * specified an exact port.
	 */
	bt_find_probe_range(isa_get_port(dev), &port_index, &max_port_index);

	if (port_index < 0)
		return (ENXIO);

	/* Attempt to find an adapter */
	for (;port_index <= max_port_index; port_index++) {
		struct bt_probe_info info;
		u_int ioport;

		ioport = bt_iop_from_bio(port_index);

		/*
		 * Ensure this port has not already been claimed already
		 * by a PCI, EISA or ISA adapter.
		 */
		if (bt_check_probed_iop(ioport) != 0)
			continue;

		/* Initialise the softc for use during probing */
		if (bt_isa_alloc_resources(dev, ioport,
					   ioport + BT_NREGS -1) != 0)
			continue;

		/* We're going to attempt to probe it now, so mark it probed */
		bt_mark_probed_bio(port_index);

		if (bt_port_probe(dev, &info) != 0) {
			if (bootverbose)
				printf("bt_isa_probe: Probe failed at 0x%x\n",
				       ioport);
			bt_isa_release_resources(dev);
			continue;
		}

		bt_isa_release_resources(dev);

		bus_set_resource(dev, SYS_RES_DRQ, 0, info.drq, 1);
		bus_set_resource(dev, SYS_RES_IRQ, 0, info.irq, 1);

		return (0);
	}

	return (ENXIO);
}

/*
 * Attach all the sub-devices we can find
 */
static int
bt_isa_attach(device_t dev)
{
	struct	bt_softc *bt = device_get_softc(dev);
	bus_dma_filter_t *filter;
	void		 *filter_arg;
	bus_addr_t	 lowaddr;
	int		 error, drq;

	/* Initialise softc */
	error = bt_isa_alloc_resources(dev, 0, ~0);
	if (error) {
		device_printf(dev, "can't allocate resources in bt_isa_attach\n");
		return error;
	}

	/* Program the DMA channel for external control */
	if ((drq = isa_get_drq(dev)) != -1)
		isa_dmacascade(drq);

	/* Allocate our parent dmatag */
	filter = NULL;
	filter_arg = NULL;
	lowaddr = BUS_SPACE_MAXADDR_24BIT;
	if (bt->model[0] == '4') {
		/*
		 * This is a VL adapter.  Typically, VL devices have access
		 * to the full 32bit address space.  On BT-445S adapters
		 * prior to revision E, there is a hardware bug that causes
		 * corruption of transfers to/from addresses in the range of
		 * the BIOS modulo 16MB.  The only properly functioning
		 * BT-445S Host Adapters have firmware version 3.37.
		 * If we encounter one of these adapters and the BIOS is
		 * installed, install a filter function for our bus_dma_map
		 * that will catch these accesses and bounce them to a safe
		 * region of memory.
		 */
		if (bt->bios_addr != 0
		 && strcmp(bt->model, "445S") == 0
		 && strcmp(bt->firmware_ver, "3.37") < 0) {
			filter = btvlbouncefilter;
			filter_arg = bt;
		} else {
			lowaddr = BUS_SPACE_MAXADDR_32BIT;
		}
	}
			
	/* XXX Should be a child of the ISA or VL bus dma tag */
	if (bus_dma_tag_create(/*parent*/NULL, /*alignemnt*/1, /*boundary*/0,
                               lowaddr, /*highaddr*/BUS_SPACE_MAXADDR,
                               filter, filter_arg,
                               /*maxsize*/BUS_SPACE_MAXSIZE_32BIT,
                               /*nsegments*/BUS_SPACE_UNRESTRICTED,
                               /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
                               /*flags*/0, &bt->parent_dmat) != 0) {
		bt_isa_release_resources(dev);
                return (ENOMEM);
        }                              

        error = bt_init(dev);
        if (error) {
		bt_isa_release_resources(dev);
                return (ENOMEM);
        }

	if (lowaddr != BUS_SPACE_MAXADDR_32BIT) {
		/* DMA tag for our sense buffers */
		if (bus_dma_tag_create(bt->parent_dmat, /*alignment*/1,
				       /*boundary*/0,
				       /*lowaddr*/BUS_SPACE_MAXADDR,
				       /*highaddr*/BUS_SPACE_MAXADDR,
				       /*filter*/NULL, /*filterarg*/NULL,
				       bt->max_ccbs
					   * sizeof(struct scsi_sense_data),
				       /*nsegments*/1,
				       /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
				       /*flags*/0, &bt->sense_dmat) != 0) {
			bt_isa_release_resources(dev);
			return (ENOMEM);
		}

		bt->init_level++;

		/* Allocation of sense buffers */
		if (bus_dmamem_alloc(bt->sense_dmat,
				     (void **)&bt->sense_buffers,
				     BUS_DMA_NOWAIT, &bt->sense_dmamap) != 0) {
			bt_isa_release_resources(dev);
			return (ENOMEM);
		}

		bt->init_level++;

		/* And permanently map them */
		bus_dmamap_load(bt->sense_dmat, bt->sense_dmamap,
       				bt->sense_buffers,
				bt->max_ccbs * sizeof(*bt->sense_buffers),
				btmapsensebuffers, bt, /*flags*/0);

		bt->init_level++;
	}

	error = bt_attach(dev);
	if (error) {
		bt_isa_release_resources(dev);
		return (error);
	}

	return (0);
}

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

static device_method_t bt_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bt_isa_probe),
	DEVMETHOD(device_attach,	bt_isa_attach),

	{ 0, 0 }
};

static driver_t bt_isa_driver = {
	"bt",
	bt_isa_methods,
	sizeof(struct bt_softc),
};

static devclass_t bt_devclass;

DRIVER_MODULE(bt, isa, bt_isa_driver, bt_devclass, 0, 0);
