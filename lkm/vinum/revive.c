/*-
 * Copyright (c) 1997, 1998
 *	Nan Yang Computer Services Limited.  All rights reserved.
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
 * $Id: revive.c,v 1.1 1998/08/14 06:16:59 grog Exp grog $
 */

#define REALLYKERNEL
#include "vinumhdr.h"
#include "request.h"

/* revive a block of a plex.  Return an error
 * indication.  EAGAIN means successful copy, but
 * that more blocks remain to be copied.
 * XXX We should specify a block size here.  At the moment,
 * just take a default value.  FIXME */
int 
revive_block(int plexno)
{
    struct plex *plex = &PLEX[plexno];
    struct buf *bp;
    int error = EAGAIN;
    int size;
    int s;						    /* priority level */

    if (plex->revive_blocksize == 0) {
	if (plex->stripesize != 0)			    /* we're striped, don't revive more than */
	    plex->revive_blocksize = min(DEFAULT_REVIVE_BLOCKSIZE, plex->stripesize); /* one block at a time */
	else
	    plex->revive_blocksize = DEFAULT_REVIVE_BLOCKSIZE;
    }
    size = min(plex->revive_blocksize, plex->length - plex->revived) << DEV_BSHIFT;

    s = splbio();
    /* Get a buffer */
    bp = geteblk(size);
    if (bp == NULL) {
	splx(s);
	return ENOMEM;
    }
    if (bp->b_qindex != 0)				    /* on a queue, */
	bremfree(bp);					    /* remove it */
    splx(s);

    /* Amount to transfer: block size, unless it
     * would overlap the end */
    bp->b_bufsize = size;
    bp->b_bcount = bp->b_bufsize;
    bp->b_resid = 0x0;
    bp->b_blkno = plex->revived;			    /* we've got this far */

    /* XXX what about reviving anonymous plexes? */

    /* First, read the data from the volume.  We don't
     * care which plex, that's bre's job */
    bp->b_dev = VINUMBDEV(plex->volno, 0, 0, VINUM_VOLUME_TYPE); /* create the device number */
    bp->b_flags = B_BUSY | B_READ;
    vinumstart(bp, 1);
    biowait(bp);
    if (bp->b_flags & B_ERROR)
	error = bp->b_error;
    else
	/* Now write to the plex */
    {
	s = splbio();
	if (bp->b_qindex != 0)				    /* on a queue, */
	    bremfree(bp);				    /* remove it */
	splx(s);
	bp->b_dev = VINUMBDEV(plex->volno, plex->volplexno, 0, VINUM_PLEX_TYPE); /* create the device number */

	bp->b_flags = B_BUSY;				    /* make this a write */
	bp->b_resid = 0x0;
	vinumstart(bp, 1);
	biowait(bp);
	if (bp->b_flags & B_ERROR)
	    error = bp->b_error;
	else {
	    plex->revived += bp->b_bcount >> DEV_BSHIFT;    /* moved this much further down */
	    if (plex->revived >= plex->length) {	    /* finished */
		plex->revived = 0;
		plex->state = plex_up;			    /* do we need to do more? */
		if (plex->volno >= 0)			    /* we have a volume, */
		    set_volume_state(plex->volno, volume_up, 0);
		printf("vinum: plex %s is %s\n", plex->name, plex_state(plex->state));
		save_config();				    /* and save the updated configuration */
		error = 0;				    /* we're done */
	    }
	}
	while (plex->waitlist) {			    /* we have waiting requests */
	    launch_requests(plex->waitlist, 1);		    /* do them now */
	    plex->waitlist = plex->waitlist->next;	    /* and move on to the next */
	}
    }
    if (bp->b_qindex == 0)				    /* not on a queue, */
	brelse(bp);					    /* is this kosher? */
    return error;
}
