/*
 * Copyright (c) 1999 Theo de Raadt.  All rights reserved.
 * Copyright (c) 1999 Aaron Campbell.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Parts from:
 *
 * Copyright (c) 1983, 1990, 1992, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
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
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 */

#include "includes.h"
RCSID("$OpenBSD: progressmeter.c,v 1.3 2003/03/17 10:38:38 markus Exp $");

#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif

#include "atomicio.h"
#include "progressmeter.h"

/* Number of seconds before xfer considered "stalled". */
#define STALLTIME	5
/* alarm() interval for updating progress meter. */
#define PROGRESSTIME	1

/* Signal handler used for updating the progress meter. */
static void update_progress_meter(int);

/* Returns non-zero if we are the foreground process. */
static int foregroundproc(void);

/* Returns width of the terminal (for progress meter calculations). */
static int get_tty_width(void);

/* Visual statistics about files as they are transferred. */
static void draw_progress_meter(void);

/* Time a transfer started. */
static struct timeval start;

/* Number of bytes of current file transferred so far. */
static volatile off_t *statbytes;

/* Total size of current file. */
static off_t totalbytes;

/* Name of current file being transferred. */
static char *curfile;

/* Time of last update. */
static struct timeval lastupdate;

/* Size at the time of the last update. */
static off_t lastsize;

void
start_progress_meter(char *file, off_t filesize, off_t *counter)
{
	if ((curfile = basename(file)) == NULL)
		curfile = file;

	totalbytes = filesize;
	statbytes = counter;
	(void) gettimeofday(&start, (struct timezone *) 0);
	lastupdate = start;
	lastsize = 0;

	draw_progress_meter();
	signal(SIGALRM, update_progress_meter);
	alarm(PROGRESSTIME);
}

void
stop_progress_meter()
{
	alarm(0);
	draw_progress_meter();
	if (foregroundproc() != 0)
		atomicio(write, fileno(stdout), "\n", 1);
}

static void
update_progress_meter(int ignore)
{
	int save_errno = errno;

	draw_progress_meter();
	signal(SIGALRM, update_progress_meter);
	alarm(PROGRESSTIME);
	errno = save_errno;
}

static int
foregroundproc(void)
{
	static pid_t pgrp = -1;
	int ctty_pgrp;

	if (pgrp == -1)
		pgrp = getpgrp();

#ifdef HAVE_TCGETPGRP
        return ((ctty_pgrp = tcgetpgrp(STDOUT_FILENO)) != -1 &&
	                ctty_pgrp == pgrp);
#else
	return ((ioctl(STDOUT_FILENO, TIOCGPGRP, &ctty_pgrp) != -1 &&
		 ctty_pgrp == pgrp));
#endif
}

static void
draw_progress_meter()
{
	static const char spaces[] = "                          "
	    "                                                   "
	    "                                                   "
	    "                                                   "
	    "                                                   "
	    "                                                   ";
	static const char prefixes[] = " KMGTP";
	struct timeval now, td, wait;
	off_t cursize, abbrevsize, bytespersec;
	double elapsed;
	int ratio, remaining, i, ai, bi, nspaces;
	char buf[512];

	if (foregroundproc() == 0)
		return;

	(void) gettimeofday(&now, (struct timezone *) 0);
	cursize = *statbytes;
	if (totalbytes != 0) {
		ratio = 100.0 * cursize / totalbytes;
		ratio = MAX(ratio, 0);
		ratio = MIN(ratio, 100);
	} else
		ratio = 100;

	abbrevsize = cursize;
	for (ai = 0; abbrevsize >= 10000 && ai < sizeof(prefixes); ai++)
		abbrevsize >>= 10;

	timersub(&now, &lastupdate, &wait);
	if (cursize > lastsize) {
		lastupdate = now;
		lastsize = cursize;
		wait.tv_sec = 0;
	}
	timersub(&now, &start, &td);
	elapsed = td.tv_sec + (td.tv_usec / 1000000.0);

	bytespersec = 0;
	if (cursize > 0) {
		bytespersec = cursize;
		if (elapsed > 0.0)
			bytespersec /= elapsed;
	}
	for (bi = 1; bytespersec >= 1024000 && bi < sizeof(prefixes); bi++)
		bytespersec >>= 10;

    	nspaces = MIN(get_tty_width() - 79, sizeof(spaces) - 1);

#ifdef HAVE_LONG_LONG_INT
	snprintf(buf, sizeof(buf),
	    "\r%-45.45s%.*s%3d%% %4lld%c%c %3lld.%01d%cB/s",
	    curfile,
	    nspaces,
	    spaces,
	    ratio,
	    (long long)abbrevsize,
	    prefixes[ai],
	    ai == 0 ? ' ' : 'B',
	    (long long)(bytespersec / 1024),
	    (int)((bytespersec % 1024) * 10 / 1024),
	    prefixes[bi]
	);
#else
		/* XXX: Handle integer overflow? */
	snprintf(buf, sizeof(buf),
	    "\r%-45.45s%.*s%3d%% %4lu%c%c %3lu.%01d%cB/s",
	    curfile,
	    nspaces,
	    spaces,
	    ratio,
	    (u_long)abbrevsize,
	    prefixes[ai],
	    ai == 0 ? ' ' : 'B',
	    (u_long)(bytespersec / 1024),
	    (int)((bytespersec % 1024) * 10 / 1024),
	    prefixes[bi]
	);
#endif

	if (cursize <= 0 || elapsed <= 0.0 || cursize > totalbytes) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    "   --:-- ETA");
	} else if (wait.tv_sec >= STALLTIME) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    " - stalled -");
	} else {
		if (cursize != totalbytes)
			remaining = (int)(totalbytes / (cursize / elapsed) -
			    elapsed);
		else
			remaining = elapsed;

		i = remaining / 3600;
		if (i)
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			    "%2d:", i);
		else
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			    "   ");
		i = remaining % 3600;
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    "%02d:%02d%s", i / 60, i % 60,
		    (cursize != totalbytes) ? " ETA" : "    ");
	}
	atomicio(write, fileno(stdout), buf, strlen(buf));
}

static int
get_tty_width(void)
{
	struct winsize winsize;

	if (ioctl(fileno(stdout), TIOCGWINSZ, &winsize) != -1)
		return (winsize.ws_col ? winsize.ws_col : 80);
	else
		return (80);
}
