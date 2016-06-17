/*
 * linux/drivers/ide/ide-taskfile.c	Version 0.38	March 05, 2003
 *
 *  Copyright (C) 2000-2002	Michael Cornwell <cornwell@acm.org>
 *  Copyright (C) 2000-2002	Andre Hedrick <andre@linux-ide.org>
 *  Copyright (C) 2001-2002	Klaus Smolin
 *					IBM Storage Technology Division
 *
 *  The big the bad and the ugly.
 *
 *  Problems to be fixed because of BH interface or the lack therefore.
 *
 *  Fill me in stupid !!!
 *
 *  HOST:
 *	General refers to the Controller and Driver "pair".
 *  DATA HANDLER:
 *	Under the context of Linux it generally refers to an interrupt handler.
 *	However, it correctly describes the 'HOST'
 *  DATA BLOCK:
 *	The amount of data needed to be transfered as predefined in the
 *	setup of the device.
 *  STORAGE ATOMIC:
 *	The 'DATA BLOCK' associated to the 'DATA HANDLER', and can be as
 *	small as a single sector or as large as the entire command block
 *	request.
 */

#include <linux/config.h>
#define __NO_VERSION__
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
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/hdreg.h>
#include <linux/ide.h>

#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/bitops.h>

#define DEBUG_TASKFILE	0	/* unset when fixed */

#if DEBUG_TASKFILE
#define DTF(x...) printk(x)
#else
#define DTF(x...)
#endif

/*
 *
 */
#define task_rq_offset(rq) \
	(((rq)->nr_sectors - (rq)->current_nr_sectors) * SECTOR_SIZE)

/*
 * for now, taskfile requests are special :/
 *
 * However, upon the creation of the atapi version of packet_command
 * data-phase ISR plus it own diagnostics and extensions for direct access
 * (ioctl,read,write,rip,stream -- atapi), the kmap/kunmap for PIO will
 * come localized.
 */
inline char *task_map_rq (struct request *rq, unsigned long *flags)
{
	if (rq->bh)
		return ide_map_buffer(rq, flags);
	return rq->buffer + task_rq_offset(rq);
}

inline void task_unmap_rq (struct request *rq, char *buf, unsigned long *flags)
{
	if (rq->bh)
		ide_unmap_buffer(buf, flags);
}

inline u32 task_read_24 (ide_drive_t *drive)
{
	return	(HWIF(drive)->INB(IDE_HCYL_REG)<<16) |
		(HWIF(drive)->INB(IDE_LCYL_REG)<<8) |
		 HWIF(drive)->INB(IDE_SECTOR_REG);
}

EXPORT_SYMBOL(task_read_24);

static void ata_bswap_data (void *buffer, int wcount)
{
	u16 *p = buffer;

	while (wcount--) {
		*p = *p << 8 | *p >> 8; p++;
		*p = *p << 8 | *p >> 8; p++;
	}
}


void taskfile_input_data (ide_drive_t *drive, void *buffer, u32 wcount)
{
	HWIF(drive)->ata_input_data(drive, buffer, wcount);
	if (drive->bswap)
		ata_bswap_data(buffer, wcount);
}

EXPORT_SYMBOL(taskfile_input_data);

void taskfile_output_data (ide_drive_t *drive, void *buffer, u32 wcount)
{
	if (drive->bswap) {
		ata_bswap_data(buffer, wcount);
		HWIF(drive)->ata_output_data(drive, buffer, wcount);
		ata_bswap_data(buffer, wcount);
	} else {
		HWIF(drive)->ata_output_data(drive, buffer, wcount);
	}
}

EXPORT_SYMBOL(taskfile_output_data);

int taskfile_lib_get_identify (ide_drive_t *drive, u8 *buf)
{
	ide_task_t args;
	memset(&args, 0, sizeof(ide_task_t));
	args.tfRegister[IDE_NSECTOR_OFFSET]	= 0x01;
	if (drive->media == ide_disk)
		args.tfRegister[IDE_COMMAND_OFFSET]	= WIN_IDENTIFY;
	else
		args.tfRegister[IDE_COMMAND_OFFSET]	= WIN_PIDENTIFY;
	args.command_type			= ide_cmd_type_parser(&args);
	return ide_raw_taskfile(drive, &args, buf);
}

EXPORT_SYMBOL(taskfile_lib_get_identify);

#ifdef CONFIG_IDE_TASK_IOCTL_DEBUG
void debug_taskfile (ide_drive_t *drive, ide_task_t *args)
{
	printk(KERN_INFO "%s: ", drive->name);
//	printk("TF.0=x%02x ", args->tfRegister[IDE_DATA_OFFSET]);
	printk("TF.1=x%02x ", args->tfRegister[IDE_FEATURE_OFFSET]);
	printk("TF.2=x%02x ", args->tfRegister[IDE_NSECTOR_OFFSET]);
	printk("TF.3=x%02x ", args->tfRegister[IDE_SECTOR_OFFSET]);
	printk("TF.4=x%02x ", args->tfRegister[IDE_LCYL_OFFSET]);
	printk("TF.5=x%02x ", args->tfRegister[IDE_HCYL_OFFSET]);
	printk("TF.6=x%02x ", args->tfRegister[IDE_SELECT_OFFSET]);
	printk("TF.7=x%02x\n", args->tfRegister[IDE_COMMAND_OFFSET]);
	printk(KERN_INFO "%s: ", drive->name);
//	printk("HTF.0=x%02x ", args->hobRegister[IDE_DATA_OFFSET_HOB]);
	printk("HTF.1=x%02x ", args->hobRegister[IDE_FEATURE_OFFSET_HOB]);
	printk("HTF.2=x%02x ", args->hobRegister[IDE_NSECTOR_OFFSET_HOB]);
	printk("HTF.3=x%02x ", args->hobRegister[IDE_SECTOR_OFFSET_HOB]);
	printk("HTF.4=x%02x ", args->hobRegister[IDE_LCYL_OFFSET_HOB]);
	printk("HTF.5=x%02x ", args->hobRegister[IDE_HCYL_OFFSET_HOB]);
	printk("HTF.6=x%02x ", args->hobRegister[IDE_SELECT_OFFSET_HOB]);
	printk("HTF.7=x%02x\n", args->hobRegister[IDE_CONTROL_OFFSET_HOB]);
}
#endif /* CONFIG_IDE_TASK_IOCTL_DEBUG */

ide_startstop_t do_rw_taskfile (ide_drive_t *drive, ide_task_t *task)
{
	ide_hwif_t *hwif	= HWIF(drive);
	task_struct_t *taskfile	= (task_struct_t *) task->tfRegister;
	hob_struct_t *hobfile	= (hob_struct_t *) task->hobRegister;
	u8 HIHI			= (drive->addressing == 1) ? 0xE0 : 0xEF;

#ifdef CONFIG_IDE_TASK_IOCTL_DEBUG
	void debug_taskfile(drive, task);
#endif /* CONFIG_IDE_TASK_IOCTL_DEBUG */

	/* ALL Command Block Executions SHALL clear nIEN, unless otherwise */
	if (IDE_CONTROL_REG) {
		/* clear nIEN */
		hwif->OUTB(drive->ctl, IDE_CONTROL_REG);
	}
	SELECT_MASK(drive, 0);

	if (drive->addressing == 1) {
		hwif->OUTB(hobfile->feature, IDE_FEATURE_REG);
		hwif->OUTB(hobfile->sector_count, IDE_NSECTOR_REG);
		hwif->OUTB(hobfile->sector_number, IDE_SECTOR_REG);
		hwif->OUTB(hobfile->low_cylinder, IDE_LCYL_REG);
		hwif->OUTB(hobfile->high_cylinder, IDE_HCYL_REG);
	}

	hwif->OUTB(taskfile->feature, IDE_FEATURE_REG);
	hwif->OUTB(taskfile->sector_count, IDE_NSECTOR_REG);
	hwif->OUTB(taskfile->sector_number, IDE_SECTOR_REG);
	hwif->OUTB(taskfile->low_cylinder, IDE_LCYL_REG);
	hwif->OUTB(taskfile->high_cylinder, IDE_HCYL_REG);

	hwif->OUTB((taskfile->device_head & HIHI) | drive->select.all, IDE_SELECT_REG);
	if (task->handler != NULL) {
		ide_execute_command(drive, taskfile->command, task->handler, WAIT_WORSTCASE, NULL);
		if (task->prehandler != NULL)
			return task->prehandler(drive, task->rq);
		return ide_started;
	}
	/* for dma commands we down set the handler */
#if 0
	if (blk_fs_request(task->rq) && drive->using_dma) {
		if (rq_data_dir(task->rq) == READ) {
			if (hwif->ide_dma_read(drive))
				return ide_stopped;
		} else {
			if (hwif->ide_dma_write(drive))
				return ide_stopped;
		}
	} else {
		if (!drive->using_dma && (task->handler == NULL))
			return ide_stopped;

		switch(taskfile->command) {
			case WIN_WRITEDMA_ONCE:
			case WIN_WRITEDMA:
			case WIN_WRITEDMA_EXT:
				hwif->ide_dma_write(drive);
				break;
			case WIN_READDMA_ONCE:
			case WIN_READDMA:
			case WIN_READDMA_EXT:
			case WIN_IDENTIFY_DMA:
				hwif->ide_dma_read(drive);
				break;
			default:
				if (task->handler == NULL)
					return ide_stopped;
		}
	}
	return ide_started;
#else
	switch(taskfile->command) {
		case WIN_WRITEDMA_ONCE:
		case WIN_WRITEDMA:
		case WIN_WRITEDMA_EXT:
			if (drive->using_dma && !(hwif->ide_dma_write(drive)))
				return ide_started;
		case WIN_READDMA_ONCE:
		case WIN_READDMA:
		case WIN_READDMA_EXT:
		case WIN_IDENTIFY_DMA:
			if (drive->using_dma && !(hwif->ide_dma_read(drive)))
				return ide_started;
		default:
			break;
	}
	return ide_stopped;
#endif
}

EXPORT_SYMBOL(do_rw_taskfile);

/*
 * Error reporting, in human readable form (luxurious, but a memory hog).
 */
