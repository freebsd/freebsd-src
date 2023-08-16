/*	$OpenBSD: pf_print_state.c,v 1.52 2008/08/12 16:40:18 david Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001 Daniel Hartmeier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/endian.h>
#include <net/if.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/sctp.h>
#include <net/pfvar.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pfctl_parser.h"
#include "pfctl.h"

void	print_name(struct pf_addr *, sa_family_t);

void
print_addr(struct pf_addr_wrap *addr, sa_family_t af, int verbose)
{
	switch (addr->type) {
	case PF_ADDR_DYNIFTL:
		printf("(%s", addr->v.ifname);
		if (addr->iflags & PFI_AFLAG_NETWORK)
			printf(":network");
		if (addr->iflags & PFI_AFLAG_BROADCAST)
			printf(":broadcast");
		if (addr->iflags & PFI_AFLAG_PEER)
			printf(":peer");
		if (addr->iflags & PFI_AFLAG_NOALIAS)
			printf(":0");
		if (verbose) {
			if (addr->p.dyncnt <= 0)
				printf(":*");
			else
				printf(":%d", addr->p.dyncnt);
		}
		printf(")");
		break;
	case PF_ADDR_TABLE:
		if (verbose)
			if (addr->p.tblcnt == -1)
				printf("<%s:*>", addr->v.tblname);
			else
				printf("<%s:%d>", addr->v.tblname,
				    addr->p.tblcnt);
		else
			printf("<%s>", addr->v.tblname);
		return;
	case PF_ADDR_RANGE: {
		char buf[48];

		if (inet_ntop(af, &addr->v.a.addr, buf, sizeof(buf)) == NULL)
			printf("?");
		else
			printf("%s", buf);
		if (inet_ntop(af, &addr->v.a.mask, buf, sizeof(buf)) == NULL)
			printf(" - ?");
		else
			printf(" - %s", buf);
		break;
	}
	case PF_ADDR_ADDRMASK:
		if (PF_AZERO(&addr->v.a.addr, AF_INET6) &&
		    PF_AZERO(&addr->v.a.mask, AF_INET6))
			printf("any");
		else {
			char buf[48];

			if (inet_ntop(af, &addr->v.a.addr, buf,
			    sizeof(buf)) == NULL)
				printf("?");
			else
				printf("%s", buf);
		}
		break;
	case PF_ADDR_NOROUTE:
		printf("no-route");
		return;
	case PF_ADDR_URPFFAILED:
		printf("urpf-failed");
		return;
	default:
		printf("?");
		return;
	}

	/* mask if not _both_ address and mask are zero */
	if (addr->type != PF_ADDR_RANGE &&
	    !(PF_AZERO(&addr->v.a.addr, AF_INET6) &&
	    PF_AZERO(&addr->v.a.mask, AF_INET6))) {
		int bits = unmask(&addr->v.a.mask, af);

		if (bits != (af == AF_INET ? 32 : 128))
			printf("/%d", bits);
	}
}

void
print_name(struct pf_addr *addr, sa_family_t af)
{
	char host[NI_MAXHOST];

	strlcpy(host, "?", sizeof(host));
	switch (af) {
	case AF_INET: {
		struct sockaddr_in sin;

		memset(&sin, 0, sizeof(sin));
		sin.sin_len = sizeof(sin);
		sin.sin_family = AF_INET;
		sin.sin_addr = addr->v4;
		getnameinfo((struct sockaddr *)&sin, sin.sin_len,
		    host, sizeof(host), NULL, 0, NI_NOFQDN);
		break;
	}
	case AF_INET6: {
		struct sockaddr_in6 sin6;

		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_family = AF_INET6;
		sin6.sin6_addr = addr->v6;
		getnameinfo((struct sockaddr *)&sin6, sin6.sin6_len,
		    host, sizeof(host), NULL, 0, NI_NOFQDN);
		break;
	}
	}
	printf("%s", host);
}

void
print_host(struct pf_addr *addr, u_int16_t port, sa_family_t af, int opts)
{
	if (opts & PF_OPT_USEDNS)
		print_name(addr, af);
	else {
		struct pf_addr_wrap aw;

		memset(&aw, 0, sizeof(aw));
		aw.v.a.addr = *addr;
		if (af == AF_INET)
			aw.v.a.mask.addr32[0] = 0xffffffff;
		else {
			memset(&aw.v.a.mask, 0xff, sizeof(aw.v.a.mask));
			af = AF_INET6;
		}
		print_addr(&aw, af, opts & PF_OPT_VERBOSE2);
	}

	if (port) {
		if (af == AF_INET)
			printf(":%u", ntohs(port));
		else
			printf("[%u]", ntohs(port));
	}
}

