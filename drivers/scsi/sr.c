/*
 *  sr.c Copyright (C) 1992 David Giller
 *           Copyright (C) 1993, 1994, 1995, 1999 Eric Youngdale
 *
 *  adapted from:
 *      sd.c Copyright (C) 1992 Drew Eckhardt
 *      Linux scsi disk driver by
 *              Drew Eckhardt <drew@colorado.edu>
 *
 *      Modified by Eric Youngdale ericy@andante.org to
 *      add scatter-gather, multiple outstanding request, and other
 *      enhancements.
 *
 *          Modified by Eric Youngdale eric@andante.org to support loadable
 *          low-level scsi drivers.
 *
 *       Modified by Thomas Quinot thomas@melchior.cuivre.fdn.fr to
 *       provide auto-eject.
 *
 *          Modified by Gerd Knorr <kraxel@cs.tu-berlin.de> to support the
 *          generic cdrom interface
 *
 *       Modified by Jens Axboe <axboe@suse.de> - Uniform sr_packet()
 *       interface, capabilities probe additions, ioctl cleanups, etc.
 *
 *       Modified by Richard Gooch <rgooch@atnf.csiro.au> to support devfs
 *
 *       Modified by Jens Axboe <axboe@suse.de> - support DVD-RAM
 *	 transparently and loose the GHOST hack
 *
 *	 Modified by Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *	 check resource allocation in sr_init and some cleanups
 *
 */

#include <linux/module.h>

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/cdrom.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#define MAJOR_NR SCSI_CDROM_MAJOR
#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"
#include "sr.h"
#include <scsi/scsi_ioctl.h>	/* For the door lock/unlock commands */
#include "constants.h"

MODULE_PARM(xa_test, "i");	/* see sr_ioctl.c */

#define MAX_RETRIES	3
#define SR_TIMEOUT	(30 * HZ)

static int sr_init(void);
static void sr_finish(void);
static int sr_attach(Scsi_Device *);
static int sr_detect(Scsi_Device *);
static void sr_detach(Scsi_Device *);

static int sr_init_command(Scsi_Cmnd *);

static struct Scsi_Device_Template sr_template =
{
	name:"cdrom",
	tag:"sr",
	scsi_type:TYPE_ROM,
	major:SCSI_CDROM_MAJOR,
	blk:1,
	detect:sr_detect,
	init:sr_init,
	finish:sr_finish,
	attach:sr_attach,
	detach:sr_detach,
	init_command:sr_init_command
};

Scsi_CD *scsi_CDs;
static int *sr_sizes;

static int *sr_blocksizes;
static int *sr_hardsizes;

static int sr_open(struct cdrom_device_info *, int);
void get_sectorsize(int);
void get_capabilities(int);

static int sr_media_change(struct cdrom_device_info *, int);
static int sr_packet(struct cdrom_device_info *, struct cdrom_generic_command *);

static void sr_release(struct cdrom_device_info *cdi)
{
	if (scsi_CDs[MINOR(cdi->dev)].device->sector_size > 2048)
		sr_set_blocklength(MINOR(cdi->dev), 2048);
	scsi_CDs[MINOR(cdi->dev)].device->access_count--;
	if (scsi_CDs[MINOR(cdi->dev)].device->host->hostt->module)
		__MOD_DEC_USE_COUNT(scsi_CDs[MINOR(cdi->dev)].device->host->hostt->module);
	if (sr_template.module)
		__MOD_DEC_USE_COUNT(sr_template.module);
}

static struct cdrom_device_ops sr_dops =
{
	open:			sr_open,
	release:		sr_release,
	drive_status:		sr_drive_status,
	media_changed:		sr_media_change,
	tray_move:		sr_tray_move,
	lock_door:		sr_lock_door,
	select_speed:		sr_select_speed,
	get_last_session:	sr_get_last_session,
	get_mcn:		sr_get_mcn,
	reset:			sr_reset,
	audio_ioctl:		sr_audio_ioctl,
	dev_ioctl:		sr_dev_ioctl,
	capability:		CDC_CLOSE_TRAY | CDC_OPEN_TRAY | CDC_LOCK |
				CDC_SELECT_SPEED | CDC_SELECT_DISC |
				CDC_MULTI_SESSION | CDC_MCN |
				CDC_MEDIA_CHANGED | CDC_PLAY_AUDIO |
				CDC_RESET | CDC_IOCTLS | CDC_DRIVE_STATUS |
				CDC_CD_R | CDC_CD_RW | CDC_DVD | CDC_DVD_R |
				CDC_DVD_RAM | CDC_GENERIC_PACKET,
	generic_packet:		sr_packet,
};

