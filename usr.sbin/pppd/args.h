/*
 * neat macro from ka9q to "do the right thing" with ansi prototypes
 * $Id: args.h,v 1.1 1993/11/11 03:54:25 paulus Exp $
 */

#ifndef __ARGS
#ifdef __STDC__
#define __ARGS(x)       x
#else
#define __ARGS(x)       ()
#endif
#endif
