/*-
 * Copyright (c) 1998 - 2008 Søren Schmidt <sos@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ata.h"
#include <sys/param.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ata.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/stdarg.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-pci.h>
#include <ata_if.h>

/* local prototypes */
static int ata_marvell_chipinit(device_t dev);
static int ata_marvell_ch_attach(device_t dev);
static int ata_marvell_setmode(device_t dev, int target, int mode);
static int ata_marvell_edma_ch_attach(device_t dev);
static int ata_marvell_edma_ch_detach(device_t dev);
static int ata_marvell_edma_status(device_t dev);
static int ata_marvell_edma_begin_transaction(struct ata_request *request);
static int ata_marvell_edma_end_transaction(struct ata_request *request);
static void ata_marvell_edma_reset(device_t dev);
static void ata_marvell_edma_dmasetprd(void *xsc, bus_dma_segment_t *segs, int nsegs, int error);
static void ata_marvell_edma_dmainit(device_t dev);

/* misc defines */
#define MV_50XX		50
#define MV_60XX		60
#define MV_6042		62
#define MV_7042		72
#define MV_61XX		61


/*
 * Marvell chipset support functions
 */
#define ATA_MV_HOST_BASE(ch) \
	((ch->unit & 3) * 0x0100) + (ch->unit > 3 ? 0x30000 : 0x20000)
#define ATA_MV_EDMA_BASE(ch) \
	((ch->unit & 3) * 0x2000) + (ch->unit > 3 ? 0x30000 : 0x20000)

struct ata_marvell_response {
    u_int16_t   tag;
    u_int8_t    edma_status;
    u_int8_t    dev_status;
    u_int32_t   timestamp;
};

struct ata_marvell_dma_prdentry {
    u_int32_t addrlo;
    u_int32_t count;
    u_int32_t addrhi;
    u_int32_t reserved;
};  

static int
ata_marvell_probe(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    static struct ata_chip_id ids[] =
    {{ ATA_M88SX5040, 0, 4, MV_50XX, ATA_SA150, "88SX5040" },
     { ATA_M88SX5041, 0, 4, MV_50XX, ATA_SA150, "88SX5041" },
     { ATA_M88SX5080, 0, 8, MV_50XX, ATA_SA150, "88SX5080" },
     { ATA_M88SX5081, 0, 8, MV_50XX, ATA_SA150, "88SX5081" },
     { ATA_M88SX6041, 0, 4, MV_60XX, ATA_SA300, "88SX6041" },
     { ATA_M88SX6042, 0, 4, MV_6042, ATA_SA300, "88SX6042" },
     { ATA_M88SX6081, 0, 8, MV_60XX, ATA_SA300, "88SX6081" },
     { ATA_M88SX7042, 0, 4, MV_7042, ATA_SA300, "88SX7042" },
     { ATA_M88SX6101, 0, 0, MV_61XX, ATA_UDMA6, "88SX6101" },
     { ATA_M88SX6102, 0, 0, MV_61XX, ATA_UDMA6, "88SX6102" },
     { ATA_M88SX6111, 0, 1, MV_61XX, ATA_UDMA6, "88SX6111" },
     { ATA_M88SX6121, 0, 2, MV_61XX, ATA_UDMA6, "88SX6121" },
     { ATA_M88SX6141, 0, 4, MV_61XX, ATA_UDMA6, "88SX6141" },
     { ATA_M88SX6145, 0, 4, MV_61XX, ATA_UDMA6, "88SX6145" },
     { 0, 0, 0, 0, 0, 0}};

    if (pci_get_vendor(dev) != ATA_MARVELL_ID)
	return ENXIO;

    if (!(ctlr->chip = ata_match_chip(dev, ids)))
	return ENXIO;

    ata_set_desc(dev);

    switch (ctlr->chip->cfg2) {
    case MV_50XX:
    case MV_60XX:
    case MV_6042:
    case MV_7042:
	ctlr->chipinit = ata_marvell_edma_chipinit;
	break;
    case MV_61XX:
	ctlr->chipinit = ata_marvell_chipinit;
	break;
    }
    return (BUS_PROBE_DEFAULT);
}

