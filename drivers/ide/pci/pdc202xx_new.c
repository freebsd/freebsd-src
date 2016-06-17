/*
 *  Promise TX2/TX4/TX2000/133 IDE driver
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 *  Split from:
 *  linux/drivers/ide/pdc202xx.c	Version 0.35	Mar. 30, 2002
 *  Copyright (C) 1998-2002		Andre Hedrick <andre@linux-ide.org>
 *  Portions Copyright (C) 1999 Promise Technology, Inc.
 *  Author: Frank Tiernan (frankt@promise.com)
 *  Released under terms of General Public License
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/ide.h>

#include <asm/io.h>
#include <asm/irq.h>

#include "ide_modes.h"
#include "pdc202xx_new.h"

#define PDC202_DEBUG_CABLE	0

#if defined(DISPLAY_PDC202XX_TIMINGS) && defined(CONFIG_PROC_FS)
#include <linux/stat.h>
#include <linux/proc_fs.h>

static u8 pdcnew_proc = 0;
#define PDC202_MAX_DEVS		5
static struct pci_dev *pdc202_devs[PDC202_MAX_DEVS];
static int n_pdc202_devs;

static char * pdcnew_info(char *buf, struct pci_dev *dev)
{
	char *p = buf;

	p += sprintf(p, "\n                                ");
	switch(dev->device) {
		case PCI_DEVICE_ID_PROMISE_20277:
			p += sprintf(p, "SBFastTrak 133 Lite"); break;
		case PCI_DEVICE_ID_PROMISE_20276:
			p += sprintf(p, "MBFastTrak 133 Lite"); break;
		case PCI_DEVICE_ID_PROMISE_20275:
			p += sprintf(p, "MBUltra133"); break;
		case PCI_DEVICE_ID_PROMISE_20271:
			p += sprintf(p, "FastTrak TX2000"); break;
		case PCI_DEVICE_ID_PROMISE_20270:
			p += sprintf(p, "FastTrak LP/TX2/TX4"); break;
		case PCI_DEVICE_ID_PROMISE_20269:
			p += sprintf(p, "Ultra133 TX2"); break;
		case PCI_DEVICE_ID_PROMISE_20268:
			p += sprintf(p, "Ultra100 TX2"); break;
		default:
			p += sprintf(p, "Ultra series"); break;
			break;
	}
	p += sprintf(p, " Chipset.\n");
	return (char *)p;
}

static int pdcnew_get_info (char *buffer, char **addr, off_t offset, int count)
{
	char *p = buffer;
	int i, len;

	for (i = 0; i < n_pdc202_devs; i++) {
		struct pci_dev *dev	= pdc202_devs[i];
		p = pdcnew_info(buffer, dev);
	}
	/* p - buffer must be less than 4k! */
	len = (p - buffer) - offset;
	*addr = buffer + offset;
	
	return len > count ? count : len;
}
#endif  /* defined(DISPLAY_PDC202XX_TIMINGS) && defined(CONFIG_PROC_FS) */


static u8 pdcnew_ratemask (ide_drive_t *drive)
{
	u8 mode;

	switch(HWIF(drive)->pci_dev->device) {
		case PCI_DEVICE_ID_PROMISE_20277:
		case PCI_DEVICE_ID_PROMISE_20276:
		case PCI_DEVICE_ID_PROMISE_20275:
		case PCI_DEVICE_ID_PROMISE_20271:
		case PCI_DEVICE_ID_PROMISE_20269:
			mode = 4;
			break;
		case PCI_DEVICE_ID_PROMISE_20270:
		case PCI_DEVICE_ID_PROMISE_20268:
			mode = 3;
			break;
		default:
			return 0;
	}
	if (!eighty_ninty_three(drive))
		mode = min(mode, (u8)1);
	return mode;
}

