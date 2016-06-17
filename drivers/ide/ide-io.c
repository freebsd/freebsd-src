/*
 *	IDE I/O functions
 *
 *	Basic PIO and command management functionality.
 *
 * This code was split off from ide.c. See ide.c for history and original
 * copyrights.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * For the avoidance of doubt the "preferred form" of this code is one which
 * is in an open non patent encumbered format. Where cryptographic key signing
 * forms part of the process of creating an executable the information
 * including keys needed to generate an equivalently functional executable
 * are deemed to be part of the source code.
 */
 
 
#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/genhd.h>
#include <linux/blkpg.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/completion.h>
#include <linux/reboot.h>
#include <linux/cdrom.h>
#include <linux/seq_file.h>
#include <linux/kmod.h>

#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/bitops.h>

#include "ide_modes.h"

/*
 *	ide_end_request		-	complete an IDE I/O
 *	@drive: IDE device for the I/O
 *	@uptodate: 
 *
 *	This is our end_request wrapper function. We complete the I/O
 *	update random number input and dequeue the request.
 */
 
int ide_end_request (ide_drive_t *drive, int uptodate)
{
	struct request *rq;
	unsigned long flags;
	int ret = 1;

	spin_lock_irqsave(&io_request_lock, flags);
	rq = HWGROUP(drive)->rq;

	/*
	 * decide whether to reenable DMA -- 3 is a random magic for now,
	 * if we DMA timeout more than 3 times, just stay in PIO
	 */
	if (drive->state == DMA_PIO_RETRY && drive->retry_pio <= 3) {
		drive->state = 0;
		HWGROUP(drive)->hwif->ide_dma_on(drive);
	}

	if (!end_that_request_first(rq, uptodate, drive->name)) {
		add_blkdev_randomness(MAJOR(rq->rq_dev));
		blkdev_dequeue_request(rq);
		HWGROUP(drive)->rq = NULL;
		end_that_request_last(rq);
		ret = 0;
	}

	spin_unlock_irqrestore(&io_request_lock, flags);
	return ret;
}

EXPORT_SYMBOL(ide_end_request);

/**
 *	ide_end_drive_cmd	-	end an explicit drive command
 *	@drive: command 
 *	@stat: status bits
 *	@err: error bits
 *
 *	Clean up after success/failure of an explicit drive command.
 *	These get thrown onto the queue so they are synchronized with
 *	real I/O operations on the drive.
 *
 *	In LBA48 mode we have to read the register set twice to get
 *	all the extra information out.
 */
 
void ide_end_drive_cmd (ide_drive_t *drive, u8 stat, u8 err)
{
	ide_hwif_t *hwif = HWIF(drive);
	unsigned long flags;
	struct request *rq;

	spin_lock_irqsave(&io_request_lock, flags);
	rq = HWGROUP(drive)->rq;
	spin_unlock_irqrestore(&io_request_lock, flags);

	switch(rq->cmd) {
		case IDE_DRIVE_CMD:
		{
			u8 *args = (u8 *) rq->buffer;
			if (rq->errors == 0)
				rq->errors = !OK_STAT(stat,READY_STAT,BAD_STAT);

			if (args) {
				args[0] = stat;
				args[1] = err;
				args[2] = hwif->INB(IDE_NSECTOR_REG);
			}
			break;
		}
		case IDE_DRIVE_TASK:
		{
			u8 *args = (u8 *) rq->buffer;
			if (rq->errors == 0)
				rq->errors = !OK_STAT(stat,READY_STAT,BAD_STAT);

			if (args) {
				args[0] = stat;
				args[1] = err;
				args[2] = hwif->INB(IDE_NSECTOR_REG);
				args[3] = hwif->INB(IDE_SECTOR_REG);
				args[4] = hwif->INB(IDE_LCYL_REG);
				args[5] = hwif->INB(IDE_HCYL_REG);
				args[6] = hwif->INB(IDE_SELECT_REG);
			}
			break;
		}
		case IDE_DRIVE_TASKFILE:
		{
			ide_task_t *args = (ide_task_t *) rq->special;
			if (rq->errors == 0)
				rq->errors = !OK_STAT(stat,READY_STAT,BAD_STAT);
				
			if (args) {
				if (args->tf_in_flags.b.data) {
					u16 data			= hwif->INW(IDE_DATA_REG);
					args->tfRegister[IDE_DATA_OFFSET]	= (data) & 0xFF;
					args->hobRegister[IDE_DATA_OFFSET_HOB]	= (data >> 8) & 0xFF;
				}
				args->tfRegister[IDE_ERROR_OFFSET]   = err;
				args->tfRegister[IDE_NSECTOR_OFFSET] = hwif->INB(IDE_NSECTOR_REG);
				args->tfRegister[IDE_SECTOR_OFFSET]  = hwif->INB(IDE_SECTOR_REG);
				args->tfRegister[IDE_LCYL_OFFSET]    = hwif->INB(IDE_LCYL_REG);
				args->tfRegister[IDE_HCYL_OFFSET]    = hwif->INB(IDE_HCYL_REG);
				args->tfRegister[IDE_SELECT_OFFSET]  = hwif->INB(IDE_SELECT_REG);
				args->tfRegister[IDE_STATUS_OFFSET]  = stat;

				if (drive->addressing == 1) {
					hwif->OUTB(drive->ctl|0x80, IDE_CONTROL_REG_HOB);
					args->hobRegister[IDE_FEATURE_OFFSET_HOB] = hwif->INB(IDE_FEATURE_REG);
					args->hobRegister[IDE_NSECTOR_OFFSET_HOB] = hwif->INB(IDE_NSECTOR_REG);
					args->hobRegister[IDE_SECTOR_OFFSET_HOB]  = hwif->INB(IDE_SECTOR_REG);
					args->hobRegister[IDE_LCYL_OFFSET_HOB]    = hwif->INB(IDE_LCYL_REG);
					args->hobRegister[IDE_HCYL_OFFSET_HOB]    = hwif->INB(IDE_HCYL_REG);
				}
			}
			break;
		}
		default:
			break;
	}
	spin_lock_irqsave(&io_request_lock, flags);
	blkdev_dequeue_request(rq);
	HWGROUP(drive)->rq = NULL;
	end_that_request_last(rq);
	spin_unlock_irqrestore(&io_request_lock, flags);
}

