/* $FreeBSD$ */

/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.in by autoheader.  */
/*
 * Copyright (C) 2004, 2005, 2007, 2009  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: acconfig.h,v 1.51.334.2 2009-02-16 23:47:15 tbox Exp $ */

/*! \file */

/***
 *** This file is not to be included by any public header files, because
 *** it does not get installed.
 ***/

/** define on DEC OSF to enable 4.4BSD style sa_len support */
/* #undef _SOCKADDR_LEN */

/** define if your system needs pthread_init() before using pthreads */
/* #undef NEED_PTHREAD_INIT */

/** define if your system has sigwait() */
#define HAVE_SIGWAIT 1

/** define if sigwait() is the UnixWare flavor */
/* #undef HAVE_UNIXWARE_SIGWAIT */

/** define on Solaris to get sigwait() to work using pthreads semantics */
/* #undef _POSIX_PTHREAD_SEMANTICS */

/** define if LinuxThreads is in use */
/* #undef HAVE_LINUXTHREADS */

/** define if sysconf() is available */
#define HAVE_SYSCONF 1

/** define if sysctlbyname() is available */
#define HAVE_SYSCTLBYNAME 1

/** define if catgets() is available */
#define HAVE_CATGETS 1

/** define if getifaddrs() exists */
#define HAVE_GETIFADDRS 1

/** define if you have the NET_RT_IFLIST sysctl variable and sys/sysctl.h */
#define HAVE_IFLIST_SYSCTL 1

/** define if tzset() is available */
#define HAVE_TZSET 1

/** define if struct addrinfo exists */
#define HAVE_ADDRINFO 1

/** define if getaddrinfo() exists */
#define HAVE_GETADDRINFO 1

/** define if gai_strerror() exists */
#define HAVE_GAISTRERROR 1

/** define if arc4random() exists */
#define HAVE_ARC4RANDOM 1

/**
 * define if pthread_setconcurrency() should be called to tell the
 * OS how many threads we might want to run.
 */
/* #undef CALL_PTHREAD_SETCONCURRENCY */

/** define if IPv6 is not disabled */
/* #undef WANT_IPV6 */

/** define if flockfile() is available */
#define HAVE_FLOCKFILE 1

/** define if getc_unlocked() is available */
#define HAVE_GETCUNLOCKED 1

/** Shut up warnings about sputaux in stdio.h on BSD/OS pre-4.1 */
/* #undef SHUTUP_SPUTAUX */
#ifdef SHUTUP_SPUTAUX
struct __sFILE;
extern __inline int __sputaux(int _c, struct __sFILE *_p);
#endif

/** Shut up warnings about missing sigwait prototype on BSD/OS 4.0* */
/* #undef SHUTUP_SIGWAIT */
#ifdef SHUTUP_SIGWAIT
int sigwait(const unsigned int *set, int *sig);
#endif

/** Shut up warnings from gcc -Wcast-qual on BSD/OS 4.1. */
/* #undef SHUTUP_STDARG_CAST */
#if defined(SHUTUP_STDARG_CAST) && defined(__GNUC__)
#include <stdarg.h>		/** Grr.  Must be included *every time*. */
/**
 * The silly continuation line is to keep configure from
 * commenting out the #undef.
 */

#undef \
	va_start
#define	va_start(ap, last) \
	do { \
		union { const void *konst; long *var; } _u; \
		_u.konst = &(last); \
		ap = (va_list)(_u.var + __va_words(__typeof(last))); \
	} while (0)
#endif /** SHUTUP_STDARG_CAST && __GNUC__ */

/** define if the system has a random number generating device */
#define PATH_RANDOMDEV "/dev/random"

/** define if pthread_attr_getstacksize() is available */
#define HAVE_PTHREAD_ATTR_GETSTACKSIZE 1

/** define if pthread_attr_setstacksize() is available */
#define HAVE_PTHREAD_ATTR_SETSTACKSIZE 1

/** define if you have strerror in the C library. */
#define HAVE_STRERROR 1

/** Define if you are running under Compaq TruCluster. */
/* #undef HAVE_TRUCLUSTER */

/* Define if OpenSSL includes DSA support */
#define HAVE_OPENSSL_DSA 1

/* Define to the length type used by the socket API (socklen_t, size_t, int). */
#define ISC_SOCKADDR_LEN_T socklen_t

/* Define if threads need PTHREAD_SCOPE_SYSTEM */
/* #undef NEED_PTHREAD_SCOPE_SYSTEM */

/* define if ATF unit tests are to be built. */
/* #undef ATF_TEST */

/* Define if recvmsg() does not meet all of the BSD socket API specifications.
   */
/* #undef BROKEN_RECVMSG */

/* Define if you cannot bind() before connect() for TCP sockets. */
/* #undef BROKEN_TCP_BIND_BEFORE_CONNECT */

/* Define to enable "rrset-order fixed" syntax. */
/* #undef DNS_RDATASET_FIXED */

/* Solaris hack to get select_large_fdset. */
/* #undef FD_SETSIZE */

