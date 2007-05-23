/*-
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)tcp.h	8.1 (Berkeley) 6/10/93
 * $FreeBSD$
 */

#ifndef _NETINET_TCP_H_
#define _NETINET_TCP_H_

#include <sys/cdefs.h>

#if __BSD_VISIBLE

typedef	u_int32_t tcp_seq;

#define tcp6_seq	tcp_seq	/* for KAME src sync over BSD*'s */
#define tcp6hdr		tcphdr	/* for KAME src sync over BSD*'s */

/*
 * TCP header.
 * Per RFC 793, September, 1981.
 */
struct tcphdr {
	u_short	th_sport;		/* source port */
	u_short	th_dport;		/* destination port */
	tcp_seq	th_seq;			/* sequence number */
	tcp_seq	th_ack;			/* acknowledgement number */
#if BYTE_ORDER == LITTLE_ENDIAN
	u_int	th_x2:4,		/* (unused) */
		th_off:4;		/* data offset */
#endif
#if BYTE_ORDER == BIG_ENDIAN
	u_int	th_off:4,		/* data offset */
		th_x2:4;		/* (unused) */
#endif
	u_char	th_flags;
#define	TH_FIN	0x01
#define	TH_SYN	0x02
#define	TH_RST	0x04
#define	TH_PUSH	0x08
#define	TH_ACK	0x10
#define	TH_URG	0x20
#define	TH_ECE	0x40
#define	TH_CWR	0x80
#define	TH_FLAGS	(TH_FIN|TH_SYN|TH_RST|TH_PUSH|TH_ACK|TH_URG|TH_ECE|TH_CWR)
#define	PRINT_TH_FLAGS	"\20\1FIN\2SYN\3RST\4PUSH\5ACK\6URG\7ECE\8CWR"

	u_short	th_win;			/* window */
	u_short	th_sum;			/* checksum */
	u_short	th_urp;			/* urgent pointer */
};

#define	TCPOPT_EOL		0
#define	   TCPOLEN_EOL			1
#define	TCPOPT_NOP		1
#define	   TCPOLEN_NOP			1
#define	TCPOPT_MAXSEG		2
#define    TCPOLEN_MAXSEG		4
#define TCPOPT_WINDOW		3
#define    TCPOLEN_WINDOW		3
#define TCPOPT_SACK_PERMITTED	4
#define    TCPOLEN_SACK_PERMITTED	2
#define TCPOPT_SACK		5
#define	   TCPOLEN_SACKHDR		2
#define    TCPOLEN_SACK			8	/* 2*sizeof(tcp_seq) */
#define TCPOPT_TIMESTAMP	8
#define    TCPOLEN_TIMESTAMP		10
#define    TCPOLEN_TSTAMP_APPA		(TCPOLEN_TIMESTAMP+2) /* appendix A */
#define	TCPOPT_SIGNATURE	19		/* Keyed MD5: RFC 2385 */
#define	   TCPOLEN_SIGNATURE		18

/* Miscellaneous constants */
#define	MAX_SACK_BLKS	6	/* Max # SACK blocks stored at receiver side */
#define	TCP_MAX_SACK	4	/* MAX # SACKs sent in any segment */


/*
 * Default maximum segment size for TCP.
 * With an IP MTU of 576, this is 536,
 * but 512 is probably more convenient.
 * This should be defined as MIN(512, IP_MSS - sizeof (struct tcpiphdr)).
 */
#define	TCP_MSS	512
/*
 * TCP_MINMSS is defined to be 216 which is fine for the smallest
 * link MTU (256 bytes, AX.25 packet radio) in the Internet.
 * However it is very unlikely to come across such low MTU interfaces
 * these days (anno dato 2003).
 * See tcp_subr.c tcp_minmss SYSCTL declaration for more comments.
 * Setting this to "0" disables the minmss check.
 */
#define	TCP_MINMSS 216

/*
 * Default maximum segment size for TCP6.
 * With an IP6 MSS of 1280, this is 1220,
 * but 1024 is probably more convenient. (xxx kazu in doubt)
 * This should be defined as MIN(1024, IP6_MSS - sizeof (struct tcpip6hdr))
 */
#define	TCP6_MSS	1024