/*
 * This function checks to see if the media has been changed in the
 * CDROM drive.  It is possible that we have already sensed a change,
 * or the drive may have sensed one and not yet reported it.  We must
 * be ready for either case. This function always reports the current
 * value of the changed bit.  If flag is 0, then the changed bit is reset.
 * This function could be done as an ioctl, but we would need to have
 * an inode for that to work, and we do not always have one.
 */

int sr_media_change(struct cdrom_device_info *cdi, int slot)
{
	int retval;

	if (CDSL_CURRENT != slot) {
		/* no changer support */
		return -EINVAL;
	}
	retval = scsi_ioctl(scsi_CDs[MINOR(cdi->dev)].device,
			    SCSI_IOCTL_TEST_UNIT_READY, 0);

	if (retval) {
		/* Unable to test, unit probably not ready.  This usually
		 * means there is no disc in the drive.  Mark as changed,
		 * and we will figure it out later once the drive is
		 * available again.  */

		scsi_CDs[MINOR(cdi->dev)].device->changed = 1;
		return 1;	/* This will force a flush, if called from
				 * check_disk_change */
	};

	retval = scsi_CDs[MINOR(cdi->dev)].device->changed;
	scsi_CDs[MINOR(cdi->dev)].device->changed = 0;
	/* If the disk changed, the capacity will now be different,
	 * so we force a re-read of this information */
	if (retval) {
		/* check multisession offset etc */
		sr_cd_check(cdi);

		/* 
		 * If the disk changed, the capacity will now be different,
		 * so we force a re-read of this information 
		 * Force 2048 for the sector size so that filesystems won't
		 * be trying to use something that is too small if the disc
		 * has changed.
		 */
		scsi_CDs[MINOR(cdi->dev)].needs_sector_size = 1;

		scsi_CDs[MINOR(cdi->dev)].device->sector_size = 2048;
	}
	return retval;
}

/*
 * rw_intr is the interrupt routine for the device driver.  It will be notified on the
 * end of a SCSI read / write, and will take on of several actions based on success or failure.
 */

static void rw_intr(Scsi_Cmnd * SCpnt)
{
	int result = SCpnt->result;
	int this_count = SCpnt->bufflen >> 9;
	int good_sectors = (result == 0 ? this_count : 0);
	int block_sectors = 0;
	int device_nr = DEVICE_NR(SCpnt->request.rq_dev);
	long error_sector;

#ifdef DEBUG
	printk("sr.c done: %x %p\n", result, SCpnt->request.bh->b_data);
#endif
	/*
	   Handle MEDIUM ERRORs or VOLUME OVERFLOWs that indicate partial success.
	   Since this is a relatively rare error condition, no care is taken to
	   avoid unnecessary additional work such as memcpy's that could be avoided.
	 */


	if (driver_byte(result) != 0 &&		/* An error occurred */
	    (SCpnt->sense_buffer[0] & 0x7f) == 0x70) {	/* Sense data is valid */
		switch (SCpnt->sense_buffer[2]) {
		case MEDIUM_ERROR:
		case VOLUME_OVERFLOW:
		case ILLEGAL_REQUEST:
			if (!(SCpnt->sense_buffer[0] & 0x80))
				break;
			error_sector = (SCpnt->sense_buffer[3] << 24) |
			(SCpnt->sense_buffer[4] << 16) |
			(SCpnt->sense_buffer[5] << 8) |
			SCpnt->sense_buffer[6];
			if (SCpnt->request.bh != NULL)
				block_sectors = SCpnt->request.bh->b_size >> 9;
			if (block_sectors < 4)
				block_sectors = 4;
			if (scsi_CDs[device_nr].device->sector_size == 2048)
				error_sector <<= 2;
			error_sector &= ~(block_sectors - 1);
			good_sectors = error_sector - SCpnt->request.sector;
			if (good_sectors < 0 || good_sectors >= this_count)
				good_sectors = 0;
			/*
			 * The SCSI specification allows for the value returned
			 * by READ CAPACITY to be up to 75 2K sectors past the
			 * last readable block.  Therefore, if we hit a medium
			 * error within the last 75 2K sectors, we decrease the
			 * saved size value.
			 */
			if ((error_sector >> 1) < sr_sizes[device_nr] &&
			    scsi_CDs[device_nr].capacity - error_sector < 4 *75)
				sr_sizes[device_nr] = error_sector >> 1;
			break;

		case RECOVERED_ERROR:
			/*
			 * An error occured, but it recovered.  Inform the
			 * user, but make sure that it's not treated as a
			 * hard error.
			 */
			print_sense("sr", SCpnt);
			result = 0;
			SCpnt->sense_buffer[0] = 0x0;
			good_sectors = this_count;
			break;

		default:
			break;
		}
	}

	/*
	 * This calls the generic completion function, now that we know
	 * how many actual sectors finished, and how many sectors we need
	 * to say have failed.
	 */
	scsi_io_completion(SCpnt, good_sectors, block_sectors);
}


