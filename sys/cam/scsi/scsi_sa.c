/*-
 * Implementation of SCSI Sequential Access Peripheral driver for CAM.
 *
 * Copyright (c) 1999, 2000 Matthew Jacob
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/queue.h>
#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/kernel.h>
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <sys/bio.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/mtio.h>
#ifdef _KERNEL
#include <sys/conf.h>
#endif
#include <sys/fcntl.h>
#include <sys/devicestat.h>

#ifndef _KERNEL
#include <stdio.h>
#include <string.h>
#endif

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/scsi_sa.h>

#ifdef _KERNEL

#include <opt_sa.h>

#ifndef SA_IO_TIMEOUT
#define SA_IO_TIMEOUT		4
#endif
#ifndef SA_SPACE_TIMEOUT
#define SA_SPACE_TIMEOUT	1 * 60
#endif
#ifndef SA_REWIND_TIMEOUT
#define SA_REWIND_TIMEOUT	2 * 60
#endif
#ifndef SA_ERASE_TIMEOUT
#define SA_ERASE_TIMEOUT	4 * 60
#endif

#define	SCSIOP_TIMEOUT		(60 * 1000)	/* not an option */

#define	IO_TIMEOUT		(SA_IO_TIMEOUT * 60 * 1000)
#define	REWIND_TIMEOUT		(SA_REWIND_TIMEOUT * 60 * 1000)
#define	ERASE_TIMEOUT		(SA_ERASE_TIMEOUT * 60 * 1000)
#define	SPACE_TIMEOUT		(SA_SPACE_TIMEOUT * 60 * 1000)

/*
 * Additional options that can be set for config: SA_1FM_AT_EOT
 */

#ifndef	UNUSED_PARAMETER
#define	UNUSED_PARAMETER(x)	x = x
#endif

#define	QFRLS(ccb)	\
	if (((ccb)->ccb_h.status & CAM_DEV_QFRZN) != 0)	\
		cam_release_devq((ccb)->ccb_h.path, 0, 0, 0, FALSE)

/*
 * Driver states
 */

MALLOC_DEFINE(M_SCSISA, "SCSI sa", "SCSI sequential access buffers");

typedef enum {
	SA_STATE_NORMAL, SA_STATE_ABNORMAL
} sa_state;

#define ccb_pflags	ppriv_field0
#define ccb_bp	 	ppriv_ptr1

#define	SA_CCB_BUFFER_IO	0x0
#define	SA_CCB_WAITING		0x1
#define	SA_CCB_TYPEMASK		0x1
#define	SA_POSITION_UPDATED	0x2

#define	Set_CCB_Type(x, type)				\
	x->ccb_h.ccb_pflags &= ~SA_CCB_TYPEMASK;	\
	x->ccb_h.ccb_pflags |= type

#define	CCB_Type(x)	(x->ccb_h.ccb_pflags & SA_CCB_TYPEMASK)



typedef enum {
	SA_FLAG_OPEN		= 0x0001,
	SA_FLAG_FIXED		= 0x0002,
	SA_FLAG_TAPE_LOCKED	= 0x0004,
	SA_FLAG_TAPE_MOUNTED	= 0x0008,
	SA_FLAG_TAPE_WP		= 0x0010,
	SA_FLAG_TAPE_WRITTEN	= 0x0020,
	SA_FLAG_EOM_PENDING	= 0x0040,
	SA_FLAG_EIO_PENDING	= 0x0080,
	SA_FLAG_EOF_PENDING	= 0x0100,
	SA_FLAG_ERR_PENDING	= (SA_FLAG_EOM_PENDING|SA_FLAG_EIO_PENDING|
				   SA_FLAG_EOF_PENDING),
	SA_FLAG_INVALID		= 0x0200,
	SA_FLAG_COMP_ENABLED	= 0x0400,
	SA_FLAG_COMP_SUPP	= 0x0800,
	SA_FLAG_COMP_UNSUPP	= 0x1000,
	SA_FLAG_TAPE_FROZEN	= 0x2000
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
	SA_QUIRK_NOCOMP		= 0x01,	/* Can't deal with compression at all */
	SA_QUIRK_FIXED		= 0x02,	/* Force fixed mode */
	SA_QUIRK_VARIABLE	= 0x04,	/* Force variable mode */
	SA_QUIRK_2FM		= 0x08,	/* Needs Two File Marks at EOD */
	SA_QUIRK_1FM		= 0x10,	/* No more than 1 File Mark at EOD */
	SA_QUIRK_NODREAD	= 0x20,	/* Don't try and dummy read density */
	SA_QUIRK_NO_MODESEL	= 0x40,	/* Don't do mode select at all */
	SA_QUIRK_NO_CPAGE	= 0x80	/* Don't use DEVICE COMPRESSION page */
} sa_quirks;

/* units are bits 4-7, 16-21 (1024 units) */
#define SAUNIT(DEV) \
	(((minor(DEV) & 0xF0) >> 4) |  ((minor(DEV) & 0x3f0000) >> 16))

#define SAMODE(z) ((minor(z) & 0x3))
#define SADENSITY(z) (((minor(z) >> 2) & 0x3))
#define	SA_IS_CTRL(z) (minor(z) & (1 << 29))

#define SA_NOT_CTLDEV	0
#define SA_CTLDEV	1

#define SA_ATYPE_R	0
#define SA_ATYPE_NR	1
#define SA_ATYPE_ER	2

#define SAMINOR(ctl, unit, mode, access) \
	((ctl << 29) | ((unit & 0x3f0) << 16) | ((unit & 0xf) << 4) | \
	(mode << 0x2) | (access & 0x3))

#define SA_NUM_MODES	4
struct sa_devs {
	struct cdev *ctl_dev;
	struct sa_mode_devs {
		struct cdev *r_dev;
		struct cdev *nr_dev;
		struct cdev *er_dev;
	} mode_devs[SA_NUM_MODES];
};

struct sa_softc {
	sa_state	state;
	sa_flags	flags;
	sa_quirks	quirks;
	struct		bio_queue_head bio_queue;
	int		queue_count;
	struct		devstat *device_stats;
	struct sa_devs	devs;
	int		blk_gran;
	int		blk_mask;
	int		blk_shift;
	u_int32_t	max_blk;
	u_int32_t	min_blk;
	u_int32_t	comp_algorithm;
	u_int32_t	saved_comp_algorithm;
	u_int32_t	media_blksize;
	u_int32_t	last_media_blksize;
	u_int32_t	media_numblks;
	u_int8_t	media_density;
	u_int8_t	speed;
	u_int8_t	scsi_rev;
	u_int8_t	dsreg;		/* mtio mt_dsreg, redux */
	int		buffer_mode;
	int		filemarks;
	union		ccb saved_ccb;
	int		last_resid_was_io;

	/*
	 * Relative to BOT Location.
	 */
	daddr_t		fileno;
	daddr_t		blkno;

	/*
	 * Latched Error Info
	 */
	struct {
		struct scsi_sense_data _last_io_sense;
		u_int32_t _last_io_resid;
		u_int8_t _last_io_cdb[CAM_MAX_CDBLEN];
		struct scsi_sense_data _last_ctl_sense;
		u_int32_t _last_ctl_resid;
		u_int8_t _last_ctl_cdb[CAM_MAX_CDBLEN];
#define	last_io_sense	errinfo._last_io_sense
#define	last_io_resid	errinfo._last_io_resid
#define	last_io_cdb	errinfo._last_io_cdb
#define	last_ctl_sense	errinfo._last_ctl_sense
#define	last_ctl_resid	errinfo._last_ctl_resid
#define	last_ctl_cdb	errinfo._last_ctl_cdb
	} errinfo;
	/*
	 * Misc other flags/state
	 */
	u_int32_t
					: 29,
		open_rdonly		: 1,	/* open read-only */
		open_pending_mount	: 1,	/* open pending mount */
		ctrl_mode		: 1;	/* control device open */
};

struct sa_quirk_entry {
	struct scsi_inquiry_pattern inq_pat;	/* matching pattern */
	sa_quirks quirks;	/* specific quirk type */
	u_int32_t prefblk;	/* preferred blocksize when in fixed mode */
};

static struct sa_quirk_entry sa_quirk_table[] =
{
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "OnStream",
		  "ADR*", "*"}, SA_QUIRK_FIXED|SA_QUIRK_NODREAD |
		   SA_QUIRK_1FM|SA_QUIRK_NO_MODESEL, 32768
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "ARCHIVE",
		  "Python 06408*", "*"}, SA_QUIRK_NODREAD, 0
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "ARCHIVE",
		  "Python 25601*", "*"}, SA_QUIRK_NOCOMP|SA_QUIRK_NODREAD, 0
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "ARCHIVE",
		  "Python*", "*"}, SA_QUIRK_NODREAD, 0
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "ARCHIVE",
		  "VIPER 150*", "*"}, SA_QUIRK_FIXED|SA_QUIRK_1FM, 512
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "ARCHIVE",
		  "VIPER 2525 25462", "-011"},
		  SA_QUIRK_NOCOMP|SA_QUIRK_1FM|SA_QUIRK_NODREAD, 0
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "ARCHIVE",
		  "VIPER 2525*", "*"}, SA_QUIRK_FIXED|SA_QUIRK_1FM, 1024
	},
#if	0
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "HP",
		  "C15*", "*"}, SA_QUIRK_VARIABLE|SA_QUIRK_NO_CPAGE, 0,
	},
#endif
 	{
 		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "HP",
		  "C56*", "*"}, SA_QUIRK_VARIABLE|SA_QUIRK_2FM, 0
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "HP",
		  "T20*", "*"}, SA_QUIRK_FIXED|SA_QUIRK_1FM, 512
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "HP",
		  "T4000*", "*"}, SA_QUIRK_FIXED|SA_QUIRK_1FM, 512
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "HP",
		  "HP-88780*", "*"}, SA_QUIRK_VARIABLE|SA_QUIRK_2FM, 0
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "KENNEDY",
		  "*", "*"}, SA_QUIRK_VARIABLE|SA_QUIRK_2FM, 0
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "M4 DATA",
		  "123107 SCSI*", "*"}, SA_QUIRK_VARIABLE|SA_QUIRK_2FM, 0
	},
	{	/* jreynold@primenet.com */
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "Seagate",
		"STT8000N*", "*"}, SA_QUIRK_1FM, 0
	},
	{	/* mike@sentex.net */
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "Seagate",
		"STT20000*", "*"}, SA_QUIRK_1FM, 0
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "TANDBERG",
		  " TDC 3600", "U07:"}, SA_QUIRK_NOCOMP|SA_QUIRK_1FM, 512
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "TANDBERG",
		  " TDC 3800", "*"}, SA_QUIRK_NOCOMP|SA_QUIRK_1FM, 512
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "TANDBERG",
		  " TDC 4100", "*"}, SA_QUIRK_NOCOMP|SA_QUIRK_1FM, 512
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "TANDBERG",
		  " TDC 4200", "*"}, SA_QUIRK_NOCOMP|SA_QUIRK_1FM, 512
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "TANDBERG",
		  " SLR*", "*"}, SA_QUIRK_1FM, 0
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "WANGTEK",
		  "5525ES*", "*"}, SA_QUIRK_FIXED|SA_QUIRK_1FM, 512
	},
	{
		{ T_SEQUENTIAL, SIP_MEDIA_REMOVABLE, "WANGTEK",
		  "51000*", "*"}, SA_QUIRK_FIXED|SA_QUIRK_1FM, 1024
	}
};

static	d_open_t	saopen;
static	d_close_t	saclose;
static	d_strategy_t	sastrategy;
static	d_ioctl_t	saioctl;
static	periph_init_t	sainit;
static	periph_ctor_t	saregister;
static	periph_oninv_t	saoninvalidate;
static	periph_dtor_t	sacleanup;
static	periph_start_t	sastart;
static	void		saasync(void *callback_arg, u_int32_t code,
				struct cam_path *path, void *arg);
static	void		sadone(struct cam_periph *periph,
			       union ccb *start_ccb);
static  int		saerror(union ccb *ccb, u_int32_t cam_flags,
				u_int32_t sense_flags);
static int		samarkswanted(struct cam_periph *);
static int		sacheckeod(struct cam_periph *periph);
static int		sagetparams(struct cam_periph *periph,
				    sa_params params_to_get,
				    u_int32_t *blocksize, u_int8_t *density,
				    u_int32_t *numblocks, int *buff_mode,
				    u_int8_t *write_protect, u_int8_t *speed,
				    int *comp_supported, int *comp_enabled,
				    u_int32_t *comp_algorithm,
				    sa_comp_t *comp_page);
static int		sasetparams(struct cam_periph *periph,
				    sa_params params_to_set,
				    u_int32_t blocksize, u_int8_t density,
				    u_int32_t comp_algorithm,
				    u_int32_t sense_flags);
static void		saprevent(struct cam_periph *periph, int action);
static int		sarewind(struct cam_periph *periph);
static int		saspace(struct cam_periph *periph, int count,
				scsi_space_code code);
static int		samount(struct cam_periph *, int, struct cdev *);
static int		saretension(struct cam_periph *periph);
static int		sareservereleaseunit(struct cam_periph *periph,
					     int reserve);
static int		saloadunload(struct cam_periph *periph, int load);
static int		saerase(struct cam_periph *periph, int longerase);
static int		sawritefilemarks(struct cam_periph *periph,
					 int nmarks, int setmarks);
static int		sardpos(struct cam_periph *periph, int, u_int32_t *);
static int		sasetpos(struct cam_periph *periph, int, u_int32_t *);


static struct periph_driver sadriver =
{
	sainit, "sa",
	TAILQ_HEAD_INITIALIZER(sadriver.units), /* generation */ 0
};

PERIPHDRIVER_DECLARE(sa, sadriver);

/* For 2.2-stable support */
#ifndef D_TAPE
#define D_TAPE 0
#endif


static struct cdevsw sa_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	saopen,
	.d_close =	saclose,
	.d_read =	physread,
	.d_write =	physwrite,
	.d_ioctl =	saioctl,
	.d_strategy =	sastrategy,
	.d_name =	"sa",
	.d_flags =	D_TAPE | D_NEEDGIANT,
};

