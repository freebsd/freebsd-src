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
 * Shared versions of debug(), log(), etc.
 *
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
RCSID("$OpenBSD: log.c,v 1.11 2000/09/30 16:27:43 markus Exp $");

#include "ssh.h"
#include "xmalloc.h"

/* Fatal messages.  This function never returns. */

void
fatal(const char *fmt,...)
{
	va_list args;
	va_start(args, fmt);
	do_log(SYSLOG_LEVEL_FATAL, fmt, args);
	va_end(args);
	fatal_cleanup();
}

/* Error messages that should be logged. */

void
error(const char *fmt,...)
{
	va_list args;
	va_start(args, fmt);
	do_log(SYSLOG_LEVEL_ERROR, fmt, args);
	va_end(args);
}

/* Log this message (information that usually should go to the log). */

void
log(const char *fmt,...)
{
	va_list args;
	va_start(args, fmt);
	do_log(SYSLOG_LEVEL_INFO, fmt, args);
	va_end(args);
}

/* More detailed messages (information that does not need to go to the log). */

void
verbose(const char *fmt,...)
{
	va_list args;
	va_start(args, fmt);
	do_log(SYSLOG_LEVEL_VERBOSE, fmt, args);
	va_end(args);
}

/* Debugging messages that should not be logged during normal operation. */

void
debug(const char *fmt,...)
{
	va_list args;
	va_start(args, fmt);
	do_log(SYSLOG_LEVEL_DEBUG1, fmt, args);
	va_end(args);
}

void
debug2(const char *fmt,...)
{
	va_list args;
	va_start(args, fmt);
	do_log(SYSLOG_LEVEL_DEBUG2, fmt, args);
	va_end(args);
}

void
debug3(const char *fmt,...)
{
	va_list args;
	va_start(args, fmt);
	do_log(SYSLOG_LEVEL_DEBUG3, fmt, args);
	va_end(args);
}

/* Fatal cleanup */

struct fatal_cleanup {
	struct fatal_cleanup *next;
	void (*proc) (void *);
	void *context;
};

static struct fatal_cleanup *fatal_cleanups = NULL;

/* Registers a cleanup function to be called by fatal() before exiting. */

void
fatal_add_cleanup(void (*proc) (void *), void *context)
{
	struct fatal_cleanup *cu;

	cu = xmalloc(sizeof(*cu));
	cu->proc = proc;
	cu->context = context;
	cu->next = fatal_cleanups;
	fatal_cleanups = cu;
}

/* Removes a cleanup frunction to be called at fatal(). */

void
fatal_remove_cleanup(void (*proc) (void *context), void *context)
{
	struct fatal_cleanup **cup, *cu;

	for (cup = &fatal_cleanups; *cup; cup = &cu->next) {
		cu = *cup;
		if (cu->proc == proc && cu->context == context) {
			*cup = cu->next;
			xfree(cu);
			return;
		}
	}
	fatal("fatal_remove_cleanup: no such cleanup function: 0x%lx 0x%lx\n",
	      (unsigned long) proc, (unsigned long) context);
}

/* Cleanup and exit */
void
fatal_cleanup(void)
{
	struct fatal_cleanup *cu, *next_cu;
	static int called = 0;

	if (called)
		exit(255);
	called = 1;
	/* Call cleanup functions. */
	for (cu = fatal_cleanups; cu; cu = next_cu) {
		next_cu = cu->next;
		debug("Calling cleanup 0x%lx(0x%lx)",
		      (unsigned long) cu->proc, (unsigned long) cu->context);
		(*cu->proc) (cu->context);
	}
	exit(255);
}

/* textual representation of log-facilities/levels */

static struct {
	const char *name;
	SyslogFacility val;
} log_facilities[] = {
	{ "DAEMON",	SYSLOG_FACILITY_DAEMON },
	{ "USER",	SYSLOG_FACILITY_USER },
	{ "AUTH",	SYSLOG_FACILITY_AUTH },
	{ "LOCAL0",	SYSLOG_FACILITY_LOCAL0 },
	{ "LOCAL1",	SYSLOG_FACILITY_LOCAL1 },
	{ "LOCAL2",	SYSLOG_FACILITY_LOCAL2 },
	{ "LOCAL3",	SYSLOG_FACILITY_LOCAL3 },
	{ "LOCAL4",	SYSLOG_FACILITY_LOCAL4 },
	{ "LOCAL5",	SYSLOG_FACILITY_LOCAL5 },
	{ "LOCAL6",	SYSLOG_FACILITY_LOCAL6 },
	{ "LOCAL7",	SYSLOG_FACILITY_LOCAL7 },
	{ NULL, 0 }
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
	{ NULL, 0 }
};

SyslogFacility
log_facility_number(char *name)
{
	int i;
	if (name != NULL)
		for (i = 0; log_facilities[i].name; i++)
			if (strcasecmp(log_facilities[i].name, name) == 0)
				return log_facilities[i].val;
	return (SyslogFacility) - 1;
}

LogLevel
log_level_number(char *name)
{
	int i;
	if (name != NULL)
		for (i = 0; log_levels[i].name; i++)
			if (strcasecmp(log_levels[i].name, name) == 0)
				return log_levels[i].val;
	return (LogLevel) - 1;
}