static request_queue_t *sr_find_queue(kdev_t dev)
{
	/*
	 * No such device
	 */
	if (MINOR(dev) >= sr_template.dev_max || !scsi_CDs[MINOR(dev)].device)
		return NULL;

	return &scsi_CDs[MINOR(dev)].device->request_queue;
}

static int sr_scatter_pad(Scsi_Cmnd *SCpnt, int s_size)
{
	struct scatterlist *sg, *old_sg = NULL;
	int i, fsize, bsize, sg_ent, sg_count;
	char *front, *back;
	void **bbpnt, **old_bbpnt = NULL;

	back = front = NULL;
	sg_ent = SCpnt->use_sg;
	bsize = 0; /* gcc... */

	/*
	 * need front pad
	 */
	if ((fsize = SCpnt->request.sector % (s_size >> 9))) {
		fsize <<= 9;
		sg_ent++;
		if ((front = scsi_malloc(fsize)) == NULL)
			goto no_mem;
	}
	/*
	 * need a back pad too
	 */
	if ((bsize = s_size - ((SCpnt->request_bufflen + fsize) % s_size))) {
		sg_ent++;
		if ((back = scsi_malloc(bsize)) == NULL)
			goto no_mem;
	}

	/*
	 * extend or allocate new scatter-gather table
	 */
	sg_count = SCpnt->use_sg;
	if (sg_count) {
		old_sg = (struct scatterlist *) SCpnt->request_buffer;
		old_bbpnt = SCpnt->bounce_buffers;
	} else {
		sg_count = 1;
		sg_ent++;
	}

	/* Get space for scatterlist and bounce buffer array. */
	i  = sg_ent * sizeof(struct scatterlist);
	i += sg_ent * sizeof(void *);
	i  = (i + 511) & ~511;

	if ((sg = scsi_malloc(i)) == NULL)
		goto no_mem;

	bbpnt = (void **)
		((char *)sg + (sg_ent * sizeof(struct scatterlist)));

	/*
	 * no more failing memory allocs possible, we can safely assign
	 * SCpnt values now
	 */
	SCpnt->sglist_len = i;
	SCpnt->use_sg = sg_count;
	memset(sg, 0, SCpnt->sglist_len);

	i = 0;
	if (fsize) {
		sg[0].address = bbpnt[0] = front;
		sg[0].length = fsize;
		i++;
	}
	if (old_sg) {
		memcpy(sg + i, old_sg, SCpnt->use_sg * sizeof(struct scatterlist));
		if (old_bbpnt)
			memcpy(bbpnt + i, old_bbpnt, SCpnt->use_sg * sizeof(void *));
		scsi_free(old_sg, (((SCpnt->use_sg * sizeof(struct scatterlist)) +
				    (SCpnt->use_sg * sizeof(void *))) + 511) & ~511);
	} else {
		sg[i].address = SCpnt->request_buffer;
		sg[i].length = SCpnt->request_bufflen;
	}

	SCpnt->request_bufflen += (fsize + bsize);
	SCpnt->request_buffer = sg;
	SCpnt->bounce_buffers = bbpnt;
	SCpnt->use_sg += i;

	if (bsize) {
		sg[SCpnt->use_sg].address = back;
		bbpnt[SCpnt->use_sg] = back;
		sg[SCpnt->use_sg].length = bsize;
		SCpnt->use_sg++;
	}

	return 0;

no_mem:
	printk("sr: ran out of mem for scatter pad\n");
	if (front)
		scsi_free(front, fsize);
	if (back)
		scsi_free(back, bsize);

	return 1;
}


