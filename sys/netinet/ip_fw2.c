/*-
 * Copyright (c) 2002 Luigi Rizzo, Universita` di Pisa
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

#define        DEB(x)
#define        DDB(x) x

/*
 * Implement IP packet firewall (new version)
 */

#if !defined(KLD_MODULE)
#include "opt_ipfw.h"
#include "opt_ip6fw.h"
#include "opt_ipdn.h"
#include "opt_inet.h"
#ifndef INET
#error IPFIREWALL requires INET.
#endif /* INET */
#endif
#include "opt_inet6.h"
#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/condvar.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/jail.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/ucred.h>
#include <net/if.h>
#include <net/radix.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_divert.h>
#include <netinet/ip_dummynet.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <netgraph/ng_ipfw.h>

#include <altq/if_altq.h>

#ifdef IPSEC
#include <netinet6/ipsec.h>
#endif

#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#ifdef INET6
#include <netinet6/scope6_var.h>
#endif

#include <netinet/if_ether.h> /* XXX for ETHERTYPE_IP */

#include <machine/in_cksum.h>	/* XXX for in_cksum */

/*
 * set_disable contains one bit per set value (0..31).
 * If the bit is set, all rules with the corresponding set
 * are disabled. Set RESVD_SET(31) is reserved for the default rule
 * and rules that are not deleted by the flush command,
 * and CANNOT be disabled.
 * Rules in set RESVD_SET can only be deleted explicitly.
 */
static u_int32_t set_disable;

static int fw_verbose;
static int verbose_limit;

static struct callout ipfw_timeout;
static uma_zone_t ipfw_dyn_rule_zone;
#define	IPFW_DEFAULT_RULE	65535

/*
 * Data structure to cache our ucred related
 * information. This structure only gets used if
 * the user specified UID/GID based constraints in
 * a firewall rule.
 */
struct ip_fw_ugid {
	gid_t		fw_groups[NGROUPS];
	int		fw_ngroups;
	uid_t		fw_uid;
	int		fw_prid;
};

#define	IPFW_TABLES_MAX		128
struct ip_fw_chain {
	struct ip_fw	*rules;		/* list of rules */
	struct ip_fw	*reap;		/* list of rules to reap */
	struct radix_node_head *tables[IPFW_TABLES_MAX];
	struct mtx	mtx;		/* lock guarding rule list */
	int		busy_count;	/* busy count for rw locks */
	int		want_write;
	struct cv	cv;
};
#define	IPFW_LOCK_INIT(_chain) \
	mtx_init(&(_chain)->mtx, "IPFW static rules", NULL, \
		MTX_DEF | MTX_RECURSE)
#define	IPFW_LOCK_DESTROY(_chain)	mtx_destroy(&(_chain)->mtx)
#define	IPFW_WLOCK_ASSERT(_chain)	do {				\
	mtx_assert(&(_chain)->mtx, MA_OWNED);				\
	NET_ASSERT_GIANT();						\
} while (0)

static __inline void
IPFW_RLOCK(struct ip_fw_chain *chain)
{
	mtx_lock(&chain->mtx);
	chain->busy_count++;
	mtx_unlock(&chain->mtx);
}

static __inline void
IPFW_RUNLOCK(struct ip_fw_chain *chain)
{
	mtx_lock(&chain->mtx);
	chain->busy_count--;
	if (chain->busy_count == 0 && chain->want_write)
		cv_signal(&chain->cv);
	mtx_unlock(&chain->mtx);
}

static __inline void
IPFW_WLOCK(struct ip_fw_chain *chain)
{
	mtx_lock(&chain->mtx);
	chain->want_write++;
	while (chain->busy_count > 0)
		cv_wait(&chain->cv, &chain->mtx);
}

static __inline void
IPFW_WUNLOCK(struct ip_fw_chain *chain)
{
	chain->want_write--;
	cv_signal(&chain->cv);
	mtx_unlock(&chain->mtx);
}

/*
 * list of rules for layer 3
 */
static struct ip_fw_chain layer3_chain;

MALLOC_DEFINE(M_IPFW, "IpFw/IpAcct", "IpFw/IpAcct chain's");
MALLOC_DEFINE(M_IPFW_TBL, "ipfw_tbl", "IpFw tables");

struct table_entry {
	struct radix_node	rn[2];
	struct sockaddr_in	addr, mask;
	u_int32_t		value;
};

static int fw_debug = 1;
static int autoinc_step = 100; /* bounded to 1..1000 in add_rule() */

#ifdef SYSCTL_NODE
SYSCTL_NODE(_net_inet_ip, OID_AUTO, fw, CTLFLAG_RW, 0, "Firewall");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, enable,
    CTLFLAG_RW | CTLFLAG_SECURE3,
    &fw_enable, 0, "Enable ipfw");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, autoinc_step, CTLFLAG_RW,
    &autoinc_step, 0, "Rule number autincrement step");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, one_pass,
    CTLFLAG_RW | CTLFLAG_SECURE3,
    &fw_one_pass, 0,
    "Only do a single pass through ipfw when using dummynet(4)");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, debug, CTLFLAG_RW,
    &fw_debug, 0, "Enable printing of debug ip_fw statements");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, verbose,
    CTLFLAG_RW | CTLFLAG_SECURE3,
    &fw_verbose, 0, "Log matches to ipfw rules");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, verbose_limit, CTLFLAG_RW,
    &verbose_limit, 0, "Set upper limit of matches of ipfw rules logged");

/*
 * Description of dynamic rules.
 *
 * Dynamic rules are stored in lists accessed through a hash table
 * (ipfw_dyn_v) whose size is curr_dyn_buckets. This value can
 * be modified through the sysctl variable dyn_buckets which is
 * updated when the table becomes empty.
 *
 * XXX currently there is only one list, ipfw_dyn.
 *
 * When a packet is received, its address fields are first masked
 * with the mask defined for the rule, then hashed, then matched
 * against the entries in the corresponding list.
 * Dynamic rules can be used for different purposes:
 *  + stateful rules;
 *  + enforcing limits on the number of sessions;
 *  + in-kernel NAT (not implemented yet)
 *
 * The lifetime of dynamic rules is regulated by dyn_*_lifetime,
 * measured in seconds and depending on the flags.
 *
 * The total number of dynamic rules is stored in dyn_count.
 * The max number of dynamic rules is dyn_max. When we reach
 * the maximum number of rules we do not create anymore. This is
 * done to avoid consuming too much memory, but also too much
 * time when searching on each packet (ideally, we should try instead
 * to put a limit on the length of the list on each bucket...).
 *
 * Each dynamic rule holds a pointer to the parent ipfw rule so
 * we know what action to perform. Dynamic rules are removed when
 * the parent rule is deleted. XXX we should make them survive.
 *
 * There are some limitations with dynamic rules -- we do not
 * obey the 'randomized match', and we do not do multiple
 * passes through the firewall. XXX check the latter!!!
 */
static ipfw_dyn_rule **ipfw_dyn_v = NULL;
static u_int32_t dyn_buckets = 256; /* must be power of 2 */
static u_int32_t curr_dyn_buckets = 256; /* must be power of 2 */

static struct mtx ipfw_dyn_mtx;		/* mutex guarding dynamic rules */
#define	IPFW_DYN_LOCK_INIT() \
	mtx_init(&ipfw_dyn_mtx, "IPFW dynamic rules", NULL, MTX_DEF)
#define	IPFW_DYN_LOCK_DESTROY()	mtx_destroy(&ipfw_dyn_mtx)
#define	IPFW_DYN_LOCK()		mtx_lock(&ipfw_dyn_mtx)
#define	IPFW_DYN_UNLOCK()	mtx_unlock(&ipfw_dyn_mtx)
#define	IPFW_DYN_LOCK_ASSERT()	mtx_assert(&ipfw_dyn_mtx, MA_OWNED)

/*
 * Timeouts for various events in handing dynamic rules.
 */
static u_int32_t dyn_ack_lifetime = 300;
static u_int32_t dyn_syn_lifetime = 20;
static u_int32_t dyn_fin_lifetime = 1;
static u_int32_t dyn_rst_lifetime = 1;
static u_int32_t dyn_udp_lifetime = 10;
static u_int32_t dyn_short_lifetime = 5;

/*
 * Keepalives are sent if dyn_keepalive is set. They are sent every
 * dyn_keepalive_period seconds, in the last dyn_keepalive_interval
 * seconds of lifetime of a rule.
 * dyn_rst_lifetime and dyn_fin_lifetime should be strictly lower
 * than dyn_keepalive_period.
 */

static u_int32_t dyn_keepalive_interval = 20;
static u_int32_t dyn_keepalive_period = 5;
static u_int32_t dyn_keepalive = 1;	/* do send keepalives */

static u_int32_t static_count;	/* # of static rules */
static u_int32_t static_len;	/* size in bytes of static rules */
static u_int32_t dyn_count;		/* # of dynamic rules */
static u_int32_t dyn_max = 4096;	/* max # of dynamic rules */

SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_buckets, CTLFLAG_RW,
    &dyn_buckets, 0, "Number of dyn. buckets");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, curr_dyn_buckets, CTLFLAG_RD,
    &curr_dyn_buckets, 0, "Current Number of dyn. buckets");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_count, CTLFLAG_RD,
    &dyn_count, 0, "Number of dyn. rules");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_max, CTLFLAG_RW,
    &dyn_max, 0, "Max number of dyn. rules");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, static_count, CTLFLAG_RD,
    &static_count, 0, "Number of static rules");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_ack_lifetime, CTLFLAG_RW,
    &dyn_ack_lifetime, 0, "Lifetime of dyn. rules for acks");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_syn_lifetime, CTLFLAG_RW,
    &dyn_syn_lifetime, 0, "Lifetime of dyn. rules for syn");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_fin_lifetime, CTLFLAG_RW,
    &dyn_fin_lifetime, 0, "Lifetime of dyn. rules for fin");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_rst_lifetime, CTLFLAG_RW,
    &dyn_rst_lifetime, 0, "Lifetime of dyn. rules for rst");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_udp_lifetime, CTLFLAG_RW,
    &dyn_udp_lifetime, 0, "Lifetime of dyn. rules for UDP");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_short_lifetime, CTLFLAG_RW,
    &dyn_short_lifetime, 0, "Lifetime of dyn. rules for other situations");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_keepalive, CTLFLAG_RW,
    &dyn_keepalive, 0, "Enable keepalives for dyn. rules");

#ifdef INET6
/*
 * IPv6 specific variables
 */
SYSCTL_DECL(_net_inet6_ip6);

static struct sysctl_ctx_list ip6_fw_sysctl_ctx;
static struct sysctl_oid *ip6_fw_sysctl_tree;
#endif /* INET6 */
#endif /* SYSCTL_NODE */

static int fw_deny_unknown_exthdrs = 1;


/*
 * L3HDR maps an ipv4 pointer into a layer3 header pointer of type T
 * Other macros just cast void * into the appropriate type
 */
#define	L3HDR(T, ip)	((T *)((u_int32_t *)(ip) + (ip)->ip_hl))
#define	TCP(p)		((struct tcphdr *)(p))
#define	UDP(p)		((struct udphdr *)(p))
#define	ICMP(p)		((struct icmphdr *)(p))
#define	ICMP6(p)	((struct icmp6_hdr *)(p))

static __inline int
icmptype_match(struct icmphdr *icmp, ipfw_insn_u32 *cmd)
{
	int type = icmp->icmp_type;

	return (type <= ICMP_MAXTYPE && (cmd->d[0] & (1<<type)) );
}

#define TT	( (1 << ICMP_ECHO) | (1 << ICMP_ROUTERSOLICIT) | \
    (1 << ICMP_TSTAMP) | (1 << ICMP_IREQ) | (1 << ICMP_MASKREQ) )

static int
is_icmp_query(struct icmphdr *icmp)
{
	int type = icmp->icmp_type;

	return (type <= ICMP_MAXTYPE && (TT & (1<<type)) );
}
#undef TT

/*
 * The following checks use two arrays of 8 or 16 bits to store the
 * bits that we want set or clear, respectively. They are in the
 * low and high half of cmd->arg1 or cmd->d[0].
 *
 * We scan options and store the bits we find set. We succeed if
 *
 *	(want_set & ~bits) == 0 && (want_clear & ~bits) == want_clear
 *
 * The code is sometimes optimized not to store additional variables.
 */

static int
flags_match(ipfw_insn *cmd, u_int8_t bits)
{
	u_char want_clear;
	bits = ~bits;

	if ( ((cmd->arg1 & 0xff) & bits) != 0)
		return 0; /* some bits we want set were clear */
	want_clear = (cmd->arg1 >> 8) & 0xff;
	if ( (want_clear & bits) != want_clear)
		return 0; /* some bits we want clear were set */
	return 1;
}

static int
ipopts_match(struct ip *ip, ipfw_insn *cmd)
{
	int optlen, bits = 0;
	u_char *cp = (u_char *)(ip + 1);
	int x = (ip->ip_hl << 2) - sizeof (struct ip);

	for (; x > 0; x -= optlen, cp += optlen) {
		int opt = cp[IPOPT_OPTVAL];

		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[IPOPT_OLEN];
			if (optlen <= 0 || optlen > x)
				return 0; /* invalid or truncated */
		}
		switch (opt) {

		default:
			break;

		case IPOPT_LSRR:
			bits |= IP_FW_IPOPT_LSRR;
			break;

		case IPOPT_SSRR:
			bits |= IP_FW_IPOPT_SSRR;
			break;

		case IPOPT_RR:
			bits |= IP_FW_IPOPT_RR;
			break;

		case IPOPT_TS:
			bits |= IP_FW_IPOPT_TS;
			break;
		}
	}
	return (flags_match(cmd, bits));
}

static int
tcpopts_match(struct tcphdr *tcp, ipfw_insn *cmd)
{
	int optlen, bits = 0;
	u_char *cp = (u_char *)(tcp + 1);
	int x = (tcp->th_off << 2) - sizeof(struct tcphdr);

	for (; x > 0; x -= optlen, cp += optlen) {
		int opt = cp[0];
		if (opt == TCPOPT_EOL)
			break;
		if (opt == TCPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[1];
			if (optlen <= 0)
				break;
		}

		switch (opt) {

		default:
			break;

		case TCPOPT_MAXSEG:
			bits |= IP_FW_TCPOPT_MSS;
			break;

		case TCPOPT_WINDOW:
			bits |= IP_FW_TCPOPT_WINDOW;
			break;

		case TCPOPT_SACK_PERMITTED:
		case TCPOPT_SACK:
			bits |= IP_FW_TCPOPT_SACK;
			break;

		case TCPOPT_TIMESTAMP:
			bits |= IP_FW_TCPOPT_TS;
			break;

		}
	}
	return (flags_match(cmd, bits));
}

static int
iface_match(struct ifnet *ifp, ipfw_insn_if *cmd)
{
	if (ifp == NULL)	/* no iface with this packet, match fails */
		return 0;
	/* Check by name or by IP address */
	if (cmd->name[0] != '\0') { /* match by name */
		/* Check name */
		if (cmd->p.glob) {
			if (fnmatch(cmd->name, ifp->if_xname, 0) == 0)
				return(1);
		} else {
			if (strncmp(ifp->if_xname, cmd->name, IFNAMSIZ) == 0)
				return(1);
		}
	} else {
		struct ifaddr *ia;

		/* XXX lock? */
		TAILQ_FOREACH(ia, &ifp->if_addrhead, ifa_link) {
			if (ia->ifa_addr == NULL)
				continue;
			if (ia->ifa_addr->sa_family != AF_INET)
				continue;
			if (cmd->p.ip.s_addr == ((struct sockaddr_in *)
			    (ia->ifa_addr))->sin_addr.s_addr)
				return(1);	/* match */
		}
	}
	return(0);	/* no match, fail ... */
}

/*
 * The verify_path function checks if a route to the src exists and
 * if it is reachable via ifp (when provided).
 * 
 * The 'verrevpath' option checks that the interface that an IP packet
 * arrives on is the same interface that traffic destined for the
 * packet's source address would be routed out of.  The 'versrcreach'
 * option just checks that the source address is reachable via any route
 * (except default) in the routing table.  These two are a measure to block
 * forged packets.  This is also commonly known as "anti-spoofing" or Unicast
 * Reverse Path Forwarding (Unicast RFP) in Cisco-ese. The name of the knobs
 * is purposely reminiscent of the Cisco IOS command,
 *
 *   ip verify unicast reverse-path
 *   ip verify unicast source reachable-via any
 *
 * which implements the same functionality. But note that syntax is
 * misleading. The check may be performed on all IP packets whether unicast,
 * multicast, or broadcast.
 */
static int
verify_path(struct in_addr src, struct ifnet *ifp)
{
	struct route ro;
	struct sockaddr_in *dst;

	bzero(&ro, sizeof(ro));

	dst = (struct sockaddr_in *)&(ro.ro_dst);
	dst->sin_family = AF_INET;
	dst->sin_len = sizeof(*dst);
	dst->sin_addr = src;
	rtalloc_ign(&ro, RTF_CLONING);

	if (ro.ro_rt == NULL)
		return 0;

	/* if ifp is provided, check for equality with rtentry */
	if (ifp != NULL && ro.ro_rt->rt_ifp != ifp) {
		RTFREE(ro.ro_rt);
		return 0;
	}

	/* if no ifp provided, check if rtentry is not default route */
	if (ifp == NULL &&
	     satosin(rt_key(ro.ro_rt))->sin_addr.s_addr == INADDR_ANY) {
		RTFREE(ro.ro_rt);
		return 0;
	}

	/* or if this is a blackhole/reject route */
	if (ifp == NULL && ro.ro_rt->rt_flags & (RTF_REJECT|RTF_BLACKHOLE)) {
		RTFREE(ro.ro_rt);
		return 0;
	}

	/* found valid route */
	RTFREE(ro.ro_rt);
	return 1;
}

