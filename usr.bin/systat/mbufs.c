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
static char sccsid[] = "@(#)mbufs.c	8.1 (Berkeley) 6/6/93";
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include "systat.h"
#include "extern.h"

static struct mbpstat **mbpstat;
static int num_objs, ncpu;
#define	GENLST	(num_objs - 1)

/* XXX: mbtypes stats temporarily disabled. */
#if 0
static u_long *m_mbtypes;
static int nmbtypes;

static struct mtnames {
	int mt_type;
	char *mt_name;
} mtnames[] = {
	{ MT_DATA, 	"data"},
	{ MT_HEADER,	"headers"},
	{ MT_SONAME,	"socknames"},
	{ MT_FTABLE,	"frags"},
	{ MT_CONTROL,	"control"},
	{ MT_OOBDATA,	"oobdata"}
};

#define	NNAMES	(sizeof (mtnames) / sizeof (mtnames[0]))
#endif

WINDOW *
openmbufs()
{
	return (subwin(stdscr, LINES-5-1, 0, 5, 0));
}

void
closembufs(w)
	WINDOW *w;
{
	if (w == NULL)
		return;
	wclear(w);
	wrefresh(w);
	delwin(w);
}

void
labelmbufs()
{
	wmove(wnd, 0, 0); wclrtoeol(wnd);
	mvwaddstr(wnd, 0, 10,
	    "/0   /5   /10  /15  /20  /25  /30  /35  /40  /45  /50  /55  /60");
}

void
showmbufs()
{
	int i, j, max, index;
	u_long totfree;
	char buf[10];
	char *mtname;

/* XXX: mbtypes stats temporarily disabled (will be back soon!) */
#if 0
	for (j = 0; j < wnd->_maxy; j++) {
		max = 0, index = -1;
		for (i = 0; i < wnd->_maxy; i++) {
			if (i == MT_FREE)
				continue;
			if (i >= nmbtypes)
				break;
			if (m_mbtypes[i] > max) {
				max = m_mbtypes[i];
				index = i;
			}
		}
		if (max == 0)
			break;

		mtname = NULL;
		for (i = 0; i < NNAMES; i++)
			if (mtnames[i].mt_type == index)
				mtname = mtnames[i].mt_name;
		if (mtname == NULL)
			mvwprintw(wnd, 1+j, 0, "%10d", index);
		else
			mvwprintw(wnd, 1+j, 0, "%-10.10s", mtname);
		wmove(wnd, 1 + j, 10);
		if (max > 60) {
			snprintf(buf, sizeof(buf), " %d", max);
			max = 60;
			while (max--)
				waddch(wnd, 'X');
			waddstr(wnd, buf);
		} else
			while (max--)
				waddch(wnd, 'X');
		wclrtoeol(wnd);
		m_mbtypes[index] = 0;
	}
#endif

	/*
	 * Print total number of free mbufs.
	 */
	totfree = mbpstat[GENLST]->mb_mbfree; 
	for (i = 0; i < ncpu; i++)
		totfree += mbpstat[i]->mb_mbfree;
	j = 0;	/* XXX */
	if (totfree > 0) {
		mvwprintw(wnd, 1+j, 0, "%-10.10s", "free");
		if (totfree > 60) {
			snprintf(buf, sizeof(buf), " %lu", totfree);
			totfree = 60;
			while(totfree--)
				waddch(wnd, 'X');
			waddstr(wnd, buf);
		} else {
			while(totfree--)
				waddch(wnd, 'X');
		}
		wclrtoeol(wnd);
		j++;
	}
	wmove(wnd, 1+j, 0); wclrtobot(wnd);
}

int
initmbufs()
{
	int i;
	size_t len;
#if 0
	size_t mbtypeslen;

	if (sysctlbyname("kern.ipc.mbtypes", NULL, &mbtypeslen, NULL, 0) < 0) {
		error("sysctl getting mbtypes size failed");
		return 0;
	}
	if ((m_mbtypes = calloc(1, mbtypeslen)) == NULL) {
		error("calloc mbtypes failed");
		return 0;
	}
	nmbtypes = mbtypeslen / sizeof(*m_mbtypes);
#endif
	len = sizeof(int);
	if (sysctlbyname("kern.smp.cpus", &ncpu, &len, NULL, 0) < 0) {
		error("sysctl getting number of cpus");
		return 0;
	}
	if (sysctlbyname("kern.ipc.mb_statpcpu", NULL, &len, NULL, 0) < 0) {
		error("sysctl getting mbpstat total size failed");
		return 0;
	}
	num_objs = (int)(len / sizeof(struct mbpstat));
	if ((mbpstat = calloc(num_objs, sizeof(struct mbpstat *))) == NULL) {
		error("calloc mbpstat pointers failed");
		return 0;
	}
	if ((mbpstat[0] = calloc(num_objs, sizeof(struct mbpstat))) == NULL) {
		error("calloc mbpstat structures failed");
		return 0;
	}
	for (i = 0; i < num_objs; i++)
		mbpstat[i] = mbpstat[0] + i;

	return 1;
}

void
fetchmbufs()
{
	size_t len;

	len = num_objs * sizeof(struct mbpstat);
	if (sysctlbyname("kern.ipc.mb_statpcpu", mbpstat[0], &len, NULL, 0) < 0)
		printw("sysctl: mbpstat: %s", strerror(errno));
#if 0
	len = nmbtypes * sizeof *m_mbtypes;
	if (sysctlbyname("kern.ipc.mbtypes", m_mbtypes, &len, 0, 0) < 0)
		printw("sysctl: mbtypes: %s", strerror(errno));
#endif
}