static int sr_init_command(Scsi_Cmnd * SCpnt)
{
	int dev, devm, block=0, this_count, s_size;

	devm = MINOR(SCpnt->request.rq_dev);
	dev = DEVICE_NR(SCpnt->request.rq_dev);

	SCSI_LOG_HLQUEUE(1, printk("Doing sr request, dev = %d, block = %d\n", devm, block));

	if (dev >= sr_template.nr_dev ||
	    !scsi_CDs[dev].device ||
	    !scsi_CDs[dev].device->online) {
		SCSI_LOG_HLQUEUE(2, printk("Finishing %ld sectors\n", SCpnt->request.nr_sectors));
		SCSI_LOG_HLQUEUE(2, printk("Retry with 0x%p\n", SCpnt));
		return 0;
	}
	if (scsi_CDs[dev].device->changed) {
		/*
		 * quietly refuse to do anything to a changed disc until the
		 * changed bit has been reset
		 */
		return 0;
	}

	if ((SCpnt->request.cmd == WRITE) && !scsi_CDs[dev].device->writeable)
		return 0;

	/*
	 * we do lazy blocksize switching (when reading XA sectors,
	 * see CDROMREADMODE2 ioctl) 
	 */
	s_size = scsi_CDs[dev].device->sector_size;
	if (s_size > 2048) {
		if (!in_interrupt())
			sr_set_blocklength(DEVICE_NR(CURRENT->rq_dev), 2048);
		else
			printk("sr: can't switch blocksize: in interrupt\n");
	}

	if (s_size != 512 && s_size != 1024 && s_size != 2048) {
		printk("sr: bad sector size %d\n", s_size);
		return 0;
	}

	block = SCpnt->request.sector / (s_size >> 9);

	/*
	 * request doesn't start on hw block boundary, add scatter pads
	 */
	if ((SCpnt->request.sector % (s_size >> 9)) || (SCpnt->request_bufflen % s_size))
		if (sr_scatter_pad(SCpnt, s_size))
			return 0;

	this_count = (SCpnt->request_bufflen >> 9) / (s_size >> 9);

	switch (SCpnt->request.cmd) {
	case WRITE:
		SCpnt->cmnd[0] = WRITE_10;
		SCpnt->sc_data_direction = SCSI_DATA_WRITE;
		break;
	case READ:
		SCpnt->cmnd[0] = READ_10;
		SCpnt->sc_data_direction = SCSI_DATA_READ;
		break;
	default:
		printk("Unknown sr command %d\n", SCpnt->request.cmd);
		return 0;
	}

	SCSI_LOG_HLQUEUE(2, printk("sr%d : %s %d/%ld 512 byte blocks.\n",
                                   devm,
		   (SCpnt->request.cmd == WRITE) ? "writing" : "reading",
				 this_count, SCpnt->request.nr_sectors));

	SCpnt->cmnd[1] = (SCpnt->device->scsi_level <= SCSI_2) ?
			 ((SCpnt->lun << 5) & 0xe0) : 0;

	if (this_count > 0xffff)
		this_count = 0xffff;

	SCpnt->cmnd[2] = (unsigned char) (block >> 24) & 0xff;
	SCpnt->cmnd[3] = (unsigned char) (block >> 16) & 0xff;
	SCpnt->cmnd[4] = (unsigned char) (block >> 8) & 0xff;
	SCpnt->cmnd[5] = (unsigned char) block & 0xff;
	SCpnt->cmnd[6] = SCpnt->cmnd[9] = 0;
	SCpnt->cmnd[7] = (unsigned char) (this_count >> 8) & 0xff;
	SCpnt->cmnd[8] = (unsigned char) this_count & 0xff;

	/*
	 * We shouldn't disconnect in the middle of a sector, so with a dumb
	 * host adapter, it's safe to assume that we can at least transfer
	 * this many bytes between each connect / disconnect.
	 */
	SCpnt->transfersize = scsi_CDs[dev].device->sector_size;
	SCpnt->underflow = this_count << 9;

	SCpnt->allowed = MAX_RETRIES;
	SCpnt->timeout_per_command = SR_TIMEOUT;

	/*
	 * This is the completion routine we use.  This is matched in terms
	 * of capability to this function.
	 */
	SCpnt->done = rw_intr;

	{
		struct scatterlist *sg = SCpnt->request_buffer;
		int i, size = 0;
		for (i = 0; i < SCpnt->use_sg; i++)
			size += sg[i].length;

		if (size != SCpnt->request_bufflen && SCpnt->use_sg) {
			printk("sr: mismatch count %d, bytes %d\n", size, SCpnt->request_bufflen);
			SCpnt->request_bufflen = size;
		}
	}

	/*
	 * This indicates that the command is ready from our end to be
	 * queued.
	 */
	return 1;
}

