/*
 * Implementation of SCSI Direct Access Peripheral driver for CAM.
 *
 * Copyright (c) 1997 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/cam/scsi/scsi_da.c,v 1.42.2.5 2000/09/11 05:40:20 ken Exp $
 */

#ifdef _KERNEL
#include "opt_hw_wdog.h"
#endif /* _KERNEL */

#include <sys/param.h>

#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#endif /* _KERNEL */

#include <sys/devicestat.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/cons.h>

#include <machine/md_var.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#ifndef _KERNEL
#include <stdio.h>
#include <string.h>
#endif /* _KERNEL */

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_extend.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_periph.h>

#include <cam/scsi/scsi_message.h>

#ifndef _KERNEL 
#include <cam/scsi/scsi_da.h>
#endif /* !_KERNEL */

#ifdef _KERNEL
typedef enum {
	DA_STATE_PROBE,
	DA_STATE_NORMAL
} da_state;

typedef enum {
	DA_FLAG_PACK_INVALID	= 0x001,
	DA_FLAG_NEW_PACK	= 0x002,
	DA_FLAG_PACK_LOCKED	= 0x004,
	DA_FLAG_PACK_REMOVABLE	= 0x008,
	DA_FLAG_TAGGED_QUEUING	= 0x010,
	DA_FLAG_NEED_OTAG	= 0x020,
	DA_FLAG_WENT_IDLE	= 0x040,
	DA_FLAG_RETRY_UA	= 0x080,
	DA_FLAG_OPEN		= 0x100
} da_flags;

typedef enum {
	DA_Q_NONE		= 0x00,
	DA_Q_NO_SYNC_CACHE	= 0x01,
	DA_Q_NO_6_BYTE		= 0x02
} da_quirks;

typedef enum {
	DA_CCB_PROBE		= 0x01,
	DA_CCB_BUFFER_IO	= 0x02,
	DA_CCB_WAITING		= 0x03,
	DA_CCB_DUMP		= 0x04,
	DA_CCB_TYPE_MASK	= 0x0F,
	DA_CCB_RETRY_UA		= 0x10
} da_ccb_state;

/* Offsets into our private area for storing information */
#define ccb_state	ppriv_field0
#define ccb_bp		ppriv_ptr1

struct disk_params {
	u_int8_t  heads;
	u_int16_t cylinders;
	u_int8_t  secs_per_track;
	u_int32_t secsize;	/* Number of bytes/sector */
	u_int32_t sectors;	/* total number sectors */
};

struct da_softc {
	struct	 buf_queue_head buf_queue;
	struct	 devstat device_stats;
	SLIST_ENTRY(da_softc) links;
	LIST_HEAD(, ccb_hdr) pending_ccbs;
	da_state state;
	da_flags flags;	
	da_quirks quirks;
	int	 minimum_cmd_size;
	int	 ordered_tag_count;
	struct	 disk_params params;
	struct	 disk disk;
	union	 ccb saved_ccb;
};

struct da_quirk_entry {
	struct scsi_inquiry_pattern inq_pat;
	da_quirks quirks;
};

static const char quantum[] = "QUANTUM";
static const char microp[] = "MICROP";

static struct da_quirk_entry da_quirk_table[] =
{
	{
		/*
		 * This particular Fujitsu drive doesn't like the
		 * synchronize cache command.
		 * Reported by: Tom Jackson <toj@gorilla.net>
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "FUJITSU", "M2954*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	
	},
	{
		/*
		 * This drive doesn't like the synchronize cache command
		 * either.  Reported by: Matthew Jacob <mjacob@feral.com>
		 * in NetBSD PR kern/6027, August 24, 1998.
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, microp, "2217*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * This drive doesn't like the synchronize cache command
		 * either.  Reported by: Hellmuth Michaelis (hm@kts.org)
		 * (PR 8882).
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, microp, "2112*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Doesn't like the synchronize cache command.
		 * Reported by: Blaz Zupan <blaz@gold.amis.net>
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "NEC", "D3847*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Doesn't like the synchronize cache command.
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, quantum, "MAVERICK 540S", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Doesn't like the synchronize cache command.
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, quantum, "LPS525S", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Doesn't work correctly with 6 byte reads/writes.
		 * Returns illegal request, and points to byte 9 of the
		 * 6-byte CDB.
		 * Reported by:  Adam McDougall <bsdx@spawnet.com>
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, quantum, "VIKING 4*", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * See above.
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, quantum, "VIKING 2*", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * This USB floppy drive uses the UFI command set. This
		 * command set is a derivative of the ATAPI command set and
		 * does not support READ_6 commands only READ_10. It also does
		 * not support sync cache (0x35).
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Y-E DATA", "USB-FDU", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE|DA_Q_NO_SYNC_CACHE
	}
};

static	d_open_t	daopen;
static	d_close_t	daclose;
static	d_strategy_t	dastrategy;
static	d_ioctl_t	daioctl;
static	d_dump_t	dadump;
static	periph_init_t	dainit;
static	void		daasync(void *callback_arg, u_int32_t code,
				struct cam_path *path, void *arg);
static	periph_ctor_t	daregister;
static	periph_dtor_t	dacleanup;
static	periph_start_t	dastart;
static	periph_oninv_t	daoninvalidate;
static	void		dadone(struct cam_periph *periph,
			       union ccb *done_ccb);
static  int		daerror(union ccb *ccb, u_int32_t cam_flags,
				u_int32_t sense_flags);
static void		daprevent(struct cam_periph *periph, int action);
static void		dasetgeom(struct cam_periph *periph,
				  struct scsi_read_capacity_data * rdcap);
static timeout_t	dasendorderedtag;
static void		dashutdown(void *arg, int howto);

#ifndef DA_DEFAULT_TIMEOUT
#define DA_DEFAULT_TIMEOUT 60	/* Timeout in seconds */
#endif

/*
 * DA_ORDEREDTAG_INTERVAL determines how often, relative
 * to the default timeout, we check to see whether an ordered
 * tagged transaction is appropriate to prevent simple tag
 * starvation.  Since we'd like to ensure that there is at least
 * 1/2 of the timeout length left for a starved transaction to
 * complete after we've sent an ordered tag, we must poll at least
 * four times in every timeout period.  This takes care of the worst
 * case where a starved transaction starts during an interval that
 * meets the requirement "don't send an ordered tag" test so it takes
 * us two intervals to determine that a tag must be sent.
 */
#ifndef DA_ORDEREDTAG_INTERVAL
#define DA_ORDEREDTAG_INTERVAL 4
#endif

static struct periph_driver dadriver =
{
	dainit, "da",
	TAILQ_HEAD_INITIALIZER(dadriver.units), /* generation */ 0
};

DATA_SET(periphdriver_set, dadriver);

#define DA_CDEV_MAJOR 13
#define DA_BDEV_MAJOR 4

/* For 2.2-stable support */
#ifndef D_DISK
#define D_DISK 0
#endif

static struct cdevsw da_cdevsw = {
	/* open */	daopen,
	/* close */	daclose,
	/* read */	physread,
	/* write */	physwrite,
	/* ioctl */	daioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	dastrategy,
	/* name */	"da",
	/* maj */	DA_CDEV_MAJOR,
	/* dump */	dadump,
	/* psize */	nopsize,
	/* flags */	D_DISK,
	/* bmaj */	DA_BDEV_MAJOR
};

static struct cdevsw dadisk_cdevsw;

static SLIST_HEAD(,da_softc) softc_list;
static struct extend_array *daperiphs;

static int
daopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct cam_periph *periph;
	struct da_softc *softc;
	struct disklabel *label;	
	int unit;
	int part;
	int error;
	int s;

	unit = dkunit(dev);
	part = dkpart(dev);
	periph = cam_extend_get(daperiphs, unit);
	if (periph == NULL)
		return (ENXIO);	

	softc = (struct da_softc *)periph->softc;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE,
	    ("daopen: dev=%s (unit %d , partition %d)\n", devtoname(dev),
	     unit, part));

