/*
 * 
 * log-client.c
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * 
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * 
 * Created: Mon Mar 20 21:13:40 1995 ylo
 * 
 * Client-side versions of debug(), log(), etc.  These print to stderr.
 * This is a stripped down version of log-server.c.
 * 
 */

#include "includes.h"
RCSID("$Id: log-client.c,v 1.6 1999/11/24 00:26:02 deraadt Exp $");

#include "xmalloc.h"
#include "ssh.h"

static LogLevel log_level = SYSLOG_LEVEL_INFO;

/* Initialize the log.
 *   av0	program name (should be argv[0])
 *   level	logging level
 */

void
log_init(char *av0, LogLevel level, SyslogFacility ignored1, int ignored2)
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
		/* unchanged */
		break;
	}
}

#define MSGBUFSIZE 1024

void
do_log(LogLevel level, const char *fmt, va_list args)
{
	char msgbuf[MSGBUFSIZE];

	if (level > log_level)
		return;
	if (level == SYSLOG_LEVEL_DEBUG)
		fprintf(stderr, "debug: ");
	vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
	fprintf(stderr, "%s", msgbuf);
	fprintf(stderr, "\r\n");
}