#ifdef INET6
/*
 * ipv6 specific rules here...
 */
static __inline int
icmp6type_match (int type, ipfw_insn_u32 *cmd)
{
	return (type <= ICMP6_MAXTYPE && (cmd->d[type/32] & (1<<(type%32)) ) );
}

static int
flow6id_match( int curr_flow, ipfw_insn_u32 *cmd )
{
	int i;
	for (i=0; i <= cmd->o.arg1; ++i )
		if (curr_flow == cmd->d[i] )
			return 1;
	return 0;
}

/* support for IP6_*_ME opcodes */
static int
search_ip6_addr_net (struct in6_addr * ip6_addr)
{
	struct ifnet *mdc;
	struct ifaddr *mdc2;
	struct in6_ifaddr *fdm;
	struct in6_addr copia;

	TAILQ_FOREACH(mdc, &ifnet, if_link)
		for (mdc2 = mdc->if_addrlist.tqh_first; mdc2;
		    mdc2 = mdc2->ifa_list.tqe_next) {
			if (!mdc2->ifa_addr)
				continue;
			if (mdc2->ifa_addr->sa_family == AF_INET6) {
				fdm = (struct in6_ifaddr *)mdc2;
				copia = fdm->ia_addr.sin6_addr;
				/* need for leaving scope_id in the sock_addr */
				in6_clearscope(&copia);
				if (IN6_ARE_ADDR_EQUAL(ip6_addr, &copia))
					return 1;
			}
		}
	return 0;
}

static int
verify_path6(struct in6_addr *src, struct ifnet *ifp)
{
	struct route_in6 ro;
	struct sockaddr_in6 *dst;

	bzero(&ro, sizeof(ro));

	dst = (struct sockaddr_in6 * )&(ro.ro_dst);
	dst->sin6_family = AF_INET6;
	dst->sin6_len = sizeof(*dst);
	dst->sin6_addr = *src;
	rtalloc_ign((struct route *)&ro, RTF_CLONING);

	if (ro.ro_rt == NULL)
		return 0;

	/* 
	 * if ifp is provided, check for equality with rtentry
	 * We should use rt->rt_ifa->ifa_ifp, instead of rt->rt_ifp,
	 * to support the case of sending packets to an address of our own.
	 * (where the former interface is the first argument of if_simloop()
	 *  (=ifp), the latter is lo0)
	 */
	if (ifp != NULL && ro.ro_rt->rt_ifa->ifa_ifp != ifp) {
		RTFREE(ro.ro_rt);
		return 0;
	}

	/* if no ifp provided, check if rtentry is not default route */
	if (ifp == NULL &&
	    IN6_IS_ADDR_UNSPECIFIED(&satosin6(rt_key(ro.ro_rt))->sin6_addr)) {
		RTFREE(ro.ro_rt);
		return 0;
	}

	/* or if this is a blackhole/reject route */
	if (ifp == NULL && ro.ro_rt->rt_flags & (RTF_REJECT|RTF_BLACKHOLE)) {
		RTFREE(ro.ro_rt);
		return 0;
	}

	/* found valid route */
	RTFREE(ro.ro_rt);
	return 1;

}
static __inline int
hash_packet6(struct ipfw_flow_id *id)
{
	u_int32_t i;
	i = (id->dst_ip6.__u6_addr.__u6_addr32[0]) ^
	    (id->dst_ip6.__u6_addr.__u6_addr32[1]) ^
	    (id->dst_ip6.__u6_addr.__u6_addr32[2]) ^
	    (id->dst_ip6.__u6_addr.__u6_addr32[3]) ^
	    (id->dst_port) ^ (id->src_port) ^ (id->flow_id6);
	return i;
}

static int
is_icmp6_query(int icmp6_type)
{
	if ((icmp6_type <= ICMP6_MAXTYPE) &&
	    (icmp6_type == ICMP6_ECHO_REQUEST ||
	    icmp6_type == ICMP6_MEMBERSHIP_QUERY ||
	    icmp6_type == ICMP6_WRUREQUEST ||
	    icmp6_type == ICMP6_FQDN_QUERY ||
	    icmp6_type == ICMP6_NI_QUERY))
		return (1);

	return (0);
}

static void
send_reject6(struct ip_fw_args *args, int code, u_short offset, u_int hlen)
{
	if (code == ICMP6_UNREACH_RST && offset == 0 &&
	    args->f_id.proto == IPPROTO_TCP) {
		struct ip6_hdr *ip6;
		struct tcphdr *tcp;
		tcp_seq ack, seq;
		int flags;
		struct {
			struct ip6_hdr ip6;
			struct tcphdr th;
		} ti;

		if (args->m->m_len < (hlen+sizeof(struct tcphdr))) {
			args->m = m_pullup(args->m, hlen+sizeof(struct tcphdr));
			if (args->m == NULL)
				return;
		}

		ip6 = mtod(args->m, struct ip6_hdr *);
		tcp = (struct tcphdr *)(mtod(args->m, char *) + hlen);

		if ((tcp->th_flags & TH_RST) != 0) {
			m_freem(args->m);
			return;
		}

		ti.ip6 = *ip6;
		ti.th = *tcp;
		ti.th.th_seq = ntohl(ti.th.th_seq);
		ti.th.th_ack = ntohl(ti.th.th_ack);
		ti.ip6.ip6_nxt = IPPROTO_TCP;

		if (ti.th.th_flags & TH_ACK) {
			ack = 0;
			seq = ti.th.th_ack;
			flags = TH_RST;
		} else {
			ack = ti.th.th_seq;
			if (((args->m)->m_flags & M_PKTHDR) != 0) {
				ack += (args->m)->m_pkthdr.len - hlen
					- (ti.th.th_off << 2);
			} else if (ip6->ip6_plen) {
				ack += ntohs(ip6->ip6_plen) + sizeof(*ip6)
					- hlen - (ti.th.th_off << 2);
			} else {
				m_freem(args->m);
				return;
			}
			if (tcp->th_flags & TH_SYN)
				ack++;
			seq = 0;
			flags = TH_RST|TH_ACK;
		}
		bcopy(&ti, ip6, sizeof(ti));
		tcp_respond(NULL, ip6, (struct tcphdr *)(ip6 + 1),
			args->m, ack, seq, flags);

	} else if (code != ICMP6_UNREACH_RST) { /* Send an ICMPv6 unreach. */
		icmp6_error(args->m, ICMP6_DST_UNREACH, code, 0);

	} else
		m_freem(args->m);

	args->m = NULL;
}

#endif /* INET6 */

static u_int64_t norule_counter;	/* counter for ipfw_log(NULL...) */

#define SNPARGS(buf, len) buf + len, sizeof(buf) > len ? sizeof(buf) - len : 0
#define SNP(buf) buf, sizeof(buf)

/*
 * We enter here when we have a rule with O_LOG.
 * XXX this function alone takes about 2Kbytes of code!
 */
static void
ipfw_log(struct ip_fw *f, u_int hlen, struct ip_fw_args *args,
	struct mbuf *m, struct ifnet *oif, u_short offset)
{
	struct ether_header *eh = args->eh;
	char *action;
	int limit_reached = 0;
	char action2[40], proto[128], fragment[32];

	fragment[0] = '\0';
	proto[0] = '\0';

	if (f == NULL) {	/* bogus pkt */
		if (verbose_limit != 0 && norule_counter >= verbose_limit)
			return;
		norule_counter++;
		if (norule_counter == verbose_limit)
			limit_reached = verbose_limit;
		action = "Refuse";
	} else {	/* O_LOG is the first action, find the real one */
		ipfw_insn *cmd = ACTION_PTR(f);
		ipfw_insn_log *l = (ipfw_insn_log *)cmd;

		if (l->max_log != 0 && l->log_left == 0)
			return;
		l->log_left--;
		if (l->log_left == 0)
			limit_reached = l->max_log;
		cmd += F_LEN(cmd);	/* point to first action */
		if (cmd->opcode == O_ALTQ) {
			ipfw_insn_altq *altq = (ipfw_insn_altq *)cmd;

			snprintf(SNPARGS(action2, 0), "Altq %d",
				altq->qid);
			cmd += F_LEN(cmd);
		}
		if (cmd->opcode == O_PROB)
			cmd += F_LEN(cmd);

		action = action2;
		switch (cmd->opcode) {
		case O_DENY:
			action = "Deny";
			break;

		case O_REJECT:
			if (cmd->arg1==ICMP_REJECT_RST)
				action = "Reset";
			else if (cmd->arg1==ICMP_UNREACH_HOST)
				action = "Reject";
			else
				snprintf(SNPARGS(action2, 0), "Unreach %d",
					cmd->arg1);
			break;

		case O_UNREACH6:
			if (cmd->arg1==ICMP6_UNREACH_RST)
				action = "Reset";
			else
				snprintf(SNPARGS(action2, 0), "Unreach %d",
					cmd->arg1);
			break;

		case O_ACCEPT:
			action = "Accept";
			break;
		case O_COUNT:
			action = "Count";
			break;
		case O_DIVERT:
			snprintf(SNPARGS(action2, 0), "Divert %d",
				cmd->arg1);
			break;
		case O_TEE:
			snprintf(SNPARGS(action2, 0), "Tee %d",
				cmd->arg1);
			break;
		case O_SKIPTO:
			snprintf(SNPARGS(action2, 0), "SkipTo %d",
				cmd->arg1);
			break;
		case O_PIPE:
			snprintf(SNPARGS(action2, 0), "Pipe %d",
				cmd->arg1);
			break;
		case O_QUEUE:
			snprintf(SNPARGS(action2, 0), "Queue %d",
				cmd->arg1);
			break;
		case O_FORWARD_IP: {
			ipfw_insn_sa *sa = (ipfw_insn_sa *)cmd;
			int len;

			len = snprintf(SNPARGS(action2, 0), "Forward to %s",
				inet_ntoa(sa->sa.sin_addr));
			if (sa->sa.sin_port)
				snprintf(SNPARGS(action2, len), ":%d",
				    sa->sa.sin_port);
			}
			break;
		case O_NETGRAPH:
			snprintf(SNPARGS(action2, 0), "Netgraph %d",
				cmd->arg1);
			break;
		case O_NGTEE:
			snprintf(SNPARGS(action2, 0), "Ngtee %d",
				cmd->arg1);
			break;
		default:
			action = "UNKNOWN";
			break;
		}
	}

	if (hlen == 0) {	/* non-ip */
		snprintf(SNPARGS(proto, 0), "MAC");

	} else {
		int len;
		char src[48], dst[48];
		struct icmphdr *icmp;
		struct tcphdr *tcp;
		struct udphdr *udp;
		/* Initialize to make compiler happy. */
		struct ip *ip = NULL;
#ifdef INET6
		struct ip6_hdr *ip6 = NULL;
		struct icmp6_hdr *icmp6;
#endif
		src[0] = '\0';
		dst[0] = '\0';
#ifdef INET6
		if (args->f_id.addr_type == 6) {
			snprintf(src, sizeof(src), "[%s]",
			    ip6_sprintf(&args->f_id.src_ip6));
			snprintf(dst, sizeof(dst), "[%s]",
			    ip6_sprintf(&args->f_id.dst_ip6));

			ip6 = (struct ip6_hdr *)mtod(m, struct ip6_hdr *);
			tcp = (struct tcphdr *)(mtod(args->m, char *) + hlen);
			udp = (struct udphdr *)(mtod(args->m, char *) + hlen);
		} else
#endif
		{
			ip = mtod(m, struct ip *);
			tcp = L3HDR(struct tcphdr, ip);
			udp = L3HDR(struct udphdr, ip);

			inet_ntoa_r(ip->ip_src, src);
			inet_ntoa_r(ip->ip_dst, dst);
		}

		switch (args->f_id.proto) {
		case IPPROTO_TCP:
			len = snprintf(SNPARGS(proto, 0), "TCP %s", src);
			if (offset == 0)
				snprintf(SNPARGS(proto, len), ":%d %s:%d",
				    ntohs(tcp->th_sport),
				    dst,
				    ntohs(tcp->th_dport));
			else
				snprintf(SNPARGS(proto, len), " %s", dst);
			break;

		case IPPROTO_UDP:
			len = snprintf(SNPARGS(proto, 0), "UDP %s", src);
			if (offset == 0)
				snprintf(SNPARGS(proto, len), ":%d %s:%d",
				    ntohs(udp->uh_sport),
				    dst,
				    ntohs(udp->uh_dport));
			else
				snprintf(SNPARGS(proto, len), " %s", dst);
			break;

		case IPPROTO_ICMP:
			icmp = L3HDR(struct icmphdr, ip);
			if (offset == 0)
				len = snprintf(SNPARGS(proto, 0),
				    "ICMP:%u.%u ",
				    icmp->icmp_type, icmp->icmp_code);
			else
				len = snprintf(SNPARGS(proto, 0), "ICMP ");
			len += snprintf(SNPARGS(proto, len), "%s", src);
			snprintf(SNPARGS(proto, len), " %s", dst);
			break;
#ifdef INET6
		case IPPROTO_ICMPV6:
			icmp6 = (struct icmp6_hdr *)(mtod(args->m, char *) + hlen);
			if (offset == 0)
				len = snprintf(SNPARGS(proto, 0),
				    "ICMPv6:%u.%u ",
				    icmp6->icmp6_type, icmp6->icmp6_code);
			else
				len = snprintf(SNPARGS(proto, 0), "ICMPv6 ");
			len += snprintf(SNPARGS(proto, len), "%s", src);
			snprintf(SNPARGS(proto, len), " %s", dst);
			break;
#endif
		default:
			len = snprintf(SNPARGS(proto, 0), "P:%d %s",
			    args->f_id.proto, src);
			snprintf(SNPARGS(proto, len), " %s", dst);
			break;
		}

#ifdef INET6
		if (args->f_id.addr_type == 6) {
			if (offset & (IP6F_OFF_MASK | IP6F_MORE_FRAG))
				snprintf(SNPARGS(fragment, 0),
				    " (frag %08x:%d@%d%s)",
				    args->f_id.frag_id6,
				    ntohs(ip6->ip6_plen) - hlen,
				    ntohs(offset & IP6F_OFF_MASK) << 3,
				    (offset & IP6F_MORE_FRAG) ? "+" : "");
		} else
#endif
		{
			int ip_off, ip_len;
			if (eh != NULL) { /* layer 2 packets are as on the wire */
				ip_off = ntohs(ip->ip_off);
				ip_len = ntohs(ip->ip_len);
			} else {
				ip_off = ip->ip_off;
				ip_len = ip->ip_len;
			}
			if (ip_off & (IP_MF | IP_OFFMASK))
				snprintf(SNPARGS(fragment, 0),
				    " (frag %d:%d@%d%s)",
				    ntohs(ip->ip_id), ip_len - (ip->ip_hl << 2),
				    offset << 3,
				    (ip_off & IP_MF) ? "+" : "");
		}
	}
	if (oif || m->m_pkthdr.rcvif)
		log(LOG_SECURITY | LOG_INFO,
		    "ipfw: %d %s %s %s via %s%s\n",
		    f ? f->rulenum : -1,
		    action, proto, oif ? "out" : "in",
		    oif ? oif->if_xname : m->m_pkthdr.rcvif->if_xname,
		    fragment);
	else
		log(LOG_SECURITY | LOG_INFO,
		    "ipfw: %d %s %s [no if info]%s\n",
		    f ? f->rulenum : -1,
		    action, proto, fragment);
	if (limit_reached)
		log(LOG_SECURITY | LOG_NOTICE,
		    "ipfw: limit %d reached on entry %d\n",
		    limit_reached, f ? f->rulenum : -1);
}

/*
 * IMPORTANT: the hash function for dynamic rules must be commutative
 * in source and destination (ip,port), because rules are bidirectional
 * and we want to find both in the same bucket.
 */
static __inline int
hash_packet(struct ipfw_flow_id *id)
{
	u_int32_t i;

#ifdef INET6
	if (IS_IP6_FLOW_ID(id)) 
		i = hash_packet6(id);
	else
#endif /* INET6 */
	i = (id->dst_ip) ^ (id->src_ip) ^ (id->dst_port) ^ (id->src_port);
	i &= (curr_dyn_buckets - 1);
	return i;
}

/**
 * unlink a dynamic rule from a chain. prev is a pointer to
 * the previous one, q is a pointer to the rule to delete,
 * head is a pointer to the head of the queue.
 * Modifies q and potentially also head.
 */
#define UNLINK_DYN_RULE(prev, head, q) {				\
	ipfw_dyn_rule *old_q = q;					\
									\
	/* remove a refcount to the parent */				\
	if (q->dyn_type == O_LIMIT)					\
		q->parent->count--;					\
	DEB(printf("ipfw: unlink entry 0x%08x %d -> 0x%08x %d, %d left\n",\
		(q->id.src_ip), (q->id.src_port),			\
		(q->id.dst_ip), (q->id.dst_port), dyn_count-1 ); )	\
	if (prev != NULL)						\
		prev->next = q = q->next;				\
	else								\
		head = q = q->next;					\
	dyn_count--;							\
	uma_zfree(ipfw_dyn_rule_zone, old_q); }