u8 taskfile_dump_status (ide_drive_t *drive, const char *msg, u8 stat)
{
	ide_hwif_t *hwif = HWIF(drive);
	unsigned long flags;
	u8 err = 0;

	local_irq_set(flags);
	printk("%s: %s: status=0x%02x", drive->name, msg, stat);
#if FANCY_STATUS_DUMPS
	printk(" { ");
	if (stat & BUSY_STAT) {
		printk("Busy ");
	} else {
		if (stat & READY_STAT)	printk("DriveReady ");
		if (stat & WRERR_STAT)	printk("DeviceFault ");
		if (stat & SEEK_STAT)	printk("SeekComplete ");
		if (stat & DRQ_STAT)	printk("DataRequest ");
		if (stat & ECC_STAT)	printk("CorrectedError ");
		if (stat & INDEX_STAT)	printk("Index ");
		if (stat & ERR_STAT)	printk("Error ");
	}
	printk("}");
#endif  /* FANCY_STATUS_DUMPS */
	printk("\n");
	if ((stat & (BUSY_STAT|ERR_STAT)) == ERR_STAT) {
		err = hwif->INB(IDE_ERROR_REG);
		printk("%s: %s: error=0x%02x", drive->name, msg, err);
#if FANCY_STATUS_DUMPS
		if (drive->media == ide_disk)
			goto media_out;

		printk(" { ");
		if (err & ABRT_ERR)	printk("DriveStatusError ");
		if (err & ICRC_ERR)	printk("Bad%s", (err & ABRT_ERR) ? "CRC " : "Sector ");
		if (err & ECC_ERR)	printk("UncorrectableError ");
		if (err & ID_ERR)	printk("SectorIdNotFound ");
		if (err & TRK0_ERR)	printk("TrackZeroNotFound ");
		if (err & MARK_ERR)	printk("AddrMarkNotFound ");
		printk("}");
		if ((err & (BBD_ERR | ABRT_ERR)) == BBD_ERR ||
		    (err & (ECC_ERR|ID_ERR|MARK_ERR))) {
			if (drive->addressing == 1) {
				u64 sectors = 0;
				u32 high = 0;
				u32 low = task_read_24(drive);
				hwif->OUTB(0x80, IDE_CONTROL_REG);
				high = task_read_24(drive);
				sectors = ((u64)high << 24) | low;
				printk(", LBAsect=%lld", sectors);
			} else {
				u8 cur  = hwif->INB(IDE_SELECT_REG);
				u8 low  = hwif->INB(IDE_LCYL_REG);
				u8 high = hwif->INB(IDE_HCYL_REG);
				u8 sect = hwif->INB(IDE_SECTOR_REG);
				/* using LBA? */
				if (cur & 0x40) {
					printk(", LBAsect=%d", (u32)
						((cur&0xf)<<24)|(high<<16)|
						(low<<8)|sect);
				} else {
					printk(", CHS=%d/%d/%d",
						((high<<8) + low),
						(cur & 0xf), sect);
				}
			}
			if (HWGROUP(drive)->rq)
				printk(", sector=%lu",
					HWGROUP(drive)->rq->sector);
		}
media_out:
#endif  /* FANCY_STATUS_DUMPS */
		printk("\n");
	}
	local_irq_restore(flags);
	return err;
}

EXPORT_SYMBOL(taskfile_dump_status);

/*
 * Clean up after success/failure of an explicit taskfile operation.
 */
void ide_end_taskfile (ide_drive_t *drive, u8 stat, u8 err)
{
	ide_hwif_t *hwif = HWIF(drive);
	unsigned long flags;
	struct request *rq;
	ide_task_t *args;
	task_ioreg_t command;

	spin_lock_irqsave(&io_request_lock, flags);
	rq = HWGROUP(drive)->rq;
	spin_unlock_irqrestore(&io_request_lock, flags);
	args = (ide_task_t *) rq->special;

	command = args->tfRegister[IDE_COMMAND_OFFSET];

	if (rq->errors == 0)
		rq->errors = !OK_STAT(stat,READY_STAT,BAD_STAT);

	if (args->tf_in_flags.b.data) {
		u16 data = hwif->INW(IDE_DATA_REG);
		args->tfRegister[IDE_DATA_OFFSET] = (data) & 0xFF;
		args->hobRegister[IDE_DATA_OFFSET_HOB]	= (data >> 8) & 0xFF;
	}
	args->tfRegister[IDE_ERROR_OFFSET]   = err;
	args->tfRegister[IDE_NSECTOR_OFFSET] = hwif->INB(IDE_NSECTOR_REG);
	args->tfRegister[IDE_SECTOR_OFFSET]  = hwif->INB(IDE_SECTOR_REG);
	args->tfRegister[IDE_LCYL_OFFSET]    = hwif->INB(IDE_LCYL_REG);
	args->tfRegister[IDE_HCYL_OFFSET]    = hwif->INB(IDE_HCYL_REG);
	args->tfRegister[IDE_SELECT_OFFSET]  = hwif->INB(IDE_SELECT_REG);
	args->tfRegister[IDE_STATUS_OFFSET]  = stat;
	if ((drive->id->command_set_2 & 0x0400) &&
	    (drive->id->cfs_enable_2 & 0x0400) &&
	    (drive->addressing == 1)) {
		hwif->OUTB(drive->ctl|0x80, IDE_CONTROL_REG_HOB);
		args->hobRegister[IDE_FEATURE_OFFSET_HOB] = hwif->INB(IDE_FEATURE_REG);
		args->hobRegister[IDE_NSECTOR_OFFSET_HOB] = hwif->INB(IDE_NSECTOR_REG);
		args->hobRegister[IDE_SECTOR_OFFSET_HOB]  = hwif->INB(IDE_SECTOR_REG);
		args->hobRegister[IDE_LCYL_OFFSET_HOB]    = hwif->INB(IDE_LCYL_REG);
		args->hobRegister[IDE_HCYL_OFFSET_HOB]    = hwif->INB(IDE_HCYL_REG);
	}

#if 0
/*	taskfile_settings_update(drive, args, command); */

	if (args->posthandler != NULL)
		args->posthandler(drive, args);
#endif

	spin_lock_irqsave(&io_request_lock, flags);
	blkdev_dequeue_request(rq);
	HWGROUP(drive)->rq = NULL;
	end_that_request_last(rq);
	spin_unlock_irqrestore(&io_request_lock, flags);
}

EXPORT_SYMBOL(ide_end_taskfile);

/*
 * try_to_flush_leftover_data() is invoked in response to a drive
 * unexpectedly having its DRQ_STAT bit set.  As an alternative to
 * resetting the drive, this routine tries to clear the condition
 * by read a sector's worth of data from the drive.  Of course,
 * this may not help if the drive is *waiting* for data from *us*.
 */
void task_try_to_flush_leftover_data (ide_drive_t *drive)
{
	int i = (drive->mult_count ? drive->mult_count : 1) * SECTOR_WORDS;

	if (drive->media != ide_disk)
		return;
	while (i > 0) {
		u32 buffer[16];
		unsigned int wcount = (i > 16) ? 16 : i;
		i -= wcount;
		taskfile_input_data(drive, buffer, wcount);
	}
}

EXPORT_SYMBOL(task_try_to_flush_leftover_data);

/*
 * taskfile_error() takes action based on the error returned by the drive.
 */
ide_startstop_t taskfile_error (ide_drive_t *drive, const char *msg, u8 stat)
{
	ide_hwif_t *hwif;
	struct request *rq;
	u8 err;

        err = taskfile_dump_status(drive, msg, stat);
	if (drive == NULL || (rq = HWGROUP(drive)->rq) == NULL)
		return ide_stopped;

	hwif = HWIF(drive);
	/* retry only "normal" I/O: */
	if (rq->cmd == IDE_DRIVE_TASKFILE) {
		rq->errors = 1;
		ide_end_taskfile(drive, stat, err);
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
				/* UDMA crc error -- just retry the operation */
				drive->crc_count++;
			} else if (err & (BBD_ERR | ECC_ERR)) {
				/* retries won't help these */
				rq->errors = ERROR_MAX;
			} else if (err & TRK0_ERR) {
				/* help it find track zero */
				rq->errors |= ERROR_RECAL;
			}
                }
media_out:
                if ((stat & DRQ_STAT) && rq_data_dir(rq) != WRITE)
                        task_try_to_flush_leftover_data(drive);
	}
	if (hwif->INB(IDE_STATUS_REG) & (BUSY_STAT|DRQ_STAT)) {
		/* force an abort */
		hwif->OUTB(WIN_IDLEIMMEDIATE, IDE_COMMAND_REG);
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

EXPORT_SYMBOL(taskfile_error);

/*
 * set_multmode_intr() is invoked on completion of a WIN_SETMULT cmd.
 */
ide_startstop_t set_multmode_intr (ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);
	u8 stat;

	if (OK_STAT(stat = hwif->INB(IDE_STATUS_REG),READY_STAT,BAD_STAT)) {
		drive->mult_count = drive->mult_req;
	} else {
		drive->mult_req = drive->mult_count = 0;
		drive->special.b.recalibrate = 1;
		(void) ide_dump_status(drive, "set_multmode", stat);
	}
	return ide_stopped;
}

EXPORT_SYMBOL(set_multmode_intr);

/*
 * set_geometry_intr() is invoked on completion of a WIN_SPECIFY cmd.
 */
ide_startstop_t set_geometry_intr (ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);
	int retries = 5;
	u8 stat;

	while (((stat = hwif->INB(IDE_STATUS_REG)) & BUSY_STAT) && retries--)
		udelay(10);

	if (OK_STAT(stat, READY_STAT, BAD_STAT))
		return ide_stopped;

	if (stat & (ERR_STAT|DRQ_STAT))
		return DRIVER(drive)->error(drive, "set_geometry_intr", stat);

	if (HWGROUP(drive)->handler != NULL)
		BUG();
	ide_set_handler(drive, &set_geometry_intr, WAIT_WORSTCASE, NULL);
	return ide_started;
}

EXPORT_SYMBOL(set_geometry_intr);

/*
 * recal_intr() is invoked on completion of a WIN_RESTORE (recalibrate) cmd.
 */
ide_startstop_t recal_intr (ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);
	u8 stat;

	if (!OK_STAT(stat = hwif->INB(IDE_STATUS_REG), READY_STAT, BAD_STAT))
		return DRIVER(drive)->error(drive, "recal_intr", stat);
	return ide_stopped;
}

EXPORT_SYMBOL(recal_intr);

/*
 * Handler for commands without a data phase
 */
ide_startstop_t task_no_data_intr (ide_drive_t *drive)
{
	ide_task_t *args	= HWGROUP(drive)->rq->special;
	ide_hwif_t *hwif	= HWIF(drive);
	u8 stat;

	local_irq_enable();
	if (!OK_STAT(stat = hwif->INB(IDE_STATUS_REG),READY_STAT,BAD_STAT)) {
		DTF("%s: command opcode 0x%02x\n", drive->name,
			args->tfRegister[IDE_COMMAND_OFFSET]);
		return DRIVER(drive)->error(drive, "task_no_data_intr", stat);
		/* calls ide_end_drive_cmd */
	}
	if (args)
		ide_end_drive_cmd(drive, stat, hwif->INB(IDE_ERROR_REG));

	return ide_stopped;
}

EXPORT_SYMBOL(task_no_data_intr);

