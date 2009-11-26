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
static int ata_intel_chipinit(device_t dev);
static int ata_intel_ch_attach(device_t dev);
static void ata_intel_reset(device_t dev);
static void ata_intel_old_setmode(device_t dev, int mode);
static void ata_intel_new_setmode(device_t dev, int mode);
static void ata_intel_sata_setmode(device_t dev, int mode);
static int ata_intel_31244_ch_attach(device_t dev);
static int ata_intel_31244_ch_detach(device_t dev);
static int ata_intel_31244_status(device_t dev);
static void ata_intel_31244_tf_write(struct ata_request *request);
static void ata_intel_31244_reset(device_t dev);

/* misc defines */
#define INTEL_AHCI	1


/*
 * Intel chipset support functions
 */
static int
ata_intel_probe(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    static struct ata_chip_id ids[] =
    {{ ATA_I82371FB,     0,          0, 2, ATA_WDMA2, "PIIX" },
     { ATA_I82371SB,     0,          0, 2, ATA_WDMA2, "PIIX3" },
     { ATA_I82371AB,     0,          0, 2, ATA_UDMA2, "PIIX4" },
     { ATA_I82443MX,     0,          0, 2, ATA_UDMA2, "PIIX4" },
     { ATA_I82451NX,     0,          0, 2, ATA_UDMA2, "PIIX4" },
     { ATA_I82801AB,     0,          0, 2, ATA_UDMA2, "ICH0" },
     { ATA_I82801AA,     0,          0, 2, ATA_UDMA4, "ICH" },
     { ATA_I82372FB,     0,          0, 2, ATA_UDMA4, "ICH" },
     { ATA_I82801BA,     0,          0, 2, ATA_UDMA5, "ICH2" },
     { ATA_I82801BA_1,   0,          0, 2, ATA_UDMA5, "ICH2" },
     { ATA_I82801CA,     0,          0, 2, ATA_UDMA5, "ICH3" },
     { ATA_I82801CA_1,   0,          0, 2, ATA_UDMA5, "ICH3" },
     { ATA_I82801DB,     0,          0, 2, ATA_UDMA5, "ICH4" },
     { ATA_I82801DB_1,   0,          0, 2, ATA_UDMA5, "ICH4" },
     { ATA_I82801EB,     0,          0, 2, ATA_UDMA5, "ICH5" },
     { ATA_I82801EB_S1,  0,          0, 2, ATA_SA150, "ICH5" },
     { ATA_I82801EB_R1,  0,          0, 2, ATA_SA150, "ICH5" },
     { ATA_I6300ESB,     0,          0, 2, ATA_UDMA5, "6300ESB" },
     { ATA_I6300ESB_S1,  0,          0, 2, ATA_SA150, "6300ESB" },
     { ATA_I6300ESB_R1,  0,          0, 2, ATA_SA150, "6300ESB" },
     { ATA_I82801FB,     0,          0, 2, ATA_UDMA5, "ICH6" },
     { ATA_I82801FB_S1,  0, INTEL_AHCI, 0, ATA_SA150, "ICH6" },
     { ATA_I82801FB_R1,  0, INTEL_AHCI, 0, ATA_SA150, "ICH6" },
     { ATA_I82801FBM,    0, INTEL_AHCI, 0, ATA_SA150, "ICH6M" },
     { ATA_I82801GB,     0,          0, 1, ATA_UDMA5, "ICH7" },
     { ATA_I82801GB_S1,  0, INTEL_AHCI, 0, ATA_SA300, "ICH7" },
     { ATA_I82801GB_R1,  0, INTEL_AHCI, 0, ATA_SA300, "ICH7" },
     { ATA_I82801GB_AH,  0, INTEL_AHCI, 0, ATA_SA300, "ICH7" },
     { ATA_I82801GBM_S1, 0, INTEL_AHCI, 0, ATA_SA150, "ICH7M" },
     { ATA_I82801GBM_R1, 0, INTEL_AHCI, 0, ATA_SA150, "ICH7M" },
     { ATA_I82801GBM_AH, 0, INTEL_AHCI, 0, ATA_SA150, "ICH7M" },
     { ATA_I63XXESB2,    0,          0, 1, ATA_UDMA5, "63XXESB2" },
     { ATA_I63XXESB2_S1, 0, INTEL_AHCI, 0, ATA_SA300, "63XXESB2" },
     { ATA_I63XXESB2_S2, 0, INTEL_AHCI, 0, ATA_SA300, "63XXESB2" },
     { ATA_I63XXESB2_R1, 0, INTEL_AHCI, 0, ATA_SA300, "63XXESB2" },
     { ATA_I63XXESB2_R2, 0, INTEL_AHCI, 0, ATA_SA300, "63XXESB2" },
     { ATA_I82801HB_S1,  0, INTEL_AHCI, 0, ATA_SA300, "ICH8" },
     { ATA_I82801HB_S2,  0, INTEL_AHCI, 0, ATA_SA300, "ICH8" },
     { ATA_I82801HB_R1,  0, INTEL_AHCI, 0, ATA_SA300, "ICH8" },
     { ATA_I82801HB_AH4, 0, INTEL_AHCI, 0, ATA_SA300, "ICH8" },
     { ATA_I82801HB_AH6, 0, INTEL_AHCI, 0, ATA_SA300, "ICH8" },
     { ATA_I82801HBM,    0,          0, 1, ATA_UDMA5, "ICH8M" },
     { ATA_I82801HBM_S1, 0, INTEL_AHCI, 0, ATA_SA300, "ICH8M" },
     { ATA_I82801HBM_S2, 0, INTEL_AHCI, 0, ATA_SA300, "ICH8M" },
     { ATA_I82801HBM_S3, 0, INTEL_AHCI, 0, ATA_SA300, "ICH8M" },
     { ATA_I82801IB_S1,  0, INTEL_AHCI, 0, ATA_SA300, "ICH9" },
     { ATA_I82801IB_S2,  0, INTEL_AHCI, 0, ATA_SA300, "ICH9" },
     { ATA_I82801IB_AH2, 0, INTEL_AHCI, 0, ATA_SA300, "ICH9" },
     { ATA_I82801IB_AH4, 0, INTEL_AHCI, 0, ATA_SA300, "ICH9" },
     { ATA_I82801IB_AH6, 0, INTEL_AHCI, 0, ATA_SA300, "ICH9" },
     { ATA_I82801IB_R1,  0, INTEL_AHCI, 0, ATA_SA300, "ICH9" },
     { ATA_I82801JIB_S1, 0, INTEL_AHCI, 0, ATA_SA300, "ICH10" },
     { ATA_I82801JIB_AH, 0, INTEL_AHCI, 0, ATA_SA300, "ICH10" },
     { ATA_I82801JIB_R1, 0, INTEL_AHCI, 0, ATA_SA300, "ICH10" },
     { ATA_I82801JIB_S2, 0, INTEL_AHCI, 0, ATA_SA300, "ICH10" },
     { ATA_I82801JD_S1,  0, INTEL_AHCI, 0, ATA_SA300, "ICH10" },
     { ATA_I82801JD_AH,  0, INTEL_AHCI, 0, ATA_SA300, "ICH10" },
     { ATA_I82801JD_R1,  0, INTEL_AHCI, 0, ATA_SA300, "ICH10" },
     { ATA_I82801JD_S2,  0, INTEL_AHCI, 0, ATA_SA300, "ICH10" },
     { ATA_I82801JI_S1,  0, INTEL_AHCI, 0, ATA_SA300, "ICH10" },
     { ATA_I82801JI_AH,  0, INTEL_AHCI, 0, ATA_SA300, "ICH10" },
     { ATA_I82801JI_R1,  0, INTEL_AHCI, 0, ATA_SA300, "ICH10" },
     { ATA_I82801JI_S2,  0, INTEL_AHCI, 0, ATA_SA300, "ICH10" },
     { ATA_I31244,       0,          0, 2, ATA_SA150, "31244" },
     { 0, 0, 0, 0, 0, 0}};

    if (pci_get_vendor(dev) != ATA_INTEL_ID)
	return ENXIO;

    if (!(ctlr->chip = ata_match_chip(dev, ids)))
	return ENXIO;

    ata_set_desc(dev);
    ctlr->chipinit = ata_intel_chipinit;
    return (BUS_PROBE_DEFAULT);
}

