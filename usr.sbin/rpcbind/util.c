/*
 * $NetBSD: util.c,v 1.4 2000/08/03 00:04:30 fvdl Exp $
 * $FreeBSD$
 */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank van der Linden.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <net/if.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <sys/poll.h>
#include <rpc/rpc.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <netconfig.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "rpcbind.h"

static struct sockaddr_in *local_in4;
#ifdef INET6
static struct sockaddr_in6 *local_in6;
#endif

static int bitmaskcmp __P((void *, void *, void *, int));
#ifdef INET6
static void in6_fillscopeid __P((struct sockaddr_in6 *));
#endif

/*
 * For all bits set in "mask", compare the corresponding bits in
 * "dst" and "src", and see if they match.
 */
static int
bitmaskcmp(void *dst, void *src, void *mask, int bytelen)
{
	int i, j;
	u_int8_t *p1 = dst, *p2 = src, *netmask = mask;
	u_int8_t bitmask;

	for (i = 0; i < bytelen; i++) {
		for (j = 0; j < 8; j++) {
			bitmask = 1 << j;
			if (!(netmask[i] & bitmask))
				continue;
			if ((p1[i] & bitmask) != (p2[i] & bitmask))
				return 1;
		}
	}

	return 0;
}

/*
 * Taken from ifconfig.c
 */
#ifdef INET6
static void
in6_fillscopeid(struct sockaddr_in6 *sin6)
{
        if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
                sin6->sin6_scope_id =
                        ntohs(*(u_int16_t *)&sin6->sin6_addr.s6_addr[2]);
                sin6->sin6_addr.s6_addr[2] = sin6->sin6_addr.s6_addr[3] = 0;
        }
}
#endif

