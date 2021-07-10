/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2001 Charles Mott <cm@linktel.net>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef _KERNEL
#include <machine/stdarg.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/rwlock.h>
#include <sys/syslog.h>
#else
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <sys/socket.h>
#include <netinet/tcp.h>

#ifdef _KERNEL
#include <netinet/libalias/alias.h>
#include <netinet/libalias/alias_local.h>
#include <netinet/libalias/alias_mod.h>
#include <net/if.h>
#else
#include "alias.h"
#include "alias_local.h"
#include "alias_mod.h"
#endif

#include "alias_db.h"

static LIST_HEAD(, libalias) instancehead = LIST_HEAD_INITIALIZER(instancehead);
int LibAliasTime;

/* Kernel module definition. */
#ifdef _KERNEL
MALLOC_DEFINE(M_ALIAS, "libalias", "packet aliasing");

MODULE_VERSION(libalias, 1);

static int
alias_mod_handler(module_t mod, int type, void *data)
{
	switch (type) {
	case MOD_QUIESCE:
	case MOD_UNLOAD:
		finishoff();
	case MOD_LOAD:
		return (0);
	default:
		return (EINVAL);
	}
}

static moduledata_t alias_mod = {
       "alias", alias_mod_handler, NULL
};

DECLARE_MODULE(alias, alias_mod, SI_SUB_DRIVERS, SI_ORDER_SECOND);
#endif

SPLAY_GENERATE(splay_out, alias_link, all.out, cmp_out);
SPLAY_GENERATE(splay_in, group_in, in, cmp_in);

static struct group_in *
StartPointIn(struct libalias *la,
    struct in_addr alias_addr, u_short alias_port, int link_type,
    int create)
{
	struct group_in *grp;
	struct group_in needle = {
		.alias_addr = alias_addr,
		.alias_port = alias_port,
		.link_type = link_type
	};

	grp = SPLAY_FIND(splay_in, &la->linkSplayIn, &needle);
	if (grp != NULL || !create || (grp = malloc(sizeof(*grp))) == NULL)
		return (grp);
	grp->alias_addr = alias_addr;
	grp->alias_port = alias_port;
	grp->link_type = link_type;
	LIST_INIT(&grp->full);
	LIST_INIT(&grp->partial);
	SPLAY_INSERT(splay_in, &la->linkSplayIn, grp);
	return (grp);
}

static int
SeqDiff(u_long x, u_long y)
{
/* Return the difference between two TCP sequence numbers
 * This function is encapsulated in case there are any unusual
 * arithmetic conditions that need to be considered.
 */
	return (ntohl(y) - ntohl(x));
}

#ifdef _KERNEL
static void
AliasLog(char *str, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vsnprintf(str, LIBALIAS_BUF_SIZE, format, ap);
	va_end(ap);
}
#else
static void
AliasLog(FILE *stream, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vfprintf(stream, format, ap);
	va_end(ap);
	fflush(stream);
}
#endif

static void
ShowAliasStats(struct libalias *la)
{
	LIBALIAS_LOCK_ASSERT(la);
	/* Used for debugging */
	if (la->logDesc) {
		int tot  = la->icmpLinkCount + la->udpLinkCount +
		    (la->sctpLinkCount>>1) + /* sctp counts half associations */
		    la->tcpLinkCount + la->pptpLinkCount +
		    la->protoLinkCount + la->fragmentIdLinkCount +
		    la->fragmentPtrLinkCount;

		AliasLog(la->logDesc,
		    "icmp=%u, udp=%u, tcp=%u, sctp=%u, pptp=%u, proto=%u, frag_id=%u frag_ptr=%u / tot=%u",
		    la->icmpLinkCount,
		    la->udpLinkCount,
		    la->tcpLinkCount,
		    la->sctpLinkCount>>1, /* sctp counts half associations */
		    la->pptpLinkCount,
		    la->protoLinkCount,
		    la->fragmentIdLinkCount,
		    la->fragmentPtrLinkCount,
		    tot);
#ifndef _KERNEL
		AliasLog(la->logDesc, " (sock=%u)\n", la->sockCount);
#endif
	}
}

void SctpShowAliasStats(struct libalias *la)
{
	ShowAliasStats(la);
}

/* get random port in network byte order */
static u_short
_RandomPort(struct libalias *la) {
	u_short port;

	port = la->aliasPortLower +
	    arc4random_uniform(la->aliasPortLength);

	return ntohs(port);
}

/* GetNewPort() allocates port numbers.  Note that if a port number
   is already in use, that does not mean that it cannot be used by
   another link concurrently.  This is because GetNewPort() looks for
   unused triplets: (dest addr, dest port, alias port). */

static int
GetNewPort(struct libalias *la, struct alias_link *lnk, int alias_port_param)
{
	int i;
	int max_trials;
	u_short port;

	LIBALIAS_LOCK_ASSERT(la);
	/*
	 * Description of alias_port_param for GetNewPort().  When
	 * this parameter is zero or positive, it precisely specifies
	 * the port number.  GetNewPort() will return this number
	 * without check that it is in use.
	*
	 * The aliasing port is automatically selected by one of
	 * two methods below:
	 *
	 * When this parameter is GET_ALIAS_PORT, it indicates to get
	 * a randomly selected port number.
	 */
	if (alias_port_param >= 0 && alias_port_param < 0x10000) {
		lnk->alias_port = (u_short) alias_port_param;
		return (0);
	}
	if (alias_port_param != GET_ALIAS_PORT) {
#ifdef LIBALIAS_DEBUG
		fprintf(stderr, "PacketAlias/GetNewPort(): ");
		fprintf(stderr, "input parameter error\n");
#endif
		return (-1);
	}

	max_trials = GET_NEW_PORT_MAX_ATTEMPTS;

	/*
	 * When the PKT_ALIAS_SAME_PORTS option is chosen,
	 * the first try will be the actual source port. If
	 * this is already in use, the remainder of the
	 * trials will be random.
	 */
	port = (la->packetAliasMode & PKT_ALIAS_SAME_PORTS)
	    ? lnk->src_port
	    : _RandomPort(la);

	/* Port number search */
	for (i = 0; i < max_trials; i++, port = _RandomPort(la)) {
		struct group_in *grp;
		struct alias_link *search_result;

		grp = StartPointIn(la, lnk->alias_addr, port, lnk->link_type, 0);
		if (grp == NULL)
			break;

		LIST_FOREACH(search_result, &grp->full, all.in) {
			if (lnk->dst_addr.s_addr == search_result->dst_addr.s_addr &&
			    lnk->dst_port == search_result->dst_port)
			    break;     /* found match */
		}
		if (search_result == NULL)
			break;
	}

	if (i >= max_trials) {
#ifdef LIBALIAS_DEBUG
		fprintf(stderr, "PacketAlias/GetNewPort(): ");
		fprintf(stderr, "could not find free port\n");
#endif
		return (-1);
	}

#ifndef NO_USE_SOCKETS
	if ((la->packetAliasMode & PKT_ALIAS_USE_SOCKETS) &&
	    (lnk->flags & LINK_PARTIALLY_SPECIFIED) &&
	    ((lnk->link_type == LINK_TCP) ||
	     (lnk->link_type == LINK_UDP))) {
		if (!GetSocket(la, port, &lnk->sockfd, lnk->link_type)) {
			return (-1);
		}
	}
#endif
	lnk->alias_port = port;

	return (0);
}

#ifndef NO_USE_SOCKETS
static		u_short
GetSocket(struct libalias *la, u_short port_net, int *sockfd, int link_type)
{
	int err;
	int sock;
	struct sockaddr_in sock_addr;

	LIBALIAS_LOCK_ASSERT(la);
	if (link_type == LINK_TCP)
		sock = socket(AF_INET, SOCK_STREAM, 0);
	else if (link_type == LINK_UDP)
		sock = socket(AF_INET, SOCK_DGRAM, 0);
	else {
#ifdef LIBALIAS_DEBUG
		fprintf(stderr, "PacketAlias/GetSocket(): ");
		fprintf(stderr, "incorrect link type\n");
#endif
		return (0);
	}

	if (sock < 0) {
#ifdef LIBALIAS_DEBUG
		fprintf(stderr, "PacketAlias/GetSocket(): ");
		fprintf(stderr, "socket() error %d\n", *sockfd);
#endif
		return (0);
	}
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	sock_addr.sin_port = port_net;

	err = bind(sock,
	    (struct sockaddr *)&sock_addr,
	    sizeof(sock_addr));
	if (err == 0) {
		la->sockCount++;
		*sockfd = sock;
		return (1);
	} else {
		close(sock);
		return (0);
	}
}
#endif

/* FindNewPortGroup() returns a base port number for an available
   range of contiguous port numbers. Note that if a port number
   is already in use, that does not mean that it cannot be used by
   another link concurrently.  This is because FindNewPortGroup()
   looks for unused triplets: (dest addr, dest port, alias port). */

