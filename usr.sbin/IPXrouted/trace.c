/*
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1995 John Hay.  All rights reserved.
 *
 * This file includes significant work done at Cornell University by
 * Bill Nesheim.  That work included by permission.
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
 *
 * $FreeBSD: src/usr.sbin/IPXrouted/trace.c,v 1.6.2.1 2000/07/20 10:35:22 kris Exp $
 */

#ifndef lint
static char sccsid[] = "@(#)trace.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */

/*
 * Routing Table Management Daemon
 */
#define	RIPCMDS
#define	SAPCMDS
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include "defs.h"

#define	NRECORDS	50		/* size of circular trace buffer */
#ifdef DEBUG
FILE	*ftrace = stdout;
int	tracing = 1;
#else DEBUG
FILE	*ftrace = NULL;
int	tracing = 0;
#endif

void dumpif(FILE *fd, struct interface *ifp);
void dumptrace(FILE *fd, char *dir, struct ifdebug *ifd);

void
traceinit(ifp)
	register struct interface *ifp;
{
	static int iftraceinit();

	if (iftraceinit(ifp, &ifp->int_input) &&
	    iftraceinit(ifp, &ifp->int_output))
		return;
	tracing = 0;
	syslog(LOG_ERR, "traceinit: can't init %s\n", ifp->int_name);
}

static int
iftraceinit(ifp, ifd)
	struct interface *ifp;
	register struct ifdebug *ifd;
{
	register struct iftrace *t;

	ifd->ifd_records =
	  (struct iftrace *)malloc(NRECORDS * sizeof (struct iftrace));
	if (ifd->ifd_records == 0)
		return (0);
	ifd->ifd_front = ifd->ifd_records;
	ifd->ifd_count = 0;
	for (t = ifd->ifd_records; t < ifd->ifd_records + NRECORDS; t++) {
		t->ift_size = 0;
		t->ift_packet = 0;
	}
	ifd->ifd_if = ifp;
	return (1);
}

void
traceon(file)
	char *file;
{

	if (ftrace != NULL)
		return;
	ftrace = fopen(file, "a");
	if (ftrace == NULL)
		return;
	dup2(fileno(ftrace), 1);
	dup2(fileno(ftrace), 2);
	tracing = 1;
}

void
traceoff(void)
{
	if (!tracing)
		return;
	if (ftrace != NULL)
		fclose(ftrace);
	ftrace = NULL;
	tracing = 0;
}

void
trace(ifd, who, p, len, m)
	register struct ifdebug *ifd;
	struct sockaddr *who;
	char *p;
	int len, m;
{
	register struct iftrace *t;

	if (ifd->ifd_records == 0)
		return;
	t = ifd->ifd_front++;
	if (ifd->ifd_front >= ifd->ifd_records + NRECORDS)
		ifd->ifd_front = ifd->ifd_records;
	if (ifd->ifd_count < NRECORDS)
		ifd->ifd_count++;
	if (t->ift_size > 0 && t->ift_packet)
		free(t->ift_packet);
	t->ift_packet = 0;
	t->ift_stamp = time(0);
	t->ift_who = *who;
	if (len > 0) {
		t->ift_packet = malloc(len);
		if (t->ift_packet)
			bcopy(p, t->ift_packet, len);
		else
			len = 0;
	}
	t->ift_size = len;
	t->ift_metric = m;
}

void
traceaction(fd, action, rt)
	FILE *fd;
	char *action;
	struct rt_entry *rt;
{
	struct sockaddr_ipx *dst, *gate;
	static struct bits {
		int	t_bits;
		char	*t_name;
	} flagbits[] = {
		{ RTF_UP,	"UP" },
		{ RTF_GATEWAY,	"GATEWAY" },
		{ RTF_HOST,	"HOST" },
		{ 0 }
	}, statebits[] = {
		{ RTS_PASSIVE,	"PASSIVE" },
		{ RTS_REMOTE,	"REMOTE" },
		{ RTS_INTERFACE,"INTERFACE" },
		{ RTS_CHANGED,	"CHANGED" },
		{ 0 }
	};
	register struct bits *p;
	register int first;
	char *cp;