static int
ata_marvell_chipinit(device_t dev)
{
	struct ata_pci_controller *ctlr = device_get_softc(dev);
	device_t child;

	if (ata_setup_interrupt(dev, ata_generic_intr))
		return ENXIO;
	/* Create AHCI subdevice if AHCI part present. */
	if (ctlr->chip->cfg1) {
	    	child = device_add_child(dev, NULL, -1);
		if (child != NULL) {
		    device_set_ivars(child, (void *)(intptr_t)-1);
		    bus_generic_attach(dev);
		}
	}
        ctlr->ch_attach = ata_marvell_ch_attach;
	ctlr->ch_detach = ata_pci_ch_detach;
	ctlr->reset = ata_generic_reset;
        ctlr->setmode = ata_marvell_setmode;
        ctlr->channels = 1;
        return (0);
}

static int
ata_marvell_ch_attach(device_t dev)
{
	struct ata_channel *ch = device_get_softc(dev);
	int error;
 
	error = ata_pci_ch_attach(dev);
    	/* dont use 32 bit PIO transfers */
	ch->flags |= ATA_USE_16BIT;
	ch->flags |= ATA_CHECKS_CABLE;
	return (error);
}

static int
ata_marvell_setmode(device_t dev, int target, int mode)
{
	struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
	struct ata_channel *ch = device_get_softc(dev);

	mode = min(mode, ctlr->chip->max_dma);
	/* Check for 80pin cable present. */
	if (mode > ATA_UDMA2 && ATA_IDX_INB(ch, ATA_BMDEVSPEC_0) & 0x01) {
		ata_print_cable(dev, "controller");
		mode = ATA_UDMA2;
	}
	/* Nothing to do to setup mode, the controller snoop SET_FEATURE cmd. */
	return (mode);
}

int
ata_marvell_edma_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev, ata_generic_intr))
	return ENXIO;

    ctlr->r_type1 = SYS_RES_MEMORY;
    ctlr->r_rid1 = PCIR_BAR(0);
    if (!(ctlr->r_res1 = bus_alloc_resource_any(dev, ctlr->r_type1,
						&ctlr->r_rid1, RF_ACTIVE)))
	return ENXIO;

    /* mask all host controller interrupts */
    ATA_OUTL(ctlr->r_res1, 0x01d64, 0x00000000);

    /* mask all PCI interrupts */
    ATA_OUTL(ctlr->r_res1, 0x01d5c, 0x00000000);

    ctlr->ch_attach = ata_marvell_edma_ch_attach;
    ctlr->ch_detach = ata_marvell_edma_ch_detach;
    ctlr->reset = ata_marvell_edma_reset;
    ctlr->setmode = ata_sata_setmode;
    ctlr->getrev = ata_sata_getrev;
    ctlr->channels = ctlr->chip->cfg1;

    /* clear host controller interrupts */
    ATA_OUTL(ctlr->r_res1, 0x20014, 0x00000000);
    if (ctlr->chip->cfg1 > 4)
	ATA_OUTL(ctlr->r_res1, 0x30014, 0x00000000);

    /* clear PCI interrupts */
    ATA_OUTL(ctlr->r_res1, 0x01d58, 0x00000000);

    /* unmask PCI interrupts we want */
    ATA_OUTL(ctlr->r_res1, 0x01d5c, 0x007fffff);

    /* unmask host controller interrupts we want */
    ATA_OUTL(ctlr->r_res1, 0x01d64, 0x000000ff/*HC0*/ | 0x0001fe00/*HC1*/ |
	     /*(1<<19) | (1<<20) | (1<<21) |*/(1<<22) | (1<<24) | (0x7f << 25));

    return 0;
}