char *
addrmerge(struct netbuf *caller, char *serv_uaddr, char *clnt_uaddr,
	  char *netid)
{
	struct ifaddrs *ifap, *ifp, *bestif;
#ifdef INET6
	struct sockaddr_in6 *servsin6, *sin6mask, *clntsin6, *ifsin6, *realsin6;
	struct sockaddr_in6 *newsin6;
#endif
	struct sockaddr_in *servsin, *sinmask, *clntsin, *newsin, *ifsin;
	struct netbuf *serv_nbp, *clnt_nbp = NULL, tbuf;
	struct sockaddr *serv_sa;
	struct sockaddr *clnt_sa;
	struct sockaddr_storage ss;
	struct netconfig *nconf;
	struct sockaddr *clnt = caller->buf;
	char *ret = NULL;

#ifdef ND_DEBUG
	if (debugging)
		fprintf(stderr, "addrmerge(caller, %s, %s, %s\n", serv_uaddr,
		    clnt_uaddr, netid);
#endif
	nconf = getnetconfigent(netid);
	if (nconf == NULL)
		return NULL;

	/*
	 * Local merge, just return a duplicate.
	 */
	if (clnt_uaddr != NULL && strncmp(clnt_uaddr, "0.0.0.0.", 8) == 0)
		return strdup(clnt_uaddr);

	serv_nbp = uaddr2taddr(nconf, serv_uaddr);
	if (serv_nbp == NULL)
		return NULL;

	serv_sa = (struct sockaddr *)serv_nbp->buf;
	if (clnt_uaddr != NULL) {
		clnt_nbp = uaddr2taddr(nconf, clnt_uaddr);
		if (clnt_nbp == NULL) {
			free(serv_nbp);
			return NULL;
		}
		clnt_sa = (struct sockaddr *)clnt_nbp->buf;
		if (clnt_sa->sa_family == AF_LOCAL) {
			free(serv_nbp);
			free(clnt_nbp);
			free(clnt_sa);
			return strdup(clnt_uaddr);
		}
	} else {
		clnt_sa = (struct sockaddr *)
		    malloc(sizeof (struct sockaddr_storage));
		memcpy(clnt_sa, clnt, clnt->sa_len);
	}

	if (getifaddrs(&ifp) < 0) {
		free(serv_nbp);
		free(clnt_sa);
		if (clnt_nbp != NULL)
			free(clnt_nbp);
		return 0;
	}

	/*
	 * Loop through all interfaces. For each interface, see if the
	 * network portion of its address is equal to that of the client.
	 * If so, we have found the interface that we want to use.
	 */
	for (ifap = ifp; ifap != NULL; ifap = ifap->ifa_next) {
		if (ifap->ifa_addr->sa_family != clnt->sa_family ||
		    !(ifap->ifa_flags & IFF_UP))
			continue;

		switch (clnt->sa_family) {
		case AF_INET:
			/*
			 * realsin: address that recvfrom gave us.
			 * ifsin: address of interface being examined.
			 * clntsin: address that client want us to contact
			 *           it on
			 * servsin: local address of RPC service.
			 * sinmask: netmask of this interface
			 * newsin: initially a copy of clntsin, eventually
			 *         the merged address
			 */
			servsin = (struct sockaddr_in *)serv_sa;
			clntsin = (struct sockaddr_in *)clnt_sa;
			sinmask = (struct sockaddr_in *)ifap->ifa_netmask;
			newsin = (struct sockaddr_in *)&ss;
			ifsin = (struct sockaddr_in *)ifap->ifa_addr;
			if (!bitmaskcmp(&ifsin->sin_addr, &clntsin->sin_addr,
			    &sinmask->sin_addr, sizeof (struct in_addr))) {
				goto found;
			}
			break;
#ifdef INET6
		case AF_INET6:
			/*
			 * realsin6: address that recvfrom gave us.
			 * ifsin6: address of interface being examined.
			 * clntsin6: address that client want us to contact
			 *           it on
			 * servsin6: local address of RPC service.
			 * sin6mask: netmask of this interface
			 * newsin6: initially a copy of clntsin, eventually
			 *          the merged address
			 *
			 * For v6 link local addresses, if the client contacted
			 * us via a link-local address, and wants us to reply
			 * to one, use the scope id to see which one.
			 */
			realsin6 = (struct sockaddr_in6 *)clnt;
			ifsin6 = (struct sockaddr_in6 *)ifap->ifa_addr;
			in6_fillscopeid(ifsin6);
			clntsin6 = (struct sockaddr_in6 *)clnt_sa;
			servsin6 = (struct sockaddr_in6 *)serv_sa;
			sin6mask = (struct sockaddr_in6 *)ifap->ifa_netmask;
			newsin6 = (struct sockaddr_in6 *)&ss;
			if (IN6_IS_ADDR_LINKLOCAL(&ifsin6->sin6_addr) &&
			    IN6_IS_ADDR_LINKLOCAL(&realsin6->sin6_addr) &&
			    IN6_IS_ADDR_LINKLOCAL(&clntsin6->sin6_addr)) {
				if (ifsin6->sin6_scope_id !=
				    realsin6->sin6_scope_id)
					continue;
				goto found;
			}
			if (!bitmaskcmp(&ifsin6->sin6_addr,
			    &clntsin6->sin6_addr, &sin6mask->sin6_addr,
			    sizeof (struct in6_addr)))
				goto found;
			break;
#endif
		default:
			goto freeit;
		}
	}
	/*
	 * Didn't find anything. Get the first possibly useful interface,
	 * preferring "normal" interfaces to point-to-point and loopback
	 * ones.
	 */
	bestif = NULL;
	for (ifap = ifp; ifap != NULL; ifap = ifap->ifa_next) {
		if (ifap->ifa_addr->sa_family != clnt->sa_family ||
		    !(ifap->ifa_flags & IFF_UP))
			continue;
		if (!(ifap->ifa_flags & IFF_LOOPBACK) &&
		    !(ifap->ifa_flags & IFF_POINTOPOINT)) {
			bestif = ifap;
			break;
		}
		if (bestif == NULL)
			bestif = ifap;
		else if ((bestif->ifa_flags & IFF_LOOPBACK) &&
		    !(ifap->ifa_flags & IFF_LOOPBACK))
			bestif = ifap;
	}
	ifap = bestif;
found:
	switch (clnt->sa_family) {
	case AF_INET:
		memcpy(newsin, ifap->ifa_addr, clnt_sa->sa_len);
		newsin->sin_port = servsin->sin_port;
		tbuf.len = clnt_sa->sa_len;
		tbuf.maxlen = sizeof (struct sockaddr_storage);
		tbuf.buf = newsin;
		break;				
#ifdef INET6
	case AF_INET6:
		memcpy(newsin6, ifsin6, clnt_sa->sa_len);
		newsin6->sin6_port = servsin6->sin6_port;
		tbuf.maxlen = sizeof (struct sockaddr_storage);
		tbuf.len = clnt_sa->sa_len;
		tbuf.buf = newsin6;
		break;
#endif
	default:
		goto freeit;
	}
	if (ifap != NULL)
		ret = taddr2uaddr(nconf, &tbuf);
freeit:
	freenetconfigent(nconf);
	free(serv_sa);
	free(serv_nbp);
	if (clnt_sa != NULL)
		free(clnt_sa);
	if (clnt_nbp != NULL)
		free(clnt_nbp);
	freeifaddrs(ifp);

#ifdef ND_DEBUG
	if (debugging)
		fprintf(stderr, "addrmerge: returning %s\n", ret);
#endif
	return ret;
}

