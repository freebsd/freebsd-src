/*
 * worm: Write Once device driver
 *
 * Copyright (C) 1995, HD Associates, Inc.
 * PO Box 276
 * Pepperell, MA 01463
 * 508 433 5266
 * dufault@hda.com
 *
 * Copyright (C) 1996-97 interface business GmbH
 *   Naumannstr. 1
 *   D-01309 Dresden
 *   F.R. Germany
 * <joerg_wunsch@interface-business.de>
 *
 * This code is contributed to the University of California at Berkeley:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      $Id: worm.c,v 1.49 1997/12/20 23:03:49 joerg Exp $
 */

#include "opt_bounce.h"
#include "opt_scsi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/cdio.h>
#include <sys/wormio.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/
#include <scsi/scsiconf.h>
#include <scsi/scsi_debug.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsi_driver.h>
#include <scsi/scsi_worm.h>
#include <scsi/scsi_cd.h>
#include <sys/dkstat.h>

struct worm_quirks
{
	/*
	 * The device-specific functions that need to be called during
	 * the several steps.
	 */
	errval	(*prepare_disk)(struct scsi_link *, int dummy, int speed);
	errval	(*prepare_track)(struct scsi_link *, struct wormio_prepare_track *t);
	errval	(*finalize_track)(struct scsi_link *);
	errval	(*finalize_disk)(struct scsi_link *, int toc_type, int onp);
	errval	(*write_session)(struct scsi_link *, struct wormio_write_session *);
};


struct scsi_data
{
	struct buf_queue_head buf_queue;
	int dkunit;		/* disk stats unit number */
	u_int32_t blk_size;	/* Size of each blocks */
#ifdef	DEVFS
	void	*b_devfs_token;
	void	*c_devfs_token;
	void	*ctl_devfs_token;
#endif

	struct worm_quirks *quirks; /* model-specific functions */
	struct wormio_prepare_track preptrack; /* scratch region */

	u_int8_t dummy;		/* use dummy writes */
	u_int8_t speed;		/* select drive speed */

	u_int32_t worm_flags;	/* driver-internal flags */
#define WORMFL_DISK_PREPED	0x01 /* disk parameters have been spec'ed */
#define WORMFL_TRACK_PREPED	0x02 /* track parameters have been sent */
#define WORMFL_WRITTEN		0x04 /* track has been written */
#define WORMFL_IOCTL_ONLY	0x08 /* O_NDELAY, only ioctls allowed */
#define WORMFL_TRACK_PREP  	0x10 /* track parameters have been spec'ed */

        int error;              /* last error */
};

struct {
    int asc;
    int devmode;
    int error;
    int ret;
} worm_error[] = {
    {0x24, WORM_Q_PLASMON|WORM_Q_PHILIPS, WORM_ABSORPTION_CONTROL_ERROR, 0},
    {0xb0, WORM_Q_PLASMON|WORM_Q_PHILIPS, WORM_CALIBRATION_AREA_ALMOST_FULL, 0},
    {0xb4, WORM_Q_PLASMON|WORM_Q_PHILIPS, WORM_CALIBRATION_AREA_FULL, SCSIRET_CONTINUE},
    {0xb5, WORM_Q_PLASMON|WORM_Q_PHILIPS, WORM_DUMMY_BLOCKS_ADDED, 0},
    {0xaa, WORM_Q_PLASMON|WORM_Q_PHILIPS, WORM_END_OF_MEDIUM, SCSIRET_CONTINUE},
    {0xad, WORM_Q_PLASMON|WORM_Q_PHILIPS, WORM_BUFFER_UNDERRUN, SCSIRET_CONTINUE},
    {0xaf, WORM_Q_PLASMON|WORM_Q_PHILIPS, WORM_OPTIMUM_POWER_CALIBRATION_ERROR, SCSIRET_CONTINUE},
    {0, 0, 0, 0}
};

static void wormstart(u_int32_t unit, u_int32_t flags);

static errval worm_open(dev_t dev, int flags, int fmt, struct proc *p,
			struct scsi_link *sc_link);
static errval worm_ioctl(dev_t dev, int cmd, caddr_t addr, int flag,
			 struct proc *p, struct scsi_link *sc_link);
static errval worm_close(dev_t dev, int flag, int fmt, struct proc *p,
			 struct scsi_link *sc_link);
static void worm_strategy(struct buf *bp, struct scsi_link *sc_link);
static errval worm_read_toc(struct scsi_link *sc_link,
			    u_int32_t mode, u_int32_t start,
			    struct cd_toc_entry *data, u_int32_t len);
static errval worm_rezero_unit(struct scsi_link *sc_link);
static errval worm_read_session_info(struct scsi_link *, struct wormio_session_info *);
static int worm_sense_handler(struct scsi_xfer *);
static errval worm_set_blksize(struct scsi_link *sc_link, int size);

/* XXX should be moved out to an LKM */
static errval rf4100_prepare_disk(struct scsi_link *, int dummy, int speed);
static errval rf4100_prepare_track(struct scsi_link *, struct wormio_prepare_track *);
static errval rf4100_finalize_track(struct scsi_link *);
static errval rf4100_finalize_disk(struct scsi_link *, int toc_type, int onp);

static errval hp4020i_prepare_disk(struct scsi_link *, int dummy, int speed);
static errval hp4020i_prepare_track(struct scsi_link *, struct  wormio_prepare_track *);
static errval hp4020i_finalize_track(struct scsi_link *);
static errval hp4020i_finalize_disk(struct scsi_link *, int toc_type, int onp);
static errval hp4020i_write_session(struct scsi_link *, struct wormio_write_session *);

static worm_devsw_installed = 0;

static	d_open_t	wormopen;
static	d_close_t	wormclose;
static	d_ioctl_t	wormioctl;
static	d_strategy_t	wormstrategy;

#define CDEV_MAJOR 62
#define BDEV_MAJOR 23
static struct cdevsw worm_cdevsw;
static struct bdevsw worm_bdevsw = 
	{ wormopen,	wormclose,	wormstrategy,	wormioctl,	/*23*/
	  nodump,	nopsize,	D_DISK,	"worm",	&worm_cdevsw,	-1 };


static int
wormunit(dev_t dev)
{
	return (minor(dev) & ~(SCSI_FIXED_MASK|SCSI_CONTROL_MASK));
}

SCSI_DEVICE_ENTRIES(worm)

static struct scsi_device worm_switch =
{
	worm_sense_handler,
	wormstart,   /* we have a queue, and this is how we service it */
	NULL,
	NULL,
	"worm",
	0,
	{0, 0},
	SDEV_ONCE_ONLY,	/* Only one open allowed */
	wormattach,
	"Write-Once",
	wormopen,
	sizeof(struct scsi_data),
	T_WORM,
	wormunit,
	0,
	worm_open,
	worm_ioctl,
	worm_close,
	worm_strategy,
};

static struct worm_quirks worm_quirks_plasmon = {
    rf4100_prepare_disk, rf4100_prepare_track,
    rf4100_finalize_track, rf4100_finalize_disk,
    0
};
static struct worm_quirks worm_quirks_philips = {
    hp4020i_prepare_disk, hp4020i_prepare_track,
    hp4020i_finalize_track, hp4020i_finalize_disk,
    hp4020i_write_session
};

