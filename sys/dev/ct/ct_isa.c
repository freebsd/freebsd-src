/* $FreeBSD$ */
/*	$NecBSD: ct_isa.c,v 1.6 1999/07/26 06:32:01 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define	SCSIBUS_RESCAN

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device_port.h>
#include <sys/errno.h>

#include <vm/vm.h>

#ifdef __NetBSD__
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_disk.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <machine/dvcfg.h>
#include <machine/physio_proc.h>
#include <machine/syspmgr.h>

#include <i386/Cbus/dev/scsi_low.h>

#include <dev/ic/wd33c93reg.h>
#include <i386/Cbus/dev/ct/ctvar.h>
#include <i386/Cbus/dev/ct/bshwvar.h>
#endif /* __NetBSD__ */

#ifdef __FreeBSD__
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <machine/md_var.h>

#include <pc98/pc98/pc98.h>
#include <isa/isavar.h>

#include <machine/dvcfg.h>
#include <machine/physio_proc.h>

#include <cam/scsi/scsi_low.h>

#include <dev/ic/wd33c93reg.h>
#include <dev/ct/ctvar.h>
#include <dev/ct/bshwvar.h>
#endif /* __FreeBSD__ */

#define	BSHW_IOSZ	0x08
#define	BSHW_IOBASE 	0xcc0
#define	BSHW_MEMSZ	(PAGE_SIZE * 2)

static int ct_isa_match(device_t);
static int ct_isa_attach(device_t);
static int ct_space_map(device_t, struct bshw *,
			struct resource **, struct resource **);
static void ct_space_unmap(device_t, struct ct_softc *);
static struct bshw *ct_find_hw(device_t);
static void ct_dmamap(void *, bus_dma_segment_t *, int, int);
static void ct_isa_bus_access_weight(struct ct_bus_access_handle *);
static void ct_isa_dmasync_before(struct ct_softc *);
static void ct_isa_dmasync_after(struct ct_softc *);

struct ct_isa_softc {
	struct ct_softc sc_ct;
	struct bshw_softc sc_bshw;
};

static struct isa_pnp_id ct_pnp_ids[] = {
	{ 0x0100e7b1,	"Logitec LHA-301" },
	{ 0x110154dc,	"I-O DATA SC-98III" },
	{ 0x4120acb4,	"MELCO IFC-NN" },
	{ 0,		NULL }
};

static device_method_t ct_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ct_isa_match),
	DEVMETHOD(device_attach,	ct_isa_attach),
	{ 0, 0 }
};

static driver_t ct_isa_driver = {
	"ct", ct_isa_methods, sizeof(struct ct_isa_softc),
};

static devclass_t ct_devclass;

DRIVER_MODULE(ct, isa, ct_isa_driver, ct_devclass, 0, 0);

static int
ct_isa_match(device_t dev)
{
	struct bshw *hw;
	struct resource *port_res, *mem_res;
	struct ct_bus_access_handle ch;
	int rv;

	if (ISA_PNP_PROBE(device_get_parent(dev), dev, ct_pnp_ids) == ENXIO)
		return ENXIO;

	switch (isa_get_logicalid(dev)) {
	case 0x0100e7b1:	/* LHA-301 */
	case 0x110154dc:	/* SC-98III */
	case 0x4120acb4:	/* IFC-NN */
		/* XXX - force to SMIT mode */
		device_set_flags(dev, device_get_flags(dev) | 0x40000);
		break;
	}

	if (isa_get_port(dev) == -1)
		bus_set_resource(dev, SYS_RES_IOPORT, 0,
				 BSHW_IOBASE, BSHW_IOSZ);

	if ((hw = ct_find_hw(dev)) == NULL)
		return ENXIO;
	if (ct_space_map(dev, hw, &port_res, &mem_res) != 0)
		return ENXIO;

	bzero(&ch, sizeof(ch));
	ch.ch_iot = rman_get_bustag(port_res);
	ch.ch_ioh = rman_get_bushandle(port_res),
	ch.ch_bus_weight = ct_isa_bus_access_weight;

	rv = ctprobesubr(&ch, 0, BSHW_DEFAULT_HOSTID,
			 BSHW_DEFAULT_CHIPCLK, NULL);
	if (rv != 0)
	{
		struct bshw_softc bshw_tab;
		struct bshw_softc *bs = &bshw_tab;

		memset(bs, 0, sizeof(*bs));
		bshw_read_settings(&ch, bs);
		bus_set_resource(dev, SYS_RES_IRQ, 0, bs->sc_irq, 1);
		bus_set_resource(dev, SYS_RES_DRQ, 0, bs->sc_drq, 1);
	}

	bus_release_resource(dev, SYS_RES_IOPORT, 0, port_res);
	if (mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, mem_res);

	if (rv != 0)
		return 0;
	return ENXIO;
}

