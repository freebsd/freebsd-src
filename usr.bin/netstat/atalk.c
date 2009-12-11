/*-
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

#if 0
#ifndef lint
static char sccsid[] = "@(#)atalk.c	1.1 (Whistle) 6/6/96";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>

#include <arpa/inet.h>
#include <net/route.h>

#include <netatalk/at.h>
#include <netatalk/ddp_var.h>

#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "netstat.h"

struct	ddpcb ddpcb;
struct	socket sockb;

static	int first = 1;

/*
 * Print a summary of connections related to a Network Systems
 * protocol.  For XXX, also give state of connection.
 * Listening processes (aflag) are suppressed unless the
 * -a (all) flag is specified.
 */

static const char *
at_pr_net(struct sockaddr_at *sat, int numeric)
{
static	char mybuf[50];

	if (!numeric) {
		switch(sat->sat_addr.s_net) {
		case 0xffff:
			return "????";
		case ATADDR_ANYNET:
			return("*");
		}
	}
	sprintf(mybuf,"%hu",ntohs(sat->sat_addr.s_net));
	return mybuf;
}

static const char *
at_pr_host(struct sockaddr_at *sat, int numeric)
{
static	char mybuf[50];

	if (!numeric) {
		switch(sat->sat_addr.s_node) {
		case ATADDR_BCAST:
			return "bcast";
		case ATADDR_ANYNODE:
			return("*");
		}
	}
	sprintf(mybuf,"%d",(unsigned int)sat->sat_addr.s_node);
	return mybuf;
}

static const char *
at_pr_port(struct sockaddr_at *sat)
{
static	char mybuf[50];
	struct servent *serv;

	switch(sat->sat_port) {
	case ATADDR_ANYPORT:
		return("*");
	case 0xff:
		return "????";
	default:
		if (numeric_port) {
			(void)snprintf(mybuf, sizeof(mybuf), "%d",
			    (unsigned int)sat->sat_port);
		} else {
			serv = getservbyport(sat->sat_port, "ddp");
			if (serv == NULL)
				(void)snprintf(mybuf, sizeof(mybuf), "%d",
				    (unsigned int) sat->sat_port);
			else
				(void) snprintf(mybuf, sizeof(mybuf), "%s",
				    serv->s_name);
		}
	}
	return mybuf;
}

static char *
at_pr_range(struct sockaddr_at *sat)
{
static	char mybuf[50];

	if(sat->sat_range.r_netrange.nr_firstnet
           != sat->sat_range.r_netrange.nr_lastnet) {
		sprintf(mybuf,"%d-%d",
			ntohs(sat->sat_range.r_netrange.nr_firstnet),
			ntohs(sat->sat_range.r_netrange.nr_lastnet));
	} else {
		sprintf(mybuf,"%d",
			ntohs(sat->sat_range.r_netrange.nr_firstnet));
	}
	return mybuf;
}


/* what == 0 for addr only == 3 */
/*         1 for net */
/*         2 for host */
/*         4 for port */
/*         8 for numeric only */
char *
atalk_print(struct sockaddr *sa, int what)
{
	struct sockaddr_at *sat = (struct sockaddr_at *)sa;
	static	char mybuf[50];
	int numeric = (what & 0x08);

	mybuf[0] = 0;
	switch (what & 0x13) {
	case 0:
		mybuf[0] = 0;
		break;
	case 1:
		sprintf(mybuf,"%s",at_pr_net(sat, numeric));
		break;
	case 2:
		sprintf(mybuf,"%s",at_pr_host(sat, numeric));
		break;
	case 3:
		sprintf(mybuf,"%s.%s",
				at_pr_net(sat, numeric),
				at_pr_host(sat, numeric));
		break;
	case 0x10:
		sprintf(mybuf,"%s", at_pr_range(sat));
	}
	if (what & 4) {
		sprintf(mybuf+strlen(mybuf),".%s",at_pr_port(sat));
	}
	return mybuf;
}

char *
atalk_print2(struct sockaddr *sa, struct sockaddr *mask, int what)
{
  int n;
  static char buf[100];
  struct sockaddr_at *sat1, *sat2;
  struct sockaddr_at thesockaddr;
  struct sockaddr *sa2;

  sat1 = (struct sockaddr_at *)sa;
  sat2 = (struct sockaddr_at *)mask;
  sa2 = (struct sockaddr *)&thesockaddr;

  thesockaddr.sat_addr.s_net = sat1->sat_addr.s_net & sat2->sat_addr.s_net;
  snprintf(buf, sizeof(buf), "%s", atalk_print(sa2, 1 |(what & 8)));
  if(sat2->sat_addr.s_net != 0xFFFF) {
    thesockaddr.sat_addr.s_net = sat1->sat_addr.s_net | ~sat2->sat_addr.s_net;
    n = strlen(buf);
    snprintf(buf + n, sizeof(buf) - n, "-%s", atalk_print(sa2, 1 |(what & 8)));
  }
  if(what & 2) {
    n = strlen(buf);
    snprintf(buf + n, sizeof(buf) - n, ".%s", atalk_print(sa, what & (~1)));
  }
  return(buf);
}

void
atalkprotopr(u_long off __unused, const char *name, int af1 __unused,
    int proto __unused)
{
	struct ddpcb *this, *next;

	if (off == 0)
		return;
	kread(off, (char *)&this, sizeof (struct ddpcb *));
	for ( ; this != NULL; this = next) {
		kread((u_long)this, (char *)&ddpcb, sizeof (ddpcb));
		next = ddpcb.ddp_next;
#if 0
		if (!aflag && atalk_nullhost(ddpcb.ddp_lsat) ) {
			continue;
		}
#endif
		kread((u_long)ddpcb.ddp_socket, (char *)&sockb, sizeof (sockb));
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
			printf("%8lx ", (u_long) this);
		printf("%-5.5s %6u %6u ", name, sockb.so_rcv.sb_cc,
			sockb.so_snd.sb_cc);
		printf(Aflag?" %-18.18s":" %-22.22s", atalk_print(
					(struct sockaddr *)&ddpcb.ddp_lsat,7));
		printf(Aflag?" %-18.18s":" %-22.22s", atalk_print(
					(struct sockaddr *)&ddpcb.ddp_fsat,7));
		putchar('\n');
	}
}

#define	ANY(x,y,z) if (x || sflag <= 1) \
	printf("\t%lu %s%s%s\n",x,y,plural(x),z)

/*
 * Dump DDP statistics structure.
 */
void
ddp_stats(u_long off __unused, const char *name, int af1 __unused,
    int proto __unused)
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
