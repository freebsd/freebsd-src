/*
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
static char sccsid[] = "@(#)iostat.c	8.1 (Berkeley) 6/6/93";
#endif not lint

#include <sys/param.h>
#include <sys/dkstat.h>
#include <sys/buf.h>

#include <string.h>
#include <stdlib.h>
#include <nlist.h>
#include <paths.h>
#include "systat.h"
#include "extern.h"

static struct nlist namelist[] = {
#define X_DK_BUSY	0
	{ "_dk_busy" },
#define X_DK_TIME	1
	{ "_dk_time" },
#define X_DK_XFER	2
	{ "_dk_xfer" },
#define X_DK_WDS	3
	{ "_dk_wds" },
#define X_DK_SEEK	4
	{ "_dk_seek" },
#define X_CP_TIME	5
	{ "_cp_time" },
#ifdef vax
#define X_MBDINIT	(X_CP_TIME+1)
	{ "_mbdinit" },
#define X_UBDINIT	(X_CP_TIME+2)
	{ "_ubdinit" },
#endif
#ifdef tahoe
#define	X_VBDINIT	(X_CP_TIME+1)
	{ "_vbdinit" },
#endif
	{ "" },
};

static struct {
	int	dk_busy;
	long	cp_time[CPUSTATES];
	long	*dk_time;
	long	*dk_wds;
	long	*dk_seek;
	long	*dk_xfer;
} s, s1;

static  int linesperregion;
static  double etime;
static  int numbers = 0;		/* default display bar graphs */
static  int msps = 0;			/* default ms/seek shown */

static int barlabels __P((int));
static void histogram __P((double, int, double));
static int numlabels __P((int));
static int stats __P((int, int, int));
static void stat1 __P((int, int));


WINDOW *
openiostat()
{
	return (subwin(stdscr, LINES-1-5, 0, 5, 0));
}

void
closeiostat(w)
	WINDOW *w;
{
	if (w == NULL)
		return;
	wclear(w);
	wrefresh(w);
	delwin(w);
}

int
initiostat()
{
	if (namelist[X_DK_BUSY].n_type == 0) {
		if (kvm_nlist(kd, namelist)) {
			nlisterr(namelist);
			return(0);
		}
		if (namelist[X_DK_BUSY].n_type == 0) {
			error("Disk init information isn't in namelist");
			return(0);
		}
	}
	if (! dkinit())
		return(0);
	if (dk_ndrive) {
#define	allocate(e, t) \
    s./**/e = (t *)calloc(dk_ndrive, sizeof (t)); \
    s1./**/e = (t *)calloc(dk_ndrive, sizeof (t));
		allocate(dk_time, long);
		allocate(dk_wds, long);
		allocate(dk_seek, long);
		allocate(dk_xfer, long);
#undef allocate
	}
	return(1);
}

void
fetchiostat()
{
	if (namelist[X_DK_BUSY].n_type == 0)
		return;
	NREAD(X_DK_BUSY, &s.dk_busy, LONG);
	NREAD(X_DK_TIME, s.dk_time, dk_ndrive * LONG);
	NREAD(X_DK_XFER, s.dk_xfer, dk_ndrive * LONG);
	NREAD(X_DK_WDS, s.dk_wds, dk_ndrive * LONG);
	NREAD(X_DK_SEEK, s.dk_seek, dk_ndrive * LONG);
	NREAD(X_CP_TIME, s.cp_time, sizeof s.cp_time);
}

#define	INSET	10

void
labeliostat()
{
	int row;

	if (namelist[X_DK_BUSY].n_type == 0) {
		error("No dk_busy defined.");
		return;
	}
	row = 0;
	wmove(wnd, row, 0); wclrtobot(wnd);
	mvwaddstr(wnd, row++, INSET,
	    "/0   /10  /20  /30  /40  /50  /60  /70  /80  /90  /100");
	mvwaddstr(wnd, row++, 0, "cpu  user|");
	mvwaddstr(wnd, row++, 0, "     nice|");
	mvwaddstr(wnd, row++, 0, "   system|");
	mvwaddstr(wnd, row++, 0, "interrupt|");
	mvwaddstr(wnd, row++, 0, "     idle|");
	if (numbers)
		row = numlabels(row + 1);
	else
		row = barlabels(row + 1);
}

static int
numlabels(row)
	int row;
{
	int i, col, regions, ndrives;

#define COLWIDTH	14
#define DRIVESPERLINE	((wnd->maxx - INSET) / COLWIDTH)
	for (ndrives = 0, i = 0; i < dk_ndrive; i++)
		if (dk_select[i])
			ndrives++;
	regions = howmany(ndrives, DRIVESPERLINE);
	/*
	 * Deduct -regions for blank line after each scrolling region.
	 */
	linesperregion = (wnd->maxy - row - regions) / regions;
	/*
	 * Minimum region contains space for two
	 * label lines and one line of statistics.
	 */
	if (linesperregion < 3)
		linesperregion = 3;
	col = INSET;
	for (i = 0; i < dk_ndrive; i++)
		if (dk_select[i] && dk_mspw[i] != 0.0) {
			if (col + COLWIDTH >= wnd->maxx - INSET) {
				col = INSET, row += linesperregion + 1;
				if (row > wnd->maxy - (linesperregion + 1))
					break;
			}
			mvwaddstr(wnd, row, col + 4, dr_name[i]);
			mvwaddstr(wnd, row + 1, col, "bps tps msps");
			col += COLWIDTH;
		}
	if (col)
		row += linesperregion + 1;
	return (row);
}

