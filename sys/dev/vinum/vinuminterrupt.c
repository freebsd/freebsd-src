/* vinuminterrupt.c: bottom half of the driver */

/*-
 * Copyright (c) 1997, 1998, 1999
 *	Nan Yang Computer Services Limited.  All rights reserved.
 *
 *  Parts copyright (c) 1997, 1998 Cybernet Corporation, NetMAX project.
 *
 *  Written by Greg Lehey
 *
 *  This software is distributed under the so-called ``Berkeley
 *  License'':
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
 *	This product includes software developed by Nan Yang Computer
 *      Services Limited.
 * 4. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *
 * $Id: vinuminterrupt.c,v 1.14 2001/05/23 23:03:37 grog Exp grog $
 * $FreeBSD$
 */

#include <dev/vinum/vinumhdr.h>
#include <dev/vinum/request.h>
#include <sys/resourcevar.h>

void complete_raid5_write(struct rqelement *);
void complete_rqe(struct buf *bp);
void sdio_done(struct buf *bp);

/*
 * Take a completed buffer, transfer the data back if
 * it's a read, and complete the high-level request
 * if this is the last subrequest.
 *
 * The bp parameter is in fact a struct rqelement, which
 * includes a couple of extras at the end.
 */
void
complete_rqe(struct buf *bp)
{
    struct rqelement *rqe;
    struct request *rq;
    struct rqgroup *rqg;
    struct buf *ubp;					    /* user buffer */
    struct drive *drive;
    struct sd *sd;
    char *gravity;					    /* for error messages */

    rqe = (struct rqelement *) bp;			    /* point to the element element that completed */
    rqg = rqe->rqg;					    /* and the request group */
    rq = rqg->rq;					    /* and the complete request */
    ubp = rq->bp;					    /* user buffer */

#ifdef VINUMDEBUG
    if (debug & DEBUG_LASTREQS)
	logrq(loginfo_iodone, (union rqinfou) rqe, ubp);
#endif
    drive = &DRIVE[rqe->driveno];
    drive->active--;					    /* one less outstanding I/O on this drive */
    vinum_conf.active--;				    /* one less outstanding I/O globally */
    if ((drive->active == (DRIVE_MAXACTIVE - 1))	    /* we were at the drive limit */
    ||(vinum_conf.active == VINUM_MAXACTIVE))		    /* or the global limit */
	wakeup(&launch_requests);			    /* let another one at it */
    if ((bp->b_io.bio_flags & BIO_ERROR) != 0) {	    /* transfer in error */
	gravity = "";
	sd = &SD[rqe->sdno];

	if (bp->b_error != 0)				    /* did it return a number? */
	    rq->error = bp->b_error;			    /* yes, put it in. */
	else if (rq->error == 0)			    /* no: do we have one already? */
	    rq->error = EIO;				    /* no: catchall "I/O error" */
	sd->lasterror = rq->error;
	if (bp->b_iocmd == BIO_READ) {			    /* read operation */
	    if ((rq->error == ENXIO) || (sd->flags & VF_RETRYERRORS) == 0) {
		gravity = " fatal";
		set_sd_state(rqe->sdno, sd_crashed, setstate_force); /* subdisk is crashed */
	    }
	    log(LOG_ERR,
		"%s:%s read error, block %lld for %ld bytes\n",
		gravity,
		sd->name,
		bp->b_blkno,
		bp->b_bcount);
	} else {					    /* write operation */
	    if ((rq->error == ENXIO) || (sd->flags & VF_RETRYERRORS) == 0) {
		gravity = "fatal ";
		set_sd_state(rqe->sdno, sd_stale, setstate_force); /* subdisk is stale */
	    }
	    log(LOG_ERR,
		"%s:%s write error, block %lld for %ld bytes\n",
		gravity,
		sd->name,
		bp->b_blkno,
		bp->b_bcount);
	}
	log(LOG_ERR,
	    "%s: user buffer block %lld for %ld bytes\n",
	    sd->name,
	    ubp->b_blkno,
	    ubp->b_bcount);
	if (rq->error == ENXIO) {			    /* the drive's down too */
	    log(LOG_ERR,
		"%s: fatal drive I/O error, block %lld for %ld bytes\n",
		DRIVE[rqe->driveno].label.name,
		bp->b_blkno,
		bp->b_bcount);
	    DRIVE[rqe->driveno].lasterror = rq->error;
	    set_drive_state(rqe->driveno,		    /* take the drive down */
		drive_down,
		setstate_force);
	}
    }
    /* Now update the statistics */
    if (bp->b_iocmd == BIO_READ) {			    /* read operation */
	DRIVE[rqe->driveno].reads++;
	DRIVE[rqe->driveno].bytes_read += bp->b_bcount;
	SD[rqe->sdno].reads++;
	SD[rqe->sdno].bytes_read += bp->b_bcount;
	PLEX[rqe->rqg->plexno].reads++;
	PLEX[rqe->rqg->plexno].bytes_read += bp->b_bcount;
	if (PLEX[rqe->rqg->plexno].volno >= 0) {	    /* volume I/O, not plex */
	    VOL[PLEX[rqe->rqg->plexno].volno].reads++;
	    VOL[PLEX[rqe->rqg->plexno].volno].bytes_read += bp->b_bcount;
	}
    } else {						    /* write operation */
	DRIVE[rqe->driveno].writes++;
	DRIVE[rqe->driveno].bytes_written += bp->b_bcount;
	SD[rqe->sdno].writes++;
	SD[rqe->sdno].bytes_written += bp->b_bcount;
	PLEX[rqe->rqg->plexno].writes++;
	PLEX[rqe->rqg->plexno].bytes_written += bp->b_bcount;
	if (PLEX[rqe->rqg->plexno].volno >= 0) {	    /* volume I/O, not plex */
	    VOL[PLEX[rqe->rqg->plexno].volno].writes++;
	    VOL[PLEX[rqe->rqg->plexno].volno].bytes_written += bp->b_bcount;
	}
    }
    if (rqg->flags & XFR_RECOVERY_READ) {		    /* recovery read, */
	int *sdata;					    /* source */
	int *data;					    /* and group data */
	int length;					    /* and count involved */
	int count;					    /* loop counter */
	struct rqelement *urqe = &rqg->rqe[rqg->badsdno];   /* rqe of the bad subdisk */

	/* XOR destination is the user data */
	sdata = (int *) &rqe->b.b_data[rqe->groupoffset << DEV_BSHIFT];	/* old data contents */
	data = (int *) &urqe->b.b_data[urqe->groupoffset << DEV_BSHIFT]; /* destination */
	length = urqe->grouplen * (DEV_BSIZE / sizeof(int)); /* and number of ints */

	for (count = 0; count < length; count++)
	    data[count] ^= sdata[count];

	/*
	 * In a normal read, we will normally read directly
	 * into the user buffer.  This doesn't work if
	 * we're also doing a recovery, so we have to
	 * copy it
	 */
	if (rqe->flags & XFR_NORMAL_READ) {		    /* normal read as well, */
	    char *src = &rqe->b.b_data[rqe->dataoffset << DEV_BSHIFT]; /* read data is here */
	    char *dst;

	    dst = (char *) ubp->b_data + (rqe->useroffset << DEV_BSHIFT); /* where to put it in user buffer */
	    length = rqe->datalen << DEV_BSHIFT;	    /* and count involved */
	    bcopy(src, dst, length);			    /* move it */
	}
    } else if ((rqg->flags & (XFR_NORMAL_WRITE | XFR_DEGRADED_WRITE)) /* RAID 4/5 group write operation  */
    &&(rqg->active == 1))				    /* and this is the last active request */
	complete_raid5_write(rqe);
    /*
     * This is the earliest place where we can be
     * sure that the request has really finished,
     * since complete_raid5_write can issue new
     * requests.
     */
    rqg->active--;					    /* this request now finished */
    if (rqg->active == 0) {				    /* request group finished, */
	rq->active--;					    /* one less */
	if (rqg->lock) {				    /* got a lock? */
	    unlockrange(rqg->plexno, rqg->lock);	    /* yes, free it */
	    rqg->lock = 0;
	}
    }
    if (rq->active == 0) {				    /* request finished, */
#ifdef VINUMDEBUG
	if (debug & DEBUG_RESID) {
	    if (ubp->b_resid != 0)			    /* still something to transfer? */
		Debugger("resid");
	}
#endif

	if (rq->error) {				    /* did we have an error? */
	    if (rq->isplex) {				    /* plex operation, */
		ubp->b_io.bio_flags |= BIO_ERROR;	    /* yes, propagate to user */
		ubp->b_error = rq->error;
	    } else					    /* try to recover */
		queue_daemon_request(daemonrq_ioerror, (union daemoninfo) rq); /* let the daemon complete */
	} else {
	    ubp->b_resid = 0;				    /* completed our transfer */
	    if (rq->isplex == 0)			    /* volume request, */
		VOL[rq->volplex.volno].active--;	    /* another request finished */
	    if (rq->flags & XFR_COPYBUF) {
		Free(ubp->b_data);
		ubp->b_data = rq->save_data;
	    }
	    bufdone(ubp);				    /* top level buffer completed */
	    freerq(rq);					    /* return the request storage */
	}
    }
}

