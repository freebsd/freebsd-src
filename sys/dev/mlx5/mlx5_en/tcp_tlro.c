/*-
 * Copyright (c) 2015 Mellanox Technologies. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/libkern.h>
#include <sys/mbuf.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/endian.h>
#include <sys/socket.h>
#include <sys/sockopt.h>
#include <sys/smp.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/ethernet.h>

#if defined(INET) || defined(INET6)
#include <netinet/in.h>
#endif

#ifdef INET
#include <netinet/ip.h>
#endif

#ifdef INET6
#include <netinet/ip6.h>
#endif

#include <netinet/tcp_var.h>

#include "tcp_tlro.h"

#ifndef M_HASHTYPE_LRO_TCP
#ifndef KLD_MODULE
#warning "M_HASHTYPE_LRO_TCP is not defined"
#endif
#define	M_HASHTYPE_LRO_TCP 254
#endif

static SYSCTL_NODE(_net_inet_tcp, OID_AUTO, tlro,
    CTLFLAG_RW, 0, "TCP turbo LRO parameters");

static MALLOC_DEFINE(M_TLRO, "TLRO", "Turbo LRO");

static int tlro_min_rate = 20;		/* Hz */

SYSCTL_INT(_net_inet_tcp_tlro, OID_AUTO, min_rate, CTLFLAG_RWTUN,
    &tlro_min_rate, 0, "Minimum serving rate in Hz");

static int tlro_max_packet = IP_MAXPACKET;

SYSCTL_INT(_net_inet_tcp_tlro, OID_AUTO, max_packet, CTLFLAG_RWTUN,
    &tlro_max_packet, 0, "Maximum packet size in bytes");

typedef struct {
	uint32_t value;
} __packed uint32_p_t;

static uint16_t
tcp_tlro_csum(const uint32_p_t *p, size_t l)
{
	const uint32_p_t *pend = p + (l / 4);
	uint64_t cs;

	for (cs = 0; p != pend; p++)
		cs += le32toh(p->value);
	while (cs > 0xffff)
		cs = (cs >> 16) + (cs & 0xffff);
	return (cs);
}

static void *
tcp_tlro_get_header(const struct mbuf *m, const u_int off,
    const u_int len)
{
	if (m->m_len < (off + len))
		return (NULL);
	return (mtod(m, char *) + off);
}

static uint8_t
tcp_tlro_info_save_timestamp(struct tlro_mbuf_data *pinfo)
{
	struct tcphdr *tcp = pinfo->tcp;
	uint32_t *ts_ptr;

	if (tcp->th_off < ((TCPOLEN_TSTAMP_APPA + sizeof(*tcp)) >> 2))
		return (0);

	ts_ptr = (uint32_t *)(tcp + 1);
	if (*ts_ptr != ntohl((TCPOPT_NOP << 24) | (TCPOPT_NOP << 16) |
	    (TCPOPT_TIMESTAMP << 8) | TCPOLEN_TIMESTAMP))
		return (0);

	/* Save timestamps */
	pinfo->tcp_ts = ts_ptr[1];
	pinfo->tcp_ts_reply = ts_ptr[2];
	return (1);
}

static void
tcp_tlro_info_restore_timestamp(struct tlro_mbuf_data *pinfoa,
    struct tlro_mbuf_data *pinfob)
{
	struct tcphdr *tcp = pinfoa->tcp;
	uint32_t *ts_ptr;

	if (tcp->th_off < ((TCPOLEN_TSTAMP_APPA + sizeof(*tcp)) >> 2))
		return;

	ts_ptr = (uint32_t *)(tcp + 1);
	if (*ts_ptr != ntohl((TCPOPT_NOP << 24) | (TCPOPT_NOP << 16) |
	    (TCPOPT_TIMESTAMP << 8) | TCPOLEN_TIMESTAMP))
		return;

