/*-
 * Copyright (c) 2002 Networks Associates Technologies, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by ThinkSec AS and
 * NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $P4: //depot/projects/openpam/lib/openpam_log.c#9 $
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#include <security/pam_appl.h>

#include "openpam_impl.h"

#if defined(openpam_log)

/*
 * OpenPAM extension
 *
 * Log a message through syslog(3)
 */

void
_openpam_log(int level, const char *func, const char *fmt, ...)
{
	va_list ap;
	char *format;
	int len, priority;

	switch (level) {
	case PAM_LOG_DEBUG:
		priority = LOG_DEBUG;
		break;
	case PAM_LOG_VERBOSE:
		priority = LOG_INFO;
		break;
	case PAM_LOG_NOTICE:
		priority = LOG_NOTICE;
		break;
	case PAM_LOG_ERROR:
		priority = LOG_ERR;
		break;
	}
	va_start(ap, fmt);
	for (len = strlen(fmt); len > 0 && isspace(fmt[len]); len--)
		/* nothing */;
	if ((format = malloc(strlen(func) + len + 16)) != NULL) {
		sprintf(format, "in %s(): %.*s\n", func, len, fmt);
		vsyslog(priority, format, ap);
#ifdef DEBUG
		vfprintf(stderr, format, ap);
#endif
		free(format);
	} else {
		vsyslog(priority, fmt, ap);
	}
	va_end(ap);
}

#else

/*
 * If openpam_log isn't defined as a macro, we're on a platform that
 * doesn't support varadic macros (or it does but we aren't aware of
 * it).  Do the next best thing.
 */

void
openpam_log(int level, const char *fmt, ...)
{
	va_list ap;
	int priority;

	switch (level) {
	case PAM_LOG_DEBUG:
		priority = LOG_DEBUG;
		break;
	case PAM_LOG_VERBOSE:
		priority = LOG_INFO;
		break;
	case PAM_LOG_NOTICE:
		priority = LOG_NOTICE;
		break;
	case PAM_LOG_ERROR:
		priority = LOG_ERR;
		break;
	}
	va_start(ap, fmt);
	vsyslog(priority, fmt, ap);
	va_end(ap);
}

#endif

/*
 * NOLIST
 */