static inline void
worm_registerdev(int unit)
{
	if (dk_ndrive < DK_NDRIVE) {
		sprintf(dk_names[dk_ndrive], "worm%d", unit);
		dk_wpms[dk_ndrive] = (1*1024*1024/2);	/* 1MB/sec XXX - fake! */
		SCSI_DATA(&worm_switch, unit)->dkunit = dk_ndrive++;
	} else {
		SCSI_DATA(&worm_switch, unit)->dkunit = -1;
	}
}

static errval
worm_size(struct scsi_link *sc_link, int flags)
{
	errval ret;
	struct scsi_data *worm = sc_link->sd;
	int blk_size;
	u_int32_t n_blks;

	SC_DEBUG(sc_link, SDEV_DB2, ("worm_size"));

	n_blks = scsi_read_capacity(sc_link, &blk_size, flags);

	/*
	 * CD-R devices can assume various sizes, depending on the
	 * intended purpose of the track.  Hence, READ CAPACITY
	 * doesn't give us any good results.  We make a more educated
	 * guess when it comes to prepare a track.
	 */

	if (n_blks > 0) {
		sc_link->flags |= SDEV_MEDIA_LOADED;
		ret = 0;
	} else {
		sc_link->flags &= ~SDEV_MEDIA_LOADED;
		ret = ENXIO;
	}

	return ret;
}

static errval
wormattach(struct scsi_link *sc_link)
{
	u_int32_t unit = sc_link->dev_unit;
#ifdef DEVFS
	int mynor;
#endif
	struct scsi_data *worm = sc_link->sd;

	if (sc_link->devmodes == 0)
		printf(", warning: unknown drive type", unit);

	bufq_init(&worm->buf_queue);

#ifdef DEVFS
	mynor = wormunit(sc_link->dev);
	worm->b_devfs_token =
		devfs_add_devswf(&worm_bdevsw, mynor,
				 DV_BLK, 0, 0, 0444, "worm%d", mynor);
	worm->c_devfs_token =
		devfs_add_devswf(&worm_cdevsw, mynor,
				 DV_CHR, 0, 0, 0644, "rworm%d", mynor);
	worm->ctl_devfs_token =
		devfs_add_devswf(&worm_cdevsw, mynor | SCSI_CONTROL_MASK,
				 DV_CHR, 0, 0, 0600, "rworm%d.ctl", mynor);
#endif
	worm_registerdev(unit);
	return 0;
}

/*
 * wormstart looks to see if there is a buf waiting for the device
 * and that the device is not already busy. If both are true,
 * It dequeues the buf and creates a scsi command to perform the
 * transfer required. The transfer request will call scsi_done
 * on completion, which will in turn call this routine again
 * so that the next queued transfer is performed.
 * The bufs are queued by the strategy routine (wormstrategy)
 *
 * This routine is also called after other non-queued requests
 * have been made of the scsi driver, to ensure that the queue
 * continues to be drained.
 * wormstart() is called at splbio
 *
 * XXX It looks like we need a "scsistart" to hoist common code up
 * into.  In particular, the removable media checking should be
 * handled in one place.
 *
 * Writes will fail if the disk and track not been prepared via the control
 * device.
 */
static void
wormstart(unit, flags)
	u_int32_t	unit;
	u_int32_t flags;
{
	struct scsi_link *sc_link = SCSI_LINK(&worm_switch, unit);
	struct scsi_data *worm = sc_link->sd;
	register struct buf *bp = 0;
	struct scsi_rw_big cmd;

	u_int32_t lba;  /* Logical block address */
	u_int32_t tl;		/* Transfer length */

	SC_DEBUG(sc_link, SDEV_DB2, ("wormstart "));

	/*
	 * We should reject all queued entries if SDEV_MEDIA_LOADED is not true.
	 */
	if (!(sc_link->flags & SDEV_MEDIA_LOADED)) {
		goto badnews;	/* no I/O.. media changed or something */
	}

	/*
	 * See if there is a buf to do and we are not already
	 * doing one
	 */
	while (sc_link->opennings != 0) {

		/* if a special awaits, let it proceed first */
		if (sc_link->flags & SDEV_WAITING) {
			sc_link->flags &= ~SDEV_WAITING;
			wakeup(sc_link);
			return;
		}

		bp = bufq_first(&worm->buf_queue);
		if (bp == NULL)
			return;

		bufq_remove(&worm->buf_queue, bp);

		if ((bp->b_flags & B_READ) == B_WRITE) {
		    if ((worm->worm_flags & WORMFL_TRACK_PREPED) == 0) {
			if ((worm->worm_flags & WORMFL_TRACK_PREP) == 0) {
			    SC_DEBUG(sc_link, SDEV_DB3, ("sequence error\n"));
			    bp->b_error = EIO;
			    bp->b_flags |= B_ERROR;
			    worm->error = WORM_SEQUENCE_ERROR;
			    biodone(bp);
			    goto badnews;
			} else {
			    if (worm->quirks->prepare_track(sc_link, &worm->preptrack)
				!= 0) {
				biodone(bp);
				goto badnews;
			    }
			    worm->worm_flags |= WORMFL_TRACK_PREPED;
			}
		    }
		}
		/*
		 *  Fill out the scsi command
		 */
		bzero(&cmd, sizeof(cmd));
		if ((bp->b_flags & B_READ) == B_WRITE) {
			cmd.op_code = WRITE_BIG;
			flags |= SCSI_DATA_OUT;
		} else {
			cmd.op_code = READ_BIG;
			flags |= SCSI_DATA_IN;
		}

		worm->error = 0;

                lba = bp->b_blkno / (worm->blk_size / DEV_BSIZE);
		tl = bp->b_bcount / worm->blk_size;

		if (bp->b_flags & B_READ)
		    /*
		     * Leave the LBA as 0 for write operations, it
		     * is reserved in this case (and wouldn't make
		     * any sense to set it at all, since CD-R write
		     * operations are in `streaming' mode anyway.
		     */
		    scsi_uto4b(lba, &cmd.addr_3);
		scsi_uto2b(tl, &cmd.length2);

		/*
		 * go ask the adapter to do all this for us
		 */
		if (scsi_scsi_cmd(sc_link,
			(struct scsi_generic *) &cmd,
			sizeof(cmd),
			(u_char *) bp->b_data,
			bp->b_bcount,
			0,
			100000,
			bp,
			flags | SCSI_NOSLEEP) == SUCCESSFULLY_QUEUED) {
			if (worm->dkunit >= 0) {	/* Cloned from od.c, possibly with same mistakes. :) */
				dk_xfer[worm->dkunit]++;
				dk_seek[worm->dkunit] = 1; /* single track */
				dk_wds[worm->dkunit] += bp->b_bcount >> 6;
			}
			if ((bp->b_flags & B_READ) == B_WRITE)
				worm->worm_flags |= WORMFL_WRITTEN;
		} else {
			printf("worm%ld: oops not queued\n", unit);
			if (bp) {
				bp->b_flags |= B_ERROR;
				bp->b_error = EIO;
				biodone(bp);
			}
		}
	} /* go back and see if we can cram more work in.. */
badnews: ;
}

