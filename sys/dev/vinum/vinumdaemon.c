/* daemon.c: kernel part of Vinum daemon */
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
 * $Id: vinumdaemon.c,v 1.8 2000/01/03 05:22:03 grog Exp grog $
 * $FreeBSD$
 */

#include <dev/vinum/vinumhdr.h>
#include <dev/vinum/request.h>

#ifdef VINUMDEBUG
#include <sys/reboot.h>
#endif

/* declarations */
void recover_io(struct request *rq);

int daemon_options = 0;					    /* options */
int daemonpid;						    /* PID of daemon */
struct daemonq *daemonq;				    /* daemon's work queue */
struct daemonq *dqend;					    /* and the end of the queue */

/*
 * We normally call Malloc to get a queue element.  In interrupt
 * context, we can't guarantee that we'll get one, since we're not
 * allowed to wait.  If malloc fails, use one of these elements.
 */

#define INTQSIZE 4
struct daemonq intq[INTQSIZE];				    /* queue elements for interrupt context */
struct daemonq *intqp;					    /* and pointer in it */

void
vinum_daemon(void)
{
    int s;
    struct daemonq *request;

    PROC_LOCK(curproc);
    curproc->p_flag |= P_SYSTEM;			    /* we're a system process */
    PROC_UNLOCK(curproc);
    mtx_lock_spin(&sched_lock);
    curproc->p_sflag |= PS_INMEM;
    mtx_unlock_spin(&sched_lock);
    daemon_save_config();				    /* start by saving the configuration */
    daemonpid = curproc->p_pid;				    /* mark our territory */
    while (1) {
	tsleep(&vinum_daemon, PRIBIO, "vinum", 0);	    /* wait for something to happen */

	/*
	 * It's conceivable that, as the result of an
	 * I/O error, we'll be out of action long
	 * enough that another daemon gets started.
	 * That's OK, just give up gracefully.
	 */
	if (curproc->p_pid != daemonpid) {		    /* we've been ousted in our sleep */
	    if (daemon_options & daemon_verbose)
		log(LOG_INFO, "vinum: abdicating\n");
	    return;
	}
	while (daemonq != NULL) {			    /* we have work to do, */
	    s = splhigh();				    /* don't get interrupted here */
	    request = daemonq;				    /* get the request */
	    daemonq = daemonq->next;			    /* and detach it */
	    if (daemonq == NULL)			    /* got to the end, */
		dqend = NULL;				    /* no end any more */
	    splx(s);

	    switch (request->type) {
		/*
		 * We had an I/O error on a request.  Go through the
		 * request and try to salvage it
		 */
	    case daemonrq_ioerror:
		if (daemon_options & daemon_verbose) {
		    struct request *rq = request->info.rq;

		    log(LOG_WARNING,
			"vinum: recovering I/O request: %p\n%s dev %d.%d, offset 0x%llx, length %ld\n",
			rq,
			rq->bp->b_iocmd == BIO_READ ? "Read" : "Write",
			major(rq->bp->b_dev),
			minor(rq->bp->b_dev),
			rq->bp->b_blkno,
			rq->bp->b_bcount);
		}
		recover_io(request->info.rq);		    /* the failed request */
		break;

		/*
		 * Write the config to disk.  We could end up with
		 * quite a few of these in a row.  Only honour the
		 * last one
		 */
	    case daemonrq_saveconfig:
		if ((daemonq == NULL)			    /* no more requests */
		||(daemonq->type != daemonrq_saveconfig)) { /* or the next isn't the same */
		    if (((daemon_options & daemon_noupdate) == 0) /* we're allowed to do it */
		    &&((vinum_conf.flags & VF_READING_CONFIG) == 0)) { /* and we're not building the config now */
			/*
			   * We obviously don't want to save a
			   * partial configuration.  Less obviously,
			   * we don't need to do anything if we're
			   * asked to write the config when we're
			   * building it up, because we save it at
			   * the end.
			 */
			if (daemon_options & daemon_verbose)
			    log(LOG_INFO, "vinum: saving config\n");
			daemon_save_config();		    /* save it */
		    }
		}
		break;

	    case daemonrq_return:			    /* been told to stop */
		if (daemon_options & daemon_verbose)
		    log(LOG_INFO, "vinum: stopping\n");
		daemon_options |= daemon_stopped;	    /* note that we've stopped */
		Free(request);
		while (daemonq != NULL) {		    /* backed up requests, */
		    request = daemonq;			    /* get the request */
		    daemonq = daemonq->next;		    /* and detach it */
		    Free(request);			    /* then free it */
		}
		wakeup(&vinumclose);			    /* and wake any waiting vinum(8)s */
		return;

	    case daemonrq_ping:				    /* tell the caller we're here */
		if (daemon_options & daemon_verbose)
		    log(LOG_INFO, "vinum: ping reply\n");
		wakeup(&vinum_finddaemon);		    /* wake up the caller */
		break;

	    case daemonrq_closedrive:			    /* close a drive */
		close_drive(request->info.drive);	    /* do it */
		break;

	    case daemonrq_init:				    /* initialize a plex */
		/* XXX */
	    case daemonrq_revive:			    /* revive a subdisk */
		/* XXX */
		/* FALLTHROUGH */
	    default:
		log(LOG_WARNING, "Invalid request\n");
		break;
	    }
	    if (request->privateinuse)			    /* one of ours, */
		request->privateinuse = 0;		    /* no longer in use */
	    else
		Free(request);				    /* return it */
	}
    }
}