static int check_in_drive_lists (ide_drive_t *drive, const char **list)
{
	struct hd_driveid *id = drive->id;

	if (pdc_quirk_drives == list) {
		while (*list) {
			if (strstr(id->model, *list++)) {
				return 2;
			}
		}
	} else {
		while (*list) {
			if (!strcmp(*list++,id->model)) {
				return 1;
			}
		}
	}
	return 0;
}

static int pdcnew_tune_chipset (ide_drive_t *drive, u8 xferspeed)
{
	ide_hwif_t *hwif	= HWIF(drive);
	struct pci_dev *dev	= hwif->pci_dev;
	u8 drive_pci		= 0x60 + (drive->dn << 2);
	u8 speed	= ide_rate_filter(pdcnew_ratemask(drive), xferspeed);

	u32			drive_conf;
	u8			AP, BP, CP, DP;
	u8			TA = 0, TB = 0, TC = 0;

	if ((drive->media != ide_disk) && (speed < XFER_SW_DMA_0))
		return -1;

	pci_read_config_dword(dev, drive_pci, &drive_conf);
	pci_read_config_byte(dev, (drive_pci), &AP);
	pci_read_config_byte(dev, (drive_pci)|0x01, &BP);
	pci_read_config_byte(dev, (drive_pci)|0x02, &CP);
	pci_read_config_byte(dev, (drive_pci)|0x03, &DP);

	if (speed < XFER_SW_DMA_0) {
		if ((AP & 0x0F) || (BP & 0x07)) {
			/* clear PIO modes of lower 8421 bits of A Register */
			pci_write_config_byte(dev, (drive_pci), AP &~0x0F);
			pci_read_config_byte(dev, (drive_pci), &AP);

			/* clear PIO modes of lower 421 bits of B Register */
			pci_write_config_byte(dev, (drive_pci)|0x01, BP &~0x07);
			pci_read_config_byte(dev, (drive_pci)|0x01, &BP);

			pci_read_config_byte(dev, (drive_pci), &AP);
			pci_read_config_byte(dev, (drive_pci)|0x01, &BP);
		}
	} else {
		if ((BP & 0xF0) && (CP & 0x0F)) {
			/* clear DMA modes of upper 842 bits of B Register */
			/* clear PIO forced mode upper 1 bit of B Register */
			pci_write_config_byte(dev, (drive_pci)|0x01, BP &~0xF0);
			pci_read_config_byte(dev, (drive_pci)|0x01, &BP);

			/* clear DMA modes of lower 8421 bits of C Register */
			pci_write_config_byte(dev, (drive_pci)|0x02, CP &~0x0F);
			pci_read_config_byte(dev, (drive_pci)|0x02, &CP);
		}
	}

	pci_read_config_byte(dev, (drive_pci), &AP);
	pci_read_config_byte(dev, (drive_pci)|0x01, &BP);
	pci_read_config_byte(dev, (drive_pci)|0x02, &CP);

	switch(speed) {
		case XFER_UDMA_6:	speed = XFER_UDMA_5;
		case XFER_UDMA_5:
		case XFER_UDMA_4:	TB = 0x20; TC = 0x01; break;
		case XFER_UDMA_2:	TB = 0x20; TC = 0x01; break;
		case XFER_UDMA_3:
		case XFER_UDMA_1:	TB = 0x40; TC = 0x02; break;
		case XFER_UDMA_0:
		case XFER_MW_DMA_2:	TB = 0x60; TC = 0x03; break;
		case XFER_MW_DMA_1:	TB = 0x60; TC = 0x04; break;
		case XFER_MW_DMA_0:
		case XFER_SW_DMA_2:	TB = 0x60; TC = 0x05; break;
		case XFER_SW_DMA_1:	TB = 0x80; TC = 0x06; break;
		case XFER_SW_DMA_0:	TB = 0xC0; TC = 0x0B; break;
		case XFER_PIO_4:	TA = 0x01; TB = 0x04; break;
		case XFER_PIO_3:	TA = 0x02; TB = 0x06; break;
		case XFER_PIO_2:	TA = 0x03; TB = 0x08; break;
		case XFER_PIO_1:	TA = 0x05; TB = 0x0C; break;
		case XFER_PIO_0:
		default:		TA = 0x09; TB = 0x13; break;
	}

	if (speed < XFER_SW_DMA_0) {
		pci_write_config_byte(dev, (drive_pci), AP|TA);
		pci_write_config_byte(dev, (drive_pci)|0x01, BP|TB);
	} else {
		pci_write_config_byte(dev, (drive_pci)|0x01, BP|TB);
		pci_write_config_byte(dev, (drive_pci)|0x02, CP|TC);
	}

#if PDC202XX_DECODE_REGISTER_INFO
	pci_read_config_byte(dev, (drive_pci), &AP);
	pci_read_config_byte(dev, (drive_pci)|0x01, &BP);
	pci_read_config_byte(dev, (drive_pci)|0x02, &CP);
	pci_read_config_byte(dev, (drive_pci)|0x03, &DP);

	decode_registers(REG_A, AP);
	decode_registers(REG_B, BP);
	decode_registers(REG_C, CP);
	decode_registers(REG_D, DP);
#endif /* PDC202XX_DECODE_REGISTER_INFO */
#if PDC202XX_DEBUG_DRIVE_INFO
	printk(KERN_DEBUG "%s: %s drive%d 0x%08x ",
		drive->name, ide_xfer_verbose(speed),
		drive->dn, drive_conf);
		pci_read_config_dword(dev, drive_pci, &drive_conf);
	printk("0x%08x\n", drive_conf);
#endif /* PDC202XX_DEBUG_DRIVE_INFO */

	return (ide_config_drive_speed(drive, speed));
}

