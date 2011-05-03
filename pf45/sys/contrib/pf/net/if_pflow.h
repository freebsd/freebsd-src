/*	$OpenBSD: if_pflow.h,v 1.5 2009/02/27 11:09:36 gollo Exp $	*/

/*
 * Copyright (c) 2008 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2008 Joerg Goltermann <jg@osn.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $FreeBSD$
 */

#ifndef _NET_IF_PFLOW_H_
#define	_NET_IF_PFLOW_H_

#define	PFLOW_ID_LEN	sizeof(u_int64_t)

#define	PFLOW_MAXFLOWS 30
#define	PFLOW_VERSION 5
#define	PFLOW_ENGINE_TYPE 42
#define	PFLOW_ENGINE_ID 42
#define	PFLOW_MAXBYTES 0xffffffff
#define	PFLOW_TIMEOUT 30

struct pflow_flow {
	u_int32_t	src_ip;
	u_int32_t	dest_ip;
	u_int32_t	nexthop_ip;
	u_int16_t	if_index_in;
	u_int16_t	if_index_out;
	u_int32_t	flow_packets;
	u_int32_t	flow_octets;
	u_int32_t	flow_start;
	u_int32_t	flow_finish;
	u_int16_t	src_port;
	u_int16_t	dest_port;
	u_int8_t	pad1;
	u_int8_t	tcp_flags;
	u_int8_t	protocol;
	u_int8_t	tos;
	u_int16_t	src_as;
	u_int16_t	dest_as;
	u_int8_t	src_mask;
	u_int8_t	dest_mask;
	u_int16_t	pad2;
} __packed;

#ifdef _KERNEL

extern int pflow_ok;

struct pflow_softc {
	struct ifnet		 sc_if;
	struct ifnet		*sc_pflow_ifp;

	unsigned int		 sc_count;
	unsigned int		 sc_maxcount;
	u_int64_t		 sc_gcounter;
	struct ip_moptions	 sc_imo;
#ifdef __FreeBSD__
	struct callout		 sc_tmo;
#else
	struct timeout		 sc_tmo;
#endif
	struct in_addr		 sc_sender_ip;
	u_int16_t		 sc_sender_port;
	struct in_addr		 sc_receiver_ip;
	u_int16_t		 sc_receiver_port;
	struct mbuf		*sc_mbuf;	/* current cumulative mbuf */
	SLIST_ENTRY(pflow_softc) sc_next;
};

extern struct pflow_softc	*pflowif;

#endif /* _KERNEL */

struct pflow_header {
	u_int16_t	version;
	u_int16_t	count;
	u_int32_t	uptime_ms;
	u_int32_t	time_sec;
	u_int32_t	time_nanosec;
	u_int32_t	flow_sequence;
	u_int8_t	engine_type;
	u_int8_t	engine_id;
	u_int8_t	reserved1;
	u_int8_t	reserved2;
} __packed;

#define	PFLOW_HDRLEN sizeof(struct pflow_header)

struct pflowstats {
	u_int64_t	pflow_flows;
	u_int64_t	pflow_packets;
	u_int64_t	pflow_onomem;
	u_int64_t	pflow_oerrors;
};

/*
 * Configuration structure for SIOCSETPFLOW SIOCGETPFLOW
 */
struct pflowreq {
	struct in_addr		sender_ip;
	struct in_addr		receiver_ip;
	u_int16_t		receiver_port;
	u_int16_t		addrmask;
#define	PFLOW_MASK_SRCIP	0x01
#define	PFLOW_MASK_DSTIP	0x02
#define	PFLOW_MASK_DSTPRT	0x04
};

#ifdef _KERNEL
int export_pflow(struct pf_state *);
int pflow_sysctl(int *, u_int,  void *, size_t *, void *, size_t);
#endif /* _KERNEL */

#endif /* _NET_IF_PFLOW_H_ */
