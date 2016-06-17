/*
 * linux/drivers/ide/pci/hpt366.c		Version 0.36	April 25, 2003
 *
 * Copyright (C) 1999-2003		Andre Hedrick <andre@linux-ide.org>
 * Portions Copyright (C) 2001	        Sun Microsystems, Inc.
 * Portions Copyright (C) 2003		Red Hat Inc
 *
 * Thanks to HighPoint Technologies for their assistance, and hardware.
 * Special Thanks to Jon Burchmore in SanDiego for the deep pockets, his
 * donation of an ABit BP6 mainboard, processor, and memory acellerated
 * development and support.
 *
 * Highpoint have their own driver (source except for the raid part)
 * available from http://www.highpoint-tech.com/hpt3xx-opensource-v131.tgz
 * This may be useful to anyone wanting to work on the mainstream hpt IDE.
 *
 * Note that final HPT370 support was done by force extraction of GPL.
 *
 * - add function for getting/setting power status of drive
 * - the HPT370's state machine can get confused. reset it before each dma 
 *   xfer to prevent that from happening.
 * - reset state engine whenever we get an error.
 * - check for busmaster state at end of dma. 
 * - use new highpoint timings.
 * - detect bus speed using highpoint register.
 * - use pll if we don't have a clock table. added a 66MHz table that's
 *   just 2x the 33MHz table.
 * - removed turnaround. NOTE: we never want to switch between pll and
 *   pci clocks as the chip can glitch in those cases. the highpoint
 *   approved workaround slows everything down too much to be useful. in
 *   addition, we would have to serialize access to each chip.
 * 	Adrian Sun <a.sun@sun.com>
 *
 * add drive timings for 66MHz PCI bus,
 * fix ATA Cable signal detection, fix incorrect /proc info
 * add /proc display for per-drive PIO/DMA/UDMA mode and
 * per-channel ATA-33/66 Cable detect.
 * 	Duncan Laurie <void@sun.com>
 *
 * fixup /proc output for multiple controllers
 *	Tim Hockin <thockin@sun.com>
 *
 * On hpt366: 
 * Reset the hpt366 on error, reset on dma
 * Fix disabling Fast Interrupt hpt366.
 * 	Mike Waychison <crlf@sun.com>
 *
 * Added support for 372N clocking and clock switching. The 372N needs
 * different clocks on read/write. This requires overloading rw_disk and
 * other deeply crazy things. Thanks to <http://www.hoerstreich.de> for
 * keeping me sane. 
 *		Alan Cox <alan@redhat.com>
 *
 */


#include <linux/config.h>
#include <linux/types.h>
#include <linux/module.h>
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

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>

#include "ide_modes.h"
#include "hpt366.h"

#if defined(DISPLAY_HPT366_TIMINGS) && defined(CONFIG_PROC_FS)
#include <linux/stat.h>
#include <linux/proc_fs.h>
#endif  /* defined(DISPLAY_HPT366_TIMINGS) && defined(CONFIG_PROC_FS) */

static unsigned int hpt_revision(struct pci_dev *dev);
static unsigned int hpt_minimum_revision(struct pci_dev *dev, int revision);

#if defined(DISPLAY_HPT366_TIMINGS) && defined(CONFIG_PROC_FS)

static u8 hpt366_proc = 0;
static struct pci_dev *hpt_devs[HPT366_MAX_DEVS];
static int n_hpt_devs;

static int hpt366_get_info (char *buffer, char **addr, off_t offset, int count)
{
	char *p	= buffer;
	char *chipset_nums[] = {"366", "366",  "368",
				"370", "370A", "372",
				"302", "371",  "374" };
	int i;

	p += sprintf(p, "\n                             "
		"HighPoint HPT366/368/370/372/374\n");
	for (i = 0; i < n_hpt_devs; i++) {
		struct pci_dev *dev = hpt_devs[i];
		unsigned long iobase = dev->resource[4].start;
		u32 class_rev = hpt_revision(dev);
		u8 c0, c1;

		p += sprintf(p, "\nController: %d\n", i);
		if(class_rev < 9)
			p += sprintf(p, "Chipset: HPT%s\n", chipset_nums[class_rev]);
		else
			p += sprintf(p, "Chipset: HPT revision %d\n", class_rev);
		p += sprintf(p, "--------------- Primary Channel "
				"--------------- Secondary Channel "
				"--------------\n");

		/* get the bus master status registers */
		c0 = inb(iobase + 0x2);
		c1 = inb(iobase + 0xa);
		p += sprintf(p, "Enabled:        %s"
				"                             %s\n",
			(c0 & 0x80) ? "no" : "yes",
			(c1 & 0x80) ? "no" : "yes");

		if (hpt_minimum_revision(dev, 3)) {
			u8 cbl;
			cbl = inb(iobase + 0x7b);
			outb(cbl | 1, iobase + 0x7b);
			outb(cbl & ~1, iobase + 0x7b);
			cbl = inb(iobase + 0x7a);
			p += sprintf(p, "Cable:          ATA-%d"
					"                          ATA-%d\n",
				(cbl & 0x02) ? 33 : 66,
				(cbl & 0x01) ? 33 : 66);
			p += sprintf(p, "\n");
		}

		p += sprintf(p, "--------------- drive0 --------- drive1 "
				"------- drive0 ---------- drive1 -------\n");
		p += sprintf(p, "DMA capable:    %s              %s" 
				"            %s               %s\n",
			(c0 & 0x20) ? "yes" : "no ", 
			(c0 & 0x40) ? "yes" : "no ",
			(c1 & 0x20) ? "yes" : "no ", 
			(c1 & 0x40) ? "yes" : "no ");

		{
			u8 c2, c3;
			/* older revs don't have these registers mapped 
			 * into io space */
			pci_read_config_byte(dev, 0x43, &c0);
			pci_read_config_byte(dev, 0x47, &c1);
			pci_read_config_byte(dev, 0x4b, &c2);
			pci_read_config_byte(dev, 0x4f, &c3);

			p += sprintf(p, "Mode:           %s             %s"
					"           %s              %s\n",
				(c0 & 0x10) ? "UDMA" : (c0 & 0x20) ? "DMA " : 
					(c0 & 0x80) ? "PIO " : "off ",
				(c1 & 0x10) ? "UDMA" : (c1 & 0x20) ? "DMA " :
					(c1 & 0x80) ? "PIO " : "off ",
				(c2 & 0x10) ? "UDMA" : (c2 & 0x20) ? "DMA " :
					(c2 & 0x80) ? "PIO " : "off ",
				(c3 & 0x10) ? "UDMA" : (c3 & 0x20) ? "DMA " :
					(c3 & 0x80) ? "PIO " : "off ");
		}
	}
	p += sprintf(p, "\n");
	
	return p-buffer;/* => must be less than 4k! */
}
#endif  /* defined(DISPLAY_HPT366_TIMINGS) && defined(CONFIG_PROC_FS) */