	/* Restore timestamps */
	ts_ptr[1] = pinfob->tcp_ts;
	ts_ptr[2] = pinfob->tcp_ts_reply;
}

static void
tcp_tlro_extract_header(struct tlro_mbuf_data *pinfo, struct mbuf *m, int seq)
{
	uint8_t *phdr = (uint8_t *)pinfo->buf;
	struct ether_header *eh;
	struct ether_vlan_header *vlan;
#ifdef INET
	struct ip *ip;
#endif
#ifdef INET6
	struct ip6_hdr *ip6;
#endif
	struct tcphdr *tcp;
	uint16_t etype;
	int diff;
	int off;

	/* Fill in information */
	pinfo->head = m;
	pinfo->last_tick = ticks;
	pinfo->sequence = seq;
	pinfo->pprev = &m_last(m)->m_next;

	off = sizeof(*eh);
	if (m->m_len < off)
		goto error;
	eh = tcp_tlro_get_header(m, 0, sizeof(*eh));
	if (eh == NULL)
		goto error;
	memcpy(phdr, &eh->ether_dhost, ETHER_ADDR_LEN);
	phdr += ETHER_ADDR_LEN;
	memcpy(phdr, &eh->ether_type, sizeof(eh->ether_type));
	phdr += sizeof(eh->ether_type);
	etype = ntohs(eh->ether_type);

	if (etype == ETHERTYPE_VLAN) {
		vlan = tcp_tlro_get_header(m, off, sizeof(*vlan));
		if (vlan == NULL)
			goto error;
		memcpy(phdr, &vlan->evl_tag, sizeof(vlan->evl_tag) +
		    sizeof(vlan->evl_proto));
		phdr += sizeof(vlan->evl_tag) + sizeof(vlan->evl_proto);
		etype = ntohs(vlan->evl_proto);
		off += sizeof(*vlan) - sizeof(*eh);
	}
	switch (etype) {
#ifdef INET
	case ETHERTYPE_IP:
		/*
		 * Cannot LRO:
		 * - Non-IP packets
		 * - Fragmented packets
		 * - Packets with IPv4 options
		 * - Non-TCP packets
		 */
		ip = tcp_tlro_get_header(m, off, sizeof(*ip));
		if (ip == NULL ||
		    (ip->ip_off & htons(IP_MF | IP_OFFMASK)) != 0 ||
		    (ip->ip_p != IPPROTO_TCP) ||
		    (ip->ip_hl << 2) != sizeof(*ip))
			goto error;

		/* Legacy IP has a header checksum that needs to be correct */
		if (!(m->m_pkthdr.csum_flags & CSUM_IP_CHECKED)) {
			/* Verify IP header */
			if (tcp_tlro_csum((uint32_p_t *)ip, sizeof(*ip)) != 0xFFFF)
				m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
			else
				m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED |
				    CSUM_IP_VALID;
		}
		/* Only accept valid checksums */
		if (!(m->m_pkthdr.csum_flags & CSUM_IP_VALID) ||
		    !(m->m_pkthdr.csum_flags & CSUM_DATA_VALID))
			goto error;
		memcpy(phdr, &ip->ip_src, sizeof(ip->ip_src) +
		    sizeof(ip->ip_dst));
		phdr += sizeof(ip->ip_src) + sizeof(ip->ip_dst);
		if (M_HASHTYPE_GET(m) == M_HASHTYPE_LRO_TCP)
			pinfo->ip_len = m->m_pkthdr.len - off;
		else
			pinfo->ip_len = ntohs(ip->ip_len);
		pinfo->ip_hdrlen = sizeof(*ip);
		pinfo->ip.v4 = ip;
		pinfo->ip_version = 4;
		off += sizeof(*ip);
		break;
#endif
#ifdef INET6
	case ETHERTYPE_IPV6:
		/*
		 * Cannot LRO:
		 * - Non-IP packets
		 * - Packets with IPv6 options
		 * - Non-TCP packets
		 */
		ip6 = tcp_tlro_get_header(m, off, sizeof(*ip6));
		if (ip6 == NULL || ip6->ip6_nxt != IPPROTO_TCP)
			goto error;
		if (!(m->m_pkthdr.csum_flags & CSUM_DATA_VALID))
			goto error;
		memcpy(phdr, &ip6->ip6_src, sizeof(struct in6_addr) +
		    sizeof(struct in6_addr));
		phdr += sizeof(struct in6_addr) + sizeof(struct in6_addr);
		if (M_HASHTYPE_GET(m) == M_HASHTYPE_LRO_TCP)
			pinfo->ip_len = m->m_pkthdr.len - off;
		else
			pinfo->ip_len = ntohs(ip6->ip6_plen) + sizeof(*ip6);
		pinfo->ip_hdrlen = sizeof(*ip6);
		pinfo->ip.v6 = ip6;
		pinfo->ip_version = 6;
		off += sizeof(*ip6);
		break;
#endif
	default:
		goto error;
	}
	tcp = tcp_tlro_get_header(m, off, sizeof(*tcp));
	if (tcp == NULL)
		goto error;
	memcpy(phdr, &tcp->th_sport, sizeof(tcp->th_sport) +
	    sizeof(tcp->th_dport));
	phdr += sizeof(tcp->th_sport) +
	    sizeof(tcp->th_dport);
	/* Store TCP header length */
	*phdr++ = tcp->th_off;
	if (tcp->th_off < (sizeof(*tcp) >> 2))
		goto error;

	/* Compute offset to data payload */
	pinfo->tcp_len = (tcp->th_off << 2);
	off += pinfo->tcp_len;

	/* Store more info */
	pinfo->data_off = off;
	pinfo->tcp = tcp;

	/* Try to save timestamp, if any */
	*phdr++ = tcp_tlro_info_save_timestamp(pinfo);

	/* Verify offset and IP/TCP length */
	if (off > m->m_pkthdr.len ||
	    pinfo->ip_len < pinfo->tcp_len)
		goto error;

	/* Compute data payload length */
	pinfo->data_len = (pinfo->ip_len - pinfo->tcp_len - pinfo->ip_hdrlen);

	/* Trim any padded data */
	diff = (m->m_pkthdr.len - off) - pinfo->data_len;
	if (diff != 0) {
		if (diff < 0)
			goto error;
		else
			m_adj(m, -diff);
	}
	/* Compute header length */
	pinfo->buf_length = phdr - (uint8_t *)pinfo->buf;
	/* Zero-pad rest of buffer */
	memset(phdr, 0, TLRO_MAX_HEADER - pinfo->buf_length);
	return;
error:
	pinfo->buf_length = 0;
}

