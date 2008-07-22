/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Server-side versions of debug(), log(), etc.  These normally send the output
 * to the system log.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
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
RCSID("$OpenBSD: log-server.c,v 1.17 2000/09/12 20:53:10 markus Exp $");

#include <syslog.h>
#include "packet.h"
#include "xmalloc.h"
#include "ssh.h"

static LogLevel log_level = SYSLOG_LEVEL_INFO;
static int log_on_stderr = 0;
static int log_facility = LOG_AUTH;

/* Initialize the log.
 *   av0	program name (should be argv[0])
 *   on_stderr	print also on stderr
 *   level	logging level
 */

void
log_init(char *av0, LogLevel level, SyslogFacility facility, int on_stderr)
{
	switch (level) {
	case SYSLOG_LEVEL_QUIET:
	case SYSLOG_LEVEL_ERROR:
	case SYSLOG_LEVEL_FATAL:
	case SYSLOG_LEVEL_INFO:
	case SYSLOG_LEVEL_VERBOSE:
	case SYSLOG_LEVEL_DEBUG1:
	case SYSLOG_LEVEL_DEBUG2:
	case SYSLOG_LEVEL_DEBUG3:
		log_level = level;
		break;
	default:
		fprintf(stderr, "Unrecognized internal syslog level code %d\n",
			(int) level);
		exit(1);
	}
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
		fprintf(stderr, "Unrecognized internal syslog facility code %d\n",
			(int) facility);
		exit(1);
	}
	log_on_stderr = on_stderr;
}

#define MSGBUFSIZ 1024

void
do_log(LogLevel level, const char *fmt, va_list args)
{
	char msgbuf[MSGBUFSIZ];
	char fmtbuf[MSGBUFSIZ];
	char *txt = NULL;
	int pri = LOG_INFO;
	extern char *__progname;

	if (level > log_level)
		return;
	switch (level) {
	case SYSLOG_LEVEL_ERROR:
		txt = "error";
		pri = LOG_ERR;
		break;
	case SYSLOG_LEVEL_FATAL:
		txt = "fatal";
		pri = LOG_ERR;
		break;
	case SYSLOG_LEVEL_INFO:
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
	if (txt != NULL) {
		snprintf(fmtbuf, sizeof(fmtbuf), "%s: %s", txt, fmt);
		vsnprintf(msgbuf, sizeof(msgbuf), fmtbuf, args);
	} else {
		vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
	}
	if (log_on_stderr) {
		fprintf(stderr, "%s\n", msgbuf);
	} else {
		openlog(__progname, LOG_PID, log_facility);
		syslog(pri, "%.500s", msgbuf);
		closelog();
	}
}
