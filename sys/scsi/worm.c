/*
 * worm: Write Once device driver
 *
 * Copyright (C) 1995, HD Associates, Inc.
 * PO Box 276
 * Pepperell, MA 01463
 * 508 433 5266
 * dufault@hda.com
 *
 * Copyright (C) 1996, interface business GmbH
 *   Tolkewitzer Str. 49
 *   D-01277 Dresden
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
 *      $Id: worm.c,v 1.29.2.2 1997/03/05 13:56:28 joerg Exp $
 */

#include "opt_bounce.h"
#include "opt_scsi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/wormio.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsi_worm.h>
#include <sys/dkstat.h>
/* #include <scsi/scsi_cd.h> */ /* XXX a CD-R includes all CD functionality */

struct worm_quirks
{
	/*
	 * Vendor and model are used for comparision; the model may be
	 * abbreviated (or could even be empty at all).
	 */
	const char *vendor;
	const char *model;
	/*
	 * The device-specific functions that need to be called during
	 * the several steps.
	 */
	errval	(*prepare_disk)(struct scsi_link *, int dummy, int speed);
	errval	(*prepare_track)(struct scsi_link *, int audio, int preemp);
	errval	(*finalize_track)(struct scsi_link *);
	errval	(*finalize_disk)(struct scsi_link *, int toc_type, int onp);
};


struct scsi_data
{
	struct buf_queue_head buf_queue;
	int dkunit;		/* disk stats unit number */
	u_int32_t n_blks;		/* Number of blocks (0 for bogus) */
	u_int32_t blk_size;	/* Size of each blocks */
#ifdef	DEVFS
	void	*b_devfs_token;
	void	*c_devfs_token;
	void	*ctl_devfs_token;
#endif

	struct worm_quirks *quirks; /* model-specific functions */

	u_int8_t dummy;		/* use dummy writes */
	u_int8_t speed;		/* select drive speed */
	u_int8_t audio;		/* write audio data */
	u_int8_t preemp;		/* audio only: use preemphasis */

	u_int32_t worm_flags;	/* driver-internal flags */
#define WORMFL_DISK_PREPED	0x01 /* disk parameters have been spec'ed */
#define WORMFL_TRACK_PREPED	0x02 /* track parameters have been spec'ed */
#define WORMFL_WRITTEN		0x04 /* track has been written */
#define WORMFL_IOCTL_ONLY	0x08 /* O_NDELAY, only ioctls allowed */
};

static void wormstart(u_int32_t unit, u_int32_t flags);

static errval worm_open(dev_t dev, int flags, int fmt, struct proc *p,
			struct scsi_link *sc_link);
static errval worm_ioctl(dev_t dev, int cmd, caddr_t addr, int flag,
			 struct proc *p, struct scsi_link *sc_link);
static errval worm_close(dev_t dev, int flag, int fmt, struct proc *p,
			 struct scsi_link *sc_link);
static void worm_strategy(struct buf *bp, struct scsi_link *sc_link);

static errval worm_quirk_select(struct scsi_link *sc_link, u_int32_t unit,
				struct wormio_quirk_select *);
static errval worm_rezero_unit(struct scsi_link *sc_link);

/* XXX should be moved out to an LKM */
static errval rf4100_prepare_disk(struct scsi_link *, int dummy, int speed);
static errval rf4100_prepare_track(struct scsi_link *, int audio, int preemp);
static errval rf4100_finalize_track(struct scsi_link *);
static errval rf4100_finalize_disk(struct scsi_link *, int toc_type, int onp);

static errval hp4020i_prepare_disk(struct scsi_link *, int dummy, int speed);
static errval hp4020i_prepare_track(struct scsi_link *, int audio, int preemp);
static errval hp4020i_finalize_track(struct scsi_link *);
static errval hp4020i_finalize_disk(struct scsi_link *, int toc_type, int onp);

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
	  nodump,	nopsize,	0,	"worm",	&worm_cdevsw,	-1 };


static int
wormunit(dev_t dev)
{
	return (minor(dev) & ~(SCSI_FIXED_MASK|SCSI_CONTROL_MASK));
}

SCSI_DEVICE_ENTRIES(worm)

