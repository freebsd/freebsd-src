/*
 * Copyright (c) 1997-2003 Erez Zadok
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgment:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      %W% (Berkeley) %G%
 *
 * $Id: xutil.c,v 1.11.2.12 2003/04/04 15:53:35 ezk Exp $
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amu.h>

/*
 * Logfp is the default logging device, and is initialized to stderr by
 * default in dplog/plog below, and in
 * amd/amfs_program.c:amfs_program_exec().
 */
FILE *logfp = NULL;

static char *am_progname = "unknown";	/* "amd" */
static char am_hostname[MAXHOSTNAMELEN + 1] = "unknown"; /* Hostname */
pid_t am_mypid = -1;		/* process ID */
serv_state amd_state;		/* amd's state */
int foreground = 1;		/* 1 == this is the top-level server */
#ifdef DEBUG
int debug_flags = 0;
#endif /* DEBUG */

#ifdef HAVE_SYSLOG
int syslogging;
#endif /* HAVE_SYSLOG */
int xlog_level = XLOG_ALL & ~XLOG_MAP & ~XLOG_STATS;
int xlog_level_init = ~0;
static int amd_program_number = AMQ_PROGRAM;

time_t clock_valid = 0;
time_t xclock_valid = 0;

#ifdef DEBUG_MEM
static int mem_bytes;
static int orig_mem_bytes;
#endif /* DEBUG_MEM */

/* forward definitions */
/* for GCC format string auditing */
static void real_plog(int lvl, const char *fmt, va_list vargs)
     __attribute__((__format__(__printf__, 2, 0)));


#ifdef DEBUG
/*
 * List of debug options.
 */
struct opt_tab dbg_opt[] =
{
  {"all", D_ALL},		/* All */
  {"amq", D_AMQ},		/* Register for AMQ program */
  {"daemon", D_DAEMON},		/* Enter daemon mode */
  {"fork", D_FORK},		/* Fork server (nofork = don't fork) */
  {"full", D_FULL},		/* Program trace */
#ifdef HAVE_CLOCK_GETTIME
  {"hrtime", D_HRTIME},		/* Print high resolution time stamps */
#endif /* HAVE_CLOCK_GETTIME */
  /* info service specific debugging (hesiod, nis, etc) */
  {"info", D_INFO},
# ifdef DEBUG_MEM
  {"mem", D_MEM},		/* Trace memory allocations */
# endif /* DEBUG_MEM */
  {"mtab", D_MTAB},		/* Use local mtab file */
  {"readdir", D_READDIR},	/* check on browsable_dirs progress */
  {"str", D_STR},		/* Debug string munging */
  {"test", D_TEST},		/* Full debug - but no daemon */
  {"trace", D_TRACE},		/* Protocol trace */
  {"xdrtrace", D_XDRTRACE},	/* Trace xdr routines */
  {0, 0}
};
#endif /* DEBUG */

/*
 * List of log options
 */
struct opt_tab xlog_opt[] =
{
  {"all", XLOG_ALL},		/* All messages */
#ifdef DEBUG
  {"debug", XLOG_DEBUG},	/* Debug messages */
#endif /* DEBUG */		/* DEBUG */
  {"error", XLOG_ERROR},	/* Non-fatal system errors */
  {"fatal", XLOG_FATAL},	/* Fatal errors */
  {"info", XLOG_INFO},		/* Information */
  {"map", XLOG_MAP},		/* Map errors */
  {"stats", XLOG_STATS},	/* Additional statistical information */
  {"user", XLOG_USER},		/* Non-fatal user errors */
  {"warn", XLOG_WARNING},	/* Warnings */
  {"warning", XLOG_WARNING},	/* Warnings */
  {0, 0}
};


void
am_set_progname(char *pn)
{
  am_progname = pn;
}


const char *
am_get_progname(void)
{
  return am_progname;
}


void
am_set_hostname(char *hn)
{
  strncpy(am_hostname, hn, MAXHOSTNAMELEN);
  am_hostname[MAXHOSTNAMELEN] = '\0';
}


const char *
am_get_hostname(void)
{
  return am_hostname;
}


pid_t
am_set_mypid(void)
{
  am_mypid = getpid();
  return am_mypid;
}