static int
tcp_tlro_cmp64(const uint64_t *pa, const uint64_t *pb)
{
	int64_t diff = 0;
	unsigned x;

	for (x = 0; x != TLRO_MAX_HEADER / 8; x++) {
		/*
		 * NOTE: Endianness does not matter in this
		 * comparisation:
		 */
		diff = pa[x] - pb[x];
		if (diff != 0)
			goto done;
	}
done:
	if (diff < 0)
		return (-1);
	else if (diff > 0)
		return (1);
	return (0);
}

static int
tcp_tlro_compare_header(const void *_ppa, const void *_ppb)
{
	const struct tlro_mbuf_ptr *ppa = _ppa;
	const struct tlro_mbuf_ptr *ppb = _ppb;
	struct tlro_mbuf_data *pinfoa = ppa->data;
	struct tlro_mbuf_data *pinfob = ppb->data;
	int ret;

	ret = (pinfoa->head == NULL) - (pinfob->head == NULL);
	if (ret != 0)
		goto done;

	ret = pinfoa->buf_length - pinfob->buf_length;
	if (ret != 0)
		goto done;
	if (pinfoa->buf_length != 0) {
		ret = tcp_tlro_cmp64(pinfoa->buf, pinfob->buf);
		if (ret != 0)
			goto done;
		ret = ntohl(pinfoa->tcp->th_seq) - ntohl(pinfob->tcp->th_seq);
		if (ret != 0)
			goto done;
		ret = ntohl(pinfoa->tcp->th_ack) - ntohl(pinfob->tcp->th_ack);
		if (ret != 0)
			goto done;
		ret = pinfoa->sequence - pinfob->sequence;
		if (ret != 0)
			goto done;
	}
done:
	return (ret);
}

