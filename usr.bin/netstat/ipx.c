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
/*
static char sccsid[] = "@(#)ns.c	8.1 (Berkeley) 6/6/93";
*/
static const char rcsid[] =
	"$Id: ipx.c,v 1.4 1996/10/28 18:56:39 wollman Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>

#include <net/route.h>
#include <net/if.h>

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
#include <stdio.h>
#include <string.h>
#include "netstat.h"

struct	ipxpcb ipxpcb;
struct	spxpcb spxpcb;
struct	socket sockb;

static char *ipx_prpr __P((struct ipx_addr *));
static void ipx_erputil __P((int, int));

static	int first = 1;

/*
 * Print a summary of connections related to a Network Systems
 * protocol.  For SPX, also give state of connection.
 * Listening processes (aflag) are suppressed unless the
 * -a (all) flag is specified.
 */

void
ipxprotopr(off, name)
	u_long off;
	char *name;
{
	struct ipxpcb cb;
	register struct ipxpcb *prev, *next;
	int isspx;

	if (off == 0)
		return;
	isspx = strcmp(name, "spx") == 0;
	kread(off, (char *)&cb, sizeof (struct ipxpcb));
	ipxpcb = cb;
	prev = (struct ipxpcb *)off;
	if (ipxpcb.ipxp_next == (struct ipxpcb *)off)
		return;
	for (;ipxpcb.ipxp_next != (struct ipxpcb *)off; prev = next) {
		u_long ppcb;

		next = ipxpcb.ipxp_next;
		kread((u_long)next, (char *)&ipxpcb, sizeof (ipxpcb));
		if (ipxpcb.ipxp_prev != prev) {
			printf("???\n");
			break;
		}
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
		printf("%-5.5s %6ld %6ld ", name, sockb.so_rcv.sb_cc,
			sockb.so_snd.sb_cc);
		printf(Aflag?" %-18.18s":" %-22.22s", ipx_prpr(&ipxpcb.ipxp_laddr));
		printf(Aflag?" %-18.18s":" %-22.22s", ipx_prpr(&ipxpcb.ipxp_faddr));
		if (isspx) {
			extern char *tcpstates[];
			if (spxpcb.s_state >= TCP_NSTATES)
				printf(" %d", spxpcb.s_state);
			else
				printf(" %s", tcpstates[spxpcb.s_state]);
		}
		putchar('\n');
		prev = next;
	}
}

#define ANY(x,y,z) (printf("\t%u %s%s%s\n",x,y,plural(x),z))

/*
 * Dump SPX statistics structure.
 */