static void
worm_strategy(struct buf *bp, struct scsi_link *sc_link)
{
	unsigned char unit;
	u_int32_t opri;
	struct scsi_data *worm;

	unit = wormunit(bp->b_dev);
	worm = sc_link->sd;

	if ((worm->worm_flags & WORMFL_IOCTL_ONLY) != 0) {
		SC_DEBUG(sc_link, SDEV_DB3,
			 ("attempted IO on ioctl-only descriptor\n"));
		bp->b_error = EBADF;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}

	/*
	 * check it's not too big a transfer for our adapter
	 */
        wormminphys(bp);

	opri = splbio();

	/*
	 * Use a bounce buffer if necessary
	 * XXX: How can we move this up?
	 */
#ifdef BOUNCE_BUFFERS
	if (sc_link->flags & SDEV_BOUNCE)
		vm_bounce_alloc(bp);
#endif

	/*
	 * Place it in the queue of activities for this device
	 * at the end.
	 */
	bufq_insert_tail(&worm->buf_queue, bp);

	wormstart(unit, 0);

	splx(opri);
	return;
}

/*
 * Open the device.
 * Only called for the "real" device, not for the control device.
 */
static int
worm_open(dev_t dev, int flags, int fmt, struct proc *p,
	  struct scsi_link *sc_link)
{
	struct scsi_data *worm;
	errval error;

	error = 0;
	worm = sc_link->sd;

	if (sc_link->flags & SDEV_OPEN)
		return EBUSY;

	/*
	 * Unknown drive type: only the control device can be opened
	 * in this case, so scsi(8) and things like cdrecord will
	 * work.
	 */
	if (sc_link->devmodes == 0)
		return ENXIO;

	/*
	 * Check that it is still responding and ok.
	 * if the media has been changed this will result in a
	 * "unit attention" error which the error code will
	 * disregard because the SDEV_OPEN flag is not yet set
	 *
	 * XXX This should REALLY be hoisted up.  As soon as Bruce
	 * finishes that slice stuff. (Add a different flag,
	 * and then do a "scsi_test_unit_ready" with the "ignore
	 * unit attention" thing set.  Then all this replicated
	 * test unit ready code can be pulled up.
	 */
	scsi_test_unit_ready(sc_link, SCSI_SILENT);

	/*
	 * The semantics of the "flags" is as follows:
	 *
	 * If the device has been opened with O_NONBLOCK set, no
	 * actual IO will be allowed, and the command sequence is only
	 * subject to the restrictions as in worm_ioctl() below.
	 */
	/*
	 * Next time actually take notice of error returns,
	 * unit attn errors are now errors.
	 */
	sc_link->flags |= SDEV_OPEN;

	if (scsi_test_unit_ready(sc_link, SCSI_SILENT) != 0) {
		SC_DEBUG(sc_link, SDEV_DB3, ("not ready\n"));
		error = ENXIO;
		goto out;
	}

	if ((flags & O_NONBLOCK) == 0) {
		scsi_start_unit(sc_link, SCSI_SILENT);
		scsi_prevent(sc_link, PR_PREVENT, SCSI_SILENT);

		if((flags & FWRITE) != 0) {
			if ((error = worm_rezero_unit(sc_link)) != 0 ||
			    (error = worm_size(sc_link, 0)) != 0) {
				SC_DEBUG(sc_link, SDEV_DB3,
					 ("rezero, or get size failed\n"));
				error = EIO;
				goto out;
			}
		} else {
			/* read/only */
			if ((error = worm_size(sc_link, 0)) != 0) {
				SC_DEBUG(sc_link, SDEV_DB3,
					 ("get size failed\n"));
				error = EIO;
				goto out;
			}
			worm->blk_size = 2048;
		}
	} else
		worm->worm_flags |= WORMFL_IOCTL_ONLY;

	switch (*(int *) sc_link->devmodes) {
	case WORM_Q_PLASMON:
	    worm->quirks = &worm_quirks_plasmon;
	    break;
	case WORM_Q_PHILIPS:
	    worm->quirks = &worm_quirks_philips;
	    break;
	default:
	    error = ENXIO; 
	}
	worm->error = 0;

  out:
	if (error) {
	    scsi_prevent(sc_link, PR_ALLOW, SCSI_SILENT);
	    worm->worm_flags &= ~(WORMFL_TRACK_PREPED| WORMFL_TRACK_PREP);
	    sc_link->flags &= ~SDEV_OPEN;
	}

	return error;
}

static int
worm_close(dev_t dev, int flags, int fmt, struct proc *p,
	   struct scsi_link *sc_link)
{
	struct scsi_data *worm = sc_link->sd;
	errval error;

	error = 0;

	if ((worm->worm_flags & WORMFL_IOCTL_ONLY) == 0) {
		if ((flags & FWRITE) != 0) {
		    worm->error = 0;
		    if ((worm->worm_flags & WORMFL_TRACK_PREPED) != 0) {
			error = (worm->quirks->finalize_track)(sc_link);
		    	worm->worm_flags &=
			    ~(WORMFL_TRACK_PREPED | WORMFL_TRACK_PREP);
		    }
		}
		scsi_prevent(sc_link, PR_ALLOW, SCSI_SILENT);
	} else
	    worm->worm_flags &= ~WORMFL_IOCTL_ONLY;

	sc_link->flags &= ~SDEV_OPEN;

	return error;
}

/*
 * Perform special action on behalf of the user.
 * Knows about the internals of this device
 */
errval
worm_ioctl(dev_t dev, int cmd, caddr_t addr, int flag, struct proc *p,
	   struct scsi_link *sc_link)
{
	errval  error = 0;
	u_int8_t  unit;
	register struct scsi_data *worm;

	/*
	 * Find the device that the user is talking about
	 */
	unit = wormunit(dev);
	worm = sc_link->sd;
	SC_DEBUG(sc_link, SDEV_DB2, ("wormioctl 0x%x ", cmd));

