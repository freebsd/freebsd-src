/*-
 * Copyright (c) 2012 Hudson River Trading LLC
 * Written by: John H. Baldwin <jhb@FreeBSD.org>
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <libprocstat.h>
#include <stdbool.h>
#include <stdio.h>
#include <libutil.h>

#include "procstat.h"

static struct {
	const char *ri_name;
	bool	ri_humanize;
	int	ri_scale;
} rusage_info[] = {
	{ "maximum RSS", true, 1 },
	{ "integral shared memory", true, 1 },
	{ "integral unshared data", true, 1 },
	{ "integral unshared stack", true, 1 },
	{ "page reclaims", false, 0 },
	{ "page faults", false, 0 },
	{ "swaps", false, 0 },
	{ "block reads", false, 0 },
	{ "block writes", false, 0 },
	{ "messages sent", false, 0 },
	{ "messages received", false, 0 },
	{ "signals received", false, 0 },
	{ "voluntary context switches", false, 0 },
	{ "involuntary context switches", false, 0 }
};

/* xxx days hh:mm:ss.uuuuuu */
static const char *
format_time(struct timeval *tv)
{
	static char buffer[32];
	int days, hours, minutes, seconds, used;

	minutes = tv->tv_sec / 60;
	seconds = tv->tv_sec % 60;
	hours = minutes / 60;
	minutes %= 60;
	days = hours / 24;
	hours %= 24;
	used = 0;
	if (days == 1)
		used += snprintf(buffer, sizeof(buffer), "1 day ");
	else if (days > 0)
		used += snprintf(buffer, sizeof(buffer), "%u days ", days);
	
	snprintf(buffer + used, sizeof(buffer) - used, "%02u:%02u:%02u.%06u   ",
	    hours, minutes, seconds, (unsigned int)tv->tv_usec);
	return (buffer);
}

static const char *
format_value(long value, bool humanize, int scale)
{
	static char buffer[14];

	if (scale != 0)
		value <<= scale * 10;
	if (humanize)
		humanize_number(buffer, sizeof(buffer), value, "B",
		    scale, HN_DECIMAL);
	else
		snprintf(buffer, sizeof(buffer), "%ld   ", value);
	return (buffer);
}

static void
print_prefix(struct kinfo_proc *kipp)
{

	printf("%5d ", kipp->ki_pid);
	if (Hflag)
		printf("%6d ", kipp->ki_tid);
	printf("%-16s ", kipp->ki_comm);
}

static void
print_rusage(struct kinfo_proc *kipp)
{
	long *lp;
	unsigned int i;

	print_prefix(kipp);
	printf("%-14s %32s\n", "user time",
	    format_time(&kipp->ki_rusage.ru_utime));
	print_prefix(kipp);
	printf("%-14s %32s\n", "system time",
	    format_time(&kipp->ki_rusage.ru_stime));
	lp = &kipp->ki_rusage.ru_maxrss;
	for (i = 0; i < nitems(rusage_info); i++) {
		print_prefix(kipp);
		printf("%-32s %14s\n", rusage_info[i].ri_name,
		    format_value(*lp, rusage_info[i].ri_humanize,
			rusage_info[i].ri_scale));
		lp++;
	}
}

void
procstat_rusage(struct procstat *procstat, struct kinfo_proc *kipp)
{
	struct kinfo_proc *kip;
	unsigned int count, i;

	if (!hflag) {
		printf("%5s ", "PID");
		if (Hflag)
			printf("%6s ", "TID");
		printf("%-16s %-32s %14s\n", "COMM", "RESOURCE",
		    "VALUE        ");
	}

	if (!Hflag) {
		print_rusage(kipp);
		return;
	}

	kip = procstat_getprocs(procstat, KERN_PROC_PID | KERN_PROC_INC_THREAD,
	    kipp->ki_pid, &count);
	if (kip == NULL)
		return;
	kinfo_proc_sort(kip, count);
	for (i = 0; i < count; i++)
		print_rusage(&kip[i]);
	procstat_freeprocs(procstat, kip);
}
