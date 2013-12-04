#include <config.h>

#include "log.h"

char *progname = "sntp";	/* for msyslog use too */

static void cleanup_log(void);

void
init_logging(void)
{
	openlog(progname, LOG_PID | LOG_CONS, OPENLOG_FAC);
	msyslog_term = TRUE;
}


void
open_logfile(
	const char *logfile
	)
{
	syslog_file = fopen(logfile, "a");	
	if (syslog_file == NULL) {
		msyslog(LOG_ERR, "sntp: Cannot open logfile %s",
			logfile);
		return;
	}
	syslogit = FALSE;
	atexit(cleanup_log);
}


static void
cleanup_log(void)
{
	syslogit = TRUE;
	fflush(syslog_file);
	fclose(syslog_file);
	syslog_file = NULL;
}
