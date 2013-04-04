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
static int ata_cmd_ch_attach(device_t dev);
static int ata_cmd_status(device_t dev);
static int ata_cmd_setmode(device_t dev, int target, int mode);
static int ata_sii_ch_attach(device_t dev);
static int ata_sii_ch_detach(device_t dev);
static int ata_sii_status(device_t dev);
static void ata_sii_reset(device_t dev);
static int ata_sii_setmode(device_t dev, int target, int mode);
static int ata_siiprb_ch_attach(device_t dev);
static int ata_siiprb_ch_detach(device_t dev);
static int ata_siiprb_status(device_t dev);
static int ata_siiprb_begin_transaction(struct ata_request *request);
static int ata_siiprb_end_transaction(struct ata_request *request);
static int ata_siiprb_pm_read(device_t dev, int port, int reg, u_int32_t *result);
static int ata_siiprb_pm_write(device_t dev, int port, int reg, u_int32_t result);
static u_int32_t ata_siiprb_softreset(device_t dev, int port);
static void ata_siiprb_reset(device_t dev);
static void ata_siiprb_dmasetprd(void *xsc, bus_dma_segment_t *segs, int nsegs, int error);
static void ata_siiprb_dmainit(device_t dev);

/* misc defines */
#define SII_MEMIO	1
#define SII_PRBIO	2
#define SII_INTR	0x01
#define SII_SETCLK	0x02
#define SII_BUG		0x04
#define SII_4CH		0x08

/*
 * Silicon Image Inc. (SiI) (former CMD) chipset support functions
 */
static int
ata_sii_probe(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    static const struct ata_chip_id ids[] =
    {{ ATA_SII3114,   0x00, SII_MEMIO, SII_4CH,    ATA_SA150, "3114" },
     { ATA_SII3512,   0x02, SII_MEMIO, 0,          ATA_SA150, "3512" },
     { ATA_SII3112,   0x02, SII_MEMIO, 0,          ATA_SA150, "3112" },
     { ATA_SII3112_1, 0x02, SII_MEMIO, 0,          ATA_SA150, "3112" },
     { ATA_SII3512,   0x00, SII_MEMIO, SII_BUG,    ATA_SA150, "3512" },
     { ATA_SII3112,   0x00, SII_MEMIO, SII_BUG,    ATA_SA150, "3112" },
     { ATA_SII3112_1, 0x00, SII_MEMIO, SII_BUG,    ATA_SA150, "3112" },
     { ATA_SII3124,   0x00, SII_PRBIO, SII_4CH,    ATA_SA300, "3124" },
     { ATA_SII3132,   0x00, SII_PRBIO, 0,          ATA_SA300, "3132" },
     { ATA_SII3132_1, 0x00, SII_PRBIO, 0,          ATA_SA300, "3132" },
     { ATA_SII3132_2, 0x00, SII_PRBIO, 0,          ATA_SA300, "3132" },
     { ATA_SII0680,   0x00, SII_MEMIO, SII_SETCLK, ATA_UDMA6, "680" },
     { ATA_CMD649,    0x00, 0,         SII_INTR,   ATA_UDMA5, "(CMD) 649" },
     { ATA_CMD648,    0x00, 0,         SII_INTR,   ATA_UDMA4, "(CMD) 648" },
     { ATA_CMD646,    0x07, 0,         0,          ATA_UDMA2, "(CMD) 646U2" },
     { ATA_CMD646,    0x00, 0,         0,          ATA_WDMA2, "(CMD) 646" },
     { 0, 0, 0, 0, 0, 0}};

    if (pci_get_vendor(dev) != ATA_SILICON_IMAGE_ID)
	return ENXIO;

    if (!(ctlr->chip = ata_match_chip(dev, ids)))
	return ENXIO;

    ata_set_desc(dev);
    ctlr->chipinit = ata_sii_chipinit;
    return (BUS_PROBE_DEFAULT);
}

int
ata_sii_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev, ata_generic_intr))
	return ENXIO;

    switch (ctlr->chip->cfg1) {
    case SII_PRBIO:
	ctlr->r_type1 = SYS_RES_MEMORY;
	ctlr->r_rid1 = PCIR_BAR(0);
	if (!(ctlr->r_res1 = bus_alloc_resource_any(dev, ctlr->r_type1,
						    &ctlr->r_rid1, RF_ACTIVE)))
	    return ENXIO;

	ctlr->r_rid2 = PCIR_BAR(2);
	ctlr->r_type2 = SYS_RES_MEMORY;
	if (!(ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						    &ctlr->r_rid2, RF_ACTIVE))){
	    bus_release_resource(dev, ctlr->r_type1, ctlr->r_rid1,ctlr->r_res1);
	    return ENXIO;
	}