/*
 * Handler for command with PIO data-in phase, READ
 */
/*
 * FIXME before 2.4 enable ...
 *	DATA integrity issue upon error. <andre@linux-ide.org>
 */
ide_startstop_t task_in_intr (ide_drive_t *drive)
{
	struct request *rq	= HWGROUP(drive)->rq;
	ide_hwif_t *hwif	= HWIF(drive);
	char *pBuf		= NULL;
	u8 stat;
	unsigned long flags;

	if (!OK_STAT(stat = hwif->INB(IDE_STATUS_REG),DATA_READY,BAD_R_STAT)) {
		if (stat & (ERR_STAT|DRQ_STAT)) {
#if 0
			DTF("%s: attempting to recover last " \
				"sector counter status=0x%02x\n",
				drive->name, stat);
			/*
			 * Expect a BUG BOMB if we attempt to rewind the
			 * offset in the BH aka PAGE in the current BLOCK
			 * segment.  This is different than the HOST segment.
			 */
#endif
			if (!rq->bh)
				rq->current_nr_sectors++;
			return DRIVER(drive)->error(drive, "task_in_intr", stat);
		}
		if (!(stat & BUSY_STAT)) {
			DTF("task_in_intr to Soon wait for next interrupt\n");
			if (HWGROUP(drive)->handler == NULL)
				ide_set_handler(drive, &task_in_intr, WAIT_WORSTCASE, NULL);
			return ide_started;  
		}
	}
#if 0

	/*
	 * Holding point for a brain dump of a thought :-/
	 */

	if (!OK_STAT(stat,DRIVE_READY,drive->bad_wstat)) {
		DTF("%s: READ attempting to recover last " \
			"sector counter status=0x%02x\n",
			drive->name, stat);
		rq->current_nr_sectors++;
		return DRIVER(drive)->error(drive, "task_in_intr", stat);
        }
	if (!rq->current_nr_sectors)
		if (!DRIVER(drive)->end_request(drive, 1))
			return ide_stopped;

	if (--rq->current_nr_sectors <= 0)
		if (!DRIVER(drive)->end_request(drive, 1))
			return ide_stopped;
#endif

	pBuf = task_map_rq(rq, &flags);
	DTF("Read: %p, rq->current_nr_sectors: %d, stat: %02x\n",
		pBuf, (int) rq->current_nr_sectors, stat);
	taskfile_input_data(drive, pBuf, SECTOR_WORDS);
	task_unmap_rq(rq, pBuf, &flags);
	/*
	 * FIXME :: We really can not legally get a new page/bh
	 * regardless, if this is the end of our segment.
	 * BH walking or segment can only be updated after we have a good
	 * hwif->INB(IDE_STATUS_REG); return.
	 */
	if (--rq->current_nr_sectors <= 0)
		if (!DRIVER(drive)->end_request(drive, 1))
			return ide_stopped;
	/*
	 * ERM, it is techincally legal to leave/exit here but it makes
	 * a mess of the code ...
	 */
	if (HWGROUP(drive)->handler == NULL)
		ide_set_handler(drive, &task_in_intr, WAIT_WORSTCASE, NULL);
	return ide_started;
}

EXPORT_SYMBOL(task_in_intr);

/*
 * Handler for command with Read Multiple
 */
ide_startstop_t task_mulin_intr (ide_drive_t *drive)
{
	ide_hwif_t *hwif	= HWIF(drive);
	struct request *rq	= HWGROUP(drive)->rq;
	char *pBuf		= NULL;
	unsigned int msect	= drive->mult_count;
	unsigned int nsect;
	unsigned long flags;
	u8 stat;

	if (!OK_STAT(stat = hwif->INB(IDE_STATUS_REG),DATA_READY,BAD_R_STAT)) {
		if (stat & (ERR_STAT|DRQ_STAT)) {
			if (!rq->bh) {
				rq->current_nr_sectors += drive->mult_count;
				/*
				 * NOTE: could rewind beyond beginning :-/
				 */
			} else {
				printk("%s: MULTI-READ assume all data " \
					"transfered is bad status=0x%02x\n",
					drive->name, stat);
			}
			return DRIVER(drive)->error(drive, "task_mulin_intr", stat);
		}
		/* no data yet, so wait for another interrupt */
		if (HWGROUP(drive)->handler == NULL)
			ide_set_handler(drive, &task_mulin_intr, WAIT_WORSTCASE, NULL);
		return ide_started;
	}

	do {
		nsect = rq->current_nr_sectors;
		if (nsect > msect)
			nsect = msect;
		pBuf = task_map_rq(rq, &flags);
		DTF("Multiread: %p, nsect: %d, msect: %d, " \
			" rq->current_nr_sectors: %d\n",
			pBuf, nsect, msect, rq->current_nr_sectors);
		taskfile_input_data(drive, pBuf, nsect * SECTOR_WORDS);
		task_unmap_rq(rq, pBuf, &flags);
		rq->errors = 0;
		rq->current_nr_sectors -= nsect;
		msect -= nsect;
		/*
		 * FIXME :: We really can not legally get a new page/bh
		 * regardless, if this is the end of our segment.
		 * BH walking or segment can only be updated after we have a
		 * good hwif->INB(IDE_STATUS_REG); return.
		 */
		if (!rq->current_nr_sectors) {
			if (!DRIVER(drive)->end_request(drive, 1))
				return ide_stopped;
		}
	} while (msect);
	if (HWGROUP(drive)->handler == NULL)
		ide_set_handler(drive, &task_mulin_intr, WAIT_WORSTCASE, NULL);
	return ide_started;
}

EXPORT_SYMBOL(task_mulin_intr);

/*
 * VERIFY ME before 2.4 ... unexpected race is possible based on details
 * RMK with 74LS245/373/374 TTL buffer logic because of passthrough.
 */
ide_startstop_t pre_task_out_intr (ide_drive_t *drive, struct request *rq)
{
	char *pBuf		= NULL;
	unsigned long flags;
	ide_startstop_t startstop;

	if (ide_wait_stat(&startstop, drive, DATA_READY,
			drive->bad_wstat, WAIT_DRQ)) {
		printk(KERN_ERR "%s: no DRQ after issuing WRITE%s\n",
			drive->name,
			drive->addressing ? "_EXT" : "");
		return startstop;
	}
	/* For Write_sectors we need to stuff the first sector */
	pBuf = task_map_rq(rq, &flags);
	taskfile_output_data(drive, pBuf, SECTOR_WORDS);
	rq->current_nr_sectors--;
	task_unmap_rq(rq, pBuf, &flags);
	return ide_started;
}

EXPORT_SYMBOL(pre_task_out_intr);

/*
 * Handler for command with PIO data-out phase WRITE
 *
 * WOOHOO this is a CORRECT STATE DIAGRAM NOW, <andre@linux-ide.org>
 */
ide_startstop_t task_out_intr (ide_drive_t *drive)
{
	ide_hwif_t *hwif	= HWIF(drive);
	struct request *rq	= HWGROUP(drive)->rq;
	char *pBuf		= NULL;
	unsigned long flags;
	u8 stat;

	if (!OK_STAT(stat = hwif->INB(IDE_STATUS_REG), DRIVE_READY, drive->bad_wstat)) {
		DTF("%s: WRITE attempting to recover last " \
			"sector counter status=0x%02x\n",
			drive->name, stat);
		rq->current_nr_sectors++;
		return DRIVER(drive)->error(drive, "task_out_intr", stat);
	}
	/*
	 * Safe to update request for partial completions.
	 * We have a good STATUS CHECK!!!
	 */
	if (!rq->current_nr_sectors)
		if (!DRIVER(drive)->end_request(drive, 1))
			return ide_stopped;
	if ((rq->current_nr_sectors==1) ^ (stat & DRQ_STAT)) {
		rq = HWGROUP(drive)->rq;
		pBuf = task_map_rq(rq, &flags);
		DTF("write: %p, rq->current_nr_sectors: %d\n",
			pBuf, (int) rq->current_nr_sectors);
		taskfile_output_data(drive, pBuf, SECTOR_WORDS);
		task_unmap_rq(rq, pBuf, &flags);
		rq->errors = 0;
		rq->current_nr_sectors--;
	}
	if (HWGROUP(drive)->handler == NULL)
		ide_set_handler(drive, &task_out_intr, WAIT_WORSTCASE, NULL);
	return ide_started;
}

EXPORT_SYMBOL(task_out_intr);

#undef ALTERNATE_STATE_DIAGRAM_MULTI_OUT

ide_startstop_t pre_task_mulout_intr (ide_drive_t *drive, struct request *rq)
{
#ifdef ALTERNATE_STATE_DIAGRAM_MULTI_OUT
	ide_hwif_t *hwif		= HWIF(drive);
	char *pBuf			= NULL;
	unsigned int nsect = 0, msect	= drive->mult_count;
        u8 stat;
	unsigned long flags;
#endif /* ALTERNATE_STATE_DIAGRAM_MULTI_OUT */

	ide_task_t *args = rq->special;
	ide_startstop_t startstop;

#if 0
	/*
	 * assign private copy for multi-write
	 */
	memcpy(&HWGROUP(drive)->wrq, rq, sizeof(struct request));
#endif

	if (ide_wait_stat(&startstop, drive, DATA_READY,
			drive->bad_wstat, WAIT_DRQ)) {
		printk(KERN_ERR "%s: no DRQ after issuing %s\n",
			drive->name,
			drive->addressing ? "MULTWRITE_EXT" : "MULTWRITE");
		return startstop;
	}
#ifdef ALTERNATE_STATE_DIAGRAM_MULTI_OUT

	do {
		nsect = rq->current_nr_sectors;
		if (nsect > msect)
			nsect = msect;
		pBuf = task_map_rq(rq, &flags);
		DTF("Pre-Multiwrite: %p, nsect: %d, msect: %d, " \
			"rq->current_nr_sectors: %ld\n",
			pBuf, nsect, msect, rq->current_nr_sectors);
		msect -= nsect;
		taskfile_output_data(drive, pBuf, nsect * SECTOR_WORDS);
		task_unmap_rq(rq, pBuf, &flags);
		rq->current_nr_sectors -= nsect;
		if (!rq->current_nr_sectors) {
			if (!DRIVER(drive)->end_request(drive, 1))
				if (!rq->bh) {
					stat = hwif->INB(IDE_STATUS_REG);
					return ide_stopped;
				}
		}
	} while (msect);
	rq->errors = 0;
	return ide_started;
#else /* ! ALTERNATE_STATE_DIAGRAM_MULTI_OUT */
	if (!(drive_is_ready(drive))) {
		int i;
		for (i=0; i<100; i++) {
			if (drive_is_ready(drive))
				break;
		}
	}

	/*
	 * WARNING :: if the drive as not acked good status we may not
	 * move the DATA-TRANSFER T-Bar as BSY != 0. <andre@linux-ide.org>
	 */
	return args->handler(drive);
#endif /* ALTERNATE_STATE_DIAGRAM_MULTI_OUT */
}

