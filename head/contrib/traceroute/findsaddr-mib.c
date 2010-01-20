/*
 * Copyright (c) 2000
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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

/* Seriously complex Solaris mib2 code */

#ifndef lint
static const char rcsid[] =
    "@(#) $Id: findsaddr-mib.c,v 1.2 2000/12/13 21:31:49 leres Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#include <sys/time.h>				/* concession to AIX */
#include <sys/stream.h>
#include <sys/tihdr.h>
#include <sys/tiuser.h>

#if __STDC__
struct mbuf;
struct rtentry;
#endif

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>

#include <inet/common.h>
#include <inet/mib2.h>
#include <inet/ip.h>
#include <inet/arp.h>

#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <unistd.h>

#include "gnuc.h"
#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#include "findsaddr.h"

/* Compatibility with older versions of Solaris */
#ifndef IRE_CACHE
#define IRE_CACHE IRE_ROUTE
#endif

#ifndef T_CURRENT
#define T_CURRENT MI_T_CURRENT
#endif

struct routelist {
	struct routelist *next;
	u_int32_t dest;
	u_int32_t mask;
	u_int32_t gate;
	char ifname[64];
};

/* Forwards */
static struct routelist *getroutelist(char *);
static void freeroutelist(struct routelist *);

/*
 * Return the source address for the given destination address
 *
 * Since solaris doesn't report the interface associated with every
 * route, we have to make two passes over the routing table.  The
 * first pass should yield a host, net, default or interface route.
 * If we find an interface route we're done. If not, we need to
 * make a second pass to find the interface route for the gateway
 * in the host, net, default route we found in the first pass.
 *
 * So instead of making a single pass through the tables as they
 * are retrieved from the kernel, we must build a linked list...
 */
const char *
findsaddr(register const struct sockaddr_in *to,
    register struct sockaddr_in *from)
{
	register struct routelist *rl, *rl2, *routelist;
	static char errbuf[512];
	u_int32_t mask, gate;

	/* Get the routing table */
	routelist = getroutelist(errbuf);
	if (routelist == NULL)
		return (errbuf);

	/* First pass; look for a route that matches */
	mask = 0;
	rl2 = NULL;
	for (rl = routelist; rl != NULL; rl = rl->next) {
		if ((to->sin_addr.s_addr & rl->mask) == rl->dest &&
		    (rl->mask > mask || mask == 0) &&
		    rl->gate != 0) {
			mask = rl->mask;
			rl2 = rl;
		}
	}
	if (rl2 == NULL) {
		freeroutelist(routelist);
		sprintf(errbuf, "%s: %.128s",
		    inet_ntoa(to->sin_addr), strerror(EHOSTUNREACH));
		return (errbuf);
	}

	/* We're done if we got one with an interface */
	if (rl2->ifname[0] != '\0') {
		freeroutelist(routelist);
		from->sin_addr.s_addr = rl2->gate;
		return (NULL);
	}

	/* First pass; look for a route that matches the gateway we found */
	mask = 0;
	gate = rl2->gate;
	rl2 = NULL;
	for (rl = routelist; rl != NULL; rl = rl->next) {
		if ((gate & rl->mask) == rl->dest &&
		    (rl->mask > mask || mask == 0) &&
		    rl->gate != 0 &&
		    rl->ifname[0] != '\0') {
			mask = rl->mask;
			rl2 = rl;
		}
	}
	if (rl2 == NULL) {
		freeroutelist(routelist);
		sprintf(errbuf, "%s: %.128s (second pass)",
		    inet_ntoa(to->sin_addr), strerror(EHOSTUNREACH));
		return (errbuf);
	}

	from->sin_addr.s_addr = rl2->gate;
	freeroutelist(routelist);
	return (NULL);
}

/* Request mib */
struct mibrq {
	struct T_optmgmt_req req;
	struct opthdr hdr;
};

/* Reply mib */
struct mibrep {
	struct T_optmgmt_ack ack;
	struct opthdr hdr;
	char buf[512];
};

static struct mibrq mibrq = {
	{ T_OPTMGMT_REQ, sizeof(mibrq.hdr), sizeof(mibrq.req), T_CURRENT },
	{ MIB2_IP }
};

static struct mibrep mibrep = {
	{ 0, 0, 0 },
	{ 0 },
	{ 0 }
};

static struct strbuf rqbuf = {
	0, sizeof(mibrq), (char *)&mibrq
};

static struct strbuf repbuf = {
	sizeof(mibrep.buf), sizeof(mibrep.ack) + sizeof(mibrep.hdr),
	    (char *)&mibrep
};

static const char devip[] = "/dev/ip";

/*
 * Construct the list of routes
 */
