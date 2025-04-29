/* $OpenBSD: log.c,v 1.62 2024/06/27 22:36:44 djm Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */
/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include <sys/types.h>

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#if defined(HAVE_STRNVIS) && defined(HAVE_VIS_H) && !defined(BROKEN_STRNVIS)
# include <vis.h>
#endif

#include "log.h"
#include "match.h"

static LogLevel log_level = SYSLOG_LEVEL_INFO;
static int log_on_stderr = 1;
static int log_stderr_fd = STDERR_FILENO;
static int log_facility = LOG_AUTH;
static const char *argv0;
static log_handler_fn *log_handler;
static void *log_handler_ctx;
static char **log_verbose;
static size_t nlog_verbose;

extern char *__progname;

#define LOG_SYSLOG_VIS	(VIS_CSTYLE|VIS_NL|VIS_TAB|VIS_OCTAL)
#define LOG_STDERR_VIS	(VIS_SAFE|VIS_OCTAL)

/* textual representation of log-facilities/levels */

static struct {
	const char *name;
	SyslogFacility val;
} log_facilities[] = {
	{ "DAEMON",	SYSLOG_FACILITY_DAEMON },
	{ "USER",	SYSLOG_FACILITY_USER },
	{ "AUTH",	SYSLOG_FACILITY_AUTH },
#ifdef LOG_AUTHPRIV
	{ "AUTHPRIV",	SYSLOG_FACILITY_AUTHPRIV },
#endif
	{ "LOCAL0",	SYSLOG_FACILITY_LOCAL0 },
	{ "LOCAL1",	SYSLOG_FACILITY_LOCAL1 },
	{ "LOCAL2",	SYSLOG_FACILITY_LOCAL2 },
	{ "LOCAL3",	SYSLOG_FACILITY_LOCAL3 },
	{ "LOCAL4",	SYSLOG_FACILITY_LOCAL4 },
	{ "LOCAL5",	SYSLOG_FACILITY_LOCAL5 },
	{ "LOCAL6",	SYSLOG_FACILITY_LOCAL6 },
	{ "LOCAL7",	SYSLOG_FACILITY_LOCAL7 },
	{ NULL,		SYSLOG_FACILITY_NOT_SET }
};

static struct {
	const char *name;
	LogLevel val;
} log_levels[] =
{
	{ "QUIET",	SYSLOG_LEVEL_QUIET },
	{ "FATAL",	SYSLOG_LEVEL_FATAL },
	{ "ERROR",	SYSLOG_LEVEL_ERROR },
	{ "INFO",	SYSLOG_LEVEL_INFO },
	{ "VERBOSE",	SYSLOG_LEVEL_VERBOSE },
	{ "DEBUG",	SYSLOG_LEVEL_DEBUG1 },
	{ "DEBUG1",	SYSLOG_LEVEL_DEBUG1 },
	{ "DEBUG2",	SYSLOG_LEVEL_DEBUG2 },
	{ "DEBUG3",	SYSLOG_LEVEL_DEBUG3 },
	{ NULL,		SYSLOG_LEVEL_NOT_SET }
};

LogLevel
log_level_get(void)
{
	return log_level;
}

SyslogFacility
log_facility_number(char *name)
{
	int i;

	if (name != NULL)
		for (i = 0; log_facilities[i].name; i++)
			if (strcasecmp(log_facilities[i].name, name) == 0)
				return log_facilities[i].val;
	return SYSLOG_FACILITY_NOT_SET;
}

const char *
log_facility_name(SyslogFacility facility)
{
	u_int i;

	for (i = 0;  log_facilities[i].name; i++)
		if (log_facilities[i].val == facility)
			return log_facilities[i].name;
	return NULL;
}

LogLevel
log_level_number(char *name)
{
	int i;

	if (name != NULL)
		for (i = 0; log_levels[i].name; i++)
			if (strcasecmp(log_levels[i].name, name) == 0)
				return log_levels[i].val;
	return SYSLOG_LEVEL_NOT_SET;
}

const char *
log_level_name(LogLevel level)
{
	u_int i;

	for (i = 0; log_levels[i].name != NULL; i++)
		if (log_levels[i].val == level)
			return log_levels[i].name;
	return NULL;
}

void
log_verbose_add(const char *s)
{
	char **tmp;

	/* Ignore failures here */
	if ((tmp = recallocarray(log_verbose, nlog_verbose, nlog_verbose + 1,
	    sizeof(*log_verbose))) != NULL) {
		log_verbose = tmp;
		if ((log_verbose[nlog_verbose] = strdup(s)) != NULL)
			nlog_verbose++;
	}
}

void
log_verbose_reset(void)
{
	size_t i;

	for (i = 0; i < nlog_verbose; i++)
		free(log_verbose[i]);
	free(log_verbose);
	log_verbose = NULL;
	nlog_verbose = 0;
}