static int
ct_isa_attach(device_t dev)
{
	struct ct_isa_softc *pct = device_get_softc(dev);
	struct ct_softc *ct = &pct->sc_ct;
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct bshw_softc *bs = &pct->sc_bshw;
	struct bshw *hw;
	int irq_rid, drq_rid, chiprev;
	u_int8_t *vaddr;
	bus_addr_t addr;
	intrmask_t s;

	hw = ct_find_hw(dev);
	if (ct_space_map(dev, hw, &ct->port_res, &ct->mem_res) != 0) {
		device_printf(dev, "bus io mem map failed\n");
		return ENXIO;
	}

	bzero(chp, sizeof(*chp));
	chp->ch_iot = rman_get_bustag(ct->port_res);
	chp->ch_ioh = rman_get_bushandle(ct->port_res);
	if (ct->mem_res) {
		chp->ch_memt = rman_get_bustag(ct->mem_res);
		chp->ch_memh = rman_get_bushandle(ct->mem_res);
	}
	chp->ch_bus_weight = ct_isa_bus_access_weight;

	irq_rid = 0;
	ct->irq_res = bus_alloc_resource(dev, SYS_RES_IRQ, &irq_rid, 0, ~0,
					 1, RF_ACTIVE);
	drq_rid = 0;
	ct->drq_res = bus_alloc_resource(dev, SYS_RES_DRQ, &drq_rid, 0, ~0,
					 1, RF_ACTIVE);
	if (ct->irq_res == NULL || ct->drq_res == NULL) {
		ct_space_unmap(dev, ct);
		return ENXIO;
	}

	if (ctprobesubr(chp, 0, BSHW_DEFAULT_HOSTID,
			BSHW_DEFAULT_CHIPCLK, &chiprev) == 0)
	{
		device_printf(dev, "hardware missing\n");
		ct_space_unmap(dev, ct);
		return ENXIO;
	}

	/* setup DMA map */
	if (bus_dma_tag_create(NULL, 1, 0,
			       BUS_SPACE_MAXADDR_24BIT, BUS_SPACE_MAXADDR,
			       NULL, NULL, MAXBSIZE, 1,
			       BUS_SPACE_MAXSIZE_32BIT,
			       BUS_DMA_ALLOCNOW, NULL, NULL,
			       &ct->sc_dmat) != 0) {
		device_printf(dev, "can't set up ISA DMA map\n");
		ct_space_unmap(dev, ct);
		return ENXIO;
	}

	if (bus_dmamem_alloc(ct->sc_dmat, (void **)&vaddr, BUS_DMA_NOWAIT,
			     &ct->sc_dmamapt) != 0) {
		device_printf(dev, "can't set up ISA DMA map\n");
		ct_space_unmap(dev, ct);
		return ENXIO;
	}

	bus_dmamap_load(ct->sc_dmat, ct->sc_dmamapt, vaddr, MAXBSIZE,
			ct_dmamap, &addr, BUS_DMA_NOWAIT);

	/* setup machdep softc */
	bs->sc_hw = hw;
	bs->sc_io_control = 0;
	bs->sc_bounce_phys = (u_int8_t *)addr;
	bs->sc_bounce_addr = vaddr;
	bs->sc_bounce_size = MAXBSIZE;
	bs->sc_minphys = (1 << 24);
	bs->sc_dmasync_before = ct_isa_dmasync_before;
	bs->sc_dmasync_after = ct_isa_dmasync_after;
	bshw_read_settings(chp, bs);

	/* setup ct driver softc */
	ct->ct_hw = bs;
	ct->ct_dma_xfer_start = bshw_dma_xfer_start;
	ct->ct_pio_xfer_start = bshw_smit_xfer_start;
	ct->ct_dma_xfer_stop = bshw_dma_xfer_stop;
	ct->ct_pio_xfer_stop = bshw_smit_xfer_stop;
	ct->ct_bus_reset = bshw_bus_reset;
	ct->ct_synch_setup = bshw_synch_setup;

	ct->sc_xmode = CT_XMODE_DMA;
	if (chp->ch_memh != NULL)
		ct->sc_xmode |= CT_XMODE_PIO;

	ct->sc_chiprev = chiprev;
	switch (chiprev)
	{
	case CT_WD33C93:
		/* s = "WD33C93"; */
		ct->sc_chipclk = 8;
		break;
	case CT_WD33C93_A:
		if (DVCFG_MAJOR(device_get_flags(dev)) > 0)
		{
			/* s = "AM33C93_A"; */
			ct->sc_chipclk = 20;
			ct->sc_chiprev = CT_AM33C93_A;
		}
		else
		{
			/* s = "WD33C93_A"; */
			ct->sc_chipclk = 10;
		}
		break;

	case CT_AM33C93_A:
		/* s = "AM33C93_A"; */
		ct->sc_chipclk = 20;
		break;

	default:
	case CT_WD33C93_B:
		/* s = "WD33C93_B"; */
		ct->sc_chipclk = 20;
		break;
	}
#if	0
	printf("%s: chiprev %s chipclk %d Mhz\n", 
		slp->sl_dev.dv_xname, s, ct->sc_chipclk);
#endif

	slp->sl_dev = dev;
	slp->sl_hostid = bs->sc_hostid;
	slp->sl_irq = isa_get_irq(dev);
	slp->sl_cfgflags = device_get_flags(dev);

	s = splcam();
	ctattachsubr(ct);
	splx(s);

	if (bus_setup_intr(dev, ct->irq_res, INTR_TYPE_CAM,
			   (driver_intr_t *)ctintr, ct, &ct->sc_ih)) {
		ct_space_unmap(dev, ct);
		return ENXIO;
	}

	return 0;
}

