/*
 * Implementation of SCSI Sequential Access Peripheral driver for CAM.
 *
 * Copyright (c) 1997 Justin T. Gibbs
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
 *      $Id$
 */

#include <sys/param.h>
#include <sys/queue.h>
#ifdef KERNEL
#include <sys/systm.h>
#include <sys/kernel.h>
#endif
#include <sys/types.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/mtio.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/devicestat.h>
#include <machine/limits.h>

#ifndef KERNEL
#include <stdio.h>
#include <string.h>
#endif

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_extend.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/scsi_sa.h>

#ifdef KERNEL

#define	SAUNIT(DEV) ((minor(DEV)&0xF0) >> 4)	/* 4 bit unit.	*/
#define	SASETUNIT(DEV, U) makedev(major(DEV), ((U) << 4))

typedef enum {
	SA_STATE_NORMAL
} sa_state;

typedef enum {
	SA_CCB_BUFFER_IO,
	SA_CCB_WAITING
} sa_ccb_types;

#define ccb_type ppriv_field0
#define ccb_bp	 ppriv_ptr1

typedef enum {
	SA_FLAG_OPEN		= 0x0001,
	SA_FLAG_FIXED		= 0x0002,
	SA_FLAG_TAPE_LOCKED	= 0x0004,
	SA_FLAG_TAPE_MOUNTED	= 0x0008,
	SA_FLAG_TAPE_WP		= 0x0010,
	SA_FLAG_TAPE_WRITTEN	= 0x0020,
	SA_FLAG_2FM_AT_EOD	= 0x0040,
	SA_FLAG_EOM_PENDING	= 0x0080,
	SA_FLAG_EIO_PENDING	= 0x0100,
	SA_FLAG_EOF_PENDING	= 0x0200,
	SA_FLAG_ERR_PENDING	= (SA_FLAG_EOM_PENDING|SA_FLAG_EIO_PENDING|
				   SA_FLAG_EOF_PENDING),
	SA_FLAG_INVALID		= 0x0400,
	SA_FLAG_COMP_ENABLED	= 0x0800,
	SA_FLAG_COMP_UNSUPP	= 0x1000
} sa_flags;

typedef enum {
	SA_MODE_REWIND		= 0x00,
	SA_MODE_NOREWIND	= 0x01,
	SA_MODE_OFFLINE		= 0x02
} sa_mode;

typedef enum {
	SA_PARAM_NONE		= 0x00,
	SA_PARAM_BLOCKSIZE	= 0x01,
	SA_PARAM_DENSITY	= 0x02,
	SA_PARAM_COMPRESSION	= 0x04,
	SA_PARAM_BUFF_MODE	= 0x08,
	SA_PARAM_NUMBLOCKS	= 0x10,
	SA_PARAM_WP		= 0x20,
	SA_PARAM_SPEED		= 0x40,
	SA_PARAM_ALL		= 0x7f
} sa_params;

typedef enum {
	SA_QUIRK_NONE		= 0x00,
	SA_QUIRK_NOCOMP		= 0x01
} sa_quirks;

struct sa_softc {
	sa_state	state;
	sa_flags	flags;
	sa_quirks	quirks;
	struct		buf_queue_head buf_queue;
	struct		devstat device_stats;
	int		blk_gran;
	int		blk_mask;
	int		blk_shift;
	u_int32_t	max_blk;
	u_int32_t	min_blk;
	u_int8_t	media_density;
	u_int32_t	media_blksize;
	u_int32_t	media_numblks;
	u_int32_t	comp_algorithm;
	u_int32_t	saved_comp_algorithm;
	u_int8_t	speed;
	int		buffer_mode;
	int		filemarks;
	union		ccb saved_ccb;
};

struct sa_quirk_entry {
	struct scsi_inquiry_pattern inq_pat;
	sa_quirks quirks;
};

static struct sa_quirk_entry sa_quirk_table[] =
{
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "ARCHIVE",
		  "Python 25601*", "*"}, /*quirks*/SA_QUIRK_NOCOMP
	}
};

static	d_open_t	saopen;
static	d_read_t	saread;
static	d_write_t	sawrite;
static	d_close_t	saclose;
static	d_strategy_t	sastrategy;
static	d_ioctl_t	saioctl;
static	periph_init_t	sainit;
static	periph_ctor_t	saregister;
static	periph_dtor_t	sacleanup;
static	periph_start_t	sastart;
static	void		saasync(void *callback_arg, u_int32_t code,
				struct cam_path *path, void *arg);
static	void		sadone(struct cam_periph *periph,
			       union ccb *start_ccb);
static  int		saerror(union ccb *ccb, u_int32_t cam_flags,
				u_int32_t sense_flags);
static int		sacheckeod(struct cam_periph *periph);
static int		sagetparams(struct cam_periph *periph,
				    sa_params params_to_get,
				    u_int32_t *blocksize, u_int8_t *density,
				    u_int32_t *numblocks, int *buff_mode,
				    u_int8_t *write_protect, u_int8_t *speed,
				    int *comp_supported, int *comp_enabled,
				    u_int32_t *comp_algorithm,
				  struct scsi_data_compression_page *comp_page);
static int		sasetparams(struct cam_periph *periph,
				    sa_params params_to_set,
				    u_int32_t blocksize, u_int8_t density,
				    u_int32_t comp_algorithm);
static void		saprevent(struct cam_periph *periph, int action);
static int		sarewind(struct cam_periph *periph);
static int		saspace(struct cam_periph *periph, int count,
				scsi_space_code code);
static int		samount(struct cam_periph *periph);
static int		saretension(struct cam_periph *periph);
static int		sareservereleaseunit(struct cam_periph *periph,
					     int reserve);
static int		saloadunload(struct cam_periph *periph, int load);
static int		saerase(struct cam_periph *periph, int longerase);
static int		sawritefilemarks(struct cam_periph *periph,
					 int nmarks, int setmarks);

static struct periph_driver sadriver =
{
	sainit, "sa",
	TAILQ_HEAD_INITIALIZER(sadriver.units), /* generation */ 0
};

DATA_SET(periphdriver_set, sadriver);

#define SAUNIT(DEV) ((minor(DEV)&0xF0) >> 4)	/* 4 bit unit. */
#define SASETUNIT(DEV, U) makedev(major(DEV), ((U) << 4))

#define SAMODE(z) ((minor(z) & 0x03))
#define SADENSITY(z) (((minor(z) >> 2) & 0x03))

/* For 2.2-stable support */
#ifndef D_TAPE
#define D_TAPE 0
#endif

#define CTLMODE	3
#define SA_CDEV_MAJOR 14
#define SA_BDEV_MAJOR 5

static struct cdevsw sa_cdevsw = 
{
	/*d_open*/	saopen,
	/*d_close*/	saclose,
	/*d_read*/	saread,
	/*d_write*/	sawrite,
	/*d_ioctl*/	saioctl,
	/*d_stop*/	nostop,
	/*d_reset*/	noreset,
	/*d_devtotty*/	nodevtotty,
	/*d_poll*/	seltrue,
	/*d_mmap*/	nommap,
	/*d_strategy*/	sastrategy,
	/*d_name*/	"sa",
	/*d_spare*/	NULL,
	/*d_maj*/	-1,
	/*d_dump*/	nodump,
	/*d_psize*/	nopsize,
	/*d_flags*/	D_TAPE,
	/*d_maxio*/	0,
	/*b_maj*/	-1
};

static struct extend_array *saperiphs;

static int
saopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct cam_periph *periph;
	struct sa_softc *softc;
	int unit;
	int mode;
	int density;
	int error;

	unit = SAUNIT(dev);
	mode = SAMODE(dev);
	density = SADENSITY(dev);

	periph = cam_extend_get(saperiphs, unit);
	if (periph == NULL)
		return (ENXIO);	

	softc = (struct sa_softc *)periph->softc;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE,
	    ("saaopen: dev=0x%x (unit %d , mode %d, density %d)\n", dev,
	     unit, mode, density));

	if (softc->flags & SA_FLAG_INVALID)
		return(ENXIO);

	if ((error = cam_periph_lock(periph, PRIBIO|PCATCH)) != 0) {
		return (error); /* error code from tsleep */
	}

	if ((softc->flags & SA_FLAG_OPEN) == 0) {
		if (cam_periph_acquire(periph) != CAM_REQ_CMP)
			return(ENXIO);

		if ((error = sareservereleaseunit(periph, TRUE)) != 0) {
			cam_periph_unlock(periph);
			cam_periph_release(periph);
			return(error);
		}
	}

	if (error == 0) {
		if ((softc->flags & SA_FLAG_OPEN) != 0) {
			error = EBUSY;
		}
		
		if (error == 0) {
			error = samount(periph);
		}
		/* Perform other checking... */
	}

	if (error == 0) {
		saprevent(periph, PR_PREVENT);
		softc->flags |= SA_FLAG_OPEN;
	}
	
	cam_periph_unlock(periph);
	return (error);
}

