/*-
 * Copyright (c) 2002-2009 Luigi Rizzo, Universita` di Pisa
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * The FreeBSD IP packet firewall, main file
 */

#include "opt_ipfw.h"
#include "opt_ipdivert.h"
#include "opt_inet.h"
#ifndef INET
#error "IPFIREWALL requires INET"
#endif /* INET */
#include "opt_inet6.h"
#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/condvar.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/jail.h>
#include <sys/module.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/ucred.h>
#include <net/ethernet.h> /* for ETHERTYPE_IP */
#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/pfil.h>
#include <net/vnet.h>

#include <netpfil/pf/pf_mtag.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_carp.h>
#include <netinet/pim.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/sctp.h>

#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#ifdef INET6
#include <netinet6/in6_pcb.h>
#include <netinet6/scope6_var.h>
#include <netinet6/ip6_var.h>
#endif

#include <netpfil/ipfw/ip_fw_private.h>

#include <machine/in_cksum.h>	/* XXX for in_cksum */

#ifdef MAC
#include <security/mac/mac_framework.h>
#endif

/*
 * static variables followed by global ones.
 * All ipfw global variables are here.
 */

/* ipfw_vnet_ready controls when we are open for business */
static VNET_DEFINE(int, ipfw_vnet_ready) = 0;
#define	V_ipfw_vnet_ready	VNET(ipfw_vnet_ready)

static VNET_DEFINE(int, fw_deny_unknown_exthdrs);
#define	V_fw_deny_unknown_exthdrs	VNET(fw_deny_unknown_exthdrs)

static VNET_DEFINE(int, fw_permit_single_frag6) = 1;
#define	V_fw_permit_single_frag6	VNET(fw_permit_single_frag6)

#ifdef IPFIREWALL_DEFAULT_TO_ACCEPT
static int default_to_accept = 1;
#else
static int default_to_accept;
#endif

VNET_DEFINE(int, autoinc_step);
VNET_DEFINE(int, fw_one_pass) = 1;

VNET_DEFINE(unsigned int, fw_tables_max);
/* Use 128 tables by default */
static unsigned int default_fw_tables = IPFW_TABLES_DEFAULT;

/*
 * Each rule belongs to one of 32 different sets (0..31).
 * The variable set_disable contains one bit per set.
 * If the bit is set, all rules in the corresponding set
 * are disabled. Set RESVD_SET(31) is reserved for the default rule
 * and rules that are not deleted by the flush command,
 * and CANNOT be disabled.
 * Rules in set RESVD_SET can only be deleted individually.
 */
VNET_DEFINE(u_int32_t, set_disable);
#define	V_set_disable			VNET(set_disable)

VNET_DEFINE(int, fw_verbose);
/* counter for ipfw_log(NULL...) */
VNET_DEFINE(u_int64_t, norule_counter);
VNET_DEFINE(int, verbose_limit);

/* layer3_chain contains the list of rules for layer 3 */
VNET_DEFINE(struct ip_fw_chain, layer3_chain);

VNET_DEFINE(int, ipfw_nat_ready) = 0;

ipfw_nat_t *ipfw_nat_ptr = NULL;
struct cfg_nat *(*lookup_nat_ptr)(struct nat_list *, int);
ipfw_nat_cfg_t *ipfw_nat_cfg_ptr;
ipfw_nat_cfg_t *ipfw_nat_del_ptr;
ipfw_nat_cfg_t *ipfw_nat_get_cfg_ptr;
ipfw_nat_cfg_t *ipfw_nat_get_log_ptr;

#ifdef SYSCTL_NODE
uint32_t dummy_def = IPFW_DEFAULT_RULE;
static int sysctl_ipfw_table_num(SYSCTL_HANDLER_ARGS);

SYSBEGIN(f3)

SYSCTL_NODE(_net_inet_ip, OID_AUTO, fw, CTLFLAG_RW, 0, "Firewall");
SYSCTL_VNET_INT(_net_inet_ip_fw, OID_AUTO, one_pass,
    CTLFLAG_RW | CTLFLAG_SECURE3, &VNET_NAME(fw_one_pass), 0,
    "Only do a single pass through ipfw when using dummynet(4)");
SYSCTL_VNET_INT(_net_inet_ip_fw, OID_AUTO, autoinc_step,
    CTLFLAG_RW, &VNET_NAME(autoinc_step), 0,
    "Rule number auto-increment step");
SYSCTL_VNET_INT(_net_inet_ip_fw, OID_AUTO, verbose,
    CTLFLAG_RW | CTLFLAG_SECURE3, &VNET_NAME(fw_verbose), 0,
    "Log matches to ipfw rules");
SYSCTL_VNET_INT(_net_inet_ip_fw, OID_AUTO, verbose_limit,
    CTLFLAG_RW, &VNET_NAME(verbose_limit), 0,
    "Set upper limit of matches of ipfw rules logged");
SYSCTL_UINT(_net_inet_ip_fw, OID_AUTO, default_rule, CTLFLAG_RD,
    &dummy_def, 0,
    "The default/max possible rule number.");
SYSCTL_VNET_PROC(_net_inet_ip_fw, OID_AUTO, tables_max,
    CTLTYPE_UINT|CTLFLAG_RW, 0, 0, sysctl_ipfw_table_num, "IU",
    "Maximum number of tables");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, default_to_accept, CTLFLAG_RDTUN,
    &default_to_accept, 0,
    "Make the default rule accept all packets.");
TUNABLE_INT("net.inet.ip.fw.default_to_accept", &default_to_accept);
TUNABLE_INT("net.inet.ip.fw.tables_max", (int *)&default_fw_tables);
SYSCTL_VNET_INT(_net_inet_ip_fw, OID_AUTO, static_count,
    CTLFLAG_RD, &VNET_NAME(layer3_chain.n_rules), 0,
    "Number of static rules");

#ifdef INET6
SYSCTL_DECL(_net_inet6_ip6);
SYSCTL_NODE(_net_inet6_ip6, OID_AUTO, fw, CTLFLAG_RW, 0, "Firewall");
SYSCTL_VNET_INT(_net_inet6_ip6_fw, OID_AUTO, deny_unknown_exthdrs,
    CTLFLAG_RW | CTLFLAG_SECURE, &VNET_NAME(fw_deny_unknown_exthdrs), 0,
    "Deny packets with unknown IPv6 Extension Headers");
SYSCTL_VNET_INT(_net_inet6_ip6_fw, OID_AUTO, permit_single_frag6,
    CTLFLAG_RW | CTLFLAG_SECURE, &VNET_NAME(fw_permit_single_frag6), 0,
    "Permit single packet IPv6 fragments");
#endif /* INET6 */

SYSEND

#endif /* SYSCTL_NODE */


/*
 * Some macros used in the various matching options.
 * L3HDR maps an ipv4 pointer into a layer3 header pointer of type T
 * Other macros just cast void * into the appropriate type
 */
#define	L3HDR(T, ip)	((T *)((u_int32_t *)(ip) + (ip)->ip_hl))
#define	TCP(p)		((struct tcphdr *)(p))
#define	SCTP(p)		((struct sctphdr *)(p))
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
iface_match(struct ifnet *ifp, ipfw_insn_if *cmd, struct ip_fw_chain *chain, uint32_t *tablearg)
{
	if (ifp == NULL)	/* no iface with this packet, match fails */
		return 0;
	/* Check by name or by IP address */
	if (cmd->name[0] != '\0') { /* match by name */
		if (cmd->name[0] == '\1') /* use tablearg to match */
			return ipfw_lookup_table_extended(chain, cmd->p.glob,
				ifp->if_xname, tablearg, IPFW_TABLE_INTERFACE);
		/* Check name */
		if (cmd->p.glob) {
			if (fnmatch(cmd->name, ifp->if_xname, 0) == 0)
				return(1);
		} else {
			if (strncmp(ifp->if_xname, cmd->name, IFNAMSIZ) == 0)
				return(1);
		}
	} else {
#if !defined(USERSPACE) && defined(__FreeBSD__)	/* and OSX too ? */
		struct ifaddr *ia;

		if_addr_rlock(ifp);
		TAILQ_FOREACH(ia, &ifp->if_addrhead, ifa_link) {
			if (ia->ifa_addr->sa_family != AF_INET)
				continue;
			if (cmd->p.ip.s_addr == ((struct sockaddr_in *)
			    (ia->ifa_addr))->sin_addr.s_addr) {
				if_addr_runlock(ifp);
				return(1);	/* match */
			}
		}
		if_addr_runlock(ifp);
#endif /* __FreeBSD__ */
	}
	return(0);	/* no match, fail ... */
}

/*
 * The verify_path function checks if a route to the src exists and
 * if it is reachable via ifp (when provided).
 * 
 * The 'verrevpath' option checks that the interface that an IP packet
 * arrives on is the same interface that traffic destined for the
 * packet's source address would be routed out of.
 * The 'versrcreach' option just checks that the source address is
 * reachable via any route (except default) in the routing table.
 * These two are a measure to block forged packets. This is also
 * commonly known as "anti-spoofing" or Unicast Reverse Path
 * Forwarding (Unicast RFP) in Cisco-ese. The name of the knobs
 * is purposely reminiscent of the Cisco IOS command,
 *
 *   ip verify unicast reverse-path
 *   ip verify unicast source reachable-via any
 *
 * which implements the same functionality. But note that the syntax
 * is misleading, and the check may be performed on all IP packets
 * whether unicast, multicast, or broadcast.
 */