EXPORT_SYMBOL(pre_task_mulout_intr);

/*
 * FIXME before enabling in 2.4 ... DATA integrity issue upon error.
 */
/*
 * Handler for command write multiple
 * Called directly from execute_drive_cmd for the first bunch of sectors,
 * afterwards only by the ISR
 */
ide_startstop_t task_mulout_intr (ide_drive_t *drive)
{
	ide_hwif_t *hwif		= HWIF(drive);
	u8 stat				= hwif->INB(IDE_STATUS_REG);
	struct request *rq		= HWGROUP(drive)->rq;
	char *pBuf			= NULL;
	ide_startstop_t startstop	= ide_stopped;
	unsigned int msect		= drive->mult_count;
	unsigned int nsect;
	unsigned long flags;

	/*
	 * (ks/hs): Handle last IRQ on multi-sector transfer,
	 * occurs after all data was sent in this chunk
	 */
	if (rq->current_nr_sectors == 0) {
		if (stat & (ERR_STAT|DRQ_STAT)) {
			if (!rq->bh) {
                                rq->current_nr_sectors += drive->mult_count;
				/*
				 * NOTE: could rewind beyond beginning :-/
				 */
			} else {
				printk(KERN_ERR "%s: MULTI-WRITE assume all data " \
					"transfered is bad status=0x%02x\n",
					drive->name, stat);
			}
			return DRIVER(drive)->error(drive, "task_mulout_intr", stat);
		}
		if (!rq->bh)
			DRIVER(drive)->end_request(drive, 1);
		return startstop;
	}
	/*
	 * DON'T be lazy code the above and below togather !!!
	 */
	if (!OK_STAT(stat,DATA_READY,BAD_R_STAT)) {
		if (stat & (ERR_STAT|DRQ_STAT)) {
			if (!rq->bh) {
				rq->current_nr_sectors += drive->mult_count;
				/*
				 * NOTE: could rewind beyond beginning :-/
				 */
			} else {
				printk(KERN_ERR "%s: MULTI-WRITE assume all data " \
					"transfered is bad status=0x%02x\n",
					drive->name, stat);
			}
			return DRIVER(drive)->error(drive, "task_mulout_intr", stat);
		}
		/* no data yet, so wait for another interrupt */
		if (HWGROUP(drive)->handler == NULL)
			ide_set_handler(drive, &task_mulout_intr, WAIT_WORSTCASE, NULL);
		return ide_started;
	}

#ifndef ALTERNATE_STATE_DIAGRAM_MULTI_OUT
	if (HWGROUP(drive)->handler != NULL) {
		unsigned long lflags;
		spin_lock_irqsave(&io_request_lock, lflags);
		HWGROUP(drive)->handler = NULL;
		del_timer(&HWGROUP(drive)->timer);
		spin_unlock_irqrestore(&io_request_lock, lflags);
	}
#endif /* ALTERNATE_STATE_DIAGRAM_MULTI_OUT */

	do {
		nsect = rq->current_nr_sectors;
		if (nsect > msect)
			nsect = msect;
		pBuf = task_map_rq(rq, &flags);
		DTF("Multiwrite: %p, nsect: %d, msect: %d, " \
			"rq->current_nr_sectors: %ld\n",
			pBuf, nsect, msect, rq->current_nr_sectors);
		msect -= nsect;
		taskfile_output_data(drive, pBuf, nsect * SECTOR_WORDS);
		task_unmap_rq(rq, pBuf, &flags);
		rq->current_nr_sectors -= nsect;
		/*
		 * FIXME :: We really can not legally get a new page/bh
		 * regardless, if this is the end of our segment.
		 * BH walking or segment can only be updated after we
		 * have a good  hwif->INB(IDE_STATUS_REG); return.
		 */
		if (!rq->current_nr_sectors) {
			if (!DRIVER(drive)->end_request(drive, 1))
				if (!rq->bh)
					return ide_stopped;
		}
	} while (msect);
	rq->errors = 0;
	if (HWGROUP(drive)->handler == NULL)
		ide_set_handler(drive, &task_mulout_intr, WAIT_WORSTCASE, NULL);
	return ide_started;
}

EXPORT_SYMBOL(task_mulout_intr);

/* Called by internal to feature out type of command being called */
//ide_pre_handler_t * ide_pre_handler_parser (task_struct_t *taskfile, hob_struct_t *hobfile)
ide_pre_handler_t * ide_pre_handler_parser (struct hd_drive_task_hdr *taskfile, struct hd_drive_hob_hdr *hobfile)
{
	switch(taskfile->command) {
				/* IDE_DRIVE_TASK_RAW_WRITE */
		case CFA_WRITE_MULTI_WO_ERASE:
	//	case WIN_WRITE_LONG:
	//	case WIN_WRITE_LONG_ONCE:
		case WIN_MULTWRITE:
		case WIN_MULTWRITE_EXT:
			return &pre_task_mulout_intr;
			
				/* IDE_DRIVE_TASK_OUT */
		case WIN_WRITE:
	//	case WIN_WRITE_ONCE:
		case WIN_WRITE_EXT:
		case WIN_WRITE_VERIFY:
		case WIN_WRITE_BUFFER:
		case CFA_WRITE_SECT_WO_ERASE:
		case WIN_DOWNLOAD_MICROCODE:
			return &pre_task_out_intr;
				/* IDE_DRIVE_TASK_OUT */
		case WIN_SMART:
			if (taskfile->feature == SMART_WRITE_LOG_SECTOR)
				return &pre_task_out_intr;
		case WIN_WRITEDMA:
	//	case WIN_WRITEDMA_ONCE:
		case WIN_WRITEDMA_QUEUED:
		case WIN_WRITEDMA_EXT:
		case WIN_WRITEDMA_QUEUED_EXT:
				/* IDE_DRIVE_TASK_OUT */
		default:
			break;
	}
	return(NULL);
}

EXPORT_SYMBOL(ide_pre_handler_parser);

/* Called by internal to feature out type of command being called */
//ide_handler_t * ide_handler_parser (task_struct_t *taskfile, hob_struct_t *hobfile)
ide_handler_t * ide_handler_parser (struct hd_drive_task_hdr *taskfile, struct hd_drive_hob_hdr *hobfile)
{
	switch(taskfile->command) {
		case WIN_IDENTIFY:
		case WIN_PIDENTIFY:
		case CFA_TRANSLATE_SECTOR:
		case WIN_READ_BUFFER:
		case WIN_READ:
	//	case WIN_READ_ONCE:
		case WIN_READ_EXT:
			return &task_in_intr;
		case WIN_SECURITY_DISABLE:
		case WIN_SECURITY_ERASE_UNIT:
		case WIN_SECURITY_SET_PASS:
		case WIN_SECURITY_UNLOCK:
		case WIN_DOWNLOAD_MICROCODE:
		case CFA_WRITE_SECT_WO_ERASE:
		case WIN_WRITE_BUFFER:
		case WIN_WRITE_VERIFY:
		case WIN_WRITE:
	//	case WIN_WRITE_ONCE:	
		case WIN_WRITE_EXT:
			return &task_out_intr;
	//	case WIN_READ_LONG:
	//	case WIN_READ_LONG_ONCE:
		case WIN_MULTREAD:
		case WIN_MULTREAD_EXT:
			return &task_mulin_intr;
	//	case WIN_WRITE_LONG:
	//	case WIN_WRITE_LONG_ONCE:
		case CFA_WRITE_MULTI_WO_ERASE:
		case WIN_MULTWRITE:
		case WIN_MULTWRITE_EXT:
			return &task_mulout_intr;
		case WIN_SMART:
			switch(taskfile->feature) {
				case SMART_READ_VALUES:
				case SMART_READ_THRESHOLDS:
				case SMART_READ_LOG_SECTOR:
					return &task_in_intr;
				case SMART_WRITE_LOG_SECTOR:
					return &task_out_intr;
				default:
					return &task_no_data_intr;
			}
		case CFA_REQ_EXT_ERROR_CODE:
		case CFA_ERASE_SECTORS:
		case WIN_VERIFY:
	//	case WIN_VERIFY_ONCE:
		case WIN_VERIFY_EXT:
		case WIN_SEEK:
			return &task_no_data_intr;
		case WIN_SPECIFY:
			return &set_geometry_intr;
		case WIN_RECAL:
	//	case WIN_RESTORE:
			return &recal_intr;
		case WIN_NOP:
		case WIN_DIAGNOSE:
		case WIN_FLUSH_CACHE:
		case WIN_FLUSH_CACHE_EXT:
		case WIN_STANDBYNOW1:
		case WIN_STANDBYNOW2:
		case WIN_SLEEPNOW1:
		case WIN_SLEEPNOW2:
		case WIN_SETIDLE1:
		case WIN_CHECKPOWERMODE1:
		case WIN_CHECKPOWERMODE2:
		case WIN_GETMEDIASTATUS:
		case WIN_MEDIAEJECT:
			return &task_no_data_intr;
		case WIN_SETMULT:
			return &set_multmode_intr;
		case WIN_READ_NATIVE_MAX:
		case WIN_SET_MAX:
		case WIN_READ_NATIVE_MAX_EXT:
		case WIN_SET_MAX_EXT:
		case WIN_SECURITY_ERASE_PREPARE:
		case WIN_SECURITY_FREEZE_LOCK:
		case WIN_DOORLOCK:
		case WIN_DOORUNLOCK:
		case WIN_SETFEATURES:
			return &task_no_data_intr;
		case DISABLE_SEAGATE:
		case EXABYTE_ENABLE_NEST:
			return &task_no_data_intr;
#ifdef CONFIG_BLK_DEV_IDEDMA
		case WIN_READDMA:
	//	case WIN_READDMA_ONCE:
		case WIN_IDENTIFY_DMA:
		case WIN_READDMA_QUEUED:
		case WIN_READDMA_EXT:
		case WIN_READDMA_QUEUED_EXT:
		case WIN_WRITEDMA:
	//	case WIN_WRITEDMA_ONCE:
		case WIN_WRITEDMA_QUEUED:
		case WIN_WRITEDMA_EXT:
		case WIN_WRITEDMA_QUEUED_EXT:
#endif
		case WIN_FORMAT:
		case WIN_INIT:
		case WIN_DEVICE_RESET:
		case WIN_QUEUED_SERVICE:
		case WIN_PACKETCMD:
		default:
			return(NULL);
	}	
}

EXPORT_SYMBOL(ide_handler_parser);