static int
saopen(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct cam_periph *periph;
	struct sa_softc *softc;
	int unit;
	int error;

	unit = SAUNIT(dev);

	periph = (struct cam_periph *)dev->si_drv1;
	if (cam_periph_acquire(periph) != CAM_REQ_CMP) {
		return (ENXIO);
	}

	cam_periph_lock(periph);

	softc = (struct sa_softc *)periph->softc;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE|CAM_DEBUG_INFO,
	    ("saopen(%d): dev=0x%x softc=0x%x\n", unit, unit, softc->flags));

	if (SA_IS_CTRL(dev)) {
		softc->ctrl_mode = 1;
		cam_periph_unlock(periph);
		return (0);
	}

	if ((error = cam_periph_hold(periph, PRIBIO|PCATCH)) != 0) {
		cam_periph_unlock(periph);
		cam_periph_release(periph);
		return (error);
	}

	if (softc->flags & SA_FLAG_OPEN) {
		error = EBUSY;
	} else if (softc->flags & SA_FLAG_INVALID) {
		error = ENXIO;
	} else {
		/*
		 * Preserve whether this is a read_only open.
		 */
		softc->open_rdonly = (flags & O_RDWR) == O_RDONLY;

		/*
		 * The function samount ensures media is loaded and ready.
		 * It also does a device RESERVE if the tape isn't yet mounted.
		 *
		 * If the mount fails and this was a non-blocking open,
		 * make this a 'open_pending_mount' action.
		 */
		error = samount(periph, flags, dev);
		if (error && (flags & O_NONBLOCK)) {
			softc->flags |= SA_FLAG_OPEN;
			softc->open_pending_mount = 1;
			cam_periph_unhold(periph);
			cam_periph_unlock(periph);
			return (0);
		}
	}

	if (error) {
		cam_periph_unhold(periph);
		cam_periph_unlock(periph);
		cam_periph_release(periph);
		return (error);
	}

	saprevent(periph, PR_PREVENT);
	softc->flags |= SA_FLAG_OPEN;

	cam_periph_unhold(periph);
	cam_periph_unlock(periph);
	return (error);
}

static int
saclose(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	struct	cam_periph *periph;
	struct	sa_softc *softc;
	int	unit, mode, error, writing, tmp;
	int	closedbits = SA_FLAG_OPEN;

	unit = SAUNIT(dev);
	mode = SAMODE(dev);
	periph = (struct cam_periph *)dev->si_drv1;
	if (periph == NULL)
		return (ENXIO);	

	cam_periph_lock(periph);

	softc = (struct sa_softc *)periph->softc;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE|CAM_DEBUG_INFO,
	    ("saclose(%d): dev=0x%x softc=0x%x\n", unit, unit, softc->flags));


	softc->open_rdonly = 0; 
	if (SA_IS_CTRL(dev)) {
		softc->ctrl_mode = 0;
		cam_periph_unlock(periph);
		cam_periph_release(periph);
		return (0);
	}

	if (softc->open_pending_mount) {
		softc->flags &= ~SA_FLAG_OPEN;
		softc->open_pending_mount = 0; 
		cam_periph_unlock(periph);
		cam_periph_release(periph);
		return (0);
	}

	if ((error = cam_periph_hold(periph, PRIBIO)) != 0) {
		cam_periph_unlock(periph);
		return (error);
	}

	/*
	 * Were we writing the tape?
	 */
	writing = (softc->flags & SA_FLAG_TAPE_WRITTEN) != 0;

	/*
	 * See whether or not we need to write filemarks. If this
	 * fails, we probably have to assume we've lost tape
	 * position.
	 */
	error = sacheckeod(periph);
	if (error) {
		xpt_print(periph->path,
		    "failed to write terminating filemark(s)\n");
		softc->flags |= SA_FLAG_TAPE_FROZEN;
	}

	/*
	 * Whatever we end up doing, allow users to eject tapes from here on.
	 */
	saprevent(periph, PR_ALLOW);

	/*
	 * Decide how to end...
	 */
	if ((softc->flags & SA_FLAG_TAPE_MOUNTED) == 0) {
		closedbits |= SA_FLAG_TAPE_FROZEN;
	} else switch (mode) {
	case SA_MODE_OFFLINE:
		/*
		 * An 'offline' close is an unconditional release of
		 * frozen && mount conditions, irrespective of whether
		 * these operations succeeded. The reason for this is
		 * to allow at least some kind of programmatic way
		 * around our state getting all fouled up. If somebody
		 * issues an 'offline' command, that will be allowed
		 * to clear state.
		 */
		(void) sarewind(periph);
		(void) saloadunload(periph, FALSE);
		closedbits |= SA_FLAG_TAPE_MOUNTED|SA_FLAG_TAPE_FROZEN;
		break;
	case SA_MODE_REWIND:
		/*
		 * If the rewind fails, return an error- if anyone cares,
		 * but not overwriting any previous error.
		 *
		 * We don't clear the notion of mounted here, but we do
		 * clear the notion of frozen if we successfully rewound.
		 */
		tmp = sarewind(periph);
		if (tmp) {
			if (error != 0)
				error = tmp;
		} else {
			closedbits |= SA_FLAG_TAPE_FROZEN;
		}
		break;
	case SA_MODE_NOREWIND:
		/*
		 * If we're not rewinding/unloading the tape, find out
		 * whether we need to back up over one of two filemarks
		 * we wrote (if we wrote two filemarks) so that appends
		 * from this point on will be sane.
		 */
		if (error == 0 && writing && (softc->quirks & SA_QUIRK_2FM)) {
			tmp = saspace(periph, -1, SS_FILEMARKS);
			if (tmp) {
				xpt_print(periph->path, "unable to backspace "
				    "over one of double filemarks at end of "
				    "tape\n");
				xpt_print(periph->path, "it is possible that "
				    "this device needs a SA_QUIRK_1FM quirk set"
				    "for it\n");
				softc->flags |= SA_FLAG_TAPE_FROZEN;
			}
		}
		break;
	default:
		xpt_print(periph->path, "unknown mode 0x%x in saclose\n", mode);
		/* NOTREACHED */
		break;
	}

	/*
	 * We wish to note here that there are no more filemarks to be written.
	 */
	softc->filemarks = 0;
	softc->flags &= ~SA_FLAG_TAPE_WRITTEN;

	/*
	 * And we are no longer open for business.
	 */
	softc->flags &= ~closedbits;

	/*
	 * Inform users if tape state if frozen....
	 */
	if (softc->flags & SA_FLAG_TAPE_FROZEN) {
		xpt_print(periph->path, "tape is now frozen- use an OFFLINE, "
		    "REWIND or MTEOM command to clear this state.\n");
	}
	
	/* release the device if it is no longer mounted */
	if ((softc->flags & SA_FLAG_TAPE_MOUNTED) == 0)
		sareservereleaseunit(periph, FALSE);

	cam_periph_unhold(periph);
	cam_periph_unlock(periph);
	cam_periph_release(periph);

	return (error);	
}

/*
 * Actually translate the requested transfer into one the physical driver
 * can understand.  The transfer is described by a buf and will include
 * only one physical transfer.
 */
static void
sastrategy(struct bio *bp)
{
	struct cam_periph *periph;
	struct sa_softc *softc;
	
	bp->bio_resid = bp->bio_bcount;
	if (SA_IS_CTRL(bp->bio_dev)) {
		biofinish(bp, NULL, EINVAL);
		return;
	}
	periph = (struct cam_periph *)bp->bio_dev->si_drv1;
	if (periph == NULL) {
		biofinish(bp, NULL, ENXIO);
		return;
	}
	cam_periph_lock(periph);

	softc = (struct sa_softc *)periph->softc;

	if (softc->flags & SA_FLAG_INVALID) {
		cam_periph_unlock(periph);
		biofinish(bp, NULL, ENXIO);
		return;
	}

	if (softc->flags & SA_FLAG_TAPE_FROZEN) {
		cam_periph_unlock(periph);
		biofinish(bp, NULL, EPERM);
		return;
	}

	/*
	 * This should actually never occur as the write(2)
	 * system call traps attempts to write to a read-only
	 * file descriptor.
	 */
	if (bp->bio_cmd == BIO_WRITE && softc->open_rdonly) {
		cam_periph_unlock(periph);
		biofinish(bp, NULL, EBADF);
		return;
	}

	if (softc->open_pending_mount) {
		int error = samount(periph, 0, bp->bio_dev);
		if (error) {
			cam_periph_unlock(periph);
			biofinish(bp, NULL, ENXIO);
			return;
		}
		saprevent(periph, PR_PREVENT);
		softc->open_pending_mount = 0;
	}


	/*
	 * If it's a null transfer, return immediately
	 */
	if (bp->bio_bcount == 0) {
		cam_periph_unlock(periph);
		biodone(bp);
		return;
	}

	/* valid request?  */
	if (softc->flags & SA_FLAG_FIXED) {
		/*
		 * Fixed block device.  The byte count must
		 * be a multiple of our block size.
		 */
		if (((softc->blk_mask != ~0) &&
		    ((bp->bio_bcount & softc->blk_mask) != 0)) ||
		    ((softc->blk_mask == ~0) &&
		    ((bp->bio_bcount % softc->min_blk) != 0))) {
			xpt_print(periph->path, "Invalid request.  Fixed block "
			    "device requests must be a multiple of %d bytes\n",
			    softc->min_blk);
			cam_periph_unlock(periph);
			biofinish(bp, NULL, EINVAL);
			return;
		}
	} else if ((bp->bio_bcount > softc->max_blk) ||
		   (bp->bio_bcount < softc->min_blk) ||
		   (bp->bio_bcount & softc->blk_mask) != 0) {

		xpt_print_path(periph->path);
		printf("Invalid request.  Variable block "
		    "device requests must be ");
		if (softc->blk_mask != 0) {
			printf("a multiple of %d ", (0x1 << softc->blk_gran));
		}
		printf("between %d and %d bytes\n", softc->min_blk,
		    softc->max_blk);
		cam_periph_unlock(periph);
		biofinish(bp, NULL, EINVAL);
		return;
        }
	
	/*
	 * Place it at the end of the queue.
	 */
	bioq_insert_tail(&softc->bio_queue, bp);
	softc->queue_count++;
#if	0
	CAM_DEBUG(periph->path, CAM_DEBUG_INFO,
	    ("sastrategy: queuing a %ld %s byte %s\n", bp->bio_bcount,
 	    (softc->flags & SA_FLAG_FIXED)?  "fixed" : "variable",
	    (bp->bio_cmd == BIO_READ)? "read" : "write"));
#endif
	if (softc->queue_count > 1) {
		CAM_DEBUG(periph->path, CAM_DEBUG_INFO,
		    ("sastrategy: queue count now %d\n", softc->queue_count));
	}
	
	/*
	 * Schedule ourselves for performing the work.
	 */
	xpt_schedule(periph, 1);
	cam_periph_unlock(periph);

	return;
}


#define	PENDING_MOUNT_CHECK(softc, periph, dev)		\
	if (softc->open_pending_mount) {		\
		error = samount(periph, 0, dev);	\
		if (error) {				\
			break;				\
		}					\
		saprevent(periph, PR_PREVENT);		\
		softc->open_pending_mount = 0;		\
	}