#define TIME_LEQ(a,b)       ((int)((a)-(b)) <= 0)

/**
 * Remove dynamic rules pointing to "rule", or all of them if rule == NULL.
 *
 * If keep_me == NULL, rules are deleted even if not expired,
 * otherwise only expired rules are removed.
 *
 * The value of the second parameter is also used to point to identify
 * a rule we absolutely do not want to remove (e.g. because we are
 * holding a reference to it -- this is the case with O_LIMIT_PARENT
 * rules). The pointer is only used for comparison, so any non-null
 * value will do.
 */
static void
remove_dyn_rule(struct ip_fw *rule, ipfw_dyn_rule *keep_me)
{
	static u_int32_t last_remove = 0;

#define FORCE (keep_me == NULL)

	ipfw_dyn_rule *prev, *q;
	int i, pass = 0, max_pass = 0;

	IPFW_DYN_LOCK_ASSERT();

	if (ipfw_dyn_v == NULL || dyn_count == 0)
		return;
	/* do not expire more than once per second, it is useless */
	if (!FORCE && last_remove == time_uptime)
		return;
	last_remove = time_uptime;

	/*
	 * because O_LIMIT refer to parent rules, during the first pass only
	 * remove child and mark any pending LIMIT_PARENT, and remove
	 * them in a second pass.
	 */
next_pass:
	for (i = 0 ; i < curr_dyn_buckets ; i++) {
		for (prev=NULL, q = ipfw_dyn_v[i] ; q ; ) {
			/*
			 * Logic can become complex here, so we split tests.
			 */
			if (q == keep_me)
				goto next;
			if (rule != NULL && rule != q->rule)
				goto next; /* not the one we are looking for */
			if (q->dyn_type == O_LIMIT_PARENT) {
				/*
				 * handle parent in the second pass,
				 * record we need one.
				 */
				max_pass = 1;
				if (pass == 0)
					goto next;
				if (FORCE && q->count != 0 ) {
					/* XXX should not happen! */
					printf("ipfw: OUCH! cannot remove rule,"
					     " count %d\n", q->count);
				}
			} else {
				if (!FORCE &&
				    !TIME_LEQ( q->expire, time_uptime ))
					goto next;
			}
             if (q->dyn_type != O_LIMIT_PARENT || !q->count) {
                     UNLINK_DYN_RULE(prev, ipfw_dyn_v[i], q);
                     continue;
             }
next:
			prev=q;
			q=q->next;
		}
	}
	if (pass++ < max_pass)
		goto next_pass;
}


/**
 * lookup a dynamic rule.
 */
static ipfw_dyn_rule *
lookup_dyn_rule_locked(struct ipfw_flow_id *pkt, int *match_direction,
	struct tcphdr *tcp)
{
	/*
	 * stateful ipfw extensions.
	 * Lookup into dynamic session queue
	 */
#define MATCH_REVERSE	0
#define MATCH_FORWARD	1
#define MATCH_NONE	2
#define MATCH_UNKNOWN	3
	int i, dir = MATCH_NONE;
	ipfw_dyn_rule *prev, *q=NULL;

	IPFW_DYN_LOCK_ASSERT();

	if (ipfw_dyn_v == NULL)
		goto done;	/* not found */
	i = hash_packet( pkt );
	for (prev=NULL, q = ipfw_dyn_v[i] ; q != NULL ; ) {
		if (q->dyn_type == O_LIMIT_PARENT && q->count)
			goto next;
		if (TIME_LEQ( q->expire, time_uptime)) { /* expire entry */
			UNLINK_DYN_RULE(prev, ipfw_dyn_v[i], q);
			continue;
		}
		if (pkt->proto == q->id.proto &&
		    q->dyn_type != O_LIMIT_PARENT) {
			if (IS_IP6_FLOW_ID(pkt)) {
			    if (IN6_ARE_ADDR_EQUAL(&(pkt->src_ip6),
				&(q->id.src_ip6)) &&
			    IN6_ARE_ADDR_EQUAL(&(pkt->dst_ip6),
				&(q->id.dst_ip6)) &&
			    pkt->src_port == q->id.src_port &&
			    pkt->dst_port == q->id.dst_port ) {
				dir = MATCH_FORWARD;
				break;
			    }
			    if (IN6_ARE_ADDR_EQUAL(&(pkt->src_ip6),
				    &(q->id.dst_ip6)) &&
				IN6_ARE_ADDR_EQUAL(&(pkt->dst_ip6),
				    &(q->id.src_ip6)) &&
				pkt->src_port == q->id.dst_port &&
				pkt->dst_port == q->id.src_port ) {
				    dir = MATCH_REVERSE;
				    break;
			    }
			} else {
			    if (pkt->src_ip == q->id.src_ip &&
				pkt->dst_ip == q->id.dst_ip &&
				pkt->src_port == q->id.src_port &&
				pkt->dst_port == q->id.dst_port ) {
				    dir = MATCH_FORWARD;
				    break;
			    }
			    if (pkt->src_ip == q->id.dst_ip &&
				pkt->dst_ip == q->id.src_ip &&
				pkt->src_port == q->id.dst_port &&
				pkt->dst_port == q->id.src_port ) {
				    dir = MATCH_REVERSE;
				    break;
			    }
			}
		}
next:
		prev = q;
		q = q->next;
	}
	if (q == NULL)
		goto done; /* q = NULL, not found */

	if ( prev != NULL) { /* found and not in front */
		prev->next = q->next;
		q->next = ipfw_dyn_v[i];
		ipfw_dyn_v[i] = q;
	}
	if (pkt->proto == IPPROTO_TCP) { /* update state according to flags */
		u_char flags = pkt->flags & (TH_FIN|TH_SYN|TH_RST);

#define BOTH_SYN	(TH_SYN | (TH_SYN << 8))
#define BOTH_FIN	(TH_FIN | (TH_FIN << 8))
		q->state |= (dir == MATCH_FORWARD ) ? flags : (flags << 8);
		switch (q->state) {
		case TH_SYN:				/* opening */
			q->expire = time_uptime + dyn_syn_lifetime;
			break;

		case BOTH_SYN:			/* move to established */
		case BOTH_SYN | TH_FIN :	/* one side tries to close */
		case BOTH_SYN | (TH_FIN << 8) :
 			if (tcp) {
#define _SEQ_GE(a,b) ((int)(a) - (int)(b) >= 0)
			    u_int32_t ack = ntohl(tcp->th_ack);
			    if (dir == MATCH_FORWARD) {
				if (q->ack_fwd == 0 || _SEQ_GE(ack, q->ack_fwd))
				    q->ack_fwd = ack;
				else { /* ignore out-of-sequence */
				    break;
				}
			    } else {
				if (q->ack_rev == 0 || _SEQ_GE(ack, q->ack_rev))
				    q->ack_rev = ack;
				else { /* ignore out-of-sequence */
				    break;
				}
			    }
			}
			q->expire = time_uptime + dyn_ack_lifetime;
			break;

		case BOTH_SYN | BOTH_FIN:	/* both sides closed */
			if (dyn_fin_lifetime >= dyn_keepalive_period)
				dyn_fin_lifetime = dyn_keepalive_period - 1;
			q->expire = time_uptime + dyn_fin_lifetime;
			break;

		default:
#if 0
			/*
			 * reset or some invalid combination, but can also
			 * occur if we use keep-state the wrong way.
			 */
			if ( (q->state & ((TH_RST << 8)|TH_RST)) == 0)
				printf("invalid state: 0x%x\n", q->state);
#endif
			if (dyn_rst_lifetime >= dyn_keepalive_period)
				dyn_rst_lifetime = dyn_keepalive_period - 1;
			q->expire = time_uptime + dyn_rst_lifetime;
			break;
		}
	} else if (pkt->proto == IPPROTO_UDP) {
		q->expire = time_uptime + dyn_udp_lifetime;
	} else {
		/* other protocols */
		q->expire = time_uptime + dyn_short_lifetime;
	}
done:
	if (match_direction)
		*match_direction = dir;
	return q;
}

static ipfw_dyn_rule *
lookup_dyn_rule(struct ipfw_flow_id *pkt, int *match_direction,
	struct tcphdr *tcp)
{
	ipfw_dyn_rule *q;

	IPFW_DYN_LOCK();
	q = lookup_dyn_rule_locked(pkt, match_direction, tcp);
	if (q == NULL)
		IPFW_DYN_UNLOCK();
	/* NB: return table locked when q is not NULL */
	return q;
}

static void
realloc_dynamic_table(void)
{
	IPFW_DYN_LOCK_ASSERT();

	/*
	 * Try reallocation, make sure we have a power of 2 and do
	 * not allow more than 64k entries. In case of overflow,
	 * default to 1024.
	 */

	if (dyn_buckets > 65536)
		dyn_buckets = 1024;
	if ((dyn_buckets & (dyn_buckets-1)) != 0) { /* not a power of 2 */
		dyn_buckets = curr_dyn_buckets; /* reset */
		return;
	}
	curr_dyn_buckets = dyn_buckets;
	if (ipfw_dyn_v != NULL)
		free(ipfw_dyn_v, M_IPFW);
	for (;;) {
		ipfw_dyn_v = malloc(curr_dyn_buckets * sizeof(ipfw_dyn_rule *),
		       M_IPFW, M_NOWAIT | M_ZERO);
		if (ipfw_dyn_v != NULL || curr_dyn_buckets <= 2)
			break;
		curr_dyn_buckets /= 2;
	}
}

/**
 * Install state of type 'type' for a dynamic session.
 * The hash table contains two type of rules:
 * - regular rules (O_KEEP_STATE)
 * - rules for sessions with limited number of sess per user
 *   (O_LIMIT). When they are created, the parent is
 *   increased by 1, and decreased on delete. In this case,
 *   the third parameter is the parent rule and not the chain.
 * - "parent" rules for the above (O_LIMIT_PARENT).
 */
static ipfw_dyn_rule *
add_dyn_rule(struct ipfw_flow_id *id, u_int8_t dyn_type, struct ip_fw *rule)
{
	ipfw_dyn_rule *r;
	int i;

	IPFW_DYN_LOCK_ASSERT();

	if (ipfw_dyn_v == NULL ||
	    (dyn_count == 0 && dyn_buckets != curr_dyn_buckets)) {
		realloc_dynamic_table();
		if (ipfw_dyn_v == NULL)
			return NULL; /* failed ! */
	}
	i = hash_packet(id);

	r = uma_zalloc(ipfw_dyn_rule_zone, M_NOWAIT | M_ZERO);
	if (r == NULL) {
		printf ("ipfw: sorry cannot allocate state\n");
		return NULL;
	}

	/* increase refcount on parent, and set pointer */
	if (dyn_type == O_LIMIT) {
		ipfw_dyn_rule *parent = (ipfw_dyn_rule *)rule;
		if ( parent->dyn_type != O_LIMIT_PARENT)
			panic("invalid parent");
		parent->count++;
		r->parent = parent;
		rule = parent->rule;
	}

	r->id = *id;
	r->expire = time_uptime + dyn_syn_lifetime;
	r->rule = rule;
	r->dyn_type = dyn_type;
	r->pcnt = r->bcnt = 0;
	r->count = 0;

	r->bucket = i;
	r->next = ipfw_dyn_v[i];
	ipfw_dyn_v[i] = r;
	dyn_count++;
	DEB(printf("ipfw: add dyn entry ty %d 0x%08x %d -> 0x%08x %d, total %d\n",
	   dyn_type,
	   (r->id.src_ip), (r->id.src_port),
	   (r->id.dst_ip), (r->id.dst_port),
	   dyn_count ); )
	return r;
}

/**
 * lookup dynamic parent rule using pkt and rule as search keys.
 * If the lookup fails, then install one.
 */
static ipfw_dyn_rule *
lookup_dyn_parent(struct ipfw_flow_id *pkt, struct ip_fw *rule)
{
	ipfw_dyn_rule *q;
	int i;

	IPFW_DYN_LOCK_ASSERT();

	if (ipfw_dyn_v) {
		int is_v6 = IS_IP6_FLOW_ID(pkt);
		i = hash_packet( pkt );
		for (q = ipfw_dyn_v[i] ; q != NULL ; q=q->next)
			if (q->dyn_type == O_LIMIT_PARENT &&
			    rule== q->rule &&
			    pkt->proto == q->id.proto &&
			    pkt->src_port == q->id.src_port &&
			    pkt->dst_port == q->id.dst_port &&
			    (
				(is_v6 &&
				 IN6_ARE_ADDR_EQUAL(&(pkt->src_ip6),
					&(q->id.src_ip6)) &&
				 IN6_ARE_ADDR_EQUAL(&(pkt->dst_ip6),
					&(q->id.dst_ip6))) ||
				(!is_v6 &&
				 pkt->src_ip == q->id.src_ip &&
				 pkt->dst_ip == q->id.dst_ip)
			    )
			) {
				q->expire = time_uptime + dyn_short_lifetime;
				DEB(printf("ipfw: lookup_dyn_parent found 0x%p\n",q);)
				return q;
			}
	}
	return add_dyn_rule(pkt, O_LIMIT_PARENT, rule);
}

/**
 * Install dynamic state for rule type cmd->o.opcode
 *
 * Returns 1 (failure) if state is not installed because of errors or because
 * session limitations are enforced.
 */
static int
install_state(struct ip_fw *rule, ipfw_insn_limit *cmd,
	struct ip_fw_args *args)
{
	static int last_log;

	ipfw_dyn_rule *q;

	DEB(printf("ipfw: install state type %d 0x%08x %u -> 0x%08x %u\n",
	    cmd->o.opcode,
	    (args->f_id.src_ip), (args->f_id.src_port),
	    (args->f_id.dst_ip), (args->f_id.dst_port) );)

	IPFW_DYN_LOCK();

	q = lookup_dyn_rule_locked(&args->f_id, NULL, NULL);

	if (q != NULL) { /* should never occur */
		if (last_log != time_uptime) {
			last_log = time_uptime;
			printf("ipfw: install_state: entry already present, done\n");
		}
		IPFW_DYN_UNLOCK();
		return 0;
	}

	if (dyn_count >= dyn_max)
		/*
		 * Run out of slots, try to remove any expired rule.
		 */
		remove_dyn_rule(NULL, (ipfw_dyn_rule *)1);

	if (dyn_count >= dyn_max) {
		if (last_log != time_uptime) {
			last_log = time_uptime;
			printf("ipfw: install_state: Too many dynamic rules\n");
		}
		IPFW_DYN_UNLOCK();
		return 1; /* cannot install, notify caller */
	}

	switch (cmd->o.opcode) {
	case O_KEEP_STATE: /* bidir rule */
		add_dyn_rule(&args->f_id, O_KEEP_STATE, rule);
		break;

	case O_LIMIT: /* limit number of sessions */
	    {
		u_int16_t limit_mask = cmd->limit_mask;
		struct ipfw_flow_id id;
		ipfw_dyn_rule *parent;

		DEB(printf("ipfw: installing dyn-limit rule %d\n",
		    cmd->conn_limit);)

		id.dst_ip = id.src_ip = 0;
		id.dst_port = id.src_port = 0;
		id.proto = args->f_id.proto;

		if (IS_IP6_FLOW_ID (&(args->f_id))) {
			if (limit_mask & DYN_SRC_ADDR)
				id.src_ip6 = args->f_id.src_ip6;
			if (limit_mask & DYN_DST_ADDR)
				id.dst_ip6 = args->f_id.dst_ip6;
		} else {
			if (limit_mask & DYN_SRC_ADDR)
				id.src_ip = args->f_id.src_ip;
			if (limit_mask & DYN_DST_ADDR)
				id.dst_ip = args->f_id.dst_ip;
		}
		if (limit_mask & DYN_SRC_PORT)
			id.src_port = args->f_id.src_port;
		if (limit_mask & DYN_DST_PORT)
			id.dst_port = args->f_id.dst_port;
		parent = lookup_dyn_parent(&id, rule);
		if (parent == NULL) {
			printf("ipfw: add parent failed\n");
			IPFW_DYN_UNLOCK();
			return 1;
		}
		if (parent->count >= cmd->conn_limit) {
			/*
			 * See if we can remove some expired rule.
			 */
			remove_dyn_rule(rule, parent);
			if (parent->count >= cmd->conn_limit) {
				if (fw_verbose && last_log != time_uptime) {
					last_log = time_uptime;
					log(LOG_SECURITY | LOG_DEBUG,
					    "drop session, too many entries\n");
				}
				IPFW_DYN_UNLOCK();
				return 1;
			}
		}
		add_dyn_rule(&args->f_id, O_LIMIT, (struct ip_fw *)parent);
	    }
		break;
	default:
		printf("ipfw: unknown dynamic rule type %u\n", cmd->o.opcode);
		IPFW_DYN_UNLOCK();
		return 1;
	}
	lookup_dyn_rule_locked(&args->f_id, NULL, NULL); /* XXX just set lifetime */
	IPFW_DYN_UNLOCK();
	return 0;
}

/*
 * Generate a TCP packet, containing either a RST or a keepalive.
 * When flags & TH_RST, we are sending a RST packet, because of a
 * "reset" action matched the packet.
 * Otherwise we are sending a keepalive, and flags & TH_
 */
