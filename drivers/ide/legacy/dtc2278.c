/*
 *  linux/drivers/ide/legacy/dtc2278.c		Version 0.02	Feb 10, 1996
 *
 *  Copyright (C) 1996  Linus Torvalds & author (see below)
 */

#undef REALLY_SLOW_IO           /* most systems can safely undef this */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/ide.h>
#include <linux/init.h>

#include <asm/io.h>

#ifdef CONFIG_BLK_DEV_DTC2278_MODULE
# define _IDE_C
# include "ide_modes.h"
# undef _IDE_C
#else
# include "ide_modes.h"
#endif /* CONFIG_BLK_DEV_DTC2278_MODULE */

/*
 * Changing this #undef to #define may solve start up problems in some systems.
 */
#undef ALWAYS_SET_DTC2278_PIO_MODE

/*
 * From: andy@cercle.cts.com (Dyan Wile)
 *
 * Below is a patch for DTC-2278 - alike software-programmable controllers
 * The code enables the secondary IDE controller and the PIO4 (3?) timings on
 * the primary (EIDE). You may probably have to enable the 32-bit support to
 * get the full speed. You better get the disk interrupts disabled ( hdparm -u0
 * /dev/hd.. ) for the drives connected to the EIDE interface. (I get my
 * filesystem  corrupted with -u1, but under heavy disk load only :-)
 *
 * This card is now forced to use the "serialize" feature,
 * and irq-unmasking is disallowed.  If io_32bit is enabled,
 * it must be done for BOTH drives on each interface.
 *
 * This code was written for the DTC2278E, but might work with any of these:
 *
 * DTC2278S has only a single IDE interface.
 * DTC2278D has two IDE interfaces and is otherwise identical to the S version.
 * DTC2278E also has serial ports and a printer port
 * DTC2278EB: has onboard BIOS, and "works like a charm" -- Kent Bradford <kent@theory.caltech.edu>
 *
 * There may be a fourth controller type. The S and D versions use the
 * Winbond chip, and I think the E version does also.
 *
 */

static void sub22 (char b, char c)
{
	int i;

	for(i = 0; i < 3; ++i) {
		inb(0x3f6);
		outb_p(b,0xb0);
		inb(0x3f6);
		outb_p(c,0xb4);
		inb(0x3f6);
		if(inb(0xb4) == c) {
			outb_p(7,0xb0);
			inb(0x3f6);
			return;	/* success */
		}
	}
}

static void tune_dtc2278 (ide_drive_t *drive, u8 pio)
{
	unsigned long flags;

	pio = ide_get_best_pio_mode(drive, pio, 4, NULL);

	if (pio >= 3) {
		spin_lock_irqsave(&io_request_lock, flags);
		/*
		 * This enables PIO mode4 (3?) on the first interface
		 */
		sub22(1,0xc3);
		sub22(0,0xa0);
		spin_unlock_irqrestore(&io_request_lock, flags);
	} else {
		/* we don't know how to set it back again.. */
	}

	/*
	 * 32bit I/O has to be enabled for *both* drives at the same time.
	 */
	drive->io_32bit = 1;
	HWIF(drive)->drives[!drive->select.b.unit].io_32bit = 1;
}

void __init probe_dtc2278 (void)
{
	unsigned long flags;

	local_irq_save(flags);
	/*
	 * This enables the second interface
	 */
	outb_p(4,0xb0);
	inb(0x3f6);
	outb_p(0x20,0xb4);
	inb(0x3f6);
#ifdef ALWAYS_SET_DTC2278_PIO_MODE
	/*
	 * This enables PIO mode4 (3?) on the first interface
	 * and may solve start-up problems for some people.
	 */
	sub22(1,0xc3);
	sub22(0,0xa0);
#endif
	local_irq_restore(flags);

	ide_hwifs[0].serialized = 1;
	ide_hwifs[1].serialized = 1;
	ide_hwifs[0].chipset = ide_dtc2278;
	ide_hwifs[1].chipset = ide_dtc2278;
	ide_hwifs[0].tuneproc = &tune_dtc2278;
	ide_hwifs[0].drives[0].no_unmask = 1;
	ide_hwifs[0].drives[1].no_unmask = 1;
	ide_hwifs[1].drives[0].no_unmask = 1;
	ide_hwifs[1].drives[1].no_unmask = 1;
	ide_hwifs[0].mate = &ide_hwifs[1];
	ide_hwifs[1].mate = &ide_hwifs[0];
	ide_hwifs[1].channel = 1;

#ifndef HWIF_PROBE_CLASSIC_METHOD
	probe_hwif_init(&ide_hwifs[0]);
	probe_hwif_init(&ide_hwifs[1]);
#endif /* HWIF_PROBE_CLASSIC_METHOD */

}

void __init dtc2278_release (void)
{
	if (ide_hwifs[0].chipset != ide_dtc2278 &&
	    ide_hwifs[1].chipset != ide_dtc2278)
		return;

	ide_hwifs[0].serialized = 0;
	ide_hwifs[1].serialized = 0;
	ide_hwifs[0].chipset = ide_unknown;
	ide_hwifs[1].chipset = ide_unknown;
	ide_hwifs[0].tuneproc = NULL;
	ide_hwifs[0].drives[0].no_unmask = 0;
	ide_hwifs[0].drives[1].no_unmask = 0;
	ide_hwifs[1].drives[0].no_unmask = 0;
	ide_hwifs[1].drives[1].no_unmask = 0;
	ide_hwifs[0].mate = NULL;
	ide_hwifs[1].mate = NULL;
}

#ifndef MODULE
/*
 * init_dtc2278:
 *
 * called by ide.c when parsing command line
 */

void __init init_dtc2278 (void)
{
	probe_dtc2278();
}

#else

MODULE_AUTHOR("See Local File");
MODULE_DESCRIPTION("support of DTC-2278 VLB IDE chipsets");
MODULE_LICENSE("GPL");

int __init dtc2278_mod_init(void)
{
	probe_dtc2278();
	if (ide_hwifs[0].chipset != ide_dtc2278 &&
	    ide_hwifs[1].chipset != ide_dtc2278) {
		dtc2278_release();
		return -ENODEV;
	}
	return 0;
}
module_init(dtc2278_mod_init);

void __init dtc2278_mod_exit(void)
{
	dtc2278_release();
}
module_exit(dtc2278_mod_exit);
#endif

