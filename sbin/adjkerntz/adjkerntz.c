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
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND
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
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "pathnames.h"

char storage[] = _PATH_OFFSET;

int main(argc, argv)
	int argc;
	char **argv;
{
	struct tm local, utc;
	struct timeval tv, *stv;
	struct timezone tz, *stz;
	/* Avoid time_t here, can be unsigned long */
	long offset, oldoffset, utcsec, localsec, diff;
	time_t final_sec;
	int ch, init = -1, verbose = 0;
	FILE *f;

	while ((ch = getopt(argc, argv, "aiv")) != EOF)
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
		case 'v':               /* verbose */
			verbose = 1;
			break;
		default:
		usage:
			fprintf(stderr, "Usage:\n\
\tadjkerntz -i [-v]\t(initial call from /etc/rc)\n\
\tadjkerntz -a [-v]\t(adjustment call from crontab)\n");
			return 2;
		}
	if (init == -1)
		goto usage;

	if (access(_PATH_CLOCK, F_OK))
		return 0;

	/* Restore saved offset */

	if (!init) {
		if ((f = fopen(storage, "r")) == NULL) {
			perror(storage);
			return 1;
		}
		if (fscanf(f, "%ld", &oldoffset) != 1) {
			fprintf(stderr, "Incorrect offset in %s\n", storage);
			return 1;
		}
		(void) fclose(f);
	}
	else
		oldoffset = 0;

/****** Critical section, do all things as fast as possible ******/

	/* get local CMOS clock and possible kernel offset */
	if (gettimeofday(&tv, &tz)) {
		perror("gettimeofday");
		return 1;
	}

	/* get the actual local timezone difference */
	local = *localtime(&tv.tv_sec);
	utc = *gmtime(&tv.tv_sec);
	utc.tm_isdst = local.tm_isdst; /* Use current timezone for mktime(), */
				       /* because it assumed local time */

	/* calculate local CMOS diff from GMT */

	utcsec = mktime(&utc);
	localsec = mktime(&local);
	if (utcsec == -1 || localsec == -1) {
		fprintf(stderr, "Wrong initial hour to call\n");
		return 1;
	}
	offset = utcsec - localsec;

	/* correct the kerneltime for this diffs */
	/* subtract kernel offset, if present, old offset too */

	diff = offset - tz.tz_minuteswest * 60 - oldoffset;

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
			fprintf(stderr, "Wrong final hour to call\n");
			return 1;
		}
		offset = utcsec - localsec;

		/* correct the kerneltime for this diffs */
		/* subtract kernel offset, if present, old offset too */

		diff = offset - tz.tz_minuteswest * 60 - oldoffset;

		if (diff != 0) {
			tv.tv_sec += diff;
			tv.tv_usec = 0;       /* we are restarting here... */
			stv = &tv;
		}
		else
			stv = NULL;
	}
	else
		stv = NULL;

	if (tz.tz_dsttime != 0 || tz.tz_minuteswest != 0) {
		tz.tz_dsttime = tz.tz_minuteswest = 0;  /* zone info is garbage */
		stz = &tz;
	}
	else
		stz = NULL;

	if (stz != NULL || stv != NULL) {
		if (settimeofday(stv, stz)) {
			perror("settimeofday");
			return 1;
		}
	}

/****** End of critical section ******/

	if (verbose)
		printf("Calculated zone offset diffs: %ld seconds\n", diff);

	if (offset != oldoffset) {
		(void) umask(022);
		/* Save offset for next calls from crontab */
		if ((f = fopen(storage, "w")) == NULL) {
			perror(storage);
			return 1;
		}
		fprintf(f, "%ld\n", offset);
		if (fclose(f)) {
			perror(storage);
			return 1;
		}
	}

	return 0;
}