/*
 *	This wants fixing so that we do everything not by classrev
 *	(which breaks on the newest chips) but by creating an
 *	enumeration of chip variants and using that
 */
 

static u32 hpt_revision (struct pci_dev *dev)
{
	u32 class_rev;
	pci_read_config_dword(dev, PCI_CLASS_REVISION, &class_rev);
	class_rev &= 0xff;

	switch(dev->device) {
		/* Remap new 372N onto 372 */
		case PCI_DEVICE_ID_TTI_HPT372N:
			class_rev = PCI_DEVICE_ID_TTI_HPT372; break;
		case PCI_DEVICE_ID_TTI_HPT374:
			class_rev = PCI_DEVICE_ID_TTI_HPT374; break;
		case PCI_DEVICE_ID_TTI_HPT371:
			class_rev = PCI_DEVICE_ID_TTI_HPT371; break;
		case PCI_DEVICE_ID_TTI_HPT302:
			class_rev = PCI_DEVICE_ID_TTI_HPT302; break;
		case PCI_DEVICE_ID_TTI_HPT372:
			class_rev = PCI_DEVICE_ID_TTI_HPT372; break;
		default:
			break;
	}
	return class_rev;
}

static u32 hpt_minimum_revision (struct pci_dev *dev, int revision)
{
	unsigned int class_rev = hpt_revision(dev);
	revision--;
	return ((int) (class_rev > revision) ? 1 : 0);
}

static int check_in_drive_lists(ide_drive_t *drive, const char **list);

static u8 hpt3xx_ratemask (ide_drive_t *drive)
{
	struct pci_dev *dev	= HWIF(drive)->pci_dev;
	u8 mode			= 0;

	if (hpt_minimum_revision(dev, 8)) {		/* HPT374 */
		mode = (HPT374_ALLOW_ATA133_6) ? 4 : 3;
	} else if (hpt_minimum_revision(dev, 7)) {	/* HPT371 */
		mode = (HPT371_ALLOW_ATA133_6) ? 4 : 3;
	} else if (hpt_minimum_revision(dev, 6)) {	/* HPT302 */
		mode = (HPT302_ALLOW_ATA133_6) ? 4 : 3;
	} else if (hpt_minimum_revision(dev, 5)) {	/* HPT372 */
		mode = (HPT372_ALLOW_ATA133_6) ? 4 : 3;
	} else if (hpt_minimum_revision(dev, 4)) {	/* HPT370A */
		mode = (HPT370_ALLOW_ATA100_5) ? 3 : 2;
	} else if (hpt_minimum_revision(dev, 3)) {	/* HPT370 */
		mode = (HPT370_ALLOW_ATA100_5) ? 3 : 2;
		mode = (check_in_drive_lists(drive, bad_ata33)) ? 0 : mode;
	} else {				/* HPT366 and HPT368 */
		mode = (check_in_drive_lists(drive, bad_ata33)) ? 0 : 2;
	}
	if (!eighty_ninty_three(drive) && (mode))
		mode = min(mode, (u8)1);
	return mode;
}

/*
 *	Note for the future; the SATA hpt37x we must set
 *	either PIO or UDMA modes 0,4,5
 */
 
static u8 hpt3xx_ratefilter (ide_drive_t *drive, u8 speed)
{
	struct pci_dev *dev	= HWIF(drive)->pci_dev;
	u8 mode			= hpt3xx_ratemask(drive);

	if (drive->media != ide_disk)
		return min(speed, (u8)XFER_PIO_4);

	switch(mode) {
		case 0x04:
			speed = min(speed, (u8)XFER_UDMA_6);
			break;
		case 0x03:
			speed = min(speed, (u8)XFER_UDMA_5);
			if (hpt_minimum_revision(dev, 5))
				break;
			if (check_in_drive_lists(drive, bad_ata100_5))
				speed = min(speed, (u8)XFER_UDMA_4);
			break;
		case 0x02:
			speed = min(speed, (u8)XFER_UDMA_4);
	/*
	 * CHECK ME, Does this need to be set to 5 ??
	 */
			if (hpt_minimum_revision(dev, 3))
				break;
			if ((check_in_drive_lists(drive, bad_ata66_4)) ||
			    (!(HPT366_ALLOW_ATA66_4)))
				speed = min(speed, (u8)XFER_UDMA_3);
			if ((check_in_drive_lists(drive, bad_ata66_3)) ||
			    (!(HPT366_ALLOW_ATA66_3)))
				speed = min(speed, (u8)XFER_UDMA_2);
			break;
		case 0x01:
			speed = min(speed, (u8)XFER_UDMA_2);
	/*
	 * CHECK ME, Does this need to be set to 5 ??
	 */
			if (hpt_minimum_revision(dev, 3))
				break;
			if (check_in_drive_lists(drive, bad_ata33))
				speed = min(speed, (u8)XFER_MW_DMA_2);
			break;
		case 0x00:
		default:
			speed = min(speed, (u8)XFER_MW_DMA_2);
			break;
	}
	return speed;
}

static int check_in_drive_lists (ide_drive_t *drive, const char **list)
{
	struct hd_driveid *id = drive->id;

	if (quirk_drives == list) {
		while (*list)
			if (strstr(id->model, *list++))
				return 1;
	} else {
		while (*list)
			if (!strcmp(*list++,id->model))
				return 1;
	}
	return 0;
}

static unsigned int pci_bus_clock_list (u8 speed, struct chipset_bus_clock_list_entry * chipset_table)
{
	for ( ; chipset_table->xfer_speed ; chipset_table++)
		if (chipset_table->xfer_speed == speed)
			return chipset_table->chipset_settings;
	return chipset_table->chipset_settings;
}

static void hpt366_tune_chipset (ide_drive_t *drive, u8 xferspeed)
{
	struct pci_dev *dev	= HWIF(drive)->pci_dev;
	u8 speed		= hpt3xx_ratefilter(drive, xferspeed);
//	u8 speed		= ide_rate_filter(hpt3xx_ratemask(drive), xferspeed);
	u8 regtime		= (drive->select.b.unit & 0x01) ? 0x44 : 0x40;
	u8 regfast		= (HWIF(drive)->channel) ? 0x55 : 0x51;
	u8 drive_fast		= 0;
	u32 reg1 = 0, reg2	= 0;

	/*
	 * Disable the "fast interrupt" prediction.
	 */
	pci_read_config_byte(dev, regfast, &drive_fast);
#if 0
	if (drive_fast & 0x02)
		pci_write_config_byte(dev, regfast, drive_fast & ~0x20);
#else
	if (drive_fast & 0x80)
		pci_write_config_byte(dev, regfast, drive_fast & ~0x80);
#endif

	reg2 = pci_bus_clock_list(speed,
		(struct chipset_bus_clock_list_entry *) pci_get_drvdata(dev));
	/*
	 * Disable on-chip PIO FIFO/buffer
	 *  (to avoid problems handling I/O errors later)
	 */
	pci_read_config_dword(dev, regtime, &reg1);
	if (speed >= XFER_MW_DMA_0) {
		reg2 = (reg2 & ~0xc0000000) | (reg1 & 0xc0000000);
	} else {
		reg2 = (reg2 & ~0x30070000) | (reg1 & 0x30070000);
	}	
	reg2 &= ~0x80000000;

	pci_write_config_dword(dev, regtime, reg2);
}