static int
ata_intel_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev, ata_generic_intr))
	return ENXIO;

    /* good old PIIX needs special treatment (not implemented) */
    if (ctlr->chip->chipid == ATA_I82371FB) {
	ctlr->setmode = ata_intel_old_setmode;
    }

    /* the intel 31244 needs special care if in DPA mode */
    else if (ctlr->chip->chipid == ATA_I31244) {
	if (pci_get_subclass(dev) != PCIS_STORAGE_IDE) {
	    ctlr->r_type2 = SYS_RES_MEMORY;
	    ctlr->r_rid2 = PCIR_BAR(0);
	    if (!(ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
							&ctlr->r_rid2,
							RF_ACTIVE)))
		return ENXIO;
	    ctlr->channels = 4;
	    ctlr->ch_attach = ata_intel_31244_ch_attach;
	    ctlr->ch_detach = ata_intel_31244_ch_detach;
	    ctlr->reset = ata_intel_31244_reset;
	}
	ctlr->setmode = ata_sata_setmode;
    }

    /* non SATA intel chips goes here */
    else if (ctlr->chip->max_dma < ATA_SA150) {
	ctlr->channels = ctlr->chip->cfg2;
	ctlr->ch_attach = ata_intel_ch_attach;
	ctlr->ch_detach = ata_pci_ch_detach;
	ctlr->setmode = ata_intel_new_setmode;
    }

    /* SATA parts can be either compat or AHCI */
    else {
	/* force all ports active "the legacy way" */
	pci_write_config(dev, 0x92, pci_read_config(dev, 0x92, 2) | 0x0f, 2);

	ctlr->ch_attach = ata_intel_ch_attach;
	ctlr->ch_detach = ata_pci_ch_detach;
	ctlr->reset = ata_intel_reset;

	/* 
	 * if we have AHCI capability and AHCI or RAID mode enabled
	 * in BIOS we try for AHCI mode
	 */ 
	if ((ctlr->chip->cfg1 == INTEL_AHCI) &&
	    (pci_read_config(dev, 0x90, 1) & 0xc0) &&
	    (ata_ahci_chipinit(dev) != ENXIO))
	    return 0;
	
	/* if BAR(5) is IO it should point to SATA interface registers */
	ctlr->r_type2 = SYS_RES_IOPORT;
	ctlr->r_rid2 = PCIR_BAR(5);
	if ((ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						   &ctlr->r_rid2, RF_ACTIVE)))
	    ctlr->setmode = ata_intel_sata_setmode;
	else
	    ctlr->setmode = ata_sata_setmode;
    }
    return 0;
}