static struct routelist *
getroutelist(char *errbuf)
{
	register int s, stat, i;
	register char *cp;
	register struct T_optmgmt_ack *ackp;
	register struct T_error_ack *eackp;
	register struct opthdr *hp;
	register mib2_ipRouteEntry_t *rp, *rp2;
	register struct routelist *rl, *rl2, *routelist;
	int flags;
	struct strbuf repbuf2;

	s = open(devip, O_RDWR, 0);
	if (s < 0) {
		sprintf(errbuf, "open %s: %.128s", devip, strerror(errno));
		return (NULL);
	}

	if ((cp = "arp", ioctl(s, I_PUSH, cp) < 0) ||
	    (cp = "tcp", ioctl(s, I_PUSH, cp) < 0) ||
	    (cp = "udp", ioctl(s, I_PUSH, cp) < 0)) {
		sprintf(errbuf, "I_PUSH %s: %.128s", cp, strerror(errno));
		close(s);
		return (NULL);
	}

	flags = 0;
	if (putmsg(s, &rqbuf, NULL, flags) < 0) {
		sprintf(errbuf, "putmsg: %.128s", strerror(errno));
		close(s);
		return (NULL);
	}

	routelist= NULL;
	rl2 = NULL;

	rp = NULL;

	ackp = &mibrep.ack;
	hp = &mibrep.hdr;
	eackp = (struct T_error_ack *)ackp;
	for (;;) {
		flags = 0;
		memset(repbuf.buf, 0, repbuf.len);
		stat = getmsg(s, &repbuf, NULL, &flags);
		if (stat < 0) {
			sprintf(errbuf, "getmsg: %.128s", strerror(errno));
			goto bail;
		}
		if (stat == 0 && repbuf.len >= sizeof(*ackp) &&
		    ackp->PRIM_type == T_OPTMGMT_ACK &&
		    ackp->MGMT_flags == T_SUCCESS &&
		    hp->len == 0) {
			/* All done! */
			goto done;
		}
		if (repbuf.len >= sizeof(*eackp) &&
		    eackp->PRIM_type == T_ERROR_ACK) {
			sprintf(errbuf, "getmsg err: %.128s",
			    strerror((eackp->TLI_error == TSYSERR) ?
			    eackp->UNIX_error : EPROTO));
			goto bail;
		}
		if (stat != MOREDATA ||
		    repbuf.len < sizeof(*ackp) ||
		    ackp->PRIM_type != T_OPTMGMT_ACK ||
		    ackp->MGMT_flags != T_SUCCESS) {
			strcpy(errbuf, "unknown getmsg err");
			goto bail;
		}

		memset(&repbuf2, 0, sizeof(repbuf2));
		repbuf2.maxlen = hp->len;
		rp = malloc(hp->len);
		if (rp == NULL) {
			sprintf(errbuf, "malloc: %.128s", strerror(errno));
			goto bail;
		}
		repbuf2.buf = (char *)rp;

		flags = 0;
		memset(repbuf2.buf, 0, repbuf2.len);
		stat = getmsg(s, NULL, &repbuf2, &flags);
		if (stat < 0) {
			sprintf(errbuf, "getmsg2: %.128s", strerror(errno));
			goto bail;
		}

		/* Spin through the routes */
		rp2 = rp;
		for (rp2 = rp; (char *)rp2 < (char *)rp + repbuf2.len; ++rp2) {
			if (hp->level != MIB2_IP || hp->name != MIB2_IP_21)
				continue;

			if (rp2->ipRouteInfo.re_ire_type == IRE_CACHE ||
			    rp2->ipRouteInfo.re_ire_type == IRE_BROADCAST)
			    continue;

			/* Got one we want to keep */
			rl = malloc(sizeof(*rl));
			if (rl == NULL) {
				sprintf(errbuf,
				    "malloc 2: %.128s", strerror(errno));
				goto bail;
			}
			memset(rl, 0, sizeof(*rl));

			rl->mask = rp2->ipRouteMask;
			rl->dest = rp2->ipRouteDest;
			rl->gate = rp2->ipRouteNextHop;
			if (rp2->ipRouteIfIndex.o_length > 0) {
				i = rp2->ipRouteIfIndex.o_length;
				if (i > sizeof(rl->ifname) - 1)
					i = sizeof(rl->ifname) - 1;
				strncpy(rl->ifname,
				    rp2->ipRouteIfIndex.o_bytes, i);
				rl->ifname[i] = '\0';
			}

			/* Keep in order (just for fun) */
			if (routelist == NULL)
				routelist = rl;
			if (rl2 != NULL)
				rl2->next = rl;
			rl2 = rl;
		}
		free(rp);
		rp = NULL;
	}

	strcpy(errbuf, "failed!");

bail:
	if (routelist != NULL) {
		freeroutelist(routelist);
		routelist = NULL;
	}
done:
	if (rp != NULL)
		free(rp);
	close(s);
	return (routelist);
}

static void
freeroutelist(register struct routelist *rl)
{
	register struct routelist *rl2;

	while (rl != NULL) {
		rl2 = rl->next;
		free(rl);
		rl = rl2;
	}
}
