/* $FreeBSD$ */

/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: platform.h.in,v 1.24.2.1.10.11 2004/03/08 09:04:52 marka Exp $ */

#ifndef ISC_PLATFORM_H
#define ISC_PLATFORM_H 1

/*****
 ***** Platform-dependent defines.
 *****/

/***
 *** Network.
 ***/

/*
 * Define if this system needs the <netinet/in6.h> header file included
 * for full IPv6 support (pretty much only UnixWare).
 */
#undef ISC_PLATFORM_NEEDNETINETIN6H

/*
 * Define if this system needs the <netinet6/in6.h> header file included
 * to support in6_pkinfo (pretty much only BSD/OS).
 */
#undef ISC_PLATFORM_NEEDNETINET6IN6H

/*
 * If sockaddrs on this system have an sa_len field, ISC_PLATFORM_HAVESALEN
 * will be defined.
 */
#define ISC_PLATFORM_HAVESALEN 1

/*
 * If this system has the IPv6 structure definitions, ISC_PLATFORM_HAVEIPV6
 * will be defined.
 */
#define ISC_PLATFORM_HAVEIPV6 1

/*
 * If this system is missing in6addr_any, ISC_PLATFORM_NEEDIN6ADDRANY will
 * be defined.
 */
#undef ISC_PLATFORM_NEEDIN6ADDRANY

/*
 * If this system is missing in6addr_loopback, ISC_PLATFORM_NEEDIN6ADDRLOOPBACK
 * will be defined.
 */
#undef ISC_PLATFORM_NEEDIN6ADDRLOOPBACK

/*
 * If this system has in6_pktinfo, ISC_PLATFORM_HAVEIN6PKTINFO will be
 * defined.
 */
#define ISC_PLATFORM_HAVEIN6PKTINFO 1

/*
 * If this system has in_addr6, rather than in6_addr, ISC_PLATFORM_HAVEINADDR6
 * will be defined.
 */
#undef ISC_PLATFORM_HAVEINADDR6

/*
 * If this system has sin6_scope_id, ISC_PLATFORM_HAVESCOPEID will be defined.
 */
#define ISC_PLATFORM_HAVESCOPEID 1

/*
 * If this system needs inet_ntop(), ISC_PLATFORM_NEEDNTOP will be defined.
 */
#undef ISC_PLATFORM_NEEDNTOP

/*
 * If this system needs inet_pton(), ISC_PLATFORM_NEEDPTON will be defined.
 */
#define ISC_PLATFORM_NEEDPTON 1

/*
 * If this system needs inet_aton(), ISC_PLATFORM_NEEDATON will be defined.
 */
#undef ISC_PLATFORM_NEEDATON

/*
 * If this system needs in_port_t, ISC_PLATFORM_NEEDPORTT will be defined.
 */
#undef ISC_PLATFORM_NEEDPORTT

/*
 * If the system needs strsep(), ISC_PLATFORM_NEEDSTRSEP will be defined.
 */
#undef ISC_PLATFORM_NEEDSTRSEP

/*
 * If the system needs strlcpy(), ISC_PLATFORM_NEEDSTRLCPY will be defined.
 */
#undef ISC_PLATFORM_NEEDSTRLCPY

/*
 * If the system needs strlcat(), ISC_PLATFORM_NEEDSTRLCAT will be defined.
 */
#undef ISC_PLATFORM_NEEDSTRLCAT

/*
 * Define either ISC_PLATFORM_BSD44MSGHDR or ISC_PLATFORM_BSD43MSGHDR.
 */
#define ISC_NET_BSD44MSGHDR 1

/*
 * Define if PTHREAD_ONCE_INIT should be surrounded by braces to
 * prevent compiler warnings (such as with gcc on Solaris 2.8).
 */
#undef ISC_PLATFORM_BRACEPTHREADONCEINIT

/*
 * Define on some UnixWare systems to fix erroneous definitions of various
 * IN6_IS_ADDR_* macros.
 */
