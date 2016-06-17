/*  -*- linux-c -*-
 *  linux/drivers/ide/legacy/pdc4030.c		Version 0.90  May 27, 1999
 *
 *  Copyright (C) 1995-2002  Linus Torvalds & authors (see below)
 */

/*
 *  Principal Author/Maintainer:  Peter Denison <promise@pnd-pc.demon.co.uk>
 *
 *  This file provides support for the second port and cache of Promise
 *  IDE interfaces, e.g. DC4030VL, DC4030VL-1 and DC4030VL-2.
 *
 *  Thanks are due to Mark Lord for advice and patiently answering stupid
 *  questions, and all those mugs^H^H^H^Hbrave souls who've tested this,
 *  especially Andre Hedrick.
 *
 *  Version 0.01	Initial version, #include'd in ide.c rather than
 *                      compiled separately.
 *                      Reads use Promise commands, writes as before. Drives
 *                      on second channel are read-only.
 *  Version 0.02        Writes working on second channel, reads on both
 *                      channels. Writes fail under high load. Suspect
 *			transfers of >127 sectors don't work.
 *  Version 0.03        Brought into line with ide.c version 5.27.
 *                      Other minor changes.
 *  Version 0.04        Updated for ide.c version 5.30
 *                      Changed initialization strategy
 *  Version 0.05	Kernel integration.  -ml
 *  Version 0.06	Ooops. Add hwgroup to direct call of ide_intr() -ml
 *  Version 0.07	Added support for DC4030 variants
 *			Secondary interface autodetection
 *  Version 0.08	Renamed to pdc4030.c
 *  Version 0.09	Obsolete - never released - did manual write request
 *			splitting before max_sectors[major][minor] available.
 *  Version 0.10	Updated for 2.1 series of kernels
 *  Version 0.11	Updated for 2.3 series of kernels
 *			Autodetection code added.
 *
 *  Version 0.90	Transition to BETA code. No lost/unexpected interrupts
 */

/*
 * Once you've compiled it in, you'll have to also enable the interface
 * setup routine from the kernel command line, as in 
 *
 *	'linux ide0=dc4030' or 'linux ide1=dc4030'
 *
 * It should now work as a second controller also ('ide1=dc4030') but only
 * if you DON'T have BIOS V4.44, which has a bug. If you have this version
 * and EPROM programming facilities, you need to fix 4 bytes:
 * 	2496:	81	81
 *	2497:	3E	3E
 *	2498:	22	98	*
 *	2499:	06	05	*
 *	249A:	F0	F0
 *	249B:	01	01
 *	...
 *	24A7:	81	81
 *	24A8:	3E	3E
 *	24A9:	22	98	*
 *	24AA:	06	05	*
 *	24AB:	70	70
 *	24AC:	01	01
 *
 * As of January 1999, Promise Technology Inc. have finally supplied me with
 * some technical information which has shed a glimmer of light on some of the
 * problems I was having, especially with writes. 
 *
 * There are still potential problems with the robustness and efficiency of
 * this driver because I still don't understand what the card is doing with
 * interrupts, however, it has been stable for a while with no reports of ill
 * effects.
 */

#define DEBUG_READ
#define DEBUG_WRITE
#define __PROMISE_4030

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
#include <asm/irq.h>

#include "pdc4030.h"

static ide_startstop_t promise_rw_disk (ide_drive_t *drive, struct request *rq, unsigned long block);

/*
 * promise_selectproc() is invoked by ide.c
 * in preparation for access to the specified drive.
 */
static void promise_selectproc (ide_drive_t *drive)
{
	unsigned int number;

	number = (HWIF(drive)->channel << 1) + drive->select.b.unit;
	HWIF(drive)->OUTB(number, IDE_FEATURE_REG);
}

/*
 * pdc4030_cmd handles the set of vendor specific commands that are initiated
 * by command F0. They all have the same success/failure notification -
 * 'P' (=0x50) on success, 'p' (=0x70) on failure.
 */
