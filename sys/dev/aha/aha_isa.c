/*
 * Product specific probe and attach routines for:
 *      Adaptec 154x.
 */
/*-
 * Copyright (c) 1999-2003 M. Warner Losh
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
 * Derived from bt isa from end, written by:
 *
 * Copyright (c) 1998 Justin T. Gibbs
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <isa/isavar.h>

#include <dev/aha/ahareg.h>

#include <cam/scsi/scsi_all.h>

static struct isa_pnp_id aha_ids[] = {
	{ADP0100_PNP,		"Adaptec 1540/1542 ISA SCSI"},	/* ADP0100 */
	{AHA1540_PNP,		"Adaptec 1540/aha-1640/aha-1535"},/* ADP1540 */
	{AHA1542_PNP,		"Adaptec 1542/aha-1535"},	/* ADP1542 */
	{AHA1542_PNPCOMPAT,	"Adaptec 1542 compatible"},	/* PNP00A0 */
	{ICU0091_PNP,		"Adaptec AHA-1540/1542 SCSI"},	/* ICU0091 */
	{0}
};

/*
 * I/O ports listed in the order enumerated by the card for certain op codes.
 */
static bus_addr_t aha_board_ports[] =
{
	0x330,
	0x334,
	0x230,
	0x234,
	0x130,
	0x134
};

/*
 * Check if the device can be found at the port given
 */
static int
aha_isa_probe(device_t dev)
{
	/*
	 * find unit and check we have that many defined
	 */
	struct	aha_softc *aha = device_get_softc(dev);
	int	error;
	u_long	port_start;
	struct resource *port_res;
	int	port_rid;
	int	drq;
	int	irq;
	config_data_t config_data;

	aha->dev = dev;
	/* Check isapnp ids */
	if (ISA_PNP_PROBE(device_get_parent(dev), dev, aha_ids) == ENXIO)
		return (ENXIO);

	port_rid = 0;
	port_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &port_rid,
	    0, ~0, AHA_NREGS, RF_ACTIVE);

	if (port_res == NULL)
		return (ENXIO);

	port_start = rman_get_start(port_res);
	aha_alloc(aha, device_get_unit(dev), rman_get_bustag(port_res),
	    rman_get_bushandle(port_res));

	/* See if there is really a card present */
	if (aha_probe(aha) || aha_fetch_adapter_info(aha)) {
		aha_free(aha);
		bus_release_resource(dev, SYS_RES_IOPORT, port_rid, port_res);
		return (ENXIO);
	}

	/*
	 * Determine our IRQ, and DMA settings and
	 * export them to the configuration system.
	 */
	error = aha_cmd(aha, AOP_INQUIRE_CONFIG, NULL, /*parmlen*/0,
	    (uint8_t*)&config_data, sizeof(config_data), DEFAULT_CMD_TIMEOUT);

	if (error != 0) {
		device_printf(dev, "Could not determine IRQ or DMA "
		    "settings for adapter at %#jx.  Failing probe\n",
		    (uintmax_t)port_start);
		aha_free(aha);
		bus_release_resource(dev, SYS_RES_IOPORT, port_rid,
		    port_res);
		return (ENXIO);
	}

	bus_release_resource(dev, SYS_RES_IOPORT, port_rid, port_res);

	switch (config_data.dma_chan) {
	case DMA_CHAN_5:
		drq = 5;
		break;
	case DMA_CHAN_6:
		drq = 6;
		break;
	case DMA_CHAN_7:
		drq = 7;
		break;
	default:
		device_printf(dev, "Invalid DMA setting for adapter at %#jx.",
		    (uintmax_t)port_start);
		return (ENXIO);
	}
	error = bus_set_resource(dev, SYS_RES_DRQ, 0, drq, 1);
	if (error)
		return error;

	irq = ffs(config_data.irq) + 8;
	error = bus_set_resource(dev, SYS_RES_IRQ, 0, irq, 1);
	return (error);
}

/*
 * Attach all the sub-devices we can find
 */
static int
aha_isa_attach(device_t dev)
{
	struct	aha_softc *aha = device_get_softc(dev);
	bus_dma_filter_t *filter;
	void		 *filter_arg;
	bus_addr_t	 lowaddr;
	void		 *ih;
	int		 error = ENOMEM;
	int		 aha_free_needed = 0;

	aha->dev = dev;
	aha->portrid = 0;
	aha->port = bus_alloc_resource(dev, SYS_RES_IOPORT, &aha->portrid,
	    0, ~0, AHA_NREGS, RF_ACTIVE);
	if (!aha->port) {
		device_printf(dev, "Unable to allocate I/O ports\n");
		goto fail;
	}

	aha->irqrid = 0;
	aha->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &aha->irqrid,
	    RF_ACTIVE);
	if (!aha->irq) {
		device_printf(dev, "Unable to allocate excluse use of irq\n");
		goto fail;
	}

	aha->drqrid = 0;
	aha->drq = bus_alloc_resource_any(dev, SYS_RES_DRQ, &aha->drqrid,
	    RF_ACTIVE);
	if (!aha->drq) {
		device_printf(dev, "Unable to allocate drq\n");
		goto fail;
	}

#if 0				/* is the drq ever unset? */
	if (dev->id_drq != -1)
		isa_dmacascade(dev->id_drq);
