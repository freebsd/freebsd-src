/*
 * triflex.c
 * 
 * IDE Chipset driver for the Compaq TriFlex IDE controller.
 * 
 * Known to work with the Compaq Workstation 5x00 series.
 *
 * Copyright (C) 2002 Hewlett-Packard Development Group, L.P.
 * Author: Torben Mathiasen <torben.mathiasen@hp.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * Loosely based on the piix & svwks drivers.
 *
 * Documentation:
 *	Not publically available.
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
#include <linux/pci.h>
#include <linux/ide.h>
#include <linux/init.h>

#include "ide_modes.h"
#include "triflex.h"

static struct pci_dev *triflex_dev;

static int triflex_get_info(char *buf, char **addr, off_t offset, int count)
{
	char *p = buf;
	int len;

	struct pci_dev *dev	= triflex_dev;
	unsigned long bibma = pci_resource_start(dev, 4);
	u8  c0 = 0, c1 = 0;
	u32 pri_timing, sec_timing;

	p += sprintf(p, "\n                                Compaq Triflex Chipset\n");
	
	pci_read_config_dword(dev, 0x70, &pri_timing);
	pci_read_config_dword(dev, 0x74, &sec_timing);

	/*
	 * at that point bibma+0x2 et bibma+0xa are byte registers
	 * to investigate:
	 */
	c0 = inb((unsigned short)bibma + 0x02);
	c1 = inb((unsigned short)bibma + 0x0a);

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

	p += sprintf(p, "DMA\n");
	p += sprintf(p, "PIO\n");

	len = (p - buf) - offset;
	*addr = buf + offset;
	
	return len > count ? count : len;
}

static int triflex_tune_chipset(ide_drive_t *drive, u8 xferspeed)
{
	ide_hwif_t *hwif = HWIF(drive);
	struct pci_dev *dev = hwif->pci_dev;
	u8 channel_offset = hwif->channel ? 0x74 : 0x70;
	u16 timing = 0;
	u32 triflex_timings = 0;
	u8 unit = (drive->select.b.unit & 0x01);
	u8 speed = ide_rate_filter(0, xferspeed);
	
	pci_read_config_dword(dev, channel_offset, &triflex_timings);
	
	switch(speed) {
		case XFER_MW_DMA_2:
			timing = 0x0103; 
			break;
		case XFER_MW_DMA_1:
			timing = 0x0203;
			break;
		case XFER_MW_DMA_0:
			timing = 0x0808;
			break;
		case XFER_SW_DMA_2:
		case XFER_SW_DMA_1:
		case XFER_SW_DMA_0:
			timing = 0x0f0f;
			break;
		case XFER_PIO_4:
			timing = 0x0202;
			break;
		case XFER_PIO_3:
			timing = 0x0204;
			break;
		case XFER_PIO_2:
			timing = 0x0404;
			break;
		case XFER_PIO_1:
			timing = 0x0508;
			break;
		case XFER_PIO_0:
			timing = 0x0808;
			break;
		default:
			return -1;
	}

	triflex_timings &= ~(0xFFFF << (16 * unit));
	triflex_timings |= (timing << (16 * unit));
	
	pci_write_config_dword(dev, channel_offset, triflex_timings);
	
	return (ide_config_drive_speed(drive, speed));
}

static void triflex_tune_drive(ide_drive_t *drive, u8 pio)
{
	int use_pio = ide_get_best_pio_mode(drive, pio, 4, NULL);
	(void) triflex_tune_chipset(drive, (XFER_PIO_0 + use_pio));
}

static int triflex_config_drive_for_dma(ide_drive_t *drive)
{
	int speed = ide_dma_speed(drive, 0); /* No ultra speeds */

	if (!speed) { 
		u8 pspeed = ide_get_best_pio_mode(drive, 255, 4, NULL);
		speed = XFER_PIO_0 + pspeed;
	}
	
	(void) triflex_tune_chipset(drive, speed);
	 return ide_dma_enable(drive);
}

static int triflex_config_drive_xfer_rate(ide_drive_t *drive)
{
	ide_hwif_t *hwif	= HWIF(drive);
	struct hd_driveid *id	= drive->id;
	
	if ((id->capability & 1) && drive->autodma) {
		if (hwif->ide_dma_bad_drive(drive))
			goto tune_pio;
		if (id->field_valid & 2) {
			if ((id->dma_mword & hwif->mwdma_mask) ||
				(id->dma_1word & hwif->swdma_mask)) {
				if (!triflex_config_drive_for_dma(drive))
					goto tune_pio;
			}
		} else 
			goto tune_pio;
	} else {
tune_pio:
		hwif->tuneproc(drive, 255);
		return hwif->ide_dma_off_quietly(drive);
	}

	return hwif->ide_dma_on(drive);
}

static void __init init_hwif_triflex(ide_hwif_t *hwif)
{
	hwif->tuneproc = &triflex_tune_drive;
	hwif->speedproc = &triflex_tune_chipset;

	hwif->atapi_dma  = 1;
	hwif->mwdma_mask = 0x07;
	hwif->swdma_mask = 0x07;
	hwif->ide_dma_check = &triflex_config_drive_xfer_rate;
	
	if (!noautodma)
		hwif->autodma = 1;
	hwif->drives[0].autodma = hwif->autodma;
	hwif->drives[1].autodma = hwif->autodma;
}

static unsigned int __init init_chipset_triflex(struct pci_dev *dev, 
		const char *name) 
{
#ifdef CONFIG_PROC_FS
	ide_pci_register_host_proc(&triflex_proc);
#endif
	return 0;	
}

static int __devinit triflex_init_one(struct pci_dev *dev, 
		const struct pci_device_id *id)
{
	ide_pci_device_t *d = &triflex_devices[id->driver_data];
	if (dev->device != d->device)
		BUG();
	
	ide_setup_pci_device(dev, d);
	triflex_dev = dev;
	MOD_INC_USE_COUNT;
	
	return 0;
}

static struct pci_driver driver = {
	.name		= "TRIFLEX IDE",
	.id_table	= triflex_pci_tbl,
	.probe		= triflex_init_one,
};

static int triflex_ide_init(void)
{
	return ide_pci_register_driver(&driver);
}

static void triflex_ide_exit(void)
{
	ide_pci_unregister_driver(&driver);
}

module_init(triflex_ide_init);
module_exit(triflex_ide_exit);

MODULE_AUTHOR("Torben Mathiasen");
MODULE_DESCRIPTION("PCI driver module for Compaq Triflex IDE");
MODULE_LICENSE("GPL");