int pdc4030_cmd(ide_drive_t *drive, u8 cmd)
{
	u32 timeout;
	u8 status_val;

	promise_selectproc(drive);	/* redundant? */
	HWIF(drive)->OUTB(0xF3, IDE_SECTOR_REG);
	HWIF(drive)->OUTB(cmd, IDE_SELECT_REG);
	HWIF(drive)->OUTB(PROMISE_EXTENDED_COMMAND, IDE_COMMAND_REG);
	timeout = HZ * 10;
	timeout += jiffies;
	do {
		if(time_after(jiffies, timeout)) {
			return 2; /* device timed out */
		}
		/* Delays at least 10ms to give interface a chance */
		mdelay(10);
		status_val = HWIF(drive)->INB(IDE_SECTOR_REG);
	} while (status_val != 0x50 && status_val != 0x70);

	if(status_val == 0x50)
		return 0; /* device returned success */
	else
		return 1; /* device returned failure */
}

/*
 * pdc4030_identify sends a vendor-specific IDENTIFY command to the drive
 */
int pdc4030_identify(ide_drive_t *drive)
{
	return pdc4030_cmd(drive, PROMISE_IDENTIFY);
}

/*
 * setup_pdc4030()
 * Completes the setup of a Promise DC4030 controller card, once found.
 */
int __init setup_pdc4030(ide_hwif_t *hwif)
{
        ide_drive_t *drive;
	ide_hwif_t *hwif2;
	struct dc_ident ident;
	int i;
	ide_startstop_t startstop;
	
	if (!hwif) return 0;

	drive = &hwif->drives[0];
	hwif2 = &ide_hwifs[hwif->index+1];
	if (hwif->chipset == ide_pdc4030) /* we've already been found ! */
		return 1;

	if (hwif->INB(IDE_NSECTOR_REG) == 0xFF ||
	    hwif->INB(IDE_SECTOR_REG) == 0xFF) {
		return 0;
	}
	if (IDE_CONTROL_REG)
		hwif->OUTB(0x08, IDE_CONTROL_REG);
	if (pdc4030_cmd(drive,PROMISE_GET_CONFIG)) {
		return 0;
	}
	if (ide_wait_stat(&startstop, drive,DATA_READY,BAD_W_STAT,WAIT_DRQ)) {
		printk(KERN_INFO
			"%s: Failed Promise read config!\n",hwif->name);
		return 0;
	}
	hwif->ata_input_data(drive, &ident, SECTOR_WORDS);
	if (ident.id[1] != 'P' || ident.id[0] != 'T') {
		return 0;
	}
	printk(KERN_INFO "%s: Promise caching controller, ",hwif->name);
	switch(ident.type) {
		case 0x43:	printk("DC4030VL-2, "); break;
		case 0x41:	printk("DC4030VL-1, "); break;
		case 0x40:	printk("DC4030VL, "); break;
		default:
			printk("unknown - type 0x%02x - please report!\n"
			       ,ident.type);
			printk("Please e-mail the following data to "
			       "promise@pnd-pc.demon.co.uk along with\n"
			       "a description of your card and drives:\n");
			for (i=0; i < 0x90; i++) {
				printk("%02x ", ((unsigned char *)&ident)[i]);
				if ((i & 0x0f) == 0x0f) printk("\n");
			}
			return 0;
	}
	printk("%dKB cache, ",(int)ident.cache_mem);
	switch(ident.irq) {
            case 0x00: hwif->irq = 14; break;
            case 0x01: hwif->irq = 12; break;
            default:   hwif->irq = 15; break;
	}
	printk("on IRQ %d\n",hwif->irq);

	/*
	 * Once found and identified, we set up the next hwif in the array
	 * (hwif2 = ide_hwifs[hwif->index+1]) with the same io ports, irq
	 * and other settings as the main hwif. This gives us two "mated"
	 * hwifs pointing to the Promise card.
	 *
	 * We also have to shift the default values for the remaining
	 * interfaces "up by one" to make room for the second interface on the
	 * same set of values.
	 */

	hwif->chipset	= hwif2->chipset = ide_pdc4030;
	hwif->mate	= hwif2;
	hwif2->mate	= hwif;
	hwif2->channel	= 1;
	hwif->rqsize	= hwif2->rqsize = 127;
	hwif->addressing = hwif2->addressing = 1;
	hwif->selectproc = hwif2->selectproc = &promise_selectproc;
	hwif->serialized = hwif2->serialized = 1;
	/* DC4030 hosted drives need their own identify... */
	hwif->identify = hwif2->identify = &pdc4030_identify;

	/* Shift the remaining interfaces up by one */
	for (i=MAX_HWIFS-1 ; i > hwif->index+1 ; i--) {
		ide_hwif_t *h = &ide_hwifs[i];

#ifdef DEBUG
		printk(KERN_DEBUG "pdc4030: Shifting i/f %d values to i/f %d\n",i-1,i);
#endif /* DEBUG */
		ide_init_hwif_ports(&h->hw, (h-1)->io_ports[IDE_DATA_OFFSET], 0, NULL);
		memcpy(h->io_ports, h->hw.io_ports, sizeof(h->io_ports));
		h->noprobe = (h-1)->noprobe;
	}
	ide_init_hwif_ports(&hwif2->hw, hwif->io_ports[IDE_DATA_OFFSET], 0, NULL);
	memcpy(hwif2->io_ports, hwif->hw.io_ports, sizeof(hwif2->io_ports));
	hwif2->irq = hwif->irq;
	hwif2->hw.irq = hwif->hw.irq = hwif->irq;
	for (i=0; i<2 ; i++) {
		hwif->drives[i].io_32bit = 3;
		hwif2->drives[i].io_32bit = 3;
		hwif->drives[i].keep_settings = 1;
		hwif2->drives[i].keep_settings = 1;
		if (!ident.current_tm[i].cyl)
			hwif->drives[i].noprobe = 1;
		if (!ident.current_tm[i+2].cyl)
			hwif2->drives[i].noprobe = 1;
	}
	
	/* Now override the normal ide disk read/write */
	hwif->rw_disk = promise_rw_disk;
	hwif2->rw_disk = promise_rw_disk;
	
#ifndef HWIF_PROBE_CLASSIC_METHOD
	probe_hwif_init(&ide_hwifs[hwif->index]);
	probe_hwif_init(&ide_hwifs[hwif2->index]);
#endif /* HWIF_PROBE_CLASSIC_METHOD */

        return 1;
}

