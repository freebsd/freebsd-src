/* interrupt.c: bottom half of the driver */

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
 * $Id: vinuminterrupt.c,v 1.5 1999/03/16 03:40:25 grog Exp grog $
 */

#define REALLYKERNEL
#include "opt_vinum.h"
#include <dev/vinum/vinumhdr.h>
#include <dev/vinum/request.h>
#include <miscfs/specfs/specdev.h>
#include <sys/resourcevar.h>

void complete_raid5_write(struct rqelement *);
void freerq(struct request *rq);
void free_rqg(struct rqgroup *rqg);
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

    rqe = (struct rqelement *) bp;			    /* point to the element element that completed */
    rqg = rqe->rqg;					    /* and the request group */
    rq = rqg->rq;					    /* and the complete request */
    ubp = rq->bp;					    /* user buffer */

#ifdef VINUMDEBUG
    if (debug & DEBUG_LASTREQS)
	logrq(loginfo_iodone, (union rqinfou) rqe, ubp);
#endif
    if ((bp->b_flags & B_ERROR) != 0) {			    /* transfer in error */
	if (bp->b_error != 0)				    /* did it return a number? */
	    rq->error = bp->b_error;			    /* yes, put it in. */
	else if (rq->error == 0)			    /* no: do we have one already? */
	    rq->error = EIO;				    /* no: catchall "I/O error" */
	SD[rqe->sdno].lasterror = rq->error;
	if (bp->b_flags & B_READ) {
	    log(LOG_ERR, "%s: fatal read I/O error\n", SD[rqe->sdno].name);
	    set_sd_state(rqe->sdno, sd_crashed, setstate_force); /* subdisk is crashed */
	} else {					    /* write operation */
	    log(LOG_ERR, "%s: fatal write I/O error\n", SD[rqe->sdno].name);
	    set_sd_state(rqe->sdno, sd_stale, setstate_force); /* subdisk is stale */
	}
	if (rq->error == ENXIO) {			    /* the drive's down too */
	    log(LOG_ERR, "%s: fatal drive I/O error\n", DRIVE[rqe->driveno].label.name);
	    DRIVE[rqe->driveno].lasterror = rq->error;
	    set_drive_state(rqe->driveno,		    /* take the drive down */
		drive_down,
		setstate_force);
	}
    }
    /* Now update the statistics */
    if (bp->b_flags & B_READ) {				    /* read operation */
	DRIVE[rqe->driveno].reads++;
	DRIVE[rqe->driveno].bytes_read += bp->b_bcount;
	SD[rqe->sdno].reads++;
	SD[rqe->sdno].bytes_read += bp->b_bcount;
	PLEX[rqe->rqg->plexno].reads++;
	PLEX[rqe->rqg->plexno].bytes_read += bp->b_bcount;
    } else {						    /* write operation */
	DRIVE[rqe->driveno].writes++;
	DRIVE[rqe->driveno].bytes_written += bp->b_bcount;
	SD[rqe->sdno].writes++;
	SD[rqe->sdno].bytes_written += bp->b_bcount;
	PLEX[rqe->rqg->plexno].writes++;
	PLEX[rqe->rqg->plexno].bytes_written += bp->b_bcount;
    }
    rqg->active--;					    /* one less request active */
    if (rqg->active == 0)				    /* request group finished, */
	rq->active--;					    /* one less */
    if (rq->active == 0) {				    /* request finished, */
#if VINUMDEBUG
	if (debug & DEBUG_RESID) {
	    if (ubp->b_resid != 0)			    /* still something to transfer? */
		Debugger("resid");

	    {
		int i;
		for (i = 0; i < ubp->b_bcount; i += 512)    /* XXX debug */
		    if (((char *) ubp->b_data)[i] != '<') { /* and not what we expected */
			log(LOG_DEBUG,
			    "At 0x%x (offset 0x%x): '%c' (0x%x)\n",
			    (int) (&((char *) ubp->b_data)[i]),
			    i,
			    ((char *) ubp->b_data)[i],
			    ((char *) ubp->b_data)[i]);
			Debugger("complete_request checksum");
		    }
	    }
	}
#endif

	if (rq->error) {				    /* did we have an error? */
	    if (rq->isplex) {				    /* plex operation, */
		ubp->b_flags |= B_ERROR;		    /* yes, propagate to user */
		ubp->b_error = rq->error;
	    } else					    /* try to recover */
		queue_daemon_request(daemonrq_ioerror, (union daemoninfo) rq); /* let the daemon complete */
	} else {
	    ubp->b_resid = 0;				    /* completed our transfer */
	    if (rq->isplex == 0)			    /* volume request, */
		VOL[rq->volplex.volno].active--;	    /* another request finished */
	    biodone(ubp);				    /* top level buffer completed */
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
	for (rqno = 0; rqno < rqg->count; rqno++)
	    if ((rqg->rqe[rqno].flags & XFR_MALLOCED)	    /* data buffer was malloced, */
	    &&rqg->rqe[rqno].b.b_data)			    /* and the allocation succeeded */
		Free(rqg->rqe[rqno].b.b_data);		    /* free it */
	nrqg = rqg->next;				    /* note the next one */
	Free(rqg);					    /* and free this one */
    }
    Free(rq);						    /* free the request itself */
}

void 
free_rqg(struct rqgroup *rqg)
{
    if ((rqg->flags & XFR_GROUPOP)			    /* RAID 5 request */
&&(rqg->rqe) /* got a buffer structure */
    &&(rqg->rqe->b.b_data))				    /* and it has a buffer allocated */
	Free(rqg->rqe->b.b_data);			    /* free it */
}

/* I/O on subdisk completed */
void 
sdio_done(struct buf *bp)
{
    struct sdbuf *sbp;

    sbp = (struct sdbuf *) bp;
    if (sbp->b.b_flags & B_ERROR) {			    /* had an error */
	bp->b_flags |= B_ERROR;
	bp->b_error = sbp->b.b_error;
    }
    bp->b_resid = sbp->b.b_resid;
    biodone(sbp->bp);					    /* complete the caller's I/O */
    /* Now update the statistics */
    if (bp->b_flags & B_READ) {				    /* read operation */
	DRIVE[sbp->driveno].reads++;
	DRIVE[sbp->driveno].bytes_read += bp->b_bcount;
	SD[sbp->sdno].reads++;
	SD[sbp->sdno].bytes_read += bp->b_bcount;
    } else {						    /* write operation */
	DRIVE[sbp->driveno].writes++;
	DRIVE[sbp->driveno].bytes_written += bp->b_bcount;
	SD[sbp->sdno].writes++;
	SD[sbp->sdno].bytes_written += bp->b_bcount;
    }
    Free(sbp);
}