EXPORT_SYMBOL(ide_end_drive_cmd);

/**
 *	try_to_flush_leftover_data	-	flush junk
 *	@drive: drive to flush
 *
 *	try_to_flush_leftover_data() is invoked in response to a drive
 *	unexpectedly having its DRQ_STAT bit set.  As an alternative to
 *	resetting the drive, this routine tries to clear the condition
 *	by read a sector's worth of data from the drive.  Of course,
 *	this may not help if the drive is *waiting* for data from *us*.
 */

void try_to_flush_leftover_data (ide_drive_t *drive)
{
	int i = (drive->mult_count ? drive->mult_count : 1) * SECTOR_WORDS;

	if (drive->media != ide_disk)
		return;
	while (i > 0) {
		u32 buffer[16];
		u32 wcount = (i > 16) ? 16 : i;

		i -= wcount;
		HWIF(drive)->ata_input_data(drive, buffer, wcount);
	}
}

EXPORT_SYMBOL(try_to_flush_leftover_data);

/*
 * FIXME Add an ATAPI error
 */

/**
 *	ide_error	-	handle an error on the IDE
 *	@drive: drive the error occurred on
 *	@msg: message to report
 *	@stat: status bits
 *
 *	ide_error() takes action based on the error returned by the drive.
 *	For normal I/O that may well include retries. We deal with
 *	both new-style (taskfile) and old style command handling here.
 *	In the case of taskfile command handling there is work left to
 *	do
 */
 
ide_startstop_t ide_error (ide_drive_t *drive, const char *msg, u8 stat)
{
	ide_hwif_t *hwif;
	struct request *rq;
	u8 err;

	err = ide_dump_status(drive, msg, stat);
	if (drive == NULL || (rq = HWGROUP(drive)->rq) == NULL)
		return ide_stopped;

	hwif = HWIF(drive);
	/* retry only "normal" I/O: */
	if (rq->cmd == IDE_DRIVE_CMD || rq->cmd == IDE_DRIVE_TASK) {
		rq->errors = 1;
		ide_end_drive_cmd(drive, stat, err);
		return ide_stopped;
	}
	if (rq->cmd == IDE_DRIVE_TASKFILE) {
		rq->errors = 1;
		ide_end_drive_cmd(drive, stat, err);
//		ide_end_taskfile(drive, stat, err);
		return ide_stopped;
	}

	if (stat & BUSY_STAT || ((stat & WRERR_STAT) && !drive->nowerr)) {
		 /* other bits are useless when BUSY */
		rq->errors |= ERROR_RESET;
	} else {
		if (drive->media != ide_disk)
			goto media_out;

		if (stat & ERR_STAT) {
			/* err has different meaning on cdrom and tape */
			if (err == ABRT_ERR) {
				if (drive->select.b.lba &&
				    (hwif->INB(IDE_COMMAND_REG) == WIN_SPECIFY))
					/* some newer drives don't
					 * support WIN_SPECIFY
					 */
					return ide_stopped;
			} else if ((err & BAD_CRC) == BAD_CRC) {
				drive->crc_count++;
				/* UDMA crc error -- just retry the operation */
			} else if (err & (BBD_ERR | ECC_ERR)) {
				/* retries won't help these */
				rq->errors = ERROR_MAX;
			} else if (err & TRK0_ERR) {
				/* help it find track zero */
				rq->errors |= ERROR_RECAL;
			}
		}
media_out:
		if ((stat & DRQ_STAT) && rq->cmd != WRITE)
			try_to_flush_leftover_data(drive);
	}
	if (hwif->INB(IDE_STATUS_REG) & (BUSY_STAT|DRQ_STAT)) {
		/* force an abort */
		hwif->OUTB(WIN_IDLEIMMEDIATE,IDE_COMMAND_REG);
	}
	if (rq->errors >= ERROR_MAX) {
		DRIVER(drive)->end_request(drive, 0);
	} else {
		if ((rq->errors & ERROR_RESET) == ERROR_RESET) {
			++rq->errors;
			return ide_do_reset(drive);
		}
		if ((rq->errors & ERROR_RECAL) == ERROR_RECAL)
			drive->special.b.recalibrate = 1;
		++rq->errors;
	}
	return ide_stopped;
}

EXPORT_SYMBOL(ide_error);

/**
 *	ide_abort	-	abort pending IDE operatins
 *	@drive: drive the error occurred on
 *	@msg: message to report
 *
 *	ide_abort kills and cleans up when we are about to do a 
 *	host initiated reset on active commands. Longer term we
 *	want handlers to have sensible abort handling themselves
 *
 *	This differs fundamentally from ide_error because in 
 *	this case the command is doing just fine when we
 *	blow it away.
 */
 
ide_startstop_t ide_abort(ide_drive_t *drive, const char *msg)
{
	ide_hwif_t *hwif;
	struct request *rq;

	if (drive == NULL || (rq = HWGROUP(drive)->rq) == NULL)
		return ide_stopped;

	hwif = HWIF(drive);
	/* retry only "normal" I/O: */
	if (rq->cmd == IDE_DRIVE_CMD || rq->cmd == IDE_DRIVE_TASK) {
		rq->errors = 1;
		ide_end_drive_cmd(drive, BUSY_STAT, 0);
		return ide_stopped;
	}
	if (rq->cmd == IDE_DRIVE_TASKFILE) {
		rq->errors = 1;
		ide_end_drive_cmd(drive, BUSY_STAT, 0);
//		ide_end_taskfile(drive, BUSY_STAT, 0);
		return ide_stopped;
	}

	rq->errors |= ERROR_RESET;
	DRIVER(drive)->end_request(drive, 0);
	return ide_stopped;
}