voidp
xmalloc(int len)
{
  voidp p;
  int retries = 600;

  /*
   * Avoid malloc's which return NULL for malloc(0)
   */
  if (len == 0)
    len = 1;

  do {
    p = (voidp) malloc((unsigned) len);
    if (p) {
#if defined(DEBUG) && defined(DEBUG_MEM)
      amuDebug(D_MEM)
	plog(XLOG_DEBUG, "Allocated size %d; block %#x", len, p);
#endif /* defined(DEBUG) && defined(DEBUG_MEM) */
      return p;
    }
    if (retries > 0) {
      plog(XLOG_ERROR, "Retrying memory allocation");
      sleep(1);
    }
  } while (--retries);

  plog(XLOG_FATAL, "Out of memory");
  going_down(1);

  abort();

  return 0;
}


/* like xmalloc, but zeros out the bytes */
voidp
xzalloc(int len)
{
  voidp p = xmalloc(len);

  if (p)
    memset(p, 0, len);
  return p;
}


voidp
xrealloc(voidp ptr, int len)
{
#if defined(DEBUG) && defined(DEBUG_MEM)
  amuDebug(D_MEM) plog(XLOG_DEBUG, "Reallocated size %d; block %#x", len, ptr);
#endif /* defined(DEBUG) && defined(DEBUG_MEM) */

  if (len == 0)
    len = 1;

  if (ptr)
    ptr = (voidp) realloc(ptr, (unsigned) len);
  else
    ptr = (voidp) xmalloc((unsigned) len);

  if (!ptr) {
    plog(XLOG_FATAL, "Out of memory in realloc");
    going_down(1);
    abort();
  }
  return ptr;
}


#if defined(DEBUG) && defined(DEBUG_MEM)
void
dxfree(char *file, int line, voidp ptr)
{
  amuDebug(D_MEM)
    plog(XLOG_DEBUG, "Free in %s:%d: block %#x", file, line, ptr);
  /* this is the only place that must NOT use XFREE()!!! */
  free(ptr);
  ptr = NULL;			/* paranoid */
}
#endif /* defined(DEBUG) && defined(DEBUG_MEM) */


#ifdef DEBUG_MEM
static void
checkup_mem(void)
{
  struct mallinfo mi = mallinfo();
  u_long uordbytes = mi.uordblks * 4096;

  if (mem_bytes != uordbytes) {
    if (orig_mem_bytes == 0)
      mem_bytes = orig_mem_bytes = uordbytes;
    else {
      fprintf(logfp, "%s[%ld]: ", am_get_progname(), (long) am_mypid);
      if (mem_bytes < uordbytes) {
	fprintf(logfp, "ALLOC: %ld bytes", uordbytes - mem_bytes);
      } else {
	fprintf(logfp, "FREE: %ld bytes", mem_bytes - uordbytes);
      }
      mem_bytes = uordbytes;
      fprintf(logfp, ", making %d missing\n", mem_bytes - orig_mem_bytes);
    }
  }
  malloc_verify();
}
#endif /* DEBUG_MEM */


/*
 * Take a log format string and expand occurrences of %m
 * with the current error code taken from errno.  Make sure
 * 'e' never gets longer than maxlen characters.
 */
static const char *
expand_error(const char *f, char *e, int maxlen)
{
  const char *p;
  char *q;
  int error = errno;
  int len = 0;

  for (p = f, q = e; (*q = *p) && len < maxlen; len++, q++, p++) {
    if (p[0] == '%' && p[1] == 'm') {
      strcpy(q, strerror(error));
      len += strlen(q) - 1;
      q += strlen(q) - 1;
      p++;
    }
  }
  e[maxlen-1] = '\0';		/* null terminate, to be sure */
  return e;
}


/*
 * Output the time of day and hostname to the logfile
 */
static void
show_time_host_and_name(int lvl)
{
  static time_t last_t = 0;
  static char *last_ctime = 0;
  time_t t;
#ifdef HAVE_CLOCK_GETTIME
  struct timespec ts;
#endif /* HAVE_CLOCK_GETTIME */
  char nsecs[11] = "";	/* '.' + 9 digits + '\0' */
  char *sev;

#ifdef HAVE_CLOCK_GETTIME
  /*
   * Some systems (AIX 4.3) seem to implement clock_gettime() as stub
   * returning ENOSYS.
   */
  if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
    t = ts.tv_sec;
#ifdef DEBUG
    amuDebug(D_HRTIME)
      sprintf(nsecs, ".%09ld", ts.tv_nsec);
#endif /* DEBUG */
  }
  else
