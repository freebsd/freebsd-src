/*
 * neat macro from ka9q to "do the right thing" with ansi prototypes
 * $Id: args.h,v 1.2 1994/09/25 02:31:51 wollman Exp $
 */

#ifndef __P
#ifdef __STDC__
#define __P(x)       x
#else
#define __P(x)       ()
#endif
#endif