static int
saclose(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct	cam_periph *periph;
	struct	sa_softc *softc;
	int	unit;
	int	mode;
	int	error;

	unit = SAUNIT(dev);
	mode = SAMODE(dev);
	periph = cam_extend_get(saperiphs, unit);
	if (periph == NULL)
		return (ENXIO);	

	softc = (struct sa_softc *)periph->softc;

	if ((error = cam_periph_lock(periph, PRIBIO)) != 0) {
		return (error); /* error code from tsleep */
	}

	sacheckeod(periph);

	saprevent(periph, PR_ALLOW);

	switch (mode) {
	case SA_MODE_REWIND:
		sarewind(periph);
		break;
	case SA_MODE_OFFLINE:
		sarewind(periph);
		saloadunload(periph, /*load*/FALSE);
		break;
	case SA_MODE_NOREWIND:
	default:
		break;
	}

	softc->flags &= ~SA_FLAG_OPEN;
	
	/* release the device */
	sareservereleaseunit(periph, FALSE);

	cam_periph_unlock(periph);
	cam_periph_release(periph);

	return (0);	
}

static int
saread(dev_t dev, struct uio *uio, int ioflag)
{
	return(physio(sastrategy, NULL, dev, 1, minphys, uio));
}

static int
sawrite(dev_t dev, struct uio *uio, int ioflag)
{
	return(physio(sastrategy, NULL, dev, 0, minphys, uio));
}

/*
 * Actually translate the requested transfer into one the physical driver
 * can understand.  The transfer is described by a buf and will include
 * only one physical transfer.
 */
static void
sastrategy(struct buf *bp)
{
	struct cam_periph *periph;
	struct sa_softc *softc;
	u_int  unit;
	int    s;
	
	unit = SAUNIT(bp->b_dev);
	periph = cam_extend_get(saperiphs, unit);
	if (periph == NULL) {
		bp->b_error = ENXIO;
		goto bad;
	}
	softc = (struct sa_softc *)periph->softc;

	/*
	 * If it's a null transfer, return immediatly
	 */
	if (bp->b_bcount == 0)
		goto done;

	/* valid request?  */
	if (softc->flags & SA_FLAG_FIXED) {
		/*
		 * Fixed block device.  The byte count must
		 * be a multiple of our block size.
		 */
		if (((softc->blk_mask != ~0)
		  && ((bp->b_bcount & softc->blk_mask) != 0))
		 || ((softc->blk_mask == ~0)
		  && ((bp->b_bcount % softc->min_blk) != 0))) {
			xpt_print_path(periph->path);
			printf("Invalid request.  Fixed block device "
			       "requests must be a multiple "
			       "of %d bytes\n", softc->min_blk);
			bp->b_error = EINVAL;
			goto bad;
		}
	} else if ((bp->b_bcount > softc->max_blk)
		|| (bp->b_bcount < softc->min_blk)
		|| (bp->b_bcount & softc->blk_mask) != 0) {

		xpt_print_path(periph->path);
		printf("Invalid request.  Variable block device "
		       "requests must be ");
		if (softc->blk_mask != 0) {
			printf("a multiple of %d ",
			       (0x1 << softc->blk_gran));
		}
		printf("between %d and %d bytes\n",
		       softc->min_blk, softc->max_blk);
		bp->b_error = EINVAL;
		goto bad;
        }
	
	/*
	 * Mask interrupts so that the pack cannot be invalidated until
	 * after we are in the queue.  Otherwise, we might not properly
	 * clean up one of the buffers.
	 */
	s = splbio();
	
	/*
	 * Place it in the queue of disk activities for this disk
	 */
	bufq_insert_tail(&softc->buf_queue, bp);

	splx(s);
	
	/*
	 * Schedule ourselves for performing the work.
	 */
	xpt_schedule(periph, /* XXX priority */1);

	return;
bad:
	bp->b_flags |= B_ERROR;
done:

	/*
	 * Correctly set the buf to indicate a completed xfer
	 */
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}

static int
saioctl(dev_t dev, u_long cmd, caddr_t arg, int flag, struct proc *p)
{
	struct cam_periph *periph;
	struct sa_softc *softc;
	int unit;
	int mode;
	int density;
	int error;

	unit = SAUNIT(dev);
	mode = SAMODE(dev);
	density = SADENSITY(dev);

	periph = cam_extend_get(saperiphs, unit);
	if (periph == NULL)
		return (ENXIO);	

	softc = (struct sa_softc *)periph->softc;

	/*
	 * Find the device that the user is talking about
	 */
	switch (cmd) {
	case MTIOCGET:
	{
		struct mtget *g = (struct mtget *)arg;

		CAM_DEBUG(periph->path, CAM_DEBUG_TRACE,
			 ("saioctl: MTIOGET\n"));

		bzero(g, sizeof(struct mtget));
		g->mt_type = 0x7;	/* Ultrix compat *//*? */
		g->mt_density = softc->media_density;
		g->mt_blksiz = softc->media_blksize;
		if (softc->flags & SA_FLAG_COMP_UNSUPP) {
			g->mt_comp = MT_COMP_UNSUPP;
			g->mt_comp0 = MT_COMP_UNSUPP;
			g->mt_comp1 = MT_COMP_UNSUPP;
			g->mt_comp2 = MT_COMP_UNSUPP;
			g->mt_comp3 = MT_COMP_UNSUPP;
		} else if ((softc->flags & SA_FLAG_COMP_ENABLED) == 0) {
			g->mt_comp = MT_COMP_DISABLED;
			g->mt_comp0 = MT_COMP_DISABLED;
			g->mt_comp1 = MT_COMP_DISABLED;
			g->mt_comp2 = MT_COMP_DISABLED;
			g->mt_comp3 = MT_COMP_DISABLED;
		} else {
			g->mt_comp = softc->comp_algorithm;
			g->mt_comp0 = softc->comp_algorithm;
			g->mt_comp1 = softc->comp_algorithm;
			g->mt_comp2 = softc->comp_algorithm;
			g->mt_comp3 = softc->comp_algorithm;
		}
		g->mt_density0 = softc->media_density;
		g->mt_density1 = softc->media_density;
		g->mt_density2 = softc->media_density;
		g->mt_density3 = softc->media_density;
		g->mt_blksiz0 = softc->media_blksize;
		g->mt_blksiz1 = softc->media_blksize;
		g->mt_blksiz2 = softc->media_blksize;
		g->mt_blksiz3 = softc->media_blksize;
		error = 0;
		break;
	}
	case MTIOCTOP:
	{
		struct mtop *mt;
		int    count;

		mt = (struct mtop *)arg;

		CAM_DEBUG(periph->path, CAM_DEBUG_TRACE,
			 ("saioctl: op=0x%x count=0x%x\n",
			  mt->mt_op, mt->mt_count));

		count = mt->mt_count;
		switch (mt->mt_op) {
		case MTWEOF:	/* write an end-of-file record */
			error = sawritefilemarks(periph, count,
						   /*setmarks*/FALSE);
			break;
		case MTBSR:	/* backward space record */
		case MTFSR:	/* forward space record */
		case MTBSF:	/* backward space file */
		case MTFSF:	/* forward space file */
		case MTEOD:	/* space to end of recorded medium */
		{
			int nmarks;
			scsi_space_code spaceop;

			nmarks = softc->filemarks;
			error = sacheckeod(periph);
			nmarks -= softc->filemarks;

			if ((mt->mt_op == MTBSR) || (mt->mt_op == MTBSF))
				count = -count;

			if ((mt->mt_op == MTBSF) || (mt->mt_op == MTFSF))
				spaceop = SS_FILEMARKS;
			else if ((mt->mt_op == MTBSR) || (mt->mt_op == MTFSR))
				spaceop = SS_BLOCKS;
			else {
				spaceop = SS_EOD;
				count = 0;
				nmarks = 0;
			}

			nmarks = softc->filemarks;
			error = sacheckeod(periph);
			nmarks -= softc->filemarks;
			if (error == 0)
				error = saspace(periph, count - nmarks,
						spaceop);
			break;
		}
		case MTREW:	/* rewind */
			error = sarewind(periph);
			break;
		case MTERASE:	/* erase */
			error = saerase(periph, count);
			break;
		case MTRETENS:	/* re-tension tape */
			error = saretension(periph);		
			break;
		case MTOFFL:	/* rewind and put the drive offline */
			/*
			 * Be sure to allow media removal before
			 * attempting the eject.
			 */
			saprevent(periph, PR_ALLOW);
			error = sarewind(periph);

			if (error == 0)
				error = saloadunload(periph, /*load*/FALSE);
			else
				break;

			/* XXX KDM */
			softc->flags &= ~SA_FLAG_TAPE_LOCKED;
			softc->flags &= ~SA_FLAG_TAPE_MOUNTED;
			break;
		case MTNOP:	/* no operation, sets status only */
		case MTCACHE:	/* enable controller cache */
		case MTNOCACHE:	/* disable controller cache */
			error = 0;
			break;
		case MTSETBSIZ:	/* Set block size for device */

			error = sasetparams(periph, SA_PARAM_BLOCKSIZE, count,
					    0, 0);
			break;
		case MTSETDNSTY:	/* Set density for device and mode */
			if (count > UCHAR_MAX) {
				error = EINVAL;	
				break;
			} else {
				error = sasetparams(periph, SA_PARAM_DENSITY,
						    0, count, 0);
			}
			break;
		case MTCOMP:	/* enable compression */
			/*
			 * Some devices don't support compression, and
			 * don't like it if you ask them for the
			 * compression page.
			 */
			if ((softc->quirks & SA_QUIRK_NOCOMP)
			 || (softc->flags & SA_FLAG_COMP_UNSUPP)) {
				error = ENODEV;
				break;
			}
			error = sasetparams(periph, SA_PARAM_COMPRESSION,
					    0, 0, count);
			break;
		default:
			error = EINVAL;
		}
		break;
	}
	case MTIOCIEOT:
	case MTIOCEEOT:
		error = 0;
		break;
	default:
		error = cam_periph_ioctl(periph, cmd, arg, saerror);
		break;
	}
	return (error);
}