static struct mbuf *
send_pkt(struct ipfw_flow_id *id, u_int32_t seq, u_int32_t ack, int flags)
{
	struct mbuf *m;
	struct ip *ip;
	struct tcphdr *tcp;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return (NULL);
	m->m_pkthdr.rcvif = (struct ifnet *)0;
	m->m_pkthdr.len = m->m_len = sizeof(struct ip) + sizeof(struct tcphdr);
	m->m_data += max_linkhdr;

	ip = mtod(m, struct ip *);
	bzero(ip, m->m_len);
	tcp = (struct tcphdr *)(ip + 1); /* no IP options */
	ip->ip_p = IPPROTO_TCP;
	tcp->th_off = 5;
	/*
	 * Assume we are sending a RST (or a keepalive in the reverse
	 * direction), swap src and destination addresses and ports.
	 */
	ip->ip_src.s_addr = htonl(id->dst_ip);
	ip->ip_dst.s_addr = htonl(id->src_ip);
	tcp->th_sport = htons(id->dst_port);
	tcp->th_dport = htons(id->src_port);
	if (flags & TH_RST) {	/* we are sending a RST */
		if (flags & TH_ACK) {
			tcp->th_seq = htonl(ack);
			tcp->th_ack = htonl(0);
			tcp->th_flags = TH_RST;
		} else {
			if (flags & TH_SYN)
				seq++;
			tcp->th_seq = htonl(0);
			tcp->th_ack = htonl(seq);
			tcp->th_flags = TH_RST | TH_ACK;
		}
	} else {
		/*
		 * We are sending a keepalive. flags & TH_SYN determines
		 * the direction, forward if set, reverse if clear.
		 * NOTE: seq and ack are always assumed to be correct
		 * as set by the caller. This may be confusing...
		 */
		if (flags & TH_SYN) {
			/*
			 * we have to rewrite the correct addresses!
			 */
			ip->ip_dst.s_addr = htonl(id->dst_ip);
			ip->ip_src.s_addr = htonl(id->src_ip);
			tcp->th_dport = htons(id->dst_port);
			tcp->th_sport = htons(id->src_port);
		}
		tcp->th_seq = htonl(seq);
		tcp->th_ack = htonl(ack);
		tcp->th_flags = TH_ACK;
	}
	/*
	 * set ip_len to the payload size so we can compute
	 * the tcp checksum on the pseudoheader
	 * XXX check this, could save a couple of words ?
	 */
	ip->ip_len = htons(sizeof(struct tcphdr));
	tcp->th_sum = in_cksum(m, m->m_pkthdr.len);
	/*
	 * now fill fields left out earlier
	 */
	ip->ip_ttl = ip_defttl;
	ip->ip_len = m->m_pkthdr.len;
	m->m_flags |= M_SKIP_FIREWALL;
	return (m);
}

/*
 * sends a reject message, consuming the mbuf passed as an argument.
 */
static void
send_reject(struct ip_fw_args *args, int code, u_short offset, int ip_len)
{

	if (code != ICMP_REJECT_RST) { /* Send an ICMP unreach */
		/* We need the IP header in host order for icmp_error(). */
		if (args->eh != NULL) {
			struct ip *ip = mtod(args->m, struct ip *);
			ip->ip_len = ntohs(ip->ip_len);
			ip->ip_off = ntohs(ip->ip_off);
		}
		icmp_error(args->m, ICMP_UNREACH, code, 0L, 0);
	} else if (offset == 0 && args->f_id.proto == IPPROTO_TCP) {
		struct tcphdr *const tcp =
		    L3HDR(struct tcphdr, mtod(args->m, struct ip *));
		if ( (tcp->th_flags & TH_RST) == 0) {
			struct mbuf *m;
			m = send_pkt(&(args->f_id), ntohl(tcp->th_seq),
				ntohl(tcp->th_ack),
				tcp->th_flags | TH_RST);
			if (m != NULL)
				ip_output(m, NULL, NULL, 0, NULL, NULL);
		}
		m_freem(args->m);
	} else
		m_freem(args->m);
	args->m = NULL;
}

/**
 *
 * Given an ip_fw *, lookup_next_rule will return a pointer
 * to the next rule, which can be either the jump
 * target (for skipto instructions) or the next one in the list (in
 * all other cases including a missing jump target).
 * The result is also written in the "next_rule" field of the rule.
 * Backward jumps are not allowed, so start looking from the next
 * rule...
 *
 * This never returns NULL -- in case we do not have an exact match,
 * the next rule is returned. When the ruleset is changed,
 * pointers are flushed so we are always correct.
 */

static struct ip_fw *
lookup_next_rule(struct ip_fw *me)
{
	struct ip_fw *rule = NULL;
	ipfw_insn *cmd;

	/* look for action, in case it is a skipto */
	cmd = ACTION_PTR(me);
	if (cmd->opcode == O_LOG)
		cmd += F_LEN(cmd);
	if (cmd->opcode == O_ALTQ)
		cmd += F_LEN(cmd);
	if ( cmd->opcode == O_SKIPTO )
		for (rule = me->next; rule ; rule = rule->next)
			if (rule->rulenum >= cmd->arg1)
				break;
	if (rule == NULL)			/* failure or not a skipto */
		rule = me->next;
	me->next_rule = rule;
	return rule;
}

static void
init_tables(struct ip_fw_chain *ch)
{
	int i;

	for (i = 0; i < IPFW_TABLES_MAX; i++)
		rn_inithead((void **)&ch->tables[i], 32);
}

static int
add_table_entry(struct ip_fw_chain *ch, uint16_t tbl, in_addr_t addr,
	uint8_t mlen, uint32_t value)
{
	struct radix_node_head *rnh;
	struct table_entry *ent;

	if (tbl >= IPFW_TABLES_MAX)
		return (EINVAL);
	rnh = ch->tables[tbl];
	ent = malloc(sizeof(*ent), M_IPFW_TBL, M_NOWAIT | M_ZERO);
	if (ent == NULL)
		return (ENOMEM);
	ent->value = value;
	ent->addr.sin_len = ent->mask.sin_len = 8;
	ent->mask.sin_addr.s_addr = htonl(mlen ? ~((1 << (32 - mlen)) - 1) : 0);
	ent->addr.sin_addr.s_addr = addr & ent->mask.sin_addr.s_addr;
	IPFW_WLOCK(&layer3_chain);
	if (rnh->rnh_addaddr(&ent->addr, &ent->mask, rnh, (void *)ent) ==
	    NULL) {
		IPFW_WUNLOCK(&layer3_chain);
		free(ent, M_IPFW_TBL);
		return (EEXIST);
	}
	IPFW_WUNLOCK(&layer3_chain);
	return (0);
}

static int
del_table_entry(struct ip_fw_chain *ch, uint16_t tbl, in_addr_t addr,
	uint8_t mlen)
{
	struct radix_node_head *rnh;
	struct table_entry *ent;
	struct sockaddr_in sa, mask;

	if (tbl >= IPFW_TABLES_MAX)
		return (EINVAL);
	rnh = ch->tables[tbl];
	sa.sin_len = mask.sin_len = 8;
	mask.sin_addr.s_addr = htonl(mlen ? ~((1 << (32 - mlen)) - 1) : 0);
	sa.sin_addr.s_addr = addr & mask.sin_addr.s_addr;
	IPFW_WLOCK(ch);
	ent = (struct table_entry *)rnh->rnh_deladdr(&sa, &mask, rnh);
	if (ent == NULL) {
		IPFW_WUNLOCK(ch);
		return (ESRCH);
	}
	IPFW_WUNLOCK(ch);
	free(ent, M_IPFW_TBL);
	return (0);
}

static int
flush_table_entry(struct radix_node *rn, void *arg)
{
	struct radix_node_head * const rnh = arg;
	struct table_entry *ent;

	ent = (struct table_entry *)
	    rnh->rnh_deladdr(rn->rn_key, rn->rn_mask, rnh);
	if (ent != NULL)
		free(ent, M_IPFW_TBL);
	return (0);
}

static int
flush_table(struct ip_fw_chain *ch, uint16_t tbl)
{
	struct radix_node_head *rnh;

	IPFW_WLOCK_ASSERT(ch);

	if (tbl >= IPFW_TABLES_MAX)
		return (EINVAL);
	rnh = ch->tables[tbl];
	rnh->rnh_walktree(rnh, flush_table_entry, rnh);
	return (0);
}

static void
flush_tables(struct ip_fw_chain *ch)
{
	uint16_t tbl;

	IPFW_WLOCK_ASSERT(ch);

	for (tbl = 0; tbl < IPFW_TABLES_MAX; tbl++)
		flush_table(ch, tbl);
}

static int
lookup_table(struct ip_fw_chain *ch, uint16_t tbl, in_addr_t addr,
	uint32_t *val)
{
	struct radix_node_head *rnh;
	struct table_entry *ent;
	struct sockaddr_in sa;

	if (tbl >= IPFW_TABLES_MAX)
		return (0);
	rnh = ch->tables[tbl];
	sa.sin_len = 8;
	sa.sin_addr.s_addr = addr;
	ent = (struct table_entry *)(rnh->rnh_lookup(&sa, NULL, rnh));
	if (ent != NULL) {
		*val = ent->value;
		return (1);
	}
	return (0);
}

static int
count_table_entry(struct radix_node *rn, void *arg)
{
	u_int32_t * const cnt = arg;

	(*cnt)++;
	return (0);
}

static int
count_table(struct ip_fw_chain *ch, uint32_t tbl, uint32_t *cnt)
{
	struct radix_node_head *rnh;

	if (tbl >= IPFW_TABLES_MAX)
		return (EINVAL);
	rnh = ch->tables[tbl];
	*cnt = 0;
	rnh->rnh_walktree(rnh, count_table_entry, cnt);
	return (0);
}

static int
dump_table_entry(struct radix_node *rn, void *arg)
{
	struct table_entry * const n = (struct table_entry *)rn;
	ipfw_table * const tbl = arg;
	ipfw_table_entry *ent;

	if (tbl->cnt == tbl->size)
		return (1);
	ent = &tbl->ent[tbl->cnt];
	ent->tbl = tbl->tbl;
	if (in_nullhost(n->mask.sin_addr))
		ent->masklen = 0;
	else
		ent->masklen = 33 - ffs(ntohl(n->mask.sin_addr.s_addr));
	ent->addr = n->addr.sin_addr.s_addr;
	ent->value = n->value;
	tbl->cnt++;
	return (0);
}

static int
dump_table(struct ip_fw_chain *ch, ipfw_table *tbl)
{
	struct radix_node_head *rnh;

	IPFW_WLOCK_ASSERT(ch);

	if (tbl->tbl >= IPFW_TABLES_MAX)
		return (EINVAL);
	rnh = ch->tables[tbl->tbl];
	tbl->cnt = 0;
	rnh->rnh_walktree(rnh, dump_table_entry, tbl);
	return (0);
}

static void
fill_ugid_cache(struct inpcb *inp, struct ip_fw_ugid *ugp)
{
	struct ucred *cr;

	if (inp->inp_socket != NULL) {
		cr = inp->inp_socket->so_cred;
		ugp->fw_prid = jailed(cr) ?
		    cr->cr_prison->pr_id : -1;
		ugp->fw_uid = cr->cr_uid;
		ugp->fw_ngroups = cr->cr_ngroups;
		bcopy(cr->cr_groups, ugp->fw_groups,
		    sizeof(ugp->fw_groups));
	}
}

static int
check_uidgid(ipfw_insn_u32 *insn,
	int proto, struct ifnet *oif,
	struct in_addr dst_ip, u_int16_t dst_port,
	struct in_addr src_ip, u_int16_t src_port,
	struct ip_fw_ugid *ugp, int *lookup, struct inpcb *inp)
{
	struct inpcbinfo *pi;
	int wildcard;
	struct inpcb *pcb;
	int match;
	gid_t *gp;

	/*
	 * Check to see if the UDP or TCP stack supplied us with
	 * the PCB. If so, rather then holding a lock and looking
	 * up the PCB, we can use the one that was supplied.
	 */
	if (inp && *lookup == 0) {
		INP_LOCK_ASSERT(inp);
		if (inp->inp_socket != NULL) {
			fill_ugid_cache(inp, ugp);
			*lookup = 1;
		}
	}
	/*
	 * If we have already been here and the packet has no
	 * PCB entry associated with it, then we can safely
	 * assume that this is a no match.
	 */
	if (*lookup == -1)
		return (0);
	if (proto == IPPROTO_TCP) {
		wildcard = 0;
		pi = &tcbinfo;
	} else if (proto == IPPROTO_UDP) {
		wildcard = 1;
		pi = &udbinfo;
	} else
		return 0;
	match = 0;
	if (*lookup == 0) {
		INP_INFO_RLOCK(pi);
		pcb =  (oif) ?
			in_pcblookup_hash(pi,
				dst_ip, htons(dst_port),
				src_ip, htons(src_port),
				wildcard, oif) :
			in_pcblookup_hash(pi,
				src_ip, htons(src_port),
				dst_ip, htons(dst_port),
				wildcard, NULL);
		if (pcb != NULL) {
			INP_LOCK(pcb);
			if (pcb->inp_socket != NULL) {
				fill_ugid_cache(pcb, ugp);
				*lookup = 1;
			}
			INP_UNLOCK(pcb);
		}
		INP_INFO_RUNLOCK(pi);
		if (*lookup == 0) {
			/*
			 * If the lookup did not yield any results, there
			 * is no sense in coming back and trying again. So
			 * we can set lookup to -1 and ensure that we wont
			 * bother the pcb system again.
			 */
			*lookup = -1;
			return (0);
		}
	} 
	if (insn->o.opcode == O_UID)
		match = (ugp->fw_uid == (uid_t)insn->d[0]);
	else if (insn->o.opcode == O_GID) {
		for (gp = ugp->fw_groups;
			gp < &ugp->fw_groups[ugp->fw_ngroups]; gp++)
			if (*gp == (gid_t)insn->d[0]) {
				match = 1;
				break;
			}
	} else if (insn->o.opcode == O_JAIL)
		match = (ugp->fw_prid == (int)insn->d[0]);
	return match;
}

/*
 * The main check routine for the firewall.
 *
 * All arguments are in args so we can modify them and return them
 * back to the caller.
 *
 * Parameters:
 *
 *	args->m	(in/out) The packet; we set to NULL when/if we nuke it.
 *		Starts with the IP header.
 *	args->eh (in)	Mac header if present, or NULL for layer3 packet.
 *	args->oif	Outgoing interface, or NULL if packet is incoming.
 *		The incoming interface is in the mbuf. (in)
 *	args->divert_rule (in/out)
 *		Skip up to the first rule past this rule number;
 *		upon return, non-zero port number for divert or tee.
 *
 *	args->rule	Pointer to the last matching rule (in/out)
 *	args->next_hop	Socket we are forwarding to (out).
 *	args->f_id	Addresses grabbed from the packet (out)
 * 	args->cookie	a cookie depending on rule action
 *
 * Return value:
 *
 *	IP_FW_PASS	the packet must be accepted
 *	IP_FW_DENY	the packet must be dropped
 *	IP_FW_DIVERT	divert packet, port in m_tag
 *	IP_FW_TEE	tee packet, port in m_tag
 *	IP_FW_DUMMYNET	to dummynet, pipe in args->cookie
 *	IP_FW_NETGRAPH	into netgraph, cookie args->cookie
 *
 */

