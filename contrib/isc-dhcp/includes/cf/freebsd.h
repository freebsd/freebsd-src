/* freebsd.h

   System dependencies for FreeBSD... */

/*
 * Copyright (c) 1996-1999 Internet Software Consortium.
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
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about the Internet Software Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 *
 * $FreeBSD$
 */

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

/* socklen_t was first defined on November 24 in sys/socket.h, and
   __FreeBSD_version was changed to 400013 on December 4, so if you
   get a compile error on this, and you updated between those dates,
   that's why.   Also, it may be that some 3.x version after 3.4 will
   have socklen_t, but no such change has been made so far. */

#if  __FreeBSD_version < 400013
#define SOCKLEN_T int
#endif

#ifdef RESCUE
#define _PATH_DHCLIENT_SCRIPT	"/rescue/dhclient-script"
#endif

#if defined (USE_DEFAULT_NETWORK)
#  define USE_BPF
#endif
#define HAVE_MKSTEMP
#ifdef NEED_PRAND_CONF
#ifndef HAVE_DEV_RANDOM
 # define HAVE_DEV_RANDOM 1
 #endif /* HAVE_DEV_RANDOM */

const char *cmds[] = {
#ifndef RESCUE
	/* rescue environment can't rely on these ... */
	/* Actually, /sbin/dhclient shouldn't use these, either. */
	"/bin/ps -axlw 2>&1",
	"/usr/sbin/arp -an 2>&1",
	"/usr/bin/netstat -an 2>&1",
	"/bin/df  2>&1",
	"/usr/bin/dig com. soa +ti=1 +retry=0 2>&1",
	"/usr/bin/netstat -an 2>&1",
	"/usr/bin/dig . soa +ti=1 +retry=0 2>&1",
	"/usr/sbin/iostat  2>&1",
	"/usr/bin/vmstat  2>&1",
	"/usr/bin/w  2>&1",
#endif
	NULL
};

const char *dirs[] = {
#ifndef RESCUE
	"/tmp",
	"/usr/tmp",
	".",
	"/",
	"/var/spool",
	"/dev",
	"/var/mail",
	"/home",
	"/usr/home",
#endif
	NULL
};

const char *files[] = {
#ifndef RESCUE
	"/var/log/messages",
	"/var/log/wtmp",
	"/var/log/lastlog",
#endif
	NULL
};
#endif /* NEED_PRAND_CONF */