static void
sainit(void)
{
	cam_status status;
	struct cam_path *path;

	/*
	 * Create our extend array for storing the devices we attach to.
	 */
	saperiphs = cam_extend_new();
	if (saperiphs == NULL) {
		printf("sa: Failed to alloc extend array!\n");
		return;
	}
	
	/*
	 * Install a global async callback.
	 */
	status = xpt_create_path(&path, NULL, CAM_XPT_PATH_ID,
				 CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD);

	if (status == CAM_REQ_CMP) {
		/* Register the async callbacks of interrest */
		struct ccb_setasync csa; /*
					  * This is an immediate CCB,
					  * so using the stack is OK
					  */
		xpt_setup_ccb(&csa.ccb_h, path, /*priority*/5);
		csa.ccb_h.func_code = XPT_SASYNC_CB;
		csa.event_enable = AC_FOUND_DEVICE;
		csa.callback = saasync;
		csa.callback_arg = NULL;
		xpt_action((union ccb *)&csa);
		status = csa.ccb_h.status;
		xpt_free_path(path);
	}

	if (status != CAM_REQ_CMP) {
		printf("sa: Failed to attach master async callback "
		       "due to status 0x%x!\n", status);
	} else {
		/* If we were successfull, register our devsw */
		cdevsw_add_generic(SA_BDEV_MAJOR, SA_CDEV_MAJOR, &sa_cdevsw);
	}
}

static void
sacleanup(struct cam_periph *periph)
{
	cam_extend_release(saperiphs, periph->unit_number);
	xpt_print_path(periph->path);
	printf("removing device entry\n");
	free(periph->softc, M_DEVBUF);
}

static void
saasync(void *callback_arg, u_int32_t code,
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

		if (cgd->pd_type != T_SEQUENTIAL)
			break;

		/*
		 * Allocate a peripheral instance for
		 * this device and start the probe
		 * process.
		 */
		status = cam_periph_alloc(saregister, sacleanup, sastart,
					  "sa", CAM_PERIPH_BIO, cgd->ccb_h.path,
					  saasync, AC_FOUND_DEVICE, cgd);

		if (status != CAM_REQ_CMP
		 && status != CAM_REQ_INPROG)
			printf("saasync: Unable to probe new device "
				"due to status 0x%x\n", status);
		break;
	}
	case AC_LOST_DEVICE:
	{
		int s;
		struct sa_softc *softc;
		struct buf *q_bp;
		struct ccb_setasync csa;

		softc = (struct sa_softc *)periph->softc;

		/*
		 * Insure that no other async callbacks that
		 * might affect this peripheral can come through.
		 */
		s = splcam();

		/*
		 * De-register any async callbacks.
		 */
		xpt_setup_ccb(&csa.ccb_h, periph->path,
			      /* priority */ 5);
		csa.ccb_h.func_code = XPT_SASYNC_CB;
		csa.event_enable = 0;
		csa.callback = saasync;
		csa.callback_arg = periph;
		xpt_action((union ccb *)&csa);

		softc->flags |= SA_FLAG_INVALID;

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
		devstat_remove_entry(&softc->device_stats);

		xpt_print_path(periph->path);
		printf("lost device\n");

		splx(s);

		cam_periph_invalidate(periph);
	}
	case AC_TRANSFER_NEG:
	case AC_SENT_BDR:
	case AC_SCSI_AEN:
	case AC_UNSOL_RESEL:
	case AC_BUS_RESET:
	default:
		break;
	}
}