static int
verify_path(struct in_addr src, struct ifnet *ifp, u_int fib)
{
#if defined(USERSPACE) || !defined(__FreeBSD__)
	return 0;
#else
	struct route ro;
	struct sockaddr_in *dst;

	bzero(&ro, sizeof(ro));

	dst = (struct sockaddr_in *)&(ro.ro_dst);
	dst->sin_family = AF_INET;
	dst->sin_len = sizeof(*dst);
	dst->sin_addr = src;
	in_rtalloc_ign(&ro, 0, fib);

	if (ro.ro_rt == NULL)
		return 0;

	/*
	 * If ifp is provided, check for equality with rtentry.
	 * We should use rt->rt_ifa->ifa_ifp, instead of rt->rt_ifp,
	 * in order to pass packets injected back by if_simloop():
	 * routing entry (via lo0) for our own address
	 * may exist, so we need to handle routing assymetry.
	 */
	if (ifp != NULL && ro.ro_rt->rt_ifa->ifa_ifp != ifp) {
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
#endif /* __FreeBSD__ */
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

	TAILQ_FOREACH(mdc, &V_ifnet, if_link) {
		if_addr_rlock(mdc);
		TAILQ_FOREACH(mdc2, &mdc->if_addrhead, ifa_link) {
			if (mdc2->ifa_addr->sa_family == AF_INET6) {
				fdm = (struct in6_ifaddr *)mdc2;
				copia = fdm->ia_addr.sin6_addr;
				/* need for leaving scope_id in the sock_addr */
				in6_clearscope(&copia);
				if (IN6_ARE_ADDR_EQUAL(ip6_addr, &copia)) {
					if_addr_runlock(mdc);
					return 1;
				}
			}
		}
		if_addr_runlock(mdc);
	}
	return 0;
}

static int
verify_path6(struct in6_addr *src, struct ifnet *ifp, u_int fib)
{
	struct route_in6 ro;
	struct sockaddr_in6 *dst;

	bzero(&ro, sizeof(ro));

	dst = (struct sockaddr_in6 * )&(ro.ro_dst);
	dst->sin6_family = AF_INET6;
	dst->sin6_len = sizeof(*dst);
	dst->sin6_addr = *src;

	in6_rtalloc_ign(&ro, 0, fib);
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
send_reject6(struct ip_fw_args *args, int code, u_int hlen, struct ip6_hdr *ip6)
{
	struct mbuf *m;

	m = args->m;
	if (code == ICMP6_UNREACH_RST && args->f_id.proto == IPPROTO_TCP) {
		struct tcphdr *tcp;
		tcp = (struct tcphdr *)((char *)ip6 + hlen);

		if ((tcp->th_flags & TH_RST) == 0) {
			struct mbuf *m0;
			m0 = ipfw_send_pkt(args->m, &(args->f_id),
			    ntohl(tcp->th_seq), ntohl(tcp->th_ack),
			    tcp->th_flags | TH_RST);
			if (m0 != NULL)
				ip6_output(m0, NULL, NULL, 0, NULL, NULL,
				    NULL);
		}
		FREE_PKT(m);
	} else if (code != ICMP6_UNREACH_RST) { /* Send an ICMPv6 unreach. */
#if 0
		/*
		 * Unlike above, the mbufs need to line up with the ip6 hdr,
		 * as the contents are read. We need to m_adj() the
		 * needed amount.
		 * The mbuf will however be thrown away so we can adjust it.
		 * Remember we did an m_pullup on it already so we
		 * can make some assumptions about contiguousness.
		 */
		if (args->L3offset)
			m_adj(m, args->L3offset);
#endif
		icmp6_error(m, ICMP6_DST_UNREACH, code, 0);
	} else
		FREE_PKT(m);

	args->m = NULL;
}

#endif /* INET6 */


/*
 * sends a reject message, consuming the mbuf passed as an argument.
 */
static void
send_reject(struct ip_fw_args *args, int code, int iplen, struct ip *ip)
{

#if 0
	/* XXX When ip is not guaranteed to be at mtod() we will
	 * need to account for this */
	 * The mbuf will however be thrown away so we can adjust it.
	 * Remember we did an m_pullup on it already so we
	 * can make some assumptions about contiguousness.
	 */
	if (args->L3offset)
		m_adj(m, args->L3offset);
#endif
	if (code != ICMP_REJECT_RST) { /* Send an ICMP unreach */
		icmp_error(args->m, ICMP_UNREACH, code, 0L, 0);
	} else if (args->f_id.proto == IPPROTO_TCP) {
		struct tcphdr *const tcp =
		    L3HDR(struct tcphdr, mtod(args->m, struct ip *));
		if ( (tcp->th_flags & TH_RST) == 0) {
			struct mbuf *m;
			m = ipfw_send_pkt(args->m, &(args->f_id),
				ntohl(tcp->th_seq), ntohl(tcp->th_ack),
				tcp->th_flags | TH_RST);
			if (m != NULL)
				ip_output(m, NULL, NULL, 0, NULL, NULL);
		}
		FREE_PKT(args->m);
	} else
		FREE_PKT(args->m);
	args->m = NULL;
}

/*
 * Support for uid/gid/jail lookup. These tests are expensive
 * (because we may need to look into the list of active sockets)
 * so we cache the results. ugid_lookupp is 0 if we have not
 * yet done a lookup, 1 if we succeeded, and -1 if we tried
 * and failed. The function always returns the match value.
 * We could actually spare the variable and use *uc, setting
 * it to '(void *)check_uidgid if we have no info, NULL if
 * we tried and failed, or any other value if successful.
 */
static int
check_uidgid(ipfw_insn_u32 *insn, struct ip_fw_args *args, int *ugid_lookupp,
    struct ucred **uc)
{
#if defined(USERSPACE)
	return 0;	// not supported in userspace
#else
#ifndef __FreeBSD__
	/* XXX */
	return cred_check(insn, proto, oif,
	    dst_ip, dst_port, src_ip, src_port,
	    (struct bsd_ucred *)uc, ugid_lookupp, ((struct mbuf *)inp)->m_skb);
#else  /* FreeBSD */
	struct in_addr src_ip, dst_ip;
	struct inpcbinfo *pi;
	struct ipfw_flow_id *id;
	struct inpcb *pcb, *inp;
	struct ifnet *oif;
	int lookupflags;
	int match;

	id = &args->f_id;
	inp = args->inp;
	oif = args->oif;

	/*
	 * Check to see if the UDP or TCP stack supplied us with
	 * the PCB. If so, rather then holding a lock and looking
	 * up the PCB, we can use the one that was supplied.
	 */
	if (inp && *ugid_lookupp == 0) {
		INP_LOCK_ASSERT(inp);
		if (inp->inp_socket != NULL) {
			*uc = crhold(inp->inp_cred);
			*ugid_lookupp = 1;
		} else
			*ugid_lookupp = -1;
	}
	/*
	 * If we have already been here and the packet has no
	 * PCB entry associated with it, then we can safely
	 * assume that this is a no match.
	 */
	if (*ugid_lookupp == -1)
		return (0);
	if (id->proto == IPPROTO_TCP) {
		lookupflags = 0;
		pi = &V_tcbinfo;
	} else if (id->proto == IPPROTO_UDP) {
		lookupflags = INPLOOKUP_WILDCARD;
		pi = &V_udbinfo;
	} else
		return 0;
	lookupflags |= INPLOOKUP_RLOCKPCB;
	match = 0;
	if (*ugid_lookupp == 0) {
		if (id->addr_type == 6) {
#ifdef INET6
			if (oif == NULL)
				pcb = in6_pcblookup_mbuf(pi,
				    &id->src_ip6, htons(id->src_port),
				    &id->dst_ip6, htons(id->dst_port),
				    lookupflags, oif, args->m);
			else
				pcb = in6_pcblookup_mbuf(pi,
				    &id->dst_ip6, htons(id->dst_port),
				    &id->src_ip6, htons(id->src_port),
				    lookupflags, oif, args->m);
#else
			*ugid_lookupp = -1;
			return (0);
#endif
		} else {
			src_ip.s_addr = htonl(id->src_ip);
			dst_ip.s_addr = htonl(id->dst_ip);
			if (oif == NULL)
				pcb = in_pcblookup_mbuf(pi,
				    src_ip, htons(id->src_port),
				    dst_ip, htons(id->dst_port),
				    lookupflags, oif, args->m);
			else
				pcb = in_pcblookup_mbuf(pi,
				    dst_ip, htons(id->dst_port),
				    src_ip, htons(id->src_port),
				    lookupflags, oif, args->m);
		}
		if (pcb != NULL) {
			INP_RLOCK_ASSERT(pcb);
			*uc = crhold(pcb->inp_cred);
			*ugid_lookupp = 1;
			INP_RUNLOCK(pcb);
		}
		if (*ugid_lookupp == 0) {
			/*
			 * We tried and failed, set the variable to -1
			 * so we will not try again on this packet.
			 */
			*ugid_lookupp = -1;
			return (0);
		}
	}
	if (insn->o.opcode == O_UID)
		match = ((*uc)->cr_uid == (uid_t)insn->d[0]);
	else if (insn->o.opcode == O_GID)
		match = groupmember((gid_t)insn->d[0], *uc);
	else if (insn->o.opcode == O_JAIL)
		match = ((*uc)->cr_prison->pr_id == (int)insn->d[0]);
	return (match);
#endif /* __FreeBSD__ */
#endif /* not supported in userspace */
}

/*
 * Helper function to set args with info on the rule after the matching
 * one. slot is precise, whereas we guess rule_id as they are
 * assigned sequentially.
 */
static inline void
set_match(struct ip_fw_args *args, int slot,
	struct ip_fw_chain *chain)
{
	args->rule.chain_id = chain->id;
	args->rule.slot = slot + 1; /* we use 0 as a marker */
	args->rule.rule_id = 1 + chain->map[slot]->id;
	args->rule.rulenum = chain->map[slot]->rulenum;
}

/*
 * Helper function to enable cached rule lookups using
 * x_next and next_rule fields in ipfw rule.
 */
static int
jump_fast(struct ip_fw_chain *chain, struct ip_fw *f, int num,
    int tablearg, int jump_backwards)
{
	int f_pos;

	/* If possible use cached f_pos (in f->next_rule),
	 * whose version is written in f->next_rule
	 * (horrible hacks to avoid changing the ABI).
	 */
	if (num != IP_FW_TABLEARG && (uintptr_t)f->x_next == chain->id)
		f_pos = (uintptr_t)f->next_rule;
	else {
		int i = IP_FW_ARG_TABLEARG(num);
		/* make sure we do not jump backward */
		if (jump_backwards == 0 && i <= f->rulenum)
			i = f->rulenum + 1;
		f_pos = ipfw_find_rule(chain, i, 0);
		/* update the cache */
		if (num != IP_FW_TABLEARG) {
			f->next_rule = (void *)(uintptr_t)f_pos;
			f->x_next = (void *)(uintptr_t)chain->id;
		}
	}

	return (f_pos);
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
 *	args->eh (in)	Mac header if present, NULL for layer3 packet.
 *	args->L3offset	Number of bytes bypassed if we came from L2.
 *			e.g. often sizeof(eh)  ** NOTYET **
 *	args->oif	Outgoing interface, NULL if packet is incoming.
 *		The incoming interface is in the mbuf. (in)
 *	args->divert_rule (in/out)
 *		Skip up to the first rule past this rule number;
 *		upon return, non-zero port number for divert or tee.
 *
 *	args->rule	Pointer to the last matching rule (in/out)
 *	args->next_hop	Socket we are forwarding to (out).
 *	args->next_hop6	IPv6 next hop we are forwarding to (out).
 *	args->f_id	Addresses grabbed from the packet (out)
 * 	args->rule.info	a cookie depending on rule action
 *
 * Return value:
 *
 *	IP_FW_PASS	the packet must be accepted
 *	IP_FW_DENY	the packet must be dropped
 *	IP_FW_DIVERT	divert packet, port in m_tag
 *	IP_FW_TEE	tee packet, port in m_tag
 *	IP_FW_DUMMYNET	to dummynet, pipe in args->cookie
 *	IP_FW_NETGRAPH	into netgraph, cookie args->cookie
 *		args->rule contains the matching rule,
 *		args->rule.info has additional information.
 *
 */
int
ipfw_chk(struct ip_fw_args *args)
{

	/*
	 * Local variables holding state while processing a packet:
	 *
	 * IMPORTANT NOTE: to speed up the processing of rules, there
	 * are some assumption on the values of the variables, which
	 * are documented here. Should you change them, please check
	 * the implementation of the various instructions to make sure
	 * that they still work.
	 *
	 * args->eh	The MAC header. It is non-null for a layer2
	 *	packet, it is NULL for a layer-3 packet.
	 * **notyet**
	 * args->L3offset Offset in the packet to the L3 (IP or equiv.) header.
	 *
	 * m | args->m	Pointer to the mbuf, as received from the caller.
	 *	It may change if ipfw_chk() does an m_pullup, or if it
	 *	consumes the packet because it calls send_reject().
	 *	XXX This has to change, so that ipfw_chk() never modifies
	 *	or consumes the buffer.
	 * ip	is the beginning of the ip(4 or 6) header.
	 *	Calculated by adding the L3offset to the start of data.
	 *	(Until we start using L3offset, the packet is
	 *	supposed to start with the ip header).
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
#ifndef __FreeBSD__
	struct bsd_ucred ucred_cache;
#else
	struct ucred *ucred_cache = NULL;
#endif
	int ucred_lookup = 0;

	/*
	 * oif | args->oif	If NULL, ipfw_chk has been called on the
	 *	inbound path (ether_input, ip_input).
	 *	If non-NULL, ipfw_chk has been called on the outbound path
	 *	(ether_output, ip_output).
	 */
	struct ifnet *oif = args->oif;

	int f_pos = 0;		/* index of current rule in the array */
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
	 *	For IPv6 offset|ip6f_mf == 0 means there is no Fragment Header
	 *	or there is a single packet fragement (fragement header added
	 *	without needed).  We will treat a single packet fragment as if
	 *	there was no fragment header (or log/block depending on the
	 *	V_fw_permit_single_frag6 sysctl setting).
	 */
	u_short offset = 0;
	u_short ip6f_mf = 0;

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
	uint8_t proto;
	uint16_t src_port = 0, dst_port = 0;	/* NOTE: host format	*/
	struct in_addr src_ip, dst_ip;		/* NOTE: network format	*/
	uint16_t iplen=0;
	int pktlen;
	uint16_t	etype = 0;	/* Host order stored ether type */

	/*
	 * dyn_dir = MATCH_UNKNOWN when rules unchecked,
	 * 	MATCH_NONE when checked and not matched (q = NULL),
	 *	MATCH_FORWARD or MATCH_REVERSE otherwise (q != NULL)
	 */
	int dyn_dir = MATCH_UNKNOWN;
	ipfw_dyn_rule *q = NULL;
	struct ip_fw_chain *chain = &V_layer3_chain;

	/*
	 * We store in ulp a pointer to the upper layer protocol header.
	 * In the ipv4 case this is easy to determine from the header,
	 * but for ipv6 we might have some additional headers in the middle.
	 * ulp is NULL if not found.
	 */
	void *ulp = NULL;		/* upper layer protocol pointer. */

	/* XXX ipv6 variables */
	int is_ipv6 = 0;
	uint8_t	icmp6_type = 0;
	uint16_t ext_hd = 0;	/* bits vector for extension header filtering */
	/* end of ipv6 variables */

	int is_ipv4 = 0;

	int done = 0;		/* flag to exit the outer loop */

	if (m->m_flags & M_SKIP_FIREWALL || (! V_ipfw_vnet_ready))
		return (IP_FW_PASS);	/* accept */

	dst_ip.s_addr = 0;		/* make sure it is initialized */
	src_ip.s_addr = 0;		/* make sure it is initialized */
	pktlen = m->m_pkthdr.len;
	args->f_id.fib = M_GETFIB(m); /* note mbuf not altered) */
	proto = args->f_id.proto = 0;	/* mark f_id invalid */
		/* XXX 0 is a valid proto: IP/IPv6 Hop-by-Hop Option */

/*
 * PULLUP_TO(len, p, T) makes sure that len + sizeof(T) is contiguous,
 * then it sets p to point at the offset "len" in the mbuf. WARNING: the
 * pointer might become stale after other pullups (but we never use it
 * this way).
 */
#define PULLUP_TO(_len, p, T)	PULLUP_LEN(_len, p, sizeof(T))
#define PULLUP_LEN(_len, p, T)					\
do {								\
	int x = (_len) + T;					\
	if ((m)->m_len < x) {					\
		args->m = m = m_pullup(m, x);			\
		if (m == NULL)					\
			goto pullup_failed;			\
	}							\
	p = (mtod(m, char *) + (_len));				\
} while (0)

	/*
	 * if we have an ether header,
	 */
	if (args->eh)
		etype = ntohs(args->eh->ether_type);

	/* Identify IP packets and fill up variables. */
	if (pktlen >= sizeof(struct ip6_hdr) &&
	    (args->eh == NULL || etype == ETHERTYPE_IPV6) && ip->ip_v == 6) {
		struct ip6_hdr *ip6 = (struct ip6_hdr *)ip;
		is_ipv6 = 1;
		args->f_id.addr_type = 6;
		hlen = sizeof(struct ip6_hdr);
		proto = ip6->ip6_nxt;

		/* Search extension headers to find upper layer protocols */
		while (ulp == NULL && offset == 0) {
			switch (proto) {
			case IPPROTO_ICMPV6:
				PULLUP_TO(hlen, ulp, struct icmp6_hdr);
				icmp6_type = ICMP6(ulp)->icmp6_type;
				break;

			case IPPROTO_TCP:
				PULLUP_TO(hlen, ulp, struct tcphdr);
				dst_port = TCP(ulp)->th_dport;
				src_port = TCP(ulp)->th_sport;
				/* save flags for dynamic rules */
				args->f_id._flags = TCP(ulp)->th_flags;
				break;

			case IPPROTO_SCTP:
				PULLUP_TO(hlen, ulp, struct sctphdr);
				src_port = SCTP(ulp)->src_port;
				dst_port = SCTP(ulp)->dest_port;
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
				switch (((struct ip6_rthdr *)ulp)->ip6r_type) {
				case 0:
					ext_hd |= EXT_RTHDR0;
					break;
				case 2:
					ext_hd |= EXT_RTHDR2;
					break;
				default:
					if (V_fw_verbose)
						printf("IPFW2: IPV6 - Unknown "
						    "Routing Header type(%d)\n",
						    ((struct ip6_rthdr *)
						    ulp)->ip6r_type);
					if (V_fw_deny_unknown_exthdrs)
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
				ip6f_mf = ((struct ip6_frag *)ulp)->ip6f_offlg &
					IP6F_MORE_FRAG;
				if (V_fw_permit_single_frag6 == 0 &&
				    offset == 0 && ip6f_mf == 0) {
					if (V_fw_verbose)
						printf("IPFW2: IPV6 - Invalid "
						    "Fragment Header\n");
					if (V_fw_deny_unknown_exthdrs)
					    return (IP_FW_DENY);
					break;
				}
				args->f_id.extra =
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
				/*
				 * Packet ends here, and IPv6 header has
				 * already been pulled up. If ip6e_len!=0
				 * then octets must be ignored.
				 */
				ulp = ip; /* non-NULL to get out of loop. */
				break;

			case IPPROTO_OSPFIGP:
				/* XXX OSPF header check? */
				PULLUP_TO(hlen, ulp, struct ip6_ext);
				break;

			case IPPROTO_PIM:
				/* XXX PIM header check? */
				PULLUP_TO(hlen, ulp, struct pim);
				break;

			case IPPROTO_CARP:
				PULLUP_TO(hlen, ulp, struct carp_header);
				if (((struct carp_header *)ulp)->carp_version !=
				    CARP_VERSION) 
					return (IP_FW_DENY);
				if (((struct carp_header *)ulp)->carp_type !=
				    CARP_ADVERTISEMENT) 
					return (IP_FW_DENY);
				break;

			case IPPROTO_IPV6:	/* RFC 2893 */
				PULLUP_TO(hlen, ulp, struct ip6_hdr);
				break;

			case IPPROTO_IPV4:	/* RFC 2893 */
				PULLUP_TO(hlen, ulp, struct ip);
				break;

			default:
				if (V_fw_verbose)
					printf("IPFW2: IPV6 - Unknown "
					    "Extension Header(%d), ext_hd=%x\n",
					     proto, ext_hd);
				if (V_fw_deny_unknown_exthdrs)
				    return (IP_FW_DENY);
				PULLUP_TO(hlen, ulp, struct ip6_ext);
				break;
			} /*switch */
		}
		ip = mtod(m, struct ip *);
		ip6 = (struct ip6_hdr *)ip;
		args->f_id.src_ip6 = ip6->ip6_src;
		args->f_id.dst_ip6 = ip6->ip6_dst;
		args->f_id.src_ip = 0;
		args->f_id.dst_ip = 0;
		args->f_id.flow_id6 = ntohl(ip6->ip6_flow);
	} else if (pktlen >= sizeof(struct ip) &&
	    (args->eh == NULL || etype == ETHERTYPE_IP) && ip->ip_v == 4) {
	    	is_ipv4 = 1;
		hlen = ip->ip_hl << 2;
		args->f_id.addr_type = 4;

		/*
		 * Collect parameters into local variables for faster matching.
		 */
		proto = ip->ip_p;
		src_ip = ip->ip_src;
		dst_ip = ip->ip_dst;
		offset = ntohs(ip->ip_off) & IP_OFFMASK;
		iplen = ntohs(ip->ip_len);
		pktlen = iplen < pktlen ? iplen : pktlen;

		if (offset == 0) {
			switch (proto) {
			case IPPROTO_TCP:
				PULLUP_TO(hlen, ulp, struct tcphdr);
				dst_port = TCP(ulp)->th_dport;
				src_port = TCP(ulp)->th_sport;
				/* save flags for dynamic rules */
				args->f_id._flags = TCP(ulp)->th_flags;
				break;

			case IPPROTO_SCTP:
				PULLUP_TO(hlen, ulp, struct sctphdr);
				src_port = SCTP(ulp)->src_port;
				dst_port = SCTP(ulp)->dest_port;
				break;

			case IPPROTO_UDP:
				PULLUP_TO(hlen, ulp, struct udphdr);
				dst_port = UDP(ulp)->uh_dport;
				src_port = UDP(ulp)->uh_sport;
				break;

			case IPPROTO_ICMP:
				PULLUP_TO(hlen, ulp, struct icmphdr);
				//args->f_id.flags = ICMP(ulp)->icmp_type;
				break;

			default:
				break;
			}
		}

		ip = mtod(m, struct ip *);
		args->f_id.src_ip = ntohl(src_ip.s_addr);
		args->f_id.dst_ip = ntohl(dst_ip.s_addr);
	}
#undef PULLUP_TO
	if (proto) { /* we may have port numbers, store them */
		args->f_id.proto = proto;
		args->f_id.src_port = src_port = ntohs(src_port);
		args->f_id.dst_port = dst_port = ntohs(dst_port);
	}

	IPFW_PF_RLOCK(chain);
	if (! V_ipfw_vnet_ready) { /* shutting down, leave NOW. */
		IPFW_PF_RUNLOCK(chain);
		return (IP_FW_PASS);	/* accept */
	}
	if (args->rule.slot) {
		/*
		 * Packet has already been tagged as a result of a previous
		 * match on rule args->rule aka args->rule_id (PIPE, QUEUE,
		 * REASS, NETGRAPH, DIVERT/TEE...)
		 * Validate the slot and continue from the next one
		 * if still present, otherwise do a lookup.
		 */
		f_pos = (args->rule.chain_id == chain->id) ?
		    args->rule.slot :
		    ipfw_find_rule(chain, args->rule.rulenum,
			args->rule.rule_id);
	} else {
		f_pos = 0;
	}

	/*
	 * Now scan the rules, and parse microinstructions for each rule.
	 * We have two nested loops and an inner switch. Sometimes we
	 * need to break out of one or both loops, or re-enter one of
	 * the loops with updated variables. Loop variables are:
	 *
	 *	f_pos (outer loop) points to the current rule.
	 *		On output it points to the matching rule.
	 *	done (outer loop) is used as a flag to break the loop.
	 *	l (inner loop)	residual length of current rule.
	 *		cmd points to the current microinstruction.
	 *
	 * We break the inner loop by setting l=0 and possibly
	 * cmdlen=0 if we don't want to advance cmd.
	 * We break the outer loop by setting done=1
	 * We can restart the inner loop by setting l>0 and f_pos, f, cmd
	 * as needed.
	 */
	for (; f_pos < chain->n_rules; f_pos++) {
		ipfw_insn *cmd;
		uint32_t tablearg = 0;
		int l, cmdlen, skip_or; /* skip rest of OR block */
		struct ip_fw *f;

		f = chain->map[f_pos];
		if (V_set_disable & (1 << f->set) )
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

/* check_body: */
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
				if (offset != 0)
					break;
				if (proto == IPPROTO_TCP ||
				    proto == IPPROTO_UDP)
					match = check_uidgid(
						    (ipfw_insn_u32 *)cmd,
						    args, &ucred_lookup,
#ifdef __FreeBSD__
						    &ucred_cache);
#else
						    (void *)&ucred_cache);
#endif
				break;

			case O_RECV:
				match = iface_match(m->m_pkthdr.rcvif,
				    (ipfw_insn_if *)cmd, chain, &tablearg);
				break;

			case O_XMIT:
				match = iface_match(oif, (ipfw_insn_if *)cmd,
				    chain, &tablearg);
				break;

			case O_VIA:
				match = iface_match(oif ? oif :
				    m->m_pkthdr.rcvif, (ipfw_insn_if *)cmd,
				    chain, &tablearg);
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
					u_int16_t *p =
					    ((ipfw_insn_u16 *)cmd)->ports;
					int i;

					for (i = cmdlen - 1; !match && i>0;
					    i--, p += 2)
						match = (etype >= p[0] &&
						    etype <= p[1]);
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
			    {
				/* For diverted packets, args->rule.info
				 * contains the divert port (in host format)
				 * reason and direction.
				 */
				uint32_t i = args->rule.info;
				match = (i&IPFW_IS_MASK) == IPFW_IS_DIVERT &&
				    cmd->arg1 & ((i & IPFW_INFO_IN) ? 1 : 2);
			    }
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
				    uint32_t key =
					(cmd->opcode == O_IP_DST_LOOKUP) ?
					    dst_ip.s_addr : src_ip.s_addr;
				    uint32_t v = 0;

				    if (cmdlen > F_INSN_SIZE(ipfw_insn_u32)) {
					/* generic lookup. The key must be
					 * in 32bit big-endian format.
					 */
					v = ((ipfw_insn_u32 *)cmd)->d[1];
					if (v == 0)
					    key = dst_ip.s_addr;
					else if (v == 1)
					    key = src_ip.s_addr;
					else if (v == 6) /* dscp */
					    key = (ip->ip_tos >> 2) & 0x3f;
					else if (offset != 0)
					    break;
					else if (proto != IPPROTO_TCP &&
						proto != IPPROTO_UDP)
					    break;
					else if (v == 2)
					    key = htonl(dst_port);
					else if (v == 3)
					    key = htonl(src_port);
#ifndef USERSPACE
					else if (v == 4 || v == 5) {
					    check_uidgid(
						(ipfw_insn_u32 *)cmd,
						args, &ucred_lookup,
#ifdef __FreeBSD__
						&ucred_cache);
					    if (v == 4 /* O_UID */)
						key = ucred_cache->cr_uid;
					    else if (v == 5 /* O_JAIL */)
						key = ucred_cache->cr_prison->pr_id;
#else /* !__FreeBSD__ */
						(void *)&ucred_cache);
					    if (v ==4 /* O_UID */)
						key = ucred_cache.uid;
					    else if (v == 5 /* O_JAIL */)
						key = ucred_cache.xid;
#endif /* !__FreeBSD__ */
					    key = htonl(key);
					} else
#endif /* !USERSPACE */
					    break;
				    }
				    match = ipfw_lookup_table(chain,
					cmd->arg1, key, &v);
				    if (!match)
					break;
				    if (cmdlen == F_INSN_SIZE(ipfw_insn_u32))
					match =
					    ((ipfw_insn_u32 *)cmd)->d[0] == v;
				    else
					tablearg = v;
				} else if (is_ipv6) {
					uint32_t v = 0;
					void *pkey = (cmd->opcode == O_IP_DST_LOOKUP) ?
						&args->f_id.dst_ip6: &args->f_id.src_ip6;
					match = ipfw_lookup_table_extended(chain,
							cmd->arg1, pkey, &v,
							IPFW_TABLE_CIDR);
					if (cmdlen == F_INSN_SIZE(ipfw_insn_u32))
						match = ((ipfw_insn_u32 *)cmd)->d[0] == v;
					if (match)
						tablearg = v;
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
					break;
				}
#ifdef INET6
				/* FALLTHROUGH */
			case O_IP6_SRC_ME:
				match= is_ipv6 && search_ip6_addr_net(&args->f_id.src_ip6);
#endif
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
					break;
				}
#ifdef INET6
				/* FALLTHROUGH */
			case O_IP6_DST_ME:
				match= is_ipv6 && search_ip6_addr_net(&args->f_id.dst_ip6);
#endif
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
				    ipopts_match(ip, cmd) );
				break;

			case O_IPVER:
				match = (is_ipv4 &&
				    cmd->arg1 == ip->ip_v);
				break;

			case O_IPID:
			case O_IPLEN:
			case O_IPTTL:
				if (is_ipv4) {	/* only for IP packets */
				    uint16_t x;
				    uint16_t *p;
				    int i;

				    if (cmd->opcode == O_IPLEN)
					x = iplen;
				    else if (cmd->opcode == O_IPTTL)
					x = ip->ip_ttl;
				    else /* must be IPID */
					x = ntohs(ip->ip_id);
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
				    (cmd->arg1 == (ip->ip_tos & 0xe0)) );
				break;

			case O_IPTOS:
				match = (is_ipv4 &&
				    flags_match(cmd, ip->ip_tos));
				break;

			case O_DSCP:
			    {
				uint32_t *p;
				uint16_t x;

				p = ((ipfw_insn_u32 *)cmd)->d;

				if (is_ipv4)
					x = ip->ip_tos >> 2;
				else if (is_ipv6) {
					uint8_t *v;
					v = &((struct ip6_hdr *)ip)->ip6_vfc;
					x = (*v & 0x0F) << 2;
					v++;
					x |= *v >> 6;
				} else
					break;

				/* DSCP bitmask is stored as low_u32 high_u32 */
				if (x > 32)
					match = *(p + 1) & (1 << (x - 32));
				else
					match = *p & (1 << x);
			    }
				break;

			case O_TCPDATALEN:
				if (proto == IPPROTO_TCP && offset == 0) {
				    struct tcphdr *tcp;
				    uint16_t x;
				    uint16_t *p;
				    int i;

				    tcp = TCP(ulp);
				    x = iplen -
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
				PULLUP_LEN(hlen, ulp, (TCP(ulp)->th_off << 2));
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
				if (proto == IPPROTO_TCP && offset == 0) {
				    uint16_t x;
				    uint16_t *p;
				    int i;

				    x = ntohs(TCP(ulp)->th_win);
				    if (cmdlen == 1) {
					match = (cmd->arg1 == x);
					break;
				    }
				    /* Otherwise we have ranges. */
				    p = ((ipfw_insn_u16 *)cmd)->ports;
				    i = cmdlen - 1;
				    for (; !match && i > 0; i--, p += 2)
					match = (x >= p[0] && x <= p[1]);
				}
				break;

			case O_ESTAB:
				/* reject packets which have SYN only */
				/* XXX should i also check for TH_ACK ? */
				match = (proto == IPPROTO_TCP && offset == 0 &&
				    (TCP(ulp)->th_flags &
				     (TH_RST | TH_ACK | TH_SYN)) != TH_SYN);
				break;

			case O_ALTQ: {
				struct pf_mtag *at;
				struct m_tag *mtag;
				ipfw_insn_altq *altq = (ipfw_insn_altq *)cmd;

				/*
				 * ALTQ uses mbuf tags from another
				 * packet filtering system - pf(4).
				 * We allocate a tag in its format
				 * and fill it in, pretending to be pf(4).
				 */
				match = 1;
				at = pf_find_mtag(m);
				if (at != NULL && at->qid != 0)
					break;
				mtag = m_tag_get(PACKET_TAG_PF,
				    sizeof(struct pf_mtag), M_NOWAIT | M_ZERO);
				if (mtag == NULL) {
					/*
					 * Let the packet fall back to the
					 * default ALTQ.
					 */
					break;
				}
				m_tag_prepend(m, mtag);
				at = (struct pf_mtag *)(mtag + 1);
				at->qid = altq->qid;
				at->hdr = ip;
				break;
			}

			case O_LOG:
				ipfw_log(f, hlen, args, m,
				    oif, offset | ip6f_mf, tablearg, ip);
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
					    m->m_pkthdr.rcvif, args->f_id.fib) :
#endif
				    verify_path(src_ip, m->m_pkthdr.rcvif,
				        args->f_id.fib)));
				break;

			case O_VERSRCREACH:
				/* Outgoing packets automatically pass/match */
				match = (hlen > 0 && ((oif != NULL) ||
#ifdef INET6
				    is_ipv6 ?
				        verify_path6(&(args->f_id.src_ip6),
				            NULL, args->f_id.fib) :
#endif
				    verify_path(src_ip, NULL, args->f_id.fib)));
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
					        m->m_pkthdr.rcvif,
						args->f_id.fib) :
