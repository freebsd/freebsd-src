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
 * $Id: vinumrevive.c,v 1.15 2001/05/23 23:04:48 grog Exp grog $
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
    u_int64_t stripe;					    /* stripe number */
    int paritysd = 0;					    /* set if this is the parity stripe */
    struct rangelock *lock;				    /* for locking */
    daddr_t stripeoffset;				    /* offset in stripe */

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

    if ((sd->revive_blocksize == 0)			    /* no block size */
    ||(sd->revive_blocksize & ((1 << DEV_BSHIFT) - 1)))	    /* or invalid block size */
	sd->revive_blocksize = DEFAULT_REVIVE_BLOCKSIZE;
    else if (sd->revive_blocksize > MAX_REVIVE_BLOCKSIZE)
	sd->revive_blocksize = MAX_REVIVE_BLOCKSIZE;
    size = min(sd->revive_blocksize >> DEV_BSHIFT, sd->sectors - sd->revived) << DEV_BSHIFT;
    sd->reviver = curproc->p_pid;			    /* note who last had a bash at it */

    /* Now decide where to read from */
    switch (plex->organization) {
    case plex_concat:
	plexblkno = sd->revived + sd->plexoffset;	    /* corresponding address in plex */
	break;

    case plex_striped:
	stripeoffset = sd->revived % plex->stripesize;	    /* offset from beginning of stripe */
	if (stripeoffset + (size >> DEV_BSHIFT) > plex->stripesize)
	    size = (plex->stripesize - stripeoffset) << DEV_BSHIFT;
	plexblkno = sd->plexoffset			    /* base */
	    + (sd->revived - stripeoffset) * plex->subdisks /* offset to beginning of stripe */
	    + stripeoffset;				    /* offset from beginning of stripe */
	break;

    case plex_raid4:
    case plex_raid5:
	stripeoffset = sd->revived % plex->stripesize;	    /* offset from beginning of stripe */
	plexblkno = sd->plexoffset			    /* base */
	    + (sd->revived - stripeoffset) * (plex->subdisks - 1) /* offset to beginning of stripe */
	    +stripeoffset;				    /* offset from beginning of stripe */
	stripe = (sd->revived / plex->stripesize);	    /* stripe number */

	/* Make sure we don't go beyond the end of the band. */
	size = min(size, (plex->stripesize - stripeoffset) << DEV_BSHIFT);
	if (plex->organization == plex_raid4)
	    psd = plex->subdisks - 1;			    /* parity subdisk for this stripe */
	else
	    psd = plex->subdisks - 1 - stripe % plex->subdisks;	/* parity subdisk for this stripe */
	paritysd = plex->sdnos[psd] == sdno;		    /* note if it's the parity subdisk */

	/*
	 * Now adjust for the strangenesses
	 * in RAID-4 and RAID-5 striping.
	 */
	if (sd->plexsdno > psd)				    /* beyond the parity stripe, */
	    plexblkno -= plex->stripesize;		    /* one stripe less */
	else if (paritysd)
	    plexblkno -= plex->stripesize * sd->plexsdno;   /* go back to the beginning of the band */
	break;

    case plex_disorg:					    /* to keep the compiler happy */
    }

    if (paritysd) {					    /* we're reviving a parity block, */
	bp = parityrebuild(plex, sd->revived, size, rebuildparity, &lock, NULL); /* do the grunt work */
	if (bp == NULL)					    /* no buffer space */
	    return ENOMEM;				    /* chicken out */
    } else {						    /* data block */
	s = splbio();
	bp = geteblk(size);				    /* Get a buffer */
	splx(s);
	if (bp == NULL)
	    return ENOMEM;

	/*
	 * Amount to transfer: block size, unless it
	 * would overlap the end.
	 */
	bp->b_bcount = size;
	bp->b_resid = bp->b_bcount;
	bp->b_blkno = plexblkno;			    /* start here */
	if (isstriped(plex))				    /* we need to lock striped plexes */
	    lock = lockrange(plexblkno << DEV_BSHIFT, bp, plex); /* lock it */
	if (vol != NULL)				    /* it's part of a volume, */
	    /*
	       * First, read the data from the volume.  We
	       * don't care which plex, that's bre's job.
	     */
	    bp->b_dev = VINUMDEV(plex->volno, 0, 0, VINUM_VOLUME_TYPE);	/* create the device number */
	else						    /* it's an unattached plex */
	    bp->b_dev = VINUM_PLEX(sd->plexno);		    /* create the device number */

	bp->b_iocmd = BIO_READ;				    /* either way, read it */
	bp->b_flags = 0;
	vinumstart(bp, 1);
	bufwait(bp);
    }

    if (bp->b_ioflags & BIO_ERROR)
	error = bp->b_error;
    else
	/* Now write to the subdisk */
    {
	bp->b_dev = VINUM_SD(sdno);			    /* create the device number */
	bp->b_flags &= ~B_DONE;				    /* no longer done */
	bp->b_ioflags = BIO_ORDERED;			    /* and make this an ordered write */
	bp->b_iocmd = BIO_WRITE;
	bp->b_resid = bp->b_bcount;
	bp->b_blkno = sd->revived;			    /* write it to here */
	sdio(bp);					    /* perform the I/O */
	bufwait(bp);
	if (bp->b_ioflags & BIO_ERROR)
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
#ifdef VINUMDEBUG
	    struct request *rq = sd->waitlist;

	    if (debug & DEBUG_REVIVECONFLICT)
		log(LOG_DEBUG,
		    "Relaunch revive conflict sd %d: %p\n%s dev %d.%d, offset 0x%x, length %ld\n",
		    rq->sdno,
		    rq,
		    rq->bp->b_iocmd == BIO_READ ? "Read" : "Write",
		    major(rq->bp->b_dev),
		    minor(rq->bp->b_dev),
		    rq->bp->b_blkno,
		    rq->bp->b_bcount);
#endif
	    launch_requests(sd->waitlist, 1);		    /* do them now */
	    sd->waitlist = sd->waitlist->next;		    /* and move on to the next */
	}
    }
    if (bp->b_qindex == 0) {				    /* not on a queue, */
	bp->b_flags |= B_INVAL;
	bp->b_ioflags &= ~BIO_ERROR;
	brelse(bp);					    /* is this kosher? */
    }
    return error;
}