int
FindNewPortGroup(struct libalias *la,
    struct in_addr dst_addr,
    struct in_addr alias_addr,
    u_short src_port,
    u_short dst_port,
    u_short port_count,
    u_char proto,
    u_char align)
{
	int i, j;
	int max_trials;
	u_short port;
	int link_type;

	LIBALIAS_LOCK_ASSERT(la);
	/*
	 * Get link_type from protocol
	 */

	switch (proto) {
	case IPPROTO_UDP:
		link_type = LINK_UDP;
		break;
	case IPPROTO_TCP:
		link_type = LINK_TCP;
		break;
	default:
		return (0);
		break;
	}

	/*
	 * The aliasing port is automatically selected by one of two
	 * methods below:
	 */
	max_trials = GET_NEW_PORT_MAX_ATTEMPTS;

	if (la->packetAliasMode & PKT_ALIAS_SAME_PORTS) {
		/*
		 * When the ALIAS_SAME_PORTS option is chosen, the first
		 * try will be the actual source port. If this is already
		 * in use, the remainder of the trials will be random.
		 */
		port = src_port;

	} else {
		port = _RandomPort(la);
	}

	/* Port number search */
	for (i = 0; i < max_trials; i++, port = _RandomPort(la)) {
		struct alias_link *search_result;

		if (align)
			port &= htons(0xfffe);

		for (j = 0; j < port_count; j++) {
			u_short port_j = ntohs(port) + j;

			if ((search_result = FindLinkIn(la, dst_addr,
			    alias_addr, dst_port, htons(port_j),
			    link_type, 0)) != NULL)
				break;
		}

		/* Found a good range, return base */
		if (j == port_count)
			return (port);
	}

#ifdef LIBALIAS_DEBUG
	fprintf(stderr, "PacketAlias/FindNewPortGroup(): ");
	fprintf(stderr, "could not find free port(s)\n");
#endif

	return (0);
}

static void
CleanupAliasData(struct libalias *la, int deletePermanent)
{
	struct alias_link *lnk, *lnk_tmp;

	LIBALIAS_LOCK_ASSERT(la);

	/* permanent entries may stay */
	TAILQ_FOREACH_SAFE(lnk, &la->checkExpire, expire.list, lnk_tmp)
		DeleteLink(&lnk, deletePermanent);
}
static void
CleanupLink(struct libalias *la, struct alias_link **lnk, int deletePermanent)
{
	LIBALIAS_LOCK_ASSERT(la);

	if (lnk == NULL || *lnk == NULL)
		return;

	if (LibAliasTime - (*lnk)->timestamp > (*lnk)->expire.time) {
		DeleteLink(lnk, deletePermanent);
		if ((*lnk) == NULL)
			return;
	}

	/* move to end, swap may fail on a single entry list */
	TAILQ_REMOVE(&la->checkExpire, (*lnk), expire.list);
	TAILQ_INSERT_TAIL(&la->checkExpire, (*lnk), expire.list);
}

static struct alias_link *
UseLink(struct libalias *la, struct alias_link *lnk)
{
	CleanupLink(la, &lnk, 0);
	if (lnk != NULL)
		lnk->timestamp = LibAliasTime;
	return (lnk);
}

static void
DeleteLink(struct alias_link **plnk, int deletePermanent)
{
	struct alias_link *lnk = *plnk;
	struct libalias *la = lnk->la;

	LIBALIAS_LOCK_ASSERT(la);
	/* Don't do anything if the link is marked permanent */
	if (!deletePermanent && (lnk->flags & LINK_PERMANENT))
		return;

#ifndef NO_FW_PUNCH
	/* Delete associated firewall hole, if any */
	ClearFWHole(lnk);
#endif

	switch (lnk->link_type) {
	case LINK_PPTP:
		LIST_REMOVE(lnk, pptp.list);
		break;
	default: {
		struct group_in *grp;

		/* Free memory allocated for LSNAT server pool */
		if (lnk->server != NULL) {
			struct server *head, *curr, *next;

			head = curr = lnk->server;
			do {
				next = curr->next;
				free(curr);
			} while ((curr = next) != head);
		} else {
			/* Adjust output table pointers */
			SPLAY_REMOVE(splay_out, &la->linkSplayOut, lnk);
		}

		/* Adjust input table pointers */
		LIST_REMOVE(lnk, all.in);

		/* Remove intermediate node, if empty */
		grp = StartPointIn(la, lnk->alias_addr, lnk->alias_port, lnk->link_type, 0);
		if (grp != NULL &&
		    LIST_EMPTY(&grp->full) &&
		    LIST_EMPTY(&grp->partial)) {
			SPLAY_REMOVE(splay_in, &la->linkSplayIn, grp);
			free(grp);
		}
	}
		break;
	}

	/* remove from housekeeping */
	TAILQ_REMOVE(&la->checkExpire, lnk, expire.list);

#ifndef NO_USE_SOCKETS
	/* Close socket, if one has been allocated */
	if (lnk->sockfd != -1) {
		la->sockCount--;
		close(lnk->sockfd);
	}
#endif
	/* Link-type dependent cleanup */
	switch (lnk->link_type) {
	case LINK_ICMP:
		la->icmpLinkCount--;
		break;
	case LINK_UDP:
		la->udpLinkCount--;
		break;
	case LINK_TCP:
		la->tcpLinkCount--;
		free(lnk->data.tcp);
		break;
	case LINK_PPTP:
		la->pptpLinkCount--;
		break;
	case LINK_FRAGMENT_ID:
		la->fragmentIdLinkCount--;
		break;
	case LINK_FRAGMENT_PTR:
		la->fragmentPtrLinkCount--;
		if (lnk->data.frag_ptr != NULL)
			free(lnk->data.frag_ptr);
		break;
	case LINK_ADDR:
		break;
	default:
		la->protoLinkCount--;
		break;
	}

	/* Free memory */
	free(lnk);
	*plnk = NULL;

	/* Write statistics, if logging enabled */
	if (la->packetAliasMode & PKT_ALIAS_LOG) {
		ShowAliasStats(la);
	}
}

struct alias_link *
AddLink(struct libalias *la, struct in_addr src_addr, struct in_addr dst_addr,
    struct in_addr alias_addr, u_short src_port, u_short dst_port,
    int alias_port_param, int link_type)
{
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);

	lnk = malloc(sizeof(struct alias_link));
	if (lnk == NULL) {
#ifdef LIBALIAS_DEBUG
		fprintf(stderr, "PacketAlias/AddLink(): ");
		fprintf(stderr, "malloc() call failed.\n");
#endif
		return (NULL);
	}
	/* Basic initialization */
	lnk->la = la;
	lnk->src_addr = src_addr;
	lnk->dst_addr = dst_addr;
	lnk->alias_addr = alias_addr;
	lnk->proxy_addr.s_addr = INADDR_ANY;
	lnk->src_port = src_port;
	lnk->dst_port = dst_port;
	lnk->proxy_port = 0;
	lnk->server = NULL;
	lnk->link_type = link_type;
#ifndef NO_USE_SOCKETS
	lnk->sockfd = -1;
#endif
	lnk->flags = 0;
	lnk->pflags = 0;
	lnk->timestamp = LibAliasTime;

	/* Expiration time */
	switch (link_type) {
	case LINK_ICMP:
		lnk->expire.time = ICMP_EXPIRE_TIME;
		break;
	case LINK_UDP:
		lnk->expire.time = UDP_EXPIRE_TIME;
		break;
	case LINK_TCP:
		lnk->expire.time = TCP_EXPIRE_INITIAL;
		break;
	case LINK_FRAGMENT_ID:
		lnk->expire.time = FRAGMENT_ID_EXPIRE_TIME;
		break;
	case LINK_FRAGMENT_PTR:
		lnk->expire.time = FRAGMENT_PTR_EXPIRE_TIME;
		break;
	default:
		lnk->expire.time = PROTO_EXPIRE_TIME;
		break;
	}

	/* Determine alias flags */
	if (dst_addr.s_addr == INADDR_ANY)
		lnk->flags |= LINK_UNKNOWN_DEST_ADDR;
	if (dst_port == 0)
		lnk->flags |= LINK_UNKNOWN_DEST_PORT;

	/* Determine alias port */
	if (GetNewPort(la, lnk, alias_port_param) != 0) {
		free(lnk);
		return (NULL);
	}
	/* Link-type dependent initialization */
	switch (link_type) {
	case LINK_ICMP:
		la->icmpLinkCount++;
		break;
	case LINK_UDP:
		la->udpLinkCount++;
		break;
	case LINK_TCP: {
		struct tcp_dat *aux_tcp;
		int i;

		aux_tcp = malloc(sizeof(struct tcp_dat));
		if (aux_tcp == NULL) {
#ifdef LIBALIAS_DEBUG
			fprintf(stderr, "PacketAlias/AddLink: ");
			fprintf(stderr, " cannot allocate auxiliary TCP data\n");
#endif
			free(lnk);
			return (NULL);
		}

		la->tcpLinkCount++;
		aux_tcp->state.in = ALIAS_TCP_STATE_NOT_CONNECTED;
		aux_tcp->state.out = ALIAS_TCP_STATE_NOT_CONNECTED;
		aux_tcp->state.index = 0;
		aux_tcp->state.ack_modified = 0;
		for (i = 0; i < N_LINK_TCP_DATA; i++)
			aux_tcp->ack[i].active = 0;
		aux_tcp->fwhole = -1;
		lnk->data.tcp = aux_tcp;
	}
		break;
	case LINK_PPTP:
		la->pptpLinkCount++;
		break;
	case LINK_FRAGMENT_ID:
		la->fragmentIdLinkCount++;
		break;
	case LINK_FRAGMENT_PTR:
		la->fragmentPtrLinkCount++;
		break;
	case LINK_ADDR:
		break;
	default:
		la->protoLinkCount++;
		break;
	}

	switch (link_type) {
	case LINK_PPTP:
		LIST_INSERT_HEAD(&la->pptpList, lnk, pptp.list);
		break;
	default: {
		struct group_in *grp;

		grp = StartPointIn(la, alias_addr, lnk->alias_port, link_type, 1);
		if (grp == NULL) {
			free(lnk);
			return (NULL);
		}

		/* Set up pointers for output lookup table */
		SPLAY_INSERT(splay_out, &la->linkSplayOut, lnk);

		/* Set up pointers for input lookup table */
		if (lnk->flags & LINK_PARTIALLY_SPECIFIED)
			LIST_INSERT_HEAD(&grp->partial, lnk, all.in);
		else
			LIST_INSERT_HEAD(&grp->full, lnk, all.in);
	}
		break;
	}

	/* Include the element into the housekeeping list */
	TAILQ_INSERT_TAIL(&la->checkExpire, lnk, expire.list);

	if (la->packetAliasMode & PKT_ALIAS_LOG)
		ShowAliasStats(la);

	return (lnk);
}