#endif /* HAVE_CLOCK_GETTIME */
    t = clocktime();

  if (t != last_t) {
    last_ctime = ctime(&t);
    last_t = t;
  }

  switch (lvl) {
  case XLOG_FATAL:
    sev = "fatal:";
    break;
  case XLOG_ERROR:
    sev = "error:";
    break;
  case XLOG_USER:
    sev = "user: ";
    break;
  case XLOG_WARNING:
    sev = "warn: ";
    break;
  case XLOG_INFO:
    sev = "info: ";
    break;
  case XLOG_DEBUG:
    sev = "debug:";
    break;
  case XLOG_MAP:
    sev = "map:  ";
    break;
  case XLOG_STATS:
    sev = "stats:";
    break;
  default:
    sev = "hmm:  ";
    break;
  }
  fprintf(logfp, "%15.15s%s %s %s[%ld]/%s ",
	  last_ctime + 4, nsecs, am_get_hostname(),
	  am_get_progname(),
	  (long) am_mypid,
	  sev);
}


#ifdef DEBUG
/*
 * Switch on/off debug options
 */
int
debug_option(char *opt)
{
  return cmdoption(opt, dbg_opt, &debug_flags);
}


void
dplog(const char *fmt, ...)
{
  va_list ap;

  if (!logfp)
    logfp = stderr;		/* initialize before possible first use */

  va_start(ap, fmt);
  real_plog(XLOG_DEBUG, fmt, ap);
  va_end(ap);
}
#endif /* DEBUG */


void
plog(int lvl, const char *fmt, ...)
{
  va_list ap;

  if (!logfp)
    logfp = stderr;		/* initialize before possible first use */

  va_start(ap, fmt);
  real_plog(lvl, fmt, ap);
  va_end(ap);
}


static void
real_plog(int lvl, const char *fmt, va_list vargs)
{
  char msg[1024];
  char efmt[1024];
  char *ptr = msg;
  static char last_msg[1024];
  static int last_count = 0, last_lvl = 0;

  if (!(xlog_level & lvl))
    return;

#ifdef DEBUG_MEM
  checkup_mem();
#endif /* DEBUG_MEM */

#ifdef HAVE_VSNPRINTF
  /*
   * XXX: ptr is 1024 bytes long, but we may write to ptr[strlen(ptr) + 2]
   * (to add an '\n', see code below) so we have to limit the string copy
   * to 1023 (including the '\0').
   */
  vsnprintf(ptr, 1023, expand_error(fmt, efmt, 1024), vargs);
  msg[1022] = '\0';		/* null terminate, to be sure */
#else /* not HAVE_VSNPRINTF */
  /*
   * XXX: ptr is 1024 bytes long.  It is possible to write into it
   * more than 1024 bytes, if efmt is already large, and vargs expand
   * as well.  This is not as safe as using vsnprintf().
   */
  vsprintf(ptr, expand_error(fmt, efmt, 1023), vargs);
  msg[1023] = '\0';		/* null terminate, to be sure */
#endif /* not HAVE_VSNPRINTF */

  ptr += strlen(ptr);
  if (ptr[-1] == '\n')
    *--ptr = '\0';

#ifdef HAVE_SYSLOG
  if (syslogging) {
    switch (lvl) {		/* from mike <mcooper@usc.edu> */
    case XLOG_FATAL:
      lvl = LOG_CRIT;
      break;
    case XLOG_ERROR:
      lvl = LOG_ERR;
      break;
    case XLOG_USER:
      lvl = LOG_WARNING;
      break;
    case XLOG_WARNING:
      lvl = LOG_WARNING;
      break;
    case XLOG_INFO:
      lvl = LOG_INFO;
      break;
    case XLOG_DEBUG:
      lvl = LOG_DEBUG;
      break;
    case XLOG_MAP:
      lvl = LOG_DEBUG;
      break;
    case XLOG_STATS:
      lvl = LOG_INFO;
      break;
    default:
      lvl = LOG_ERR;
      break;
    }
    syslog(lvl, "%s", msg);
    return;
  }
#endif /* HAVE_SYSLOG */

  *ptr++ = '\n';
  *ptr = '\0';

  /*
   * mimic syslog behavior: only write repeated strings if they differ
   */
  switch (last_count) {
  case 0:			/* never printed at all */
    last_count = 1;
    strncpy(last_msg, msg, 1024);
    last_lvl = lvl;
    show_time_host_and_name(lvl); /* mimic syslog header */
    fwrite(msg, ptr - msg, 1, logfp);
    fflush(logfp);
    break;

  case 1:			/* item printed once, if same, don't repeat */
    if (STREQ(last_msg, msg)) {
      last_count++;
    } else {			/* last msg printed once, new one differs */
      /* last_count remains at 1 */
      strncpy(last_msg, msg, 1024);
      last_lvl = lvl;
      show_time_host_and_name(lvl); /* mimic syslog header */
      fwrite(msg, ptr - msg, 1, logfp);
      fflush(logfp);
    }
    break;

  case 100:
    /*
     * Don't allow repetitions longer than 100, so you can see when something
     * cycles like crazy.
     */
    show_time_host_and_name(last_lvl);
    sprintf(last_msg, "last message repeated %d times\n", last_count);
    fwrite(last_msg, strlen(last_msg), 1, logfp);
    fflush(logfp);
    last_count = 0;		/* start from scratch */
    break;

  default:			/* item repeated multiple times */
    if (STREQ(last_msg, msg)) {
      last_count++;
    } else {		/* last msg repeated+skipped, new one differs */
      show_time_host_and_name(last_lvl);
      sprintf(last_msg, "last message repeated %d times\n", last_count);
      fwrite(last_msg, strlen(last_msg), 1, logfp);
      strncpy(last_msg, msg, 1024);
      last_count = 1;
      last_lvl = lvl;
      show_time_host_and_name(lvl); /* mimic syslog header */
      fwrite(msg, ptr - msg, 1, logfp);
      fflush(logfp);
    }
    break;
  }

}


