/*
 * linux/drivers/ide/pci/hpt34x.c		Version 0.40	Sept 10, 2002
 *
 * Copyright (C) 1998-2000	Andre Hedrick <andre@linux-ide.org>
 * May be copied or modified under the terms of the GNU General Public License
 *
 *
 * 00:12.0 Unknown mass storage controller:
 * Triones Technologies, Inc.
 * Unknown device 0003 (rev 01)
 *
 * hde: UDMA 2 (0x0000 0x0002) (0x0000 0x0010)
 * hdf: UDMA 2 (0x0002 0x0012) (0x0010 0x0030)
 * hde: DMA 2  (0x0000 0x0002) (0x0000 0x0010)
 * hdf: DMA 2  (0x0002 0x0012) (0x0010 0x0030)
 * hdg: DMA 1  (0x0012 0x0052) (0x0030 0x0070)
 * hdh: DMA 1  (0x0052 0x0252) (0x0070 0x00f0)
 *
 * ide-pci.c reference
 *
 * Since there are two cards that report almost identically,
 * the only discernable difference is the values reported in pcicmd.
 * Booting-BIOS card or HPT363 :: pcicmd == 0x07
 * Non-bootable card or HPT343 :: pcicmd == 0x05
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
#include "hpt34x.h"

#if defined(DISPLAY_HPT34X_TIMINGS) && defined(CONFIG_PROC_FS)
#include <linux/stat.h>
#include <linux/proc_fs.h>

static u8 hpt34x_proc = 0;

#define HPT34X_MAX_DEVS		8
static struct pci_dev *hpt34x_devs[HPT34X_MAX_DEVS];
static int n_hpt34x_devs;

static int hpt34x_get_info (char *buffer, char **addr, off_t offset, int count)
{
	char *p = buffer;
	int i, len;

	p += sprintf(p, "\n                             "
			"HPT34X Chipset.\n");
	for (i = 0; i < n_hpt34x_devs; i++) {
		struct pci_dev *dev = hpt34x_devs[i];
		unsigned long bibma = pci_resource_start(dev, 4);
		u8  c0 = 0, c1 = 0;

		/*
		 * at that point bibma+0x2 et bibma+0xa are byte registers
		 * to investigate:
		 */
		c0 = inb_p((u16)bibma + 0x02);
		c1 = inb_p((u16)bibma + 0x0a);
		p += sprintf(p, "\nController: %d\n", i);
		p += sprintf(p, "--------------- Primary Channel "
				"---------------- Secondary Channel "
				"-------------\n");
		p += sprintf(p, "                %sabled "
				"                        %sabled\n",
				(c0&0x80) ? "dis" : " en",
				(c1&0x80) ? "dis" : " en");
		p += sprintf(p, "--------------- drive0 --------- drive1 "
				"-------- drive0 ---------- drive1 ------\n");
		p += sprintf(p, "DMA enabled:    %s              %s"
				"             %s               %s\n",
				(c0&0x20) ? "yes" : "no ",
				(c0&0x40) ? "yes" : "no ",
				(c1&0x20) ? "yes" : "no ",
				(c1&0x40) ? "yes" : "no " );

		p += sprintf(p, "UDMA\n");
		p += sprintf(p, "DMA\n");
		p += sprintf(p, "PIO\n");
	}
	p += sprintf(p, "\n");

	/* p - buffer must be less than 4k! */
	len = (p - buffer) - offset;
	*addr = buffer + offset;
	
	return len > count ? count : len;
}
#endif  /* defined(DISPLAY_HPT34X_TIMINGS) && defined(CONFIG_PROC_FS) */

static u8 hpt34x_ratemask (ide_drive_t *drive)
{
	return 1;
}

static void hpt34x_clear_chipset (ide_drive_t *drive)
{
	struct pci_dev *dev	= HWIF(drive)->pci_dev;
	u32 reg1 = 0, tmp1 = 0, reg2 = 0, tmp2 = 0;

	pci_read_config_dword(dev, 0x44, &reg1);
	pci_read_config_dword(dev, 0x48, &reg2);
	tmp1 = ((0x00 << (3*drive->dn)) | (reg1 & ~(7 << (3*drive->dn))));
	tmp2 = (reg2 & ~(0x11 << drive->dn));
	pci_write_config_dword(dev, 0x44, tmp1);
	pci_write_config_dword(dev, 0x48, tmp2);
}