static void hpt368_tune_chipset (ide_drive_t *drive, u8 speed)
{
	hpt366_tune_chipset(drive, speed);
}

static void hpt370_tune_chipset (ide_drive_t *drive, u8 xferspeed)
{
	struct pci_dev *dev = HWIF(drive)->pci_dev;
	u8 speed	= hpt3xx_ratefilter(drive, xferspeed);
//	u8 speed	= ide_rate_filter(hpt3xx_ratemask(drive), xferspeed);
	u8 regfast	= (HWIF(drive)->channel) ? 0x55 : 0x51;
	u8 drive_pci	= 0x40 + (drive->dn * 4);
	u8 new_fast	= 0, drive_fast = 0;
	u32 list_conf	= 0, drive_conf = 0;
	u32 conf_mask	= (speed >= XFER_MW_DMA_0) ? 0xc0000000 : 0x30070000;

	/*
	 * Disable the "fast interrupt" prediction.
	 * don't holdoff on interrupts. (== 0x01 despite what the docs say) 
	 */
	pci_read_config_byte(dev, regfast, &drive_fast);
	new_fast = drive_fast;
	if (new_fast & 0x02)
		new_fast &= ~0x02;

#ifdef HPT_DELAY_INTERRUPT
	if (new_fast & 0x01)
		new_fast &= ~0x01;
#else
	if ((new_fast & 0x01) == 0)
		new_fast |= 0x01;
#endif
	if (new_fast != drive_fast)
		pci_write_config_byte(dev, regfast, new_fast);

	list_conf = pci_bus_clock_list(speed, 
				       (struct chipset_bus_clock_list_entry *)
				       pci_get_drvdata(dev));

	pci_read_config_dword(dev, drive_pci, &drive_conf);
	list_conf = (list_conf & ~conf_mask) | (drive_conf & conf_mask);
	
	if (speed < XFER_MW_DMA_0) {
		list_conf &= ~0x80000000; /* Disable on-chip PIO FIFO/buffer */
	}

	pci_write_config_dword(dev, drive_pci, list_conf);
}

static void hpt372_tune_chipset (ide_drive_t *drive, u8 xferspeed)
{
	struct pci_dev *dev	= HWIF(drive)->pci_dev;
	u8 speed	= hpt3xx_ratefilter(drive, xferspeed);
//	u8 speed	= ide_rate_filter(hpt3xx_ratemask(drive), xferspeed);
	u8 regfast	= (HWIF(drive)->channel) ? 0x55 : 0x51;
	u8 drive_fast	= 0, drive_pci = 0x40 + (drive->dn * 4);
	u32 list_conf	= 0, drive_conf = 0;
	u32 conf_mask	= (speed >= XFER_MW_DMA_0) ? 0xc0000000 : 0x30070000;
	/*
	 * Disable the "fast interrupt" prediction.
	 * don't holdoff on interrupts. (== 0x01 despite what the docs say)
	 */
	pci_read_config_byte(dev, regfast, &drive_fast);
	drive_fast &= ~0x07;
	pci_write_config_byte(dev, regfast, drive_fast);
					
	list_conf = pci_bus_clock_list(speed,
			(struct chipset_bus_clock_list_entry *)
					pci_get_drvdata(dev));
	pci_read_config_dword(dev, drive_pci, &drive_conf);
	list_conf = (list_conf & ~conf_mask) | (drive_conf & conf_mask);
	if (speed < XFER_MW_DMA_0)
		list_conf &= ~0x80000000; /* Disable on-chip PIO FIFO/buffer */
	pci_write_config_dword(dev, drive_pci, list_conf);
}

static void hpt374_tune_chipset (ide_drive_t *drive, u8 speed)
{
	hpt372_tune_chipset(drive, speed);
}

static int hpt3xx_tune_chipset (ide_drive_t *drive, u8 speed)
{
	struct pci_dev *dev	= HWIF(drive)->pci_dev;

	if (hpt_minimum_revision(dev, 8))
		hpt374_tune_chipset(drive, speed);
#if 0
	else if (hpt_minimum_revision(dev, 7))
		hpt371_tune_chipset(drive, speed);
	else if (hpt_minimum_revision(dev, 6))
		hpt302_tune_chipset(drive, speed);
#endif
	else if (hpt_minimum_revision(dev, 5))
		hpt372_tune_chipset(drive, speed);
	else if (hpt_minimum_revision(dev, 3))
		hpt370_tune_chipset(drive, speed);
	else if (hpt_minimum_revision(dev, 2))
		hpt368_tune_chipset(drive, speed);
	else
                hpt366_tune_chipset(drive, speed);

	return ((int) ide_config_drive_speed(drive, speed));
}

static void hpt3xx_tune_drive (ide_drive_t *drive, u8 pio)
{
	pio = ide_get_best_pio_mode(drive, pio, 5, NULL);
	(void) hpt3xx_tune_chipset(drive, (XFER_PIO_0 + pio));
}

/*
 * This allows the configuration of ide_pci chipset registers
 * for cards that learn about the drive's UDMA, DMA, PIO capabilities
 * after the drive is reported by the OS.  Initally for designed for
 * HPT366 UDMA chipset by HighPoint|Triones Technologies, Inc.
 *
 * check_in_drive_lists(drive, bad_ata66_4)
 * check_in_drive_lists(drive, bad_ata66_3)
 * check_in_drive_lists(drive, bad_ata33)
 *
 */
static int config_chipset_for_dma (ide_drive_t *drive)
{
	u8 speed = ide_dma_speed(drive, hpt3xx_ratemask(drive));

	if (!speed)
		return 0;

	if (pci_get_drvdata(HWIF(drive)->pci_dev) == NULL)
		return 0;
		
	(void) hpt3xx_tune_chipset(drive, speed);
	return ide_dma_enable(drive);
}

static int hpt3xx_quirkproc (ide_drive_t *drive)
{
	return ((int) check_in_drive_lists(drive, quirk_drives));
}

static void hpt3xx_intrproc (ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);

	if (drive->quirk_list)
		return;
	/* drives in the quirk_list may not like intr setups/cleanups */
	hwif->OUTB(drive->ctl|2, IDE_CONTROL_REG);
}

