/* config.h - configurables for Vixie Cron
 *
 * $Header: /a/cvs/386BSD/src/libexec/crond/config.h,v 1.1.1.1 1993/06/12 14:55:04 rgrimes Exp $
 */

/* Copyright 1988,1990 by Paul Vixie
 * All rights reserved
 *
 * Distribute freely, except: don't remove my name from the source or
 * documentation (don't take credit for my work), mark your changes (don't
 * get me blamed for your possible bugs), don't alter or remove this
 * notice.  May be sold if buildable source is provided to buyer.  No
 * warrantee of any kind, express or implied, is included with this
 * software; use at your own risk, responsibility for damages (if any) to
 * anyone resulting from the use of this software rests entirely with the
 * user.
 *
 * Send bug reports, bug fixes, enhancements, requests, flames, etc., and
 * I'll try to keep a version up to date.  I can be reached as follows:
 * Paul Vixie, 329 Noe Street, San Francisco, CA, 94114, (415) 864-7013,
 * paul@vixie.sf.ca.us || {hoptoad,pacbell,decwrl,crash}!vixie!paul
 */

#ifndef	_CONFIG_FLAG
#define	_CONFIG_FLAG

/*
 * these are site-dependent
 */
			/*
			 * choose one of these MAILCMD commands.  I use
			 * /bin/mail for speed; it makes biff bark but doesn't
			 * do aliasing.  /usr/lib/sendmail does aliasing but is
			 * a hog for short messages.  aliasing is not needed
			 * if you make use of the MAILTO= feature in crontabs.
			 * (hint: MAILTO= was added for this reason).
			 */

# define MAILCMD "/usr/sbin/sendmail -F\"Cron Daemon\" -odi -oem -or0s %s" /*-*/
			/* -Fx	 = set full-name of sender
			 * -odi	 = Option Deliverymode Interactive
			 * -oem	 = Option Errors Mailedtosender
			 * -or0s = Option Readtimeout -- don't time out
			 */

/* # define MAILCMD "/bin/mail -d  %s"		/*-*/
			/* -d = undocumented but common flag: deliver locally?
			 */

#ifndef CRONDIR
			/* CRONDIR is where crond(8) and crontab(1) both chdir
			 * to; SPOOL_DIR, ALLOW_FILE, DENY_FILE, and LOG_FILE
			 * are all relative to this directory.
			 *
			 * this can and should be set in the Makefile.
			 */
# define CRONDIR	"/var/cron"
#endif

			/* SPOOLDIR is where the crontabs live.
			 * This directory will have its modtime updated
			 * whenever crontab(1) changes a crontab; this is
			 * the signal for crond(8) to look at each individual
			 * crontab file and reload those whose modtimes are
			 * newer than they were last time around (or which
			 * didn't exist last time around...)
			 */
#define SPOOL_DIR	"tabs"

			/* undefining these turns off their features.  note
			 * that ALLOW_FILE and DENY_FILE must both be defined
			 * in order to enable the allow/deny code.  If neither
			 * LOG_FILE or SYSLOG is defined, we don't log.  If
			 * both are defined, we log both ways.
			 */
#define	ALLOW_FILE	"allow"		/*-*/
#define DENY_FILE	"deny"		/*-*/
#define LOG_FILE	"log"		/*-*/

			/* if ALLOW_FILE and DENY_FILE are not defined or are
			 * defined but neither exists, should crontab(1) be
			 * usable only by root?
			 */
/*#define ALLOW_ONLY_ROOT			/*-*/

			/* if you want to use syslog(3) instead of appending
			 * to CRONDIR/LOG_FILE (/var/cron/log, e.g.), define
			 * SYSLOG here.  Note that quite a bit of logging
			 * info is written, and that you probably don't want
			 * to use this on 4.2bsd since everything goes in
			 * /usr/spool/mqueue/syslog.  On 4.[34]bsd you can
			 * tell /etc/syslog.conf to send cron's logging to
			 * a separate file.
			 */
/*#define SYSLOG	 			/*-*/

			/* this is the name of the environment variable
			 * that contains the user name.  it isn't read by
			 * cron, but it is SET by crond in the environments
			 * it creates for subprocesses.  on BSD, it will
			 * always be USER; on SysV it could be LOGNAME or
			 * something else.
			 */
#if defined(BSD)
# define USERENV	"USER"
#endif
#if defined(ATT)
# define USERENV	"LOGNAME"
#endif

			/* where should the daemon stick its PID?
			 */
#define PIDFILE		"/var/run/crond.pid"

#endif /*CONFIG_FLAG*/