#define	TCP_MAXWIN	65535	/* largest value for (unscaled) window */
#define	TTCP_CLIENT_SND_WND	4096	/* dflt send window for T/TCP client */

#define TCP_MAX_WINSHIFT	14	/* maximum window shift */

#define TCP_MAXBURST		4	/* maximum segments in a burst */

#define TCP_MAXHLEN	(0xf<<2)	/* max length of header in bytes */
#define TCP_MAXOLEN	(TCP_MAXHLEN - sizeof(struct tcphdr))
					/* max space left for options */
#endif /* __BSD_VISIBLE */

/*
 * User-settable options (used with setsockopt).
 */
#define	TCP_NODELAY	0x01	/* don't delay send to coalesce packets */
#if __BSD_VISIBLE
#define	TCP_MAXSEG	0x02	/* set maximum segment size */
#define TCP_NOPUSH	0x04	/* don't push last block of write */
#define TCP_NOOPT	0x08	/* don't use TCP options */
#define TCP_MD5SIG	0x10	/* use MD5 digests (RFC2385) */
#define	TCP_INFO	0x20	/* retrieve tcp_info structure */

#define	TCPI_OPT_TIMESTAMPS	0x01
#define	TCPI_OPT_SACK		0x02
#define	TCPI_OPT_WSCALE		0x04
#define	TCPI_OPT_ECN		0x08

/*
 * The TCP_INFO socket option comes from the Linux 2.6 TCP API, and permits
 * the caller to query certain information about the state of a TCP
 * connection.  We provide an overlapping set of fields with the Linux
 * implementation, but since this is a fixed size structure, room has been
 * left for growth.  In order to maximize potential future compatibility with
 * the Linux API, the same variable names and order have been adopted, and
 * padding left to make room for omitted fields in case they are added later.
 *
 * XXX: This is currently an unstable ABI/API, in that it is expected to
 * change.
 */
struct tcp_info {
	u_int8_t	tcpi_state;		/* TCP FSM state. */
	u_int8_t	__tcpi_ca_state;
	u_int8_t	__tcpi_retransmits;
	u_int8_t	__tcpi_probes;
	u_int8_t	__tcpi_backoff;
	u_int8_t	tcpi_options;		/* Options enabled on conn. */
	u_int8_t	tcpi_snd_wscale:4,	/* RFC1323 send shift value. */
			tcpi_rcv_wscale:4;	/* RFC1323 recv shift value. */

	u_int32_t	__tcpi_rto;
	u_int32_t	__tcpi_ato;
	u_int32_t	__tcpi_snd_mss;
	u_int32_t	__tcpi_rcv_mss;

	u_int32_t	__tcpi_unacked;
	u_int32_t	__tcpi_sacked;
	u_int32_t	__tcpi_lost;
	u_int32_t	__tcpi_retrans;
	u_int32_t	__tcpi_fackets;

	/* Times; measurements in usecs. */
	u_int32_t	__tcpi_last_data_sent;
	u_int32_t	__tcpi_last_ack_sent;	/* Also unimpl. on Linux? */
	u_int32_t	__tcpi_last_data_recv;
	u_int32_t	__tcpi_last_ack_recv;

	/* Metrics; variable units. */
	u_int32_t	__tcpi_pmtu;
	u_int32_t	__tcpi_rcv_ssthresh;
	u_int32_t	tcpi_rtt;		/* Smoothed RTT in usecs. */
	u_int32_t	tcpi_rttvar;		/* RTT variance in usecs. */
	u_int32_t	tcpi_snd_ssthresh;	/* Slow start threshold. */
	u_int32_t	tcpi_snd_cwnd;		/* Send congestion window. */
	u_int32_t	__tcpi_advmss;
	u_int32_t	__tcpi_reordering;

	u_int32_t	__tcpi_rcv_rtt;
	u_int32_t	tcpi_rcv_space;		/* Advertised recv window. */

	/* FreeBSD extensions to tcp_info. */
	u_int32_t	tcpi_snd_wnd;		/* Advertised send window. */
	u_int32_t	tcpi_snd_bwnd;		/* Bandwidth send window. */

	/* Padding to grow without breaking ABI. */
	u_int32_t	__tcpi_pad[32];		/* Padding. */
};
#endif

#endif /* !_NETINET_TCP_H_ */