static struct scsi_device worm_switch =
{
	NULL,
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

/* XXX This should become the registration table for the LKMs. */
struct worm_quirks worm_quirks[] = {
	{
		"PLASMON", "RF410",
		rf4100_prepare_disk, rf4100_prepare_track,
		rf4100_finalize_track, rf4100_finalize_disk
	},
	{
		"HP", "4020i",
		hp4020i_prepare_disk, hp4020i_prepare_track,
		hp4020i_finalize_track, hp4020i_finalize_disk
	},
	{
		"PHILIPS", "CDD2000",
		hp4020i_prepare_disk, hp4020i_prepare_track,
		hp4020i_finalize_track, hp4020i_finalize_disk
	},
	{0}
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

	SC_DEBUG(sc_link, SDEV_DB2, ("worm_size"));

	worm->n_blks = scsi_read_capacity(sc_link, &worm->blk_size,
					  flags);

	/*
	 * CD-R devices can assume various sizes, depending on the
	 * intended purpose of the track.  Hence, READ CAPACITY
	 * doesn't give us any good results.  Make a more educated
	 * guess instead.
	 */
	worm->blk_size = (worm->audio? 2352: 2048);

	if (worm->n_blks)
	{
		sc_link->flags |= SDEV_MEDIA_LOADED;
		ret = 0;
	}
	else
	{
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

	TAILQ_INIT(&worm->buf_queue);

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

	u_int32_t lba;	/* Logical block address */
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

		bp = worm->buf_queue.tqh_first;
		if (bp == NULL) {	/* yes, an assign */
			return;
		}
		TAILQ_REMOVE( &worm->buf_queue, bp, b_act);

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


		lba = bp->b_blkno / (worm->blk_size / DEV_BSIZE);
		tl = bp->b_bcount / worm->blk_size;

		scsi_uto4b(lba, &cmd.addr_3);
		scsi_uto2b(tl, &cmd.length2);

		/*
		 * go ask the adapter to do all this for us
		 */
		if (scsi_scsi_cmd(sc_link,
			(struct scsi_generic *) &cmd,
			sizeof(cmd),
			(u_char *) bp->b_un.b_addr,
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
badnews:
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
	 * The ugly modulo operation is necessary since audio tracks
	 * have a block size of 2352 bytes.
	 */
	if (!(sc_link->flags & SDEV_MEDIA_LOADED) ||
	    bp->b_blkno * DEV_BSIZE > worm->n_blks * worm->blk_size||
	    (bp->b_bcount % worm->blk_size) != 0) {
		SC_DEBUG(sc_link, SDEV_DB3,
			 ("worm block size / capacity error, "
			  "b_blkno = %d, n_blks = %d, blk_size = %d, "
			  "b_bcount = %d\n",
			  bp->b_blkno, worm->n_blks, worm->blk_size,
			  bp->b_bcount) );
		bp->b_error = EIO;
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
	TAILQ_INSERT_TAIL(&worm->buf_queue, bp, b_act);

	wormstart(unit, 0);

	splx(opri);
	return;
}

/*
 * Open the device.
 * Only called for the "real" device, not for the control device.
 * Will fail if the disk and track not been prepared via the control
 * device.
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
	 *
	 * If the device is to be opened with O_RDWR/O_WRONLY, the
	 * disk and track must have been prepared accordingly by
	 * preceding ioctls (on an O_NONBLOCK descriptor for the device),
	 * or a sequence error will result here.
	 */
	if ((flags & FWRITE) != 0 &&
	    (worm->worm_flags & WORMFL_TRACK_PREPED) == 0) {
		SC_DEBUG(sc_link, SDEV_DB3, ("sequence error\n"));
		return ENXIO;
	}

	/*
	 * Next time actually take notice of error returns,
	 * unit attn errors are now errors.
	 */
	sc_link->flags |= SDEV_OPEN;

	if (scsi_test_unit_ready(sc_link, SCSI_SILENT) != 0) {
		SC_DEBUG(sc_link, SDEV_DB3, ("not ready\n"));
		if ((flags & FWRITE) != 0)
			worm->worm_flags &= ~WORMFL_TRACK_PREPED;
		sc_link->flags &= ~SDEV_OPEN;
		return ENXIO;
	}

	if ((flags & O_NONBLOCK) == 0) {
		scsi_start_unit(sc_link, SCSI_SILENT);
		scsi_prevent(sc_link, PR_PREVENT, SCSI_SILENT);

		if((flags & FWRITE) != 0) {
			if ((error = worm_rezero_unit(sc_link)) != 0 ||
			    (error = worm_size(sc_link, 0)) != 0 ||
			    (error = (worm->quirks->prepare_track)
			     (sc_link, worm->audio, worm->preemp)) != 0) {
				SC_DEBUG(sc_link, SDEV_DB3,
					 ("rezero, get size, or prepare_track failed\n"));
				scsi_stop_unit(sc_link, 0, SCSI_SILENT);
				scsi_prevent(sc_link, PR_ALLOW, SCSI_SILENT);
				worm->worm_flags &= ~WORMFL_TRACK_PREPED;
				sc_link->flags &= ~SDEV_OPEN;
			}
		} else {
			/* read/only */
			if ((error = worm_size(sc_link, 0)) != 0) {
				SC_DEBUG(sc_link, SDEV_DB3,
					 ("get size failed\n"));
				scsi_stop_unit(sc_link, 0, SCSI_SILENT);
				scsi_prevent(sc_link, PR_ALLOW, SCSI_SILENT);
				worm->worm_flags &= ~WORMFL_TRACK_PREPED;
				sc_link->flags &= ~SDEV_OPEN;
			}
		}
	} else
		worm->worm_flags |= WORMFL_IOCTL_ONLY;

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
		scsi_stop_unit(sc_link, 0, SCSI_SILENT);
		scsi_prevent(sc_link, PR_ALLOW, SCSI_SILENT);

		sc_link->flags &= ~SDEV_OPEN;

		if ((flags & FWRITE) != 0) {
			worm->worm_flags &= ~WORMFL_TRACK_PREPED;
			error = (worm->quirks->finalize_track)(sc_link);
		}
	}
	sc_link->flags &= ~SDEV_OPEN;
	worm->worm_flags &= ~WORMFL_IOCTL_ONLY;

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
	case WORMIOCQUIRKSELECT:
		error = worm_quirk_select(sc_link, unit,
					  (struct wormio_quirk_select *)addr);
		break;

	case WORMIOCPREPDISK:
		if (worm->quirks == 0)
			error = ENXIO;
		else {
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
		if (worm->quirks == 0)
			error = ENXIO;
		else {
			struct wormio_prepare_track *w =
				(struct wormio_prepare_track *)addr;
			if (w->audio != 0 && w->audio != 1)
				error = EINVAL;
			else if (w->audio == 0 && w->preemp)
				error = EINVAL;
			else if ((worm->worm_flags & WORMFL_DISK_PREPED)==0)
				error = EINVAL;
			else {
				worm->audio = w->audio;
				worm->preemp = w->preemp;
				worm->worm_flags |=
					WORMFL_TRACK_PREPED;
			}
		}
		break;

	case WORMIOCFIXATION:
		if (worm->quirks == 0)
			error = ENXIO;		
		else {
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
				if (worm->dummy == 0)
					error = (worm->quirks->finalize_disk)
						(sc_link, w->toc_type, w->onp);
			}
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
worm_quirk_select(struct scsi_link *sc_link, u_int32_t unit,
		  struct wormio_quirk_select *qs)
{
	struct worm_quirks *qp;
	struct scsi_data *worm = sc_link->sd;
	errval error = 0;
	
	SC_DEBUG(sc_link, SDEV_DB2, ("worm_quirk_select"));

	for (qp = worm_quirks; qp->vendor; qp++)
		if (strcmp(qp->vendor, qs->vendor) == 0 &&
		    strncmp(qp->model, qs->model, strlen(qp->model)) == 0)
			break;
	if (qp->vendor) {
		SC_DEBUG(sc_link, SDEV_DB3,
			 ("worm_quirk_select: %s %s",
			  qp->vendor, qp->model));
		worm->quirks = qp;
	}
	else
		error = EINVAL;

	return error;
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
#define RF4110_MODE_2		0x02	/* mode 2 blocks are enabled */
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
rf4100_prepare_track(struct scsi_link *sc_link, int audio, int preemp)
{
	struct scsi_mode_select scsi_cmd;
	struct {
		struct scsi_mode_header header;
		struct blk_desc blk_desc;
		struct plasmon_rf4100_pages page;
	} dat;
	u_int32_t pagelen, dat_len, blk_len;

	pagelen = sizeof(dat.page.pages.page_0x21) + PAGE_HEADERLEN;
	dat_len = sizeof(struct scsi_mode_header)
		+ sizeof(struct blk_desc)
		+ pagelen;

	SC_DEBUG(sc_link, SDEV_DB2, ("rf4100_prepare_track"));

	if (!audio && preemp)
		return EINVAL;

	/*
	 * By now, make a simple decision about the block length to be
	 * used.  It's just only Red Book (Audio) == 2352 bytes, or
	 * Yellow Book (CD-ROM) Mode 1 == 2048 bytes.
	 */
	blk_len = audio? 2352: 2048;

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
	/* dat.header.dev_spec = host application code; (see spec) */
	scsi_uto3b(blk_len, dat.blk_desc.blklen);
	dat.page.page_code = RF4100_PAGE_CODE_21;
	dat.page.param_len = sizeof(dat.page.pages.page_0x21);
	dat.page.pages.page_0x21.mode =
		(audio? RF4100_AUDIO_MODE: RF4100_MODE_1) +
		(preemp? RF4100_MODE_1: 0);
	/* dat.page.pages.page_0x21.track_number = 0; (current track) */
	
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
	
	SC_DEBUG(sc_link, SDEV_DB2, ("rf4100_finalize_track"));

	/*
	 * Only a "synchronize cache" is needed.
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.op_code = SYNCHRONIZE_CACHE;
	return scsi_scsi_cmd(sc_link,
			     (struct scsi_generic *) &cmd,
			     sizeof(cmd),
			     0,	/* no data transfer */
			     0,
			     1,
			     60000, /* this may take a while */
			     NULL,
			     0);
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
hp4020i_prepare_track(struct scsi_link *sc_link, int audio, int preemp)
{
	struct scsi_mode_select scsi_cmd;
	struct {
		struct scsi_mode_header header;
		struct blk_desc blk_desc;
		struct hp_4020i_pages page;
	} dat;
	u_int32_t pagelen, dat_len, blk_len;

	pagelen = sizeof(dat.page.pages.page_0x21) + PAGE_HEADERLEN;
	dat_len = sizeof(struct scsi_mode_header)
		+ sizeof(struct blk_desc)
		+ pagelen;

	SC_DEBUG(sc_link, SDEV_DB2, ("hp4020i_prepare_track"));

	if (!audio && preemp)
		return EINVAL;

	/*
	 * By now, make a simple decision about the block length to be
	 * used.  It's just only Red Book (Audio) == 2352 bytes, or
	 * Yellow Book (CD-ROM) Mode 1 == 2048 bytes.
	 */
	blk_len = audio? 2352: 2048;

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
	/* dat.header.dev_spec = host application code; (see spec) */
	scsi_uto3b(blk_len, dat.blk_desc.blklen);
	dat.page.page_code = HP4020I_PAGE_CODE_21;
	dat.page.param_len = sizeof(dat.page.pages.page_0x21);
	dat.page.pages.page_0x21.mode =
		(audio? HP4020I_AUDIO_MODE: HP4020I_MODE_1) +
		(preemp? HP4020I_MODE_1: 0);
	/* dat.page.pages.page_0x21.track_number = 0; (current track) */
	
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
	
	SC_DEBUG(sc_link, SDEV_DB2, ("hp4020i_finalize_track"));

	/*
	 * Only a "synchronize cache" is needed.
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.op_code = SYNCHRONIZE_CACHE;
	return scsi_scsi_cmd(sc_link,
			     (struct scsi_generic *) &cmd,
			     sizeof(cmd),
			     0,	/* no data transfer */
			     0,
			     1,
			     60000, /* this may take a while */
			     NULL,
			     0);
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

/*
 * End HP C4324/C4325 (4020i) section.
 */