static void
tcp_tlro_sort(struct tlro_ctrl *tlro)
{
	if (tlro->curr == 0)
		return;

	qsort(tlro->mbuf, tlro->curr, sizeof(struct tlro_mbuf_ptr),
	    &tcp_tlro_compare_header);
}

static int
tcp_tlro_get_ticks(void)
{
	int to = tlro_min_rate;

	if (to < 1)
		to = 1;
	to = hz / to;
	if (to < 1)
		to = 1;
	return (to);
}

static void
tcp_tlro_combine(struct tlro_ctrl *tlro, int force)
{
	struct tlro_mbuf_data *pinfoa;
	struct tlro_mbuf_data *pinfob;
	uint32_t cs;
	int curr_ticks = ticks;
	int ticks_limit = tcp_tlro_get_ticks();
	unsigned x;
	unsigned y;
	unsigned z;
	int temp;

	if (tlro->curr == 0)
		return;

	for (y = 0; y != tlro->curr;) {
		struct mbuf *m;

		pinfoa = tlro->mbuf[y].data;
		for (x = y + 1; x != tlro->curr; x++) {
			pinfob = tlro->mbuf[x].data;
			if (pinfoa->buf_length != pinfob->buf_length ||
			    tcp_tlro_cmp64(pinfoa->buf, pinfob->buf) != 0)
				break;
		}
		if (pinfoa->buf_length == 0) {
			/* Forward traffic which cannot be combined */
			for (z = y; z != x; z++) {
				/* Just forward packets */
				pinfob = tlro->mbuf[z].data;

				m = pinfob->head;

				/* Reset info structure */
				pinfob->head = NULL;
				pinfob->buf_length = 0;

				/* Do stats */
				tlro->lro_flushed++;

				/* Input packet to network layer */
				(*tlro->ifp->if_input) (tlro->ifp, m);
			}
			y = z;
			continue;
		}

		/* Compute current checksum subtracted some header parts */
		temp = (pinfoa->ip_len - pinfoa->ip_hdrlen);
		cs = ((temp & 0xFF) << 8) + ((temp & 0xFF00) >> 8) +
		    tcp_tlro_csum((uint32_p_t *)pinfoa->tcp, pinfoa->tcp_len);

		/* Append all fragments into one block */
		for (z = y + 1; z != x; z++) {

			pinfob = tlro->mbuf[z].data;

			/* Check for command packets */
			if ((pinfoa->tcp->th_flags & ~(TH_ACK | TH_PUSH)) ||
			    (pinfob->tcp->th_flags & ~(TH_ACK | TH_PUSH)))
				break;

			/* Check if there is enough space */
			if ((pinfoa->ip_len + pinfob->data_len) > tlro_max_packet)
				break;

			/* Try to append the new segment */
			temp = ntohl(pinfoa->tcp->th_seq) + pinfoa->data_len;
			if (temp != (int)ntohl(pinfob->tcp->th_seq))
				break;

			temp = pinfob->ip_len - pinfob->ip_hdrlen;
			cs += ((temp & 0xFF) << 8) + ((temp & 0xFF00) >> 8) +
			    tcp_tlro_csum((uint32_p_t *)pinfob->tcp, pinfob->tcp_len);
			/* Remove fields which appear twice */
			cs += (IPPROTO_TCP << 8);
			if (pinfob->ip_version == 4) {
				cs += tcp_tlro_csum((uint32_p_t *)&pinfob->ip.v4->ip_src, 4);
				cs += tcp_tlro_csum((uint32_p_t *)&pinfob->ip.v4->ip_dst, 4);
			} else {
				cs += tcp_tlro_csum((uint32_p_t *)&pinfob->ip.v6->ip6_src, 16);
				cs += tcp_tlro_csum((uint32_p_t *)&pinfob->ip.v6->ip6_dst, 16);
			}
			/* Remainder computation */
			while (cs > 0xffff)
				cs = (cs >> 16) + (cs & 0xffff);

			/* Update window and ack sequence number */
			pinfoa->tcp->th_ack = pinfob->tcp->th_ack;
			pinfoa->tcp->th_win = pinfob->tcp->th_win;

			/* Check if we should restore the timestamp */
			tcp_tlro_info_restore_timestamp(pinfoa, pinfob);

			/* Accumulate TCP flags */
			pinfoa->tcp->th_flags |= pinfob->tcp->th_flags;

			/* update lengths */
			pinfoa->ip_len += pinfob->data_len;
			pinfoa->data_len += pinfob->data_len;

			/* Clear mbuf pointer - packet is accumulated */
			m = pinfob->head;

			/* Reset info structure */
			pinfob->head = NULL;
			pinfob->buf_length = 0;

			/* Append data to mbuf [y] */
			m_adj(m, pinfob->data_off);
			/* Delete mbuf tags, if any */
			m_tag_delete_chain(m, NULL);
			/* Clear packet header flag */
			m->m_flags &= ~M_PKTHDR;

			/* Concat mbuf(s) to end of list */
			pinfoa->pprev[0] = m;
			m = m_last(m);
			pinfoa->pprev = &m->m_next;
			pinfoa->head->m_pkthdr.len += pinfob->data_len;
		}
		/* Compute new TCP header checksum */
		pinfoa->tcp->th_sum = 0;

		temp = pinfoa->ip_len - pinfoa->ip_hdrlen;
		cs = (cs ^ 0xFFFF) +
		    tcp_tlro_csum((uint32_p_t *)pinfoa->tcp, pinfoa->tcp_len) +
		    ((temp & 0xFF) << 8) + ((temp & 0xFF00) >> 8);

		/* Remainder computation */
		while (cs > 0xffff)
			cs = (cs >> 16) + (cs & 0xffff);

		/* Update new checksum */
		pinfoa->tcp->th_sum = ~htole16(cs);

		/* Update IP length, if any */
		if (pinfoa->ip_version == 4) {
			if (pinfoa->ip_len > IP_MAXPACKET) {
				M_HASHTYPE_SET(pinfoa->head, M_HASHTYPE_LRO_TCP);
				pinfoa->ip.v4->ip_len = htons(IP_MAXPACKET);
			} else {
				pinfoa->ip.v4->ip_len = htons(pinfoa->ip_len);
			}
		} else {
			if (pinfoa->ip_len > (IP_MAXPACKET + sizeof(*pinfoa->ip.v6))) {
				M_HASHTYPE_SET(pinfoa->head, M_HASHTYPE_LRO_TCP);
				pinfoa->ip.v6->ip6_plen = htons(IP_MAXPACKET);
			} else {
				temp = pinfoa->ip_len - sizeof(*pinfoa->ip.v6);
				pinfoa->ip.v6->ip6_plen = htons(temp);
			}
		}

		temp = curr_ticks - pinfoa->last_tick;
		/* Check if packet should be forwarded */
		if (force != 0 || z != x || temp >= ticks_limit ||
		    pinfoa->data_len == 0) {

			/* Compute new IPv4 header checksum */
			if (pinfoa->ip_version == 4) {
				pinfoa->ip.v4->ip_sum = 0;
				cs = tcp_tlro_csum((uint32_p_t *)pinfoa->ip.v4,
				    sizeof(*pinfoa->ip.v4));
				pinfoa->ip.v4->ip_sum = ~htole16(cs);
			}
			/* Forward packet */
			m = pinfoa->head;

			/* Reset info structure */
			pinfoa->head = NULL;
			pinfoa->buf_length = 0;

			/* Do stats */
			tlro->lro_flushed++;

			/* Input packet to network layer */
			(*tlro->ifp->if_input) (tlro->ifp, m);
		}
		y = z;
	}

	/* Cleanup all NULL heads */
	for (y = 0; y != tlro->curr; y++) {
		if (tlro->mbuf[y].data->head == NULL) {
			for (z = y + 1; z != tlro->curr; z++) {
				struct tlro_mbuf_ptr ptemp;
				if (tlro->mbuf[z].data->head == NULL)
					continue;
				ptemp = tlro->mbuf[y];
				tlro->mbuf[y] = tlro->mbuf[z];
				tlro->mbuf[z] = ptemp;
				y++;
			}
			break;
		}
	}
	tlro->curr = y;
}

