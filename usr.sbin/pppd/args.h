/*
 * neat macro from ka9q to "do the right thing" with ansi prototypes
 * $FreeBSD$
 */

#ifndef __P
#ifdef __STDC__
#define __P(x)       x
#else
#define __P(x)       ()
#endif
#endif