#endif
					    verify_path(src_ip,
					    	m->m_pkthdr.rcvif,
					        args->f_id.fib);
				else
					match = 1;
				break;

			case O_IPSEC:
#ifdef IPSEC
				match = (m_tag_find(m,
				    PACKET_TAG_IPSEC_IN_DONE, NULL) != NULL);
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
			case O_IP6_DST_MASK:
				if (is_ipv6) {
					int i = cmdlen - 1;
					struct in6_addr p;
					struct in6_addr *d =
					    &((ipfw_insn_ip6 *)cmd)->addr6;

					for (; !match && i > 0; d += 2,
					    i -= F_INSN_SIZE(struct in6_addr)
					    * 2) {
						p = (cmd->opcode ==
						    O_IP6_SRC_MASK) ?
						    args->f_id.src_ip6:
						    args->f_id.dst_ip6;
						APPLY_MASK(&p, &d[1]);
						match =
						    IN6_ARE_ADDR_EQUAL(&d[0],
						    &p);
					}
				}
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

			case O_TAG: {
				struct m_tag *mtag;
				uint32_t tag = IP_FW_ARG_TABLEARG(cmd->arg1);

				/* Packet is already tagged with this tag? */
				mtag = m_tag_locate(m, MTAG_IPFW, tag, NULL);

				/* We have `untag' action when F_NOT flag is
				 * present. And we must remove this mtag from
				 * mbuf and reset `match' to zero (`match' will
				 * be inversed later).
				 * Otherwise we should allocate new mtag and
				 * push it into mbuf.
				 */
				if (cmd->len & F_NOT) { /* `untag' action */
					if (mtag != NULL)
						m_tag_delete(m, mtag);
					match = 0;
				} else {
					if (mtag == NULL) {
						mtag = m_tag_alloc( MTAG_IPFW,
						    tag, 0, M_NOWAIT);
						if (mtag != NULL)
							m_tag_prepend(m, mtag);
					}
					match = 1;
				}
				break;
			}

			case O_FIB: /* try match the specified fib */
				if (args->f_id.fib == cmd->arg1)
					match = 1;
				break;

			case O_SOCKARG:	{
#ifndef USERSPACE	/* not supported in userspace */
				struct inpcb *inp = args->inp;
				struct inpcbinfo *pi;
				
				if (is_ipv6) /* XXX can we remove this ? */
					break;

				if (proto == IPPROTO_TCP)
					pi = &V_tcbinfo;
				else if (proto == IPPROTO_UDP)
					pi = &V_udbinfo;
				else
					break;

				/*
				 * XXXRW: so_user_cookie should almost
				 * certainly be inp_user_cookie?
				 */

				/* For incomming packet, lookup up the 
				inpcb using the src/dest ip/port tuple */
				if (inp == NULL) {
					inp = in_pcblookup(pi, 
						src_ip, htons(src_port),
						dst_ip, htons(dst_port),
						INPLOOKUP_RLOCKPCB, NULL);
					if (inp != NULL) {
						tablearg =
						    inp->inp_socket->so_user_cookie;
						if (tablearg)
							match = 1;
						INP_RUNLOCK(inp);
					}
				} else {
					if (inp->inp_socket) {
						tablearg =
						    inp->inp_socket->so_user_cookie;
						if (tablearg)
							match = 1;
					}
				}
#endif /* !USERSPACE */
				break;
			}

			case O_TAGGED: {
				struct m_tag *mtag;
				uint32_t tag = IP_FW_ARG_TABLEARG(cmd->arg1);

				if (cmdlen == 1) {
					match = m_tag_locate(m, MTAG_IPFW,
					    tag, NULL) != NULL;
					break;
				}

				/* we have ranges */
				for (mtag = m_tag_first(m);
				    mtag != NULL && !match;
				    mtag = m_tag_next(m, mtag)) {
					uint16_t *p;
					int i;

					if (mtag->m_tag_cookie != MTAG_IPFW)
						continue;

					p = ((ipfw_insn_u16 *)cmd)->ports;
					i = cmdlen - 1;
					for(; !match && i > 0; i--, p += 2)
						match =
						    mtag->m_tag_id >= p[0] &&
						    mtag->m_tag_id <= p[1];
				}
				break;
			}
				
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
			 * but we need to set l=0, done=1)
			 *
			 * Exceptions:
			 * O_COUNT and O_SKIPTO actions:
			 *   instead of terminating, we jump to the next rule
			 *   (setting l=0), or to the SKIPTO target (setting
			 *   f/f_len, cmd and l as needed), respectively.
			 *
			 * O_TAG, O_LOG and O_ALTQ action parameters:
			 *   perform some action and set match = 1;
			 *
			 * O_LIMIT and O_KEEP_STATE: these opcodes are
			 *   not real 'actions', and are stored right
			 *   before the 'action' part of the rule.
			 *   These opcodes try to install an entry in the
			 *   state tables; if successful, we continue with
			 *   the next opcode (match=1; break;), otherwise
			 *   the packet must be dropped (set retval,
			 *   break loops with l=0, done=1)
			 *
			 * O_PROBE_STATE and O_CHECK_STATE: these opcodes
			 *   cause a lookup of the state table, and a jump
			 *   to the 'action' part of the parent rule
			 *   if an entry is found, or
			 *   (CHECK_STATE only) a jump to the next rule if
			 *   the entry is not found.
			 *   The result of the lookup is cached so that
			 *   further instances of these opcodes become NOPs.
			 *   The jump to the next rule is done by setting
			 *   l=0, cmdlen=0.
			 */
			case O_LIMIT:
			case O_KEEP_STATE:
				if (ipfw_install_state(f,
				    (ipfw_insn_limit *)cmd, args, tablearg)) {
					/* error or limit violation */
					retval = IP_FW_DENY;
					l = 0;	/* exit inner loop */
					done = 1; /* exit outer loop */
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
				    (q = ipfw_lookup_dyn_rule(&args->f_id,
				     &dyn_dir, proto == IPPROTO_TCP ?
					TCP(ulp) : NULL))
					!= NULL) {
					/*
					 * Found dynamic entry, update stats
					 * and jump to the 'action' part of
					 * the parent rule by setting
					 * f, cmd, l and clearing cmdlen.
					 */
					IPFW_INC_DYN_COUNTER(q, pktlen);
					/* XXX we would like to have f_pos
					 * readily accessible in the dynamic
				         * rule, instead of having to
					 * lookup q->rule.
					 */
					f = q->rule;
					f_pos = ipfw_find_rule(chain,
						f->rulenum, f->id);
					cmd = ACTION_PTR(f);
					l = f->cmd_len - f->act_ofs;
					ipfw_dyn_unlock(q);
					cmdlen = 0;
					match = 1;
					break;
				}
				/*
				 * Dynamic entry not found. If CHECK_STATE,
				 * skip to next rule, if PROBE_STATE just
				 * ignore and continue with next opcode.
				 */
				if (cmd->opcode == O_CHECK_STATE)
					l = 0;	/* exit inner loop */
				match = 1;
				break;

			case O_ACCEPT:
				retval = 0;	/* accept */
				l = 0;		/* exit inner loop */
				done = 1;	/* exit outer loop */
				break;

			case O_PIPE:
			case O_QUEUE:
				set_match(args, f_pos, chain);
				args->rule.info = IP_FW_ARG_TABLEARG(cmd->arg1);
				if (cmd->opcode == O_PIPE)
					args->rule.info |= IPFW_IS_PIPE;
				if (V_fw_one_pass)
					args->rule.info |= IPFW_ONEPASS;
				retval = IP_FW_DUMMYNET;
				l = 0;          /* exit inner loop */
				done = 1;       /* exit outer loop */
				break;

			case O_DIVERT:
			case O_TEE:
				if (args->eh) /* not on layer 2 */
				    break;
				/* otherwise this is terminal */
				l = 0;		/* exit inner loop */
				done = 1;	/* exit outer loop */
				retval = (cmd->opcode == O_DIVERT) ?
					IP_FW_DIVERT : IP_FW_TEE;
				set_match(args, f_pos, chain);
				args->rule.info = IP_FW_ARG_TABLEARG(cmd->arg1);
				break;

			case O_COUNT:
				IPFW_INC_RULE_COUNTER(f, pktlen);
				l = 0;		/* exit inner loop */
				break;

			case O_SKIPTO:
			    IPFW_INC_RULE_COUNTER(f, pktlen);
			    f_pos = jump_fast(chain, f, cmd->arg1, tablearg, 0);
			    /*
			     * Skip disabled rules, and re-enter
			     * the inner loop with the correct
			     * f_pos, f, l and cmd.
			     * Also clear cmdlen and skip_or
			     */
			    for (; f_pos < chain->n_rules - 1 &&
				    (V_set_disable &
				     (1 << chain->map[f_pos]->set));
				    f_pos++)
				;
			    /* Re-enter the inner loop at the skipto rule. */
			    f = chain->map[f_pos];
			    l = f->cmd_len;
			    cmd = f->cmd;
			    match = 1;
			    cmdlen = 0;
			    skip_or = 0;
			    continue;
			    break;	/* not reached */

			case O_CALLRETURN: {
				/*
				 * Implementation of `subroutine' call/return,
				 * in the stack carried in an mbuf tag. This
				 * is different from `skipto' in that any call
				 * address is possible (`skipto' must prevent
				 * backward jumps to avoid endless loops).
				 * We have `return' action when F_NOT flag is
				 * present. The `m_tag_id' field is used as
				 * stack pointer.
				 */
				struct m_tag *mtag;
				uint16_t jmpto, *stack;

#define	IS_CALL		((cmd->len & F_NOT) == 0)
#define	IS_RETURN	((cmd->len & F_NOT) != 0)
				/*
				 * Hand-rolled version of m_tag_locate() with
				 * wildcard `type'.
				 * If not already tagged, allocate new tag.
				 */
				mtag = m_tag_first(m);
				while (mtag != NULL) {
					if (mtag->m_tag_cookie ==
					    MTAG_IPFW_CALL)
						break;
					mtag = m_tag_next(m, mtag);
				}
				if (mtag == NULL && IS_CALL) {
					mtag = m_tag_alloc(MTAG_IPFW_CALL, 0,
					    IPFW_CALLSTACK_SIZE *
					    sizeof(uint16_t), M_NOWAIT);
					if (mtag != NULL)
						m_tag_prepend(m, mtag);
				}

				/*
				 * On error both `call' and `return' just
				 * continue with next rule.
				 */
				if (IS_RETURN && (mtag == NULL ||
				    mtag->m_tag_id == 0)) {
					l = 0;		/* exit inner loop */
					break;
				}
				if (IS_CALL && (mtag == NULL ||
				    mtag->m_tag_id >= IPFW_CALLSTACK_SIZE)) {
					printf("ipfw: call stack error, "
					    "go to next rule\n");
					l = 0;		/* exit inner loop */
					break;
				}

				IPFW_INC_RULE_COUNTER(f, pktlen);
				stack = (uint16_t *)(mtag + 1);

				/*
				 * The `call' action may use cached f_pos
				 * (in f->next_rule), whose version is written
				 * in f->next_rule.
				 * The `return' action, however, doesn't have
				 * fixed jump address in cmd->arg1 and can't use
				 * cache.
				 */
				if (IS_CALL) {
					stack[mtag->m_tag_id] = f->rulenum;
					mtag->m_tag_id++;
			    		f_pos = jump_fast(chain, f, cmd->arg1,
					    tablearg, 1);
				} else {	/* `return' action */
					mtag->m_tag_id--;
					jmpto = stack[mtag->m_tag_id] + 1;
					f_pos = ipfw_find_rule(chain, jmpto, 0);
				}

				/*
				 * Skip disabled rules, and re-enter
				 * the inner loop with the correct
				 * f_pos, f, l and cmd.
				 * Also clear cmdlen and skip_or
				 */
				for (; f_pos < chain->n_rules - 1 &&
				    (V_set_disable &
				    (1 << chain->map[f_pos]->set)); f_pos++)
					;
				/* Re-enter the inner loop at the dest rule. */
				f = chain->map[f_pos];
				l = f->cmd_len;
				cmd = f->cmd;
				cmdlen = 0;
				skip_or = 0;
				continue;
				break;	/* NOTREACHED */
			}