/*
 * If alias_port_param is less than zero, alias port will be automatically
 * chosen. If greater than zero, equal to alias port
 */
static struct alias_link *
ReLink(struct alias_link *old_lnk,
    struct in_addr src_addr,
    struct in_addr dst_addr,
    struct in_addr alias_addr,
    u_short src_port,
    u_short dst_port,
    int alias_port_param,
    int link_type,
    int deletePermanent)
{
	struct alias_link *new_lnk;
	struct libalias *la = old_lnk->la;

	LIBALIAS_LOCK_ASSERT(la);
	new_lnk = AddLink(la, src_addr, dst_addr, alias_addr,
	    src_port, dst_port, alias_port_param,
	    link_type);
#ifndef NO_FW_PUNCH
	if (new_lnk != NULL &&
	    old_lnk->link_type == LINK_TCP &&
	    old_lnk->data.tcp->fwhole > 0) {
		PunchFWHole(new_lnk);
	}
#endif
	DeleteLink(&old_lnk, deletePermanent);
	return (new_lnk);
}

static struct alias_link *
_SearchLinkOut(struct libalias *la, struct in_addr src_addr,
    struct in_addr dst_addr,
    u_short src_port,
    u_short dst_port,
    int link_type) {
	struct alias_link *lnk;
	struct alias_link needle = {
		.src_addr = src_addr,
		.dst_addr = dst_addr,
		.src_port = src_port,
		.dst_port = dst_port,
		.link_type = link_type
	};

	lnk = SPLAY_FIND(splay_out, &la->linkSplayOut, &needle);
	return (UseLink(la, lnk));
}

static struct alias_link *
_FindLinkOut(struct libalias *la, struct in_addr src_addr,
    struct in_addr dst_addr,
    u_short src_port,
    u_short dst_port,
    int link_type,
    int replace_partial_links)
{
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);
	lnk = _SearchLinkOut(la, src_addr, dst_addr, src_port, dst_port, link_type);
	if (lnk != NULL || !replace_partial_links)
		return (lnk);

	/* Search for partially specified links. */
	if (dst_port != 0 && dst_addr.s_addr != INADDR_ANY) {
		lnk = _SearchLinkOut(la, src_addr, dst_addr, src_port, 0,
		    link_type);
		if (lnk == NULL)
			lnk = _SearchLinkOut(la, src_addr, ANY_ADDR, src_port,
			    dst_port, link_type);
	}
	if (lnk == NULL &&
	    (dst_port != 0 || dst_addr.s_addr != INADDR_ANY)) {
		lnk = _SearchLinkOut(la, src_addr, ANY_ADDR, src_port, 0,
		    link_type);
	}
	if (lnk != NULL) {
		lnk = ReLink(lnk,
		    src_addr, dst_addr, lnk->alias_addr,
		    src_port, dst_port, lnk->alias_port,
		    link_type, 0);
	}
	return (lnk);
}

static struct alias_link *
FindLinkOut(struct libalias *la, struct in_addr src_addr,
    struct in_addr dst_addr,
    u_short src_port,
    u_short dst_port,
    int link_type,
    int replace_partial_links)
{
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);
	lnk = _FindLinkOut(la, src_addr, dst_addr, src_port, dst_port,
	    link_type, replace_partial_links);

	if (lnk == NULL) {
		/*
		 * The following allows permanent links to be specified as
		 * using the default source address (i.e. device interface
		 * address) without knowing in advance what that address
		 * is.
		 */
		if (la->aliasAddress.s_addr != INADDR_ANY &&
		    src_addr.s_addr == la->aliasAddress.s_addr) {
			lnk = _FindLinkOut(la, ANY_ADDR, dst_addr, src_port, dst_port,
			    link_type, replace_partial_links);
		}
	}
	return (lnk);
}

static struct alias_link *
_FindLinkIn(struct libalias *la, struct in_addr dst_addr,
    struct in_addr alias_addr,
    u_short dst_port,
    u_short alias_port,
    int link_type,
    int replace_partial_links)
{
	int flags_in;
	struct group_in *grp;
	struct alias_link *lnk;
	struct alias_link *lnk_unknown_all;
	struct alias_link *lnk_unknown_dst_addr;
	struct alias_link *lnk_unknown_dst_port;
	struct in_addr src_addr;
	u_short src_port;

	LIBALIAS_LOCK_ASSERT(la);
	/* Initialize pointers */
	lnk_unknown_all = NULL;
	lnk_unknown_dst_addr = NULL;
	lnk_unknown_dst_port = NULL;

	/* If either the dest addr or port is unknown, the search
	 * loop will have to know about this. */
	flags_in = 0;
	if (dst_addr.s_addr == INADDR_ANY)
		flags_in |= LINK_UNKNOWN_DEST_ADDR;
	if (dst_port == 0)
		flags_in |= LINK_UNKNOWN_DEST_PORT;

	/* Search loop */
	grp = StartPointIn(la, alias_addr, alias_port, link_type, 0);
	if (grp == NULL)
		return (NULL);

	switch (flags_in) {
	case 0:
		LIST_FOREACH(lnk, &grp->full, all.in) {
			if (lnk->dst_addr.s_addr == dst_addr.s_addr &&
			    lnk->dst_port == dst_port)
				return (UseLink(la, lnk));
		}
		break;
	case LINK_UNKNOWN_DEST_PORT:
		LIST_FOREACH(lnk, &grp->full, all.in) {
			if(lnk->dst_addr.s_addr == dst_addr.s_addr) {
				lnk_unknown_dst_port = lnk;
				break;
			}
		}
		break;
	case LINK_UNKNOWN_DEST_ADDR:
		LIST_FOREACH(lnk, &grp->full, all.in) {
			if(lnk->dst_port == dst_port) {
				lnk_unknown_dst_addr = lnk;
				break;
			}
		}
		break;
	case LINK_PARTIALLY_SPECIFIED:
		lnk_unknown_all = LIST_FIRST(&grp->full);
		break;
	}

	if (lnk_unknown_dst_port == NULL) {
		LIST_FOREACH(lnk, &grp->partial, all.in) {
			int flags = (flags_in | lnk->flags) & LINK_PARTIALLY_SPECIFIED;

			if (flags == LINK_PARTIALLY_SPECIFIED &&
			    lnk_unknown_all == NULL)
				lnk_unknown_all = lnk;
			if (flags == LINK_UNKNOWN_DEST_ADDR &&
			    lnk->dst_port == dst_port &&
			    lnk_unknown_dst_addr == NULL)
				lnk_unknown_dst_addr = lnk;
			if (flags == LINK_UNKNOWN_DEST_PORT &&
			    lnk->dst_addr.s_addr == dst_addr.s_addr) {
				lnk_unknown_dst_port = lnk;
				break;
			}
		}
	}

	lnk = (lnk_unknown_dst_port != NULL) ? lnk_unknown_dst_port
	    : (lnk_unknown_dst_addr != NULL) ? lnk_unknown_dst_addr
	    : lnk_unknown_all;

	if (lnk == NULL || !replace_partial_links)
		return (lnk);

	if (lnk->server != NULL) {	/* LSNAT link */
		src_addr = lnk->server->addr;
		src_port = lnk->server->port;
		lnk->server = lnk->server->next;
	} else {
		src_addr = lnk->src_addr;
		src_port = lnk->src_port;
	}

	if (link_type == LINK_SCTP) {
		lnk->src_addr = src_addr;
		lnk->src_port = src_port;
	} else {
		lnk = ReLink(lnk,
		    src_addr, dst_addr, alias_addr,
		    src_port, dst_port, alias_port,
		    link_type, 0);
	}
	return (lnk);
}

