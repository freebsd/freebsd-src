/*-
 * Copyright (c) 1997, 1998
 *	Cybernet Corporation and Nan Yang Computer Services Limited.
 *      All rights reserved.
 *
 *  This software was developed as part of the NetMAX project.
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
 *	This product includes software developed by Cybernet Corporation
 *      and Nan Yang Computer Services Limited
 * 4. Neither the name of the Companies nor the names of its contributors
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
 * $Id: vinumraid5.c,v 1.23 2003/02/08 03:32:45 grog Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <dev/vinum/vinumhdr.h>
#include <dev/vinum/request.h>
#include <sys/resourcevar.h>

/*
 * Parameters which describe the current transfer.
 * These are only used for calculation, but they
 * need to be passed to other functions, so it's
 * tidier to put them in a struct
 */
struct metrics {
    daddr_t stripebase;					    /* base address of stripe (1st subdisk) */
    int stripeoffset;					    /* offset in stripe */
    int stripesectors;					    /* total sectors to transfer in this stripe */
    daddr_t sdbase;					    /* offset in subdisk of stripe base */
    int sdcount;					    /* number of disks involved in this transfer */
    daddr_t diskstart;					    /* remember where this transfer starts */
    int psdno;						    /* number of parity subdisk */
    int badsdno;					    /* number of down subdisk, if there is one */
    int firstsdno;					    /* first data subdisk number */
    /* These correspond to the fields in rqelement, sort of */
    int useroffset;
    /*
     * Initial offset and length values for the first
     * data block
     */
    int initoffset;					    /* start address of block to transfer */
    short initlen;					    /* length in sectors of data transfer */
    /* Define a normal operation */
    int dataoffset;					    /* start address of block to transfer */
    int datalen;					    /* length in sectors of data transfer */
    /* Define a group operation */
    int groupoffset;					    /* subdisk offset of group operation */
    int grouplen;					    /* length in sectors of group operation */
    /* Define a normal write operation */
    int writeoffset;					    /* subdisk offset of normal write */
    int writelen;					    /* length in sectors of write operation */
    enum xferinfo flags;				    /* to check what we're doing */
    int rqcount;					    /* number of elements in request */
};

enum requeststatus bre5(struct request *rq,
    int plexno,
    daddr_t * diskstart,
    daddr_t diskend);
void complete_raid5_write(struct rqelement *);
enum requeststatus build_rq_buffer(struct rqelement *rqe, struct plex *plex);
void setrqebounds(struct rqelement *rqe, struct metrics *mp);

/*
 * define the low-level requests needed to perform
 * a high-level I/O operation for a specific plex
 * 'plexno'.
 *
 * Return 0 if all subdisks involved in the
 * request are up, 1 if some subdisks are not up,
 * and -1 if the request is at least partially
 * outside the bounds of the subdisks.
 *
 * Modify the pointer *diskstart to point to the
 * end address.  On read, return on the first bad
 * subdisk, so that the caller
 * (build_read_request) can try alternatives.
 *
 * On entry to this routine, the prq structures
 * are not assigned.  The assignment is performed
 * by expandrq().  Strictly speaking, the elements
 * rqe->sdno of all entries should be set to -1,
 * since 0 (from bzero) is a valid subdisk number.
 * We avoid this problem by initializing the ones
 * we use, and not looking at the others (index >=
 * prq->requests).
 */
