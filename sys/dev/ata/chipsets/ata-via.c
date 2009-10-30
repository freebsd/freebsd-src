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
static int ata_via_chipinit(device_t dev);
static int ata_via_ch_attach(device_t dev);
static int ata_via_ch_detach(device_t dev);
static void ata_via_reset(device_t dev);
static void ata_via_old_setmode(device_t dev, int mode);
static void ata_via_southbridge_fixup(device_t dev);
static void ata_via_new_setmode(device_t dev, int mode);

/* misc defines */
#define VIA33           0
#define VIA66           1
#define VIA100          2
#define VIA133          3

#define VIACLK          0x01
#define VIABUG          0x02
#define VIABAR          0x04
#define VIAAHCI         0x08


/*
 * VIA Technologies Inc. chipset support functions
 */
static int
ata_via_probe(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    static struct ata_chip_id ids[] =
    {{ ATA_VIA82C586, 0x02, VIA33,  0x00,    ATA_UDMA2, "82C586B" },
     { ATA_VIA82C586, 0x00, VIA33,  0x00,    ATA_WDMA2, "82C586" },
     { ATA_VIA82C596, 0x12, VIA66,  VIACLK,  ATA_UDMA4, "82C596B" },
     { ATA_VIA82C596, 0x00, VIA33,  0x00,    ATA_UDMA2, "82C596" },
     { ATA_VIA82C686, 0x40, VIA100, VIABUG,  ATA_UDMA5, "82C686B"},
     { ATA_VIA82C686, 0x10, VIA66,  VIACLK,  ATA_UDMA4, "82C686A" },
     { ATA_VIA82C686, 0x00, VIA33,  0x00,    ATA_UDMA2, "82C686" },
     { ATA_VIA8231,   0x00, VIA100, VIABUG,  ATA_UDMA5, "8231" },
     { ATA_VIA8233,   0x00, VIA100, 0x00,    ATA_UDMA5, "8233" },
     { ATA_VIA8233C,  0x00, VIA100, 0x00,    ATA_UDMA5, "8233C" },
     { ATA_VIA8233A,  0x00, VIA133, 0x00,    ATA_UDMA6, "8233A" },
     { ATA_VIA8235,   0x00, VIA133, 0x00,    ATA_UDMA6, "8235" },
     { ATA_VIA8237,   0x00, VIA133, 0x00,    ATA_UDMA6, "8237" },
     { ATA_VIA8237A,  0x00, VIA133, 0x00,    ATA_UDMA6, "8237A" },
     { ATA_VIA8237S,  0x00, VIA133, 0x00,    ATA_UDMA6, "8237S" },
     { ATA_VIA8237_5372, 0x00, VIA133, 0x00, ATA_UDMA6, "8237" },
     { ATA_VIA8237_7372, 0x00, VIA133, 0x00, ATA_UDMA6, "8237" },
     { ATA_VIA8251,   0x00, VIA133, 0x00,    ATA_UDMA6, "8251" },
     { 0, 0, 0, 0, 0, 0 }};
    static struct ata_chip_id new_ids[] =
    {{ ATA_VIA6410,   0x00, 0,      0x00,    ATA_UDMA6, "6410" },
     { ATA_VIA6420,   0x00, 7,      0x00,    ATA_SA150, "6420" },
     { ATA_VIA6421,   0x00, 6,      VIABAR,  ATA_SA150, "6421" },
     { ATA_VIA8237A,  0x00, 7,      0x00,    ATA_SA150, "8237A" },
     { ATA_VIA8237S,  0x00, 7,      0x00,    ATA_SA150, "8237S" },
     { ATA_VIA8237_5372, 0x00, 7,   0x00,    ATA_SA300, "8237" },
     { ATA_VIA8237_7372, 0x00, 7,   0x00,    ATA_SA300, "8237" },
     { ATA_VIA8251,   0x00, 0,      VIAAHCI, ATA_SA300, "8251" },
     { 0, 0, 0, 0, 0, 0 }};

    if (pci_get_vendor(dev) != ATA_VIA_ID)
	return ENXIO;

    if (pci_get_devid(dev) == ATA_VIA82C571) {
	if (!(ctlr->chip = ata_find_chip(dev, ids, -99))) 
	    return ENXIO;
    }
    else {
	if (!(ctlr->chip = ata_match_chip(dev, new_ids))) 
	    return ENXIO;
    }

    ata_set_desc(dev);
    ctlr->chipinit = ata_via_chipinit;
    return (BUS_PROBE_DEFAULT);
}