static void hpt3xx_maskproc (ide_drive_t *drive, int mask)
{
	struct pci_dev *dev = HWIF(drive)->pci_dev;

	if (drive->quirk_list) {
		if (hpt_minimum_revision(dev,3)) {
			u8 reg5a = 0;
			pci_read_config_byte(dev, 0x5a, &reg5a);
			if (((reg5a & 0x10) >> 4) != mask)
				pci_write_config_byte(dev, 0x5a, mask ? (reg5a | 0x10) : (reg5a & ~0x10));
		} else {
			if (mask) {
				disable_irq(HWIF(drive)->irq);
			} else {
				enable_irq(HWIF(drive)->irq);
			}
		}
	} else {
		if (IDE_CONTROL_REG)
			HWIF(drive)->OUTB(mask ? (drive->ctl | 2) :
						 (drive->ctl & ~2),
						 IDE_CONTROL_REG);
	}
}

static int hpt366_config_drive_xfer_rate (ide_drive_t *drive)
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
			if (id->dma_mword & hwif->mwdma_mask) {
				/* Force if Capable regular DMA modes */
				if (!config_chipset_for_dma(drive))
					goto no_dma_set;
			}
		} else if (hwif->ide_dma_good_drive(drive) &&
			   (id->eide_dma_time < 150)) {
			/* Consult the list of known "good" drives */
			if (!config_chipset_for_dma(drive))
				goto no_dma_set;
		} else {
			goto fast_ata_pio;
		}
	} else if ((id->capability & 8) || (id->field_valid & 2)) {
fast_ata_pio:
no_dma_set:
		hpt3xx_tune_drive(drive, 5);
		return hwif->ide_dma_off_quietly(drive);
	}
	return hwif->ide_dma_on(drive);
}

/*
 * This is specific to the HPT366 UDMA bios chipset
 * by HighPoint|Triones Technologies, Inc.
 */
static int hpt366_ide_dma_lostirq (ide_drive_t *drive)
{
	struct pci_dev *dev	= HWIF(drive)->pci_dev;
	u8 reg50h = 0, reg52h = 0, reg5ah = 0;

	pci_read_config_byte(dev, 0x50, &reg50h);
	pci_read_config_byte(dev, 0x52, &reg52h);
	pci_read_config_byte(dev, 0x5a, &reg5ah);
	printk("%s: (%s)  reg50h=0x%02x, reg52h=0x%02x, reg5ah=0x%02x\n",
		drive->name, __FUNCTION__, reg50h, reg52h, reg5ah);
	if (reg5ah & 0x10)
		pci_write_config_byte(dev, 0x5a, reg5ah & ~0x10);
#if 0
	/* how about we flush and reset, mmmkay? */
	pci_write_config_byte(dev, 0x51, 0x1F);
	/* fall through to a reset */
	case ide_dma_begin:
	case ide_dma_end:
	/* reset the chips state over and over.. */
	pci_write_config_byte(dev, 0x51, 0x13);
#endif
	return __ide_dma_lostirq(drive);
}

static void hpt370_clear_engine (ide_drive_t *drive)
{
	u8 regstate = HWIF(drive)->channel ? 0x54 : 0x50;
	pci_write_config_byte(HWIF(drive)->pci_dev, regstate, 0x37);
	udelay(10);
}

static int hpt370_ide_dma_begin (ide_drive_t *drive)
{
#ifdef HPT_RESET_STATE_ENGINE
	hpt370_clear_engine(drive);
#endif
	return __ide_dma_begin(drive);
}

static int hpt370_ide_dma_end (ide_drive_t *drive)
{
	ide_hwif_t *hwif	= HWIF(drive);
	u8 dma_stat		= hwif->INB(hwif->dma_status);

	if (dma_stat & 0x01) {
		/* wait a little */
		udelay(20);
		dma_stat = hwif->INB(hwif->dma_status);
	}
	if ((dma_stat & 0x01) != 0) 
		/* fallthrough */
		(void) HWIF(drive)->ide_dma_timeout(drive);

	return __ide_dma_end(drive);
}

static void hpt370_lostirq_timeout (ide_drive_t *drive)
{
	ide_hwif_t *hwif	= HWIF(drive);
	u8 bfifo = 0, reginfo	= hwif->channel ? 0x56 : 0x52;
	u8 dma_stat = 0, dma_cmd = 0;

	pci_read_config_byte(HWIF(drive)->pci_dev, reginfo, &bfifo);
	printk("%s: %d bytes in FIFO\n", drive->name, bfifo);
	hpt370_clear_engine(drive);
	/* get dma command mode */
	dma_cmd = hwif->INB(hwif->dma_command);
	/* stop dma */
	hwif->OUTB(dma_cmd & ~0x1, hwif->dma_command);
	dma_stat = hwif->INB(hwif->dma_status);
	/* clear errors */
	hwif->OUTB(dma_stat | 0x6, hwif->dma_status);
}

static int hpt370_ide_dma_timeout (ide_drive_t *drive)
{
	hpt370_lostirq_timeout(drive);
	hpt370_clear_engine(drive);
	return __ide_dma_timeout(drive);
}

static int hpt370_ide_dma_lostirq (ide_drive_t *drive)
{
	hpt370_lostirq_timeout(drive);
	hpt370_clear_engine(drive);
	return __ide_dma_lostirq(drive);
}

/* returns 1 if DMA IRQ issued, 0 otherwise */
static int hpt374_ide_dma_test_irq(ide_drive_t *drive)
{
	ide_hwif_t *hwif	= HWIF(drive);
	u16 bfifo		= 0;
	u8 reginfo		= hwif->channel ? 0x56 : 0x52;
	u8 dma_stat;

	pci_read_config_word(hwif->pci_dev, reginfo, &bfifo);
	if (bfifo & 0x1FF) {
//		printk("%s: %d bytes in FIFO\n", drive->name, bfifo);
		return 0;
	}

	dma_stat = hwif->INB(hwif->dma_status);
	/* return 1 if INTR asserted */
	if ((dma_stat & 4) == 4)
		return 1;

	if (!drive->waiting_for_dma)
		printk(KERN_WARNING "%s: (%s) called while not waiting\n",
				drive->name, __FUNCTION__);
	return 0;
}

static int hpt374_ide_dma_end (ide_drive_t *drive)
{
	struct pci_dev *dev	= HWIF(drive)->pci_dev;
	ide_hwif_t *hwif	= HWIF(drive);
	u8 msc_stat = 0, mscreg	= hwif->channel ? 0x54 : 0x50;
	u8 bwsr_stat = 0, bwsr_mask = hwif->channel ? 0x02 : 0x01;

	pci_read_config_byte(dev, 0x6a, &bwsr_stat);
	pci_read_config_byte(dev, mscreg, &msc_stat);
	if ((bwsr_stat & bwsr_mask) == bwsr_mask)
		pci_write_config_byte(dev, mscreg, msc_stat|0x30);
	return __ide_dma_end(drive);
}

/**
 *	hpt372n_set_clock	-	perform clock switching dance
 *	@drive: Drive to switch
 *	@mode: Switching mode (0x21 for write, 0x23 otherwise)
 *
 *	Switch the DPLL clock on the HPT372N devices. This is a
 *	right mess.
 */
 
