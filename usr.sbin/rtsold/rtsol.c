/*	$KAME: rtsol.c,v 1.11 2000/08/13 06:14:59 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>

#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>

#include <arpa/inet.h>

#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include "rtsold.h"

#define ALLROUTER "ff02::2"

static struct msghdr rcvmhdr;
static struct msghdr sndmhdr;
static struct iovec rcviov[2];
static struct iovec sndiov[2];
static struct sockaddr_in6 from;

int rssock;

static struct sockaddr_in6 sin6_allrouters = {sizeof(sin6_allrouters), AF_INET6};

int
sockopen()
{
	int on;
	struct icmp6_filter filt;
	static u_char answer[1500];
	int rcvcmsglen, sndcmsglen;
	static u_char *rcvcmsgbuf = NULL, *sndcmsgbuf = NULL;

	sndcmsglen = rcvcmsglen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
		CMSG_SPACE(sizeof(int));
	if (rcvcmsgbuf == NULL && (rcvcmsgbuf = malloc(rcvcmsglen)) == NULL) {
		warnmsg(LOG_ERR, __FUNCTION__,
			"malloc for receive msghdr failed");
		return(-1);
	}
	if (sndcmsgbuf == NULL && (sndcmsgbuf = malloc(sndcmsglen)) == NULL) { 
		warnmsg(LOG_ERR, __FUNCTION__,
			"malloc for send msghdr failed");
		return(-1);
	}
	memset(&sin6_allrouters, 0, sizeof(struct sockaddr_in6));
	if (inet_pton(AF_INET6, ALLROUTER,
		      &sin6_allrouters.sin6_addr.s6_addr) != 1) {
		warnmsg(LOG_ERR, __FUNCTION__, "inet_pton failed for %s",
		       ALLROUTER);
		return(-1);
	}

	if ((rssock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0) {
		warnmsg(LOG_ERR, __FUNCTION__, "socket: %s", strerror(errno));
		return(-1);
	}

	/* specify to tell receiving interface */
	on = 1;
#ifdef IPV6_RECVPKTINFO
	if (setsockopt(rssock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on,
		       sizeof(on)) < 0) {
		warnmsg(LOG_ERR, __FUNCTION__, "IPV6_RECVPKTINFO: %s",
		       strerror(errno));
		exit(1);
	}
#else  /* old adv. API */
	if (setsockopt(rssock, IPPROTO_IPV6, IPV6_PKTINFO, &on,
		       sizeof(on)) < 0) {
		warnmsg(LOG_ERR, __FUNCTION__, "IPV6_PKTINFO: %s",
		       strerror(errno));
		exit(1);
	}
#endif 

	on = 1;
	/* specify to tell value of hoplimit field of received IP6 hdr */
#ifdef IPV6_RECVHOPLIMIT
	if (setsockopt(rssock, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &on,
		       sizeof(on)) < 0) {
		warnmsg(LOG_ERR, __FUNCTION__, "IPV6_RECVHOPLIMIT: %s",
		       strerror(errno));
		exit(1);
	}
#else  /* old adv. API */
	if (setsockopt(rssock, IPPROTO_IPV6, IPV6_HOPLIMIT, &on,
		       sizeof(on)) < 0) {
		warnmsg(LOG_ERR, __FUNCTION__, "IPV6_HOPLIMIT: %s",
		       strerror(errno));
		exit(1);
	}
#endif 

	/* specfiy to accept only router advertisements on the socket */
	ICMP6_FILTER_SETBLOCKALL(&filt);
	ICMP6_FILTER_SETPASS(ND_ROUTER_ADVERT, &filt);
	if (setsockopt(rssock, IPPROTO_ICMPV6, ICMP6_FILTER, &filt,
		       sizeof(filt)) == -1) {
		warnmsg(LOG_ERR, __FUNCTION__, "setsockopt(ICMP6_FILTER): %s",
		       strerror(errno));
		return(-1);
	}

	/* initialize msghdr for receiving packets */
	rcviov[0].iov_base = (caddr_t)answer;
	rcviov[0].iov_len = sizeof(answer);
	rcvmhdr.msg_name = (caddr_t)&from;
	rcvmhdr.msg_namelen = sizeof(from);
	rcvmhdr.msg_iov = rcviov;
	rcvmhdr.msg_iovlen = 1;
	rcvmhdr.msg_control = (caddr_t) rcvcmsgbuf;
	rcvmhdr.msg_controllen = rcvcmsglen;

	/* initialize msghdr for sending packets */
	sndmhdr.msg_namelen = sizeof(struct sockaddr_in6);
	sndmhdr.msg_iov = sndiov;
	sndmhdr.msg_iovlen = 1;
	sndmhdr.msg_control = (caddr_t)sndcmsgbuf;
	sndmhdr.msg_controllen = sndcmsglen;

	return(rssock);
}

