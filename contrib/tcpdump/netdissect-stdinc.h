/*
 * Copyright (c) 2002 - 2003
 * NetGroup, Politecnico di Torino (Italy)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Politecnico di Torino nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Include the appropriate OS header files on Windows and various flavors
 * of UNIX, include various non-OS header files on Windows, and define
 * various items as needed, to isolate most of netdissect's platform
 * differences to this one file.
 */

#ifndef netdissect_stdinc_h
#define netdissect_stdinc_h

#include "ftmacros.h"

#include <errno.h>

#include "compiler-tests.h"

#include "varattrs.h"

/*
 * If we're compiling with Visual Studio, make sure we have at least
 * VS 2015 or later, so we have sufficient C99 support.
 *
 * XXX - verify that we have at least C99 support on UN*Xes?
 *
 * What about MinGW or various DOS toolchains?  We're currently assuming
 * sufficient C99 support there.
 */
#if defined(_MSC_VER)
  /*
   * Make sure we have VS 2015 or later.
   */
  #if _MSC_VER < 1900
    #error "Building tcpdump requires VS 2015 or later"
  #endif
#endif

/*
 * Get the C99 types, and the PRI[doux]64 format strings, defined.
 */
#ifdef HAVE_PCAP_PCAP_INTTYPES_H
  /*
   * We have pcap/pcap-inttypes.h; use that, as it'll do all the
   * work, and won't cause problems if a file includes this file
   * and later includes a pcap header file that also includes
   * pcap/pcap-inttypes.h.
   */
  #include <pcap/pcap-inttypes.h>
#else
  /*
   * OK, we don't have pcap/pcap-inttypes.h, so we'll have to
   * do the work ourselves, but at least we don't have to
   * worry about other headers including it and causing
   * clashes.
   */

  /*
   * Include <inttypes.h> to get the integer types and PRi[doux]64 values
   * defined.
   *
   * If the compiler is MSVC, we require VS 2015 or newer, so we
   * have <inttypes.h> - and support for %zu in the formatted
   * printing functions.
   *
   * If the compiler is MinGW, we assume we have <inttypes.h> - and
   * support for %zu in the formatted printing functions.
   *
   * If the target is UN*X, we assume we have a C99-or-later development
   * environment, and thus have <inttypes.h> - and support for %zu in
   * the formatted printing functions.
   *
   * If the target is MS-DOS, we assume we have <inttypes.h> - and support
   * for %zu in the formatted printing functions.
   */
  #include <inttypes.h>

  #if defined(_MSC_VER)
    /*
     * Suppress definition of intN_t in bittypes.h, which might be included
     * by <pcap/pcap.h> in older versions of WinPcap.
     * (Yes, HAVE_U_INTn_T, as the definition guards are UN*X-oriented.)
     */
    #define HAVE_U_INT8_T
    #define HAVE_U_INT16_T
    #define HAVE_U_INT32_T
    #define HAVE_U_INT64_T
  #endif
#endif /* HAVE_PCAP_PCAP_INTTYPES_H */

#ifdef _WIN32

/*
 * Includes and definitions for Windows.
 */

#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>

#ifdef _MSC_VER
  /*
   * Compiler is MSVC.
   *
   * We require VS 2015 or newer, so we have strtoll().  Use that for
   * strtoint64_t().
   */
  #define strtoint64_t	strtoll

  /*
   * And we have LL as a suffix for constants, so use that.
   */
  #define INT64_T_CONSTANT(constant)	(constant##LL)
#else
  /*
   * Non-Microsoft compiler.
   *
   * XXX - should we use strtoll or should we use _strtoi64()?
   */
  #define strtoint64_t		strtoll

  /*
   * Assume LL works.
   */
  #define INT64_T_CONSTANT(constant)	(constant##LL)
#endif

#ifdef _MSC_VER
  /*
   * Microsoft tries to avoid polluting the C namespace with UN*Xisms,
   * by adding a preceding underscore; we *want* the UN*Xisms, so add
   * #defines to let us use them.
   */
  #define isatty _isatty
  #define stat _stat
  #define strdup _strdup
  #define open _open
  #define read _read
  #define close _close
  #define O_RDONLY _O_RDONLY

  /*
   * We define our_fstat64 as _fstati64, and define our_statb as
   * struct _stati64, so we get 64-bit file sizes.
   */
  #define our_fstat _fstati64
  #define our_statb struct _stati64

  /*
   * If <crtdbg.h> has been included, and _DEBUG is defined, and
   * __STDC__ is zero, <crtdbg.h> will define strdup() to call
   * _strdup_dbg().  So if it's already defined, don't redefine
   * it.
   */
  #ifndef strdup
    #define strdup _strdup
  #endif

  /*
   * Windows doesn't have ssize_t; routines such as _read() return int.
   */
  typedef int ssize_t;
#endif  /* _MSC_VER */

/*
 * With MSVC, for C, __inline is used to make a function an inline.
 */
#ifdef _MSC_VER
#define inline __inline
#endif