/* Define to nothing if C supports flexible array members, and to 1 if it does
   not. That way, with a declaration like `struct s { int n; double
   d[FLEXIBLE_ARRAY_MEMBER]; };', the struct hack can be used with pre-C99
   compilers. When computing the size of such an object, don't use 'sizeof
   (struct s)' as it overestimates the size. Use 'offsetof (struct s, d)'
   instead. Don't use 'offsetof (struct s, d[0])', as this doesn't work with
   MSVC and with C++ compilers. */
#define FLEXIBLE_ARRAY_MEMBER /**/

/* Define to 1 if you have the `chroot' function. */
#define HAVE_CHROOT 1

/* Define to 1 if you have the <devpoll.h> header file. */
/* #undef HAVE_DEVPOLL_H */

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the `EVP_sha256' function. */
#define HAVE_EVP_SHA256 1

/* Define to 1 if you have the `EVP_sha512' function. */
#define HAVE_EVP_SHA512 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `getenv' function. */
#define HAVE_GETENV 1

/* Define to 1 if you have the <gssapi/gssapi.h> header file. */
/* #undef HAVE_GSSAPI_GSSAPI_H */

/* Define to 1 if you have the <gssapi.h> header file. */
/* #undef HAVE_GSSAPI_H */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <kerberosv5/krb5.h> header file. */
/* #undef HAVE_KERBEROSV5_KRB5_H */

/* Define to 1 if you have the <krb5.h> header file. */
/* #undef HAVE_KRB5_H */

/* Define to 1 if you have the <krb5/krb5.h> header file. */
/* #undef HAVE_KRB5_KRB5_H */

/* Define to 1 if you have the `c' library (-lc). */
/* #undef HAVE_LIBC */

/* Define to 1 if you have the `cap' library (-lcap). */
/* #undef HAVE_LIBCAP */

/* Define to 1 if you have the `c_r' library (-lc_r). */
/* #undef HAVE_LIBC_R */

/* Define to 1 if you have the `nsl' library (-lnsl). */
/* #undef HAVE_LIBNSL */

/* Define to 1 if you have the `pthread' library (-lpthread). */
/* #undef HAVE_LIBPTHREAD */

/* Define to 1 if you have the `scf' library (-lscf). */
/* #undef HAVE_LIBSCF */

/* Define to 1 if you have the `socket' library (-lsocket). */
/* #undef HAVE_LIBSOCKET */

/* Define to 1 if you have the `thr' library (-lthr). */
/* #undef HAVE_LIBTHR */

/* Define if libxml2 was found */
/* #undef HAVE_LIBXML2 */

/* Define to 1 if you have the <linux/capability.h> header file. */
/* #undef HAVE_LINUX_CAPABILITY_H */

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `nanosleep' function. */
#define HAVE_NANOSLEEP 1

/* Define to 1 if you have the <net/if6.h> header file. */
/* #undef HAVE_NET_IF6_H */

/* Define to 1 if you have the `setlocale' function. */
#define HAVE_SETLOCALE 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/capability.h> header file. */
/* #undef HAVE_SYS_CAPABILITY_H */

/* Define to 1 if you have the <sys/devpoll.h> header file. */
/* #undef HAVE_SYS_DEVPOLL_H */

/* Define to 1 if you have the <sys/dyntune.h> header file. */
/* #undef HAVE_SYS_DYNTUNE_H */

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/prctl.h> header file. */
/* #undef HAVE_SYS_PRCTL_H */

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/sockio.h> header file. */
#define HAVE_SYS_SOCKIO_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/sysctl.h> header file. */
#define HAVE_SYS_SYSCTL_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/un.h> header file. */
#define HAVE_SYS_UN_H 1

/* Define if running under Compaq TruCluster */
/* #undef HAVE_TRUCLUSTER */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Defined if extern char *optarg is not declared. */
/* #undef NEED_OPTARG */

/* Define if connect does not honour the permission on the UNIX domain socket.
   */
/* #undef NEED_SECURE_DIRECTORY */

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* Sets which flag to pass to open/fcntl to make non-blocking
   (O_NDELAY/O_NONBLOCK). */
#define PORT_NONBLOCK O_NONBLOCK

/* The size of `void *', as computed by sizeof. */
#define SIZEOF_VOID_P 8

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Defined if you need to use ioctl(FIONBIO) instead a fcntl call to make
   non-blocking. */
/* #undef USE_FIONBIO_IOCTL */

/* define if idnkit support is to be included. */
/* #undef WITH_IDN */

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel and VAX). */
#if defined __BIG_ENDIAN__
# define WORDS_BIGENDIAN 1
#elif ! defined __LITTLE_ENDIAN__
/* # undef WORDS_BIGENDIAN */
#endif

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
#define inline /**/
#endif

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef ssize_t */

/* Define to `unsigned long' if <sys/types.h> does not define. */
/* #undef uintptr_t */

/* Define to empty if the keyword `volatile' does not work. Warning: valid
   code using `volatile' can become incorrect without. Disable with care. */
/* #undef volatile */
