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
/* From:
static char sccsid[] = "@(#)mbufs.c	8.1 (Berkeley) 6/6/93";
static const char rcsid[] =
	"Id: mbufs.c,v 1.5 1997/02/24 20:59:03 wollman Exp";
*/
static const char rcsid[] =
  "$FreeBSD: src/usr.bin/systat/icmp.c,v 1.2 1999/08/28 01:06:01 peter Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>

#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include "systat.h"
#include "extern.h"
#include "mode.h"

static struct icmpstat icmpstat, initstat, oldstat;

/*-
--0         1         2         3         4         5         6         7
--0123456789012345678901234567890123456789012345678901234567890123456789012345
01          ICMP Input                         ICMP Output
02999999999 total messages           999999999 total messages
03999999999 with bad code            999999999 errors generated
04999999999 with bad length          999999999 suppressed - original too short
05999999999 with bad checksum        999999999 suppressed - original was ICMP
06999999999 with insufficient data   999999999 responses sent
07                                   999999999 suppressed - multicast echo
08                                   999999999 suppressed - multicast tstamp
09
10          Input Histogram                    Output Histogram
11999999999 echo response            999999999 echo response
12999999999 echo request             999999999 echo request
13999999999 destination unreachable  999999999 destination unreachable
14999999999 redirect                 999999999 redirect
15999999999 time-to-live exceeded    999999999 time-to-line exceeded
16999999999 parameter problem        999999999 parameter problem
17999999999 router advertisement     999999999 router solicitation
18
19
--0123456789012345678901234567890123456789012345678901234567890123456789012345
--0         1         2         3         4         5         6         7
*/

WINDOW *
openicmp(void)
{
	return (subwin(stdscr, LINES-5-1, 0, 5, 0));
}

void
closeicmp(w)
	WINDOW *w;
{
	if (w == NULL)
		return;
	wclear(w);
	wrefresh(w);
	delwin(w);
}

void
labelicmp(void)
{
	wmove(wnd, 0, 0); wclrtoeol(wnd);
#define L(row, str) mvwprintw(wnd, row, 10, str)
#define R(row, str) mvwprintw(wnd, row, 45, str);
	L(1, "ICMP Input");		R(1, "ICMP Output");
	L(2, "total messages");		R(2, "total messages");
	L(3, "with bad code");		R(3, "errors generated");
	L(4, "with bad length");	R(4, "suppressed - original too short");
	L(5, "with bad checksum");	R(5, "suppressed - original was ICMP");
	L(6, "with insufficient data");	R(6, "responses sent");
	;				R(7, "suppressed - multicast echo");
	;				R(8, "suppressed - multicast tstamp");
	L(10, "Input Histogram");	R(10, "Output Histogram");
#define B(row, str) L(row, str); R(row, str)
	B(11, "echo response");
	B(12, "echo request");
	B(13, "destination unreachable");
	B(14, "redirect");
	B(15, "time-to-live exceeded");
	B(16, "parameter problem");
	L(17, "router advertisement");	R(17, "router solicitation");
#undef L
#undef R
#undef B
}

static void
domode(struct icmpstat *ret)
{
	const struct icmpstat *sub;
	int i, divisor = 1;

	switch(currentmode) {
	case display_RATE:
		sub = &oldstat;
		divisor = naptime;
		break;
	case display_DELTA:
		sub = &oldstat;
		break;
	case display_SINCE:
		sub = &initstat;
		break;
	default:
		*ret = icmpstat;
		return;
	}
#define DO(stat) ret->stat = (icmpstat.stat - sub->stat) / divisor
	DO(icps_error);
	DO(icps_oldshort);
	DO(icps_oldicmp);
	for (i = 0; i <= ICMP_MAXTYPE; i++) {
		DO(icps_outhist[i]);
	}
	DO(icps_badcode);
	DO(icps_tooshort);
	DO(icps_checksum);
	DO(icps_badlen);
	DO(icps_reflect);
	for (i = 0; i <= ICMP_MAXTYPE; i++) {
		DO(icps_inhist[i]);
	}
	DO(icps_bmcastecho);
	DO(icps_bmcasttstamp);
#undef DO
}
	
void
showicmp(void)
{
	struct icmpstat stats;
	u_long totalin, totalout;
	int i;

	memset(&stats, 0, sizeof stats);
	domode(&stats);
	for (i = totalin = totalout = 0; i <= ICMP_MAXTYPE; i++) {
		totalin += stats.icps_inhist[i];
		totalout += stats.icps_outhist[i];
	}
	totalin += stats.icps_badcode + stats.icps_badlen + 
		stats.icps_checksum + stats.icps_tooshort;
	mvwprintw(wnd, 2, 0, "%9lu", totalin);
	mvwprintw(wnd, 2, 35, "%9lu", totalout);

#define DO(stat, row, col) \
	mvwprintw(wnd, row, col, "%9lu", stats.stat)

	DO(icps_badcode, 3, 0);
	DO(icps_badlen, 4, 0);
	DO(icps_checksum, 5, 0);
	DO(icps_tooshort, 6, 0);
	DO(icps_error, 3, 35);
	DO(icps_oldshort, 4, 35);
	DO(icps_oldicmp, 5, 35);
	DO(icps_reflect, 6, 35);
	DO(icps_bmcastecho, 7, 35);
	DO(icps_bmcasttstamp, 8, 35);
#define DO2(type, row) DO(icps_inhist[type], row, 0); DO(icps_outhist[type], \
							 row, 35)
	DO2(ICMP_ECHOREPLY, 11);
	DO2(ICMP_ECHO, 12);
	DO2(ICMP_UNREACH, 13);
	DO2(ICMP_REDIRECT, 14);
	DO2(ICMP_TIMXCEED, 15);
	DO2(ICMP_PARAMPROB, 16);
	DO(icps_inhist[ICMP_ROUTERADVERT], 17, 0);
	DO(icps_outhist[ICMP_ROUTERSOLICIT], 17, 35);
#undef DO
#undef DO2
}

int
initicmp(void)
{
	size_t len;
	int name[4];

	name[0] = CTL_NET;
	name[1] = PF_INET;
	name[2] = IPPROTO_ICMP;
	name[3] = ICMPCTL_STATS;

	len = 0;
	if (sysctl(name, 4, 0, &len, 0, 0) < 0) {
		error("sysctl getting icmpstat size failed");
		return 0;
	}
	if (len > sizeof icmpstat) {
		error("icmpstat structure has grown--recompile systat!");
		return 0;
	}
	if (sysctl(name, 4, &initstat, &len, 0, 0) < 0) {
		error("sysctl getting icmpstat size failed");
		return 0;
	}
	oldstat = initstat;
	return 1;
}

void
reseticmp(void)
{
	size_t len;
	int name[4];

	name[0] = CTL_NET;
	name[1] = PF_INET;
	name[2] = IPPROTO_ICMP;
	name[3] = ICMPCTL_STATS;

	len = sizeof initstat;
	if (sysctl(name, 4, &initstat, &len, 0, 0) < 0) {
		error("sysctl getting icmpstat size failed");
	}
	oldstat = initstat;
}

void
fetchicmp(void)
{
	int name[4];
	size_t len;

	oldstat = icmpstat;
	name[0] = CTL_NET;
	name[1] = PF_INET;
	name[2] = IPPROTO_ICMP;
	name[3] = ICMPCTL_STATS;
	len = sizeof icmpstat;

	if (sysctl(name, 4, &icmpstat, &len, 0, 0) < 0)
		return;
}