static void hpt372n_set_clock(ide_drive_t *drive, int mode)
{
	ide_hwif_t *hwif	= HWIF(drive);
	
	/* FIXME: should we check for DMA active and BUG() */
	/* Tristate the bus */
	outb(0x80, hwif->dma_base+0x73);
	outb(0x80, hwif->dma_base+0x77);
	
	/* Switch clock and reset channels */
	outb(mode, hwif->dma_base+0x7B);
	outb(0xC0, hwif->dma_base+0x79);
	
	/* Reset state machines */
	outb(0x37, hwif->dma_base+0x70);
	outb(0x37, hwif->dma_base+0x74);
	
	/* Complete reset */
	outb(0x00, hwif->dma_base+0x79);
	
	/* Reconnect channels to bus */
	outb(0x00, hwif->dma_base+0x73);
	outb(0x00, hwif->dma_base+0x79);
}

/**
 *	hpt372n_rw_disk		-	wrapper for I/O
 *	@drive: drive for command
 *	@rq: block request structure
 *	@block: block number
 *
 *	This is called when a disk I/O is issued to the 372N instead
 *	of the default functionality. We need it because of the clock
 *	switching
 *
 */
 
static ide_startstop_t hpt372n_rw_disk(ide_drive_t *drive, struct request *rq, unsigned long block)
{
	int wantclock;
	
	if(rq_data_dir(rq) == READ)
		wantclock = 0x21;
	else
		wantclock = 0x23;
		
	if(HWIF(drive)->config_data != wantclock)
	{
		hpt372n_set_clock(drive, wantclock);
		HWIF(drive)->config_data = wantclock;
	}
	return __ide_do_rw_disk(drive, rq, block);
}

/*
 * Since SUN Cobalt is attempting to do this operation, I should disclose
 * this has been a long time ago Thu Jul 27 16:40:57 2000 was the patch date
 * HOTSWAP ATA Infrastructure.
 */

static void hpt3xx_reset (ide_drive_t *drive)
{
#if 0
	unsigned long high_16	= pci_resource_start(HWIF(drive)->pci_dev, 4);
	u8 reset	= (HWIF(drive)->channel) ? 0x80 : 0x40;
	u8 reg59h	= 0;

	pci_read_config_byte(HWIF(drive)->pci_dev, 0x59, &reg59h);
	pci_write_config_byte(HWIF(drive)->pci_dev, 0x59, reg59h|reset);
	pci_write_config_byte(HWIF(drive)->pci_dev, 0x59, reg59h);
#endif
}