	if (fd == NULL)
		return;
	fprintf(fd, "%s ", action);
	dst = (struct sockaddr_ipx *)&rt->rt_dst;
	gate = (struct sockaddr_ipx *)&rt->rt_router;
	fprintf(fd, "dst %s, ", ipxdp_ntoa(&dst->sipx_addr));
	fprintf(fd, "router %s, metric %d, ticks %d, flags",
	     ipxdp_ntoa(&gate->sipx_addr), rt->rt_metric, rt->rt_ticks);
	cp = " %s";
	for (first = 1, p = flagbits; p->t_bits > 0; p++) {
		if ((rt->rt_flags & p->t_bits) == 0)
			continue;
		fprintf(fd, cp, p->t_name);
		if (first) {
			cp = "|%s";
			first = 0;
		}
	}
	fprintf(fd, " state");
	cp = " %s";
	for (first = 1, p = statebits; p->t_bits > 0; p++) {
		if ((rt->rt_state & p->t_bits) == 0)
			continue;
		fprintf(fd, cp, p->t_name);
		if (first) {
			cp = "|%s";
			first = 0;
		}
	}
	putc('\n', fd);
	if (!tracepackets && (rt->rt_state & RTS_PASSIVE) == 0 && rt->rt_ifp)
		dumpif(fd, rt->rt_ifp);
	fflush(fd);
}

void
traceactionlog(action, rt)
	char *action;
	struct rt_entry *rt;
{
	struct sockaddr_ipx *dst, *gate;
	static struct bits {
		int	t_bits;
		char	*t_name;
	} flagbits[] = {
		{ RTF_UP,	"UP" },
		{ RTF_GATEWAY,	"GATEWAY" },
		{ RTF_HOST,	"HOST" },
		{ 0 }
	}, statebits[] = {
		{ RTS_PASSIVE,	"PASSIVE" },
		{ RTS_REMOTE,	"REMOTE" },
		{ RTS_INTERFACE,"INTERFACE" },
		{ RTS_CHANGED,	"CHANGED" },
		{ 0 }
	};
	register struct bits *p;
	register int first;
	char *cp;
	char *lstr, *olstr;

	dst = (struct sockaddr_ipx *)&rt->rt_dst;
	gate = (struct sockaddr_ipx *)&rt->rt_router;
	asprintf(&lstr, "%s dst %s,", action, ipxdp_ntoa(&dst->sipx_addr));
	olstr = lstr;
	asprintf(&lstr, "%s router %s, metric %d, ticks %d, flags",
	     olstr, ipxdp_ntoa(&gate->sipx_addr), rt->rt_metric, rt->rt_ticks);
	free(olstr);
	olstr = lstr;
	cp = "%s %s";
	for (first = 1, p = flagbits; p->t_bits > 0; p++) {
		if ((rt->rt_flags & p->t_bits) == 0)
			continue;
		asprintf(&lstr, cp, olstr, p->t_name);
		free(olstr);
		olstr = lstr;
		if (first) {
			cp = "%s|%s";
			first = 0;
		}
	}
	asprintf(&lstr, "%s state", olstr);
	free(olstr);
	olstr = lstr;
	cp = "%s %s";
	for (first = 1, p = statebits; p->t_bits > 0; p++) {
		if ((rt->rt_state & p->t_bits) == 0)
			continue;
		asprintf(&lstr, cp, olstr, p->t_name);
		free(olstr);
		olstr = lstr;
		if (first) {
			cp = "%s|%s";
			first = 0;
		}
	}
	syslog(LOG_DEBUG, "%s", lstr);
	free(lstr);
}