ide_post_handler_t * ide_post_handler_parser (struct hd_drive_task_hdr *taskfile, struct hd_drive_hob_hdr *hobfile)
{
	switch(taskfile->command) {
		case WIN_SPECIFY:	/* set_geometry_intr */
		case WIN_RESTORE:	/* recal_intr */
		case WIN_SETMULT:	/* set_multmode_intr */
		default:
			return(NULL);
	}
}

EXPORT_SYMBOL(ide_post_handler_parser);

/* Called by ioctl to feature out type of command being called */
int ide_cmd_type_parser (ide_task_t *args)
{

	task_struct_t *taskfile = (task_struct_t *) args->tfRegister;
	hob_struct_t *hobfile   = (hob_struct_t *) args->hobRegister;

	args->prehandler	= ide_pre_handler_parser(taskfile, hobfile);
	args->handler		= ide_handler_parser(taskfile, hobfile);
	args->posthandler	= ide_post_handler_parser(taskfile, hobfile);

	switch(args->tfRegister[IDE_COMMAND_OFFSET]) {
		case WIN_IDENTIFY:
		case WIN_PIDENTIFY:
			return IDE_DRIVE_TASK_IN;
		case CFA_TRANSLATE_SECTOR:
		case WIN_READ:
	//	case WIN_READ_ONCE:
		case WIN_READ_EXT:
		case WIN_READ_BUFFER:
			return IDE_DRIVE_TASK_IN;
		case WIN_WRITE:
	//	case WIN_WRITE_ONCE:
		case WIN_WRITE_EXT:
		case WIN_WRITE_VERIFY:
		case WIN_WRITE_BUFFER:
		case CFA_WRITE_SECT_WO_ERASE:
		case WIN_DOWNLOAD_MICROCODE:
			return IDE_DRIVE_TASK_RAW_WRITE;
	//	case WIN_READ_LONG:
	//	case WIN_READ_LONG_ONCE:
		case WIN_MULTREAD:
		case WIN_MULTREAD_EXT:
			return IDE_DRIVE_TASK_IN;
	//	case WIN_WRITE_LONG:
	//	case WIN_WRITE_LONG_ONCE:
		case CFA_WRITE_MULTI_WO_ERASE:
		case WIN_MULTWRITE:
		case WIN_MULTWRITE_EXT:
			return IDE_DRIVE_TASK_RAW_WRITE;
		case WIN_SECURITY_DISABLE:
		case WIN_SECURITY_ERASE_UNIT:
		case WIN_SECURITY_SET_PASS:
		case WIN_SECURITY_UNLOCK:
			return IDE_DRIVE_TASK_OUT;
		case WIN_SMART:
			args->tfRegister[IDE_LCYL_OFFSET] = SMART_LCYL_PASS;
			args->tfRegister[IDE_HCYL_OFFSET] = SMART_HCYL_PASS;
			switch(args->tfRegister[IDE_FEATURE_OFFSET]) {
				case SMART_READ_VALUES:
				case SMART_READ_THRESHOLDS:
				case SMART_READ_LOG_SECTOR:
					return IDE_DRIVE_TASK_IN;
				case SMART_WRITE_LOG_SECTOR:
					return IDE_DRIVE_TASK_OUT;
				default:
					return IDE_DRIVE_TASK_NO_DATA;
			}
#ifdef CONFIG_BLK_DEV_IDEDMA
		case WIN_READDMA:
	//	case WIN_READDMA_ONCE:
		case WIN_IDENTIFY_DMA:
		case WIN_READDMA_QUEUED:
		case WIN_READDMA_EXT:
		case WIN_READDMA_QUEUED_EXT:
			return IDE_DRIVE_TASK_IN;
		case WIN_WRITEDMA:
	//	case WIN_WRITEDMA_ONCE:
		case WIN_WRITEDMA_QUEUED:
		case WIN_WRITEDMA_EXT:
		case WIN_WRITEDMA_QUEUED_EXT:
			return IDE_DRIVE_TASK_RAW_WRITE;
#endif
		case WIN_SETFEATURES:
			switch(args->tfRegister[IDE_FEATURE_OFFSET]) {
				case SETFEATURES_EN_8BIT:
				case SETFEATURES_EN_WCACHE:
					return IDE_DRIVE_TASK_NO_DATA;
				case SETFEATURES_XFER:
					return IDE_DRIVE_TASK_SET_XFER;
				case SETFEATURES_DIS_DEFECT:
				case SETFEATURES_EN_APM:
				case SETFEATURES_DIS_MSN:
				case SETFEATURES_DIS_RETRY:
				case SETFEATURES_EN_AAM:
				case SETFEATURES_RW_LONG:
				case SETFEATURES_SET_CACHE:
				case SETFEATURES_DIS_RLA:
				case SETFEATURES_EN_RI:
				case SETFEATURES_EN_SI:
				case SETFEATURES_DIS_RPOD:
				case SETFEATURES_DIS_WCACHE:
				case SETFEATURES_EN_DEFECT:
				case SETFEATURES_DIS_APM:
				case SETFEATURES_EN_ECC:
				case SETFEATURES_EN_MSN:
				case SETFEATURES_EN_RETRY:
				case SETFEATURES_EN_RLA:
				case SETFEATURES_PREFETCH:
				case SETFEATURES_4B_RW_LONG:
				case SETFEATURES_DIS_AAM:
				case SETFEATURES_EN_RPOD:
				case SETFEATURES_DIS_RI:
				case SETFEATURES_DIS_SI:
				default:
					return IDE_DRIVE_TASK_NO_DATA;
			}
		case WIN_NOP:
		case CFA_REQ_EXT_ERROR_CODE:
		case CFA_ERASE_SECTORS:
		case WIN_VERIFY:
	//	case WIN_VERIFY_ONCE:
		case WIN_VERIFY_EXT:
		case WIN_SEEK:
		case WIN_SPECIFY:
		case WIN_RESTORE:
		case WIN_DIAGNOSE:
		case WIN_FLUSH_CACHE:
		case WIN_FLUSH_CACHE_EXT:
		case WIN_STANDBYNOW1:
		case WIN_STANDBYNOW2:
		case WIN_SLEEPNOW1:
		case WIN_SLEEPNOW2:
		case WIN_SETIDLE1:
		case DISABLE_SEAGATE:
		case WIN_CHECKPOWERMODE1:
		case WIN_CHECKPOWERMODE2:
		case WIN_GETMEDIASTATUS:
		case WIN_MEDIAEJECT:
		case WIN_SETMULT:
		case WIN_READ_NATIVE_MAX:
		case WIN_SET_MAX:
		case WIN_READ_NATIVE_MAX_EXT:
		case WIN_SET_MAX_EXT:
		case WIN_SECURITY_ERASE_PREPARE:
		case WIN_SECURITY_FREEZE_LOCK:
		case EXABYTE_ENABLE_NEST:
		case WIN_DOORLOCK:
		case WIN_DOORUNLOCK:
			return IDE_DRIVE_TASK_NO_DATA;
		case WIN_FORMAT:
		case WIN_INIT:
		case WIN_DEVICE_RESET:
		case WIN_QUEUED_SERVICE:
		case WIN_PACKETCMD:
		default:
			return IDE_DRIVE_TASK_INVALID;
	}
}

EXPORT_SYMBOL(ide_cmd_type_parser);

/*
 * This function is intended to be used prior to invoking ide_do_drive_cmd().
 */
void ide_init_drive_taskfile (struct request *rq)
{
	memset(rq, 0, sizeof(*rq));
	rq->cmd = IDE_DRIVE_TASK_NO_DATA;
}

EXPORT_SYMBOL(ide_init_drive_taskfile);

#if 1

int ide_diag_taskfile (ide_drive_t *drive, ide_task_t *args, unsigned long data_size, u8 *buf)
{
	struct request rq;

	ide_init_drive_taskfile(&rq);
	rq.cmd = IDE_DRIVE_TASKFILE;
	rq.buffer = buf;

	/*
	 * (ks) We transfer currently only whole sectors.
	 * This is suffient for now.  But, it would be great,
	 * if we would find a solution to transfer any size.
	 * To support special commands like READ LONG.
	 */
	if (args->command_type != IDE_DRIVE_TASK_NO_DATA) {
		if (data_size == 0)
			rq.current_nr_sectors = rq.nr_sectors = (args->hobRegister[IDE_NSECTOR_OFFSET_HOB] << 8) | args->tfRegister[IDE_NSECTOR_OFFSET];
		/*	rq.hard_cur_sectors	*/
		else
			rq.current_nr_sectors = rq.nr_sectors = data_size / SECTOR_SIZE;
		/*	rq.hard_cur_sectors	*/
	}

	if (args->tf_out_flags.all == 0) {
		/*
		 * clean up kernel settings for driver sanity, regardless.
		 * except for discrete diag services.
		 */
		args->posthandler = ide_post_handler_parser(
				(struct hd_drive_task_hdr *) args->tfRegister,
				(struct hd_drive_hob_hdr *) args->hobRegister);

	}
	rq.special = args;
	return ide_do_drive_cmd(drive, &rq, ide_wait);
}

#else

int ide_diag_taskfile (ide_drive_t *drive, ide_task_t *args, unsigned long data_size, u8 *buf)
{
	struct request *rq;
	unsigned long flags;
	ide_hwgroup_t *hwgroup = HWGROUP(drive);
	unsigned int major = HWIF(drive)->major;
	struct list_head *queue_head = &drive->queue.queue_head;
	DECLARE_COMPLETION(wait);

	if (HWIF(drive)->chipset == ide_pdc4030 && buf != NULL)
		return -ENOSYS; /* special drive cmds not supported */

	memset(rq, 0, sizeof(*rq));
	rq->cmd = IDE_DRIVE_TASKFILE;
	rq->buffer = buf;

	/*
	 * (ks) We transfer currently only whole sectors.
	 * This is suffient for now.  But, it would be great,
	 * if we would find a solution to transfer any size.
	 * To support special commands like READ LONG.
	 */
	if (args->command_type != IDE_DRIVE_TASK_NO_DATA) {
		if (data_size == 0) {
			ata_nsector_t nsector;
			nsector.b.low = args->hobRegister[IDE_NSECTOR_OFFSET_HOB];
			nsector.b.high = args->tfRegister[IDE_NSECTOR_OFFSET];
			rq.nr_sectors = nsector.all;
		} else {
			rq.nr_sectors = data_size / SECTOR_SIZE;
		}
		rq.current_nr_sectors = rq.nr_sectors;
	//	rq.hard_cur_sectors = rq.nr_sectors;
	}

	if (args->tf_out_flags.all == 0) {
		/*
		 * clean up kernel settings for driver sanity, regardless.
		 * except for discrete diag services.
		 */
		args->posthandler = ide_post_handler_parser(
				(struct hd_drive_task_hdr *) args->tfRegister,
				(struct hd_drive_hob_hdr *) args->hobRegister);
	}
	rq->special = args;
	rq->errors = 0;
	rq->rq_status = RQ_ACTIVE;
	rq->rq_dev = MKDEV(major,(drive->select.b.unit)<<PARTN_BITS);
	rq->waiting = &wait;

	spin_lock_irqsave(&io_request_lock, flags);
	queue_head = queue_head->prev;
	list_add(&rq->queue, queue_head);
	ide_do_request(hwgroup, 0);
	spin_unlock_irqrestore(&io_request_lock, flags);

	wait_for_completion(&wait);	/* wait for it to be serviced */
	return rq->errors ? -EIO : 0;	/* return -EIO if errors */
}

