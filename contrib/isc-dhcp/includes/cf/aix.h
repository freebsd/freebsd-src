/* aix.h */
/*
 * Copyright (c) 1996 The Internet Software Consortium.  All rights reserved.
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
 * INTERNET SOFTWARE CONSORTIUM OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#define int8_t		char
#define int16_t		short
#define int32_t		long

#define u_int8_t	unsigned char
#define u_int16_t	unsigned short 
#define u_int32_t	unsigned long 

#include <sys/types.h>

#include <syslog.h>

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <signal.h>
#include <setjmp.h>
#include <limits.h>

extern int h_errno;

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>

#ifndef _PATH_DHCPD_PID
#define _PATH_DHCPD_PID	"/etc/dhcpd.pid"
#endif
#ifndef _PATH_DHCLIENT_PID
#define _PATH_DHCLIENT_PID "/etc/dhclient.pid"
#endif
#ifndef _PATH_DHCRELAY_PID
#define _PATH_DHCRELAY_PID "/etc/dhcrelay.pid"
#endif

#include <varargs.h>
#define VA_DOTDOTDOT va_alist
#define VA_start(list, last) va_start (list)

#define vsnprintf(buf, size, fmt, list) vsprintf (buf, fmt, list)
#define NO_SNPRINTF

#define EOL '\n'
#define VOIDPTR void *

#include <time.h>

#define TIME time_t
#define GET_TIME(x)	time ((x))

#define random	rand

#define USE_SOCKETS	1
#define HAVE_SA_LEN	1
#undef FDDI
