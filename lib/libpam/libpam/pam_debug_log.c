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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include <security/pam_modules.h>
#include <security/pam_mod_misc.h>

#define	FMTBUFSIZ	256

static char *modulename(const char *);

/* Log a debug message, including the function name and a
 * cleaned up filename.
 */
void
_pam_log(struct options *options, const char *file, const char *function,
    const char *format, ...)
{
	va_list ap;
	char *fmtbuf, *modname;

	if (pam_test_option(options, PAM_OPT_DEBUG, NULL)) {
		modname = modulename(file);
		va_start(ap, format);
		asprintf(&fmtbuf, "%s: %s: %s", modname, function, format);
		vsyslog(LOG_DEBUG, fmtbuf, ap);
		free(fmtbuf);
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
	char *modname;

	if (pam_test_option(options, PAM_OPT_DEBUG, NULL)) {
		modname = modulename(file);

		switch (retval) {
		case PAM_SUCCESS:
			syslog(LOG_DEBUG, "%s: %s: returning PAM_SUCCESS",
			    modname, function);
			break;
		case PAM_AUTH_ERR:
			syslog(LOG_DEBUG, "%s: %s: returning PAM_AUTH_ERR",
			    modname, function);
			break;
		case PAM_IGNORE:
			syslog(LOG_DEBUG, "%s: %s: returning PAM_IGNORE",
			    modname, function);
			break;
		case PAM_PERM_DENIED:
			syslog(LOG_DEBUG, "%s: %s: returning PAM_PERM_DENIED",
			    modname, function);
			break;
		default:
			syslog(LOG_DEBUG, "%s: %s: returning (%d)",
			    modname, function, retval);
		}

		free(modname);
	}
}

/* Print a verbose error, including the function name and a
 * cleaned up filename.
 */
void
_pam_verbose_error(pam_handle_t *pamh, struct options *options,
    const char *file, const char *function, const char *format, ...)
{
	va_list ap;
	char *statusmsg, *fmtbuf, *modname;

	if (!pam_test_option(options, PAM_OPT_NO_WARN, NULL)) {
		modname = modulename(file);
		va_start(ap, format);
		asprintf(&fmtbuf, "%s: %s: %s", modname, function, format);
		vasprintf(&statusmsg, fmtbuf, ap);
		pam_prompt(pamh, PAM_ERROR_MSG, statusmsg, NULL);
		free(statusmsg);
		free(fmtbuf);
		va_end(ap);
	}
}

static char *
modulename(const char *file)
{
	char *modname, *period;

	modname = strdup(basename(file));
	period = strchr(modname, '.');
	if (period != NULL)
		*period = '\0';

	return modname;
}
