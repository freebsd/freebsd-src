/* freebsd.h

   System dependencies for FreeBSD... */

/*
 * Copyright (c) 1996, 1998 The Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE INTERNET SOFTWARE CONSORTIUM OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software was written for the Internet Software Consortium by Ted Lemon
 * under a contract with Vixie Laboratories.
 */

#define _ANSI_SOURCE

#include <syslog.h>
#include <sys/types.h>
#include <string.h>
#include <paths.h>
#include <errno.h>
#include <unistd.h>
#include <setjmp.h>
#include <limits.h>

#include <sys/wait.h>
#include <signal.h>

extern int h_errno;

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_arp.h>
#if !defined (INADDR_LOOPBACK)
# define INADDR_LOOPBACK ((u_int32_t)0x7f000001)
#endif

/* Varargs stuff... */
#include <stdarg.h>
#define VA_DOTDOTDOT ...
#define va_dcl
#define VA_start(list, last) va_start (list, last)

#ifndef _PATH_DHCPD_PID
#define _PATH_DHCPD_PID	"/var/run/dhcpd.pid"
#endif
#ifndef _PATH_DHCPD_DB
#define _PATH_DHCPD_DB	"/var/db/dhcpd.leases"
#endif
#ifndef _PATH_DHCLIENT_PID
#define _PATH_DHCLIENT_PID "/var/run/dhclient.pid"
#endif
#ifndef _PATH_DHCLIENT_DB
#define _PATH_DHCLIENT_DB "/var/db/dhclient.leases"
#endif

#define EOL	'\n'
#define VOIDPTR void *

/* Time stuff... */
#include <sys/time.h>
#define TIME time_t
#define GET_TIME(x)	time ((x))

#define HAVE_SA_LEN

#if defined (USE_DEFAULT_NETWORK)
#  define USE_BPF
#endif
