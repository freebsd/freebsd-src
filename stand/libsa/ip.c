/*
 * Copyright (c) 1992 Regents of the University of California.
 * All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

/*
 * The send and receive functions were originally implemented in udp.c and
 * moved here. Also it is likely some more cleanup can be done, especially
 * once we will implement the support for tcp.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <string.h>
#include <stdbool.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>

#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include "stand.h"
#include "net.h"

typedef STAILQ_HEAD(ipqueue, ip_queue) ip_queue_t;
struct ip_queue {
	void		*ipq_pkt;
	struct ip	*ipq_hdr;
	STAILQ_ENTRY(ip_queue) ipq_next;
};

/*
 * Fragment re-assembly queue.
 */
struct ip_reasm {
	struct in_addr	ip_src;
	struct in_addr	ip_dst;
	uint16_t	ip_id;
	uint8_t		ip_proto;
	uint8_t		ip_ttl;
	size_t		ip_total_size;
	ip_queue_t	ip_queue;
	void		*ip_pkt;
	struct ip	*ip_hdr;
	STAILQ_ENTRY(ip_reasm) ip_next;
};

STAILQ_HEAD(ire_list, ip_reasm) ire_list = STAILQ_HEAD_INITIALIZER(ire_list);

/* Caller must leave room for ethernet and ip headers in front!! */
ssize_t
sendip(struct iodesc *d, void *pkt, size_t len, uint8_t proto)
{
	ssize_t cc;
	struct ip *ip;
	u_char *ea;

	DEBUG_PRINTF(1, ("sendip: proto: %x d=%p called.\n", proto, (void *)d));
	DEBUG_PRINTF(1, ("saddr: %s:%d daddr: %s:%d\n",
	    inet_ntoa(d->myip), ntohs(d->myport),
	    inet_ntoa(d->destip), ntohs(d->destport)));

	ip = (struct ip *)pkt - 1;
	len += sizeof(*ip);

	bzero(ip, sizeof(*ip));

	ip->ip_v = IPVERSION;			/* half-char */
	ip->ip_hl = sizeof(*ip) >> 2;		/* half-char */
	ip->ip_len = htons(len);
	ip->ip_p = proto;			/* char */
	ip->ip_ttl = IPDEFTTL;			/* char */
	ip->ip_src = d->myip;
	ip->ip_dst = d->destip;
	ip->ip_sum = in_cksum(ip, sizeof(*ip));	 /* short, but special */

	if (ip->ip_dst.s_addr == INADDR_BROADCAST || ip->ip_src.s_addr == 0 ||
	    netmask == 0 || SAMENET(ip->ip_src, ip->ip_dst, netmask))
		ea = arpwhohas(d, ip->ip_dst);
	else
		ea = arpwhohas(d, gateip);

	cc = sendether(d, ip, len, ea, ETHERTYPE_IP);
	if (cc == -1)
		return (-1);
	if (cc != len)
		panic("sendip: bad write (%zd != %zd)", cc, len);
	return (cc - sizeof(*ip));
}

static void
ip_reasm_free(struct ip_reasm *ipr)
{
	struct ip_queue *ipq;

	while ((ipq = STAILQ_FIRST(&ipr->ip_queue)) != NULL) {
		STAILQ_REMOVE_HEAD(&ipr->ip_queue, ipq_next);
		free(ipq->ipq_pkt);
		free(ipq);
	}
	free(ipr->ip_pkt);
	free(ipr);
}

