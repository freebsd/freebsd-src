/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Yandex LLC
 * Copyright (c) 2020 Andrey V. Elsukov <ae@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma D depends_on provider ipfw

/* ipfw_chk() return values */
#pragma D binding "1.0" IP_FW_PASS
inline int IP_FW_PASS = 	0;
#pragma D binding "1.0" IP_FW_DENY
inline int IP_FW_DENY = 	1;
#pragma D binding "1.0" IP_FW_DIVERT
inline int IP_FW_DIVERT =	2;
#pragma D binding "1.0" IP_FW_TEE
inline int IP_FW_TEE =		3;
#pragma D binding "1.0" IP_FW_DUMMYNET
inline int IP_FW_DUMMYNET =	4;
#pragma D binding "1.0" IP_FW_NETGRAPH
inline int IP_FW_NETGRAPH =	5;
#pragma D binding "1.0" IP_FW_NGTEE
inline int IP_FW_NGTEE =	6;
#pragma D binding "1.0" IP_FW_NAT
inline int IP_FW_NAT =		7;
#pragma D binding "1.0" IP_FW_REASS
inline int IP_FW_REASS =	8;
#pragma D binding "1.0" IP_FW_NAT64
inline int IP_FW_NAT64 =	9;

#pragma D binding "1.0" ipfw_retcodes
inline string ipfw_retcodes[int ret] =
	ret == IP_FW_PASS ? "PASS" :
	ret == IP_FW_DENY ? "DENY" :
	ret == IP_FW_DIVERT ? "DIVERT" :
	ret == IP_FW_TEE ? "TEE" :
	ret == IP_FW_DUMMYNET ? "DUMMYNET" :
	ret == IP_FW_NETGRAPH ? "NETGRAPH" :
	ret == IP_FW_NGTEE ? "NGTEE" :
	ret == IP_FW_NAT ? "NAT" :
	ret == IP_FW_REASS ? "REASS" :
	ret == IP_FW_NAT64 ? "NAT64" :
	"<unknown>";

/* ip_fw_args flags */
#pragma D binding "1.0" IPFW_ARGS_ETHER
inline int IPFW_ARGS_ETHER =	0x00010000; /* valid ethernet header */
#pragma D binding "1.0" IPFW_ARGS_NH4
inline int IPFW_ARGS_NH4 =	0x00020000; /* IPv4 next hop in hopstore */
#pragma D binding "1.0" IPFW_ARGS_NH6
inline int IPFW_ARGS_NH6 =	0x00040000; /* IPv6 next hop in hopstore */
#pragma D binding "1.0" IPFW_ARGS_NH4PTR
inline int IPFW_ARGS_NH4PTR =	0x00080000; /* IPv4 next hop in next_hop */
#pragma D binding "1.0" IPFW_ARGS_NH6PTR
inline int IPFW_ARGS_NH6PTR =	0x00100000; /* IPv6 next hop in next_hop6 */
#pragma D binding "1.0" IPFW_ARGS_REF
inline int IPFW_ARGS_REF =	0x00200000; /* valid ipfw_rule_ref	*/
#pragma D binding "1.0" IPFW_ARGS_IN
inline int IPFW_ARGS_IN =	0x00400000; /* called on input */
#pragma D binding "1.0" IPFW_ARGS_OUT	
inline int IPFW_ARGS_OUT =	0x00800000; /* called on output */
#pragma D binding "1.0" IPFW_ARGS_IP4
inline int IPFW_ARGS_IP4 =	0x01000000; /* belongs to v4 ISR */
#pragma D binding "1.0" IPFW_ARGS_IP6
inline int IPFW_ARGS_IP6 =	0x02000000; /* belongs to v6 ISR */
#pragma D binding "1.0" IPFW_ARGS_DROP
inline int IPFW_ARGS_DROP =	0x04000000; /* drop it (dummynet) */
#pragma D binding "1.0" IPFW_ARGS_LENMASK
inline int IPFW_ARGS_LENMASK =	0x0000ffff; /* length of data in *mem */

/* ipfw_rule_ref.info */
#pragma D binding "1.0" IPFW_INFO_MASK
inline int IPFW_INFO_MASK =	0x0000ffff;
#pragma D binding "1.0" IPFW_INFO_OUT
inline int IPFW_INFO_OUT =	0x00000000;
#pragma D binding "1.0" IPFW_INFO_IN
inline int IPFW_INFO_IN =	0x80000000;
#pragma D binding "1.0" IPFW_ONEPASS
inline int IPFW_ONEPASS =	0x40000000;
#pragma D binding "1.0" IPFW_IS_MASK
inline int IPFW_IS_MASK =	0x30000000;
#pragma D binding "1.0" IPFW_IS_DIVERT
inline int IPFW_IS_DIVERT =	0x20000000;
#pragma D binding "1.0" IPFW_IS_DUMMYNET
inline int IPFW_IS_DUMMYNET =	0x10000000;
#pragma D binding "1.0" IPFW_IS_PIPE
inline int IPFW_IS_PIPE =	0x08000000;

