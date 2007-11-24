/*  Copyright (C) 1996, 2000 N.M. Maclaren
    Copyright (C) 1996, 2000 The University of Cambridge

This includes all of the kludges necessary for certain broken systems.  It is
called after all other headers.  All of the modules set a flag to say which
they are, but none of the current kludges critically need that information. */



/* stdlib.h is broken under SunOS4. */

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS       0
#define EXIT_FAILURE       1
#endif



/* stdio.h is also broken under SunOS4. */

#ifndef SEEK_SET
#define SEEK_SET           0
#endif



/* netinet/in.h sometimes omits INADDR_LOOPBACK, or makes it conditional on
peculiar preprocessor symbols. */

#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK    0x7f000001ul
#endif



/* HP-UX up to version 9.x does not have adjtime, so make it fail.  This needs
a flag setting in Makefile. */

#ifdef ADJTIME_MISSING
#define adjtime(x,y)       1
#endif



/* O_NONBLOCK doesn't work under Ultrix 4.3.  This needs a flag setting in
Makefile. */

#ifdef NONBLOCK_BROKEN
#ifdef O_NONBLOCK
#undef O_NONBLOCK
#endif
#define O_NONBLOCK         O_NDELAY
#endif



/* Some older systems use EWOULDBLOCK rather than EAGAIN, but don't assume that
it is defined.  The differences are not relevant to this program. */

#ifndef EWOULDBLOCK
#define EWOULDBLOCK        EAGAIN
#endif