static bool
ip_reasm_add(struct ip_reasm *ipr, void *pkt, struct ip *ip)
{
	struct ip_queue *ipq, *p;
	uint16_t off_q, off_ip;

	if ((ipq = calloc(1, sizeof(*ipq))) == NULL)
		return (false);

	ipq->ipq_pkt = pkt;
	ipq->ipq_hdr = ip;

	STAILQ_FOREACH(p, &ipr->ip_queue, ipq_next) {
		off_q = ntohs(p->ipq_hdr->ip_off) & IP_OFFMASK;
		off_ip = ntohs(ip->ip_off) & IP_OFFMASK;

		if (off_q == off_ip) {	/* duplicate */
			free(pkt);
			free(ipq);
			return (true);
		}

		if (off_ip < off_q) {
			/*
			 * Everything in queue has larger offset,
			 * drop out of loop and insert to HEAD.
			 */
			break;
		}

		/*
		 * p in queue is smaller than ip, check if we need to put
		 * ip after p or after p->next.
		 */
		struct ip_queue *next = STAILQ_NEXT(p, ipq_next);
		if (next == NULL) {
			/* insert after p */
			STAILQ_INSERT_AFTER(&ipr->ip_queue, p, ipq, ipq_next);
			return (true);
		}

		off_q = ntohs(next->ipq_hdr->ip_off) & IP_OFFMASK;
		if (off_ip < off_q) {
			/* next fragment offset is larger, insert after p. */
			STAILQ_INSERT_AFTER(&ipr->ip_queue, p, ipq, ipq_next);
			return (true);
		}
		/* next fragment offset is smaller, loop */
	}
	STAILQ_INSERT_HEAD(&ipr->ip_queue, ipq, ipq_next);
	return (true);
}

/*
 * Receive a IP packet and validate it is for us.
 */