int
ipfw_chk(struct ip_fw_args *args)
{
	/*
	 * Local variables hold state during the processing of a packet.
	 *
	 * IMPORTANT NOTE: to speed up the processing of rules, there
	 * are some assumption on the values of the variables, which
	 * are documented here. Should you change them, please check
	 * the implementation of the various instructions to make sure
	 * that they still work.
	 *
	 * args->eh	The MAC header. It is non-null for a layer2
	 *	packet, it is NULL for a layer-3 packet.
	 *
	 * m | args->m	Pointer to the mbuf, as received from the caller.
	 *	It may change if ipfw_chk() does an m_pullup, or if it
	 *	consumes the packet because it calls send_reject().
	 *	XXX This has to change, so that ipfw_chk() never modifies
	 *	or consumes the buffer.
	 * ip	is simply an alias of the value of m, and it is kept
	 *	in sync with it (the packet is	supposed to start with
	 *	the ip header).
	 */
	struct mbuf *m = args->m;
	struct ip *ip = mtod(m, struct ip *);

	/*
	 * For rules which contain uid/gid or jail constraints, cache
	 * a copy of the users credentials after the pcb lookup has been
	 * executed. This will speed up the processing of rules with
	 * these types of constraints, as well as decrease contention
	 * on pcb related locks.
	 */
	struct ip_fw_ugid fw_ugid_cache;
	int ugid_lookup = 0;

	/*
	 * divinput_flags	If non-zero, set to the IP_FW_DIVERT_*_FLAG
	 *	associated with a packet input on a divert socket.  This
	 *	will allow to distinguish traffic and its direction when
	 *	it originates from a divert socket.
	 */
	u_int divinput_flags = 0;

	/*
	 * oif | args->oif	If NULL, ipfw_chk has been called on the
	 *	inbound path (ether_input, ip_input).
	 *	If non-NULL, ipfw_chk has been called on the outbound path
	 *	(ether_output, ip_output).
	 */
	struct ifnet *oif = args->oif;

	struct ip_fw *f = NULL;		/* matching rule */
	int retval = 0;

	/*
	 * hlen	The length of the IP header.
	 */
	u_int hlen = 0;		/* hlen >0 means we have an IP pkt */

	/*
	 * offset	The offset of a fragment. offset != 0 means that
	 *	we have a fragment at this offset of an IPv4 packet.
	 *	offset == 0 means that (if this is an IPv4 packet)
	 *	this is the first or only fragment.
	 *	For IPv6 offset == 0 means there is no Fragment Header. 
	 *	If offset != 0 for IPv6 always use correct mask to
	 *	get the correct offset because we add IP6F_MORE_FRAG
	 *	to be able to dectect the first fragment which would
	 *	otherwise have offset = 0.
	 */
	u_short offset = 0;

	/*
	 * Local copies of addresses. They are only valid if we have
	 * an IP packet.
	 *
	 * proto	The protocol. Set to 0 for non-ip packets,
	 *	or to the protocol read from the packet otherwise.
	 *	proto != 0 means that we have an IPv4 packet.
	 *
	 * src_port, dst_port	port numbers, in HOST format. Only
	 *	valid for TCP and UDP packets.
	 *
	 * src_ip, dst_ip	ip addresses, in NETWORK format.
	 *	Only valid for IPv4 packets.
	 */
	u_int8_t proto;
	u_int16_t src_port = 0, dst_port = 0;	/* NOTE: host format	*/
	struct in_addr src_ip, dst_ip;		/* NOTE: network format	*/
	u_int16_t ip_len=0;
	int pktlen;

	/*
	 * dyn_dir = MATCH_UNKNOWN when rules unchecked,
	 * 	MATCH_NONE when checked and not matched (q = NULL),
	 *	MATCH_FORWARD or MATCH_REVERSE otherwise (q != NULL)
	 */
	int dyn_dir = MATCH_UNKNOWN;
	ipfw_dyn_rule *q = NULL;
	struct ip_fw_chain *chain = &layer3_chain;
	struct m_tag *mtag;

	/*
	 * We store in ulp a pointer to the upper layer protocol header.
	 * In the ipv4 case this is easy to determine from the header,
	 * but for ipv6 we might have some additional headers in the middle.
	 * ulp is NULL if not found.
	 */
	void *ulp = NULL;		/* upper layer protocol pointer. */
	/* XXX ipv6 variables */
	int is_ipv6 = 0;
	u_int16_t ext_hd = 0;	/* bits vector for extension header filtering */
	/* end of ipv6 variables */
	int is_ipv4 = 0;

	if (m->m_flags & M_SKIP_FIREWALL)
		return (IP_FW_PASS);	/* accept */

	pktlen = m->m_pkthdr.len;
	proto = args->f_id.proto = 0;	/* mark f_id invalid */
		/* XXX 0 is a valid proto: IP/IPv6 Hop-by-Hop Option */

/*
 * PULLUP_TO(len, p, T) makes sure that len + sizeof(T) is contiguous,
 * then it sets p to point at the offset "len" in the mbuf. WARNING: the
 * pointer might become stale after other pullups (but we never use it
 * this way).
 */
#define PULLUP_TO(len, p, T)						\
do {									\
	int x = (len) + sizeof(T);					\
	if ((m)->m_len < x) {						\
		args->m = m = m_pullup(m, x);				\
		if (m == NULL)						\
			goto pullup_failed;				\
	}								\
	p = (mtod(m, char *) + (len));					\
} while (0)

	/* Identify IP packets and fill up variables. */
	if (pktlen >= sizeof(struct ip6_hdr) &&
	    (args->eh == NULL || ntohs(args->eh->ether_type)==ETHERTYPE_IPV6) &&
	    mtod(m, struct ip *)->ip_v == 6) {
		is_ipv6 = 1;
		args->f_id.addr_type = 6;
		hlen = sizeof(struct ip6_hdr);
		proto = mtod(m, struct ip6_hdr *)->ip6_nxt;

		/* Search extension headers to find upper layer protocols */
		while (ulp == NULL) {
			switch (proto) {
			case IPPROTO_ICMPV6:
				PULLUP_TO(hlen, ulp, struct icmp6_hdr);
				args->f_id.flags = ICMP6(ulp)->icmp6_type;
				break;

			case IPPROTO_TCP:
				PULLUP_TO(hlen, ulp, struct tcphdr);
				dst_port = TCP(ulp)->th_dport;
				src_port = TCP(ulp)->th_sport;
				args->f_id.flags = TCP(ulp)->th_flags;
				break;

			case IPPROTO_UDP:
				PULLUP_TO(hlen, ulp, struct udphdr);
				dst_port = UDP(ulp)->uh_dport;
				src_port = UDP(ulp)->uh_sport;
				break;

			case IPPROTO_HOPOPTS:	/* RFC 2460 */
				PULLUP_TO(hlen, ulp, struct ip6_hbh);
				ext_hd |= EXT_HOPOPTS;
				hlen += (((struct ip6_hbh *)ulp)->ip6h_len + 1) << 3;
				proto = ((struct ip6_hbh *)ulp)->ip6h_nxt;
				ulp = NULL;
				break;

			case IPPROTO_ROUTING:	/* RFC 2460 */
				PULLUP_TO(hlen, ulp, struct ip6_rthdr);
				if (((struct ip6_rthdr *)ulp)->ip6r_type != 0) {
					printf("IPFW2: IPV6 - Unknown Routing "
					    "Header type(%d)\n",
					    ((struct ip6_rthdr *)ulp)->ip6r_type);
					if (fw_deny_unknown_exthdrs)
					    return (IP_FW_DENY);
					break;
				}
				ext_hd |= EXT_ROUTING;
				hlen += (((struct ip6_rthdr *)ulp)->ip6r_len + 1) << 3;
				proto = ((struct ip6_rthdr *)ulp)->ip6r_nxt;
				ulp = NULL;
				break;

			case IPPROTO_FRAGMENT:	/* RFC 2460 */
				PULLUP_TO(hlen, ulp, struct ip6_frag);
				ext_hd |= EXT_FRAGMENT;
				hlen += sizeof (struct ip6_frag);
				proto = ((struct ip6_frag *)ulp)->ip6f_nxt;
				offset = ((struct ip6_frag *)ulp)->ip6f_offlg &
					IP6F_OFF_MASK;
				/* Add IP6F_MORE_FRAG for offset of first
				 * fragment to be != 0. */
				offset |= ((struct ip6_frag *)ulp)->ip6f_offlg &
					IP6F_MORE_FRAG;
				if (offset == 0) {
					printf("IPFW2: IPV6 - Invalid Fragment "
					    "Header\n");
					if (fw_deny_unknown_exthdrs)
					    return (IP_FW_DENY);
					break;
				}
				args->f_id.frag_id6 =
				    ntohl(((struct ip6_frag *)ulp)->ip6f_ident);
				ulp = NULL;
				break;

			case IPPROTO_DSTOPTS:	/* RFC 2460 */
				PULLUP_TO(hlen, ulp, struct ip6_hbh);
				ext_hd |= EXT_DSTOPTS;
				hlen += (((struct ip6_hbh *)ulp)->ip6h_len + 1) << 3;
				proto = ((struct ip6_hbh *)ulp)->ip6h_nxt;
				ulp = NULL;
				break;

			case IPPROTO_AH:	/* RFC 2402 */
				PULLUP_TO(hlen, ulp, struct ip6_ext);
				ext_hd |= EXT_AH;
				hlen += (((struct ip6_ext *)ulp)->ip6e_len + 2) << 2;
				proto = ((struct ip6_ext *)ulp)->ip6e_nxt;
				ulp = NULL;
				break;

			case IPPROTO_ESP:	/* RFC 2406 */
				PULLUP_TO(hlen, ulp, uint32_t);	/* SPI, Seq# */
				/* Anything past Seq# is variable length and
				 * data past this ext. header is encrypted. */
				ext_hd |= EXT_ESP;
				break;

			case IPPROTO_NONE:	/* RFC 2460 */
				PULLUP_TO(hlen, ulp, struct ip6_ext);
				/* Packet ends here. if ip6e_len!=0 octets
				 * must be ignored. */
				break;

			case IPPROTO_OSPFIGP:
				/* XXX OSPF header check? */
				PULLUP_TO(hlen, ulp, struct ip6_ext);
				break;

			default:
				printf("IPFW2: IPV6 - Unknown Extension "
				    "Header(%d), ext_hd=%x\n", proto, ext_hd);
				if (fw_deny_unknown_exthdrs)
				    return (IP_FW_DENY);
				break;
			} /*switch */
		}
		args->f_id.src_ip6 = mtod(m,struct ip6_hdr *)->ip6_src;
		args->f_id.dst_ip6 = mtod(m,struct ip6_hdr *)->ip6_dst;
		args->f_id.src_ip = 0;
		args->f_id.dst_ip = 0;
		args->f_id.flow_id6 = ntohl(mtod(m, struct ip6_hdr *)->ip6_flow);
	} else if (pktlen >= sizeof(struct ip) &&
	    (args->eh == NULL || ntohs(args->eh->ether_type) == ETHERTYPE_IP) &&
	    mtod(m, struct ip *)->ip_v == 4) {
	    	is_ipv4 = 1;
		ip = mtod(m, struct ip *);
		hlen = ip->ip_hl << 2;
		args->f_id.addr_type = 4;

		/*
		 * Collect parameters into local variables for faster matching.
		 */
		proto = ip->ip_p;
		src_ip = ip->ip_src;
		dst_ip = ip->ip_dst;
		if (args->eh != NULL) { /* layer 2 packets are as on the wire */
			offset = ntohs(ip->ip_off) & IP_OFFMASK;
			ip_len = ntohs(ip->ip_len);
		} else {
			offset = ip->ip_off & IP_OFFMASK;
			ip_len = ip->ip_len;
		}
		pktlen = ip_len < pktlen ? ip_len : pktlen;

		if (offset == 0) {
			switch (proto) {
			case IPPROTO_TCP:
				PULLUP_TO(hlen, ulp, struct tcphdr);
				dst_port = TCP(ulp)->th_dport;
				src_port = TCP(ulp)->th_sport;
				args->f_id.flags = TCP(ulp)->th_flags;
				break;

			case IPPROTO_UDP:
				PULLUP_TO(hlen, ulp, struct udphdr);
				dst_port = UDP(ulp)->uh_dport;
				src_port = UDP(ulp)->uh_sport;
				break;

			case IPPROTO_ICMP:
				PULLUP_TO(hlen, ulp, struct icmphdr);
				args->f_id.flags = ICMP(ulp)->icmp_type;
				break;

			default:
				break;
			}
		}

		args->f_id.src_ip = ntohl(src_ip.s_addr);
		args->f_id.dst_ip = ntohl(dst_ip.s_addr);
	}
#undef PULLUP_TO
	if (proto) { /* we may have port numbers, store them */
		args->f_id.proto = proto;
		args->f_id.src_port = src_port = ntohs(src_port);
		args->f_id.dst_port = dst_port = ntohs(dst_port);
	}

	IPFW_RLOCK(chain);
	mtag = m_tag_find(m, PACKET_TAG_DIVERT, NULL);
	if (args->rule) {
		/*
		 * Packet has already been tagged. Look for the next rule
		 * to restart processing.
		 *
		 * If fw_one_pass != 0 then just accept it.
		 * XXX should not happen here, but optimized out in
		 * the caller.
		 */
		if (fw_one_pass) {
			IPFW_RUNLOCK(chain);
			return (IP_FW_PASS);
		}

		f = args->rule->next_rule;
		if (f == NULL)
			f = lookup_next_rule(args->rule);
	} else {
		/*
		 * Find the starting rule. It can be either the first
		 * one, or the one after divert_rule if asked so.
		 */
		int skipto = mtag ? divert_cookie(mtag) : 0;

		f = chain->rules;
		if (args->eh == NULL && skipto != 0) {
			if (skipto >= IPFW_DEFAULT_RULE) {
				IPFW_RUNLOCK(chain);
				return (IP_FW_DENY); /* invalid */
			}
			while (f && f->rulenum <= skipto)
				f = f->next;
			if (f == NULL) {	/* drop packet */
				IPFW_RUNLOCK(chain);
				return (IP_FW_DENY);
			}
		}
	}
	/* reset divert rule to avoid confusion later */
	if (mtag) {
		divinput_flags = divert_info(mtag) &
		    (IP_FW_DIVERT_OUTPUT_FLAG | IP_FW_DIVERT_LOOPBACK_FLAG);
		m_tag_delete(m, mtag);
	}

	/*
	 * Now scan the rules, and parse microinstructions for each rule.
	 */
	for (; f; f = f->next) {
		int l, cmdlen;
		ipfw_insn *cmd;
		int skip_or; /* skip rest of OR block */

again:
		if (set_disable & (1 << f->set) )
			continue;

		skip_or = 0;
		for (l = f->cmd_len, cmd = f->cmd ; l > 0 ;
		    l -= cmdlen, cmd += cmdlen) {
			int match;

			/*
			 * check_body is a jump target used when we find a
			 * CHECK_STATE, and need to jump to the body of
			 * the target rule.
			 */

check_body:
			cmdlen = F_LEN(cmd);
			/*
			 * An OR block (insn_1 || .. || insn_n) has the
			 * F_OR bit set in all but the last instruction.
			 * The first match will set "skip_or", and cause
			 * the following instructions to be skipped until
			 * past the one with the F_OR bit clear.
			 */
			if (skip_or) {		/* skip this instruction */
				if ((cmd->len & F_OR) == 0)
					skip_or = 0;	/* next one is good */
				continue;
			}
			match = 0; /* set to 1 if we succeed */

			switch (cmd->opcode) {
			/*
			 * The first set of opcodes compares the packet's
			 * fields with some pattern, setting 'match' if a
			 * match is found. At the end of the loop there is
			 * logic to deal with F_NOT and F_OR flags associated
			 * with the opcode.
			 */
			case O_NOP:
				match = 1;
				break;

			case O_FORWARD_MAC:
				printf("ipfw: opcode %d unimplemented\n",
				    cmd->opcode);
				break;

			case O_GID:
			case O_UID:
			case O_JAIL:
				/*
				 * We only check offset == 0 && proto != 0,
				 * as this ensures that we have a
				 * packet with the ports info.
				 */
				if (offset!=0)
					break;
				if (is_ipv6) /* XXX to be fixed later */
					break;
				if (proto == IPPROTO_TCP ||
				    proto == IPPROTO_UDP)
					match = check_uidgid(
						    (ipfw_insn_u32 *)cmd,
						    proto, oif,
						    dst_ip, dst_port,
						    src_ip, src_port, &fw_ugid_cache,
						    &ugid_lookup, args->inp);
				break;

			case O_RECV:
				match = iface_match(m->m_pkthdr.rcvif,
				    (ipfw_insn_if *)cmd);
				break;

			case O_XMIT:
				match = iface_match(oif, (ipfw_insn_if *)cmd);
				break;

			case O_VIA:
				match = iface_match(oif ? oif :
				    m->m_pkthdr.rcvif, (ipfw_insn_if *)cmd);
				break;

			case O_MACADDR2:
				if (args->eh != NULL) {	/* have MAC header */
					u_int32_t *want = (u_int32_t *)
						((ipfw_insn_mac *)cmd)->addr;
					u_int32_t *mask = (u_int32_t *)
						((ipfw_insn_mac *)cmd)->mask;
					u_int32_t *hdr = (u_int32_t *)args->eh;

					match =
					    ( want[0] == (hdr[0] & mask[0]) &&
					      want[1] == (hdr[1] & mask[1]) &&
					      want[2] == (hdr[2] & mask[2]) );
				}
				break;

			case O_MAC_TYPE:
				if (args->eh != NULL) {
					u_int16_t t =
					    ntohs(args->eh->ether_type);
					u_int16_t *p =
					    ((ipfw_insn_u16 *)cmd)->ports;
					int i;

					for (i = cmdlen - 1; !match && i>0;
					    i--, p += 2)
						match = (t>=p[0] && t<=p[1]);
				}
				break;

			case O_FRAG:
				match = (offset != 0);
				break;

			case O_IN:	/* "out" is "not in" */
				match = (oif == NULL);
				break;

			case O_LAYER2:
				match = (args->eh != NULL);
				break;

			case O_DIVERTED:
				match = (cmd->arg1 & 1 && divinput_flags &
				    IP_FW_DIVERT_LOOPBACK_FLAG) ||
					(cmd->arg1 & 2 && divinput_flags &
				    IP_FW_DIVERT_OUTPUT_FLAG);
				break;

			case O_PROTO:
				/*
				 * We do not allow an arg of 0 so the
				 * check of "proto" only suffices.
				 */
				match = (proto == cmd->arg1);
				break;

			case O_IP_SRC:
				match = is_ipv4 &&
				    (((ipfw_insn_ip *)cmd)->addr.s_addr ==
				    src_ip.s_addr);
				break;

			case O_IP_SRC_LOOKUP:
			case O_IP_DST_LOOKUP:
				if (is_ipv4) {
				    uint32_t a =
					(cmd->opcode == O_IP_DST_LOOKUP) ?
					    dst_ip.s_addr : src_ip.s_addr;
				    uint32_t v;

				    match = lookup_table(chain, cmd->arg1, a,
					&v);
				    if (!match)
					break;
				    if (cmdlen == F_INSN_SIZE(ipfw_insn_u32))
					match =
					    ((ipfw_insn_u32 *)cmd)->d[0] == v;
				}
				break;

			case O_IP_SRC_MASK:
			case O_IP_DST_MASK:
				if (is_ipv4) {
				    uint32_t a =
					(cmd->opcode == O_IP_DST_MASK) ?
					    dst_ip.s_addr : src_ip.s_addr;
				    uint32_t *p = ((ipfw_insn_u32 *)cmd)->d;
				    int i = cmdlen-1;

				    for (; !match && i>0; i-= 2, p+= 2)
					match = (p[0] == (a & p[1]));
				}
				break;

			case O_IP_SRC_ME:
				if (is_ipv4) {
					struct ifnet *tif;

					INADDR_TO_IFP(src_ip, tif);
					match = (tif != NULL);
				}
				break;

			case O_IP_DST_SET:
			case O_IP_SRC_SET:
				if (is_ipv4) {
					u_int32_t *d = (u_int32_t *)(cmd+1);
					u_int32_t addr =
					    cmd->opcode == O_IP_DST_SET ?
						args->f_id.dst_ip :
						args->f_id.src_ip;

					    if (addr < d[0])
						    break;
					    addr -= d[0]; /* subtract base */
					    match = (addr < cmd->arg1) &&
						( d[ 1 + (addr>>5)] &
						  (1<<(addr & 0x1f)) );
				}
				break;

			case O_IP_DST:
				match = is_ipv4 &&
				    (((ipfw_insn_ip *)cmd)->addr.s_addr ==
				    dst_ip.s_addr);
				break;

			case O_IP_DST_ME:
				if (is_ipv4) {
					struct ifnet *tif;

					INADDR_TO_IFP(dst_ip, tif);
					match = (tif != NULL);
				}
				break;

			case O_IP_SRCPORT:
			case O_IP_DSTPORT:
				/*
				 * offset == 0 && proto != 0 is enough
				 * to guarantee that we have a
				 * packet with port info.
				 */
				if ((proto==IPPROTO_UDP || proto==IPPROTO_TCP)
				    && offset == 0) {
					u_int16_t x =
					    (cmd->opcode == O_IP_SRCPORT) ?
						src_port : dst_port ;
					u_int16_t *p =
					    ((ipfw_insn_u16 *)cmd)->ports;
					int i;

					for (i = cmdlen - 1; !match && i>0;
					    i--, p += 2)
						match = (x>=p[0] && x<=p[1]);
				}
				break;

			case O_ICMPTYPE:
				match = (offset == 0 && proto==IPPROTO_ICMP &&
				    icmptype_match(ICMP(ulp), (ipfw_insn_u32 *)cmd) );
				break;

#ifdef INET6
			case O_ICMP6TYPE:
				match = is_ipv6 && offset == 0 &&
				    proto==IPPROTO_ICMPV6 &&
				    icmp6type_match(
					ICMP6(ulp)->icmp6_type,
					(ipfw_insn_u32 *)cmd);
				break;
#endif /* INET6 */

			case O_IPOPT:
				match = (is_ipv4 &&
				    ipopts_match(mtod(m, struct ip *), cmd) );
				break;

			case O_IPVER:
				match = (is_ipv4 &&
				    cmd->arg1 == mtod(m, struct ip *)->ip_v);
				break;

			case O_IPID:
			case O_IPLEN:
			case O_IPTTL:
				if (is_ipv4) {	/* only for IP packets */
				    uint16_t x;
				    uint16_t *p;
				    int i;

				    if (cmd->opcode == O_IPLEN)
					x = ip_len;
				    else if (cmd->opcode == O_IPTTL)
					x = mtod(m, struct ip *)->ip_ttl;
				    else /* must be IPID */
					x = ntohs(mtod(m, struct ip *)->ip_id);
				    if (cmdlen == 1) {
					match = (cmd->arg1 == x);
					break;
				    }
				    /* otherwise we have ranges */
				    p = ((ipfw_insn_u16 *)cmd)->ports;
				    i = cmdlen - 1;
				    for (; !match && i>0; i--, p += 2)
					match = (x >= p[0] && x <= p[1]);
				}
				break;

			case O_IPPRECEDENCE:
				match = (is_ipv4 &&
				    (cmd->arg1 == (mtod(m, struct ip *)->ip_tos & 0xe0)) );
				break;

			case O_IPTOS:
				match = (is_ipv4 &&
				    flags_match(cmd, mtod(m, struct ip *)->ip_tos));
				break;

			case O_TCPDATALEN:
				if (proto == IPPROTO_TCP && offset == 0) {
				    struct tcphdr *tcp;
				    uint16_t x;
				    uint16_t *p;
				    int i;

				    tcp = TCP(ulp);
				    x = ip_len -
					((ip->ip_hl + tcp->th_off) << 2);
				    if (cmdlen == 1) {
					match = (cmd->arg1 == x);
					break;
				    }
				    /* otherwise we have ranges */
				    p = ((ipfw_insn_u16 *)cmd)->ports;
				    i = cmdlen - 1;
				    for (; !match && i>0; i--, p += 2)
					match = (x >= p[0] && x <= p[1]);
				}
				break;

			case O_TCPFLAGS:
				match = (proto == IPPROTO_TCP && offset == 0 &&
				    flags_match(cmd, TCP(ulp)->th_flags));
				break;

			case O_TCPOPTS:
				match = (proto == IPPROTO_TCP && offset == 0 &&
				    tcpopts_match(TCP(ulp), cmd));
				break;

			case O_TCPSEQ:
				match = (proto == IPPROTO_TCP && offset == 0 &&
				    ((ipfw_insn_u32 *)cmd)->d[0] ==
					TCP(ulp)->th_seq);
				break;

			case O_TCPACK:
				match = (proto == IPPROTO_TCP && offset == 0 &&
				    ((ipfw_insn_u32 *)cmd)->d[0] ==
					TCP(ulp)->th_ack);
				break;

			case O_TCPWIN:
				match = (proto == IPPROTO_TCP && offset == 0 &&
				    cmd->arg1 == TCP(ulp)->th_win);
				break;

			case O_ESTAB:
				/* reject packets which have SYN only */
				/* XXX should i also check for TH_ACK ? */
				match = (proto == IPPROTO_TCP && offset == 0 &&
				    (TCP(ulp)->th_flags &
				     (TH_RST | TH_ACK | TH_SYN)) != TH_SYN);
				break;

			case O_ALTQ: {
				struct altq_tag *at;
				ipfw_insn_altq *altq = (ipfw_insn_altq *)cmd;

				match = 1;
				mtag = m_tag_find(m, PACKET_TAG_PF_QID, NULL);
				if (mtag != NULL)
					break;
				mtag = m_tag_get(PACKET_TAG_PF_QID,
						sizeof(struct altq_tag),
						M_NOWAIT);
				if (mtag == NULL) {
					/*
					 * Let the packet fall back to the
					 * default ALTQ.
					 */
					break;
				}
				at = (struct altq_tag *)(mtag+1);
				at->qid = altq->qid;
				if (is_ipv4)
					at->af = AF_INET;
				else
					at->af = AF_LINK;
				at->hdr = ip;
				m_tag_prepend(m, mtag);
				break;
			}

			case O_LOG:
				if (fw_verbose)
					ipfw_log(f, hlen, args, m, oif, offset);
				match = 1;
				break;

			case O_PROB:
				match = (random()<((ipfw_insn_u32 *)cmd)->d[0]);
				break;

			case O_VERREVPATH:
				/* Outgoing packets automatically pass/match */
				match = ((oif != NULL) ||
				    (m->m_pkthdr.rcvif == NULL) ||
				    (
#ifdef INET6
				    is_ipv6 ?
					verify_path6(&(args->f_id.src_ip6),
					    m->m_pkthdr.rcvif) :
#endif
				    verify_path(src_ip, m->m_pkthdr.rcvif)));
				break;

			case O_VERSRCREACH:
				/* Outgoing packets automatically pass/match */
				match = (hlen > 0 && ((oif != NULL) ||
#ifdef INET6
				    is_ipv6 ?
				        verify_path6(&(args->f_id.src_ip6),
				            NULL) :
#endif
				    verify_path(src_ip, NULL)));
				break;

			case O_ANTISPOOF:
				/* Outgoing packets automatically pass/match */
				if (oif == NULL && hlen > 0 &&
				    (  (is_ipv4 && in_localaddr(src_ip))
#ifdef INET6
				    || (is_ipv6 &&
				        in6_localaddr(&(args->f_id.src_ip6)))
#endif
				    ))
					match =
#ifdef INET6
					    is_ipv6 ? verify_path6(
					        &(args->f_id.src_ip6),
					        m->m_pkthdr.rcvif) :
#endif
					    verify_path(src_ip,
					        m->m_pkthdr.rcvif);
				else
					match = 1;
				break;

			case O_IPSEC:
#ifdef FAST_IPSEC
				match = (m_tag_find(m,
				    PACKET_TAG_IPSEC_IN_DONE, NULL) != NULL);
#endif
#ifdef IPSEC
				match = (ipsec_getnhist(m) != 0);
#endif
				/* otherwise no match */
				break;

#ifdef INET6
			case O_IP6_SRC:
				match = is_ipv6 &&
				    IN6_ARE_ADDR_EQUAL(&args->f_id.src_ip6,
				    &((ipfw_insn_ip6 *)cmd)->addr6);
				break;

			case O_IP6_DST:
				match = is_ipv6 &&
				IN6_ARE_ADDR_EQUAL(&args->f_id.dst_ip6,
				    &((ipfw_insn_ip6 *)cmd)->addr6);
				break;
			case O_IP6_SRC_MASK:
				if (is_ipv6) {
					ipfw_insn_ip6 *te = (ipfw_insn_ip6 *)cmd;
					struct in6_addr p = args->f_id.src_ip6;

					APPLY_MASK(&p, &te->mask6);
					match = IN6_ARE_ADDR_EQUAL(&te->addr6, &p);
				}
				break;

			case O_IP6_DST_MASK:
				if (is_ipv6) {
					ipfw_insn_ip6 *te = (ipfw_insn_ip6 *)cmd;
					struct in6_addr p = args->f_id.dst_ip6;

					APPLY_MASK(&p, &te->mask6);
					match = IN6_ARE_ADDR_EQUAL(&te->addr6, &p);
				}
				break;

			case O_IP6_SRC_ME:
				match= is_ipv6 && search_ip6_addr_net(&args->f_id.src_ip6);
				break;

			case O_IP6_DST_ME:
				match= is_ipv6 && search_ip6_addr_net(&args->f_id.dst_ip6);
				break;

			case O_FLOW6ID:
				match = is_ipv6 &&
				    flow6id_match(args->f_id.flow_id6,
				    (ipfw_insn_u32 *) cmd);
				break;

			case O_EXT_HDR:
				match = is_ipv6 &&
				    (ext_hd & ((ipfw_insn *) cmd)->arg1);
				break;

			case O_IP6:
				match = is_ipv6;
				break;
#endif

			case O_IP4:
				match = is_ipv4;
				break;

			/*
			 * The second set of opcodes represents 'actions',
			 * i.e. the terminal part of a rule once the packet
			 * matches all previous patterns.
			 * Typically there is only one action for each rule,
			 * and the opcode is stored at the end of the rule
			 * (but there are exceptions -- see below).
			 *
			 * In general, here we set retval and terminate the
			 * outer loop (would be a 'break 3' in some language,
			 * but we need to do a 'goto done').
			 *
			 * Exceptions:
			 * O_COUNT and O_SKIPTO actions:
			 *   instead of terminating, we jump to the next rule
			 *   ('goto next_rule', equivalent to a 'break 2'),
			 *   or to the SKIPTO target ('goto again' after
			 *   having set f, cmd and l), respectively.
			 *
			 * O_LOG and O_ALTQ action parameters:
			 *   perform some action and set match = 1;
			 *
			 * O_LIMIT and O_KEEP_STATE: these opcodes are
			 *   not real 'actions', and are stored right
			 *   before the 'action' part of the rule.
			 *   These opcodes try to install an entry in the
			 *   state tables; if successful, we continue with
			 *   the next opcode (match=1; break;), otherwise
			 *   the packet *   must be dropped
			 *   ('goto done' after setting retval);
			 *
			 * O_PROBE_STATE and O_CHECK_STATE: these opcodes
			 *   cause a lookup of the state table, and a jump
			 *   to the 'action' part of the parent rule
			 *   ('goto check_body') if an entry is found, or
			 *   (CHECK_STATE only) a jump to the next rule if
			 *   the entry is not found ('goto next_rule').
			 *   The result of the lookup is cached to make
			 *   further instances of these opcodes are
			 *   effectively NOPs.
			 */
			case O_LIMIT:
			case O_KEEP_STATE:
				if (install_state(f,
				    (ipfw_insn_limit *)cmd, args)) {
					retval = IP_FW_DENY;
					goto done; /* error/limit violation */
				}
				match = 1;
				break;

			case O_PROBE_STATE:
			case O_CHECK_STATE:
				/*
				 * dynamic rules are checked at the first
				 * keep-state or check-state occurrence,
				 * with the result being stored in dyn_dir.
				 * The compiler introduces a PROBE_STATE
				 * instruction for us when we have a
				 * KEEP_STATE (because PROBE_STATE needs
				 * to be run first).
				 */
				if (dyn_dir == MATCH_UNKNOWN &&
				    (q = lookup_dyn_rule(&args->f_id,
				     &dyn_dir, proto == IPPROTO_TCP ?
					TCP(ulp) : NULL))
					!= NULL) {
					/*
					 * Found dynamic entry, update stats
					 * and jump to the 'action' part of
					 * the parent rule.
					 */
					q->pcnt++;
					q->bcnt += pktlen;
					f = q->rule;
					cmd = ACTION_PTR(f);
					l = f->cmd_len - f->act_ofs;
					IPFW_DYN_UNLOCK();
					goto check_body;
				}
				/*
				 * Dynamic entry not found. If CHECK_STATE,
				 * skip to next rule, if PROBE_STATE just
				 * ignore and continue with next opcode.
				 */
				if (cmd->opcode == O_CHECK_STATE)
					goto next_rule;
				match = 1;
				break;

			case O_ACCEPT:
				retval = 0;	/* accept */
				goto done;

			case O_PIPE:
			case O_QUEUE:
				args->rule = f; /* report matching rule */
				args->cookie = cmd->arg1;
				retval = IP_FW_DUMMYNET;
				goto done;

			case O_DIVERT:
			case O_TEE: {
				struct divert_tag *dt;

				if (args->eh) /* not on layer 2 */
					break;
				mtag = m_tag_get(PACKET_TAG_DIVERT,
						sizeof(struct divert_tag),
						M_NOWAIT);
				if (mtag == NULL) {
					/* XXX statistic */
					/* drop packet */
					IPFW_RUNLOCK(chain);
					return (IP_FW_DENY);
				}
				dt = (struct divert_tag *)(mtag+1);
				dt->cookie = f->rulenum;
				dt->info = cmd->arg1;
				m_tag_prepend(m, mtag);
				retval = (cmd->opcode == O_DIVERT) ?
				    IP_FW_DIVERT : IP_FW_TEE;
				goto done;
			}

			case O_COUNT:
			case O_SKIPTO:
				f->pcnt++;	/* update stats */
				f->bcnt += pktlen;
				f->timestamp = time_uptime;
				if (cmd->opcode == O_COUNT)
					goto next_rule;
				/* handle skipto */
				if (f->next_rule == NULL)
					lookup_next_rule(f);
				f = f->next_rule;
				goto again;

			case O_REJECT:
				/*
				 * Drop the packet and send a reject notice
				 * if the packet is not ICMP (or is an ICMP
				 * query), and it is not multicast/broadcast.
				 */
				if (hlen > 0 && is_ipv4 &&
				    (proto != IPPROTO_ICMP ||
				     is_icmp_query(ICMP(ulp))) &&
				    !(m->m_flags & (M_BCAST|M_MCAST)) &&
				    !IN_MULTICAST(ntohl(dst_ip.s_addr))) {
					send_reject(args, cmd->arg1,
					    offset,ip_len);
					m = args->m;
				}
				/* FALLTHROUGH */
#ifdef INET6
			case O_UNREACH6:
				if (hlen > 0 && is_ipv6 &&
				    (proto != IPPROTO_ICMPV6 ||
				     (is_icmp6_query(args->f_id.flags) == 1)) &&
				    !(m->m_flags & (M_BCAST|M_MCAST)) &&
				    !IN6_IS_ADDR_MULTICAST(&args->f_id.dst_ip6)) {
					send_reject6(args, cmd->arg1,
					    offset, hlen);
					m = args->m;
				}
				/* FALLTHROUGH */
#endif
			case O_DENY:
				retval = IP_FW_DENY;
				goto done;

			case O_FORWARD_IP:
				if (args->eh)	/* not valid on layer2 pkts */
					break;
				if (!q || dyn_dir == MATCH_FORWARD)
					args->next_hop =
					    &((ipfw_insn_sa *)cmd)->sa;
				retval = IP_FW_PASS;
				goto done;

			case O_NETGRAPH:
			case O_NGTEE:
				args->rule = f;	/* report matching rule */
				args->cookie = cmd->arg1;
				retval = (cmd->opcode == O_NETGRAPH) ?
				    IP_FW_NETGRAPH : IP_FW_NGTEE;
				goto done;

			default:
				panic("-- unknown opcode %d\n", cmd->opcode);
			} /* end of switch() on opcodes */

			if (cmd->len & F_NOT)
				match = !match;

			if (match) {
				if (cmd->len & F_OR)
					skip_or = 1;
			} else {
				if (!(cmd->len & F_OR)) /* not an OR block, */
					break;		/* try next rule    */
			}

		}	/* end of inner for, scan opcodes */

next_rule:;		/* try next rule		*/

	}		/* end of outer for, scan rules */
	printf("ipfw: ouch!, skip past end of rules, denying packet\n");
	IPFW_RUNLOCK(chain);
	return (IP_FW_DENY);

done:
	/* Update statistics */
	f->pcnt++;
	f->bcnt += pktlen;
	f->timestamp = time_uptime;
	IPFW_RUNLOCK(chain);
	return (retval);

pullup_failed:
	if (fw_verbose)
		printf("ipfw: pullup failed\n");
	return (IP_FW_DENY);
}

