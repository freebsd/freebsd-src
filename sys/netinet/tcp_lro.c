/******************************************************************************

Copyright (c) 2007, Myricom Inc.
Copyright (c) 2008, Intel Corporation.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Myricom Inc, nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

 3. Neither the name of the Intel Corporation, nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

$FreeBSD$ 
***************************************************************************/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_media.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/tcp_lro.h>

#include <machine/bus.h>
#include <machine/in_cksum.h>


static uint16_t do_csum_data(uint16_t *raw, int len)
{
	uint32_t csum;
	csum = 0;
	while (len > 0) {
		csum += *raw;
		raw++;
		csum += *raw;
		raw++;
		len -= 4;
	}
	csum = (csum >> 16) + (csum & 0xffff);
	csum = (csum >> 16) + (csum & 0xffff);
	return (uint16_t)csum;
}

/*
 * Allocate and init the LRO data structures
 */
int
tcp_lro_init(struct lro_ctrl *cntl)
{
	struct lro_entry *lro;
	int i, error = 0;

	SLIST_INIT(&cntl->lro_free);
	SLIST_INIT(&cntl->lro_active);

	cntl->lro_bad_csum = 0;
	cntl->lro_queued = 0;
	cntl->lro_flushed = 0;

	for (i = 0; i < LRO_ENTRIES; i++) {
                lro = (struct lro_entry *) malloc(sizeof (struct lro_entry),
		    M_DEVBUF, M_NOWAIT | M_ZERO);
                if (lro == NULL) {
			if (i == 0)
				error = ENOMEM;
                        break;
                }
		cntl->lro_cnt = i;
                SLIST_INSERT_HEAD(&cntl->lro_free, lro, next);
        }

	return (error);
}

void
tcp_lro_free(struct lro_ctrl *cntl)
{
	struct lro_entry *entry;

	while (!SLIST_EMPTY(&cntl->lro_free)) {
		entry = SLIST_FIRST(&cntl->lro_free);
               	SLIST_REMOVE_HEAD(&cntl->lro_free, next);
		free(entry, M_DEVBUF);
	}
}

void
tcp_lro_flush(struct lro_ctrl *cntl, struct lro_entry *lro)
{
	struct ifnet *ifp;
	struct ip *ip;
	struct tcphdr *tcp;
	uint32_t *ts_ptr;
	uint32_t tcplen, tcp_csum;


	if (lro->append_cnt) {
		/* incorporate the new len into the ip header and
		 * re-calculate the checksum */
		ip = lro->ip;
		ip->ip_len = htons(lro->len - ETHER_HDR_LEN);
		ip->ip_sum = 0;
		ip->ip_sum = 0xffff ^ 
			do_csum_data((uint16_t*)ip,
					      sizeof (*ip));

		lro->m_head->m_pkthdr.csum_flags = CSUM_IP_CHECKED |
			CSUM_IP_VALID | CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
		lro->m_head->m_pkthdr.csum_data = 0xffff;
		lro->m_head->m_pkthdr.len = lro->len;

		/* incorporate the latest ack into the tcp header */
		tcp = (struct tcphdr *) (ip + 1);
		tcp->th_ack = lro->ack_seq;
		tcp->th_win = lro->window;
		/* incorporate latest timestamp into the tcp header */
		if (lro->timestamp) {
			ts_ptr = (uint32_t *)(tcp + 1);
			ts_ptr[1] = htonl(lro->tsval);
			ts_ptr[2] = lro->tsecr;
		}
		/* 
		 * update checksum in tcp header by re-calculating the
		 * tcp pseudoheader checksum, and adding it to the checksum
		 * of the tcp payload data 
		 */
		tcp->th_sum = 0;
		tcplen = lro->len - sizeof(*ip) - ETHER_HDR_LEN;
		tcp_csum = lro->data_csum;
		tcp_csum += in_pseudo(ip->ip_src.s_addr, ip->ip_dst.s_addr,
				      htons(tcplen + IPPROTO_TCP));
		tcp_csum += do_csum_data((uint16_t*)tcp,
						  tcp->th_off << 2);
		tcp_csum = (tcp_csum & 0xffff) + (tcp_csum >> 16);
		tcp_csum = (tcp_csum & 0xffff) + (tcp_csum >> 16);
		tcp->th_sum = 0xffff ^ tcp_csum;
	}
	ifp = cntl->ifp;
	(*ifp->if_input)(cntl->ifp, lro->m_head);
	cntl->lro_queued += lro->append_cnt + 1;
	cntl->lro_flushed++;
	lro->m_head = NULL;
	lro->timestamp = 0;
	lro->append_cnt = 0;
	SLIST_INSERT_HEAD(&cntl->lro_free, lro, next);
}

