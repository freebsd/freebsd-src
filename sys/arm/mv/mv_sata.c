/*-
 * Copyright (C) 2008-2009 Semihalf
 * All rights reserved.
 *
 * Initial version developed by Ilya Bakulin. Full functionality and bringup
 * by Piotr Ziecik.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
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
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/resource.h>
#include <sys/systm.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/endian.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/ata.h>
#include <dev/ata/ata-all.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "ata_if.h"

#include "mvreg.h"
#include "mvvar.h"

/* Useful macros */
#define EDMA_TIMEOUT		100000 /* 100 ms */
#define SATA_INL(sc, reg)	ATA_INL((sc)->sc_mem_res, reg)
#define SATA_OUTL(sc, reg, val)	ATA_OUTL((sc)->sc_mem_res, reg, val)

/* HW-related data structures */
struct sata_prdentry {
	uint32_t	prd_addrlo;
	uint32_t	prd_count;
	uint32_t	prd_addrhi;
	uint32_t	prd_reserved;
};

struct sata_crqb {
	uint32_t	crqb_prdlo;
	uint32_t	crqb_prdhi;
	uint32_t	crqb_flags;
	uint16_t	crqb_count;
	uint16_t	crqb_reserved1[2];
	uint8_t		crqb_ata_command;
	uint8_t		crqb_ata_feature;
	uint8_t		crqb_ata_lba_low;
	uint8_t		crqb_ata_lba_mid;
	uint8_t		crqb_ata_lba_high;
	uint8_t		crqb_ata_device;
	uint8_t		crqb_ata_lba_low_p;
	uint8_t		crqb_ata_lba_mid_p;
	uint8_t		crqb_ata_lba_high_p;
	uint8_t		crqb_ata_feature_p;
	uint8_t		crqb_ata_count;
	uint8_t		crqb_ata_count_p;
	uint16_t	crqb_reserved2;
};

struct sata_crpb {
	uint8_t		crpb_tag;
	uint8_t		crpb_reserved;
	uint8_t		crpb_edma_status;
	uint8_t		crpb_dev_status;
	uint32_t	crpb_timestamp;
};

/* Identification section. */
struct sata_softc {
	device_t		sc_dev;
	unsigned int		sc_version;
	unsigned int		sc_edma_qlen;
	uint32_t		sc_edma_reqis_mask;
	uint32_t		sc_edma_resos_mask;
	struct resource		*sc_mem_res;
	bus_space_tag_t		sc_mem_res_bustag;
	bus_space_handle_t	sc_mem_res_bushdl;
	struct resource		*sc_irq_res;
	void			*sc_irq_cookiep;
	struct {
		void	(*function)(void *);
		void	*argument;
	} sc_interrupt[SATA_CHAN_NUM];
};

/* Controller functions */
static int	sata_probe(device_t dev);
static int	sata_attach(device_t dev);
static int	sata_detach(device_t dev);
static void	sata_intr(void*);
static struct resource * sata_alloc_resource(device_t dev, device_t child,
    int type, int *rid, u_long start, u_long end, u_long count, u_int flags);
static int	sata_release_resource(device_t dev, device_t child, int type,
    int rid, struct resource *r);
static int	sata_setup_intr(device_t dev, device_t child,
    struct resource *irq, int flags, driver_filter_t *filt,
    driver_intr_t *function, void *argument, void **cookiep);
static int	sata_teardown_intr(device_t dev, device_t child,
    struct resource *irq, void *cookie);

/* Channel functions */
static int	sata_channel_probe(device_t dev);
static int	sata_channel_attach(device_t dev);
static int	sata_channel_detach(device_t dev);
static int	sata_channel_begin_transaction(struct ata_request *request);
static int	sata_channel_end_transaction(struct ata_request *request);
static int	sata_channel_status(device_t dev);
static int	sata_channel_setmode(device_t dev, int target, int mode);
static int	sata_channel_getrev(device_t dev, int target);
static void	sata_channel_reset(device_t dev);
static void	sata_channel_dmasetprd(void *xsc, bus_dma_segment_t *segs,
    int nsegs, int error);