#endif

EXPORT_SYMBOL(ide_diag_taskfile);

int ide_raw_taskfile (ide_drive_t *drive, ide_task_t *args, u8 *buf)
{
	return ide_diag_taskfile(drive, args, 0, buf);
}

EXPORT_SYMBOL(ide_raw_taskfile);
	
#ifdef CONFIG_IDE_TASK_IOCTL_DEBUG
char * ide_ioctl_verbose (unsigned int cmd)
{
	return("unknown");
}

char * ide_task_cmd_verbose (u8 task)
{
	return("unknown");
}
#endif /* CONFIG_IDE_TASK_IOCTL_DEBUG */

#define MAX_DMA		(256*SECTOR_WORDS)

ide_startstop_t flagged_taskfile(ide_drive_t *, ide_task_t *);
ide_startstop_t flagged_task_no_data_intr(ide_drive_t *);
ide_startstop_t flagged_task_in_intr(ide_drive_t *);
ide_startstop_t flagged_task_mulin_intr(ide_drive_t *);
ide_startstop_t flagged_pre_task_out_intr(ide_drive_t *, struct request *);
ide_startstop_t flagged_task_out_intr(ide_drive_t *);
ide_startstop_t flagged_pre_task_mulout_intr(ide_drive_t *, struct request *);
ide_startstop_t flagged_task_mulout_intr(ide_drive_t *);

int ide_taskfile_ioctl (ide_drive_t *drive, struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	ide_task_request_t	*req_task;
	ide_task_t		args;
	u8 *outbuf		= NULL;
	u8 *inbuf		= NULL;
	task_ioreg_t *argsptr	= args.tfRegister;
	task_ioreg_t *hobsptr	= args.hobRegister;
	int err			= 0;
	int tasksize		= sizeof(struct ide_task_request_s);
	int taskin		= 0;
	int taskout		= 0;
	u8 io_32bit		= drive->io_32bit;

//	printk("IDE Taskfile ...\n");

	req_task = kmalloc(tasksize, GFP_KERNEL);
	if (req_task == NULL) return -ENOMEM;
	memset(req_task, 0, tasksize);
	if (copy_from_user(req_task, (void *) arg, tasksize)) {
		kfree(req_task);
		return -EFAULT;
	}

	taskout = (int) req_task->out_size;
	taskin  = (int) req_task->in_size;

	if (taskout) {
		int outtotal = tasksize;
		outbuf = kmalloc(taskout, GFP_KERNEL);
		if (outbuf == NULL) {
			err = -ENOMEM;
			goto abort;
		}
		memset(outbuf, 0, taskout);
		if (copy_from_user(outbuf, (void *)arg + outtotal, taskout)) {
			err = -EFAULT;
			goto abort;
		}
	}

	if (taskin) {
		int intotal = tasksize + taskout;
		inbuf = kmalloc(taskin, GFP_KERNEL);
		if (inbuf == NULL) {
			err = -ENOMEM;
			goto abort;
		}
		memset(inbuf, 0, taskin);
		if (copy_from_user(inbuf, (void *)arg + intotal , taskin)) {
			err = -EFAULT;
			goto abort;
		}
	}

	memset(&args, 0, sizeof(ide_task_t));
	memcpy(argsptr, req_task->io_ports, HDIO_DRIVE_TASK_HDR_SIZE);
	memcpy(hobsptr, req_task->hob_ports, HDIO_DRIVE_HOB_HDR_SIZE);

	args.tf_in_flags  = req_task->in_flags;
	args.tf_out_flags = req_task->out_flags;
	args.data_phase   = req_task->data_phase;
	args.command_type = req_task->req_cmd;

#ifdef CONFIG_IDE_TASK_IOCTL_DEBUG
	DTF("%s: ide_ioctl_cmd %s:  ide_task_cmd %s\n",
		drive->name,
		ide_ioctl_verbose(cmd),
		ide_task_cmd_verbose(args.tfRegister[IDE_COMMAND_OFFSET]));
#endif /* CONFIG_IDE_TASK_IOCTL_DEBUG */

	drive->io_32bit = 0;
	switch(req_task->data_phase) {
		case TASKFILE_OUT_DMAQ:
		case TASKFILE_OUT_DMA:
			err = ide_diag_taskfile(drive, &args, taskout, outbuf);
			break;
		case TASKFILE_IN_DMAQ:
		case TASKFILE_IN_DMA:
			err = ide_diag_taskfile(drive, &args, taskin, inbuf);
			break;
		case TASKFILE_IN_OUT:
#if 0
			args.prehandler = &pre_task_out_intr;
			args.handler = &task_out_intr;
			args.posthandler = NULL;
			err = ide_diag_taskfile(drive, &args, taskout, outbuf);
			args.prehandler = NULL;
			args.handler = &task_in_intr;
			args.posthandler = NULL;
			err = ide_diag_taskfile(drive, &args, taskin, inbuf);
			break;
#else
			err = -EFAULT;
			goto abort;
#endif
		case TASKFILE_MULTI_OUT:
			if (!drive->mult_count) {
				/* (hs): give up if multcount is not set */
				printk(KERN_ERR "%s: %s Multimode Write " \
					"multcount is not set\n",
					drive->name, __FUNCTION__);
				err = -EPERM;
				goto abort;
			}
			if (args.tf_out_flags.all != 0) {
				args.prehandler = &flagged_pre_task_mulout_intr;
				args.handler = &flagged_task_mulout_intr;
			} else {
				args.prehandler = &pre_task_mulout_intr;
				args.handler = &task_mulout_intr;
			}
			err = ide_diag_taskfile(drive, &args, taskout, outbuf);
			break;
		case TASKFILE_OUT:
			if (args.tf_out_flags.all != 0) {
				args.prehandler = &flagged_pre_task_out_intr;
				args.handler    = &flagged_task_out_intr;
			} else {
				args.prehandler = &pre_task_out_intr;
				args.handler = &task_out_intr;
			}
			err = ide_diag_taskfile(drive, &args, taskout, outbuf);
			break;
		case TASKFILE_MULTI_IN:
			if (!drive->mult_count) {
				/* (hs): give up if multcount is not set */
				printk(KERN_ERR "%s: %s Multimode Read failure " \
					"multcount is not set\n",
					drive->name, __FUNCTION__);
				err = -EPERM;
				goto abort;
			}
			if (args.tf_out_flags.all != 0) {
				args.handler = &flagged_task_mulin_intr;
			} else {
				args.handler = &task_mulin_intr;
			}
			err = ide_diag_taskfile(drive, &args, taskin, inbuf);
			break;
		case TASKFILE_IN:
			if (args.tf_out_flags.all != 0) {
				args.handler = &flagged_task_in_intr;
			} else {
				args.handler = &task_in_intr;
			}
			err = ide_diag_taskfile(drive, &args, taskin, inbuf);
			break;
		case TASKFILE_NO_DATA:
			if (args.tf_out_flags.all != 0) {
				args.handler = &flagged_task_no_data_intr;
			} else {
				args.handler = &task_no_data_intr;
			}
			err = ide_diag_taskfile(drive, &args, 0, NULL);
			break;
		default:
			err = -EFAULT;
			goto abort;
	}

	memcpy(req_task->io_ports, &(args.tfRegister), HDIO_DRIVE_TASK_HDR_SIZE);
	memcpy(req_task->hob_ports, &(args.hobRegister), HDIO_DRIVE_HOB_HDR_SIZE);
	req_task->in_flags  = args.tf_in_flags;
	req_task->out_flags = args.tf_out_flags;

	if (copy_to_user((void *)arg, req_task, tasksize)) {
		err = -EFAULT;
		goto abort;
	}
	if (taskout) {
		int outtotal = tasksize;
		if (copy_to_user((void *)arg+outtotal, outbuf, taskout)) {
			err = -EFAULT;
			goto abort;
		}
	}
	if (taskin) {
		int intotal = tasksize + taskout;
		if (copy_to_user((void *)arg+intotal, inbuf, taskin)) {
			err = -EFAULT;
			goto abort;
		}
	}
abort:
	kfree(req_task);
	if (outbuf != NULL)
		kfree(outbuf);
	if (inbuf != NULL)
		kfree(inbuf);

//	printk("IDE Taskfile ioctl ended. rc = %i\n", err);

	drive->io_32bit = io_32bit;

	return err;
}

EXPORT_SYMBOL(ide_taskfile_ioctl);

int ide_wait_cmd (ide_drive_t *drive, u8 cmd, u8 nsect, u8 feature, u8 sectors, u8 *buf)
{
	struct request rq;
	u8 buffer[4];

	if (!buf)
		buf = buffer;
	memset(buf, 0, 4 + SECTOR_WORDS * 4 * sectors);
	ide_init_drive_cmd(&rq);
	rq.buffer = buf;
	*buf++ = cmd;
	*buf++ = nsect;
	*buf++ = feature;
	*buf++ = sectors;
	return ide_do_drive_cmd(drive, &rq, ide_wait);
}

EXPORT_SYMBOL(ide_wait_cmd);

/*
 * FIXME : this needs to map into at taskfile. <andre@linux-ide.org>
 */