static int hpt34x_tune_chipset (ide_drive_t *drive, u8 xferspeed)
{
	struct pci_dev *dev	= HWIF(drive)->pci_dev;
	u8 speed	= ide_rate_filter(hpt34x_ratemask(drive), xferspeed);
	u32 reg1= 0, tmp1 = 0, reg2 = 0, tmp2 = 0;
	u8			hi_speed, lo_speed;

	SPLIT_BYTE(speed, hi_speed, lo_speed);

	if (hi_speed & 7) {
		hi_speed = (hi_speed & 4) ? 0x01 : 0x10;
	} else {
		lo_speed <<= 5;
		lo_speed >>= 5;
	}

	pci_read_config_dword(dev, 0x44, &reg1);
	pci_read_config_dword(dev, 0x48, &reg2);
	tmp1 = ((lo_speed << (3*drive->dn)) | (reg1 & ~(7 << (3*drive->dn))));
	tmp2 = ((hi_speed << drive->dn) | reg2);
	pci_write_config_dword(dev, 0x44, tmp1);
	pci_write_config_dword(dev, 0x48, tmp2);

#if HPT343_DEBUG_DRIVE_INFO
	printk("%s: %s drive%d (0x%04x 0x%04x) (0x%04x 0x%04x)" \
		" (0x%02x 0x%02x)\n",
		drive->name, ide_xfer_verbose(speed),
		drive->dn, reg1, tmp1, reg2, tmp2,
		hi_speed, lo_speed);
#endif /* HPT343_DEBUG_DRIVE_INFO */

	return(ide_config_drive_speed(drive, speed));
}

static void hpt34x_tune_drive (ide_drive_t *drive, u8 pio)
{
	pio = ide_get_best_pio_mode(drive, pio, 5, NULL);
	hpt34x_clear_chipset(drive);
	(void) hpt34x_tune_chipset(drive, (XFER_PIO_0 + pio));
}

/*
 * This allows the configuration of ide_pci chipset registers
 * for cards that learn about the drive's UDMA, DMA, PIO capabilities
 * after the drive is reported by the OS.  Initially for designed for
 * HPT343 UDMA chipset by HighPoint|Triones Technologies, Inc.
 */

static int config_chipset_for_dma (ide_drive_t *drive)
{
	u8 speed = ide_dma_speed(drive, hpt34x_ratemask(drive));

	if (!(speed))
		return 0;

	hpt34x_clear_chipset(drive);
	(void) hpt34x_tune_chipset(drive, speed);
	return ide_dma_enable(drive);
}

static int hpt34x_config_drive_xfer_rate (ide_drive_t *drive)
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
				if ((id->field_valid & 2) && dma)
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
			/* Consult the list of known "good" drives */
			if (!config_chipset_for_dma(drive))
				goto no_dma_set;
		} else {
			goto fast_ata_pio;
		}
	} else if ((id->capability & 8) || (id->field_valid & 2)) {
fast_ata_pio:
no_dma_set:
		hpt34x_tune_drive(drive, 255);
		return hwif->ide_dma_off_quietly(drive);
	}

#ifndef CONFIG_HPT34X_AUTODMA
	return hwif->ide_dma_off_quietly(drive);
#endif /* CONFIG_HPT34X_AUTODMA */
	return hwif->ide_dma_on(drive);
}

/*
 * If the BIOS does not set the IO base addaress to XX00, 343 will fail.
 */
#define	HPT34X_PCI_INIT_REG		0x80