static int
saioctl(struct cdev *dev, u_long cmd, caddr_t arg, int flag, struct thread *td)
{
	struct cam_periph *periph;
	struct sa_softc *softc;
	scsi_space_code spaceop;
	int didlockperiph = 0;
	int mode;
	int error = 0;

	mode = SAMODE(dev);
	error = 0;		/* shut up gcc */
	spaceop = 0;		/* shut up gcc */

	periph = (struct cam_periph *)dev->si_drv1;
	if (periph == NULL)
		return (ENXIO);	

	cam_periph_lock(periph);
	softc = (struct sa_softc *)periph->softc;

	/*
	 * Check for control mode accesses. We allow MTIOCGET and
	 * MTIOCERRSTAT (but need to be the only one open in order
	 * to clear latched status), and MTSETBSIZE, MTSETDNSTY
	 * and MTCOMP (but need to be the only one accessing this
	 * device to run those).
	 */

	if (SA_IS_CTRL(dev)) {
		switch (cmd) {
		case MTIOCGETEOTMODEL:
		case MTIOCGET:
			break;
		case MTIOCERRSTAT:
			/*
			 * If the periph isn't already locked, lock it
			 * so our MTIOCERRSTAT can reset latched error stats.
			 *
			 * If the periph is already locked, skip it because
			 * we're just getting status and it'll be up to the
			 * other thread that has this device open to do
			 * an MTIOCERRSTAT that would clear latched status.
			 */
			if ((periph->flags & CAM_PERIPH_LOCKED) == 0) {
				error = cam_periph_hold(periph, PRIBIO|PCATCH);
				 if (error != 0)
					return (error);
				didlockperiph = 1;
			}
			break;

		case MTIOCTOP:
		{
			struct mtop *mt = (struct mtop *) arg;

			/*
			 * Check to make sure it's an OP we can perform
			 * with no media inserted.
			 */
			switch (mt->mt_op) {
			case MTSETBSIZ:
			case MTSETDNSTY:
			case MTCOMP:
				mt = NULL;
				/* FALLTHROUGH */
			default:
				break;
			}
			if (mt != NULL) {
				break;
			}
			/* FALLTHROUGH */
		}
		case MTIOCSETEOTMODEL:
			/*
			 * We need to acquire the peripheral here rather
			 * than at open time because we are sharing writable
			 * access to data structures.
			 */
			error = cam_periph_hold(periph, PRIBIO|PCATCH);
			if (error != 0)
				return (error);
			didlockperiph = 1;
			break;

		default:
			return (EINVAL);
		}
	}

	/*
	 * Find the device that the user is talking about
	 */
	switch (cmd) {
	case MTIOCGET:
	{
		struct mtget *g = (struct mtget *)arg;

		/*
		 * If this isn't the control mode device, actually go out
		 * and ask the drive again what it's set to.
		 */
		if (!SA_IS_CTRL(dev) && !softc->open_pending_mount) {
			u_int8_t write_protect;
			int comp_enabled, comp_supported;
			error = sagetparams(periph, SA_PARAM_ALL,
			    &softc->media_blksize, &softc->media_density,
			    &softc->media_numblks, &softc->buffer_mode,
			    &write_protect, &softc->speed, &comp_supported,
			    &comp_enabled, &softc->comp_algorithm, NULL);
			if (error)
				break;
			if (write_protect)
				softc->flags |= SA_FLAG_TAPE_WP;
			else
				softc->flags &= ~SA_FLAG_TAPE_WP;
			softc->flags &= ~(SA_FLAG_COMP_SUPP|
			    SA_FLAG_COMP_ENABLED|SA_FLAG_COMP_UNSUPP);
			if (comp_supported) {
				if (softc->saved_comp_algorithm == 0)
					softc->saved_comp_algorithm =
					    softc->comp_algorithm;
				softc->flags |= SA_FLAG_COMP_SUPP;
				if (comp_enabled)
					softc->flags |= SA_FLAG_COMP_ENABLED;
			} else  
				softc->flags |= SA_FLAG_COMP_UNSUPP;
		}
		bzero(g, sizeof(struct mtget));
		g->mt_type = MT_ISAR;
		if (softc->flags & SA_FLAG_COMP_UNSUPP) {
			g->mt_comp = MT_COMP_UNSUPP;
			g->mt_comp0 = MT_COMP_UNSUPP;
			g->mt_comp1 = MT_COMP_UNSUPP;
			g->mt_comp2 = MT_COMP_UNSUPP;
			g->mt_comp3 = MT_COMP_UNSUPP;
		} else {
			if ((softc->flags & SA_FLAG_COMP_ENABLED) == 0) {
				g->mt_comp = MT_COMP_DISABLED;
			} else {
				g->mt_comp = softc->comp_algorithm;
			}
			g->mt_comp0 = softc->comp_algorithm;
			g->mt_comp1 = softc->comp_algorithm;
			g->mt_comp2 = softc->comp_algorithm;
			g->mt_comp3 = softc->comp_algorithm;
		}
		g->mt_density = softc->media_density;
		g->mt_density0 = softc->media_density;
		g->mt_density1 = softc->media_density;
		g->mt_density2 = softc->media_density;
		g->mt_density3 = softc->media_density;
		g->mt_blksiz = softc->media_blksize;
		g->mt_blksiz0 = softc->media_blksize;
		g->mt_blksiz1 = softc->media_blksize;
		g->mt_blksiz2 = softc->media_blksize;
		g->mt_blksiz3 = softc->media_blksize;
		g->mt_fileno = softc->fileno;
		g->mt_blkno = softc->blkno;
		g->mt_dsreg = (short) softc->dsreg;
		/*
		 * Yes, we know that this is likely to overflow
		 */
		if (softc->last_resid_was_io) {
			if ((g->mt_resid = (short) softc->last_io_resid) != 0) {
				if (SA_IS_CTRL(dev) == 0 || didlockperiph) {
					softc->last_io_resid = 0;
				}
			}
		} else {
			if ((g->mt_resid = (short)softc->last_ctl_resid) != 0) {
				if (SA_IS_CTRL(dev) == 0 || didlockperiph) {
					softc->last_ctl_resid = 0;
				}
			}
		}
		error = 0;
		break;
	}
	case MTIOCERRSTAT:
	{
		struct scsi_tape_errors *sep =
		    &((union mterrstat *)arg)->scsi_errstat;

		CAM_DEBUG(periph->path, CAM_DEBUG_TRACE,
		    ("saioctl: MTIOCERRSTAT\n"));

		bzero(sep, sizeof(*sep));
		sep->io_resid = softc->last_io_resid;
		bcopy((caddr_t) &softc->last_io_sense, sep->io_sense,
		    sizeof (sep->io_sense));
		bcopy((caddr_t) &softc->last_io_cdb, sep->io_cdb,
		    sizeof (sep->io_cdb));
		sep->ctl_resid = softc->last_ctl_resid;
		bcopy((caddr_t) &softc->last_ctl_sense, sep->ctl_sense,
		    sizeof (sep->ctl_sense));
		bcopy((caddr_t) &softc->last_ctl_cdb, sep->ctl_cdb,
		    sizeof (sep->ctl_cdb));

		if ((SA_IS_CTRL(dev) == 0 && softc->open_pending_mount) ||
		    didlockperiph)
			bzero((caddr_t) &softc->errinfo,
			    sizeof (softc->errinfo));
		error = 0;
		break;
	}
	case MTIOCTOP:
	{
		struct mtop *mt;
		int    count;

		PENDING_MOUNT_CHECK(softc, periph, dev);

		mt = (struct mtop *)arg;


		CAM_DEBUG(periph->path, CAM_DEBUG_TRACE,
			 ("saioctl: op=0x%x count=0x%x\n",
			  mt->mt_op, mt->mt_count));

		count = mt->mt_count;
		switch (mt->mt_op) {
		case MTWEOF:	/* write an end-of-file marker */
			/*
			 * We don't need to clear the SA_FLAG_TAPE_WRITTEN
			 * flag because by keeping track of filemarks
			 * we have last written we know ehether or not
			 * we need to write more when we close the device.
			 */
			error = sawritefilemarks(periph, count, FALSE);
			break;
		case MTWSS:	/* write a setmark */
			error = sawritefilemarks(periph, count, TRUE);
			break;
		case MTBSR:	/* backward space record */
		case MTFSR:	/* forward space record */
		case MTBSF:	/* backward space file */
		case MTFSF:	/* forward space file */
		case MTBSS:	/* backward space setmark */
		case MTFSS:	/* forward space setmark */
		case MTEOD:	/* space to end of recorded medium */
		{
			int nmarks;

			spaceop = SS_FILEMARKS;
			nmarks = softc->filemarks;
			error = sacheckeod(periph);
			if (error) {
				xpt_print(periph->path,
				    "EOD check prior to spacing failed\n");
				softc->flags |= SA_FLAG_EIO_PENDING;
				break;
			}
			nmarks -= softc->filemarks;
			switch(mt->mt_op) {
			case MTBSR:
				count = -count;
				/* FALLTHROUGH */
			case MTFSR:
				spaceop = SS_BLOCKS;
				break;
			case MTBSF:
				count = -count;
				/* FALLTHROUGH */
			case MTFSF:
				break;
			case MTBSS:
				count = -count;
				/* FALLTHROUGH */
			case MTFSS:
				spaceop = SS_SETMARKS;
				break;
			case MTEOD:
				spaceop = SS_EOD;
				count = 0;
				nmarks = 0;
				break;
			default:
				error = EINVAL;
				break;
			}
			if (error)
				break;

			nmarks = softc->filemarks;
			/*
			 * XXX: Why are we checking again?
			 */
			error = sacheckeod(periph);
			if (error)
				break;
			nmarks -= softc->filemarks;
			error = saspace(periph, count - nmarks, spaceop);
			/*
			 * At this point, clear that we've written the tape
			 * and that we've written any filemarks. We really
			 * don't know what the applications wishes to do next-
			 * the sacheckeod's will make sure we terminated the
			 * tape correctly if we'd been writing, but the next
			 * action the user application takes will set again
			 * whether we need to write filemarks.
			 */
			softc->flags &=
			    ~(SA_FLAG_TAPE_WRITTEN|SA_FLAG_TAPE_FROZEN);
			softc->filemarks = 0;
			break;
		}
		case MTREW:	/* rewind */
			PENDING_MOUNT_CHECK(softc, periph, dev);
			(void) sacheckeod(periph);
			error = sarewind(periph);
			/* see above */
			softc->flags &=
			    ~(SA_FLAG_TAPE_WRITTEN|SA_FLAG_TAPE_FROZEN);
			softc->flags &= ~SA_FLAG_ERR_PENDING;
			softc->filemarks = 0;
			break;
		case MTERASE:	/* erase */
			PENDING_MOUNT_CHECK(softc, periph, dev);
			error = saerase(periph, count);
			softc->flags &=
			    ~(SA_FLAG_TAPE_WRITTEN|SA_FLAG_TAPE_FROZEN);
			softc->flags &= ~SA_FLAG_ERR_PENDING;
			break;
		case MTRETENS:	/* re-tension tape */
			PENDING_MOUNT_CHECK(softc, periph, dev);
			error = saretension(periph);		
			softc->flags &=
			    ~(SA_FLAG_TAPE_WRITTEN|SA_FLAG_TAPE_FROZEN);
			softc->flags &= ~SA_FLAG_ERR_PENDING;
			break;
		case MTOFFL:	/* rewind and put the drive offline */

			PENDING_MOUNT_CHECK(softc, periph, dev);

			(void) sacheckeod(periph);
			/* see above */
			softc->flags &= ~SA_FLAG_TAPE_WRITTEN;
			softc->filemarks = 0;

			error = sarewind(periph);
			/* clear the frozen flag anyway */
			softc->flags &= ~SA_FLAG_TAPE_FROZEN;

			/*
			 * Be sure to allow media removal before ejecting.
			 */

			saprevent(periph, PR_ALLOW);
			if (error == 0) {
				error = saloadunload(periph, FALSE);
				if (error == 0) {
					softc->flags &= ~SA_FLAG_TAPE_MOUNTED;
				}
			}
			break;

		case MTNOP:	/* no operation, sets status only */
		case MTCACHE:	/* enable controller cache */
		case MTNOCACHE:	/* disable controller cache */
			error = 0;
			break;

		case MTSETBSIZ:	/* Set block size for device */

			PENDING_MOUNT_CHECK(softc, periph, dev);

			error = sasetparams(periph, SA_PARAM_BLOCKSIZE, count,
					    0, 0, 0);
			if (error == 0) {
				softc->last_media_blksize =
				    softc->media_blksize;
				softc->media_blksize = count;
				if (count) {
					softc->flags |= SA_FLAG_FIXED;
					if (powerof2(count)) {
						softc->blk_shift =
						    ffs(count) - 1;
						softc->blk_mask = count - 1;
					} else {
						softc->blk_mask = ~0;
						softc->blk_shift = 0;
					}
					/*
					 * Make the user's desire 'persistent'.
					 */
					softc->quirks &= ~SA_QUIRK_VARIABLE;
					softc->quirks |= SA_QUIRK_FIXED;
				} else {
					softc->flags &= ~SA_FLAG_FIXED;
					if (softc->max_blk == 0) {
						softc->max_blk = ~0;
					}
					softc->blk_shift = 0;
					if (softc->blk_gran != 0) {
						softc->blk_mask =
						    softc->blk_gran - 1;
					} else {
						softc->blk_mask = 0;
					}
					/*
					 * Make the user's desire 'persistent'.
					 */
					softc->quirks |= SA_QUIRK_VARIABLE;
					softc->quirks &= ~SA_QUIRK_FIXED;
				}
			}
			break;
		case MTSETDNSTY:	/* Set density for device and mode */
			PENDING_MOUNT_CHECK(softc, periph, dev);

			if (count > UCHAR_MAX) {
				error = EINVAL;	
				break;
			} else {
				error = sasetparams(periph, SA_PARAM_DENSITY,
						    0, count, 0, 0);
			}
			break;
		case MTCOMP:	/* enable compression */
			PENDING_MOUNT_CHECK(softc, periph, dev);
			/*
			 * Some devices don't support compression, and
			 * don't like it if you ask them for the
			 * compression page.
			 */
			if ((softc->quirks & SA_QUIRK_NOCOMP) ||
			    (softc->flags & SA_FLAG_COMP_UNSUPP)) {
				error = ENODEV;
				break;
			}
			error = sasetparams(periph, SA_PARAM_COMPRESSION,
			    0, 0, count, SF_NO_PRINT);
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
	case MTIOCRDSPOS:
		PENDING_MOUNT_CHECK(softc, periph, dev);
		error = sardpos(periph, 0, (u_int32_t *) arg);
		break;
	case MTIOCRDHPOS:
		PENDING_MOUNT_CHECK(softc, periph, dev);
		error = sardpos(periph, 1, (u_int32_t *) arg);
		break;
	case MTIOCSLOCATE:
		PENDING_MOUNT_CHECK(softc, periph, dev);
		error = sasetpos(periph, 0, (u_int32_t *) arg);
		break;
	case MTIOCHLOCATE:
		PENDING_MOUNT_CHECK(softc, periph, dev);
		error = sasetpos(periph, 1, (u_int32_t *) arg);
		break;
	case MTIOCGETEOTMODEL:
		error = 0;
		if (softc->quirks & SA_QUIRK_1FM)
			mode = 1;
		else
			mode = 2;
		*((u_int32_t *) arg) = mode;
		break;
	case MTIOCSETEOTMODEL:
		error = 0;
		switch (*((u_int32_t *) arg)) {
		case 1:
			softc->quirks &= ~SA_QUIRK_2FM;
			softc->quirks |= SA_QUIRK_1FM;
			break;
		case 2:
			softc->quirks &= ~SA_QUIRK_1FM;
			softc->quirks |= SA_QUIRK_2FM;
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	default:
		error = cam_periph_ioctl(periph, cmd, arg, saerror);
		break;
	}

	/*
	 * Check to see if we cleared a frozen state
	 */
	if (error == 0 && (softc->flags & SA_FLAG_TAPE_FROZEN)) {
		switch(cmd) {
		case MTIOCRDSPOS:
		case MTIOCRDHPOS:
		case MTIOCSLOCATE:
		case MTIOCHLOCATE:
			softc->fileno = (daddr_t) -1;
			softc->blkno = (daddr_t) -1;
			softc->flags &= ~SA_FLAG_TAPE_FROZEN;
			xpt_print(periph->path,
			    "tape state now unfrozen.\n");
			break;
		default:
			break;
		}
	}
	if (didlockperiph) {
		cam_periph_unhold(periph);
	}
	cam_periph_unlock(periph);
	return (error);
}

static void
sainit(void)
{
	cam_status status;

	/*
	 * Install a global async callback.
	 */
	status = xpt_register_async(AC_FOUND_DEVICE, saasync, NULL, NULL);

	if (status != CAM_REQ_CMP) {
		printf("sa: Failed to attach master async callback "
		       "due to status 0x%x!\n", status);
	}
}

static void
saoninvalidate(struct cam_periph *periph)
{
	struct sa_softc *softc;

	softc = (struct sa_softc *)periph->softc;

	/*
	 * De-register any async callbacks.
	 */
	xpt_register_async(0, saasync, periph, periph->path);

	softc->flags |= SA_FLAG_INVALID;

	/*
	 * Return all queued I/O with ENXIO.
	 * XXX Handle any transactions queued to the card
	 *     with XPT_ABORT_CCB.
	 */
	bioq_flush(&softc->bio_queue, NULL, ENXIO);
	softc->queue_count = 0;

	xpt_print(periph->path, "lost device\n");

}

static void
sacleanup(struct cam_periph *periph)
{
	struct sa_softc *softc;
	int i;

	softc = (struct sa_softc *)periph->softc;

	devstat_remove_entry(softc->device_stats);

	destroy_dev(softc->devs.ctl_dev);

	for (i = 0; i < SA_NUM_MODES; i++) {
		destroy_dev(softc->devs.mode_devs[i].r_dev);
		destroy_dev(softc->devs.mode_devs[i].nr_dev);
		destroy_dev(softc->devs.mode_devs[i].er_dev);
	}

	xpt_print(periph->path, "removing device entry\n");
	free(softc, M_SCSISA);
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
		if (cgd == NULL)
			break;

		if (SID_TYPE(&cgd->inq_data) != T_SEQUENTIAL)
			break;

		/*
		 * Allocate a peripheral instance for
		 * this device and start the probe
		 * process.
		 */
		status = cam_periph_alloc(saregister, saoninvalidate,
					  sacleanup, sastart,
					  "sa", CAM_PERIPH_BIO, cgd->ccb_h.path,
					  saasync, AC_FOUND_DEVICE, cgd);

		if (status != CAM_REQ_CMP
		 && status != CAM_REQ_INPROG)
			printf("saasync: Unable to probe new device "
				"due to status 0x%x\n", status);
		break;
	}
	default:
		cam_periph_async(periph, code, path, arg);
		break;
	}
}

static cam_status
saregister(struct cam_periph *periph, void *arg)
{
	struct sa_softc *softc;
	struct ccb_getdev *cgd;
	caddr_t match;
	int i;
	
	cgd = (struct ccb_getdev *)arg;
	if (periph == NULL) {
		printf("saregister: periph was NULL!!\n");
		return (CAM_REQ_CMP_ERR);
	}

	if (cgd == NULL) {
		printf("saregister: no getdev CCB, can't register device\n");
		return (CAM_REQ_CMP_ERR);
	}

	softc = (struct sa_softc *)
	    malloc(sizeof (*softc), M_SCSISA, M_NOWAIT | M_ZERO);
	if (softc == NULL) {
		printf("saregister: Unable to probe new device. "
		       "Unable to allocate softc\n");				
		return (CAM_REQ_CMP_ERR);
	}
	softc->scsi_rev = SID_ANSI_REV(&cgd->inq_data);
	softc->state = SA_STATE_NORMAL;
	softc->fileno = (daddr_t) -1;
	softc->blkno = (daddr_t) -1;

	bioq_init(&softc->bio_queue);
	periph->softc = softc;

	/*
	 * See if this device has any quirks.
	 */
	match = cam_quirkmatch((caddr_t)&cgd->inq_data,
			       (caddr_t)sa_quirk_table,
			       sizeof(sa_quirk_table)/sizeof(*sa_quirk_table),
			       sizeof(*sa_quirk_table), scsi_inquiry_match);

	if (match != NULL) {
		softc->quirks = ((struct sa_quirk_entry *)match)->quirks;
		softc->last_media_blksize =
		    ((struct sa_quirk_entry *)match)->prefblk;
#ifdef	CAMDEBUG
		xpt_print(periph->path, "found quirk entry %d\n",
		    (int) (((struct sa_quirk_entry *) match) - sa_quirk_table));
#endif
	} else
		softc->quirks = SA_QUIRK_NONE;

	/*
 	 * The SA driver supports a blocksize, but we don't know the
	 * blocksize until we media is inserted.  So, set a flag to
	 * indicate that the blocksize is unavailable right now.
	 */
	cam_periph_unlock(periph);
	softc->device_stats = devstat_new_entry("sa", periph->unit_number, 0,
	    DEVSTAT_BS_UNAVAILABLE, SID_TYPE(&cgd->inq_data) |
	    DEVSTAT_TYPE_IF_SCSI, DEVSTAT_PRIORITY_TAPE);

	softc->devs.ctl_dev = make_dev(&sa_cdevsw, SAMINOR(SA_CTLDEV,
	    periph->unit_number, 0, SA_ATYPE_R), UID_ROOT, GID_OPERATOR,
	    0660, "%s%d.ctl", periph->periph_name, periph->unit_number);
	softc->devs.ctl_dev->si_drv1 = periph;

	for (i = 0; i < SA_NUM_MODES; i++) {

		softc->devs.mode_devs[i].r_dev = make_dev(&sa_cdevsw,
		    SAMINOR(SA_NOT_CTLDEV, periph->unit_number, i, SA_ATYPE_R),
		    UID_ROOT, GID_OPERATOR, 0660, "%s%d.%d",
		    periph->periph_name, periph->unit_number, i);
		softc->devs.mode_devs[i].r_dev->si_drv1 = periph;

		softc->devs.mode_devs[i].nr_dev = make_dev(&sa_cdevsw,
		    SAMINOR(SA_NOT_CTLDEV, periph->unit_number, i, SA_ATYPE_NR),
		    UID_ROOT, GID_OPERATOR, 0660, "n%s%d.%d",
		    periph->periph_name, periph->unit_number, i);
		softc->devs.mode_devs[i].nr_dev->si_drv1 = periph;

		softc->devs.mode_devs[i].er_dev = make_dev(&sa_cdevsw,
		    SAMINOR(SA_NOT_CTLDEV, periph->unit_number, i, SA_ATYPE_ER),
		    UID_ROOT, GID_OPERATOR, 0660, "e%s%d.%d",
		    periph->periph_name, periph->unit_number, i);
		softc->devs.mode_devs[i].er_dev->si_drv1 = periph;

		/*
		 * Make the (well known) aliases for the first mode.
		 */
		if (i == 0) {
			struct cdev *alias;

			alias = make_dev_alias(softc->devs.mode_devs[i].r_dev,
			   "%s%d", periph->periph_name, periph->unit_number);
			alias->si_drv1 = periph;
			alias = make_dev_alias(softc->devs.mode_devs[i].nr_dev,
			    "n%s%d", periph->periph_name, periph->unit_number);
			alias->si_drv1 = periph;
			alias = make_dev_alias(softc->devs.mode_devs[i].er_dev,
			    "e%s%d", periph->periph_name, periph->unit_number);
			alias->si_drv1 = periph;
		}
	}
	cam_periph_lock(periph);

	/*
	 * Add an async callback so that we get
	 * notified if this device goes away.
	 */
	xpt_register_async(AC_LOST_DEVICE, saasync, periph, periph->path);

	xpt_announce_periph(periph, NULL);

	return (CAM_REQ_CMP);
}

static void
sastart(struct cam_periph *periph, union ccb *start_ccb)
{
	struct sa_softc *softc;

	softc = (struct sa_softc *)periph->softc;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("sastart\n"));

	
	switch (softc->state) {
	case SA_STATE_NORMAL:
	{
		/* Pull a buffer from the queue and get going on it */		
		struct bio *bp;

		/*
		 * See if there is a buf with work for us to do..
		 */
		bp = bioq_first(&softc->bio_queue);
		if (periph->immediate_priority <= periph->pinfo.priority) {
			CAM_DEBUG_PRINT(CAM_DEBUG_SUBTRACE,
					("queuing for immediate ccb\n"));
			Set_CCB_Type(start_ccb, SA_CCB_WAITING);
			SLIST_INSERT_HEAD(&periph->ccb_list, &start_ccb->ccb_h,
					  periph_links.sle);
			periph->immediate_priority = CAM_PRIORITY_NONE;
			wakeup(&periph->ccb_list);
		} else if (bp == NULL) {
			xpt_release_ccb(start_ccb);
		} else if ((softc->flags & SA_FLAG_ERR_PENDING) != 0) {
			struct bio *done_bp;
again:
			softc->queue_count--;
			bioq_remove(&softc->bio_queue, bp);
			bp->bio_resid = bp->bio_bcount;
			done_bp = bp;
			if ((softc->flags & SA_FLAG_EOM_PENDING) != 0) {
				/*
				 * We now just clear errors in this case
				 * and let the residual be the notifier.
				 */
				bp->bio_error = 0;
			} else if ((softc->flags & SA_FLAG_EOF_PENDING) != 0) {
				/*
				 * This can only happen if we're reading
				 * in fixed length mode. In this case,
				 * we dump the rest of the list the
				 * same way.
				 */
				bp->bio_error = 0;
				if (bioq_first(&softc->bio_queue) != NULL) {
					biodone(done_bp);
					goto again;
				}
			} else if ((softc->flags & SA_FLAG_EIO_PENDING) != 0) {
				bp->bio_error = EIO;
				bp->bio_flags |= BIO_ERROR;
			}
			bp = bioq_first(&softc->bio_queue);
			/*
			 * Only if we have no other buffers queued up
			 * do we clear the pending error flag.
			 */
			if (bp == NULL)
				softc->flags &= ~SA_FLAG_ERR_PENDING;
			CAM_DEBUG(periph->path, CAM_DEBUG_INFO,
			    ("sastart- ERR_PENDING now 0x%x, bp is %sNULL, "
			    "%d more buffers queued up\n",
			    (softc->flags & SA_FLAG_ERR_PENDING),
			    (bp != NULL)? "not " : " ", softc->queue_count));
			xpt_release_ccb(start_ccb);
			biodone(done_bp);
		} else {
			u_int32_t length;

			bioq_remove(&softc->bio_queue, bp);
			softc->queue_count--;

			if ((softc->flags & SA_FLAG_FIXED) != 0) {
				if (softc->blk_shift != 0) {
					length =
					    bp->bio_bcount >> softc->blk_shift;
				} else if (softc->media_blksize != 0) {
					length = bp->bio_bcount /
					    softc->media_blksize;
				} else {
					bp->bio_error = EIO;
					xpt_print(periph->path, "zero blocksize"
					    " for FIXED length writes?\n");
					biodone(bp);
					break;
				}
#if	0
				CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_INFO,
				    ("issuing a %d fixed record %s\n",
				    length,  (bp->bio_cmd == BIO_READ)? "read" :
				    "write"));
#endif
			} else {
				length = bp->bio_bcount;
#if	0
				CAM_DEBUG(start_ccb->ccb_h.path, CAM_DEBUG_INFO,
				    ("issuing a %d variable byte %s\n",
				    length,  (bp->bio_cmd == BIO_READ)? "read" :
				    "write"));
#endif
			}
			devstat_start_transaction_bio(softc->device_stats, bp);
			/*
			 * Some people have theorized that we should
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
			 *
			 * I believe that this is a non-issue. If user apps
			 * don't adjust their read size to match our record
			 * size, that's just life. Anyway, the typical usage
			 * would be to issue, e.g., 64KB reads and occasionally
			 * have to do deal with 512 byte or 1KB intermediate
			 * records.
			 */
			softc->dsreg = (bp->bio_cmd == BIO_READ)?
			    MTIO_DSREG_RD : MTIO_DSREG_WR;
			scsi_sa_read_write(&start_ccb->csio, 0, sadone,
			    MSG_SIMPLE_Q_TAG, (bp->bio_cmd == BIO_READ),
			    FALSE, (softc->flags & SA_FLAG_FIXED) != 0,
			    length, bp->bio_data, bp->bio_bcount, SSD_FULL_SIZE,
			    IO_TIMEOUT);
			start_ccb->ccb_h.ccb_pflags &= ~SA_POSITION_UPDATED;
			Set_CCB_Type(start_ccb, SA_CCB_BUFFER_IO);
			start_ccb->ccb_h.ccb_bp = bp;
			bp = bioq_first(&softc->bio_queue);
			xpt_action(start_ccb);
		}
		
		if (bp != NULL) {
			/* Have more work to do, so ensure we stay scheduled */
			xpt_schedule(periph, 1);
		}
		break;
	}
	case SA_STATE_ABNORMAL:
	default:
		panic("state 0x%x in sastart", softc->state);
		break;
	}
}


