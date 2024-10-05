/*
 * Copyright (c) 1993, 1994, 1995, 1996, 1998
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <config.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <syslog.h>
#endif

#include "portability.h"

#include "log.h"

static int log_to_systemlog;
static int log_debug_messages;

static void rpcapd_vlog_stderr(log_priority,
    PCAP_FORMAT_STRING(const char *), va_list) PCAP_PRINTFLIKE(2, 0);

static void rpcapd_vlog_stderr(log_priority priority, const char *message, va_list ap)
{
	const char *tag;

	/*
	 * Squelch warnings from compilers that *don't* assume that
	 * priority always has a valid enum value and therefore don't
	 * assume that we'll always go through one of the case arms.
	 *
	 * If we have a default case, compilers that *do* assume that
	 * will then complain about the default case code being
	 * unreachable.
	 *
	 * Damned if you do, damned if you don't.
	 */
	tag = "";

	switch (priority) {

	case LOGPRIO_DEBUG:
		tag = "DEBUG: ";
		break;

	case LOGPRIO_INFO:
		tag = "";
		break;

	case LOGPRIO_WARNING:
		tag = "warning: ";
		break;

	case LOGPRIO_ERROR:
		tag = "error: ";
		break;
	}

	fprintf(stderr, "rpcapd: %s", tag);
	vfprintf(stderr, message, ap);
	putc('\n', stderr);
}

static void rpcapd_vlog_systemlog(log_priority,
    PCAP_FORMAT_STRING(const char *), va_list) PCAP_PRINTFLIKE(2, 0);

#ifdef _WIN32
#define MESSAGE_SUBKEY \
    "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\rpcapd"

static void rpcapd_vlog_systemlog(log_priority priority, const char *message,
    va_list ap)
{
#if 0
	static int initialized = 0;
	HKEY hey_handle;
	static HANDLE log_handle;
	WORD eventlog_type;
	DWORD event_id;
	char msgbuf[1024];
	char *strings[1];

	if (!initialized) {
		/*
		 * Register our message stuff in the Registry.
		 *
		 * First, create the registry key for us.  If the key
		 * already exists, this succeeds and returns a handle
		 * for it.
		 */
		if (RegCreateKey(HKEY_LOCAL_MACHINE, MESSAGE_SUBKEY,
		    &key_handle) != ERROR_SUCCESS) {
			/*
			 * Failed - give up and just log this message,
			 * and all subsequent messages, to the
			 * standard error.
			 */
			log_to_systemlog = 0;
			initialized = 1;
			rpcapd_vlog_stderr(priority, message, ap);
			return;
		}
		log_handle = RegisterEventSource(NULL, "rpcapd");
		initialized = 1;
	}

	switch (priority) {

	case LOGPRIO_DEBUG:
		//
		// XXX - what *should* we do about debug messages?
		//
		eventlog_type = EVENTLOG_INFORMATION_TYPE;
		event_id = RPCAPD_INFO_ID;
		break;

	case LOGPRIO_INFO:
		eventlog_type = EVENTLOG_INFORMATION_TYPE;
		event_id = RPCAPD_INFO_ID;
		break;

	case LOGPRIO_WARNING:
		eventlog_type = EVENTLOG_WARNING_TYPE;
		event_id = RPCAPD_WARNING_ID;
		break;

	case LOGPRIO_ERROR:
		eventlog_type = EVENTLOG_ERROR_TYPE;
		event_id = RPCAPD_ERROR_ID;
		break;

	default:
		/* Don't do this. */
		return;
	}

	vsprintf(msgbuf, message, ap);

	strings[0] = msgbuf;
	/*
	 * If this fails, how are we going to report it?
	 */
	(void) ReportEvent(log_handle, eventlog_type, 0, event_id, NULL, 1, 0,
	    strings, NULL);
#else
	rpcapd_vlog_stderr(priority, message, ap);
#endif
}
#else
static void rpcapd_vlog_systemlog(log_priority priority, const char *message,
    va_list ap)
{
	static int initialized = 0;
	int syslog_priority;

	if (!initialized) {
		//
		// Open the log.
		//
		openlog("rpcapd", LOG_PID, LOG_DAEMON);
		initialized = 1;
	}

	switch (priority) {

	case LOGPRIO_DEBUG:
		syslog_priority = LOG_DEBUG;
		break;

	case LOGPRIO_INFO:
		syslog_priority = LOG_INFO;
		break;

	case LOGPRIO_WARNING:
		syslog_priority = LOG_WARNING;
		break;

	case LOGPRIO_ERROR:
		syslog_priority = LOG_ERR;
		break;

	default:
		/* Don't do this. */
		return;
	}

#ifdef HAVE_VSYSLOG
	vsyslog(syslog_priority, message, ap);
#else
	/*
	 * Thanks, IBM, for not providing vsyslog() in AIX!
	 *
	 * They also warn that the syslog functions shouldn't
	 * be used in multithreaded programs, but the only thing
	 * obvious that seems to make the syslog_r functions
	 * better is that they have an additional argument
	 * that points to the information that's static to
	 * the syslog code in non-thread-safe versions.  Most
	 * of that data is set by openlog(); since we already
	 * do an openlog before doing logging, and don't
	 * change that data afterwards, I suspect that, in
	 * practice, the regular syslog routines are OK for
	 * us (especially given that we'd end up having one
	 * static struct syslog_data anyway, which means we'd
	 * just be like the non-thread-safe version).
	 */
	char logbuf[1024+1];

	vsnprintf(logbuf, sizeof logbuf, message, ap);
	syslog(syslog_priority, "%s", logbuf);
#endif
}
#endif

void rpcapd_log_set(int log_to_systemlog_arg, int log_debug_messages_arg)
{
	log_debug_messages = log_debug_messages_arg;
	log_to_systemlog = log_to_systemlog_arg;
}

void rpcapd_log(log_priority priority, const char *message, ...)
{
	va_list ap;

	if (priority != LOGPRIO_DEBUG || log_debug_messages) {
		va_start(ap, message);
		if (log_to_systemlog)
		{
			rpcapd_vlog_systemlog(priority, message, ap);
		}
		else
		{
			rpcapd_vlog_stderr(priority, message, ap);
		}
		va_end(ap);
	}
}
