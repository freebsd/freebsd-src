/*
 *  linux/drivers/ide/pci/slc90e66.c	Version 0.11	September 11, 2002
 *
 *  Copyright (C) 2000-2002 Andre Hedrick <andre@linux-ide.org>
 *
 * This a look-a-like variation of the ICH0 PIIX4 Ultra-66,
 * but this keeps the ISA-Bridge and slots alive.
 *
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/hdreg.h>
#include <linux/ide.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <asm/io.h>

#include "ide_modes.h"
#include "slc90e66.h"

#if defined(DISPLAY_SLC90E66_TIMINGS) && defined(CONFIG_PROC_FS)
#include <linux/stat.h>
#include <linux/proc_fs.h>

static u8 slc90e66_proc = 0;
static struct pci_dev *bmide_dev;

static int slc90e66_get_info (char *buffer, char **addr, off_t offset, int count)
{
	char *p = buffer;
	int len;
	unsigned long bibma = pci_resource_start(bmide_dev, 4);
	u16 reg40 = 0, psitre = 0, reg42 = 0, ssitre = 0;
	u8  c0 = 0, c1 = 0;
	u8  reg44 = 0, reg47 = 0, reg48 = 0, reg4a = 0, reg4b = 0;

	pci_read_config_word(bmide_dev, 0x40, &reg40);
	pci_read_config_word(bmide_dev, 0x42, &reg42);
	pci_read_config_byte(bmide_dev, 0x44, &reg44);
	pci_read_config_byte(bmide_dev, 0x47, &reg47);
	pci_read_config_byte(bmide_dev, 0x48, &reg48);
	pci_read_config_byte(bmide_dev, 0x4a, &reg4a);
	pci_read_config_byte(bmide_dev, 0x4b, &reg4b);

	psitre = (reg40 & 0x4000) ? 1 : 0;
	ssitre = (reg42 & 0x4000) ? 1 : 0;

        /*
         * at that point bibma+0x2 et bibma+0xa are byte registers
         * to investigate:
         */
	c0 = inb_p(bibma + 0x02);
	c1 = inb_p(bibma + 0x0a);

	p += sprintf(p, "                                SLC90E66 Chipset.\n");
	p += sprintf(p, "--------------- Primary Channel "
			"---------------- Secondary Channel "
			"-------------\n");
	p += sprintf(p, "                %sabled "
			"                        %sabled\n",
			(c0&0x80) ? "dis" : " en",
			(c1&0x80) ? "dis" : " en");
	p += sprintf(p, "--------------- drive0 --------- drive1 "
			"-------- drive0 ---------- drive1 ------\n");
	p += sprintf(p, "DMA enabled:    %s              %s "
			"            %s               %s\n",
			(c0&0x20) ? "yes" : "no ",
			(c0&0x40) ? "yes" : "no ",
			(c1&0x20) ? "yes" : "no ",
			(c1&0x40) ? "yes" : "no " );
	p += sprintf(p, "UDMA enabled:   %s              %s "
			"            %s               %s\n",
			(reg48&0x01) ? "yes" : "no ",
			(reg48&0x02) ? "yes" : "no ",
			(reg48&0x04) ? "yes" : "no ",
			(reg48&0x08) ? "yes" : "no " );
	p += sprintf(p, "UDMA enabled:   %s                %s "
			"              %s                 %s\n",
			((reg4a&0x04)==0x04) ? "4" :
			((reg4a&0x03)==0x03) ? "3" :
			(reg4a&0x02) ? "2" :
			(reg4a&0x01) ? "1" :
			(reg4a&0x00) ? "0" : "X",
			((reg4a&0x40)==0x40) ? "4" :
			((reg4a&0x30)==0x30) ? "3" :
			(reg4a&0x20) ? "2" :
			(reg4a&0x10) ? "1" :
			(reg4a&0x00) ? "0" : "X",
			((reg4b&0x04)==0x04) ? "4" :
			((reg4b&0x03)==0x03) ? "3" :
			(reg4b&0x02) ? "2" :
			(reg4b&0x01) ? "1" :
			(reg4b&0x00) ? "0" : "X",
			((reg4b&0x40)==0x40) ? "4" :
			((reg4b&0x30)==0x30) ? "3" :
			(reg4b&0x20) ? "2" :
			(reg4b&0x10) ? "1" :
			(reg4b&0x00) ? "0" : "X");

	p += sprintf(p, "UDMA\n");
	p += sprintf(p, "DMA\n");
	p += sprintf(p, "PIO\n");

/*
 *	FIXME.... Add configuration junk data....blah blah......
 */

	/* p - buffer must be less than 4k! */
	len = (p - buffer) - offset;
	*addr = buffer + offset;
	
	return len > count ? count : len;
}
#endif  /* defined(DISPLAY_SLC90E66_TIMINGS) && defined(CONFIG_PROC_FS) */

static u8 slc90e66_ratemask (ide_drive_t *drive)
{
	u8 mode	= 2;

	if (!eighty_ninty_three(drive))
		mode = min(mode, (u8)1);
	return mode;
}