static void
sadone(struct cam_periph *periph, union ccb *done_ccb)
{
	struct sa_softc *softc;
	struct ccb_scsiio *csio;

	softc = (struct sa_softc *)periph->softc;
	csio = &done_ccb->csio;
	switch (CCB_Type(csio)) {
	case SA_CCB_BUFFER_IO:
	{
		struct bio *bp;
		int error;

		softc->dsreg = MTIO_DSREG_REST;
		bp = (struct bio *)done_ccb->ccb_h.ccb_bp;
		error = 0;
		if ((done_ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			if ((error = saerror(done_ccb, 0, 0)) == ERESTART) {
				/*
				 * A retry was scheduled, so just return.
				 */
				return;
			}
		}

		if (error == EIO) {

			/*
			 * Catastrophic error. Mark the tape as frozen
			 * (we no longer know tape position).
			 *
			 * Return all queued I/O with EIO, and unfreeze
			 * our queue so that future transactions that
			 * attempt to fix this problem can get to the
			 * device.
			 *
			 */

			softc->flags |= SA_FLAG_TAPE_FROZEN;
			bioq_flush(&softc->bio_queue, NULL, EIO);
		}
		if (error != 0) {
			bp->bio_resid = bp->bio_bcount;
			bp->bio_error = error;
			bp->bio_flags |= BIO_ERROR;
			/*
			 * In the error case, position is updated in saerror.
			 */
		} else {
			bp->bio_resid = csio->resid;
			bp->bio_error = 0;
			if (csio->resid != 0) {
				bp->bio_flags |= BIO_ERROR;
			}
			if (bp->bio_cmd == BIO_WRITE) {
				softc->flags |= SA_FLAG_TAPE_WRITTEN;
				softc->filemarks = 0;
			}
			if (!(csio->ccb_h.ccb_pflags & SA_POSITION_UPDATED) &&
			    (softc->blkno != (daddr_t) -1)) {
				if ((softc->flags & SA_FLAG_FIXED) != 0) {
					u_int32_t l;
					if (softc->blk_shift != 0) {
						l = bp->bio_bcount >>
							softc->blk_shift;
					} else {
						l = bp->bio_bcount /
							softc->media_blksize;
					}
					softc->blkno += (daddr_t) l;
				} else {
					softc->blkno++;
				}
			}
		}
		/*
		 * If we had an error (immediate or pending),
		 * release the device queue now.
		 */
		if (error || (softc->flags & SA_FLAG_ERR_PENDING))
			cam_release_devq(done_ccb->ccb_h.path, 0, 0, 0, 0);
#ifdef	CAMDEBUG
		if (error || bp->bio_resid) {
			CAM_DEBUG(periph->path, CAM_DEBUG_INFO,
			    	  ("error %d resid %ld count %ld\n", error,
				  bp->bio_resid, bp->bio_bcount));
		}
#endif
		biofinish(bp, softc->device_stats, 0);
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

/*
 * Mount the tape (make sure it's ready for I/O).
 */
static int
samount(struct cam_periph *periph, int oflags, struct cdev *dev)
{
	struct	sa_softc *softc;
	union	ccb *ccb;
	int	error;

	/*
	 * oflags can be checked for 'kind' of open (read-only check) - later
	 * dev can be checked for a control-mode or compression open - later
	 */
	UNUSED_PARAMETER(oflags);
	UNUSED_PARAMETER(dev);


	softc = (struct sa_softc *)periph->softc;

	/*
	 * This should determine if something has happend since the last
	 * open/mount that would invalidate the mount. We do *not* want
	 * to retry this command- we just want the status. But we only
	 * do this if we're mounted already- if we're not mounted,
	 * we don't care about the unit read state and can instead use
	 * this opportunity to attempt to reserve the tape unit.
	 */
	
	if (softc->flags & SA_FLAG_TAPE_MOUNTED) {
		ccb = cam_periph_getccb(periph, 1);
		scsi_test_unit_ready(&ccb->csio, 0, sadone,
		    MSG_SIMPLE_Q_TAG, SSD_FULL_SIZE, IO_TIMEOUT);
		error = cam_periph_runccb(ccb, saerror, 0, SF_NO_PRINT,
		    softc->device_stats);
		QFRLS(ccb);
		if (error == ENXIO) {
			softc->flags &= ~SA_FLAG_TAPE_MOUNTED;
			scsi_test_unit_ready(&ccb->csio, 0, sadone,
			    MSG_SIMPLE_Q_TAG, SSD_FULL_SIZE, IO_TIMEOUT);
			error = cam_periph_runccb(ccb, saerror, 0, SF_NO_PRINT,
			    softc->device_stats);
			QFRLS(ccb);
		} else if (error) {
			/*
			 * We don't need to freeze the tape because we
			 * will now attempt to rewind/load it.
			 */
			softc->flags &= ~SA_FLAG_TAPE_MOUNTED;
			if (CAM_DEBUGGED(periph->path, CAM_DEBUG_INFO)) {
				xpt_print(periph->path,
				    "error %d on TUR in samount\n", error);
			}
		}
	} else {
		error = sareservereleaseunit(periph, TRUE);
		if (error) {
			return (error);
		}
		ccb = cam_periph_getccb(periph, 1);
		scsi_test_unit_ready(&ccb->csio, 0, sadone,
		    MSG_SIMPLE_Q_TAG, SSD_FULL_SIZE, IO_TIMEOUT);
		error = cam_periph_runccb(ccb, saerror, 0, SF_NO_PRINT,
		    softc->device_stats);
		QFRLS(ccb);
	}

	if ((softc->flags & SA_FLAG_TAPE_MOUNTED) == 0) {
		struct scsi_read_block_limits_data *rblim = NULL;
		int comp_enabled, comp_supported;
		u_int8_t write_protect, guessing = 0;

		/*
		 * Clear out old state.
		 */
		softc->flags &= ~(SA_FLAG_TAPE_WP|SA_FLAG_TAPE_WRITTEN|
				  SA_FLAG_ERR_PENDING|SA_FLAG_COMP_ENABLED|
				  SA_FLAG_COMP_SUPP|SA_FLAG_COMP_UNSUPP);
		softc->filemarks = 0;

		/*
		 * *Very* first off, make sure we're loaded to BOT.
		 */
		scsi_load_unload(&ccb->csio, 2, sadone, MSG_SIMPLE_Q_TAG, FALSE,
		    FALSE, FALSE, 1, SSD_FULL_SIZE, REWIND_TIMEOUT);
		error = cam_periph_runccb(ccb, saerror, 0, SF_NO_PRINT,
		    softc->device_stats);
		QFRLS(ccb);

		/*
		 * In case this doesn't work, do a REWIND instead
		 */
		if (error) {
			scsi_rewind(&ccb->csio, 2, sadone, MSG_SIMPLE_Q_TAG,
			    FALSE, SSD_FULL_SIZE, REWIND_TIMEOUT);
			error = cam_periph_runccb(ccb, saerror, 0, SF_NO_PRINT,
				softc->device_stats);
			QFRLS(ccb);
		}
		if (error) {
			xpt_release_ccb(ccb);
			goto exit;
		}

		/*
		 * Do a dummy test read to force access to the
		 * media so that the drive will really know what's
		 * there. We actually don't really care what the
		 * blocksize on tape is and don't expect to really
		 * read a full record.
		 */
		rblim = (struct  scsi_read_block_limits_data *)
		    malloc(8192, M_SCSISA, M_NOWAIT);
		if (rblim == NULL) {
			xpt_print(periph->path, "no memory for test read\n");
			xpt_release_ccb(ccb);
			error = ENOMEM;
			goto exit;
		}

		if ((softc->quirks & SA_QUIRK_NODREAD) == 0) {
			scsi_sa_read_write(&ccb->csio, 0, sadone,
			    MSG_SIMPLE_Q_TAG, 1, FALSE, 0, 8192,
			    (void *) rblim, 8192, SSD_FULL_SIZE,
			    IO_TIMEOUT);
			(void) cam_periph_runccb(ccb, saerror, 0, SF_NO_PRINT,
			    softc->device_stats);
			QFRLS(ccb);
			scsi_rewind(&ccb->csio, 1, sadone, MSG_SIMPLE_Q_TAG,
			    FALSE, SSD_FULL_SIZE, REWIND_TIMEOUT);
			error = cam_periph_runccb(ccb, saerror, CAM_RETRY_SELTO,
			    SF_NO_PRINT | SF_RETRY_UA,
			    softc->device_stats);
			QFRLS(ccb);
			if (error) {
				xpt_print(periph->path,
				    "unable to rewind after test read\n");
				xpt_release_ccb(ccb);
				goto exit;
			}
		}

		/*
		 * Next off, determine block limits.
		 */
		scsi_read_block_limits(&ccb->csio, 5, sadone, MSG_SIMPLE_Q_TAG,
		    rblim, SSD_FULL_SIZE, SCSIOP_TIMEOUT);

		error = cam_periph_runccb(ccb, saerror, CAM_RETRY_SELTO,
		    SF_NO_PRINT | SF_RETRY_UA, softc->device_stats);

		QFRLS(ccb);
		xpt_release_ccb(ccb);

		if (error != 0) {
			/*
			 * If it's less than SCSI-2, READ BLOCK LIMITS is not
			 * a MANDATORY command. Anyway- it doesn't matter-
			 * we can proceed anyway.
			 */
			softc->blk_gran = 0;
			softc->max_blk = ~0;
			softc->min_blk = 0;
		} else {
			if (softc->scsi_rev >= SCSI_REV_SPC) {
				softc->blk_gran = RBL_GRAN(rblim);
			} else {
				softc->blk_gran = 0;
			}
			/*
			 * We take max_blk == min_blk to mean a default to
			 * fixed mode- but note that whatever we get out of
			 * sagetparams below will actually determine whether
			 * we are actually *in* fixed mode.
			 */
			softc->max_blk = scsi_3btoul(rblim->maximum);
			softc->min_blk = scsi_2btoul(rblim->minimum);


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

		if (error != 0) {
			/*
			 * We could work a little harder here. We could
			 * adjust our attempts to get information. It
			 * might be an ancient tape drive. If someone
			 * nudges us, we'll do that.
			 */
			goto exit;
		}

		/*
		 * If no quirk has determined that this is a device that is
		 * preferred to be in fixed or variable mode, now is the time
		 * to find out.
	 	 */
		if ((softc->quirks & (SA_QUIRK_FIXED|SA_QUIRK_VARIABLE)) == 0) {
			guessing = 1;
			/*
			 * This could be expensive to find out. Luckily we
			 * only need to do this once. If we start out in
			 * 'default' mode, try and set ourselves to one
			 * of the densities that would determine a wad
			 * of other stuff. Go from highest to lowest.
			 */
			if (softc->media_density == SCSI_DEFAULT_DENSITY) {
				int i;
				static u_int8_t ctry[] = {
					SCSI_DENSITY_HALFINCH_PE,
					SCSI_DENSITY_HALFINCH_6250C,
					SCSI_DENSITY_HALFINCH_6250,
					SCSI_DENSITY_HALFINCH_1600,
					SCSI_DENSITY_HALFINCH_800,
					SCSI_DENSITY_QIC_4GB,
					SCSI_DENSITY_QIC_2GB,
					SCSI_DENSITY_QIC_525_320,
					SCSI_DENSITY_QIC_150,
					SCSI_DENSITY_QIC_120,
					SCSI_DENSITY_QIC_24,
					SCSI_DENSITY_QIC_11_9TRK,
					SCSI_DENSITY_QIC_11_4TRK,
					SCSI_DENSITY_QIC_1320,
					SCSI_DENSITY_QIC_3080,
					0
				};
				for (i = 0; ctry[i]; i++) {
					error = sasetparams(periph,
					    SA_PARAM_DENSITY, 0, ctry[i],
					    0, SF_NO_PRINT);
					if (error == 0) {
						softc->media_density = ctry[i];
						break;
					}
				}
			}
			switch (softc->media_density) {
			case SCSI_DENSITY_QIC_11_4TRK:
			case SCSI_DENSITY_QIC_11_9TRK:
			case SCSI_DENSITY_QIC_24:
			case SCSI_DENSITY_QIC_120:
			case SCSI_DENSITY_QIC_150:
			case SCSI_DENSITY_QIC_525_320:
			case SCSI_DENSITY_QIC_1320:
			case SCSI_DENSITY_QIC_3080:
				softc->quirks &= ~SA_QUIRK_2FM;
				softc->quirks |= SA_QUIRK_FIXED|SA_QUIRK_1FM;
				softc->last_media_blksize = 512;
				break;
			case SCSI_DENSITY_QIC_4GB:
			case SCSI_DENSITY_QIC_2GB:
				softc->quirks &= ~SA_QUIRK_2FM;
				softc->quirks |= SA_QUIRK_FIXED|SA_QUIRK_1FM;
				softc->last_media_blksize = 1024;
				break;
			default:
				softc->last_media_blksize =
				    softc->media_blksize;
				softc->quirks |= SA_QUIRK_VARIABLE;
				break;
			}
		}

		/*
		 * If no quirk has determined that this is a device that needs
		 * to have 2 Filemarks at EOD, now is the time to find out.
		 */

		if ((softc->quirks & SA_QUIRK_2FM) == 0) {
			switch (softc->media_density) {
			case SCSI_DENSITY_HALFINCH_800:
			case SCSI_DENSITY_HALFINCH_1600:
			case SCSI_DENSITY_HALFINCH_6250:
			case SCSI_DENSITY_HALFINCH_6250C:
			case SCSI_DENSITY_HALFINCH_PE:
				softc->quirks &= ~SA_QUIRK_1FM;
				softc->quirks |= SA_QUIRK_2FM;
				break;
			default:
				break;
			}
		}

		/*
		 * Now validate that some info we got makes sense.
		 */
		if ((softc->max_blk < softc->media_blksize) ||
		    (softc->min_blk > softc->media_blksize &&
		    softc->media_blksize)) {
			xpt_print(periph->path,
			    "BLOCK LIMITS (%d..%d) could not match current "
			    "block settings (%d)- adjusting\n", softc->min_blk,
			    softc->max_blk, softc->media_blksize);
			softc->max_blk = softc->min_blk =
			    softc->media_blksize;
		}

		/*
		 * Now put ourselves into the right frame of mind based
		 * upon quirks...
		 */
tryagain:
		/*
		 * If we want to be in FIXED mode and our current blocksize
		 * is not equal to our last blocksize (if nonzero), try and
		 * set ourselves to this last blocksize (as the 'preferred'
		 * block size).  The initial quirkmatch at registry sets the
		 * initial 'last' blocksize. If, for whatever reason, this
		 * 'last' blocksize is zero, set the blocksize to 512,
		 * or min_blk if that's larger.
		 */
		if ((softc->quirks & SA_QUIRK_FIXED) &&
		    (softc->quirks & SA_QUIRK_NO_MODESEL) == 0 &&
		    (softc->media_blksize != softc->last_media_blksize)) {
			softc->media_blksize = softc->last_media_blksize;
			if (softc->media_blksize == 0) {
				softc->media_blksize = 512;
				if (softc->media_blksize < softc->min_blk) {
					softc->media_blksize = softc->min_blk;
				}
			}
			error = sasetparams(periph, SA_PARAM_BLOCKSIZE,
			    softc->media_blksize, 0, 0, SF_NO_PRINT);
			if (error) {
				xpt_print(periph->path,
				    "unable to set fixed blocksize to %d\n",
				    softc->media_blksize);
				goto exit;
			}
		}

		if ((softc->quirks & SA_QUIRK_VARIABLE) && 
		    (softc->media_blksize != 0)) {
			softc->last_media_blksize = softc->media_blksize;
			softc->media_blksize = 0;
			error = sasetparams(periph, SA_PARAM_BLOCKSIZE,
			    0, 0, 0, SF_NO_PRINT);
			if (error) {
				/*
				 * If this fails and we were guessing, just
				 * assume that we got it wrong and go try
				 * fixed block mode. Don't even check against
				 * density code at this point.
				 */
				if (guessing) {
					softc->quirks &= ~SA_QUIRK_VARIABLE;
					softc->quirks |= SA_QUIRK_FIXED;
					if (softc->last_media_blksize == 0)
						softc->last_media_blksize = 512;
					goto tryagain;
				}
				xpt_print(periph->path,
				    "unable to set variable blocksize\n");
				goto exit;
			}
		}

		/*
		 * Now that we have the current block size,
		 * set up some parameters for sastart's usage.
		 */
		if (softc->media_blksize) {
			softc->flags |= SA_FLAG_FIXED;
			if (powerof2(softc->media_blksize)) {
				softc->blk_shift =
				    ffs(softc->media_blksize) - 1;
				softc->blk_mask = softc->media_blksize - 1;
			} else {
				softc->blk_mask = ~0;
				softc->blk_shift = 0;
			}
		} else {
			/*
			 * The SCSI-3 spec allows 0 to mean "unspecified".
			 * The SCSI-1 spec allows 0 to mean 'infinite'.
			 *
			 * Either works here.
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

		if (write_protect) 
			softc->flags |= SA_FLAG_TAPE_WP;

		if (comp_supported) {
			if (softc->saved_comp_algorithm == 0)
				softc->saved_comp_algorithm =
				    softc->comp_algorithm;
			softc->flags |= SA_FLAG_COMP_SUPP;
			if (comp_enabled)
				softc->flags |= SA_FLAG_COMP_ENABLED;
		} else
			softc->flags |= SA_FLAG_COMP_UNSUPP;

		if ((softc->buffer_mode == SMH_SA_BUF_MODE_NOBUF) &&
		    (softc->quirks & SA_QUIRK_NO_MODESEL) == 0) {
			error = sasetparams(periph, SA_PARAM_BUFF_MODE, 0,
			    0, 0, SF_NO_PRINT);
			if (error == 0) {
				softc->buffer_mode = SMH_SA_BUF_MODE_SIBUF;
			} else {
				xpt_print(periph->path,
				    "unable to set buffered mode\n");
			}
			error = 0;	/* not an error */
		}


		if (error == 0) {
			softc->flags |= SA_FLAG_TAPE_MOUNTED;
		}
exit:
		if (rblim != NULL)
			free(rblim, M_SCSISA);

		if (error != 0) {
			softc->dsreg = MTIO_DSREG_NIL;
		} else {
			softc->fileno = softc->blkno = 0;
			softc->dsreg = MTIO_DSREG_REST;
		}
#ifdef	SA_1FM_AT_EOD
		if ((softc->quirks & SA_QUIRK_2FM) == 0)
			softc->quirks |= SA_QUIRK_1FM;
#else
		if ((softc->quirks & SA_QUIRK_1FM) == 0)
			softc->quirks |= SA_QUIRK_2FM;
#endif
	} else
		xpt_release_ccb(ccb);

	/*
	 * If we return an error, we're not mounted any more,
	 * so release any device reservation.
	 */
	if (error != 0) {
		(void) sareservereleaseunit(periph, FALSE);
	} else {
		/*
		 * Clear I/O residual.
		 */
		softc->last_io_resid = 0;
		softc->last_ctl_resid = 0;
	}
	return (error);
}

/*
 * How many filemarks do we need to write if we were to terminate the
 * tape session right now? Note that this can be a negative number
 */

static int
samarkswanted(struct cam_periph *periph)
{
	int	markswanted;
	struct	sa_softc *softc;

	softc = (struct sa_softc *)periph->softc;
	markswanted = 0;
	if ((softc->flags & SA_FLAG_TAPE_WRITTEN) != 0) {
		markswanted++;
		if (softc->quirks & SA_QUIRK_2FM)
			markswanted++;
	}
	markswanted -= softc->filemarks;
	return (markswanted);
}

static int
sacheckeod(struct cam_periph *periph)
{
	int	error;
	int	markswanted;

	markswanted = samarkswanted(periph);

	if (markswanted > 0) {
		error = sawritefilemarks(periph, markswanted, FALSE);
	} else {
		error = 0;
	}
	return (error);
}

static int
saerror(union ccb *ccb, u_int32_t cflgs, u_int32_t sflgs)
{
	static const char *toobig =
	    "%d-byte tape record bigger than supplied buffer\n";
	struct	cam_periph *periph;
	struct	sa_softc *softc;
	struct	ccb_scsiio *csio;
	struct	scsi_sense_data *sense;
	u_int32_t resid = 0;
	int32_t	info = 0;
	cam_status status;
	int error_code, sense_key, asc, ascq, error, aqvalid;

	periph = xpt_path_periph(ccb->ccb_h.path);
	softc = (struct sa_softc *)periph->softc;
	csio = &ccb->csio;
	sense = &csio->sense_data;
	scsi_extract_sense(sense, &error_code, &sense_key, &asc, &ascq);
	aqvalid = sense->extra_len >= 6;
	error = 0;

	status = csio->ccb_h.status & CAM_STATUS_MASK;

	/*
	 * Calculate/latch up, any residuals... We do this in a funny 2-step
	 * so we can print stuff here if we have CAM_DEBUG enabled for this
	 * unit.
	 */
	if (status == CAM_SCSI_STATUS_ERROR) {
		if ((sense->error_code & SSD_ERRCODE_VALID) != 0) {
			info = (int32_t) scsi_4btoul(sense->info);
			resid = info;
			if ((softc->flags & SA_FLAG_FIXED) != 0)
				resid *= softc->media_blksize;
		} else {
			resid = csio->dxfer_len;
			info = resid;
			if ((softc->flags & SA_FLAG_FIXED) != 0) {
				if (softc->media_blksize)
					info /= softc->media_blksize;
			}
		}
		if (CCB_Type(csio) == SA_CCB_BUFFER_IO) {
			bcopy((caddr_t) sense, (caddr_t) &softc->last_io_sense,
			    sizeof (struct scsi_sense_data));
			bcopy(csio->cdb_io.cdb_bytes, softc->last_io_cdb,
			    (int) csio->cdb_len);
			softc->last_io_resid = resid;
			softc->last_resid_was_io = 1;
		} else {
			bcopy((caddr_t) sense, (caddr_t) &softc->last_ctl_sense,
			    sizeof (struct scsi_sense_data));
			bcopy(csio->cdb_io.cdb_bytes, softc->last_ctl_cdb,
			    (int) csio->cdb_len);
			softc->last_ctl_resid = resid;
			softc->last_resid_was_io = 0;
		}
		CAM_DEBUG(periph->path, CAM_DEBUG_INFO, ("CDB[0]=0x%x Key 0x%x "
		    "ASC/ASCQ 0x%x/0x%x CAM STATUS 0x%x flags 0x%x resid %d "
		    "dxfer_len %d\n", csio->cdb_io.cdb_bytes[0] & 0xff,
		    sense_key, asc, ascq, status,
		    sense->flags & ~SSD_KEY_RESERVED, resid, csio->dxfer_len));
	} else {
		CAM_DEBUG(periph->path, CAM_DEBUG_INFO,
		    ("Cam Status 0x%x\n", status));
	}

	switch (status) {
	case CAM_REQ_CMP:
		return (0);
	case CAM_SCSI_STATUS_ERROR:
		/*
		 * If a read/write command, we handle it here.
		 */
		if (CCB_Type(csio) != SA_CCB_WAITING) {
			break;
		}
		/*
		 * If this was just EOM/EOP, Filemark, Setmark or ILI detected
		 * on a non read/write command, we assume it's not an error
		 * and propagate the residule and return.
		 */
		if ((aqvalid && asc == 0 && ascq > 0 && ascq <= 5) ||
		    (aqvalid == 0 && sense_key == SSD_KEY_NO_SENSE)) {
			csio->resid = resid;
			QFRLS(ccb);
			return (0);
		}
		/*
		 * Otherwise, we let the common code handle this.
		 */
		return (cam_periph_error(ccb, cflgs, sflgs, &softc->saved_ccb));

	/*
	 * XXX: To Be Fixed
	 * We cannot depend upon CAM honoring retry counts for these.
	 */
	case CAM_SCSI_BUS_RESET:
	case CAM_BDR_SENT:
		if (ccb->ccb_h.retry_count <= 0) {
			return (EIO);
		}
		/* FALLTHROUGH */
	default:
		return (cam_periph_error(ccb, cflgs, sflgs, &softc->saved_ccb));
	}

	/*
	 * Handle filemark, end of tape, mismatched record sizes....
	 * From this point out, we're only handling read/write cases.
	 * Handle writes && reads differently.
	 */

	if (csio->cdb_io.cdb_bytes[0] == SA_WRITE) {
		if (sense_key == SSD_KEY_VOLUME_OVERFLOW) {
			csio->resid = resid;
			error = ENOSPC;
		} else if (sense->flags & SSD_EOM) {
			softc->flags |= SA_FLAG_EOM_PENDING;
			/*
			 * Grotesque as it seems, the few times
			 * I've actually seen a non-zero resid,
			 * the tape drive actually lied and had
			 * written all the data!.
			 */
			csio->resid = 0;
		}
	} else {
		csio->resid = resid;
		if (sense_key == SSD_KEY_BLANK_CHECK) {
			if (softc->quirks & SA_QUIRK_1FM) {
				error = 0;
				softc->flags |= SA_FLAG_EOM_PENDING;
			} else {
				error = EIO;
			}
		} else if (sense->flags & SSD_FILEMARK) {
			if (softc->flags & SA_FLAG_FIXED) {
				error = -1;
				softc->flags |= SA_FLAG_EOF_PENDING;
			}
			/*
			 * Unconditionally, if we detected a filemark on a read,
			 * mark that we've run moved a file ahead.
			 */
			if (softc->fileno != (daddr_t) -1) {
				softc->fileno++;
				softc->blkno = 0;
				csio->ccb_h.ccb_pflags |= SA_POSITION_UPDATED;
			}
		}
	}

	/*
	 * Incorrect Length usually applies to read, but can apply to writes.
	 */
	if (error == 0 && (sense->flags & SSD_ILI)) {
		if (info < 0) {
			xpt_print(csio->ccb_h.path, toobig,
			    csio->dxfer_len - info);
			csio->resid = csio->dxfer_len;
			error = EIO;
		} else {
			csio->resid = resid;
			if (softc->flags & SA_FLAG_FIXED) {
				softc->flags |= SA_FLAG_EIO_PENDING;
			}
			/*
			 * Bump the block number if we hadn't seen a filemark.
			 * Do this independent of errors (we've moved anyway).
			 */
			if ((sense->flags & SSD_FILEMARK) == 0) {
				if (softc->blkno != (daddr_t) -1) {
					softc->blkno++;
					csio->ccb_h.ccb_pflags |=
					   SA_POSITION_UPDATED;
				}
			}
		}
	}

	if (error <= 0) {
		/*
		 * Unfreeze the queue if frozen as we're not returning anything
		 * to our waiters that would indicate an I/O error has occurred
		 * (yet).
		 */
		QFRLS(ccb);
		error = 0;
	}
	return (error);
}

static int
sagetparams(struct cam_periph *periph, sa_params params_to_get,
	    u_int32_t *blocksize, u_int8_t *density, u_int32_t *numblocks,
	    int *buff_mode, u_int8_t *write_protect, u_int8_t *speed,
	    int *comp_supported, int *comp_enabled, u_int32_t *comp_algorithm,
	    sa_comp_t *tcs)
{
	union ccb *ccb;
	void *mode_buffer;
	struct scsi_mode_header_6 *mode_hdr;
	struct scsi_mode_blk_desc *mode_blk;
	int mode_buffer_len;
	struct sa_softc *softc;
	u_int8_t cpage;
	int error;
	cam_status status;

	softc = (struct sa_softc *)periph->softc;
	ccb = cam_periph_getccb(periph, 1);
	if (softc->quirks & SA_QUIRK_NO_CPAGE)
		cpage = SA_DEVICE_CONFIGURATION_PAGE;
	else
		cpage = SA_DATA_COMPRESSION_PAGE;

retry:
	mode_buffer_len = sizeof(*mode_hdr) + sizeof(*mode_blk);

	if (params_to_get & SA_PARAM_COMPRESSION) {
		if (softc->quirks & SA_QUIRK_NOCOMP) {
			*comp_supported = FALSE;
			params_to_get &= ~SA_PARAM_COMPRESSION;
		} else
			mode_buffer_len += sizeof (sa_comp_t);
	}

	/* XXX Fix M_NOWAIT */
	mode_buffer = malloc(mode_buffer_len, M_SCSISA, M_NOWAIT | M_ZERO);
	if (mode_buffer == NULL) {
		xpt_release_ccb(ccb);
		return (ENOMEM);
	}
	mode_hdr = (struct scsi_mode_header_6 *)mode_buffer;
	mode_blk = (struct scsi_mode_blk_desc *)&mode_hdr[1];

	/* it is safe to retry this */
	scsi_mode_sense(&ccb->csio, 5, sadone, MSG_SIMPLE_Q_TAG, FALSE,
	    SMS_PAGE_CTRL_CURRENT, (params_to_get & SA_PARAM_COMPRESSION) ?
	    cpage : SMS_VENDOR_SPECIFIC_PAGE, mode_buffer, mode_buffer_len,
	    SSD_FULL_SIZE, SCSIOP_TIMEOUT);

	error = cam_periph_runccb(ccb, saerror, 0, SF_NO_PRINT,
	    softc->device_stats);
	QFRLS(ccb);

	status = ccb->ccb_h.status & CAM_STATUS_MASK;

	if (error == EINVAL && (params_to_get & SA_PARAM_COMPRESSION) != 0) {
		/*
		 * Hmm. Let's see if we can try another page...
		 * If we've already done that, give up on compression
		 * for this device and remember this for the future
		 * and attempt the request without asking for compression
		 * info.
		 */
		if (cpage == SA_DATA_COMPRESSION_PAGE) {
			cpage = SA_DEVICE_CONFIGURATION_PAGE;
			goto retry;
		}
		softc->quirks |= SA_QUIRK_NOCOMP;
		free(mode_buffer, M_SCSISA);
		goto retry;
	} else if (status == CAM_SCSI_STATUS_ERROR) {
		/* Tell the user about the fatal error. */
		scsi_sense_print(&ccb->csio);
		goto sagetparamsexit;
	}

	/*
	 * If the user only wants the compression information, and
	 * the device doesn't send back the block descriptor, it's
	 * no big deal.  If the user wants more than just
	 * compression, though, and the device doesn't pass back the
	 * block descriptor, we need to send another mode sense to
	 * get the block descriptor.
	 */
	if ((mode_hdr->blk_desc_len == 0) &&
	    (params_to_get & SA_PARAM_COMPRESSION) &&
	    (params_to_get & ~(SA_PARAM_COMPRESSION))) {

		/*
		 * Decrease the mode buffer length by the size of
		 * the compression page, to make sure the data
		 * there doesn't get overwritten.
		 */
		mode_buffer_len -= sizeof (sa_comp_t);

		/*
		 * Now move the compression page that we presumably
		 * got back down the memory chunk a little bit so
		 * it doesn't get spammed.
		 */
		bcopy(&mode_hdr[0], &mode_hdr[1], sizeof (sa_comp_t));
		bzero(&mode_hdr[0], sizeof (mode_hdr[0]));

		/*
		 * Now, we issue another mode sense and just ask
		 * for the block descriptor, etc.
		 */

		scsi_mode_sense(&ccb->csio, 2, sadone, MSG_SIMPLE_Q_TAG, FALSE,
		    SMS_PAGE_CTRL_CURRENT, SMS_VENDOR_SPECIFIC_PAGE,
		    mode_buffer, mode_buffer_len, SSD_FULL_SIZE,
		    SCSIOP_TIMEOUT);

		error = cam_periph_runccb(ccb, saerror, 0, SF_NO_PRINT,
		    softc->device_stats);
		QFRLS(ccb);

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
		*write_protect = (mode_hdr->dev_spec & SMH_SA_WP)? TRUE : FALSE;

	if (params_to_get & SA_PARAM_SPEED)
		*speed = mode_hdr->dev_spec & SMH_SA_SPEED_MASK;

	if (params_to_get & SA_PARAM_COMPRESSION) {
		sa_comp_t *ntcs = (sa_comp_t *) &mode_blk[1];
		if (cpage == SA_DATA_COMPRESSION_PAGE) {
			struct scsi_data_compression_page *cp = &ntcs->dcomp;
			*comp_supported =
			    (cp->dce_and_dcc & SA_DCP_DCC)? TRUE : FALSE;
			*comp_enabled =
			    (cp->dce_and_dcc & SA_DCP_DCE)? TRUE : FALSE;
			*comp_algorithm = scsi_4btoul(cp->comp_algorithm);
		} else {
			struct scsi_dev_conf_page *cp = &ntcs->dconf;
			/*
			 * We don't really know whether this device supports
			 * Data Compression if the the algorithm field is
			 * zero. Just say we do.
			 */
			*comp_supported = TRUE;
			*comp_enabled =
			    (cp->sel_comp_alg != SA_COMP_NONE)? TRUE : FALSE;
			*comp_algorithm = cp->sel_comp_alg;
		}
		if (tcs != NULL)
			bcopy(ntcs, tcs, sizeof (sa_comp_t));
	}

	if (CAM_DEBUGGED(periph->path, CAM_DEBUG_INFO)) {
		int idx;
		char *xyz = mode_buffer;
		xpt_print_path(periph->path);
		printf("Mode Sense Data=");
		for (idx = 0; idx < mode_buffer_len; idx++)
			printf(" 0x%02x", xyz[idx] & 0xff);
		printf("\n");
	}

sagetparamsexit:

	xpt_release_ccb(ccb);
	free(mode_buffer, M_SCSISA);
	return (error);
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
	    u_int32_t blocksize, u_int8_t density, u_int32_t calg,
	    u_int32_t sense_flags)
{
	struct sa_softc *softc;
	u_int32_t current_blocksize;
	u_int32_t current_calg;
	u_int8_t current_density;
	u_int8_t current_speed;
	int comp_enabled, comp_supported;
	void *mode_buffer;
	int mode_buffer_len;
	struct scsi_mode_header_6 *mode_hdr;
	struct scsi_mode_blk_desc *mode_blk;
	sa_comp_t *ccomp, *cpage;
	int buff_mode;
	union ccb *ccb = NULL;
	int error;

	softc = (struct sa_softc *)periph->softc;

	ccomp = malloc(sizeof (sa_comp_t), M_SCSISA, M_NOWAIT);
	if (ccomp == NULL)
		return (ENOMEM);

	/*
	 * Since it doesn't make sense to set the number of blocks, or
	 * write protection, we won't try to get the current value.  We
	 * always want to get the blocksize, so we can set it back to the
	 * proper value.
	 */
	error = sagetparams(periph,
	    params_to_set | SA_PARAM_BLOCKSIZE | SA_PARAM_SPEED,
	    &current_blocksize, &current_density, NULL, &buff_mode, NULL,
	    &current_speed, &comp_supported, &comp_enabled,
	    &current_calg, ccomp);

	if (error != 0) {
		free(ccomp, M_SCSISA);
		return (error);
	}

	mode_buffer_len = sizeof(*mode_hdr) + sizeof(*mode_blk);
	if (params_to_set & SA_PARAM_COMPRESSION)
		mode_buffer_len += sizeof (sa_comp_t);

	mode_buffer = malloc(mode_buffer_len, M_SCSISA, M_NOWAIT | M_ZERO);
	if (mode_buffer == NULL) {
		free(ccomp, M_SCSISA);
		return (ENOMEM);
	}

	mode_hdr = (struct scsi_mode_header_6 *)mode_buffer;
	mode_blk = (struct scsi_mode_blk_desc *)&mode_hdr[1];

	ccb = cam_periph_getccb(periph, 1);

retry:

	if (params_to_set & SA_PARAM_COMPRESSION) {
		if (mode_blk) {
			cpage = (sa_comp_t *)&mode_blk[1];
		} else {
			cpage = (sa_comp_t *)&mode_hdr[1];
		}
		bcopy(ccomp, cpage, sizeof (sa_comp_t));
		cpage->hdr.pagecode &= ~0x80;
	} else
		cpage = NULL;

	/*
	 * If the caller wants us to set the blocksize, use the one they
	 * pass in.  Otherwise, use the blocksize we got back from the
	 * mode select above.
	 */
	if (mode_blk) {
		if (params_to_set & SA_PARAM_BLOCKSIZE)
			scsi_ulto3b(blocksize, mode_blk->blklen);
		else
			scsi_ulto3b(current_blocksize, mode_blk->blklen);

		/*
		 * Set density if requested, else preserve old density.
		 * SCSI_SAME_DENSITY only applies to SCSI-2 or better
		 * devices, else density we've latched up in our softc.
		 */
		if (params_to_set & SA_PARAM_DENSITY) {
			mode_blk->density = density;
		} else if (softc->scsi_rev > SCSI_REV_CCS) {
			mode_blk->density = SCSI_SAME_DENSITY;
		} else {
			mode_blk->density = softc->media_density;
		}
	}

	/*
	 * For mode selects, these two fields must be zero.
	 */
	mode_hdr->data_length = 0;
	mode_hdr->medium_type = 0;

	/* set the speed to the current value */
	mode_hdr->dev_spec = current_speed;

	/* if set, set single-initiator buffering mode */
	if (softc->buffer_mode == SMH_SA_BUF_MODE_SIBUF) {
		mode_hdr->dev_spec |= SMH_SA_BUF_MODE_SIBUF;
	}

	if (mode_blk)
		mode_hdr->blk_desc_len = sizeof(struct scsi_mode_blk_desc);
	else
		mode_hdr->blk_desc_len = 0;

	/*
	 * First, if the user wants us to set the compression algorithm or
	 * just turn compression on, check to make sure that this drive
	 * supports compression.
	 */
	if (params_to_set & SA_PARAM_COMPRESSION) {
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
		switch (ccomp->hdr.pagecode & ~0x80) {
		case SA_DEVICE_CONFIGURATION_PAGE:
		{
			struct scsi_dev_conf_page *dcp = &cpage->dconf;
			if (calg == 0) {
				dcp->sel_comp_alg = SA_COMP_NONE;
				break;
			}
			if (calg != MT_COMP_ENABLE) {
				dcp->sel_comp_alg = calg;
			} else if (dcp->sel_comp_alg == SA_COMP_NONE &&
			    softc->saved_comp_algorithm != 0) {
				dcp->sel_comp_alg = softc->saved_comp_algorithm;
			}
			break;
		}
		case SA_DATA_COMPRESSION_PAGE:
		if (ccomp->dcomp.dce_and_dcc & SA_DCP_DCC) {
			struct scsi_data_compression_page *dcp = &cpage->dcomp;
			if (calg == 0) {
				/*
				 * Disable compression, but leave the
				 * decompression and the capability bit
				 * alone.
				 */
				dcp->dce_and_dcc = SA_DCP_DCC;
				dcp->dde_and_red |= SA_DCP_DDE;
				break;
			}
			/* enable compression && decompression */
			dcp->dce_and_dcc = SA_DCP_DCE | SA_DCP_DCC;
			dcp->dde_and_red |= SA_DCP_DDE;
			/*
			 * If there, use compression algorithm from caller.
			 * Otherwise, if there's a saved compression algorithm
			 * and there is no current algorithm, use the saved
			 * algorithm. Else parrot back what we got and hope
			 * for the best.
			 */
			if (calg != MT_COMP_ENABLE) {
				scsi_ulto4b(calg, dcp->comp_algorithm);
				scsi_ulto4b(calg, dcp->decomp_algorithm);
			} else if (scsi_4btoul(dcp->comp_algorithm) == 0 &&
			    softc->saved_comp_algorithm != 0) {
				scsi_ulto4b(softc->saved_comp_algorithm,
				    dcp->comp_algorithm);
				scsi_ulto4b(softc->saved_comp_algorithm,
				    dcp->decomp_algorithm);
			}
			break;
		}
		/*
		 * Compression does not appear to be supported-
		 * at least via the DATA COMPRESSION page. It
		 * would be too much to ask us to believe that
		 * the page itself is supported, but incorrectly
		 * reports an ability to manipulate data compression,
		 * so we'll assume that this device doesn't support
		 * compression. We can just fall through for that.
		 */
		/* FALLTHROUGH */
		default:
			/*
			 * The drive doesn't seem to support compression,
			 * so turn off the set compression bit.
			 */
			params_to_set &= ~SA_PARAM_COMPRESSION;
			xpt_print(periph->path,
			    "device does not seem to support compression\n");

			/*
			 * If that was the only thing the user wanted us to set,
			 * clean up allocated resources and return with
			 * 'operation not supported'.
			 */
			if (params_to_set == SA_PARAM_NONE) {
				free(mode_buffer, M_SCSISA);
				xpt_release_ccb(ccb);
				return (ENODEV);
			}
		
			/*
			 * That wasn't the only thing the user wanted us to set.
			 * So, decrease the stated mode buffer length by the
			 * size of the compression mode page.
			 */
			mode_buffer_len -= sizeof(sa_comp_t);
		}
	}

	/* It is safe to retry this operation */
	scsi_mode_select(&ccb->csio, 5, sadone, MSG_SIMPLE_Q_TAG,
	    (params_to_set & SA_PARAM_COMPRESSION)? TRUE : FALSE,
	    FALSE, mode_buffer, mode_buffer_len, SSD_FULL_SIZE, SCSIOP_TIMEOUT);

	error = cam_periph_runccb(ccb, saerror, 0,
	    sense_flags, softc->device_stats);
	QFRLS(ccb);

	if (CAM_DEBUGGED(periph->path, CAM_DEBUG_INFO)) {
		int idx;
		char *xyz = mode_buffer;
		xpt_print_path(periph->path);
		printf("Err%d, Mode Select Data=", error);
		for (idx = 0; idx < mode_buffer_len; idx++)
			printf(" 0x%02x", xyz[idx] & 0xff);
		printf("\n");
	}


	if (error) {
		/*
		 * If we can, try without setting density/blocksize.
		 */
		if (mode_blk) {
			if ((params_to_set &
			    (SA_PARAM_DENSITY|SA_PARAM_BLOCKSIZE)) == 0) {
				mode_blk = NULL;
				goto retry;
			}
		} else {
			mode_blk = (struct scsi_mode_blk_desc *)&mode_hdr[1];
			cpage = (sa_comp_t *)&mode_blk[1];
		}

		/*
		 * If we were setting the blocksize, and that failed, we
		 * want to set it to its original value.  If we weren't
		 * setting the blocksize, we don't want to change it.
		 */
		scsi_ulto3b(current_blocksize, mode_blk->blklen);

		/*
		 * Set density if requested, else preserve old density.
		 * SCSI_SAME_DENSITY only applies to SCSI-2 or better
		 * devices, else density we've latched up in our softc.
		 */
		if (params_to_set & SA_PARAM_DENSITY) {
			mode_blk->density = current_density;
		} else if (softc->scsi_rev > SCSI_REV_CCS) {
			mode_blk->density = SCSI_SAME_DENSITY;
		} else {
			mode_blk->density = softc->media_density;
		}

		if (params_to_set & SA_PARAM_COMPRESSION)
			bcopy(ccomp, cpage, sizeof (sa_comp_t));

		/*
		 * The retry count is the only CCB field that might have been
		 * changed that we care about, so reset it back to 1.
		 */
		ccb->ccb_h.retry_count = 1;
		cam_periph_runccb(ccb, saerror, 0, sense_flags,
		    softc->device_stats);
		QFRLS(ccb);
	}

	xpt_release_ccb(ccb);

	if (ccomp != NULL)
		free(ccomp, M_SCSISA);

	if (params_to_set & SA_PARAM_COMPRESSION) {
		if (error) {
			softc->flags &= ~SA_FLAG_COMP_ENABLED;
			/*
			 * Even if we get an error setting compression,
			 * do not say that we don't support it. We could
			 * have been wrong, or it may be media specific.
			 *	softc->flags &= ~SA_FLAG_COMP_SUPP;
			 */
			softc->saved_comp_algorithm = softc->comp_algorithm;
			softc->comp_algorithm = 0;
		} else {
			softc->flags |= SA_FLAG_COMP_ENABLED;
			softc->comp_algorithm = calg;
		}
	}

	free(mode_buffer, M_SCSISA);
	return (error);
}

static void
saprevent(struct cam_periph *periph, int action)
{
	struct	sa_softc *softc;
	union	ccb *ccb;		
	int	error, sf;
		
	softc = (struct sa_softc *)periph->softc;

	if ((action == PR_ALLOW) && (softc->flags & SA_FLAG_TAPE_LOCKED) == 0)
		return;
	if ((action == PR_PREVENT) && (softc->flags & SA_FLAG_TAPE_LOCKED) != 0)
		return;

	/*
	 * We can be quiet about illegal requests.
	 */
	if (CAM_DEBUGGED(periph->path, CAM_DEBUG_INFO)) {
		sf = 0;
	} else
		sf = SF_QUIET_IR;

	ccb = cam_periph_getccb(periph, 1);

	/* It is safe to retry this operation */
	scsi_prevent(&ccb->csio, 5, sadone, MSG_SIMPLE_Q_TAG, action,
	    SSD_FULL_SIZE, SCSIOP_TIMEOUT);

	error = cam_periph_runccb(ccb, saerror, 0, sf, softc->device_stats);
	QFRLS(ccb);
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

	ccb = cam_periph_getccb(periph, 1);

	/* It is safe to retry this operation */
	scsi_rewind(&ccb->csio, 2, sadone, MSG_SIMPLE_Q_TAG, FALSE,
	    SSD_FULL_SIZE, REWIND_TIMEOUT);

	softc->dsreg = MTIO_DSREG_REW;
	error = cam_periph_runccb(ccb, saerror, 0, 0, softc->device_stats);
	softc->dsreg = MTIO_DSREG_REST;

	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path, 0, 0, 0, FALSE);

	xpt_release_ccb(ccb);
	if (error == 0)
		softc->fileno = softc->blkno = (daddr_t) 0;
	else
		softc->fileno = softc->blkno = (daddr_t) -1;
	return (error);
}

static int
saspace(struct cam_periph *periph, int count, scsi_space_code code)
{
	union	ccb *ccb;
	struct	sa_softc *softc;
	int	error;
		
	softc = (struct sa_softc *)periph->softc;

	ccb = cam_periph_getccb(periph, 1);

	/* This cannot be retried */

	scsi_space(&ccb->csio, 0, sadone, MSG_SIMPLE_Q_TAG, code, count,
	    SSD_FULL_SIZE, SPACE_TIMEOUT);

	/*
	 * Clear residual because we will be using it.
	 */
	softc->last_ctl_resid = 0;

	softc->dsreg = (count < 0)? MTIO_DSREG_REV : MTIO_DSREG_FWD;
	error = cam_periph_runccb(ccb, saerror, 0, 0, softc->device_stats);
	softc->dsreg = MTIO_DSREG_REST;

	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path, 0, 0, 0, FALSE);

	xpt_release_ccb(ccb);

	/*
	 * If a spacing operation has failed, we need to invalidate
	 * this mount.
	 *
	 * If the spacing operation was setmarks or to end of recorded data,
	 * we no longer know our relative position.
	 *
	 * If the spacing operations was spacing files in reverse, we
	 * take account of the residual, but still check against less
	 * than zero- if we've gone negative, we must have hit BOT.
	 *
	 * If the spacing operations was spacing records in reverse and
	 * we have a residual, we've either hit BOT or hit a filemark.
	 * In the former case, we know our new record number (0). In
	 * the latter case, we have absolutely no idea what the real
	 * record number is- we've stopped between the end of the last
	 * record in the previous file and the filemark that stopped
	 * our spacing backwards.
	 */
	if (error) {
		softc->fileno = softc->blkno = (daddr_t) -1;
	} else if (code == SS_SETMARKS || code == SS_EOD) {
		softc->fileno = softc->blkno = (daddr_t) -1;
	} else if (code == SS_FILEMARKS && softc->fileno != (daddr_t) -1) {
		softc->fileno += (count - softc->last_ctl_resid);
		if (softc->fileno < 0)	/* we must of hit BOT */
			softc->fileno = 0;
		softc->blkno = 0;
	} else if (code == SS_BLOCKS && softc->blkno != (daddr_t) -1) {
		softc->blkno += (count - softc->last_ctl_resid);
		if (count < 0) {
			if (softc->last_ctl_resid || softc->blkno < 0) {
				if (softc->fileno == 0) {
					softc->blkno = 0;
				} else {
					softc->blkno = (daddr_t) -1;
				}
			}
		}
	}
	return (error);
}

static int
sawritefilemarks(struct cam_periph *periph, int nmarks, int setmarks)
{
	union	ccb *ccb;
	struct	sa_softc *softc;
	int	error, nwm = 0;

	softc = (struct sa_softc *)periph->softc;
	if (softc->open_rdonly)
		return (EBADF);

	ccb = cam_periph_getccb(periph, 1);
	/*
	 * Clear residual because we will be using it.
	 */
	softc->last_ctl_resid = 0;

	softc->dsreg = MTIO_DSREG_FMK;
	/* this *must* not be retried */
	scsi_write_filemarks(&ccb->csio, 0, sadone, MSG_SIMPLE_Q_TAG,
	    FALSE, setmarks, nmarks, SSD_FULL_SIZE, IO_TIMEOUT);
	softc->dsreg = MTIO_DSREG_REST;


	error = cam_periph_runccb(ccb, saerror, 0, 0, softc->device_stats);

	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path, 0, 0, 0, FALSE);

	if (error == 0 && nmarks) {
		struct sa_softc *softc = (struct sa_softc *)periph->softc;
		nwm = nmarks - softc->last_ctl_resid;
		softc->filemarks += nwm;
	}

	xpt_release_ccb(ccb);

	/*
	 * Update relative positions (if we're doing that).
	 */
	if (error) {
		softc->fileno = softc->blkno = (daddr_t) -1;
	} else if (softc->fileno != (daddr_t) -1) {
		softc->fileno += nwm;
		softc->blkno = 0;
	}
	return (error);
}

static int
sardpos(struct cam_periph *periph, int hard, u_int32_t *blkptr)
{
	struct scsi_tape_position_data loc;
	union ccb *ccb;
	struct sa_softc *softc = (struct sa_softc *)periph->softc;
	int error;

	/*
	 * We try and flush any buffered writes here if we were writing
	 * and we're trying to get hardware block position. It eats
	 * up performance substantially, but I'm wary of drive firmware.
	 *
	 * I think that *logical* block position is probably okay-
	 * but hardware block position might have to wait for data
	 * to hit media to be valid. Caveat Emptor.
	 */

	if (hard && (softc->flags & SA_FLAG_TAPE_WRITTEN)) {
		error = sawritefilemarks(periph, 0, 0);
		if (error && error != EACCES)
			return (error);
	}

	ccb = cam_periph_getccb(periph, 1);
	scsi_read_position(&ccb->csio, 1, sadone, MSG_SIMPLE_Q_TAG,
	    hard, &loc, SSD_FULL_SIZE, SCSIOP_TIMEOUT);
	softc->dsreg = MTIO_DSREG_RBSY;
	error = cam_periph_runccb(ccb, saerror, 0, 0, softc->device_stats);
	softc->dsreg = MTIO_DSREG_REST;
	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path, 0, 0, 0, 0);

	if (error == 0) {
		if (loc.flags & SA_RPOS_UNCERTAIN) {
			error = EINVAL;		/* nothing is certain */
		} else {
			*blkptr = scsi_4btoul(loc.firstblk);
		}
	}

	xpt_release_ccb(ccb);
	return (error);
}

static int
sasetpos(struct cam_periph *periph, int hard, u_int32_t *blkptr)
{
	union ccb *ccb;
	struct sa_softc *softc;
	int error;

	/*
	 * We used to try and flush any buffered writes here.
	 * Now we push this onto user applications to either
	 * flush the pending writes themselves (via a zero count
	 * WRITE FILEMARKS command) or they can trust their tape
	 * drive to do this correctly for them.
 	 */

	softc = (struct sa_softc *)periph->softc;
	ccb = cam_periph_getccb(periph, 1);

	
	scsi_set_position(&ccb->csio, 1, sadone, MSG_SIMPLE_Q_TAG,
	    hard, *blkptr, SSD_FULL_SIZE, SPACE_TIMEOUT);


	softc->dsreg = MTIO_DSREG_POS;
	error = cam_periph_runccb(ccb, saerror, 0, 0, softc->device_stats);
	softc->dsreg = MTIO_DSREG_REST;
	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path, 0, 0, 0, 0);
	xpt_release_ccb(ccb);
	/*
	 * Note relative file && block number position as now unknown.
	 */
	softc->fileno = softc->blkno = (daddr_t) -1;
	return (error);
}

static int
saretension(struct cam_periph *periph)
{
	union ccb *ccb;
	struct sa_softc *softc;
	int error;

	softc = (struct sa_softc *)periph->softc;

	ccb = cam_periph_getccb(periph, 1);

	/* It is safe to retry this operation */
	scsi_load_unload(&ccb->csio, 5, sadone, MSG_SIMPLE_Q_TAG, FALSE,
	    FALSE, TRUE,  TRUE, SSD_FULL_SIZE, ERASE_TIMEOUT);

	softc->dsreg = MTIO_DSREG_TEN;
	error = cam_periph_runccb(ccb, saerror, 0, 0, softc->device_stats);
	softc->dsreg = MTIO_DSREG_REST;

	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path, 0, 0, 0, FALSE);
	xpt_release_ccb(ccb);
	if (error == 0)
		softc->fileno = softc->blkno = (daddr_t) 0;
	else
		softc->fileno = softc->blkno = (daddr_t) -1;
	return (error);
}