enum requeststatus
bre5(struct request *rq,
    int plexno,
    daddr_t * diskaddr,
    daddr_t diskend)
{
    struct metrics m;					    /* most of the information */
    struct sd *sd;
    struct plex *plex;
    struct buf *bp;					    /* user's bp */
    struct rqgroup *rqg;				    /* the request group that we will create */
    struct rqelement *rqe;				    /* point to this request information */
    int rsectors;					    /* sectors remaining in this stripe */
    int mysdno;						    /* another sd index in loops */
    int rqno;						    /* request number */

    rqg = NULL;						    /* shut up, damn compiler */
    m.diskstart = *diskaddr;				    /* start of transfer */
    bp = rq->bp;					    /* buffer pointer */
    plex = &PLEX[plexno];				    /* point to the plex */


    while (*diskaddr < diskend) {			    /* until we get it all sorted out */
	if (*diskaddr >= plex->length)			    /* beyond the end of the plex */
	    return REQUEST_EOF;				    /* can't continue */

	m.badsdno = -1;					    /* no bad subdisk yet */

	/* Part A: Define the request */
	/*
	 * First, calculate some sizes:
	 * The offset of the start address from
	 * the start of the stripe.
	 */
	m.stripeoffset = *diskaddr % (plex->stripesize * (plex->subdisks - 1));

	/*
	 * The plex-relative address of the
	 * start of the stripe.
	 */
	m.stripebase = *diskaddr - m.stripeoffset;

	/* subdisk containing the parity stripe */
	if (plex->organization == plex_raid5)
	    m.psdno = plex->subdisks - 1
		- (*diskaddr / (plex->stripesize * (plex->subdisks - 1)))
		% plex->subdisks;
	else						    /* RAID-4 */
	    m.psdno = plex->subdisks - 1;

	/*
	 * The number of the subdisk in which
	 * the start is located.
	 */
	m.firstsdno = m.stripeoffset / plex->stripesize;
	if (m.firstsdno >= m.psdno)			    /* at or past parity sd */
	    m.firstsdno++;				    /* increment it */

	/*
	 * The offset from the beginning of
	 * the stripe on this subdisk.
	 */
	m.initoffset = m.stripeoffset % plex->stripesize;

	/* The offset of the stripe start relative to this subdisk */
	m.sdbase = m.stripebase / (plex->subdisks - 1);

	m.useroffset = *diskaddr - m.diskstart;		    /* The offset of the start in the user buffer */

	/*
	 * The number of sectors to transfer in the
	 * current (first) subdisk.
	 */
	m.initlen = min(diskend - *diskaddr,		    /* the amount remaining to transfer */
	    plex->stripesize - m.initoffset);		    /* and the amount left in this block */

	/*
	 * The number of sectors to transfer in this stripe
	 * is the minumum of the amount remaining to transfer
	 * and the amount left in this stripe.
	 */
	m.stripesectors = min(diskend - *diskaddr,
	    plex->stripesize * (plex->subdisks - 1) - m.stripeoffset);

	/* The number of data subdisks involved in this request */
	m.sdcount = (m.stripesectors + m.initoffset + plex->stripesize - 1) / plex->stripesize;

	/* Part B: decide what kind of transfer this will be.

	 * start and end addresses of the transfer in
	 * the current block.
	 *
	 * There are a number of different kinds of
	 * transfer, each of which relates to a
	 * specific subdisk:
	 *
	 * 1. Normal read.  All participating subdisks
	 *    are up, and the transfer can be made
	 *    directly to the user buffer.  The bounds
	 *    of the transfer are described by
	 *    m.dataoffset and m.datalen.  We have
	 *    already calculated m.initoffset and
	 *    m.initlen, which define the parameters
	 *    for the first data block.
	 *
	 * 2. Recovery read.  One participating
	 *    subdisk is down.  To recover data, all
	 *    the other subdisks, including the parity
	 *    subdisk, must be read.  The data is
	 *    recovered by exclusive-oring all the
	 *    other blocks.  The bounds of the
	 *    transfer are described by m.groupoffset
	 *    and m.grouplen.
	 *
	 * 3. A read request may request reading both
	 *    available data (normal read) and
	 *    non-available data (recovery read).
	 *    This can be a problem if the address
	 *    ranges of the two reads do not coincide:
	 *    in this case, the normal read needs to
	 *    be extended to cover the address range
	 *    of the recovery read, and must thus be
	 *    performed out of malloced memory.
	 *
	 * 4. Normal write.  All the participating
	 *    subdisks are up.  The bounds of the
	 *    transfer are described by m.dataoffset
	 *    and m.datalen.  Since these values
	 *    differ for each block, we calculate the
	 *    bounds for the parity block
	 *    independently as the maximum of the
	 *    individual blocks and store these values
	 *    in m.writeoffset and m.writelen.  This
	 *    write proceeds in four phases:
	 *
	 *    i.  Read the old contents of each block
	 *        and the parity block.
	 *    ii.  ``Remove'' the old contents from
	 *         the parity block with exclusive or.
	 *    iii. ``Insert'' the new contents of the
	 *          block in the parity block, again
	 *          with exclusive or.
	 *
	 *    iv.  Write the new contents of the data
	 *         blocks and the parity block.  The data
	 *         block transfers can be made directly from
	 *         the user buffer.
	 *
	 * 5. Degraded write where the data block is
	 *    not available.  The bounds of the
	 *    transfer are described by m.groupoffset
	 *    and m.grouplen. This requires the
	 *    following steps:
	 *
	 *    i.  Read in all the other data blocks,
	 *        excluding the parity block.
	 *
	 *    ii.  Recreate the parity block from the
	 *         other data blocks and the data to be
	 *         written.
	 *
	 *    iii. Write the parity block.
	 *
	 * 6. Parityless write, a write where the
	 *    parity block is not available.  This is
	 *    in fact the simplest: just write the
	 *    data blocks.  This can proceed directly
	 *    from the user buffer.  The bounds of the
	 *    transfer are described by m.dataoffset
	 *    and m.datalen.
	 *
	 * 7. Combination of degraded data block write
	 *    and normal write.  In this case the
	 *    address ranges of the reads may also
	 *    need to be extended to cover all
	 *    participating blocks.
	 *
	 * All requests in a group transfer transfer
	 * the same address range relative to their
	 * subdisk.  The individual transfers may
	 * vary, but since our group of requests is
	 * all in a single slice, we can define a
	 * range in which they all fall.
	 *
	 * In the following code section, we determine
	 * which kind of transfer we will perform.  If
	 * there is a group transfer, we also decide
	 * its bounds relative to the subdisks.  At
	 * the end, we have the following values:
	 *
	 *  m.flags indicates the kinds of transfers
	 *    we will perform.
	 *  m.initoffset indicates the offset of the
	 *    beginning of any data operation relative
	 *    to the beginning of the stripe base.
	 *  m.initlen specifies the length of any data
	 *    operation.
	 *  m.dataoffset contains the same value as
	 *    m.initoffset.
	 *  m.datalen contains the same value as
	 *    m.initlen.  Initially dataoffset and
	 *    datalen describe the parameters for the
	 *    first data block; while building the data
	 *    block requests, they are updated for each
	 *    block.
	 *  m.groupoffset indicates the offset of any
	 *    group operation relative to the beginning
	 *    of the stripe base.
	 *  m.grouplen specifies the length of any
	 *    group operation.
	 *  m.writeoffset indicates the offset of a
	 *    normal write relative to the beginning of
	 *    the stripe base.  This value differs from
	 *    m.dataoffset in that it applies to the
	 *    entire operation, and not just the first
	 *    block.
	 *  m.writelen specifies the total span of a
	 *    normal write operation.  writeoffset and
	 *    writelen are used to define the parity
	 *    block.
	 */
	m.groupoffset = 0;				    /* assume no group... */
	m.grouplen = 0;					    /* until we know we have one */
	m.writeoffset = m.initoffset;			    /* start offset of transfer */
	m.writelen = 0;					    /* nothing to write yet */
	m.flags = 0;					    /* no flags yet */
	rsectors = m.stripesectors;			    /* remaining sectors to examine */
	m.dataoffset = m.initoffset;			    /* start at the beginning of the transfer */
	m.datalen = m.initlen;

	if (m.sdcount > 1) {
	    plex->multiblock++;				    /* more than one block for the request */
	    /*
	     * If we have two transfers that don't overlap,
	     * (one at the end of the first block, the other
	     * at the beginning of the second block),
	     * it's cheaper to split them.
	     */
	    if (rsectors < plex->stripesize) {
		m.sdcount = 1;				    /* just one subdisk */
		m.stripesectors = m.initlen;		    /* and just this many sectors */
		rsectors = m.initlen;			    /* and in the loop counter */
	    }
	}
	if (SD[plex->sdnos[m.psdno]].state < sd_reborn)	    /* is our parity subdisk down? */
	    m.badsdno = m.psdno;			    /* note that it's down */
	if (bp->b_iocmd == BIO_READ) {			    /* read operation */
	    for (mysdno = m.firstsdno; rsectors > 0; mysdno++) {
		if (mysdno == m.psdno)			    /* ignore parity on read */
		    mysdno++;
		if (mysdno == plex->subdisks)		    /* wraparound */
		    mysdno = 0;
		if (mysdno == m.psdno)			    /* parity, */
		    mysdno++;				    /* we've given already */

		if (SD[plex->sdnos[mysdno]].state < sd_reborn) { /* got a bad subdisk, */
		    if (m.badsdno >= 0)			    /* we had one already, */
			return REQUEST_DOWN;		    /* we can't take a second */
		    m.badsdno = mysdno;			    /* got the first */
		    m.groupoffset = m.dataoffset;	    /* define the bounds */
		    m.grouplen = m.datalen;
		    m.flags |= XFR_RECOVERY_READ;	    /* we need recovery */
		    plex->recovered_reads++;		    /* count another one */
		} else
		    m.flags |= XFR_NORMAL_READ;		    /* normal read */

		/* Update the pointers for the next block */
		m.dataoffset = 0;			    /* back to the start of the stripe */
		rsectors -= m.datalen;			    /* remaining sectors to examine */
		m.datalen = min(rsectors, plex->stripesize); /* amount that will fit in this block */
	    }
	} else {					    /* write operation */
	    for (mysdno = m.firstsdno; rsectors > 0; mysdno++) {
		if (mysdno == m.psdno)			    /* parity stripe, we've dealt with that */
		    mysdno++;
		if (mysdno == plex->subdisks)		    /* wraparound */
		    mysdno = 0;
		if (mysdno == m.psdno)			    /* parity, */
		    mysdno++;				    /* we've given already */

		sd = &SD[plex->sdnos[mysdno]];
		if (sd->state != sd_up) {
		    enum requeststatus s;

		    s = checksdstate(sd, rq, *diskaddr, diskend); /* do we need to change state? */
		    if (s && (m.badsdno >= 0)) {	    /* second bad disk, */
			int sdno;
			/*
			 * If the parity disk is down, there's
			 * no recovery.  We make all involved
			 * subdisks stale.  Otherwise, we
			 * should be able to recover, but it's
			 * like pulling teeth.  Fix it later.
			 */
			for (sdno = 0; sdno < m.sdcount; sdno++) {
			    struct sd *sd = &SD[plex->sdnos[sdno]];
			    if (sd->state >= sd_reborn)	    /* sort of up, */
				set_sd_state(sd->sdno, sd_stale, setstate_force); /* make it stale */
			}
			return s;			    /* and crap out */
		    }
		    m.badsdno = mysdno;			    /* note which one is bad */
		    m.flags |= XFR_DEGRADED_WRITE;	    /* we need recovery */
		    plex->degraded_writes++;		    /* count another one */
		    m.groupoffset = m.dataoffset;	    /* define the bounds */
		    m.grouplen = m.datalen;
		} else {
		    m.flags |= XFR_NORMAL_WRITE;	    /* normal write operation */
		    if (m.writeoffset > m.dataoffset) {	    /* move write operation lower */
			m.writelen = max(m.writeoffset + m.writelen,
			    m.dataoffset + m.datalen)
			    - m.dataoffset;
			m.writeoffset = m.dataoffset;
		    } else
			m.writelen = max(m.writeoffset + m.writelen,
			    m.dataoffset + m.datalen)
			    - m.writeoffset;
		}

		/* Update the pointers for the next block */
		m.dataoffset = 0;			    /* back to the start of the stripe */
		rsectors -= m.datalen;			    /* remaining sectors to examine */
		m.datalen = min(rsectors, plex->stripesize); /* amount that will fit in this block */
	    }
	    if (m.badsdno == m.psdno) {			    /* got a bad parity block, */
		struct sd *psd = &SD[plex->sdnos[m.psdno]];

		if (psd->state == sd_down)
		    set_sd_state(psd->sdno, sd_obsolete, setstate_force); /* it's obsolete now */
		else if (psd->state == sd_crashed)
		    set_sd_state(psd->sdno, sd_stale, setstate_force); /* it's stale now */
		m.flags &= ~XFR_NORMAL_WRITE;		    /* this write isn't normal, */
		m.flags |= XFR_PARITYLESS_WRITE;	    /* it's parityless */
		plex->parityless_writes++;		    /* count another one */
	    }
	}

	/* reset the initial transfer values */
	m.dataoffset = m.initoffset;			    /* start at the beginning of the transfer */
	m.datalen = m.initlen;

	/* decide how many requests we need */
	if (m.flags & (XFR_RECOVERY_READ | XFR_DEGRADED_WRITE))
	    /* doing a recovery read or degraded write, */
	    m.rqcount = plex->subdisks;			    /* all subdisks */
	else if (m.flags & XFR_NORMAL_WRITE)		    /* normal write, */
	    m.rqcount = m.sdcount + 1;			    /* all data blocks and the parity block */
	else						    /* parityless write or normal read */
	    m.rqcount = m.sdcount;			    /* just the data blocks */

	/* Part C: build the requests */
	rqg = allocrqg(rq, m.rqcount);			    /* get a request group */
	if (rqg == NULL) {				    /* malloc failed */
	    bp->b_error = ENOMEM;
	    bp->b_ioflags |= BIO_ERROR;
	    return REQUEST_ENOMEM;
	}
	rqg->plexno = plexno;
	rqg->flags = m.flags;
	rqno = 0;					    /* index in the request group */

	/* 1: PARITY BLOCK */
	/*
	 * Are we performing an operation which requires parity?  In that case,
	 * work out the parameters and define the parity block.
	 * XFR_PARITYOP is XFR_NORMAL_WRITE | XFR_RECOVERY_READ | XFR_DEGRADED_WRITE
	 */
	if (m.flags & XFR_PARITYOP) {			    /* need parity */
	    rqe = &rqg->rqe[rqno];			    /* point to element */
	    sd = &SD[plex->sdnos[m.psdno]];		    /* the subdisk in question */
	    rqe->rqg = rqg;				    /* point back to group */
	    rqe->flags = (m.flags | XFR_PARITY_BLOCK | XFR_MALLOCED) /* always malloc parity block */
	    &~(XFR_NORMAL_READ | XFR_PARITYLESS_WRITE);	    /* transfer flags without data op stuf */
	    setrqebounds(rqe, &m);			    /* set up the bounds of the transfer */
	    rqe->sdno = sd->sdno;			    /* subdisk number */
	    rqe->driveno = sd->driveno;
	    if (build_rq_buffer(rqe, plex))		    /* build the buffer */
		return REQUEST_ENOMEM;			    /* can't do it */
	    rqe->b.b_iocmd = BIO_READ;			    /* we must read first */
	    m.sdcount++;				    /* adjust the subdisk count */
	    rqno++;					    /* and point to the next request */
	}
	/*
	 * 2: DATA BLOCKS
	 * Now build up requests for the blocks required
	 * for individual transfers
	 */
	for (mysdno = m.firstsdno; rqno < m.sdcount; mysdno++, rqno++) {
	    if (mysdno == m.psdno)			    /* parity, */
		mysdno++;				    /* we've given already */
	    if (mysdno == plex->subdisks)		    /* got to the end, */
		mysdno = 0;				    /* wrap around */
	    if (mysdno == m.psdno)			    /* parity, */
		mysdno++;				    /* we've given already */

	    rqe = &rqg->rqe[rqno];			    /* point to element */
	    sd = &SD[plex->sdnos[mysdno]];		    /* the subdisk in question */
	    rqe->rqg = rqg;				    /* point to group */
	    if (m.flags & XFR_NEEDS_MALLOC)		    /* we need a malloced buffer first */
		rqe->flags = m.flags | XFR_DATA_BLOCK | XFR_MALLOCED; /* transfer flags */
	    else
		rqe->flags = m.flags | XFR_DATA_BLOCK;	    /* transfer flags */
	    if (mysdno == m.badsdno) {			    /* this is the bad subdisk */
		rqg->badsdno = rqno;			    /* note which one */
		rqe->flags |= XFR_BAD_SUBDISK;		    /* note that it's dead */
		/*
		 * we can't read or write from/to it,
		 * but we don't need to malloc
		 */
		rqe->flags &= ~(XFR_MALLOCED | XFR_NORMAL_READ | XFR_NORMAL_WRITE);
	    }
	    setrqebounds(rqe, &m);			    /* set up the bounds of the transfer */
	    rqe->useroffset = m.useroffset;		    /* offset in user buffer */
	    rqe->sdno = sd->sdno;			    /* subdisk number */
	    rqe->driveno = sd->driveno;
	    if (build_rq_buffer(rqe, plex))		    /* build the buffer */
		return REQUEST_ENOMEM;			    /* can't do it */
	    if ((m.flags & XFR_PARITYOP)		    /* parity operation, */
	    &&((m.flags & XFR_BAD_SUBDISK) == 0))	    /* and not the bad subdisk, */
		rqe->b.b_iocmd = BIO_READ;		    /* we must read first */

	    /* Now update pointers for the next block */
	    *diskaddr += m.datalen;			    /* skip past what we've done */
	    m.stripesectors -= m.datalen;		    /* deduct from what's left */
	    m.useroffset += m.datalen;			    /* and move on in the user buffer */
	    m.datalen = min(m.stripesectors, plex->stripesize);	/* and recalculate */
	    m.dataoffset = 0;				    /* start at the beginning of next block */
	}

	/*
	 * 3: REMAINING BLOCKS FOR RECOVERY
	 * Finally, if we have a recovery operation, build
	 * up transfers for the other subdisks.  Follow the
	 * subdisks around until we get to where we started.
	 * These requests use only the group parameters.
	 */
	if ((rqno < m.rqcount)				    /* haven't done them all already */
	&&(m.flags & (XFR_RECOVERY_READ | XFR_DEGRADED_WRITE))) {
	    for (; rqno < m.rqcount; rqno++, mysdno++) {
		if (mysdno == m.psdno)			    /* parity, */
		    mysdno++;				    /* we've given already */
		if (mysdno == plex->subdisks)		    /* got to the end, */
		    mysdno = 0;				    /* wrap around */
		if (mysdno == m.psdno)			    /* parity, */
		    mysdno++;				    /* we've given already */

		rqe = &rqg->rqe[rqno];			    /* point to element */
		sd = &SD[plex->sdnos[mysdno]];		    /* the subdisk in question */
		rqe->rqg = rqg;				    /* point to group */

		rqe->sdoffset = m.sdbase + m.groupoffset;   /* start of transfer */
		rqe->dataoffset = 0;			    /* for tidiness' sake */
		rqe->groupoffset = 0;			    /* group starts at the beginining */
		rqe->datalen = 0;
		rqe->grouplen = m.grouplen;
		rqe->buflen = m.grouplen;
		rqe->flags = (m.flags | XFR_MALLOCED)	    /* transfer flags without data op stuf */
		&~XFR_DATAOP;
		rqe->sdno = sd->sdno;			    /* subdisk number */
		rqe->driveno = sd->driveno;
		if (build_rq_buffer(rqe, plex))		    /* build the buffer */
		    return REQUEST_ENOMEM;		    /* can't do it */
		rqe->b.b_iocmd = BIO_READ;		    /* we must read first */
	    }
	}
	/*
	 * We need to lock the address range before
	 * doing anything.  We don't have to be
	 * performing a recovery operation: somebody
	 * else could be doing so, and the results could
	 * influence us.  Note the fact here, we'll perform
	 * the lock in launch_requests.
	 */
	rqg->lockbase = m.stripebase;
	if (*diskaddr < diskend)			    /* didn't finish the request on this stripe */
	    plex->multistripe++;			    /* count another one */
    }
    return REQUEST_OK;
}

