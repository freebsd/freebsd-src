/* 
 * tclMatherr.c --
 *
 *	This function provides a default implementation of the
 *	"matherr" function, for SYS-V systems where it's needed.
 *
 * Copyright (c) 1993-1994 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMtherr.c 1.12 96/06/22 16:36:57
 */

#include "tclInt.h"
#include <math.h>

#ifndef TCL_GENERIC_ONLY
#include "tclPort.h"
#else
#define NO_ERRNO_H
#endif

#ifdef NO_ERRNO_H
extern int errno;			/* Use errno from tclExecute.c. */
#define EDOM 33
#define ERANGE 34
#endif

/*
 * The following variable is secretly shared with Tcl so we can
 * tell if expression evaluation is in progress.  If not, matherr
 * just emulates the default behavior, which includes printing
 * a message.
 */

extern int tcl_MathInProgress;

/*
 * The following definitions allow matherr to compile on systems
 * that don't really support it.  The compiled procedure is bogus,
 * but it will never be executed on these systems anyway.
 */

#ifndef NEED_MATHERR
struct exception {
    int type;
};
#define DOMAIN 0
#define SING 0
#endif

/*
 *----------------------------------------------------------------------
 *
 * matherr --
 *
 *	This procedure is invoked on Sys-V systems when certain
 *	errors occur in mathematical functions.  Type "man matherr"
 *	for more information on how this function works.
 *
 * Results:
 *	Returns 1 to indicate that we've handled the error
 *	locally.
 *
 * Side effects:
 *	Sets errno based on what's in xPtr.
 *
 *----------------------------------------------------------------------
 */

int
matherr(xPtr)
    struct exception *xPtr;	/* Describes error that occurred. */
{
    if (!tcl_MathInProgress) {
	return 0;
    }
    if ((xPtr->type == DOMAIN) || (xPtr->type == SING)) {
	errno = EDOM;
    } else {
	errno = ERANGE;
    }
    return 1;
}