#ifdef __sparc64__
	if (!bus_space_map(rman_get_bustag(ctlr->r_res2),
	    rman_get_bushandle(ctlr->r_res2), rman_get_size(ctlr->r_res2),
	    BUS_SPACE_MAP_LINEAR, NULL)) {
	    	bus_release_resource(dev, ctlr->r_type1, ctlr->r_rid1,
		    ctlr->r_res1);
		bus_release_resource(dev, ctlr->r_type2, ctlr->r_rid2,
		    ctlr->r_res2);
		return (ENXIO);
	}
#endif
	ctlr->ch_attach = ata_siiprb_ch_attach;
	ctlr->ch_detach = ata_siiprb_ch_detach;
	ctlr->reset = ata_siiprb_reset;
	ctlr->setmode = ata_sata_setmode;
	ctlr->getrev = ata_sata_getrev;
	ctlr->channels = (ctlr->chip->cfg2 == SII_4CH) ? 4 : 2;

	/* reset controller */
	ATA_OUTL(ctlr->r_res1, 0x0040, 0x80000000);
	DELAY(10000);
	ATA_OUTL(ctlr->r_res1, 0x0040, 0x0000000f);
	break;

    case SII_MEMIO:
	ctlr->r_type2 = SYS_RES_MEMORY;
	ctlr->r_rid2 = PCIR_BAR(5);
	if (!(ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						    &ctlr->r_rid2, RF_ACTIVE))){
	    if (ctlr->chip->chipid != ATA_SII0680 ||
			    (pci_read_config(dev, 0x8a, 1) & 1))
		return ENXIO;
	}

	if (ctlr->chip->cfg2 & SII_SETCLK) {
	    if ((pci_read_config(dev, 0x8a, 1) & 0x30) != 0x10)
		pci_write_config(dev, 0x8a,
				 (pci_read_config(dev, 0x8a, 1) & 0xcf)|0x10,1);
	    if ((pci_read_config(dev, 0x8a, 1) & 0x30) != 0x10)
		device_printf(dev, "%s could not set ATA133 clock\n",
			      ctlr->chip->text);
	}

	/* if we have 4 channels enable the second set */
	if (ctlr->chip->cfg2 & SII_4CH) {
	    ATA_OUTL(ctlr->r_res2, 0x0200, 0x00000002);
	    ctlr->channels = 4;
	}

	/* dont block interrupts from any channel */
	pci_write_config(dev, 0x48,
			 (pci_read_config(dev, 0x48, 4) & ~0x03c00000), 4);

	/* enable PCI interrupt as BIOS might not */
	pci_write_config(dev, 0x8a, (pci_read_config(dev, 0x8a, 1) & 0x3f), 1);

	if (ctlr->r_res2) {
	    ctlr->ch_attach = ata_sii_ch_attach;
	    ctlr->ch_detach = ata_sii_ch_detach;
	}

	if (ctlr->chip->max_dma >= ATA_SA150) {
	    ctlr->reset = ata_sii_reset;
	    ctlr->setmode = ata_sata_setmode;
	    ctlr->getrev = ata_sata_getrev;
	}
	else
	    ctlr->setmode = ata_sii_setmode;
	break;
    
    default:
	if ((pci_read_config(dev, 0x51, 1) & 0x08) != 0x08) {
	    device_printf(dev, "HW has secondary channel disabled\n");
	    ctlr->channels = 1;
	}    

	/* enable interrupt as BIOS might not */
	pci_write_config(dev, 0x71, 0x01, 1);

	ctlr->ch_attach = ata_cmd_ch_attach;
	ctlr->ch_detach = ata_pci_ch_detach;
	ctlr->setmode = ata_cmd_setmode;
	break;
    }
    return 0;
}

static int
ata_cmd_ch_attach(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    /* setup the usual register normal pci style */
    if (ata_pci_ch_attach(dev))
	return ENXIO;

    if (ctlr->chip->cfg2 & SII_INTR)
	ch->hw.status = ata_cmd_status;

	ch->flags |= ATA_NO_ATAPI_DMA;

    return 0;
}

