/*
	linux/kernel/blk_drv/mcd.c - Mitsumi CDROM driver

	Copyright (C) 1992  Martin Harriss
	Portions Copyright (C) 2001 Red Hat

	martin@bdsi.com (no longer valid - where are you now, Martin?)

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2, or (at your option)
	any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	HISTORY

	0.1	First attempt - internal use only
	0.2	Cleaned up delays and use of timer - alpha release
	0.3	Audio support added
	0.3.1 Changes for mitsumi CRMC LU005S march version
		   (stud11@cc4.kuleuven.ac.be)
        0.3.2 bug fixes to the ioctls and merged with ALPHA0.99-pl12
		   (Jon Tombs <jon@robots.ox.ac.uk>)
        0.3.3 Added more #defines and mcd_setup()
   		   (Jon Tombs <jon@gtex02.us.es>)

	October 1993 Bernd Huebner and Ruediger Helsch, Unifix Software GmbH,
	Braunschweig, Germany: rework to speed up data read operation.
	Also enabled definition of irq and address from bootstrap, using the
	environment.
	November 93 added code for FX001 S,D (single & double speed).
	February 94 added code for broken M 5/6 series of 16-bit single speed.


        0.4   
        Added support for loadable MODULEs, so mcd can now also be loaded by 
        insmod and removed by rmmod during runtime.
        Werner Zimmermann (zimmerma@rz.fht-esslingen.de), Mar. 26, 95

	0.5
	I added code for FX001 D to drop from double speed to single speed 
	when encountering errors... this helps with some "problematic" CD's
	that are supposedly "OUT OF TOLERANCE" (but are really shitty presses!)
	severely scratched, or possibly slightly warped! I have noticed that
	the Mitsumi 2x/4x drives are just less tolerant and the firmware is 
	not smart enough to drop speed,	so let's just kludge it with software!
	****** THE 4X SPEED MITSUMI DRIVES HAVE THE SAME PROBLEM!!!!!! ******
	Anyone want to "DONATE" one to me?! ;) I hear sometimes they are
	even WORSE! ;)
	** HINT... HINT... TAKE NOTES MITSUMI This could save some hassles with
	certain "large" CD's that have data on the outside edge in your 
	DOS DRIVERS .... Accuracy counts... speed is secondary ;)
	17 June 95 Modifications By Andrew J. Kroll <ag784@freenet.buffalo.edu>
	07 July 1995 Modifications by Andrew J. Kroll

	Bjorn Ekwall <bj0rn@blox.se> added unregister_blkdev to mcd_init()

	Michael K. Johnson <johnsonm@redhat.com> added retries on open
	for slow drives which take a while to recognize that they contain
	a CD.

	November 1997 -- ported to the Uniform CD-ROM driver by Erik Andersen.
	March    1999 -- made io base and irq CONFIG_ options (Tigran Aivazian).
	
	November 1999 -- Make kernel-parameter implementation work with 2.3.x 
	                 Removed init_module & cleanup_module in favor of 
			 module_init & module_exit.
			 Torben Mathiasen <tmm@image.dk>
		
	September 2001 - Reformatted and cleaned up the code
			 Alan Cox <alan@redhat.com>			 
*/

#include <linux/module.h>

#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/cdrom.h>
#include <linux/ioport.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/config.h>

/* #define REALLY_SLOW_IO  */
#include <asm/system.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#define MAJOR_NR MITSUMI_CDROM_MAJOR
#include <linux/blk.h>

#define mcd_port mcd		/* for compatible parameter passing with "insmod" */
#include "mcd.h"

static int mcd_blocksizes[1];


/* I added A flag to drop to 1x speed if too many errors 0 = 1X ; 1 = 2X */
static int mcdDouble;

/* How many sectors to hold at 1x speed counter */
static int mcd1xhold;

/* Is the drive connected properly and responding?? */
static int mcdPresent;

#define QUICK_LOOP_DELAY udelay(45)	/* use udelay */
#define QUICK_LOOP_COUNT 20

#define CURRENT_VALID \
(!QUEUE_EMPTY && MAJOR(CURRENT -> rq_dev) == MAJOR_NR && CURRENT -> cmd == READ \
&& CURRENT -> sector != -1)

#define MFL_STATUSorDATA (MFL_STATUS | MFL_DATA)
#define MCD_BUF_SIZ 16
static volatile int mcd_transfer_is_active;
static char mcd_buf[2048 * MCD_BUF_SIZ];	/* buffer for block size conversion */
static volatile int mcd_buf_bn[MCD_BUF_SIZ], mcd_next_bn;
static volatile int mcd_buf_in, mcd_buf_out = -1;
static volatile int mcd_error;
static int mcd_open_count;
enum mcd_state_e {
	MCD_S_IDLE,		/* 0 */
	MCD_S_START,		/* 1 */
	MCD_S_MODE,		/* 2 */
	MCD_S_READ,		/* 3 */
	MCD_S_DATA,		/* 4 */
	MCD_S_STOP,		/* 5 */
	MCD_S_STOPPING		/* 6 */
};
static volatile enum mcd_state_e mcd_state = MCD_S_IDLE;
static int mcd_mode = -1;
static int MCMD_DATA_READ = MCMD_PLAY_READ;

#define READ_TIMEOUT 3000

int mitsumi_bug_93_wait;

static short mcd_port = CONFIG_MCD_BASE;	/* used as "mcd" by "insmod" */
static int mcd_irq = CONFIG_MCD_IRQ;	/* must directly follow mcd_port */
MODULE_PARM(mcd, "1-2i");

static int McdTimeout, McdTries;
static DECLARE_WAIT_QUEUE_HEAD(mcd_waitq);

static struct mcd_DiskInfo DiskInfo;
static struct mcd_Toc Toc[MAX_TRACKS];
static struct mcd_Play_msf mcd_Play;

static int audioStatus;
static char mcdDiskChanged;
static char tocUpToDate;
static char mcdVersion;

