/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2003, Trent Nelson, <trent@arpa.com>.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTIFSTAT_ERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_mib.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <fnmatch.h>

#include "systat.h"
#include "extern.h"
#include "convtbl.h"

				/* Column numbers */

#define C1	0		/*  0-19 */
#define C2	20		/* 20-39 */
#define C3	40		/* 40-59 */
#define C4	60		/* 60-80 */
#define C5	80		/* Used for label positioning. */

static const int col2 = C2;
static const int col3 = C3;
static const int col4 = C4;

static SLIST_HEAD(, if_stat)		curlist;

struct if_stat {
	SLIST_ENTRY(if_stat)	 link;
	char	display_name[IF_NAMESIZE];
	char	dev_name[IFNAMSIZ]; 	/* copied from ifmibdata */
	struct	ifmibdata if_mib;
	struct	timeval tv;
	struct	timeval tv_lastchanged;
	uint64_t if_in_curtraffic;
	uint64_t if_out_curtraffic;
	uint64_t if_in_traffic_peak;
	uint64_t if_out_traffic_peak;
	uint64_t if_in_curpps;
	uint64_t if_out_curpps;
	uint64_t if_in_pps_peak;
	uint64_t if_out_pps_peak;
	u_int	if_row;			/* Index into ifmib sysctl */
	int 	if_ypos;		/* -1 if not being displayed */
	bool	display;
	u_int	match;
};

static	 int needclear = 0;
static	 bool displayall = false;

static	 void  format_device_name(struct if_stat *);
static	 int   getifmibdata(const int, struct ifmibdata *);
static	 void  sort_interface_list(void);
static	 u_int getifnum(void);
static	 void  clearifstat(void);

#define IFSTAT_ERR(n, s)	do {					\
	putchar('\014');						\
	closeifstat(wnd);						\
	err((n), (s));							\
} while (0)

#define TOPLINE 3
#define TOPLABEL \
"      Interface           Traffic               Peak                Total"

#define STARTING_ROW	(TOPLINE + 1)
#define ROW_SPACING	(3)

#define IN_col2		(showpps ? ifp->if_in_curpps : ifp->if_in_curtraffic)
#define OUT_col2	(showpps ? ifp->if_out_curpps : ifp->if_out_curtraffic)
#define IN_col3		(showpps ? \
		ifp->if_in_pps_peak : ifp->if_in_traffic_peak)
#define OUT_col3	(showpps ? \
		ifp->if_out_pps_peak : ifp->if_out_traffic_peak)
#define IN_col4		(showpps ? \
	ifp->if_mib.ifmd_data.ifi_ipackets : ifp->if_mib.ifmd_data.ifi_ibytes)
#define OUT_col4	(showpps ? \
	ifp->if_mib.ifmd_data.ifi_opackets : ifp->if_mib.ifmd_data.ifi_obytes)

#define EMPTY_COLUMN 	"                    "
#define CLEAR_COLUMN(y, x)	mvprintw((y), (x), "%20s", EMPTY_COLUMN);

