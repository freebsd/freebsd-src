/*
 * Copyright (c) 1983, 1988, 1993
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
static char sccsid[] = "@(#)atalk.c	1.1 (Whistle) 6/6/96";
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>

#include <net/route.h>
#include <net/if.h>

#include <netinet/tcp_fsm.h>

#include <netatalk/at.h>
#include <netatalk/ddp_var.h>

#include <nlist.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "netstat.h"

struct	ddpcb ddpcb;
struct	socket sockb;

static void atalk_erputil __P((int, int));

static	int first = 1;

/*
 * Print a summary of connections related to a Network Systems
 * protocol.  For XXX, also give state of connection.
 * Listening processes (aflag) are suppressed unless the
 * -a (all) flag is specified.
 */

void
atalkprotopr(off, name)
	u_long off;
	char *name;
{
	struct ddpcb cb;
	register struct ddpcb *prev, *next;
	struct ddpcb *initial;

	if (off == 0)
		return;
	kread(off, (char *)&initial, sizeof (struct ddpcb *));
	ddpcb = cb;
	prev = (struct ddpcb *)off;
	for (next = initial ;next != NULL; prev = next) {
		u_long ppcb;

		kread((u_long)next, (char *)&ddpcb, sizeof (ddpcb));
		next = ddpcb.ddp_next;
#if 0
		if (!aflag && atalk_nullhost(ddpcb.ddp_lsat) ) {
			continue;
		}
#endif
		kread((u_long)ddpcb.ddp_socket,
				(char *)&sockb, sizeof (sockb));
		if (first) {
			printf("Active ATALK connections");
			if (aflag)
				printf(" (including servers)");
			putchar('\n');
			if (Aflag)
				printf("%-8.8s ", "PCB");
			printf(Aflag ?
				"%-5.5s %-6.6s %-6.6s  %-18.18s %-18.18s %s\n" :
				"%-5.5s %-6.6s %-6.6s  %-22.22s %-22.22s %s\n",
				"Proto", "Recv-Q", "Send-Q",
				"Local Address", "Foreign Address", "(state)");
			first = 0;
		}
		if (Aflag)
			printf("%8x ", ppcb);
		printf("%-5.5s %6d %6d ", name, sockb.so_rcv.sb_cc,
			sockb.so_snd.sb_cc);
		printf(Aflag?" %-18.18s":" %-22.22s", atalk_print(
					(struct sockaddr *)&ddpcb.ddp_lsat));
		printf(Aflag?" %-18.18s":" %-22.22s", atalk_print(
					(struct sockaddr *)&ddpcb.ddp_fsat));
		putchar('\n');
	}
}
#define ANY(x,y,z) \
	((x) ? printf("\t%d %s%s%s\n",x,y,plural(x),z) : 0)

/*
 * Dump DDP statistics structure.
 */
void
ddp_stats(off, name)
	u_long off;
	char *name;
{
	struct ddpstat ddpstat;

	if (off == 0)
		return;
	kread(off, (char *)&ddpstat, sizeof (ddpstat));
	printf("%s:\n", name);
	ANY(ddpstat.ddps_short, "packet", " with short headers ");
	ANY(ddpstat.ddps_long, "packet", " with long headers ");
	ANY(ddpstat.ddps_nosum, "packet", " with no checksum ");
	ANY(ddpstat.ddps_tooshort, "packet", " too short ");
	ANY(ddpstat.ddps_badsum, "packet", " with bad checksum ");
	ANY(ddpstat.ddps_toosmall, "packet", " with not enough data ");
	ANY(ddpstat.ddps_forward, "packet", " forwarded ");
	ANY(ddpstat.ddps_encap, "packet", " encapsulated ");
	ANY(ddpstat.ddps_cantforward, "packet", " rcvd for unreachable dest ");
	ANY(ddpstat.ddps_nosockspace, "packet", " dropped due to no socket space ");
}
#undef ANY