static u8 slc90e66_dma_2_pio (u8 xfer_rate) {
	switch(xfer_rate) {
		case XFER_UDMA_4:
		case XFER_UDMA_3:
		case XFER_UDMA_2:
		case XFER_UDMA_1:
		case XFER_UDMA_0:
		case XFER_MW_DMA_2:
		case XFER_PIO_4:
			return 4;
		case XFER_MW_DMA_1:
		case XFER_PIO_3:
			return 3;
		case XFER_SW_DMA_2:
		case XFER_PIO_2:
			return 2;
		case XFER_MW_DMA_0:
		case XFER_SW_DMA_1:
		case XFER_SW_DMA_0:
		case XFER_PIO_1:
		case XFER_PIO_0:
		case XFER_PIO_SLOW:
		default:
			return 0;
	}
}

/*
 *  Based on settings done by AMI BIOS
 *  (might be useful if drive is not registered in CMOS for any reason).
 */
static void slc90e66_tune_drive (ide_drive_t *drive, u8 pio)
{
	ide_hwif_t *hwif	= HWIF(drive);
	struct pci_dev *dev	= hwif->pci_dev;
	int is_slave		= (&hwif->drives[1] == drive);
	int master_port		= hwif->channel ? 0x42 : 0x40;
	int slave_port		= 0x44;
	unsigned long flags;
	u16 master_data;
	u8 slave_data;
				 /* ISP  RTC */
	u8 timings[][2]	= { { 0, 0 },
				    { 0, 0 },
				    { 1, 0 },
				    { 2, 1 },
				    { 2, 3 }, };

	pio = ide_get_best_pio_mode(drive, pio, 5, NULL);
	spin_lock_irqsave(&ide_lock, flags);
	pci_read_config_word(dev, master_port, &master_data);
	if (is_slave) {
		master_data = master_data | 0x4000;
		if (pio > 1)
			/* enable PPE, IE and TIME */
			master_data = master_data | 0x0070;
		pci_read_config_byte(dev, slave_port, &slave_data);
		slave_data = slave_data & (hwif->channel ? 0x0f : 0xf0);
		slave_data = slave_data | (((timings[pio][0] << 2) | timings[pio][1]) << (hwif->channel ? 4 : 0));
	} else {
		master_data = master_data & 0xccf8;
		if (pio > 1)
			/* enable PPE, IE and TIME */
			master_data = master_data | 0x0007;
		master_data = master_data | (timings[pio][0] << 12) | (timings[pio][1] << 8);
	}
	pci_write_config_word(dev, master_port, master_data);
	if (is_slave)
		pci_write_config_byte(dev, slave_port, slave_data);
	spin_unlock_irqrestore(&ide_lock, flags);
}

static int slc90e66_tune_chipset (ide_drive_t *drive, u8 xferspeed)
{
	ide_hwif_t *hwif	= HWIF(drive);
	struct pci_dev *dev	= hwif->pci_dev;
	u8 maslave		= hwif->channel ? 0x42 : 0x40;
	u8 speed	= ide_rate_filter(slc90e66_ratemask(drive), xferspeed);
	int sitre = 0, a_speed	= 7 << (drive->dn * 4);
	int u_speed = 0, u_flag = 1 << drive->dn;
	u16			reg4042, reg44, reg48, reg4a;

	pci_read_config_word(dev, maslave, &reg4042);
	sitre = (reg4042 & 0x4000) ? 1 : 0;
	pci_read_config_word(dev, 0x44, &reg44);
	pci_read_config_word(dev, 0x48, &reg48);
	pci_read_config_word(dev, 0x4a, &reg4a);

	switch(speed) {
#ifdef CONFIG_BLK_DEV_IDEDMA
		case XFER_UDMA_4:	u_speed = 4 << (drive->dn * 4); break;
		case XFER_UDMA_3:	u_speed = 3 << (drive->dn * 4); break;
		case XFER_UDMA_2:	u_speed = 2 << (drive->dn * 4); break;
		case XFER_UDMA_1:	u_speed = 1 << (drive->dn * 4); break;
		case XFER_UDMA_0:	u_speed = 0 << (drive->dn * 4); break;
		case XFER_MW_DMA_2:
		case XFER_MW_DMA_1:
		case XFER_SW_DMA_2:	break;
#endif /* CONFIG_BLK_DEV_IDEDMA */
		case XFER_PIO_4:
		case XFER_PIO_3:
		case XFER_PIO_2:
		case XFER_PIO_0:        break;
		default:		return -1;
	}

	if (speed >= XFER_UDMA_0) {
		if (!(reg48 & u_flag))
			pci_write_config_word(dev, 0x48, reg48|u_flag);
		if ((reg4a & u_speed) != u_speed) {
			pci_write_config_word(dev, 0x4a, reg4a & ~a_speed);
			pci_read_config_word(dev, 0x4a, &reg4a);
			pci_write_config_word(dev, 0x4a, reg4a|u_speed);
		}
	} else {
		if (reg48 & u_flag)
			pci_write_config_word(dev, 0x48, reg48 & ~u_flag);
		if (reg4a & a_speed)
			pci_write_config_word(dev, 0x4a, reg4a & ~a_speed);
	}

	slc90e66_tune_drive(drive, slc90e66_dma_2_pio(speed));
	return (ide_config_drive_speed(drive, speed));
}

