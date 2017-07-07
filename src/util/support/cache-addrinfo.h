/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2004 by the Massachusetts Institute of Technology,
 * Cambridge, MA, USA.  All Rights Reserved.
 *
 * This software is being provided to you, the LICENSEE, by the
 * Massachusetts Institute of Technology (M.I.T.) under the following
 * license.  By obtaining, using and/or copying this software, you agree
 * that you have read, understood, and will comply with these terms and
 * conditions:
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify and distribute
 * this software and its documentation for any purpose and without fee or
 * royalty is hereby granted, provided that you agree to comply with the
 * following copyright notice and statements, including the disclaimer, and
 * that the same appear on ALL copies of the software and documentation,
 * including modifications that you make for internal use or for
 * distribution:
 *
 * THIS SOFTWARE IS PROVIDED "AS IS", AND M.I.T. MAKES NO REPRESENTATIONS
 * OR WARRANTIES, EXPRESS OR IMPLIED.  By way of example, but not
 * limitation, M.I.T. MAKES NO REPRESENTATIONS OR WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE USE OF
 * THE LICENSED SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY
 * PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
 *
 * The name of the Massachusetts Institute of Technology or M.I.T. may NOT
 * be used in advertising or publicity pertaining to distribution of the
 * software.  Title to copyright in this software and any associated
 * documentation shall at all times remain with M.I.T., and USER agrees to
 * preserve same.
 *
 * Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 */

/*
 * Approach overview:
 *
 * If a system version is available but buggy, save handles to it,
 * redefine the names to refer to static functions defined here, and
 * in those functions, call the system versions and fix up the
 * returned data.  Use the native data structures and flag values.
 *
 * If no system version exists, use gethostby* and fake it.  Define
 * the data structures and flag values locally.
 *
 *
 * On Mac OS X, getaddrinfo results aren't cached (though
 * gethostbyname results are), so we need to build a cache here.  Now
 * things are getting really messy.  Because the cache is in use, we
 * use getservbyname, and throw away thread safety.  (Not that the
 * cache is thread safe, but when we get locking support, that'll be
 * dealt with.)  This code needs tearing down and rebuilding, soon.
 *
 *
 * Note that recent Windows developers' code has an interesting hack:
 * When you include the right header files, with the right set of
 * macros indicating system versions, you'll get an inline function
 * that looks for getaddrinfo (or whatever) in the system library, and
 * calls it if it's there.  If it's not there, it fakes it with
 * gethostby* calls.
 *
 * We're taking a simpler approach: A system provides these routines or
 * it does not.
 *
 * Someday, we may want to take into account different versions (say,
 * different revs of GNU libc) where some are broken in one way, and
 * some work or are broken in another way.  Cross that bridge when we
 * come to it.
 */

/* To do, maybe:
 *
 * + For AIX 4.3.3, using the RFC 2133 definition: Implement
 *   AI_NUMERICHOST.  It's not defined in the header file.
 *
 *   For certain (old?) versions of GNU libc, AI_NUMERICHOST is
 *   defined but not implemented.
 *
 * + Use gethostbyname2, inet_aton and other IPv6 or thread-safe
 *   functions if available.  But, see
 *   http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=135182 for one
 *   gethostbyname2 problem on Linux.  And besides, if a platform is
 *   supporting IPv6 at all, they really should be doing getaddrinfo
 *   by now.
 *
 * + inet_ntop, inet_pton
 *
 * + Conditionally export/import the function definitions, so a
 *   library can have a single copy instead of multiple.
 *
 * + Upgrade host requirements to include working implementations of
 *   these functions, and throw all this away.  Pleeease?  :-)
 */

#include "k5-platform.h"
#include "k5-thread.h"
#include "port-sockets.h"
#include "socket-utils.h"
#include "fake-addrinfo.h"

#if defined (__APPLE__) && defined (__MACH__) && 0
#define FAI_CACHE
#endif

struct face {
    struct in_addr *addrs4;
    struct in6_addr *addrs6;
    unsigned int naddrs4, naddrs6;
    time_t expiration;
    char *canonname, *name;
    struct face *next;
};

/* fake addrinfo cache */
struct fac {
    k5_mutex_t lock;
    struct face *data;
};

extern struct fac krb5int_fac;

extern int krb5int_init_fac (void);
extern void krb5int_fini_fac (void);