/*
 * detect_pdc4030()
 * Tests for the presence of a DC4030 Promise card on this interface
 * Returns: 1 if found, 0 if not found
 */
int __init detect_pdc4030(ide_hwif_t *hwif)
{
	ide_drive_t *drive = &hwif->drives[0];

	if (IDE_DATA_REG == 0) { /* Skip test for non-existent interface */
		return 0;
	}
	hwif->OUTB(0xF3, IDE_SECTOR_REG);
	hwif->OUTB(0x14, IDE_SELECT_REG);
	hwif->OUTB(PROMISE_EXTENDED_COMMAND, IDE_COMMAND_REG);
	
	ide_delay_50ms();

	if (hwif->INB(IDE_ERROR_REG) == 'P' &&
	    hwif->INB(IDE_NSECTOR_REG) == 'T' &&
	    hwif->INB(IDE_SECTOR_REG) == 'I') {
		return 1;
	} else {
		return 0;
	}
}


#ifndef MODULE
void __init ide_probe_for_pdc4030(void)
#else
int ide_probe_for_pdc4030(void)
#endif
{
	unsigned int	index;
	ide_hwif_t	*hwif;

	for (index = 0; index < MAX_HWIFS; index++) {
		hwif = &ide_hwifs[index];
		if (hwif->chipset == ide_unknown && detect_pdc4030(hwif)) {
#ifndef MODULE
			setup_pdc4030(hwif);
#else
			return setup_pdc4030(hwif);
#endif
		}
	}
#ifdef MODULE
	return 0;
#endif
}

