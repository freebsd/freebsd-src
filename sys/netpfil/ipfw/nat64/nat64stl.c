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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/rmlock.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_pflog.h>
#include <net/pfil.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/ip_fw.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/ip_fw_nat64.h>

#include <netpfil/ipfw/ip_fw_private.h>
#include <netpfil/ipfw/nat64/ip_fw_nat64.h>
#include <netpfil/ipfw/nat64/nat64_translate.h>
#include <netpfil/ipfw/nat64/nat64stl.h>
#include <netpfil/pf/pf.h>

#define	NAT64_LOOKUP(chain, cmd)	\
	(struct nat64stl_cfg *)SRV_OBJECT((chain), (cmd)->arg1)

static void
nat64stl_log(struct pfloghdr *plog, struct mbuf *m, sa_family_t family,
    uint32_t kidx)
{
	static uint32_t pktid = 0;

	memset(plog, 0, sizeof(plog));
	plog->length = PFLOG_REAL_HDRLEN;
	plog->af = family;
	plog->action = PF_NAT;
	plog->dir = PF_IN;
	plog->rulenr = htonl(kidx);
	plog->subrulenr = htonl(++pktid);
	plog->ruleset[0] = '\0';
	strlcpy(plog->ifname, "NAT64STL", sizeof(plog->ifname));
	ipfw_bpf_mtap2(plog, PFLOG_HDRLEN, m);
}

static int
nat64stl_handle_ip4(struct ip_fw_chain *chain, struct nat64stl_cfg *cfg,
    struct mbuf *m, uint32_t tablearg)
{
	struct pfloghdr loghdr, *logdata;
	struct in6_addr saddr, daddr;
	struct ip *ip;

	ip = mtod(m, struct ip*);
	if (nat64_check_ip4(ip->ip_src.s_addr) != 0 ||
	    nat64_check_ip4(ip->ip_dst.s_addr) != 0 ||
	    nat64_check_private_ip4(ip->ip_src.s_addr) != 0 ||
	    nat64_check_private_ip4(ip->ip_dst.s_addr) != 0)
		return (NAT64SKIP);

	daddr = TARG_VAL(chain, tablearg, nh6);
	if (nat64_check_ip6(&daddr) != 0)
		return (NAT64MFREE);
	saddr = cfg->prefix6;
	nat64_set_ip4(&saddr, ip->ip_src.s_addr);

	if (cfg->flags & NAT64_LOG) {
		logdata = &loghdr;
		nat64stl_log(logdata, m, AF_INET, cfg->no.kidx);
	} else
		logdata = NULL;
	return (nat64_do_handle_ip4(m, &saddr, &daddr, 0, &cfg->stats,
	    logdata));
}

static int
nat64stl_handle_ip6(struct ip_fw_chain *chain, struct nat64stl_cfg *cfg,
    struct mbuf *m, uint32_t tablearg)
{
	struct pfloghdr loghdr, *logdata;
	struct ip6_hdr *ip6;
	uint32_t aaddr;

	aaddr = htonl(TARG_VAL(chain, tablearg, nh4));

	/*
	 * NOTE: we expect ipfw_chk() did m_pullup() up to upper level
	 * protocol's headers. Also we skip some checks, that ip6_input(),
	 * ip6_forward(), ip6_fastfwd() and ipfw_chk() already did.
	 */
	ip6 = mtod(m, struct ip6_hdr *);
	/* Check ip6_dst matches configured prefix */
	if (bcmp(&ip6->ip6_dst, &cfg->prefix6, cfg->plen6 / 8) != 0)
		return (NAT64SKIP);

	if (cfg->flags & NAT64_LOG) {
		logdata = &loghdr;
		nat64stl_log(logdata, m, AF_INET6, cfg->no.kidx);
	} else
		logdata = NULL;
	return (nat64_do_handle_ip6(m, aaddr, 0, &cfg->stats, logdata));
}

static int
nat64stl_handle_icmp6(struct ip_fw_chain *chain, struct nat64stl_cfg *cfg,
    struct mbuf *m)
{
	struct pfloghdr loghdr, *logdata;
	nat64_stats_block *stats;
	struct ip6_hdr *ip6i;
	struct icmp6_hdr *icmp6;
	uint32_t tablearg;
	int hlen, proto;

