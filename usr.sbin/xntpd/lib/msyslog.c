/* msyslog.c,v 3.1 1993/07/06 01:08:36 jbj Exp
 * msyslog - either send a message to the terminal or print it on
 *	     the standard output.
 *
 * Converted to use varargs, much better ... jks
 */
#include <stdio.h>
#include <errno.h>

/* alternative, as Solaris 2.x defines __STDC__ as 0 in a largely standard
   conforming environment
   #if __STDC__ || (defined(SOLARIS) && defined(__STDC__))
*/
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "ntp_types.h"
#include "ntp_string.h"
#include "ntp_syslog.h"
#include "ntp_stdlib.h"

#undef syslog

int syslogit = 1;
FILE *syslog_file = NULL;

extern	int errno;
extern	char *progname;

#if defined(__STDC__)
void msyslog(int level, char *fmt, ...)
#else
/*VARARGS*/
void msyslog(va_alist)
	va_dcl
#endif
{
#ifndef __STDC__
	int level;
	char *fmt;
#endif
	va_list ap;
	char buf[1025], nfmt[256], xerr[50], *err;
	register int c, l;
	register char *n, *f, *prog;
	extern int sys_nerr;
	extern char *sys_errlist[];
	int olderrno;

#ifdef __STDC__
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
		if ((unsigned)olderrno > sys_nerr)
			sprintf((err = xerr), "error %d", olderrno);
		else
			err = sys_errlist[olderrno];
		if (n + (l = strlen(err)) < &nfmt[254]) {
			strcpy(n, err);
			n += strlen(err);
		}
	}
	*n++ = '\n';
	*n = '\0';

	vsprintf(buf, nfmt, ap);
	if (syslogit)
		syslog(level, buf);
	else {
		extern char * humanlogtime P((void));

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
                (void) fprintf(out_file, "%s: %s", prog, buf);
		fflush (out_file);
	}
	va_end(ap);
}
