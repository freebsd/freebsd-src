/* $FreeBSD$ */

/*
 * Copyright (C) 2004-2010  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

/* $Id: platform.h.in,v 1.48.84.4 2010-06-03 23:47:49 tbox Exp $ */

#ifndef ISC_PLATFORM_H
#define ISC_PLATFORM_H 1

/*! \file */

/*****
 ***** Platform-dependent defines.
 *****/

/***
 *** Network.
 ***/

/*! \brief
 * Define if this system needs the <netinet/in6.h> header file included
 * for full IPv6 support (pretty much only UnixWare).
 */
#undef ISC_PLATFORM_NEEDNETINETIN6H

/*! \brief
 * Define if this system needs the <netinet6/in6.h> header file included
 * to support in6_pkinfo (pretty much only BSD/OS).
 */
#undef ISC_PLATFORM_NEEDNETINET6IN6H

/*! \brief
 * If sockaddrs on this system have an sa_len field, ISC_PLATFORM_HAVESALEN
 * will be defined.
 */
#define ISC_PLATFORM_HAVESALEN 1

/*! \brief
 * If this system has the IPv6 structure definitions, ISC_PLATFORM_HAVEIPV6
 * will be defined.
 */
#define ISC_PLATFORM_HAVEIPV6 1

/*! \brief
 * If this system is missing in6addr_any, ISC_PLATFORM_NEEDIN6ADDRANY will
 * be defined.
 */
#undef ISC_PLATFORM_NEEDIN6ADDRANY

/*! \brief
 * If this system is missing in6addr_loopback, ISC_PLATFORM_NEEDIN6ADDRLOOPBACK
 * will be defined.
 */
#undef ISC_PLATFORM_NEEDIN6ADDRLOOPBACK

/*! \brief
 * If this system has in6_pktinfo, ISC_PLATFORM_HAVEIN6PKTINFO will be
 * defined.
 */
#define ISC_PLATFORM_HAVEIN6PKTINFO 1

/*! \brief
 * If this system has in_addr6, rather than in6_addr, ISC_PLATFORM_HAVEINADDR6
 * will be defined.
 */
#undef ISC_PLATFORM_HAVEINADDR6

/*! \brief
 * If this system has sin6_scope_id, ISC_PLATFORM_HAVESCOPEID will be defined.
 */
#define ISC_PLATFORM_HAVESCOPEID 1

/*! \brief
 * If this system needs inet_ntop(), ISC_PLATFORM_NEEDNTOP will be defined.
 */
#undef ISC_PLATFORM_NEEDNTOP

/*! \brief
 * If this system needs inet_pton(), ISC_PLATFORM_NEEDPTON will be defined.
 */
#undef ISC_PLATFORM_NEEDPTON

/*! \brief
 * If this system needs in_port_t, ISC_PLATFORM_NEEDPORTT will be defined.
 */
#undef ISC_PLATFORM_NEEDPORTT

/*! \brief
 * Define if the system has struct lifconf which is a extended struct ifconf
 * for IPv6.
 */
#undef ISC_PLATFORM_HAVELIFCONF

/*! \brief
 * Define if the system has struct if_laddrconf which is a extended struct
 * ifconf for IPv6.
 */
#undef ISC_PLATFORM_HAVEIF_LADDRCONF

/*! \brief
 * Define if the system has struct if_laddrreq.
 */
#undef ISC_PLATFORM_HAVEIF_LADDRREQ

/*! \brief
 * Define either ISC_PLATFORM_BSD44MSGHDR or ISC_PLATFORM_BSD43MSGHDR.
 */
#define ISC_NET_BSD44MSGHDR 1

/*! \brief
 * Define if the system supports if_nametoindex.
 */
#define ISC_PLATFORM_HAVEIFNAMETOINDEX 1

/*! \brief
 * Define on some UnixWare systems to fix erroneous definitions of various
 * IN6_IS_ADDR_* macros.
 */
#undef ISC_PLATFORM_FIXIN6ISADDR

/*! \brief
 * Define if the system supports kqueue multiplexing
 */
#define ISC_PLATFORM_HAVEKQUEUE 1

/*! \brief
 * Define if the system supports epoll multiplexing
 */
#undef ISC_PLATFORM_HAVEEPOLL

/*! \brief
 * Define if the system supports /dev/poll multiplexing
 */
#undef ISC_PLATFORM_HAVEDEVPOLL

