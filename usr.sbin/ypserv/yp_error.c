/*
 * Copyright (c) 1995
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/*
 * error logging/reporting facilities
 * stolen from /usr/libexec/mail.local via ypserv
 *
 * $Id: yp_error.c,v 1.2 1995/12/06 16:02:56 wpaul Exp $
 */

#include <stdio.h>
#include <sys/types.h>
#include <syslog.h>

int debug;
extern int _rpcpmstart;

extern char *progname;

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void verr(fmt, ap)
	const char *fmt;
	_BSD_VA_LIST_ ap;

{
	if (debug && !_rpcpmstart) {
		fprintf(stderr,"%s: ",progname);
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");
	} else {
		vsyslog(LOG_NOTICE, fmt, ap);
	}
}

void
#ifdef __STDC__
yp_error(const char *fmt, ...)
#else
yp_error(fmt, va_list)
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	verr(fmt,ap);
	va_end(ap);
}