#undef ISC_PLATFORM_FIXIN6ISADDR

/***
 *** Printing.
 ***/

/*
 * If this system needs vsnprintf() and snprintf(), ISC_PLATFORM_NEEDVSNPRINTF
 * will be defined.
 */
#undef ISC_PLATFORM_NEEDVSNPRINTF

/*
 * If this system need a modern sprintf() that returns (int) not (char*).
 */
#undef ISC_PLATFORM_NEEDSPRINTF

/*
 * The printf format string modifier to use with isc_uint64_t values.
 */
#define ISC_PLATFORM_QUADFORMAT "ll"

/*
 * Defined if we are using threads.
 */
#undef ISC_PLATFORM_USETHREADS

/*
 * Defined if unistd.h does not cause fd_set to be delared.
 */
#undef ISC_PLATFORM_NEEDSYSSELECTH

/*
 * Type used for resource limits.
 */
#define ISC_PLATFORM_RLIMITTYPE rlim_t

/*
 * Define if your compiler supports "long long int".
 */
#define ISC_PLATFORM_HAVELONGLONG 1

/*
 * Define if the system has struct lifconf which is a extended struct ifconf
 * for IPv6.
 */
#undef ISC_PLATFORM_HAVELIFCONF

/*
 * Define if the system has struct if_laddrconf which is a extended struct
 * ifconf for IPv6.
 */
#undef ISC_PLATFORM_HAVEIF_LADDRCONF

/*
 * Define if the system has struct if_laddrreq.
 */
#undef ISC_PLATFORM_HAVEIF_LADDRREQ

/*
 * Used to control how extern data is linked; needed for Win32 platforms.
 */
#undef ISC_PLATFORM_USEDECLSPEC

/*
 * Define if the system supports if_nametoindex.
 */
#define ISC_PLATFORM_HAVEIFNAMETOINDEX 1

/*
 * Define if this system needs strtoul.
 */
#undef ISC_PLATFORM_NEEDSTRTOUL

/*
 * Define if this system needs memmove.
 */
#undef ISC_PLATFORM_NEEDMEMMOVE

#ifndef ISC_PLATFORM_USEDECLSPEC
#define LIBISC_EXTERNAL_DATA
#define LIBDNS_EXTERNAL_DATA
#define LIBISCCC_EXTERNAL_DATA
#define LIBISCCFG_EXTERNAL_DATA
#define LIBBIND9_EXTERNAL_DATA
#else /* ISC_PLATFORM_USEDECLSPEC */
#ifdef LIBISC_EXPORTS
#define LIBISC_EXTERNAL_DATA __declspec(dllexport)
#else
#define LIBISC_EXTERNAL_DATA __declspec(dllimport)
#endif
#ifdef LIBDNS_EXPORTS
#define LIBDNS_EXTERNAL_DATA __declspec(dllexport)
#else
#define LIBDNS_EXTERNAL_DATA __declspec(dllimport)
#endif
#ifdef LIBISCCC_EXPORTS
#define LIBISCCC_EXTERNAL_DATA __declspec(dllexport)
#else
#define LIBISCCC_EXTERNAL_DATA __declspec(dllimport)
#endif
#ifdef LIBISCCFG_EXPORTS
#define LIBISCCFG_EXTERNAL_DATA __declspec(dllexport)
#else
#define LIBISCCFG_EXTERNAL_DATA __declspec(dllimport)
#endif
#ifdef LIBBIND9_EXPORTS
#define LIBBIND9_EXTERNAL_DATA __declspec(dllexport)
#else
#define LIBBIND9_EXTERNAL_DATA __declspec(dllimport)
#endif
#endif /* ISC_PLATFORM_USEDECLSPEC */

/*
 * Tell emacs to use C mode for this file.
 *
 * Local Variables:
 * mode: c
 * End:
 */

#endif /* ISC_PLATFORM_H */