static void mcd_transfer(void);
static void mcd_poll(unsigned long dummy);
static void mcd_invalidate_buffers(void);
static void hsg2msf(long hsg, struct msf *msf);
static void bin2bcd(unsigned char *p);
static int bcd2bin(unsigned char bcd);
static int mcdStatus(void);
static void sendMcdCmd(int cmd, struct mcd_Play_msf *params);
static int getMcdStatus(int timeout);
static int GetQChannelInfo(struct mcd_Toc *qp);
static int updateToc(void);
static int GetDiskInfo(void);
static int GetToc(void);
static int getValue(unsigned char *result);
static int mcd_open(struct cdrom_device_info *cdi, int purpose);
static void mcd_release(struct cdrom_device_info *cdi);
static int mcd_media_changed(struct cdrom_device_info *cdi, int disc_nr);
static int mcd_tray_move(struct cdrom_device_info *cdi, int position);
int mcd_audio_ioctl(struct cdrom_device_info *cdi, unsigned int cmd,
		    void *arg);
int mcd_drive_status(struct cdrom_device_info *cdi, int slot_nr);

struct block_device_operations mcd_bdops =
{
	owner:			THIS_MODULE,
	open:			cdrom_open,
	release:		cdrom_release,
	ioctl:			cdrom_ioctl,
	check_media_change:	cdrom_media_changed,
};

static struct timer_list mcd_timer;

static struct cdrom_device_ops mcd_dops = {
	open:mcd_open,
	release:mcd_release,
	drive_status:mcd_drive_status,
	media_changed:mcd_media_changed,
	tray_move:mcd_tray_move,
	audio_ioctl:mcd_audio_ioctl,
	capability:CDC_OPEN_TRAY | CDC_MEDIA_CHANGED |
	    CDC_PLAY_AUDIO | CDC_DRIVE_STATUS,
};

static struct cdrom_device_info mcd_info = {
	ops:&mcd_dops,
	speed:2,
	capacity:1,
	name:"mcd",
};

#ifndef MODULE
static int __init mcd_setup(char *str)
{
	int ints[9];

	(void) get_options(str, ARRAY_SIZE(ints), ints);

	if (ints[0] > 0)
		mcd_port = ints[1];
	if (ints[0] > 1)
		mcd_irq = ints[2];
	if (ints[0] > 2)
		mitsumi_bug_93_wait = ints[3];

	return 1;
}

__setup("mcd=", mcd_setup);

#endif				/* MODULE */

static int mcd_media_changed(struct cdrom_device_info *cdi, int disc_nr)
{
	return 0;
}


/*
 * Do a 'get status' command and get the result.  Only use from the top half
 * because it calls 'getMcdStatus' which sleeps.
 */

static int statusCmd(void)
{
	int st = -1, retry;

	for (retry = 0; retry < MCD_RETRY_ATTEMPTS; retry++) {
		/* send get-status cmd */
		outb(MCMD_GET_STATUS, MCDPORT(0));

		st = getMcdStatus(MCD_STATUS_DELAY);
		if (st != -1)
			break;
	}

	return st;
}


/*
 * Send a 'Play' command and get the status.  Use only from the top half.
 */

static int mcdPlay(struct mcd_Play_msf *arg)
{
	int retry, st = -1;

	for (retry = 0; retry < MCD_RETRY_ATTEMPTS; retry++) {
		sendMcdCmd(MCMD_PLAY_READ, arg);
		st = getMcdStatus(2 * MCD_STATUS_DELAY);
		if (st != -1)
			break;
	}

	return st;
}


static int mcd_tray_move(struct cdrom_device_info *cdi, int position)
{
	int i;
	if (position) {
		/*  Eject */
		/* all drives can at least stop! */
		if (audioStatus == CDROM_AUDIO_PLAY) {
			outb(MCMD_STOP, MCDPORT(0));
			i = getMcdStatus(MCD_STATUS_DELAY);
		}

		audioStatus = CDROM_AUDIO_NO_STATUS;

		outb(MCMD_EJECT, MCDPORT(0));
		/*
		 * the status (i) shows failure on all but the FX drives.
		 * But nothing we can do about that in software!
		 * So just read the status and forget it. - Jon.
		 */
		i = getMcdStatus(MCD_STATUS_DELAY);
		return 0;
	} else
		return -EINVAL;
}

long msf2hsg(struct msf *mp)
{
	return bcd2bin(mp->frame) + bcd2bin(mp->sec) * 75 + bcd2bin(mp->min) * 4500 - 150;
}