	switch (cmd) {
	case WORMIOCPREPDISK:
	{
	    struct wormio_prepare_disk *w =
		(struct wormio_prepare_disk *)addr;
	    if (w->dummy != 0 && w->dummy != 1)
		error = EINVAL;
	    else {
		error = (worm->quirks->prepare_disk)
		    (sc_link, w->dummy, w->speed);
		if (error == 0) {
		    worm->worm_flags |= WORMFL_DISK_PREPED;
		    worm->dummy = w->dummy;
		    worm->speed = w->speed;
		}
	    }
	}
	break;

	case WORMIOCPREPTRACK:
	{
	    struct wormio_prepare_track *w =
		(struct wormio_prepare_track *)addr;
	    if (w->audio != 0 && w->audio != 1)
		error = EINVAL;
	    else if (w->audio == 0 && w->preemp)
		error = EINVAL;
	    else if ((worm->worm_flags & WORMFL_DISK_PREPED)==0) {
		error = EINVAL;
		worm->error = WORM_SEQUENCE_ERROR;
	    } else {
		/*
		 * This sets a flag only.  Actual preparation of a
		 * track will be deferred up to the first write.
		 */
		worm->worm_flags |= WORMFL_TRACK_PREP;
		worm->preptrack = *w;
	    }
	}
	break;

	case WORMIOCFINISHTRACK:
	    if ((worm->worm_flags & WORMFL_TRACK_PREPED) != 0)
		error = (worm->quirks->finalize_track)(sc_link);
	    worm->worm_flags &= ~(WORMFL_TRACK_PREPED | WORMFL_TRACK_PREP);
	    break;

	case WORMIOCFIXATION:
	{
	    struct wormio_fixation *w =
		(struct wormio_fixation *)addr;
	    
	    if ((worm->worm_flags & WORMFL_WRITTEN) == 0)
		error = EINVAL;
	    else if (w->toc_type < WORM_TOC_TYPE_AUDIO ||
		     w->toc_type > WORM_TOC_TYPE_CDI)
		error = EINVAL;
	    else if (w->onp != 0 && w->onp != 1)
		error = EINVAL;
	    else {
		worm->worm_flags = 0;
		/* no fixation needed if dummy write */
		if (worm->dummy == 0) {
		    worm->error = 0;
		    error = (worm->quirks->finalize_disk)
			(sc_link, w->toc_type, w->onp);
		}
	    }
	}
	break;

	case WORMIOCREADSESSIONINFO:
	{
	    struct wormio_session_info si;
	    error = worm_read_session_info(sc_link, &si);
	    if (error)
		break;
	    NTOHS(si.lead_in);
	    NTOHS(si.lead_out);
	    bcopy(&si, addr, sizeof si);
	}
	break;

	case WORMIOCWRITESESSION:
	    if (worm->quirks->write_session) {
		error = (worm->quirks->write_session)
		    (sc_link, (struct wormio_write_session *) addr);
		if (!error)
		    worm->worm_flags |=	WORMFL_TRACK_PREPED | WORMFL_TRACK_PREP;
	    } else
		error = ENXIO;
	    break;

	case WORMIOERROR:
            bcopy(&(worm->error), addr, sizeof (int));
	    break;

	case CDIOREADTOCHEADER:
		{
			struct ioc_toc_header th;
			error = worm_read_toc(sc_link, 0, 0,
					      (struct cd_toc_entry *)&th,
					      sizeof th);
			if (error)
				break;
			NTOHS(th.len);
			bcopy(&th, addr, sizeof th);
		}
		break;

	case CDIOREADTOCENTRYS:
		{
			struct {
				struct ioc_toc_header header;
				struct cd_toc_entry entries[100];
			} data;
			struct {
				struct ioc_toc_header header;
				struct cd_toc_entry entry;
			} lead;
			struct ioc_read_toc_entry *te =
				(struct ioc_read_toc_entry *) addr;
			struct ioc_toc_header *th;
			u_int32_t len, readlen, idx, num;
			u_int32_t starting_track = te->starting_track;

			if (te->data_len < sizeof(struct cd_toc_entry)
			    || (te->data_len % sizeof(struct cd_toc_entry)) != 0
			    || te->address_format != CD_MSF_FORMAT
			    && te->address_format != CD_LBA_FORMAT) {
				error = EINVAL;
				break;
			}

			th = &data.header;
			error = worm_read_toc(sc_link, 0, 0,
					      (struct cd_toc_entry *)th,
					      sizeof (*th));
			if (error)
				break;

			if (starting_track == 0)
				starting_track = th->starting_track;
			else if (starting_track == 0xaa)
				starting_track = th->ending_track + 1;
			else if (starting_track < th->starting_track ||
				 starting_track > th->ending_track + 1) {
				error = EINVAL;
				break;
			}

			/* calculate reading length without leadout entry */
			readlen = (th->ending_track - starting_track + 1) *
				  sizeof(struct cd_toc_entry);

			/* and with leadout entry */
			len = readlen + sizeof(struct cd_toc_entry);
			if (te->data_len < len) {
				len = te->data_len;
				if (readlen > len)
					readlen = len;
			}
			if (len > sizeof(data.entries)) {
				error = EINVAL;
				break;
			}
			num = len / sizeof(struct cd_toc_entry);

			if (readlen > 0) {
				error = worm_read_toc(sc_link,
						      te->address_format,
						      starting_track,
						      (struct cd_toc_entry *)&data,
						      readlen + sizeof (*th));
				if (error)
					break;
			}

			/* make leadout entry if needed */
			idx = starting_track + num - 1;
			if (idx == th->ending_track + 1) {
				error = worm_read_toc(sc_link,
						      te->address_format, 0xaa,
						      (struct cd_toc_entry *)&lead,
						      sizeof(lead));
				if (error)
					break;
				data.entries[idx - starting_track] = lead.entry;
			}

			error = copyout(data.entries, te->data, len);
		}
		break;

	case CDIOREADTOCENTRY:
		{
			struct {
				struct ioc_toc_header header;
				struct cd_toc_entry entry;
			} data;
			struct ioc_read_toc_single_entry *te =
				(struct ioc_read_toc_single_entry *) addr;
			struct ioc_toc_header *th;
			u_int32_t track;

			if (te->address_format != CD_MSF_FORMAT
			    && te->address_format != CD_LBA_FORMAT) {
				error = EINVAL;
				break;
			}

			th = &data.header;
			error = worm_read_toc(sc_link, 0, 0,
					      (struct cd_toc_entry *)th,
					      sizeof (*th));
			if (error)
				break;

			track = te->track;
			if (track == 0)
				track = th->starting_track;
			else if (track == 0xaa)
				/* OK */;
			else if (track < th->starting_track ||
				 track > th->ending_track + 1) {
				error = EINVAL;
				break;
			}

			error = worm_read_toc(sc_link, te->address_format,
					      track,
					      (struct cd_toc_entry *)&data,
					      sizeof data);
			if (error)
				break;

			bcopy(&data.entry, &te->entry,
			      sizeof(struct cd_toc_entry));
		}
		break;
		
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}


static errval
worm_rezero_unit(struct scsi_link *sc_link)
{
	struct scsi_rezero_unit cmd;
	
	SC_DEBUG(sc_link, SDEV_DB2, ("worm_rezero_unit"));

	/*
	 * Re-initialize the unit, just to be sure.
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.op_code = REZERO_UNIT;
	return scsi_scsi_cmd(sc_link,
			     (struct scsi_generic *) &cmd,
			     sizeof(cmd),
			     0,	/* no data transfer */
			     0,
			     /*WORMRETRY*/ 4,
			     5000,
			     NULL,
			     0);
}

static errval
worm_read_session_info(struct scsi_link *sc_link, struct wormio_session_info *data)
{
	struct scsi_read_session_info cmd;
	
	SC_DEBUG(sc_link, SDEV_DB2, ("worm_read_session_info"));

	bzero(&cmd, sizeof(cmd));
	cmd.op_code = READ_SESSION_INFO;
	cmd.transfer_length = sizeof(struct wormio_session_info);
	return scsi_scsi_cmd(sc_link,
			     (struct scsi_generic *) &cmd,
			     sizeof(cmd),
			     (u_char *) data,
			     sizeof(struct wormio_session_info),
			     /*WORMRETRY*/ 4,
			     5000,
			     NULL,
			     SCSI_DATA_IN);
}

/*
 * Read table of contents
 *
 * Stolen from cd.c
 */