static int
ata_via_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev, ata_generic_intr))
	return ENXIO;
    
    if (ctlr->chip->max_dma >= ATA_SA150) {
	/* do we have AHCI capability ? */
	if ((ctlr->chip->cfg2 == VIAAHCI) && ata_ahci_chipinit(dev) != ENXIO)
	    return 0;

	ctlr->r_type2 = SYS_RES_IOPORT;
	ctlr->r_rid2 = PCIR_BAR(5);
	if ((ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						   &ctlr->r_rid2, RF_ACTIVE))) {
	    ctlr->ch_attach = ata_via_ch_attach;
	    ctlr->ch_detach = ata_via_ch_detach;
	    ctlr->reset = ata_via_reset;
	}

	if (ctlr->chip->cfg2 & VIABAR) {
	    ctlr->channels = 3;
	    ctlr->setmode = ata_via_new_setmode;
	}
	else
	    ctlr->setmode = ata_sata_setmode;
	return 0;
    }

    /* prepare for ATA-66 on the 82C686a and 82C596b */
    if (ctlr->chip->cfg2 & VIACLK)
	pci_write_config(dev, 0x50, 0x030b030b, 4);       

    /* the southbridge might need the data corruption fix */
    if (ctlr->chip->cfg2 & VIABUG)
	ata_via_southbridge_fixup(dev);

    /* set fifo configuration half'n'half */
    pci_write_config(dev, 0x43, 
		     (pci_read_config(dev, 0x43, 1) & 0x90) | 0x2a, 1);

    /* set status register read retry */
    pci_write_config(dev, 0x44, pci_read_config(dev, 0x44, 1) | 0x08, 1);

    /* set DMA read & end-of-sector fifo flush */
    pci_write_config(dev, 0x46, 
		     (pci_read_config(dev, 0x46, 1) & 0x0c) | 0xf0, 1);

    /* set sector size */
    pci_write_config(dev, 0x60, DEV_BSIZE, 2);
    pci_write_config(dev, 0x68, DEV_BSIZE, 2);

    ctlr->setmode = ata_via_old_setmode;
    return 0;
}

static int
ata_via_ch_attach(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    /* newer SATA chips has resources in one BAR for each channel */
    if (ctlr->chip->cfg2 & VIABAR) {
	struct resource *r_io;
	int i, rid;
		
	ata_pci_dmainit(dev);

	rid = PCIR_BAR(ch->unit);
	if (!(r_io = bus_alloc_resource_any(device_get_parent(dev),
					    SYS_RES_IOPORT,
					    &rid, RF_ACTIVE)))
	    return ENXIO;

	for (i = ATA_DATA; i <= ATA_COMMAND; i ++) {
	    ch->r_io[i].res = r_io;
	    ch->r_io[i].offset = i;
	}
	ch->r_io[ATA_CONTROL].res = r_io;
	ch->r_io[ATA_CONTROL].offset = 2 + ATA_IOSIZE;
	ch->r_io[ATA_IDX_ADDR].res = r_io;
	ata_default_registers(dev);
	for (i = ATA_BMCMD_PORT; i <= ATA_BMDTP_PORT; i++) {
	    ch->r_io[i].res = ctlr->r_res1;
	    ch->r_io[i].offset = (i - ATA_BMCMD_PORT)+(ch->unit * ATA_BMIOSIZE);
	}
	ata_pci_hw(dev);
	if (ch->unit >= 2)
	    return 0;
    }
    else {
	/* setup the usual register normal pci style */
	if (ata_pci_ch_attach(dev))
	    return ENXIO;
    }

    ch->r_io[ATA_SSTATUS].res = ctlr->r_res2;
    ch->r_io[ATA_SSTATUS].offset = (ch->unit << ctlr->chip->cfg1);
    ch->r_io[ATA_SERROR].res = ctlr->r_res2;
    ch->r_io[ATA_SERROR].offset = 0x04 + (ch->unit << ctlr->chip->cfg1);
    ch->r_io[ATA_SCONTROL].res = ctlr->r_res2;
    ch->r_io[ATA_SCONTROL].offset = 0x08 + (ch->unit << ctlr->chip->cfg1);
    ch->flags |= ATA_NO_SLAVE;

    /* XXX SOS PHY hotplug handling missing in VIA chip ?? */
    /* XXX SOS unknown how to enable PHY state change interrupt */
    return 0;
}

