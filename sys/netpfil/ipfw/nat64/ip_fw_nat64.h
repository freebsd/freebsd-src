/*-
 * Copyright (c) 2015-2016 Yandex LLC
 * Copyright (c) 2015-2016 Andrey V. Elsukov <ae@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_IP_FW_NAT64_H_
#define	_IP_FW_NAT64_H_

#define	DPRINTF(mask, fmt, ...)	\
    if (nat64_debug & (mask))	\
	printf("NAT64: %s: " fmt "\n", __func__, ## __VA_ARGS__)
#define	DP_GENERIC	0x0001
#define	DP_OBJ		0x0002
#define	DP_JQUEUE	0x0004
#define	DP_STATE	0x0008
#define	DP_DROPS	0x0010
#define	DP_ALL		0xFFFF
extern int nat64_debug;

#if 0
#define	NAT64NOINLINE	__noinline
#else
#define	NAT64NOINLINE
#endif

int nat64stl_init(struct ip_fw_chain *ch, int first);
void nat64stl_uninit(struct ip_fw_chain *ch, int last);
int nat64lsn_init(struct ip_fw_chain *ch, int first);
void nat64lsn_uninit(struct ip_fw_chain *ch, int last);

struct ip_fw_nat64_stats {
	counter_u64_t	opcnt64;	/* 6to4 of packets translated */
	counter_u64_t	opcnt46;	/* 4to6 of packets translated */
	counter_u64_t	ofrags;		/* number of fragments generated */
	counter_u64_t	ifrags;		/* number of fragments received */
	counter_u64_t	oerrors;	/* number of output errors */
	counter_u64_t	noroute4;
	counter_u64_t	noroute6;
	counter_u64_t	nomatch4;	/* No addr/port match */
	counter_u64_t	noproto;	/* Protocol not supported */
	counter_u64_t	nomem;		/* mbufs allocation failed */
	counter_u64_t	dropped;	/* number of packets silently
					 * dropped due to some errors/
					 * unsupported/etc.
					 */

	counter_u64_t	jrequests;	/* number of jobs requests queued */
	counter_u64_t	jcalls;		/* number of jobs handler calls */
	counter_u64_t	jhostsreq;	/* number of hosts requests */
	counter_u64_t	jportreq;
	counter_u64_t	jhostfails;
	counter_u64_t	jportfails;
	counter_u64_t	jmaxlen;
	counter_u64_t	jnomem;
	counter_u64_t	jreinjected;

	counter_u64_t	screated;
	counter_u64_t	sdeleted;
	counter_u64_t	spgcreated;
	counter_u64_t	spgdeleted;
};

#define	IPFW_NAT64_VERSION	1
#define	NAT64STATS	(sizeof(struct ip_fw_nat64_stats) / sizeof(uint64_t))
typedef struct _nat64_stats_block {
	counter_u64_t		stats[NAT64STATS];
} nat64_stats_block;
#define	NAT64STAT_ADD(s, f, v)		\
    counter_u64_add((s)->stats[		\
	offsetof(struct ip_fw_nat64_stats, f) / sizeof(uint64_t)], (v))
#define	NAT64STAT_INC(s, f)	NAT64STAT_ADD(s, f, 1)
#define	NAT64STAT_FETCH(s, f)		\
    counter_u64_fetch((s)->stats[	\
	offsetof(struct ip_fw_nat64_stats, f) / sizeof(uint64_t)])

#define	L3HDR(_ip, _t)	((_t)((u_int32_t *)(_ip) + (_ip)->ip_hl))
#define	TCP(p)		((struct tcphdr *)(p))
#define	UDP(p)		((struct udphdr *)(p))
#define	ICMP(p)		((struct icmphdr *)(p))
#define	ICMP6(p)	((struct icmp6_hdr *)(p))

#define	NAT64SKIP	0
#define	NAT64RETURN	1
#define	NAT64MFREE	-1

/* Well-known prefix 64:ff9b::/96 */
#define	IPV6_ADDR_INT32_WKPFX	htonl(0x64ff9b)
#define	IN6_IS_ADDR_WKPFX(a)	\
    ((a)->s6_addr32[0] == IPV6_ADDR_INT32_WKPFX && \
	(a)->s6_addr32[1] == 0 && (a)->s6_addr32[2] == 0)

#endif