static int
ata_cmd_status(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    u_int8_t reg71;

    if (((reg71 = pci_read_config(device_get_parent(dev), 0x71, 1)) &
	 (ch->unit ? 0x08 : 0x04))) {
	pci_write_config(device_get_parent(dev), 0x71,
			 reg71 & ~(ch->unit ? 0x04 : 0x08), 1);
	return ata_pci_status(dev);
    }
    return 0;
}

static int
ata_cmd_setmode(device_t dev, int target, int mode)
{
	device_t parent = device_get_parent(dev);
	struct ata_pci_controller *ctlr = device_get_softc(parent);
	struct ata_channel *ch = device_get_softc(dev);
	int devno = (ch->unit << 1) + target;
	int treg = 0x54 + ((devno < 3) ? (devno << 1) : 7);
	int ureg = ch->unit ? 0x7b : 0x73;
	int piomode;
	static const uint8_t piotimings[] =
	    { 0xa9, 0x57, 0x44, 0x32, 0x3f, 0x87, 0x32, 0x3f };
	static const uint8_t udmatimings[][2] =
	    { { 0x31,  0xc2 }, { 0x21,  0x82 }, { 0x11,  0x42 },
	      { 0x25,  0x8a }, { 0x15,  0x4a }, { 0x05,  0x0a } };

	mode = min(mode, ctlr->chip->max_dma);
	if (mode >= ATA_UDMA0) {        
		u_int8_t umode = pci_read_config(parent, ureg, 1);

	        umode &= ~(target == 0 ? 0x35 : 0xca);
		umode |= udmatimings[mode & ATA_MODE_MASK][target];
		pci_write_config(parent, ureg, umode, 1);
		piomode = ATA_PIO4;
	} else { 
		pci_write_config(parent, ureg, 
			     pci_read_config(parent, ureg, 1) &
			     ~(target == 0 ? 0x35 : 0xca), 1);
		piomode = mode;
	}
	pci_write_config(parent, treg, piotimings[ata_mode2idx(piomode)], 1);
	return (mode);
}

static int
ata_sii_ch_attach(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int unit01 = (ch->unit & 1), unit10 = (ch->unit & 2);
    int i;

    for (i = ATA_DATA; i <= ATA_COMMAND; i++) {
	ch->r_io[i].res = ctlr->r_res2;
	ch->r_io[i].offset = 0x80 + i + (unit01 << 6) + (unit10 << 8);
    }
    ch->r_io[ATA_CONTROL].res = ctlr->r_res2;
    ch->r_io[ATA_CONTROL].offset = 0x8a + (unit01 << 6) + (unit10 << 8);
    ch->r_io[ATA_IDX_ADDR].res = ctlr->r_res2;
    ata_default_registers(dev);

    ch->r_io[ATA_BMCMD_PORT].res = ctlr->r_res2;
    ch->r_io[ATA_BMCMD_PORT].offset = 0x00 + (unit01 << 3) + (unit10 << 8);
    ch->r_io[ATA_BMSTAT_PORT].res = ctlr->r_res2;
    ch->r_io[ATA_BMSTAT_PORT].offset = 0x02 + (unit01 << 3) + (unit10 << 8);
    ch->r_io[ATA_BMDTP_PORT].res = ctlr->r_res2;
    ch->r_io[ATA_BMDTP_PORT].offset = 0x04 + (unit01 << 3) + (unit10 << 8);

    if (ctlr->chip->max_dma >= ATA_SA150) {
	ch->r_io[ATA_SSTATUS].res = ctlr->r_res2;
	ch->r_io[ATA_SSTATUS].offset = 0x104 + (unit01 << 7) + (unit10 << 8);
	ch->r_io[ATA_SERROR].res = ctlr->r_res2;
	ch->r_io[ATA_SERROR].offset = 0x108 + (unit01 << 7) + (unit10 << 8);
	ch->r_io[ATA_SCONTROL].res = ctlr->r_res2;
	ch->r_io[ATA_SCONTROL].offset = 0x100 + (unit01 << 7) + (unit10 << 8);
	ch->flags |= ATA_NO_SLAVE;
	ch->flags |= ATA_SATA;
	ch->flags |= ATA_KNOWN_PRESENCE;

	/* enable PHY state change interrupt */
	ATA_OUTL(ctlr->r_res2, 0x148 + (unit01 << 7) + (unit10 << 8),(1 << 16));
    }

    if (ctlr->chip->cfg2 & SII_BUG) {
	/* work around errata in early chips */
	ch->dma.boundary = 8192;
	ch->dma.segsize = 15 * DEV_BSIZE;
    }

    ata_pci_hw(dev);
    ch->hw.status = ata_sii_status;
    if (ctlr->chip->cfg2 & SII_SETCLK)
	ch->flags |= ATA_CHECKS_CABLE;

    ata_pci_dmainit(dev);

    return 0;
}

