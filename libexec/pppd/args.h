/*
 * neat macro from ka9q to "do the right thing" with ansi prototypes
 */

#ifndef __ARGS
#ifdef __STDC__
#define __ARGS(x)       x
#else
#define __ARGS(x)       ()
#endif
#endif