	if ((error = cam_periph_lock(periph, PRIBIO|PCATCH)) != 0) {
		return (error); /* error code from tsleep */
	}

	if (cam_periph_acquire(periph) != CAM_REQ_CMP)
		return(ENXIO);
	softc->flags |= DA_FLAG_OPEN;

	s = splsoftcam();
	if ((softc->flags & DA_FLAG_PACK_INVALID) != 0) {
		/* Invalidate our pack information. */
		disk_invalidate(&softc->disk);
		softc->flags &= ~DA_FLAG_PACK_INVALID;
	}
	splx(s);

	/* Do a read capacity */
	{
		struct scsi_read_capacity_data *rcap;
		union  ccb *ccb;

		rcap = (struct scsi_read_capacity_data *)malloc(sizeof(*rcap),
								M_TEMP,
								M_WAITOK);
		
		ccb = cam_periph_getccb(periph, /*priority*/1);
		scsi_read_capacity(&ccb->csio,
				   /*retries*/1,
				   /*cbfncp*/dadone,
				   MSG_SIMPLE_Q_TAG,
				   rcap,
				   SSD_FULL_SIZE,
				   /*timeout*/60000);
		ccb->ccb_h.ccb_bp = NULL;

		error = cam_periph_runccb(ccb, daerror, /*cam_flags*/0,
					  /*sense_flags*/SF_RETRY_UA |
							 SF_RETRY_SELTO,
					  &softc->device_stats);

		xpt_release_ccb(ccb);

		if (error == 0) {
			dasetgeom(periph, rcap);
		}

		free(rcap, M_TEMP);
	}

	if (error == 0) {
		struct ccb_getdev cgd;

		/* Build label for whole disk. */
		label = &softc->disk.d_label;
		bzero(label, sizeof(*label));
		label->d_type = DTYPE_SCSI;

		/*
		 * Grab the inquiry data to get the vendor and product names.
		 * Put them in the typename and packname for the label.
		 */
		xpt_setup_ccb(&cgd.ccb_h, periph->path, /*priority*/ 1);
		cgd.ccb_h.func_code = XPT_GDEV_TYPE;
		xpt_action((union ccb *)&cgd);

		strncpy(label->d_typename, cgd.inq_data.vendor,
			min(SID_VENDOR_SIZE, sizeof(label->d_typename)));
		strncpy(label->d_packname, cgd.inq_data.product,
			min(SID_PRODUCT_SIZE, sizeof(label->d_packname)));
		
		label->d_secsize = softc->params.secsize;
		label->d_nsectors = softc->params.secs_per_track;
		label->d_ntracks = softc->params.heads;
		label->d_ncylinders = softc->params.cylinders;
		label->d_secpercyl = softc->params.heads
				  * softc->params.secs_per_track;
		label->d_secperunit = softc->params.sectors;

		if (((softc->flags & DA_FLAG_PACK_REMOVABLE) != 0)) {
			daprevent(periph, PR_PREVENT);
		}
	
		/*
		 * Check to see whether or not the blocksize is set yet.
		 * If it isn't, set it and then clear the blocksize
		 * unavailable flag for the device statistics.
		 */
		if ((softc->device_stats.flags & DEVSTAT_BS_UNAVAILABLE) != 0){
			softc->device_stats.block_size = softc->params.secsize;
			softc->device_stats.flags &= ~DEVSTAT_BS_UNAVAILABLE;
		}
	}
	
	if (error != 0) {
		if ((softc->flags & DA_FLAG_PACK_REMOVABLE) != 0) {
			daprevent(periph, PR_ALLOW);
		}
	}
	cam_periph_unlock(periph);
	return (error);
}

