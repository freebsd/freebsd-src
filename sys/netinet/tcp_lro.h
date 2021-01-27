/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2006, Myricom Inc.
 * Copyright (c) 2008, Intel Corporation.
 * Copyright (c) 2016 Mellanox Technologies.
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
 *
 * $FreeBSD$
 */

#ifndef _TCP_LRO_H_
#define _TCP_LRO_H_

#include <sys/time.h>

#ifndef TCP_LRO_ENTRIES
/* Define default number of LRO entries per RX queue */
#define	TCP_LRO_ENTRIES	8
#endif

/*
 * Flags for ACK entry for compression
 * the bottom 8 bits has the th_flags.
 * LRO itself adds only the TSTMP flags
 * to indicate if either of the types
 * of timestamps are filled and the
 * HAS_TSTMP option to indicate if the
 * TCP timestamp option is valid.
 *
 * The other 5 flag bits are for processing
 * by a stack.
 *
 */
#define TSTMP_LRO		0x0100
#define TSTMP_HDWR		0x0200
#define HAS_TSTMP		0x0400

/* Flags in LRO entry */
#define CAN_USE_ACKCMP		0x0001
#define HAS_COMP_ENTRIES	0x0002

struct inpcb;

struct lro_entry {
	LIST_ENTRY(lro_entry)	next;
	LIST_ENTRY(lro_entry)	hash_next;
	struct mbuf		*m_head;
	struct mbuf		*m_tail;
	struct mbuf		*m_last_mbuf;
	struct mbuf		*m_prev_last;
	struct inpcb 		*inp;
	union {
		struct ip	*ip4;
		struct ip6_hdr	*ip6;
	} leip;
	union {
		in_addr_t	s_ip4;
		struct in6_addr	s_ip6;
	} lesource;
	union {
		in_addr_t	d_ip4;
		struct in6_addr	d_ip6;
	} ledest;
	uint16_t		source_port;
	uint16_t		dest_port;
	uint16_t		eh_type;	/* EthernetHeader type. */
	uint16_t		append_cnt;
	uint32_t		p_len;		/* IP header payload length. */
	uint32_t		ulp_csum;	/* TCP, etc. checksum. */
	uint32_t		next_seq;	/* tcp_seq */
	uint32_t		ack_seq;	/* tcp_seq */
	uint32_t		tsval;
	uint32_t		tsecr;
	uint32_t		tcp_tot_p_len;	/* TCP payload length of chain */
	uint16_t		window;
	uint16_t		timestamp;	/* flag, not a TCP hdr field. */
	uint16_t		need_wakeup;
	uint16_t		mbuf_cnt;	/* Count of mbufs collected see note */
	uint16_t		mbuf_appended;
	uint16_t		cmp_ack_cnt;
	uint16_t		flags;
	uint16_t		strip_cnt;
	struct timeval		mtime;
};
/*
 * Note: The mbuf_cnt field tracks our number of mbufs added to the m_next
 *       list. Each mbuf counted can have data and of course it will
 *	 have an ack as well (by defintion any inbound tcp segment will
 *	 have an ack value. We use this count to tell us how many ACK's
 *	 are present for our ack-count threshold. If we exceed that or
 *	 the data threshold we will wake up the endpoint.
 */
LIST_HEAD(lro_head, lro_entry);

#define	le_ip4			leip.ip4
#define	le_ip6			leip.ip6
#define	source_ip4		lesource.s_ip4
#define	dest_ip4		ledest.d_ip4
#define	source_ip6		lesource.s_ip6
#define	dest_ip6		ledest.d_ip6

struct lro_mbuf_sort {
	uint64_t seq;
	struct mbuf *mb;
};

/* NB: This is part of driver structs. */
struct lro_ctrl {
	struct ifnet	*ifp;
	struct lro_mbuf_sort *lro_mbuf_data;
	struct timeval lro_last_flush;
	uint64_t	lro_queued;
	uint64_t	lro_flushed;
	uint64_t	lro_bad_csum;
	unsigned	lro_cnt;
	unsigned	lro_mbuf_count;
	unsigned	lro_mbuf_max;
	unsigned short	lro_ackcnt_lim;		/* max # of aggregated ACKs */
	unsigned 	lro_length_lim;		/* max len of aggregated data */

	u_long		lro_hashsz;
	struct lro_head	*lro_hash;
	struct lro_head	lro_active;
	struct lro_head	lro_free;
};

struct tcp_ackent {
	uint64_t timestamp;	/* hardware or sofware timestamp, valid if TSTMP_LRO or TSTMP_HDRW set */
	uint32_t seq;		/* th_seq value */
	uint32_t ack;		/* th_ack value */
	uint32_t ts_value;	/* If ts option value, valid if HAS_TSTMP is set */
	uint32_t ts_echo;	/* If ts option echo, valid if HAS_TSTMP is set */
	uint16_t win;		/* TCP window */
	uint16_t flags;		/* Flags to say if TS is present and type of timestamp and th_flags */
	uint8_t  codepoint;	/* IP level codepoint including ECN bits */
	uint8_t  ack_val_set;	/* Classification of ack used by the stack */
	uint8_t  pad[2];	/* To 32 byte boundary */
};

/* We use two M_PROTO on the mbuf */
#define M_ACKCMP	M_PROTO4   /* Indicates LRO is sending in a  Ack-compression mbuf */
#define M_LRO_EHDRSTRP	M_PROTO6   /* Indicates that LRO has stripped the etherenet header */

#define	TCP_LRO_LENGTH_MAX	65535
#define	TCP_LRO_ACKCNT_MAX	65535		/* unlimited */

int tcp_lro_init(struct lro_ctrl *);
int tcp_lro_init_args(struct lro_ctrl *, struct ifnet *, unsigned, unsigned);
void tcp_lro_free(struct lro_ctrl *);
void tcp_lro_flush_inactive(struct lro_ctrl *, const struct timeval *);
void tcp_lro_flush(struct lro_ctrl *, struct lro_entry *);
void tcp_lro_flush_all(struct lro_ctrl *);
int tcp_lro_rx(struct lro_ctrl *, struct mbuf *, uint32_t);
void tcp_lro_queue_mbuf(struct lro_ctrl *, struct mbuf *);
void tcp_lro_reg_mbufq(void);
void tcp_lro_dereg_mbufq(void);

#define	TCP_LRO_NO_ENTRIES	-2
#define	TCP_LRO_CANNOT		-1
#define	TCP_LRO_NOT_SUPPORTED	1

#endif /* _TCP_LRO_H_ */
