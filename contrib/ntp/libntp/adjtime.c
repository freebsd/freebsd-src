#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef NEED_HPUX_ADJTIME
/*************************************************************************/
/* (c) Copyright Tai Jin, 1988.  All Rights Reserved.                    */
/*     Hewlett-Packard Laboratories.                                     */
/*                                                                       */
/* Permission is hereby granted for unlimited modification, use, and     */
/* distribution.  This software is made available with no warranty of    */
/* any kind, express or implied.  This copyright notice must remain      */
/* intact in all versions of this software.                              */
/*                                                                       */
/* The author would appreciate it if any bug fixes and enhancements were */
/* to be sent back to him for incorporation into future versions of this */
/* software.  Please send changes to tai@iag.hp.com or ken@sdd.hp.com.   */
/*************************************************************************/

/*
 * Revision history
 *
 * 9 Jul 94	David L. Mills, Unibergity of Delabunch
 *		Implemented variable threshold to limit age of
 *		corrections; reformatted code for readability.
 */

#ifndef lint
static char RCSid[] = "adjtime.c,v 3.1 1993/07/06 01:04:42 jbj Exp";
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>
#include <signal.h>
#include "adjtime.h"

#define abs(x)  ((x) < 0 ? -(x) : (x))

/*
 * The following paramters are appropriate for an NTP adjustment
 * interval of one second.
 */
#define ADJ_THRESH 200		/* initial threshold */
#define ADJ_DELTA 4		/* threshold decrement */

static long adjthresh;		/* adjustment threshold */
static long saveup;		/* corrections accumulator */

/*
 * clear_adjtime - reset accumulator and threshold variables
 */
void
_clear_adjtime(void)
{
	saveup = 0;
	adjthresh = ADJ_THRESH;
}

/*
 * adjtime - hp-ux copout of the standard Unix adjtime() system call
 */
int
adjtime(
	register struct timeval *delta,
	register struct timeval *olddelta
	)
{
	struct timeval newdelta;

	/*
	 * Corrections greater than one second are done immediately.
	 */
	if (delta->tv_sec) {
		adjthresh = ADJ_THRESH;
		saveup = 0;
		return(_adjtime(delta, olddelta));
	}

	/*
	 * Corrections less than one second are accumulated until
	 * tripping a threshold, which is initially set at ADJ_THESH and
	 * reduced in ADJ_DELTA steps to zero. The idea here is to
	 * introduce large corrections quickly, while making sure that
	 * small corrections are introduced without excessive delay. The
	 * idea comes from the ARPAnet routing update algorithm.
	 */
	saveup += delta->tv_usec;
	if (abs(saveup) >= adjthresh) {
		adjthresh = ADJ_THRESH;
		newdelta.tv_sec = 0;
		newdelta.tv_usec = saveup;
		saveup = 0;
		return(_adjtime(&newdelta, olddelta));
	} else {
		adjthresh -= ADJ_DELTA;
	}

	/*
	 * While nobody uses it, return the residual before correction,
	 * as per Unix convention.
	 */
	if (olddelta)
	    olddelta->tv_sec = olddelta->tv_usec = 0;
	return(0);
}

/*
 * _adjtime - does the actual work
 */
int
_adjtime(
	register struct timeval *delta,
	register struct timeval *olddelta
	)
{
	register int mqid;
	MsgBuf msg;
	register MsgBuf *msgp = &msg;

	/*
	 * Get the key to the adjtime message queue (note that we must
	 * get it every time because the queue might have been removed
	 * and recreated)
	 */
	if ((mqid = msgget(KEY, 0)) == -1)
	    return (-1);
	msgp->msgb.mtype = CLIENT;
	msgp->msgb.tv = *delta;
	if (olddelta)
	    msgp->msgb.code = DELTA2;
	else
	    msgp->msgb.code = DELTA1;

	/*
	 * Tickle adjtimed and snatch residual, if indicated. Lots of
	 * fanatic error checking here.
	 */
	if (msgsnd(mqid, &msgp->msgp, MSGSIZE, 0) == -1)
	    return (-1);
	if (olddelta) {
		if (msgrcv(mqid, &msgp->msgp, MSGSIZE, SERVER, 0) == -1)
		    return (-1);
		*olddelta = msgp->msgb.tv;
	}
	return (0);
}

#else /* not NEED_HPUX_ADJTIME */
int adjtime_bs;
#endif /* not NEED_HPUX_ADJTIME */
