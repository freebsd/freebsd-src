/*
 * worm: Write Once device driver
 *
 * Copyright (C) 1995, HD Associates, Inc.
 * PO Box 276
 * Pepperell, MA 01463
 * 508 433 5266
 * dufault@hda.com
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
 *      $Id: worm.c,v 1.7 1995/05/30 08:13:58 rgrimes Exp $
 */

/* XXX This is PRELIMINARY.
 *
 *     We need the "kern devconf" stuff, but I'm not
 *     going to add it until it is done in a simple way that provides
 *     base behavior in scsi_driver.c
 *
 *     Until Bruce finishes the slice stuff there will be no partitions.
 *     When it is finished I hope to hoist the partition code up into
 *     "scsi_driver" and use common code for all devices.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_disk.h>

struct scsi_data {
	struct buf *buf_queue;		/* the queue of pending IO operations */

	u_int32 n_blks;				/* Number of blocks (0 for bogus) */
	u_int32 blk_size;			/* Size of each blocks */
};

void wormstart(u_int32 unit, u_int32 flags);

errval worm_open(dev_t dev, int flags, int fmt, struct proc *p,
struct scsi_link *sc_link);
errval worm_ioctl(dev_t dev, int cmd, caddr_t addr, int flag,
		struct proc *p, struct scsi_link *sc_link);
errval worm_close(dev_t dev, int flag, int fmt, struct proc *p,
        struct scsi_link *sc_link);
void worm_strategy(struct buf *bp, struct scsi_link *sc_link);

SCSI_DEVICE_ENTRIES(worm)

struct scsi_device worm_switch =
{
    NULL,
    wormstart,			/* we have a queue, and this is how we service it */
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
	0,
	0,
	worm_open,
	0,
	worm_close,
	worm_strategy,
};

static int worm_size(struct scsi_link *sc_link, int flags)
{
	int ret;
	struct scsi_data *worm = sc_link->sd;

	worm->n_blks = scsi_read_capacity(sc_link, &worm->blk_size,
					  flags);

	if(worm->blk_size == 0)
		/* XXX */
		worm->blk_size = 2048;
	if (worm->n_blks)
	{
		sc_link->flags |= SDEV_MEDIA_LOADED;
		ret = 1;
	}
	else
	{
		sc_link->flags &= ~SDEV_MEDIA_LOADED;
		ret = 0;
	}

	return ret;
}

errval
wormattach(struct scsi_link *sc_link)
{
	struct scsi_data *worm = sc_link->sd;

	printf("- UNTESTED ");

	if (worm_size(sc_link, SCSI_NOSLEEP | SCSI_NOMASK) == 0)
		printf("- can't get capacity.");
	else
		printf("with %ld %ld byte blocks.", worm->n_blks, worm->blk_size);
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
void
wormstart(unit, flags)
	u_int32	unit;
	u_int32 flags;
{
	struct scsi_link *sc_link = SCSI_LINK(&worm_switch, unit);
	struct scsi_data *worm = sc_link->sd;
	register struct buf *bp = 0;
	struct
	{
		u_char	op_code;
		u_char	byte2;
		u_char	lba3;	/* I don't want to worry about packing */
		u_char	lba2;
		u_char	lba1;
		u_char	lba0;
		u_char	reserved;
		u_char  tl1;
		u_char  tl0;
		u_char	ctl;
	} cmd;

	u_int32 lba;	/* Logical block address */
	u_int32 tl;		/* Transfer length */

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
		if ((bp = worm->buf_queue) == NULL) {
			return;	/* no work to bother with */
		}
		worm->buf_queue = bp->b_actf;

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

		scsi_uto4b(lba, &cmd.lba3);
		scsi_uto2b(tl, &cmd.tl1);

		/*
		 * go ask the adapter to do all this for us
		 */
		if (scsi_scsi_cmd(sc_link,
			(struct scsi_generic *) &cmd,
			sizeof(cmd),
			(u_char *) bp->b_un.b_addr,
			bp->b_bcount,
			0,	/* can't retry a read on a tape really */
			100000,
			bp,
			flags | SCSI_NOSLEEP) == SUCCESSFULLY_QUEUED) {
		} else {
badnews:
			printf("worm%ld: oops not queued\n", unit);
			if (bp) {
				bp->b_flags |= B_ERROR;
				bp->b_error = EIO;
				biodone(bp);
			}
		}
	} /* go back and see if we can cram more work in.. */
}

void
worm_strategy(struct buf *bp, struct scsi_link *sc_link)
{
	struct buf **dp;
	unsigned char unit;
	u_int32 opri;
	struct scsi_data *worm;

	unit = minor((bp->b_dev));
	worm = sc_link->sd;

	/* XXX: Can't we move this check up to "scsi_strategy"?
	 */
	if (!(sc_link->flags & SDEV_MEDIA_LOADED) ||
	bp->b_blkno > worm->n_blks ||
	bp->b_bcount & (worm->blk_size - 1)) {
		bp->b_error = EIO;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}

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
	dp = &(worm->buf_queue);
	while (*dp) {
		dp = &((*dp)->b_actf);
	}
	*dp = bp;
	bp->b_actf = NULL;

	wormstart(unit, 0);

	splx(opri);
	return;
}

/*
 * Open the device.  XXX: I'm completely guessing at this sequence.
 */
int
worm_open(dev_t dev, int flags, int fmt, struct proc *p,
struct scsi_link *sc_link)
{
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
	 * Next time actually take notice of error returns
	 */
	sc_link->flags |= SDEV_OPEN;	/* unit attn errors are now errors */

	if (scsi_test_unit_ready(sc_link, SCSI_SILENT) != 0) {
		SC_DEBUG(sc_link, SDEV_DB3, ("not ready\n"));
		sc_link->flags &= ~SDEV_OPEN;
		return ENXIO;
	}

	scsi_start_unit(sc_link, SCSI_SILENT);

	scsi_prevent(sc_link, PR_PREVENT, SCSI_SILENT);

	if (worm_size(sc_link, 0) == 0) {
		scsi_stop_unit(sc_link, 0, SCSI_SILENT);
		scsi_prevent(sc_link, PR_ALLOW, SCSI_SILENT);
		sc_link->flags &= ~SDEV_OPEN;
		return ENXIO;
	}

	return 0;
}

int
worm_close(dev_t dev, int flag, int fmt, struct proc *p,
        struct scsi_link *sc_link)
{
	scsi_stop_unit(sc_link, 0, SCSI_SILENT);
	scsi_prevent(sc_link, PR_ALLOW, SCSI_SILENT);
	sc_link->flags &= ~SDEV_OPEN;

	return 0;
}