static cam_status
saregister(struct cam_periph *periph, void *arg)
{
	int s;
	struct sa_softc *softc;
	struct ccb_setasync csa;
	struct ccb_getdev *cgd;
	caddr_t match;
	
	cgd = (struct ccb_getdev *)arg;
	if (periph == NULL) {
		printf("saregister: periph was NULL!!\n");
		return(CAM_REQ_CMP_ERR);
	}

	if (cgd == NULL) {
		printf("saregister: no getdev CCB, can't register device\n");
		return(CAM_REQ_CMP_ERR);
	}

	softc = (struct sa_softc *)malloc(sizeof(*softc),M_DEVBUF,M_NOWAIT);

	if (softc == NULL) {
		printf("saregister: Unable to probe new device. "
		       "Unable to allocate softc\n");				
		return(CAM_REQ_CMP_ERR);
	}

	bzero(softc, sizeof(*softc));
	softc->state = SA_STATE_NORMAL;
	bufq_init(&softc->buf_queue);
	periph->softc = softc;
	cam_extend_set(saperiphs, periph->unit_number, periph);

	/*
	 * See if this device has any quirks.
	 */
	match = cam_quirkmatch((caddr_t)&cgd->inq_data,
			       (caddr_t)sa_quirk_table,
			       sizeof(sa_quirk_table)/sizeof(*sa_quirk_table),
			       sizeof(*sa_quirk_table), scsi_inquiry_match);

	if (match != NULL)
		softc->quirks = ((struct sa_quirk_entry *)match)->quirks;
	else
		softc->quirks = SA_QUIRK_NONE;

	/*
 	 * The SA driver supports a blocksize, but we don't know the
	 * blocksize until we sense the media.  So, set a flag to
	 * indicate that the blocksize is unavailable right now.
	 * We'll clear the flag as soon as we've done a read capacity.
	 */
	devstat_add_entry(&softc->device_stats, "sa",
			  periph->unit_number, 0,
			  DEVSTAT_BS_UNAVAILABLE,
			  cgd->pd_type | DEVSTAT_TYPE_IF_SCSI);
  
	/*
	 * Add an async callback so that we get
	 * notified if this device goes away.
	 */
	xpt_setup_ccb(&csa.ccb_h, periph->path, /* priority */ 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = AC_LOST_DEVICE;
	csa.callback = saasync;
	csa.callback_arg = periph;
	xpt_action((union ccb *)&csa);

	xpt_announce_periph(periph, NULL);

	return(CAM_REQ_CMP);
}

static void
sastart(struct cam_periph *periph, union ccb *start_ccb)
{
	struct sa_softc *softc;

	softc = (struct sa_softc *)periph->softc;

	
	switch (softc->state) {
	case SA_STATE_NORMAL:
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
			start_ccb->ccb_h.ccb_type = SA_CCB_WAITING;
			SLIST_INSERT_HEAD(&periph->ccb_list, &start_ccb->ccb_h,
					  periph_links.sle);
			periph->immediate_priority = CAM_PRIORITY_NONE;
			splx(s);
			wakeup(&periph->ccb_list);
		} else if (bp == NULL) {
			splx(s);
			xpt_release_ccb(start_ccb);
		} else if ((softc->flags & SA_FLAG_ERR_PENDING) != 0) {

			bufq_remove(&softc->buf_queue, bp);
			bp->b_resid = bp->b_bcount;
			bp->b_flags |= B_ERROR;
			if ((softc->flags & SA_FLAG_EOM_PENDING) != 0) {
				if ((bp->b_flags & B_READ) == 0)
					bp->b_error = ENOSPC;
			}
			if ((softc->flags & SA_FLAG_EIO_PENDING) != 0) {
				bp->b_error = EIO;
			}
			softc->flags &= ~SA_FLAG_ERR_PENDING;
			bp = bufq_first(&softc->buf_queue);
			splx(s);
			biodone(bp);
		} else {
			u_int32_t length;

			bufq_remove(&softc->buf_queue, bp);

			if ((softc->flags & SA_FLAG_FIXED) != 0) {
				if (softc->blk_shift != 0) {
					length =
					    bp->b_bcount >> softc->blk_shift;
				} else {
					length =
					    bp->b_bcount / softc->min_blk;
				}
			} else {
				length = bp->b_bcount;
			}

			devstat_start_transaction(&softc->device_stats);

			/*
			 * XXX - Perhaps we should...
			 * suppress illegal length indication if we are
			 * running in variable block mode so that we don't
			 * have to request sense every time our requested
			 * block size is larger than the written block.
			 * The residual information from the ccb allows
			 * us to identify this situation anyway.  The only
			 * problem with this is that we will not get
			 * information about blocks that are larger than
			 * our read buffer unless we set the block size
			 * in the mode page to something other than 0.
			 */
			scsi_sa_read_write(&start_ccb->csio,
					   /*retries*/4,
					   sadone,
					   MSG_SIMPLE_Q_TAG,
					   bp->b_flags & B_READ,
					   /*SILI*/FALSE,
					   softc->flags & SA_FLAG_FIXED,
					   length,
					   bp->b_data,
					   bp->b_bcount,
					   SSD_FULL_SIZE,
					   120 * 60 * 1000); /* 2min */
			start_ccb->ccb_h.ccb_type = SA_CCB_BUFFER_IO;
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
	}
}


static void
sadone(struct cam_periph *periph, union ccb *done_ccb)
{
	struct sa_softc *softc;
	struct ccb_scsiio *csio;

	softc = (struct sa_softc *)periph->softc;
	csio = &done_ccb->csio;
	switch (csio->ccb_h.ccb_type) {
	case SA_CCB_BUFFER_IO:
	{
		struct buf *bp;
		int error;

		bp = (struct buf *)done_ccb->ccb_h.ccb_bp;
		error = 0;
		if ((done_ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			
			if ((error = saerror(done_ccb, 0, 0)) == ERESTART) {
				/*
				 * A retry was scheuled, so
				 * just return.
				 */
				return;
			}
		}

		if (error == EIO) {
			int s;			
			struct buf *q_bp;

			/*
			 * Catastrophic error.  Mark our pack as invalid,
			 * return all queued I/O with EIO, and unfreeze
			 * our queue so that future transactions that
			 * attempt to fix this problem can get to the
			 * device.
			 *
			 */

			s = splbio();
			softc->flags &= ~SA_FLAG_TAPE_MOUNTED;

			while ((q_bp = bufq_first(&softc->buf_queue)) != NULL) {
				bufq_remove(&softc->buf_queue, q_bp);
				q_bp->b_resid = q_bp->b_bcount;
				q_bp->b_error = EIO;
				q_bp->b_flags |= B_ERROR;
				biodone(q_bp);
			}
			splx(s);
		}
		if (error != 0) {
			bp->b_resid = bp->b_bcount;
			bp->b_error = error;
			bp->b_flags |= B_ERROR;
			cam_release_devq(done_ccb->ccb_h.path,
					 /*relsim_flags*/0,
					 /*reduction*/0,
					 /*timeout*/0,
					 /*getcount_only*/0);
		} else {
			bp->b_resid = csio->resid;
			bp->b_error = 0;
			if (csio->resid != 0) {
				bp->b_flags |= B_ERROR;
			}
			if ((bp->b_flags & B_READ) == 0) {
				softc->flags |= SA_FLAG_TAPE_WRITTEN;
				softc->filemarks = 0;
			}
		}

		devstat_end_transaction(&softc->device_stats,
					bp->b_bcount - bp->b_resid,
					done_ccb->csio.tag_action & 0xf,
					(bp->b_flags & B_READ) ? DEVSTAT_READ
							       : DEVSTAT_WRITE);
		biodone(bp);
		break;
	}
	case SA_CCB_WAITING:
	{
		/* Caller will release the CCB */
		wakeup(&done_ccb->ccb_h.cbfcnp);
		return;
	}
	}
	xpt_release_ccb(done_ccb);
}

static int
samount(struct cam_periph *periph)
{
	struct	sa_softc *softc;
	union	ccb *ccb;
	struct	ccb_scsiio *csio;
	int	error;

	softc = (struct sa_softc *)periph->softc;
	ccb = cam_periph_getccb(periph, /* priority */1);
	csio = &ccb->csio;
	error = 0;

	/*
	 * Determine if something has happend since the last
	 * open/mount that would invalidate a mount.  This
	 * will also eat any pending UAs.
	 */
	scsi_test_unit_ready(csio,
			     /*retries*/1,
			     sadone,
			     MSG_SIMPLE_Q_TAG,
			     SSD_FULL_SIZE,
			     /*timeout*/5000);

	cam_periph_runccb(ccb, /*error handler*/NULL, /*cam_flags*/0,
			  /*sense_flags*/0, &softc->device_stats);

	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0) {	
		cam_release_devq(ccb->ccb_h.path,
				 /*relsim_flags*/0,
				 /*reduction*/0, 
				 /*timeout*/0,
				 /*getcount_only*/0);
		softc->flags &= ~SA_FLAG_TAPE_MOUNTED;
	}

	if ((softc->flags & SA_FLAG_TAPE_MOUNTED) == 0) {
		struct	scsi_read_block_limits_data *rblim;
		int	buff_mode, comp_enabled, comp_supported;
		u_int8_t write_protect;

		/*
		 * Clear out old state.
		 */
		softc->flags &= ~(SA_FLAG_TAPE_WP|SA_FLAG_TAPE_WRITTEN|
				  SA_FLAG_ERR_PENDING|SA_FLAG_COMP_ENABLED|
				  SA_FLAG_COMP_UNSUPP);
		softc->filemarks = 0;

		/*
		 * First off, determine block limits.
		 */
		rblim = (struct  scsi_read_block_limits_data *)
		    malloc(sizeof(*rblim), M_TEMP, M_WAITOK);

		scsi_read_block_limits(csio,
				       /*retries*/1,
				       sadone,
				       MSG_SIMPLE_Q_TAG,
				       rblim,
				       SSD_FULL_SIZE,
				       /*timeout*/5000);

		error = cam_periph_runccb(ccb, saerror, /*cam_flags*/0,
					  /*sense_flags*/SF_RETRY_UA,
					  &softc->device_stats);

		xpt_release_ccb(ccb);

		if (error != 0)
			goto exit;

		softc->blk_gran = RBL_GRAN(rblim);
		softc->max_blk = scsi_3btoul(rblim->maximum);
		softc->min_blk = scsi_2btoul(rblim->minimum);
		if (softc->max_blk == softc->min_blk) {
			softc->flags |= SA_FLAG_FIXED;
			if (powerof2(softc->min_blk)) {
				softc->blk_mask = softc->min_blk - 1;
				softc->blk_shift = 0;
				softc->blk_shift = ffs(softc->min_blk) - 1;
			} else {
				softc->blk_mask = ~0;
				softc->blk_shift = 0;
			}
		} else {
			/*
			 * SCSI-III spec allows 0
			 * to mean "unspecified"
			 */
			if (softc->max_blk == 0) {
				softc->max_blk = ~0;
			}
			softc->blk_shift = 0;
			if (softc->blk_gran != 0) {
				softc->blk_mask = softc->blk_gran - 1;
			} else {
				softc->blk_mask = 0;
			}
		}

		/*
		 * Next, perform a mode sense to determine
		 * current density, blocksize, compression etc.
		 */
		error = sagetparams(periph, SA_PARAM_ALL,
				    &softc->media_blksize,
				    &softc->media_density,
				    &softc->media_numblks,
				    &softc->buffer_mode, &write_protect,
				    &softc->speed, &comp_supported,
				    &comp_enabled, &softc->comp_algorithm,
				    NULL);

		if (error != 0)
			goto exit;

		if (write_protect) 
			softc->flags |= SA_FLAG_TAPE_WP;

		if (comp_supported) {
			if (comp_enabled) {
				softc->flags |= SA_FLAG_COMP_ENABLED;

				if (softc->saved_comp_algorithm == 0)
					softc->saved_comp_algorithm =
						softc->comp_algorithm;
			}
		} else
			softc->flags |= SA_FLAG_COMP_UNSUPP;

		if (softc->buffer_mode != SMH_SA_BUF_MODE_NOBUF)
			goto exit;

		error = sasetparams(periph, SA_PARAM_BUFF_MODE, 0, 0, 0);

		if (error == 0)
			softc->buffer_mode = SMH_SA_BUF_MODE_SIBUF;
exit:
		if (rblim != NULL)
			free(rblim, M_TEMP);

		if (error != 0) {
			cam_release_devq(ccb->ccb_h.path,
					 /*relsim_flags*/0,
					 /*reduction*/0, 
					 /*timeout*/0,
					 /*getcount_only*/0);
		}
	} else
		xpt_release_ccb(ccb);

	return (error);
}

static int
sacheckeod(struct cam_periph *periph)
{
	int	error;
	int	markswanted;
	struct	sa_softc *softc;

	softc = (struct sa_softc *)periph->softc;
	markswanted = 0;

	if ((softc->flags & SA_FLAG_TAPE_WRITTEN) != 0) {
		markswanted++;

		if ((softc->flags & SA_FLAG_2FM_AT_EOD) != 0)
			markswanted++;
	}

	if (softc->filemarks < markswanted) {
		markswanted -= softc->filemarks;
		error = sawritefilemarks(periph, markswanted,
					 /*setmarks*/FALSE);
	} else {
		error = 0;
	}
	return (error);
}

static int
saerror(union ccb *ccb, u_int32_t cam_flags, u_int32_t sense_flags)
{
	struct	cam_periph *periph;
	struct	sa_softc *softc;
	struct	ccb_scsiio *csio;
	struct	scsi_sense_data *sense;
	int	error_code, sense_key, asc, ascq;
	int	error;

	periph = xpt_path_periph(ccb->ccb_h.path);
	softc = (struct sa_softc *)periph->softc;
	csio = &ccb->csio;
	sense = &csio->sense_data;
	scsi_extract_sense(sense, &error_code, &sense_key, &asc, &ascq);
	error = 0;

	if (((csio->ccb_h.status & CAM_STATUS_MASK) == CAM_SCSI_STATUS_ERROR)
	 && ((sense->flags & (SSD_EOM|SSD_FILEMARK|SSD_ILI)) != 0)
	 && ((sense_key == SSD_KEY_NO_SENSE)
	  || (sense_key == SSD_KEY_BLANK_CHECK))) {
		u_int32_t info;
		u_int32_t resid;
		int	  defer_action;

		/*
		 * Filter out some sense codes of interest.
		 */
		if ((sense->error_code & SSD_ERRCODE_VALID) != 0) {
			info = scsi_4btoul(sense->info);
			resid = info;
			if ((softc->flags & SA_FLAG_FIXED) != 0)
				resid *= softc->media_blksize;
		} else {
			resid = csio->dxfer_len;
			info = resid;
			if ((softc->flags & SA_FLAG_FIXED) != 0)
				info /= softc->media_blksize;
		}
		if ((resid > 0 && resid < csio->dxfer_len)
		 && (softc->flags & SA_FLAG_FIXED) != 0)
			defer_action = TRUE;
		else
			defer_action = FALSE;

		if ((sense->flags & SSD_EOM) != 0
		 || (sense_key == 0x8 /* BLANK CHECK*/)) {
			csio->resid = resid;
			if (defer_action) {
				softc->flags |= SA_FLAG_EOM_PENDING;
			} else {
				if (csio->cdb_io.cdb_bytes[0] == SA_WRITE)
					error = ENOSPC;
			}
		}
		if ((sense->flags & SSD_FILEMARK) != 0) {
			csio->resid = resid;
			if (defer_action)
				softc->flags |= SA_FLAG_EOF_PENDING;
		}
		if (sense->flags & SSD_ILI) {
			if (info < 0) {
				/*
				 * The record was too big.
				 */
				xpt_print_path(csio->ccb_h.path);
				printf("%d-byte tape record bigger "
				       "than suplied read buffer\n",
				       csio->dxfer_len - info);
				csio->resid = csio->dxfer_len;
				error = EIO;
			} else {
				csio->resid = resid;
				if ((softc->flags & SA_FLAG_FIXED) != 0) {
					if (defer_action)
						softc->flags |=
						    SA_FLAG_EIO_PENDING;
					else
						error = EIO;
				}
			}
		}
	}
	if (error == 0)
		error = cam_periph_error(ccb, cam_flags, sense_flags,
					 &softc->saved_ccb);

	return (error);
}

static int
sagetparams(struct cam_periph *periph, sa_params params_to_get,
	    u_int32_t *blocksize, u_int8_t *density, u_int32_t *numblocks,
	    int *buff_mode, u_int8_t *write_protect, u_int8_t *speed,
	    int *comp_supported, int *comp_enabled, u_int32_t *comp_algorithm,
	    struct scsi_data_compression_page *comp_page)
{
	union ccb *ccb;
	void *mode_buffer;
	struct scsi_mode_header_6 *mode_hdr;
	struct scsi_mode_blk_desc *mode_blk;
	struct scsi_data_compression_page *ncomp_page;
	int mode_buffer_len;
	struct sa_softc *softc;
	int error;
	cam_status status;

	softc = (struct sa_softc *)periph->softc;

	ccb = cam_periph_getccb(periph, /*priority*/ 1);

retry:
	mode_buffer_len = sizeof(*mode_hdr) + sizeof(*mode_blk);

	if (params_to_get & SA_PARAM_COMPRESSION) {
		if (softc->quirks & SA_QUIRK_NOCOMP) {
			*comp_supported = FALSE;
			params_to_get &= ~SA_PARAM_COMPRESSION;
		} else
			mode_buffer_len +=
				sizeof(struct scsi_data_compression_page);
	}

	mode_buffer = malloc(mode_buffer_len, M_TEMP, M_WAITOK);

	bzero(mode_buffer, mode_buffer_len);

	mode_hdr = (struct scsi_mode_header_6 *)mode_buffer;
	mode_blk = (struct scsi_mode_blk_desc *)&mode_hdr[1];

	if (params_to_get & SA_PARAM_COMPRESSION)
		ncomp_page = (struct scsi_data_compression_page *)&mode_blk[1];
	else
		ncomp_page = NULL;

	scsi_mode_sense(&ccb->csio,
			/*retries*/ 1,
			/*cbfcnp*/ sadone,
			/*tag_action*/ MSG_SIMPLE_Q_TAG,
			/*dbd*/ FALSE,
			/*page_code*/ SMS_PAGE_CTRL_CURRENT,
			/*page*/ (params_to_get & SA_PARAM_COMPRESSION) ?
				  SA_DATA_COMPRESSION_PAGE :
				  SMS_VENDOR_SPECIFIC_PAGE,
			/*param_buf*/ mode_buffer,
			/*param_len*/ mode_buffer_len,
			/*sense_len*/ SSD_FULL_SIZE,
			/*timeout*/ 5000);

	error = cam_periph_runccb(ccb, saerror, /*cam_flags*/ 0,
				  /*sense_flags*/SF_NO_PRINT,
				  &softc->device_stats);

	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path,
				 /* relsim_flags */0,
				 /* opening_reduction */0,
				 /* timeout */0,
				 /* getcount_only */ FALSE);

	status = ccb->ccb_h.status & CAM_STATUS_MASK;

	if (error == EINVAL
	 && (params_to_get & SA_PARAM_COMPRESSION) != 0) {
		/*
		 * Most likely doesn't support the compression
		 * page.  Remeber this for the future and attempt
		 * the request without asking for compression info.
		 */
		softc->quirks |= SA_QUIRK_NOCOMP;
		free(mode_buffer, M_TEMP);
		goto retry;
	} else if (error == 0) {
		struct scsi_data_compression_page *temp_comp_page;

		temp_comp_page = NULL;

		/*
		 * If the user only wants the compression information, and
		 * the device doesn't send back the block descriptor, it's
		 * no big deal.  If the user wants more than just
		 * compression, though, and the device doesn't pass back the
		 * block descriptor, we need to send another mode sense to
		 * get the block descriptor.
		 */
		if ((mode_hdr->blk_desc_len == 0) 
		 && (params_to_get & SA_PARAM_COMPRESSION)
		 && ((params_to_get & ~(SA_PARAM_COMPRESSION)) != 0)) {

			/*
			 * Decrease the mode buffer length by the size of
			 * the compression page, to make sure the data
			 * there doesn't get overwritten.
			 */
			mode_buffer_len -= sizeof(*ncomp_page);

			/*
			 * Now move the compression page that we presumably
			 * got back down the memory chunk a little bit so
			 * it doesn't get spammed.
			 */
			temp_comp_page =
			      (struct scsi_data_compression_page *)&mode_hdr[1];
			bcopy(temp_comp_page, ncomp_page, sizeof(*ncomp_page));

			/*
			 * Now, we issue another mode sense and just ask
			 * for the block descriptor, etc.
			 */
			scsi_mode_sense(&ccb->csio,
					/*retries*/ 1,
					/*cbfcnp*/ sadone,
					/*tag_action*/ MSG_SIMPLE_Q_TAG,
					/*dbd*/ FALSE,
					/*page_code*/ SMS_PAGE_CTRL_CURRENT,
					/*page*/ SMS_VENDOR_SPECIFIC_PAGE,
					/*param_buf*/ mode_buffer,
					/*param_len*/ mode_buffer_len,
					/*sense_len*/ SSD_FULL_SIZE,
					/*timeout*/ 5000);

			error = cam_periph_runccb(ccb, saerror, /*cam_flags*/ 0,
						  /*sense_flags*/ 0,
						  &softc->device_stats);

			if (error != 0)
				goto sagetparamsexit;

		}

		if (params_to_get & SA_PARAM_BLOCKSIZE)
			*blocksize = scsi_3btoul(mode_blk->blklen);

		if (params_to_get & SA_PARAM_NUMBLOCKS)
			*numblocks = scsi_3btoul(mode_blk->nblocks);

		if (params_to_get & SA_PARAM_BUFF_MODE)
			*buff_mode = mode_hdr->dev_spec & SMH_SA_BUF_MODE_MASK;

		if (params_to_get & SA_PARAM_DENSITY)
			*density = mode_blk->density;

		if (params_to_get & SA_PARAM_WP)
			*write_protect = (mode_hdr->dev_spec & SMH_SA_WP) ?
					 TRUE : FALSE;
		if (params_to_get & SA_PARAM_SPEED)
			*speed = mode_hdr->dev_spec & SMH_SA_SPEED_MASK;

		if (params_to_get & SA_PARAM_COMPRESSION) {
			*comp_supported =(ncomp_page->dce_and_dcc & SA_DCP_DCC)?
					 TRUE : FALSE;
			*comp_enabled = (ncomp_page->dce_and_dcc & SA_DCP_DCE)?
					TRUE : FALSE;
			*comp_algorithm =
				scsi_4btoul(ncomp_page->comp_algorithm);
			if (comp_page != NULL)
				bcopy(ncomp_page, comp_page,sizeof(*comp_page));
		}

	} else if (status == CAM_SCSI_STATUS_ERROR) {
		/* Tell the user about the fatal error. */
		scsi_sense_print(&ccb->csio);
	}

sagetparamsexit:

	xpt_release_ccb(ccb);
	free(mode_buffer, M_TEMP);
	return(error);
}