EXPORT_SYMBOL(ide_abort);

/**
 *	ide_cmd		-	issue a simple drive command
 *	@drive: drive the command is for
 *	@cmd: command byte
 *	@nsect: sector byte
 *	@handler: handler for the command completion
 *
 *	Issue a simple drive command with interrupts.
 *	The drive must be selected beforehand.
 */

void ide_cmd (ide_drive_t *drive, u8 cmd, u8 nsect, ide_handler_t *handler)
{
	ide_hwif_t *hwif = HWIF(drive);
	if (IDE_CONTROL_REG)
		hwif->OUTB(drive->ctl,IDE_CONTROL_REG);	/* clear nIEN */
	SELECT_MASK(drive,0);
	hwif->OUTB(nsect,IDE_NSECTOR_REG);
	ide_execute_command(drive, cmd, handler, WAIT_CMD, NULL);
}

EXPORT_SYMBOL(ide_cmd);

/**
 *	drive_cmd_intr		- 	drive command completion interrupt
 *	@drive: drive the completion interrupt occurred on
 *
 *	drive_cmd_intr() is invoked on completion of a special DRIVE_CMD.
 *	We do any neccessary daya reading and then wait for the drive to
 *	go non busy. At that point we may read the error data and complete
 *	the request
 */
 
ide_startstop_t drive_cmd_intr (ide_drive_t *drive)
{
	struct request *rq = HWGROUP(drive)->rq;
	ide_hwif_t *hwif = HWIF(drive);
	u8 *args = (u8 *) rq->buffer;
	u8 stat = hwif->INB(IDE_STATUS_REG);
	int retries = 10;

	local_irq_enable();
	if ((stat & DRQ_STAT) && args && args[3]) {
		u8 io_32bit = drive->io_32bit;
		drive->io_32bit = 0;
		hwif->ata_input_data(drive, &args[4], args[3] * SECTOR_WORDS);
		drive->io_32bit = io_32bit;
		while (((stat = hwif->INB(IDE_STATUS_REG)) & BUSY_STAT) && retries--)
			udelay(100);
	}

	if (!OK_STAT(stat, READY_STAT, BAD_STAT))
		return DRIVER(drive)->error(drive, "drive_cmd", stat);
		/* calls ide_end_drive_cmd */
	ide_end_drive_cmd(drive, stat, hwif->INB(IDE_ERROR_REG));
	return ide_stopped;
}

EXPORT_SYMBOL(drive_cmd_intr);

/**
 *	do_special		-	issue some special commands
 *	@drive: drive the command is for
 *
 *	do_special() is used to issue WIN_SPECIFY, WIN_RESTORE, and WIN_SETMULT
 *	commands to a drive.  It used to do much more, but has been scaled
 *	back.
 */

ide_startstop_t do_special (ide_drive_t *drive)
{
	special_t *s = &drive->special;

#ifdef DEBUG
	printk("%s: do_special: 0x%02x\n", drive->name, s->all);
#endif
	if (s->b.set_tune) {
		s->b.set_tune = 0;
		if (HWIF(drive)->tuneproc != NULL)
			HWIF(drive)->tuneproc(drive, drive->tune_req);
		return ide_stopped;
	}
	else
		return DRIVER(drive)->special(drive);
}

EXPORT_SYMBOL(do_special);

/**
 *	execute_drive_command	-	issue special drive command
 *	@drive: the drive to issue th command on
 *	@rq: the request structure holding the command
 *
 *	execute_drive_cmd() issues a special drive command,  usually 
 *	initiated by ioctl() from the external hdparm program. The
 *	command can be a drive command, drive task or taskfile 
 *	operation. Weirdly you can call it with NULL to wait for
 *	all commands to finish. Don't do this as that is due to change
 */

ide_startstop_t execute_drive_cmd (ide_drive_t *drive, struct request *rq)
{
	ide_hwif_t *hwif = HWIF(drive);
 	switch(rq->cmd) {
 		case IDE_DRIVE_TASKFILE:
 		{
 			ide_task_t *args = rq->special;
 
 			if (!(args)) break;
 
			if (args->tf_out_flags.all != 0) 
				return flagged_taskfile(drive, args);
			return do_rw_taskfile(drive, args);
 		}
 		case IDE_DRIVE_TASK:
 		{
 			u8 *args = rq->buffer;
 			u8 sel;
 
 			if (!(args)) break;
#ifdef DEBUG
 			printk("%s: DRIVE_TASK_CMD ", drive->name);
 			printk("cmd=0x%02x ", args[0]);
 			printk("fr=0x%02x ", args[1]);
 			printk("ns=0x%02x ", args[2]);
 			printk("sc=0x%02x ", args[3]);
 			printk("lcyl=0x%02x ", args[4]);
 			printk("hcyl=0x%02x ", args[5]);
 			printk("sel=0x%02x\n", args[6]);
#endif
 			hwif->OUTB(args[1], IDE_FEATURE_REG);
 			hwif->OUTB(args[3], IDE_SECTOR_REG);
 			hwif->OUTB(args[4], IDE_LCYL_REG);
 			hwif->OUTB(args[5], IDE_HCYL_REG);
 			sel = (args[6] & ~0x10);
 			if (drive->select.b.unit)
 				sel |= 0x10;
 			hwif->OUTB(sel, IDE_SELECT_REG);
 			ide_cmd(drive, args[0], args[2], &drive_cmd_intr);
 			return ide_started;
 		}
 		case IDE_DRIVE_CMD:
 		{
 			u8 *args = rq->buffer;
 
 			if (!(args)) break;
#ifdef DEBUG
 			printk("%s: DRIVE_CMD ", drive->name);
 			printk("cmd=0x%02x ", args[0]);
 			printk("sc=0x%02x ", args[1]);
 			printk("fr=0x%02x ", args[2]);
 			printk("xx=0x%02x\n", args[3]);
#endif
 			if (args[0] == WIN_SMART) {
 				hwif->OUTB(0x4f, IDE_LCYL_REG);
 				hwif->OUTB(0xc2, IDE_HCYL_REG);
 				hwif->OUTB(args[2],IDE_FEATURE_REG);
 				hwif->OUTB(args[1],IDE_SECTOR_REG);
 				ide_cmd(drive, args[0], args[3], &drive_cmd_intr);
 				return ide_started;
 			}
 			hwif->OUTB(args[2],IDE_FEATURE_REG);
 			ide_cmd(drive, args[0], args[1], &drive_cmd_intr);
 			return ide_started;
 		}
 		default:
 			break;
 	}
 	/*
 	 * NULL is actually a valid way of waiting for
 	 * all current requests to be flushed from the queue.
 	 */
#ifdef DEBUG
 	printk("%s: DRIVE_CMD (null)\n", drive->name);
#endif
 	ide_end_drive_cmd(drive,
			hwif->INB(IDE_STATUS_REG),
			hwif->INB(IDE_ERROR_REG));
 	return ide_stopped;
}