void __init release_pdc4030(ide_hwif_t *hwif, ide_hwif_t *mate)
{
	hwif->chipset = ide_unknown;
	hwif->selectproc = NULL;
	hwif->serialized = 0;
	hwif->drives[0].io_32bit = 0;
	hwif->drives[1].io_32bit = 0;
	hwif->drives[0].keep_settings = 0;
	hwif->drives[1].keep_settings = 0;
	hwif->drives[0].noprobe = 0;
	hwif->drives[1].noprobe = 0;

	if (mate != NULL) {
		mate->chipset = ide_unknown;
		mate->selectproc = NULL;
		mate->serialized = 0;
		mate->drives[0].io_32bit = 0;
		mate->drives[1].io_32bit = 0;
		mate->drives[0].keep_settings = 0;
		mate->drives[1].keep_settings = 0;
		mate->drives[0].noprobe = 0;
		mate->drives[1].noprobe = 0;
	}
}

#ifndef MODULE
/*
 * init_pdc4030:
 *
 * called by ide.c when parsing command line
 */

void __init init_pdc4030(void)
{
	ide_register_driver(ide_probe_for_pdc4030);
}

#else

MODULE_AUTHOR("Peter Denison");
MODULE_DESCRIPTION("Support of Promise 4030 VLB series IDE chipsets");
MODULE_LICENSE("GPL");

int __init pdc4030_mod_init(void)
{
	if (!ide_probe_for_pdc4030())
		return -ENODEV;
        return 0;
}
module_init(pdc4030_mod_init);

void __init pdc4030_mod_exit(void)
{
	unsigned int    index;
	ide_hwif_t      *hwif;

	for (index = 0; index < MAX_HWIFS; index++) {
		hwif = &ide_hwifs[index];
		if (hwif->chipset == ide_pdc4030) {
			ide_hwif_t *mate = &ide_hwifs[hwif->index+1];
			if (mate->chipset == ide_pdc4030)
				release_pdc4030(hwif, mate);
			else
				release_pdc4030(hwif, NULL);
                }
        }
}
module_exit(pdc4030_mod_exit);
#endif

/*
 * promise_read_intr() is the handler for disk read/multread interrupts
 */
static ide_startstop_t promise_read_intr (ide_drive_t *drive)
{
	int total_remaining;
	unsigned int sectors_left, sectors_avail, nsect;
	struct request *rq;
	ata_status_t status;
#ifdef CONFIG_IDE_TASKFILE_IO
	unsigned long flags;
	char *to;
#endif /* CONFIG_IDE_TASKFILE_IO */

	status.all = HWIF(drive)->INB(IDE_STATUS_REG);
	if (!OK_STAT(status.all, DATA_READY, BAD_R_STAT))
		return DRIVER(drive)->error(drive,
			"promise_read_intr", status.all);

read_again:
	do {
		sectors_left = HWIF(drive)->INB(IDE_NSECTOR_REG);
		HWIF(drive)->INB(IDE_SECTOR_REG);
	} while (HWIF(drive)->INB(IDE_NSECTOR_REG) != sectors_left);
	rq = HWGROUP(drive)->rq;
	sectors_avail = rq->nr_sectors - sectors_left;
	if (!sectors_avail)
		goto read_again;

read_next:
	rq = HWGROUP(drive)->rq;
	nsect = rq->current_nr_sectors;
	if (nsect > sectors_avail)
		nsect = sectors_avail;
	sectors_avail -= nsect;
#ifdef CONFIG_IDE_TASKFILE_IO
	to = ide_map_buffer(rq, &flags);
	HWIF(drive)->ata_input_data(drive, to, nsect * SECTOR_WORDS);
#else /* !CONFIG_IDE_TASKFILE_IO */
	HWIF(drive)->ata_input_data(drive, rq->buffer, nsect * SECTOR_WORDS);
#endif /* CONFIG_IDE_TASKFILE_IO */

#ifdef DEBUG_READ
	printk(KERN_DEBUG "%s:  promise_read: sectors(%ld-%ld), "
	       "buf=0x%08lx, rem=%ld\n", drive->name, rq->sector,
	       rq->sector+nsect-1,
#ifdef CONFIG_IDE_TASKFILE_IO
		(unsigned long) to,
#else /* !CONFIG_IDE_TASKFILE_IO */
		(unsigned long) rq->buffer,
#endif /* CONFIG_IDE_TASKFILE_IO */
	       rq->nr_sectors-nsect);
#endif /* DEBUG_READ */

#ifdef CONFIG_IDE_TASKFILE_IO
	ide_unmap_buffer(to, &flags);
#else /* !CONFIG_IDE_TASKFILE_IO */
	rq->buffer += nsect<<9;
#endif /* CONFIG_IDE_TASKFILE_IO */
	rq->sector += nsect;
	rq->errors = 0;
	rq->nr_sectors -= nsect;
	total_remaining = rq->nr_sectors;
	if ((rq->current_nr_sectors -= nsect) <= 0) {
		DRIVER(drive)->end_request(drive, 1);
	}
/*
 * Now the data has been read in, do the following:
 * 
 * if there are still sectors left in the request, 
 *   if we know there are still sectors available from the interface,
 *     go back and read the next bit of the request.
 *   else if DRQ is asserted, there are more sectors available, so
 *     go back and find out how many, then read them in.
 *   else if BUSY is asserted, we are going to get an interrupt, so
 *     set the handler for the interrupt and just return
 */
	if (total_remaining > 0) {
		if (sectors_avail)
			goto read_next;
		status.all = HWIF(drive)->INB(IDE_STATUS_REG);
		if (status.b.drq)
			goto read_again;
		if (status.b.bsy) {
			if (HWGROUP(drive)->handler != NULL)
				BUG();
			ide_set_handler(drive,
					&promise_read_intr,
					WAIT_CMD,
					NULL);
#ifdef DEBUG_READ
			printk(KERN_DEBUG "%s: promise_read: waiting for"
			       "interrupt\n", drive->name);
#endif /* DEBUG_READ */
			return ide_started;
		}
		printk(KERN_ERR "%s: Eeek! promise_read_intr: sectors left "
		       "!DRQ !BUSY\n", drive->name);
		return DRIVER(drive)->error(drive,
				"promise read intr", status.all);
	}
	return ide_stopped;
}