static struct alias_link *
FindLinkIn(struct libalias *la, struct in_addr dst_addr,
    struct in_addr alias_addr,
    u_short dst_port,
    u_short alias_port,
    int link_type,
    int replace_partial_links)
{
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);
	lnk = _FindLinkIn(la, dst_addr, alias_addr, dst_port, alias_port,
	    link_type, replace_partial_links);

	if (lnk == NULL) {
		/*
		 * The following allows permanent links to be specified as
		 * using the default aliasing address (i.e. device
		 * interface address) without knowing in advance what that
		 * address is.
		 */
		if (la->aliasAddress.s_addr != INADDR_ANY &&
		    alias_addr.s_addr == la->aliasAddress.s_addr) {
			lnk = _FindLinkIn(la, dst_addr, ANY_ADDR, dst_port, alias_port,
			    link_type, replace_partial_links);
		}
	}
	return (lnk);
}

/* External routines for finding/adding links

-- "external" means outside alias_db.c, but within alias*.c --

    FindIcmpIn(), FindIcmpOut()
    FindFragmentIn1(), FindFragmentIn2()
    AddFragmentPtrLink(), FindFragmentPtr()
    FindProtoIn(), FindProtoOut()
    FindUdpTcpIn(), FindUdpTcpOut()
    AddPptp(), FindPptpOutByCallId(), FindPptpInByCallId(),
    FindPptpOutByPeerCallId(), FindPptpInByPeerCallId()
    FindOriginalAddress(), FindAliasAddress()

(prototypes in alias_local.h)
*/

struct alias_link *
FindIcmpIn(struct libalias *la, struct in_addr dst_addr,
    struct in_addr alias_addr,
    u_short id_alias,
    int create)
{
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);
	lnk = FindLinkIn(la, dst_addr, alias_addr,
	    NO_DEST_PORT, id_alias,
	    LINK_ICMP, 0);
	if (lnk == NULL && create && !(la->packetAliasMode & PKT_ALIAS_DENY_INCOMING)) {
		struct in_addr target_addr;

		target_addr = FindOriginalAddress(la, alias_addr);
		lnk = AddLink(la, target_addr, dst_addr, alias_addr,
		    id_alias, NO_DEST_PORT, id_alias,
		    LINK_ICMP);
	}
	return (lnk);
}

struct alias_link *
FindIcmpOut(struct libalias *la, struct in_addr src_addr,
    struct in_addr dst_addr,
    u_short id,
    int create)
{
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);
	lnk = FindLinkOut(la, src_addr, dst_addr,
	    id, NO_DEST_PORT,
	    LINK_ICMP, 0);
	if (lnk == NULL && create) {
		struct in_addr alias_addr;

		alias_addr = FindAliasAddress(la, src_addr);
		lnk = AddLink(la, src_addr, dst_addr, alias_addr,
		    id, NO_DEST_PORT, GET_ALIAS_ID,
		    LINK_ICMP);
	}
	return (lnk);
}

struct alias_link *
FindFragmentIn1(struct libalias *la, struct in_addr dst_addr,
    struct in_addr alias_addr,
    u_short ip_id)
{
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);
	lnk = FindLinkIn(la, dst_addr, alias_addr,
	    NO_DEST_PORT, ip_id,
	    LINK_FRAGMENT_ID, 0);

	if (lnk == NULL) {
		lnk = AddLink(la, ANY_ADDR, dst_addr, alias_addr,
		    NO_SRC_PORT, NO_DEST_PORT, ip_id,
		    LINK_FRAGMENT_ID);
	}
	return (lnk);
}

/* Doesn't add a link if one is not found. */
struct alias_link *
FindFragmentIn2(struct libalias *la, struct in_addr dst_addr,
    struct in_addr alias_addr, u_short ip_id)
{
	LIBALIAS_LOCK_ASSERT(la);
	return FindLinkIn(la, dst_addr, alias_addr,
	    NO_DEST_PORT, ip_id,
	    LINK_FRAGMENT_ID, 0);
}

struct alias_link *
AddFragmentPtrLink(struct libalias *la, struct in_addr dst_addr,
    u_short ip_id)
{
	LIBALIAS_LOCK_ASSERT(la);
	return AddLink(la, ANY_ADDR, dst_addr, ANY_ADDR,
	    NO_SRC_PORT, NO_DEST_PORT, ip_id,
	    LINK_FRAGMENT_PTR);
}

struct alias_link *
FindFragmentPtr(struct libalias *la, struct in_addr dst_addr,
    u_short ip_id)
{
	LIBALIAS_LOCK_ASSERT(la);
	return FindLinkIn(la, dst_addr, ANY_ADDR,
	    NO_DEST_PORT, ip_id,
	    LINK_FRAGMENT_PTR, 0);
}

struct alias_link *
FindProtoIn(struct libalias *la, struct in_addr dst_addr,
    struct in_addr alias_addr,
    u_char proto)
{
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);
	lnk = FindLinkIn(la, dst_addr, alias_addr,
	    NO_DEST_PORT, 0,
	    proto, 1);

	if (lnk == NULL && !(la->packetAliasMode & PKT_ALIAS_DENY_INCOMING)) {
		struct in_addr target_addr;

		target_addr = FindOriginalAddress(la, alias_addr);
		lnk = AddLink(la, target_addr, dst_addr, alias_addr,
		    NO_SRC_PORT, NO_DEST_PORT, 0,
		    proto);
	}
	return (lnk);
}

struct alias_link *
FindProtoOut(struct libalias *la, struct in_addr src_addr,
    struct in_addr dst_addr,
    u_char proto)
{
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);
	lnk = FindLinkOut(la, src_addr, dst_addr,
	    NO_SRC_PORT, NO_DEST_PORT,
	    proto, 1);

	if (lnk == NULL) {
		struct in_addr alias_addr;

		alias_addr = FindAliasAddress(la, src_addr);
		lnk = AddLink(la, src_addr, dst_addr, alias_addr,
		    NO_SRC_PORT, NO_DEST_PORT, 0,
		    proto);
	}
	return (lnk);
}

struct alias_link *
FindUdpTcpIn(struct libalias *la, struct in_addr dst_addr,
    struct in_addr alias_addr,
    u_short dst_port,
    u_short alias_port,
    u_char proto,
    int create)
{
	int link_type;
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);
	switch (proto) {
	case IPPROTO_UDP:
		link_type = LINK_UDP;
		break;
	case IPPROTO_TCP:
		link_type = LINK_TCP;
		break;
	default:
		return (NULL);
		break;
	}

	lnk = FindLinkIn(la, dst_addr, alias_addr,
	    dst_port, alias_port,
	    link_type, create);

	if (lnk == NULL && create && !(la->packetAliasMode & PKT_ALIAS_DENY_INCOMING)) {
		struct in_addr target_addr;

		target_addr = FindOriginalAddress(la, alias_addr);
		lnk = AddLink(la, target_addr, dst_addr, alias_addr,
		    alias_port, dst_port, alias_port,
		    link_type);
	}
	return (lnk);
}

struct alias_link *
FindUdpTcpOut(struct libalias *la, struct in_addr src_addr,
    struct in_addr dst_addr,
    u_short src_port,
    u_short dst_port,
    u_char proto,
    int create)
{
	int link_type;
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);
	switch (proto) {
	case IPPROTO_UDP:
		link_type = LINK_UDP;
		break;
	case IPPROTO_TCP:
		link_type = LINK_TCP;
		break;
	default:
		return (NULL);
		break;
	}

	lnk = FindLinkOut(la, src_addr, dst_addr, src_port, dst_port, link_type, create);

	if (lnk == NULL && create) {
		struct in_addr alias_addr;

		alias_addr = FindAliasAddress(la, src_addr);
		lnk = AddLink(la, src_addr, dst_addr, alias_addr,
		    src_port, dst_port, GET_ALIAS_PORT,
		    link_type);
	}
	return (lnk);
}

struct alias_link *
AddPptp(struct libalias *la, struct in_addr src_addr,
    struct in_addr dst_addr,
    struct in_addr alias_addr,
    u_int16_t src_call_id)
{
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);
	lnk = AddLink(la, src_addr, dst_addr, alias_addr,
	    src_call_id, 0, GET_ALIAS_PORT,
	    LINK_PPTP);

	return (lnk);
}

struct alias_link *
FindPptpOutByCallId(struct libalias *la, struct in_addr src_addr,
    struct in_addr dst_addr,
    u_int16_t src_call_id)
{
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);
	LIST_FOREACH(lnk, &la->pptpList, pptp.list)
		if (lnk->src_addr.s_addr == src_addr.s_addr &&
		    lnk->dst_addr.s_addr == dst_addr.s_addr &&
		    lnk->src_port == src_call_id)
			break;

	return (UseLink(la, lnk));
}

struct alias_link *
FindPptpOutByPeerCallId(struct libalias *la, struct in_addr src_addr,
    struct in_addr dst_addr,
    u_int16_t dst_call_id)
{
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);
	LIST_FOREACH(lnk, &la->pptpList, pptp.list)
		if (lnk->src_addr.s_addr == src_addr.s_addr &&
		    lnk->dst_addr.s_addr == dst_addr.s_addr &&
		    lnk->dst_port == dst_call_id)
			break;

	return (UseLink(la, lnk));
}