/*
 *** Printing.
 ***/

/*! \brief
 * If this system needs vsnprintf() and snprintf(), ISC_PLATFORM_NEEDVSNPRINTF
 * will be defined.
 */
#undef ISC_PLATFORM_NEEDVSNPRINTF

/*! \brief
 * If this system need a modern sprintf() that returns (int) not (char*).
 */
#undef ISC_PLATFORM_NEEDSPRINTF

/*! \brief
 * The printf format string modifier to use with isc_uint64_t values.
 */
#define ISC_PLATFORM_QUADFORMAT "ll"

/***
 *** String functions.
 ***/
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
 * Define if this system needs strtoul.
 */
#undef ISC_PLATFORM_NEEDSTRTOUL

/*
 * Define if this system needs memmove.
 */
#undef ISC_PLATFORM_NEEDMEMMOVE

/***
 *** Miscellaneous.
 ***/

/*
 * Defined if we are using threads.
 */
#define ISC_PLATFORM_USETHREADS 1

/*
 * Defined if unistd.h does not cause fd_set to be delared.
 */
#undef ISC_PLATFORM_NEEDSYSSELECTH

/*
 * Defined to <gssapi.h> or <gssapi/gssapi.h> for how to include
 * the GSSAPI header.
 */


/*
 * Defined to <krb5.h> or <krb5/krb5.h> for how to include
 * the KRB5 header.
 */


/*
 * Type used for resource limits.
 */
#define ISC_PLATFORM_RLIMITTYPE rlim_t

/*
 * Define if your compiler supports "long long int".
 */
#define ISC_PLATFORM_HAVELONGLONG 1

/*
 * Define if PTHREAD_ONCE_INIT should be surrounded by braces to
 * prevent compiler warnings (such as with gcc on Solaris 2.8).
 */
#undef ISC_PLATFORM_BRACEPTHREADONCEINIT

/*
 * Used to control how extern data is linked; needed for Win32 platforms.
 */
#undef ISC_PLATFORM_USEDECLSPEC

/*
 * Define if the platform has <sys/un.h>.
 */
#define ISC_PLATFORM_HAVESYSUNH 1

/*
 * If the "xadd" operation is available on this architecture,
 * ISC_PLATFORM_HAVEXADD will be defined.
 */
#define ISC_PLATFORM_HAVEXADD 1

/*
 * If the "xaddq" operation (64bit xadd) is available on this architecture,
 * ISC_PLATFORM_HAVEXADDQ will be defined.
 */
/*
 * FreeBSD local modification, preserve this over upgrades
 */
#ifdef __amd64__
#define ISC_PLATFORM_HAVEXADDQ 1
#else
#undef ISC_PLATFORM_HAVEXADDQ
#endif

/*
 * If the "atomic swap" operation is available on this architecture,
 * ISC_PLATFORM_HAVEATOMICSTORE" will be defined.
 */
#define ISC_PLATFORM_HAVEATOMICSTORE 1

/*
 * If the "compare-and-exchange" operation is available on this architecture,
 * ISC_PLATFORM_HAVECMPXCHG will be defined.
 */
#define ISC_PLATFORM_HAVECMPXCHG 1

/*
 * Define if gcc ASM extension is available
 */
#define ISC_PLATFORM_USEGCCASM 1

/*
 * Define if Tru64 style ASM syntax must be used.
 */
#undef ISC_PLATFORM_USEOSFASM

/*
 * Define if the standard __asm function must be used.
 */


/*
 * Define if the platform has <strings.h>.
 */
#define ISC_PLATFORM_HAVESTRINGSH 1

/***
 ***	Windows dll support.
 ***/

/*
 * Define if MacOS style of PPC assembly must be used.
 * e.g. "r6", not "6", for register six.
 */


#ifndef ISC_PLATFORM_USEDECLSPEC
#define LIBISC_EXTERNAL_DATA
#define LIBDNS_EXTERNAL_DATA
#define LIBISCCC_EXTERNAL_DATA
#define LIBISCCFG_EXTERNAL_DATA
#define LIBBIND9_EXTERNAL_DATA
#else /*! \brief ISC_PLATFORM_USEDECLSPEC */
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
#endif /*! \brief ISC_PLATFORM_USEDECLSPEC */

/*
 * Tell emacs to use C mode for this file.
 *
 * Local Variables:
 * mode: c
 * End:
 */

#endif /* ISC_PLATFORM_H */