/*
 * Display current debug options
 */
void
show_opts(int ch, struct opt_tab *opts)
{
  int i;
  int s = '{';

  fprintf(stderr, "\t[-%c {no}", ch);
  for (i = 0; opts[i].opt; i++) {
    fprintf(stderr, "%c%s", s, opts[i].opt);
    s = ',';
  }
  fputs("}]\n", stderr);
}


int
cmdoption(char *s, struct opt_tab *optb, int *flags)
{
  char *p = s;
  int errs = 0;

  while (p && *p) {
    int neg;
    char *opt;
    struct opt_tab *dp, *dpn = 0;

    s = p;
    p = strchr(p, ',');
    if (p)
      *p = '\0';

    /* check for "no" prefix to options */
    if (s[0] == 'n' && s[1] == 'o') {
      opt = s + 2;
      neg = 1;
    } else {
      opt = s;
      neg = 0;
    }

    /*
     * Scan the array of debug options to find the
     * corresponding flag value.  If it is found
     * then set (or clear) the flag (depending on
     * whether the option was prefixed with "no").
     */
    for (dp = optb; dp->opt; dp++) {
      if (STREQ(opt, dp->opt))
	break;
      if (opt != s && !dpn && STREQ(s, dp->opt))
	dpn = dp;
    }

    if (dp->opt || dpn) {
      if (!dp->opt) {
	dp = dpn;
	neg = !neg;
      }
      if (neg)
	*flags &= ~dp->flag;
      else
	*flags |= dp->flag;
    } else {
      /*
       * This will log to stderr when parsing the command line
       * since any -l option will not yet have taken effect.
       */
      plog(XLOG_USER, "option \"%s\" not recognized", s);
      errs++;
    }

    /*
     * Put the comma back
     */
    if (p)
      *p++ = ',';
  }

  return errs;
}


/*
 * Switch on/off logging options
 */
int
switch_option(char *opt)
{
  int xl = xlog_level;
  int rc = cmdoption(opt, xlog_opt, &xl);

  if (rc) {
    rc = EINVAL;
  } else {
    /*
     * Keep track of initial log level, and
     * don't allow options to be turned off.
     */
    if (xlog_level_init == ~0)
      xlog_level_init = xl;
    else
      xl |= xlog_level_init;
    xlog_level = xl;
  }
  return rc;
}

#ifdef LOG_DAEMON
/*
 * get syslog facility to use.
 * logfile can be "syslog", "syslog:daemon", "syslog:local7", etc.
 */
