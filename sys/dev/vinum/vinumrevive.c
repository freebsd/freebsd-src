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
 * $Id: vinumrevive.c,v 1.9 1999/10/12 04:38:27 grog Exp grog $
 * $FreeBSD$
 */

#include <dev/vinum/vinumhdr.h>
#include <dev/vinum/request.h>

/*
 * Revive a block of a subdisk.  Return an error
 * indication.  EAGAIN means successful copy, but
 * that more blocks remain to be copied.  EINVAL
 * means that the subdisk isn't associated with a
 * plex (which means a programming error if we get
 * here at all; FIXME).
 */
int
revive_block(int sdno)
{
    int s;						    /* priority level */
    struct sd *sd;
    struct plex *plex;
    struct volume *vol;
    struct buf *bp;
    int error = EAGAIN;
    int size;						    /* size of revive block, bytes */
    daddr_t plexblkno;					    /* lblkno in plex */
    int psd;						    /* parity subdisk number */
    int stripe;						    /* stripe number */
    int isparity = 0;					    /* set if this is the parity stripe */
    struct rangelock *lock;				    /* for locking */

    plexblkno = 0;					    /* to keep the compiler happy */
    sd = &SD[sdno];
    lock = NULL;
    if (sd->plexno < 0)					    /* no plex? */
	return EINVAL;
    plex = &PLEX[sd->plexno];				    /* point to plex */
    if (plex->volno >= 0)
	vol = &VOL[plex->volno];
    else
	vol = NULL;

    if (sd->revive_blocksize == 0) {
	if (plex->stripesize != 0)			    /* we're striped, don't revive more than */
	    sd->revive_blocksize = min(DEFAULT_REVIVE_BLOCKSIZE, /* one block at a time */
		plex->stripesize << DEV_BSHIFT);
	else
	    sd->revive_blocksize = DEFAULT_REVIVE_BLOCKSIZE;
    } else if (sd->revive_blocksize > MAX_REVIVE_BLOCKSIZE)
	sd->revive_blocksize = MAX_REVIVE_BLOCKSIZE;

    size = min(sd->revive_blocksize >> DEV_BSHIFT, sd->sectors - sd->revived) << DEV_BSHIFT;

    s = splbio();
    bp = geteblk(size);					    /* Get a buffer */
    if (bp == NULL) {
	splx(s);
	return ENOMEM;
    }
    if (bp->b_qindex != 0)				    /* on a queue, */
	bremfree(bp);					    /* remove it XXX how can this happen? */
    splx(s);

    /*
     * Amount to transfer: block size, unless it
     * would overlap the end
     */
    bp->b_bufsize = size;
    bp->b_bcount = bp->b_bufsize;
    bp->b_resid = bp->b_bcount;

    /* Now decide where to read from */

    switch (plex->organization) {
	daddr_t stripeoffset;				    /* offset in stripe */

    case plex_concat:
	plexblkno = sd->revived + sd->plexoffset;	    /* corresponding address in plex */
	break;

    case plex_striped:
	stripeoffset = sd->revived % plex->stripesize;	    /* offset from beginning of stripe */
	plexblkno = sd->plexoffset			    /* base */
	    + (sd->revived - stripeoffset) * plex->subdisks /* offset to beginning of stripe */
	    + sd->revived % plex->stripesize;		    /* offset from beginning of stripe */
	lock = lockrange(plexblkno << DEV_BSHIFT, bp, plex); /* lock it */
	break;

    case plex_raid5:
	stripeoffset = sd->revived % plex->stripesize;	    /* offset from beginning of stripe */
	plexblkno = sd->plexoffset			    /* base */
	    + (sd->revived - stripeoffset) * (plex->subdisks - 1) /* offset to beginning of stripe */
	    +stripeoffset;				    /* offset from beginning of stripe */
	stripe = (sd->revived / plex->stripesize);	    /* stripe number */
	psd = plex->subdisks - 1 - stripe % plex->subdisks; /* parity subdisk for this stripe */
	isparity = plex->sdnos[psd] == sdno;		    /* note if it's the parity subdisk */

	/*
	 * Now adjust for the strangenesses
	 * in RAID-5 striping.
	 */
	if (sd->plexsdno > psd)				    /* beyond the parity stripe, */
	    plexblkno -= plex->stripesize;		    /* one stripe less */
	lock = lockrange(plexblkno << DEV_BSHIFT, bp, plex); /* lock it */
	break;

    case plex_disorg:					    /* to keep the compiler happy */
    }

    if (isparity) {					    /* we're reviving a parity block, */
	int mysdno;
	int *tbuf;					    /* temporary buffer to read the stuff in to */
	caddr_t parity_buf;				    /* the address supplied by geteblk */
	int isize;
	int i;

	tbuf = (int *) Malloc(size);
	isize = size / (sizeof(int));			    /* number of ints in the buffer */

	/*
	 * We have calculated plexblkno assuming it
	 * was a data block.  Go back to the beginning
	 * of the band.
	 */
	plexblkno -= plex->stripesize * sd->plexsdno;

	/*
	 * Read each subdisk in turn, except for this
	 * one, and xor them together.
	 */
	parity_buf = bp->b_data;			    /* save the buffer getblk gave us */
	bzero(parity_buf, size);			    /* start with nothing */
	bp->b_data = (caddr_t) tbuf;			    /* read into here */
	for (mysdno = 0; mysdno < plex->subdisks; mysdno++) { /* for each subdisk */
	    if (mysdno != sdno) {			    /* not our subdisk */
		if (vol != NULL)			    /* it's part of a volume, */
		    /*
		       * First, read the data from the volume.
		       * We don't care which plex, that's the
		       * driver's job.
		     */
		    bp->b_dev = VINUMBDEV(plex->volno, 0, 0, VINUM_VOLUME_TYPE); /* create the device number */
		else					    /* it's an unattached plex */
		    bp->b_dev = VINUMRBDEV(sd->plexno, VINUM_RAWPLEX_TYPE); /* create the device number */

		bp->b_blkno = plexblkno;		    /* read from here */
		bp->b_flags = B_READ;			    /* either way, read it */
		BUF_LOCKINIT(bp);			    /* get a lock for the buffer */
		BUF_LOCK(bp, LK_EXCLUSIVE);		    /* and lock it */
		vinumstart(bp, 1);
		biowait(bp);
		if (bp->b_flags & B_ERROR)		    /* can't read, */
		    /*
		       * If we have a read error, there's
		       * nothing we can do.  By this time, the
		       * daemon has already run out of magic.
		     */
		    break;
		/*
		 * To save time, we do the XOR wordwise.
		 * This requires sectors to be a multiple
		 * of the length of an int, which is
		 * currently always the case.
		 */
		for (i = 0; i < isize; i++)
		    ((int *) parity_buf)[i] ^= tbuf[i];	    /* xor in the buffer */
		plexblkno += plex->stripesize;		    /* move on to the next subdisk */
	    }
	}
	bp->b_data = parity_buf;			    /* put the buf header back the way it was */
	Free(tbuf);
    } else {
	bp->b_blkno = plexblkno;			    /* start here */
	if (vol != NULL)				    /* it's part of a volume, */
	    /*
	       * First, read the data from the volume.  We
	       * don't care which plex, that's bre's job.
	     */
	    bp->b_dev = VINUMBDEV(plex->volno, 0, 0, VINUM_VOLUME_TYPE); /* create the device number */
	else						    /* it's an unattached plex */
	    bp->b_dev = VINUMRBDEV(sd->plexno, VINUM_RAWPLEX_TYPE); /* create the device number */

	bp->b_flags = B_READ;				    /* either way, read it */
	vinumstart(bp, 1);
	biowait(bp);
    }
    if (bp->b_flags & B_ERROR)
	error = bp->b_error;
    else
	/* Now write to the subdisk */
    {
	s = splbio();
	if (bp->b_qindex != 0)				    /* on a queue, */
	    bremfree(bp);				    /* remove it */
	splx(s);

	bp->b_dev = VINUMRBDEV(sdno, VINUM_RAWSD_TYPE);	    /* create the device number */
	bp->b_flags = B_ORDERED;			    /* and make this an ordered write */
	BUF_LOCKINIT(bp);				    /* get a lock for the buffer */
	BUF_LOCK(bp, LK_EXCLUSIVE);			    /* and lock it */
	bp->b_resid = bp->b_bcount;
	bp->b_blkno = sd->revived;			    /* write it to here */
	sdio(bp);					    /* perform the I/O */
	biowait(bp);
	if (bp->b_flags & B_ERROR)
	    error = bp->b_error;
	else {
	    sd->revived += bp->b_bcount >> DEV_BSHIFT;	    /* moved this much further down */
	    if (sd->revived >= sd->sectors) {		    /* finished */
		sd->revived = 0;
		set_sd_state(sdno, sd_up, setstate_force);  /* bring the sd up */
		log(LOG_INFO, "vinum: %s is %s\n", sd->name, sd_state(sd->state));
		save_config();				    /* and save the updated configuration */
		error = 0;				    /* we're done */
	    }
	}
	if (lock)					    /* we took a lock, */
	    unlockrange(sd->plexno, lock);		    /* give it back */
	while (sd->waitlist) {				    /* we have waiting requests */
#if VINUMDEBUG
	    struct request *rq = sd->waitlist;

	    if (debug & DEBUG_REVIVECONFLICT)
		log(LOG_DEBUG,
		    "Relaunch revive conflict sd %d: %x\n%s dev %d.%d, offset 0x%x, length %ld\n",
		    rq->sdno,
		    (u_int) rq,
		    rq->bp->b_flags & B_READ ? "Read" : "Write",
		    major(rq->bp->b_dev),
		    minor(rq->bp->b_dev),
		    rq->bp->b_blkno,
		    rq->bp->b_bcount);
#endif
	    launch_requests(sd->waitlist, 1);		    /* do them now */
	    sd->waitlist = sd->waitlist->next;		    /* and move on to the next */
	}
    }
    if (bp->b_qindex == 0)				    /* not on a queue, */
	brelse(bp);					    /* is this kosher? */
    return error;
}