int mcd_audio_ioctl(struct cdrom_device_info *cdi, unsigned int cmd,
		    void *arg)
{
	int i, st;
	struct mcd_Toc qInfo;
	struct cdrom_ti *ti;
	struct cdrom_tochdr *tocHdr;
	struct cdrom_msf *msf;
	struct cdrom_subchnl *subchnl;
	struct cdrom_tocentry *entry;
	struct mcd_Toc *tocPtr;
	struct cdrom_volctrl *volctrl;

	st = statusCmd();
	if (st < 0)
		return -EIO;

	if (!tocUpToDate) {
		i = updateToc();
		if (i < 0)
			return i;	/* error reading TOC */
	}

	switch (cmd) {
	case CDROMSTART:	/* Spin up the drive */
		/* Don't think we can do this.  Even if we could,
		 * I think the drive times out and stops after a while
		 * anyway.  For now, ignore it.
		 */

		return 0;

	case CDROMSTOP:	/* Spin down the drive */
		outb(MCMD_STOP, MCDPORT(0));
		i = getMcdStatus(MCD_STATUS_DELAY);

		/* should we do anything if it fails? */

		audioStatus = CDROM_AUDIO_NO_STATUS;
		return 0;

	case CDROMPAUSE:	/* Pause the drive */
		if (audioStatus != CDROM_AUDIO_PLAY)
			return -EINVAL;

		outb(MCMD_STOP, MCDPORT(0));
		i = getMcdStatus(MCD_STATUS_DELAY);

		if (GetQChannelInfo(&qInfo) < 0) {
			/* didn't get q channel info */

			audioStatus = CDROM_AUDIO_NO_STATUS;
			return 0;
		}

		mcd_Play.start = qInfo.diskTime;	/* remember restart point */

		audioStatus = CDROM_AUDIO_PAUSED;
		return 0;

	case CDROMRESUME:	/* Play it again, Sam */
		if (audioStatus != CDROM_AUDIO_PAUSED)
			return -EINVAL;

		/* restart the drive at the saved position. */

		i = mcdPlay(&mcd_Play);
		if (i < 0) {
			audioStatus = CDROM_AUDIO_ERROR;
			return -EIO;
		}

		audioStatus = CDROM_AUDIO_PLAY;
		return 0;

	case CDROMPLAYTRKIND:	/* Play a track.  This currently ignores index. */

		ti = (struct cdrom_ti *) arg;

		if (ti->cdti_trk0 < DiskInfo.first
		    || ti->cdti_trk0 > DiskInfo.last
		    || ti->cdti_trk1 < ti->cdti_trk0) {
			return -EINVAL;
		}

		if (ti->cdti_trk1 > DiskInfo.last)
			ti->cdti_trk1 = DiskInfo.last;

		mcd_Play.start = Toc[ti->cdti_trk0].diskTime;
		mcd_Play.end = Toc[ti->cdti_trk1 + 1].diskTime;

#ifdef MCD_DEBUG
		printk("play: %02x:%02x.%02x to %02x:%02x.%02x\n",
		       mcd_Play.start.min, mcd_Play.start.sec,
		       mcd_Play.start.frame, mcd_Play.end.min,
		       mcd_Play.end.sec, mcd_Play.end.frame);
#endif

		i = mcdPlay(&mcd_Play);
		if (i < 0) {
			audioStatus = CDROM_AUDIO_ERROR;
			return -EIO;
		}

		audioStatus = CDROM_AUDIO_PLAY;
		return 0;

	case CDROMPLAYMSF:	/* Play starting at the given MSF address. */

		if (audioStatus == CDROM_AUDIO_PLAY) {
			outb(MCMD_STOP, MCDPORT(0));
			i = getMcdStatus(MCD_STATUS_DELAY);
			audioStatus = CDROM_AUDIO_NO_STATUS;
		}

		msf = (struct cdrom_msf *) arg;

		/* convert to bcd */

		bin2bcd(&msf->cdmsf_min0);
		bin2bcd(&msf->cdmsf_sec0);
		bin2bcd(&msf->cdmsf_frame0);
		bin2bcd(&msf->cdmsf_min1);
		bin2bcd(&msf->cdmsf_sec1);
		bin2bcd(&msf->cdmsf_frame1);

		mcd_Play.start.min = msf->cdmsf_min0;
		mcd_Play.start.sec = msf->cdmsf_sec0;
		mcd_Play.start.frame = msf->cdmsf_frame0;
		mcd_Play.end.min = msf->cdmsf_min1;
		mcd_Play.end.sec = msf->cdmsf_sec1;
		mcd_Play.end.frame = msf->cdmsf_frame1;

#ifdef MCD_DEBUG
		printk("play: %02x:%02x.%02x to %02x:%02x.%02x\n",
		       mcd_Play.start.min, mcd_Play.start.sec,
		       mcd_Play.start.frame, mcd_Play.end.min,
		       mcd_Play.end.sec, mcd_Play.end.frame);
#endif

		i = mcdPlay(&mcd_Play);
		if (i < 0) {
			audioStatus = CDROM_AUDIO_ERROR;
			return -EIO;
		}

		audioStatus = CDROM_AUDIO_PLAY;
		return 0;

	case CDROMREADTOCHDR:	/* Read the table of contents header */
		tocHdr = (struct cdrom_tochdr *) arg;
		tocHdr->cdth_trk0 = DiskInfo.first;
		tocHdr->cdth_trk1 = DiskInfo.last;
		return 0;

	case CDROMREADTOCENTRY:	/* Read an entry in the table of contents */
		entry = (struct cdrom_tocentry *) arg;
		if (entry->cdte_track == CDROM_LEADOUT)
			tocPtr = &Toc[DiskInfo.last - DiskInfo.first + 1];

		else if (entry->cdte_track > DiskInfo.last
			 || entry->cdte_track < DiskInfo.first)
			return -EINVAL;

		else
			tocPtr = &Toc[entry->cdte_track];

		entry->cdte_adr = tocPtr->ctrl_addr;
		entry->cdte_ctrl = tocPtr->ctrl_addr >> 4;

		if (entry->cdte_format == CDROM_LBA)
			entry->cdte_addr.lba = msf2hsg(&tocPtr->diskTime);

		else if (entry->cdte_format == CDROM_MSF) {
			entry->cdte_addr.msf.minute =
			    bcd2bin(tocPtr->diskTime.min);
			entry->cdte_addr.msf.second =
			    bcd2bin(tocPtr->diskTime.sec);
			entry->cdte_addr.msf.frame =
			    bcd2bin(tocPtr->diskTime.frame);
		}

		else
			return -EINVAL;

		return 0;

	case CDROMSUBCHNL:	/* Get subchannel info */

		subchnl = (struct cdrom_subchnl *) arg;
		if (GetQChannelInfo(&qInfo) < 0)
			return -EIO;

		subchnl->cdsc_audiostatus = audioStatus;
		subchnl->cdsc_adr = qInfo.ctrl_addr;
		subchnl->cdsc_ctrl = qInfo.ctrl_addr >> 4;
		subchnl->cdsc_trk = bcd2bin(qInfo.track);
		subchnl->cdsc_ind = bcd2bin(qInfo.pointIndex);
		subchnl->cdsc_absaddr.msf.minute = bcd2bin(qInfo.diskTime.min);
		subchnl->cdsc_absaddr.msf.second = bcd2bin(qInfo.diskTime.sec);
		subchnl->cdsc_absaddr.msf.frame  = bcd2bin(qInfo.diskTime.frame);
		subchnl->cdsc_reladdr.msf.minute = bcd2bin(qInfo.trackTime.min);
		subchnl->cdsc_reladdr.msf.second = bcd2bin(qInfo.trackTime.sec);
		subchnl->cdsc_reladdr.msf.frame  = bcd2bin(qInfo.trackTime.frame);
		return (0);

	case CDROMVOLCTRL:	/* Volume control */
		volctrl = (struct cdrom_volctrl *) arg;
		outb(MCMD_SET_VOLUME, MCDPORT(0));
		outb(volctrl->channel0, MCDPORT(0));
		outb(255, MCDPORT(0));
		outb(volctrl->channel1, MCDPORT(0));
		outb(255, MCDPORT(0));

		i = getMcdStatus(MCD_STATUS_DELAY);
		if (i < 0)
			return -EIO;

		{
			char a, b, c, d;

			getValue(&a);
			getValue(&b);
			getValue(&c);
			getValue(&d);
		}

		return 0;

	default:
		return -EINVAL;
	}
}

