/* 
 * tclUnixEvent.c --
 *
 *	This file implements Unix specific event related routines.
 *
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclUnixEvent.c 1.1 97/03/04 14:19:34
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Sleep --
 *
 *	Delay execution for the specified number of milliseconds.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Time passes.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_Sleep(ms)
    int ms;			/* Number of milliseconds to sleep. */
{
    static struct timeval delay;
    Tcl_Time before, after;

    /*
     * The only trick here is that select appears to return early
     * under some conditions, so we have to check to make sure that
     * the right amount of time really has elapsed.  If it's too
     * early, go back to sleep again.
     */

    TclpGetTime(&before);
    after = before;
    after.sec += ms/1000;
    after.usec += (ms%1000)*1000;
    if (after.usec > 1000000) {
	after.usec -= 1000000;
	after.sec += 1;
    }
    while (1) {
	delay.tv_sec = after.sec - before.sec;
	delay.tv_usec = after.usec - before.usec;
	if (delay.tv_usec < 0) {
	    delay.tv_usec += 1000000;
	    delay.tv_sec -= 1;
	}

	/*
	 * Special note:  must convert delay.tv_sec to int before comparing
	 * to zero, since delay.tv_usec is unsigned on some platforms.
	 */

	if ((((int) delay.tv_sec) < 0)
		|| ((delay.tv_usec == 0) && (delay.tv_sec == 0))) {
	    break;
	}
	(void) select(0, (SELECT_MASK *) 0, (SELECT_MASK *) 0,
		(SELECT_MASK *) 0, &delay);
	TclpGetTime(&before);
    }
}
