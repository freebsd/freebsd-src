/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "krb_locl.h"
#include <klog.h>

RCSID("$Id: logging.c,v 1.18.2.1 2000/10/13 15:57:34 assar Exp $");

struct krb_log_facility {
    char filename[MaxPathLen]; 
    FILE *file; 
    krb_log_func_t func;
};

int
krb_vlogger(struct krb_log_facility *f, const char *format, va_list args)
{
    FILE *file = NULL;
    int ret;

    if (f->file != NULL)
	file = f->file;
    else if (f->filename && f->filename[0])
	file = fopen(f->filename, "a");

    if (file == NULL)
      return KFAILURE;

    ret = f->func(file, format, args);

    if (file != f->file)
	fclose(file);
    return ret;
}

int
krb_logger(struct krb_log_facility *f, const char *format, ...)
{
    va_list args;
    int ret;
    va_start(args, format);
    ret = krb_vlogger(f, format, args);
    va_end(args);
    return ret;
}

/*
 * If FILE * is given log to it, otherwise, log to filename. When
 * given a file name the file is opened and closed for each log
 * record.
 */
int
krb_openlog(struct krb_log_facility *f,
	    char *filename,
	    FILE *file,
	    krb_log_func_t func)
{
    strlcpy(f->filename, filename, MaxPathLen);
    f->file = file;
    f->func = func;
    return KSUCCESS;
}

/* ------------------------------------------------------------
   Compatibility functions from warning.c
   ------------------------------------------------------------ */

static int
log_tty(FILE *f, const char *format,  va_list args)
{
    if (f != NULL && isatty(fileno(f)))
	vfprintf(f, format, args);
    return KSUCCESS;
}

/* stderr */
static struct krb_log_facility std_log = { "/dev/tty", NULL, log_tty };

static void
init_std_log (void)
{
  static int done = 0;

  if (!done) {
    std_log.file = stderr;
    done = 1;
  }
}

/*
 *
 */
void
krb_set_warnfn (krb_warnfn_t newfunc)
{
    init_std_log ();
    std_log.func =  newfunc;
}

/*
 *
 */
krb_warnfn_t
krb_get_warnfn (void)
{
    init_std_log ();
    return std_log.func;
}

/*
 * Log warnings to stderr if it's a tty.
 */
void
krb_warning (const char *format, ...)
{
    va_list args;
    
    init_std_log ();
    va_start(args, format);
    krb_vlogger(&std_log, format, args);
    va_end(args);
}

/* ------------------------------------------------------------
   Compatibility functions from klog.c and log.c
   ------------------------------------------------------------ */

/*
 * Used by kerberos and kadmind daemons and in libkrb (rd_req.c).
 *
 * By default they log to the kerberos server log-file (KRBLOG) to be
 * backwards compatible.
 */

static int
log_with_timestamp_and_nl(FILE *file, const char *format, va_list args)
{
    time_t now;
    if(file == NULL)
	return KFAILURE;
    time(&now);
    fputs(krb_stime(&now), file);
    fputs(": ", file);
    vfprintf(file, format, args);
    fputs("\n", file);
    fflush(file);
    return KSUCCESS;
}

static struct krb_log_facility
file_log = { KRBLOG, NULL, log_with_timestamp_and_nl };

/*
 * kset_logfile() changes the name of the file to which
 * messages are logged.  If kset_logfile() is not called,
 * the logfile defaults to KRBLOG, defined in "krb.h".
 */

void
kset_logfile(char *filename)
{
    krb_openlog(&file_log, filename, NULL, log_with_timestamp_and_nl);
}

/*
 * krb_log() and klog() is used to add entries to the logfile.
 *
 * The log entry consists of a timestamp and the given arguments
 * printed according to the given "format" string.
 *
 * The log file is opened and closed for each log entry.
 *
 * If the given log type "type" is unknown, or if the log file
 * cannot be opened, no entry is made to the log file.
 *
 * CHANGE: the type is always ignored
 *
 * The return value of klog() is always a pointer to the formatted log
 * text string "logtxt".
 */

/* Used in kerberos.c only. */
char *
klog(int type, const char *format, ...)
{
    static char logtxt[1024];

    va_list ap;

    va_start(ap, format);
    vsnprintf(logtxt, sizeof(logtxt), format, ap);
    va_end(ap);
    
    krb_logger(&file_log, "%s", logtxt);
    
    return logtxt;
}

/* Used in kadmind and rd_req.c */
void
krb_log(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    krb_vlogger(&file_log, format, args);
    va_end(args);
}