static errval
worm_read_toc(struct scsi_link *sc_link, u_int32_t mode, u_int32_t start,
	      struct cd_toc_entry *data, u_int32_t len)
{
	struct scsi_read_toc scsi_cmd;
	u_int32_t ntoc;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	ntoc = len;

	scsi_cmd.op_code = READ_TOC;
	if (mode == CD_MSF_FORMAT)
		scsi_cmd.byte2 |= CD_MSF;
	scsi_cmd.from_track = start;
	scsi_cmd.data_len[0] = (ntoc) >> 8;
	scsi_cmd.data_len[1] = (ntoc) & 0xff;
	return (scsi_scsi_cmd(sc_link,
			      (struct scsi_generic *) &scsi_cmd,
			      sizeof(struct scsi_read_toc),
			      (u_char *) data,
			      len,
			      /*WORMRETRY*/ 4,
			      5000,
			      NULL,
			      SCSI_DATA_IN));
}

static int
worm_sense_handler(struct scsi_xfer *xs)
{
    struct scsi_data *worm;
    struct scsi_sense_data *sense;
    struct scsi_sense_extended *ext;
    int asc, devmode, i;
   
    worm = xs->sc_link->sd;
    sense = &(xs->sense);
    ext = (struct scsi_sense_extended *) &(sense->ext.extended);
    asc = ext->add_sense_code;
    devmode = *(int *) xs->sc_link->devmodes;

    for (i = 0; worm_error[i].asc; i++)
	if ((asc == worm_error[i].asc) && (devmode & worm_error[i].devmode)) {
	    worm->error = worm_error[i].error;
	    return worm_error[i].ret;
	}
    worm->error = -1;
    return SCSIRET_CONTINUE;
}

static errval 
worm_set_blksize(struct scsi_link *sc_link, int size)
{
    struct scsi_mode_select scsi_cmd;
    struct {
	struct scsi_mode_header header;
	struct blk_desc desc;
    } dat;
    bzero(&scsi_cmd, sizeof(scsi_cmd));
    bzero(&dat, sizeof(dat));
    scsi_cmd.op_code = MODE_SELECT;
    scsi_cmd.length = sizeof(dat);
    dat.header.blk_desc_len = sizeof(struct blk_desc);
    scsi_uto3b(size, dat.desc.blklen);
    return scsi_scsi_cmd(sc_link,
			  (struct scsi_generic *) &scsi_cmd,
			  sizeof(scsi_cmd),
			  (u_char *) &dat,
			  sizeof(dat),
			  /*WORM_RETRIES*/ 4,
			  5000,
			  NULL,
			  SCSI_DATA_OUT);
}

static void
worm_drvinit(void *unused)
{

	if (!worm_devsw_installed) {
		bdevsw_add_generic(BDEV_MAJOR, CDEV_MAJOR, &worm_bdevsw);
		worm_devsw_installed = 1;
    	}
}

SYSINIT(wormdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,worm_drvinit,NULL)

/*
 * Begin device-specific stuff.  Subject to being moved out to LKMs.
 */

static u_char
ascii_to_6bit (char c)
{
    if (c < '0' || c > 'Z' || (c > '9' && c < 'A'))
	return 0;
    if (c <= '9')
	return c - '0';
    else
	return c - 'A' + 11;
}
/*
 * PLASMON RF4100/4102
 * Perhaps other Plasmon's, too.
 *
 * NB: By now, you'll certainly have to compare the SCSI reference
 * manual in order to understand the following.
 */

/* The following mode pages might apply to other drives as well. */

struct plasmon_rf4100_pages
{
	u_char	page_code;
#define RF4100_PAGE_CODE_20 0x20
#define RF4100_PAGE_CODE_21 0x21
#define RF4100_PAGE_CODE_22 0x22
#define RF4100_PAGE_CODE_23 0x23
#define RF4100_PAGE_CODE_24 0x24
#define RF4100_PAGE_CODE_25 0x25
	u_char	param_len;
	union
	{
		/* page 0x20 omitted by now */
		struct
		{
			u_char	reserved1;
			u_char	mode;
#define RF4100_RAW_MODE		0x10	/* raw mode enabled */
#define RF4100_MIXED_MODE	0x08	/* mixed mode data enabled */
#define RF4100_AUDIO_MODE	0x04	/* audio mode data enabled */
#define RF4100_MODE_1		0x01	/* mode 1 blocks are enabled */
#define RF4100_MODE_2		0x02	/* mode 2 blocks are enabled */
			u_char	track_number;
			u_char	isrc_i1; /* country code, ASCII */
			u_char	isrc_i2;
			u_char	isrc_i3; /* owner code, ASCII */
			u_char	isrc_i4;
			u_char	isrc_i5;
			u_char	isrc_i6_7; /* country code, BCD */
			u_char	isrc_i8_9; /* serial number, BCD */
			u_char	isrc_i10_11;
			u_char	isrc_i12_0;
			u_char	reserved2[2];
		}
		page_0x21;
		/* mode page 0x22 omitted by now */
		struct
		{
			u_char	speed_select;
#define RF4100_SPEED_AUDIO	0x01
#define RF4100_SPEED_DOUBLE	0x02
			u_char	dummy_write;
#define RF4100_DUMMY_WRITE	0x01
			u_char	reserved[4];
		}
		page_0x23;
		/* pages 0x24 and 0x25 omitted by now */
	}
	pages;
};


static errval
rf4100_prepare_disk(struct scsi_link *sc_link, int dummy, int speed)
{
	struct scsi_mode_select scsi_cmd;
	struct {
		struct scsi_mode_header header;
		struct plasmon_rf4100_pages page;
	} dat;
	u_int32_t pagelen, dat_len;

	pagelen = sizeof(dat.page.pages.page_0x23) + PAGE_HEADERLEN;
	dat_len = sizeof(struct scsi_mode_header) + pagelen;

	SC_DEBUG(sc_link, SDEV_DB2, ("rf4100_prepare_disk"));

	if (speed != RF4100_SPEED_AUDIO && speed != RF4100_SPEED_DOUBLE)
		return EINVAL;

	/*
	 * Set up a mode page 0x23
	 */
	bzero(&dat, sizeof(dat));
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = MODE_SELECT;
	scsi_cmd.length = dat_len;
	/* dat.header.dev_spec = host application code; (see spec) */
	dat.page.page_code = RF4100_PAGE_CODE_23;
	dat.page.param_len = sizeof(dat.page.pages.page_0x23);
	dat.page.pages.page_0x23.speed_select = speed;
	dat.page.pages.page_0x23.dummy_write = dummy? RF4100_DUMMY_WRITE: 0;
	/*
	 * Fire it off.
	 */
	return scsi_scsi_cmd(sc_link,
			     (struct scsi_generic *) &scsi_cmd,
			     sizeof(scsi_cmd),
			     (u_char *) &dat,
			     dat_len,
			     /*WORM_RETRIES*/ 4,
			     5000,
			     NULL,
			     SCSI_DATA_OUT);
}