static int
ata_sii_ch_detach(device_t dev)
{

    ata_pci_dmafini(dev);
    return (0);
}

static int
ata_sii_status(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int offset0 = ((ch->unit & 1) << 3) + ((ch->unit & 2) << 8);
    int offset1 = ((ch->unit & 1) << 6) + ((ch->unit & 2) << 8);

    /* do we have any PHY events ? */
    if (ctlr->chip->max_dma >= ATA_SA150 &&
	(ATA_INL(ctlr->r_res2, 0x10 + offset0) & 0x00000010))
	ata_sata_phy_check_events(dev, -1);

    if (ATA_INL(ctlr->r_res2, 0xa0 + offset1) & 0x00000800)
	return ata_pci_status(dev);
    else
	return 0;
}

static void
ata_sii_reset(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int offset = ((ch->unit & 1) << 7) + ((ch->unit & 2) << 8);
    uint32_t val;

    /* Apply R_ERR on DMA activate FIS errata workaround. */
    val = ATA_INL(ctlr->r_res2, 0x14c + offset);
    if ((val & 0x3) == 0x1)
	ATA_OUTL(ctlr->r_res2, 0x14c + offset, val & ~0x3);

    if (ata_sata_phy_reset(dev, -1, 1))
	ata_generic_reset(dev);
    else
	ch->devices = 0;
}

static int
ata_sii_setmode(device_t dev, int target, int mode)
{
	device_t parent = device_get_parent(dev);
	struct ata_pci_controller *ctlr = device_get_softc(parent);
	struct ata_channel *ch = device_get_softc(dev);
	int rego = (ch->unit << 4) + (target << 1);
	int mreg = ch->unit ? 0x84 : 0x80;
	int mask = 0x03 << (target << 2);
	int mval = pci_read_config(parent, mreg, 1) & ~mask;
	int piomode;
	u_int8_t preg = 0xa4 + rego;
	u_int8_t dreg = 0xa8 + rego;
	u_int8_t ureg = 0xac + rego;
	static const uint16_t piotimings[] =
	    { 0x328a, 0x2283, 0x1104, 0x10c3, 0x10c1 };
	static const uint16_t dmatimings[] = { 0x2208, 0x10c2, 0x10c1 };
	static const uint8_t udmatimings[] =
	    { 0xf, 0xb, 0x7, 0x5, 0x3, 0x2, 0x1 };

	mode = min(mode, ctlr->chip->max_dma);

	if (ctlr->chip->cfg2 & SII_SETCLK) {
	    if (ata_dma_check_80pin && mode > ATA_UDMA2 &&
		(pci_read_config(parent, 0x79, 1) &
				 (ch->unit ? 0x02 : 0x01))) {
		ata_print_cable(dev, "controller");
		mode = ATA_UDMA2;
	    }
	}
	if (mode >= ATA_UDMA0) {
		pci_write_config(parent, mreg,
			 mval | (0x03 << (target << 2)), 1);
		pci_write_config(parent, ureg, 
			 (pci_read_config(parent, ureg, 1) & ~0x3f) |
			 udmatimings[mode & ATA_MODE_MASK], 1);
		piomode = ATA_PIO4;
	} else if (mode >= ATA_WDMA0) {
		pci_write_config(parent, mreg,
			 mval | (0x02 << (target << 2)), 1);
		pci_write_config(parent, dreg, dmatimings[mode & ATA_MODE_MASK], 2);
		piomode = (mode == ATA_WDMA0) ? ATA_PIO0 :
		    (mode == ATA_WDMA1) ? ATA_PIO3 : ATA_PIO4;
	} else {
		pci_write_config(parent, mreg,
			 mval | (0x01 << (target << 2)), 1);
		piomode = mode;
	}
	pci_write_config(parent, preg, piotimings[ata_mode2idx(piomode)], 2);
	return (mode);
}

struct ata_siiprb_dma_prdentry {
    u_int64_t addr;
    u_int32_t count;
    u_int32_t control;
} __packed;