struct block_device_operations sr_bdops =
{
	owner:			THIS_MODULE,
	open:			cdrom_open,
	release:		cdrom_release,
	ioctl:			cdrom_ioctl,
	check_media_change:	cdrom_media_changed,
};

static int sr_open(struct cdrom_device_info *cdi, int purpose)
{
	check_disk_change(cdi->dev);

	if (MINOR(cdi->dev) >= sr_template.dev_max
	    || !scsi_CDs[MINOR(cdi->dev)].device) {
		return -ENXIO;	/* No such device */
	}
	/*
	 * If the device is in error recovery, wait until it is done.
	 * If the device is offline, then disallow any access to it.
	 */
	if (!scsi_block_when_processing_errors(scsi_CDs[MINOR(cdi->dev)].device)) {
		return -ENXIO;
	}
	scsi_CDs[MINOR(cdi->dev)].device->access_count++;
	if (scsi_CDs[MINOR(cdi->dev)].device->host->hostt->module)
		__MOD_INC_USE_COUNT(scsi_CDs[MINOR(cdi->dev)].device->host->hostt->module);
	if (sr_template.module)
		__MOD_INC_USE_COUNT(sr_template.module);

	/* If this device did not have media in the drive at boot time, then
	 * we would have been unable to get the sector size.  Check to see if
	 * this is the case, and try again.
	 */

	if (scsi_CDs[MINOR(cdi->dev)].needs_sector_size)
		get_sectorsize(MINOR(cdi->dev));

	return 0;
}

static int sr_detect(Scsi_Device * SDp)
{

	if (SDp->type != TYPE_ROM && SDp->type != TYPE_WORM)
		return 0;
	sr_template.dev_noticed++;
	return 1;
}

static int sr_attach(Scsi_Device * SDp)
{
	Scsi_CD *cpnt;
	int i;

	if (SDp->type != TYPE_ROM && SDp->type != TYPE_WORM)
		return 1;

	if (sr_template.nr_dev >= sr_template.dev_max) {
		SDp->attached--;
		return 1;
	}
	for (cpnt = scsi_CDs, i = 0; i < sr_template.dev_max; i++, cpnt++)
		if (!cpnt->device)
			break;

	if (i >= sr_template.dev_max)
		panic("scsi_devices corrupt (sr)");


	scsi_CDs[i].device = SDp;

	sr_template.nr_dev++;
	if (sr_template.nr_dev > sr_template.dev_max)
		panic("scsi_devices corrupt (sr)");

	printk("Attached scsi CD-ROM sr%d at scsi%d, channel %d, id %d, lun %d\n",
	       i, SDp->host->host_no, SDp->channel, SDp->id, SDp->lun);
	return 0;
}


void get_sectorsize(int i)
{
	unsigned char cmd[10];
	unsigned char *buffer;
	int the_result, retries;
	int sector_size;
	Scsi_Request *SRpnt;

	buffer = (unsigned char *) scsi_malloc(512);
	SRpnt = scsi_allocate_request(scsi_CDs[i].device);
	
	if(buffer == NULL || SRpnt == NULL)
	{
		scsi_CDs[i].capacity = 0x1fffff;
		sector_size = 2048;	/* A guess, just in case */
		scsi_CDs[i].needs_sector_size = 1;
		if(buffer)
			scsi_free(buffer, 512);
		if(SRpnt)
			scsi_release_request(SRpnt);
		return;
	}	

	retries = 3;
	do {
		cmd[0] = READ_CAPACITY;
		cmd[1] = (scsi_CDs[i].device->scsi_level <= SCSI_2) ?
			 ((scsi_CDs[i].device->lun << 5) & 0xe0) : 0;
		memset((void *) &cmd[2], 0, 8);
		SRpnt->sr_request.rq_status = RQ_SCSI_BUSY;	/* Mark as really busy */
		SRpnt->sr_cmd_len = 0;

		memset(buffer, 0, 8);

		/* Do the command and wait.. */

		SRpnt->sr_data_direction = SCSI_DATA_READ;
		scsi_wait_req(SRpnt, (void *) cmd, (void *) buffer,
			      8, SR_TIMEOUT, MAX_RETRIES);

		the_result = SRpnt->sr_result;
		retries--;

	} while (the_result && retries);


	scsi_release_request(SRpnt);
	SRpnt = NULL;

	if (the_result) {
		scsi_CDs[i].capacity = 0x1fffff;
		sector_size = 2048;	/* A guess, just in case */
		scsi_CDs[i].needs_sector_size = 1;
	} else {
#if 0
		if (cdrom_get_last_written(MKDEV(MAJOR_NR, i),
					   &scsi_CDs[i].capacity))
#endif
			scsi_CDs[i].capacity = 1 + ((buffer[0] << 24) |
						    (buffer[1] << 16) |
						    (buffer[2] << 8) |
						    buffer[3]);
		sector_size = (buffer[4] << 24) |
		    (buffer[5] << 16) | (buffer[6] << 8) | buffer[7];
		switch (sector_size) {
			/*
			 * HP 4020i CD-Recorder reports 2340 byte sectors
			 * Philips CD-Writers report 2352 byte sectors
			 *
			 * Use 2k sectors for them..
			 */
		case 0:
		case 2340:
		case 2352:
			sector_size = 2048;
			/* fall through */
		case 2048:
			scsi_CDs[i].capacity *= 4;
			/* fall through */
		case 512:
			break;
		default:
			printk("sr%d: unsupported sector size %d.\n",
			       i, sector_size);
			scsi_CDs[i].capacity = 0;
			scsi_CDs[i].needs_sector_size = 1;
		}

		scsi_CDs[i].device->sector_size = sector_size;

		/*
		 * Add this so that we have the ability to correctly gauge
		 * what the device is capable of.
		 */
		scsi_CDs[i].needs_sector_size = 0;
		sr_sizes[i] = scsi_CDs[i].capacity >> (BLOCK_SIZE_BITS - 9);
	};
	scsi_free(buffer, 512);
}