static errval
rf4100_prepare_track(struct scsi_link *sc_link, struct wormio_prepare_track *t)
{
	struct scsi_mode_select scsi_cmd;
	struct scsi_data *worm;
	struct {
		struct scsi_mode_header header;
		struct blk_desc blk_desc;
		struct plasmon_rf4100_pages page;
	} dat;
	u_int32_t pagelen, dat_len, blk_len;
	int year;

	worm = sc_link->sd;

	pagelen = sizeof(dat.page.pages.page_0x21) + PAGE_HEADERLEN;
	dat_len = sizeof(struct scsi_mode_header)
		+ sizeof(struct blk_desc)
		+ pagelen;

	SC_DEBUG(sc_link, SDEV_DB2, ("rf4100_prepare_track"));

	/*
	 * Set up a mode page 0x21.  Note that the block descriptor is
	 * mandatory in at least one of the MODE SELECT commands, in
	 * order to select the block length in question.  We do this
	 * here, just prior to opening the write channel.  (Spec:
	 * ``All information for the write is included in the MODE
	 * SELECT, MODE PAGE 21h, and the write channel can be
	 * considered open on receipt of the first WRITE command.''  I
	 * didn't have luck with an explicit WRITE TRACK command
	 * anyway, this might be different for other CD-R drives. -
	 * Jörg)
	 */
	bzero(&dat, sizeof(dat));
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = MODE_SELECT;
	scsi_cmd.length = dat_len;
	dat.header.blk_desc_len = sizeof(struct blk_desc);
	dat.page.page_code = RF4100_PAGE_CODE_21;
	dat.page.param_len = sizeof(dat.page.pages.page_0x21);
	/* dat.header.dev_spec = host application code; (see spec) */
	if (t->audio) {
	    blk_len = 2352;
	    dat.page.pages.page_0x21.mode = RF4100_AUDIO_MODE + 
		(t->preemp? RF4100_MODE_1 : 0);
	} else
	    switch (t->track_type) {
	    case BLOCK_RAW: 
		blk_len = 2352;
		dat.page.pages.page_0x21.mode = RF4100_RAW_MODE;
		break;
	    case BLOCK_MODE_1: 
		blk_len = 2048;
		dat.page.pages.page_0x21.mode = RF4100_MODE_1;
		break;
	    case BLOCK_MODE_2: 
		blk_len = 2336; 
		dat.page.pages.page_0x21.mode = RF4100_MODE_2;
		break;
	    case BLOCK_MODE_2_FORM_1:
		blk_len = 2048;
		dat.page.pages.page_0x21.mode = RF4100_MODE_2;
		break;
	    case BLOCK_MODE_2_FORM_1b:
		blk_len = 2056;
		dat.page.pages.page_0x21.mode = RF4100_MODE_2;
		break;
	    case BLOCK_MODE_2_FORM_2:
		blk_len = 2324;
		dat.page.pages.page_0x21.mode = RF4100_MODE_2;
		break;
	    case BLOCK_MODE_2_FORM_2b:
		blk_len = 2332;
		dat.page.pages.page_0x21.mode = RF4100_MODE_2;
		break;
	    default:
		return EINVAL;
	    }
	dat.page.pages.page_0x21.mode |= t->copy_bits << 5;
	
	worm->blk_size = blk_len;

	dat.page.pages.page_0x21.track_number = t->track_number;

	dat.page.pages.page_0x21.isrc_i1 = ascii_to_6bit(t->ISRC_country[0]);
	dat.page.pages.page_0x21.isrc_i2 = ascii_to_6bit(t->ISRC_country[1]);
	dat.page.pages.page_0x21.isrc_i3 = ascii_to_6bit(t->ISRC_owner[0]);
	dat.page.pages.page_0x21.isrc_i4 = ascii_to_6bit(t->ISRC_owner[1]);
	dat.page.pages.page_0x21.isrc_i5 = ascii_to_6bit(t->ISRC_owner[2]);
	year = t->ISRC_year > 1900 ? t->ISRC_year - 1900 : t->ISRC_year;
	if (year > 99 || year < 0)
	    return EINVAL;
	dat.page.pages.page_0x21.isrc_i6_7 = bin2bcd(year);
	if (t->ISRC_serial[0]) {
	    dat.page.pages.page_0x21.isrc_i8_9 = ((t->ISRC_serial[0]-'0') << 4) |
		(t->ISRC_serial[1] - '0');
	    dat.page.pages.page_0x21.isrc_i10_11 = ((t->ISRC_serial[2]-'0') << 4) |
		(t->ISRC_serial[3] - '0');
	    dat.page.pages.page_0x21.isrc_i12_0 =  (t->ISRC_serial[4] - '0' << 4);
	}
	scsi_uto3b(blk_len, dat.blk_desc.blklen);

	/*
	 * Fire it off.
	 */
	return scsi_scsi_cmd(sc_link,
			     (struct scsi_generic *) &scsi_cmd,
			     sizeof(scsi_cmd),
			     (u_char *) &dat,
			     dat_len,
			     /*WORM_RETRIES*/ 4,
			     5000,
			     NULL,
			     SCSI_DATA_OUT);
}


static errval
rf4100_finalize_track(struct scsi_link *sc_link)
{
	struct scsi_synchronize_cache cmd;
	int error;

	SC_DEBUG(sc_link, SDEV_DB2, ("rf4100_finalize_track"));

	/*
	 * Only a "synchronize cache" is needed.
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.op_code = SYNCHRONIZE_CACHE;
	error = scsi_scsi_cmd(sc_link,
			     (struct scsi_generic *) &cmd,
			     sizeof(cmd),
			     0,	/* no data transfer */
			     0,
			     1,
			     60000, /* this may take a while */
			     NULL,
			     0);
	if (!error) 
	    error = worm_set_blksize(sc_link, 2048);

	return error;
}


static errval
rf4100_finalize_disk(struct scsi_link *sc_link, int toc_type, int onp)
{
	struct scsi_fixation cmd;

	SC_DEBUG(sc_link, SDEV_DB2, ("rf4100_finalize_disk"));

	if (toc_type < 0 || toc_type > WORM_TOC_TYPE_CDI)
		return EINVAL;

	/*
	 * Fixate this session.  Mark the next one as opened if onp
	 * is true.  Otherwise, the disk will be finalized once and
	 * for all.  ONP stands for "open next program area".
	 */
	
	bzero(&cmd, sizeof(cmd));
	cmd.op_code = FIXATION;
	cmd.action = (onp? WORM_FIXATION_ONP: 0) + toc_type;
	return scsi_scsi_cmd(sc_link,
			     (struct scsi_generic *) &cmd,
			     sizeof(cmd),
			     0,	/* no data transfer */
			     0,
			     1,
			     20*60*1000, /* takes a huge amount of time */
			     NULL,
			     0);
}

/*
 * End Plasmon RF4100/4102 section.
 */

/*
 * HP C4324/C4325 (This is what the scsi spec. and firmware says) 
 * Drive model 4020i
 *  This is very similar to the Plasmon above.
 */

/* The following mode pages might apply to other drives as well. */