#define ATA_SIIPRB_DMA_ENTRIES		129
struct ata_siiprb_ata_command {
    struct ata_siiprb_dma_prdentry prd[ATA_SIIPRB_DMA_ENTRIES];
} __packed;

struct ata_siiprb_atapi_command {
    u_int8_t ccb[16];
    struct ata_siiprb_dma_prdentry prd[ATA_SIIPRB_DMA_ENTRIES];
} __packed;

struct ata_siiprb_command {
    u_int16_t control;
    u_int16_t protocol_override;
    u_int32_t transfer_count;
    u_int8_t fis[24];
    union {
	struct ata_siiprb_ata_command ata;
	struct ata_siiprb_atapi_command atapi;
    } u;
} __packed;

static int
ata_siiprb_ch_attach(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int offset = ch->unit * 0x2000;

    ata_siiprb_dmainit(dev);

    /* set the SATA resources */
    ch->r_io[ATA_SSTATUS].res = ctlr->r_res2;
    ch->r_io[ATA_SSTATUS].offset = 0x1f04 + offset;
    ch->r_io[ATA_SERROR].res = ctlr->r_res2;
    ch->r_io[ATA_SERROR].offset = 0x1f08 + offset;
    ch->r_io[ATA_SCONTROL].res = ctlr->r_res2;
    ch->r_io[ATA_SCONTROL].offset = 0x1f00 + offset;
    ch->r_io[ATA_SACTIVE].res = ctlr->r_res2;
    ch->r_io[ATA_SACTIVE].offset = 0x1f0c + offset;
   
    ch->hw.status = ata_siiprb_status;
    ch->hw.begin_transaction = ata_siiprb_begin_transaction;
    ch->hw.end_transaction = ata_siiprb_end_transaction;
    ch->hw.command = NULL;	/* not used here */
    ch->hw.softreset = ata_siiprb_softreset;
    ch->hw.pm_read = ata_siiprb_pm_read;
    ch->hw.pm_write = ata_siiprb_pm_write;
    ch->flags |= ATA_NO_SLAVE;
    ch->flags |= ATA_SATA;
    return 0;
}

static int
ata_siiprb_ch_detach(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    if (ch->dma.work_tag && ch->dma.work_map)
	bus_dmamap_sync(ch->dma.work_tag, ch->dma.work_map,
	    BUS_DMASYNC_POSTWRITE);
    ata_dmafini(dev);
    return 0;
}

static int
ata_siiprb_status(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    u_int32_t action = ATA_INL(ctlr->r_res1, 0x0044);
    int offset = ch->unit * 0x2000;

    if (action & (1 << ch->unit)) {
	u_int32_t istatus = ATA_INL(ctlr->r_res2, 0x1008 + offset);

	/* do we have any PHY events ? */
	ata_sata_phy_check_events(dev, -1);

	/* clear interrupt(s) */
	ATA_OUTL(ctlr->r_res2, 0x1008 + offset, istatus);

	/* do we have any device action ? */
	return (istatus & 0x00000003);
    }
    return 0;
}

static int
ata_siiprb_begin_transaction(struct ata_request *request)
{
    struct ata_pci_controller *ctlr=device_get_softc(device_get_parent(request->parent));
    struct ata_channel *ch = device_get_softc(request->parent);
    struct ata_siiprb_command *prb;
    struct ata_siiprb_dma_prdentry *prd;
    int offset = ch->unit * 0x2000;
    u_int64_t prb_bus;

    /* SOS XXX */
    if (request->u.ata.command == ATA_DEVICE_RESET) {
        request->result = 0;
        return ATA_OP_FINISHED;
    }

    /* get a piece of the workspace for this request */
    prb = (struct ata_siiprb_command *)ch->dma.work;

    /* clear the prb structure */
    bzero(prb, sizeof(struct ata_siiprb_command));

    /* setup the FIS for this request */
    if (!ata_request2fis_h2d(request, &prb->fis[0])) {
        device_printf(request->parent, "setting up SATA FIS failed\n");
        request->result = EIO;
        return ATA_OP_FINISHED;
    }

    /* setup transfer type */
    if (request->flags & ATA_R_ATAPI) {
	bcopy(request->u.atapi.ccb, prb->u.atapi.ccb, 16);
	if (request->flags & ATA_R_ATAPI16)
	    ATA_OUTL(ctlr->r_res2, 0x1000 + offset, 0x00000020);
	else
	    ATA_OUTL(ctlr->r_res2, 0x1004 + offset, 0x00000020);
	if (request->flags & ATA_R_READ)
	    prb->control = htole16(0x0010);
	if (request->flags & ATA_R_WRITE)
	    prb->control = htole16(0x0020);
	prd = &prb->u.atapi.prd[0];
    }
    else
	prd = &prb->u.ata.prd[0];

    /* if request moves data setup and load SG list */
    if (request->flags & (ATA_R_READ | ATA_R_WRITE)) {
	if (ch->dma.load(request, prd, NULL)) {
	    device_printf(request->parent, "setting up DMA failed\n");
	    request->result = EIO;
	    return ATA_OP_FINISHED;
	}
    }

    bus_dmamap_sync(ch->dma.work_tag, ch->dma.work_map, BUS_DMASYNC_PREWRITE);

    /* activate the prb */
    prb_bus = ch->dma.work_bus;
    ATA_OUTL(ctlr->r_res2, 0x1c00 + offset, prb_bus);
    ATA_OUTL(ctlr->r_res2, 0x1c04 + offset, prb_bus>>32);

    /* start the timeout */
    callout_reset(&request->callout, request->timeout * hz,
                  (timeout_t*)ata_timeout, request);
    return ATA_OP_CONTINUES;
}