/*
 * Take care of the different block sizes between cdrom and Linux.
 * When Linux gets variable block sizes this will probably go away.
 */

static void mcd_transfer(void)
{
	if (CURRENT_VALID) {
		while (CURRENT->nr_sectors) {
			int bn = CURRENT->sector / 4;
			int i;
			for (i = 0; i < MCD_BUF_SIZ && mcd_buf_bn[i] != bn;
			     ++i);
			if (i < MCD_BUF_SIZ) {
				int offs =(i * 4 + (CURRENT->sector & 3)) * 512;
				int nr_sectors = 4 - (CURRENT->sector & 3);
				if (mcd_buf_out != i) {
					mcd_buf_out = i;
					if (mcd_buf_bn[i] != bn) {
						mcd_buf_out = -1;
						continue;
					}
				}
				if (nr_sectors > CURRENT->nr_sectors)
					nr_sectors = CURRENT->nr_sectors;
				memcpy(CURRENT->buffer, mcd_buf + offs,
				       nr_sectors * 512);
				CURRENT->nr_sectors -= nr_sectors;
				CURRENT->sector += nr_sectors;
				CURRENT->buffer += nr_sectors * 512;
			} else {
				mcd_buf_out = -1;
				break;
			}
		}
	}
}


/*
 * We only seem to get interrupts after an error.
 * Just take the interrupt and clear out the status reg.
 */

static void mcd_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	int st;

	st = inb(MCDPORT(1)) & 0xFF;
	test1(printk("<int1-%02X>", st));
	if (!(st & MFL_STATUS)) {
		st = inb(MCDPORT(0)) & 0xFF;
		test1(printk("<int0-%02X>", st));
		if ((st & 0xFF) != 0xFF)
			mcd_error = st ? st & 0xFF : -1;
	}
}


static void do_mcd_request(request_queue_t * q)
{
	test2(printk(" do_mcd_request(%ld+%ld)\n", CURRENT->sector,
	       CURRENT->nr_sectors));

		mcd_transfer_is_active = 1;
	while (CURRENT_VALID) {
		if (CURRENT->bh) {
			if (!buffer_locked(CURRENT->bh))
				panic(DEVICE_NAME ": block not locked");
		}
		mcd_transfer();
		if (CURRENT->nr_sectors == 0) {
			end_request(1);
		} else {
			mcd_buf_out = -1;	/* Want to read a block not in buffer */
			if (mcd_state == MCD_S_IDLE) {
				if (!tocUpToDate) {
					if (updateToc() < 0) {
						while (CURRENT_VALID)
							end_request(0);
						break;
					}
				}
				mcd_state = MCD_S_START;
				McdTries = 5;
				mcd_timer.function = mcd_poll;
				mod_timer(&mcd_timer, jiffies + 1);
			}
			break;
		}
	}
	mcd_transfer_is_active = 0;
	test2(printk(" do_mcd_request ends\n"));
}



