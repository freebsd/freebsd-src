/*
 * neat macro from ka9q to "do the right thing" with ansi prototypes
 * $Id: args.h,v 1.3 1996/03/01 19:29:34 phk Exp $
 */

#ifndef __P
#ifdef __STDC__
#define __P(x)       x
#else
#define __P(x)       ()
#endif
#endif