#endif
	isa_dmacascade(rman_get_start(aha->drq));

	/* Allocate our parent dmatag */
	filter = NULL;
	filter_arg = NULL;
	lowaddr = BUS_SPACE_MAXADDR_24BIT;

	if (bus_dma_tag_create(	/* parent	*/ NULL,
				/* alignemnt	*/ 1,
				/* boundary	*/ 0,
				/* lowaddr	*/ lowaddr,
				/* highaddr	*/ BUS_SPACE_MAXADDR,
				/* filter	*/ filter,
				/* filterarg	*/ filter_arg,
				/* maxsize	*/ BUS_SPACE_MAXSIZE_24BIT,
				/* nsegments	*/ ~0,
				/* maxsegsz	*/ BUS_SPACE_MAXSIZE_24BIT,
				/* flags	*/ 0,
				/* lockfunc	*/ busdma_lock_mutex,
				/* lockarg	*/ &Giant,
				&aha->parent_dmat) != 0) {
		device_printf(dev, "dma tag create failed.\n");
		goto fail;
        }                              

	if (aha_init(aha)) {
		device_printf(dev, "init failed\n");
		goto fail;
        }
	/*
	 * The 1542A and B look the same.  So we guess based on
	 * the firmware revision.  It appears that only rev 0 is on
	 * the A cards.
	 */
	if (aha->boardid <= BOARD_1542 && aha->fw_major == 0) {
		device_printf(dev, "154xA may not work\n");
		aha->ccb_sg_opcode = INITIATOR_SG_CCB;
		aha->ccb_ccb_opcode = INITIATOR_CCB;
	}
	aha_free_needed++;
	
	error = aha_attach(aha);
	if (error) {
		device_printf(dev, "attach failed\n");
		goto fail;
	}

	error = bus_setup_intr(dev, aha->irq, INTR_TYPE_CAM|INTR_ENTROPY,
	    aha_intr, aha, &ih);
	if (error) {
		device_printf(dev, "Unable to register interrupt handler\n");
                goto fail;
	}

	return (0);
fail: ;
	bus_free_resource(dev, SYS_RES_IOPORT, aha->port);
	bus_free_resource(dev, SYS_RES_IRQ, aha->irq);
	bus_free_resource(dev, SYS_RES_DRQ, aha->drq);
	if (aha_free_needed)
		aha_free(aha);
	return (error);
}

static int
aha_isa_detach(device_t dev)
{
	struct aha_softc *aha = (struct aha_softc *)device_get_softc(dev);
	int error;

	error = bus_teardown_intr(dev, aha->irq, aha->ih);
	if (error)
		device_printf(dev, "failed to unregister interrupt handler\n");

	bus_free_resource(dev, SYS_RES_IOPORT, aha->port);
	bus_free_resource(dev, SYS_RES_IRQ, aha->irq);
	bus_free_resource(dev, SYS_RES_DRQ, aha->drq);

	error = aha_detach(aha);
	if (error) {
		device_printf(dev, "detach failed\n");
		return (error);
	}
	aha_free(aha);

	return (0);
}

static void
aha_isa_identify(driver_t *driver, device_t parent)
{
	int i;
	bus_addr_t ioport;
	struct aha_softc aha;
	int rid;
	struct resource *res;
	device_t child;

	/* Attempt to find an adapter */
	for (i = 0; i < sizeof(aha_board_ports) / sizeof(aha_board_ports[0]);
	    i++) {
		bzero(&aha, sizeof(aha));
		ioport = aha_board_ports[i];
		/*
		 * XXX Check to see if we have a hard-wired aha device at
		 * XXX this port, if so, skip.  This should also cover the
		 * XXX case where we are run multiple times due to, eg,
		 * XXX kldload/kldunload.
		 */
		rid = 0;
		res = bus_alloc_resource(parent, SYS_RES_IOPORT, &rid,
		    ioport, ioport, AHA_NREGS, RF_ACTIVE);
		if (res == NULL)
			continue;
		aha_alloc(&aha, -1, rman_get_bustag(res),
		    rman_get_bushandle(res));
		/* See if there is really a card present */
		if (aha_probe(&aha) || aha_fetch_adapter_info(&aha))
			goto not_this_one;
		child = BUS_ADD_CHILD(parent, ISA_ORDER_SPECULATIVE, "aha", -1);
		bus_set_resource(child, SYS_RES_IOPORT, 0, ioport, AHA_NREGS);
		/*
		 * Could query the board and set IRQ/DRQ, but probe does
		 * that.
		 */
	not_this_one:;
		bus_release_resource(parent, SYS_RES_IOPORT, rid, res);
		aha_free(&aha);
	}
}

static device_method_t aha_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aha_isa_probe),
	DEVMETHOD(device_attach,	aha_isa_attach),
	DEVMETHOD(device_detach,	aha_isa_detach),
	DEVMETHOD(device_identify,	aha_isa_identify),

	{ 0, 0 }
};

static driver_t aha_isa_driver = {
	"aha",
	aha_isa_methods,
	sizeof(struct aha_softc),
};

static devclass_t aha_devclass;

DRIVER_MODULE(aha, isa, aha_isa_driver, aha_devclass, 0, 0);
