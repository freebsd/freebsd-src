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
static LONG adjthresh = 400L;
static LONG saveup;


_clear_adjtime()
{
    saveup = 0L;
}


adjtime(delta, olddelta)
    register struct timeval *delta;
    register struct timeval *olddelta;
{
    struct timeval newdelta;

    /* If they are giving us seconds, ignore up to current threshold saved */
    if (delta->tv_sec) {
	saveup = 0L;
	return(_adjtime(delta, olddelta));
    }

    /* add in, needs check for overflow ? */
    saveup += delta->tv_usec;

    /* Broke the threshold, call adjtime() */
    if (abs(saveup) > adjthresh) {
	newdelta.tv_sec = 0L;
	newdelta.tv_usec = saveup;
	saveup = 0L;
	return(_adjtime(&newdelta, olddelta));
    }

    if (olddelta)
      olddelta->tv_sec = olddelta->tv_usec = 0L;
    return(0);
}


_adjtime(delta, olddelta)
    register struct timeval *delta;
    register struct timeval *olddelta;
{
    register int mqid;
    MsgBuf msg;
    register MsgBuf *msgp = &msg;

    /*
     * get the key to the adjtime message queue
     * (note that we must get it every time because the queue might have been
     *  removed and recreated)
     */
    if ((mqid = msgget(KEY, 0)) == -1)
	return (-1);

    msgp->msgb.mtype = CLIENT;
    msgp->msgb.tv = *delta;

    if (olddelta)
	msgp->msgb.code = DELTA2;
    else
	msgp->msgb.code = DELTA1;

    if (msgsnd(mqid, &msgp->msgp, MSGSIZE, 0) == -1)
	return (-1);

    if (olddelta) {
	if (msgrcv(mqid, &msgp->msgp, MSGSIZE, SERVER, 0) == -1)
	    return (-1);

	*olddelta = msgp->msgb.tv;
    }

    return (0);
}