static int pdcnew_new_tune_chipset (ide_drive_t *drive, u8 xferspeed)
{
	ide_hwif_t *hwif	= HWIF(drive);
	unsigned long indexreg	= hwif->dma_vendor1;
	unsigned long datareg	= hwif->dma_vendor3;
	u8 thold		= 0x10;
	u8 adj			= (drive->dn%2) ? 0x08 : 0x00;
	u8 speed		= ide_rate_filter(pdcnew_ratemask(drive), xferspeed);

	if (speed == XFER_UDMA_2) {
		hwif->OUTB((thold + adj), indexreg);
		hwif->OUTB((hwif->INB(datareg) & 0x7f), datareg);
	}

	switch (speed) {
		case XFER_UDMA_7:
			speed = XFER_UDMA_6;
		case XFER_UDMA_6:	set_ultra(0x1a, 0x01, 0xcb); break;
		case XFER_UDMA_5:	set_ultra(0x1a, 0x02, 0xcb); break;
		case XFER_UDMA_4:	set_ultra(0x1a, 0x03, 0xcd); break;
		case XFER_UDMA_3:	set_ultra(0x1a, 0x05, 0xcd); break;
		case XFER_UDMA_2:	set_ultra(0x2a, 0x07, 0xcd); break;
		case XFER_UDMA_1:	set_ultra(0x3a, 0x0a, 0xd0); break;
		case XFER_UDMA_0:	set_ultra(0x4a, 0x0f, 0xd5); break;
		case XFER_MW_DMA_2:	set_ata2(0x69, 0x25); break;
		case XFER_MW_DMA_1:	set_ata2(0x6b, 0x27); break;
		case XFER_MW_DMA_0:	set_ata2(0xdf, 0x5f); break;
		case XFER_PIO_4:	set_pio(0x23, 0x09, 0x25); break;
		case XFER_PIO_3:	set_pio(0x27, 0x0d, 0x35); break;
		case XFER_PIO_2:	set_pio(0x23, 0x26, 0x64); break;
		case XFER_PIO_1:	set_pio(0x46, 0x29, 0xa4); break;
		case XFER_PIO_0:	set_pio(0xfb, 0x2b, 0xac); break;
		default:
			;
	}

	return (ide_config_drive_speed(drive, speed));
}