struct alias_link *
FindPptpInByCallId(struct libalias *la, struct in_addr dst_addr,
    struct in_addr alias_addr,
    u_int16_t dst_call_id)
{
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);

	LIST_FOREACH(lnk, &la->pptpList, pptp.list)
		if (lnk->dst_port == dst_call_id &&
		    lnk->dst_addr.s_addr == dst_addr.s_addr &&
		    lnk->alias_addr.s_addr == alias_addr.s_addr)
			break;

	return (UseLink(la, lnk));
}

struct alias_link *
FindPptpInByPeerCallId(struct libalias *la, struct in_addr dst_addr,
    struct in_addr alias_addr,
    u_int16_t alias_call_id)
{
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);
	LIST_FOREACH(lnk, &la->pptpList, pptp.list)
		if (lnk->alias_port == alias_call_id &&
		    lnk->dst_addr.s_addr == dst_addr.s_addr &&
		    lnk->alias_addr.s_addr == alias_addr.s_addr)
			break;

	return (lnk);
}

struct alias_link *
FindRtspOut(struct libalias *la, struct in_addr src_addr,
    struct in_addr dst_addr,
    u_short src_port,
    u_short alias_port,
    u_char proto)
{
	int link_type;
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);
	switch (proto) {
	case IPPROTO_UDP:
		link_type = LINK_UDP;
		break;
	case IPPROTO_TCP:
		link_type = LINK_TCP;
		break;
	default:
		return (NULL);
		break;
	}

	lnk = FindLinkOut(la, src_addr, dst_addr, src_port, 0, link_type, 1);

	if (lnk == NULL) {
		struct in_addr alias_addr;

		alias_addr = FindAliasAddress(la, src_addr);
		lnk = AddLink(la, src_addr, dst_addr, alias_addr,
		    src_port, 0, alias_port,
		    link_type);
	}
	return (lnk);
}

struct in_addr
FindOriginalAddress(struct libalias *la, struct in_addr alias_addr)
{
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);
	lnk = FindLinkIn(la, ANY_ADDR, alias_addr,
	    0, 0, LINK_ADDR, 0);
	if (lnk == NULL) {
		if (la->targetAddress.s_addr == INADDR_ANY)
			return (alias_addr);
		else if (la->targetAddress.s_addr == INADDR_NONE)
			return (la->aliasAddress.s_addr != INADDR_ANY) ?
			    la->aliasAddress : alias_addr;
		else
			return (la->targetAddress);
	} else {
		if (lnk->server != NULL) {	/* LSNAT link */
			struct in_addr src_addr;

			src_addr = lnk->server->addr;
			lnk->server = lnk->server->next;
			return (src_addr);
		} else if (lnk->src_addr.s_addr == INADDR_ANY)
			return (la->aliasAddress.s_addr != INADDR_ANY) ?
			    la->aliasAddress : alias_addr;
		else
			return (lnk->src_addr);
	}
}

struct in_addr
FindAliasAddress(struct libalias *la, struct in_addr original_addr)
{
	struct alias_link *lnk;

	LIBALIAS_LOCK_ASSERT(la);
	lnk = FindLinkOut(la, original_addr, ANY_ADDR,
	    0, 0, LINK_ADDR, 0);
	if (lnk == NULL) {
		return (la->aliasAddress.s_addr != INADDR_ANY) ?
		    la->aliasAddress : original_addr;
	} else {
		if (lnk->alias_addr.s_addr == INADDR_ANY)
			return (la->aliasAddress.s_addr != INADDR_ANY) ?
			    la->aliasAddress : original_addr;
		else
			return (lnk->alias_addr);
	}
}

/* External routines for getting or changing link data
   (external to alias_db.c, but internal to alias*.c)

    SetFragmentData(), GetFragmentData()
    SetFragmentPtr(), GetFragmentPtr()
    SetStateIn(), SetStateOut(), GetStateIn(), GetStateOut()
    GetOriginalAddress(), GetDestAddress(), GetAliasAddress()
    GetOriginalPort(), GetAliasPort()
    SetAckModified(), GetAckModified()
    GetDeltaAckIn(), GetDeltaSeqOut(), AddSeq()
    SetProtocolFlags(), GetProtocolFlags()
    SetDestCallId()
*/

void
SetFragmentAddr(struct alias_link *lnk, struct in_addr src_addr)
{
	lnk->data.frag_addr = src_addr;
}

void
GetFragmentAddr(struct alias_link *lnk, struct in_addr *src_addr)
{
	*src_addr = lnk->data.frag_addr;
}

void
SetFragmentPtr(struct alias_link *lnk, void *fptr)
{
	lnk->data.frag_ptr = fptr;
}

void
GetFragmentPtr(struct alias_link *lnk, void **fptr)
{
	*fptr = lnk->data.frag_ptr;
}

void
SetStateIn(struct alias_link *lnk, int state)
{
	/* TCP input state */
	switch (state) {
		case ALIAS_TCP_STATE_DISCONNECTED:
		if (lnk->data.tcp->state.out != ALIAS_TCP_STATE_CONNECTED)
			lnk->expire.time = TCP_EXPIRE_DEAD;
		else
			lnk->expire.time = TCP_EXPIRE_SINGLEDEAD;
		break;
	case ALIAS_TCP_STATE_CONNECTED:
		if (lnk->data.tcp->state.out == ALIAS_TCP_STATE_CONNECTED)
			lnk->expire.time = TCP_EXPIRE_CONNECTED;
		break;
	default:
#ifdef _KERNEL
		panic("libalias:SetStateIn() unknown state");
#else
		abort();
#endif
	}
	lnk->data.tcp->state.in = state;
}

void
SetStateOut(struct alias_link *lnk, int state)
{
	/* TCP output state */
	switch (state) {
		case ALIAS_TCP_STATE_DISCONNECTED:
		if (lnk->data.tcp->state.in != ALIAS_TCP_STATE_CONNECTED)
			lnk->expire.time = TCP_EXPIRE_DEAD;
		else
			lnk->expire.time = TCP_EXPIRE_SINGLEDEAD;
		break;
	case ALIAS_TCP_STATE_CONNECTED:
		if (lnk->data.tcp->state.in == ALIAS_TCP_STATE_CONNECTED)
			lnk->expire.time = TCP_EXPIRE_CONNECTED;
		break;
	default:
#ifdef _KERNEL
		panic("libalias:SetStateOut() unknown state");
#else
		abort();
#endif
	}
	lnk->data.tcp->state.out = state;
}

int
GetStateIn(struct alias_link *lnk)
{
	/* TCP input state */
	return (lnk->data.tcp->state.in);
}

int
GetStateOut(struct alias_link *lnk)
{
	/* TCP output state */
	return (lnk->data.tcp->state.out);
}

struct in_addr
GetOriginalAddress(struct alias_link *lnk)
{
	if (lnk->src_addr.s_addr == INADDR_ANY)
		return (lnk->la->aliasAddress);
	else
		return (lnk->src_addr);
}

struct in_addr
GetDestAddress(struct alias_link *lnk)
{
	return (lnk->dst_addr);
}

struct in_addr
GetAliasAddress(struct alias_link *lnk)
{
	if (lnk->alias_addr.s_addr == INADDR_ANY)
		return (lnk->la->aliasAddress);
	else
		return (lnk->alias_addr);
}

struct in_addr
GetDefaultAliasAddress(struct libalias *la)
{
	LIBALIAS_LOCK_ASSERT(la);
	return (la->aliasAddress);
}

void
SetDefaultAliasAddress(struct libalias *la, struct in_addr alias_addr)
{
	LIBALIAS_LOCK_ASSERT(la);
	la->aliasAddress = alias_addr;
}

u_short
GetOriginalPort(struct alias_link *lnk)
{
	return (lnk->src_port);
}

u_short
GetAliasPort(struct alias_link *lnk)
{
	return (lnk->alias_port);
}

#ifndef NO_FW_PUNCH
static u_short
GetDestPort(struct alias_link *lnk)
{
	return (lnk->dst_port);
}

#endif

/* Indicate that ACK numbers have been modified in a TCP connection */
void
SetAckModified(struct alias_link *lnk)
{
	lnk->data.tcp->state.ack_modified = 1;
}

struct in_addr
GetProxyAddress(struct alias_link *lnk)
{
	return (lnk->proxy_addr);
}

void
SetProxyAddress(struct alias_link *lnk, struct in_addr addr)
{
	lnk->proxy_addr = addr;
}

u_short
GetProxyPort(struct alias_link *lnk)
{
	return (lnk->proxy_port);
}

void
SetProxyPort(struct alias_link *lnk, u_short port)
{
	lnk->proxy_port = port;
}

/* See if ACK numbers have been modified */
int
GetAckModified(struct alias_link *lnk)
{
	return (lnk->data.tcp->state.ack_modified);
}

/*
 * Find out how much the ACK number has been altered for an
 * incoming TCP packet.  To do this, a circular list of ACK
 * numbers where the TCP packet size was altered is searched.
 */