static int
daclose(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct	cam_periph *periph;
	struct	da_softc *softc;
	int	unit;
	int	error;

	unit = dkunit(dev);
	periph = cam_extend_get(daperiphs, unit);
	if (periph == NULL)
		return (ENXIO);	

	softc = (struct da_softc *)periph->softc;

	if ((error = cam_periph_lock(periph, PRIBIO)) != 0) {
		return (error); /* error code from tsleep */
	}

	if ((softc->quirks & DA_Q_NO_SYNC_CACHE) == 0) {
		union	ccb *ccb;

		ccb = cam_periph_getccb(periph, /*priority*/1);

		scsi_synchronize_cache(&ccb->csio,
				       /*retries*/1,
				       /*cbfcnp*/dadone,
				       MSG_SIMPLE_Q_TAG,
				       /*begin_lba*/0,/* Cover the whole disk */
				       /*lb_count*/0,
				       SSD_FULL_SIZE,
				       5 * 60 * 1000);

		cam_periph_runccb(ccb, /*error_routine*/NULL, /*cam_flags*/0,
				  /*sense_flags*/SF_RETRY_UA,
				  &softc->device_stats);

		if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			if ((ccb->ccb_h.status & CAM_STATUS_MASK) ==
			     CAM_SCSI_STATUS_ERROR) {
				int asc, ascq;
				int sense_key, error_code;

				scsi_extract_sense(&ccb->csio.sense_data,
						   &error_code,
						   &sense_key, 
						   &asc, &ascq);
				if (sense_key != SSD_KEY_ILLEGAL_REQUEST)
					scsi_sense_print(&ccb->csio);
			} else {
				xpt_print_path(periph->path);
				printf("Synchronize cache failed, status "
				       "== 0x%x, scsi status == 0x%x\n",
				       ccb->csio.ccb_h.status,
				       ccb->csio.scsi_status);
			}
		}

		if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
			cam_release_devq(ccb->ccb_h.path,
					 /*relsim_flags*/0,
					 /*reduction*/0,
					 /*timeout*/0,
					 /*getcount_only*/0);

		xpt_release_ccb(ccb);

	}

	if ((softc->flags & DA_FLAG_PACK_REMOVABLE) != 0) {
		daprevent(periph, PR_ALLOW);
		/*
		 * If we've got removeable media, mark the blocksize as
		 * unavailable, since it could change when new media is
		 * inserted.
		 */
		softc->device_stats.flags |= DEVSTAT_BS_UNAVAILABLE;
	}

	softc->flags &= ~DA_FLAG_OPEN;
	cam_periph_unlock(periph);
	cam_periph_release(periph);
	return (0);	
}

/*
 * Actually translate the requested transfer into one the physical driver
 * can understand.  The transfer is described by a buf and will include
 * only one physical transfer.
 */
static void
dastrategy(struct buf *bp)
{
	struct cam_periph *periph;
	struct da_softc *softc;
	u_int  unit;
	u_int  part;
	int    s;
	
	unit = dkunit(bp->b_dev);
	part = dkpart(bp->b_dev);
	periph = cam_extend_get(daperiphs, unit);
	if (periph == NULL) {
		bp->b_error = ENXIO;
		goto bad;		
	}
	softc = (struct da_softc *)periph->softc;
#if 0
	/*
	 * check it's not too big a transfer for our adapter
	 */
	scsi_minphys(bp,&sd_switch);
#endif

	/*
	 * Mask interrupts so that the pack cannot be invalidated until
	 * after we are in the queue.  Otherwise, we might not properly
	 * clean up one of the buffers.
	 */
	s = splbio();
	
	/*
	 * If the device has been made invalid, error out
	 */
	if ((softc->flags & DA_FLAG_PACK_INVALID)) {
		splx(s);
		bp->b_error = ENXIO;
		goto bad;
	}
	
	/*
	 * Place it in the queue of disk activities for this disk
	 */
	bufqdisksort(&softc->buf_queue, bp);

	splx(s);
	
	/*
	 * Schedule ourselves for performing the work.
	 */
	xpt_schedule(periph, /* XXX priority */1);

	return;
bad:
	bp->b_flags |= B_ERROR;

	/*
	 * Correctly set the buf to indicate a completed xfer
	 */
	bp->b_resid = bp->b_bcount;
	biodone(bp);
	return;
}

/* For 2.2-stable support */
#ifndef ENOIOCTL
#define ENOIOCTL -1
#endif

static int
daioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	struct cam_periph *periph;
	struct da_softc *softc;
	int unit;
	int error;

	unit = dkunit(dev);
	periph = cam_extend_get(daperiphs, unit);
	if (periph == NULL)
		return (ENXIO);	

	softc = (struct da_softc *)periph->softc;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("daioctl\n"));

	if ((error = cam_periph_lock(periph, PRIBIO|PCATCH)) != 0) {
		return (error); /* error code from tsleep */
	}	

	error = cam_periph_ioctl(periph, cmd, addr, daerror);

	cam_periph_unlock(periph);
	
	return (error);
}