void get_capabilities(int i)
{
	unsigned char cmd[6];
	unsigned char *buffer;
	int rc, n;

	static char *loadmech[] =
	{
		"caddy",
		"tray",
		"pop-up",
		"",
		"changer",
		"cartridge changer",
		"",
		""
	};

	buffer = (unsigned char *) scsi_malloc(512);
	if (!buffer)
	{
		printk(KERN_ERR "sr: out of memory.\n");
		return;
	}
	cmd[0] = MODE_SENSE;
	cmd[1] = (scsi_CDs[i].device->scsi_level <= SCSI_2) ?
		 ((scsi_CDs[i].device->lun << 5) & 0xe0) : 0;
	cmd[2] = 0x2a;
	cmd[4] = 128;
	cmd[3] = cmd[5] = 0;
	rc = sr_do_ioctl(i, cmd, buffer, 128, 1, SCSI_DATA_READ, NULL);

	if (rc) {
		/* failed, drive doesn't have capabilities mode page */
		scsi_CDs[i].cdi.speed = 1;
		scsi_CDs[i].cdi.mask |= (CDC_CD_R | CDC_CD_RW | CDC_DVD_R |
					 CDC_DVD | CDC_DVD_RAM |
					 CDC_SELECT_DISC | CDC_SELECT_SPEED);
		scsi_free(buffer, 512);
		printk("sr%i: scsi-1 drive\n", i);
		return;
	}
	n = buffer[3] + 4;
	scsi_CDs[i].cdi.speed = ((buffer[n + 8] << 8) + buffer[n + 9]) / 176;
	scsi_CDs[i].readcd_known = 1;
	scsi_CDs[i].readcd_cdda = buffer[n + 5] & 0x01;
	/* print some capability bits */
	printk("sr%i: scsi3-mmc drive: %dx/%dx %s%s%s%s%s%s\n", i,
	       ((buffer[n + 14] << 8) + buffer[n + 15]) / 176,
	       scsi_CDs[i].cdi.speed,
	       buffer[n + 3] & 0x01 ? "writer " : "",	/* CD Writer */
	       buffer[n + 3] & 0x20 ? "dvd-ram " : "",
	       buffer[n + 2] & 0x02 ? "cd/rw " : "",	/* can read rewriteable */
	       buffer[n + 4] & 0x20 ? "xa/form2 " : "",		/* can read xa/from2 */
	       buffer[n + 5] & 0x01 ? "cdda " : "",	/* can read audio data */
	       loadmech[buffer[n + 6] >> 5]);
	if ((buffer[n + 6] >> 5) == 0)
		/* caddy drives can't close tray... */
		scsi_CDs[i].cdi.mask |= CDC_CLOSE_TRAY;
	if ((buffer[n + 2] & 0x8) == 0)
		/* not a DVD drive */
		scsi_CDs[i].cdi.mask |= CDC_DVD;
	if ((buffer[n + 3] & 0x20) == 0) {
		/* can't write DVD-RAM media */
		scsi_CDs[i].cdi.mask |= CDC_DVD_RAM;
	} else {
		scsi_CDs[i].device->writeable = 1;
	}
	if ((buffer[n + 3] & 0x10) == 0)
		/* can't write DVD-R media */
		scsi_CDs[i].cdi.mask |= CDC_DVD_R;
	if ((buffer[n + 3] & 0x2) == 0)
		/* can't write CD-RW media */
		scsi_CDs[i].cdi.mask |= CDC_CD_RW;
	if ((buffer[n + 3] & 0x1) == 0)
		/* can't write CD-R media */
		scsi_CDs[i].cdi.mask |= CDC_CD_R;
	if ((buffer[n + 6] & 0x8) == 0)
		/* can't eject */
		scsi_CDs[i].cdi.mask |= CDC_OPEN_TRAY;

	if ((buffer[n + 6] >> 5) == mechtype_individual_changer ||
	    (buffer[n + 6] >> 5) == mechtype_cartridge_changer)
		scsi_CDs[i].cdi.capacity =
		    cdrom_number_of_slots(&(scsi_CDs[i].cdi));
	if (scsi_CDs[i].cdi.capacity <= 1)
		/* not a changer */
		scsi_CDs[i].cdi.mask |= CDC_SELECT_DISC;
	/*else    I don't think it can close its tray
	   scsi_CDs[i].cdi.mask |= CDC_CLOSE_TRAY; */

	scsi_free(buffer, 512);
}

