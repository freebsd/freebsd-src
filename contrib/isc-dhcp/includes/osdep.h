/* osdep.h

   Operating system dependencies... */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   http://www.isc.org/
 *
 * This software has been written for Internet Systems Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about Internet Systems Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#if !defined (__ISC_DHCP_OSDEP_H__)
#define __ISC_DHCP_OSDEP_H__

#include "site.h"

/* Porting::

   If you add a new network API, you must add a check for it below: */

#if !defined (USE_SOCKETS) && \
    !defined (USE_SOCKET_SEND) && \
    !defined (USE_SOCKET_RECEIVE) && \
    !defined (USE_RAW_SOCKETS) && \
    !defined (USE_RAW_SEND) && \
    !defined (USE_SOCKET_RECEIVE) && \
    !defined (USE_BPF) && \
    !defined (USE_BPF_SEND) && \
    !defined (USE_BPF_RECEIVE) && \
    !defined (USE_LPF) && \
    !defined (USE_LPF_SEND) && \
    !defined (USE_LPF_RECEIVE) && \
    !defined (USE_NIT) && \
    !defined (USE_NIT_SEND) && \
    !defined (USE_NIT_RECEIVE) && \
    !defined (USR_DLPI_SEND) && \
    !defined (USE_DLPI_RECEIVE)
#  define USE_DEFAULT_NETWORK
#endif

#if !defined (TIME_MAX)
# define TIME_MAX 2147483647
#endif

/* Porting::

   If you add a new system configuration file, include it here: */

#if defined (sun)
# if defined (__svr4__) || defined (__SVR4)
#  include "cf/sunos5-5.h"
# else
#  include "cf/sunos4.h"
# endif
#endif

#ifdef aix
#  include "cf/aix.h"
#endif

#ifdef bsdi
#  include "cf/bsdos.h"
#endif

#ifdef __NetBSD__
#  include "cf/netbsd.h"
#endif

#ifdef __FreeBSD__
#  include "cf/freebsd.h"
#endif

#ifdef OpenBSD
#  include "cf/openbsd.h"
#endif

#if defined (__osf__) && defined (__alpha)
#  include "cf/alphaosf.h"
#endif

#ifdef ultrix
#  include "cf/ultrix.h"
#endif

#ifdef linux
#  include "cf/linux.h"
#endif

#ifdef SCO
#  include "cf/sco.h"
#endif

#if defined (hpux) || defined (__hpux)
#  include "cf/hpux.h"
#endif

#ifdef __QNX__
#  include "cf/qnx.h"
#endif

#ifdef __CYGWIN32__
#  include "cf/cygwin32.h"
#endif

#ifdef __APPLE__
# include "cf/rhapsody.h"
#else
# if defined (NeXT)
#  include "cf/nextstep.h"
# endif
#endif

/* snprintf/vsnprintf hacks.  for systems with no libc versions only. */
#ifdef NO_SNPRINTF
  extern int isc_print_snprintf(char *, size_t, const char *, ...);
  extern int isc_print_vsnprintf(char *, size_t, const char *, va_list ap);
# define snprintf  isc_print_snprintf
# define vsnprintf isc_print_vsnprintf
#endif

/* Porting::

   If you add a new network API, and have it set up so that it can be
   used for sending or receiving, but doesn't have to be used for both,
   then set up an ifdef like the ones below: */

#ifdef USE_SOCKETS
#  define USE_SOCKET_SEND
#  define USE_SOCKET_RECEIVE
#endif

#ifdef USE_RAW_SOCKETS
#  define USE_RAW_SEND
#  define USE_SOCKET_RECEIVE
#endif

#ifdef USE_BPF
#  define USE_BPF_SEND
#  define USE_BPF_RECEIVE
#endif

#ifdef USE_LPF
#  define USE_LPF_SEND
#  define USE_LPF_RECEIVE
#endif

#ifdef USE_NIT
#  define USE_NIT_SEND
#  define USE_NIT_RECEIVE
#endif

#ifdef USE_DLPI
#  define USE_DLPI_SEND
#  define USE_DLPI_RECEIVE
#endif

#ifdef USE_UPF
#  define USE_UPF_SEND
#  define USE_UPF_RECEIVE
#endif

/* Porting::

   If you add support for sending packets directly out an interface,
   and your support does not do ARP or routing, you must use a fallback
   mechanism to deal with packets that need to be sent to routers.
   Currently, all low-level packet interfaces use BSD sockets as a
   fallback. */