static struct bshw *
ct_find_hw(device_t dev)
{
	return DVCFG_HW(&bshw_hwsel, DVCFG_MAJOR(device_get_flags(dev)));
}

static int
ct_space_map(device_t dev, struct bshw *hw,
	     struct resource **iohp, struct resource **memhp)
{
	int port_rid, mem_rid;

	*memhp = NULL;

	port_rid = 0;
	*iohp = bus_alloc_resource(dev, SYS_RES_IOPORT, &port_rid, 0, ~0,
				   BSHW_IOSZ, RF_ACTIVE);
	if (*iohp == NULL)
		return ENXIO;

	if ((hw->hw_flags & BSHW_SMFIFO) == 0 || isa_get_maddr(dev) == -1)
		return 0;

	mem_rid = 0;
	*memhp = bus_alloc_resource(dev, SYS_RES_MEMORY, &mem_rid, 0, ~0,
				    BSHW_MEMSZ, RF_ACTIVE);
	if (*memhp == NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, port_rid, *iohp);
		return ENXIO;
	}

	return 0;
}

static void
ct_space_unmap(device_t dev, struct ct_softc *ct)
{
	if (ct->port_res != NULL)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, ct->port_res);
	if (ct->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, ct->mem_res);
	if (ct->irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, ct->irq_res);
	if (ct->drq_res != NULL)
		bus_release_resource(dev, SYS_RES_DRQ, 0, ct->drq_res);
}

static void
ct_dmamap(void *arg, bus_dma_segment_t *seg, int nseg, int error)
{
	bus_addr_t *addr = (bus_addr_t *)arg;

	*addr = seg->ds_addr;
}

static void
ct_isa_bus_access_weight(chp)
	struct ct_bus_access_handle *chp;
{

	outb(0x5f, 0);
}

static void
ct_isa_dmasync_before(ct)
	struct ct_softc *ct;
{

	if (need_pre_dma_flush)
		wbinvd();
}

static void
ct_isa_dmasync_after(ct)
	struct ct_softc *ct;
{

	if (need_post_dma_flush)
		invd();
}