static int
ata_siiprb_end_transaction(struct ata_request *request)
{
    struct ata_pci_controller *ctlr=device_get_softc(device_get_parent(request->parent));
    struct ata_channel *ch = device_get_softc(request->parent);
    struct ata_siiprb_command *prb;
    int offset = ch->unit * 0x2000;
    int error, timeout;

    /* kill the timeout */
    callout_stop(&request->callout);

    bus_dmamap_sync(ch->dma.work_tag, ch->dma.work_map, BUS_DMASYNC_POSTWRITE);

    prb = (struct ata_siiprb_command *)
	((u_int8_t *)rman_get_virtual(ctlr->r_res2) + offset);

    /* any controller errors flagged ? */
    if ((error = ATA_INL(ctlr->r_res2, 0x1024 + offset))) {
	if (bootverbose)
	    printf("ata_siiprb_end_transaction %s error=%08x\n",
		   ata_cmd2str(request), error);

	/* if device error status get details */
	if (error == 1 || error == 2) {
	    request->status = prb->fis[2];
	    if (request->status & ATA_S_ERROR)
		request->error = prb->fis[3];
	}

 	/* SOS XXX handle other controller errors here */

	/* initialize port */
	ATA_OUTL(ctlr->r_res2, 0x1000 + offset, 0x00000004);

	/* poll for port ready */
	for (timeout = 0; timeout < 1000; timeout++) {
	    DELAY(1000);
            if (ATA_INL(ctlr->r_res2, 0x1008 + offset) & 0x00040000)
        	break;
	}
	if (bootverbose) {
	    if (timeout >= 1000)
		device_printf(ch->dev, "port initialize timeout\n");
	    else
		device_printf(ch->dev, "port initialize time=%dms\n", timeout);
	}
    }

    /* Read back registers to the request struct. */
    if ((request->flags & ATA_R_ATAPI) == 0 &&
	((request->status & ATA_S_ERROR) ||
	 (request->flags & (ATA_R_CONTROL | ATA_R_NEEDRESULT)))) {
	request->u.ata.count = prb->fis[12] | ((u_int16_t)prb->fis[13] << 8);
	request->u.ata.lba = prb->fis[4] | ((u_int64_t)prb->fis[5] << 8) |
			     ((u_int64_t)prb->fis[6] << 16);
	if (request->flags & ATA_R_48BIT)
	    request->u.ata.lba |= ((u_int64_t)prb->fis[8] << 24) |
				  ((u_int64_t)prb->fis[9] << 32) |
				  ((u_int64_t)prb->fis[10] << 40);
	else
	    request->u.ata.lba |= ((u_int64_t)(prb->fis[7] & 0x0f) << 24);
    }

    /* update progress */
    if (!(request->status & ATA_S_ERROR) && !(request->flags & ATA_R_TIMEOUT)) {
	if (request->flags & ATA_R_READ)
	    request->donecount = le32toh(prb->transfer_count);
	else
	    request->donecount = request->bytecount;
    }

    /* release SG list etc */
    ch->dma.unload(request);

    return ATA_OP_FINISHED;
}