static ssize_t
readipv4(struct iodesc *d, void **pkt, void **payload, ssize_t n)
{
	struct ip *ip = *payload;
	size_t hlen;
	struct ether_header *eh;
	struct udphdr *uh;
	char *ptr = *pkt;
	struct ip_reasm *ipr;
	struct ip_queue *ipq, *last;
	bool morefrag, isfrag;
	uint16_t fragoffset;

	if (n < sizeof(*ip)) {
		free(ptr);
		errno = EAGAIN;	/* Call me again. */
		return (-1);
	}

	hlen = ip->ip_hl << 2;
	if (hlen < sizeof(*ip) ||
	    in_cksum(ip, hlen) != 0) {
		DEBUG_PRINTF(1, ("%s: short hdr or bad cksum.\n", __func__));
		free(ptr);
		errno = EAGAIN;	/* Call me again. */
		return (-1);
	}

	if (n < ntohs(ip->ip_len)) {
		DEBUG_PRINTF(1, ("readip: bad length %zd < %d.\n",
		       n, ntohs(ip->ip_len)));
		free(ptr);
		errno = EAGAIN; /* Call me again. */
		return (-1);
	}

	fragoffset = (ntohs(ip->ip_off) & IP_OFFMASK) * 8;
	morefrag = (ntohs(ip->ip_off) & IP_MF) == 0 ? false : true;
	isfrag = morefrag || fragoffset != 0;

	uh = (struct udphdr *)((uintptr_t)ip + sizeof(*ip));

	if (d->myip.s_addr && ip->ip_dst.s_addr != d->myip.s_addr) {
		DEBUG_PRINTF(1, ("%s: not for us: saddr %s (%d) != %s (%d)\n",
		    __func__, inet_ntoa(d->myip), ntohs(d->myport),
		    inet_ntoa(ip->ip_dst), ntohs(uh->uh_dport)));
		free(ptr);
		errno = EAGAIN; /* Call me again. */
		return (-1);
	}

	/* Unfragmented packet. */
	if (!isfrag) {
		DEBUG_PRINTF(1, ("%s: unfragmented saddr %s:%d -> %s:%d\n",
		    __func__,
		    inet_ntoa(ip->ip_src), ntohs(uh->uh_sport),
		    inet_ntoa(ip->ip_dst), ntohs(uh->uh_dport)));
		/* If there were ip options, make them go away */
		if (hlen != sizeof(*ip)) {
			bcopy(((u_char *)ip) + hlen, uh,
			    ntohs(uh->uh_ulen) - hlen);
			ip->ip_len = htons(sizeof(*ip));
			n -= hlen - sizeof(*ip);
		}

		n = (n > (ntohs(ip->ip_len) - sizeof(*ip))) ?
		    ntohs(ip->ip_len) - sizeof(*ip) : n;
		*pkt = ptr;
		*payload = (void *)((uintptr_t)ip + sizeof(*ip));
		return (n);
	}

	STAILQ_FOREACH(ipr, &ire_list, ip_next) {
		if (ipr->ip_src.s_addr == ip->ip_src.s_addr &&
		    ipr->ip_dst.s_addr == ip->ip_dst.s_addr &&
		    ipr->ip_id == ip->ip_id &&
		    ipr->ip_proto == ip->ip_p)
			break;
	}

	/* Allocate new reassembly entry */
	if (ipr == NULL) {
		if ((ipr = calloc(1, sizeof(*ipr))) == NULL) {
			free(ptr);
			return (-1);
		}

		ipr->ip_src = ip->ip_src;
		ipr->ip_dst = ip->ip_dst;
		ipr->ip_id = ip->ip_id;
		ipr->ip_proto = ip->ip_p;
		ipr->ip_ttl = MAXTTL;
		STAILQ_INIT(&ipr->ip_queue);
		STAILQ_INSERT_TAIL(&ire_list, ipr, ip_next);
		DEBUG_PRINTF(1, ("%s: new reassembly ID=%d %s -> %s\n",
		    __func__, ntohs(ip->ip_id), inet_ntoa(ip->ip_src),
		    inet_ntoa(ip->ip_dst)));
	}

	/*
	 * NOTE: with ip_reasm_add() ptr will be stored in reassembly
	 * queue and we can not free it without destroying the queue.
	 */
	if (!ip_reasm_add(ipr, ptr, ip)) {
		STAILQ_REMOVE(&ire_list, ipr, ip_reasm, ip_next);
		free(ipr);
		free(ptr);
		return (-1);
	}

	/*
	 * Walk the packet list in reassembly queue, if we got all the
	 * fragments, build the packet.
	 */
	n = 0;
	last = NULL;
	STAILQ_FOREACH(ipq, &ipr->ip_queue, ipq_next) {
		fragoffset = (ntohs(ipq->ipq_hdr->ip_off) & IP_OFFMASK) * 8;
		if (fragoffset != n) {
			DEBUG_PRINTF(1, ("%s: need more fragments %d %s -> ",
			    __func__, ntohs(ipq->ipq_hdr->ip_id),
			    inet_ntoa(ipq->ipq_hdr->ip_src)));
			DEBUG_PRINTF(1, ("%s offset=%d MF=%d\n",
			    inet_ntoa(ipq->ipq_hdr->ip_dst),
			    fragoffset,
			    (ntohs(ipq->ipq_hdr->ip_off) & IP_MF) != 0));
			errno = EAGAIN;
			return (-1);
		}

		n += ntohs(ipq->ipq_hdr->ip_len) - (ipq->ipq_hdr->ip_hl << 2);
		last = ipq;
	}

	/* complete queue has last packet with MF 0 */
	if ((ntohs(last->ipq_hdr->ip_off) & IP_MF) != 0) {
		DEBUG_PRINTF(1, ("%s: need more fragments %d %s -> ",
		    __func__, ntohs(last->ipq_hdr->ip_id),
		    inet_ntoa(last->ipq_hdr->ip_src)));
		DEBUG_PRINTF(1, ("%s offset=%d MF=%d\n",
		    inet_ntoa(last->ipq_hdr->ip_dst),
		    (ntohs(last->ipq_hdr->ip_off) & IP_OFFMASK) * 8,
		    (ntohs(last->ipq_hdr->ip_off) & IP_MF) != 0));
		errno = EAGAIN;
		return (-1);
	}

	ipr->ip_total_size = n + sizeof(*ip) + sizeof(struct ether_header);
	ipr->ip_pkt = malloc(ipr->ip_total_size + 2);
	if (ipr->ip_pkt == NULL) {
		STAILQ_REMOVE(&ire_list, ipr, ip_reasm, ip_next);
		ip_reasm_free(ipr);
		return (-1);
	}

	ipq = STAILQ_FIRST(&ipr->ip_queue);
	/* Fabricate ethernet header */
	eh = (struct ether_header *)((uintptr_t)ipr->ip_pkt + 2);
	bcopy((void *)((uintptr_t)ipq->ipq_pkt + 2), eh, sizeof(*eh));

	/* Fabricate IP header */
	ipr->ip_hdr = (struct ip *)((uintptr_t)eh + sizeof(*eh));
	bcopy(ipq->ipq_hdr, ipr->ip_hdr, sizeof(*ipr->ip_hdr));
	ipr->ip_hdr->ip_hl = sizeof(*ipr->ip_hdr) >> 2;
	ipr->ip_hdr->ip_len = htons(n);
	ipr->ip_hdr->ip_sum = 0;
	ipr->ip_hdr->ip_sum = in_cksum(ipr->ip_hdr, sizeof(*ipr->ip_hdr));

	n = 0;
	ptr = (char *)((uintptr_t)ipr->ip_hdr + sizeof(*ipr->ip_hdr));
	STAILQ_FOREACH(ipq, &ipr->ip_queue, ipq_next) {
		char *data;
		size_t len;

		hlen = ipq->ipq_hdr->ip_hl << 2;
		len = ntohs(ipq->ipq_hdr->ip_len) - hlen;
		data = (char *)((uintptr_t)ipq->ipq_hdr + hlen);

		bcopy(data, ptr + n, len);
		n += len;
	}

	*pkt = ipr->ip_pkt;
	ipr->ip_pkt = NULL;	/* Avoid free from ip_reasm_free() */
	*payload = ptr;

	/* Clean up the reassembly list */
	while ((ipr = STAILQ_FIRST(&ire_list)) != NULL) {
		STAILQ_REMOVE_HEAD(&ire_list, ip_next);
		ip_reasm_free(ipr);
	}
	DEBUG_PRINTF(1, ("%s: completed fragments ID=%d %s -> %s\n",
	    __func__, ntohs(ip->ip_id), inet_ntoa(ip->ip_src),
	    inet_ntoa(ip->ip_dst)));
	return (n);
}

