/* $OpenBSD: progressmeter.c,v 1.53 2023/04/12 14:22:04 jsg Exp $ */
/*
 * Copyright (c) 2003 Nils Nordman.  All rights reserved.
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

#include "includes.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "progressmeter.h"
#include "atomicio.h"
#include "misc.h"
#include "utf8.h"

#define DEFAULT_WINSIZE 80
#define MAX_WINSIZE 512
#define PADDING 1		/* padding between the progress indicators */
#define UPDATE_INTERVAL 1	/* update the progress meter every second */
#define STALL_TIME 5		/* we're stalled after this many seconds */

/* determines whether we can output to the terminal */
static int can_output(void);

/* window resizing */
static void sig_winch(int);
static void setscreensize(void);

/* signal handler for updating the progress meter */
static void sig_alarm(int);

static double start;		/* start progress */
static double last_update;	/* last progress update */
static const char *file;	/* name of the file being transferred */
static off_t start_pos;		/* initial position of transfer */
static off_t end_pos;		/* ending position of transfer */
static off_t cur_pos;		/* transfer position as of last refresh */
static volatile off_t *counter;	/* progress counter */
static long stalled;		/* how long we have been stalled */
static int bytes_per_second;	/* current speed in bytes per second */
static int win_size;		/* terminal window size */
static volatile sig_atomic_t win_resized; /* for window resizing */
static volatile sig_atomic_t alarm_fired;

/* units for format_size */
static const char unit[] = " KMGT";

static int
can_output(void)
{
	return (getpgrp() == tcgetpgrp(STDOUT_FILENO));
}

/* size needed to format integer type v, using (nbits(v) * log2(10) / 10) */
#define STRING_SIZE(v) (((sizeof(v) * 8 * 4) / 10) + 1)

static const char *
format_rate(off_t bytes)
{
	int i;
	static char buf[STRING_SIZE(bytes) * 2 + 16];

	bytes *= 100;
	for (i = 0; bytes >= 100*1000 && unit[i] != 'T'; i++)
		bytes = (bytes + 512) / 1024;
	if (i == 0) {
		i++;
		bytes = (bytes + 512) / 1024;
	}
	snprintf(buf, sizeof(buf), "%3lld.%1lld%c%s",
	    (long long) (bytes + 5) / 100,
	    (long long) (bytes + 5) / 10 % 10,
	    unit[i],
	    i ? "B" : " ");
	return buf;
}

static const char *
format_size(off_t bytes)
{
	int i;
	static char buf[STRING_SIZE(bytes) + 16];

	for (i = 0; bytes >= 10000 && unit[i] != 'T'; i++)
		bytes = (bytes + 512) / 1024;
	snprintf(buf, sizeof(buf), "%4lld%c%s",
	    (long long) bytes,
	    unit[i],
	    i ? "B" : " ");
	return buf;
}

void
refresh_progress_meter(int force_update)
{
	char *buf = NULL, *obuf = NULL;
	off_t transferred;
	double elapsed, now;
	int percent;
	off_t bytes_left;
	int cur_speed;
	int hours, minutes, seconds;
	int file_len, cols;

	if ((!force_update && !alarm_fired && !win_resized) || !can_output())
		return;
	alarm_fired = 0;

	if (win_resized) {
		setscreensize();
		win_resized = 0;
	}

	transferred = *counter - (cur_pos ? cur_pos : start_pos);
	cur_pos = *counter;
	now = monotime_double();
	bytes_left = end_pos - cur_pos;

	if (bytes_left > 0)
		elapsed = now - last_update;
	else {
		elapsed = now - start;
		/* Calculate true total speed when done */
		transferred = end_pos - start_pos;
		bytes_per_second = 0;
	}

	/* calculate speed */
	if (elapsed != 0)
		cur_speed = (transferred / elapsed);
	else
		cur_speed = transferred;

#define AGE_FACTOR 0.9
	if (bytes_per_second != 0) {
		bytes_per_second = (bytes_per_second * AGE_FACTOR) +
		    (cur_speed * (1.0 - AGE_FACTOR));
	} else
		bytes_per_second = cur_speed;

	last_update = now;

	/* Don't bother if we can't even display the completion percentage */
	if (win_size < 4)
		return;

	/* filename */
	file_len = cols = win_size - 36;
	if (file_len > 0) {
		asmprintf(&buf, INT_MAX, &cols, "%-*s", file_len, file);
		/* If we used fewer columns than expected then pad */
		if (cols < file_len)
			xextendf(&buf, NULL, "%*s", file_len - cols, "");
	}
	/* percent of transfer done */
	if (end_pos == 0 || cur_pos == end_pos)
		percent = 100;
	else
		percent = ((float)cur_pos / end_pos) * 100;

	/* percent / amount transferred / bandwidth usage */
	xextendf(&buf, NULL, " %3d%% %s %s/s ", percent, format_size(cur_pos),
	    format_rate((off_t)bytes_per_second));

	/* ETA */
	if (!transferred)
		stalled += elapsed;
	else
		stalled = 0;

	if (stalled >= STALL_TIME)
		xextendf(&buf, NULL, "- stalled -");
	else if (bytes_per_second == 0 && bytes_left)
		xextendf(&buf, NULL, "  --:-- ETA");
	else {
		if (bytes_left > 0)
			seconds = bytes_left / bytes_per_second;
		else
			seconds = elapsed;

		hours = seconds / 3600;
		seconds -= hours * 3600;
		minutes = seconds / 60;
		seconds -= minutes * 60;

		if (hours != 0) {
			xextendf(&buf, NULL, "%d:%02d:%02d",
			    hours, minutes, seconds);
		} else
			xextendf(&buf, NULL, "  %02d:%02d", minutes, seconds);

		if (bytes_left > 0)
			xextendf(&buf, NULL, " ETA");
		else
			xextendf(&buf, NULL, "    ");
	}

	/* Finally, truncate string at window width */
	cols = win_size - 1;
	asmprintf(&obuf, INT_MAX, &cols, " %s", buf);
	if (obuf != NULL) {
		*obuf = '\r'; /* must insert as asmprintf() would escape it */
		atomicio(vwrite, STDOUT_FILENO, obuf, strlen(obuf));
	}
	free(buf);
	free(obuf);
}

static void
sig_alarm(int ignore)
{
	alarm_fired = 1;
	alarm(UPDATE_INTERVAL);
}

void
start_progress_meter(const char *f, off_t filesize, off_t *ctr)
{
	start = last_update = monotime_double();
	file = f;
	start_pos = *ctr;
	end_pos = filesize;
	cur_pos = 0;
	counter = ctr;
	stalled = 0;
	bytes_per_second = 0;

	setscreensize();
	refresh_progress_meter(1);

	ssh_signal(SIGALRM, sig_alarm);
	ssh_signal(SIGWINCH, sig_winch);
	alarm(UPDATE_INTERVAL);
}

void
stop_progress_meter(void)
{
	alarm(0);

	if (!can_output())
		return;

	/* Ensure we complete the progress */
	if (cur_pos != end_pos)
		refresh_progress_meter(1);

	atomicio(vwrite, STDOUT_FILENO, "\n", 1);
}

static void
sig_winch(int sig)
{
	win_resized = 1;
}

static void
setscreensize(void)
{
	struct winsize winsize;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsize) != -1 &&
	    winsize.ws_col != 0) {
		if (winsize.ws_col > MAX_WINSIZE)
			win_size = MAX_WINSIZE;
		else
			win_size = winsize.ws_col;
	} else
		win_size = DEFAULT_WINSIZE;
	win_size += 1;					/* trailing \0 */
}