/*
 * When a rule is added/deleted, clear the next_rule pointers in all rules.
 * These will be reconstructed on the fly as packets are matched.
 */
static void
flush_rule_ptrs(struct ip_fw_chain *chain)
{
	struct ip_fw *rule;

	IPFW_WLOCK_ASSERT(chain);

	for (rule = chain->rules; rule; rule = rule->next)
		rule->next_rule = NULL;
}

/*
 * Add a new rule to the list. Copy the rule into a malloc'ed area, then
 * possibly create a rule number and add the rule to the list.
 * Update the rule_number in the input struct so the caller knows it as well.
 */
static int
add_rule(struct ip_fw_chain *chain, struct ip_fw *input_rule)
{
	struct ip_fw *rule, *f, *prev;
	int l = RULESIZE(input_rule);

	if (chain->rules == NULL && input_rule->rulenum != IPFW_DEFAULT_RULE)
		return (EINVAL);

	rule = malloc(l, M_IPFW, M_NOWAIT | M_ZERO);
	if (rule == NULL)
		return (ENOSPC);

	bcopy(input_rule, rule, l);

	rule->next = NULL;
	rule->next_rule = NULL;

	rule->pcnt = 0;
	rule->bcnt = 0;
	rule->timestamp = 0;

	IPFW_WLOCK(chain);

	if (chain->rules == NULL) {	/* default rule */
		chain->rules = rule;
		goto done;
        }

	/*
	 * If rulenum is 0, find highest numbered rule before the
	 * default rule, and add autoinc_step
	 */
	if (autoinc_step < 1)
		autoinc_step = 1;
	else if (autoinc_step > 1000)
		autoinc_step = 1000;
	if (rule->rulenum == 0) {
		/*
		 * locate the highest numbered rule before default
		 */
		for (f = chain->rules; f; f = f->next) {
			if (f->rulenum == IPFW_DEFAULT_RULE)
				break;
			rule->rulenum = f->rulenum;
		}
		if (rule->rulenum < IPFW_DEFAULT_RULE - autoinc_step)
			rule->rulenum += autoinc_step;
		input_rule->rulenum = rule->rulenum;
	}

	/*
	 * Now insert the new rule in the right place in the sorted list.
	 */
	for (prev = NULL, f = chain->rules; f; prev = f, f = f->next) {
		if (f->rulenum > rule->rulenum) { /* found the location */
			if (prev) {
				rule->next = f;
				prev->next = rule;
			} else { /* head insert */
				rule->next = chain->rules;
				chain->rules = rule;
			}
			break;
		}
	}
	flush_rule_ptrs(chain);
done:
	static_count++;
	static_len += l;
	IPFW_WUNLOCK(chain);
	DEB(printf("ipfw: installed rule %d, static count now %d\n",
		rule->rulenum, static_count);)
	return (0);
}

