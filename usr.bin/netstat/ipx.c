/*
 * Copyright (c) 2004, Robert N. M. Watson
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
static char sccsid[] = "@(#)ns.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>

#include <net/route.h>

#include <netinet/tcp_fsm.h>

#include <netipx/ipx.h>
#include <netipx/ipx_pcb.h>
#include <netipx/ipx_var.h>
#ifdef IPXERRORMSGS
#include <netipx/ipx_error.h>
#endif
#include <netipx/spx.h>
#include <netipx/spx_timer.h>
#include <netipx/spx_var.h>
#define SANAMES
#include <netipx/spx_debug.h>

#include <nlist.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "netstat.h"

static char *ipx_prpr (struct ipx_addr *);

extern char *tcpstates[];

/*
 * Print a summary of connections related to a Network Systems
 * protocol.  For SPX, also give state of connection.
 * Listening processes (aflag) are suppressed unless the
 * -a (all) flag is specified.
 */

void
ipxprotopr(u_long off, const char *name, int af1 __unused, int proto __unused)
{
	struct ipxpcbhead cb;
	struct ipxpcb *ipxp;
	struct ipxpcb ipxpcb;
	struct spxpcb spxpcb;
	struct socket sockb;
	static int first = 1;
	int isspx;

	if (off == 0)
		return;

	isspx = strcmp(name, "spx") == 0;
	kread(off, (char *)&cb, sizeof (struct ipxpcbhead));
	ipxp = LIST_FIRST(&cb);
	while (ipxp != NULL) {
		u_long ppcb;

		kread((u_long)ipxp, (char *)&ipxpcb, sizeof (ipxpcb));
		ipxp = LIST_NEXT(&ipxpcb, ipxp_list);

		if (!aflag && ipx_nullhost(ipxpcb.ipxp_faddr) ) {
			continue;
		}
		kread((u_long)ipxpcb.ipxp_socket,
				(char *)&sockb, sizeof (sockb));
		ppcb = (u_long) ipxpcb.ipxp_pcb;
		if (ppcb) {
			if (isspx) {
				kread(ppcb, (char *)&spxpcb, sizeof (spxpcb));
			} else continue;
		} else
			if (isspx) continue;
		if (first) {
			printf("Active IPX connections");
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
			printf("%8lx ", ppcb);
		printf("%-5.5s %6u %6u ", name, sockb.so_rcv.sb_cc,
			sockb.so_snd.sb_cc);
		printf(Aflag?" %-18.18s":" %-22.22s", ipx_prpr(&ipxpcb.ipxp_laddr));
		printf(Aflag?" %-18.18s":" %-22.22s", ipx_prpr(&ipxpcb.ipxp_faddr));
		if (isspx) {
			if (spxpcb.s_state >= TCP_NSTATES)
				printf(" %d", spxpcb.s_state);
			else
				printf(" %s", tcpstates[spxpcb.s_state]);
		}
		putchar('\n');
	}
}

#define ANY(x,y,z) \
	    if (x || sflag <= 1) printf("\t%u %s%s%s\n", x, y, plural(x), z)
#define ANYl(x,y,z) \
	    if (x || sflag <= 1) printf("\t%lu %s%s%s\n", x, y, plural(x), z)

/*
 * Dump SPX statistics structure.
 */
void
spx_stats(u_long off, const char *name, int af1 __unused, int proto __unused)
{
	struct spx_istat spx_istat;
#define spxstat spx_istat.newstats

	if (off == 0)
		return;
	kread(off, (char *)&spx_istat, sizeof (spx_istat));
	printf("%s:\n", name);
	ANY(spx_istat.nonucn, "connection", " dropped due to no new sockets ");
	ANY(spx_istat.gonawy, "connection", " terminated due to our end dying");
	ANY(spx_istat.nonucn, "connection",
	    " dropped due to inability to connect");
	ANY(spx_istat.noconn, "connection",
	    " dropped due to inability to connect");
	ANY(spx_istat.notme, "connection",
	    " incompleted due to mismatched id's");
	ANY(spx_istat.wrncon, "connection", " dropped due to mismatched id's");
	ANY(spx_istat.bdreas, "packet", " dropped out of sequence");
	ANY(spx_istat.lstdup, "packet", " duplicating the highest packet");
	ANY(spx_istat.notyet, "packet", " refused as exceeding allocation");
	ANYl(spxstat.spxs_connattempt, "connection", " initiated");
	ANYl(spxstat.spxs_accepts, "connection", " accepted");
	ANYl(spxstat.spxs_connects, "connection", " established");
	ANYl(spxstat.spxs_drops, "connection", " dropped");
	ANYl(spxstat.spxs_conndrops, "embryonic connection", " dropped");
	ANYl(spxstat.spxs_closed, "connection", " closed (includes drops)");
	ANYl(spxstat.spxs_segstimed, "packet", " where we tried to get rtt");
	ANYl(spxstat.spxs_rttupdated, "time", " we got rtt");
	ANYl(spxstat.spxs_delack, "delayed ack", " sent");
	ANYl(spxstat.spxs_timeoutdrop, "connection",
	    " dropped in rxmt timeout");
	ANYl(spxstat.spxs_rexmttimeo, "retransmit timeout", "");
	ANYl(spxstat.spxs_persisttimeo, "persist timeout", "");
	ANYl(spxstat.spxs_keeptimeo, "keepalive timeout", "");
	ANYl(spxstat.spxs_keepprobe, "keepalive probe", " sent");
	ANYl(spxstat.spxs_keepdrops, "connection", " dropped in keepalive");
	ANYl(spxstat.spxs_sndtotal, "total packet", " sent");
	ANYl(spxstat.spxs_sndpack, "data packet", " sent");
	ANYl(spxstat.spxs_sndbyte, "data byte", " sent");
	ANYl(spxstat.spxs_sndrexmitpack, "data packet", " retransmitted");
	ANYl(spxstat.spxs_sndrexmitbyte, "data byte", " retransmitted");
	ANYl(spxstat.spxs_sndacks, "ack-only packet", " sent");
	ANYl(spxstat.spxs_sndprobe, "window probe", " sent");
	ANYl(spxstat.spxs_sndurg, "packet", " sent with URG only");
	ANYl(spxstat.spxs_sndwinup, "window update-only packet", " sent");
	ANYl(spxstat.spxs_sndctrl, "control (SYN|FIN|RST) packet", " sent");
	ANYl(spxstat.spxs_sndvoid, "request", " to send a non-existant packet");
	ANYl(spxstat.spxs_rcvtotal, "total packet", " received");
	ANYl(spxstat.spxs_rcvpack, "packet", " received in sequence");
	ANYl(spxstat.spxs_rcvbyte, "byte", " received in sequence");
	ANYl(spxstat.spxs_rcvbadsum, "packet", " received with ccksum errs");
	ANYl(spxstat.spxs_rcvbadoff, "packet", " received with bad offset");
	ANYl(spxstat.spxs_rcvshort, "packet", " received too short");
	ANYl(spxstat.spxs_rcvduppack, "duplicate-only packet", " received");
	ANYl(spxstat.spxs_rcvdupbyte, "duplicate-only byte", " received");
	ANYl(spxstat.spxs_rcvpartduppack, "packet",
	    " with some duplicate data");
	ANYl(spxstat.spxs_rcvpartdupbyte, "dup. byte", " in part-dup. packet");
	ANYl(spxstat.spxs_rcvoopack, "out-of-order packet", " received");
	ANYl(spxstat.spxs_rcvoobyte, "out-of-order byte", " received");
	ANYl(spxstat.spxs_rcvpackafterwin, "packet", " with data after window");
	ANYl(spxstat.spxs_rcvbyteafterwin, "byte", " rcvd after window");
	ANYl(spxstat.spxs_rcvafterclose, "packet", " rcvd after 'close'");
	ANYl(spxstat.spxs_rcvwinprobe, "rcvd window probe packet", "");
	ANYl(spxstat.spxs_rcvdupack, "rcvd duplicate ack", "");
	ANYl(spxstat.spxs_rcvacktoomuch, "rcvd ack", " for unsent data");
	ANYl(spxstat.spxs_rcvackpack, "rcvd ack packet", "");
	ANYl(spxstat.spxs_rcvackbyte, "byte", " acked by rcvd acks");
	ANYl(spxstat.spxs_rcvwinupd, "rcvd window update packet", "");
}

/*
 * Dump IPX statistics structure.
 */
void
ipx_stats(u_long off, const char *name, int af1 __unused, int proto __unused)
{
	struct ipxstat ipxstat;

	if (off == 0)
		return;
	kread(off, (char *)&ipxstat, sizeof (ipxstat));
	printf("%s:\n", name);
	ANYl(ipxstat.ipxs_total, "total packet", " received");
	ANYl(ipxstat.ipxs_badsum, "packet", " with bad checksums");
	ANYl(ipxstat.ipxs_tooshort, "packet", " smaller than advertised");
	ANYl(ipxstat.ipxs_toosmall, "packet", " smaller than a header");
	ANYl(ipxstat.ipxs_forward, "packet", " forwarded");
	ANYl(ipxstat.ipxs_cantforward, "packet", " not forwardable");
	ANYl(ipxstat.ipxs_delivered, "packet", " for this host");
	ANYl(ipxstat.ipxs_localout, "packet", " sent from this host");
	ANYl(ipxstat.ipxs_odropped, "packet", " dropped due to no bufs, etc.");
	ANYl(ipxstat.ipxs_noroute, "packet", " discarded due to no route");
	ANYl(ipxstat.ipxs_mtutoosmall, "packet", " too big");
}

#ifdef IPXERRORMSGS
static	struct {
	u_short code;
	char *name;
	char *where;
} ipx_errnames[] = {
	{0, "Unspecified Error", " at Destination"},
	{1, "Bad Checksum", " at Destination"},
	{2, "No Listener", " at Socket"},
	{3, "Packet", " Refused due to lack of space at Destination"},
	{01000, "Unspecified Error", " while gatewayed"},
	{01001, "Bad Checksum", " while gatewayed"},
	{01002, "Packet", " forwarded too many times"},
	{01003, "Packet", " too large to be forwarded"},
	{-1, 0, 0},
};

/*
 * Dump IPX Error statistics structure.
 */
/*ARGSUSED*/
void
ipxerr_stats(u_long off, const char *name, int af __unused, int proto __unused)
{
	struct ipx_errstat ipx_errstat;
	int j;
	int histoprint = 1;
	int z;

	if (off == 0)
		return;
	kread(off, (char *)&ipx_errstat, sizeof (ipx_errstat));
	printf("IPX error statistics:\n");
	ANY(ipx_errstat.ipx_es_error, "call", " to ipx_error");
	ANY(ipx_errstat.ipx_es_oldshort, "error",
		" ignored due to insufficient addressing");
	ANY(ipx_errstat.ipx_es_oldipx_err, "error request",
		" in response to error packets");
	ANY(ipx_errstat.ipx_es_tooshort, "error packet",
		" received incomplete");
	ANY(ipx_errstat.ipx_es_badcode, "error packet",
		" received of unknown type");
	for(j = 0; j < IPX_ERR_MAX; j ++) {
		z = ipx_errstat.ipx_es_outhist[j];
		if (z && histoprint) {
			printf("Output Error Histogram:\n");
			histoprint = 0;
		}
		ipx_erputil(z, ipx_errstat.ipx_es_codes[j]);
	}
	histoprint = 1;
	for(j = 0; j < IPX_ERR_MAX; j ++) {
		z = ipx_errstat.ipx_es_inhist[j];
		if (z && histoprint) {
			printf("Input Error Histogram:\n");
			histoprint = 0;
		}
		ipx_erputil(z, ipx_errstat.ipx_es_codes[j]);
	}
}

static void
ipx_erputil(int z, int c)
{
	int j;
	char codebuf[30];
	char *name, *where;

	for(j = 0;; j ++) {
		if ((name = ipx_errnames[j].name) == 0)
			break;
		if (ipx_errnames[j].code == c)
			break;
	}
	if (name == 0)  {
		if (c > 01000)
			where = "in transit";
		else
			where = "at destination";
		sprintf(codebuf, "Unknown IPX error code 0%o", c);
		name = codebuf;
	} else
		where =  ipx_errnames[j].where;
	ANY(z, name, where);
}
#endif /* IPXERRORMSGS */

static struct sockaddr_ipx ssipx = { .sipx_family = AF_IPX };

static
char *ipx_prpr(struct ipx_addr *x)
{
	struct sockaddr_ipx *sipx = &ssipx;

	sipx->sipx_addr = *x;
	return(ipx_print((struct sockaddr *)sipx));
}
