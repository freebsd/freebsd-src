/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 *
 * $FreeBSD$
 */
/*
 * Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2013 Mark Johnston <markj@freebsd.org>
 */

#pragma D depends_on library ip.d
#pragma D depends_on module kernel
#pragma D depends_on provider tcp

/*
 * Convert a TCP state value to a string.
 */
#pragma D binding "1.6.3" TCPS_CLOSED
inline int TCPS_CLOSED =	0;
#pragma D binding "1.6.3" TCPS_LISTEN
inline int TCPS_LISTEN =	1;
#pragma D binding "1.6.3" TCPS_SYN_SENT
inline int TCPS_SYN_SENT =	2;
#pragma D binding "1.6.3" TCPS_SYN_RECEIVED
inline int TCPS_SYN_RECEIVED =	3;
#pragma D binding "1.6.3" TCPS_ESTABLISHED
inline int TCPS_ESTABLISHED =	4;
#pragma D binding "1.6.3" TCPS_CLOSE_WAIT
inline int TCPS_CLOSE_WAIT =	5;
#pragma D binding "1.6.3" TCPS_FIN_WAIT_1
inline int TCPS_FIN_WAIT_1 =	6;
#pragma D binding "1.6.3" TCPS_CLOSING
inline int TCPS_CLOSING =	7;
#pragma D binding "1.6.3" TCPS_LAST_ACK
inline int TCPS_LAST_ACK =	8;
#pragma D binding "1.6.3" TCPS_FIN_WAIT_2
inline int TCPS_FIN_WAIT_2 =	9;
#pragma D binding "1.6.3" TCPS_TIME_WAIT
inline int TCPS_TIME_WAIT =	10;

/* TCP segment flags. */
#pragma D binding "1.6.3" TH_FIN
inline uint8_t TH_FIN =		0x01;
#pragma D binding "1.6.3" TH_SYN
inline uint8_t TH_SYN =		0x02;
#pragma D binding "1.6.3" TH_RST
inline uint8_t TH_RST =		0x04;
#pragma D binding "1.6.3" TH_PUSH
inline uint8_t TH_PUSH =	0x08;
#pragma D binding "1.6.3" TH_ACK
inline uint8_t TH_ACK =		0x10;
#pragma D binding "1.6.3" TH_URG
inline uint8_t TH_URG =		0x20;
#pragma D binding "1.6.3" TH_ECE
inline uint8_t TH_ECE =		0x40;
#pragma D binding "1.6.3" TH_CWR
inline uint8_t TH_CWR =		0x80;

/* TCP connection state strings. */
#pragma D binding "1.6.3" tcp_state_string
inline string tcp_state_string[int32_t state] =
	state == TCPS_CLOSED ?		"state-closed" :
	state == TCPS_LISTEN ?		"state-listen" :
	state == TCPS_SYN_SENT ?	"state-syn-sent" :
	state == TCPS_SYN_RECEIVED ?	"state-syn-received" :
	state == TCPS_ESTABLISHED ?	"state-established" :
	state == TCPS_CLOSE_WAIT ?	"state-close-wait" :
	state == TCPS_FIN_WAIT_1 ?	"state-fin-wait-1" :
	state == TCPS_CLOSING ?		"state-closing" :
	state == TCPS_LAST_ACK ?	"state-last-ack" :
	state == TCPS_FIN_WAIT_2 ?	"state-fin-wait-2" :
	state == TCPS_TIME_WAIT ?	"state-time-wait" :
	"<unknown>";

/*
 * tcpsinfo contains stable TCP details from tcp_t.
 */