/* EDMA functions */
static int	sata_edma_ctrl(device_t dev, int on);
static int	sata_edma_is_running(device_t);

static device_method_t sata_methods[] = {
	/* Device method */
	DEVMETHOD(device_probe,		sata_probe),
	DEVMETHOD(device_attach,	sata_attach),
	DEVMETHOD(device_detach,	sata_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* ATA bus methods. */
	DEVMETHOD(bus_alloc_resource,		sata_alloc_resource),
	DEVMETHOD(bus_release_resource,		sata_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,		sata_setup_intr),
	DEVMETHOD(bus_teardown_intr,		sata_teardown_intr),
	{ 0, 0 },
};

static driver_t sata_driver = {
	"sata",
	sata_methods,
	sizeof(struct sata_softc),
};

devclass_t sata_devclass;

DRIVER_MODULE(sata, simplebus, sata_driver, sata_devclass, 0, 0);
MODULE_VERSION(sata, 1);
MODULE_DEPEND(sata, ata, 1, 1, 1);

static int
sata_probe(device_t dev)
{
	struct sata_softc *sc;
	uint32_t d, r;

	if (!ofw_bus_is_compatible(dev, "mrvl,sata"))
		return (ENXIO);

	soc_id(&d, &r);
	sc = device_get_softc(dev);

	switch(d) {
	case MV_DEV_88F5182:
		sc->sc_version = 1;
		sc->sc_edma_qlen = 128;
		break;
	case MV_DEV_88F6281:
	case MV_DEV_MV78100:
	case MV_DEV_MV78100_Z0:
		sc->sc_version = 2;
		sc->sc_edma_qlen = 32;
		break;
	default:
		device_printf(dev, "unsupported SoC (ID: 0x%08X)!\n", d);
		return (ENXIO);
	}

	sc->sc_edma_reqis_mask = (sc->sc_edma_qlen - 1) << SATA_EDMA_REQIS_OFS;
	sc->sc_edma_resos_mask = (sc->sc_edma_qlen - 1) << SATA_EDMA_RESOS_OFS;

	device_set_desc(dev, "Marvell Integrated SATA Controller");
	return (0);
}

static int
sata_attach(device_t dev)
{
	struct sata_softc *sc;
	int mem_id, irq_id, error, i;
	device_t ata_chan;
	uint32_t reg;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	mem_id = 0;
	irq_id = 0;

	/* Allocate resources */
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &mem_id, RF_ACTIVE);
	if (sc->sc_mem_res == NULL) {
		device_printf(dev, "could not allocate memory.\n");
		return (ENOMEM);
	}

	sc->sc_mem_res_bustag = rman_get_bustag(sc->sc_mem_res);
	sc->sc_mem_res_bushdl = rman_get_bushandle(sc->sc_mem_res);
	KASSERT(sc->sc_mem_res_bustag && sc->sc_mem_res_bushdl,
	    ("cannot get bus handle or tag."));

	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &irq_id,
	    RF_ACTIVE);
	if (sc->sc_irq_res == NULL) {
		device_printf(dev, "could not allocate IRQ.\n");
		error = ENOMEM;
		goto err;
	}

	error = bus_setup_intr(dev, sc->sc_irq_res,
	    INTR_TYPE_BIO | INTR_MPSAFE | INTR_ENTROPY,
	    NULL, sata_intr, sc, &sc->sc_irq_cookiep);
	if (error != 0) {
		device_printf(dev, "could not setup interrupt.\n");
		goto err;
	}

	/* Attach channels */
	for (i = 0; i < SATA_CHAN_NUM; i++) {
		ata_chan = device_add_child(dev, "ata",
		    devclass_find_free_unit(ata_devclass, 0));

		if (!ata_chan) {
			device_printf(dev, "cannot add channel %d.\n", i);
			error = ENOMEM;
			goto err;
		}
	}

	/* Disable interrupt coalescing */
	reg = SATA_INL(sc, SATA_CR);
	for (i = 0; i < SATA_CHAN_NUM; i++)
		reg |= SATA_CR_COALDIS(i);

	/* Disable DMA byte swapping */
	if (sc->sc_version == 2)
		reg |= SATA_CR_NODMABS | SATA_CR_NOEDMABS |
		    SATA_CR_NOPRDPBS;

	SATA_OUTL(sc, SATA_CR, reg);

	/* Clear and mask all interrupts */
	SATA_OUTL(sc, SATA_ICR, 0);
	SATA_OUTL(sc, SATA_MIMR, 0);

	return(bus_generic_attach(dev));