static int
ata_siiprb_issue_cmd(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    u_int64_t prb_bus = ch->dma.work_bus;
    u_int32_t status;
    int offset = ch->unit * 0x2000;
    int timeout;

    bus_dmamap_sync(ch->dma.work_tag, ch->dma.work_map, BUS_DMASYNC_PREWRITE);

    /* issue command to chip */
    ATA_OUTL(ctlr->r_res2, 0x1c00 + offset, prb_bus);
    ATA_OUTL(ctlr->r_res2, 0x1c04 + offset, prb_bus >> 32);

    /* poll for command finished */
    for (timeout = 0; timeout < 10000; timeout++) {
        DELAY(1000);
        if ((status = ATA_INL(ctlr->r_res2, 0x1008 + offset)) & 0x00010000)
            break;
    }

    bus_dmamap_sync(ch->dma.work_tag, ch->dma.work_map, BUS_DMASYNC_POSTWRITE);

    // SOS XXX ATA_OUTL(ctlr->r_res2, 0x1008 + offset, 0x00010000);
    ATA_OUTL(ctlr->r_res2, 0x1008 + offset, 0x08ff08ff);

    if (timeout >= 1000)
	return EIO;

    if (bootverbose)
	device_printf(dev, "siiprb_issue_cmd time=%dms status=%08x\n",
		      timeout, status);
    return 0;
}

static int
ata_siiprb_pm_read(device_t dev, int port, int reg, u_int32_t *result)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_siiprb_command *prb = (struct ata_siiprb_command *)ch->dma.work;
    int offset = ch->unit * 0x2000;

    if (port < 0) {
	*result = ATA_IDX_INL(ch, reg);
	return (0);
    }
    if (port < ATA_PM) {
	switch (reg) {
	case ATA_SSTATUS:
	    reg = 0;
	    break;
	case ATA_SERROR:
	    reg = 1;
	    break;
	case ATA_SCONTROL:
	    reg = 2;
	    break;
	default:
	    return (EINVAL);
	}
    }
    bzero(prb, sizeof(struct ata_siiprb_command));
    prb->fis[0] = 0x27;	/* host to device */
    prb->fis[1] = 0x8f;	/* command FIS to PM port */
    prb->fis[2] = ATA_READ_PM;
    prb->fis[3] = reg;
    prb->fis[7] = port;
    if (ata_siiprb_issue_cmd(dev)) {
	device_printf(dev, "error reading PM port\n");
	return EIO;
    }
    prb = (struct ata_siiprb_command *)
	((u_int8_t *)rman_get_virtual(ctlr->r_res2) + offset);
    *result = prb->fis[12]|(prb->fis[4]<<8)|(prb->fis[5]<<16)|(prb->fis[6]<<24);
    return 0;
}

static int
ata_siiprb_pm_write(device_t dev, int port, int reg, u_int32_t value)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_siiprb_command *prb = (struct ata_siiprb_command *)ch->dma.work;
    int offset = ch->unit * 0x2000;

    if (port < 0) {
	ATA_IDX_OUTL(ch, reg, value);
	return (0);
    }
    if (port < ATA_PM) {
	switch (reg) {
	case ATA_SSTATUS:
	    reg = 0;
	    break;
	case ATA_SERROR:
	    reg = 1;
	    break;
	case ATA_SCONTROL:
	    reg = 2;
	    break;
	default:
	    return (EINVAL);
	}
    }
    bzero(prb, sizeof(struct ata_siiprb_command));
    prb->fis[0] = 0x27;	/* host to device */
    prb->fis[1] = 0x8f;	/* command FIS to PM port */
    prb->fis[2] = ATA_WRITE_PM;
    prb->fis[3] = reg;
    prb->fis[7] = port;
    prb->fis[12] = value & 0xff;
    prb->fis[4] = (value >> 8) & 0xff;
    prb->fis[5] = (value >> 16) & 0xff;
    prb->fis[6] = (value >> 24) & 0xff;
    if (ata_siiprb_issue_cmd(dev)) {
	device_printf(dev, "error writing PM port\n");
	return ATA_E_ABORT;
    }
    prb = (struct ata_siiprb_command *)
	((u_int8_t *)rman_get_virtual(ctlr->r_res2) + offset);
    return prb->fis[3];
}