/*
 * Receive a IP packet.
 */
ssize_t
readip(struct iodesc *d, void **pkt, void **payload, time_t tleft,
    uint8_t proto)
{
	time_t t;
	ssize_t ret = -1;

	t = getsecs();
	while ((getsecs() - t) < tleft) {
		ssize_t n;
		uint16_t etype; /* host order */
		void *ptr = NULL;
		void *data = NULL;

		errno = 0;
		n = readether(d, &ptr, &data, tleft, &etype);
		if (n == -1) {
			free(ptr);
			continue;
		}
		/* Ethernet address checks are done in readether() */

		/* Need to respond to ARP requests. */
		if (etype == ETHERTYPE_ARP) {
			struct arphdr *ah = data;

			DEBUG_PRINTF(1, ("%s: ARP request\n", __func__));

			if (ah->ar_op == htons(ARPOP_REQUEST)) {
				/* Send ARP reply */
				arp_reply(d, ah);
			}
			free(ptr);
			continue;	/* Get next packet */
		}

		if (etype == ETHERTYPE_IP) {
			struct ip *ip = data;

			if (ip->ip_v == IPVERSION &&	/* half char */
			    ip->ip_p == proto) {
				errno = 0;
				ret = readipv4(d, &ptr, &data, n);
				if (ret >= 0) {
					*pkt = ptr;
					*payload = data;
					return (ret);
				}

				/*
				 * Bubble up the error if it wasn't successful
				 */
				if (errno != EAGAIN)
					return (-1);
				continue;
			}
			DEBUG_PRINTF(1, ("%s: IP version or proto. "
			    "ip_v=%d ip_p=%d\n",
			    __func__, ip->ip_v, ip->ip_p));
			free(ptr);
			continue;
		}
		free(ptr);
	}
	/* We've exhausted tleft; timeout */
	errno = ETIMEDOUT;
	DEBUG_PRINTF(1, ("%s: timeout\n", __func__));
	return (-1);
}
