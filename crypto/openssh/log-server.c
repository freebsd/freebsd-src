/*
 * 
 * log-server.c
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * 
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * 
 * Created: Mon Mar 20 21:19:30 1995 ylo
 * 
 * Server-side versions of debug(), log(), etc.  These normally send the output
 * to the system log.
 * 
 */

#include "includes.h"
RCSID("$Id: log-server.c,v 1.11 1999/11/24 00:26:02 deraadt Exp $");

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
	case SYSLOG_LEVEL_DEBUG:
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

#define MSGBUFSIZE 1024

void
do_log(LogLevel level, const char *fmt, va_list args)
{
	char msgbuf[MSGBUFSIZE];
	char fmtbuf[MSGBUFSIZE];
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
	case SYSLOG_LEVEL_DEBUG:
		txt = "debug";
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
	if (log_on_stderr)
		fprintf(stderr, "%s\n", msgbuf);
	openlog(__progname, LOG_PID, log_facility);
	syslog(pri, "%.500s", msgbuf);
	closelog();
}