static u_int32_t
ata_siiprb_softreset(device_t dev, int port)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_siiprb_command *prb = (struct ata_siiprb_command *)ch->dma.work;
    u_int32_t signature;
    int offset = ch->unit * 0x2000;

    /* setup the workspace for a soft reset command */
    bzero(prb, sizeof(struct ata_siiprb_command));
    prb->control = htole16(0x0080);
    prb->fis[1] = port & 0x0f;

    /* issue soft reset */
    if (ata_siiprb_issue_cmd(dev))
	return -1;

    ata_udelay(150000);

    /* get possible signature */
    prb = (struct ata_siiprb_command *)
	((u_int8_t *)rman_get_virtual(ctlr->r_res2) + offset);
    signature=prb->fis[12]|(prb->fis[4]<<8)|(prb->fis[5]<<16)|(prb->fis[6]<<24);

    /* clear error bits/interrupt */
    ATA_IDX_OUTL(ch, ATA_SERROR, 0xffffffff);

    return signature;
}

static void
ata_siiprb_reset(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int offset = ch->unit * 0x2000;
    u_int32_t status, signature;
    int timeout;

    /* disable interrupts */
    ATA_OUTL(ctlr->r_res2, 0x1014 + offset, 0x000000ff);

    /* reset channel HW */
    ATA_OUTL(ctlr->r_res2, 0x1000 + offset, 0x00000001);
    DELAY(1000);
    ATA_OUTL(ctlr->r_res2, 0x1004 + offset, 0x00000001);
    DELAY(10000);

    /* poll for channel ready */
    for (timeout = 0; timeout < 1000; timeout++) {
        if ((status = ATA_INL(ctlr->r_res2, 0x1008 + offset)) & 0x00040000)
            break;
        DELAY(1000);
    }

    if (bootverbose) {
	if (timeout >= 1000)
	    device_printf(dev, "channel HW reset timeout\n");
	else
	    device_printf(dev, "channel HW reset time=%dms\n", timeout);
    }

    /* reset phy */
    if (!ata_sata_phy_reset(dev, -1, 1)) {
	if (bootverbose)
	    device_printf(dev, "phy reset found no device\n");
	ch->devices = 0;
	goto finish;
    }

    /* issue soft reset */
    signature = ata_siiprb_softreset(dev, ATA_PM);
    if (bootverbose)
	device_printf(dev, "SIGNATURE=%08x\n", signature);

    /* figure out whats there */
    switch (signature >> 16) {
    case 0x0000:
	ch->devices = ATA_ATA_MASTER;
	break;
    case 0x9669:
	ch->devices = ATA_PORTMULTIPLIER;
	ATA_OUTL(ctlr->r_res2, 0x1000 + offset, 0x2000); /* enable PM support */
	//SOS XXX need to clear all PM status and interrupts!!!!
	ata_pm_identify(dev);
	break;
    case 0xeb14:
	ch->devices = ATA_ATAPI_MASTER;
	break;
    default:
	ch->devices = 0;
    }
    if (bootverbose)
        device_printf(dev, "siiprb_reset devices=%08x\n", ch->devices);

finish:
    /* clear interrupt(s) */
    ATA_OUTL(ctlr->r_res2, 0x1008 + offset, 0x000008ff);

    /* require explicit interrupt ack */
    ATA_OUTL(ctlr->r_res2, 0x1000 + offset, 0x00000008);

    /* 64bit mode */
    ATA_OUTL(ctlr->r_res2, 0x1004 + offset, 0x00000400);

    /* enable interrupts wanted */
    ATA_OUTL(ctlr->r_res2, 0x1010 + offset, 0x000000ff);
}

static void
ata_siiprb_dmasetprd(void *xsc, bus_dma_segment_t *segs, int nsegs, int error)
{
    struct ata_dmasetprd_args *args = xsc;
    struct ata_siiprb_dma_prdentry *prd = args->dmatab;
    int i;

    if ((args->error = error))
	return;

    for (i = 0; i < nsegs; i++) {
	prd[i].addr = htole64(segs[i].ds_addr);
	prd[i].count = htole32(segs[i].ds_len);
    }
    prd[i - 1].control = htole32(ATA_DMA_EOT);
    KASSERT(nsegs <= ATA_SIIPRB_DMA_ENTRIES,("too many DMA segment entries\n"));
    args->nsegs = nsegs;
}

static void
ata_siiprb_dmainit(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    /* note start and stop are not used here */
    ch->dma.setprd = ata_siiprb_dmasetprd;
    ch->dma.max_address = BUS_SPACE_MAXADDR;
    ch->dma.max_iosize = (ATA_SIIPRB_DMA_ENTRIES - 1) * PAGE_SIZE;
    ata_dmainit(dev);
}

ATA_DECLARE_DRIVER(ata_sii);