static int hpt3xx_tristate (ide_drive_t * drive, int state)
{
	ide_hwif_t *hwif	= HWIF(drive);
	struct pci_dev *dev	= hwif->pci_dev;
	u8 reg59h = 0, reset	= (hwif->channel) ? 0x80 : 0x40;
	u8 regXXh = 0, state_reg= (hwif->channel) ? 0x57 : 0x53;

	if (!hwif)
		return -EINVAL;

//	hwif->bus_state = state;

	pci_read_config_byte(dev, 0x59, &reg59h);
	pci_read_config_byte(dev, state_reg, &regXXh);

	switch(state)
	{
		case BUSSTATE_ON:
			(void) ide_do_reset(drive);
			pci_write_config_byte(dev, state_reg, regXXh|0x80);
			pci_write_config_byte(dev, 0x59, reg59h|reset);
			break;
		case BUSSTATE_OFF:
			pci_write_config_byte(dev, 0x59, reg59h & ~(reset));
			pci_write_config_byte(dev, state_reg, regXXh & ~(0x80));
			(void) ide_do_reset(drive);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

/* 
 * set/get power state for a drive.
 * turning the power off does the following things:
 *   1) soft-reset the drive
 *   2) tri-states the ide bus
 *
 * when we turn things back on, we need to re-initialize things.
 */
#define TRISTATE_BIT  0x8000
static int hpt370_busproc(ide_drive_t * drive, int state)
{
	ide_hwif_t *hwif	= HWIF(drive);
	struct pci_dev *dev	= hwif->pci_dev;
	u8 tristate = 0, resetmask = 0, bus_reg = 0;
	u16 tri_reg;

	if (!hwif)
		return -EINVAL;

	hwif->bus_state = state;

	if (hwif->channel) { 
		/* secondary channel */
		tristate = 0x56;
		resetmask = 0x80; 
	} else { 
		/* primary channel */
		tristate = 0x52;
		resetmask = 0x40;
	}

	/* grab status */
	pci_read_config_word(dev, tristate, &tri_reg);
	pci_read_config_byte(dev, 0x59, &bus_reg);

	/* set the state. we don't set it if we don't need to do so.
	 * make sure that the drive knows that it has failed if it's off */
	switch (state) {
	case BUSSTATE_ON:
		hwif->drives[0].failures = 0;
		hwif->drives[1].failures = 0;
		if ((bus_reg & resetmask) == 0)
			return 0;
		tri_reg &= ~TRISTATE_BIT;
		bus_reg &= ~resetmask;
		break;
	case BUSSTATE_OFF:
		hwif->drives[0].failures = hwif->drives[0].max_failures + 1;
		hwif->drives[1].failures = hwif->drives[1].max_failures + 1;
		if ((tri_reg & TRISTATE_BIT) == 0 && (bus_reg & resetmask))
			return 0;
		tri_reg &= ~TRISTATE_BIT;
		bus_reg |= resetmask;
		break;
	case BUSSTATE_TRISTATE:
		hwif->drives[0].failures = hwif->drives[0].max_failures + 1;
		hwif->drives[1].failures = hwif->drives[1].max_failures + 1;
		if ((tri_reg & TRISTATE_BIT) && (bus_reg & resetmask))
			return 0;
		tri_reg |= TRISTATE_BIT;
		bus_reg |= resetmask;
		break;
	default:
		return -EINVAL;
	}
	pci_write_config_byte(dev, 0x59, bus_reg);
	pci_write_config_word(dev, tristate, tri_reg);

	return 0;
}

static int __init init_hpt37x(struct pci_dev *dev)
{
	int adjust, i;
	u16 freq;
	u32 pll;
	u8 reg5bh;
	u8 reg5ah;
	unsigned long dmabase = pci_resource_start(dev, 4);
	u8 did, rid;	
	int is_372n = 0;
#if 1
	pci_read_config_byte(dev, 0x5a, &reg5ah);
	/* interrupt force enable */
	pci_write_config_byte(dev, 0x5a, (reg5ah & ~0x10));
#endif

	if(dmabase)
	{
		did = inb(dmabase + 0x22);
		rid = inb(dmabase + 0x28);
	
		if((did == 4 && rid == 6) || (did == 5 && rid > 1))
			is_372n = 1;
	}
		
	/*
	 * default to pci clock. make sure MA15/16 are set to output
	 * to prevent drives having problems with 40-pin cables. Needed
	 * for some drives such as IBM-DTLA which will not enter ready
	 * state on reset when PDIAG is a input.
	 *
	 * ToDo: should we set 0x21 when using PLL mode ?
	 */
	pci_write_config_byte(dev, 0x5b, 0x23);

	/*
	 * set up the PLL. we need to adjust it so that it's stable. 
	 * freq = Tpll * 192 / Tpci
	 *
	 * Todo. For non x86 should probably check the dword is
	 * set to 0xABCDExxx indicating the BIOS saved f_CNT
	 */
	pci_read_config_word(dev, 0x78, &freq);
	freq &= 0x1FF;
	
	/*
	 * The 372N uses different PCI clock information and has
	 * some other complications
	 *	On PCI33 timing we must clock switch
	 *	On PCI66 timing we must NOT use the PCI clock
	 *
	 * Currently we always set up the PLL for the 372N
	 */
	 
	pci_set_drvdata(dev, NULL);
	
	if(is_372n)
	{
		printk(KERN_INFO "hpt: HPT372N detected, using 372N timing.\n");
		if(freq < 0x55)
			pll = F_LOW_PCI_33;
		else if(freq < 0x70)
			pll = F_LOW_PCI_40;
		else if(freq < 0x7F)
			pll = F_LOW_PCI_50;
		else
			pll = F_LOW_PCI_66;
			
		printk(KERN_INFO "FREQ: %d PLL: %d\n", freq, pll);
			
		/* We always use the pll not the PCI clock on 372N */
	}
	else
	{
		if(freq < 0x9C)
			pll = F_LOW_PCI_33;
		else if(freq < 0xb0)
			pll = F_LOW_PCI_40;
		else if(freq <0xc8)
			pll = F_LOW_PCI_50;
		else
			pll = F_LOW_PCI_66;
	
		if (pll == F_LOW_PCI_33) {
			if (hpt_minimum_revision(dev,8))
				pci_set_drvdata(dev, (void *) thirty_three_base_hpt374);
			else if (hpt_minimum_revision(dev,5))
				pci_set_drvdata(dev, (void *) thirty_three_base_hpt372);
			else if (hpt_minimum_revision(dev,4))
				pci_set_drvdata(dev, (void *) thirty_three_base_hpt370a);
			else
				pci_set_drvdata(dev, (void *) thirty_three_base_hpt370);
			printk("HPT37X: using 33MHz PCI clock\n");
		} else if (pll == F_LOW_PCI_40) {
			/* Unsupported */
		} else if (pll == F_LOW_PCI_50) {
			if (hpt_minimum_revision(dev,8))
				pci_set_drvdata(dev, NULL);
			else if (hpt_minimum_revision(dev,5))
				pci_set_drvdata(dev, (void *) fifty_base_hpt372);
			else if (hpt_minimum_revision(dev,4))
				pci_set_drvdata(dev, (void *) fifty_base_hpt370a);
			else
				pci_set_drvdata(dev, (void *) fifty_base_hpt370a);
			printk("HPT37X: using 50MHz PCI clock\n");
		} else {
			if (hpt_minimum_revision(dev,8))
			{
				printk(KERN_ERR "HPT37x: 66MHz timings are not supported.\n");
			}
			else if (hpt_minimum_revision(dev,5))
				pci_set_drvdata(dev, (void *) sixty_six_base_hpt372);
			else if (hpt_minimum_revision(dev,4))
				pci_set_drvdata(dev, (void *) sixty_six_base_hpt370a);
			else
				pci_set_drvdata(dev, (void *) sixty_six_base_hpt370);
			printk("HPT37X: using 66MHz PCI clock\n");
		}
	}
			
	/*
	 * only try the pll if we don't have a table for the clock
	 * speed that we're running at. NOTE: the internal PLL will
	 * result in slow reads when using a 33MHz PCI clock. we also
	 * don't like to use the PLL because it will cause glitches
	 * on PRST/SRST when the HPT state engine gets reset.
	 *
	 * ToDo: Use 66MHz PLL when ATA133 devices are present on a
	 * 372 device so we can get ATA133 support
	 */
	if (pci_get_drvdata(dev)) 
		goto init_hpt37X_done;
	
	if (hpt_minimum_revision(dev,8))
	{
		printk(KERN_ERR "HPT374: Only 33MHz PCI timings are supported.\n");
		return -EOPNOTSUPP;
	}
	/*
	 * adjust PLL based upon PCI clock, enable it, and wait for
	 * stabilization.
	 */
	adjust = 0;
	freq = (pll < F_LOW_PCI_50) ? 2 : 4;
	while (adjust++ < 6) {
		pci_write_config_dword(dev, 0x5c, (freq + pll) << 16 |
				       pll | 0x100);

		/* wait for clock stabilization */
		for (i = 0; i < 0x50000; i++) {
			pci_read_config_byte(dev, 0x5b, &reg5bh);
			if (reg5bh & 0x80) {
				/* spin looking for the clock to destabilize */
				for (i = 0; i < 0x1000; ++i) {
					pci_read_config_byte(dev, 0x5b, 
							     &reg5bh);
					if ((reg5bh & 0x80) == 0)
						goto pll_recal;
				}
				pci_read_config_dword(dev, 0x5c, &pll);
				pci_write_config_dword(dev, 0x5c, 
						       pll & ~0x100);
				pci_write_config_byte(dev, 0x5b, 0x21);
				if (hpt_minimum_revision(dev,5))
					pci_set_drvdata(dev, (void *) fifty_base_hpt372);
				else if (hpt_minimum_revision(dev,4))
					pci_set_drvdata(dev, (void *) fifty_base_hpt370a);
				else
					pci_set_drvdata(dev, (void *) fifty_base_hpt370a);
				printk("HPT37X: using 50MHz internal PLL\n");
				goto init_hpt37X_done;
			}
		}
pll_recal:
		if (adjust & 1)
			pll -= (adjust >> 1);
		else
			pll += (adjust >> 1);
	} 

init_hpt37X_done:
	/* reset state engine */
	pci_write_config_byte(dev, 0x50, 0x37); 
	pci_write_config_byte(dev, 0x54, 0x37); 
	udelay(100);
	return 0;
}

static int __init init_hpt366 (struct pci_dev *dev)
{
	u32 reg1	= 0;
	u8 drive_fast	= 0;

	/*
	 * Disable the "fast interrupt" prediction.
	 */
	pci_read_config_byte(dev, 0x51, &drive_fast);
	if (drive_fast & 0x80)
		pci_write_config_byte(dev, 0x51, drive_fast & ~0x80);
	pci_read_config_dword(dev, 0x40, &reg1);
									
	/* detect bus speed by looking at control reg timing: */
	switch((reg1 >> 8) & 7) {
		case 5:
			pci_set_drvdata(dev, (void *) forty_base_hpt366);
			break;
		case 9:
			pci_set_drvdata(dev, (void *) twenty_five_base_hpt366);
			break;
		case 7:
		default:
			pci_set_drvdata(dev, (void *) thirty_three_base_hpt366);
			break;
	}

	if (!pci_get_drvdata(dev))
	{
		printk(KERN_ERR "hpt366: unknown bus timing.\n");
		pci_set_drvdata(dev, NULL);
	}
	return 0;
}

static unsigned int __init init_chipset_hpt366 (struct pci_dev *dev, const char *name)
{
	int ret = 0;
	u8 test = 0;

	if (dev->resource[PCI_ROM_RESOURCE].start)
		pci_write_config_byte(dev, PCI_ROM_ADDRESS,
			dev->resource[PCI_ROM_RESOURCE].start | PCI_ROM_ADDRESS_ENABLE);

	pci_read_config_byte(dev, PCI_CACHE_LINE_SIZE, &test);
	if (test != (L1_CACHE_BYTES / 4))
		pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE,
			(L1_CACHE_BYTES / 4));

	pci_read_config_byte(dev, PCI_LATENCY_TIMER, &test);
	if (test != 0x78)
		pci_write_config_byte(dev, PCI_LATENCY_TIMER, 0x78);

	pci_read_config_byte(dev, PCI_MIN_GNT, &test);
	if (test != 0x08)
		pci_write_config_byte(dev, PCI_MIN_GNT, 0x08);

	pci_read_config_byte(dev, PCI_MAX_LAT, &test);
	if (test != 0x08)
		pci_write_config_byte(dev, PCI_MAX_LAT, 0x08);

	if (hpt_minimum_revision(dev, 3)) {
		ret = init_hpt37x(dev);
	} else {
		ret =init_hpt366(dev);
	}
	if (ret)
		return ret;
	
#if defined(DISPLAY_HPT366_TIMINGS) && defined(CONFIG_PROC_FS)
	hpt_devs[n_hpt_devs++] = dev;

	if (!hpt366_proc) {
		hpt366_proc = 1;
		ide_pci_register_host_proc(&hpt366_procs[0]);
	}
#endif /* DISPLAY_HPT366_TIMINGS && CONFIG_PROC_FS */

	return dev->irq;
}

static void __init init_hwif_hpt366 (ide_hwif_t *hwif)
{
	struct pci_dev *dev		= hwif->pci_dev;
	u8 ata66 = 0, regmask		= (hwif->channel) ? 0x01 : 0x02;
	u8 did, rid;
	unsigned long dmabase		= hwif->dma_base;
	int is_372n = 0;
	
	if(dmabase)
	{
		did = inb(dmabase + 0x22);
		rid = inb(dmabase + 0x28);
	
		if((did == 4 && rid == 6) || (did == 5 && rid > 1))
			is_372n = 1;
	}
		
	if(is_372n)
		printk(KERN_ERR "HPT372N support is EXPERIMENTAL ONLY.\n");
		
	hwif->tuneproc			= &hpt3xx_tune_drive;
	hwif->speedproc			= &hpt3xx_tune_chipset;
	hwif->quirkproc			= &hpt3xx_quirkproc;
	hwif->intrproc			= &hpt3xx_intrproc;
	hwif->maskproc			= &hpt3xx_maskproc;
	
	if(is_372n)
		hwif->rw_disk = &hpt372n_rw_disk;

	/*
	 * The HPT37x uses the CBLID pins as outputs for MA15/MA16
	 * address lines to access an external eeprom.  To read valid
	 * cable detect state the pins must be enabled as inputs.
	 */
	if (hpt_minimum_revision(dev, 8) && PCI_FUNC(dev->devfn) & 1) {
		/*
		 * HPT374 PCI function 1
		 * - set bit 15 of reg 0x52 to enable TCBLID as input
		 * - set bit 15 of reg 0x56 to enable FCBLID as input
		 */
		u16 mcr3, mcr6;
		pci_read_config_word(dev, 0x52, &mcr3);
		pci_read_config_word(dev, 0x56, &mcr6);
		pci_write_config_word(dev, 0x52, mcr3 | 0x8000);
		pci_write_config_word(dev, 0x56, mcr6 | 0x8000);
		/* now read cable id register */
		pci_read_config_byte(dev, 0x5a, &ata66);
		pci_write_config_word(dev, 0x52, mcr3);
		pci_write_config_word(dev, 0x56, mcr6);
	} else if (hpt_minimum_revision(dev, 3)) {
		/*
		 * HPT370/372 and 374 pcifn 0
		 * - clear bit 0 of 0x5b to enable P/SCBLID as inputs
		 */
		u8 scr2;
		pci_read_config_byte(dev, 0x5b, &scr2);
		pci_write_config_byte(dev, 0x5b, scr2 & ~1);
		/* now read cable id register */
		pci_read_config_byte(dev, 0x5a, &ata66);
		pci_write_config_byte(dev, 0x5b, scr2);
	} else {
		pci_read_config_byte(dev, 0x5a, &ata66);
	}

#ifdef DEBUG
	printk("HPT366: reg5ah=0x%02x ATA-%s Cable Port%d\n",
		ata66, (ata66 & regmask) ? "33" : "66",
		PCI_FUNC(hwif->pci_dev->devfn));
#endif /* DEBUG */

#ifdef HPT_SERIALIZE_IO
	/* serialize access to this device */
	if (hwif->mate)
		hwif->serialized = hwif->mate->serialized = 1;
#endif

	if (hpt_minimum_revision(dev,3)) {
		u8 reg5ah = 0;
			pci_write_config_byte(dev, 0x5a, reg5ah & ~0x10);
		/*
		 * set up ioctl for power status.
		 * note: power affects both
		 * drives on each channel
		 */
		hwif->resetproc	= &hpt3xx_reset;
		hwif->busproc	= &hpt370_busproc;
//		hwif->drives[0].autotune = hwif->drives[1].autotune = 1;
	} else if (hpt_minimum_revision(dev,2)) {
		hwif->resetproc	= &hpt3xx_reset;
		hwif->busproc	= &hpt3xx_tristate;
	} else {
		hwif->resetproc = &hpt3xx_reset;
		hwif->busproc   = &hpt3xx_tristate;
	}

	if (!hwif->dma_base) {
		hwif->drives[0].autotune = 1;
		hwif->drives[1].autotune = 1;
		return;
	}

	hwif->ultra_mask = 0x7f;
	hwif->mwdma_mask = 0x07;

	if (!(hwif->udma_four))
		hwif->udma_four = ((ata66 & regmask) ? 0 : 1);
	hwif->ide_dma_check = &hpt366_config_drive_xfer_rate;

	if (hpt_minimum_revision(dev,8)) {
		hwif->ide_dma_test_irq = &hpt374_ide_dma_test_irq;
		hwif->ide_dma_end = &hpt374_ide_dma_end;
	} else if (hpt_minimum_revision(dev,5)) {
		hwif->ide_dma_test_irq = &hpt374_ide_dma_test_irq;
		hwif->ide_dma_end = &hpt374_ide_dma_end;
	} else if (hpt_minimum_revision(dev,3)) {
		hwif->ide_dma_begin = &hpt370_ide_dma_begin;
		hwif->ide_dma_end = &hpt370_ide_dma_end;
		hwif->ide_dma_timeout = &hpt370_ide_dma_timeout;
		hwif->ide_dma_lostirq = &hpt370_ide_dma_lostirq;
	} else if (hpt_minimum_revision(dev,2))
		hwif->ide_dma_lostirq = &hpt366_ide_dma_lostirq;
	else
		hwif->ide_dma_lostirq = &hpt366_ide_dma_lostirq;

	if (!noautodma)
		hwif->autodma = 1;
	hwif->drives[0].autodma = hwif->autodma;
	hwif->drives[1].autodma = hwif->autodma;
}

static void __init init_dma_hpt366 (ide_hwif_t *hwif, unsigned long dmabase)
{
	u8 masterdma	= 0, slavedma = 0;
	u8 dma_new	= 0, dma_old = 0;
	u8 primary	= hwif->channel ? 0x4b : 0x43;
	u8 secondary	= hwif->channel ? 0x4f : 0x47;
	unsigned long flags;

	if (!dmabase)
		return;
		
	if(pci_get_drvdata(hwif->pci_dev) == NULL)
	{
		printk(KERN_WARNING "hpt: no known IDE timings, disabling DMA.\n");
		return;
	}

	dma_old = hwif->INB(dmabase+2);

	local_irq_save(flags);

	dma_new = dma_old;
	pci_read_config_byte(hwif->pci_dev, primary, &masterdma);
	pci_read_config_byte(hwif->pci_dev, secondary, &slavedma);

	if (masterdma & 0x30)	dma_new |= 0x20;
	if (slavedma & 0x30)	dma_new |= 0x40;
	if (dma_new != dma_old)
		hwif->OUTB(dma_new, dmabase+2);

	local_irq_restore(flags);

	ide_setup_dma(hwif, dmabase, 8);
}

extern void ide_setup_pci_device(struct pci_dev *, ide_pci_device_t *);
extern void ide_setup_pci_devices(struct pci_dev *, struct pci_dev *, ide_pci_device_t *);

static void __init init_setup_hpt374 (struct pci_dev *dev, ide_pci_device_t *d)
{
	struct pci_dev *findev = NULL;

	if (PCI_FUNC(dev->devfn) & 1)
		return;

	pci_for_each_dev(findev) {
		if ((findev->vendor == dev->vendor) &&
		    (findev->device == dev->device) &&
		    ((findev->devfn - dev->devfn) == 1) &&
		    (PCI_FUNC(findev->devfn) & 1)) {
			u8 irq = 0, irq2 = 0;
			pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq);
			pci_read_config_byte(findev, PCI_INTERRUPT_LINE, &irq2);
			if (irq != irq2) {
				pci_write_config_byte(findev,
						PCI_INTERRUPT_LINE, irq);
				findev->irq = dev->irq;
				printk("%s: pci-config space interrupt "
					"fixed.\n", d->name);
			}
			ide_setup_pci_devices(dev, findev, d);
			return;
		}
	}
	ide_setup_pci_device(dev, d);
}