static int
sareservereleaseunit(struct cam_periph *periph, int reserve)
{
	union ccb *ccb;
	struct sa_softc *softc;
	int error;

	softc = (struct sa_softc *)periph->softc;
	ccb = cam_periph_getccb(periph,  1);

	/* It is safe to retry this operation */
	scsi_reserve_release_unit(&ccb->csio, 2, sadone, MSG_SIMPLE_Q_TAG,
	    FALSE,  0, SSD_FULL_SIZE,  SCSIOP_TIMEOUT, reserve);
	softc->dsreg = MTIO_DSREG_RBSY;
	error = cam_periph_runccb(ccb, saerror, 0,
	    SF_RETRY_UA | SF_NO_PRINT, softc->device_stats);
	softc->dsreg = MTIO_DSREG_REST;
	QFRLS(ccb);
	xpt_release_ccb(ccb);

	/*
	 * If the error was Illegal Request, then the device doesn't support
	 * RESERVE/RELEASE. This is not an error.
	 */
	if (error == EINVAL) {
		error = 0;
	}

	return (error);
}

static int
saloadunload(struct cam_periph *periph, int load)
{
	union	ccb *ccb;
	struct	sa_softc *softc;
	int	error;

	softc = (struct sa_softc *)periph->softc;

	ccb = cam_periph_getccb(periph, 1);

	/* It is safe to retry this operation */
	scsi_load_unload(&ccb->csio, 5, sadone, MSG_SIMPLE_Q_TAG, FALSE,
	    FALSE, FALSE, load, SSD_FULL_SIZE, REWIND_TIMEOUT);

	softc->dsreg = (load)? MTIO_DSREG_LD : MTIO_DSREG_UNL;
	error = cam_periph_runccb(ccb, saerror, 0, 0, softc->device_stats);
	softc->dsreg = MTIO_DSREG_REST;
	QFRLS(ccb);
	xpt_release_ccb(ccb);

	if (error || load == 0)
		softc->fileno = softc->blkno = (daddr_t) -1;
	else if (error == 0)
		softc->fileno = softc->blkno = (daddr_t) 0;
	return (error);
}