/* Free a request block and anything hanging off it */
void
freerq(struct request *rq)
{
    struct rqgroup *rqg;
    struct rqgroup *nrqg;				    /* next in chain */
    int rqno;

    for (rqg = rq->rqg; rqg != NULL; rqg = nrqg) {	    /* through the whole request chain */
	if (rqg->lock)					    /* got a lock? */
	    unlockrange(rqg->plexno, rqg->lock);	    /* yes, free it */
	for (rqno = 0; rqno < rqg->count; rqno++) {
	    if ((rqg->rqe[rqno].flags & XFR_MALLOCED)	    /* data buffer was malloced, */
	    &&rqg->rqe[rqno].b.b_data)			    /* and the allocation succeeded */
		Free(rqg->rqe[rqno].b.b_data);		    /* free it */
	    if (rqg->rqe[rqno].flags & XFR_BUFLOCKED) {	    /* locked this buffer, */
		BUF_UNLOCK(&rqg->rqe[rqno].b);		    /* unlock it again */
		BUF_LOCKFREE(&rqg->rqe[rqno].b);
	    }
	}
	nrqg = rqg->next;				    /* note the next one */
	Free(rqg);					    /* and free this one */
    }
    Free(rq);						    /* free the request itself */
}

/* I/O on subdisk completed */
void
sdio_done(struct buf *bp)
{
    struct sdbuf *sbp;

    sbp = (struct sdbuf *) bp;
    if (sbp->b.b_io.bio_flags & BIO_ERROR) {		    /* had an error */
	sbp->bp->b_io.bio_flags |= BIO_ERROR;		    /* propagate upwards */
	sbp->bp->b_error = sbp->b.b_error;
    }
#ifdef VINUMDEBUG
    if (debug & DEBUG_LASTREQS)
	logrq(loginfo_sdiodone, (union rqinfou) bp, bp);
#endif
    sbp->bp->b_resid = sbp->b.b_resid;			    /* copy the resid field */
    /* Now update the statistics */
    if (bp->b_iocmd == BIO_READ) {			    /* read operation */
	DRIVE[sbp->driveno].reads++;
	DRIVE[sbp->driveno].bytes_read += sbp->b.b_bcount;
	SD[sbp->sdno].reads++;
	SD[sbp->sdno].bytes_read += sbp->b.b_bcount;
    } else {						    /* write operation */
	DRIVE[sbp->driveno].writes++;
	DRIVE[sbp->driveno].bytes_written += sbp->b.b_bcount;
	SD[sbp->sdno].writes++;
	SD[sbp->sdno].bytes_written += sbp->b.b_bcount;
    }
    bufdone(sbp->bp);					    /* complete the caller's I/O */
    BUF_UNLOCK(&sbp->b);
    BUF_LOCKFREE(&sbp->b);
    Free(sbp);
}

