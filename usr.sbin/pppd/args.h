/*
 * neat macro from ka9q to "do the right thing" with ansi prototypes
 * args.h,v 1.2 1994/09/25 02:31:51 wollman Exp
 */

#ifndef __ARGS
#ifdef __STDC__
#define __ARGS(x)       x
#else
#define __ARGS(x)       ()
#endif
#endif