EXPORT_SYMBOL(execute_drive_cmd);

/**
 *	ide_start_request	-	start of I/O and command issuing for IDE
 *
 *	ide_start_request() initiates handling of a new I/O request. It
 *	accepts commands and I/O (read/write) requests. It also does
 *	the final remapping for weird stuff like EZDrive. Once 
 *	device mapper can work sector level the EZDrive stuff can go away
 *
 *	FIXME: this function needs a rename
 */
 
static ide_startstop_t ide_start_request (ide_drive_t *drive, struct request *rq)
{
	ide_startstop_t startstop;
	unsigned long block, blockend;
	unsigned int minor = MINOR(rq->rq_dev), unit = minor >> PARTN_BITS;
	ide_hwif_t *hwif = HWIF(drive);

#ifdef DEBUG
	printk("%s: ide_start_request: current=0x%08lx\n",
		hwif->name, (unsigned long) rq);
#endif

	/* bail early if we've exceeded max_failures */
	if (!drive->present || (drive->max_failures && (drive->failures > drive->max_failures))) {
		goto kill_rq;
	}

	/*
	 * bail early if we've sent a device to sleep, however how to wake
	 * this needs to be a masked flag.  FIXME for proper operations.
	 */
	if (drive->suspend_reset) {
		goto kill_rq;
	}

	if (unit >= MAX_DRIVES) {
		printk(KERN_ERR "%s: bad device number: %s\n",
			hwif->name, kdevname(rq->rq_dev));
		goto kill_rq;
	}
#ifdef DEBUG
	if (rq->bh && !buffer_locked(rq->bh)) {
		printk(KERN_ERR "%s: block not locked\n", drive->name);
		goto kill_rq;
	}
#endif
	block    = rq->sector;
	blockend = block + rq->nr_sectors;

	if (blk_fs_request(rq) &&
	    (drive->media == ide_disk || drive->media == ide_floppy)) {
		if ((blockend < block) || (blockend > drive->part[minor&PARTN_MASK].nr_sects)) {
			printk(KERN_ERR "%s%c: bad access: block=%ld, count=%ld\n", drive->name,
			 (minor&PARTN_MASK)?'0'+(minor&PARTN_MASK):' ', block, rq->nr_sectors);
			goto kill_rq;
		}
		block += drive->part[minor&PARTN_MASK].start_sect + drive->sect0;
	}
	/* Yecch - this will shift the entire interval,
	   possibly killing some innocent following sector */
	if (block == 0 && drive->remap_0_to_1 == 1)
		block = 1;  /* redirect MBR access to EZ-Drive partn table */

	SELECT_DRIVE(drive);
	if (ide_wait_stat(&startstop, drive, drive->ready_stat, BUSY_STAT|DRQ_STAT, WAIT_READY)) {
		printk(KERN_ERR "%s: drive not ready for command\n", drive->name);
		return startstop;
	}
	if (!drive->special.all) {
		switch(rq->cmd) {
			case IDE_DRIVE_CMD:
			case IDE_DRIVE_TASK:
				return execute_drive_cmd(drive, rq);
			case IDE_DRIVE_TASKFILE:
				return execute_drive_cmd(drive, rq);
			default:
				break;
		}
		return (DRIVER(drive)->do_request(drive, rq, block));
	}
	return do_special(drive);
kill_rq:
	DRIVER(drive)->end_request(drive, 0);
	return ide_stopped;
}

/**
 *	ide_stall_queue		-	pause an IDE device
 *	@drive: drive to stall
 *	@timeout: time to stall for (jiffies)
 *
 *	ide_stall_queue() can be used by a drive to give excess bandwidth back
 *	to the hwgroup by sleeping for timeout jiffies.
 */
 
void ide_stall_queue (ide_drive_t *drive, unsigned long timeout)
{
	if (timeout > WAIT_WORSTCASE)
		timeout = WAIT_WORSTCASE;
	drive->sleep = timeout + jiffies;
}

EXPORT_SYMBOL(ide_stall_queue);

#define WAKEUP(drive)	((drive)->service_start + 2 * (drive)->service_time)

/**
 *	choose_drive		-	select a drive to service
 *	@hwgroup: hardware group to select on
 *
 *	choose_drive() selects the next drive which will be serviced.
 *	This is neccessary because the IDE layer can't issue commands
 *	to both drives on the same cable, unlike SCSI.
 */
 
