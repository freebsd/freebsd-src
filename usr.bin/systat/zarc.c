/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 - 2017, 2019 Yoshihiro Ota
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
#include <sys/types.h>
#include <sys/sysctl.h>

#include <inttypes.h>
#include <string.h>
#include <err.h>
#include <libutil.h>

#include "systat.h"
#include "extern.h"
#include "devs.h"

struct zfield {
	uint64_t arcstats;
	uint64_t arcstats_demand_data;
	uint64_t arcstats_demand_metadata;
	uint64_t arcstats_prefetch_data;
	uint64_t arcstats_prefetch_metadata;
	uint64_t zfetchstats;
	uint64_t arcstats_l2;
};

static struct zarcstats {
	struct zfield hits;
	struct zfield misses;
} curstat, initstat, oldstat;

struct zarcrates {
	struct zfield current;
	struct zfield total;
};

static void
getinfo(struct zarcstats *ls);

WINDOW *
openzarc(void)
{

	return (subwin(stdscr, LINES - 3 - 1, 0, MAINWIN_ROW, 0));
}

void
closezarc(WINDOW *w)
{

	if (w == NULL)
		return;
	wclear(w);
	wrefresh(w);
	delwin(w);
}

void
labelzarc(void)
{
	int row = 1;

	wmove(wnd, 0, 0); wclrtoeol(wnd);
	mvwprintw(wnd, 0, 31+1, "%4.4s %6.6s %6.6s | Total %4.4s %6.6s %6.6s",
		"Rate", "Hits", "Misses", "Rate", "Hits", "Misses");
#define L(str) mvwprintw(wnd, row++, 5, \
		"%-26.26s:   %%               |          %%", #str)
	L(arcstats);
	L(arcstats.demand_data);
	L(arcstats.demand_metadata);
	L(arcstats.prefetch_data);
	L(arcstats.prefetch_metadata);
	L(zfetchstats);
	L(arcstats.l2);
#undef L
	dslabel(12, 0, 18);
}

static int
calc_rate(uint64_t hits, uint64_t misses)
{
    if(hits)
	return 100 * hits / (hits + misses);
    else
	return 0;
}

static void
domode(struct zarcstats *delta, struct zarcrates *rate)
{
#define DO(stat) \
	delta->hits.stat = (curstat.hits.stat - oldstat.hits.stat); \
	delta->misses.stat = (curstat.misses.stat - oldstat.misses.stat); \
	rate->current.stat = calc_rate(delta->hits.stat, delta->misses.stat); \
	rate->total.stat = calc_rate(curstat.hits.stat, curstat.misses.stat)
	DO(arcstats);
	DO(arcstats_demand_data);
	DO(arcstats_demand_metadata);
	DO(arcstats_prefetch_data);
	DO(arcstats_prefetch_metadata);
	DO(zfetchstats);
	DO(arcstats_l2);
	DO(arcstats);
	DO(arcstats_demand_data);
	DO(arcstats_demand_metadata);
	DO(arcstats_prefetch_data);
	DO(arcstats_prefetch_metadata);
	DO(zfetchstats);
	DO(arcstats_l2);
#undef DO
}

void
showzarc(void)
{
	int row = 1;
	struct zarcstats delta = {};
	struct zarcrates rate = {};

	domode(&delta, &rate);

#define DO(stat, col, width) \
	sysputuint64(wnd, row, col, width, stat, HN_DIVISOR_1000)
#define	RATES(stat) mvwprintw(wnd, row, 31+1, "%3"PRIu64, rate.current.stat);\
	mvwprintw(wnd, row, 31+1+5+7+7+8, "%3"PRIu64, rate.total.stat)
#define	HITS(stat) DO(delta.hits.stat, 31+1+5, 6); \
	DO(curstat.hits.stat, 31+1+5+7+7+8+5, 6)
#define	MISSES(stat) DO(delta.misses.stat, 31+1+5+7, 6); \
	DO(curstat.misses.stat, 31+1+5+7+7+8+5+7, 6)
#define	E(stat) RATES(stat); HITS(stat); MISSES(stat); ++row
	E(arcstats);
	E(arcstats_demand_data);
	E(arcstats_demand_metadata);
	E(arcstats_prefetch_data);
	E(arcstats_prefetch_metadata);
	E(zfetchstats);
	E(arcstats_l2);
#undef DO
#undef E
#undef MISSES
#undef HITS
#undef RATES
	dsshow(12, 0, 18, &cur_dev, &last_dev);
}

int
initzarc(void)
{
	dsinit(12);
	getinfo(&initstat);
	curstat = oldstat = initstat;

	return 1;
}

void
resetzarc(void)
{

	initzarc();
}

static void
getinfo(struct zarcstats *ls)
{
	struct devinfo *tmp_dinfo;

	tmp_dinfo = last_dev.dinfo;
	last_dev.dinfo = cur_dev.dinfo;
	cur_dev.dinfo = tmp_dinfo;

	last_dev.snap_time = cur_dev.snap_time;
	dsgetinfo(&cur_dev);

	size_t size = sizeof(ls->hits.arcstats);
	if (sysctlbyname("kstat.zfs.misc.arcstats.hits",
		&ls->hits.arcstats, &size, NULL, 0) != 0)
		return;
	GETSYSCTL("kstat.zfs.misc.arcstats.misses",
		ls->misses.arcstats);
	GETSYSCTL("kstat.zfs.misc.arcstats.demand_data_hits",
		ls->hits.arcstats_demand_data);
	GETSYSCTL("kstat.zfs.misc.arcstats.demand_data_misses",
		ls->misses.arcstats_demand_data);
	GETSYSCTL("kstat.zfs.misc.arcstats.demand_metadata_hits",
		ls->hits.arcstats_demand_metadata);
	GETSYSCTL("kstat.zfs.misc.arcstats.demand_metadata_misses",
		ls->misses.arcstats_demand_metadata);
	GETSYSCTL("kstat.zfs.misc.arcstats.prefetch_data_hits",
		ls->hits.arcstats_prefetch_data);
	GETSYSCTL("kstat.zfs.misc.arcstats.prefetch_data_misses",
		ls->misses.arcstats_prefetch_data);
	GETSYSCTL("kstat.zfs.misc.arcstats.prefetch_metadata_hits",
		ls->hits.arcstats_prefetch_metadata);
	GETSYSCTL("kstat.zfs.misc.arcstats.prefetch_metadata_misses",
		ls->misses.arcstats_prefetch_metadata);
	GETSYSCTL("kstat.zfs.misc.zfetchstats.hits",
		ls->hits.zfetchstats);
	GETSYSCTL("kstat.zfs.misc.zfetchstats.misses",
		ls->misses.zfetchstats);
	GETSYSCTL("kstat.zfs.misc.arcstats.l2_hits",
		ls->hits.arcstats_l2);
	GETSYSCTL("kstat.zfs.misc.arcstats.l2_misses",
		ls->misses.arcstats_l2);
}

void
fetchzarc(void)
{

	oldstat = curstat;
	getinfo(&curstat);
}
