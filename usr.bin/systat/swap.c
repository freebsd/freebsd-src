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
static char sccsid[] = "@(#)swap.c	8.2 (Berkeley) 2/21/94";
#endif /* not lint */

/*
 * swapinfo - based on a program of the same name by Kevin Lahey
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/rlist.h>

#include <kvm.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "systat.h"
#include "extern.h"

extern char *devname __P((int, int));
extern char *getbsize __P((int *headerlenp, long *blocksizep));
void showspace __P((char *header, int hlen, long blocksize));

kvm_t	*kd;

struct nlist syms[] = {
	{ "_swaplist" },/* list of free swap areas */
#define VM_SWAPLIST	0
	{ "_swdevt" },	/* list of swap devices and sizes */
#define VM_SWDEVT	1
	{ "_nswap" },	/* size of largest swap device */
#define VM_NSWAP	2
	{ "_nswdev" },	/* number of swap devices */
#define VM_NSWDEV	3
	{ "_dmmax" },	/* maximum size of a swap block */
#define VM_DMMAX	4
	0
};

static int nswap, nswdev, dmmax;
static struct swdevt *sw;
static long *perdev, blocksize;
static int nfree, hlen;
static struct rlisthdr swaplist;

#define	SVAR(var) __STRING(var)	/* to force expansion */
#define	KGET(idx, var) \
	KGET1(idx, &var, sizeof(var), SVAR(var), (0))
#define	KGET1(idx, p, s, msg, rv) \
	KGET2(syms[idx].n_value, p, s, msg, rv)
#define	KGET2(addr, p, s, msg, rv) \
	if (kvm_read(kd, addr, p, s) != s) { \
		error("cannot read %s: %s", msg, kvm_geterr(kd)); \
		return rv; \
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

/*
 * The meat of all the swap stuff is stolen from pstat(8)'s
 * swapmode(), which is based on a program called swapinfo written by
 * Kevin Lahey <kml@rokkaku.atl.ga.us>.
 */

initswap()
{
	int i;
	char msgbuf[BUFSIZ];
	static int once = 0;
	u_long ptr;

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
	if ((sw = malloc(nswdev * sizeof(*sw))) == NULL ||
	    (perdev = malloc(nswdev * sizeof(*perdev))) == NULL)
		err(1, "malloc");
	KGET1(VM_SWDEVT, &ptr, sizeof ptr, "swdevt", (0));
	KGET2(ptr, sw, nswdev * sizeof(*sw), "*swdevt", (0));
	once = 1;
	return (1);
}

void
fetchswap()
{
	struct rlist head;
	struct rlist *swapptr;

	/* Count up swap space. */
	nfree = 0;
	memset(perdev, 0, nswdev * sizeof(*perdev));
	KGET1(VM_SWAPLIST, &swaplist, sizeof swaplist, "swaplist", /* none */);
	swapptr = swaplist.rlh_list;
	while (swapptr) {
		int	top, bottom, next_block;

		KGET2((unsigned long)swapptr, &head,
		      sizeof(struct rlist), "swapptr", /* none */);

		top = head.rl_end;
		bottom = head.rl_start;

		nfree += top - bottom + 1;

		/*
		 * Swap space is split up among the configured disks.
		 *
		 * For interleaved swap devices, the first dmmax blocks
		 * of swap space some from the first disk, the next dmmax
		 * blocks from the next, and so on up to nswap blocks.
		 *
		 * The list of free space joins adjacent free blocks,
		 * ignoring device boundries.  If we want to keep track
		 * of this information per device, we'll just have to
		 * extract it ourselves.
		 */
		while (top / dmmax != bottom / dmmax) {
			next_block = ((bottom + dmmax) / dmmax);
			perdev[(bottom / dmmax) % nswdev] +=
				next_block * dmmax - bottom;
			bottom = next_block * dmmax;
		}
		perdev[(bottom / dmmax) % nswdev] +=
			top - bottom + 1;

		swapptr = head.rl_next;
	}

}

void
labelswap()
{
	char *header;
	int row, i;

	row = 0;
	wmove(wnd, row, 0); wclrtobot(wnd);
	header = getbsize(&hlen, &blocksize);
	mvwprintw(wnd, row++, 0, "%-5s%*s%9s %55s",
	    "Disk", hlen, header, "Used",
	    "/0%  /10% /20% /30% /40% /50% /60% /70% /80% /90% /100");
	for (i = 0; i < nswdev; i++) {
		if (!sw[i].sw_freed)
			continue;
		mvwprintw(wnd, i + 1, 0, "%-5s",
			  sw[i].sw_dev != NODEV?
			  devname(sw[i].sw_dev, S_IFBLK): "[NFS]");
	}
}

void
showswap()
{
	int col, row, div, i, j, k, avail, npfree, used, xsize, xfree;

	div = blocksize / 512;
	avail = npfree = 0;
	for (i = k = 0; i < nswdev; i++) {
		/*
		 * Don't report statistics for partitions which have not
		 * yet been activated via swapon(8).
		 */
		if (!sw[i].sw_freed)
			continue;

		col = 5;
		mvwprintw(wnd, i + 1, col, "%*d", hlen, sw[i].sw_nblks / div);
		col += hlen;

		/*
		 * The first dmmax is never allocated to avoid trashing of
		 * disklabels
		 */
		xsize = sw[i].sw_nblks - dmmax;
		xfree = perdev[i];
		used = xsize - xfree;
		mvwprintw(wnd, i + 1, col, "%9d  ", used / div);
		for (j = (100 * used / xsize + 1) / 2; j > 0; j--)
			waddch(wnd, 'X');
		npfree++;
		avail += xsize;
		k++;
	}
	/*
	 * If only one partition has been set up via swapon(8), we don't
	 * need to bother with totals.
	 */
	if (npfree > 1) {
		used = avail - nfree;
		mvwprintw(wnd, k + 1, 0, "%-5s%*d%9d  ",
		    "Total", hlen, avail / div, used / div);
		for (j = (100 * used / avail + 1) / 2; j > 0; j--)
			waddch(wnd, 'X');
	}
}
