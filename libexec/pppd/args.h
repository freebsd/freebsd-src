/*
 * neat macro from ka9q to "do the right thing" with ansi prototypes
 * $Id: args.h,v 1.2 1994/03/30 09:31:21 jkh Exp $
 */

#ifndef __ARGS
#ifdef __STDC__
#define __ARGS(x)       x
#else
#define __ARGS(x)       ()
#endif
#endif