/*
 * Check or rebuild the parity blocks of a RAID-4
 * or RAID-5 plex.
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
parityops(struct vinum_ioctl_msg *data)
{
    int plexno;
    struct plex *plex;
    int size;						    /* I/O transfer size, bytes */
    int stripe;						    /* stripe number in plex */
    int psd;						    /* parity subdisk number */
    struct rangelock *lock;				    /* lock on stripe */
    struct _ioctl_reply *reply;
    off_t pstripe;					    /* pointer to our stripe counter */
    struct buf *pbp;
    off_t errorloc;					    /* offset of parity error */
    enum parityop op;					    /* operation to perform */

    plexno = data->index;
    op = data->op;
    pbp = NULL;
    reply = (struct _ioctl_reply *) data;
    reply->error = EAGAIN;				    /* expect to repeat this call */
    plex = &PLEX[plexno];
    if (!isparity(plex)) {				    /* not RAID-4 or RAID-5 */
	reply->error = EINVAL;
	return;
    } else if (plex->state < plex_flaky) {
	reply->error = EIO;
	strcpy(reply->msg, "Plex is not completely accessible\n");
	return;
    }
    pstripe = data->offset;
    stripe = pstripe / plex->stripesize;		    /* stripe number */
    psd = plex->subdisks - 1 - stripe % plex->subdisks;	    /* parity subdisk for this stripe */
    size = min(DEFAULT_REVIVE_BLOCKSIZE,		    /* one block at a time */
	plex->stripesize << DEV_BSHIFT);

    pbp = parityrebuild(plex, pstripe, size, op, &lock, &errorloc); /* do the grunt work */
    if (pbp == NULL) {					    /* no buffer space */
	reply->error = ENOMEM;
	return;						    /* chicken out */
    }
    /*
     * Now we have a result in the data buffer of
     * the parity buffer header, which we have kept.
     * Decide what to do with it.
     */
    reply->msg[0] = '\0';				    /* until shown otherwise */
    if ((pbp->b_ioflags & BIO_ERROR) == 0) {		    /* no error */
	if ((op == rebuildparity)
	    || (op == rebuildandcheckparity)) {
	    pbp->b_iocmd = BIO_WRITE;
	    pbp->b_resid = pbp->b_bcount;
	    sdio(pbp);					    /* write the parity block */
	    bufwait(pbp);
	}
	if (((op == checkparity)
		|| (op == rebuildandcheckparity))
	    && (errorloc != -1)) {
	    if (op == checkparity)
		reply->error = EIO;
	    sprintf(reply->msg,
		"Parity incorrect at offset 0x%llx\n",
		errorloc);
	}
	if (reply->error == EAGAIN) {			    /* still OK, */
	    plex->checkblock = pstripe + (pbp->b_bcount >> DEV_BSHIFT);	/* moved this much further down */
	    if (plex->checkblock >= SD[plex->sdnos[0]].sectors) { /* finished */
		plex->checkblock = 0;
		reply->error = 0;
	    }
	}
    }
    if (pbp->b_ioflags & BIO_ERROR)
	reply->error = pbp->b_error;
    pbp->b_flags |= B_INVAL;
    pbp->b_ioflags &= ~BIO_ERROR;
    brelse(pbp);
    unlockrange(plexno, lock);
}