static int
saerase(struct cam_periph *periph, int longerase)
{

	union	ccb *ccb;
	struct	sa_softc *softc;
	int error;

	softc = (struct sa_softc *)periph->softc;
	if (softc->open_rdonly)
		return (EBADF);

	ccb = cam_periph_getccb(periph, 1);

	scsi_erase(&ccb->csio, 1, sadone, MSG_SIMPLE_Q_TAG, FALSE, longerase,
	    SSD_FULL_SIZE, ERASE_TIMEOUT);

	softc->dsreg = MTIO_DSREG_ZER;
	error = cam_periph_runccb(ccb, saerror, 0, 0, softc->device_stats);
	softc->dsreg = MTIO_DSREG_REST;

	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path, 0, 0, 0, FALSE);
	xpt_release_ccb(ccb);
	return (error);
}

#endif /* _KERNEL */

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

	cam_fill_csio(csio, retries, cbfcnp, CAM_DIR_IN, tag_action,
	     (u_int8_t *)rlimit_buf, sizeof(*rlimit_buf), sense_len,
	     sizeof(*scsi_cmd), timeout);

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

	cam_fill_csio(csio, retries, cbfcnp, readop ? CAM_DIR_IN : CAM_DIR_OUT,
	    tag_action, data_ptr, dxfer_len, sense_len,
	    sizeof(*scsi_cmd), timeout);
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

	cam_fill_csio(csio, retries, cbfcnp, CAM_DIR_NONE, tag_action,
	    NULL, 0, sense_len, sizeof(*scsi_cmd), timeout);	
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
	
	cam_fill_csio(csio, retries, cbfcnp, CAM_DIR_NONE, tag_action, NULL,
	    0, sense_len, sizeof(*scsi_cmd), timeout);
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

	cam_fill_csio(csio, retries, cbfcnp, CAM_DIR_NONE, tag_action, NULL,
	    0, sense_len, sizeof(*scsi_cmd), timeout);
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

	cam_fill_csio(csio, retries, cbfcnp, CAM_DIR_NONE, tag_action, NULL,
	    0, sense_len, sizeof(*scsi_cmd), timeout);
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

	cam_fill_csio(csio, retries, cbfcnp, CAM_DIR_NONE, tag_action, NULL,
	    0, sense_len, sizeof(*scsi_cmd), timeout);
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

	cam_fill_csio(csio, retries, cbfcnp, CAM_DIR_NONE, tag_action, NULL,
	    0, sense_len, sizeof(*scsi_cmd), timeout);
}