typedef struct tcpsinfo {
	uintptr_t tcps_addr;
	int tcps_local;			/* is delivered locally, boolean */
	int tcps_active;		/* active open (from here), boolean */
	uint16_t tcps_lport;		/* local port */
	uint16_t tcps_rport;		/* remote port */
	string tcps_laddr;		/* local address, as a string */
	string tcps_raddr;		/* remote address, as a string */
	int32_t tcps_state;		/* TCP state */
	uint32_t tcps_iss;		/* Initial sequence # sent */
	uint32_t tcps_suna;		/* sequence # sent but unacked */
	uint32_t tcps_snxt;		/* next sequence # to send */
	uint32_t tcps_rack;		/* sequence # we have acked */
	uint32_t tcps_rnxt;		/* next sequence # expected */
	uint32_t tcps_swnd;		/* send window size */
	int32_t tcps_snd_ws;		/* send window scaling */
	uint32_t tcps_rwnd;		/* receive window size */
	int32_t tcps_rcv_ws;		/* receive window scaling */
	uint32_t tcps_cwnd;		/* congestion window */
	uint32_t tcps_cwnd_ssthresh;	/* threshold for congestion avoidance */
	uint32_t tcps_sack_fack;	/* SACK sequence # we have acked */
	uint32_t tcps_sack_snxt;	/* next SACK seq # for retransmission */
	uint32_t tcps_rto;		/* round-trip timeout, msec */
	uint32_t tcps_mss;		/* max segment size */
	int tcps_retransmit;		/* retransmit send event, boolean */
	int tcps_srtt;                  /* smoothed RTT in units of (TCP_RTT_SCALE*hz) */
} tcpsinfo_t;

/*
 * tcplsinfo provides the old tcp state for state changes.
 */
typedef struct tcplsinfo {
	int32_t tcps_state;		/* previous TCP state */
} tcplsinfo_t;

/*
 * tcpinfo is the TCP header fields.
 */
typedef struct tcpinfo {
	uint16_t tcp_sport;		/* source port */
	uint16_t tcp_dport;		/* destination port */
	uint32_t tcp_seq;		/* sequence number */
	uint32_t tcp_ack;		/* acknowledgment number */
	uint8_t tcp_offset;		/* data offset, in bytes */
	uint8_t tcp_flags;		/* flags */
	uint16_t tcp_window;		/* window size */
	uint16_t tcp_checksum;		/* checksum */
	uint16_t tcp_urgent;		/* urgent data pointer */
	struct tcphdr *tcp_hdr;		/* raw TCP header */
} tcpinfo_t;

/*
 * A clone of tcpinfo_t used to handle the fact that the TCP input path
 * overwrites some fields of the TCP header with their host-order equivalents.
 * Unfortunately, DTrace doesn't let us simply typedef a new name for struct
 * tcpinfo and define a separate translator for it.
 */
typedef struct tcpinfoh {
	uint16_t tcp_sport;		/* source port */
	uint16_t tcp_dport;		/* destination port */
	uint32_t tcp_seq;		/* sequence number */
	uint32_t tcp_ack;		/* acknowledgment number */
	uint8_t tcp_offset;		/* data offset, in bytes */
	uint8_t tcp_flags;		/* flags */
	uint16_t tcp_window;		/* window size */
	uint16_t tcp_checksum;		/* checksum */
	uint16_t tcp_urgent;		/* urgent data pointer */
	struct tcphdr *tcp_hdr;		/* raw TCP header */
} tcpinfoh_t;

#pragma D binding "1.6.3" translator
translator csinfo_t < struct tcpcb *p > {
	cs_addr =	NULL;
	cs_cid =	(uint64_t)(p == NULL ? 0 : p->t_inpcb);
	cs_pid =	0;
	cs_zoneid =	0;
};