static inline ide_drive_t *choose_drive (ide_hwgroup_t *hwgroup)
{
	ide_drive_t *drive, *best;

repeat:	
	best = NULL;
	drive = hwgroup->drive;
	do {
		if (!blk_queue_empty(&drive->queue) && (!drive->sleep || time_after_eq(jiffies, drive->sleep))) {
			if (!best
			 || (drive->sleep && (!best->sleep || 0 < (signed long)(best->sleep - drive->sleep)))
			 || (!best->sleep && 0 < (signed long)(WAKEUP(best) - WAKEUP(drive))))
			{
				if (!blk_queue_plugged(&drive->queue))
					best = drive;
			}
		}
	} while ((drive = drive->next) != hwgroup->drive);
	if (best && best->nice1 && !best->sleep && best != hwgroup->drive && best->service_time > WAIT_MIN_SLEEP) {
		long t = (signed long)(WAKEUP(best) - jiffies);
		if (t >= WAIT_MIN_SLEEP) {
		/*
		 * We *may* have some time to spare, but first let's see if
		 * someone can potentially benefit from our nice mood today..
		 */
			drive = best->next;
			do {
				if (!drive->sleep
				 && 0 < (signed long)(WAKEUP(drive) - (jiffies - best->service_time))
				 && 0 < (signed long)((jiffies + t) - WAKEUP(drive)))
				{
					ide_stall_queue(best, IDE_MIN(t, 10 * WAIT_MIN_SLEEP));
					goto repeat;
				}
			} while ((drive = drive->next) != best);
		}
	}
	return best;
}

/*
 * Issue a new request to a drive from hwgroup
 * Caller must have already done spin_lock_irqsave(&io_request_lock, ..);
 *
 * A hwgroup is a serialized group of IDE interfaces.  Usually there is
 * exactly one hwif (interface) per hwgroup, but buggy controllers (eg. CMD640)
 * may have both interfaces in a single hwgroup to "serialize" access.
 * Or possibly multiple ISA interfaces can share a common IRQ by being grouped
 * together into one hwgroup for serialized access.
 *
 * Note also that several hwgroups can end up sharing a single IRQ,
 * possibly along with many other devices.  This is especially common in
 * PCI-based systems with off-board IDE controller cards.
 *
 * The IDE driver uses the single global io_request_lock spinlock to protect
 * access to the request queues, and to protect the hwgroup->busy flag.
 *
 * The first thread into the driver for a particular hwgroup sets the
 * hwgroup->busy flag to indicate that this hwgroup is now active,
 * and then initiates processing of the top request from the request queue.
 *
 * Other threads attempting entry notice the busy setting, and will simply
 * queue their new requests and exit immediately.  Note that hwgroup->busy
 * remains set even when the driver is merely awaiting the next interrupt.
 * Thus, the meaning is "this hwgroup is busy processing a request".
 *
 * When processing of a request completes, the completing thread or IRQ-handler
 * will start the next request from the queue.  If no more work remains,
 * the driver will clear the hwgroup->busy flag and exit.
 *
 * The io_request_lock (spinlock) is used to protect all access to the
 * hwgroup->busy flag, but is otherwise not needed for most processing in
 * the driver.  This makes the driver much more friendlier to shared IRQs
 * than previous designs, while remaining 100% (?) SMP safe and capable.
 */
/* --BenH: made non-static as ide-pmac.c uses it to kick the hwgroup back
 *         into life on wakeup from machine sleep.
 */ 
void ide_do_request (ide_hwgroup_t *hwgroup, int masked_irq)
{
	ide_drive_t	*drive;
	ide_hwif_t	*hwif;
	struct request	*rq;
	ide_startstop_t	startstop;

	/* for atari only: POSSIBLY BROKEN HERE(?) */
	ide_get_lock(ide_intr, hwgroup);

	/* necessary paranoia: ensure IRQs are masked on local CPU */
	local_irq_disable();

	while (!hwgroup->busy) {
		hwgroup->busy = 1;
		drive = choose_drive(hwgroup);
		if (drive == NULL) {
			unsigned long sleep = 0;
			hwgroup->rq = NULL;
			drive = hwgroup->drive;
			do {
				if (drive->sleep && (!sleep || 0 < (signed long)(sleep - drive->sleep)))
					sleep = drive->sleep;
			} while ((drive = drive->next) != hwgroup->drive);
			if (sleep) {
		/*
		 * Take a short snooze, and then wake up this hwgroup again.
		 * This gives other hwgroups on the same a chance to
		 * play fairly with us, just in case there are big differences
		 * in relative throughputs.. don't want to hog the cpu too much.
		 */
				if (time_before(sleep, jiffies + WAIT_MIN_SLEEP))
					sleep = jiffies + WAIT_MIN_SLEEP;
#if 1
				if (timer_pending(&hwgroup->timer))
					printk(KERN_ERR "ide_set_handler: timer already active\n");
#endif
				/* so that ide_timer_expiry knows what to do */
				hwgroup->sleeping = 1;
				mod_timer(&hwgroup->timer, sleep);
				/* we purposely leave hwgroup->busy==1
				 * while sleeping */
			} else {
				/* Ugly, but how can we sleep for the lock
				 * otherwise? perhaps from tq_disk?
				 */

				/* for atari only */
				ide_release_lock();
				hwgroup->busy = 0;
			}
			/* no more work for this hwgroup (for now) */
			return;
		}
		hwif = HWIF(drive);
		if (hwgroup->hwif->sharing_irq &&
		    hwif != hwgroup->hwif &&
		    hwif->io_ports[IDE_CONTROL_OFFSET]) {
			/* set nIEN for previous hwif */
			SELECT_INTERRUPT(drive);
		}
		hwgroup->hwif = hwif;
		hwgroup->drive = drive;
		drive->sleep = 0;
		drive->service_start = jiffies;

		/* paranoia */
		if (blk_queue_plugged(&drive->queue))
			printk(KERN_ERR "%s: Huh? nuking plugged queue\n", drive->name);

		rq = blkdev_entry_next_request(&drive->queue.queue_head);
		hwgroup->rq = rq;
		/*
		 * Some systems have trouble with IDE IRQs arriving while
		 * the driver is still setting things up.  So, here we disable
		 * the IRQ used by this interface while the request is being started.
		 * This may look bad at first, but pretty much the same thing
		 * happens anyway when any interrupt comes in, IDE or otherwise
		 *  -- the kernel masks the IRQ while it is being handled.
		 */
		if (hwif->irq != masked_irq)
			disable_irq_nosync(hwif->irq);
		spin_unlock(&io_request_lock);
		local_irq_enable();
			/* allow other IRQs while we start this request */
		startstop = ide_start_request(drive, rq);
		spin_lock_irq(&io_request_lock);
		if (hwif->irq != masked_irq)
			enable_irq(hwif->irq);
		if (startstop == ide_stopped)
			hwgroup->busy = 0;
	}
}