static void __init init_setup_hpt37x (struct pci_dev *dev, ide_pci_device_t *d)
{
	ide_setup_pci_device(dev, d);
}

static void __init init_setup_hpt366 (struct pci_dev *dev, ide_pci_device_t *d)
{
	struct pci_dev *findev = NULL;
	u8 pin1 = 0, pin2 = 0;
	unsigned int class_rev;
	static char *chipset_names[] = {"HPT366", "HPT366",  "HPT368",
				 "HPT370", "HPT370A", "HPT372"};

	if (PCI_FUNC(dev->devfn) & 1)
		return;

	pci_read_config_dword(dev, PCI_CLASS_REVISION, &class_rev);
	class_rev &= 0xff;

	/* New ident 372N reports revision 1. We could do the 
	   io port based type identification instead perhaps (DID, RID) */
	   
	if(d->device == PCI_DEVICE_ID_TTI_HPT372N)
		class_rev = 5;
		
	if(class_rev < 6)
		d->name = chipset_names[class_rev];

	switch(class_rev) {
		case 5:
		case 4:
		case 3: ide_setup_pci_device(dev, d);
			return;
		default:	break;
	}

	d->channels = 1;

	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin1);
	pci_for_each_dev(findev) {
		if ((findev->vendor == dev->vendor) &&
		    (findev->device == dev->device) &&
		    ((findev->devfn - dev->devfn) == 1) &&
		    (PCI_FUNC(findev->devfn) & 1)) {
			pci_read_config_byte(findev, PCI_INTERRUPT_PIN, &pin2);
			if ((pin1 != pin2) && (dev->irq == findev->irq)) {
				d->bootable = ON_BOARD;
				printk("%s: onboard version of chipset, "
					"pin1=%d pin2=%d\n", d->name,
					pin1, pin2);
			}
			ide_setup_pci_devices(dev, findev, d);
			return;
		}
	}
	ide_setup_pci_device(dev, d);
}


