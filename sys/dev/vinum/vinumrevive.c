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
 * $Id: vinumrevive.c,v 1.8 1999/06/28 01:57:50 grog Exp grog $
 */

#include <dev/vinum/vinumhdr.h>
#include <dev/vinum/request.h>

/*
 * revive a block of a subdisk.  Return an error
 * indication.  EAGAIN means successful copy, but
 * that more blocks remain to be copied.  EINVAL means
 * that the subdisk isn't associated with a plex (which
 * means a programming error if we get here at all;
 * FIXME)
 * XXX We should specify a block size here.  At the moment,
 * just take a default value.  FIXME 
 */
int 
revive_block(int sdno)
{
    struct sd *sd;
    struct plex *plex;
    struct volume *vol;
    struct buf *bp;
    int error = EAGAIN;
    int size;						    /* size of revive block, bytes */
    int s;						    /* priority level */
    daddr_t plexblkno;					    /* lblkno in plex */
    int psd;						    /* parity subdisk number */
    int stripe;						    /* stripe number */
    int isparity = 0;					    /* set if this is the parity stripe */

    plexblkno = 0;					    /* to keep the compiler happy */
    sd = &SD[sdno];
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
    }
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
    bp->b_resid = 0;

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
	break;

    case plex_raid5:
	stripeoffset = sd->revived % plex->stripesize;	    /* offset from beginning of stripe */
	plexblkno = sd->plexoffset			    /* base */
	    + (sd->revived - stripeoffset) * (plex->subdisks - 1) /* offset to beginning of stripe */
	    +sd->revived % plex->stripesize;		    /* offset from beginning of stripe */
	stripe = (sd->revived / plex->stripesize);	    /* stripe number */
	psd = plex->subdisks - 1 - stripe % plex->subdisks; /* parity subdisk for this stripe */
	isparity = plex->sdnos[psd] == sdno;		    /* note if it's the parity subdisk */
	/*
	 * Now adjust for the strangenesses 
	 * in RAID-5 striping 
	 */
	if (sd->plexsdno > psd)				    /* beyond the parity stripe, */
	    plexblkno -= plex->stripesize;		    /* one stripe less */
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
	 * of the band 
	 */
	plexblkno -= plex->stripesize * sd->plexsdno;

	/*
	 * Read each subdisk in turn, except for
	 * this one, and xor them together 
	 */
	parity_buf = bp->b_data;			    /* save the buffer getblk gave us */
	bzero(parity_buf, size);			    /* start with nothing */
	bp->b_data = (caddr_t) tbuf;			    /* read into here */
	for (mysdno = 0; mysdno < plex->subdisks; mysdno++) { /* for each subdisk */
	    if (mysdno != sdno) {			    /* not our subdisk */
		if (vol != NULL)			    /* it's part of a volume, */
		    /*
		       * First, read the data from the volume.  We don't
		       * care which plex, that's the driver's job 
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
		       * If we have a read error, there's nothing
		       * we can do.  By this time, the daemon has
		       * already run out of magic 
		     */
		    break;
		/*
		 * To save time, we do the XOR wordwise.  This
		 * requires sectors to be a multiple of the
		 * length of an int, which is currently always
		 * the case 
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
	       * First, read the data from the volume.  We don't
	       * care which plex, that's bre's job 
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
	bp->b_resid = 0x0;
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
