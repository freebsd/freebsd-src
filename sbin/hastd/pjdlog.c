/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * Copyright (c) 2011 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
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

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "pjdlog.h"

static bool pjdlog_initialized = false;
static int pjdlog_mode = PJDLOG_MODE_STD;
static int pjdlog_debug_level = 0;
static char pjdlog_prefix[128];

void
pjdlog_init(int mode)
{

	assert(!pjdlog_initialized);
	assert(mode == PJDLOG_MODE_STD || mode == PJDLOG_MODE_SYSLOG);

	if (mode == PJDLOG_MODE_SYSLOG)
		openlog(NULL, LOG_PID | LOG_NDELAY, LOG_DAEMON);
	pjdlog_mode = mode;

	pjdlog_initialized = true;
}

void
pjdlog_fini(void)
{

	assert(pjdlog_initialized);

	if (pjdlog_mode == PJDLOG_MODE_SYSLOG)
		closelog();

	pjdlog_initialized = false;
}

/*
 * Configure where the logs should go.
 * By default they are send to stdout/stderr, but after going into background
 * (eg. by calling daemon(3)) application is responsible for changing mode to
 * PJDLOG_MODE_SYSLOG, so logs will be send to syslog.
 */
void
pjdlog_mode_set(int mode)
{

	assert(pjdlog_initialized);
	assert(mode == PJDLOG_MODE_STD || mode == PJDLOG_MODE_SYSLOG);

	if (pjdlog_mode == mode)
		return;

	if (mode == PJDLOG_MODE_SYSLOG)
		openlog(NULL, LOG_PID | LOG_NDELAY, LOG_DAEMON);
	else /* if (mode == PJDLOG_MODE_STD) */
		closelog();

	pjdlog_mode = mode;
}

/*
 * Return current mode.
 */
int
pjdlog_mode_get(void)
{

	assert(pjdlog_initialized);

	return (pjdlog_mode);
}

/*
 * Set debug level. All the logs above the level specified here will be
 * ignored.
 */
void
pjdlog_debug_set(int level)
{

	assert(pjdlog_initialized);
	assert(level >= 0);

	pjdlog_debug_level = level;
}

/*
 * Return current debug level.
 */
int
pjdlog_debug_get(void)
{

	assert(pjdlog_initialized);

	return (pjdlog_debug_level);
}

/*
 * Set prefix that will be used before each log.
 * Setting prefix to NULL will remove it.
 */
void
pjdlog_prefix_set(const char *fmt, ...)
{
	va_list ap;

	assert(pjdlog_initialized);

	va_start(ap, fmt);
	pjdlogv_prefix_set(fmt, ap);
	va_end(ap);
}

/*
 * Set prefix that will be used before each log.
 * Setting prefix to NULL will remove it.
 */
void
pjdlogv_prefix_set(const char *fmt, va_list ap)
{

	assert(pjdlog_initialized);
	assert(fmt != NULL);

	vsnprintf(pjdlog_prefix, sizeof(pjdlog_prefix), fmt, ap);
}

/*
 * Convert log level into string.
 */
static const char *
pjdlog_level_string(int loglevel)
{

	switch (loglevel) {
	case LOG_EMERG:
		return ("EMERG");
	case LOG_ALERT:
		return ("ALERT");
	case LOG_CRIT:
		return ("CRIT");
	case LOG_ERR:
		return ("ERROR");
	case LOG_WARNING:
		return ("WARNING");
	case LOG_NOTICE:
		return ("NOTICE");
	case LOG_INFO:
		return ("INFO");
	case LOG_DEBUG:
		return ("DEBUG");
	}
	assert(!"Invalid log level.");
	abort();	/* XXX: gcc */
}

/*
 * Common log routine.
 */
void
pjdlog_common(int loglevel, int debuglevel, int error, const char *fmt, ...)
{
	va_list ap;

	assert(pjdlog_initialized);

	va_start(ap, fmt);
	pjdlogv_common(loglevel, debuglevel, error, fmt, ap);
	va_end(ap);
}

/*
 * Common log routine, which can handle regular log level as well as debug
 * level. We decide here where to send the logs (stdout/stderr or syslog).
 */