static int
ata_intel_ch_attach(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    /* setup the usual register normal pci style */
    if (ata_pci_ch_attach(dev))
	return ENXIO;

    /* if r_res2 is valid it points to SATA interface registers */
    if (ctlr->r_res2) {
	ch->r_io[ATA_IDX_ADDR].res = ctlr->r_res2;
	ch->r_io[ATA_IDX_ADDR].offset = 0x00;
	ch->r_io[ATA_IDX_DATA].res = ctlr->r_res2;
	ch->r_io[ATA_IDX_DATA].offset = 0x04;
    }

    ch->flags |= ATA_ALWAYS_DMASTAT;
    return 0;
}

static void
ata_intel_reset(device_t dev)
{
    device_t parent = device_get_parent(dev);
    struct ata_pci_controller *ctlr = device_get_softc(parent);
    struct ata_channel *ch = device_get_softc(dev);
    int mask, timeout;

    /* ICH6 & ICH7 in compat mode has 4 SATA ports as master/slave on 2 ch's */
    if (ctlr->chip->cfg1) {
	mask = (0x0005 << ch->unit);
    }
    else {
	/* ICH5 in compat mode has SATA ports as master/slave on 1 channel */
	if (pci_read_config(parent, 0x90, 1) & 0x04)
	    mask = 0x0003;
	else {
	    mask = (0x0001 << ch->unit);
	    /* XXX SOS should be in intel_ch_attach if we grow it */
	    ch->flags |= ATA_NO_SLAVE;
	}
    }
    pci_write_config(parent, 0x92, pci_read_config(parent, 0x92, 2) & ~mask, 2);
    DELAY(10);
    pci_write_config(parent, 0x92, pci_read_config(parent, 0x92, 2) | mask, 2);

    /* wait up to 1 sec for "connect well" */
    for (timeout = 0; timeout < 100 ; timeout++) {
	if (((pci_read_config(parent, 0x92, 2) & (mask << 4)) == (mask << 4)) &&
	    (ATA_IDX_INB(ch, ATA_STATUS) != 0xff))
	    break;
	ata_udelay(10000);
    }
    ata_generic_reset(dev);
}