/**
 * Remove a static rule (including derived * dynamic rules)
 * and place it on the ``reap list'' for later reclamation.
 * The caller is in charge of clearing rule pointers to avoid
 * dangling pointers.
 * @return a pointer to the next entry.
 * Arguments are not checked, so they better be correct.
 */
static struct ip_fw *
remove_rule(struct ip_fw_chain *chain, struct ip_fw *rule, struct ip_fw *prev)
{
	struct ip_fw *n;
	int l = RULESIZE(rule);

	IPFW_WLOCK_ASSERT(chain);

	n = rule->next;
	IPFW_DYN_LOCK();
	remove_dyn_rule(rule, NULL /* force removal */);
	IPFW_DYN_UNLOCK();
	if (prev == NULL)
		chain->rules = n;
	else
		prev->next = n;
	static_count--;
	static_len -= l;

	rule->next = chain->reap;
	chain->reap = rule;

	return n;
}

/**
 * Reclaim storage associated with a list of rules.  This is
 * typically the list created using remove_rule.
 */
static void
reap_rules(struct ip_fw *head)
{
	struct ip_fw *rule;

	while ((rule = head) != NULL) {
		head = head->next;
		if (DUMMYNET_LOADED)
			ip_dn_ruledel_ptr(rule);
		free(rule, M_IPFW);
	}
}

/*
 * Remove all rules from a chain (except rules in set RESVD_SET
 * unless kill_default = 1).  The caller is responsible for
 * reclaiming storage for the rules left in chain->reap.
 */
static void
free_chain(struct ip_fw_chain *chain, int kill_default)
{
	struct ip_fw *prev, *rule;

	IPFW_WLOCK_ASSERT(chain);

	flush_rule_ptrs(chain); /* more efficient to do outside the loop */
	for (prev = NULL, rule = chain->rules; rule ; )
		if (kill_default || rule->set != RESVD_SET)
			rule = remove_rule(chain, rule, prev);
		else {
			prev = rule;
			rule = rule->next;
		}
}

/**
 * Remove all rules with given number, and also do set manipulation.
 * Assumes chain != NULL && *chain != NULL.
 *
 * The argument is an u_int32_t. The low 16 bit are the rule or set number,
 * the next 8 bits are the new set, the top 8 bits are the command:
 *
 *	0	delete rules with given number
 *	1	delete rules with given set number
 *	2	move rules with given number to new set
 *	3	move rules with given set number to new set
 *	4	swap sets with given numbers
 */
static int
del_entry(struct ip_fw_chain *chain, u_int32_t arg)
{
	struct ip_fw *prev = NULL, *rule;
	u_int16_t rulenum;	/* rule or old_set */
	u_int8_t cmd, new_set;

	rulenum = arg & 0xffff;
	cmd = (arg >> 24) & 0xff;
	new_set = (arg >> 16) & 0xff;

	if (cmd > 4)
		return EINVAL;
	if (new_set > RESVD_SET)
		return EINVAL;
	if (cmd == 0 || cmd == 2) {
		if (rulenum >= IPFW_DEFAULT_RULE)
			return EINVAL;
	} else {
		if (rulenum > RESVD_SET)	/* old_set */
			return EINVAL;
	}

	IPFW_WLOCK(chain);
	rule = chain->rules;
	chain->reap = NULL;
	switch (cmd) {
	case 0:	/* delete rules with given number */
		/*
		 * locate first rule to delete
		 */
		for (; rule->rulenum < rulenum; prev = rule, rule = rule->next)
			;
		if (rule->rulenum != rulenum) {
			IPFW_WUNLOCK(chain);
			return EINVAL;
		}

		/*
		 * flush pointers outside the loop, then delete all matching
		 * rules. prev remains the same throughout the cycle.
		 */
		flush_rule_ptrs(chain);
		while (rule->rulenum == rulenum)
			rule = remove_rule(chain, rule, prev);
		break;

	case 1:	/* delete all rules with given set number */
		flush_rule_ptrs(chain);
		rule = chain->rules;
		while (rule->rulenum < IPFW_DEFAULT_RULE)
			if (rule->set == rulenum)
				rule = remove_rule(chain, rule, prev);
			else {
				prev = rule;
				rule = rule->next;
			}
		break;

	case 2:	/* move rules with given number to new set */
		rule = chain->rules;
		for (; rule->rulenum < IPFW_DEFAULT_RULE; rule = rule->next)
			if (rule->rulenum == rulenum)
				rule->set = new_set;
		break;

	case 3: /* move rules with given set number to new set */
		for (; rule->rulenum < IPFW_DEFAULT_RULE; rule = rule->next)
			if (rule->set == rulenum)
				rule->set = new_set;
		break;

	case 4: /* swap two sets */
		for (; rule->rulenum < IPFW_DEFAULT_RULE; rule = rule->next)
			if (rule->set == rulenum)
				rule->set = new_set;
			else if (rule->set == new_set)
				rule->set = rulenum;
		break;
	}
	/*
	 * Look for rules to reclaim.  We grab the list before
	 * releasing the lock then reclaim them w/o the lock to
	 * avoid a LOR with dummynet.
	 */
	rule = chain->reap;
	chain->reap = NULL;
	IPFW_WUNLOCK(chain);
	if (rule)
		reap_rules(rule);
	return 0;
}

/*
 * Clear counters for a specific rule.
 * The enclosing "table" is assumed locked.
 */
static void
clear_counters(struct ip_fw *rule, int log_only)
{
	ipfw_insn_log *l = (ipfw_insn_log *)ACTION_PTR(rule);

	if (log_only == 0) {
		rule->bcnt = rule->pcnt = 0;
		rule->timestamp = 0;
	}
	if (l->o.opcode == O_LOG)
		l->log_left = l->max_log;
}

/**
 * Reset some or all counters on firewall rules.
 * @arg frwl is null to clear all entries, or contains a specific
 * rule number.
 * @arg log_only is 1 if we only want to reset logs, zero otherwise.
 */
static int
zero_entry(struct ip_fw_chain *chain, int rulenum, int log_only)
{
	struct ip_fw *rule;
	char *msg;

	IPFW_WLOCK(chain);
	if (rulenum == 0) {
		norule_counter = 0;
		for (rule = chain->rules; rule; rule = rule->next)
			clear_counters(rule, log_only);
		msg = log_only ? "ipfw: All logging counts reset.\n" :
				"ipfw: Accounting cleared.\n";
	} else {
		int cleared = 0;
		/*
		 * We can have multiple rules with the same number, so we
		 * need to clear them all.
		 */
		for (rule = chain->rules; rule; rule = rule->next)
			if (rule->rulenum == rulenum) {
				while (rule && rule->rulenum == rulenum) {
					clear_counters(rule, log_only);
					rule = rule->next;
				}
				cleared = 1;
				break;
			}
		if (!cleared) {	/* we did not find any matching rules */
			IPFW_WUNLOCK(chain);
			return (EINVAL);
		}
		msg = log_only ? "ipfw: Entry %d logging count reset.\n" :
				"ipfw: Entry %d cleared.\n";
	}
	IPFW_WUNLOCK(chain);

	if (fw_verbose)
		log(LOG_SECURITY | LOG_NOTICE, msg, rulenum);
	return (0);
}

/*
 * Check validity of the structure before insert.
 * Fortunately rules are simple, so this mostly need to check rule sizes.
 */
static int
check_ipfw_struct(struct ip_fw *rule, int size)
{
	int l, cmdlen = 0;
	int have_action=0;
	ipfw_insn *cmd;

	if (size < sizeof(*rule)) {
		printf("ipfw: rule too short\n");
		return (EINVAL);
	}
	/* first, check for valid size */
	l = RULESIZE(rule);
	if (l != size) {
		printf("ipfw: size mismatch (have %d want %d)\n", size, l);
		return (EINVAL);
	}
	if (rule->act_ofs >= rule->cmd_len) {
		printf("ipfw: bogus action offset (%u > %u)\n",
		    rule->act_ofs, rule->cmd_len - 1);
		return (EINVAL);
	}
	/*
	 * Now go for the individual checks. Very simple ones, basically only
	 * instruction sizes.
	 */
	for (l = rule->cmd_len, cmd = rule->cmd ;
			l > 0 ; l -= cmdlen, cmd += cmdlen) {
		cmdlen = F_LEN(cmd);
		if (cmdlen > l) {
			printf("ipfw: opcode %d size truncated\n",
			    cmd->opcode);
			return EINVAL;
		}
		DEB(printf("ipfw: opcode %d\n", cmd->opcode);)
		switch (cmd->opcode) {
		case O_PROBE_STATE:
		case O_KEEP_STATE:
		case O_PROTO:
		case O_IP_SRC_ME:
		case O_IP_DST_ME:
		case O_LAYER2:
		case O_IN:
		case O_FRAG:
		case O_DIVERTED:
		case O_IPOPT:
		case O_IPTOS:
		case O_IPPRECEDENCE:
		case O_IPVER:
		case O_TCPWIN:
		case O_TCPFLAGS:
		case O_TCPOPTS:
		case O_ESTAB:
		case O_VERREVPATH:
		case O_VERSRCREACH:
		case O_ANTISPOOF:
		case O_IPSEC:
#ifdef INET6
		case O_IP6_SRC_ME:
		case O_IP6_DST_ME:
		case O_EXT_HDR:
		case O_IP6:
#endif
		case O_IP4:
			if (cmdlen != F_INSN_SIZE(ipfw_insn))
				goto bad_size;
			break;

		case O_UID:
		case O_GID:
		case O_JAIL:
		case O_IP_SRC:
		case O_IP_DST:
		case O_TCPSEQ:
		case O_TCPACK:
		case O_PROB:
		case O_ICMPTYPE:
			if (cmdlen != F_INSN_SIZE(ipfw_insn_u32))
				goto bad_size;
			break;

		case O_LIMIT:
			if (cmdlen != F_INSN_SIZE(ipfw_insn_limit))
				goto bad_size;
			break;

		case O_LOG:
			if (cmdlen != F_INSN_SIZE(ipfw_insn_log))
				goto bad_size;

			((ipfw_insn_log *)cmd)->log_left =
			    ((ipfw_insn_log *)cmd)->max_log;

			break;

		case O_IP_SRC_MASK:
		case O_IP_DST_MASK:
			/* only odd command lengths */
			if ( !(cmdlen & 1) || cmdlen > 31)
				goto bad_size;
			break;

		case O_IP_SRC_SET:
		case O_IP_DST_SET:
			if (cmd->arg1 == 0 || cmd->arg1 > 256) {
				printf("ipfw: invalid set size %d\n",
					cmd->arg1);
				return EINVAL;
			}
			if (cmdlen != F_INSN_SIZE(ipfw_insn_u32) +
			    (cmd->arg1+31)/32 )
				goto bad_size;
			break;

		case O_IP_SRC_LOOKUP:
		case O_IP_DST_LOOKUP:
			if (cmd->arg1 >= IPFW_TABLES_MAX) {
				printf("ipfw: invalid table number %d\n",
				    cmd->arg1);
				return (EINVAL);
			}
			if (cmdlen != F_INSN_SIZE(ipfw_insn) &&
			    cmdlen != F_INSN_SIZE(ipfw_insn_u32))
				goto bad_size;
			break;

		case O_MACADDR2:
			if (cmdlen != F_INSN_SIZE(ipfw_insn_mac))
				goto bad_size;
			break;

		case O_NOP:
		case O_IPID:
		case O_IPTTL:
		case O_IPLEN:
		case O_TCPDATALEN:
			if (cmdlen < 1 || cmdlen > 31)
				goto bad_size;
			break;

		case O_MAC_TYPE:
		case O_IP_SRCPORT:
		case O_IP_DSTPORT: /* XXX artificial limit, 30 port pairs */
			if (cmdlen < 2 || cmdlen > 31)
				goto bad_size;
			break;

		case O_RECV:
		case O_XMIT:
		case O_VIA:
			if (cmdlen != F_INSN_SIZE(ipfw_insn_if))
				goto bad_size;
			break;

		case O_ALTQ:
			if (cmdlen != F_INSN_SIZE(ipfw_insn_altq))
				goto bad_size;
			break;

		case O_PIPE:
		case O_QUEUE:
			if (cmdlen != F_INSN_SIZE(ipfw_insn))
				goto bad_size;
			goto check_action;

		case O_FORWARD_IP:
#ifdef	IPFIREWALL_FORWARD
			if (cmdlen != F_INSN_SIZE(ipfw_insn_sa))
				goto bad_size;
			goto check_action;
#else
			return EINVAL;
#endif

		case O_DIVERT:
		case O_TEE:
			if (ip_divert_ptr == NULL)
				return EINVAL;
			else
				goto check_size;
		case O_NETGRAPH:
		case O_NGTEE:
			if (!NG_IPFW_LOADED)
				return EINVAL;
			else
				goto check_size;
		case O_FORWARD_MAC: /* XXX not implemented yet */
		case O_CHECK_STATE:
		case O_COUNT:
		case O_ACCEPT:
		case O_DENY:
		case O_REJECT:
#ifdef INET6
		case O_UNREACH6:
#endif
		case O_SKIPTO:
check_size:
			if (cmdlen != F_INSN_SIZE(ipfw_insn))
				goto bad_size;
check_action:
			if (have_action) {
				printf("ipfw: opcode %d, multiple actions"
					" not allowed\n",
					cmd->opcode);
				return EINVAL;
			}
			have_action = 1;
			if (l != cmdlen) {
				printf("ipfw: opcode %d, action must be"
					" last opcode\n",
					cmd->opcode);
				return EINVAL;
			}
			break;
#ifdef INET6
		case O_IP6_SRC:
		case O_IP6_DST:
			if (cmdlen != F_INSN_SIZE(struct in6_addr) +
			    F_INSN_SIZE(ipfw_insn))
				goto bad_size;
			break;

		case O_FLOW6ID:
			if (cmdlen != F_INSN_SIZE(ipfw_insn_u32) +
			    ((ipfw_insn_u32 *)cmd)->o.arg1)
				goto bad_size;
			break;

		case O_IP6_SRC_MASK:
		case O_IP6_DST_MASK:
			if ( !(cmdlen & 1) || cmdlen > 127)
				goto bad_size;
			break;
		case O_ICMP6TYPE:
			if( cmdlen != F_INSN_SIZE( ipfw_insn_icmp6 ) )
				goto bad_size;
			break;
#endif

		default:
			switch (cmd->opcode) {
#ifndef INET6
			case O_IP6_SRC_ME:
			case O_IP6_DST_ME:
			case O_EXT_HDR:
			case O_IP6:
			case O_UNREACH6:
			case O_IP6_SRC:
			case O_IP6_DST:
			case O_FLOW6ID:
			case O_IP6_SRC_MASK:
			case O_IP6_DST_MASK:
			case O_ICMP6TYPE:
				printf("ipfw: no IPv6 support in kernel\n");
				return EPROTONOSUPPORT;
#endif
			default:
				printf("ipfw: opcode %d, unknown opcode\n",
					cmd->opcode);
				return EINVAL;
			}
		}
	}
	if (have_action == 0) {
		printf("ipfw: missing action\n");
		return EINVAL;
	}
	return 0;

bad_size:
	printf("ipfw: opcode %d size %d wrong\n",
		cmd->opcode, cmdlen);
	return EINVAL;
}