#undef IS_CALL
#undef IS_RETURN

			case O_REJECT:
				/*
				 * Drop the packet and send a reject notice
				 * if the packet is not ICMP (or is an ICMP
				 * query), and it is not multicast/broadcast.
				 */
				if (hlen > 0 && is_ipv4 && offset == 0 &&
				    (proto != IPPROTO_ICMP ||
				     is_icmp_query(ICMP(ulp))) &&
				    !(m->m_flags & (M_BCAST|M_MCAST)) &&
				    !IN_MULTICAST(ntohl(dst_ip.s_addr))) {
					send_reject(args, cmd->arg1, iplen, ip);
					m = args->m;
				}
				/* FALLTHROUGH */
#ifdef INET6
			case O_UNREACH6:
				if (hlen > 0 && is_ipv6 &&
				    ((offset & IP6F_OFF_MASK) == 0) &&
				    (proto != IPPROTO_ICMPV6 ||
				     (is_icmp6_query(icmp6_type) == 1)) &&
				    !(m->m_flags & (M_BCAST|M_MCAST)) &&
				    !IN6_IS_ADDR_MULTICAST(&args->f_id.dst_ip6)) {
					send_reject6(
					    args, cmd->arg1, hlen,
					    (struct ip6_hdr *)ip);
					m = args->m;
				}
				/* FALLTHROUGH */