static void
ata_intel_old_setmode(device_t dev, int mode)
{
    /* NOT YET */
}

static void
ata_intel_new_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int devno = (ch->unit << 1) + atadev->unit;
    u_int32_t reg40 = pci_read_config(gparent, 0x40, 4);
    u_int8_t reg44 = pci_read_config(gparent, 0x44, 1);
    u_int8_t reg48 = pci_read_config(gparent, 0x48, 1);
    u_int16_t reg4a = pci_read_config(gparent, 0x4a, 2);
    u_int16_t reg54 = pci_read_config(gparent, 0x54, 2);
    u_int32_t mask40 = 0, new40 = 0;
    u_int8_t mask44 = 0, new44 = 0;
    int error;
    u_int8_t timings[] = { 0x00, 0x00, 0x10, 0x21, 0x23, 0x00, 0x21, 0x23,
			   0x23, 0x23, 0x23, 0x23, 0x23, 0x23 };

    mode = ata_limit_mode(dev, mode, ctlr->chip->max_dma);

    if ( mode > ATA_UDMA2 && !(reg54 & (0x10 << devno))) {
	ata_print_cable(dev, "controller");
	mode = ATA_UDMA2;
    }

    error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);

    if (bootverbose)
	device_printf(dev, "%ssetting %s on %s chip\n",
		      (error) ? "FAILURE " : "",
		      ata_mode2str(mode), ctlr->chip->text);
    if (!error) {
	if (mode >= ATA_UDMA0) {
	    u_int8_t utimings[] = { 0x00, 0x01, 0x02, 0x01, 0x02, 0x01, 0x02 };

	    pci_write_config(gparent, 0x48, reg48 | (0x0001 << devno), 2);
	    pci_write_config(gparent, 0x4a,
			     (reg4a & ~(0x3 << (devno << 2))) |
			     (utimings[mode & ATA_MODE_MASK] << (devno<<2)), 2);
	}
	else {
	    pci_write_config(gparent, 0x48, reg48 & ~(0x0001 << devno), 2);
	    pci_write_config(gparent, 0x4a, (reg4a & ~(0x3 << (devno << 2))),2);
	}
	reg54 |= 0x0400;
	if (mode >= ATA_UDMA3)
	    reg54 |= (0x1 << devno);
	else
	    reg54 &= ~(0x1 << devno);
	if (mode >= ATA_UDMA5)
	    reg54 |= (0x1000 << devno);
	else 
	    reg54 &= ~(0x1000 << devno);

	pci_write_config(gparent, 0x54, reg54, 2);

	reg40 &= ~0x00ff00ff;
	reg40 |= 0x40774077;

	if (atadev->unit == ATA_MASTER) {
	    mask40 = 0x3300;
	    new40 = timings[ata_mode2idx(mode)] << 8;
	}
	else {
	    mask44 = 0x0f;
	    new44 = ((timings[ata_mode2idx(mode)] & 0x30) >> 2) |
		    (timings[ata_mode2idx(mode)] & 0x03);
	}
	if (ch->unit) {
	    mask40 <<= 16;
	    new40 <<= 16;
	    mask44 <<= 4;
	    new44 <<= 4;
	}
	pci_write_config(gparent, 0x40, (reg40 & ~mask40) | new40, 4);
	pci_write_config(gparent, 0x44, (reg44 & ~mask44) | new44, 1);

	atadev->mode = mode;
    }
}