#ifdef CONFIG_BLK_DEV_IDEDMA
static int slc90e66_config_drive_for_dma (ide_drive_t *drive)
{
	u8 speed = ide_dma_speed(drive, slc90e66_ratemask(drive));

	if (!(speed)) {
		u8 tspeed = ide_get_best_pio_mode(drive, 255, 5, NULL);
		speed = slc90e66_dma_2_pio(XFER_PIO_0 + tspeed);
	}

	(void) slc90e66_tune_chipset(drive, speed);
	return ide_dma_enable(drive);
}

static int slc90e66_config_drive_xfer_rate (ide_drive_t *drive)
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
				int dma = slc90e66_config_drive_for_dma(drive);
				if ((id->field_valid & 2) && !dma)
					goto try_dma_modes;
			}
		} else if (id->field_valid & 2) {
try_dma_modes:
			if ((id->dma_mword & hwif->mwdma_mask) ||
			    (id->dma_1word & hwif->swdma_mask)) {
				/* Force if Capable regular DMA modes */
				if (!slc90e66_config_drive_for_dma(drive))
					goto no_dma_set;
			}
		} else if (hwif->ide_dma_good_drive(drive) &&
			   (id->eide_dma_time < 150)) {
			/* Consult the list of known "good" drives */
			if (!slc90e66_config_drive_for_dma(drive))
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
#endif /* CONFIG_BLK_DEV_IDEDMA */

static unsigned int __init init_chipset_slc90e66 (struct pci_dev *dev, const char *name)
{
#if defined(DISPLAY_SLC90E66_TIMINGS) && defined(CONFIG_PROC_FS)
	if (!slc90e66_proc) {
		slc90e66_proc = 1;
		bmide_dev = dev;
		ide_pci_register_host_proc(&slc90e66_procs[0]);
	}
#endif /* DISPLAY_SLC90E66_TIMINGS && CONFIG_PROC_FS */
	return 0;
}

static void __init init_hwif_slc90e66 (ide_hwif_t *hwif)
{
	u8 reg47 = 0;
	u8 mask = hwif->channel ? 0x01 : 0x02;  /* bit0:Primary */

	hwif->autodma = 0;

	if (!hwif->irq)
		hwif->irq = hwif->channel ? 15 : 14;

	hwif->speedproc = &slc90e66_tune_chipset;
	hwif->tuneproc = &slc90e66_tune_drive;

	pci_read_config_byte(hwif->pci_dev, 0x47, &reg47);

	if (!hwif->dma_base) {
		hwif->drives[0].autotune = 1;
		hwif->drives[1].autotune = 1;
		return;
	}

	hwif->atapi_dma = 1;
	hwif->ultra_mask = 0x1f;
	hwif->mwdma_mask = 0x07;
	hwif->swdma_mask = 0x07;

#ifdef CONFIG_BLK_DEV_IDEDMA 
	if (!(hwif->udma_four))
		/* bit[0(1)]: 0:80, 1:40 */
		hwif->udma_four = (reg47 & mask) ? 0 : 1;

	hwif->ide_dma_check = &slc90e66_config_drive_xfer_rate;
	if (!noautodma)
		hwif->autodma = 1;
	hwif->drives[0].autodma = hwif->autodma;
	hwif->drives[1].autodma = hwif->autodma;
#endif /* !CONFIG_BLK_DEV_IDEDMA */
}

static void __init init_dma_slc90e66 (ide_hwif_t *hwif, unsigned long dmabase)
{
	ide_setup_dma(hwif, dmabase, 8);
}

extern void ide_setup_pci_device(struct pci_dev *, ide_pci_device_t *);


static int __devinit slc90e66_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	ide_pci_device_t *d = &slc90e66_chipsets[id->driver_data];
	if (dev->device != d->device)
		BUG();
	ide_setup_pci_device(dev, d);
	MOD_INC_USE_COUNT;
	return 0;
}

static struct pci_device_id slc90e66_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_EFAR, PCI_DEVICE_ID_EFAR_SLC90E66_1, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ 0, },
};

static struct pci_driver driver = {
	.name		= "SLC90e66 IDE",
	.id_table	= slc90e66_pci_tbl,
	.probe		= slc90e66_init_one,
};

static int slc90e66_ide_init(void)
{
	return ide_pci_register_driver(&driver);
}

static void slc90e66_ide_exit(void)
{
	ide_pci_unregister_driver(&driver);
}

module_init(slc90e66_ide_init);
module_exit(slc90e66_ide_exit);

MODULE_AUTHOR("Andre Hedrick");
MODULE_DESCRIPTION("PCI driver module for SLC90E66 IDE");
MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;