/*
 * promise_complete_pollfunc()
 * This is the polling function for waiting (nicely!) until drive stops
 * being busy. It is invoked at the end of a write, after the previous poll
 * has finished.
 *
 * Once not busy, the end request is called.
 */
static ide_startstop_t promise_complete_pollfunc(ide_drive_t *drive)
{
	ide_hwgroup_t *hwgroup = HWGROUP(drive);
	struct request *rq = hwgroup->rq;
	int i;

	if ((HWIF(drive)->INB(IDE_STATUS_REG)) & BUSY_STAT) {
		if (time_before(jiffies, hwgroup->poll_timeout)) {
			if (hwgroup->handler != NULL)
				BUG();
			ide_set_handler(drive,
					&promise_complete_pollfunc,
					HZ/100,
					NULL);
			return ide_started; /* continue polling... */
		}
		hwgroup->poll_timeout = 0;
		printk(KERN_ERR "%s: completion timeout - still busy!\n",
		       drive->name);
		return DRIVER(drive)->error(drive, "busy timeout",
				HWIF(drive)->INB(IDE_STATUS_REG));
	}

	hwgroup->poll_timeout = 0;
#ifdef DEBUG_WRITE
	printk(KERN_DEBUG "%s: Write complete - end_request\n", drive->name);
#endif /* DEBUG_WRITE */
	for (i = rq->nr_sectors; i > 0; ) {
		i -= rq->current_nr_sectors;
		DRIVER(drive)->end_request(drive, 1);
	}
	return ide_stopped;
}

/*
 * promise_multwrite() transfers a block of up to mcount sectors of data
 * to a drive as part of a disk multiple-sector write operation.
 *
 * Returns 0 on success.
 *
 * Note that we may be called from two contexts - the do_rw_disk context
 * and IRQ context. The IRQ can happen any time after we've output the
 * full "mcount" number of sectors, so we must make sure we update the
 * state _before_ we output the final part of the data!
 */