#endif
			case O_DENY:
				retval = IP_FW_DENY;
				l = 0;		/* exit inner loop */
				done = 1;	/* exit outer loop */
				break;

			case O_FORWARD_IP:
				if (args->eh)	/* not valid on layer2 pkts */
					break;
				if (q == NULL || q->rule != f ||
				    dyn_dir == MATCH_FORWARD) {
				    struct sockaddr_in *sa;
				    sa = &(((ipfw_insn_sa *)cmd)->sa);
				    if (sa->sin_addr.s_addr == INADDR_ANY) {
					bcopy(sa, &args->hopstore,
							sizeof(*sa));
					args->hopstore.sin_addr.s_addr =
						    htonl(tablearg);
					args->next_hop = &args->hopstore;
				    } else {
					args->next_hop = sa;
				    }
				}
				retval = IP_FW_PASS;
				l = 0;          /* exit inner loop */
				done = 1;       /* exit outer loop */
				break;

#ifdef INET6
			case O_FORWARD_IP6:
				if (args->eh)	/* not valid on layer2 pkts */
					break;
				if (q == NULL || q->rule != f ||
				    dyn_dir == MATCH_FORWARD) {
					struct sockaddr_in6 *sin6;

					sin6 = &(((ipfw_insn_sa6 *)cmd)->sa);
					args->next_hop6 = sin6;
				}
				retval = IP_FW_PASS;
				l = 0;		/* exit inner loop */
				done = 1;	/* exit outer loop */
				break;