int ide_cmd_ioctl (ide_drive_t *drive, struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
#if 1
	int err = -EIO;
	u8 args[4], *argbuf = args;
	u8 xfer_rate = 0;
	int argsize = 4;
	ide_task_t tfargs;

	if (NULL == (void *) arg) {
		struct request rq;
		ide_init_drive_cmd(&rq);
		return ide_do_drive_cmd(drive, &rq, ide_wait);
	}

	if (copy_from_user(args, (void *)arg, 4))
		return -EFAULT;

	memset(&tfargs, 0, sizeof(ide_task_t));
	tfargs.tfRegister[IDE_FEATURE_OFFSET] = args[2];
	tfargs.tfRegister[IDE_NSECTOR_OFFSET] = args[3];
	tfargs.tfRegister[IDE_SECTOR_OFFSET]  = args[1];
	tfargs.tfRegister[IDE_LCYL_OFFSET]    = 0x00;
	tfargs.tfRegister[IDE_HCYL_OFFSET]    = 0x00;
	tfargs.tfRegister[IDE_SELECT_OFFSET]  = 0x00;
	tfargs.tfRegister[IDE_COMMAND_OFFSET] = args[0];

	if (args[3]) {
		argsize = 4 + (SECTOR_WORDS * 4 * args[3]);
		argbuf = kmalloc(argsize, GFP_KERNEL);
		if (argbuf == NULL)
			return -ENOMEM;
		memcpy(argbuf, args, 4);
	}
	if (set_transfer(drive, &tfargs)) {
		xfer_rate = args[1];
		if (ide_ata66_check(drive, &tfargs))
			goto abort;
	}

	err = ide_wait_cmd(drive, args[0], args[1], args[2], args[3], argbuf);

	if (!err && xfer_rate) {
		/* active-retuning-calls future */
		ide_set_xfer_rate(drive, xfer_rate);
		ide_driveid_update(drive);
	}
abort:
	if (copy_to_user((void *)arg, argbuf, argsize))
		err = -EFAULT;
	if (argsize > 4)
		kfree(argbuf);
	return err;

#else

	int err = 0;
	u8 args[4], *argbuf = args;
	u8 xfer_rate = 0;
	int argsize = 0;
	ide_task_t tfargs;

	if (NULL == (void *) arg) {
		struct request rq;
		ide_init_drive_cmd(&rq);
		return ide_do_drive_cmd(drive, &rq, ide_wait);
	}

	if (copy_from_user(args, (void *)arg, 4))
		return -EFAULT;

	memset(&tfargs, 0, sizeof(ide_task_t));
	tfargs.tfRegister[IDE_FEATURE_OFFSET] = args[2];
	tfargs.tfRegister[IDE_NSECTOR_OFFSET] = args[3];
	tfargs.tfRegister[IDE_SECTOR_OFFSET]  = args[1];
	tfargs.tfRegister[IDE_LCYL_OFFSET]    = 0x00;
	tfargs.tfRegister[IDE_HCYL_OFFSET]    = 0x00;
	tfargs.tfRegister[IDE_SELECT_OFFSET]  = 0x00;
	tfargs.tfRegister[IDE_COMMAND_OFFSET] = args[0];

	if (args[3]) {
		argsize = (SECTOR_WORDS * 4 * args[3]);
		argbuf = kmalloc(argsize, GFP_KERNEL);
		if (argbuf == NULL)
			return -ENOMEM;
	}

	if (set_transfer(drive, &tfargs)) {
		xfer_rate = args[1];
		if (ide_ata66_check(drive, &tfargs))
			goto abort;
	}

	tfargs.command_type = ide_cmd_type_parser(&tfargs);
	err = ide_raw_taskfile(drive, &tfargs, argbuf);

	if (!err && xfer_rate) {
		/* active-retuning-calls future */
		ide_set_xfer_rate(drive, xfer_rate);
		ide_driveid_update(drive);
	}
abort:
	args[0] = tfargs.tfRegister[IDE_COMMAND_OFFSET];
	args[1] = tfargs.tfRegister[IDE_FEATURE_OFFSET];
	args[2] = tfargs.tfRegister[IDE_NSECTOR_OFFSET];
	args[3] = 0;

	if (copy_to_user((void *)arg, argbuf, 4))
		err = -EFAULT;
	if (argbuf != NULL) {
		if (copy_to_user((void *)arg, argbuf + 4, argsize))
			err = -EFAULT;
		kfree(argbuf);
	}
	return err;

#endif
}

EXPORT_SYMBOL(ide_cmd_ioctl);

int ide_wait_cmd_task (ide_drive_t *drive, u8 *buf)
{
	struct request rq;

	ide_init_drive_cmd(&rq);
	rq.cmd = IDE_DRIVE_TASK;
	rq.buffer = buf;
	return ide_do_drive_cmd(drive, &rq, ide_wait);
}

EXPORT_SYMBOL(ide_wait_cmd_task);

/*
 * FIXME : this needs to map into at taskfile. <andre@linux-ide.org>
 */
int ide_task_ioctl (ide_drive_t *drive, struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	u8 args[7], *argbuf = args;
	int argsize = 7;

	if (copy_from_user(args, (void *)arg, 7))
		return -EFAULT;
	err = ide_wait_cmd_task(drive, argbuf);
	if (copy_to_user((void *)arg, argbuf, argsize))
		err = -EFAULT;
	return err;
}

EXPORT_SYMBOL(ide_task_ioctl);

/*
 * NOTICE: This is additions from IBM to provide a discrete interface,
 * for selective taskregister access operations.  Nice JOB Klaus!!!
 * Glad to be able to work and co-develop this with you and IBM.
 */
ide_startstop_t flagged_taskfile (ide_drive_t *drive, ide_task_t *task)
{
	ide_hwif_t *hwif	= HWIF(drive);
	task_struct_t *taskfile	= (task_struct_t *) task->tfRegister;
	hob_struct_t *hobfile	= (hob_struct_t *) task->hobRegister;
#if DEBUG_TASKFILE
	u8 status;
#endif


#ifdef CONFIG_IDE_TASK_IOCTL_DEBUG
	void debug_taskfile(drive, task);
#endif /* CONFIG_IDE_TASK_IOCTL_DEBUG */

	/*
	 * (ks) Check taskfile in/out flags.
	 * If set, then execute as it is defined.
	 * If not set, then define default settings.
	 * The default values are:
	 *	write and read all taskfile registers (except data) 
	 *	write and read the hob registers (sector,nsector,lcyl,hcyl)
	 */
	if (task->tf_out_flags.all == 0) {
		task->tf_out_flags.all = IDE_TASKFILE_STD_OUT_FLAGS;
		if (drive->addressing == 1)
			task->tf_out_flags.all |= (IDE_HOB_STD_OUT_FLAGS << 8);
        }

	if (task->tf_in_flags.all == 0) {
		task->tf_in_flags.all = IDE_TASKFILE_STD_IN_FLAGS;
		if (drive->addressing == 1)
			task->tf_in_flags.all |= (IDE_HOB_STD_IN_FLAGS  << 8);
        }

	/* ALL Command Block Executions SHALL clear nIEN, unless otherwise */
	if (IDE_CONTROL_REG)
		/* clear nIEN */
		hwif->OUTB(drive->ctl, IDE_CONTROL_REG);
	SELECT_MASK(drive, 0);

#if DEBUG_TASKFILE
	status = hwif->INB(IDE_STATUS_REG);
	if (status & 0x80) {
		printk("flagged_taskfile -> Bad status. Status = %02x. wait 100 usec ...\n", status);
		udelay(100);
		status = hwif->INB(IDE_STATUS_REG);
		printk("flagged_taskfile -> Status = %02x\n", status);
	}
#endif

	if (task->tf_out_flags.b.data) {
		u16 data =  taskfile->data + (hobfile->data << 8);
		hwif->OUTW(data, IDE_DATA_REG);
	}

	/* (ks) send hob registers first */
	if (task->tf_out_flags.b.nsector_hob)
		hwif->OUTB(hobfile->sector_count, IDE_NSECTOR_REG);
	if (task->tf_out_flags.b.sector_hob)
		hwif->OUTB(hobfile->sector_number, IDE_SECTOR_REG);
	if (task->tf_out_flags.b.lcyl_hob)
		hwif->OUTB(hobfile->low_cylinder, IDE_LCYL_REG);
	if (task->tf_out_flags.b.hcyl_hob)
		hwif->OUTB(hobfile->high_cylinder, IDE_HCYL_REG);

	/* (ks) Send now the standard registers */
	if (task->tf_out_flags.b.error_feature)
		hwif->OUTB(taskfile->feature, IDE_FEATURE_REG);
	/* refers to number of sectors to transfer */
	if (task->tf_out_flags.b.nsector)
		hwif->OUTB(taskfile->sector_count, IDE_NSECTOR_REG);
	/* refers to sector offset or start sector */
	if (task->tf_out_flags.b.sector)
		hwif->OUTB(taskfile->sector_number, IDE_SECTOR_REG);
	if (task->tf_out_flags.b.lcyl)
		hwif->OUTB(taskfile->low_cylinder, IDE_LCYL_REG);
	if (task->tf_out_flags.b.hcyl)
		hwif->OUTB(taskfile->high_cylinder, IDE_HCYL_REG);

        /*
	 * (ks) In the flagged taskfile approch, we will used all specified
	 * registers and the register value will not be changed. Except the
	 * select bit (master/slave) in the drive_head register. We must make
	 * sure that the desired drive is selected.
	 */
	hwif->OUTB(taskfile->device_head | drive->select.all, IDE_SELECT_REG);
	switch(task->data_phase) {

   	        case TASKFILE_OUT_DMAQ:
		case TASKFILE_OUT_DMA:
			hwif->ide_dma_write(drive);
			break;

		case TASKFILE_IN_DMAQ:
		case TASKFILE_IN_DMA:
			hwif->ide_dma_read(drive);
			break;

	        default:
 			if (task->handler == NULL)
				return ide_stopped;

			/* Issue the command */
			ide_execute_command(drive, taskfile->command, task->handler, WAIT_WORSTCASE, NULL);
			if (task->prehandler != NULL)
				return task->prehandler(drive, HWGROUP(drive)->rq);
	}

	return ide_started;
}

EXPORT_SYMBOL(flagged_taskfile);

ide_startstop_t flagged_task_no_data_intr (ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);
	u8 stat;

	local_irq_enable();

	if (!OK_STAT(stat = hwif->INB(IDE_STATUS_REG), READY_STAT, BAD_STAT)) {
		if (stat & ERR_STAT) {
			return DRIVER(drive)->error(drive, "flagged_task_no_data_intr", stat);
		}
		/*
		 * (ks) Unexpected ATA data phase detected.
		 * This should not happen. But, it can !
		 * I am not sure, which function is best to clean up
		 * this situation.  I choose: ide_error(...)
		 */
 		return DRIVER(drive)->error(drive, "flagged_task_no_data_intr (unexpected phase)", stat); 
	}

	ide_end_drive_cmd(drive, stat, hwif->INB(IDE_ERROR_REG));

	return ide_stopped;
}

/*
 * Handler for command with PIO data-in phase
 */