#pragma D binding "1.6.3" translator
translator tcpsinfo_t < struct tcpcb *p > {
	tcps_addr =		(uintptr_t)p;
	tcps_local =		-1; /* XXX */
	tcps_active =		-1; /* XXX */
	tcps_lport =		p == NULL ? 0 : ntohs(p->t_inpcb->inp_inc.inc_ie.ie_lport);
	tcps_rport =		p == NULL ? 0 : ntohs(p->t_inpcb->inp_inc.inc_ie.ie_fport);
	tcps_laddr =		p == NULL ? 0 :
	    p->t_inpcb->inp_vflag == INP_IPV4 ?
	    inet_ntoa(&p->t_inpcb->inp_inc.inc_ie.ie_dependladdr.ie46_local.ia46_addr4.s_addr) :
	    inet_ntoa6(&p->t_inpcb->inp_inc.inc_ie.ie_dependladdr.ie6_local);
	tcps_raddr =		p == NULL ? 0 :
	    p->t_inpcb->inp_vflag == INP_IPV4 ?
	    inet_ntoa(&p->t_inpcb->inp_inc.inc_ie.ie_dependfaddr.ie46_foreign.ia46_addr4.s_addr) :
	    inet_ntoa6(&p->t_inpcb->inp_inc.inc_ie.ie_dependfaddr.ie6_foreign);
	tcps_state =		p == NULL ? -1 : p->t_state;
	tcps_iss =		p == NULL ? 0  : p->iss;
	tcps_suna =		p == NULL ? 0  : p->snd_una;
	tcps_snxt =		p == NULL ? 0  : p->snd_nxt;
	tcps_rack =		p == NULL ? 0  : p->last_ack_sent;
	tcps_rnxt =		p == NULL ? 0  : p->rcv_nxt;
	tcps_swnd =		p == NULL ? -1  : p->snd_wnd;
	tcps_snd_ws =		p == NULL ? -1  : p->snd_scale;
	tcps_rwnd =		p == NULL ? -1  : p->rcv_wnd;
	tcps_rcv_ws =		p == NULL ? -1  : p->rcv_scale;
	tcps_cwnd =		p == NULL ? -1  : p->snd_cwnd;
	tcps_cwnd_ssthresh =	p == NULL ? -1  : p->snd_ssthresh;
	tcps_sack_fack =	p == NULL ? 0  : p->snd_fack;
	tcps_sack_snxt =	p == NULL ? 0  : p->sack_newdata;
	tcps_rto =		p == NULL ? -1 : (p->t_rxtcur * 1000) / `hz;
	tcps_mss =		p == NULL ? -1  : p->t_maxseg;
	tcps_retransmit =	p == NULL ? -1 : p->t_rxtshift > 0 ? 1 : 0;
	tcps_srtt =             p == NULL ? -1  : p->t_srtt;   /* smoothed RTT in units of (TCP_RTT_SCALE*hz) */
};

#pragma D binding "1.6.3" translator
translator tcpinfo_t < struct tcphdr *p > {
	tcp_sport =	p == NULL ? 0  : ntohs(p->th_sport);
	tcp_dport =	p == NULL ? 0  : ntohs(p->th_dport);
	tcp_seq =	p == NULL ? -1 : ntohl(p->th_seq);
	tcp_ack =	p == NULL ? -1 : ntohl(p->th_ack);
	tcp_offset =	p == NULL ? -1 : (p->th_off >> 2);
	tcp_flags =	p == NULL ? 0  : p->th_flags;
	tcp_window =	p == NULL ? 0  : ntohs(p->th_win);
	tcp_checksum =	p == NULL ? 0  : ntohs(p->th_sum);
	tcp_urgent =	p == NULL ? 0  : ntohs(p->th_urp);
	tcp_hdr =	(struct tcphdr *)p;
};

/*
 * This translator differs from the one for tcpinfo_t in that the sequence
 * number, acknowledgement number, window size and urgent pointer are already
 * in host order and thus don't need to be converted.
 */
#pragma D binding "1.6.3" translator
translator tcpinfoh_t < struct tcphdr *p > {
	tcp_sport =	p == NULL ? 0  : ntohs(p->th_sport);
	tcp_dport =	p == NULL ? 0  : ntohs(p->th_dport);
	tcp_seq =	p == NULL ? -1 : p->th_seq;
	tcp_ack =	p == NULL ? -1 : p->th_ack;
	tcp_offset =	p == NULL ? -1 : (p->th_off >> 2);
	tcp_flags =	p == NULL ? 0  : p->th_flags;
	tcp_window =	p == NULL ? 0  : (p->th_win);
	tcp_checksum =	p == NULL ? 0  : ntohs(p->th_sum);
	tcp_urgent =	p == NULL ? 0  : p->th_urp;
	tcp_hdr =	(struct tcphdr *)p;
};

#pragma D binding "1.6.3" translator
translator tcplsinfo_t < int s > {
	tcps_state =	s;
};