static void mcd_poll(unsigned long dummy)
{
	int st;


	if (mcd_error) {
		if (mcd_error & 0xA5) {
			printk(KERN_ERR "mcd: I/O error 0x%02x", mcd_error);
			if (mcd_error & 0x80)
				printk(" (Door open)");
			if (mcd_error & 0x20)
				printk(" (Disk changed)");
			if (mcd_error & 0x04) {
				printk(" (Read error)");	/* Bitch about the problem. */

				/* Time to get fancy! If at 2x speed and 1 error, drop to 1x speed! */
				/* Interesting how it STAYS at MCD_RETRY_ATTEMPTS on first error! */
				/* But I find that rather HANDY!!! */
				/* Neat! it REALLY WORKS on those LOW QUALITY CD's!!! Smile! :) */
				/* AJK [06/17/95] */

				/* Slap the CD down to single speed! */
				if (mcdDouble == 1
				    && McdTries == MCD_RETRY_ATTEMPTS
				    && MCMD_DATA_READ == MCMD_2X_READ) {
					MCMD_DATA_READ = MCMD_PLAY_READ;	/* Uhhh, Ummmm, muhuh-huh! */
					mcd1xhold = SINGLE_HOLD_SECTORS;	/* Hey Beavis! */
					printk(" Speed now 1x");	/* Pull my finger! */
				}
			}
			printk("\n");
			mcd_invalidate_buffers();
#ifdef WARN_IF_READ_FAILURE
			if (McdTries == MCD_RETRY_ATTEMPTS)
				printk(KERN_ERR "mcd: read of block %d failed\n",
				       mcd_next_bn);
#endif
			if (!McdTries--) {
				/* Nuts! This cd is ready for recycling! */
				/* When WAS the last time YOU cleaned it CORRECTLY?! */
				printk(KERN_ERR "mcd: read of block %d failed, giving up\n",
				     mcd_next_bn);
				if (mcd_transfer_is_active) {
					McdTries = 0;
					goto ret;
				}
				if (CURRENT_VALID)
					end_request(0);
				McdTries = MCD_RETRY_ATTEMPTS;
			}
		}
		mcd_error = 0;
		mcd_state = MCD_S_STOP;
	}
	/* Switch back to Double speed if enough GOOD sectors were read! */

	/* Are we a double speed with a crappy CD?! */
	if (mcdDouble == 1 && McdTries == MCD_RETRY_ATTEMPTS
	    && MCMD_DATA_READ == MCMD_PLAY_READ) {
		/* We ARE a double speed and we ARE bitching! */
		if (mcd1xhold == 0) {	/* Okay, Like are we STILL at single speed? *//* We need to switch back to double speed now... */
			MCMD_DATA_READ = MCMD_2X_READ;	/* Uhhh... BACK You GO! */
			printk(KERN_INFO "mcd: Switching back to 2X speed!\n");	/* Tell 'em! */
		} else
			mcd1xhold--;	/* No?! Count down the good reads some more... */
		/* and try, try again! */
	}

immediately:
	switch (mcd_state) {
	case MCD_S_IDLE:
		test3(printk("MCD_S_IDLE\n"));
		goto out;

	case MCD_S_START:
		test3(printk("MCD_S_START\n"));
		outb(MCMD_GET_STATUS, MCDPORT(0));
		mcd_state = mcd_mode == 1 ? MCD_S_READ : MCD_S_MODE;
		McdTimeout = 3000;
		break;

	case MCD_S_MODE:
		test3(printk("MCD_S_MODE\n"));
		if ((st = mcdStatus()) != -1) {
			if (st & MST_DSK_CHG) {
				mcdDiskChanged = 1;
				tocUpToDate = 0;
				mcd_invalidate_buffers();
			}

set_mode_immediately:
			if ((st & MST_DOOR_OPEN) || !(st & MST_READY)) {
				mcdDiskChanged = 1;
				tocUpToDate = 0;
				if (mcd_transfer_is_active) {
					mcd_state = MCD_S_START;
					goto immediately;
				}
				printk(KERN_INFO);
				printk((st & MST_DOOR_OPEN) ?
				       "mcd: door open\n" :
				       "mcd: disk removed\n");
				mcd_state = MCD_S_IDLE;
				while (CURRENT_VALID)
					end_request(0);
				goto out;
			}
			outb(MCMD_SET_MODE, MCDPORT(0));
			outb(1, MCDPORT(0));
			mcd_mode = 1;
			mcd_state = MCD_S_READ;
			McdTimeout = 3000;
		}
		break;

	case MCD_S_READ:
		test3(printk("MCD_S_READ\n"));
		if ((st = mcdStatus()) != -1) {
			if (st & MST_DSK_CHG) {
				mcdDiskChanged = 1;
				tocUpToDate = 0;
				mcd_invalidate_buffers();
			}

read_immediately:
			if ((st & MST_DOOR_OPEN) || !(st & MST_READY)) {
				mcdDiskChanged = 1;
				tocUpToDate = 0;
				if (mcd_transfer_is_active) {
					mcd_state = MCD_S_START;
					goto immediately;
				}
				printk(KERN_INFO);
				printk((st & MST_DOOR_OPEN) ?
				       "mcd: door open\n" :
				       "mcd: disk removed\n");
				mcd_state = MCD_S_IDLE;
				while (CURRENT_VALID)
					end_request(0);
				goto out;
			}

			if (CURRENT_VALID) {
				struct mcd_Play_msf msf;
				mcd_next_bn = CURRENT->sector / 4;
				hsg2msf(mcd_next_bn, &msf.start);
				msf.end.min = ~0;
				msf.end.sec = ~0;
				msf.end.frame = ~0;
				sendMcdCmd(MCMD_DATA_READ, &msf);
				mcd_state = MCD_S_DATA;
				McdTimeout = READ_TIMEOUT;
			} else {
				mcd_state = MCD_S_STOP;
				goto immediately;
			}

		}
		break;

	case MCD_S_DATA:
		test3(printk("MCD_S_DATA\n"));
		st = inb(MCDPORT(1)) & (MFL_STATUSorDATA);
data_immediately:
		test5(printk("Status %02x\n", st))
		switch (st) {
		case MFL_DATA:
#ifdef WARN_IF_READ_FAILURE
			if (McdTries == 5)
				printk(KERN_WARNING "mcd: read of block %d failed\n",
				       mcd_next_bn);
#endif
			if (!McdTries--) {
				printk(KERN_ERR "mcd: read of block %d failed, giving up\n", mcd_next_bn);
				if (mcd_transfer_is_active) {
					McdTries = 0;
					break;
				}
				if (CURRENT_VALID)
					end_request(0);
				McdTries = 5;
			}
			mcd_state = MCD_S_START;
			McdTimeout = READ_TIMEOUT;
			goto immediately;

		case MFL_STATUSorDATA:
			break;

		default:
			McdTries = 5;
			if (!CURRENT_VALID && mcd_buf_in == mcd_buf_out) {
				mcd_state = MCD_S_STOP;
				goto immediately;
			}
			mcd_buf_bn[mcd_buf_in] = -1;
			insb(MCDPORT(0), mcd_buf + 2048 * mcd_buf_in,
				  2048);
			mcd_buf_bn[mcd_buf_in] = mcd_next_bn++;
			if (mcd_buf_out == -1)
				mcd_buf_out = mcd_buf_in;
			mcd_buf_in = mcd_buf_in + 1 == MCD_BUF_SIZ ? 0 : mcd_buf_in + 1;
			if (!mcd_transfer_is_active) {
				while (CURRENT_VALID) {
					mcd_transfer();
					if (CURRENT->nr_sectors == 0)
						end_request(1);
					else
						break;
				}
			}

			if (CURRENT_VALID
			    && (CURRENT->sector / 4 < mcd_next_bn ||
				CURRENT->sector / 4 > mcd_next_bn + 16)) {
				mcd_state = MCD_S_STOP;
				goto immediately;
			}
			McdTimeout = READ_TIMEOUT;
			{
				int count = QUICK_LOOP_COUNT;
				while (count--) {
					QUICK_LOOP_DELAY;
					if ((st = (inb(MCDPORT(1))) & (MFL_STATUSorDATA)) != (MFL_STATUSorDATA)) {
						test4(printk(" %d ", QUICK_LOOP_COUNT - count));
						goto data_immediately;
					}
				}
				test4(printk("ended "));
			}
			break;
		}
		break;

	case MCD_S_STOP:
		test3(printk("MCD_S_STOP\n"));
		if (!mitsumi_bug_93_wait)
			goto do_not_work_around_mitsumi_bug_93_1;

		McdTimeout = mitsumi_bug_93_wait;
		mcd_state = 9 + 3 + 1;
		break;

	case 9 + 3 + 1:
		if (McdTimeout)
			break;

do_not_work_around_mitsumi_bug_93_1:
		outb(MCMD_STOP, MCDPORT(0));
		if ((inb(MCDPORT(1)) & MFL_STATUSorDATA) == MFL_STATUS) {
			int i = 4096;
			do {
				inb(MCDPORT(0));
			} while ((inb(MCDPORT(1)) & MFL_STATUSorDATA) == MFL_STATUS && --i);
			outb(MCMD_STOP, MCDPORT(0));
			if ((inb(MCDPORT(1)) & MFL_STATUSorDATA) == MFL_STATUS) {
				i = 4096;
				do {
					inb(MCDPORT(0));
				} while ((inb(MCDPORT(1)) & MFL_STATUSorDATA) == MFL_STATUS && --i);
				outb(MCMD_STOP, MCDPORT(0));
			}
		}

		mcd_state = MCD_S_STOPPING;
		McdTimeout = 1000;
		break;

	case MCD_S_STOPPING:
		test3(printk("MCD_S_STOPPING\n"));
		if ((st = mcdStatus()) == -1 && McdTimeout)
			break;

		if ((st != -1) && (st & MST_DSK_CHG)) {
			mcdDiskChanged = 1;
			tocUpToDate = 0;
			mcd_invalidate_buffers();
		}
		if (!mitsumi_bug_93_wait)
			goto do_not_work_around_mitsumi_bug_93_2;

		McdTimeout = mitsumi_bug_93_wait;
		mcd_state = 9 + 3 + 2;
		break;

	case 9 + 3 + 2:
		if (McdTimeout)
			break;
		st = -1;

do_not_work_around_mitsumi_bug_93_2:
		test3(printk("CURRENT_VALID %d mcd_mode %d\n", CURRENT_VALID, mcd_mode));
		if (CURRENT_VALID) {
			if (st != -1) {
				if (mcd_mode == 1)
					goto read_immediately;
				else
					goto set_mode_immediately;
			} else {
				mcd_state = MCD_S_START;
				McdTimeout = 1;
			}
		} else {
			mcd_state = MCD_S_IDLE;
			goto out;
		}
		break;
	default:
		printk(KERN_ERR "mcd: invalid state %d\n", mcd_state);
		goto out;
	}
ret:
	if (!McdTimeout--) {
		printk(KERN_WARNING "mcd: timeout in state %d\n", mcd_state);
		mcd_state = MCD_S_STOP;
	}
	mcd_timer.function = mcd_poll;
	mod_timer(&mcd_timer, jiffies + 1);
out:
	return;
}