/*
 * Copy the static and dynamic rules to the supplied buffer
 * and return the amount of space actually used.
 */
static size_t
ipfw_getrules(struct ip_fw_chain *chain, void *buf, size_t space)
{
	char *bp = buf;
	char *ep = bp + space;
	struct ip_fw *rule;
	int i;

	/* XXX this can take a long time and locking will block packet flow */
	IPFW_RLOCK(chain);
	for (rule = chain->rules; rule ; rule = rule->next) {
		/*
		 * Verify the entry fits in the buffer in case the
		 * rules changed between calculating buffer space and
		 * now.  This would be better done using a generation
		 * number but should suffice for now.
		 */
		i = RULESIZE(rule);
		if (bp + i <= ep) {
			bcopy(rule, bp, i);
			bcopy(&set_disable, &(((struct ip_fw *)bp)->next_rule),
			    sizeof(set_disable));
			bp += i;
		}
	}
	IPFW_RUNLOCK(chain);
	if (ipfw_dyn_v) {
		ipfw_dyn_rule *p, *last = NULL;

		IPFW_DYN_LOCK();
		for (i = 0 ; i < curr_dyn_buckets; i++)
			for (p = ipfw_dyn_v[i] ; p != NULL; p = p->next) {
				if (bp + sizeof *p <= ep) {
					ipfw_dyn_rule *dst =
						(ipfw_dyn_rule *)bp;
					bcopy(p, dst, sizeof *p);
					bcopy(&(p->rule->rulenum), &(dst->rule),
					    sizeof(p->rule->rulenum));
					/*
					 * store a non-null value in "next".
					 * The userland code will interpret a
					 * NULL here as a marker
					 * for the last dynamic rule.
					 */
					bcopy(&dst, &dst->next, sizeof(dst));
					last = dst;
					dst->expire =
					    TIME_LEQ(dst->expire, time_uptime) ?
						0 : dst->expire - time_uptime ;
					bp += sizeof(ipfw_dyn_rule);
				}
			}
		IPFW_DYN_UNLOCK();
		if (last != NULL) /* mark last dynamic rule */
			bzero(&last->next, sizeof(last));
	}
	return (bp - (char *)buf);
}


/**
 * {set|get}sockopt parser.
 */
static int
ipfw_ctl(struct sockopt *sopt)
{
#define	RULE_MAXSIZE	(256*sizeof(u_int32_t))
	int error, rule_num;
	size_t size;
	struct ip_fw *buf, *rule;
	u_int32_t rulenum[2];

	error = suser(sopt->sopt_td);
	if (error)
		return (error);

	/*
	 * Disallow modifications in really-really secure mode, but still allow
	 * the logging counters to be reset.
	 */
	if (sopt->sopt_name == IP_FW_ADD ||
	    (sopt->sopt_dir == SOPT_SET && sopt->sopt_name != IP_FW_RESETLOG)) {
#if __FreeBSD_version >= 500034
		error = securelevel_ge(sopt->sopt_td->td_ucred, 3);
		if (error)
			return (error);
#else /* FreeBSD 4.x */
		if (securelevel >= 3)
			return (EPERM);
#endif
	}

	error = 0;

	switch (sopt->sopt_name) {
	case IP_FW_GET:
		/*
		 * pass up a copy of the current rules. Static rules
		 * come first (the last of which has number IPFW_DEFAULT_RULE),
		 * followed by a possibly empty list of dynamic rule.
		 * The last dynamic rule has NULL in the "next" field.
		 *
		 * Note that the calculated size is used to bound the
		 * amount of data returned to the user.  The rule set may
		 * change between calculating the size and returning the
		 * data in which case we'll just return what fits.
		 */
		size = static_len;	/* size of static rules */
		if (ipfw_dyn_v)		/* add size of dyn.rules */
			size += (dyn_count * sizeof(ipfw_dyn_rule));

		/*
		 * XXX todo: if the user passes a short length just to know
		 * how much room is needed, do not bother filling up the
		 * buffer, just jump to the sooptcopyout.
		 */
		buf = malloc(size, M_TEMP, M_WAITOK);
		error = sooptcopyout(sopt, buf,
				ipfw_getrules(&layer3_chain, buf, size));
		free(buf, M_TEMP);
		break;

	case IP_FW_FLUSH:
		/*
		 * Normally we cannot release the lock on each iteration.
		 * We could do it here only because we start from the head all
		 * the times so there is no risk of missing some entries.
		 * On the other hand, the risk is that we end up with
		 * a very inconsistent ruleset, so better keep the lock
		 * around the whole cycle.
		 *
		 * XXX this code can be improved by resetting the head of
		 * the list to point to the default rule, and then freeing
		 * the old list without the need for a lock.
		 */

		IPFW_WLOCK(&layer3_chain);
		layer3_chain.reap = NULL;
		free_chain(&layer3_chain, 0 /* keep default rule */);
		rule = layer3_chain.reap, layer3_chain.reap = NULL;
		IPFW_WUNLOCK(&layer3_chain);
		if (layer3_chain.reap != NULL)
			reap_rules(rule);
		break;

	case IP_FW_ADD:
		rule = malloc(RULE_MAXSIZE, M_TEMP, M_WAITOK);
		error = sooptcopyin(sopt, rule, RULE_MAXSIZE,
			sizeof(struct ip_fw) );
		if (error == 0)
			error = check_ipfw_struct(rule, sopt->sopt_valsize);
		if (error == 0) {
			error = add_rule(&layer3_chain, rule);
			size = RULESIZE(rule);
			if (!error && sopt->sopt_dir == SOPT_GET)
				error = sooptcopyout(sopt, rule, size);
		}
		free(rule, M_TEMP);
		break;

	case IP_FW_DEL:
		/*
		 * IP_FW_DEL is used for deleting single rules or sets,
		 * and (ab)used to atomically manipulate sets. Argument size
		 * is used to distinguish between the two:
		 *    sizeof(u_int32_t)
		 *	delete single rule or set of rules,
		 *	or reassign rules (or sets) to a different set.
		 *    2*sizeof(u_int32_t)
		 *	atomic disable/enable sets.
		 *	first u_int32_t contains sets to be disabled,
		 *	second u_int32_t contains sets to be enabled.
		 */
		error = sooptcopyin(sopt, rulenum,
			2*sizeof(u_int32_t), sizeof(u_int32_t));
		if (error)
			break;
		size = sopt->sopt_valsize;
		if (size == sizeof(u_int32_t))	/* delete or reassign */
			error = del_entry(&layer3_chain, rulenum[0]);
		else if (size == 2*sizeof(u_int32_t)) /* set enable/disable */
			set_disable =
			    (set_disable | rulenum[0]) & ~rulenum[1] &
			    ~(1<<RESVD_SET); /* set RESVD_SET always enabled */
		else
			error = EINVAL;
		break;

	case IP_FW_ZERO:
	case IP_FW_RESETLOG: /* argument is an int, the rule number */
		rule_num = 0;
		if (sopt->sopt_val != 0) {
		    error = sooptcopyin(sopt, &rule_num,
			    sizeof(int), sizeof(int));
		    if (error)
			break;
		}
		error = zero_entry(&layer3_chain, rule_num,
			sopt->sopt_name == IP_FW_RESETLOG);
		break;

	case IP_FW_TABLE_ADD:
		{
			ipfw_table_entry ent;

			error = sooptcopyin(sopt, &ent,
			    sizeof(ent), sizeof(ent));
			if (error)
				break;
			error = add_table_entry(&layer3_chain, ent.tbl,
			    ent.addr, ent.masklen, ent.value);
		}
		break;

	case IP_FW_TABLE_DEL:
		{
			ipfw_table_entry ent;

			error = sooptcopyin(sopt, &ent,
			    sizeof(ent), sizeof(ent));
			if (error)
				break;
			error = del_table_entry(&layer3_chain, ent.tbl,
			    ent.addr, ent.masklen);
		}
		break;

	case IP_FW_TABLE_FLUSH:
		{
			u_int16_t tbl;

			error = sooptcopyin(sopt, &tbl,
			    sizeof(tbl), sizeof(tbl));
			if (error)
				break;
			IPFW_WLOCK(&layer3_chain);
			error = flush_table(&layer3_chain, tbl);
			IPFW_WUNLOCK(&layer3_chain);
		}
		break;

	case IP_FW_TABLE_GETSIZE:
		{
			u_int32_t tbl, cnt;

			if ((error = sooptcopyin(sopt, &tbl, sizeof(tbl),
			    sizeof(tbl))))
				break;
			IPFW_RLOCK(&layer3_chain);
			if ((error = count_table(&layer3_chain, tbl, &cnt)))
				break;
			IPFW_RUNLOCK(&layer3_chain);
			error = sooptcopyout(sopt, &cnt, sizeof(cnt));
		}
		break;

	case IP_FW_TABLE_LIST:
		{
			ipfw_table *tbl;

			if (sopt->sopt_valsize < sizeof(*tbl)) {
				error = EINVAL;
				break;
			}
			size = sopt->sopt_valsize;
			tbl = malloc(size, M_TEMP, M_WAITOK);
			if (tbl == NULL) {
				error = ENOMEM;
				break;
			}
			error = sooptcopyin(sopt, tbl, size, sizeof(*tbl));
			if (error) {
				free(tbl, M_TEMP);
				break;
			}
			tbl->size = (size - sizeof(*tbl)) /
			    sizeof(ipfw_table_entry);
			IPFW_WLOCK(&layer3_chain);
			error = dump_table(&layer3_chain, tbl);
			if (error) {
				IPFW_WUNLOCK(&layer3_chain);
				free(tbl, M_TEMP);
				break;
			}
			IPFW_WUNLOCK(&layer3_chain);
			error = sooptcopyout(sopt, tbl, size);
			free(tbl, M_TEMP);
		}
		break;

	default:
		printf("ipfw: ipfw_ctl invalid option %d\n", sopt->sopt_name);
		error = EINVAL;
	}

	return (error);
#undef RULE_MAXSIZE
}

/**
 * dummynet needs a reference to the default rule, because rules can be
 * deleted while packets hold a reference to them. When this happens,
 * dummynet changes the reference to the default rule (it could well be a
 * NULL pointer, but this way we do not need to check for the special
 * case, plus here he have info on the default behaviour).
 */
struct ip_fw *ip_fw_default_rule;

/*
 * This procedure is only used to handle keepalives. It is invoked
 * every dyn_keepalive_period
 */
static void
ipfw_tick(void * __unused unused)
{
	struct mbuf *m0, *m, *mnext, **mtailp;
	int i;
	ipfw_dyn_rule *q;

	if (dyn_keepalive == 0 || ipfw_dyn_v == NULL || dyn_count == 0)
		goto done;

	/*
	 * We make a chain of packets to go out here -- not deferring
	 * until after we drop the IPFW dynamic rule lock would result
	 * in a lock order reversal with the normal packet input -> ipfw
	 * call stack.
	 */
	m0 = NULL;
	mtailp = &m0;
	IPFW_DYN_LOCK();
	for (i = 0 ; i < curr_dyn_buckets ; i++) {
		for (q = ipfw_dyn_v[i] ; q ; q = q->next ) {
			if (q->dyn_type == O_LIMIT_PARENT)
				continue;
			if (q->id.proto != IPPROTO_TCP)
				continue;
			if ( (q->state & BOTH_SYN) != BOTH_SYN)
				continue;
			if (TIME_LEQ( time_uptime+dyn_keepalive_interval,
			    q->expire))
				continue;	/* too early */
			if (TIME_LEQ(q->expire, time_uptime))
				continue;	/* too late, rule expired */

			*mtailp = send_pkt(&(q->id), q->ack_rev - 1,
				q->ack_fwd, TH_SYN);
			if (*mtailp != NULL)
				mtailp = &(*mtailp)->m_nextpkt;
			*mtailp = send_pkt(&(q->id), q->ack_fwd - 1,
				q->ack_rev, 0);
			if (*mtailp != NULL)
				mtailp = &(*mtailp)->m_nextpkt;
		}
	}
	IPFW_DYN_UNLOCK();
	for (m = mnext = m0; m != NULL; m = mnext) {
		mnext = m->m_nextpkt;
		m->m_nextpkt = NULL;
		ip_output(m, NULL, NULL, 0, NULL, NULL);
	}
done:
	callout_reset(&ipfw_timeout, dyn_keepalive_period*hz, ipfw_tick, NULL);
}

int
ipfw_init(void)
{
	struct ip_fw default_rule;
	int error;

#ifdef INET6
	/* Setup IPv6 fw sysctl tree. */
	sysctl_ctx_init(&ip6_fw_sysctl_ctx);
	ip6_fw_sysctl_tree = SYSCTL_ADD_NODE(&ip6_fw_sysctl_ctx,
		SYSCTL_STATIC_CHILDREN(_net_inet6_ip6), OID_AUTO, "fw",
		CTLFLAG_RW | CTLFLAG_SECURE, 0, "Firewall");
	SYSCTL_ADD_INT(&ip6_fw_sysctl_ctx, SYSCTL_CHILDREN(ip6_fw_sysctl_tree),
		OID_AUTO, "deny_unknown_exthdrs", CTLFLAG_RW | CTLFLAG_SECURE,
		&fw_deny_unknown_exthdrs, 0,
		"Deny packets with unknown IPv6 Extension Headers");
#endif

	layer3_chain.rules = NULL;
	layer3_chain.want_write = 0;
	layer3_chain.busy_count = 0;
	cv_init(&layer3_chain.cv, "Condition variable for IPFW rw locks");
	IPFW_LOCK_INIT(&layer3_chain);
	ipfw_dyn_rule_zone = uma_zcreate("IPFW dynamic rule zone",
	    sizeof(ipfw_dyn_rule), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	IPFW_DYN_LOCK_INIT();
	callout_init(&ipfw_timeout, NET_CALLOUT_MPSAFE);

	bzero(&default_rule, sizeof default_rule);

	default_rule.act_ofs = 0;
	default_rule.rulenum = IPFW_DEFAULT_RULE;
	default_rule.cmd_len = 1;
	default_rule.set = RESVD_SET;

	default_rule.cmd[0].len = 1;
	default_rule.cmd[0].opcode =
#ifdef IPFIREWALL_DEFAULT_TO_ACCEPT
				1 ? O_ACCEPT :
#endif
				O_DENY;

	error = add_rule(&layer3_chain, &default_rule);
	if (error != 0) {
		printf("ipfw2: error %u initializing default rule "
			"(support disabled)\n", error);
		IPFW_DYN_LOCK_DESTROY();
		IPFW_LOCK_DESTROY(&layer3_chain);
		return (error);
	}

	ip_fw_default_rule = layer3_chain.rules;
	printf("ipfw2 (+ipv6) initialized, divert %s, "
		"rule-based forwarding "
#ifdef IPFIREWALL_FORWARD
		"enabled, "
#else
		"disabled, "
#endif
		"default to %s, logging ",
#ifdef IPDIVERT
		"enabled",
#else
		"loadable",
#endif
		default_rule.cmd[0].opcode == O_ACCEPT ? "accept" : "deny");

#ifdef IPFIREWALL_VERBOSE
	fw_verbose = 1;
#endif
#ifdef IPFIREWALL_VERBOSE_LIMIT
	verbose_limit = IPFIREWALL_VERBOSE_LIMIT;
#endif
	if (fw_verbose == 0)
		printf("disabled\n");
	else if (verbose_limit == 0)
		printf("unlimited\n");
	else
		printf("limited to %d packets/entry by default\n",
		    verbose_limit);

	init_tables(&layer3_chain);
	ip_fw_ctl_ptr = ipfw_ctl;
	ip_fw_chk_ptr = ipfw_chk;
	callout_reset(&ipfw_timeout, hz, ipfw_tick, NULL);

	return (0);
}

void
ipfw_destroy(void)
{
	struct ip_fw *reap;

	ip_fw_chk_ptr = NULL;
	ip_fw_ctl_ptr = NULL;
	callout_drain(&ipfw_timeout);
	IPFW_WLOCK(&layer3_chain);
	flush_tables(&layer3_chain);
	layer3_chain.reap = NULL;
	free_chain(&layer3_chain, 1 /* kill default rule */);
	reap = layer3_chain.reap, layer3_chain.reap = NULL;
	IPFW_WUNLOCK(&layer3_chain);
	if (reap != NULL)
		reap_rules(reap);
	IPFW_DYN_LOCK_DESTROY();
	uma_zdestroy(ipfw_dyn_rule_zone);
	IPFW_LOCK_DESTROY(&layer3_chain);

#ifdef INET6
	/* Free IPv6 fw sysctl tree. */
	sysctl_ctx_free(&ip6_fw_sysctl_ctx);
#endif

	printf("IP firewall unloaded\n");
}