/*
 * Recover a failed I/O operation.
 *
 * The correct way to do this is to examine the request and determine
 * how to recover each individual failure.  In the case of a write,
 * this could be as simple as doing nothing: the defective drives may
 * already be down, and there may be nothing else to do.  In case of
 * a read, it will be necessary to retry if there are alternative
 * copies of the data.
 *
 * The easy way (here) is just to reissue the request.  This will take
 * a little longer, but nothing like as long as the failure will have
 * taken.
 *
 */
void
recover_io(struct request *rq)
{
    /*
     * This should read:
     *
     *     vinumstrategy(rq->bp);
     *
     * Negotiate with phk to get it fixed.
     */
    DEV_STRATEGY(rq->bp, 0);				    /* reissue the command */
}

/* Functions called to interface with the daemon */

/* queue a request for the daemon */
void
queue_daemon_request(enum daemonrq type, union daemoninfo info)
{
    int s;

    struct daemonq *qelt = (struct daemonq *) Malloc(sizeof(struct daemonq));

    if (qelt == NULL) {					    /* malloc failed, we're prepared for that */
	/*
	 * Take one of our spares.  Give up if it's still in use; the only
	 * message we're likely to get here is a 'drive failed' message,
	 * and that'll come by again if we miss it.
	 */
	if (intqp->privateinuse)			    /* still in use? */
	    return;					    /* yes, give up */
	qelt = intqp++;
	if (intqp == &intq[INTQSIZE])			    /* got to the end, */
	    intqp = intq;				    /* wrap around */
	qelt->privateinuse = 1;				    /* it's ours, and it's in use */
    } else
	qelt->privateinuse = 0;

    qelt->next = NULL;					    /* end of the chain */
    qelt->type = type;
    qelt->info = info;
    s = splhigh();
    if (daemonq) {					    /* something queued already */
	dqend->next = qelt;
	dqend = qelt;
    } else {						    /* queue is empty, */
	daemonq = qelt;					    /* this is the whole queue */
	dqend = qelt;
    }
    splx(s);
    wakeup(&vinum_daemon);				    /* and give the dæmon a kick */
}

/*
 * see if the daemon is running.  Return 0 (no error)
 * if it is, ESRCH otherwise
 */
int
vinum_finddaemon()
{
    int result;

    if (daemonpid != 0) {				    /* we think we have a daemon, */
	queue_daemon_request(daemonrq_ping, (union daemoninfo) 0); /* queue a ping */
	result = tsleep(&vinum_finddaemon, PUSER, "reap", 2 * hz);
	if (result == 0)				    /* yup, the daemon's up and running */
	    return 0;
    }
    /* no daemon, or we couldn't talk to it: start it */
    vinum_daemon();					    /* start the daemon */
    return 0;
}

int
vinum_setdaemonopts(int options)
{
    daemon_options = options;
    return 0;
}