/*
 * Helper function for rqe5: adjust the bounds of
 * the transfers to minimize the buffer
 * allocation.
 *
 * Each request can handle two of three different
 * data ranges:
 *
 * 1.  The range described by the parameters
 *     dataoffset and datalen, for normal read or
 *     parityless write.
 * 2.  The range described by the parameters
 *     groupoffset and grouplen, for recovery read
 *     and degraded write.
 * 3.  For normal write, the range depends on the
 *     kind of block.  For data blocks, the range
 *     is defined by dataoffset and datalen.  For
 *     parity blocks, it is defined by writeoffset
 *     and writelen.
 *
 * In order not to allocate more memory than
 * necessary, this function adjusts the bounds
 * parameter for each request to cover just the
 * minimum necessary for the function it performs.
 * This will normally vary from one request to the
 * next.
 *
 * Things are slightly different for the parity
 * block.  In this case, the bounds defined by
 * mp->writeoffset and mp->writelen also play a
 * rôle.  Select this case by setting the
 * parameter forparity != 0.
 */
void
setrqebounds(struct rqelement *rqe, struct metrics *mp)
{
    /* parity block of a normal write */
    if ((rqe->flags & (XFR_NORMAL_WRITE | XFR_PARITY_BLOCK))
	== (XFR_NORMAL_WRITE | XFR_PARITY_BLOCK)) {	    /* case 3 */
	if (rqe->flags & XFR_DEGRADED_WRITE) {		    /* also degraded write */
	    /*
	     * With a combined normal and degraded write, we
	     * will zero out the area of the degraded write
	     * in the second phase, so we don't need to read
	     * it in.  Unfortunately, we need a way to tell
	     * build_request_buffer the size of the buffer,
	     * and currently that's the length of the read.
	     * As a result, we read everything, even the stuff
	     * that we're going to nuke.
	     * FIXME XXX
	     */
	    if (mp->groupoffset < mp->writeoffset) {	    /* group operation starts lower */
		rqe->sdoffset = mp->sdbase + mp->groupoffset; /* start of transfer */
		rqe->dataoffset = mp->writeoffset - mp->groupoffset; /* data starts here */
		rqe->groupoffset = 0;			    /* and the group at the beginning */
	    } else {					    /* individual data starts first */
		rqe->sdoffset = mp->sdbase + mp->writeoffset; /* start of transfer */
		rqe->dataoffset = 0;			    /* individual data starts at the beginning */
		rqe->groupoffset = mp->groupoffset - mp->writeoffset; /* group starts here */
	    }
	    rqe->datalen = mp->writelen;
	    rqe->grouplen = mp->grouplen;
	} else {					    /* just normal write (case 3) */
	    rqe->sdoffset = mp->sdbase + mp->writeoffset;   /* start of transfer */
	    rqe->dataoffset = 0;			    /* degradation starts at the beginning */
	    rqe->groupoffset = 0;			    /* for tidiness' sake */
	    rqe->datalen = mp->writelen;
	    rqe->grouplen = 0;
	}
    } else if (rqe->flags & XFR_DATAOP) {		    /* data operation (case 1 or 3) */
	if (rqe->flags & XFR_GROUPOP) {			    /* also a group operation (case 2) */
	    if (mp->groupoffset < mp->dataoffset) {	    /* group operation starts lower */
		rqe->sdoffset = mp->sdbase + mp->groupoffset; /* start of transfer */
		rqe->dataoffset = mp->dataoffset - mp->groupoffset; /* data starts here */
		rqe->groupoffset = 0;			    /* and the group at the beginning */
	    } else {					    /* individual data starts first */
		rqe->sdoffset = mp->sdbase + mp->dataoffset; /* start of transfer */
		rqe->dataoffset = 0;			    /* individual data starts at the beginning */
		rqe->groupoffset = mp->groupoffset - mp->dataoffset; /* group starts here */
	    }
	    rqe->datalen = mp->datalen;
	    rqe->grouplen = mp->grouplen;
	} else {					    /* just data operation (case 1) */
	    rqe->sdoffset = mp->sdbase + mp->dataoffset;    /* start of transfer */
	    rqe->dataoffset = 0;			    /* degradation starts at the beginning */
	    rqe->groupoffset = 0;			    /* for tidiness' sake */
	    rqe->datalen = mp->datalen;
	    rqe->grouplen = 0;
	}
    } else {						    /* just group operations (case 2) */
	rqe->sdoffset = mp->sdbase + mp->groupoffset;	    /* start of transfer */
	rqe->dataoffset = 0;				    /* for tidiness' sake */
	rqe->groupoffset = 0;				    /* group starts at the beginining */
	rqe->datalen = 0;
	rqe->grouplen = mp->grouplen;
    }
    rqe->buflen = max(rqe->dataoffset + rqe->datalen,	    /* total buffer length */
	rqe->groupoffset + rqe->grouplen);
}
/* Local Variables: */
/* fill-column: 50 */
/* End: */