/*   0    1    2    3    4    5    6   7   8
 * 960, 480, 390, 300, 240, 180, 120, 90, 60
 *           180, 150, 120,  90,  60
 * DMA_Speed
 * 180, 120,  90,  90,  90,  60,  30
 *  11,   5,   4,   3,   2,   1,   0
 */
static int config_chipset_for_pio (ide_drive_t *drive, u8 pio)
{
	u8 speed = 0;

	if (pio == 5) pio = 4;
	speed = XFER_PIO_0 + ide_get_best_pio_mode(drive, 255, pio, NULL);
        
	return ((int) pdcnew_tune_chipset(drive, speed));
}

static void pdcnew_tune_drive (ide_drive_t *drive, u8 pio)
{
	(void) config_chipset_for_pio(drive, pio);
}

static u8 pdcnew_new_cable_detect (ide_hwif_t *hwif)
{
	hwif->OUTB(0x0b, hwif->dma_vendor1);
	return ((u8)((hwif->INB(hwif->dma_vendor3) & 0x04)));
}
static int config_chipset_for_dma (ide_drive_t *drive)
{
	struct hd_driveid *id	= drive->id;
	ide_hwif_t *hwif	= HWIF(drive);
	struct pci_dev *dev	= hwif->pci_dev;
	u8 speed		= -1;
	u8 cable		= 0;

	u8 ultra_66		= ((id->dma_ultra & 0x0010) ||
				   (id->dma_ultra & 0x0008)) ? 1 : 0;

	switch(dev->device) {
		case PCI_DEVICE_ID_PROMISE_20277:
		case PCI_DEVICE_ID_PROMISE_20276:
		case PCI_DEVICE_ID_PROMISE_20275:
		case PCI_DEVICE_ID_PROMISE_20271:
		case PCI_DEVICE_ID_PROMISE_20269:
		case PCI_DEVICE_ID_PROMISE_20270:
		case PCI_DEVICE_ID_PROMISE_20268:
			cable = pdcnew_new_cable_detect(hwif);
#if PDC202_DEBUG_CABLE
			printk(KERN_DEBUG "%s: %s-pin cable, %s-pin cable, %d\n",
				hwif->name, hwif->udma_four ? "80" : "40",
				cable ? "40" : "80", cable);
#endif /* PDC202_DEBUG_CABLE */
			break;
		default:
			/* If it's not one we know we should never
			   arrive here.. */
			BUG();
	}

	/*
	 * Set the control register to use the 66Mhz system
	 * clock for UDMA 3/4 mode operation. If one drive on
	 * a channel is U66 capable but the other isn't we
	 * fall back to U33 mode. The BIOS INT 13 hooks turn
	 * the clock on then off for each read/write issued. I don't
	 * do that here because it would require modifying the
	 * kernel, separating the fop routines from the kernel or
	 * somehow hooking the fops calls. It may also be possible to
	 * leave the 66Mhz clock on and readjust the timing
	 * parameters.
	 */

	if ((ultra_66) && (cable)) {
#ifdef DEBUG
		printk(KERN_DEBUG "ULTRA 66/100/133: %s channel of Ultra 66/100/133 "
			"requires an 80-pin cable for Ultra66 operation.\n",
			hwif->channel ? "Secondary" : "Primary");
		printk(KERN_DEBUG "         Switching to Ultra33 mode.\n");
#endif /* DEBUG */
		/* Primary   : zero out second bit */
		/* Secondary : zero out fourth bit */
		printk(KERN_WARNING "Warning: %s channel requires an 80-pin cable for operation.\n", hwif->channel ? "Secondary":"Primary");
		printk(KERN_WARNING "%s reduced to Ultra33 mode.\n", drive->name);
	}

	if (drive->media != ide_disk)
		return 0;
	if (id->capability & 4) {	/* IORDY_EN & PREFETCH_EN */
		hwif->OUTB((0x13 + ((drive->dn%2) ? 0x08 : 0x00)), hwif->dma_vendor1);
		hwif->OUTB((hwif->INB(hwif->dma_vendor3)|0x03), hwif->dma_vendor3);
	}

	speed = ide_dma_speed(drive, pdcnew_ratemask(drive));

	if (!(speed)) {
		hwif->tuneproc(drive, 5);
		return 0;
	}

	(void) hwif->speedproc(drive, speed);
	return ide_dma_enable(drive);
}