#define DOPUTRATE(c, r, d)	do {					\
	CLEAR_COLUMN(r, c);						\
	if (showpps) {							\
		mvprintw(r, (c), "%10.3f %cp%s  ",			\
			 convert(d##_##c, curscale),			\
			 *get_string(d##_##c, curscale),		\
			 "/s");						\
	}								\
	else {								\
		mvprintw(r, (c), "%10.3f %s%s  ",			\
			 convert(d##_##c, curscale),			\
			 get_string(d##_##c, curscale),			\
			 "/s");						\
	}								\
} while (0)

#define DOPUTTOTAL(c, r, d)	do {					\
	CLEAR_COLUMN((r), (c));						\
	if (showpps) {							\
		mvprintw((r), (c), "%12.3f %cp  ",			\
			 convert(d##_##c, SC_AUTO),			\
			 *get_string(d##_##c, SC_AUTO));		\
	}								\
	else {								\
		mvprintw((r), (c), "%12.3f %s  ",			\
			 convert(d##_##c, SC_AUTO),			\
			 get_string(d##_##c, SC_AUTO));			\
	}								\
} while (0)

#define PUTRATE(c, r)	do {						\
	DOPUTRATE(c, (r), IN);						\
	DOPUTRATE(c, (r)+1, OUT);					\
} while (0)

#define PUTTOTAL(c, r)	do {						\
	DOPUTTOTAL(c, (r), IN);						\
	DOPUTTOTAL(c, (r)+1, OUT);					\
} while (0)

#define PUTNAME(p) do {							\
	mvprintw(p->if_ypos, 0, "%s", p->display_name);			\
	mvprintw(p->if_ypos, col2-3, "%s", (const char *)"in");		\
	mvprintw(p->if_ypos+1, col2-3, "%s", (const char *)"out");	\
} while (0)

WINDOW *
openifstat(void)
{
	return (subwin(stdscr, LINES-3-1, 0, MAINWIN_ROW, 0));
}

void
closeifstat(WINDOW *w)
{
	struct if_stat	*node = NULL;

	while (!SLIST_EMPTY(&curlist)) {
		node = SLIST_FIRST(&curlist);
		SLIST_REMOVE_HEAD(&curlist, link);
		free(node);
	}

	if (w != NULL) {
		wclear(w);
		wrefresh(w);
		delwin(w);
	}

	return;
}

void
labelifstat(void)
{

	wmove(wnd, TOPLINE, 0);
	wclrtoeol(wnd);
	mvprintw(TOPLINE, 0, "%s", TOPLABEL);

	return;
}

void
showifstat(void)
{
	struct	if_stat *ifp = NULL;
	
	SLIST_FOREACH(ifp, &curlist, link) {
		if (ifp->if_ypos < LINES - 3 && ifp->if_ypos != -1) {
			if (!ifp->display || ifp->match == 0) {
					wmove(wnd, ifp->if_ypos, 0);
					wclrtoeol(wnd);
					wmove(wnd, ifp->if_ypos + 1, 0);
					wclrtoeol(wnd);
			} else {
				PUTNAME(ifp);
				PUTRATE(col2, ifp->if_ypos);
				PUTRATE(col3, ifp->if_ypos);
				PUTTOTAL(col4, ifp->if_ypos);
			}
		}
	}

	return;
}

int
initifstat(void)
{
	struct   if_stat *p = NULL;
	u_int	 n, i;

	n = getifnum();
	if (n <= 0)
		return (-1);

	SLIST_INIT(&curlist);

	for (i = 0; i < n; i++) {
		p = (struct if_stat *)calloc(1, sizeof(struct if_stat));
		if (p == NULL)
			IFSTAT_ERR(1, "out of memory");
		p->if_row = i+1;
		if (getifmibdata(p->if_row, &p->if_mib) == -1) {
			free(p);
			continue;
		}
		SLIST_INSERT_HEAD(&curlist, p, link);
		format_device_name(p);
		p->match = 1;

		/*
		 * Initially, we only display interfaces that have
		 * received some traffic unless display-all is on.
		 */
		if (displayall || p->if_mib.ifmd_data.ifi_ibytes != 0)
			p->display = true;
	}

	sort_interface_list();

	return (1);
}

void
fetchifstat(void)
{
	struct	if_stat *ifp = NULL, *temp_var;
	struct	timeval tv, new_tv, old_tv;
	double	elapsed = 0.0;
	uint64_t new_inb, new_outb, old_inb, old_outb = 0;
	uint64_t new_inp, new_outp, old_inp, old_outp = 0;

	SLIST_FOREACH_SAFE(ifp, &curlist, link, temp_var) {
		/*
		 * Grab a copy of the old input/output values before we
		 * call getifmibdata().
		 */
		old_inb = ifp->if_mib.ifmd_data.ifi_ibytes;
		old_outb = ifp->if_mib.ifmd_data.ifi_obytes;
		old_inp = ifp->if_mib.ifmd_data.ifi_ipackets;
		old_outp = ifp->if_mib.ifmd_data.ifi_opackets;
		ifp->tv_lastchanged = ifp->if_mib.ifmd_data.ifi_lastchange;

		(void)gettimeofday(&new_tv, NULL);
		if (getifmibdata(ifp->if_row, &ifp->if_mib) == -1 ) {
			/* if a device was removed */
			SLIST_REMOVE(&curlist, ifp, if_stat, link);
			free(ifp);
			needsort = 1;
			clearifstat();
		} else if (strcmp(ifp->dev_name, ifp->if_mib.ifmd_name) != 0 ) {
			/* a device was removed and another one was added */
			format_device_name(ifp);
			/* clear to the current value for the new device */
			old_inb = ifp->if_mib.ifmd_data.ifi_ibytes;
			old_outb = ifp->if_mib.ifmd_data.ifi_obytes;
			old_inp = ifp->if_mib.ifmd_data.ifi_ipackets;
			old_outp = ifp->if_mib.ifmd_data.ifi_opackets;
			needsort = 1;
		}

		new_inb = ifp->if_mib.ifmd_data.ifi_ibytes;
		new_outb = ifp->if_mib.ifmd_data.ifi_obytes;
		new_inp = ifp->if_mib.ifmd_data.ifi_ipackets;
		new_outp = ifp->if_mib.ifmd_data.ifi_opackets;

		/* Display interface if it's received some traffic. */
		if (!ifp->display && new_inb > 0 && old_inb == 0) {
			ifp->display = true;
			needsort = 1;
		}

		/*
		 * The rest is pretty trivial.  Calculate the new values
		 * for our current traffic rates, and while we're there,
		 * see if we have new peak rates.
		 */
		old_tv = ifp->tv;
		timersub(&new_tv, &old_tv, &tv);
		elapsed = tv.tv_sec + (tv.tv_usec * 1e-6);

		ifp->if_in_curtraffic = new_inb - old_inb;
		ifp->if_out_curtraffic = new_outb - old_outb;

		ifp->if_in_curpps = new_inp - old_inp;
		ifp->if_out_curpps = new_outp - old_outp;

		/*
		 * Rather than divide by the time specified on the comm-
		 * and line, we divide by ``elapsed'' as this is likely
		 * to be more accurate.
		 */
		ifp->if_in_curtraffic /= elapsed;
		ifp->if_out_curtraffic /= elapsed;
		ifp->if_in_curpps /= elapsed;
		ifp->if_out_curpps /= elapsed;

		if (ifp->if_in_curtraffic > ifp->if_in_traffic_peak)
			ifp->if_in_traffic_peak = ifp->if_in_curtraffic;

		if (ifp->if_out_curtraffic > ifp->if_out_traffic_peak)
			ifp->if_out_traffic_peak = ifp->if_out_curtraffic;

		if (ifp->if_in_curpps > ifp->if_in_pps_peak)
			ifp->if_in_pps_peak = ifp->if_in_curpps;

		if (ifp->if_out_curpps > ifp->if_out_pps_peak)
			ifp->if_out_pps_peak = ifp->if_out_curpps;

		ifp->tv.tv_sec = new_tv.tv_sec;
		ifp->tv.tv_usec = new_tv.tv_usec;

	}

	if (needsort)
		sort_interface_list();

	return;
}

/*
 * We want to right justify our interface names against the first column
 * (first sixteen or so characters), so we need to do some alignment.
 * We save original name so that we can find a same spot is take by a
 * different device.
 */
static void
format_device_name(struct if_stat *ifp)
{

	if (ifp != NULL ) {
		snprintf(ifp->display_name, IF_NAMESIZE, "%*s", IF_NAMESIZE-1,
		    ifp->if_mib.ifmd_name);
		strcpy(ifp->dev_name, ifp->if_mib.ifmd_name);
	}
}

static int
check_match(const char *ifname) 
{
	char *p = matchline, *ch, t;
	int match = 0, mlen;

	if (matchline == NULL)
		return (0);

	/* Strip leading whitespaces */
	while (*p == ' ')
		p ++;

	ch = p;
	while ((mlen = strcspn(ch, " ;,")) != 0) {
		p = ch + mlen;
		t = *p;
		if (p - ch > 0) {
			*p = '\0';
			if (fnmatch(ch, ifname, FNM_CASEFOLD) == 0) {
				*p = t;
				return (1);
			}
			*p = t;
			ch = p + strspn(p, " ;,");
		}
		else {
			ch = p + strspn(p, " ;,");
		}
	}

	return (match);
}

/*
 * This function iterates through our list of interfaces, identifying
 * those that are to be displayed (ifp->display = 1).  For each interf-
 * rface that we're displaying, we generate an appropriate position for
 * it on the screen (ifp->if_ypos).
 *
 * This function is called any time a change is made to an interface's
 * ``display'' state.
 */
void
sort_interface_list(void)
{
	struct	if_stat	*ifp = NULL;
	u_int	y = 0;

	y = STARTING_ROW;
	SLIST_FOREACH(ifp, &curlist, link) {
		if (matchline && !check_match(ifp->if_mib.ifmd_name))
			ifp->match = 0;
		else
			ifp->match = 1;
		if (ifp->display && ifp->match) {
			ifp->if_ypos = y;
			y += ROW_SPACING;
		}
		else
			ifp->if_ypos = -1;
	}
	
	needsort = 0;
	needclear = 1;
}

static
unsigned int
getifnum(void)
{
	u_int	data    = 0;
	size_t	datalen = 0;
	static	int name[] = { CTL_NET,
			       PF_LINK,
			       NETLINK_GENERIC,
			       IFMIB_SYSTEM,
			       IFMIB_IFCOUNT };

	datalen = sizeof(data);
	if (sysctl(name, 5, (void *)&data, (size_t *)&datalen, (void *)NULL,
	    (size_t)0) != 0)
		IFSTAT_ERR(1, "sysctl error");
	return (data);
}

static int
getifmibdata(int row, struct ifmibdata *data)
{
	int	ret = 0;
	size_t	datalen = 0;
	static	int name[] = { CTL_NET,
			       PF_LINK,
			       NETLINK_GENERIC,
			       IFMIB_IFDATA,
			       0,
			       IFDATA_GENERAL };
	datalen = sizeof(*data);
	name[4] = row;

	ret = sysctl(name, 6, (void *)data, (size_t *)&datalen, (void *)NULL,
	    (size_t)0);
	if ((ret != 0) && (errno != ENOENT))
		IFSTAT_ERR(2, "sysctl error getting interface data");

	return (ret);
}

int
cmdifstat(const char *cmd, const char *args)
{
	int	retval = 0;

	retval = ifcmd(cmd, args);
	/* ifcmd() returns 1 on success */
	if (retval == 1) {
		if (needclear)
			clearifstat();
	}
	else if (prefix(cmd, "all")) {
		retval = 1;
		displayall = true;
	}
	return (retval);
}

static void
clearifstat(void)
{

	showifstat();
	refresh();
	werase(wnd);
	labelifstat();
	needclear = 0;
}