// XXX ip free
int
GetDeltaAckIn(u_long ack, struct alias_link *lnk)
{
	int i, j;
	int delta, ack_diff_min;

	delta = 0;
	ack_diff_min = -1;
	i = lnk->data.tcp->state.index;
	for (j = 0; j < N_LINK_TCP_DATA; j++) {
		struct ack_data_record x;

		if (i == 0)
			i = N_LINK_TCP_DATA;
		i--;
		x = lnk->data.tcp->ack[i];
		if (x.active == 1) {
			int ack_diff;

			ack_diff = SeqDiff(x.ack_new, ack);
			if (ack_diff >= 0) {
				if (ack_diff_min >= 0) {
					if (ack_diff < ack_diff_min) {
						delta = x.delta;
						ack_diff_min = ack_diff;
					}
				} else {
					delta = x.delta;
					ack_diff_min = ack_diff;
				}
			}
		}
	}
	return (delta);
}

/*
 * Find out how much the sequence number has been altered for an
 * outgoing TCP packet.  To do this, a circular list of ACK numbers
 * where the TCP packet size was altered is searched.
 */
// XXX ip free
int
GetDeltaSeqOut(u_long seq, struct alias_link *lnk)
{
	int i, j;
	int delta, seq_diff_min;

	delta = 0;
	seq_diff_min = -1;
	i = lnk->data.tcp->state.index;
	for (j = 0; j < N_LINK_TCP_DATA; j++) {
		struct ack_data_record x;

		if (i == 0)
			i = N_LINK_TCP_DATA;
		i--;
		x = lnk->data.tcp->ack[i];
		if (x.active == 1) {
			int seq_diff;

			seq_diff = SeqDiff(x.ack_old, seq);
			if (seq_diff >= 0) {
				if (seq_diff_min >= 0) {
					if (seq_diff < seq_diff_min) {
						delta = x.delta;
						seq_diff_min = seq_diff;
					}
				} else {
					delta = x.delta;
					seq_diff_min = seq_diff;
				}
			}
		}
	}
	return (delta);
}

/*
 * When a TCP packet has been altered in length, save this
 * information in a circular list.  If enough packets have been
 * altered, then this list will begin to overwrite itself.
 */
// XXX ip free
void
AddSeq(struct alias_link *lnk, int delta, u_int ip_hl, u_short ip_len,
    u_long th_seq, u_int th_off)
{
	struct ack_data_record x;
	int hlen, tlen, dlen;
	int i;

	hlen = (ip_hl + th_off) << 2;
	tlen = ntohs(ip_len);
	dlen = tlen - hlen;

	x.ack_old = htonl(ntohl(th_seq) + dlen);
	x.ack_new = htonl(ntohl(th_seq) + dlen + delta);
	x.delta = delta;
	x.active = 1;

	i = lnk->data.tcp->state.index;
	lnk->data.tcp->ack[i] = x;

	i++;
	if (i == N_LINK_TCP_DATA)
		lnk->data.tcp->state.index = 0;
	else
		lnk->data.tcp->state.index = i;
}

void
SetExpire(struct alias_link *lnk, int expire)
{
	if (expire == 0) {
		lnk->flags &= ~LINK_PERMANENT;
		DeleteLink(&lnk, 0);
	} else if (expire == -1) {
		lnk->flags |= LINK_PERMANENT;
	} else if (expire > 0) {
		lnk->expire.time = expire;
	} else {
#ifdef LIBALIAS_DEBUG
		fprintf(stderr, "PacketAlias/SetExpire(): ");
		fprintf(stderr, "error in expire parameter\n");
#endif
	}
}

void
SetProtocolFlags(struct alias_link *lnk, int pflags)
{
	lnk->pflags = pflags;
}

int
GetProtocolFlags(struct alias_link *lnk)
{
	return (lnk->pflags);
}

void
SetDestCallId(struct alias_link *lnk, u_int16_t cid)
{
	LIBALIAS_LOCK_ASSERT(lnk->la);
	ReLink(lnk, lnk->src_addr, lnk->dst_addr, lnk->alias_addr,
	    lnk->src_port, cid, lnk->alias_port, lnk->link_type, 1);
}

/* Miscellaneous Functions

    HouseKeeping()
    InitPacketAliasLog()
    UninitPacketAliasLog()
*/

/*
    Whenever an outgoing or incoming packet is handled, HouseKeeping()
    is called to find and remove timed-out aliasing links.  Logic exists
    to sweep through the entire table and linked list structure
    every 60 seconds.

    (prototype in alias_local.h)
*/

void
HouseKeeping(struct libalias *la)
{
	static unsigned int packets = 0;
	static unsigned int packet_limit = 1000;

	LIBALIAS_LOCK_ASSERT(la);
	packets++;

	/*
	 * User space time/gettimeofday/... is very expensive.
	 * Kernel space cache trashing is unnecessary.
	 *
	 * Save system time (seconds) in global variable LibAliasTime
	 * for use by other functions. This is done so as not to
	 * unnecessarily waste timeline by making system calls.
	 *
	 * Reduce the amount of house keeping work substantially by
	 * sampling over the packets.
	 */
	if (packet_limit <= 1 || packets % packet_limit == 0) {
		time_t now;

#ifdef _KERNEL
		now = time_uptime;
#else
		now = time(NULL);
#endif
		if (now != LibAliasTime) {
			/* retry three times a second */
			packet_limit = packets / 3;
			packets = 0;
			LibAliasTime = now;
		}

	}
	/* Do a cleanup for the first packets of the new second only */
	if (packets < (la->udpLinkCount + la->tcpLinkCount)) {
		struct alias_link * lnk = TAILQ_FIRST(&la->checkExpire);

		CleanupLink(la, &lnk, 0);
	}
}

/* Init the log file and enable logging */
static int
InitPacketAliasLog(struct libalias *la)
{
	LIBALIAS_LOCK_ASSERT(la);
	if (~la->packetAliasMode & PKT_ALIAS_LOG) {
#ifdef _KERNEL
		if ((la->logDesc = malloc(LIBALIAS_BUF_SIZE)))
			;
#else
		if ((la->logDesc = fopen("/var/log/alias.log", "w")))
			fprintf(la->logDesc, "PacketAlias/InitPacketAliasLog: Packet alias logging enabled.\n");
#endif
		else
			return (ENOMEM); /* log initialization failed */
		la->packetAliasMode |= PKT_ALIAS_LOG;
	}

	return (1);
}

/* Close the log-file and disable logging. */
static void
UninitPacketAliasLog(struct libalias *la)
{
	LIBALIAS_LOCK_ASSERT(la);
	if (la->logDesc) {
#ifdef _KERNEL
		free(la->logDesc);
#else
		fclose(la->logDesc);
#endif
		la->logDesc = NULL;
	}
	la->packetAliasMode &= ~PKT_ALIAS_LOG;
}

/* Outside world interfaces

-- "outside world" means other than alias*.c routines --

    PacketAliasRedirectPort()
    PacketAliasAddServer()
    PacketAliasRedirectProto()
    PacketAliasRedirectAddr()
    PacketAliasRedirectDynamic()
    PacketAliasRedirectDelete()
    PacketAliasSetAddress()
    PacketAliasInit()
    PacketAliasUninit()
    PacketAliasSetMode()

(prototypes in alias.h)
*/

/* Redirection from a specific public addr:port to a
   private addr:port */
struct alias_link *
LibAliasRedirectPort(struct libalias *la, struct in_addr src_addr, u_short src_port,
    struct in_addr dst_addr, u_short dst_port,
    struct in_addr alias_addr, u_short alias_port,
    u_char proto)
{
	int link_type;
	struct alias_link *lnk;

	LIBALIAS_LOCK(la);
	switch (proto) {
	case IPPROTO_UDP:
		link_type = LINK_UDP;
		break;
	case IPPROTO_TCP:
		link_type = LINK_TCP;
		break;
	case IPPROTO_SCTP:
		link_type = LINK_SCTP;
		break;
	default:
#ifdef LIBALIAS_DEBUG
		fprintf(stderr, "PacketAliasRedirectPort(): ");
		fprintf(stderr, "only SCTP, TCP and UDP protocols allowed\n");
#endif
		lnk = NULL;
		goto getout;
	}

	lnk = AddLink(la, src_addr, dst_addr, alias_addr,
	    src_port, dst_port, alias_port,
	    link_type);

	if (lnk != NULL) {
		lnk->flags |= LINK_PERMANENT;
	}
#ifdef LIBALIAS_DEBUG
	else {
		fprintf(stderr, "PacketAliasRedirectPort(): "
		    "call to AddLink() failed\n");
	}
#endif

getout:
	LIBALIAS_UNLOCK(la);
	return (lnk);
}

/* Add server to the pool of servers */
int
LibAliasAddServer(struct libalias *la, struct alias_link *lnk, struct in_addr addr, u_short port)
{
	struct server *server;
	int res;

	LIBALIAS_LOCK(la);
	(void)la;

	switch (lnk->link_type) {
	case LINK_PPTP:
		server = NULL;
		break;
	default:
		server = malloc(sizeof(struct server));
		break;
	}

	if (server != NULL) {
		struct server *head;

		server->addr = addr;
		server->port = port;

		head = lnk->server;
		if (head == NULL) {
			server->next = server;
			/* not usable for outgoing connections */
			SPLAY_REMOVE(splay_out, &la->linkSplayOut, lnk);
		} else {
			struct server *s;

			for (s = head; s->next != head; s = s->next)
				;
			s->next = server;
			server->next = head;
		}
		lnk->server = server;
		res = 0;
	} else
		res = -1;

	LIBALIAS_UNLOCK(la);
	return (res);
}