static void
ata_intel_sata_setmode(device_t dev, int mode)
{
    struct ata_device *atadev = device_get_softc(dev);

    if (atadev->param.satacapabilities != 0x0000 &&
	atadev->param.satacapabilities != 0xffff) {

	struct ata_channel *ch = device_get_softc(device_get_parent(dev));
	int devno = (ch->unit << 1) + atadev->unit;

	/* on some drives we need to set the transfer mode */
	ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0,
		       ata_limit_mode(dev, mode, ATA_UDMA6));

	/* set ATA_SSTATUS register offset */
	ATA_IDX_OUTL(ch, ATA_IDX_ADDR, devno * 0x100);

	/* query SATA STATUS for the speed */
	if ((ATA_IDX_INL(ch, ATA_IDX_DATA) & ATA_SS_CONWELL_MASK) ==
	    ATA_SS_CONWELL_GEN2)
	    atadev->mode = ATA_SA300;
	else
	    atadev->mode = ATA_SA150;
    }
    else {
	mode = ata_limit_mode(dev, mode, ATA_UDMA5);
	if (!ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode))
	    atadev->mode = mode;
    }
}

static int
ata_intel_31244_ch_attach(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int i;
    int ch_offset;

    ata_pci_dmainit(dev);

    ch_offset = 0x200 + ch->unit * 0x200;

    for (i = ATA_DATA; i < ATA_MAX_RES; i++)
	ch->r_io[i].res = ctlr->r_res2;

    /* setup ATA registers */
    ch->r_io[ATA_DATA].offset = ch_offset + 0x00;
    ch->r_io[ATA_FEATURE].offset = ch_offset + 0x06;
    ch->r_io[ATA_COUNT].offset = ch_offset + 0x08;
    ch->r_io[ATA_SECTOR].offset = ch_offset + 0x0c;
    ch->r_io[ATA_CYL_LSB].offset = ch_offset + 0x10;
    ch->r_io[ATA_CYL_MSB].offset = ch_offset + 0x14;
    ch->r_io[ATA_DRIVE].offset = ch_offset + 0x18;
    ch->r_io[ATA_COMMAND].offset = ch_offset + 0x1d;
    ch->r_io[ATA_ERROR].offset = ch_offset + 0x04;
    ch->r_io[ATA_STATUS].offset = ch_offset + 0x1c;
    ch->r_io[ATA_ALTSTAT].offset = ch_offset + 0x28;
    ch->r_io[ATA_CONTROL].offset = ch_offset + 0x29;

    /* setup DMA registers */
    ch->r_io[ATA_SSTATUS].offset = ch_offset + 0x100;
    ch->r_io[ATA_SERROR].offset = ch_offset + 0x104;
    ch->r_io[ATA_SCONTROL].offset = ch_offset + 0x108;

    /* setup SATA registers */
    ch->r_io[ATA_BMCMD_PORT].offset = ch_offset + 0x70;
    ch->r_io[ATA_BMSTAT_PORT].offset = ch_offset + 0x72;
    ch->r_io[ATA_BMDTP_PORT].offset = ch_offset + 0x74;

    ch->flags |= ATA_NO_SLAVE;
    ata_pci_hw(dev);
    ch->hw.status = ata_intel_31244_status;
    ch->hw.tf_write = ata_intel_31244_tf_write;

    /* enable PHY state change interrupt */
    ATA_OUTL(ctlr->r_res2, 0x4,
	     ATA_INL(ctlr->r_res2, 0x04) | (0x01 << (ch->unit << 3)));
    return 0;
}