ide_startstop_t flagged_task_in_intr (ide_drive_t *drive)
{
	ide_hwif_t *hwif	= HWIF(drive);
	u8 stat			= hwif->INB(IDE_STATUS_REG);
	struct request *rq	= HWGROUP(drive)->rq;
	char *pBuf		= NULL;
	int retries             = 5;

	if (rq->current_nr_sectors == 0) 
		return DRIVER(drive)->error(drive, "flagged_task_in_intr (no data requested)", stat); 

	if (!OK_STAT(stat, DATA_READY, BAD_R_STAT)) {
		if (stat & ERR_STAT) {
			return DRIVER(drive)->error(drive, "flagged_task_in_intr", stat);
		}
		/*
		 * (ks) Unexpected ATA data phase detected.
		 * This should not happen. But, it can !
		 * I am not sure, which function is best to clean up
		 * this situation.  I choose: ide_error(...)
		 */
		return DRIVER(drive)->error(drive, "flagged_task_in_intr (unexpected data phase)", stat); 
	}

	pBuf = rq->buffer + ((rq->nr_sectors - rq->current_nr_sectors) * SECTOR_SIZE);
	DTF("Read - rq->current_nr_sectors: %d, status: %02x\n", (int) rq->current_nr_sectors, stat);

	taskfile_input_data(drive, pBuf, SECTOR_WORDS);

	if (--rq->current_nr_sectors != 0) {
		/*
                 * (ks) We don't know which command was executed. 
		 * So, we wait the 'WORSTCASE' value.
                 */
		ide_set_handler(drive, &flagged_task_in_intr,  WAIT_WORSTCASE, NULL);
		return ide_started;
	}
	/*
	 * (ks) Last sector was transfered, wait until drive is ready. 
	 * This can take up to 10 usec. We willl wait max 50 us.
	 */
	while (((stat = hwif->INB(IDE_STATUS_REG)) & BUSY_STAT) && retries--)
		udelay(10);
	ide_end_drive_cmd (drive, stat, hwif->INB(IDE_ERROR_REG));

	return ide_stopped;
}

ide_startstop_t flagged_task_mulin_intr (ide_drive_t *drive)
{
	ide_hwif_t *hwif	= HWIF(drive);
	u8 stat			= hwif->INB(IDE_STATUS_REG);
	struct request *rq	= HWGROUP(drive)->rq;
	char *pBuf		= NULL;
	int retries             = 5;
	unsigned int msect, nsect;

	if (rq->current_nr_sectors == 0) 
		return DRIVER(drive)->error(drive, "flagged_task_mulin_intr (no data requested)", stat); 

	msect = drive->mult_count;
	if (msect == 0) 
		return DRIVER(drive)->error(drive, "flagged_task_mulin_intr (multimode not set)", stat); 

	if (!OK_STAT(stat, DATA_READY, BAD_R_STAT)) {
		if (stat & ERR_STAT) {
			return DRIVER(drive)->error(drive, "flagged_task_mulin_intr", stat);
		}
		/*
		 * (ks) Unexpected ATA data phase detected.
		 * This should not happen. But, it can !
		 * I am not sure, which function is best to clean up
		 * this situation.  I choose: ide_error(...)
		 */
		return DRIVER(drive)->error(drive, "flagged_task_mulin_intr (unexpected data phase)", stat); 
	}

	nsect = (rq->current_nr_sectors > msect) ? msect : rq->current_nr_sectors;
	pBuf = rq->buffer + ((rq->nr_sectors - rq->current_nr_sectors) * SECTOR_SIZE);

	DTF("Multiread: %p, nsect: %d , rq->current_nr_sectors: %ld\n",
	    pBuf, nsect, rq->current_nr_sectors);

	taskfile_input_data(drive, pBuf, nsect * SECTOR_WORDS);

	rq->current_nr_sectors -= nsect;
	if (rq->current_nr_sectors != 0) {
		/*
                 * (ks) We don't know which command was executed. 
		 * So, we wait the 'WORSTCASE' value.
                 */
		ide_set_handler(drive, &flagged_task_mulin_intr,  WAIT_WORSTCASE, NULL);
		return ide_started;
	}

	/*
	 * (ks) Last sector was transfered, wait until drive is ready. 
	 * This can take up to 10 usec. We willl wait max 50 us.
	 */
	while (((stat = hwif->INB(IDE_STATUS_REG)) & BUSY_STAT) && retries--)
		udelay(10);
	ide_end_drive_cmd (drive, stat, hwif->INB(IDE_ERROR_REG));

	return ide_stopped;
}

/*
 * Pre handler for command with PIO data-out phase
 */
ide_startstop_t flagged_pre_task_out_intr (ide_drive_t *drive, struct request *rq)
{
	ide_hwif_t *hwif	= HWIF(drive);
	u8 stat			= hwif->INB(IDE_STATUS_REG);
	ide_startstop_t startstop;

	if (!rq->current_nr_sectors) {
		return DRIVER(drive)->error(drive, "flagged_pre_task_out_intr (write data not specified)", stat);
	}

	if (ide_wait_stat(&startstop, drive, DATA_READY,
			BAD_W_STAT, WAIT_DRQ)) {
		printk(KERN_ERR "%s: No DRQ bit after issuing write command.\n", drive->name);
		return startstop;
	}

	taskfile_output_data(drive, rq->buffer, SECTOR_WORDS);
	--rq->current_nr_sectors;

	return ide_started;
}

ide_startstop_t flagged_task_out_intr (ide_drive_t *drive)
{
	ide_hwif_t *hwif	= HWIF(drive);
	u8 stat			= hwif->INB(IDE_STATUS_REG);
	struct request *rq	= HWGROUP(drive)->rq;
	char *pBuf		= NULL;

	if (!OK_STAT(stat, DRIVE_READY, BAD_W_STAT)) 
		return DRIVER(drive)->error(drive, "flagged_task_out_intr", stat);
	
	if (!rq->current_nr_sectors) { 
		ide_end_drive_cmd (drive, stat, hwif->INB(IDE_ERROR_REG));
		return ide_stopped;
	}

	if (!OK_STAT(stat, DATA_READY, BAD_W_STAT)) {
		/*
		 * (ks) Unexpected ATA data phase detected.
		 * This should not happen. But, it can !
		 * I am not sure, which function is best to clean up
		 * this situation.  I choose: ide_error(...)
		 */
		return DRIVER(drive)->error(drive, "flagged_task_out_intr (unexpected data phase)", stat); 
	}

	pBuf = rq->buffer + ((rq->nr_sectors - rq->current_nr_sectors) * SECTOR_SIZE);
	DTF("Write - rq->current_nr_sectors: %d, status: %02x\n",
		(int) rq->current_nr_sectors, stat);

	taskfile_output_data(drive, pBuf, SECTOR_WORDS);
	--rq->current_nr_sectors;

	/*
	 * (ks) We don't know which command was executed. 
	 * So, we wait the 'WORSTCASE' value.
	 */
	ide_set_handler(drive, &flagged_task_out_intr, WAIT_WORSTCASE, NULL);

	return ide_started;
}

ide_startstop_t flagged_pre_task_mulout_intr (ide_drive_t *drive, struct request *rq)
{
	ide_hwif_t *hwif	= HWIF(drive);
	u8 stat			= hwif->INB(IDE_STATUS_REG);
	char *pBuf		= NULL;
	ide_startstop_t startstop;
	unsigned int msect, nsect;

	if (!rq->current_nr_sectors) 
		return DRIVER(drive)->error(drive, "flagged_pre_task_mulout_intr (write data not specified)", stat);

	msect = drive->mult_count;
	if (msect == 0)
		return DRIVER(drive)->error(drive, "flagged_pre_task_mulout_intr (multimode not set)", stat);

	if (ide_wait_stat(&startstop, drive, DATA_READY,
			BAD_W_STAT, WAIT_DRQ)) {
		printk(KERN_ERR "%s: No DRQ bit after issuing write command.\n", drive->name);
		return startstop;
	}

	nsect = (rq->current_nr_sectors > msect) ? msect : rq->current_nr_sectors;
	pBuf = rq->buffer + ((rq->nr_sectors - rq->current_nr_sectors) * SECTOR_SIZE);
	DTF("Multiwrite: %p, nsect: %d , rq->current_nr_sectors: %ld\n",
	    pBuf, nsect, rq->current_nr_sectors);

	taskfile_output_data(drive, pBuf, nsect * SECTOR_WORDS);

	rq->current_nr_sectors -= nsect;

	return ide_started;
}

ide_startstop_t flagged_task_mulout_intr (ide_drive_t *drive)
{
	ide_hwif_t *hwif	= HWIF(drive);
	u8 stat			= hwif->INB(IDE_STATUS_REG);
	struct request *rq	= HWGROUP(drive)->rq;
	char *pBuf		= NULL;
	unsigned int msect, nsect;

	msect = drive->mult_count;
	if (msect == 0)
		return DRIVER(drive)->error(drive, "flagged_task_mulout_intr (multimode not set)", stat);

	if (!OK_STAT(stat, DRIVE_READY, BAD_W_STAT)) 
		return DRIVER(drive)->error(drive, "flagged_task_mulout_intr", stat);
	
	if (!rq->current_nr_sectors) { 
		ide_end_drive_cmd (drive, stat, hwif->INB(IDE_ERROR_REG));
		return ide_stopped;
	}

	if (!OK_STAT(stat, DATA_READY, BAD_W_STAT)) {
		/*
		 * (ks) Unexpected ATA data phase detected.
		 * This should not happen. But, it can !
		 * I am not sure, which function is best to clean up
		 * this situation.  I choose: ide_error(...)
		 */
		return DRIVER(drive)->error(drive, "flagged_task_mulout_intr (unexpected data phase)", stat); 
	}

	nsect = (rq->current_nr_sectors > msect) ? msect : rq->current_nr_sectors;
	pBuf = rq->buffer + ((rq->nr_sectors - rq->current_nr_sectors) * SECTOR_SIZE);
	DTF("Multiwrite: %p, nsect: %d , rq->current_nr_sectors: %ld\n",
	    pBuf, nsect, rq->current_nr_sectors);

	taskfile_output_data(drive, pBuf, nsect * SECTOR_WORDS);
	rq->current_nr_sectors -= nsect;

	/*
	 * (ks) We don't know which command was executed. 
	 * So, we wait the 'WORSTCASE' value.
	 */
	ide_set_handler(drive, &flagged_task_mulout_intr, WAIT_WORSTCASE, NULL);

	return ide_started;
}

/*
 * Beginning of Taskfile OPCODE Library and feature sets.
 */

#ifdef CONFIG_PKT_TASK_IOCTL

int pkt_taskfile_ioctl (ide_drive_t *drive, struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
#if 0
	switch(req_task->data_phase) {
		case TASKFILE_P_OUT_DMAQ:
		case TASKFILE_P_IN_DMAQ:
		case TASKFILE_P_OUT_DMA:
		case TASKFILE_P_IN_DMA:
		case TASKFILE_P_OUT:
		case TASKFILE_P_IN:
	}
#endif
	return -ENOMSG;
}

EXPORT_SYMBOL(pkt_taskfile_ioctl);

#endif /* CONFIG_PKT_TASK_IOCTL */