static void
tcp_tlro_cleanup(struct tlro_ctrl *tlro)
{
	while (tlro->curr != 0 &&
	    tlro->mbuf[tlro->curr - 1].data->head == NULL)
		tlro->curr--;
}

void
tcp_tlro_flush(struct tlro_ctrl *tlro, int force)
{
	if (tlro->curr == 0)
		return;

	tcp_tlro_sort(tlro);
	tcp_tlro_cleanup(tlro);
	tcp_tlro_combine(tlro, force);
}

int
tcp_tlro_init(struct tlro_ctrl *tlro, struct ifnet *ifp,
    int max_mbufs)
{
	ssize_t size;
	uint32_t x;

	/* Set zero defaults */
	memset(tlro, 0, sizeof(*tlro));

	/* Compute size needed for data */
	size = (sizeof(struct tlro_mbuf_ptr) * max_mbufs) +
	    (sizeof(struct tlro_mbuf_data) * max_mbufs);

	/* Range check */
	if (max_mbufs <= 0 || size <= 0 || ifp == NULL)
		return (EINVAL);

	/* Setup tlro control structure */
	tlro->mbuf = malloc(size, M_TLRO, M_WAITOK | M_ZERO);
	tlro->max = max_mbufs;
	tlro->ifp = ifp;

	/* Setup pointer array */
	for (x = 0; x != tlro->max; x++) {
		tlro->mbuf[x].data = ((struct tlro_mbuf_data *)
		    &tlro->mbuf[max_mbufs]) + x;
	}
	return (0);
}