EXPORT_SYMBOL(ide_do_request);

/*
 * ide_get_queue() returns the queue which corresponds to a given device.
 */
request_queue_t *ide_get_queue (kdev_t dev)
{
	ide_hwif_t *hwif = (ide_hwif_t *)blk_dev[MAJOR(dev)].data;

	return &hwif->drives[DEVICE_NR(dev) & 1].queue;
}

EXPORT_SYMBOL(ide_get_queue);

/*
 * Passes the stuff to ide_do_request
 */
void do_ide_request(request_queue_t *q)
{
	ide_do_request(q->queuedata, IDE_NO_IRQ);
}

EXPORT_SYMBOL(do_ide_request);

/*
 * un-busy the hwgroup etc, and clear any pending DMA status. we want to
 * retry the current request in pio mode instead of risking tossing it
 * all away
 */
static ide_startstop_t ide_dma_timeout_retry(ide_drive_t *drive, int error)
{
	ide_hwif_t *hwif = HWIF(drive);
	struct request *rq;
	ide_startstop_t ret = ide_stopped;

	/*
	 * end current dma transaction
	 */
	(void) hwif->ide_dma_end(drive);

	/*
	 * complain a little, later we might remove some of this verbosity
	 */

	if (error < 0) {
		printk(KERN_ERR "%s: error waiting for DMA\n", drive->name);
		(void)HWIF(drive)->ide_dma_end(drive);
		ret = DRIVER(drive)->error(drive, "dma timeout retry",
				hwif->INB(IDE_STATUS_REG));
	} else {
		printk(KERN_ERR "%s: timeout waiting for DMA\n", drive->name);
		(void) hwif->ide_dma_timeout(drive);
	}

	/*
	 * disable dma for now, but remember that we did so because of
	 * a timeout -- we'll reenable after we finish this next request
	 * (or rather the first chunk of it) in pio.
	 */
	drive->retry_pio++;
	drive->state = DMA_PIO_RETRY;
	(void) hwif->ide_dma_off_quietly(drive);

	/*
	 * un-busy drive etc (hwgroup->busy is cleared on return) and
	 * make sure request is sane
	 */
	rq = HWGROUP(drive)->rq;
	HWGROUP(drive)->rq = NULL;

	rq->errors = 0;
	rq->sector = rq->bh->b_rsector;
	rq->current_nr_sectors = rq->bh->b_size >> 9;
	rq->hard_cur_sectors = rq->current_nr_sectors;
	rq->buffer = rq->bh->b_data;

	return ret;
}

/**
 *	ide_timer_expiry	-	handle lack of an IDE interrupt
 *	@data: timer callback magic (hwgroup)
 *
 *	An IDE command has timed out before the expected drive return
 *	occurred. At this point we attempt to clean up the current
 *	mess. If the current handler includes an expiry handler then
 *	we invoke the expiry handler, and providing it is happy the
 *	work is done. If that fails we apply generic recovery rules
 *	invoking the handler and checking the drive DMA status. We
 *	have an excessively incestuous relationship with the DMA
 *	logic that wants cleaning up.
 */
 
void ide_timer_expiry (unsigned long data)
{
	ide_hwgroup_t	*hwgroup = (ide_hwgroup_t *) data;
	ide_handler_t	*handler;
	ide_expiry_t	*expiry;
 	unsigned long	flags;
	unsigned long	wait = -1;

	spin_lock_irqsave(&io_request_lock, flags);

	if ((handler = hwgroup->handler) == NULL) {
		/*
		 * Either a marginal timeout occurred
		 * (got the interrupt just as timer expired),
		 * or we were "sleeping" to give other devices a chance.
		 * Either way, we don't really want to complain about anything.
		 */
		if (hwgroup->sleeping) {
			hwgroup->sleeping = 0;
			hwgroup->busy = 0;
		}
	} else {
		ide_drive_t *drive = hwgroup->drive;
		if (!drive) {
			printk(KERN_ERR "ide_timer_expiry: hwgroup->drive was NULL\n");
			hwgroup->handler = NULL;
		} else {
			ide_hwif_t *hwif;
			ide_startstop_t startstop = ide_stopped;
			if (!hwgroup->busy) {
				hwgroup->busy = 1;	/* paranoia */
				printk(KERN_ERR "%s: ide_timer_expiry: hwgroup->busy was 0 ??\n", drive->name);
			}
			if ((expiry = hwgroup->expiry) != NULL) {
				/* continue */
				if ((wait = expiry(drive)) > 0) {
					/* reset timer */
					hwgroup->timer.expires  = jiffies + wait;
					add_timer(&hwgroup->timer);
					spin_unlock_irqrestore(&io_request_lock, flags);
					return;
				}
			}
			hwgroup->handler = NULL;
			/*
			 * We need to simulate a real interrupt when invoking
			 * the handler() function, which means we need to
			 * globally mask the specific IRQ:
			 */
			spin_unlock(&io_request_lock);
			hwif  = HWIF(drive);
#if DISABLE_IRQ_NOSYNC
			disable_irq_nosync(hwif->irq);
#else
			/* disable_irq_nosync ?? */
			disable_irq(hwif->irq);
#endif /* DISABLE_IRQ_NOSYNC */

			/* local CPU only,
			 * as if we were handling an interrupt */
			local_irq_disable();
			if (hwgroup->poll_timeout != 0) {
				startstop = handler(drive);
			} else if (drive_is_ready(drive)) {
				if (drive->waiting_for_dma)
					(void) hwgroup->hwif->ide_dma_lostirq(drive);
				(void)ide_ack_intr(hwif);
				printk(KERN_ERR "%s: lost interrupt\n", drive->name);
				startstop = handler(drive);
			} else {
				if (drive->waiting_for_dma) {
					startstop = ide_dma_timeout_retry(drive, wait);
				} else {
					startstop = DRIVER(drive)->error(drive, "irq timeout", hwif->INB(IDE_STATUS_REG));
				}
			}
			drive->service_time = jiffies - drive->service_start;
			spin_lock_irq(&io_request_lock);
			enable_irq(hwif->irq);
			if (startstop == ide_stopped)
				hwgroup->busy = 0;
		}
	}
	ide_do_request(hwgroup, IDE_NO_IRQ);
	spin_unlock_irqrestore(&io_request_lock, flags);
}