static int
barlabels(row)
	int row;
{
	int i;

	mvwaddstr(wnd, row++, INSET,
	    "/0   /5   /10  /15  /20  /25  /30  /35  /40  /45  /50");
	linesperregion = 2 + msps;
	for (i = 0; i < dk_ndrive; i++)
		if (dk_select[i] && dk_mspw[i] != 0.0) {
			if (row > wnd->maxy - linesperregion)
				break;
			mvwprintw(wnd, row++, 0, "%-4.4s  bps|", dr_name[i]);
			mvwaddstr(wnd, row++, 0, "      tps|");
			if (msps)
				mvwaddstr(wnd, row++, 0, "     msps|");
		}
	return (row);
}


void
showiostat()
{
	register long t;
	register int i, row, col;

	if (namelist[X_DK_BUSY].n_type == 0)
		return;
	for (i = 0; i < dk_ndrive; i++) {
#define X(fld)	t = s.fld[i]; s.fld[i] -= s1.fld[i]; s1.fld[i] = t
		X(dk_xfer); X(dk_seek); X(dk_wds); X(dk_time);
	}
	etime = 0;
	for(i = 0; i < CPUSTATES; i++) {
		X(cp_time);
		etime += s.cp_time[i];
	}
	if (etime == 0.0)
		etime = 1.0;
	etime /= hertz;
	row = 1;
	for (i = 0; i < CPUSTATES; i++)
		stat1(row++, i);
	if (!numbers) {
		row += 2;
		for (i = 0; i < dk_ndrive; i++)
			if (dk_select[i] && dk_mspw[i] != 0.0) {
				if (row > wnd->maxy - linesperregion)
					break;
				row = stats(row, INSET, i);
			}
		return;
	}
	col = INSET;
	wmove(wnd, row + linesperregion, 0);
	wdeleteln(wnd);
	wmove(wnd, row + 3, 0);
	winsertln(wnd);
	for (i = 0; i < dk_ndrive; i++)
		if (dk_select[i] && dk_mspw[i] != 0.0) {
			if (col + COLWIDTH >= wnd->maxx - INSET) {
				col = INSET, row += linesperregion + 1;
				if (row > wnd->maxy - (linesperregion + 1))
					break;
				wmove(wnd, row + linesperregion, 0);
				wdeleteln(wnd);
				wmove(wnd, row + 3, 0);
				winsertln(wnd);
			}
			(void) stats(row + 3, col, i);
			col += COLWIDTH;
		}
}

static int
stats(row, col, dn)
	int row, col, dn;
{
	double atime, words, xtime, itime;

	atime = s.dk_time[dn];
	atime /= hertz;
	words = s.dk_wds[dn]*32.0;	/* number of words transferred */
	xtime = dk_mspw[dn]*words;	/* transfer time */
	itime = atime - xtime;		/* time not transferring */
	if (xtime < 0)
		itime += xtime, xtime = 0;
	if (itime < 0)
		xtime += itime, itime = 0;
	if (numbers) {
		mvwprintw(wnd, row, col, "%3.0f%4.0f%5.1f",
		    words / 512 / etime, s.dk_xfer[dn] / etime,
		    s.dk_seek[dn] ? itime * 1000. / s.dk_seek[dn] : 0.0);
		return (row);
	}
	wmove(wnd, row++, col);
	histogram(words / 512 / etime, 50, 1.0);
	wmove(wnd, row++, col);
	histogram(s.dk_xfer[dn] / etime, 50, 1.0);
	if (msps) {
		wmove(wnd, row++, col);
		histogram(s.dk_seek[dn] ? itime * 1000. / s.dk_seek[dn] : 0,
		   50, 1.0);
	}
	return (row);
}

static void
stat1(row, o)
	int row, o;
{
	register int i;
	double time;

	time = 0;
	for (i = 0; i < CPUSTATES; i++)
		time += s.cp_time[i];
	if (time == 0.0)
		time = 1.0;
	wmove(wnd, row, INSET);
#define CPUSCALE	0.5
	histogram(100.0 * s.cp_time[o] / time, 50, CPUSCALE);
}

static void
histogram(val, colwidth, scale)
	double val;
	int colwidth;
	double scale;
{
	char buf[10];
	register int k;
	register int v = (int)(val * scale) + 0.5;

	k = MIN(v, colwidth);
	if (v > colwidth) {
		snprintf(buf, sizeof(buf), "%4.1f", val);
		k -= strlen(buf);
		while (k--)
			waddch(wnd, 'X');
		waddstr(wnd, buf);
		return;
	}
	while (k--)
		waddch(wnd, 'X');
	wclrtoeol(wnd);
}

int
cmdiostat(cmd, args)
	char *cmd, *args;
{

	if (prefix(cmd, "msps"))
		msps = !msps;
	else if (prefix(cmd, "numbers"))
		numbers = 1;
	else if (prefix(cmd, "bars"))
		numbers = 0;
	else if (!dkcmd(cmd, args))
		return (0);
	wclear(wnd);
	labeliostat();
	refresh();
	return (1);
}