#endif

			case O_NETGRAPH:
			case O_NGTEE:
				set_match(args, f_pos, chain);
				args->rule.info = IP_FW_ARG_TABLEARG(cmd->arg1);
				if (V_fw_one_pass)
					args->rule.info |= IPFW_ONEPASS;
				retval = (cmd->opcode == O_NETGRAPH) ?
				    IP_FW_NETGRAPH : IP_FW_NGTEE;
				l = 0;          /* exit inner loop */
				done = 1;       /* exit outer loop */
				break;

			case O_SETFIB: {
				uint32_t fib;

				IPFW_INC_RULE_COUNTER(f, pktlen);
				fib = IP_FW_ARG_TABLEARG(cmd->arg1);
				if (fib >= rt_numfibs)
					fib = 0;
				M_SETFIB(m, fib);
				args->f_id.fib = fib;
				l = 0;		/* exit inner loop */
				break;
		        }

			case O_SETDSCP: {
				uint16_t code;

				code = IP_FW_ARG_TABLEARG(cmd->arg1) & 0x3F;
				l = 0;		/* exit inner loop */
				if (is_ipv4) {
					uint16_t a;

					a = ip->ip_tos;
					ip->ip_tos = (code << 2) | (ip->ip_tos & 0x03);
					a += ntohs(ip->ip_sum) - ip->ip_tos;
					ip->ip_sum = htons(a);
				} else if (is_ipv6) {
					uint8_t *v;

					v = &((struct ip6_hdr *)ip)->ip6_vfc;
					*v = (*v & 0xF0) | (code >> 2);
					v++;
					*v = (*v & 0x3F) | ((code & 0x03) << 6);
				} else
					break;

				IPFW_INC_RULE_COUNTER(f, pktlen);
				break;
			}

			case O_NAT:
				l = 0;          /* exit inner loop */
				done = 1;       /* exit outer loop */
 				if (!IPFW_NAT_LOADED) {
				    retval = IP_FW_DENY;
				    break;
				}

				struct cfg_nat *t;
				int nat_id;

				set_match(args, f_pos, chain);
				/* Check if this is 'global' nat rule */
				if (cmd->arg1 == 0) {
					retval = ipfw_nat_ptr(args, NULL, m);
					break;
				}
				t = ((ipfw_insn_nat *)cmd)->nat;
				if (t == NULL) {
					nat_id = IP_FW_ARG_TABLEARG(cmd->arg1);
					t = (*lookup_nat_ptr)(&chain->nat, nat_id);

					if (t == NULL) {
					    retval = IP_FW_DENY;
					    break;
					}
					if (cmd->arg1 != IP_FW_TABLEARG)
					    ((ipfw_insn_nat *)cmd)->nat = t;
				}
				retval = ipfw_nat_ptr(args, t, m);
				break;

			case O_REASS: {
				int ip_off;

				IPFW_INC_RULE_COUNTER(f, pktlen);
				l = 0;	/* in any case exit inner loop */
				ip_off = ntohs(ip->ip_off);

				/* if not fragmented, go to next rule */
				if ((ip_off & (IP_MF | IP_OFFMASK)) == 0)
				    break;

				args->m = m = ip_reass(m);

				/*
				 * do IP header checksum fixup.
				 */
				if (m == NULL) { /* fragment got swallowed */
				    retval = IP_FW_DENY;
				} else { /* good, packet complete */
				    int hlen;

				    ip = mtod(m, struct ip *);
				    hlen = ip->ip_hl << 2;
				    ip->ip_sum = 0;
				    if (hlen == sizeof(struct ip))
					ip->ip_sum = in_cksum_hdr(ip);
				    else
					ip->ip_sum = in_cksum(m, hlen);
				    retval = IP_FW_REASS;
				    set_match(args, f_pos, chain);
				}
				done = 1;	/* exit outer loop */
				break;
			}

			default:
				panic("-- unknown opcode %d\n", cmd->opcode);
			} /* end of switch() on opcodes */
			/*
			 * if we get here with l=0, then match is irrelevant.
			 */

			if (cmd->len & F_NOT)
				match = !match;

			if (match) {
				if (cmd->len & F_OR)
					skip_or = 1;
			} else {
				if (!(cmd->len & F_OR)) /* not an OR block, */
					break;		/* try next rule    */
			}

		}	/* end of inner loop, scan opcodes */