err:
	sata_detach(dev);
	return (error);
}

static int
sata_detach(device_t dev)
{
	struct sata_softc *sc;

	sc = device_get_softc(dev);

	if (device_is_attached(dev))
		bus_generic_detach(dev);

	if (sc->sc_mem_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->sc_mem_res), sc->sc_mem_res);
		sc->sc_mem_res = NULL;
	}

	if (sc->sc_irq_res != NULL) {
		bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_irq_cookiep);
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->sc_irq_res), sc->sc_irq_res);
		sc->sc_irq_res = NULL;
	}

	return (0);
}

static struct resource *
sata_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct sata_softc *sc;

	sc = device_get_softc(dev);

	KASSERT(type == SYS_RES_IRQ && *rid == ATA_IRQ_RID,
	    ("illegal resource request (type %u, rid %u).",
	    type, *rid));

	return (sc->sc_irq_res);
}

static int
sata_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{

	KASSERT(type == SYS_RES_IRQ && rid == ATA_IRQ_RID,
	    ("strange type %u and/or rid %u while releasing resource.", type,
	    rid));

	return (0);
}

static int
sata_setup_intr(device_t dev, device_t child, struct resource *irq, int flags,
    driver_filter_t *filt, driver_intr_t *function, void *argument,
    void **cookiep)
{
	struct sata_softc *sc;
	struct ata_channel *ch;

	sc = device_get_softc(dev);
	ch = device_get_softc(child);

	if (filt != NULL) {
		device_printf(dev, "filter interrupts are not supported.\n");
		return (EINVAL);
	}

	sc->sc_interrupt[ch->unit].function = function;
	sc->sc_interrupt[ch->unit].argument = argument;
	*cookiep = sc;

	return (0);
}

static int
sata_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie)
{
	struct sata_softc *sc;
	struct ata_channel *ch;

	sc = device_get_softc(dev);
	ch = device_get_softc(child);

	sc->sc_interrupt[ch->unit].function = NULL;
	sc->sc_interrupt[ch->unit].argument = NULL;

	return (0);
}

static void
sata_intr(void *xsc)
{
	struct sata_softc *sc;
	int unit;

	sc = xsc;

	/*
	 * Behave like ata_generic_intr() for PCI controllers.
	 * Simply invoke ISRs on all channels.
	 */
	for (unit = 0; unit < SATA_CHAN_NUM; unit++)
		if (sc->sc_interrupt[unit].function != NULL)
			sc->sc_interrupt[unit].function(
			    sc->sc_interrupt[unit].argument);
}

static int
sata_channel_probe(device_t dev)
{

	device_set_desc(dev, "Marvell Integrated SATA Channel");
	return (ata_probe(dev));
}