static void mcd_invalidate_buffers(void)
{
	int i;
	for (i = 0; i < MCD_BUF_SIZ; ++i)
		mcd_buf_bn[i] = -1;
	mcd_buf_out = -1;
}

/*
 * Open the device special file.  Check that a disk is in.
 */
static int mcd_open(struct cdrom_device_info *cdi, int purpose)
{
	int st, count = 0;
	if (mcdPresent == 0)
		return -ENXIO;	/* no hardware */

	if (mcd_open_count || mcd_state != MCD_S_IDLE)
		goto bump_count;

	mcd_invalidate_buffers();
	do {
		st = statusCmd();	/* check drive status */
		if (st == -1)
			goto err_out;	/* drive doesn't respond */
		if ((st & MST_READY) == 0) {	/* no disk? wait a sec... */
			current->state = TASK_INTERRUPTIBLE;
			schedule_timeout(HZ);
		}
	} while (((st & MST_READY) == 0) && count++ < MCD_RETRY_ATTEMPTS);

	if (updateToc() < 0)
		goto err_out;

bump_count:
	++mcd_open_count;
	return 0;

err_out:
	return -EIO;
}


/*
 * On close, we flush all mcd blocks from the buffer cache.
 */
static void mcd_release(struct cdrom_device_info *cdi)
{
	if (!--mcd_open_count) {
		mcd_invalidate_buffers();
	}
}



/* This routine gets called during initialization if things go wrong,
 * and is used in mcd_exit as well. */
static void cleanup(int level)
{
	switch (level) {
	case 3:
		if (unregister_cdrom(&mcd_info)) {
			printk(KERN_WARNING "Can't unregister cdrom mcd\n");
			return;
		}
		free_irq(mcd_irq, NULL);
	case 2:
		release_region(mcd_port, 4);
	case 1:
		if (devfs_unregister_blkdev(MAJOR_NR, "mcd")) {
			printk(KERN_WARNING "Can't unregister major mcd\n");
			return;
		}
		blk_cleanup_queue(BLK_DEFAULT_QUEUE(MAJOR_NR));
	default:;
	}
}



/*
 * Test for presence of drive and initialize it.  Called at boot time.
 */

int __init mcd_init(void)
{
	int count;
	unsigned char result[3];
	char msg[80];

	if (mcd_port <= 0 || mcd_irq <= 0) {
		printk(KERN_INFO "mcd: not probing.\n");
		return -EIO;
	}

	if (devfs_register_blkdev(MAJOR_NR, "mcd", &mcd_bdops) != 0) {
		printk(KERN_ERR "mcd: Unable to get major %d for Mitsumi CD-ROM\n", MAJOR_NR);
		return -EIO;
	}
	if (check_region(mcd_port, 4)) {
		cleanup(1);
		printk(KERN_ERR "mcd: Initialization failed, I/O port (%X) already in use\n", mcd_port);
		return -EIO;
	}

	blksize_size[MAJOR_NR] = mcd_blocksizes;
	blk_init_queue(BLK_DEFAULT_QUEUE(MAJOR_NR), DEVICE_REQUEST);
	read_ahead[MAJOR_NR] = 4;

	/* check for card */

	outb(0, MCDPORT(1));	/* send reset */
	for (count = 0; count < 2000000; count++)
		(void) inb(MCDPORT(1));	/* delay a bit */

	outb(0x40, MCDPORT(0));	/* send get-stat cmd */
	for (count = 0; count < 2000000; count++)
		if (!(inb(MCDPORT(1)) & MFL_STATUS))
			break;

	if (count >= 2000000) {
		printk(KERN_INFO "mcd: initialisation failed - No mcd device at 0x%x irq %d\n",
		       mcd_port, mcd_irq);
		cleanup(1);
		return -EIO;
	}
	count = inb(MCDPORT(0));	/* pick up the status */

	outb(MCMD_GET_VERSION, MCDPORT(0));
	for (count = 0; count < 3; count++)
		if (getValue(result + count)) {
			printk(KERN_ERR "mcd: mitsumi get version failed at 0x%x\n",
			       mcd_port);
			cleanup(1);
			return -EIO;
		}

	if (result[0] == result[1] && result[1] == result[2]) {
		cleanup(1);
		return -EIO;
	}

	mcdVersion = result[2];

	if (mcdVersion >= 4)
		outb(4, MCDPORT(2));	/* magic happens */

	/* don't get the IRQ until we know for sure the drive is there */

	if (request_irq(mcd_irq, mcd_interrupt, SA_INTERRUPT, "Mitsumi CD", NULL)) {
		printk(KERN_ERR "mcd: Unable to get IRQ%d for Mitsumi CD-ROM\n", mcd_irq);
		cleanup(1);
		return -EIO;
	}

	if (result[1] == 'D') {
		MCMD_DATA_READ = MCMD_2X_READ;
		/* Added flag to drop to 1x speed if too many errors */
		mcdDouble = 1;
	} else
		mcd_info.speed = 1;
	sprintf(msg, " mcd: Mitsumi %s Speed CD-ROM at port=0x%x,"
		" irq=%d\n", mcd_info.speed == 1 ? "Single" : "Double",
		mcd_port, mcd_irq);

	request_region(mcd_port, 4, "mcd");

	outb(MCMD_CONFIG_DRIVE, MCDPORT(0));
	outb(0x02, MCDPORT(0));
	outb(0x00, MCDPORT(0));
	getValue(result);

	outb(MCMD_CONFIG_DRIVE, MCDPORT(0));
	outb(0x10, MCDPORT(0));
	outb(0x04, MCDPORT(0));
	getValue(result);

	mcd_invalidate_buffers();
	mcdPresent = 1;

	mcd_info.dev = MKDEV(MAJOR_NR, 0);

	if (register_cdrom(&mcd_info) != 0) {
		printk(KERN_ERR "mcd: Unable to register Mitsumi CD-ROM.\n");
		cleanup(3);
		return -EIO;
	}
	devfs_plain_cdrom(&mcd_info, &mcd_bdops);
	printk(msg);

	return 0;
}


