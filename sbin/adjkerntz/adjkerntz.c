/*
 * Copyright (C) 1993 by Andrew A. Chernov, Moscow, Russia.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
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
 */

#ifndef lint
char copyright[] =
"@(#)Copyright (C) 1993 by Andrew A. Chernov, Moscow, Russia.\n\
 All rights reserved.\n";
#endif /* not lint */

/*
 * Andrew A. Chernov   <ache@astral.msk.su>    Dec 20 1993
 *
 * Fix kernel time value if machine run wall CMOS clock
 * (and /etc/wall_cmos_clock file present)
 * using zoneinfo rules or direct TZ environment variable set.
 * Use Joerg Wunsch idea for seconds accurate offset calculation
 * with Garrett Wollman and Bruce Evans fixes.
 *
 */
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>
#include <machine/cpu.h>
#include <sys/sysctl.h>

#include "pathnames.h"

#define REPORT_PERIOD (30*60)

void fake() {}

int main(argc, argv)
	int argc;
	char **argv;
{
	struct tm local, utc;
	struct timeval tv, *stv;
	struct timezone tz, *stz;
	int kern_offset;
	size_t len;
	int mib[2];
	/* Avoid time_t here, can be unsigned long or worse */
	long offset, utcsec, localsec, diff;
	time_t initial_sec, final_sec;
	int ch, init = -1;
	int disrtcset, need_restore = 0;
	sigset_t mask, emask;

	while ((ch = getopt(argc, argv, "ai")) != EOF)
		switch((char)ch) {
		case 'i':               /* initial call, save offset */
			if (init != -1)
				goto usage;
			init = 1;
			break;
		case 'a':               /* adjustment call, use saved offset */
			if (init != -1)
				goto usage;
			init = 0;
			break;
		default:
		usage:
			fprintf(stderr, "Usage:\n\
\tadjkerntz -i\t(initial call from /etc/rc)\n\
\tadjkerntz -a\t(adjustment call from crontab)\n");
  			return 2;
		}
	if (init == -1)
		goto usage;
  
	if (access(_PATH_CLOCK, F_OK))
		return 0;

	sigemptyset(&mask);
	sigemptyset(&emask);
	sigaddset(&mask, SIGTERM);

	openlog("adjkerntz", LOG_PID|LOG_PERROR, LOG_DAEMON);

	(void) signal(SIGHUP, SIG_IGN);

	if (init && daemon(0, 1)) {
		syslog(LOG_ERR, "daemon: %m");
		return 1;
	}

again:

	(void) sigprocmask(SIG_BLOCK, &mask, NULL);
	(void) signal(SIGTERM, fake);

/****** Critical section, do all things as fast as possible ******/

	/* get local CMOS clock and possible kernel offset */
	if (gettimeofday(&tv, &tz)) {
		syslog(LOG_ERR, "gettimeofday: %m");
		return 1;
	}

	/* get the actual local timezone difference */
	initial_sec = tv.tv_sec;
	local = *localtime(&initial_sec);
	utc = *gmtime(&initial_sec);
	utc.tm_isdst = local.tm_isdst; /* Use current timezone for mktime(), */
				       /* because it assumed local time */

	/* calculate local CMOS diff from GMT */

	utcsec = mktime(&utc);
	localsec = mktime(&local);
	if (utcsec == -1 || localsec == -1) {
		/*
		 * XXX user can only control local time, and it is
		 * unacceptable to fail here for init.  2:30 am in the
		 * middle of the nonexistent hour means 3:30 am.
		 */
		syslog(LOG_WARNING,
		"Nonexistent local time -- will retry after %d secs", REPORT_PERIOD);
		(void) signal(SIGTERM, SIG_DFL);
		(void) sigprocmask(SIG_UNBLOCK, &mask, NULL);
		(void) sleep(REPORT_PERIOD);
		goto again;
	}
	offset = utcsec - localsec;

	mib[0] = CTL_MACHDEP;
	mib[1] = CPU_ADJKERNTZ;
	len = sizeof(kern_offset);
	if (sysctl(mib, 2, &kern_offset, &len, NULL, 0) == -1) {
		syslog(LOG_ERR, "sysctl(get_offset): %m");
		return 1;
	}

	stv = NULL;
	stz = NULL;

	/* correct the kerneltime for this diffs */
	/* subtract kernel offset, if present, old offset too */

	diff = offset - tz.tz_minuteswest * 60 - kern_offset;

	if (diff != 0) {

		/* Yet one step for final time */

		final_sec = tv.tv_sec + diff;

		/* get the actual local timezone difference */
		local = *localtime(&final_sec);
		utc = *gmtime(&final_sec);
		utc.tm_isdst = local.tm_isdst; /* Use current timezone for mktime(), */
					       /* because it assumed local time */

		utcsec = mktime(&utc);
		localsec = mktime(&local);
		if (utcsec == -1 || localsec == -1) {
			/*
			 * XXX as above.  The user has even less control,
			 * but perhaps we never get here.
			 */
			syslog(LOG_WARNING,
		"Nonexistent (final) local time -- will retry after %d secs", REPORT_PERIOD);
			(void) signal(SIGTERM, SIG_DFL);
			(void) sigprocmask(SIG_UNBLOCK, &mask, NULL);
			(void) sleep(REPORT_PERIOD);
			goto again;
		}
		offset = utcsec - localsec;

		/* correct the kerneltime for this diffs */
		/* subtract kernel offset, if present, old offset too */

		diff = offset - tz.tz_minuteswest * 60 - kern_offset;

		if (diff != 0) {
			tv.tv_sec += diff;
			tv.tv_usec = 0;       /* we are restarting here... */
			stv = &tv;
		}
	}

	if (tz.tz_dsttime != 0 || tz.tz_minuteswest != 0) {
		tz.tz_dsttime = tz.tz_minuteswest = 0;  /* zone info is garbage */
		stz = &tz;
	}

	/* if init and something will be changed, don't touch RTC at all */
	if (init && (stv != NULL || kern_offset != offset)) {
		mib[0] = CTL_MACHDEP;
		mib[1] = CPU_DISRTCSET;
		len = sizeof(disrtcset);
		if (sysctl(mib, 2, &disrtcset, &len, NULL, 0) == -1) {
			syslog(LOG_ERR, "sysctl(get_disrtcset): %m");
			return 1;
		}
		if (disrtcset == 0) {
			disrtcset = 1;
			need_restore = 1;
			if (sysctl(mib, 2, NULL, NULL, &disrtcset, len) == -1) {
				syslog(LOG_ERR, "sysctl(set_disrtcset): %m");
				return 1;
			}
		}
	}

	if ((   (init && (stv != NULL || stz != NULL))
	     || (stz != NULL && stv == NULL)
	    )
	    && settimeofday(stv, stz)
	   ) {
		syslog(LOG_ERR, "settimeofday: %m");
		return 1;
	}

	/* init: don't write RTC, !init: write RTC */
	if (kern_offset != offset) {
		kern_offset = offset;
		mib[0] = CTL_MACHDEP;
		mib[1] = CPU_ADJKERNTZ;
		len = sizeof(kern_offset);
		if (sysctl(mib, 2, NULL, NULL, &kern_offset, len) == -1) {
			syslog(LOG_ERR, "sysctl(update_offset): %m");
			return 1;
		}
	}

	if (need_restore) {
		need_restore = 0;
		disrtcset = 0;
		len = sizeof(disrtcset);
		if (sysctl(mib, 2, NULL, NULL, &disrtcset, len) == -1) {
			syslog(LOG_ERR, "sysctl(restore_disrtcset): %m");
			return 1;
		}
	}

/****** End of critical section ******/

	if (init) {
		init = 0;
		/* wait for signals and acts like -a */
		(void) sigsuspend(&emask);
		goto again;
	}

	return 0;
}