static int pdcnew_config_drive_xfer_rate (ide_drive_t *drive)
{
	ide_hwif_t *hwif	= HWIF(drive);
	struct hd_driveid *id	= drive->id;

	drive->init_speed = 0;

	if ((id->capability & 1) && drive->autodma) {
		/* Consult the list of known "bad" drives */
		if (hwif->ide_dma_bad_drive(drive))
			goto fast_ata_pio;
		if (id->field_valid & 4) {
			if (id->dma_ultra & hwif->ultra_mask) {
				/* Force if Capable UltraDMA */
				int dma = config_chipset_for_dma(drive);
				if ((id->field_valid & 2) && !dma)
					goto try_dma_modes;
			}
		} else if (id->field_valid & 2) {
try_dma_modes:
			if ((id->dma_mword & hwif->mwdma_mask) ||
			    (id->dma_1word & hwif->swdma_mask)) {
				/* Force if Capable regular DMA modes */
				if (!config_chipset_for_dma(drive))
					goto no_dma_set;
			}
		} else if (hwif->ide_dma_good_drive(drive) &&
			    (id->eide_dma_time < 150)) {
				goto no_dma_set;
			/* Consult the list of known "good" drives */
			if (!config_chipset_for_dma(drive))
				goto no_dma_set;
		} else {
			goto fast_ata_pio;
		}
	} else if ((id->capability & 8) || (id->field_valid & 2)) {
fast_ata_pio:
no_dma_set:
		hwif->tuneproc(drive, 5);
		return hwif->ide_dma_off_quietly(drive);
	}
	return hwif->ide_dma_on(drive);
}

static int pdcnew_quirkproc (ide_drive_t *drive)
{
	return ((int) check_in_drive_lists(drive, pdc_quirk_drives));
}

static int pdcnew_ide_dma_lostirq(ide_drive_t *drive)
{
	if (HWIF(drive)->resetproc != NULL)
		HWIF(drive)->resetproc(drive);
	return __ide_dma_lostirq(drive);
}

static int pdcnew_ide_dma_timeout(ide_drive_t *drive)
{
	if (HWIF(drive)->resetproc != NULL)
		HWIF(drive)->resetproc(drive);
	return __ide_dma_timeout(drive);
}

static void pdcnew_new_reset (ide_drive_t *drive)
{
	/*
	 * Deleted this because it is redundant from the caller.
	 */
	printk(KERN_WARNING "PDC202XX: %s channel reset.\n",
		HWIF(drive)->channel ? "Secondary" : "Primary");
}

static void pdcnew_reset_host (ide_hwif_t *hwif)
{
//	unsigned long high_16	= hwif->dma_base - (8*(hwif->channel));
	unsigned long high_16	= hwif->dma_master;
	u8 udma_speed_flag	= hwif->INB(high_16|0x001f);

	hwif->OUTB((udma_speed_flag | 0x10), (high_16|0x001f));
	mdelay(100);
	hwif->OUTB((udma_speed_flag & ~0x10), (high_16|0x001f));
	mdelay(2000);	/* 2 seconds ?! */

	printk(KERN_WARNING "PDC202XX: %s channel reset.\n",
		hwif->channel ? "Secondary" : "Primary");
}