	hlen = 0;
	stats = &cfg->stats;
	proto = nat64_getlasthdr(m, &hlen);
	if (proto != IPPROTO_ICMPV6) {
		NAT64STAT_INC(stats, dropped);
		return (NAT64MFREE);
	}
	icmp6 = mtodo(m, hlen);
	switch (icmp6->icmp6_type) {
	case ICMP6_DST_UNREACH:
	case ICMP6_PACKET_TOO_BIG:
	case ICMP6_TIME_EXCEED_TRANSIT:
	case ICMP6_PARAM_PROB:
		break;
	default:
		NAT64STAT_INC(stats, dropped);
		return (NAT64MFREE);
	}
	hlen += sizeof(struct icmp6_hdr);
	if (m->m_pkthdr.len < hlen + sizeof(struct ip6_hdr) + ICMP_MINLEN) {
		NAT64STAT_INC(stats, dropped);
		return (NAT64MFREE);
	}
	if (m->m_len < hlen + sizeof(struct ip6_hdr) + ICMP_MINLEN)
		m = m_pullup(m, hlen + sizeof(struct ip6_hdr) + ICMP_MINLEN);
	if (m == NULL) {
		NAT64STAT_INC(stats, nomem);
		return (NAT64RETURN);
	}
	/*
	 * Use destination address from inner IPv6 header to determine
	 * IPv4 mapped address.
	 */
	ip6i = mtodo(m, hlen);
	if (ipfw_lookup_table_extended(chain, cfg->map64,
	    sizeof(struct in6_addr), &ip6i->ip6_dst, &tablearg) == 0) {
		m_freem(m);
		return (NAT64RETURN);
	}
	if (cfg->flags & NAT64_LOG) {
		logdata = &loghdr;
		nat64stl_log(logdata, m, AF_INET6, cfg->no.kidx);
	} else
		logdata = NULL;
	return (nat64_handle_icmp6(m, 0,
	    htonl(TARG_VAL(chain, tablearg, nh4)), 0, stats, logdata));
}

int
ipfw_nat64stl(struct ip_fw_chain *chain, struct ip_fw_args *args,
    ipfw_insn *cmd, int *done)
{
	ipfw_insn *icmd;
	struct nat64stl_cfg *cfg;
	uint32_t tablearg;
	int ret;

	IPFW_RLOCK_ASSERT(chain);

	*done = 0; /* try next rule if not matched */
	icmd = cmd + 1;
	if (cmd->opcode != O_EXTERNAL_ACTION ||
	    cmd->arg1 != V_nat64stl_eid ||
	    icmd->opcode != O_EXTERNAL_INSTANCE ||
	    (cfg = NAT64_LOOKUP(chain, icmd)) == NULL)
		return (0);

	switch (args->f_id.addr_type) {
	case 4:
		ret = ipfw_lookup_table(chain, cfg->map46,
		    htonl(args->f_id.dst_ip), &tablearg);
		break;
	case 6:
		ret = ipfw_lookup_table_extended(chain, cfg->map64,
		    sizeof(struct in6_addr), &args->f_id.src_ip6, &tablearg);
		break;
	default:
		return (0);
	}
	if (ret == 0) {
		/*
		 * In case when packet is ICMPv6 message from an intermediate
		 * router, the source address of message will not match the
		 * addresses from our map64 table.
		 */
		if (args->f_id.proto != IPPROTO_ICMPV6)
			return (0);

		ret = nat64stl_handle_icmp6(chain, cfg, args->m);
	} else {
		if (args->f_id.addr_type == 4)
			ret = nat64stl_handle_ip4(chain, cfg, args->m,
			    tablearg);
		else
			ret = nat64stl_handle_ip6(chain, cfg, args->m,
			    tablearg);
	}
	if (ret == NAT64SKIP)
		return (0);

	*done = 1; /* terminate the search */
	if (ret == NAT64MFREE)
		m_freem(args->m);
	args->m = NULL;
	return (IP_FW_DENY);
}