#undef PULLUP_LEN

		if (done)
			break;

/* next_rule:; */	/* try next rule		*/

	}		/* end of outer for, scan rules */

	if (done) {
		struct ip_fw *rule = chain->map[f_pos];
		/* Update statistics */
		IPFW_INC_RULE_COUNTER(rule, pktlen);
	} else {
		retval = IP_FW_DENY;
		printf("ipfw: ouch!, skip past end of rules, denying packet\n");
	}
	IPFW_PF_RUNLOCK(chain);
#ifdef __FreeBSD__
	if (ucred_cache != NULL)
		crfree(ucred_cache);
#endif
	return (retval);

pullup_failed:
	if (V_fw_verbose)
		printf("ipfw: pullup failed\n");
	return (IP_FW_DENY);
}

/*
 * Set maximum number of tables that can be used in given VNET ipfw instance.
 */
#ifdef SYSCTL_NODE
static int
sysctl_ipfw_table_num(SYSCTL_HANDLER_ARGS)
{
	int error;
	unsigned int ntables;

	ntables = V_fw_tables_max;

	error = sysctl_handle_int(oidp, &ntables, 0, req);
	/* Read operation or some error */
	if ((error != 0) || (req->newptr == NULL))
		return (error);

	return (ipfw_resize_tables(&V_layer3_chain, ntables));
}
#endif
/*
 * Module and VNET glue
 */