/*
 * Initialize the log.
 */

void
log_init(const char *av0, LogLevel level, SyslogFacility facility,
    int on_stderr)
{
#if defined(HAVE_OPENLOG_R) && defined(SYSLOG_DATA_INIT)
	struct syslog_data sdata = SYSLOG_DATA_INIT;
#endif

	argv0 = av0;

	if (log_change_level(level) != 0) {
		fprintf(stderr, "Unrecognized internal syslog level code %d\n",
		    (int) level);
		exit(1);
	}

	log_handler = NULL;
	log_handler_ctx = NULL;

	log_on_stderr = on_stderr;
	if (on_stderr)
		return;

	switch (facility) {
	case SYSLOG_FACILITY_DAEMON:
		log_facility = LOG_DAEMON;
		break;
	case SYSLOG_FACILITY_USER:
		log_facility = LOG_USER;
		break;
	case SYSLOG_FACILITY_AUTH:
		log_facility = LOG_AUTH;
		break;
#ifdef LOG_AUTHPRIV
	case SYSLOG_FACILITY_AUTHPRIV:
		log_facility = LOG_AUTHPRIV;
		break;
#endif
	case SYSLOG_FACILITY_LOCAL0:
		log_facility = LOG_LOCAL0;
		break;
	case SYSLOG_FACILITY_LOCAL1:
		log_facility = LOG_LOCAL1;
		break;
	case SYSLOG_FACILITY_LOCAL2:
		log_facility = LOG_LOCAL2;
		break;
	case SYSLOG_FACILITY_LOCAL3:
		log_facility = LOG_LOCAL3;
		break;
	case SYSLOG_FACILITY_LOCAL4:
		log_facility = LOG_LOCAL4;
		break;
	case SYSLOG_FACILITY_LOCAL5:
		log_facility = LOG_LOCAL5;
		break;
	case SYSLOG_FACILITY_LOCAL6:
		log_facility = LOG_LOCAL6;
		break;
	case SYSLOG_FACILITY_LOCAL7:
		log_facility = LOG_LOCAL7;
		break;
	default:
		fprintf(stderr,
		    "Unrecognized internal syslog facility code %d\n",
		    (int) facility);
		exit(1);
	}

	/*
	 * If an external library (eg libwrap) attempts to use syslog
	 * immediately after reexec, syslog may be pointing to the wrong
	 * facility, so we force an open/close of syslog here.
	 */
#if defined(HAVE_OPENLOG_R) && defined(SYSLOG_DATA_INIT)
	openlog_r(argv0 ? argv0 : __progname, LOG_PID, log_facility, &sdata);
	closelog_r(&sdata);
#else
	openlog(argv0 ? argv0 : __progname, LOG_PID, log_facility);
	closelog();
#endif
}

int
log_change_level(LogLevel new_log_level)
{
	/* no-op if log_init has not been called */
	if (argv0 == NULL)
		return 0;

	switch (new_log_level) {
	case SYSLOG_LEVEL_QUIET:
	case SYSLOG_LEVEL_FATAL:
	case SYSLOG_LEVEL_ERROR:
	case SYSLOG_LEVEL_INFO:
	case SYSLOG_LEVEL_VERBOSE:
	case SYSLOG_LEVEL_DEBUG1:
	case SYSLOG_LEVEL_DEBUG2:
	case SYSLOG_LEVEL_DEBUG3:
		log_level = new_log_level;
		return 0;
	default:
		return -1;
	}
}

int
log_is_on_stderr(void)
{
	return log_on_stderr && log_stderr_fd == STDERR_FILENO;
}

/* redirect what would usually get written to stderr to specified file */
void
log_redirect_stderr_to(const char *logfile)
{
	int fd;

	if (logfile == NULL) {
		if (log_stderr_fd != STDERR_FILENO) {
			close(log_stderr_fd);
			log_stderr_fd = STDERR_FILENO;
		}
		return;
	}

	if ((fd = open(logfile, O_WRONLY|O_CREAT|O_APPEND, 0600)) == -1) {
		fprintf(stderr, "Couldn't open logfile %s: %s\n", logfile,
		    strerror(errno));
		exit(1);
	}
	log_stderr_fd = fd;
}

#define MSGBUFSIZ 1024

void
set_log_handler(log_handler_fn *handler, void *ctx)
{
	log_handler = handler;
	log_handler_ctx = ctx;
}