void
print_seq(struct pfctl_state_peer *p)
{
	if (p->seqdiff)
		printf("[%u + %u](+%u)", p->seqlo,
		    p->seqhi - p->seqlo, p->seqdiff);
	else
		printf("[%u + %u]", p->seqlo,
		    p->seqhi - p->seqlo);
}


static const char *
sctp_state_name(int state)
{
	switch (state) {
	case SCTP_CLOSED:
		return ("CLOSED");
	case SCTP_BOUND:
		return ("BOUND");
	case SCTP_LISTEN:
		return ("LISTEN");
	case SCTP_COOKIE_WAIT:
		return ("COOKIE_WAIT");
	case SCTP_COOKIE_ECHOED:
		return ("COOKIE_ECHOED");
	case SCTP_ESTABLISHED:
		return ("ESTABLISHED");
	case SCTP_SHUTDOWN_SENT:
		return ("SHUTDOWN_SENT");
	case SCTP_SHUTDOWN_RECEIVED:
		return ("SHUTDOWN_RECEIVED");
	case SCTP_SHUTDOWN_ACK_SENT:
		return ("SHUTDOWN_ACK_SENT");
	case SCTP_SHUTDOWN_PENDING:
		return ("SHUTDOWN_PENDING");
	default:
		return ("?");
	}
}