static void hsg2msf(long hsg, struct msf *msf)
{
	hsg += 150;
	msf->min = hsg / 4500;
	hsg %= 4500;
	msf->sec = hsg / 75;
	msf->frame = hsg % 75;

	bin2bcd(&msf->min);	/* convert to BCD */
	bin2bcd(&msf->sec);
	bin2bcd(&msf->frame);
}


static void bin2bcd(unsigned char *p)
{
	int u, t;

	u = *p % 10;
	t = *p / 10;
	*p = u | (t << 4);
}

static int bcd2bin(unsigned char bcd)
{
	return (bcd >> 4) * 10 + (bcd & 0xF);
}


/*
 * See if a status is ready from the drive and return it
 * if it is ready.
 */

static int mcdStatus(void)
{
	int i;
	int st;

	st = inb(MCDPORT(1)) & MFL_STATUS;
	if (!st) {
		i = inb(MCDPORT(0)) & 0xFF;
		return i;
	} else
		return -1;
}


/*
 * Send a play or read command to the drive
 */

static void sendMcdCmd(int cmd, struct mcd_Play_msf *params)
{
	outb(cmd, MCDPORT(0));
	outb(params->start.min, MCDPORT(0));
	outb(params->start.sec, MCDPORT(0));
	outb(params->start.frame, MCDPORT(0));
	outb(params->end.min, MCDPORT(0));
	outb(params->end.sec, MCDPORT(0));
	outb(params->end.frame, MCDPORT(0));
}


/*
 * Timer interrupt routine to test for status ready from the drive.
 * (see the next routine)
 */

static void mcdStatTimer(unsigned long dummy)
{
	if (!(inb(MCDPORT(1)) & MFL_STATUS)) {
		wake_up(&mcd_waitq);
		return;
	}

	McdTimeout--;
	if (McdTimeout <= 0) {
		wake_up(&mcd_waitq);
		return;
	}
	mcd_timer.function = mcdStatTimer;
	mod_timer(&mcd_timer, jiffies + 1);
}


/*
 * Wait for a status to be returned from the drive.  The actual test
 * (see routine above) is done by the timer interrupt to avoid
 * excessive rescheduling.
 */

static int getMcdStatus(int timeout)
{
	int st;

	McdTimeout = timeout;
	mcd_timer.function = mcdStatTimer;
	mod_timer(&mcd_timer, jiffies + 1);
	sleep_on(&mcd_waitq);
	if (McdTimeout <= 0)
		return -1;

	st = inb(MCDPORT(0)) & 0xFF;
	if (st == 0xFF)
		return -1;

	if ((st & MST_BUSY) == 0 && audioStatus == CDROM_AUDIO_PLAY)
		/* XXX might be an error? look at q-channel? */
		audioStatus = CDROM_AUDIO_COMPLETED;

	if (st & MST_DSK_CHG) {
		mcdDiskChanged = 1;
		tocUpToDate = 0;
		audioStatus = CDROM_AUDIO_NO_STATUS;
	}

	return st;
}


/* gives current state of the drive This function is quite unreliable, 
   and should probably be rewritten by someone, eventually... */

int mcd_drive_status(struct cdrom_device_info *cdi, int slot_nr)
{
	int st;

	st = statusCmd();	/* check drive status */
	if (st == -1)
		return -EIO;	/* drive doesn't respond */
	if ((st & MST_READY))
		return CDS_DISC_OK;
	if ((st & MST_DOOR_OPEN))
		return CDS_TRAY_OPEN;
	if ((st & MST_DSK_CHG))
		return CDS_NO_DISC;
	if ((st & MST_BUSY))
		return CDS_DRIVE_NOT_READY;
	return -EIO;
}


/*
 * Read a value from the drive.
 */

static int getValue(unsigned char *result)
{
	int count;
	int s;

	for (count = 0; count < 2000; count++)
		if (!(inb(MCDPORT(1)) & MFL_STATUS))
			break;

	if (count >= 2000) {
		printk("mcd: getValue timeout\n");
		return -1;
	}

	s = inb(MCDPORT(0)) & 0xFF;
	*result = (unsigned char) s;
	return 0;
}

/*
 * Read the current Q-channel info.  Also used for reading the
 * table of contents.
 */

