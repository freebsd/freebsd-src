/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: log.c,v 4.7 88/12/01 14:15:14 jtkohl Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef lint
static char *rcsid =
"$FreeBSD$";
#endif /* lint */
#endif

#include <sys/time.h>
#include <stdio.h>
#include <krb.h>
#include <klog.h>

static char *log_name = KRBLOG;
static is_open;

/*
 * This file contains three logging routines: set_logfile()
 * to determine the file that log entries should be written to;
 * and log() and new_log() to write log entries to the file.
 */

/*
 * log() is used to add entries to the logfile (see set_logfile()
 * below).  Note that it is probably not portable since it makes
 * assumptions about what the compiler will do when it is called
 * with less than the correct number of arguments which is the
 * way it is usually called.
 *
 * The log entry consists of a timestamp and the given arguments
 * printed according to the given "format".
 *
 * The log file is opened and closed for each log entry.
 *
 * The return value is undefined.
 */

__BEGIN_DECLS
char	*month_sname __P((int));
__END_DECLS


/*VARARGS1 */
void log(format,a1,a2,a3,a4,a5,a6,a7,a8,a9,a0)
    char *format;
    int a1,a2,a3,a4,a5,a6,a7,a8,a9,a0;
{
    FILE *logfile, *fopen();
    long time(),now;
    struct tm *tm;

    if ((logfile = fopen(log_name,"a")) == NULL)
        return;

    (void) time(&now);
    tm = localtime(&now);

    fprintf(logfile,"%2d-%s-%02d %02d:%02d:%02d ",tm->tm_mday,
            month_sname(tm->tm_mon + 1),tm->tm_year,
            tm->tm_hour, tm->tm_min, tm->tm_sec);
    fprintf(logfile,format,a1,a2,a3,a4,a5,a6,a7,a8,a9,a0);
    fprintf(logfile,"\n");
    (void) fclose(logfile);
    return;
}

/*
 * set_logfile() changes the name of the file to which
 * messages are logged.  If set_logfile() is not called,
 * the logfile defaults to KRBLOG, defined in "krb.h".
 */

void
set_logfile(filename)
    char *filename;
{
    log_name = filename;
    is_open = 0;
}

/*
 * new_log() appends a log entry containing the give time "t" and the
 * string "string" to the logfile (see set_logfile() above).  The file
 * is opened once and left open.  The routine returns 1 on failure, 0
 * on success.
 */

int
new_log(t,string)
    long t;
    char *string;
{
    static FILE *logfile;

    long time();
    struct tm *tm;

    if (!is_open) {
        if ((logfile = fopen(log_name,"a")) == NULL) return(1);
        is_open = 1;
    }

    if (t) {
        tm = localtime(&t);

        fprintf(logfile,"\n%2d-%s-%02d %02d:%02d:%02d  %s",tm->tm_mday,
                month_sname(tm->tm_mon + 1),tm->tm_year,
                tm->tm_hour, tm->tm_min, tm->tm_sec, string);
    }
    else {
        fprintf(logfile,"\n%20s%s","",string);
    }

    (void) fflush(logfile);
    return(0);
}
