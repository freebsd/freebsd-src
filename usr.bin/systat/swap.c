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
static char sccsid[] = "@(#)swap.c	8.3 (Berkeley) 4/29/95";
#endif /* not lint */

/*
 * swapinfo - based on a program of the same name by Kevin Lahey
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/map.h>
#include <sys/stat.h>

#include <kvm.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "systat.h"
#include "extern.h"

extern char *getbsize __P((int *headerlenp, long *blocksizep));
void showspace __P((char *header, int hlen, long blocksize));

kvm_t	*kd;

struct nlist syms[] = {
	{ "_swapmap" },	/* list of free swap areas */
#define VM_SWAPMAP	0
	{ "_nswapmap" },/* size of the swap map */
#define VM_NSWAPMAP	1
	{ "_swdevt" },	/* list of swap devices and sizes */
#define VM_SWDEVT	2
	{ "_nswap" },	/* size of largest swap device */
#define VM_NSWAP	3
	{ "_nswdev" },	/* number of swap devices */
#define VM_NSWDEV	4
	{ "_dmmax" },	/* maximum size of a swap block */
#define VM_DMMAX	5
	0
};

static int nswap, nswdev, dmmax, nswapmap;
static struct swdevt *sw;
static long *perdev, blocksize;
static struct map *swapmap, *kswapmap;
static struct mapent *mp;
static int nfree, hlen;

#define	SVAR(var) __STRING(var)	/* to force expansion */
#define	KGET(idx, var) \
	KGET1(idx, &var, sizeof(var), SVAR(var))
#define	KGET1(idx, p, s, msg) \
	KGET2(syms[idx].n_value, p, s, msg)
#define	KGET2(addr, p, s, msg) \
	if (kvm_read(kd, addr, p, s) != s) { \
		error("cannot read %s: %s", msg, kvm_geterr(kd)); \
		return (0); \
	}

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

initswap()
{
	int i;
	char msgbuf[BUFSIZ];
	static int once = 0;

	if (once)
		return (1);
	if (kvm_nlist(kd, syms)) {
		strcpy(msgbuf, "systat: swap: cannot find");
		for (i = 0; syms[i].n_name != NULL; i++) {
			if (syms[i].n_value == 0) {
				strcat(msgbuf, " ");
				strcat(msgbuf, syms[i].n_name);
			}
		}
		error(msgbuf);
		return (0);
	}
	KGET(VM_NSWAP, nswap);
	KGET(VM_NSWDEV, nswdev);
	KGET(VM_DMMAX, dmmax);
	KGET(VM_NSWAPMAP, nswapmap);
	KGET(VM_SWAPMAP, kswapmap);	/* kernel `swapmap' is a pointer */
	if ((sw = malloc(nswdev * sizeof(*sw))) == NULL ||
	    (perdev = malloc(nswdev * sizeof(*perdev))) == NULL ||
	    (mp = malloc(nswapmap * sizeof(*mp))) == NULL) {
		error("swap malloc");
		return (0);
	}
	KGET1(VM_SWDEVT, sw, nswdev * sizeof(*sw), "swdevt");
	once = 1;
	return (1);
}

void
fetchswap()
{
	int s, e, i;

	s = nswapmap * sizeof(*mp);
	if (kvm_read(kd, (long)kswapmap, mp, s) != s)
		error("cannot read swapmap: %s", kvm_geterr(kd));

	/* first entry in map is `struct map'; rest are mapent's */
	swapmap = (struct map *)mp;
	if (nswapmap != swapmap->m_limit - (struct mapent *)kswapmap)
		error("panic: swap: nswapmap goof");

	/*
	 * Count up swap space.
	 */
	nfree = 0;
	bzero(perdev, nswdev * sizeof(*perdev));
	for (mp++; mp->m_addr != 0; mp++) {
		s = mp->m_addr;			/* start of swap region */
		e = mp->m_addr + mp->m_size;	/* end of region */
		nfree += mp->m_size;

		/*
		 * Swap space is split up among the configured disks.
		 * The first dmmax blocks of swap space some from the
		 * first disk, the next dmmax blocks from the next, 
		 * and so on.  The list of free space joins adjacent
		 * free blocks, ignoring device boundries.  If we want
		 * to keep track of this information per device, we'll
		 * just have to extract it ourselves.
		 */

		/* calculate first device on which this falls */
		i = (s / dmmax) % nswdev;
		while (s < e) {		/* XXX this is inefficient */
			int bound = roundup(s + 1, dmmax);

			if (bound > e)
				bound = e;
			perdev[i] += bound - s;
			if (++i >= nswdev)
				i = 0;
			s = bound;
		}
	}
}

void
labelswap()
{
	char *header, *p;
	int row, i;

	row = 0;
	wmove(wnd, row, 0); wclrtobot(wnd);
	header = getbsize(&hlen, &blocksize);
	mvwprintw(wnd, row++, 0, "%-5s%*s%9s  %55s",
	    "Disk", hlen, header, "Used",
	    "/0%  /10% /20% /30% /40% /50% /60% /70% /80% /90% /100%");
	for (i = 0; i < nswdev; i++) {
		p = devname(sw[i].sw_dev, S_IFBLK);
		mvwprintw(wnd, i + 1, 0, "%-5s", p == NULL ? "??" : p);
	}
}

void
showswap()
{
	int col, row, div, i, j, avail, npfree, used, xsize, xfree;

	div = blocksize / 512;
	avail = npfree = 0;
	for (i = 0; i < nswdev; i++) {
		col = 5;
		mvwprintw(wnd, i + 1, col, "%*d", hlen, sw[i].sw_nblks / div);
		col += hlen;
		/*
		 * Don't report statistics for partitions which have not
		 * yet been activated via swapon(8).
		 */
		if (!sw[i].sw_freed) {
			mvwprintw(wnd, i + 1, col + 8,
			    "0  *** not available for swapping ***");
			continue;
		}
		xsize = sw[i].sw_nblks;
		xfree = perdev[i];
		used = xsize - xfree;
		mvwprintw(wnd, i + 1, col, "%9d  ", used / div);
		for (j = (100 * used / xsize + 1) / 2; j > 0; j--)
			waddch(wnd, 'X');
		npfree++;
		avail += xsize;
	}
	/* 
	 * If only one partition has been set up via swapon(8), we don't
	 * need to bother with totals.
	 */
	if (npfree > 1) {
		used = avail - nfree;
		mvwprintw(wnd, i + 1, 0, "%-5s%*d%9d  ",
		    "Total", hlen, avail / div, used / div);
		for (j = (100 * used / avail + 1) / 2; j > 0; j--)
			waddch(wnd, 'X');
	}
}