int GetQChannelInfo(struct mcd_Toc *qp)
{
	unsigned char notUsed;
	int retry;

	for (retry = 0; retry < MCD_RETRY_ATTEMPTS; retry++) {
		outb(MCMD_GET_Q_CHANNEL, MCDPORT(0));
		if (getMcdStatus(MCD_STATUS_DELAY) != -1)
			break;
	}

	if (retry >= MCD_RETRY_ATTEMPTS)
		return -1;

	if (getValue(&qp->ctrl_addr) < 0)
		return -1;
	if (getValue(&qp->track) < 0)
		return -1;
	if (getValue(&qp->pointIndex) < 0)
		return -1;
	if (getValue(&qp->trackTime.min) < 0)
		return -1;
	if (getValue(&qp->trackTime.sec) < 0)
		return -1;
	if (getValue(&qp->trackTime.frame) < 0)
		return -1;
	if (getValue(&notUsed) < 0)
		return -1;
	if (getValue(&qp->diskTime.min) < 0)
		return -1;
	if (getValue(&qp->diskTime.sec) < 0)
		return -1;
	if (getValue(&qp->diskTime.frame) < 0)
		return -1;

	return 0;
}

/*
 * Read the table of contents (TOC) and TOC header if necessary
 */

static int updateToc(void)
{
	if (tocUpToDate)
		return 0;

	if (GetDiskInfo() < 0)
		return -EIO;

	if (GetToc() < 0)
		return -EIO;

	tocUpToDate = 1;
	return 0;
}

/*
 * Read the table of contents header
 */

static int GetDiskInfo(void)
{
	int retry;

	for (retry = 0; retry < MCD_RETRY_ATTEMPTS; retry++) {
		outb(MCMD_GET_DISK_INFO, MCDPORT(0));
		if (getMcdStatus(MCD_STATUS_DELAY) != -1)
			break;
	}

	if (retry >= MCD_RETRY_ATTEMPTS)
		return -1;

	if (getValue(&DiskInfo.first) < 0)
		return -1;
	if (getValue(&DiskInfo.last) < 0)
		return -1;

	DiskInfo.first = bcd2bin(DiskInfo.first);
	DiskInfo.last = bcd2bin(DiskInfo.last);

#ifdef MCD_DEBUG
	printk
	    ("Disk Info: first %d last %d length %02x:%02x.%02x first %02x:%02x.%02x\n",
	     DiskInfo.first, DiskInfo.last, DiskInfo.diskLength.min,
	     DiskInfo.diskLength.sec, DiskInfo.diskLength.frame,
	     DiskInfo.firstTrack.min, DiskInfo.firstTrack.sec,
	     DiskInfo.firstTrack.frame);
#endif

	if (getValue(&DiskInfo.diskLength.min) < 0)
		return -1;
	if (getValue(&DiskInfo.diskLength.sec) < 0)
		return -1;
	if (getValue(&DiskInfo.diskLength.frame) < 0)
		return -1;
	if (getValue(&DiskInfo.firstTrack.min) < 0)
		return -1;
	if (getValue(&DiskInfo.firstTrack.sec) < 0)
		return -1;
	if (getValue(&DiskInfo.firstTrack.frame) < 0)
		return -1;

	return 0;
}

/*
 * Read the table of contents (TOC)
 */

static int GetToc(void)
{
	int i, px;
	int limit;
	int retry;
	struct mcd_Toc qInfo;

	for (i = 0; i < MAX_TRACKS; i++)
		Toc[i].pointIndex = 0;

	i = DiskInfo.last + 3;

	for (retry = 0; retry < MCD_RETRY_ATTEMPTS; retry++) {
		outb(MCMD_STOP, MCDPORT(0));
		if (getMcdStatus(MCD_STATUS_DELAY) != -1)
			break;
	}

	if (retry >= MCD_RETRY_ATTEMPTS)
		return -1;

	for (retry = 0; retry < MCD_RETRY_ATTEMPTS; retry++) {
		outb(MCMD_SET_MODE, MCDPORT(0));
		outb(0x05, MCDPORT(0));	/* mode: toc */
		mcd_mode = 0x05;
		if (getMcdStatus(MCD_STATUS_DELAY) != -1)
			break;
	}

	if (retry >= MCD_RETRY_ATTEMPTS)
		return -1;

	for (limit = 300; limit > 0; limit--) {
		if (GetQChannelInfo(&qInfo) < 0)
			break;

		px = bcd2bin(qInfo.pointIndex);
		if (px > 0 && px < MAX_TRACKS && qInfo.track == 0)
			if (Toc[px].pointIndex == 0) {
				Toc[px] = qInfo;
				i--;
			}

		if (i <= 0)
			break;
	}

	Toc[DiskInfo.last + 1].diskTime = DiskInfo.diskLength;

	for (retry = 0; retry < MCD_RETRY_ATTEMPTS; retry++) {
		outb(MCMD_SET_MODE, MCDPORT(0));
		outb(0x01, MCDPORT(0));
		mcd_mode = 1;
		if (getMcdStatus(MCD_STATUS_DELAY) != -1)
			break;
	}

#ifdef MCD_DEBUG
	for (i = 1; i <= DiskInfo.last; i++)
		printk
		    ("i = %2d ctl-adr = %02X track %2d px %02X %02X:%02X.%02X    %02X:%02X.%02X\n",
		     i, Toc[i].ctrl_addr, Toc[i].track, Toc[i].pointIndex,
		     Toc[i].trackTime.min, Toc[i].trackTime.sec,
		     Toc[i].trackTime.frame, Toc[i].diskTime.min,
		     Toc[i].diskTime.sec, Toc[i].diskTime.frame);
	for (i = 100; i < 103; i++)
		printk
		    ("i = %2d ctl-adr = %02X track %2d px %02X %02X:%02X.%02X    %02X:%02X.%02X\n",
		     i, Toc[i].ctrl_addr, Toc[i].track, Toc[i].pointIndex,
		     Toc[i].trackTime.min, Toc[i].trackTime.sec,
		     Toc[i].trackTime.frame, Toc[i].diskTime.min,
		     Toc[i].diskTime.sec, Toc[i].diskTime.frame);
#endif

	return limit > 0 ? 0 : -1;
}

void __exit mcd_exit(void)
{
	cleanup(3);
	del_timer_sync(&mcd_timer);
}

#ifdef MODULE
module_init(mcd_init);
#endif
module_exit(mcd_exit);

MODULE_AUTHOR("Martin Harriss");
MODULE_LICENSE("GPL");