void
tcp_tlro_free(struct tlro_ctrl *tlro)
{
	struct tlro_mbuf_data *pinfo;
	struct mbuf *m;
	uint32_t y;

	/* Check if not setup */
	if (tlro->mbuf == NULL)
		return;
	/* Free MBUF array and any leftover MBUFs */
	for (y = 0; y != tlro->max; y++) {

		pinfo = tlro->mbuf[y].data;

		m = pinfo->head;

		/* Reset info structure */
		pinfo->head = NULL;
		pinfo->buf_length = 0;

		m_freem(m);
	}
	free(tlro->mbuf, M_TLRO);
	/* Reset buffer */
	memset(tlro, 0, sizeof(*tlro));
}

void
tcp_tlro_rx(struct tlro_ctrl *tlro, struct mbuf *m)
{
	if (m->m_len > 0 && tlro->curr < tlro->max) {
		/* do stats */
		tlro->lro_queued++;

		/* extract header */
		tcp_tlro_extract_header(tlro->mbuf[tlro->curr++].data,
		    m, tlro->sequence++);
	} else if (tlro->ifp != NULL) {
		/* do stats */
		tlro->lro_flushed++;

		/* input packet to network layer */
		(*tlro->ifp->if_input) (tlro->ifp, m);
	} else {
		/* packet drop */
		m_freem(m);
	}
}