/*
 * The purpose of this function is to set one of four different parameters
 * for a tape drive:
 *	- blocksize
 *	- density
 *	- compression / compression algorithm
 *	- buffering mode
 *
 * The assumption is that this will be called from saioctl(), and therefore
 * from a process context.  Thus the waiting malloc calls below.  If that
 * assumption ever changes, the malloc calls should be changed to be
 * NOWAIT mallocs.
 *
 * Any or all of the four parameters may be set when this function is
 * called.  It should handle setting more than one parameter at once.
 */
static int
sasetparams(struct cam_periph *periph, sa_params params_to_set,
	    u_int32_t blocksize, u_int8_t density, u_int32_t comp_algorithm)
{
	struct sa_softc *softc;
	u_int32_t current_blocksize;
	u_int32_t current_comp_algorithm;
	u_int8_t current_density;
	u_int8_t current_speed;
	int comp_enabled, comp_supported;
	void *mode_buffer;
	int mode_buffer_len;
	struct scsi_mode_header_6 *mode_hdr;
	struct scsi_mode_blk_desc *mode_blk;
	struct scsi_data_compression_page *comp_page;
	struct scsi_data_compression_page *current_comp_page;
	int buff_mode;
	union ccb *ccb;
	int error;

	softc = (struct sa_softc *)periph->softc;

	/* silence the compiler */
	ccb = NULL;

	current_comp_page = malloc(sizeof(*current_comp_page),M_TEMP, M_WAITOK);

	/*
	 * Since it doesn't make sense to set the number of blocks, or
	 * write protection, we won't try to get the current value.  We
	 * always want to get the blocksize, so we can set it back to the
	 * proper value.
	 */
	error = sagetparams(periph, params_to_set | SA_PARAM_BLOCKSIZE |
			    SA_PARAM_SPEED, &current_blocksize,
			    &current_density, NULL, &buff_mode, NULL,
			    &current_speed, &comp_supported, &comp_enabled,
			    &current_comp_algorithm, current_comp_page);

	if (error != 0) {
		free(current_comp_page, M_TEMP);
		return(error);
	}

	mode_buffer_len = sizeof(*mode_hdr) + sizeof(*mode_blk);
	if (params_to_set & SA_PARAM_COMPRESSION)
		mode_buffer_len += sizeof(struct scsi_data_compression_page);

	mode_buffer = malloc(mode_buffer_len, M_TEMP, M_WAITOK);

	bzero(mode_buffer, mode_buffer_len);

	mode_hdr = (struct scsi_mode_header_6 *)mode_buffer;
	mode_blk = (struct scsi_mode_blk_desc *)&mode_hdr[1];

	if (params_to_set & SA_PARAM_COMPRESSION) {
		comp_page = (struct scsi_data_compression_page *)&mode_blk[1];
		bcopy(current_comp_page, comp_page, sizeof(*comp_page));
	} else
		comp_page = NULL;

	/*
	 * If the caller wants us to set the blocksize, use the one they
	 * pass in.  Otherwise, use the blocksize we got back from the
	 * mode select above.
	 */
	if (params_to_set & SA_PARAM_BLOCKSIZE)
		scsi_ulto3b(blocksize, mode_blk->blklen);
	else
		scsi_ulto3b(current_blocksize, mode_blk->blklen);

	/*
	 * 0x7f means "same as before"
	 */
	if (params_to_set & SA_PARAM_DENSITY)
		mode_blk->density = density;
	else
		mode_blk->density = 0x7f;

	/*
	 * For mode selects, these two fields must be zero.
	 */
	mode_hdr->data_length = 0;
	mode_hdr->medium_type = 0;

	/* set the speed to the current value */
	mode_hdr->dev_spec = current_speed;

	/* set single-initiator buffering mode */
	mode_hdr->dev_spec |= SMH_SA_BUF_MODE_SIBUF;

	mode_hdr->blk_desc_len = sizeof(struct scsi_mode_blk_desc);

	/*
	 * First, if the user wants us to set the compression algorithm or
	 * just turn compression on, check to make sure that this drive
	 * supports compression.
	 */
	if ((params_to_set & SA_PARAM_COMPRESSION) 
	 && (current_comp_page->dce_and_dcc & SA_DCP_DCC)) {

		/*
		 * If the compression algorithm is 0, disable compression.
		 * If the compression algorithm is non-zero, enable
		 * compression and set the compression type to the
		 * specified compression algorithm, unless the algorithm is
		 * MT_COMP_ENABLE.  In that case, we look at the
		 * compression algorithm that is currently set and if it is
		 * non-zero, we leave it as-is.  If it is zero, and we have
		 * saved a compression algorithm from a time when
		 * compression was enabled before, set the compression to
		 * the saved value.
		 */
		if (comp_algorithm == 0) {
			/* disable compression */
			comp_page->dce_and_dcc &= ~SA_DCP_DCE;
		} else {
			/* enable compression */
			comp_page->dce_and_dcc |= SA_DCP_DCE;

			/* enable decompression */
			comp_page->dde_and_red |= SA_DCP_DDE;

			if (comp_algorithm != MT_COMP_ENABLE) {
				/* set the compression algorithm */
				scsi_ulto4b(comp_algorithm,
					    comp_page->comp_algorithm);

			} else if ((scsi_4btoul(comp_page->comp_algorithm) == 0)
				&& (softc->saved_comp_algorithm != 0)) {
				scsi_ulto4b(softc->saved_comp_algorithm,
					    comp_page->comp_algorithm);
			}
		}
	} else if (params_to_set & SA_PARAM_COMPRESSION) {
		/*
		 * The drive doesn't support compression, so turn off the
		 * set compression bit.
		 */
		params_to_set &= ~SA_PARAM_COMPRESSION;

		/*
		 * Should probably do something other than a printf...like
		 * set a flag in the softc saying that this drive doesn't
		 * support compression.
		 */
		xpt_print_path(periph->path);
		printf("sasetparams: device does not support compression\n");

		/*
		 * If that was the only thing the user wanted us to set,
		 * clean up allocated resources and return with 'operation
		 * not supported'.
		 */
		if (params_to_set == SA_PARAM_NONE) {
			free(mode_buffer, M_TEMP);
			return(ENODEV);
		}
	
		/*
		 * That wasn't the only thing the user wanted us to set.
		 * So, decrease the stated mode buffer length by the size
		 * of the compression mode page.
		 */
		mode_buffer_len -= sizeof(*comp_page);
	}

	ccb = cam_periph_getccb(periph, /*priority*/ 1);

	scsi_mode_select(&ccb->csio,
			/*retries*/1,
			/*cbfcnp*/ sadone,
			/*tag_action*/ MSG_SIMPLE_Q_TAG,
			/*scsi_page_fmt*/(params_to_set & SA_PARAM_COMPRESSION)?
					 TRUE : FALSE,
			/*save_pages*/ FALSE,
			/*param_buf*/ mode_buffer,
			/*param_len*/ mode_buffer_len,
			/*sense_len*/ SSD_FULL_SIZE,
			/*timeout*/ 5000);

	error = cam_periph_runccb(ccb, saerror, /*cam_flags*/ 0,
				  /*sense_flags*/ 0, &softc->device_stats);

	if (error == 0) {
		xpt_release_ccb(ccb);
	} else {
		if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
			cam_release_devq(ccb->ccb_h.path,
					 /*relsim_flags*/0,
					 /*reduction*/0, 
					 /*timeout*/0,
					 /*getcount_only*/0);
		/*
		 * If we were setting the blocksize, and that failed, we
		 * want to set it to its original value.  If we weren't
		 * setting the blocksize, we don't want to change it.
		 */
		scsi_ulto3b(current_blocksize, mode_blk->blklen);

		/*
		 * 0x7f means "same as before".
		 */
		if (params_to_set & SA_PARAM_DENSITY)
			mode_blk->density = current_density;
		else
			mode_blk->density = 0x7f;

		if (params_to_set & SA_PARAM_COMPRESSION)
			bcopy(current_comp_page, comp_page,
			      sizeof(struct scsi_data_compression_page));

		/*
		 * The retry count is the only CCB field that might have been
		 * changed that we care about, so reset it back to 1.
		 */
		ccb->ccb_h.retry_count = 1;
		cam_periph_runccb(ccb, saerror, /*cam_flags*/ 0,
				  /*sense_flags*/ 0, &softc->device_stats);

		if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
			cam_release_devq(ccb->ccb_h.path,
					 /*relsim_flags*/0,
					 /*reduction*/0, 
					 /*timeout*/0,
					 /*getcount_only*/0);

		xpt_release_ccb(ccb);
	}

	if (params_to_set & SA_PARAM_COMPRESSION)
		free(current_comp_page, M_TEMP);

	free(mode_buffer, M_TEMP);
	return(error);
}