static int
ata_marvell_edma_ch_attach(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    u_int64_t work;
    int i;

    ata_marvell_edma_dmainit(dev);
    work = ch->dma.work_bus;
    /* clear work area */
    bzero(ch->dma.work, 1024+256);
    bus_dmamap_sync(ch->dma.work_tag, ch->dma.work_map,
	BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

    /* set legacy ATA resources */
    for (i = ATA_DATA; i <= ATA_COMMAND; i++) {
	ch->r_io[i].res = ctlr->r_res1;
	ch->r_io[i].offset = 0x02100 + (i << 2) + ATA_MV_EDMA_BASE(ch);
    }
    ch->r_io[ATA_CONTROL].res = ctlr->r_res1;
    ch->r_io[ATA_CONTROL].offset = 0x02120 + ATA_MV_EDMA_BASE(ch);
    ch->r_io[ATA_IDX_ADDR].res = ctlr->r_res1;
    ata_default_registers(dev);

    /* set SATA resources */
    switch (ctlr->chip->cfg2) {
    case MV_50XX:
	ch->r_io[ATA_SSTATUS].res = ctlr->r_res1;
	ch->r_io[ATA_SSTATUS].offset =  0x00100 + ATA_MV_HOST_BASE(ch);
	ch->r_io[ATA_SERROR].res = ctlr->r_res1;
	ch->r_io[ATA_SERROR].offset = 0x00104 + ATA_MV_HOST_BASE(ch);
	ch->r_io[ATA_SCONTROL].res = ctlr->r_res1;
	ch->r_io[ATA_SCONTROL].offset = 0x00108 + ATA_MV_HOST_BASE(ch);
	break;
    case MV_60XX:
    case MV_6042:
    case MV_7042:
	ch->r_io[ATA_SSTATUS].res = ctlr->r_res1;
	ch->r_io[ATA_SSTATUS].offset =  0x02300 + ATA_MV_EDMA_BASE(ch);
	ch->r_io[ATA_SERROR].res = ctlr->r_res1;
	ch->r_io[ATA_SERROR].offset = 0x02304 + ATA_MV_EDMA_BASE(ch);
	ch->r_io[ATA_SCONTROL].res = ctlr->r_res1;
	ch->r_io[ATA_SCONTROL].offset = 0x02308 + ATA_MV_EDMA_BASE(ch);
	ch->r_io[ATA_SACTIVE].res = ctlr->r_res1;
	ch->r_io[ATA_SACTIVE].offset = 0x02350 + ATA_MV_EDMA_BASE(ch);
	break;
    }

    ch->flags |= ATA_NO_SLAVE;
    ch->flags |= ATA_USE_16BIT; /* XXX SOS needed ? */
    ch->flags |= ATA_SATA;
    ata_generic_hw(dev);
    ch->hw.begin_transaction = ata_marvell_edma_begin_transaction;
    ch->hw.end_transaction = ata_marvell_edma_end_transaction;
    ch->hw.status = ata_marvell_edma_status;

    /* disable the EDMA machinery */
    ATA_OUTL(ctlr->r_res1, 0x02028 + ATA_MV_EDMA_BASE(ch), 0x00000002);
    DELAY(100000);       /* SOS should poll for disabled */

    /* set configuration to non-queued 128b read transfers stop on error */
    ATA_OUTL(ctlr->r_res1, 0x02000 + ATA_MV_EDMA_BASE(ch), (1<<11) | (1<<13));

    /* request queue base high */
    ATA_OUTL(ctlr->r_res1, 0x02010 + ATA_MV_EDMA_BASE(ch), work >> 32);

    /* request queue in ptr */
    ATA_OUTL(ctlr->r_res1, 0x02014 + ATA_MV_EDMA_BASE(ch), work & 0xffffffff);

    /* request queue out ptr */
    ATA_OUTL(ctlr->r_res1, 0x02018 + ATA_MV_EDMA_BASE(ch), 0x0);

    /* response queue base high */
    work += 1024;
    ATA_OUTL(ctlr->r_res1, 0x0201c + ATA_MV_EDMA_BASE(ch), work >> 32);

    /* response queue in ptr */
    ATA_OUTL(ctlr->r_res1, 0x02020 + ATA_MV_EDMA_BASE(ch), 0x0);

    /* response queue out ptr */
    ATA_OUTL(ctlr->r_res1, 0x02024 + ATA_MV_EDMA_BASE(ch), work & 0xffffffff);

    /* clear SATA error register */
    ATA_IDX_OUTL(ch, ATA_SERROR, ATA_IDX_INL(ch, ATA_SERROR));

    /* clear any outstanding error interrupts */
    ATA_OUTL(ctlr->r_res1, 0x02008 + ATA_MV_EDMA_BASE(ch), 0x0);

    /* unmask all error interrupts */
    ATA_OUTL(ctlr->r_res1, 0x0200c + ATA_MV_EDMA_BASE(ch), ~0x0);
    
    /* enable EDMA machinery */
    ATA_OUTL(ctlr->r_res1, 0x02028 + ATA_MV_EDMA_BASE(ch), 0x00000001);
    return 0;
}

static int
ata_marvell_edma_ch_detach(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    if (ch->dma.work_tag && ch->dma.work_map)
	bus_dmamap_sync(ch->dma.work_tag, ch->dma.work_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
    ata_dmafini(dev);
    return (0);
}

static int
ata_marvell_edma_status(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    u_int32_t cause = ATA_INL(ctlr->r_res1, 0x01d60);
    int shift = (ch->unit << 1) + (ch->unit > 3);

    if (cause & (1 << shift)) {

	/* clear interrupt(s) */
	ATA_OUTL(ctlr->r_res1, 0x02008 + ATA_MV_EDMA_BASE(ch), 0x0);

	/* do we have any PHY events ? */
	ata_sata_phy_check_events(dev);
    }

    /* do we have any device action ? */
    return (cause & (2 << shift));
}

/* must be called with ATA channel locked and state_mtx held */
static int
ata_marvell_edma_begin_transaction(struct ata_request *request)
{
    struct ata_pci_controller *ctlr=device_get_softc(device_get_parent(request->parent));
    struct ata_channel *ch = device_get_softc(request->parent);
    u_int32_t req_in;
    u_int8_t *bytep;
    int i;
    int error, slot;

    /* only DMA R/W goes through the EMDA machine */
    if (request->u.ata.command != ATA_READ_DMA &&
	request->u.ata.command != ATA_WRITE_DMA &&
	request->u.ata.command != ATA_READ_DMA48 &&
	request->u.ata.command != ATA_WRITE_DMA48) {

	/* disable the EDMA machinery */
	if (ATA_INL(ctlr->r_res1, 0x02028 + ATA_MV_EDMA_BASE(ch)) & 0x00000001)
	    ATA_OUTL(ctlr->r_res1, 0x02028 + ATA_MV_EDMA_BASE(ch), 0x00000002);
	return ata_begin_transaction(request);
    }

    /* check sanity, setup SG list and DMA engine */
    if ((error = ch->dma.load(request, NULL, NULL))) {
	device_printf(request->parent, "setting up DMA failed\n");
	request->result = error;
	return ATA_OP_FINISHED;
    }

    /* get next free request queue slot */
    req_in = ATA_INL(ctlr->r_res1, 0x02014 + ATA_MV_EDMA_BASE(ch));
    slot = (((req_in & ~0xfffffc00) >> 5) + 0) & 0x1f;
    bytep = (u_int8_t *)(ch->dma.work);
    bytep += (slot << 5);

    /* fill in this request */
    le32enc(bytep + 0 * sizeof(u_int32_t),
	request->dma->sg_bus & 0xffffffff);
    le32enc(bytep + 1 * sizeof(u_int32_t),
	(u_int64_t)request->dma->sg_bus >> 32);
    if (ctlr->chip->cfg2 != MV_6042 && ctlr->chip->cfg2 != MV_7042) {
	    le16enc(bytep + 4 * sizeof(u_int16_t),
		(request->flags & ATA_R_READ ? 0x01 : 0x00) | (request->tag << 1));

	    i = 10;
	    bytep[i++] = (request->u.ata.count >> 8) & 0xff;
	    bytep[i++] = 0x10 | ATA_COUNT;
	    bytep[i++] = request->u.ata.count & 0xff;
	    bytep[i++] = 0x10 | ATA_COUNT;

	    bytep[i++] = (request->u.ata.lba >> 24) & 0xff;
	    bytep[i++] = 0x10 | ATA_SECTOR;
	    bytep[i++] = request->u.ata.lba & 0xff;
	    bytep[i++] = 0x10 | ATA_SECTOR;

	    bytep[i++] = (request->u.ata.lba >> 32) & 0xff;
	    bytep[i++] = 0x10 | ATA_CYL_LSB;
	    bytep[i++] = (request->u.ata.lba >> 8) & 0xff;
	    bytep[i++] = 0x10 | ATA_CYL_LSB;

	    bytep[i++] = (request->u.ata.lba >> 40) & 0xff;
	    bytep[i++] = 0x10 | ATA_CYL_MSB;
	    bytep[i++] = (request->u.ata.lba >> 16) & 0xff;
	    bytep[i++] = 0x10 | ATA_CYL_MSB;

	    bytep[i++] = ATA_D_LBA | ATA_D_IBM | ((request->u.ata.lba >> 24) & 0xf);
	    bytep[i++] = 0x10 | ATA_DRIVE;

	    bytep[i++] = request->u.ata.command;
	    bytep[i++] = 0x90 | ATA_COMMAND;
    } else {
	    le32enc(bytep + 2 * sizeof(u_int32_t),
		(request->flags & ATA_R_READ ? 0x01 : 0x00) | (request->tag << 1));

	    i = 16;
	    bytep[i++] = 0;
	    bytep[i++] = 0;
	    bytep[i++] = request->u.ata.command;
	    bytep[i++] = request->u.ata.feature & 0xff;

	    bytep[i++] = request->u.ata.lba & 0xff;
	    bytep[i++] = (request->u.ata.lba >> 8) & 0xff;
	    bytep[i++] = (request->u.ata.lba >> 16) & 0xff;
	    bytep[i++] = ATA_D_LBA | ATA_D_IBM | ((request->u.ata.lba >> 24) & 0x0f);

	    bytep[i++] = (request->u.ata.lba >> 24) & 0xff;
	    bytep[i++] = (request->u.ata.lba >> 32) & 0xff;
	    bytep[i++] = (request->u.ata.lba >> 40) & 0xff;
	    bytep[i++] = (request->u.ata.feature >> 8) & 0xff;

	    bytep[i++] = request->u.ata.count & 0xff;
	    bytep[i++] = (request->u.ata.count >> 8) & 0xff;
	    bytep[i++] = 0;
	    bytep[i++] = 0;
    }

    bus_dmamap_sync(ch->dma.work_tag, ch->dma.work_map,
	BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

    /* enable EDMA machinery if needed */
    if (!(ATA_INL(ctlr->r_res1, 0x02028 + ATA_MV_EDMA_BASE(ch)) & 0x00000001)) {
	ATA_OUTL(ctlr->r_res1, 0x02028 + ATA_MV_EDMA_BASE(ch), 0x00000001);
	while (!(ATA_INL(ctlr->r_res1,
			 0x02028 + ATA_MV_EDMA_BASE(ch)) & 0x00000001))
	    DELAY(10);
    }

    /* tell EDMA it has a new request */
    slot = (((req_in & ~0xfffffc00) >> 5) + 1) & 0x1f;
    req_in &= 0xfffffc00;
    req_in += (slot << 5);
    ATA_OUTL(ctlr->r_res1, 0x02014 + ATA_MV_EDMA_BASE(ch), req_in);
   
    return ATA_OP_CONTINUES;
}

/* must be called with ATA channel locked and state_mtx held */
static int
ata_marvell_edma_end_transaction(struct ata_request *request)
{
    struct ata_pci_controller *ctlr=device_get_softc(device_get_parent(request->parent));
    struct ata_channel *ch = device_get_softc(request->parent);
    int offset = (ch->unit > 3 ? 0x30014 : 0x20014);
    u_int32_t icr = ATA_INL(ctlr->r_res1, offset);
    int res;

    /* EDMA interrupt */
    if ((icr & (0x0001 << (ch->unit & 3)))) {
	struct ata_marvell_response *response;
	u_int32_t rsp_in, rsp_out;
	int slot;

	/* stop timeout */
	callout_stop(&request->callout);

	/* get response ptr's */
	rsp_in = ATA_INL(ctlr->r_res1, 0x02020 + ATA_MV_EDMA_BASE(ch));
	rsp_out = ATA_INL(ctlr->r_res1, 0x02024 + ATA_MV_EDMA_BASE(ch));
	slot = (((rsp_in & ~0xffffff00) >> 3)) & 0x1f;
	rsp_out &= 0xffffff00;
	rsp_out += (slot << 3);
	bus_dmamap_sync(ch->dma.work_tag, ch->dma.work_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	response = (struct ata_marvell_response *)
		   (ch->dma.work + 1024 + (slot << 3));

	/* record status for this request */
	request->status = response->dev_status;
	request->error = 0; 

	/* ack response */
	ATA_OUTL(ctlr->r_res1, 0x02024 + ATA_MV_EDMA_BASE(ch), rsp_out);

	/* update progress */
	if (!(request->status & ATA_S_ERROR) &&
	    !(request->flags & ATA_R_TIMEOUT))
	    request->donecount = request->bytecount;

	/* unload SG list */
	ch->dma.unload(request);

	res = ATA_OP_FINISHED;
    }

    /* legacy ATA interrupt */
    else {
	res = ata_end_transaction(request);
    }

    /* ack interrupt */
    ATA_OUTL(ctlr->r_res1, offset, ~(icr & (0x0101 << (ch->unit & 3))));
    return res;
}

static void
ata_marvell_edma_reset(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    /* disable the EDMA machinery */
    ATA_OUTL(ctlr->r_res1, 0x02028 + ATA_MV_EDMA_BASE(ch), 0x00000002);
    while ((ATA_INL(ctlr->r_res1, 0x02028 + ATA_MV_EDMA_BASE(ch)) & 0x00000001))
	DELAY(10);

    /* clear SATA error register */
    ATA_IDX_OUTL(ch, ATA_SERROR, ATA_IDX_INL(ch, ATA_SERROR));

    /* clear any outstanding error interrupts */
    ATA_OUTL(ctlr->r_res1, 0x02008 + ATA_MV_EDMA_BASE(ch), 0x0);

    /* unmask all error interrupts */
    ATA_OUTL(ctlr->r_res1, 0x0200c + ATA_MV_EDMA_BASE(ch), ~0x0);

    /* enable channel and test for devices */
    if (ata_sata_phy_reset(dev, -1, 1))
	ata_generic_reset(dev);

    /* enable EDMA machinery */
    ATA_OUTL(ctlr->r_res1, 0x02028 + ATA_MV_EDMA_BASE(ch), 0x00000001);
}

static void
ata_marvell_edma_dmasetprd(void *xsc, bus_dma_segment_t *segs, int nsegs,
			   int error)
{
    struct ata_dmasetprd_args *args = xsc;
    struct ata_marvell_dma_prdentry *prd = args->dmatab;
    int i;

    if ((args->error = error))
	return;

    for (i = 0; i < nsegs; i++) {
	prd[i].addrlo = htole32(segs[i].ds_addr);
	prd[i].count = htole32(segs[i].ds_len);
	prd[i].addrhi = htole32((u_int64_t)segs[i].ds_addr >> 32);
	prd[i].reserved = 0;
    }
    prd[i - 1].count |= htole32(ATA_DMA_EOT);
    KASSERT(nsegs <= ATA_DMA_ENTRIES, ("too many DMA segment entries\n"));
    args->nsegs = nsegs;
}

static void
ata_marvell_edma_dmainit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    ata_dmainit(dev);
    /* note start and stop are not used here */
    ch->dma.setprd = ata_marvell_edma_dmasetprd;
	
    /* if 64bit support present adjust max address used */
    if (ATA_INL(ctlr->r_res1, 0x00d00) & 0x00000004)
	ch->dma.max_address = BUS_SPACE_MAXADDR;

    /* chip does not reliably do 64K DMA transfers */
    if (ctlr->chip->cfg2 == MV_50XX || ctlr->chip->cfg2 == MV_60XX)
	ch->dma.max_iosize = 64 * DEV_BSIZE;
}

ATA_DECLARE_DRIVER(ata_marvell);