void pdcnew_reset (ide_drive_t *drive)
{
	ide_hwif_t *hwif	= HWIF(drive);
	ide_hwif_t *mate	= hwif->mate;
	
	pdcnew_reset_host(hwif);
	pdcnew_reset_host(mate);
#if 0
	/*
	 * FIXME: Have to kick all the drives again :-/
	 * What a pain in the ACE!
	 */
	if (hwif->present) {
		u16 hunit = 0;
		hwif->initializing = 1;
		for (hunit = 0; hunit < MAX_DRIVES; ++hunit) {
			ide_drive_t *hdrive = &hwif->drives[hunit];
			if (hdrive->present) {
				if (hwif->ide_dma_check)
					hwif->ide_dma_check(hdrive);
				else
					hwif->tuneproc(hdrive, 5);
			}
		}
		hwif->initializing = 0;
	}
	if (mate->present) {
		u16 munit = 0;
		mate->initializing = 1;
		for (munit = 0; munit < MAX_DRIVES; ++munit) {
			ide_drive_t *mdrive = &mate->drives[munit];
			if (mdrive->present) {
				if (mate->ide_dma_check) 
					mate->ide_dma_check(mdrive);
				else
					mate->tuneproc(mdrive, 5);
			}
		}
		mate->initializing = 0;
	}
#else
	hwif->tuneproc(drive, 5);
#endif
}

static unsigned int __init init_chipset_pdcnew (struct pci_dev *dev, const char *name)
{
	if (dev->resource[PCI_ROM_RESOURCE].start) {
		pci_write_config_dword(dev, PCI_ROM_ADDRESS,
			dev->resource[PCI_ROM_RESOURCE].start | PCI_ROM_ADDRESS_ENABLE);
		printk(KERN_INFO "%s: ROM enabled at 0x%08lx\n",
			name, dev->resource[PCI_ROM_RESOURCE].start);
	}

#if defined(DISPLAY_PDC202XX_TIMINGS) && defined(CONFIG_PROC_FS)
	pdc202_devs[n_pdc202_devs++] = dev;

	if (!pdcnew_proc) {
		pdcnew_proc = 1;
		ide_pci_register_host_proc(&pdcnew_procs[0]);
	}
#endif /* DISPLAY_PDC202XX_TIMINGS && CONFIG_PROC_FS */

	return dev->irq;
}

static void __init init_hwif_pdc202new (ide_hwif_t *hwif)
{
	hwif->autodma = 0;

	hwif->tuneproc  = &pdcnew_tune_drive;
	hwif->quirkproc = &pdcnew_quirkproc;
	hwif->speedproc = &pdcnew_new_tune_chipset;
	hwif->resetproc = &pdcnew_new_reset;

	if (!hwif->dma_base) {
		hwif->drives[0].autotune = hwif->drives[1].autotune = 1;
		return;
	}

	hwif->ultra_mask = 0x7f;
	hwif->mwdma_mask = 0x07;

	hwif->ide_dma_check = &pdcnew_config_drive_xfer_rate;
	hwif->ide_dma_lostirq = &pdcnew_ide_dma_lostirq;
	hwif->ide_dma_timeout = &pdcnew_ide_dma_timeout;
	if (!(hwif->udma_four))
		hwif->udma_four = (pdcnew_new_cable_detect(hwif)) ? 0 : 1;
	if (!noautodma)
		hwif->autodma = 1;
	hwif->drives[0].autodma = hwif->drives[1].autodma = hwif->autodma;
#if PDC202_DEBUG_CABLE
	printk(KERN_DEBUG "%s: %s-pin cable\n",
		hwif->name, hwif->udma_four ? "80" : "40");
#endif /* PDC202_DEBUG_CABLE */
}

static void __init init_dma_pdc202new (ide_hwif_t *hwif, unsigned long dmabase)
{
	ide_setup_dma(hwif, dmabase, 8);
}

extern void ide_setup_pci_device(struct pci_dev *, ide_pci_device_t *);
extern void ide_setup_pci_devices(struct pci_dev *, struct pci_dev *, ide_pci_device_t *);

static void __init init_setup_pdcnew (struct pci_dev *dev, ide_pci_device_t *d)
{
	ide_setup_pci_device(dev, d);
}