int
tcp_lro_rx(struct lro_ctrl *cntl, struct mbuf *m_head, uint32_t csum)
{
	struct ether_header *eh;
	struct ip *ip;
	struct tcphdr *tcp;
	uint32_t *ts_ptr;
	struct mbuf *m_nxt, *m_tail;
	struct lro_entry *lro;
	int hlen, ip_len, tcp_hdr_len, tcp_data_len, tot_len;
	int opt_bytes, trim, csum_flags;
	uint32_t seq, tmp_csum, device_mtu;


	eh = mtod(m_head, struct ether_header *);
	if (eh->ether_type != htons(ETHERTYPE_IP))
		return 1;
	ip = (struct ip *) (eh + 1);
	if (ip->ip_p != IPPROTO_TCP)
		return 1;
	
	/* ensure there are no options */
	if ((ip->ip_hl << 2) != sizeof (*ip))
		return -1;

	/* .. and the packet is not fragmented */
	if (ip->ip_off & htons(IP_MF|IP_OFFMASK))
		return -1;

	/* verify that the IP header checksum is correct */
	csum_flags = m_head->m_pkthdr.csum_flags;
	if (csum_flags & CSUM_IP_CHECKED) {
		if (__predict_false((csum_flags & CSUM_IP_VALID) == 0)) {
			cntl->lro_bad_csum++;
			return -1;
		}
	} else {
		tmp_csum = do_csum_data((uint16_t *)ip, sizeof (*ip));
		if (__predict_false((tmp_csum ^ 0xffff) != 0)) {
			cntl->lro_bad_csum++;
			return -1;
		}
	}
	
	/* find the TCP header */
	tcp = (struct tcphdr *) (ip + 1);

	/* Get the TCP checksum if we dont have it */
	if (!csum)
		csum = tcp->th_sum;

	/* ensure no bits set besides ack or psh */
	if ((tcp->th_flags & ~(TH_ACK | TH_PUSH)) != 0)
		return -1;

	/* check for timestamps. Since the only option we handle are
	   timestamps, we only have to handle the simple case of
	   aligned timestamps */

	opt_bytes = (tcp->th_off << 2) - sizeof (*tcp);
	tcp_hdr_len =  sizeof (*tcp) + opt_bytes;
	ts_ptr = (uint32_t *)(tcp + 1);
	if (opt_bytes != 0) {
		if (__predict_false(opt_bytes != TCPOLEN_TSTAMP_APPA) ||
		    (*ts_ptr !=  ntohl(TCPOPT_NOP<<24|TCPOPT_NOP<<16|
		    TCPOPT_TIMESTAMP<<8|TCPOLEN_TIMESTAMP)))
			return -1;
	}

	ip_len = ntohs(ip->ip_len);
	tcp_data_len = ip_len - (tcp->th_off << 2) - sizeof (*ip);
	

	/* 
	 * If frame is padded beyond the end of the IP packet,
	 * then we must trim the extra bytes off the end.
	 */
	tot_len = m_head->m_pkthdr.len;
	trim = tot_len - (ip_len + ETHER_HDR_LEN);
	if (trim != 0) {
		if (trim < 0) {
			/* truncated packet */
			return -1;
		}
		m_adj(m_head, -trim);
		tot_len = m_head->m_pkthdr.len;
	}

	m_nxt = m_head;
	m_tail = NULL; /* -Wuninitialized */
	while (m_nxt != NULL) {
		m_tail = m_nxt;
		m_nxt = m_tail->m_next;
	}

	hlen = ip_len + ETHER_HDR_LEN - tcp_data_len;
	seq = ntohl(tcp->th_seq);

	SLIST_FOREACH(lro, &cntl->lro_active, next) {
		if (lro->source_port == tcp->th_sport && 
		    lro->dest_port == tcp->th_dport &&
		    lro->source_ip == ip->ip_src.s_addr && 
		    lro->dest_ip == ip->ip_dst.s_addr) {
			/* Try to append it */

			if (__predict_false(seq != lro->next_seq)) {
				/* out of order packet */
				SLIST_REMOVE(&cntl->lro_active, lro,
					     lro_entry, next);
				tcp_lro_flush(cntl, lro);
				return -1;
			}

			if (opt_bytes) {
				uint32_t tsval = ntohl(*(ts_ptr + 1));
				/* make sure timestamp values are increasing */
				if (__predict_false(lro->tsval > tsval || 
					     *(ts_ptr + 2) == 0)) {
					return -1;
				}
				lro->tsval = tsval;
				lro->tsecr = *(ts_ptr + 2);
			}

			lro->next_seq += tcp_data_len;
			lro->ack_seq = tcp->th_ack;
			lro->window = tcp->th_win;
			lro->append_cnt++;
			if (tcp_data_len == 0) {
				m_freem(m_head);
				return 0;
			}
			/* subtract off the checksum of the tcp header
                         * from the hardware checksum, and add it to the
                         * stored tcp data checksum.  Byteswap the checksum
			 * if the total length so far is odd 
                         */
			tmp_csum = do_csum_data((uint16_t*)tcp,
							 tcp_hdr_len);
			csum = csum + (tmp_csum ^ 0xffff);
			csum = (csum & 0xffff) + (csum >> 16);
			csum = (csum & 0xffff) + (csum >> 16);
			if (lro->len & 0x1) {
				/* Odd number of bytes so far, flip bytes */
				csum = ((csum << 8) | (csum >> 8)) & 0xffff;
			}
			csum = csum + lro->data_csum;
			csum = (csum & 0xffff) + (csum >> 16);
			csum = (csum & 0xffff) + (csum >> 16);
			lro->data_csum = csum;

			lro->len += tcp_data_len;

			/* adjust mbuf so that m->m_data points to
			   the first byte of the payload */
			m_adj(m_head, hlen);
			/* append mbuf chain */
			lro->m_tail->m_next = m_head;
			/* advance the last pointer */
			lro->m_tail = m_tail;
			/* flush packet if required */
			device_mtu = cntl->ifp->if_mtu;
			if (lro->len > (65535 - device_mtu)) {
				SLIST_REMOVE(&cntl->lro_active, lro,
					     lro_entry, next);
				tcp_lro_flush(cntl, lro);
			}
			return 0;
		}
	}

	if (SLIST_EMPTY(&cntl->lro_free))
	    return -1;

	/* start a new chain */
	lro = SLIST_FIRST(&cntl->lro_free);
	SLIST_REMOVE_HEAD(&cntl->lro_free, next);
	SLIST_INSERT_HEAD(&cntl->lro_active, lro, next);
	lro->source_port = tcp->th_sport;
	lro->dest_port = tcp->th_dport;
	lro->source_ip = ip->ip_src.s_addr;
	lro->dest_ip = ip->ip_dst.s_addr;
	lro->next_seq = seq + tcp_data_len;
	lro->mss = tcp_data_len;
	lro->ack_seq = tcp->th_ack;
	lro->window = tcp->th_win;

	/* save the checksum of just the TCP payload by
	 * subtracting off the checksum of the TCP header from
	 * the entire hardware checksum 
	 * Since IP header checksum is correct, checksum over
	 * the IP header is -0.  Substracting -0 is unnecessary.
	 */
	tmp_csum = do_csum_data((uint16_t*)tcp, tcp_hdr_len);
	csum = csum + (tmp_csum ^ 0xffff);
	csum = (csum & 0xffff) + (csum >> 16);
	csum = (csum & 0xffff) + (csum >> 16);
	lro->data_csum = csum;
	
	lro->ip = ip;
	/* record timestamp if it is present */
	if (opt_bytes) {
		lro->timestamp = 1;
		lro->tsval = ntohl(*(ts_ptr + 1));
		lro->tsecr = *(ts_ptr + 2);
	}
	lro->len = tot_len;
	lro->m_head = m_head;
	lro->m_tail = m_tail;
	return 0;
}