/**
 *	hpt366_init_one	-	called when an HPT366 is found
 *	@dev: the hpt366 device
 *	@id: the matching pci id
 *
 *	Called when the PCI registration layer (or the IDE initialization)
 *	finds a device matching our IDE device tables.
 */
 
static int __devinit hpt366_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	ide_pci_device_t *d = &hpt366_chipsets[id->driver_data];

	if (dev->device != d->device)
		BUG();
	d->init_setup(dev, d);
	MOD_INC_USE_COUNT;
	return 0;
}

static struct pci_device_id hpt366_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_TTI, PCI_DEVICE_ID_TTI_HPT366, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ PCI_VENDOR_ID_TTI, PCI_DEVICE_ID_TTI_HPT372, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 1},
	{ PCI_VENDOR_ID_TTI, PCI_DEVICE_ID_TTI_HPT302, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 2},
	{ PCI_VENDOR_ID_TTI, PCI_DEVICE_ID_TTI_HPT371, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 3},
	{ PCI_VENDOR_ID_TTI, PCI_DEVICE_ID_TTI_HPT374, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 4},
	{ PCI_VENDOR_ID_TTI, PCI_DEVICE_ID_TTI_HPT372N, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 5},
	{ 0, },
};

static struct pci_driver driver = {
	.name		= "HPT366 IDE",
	.id_table	= hpt366_pci_tbl,
	.probe		= hpt366_init_one,
};

static int hpt366_ide_init(void)
{
	return ide_pci_register_driver(&driver);
}

static void hpt366_ide_exit(void)
{
	ide_pci_unregister_driver(&driver);
}

module_init(hpt366_ide_init);
module_exit(hpt366_ide_exit);

MODULE_AUTHOR("Andre Hedrick");
MODULE_DESCRIPTION("PCI driver module for Highpoint HPT366 IDE");
MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;
