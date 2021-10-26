/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1980, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2017, 2020 Yoshihiro Ota
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#ifdef lint
static const char sccsid[] = "@(#)swap.c	8.3 (Berkeley) 4/29/95";
#endif

/*
 * swapinfo - based on a program of the same name by Kevin Lahey
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <kvm.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <err.h>

#include "systat.h"
#include "extern.h"
#include "devs.h"

static int pathlen;

WINDOW *
openswap(void)
{

	return (subwin(stdscr, LINES - 3 - 1, 0, MAINWIN_ROW, 0));
}

void
closeswap(WINDOW *w)
{

	if (w == NULL)
		return;
	wclear(w);
	wrefresh(w);
	delwin(w);
}

/*
 * The meat of all the swap stuff is stolen from pstat(8)'s
 * swapmode(), which is based on a program called swapinfo written by
 * Kevin Lahey <kml@rokkaku.atl.ga.us>.
 */

#define NSWAP	16

static struct kvm_swap kvmsw[NSWAP];
static int kvnsw, okvnsw;

int
initswap(void)
{
	static int once = 0;

	if (once)
		return (1);

	if ((kvnsw = kvm_getswapinfo(kd, kvmsw, NSWAP, 0)) < 0) {
		error("systat: kvm_getswapinfo failed");
		return (0);
	}
	pathlen = 80 - 50 /* % */ - 5 /* Used */ - 5 /* Size */ - 3 /* space */;
	dsinit(12);
	procinit();
	once = 1;

	return (1);
}

void
fetchswap(void)
{

	okvnsw = kvnsw;
	if ((kvnsw = kvm_getswapinfo(kd, kvmsw, NSWAP, 0)) < 0) {
		error("systat: kvm_getswapinfo failed");
		return;
	}

	struct devinfo *tmp_dinfo;

	tmp_dinfo = last_dev.dinfo;
	last_dev.dinfo = cur_dev.dinfo;
	cur_dev.dinfo = tmp_dinfo;

	last_dev.snap_time = cur_dev.snap_time;
	dsgetinfo(&cur_dev);
	procgetinfo();
}

void
labelswap(void)
{

	werase(wnd);

	dslabel(12, 0, LINES - DISKHIGHT - 1);

	if (kvnsw <= 0) {
		mvwprintw(wnd, 0, 0, "(swap not configured)");
		return;
	}

	mvwprintw(wnd, 0, 0, "%*s%5s %5s %s",
	    -pathlen, "Device/Path", "Size", "Used",
	    "|0%  /10  /20  /30  /40  / 60\\  70\\  80\\  90\\ 100|");
}

void
showswap(void)
{
	const char *name;
	int count, i;

	if (kvnsw != okvnsw)
		labelswap();

	dsshow(12, 0, LINES - DISKHIGHT - 1, &cur_dev, &last_dev);

	if (kvnsw <= 0)
		return;

	for (i = (kvnsw == 1 ? 0 : kvnsw); i >= 0; i--) {
		name = i == kvnsw ? "Total" : kvmsw[i].ksw_devname;
		mvwprintw(wnd, 1 + i, 0, "%-*.*s", pathlen, pathlen - 1, name);

		sysputpage(wnd, i + 1, pathlen, 5, kvmsw[i].ksw_total, 0);
		sysputpage(wnd, i + 1, pathlen + 5 + 1, 5, kvmsw[i].ksw_used,
		    0);

		if (kvmsw[i].ksw_used > 0) {
			count = 50 * kvmsw[i].ksw_used / kvmsw[i].ksw_total;
			sysputXs(wnd, i + 1, pathlen + 5 + 1 + 5 + 1, count);
		}
		wclrtoeol(wnd);
	}
	count = kvnsw == 1 ? 2 : 3;
	proclabel(kvnsw + count);
	procshow(kvnsw + count, LINES - 5 - kvnsw + 3 - DISKHIGHT + 1,
	    kvmsw[kvnsw].ksw_total);
}