int promise_multwrite (ide_drive_t *drive, unsigned int mcount)
{
	ide_hwgroup_t *hwgroup	= HWGROUP(drive);
	struct request *rq	= &hwgroup->wrq;

	do {
		char *buffer;
		int nsect = rq->current_nr_sectors;
#ifdef CONFIG_IDE_TASKFILE_IO
		unsigned long flags;
#endif /* CONFIG_IDE_TASKFILE_IO */

		if (nsect > mcount)
			nsect = mcount;
		mcount -= nsect;
#ifdef CONFIG_IDE_TASKFILE_IO
		buffer = ide_map_buffer(rq, &flags);
		rq->sector += nsect;
#else /* !CONFIG_IDE_TASKFILE_IO */
		buffer = rq->buffer;

		rq->sector += nsect;
		rq->buffer += nsect << 9;
#endif /* CONFIG_IDE_TASKFILE_IO */
		rq->nr_sectors -= nsect;
		rq->current_nr_sectors -= nsect;

		/* Do we move to the next bh after this? */
		if (!rq->current_nr_sectors) {
			struct buffer_head *bh = rq->bh->b_reqnext;

			/* end early early we ran out of requests */
			if (!bh) {
				mcount = 0;
			} else {
				rq->bh			= bh;
				rq->current_nr_sectors	= bh->b_size >> 9;
				rq->hard_cur_sectors = rq->current_nr_sectors;
				rq->buffer		= bh->b_data;
			}
		}

		/*
		 * Ok, we're all setup for the interrupt
		 * re-entering us on the last transfer.
		 */
		taskfile_output_data(drive, buffer, nsect<<7);
#ifdef CONFIG_IDE_TASKFILE_IO
		ide_unmap_buffer(buffer, &flags);
#endif /* CONFIG_IDE_TASKFILE_IO */
	} while (mcount);

	return 0;
}

/*
 * promise_write_pollfunc() is the handler for disk write completion polling.
 */
static ide_startstop_t promise_write_pollfunc (ide_drive_t *drive)
{
	ide_hwgroup_t *hwgroup = HWGROUP(drive);

	if (HWIF(drive)->INB(IDE_NSECTOR_REG) != 0) {
		if (time_before(jiffies, hwgroup->poll_timeout)) {
			if (hwgroup->handler != NULL)
				BUG();
			ide_set_handler(drive,
					&promise_write_pollfunc,
					HZ/100,
					NULL);
			return ide_started; /* continue polling... */
		}
		hwgroup->poll_timeout = 0;
		printk(KERN_ERR "%s: write timed-out!\n",drive->name);
		return DRIVER(drive)->error(drive, "write timeout",
				HWIF(drive)->INB(IDE_STATUS_REG));
	}

	/*
	 * Now write out last 4 sectors and poll for not BUSY
	 */
	promise_multwrite(drive, 4);
	hwgroup->poll_timeout = jiffies + WAIT_WORSTCASE;
	if (hwgroup->handler != NULL)
		BUG();
	ide_set_handler(drive, &promise_complete_pollfunc, HZ/100, NULL);
#ifdef DEBUG_WRITE
	printk(KERN_DEBUG "%s: Done last 4 sectors - status = %02x\n",
		drive->name, HWIF(drive)->INB(IDE_STATUS_REG));
#endif /* DEBUG_WRITE */
	return ide_started;
}

/*
 * promise_write() transfers a block of one or more sectors of data to a
 * drive as part of a disk write operation. All but 4 sectors are transferred
 * in the first attempt, then the interface is polled (nicely!) for completion
 * before the final 4 sectors are transferred. There is no interrupt generated
 * on writes (at least on the DC4030VL-2), we just have to poll for NOT BUSY.
 */
