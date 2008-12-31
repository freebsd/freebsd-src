/*******************************************************************************

Copyright (c) 2006, Myricom Inc.
Copyright (c) 2008, Intel Corporation.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Myricom Inc, nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

 2. Neither the name of the Intel Corporation, nor the names of its
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


$FreeBSD: src/sys/netinet/tcp_lro.h,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

***************************************************************************/
#ifndef _TCP_LRO_H_
#define _TCP_LRO_H_

struct lro_entry;
struct lro_entry
{
	SLIST_ENTRY(lro_entry) next;
	struct mbuf  	*m_head;
	struct mbuf	*m_tail;
	int		timestamp;
	struct ip	*ip;
	uint32_t	tsval;
	uint32_t	tsecr;
	uint32_t	source_ip;
	uint32_t	dest_ip;
	uint32_t	next_seq;
	uint32_t	ack_seq;
	uint32_t	len;
	uint32_t	data_csum;
	uint16_t	window;
	uint16_t	source_port;
	uint16_t	dest_port;
	uint16_t	append_cnt;
	uint16_t	mss;
	
};
SLIST_HEAD(lro_head, lro_entry);

struct lro_ctrl {
	struct ifnet	*ifp;
	int		lro_queued;
	int		lro_flushed;
	int		lro_bad_csum;
	int		lro_cnt;

	struct lro_head	lro_active;
	struct lro_head	lro_free;
};


int tcp_lro_init(struct lro_ctrl *);
void tcp_lro_free(struct lro_ctrl *);
void tcp_lro_flush(struct lro_ctrl *, struct lro_entry *);
int tcp_lro_rx(struct lro_ctrl *, struct mbuf *, uint32_t);

/* Number of LRO entries - these are per rx queue */
#define LRO_ENTRIES			8

#endif /* _TCP_LRO_H_ */
