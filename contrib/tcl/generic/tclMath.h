/*
 * tclMath.h --
 *
 *	This file is necessary because of Metrowerks CodeWarrior Pro 1
 *	on the Macintosh. With 8-byte doubles turned on, the definitions of
 *	sin, cos, acos, etc., are screwed up.  They are fine as long as
 *	they are used as function calls, but if the function pointers
 *	are passed around and used, they will crash hard on the 68K.
 *
 * Copyright (c) 1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMath.h 1.2 97/07/23 17:39:14
 */

#ifndef _TCLMATH
#define _TCLMATH

#if defined(MAC_TCL)
#   include "tclMacMath.h"
#else
#   include <math.h>
#endif

#endif /* _TCLMATH */