void
spx_stats(off, name)
	u_long off;
	char *name;
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
	ANY(spxstat.spxs_connattempt, "connection", " initiated");
	ANY(spxstat.spxs_accepts, "connection", " accepted");
	ANY(spxstat.spxs_connects, "connection", " established");
	ANY(spxstat.spxs_drops, "connection", " dropped");
	ANY(spxstat.spxs_conndrops, "embryonic connection", " dropped");
	ANY(spxstat.spxs_closed, "connection", " closed (includes drops)");
	ANY(spxstat.spxs_segstimed, "packet", " where we tried to get rtt");
	ANY(spxstat.spxs_rttupdated, "time", " we got rtt");
	ANY(spxstat.spxs_delack, "delayed ack", " sent");
	ANY(spxstat.spxs_timeoutdrop, "connection", " dropped in rxmt timeout");
	ANY(spxstat.spxs_rexmttimeo, "retransmit timeout", "");
	ANY(spxstat.spxs_persisttimeo, "persist timeout", "");
	ANY(spxstat.spxs_keeptimeo, "keepalive timeout", "");
	ANY(spxstat.spxs_keepprobe, "keepalive probe", " sent");
	ANY(spxstat.spxs_keepdrops, "connection", " dropped in keepalive");
	ANY(spxstat.spxs_sndtotal, "total packet", " sent");
	ANY(spxstat.spxs_sndpack, "data packet", " sent");
	ANY(spxstat.spxs_sndbyte, "data byte", " sent");
	ANY(spxstat.spxs_sndrexmitpack, "data packet", " retransmitted");
	ANY(spxstat.spxs_sndrexmitbyte, "data byte", " retransmitted");
	ANY(spxstat.spxs_sndacks, "ack-only packet", " sent");
	ANY(spxstat.spxs_sndprobe, "window probe", " sent");
	ANY(spxstat.spxs_sndurg, "packet", " sent with URG only");
	ANY(spxstat.spxs_sndwinup, "window update-only packet", " sent");
	ANY(spxstat.spxs_sndctrl, "control (SYN|FIN|RST) packet", " sent");
	ANY(spxstat.spxs_sndvoid, "request", " to send a non-existant packet");
	ANY(spxstat.spxs_rcvtotal, "total packet", " received");
	ANY(spxstat.spxs_rcvpack, "packet", " received in sequence");
	ANY(spxstat.spxs_rcvbyte, "byte", " received in sequence");
	ANY(spxstat.spxs_rcvbadsum, "packet", " received with ccksum errs");
	ANY(spxstat.spxs_rcvbadoff, "packet", " received with bad offset");
	ANY(spxstat.spxs_rcvshort, "packet", " received too short");
	ANY(spxstat.spxs_rcvduppack, "duplicate-only packet", " received");
	ANY(spxstat.spxs_rcvdupbyte, "duplicate-only byte", " received");
	ANY(spxstat.spxs_rcvpartduppack, "packet", " with some duplicate data");
	ANY(spxstat.spxs_rcvpartdupbyte, "dup. byte", " in part-dup. packet");
	ANY(spxstat.spxs_rcvoopack, "out-of-order packet", " received");
	ANY(spxstat.spxs_rcvoobyte, "out-of-order byte", " received");
	ANY(spxstat.spxs_rcvpackafterwin, "packet", " with data after window");
	ANY(spxstat.spxs_rcvbyteafterwin, "byte", " rcvd after window");
	ANY(spxstat.spxs_rcvafterclose, "packet", " rcvd after 'close'");
	ANY(spxstat.spxs_rcvwinprobe, "rcvd window probe packet", "");
	ANY(spxstat.spxs_rcvdupack, "rcvd duplicate ack", "");
	ANY(spxstat.spxs_rcvacktoomuch, "rcvd ack", " for unsent data");
	ANY(spxstat.spxs_rcvackpack, "rcvd ack packet", "");
	ANY(spxstat.spxs_rcvackbyte, "byte", " acked by rcvd acks");
	ANY(spxstat.spxs_rcvwinupd, "rcvd window update packet", "");
}

#undef ANY
#define ANY(x,y,z)  (printf("\t%u %s%s%s\n",x,y,plural(x),z))

/*
 * Dump IPX statistics structure.
 */
void
ipx_stats(off, name)
	u_long off;
	char *name;
{
	struct ipxstat ipxstat;

	if (off == 0)
		return;
	kread(off, (char *)&ipxstat, sizeof (ipxstat));
	printf("%s:\n", name);
	ANY(ipxstat.ipxs_total, "total packet", " received");
	ANY(ipxstat.ipxs_badsum, "packet", " with bad checksums");
	ANY(ipxstat.ipxs_tooshort, "packet", " smaller than advertised");
	ANY(ipxstat.ipxs_toosmall, "packet", " smaller than a header");
	ANY(ipxstat.ipxs_forward, "packet", " forwarded");
	ANY(ipxstat.ipxs_cantforward, "packet", " not forwardable");
	ANY(ipxstat.ipxs_delivered, "packet", " for this host");
	ANY(ipxstat.ipxs_localout, "packet", " sent from this host");
	ANY(ipxstat.ipxs_odropped, "packet", " dropped due to no bufs, etc.");
	ANY(ipxstat.ipxs_noroute, "packet", " discarded due to no route");
	ANY(ipxstat.ipxs_mtutoosmall, "packet", " too big");
}

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

#ifdef IPXERRORMSGS
/*
 * Dump IPX Error statistics structure.
 */
/*ARGSUSED*/
void
ipxerr_stats(off, name)
	u_long off;
	char *name;
{
	struct ipx_errstat ipx_errstat;
	register int j;
	register int histoprint = 1;
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
ipx_erputil(z, c)
	int z, c;
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

static struct sockaddr_ipx ssipx = {AF_IPX};

static
char *ipx_prpr(x)
	struct ipx_addr *x;
{
	struct sockaddr_ipx *sipx = &ssipx;

	sipx->sipx_addr = *x;
	return(ipx_print((struct sockaddr *)sipx));
}