#if defined(AF_INET6) && !defined(HAVE_OS_IPV6_SUPPORT)
#define HAVE_OS_IPV6_SUPPORT
#endif

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

/* It is in MSVC's <errno.h>, but not defined in MingW+Watcom.
 */
#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT WSAEAFNOSUPPORT
#endif

#ifndef caddr_t
typedef char *caddr_t;
#endif /* caddr_t */

#define MAXHOSTNAMELEN	64

#else /* _WIN32 */

/*
 * Includes and definitions for various flavors of UN*X.
 */

#include <unistd.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/types.h>			/* concession to AIX */
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <time.h>

#include <arpa/inet.h>

/*
 * We should have large file support enabled, if it's available,
 * so just use fstat as our_fstat and struct stat as our_statb.
 */
#define our_fstat fstat
#define our_statb struct stat

/*
 * Assume all UN*Xes have strtoll(), and use it for strtoint64_t().
 */
#define strtoint64_t	strtoll

/*
 * Assume LL works.
 */
#define INT64_T_CONSTANT(constant)	(constant##LL)
#endif /* _WIN32 */

/*
 * Function attributes, for various compilers.
 */
#include "funcattrs.h"

/*
 * fopen() read and write modes for text files and binary files.
 */
#if defined(_WIN32) || defined(MSDOS)
  #define FOPEN_READ_TXT   "rt"
  #define FOPEN_READ_BIN   "rb"
  #define FOPEN_WRITE_TXT  "wt"
  #define FOPEN_WRITE_BIN  "wb"
#else
  #define FOPEN_READ_TXT   "r"
  #define FOPEN_READ_BIN   FOPEN_READ_TXT
  #define FOPEN_WRITE_TXT  "w"
  #define FOPEN_WRITE_BIN  FOPEN_WRITE_TXT
#endif

/*
 * Inline x86 assembler-language versions of ntoh[ls]() and hton[ls](),
 * defined if the OS doesn't provide them.  These assume no more than
 * an 80386, so, for example, it avoids the bswap instruction added in
 * the 80486.
 *
 * (We don't use them on macOS; Apple provides their own, which *doesn't*
 * avoid the bswap instruction, as macOS only supports machines that
 * have it.)
 */
#if defined(__GNUC__) && defined(__i386__) && !defined(__APPLE__) && !defined(__ntohl)
  #undef ntohl
  #undef ntohs
  #undef htonl
  #undef htons

  static __inline__ unsigned long __ntohl (unsigned long x);
  static __inline__ unsigned short __ntohs (unsigned short x);

  #define ntohl(x)  __ntohl(x)
  #define ntohs(x)  __ntohs(x)
  #define htonl(x)  __ntohl(x)
  #define htons(x)  __ntohs(x)

  static __inline__ unsigned long __ntohl (unsigned long x)
  {
    __asm__ ("xchgb %b0, %h0\n\t"   /* swap lower bytes  */
             "rorl  $16, %0\n\t"    /* swap words        */
             "xchgb %b0, %h0"       /* swap higher bytes */
            : "=q" (x) : "0" (x));
    return (x);
  }

  static __inline__ unsigned short __ntohs (unsigned short x)
  {
    __asm__ ("xchgb %b0, %h0"       /* swap bytes */
            : "=q" (x) : "0" (x));
    return (x);
  }
#endif

/*
 * If the OS doesn't define AF_INET6 and struct in6_addr:
 *
 * define AF_INET6, so we can use it internally as a "this is an
 * IPv6 address" indication;
 *
 * define struct in6_addr so that we can use it for IPv6 addresses.
 */
#ifndef HAVE_OS_IPV6_SUPPORT
#ifndef AF_INET6
#define AF_INET6	24

struct in6_addr {
	union {
		__uint8_t   __u6_addr8[16];
		__uint16_t  __u6_addr16[8];
		__uint32_t  __u6_addr32[4];
	} __u6_addr;			/* 128-bit IP6 address */
};
#endif
#endif

#ifndef NI_MAXHOST
#define	NI_MAXHOST	1025
#endif

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/*
 * Statement attributes, for various compilers.
 *
 * This was introduced sufficiently recently that compilers implementing
 * it also implement __has_attribute() (for example, GCC 5.0 and later
 * have __has_attribute(), and the "fallthrough" attribute was introduced
 * in GCC 7).
 *
 * Unfortunately, Clang does this wrong - a statement
 *
 *    __attribute__ ((fallthrough));
 *
 * produces bogus -Wmissing-declaration "declaration does not declare
 * anything" warnings (dear Clang: that's not a declaration, it's an
 * empty statement).  GCC, however, has no trouble with this.
 */
#if __has_attribute(fallthrough) && !defined(__clang__)
#  define ND_FALL_THROUGH __attribute__ ((fallthrough))
#else
#  define ND_FALL_THROUGH
#endif /*  __has_attribute(fallthrough) */

#endif /* netdissect_stdinc_h */
