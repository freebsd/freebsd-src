/*
 * msyslog - either send a message to the terminal or print it on
 *	     the standard output.
 *
 * Converted to use varargs, much better ... jks
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdio.h>

#include "ntp_types.h"
#include "ntp_string.h"
#include "ntp_syslog.h"
#include "ntp_stdlib.h"

#ifdef SYS_WINNT
# include "..\ports\winnt\libntp\log.h"
# include "..\ports\winnt\libntp\messages.h"
#endif

int syslogit = 1;

FILE *syslog_file = NULL;

u_long ntp_syslogmask =  ~ (u_long) 0;

#ifdef SYS_WINNT
HANDLE  hEventSource;
LPTSTR lpszStrings[1];
static WORD event_type[] = {
	EVENTLOG_ERROR_TYPE, EVENTLOG_ERROR_TYPE, EVENTLOG_ERROR_TYPE, EVENTLOG_ERROR_TYPE,
	EVENTLOG_WARNING_TYPE,
	EVENTLOG_INFORMATION_TYPE, EVENTLOG_INFORMATION_TYPE, EVENTLOG_INFORMATION_TYPE,
};
#endif /* SYS_WINNT */
extern	char *progname;

#if defined(__STDC__) || defined(HAVE_STDARG_H)
void msyslog(int level, const char *fmt, ...)
#else /* defined(__STDC__) || defined(HAVE_STDARG_H) */
     /*VARARGS*/
     void msyslog(va_alist)
     va_dcl
#endif /* defined(__STDC__) || defined(HAVE_STDARG_H) */
{
#if defined(__STDC__) || defined(HAVE_STDARG_H)
#else
	int level;
	const char *fmt;
#endif
	va_list ap;
	char buf[1025], nfmt[256];
#if defined(SYS_WINNT)
	char xerr[50];
#endif
	register int c;
	register char *n, *prog;
	register const char *f;
	int olderrno;
	char *err;

#if defined(__STDC__) || defined(HAVE_STDARG_H)
	va_start(ap, fmt);
#else
	va_start(ap);

	level = va_arg(ap, int);
	fmt = va_arg(ap, char *);
#endif

	olderrno = errno;
	n = nfmt;
	f = fmt;
	while ((c = *f++) != '\0' && c != '\n' && n < &nfmt[252]) {
		if (c != '%') {
			*n++ = c;
			continue;
		}
		if ((c = *f++) != 'm') {
			*n++ = '%';
			*n++ = c;
			continue;
		}
		err = 0;
#if !defined(SYS_WINNT)
		err = strerror(olderrno);
#else  /* SYS_WINNT */
		err = xerr;
		FormatMessage( 
			FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			GetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* Default language */
			(LPTSTR) err,
			sizeof(xerr),
			NULL);

#endif /* SYS_WINNT */
		if ((n + strlen(err)) < &nfmt[254]) {
			strcpy(n, err);
			n += strlen(err);
		}
	}
#if !defined(VMS)
	if (!syslogit)
#endif /* VMS */
	    *n++ = '\n';
	*n = '\0';

	vsnprintf(buf, sizeof(buf), nfmt, ap);
#if !defined(VMS) && !defined (SYS_VXWORKS)
	if (syslogit)
#ifndef SYS_WINNT
	    syslog(level, "%s", buf);
#else
	{
		lpszStrings[0] = buf;
 
		switch (event_type[level])
		{
		    case EVENTLOG_ERROR_TYPE:
			reportAnEEvent(NTP_ERROR,1,lpszStrings);
			break;
		    case EVENTLOG_INFORMATION_TYPE:
			reportAnIEvent(NTP_INFO,1,lpszStrings);
			break;
		    case EVENTLOG_WARNING_TYPE:
			reportAnWEvent(NTP_WARNING,1,lpszStrings);
			break;
		} /* switch end */

	} 
#endif /* SYS_WINNT */
	else
#endif /* VMS  && SYS_VXWORKS*/
	{
		FILE *out_file = syslog_file ? syslog_file
			: level <= LOG_ERR ? stderr : stdout;
		/* syslog() provides the timestamp, so if we're not using
		   syslog, we must provide it. */
		prog = strrchr(progname, '/');
		if (prog == NULL)
		    prog = progname;
		else
		    prog++;
		(void) fprintf(out_file, "%s ", humanlogtime ());
		(void) fprintf(out_file, "%s[%d]: %s", prog, (int)getpid(), buf);
		fflush (out_file);
	}
	va_end(ap);
}