/*
 * Check or rebuild the parity blocks of a RAID-5
 * plex.
 *
 * The variables plex->checkblock and
 * plex->rebuildblock represent the
 * subdisk-relative address of the stripe we're
 * looking at, not the plex-relative address.  We
 * store it in the plex and not as a local
 * variable because this function could be
 * stopped, and we don't want to repeat the part
 * we've already done.  This is also the reason
 * why we don't initialize it here except at the
 * end.  It gets initialized with the plex on
 * creation.
 *
 * Each call to this function processes at most
 * one stripe.  We can't loop in this function,
 * because we're unstoppable, so we have to be
 * called repeatedly from userland.
 */
void
parityops(struct vinum_ioctl_msg *data, enum parityop op)
{
    int plexno;
    int s;
    struct plex *plex;
    int sdno;
    int *tbuf;						    /* temporary buffer to read the stuff in to */
    int *parity_buf;					    /* the address supplied by geteblk */
    int size;						    /* I/O transfer size, bytes */
    int mysize;						    /* I/O transfer size for this transfer */
    int isize;						    /* mysize in ints */
    int i;
    int stripe;						    /* stripe number in plex */
    int psd;						    /* parity subdisk number */
    struct rangelock *lock;				    /* lock on stripe */
    struct _ioctl_reply *reply;
    u_int64_t *pstripe;					    /* pointer to our stripe counter */
    struct buf **bpp;					    /* pointers to our bps */

    plexno = data->index;
    reply = (struct _ioctl_reply *) data;
    reply->error = EAGAIN;				    /* expect to repeat this call */
    reply->msg[0] = '\0';
    plex = &PLEX[plexno];
    if (plex->organization != plex_raid5) {
	reply->error = EINVAL;
	return;
    }
    if (op == rebuildparity)				    /* point to our counter */
	pstripe = &plex->rebuildblock;
    else
	pstripe = &plex->checkblock;
    stripe = *pstripe / plex->stripesize;		    /* stripe number */
    psd = plex->subdisks - 1 - stripe % plex->subdisks;	    /* parity subdisk for this stripe */
    size = min(DEFAULT_REVIVE_BLOCKSIZE,		    /* one block at a time */
	plex->stripesize << DEV_BSHIFT);

    /*
     * It's possible that the default transfer
     * size we chose is not a factor of the stripe
     * size.  We *must* limit this operation to a
     * single stripe, at least for rebuild, since
     * the parity subdisk changes between stripes,
     * so in this case we need to perform a short
     * transfer.  Set variable mysize to reflect
     * this.
     */
    mysize = min(size, (plex->stripesize * (stripe + 1) - *pstripe) << DEV_BSHIFT);
    isize = mysize / (sizeof(int));			    /* number of ints in the buffer */

    tbuf = (int *) Malloc(mysize * plex->subdisks);	    /* space for the whole stripe */
    parity_buf = &tbuf[isize * psd];			    /* this is the parity stripe */
    if (op == rebuildparity)
	bzero(parity_buf, mysize);
    bpp = (struct buf **) Malloc(plex->subdisks * sizeof(struct buf *)); /* array of pointers to bps */

    /* First, issue requests for all subdisks in parallel */
    for (sdno = 0; sdno < plex->subdisks; sdno++) {	    /* for each subdisk */
	/* Get a buffer header and initialize it. */
	s = splbio();
	bpp[sdno] = geteblk(mysize);			    /* Get a buffer */
	if (bpp[sdno] == NULL) {
	    splx(s);
	    reply->error = ENOMEM;
	    return;
	}
	if (bpp[sdno]->b_qindex != 0)			    /* on a queue, */
	    bremfree(bpp[sdno]);			    /* remove it */
	splx(s);
	bpp[sdno]->b_data = (caddr_t) & tbuf[isize * sdno]; /* read into here */
	bpp[sdno]->b_dev = VINUMRBDEV(plex->sdnos[sdno],    /* device number */
	    VINUM_RAWSD_TYPE);
	bpp[sdno]->b_flags = B_READ;			    /* either way, read it */
	bpp[sdno]->b_bufsize = mysize;
	bpp[sdno]->b_bcount = bpp[sdno]->b_bufsize;
	bpp[sdno]->b_resid = bpp[sdno]->b_bcount;
	bpp[sdno]->b_blkno = *pstripe;			    /* read from here */
    }

    /*
     * Now lock the stripe with the first non-parity
     * bp as locking bp.
     */
    lock = lockrange(stripe * plex->stripesize * (plex->subdisks - 1),
	bpp[psd ? 0 : 1],
	plex);

    /* Then issue requests for all subdisks in parallel */
    for (sdno = 0; sdno < plex->subdisks; sdno++) {	    /* for each subdisk */
	if ((sdno != psd)
	    || (op == checkparity)) {
	    BUF_LOCKINIT(bpp[sdno]);			    /* get a lock for the buffer */
	    BUF_LOCK(bpp[sdno], LK_EXCLUSIVE);		    /* and lock it */
	    sdio(bpp[sdno]);
	}
    }

    /*
     * Next, wait for the requests to complete.
     * We wait in the order in which they were
     * issued, which isn't necessarily the order in
     * which they complete, but we don't have a
     * convenient way of doing the latter, and the
     * delay is minimal.
     */
    for (sdno = 0; sdno < plex->subdisks; sdno++) {	    /* for each subdisk */
	if ((sdno != psd)
	    || (op == checkparity)) {
	    biowait(bpp[sdno]);
	    if (bpp[sdno]->b_flags & B_ERROR)		    /* can't read, */
		reply->error = bpp[sdno]->b_error;
	    brelse(bpp[sdno]);				    /* give back our resources */
	}
    }

    /*
     * Finally, do the xors.  We need to do this in
     * a separate loop because we don't know when
     * the parity stripe will be completed.
     */
    for (sdno = 0; sdno < plex->subdisks; sdno++) {	    /* for each subdisk */
	int *sbuf = &tbuf[isize * sdno];

	if (sdno != psd) {
	    /*
	     * To save time, we do the XOR wordwise.
	     * This requires sectors to be a multiple
	     * of the length of an int, which is
	     * currently always the case.
	     */
	    for (i = 0; i < isize; i++)
		((int *) parity_buf)[i] ^= sbuf[i];	    /* xor in the buffer */
	}
    }

    if (reply->error == EAGAIN) {			    /* no other error */
	/*
	 * Finished building the parity block.  Now
	 * decide what to do with it.
	 */
	if (op == checkparity) {
	    for (i = 0; i < isize; i++) {
		if (((int *) parity_buf)[i] != 0) {
		    reply->error = EIO;
		    sprintf(reply->msg,
			"Parity incorrect at offset 0x%lx\n",
			(u_long) (*pstripe << DEV_BSHIFT) * (plex->subdisks - 1)
			+ i * sizeof(int));
		    break;
		}
	    }
	} else {					    /* rebuildparity */
	    s = splbio();
	    if (bpp[psd]->b_qindex != 0)		    /* on a queue, */
		bremfree(bpp[psd]);			    /* remove it */
	    splx(s);

	    bpp[psd]->b_dev = VINUMRBDEV(psd, VINUM_RAWSD_TYPE); /* create the device number */
	    BUF_LOCKINIT(bpp[psd]);			    /* get a lock for the buffer */
	    BUF_LOCK(bpp[psd], LK_EXCLUSIVE);		    /* and lock it */
	    bpp[psd]->b_resid = bpp[psd]->b_bcount;
	    bpp[psd]->b_blkno = *pstripe;		    /* write it to here */
	    sdio(bpp[psd]);				    /* perform the I/O */
	    biowait(bpp[psd]);
	    brelse(bpp[psd]);
	}
	if (bpp[psd]->b_flags & B_ERROR)
	    reply->error = bpp[psd]->b_error;
	if (reply->error == EAGAIN) {			    /* still OK, */
	    *pstripe += mysize >> DEV_BSHIFT;		    /* moved this much further down */
	    if (*pstripe >= SD[plex->sdnos[0]].sectors) {   /* finished */
		*pstripe = 0;
		reply->error = 0;
	    }
	}
    }
    /* release our resources */
    unlockrange(plexno, lock);
    Free(bpp);
    Free(tbuf);
}