static int
get_syslog_facility(const char *logfile)
{
  char *facstr;

  /* parse facility string */
  facstr = strchr(logfile, ':');
  if (!facstr)			/* log file was "syslog" */
    return LOG_DAEMON;
  facstr++;
  if (!facstr || facstr[0] == '\0') { /* log file was "syslog:" */
    plog(XLOG_WARNING, "null syslog facility, using LOG_DAEMON");
    return LOG_DAEMON;
  }

#ifdef LOG_KERN
  if (STREQ(facstr, "kern"))
      return LOG_KERN;
#endif /* not LOG_KERN */
#ifdef LOG_USER
  if (STREQ(facstr, "user"))
      return LOG_USER;
#endif /* not LOG_USER */
#ifdef LOG_MAIL
  if (STREQ(facstr, "mail"))
      return LOG_MAIL;
#endif /* not LOG_MAIL */

  if (STREQ(facstr, "daemon"))
      return LOG_DAEMON;

#ifdef LOG_AUTH
  if (STREQ(facstr, "auth"))
      return LOG_AUTH;
#endif /* not LOG_AUTH */
#ifdef LOG_SYSLOG
  if (STREQ(facstr, "syslog"))
      return LOG_SYSLOG;
#endif /* not LOG_SYSLOG */
#ifdef LOG_LPR
  if (STREQ(facstr, "lpr"))
      return LOG_LPR;
#endif /* not LOG_LPR */
#ifdef LOG_NEWS
  if (STREQ(facstr, "news"))
      return LOG_NEWS;
#endif /* not LOG_NEWS */
#ifdef LOG_UUCP
  if (STREQ(facstr, "uucp"))
      return LOG_UUCP;
#endif /* not LOG_UUCP */
#ifdef LOG_CRON
  if (STREQ(facstr, "cron"))
      return LOG_CRON;
#endif /* not LOG_CRON */
#ifdef LOG_LOCAL0
  if (STREQ(facstr, "local0"))
      return LOG_LOCAL0;
#endif /* not LOG_LOCAL0 */
#ifdef LOG_LOCAL1
  if (STREQ(facstr, "local1"))
      return LOG_LOCAL1;
#endif /* not LOG_LOCAL1 */
#ifdef LOG_LOCAL2
  if (STREQ(facstr, "local2"))
      return LOG_LOCAL2;
#endif /* not LOG_LOCAL2 */
#ifdef LOG_LOCAL3
  if (STREQ(facstr, "local3"))
      return LOG_LOCAL3;
#endif /* not LOG_LOCAL3 */
#ifdef LOG_LOCAL4
  if (STREQ(facstr, "local4"))
      return LOG_LOCAL4;
#endif /* not LOG_LOCAL4 */
#ifdef LOG_LOCAL5
  if (STREQ(facstr, "local5"))
      return LOG_LOCAL5;
#endif /* not LOG_LOCAL5 */
#ifdef LOG_LOCAL6
  if (STREQ(facstr, "local6"))
      return LOG_LOCAL6;
#endif /* not LOG_LOCAL6 */
#ifdef LOG_LOCAL7
  if (STREQ(facstr, "local7"))
      return LOG_LOCAL7;
#endif /* not LOG_LOCAL7 */

  /* didn't match anything else */
  plog(XLOG_WARNING, "unknown syslog facility \"%s\", using LOG_DAEMON", facstr);
  return LOG_DAEMON;
}
#endif /* not LOG_DAEMON */


/*
 * Change current logfile
 */
int
switch_to_logfile(char *logfile, int old_umask)
{
  FILE *new_logfp = stderr;

  if (logfile) {
#ifdef HAVE_SYSLOG
    syslogging = 0;
#endif /* HAVE_SYSLOG */

    if (STREQ(logfile, "/dev/stderr"))
      new_logfp = stderr;
    else if (NSTREQ(logfile, "syslog", strlen("syslog"))) {

#ifdef HAVE_SYSLOG
      syslogging = 1;
      new_logfp = stderr;
      openlog(am_get_progname(),
	      LOG_PID
# ifdef LOG_CONS
	      | LOG_CONS
# endif /* LOG_CONS */
# ifdef LOG_NOWAIT
	      | LOG_NOWAIT
# endif /* LOG_NOWAIT */
# ifdef LOG_DAEMON
	      , get_syslog_facility(logfile)
# endif /* LOG_DAEMON */
	      );
#else /* not HAVE_SYSLOG */
      plog(XLOG_WARNING, "syslog option not supported, logging unchanged");
#endif /* not HAVE_SYSLOG */

    } else {
      (void) umask(old_umask);
      new_logfp = fopen(logfile, "a");
      umask(0);
    }
  }

  /*
   * If we couldn't open a new file, then continue using the old.
   */
  if (!new_logfp && logfile) {
    plog(XLOG_USER, "%s: Can't open logfile: %m", logfile);
    return 1;
  }

  /*
   * Close the previous file
   */
  if (logfp && logfp != stderr)
    (void) fclose(logfp);
  logfp = new_logfp;

  if (logfile)
    plog(XLOG_INFO, "switched to logfile \"%s\"", logfile);
  else
    plog(XLOG_INFO, "no logfile defined; using stderr");

  return 0;
}