static ide_startstop_t promise_write (ide_drive_t *drive)
{
	ide_hwgroup_t *hwgroup = HWGROUP(drive);
	struct request *rq = &hwgroup->wrq;

#ifdef DEBUG_WRITE
	printk(KERN_DEBUG "%s: promise_write: sectors(%ld-%ld), "
	       "buffer=%p\n", drive->name, rq->sector,
	       rq->sector + rq->nr_sectors - 1, rq->buffer);
#endif /* DEBUG_WRITE */

	/*
	 * If there are more than 4 sectors to transfer, do n-4 then go into
	 * the polling strategy as defined above.
	 */
	if (rq->nr_sectors > 4) {
		if (promise_multwrite(drive, rq->nr_sectors - 4))
			return ide_stopped;
		hwgroup->poll_timeout = jiffies + WAIT_WORSTCASE;
		if (hwgroup->handler != NULL)	/* paranoia check */
			BUG();
		ide_set_handler (drive, &promise_write_pollfunc, HZ/100, NULL);
		return ide_started;
	} else {
	/*
	 * There are 4 or fewer sectors to transfer, do them all in one go
	 * and wait for NOT BUSY.
	 */
		if (promise_multwrite(drive, rq->nr_sectors))
			return ide_stopped;
		hwgroup->poll_timeout = jiffies + WAIT_WORSTCASE;
		if (hwgroup->handler != NULL)
			BUG();
		ide_set_handler(drive,
				&promise_complete_pollfunc,
				HZ/100,
				NULL);

#ifdef DEBUG_WRITE
		printk(KERN_DEBUG "%s: promise_write: <= 4 sectors, "
			"status = %02x\n", drive->name,
			HWIF(drive)->INB(IDE_STATUS_REG));
#endif /* DEBUG_WRITE */
		return ide_started;
	}
}

/*
 * do_pdc4030_io() is called from promise_rw_disk, having had the block number
 * already set up. It issues a READ or WRITE command to the Promise
 * controller, assuming LBA has been used to set up the block number.
 */