void
tracesapactionlog(action, sap)
	char *action;
	struct sap_entry *sap;
{
	syslog(LOG_DEBUG, "%-12.12s  service %04X %-20.20s "
		    "addr %s.%04X %c metric %d\n",
		     action,
		     ntohs(sap->sap.ServType),
		     sap->sap.ServName,
		     ipxdp_ntoa(&sap->sap.ipx),
		     ntohs(sap->sap.ipx.x_port),
		     (sap->clone ? 'C' : ' '),
		     ntohs(sap->sap.hops));
}

void
dumpif(fd, ifp)
	register struct interface *ifp;
	FILE *fd;
{
	if (ifp->int_input.ifd_count || ifp->int_output.ifd_count) {
		fprintf(fd, "*** Packet history for interface %s ***\n",
			ifp->int_name);
		dumptrace(fd, "to", &ifp->int_output);
		dumptrace(fd, "from", &ifp->int_input);
		fprintf(fd, "*** end packet history ***\n");
	}
}

void
dumptrace(fd, dir, ifd)
	FILE *fd;
	char *dir;
	register struct ifdebug *ifd;
{
	register struct iftrace *t;
	char *cp = !strcmp(dir, "to") ? "Output" : "Input";

	if (ifd->ifd_front == ifd->ifd_records &&
	    ifd->ifd_front->ift_size == 0) {
		fprintf(fd, "%s: no packets.\n", cp);
		return;
	}
	fprintf(fd, "%s trace:\n", cp);
	t = ifd->ifd_front - ifd->ifd_count;
	if (t < ifd->ifd_records)
		t += NRECORDS;
	for ( ; ifd->ifd_count; ifd->ifd_count--, t++) {
		if (t >= ifd->ifd_records + NRECORDS)
			t = ifd->ifd_records;
		if (t->ift_size == 0)
			continue;
		fprintf(fd, "%.24s: metric=%d\n", ctime(&t->ift_stamp),
			t->ift_metric);
		dumppacket(fd, dir, &t->ift_who, t->ift_packet, t->ift_size);
	}
}

void
dumppacket(fd, dir, source, cp, size)
	FILE *fd;
	char *dir;
	struct sockaddr *source;
	char *cp;
	register int size;
{
	register struct rip *msg = (struct rip *)cp;
	register struct netinfo *n;
	struct sockaddr_ipx *who = (struct sockaddr_ipx *)source;

	if (msg->rip_cmd && ntohs(msg->rip_cmd) < RIPCMD_MAX)
		fprintf(fd, "%s %s %s#%x", ripcmds[ntohs(msg->rip_cmd)],
		    dir, ipxdp_ntoa(&who->sipx_addr), 
		    ntohs(who->sipx_addr.x_port));
	else {
		fprintf(fd, "Bad cmd 0x%x %s %s#%x\n", ntohs(msg->rip_cmd),
		    dir, ipxdp_ntoa(&who->sipx_addr), 
		    ntohs(who->sipx_addr.x_port));
		fprintf(fd, "size=%d cp=%x packet=%x\n", size, 
			(u_int)cp, (u_int)packet);
		return;
	}
	switch (ntohs(msg->rip_cmd)) {

	case RIPCMD_REQUEST:
	case RIPCMD_RESPONSE:
		fprintf(fd, ":\n");
		size -= sizeof (u_short);
		n = msg->rip_nets;
		for (; size > 0; n++, size -= sizeof (struct netinfo)) {
			if (size < sizeof (struct netinfo))
				break;
			fprintf(fd, "\tnet %s metric %d ticks %d\n",
			     ipxdp_nettoa(n->rip_dst),
			     ntohs(n->rip_metric),
			     ntohs(n->rip_ticks));
		}
		break;

	}
}