void
pjdlogv_common(int loglevel, int debuglevel, int error, const char *fmt,
    va_list ap)
{

	assert(pjdlog_initialized);
	assert(loglevel == LOG_EMERG || loglevel == LOG_ALERT ||
	    loglevel == LOG_CRIT || loglevel == LOG_ERR ||
	    loglevel == LOG_WARNING || loglevel == LOG_NOTICE ||
	    loglevel == LOG_INFO || loglevel == LOG_DEBUG);
	assert(loglevel != LOG_DEBUG || debuglevel > 0);
	assert(error >= -1);

	/* Ignore debug above configured level. */
	if (loglevel == LOG_DEBUG && debuglevel > pjdlog_debug_level)
		return;

	switch (pjdlog_mode) {
	case PJDLOG_MODE_STD:
	    {
		FILE *out;

		/*
		 * We send errors and warning to stderr and the rest to stdout.
		 */
		switch (loglevel) {
		case LOG_EMERG:
		case LOG_ALERT:
		case LOG_CRIT:
		case LOG_ERR:
		case LOG_WARNING:
			out = stderr;
			break;
		case LOG_NOTICE:
		case LOG_INFO:
		case LOG_DEBUG:
			out = stdout;
			break;
		default:
			assert(!"Invalid loglevel.");
			abort();	/* XXX: gcc */
		}

		fprintf(out, "[%s]", pjdlog_level_string(loglevel));
		/* Attach debuglevel if this is debug log. */
		if (loglevel == LOG_DEBUG)
			fprintf(out, "[%d]", debuglevel);
		fprintf(out, " %s", pjdlog_prefix);
		vfprintf(out, fmt, ap);
		if (error != -1)
			fprintf(out, ": %s.", strerror(error));
		fprintf(out, "\n");
		fflush(out);
		break;
	    }
	case PJDLOG_MODE_SYSLOG:
	    {
		char log[1024];
		int len;

		len = snprintf(log, sizeof(log), "%s", pjdlog_prefix);
		if ((size_t)len < sizeof(log))
			len += vsnprintf(log + len, sizeof(log) - len, fmt, ap);
		if (error != -1 && (size_t)len < sizeof(log)) {
			(void)snprintf(log + len, sizeof(log) - len, ": %s.",
			    strerror(error));
		}
		syslog(loglevel, "%s", log);
		break;
	    }
	default:
		assert(!"Invalid mode.");
	}
}

/*
 * Regular logs.
 */
void
pjdlogv(int loglevel, const char *fmt, va_list ap)
{

	assert(pjdlog_initialized);

	/* LOG_DEBUG is invalid here, pjdlogv?_debug() should be used. */
	assert(loglevel == LOG_EMERG || loglevel == LOG_ALERT ||
	    loglevel == LOG_CRIT || loglevel == LOG_ERR ||
	    loglevel == LOG_WARNING || loglevel == LOG_NOTICE ||
	    loglevel == LOG_INFO);

	pjdlogv_common(loglevel, 0, -1, fmt, ap);
}

/*
 * Regular logs.
 */
void
pjdlog(int loglevel, const char *fmt, ...)
{
	va_list ap;

	assert(pjdlog_initialized);

	va_start(ap, fmt);
	pjdlogv(loglevel, fmt, ap);
	va_end(ap);
}

/*
 * Debug logs.
 */
void
pjdlogv_debug(int debuglevel, const char *fmt, va_list ap)
{

	assert(pjdlog_initialized);

	pjdlogv_common(LOG_DEBUG, debuglevel, -1, fmt, ap);
}

/*
 * Debug logs.
 */
void
pjdlog_debug(int debuglevel, const char *fmt, ...)
{
	va_list ap;

	assert(pjdlog_initialized);

	va_start(ap, fmt);
	pjdlogv_debug(debuglevel, fmt, ap);
	va_end(ap);
}

/*
 * Error logs with errno logging.
 */
void
pjdlogv_errno(int loglevel, const char *fmt, va_list ap)
{

	assert(pjdlog_initialized);

	pjdlogv_common(loglevel, 0, errno, fmt, ap);
}

/*
 * Error logs with errno logging.
 */
void
pjdlog_errno(int loglevel, const char *fmt, ...)
{
	va_list ap;

	assert(pjdlog_initialized);

	va_start(ap, fmt);
	pjdlogv_errno(loglevel, fmt, ap);
	va_end(ap);
}

/*
 * Log error, errno and exit.
 */
void
pjdlogv_exit(int exitcode, const char *fmt, va_list ap)
{

	assert(pjdlog_initialized);

	pjdlogv_errno(LOG_ERR, fmt, ap);
	exit(exitcode);
	/* NOTREACHED */
}

/*
 * Log error, errno and exit.
 */
void
pjdlog_exit(int exitcode, const char *fmt, ...)
{
	va_list ap;

	assert(pjdlog_initialized);

	va_start(ap, fmt);
	pjdlogv_exit(exitcode, fmt, ap);
	/* NOTREACHED */
	va_end(ap);
}

/*
 * Log error and exit.
 */
void
pjdlogv_exitx(int exitcode, const char *fmt, va_list ap)
{

	assert(pjdlog_initialized);

	pjdlogv(LOG_ERR, fmt, ap);
	exit(exitcode);
	/* NOTREACHED */
}

/*
 * Log error and exit.
 */
void
pjdlog_exitx(int exitcode, const char *fmt, ...)
{
	va_list ap;

	assert(pjdlog_initialized);

	va_start(ap, fmt);
	pjdlogv_exitx(exitcode, fmt, ap);
	/* NOTREACHED */
	va_end(ap);
}

/*
 * Log assertion and exit.
 */
void
pjdlog_verify(const char *func, const char *file, int line,
    const char *failedexpr)
{

	if (func == NULL) {
		pjdlog_critical("Assertion failed: (%s), file %s, line %d.",
		    failedexpr, file, line);
	} else {
		pjdlog_critical("Assertion failed: (%s), function %s, file %s, line %d.",
		    failedexpr, func, file, line);
	}
	abort();
}