struct hp_4020i_pages
{
	u_char	page_code;
#define HP4020I_PAGE_CODE_20 0x20
#define HP4020I_PAGE_CODE_21 0x21
#define HP4020I_PAGE_CODE_22 0x22
#define HP4020I_PAGE_CODE_23 0x23
#define HP4020I_PAGE_CODE_24 0x24
#define HP4020I_PAGE_CODE_25 0x25
	u_char	param_len;
	union
	{
		/* page 0x20 omitted by now */
		struct
		{
			u_char	reserved1;
			u_char	mode;
#define HP4020I_RAW_MODE		0x10	/* raw mode enabled */
#define HP4020I_MIXED_MODE	0x08	/* mixed mode data enabled */
#define HP4020I_AUDIO_MODE	0x04	/* audio mode data enabled */
#define HP4020I_MODE_1		0x01	/* mode 1 blocks are enabled */
#define HP4020I_MODE_2		0x02	/* mode 2 blocks are enabled */
			u_char	track_number;
			u_char	isrc_i1; /* country code, ASCII */
			u_char	isrc_i2;
			u_char	isrc_i3; /* owner code, ASCII */
			u_char	isrc_i4;
			u_char	isrc_i5;
			u_char	isrc_i6_7; /* year code, BCD */
			u_char	isrc_i8_9; /* serial number, BCD */
			u_char	isrc_i10_11;
			u_char	isrc_i12_0;
			u_char	reserved2[2];
		}
		page_0x21;
		struct
		{
			u_char	catalog_valid;
			u_char	catalog_c1_c2; /* catalog number, BCD */
			u_char	catalog_c3_c4;
			u_char	catalog_c5_c6;
			u_char	catalog_c7_c8;
			u_char	catalog_c9_c10;
			u_char	catalog_c11_c12;
			u_char	catalog_c13_0;
		} page_0x22;
		struct
		{
			u_char	speed_select;
#define HP4020I_SPEED_AUDIO	0x01
#define HP4020I_SPEED_DOUBLE	0x02
			u_char	dummy_write;
#define HP4020I_DUMMY_WRITE	0x01
			u_char	reserved[4];
		}
		page_0x23;
		/* pages 0x24 and 0x25 omitted by now */
	}
	pages;
};

static errval
hp4020i_prepare_disk(struct scsi_link *sc_link, int dummy, int speed)
{
	struct scsi_mode_select scsi_cmd;
	struct {
		struct scsi_mode_header header;
		struct hp_4020i_pages page;
	} dat;
	u_int32_t pagelen, dat_len;

	pagelen = sizeof(dat.page.pages.page_0x23) + PAGE_HEADERLEN;
	dat_len = sizeof(struct scsi_mode_header) + pagelen;

	SC_DEBUG(sc_link, SDEV_DB2, ("hp4020i_prepare_disk"));

	if (speed != HP4020I_SPEED_AUDIO && speed != HP4020I_SPEED_DOUBLE)
		return EINVAL;

	/*
	 * Set up a mode page 0x23
	 */
	bzero(&dat, sizeof(dat));
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = MODE_SELECT;
	scsi_cmd.byte2 |= SMS_PF;
	scsi_cmd.length = dat_len;
	/* dat.header.dev_spec = host application code; (see spec) */
	dat.page.page_code = HP4020I_PAGE_CODE_23;
	dat.page.param_len = sizeof(dat.page.pages.page_0x23);
	dat.page.pages.page_0x23.speed_select = speed;
	dat.page.pages.page_0x23.dummy_write = dummy? HP4020I_DUMMY_WRITE: 0;
	/*
	 * Fire it off.
	 */
	return scsi_scsi_cmd(sc_link,
			     (struct scsi_generic *) &scsi_cmd,
			     sizeof(scsi_cmd),
			     (u_char *) &dat,
			     dat_len,
			     /*WORM_RETRIES*/ 4,
			     5000,
			     NULL,
			     SCSI_DATA_OUT);
}


static errval
hp4020i_prepare_track(struct scsi_link *sc_link, struct wormio_prepare_track *t)
{
	struct scsi_mode_select scsi_cmd;
	struct scsi_data *worm;
	struct {
		struct scsi_mode_header header;
		struct blk_desc blk_desc;
		struct hp_4020i_pages page;
	} dat;
	u_int32_t pagelen, dat_len, blk_len;
	int year;

	worm = sc_link->sd;

	pagelen = sizeof(dat.page.pages.page_0x21) + PAGE_HEADERLEN;
	dat_len = sizeof(struct scsi_mode_header)
		+ sizeof(struct blk_desc)
		+ pagelen;

	SC_DEBUG(sc_link, SDEV_DB2, ("hp4020i_prepare_track"));


	/*
	 * Set up a mode page 0x21.  Note that the block descriptor is
	 * mandatory in at least one of the MODE SELECT commands, in
	 * order to select the block length in question.  We do this
	 * here, just prior to opening the write channel.  (Spec:
	 * ``All information for the write is included in the MODE
	 * SELECT, MODE PAGE 21h, and the write channel can be
	 * considered open on receipt of the first WRITE command.''  I
	 * didn't have luck with an explicit WRITE TRACK command
	 * anyway, this might be different for other CD-R drives. -
	 * Jörg)
	 */
	bzero(&dat, sizeof(dat));
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = MODE_SELECT;
	scsi_cmd.byte2 |= SMS_PF;
	scsi_cmd.length = dat_len;
	dat.header.blk_desc_len = sizeof(struct blk_desc);
	dat.page.page_code = HP4020I_PAGE_CODE_21;
	dat.page.param_len = sizeof(dat.page.pages.page_0x21);
	/* dat.header.dev_spec = host application code; (see spec) */
	if (t->audio) {
	    blk_len = 2352;
	    dat.page.pages.page_0x21.mode = HP4020I_AUDIO_MODE + 
		(t->preemp? HP4020I_MODE_1 : 0);
	} else
	    switch (t->track_type) {
	    case BLOCK_RAW: 
		blk_len = 2352;
		dat.page.pages.page_0x21.mode = HP4020I_RAW_MODE;
		break;
	    case BLOCK_MODE_1: 
		blk_len = 2048;
		dat.page.pages.page_0x21.mode = HP4020I_MODE_1;
		break;
	    case BLOCK_MODE_2: 
		blk_len = 2336; 
		dat.page.pages.page_0x21.mode = HP4020I_MODE_2;
		break;
	    case BLOCK_MODE_2_FORM_1:
		blk_len = 2048;
		dat.page.pages.page_0x21.mode = HP4020I_MODE_2;
		break;
	    case BLOCK_MODE_2_FORM_1b:
		blk_len = 2056;
		dat.page.pages.page_0x21.mode = HP4020I_MODE_2;
		break;
	    case BLOCK_MODE_2_FORM_2:
		blk_len = 2324;
		dat.page.pages.page_0x21.mode = HP4020I_MODE_2;
		break;
	    case BLOCK_MODE_2_FORM_2b:
		blk_len = 2332;
		dat.page.pages.page_0x21.mode = HP4020I_MODE_2;
		break;
	    default:
		return EINVAL;
	    }
	dat.page.pages.page_0x21.mode |= t->copy_bits << 5;
	
	worm->blk_size = blk_len;

	dat.page.pages.page_0x21.track_number = t->track_number;

	dat.page.pages.page_0x21.isrc_i1 = ascii_to_6bit(t->ISRC_country[0]);
	dat.page.pages.page_0x21.isrc_i2 = ascii_to_6bit(t->ISRC_country[1]);
	dat.page.pages.page_0x21.isrc_i3 = ascii_to_6bit(t->ISRC_owner[0]);
	dat.page.pages.page_0x21.isrc_i4 = ascii_to_6bit(t->ISRC_owner[1]);
	dat.page.pages.page_0x21.isrc_i5 = ascii_to_6bit(t->ISRC_owner[2]);
	year = t->ISRC_year > 1900 ? t->ISRC_year - 1900 : t->ISRC_year;
	if (year > 99 || year < 0)
	    return EINVAL;
	dat.page.pages.page_0x21.isrc_i6_7 = bin2bcd(year);
	if (t->ISRC_serial[0]) {
	    dat.page.pages.page_0x21.isrc_i8_9 = ((t->ISRC_serial[0]-'0') << 4) |
		(t->ISRC_serial[1] - '0');
	    dat.page.pages.page_0x21.isrc_i10_11 = ((t->ISRC_serial[2]-'0') << 4) |
		(t->ISRC_serial[3] - '0');
	    dat.page.pages.page_0x21.isrc_i12_0 =  (t->ISRC_serial[4] - '0' << 4);
	}

	scsi_uto3b(blk_len, dat.blk_desc.blklen);
	
	/*
	 * Fire it off.
	 */
	return scsi_scsi_cmd(sc_link,
			     (struct scsi_generic *) &scsi_cmd,
			     sizeof(scsi_cmd),
			     (u_char *) &dat,
			     dat_len,
			     /*WORM_RETRIES*/ 4,
			     5000,
			     NULL,
			     SCSI_DATA_OUT);
}


