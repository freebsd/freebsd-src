/*
 * Copyright (C) 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* From BIND 9 lib/isc/win32/: syslog.c,v 1.6 2002/08/03 01:34:14 mayer */

#include <config.h>

#include <stdio.h>
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>

#include <isc/strerror.h>
#include "ntp_stdlib.h"

#include "messages.h"

static HANDLE hAppLog = NULL;
static FILE *log_stream;
static int debug_level = 0;
static char progname[51] = "NTP";
static int logmask = 0;

/*
 * Log to the NT Event Log
 */
void
syslog(int level, const char *fmt, ...) {
	va_list ap;
	char buf[1024];
	char *str[1];

	str[0] = buf;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	/* Make sure that the channel is open to write the event */
	if (hAppLog == NULL) {
		openlog(progname, LOG_PID);
	}
	switch (level) {
		case LOG_INFO:
		case LOG_NOTICE:
		case LOG_DEBUG:
			ReportEvent(hAppLog, EVENTLOG_INFORMATION_TYPE, 0,
				    NTP_INFO, NULL, 1, 0, str, NULL);
			break;
		case LOG_WARNING:
			ReportEvent(hAppLog, EVENTLOG_WARNING_TYPE, 0,
				    NTP_WARNING, NULL, 1, 0, str, NULL);
			break;
		default:
			ReportEvent(hAppLog, EVENTLOG_ERROR_TYPE, 0,
				    NTP_ERROR, NULL, 1, 0, str, NULL);
			break;
		}
}

/*
 * Initialize event logging
 */
void
openlog(const char *name, int flags, ...) {
	/* Get a handle to the Application event log */
	hAppLog = RegisterEventSource(NULL, progname);
	strlcpy(progname, name, sizeof(progname));
}

/*
 * Close the Handle to the application Event Log
 * We don't care whether or not we succeeded so ignore return values
 * In fact if we failed then we would have nowhere to put the message
 */
void
closelog() {
	DeregisterEventSource(hAppLog);
}

/*
 * Keep event logging synced with the current debug level
 */
void
ModifyLogLevel(int level) {
	debug_level = level;	
}
/*
 * Set the log priority mask to the given value.
 * Return the previous priority mask
 * Note that this setting is ignored in Win32
 */
int
setlogmask(int maskpri) {
	int temp = logmask;
	logmask = maskpri;
	return (temp);
}

/*
 * Initialize logging for the port section of libbind.
 * Piggyback onto stream given.
 */
void
InitNTLogging(FILE *stream, int debug) {
	log_stream = stream;
	ModifyLogLevel(debug);
}
/*
 * This function is for reporting errors to the application
 * event log in case the regular syslog is not available
 * mainly during startup. It should not be used under normal
 * circumstances.
 */
void
NTReportError(const char *name, const char *str) {
	HANDLE hNTAppLog = NULL;
	const char *buf[1];

	buf[0] = str;

	hNTAppLog = RegisterEventSource(NULL, name);

	ReportEvent(hNTAppLog, EVENTLOG_ERROR_TYPE, 0,
		    NTP_ERROR, NULL, 1, 0, buf, NULL);

	DeregisterEventSource(hNTAppLog);
}


/*
 * ntp_strerror() - provide strerror()-compatible wrapper for libisc's
 *		    isc__strerror(), which knows about Windows as well as
 *		    C runtime error messages.
 */

char *
ntp_strerror(
	int code
	)
{
	char *	buf;

	LIB_GETBUF(buf);
	isc__strerror(code, buf, LIB_BUFLENGTH);

	return buf;
}
