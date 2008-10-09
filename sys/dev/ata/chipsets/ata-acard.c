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
static int ata_acard_chipinit(device_t dev);
static int ata_acard_allocate(device_t dev);
static int ata_acard_status(device_t dev);
static void ata_acard_850_setmode(device_t dev, int mode);
static void ata_acard_86X_setmode(device_t dev, int mode);
static int ata_serialize(device_t dev, int flags);

/* misc defines */
#define ATP_OLD		1


/*
 * Acard chipset support functions
 */
static int
ata_acard_probe(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    static struct ata_chip_id ids[] =
    {{ ATA_ATP850R, 0, ATP_OLD, 0x00, ATA_UDMA2, "ATP850" },
     { ATA_ATP860A, 0, 0,       0x00, ATA_UDMA4, "ATP860A" },
     { ATA_ATP860R, 0, 0,       0x00, ATA_UDMA4, "ATP860R" },
     { ATA_ATP865A, 0, 0,       0x00, ATA_UDMA6, "ATP865A" },
     { ATA_ATP865R, 0, 0,       0x00, ATA_UDMA6, "ATP865R" },
     { 0, 0, 0, 0, 0, 0}};

    if (pci_get_vendor(dev) != ATA_ACARD_ID)
	return ENXIO;

    if (!(ctlr->chip = ata_match_chip(dev, ids)))
	return ENXIO;

    ata_set_desc(dev);
    ctlr->chipinit = ata_acard_chipinit;
    return 0;
}

static int
ata_acard_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev, ata_generic_intr))
	return ENXIO;

    ctlr->allocate = ata_acard_allocate;
    if (ctlr->chip->cfg1 == ATP_OLD) {
	ctlr->setmode = ata_acard_850_setmode;
	ctlr->locking = ata_serialize;
    }
    else
	ctlr->setmode = ata_acard_86X_setmode;
    return 0;
}

static int
ata_acard_allocate(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    /* setup the usual register normal pci style */
    if (ata_pci_allocate(dev))
	return ENXIO;

    ch->hw.status = ata_acard_status;
    return 0;
}

static int
ata_acard_status(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    if (ctlr->chip->cfg1 == ATP_OLD &&
	ATA_LOCKING(dev, ATA_LF_WHICH) != ch->unit)
	    return 0;
    if (ch->dma.flags & ATA_DMA_ACTIVE) {
	int bmstat = ATA_IDX_INB(ch, ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;

	if ((bmstat & (ATA_BMSTAT_ACTIVE | ATA_BMSTAT_INTERRUPT)) !=
	    ATA_BMSTAT_INTERRUPT)
	    return 0;
	ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, bmstat & ~ATA_BMSTAT_ERROR);
	DELAY(1);
	ATA_IDX_OUTB(ch, ATA_BMCMD_PORT,
		     ATA_IDX_INB(ch, ATA_BMCMD_PORT) & ~ATA_BMCMD_START_STOP);
	DELAY(1);
    }
    if (ATA_IDX_INB(ch, ATA_ALTSTAT) & ATA_S_BUSY) {
	DELAY(100);
	if (ATA_IDX_INB(ch, ATA_ALTSTAT) & ATA_S_BUSY)
	    return 0;
    }
    return 1;
}

static void
ata_acard_850_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int devno = (ch->unit << 1) + atadev->unit;
    int error;

    mode = ata_limit_mode(dev, mode,
			  ata_atapi(dev) ? ATA_PIO_MAX : ctlr->chip->max_dma);

    /* XXX SOS missing WDMA0+1 + PIO modes */
    if (mode >= ATA_WDMA2) {
	error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);
	if (bootverbose)
	    device_printf(dev, "%ssetting %s on %s chip\n",
			  (error) ? "FAILURE " : "",
			  ata_mode2str(mode), ctlr->chip->text);
	if (!error) {
	    u_int8_t reg54 = pci_read_config(gparent, 0x54, 1);
	    
	    reg54 &= ~(0x03 << (devno << 1));
	    if (mode >= ATA_UDMA0)
		reg54 |= (((mode & ATA_MODE_MASK) + 1) << (devno << 1));
	    pci_write_config(gparent, 0x54, reg54, 1);
	    pci_write_config(gparent, 0x4a, 0xa6, 1);
	    pci_write_config(gparent, 0x40 + (devno << 1), 0x0301, 2);
	    atadev->mode = mode;
	    return;
	}
    }
    /* we could set PIO mode timings, but we assume the BIOS did that */
}

static void
ata_acard_86X_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int devno = (ch->unit << 1) + atadev->unit;
    int error;


    mode = ata_limit_mode(dev, mode,
			  ata_atapi(dev) ? ATA_PIO_MAX : ctlr->chip->max_dma);

    mode = ata_check_80pin(dev, mode);

    /* XXX SOS missing WDMA0+1 + PIO modes */
    if (mode >= ATA_WDMA2) {
	error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);
	if (bootverbose)
	    device_printf(dev, "%ssetting %s on %s chip\n",
			  (error) ? "FAILURE " : "",
			  ata_mode2str(mode), ctlr->chip->text);
	if (!error) {
	    u_int16_t reg44 = pci_read_config(gparent, 0x44, 2);
	    
	    reg44 &= ~(0x000f << (devno << 2));
	    if (mode >= ATA_UDMA0)
		reg44 |= (((mode & ATA_MODE_MASK) + 1) << (devno << 2));
	    pci_write_config(gparent, 0x44, reg44, 2);
	    pci_write_config(gparent, 0x4a, 0xa6, 1);
	    pci_write_config(gparent, 0x40 + devno, 0x31, 1);
	    atadev->mode = mode;
	    return;
	}
    }
    /* we could set PIO mode timings, but we assume the BIOS did that */
}

struct ata_serialize {
    struct mtx  locked_mtx;
    int         locked_ch;
    int         restart_ch;
};

static int
ata_serialize(device_t dev, int flags)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_serialize *serial;
    static int inited = 0;
    int res;

    if (!inited) {
	serial = malloc(sizeof(struct ata_serialize),
			      M_TEMP, M_NOWAIT | M_ZERO);
	mtx_init(&serial->locked_mtx, "ATA serialize lock", NULL, MTX_DEF); 
	serial->locked_ch = -1;
	serial->restart_ch = -1;
	device_set_ivars(ctlr->dev, serial);
	inited = 1;
    }
    else
	serial = device_get_ivars(ctlr->dev);

    mtx_lock(&serial->locked_mtx);
    switch (flags) {
    case ATA_LF_LOCK:
	if (serial->locked_ch == -1)
	    serial->locked_ch = ch->unit;
	if (serial->locked_ch != ch->unit)
	    serial->restart_ch = ch->unit;
	break;

    case ATA_LF_UNLOCK:
	if (serial->locked_ch == ch->unit) {
	    serial->locked_ch = -1;
	    if (serial->restart_ch != -1) {
		if ((ch = ctlr->interrupt[serial->restart_ch].argument)) {
		    serial->restart_ch = -1;
		    mtx_unlock(&serial->locked_mtx);
		    ata_start(dev);
		    return -1;
		}
	    }
	}
	break;

    case ATA_LF_WHICH:
	break;
    }
    res = serial->locked_ch;
    mtx_unlock(&serial->locked_mtx);
    return res;
}

ATA_DECLARE_DRIVER(ata_acard);