/*
 * Read Tape Position command.
 */
void
scsi_read_position(struct ccb_scsiio *csio, u_int32_t retries,
		   void (*cbfcnp)(struct cam_periph *, union ccb *),
		   u_int8_t tag_action, int hardsoft,
		   struct scsi_tape_position_data *sbp,
		   u_int8_t sense_len, u_int32_t timeout)
{
	struct scsi_tape_read_position *scmd;

	cam_fill_csio(csio, retries, cbfcnp, CAM_DIR_IN, tag_action,
	    (u_int8_t *)sbp, sizeof (*sbp), sense_len, sizeof(*scmd), timeout);
	scmd = (struct scsi_tape_read_position *)&csio->cdb_io.cdb_bytes;
	bzero(scmd, sizeof(*scmd));
	scmd->opcode = READ_POSITION;
	scmd->byte1 = hardsoft;
}

/*
 * Set Tape Position command.
 */
void
scsi_set_position(struct ccb_scsiio *csio, u_int32_t retries,
		   void (*cbfcnp)(struct cam_periph *, union ccb *),
		   u_int8_t tag_action, int hardsoft, u_int32_t blkno,
		   u_int8_t sense_len, u_int32_t timeout)
{
	struct scsi_tape_locate *scmd;

	cam_fill_csio(csio, retries, cbfcnp, CAM_DIR_NONE, tag_action,
	    (u_int8_t *)NULL, 0, sense_len, sizeof(*scmd), timeout);
	scmd = (struct scsi_tape_locate *)&csio->cdb_io.cdb_bytes;
	bzero(scmd, sizeof(*scmd));
	scmd->opcode = LOCATE;
	if (hardsoft)
		scmd->byte1 |= SA_SPOS_BT;
	scsi_ulto4b(blkno, scmd->blkaddr);
}