void
unregister_amq(void)
{
#ifdef DEBUG
  amuDebug(D_AMQ)
#endif /* DEBUG */
    /* find which instance of amd to unregister */
    pmap_unset(get_amd_program_number(), AMQ_VERSION);
}


void
going_down(int rc)
{
  if (foreground) {
    if (amd_state != Start) {
      if (amd_state != Done)
	return;
      unregister_amq();
    }
  }
  if (foreground) {
    plog(XLOG_INFO, "Finishing with status %d", rc);
  } else {
#ifdef DEBUG
    dlog("background process exiting with status %d", rc);
#endif /* DEBUG */
  }

  exit(rc);
}


/* return the rpc program number under which amd was used */
int
get_amd_program_number(void)
{
  return amd_program_number;
}


/* set the rpc program number used for amd */
void
set_amd_program_number(int program)
{
  amd_program_number = program;
}


/*
 * Release the controlling tty of the process pid.
 *
 * Algorithm: try these in order, if available, until one of them
 * succeeds: setsid(), ioctl(fd, TIOCNOTTY, 0).
 * Do not use setpgid(): on some OSs it may release the controlling tty,
 * even if the man page does not mention it, but on other OSs it does not.
 * Also avoid setpgrp(): it works on some systems, and on others it is
 * identical to setpgid().
 */
void
amu_release_controlling_tty(void)
{
#ifdef TIOCNOTTY
  int fd;
#endif /* TIOCNOTTY */
  int tempfd;

  /*
   * In daemon mode, leaving open file descriptors to terminals or pipes
   * can be a really bad idea.
   * Case in point: the redhat startup script calls us through their 'initlog'
   * program, which exits as soon as the original amd process exits. If,
   * at some point, a misbehaved library function decides to print something
   * to the screen, we get a SIGPIPE and die.
   * And guess what: NIS glibc functions will attempt to print to stderr
   * "YPBINDPROC_DOMAIN: Domain not bound" if ypbind is running but can't find
   * a ypserver.
   *
   * So we close all of our "terminal" filedescriptors, i.e. 0, 1 and 2, then
   * reopen them as /dev/null.
   *
   * XXX We should also probably set the SIGPIPE handler to SIG_IGN.
   */
  tempfd = open("/dev/null", O_RDWR);
  fflush(stdin);  close(0); dup2(tempfd, 0);
  fflush(stdout); close(1); dup2(tempfd, 1);
  fflush(stderr); close(2); dup2(tempfd, 2);
  close(tempfd);

#ifdef HAVE_SETSID
  /* XXX: one day maybe use vhangup(2) */
  if (setsid() < 0) {
    plog(XLOG_WARNING, "Could not release controlling tty using setsid(): %m");
  } else {
    plog(XLOG_INFO, "released controlling tty using setsid()");
    return;
  }
#endif /* HAVE_SETSID */

#ifdef TIOCNOTTY
  fd = open("/dev/tty", O_RDWR);
  if (fd < 0) {
    /* not an error if already no controlling tty */
    if (errno != ENXIO)
      plog(XLOG_WARNING, "Could not open controlling tty: %m");
  } else {
    if (ioctl(fd, TIOCNOTTY, 0) < 0 && errno != ENOTTY)
      plog(XLOG_WARNING, "Could not disassociate tty (TIOCNOTTY): %m");
    else
      plog(XLOG_INFO, "released controlling tty using ioctl(TIOCNOTTY)");
    close(fd);
  }
  return;
#endif /* not TIOCNOTTY */

  plog(XLOG_ERROR, "unable to release controlling tty");
}