void
network_init()
{
#ifdef INET6
	struct ifaddrs *ifap, *ifp;
	struct ipv6_mreq mreq6;
	int ifindex, s;
#endif
	int ecode;
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	if ((ecode = getaddrinfo(NULL, "sunrpc", &hints, &res))) {
		if (debugging)
			fprintf(stderr, "can't get local ip4 address: %s\n",
			    gai_strerror(ecode));
	} else {
		local_in4 = (struct sockaddr_in *)malloc(sizeof *local_in4);
		if (local_in4 == NULL) {
			if (debugging)
				fprintf(stderr, "can't alloc local ip4 addr\n");
		}
		memcpy(local_in4, res->ai_addr, sizeof *local_in4);
	}

#ifdef INET6
	hints.ai_family = AF_INET6;
	if ((ecode = getaddrinfo(NULL, "sunrpc", &hints, &res))) {
		if (debugging)
			fprintf(stderr, "can't get local ip6 address: %s\n",
			    gai_strerror(ecode));
	} else {
		local_in6 = (struct sockaddr_in6 *)malloc(sizeof *local_in6);
		if (local_in6 == NULL) {
			if (debugging)
				fprintf(stderr, "can't alloc local ip6 addr\n");
		}
		memcpy(local_in6, res->ai_addr, sizeof *local_in6);
	}

	/*
	 * Now join the RPC ipv6 multicast group on all interfaces.
	 */
	if (getifaddrs(&ifp) < 0)
		return;

	mreq6.ipv6mr_interface = 0;
	inet_pton(AF_INET6, RPCB_MULTICAST_ADDR, &mreq6.ipv6mr_multiaddr);

	s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

	/*
	 * Loop through all interfaces. For each interface, see if the
	 * network portion of its address is equal to that of the client.
	 * If so, we have found the interface that we want to use.
	 */
	for (ifap = ifp; ifap != NULL; ifap = ifap->ifa_next) {
		if (ifap->ifa_addr->sa_family != AF_INET6 ||
		    !(ifap->ifa_flags & IFF_MULTICAST))
			continue;
		ifindex = if_nametoindex(ifap->ifa_name);
		if (ifindex == mreq6.ipv6mr_interface)
			/*
			 * Already did this one.
			 */
			continue;
		mreq6.ipv6mr_interface = ifindex;
		if (setsockopt(s, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq6,
		    sizeof mreq6) < 0)
			if (debugging)
				perror("setsockopt v6 multicast");
	}
#endif

	/* close(s); */
}

struct sockaddr *
local_sa(int af)
{
	switch (af) {
	case AF_INET:
		return (struct sockaddr *)local_in4;
#ifdef INET6
	case AF_INET6:
		return (struct sockaddr *)local_in6;
#endif
	default:
		return NULL;
	}
}