/*
 * sr_packet() is the entry point for the generic commands generated
 * by the Uniform CD-ROM layer. 
 */
static int sr_packet(struct cdrom_device_info *cdi, struct cdrom_generic_command *cgc)
{
	Scsi_Device *device = scsi_CDs[MINOR(cdi->dev)].device;

	/* set the LUN */
	if (device->scsi_level <= SCSI_2)
		cgc->cmd[1] |= device->lun << 5;

	cgc->stat = sr_do_ioctl(MINOR(cdi->dev), cgc->cmd, cgc->buffer, cgc->buflen, cgc->quiet, cgc->data_direction, cgc->sense);

	return cgc->stat;
}

static int sr_registered;

static int sr_init()
{
	int i;

	if (sr_template.dev_noticed == 0)
		return 0;

	if (!sr_registered) {
		if (devfs_register_blkdev(MAJOR_NR, "sr", &sr_bdops)) {
			printk("Unable to get major %d for SCSI-CD\n", MAJOR_NR);
			sr_template.dev_noticed = 0;
			return 1;
		}
		sr_registered++;
	}
	if (scsi_CDs)
		return 0;

	sr_template.dev_max = sr_template.dev_noticed + SR_EXTRA_DEVS;
	scsi_CDs = kmalloc(sr_template.dev_max * sizeof(Scsi_CD), GFP_ATOMIC);
	if (!scsi_CDs)
		goto cleanup_devfs;
	memset(scsi_CDs, 0, sr_template.dev_max * sizeof(Scsi_CD));

	sr_sizes = kmalloc(sr_template.dev_max * sizeof(int), GFP_ATOMIC);
	if (!sr_sizes)
		goto cleanup_cds;
	memset(sr_sizes, 0, sr_template.dev_max * sizeof(int));

	sr_blocksizes = kmalloc(sr_template.dev_max * sizeof(int), GFP_ATOMIC);
	if (!sr_blocksizes)
		goto cleanup_sizes;

	sr_hardsizes = kmalloc(sr_template.dev_max * sizeof(int), GFP_ATOMIC);
	if (!sr_hardsizes)
		goto cleanup_blocksizes;
	/*
	 * These are good guesses for the time being.
	 */
	for (i = 0; i < sr_template.dev_max; i++) {
		sr_blocksizes[i] = 2048;
		sr_hardsizes[i] = 2048;
        }
	blksize_size[MAJOR_NR] = sr_blocksizes;
        hardsect_size[MAJOR_NR] = sr_hardsizes;
	return 0;
cleanup_blocksizes:
	kfree(sr_blocksizes);
cleanup_sizes:
	kfree(sr_sizes);
cleanup_cds:
	kfree(scsi_CDs);
	scsi_CDs = NULL;
cleanup_devfs:
	devfs_unregister_blkdev(MAJOR_NR, "sr");
	sr_template.dev_noticed = 0;
	sr_registered--;
	return 1;
}

