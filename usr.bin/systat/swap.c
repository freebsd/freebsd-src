/*-
 * Copyright (c) 1980, 1992, 1993
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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)swap.c	8.3 (Berkeley) 4/29/95";
#endif
static const char rcsid[] =
  "$FreeBSD: src/usr.bin/systat/swap.c,v 1.12.2.1 2000/07/02 10:03:17 ps Exp $";
#endif /* not lint */

/*
 * swapinfo - based on a program of the same name by Kevin Lahey
 */

#include <sys/param.h>
#include <sys/conf.h>
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

extern char *getbsize __P((int *headerlenp, long *blocksizep));
void showspace __P((char *header, int hlen, long blocksize));

kvm_t	*kd;

static long blocksize;
static int hlen;

WINDOW *
openswap()
{
	return (subwin(stdscr, LINES-5-1, 0, 5, 0));
}

void
closeswap(w)
	WINDOW *w;
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

int
initswap()
{
	int i;
	char msgbuf[BUFSIZ];
	char *cp;
	static int once = 0;
	u_long ptr;
	struct kvm_swap dummy;

	if (once)
		return (1);

	if (kvm_getswapinfo(kd, &dummy, 1, 0) < 0) {
		snprintf(msgbuf, sizeof(msgbuf), "systat: kvm_getswapinfo failed");
		error(msgbuf);
		return (0);
	}

	once = 1;
	return (1);
}

static struct kvm_swap	kvmsw[16];
static int kvnsw;

void
fetchswap()
{
	kvnsw = kvm_getswapinfo(kd, kvmsw, 16, 0);
}

void
labelswap()
{
	char *header, *p;
	int row, i;

	fetchswap();

	row = 0;
	wmove(wnd, row, 0); wclrtobot(wnd);
	header = getbsize(&hlen, &blocksize);
	mvwprintw(wnd, row++, 0, "%-5s%*s%9s %55s",
	    "Disk", hlen, header, "Used",
	    "/0%  /10% /20% /30% /40% /50% /60% /70% /80% /90% /100");

	for (i = 0; i < kvnsw; ++i) {
		mvwprintw(wnd, i + 1, 0, "%-5s", kvmsw[i].ksw_devname);
	}
}

void
showswap()
{
	int i;
	int pagesize = getpagesize();

#define CONVERT(v)      ((int)((quad_t)(v) * pagesize / blocksize))

	for (i = 0; i <= kvnsw; ++i) {
		int col = 5;
		int count;

		if (i == kvnsw) {
			if (kvnsw == 1)
				break;
			mvwprintw(
			    wnd,
			    i + 1,
			    col,
			    "%-5s",
			    "Total"
			);
			col += 5;
		}
		if (kvmsw[i].ksw_total == 0) {
			mvwprintw(
			    wnd,
			    i + 1,
			    col + 5,
			    "(swap not configured)"
			);
			continue;
		}

		mvwprintw(
		    wnd, 
		    i + 1, 
		    col,
		    "%*d",
		    hlen, 
		    CONVERT(kvmsw[i].ksw_total)
		);
		col += hlen;

		mvwprintw(
		    wnd,
		    i + 1,
		    col, 
		    "%9d  ",
		    CONVERT(kvmsw[i].ksw_used)
		);

		count = (int)((double)kvmsw[i].ksw_used * 49.999 /
		    (double)kvmsw[i].ksw_total);

		while (count >= 0) {
			waddch(wnd, 'X');
			--count;
		}
	}
}
