#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
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
 */
void
err(char *fmt, ...)
    {
    va_list ap;
    time_t now;
    struct tm *tm;
    FILE *fp;

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

    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);

    fprintf(fp, "\n");
    fflush(fp);
    }