void
sendpacket(struct ifinfo *ifinfo)
{
	int i;
	struct cmsghdr *cm;
	struct in6_pktinfo *pi;

	sndmhdr.msg_name = (caddr_t)&sin6_allrouters;
	sndmhdr.msg_iov[0].iov_base = (caddr_t)ifinfo->rs_data;
	sndmhdr.msg_iov[0].iov_len = ifinfo->rs_datalen;

	cm = CMSG_FIRSTHDR(&sndmhdr);
	/* specify the outgoing interface */
	cm->cmsg_level = IPPROTO_IPV6;
	cm->cmsg_type = IPV6_PKTINFO;
	cm->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	pi = (struct in6_pktinfo *)CMSG_DATA(cm);
	memset(&pi->ipi6_addr, 0, sizeof(pi->ipi6_addr));	/*XXX*/
	pi->ipi6_ifindex = ifinfo->sdl->sdl_index;

	/* specify the hop limit of the packet */
	{
		int hoplimit = 255;

		cm = CMSG_NXTHDR(&sndmhdr, cm);
		cm->cmsg_level = IPPROTO_IPV6;
		cm->cmsg_type = IPV6_HOPLIMIT;
		cm->cmsg_len = CMSG_LEN(sizeof(int));
		memcpy(CMSG_DATA(cm), &hoplimit, sizeof(int));
	}

	warnmsg(LOG_DEBUG,
	       __FUNCTION__, "send RS on %s, whose state is %d",
	       ifinfo->ifname, ifinfo->state);

	i = sendmsg(rssock, &sndmhdr, 0);

	if (i < 0 || i != ifinfo->rs_datalen) {
		/*
		 * ENETDOWN is not so serious, especially when using several
		 * network cards on a mobile node. We ignore it.
		 */
		if (errno != ENETDOWN || dflag > 0)
			warnmsg(LOG_ERR, __FUNCTION__, "sendmsg on %s: %s",
				ifinfo->ifname, strerror(errno));
	}

	/* update counter */
	ifinfo->probes++;
}

void
rtsol_input(int s)
{
	int i;
	int *hlimp = NULL;
	struct icmp6_hdr *icp;
	int ifindex = 0;
	struct cmsghdr *cm;
	struct in6_pktinfo *pi = NULL;
	struct ifinfo *ifi = NULL;
	u_char ntopbuf[INET6_ADDRSTRLEN], ifnamebuf[IFNAMSIZ];

	/* get message */
	if ((i = recvmsg(s, &rcvmhdr, 0)) < 0) {
		warnmsg(LOG_ERR, __FUNCTION__, "recvmsg: %s", strerror(errno));
		return;
	}

	/* extract optional information via Advanced API */
	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(&rcvmhdr);
	     cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(&rcvmhdr, cm)) {
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_PKTINFO &&
		    cm->cmsg_len == CMSG_LEN(sizeof(struct in6_pktinfo))) {
			pi = (struct in6_pktinfo *)(CMSG_DATA(cm));
			ifindex = pi->ipi6_ifindex;
		}
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_HOPLIMIT &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			hlimp = (int *)CMSG_DATA(cm);
	}

	if (ifindex == 0) {
		warnmsg(LOG_ERR,
		       __FUNCTION__, "failed to get receiving interface");
		return;
	}
	if (hlimp == NULL) {
		warnmsg(LOG_ERR,
		       __FUNCTION__, "failed to get receiving hop limit");
		return;
	}

	if (i < sizeof(struct nd_router_advert)) {
		warnmsg(LOG_ERR,
		       __FUNCTION__, "packet size(%d) is too short", i);
		return;
	}

	icp = (struct icmp6_hdr *)rcvmhdr.msg_iov[0].iov_base;

	if (icp->icmp6_type != ND_ROUTER_ADVERT) {
		warnmsg(LOG_ERR, __FUNCTION__,
			"invalid icmp type(%d) from %s on %s", icp->icmp6_type,
		       inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
				 INET6_ADDRSTRLEN),
		       if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	if (icp->icmp6_code != 0) {
		warnmsg(LOG_ERR, __FUNCTION__,
			"invalid icmp code(%d) from %s on %s", icp->icmp6_code,
		       inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
				 INET6_ADDRSTRLEN),
		       if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	if (*hlimp != 255) {
		warnmsg(LOG_NOTICE, __FUNCTION__,
			"invalid RA with hop limit(%d) from %s on %s",
		       *hlimp,
		       inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
				 INET6_ADDRSTRLEN),
		       if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	if (pi && !IN6_IS_ADDR_LINKLOCAL(&from.sin6_addr)) {
		warnmsg(LOG_NOTICE, __FUNCTION__,
			"invalid RA with non link-local source from %s on %s",
		       inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
				 INET6_ADDRSTRLEN),
		       if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	/* xxx: more validation? */

	if ((ifi = find_ifinfo(pi->ipi6_ifindex)) == NULL) {
		warnmsg(LOG_NOTICE, __FUNCTION__,
			"received RA from %s on an unexpeced IF(%s)",
		       inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
				 INET6_ADDRSTRLEN),
		       if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	warnmsg(LOG_DEBUG, __FUNCTION__,
		"received RA from %s on %s, state is %d",
	       inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
			 INET6_ADDRSTRLEN),
	       ifi->ifname, ifi->state);

	ifi->racnt++;

	switch(ifi->state) {
	 case IFS_IDLE:		/* should be ignored */
	 case IFS_DELAY:		/* right? */
		 break;
	 case IFS_PROBE:
		 ifi->state = IFS_IDLE;
		 ifi->probes = 0;
		 rtsol_timer_update(ifi);
		 break;
	}
}