EXPORT_SYMBOL(ide_timer_expiry);

/**
 *	unexpected_intr		-	handle an unexpected IDE interrupt
 *	@irq: interrupt line
 *	@hwgroup: hwgroup being processed
 *
 *	There's nothing really useful we can do with an unexpected interrupt,
 *	other than reading the status register (to clear it), and logging it.
 *	There should be no way that an irq can happen before we're ready for it,
 *	so we needn't worry much about losing an "important" interrupt here.
 *
 *	On laptops (and "green" PCs), an unexpected interrupt occurs whenever
 *	the drive enters "idle", "standby", or "sleep" mode, so if the status
 *	looks "good", we just ignore the interrupt completely.
 *
 *	This routine assumes __cli() is in effect when called.
 *
 *	If an unexpected interrupt happens on irq15 while we are handling irq14
 *	and if the two interfaces are "serialized" (CMD640), then it looks like
 *	we could screw up by interfering with a new request being set up for 
 *	irq15.
 *
 *	In reality, this is a non-issue.  The new command is not sent unless 
 *	the drive is ready to accept one, in which case we know the drive is
 *	not trying to interrupt us.  And ide_set_handler() is always invoked
 *	before completing the issuance of any new drive command, so we will not
 *	be accidentally invoked as a result of any valid command completion
 *	interrupt.
 *
 *	Note that we must walk the entire hwgroup here. We know which hwif
 *	is doing the current command, but we don't know which hwif burped
 *	mysteriously.
 */
 
static void unexpected_intr (int irq, ide_hwgroup_t *hwgroup)
{
	u8 stat;
	ide_hwif_t *hwif = hwgroup->hwif;

	/*
	 * handle the unexpected interrupt
	 */
	do {
		if (hwif->irq == irq) {
			stat = hwif->INB(hwif->io_ports[IDE_STATUS_OFFSET]);
			if (!OK_STAT(stat, READY_STAT, BAD_STAT)) {
				/* Try to not flood the console with msgs */
				static unsigned long last_msgtime, count;
				++count;
				if (time_after(jiffies, last_msgtime + HZ)) {
					last_msgtime = jiffies;
					printk(KERN_ERR "%s%s: unexpected interrupt, "
						"status=0x%02x, count=%ld\n",
						hwif->name,
						(hwif->next==hwgroup->hwif) ? "" : "(?)", stat, count);
				}
			}
		}
	} while ((hwif = hwif->next) != hwgroup->hwif);
}

/**
 *	ide_intr	-	default IDE interrupt handler
 *	@irq: interrupt number
 *	@dev_id: hwif group
 *	@regs: unused weirdness from the kernel irq layer
 *
 *	This is the default IRQ handler for the IDE layer. You should
 *	not need to override it. If you do be aware it is subtle in
 *	places
 *
 *	hwgroup->hwif is the interface in the group currently performing
 *	a command. hwgroup->drive is the drive and hwgroup->handler is
 *	the IRQ handler to call. As we issue a command the handlers
 *	step through multiple states, reassigning the handler to the
 *	next step in the process. Unlike a smart SCSI controller IDE
 *	expects the main processor to sequence the various transfer
 *	stages. We also manage a poll timer to catch up with most
 *	timeout situations. There are still a few where the handlers
 *	don't ever decide to give up.
 *
 *	The handler eventually returns ide_stopped to indicate the
 *	request completed. At this point we issue the next request
 *	on the hwgroup and the process begins again.
 */
 
void ide_intr (int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;
	ide_hwgroup_t *hwgroup = (ide_hwgroup_t *)dev_id;
	ide_hwif_t *hwif;
	ide_drive_t *drive;
	ide_handler_t *handler;
	ide_startstop_t startstop;

	spin_lock_irqsave(&io_request_lock, flags);
	hwif = hwgroup->hwif;

	if (!ide_ack_intr(hwif)) {
		spin_unlock_irqrestore(&io_request_lock, flags);
		return;
	}

	if ((handler = hwgroup->handler) == NULL ||
	    hwgroup->poll_timeout != 0) {
		/*
		 * Not expecting an interrupt from this drive.
		 * That means this could be:
		 *	(1) an interrupt from another PCI device
		 *	sharing the same PCI INT# as us.
		 * or	(2) a drive just entered sleep or standby mode,
		 *	and is interrupting to let us know.
		 * or	(3) a spurious interrupt of unknown origin.
		 *
		 * For PCI, we cannot tell the difference,
		 * so in that case we just ignore it and hope it goes away.
		 */
#ifdef CONFIG_BLK_DEV_IDEPCI
		if (hwif->pci_dev && !hwif->pci_dev->vendor)
#endif	/* CONFIG_BLK_DEV_IDEPCI */
		{
			/*
			 * Probably not a shared PCI interrupt,
			 * so we can safely try to do something about it:
			 */
			unexpected_intr(irq, hwgroup);
#ifdef CONFIG_BLK_DEV_IDEPCI
		} else {
			/*
			 * Whack the status register, just in case
			 * we have a leftover pending IRQ.
			 */
			(void) hwif->INB(hwif->io_ports[IDE_STATUS_OFFSET]);
#endif /* CONFIG_BLK_DEV_IDEPCI */
		}
		spin_unlock_irqrestore(&io_request_lock, flags);
		return;
	}
	drive = hwgroup->drive;
	if (!drive) {
		/*
		 * This should NEVER happen, and there isn't much
		 * we could do about it here.
		 */
		spin_unlock_irqrestore(&io_request_lock, flags);
		return;
	}
	if (!drive_is_ready(drive)) {
		/*
		 * This happens regularly when we share a PCI IRQ with
		 * another device.  Unfortunately, it can also happen
		 * with some buggy drives that trigger the IRQ before
		 * their status register is up to date.  Hopefully we have
		 * enough advance overhead that the latter isn't a problem.
		 */
		spin_unlock_irqrestore(&io_request_lock, flags);
		return;
	}
	if (!hwgroup->busy) {
		hwgroup->busy = 1;	/* paranoia */
		printk(KERN_ERR "%s: ide_intr: hwgroup->busy was 0 ??\n", drive->name);
	}
	hwgroup->handler = NULL;
	del_timer(&hwgroup->timer);
	spin_unlock(&io_request_lock);

	if (drive->unmask)
		local_irq_enable();

	/* service this interrupt, may set handler for next interrupt */
	startstop = handler(drive);
	spin_lock_irq(&io_request_lock);

	/*
	 * Note that handler() may have set things up for another
	 * interrupt to occur soon, but it cannot happen until
	 * we exit from this routine, because it will be the
	 * same irq as is currently being serviced here, and Linux
	 * won't allow another of the same (on any CPU) until we return.
	 */
	drive->service_time = jiffies - drive->service_start;
	if (startstop == ide_stopped) {
		if (hwgroup->handler == NULL) {	/* paranoia */
			hwgroup->busy = 0;
			ide_do_request(hwgroup, hwif->irq);
		} else {
			printk(KERN_ERR "%s: ide_intr: huh? expected NULL handler "
				"on exit\n", drive->name);
		}
	}
	spin_unlock_irqrestore(&io_request_lock, flags);
}