static void __init init_setup_pdc20270 (struct pci_dev *dev, ide_pci_device_t *d)
{
	struct pci_dev *findev;

	if ((dev->bus->self &&
	     dev->bus->self->vendor == PCI_VENDOR_ID_DEC) &&
	    (dev->bus->self->device == PCI_DEVICE_ID_DEC_21150)) {
		if (PCI_SLOT(dev->devfn) & 2) {
			return;
		}
		d->extra = 0;
		pci_for_each_dev(findev) {
			if ((findev->vendor == dev->vendor) &&
			    (findev->device == dev->device) &&
			    (PCI_SLOT(findev->devfn) & 2)) {
				u8 irq = 0, irq2 = 0;
				pci_read_config_byte(dev,
					PCI_INTERRUPT_LINE, &irq);
				pci_read_config_byte(findev,
					PCI_INTERRUPT_LINE, &irq2);
				if (irq != irq2) {
					findev->irq = dev->irq;
					pci_write_config_byte(findev,
						PCI_INTERRUPT_LINE, irq);
				}
				ide_setup_pci_devices(dev, findev, d);
				return;
			}
		}
	}
	ide_setup_pci_device(dev, d);
}

static void __init init_setup_pdc20276 (struct pci_dev *dev, ide_pci_device_t *d)
{
	if ((dev->bus->self) &&
	    (dev->bus->self->vendor == PCI_VENDOR_ID_INTEL) &&
	    ((dev->bus->self->device == PCI_DEVICE_ID_INTEL_I960) ||
	     (dev->bus->self->device == PCI_DEVICE_ID_INTEL_I960RM))) {
		printk(KERN_INFO "ide: Skipping Promise PDC20276 "
			"attached to I2O RAID controller.\n");
		return;
	}
	ide_setup_pci_device(dev, d);
}

/**
 *	pdc202new_init_one	-	called when a pdc202xx is found
 *	@dev: the pdc202new device
 *	@id: the matching pci id
 *
 *	Called when the PCI registration layer (or the IDE initialization)
 *	finds a device matching our IDE device tables.
 */
 
static int __devinit pdc202new_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	ide_pci_device_t *d = &pdcnew_chipsets[id->driver_data];

	if (dev->device != d->device)
		BUG();
	d->init_setup(dev, d);
	MOD_INC_USE_COUNT;
	return 0;
}

static struct pci_device_id pdc202new_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_PROMISE, PCI_DEVICE_ID_PROMISE_20268, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ PCI_VENDOR_ID_PROMISE, PCI_DEVICE_ID_PROMISE_20269, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 1},
	{ PCI_VENDOR_ID_PROMISE, PCI_DEVICE_ID_PROMISE_20270, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 2},
	{ PCI_VENDOR_ID_PROMISE, PCI_DEVICE_ID_PROMISE_20271, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 3},
	{ PCI_VENDOR_ID_PROMISE, PCI_DEVICE_ID_PROMISE_20275, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 4},
	{ PCI_VENDOR_ID_PROMISE, PCI_DEVICE_ID_PROMISE_20276, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 5},
	{ PCI_VENDOR_ID_PROMISE, PCI_DEVICE_ID_PROMISE_20277, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 6},
	{ 0, },
};

static struct pci_driver driver = {
	.name		= "Promise IDE",
	.id_table	= pdc202new_pci_tbl,
	.probe		= pdc202new_init_one,
};

static int pdc202new_ide_init(void)
{
	return ide_pci_register_driver(&driver);
}

static void pdc202new_ide_exit(void)
{
	ide_pci_unregister_driver(&driver);
}

module_init(pdc202new_ide_init);
module_exit(pdc202new_ide_exit);

MODULE_AUTHOR("Andre Hedrick, Frank Tiernan");
MODULE_DESCRIPTION("PCI driver module for Promise PDC20268 and higher");
MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;