static void
do_log(LogLevel level, int force, const char *suffix, const char *fmt,
    va_list args)
{
#if defined(HAVE_OPENLOG_R) && defined(SYSLOG_DATA_INIT)
	struct syslog_data sdata = SYSLOG_DATA_INIT;
#endif
	char msgbuf[MSGBUFSIZ];
	char fmtbuf[MSGBUFSIZ];
	char *txt = NULL;
	int pri = LOG_INFO;
	int saved_errno = errno;
	log_handler_fn *tmp_handler;
	const char *progname = argv0 != NULL ? argv0 : __progname;

	if (!force && level > log_level)
		return;

	switch (level) {
	case SYSLOG_LEVEL_FATAL:
		if (!log_on_stderr)
			txt = "fatal";
		pri = LOG_CRIT;
		break;
	case SYSLOG_LEVEL_ERROR:
		if (!log_on_stderr)
			txt = "error";
		pri = LOG_ERR;
		break;
	case SYSLOG_LEVEL_INFO:
		pri = LOG_INFO;
		break;
	case SYSLOG_LEVEL_VERBOSE:
		pri = LOG_INFO;
		break;
	case SYSLOG_LEVEL_DEBUG1:
		txt = "debug1";
		pri = LOG_DEBUG;
		break;
	case SYSLOG_LEVEL_DEBUG2:
		txt = "debug2";
		pri = LOG_DEBUG;
		break;
	case SYSLOG_LEVEL_DEBUG3:
		txt = "debug3";
		pri = LOG_DEBUG;
		break;
	default:
		txt = "internal error";
		pri = LOG_ERR;
		break;
	}
	if (txt != NULL && log_handler == NULL) {
		snprintf(fmtbuf, sizeof(fmtbuf), "%s: %s", txt, fmt);
		vsnprintf(msgbuf, sizeof(msgbuf), fmtbuf, args);
	} else {
		vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
	}
	if (suffix != NULL) {
		snprintf(fmtbuf, sizeof(fmtbuf), "%s: %s", msgbuf, suffix);
		strlcpy(msgbuf, fmtbuf, sizeof(msgbuf));
	}
	strnvis(fmtbuf, msgbuf, sizeof(fmtbuf),
	    log_on_stderr ? LOG_STDERR_VIS : LOG_SYSLOG_VIS);
	if (log_handler != NULL) {
		/* Avoid recursion */
		tmp_handler = log_handler;
		log_handler = NULL;
		tmp_handler(level, force, fmtbuf, log_handler_ctx);
		log_handler = tmp_handler;
	} else if (log_on_stderr) {
		snprintf(msgbuf, sizeof msgbuf, "%s%s%.*s\r\n",
		    (log_on_stderr > 1) ? progname : "",
		    (log_on_stderr > 1) ? ": " : "",
		    (int)sizeof msgbuf - 3, fmtbuf);
		(void)write(log_stderr_fd, msgbuf, strlen(msgbuf));
	} else {
#if defined(HAVE_OPENLOG_R) && defined(SYSLOG_DATA_INIT)
		openlog_r(progname, LOG_PID, log_facility, &sdata);
		syslog_r(pri, &sdata, "%.500s", fmtbuf);
		closelog_r(&sdata);
#else
		openlog(progname, LOG_PID, log_facility);
		syslog(pri, "%.500s", fmtbuf);
		closelog();
#endif
	}
	errno = saved_errno;
}

void
sshlog(const char *file, const char *func, int line, int showfunc,
    LogLevel level, const char *suffix, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	sshlogv(file, func, line, showfunc, level, suffix, fmt, args);
	va_end(args);
}

void
sshlogdie(const char *file, const char *func, int line, int showfunc,
    LogLevel level, const char *suffix, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	sshlogv(file, func, line, showfunc, SYSLOG_LEVEL_INFO,
	    suffix, fmt, args);
	va_end(args);
	cleanup_exit(255);
}

void
sshlogv(const char *file, const char *func, int line, int showfunc,
    LogLevel level, const char *suffix, const char *fmt, va_list args)
{
	char tag[128], fmt2[MSGBUFSIZ + 128];
	int forced = 0;
	const char *cp;
	size_t i;

	/* short circuit processing early if we're not going to log anything */
	if (nlog_verbose == 0 && level > log_level)
		return;

	snprintf(tag, sizeof(tag), "%.48s:%.48s():%d (pid=%ld)",
	    (cp = strrchr(file, '/')) == NULL ? file : cp + 1, func, line,
	    (long)getpid());
	for (i = 0; i < nlog_verbose; i++) {
		if (match_pattern_list(tag, log_verbose[i], 0) == 1) {
			forced = 1;
			break;
		}
	}

	if (forced)
		snprintf(fmt2, sizeof(fmt2), "%s: %s", tag, fmt);
	else if (showfunc)
		snprintf(fmt2, sizeof(fmt2), "%s: %s", func, fmt);
	else
		strlcpy(fmt2, fmt, sizeof(fmt2));

	do_log(level, forced, suffix, fmt2, args);
}

void
sshlogdirect(LogLevel level, int forced, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	do_log(level, forced, NULL, fmt, args);
	va_end(args);
}
