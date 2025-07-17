/*-
 * Copyright (c) 2016 Maksym Sobolyev <sobomax@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/time.h>
#include <time.h>

#include "uc_check_time.h"

static const struct timeval max_diff_tv = {.tv_sec = 1, .tv_usec = 0};
static const struct timespec max_diff_ts = {.tv_sec = 1, .tv_nsec = 0};

int
uc_check_bintime(const struct bintime *mt)
{
	struct timespec bt;

	bintime2timespec(mt, &bt);
	return (uc_check_timespec_real(&bt));
}

int
uc_check_timeval(const struct timeval *bt)
{
	struct timeval ct, dt;

	if (gettimeofday(&ct, NULL) < 0)
		return (-1);
	timersub(&ct, bt, &dt);
	if (!timercmp(&dt, &max_diff_tv, <))
		return (-1);

	return (0);
}

int
uc_check_timespec_real(const struct timespec *bt)
{
	struct timespec ct;

	if (clock_gettime(CLOCK_REALTIME, &ct) < 0)
		return (-1);
	timespecsub(&ct, bt, &ct);
	if (!timespeccmp(&ct, &max_diff_ts, <))
		return (-1);

	return (0);
}

int
uc_check_timespec_mono(const struct timespec *bt)
{
	struct timespec ct;

	if (clock_gettime(CLOCK_MONOTONIC, &ct) < 0)
		return (-1);
	timespecsub(&ct, bt, &ct);
	if (!timespeccmp(&ct, &max_diff_ts, <))
		return (-1);

	return (0);
}