static unsigned int __init init_chipset_hpt34x (struct pci_dev *dev, const char *name)
{
	int i = 0;
	unsigned long hpt34xIoBase = pci_resource_start(dev, 4);
	unsigned long hpt_addr[4] = { 0x20, 0x34, 0x28, 0x3c };
	unsigned long hpt_addr_len[4] = { 7, 3, 7, 3 };
	u16 cmd;
	unsigned long flags;

	local_irq_save(flags);

	pci_write_config_byte(dev, HPT34X_PCI_INIT_REG, 0x00);
	pci_read_config_word(dev, PCI_COMMAND, &cmd);

	if (cmd & PCI_COMMAND_MEMORY) {
		if (pci_resource_start(dev, PCI_ROM_RESOURCE)) {
			pci_write_config_byte(dev, PCI_ROM_ADDRESS,
				dev->resource[PCI_ROM_RESOURCE].start | PCI_ROM_ADDRESS_ENABLE);
			printk(KERN_INFO "HPT345: ROM enabled at 0x%08lx\n",
				dev->resource[PCI_ROM_RESOURCE].start);
		}
		pci_write_config_byte(dev, PCI_LATENCY_TIMER, 0xF0);
	} else {
		pci_write_config_byte(dev, PCI_LATENCY_TIMER, 0x20);
	}

	/*
	 * Since 20-23 can be assigned and are R/W, we correct them.
	 */
	pci_write_config_word(dev, PCI_COMMAND, cmd & ~PCI_COMMAND_IO);
	for(i=0; i<4; i++) {
		dev->resource[i].start = (hpt34xIoBase + hpt_addr[i]);
		dev->resource[i].end = dev->resource[i].start + hpt_addr_len[i];
		dev->resource[i].flags = IORESOURCE_IO;
		pci_write_config_dword(dev,
				(PCI_BASE_ADDRESS_0 + (i * 4)),
				dev->resource[i].start);
	}
	pci_write_config_word(dev, PCI_COMMAND, cmd);

	local_irq_restore(flags);

#if defined(DISPLAY_HPT34X_TIMINGS) && defined(CONFIG_PROC_FS)
	hpt34x_devs[n_hpt34x_devs++] = dev;

	if (!hpt34x_proc) {
		hpt34x_proc = 1;
		ide_pci_register_host_proc(&hpt34x_procs[0]);
	}
#endif /* DISPLAY_HPT34X_TIMINGS && CONFIG_PROC_FS */

	return dev->irq;
}

static void __init init_hwif_hpt34x (ide_hwif_t *hwif)
{
	u16 pcicmd = 0;

	hwif->autodma = 0;

	hwif->tuneproc = &hpt34x_tune_drive;
	hwif->speedproc = &hpt34x_tune_chipset;
	hwif->no_dsc = 1;
	hwif->drives[0].autotune = 1;
	hwif->drives[1].autotune = 1;

	pci_read_config_word(hwif->pci_dev, PCI_COMMAND, &pcicmd);

	if (!hwif->dma_base)
		return;

	hwif->ultra_mask = 0x07;
	hwif->mwdma_mask = 0x07;
	hwif->swdma_mask = 0x07;

	hwif->ide_dma_check = &hpt34x_config_drive_xfer_rate;
	if (!noautodma)
		hwif->autodma = (pcicmd & PCI_COMMAND_MEMORY) ? 1 : 0;
	hwif->drives[0].autodma = hwif->autodma;
	hwif->drives[1].autodma = hwif->autodma;
}

static void __init init_dma_hpt34x (ide_hwif_t *hwif, unsigned long dmabase)
{
	ide_setup_dma(hwif, dmabase, 8);
}

extern void ide_setup_pci_device(struct pci_dev *, ide_pci_device_t *);

static int __devinit hpt34x_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	ide_pci_device_t *d = &hpt34x_chipsets[id->driver_data];
	static char *chipset_names[] = {"HPT343", "HPT345"};
	u16 pcicmd = 0;

	pci_read_config_word(dev, PCI_COMMAND, &pcicmd);

	d->name = chipset_names[(pcicmd & PCI_COMMAND_MEMORY) ? 1 : 0];
	d->bootable = (pcicmd & PCI_COMMAND_MEMORY) ? OFF_BOARD : NEVER_BOARD;

	ide_setup_pci_device(dev, d);
	MOD_INC_USE_COUNT;
	return 0;
}

static struct pci_device_id hpt34x_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_TTI, PCI_DEVICE_ID_TTI_HPT343, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ 0, },
};

static struct pci_driver driver = {
	.name		= "HPT34x IDE",
	.id_table	= hpt34x_pci_tbl,
	.probe		= hpt34x_init_one,
};

static int hpt34x_ide_init(void)
{
	return ide_pci_register_driver(&driver);
}

static void hpt34x_ide_exit(void)
{
	ide_pci_unregister_driver(&driver);
}

module_init(hpt34x_ide_init);
module_exit(hpt34x_ide_exit);

MODULE_AUTHOR("Andre Hedrick");
MODULE_DESCRIPTION("PCI driver module for Highpoint 34x IDE");
MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;