static int
ata_intel_31244_ch_detach(device_t dev)
{

    ata_pci_dmafini(dev);
    return (0);
}

static int
ata_intel_31244_status(device_t dev)
{
    /* do we have any PHY events ? */
    ata_sata_phy_check_events(dev);

    /* any drive action to take care of ? */
    return ata_pci_status(dev);
}

static void
ata_intel_31244_tf_write(struct ata_request *request)
{
    struct ata_channel *ch = device_get_softc(request->parent);
    struct ata_device *atadev = device_get_softc(request->dev);

    if (request->flags & ATA_R_48BIT) {
	ATA_IDX_OUTW(ch, ATA_FEATURE, request->u.ata.feature);
	ATA_IDX_OUTW(ch, ATA_COUNT, request->u.ata.count);
	ATA_IDX_OUTW(ch, ATA_SECTOR, ((request->u.ata.lba >> 16) & 0xff00) |
				      (request->u.ata.lba & 0x00ff));
	ATA_IDX_OUTW(ch, ATA_CYL_LSB, ((request->u.ata.lba >> 24) & 0xff00) |
				       ((request->u.ata.lba >> 8) & 0x00ff));
	ATA_IDX_OUTW(ch, ATA_CYL_MSB, ((request->u.ata.lba >> 32) & 0xff00) | 
				       ((request->u.ata.lba >> 16) & 0x00ff));
	ATA_IDX_OUTW(ch, ATA_DRIVE, ATA_D_LBA | ATA_DEV(request->unit));
    }
    else {
	ATA_IDX_OUTB(ch, ATA_FEATURE, request->u.ata.feature);
	ATA_IDX_OUTB(ch, ATA_COUNT, request->u.ata.count);
	if (atadev->flags & ATA_D_USE_CHS) {
	    int heads, sectors;
    
	    if (atadev->param.atavalid & ATA_FLAG_54_58) {
		heads = atadev->param.current_heads;
		sectors = atadev->param.current_sectors;
	    }
	    else {
		heads = atadev->param.heads;
		sectors = atadev->param.sectors;
	    }
	    ATA_IDX_OUTB(ch, ATA_SECTOR, (request->u.ata.lba % sectors)+1);
	    ATA_IDX_OUTB(ch, ATA_CYL_LSB,
			 (request->u.ata.lba / (sectors * heads)));
	    ATA_IDX_OUTB(ch, ATA_CYL_MSB,
			 (request->u.ata.lba / (sectors * heads)) >> 8);
	    ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | ATA_DEV(request->unit) | 
			 (((request->u.ata.lba% (sectors * heads)) /
			   sectors) & 0xf));
	}
	else {
	    ATA_IDX_OUTB(ch, ATA_SECTOR, request->u.ata.lba);
	    ATA_IDX_OUTB(ch, ATA_CYL_LSB, request->u.ata.lba >> 8);
	    ATA_IDX_OUTB(ch, ATA_CYL_MSB, request->u.ata.lba >> 16);
	    ATA_IDX_OUTB(ch, ATA_DRIVE,
			 ATA_D_IBM | ATA_D_LBA | ATA_DEV(request->unit) |
			 ((request->u.ata.lba >> 24) & 0x0f));
	}
    }
}

static void
ata_intel_31244_reset(device_t dev)
{
    if (ata_sata_phy_reset(dev, -1, 1))
	ata_generic_reset(dev);
}

ATA_DECLARE_DRIVER(ata_intel);
MODULE_DEPEND(ata_intel, ata_ahci, 1, 1, 1);
