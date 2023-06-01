/* Copyright 1993,1994 by Paul Vixie
 * All rights reserved
 */

/*
 * Copyright (c) 1997 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * $Id: pathnames.h,v 1.4 1998/08/14 00:32:41 vixie Exp $
 */

#ifndef CRONDIR
			/* CRONDIR is where crond(8) and crontab(1) both chdir
			 * to; SPOOL_DIR, ALLOW_FILE, DENY_FILE, and LOG_FILE
			 * are all relative to this directory.
			 */
#define CRONDIR		"/var/cron"
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
			 * both are defined, we log both ways.  Note that if
			 * LOG_CRON is defined by <syslog.h>, LOG_FILE will not
			 * be used.
			 */
#define	ALLOW_FILE	"allow"
#define DENY_FILE	"deny"
#define LOG_FILE	"log"

			/* where should the daemon stick its PID?
			 */
#define PIDDIR	_PATH_VARRUN
#define PIDFILE		"cron.pid"

			/* 4.3BSD-style crontab */
#define SYSCRONTAB	"/etc/crontab"
#define SYSCRONTABS	"/etc/cron.d"
#define LOCALSYSCRONTABS	_PATH_LOCALBASE "/etc/cron.d"

			/* what editor to use if no EDITOR or VISUAL
			 * environment variable specified.
			 */
#define EDITOR _PATH_VI