/* Start the second phase of a RAID-4 or RAID-5 group write operation. */
void
complete_raid5_write(struct rqelement *rqe)
{
    int *sdata;						    /* source */
    int *pdata;						    /* and parity block data */
    int length;						    /* and count involved */
    int count;						    /* loop counter */
    int rqno;						    /* request index */
    int rqoffset;					    /* offset of request data from parity data */
    struct buf *ubp;					    /* user buffer header */
    struct request *rq;					    /* pointer to our request */
    struct rqgroup *rqg;				    /* and to the request group */
    struct rqelement *prqe;				    /* point to the parity block */
    struct drive *drive;				    /* drive to access */

    rqg = rqe->rqg;					    /* and to our request group */
    rq = rqg->rq;					    /* point to our request */
    ubp = rq->bp;					    /* user's buffer header */
    prqe = &rqg->rqe[0];				    /* point to the parity block */

    /*
     * If we get to this function, we have normal or
     * degraded writes, or a combination of both.  We do
     * the same thing in each case: we perform an
     * exclusive or to the parity block.  The only
     * difference is the origin of the data and the
     * address range.
     */
    if (rqe->flags & XFR_DEGRADED_WRITE) {		    /* do the degraded write stuff */
	pdata = (int *) (&prqe->b.b_data[(prqe->groupoffset) << DEV_BSHIFT]); /* parity data pointer */
	bzero(pdata, prqe->grouplen << DEV_BSHIFT);	    /* start with nothing in the parity block */

	/* Now get what data we need from each block */
	for (rqno = 1; rqno < rqg->count; rqno++) {	    /* for all the data blocks */
	    rqe = &rqg->rqe[rqno];			    /* this request */
	    sdata = (int *) (&rqe->b.b_data[rqe->groupoffset << DEV_BSHIFT]); /* old data */
	    length = rqe->grouplen << (DEV_BSHIFT - 2);	    /* and count involved */

	    /*
	     * Add the data block to the parity block.  Before
	     * we started the request, we zeroed the parity
	     * block, so the result of adding all the other
	     * blocks and the block we want to write will be
	     * the correct parity block.
	     */
	    for (count = 0; count < length; count++)
		pdata[count] ^= sdata[count];
	    if ((rqe->flags & XFR_MALLOCED)		    /* the buffer was malloced, */
	    &&((rqg->flags & XFR_NORMAL_WRITE) == 0)) {	    /* and we have no normal write, */
		Free(rqe->b.b_data);			    /* free it now */
		rqe->flags &= ~XFR_MALLOCED;
	    }
	}
    }
    if (rqg->flags & XFR_NORMAL_WRITE) {		    /* do normal write stuff */
	/* Get what data we need from each block */
	for (rqno = 1; rqno < rqg->count; rqno++) {	    /* for all the data blocks */
	    rqe = &rqg->rqe[rqno];			    /* this request */
	    if ((rqe->flags & (XFR_DATA_BLOCK | XFR_BAD_SUBDISK | XFR_NORMAL_WRITE))
		== (XFR_DATA_BLOCK | XFR_NORMAL_WRITE)) {   /* good data block to write */
		sdata = (int *) &rqe->b.b_data[rqe->dataoffset << DEV_BSHIFT]; /* old data contents */
		rqoffset = rqe->dataoffset + rqe->sdoffset - prqe->sdoffset; /* corresponding parity block offset */
		pdata = (int *) (&prqe->b.b_data[rqoffset << DEV_BSHIFT]); /* parity data pointer */
		length = rqe->datalen * (DEV_BSIZE / sizeof(int)); /* and number of ints */

		/*
		 * "remove" the old data block
		 * from the parity block
		 */
		if ((pdata < ((int *) prqe->b.b_data))
		    || (&pdata[length] > ((int *) (prqe->b.b_data + prqe->b.b_bcount)))
		    || (sdata < ((int *) rqe->b.b_data))
		    || (&sdata[length] > ((int *) (rqe->b.b_data + rqe->b.b_bcount))))
		    panic("complete_raid5_write: bounds overflow");
		for (count = 0; count < length; count++)
		    pdata[count] ^= sdata[count];

		/* "add" the new data block */
		sdata = (int *) (&ubp->b_data[rqe->useroffset << DEV_BSHIFT]); /* new data */
		if ((sdata < ((int *) ubp->b_data))
		    || (&sdata[length] > ((int *) (ubp->b_data + ubp->b_bcount))))
		    panic("complete_raid5_write: bounds overflow");
		for (count = 0; count < length; count++)
		    pdata[count] ^= sdata[count];

		/* Free the malloced buffer */
		if (rqe->flags & XFR_MALLOCED) {	    /* the buffer was malloced, */
		    Free(rqe->b.b_data);		    /* free it */
		    rqe->flags &= ~XFR_MALLOCED;
		} else
		    panic("complete_raid5_write: malloc conflict");

		if ((rqe->b.b_iocmd == BIO_READ)	    /* this was a read */
		&&((rqe->flags & XFR_BAD_SUBDISK) == 0)) {  /* and we can write this block */
		    rqe->b.b_flags &= ~B_DONE;		    /* start a new request */
		    rqe->b.b_iocmd = BIO_WRITE;		    /* we're writing now */
		    rqe->b.b_iodone = complete_rqe;	    /* call us here when done */
		    rqe->flags &= ~XFR_PARITYOP;	    /* reset flags that brought us here */
		    rqe->b.b_data = &ubp->b_data[rqe->useroffset << DEV_BSHIFT]; /* point to the user data */
		    rqe->b.b_bcount = rqe->datalen << DEV_BSHIFT; /* length to write */
		    rqe->b.b_bufsize = rqe->b.b_bcount;	    /* don't claim more */
		    rqe->b.b_resid = rqe->b.b_bcount;	    /* nothing transferred */
		    rqe->b.b_blkno += rqe->dataoffset;	    /* point to the correct block */
		    rqg->active++;			    /* another active request */
		    drive = &DRIVE[rqe->driveno];	    /* drive to access */

							    /* We can't sleep here, so we just increment the counters. */
		    drive->active++;
		    if (drive->active >= drive->maxactive)
			drive->maxactive = drive->active;
		    vinum_conf.active++;
		    if (vinum_conf.active >= vinum_conf.maxactive)
			vinum_conf.maxactive = vinum_conf.active;
#ifdef VINUMDEBUG
		    if (debug & DEBUG_ADDRESSES)
			log(LOG_DEBUG,
			    "  %s dev %d.%d, sd %d, offset 0x%x, devoffset 0x%llx, length %ld\n",
			    rqe->b.b_iocmd == BIO_READ ? "Read" : "Write",
			    major(rqe->b.b_dev),
			    minor(rqe->b.b_dev),
			    rqe->sdno,
			    (u_int) (rqe->b.b_blkno - SD[rqe->sdno].driveoffset),
			    rqe->b.b_blkno,
			    rqe->b.b_bcount);
		    if (debug & DEBUG_LASTREQS)
			logrq(loginfo_raid5_data, (union rqinfou) rqe, ubp);
#endif
		    DEV_STRATEGY(&rqe->b, 0);
		}
	    }
	}
    }
    /* Finally, write the parity block */
    rqe = &rqg->rqe[0];
    rqe->b.b_flags &= ~B_DONE;				    /* we're not done */
    rqe->b.b_iocmd = BIO_WRITE;				    /* we're writing now */
    rqe->b.b_iodone = complete_rqe;			    /* call us here when done */
    rqg->flags &= ~XFR_PARITYOP;			    /* reset flags that brought us here */
    rqe->b.b_bcount = rqe->buflen << DEV_BSHIFT;	    /* length to write */
    rqe->b.b_bufsize = rqe->b.b_bcount;			    /* don't claim we have more */
    rqe->b.b_resid = rqe->b.b_bcount;			    /* nothing transferred */
    rqg->active++;					    /* another active request */
    drive = &DRIVE[rqe->driveno];			    /* drive to access */

    /* We can't sleep here, so we just increment the counters. */
    drive->active++;
    if (drive->active >= drive->maxactive)
	drive->maxactive = drive->active;
    vinum_conf.active++;
    if (vinum_conf.active >= vinum_conf.maxactive)
	vinum_conf.maxactive = vinum_conf.active;

#ifdef VINUMDEBUG
    if (debug & DEBUG_ADDRESSES)
	log(LOG_DEBUG,
	   "  %s dev %d.%d, sd %d, offset 0x%x, devoffset 0x%llx, length %ld\n",
	    rqe->b.b_iocmd == BIO_READ ? "Read" : "Write",
	    major(rqe->b.b_dev),
	    minor(rqe->b.b_dev),
	    rqe->sdno,
	    (u_int) (rqe->b.b_blkno - SD[rqe->sdno].driveoffset),
	    rqe->b.b_blkno,
	    rqe->b.b_bcount);
    if (debug & DEBUG_LASTREQS)
	logrq(loginfo_raid5_parity, (union rqinfou) rqe, ubp);
#endif
    DEV_STRATEGY(&rqe->b, 0);
}

/* Local Variables: */
/* fill-column: 50 */
/* End: */