typedef struct ipfw_match_info {
	uint32_t	flags;

	struct mbuf	*m;
	void		*mem;
	struct inpcb	*inp;
	struct ifnet	*ifp;
	struct ip	*ipp;
	struct ip6_hdr	*ip6p;

	/* flow id */
	uint8_t		addr_type;
	uint8_t		proto;
	uint8_t		proto_flags;
	uint16_t	fib;	/* XXX */
	in_addr_t	dst_ip;	/* in network byte order */
	in_addr_t	src_ip;	/* in network byte order */
	struct in6_addr	dst_ip6;
	struct in6_addr	src_ip6;

	uint16_t	dst_port; /* in host byte order */
	uint16_t	src_port; /* in host byte order */

	uint32_t	flowid;	/* IPv6 flowid */
	uint32_t	extra;

	/* ipfw_rule_ref */
	uint32_t	slot;
	uint32_t	rulenum;
	uint32_t	rule_id;
	uint32_t	chain_id;
	uint32_t	match_info;
} ipfw_match_info_t;

#pragma D binding "1.0" translator
translator ipfw_match_info_t < struct ip_fw_args *p > {
	flags =		p->flags;
	m =		(p->flags & IPFW_ARGS_LENMASK) ? NULL : p->m;
	mem =		(p->flags & IPFW_ARGS_LENMASK) ? p->mem : NULL;
	inp =		p->inp;
	ifp =		p->ifp;
	/* Initialize IP pointer corresponding to addr_type */
	ipp =		(p->flags & IPFW_ARGS_IP4) ?
	    (p->flags & IPFW_ARGS_LENMASK) ? (struct ip *)p->mem :
	    (p->m != NULL) ? (struct ip *)p->m->m_data : NULL : NULL;
	ip6p =		(p->flags & IPFW_ARGS_IP6) ?
	    (p->flags & IPFW_ARGS_LENMASK) ? (struct ip6_hdr *)p->mem :
	    (p->m != NULL) ? (struct ip6_hdr *)p->m->m_data : NULL : NULL;

	/* fill f_id fields */
	addr_type =	p->f_id.addr_type;
	proto =		p->f_id.proto;
	proto_flags =	p->f_id._flags;

	/* f_id.fib keeps truncated fibnum, use mbuf's fibnum if possible */
	fib =		p->m != NULL ? p->m->m_pkthdr.fibnum : p->f_id.fib;

	/*
	 * ipfw_chk() keeps IPv4 addresses in host byte order. But for
	 * dtrace script it is useful to have them in network byte order,
	 * because inet_ntoa() uses address in network byte order.
	 */
	dst_ip =	htonl(p->f_id.dst_ip);
	src_ip =	htonl(p->f_id.src_ip);

	dst_ip6 =	p->f_id.dst_ip6;
	src_ip6 =	p->f_id.src_ip6;

	dst_port =	p->f_id.dst_port;
	src_port =	p->f_id.src_port;

	flowid =	p->f_id.flow_id6;
	extra = 	p->f_id.extra;

	/* ipfw_rule_ref */
	slot =		(p->flags & IPFW_ARGS_REF) ? p->rule.slot : 0;
	rulenum =	(p->flags & IPFW_ARGS_REF) ? p->rule.rulenum : 0;
	rule_id =	(p->flags & IPFW_ARGS_REF) ? p->rule.rule_id : 0;
	chain_id =	(p->flags & IPFW_ARGS_REF) ? p->rule.chain_id : 0;
	match_info =	(p->flags & IPFW_ARGS_REF) ? p->rule.info : 0;
};

typedef struct ipfw_rule_info {
	uint16_t	act_ofs;
	uint16_t	cmd_len;
	uint32_t	rulenum;
	uint8_t		flags;
	uint8_t		set;
	uint32_t	rule_id;
	uint32_t	cached_id;
	uint32_t	cached_pos;
	uint32_t	refcnt;
} ipfw_rule_info_t;

#pragma D binding "1.0" translator
translator ipfw_rule_info_t < struct ip_fw *r > {
	act_ofs =	r->act_ofs;
	cmd_len =	r->cmd_len;
	rulenum =	r->rulenum;
	flags =		r->flags;
	set =		r->set;
	rule_id =	r->id;
	cached_id =	r->cache.id;
	cached_pos =	r->cache.pos;
	refcnt =	r->refcnt;
};