/*
 * Initialize a subdisk by writing zeroes to the
 * complete address space.  If check is set,
 * check each transfer for correctness.
 *
 * Each call to this function writes (and maybe
 * checks) a single block.
 */
int
initsd(int sdno, int verify)
{
    int s;						    /* priority level */
    struct sd *sd;
    struct plex *plex;
    struct volume *vol;
    struct buf *bp;
    int error;
    int size;						    /* size of init block, bytes */
    daddr_t plexblkno;					    /* lblkno in plex */
    int verified;					    /* set when we're happy with what we wrote */

    error = 0;
    plexblkno = 0;					    /* to keep the compiler happy */
    sd = &SD[sdno];
    if (sd->plexno < 0)					    /* no plex? */
	return EINVAL;
    plex = &PLEX[sd->plexno];				    /* point to plex */
    if (plex->volno >= 0)
	vol = &VOL[plex->volno];
    else
	vol = NULL;

    if (sd->init_blocksize == 0) {
	if (plex->stripesize != 0)			    /* we're striped, don't init more than */
	    sd->init_blocksize = min(DEFAULT_REVIVE_BLOCKSIZE, /* one block at a time */
		plex->stripesize << DEV_BSHIFT);
	else
	    sd->init_blocksize = DEFAULT_REVIVE_BLOCKSIZE;
    } else if (sd->init_blocksize > MAX_REVIVE_BLOCKSIZE)
	sd->init_blocksize = MAX_REVIVE_BLOCKSIZE;

    size = min(sd->init_blocksize >> DEV_BSHIFT, sd->sectors - sd->initialized) << DEV_BSHIFT;

    verified = 0;
    while (!verified) {					    /* until we're happy with it, */
	s = splbio();
	bp = geteblk(size);				    /* Get a buffer */
	if (bp == NULL) {
	    splx(s);
	    return ENOMEM;
	}
	if (bp->b_qindex != 0)				    /* on a queue, */
	    bremfree(bp);				    /* remove it XXX how can this happen? */
	splx(s);

	bp->b_bufsize = size;
	bp->b_bcount = bp->b_bufsize;
	bp->b_resid = bp->b_bcount;
	bp->b_data = Malloc(bp->b_bcount);
	bp->b_blkno = sd->initialized;			    /* write it to here */
	if (bp->b_data == NULL)
	    return ENOMEM;
	bzero(bp->b_data, bp->b_bcount);
	bp->b_dev = VINUMRBDEV(sdno, VINUM_RAWSD_TYPE);	    /* create the device number */
	BUF_LOCKINIT(bp);				    /* get a lock for the buffer */
	BUF_LOCK(bp, LK_EXCLUSIVE);			    /* and lock it */
	sdio(bp);					    /* perform the I/O */
	biowait(bp);
	if (bp->b_flags & B_ERROR)
	    error = bp->b_error;
	Free(bp->b_data);
	if (bp->b_qindex == 0)				    /* not on a queue, */
	    brelse(bp);					    /* is this kosher? */
	if ((error == 0) && verify) {			    /* check that it got there */
	    s = splbio();
	    bp = geteblk(size);				    /* get a buffer */
	    if (bp == NULL) {
		splx(s);
		error = ENOMEM;
	    } else {
		if (bp->b_qindex != 0)			    /* on a queue, */
		    bremfree(bp);			    /* remove it XXX how can this happen? */
		splx(s);

		bp->b_bufsize = size;
		bp->b_bcount = bp->b_bufsize;
		bp->b_resid = bp->b_bcount;
		bp->b_data = Malloc(bp->b_bcount);
		bp->b_blkno = sd->initialized;		    /* read from here */
		if (bp->b_data == NULL) {
		    brelse(bp);
		    error = ENOMEM;
		    break;
		}
		bp->b_dev = VINUMRBDEV(sdno, VINUM_RAWSD_TYPE);	/* create the device number */
		bp->b_flags |= B_READ;			    /* read it back */
		BUF_LOCKINIT(bp);			    /* get a lock for the buffer */
		BUF_LOCK(bp, LK_EXCLUSIVE);		    /* and lock it */
		sdio(bp);
		biowait(bp);
		if (bp->b_flags & B_ERROR)
		    error = bp->b_error;
		else if ((*bp->b_data != 0)		    /* first word spammed */
		||(bcmp(bp->b_data, &bp->b_data[1], bp->b_bcount - 1))) { /* or one of the others */
		    printf("vinum: init error on %s, offset 0x%llx sectors\n",
			sd->name,
			sd->initialized);
		    verified = 0;
		} else
		    verified = 1;
		Free(bp->b_data);
		if (bp->b_qindex == 0)			    /* not on a queue, */
		    brelse(bp);				    /* is this kosher? */
	    }
	} else
	    verified = 1;
    }
    if (error == 0) {					    /* did it, */
	sd->initialized += size >> DEV_BSHIFT;		    /* moved this much further down */
	if (sd->initialized >= sd->sectors) {		    /* finished */
	    sd->initialized = 0;
	    set_sd_state(sdno, sd_initialized, setstate_force);	/* bring the sd up */
	    log(LOG_INFO, "vinum: %s is %s\n", sd->name, sd_state(sd->state));
	    save_config();				    /* and save the updated configuration */
	} else						    /* more to go, */
	    error = EAGAIN;				    /* ya'll come back, see? */
    }
    return error;
}

/* Local Variables: */
/* fill-column: 50 */
/* End: */
