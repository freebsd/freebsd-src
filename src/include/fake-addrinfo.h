/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2001,2002,2003,2004 by the Massachusetts Institute of Technology,
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

/* Approach overview:

   If a system version is available but buggy, save handles to it (via
   inline functions in a support library), redefine the names to refer
   to library functions, and in those functions, call the system
   versions and fix up the returned data.  Use the native data
   structures and flag values.

   If no system version exists, use gethostby* and fake it.  Define
   the data structures and flag values locally.


   On macOS, getaddrinfo results aren't cached (though
   gethostbyname results are), so we need to build a cache here.  Now
   things are getting really messy.  Because the cache is in use, we
   use getservbyname, and throw away thread safety.  (Not that the
   cache is thread safe, but when we get locking support, that'll be
   dealt with.)  This code needs tearing down and rebuilding, soon.


   Note that recent Windows developers' code has an interesting hack:
   When you include the right header files, with the right set of
   macros indicating system versions, you'll get an inline function
   that looks for getaddrinfo (or whatever) in the system library, and
   calls it if it's there.  If it's not there, it fakes it with
   gethostby* calls.

   We're taking a simpler approach: A system provides these routines or
   it does not.

   Someday, we may want to take into account different versions (say,
   different revs of GNU libc) where some are broken in one way, and
   some work or are broken in another way.  Cross that bridge when we
   come to it.  */

/* To do, maybe:

   + For AIX 4.3.3, using the RFC 2133 definition: Implement
   AI_NUMERICHOST.  It's not defined in the header file.

   For certain (old?) versions of GNU libc, AI_NUMERICHOST is
   defined but not implemented.

   + Use gethostbyname2, inet_aton and other IPv6 or thread-safe
   functions if available.  But, see
   http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=135182 for one
   gethostbyname2 problem on Linux.  And besides, if a platform is
   supporting IPv6 at all, they really should be doing getaddrinfo
   by now.

   + inet_ntop, inet_pton

   + Conditionally export/import the function definitions, so a
   library can have a single copy instead of multiple.

   + Upgrade host requirements to include working implementations of
   these functions, and throw all this away.  Pleeease?  :-)  */

#ifndef FAI_DEFINED
#define FAI_DEFINED
#include "port-sockets.h"
#include "socket-utils.h"

#if !defined (HAVE_GETADDRINFO)

#undef  addrinfo
#define addrinfo        my_fake_addrinfo

struct addrinfo {
    int ai_family;              /* PF_foo */
    int ai_socktype;            /* SOCK_foo */
    int ai_protocol;            /* 0, IPPROTO_foo */
    int ai_flags;               /* AI_PASSIVE etc */
    size_t ai_addrlen;          /* real length of socket address */
    char *ai_canonname;         /* canonical name of host */
    struct sockaddr *ai_addr;   /* pointer to variable-size address */
    struct addrinfo *ai_next;   /* next in linked list */
};

#undef  AI_PASSIVE
#define AI_PASSIVE      0x01
#undef  AI_CANONNAME
#define AI_CANONNAME    0x02
#undef  AI_NUMERICHOST
#define AI_NUMERICHOST  0x04
/* RFC 2553 says these are part of the interface for getipnodebyname,
   not for getaddrinfo.  RFC 3493 says they're part of the interface
   for getaddrinfo, and getipnodeby* are deprecated.  Our fake
   getaddrinfo implementation here does IPv4 only anyways.  */
#undef  AI_V4MAPPED
#define AI_V4MAPPED     0
#undef  AI_ADDRCONFIG
#define AI_ADDRCONFIG   0
#undef  AI_ALL
#define AI_ALL          0
#undef  AI_DEFAULT
#define AI_DEFAULT      (AI_V4MAPPED|AI_ADDRCONFIG)

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif
#ifndef NI_MAXSERV
#define NI_MAXSERV 32
#endif

#undef  NI_NUMERICHOST
#define NI_NUMERICHOST  0x01
#undef  NI_NUMERICSERV
#define NI_NUMERICSERV  0x02
#undef  NI_NAMEREQD
#define NI_NAMEREQD     0x04
#undef  NI_DGRAM
#define NI_DGRAM        0x08
#undef  NI_NOFQDN
#define NI_NOFQDN       0x10


#undef  EAI_ADDRFAMILY
#define EAI_ADDRFAMILY  1
#undef  EAI_AGAIN
#define EAI_AGAIN       2
#undef  EAI_BADFLAGS
#define EAI_BADFLAGS    3
#undef  EAI_FAIL
#define EAI_FAIL        4
#undef  EAI_FAMILY
#define EAI_FAMILY      5
#undef  EAI_MEMORY
#define EAI_MEMORY      6
#undef  EAI_NODATA
#define EAI_NODATA      7
#undef  EAI_NONAME
#define EAI_NONAME      8
#undef  EAI_SERVICE
#define EAI_SERVICE     9
#undef  EAI_SOCKTYPE
#define EAI_SOCKTYPE    10
#undef  EAI_SYSTEM
#define EAI_SYSTEM      11

#endif /* ! HAVE_GETADDRINFO */

/* Fudge things on older gai implementations.  */
/* AIX 4.3.3 is based on RFC 2133; no AI_NUMERICHOST.  */
#ifndef AI_NUMERICHOST
# define AI_NUMERICHOST 0
#endif
/* Partial RFC 2553 implementations may not have AI_ADDRCONFIG and
   friends, which RFC 3493 says are now part of the getaddrinfo
   interface, and we'll want to use.  */
#ifndef AI_ADDRCONFIG
# define AI_ADDRCONFIG 0
#endif
#ifndef AI_V4MAPPED
# define AI_V4MAPPED 0
#endif
#ifndef AI_ALL
# define AI_ALL 0
#endif
#ifndef AI_DEFAULT
# define AI_DEFAULT (AI_ADDRCONFIG|AI_V4MAPPED)
#endif

#if defined(NEED_INSIXADDR_ANY)
/* If compiling with IPv6 support and C library does not define in6addr_any */
extern const struct in6_addr krb5int_in6addr_any;
#undef in6addr_any
#define in6addr_any krb5int_in6addr_any
#endif

/* Call out to stuff defined in libkrb5support.  */
extern int krb5int_getaddrinfo (const char *node, const char *service,
                                const struct addrinfo *hints,
                                struct addrinfo **aip);
extern void krb5int_freeaddrinfo (struct addrinfo *ai);
extern const char *krb5int_gai_strerror(int err);
extern int krb5int_getnameinfo (const struct sockaddr *sa, socklen_t salen,
                                char *hbuf, size_t hbuflen,
                                char *sbuf, size_t sbuflen,
                                int flags);
#ifndef IMPLEMENT_FAKE_GETADDRINFO
#undef  getaddrinfo
#define getaddrinfo krb5int_getaddrinfo
#undef  freeaddrinfo
#define freeaddrinfo krb5int_freeaddrinfo
#undef  gai_strerror
#define gai_strerror krb5int_gai_strerror
#undef  getnameinfo
#define getnameinfo krb5int_getnameinfo
#endif

#endif /* FAI_DEFINED */