void
print_state(struct pfctl_state *s, int opts)
{
	struct pfctl_state_peer *src, *dst;
	struct pfctl_state_key *key, *sk, *nk;
	const char *protoname;
	int min, sec;
	sa_family_t af;
	uint8_t proto;
#ifndef __NO_STRICT_ALIGNMENT
	struct pfctl_state_key aligned_key[2];

	bcopy(&s->key, aligned_key, sizeof(aligned_key));
	key = aligned_key;
#else
	key = s->key;
#endif

	af = s->key[PF_SK_WIRE].af;
	proto = s->key[PF_SK_WIRE].proto;

	if (s->direction == PF_OUT) {
		src = &s->src;
		dst = &s->dst;
		sk = &key[PF_SK_STACK];
		nk = &key[PF_SK_WIRE];
		if (proto == IPPROTO_ICMP || proto == IPPROTO_ICMPV6)
			sk->port[0] = nk->port[0];
	} else {
		src = &s->dst;
		dst = &s->src;
		sk = &key[PF_SK_WIRE];
		nk = &key[PF_SK_STACK];
		if (proto == IPPROTO_ICMP || proto == IPPROTO_ICMPV6)
			sk->port[1] = nk->port[1];
	}
	printf("%s ", s->ifname);
	if ((protoname = pfctl_proto2name(proto)) != NULL)
		printf("%s ", protoname);
	else
		printf("%u ", proto);

	print_host(&nk->addr[1], nk->port[1], af, opts);
	if (PF_ANEQ(&nk->addr[1], &sk->addr[1], af) ||
	    nk->port[1] != sk->port[1]) {
		printf(" (");
		print_host(&sk->addr[1], sk->port[1], af, opts);
		printf(")");
	}
	if (s->direction == PF_OUT)
		printf(" -> ");
	else
		printf(" <- ");
	print_host(&nk->addr[0], nk->port[0], af, opts);
	if (PF_ANEQ(&nk->addr[0], &sk->addr[0], af) ||
	    nk->port[0] != sk->port[0]) {
		printf(" (");
		print_host(&sk->addr[0], sk->port[0], af, opts);
		printf(")");
	}

	printf("    ");
	if (proto == IPPROTO_TCP) {
		if (src->state <= TCPS_TIME_WAIT &&
		    dst->state <= TCPS_TIME_WAIT)
			printf("   %s:%s\n", tcpstates[src->state],
			    tcpstates[dst->state]);
		else if (src->state == PF_TCPS_PROXY_SRC ||
		    dst->state == PF_TCPS_PROXY_SRC)
			printf("   PROXY:SRC\n");
		else if (src->state == PF_TCPS_PROXY_DST ||
		    dst->state == PF_TCPS_PROXY_DST)
			printf("   PROXY:DST\n");
		else
			printf("   <BAD STATE LEVELS %u:%u>\n",
			    src->state, dst->state);
		if (opts & PF_OPT_VERBOSE) {
			printf("   ");
			print_seq(src);
			if (src->wscale && dst->wscale)
				printf(" wscale %u",
				    src->wscale & PF_WSCALE_MASK);
			printf("  ");
			print_seq(dst);
			if (src->wscale && dst->wscale)
				printf(" wscale %u",
				    dst->wscale & PF_WSCALE_MASK);
			printf("\n");
		}
	} else if (proto == IPPROTO_UDP && src->state < PFUDPS_NSTATES &&
	    dst->state < PFUDPS_NSTATES) {
		const char *states[] = PFUDPS_NAMES;

		printf("   %s:%s\n", states[src->state], states[dst->state]);
	} else if (proto == IPPROTO_SCTP) {
		printf("   %s:%s\n", sctp_state_name(src->state),
		    sctp_state_name(dst->state));
#ifndef INET6
	} else if (proto != IPPROTO_ICMP && src->state < PFOTHERS_NSTATES &&
	    dst->state < PFOTHERS_NSTATES) {
#else
	} else if (proto != IPPROTO_ICMP && proto != IPPROTO_ICMPV6 &&
	    src->state < PFOTHERS_NSTATES && dst->state < PFOTHERS_NSTATES) {
#endif
		/* XXX ICMP doesn't really have state levels */
		const char *states[] = PFOTHERS_NAMES;

		printf("   %s:%s\n", states[src->state], states[dst->state]);
	} else {
		printf("   %u:%u\n", src->state, dst->state);
	}

	if (opts & PF_OPT_VERBOSE) {
		u_int32_t creation = s->creation;
		u_int32_t expire = s->expire;

		sec = creation % 60;
		creation /= 60;
		min = creation % 60;
		creation /= 60;
		printf("   age %.2u:%.2u:%.2u", creation, min, sec);
		sec = expire % 60;
		expire /= 60;
		min = expire % 60;
		expire /= 60;
		printf(", expires in %.2u:%.2u:%.2u", expire, min, sec);

		printf(", %ju:%ju pkts, %ju:%ju bytes",
		    s->packets[0],
		    s->packets[1],
		    s->bytes[0],
		    s->bytes[1]);
		if (s->anchor != -1)
			printf(", anchor %u", s->anchor);
		if (s->rule != -1)
			printf(", rule %u", s->rule);
		if (s->state_flags & PFSTATE_ALLOWOPTS)
			printf(", allow-opts");
		if (s->state_flags & PFSTATE_SLOPPY)
			printf(", sloppy");
		if (s->state_flags & PFSTATE_NOSYNC)
			printf(", no-sync");
		if (s->state_flags & PFSTATE_ACK)
			printf(", psync-ack");
		if (s->state_flags & PFSTATE_NODF)
			printf(", no-df");
		if (s->state_flags & PFSTATE_SETTOS)
			printf(", set-tos 0x%2.2x", s->set_tos);
		if (s->state_flags & PFSTATE_RANDOMID)
			printf(", random-id");
		if (s->state_flags & PFSTATE_SCRUB_TCP)
			printf(", reassemble-tcp");
		if (s->state_flags & PFSTATE_SETPRIO)
			printf(", set-prio (0x%02x 0x%02x)",
			    s->set_prio[0], s->set_prio[1]);
		if (s->dnpipe || s->dnrpipe) {
			if (s->state_flags & PFSTATE_DN_IS_PIPE)
				printf(", dummynet pipe (%d %d)",
				s->dnpipe, s->dnrpipe);
			if (s->state_flags & PFSTATE_DN_IS_QUEUE)
				printf(", dummynet queue (%d %d)",
				s->dnpipe, s->dnrpipe);
		}
		if (s->sync_flags & PFSYNC_FLAG_SRCNODE)
			printf(", source-track");
		if (s->sync_flags & PFSYNC_FLAG_NATSRCNODE)
			printf(", sticky-address");
		if (s->log)
			printf(", log");
		if (s->log & PF_LOG_ALL)
			printf(" (all)");
		if (s->min_ttl)
			printf(", min-ttl %d", s->min_ttl);
		if (s->max_mss)
			printf(", max-mss %d", s->max_mss);
		printf("\n");
	}
	if (opts & PF_OPT_VERBOSE2) {
		u_int64_t id;

		bcopy(&s->id, &id, sizeof(u_int64_t));
		printf("   id: %016jx creatorid: %08x", id, s->creatorid);
		if (s->rt) {
			switch (s->rt) {
				case PF_ROUTETO:
					printf(" route-to: ");
					break;
				case PF_DUPTO:
					printf(" dup-to: ");
					break;
				case PF_REPLYTO:
					printf(" reply-to: ");
					break;
				default:
					printf(" gateway: ");
			}
			print_host(&s->rt_addr, 0, af, opts);
			if (s->rt_ifname[0])
				printf("@%s", s->rt_ifname);
		}
		if (s->rtableid != -1)
			printf(" rtable: %d", s->rtableid);
		printf("\n");

		if (strcmp(s->ifname, s->orig_ifname) != 0)
			printf("   origif: %s\n", s->orig_ifname);
	}
}

int
unmask(struct pf_addr *m, sa_family_t af)
{
	int i = 31, j = 0, b = 0;
	u_int32_t tmp;

	while (j < 4 && m->addr32[j] == 0xffffffff) {
		b += 32;
		j++;
	}
	if (j < 4) {
		tmp = ntohl(m->addr32[j]);
		for (i = 31; tmp & (1 << i); --i)
			b++;
	}
	return (b);
}