#ifndef CONFIG_IDE_TASKFILE_IO
ide_startstop_t do_pdc4030_io (ide_drive_t *drive, struct request *rq)
{
#else /* CONFIG_IDE_TASKFILE_IO */
ide_startstop_t do_pdc4030_io (ide_drive_t *drive, ide_task_t *task)
{
	struct request *rq	= HWGROUP(drive)->rq;
	task_struct_t *taskfile = (task_struct_t *) task->tfRegister;
#endif /* CONFIG_IDE_TASKFILE_IO */
	ide_startstop_t startstop;
	unsigned long timeout;
	u8 stat = 0;

#ifdef CONFIG_IDE_TASKFILE_IO
	if (IDE_CONTROL_REG)
		HWIF(drive)->OUTB(drive->ctl, IDE_CONTROL_REG);	/* clear nIEN */
	SELECT_MASK(drive, 0);
	HWIF(drive)->OUTB(taskfile->feature, IDE_FEATURE_REG);
	HWIF(drive)->OUTB(taskfile->sector_count, IDE_NSECTOR_REG);
	/* refers to number of sectors to transfer */
	HWIF(drive)->OUTB(taskfile->sector_number, IDE_SECTOR_REG);
	/* refers to sector offset or start sector */
	HWIF(drive)->OUTB(taskfile->low_cylinder, IDE_LCYL_REG);
	HWIF(drive)->OUTB(taskfile->high_cylinder, IDE_HCYL_REG);
	HWIF(drive)->OUTB(taskfile->device_head, IDE_SELECT_REG);
	HWIF(drive)->OUTB(taskfile->command, IDE_COMMAND_REG);
#endif /* CONFIG_IDE_TASKFILE_IO */

	switch(rq->cmd) {
	case READ:
#ifndef CONFIG_IDE_TASKFILE_IO
		HWIF(drive)->OUTB(PROMISE_READ, IDE_COMMAND_REG);
#endif /* CONFIG_IDE_TASKFILE_IO */
/*
 * The card's behaviour is odd at this point. If the data is
 * available, DRQ will be true, and no interrupt will be
 * generated by the card. If this is the case, we need to call the 
 * "interrupt" handler (promise_read_intr) directly. Otherwise, if
 * an interrupt is going to occur, bit0 of the SELECT register will
 * be high, so we can set the handler the just return and be interrupted.
 * If neither of these is the case, we wait for up to 50ms (badly I'm
 * afraid!) until one of them is.
 */
		timeout = jiffies + HZ/20; /* 50ms wait */
		do {
			stat = HWIF(drive)->INB(IDE_STATUS_REG);
			if (stat & DRQ_STAT) {
				udelay(1);
				return promise_read_intr(drive);
			}
			if (HWIF(drive)->INB(IDE_SELECT_REG) & 0x01) {
#ifdef DEBUG_READ
				printk(KERN_DEBUG "%s: read: waiting for "
						"interrupt\n", drive->name);
#endif /* DEBUG_READ */
				ide_set_handler(drive,
						&promise_read_intr,
						WAIT_CMD,
						NULL);
				return ide_started;
			}
			udelay(1);
		} while (time_before(jiffies, timeout));

		printk(KERN_ERR "%s: reading: No DRQ and not "
				"waiting - Odd!\n", drive->name);
		return ide_stopped;
	case WRITE:
#ifndef CONFIG_IDE_TASKFILE_IO
		HWIF(drive)->OUTB(PROMISE_WRITE, IDE_COMMAND_REG);
#endif /* CONFIG_IDE_TASKFILE_IO */
		if (ide_wait_stat(&startstop, drive, DATA_READY,
				drive->bad_wstat, WAIT_DRQ)) {
			printk(KERN_ERR "%s: no DRQ after issuing "
				"PROMISE_WRITE\n", drive->name);
			return startstop;
	    	}
		if (!drive->unmask)
			local_irq_disable();
		HWGROUP(drive)->wrq = *rq; /* scratchpad */
		return promise_write(drive);
	default:
		printk("KERN_WARNING %s: bad command: %d\n",
			drive->name, rq->cmd);
		DRIVER(drive)->end_request(drive, 0);
		return ide_stopped;
	}
}

static ide_startstop_t promise_rw_disk (ide_drive_t *drive, struct request *rq, unsigned long block)
{
	/* The four drives on the two logical (one physical) interfaces
	   are distinguished by writing the drive number (0-3) to the
	   Feature register.
	   FIXME: Is promise_selectproc now redundant??
	*/
	int drive_number = (HWIF(drive)->channel << 1) + drive->select.b.unit;
#ifndef CONFIG_IDE_TASKFILE_IO
	ide_hwif_t *hwif = HWIF(drive);

	BUG_ON(rq->nr_sectors > 127);

	if (IDE_CONTROL_REG)
		hwif->OUTB(drive->ctl, IDE_CONTROL_REG);

#ifdef DEBUG
	printk("%s: %sing: LBAsect=%ld, sectors=%ld, "
		"buffer=0x%08lx\n", drive->name,
		(rq->cmd==READ)?"read":"writ", block,
		rq->nr_sectors, (unsigned long) rq->buffer);
#endif
	hwif->OUTB(drive_number, IDE_FEATURE_REG);
	hwif->OUTB(rq->nr_sectors, IDE_NSECTOR_REG);
	hwif->OUTB(block,IDE_SECTOR_REG);
	hwif->OUTB(block>>=8,IDE_LCYL_REG);
	hwif->OUTB(block>>=8,IDE_HCYL_REG);
	hwif->OUTB(((block>>8)&0x0f)|drive->select.all,IDE_SELECT_REG);

	return do_pdc4030_io(drive, rq);

#else /* CONFIG_IDE_TASKFILE_IO */

	struct hd_drive_task_hdr	taskfile;
	ide_task_t			args;

	memset(&taskfile, 0, sizeof(struct hd_drive_task_hdr));

	taskfile.feature	= drive_number;
	taskfile.sector_count	= rq->nr_sectors;
	taskfile.sector_number	= block;
	taskfile.low_cylinder	= (block>>=8);
	taskfile.high_cylinder	= (block>>=8);
	taskfile.device_head	= ((block>>8)&0x0f)|drive->select.all;
	taskfile.command	= (rq->cmd==READ)?PROMISE_READ:PROMISE_WRITE;

	memcpy(args.tfRegister, &taskfile, sizeof(struct hd_drive_task_hdr));
	memset(args.hobRegister, 0, sizeof(struct hd_drive_hob_hdr));
	/* We can't call ide_cmd_type_parser here, since it won't understand
	   our command, but that doesn't matter, since we don't use the
	   generic interrupt handlers either. Setup the bits of args that we
	   do need.
	*/
	args.handler		= NULL;
	args.rq			= (struct request *) rq;
	rq->special		= (ide_task_t *)&args;

	return do_pdc4030_io(drive, &args);
#endif /* CONFIG_IDE_TASKFILE_IO */
}
