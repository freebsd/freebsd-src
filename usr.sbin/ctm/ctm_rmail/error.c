/*
 * Routines for logging error messages or other informative messages.
 *
 * Log messages can easily contain the program name, a time stamp, system
 * error messages, and arbitrary printf-style strings, and can be directed
 * to stderr or a log file.
 *
 * Author: Stephen McKay
 *
 * NOTICE: This is free software.  I hope you get some use from this program.
 * In return you should think about all the nice people who give away software.
 * Maybe you should write some free software too.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include "error.h"

static FILE *error_fp = NULL;
static char *prog = NULL;


/*
 * Log errors to the given file.
 */
void
err_set_log(char *log_file)
    {
    FILE *fp;

    if ((fp = fopen(log_file, "a")) == NULL)
	err("cannot log to '%s'", log_file);
    else
	error_fp = fp;
    }


/*
 * Set the error prefix if not logging to a file.
 */
void
err_prog_name(char *name)
    {
    if ((prog = strrchr(name, '/')) == NULL)
	prog = name;
    else
	prog++;
    }


/*
 * Log an error.
 *
 * A leading '*' in the message format means we want the system errno
 * decoded and appended.
 */
void
err(const char *fmt, ...)
    {
    va_list ap;
    time_t now;
    struct tm *tm;
    FILE *fp;
    int x = errno;
    int want_errno;

    if ((fp = error_fp) == NULL)
	{
	fp = stderr;
	if (prog != NULL)
	    fprintf(fp, "%s: ", prog);
	}
    else
	{
	time(&now);
	tm = localtime(&now);
	fprintf(fp, "%04d-%02d-%02d %02d:%02d ", tm->tm_year+1900,
	    tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);
	}

    want_errno = 0;
    if (*fmt == '*')
	want_errno++, fmt++;

    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);

    if (want_errno)
	fprintf(fp, ": %s", strerror(x));

    fprintf(fp, "\n");
    fflush(fp);
    }
