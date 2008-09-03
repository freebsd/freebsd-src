/*  Copyright (C) 1996 N.M. Maclaren
    Copyright (C) 1996 The University of Cambridge

This includes all of the code needed to handle the time.  It assumes rather
more than is defined by POSIX, unfortunately.  Systems that do not have the
'X/Open' extensions may need changes. */



#include "header.h"

#include <sys/types.h>
#include <sys/time.h>

#define TIMING
#include "kludges.h"
#undef TIMING



#define MILLION_L    1000000l          /* For conversion to/from timeval */
#define MILLION_D       1.0e6          /* Must be equal to MILLION_L */



double current_time (double offset) {

/* Get the current UTC time in seconds since the Epoch plus an offset (usually
the time from the beginning of the century to the Epoch!) */

    struct timeval current;

    errno = 0;
    if (gettimeofday(&current,NULL))
        fatal(1,"unable to read current machine/system time",NULL);
    return offset+current.tv_sec+1.0e-6*current.tv_usec;
}



time_t convert_time (double value, int *millisecs) {

/* Convert the time to the ANSI C form. */

    time_t result = (time_t)value;

    if ((*millisecs = (int)(1000.0*(value-result))) >= 1000) {
        *millisecs = 0;
        ++result;
    }
    return result;
}



void adjust_time (double difference, int immediate, double ignore) {

/* Adjust the current UTC time.  This is portable, even if struct timeval uses
an unsigned long for tv_sec. */

    struct timeval old, new, adjust, previous;
    char text[40];
    long n;

/* Start by converting to timeval format. Note that we have to cater for
negative, unsigned values. */

    if ((n = (long)difference) > difference) --n;
    adjust.tv_sec = n;
    adjust.tv_usec = (long)(MILLION_D*(difference-n));
    errno = 0;
    if (gettimeofday(&old,NULL))
        fatal(1,"unable to read machine/system time",NULL);
    new.tv_sec = old.tv_sec+adjust.tv_sec;
    new.tv_usec = (n = (long)old.tv_usec+(long)adjust.tv_usec);
    if (n < 0) {
        new.tv_usec += MILLION_L;
        --new.tv_sec;
    } else if (n >= MILLION_L) {
        new.tv_usec -= MILLION_L;
        ++new.tv_sec;
    }

/* Now diagnose the situation if necessary, and perform the dirty deed. */

    if (verbose > 2)
        fprintf(stderr,
            "Times: old=(%ld,%.6ld) new=(%ld,%.6ld) adjust=(%ld,%.6ld)\n",
            (long)old.tv_sec,(long)old.tv_usec,
            (long)new.tv_sec,(long)new.tv_usec,
            (long)adjust.tv_sec,(long)adjust.tv_usec);
    if (immediate) {
        errno = 0;
        if (settimeofday(&new,NULL))
            fatal(1,"unable to reset current system time",NULL);
    } else {
	previous.tv_sec  = 0;
	previous.tv_usec = 0;
        errno = 0;
        if (adjtime(&adjust,&previous))
            fatal(1,"unable to adjust current system time",NULL);
        if (previous.tv_sec != 0 || previous.tv_usec != 0) {
            sprintf(text,"(%ld,%.6ld)",
                (long)previous.tv_sec,(long)previous.tv_usec);
            if (previous.tv_sec+1.0e-6*previous.tv_usec > ignore)
                fatal(0,"outstanding time adjustment %s",text);
            else if (verbose)
                fprintf(stderr,"%s: outstanding time adjustment %s\n",
                    argv0,text);
        }
    }
}