static int
ata_via_ch_detach(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    /* newer SATA chips has resources in one BAR for each channel */
    if (ctlr->chip->cfg2 & VIABAR) {
	int rid;
		
	rid = PCIR_BAR(ch->unit);
	bus_release_resource(device_get_parent(dev),
	    SYS_RES_IOPORT, rid, ch->r_io[ATA_CONTROL].res);

	ata_pci_dmafini(dev);
    }
    else {
	/* setup the usual register normal pci style */
	if (ata_pci_ch_detach(dev))
	    return ENXIO;
    }

    return 0;
}

static void
ata_via_reset(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    if ((ctlr->chip->cfg2 & VIABAR) && (ch->unit > 1))
        ata_generic_reset(dev);
    else
	if (ata_sata_phy_reset(dev, -1, 1))
	    ata_generic_reset(dev);
}

static void
ata_via_new_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int error;

    if ((ctlr->chip->cfg2 & VIABAR) && (ch->unit > 1)) {
        u_int8_t pio_timings[] = { 0xa8, 0x65, 0x65, 0x32, 0x20,
				   0x65, 0x32, 0x20,
				   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20 };
        u_int8_t dma_timings[] = { 0xee, 0xe8, 0xe6, 0xe4, 0xe2, 0xe1, 0xe0 };

	mode = ata_check_80pin(dev, ata_limit_mode(dev, mode, ATA_UDMA6));
	error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);
	if (bootverbose)
	    device_printf(dev, "%ssetting %s on %s chip\n",
			  (error) ? "FAILURE " : "", ata_mode2str(mode),
			  ctlr->chip->text);
	if (!error) {
	    pci_write_config(gparent, 0xab, pio_timings[ata_mode2idx(mode)], 1);
	    if (mode >= ATA_UDMA0)
		pci_write_config(gparent, 0xb3,
				 dma_timings[mode & ATA_MODE_MASK], 1);
	    atadev->mode = mode;
	}
    }
    else
	ata_sata_setmode(dev, mode);
}

static void
ata_via_old_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    u_int8_t timings[] = { 0xa8, 0x65, 0x42, 0x22, 0x20, 0x42, 0x22, 0x20,
			   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20 };
    int modes[][7] = {
	{ 0xc2, 0xc1, 0xc0, 0x00, 0x00, 0x00, 0x00 },   /* VIA ATA33 */
	{ 0xee, 0xec, 0xea, 0xe9, 0xe8, 0x00, 0x00 },   /* VIA ATA66 */
	{ 0xf7, 0xf6, 0xf4, 0xf2, 0xf1, 0xf0, 0x00 },   /* VIA ATA100 */
	{ 0xf7, 0xf7, 0xf6, 0xf4, 0xf2, 0xf1, 0xf0 } }; /* VIA ATA133 */
    int devno = (ch->unit << 1) + atadev->unit;
    int reg = 0x53 - devno;
    int error;

    mode = ata_limit_mode(dev, mode, ctlr->chip->max_dma);
    mode = ata_check_80pin(dev, mode);

    error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);
    if (bootverbose)
	device_printf(dev, "%ssetting %s on %s chip\n",
		      (error) ? "FAILURE " : "", ata_mode2str(mode),
		      ctlr->chip->text);
    if (!error) {
	if (ctlr->chip->cfg1 != VIA133)
	    pci_write_config(gparent, reg - 0x08,timings[ata_mode2idx(mode)],1);
	if (mode >= ATA_UDMA0)
	    pci_write_config(gparent, reg,
			     modes[ctlr->chip->cfg1][mode & ATA_MODE_MASK], 1);
	else
	    pci_write_config(gparent, reg, 0x8b, 1);
	atadev->mode = mode;
    }
}

static void
ata_via_southbridge_fixup(device_t dev)
{
    device_t *children;
    int nchildren, i;

    if (device_get_children(device_get_parent(dev), &children, &nchildren))
	return;

    for (i = 0; i < nchildren; i++) {
	if (pci_get_devid(children[i]) == ATA_VIA8363 ||
	    pci_get_devid(children[i]) == ATA_VIA8371 ||
	    pci_get_devid(children[i]) == ATA_VIA8662 ||
	    pci_get_devid(children[i]) == ATA_VIA8361) {
	    u_int8_t reg76 = pci_read_config(children[i], 0x76, 1);

	    if ((reg76 & 0xf0) != 0xd0) {
		device_printf(dev,
		"Correcting VIA config for southbridge data corruption bug\n");
		pci_write_config(children[i], 0x75, 0x80, 1);
		pci_write_config(children[i], 0x76, (reg76 & 0x0f) | 0xd0, 1);
	    }
	    break;
	}
    }
    free(children, M_TEMP);
}

ATA_DECLARE_DRIVER(ata_via);
MODULE_DEPEND(ata_via, ata_ahci, 1, 1, 1);
