/*-
 * Copyright 2001 Mark R V Murray
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * $FreeBSD$
 */

#include <security/pam_modules.h>
#include <libgen.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>

#include "pam_mod_misc.h"

#define	FMTBUFSIZ	256

/* Log a debug message, including the function name and a
 * cleaned up filename.
 */
void
_pam_log(struct options *options, const char *file, const char *function,
    const char *format, ...)
{
	va_list ap;
	char *period;
	char fmtbuf[FMTBUFSIZ];

	if (pam_test_option(options, PAM_OPT_DEBUG, NULL)) {
		strncpy(fmtbuf, basename(file), FMTBUFSIZ);
		period = strchr(fmtbuf, '.');
		if (period != NULL)
			*period = '\0';
		strncat(fmtbuf, ": ", FMTBUFSIZ);
		strncat(fmtbuf, function, FMTBUFSIZ);
		strncat(fmtbuf, ": ", FMTBUFSIZ);
		strncat(fmtbuf, format, FMTBUFSIZ);
		va_start(ap, format);
		vsyslog(LOG_DEBUG, fmtbuf, ap);
		va_end(ap);
	}
}

/* Log a return value, including the function name and a
 * cleaned up filename.
 */
void
_pam_log_retval(struct options *options, const char *file, const char *function,
    int retval)
{
	char *period;
	char fmtbuf[FMTBUFSIZ];

	if (pam_test_option(options, PAM_OPT_DEBUG, NULL)) {
		strncpy(fmtbuf, basename(file), FMTBUFSIZ);
		period = strchr(fmtbuf, '.');
		if (period != NULL)
			*period = '\0';
		strncat(fmtbuf, ": ", FMTBUFSIZ);
		strncat(fmtbuf, function, FMTBUFSIZ);
		switch (retval) {
		case PAM_SUCCESS:
			strncat(fmtbuf, ": returning PAM_SUCCESS", FMTBUFSIZ);
			syslog(LOG_DEBUG, fmtbuf);
			break;
		case PAM_AUTH_ERR:
			strncat(fmtbuf, ": returning PAM_AUTH_ERR", FMTBUFSIZ);
			syslog(LOG_DEBUG, fmtbuf);
			break;
		default:
			strncat(fmtbuf, ": returning %d", FMTBUFSIZ);
			syslog(LOG_DEBUG, fmtbuf, retval);
		}
	}
}