void
dumpsappacket(fd, dir, source, cp, size)
	FILE *fd;
	char *dir;
	struct sockaddr *source;
	char *cp;
	register int size;
{
	register struct sap_packet *msg = (struct sap_packet *)cp;
	register struct sap_info *n;
	struct sockaddr_ipx *who = (struct sockaddr_ipx *)source;

	if (msg->sap_cmd && ntohs(msg->sap_cmd) < SAPCMD_MAX)
		fprintf(fd, "%s %s %s#%x", sapcmds[ntohs(msg->sap_cmd)],
		    dir, ipxdp_ntoa(&who->sipx_addr), 
		    ntohs(who->sipx_addr.x_port));
	else {
		fprintf(fd, "Bad cmd 0x%x %s %s#%x\n", ntohs(msg->sap_cmd),
		    dir, ipxdp_ntoa(&who->sipx_addr), 
		    ntohs(who->sipx_addr.x_port));
		fprintf(fd, "size=%d cp=%x packet=%x\n", size, 
			(u_int)cp, (u_int)packet);
		return;
	}
	switch (ntohs(msg->sap_cmd)) {

	case SAP_REQ:
	case SAP_RESP:
	case SAP_REQ_NEAR:
	case SAP_RESP_NEAR:
		fprintf(fd, ":\n");
		size -= sizeof (u_short);
		n = msg->sap;
		for (; size > 0; n++, size -= sizeof (struct sap_info)) {
			if (size < sizeof (struct sap_info))
				break;
			fprintf(fd, "  service %04X %-20.20s "
				    "addr %s.%04X metric %d\n",
			     ntohs(n->ServType),
			     n->ServName,
			     ipxdp_ntoa(&n->ipx),
			     ntohs(n->ipx.x_port),
			     ntohs(n->hops));
		}
		break;

	}
}

void
dumpsaptable(fd, sh)
	FILE *fd;
	struct sap_hash *sh;
{
	register struct sap_entry *sap;
	struct sap_hash *hash;
	int x = 0;

	fprintf(fd, "------- SAP table dump. -------\n");
	for (hash = sh; hash < &sh[SAPHASHSIZ]; hash++, x++) {
		fprintf(fd, "HASH %d\n", x);
		sap = hash->forw;
		for (; sap != (struct sap_entry *)hash; sap = sap->forw) {
			fprintf(fd, "  service %04X %-20.20s "
				    "addr %s.%04X %c metric %d\n",
				     ntohs(sap->sap.ServType),
				     sap->sap.ServName,
				     ipxdp_ntoa(&sap->sap.ipx),
				     ntohs(sap->sap.ipx.x_port),
				     (sap->clone ? 'C' : ' '),
				     ntohs(sap->sap.hops));
		}
	}
	fprintf(fd, "\n");
}

void
dumpriptable(fd)
	FILE *fd;
{
	register struct rt_entry *rip;
	struct rthash *hash;
	int x;
	struct rthash *rh = nethash;

	fprintf(fd, "------- RIP table dump. -------\n");
	x = 0;
	fprintf(fd, "Network table.\n");

	for (hash = rh; hash < &rh[ROUTEHASHSIZ]; hash++, x++) {
		fprintf(fd, "HASH %d\n", x);
		rip = hash->rt_forw;
		for (; rip != (struct rt_entry *)hash; rip = rip->rt_forw) {
			fprintf(fd, "  dest %s\t", 
				ipxdp_ntoa(&satoipx_addr(rip->rt_dst)));
			fprintf(fd, "%s metric %d, ticks %d\n",
				ipxdp_ntoa(&satoipx_addr(rip->rt_router)),
				rip->rt_metric,
				rip->rt_ticks);
		}
	}
	fprintf(fd, "\n");
}

union ipx_net_u net;

char *
ipxdp_nettoa(val)
union ipx_net val;
{
	static char buf[100];
	net.net_e = val;
	(void)sprintf(buf, "%lx", ntohl(net.long_e));
	return (buf);
}


char *
ipxdp_ntoa(addr)
struct ipx_addr *addr;
{
    static char buf[100];

    (void)sprintf(buf, "%s#%x:%x:%x:%x:%x:%x",
	ipxdp_nettoa(addr->x_net),
	addr->x_host.c_host[0], addr->x_host.c_host[1], 
	addr->x_host.c_host[2], addr->x_host.c_host[3], 
	addr->x_host.c_host[4], addr->x_host.c_host[5]);
	
    return(buf);
}
