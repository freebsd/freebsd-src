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
 *
 * $FreeBSD$
 */

#ifndef _TCP_TLRO_H_
#define	_TCP_TLRO_H_

#define	TLRO_MAX_HEADER	64		/* bytes */

struct ip;
struct ip6_hdr;
struct tcphdr;

struct tlro_mbuf_data {
	union {
#ifdef INET
		struct ip *v4;
#endif
#ifdef INET6
		struct ip6_hdr *v6;
#endif
	} ip;
	struct tcphdr *tcp;
	struct mbuf *head;
	struct mbuf **pprev;
	int	last_tick;
	int	sequence;
	int	data_len;
	int	data_off;
	int	ip_hdrlen;
	int	ip_len;
	uint32_t tcp_ts;
	uint32_t tcp_ts_reply;
	uint16_t tcp_len;
	uint8_t	ip_version;
	uint8_t	buf_length;		/* in 32-bit words */
	uint64_t buf[TLRO_MAX_HEADER / 8];
} __aligned(256);

struct tlro_mbuf_ptr {
	struct tlro_mbuf_data *data;
};

/* NB: This is part of driver structs */
struct tlro_ctrl {
	struct ifnet *ifp;
	struct tlro_mbuf_ptr *mbuf;
	uint64_t lro_queued;
	uint64_t lro_flushed;
	uint32_t max;
	uint32_t curr;
	int	sequence;
};

int	tcp_tlro_init(struct tlro_ctrl *, struct ifnet *, int);
void	tcp_tlro_free(struct tlro_ctrl *);
void	tcp_tlro_flush(struct tlro_ctrl *, int);
void	tcp_tlro_rx(struct tlro_ctrl *, struct mbuf *);

#endif					/* _TCP_TLRO_H_ */