static int
sata_channel_attach(device_t dev)
{
	struct sata_softc *sc;
	struct ata_channel *ch;
	uint64_t work;
	int error, i;

	sc = device_get_softc(device_get_parent(dev));
	ch = device_get_softc(dev);

	if (ch->attached)
		return (0);

	ch->dev = dev;
	ch->unit = device_get_unit(dev);
	ch->flags |= ATA_USE_16BIT | ATA_NO_SLAVE | ATA_SATA;

	/* Set legacy ATA resources. */
	for (i = ATA_DATA; i <= ATA_COMMAND; i++) {
		ch->r_io[i].res = sc->sc_mem_res;
		ch->r_io[i].offset = SATA_SHADOWR_BASE(ch->unit) + (i << 2);
	}

	ch->r_io[ATA_CONTROL].res = sc->sc_mem_res;
	ch->r_io[ATA_CONTROL].offset = SATA_SHADOWR_CONTROL(ch->unit);

	ch->r_io[ATA_IDX_ADDR].res = sc->sc_mem_res;
	ata_default_registers(dev);

	/* Set SATA resources. */
	ch->r_io[ATA_SSTATUS].res = sc->sc_mem_res;
	ch->r_io[ATA_SSTATUS].offset = SATA_SATA_SSTATUS(ch->unit);
	ch->r_io[ATA_SERROR].res = sc->sc_mem_res;
	ch->r_io[ATA_SERROR].offset = SATA_SATA_SERROR(ch->unit);
	ch->r_io[ATA_SCONTROL].res = sc->sc_mem_res;
	ch->r_io[ATA_SCONTROL].offset = SATA_SATA_SCONTROL(ch->unit);
	ata_generic_hw(dev);

	ch->hw.begin_transaction = sata_channel_begin_transaction;
	ch->hw.end_transaction = sata_channel_end_transaction;
	ch->hw.status = sata_channel_status;

	/* Set DMA resources */
	ata_dmainit(dev);
	ch->dma.setprd = sata_channel_dmasetprd;

	/* Clear work area */
	KASSERT(sc->sc_edma_qlen * (sizeof(struct sata_crqb) +
	    sizeof(struct sata_crpb)) <= ch->dma.max_iosize,
	    ("insufficient DMA memory for request/response queues.\n"));
	bzero(ch->dma.work, sc->sc_edma_qlen * (sizeof(struct sata_crqb) +
	    sizeof(struct sata_crpb)));
	bus_dmamap_sync(ch->dma.work_tag, ch->dma.work_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/* Turn off EDMA engine */
	error = sata_edma_ctrl(dev, 0);
	if (error) {
		ata_dmafini(dev);
		return (error);
	}

	/*
	 * Initialize EDMA engine:
	 *	- Native Command Queuing off,
	 *	- Non-Queued operation,
	 *	- Host Queue Cache enabled.
	 */
	SATA_OUTL(sc, SATA_EDMA_CFG(ch->unit), SATA_EDMA_CFG_HQCACHE |
	    (sc->sc_version == 1) ? SATA_EDMA_CFG_QL128 : 0);

	/* Set request queue pointers */
	work = ch->dma.work_bus;
	SATA_OUTL(sc, SATA_EDMA_REQBAHR(ch->unit), work >> 32);
	SATA_OUTL(sc, SATA_EDMA_REQIPR(ch->unit), work & 0xFFFFFFFF);
	SATA_OUTL(sc, SATA_EDMA_REQOPR(ch->unit), work & 0xFFFFFFFF);

	/* Set response queue pointers */
	work += sc->sc_edma_qlen * sizeof(struct sata_crqb);
	SATA_OUTL(sc, SATA_EDMA_RESBAHR(ch->unit), work >> 32);
	SATA_OUTL(sc, SATA_EDMA_RESIPR(ch->unit), work & 0xFFFFFFFF);
	SATA_OUTL(sc, SATA_EDMA_RESOPR(ch->unit), work & 0xFFFFFFFF);

	/* Clear any outstanding interrupts */
	ATA_IDX_OUTL(ch, ATA_SERROR, ATA_IDX_INL(ch, ATA_SERROR));
	SATA_OUTL(sc, SATA_SATA_FISICR(ch->unit), 0);
	SATA_OUTL(sc, SATA_EDMA_IECR(ch->unit), 0);
	SATA_OUTL(sc, SATA_ICR,
	    ~(SATA_ICR_DEV(ch->unit) | SATA_ICR_DMADONE(ch->unit)));

	/* Umask channel interrupts */
	SATA_OUTL(sc, SATA_EDMA_IEMR(ch->unit), 0xFFFFFFFF);
	SATA_OUTL(sc, SATA_MIMR, SATA_INL(sc, SATA_MIMR) |
	    SATA_MICR_DONE(ch->unit) | SATA_MICR_DMADONE(ch->unit) |
	    SATA_MICR_ERR(ch->unit));

	ch->attached = 1;

	return (ata_attach(dev));
}

static int
sata_channel_detach(device_t dev)
{
	struct sata_softc *sc;
	struct ata_channel *ch;
	int error;

	sc = device_get_softc(device_get_parent(dev));
	ch = device_get_softc(dev);

	if (!ch->attached)
		return (0);

	/* Turn off EDMA engine */
	sata_edma_ctrl(dev, 0);

	/* Mask chanel interrupts */
	SATA_OUTL(sc, SATA_EDMA_IEMR(ch->unit), 0);
	SATA_OUTL(sc, SATA_MIMR, SATA_INL(sc, SATA_MIMR) & ~(
	    SATA_MICR_DONE(ch->unit) | SATA_MICR_DMADONE(ch->unit) |
	    SATA_MICR_ERR(ch->unit)));

	error = ata_detach(dev);
	ata_dmafini(dev);

	ch->attached = 0;

	return (error);
}

static int
sata_channel_begin_transaction(struct ata_request *request)
{
	struct sata_softc *sc;
	struct ata_channel *ch;
	struct sata_crqb *crqb;
	uint32_t req_in;
	int error, slot;

	sc = device_get_softc(device_get_parent(request->parent));
	ch = device_get_softc(request->parent);

	mtx_assert(&ch->state_mtx, MA_OWNED);

	/* Only DMA R/W goes through the EDMA machine. */
	if (request->u.ata.command != ATA_READ_DMA &&
	    request->u.ata.command != ATA_WRITE_DMA &&
	    request->u.ata.command != ATA_READ_DMA48 &&
	    request->u.ata.command != ATA_WRITE_DMA48) {

		/* Disable EDMA before accessing legacy registers */
		if (sata_edma_is_running(request->parent)) {
			error = sata_edma_ctrl(request->parent, 0);
			if (error) {
				request->result = error;
				return (ATA_OP_FINISHED);
			}
		}

		return (ata_begin_transaction(request));
	}

	/* Prepare data for DMA */
	if ((error = ch->dma.load(request, NULL, NULL))) {
		device_printf(request->parent, "setting up DMA failed!\n");
		request->result = error;
		return ATA_OP_FINISHED;
	}

	/* Get next free queue slot */
	req_in = SATA_INL(sc, SATA_EDMA_REQIPR(ch->unit));
	slot = (req_in & sc->sc_edma_reqis_mask) >> SATA_EDMA_REQIS_OFS;
	crqb = (struct sata_crqb *)(ch->dma.work +
	    (slot << SATA_EDMA_REQIS_OFS));

	/* Fill in request */
	bus_dmamap_sync(ch->dma.work_tag, ch->dma.work_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	crqb->crqb_prdlo = htole32((uint64_t)request->dma->sg_bus & 0xFFFFFFFF);
	crqb->crqb_prdhi = htole32((uint64_t)request->dma->sg_bus >> 32);
	crqb->crqb_flags = htole32((request->flags & ATA_R_READ ? 0x01 : 0x00) |
	    (request->tag << 1));

	crqb->crqb_ata_command = request->u.ata.command;
	crqb->crqb_ata_feature = request->u.ata.feature;
	crqb->crqb_ata_lba_low = request->u.ata.lba;
	crqb->crqb_ata_lba_mid = request->u.ata.lba >> 8;
	crqb->crqb_ata_lba_high = request->u.ata.lba >> 16;
	crqb->crqb_ata_device = ((request->u.ata.lba >> 24) & 0x0F) | (1 << 6);
	crqb->crqb_ata_lba_low_p = request->u.ata.lba >> 24;
	crqb->crqb_ata_lba_mid_p = request->u.ata.lba >> 32;
	crqb->crqb_ata_lba_high_p = request->u.ata.lba >> 40;
	crqb->crqb_ata_feature_p = request->u.ata.feature >> 8;
	crqb->crqb_ata_count = request->u.ata.count;
	crqb->crqb_ata_count_p = request->u.ata.count >> 8;

	bus_dmamap_sync(ch->dma.work_tag, ch->dma.work_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/* Enable EDMA if disabled */
	if (!sata_edma_is_running(request->parent)) {
		error = sata_edma_ctrl(request->parent, 1);
		if (error) {
			ch->dma.unload(request);
			request->result = error;
			return (ATA_OP_FINISHED);
		}
	}

	/* Tell EDMA about new request */
	req_in = (req_in & ~sc->sc_edma_reqis_mask) | (((slot + 1) <<
	    SATA_EDMA_REQIS_OFS) & sc->sc_edma_reqis_mask);

	SATA_OUTL(sc, SATA_EDMA_REQIPR(ch->unit), req_in);

	return (ATA_OP_CONTINUES);
}

static int
sata_channel_end_transaction(struct ata_request *request)
{
	struct sata_softc *sc;
	struct ata_channel *ch;
	struct sata_crpb *crpb;
	uint32_t res_in, res_out, icr;
	int slot;

	sc = device_get_softc(device_get_parent(request->parent));
	ch = device_get_softc(request->parent);

	mtx_assert(&ch->state_mtx, MA_OWNED);

	icr = SATA_INL(sc, SATA_ICR);
	if (icr & SATA_ICR_DMADONE(ch->unit)) {
		/* Get current response slot */
		res_out = SATA_INL(sc, SATA_EDMA_RESOPR(ch->unit));
		slot = (res_out & sc->sc_edma_resos_mask) >>
		    SATA_EDMA_RESOS_OFS;
		crpb = (struct sata_crpb *)(ch->dma.work +
		    (sc->sc_edma_qlen * sizeof(struct sata_crqb)) +
		    (slot << SATA_EDMA_RESOS_OFS));

		/* Record this request status */
		bus_dmamap_sync(ch->dma.work_tag, ch->dma.work_map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		request->status = crpb->crpb_dev_status;
		request->error = 0;

		bus_dmamap_sync(ch->dma.work_tag, ch->dma.work_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/* Update response queue pointer */
		res_out = (res_out & ~sc->sc_edma_resos_mask) | (((slot + 1) <<
		    SATA_EDMA_RESOS_OFS) & sc->sc_edma_resos_mask);

		SATA_OUTL(sc, SATA_EDMA_RESOPR(ch->unit), res_out);

		/* Ack DMA interrupt if there is nothing more to do */
		res_in = SATA_INL(sc, SATA_EDMA_RESIPR(ch->unit));
		res_in &= sc->sc_edma_resos_mask;
		res_out &= sc->sc_edma_resos_mask;

		if (res_in == res_out)
			SATA_OUTL(sc, SATA_ICR,
			    ~SATA_ICR_DMADONE(ch->unit));

		/* Update progress */
		if (!(request->status & ATA_S_ERROR) &&
		    !(request->flags & ATA_R_TIMEOUT))
			request->donecount = request->bytecount;

		/* Unload DMA data */
		ch->dma.unload(request);

		return(ATA_OP_FINISHED);
	}

	/* Legacy ATA interrupt */
	return (ata_end_transaction(request));
}

static int
sata_channel_status(device_t dev)
{
	struct sata_softc *sc;
	struct ata_channel *ch;
	uint32_t icr, iecr;

	sc = device_get_softc(device_get_parent(dev));
	ch = device_get_softc(dev);

	icr = SATA_INL(sc, SATA_ICR);
	iecr = SATA_INL(sc, SATA_EDMA_IECR(ch->unit));

	if ((icr & SATA_ICR_DEV(ch->unit)) || iecr) {
		/* Disable EDMA before accessing SATA registers */
		sata_edma_ctrl(dev, 0);
		ata_sata_phy_check_events(dev);

		/* Ack device and error interrupt */
		SATA_OUTL(sc, SATA_ICR, ~SATA_ICR_DEV(ch->unit));
		SATA_OUTL(sc, SATA_EDMA_IECR(ch->unit), 0);
	}

	icr &= SATA_ICR_DEV(ch->unit) | SATA_ICR_DMADONE(ch->unit);
	return (icr);
}

static void
sata_channel_reset(device_t dev)
{
	struct sata_softc *sc;
	struct ata_channel *ch;

	sc = device_get_softc(device_get_parent(dev));
	ch = device_get_softc(dev);

	/* Disable EDMA before using legacy registers */
	sata_edma_ctrl(dev, 0);

	/* Mask all EDMA interrups */
	SATA_OUTL(sc, SATA_EDMA_IEMR(ch->unit), 0);

	/* Reset EDMA */
	SATA_OUTL(sc, SATA_EDMA_CMD(ch->unit), SATA_EDMA_CMD_RESET);
	DELAY(25);
	SATA_OUTL(sc, SATA_EDMA_CMD(ch->unit), 0);

	/* Reset PHY and device */
	if (ata_sata_phy_reset(dev, -1, 1))
		ata_generic_reset(dev);
	else
		ch->devices = 0;

	/* Clear EDMA errors */
	SATA_OUTL(sc, SATA_SATA_FISICR(ch->unit), 0);
	SATA_OUTL(sc, SATA_EDMA_IECR(ch->unit), 0);

	/* Unmask all EDMA interrups */
	SATA_OUTL(sc, SATA_EDMA_IEMR(ch->unit), 0xFFFFFFFF);
}

static int
sata_channel_setmode(device_t parent, int target, int mode)
{

	/* Disable EDMA before using legacy registers */
	sata_edma_ctrl(parent, 0);
	return (ata_sata_setmode(parent, target, mode));
}

static int
sata_channel_getrev(device_t parent, int target)
{

	/* Disable EDMA before using legacy registers */
	sata_edma_ctrl(parent, 0);
	return (ata_sata_getrev(parent, target));
}

static void
sata_channel_dmasetprd(void *xsc, bus_dma_segment_t *segs, int nsegs,
    int error)
{
	struct ata_dmasetprd_args *args;
	struct sata_prdentry *prd;
	int i;

	args = xsc;
	prd = args->dmatab;

	if ((args->error = error))
		return;

	for (i = 0; i < nsegs; i++) {
		prd[i].prd_addrlo = htole32(segs[i].ds_addr);
		prd[i].prd_addrhi = htole32((uint64_t)segs[i].ds_addr >> 32);
		prd[i].prd_count = htole32(segs[i].ds_len);
	}

	prd[i - 1].prd_count |= htole32(ATA_DMA_EOT);
	KASSERT(nsegs <= ATA_DMA_ENTRIES, ("too many DMA segment entries.\n"));
	args->nsegs = nsegs;
}

static int
sata_edma_ctrl(device_t dev, int on)
{
	struct sata_softc *sc;
	struct ata_channel *ch;
	int bit, timeout;
	uint32_t reg;

	sc = device_get_softc(device_get_parent(dev));
	ch = device_get_softc(dev);
	bit = on ? SATA_EDMA_CMD_ENABLE : SATA_EDMA_CMD_DISABLE;
	timeout = EDMA_TIMEOUT;

	SATA_OUTL(sc, SATA_EDMA_CMD(ch->unit), bit);

	while (1) {
		DELAY(1);

		reg = SATA_INL(sc, SATA_EDMA_CMD(ch->unit));

		/* Enable bit will be 1 after disable command completion */
		if (on && (reg & SATA_EDMA_CMD_ENABLE))
			break;

		/* Disable bit will be 0 after disable command completion */
		if (!on && !(reg & SATA_EDMA_CMD_DISABLE))
			break;

		if (timeout-- <= 0) {
			device_printf(dev, "EDMA command timeout!\n");
			return (ETIMEDOUT);
		}
	}

	return (0);
}

static int
sata_edma_is_running(device_t dev)
{
	struct sata_softc *sc;
	struct ata_channel *ch;

	sc = device_get_softc(device_get_parent(dev));
	ch = device_get_softc(dev);

	return (SATA_INL(sc, SATA_EDMA_CMD(ch->unit)) & SATA_EDMA_CMD_ENABLE);
}

static device_method_t sata_channel_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,		sata_channel_probe),
	DEVMETHOD(device_attach,	sata_channel_attach),
	DEVMETHOD(device_detach,	sata_channel_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	ata_suspend),
	DEVMETHOD(device_resume,	ata_resume),

	/* ATA channel interface */
	DEVMETHOD(ata_reset,		sata_channel_reset),
	DEVMETHOD(ata_setmode,		sata_channel_setmode),
	DEVMETHOD(ata_getrev,		sata_channel_getrev),
	{ 0, 0 }
};

driver_t sata_channel_driver = {
	"ata",
	sata_channel_methods,
	sizeof(struct ata_channel),
};

DRIVER_MODULE(ata, sata, sata_channel_driver, ata_devclass, 0, 0);