/*
 * Stuff that must be initialised only on boot or module load
 */
static int
ipfw_init(void)
{
	int error = 0;

	/*
 	 * Only print out this stuff the first time around,
	 * when called from the sysinit code.
	 */
	printf("ipfw2 "
#ifdef INET6
		"(+ipv6) "
#endif
		"initialized, divert %s, nat %s, "
		"default to %s, logging ",
#ifdef IPDIVERT
		"enabled",
#else
		"loadable",
#endif
#ifdef IPFIREWALL_NAT
		"enabled",
#else
		"loadable",
#endif
		default_to_accept ? "accept" : "deny");

	/*
	 * Note: V_xxx variables can be accessed here but the vnet specific
	 * initializer may not have been called yet for the VIMAGE case.
	 * Tuneables will have been processed. We will print out values for
	 * the default vnet. 
	 * XXX This should all be rationalized AFTER 8.0
	 */
	if (V_fw_verbose == 0)
		printf("disabled\n");
	else if (V_verbose_limit == 0)
		printf("unlimited\n");
	else
		printf("limited to %d packets/entry by default\n",
		    V_verbose_limit);

	/* Check user-supplied table count for validness */
	if (default_fw_tables > IPFW_TABLES_MAX)
	  default_fw_tables = IPFW_TABLES_MAX;

	ipfw_log_bpf(1); /* init */
	return (error);
}

/*
 * Called for the removal of the last instance only on module unload.
 */
static void
ipfw_destroy(void)
{

	ipfw_log_bpf(0); /* uninit */
	printf("IP firewall unloaded\n");
}

/*
 * Stuff that must be initialized for every instance
 * (including the first of course).
 */
static int
vnet_ipfw_init(const void *unused)
{
	int error;
	struct ip_fw *rule = NULL;
	struct ip_fw_chain *chain;

	chain = &V_layer3_chain;

	/* First set up some values that are compile time options */
	V_autoinc_step = 100;	/* bounded to 1..1000 in add_rule() */
	V_fw_deny_unknown_exthdrs = 1;
#ifdef IPFIREWALL_VERBOSE
	V_fw_verbose = 1;
#endif
#ifdef IPFIREWALL_VERBOSE_LIMIT
	V_verbose_limit = IPFIREWALL_VERBOSE_LIMIT;
#endif
#ifdef IPFIREWALL_NAT
	LIST_INIT(&chain->nat);
#endif

	/* insert the default rule and create the initial map */
	chain->n_rules = 1;
	chain->static_len = sizeof(struct ip_fw);
	chain->map = malloc(sizeof(struct ip_fw *), M_IPFW, M_WAITOK | M_ZERO);
	if (chain->map)
		rule = malloc(chain->static_len, M_IPFW, M_WAITOK | M_ZERO);

	/* Set initial number of tables */
	V_fw_tables_max = default_fw_tables;
	error = ipfw_init_tables(chain);
	if (error) {
		printf("ipfw2: setting up tables failed\n");
		free(chain->map, M_IPFW);
		free(rule, M_IPFW);
		return (ENOSPC);
	}

	/* fill and insert the default rule */
	rule->act_ofs = 0;
	rule->rulenum = IPFW_DEFAULT_RULE;
	rule->cmd_len = 1;
	rule->set = RESVD_SET;
	rule->cmd[0].len = 1;
	rule->cmd[0].opcode = default_to_accept ? O_ACCEPT : O_DENY;
	chain->rules = chain->default_rule = chain->map[0] = rule;
	chain->id = rule->id = 1;

	IPFW_LOCK_INIT(chain);
	ipfw_dyn_init(chain);

	/* First set up some values that are compile time options */
	V_ipfw_vnet_ready = 1;		/* Open for business */

	/*
	 * Hook the sockopt handler and pfil hooks for ipv4 and ipv6.
	 * Even if the latter two fail we still keep the module alive
	 * because the sockopt and layer2 paths are still useful.
	 * ipfw[6]_hook return 0 on success, ENOENT on failure,
	 * so we can ignore the exact return value and just set a flag.
	 *
	 * Note that V_fw[6]_enable are manipulated by a SYSCTL_PROC so
	 * changes in the underlying (per-vnet) variables trigger
	 * immediate hook()/unhook() calls.
	 * In layer2 we have the same behaviour, except that V_ether_ipfw
	 * is checked on each packet because there are no pfil hooks.
	 */
	V_ip_fw_ctl_ptr = ipfw_ctl;
	error = ipfw_attach_hooks(1);
	return (error);
}

/*
 * Called for the removal of each instance.
 */
static int
vnet_ipfw_uninit(const void *unused)
{
	struct ip_fw *reap, *rule;
	struct ip_fw_chain *chain = &V_layer3_chain;
	int i;

	V_ipfw_vnet_ready = 0; /* tell new callers to go away */
	/*
	 * disconnect from ipv4, ipv6, layer2 and sockopt.
	 * Then grab, release and grab again the WLOCK so we make
	 * sure the update is propagated and nobody will be in.
	 */
	(void)ipfw_attach_hooks(0 /* detach */);
	V_ip_fw_ctl_ptr = NULL;
	IPFW_UH_WLOCK(chain);
	IPFW_UH_WUNLOCK(chain);
	IPFW_UH_WLOCK(chain);

	IPFW_WLOCK(chain);
	ipfw_dyn_uninit(0);	/* run the callout_drain */
	IPFW_WUNLOCK(chain);

	ipfw_destroy_tables(chain);
	reap = NULL;
	IPFW_WLOCK(chain);
	for (i = 0; i < chain->n_rules; i++) {
		rule = chain->map[i];
		rule->x_next = reap;
		reap = rule;
	}
	if (chain->map)
		free(chain->map, M_IPFW);
	IPFW_WUNLOCK(chain);
	IPFW_UH_WUNLOCK(chain);
	if (reap != NULL)
		ipfw_reap_rules(reap);
	IPFW_LOCK_DESTROY(chain);
	ipfw_dyn_uninit(1);	/* free the remaining parts */
	return 0;
}

/*
 * Module event handler.
 * In general we have the choice of handling most of these events by the
 * event handler or by the (VNET_)SYS(UN)INIT handlers. I have chosen to
 * use the SYSINIT handlers as they are more capable of expressing the
 * flow of control during module and vnet operations, so this is just
 * a skeleton. Note there is no SYSINIT equivalent of the module
 * SHUTDOWN handler, but we don't have anything to do in that case anyhow.
 */
static int
ipfw_modevent(module_t mod, int type, void *unused)
{
	int err = 0;

	switch (type) {
	case MOD_LOAD:
		/* Called once at module load or
	 	 * system boot if compiled in. */
		break;
	case MOD_QUIESCE:
		/* Called before unload. May veto unloading. */
		break;
	case MOD_UNLOAD:
		/* Called during unload. */
		break;
	case MOD_SHUTDOWN:
		/* Called during system shutdown. */
		break;
	default:
		err = EOPNOTSUPP;
		break;
	}
	return err;
}

static moduledata_t ipfwmod = {
	"ipfw",
	ipfw_modevent,
	0
};

/* Define startup order. */
#define	IPFW_SI_SUB_FIREWALL	SI_SUB_PROTO_IFATTACHDOMAIN
#define	IPFW_MODEVENT_ORDER	(SI_ORDER_ANY - 255) /* On boot slot in here. */
#define	IPFW_MODULE_ORDER	(IPFW_MODEVENT_ORDER + 1) /* A little later. */
#define	IPFW_VNET_ORDER		(IPFW_MODEVENT_ORDER + 2) /* Later still. */

DECLARE_MODULE(ipfw, ipfwmod, IPFW_SI_SUB_FIREWALL, IPFW_MODEVENT_ORDER);
MODULE_VERSION(ipfw, 2);
/* should declare some dependencies here */

/*
 * Starting up. Done in order after ipfwmod() has been called.
 * VNET_SYSINIT is also called for each existing vnet and each new vnet.
 */
SYSINIT(ipfw_init, IPFW_SI_SUB_FIREWALL, IPFW_MODULE_ORDER,
	    ipfw_init, NULL);
VNET_SYSINIT(vnet_ipfw_init, IPFW_SI_SUB_FIREWALL, IPFW_VNET_ORDER,
	    vnet_ipfw_init, NULL);
 
/*
 * Closing up shop. These are done in REVERSE ORDER, but still
 * after ipfwmod() has been called. Not called on reboot.
 * VNET_SYSUNINIT is also called for each exiting vnet as it exits.
 * or when the module is unloaded.
 */
SYSUNINIT(ipfw_destroy, IPFW_SI_SUB_FIREWALL, IPFW_MODULE_ORDER,
	    ipfw_destroy, NULL);
VNET_SYSUNINIT(vnet_ipfw_uninit, IPFW_SI_SUB_FIREWALL, IPFW_VNET_ORDER,
	    vnet_ipfw_uninit, NULL);
/* end of file */