static int
dadump(dev_t dev)
{
	struct	    cam_periph *periph;
	struct	    da_softc *softc;
	u_int	    unit;
	u_int	    part;
	u_int	    secsize;
	u_int	    num;	/* number of sectors to write */
	u_int	    blknum;
	long	    blkcnt;
	vm_offset_t addr;	
	struct	    ccb_scsiio csio;
	int	    error;

	/* toss any characters present prior to dump */
	while (cncheckc() != -1)
		;

	unit = dkunit(dev);
	part = dkpart(dev);
	periph = cam_extend_get(daperiphs, unit);
	if (periph == NULL) {
		return (ENXIO);
	}
	softc = (struct da_softc *)periph->softc;
	
	if ((softc->flags & DA_FLAG_PACK_INVALID) != 0)
		return (ENXIO);

	error = disk_dumpcheck(dev, &num, &blknum, &secsize);
	if (error)
		return (error);

	addr = 0;	/* starting address */
	blkcnt = howmany(PAGE_SIZE, secsize);

	while (num > 0) {
		void *va;

		if (is_physical_memory(addr)) {
			va = pmap_kenter_temporary(trunc_page(addr));
		} else {
			va = pmap_kenter_temporary(trunc_page(0));
		}

		xpt_setup_ccb(&csio.ccb_h, periph->path, /*priority*/1);
		csio.ccb_h.ccb_state = DA_CCB_DUMP;
		scsi_read_write(&csio,
				/*retries*/1,
				dadone,
				MSG_ORDERED_Q_TAG,
				/*read*/FALSE,
				/*byte2*/0,
				/*minimum_cmd_size*/ softc->minimum_cmd_size,
				blknum,
				blkcnt,
				/*data_ptr*/(u_int8_t *) va,
				/*dxfer_len*/blkcnt * secsize,
				/*sense_len*/SSD_FULL_SIZE,
				DA_DEFAULT_TIMEOUT * 1000);		
		xpt_polled_action((union ccb *)&csio);

		if ((csio.ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			printf("Aborting dump due to I/O error.\n");
			if ((csio.ccb_h.status & CAM_STATUS_MASK) ==
			     CAM_SCSI_STATUS_ERROR)
				scsi_sense_print(&csio);
			else
				printf("status == 0x%x, scsi status == 0x%x\n",
				       csio.ccb_h.status, csio.scsi_status);
			return(EIO);
		}
		
		if (addr % (1024 * 1024) == 0) {
#ifdef	HW_WDOG
			if (wdog_tickler)
				(*wdog_tickler)();
#endif /* HW_WDOG */
			/* Count in MB of data left to write */
			printf("%d ", (num  * softc->params.secsize)
				     / (1024 * 1024));
		}
		
		/* update block count */
		num -= blkcnt;
		blknum += blkcnt;
		addr += PAGE_SIZE;

		/* operator aborting dump? */
		if (cncheckc() != -1)
			return (EINTR);
	}

	/*
	 * Sync the disk cache contents to the physical media.
	 */
	if ((softc->quirks & DA_Q_NO_SYNC_CACHE) == 0) {

		xpt_setup_ccb(&csio.ccb_h, periph->path, /*priority*/1);
		csio.ccb_h.ccb_state = DA_CCB_DUMP;
		scsi_synchronize_cache(&csio,
				       /*retries*/1,
				       /*cbfcnp*/dadone,
				       MSG_SIMPLE_Q_TAG,
				       /*begin_lba*/0,/* Cover the whole disk */
				       /*lb_count*/0,
				       SSD_FULL_SIZE,
				       5 * 60 * 1000);
		xpt_polled_action((union ccb *)&csio);

		if ((csio.ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			if ((csio.ccb_h.status & CAM_STATUS_MASK) ==
			     CAM_SCSI_STATUS_ERROR) {
				int asc, ascq;
				int sense_key, error_code;

				scsi_extract_sense(&csio.sense_data,
						   &error_code,
						   &sense_key, 
						   &asc, &ascq);
				if (sense_key != SSD_KEY_ILLEGAL_REQUEST)
					scsi_sense_print(&csio);
			} else {
				xpt_print_path(periph->path);
				printf("Synchronize cache failed, status "
				       "== 0x%x, scsi status == 0x%x\n",
				       csio.ccb_h.status, csio.scsi_status);
			}
		}
	}
	return (0);
}

static void
dainit(void)
{
	cam_status status;
	struct cam_path *path;

	/*
	 * Create our extend array for storing the devices we attach to.
	 */
	daperiphs = cam_extend_new();
	SLIST_INIT(&softc_list);
	if (daperiphs == NULL) {
		printf("da: Failed to alloc extend array!\n");
		return;
	}
	
	/*
	 * Install a global async callback.  This callback will
	 * receive async callbacks like "new device found".
	 */
	status = xpt_create_path(&path, /*periph*/NULL, CAM_XPT_PATH_ID,
				 CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD);

	if (status == CAM_REQ_CMP) {
		struct ccb_setasync csa;

                xpt_setup_ccb(&csa.ccb_h, path, /*priority*/5);
                csa.ccb_h.func_code = XPT_SASYNC_CB;
                csa.event_enable = AC_FOUND_DEVICE;
                csa.callback = daasync;
                csa.callback_arg = NULL;
                xpt_action((union ccb *)&csa);
		status = csa.ccb_h.status;
                xpt_free_path(path);
        }

	if (status != CAM_REQ_CMP) {
		printf("da: Failed to attach master async callback "
		       "due to status 0x%x!\n", status);
	} else {

		/*
		 * Schedule a periodic event to occasioanly send an
		 * ordered tag to a device.
		 */
		timeout(dasendorderedtag, NULL,
			(DA_DEFAULT_TIMEOUT * hz) / DA_ORDEREDTAG_INTERVAL);

		/* Register our shutdown event handler */
		if ((EVENTHANDLER_REGISTER(shutdown_post_sync, dashutdown, 
					   NULL, SHUTDOWN_PRI_DEFAULT)) == NULL)
		    printf("dainit: shutdown event registration failed!\n");
	}
}

static void
daoninvalidate(struct cam_periph *periph)
{
	int s;
	struct da_softc *softc;
	struct buf *q_bp;
	struct ccb_setasync csa;

	softc = (struct da_softc *)periph->softc;

	/*
	 * De-register any async callbacks.
	 */
	xpt_setup_ccb(&csa.ccb_h, periph->path,
		      /* priority */ 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = 0;
	csa.callback = daasync;
	csa.callback_arg = periph;
	xpt_action((union ccb *)&csa);

	softc->flags |= DA_FLAG_PACK_INVALID;

	/*
	 * Although the oninvalidate() routines are always called at
	 * splsoftcam, we need to be at splbio() here to keep the buffer
	 * queue from being modified while we traverse it.
	 */
	s = splbio();

	/*
	 * Return all queued I/O with ENXIO.
	 * XXX Handle any transactions queued to the card
	 *     with XPT_ABORT_CCB.
	 */
	while ((q_bp = bufq_first(&softc->buf_queue)) != NULL){
		bufq_remove(&softc->buf_queue, q_bp);
		q_bp->b_resid = q_bp->b_bcount;
		q_bp->b_error = ENXIO;
		q_bp->b_flags |= B_ERROR;
		biodone(q_bp);
	}
	splx(s);

	SLIST_REMOVE(&softc_list, softc, da_softc, links);

	xpt_print_path(periph->path);
	printf("lost device\n");
}

static void
dacleanup(struct cam_periph *periph)
{
	struct da_softc *softc;

	softc = (struct da_softc *)periph->softc;

	devstat_remove_entry(&softc->device_stats);
	cam_extend_release(daperiphs, periph->unit_number);
	xpt_print_path(periph->path);
	printf("removing device entry\n");
	free(softc, M_DEVBUF);
}

static void
daasync(void *callback_arg, u_int32_t code,
	struct cam_path *path, void *arg)
{
	struct cam_periph *periph;

	periph = (struct cam_periph *)callback_arg;
	switch (code) {
	case AC_FOUND_DEVICE:
	{
		struct ccb_getdev *cgd;
		cam_status status;
 
		cgd = (struct ccb_getdev *)arg;

		if (SID_TYPE(&cgd->inq_data) != T_DIRECT
		    && SID_TYPE(&cgd->inq_data) != T_OPTICAL)
			break;

		/*
		 * Allocate a peripheral instance for
		 * this device and start the probe
		 * process.
		 */
		status = cam_periph_alloc(daregister, daoninvalidate,
					  dacleanup, dastart,
					  "da", CAM_PERIPH_BIO,
					  cgd->ccb_h.path, daasync,
					  AC_FOUND_DEVICE, cgd);

		if (status != CAM_REQ_CMP
		 && status != CAM_REQ_INPROG)
			printf("daasync: Unable to attach to new device "
				"due to status 0x%x\n", status);
		break;
	}
	case AC_SENT_BDR:
	case AC_BUS_RESET:
	{
		struct da_softc *softc;
		struct ccb_hdr *ccbh;
		int s;

		softc = (struct da_softc *)periph->softc;
		s = splsoftcam();
		/*
		 * Don't fail on the expected unit attention
		 * that will occur.
		 */
		softc->flags |= DA_FLAG_RETRY_UA;
		for (ccbh = LIST_FIRST(&softc->pending_ccbs);
		     ccbh != NULL; ccbh = LIST_NEXT(ccbh, periph_links.le))
			ccbh->ccb_state |= DA_CCB_RETRY_UA;
		splx(s);
		/* FALLTHROUGH*/
	}
	default:
		cam_periph_async(periph, code, path, arg);
		break;
	}
}

static cam_status
daregister(struct cam_periph *periph, void *arg)
{
	int s;
	struct da_softc *softc;
	struct ccb_setasync csa;
	struct ccb_getdev *cgd;
	caddr_t match;

	cgd = (struct ccb_getdev *)arg;
	if (periph == NULL) {
		printf("daregister: periph was NULL!!\n");
		return(CAM_REQ_CMP_ERR);
	}

	if (cgd == NULL) {
		printf("daregister: no getdev CCB, can't register device\n");
		return(CAM_REQ_CMP_ERR);
	}

	softc = (struct da_softc *)malloc(sizeof(*softc),M_DEVBUF,M_NOWAIT);

	if (softc == NULL) {
		printf("daregister: Unable to probe new device. "
		       "Unable to allocate softc\n");				
		return(CAM_REQ_CMP_ERR);
	}

	bzero(softc, sizeof(*softc));
	LIST_INIT(&softc->pending_ccbs);
	softc->state = DA_STATE_PROBE;
	bufq_init(&softc->buf_queue);
	if (SID_IS_REMOVABLE(&cgd->inq_data))
		softc->flags |= DA_FLAG_PACK_REMOVABLE;
	if ((cgd->inq_data.flags & SID_CmdQue) != 0)
		softc->flags |= DA_FLAG_TAGGED_QUEUING;

	periph->softc = softc;
	
	cam_extend_set(daperiphs, periph->unit_number, periph);

	/*
	 * See if this device has any quirks.
	 */
	match = cam_quirkmatch((caddr_t)&cgd->inq_data,
			       (caddr_t)da_quirk_table,
			       sizeof(da_quirk_table)/sizeof(*da_quirk_table),
			       sizeof(*da_quirk_table), scsi_inquiry_match);

	if (match != NULL)
		softc->quirks = ((struct da_quirk_entry *)match)->quirks;
	else
		softc->quirks = DA_Q_NONE;

	if (softc->quirks & DA_Q_NO_6_BYTE)
		softc->minimum_cmd_size = 10;
	else
		softc->minimum_cmd_size = 6;

	/*
	 * Block our timeout handler while we
	 * add this softc to the dev list.
	 */
	s = splsoftclock();
	SLIST_INSERT_HEAD(&softc_list, softc, links);
	splx(s);

	/*
	 * The DA driver supports a blocksize, but
	 * we don't know the blocksize until we do 
	 * a read capacity.  So, set a flag to
	 * indicate that the blocksize is 
	 * unavailable right now.  We'll clear the
	 * flag as soon as we've done a read capacity.
	 */
	devstat_add_entry(&softc->device_stats, "da", 
			  periph->unit_number, 0,
	  		  DEVSTAT_BS_UNAVAILABLE,
			  SID_TYPE(&cgd->inq_data) | DEVSTAT_TYPE_IF_SCSI,
			  DEVSTAT_PRIORITY_DISK);

	/*
	 * Register this media as a disk
	 */
	disk_create(periph->unit_number, &softc->disk, 0, 
	    &da_cdevsw, &dadisk_cdevsw);

	/*
	 * Add async callbacks for bus reset and
	 * bus device reset calls.  I don't bother
	 * checking if this fails as, in most cases,
	 * the system will function just fine without
	 * them and the only alternative would be to
	 * not attach the device on failure.
	 */
	xpt_setup_ccb(&csa.ccb_h, periph->path, /*priority*/5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = AC_SENT_BDR | AC_BUS_RESET | AC_LOST_DEVICE;
	csa.callback = daasync;
	csa.callback_arg = periph;
	xpt_action((union ccb *)&csa);
	/*
	 * Lock this peripheral until we are setup.
	 * This first call can't block
	 */
	(void)cam_periph_lock(periph, PRIBIO);
	xpt_schedule(periph, /*priority*/5);

	return(CAM_REQ_CMP);
}

static void
dastart(struct cam_periph *periph, union ccb *start_ccb)
{
	struct da_softc *softc;

	softc = (struct da_softc *)periph->softc;

	
	switch (softc->state) {
	case DA_STATE_NORMAL:
	{
		/* Pull a buffer from the queue and get going on it */		
		struct buf *bp;
		int s;

		/*
		 * See if there is a buf with work for us to do..
		 */
		s = splbio();
		bp = bufq_first(&softc->buf_queue);
		if (periph->immediate_priority <= periph->pinfo.priority) {
			CAM_DEBUG_PRINT(CAM_DEBUG_SUBTRACE,
					("queuing for immediate ccb\n"));
			start_ccb->ccb_h.ccb_state = DA_CCB_WAITING;
			SLIST_INSERT_HEAD(&periph->ccb_list, &start_ccb->ccb_h,
					  periph_links.sle);
			periph->immediate_priority = CAM_PRIORITY_NONE;
			splx(s);
			wakeup(&periph->ccb_list);
		} else if (bp == NULL) {
			splx(s);
			xpt_release_ccb(start_ccb);
		} else {
			int oldspl;
			u_int8_t tag_code;

			bufq_remove(&softc->buf_queue, bp);

			devstat_start_transaction(&softc->device_stats);

			if ((bp->b_flags & B_ORDERED) != 0
			 || (softc->flags & DA_FLAG_NEED_OTAG) != 0) {
				softc->flags &= ~DA_FLAG_NEED_OTAG;
				softc->ordered_tag_count++;
				tag_code = MSG_ORDERED_Q_TAG;
			} else {
				tag_code = MSG_SIMPLE_Q_TAG;
			}
			scsi_read_write(&start_ccb->csio,
					/*retries*/4,
					dadone,
					tag_code,
					bp->b_flags & B_READ,
					/*byte2*/0,
					softc->minimum_cmd_size,
					bp->b_pblkno,
					bp->b_bcount / softc->params.secsize,
					bp->b_data,
					bp->b_bcount,
					/*sense_len*/SSD_FULL_SIZE,
					DA_DEFAULT_TIMEOUT * 1000);
			start_ccb->ccb_h.ccb_state = DA_CCB_BUFFER_IO;

			/*
			 * Block out any asyncronous callbacks
			 * while we touch the pending ccb list.
			 */
			oldspl = splcam();
			LIST_INSERT_HEAD(&softc->pending_ccbs,
					 &start_ccb->ccb_h, periph_links.le);
			splx(oldspl);

			/* We expect a unit attention from this device */
			if ((softc->flags & DA_FLAG_RETRY_UA) != 0) {
				start_ccb->ccb_h.ccb_state |= DA_CCB_RETRY_UA;
				softc->flags &= ~DA_FLAG_RETRY_UA;
			}

			start_ccb->ccb_h.ccb_bp = bp;
			bp = bufq_first(&softc->buf_queue);
			splx(s);

			xpt_action(start_ccb);
		}
		
		if (bp != NULL) {
			/* Have more work to do, so ensure we stay scheduled */
			xpt_schedule(periph, /* XXX priority */1);
		}
		break;
	}
	case DA_STATE_PROBE:
	{
		struct ccb_scsiio *csio;
		struct scsi_read_capacity_data *rcap;

		rcap = (struct scsi_read_capacity_data *)malloc(sizeof(*rcap),
								M_TEMP,
								M_NOWAIT);
		if (rcap == NULL) {
			printf("dastart: Couldn't malloc read_capacity data\n");
			/* da_free_periph??? */
			break;
		}
		csio = &start_ccb->csio;
		scsi_read_capacity(csio,
				   /*retries*/4,
				   dadone,
				   MSG_SIMPLE_Q_TAG,
				   rcap,
				   SSD_FULL_SIZE,
				   /*timeout*/5000);
		start_ccb->ccb_h.ccb_bp = NULL;
		start_ccb->ccb_h.ccb_state = DA_CCB_PROBE;
		xpt_action(start_ccb);
		break;
	}
	}
}


static void
dadone(struct cam_periph *periph, union ccb *done_ccb)
{
	struct da_softc *softc;
	struct ccb_scsiio *csio;

	softc = (struct da_softc *)periph->softc;
	csio = &done_ccb->csio;
	switch (csio->ccb_h.ccb_state & DA_CCB_TYPE_MASK) {
	case DA_CCB_BUFFER_IO:
	{
		struct buf *bp;
		int    oldspl;

		bp = (struct buf *)done_ccb->ccb_h.ccb_bp;
		if ((done_ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			int error;
			int s;
			int sf;
			
			if ((csio->ccb_h.ccb_state & DA_CCB_RETRY_UA) != 0)
				sf = SF_RETRY_UA;
			else
				sf = 0;

			/* Retry selection timeouts */
			sf |= SF_RETRY_SELTO;

			if ((error = daerror(done_ccb, 0, sf)) == ERESTART) {
				/*
				 * A retry was scheuled, so
				 * just return.
				 */
				return;
			}
			if (error != 0) {
				struct buf *q_bp;

				s = splbio();

				if (error == ENXIO) {
					/*
					 * Catastrophic error.  Mark our pack as
					 * invalid.
					 */
					/* XXX See if this is really a media
					 *     change first.
					 */
					xpt_print_path(periph->path);
					printf("Invalidating pack\n");
					softc->flags |= DA_FLAG_PACK_INVALID;
				}

				/*
				 * return all queued I/O with EIO, so that
				 * the client can retry these I/Os in the
				 * proper order should it attempt to recover.
				 */
				while ((q_bp = bufq_first(&softc->buf_queue))
					!= NULL) {
					bufq_remove(&softc->buf_queue, q_bp);
					q_bp->b_resid = q_bp->b_bcount;
					q_bp->b_error = EIO;
					q_bp->b_flags |= B_ERROR;
					biodone(q_bp);
				}
				splx(s);
				bp->b_error = error;
				bp->b_resid = bp->b_bcount;
				bp->b_flags |= B_ERROR;
			} else {
				bp->b_resid = csio->resid;
				bp->b_error = 0;
				if (bp->b_resid != 0) {
					/* Short transfer ??? */
					bp->b_flags |= B_ERROR;
				}
			}
			if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
				cam_release_devq(done_ccb->ccb_h.path,
						 /*relsim_flags*/0,
						 /*reduction*/0,
						 /*timeout*/0,
						 /*getcount_only*/0);
		} else {
			bp->b_resid = csio->resid;
			if (csio->resid > 0)
				bp->b_flags |= B_ERROR;
		}

		/*
		 * Block out any asyncronous callbacks
		 * while we touch the pending ccb list.
		 */
		oldspl = splcam();
		LIST_REMOVE(&done_ccb->ccb_h, periph_links.le);
		splx(oldspl);

		if (softc->device_stats.busy_count == 0)
			softc->flags |= DA_FLAG_WENT_IDLE;

		devstat_end_transaction_buf(&softc->device_stats, bp);
		biodone(bp);
		break;
	}
	case DA_CCB_PROBE:
	{
		struct	   scsi_read_capacity_data *rdcap;
		char	   announce_buf[80];

		rdcap = (struct scsi_read_capacity_data *)csio->data_ptr;
		
		if ((csio->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
			struct disk_params *dp;

			dasetgeom(periph, rdcap);
			dp = &softc->params;
			snprintf(announce_buf, sizeof(announce_buf),
			        "%luMB (%u %u byte sectors: %dH %dS/T %dC)",
				(unsigned long) (((u_int64_t)dp->secsize *
				dp->sectors) / (1024*1024)), dp->sectors,
				dp->secsize, dp->heads, dp->secs_per_track,
				dp->cylinders);
		} else {
			int	error;

			announce_buf[0] = '\0';

			/*
			 * Retry any UNIT ATTENTION type errors.  They
			 * are expected at boot.
			 */
			error = daerror(done_ccb, 0, SF_RETRY_UA |
					SF_RETRY_SELTO | SF_NO_PRINT);
			if (error == ERESTART) {
				/*
				 * A retry was scheuled, so
				 * just return.
				 */
				return;
			} else if (error != 0) {
				struct scsi_sense_data *sense;
				int asc, ascq;
				int sense_key, error_code;
				int have_sense;
				cam_status status;
				struct ccb_getdev cgd;

				/* Don't wedge this device's queue */
				cam_release_devq(done_ccb->ccb_h.path,
						 /*relsim_flags*/0,
						 /*reduction*/0,
						 /*timeout*/0,
						 /*getcount_only*/0);

				status = done_ccb->ccb_h.status;

				xpt_setup_ccb(&cgd.ccb_h, 
					      done_ccb->ccb_h.path,
					      /* priority */ 1);
				cgd.ccb_h.func_code = XPT_GDEV_TYPE;
				xpt_action((union ccb *)&cgd);

				if (((csio->ccb_h.flags & CAM_SENSE_PHYS) != 0)
				 || ((csio->ccb_h.flags & CAM_SENSE_PTR) != 0)
				 || ((status & CAM_AUTOSNS_VALID) == 0))
					have_sense = FALSE;
				else
					have_sense = TRUE;

				if (have_sense) {
					sense = &csio->sense_data;
					scsi_extract_sense(sense, &error_code,
							   &sense_key, 
							   &asc, &ascq);
				}
				/*
				 * Attach to anything that claims to be a
				 * direct access or optical disk device,
				 * as long as it doesn't return a "Logical
				 * unit not supported" (0x25) error.
				 */
				if ((have_sense) && (asc != 0x25)
				 && (error_code == SSD_CURRENT_ERROR))
					snprintf(announce_buf,
					    sizeof(announce_buf),
						"Attempt to query device "
						"size failed: %s, %s",
						scsi_sense_key_text[sense_key],
						scsi_sense_desc(asc,ascq,
								&cgd.inq_data));
				else { 
					if (have_sense)
						scsi_sense_print(
							&done_ccb->csio);
					else {
						xpt_print_path(periph->path);
						printf("got CAM status %#x\n",
						       done_ccb->ccb_h.status);
					}

					xpt_print_path(periph->path);
					printf("fatal error, failed" 
					       " to attach to device\n");

					/*
					 * Free up resources.
					 */
					cam_periph_invalidate(periph);
				} 
			}
		}
		free(rdcap, M_TEMP);
		if (announce_buf[0] != '\0')
			xpt_announce_periph(periph, announce_buf);
		softc->state = DA_STATE_NORMAL;		
		/*
		 * Since our peripheral may be invalidated by an error
		 * above or an external event, we must release our CCB
		 * before releasing the probe lock on the peripheral.
		 * The peripheral will only go away once the last lock
		 * is removed, and we need it around for the CCB release
		 * operation.
		 */
		xpt_release_ccb(done_ccb);
		cam_periph_unlock(periph);
		return;
	}
	case DA_CCB_WAITING:
	{
		/* Caller will release the CCB */
		wakeup(&done_ccb->ccb_h.cbfcnp);
		return;
	}
	case DA_CCB_DUMP:
		/* No-op.  We're polling */
		return;
	default:
		break;
	}
	xpt_release_ccb(done_ccb);
}

static int
daerror(union ccb *ccb, u_int32_t cam_flags, u_int32_t sense_flags)
{
	struct da_softc	  *softc;
	struct cam_periph *periph;

	periph = xpt_path_periph(ccb->ccb_h.path);
	softc = (struct da_softc *)periph->softc;

	/*
	 * XXX
	 * Until we have a better way of doing pack validation,
	 * don't treat UAs as errors.
	 */
	sense_flags |= SF_RETRY_UA;
	return(cam_periph_error(ccb, cam_flags, sense_flags,
				&softc->saved_ccb));
}

static void
daprevent(struct cam_periph *periph, int action)
{
	struct	da_softc *softc;
	union	ccb *ccb;		
	int	error;
		
	softc = (struct da_softc *)periph->softc;

	if (((action == PR_ALLOW)
	  && (softc->flags & DA_FLAG_PACK_LOCKED) == 0)
	 || ((action == PR_PREVENT)
	  && (softc->flags & DA_FLAG_PACK_LOCKED) != 0)) {
		return;
	}

	ccb = cam_periph_getccb(periph, /*priority*/1);

	scsi_prevent(&ccb->csio,
		     /*retries*/1,
		     /*cbcfp*/dadone,
		     MSG_SIMPLE_Q_TAG,
		     action,
		     SSD_FULL_SIZE,
		     5000);

	error = cam_periph_runccb(ccb, /*error_routine*/NULL, /*cam_flags*/0,
				  /*sense_flags*/0, &softc->device_stats);

	if (error == 0) {
		if (action == PR_ALLOW)
			softc->flags &= ~DA_FLAG_PACK_LOCKED;
		else
			softc->flags |= DA_FLAG_PACK_LOCKED;
	}

	xpt_release_ccb(ccb);
}

static void
dasetgeom(struct cam_periph *periph, struct scsi_read_capacity_data * rdcap)
{
	struct ccb_calc_geometry ccg;
	struct da_softc *softc;
	struct disk_params *dp;

	softc = (struct da_softc *)periph->softc;

	dp = &softc->params;
	dp->secsize = scsi_4btoul(rdcap->length);
	dp->sectors = scsi_4btoul(rdcap->addr) + 1;
	/*
	 * Have the controller provide us with a geometry
	 * for this disk.  The only time the geometry
	 * matters is when we boot and the controller
	 * is the only one knowledgeable enough to come
	 * up with something that will make this a bootable
	 * device.
	 */
	xpt_setup_ccb(&ccg.ccb_h, periph->path, /*priority*/1);
	ccg.ccb_h.func_code = XPT_CALC_GEOMETRY;
	ccg.block_size = dp->secsize;
	ccg.volume_size = dp->sectors;
	ccg.heads = 0;
	ccg.secs_per_track = 0;
	ccg.cylinders = 0;
	xpt_action((union ccb*)&ccg);
	dp->heads = ccg.heads;
	dp->secs_per_track = ccg.secs_per_track;
	dp->cylinders = ccg.cylinders;
}

static void
dasendorderedtag(void *arg)
{
	struct da_softc *softc;
	int s;

	for (softc = SLIST_FIRST(&softc_list);
	     softc != NULL;
	     softc = SLIST_NEXT(softc, links)) {
		s = splsoftcam();
		if ((softc->ordered_tag_count == 0) 
		 && ((softc->flags & DA_FLAG_WENT_IDLE) == 0)) {
			softc->flags |= DA_FLAG_NEED_OTAG;
		}
		if (softc->device_stats.busy_count > 0)
			softc->flags &= ~DA_FLAG_WENT_IDLE;

		softc->ordered_tag_count = 0;
		splx(s);
	}
	/* Queue us up again */
	timeout(dasendorderedtag, NULL,
		(DA_DEFAULT_TIMEOUT * hz) / DA_ORDEREDTAG_INTERVAL);
}

/*
 * Step through all DA peripheral drivers, and if the device is still open,
 * sync the disk cache to physical media.
 */
static void
dashutdown(void * arg, int howto)
{
	struct cam_periph *periph;
	struct da_softc *softc;

	for (periph = TAILQ_FIRST(&dadriver.units); periph != NULL;
	     periph = TAILQ_NEXT(periph, unit_links)) {
		union ccb ccb;
		softc = (struct da_softc *)periph->softc;

		/*
		 * We only sync the cache if the drive is still open, and
		 * if the drive is capable of it..
		 */
		if (((softc->flags & DA_FLAG_OPEN) == 0)
		 || (softc->quirks & DA_Q_NO_SYNC_CACHE))
			continue;

		xpt_setup_ccb(&ccb.ccb_h, periph->path, /*priority*/1);

		ccb.ccb_h.ccb_state = DA_CCB_DUMP;
		scsi_synchronize_cache(&ccb.csio,
				       /*retries*/1,
				       /*cbfcnp*/dadone,
				       MSG_SIMPLE_Q_TAG,
				       /*begin_lba*/0, /* whole disk */
				       /*lb_count*/0,
				       SSD_FULL_SIZE,
				       5 * 60 * 1000);

		xpt_polled_action(&ccb);

		if ((ccb.ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			if (((ccb.ccb_h.status & CAM_STATUS_MASK) ==
			     CAM_SCSI_STATUS_ERROR)
			 && (ccb.csio.scsi_status == SCSI_STATUS_CHECK_COND)){
				int error_code, sense_key, asc, ascq;

				scsi_extract_sense(&ccb.csio.sense_data,
						   &error_code, &sense_key,
						   &asc, &ascq);

				if (sense_key != SSD_KEY_ILLEGAL_REQUEST)
					scsi_sense_print(&ccb.csio);
			} else {
				xpt_print_path(periph->path);
				printf("Synchronize cache failed, status "
				       "== 0x%x, scsi status == 0x%x\n",
				       ccb.ccb_h.status, ccb.csio.scsi_status);
			}
		}

		if ((ccb.ccb_h.status & CAM_DEV_QFRZN) != 0)
			cam_release_devq(ccb.ccb_h.path,
					 /*relsim_flags*/0,
					 /*reduction*/0,
					 /*timeout*/0,
					 /*getcount_only*/0);

	}
}

#else /* !_KERNEL */

/*
 * XXX This is only left out of the kernel build to silence warnings.  If,
 * for some reason this function is used in the kernel, the ifdefs should
 * be moved so it is included both in the kernel and userland.
 */
void
scsi_format_unit(struct ccb_scsiio *csio, u_int32_t retries,
		 void (*cbfcnp)(struct cam_periph *, union ccb *),
		 u_int8_t tag_action, u_int8_t byte2, u_int16_t ileave,
		 u_int8_t *data_ptr, u_int32_t dxfer_len, u_int8_t sense_len,
		 u_int32_t timeout)
{
	struct scsi_format_unit *scsi_cmd;

	scsi_cmd = (struct scsi_format_unit *)&csio->cdb_io.cdb_bytes;
	scsi_cmd->opcode = FORMAT_UNIT;
	scsi_cmd->byte2 = byte2;
	scsi_ulto2b(ileave, scsi_cmd->interleave);

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/ (dxfer_len > 0) ? CAM_DIR_OUT : CAM_DIR_NONE,
		      tag_action,
		      data_ptr,
		      dxfer_len,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}

#endif /* _KERNEL */