static void
saprevent(struct cam_periph *periph, int action)
{
	struct	sa_softc *softc;
	union	ccb *ccb;		
	int	error;
		
	softc = (struct sa_softc *)periph->softc;

	if (((action == PR_ALLOW)
	  && (softc->flags & SA_FLAG_TAPE_LOCKED) == 0)
	 || ((action == PR_PREVENT)
	  && (softc->flags & SA_FLAG_TAPE_LOCKED) != 0)) {
		return;
	}

	ccb = cam_periph_getccb(periph, /*priority*/1);

	scsi_prevent(&ccb->csio,
		     /*retries*/0,
		     /*cbcfp*/sadone,
		     MSG_SIMPLE_Q_TAG,
		     action,
		     SSD_FULL_SIZE,
		     60000);

	error = cam_periph_runccb(ccb, saerror, /*cam_flags*/0,
				  /*sense_flags*/0, &softc->device_stats);

	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path,
				 /*relsim_flags*/0,
				 /*reduction*/0, 
				 /*timeout*/0,
				 /*getcount_only*/0);
		

	if (error == 0) {
		if (action == PR_ALLOW)
			softc->flags &= ~SA_FLAG_TAPE_LOCKED;
		else
			softc->flags |= SA_FLAG_TAPE_LOCKED;
	}

	xpt_release_ccb(ccb);
}

