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
 *	$Id: bt_isa.c,v 1.6 1999/03/08 21:32:59 gibbs Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <i386/isa/isa_device.h>
#include <dev/buslogic/btreg.h>

#include <cam/scsi/scsi_all.h>

static	int bt_isa_probe __P((struct isa_device *dev));
static	int bt_isa_attach __P((struct isa_device *dev));
static	void bt_isa_intr __P((void *unit));

static	bus_dma_filter_t btvlbouncefilter;
static	bus_dmamap_callback_t btmapsensebuffers;

struct isa_driver btdriver =
{
    bt_isa_probe,
    bt_isa_attach,
    "bt"
};

/*
 * Check if the device can be found at the port given
 * and if so, set it up ready for further work
 * as an argument, takes the isa_device structure from
 * autoconf.c
 */
static int
bt_isa_probe(dev)
	struct isa_device *dev;
{
	/*
	 * find unit and check we have that many defined
	 */
	struct	bt_softc *bt;
	int	port_index;
        int	max_port_index;

	/*
	 * We ignore the unit number assigned by config to allow
	 * consistant numbering between PCI/EISA/ISA devices.
	 * This is a total kludge until we have a configuration
	 * manager.
	 */
	dev->id_unit = bt_unit;

	bt = NULL;
	port_index = 0;
	max_port_index = BT_NUM_ISAPORTS - 1;
	/*
	 * Bound our board search if the user has
	 * specified an exact port.
	 */
	bt_find_probe_range(dev->id_iobase, &port_index, &max_port_index);

	if (port_index < 0)
		return 0;

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
		dev->id_iobase = ioport;
		if (haveseen_isadev(dev, CC_IOADDR | CC_QUIET))
			continue;

		/* Allocate a softc for use during probing */
		bt = bt_alloc(dev->id_unit, I386_BUS_SPACE_IO, ioport);

		if (bt == NULL)
			break;

		/* We're going to attempt to probe it now, so mark it probed */
		bt_mark_probed_bio(port_index);

		if (bt_port_probe(bt, &info) != 0) {
			printf("bt_isa_probe: Probe failed for card at 0x%x\n",
			       ioport);
			bt_free(bt);
			continue;
		}

		dev->id_drq = info.drq;
		dev->id_irq = 0x1 << info.irq;
		dev->id_intr = bt_isa_intr;

		bt_unit++;
		return (BT_NREGS);
	}

	return (0);
}

/*
 * Attach all the sub-devices we can find
 */
static int
bt_isa_attach(dev)
	struct isa_device *dev;
{
	struct	bt_softc *bt;
	bus_dma_filter_t *filter;
	void		 *filter_arg;
	bus_addr_t	 lowaddr;

	bt = bt_softcs[dev->id_unit];
	if (dev->id_drq != -1)
		isa_dmacascade(dev->id_drq);

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
	if (bus_dma_tag_create(/*parent*/NULL, /*alignemnt*/0, /*boundary*/0,
                               lowaddr, /*highaddr*/BUS_SPACE_MAXADDR,
                               filter, filter_arg,
                               /*maxsize*/BUS_SPACE_MAXSIZE_32BIT,
                               /*nsegments*/BUS_SPACE_UNRESTRICTED,
                               /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
                               /*flags*/0, &bt->parent_dmat) != 0) {
                bt_free(bt);
                return (-1);
        }                              

        if (bt_init(bt)) {
                bt_free(bt);
                return (-1);
        }

	if (lowaddr != BUS_SPACE_MAXADDR_32BIT) {
		/* DMA tag for our sense buffers */
		if (bus_dma_tag_create(bt->parent_dmat, /*alignment*/0,
				       /*boundary*/0,
				       /*lowaddr*/BUS_SPACE_MAXADDR,
				       /*highaddr*/BUS_SPACE_MAXADDR,
				       /*filter*/NULL, /*filterarg*/NULL,
				       bt->max_ccbs
					   * sizeof(struct scsi_sense_data),
				       /*nsegments*/1,
				       /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
				       /*flags*/0, &bt->sense_dmat) != 0) {
			bt_free(bt);
			return (-1);
		}

		bt->init_level++;

		/* Allocation of sense buffers */
		if (bus_dmamem_alloc(bt->sense_dmat,
				     (void **)&bt->sense_buffers,
				     BUS_DMA_NOWAIT, &bt->sense_dmamap) != 0) {
			bt_free(bt);
			return (-1);
		}

		bt->init_level++;

		/* And permanently map them */
		bus_dmamap_load(bt->sense_dmat, bt->sense_dmamap,
       				bt->sense_buffers,
				bt->max_ccbs * sizeof(*bt->sense_buffers),
				btmapsensebuffers, bt, /*flags*/0);

		bt->init_level++;
	}

	return (bt_attach(bt));
}

/*
 * Handle an ISA interrupt.
 * XXX should go away as soon as ISA interrupt handlers
 * take a (void *) arg.
 */
static void
bt_isa_intr(void *unit)
{
	struct bt_softc* arg = bt_softcs[(int)unit];
	bt_intr((void *)arg);
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