/*
 * Rebuild a parity stripe.  Return pointer to
 * parity bp.  On return,
 *
 * 1.  The band is locked.  The caller must unlock
 *     the band and release the buffer header.
 *
 * 2.  All buffer headers except php have been
 *     released.  The caller must release pbp.
 *
 * 3.  For checkparity and rebuildandcheckparity,
 *     the parity is compared with the current
 *     parity block.  If it's different, the
 *     offset of the error is returned to
 *     errorloc.  The caller can set the value of
 *     the pointer to NULL if this is called for
 *     rebuilding parity.
 *
 * pstripe is the subdisk-relative base address of
 * the data to be reconstructed, size is the size
 * of the transfer in bytes.
 */
struct buf *
parityrebuild(struct plex *plex,
    u_int64_t pstripe,
    int size,
    enum parityop op,
    struct rangelock **lockp,
    off_t * errorloc)
{
    int error;
    int s;
    int sdno;
    u_int64_t stripe;					    /* stripe number */
    int *parity_buf;					    /* buffer address for current parity block */
    int *newparity_buf;					    /* and for new parity block */
    int mysize;						    /* I/O transfer size for this transfer */
    int isize;						    /* mysize in ints */
    int i;
    int psd;						    /* parity subdisk number */
    int newpsd;						    /* and "subdisk number" of new parity */
    struct buf **bpp;					    /* pointers to our bps */
    struct buf *pbp;					    /* buffer header for parity stripe */
    int *sbuf;
    int bufcount;					    /* number of buffers we need */

    stripe = pstripe / plex->stripesize;		    /* stripe number */
    psd = plex->subdisks - 1 - stripe % plex->subdisks;	    /* parity subdisk for this stripe */
    parity_buf = NULL;					    /* to keep the compiler happy */
    error = 0;

    /*
     * It's possible that the default transfer size
     * we chose is not a factor of the stripe size.
     * We *must* limit this operation to a single
     * stripe, at least for RAID-5 rebuild, since
     * the parity subdisk changes between stripes,
     * so in this case we need to perform a short
     * transfer.  Set variable mysize to reflect
     * this.
     */
    mysize = min(size, (plex->stripesize * (stripe + 1) - pstripe) << DEV_BSHIFT);
    isize = mysize / (sizeof(int));			    /* number of ints in the buffer */
    bufcount = plex->subdisks + 1;			    /* sd buffers plus result buffer */
    newpsd = plex->subdisks;
    bpp = (struct buf **) Malloc(bufcount * sizeof(struct buf *)); /* array of pointers to bps */

    /* First, build requests for all subdisks */
    for (sdno = 0; sdno < bufcount; sdno++) {		    /* for each subdisk */
	if ((sdno != psd) || (op != rebuildparity)) {
	    /* Get a buffer header and initialize it. */
	    s = splbio();
	    bpp[sdno] = geteblk(mysize);		    /* Get a buffer */
	    if (bpp[sdno] == NULL) {
		while (sdno-- > 0) {			    /* release the ones we got */
		    bpp[sdno]->b_flags |= B_INVAL;
		    brelse(bpp[sdno]);			    /* give back our resources */
		}
		splx(s);
		printf("vinum: can't allocate buffer space for parity op.\n");
		return NULL;				    /* no bpps */
	    }
	    splx(s);
	    if (sdno == psd)
		parity_buf = (int *) bpp[sdno]->b_data;
	    if (sdno == newpsd)				    /* the new one? */
		bpp[sdno]->b_dev = VINUM_SD(plex->sdnos[psd]); /* write back to the parity SD */
	    else
		bpp[sdno]->b_dev = VINUM_SD(plex->sdnos[sdno]);	/* device number */
	    bpp[sdno]->b_iocmd = BIO_READ;		    /* either way, read it */
	    bpp[sdno]->b_flags = 0;
	    bpp[sdno]->b_bcount = mysize;
	    bpp[sdno]->b_resid = bpp[sdno]->b_bcount;
	    bpp[sdno]->b_blkno = pstripe;		    /* transfer from here */
	}
    }

    /* Initialize result buffer */
    pbp = bpp[newpsd];
    newparity_buf = (int *) bpp[newpsd]->b_data;
    bzero(newparity_buf, mysize);

    /*
     * Now lock the stripe with the first non-parity
     * bp as locking bp.
     */
    *lockp = lockrange(pstripe * plex->stripesize * (plex->subdisks - 1),
	bpp[psd ? 0 : 1],
	plex);

    /*
     * Then issue requests for all subdisks in
     * parallel.  Don't transfer the parity stripe
     * if we're rebuilding parity, unless we also
     * want to check it.
     */
    for (sdno = 0; sdno < plex->subdisks; sdno++) {	    /* for each real subdisk */
	if ((sdno != psd) || (op != rebuildparity)) {
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
	if ((sdno != psd) || (op != rebuildparity)) {
	    bufwait(bpp[sdno]);
	    if (bpp[sdno]->b_ioflags & BIO_ERROR)	    /* can't read, */
		error = bpp[sdno]->b_error;
	    else if (sdno != psd) {			    /* update parity */
		sbuf = (int *) bpp[sdno]->b_data;
		for (i = 0; i < isize; i++)
		    ((int *) newparity_buf)[i] ^= sbuf[i];  /* xor in the buffer */
	    }
	}
	if (sdno != psd) {				    /* release all bps except parity */
	    bpp[sdno]->b_flags |= B_INVAL;
	    brelse(bpp[sdno]);				    /* give back our resources */
	}
    }

    /*
     * If we're checking, compare the calculated
     * and the read parity block.  If they're
     * different, return the plex-relative offset;
     * otherwise return -1.
     */
    if ((op == checkparity)
	|| (op == rebuildandcheckparity)) {
	*errorloc = -1;					    /* no error yet */
	for (i = 0; i < isize; i++) {
	    if (parity_buf[i] != newparity_buf[i]) {
		*errorloc = (off_t) (pstripe << DEV_BSHIFT) * (plex->subdisks - 1)
		    + i * sizeof(int);
		break;
	    }
	}
	bpp[psd]->b_flags |= B_INVAL;
	brelse(bpp[psd]);				    /* give back our resources */
    }
    /* release our resources */
    Free(bpp);
    if (error) {
	pbp->b_ioflags |= BIO_ERROR;
	pbp->b_error = error;
    }
    return pbp;
}

/*
 * Initialize a subdisk by writing zeroes to the
 * complete address space.  If verify is set,
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
	splx(s);
	if (bp == NULL)
	    return ENOMEM;

	bp->b_bcount = size;
	bp->b_resid = bp->b_bcount;
	bp->b_blkno = sd->initialized;			    /* write it to here */
	bzero(bp->b_data, bp->b_bcount);
	bp->b_dev = VINUM_SD(sdno);			    /* create the device number */
	bp->b_iocmd = BIO_WRITE;
	sdio(bp);					    /* perform the I/O */
	bufwait(bp);
	if (bp->b_ioflags & BIO_ERROR)
	    error = bp->b_error;
	if (bp->b_qindex == 0) {			    /* not on a queue, */
	    bp->b_flags |= B_INVAL;
	    bp->b_ioflags &= ~BIO_ERROR;
	    brelse(bp);					    /* is this kosher? */
	}
	if ((error == 0) && verify) {			    /* check that it got there */
	    s = splbio();
	    bp = geteblk(size);				    /* get a buffer */
	    if (bp == NULL) {
		splx(s);
		error = ENOMEM;
	    } else {
		bp->b_bcount = size;
		bp->b_resid = bp->b_bcount;
		bp->b_blkno = sd->initialized;		    /* read from here */
		bp->b_dev = VINUM_SD(sdno);		    /* create the device number */
		bp->b_iocmd = BIO_READ;			    /* read it back */
		splx(s);
		sdio(bp);
		bufwait(bp);
		/*
		 * XXX Bug fix code.  This is hopefully no
		 * longer needed (21 February 2000).
		 */
		if (bp->b_ioflags & BIO_ERROR)
		    error = bp->b_error;
		else if ((*bp->b_data != 0)		    /* first word spammed */
		||(bcmp(bp->b_data, &bp->b_data[1], bp->b_bcount - 1))) { /* or one of the others */
		    printf("vinum: init error on %s, offset 0x%llx sectors\n",
			sd->name,
			(long long) sd->initialized);
		    verified = 0;
		} else
		    verified = 1;
		if (bp->b_qindex == 0) {		    /* not on a queue, */
		    bp->b_flags |= B_INVAL;
		    bp->b_ioflags &= ~BIO_ERROR;
		    brelse(bp);				    /* is this kosher? */
		}
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
