/* 
 * tclUnixTime.c --
 *
 *	Contains Unix specific versions of Tcl functions that
 *	obtain time values from the operating system.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclUnixTime.c 1.10 96/02/15 11:58:41
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 *-----------------------------------------------------------------------------
 *
 * TclGetSeconds --
 *
 *	This procedure returns the number of seconds from the epoch.  On
 *	most Unix systems the epoch is Midnight Jan 1, 1970 GMT.
 *
 * Results:
 *	Number of seconds from the epoch.
 *
 * Side effects:
 *	None.
 *
 *-----------------------------------------------------------------------------
 */

unsigned long
TclGetSeconds()
{
    return time((time_t *) NULL);
}

/*
 *-----------------------------------------------------------------------------
 *
 * TclGetClicks --
 *
 *	This procedure returns a value that represents the highest resolution
 *	clock available on the system.  There are no garantees on what the
 *	resolution will be.  In Tcl we will call this value a "click".  The
 *	start time is also system dependant.
 *
 * Results:
 *	Number of clicks from some start time.
 *
 * Side effects:
 *	None.
 *
 *-----------------------------------------------------------------------------
 */

unsigned long
TclGetClicks()
{
    unsigned long now;
#ifdef NO_GETTOD
    struct tms dummy;
#else
    struct timeval date;
    struct timezone tz;
#endif

#ifdef NO_GETTOD
    now = (unsigned long) times(&dummy);
#else
    gettimeofday(&date, &tz);
    now = date.tv_sec*1000000 + date.tv_usec;
#endif

    return now;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetTimeZone --
 *
 *	Determines the current timezone.  The method varies wildly
 *	between different platform implementations, so its hidden in
 *	this function.
 *
 * Results:
 *	Hours east of GMT.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclGetTimeZone (currentTime)
    unsigned long  currentTime;
{
    /*
     * Determine how a timezone is obtained from "struct tm".  If there is no
     * time zone in this struct (very lame) then use the timezone variable.
     * This is done in a way to make the timezone variable the method of last
     * resort, as some systems have it in addition to a field in "struct tm".
     * The gettimeofday system call can also be used to determine the time
     * zone.
     */
    
#if defined(HAVE_TM_TZADJ)
#   define TCL_GOT_TIMEZONE
    time_t      curTime = (time_t) currentTime;
    struct tm  *timeDataPtr = localtime(&curTime);
    int         timeZone;

    timeZone = timeDataPtr->tm_tzadj  / 60;
    if (timeDataPtr->tm_isdst) {
        timeZone += 60;
    }
    
    return timeZone;
#endif

#if defined(HAVE_TM_GMTOFF) && !defined (TCL_GOT_TIMEZONE)
#   define TCL_GOT_TIMEZONE
    time_t     curTime = (time_t) currentTime;
    struct tm *timeDataPtr = localtime(&currentTime);
    int        timeZone;

    timeZone = -(timeDataPtr->tm_gmtoff / 60);
    if (timeDataPtr->tm_isdst) {
        timeZone += 60;
    }
    
    return timeZone;
#endif

    /*
     * Must prefer timezone variable over gettimeofday, as gettimeofday does
     * not return timezone information on many systems that have moved this
     * information outside of the kernel.
     */
    
#if defined(HAVE_TIMEZONE_VAR) && !defined (TCL_GOT_TIMEZONE)
#   define TCL_GOT_TIMEZONE
    static int setTZ = 0;
    int        timeZone;

    if (!setTZ) {
        tzset();
        setTZ = 1;
    }

    /*
     * Note: this is not a typo in "timezone" below!  See tzset
     * documentation for details.
     */

    timeZone = timezone / 60;

    return timeZone;
#endif

#if defined(HAVE_GETTIMEOFDAY) && !defined (TCL_GOT_TIMEZONE)
#   define TCL_GOT_TIMEZONE
    struct timeval  tv;
    struct timezone tz;
    int timeZone;

    gettimeofday(&tv, &tz);
    timeZone = tz.tz_minuteswest;
    if (tz.tz_dsttime) {
        timeZone += 60;
    }
    
    return timeZone;
#endif

#ifndef TCL_GOT_TIMEZONE
    /*
     * Cause compile error, we don't know how to get timezone.
     */
    error: autoconf did not figure out how to determine the timezone. 
#endif

}

/*
 *----------------------------------------------------------------------
 *
 * TclGetTime --
 *
 *	Gets the current system time in seconds and microseconds
 *	since the beginning of the epoch: 00:00 UCT, January 1, 1970.
 *
 * Results:
 *	Returns the current time in timePtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TclGetTime(timePtr)
    Tcl_Time *timePtr;		/* Location to store time information. */
{
    struct timeval tv;
    struct timezone tz;
    
    (void) gettimeofday(&tv, &tz);
    timePtr->sec = tv.tv_sec;
    timePtr->usec = tv.tv_usec;
}