/* Redirect packets of a given IP protocol from a specific
   public address to a private address */
struct alias_link *
LibAliasRedirectProto(struct libalias *la, struct in_addr src_addr,
    struct in_addr dst_addr,
    struct in_addr alias_addr,
    u_char proto)
{
	struct alias_link *lnk;

	LIBALIAS_LOCK(la);
	lnk = AddLink(la, src_addr, dst_addr, alias_addr,
	    NO_SRC_PORT, NO_DEST_PORT, 0,
	    proto);

	if (lnk != NULL) {
		lnk->flags |= LINK_PERMANENT;
	}
#ifdef LIBALIAS_DEBUG
	else {
		fprintf(stderr, "PacketAliasRedirectProto(): "
		    "call to AddLink() failed\n");
	}
#endif

	LIBALIAS_UNLOCK(la);
	return (lnk);
}

/* Static address translation */
struct alias_link *
LibAliasRedirectAddr(struct libalias *la, struct in_addr src_addr,
    struct in_addr alias_addr)
{
	struct alias_link *lnk;

	LIBALIAS_LOCK(la);
	lnk = AddLink(la, src_addr, ANY_ADDR, alias_addr,
	    0, 0, 0,
	    LINK_ADDR);

	if (lnk != NULL) {
		lnk->flags |= LINK_PERMANENT;
	}
#ifdef LIBALIAS_DEBUG
	else {
		fprintf(stderr, "PacketAliasRedirectAddr(): "
		    "call to AddLink() failed\n");
	}
#endif

	LIBALIAS_UNLOCK(la);
	return (lnk);
}

/* Mark the aliasing link dynamic */
int
LibAliasRedirectDynamic(struct libalias *la, struct alias_link *lnk)
{
	int res;

	LIBALIAS_LOCK(la);
	(void)la;

	if (lnk->flags & LINK_PARTIALLY_SPECIFIED)
		res = -1;
	else {
		lnk->flags &= ~LINK_PERMANENT;
		res = 0;
	}
	LIBALIAS_UNLOCK(la);
	return (res);
}

/* This is a dangerous function to put in the API,
   because an invalid pointer can crash the program. */
void
LibAliasRedirectDelete(struct libalias *la, struct alias_link *lnk)
{
	LIBALIAS_LOCK(la);
	(void)la;
	DeleteLink(&lnk, 1);
	LIBALIAS_UNLOCK(la);
}

void
LibAliasSetAddress(struct libalias *la, struct in_addr addr)
{
	LIBALIAS_LOCK(la);
	if (la->packetAliasMode & PKT_ALIAS_RESET_ON_ADDR_CHANGE
	    && la->aliasAddress.s_addr != addr.s_addr)
		CleanupAliasData(la, 0);

	la->aliasAddress = addr;
	LIBALIAS_UNLOCK(la);
}

void
LibAliasSetAliasPortRange(struct libalias *la, u_short port_low,
    u_short port_high)
{
	LIBALIAS_LOCK(la);
	if (port_low) {
		la->aliasPortLower = port_low;
		/* Add 1 to the aliasPortLength as modulo has range of 1 to n-1 */
		la->aliasPortLength = port_high - port_low + 1;
	} else {
		/* Set default values */
		la->aliasPortLower = 0x8000;
		la->aliasPortLength = 0x8000;
	}
	LIBALIAS_UNLOCK(la);
}

void
LibAliasSetTarget(struct libalias *la, struct in_addr target_addr)
{
	LIBALIAS_LOCK(la);
	la->targetAddress = target_addr;
	LIBALIAS_UNLOCK(la);
}

static void
finishoff(void)
{
	while (!LIST_EMPTY(&instancehead))
		LibAliasUninit(LIST_FIRST(&instancehead));
}

struct libalias *
LibAliasInit(struct libalias *la)
{
	if (la == NULL) {
#ifdef _KERNEL
#undef malloc	/* XXX: ugly */
		la = malloc(sizeof *la, M_ALIAS, M_WAITOK | M_ZERO);
#else
		la = calloc(sizeof *la, 1);
		if (la == NULL)
			return (la);
#endif

#ifndef _KERNEL
		/* kernel cleans up on module unload */
		if (LIST_EMPTY(&instancehead))
			atexit(finishoff);
#endif
		LIST_INSERT_HEAD(&instancehead, la, instancelist);

#ifdef _KERNEL
		LibAliasTime = time_uptime;
#else
		LibAliasTime = time(NULL);
#endif

		SPLAY_INIT(&la->linkSplayIn);
		SPLAY_INIT(&la->linkSplayOut);
		LIST_INIT(&la->pptpList);
		TAILQ_INIT(&la->checkExpire);
#ifdef _KERNEL
		AliasSctpInit(la);
#endif
		LIBALIAS_LOCK_INIT(la);
		LIBALIAS_LOCK(la);
	} else {
		LIBALIAS_LOCK(la);
		CleanupAliasData(la, 1);
#ifdef _KERNEL
		AliasSctpTerm(la);
		AliasSctpInit(la);
#endif
	}

	la->aliasAddress.s_addr = INADDR_ANY;
	la->targetAddress.s_addr = INADDR_ANY;
	la->aliasPortLower = 0x8000;
	la->aliasPortLength = 0x8000;

	la->icmpLinkCount = 0;
	la->udpLinkCount = 0;
	la->tcpLinkCount = 0;
	la->sctpLinkCount = 0;
	la->pptpLinkCount = 0;
	la->protoLinkCount = 0;
	la->fragmentIdLinkCount = 0;
	la->fragmentPtrLinkCount = 0;
	la->sockCount = 0;

	la->packetAliasMode = PKT_ALIAS_SAME_PORTS
#ifndef NO_USE_SOCKETS
	    | PKT_ALIAS_USE_SOCKETS
#endif
	    | PKT_ALIAS_RESET_ON_ADDR_CHANGE;
#ifndef NO_FW_PUNCH
	la->fireWallFD = -1;
#endif
#ifndef _KERNEL
	LibAliasRefreshModules();
#endif
	LIBALIAS_UNLOCK(la);
	return (la);
}

void
LibAliasUninit(struct libalias *la)
{
	LIBALIAS_LOCK(la);
#ifdef _KERNEL
	AliasSctpTerm(la);
#endif
	CleanupAliasData(la, 1);
	UninitPacketAliasLog(la);
#ifndef NO_FW_PUNCH
	UninitPunchFW(la);
#endif
	LIST_REMOVE(la, instancelist);
	LIBALIAS_UNLOCK(la);
	LIBALIAS_LOCK_DESTROY(la);
	free(la);
}

/* Change mode for some operations */
unsigned int
LibAliasSetMode(
    struct libalias *la,
    unsigned int flags,		/* Which state to bring flags to */
    unsigned int mask		/* Mask of which flags to affect (use 0 to
				 * do a probe for flag values) */
)
{
	int res = -1;

	LIBALIAS_LOCK(la);
	if (flags & mask & PKT_ALIAS_LOG) {
		/* Enable logging */
		if (InitPacketAliasLog(la) == ENOMEM)
			goto getout;
	} else if (~flags & mask & PKT_ALIAS_LOG)
		/* _Disable_ logging */
		UninitPacketAliasLog(la);

#ifndef NO_FW_PUNCH
	if (flags & mask & PKT_ALIAS_PUNCH_FW)
		/* Start punching holes in the firewall? */
		InitPunchFW(la);
	else if (~flags & mask & PKT_ALIAS_PUNCH_FW)
		/* Stop punching holes in the firewall? */
		UninitPunchFW(la);
#endif

	/* Other flags can be set/cleared without special action */
	la->packetAliasMode = (flags & mask) | (la->packetAliasMode & ~mask);
	res = la->packetAliasMode;
getout:
	LIBALIAS_UNLOCK(la);
	return (res);
}

#ifndef NO_FW_PUNCH

/*****************
  Code to support firewall punching.  This shouldn't really be in this
  file, but making variables global is evil too.
  ****************/

/* Firewall include files */
#include <net/if.h>
#include <netinet/ip_fw.h>
#include <string.h>
#include <err.h>

/*
 * helper function, updates the pointer to cmd with the length
 * of the current command, and also cleans up the first word of
 * the new command in case it has been clobbered before.
 */
static ipfw_insn *
next_cmd(ipfw_insn * cmd)
{
	cmd += F_LEN(cmd);
	bzero(cmd, sizeof(*cmd));
	return (cmd);
}

/*
 * A function to fill simple commands of size 1.
 * Existing flags are preserved.
 */
static ipfw_insn *
fill_cmd(ipfw_insn * cmd, enum ipfw_opcodes opcode, int size,
    int flags, u_int16_t arg)
{
	cmd->opcode = opcode;
	cmd->len = ((cmd->len | flags) & (F_NOT | F_OR)) | (size & F_LEN_MASK);
	cmd->arg1 = arg;
	return next_cmd(cmd);
}

