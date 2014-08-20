/*
 * Copyright 2007 Rob Kendrick <rjek@netsurf-browser.org>
 * Copyright 2004-2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 John Tytgat <joty@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>

#include "utils/log.h"

bool verbose_log = false;

nserror nslog_init(nslog_ensure_t *ensure, int *pargc, char **argv)
{
	nserror ret = NSERROR_OK;

	if (((*pargc) > 1) &&
	    (argv[1][0] == '-') &&
	    (argv[1][1] == 'v') &&
	    (argv[1][2] == 0)) {
		int argcmv;
		for (argcmv = 2; argcmv < (*pargc); argcmv++) {
			argv[argcmv - 1] = argv[argcmv];
		}
		(*pargc)--;

		/* ensure we actually show logging */
		verbose_log = true;
	}

	/* ensure output file handle is correctly configured */
	if ((verbose_log == true) &&
	    (ensure != NULL) &&
	    (ensure(stderr) == false)) {
		/* failed to ensure output configuration */
		ret = NSERROR_INIT_FAILED;
		verbose_log = false;
	}

	return ret;
}

#ifndef NDEBUG

/* Subtract the `struct timeval' values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0.  
*/

static int
timeval_subtract(struct timeval *result, struct timeval *x, struct timeval *y)
{
	/* Perform the carry for the later subtraction by updating y. */
	if (x->tv_usec < y->tv_usec) {
		int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
		y->tv_usec -= 1000000 * nsec;
		y->tv_sec += nsec;
	}
	if (x->tv_usec - y->tv_usec > 1000000) {
		int nsec = (x->tv_usec - y->tv_usec) / 1000000;
		y->tv_usec += 1000000 * nsec;
		y->tv_sec -= nsec;
	}

	/* Compute the time remaining to wait.
	   tv_usec is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;

	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}

const char *nslog_gettime(void)
{
	static struct timeval start_tv;
	static char buff[32];

	struct timeval tv;
        struct timeval now_tv;

	if (!timerisset(&start_tv)) {
		gettimeofday(&start_tv, NULL);		
	}
        gettimeofday(&now_tv, NULL);

	timeval_subtract(&tv, &now_tv, &start_tv);

        snprintf(buff, sizeof(buff),"(%ld.%06ld)", 
			(long)tv.tv_sec, (long)tv.tv_usec);

        return buff;
}

void nslog_log(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	vfprintf(stderr, format, ap);

	va_end(ap);
}

#endif