static errval
hp4020i_finalize_track(struct scsi_link *sc_link)
{
	struct scsi_synchronize_cache cmd;
	int error;

	SC_DEBUG(sc_link, SDEV_DB2, ("hp4020i_finalize_track"));

	/*
	 * Only a "synchronize cache" is needed.
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.op_code = SYNCHRONIZE_CACHE;
	error = scsi_scsi_cmd(sc_link,
			     (struct scsi_generic *) &cmd,
			     sizeof(cmd),
			     0,	/* no data transfer */
			     0,
			     1,
			     60000, /* this may take a while */
			     NULL,
			     0);
	if (!error) 
	    error = worm_set_blksize(sc_link, 2048);

	return error;
}


static errval
hp4020i_finalize_disk(struct scsi_link *sc_link, int toc_type, int onp)
{
	struct scsi_fixation cmd;

	SC_DEBUG(sc_link, SDEV_DB2, ("hp4020i_finalize_disk"));

	if (toc_type < 0 || toc_type > WORM_TOC_TYPE_CDI)
		return EINVAL;

	/*
	 * Fixate this session.  Mark the next one as opened if onp
	 * is true.  Otherwise, the disk will be finalized once and
	 * for all.  ONP stands for "open next program area".
	 */
	
	bzero(&cmd, sizeof(cmd));
	cmd.op_code = FIXATION;
	cmd.action = (onp? WORM_FIXATION_ONP: 0) + toc_type;
	return scsi_scsi_cmd(sc_link,
			     (struct scsi_generic *) &cmd,
			     sizeof(cmd),
			     0,	/* no data transfer */
			     0,
			     1,
			     20*60*1000, /* takes a huge amount of time */
			     NULL,
			     0);
}
static errval
hp4020i_write_session(struct scsi_link *sc_link, struct wormio_write_session *ws)
{
    struct {
	struct scsi_mode_header header;
	struct blk_desc blk_desc;
	struct hp_4020i_pages page;
	} dat;
    struct scsi_mode_select cmd1;
    struct scsi_write_session cmd2;
    struct scsi_data *worm;
    u_int32_t pagelen, dat_len, blk_len;
    errval error;

    SC_DEBUG(sc_link, SDEV_DB2, ("hp4020i_write_session"));

    if (ws->toc_type < 0 || ws->toc_type > WORM_TOC_TYPE_CDI || ws->lofp & ~3)
	return EINVAL;

    pagelen = sizeof(dat.page.pages.page_0x22) + PAGE_HEADERLEN;
    dat_len = sizeof(struct scsi_mode_header)
	+ sizeof(struct blk_desc)
	+ pagelen;

    worm = sc_link->sd;

    /* set the block size to 2352 and the catalog */
    bzero(&dat, sizeof(dat));
    bzero(&cmd1, sizeof(cmd1));
    cmd1.op_code = MODE_SELECT;
    cmd1.byte2 |= SMS_PF;
    cmd1.length = dat_len;
    dat.header.blk_desc_len = sizeof(struct blk_desc);
    dat.page.page_code = HP4020I_PAGE_CODE_22;
    dat.page.param_len = sizeof(dat.page.pages.page_0x22);
    blk_len = 2352;
    if (ws->catalog[0] >= '0' && ws->catalog[0] <= '9') {
	dat.page.pages.page_0x22.catalog_valid = 1;
	dat.page.pages.page_0x22.catalog_c1_c2 = ((ws->catalog[0]-'0') << 4)
	    | ((ws->catalog[1]-'0') << 4);
	dat.page.pages.page_0x22.catalog_c3_c4 = ((ws->catalog[2]-'0') << 4)
	    | ((ws->catalog[3]-'0') << 4);
	dat.page.pages.page_0x22.catalog_c5_c6 = ((ws->catalog[4]-'0') << 4)
	    | ((ws->catalog[5]-'0') << 4);
	dat.page.pages.page_0x22.catalog_c7_c8 = ((ws->catalog[6]-'0') << 4)
	    | ((ws->catalog[7]-'0') << 4);
	dat.page.pages.page_0x22.catalog_c9_c10 = ((ws->catalog[8]-'0') << 4)
	    | ((ws->catalog[9]-'0') << 4);
	dat.page.pages.page_0x22.catalog_c11_c12 = ((ws->catalog[10]-'0') << 4)
	    | ((ws->catalog[11]-'0') << 4);
	dat.page.pages.page_0x22.catalog_c13_0 = ((ws->catalog[12]-'0') << 4);
    }
    scsi_uto3b(blk_len, dat.blk_desc.blklen);
	
    error = scsi_scsi_cmd(sc_link,
			  (struct scsi_generic *) &cmd1,
			  sizeof(cmd1),
			  (u_char *) &dat,
			  dat_len,
			  /*WORM_RETRIES*/ 4,
			  5000,
			  NULL,
			  SCSI_DATA_OUT);

    if (!error) {
	worm->blk_size = blk_len;
	bzero(&cmd2, sizeof(cmd2));
	cmd2.op_code = WRITE_SESSION;
	cmd2.action = (ws->lofp << 4) | (ws->onp? WORM_FIXATION_ONP: 0) + ws->toc_type;
	scsi_uto2b(ws->length, &cmd2.transfer_length_2);

	error = scsi_scsi_cmd(sc_link,
			      (struct scsi_generic *) &cmd2,
			      sizeof(cmd2),
			      ws->track_desc,
			      ws->length,
			      1,
			      5000,
			      NULL,
			      SCSI_DATA_OUT);
    }
    return error;
}
    
/*
 * End HP C4324/C4325 (4020i) section.
 */