static ipfw_insn *
fill_ip(ipfw_insn * cmd1, enum ipfw_opcodes opcode, u_int32_t addr)
{
	ipfw_insn_ip *cmd = (ipfw_insn_ip *)cmd1;

	cmd->addr.s_addr = addr;
	return fill_cmd(cmd1, opcode, F_INSN_SIZE(ipfw_insn_u32), 0, 0);
}

static ipfw_insn *
fill_one_port(ipfw_insn * cmd1, enum ipfw_opcodes opcode, u_int16_t port)
{
	ipfw_insn_u16 *cmd = (ipfw_insn_u16 *)cmd1;

	cmd->ports[0] = cmd->ports[1] = port;
	return fill_cmd(cmd1, opcode, F_INSN_SIZE(ipfw_insn_u16), 0, 0);
}

static int
fill_rule(void *buf, int bufsize, int rulenum,
    enum ipfw_opcodes action, int proto,
    struct in_addr sa, u_int16_t sp, struct in_addr da, u_int16_t dp)
{
	struct ip_fw *rule = (struct ip_fw *)buf;
	ipfw_insn *cmd = (ipfw_insn *)rule->cmd;

	bzero(buf, bufsize);
	rule->rulenum = rulenum;

	cmd = fill_cmd(cmd, O_PROTO, F_INSN_SIZE(ipfw_insn), 0, proto);
	cmd = fill_ip(cmd, O_IP_SRC, sa.s_addr);
	cmd = fill_one_port(cmd, O_IP_SRCPORT, sp);
	cmd = fill_ip(cmd, O_IP_DST, da.s_addr);
	cmd = fill_one_port(cmd, O_IP_DSTPORT, dp);

	rule->act_ofs = (u_int32_t *)cmd - (u_int32_t *)rule->cmd;
	cmd = fill_cmd(cmd, action, F_INSN_SIZE(ipfw_insn), 0, 0);

	rule->cmd_len = (u_int32_t *)cmd - (u_int32_t *)rule->cmd;

	return ((char *)cmd - (char *)buf);
}

static void
InitPunchFW(struct libalias *la)
{
	la->fireWallField = malloc(la->fireWallNumNums);
	if (la->fireWallField) {
		memset(la->fireWallField, 0, la->fireWallNumNums);
		if (la->fireWallFD < 0) {
			la->fireWallFD = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
		}
		ClearAllFWHoles(la);
		la->fireWallActiveNum = la->fireWallBaseNum;
	}
}

static void
UninitPunchFW(struct libalias *la)
{
	ClearAllFWHoles(la);
	if (la->fireWallFD >= 0)
		close(la->fireWallFD);
	la->fireWallFD = -1;
	if (la->fireWallField)
		free(la->fireWallField);
	la->fireWallField = NULL;
	la->packetAliasMode &= ~PKT_ALIAS_PUNCH_FW;
}

/* Make a certain link go through the firewall */
void
PunchFWHole(struct alias_link *lnk)
{
	struct libalias *la;
	int r;			/* Result code */
	struct ip_fw rule;	/* On-the-fly built rule */
	int fwhole;		/* Where to punch hole */

	la = lnk->la;

	/* Don't do anything unless we are asked to */
	if (!(la->packetAliasMode & PKT_ALIAS_PUNCH_FW) ||
	    la->fireWallFD < 0 ||
	    lnk->link_type != LINK_TCP)
		return;

	memset(&rule, 0, sizeof rule);

	/** Build rule **/

	/* Find empty slot */
	for (fwhole = la->fireWallActiveNum;
	    fwhole < la->fireWallBaseNum + la->fireWallNumNums &&
	    fw_tstfield(la, la->fireWallField, fwhole);
	    fwhole++);
	if (fwhole == la->fireWallBaseNum + la->fireWallNumNums) {
		for (fwhole = la->fireWallBaseNum;
		    fwhole < la->fireWallActiveNum &&
		    fw_tstfield(la, la->fireWallField, fwhole);
		    fwhole++);
		if (fwhole == la->fireWallActiveNum) {
			/* No rule point empty - we can't punch more holes. */
			la->fireWallActiveNum = la->fireWallBaseNum;
#ifdef LIBALIAS_DEBUG
			fprintf(stderr, "libalias: Unable to create firewall hole!\n");
#endif
			return;
		}
	}
	/* Start next search at next position */
	la->fireWallActiveNum = fwhole + 1;

	/*
	 * generate two rules of the form
	 *
	 * add fwhole accept tcp from OAddr OPort to DAddr DPort add fwhole
	 * accept tcp from DAddr DPort to OAddr OPort
	 */
	if (GetOriginalPort(lnk) != 0 && GetDestPort(lnk) != 0) {
		u_int32_t rulebuf[255];
		int i;

		i = fill_rule(rulebuf, sizeof(rulebuf), fwhole,
		    O_ACCEPT, IPPROTO_TCP,
		    GetOriginalAddress(lnk), ntohs(GetOriginalPort(lnk)),
		    GetDestAddress(lnk), ntohs(GetDestPort(lnk)));
		r = setsockopt(la->fireWallFD, IPPROTO_IP, IP_FW_ADD, rulebuf, i);
		if (r)
			err(1, "alias punch inbound(1) setsockopt(IP_FW_ADD)");

		i = fill_rule(rulebuf, sizeof(rulebuf), fwhole,
		    O_ACCEPT, IPPROTO_TCP,
		    GetDestAddress(lnk), ntohs(GetDestPort(lnk)),
		    GetOriginalAddress(lnk), ntohs(GetOriginalPort(lnk)));
		r = setsockopt(la->fireWallFD, IPPROTO_IP, IP_FW_ADD, rulebuf, i);
		if (r)
			err(1, "alias punch inbound(2) setsockopt(IP_FW_ADD)");
	}

	/* Indicate hole applied */
	lnk->data.tcp->fwhole = fwhole;
	fw_setfield(la, la->fireWallField, fwhole);
}

/* Remove a hole in a firewall associated with a particular alias
   lnk.  Calling this too often is harmless. */
static void
ClearFWHole(struct alias_link *lnk)
{
	struct libalias *la;

	la = lnk->la;
	if (lnk->link_type == LINK_TCP) {
		int fwhole = lnk->data.tcp->fwhole;  /* Where is the firewall hole? */
		struct ip_fw rule;

		if (fwhole < 0)
			return;

		memset(&rule, 0, sizeof rule);	/* useless for ipfw2 */
		while (!setsockopt(la->fireWallFD, IPPROTO_IP, IP_FW_DEL,
		    &fwhole, sizeof fwhole));
		fw_clrfield(la, la->fireWallField, fwhole);
		lnk->data.tcp->fwhole = -1;
	}
}

/* Clear out the entire range dedicated to firewall holes. */
static void
ClearAllFWHoles(struct libalias *la)
{
	struct ip_fw rule;	/* On-the-fly built rule */
	int i;

	if (la->fireWallFD < 0)
		return;

	memset(&rule, 0, sizeof rule);
	for (i = la->fireWallBaseNum; i < la->fireWallBaseNum + la->fireWallNumNums; i++) {
		int r = i;

		while (!setsockopt(la->fireWallFD, IPPROTO_IP, IP_FW_DEL, &r, sizeof r));
	}
	/* XXX: third arg correct here ? /phk */
	memset(la->fireWallField, 0, la->fireWallNumNums);
}

#endif /* !NO_FW_PUNCH */

void
LibAliasSetFWBase(struct libalias *la, unsigned int base, unsigned int num)
{
	LIBALIAS_LOCK(la);
#ifndef NO_FW_PUNCH
	la->fireWallBaseNum = base;
	la->fireWallNumNums = num;
#endif
	LIBALIAS_UNLOCK(la);
}

void
LibAliasSetSkinnyPort(struct libalias *la, unsigned int port)
{
	LIBALIAS_LOCK(la);
	la->skinnyPort = port;
	LIBALIAS_UNLOCK(la);
}

/*
 * Find the address to redirect incoming packets
 */
struct in_addr
FindSctpRedirectAddress(struct libalias *la,  struct sctp_nat_msg *sm)
{
	struct alias_link *lnk;
	struct in_addr redir;

	LIBALIAS_LOCK_ASSERT(la);
	lnk = FindLinkIn(la, sm->ip_hdr->ip_src, sm->ip_hdr->ip_dst,
	    sm->sctp_hdr->dest_port,sm->sctp_hdr->dest_port, LINK_SCTP, 1);
	if (lnk != NULL) {
		/* port redirect */
		return (lnk->src_addr);
	} else {
		redir = FindOriginalAddress(la,sm->ip_hdr->ip_dst);
		if (redir.s_addr == la->aliasAddress.s_addr ||
		    redir.s_addr == la->targetAddress.s_addr) {
			/* No address found */
			lnk = FindLinkIn(la, sm->ip_hdr->ip_src, sm->ip_hdr->ip_dst,
			    NO_DEST_PORT, 0, LINK_SCTP, 1);
			if (lnk != NULL)
				/* redirect proto */
				return (lnk->src_addr);
		}
		return (redir); /* address redirect */
	}
}