static int
sarewind(struct cam_periph *periph)
{
	union	ccb *ccb;
	struct	sa_softc *softc;
	int	error;
		
	softc = (struct sa_softc *)periph->softc;

	ccb = cam_periph_getccb(periph, /*priority*/1);

	/*
	 * Put in a 2 hour timeout to deal with especially slow tape drives.
	 */
	scsi_rewind(&ccb->csio,
		    /*retries*/1,
		    /*cbcfp*/sadone,
		    MSG_SIMPLE_Q_TAG,
		    /*immediate*/FALSE,
		    SSD_FULL_SIZE,
		    (120 * 60 * 1000)); /* 2 hours */

	error = cam_periph_runccb(ccb, saerror, /*cam_flags*/0,
				  /*sense_flags*/0, &softc->device_stats);

	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path,
				 /*relsim_flags*/0,
				 /*reduction*/0, 
				 /*timeout*/0,
				 /*getcount_only*/0);

	xpt_release_ccb(ccb);

	return (error);
}

static int
saspace(struct cam_periph *periph, int count, scsi_space_code code)
{
	union	ccb *ccb;
	struct	sa_softc *softc;
	int	error;
		
	softc = (struct sa_softc *)periph->softc;

	ccb = cam_periph_getccb(periph, /*priority*/1);

	scsi_space(&ccb->csio,
		   /*retries*/1,
		   /*cbcfp*/sadone,
		   MSG_SIMPLE_Q_TAG,
		   code, count,
		   SSD_FULL_SIZE,
		   60 * 60 *1000);

	error = cam_periph_runccb(ccb, saerror, /*cam_flags*/0,
				  /*sense_flags*/0, &softc->device_stats);

	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path,
				 /*relsim_flags*/0,
				 /*reduction*/0, 
				 /*timeout*/0,
				 /*getcount_only*/0);

	xpt_release_ccb(ccb);

	return (error);
}

static int
sawritefilemarks(struct cam_periph *periph, int nmarks, int setmarks)
{
	union	ccb *ccb;
	struct	sa_softc *softc;
	int	error;

	softc = (struct sa_softc *)periph->softc;

	ccb = cam_periph_getccb(periph, /*priority*/1);

	scsi_write_filemarks(&ccb->csio,
			     /*retries*/1,
			     /*cbcfp*/sadone,
			     MSG_SIMPLE_Q_TAG,
			     /*immediate*/FALSE,
			     setmarks,
			     nmarks,
			     SSD_FULL_SIZE,
			     60000);

	error = cam_periph_runccb(ccb, saerror, /*cam_flags*/0,
				  /*sense_flags*/0, &softc->device_stats);

	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path,
				 /*relsim_flags*/0,
				 /*reduction*/0, 
				 /*timeout*/0,
				 /*getcount_only*/0);

	if (error == 0) {
		struct sa_softc *softc;

		softc = (struct sa_softc *)periph->softc;
		softc->filemarks += nmarks;
	}

	xpt_release_ccb(ccb);

	return (error);
}

static int
saretension(struct cam_periph *periph)
{
	union ccb *ccb;
	struct sa_softc *softc;
	int error;

	softc = (struct sa_softc *)periph->softc;

	ccb = cam_periph_getccb(periph, /*priority*/1);

	scsi_load_unload(&ccb->csio,
			 /*retries*/ 1,
			 /*cbfcnp*/ sadone,
			 MSG_SIMPLE_Q_TAG,
			 /*immediate*/ FALSE,
			 /*eot*/ FALSE,
			 /*reten*/ TRUE,
			 /*load*/ TRUE,
			 SSD_FULL_SIZE,
			 60000);

	error = cam_periph_runccb(ccb, saerror, /*cam_flags*/0,
				  /*sense_flags*/0, &softc->device_stats);

	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path,
				 /*relsim_flags*/0,
				 /*reduction*/0, 
				 /*timeout*/0,
				 /*getcount_only*/0);

	xpt_release_ccb(ccb);

	return(error);
}

static int
sareservereleaseunit(struct cam_periph *periph, int reserve)
{
	union ccb *ccb;
	struct sa_softc *softc;
	int error;

	softc = (struct sa_softc *)periph->softc;

	ccb = cam_periph_getccb(periph, /*priority*/ 1);

	scsi_reserve_release_unit(&ccb->csio,
				  /*retries*/ 1,
				  /*cbfcnp*/ sadone,
				  /*tag_action*/ MSG_SIMPLE_Q_TAG,
				  /*third_party*/ FALSE,
				  /*third_party_id*/ 0,
				  /*sense_len*/ SSD_FULL_SIZE,
				  /*timeout*/ 5000,
				  reserve);

	/*
	 * We set SF_RETRY_UA, since this is often the first command run
	 * when a tape device is opened, and there may be a unit attention
	 * condition pending.
	 */
	error = cam_periph_runccb(ccb, saerror, /*cam_flags*/0,
				  /*sense_flags*/SF_RETRY_UA,
				  &softc->device_stats);

	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path,
				 /*relsim_flags*/0,
				 /*reduction*/0, 
				 /*timeout*/0,
				 /*getcount_only*/0);

	xpt_release_ccb(ccb);

	return (error);
}