#if defined (USE_BPF_SEND) || defined (USE_NIT_SEND) || \
    defined (USE_DLPI_SEND) || defined (USE_UPF_SEND) || \
    defined (USE_LPF_SEND) || \
    (defined (USE_SOCKET_SEND) && defined (HAVE_SO_BINDTODEVICE))
#  define USE_SOCKET_FALLBACK
#  define USE_FALLBACK
#endif

/* Porting::

   If you add support for sending packets directly out an interface
   and need to be able to assemble packets, add the USE_XXX_SEND
   definition for your interface to the list tested below. */

#if defined (USE_RAW_SEND) || defined (USE_BPF_SEND) || \
		defined (USE_NIT_SEND) || defined (USE_UPF_SEND) || \
		defined (USE_DLPI_SEND) || defined (USE_LPF_SEND)
#  define PACKET_ASSEMBLY
#endif

/* Porting::

   If you add support for receiving packets directly from an interface
   and need to be able to decode raw packets, add the USE_XXX_RECEIVE
   definition for your interface to the list tested below. */

#if defined (USE_RAW_RECEIVE) || defined (USE_BPF_SEND) || \
		defined (USE_NIT_RECEIVE) || defined (USE_UPF_RECEIVE) || \
		defined (USE_DLPI_RECEIVE) || defined (USE_LPF_RECEIVE)
#  define PACKET_DECODING
#endif

/* If we don't have a DLPI packet filter, we have to filter in userland.
   Probably not worth doing, actually. */
#if defined (USE_DLPI_RECEIVE) && !defined (USE_DLPI_PFMOD)
#  define USERLAND_FILTER
#endif

/* jmp_buf is assumed to be a struct unless otherwise defined in the
   system header. */
#ifndef jbp_decl
# define jbp_decl(x)	jmp_buf *x
#endif
#ifndef jref
# define jref(x)	(&(x))
#endif
#ifndef jdref
# define jdref(x)	(*(x))
#endif
#ifndef jrefproto
# define jrefproto	jmp_buf *
#endif

#ifndef BPF_FORMAT
# define BPF_FORMAT "/dev/bpf%d"
#endif

#if defined (F_SETFD) && !defined (HAVE_SETFD)
# define HAVE_SETFD
#endif

#if defined (IFF_POINTOPOINT) && !defined (HAVE_IFF_POINTOPOINT)
# define HAVE_IFF_POINTOPOINT
#endif

#if defined (AF_LINK) && !defined (HAVE_AF_LINK)
# define HAVE_AF_LINK
#endif

#if defined (ARPHRD_TUNNEL) && !defined (HAVE_ARPHRD_TUNNEL)
# define HAVE_ARPHRD_TUNNEL
#endif

#if defined (ARPHRD_LOOPBACK) && !defined (HAVE_ARPHRD_LOOPBACK)
# define HAVE_ARPHRD_LOOPBACK
#endif

#if defined (ARPHRD_ROSE) && !defined (HAVE_ARPHRD_ROSE)
# define HAVE_ARPHRD_ROSE
#endif

#if defined (ARPHRD_IEEE802) && !defined (HAVE_ARPHRD_IEEE802)
# define HAVE_ARPHRD_IEEE802
#endif

#if defined (ARPHRD_IEEE802_TR) && !defined (HAVE_ARPHRD_IEEE802_TR)
# define HAVE_ARPHRD_IEEE802_TR
#endif

#if defined (ARPHRD_FDDI) && !defined (HAVE_ARPHRD_FDDI)
# define HAVE_ARPHRD_FDDI
#endif

#if defined (ARPHRD_AX25) && !defined (HAVE_ARPHRD_AX25)
# define HAVE_ARPHRD_AX25
#endif

#if defined (ARPHRD_NETROM) && !defined (HAVE_ARPHRD_NETROM)
# define HAVE_ARPHRD_NETROM
#endif

#if defined (ARPHRD_METRICOM) && !defined (HAVE_ARPHRD_METRICOM)
# define HAVE_ARPHRD_METRICOM
#endif

#if defined (SO_BINDTODEVICE) && !defined (HAVE_SO_BINDTODEVICE)
# define HAVE_SO_BINDTODEVICE
#endif

#if defined (AF_LINK) && !defined (HAVE_AF_LINK)
# define HAVE_AF_LINK
#endif

/* Linux needs to define SHUT_* in /usr/include/sys/socket.h someday... */
#if !defined (SHUT_RD)
# define SHUT_RD 0
#endif

#if !defined (SOCKLEN_T)
#define SOCKLEN_T socklen_t
#endif

#endif /* __ISC_DHCP_OSDEP_H__ */