EXPORT_SYMBOL(ide_intr);

/*
 * get_info_ptr() returns the (ide_drive_t *) for a given device number.
 * It returns NULL if the given device number does not match any present drives.
 */
ide_drive_t *ide_info_ptr (kdev_t i_rdev, int force)
{
	int		major = MAJOR(i_rdev);
	unsigned int	h;

	for (h = 0; h < MAX_HWIFS; ++h) {
		ide_hwif_t  *hwif = &ide_hwifs[h];
		if (hwif->present && major == hwif->major) {
			unsigned unit = DEVICE_NR(i_rdev);
			if (unit < MAX_DRIVES) {
				ide_drive_t *drive = &hwif->drives[unit];
				if (drive->present || force)
					return drive;
			}
			break;
		}
	}
	return NULL;
}

EXPORT_SYMBOL(ide_info_ptr);

/**
 *	ide_init_drive_cmd	-	initialize a drive command request
 *	@rq: request object
 *
 *	Initialize a request before we fill it in and send it down to
 *	ide_do_drive_cmd. Commands must be set up by this function. Right
 *	now it doesn't do a lot, but if that changes abusers will have a
 *	nasty suprise.
 */

void ide_init_drive_cmd (struct request *rq)
{
	memset(rq, 0, sizeof(*rq));
	rq->cmd = IDE_DRIVE_CMD;
}

EXPORT_SYMBOL(ide_init_drive_cmd);

/**
 *	ide_do_drive_cmd	-	issue IDE special command
 *	@drive: device to issue command
 *	@rq: request to issue
 *	@action: action for processing
 *
 *	This function issues a special IDE device request
 *	onto the request queue.
 *
 *	If action is ide_wait, then the rq is queued at the end of the
 *	request queue, and the function sleeps until it has been processed.
 *	This is for use when invoked from an ioctl handler.
 *
 *	If action is ide_preempt, then the rq is queued at the head of
 *	the request queue, displacing the currently-being-processed
 *	request and this function returns immediately without waiting
 *	for the new rq to be completed.  This is VERY DANGEROUS, and is
 *	intended for careful use by the ATAPI tape/cdrom driver code.
 *
 *	If action is ide_next, then the rq is queued immediately after
 *	the currently-being-processed-request (if any), and the function
 *	returns without waiting for the new rq to be completed.  As above,
 *	This is VERY DANGEROUS, and is intended for careful use by the
 *	ATAPI tape/cdrom driver code.
 *
 *	If action is ide_end, then the rq is queued at the end of the
 *	request queue, and the function returns immediately without waiting
 *	for the new rq to be completed. This is again intended for careful
 *	use by the ATAPI tape/cdrom driver code.
 */
 
int ide_do_drive_cmd (ide_drive_t *drive, struct request *rq, ide_action_t action)
{
	unsigned long flags;
	ide_hwgroup_t *hwgroup = HWGROUP(drive);
	unsigned int major = HWIF(drive)->major;
	request_queue_t *q = &drive->queue;
	struct list_head *queue_head = &q->queue_head;
	DECLARE_COMPLETION(wait);

#ifdef CONFIG_BLK_DEV_PDC4030
	if (HWIF(drive)->chipset == ide_pdc4030 && rq->buffer != NULL)
		return -ENOSYS;  /* special drive cmds not supported */
#endif
	rq->errors = 0;
	rq->rq_status = RQ_ACTIVE;
	rq->rq_dev = MKDEV(major,(drive->select.b.unit)<<PARTN_BITS);
	if (action == ide_wait)
		rq->waiting = &wait;
	spin_lock_irqsave(&io_request_lock, flags);
	if (blk_queue_empty(q) || action == ide_preempt) {
		if (action == ide_preempt)
			hwgroup->rq = NULL;
	} else {
		if (action == ide_wait || action == ide_end) {
			queue_head = queue_head->prev;
		} else
			queue_head = queue_head->next;
	}
	list_add(&rq->queue, queue_head);
	ide_do_request(hwgroup, IDE_NO_IRQ);
	spin_unlock_irqrestore(&io_request_lock, flags);
	if (action == ide_wait) {
		/* wait for it to be serviced */
		wait_for_completion(&wait);
		/* return -EIO if errors */
		return rq->errors ? -EIO : 0;
	}
	return 0;

}

EXPORT_SYMBOL(ide_do_drive_cmd);