static int
saloadunload(struct cam_periph *periph, int load)
{
	union	ccb *ccb;
	struct	sa_softc *softc;
	int	error;

	softc = (struct sa_softc *)periph->softc;

	ccb = cam_periph_getccb(periph, /*priority*/1);

	scsi_load_unload(&ccb->csio,
			 /*retries*/1,
			 /*cbfcnp*/sadone,
			 MSG_SIMPLE_Q_TAG,
			 /*immediate*/FALSE,
			 /*eot*/FALSE,
			 /*reten*/FALSE,
			 load,
			 SSD_FULL_SIZE,
			 60000);

	error = cam_periph_runccb(ccb, saerror, /*cam_flags*/0,
				  /*sense_flags*/0, &softc->device_stats);

	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path,
				 /*relsim_flags*/0,
				 /*reduction*/0, 
				 /*timeout*/0,
				 /*getcount_only*/0);

	xpt_release_ccb(ccb);

	return (error);
}

static int
saerase(struct cam_periph *periph, int longerase)
{

	union	ccb *ccb;
	struct	sa_softc *softc;
	int error;

	softc = (struct sa_softc *)periph->softc;

	ccb = cam_periph_getccb(periph, /*priority*/ 1);

	scsi_erase(&ccb->csio,
		   /*retries*/ 1,
		   /*cbfcnp*/ sadone,
		   /*tag_action*/ MSG_SIMPLE_Q_TAG,
		   /*immediate*/ FALSE,
		   /*long_erase*/ longerase,
		   /*sense_len*/ SSD_FULL_SIZE,
		   /*timeout*/ 4 * 60 * 60 * 1000);  /* 4 hours */

	error = cam_periph_runccb(ccb, saerror, /*cam_flags*/0,
				  /*sense_flags*/0, &softc->device_stats);

	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path,
				 /*relsim_flags*/0,
				 /*reduction*/0, 
				 /*timeout*/0,
				 /*getcount_only*/0);

	xpt_release_ccb(ccb);

	return (error);
}

#endif /* KERNEL */

/*
 * Read tape block limits command.
 */
void
scsi_read_block_limits(struct ccb_scsiio *csio, u_int32_t retries,
		   void (*cbfcnp)(struct cam_periph *, union ccb *),
		   u_int8_t tag_action,
		   struct scsi_read_block_limits_data *rlimit_buf,
		   u_int8_t sense_len, u_int32_t timeout)
{
	struct scsi_read_block_limits *scsi_cmd;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_IN,
		      tag_action,
		      /*data_ptr*/(u_int8_t *)rlimit_buf,
		      /*dxfer_len*/sizeof(*rlimit_buf),
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);

	scsi_cmd = (struct scsi_read_block_limits *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));
	scsi_cmd->opcode = READ_BLOCK_LIMITS;
}

void
scsi_sa_read_write(struct ccb_scsiio *csio, u_int32_t retries,
		   void (*cbfcnp)(struct cam_periph *, union ccb *),
		   u_int8_t tag_action, int readop, int sli,
		   int fixed, u_int32_t length, u_int8_t *data_ptr,
		   u_int32_t dxfer_len, u_int8_t sense_len, u_int32_t timeout)
{
	struct scsi_sa_rw *scsi_cmd;

	scsi_cmd = (struct scsi_sa_rw *)&csio->cdb_io.cdb_bytes;
	scsi_cmd->opcode = readop ? SA_READ : SA_WRITE;
	scsi_cmd->sli_fixed = 0;
	if (sli && readop)
		scsi_cmd->sli_fixed |= SAR_SLI;
	if (fixed)
		scsi_cmd->sli_fixed |= SARW_FIXED;
	scsi_ulto3b(length, scsi_cmd->length);
	scsi_cmd->control = 0;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/readop ? CAM_DIR_IN : CAM_DIR_OUT,
		      tag_action,
		      data_ptr,
		      dxfer_len, 
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}

void
scsi_load_unload(struct ccb_scsiio *csio, u_int32_t retries,         
		 void (*cbfcnp)(struct cam_periph *, union ccb *),   
		 u_int8_t tag_action, int immediate, int eot,
		 int reten, int load, u_int8_t sense_len,
		 u_int32_t timeout)
{
	struct scsi_load_unload *scsi_cmd;

	scsi_cmd = (struct scsi_load_unload *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));
	scsi_cmd->opcode = LOAD_UNLOAD;
	if (immediate)
		scsi_cmd->immediate = SLU_IMMED;
	if (eot)
		scsi_cmd->eot_reten_load |= SLU_EOT;
	if (reten)
		scsi_cmd->eot_reten_load |= SLU_RETEN;
	if (load)
		scsi_cmd->eot_reten_load |= SLU_LOAD;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_NONE,
		      tag_action,
		      /*data_ptr*/NULL,
		      /*dxfer_len*/0,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);	
}

void
scsi_rewind(struct ccb_scsiio *csio, u_int32_t retries,         
	    void (*cbfcnp)(struct cam_periph *, union ccb *),   
	    u_int8_t tag_action, int immediate, u_int8_t sense_len,     
	    u_int32_t timeout)
{
	struct scsi_rewind *scsi_cmd;

	scsi_cmd = (struct scsi_rewind *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));
	scsi_cmd->opcode = REWIND;
	if (immediate)
		scsi_cmd->immediate = SREW_IMMED;
	
	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_NONE,
		      tag_action,
		      /*data_ptr*/NULL,
		      /*dxfer_len*/0,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}

void
scsi_space(struct ccb_scsiio *csio, u_int32_t retries,
	   void (*cbfcnp)(struct cam_periph *, union ccb *),
	   u_int8_t tag_action, scsi_space_code code,
	   u_int32_t count, u_int8_t sense_len, u_int32_t timeout)
{
	struct scsi_space *scsi_cmd;

	scsi_cmd = (struct scsi_space *)&csio->cdb_io.cdb_bytes;
	scsi_cmd->opcode = SPACE;
	scsi_cmd->code = code;
	scsi_ulto3b(count, scsi_cmd->count);
	scsi_cmd->control = 0;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_NONE,
		      tag_action,
		      /*data_ptr*/NULL,
		      /*dxfer_len*/0,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}

void
scsi_write_filemarks(struct ccb_scsiio *csio, u_int32_t retries,
		     void (*cbfcnp)(struct cam_periph *, union ccb *),
		     u_int8_t tag_action, int immediate, int setmark,
		     u_int32_t num_marks, u_int8_t sense_len,
		     u_int32_t timeout)
{
	struct scsi_write_filemarks *scsi_cmd;

	scsi_cmd = (struct scsi_write_filemarks *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));
	scsi_cmd->opcode = WRITE_FILEMARKS;
	if (immediate)
		scsi_cmd->byte2 |= SWFMRK_IMMED;
	if (setmark)
		scsi_cmd->byte2 |= SWFMRK_WSMK;
	
	scsi_ulto3b(num_marks, scsi_cmd->num_marks);

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/CAM_DIR_NONE,
		      tag_action,
		      /*data_ptr*/NULL,
		      /*dxfer_len*/0,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}

/*
 * The reserve and release unit commands differ only by their opcodes.
 */
void
scsi_reserve_release_unit(struct ccb_scsiio *csio, u_int32_t retries,
			  void (*cbfcnp)(struct cam_periph *, union ccb *),
			  u_int8_t tag_action, int third_party,
			  int third_party_id, u_int8_t sense_len,
			  u_int32_t timeout, int reserve)
{
	struct scsi_reserve_release_unit *scsi_cmd;

	scsi_cmd = (struct scsi_reserve_release_unit *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));

	if (reserve)
		scsi_cmd->opcode = RESERVE_UNIT;
	else
		scsi_cmd->opcode = RELEASE_UNIT;

	if (third_party) {
		scsi_cmd->lun_thirdparty |= SRRU_3RD_PARTY;
		scsi_cmd->lun_thirdparty |=
			((third_party_id << SRRU_3RD_SHAMT) & SRRU_3RD_MASK);
	}

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/ CAM_DIR_NONE,
		      tag_action,
		      /*data_ptr*/ NULL,
		      /*dxfer_len*/ 0,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}

void
scsi_erase(struct ccb_scsiio *csio, u_int32_t retries,
	   void (*cbfcnp)(struct cam_periph *, union ccb *),
	   u_int8_t tag_action, int immediate, int long_erase,
	   u_int8_t sense_len, u_int32_t timeout)
{
	struct scsi_erase *scsi_cmd;

	scsi_cmd = (struct scsi_erase *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));

	scsi_cmd->opcode = ERASE;

	if (immediate)
		scsi_cmd->lun_imm_long |= SE_IMMED;

	if (long_erase)
		scsi_cmd->lun_imm_long |= SE_LONG;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/ CAM_DIR_NONE,
		      tag_action,
		      /*data_ptr*/ NULL,
		      /*dxfer_len*/ 0,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}