void sr_finish()
{
	int i;
	char name[6];

	blk_dev[MAJOR_NR].queue = sr_find_queue;
	blk_size[MAJOR_NR] = sr_sizes;

	for (i = 0; i < sr_template.nr_dev; ++i) {
		/* If we have already seen this, then skip it.  Comes up
		 * with loadable modules. */
		if (scsi_CDs[i].capacity)
			continue;
		scsi_CDs[i].capacity = 0x1fffff;
		scsi_CDs[i].device->sector_size = 2048;		/* A guess, just in case */
		scsi_CDs[i].needs_sector_size = 1;
		scsi_CDs[i].device->changed = 1;	/* force recheck CD type */
#if 0
		/* seems better to leave this for later */
		get_sectorsize(i);
		printk("Scd sectorsize = %d bytes.\n", scsi_CDs[i].sector_size);
#endif
		scsi_CDs[i].use = 1;

		scsi_CDs[i].device->ten = 1;
		scsi_CDs[i].device->remap = 1;
		scsi_CDs[i].readcd_known = 0;
		scsi_CDs[i].readcd_cdda = 0;
		sr_sizes[i] = scsi_CDs[i].capacity >> (BLOCK_SIZE_BITS - 9);

		scsi_CDs[i].cdi.ops = &sr_dops;
		scsi_CDs[i].cdi.handle = &scsi_CDs[i];
		scsi_CDs[i].cdi.dev = MKDEV(MAJOR_NR, i);
		scsi_CDs[i].cdi.mask = 0;
		scsi_CDs[i].cdi.capacity = 1;
		/*
		 *	FIXME: someone needs to handle a get_capabilities
		 *	failure properly ??
		 */
		get_capabilities(i);
		sr_vendor_init(i);

		sprintf(name, "sr%d", i);
		strcpy(scsi_CDs[i].cdi.name, name);
                scsi_CDs[i].cdi.de =
                    devfs_register (scsi_CDs[i].device->de, "cd",
                                    DEVFS_FL_DEFAULT, MAJOR_NR, i,
                                    S_IFBLK | S_IRUGO | S_IWUGO,
                                    &sr_bdops, NULL);
		register_cdrom(&scsi_CDs[i].cdi);
	}


	/* If our host adapter is capable of scatter-gather, then we increase
	 * the read-ahead to 16 blocks (32 sectors).  If not, we use
	 * a two block (4 sector) read ahead. */
	if (scsi_CDs[0].device && scsi_CDs[0].device->host->sg_tablesize)
		read_ahead[MAJOR_NR] = 32;	/* 32 sector read-ahead.  Always removable. */
	else
		read_ahead[MAJOR_NR] = 4;	/* 4 sector read-ahead */

	return;
}

static void sr_detach(Scsi_Device * SDp)
{
	Scsi_CD *cpnt;
	int i;

	if (scsi_CDs == NULL)
		return;
	for (cpnt = scsi_CDs, i = 0; i < sr_template.dev_max; i++, cpnt++)
		if (cpnt->device == SDp) {
			/*
			 * Since the cdrom is read-only, no need to sync the device.
			 * We should be kind to our buffer cache, however.
			 */
			invalidate_device(MKDEV(MAJOR_NR, i), 0);

			/*
			 * Reset things back to a sane state so that one can re-load a new
			 * driver (perhaps the same one).
			 */
			unregister_cdrom(&(cpnt->cdi));
			cpnt->device = NULL;
			cpnt->capacity = 0;
			SDp->attached--;
			sr_template.nr_dev--;
			sr_template.dev_noticed--;
			sr_sizes[i] = 0;
			return;
		}
	return;
}

static int __init init_sr(void)
{
	sr_template.module = THIS_MODULE;
	return scsi_register_module(MODULE_SCSI_DEV, &sr_template);
}

static void __exit exit_sr(void)
{
	scsi_unregister_module(MODULE_SCSI_DEV, &sr_template);
	devfs_unregister_blkdev(MAJOR_NR, "sr");
	sr_registered--;
	if (scsi_CDs != NULL) {
		kfree(scsi_CDs);

		kfree(sr_sizes);
		sr_sizes = NULL;

		kfree(sr_blocksizes);
		sr_blocksizes = NULL;
		kfree(sr_hardsizes);
		sr_hardsizes = NULL;
	}
	blksize_size[MAJOR_NR] = NULL;
        hardsect_size[MAJOR_NR] = NULL;
	blk_size[MAJOR_NR] = NULL;
	read_ahead[MAJOR_NR] = 0;

	sr_template.dev_max = 0;
}

module_init(init_sr);
module_exit(exit_sr);
MODULE_LICENSE("GPL");
