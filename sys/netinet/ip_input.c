/*-
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *	@(#)ip_input.c	8.2 (Berkeley) 1/4/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bootp.h"
#include "opt_ipfw.h"
#include "opt_ipstealth.h"
#include "opt_ipsec.h"
#include "opt_route.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>

#include <net/pfil.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/vnet.h>
#include <net/flowtable.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_options.h>
#include <machine/in_cksum.h>
#include <netinet/ip_carp.h>
#ifdef IPSEC
#include <netinet/ip_ipsec.h>
#endif /* IPSEC */

#include <sys/socketvar.h>

#include <security/mac/mac_framework.h>

#ifdef CTASSERT
CTASSERT(sizeof(struct ip) == 20);
#endif

struct	rwlock in_ifaddr_lock;
RW_SYSINIT(in_ifaddr_lock, &in_ifaddr_lock, "in_ifaddr_lock");

VNET_DEFINE(int, rsvp_on);

VNET_DEFINE(int, ipforwarding);
SYSCTL_VNET_INT(_net_inet_ip, IPCTL_FORWARDING, forwarding, CTLFLAG_RW,
    &VNET_NAME(ipforwarding), 0,
    "Enable IP forwarding between interfaces");

static VNET_DEFINE(int, ipsendredirects) = 1;	/* XXX */
#define	V_ipsendredirects	VNET(ipsendredirects)
SYSCTL_VNET_INT(_net_inet_ip, IPCTL_SENDREDIRECTS, redirect, CTLFLAG_RW,
    &VNET_NAME(ipsendredirects), 0,
    "Enable sending IP redirects");

VNET_DEFINE(int, ip_defttl) = IPDEFTTL;
SYSCTL_VNET_INT(_net_inet_ip, IPCTL_DEFTTL, ttl, CTLFLAG_RW,
    &VNET_NAME(ip_defttl), 0,
    "Maximum TTL on IP packets");

static VNET_DEFINE(int, ip_keepfaith);
#define	V_ip_keepfaith		VNET(ip_keepfaith)
SYSCTL_VNET_INT(_net_inet_ip, IPCTL_KEEPFAITH, keepfaith, CTLFLAG_RW,
    &VNET_NAME(ip_keepfaith), 0,
    "Enable packet capture for FAITH IPv4->IPv6 translater daemon");

static VNET_DEFINE(int, ip_sendsourcequench);
#define	V_ip_sendsourcequench	VNET(ip_sendsourcequench)
SYSCTL_VNET_INT(_net_inet_ip, OID_AUTO, sendsourcequench, CTLFLAG_RW,
    &VNET_NAME(ip_sendsourcequench), 0,
    "Enable the transmission of source quench packets");

VNET_DEFINE(int, ip_do_randomid);
SYSCTL_VNET_INT(_net_inet_ip, OID_AUTO, random_id, CTLFLAG_RW,
    &VNET_NAME(ip_do_randomid), 0,
    "Assign random ip_id values");

/*
 * XXX - Setting ip_checkinterface mostly implements the receive side of
 * the Strong ES model described in RFC 1122, but since the routing table
 * and transmit implementation do not implement the Strong ES model,
 * setting this to 1 results in an odd hybrid.
 *
 * XXX - ip_checkinterface currently must be disabled if you use ipnat
 * to translate the destination address to another local interface.
 *
 * XXX - ip_checkinterface must be disabled if you add IP aliases
 * to the loopback interface instead of the interface where the
 * packets for those addresses are received.
 */
static VNET_DEFINE(int, ip_checkinterface);
#define	V_ip_checkinterface	VNET(ip_checkinterface)
SYSCTL_VNET_INT(_net_inet_ip, OID_AUTO, check_interface, CTLFLAG_RW,
    &VNET_NAME(ip_checkinterface), 0,
    "Verify packet arrives on correct interface");

VNET_DEFINE(struct pfil_head, inet_pfil_hook);	/* Packet filter hooks */

static struct netisr_handler ip_nh = {
	.nh_name = "ip",
	.nh_handler = ip_input,
	.nh_proto = NETISR_IP,
	.nh_policy = NETISR_POLICY_FLOW,
};

extern	struct domain inetdomain;
extern	struct protosw inetsw[];
u_char	ip_protox[IPPROTO_MAX];
VNET_DEFINE(struct in_ifaddrhead, in_ifaddrhead);  /* first inet address */
VNET_DEFINE(struct in_ifaddrhashhead *, in_ifaddrhashtbl); /* inet addr hash table  */
VNET_DEFINE(u_long, in_ifaddrhmask);		/* mask for hash table */

VNET_DEFINE(struct ipstat, ipstat);
SYSCTL_VNET_STRUCT(_net_inet_ip, IPCTL_STATS, stats, CTLFLAG_RW,
    &VNET_NAME(ipstat), ipstat,
    "IP statistics (struct ipstat, netinet/ip_var.h)");

static VNET_DEFINE(uma_zone_t, ipq_zone);
static VNET_DEFINE(TAILQ_HEAD(ipqhead, ipq), ipq[IPREASS_NHASH]);
static struct mtx ipqlock;

#define	V_ipq_zone		VNET(ipq_zone)
#define	V_ipq			VNET(ipq)

#define	IPQ_LOCK()	mtx_lock(&ipqlock)
#define	IPQ_UNLOCK()	mtx_unlock(&ipqlock)
#define	IPQ_LOCK_INIT()	mtx_init(&ipqlock, "ipqlock", NULL, MTX_DEF)
#define	IPQ_LOCK_ASSERT()	mtx_assert(&ipqlock, MA_OWNED)

static void	maxnipq_update(void);
static void	ipq_zone_change(void *);
static void	ip_drain_locked(void);

static VNET_DEFINE(int, maxnipq);  /* Administrative limit on # reass queues. */
static VNET_DEFINE(int, nipq);			/* Total # of reass queues */
#define	V_maxnipq		VNET(maxnipq)
#define	V_nipq			VNET(nipq)
SYSCTL_VNET_INT(_net_inet_ip, OID_AUTO, fragpackets, CTLFLAG_RD,
    &VNET_NAME(nipq), 0,
    "Current number of IPv4 fragment reassembly queue entries");

static VNET_DEFINE(int, maxfragsperpacket);
#define	V_maxfragsperpacket	VNET(maxfragsperpacket)
SYSCTL_VNET_INT(_net_inet_ip, OID_AUTO, maxfragsperpacket, CTLFLAG_RW,
    &VNET_NAME(maxfragsperpacket), 0,
    "Maximum number of IPv4 fragments allowed per packet");

struct callout	ipport_tick_callout;

#ifdef IPCTL_DEFMTU
SYSCTL_INT(_net_inet_ip, IPCTL_DEFMTU, mtu, CTLFLAG_RW,
    &ip_mtu, 0, "Default MTU");
#endif

#ifdef IPSTEALTH
VNET_DEFINE(int, ipstealth);
SYSCTL_VNET_INT(_net_inet_ip, OID_AUTO, stealth, CTLFLAG_RW,
    &VNET_NAME(ipstealth), 0,
    "IP stealth mode, no TTL decrementation on forwarding");
#endif

#ifdef FLOWTABLE
static VNET_DEFINE(int, ip_output_flowtable_size) = 2048;
VNET_DEFINE(struct flowtable *, ip_ft);
#define	V_ip_output_flowtable_size	VNET(ip_output_flowtable_size)

SYSCTL_VNET_INT(_net_inet_ip, OID_AUTO, output_flowtable_size, CTLFLAG_RDTUN,
    &VNET_NAME(ip_output_flowtable_size), 2048,
    "number of entries in the per-cpu output flow caches");
#endif

VNET_DEFINE(int, fw_one_pass) = 1;

static void	ip_freef(struct ipqhead *, struct ipq *);

/*
 * Kernel module interface for updating ipstat.  The argument is an index
 * into ipstat treated as an array of u_long.  While this encodes the general
 * layout of ipstat into the caller, it doesn't encode its location, so that
 * future changes to add, for example, per-CPU stats support won't cause
 * binary compatibility problems for kernel modules.
 */
void
kmod_ipstat_inc(int statnum)
{

	(*((u_long *)&V_ipstat + statnum))++;
}

void
kmod_ipstat_dec(int statnum)
{

	(*((u_long *)&V_ipstat + statnum))--;
}

static int
sysctl_netinet_intr_queue_maxlen(SYSCTL_HANDLER_ARGS)
{
	int error, qlimit;

	netisr_getqlimit(&ip_nh, &qlimit);
	error = sysctl_handle_int(oidp, &qlimit, 0, req);
	if (error || !req->newptr)
		return (error);
	if (qlimit < 1)
		return (EINVAL);
	return (netisr_setqlimit(&ip_nh, qlimit));
}
SYSCTL_PROC(_net_inet_ip, IPCTL_INTRQMAXLEN, intr_queue_maxlen,
    CTLTYPE_INT|CTLFLAG_RW, 0, 0, sysctl_netinet_intr_queue_maxlen, "I",
    "Maximum size of the IP input queue");

static int
sysctl_netinet_intr_queue_drops(SYSCTL_HANDLER_ARGS)
{
	u_int64_t qdrops_long;
	int error, qdrops;

	netisr_getqdrops(&ip_nh, &qdrops_long);
	qdrops = qdrops_long;
	error = sysctl_handle_int(oidp, &qdrops, 0, req);
	if (error || !req->newptr)
		return (error);
	if (qdrops != 0)
		return (EINVAL);
	netisr_clearqdrops(&ip_nh);
	return (0);
}

SYSCTL_PROC(_net_inet_ip, IPCTL_INTRQDROPS, intr_queue_drops,
    CTLTYPE_INT|CTLFLAG_RD, 0, 0, sysctl_netinet_intr_queue_drops, "I",
    "Number of packets dropped from the IP input queue");

/*
 * IP initialization: fill in IP protocol switch table.
 * All protocols not implemented in kernel go to raw IP protocol handler.
 */
void
ip_init(void)
{
	struct protosw *pr;
	int i;

	V_ip_id = time_second & 0xffff;

	TAILQ_INIT(&V_in_ifaddrhead);
	V_in_ifaddrhashtbl = hashinit(INADDR_NHASH, M_IFADDR, &V_in_ifaddrhmask);

	/* Initialize IP reassembly queue. */
	for (i = 0; i < IPREASS_NHASH; i++)
		TAILQ_INIT(&V_ipq[i]);
	V_maxnipq = nmbclusters / 32;
	V_maxfragsperpacket = 16;
	V_ipq_zone = uma_zcreate("ipq", sizeof(struct ipq), NULL, NULL, NULL,
	    NULL, UMA_ALIGN_PTR, 0);
	maxnipq_update();

	/* Initialize packet filter hooks. */
	V_inet_pfil_hook.ph_type = PFIL_TYPE_AF;
	V_inet_pfil_hook.ph_af = AF_INET;
	if ((i = pfil_head_register(&V_inet_pfil_hook)) != 0)
		printf("%s: WARNING: unable to register pfil hook, "
			"error %d\n", __func__, i);

#ifdef FLOWTABLE
	if (TUNABLE_INT_FETCH("net.inet.ip.output_flowtable_size",
		&V_ip_output_flowtable_size)) {
		if (V_ip_output_flowtable_size < 256)
			V_ip_output_flowtable_size = 256;
		if (!powerof2(V_ip_output_flowtable_size)) {
			printf("flowtable must be power of 2 size\n");
			V_ip_output_flowtable_size = 2048;
		}
	} else {
		/*
		 * round up to the next power of 2
		 */
		V_ip_output_flowtable_size = 1 << fls((1024 + maxusers * 64)-1);
	}
	V_ip_ft = flowtable_alloc("ipv4", V_ip_output_flowtable_size, FL_PCPU);
#endif

	/* Skip initialization of globals for non-default instances. */
	if (!IS_DEFAULT_VNET(curvnet))
		return;

	pr = pffindproto(PF_INET, IPPROTO_RAW, SOCK_RAW);
	if (pr == NULL)
		panic("ip_init: PF_INET not found");

	/* Initialize the entire ip_protox[] array to IPPROTO_RAW. */
	for (i = 0; i < IPPROTO_MAX; i++)
		ip_protox[i] = pr - inetsw;
	/*
	 * Cycle through IP protocols and put them into the appropriate place
	 * in ip_protox[].
	 */
	for (pr = inetdomain.dom_protosw;
	    pr < inetdomain.dom_protoswNPROTOSW; pr++)
		if (pr->pr_domain->dom_family == PF_INET &&
		    pr->pr_protocol && pr->pr_protocol != IPPROTO_RAW) {
			/* Be careful to only index valid IP protocols. */
			if (pr->pr_protocol < IPPROTO_MAX)
				ip_protox[pr->pr_protocol] = pr - inetsw;
		}

	/* Start ipport_tick. */
	callout_init(&ipport_tick_callout, CALLOUT_MPSAFE);
	callout_reset(&ipport_tick_callout, 1, ipport_tick, NULL);
	EVENTHANDLER_REGISTER(shutdown_pre_sync, ip_fini, NULL,
		SHUTDOWN_PRI_DEFAULT);
	EVENTHANDLER_REGISTER(nmbclusters_change, ipq_zone_change,
		NULL, EVENTHANDLER_PRI_ANY);

	/* Initialize various other remaining things. */
	IPQ_LOCK_INIT();
	netisr_register(&ip_nh);
}

#ifdef VIMAGE
void
ip_destroy(void)
{

	/* Cleanup in_ifaddr hash table; should be empty. */
	hashdestroy(V_in_ifaddrhashtbl, M_IFADDR, V_in_ifaddrhmask);

	IPQ_LOCK();
	ip_drain_locked();
	IPQ_UNLOCK();

	uma_zdestroy(V_ipq_zone);
}
#endif

void
ip_fini(void *xtp)
{

	callout_stop(&ipport_tick_callout);
}

/*
 * Ip input routine.  Checksum and byte swap header.  If fragmented
 * try to reassemble.  Process options.  Pass to next level.
 */
void
ip_input(struct mbuf *m)
{
	struct ip *ip = NULL;
	struct in_ifaddr *ia = NULL;
	struct ifaddr *ifa;
	struct ifnet *ifp;
	int    checkif, hlen = 0;
	u_short sum;
	int dchg = 0;				/* dest changed after fw */
	struct in_addr odst;			/* original dst address */

	M_ASSERTPKTHDR(m);

	if (m->m_flags & M_FASTFWD_OURS) {
		/*
		 * Firewall or NAT changed destination to local.
		 * We expect ip_len and ip_off to be in host byte order.
		 */
		m->m_flags &= ~M_FASTFWD_OURS;
		/* Set up some basics that will be used later. */
		ip = mtod(m, struct ip *);
		hlen = ip->ip_hl << 2;
		goto ours;
	}

	IPSTAT_INC(ips_total);

	if (m->m_pkthdr.len < sizeof(struct ip))
		goto tooshort;

	if (m->m_len < sizeof (struct ip) &&
	    (m = m_pullup(m, sizeof (struct ip))) == NULL) {
		IPSTAT_INC(ips_toosmall);
		return;
	}
	ip = mtod(m, struct ip *);

	if (ip->ip_v != IPVERSION) {
		IPSTAT_INC(ips_badvers);
		goto bad;
	}

	hlen = ip->ip_hl << 2;
	if (hlen < sizeof(struct ip)) {	/* minimum header length */
		IPSTAT_INC(ips_badhlen);
		goto bad;
	}
	if (hlen > m->m_len) {
		if ((m = m_pullup(m, hlen)) == NULL) {
			IPSTAT_INC(ips_badhlen);
			return;
		}
		ip = mtod(m, struct ip *);
	}

	/* 127/8 must not appear on wire - RFC1122 */
	ifp = m->m_pkthdr.rcvif;
	if ((ntohl(ip->ip_dst.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET ||
	    (ntohl(ip->ip_src.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET) {
		if ((ifp->if_flags & IFF_LOOPBACK) == 0) {
			IPSTAT_INC(ips_badaddr);
			goto bad;
		}
	}

	if (m->m_pkthdr.csum_flags & CSUM_IP_CHECKED) {
		sum = !(m->m_pkthdr.csum_flags & CSUM_IP_VALID);
	} else {
		if (hlen == sizeof(struct ip)) {
			sum = in_cksum_hdr(ip);
		} else {
			sum = in_cksum(m, hlen);
		}
	}
	if (sum) {
		IPSTAT_INC(ips_badsum);
		goto bad;
	}

#ifdef ALTQ
	if (altq_input != NULL && (*altq_input)(m, AF_INET) == 0)
		/* packet is dropped by traffic conditioner */
		return;
#endif

	/*
	 * Convert fields to host representation.
	 */
	ip->ip_len = ntohs(ip->ip_len);
	if (ip->ip_len < hlen) {
		IPSTAT_INC(ips_badlen);
		goto bad;
	}
	ip->ip_off = ntohs(ip->ip_off);

	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IP header would have us expect.
	 * Trim mbufs if longer than we expect.
	 * Drop packet if shorter than we expect.
	 */
	if (m->m_pkthdr.len < ip->ip_len) {
tooshort:
		IPSTAT_INC(ips_tooshort);
		goto bad;
	}
	if (m->m_pkthdr.len > ip->ip_len) {
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = ip->ip_len;
			m->m_pkthdr.len = ip->ip_len;
		} else
			m_adj(m, ip->ip_len - m->m_pkthdr.len);
	}
#ifdef IPSEC
	/*
	 * Bypass packet filtering for packets from a tunnel (gif).
	 */
	if (ip_ipsec_filtertunnel(m))
		goto passin;
#endif /* IPSEC */

	/*
	 * Run through list of hooks for input packets.
	 *
	 * NB: Beware of the destination address changing (e.g.
	 *     by NAT rewriting).  When this happens, tell
	 *     ip_forward to do the right thing.
	 */

	/* Jump over all PFIL processing if hooks are not active. */
	if (!PFIL_HOOKED(&V_inet_pfil_hook))
		goto passin;

	odst = ip->ip_dst;
	if (pfil_run_hooks(&V_inet_pfil_hook, &m, ifp, PFIL_IN, NULL) != 0)
		return;
	if (m == NULL)			/* consumed by filter */
		return;

	ip = mtod(m, struct ip *);
	dchg = (odst.s_addr != ip->ip_dst.s_addr);
	ifp = m->m_pkthdr.rcvif;

#ifdef IPFIREWALL_FORWARD
	if (m->m_flags & M_FASTFWD_OURS) {
		m->m_flags &= ~M_FASTFWD_OURS;
		goto ours;
	}
	if ((dchg = (m_tag_find(m, PACKET_TAG_IPFORWARD, NULL) != NULL)) != 0) {
		/*
		 * Directly ship the packet on.  This allows forwarding
		 * packets originally destined to us to some other directly
		 * connected host.
		 */
		ip_forward(m, dchg);
		return;
	}
#endif /* IPFIREWALL_FORWARD */

passin:
	/*
	 * Process options and, if not destined for us,
	 * ship it on.  ip_dooptions returns 1 when an
	 * error was detected (causing an icmp message
	 * to be sent and the original packet to be freed).
	 */
	if (hlen > sizeof (struct ip) && ip_dooptions(m, 0))
		return;

        /* greedy RSVP, snatches any PATH packet of the RSVP protocol and no
         * matter if it is destined to another node, or whether it is 
         * a multicast one, RSVP wants it! and prevents it from being forwarded
         * anywhere else. Also checks if the rsvp daemon is running before
	 * grabbing the packet.
         */
	if (V_rsvp_on && ip->ip_p==IPPROTO_RSVP) 
		goto ours;

	/*
	 * Check our list of addresses, to see if the packet is for us.
	 * If we don't have any addresses, assume any unicast packet
	 * we receive might be for us (and let the upper layers deal
	 * with it).
	 */
	if (TAILQ_EMPTY(&V_in_ifaddrhead) &&
	    (m->m_flags & (M_MCAST|M_BCAST)) == 0)
		goto ours;

	/*
	 * Enable a consistency check between the destination address
	 * and the arrival interface for a unicast packet (the RFC 1122
	 * strong ES model) if IP forwarding is disabled and the packet
	 * is not locally generated and the packet is not subject to
	 * 'ipfw fwd'.
	 *
	 * XXX - Checking also should be disabled if the destination
	 * address is ipnat'ed to a different interface.
	 *
	 * XXX - Checking is incompatible with IP aliases added
	 * to the loopback interface instead of the interface where
	 * the packets are received.
	 *
	 * XXX - This is the case for carp vhost IPs as well so we
	 * insert a workaround. If the packet got here, we already
	 * checked with carp_iamatch() and carp_forus().
	 */
	checkif = V_ip_checkinterface && (V_ipforwarding == 0) && 
	    ifp != NULL && ((ifp->if_flags & IFF_LOOPBACK) == 0) &&
	    ifp->if_carp == NULL && (dchg == 0);

	/*
	 * Check for exact addresses in the hash bucket.
	 */
	/* IN_IFADDR_RLOCK(); */
	LIST_FOREACH(ia, INADDR_HASH(ip->ip_dst.s_addr), ia_hash) {
		/*
		 * If the address matches, verify that the packet
		 * arrived via the correct interface if checking is
		 * enabled.
		 */
		if (IA_SIN(ia)->sin_addr.s_addr == ip->ip_dst.s_addr && 
		    (!checkif || ia->ia_ifp == ifp)) {
			ifa_ref(&ia->ia_ifa);
			/* IN_IFADDR_RUNLOCK(); */
			goto ours;
		}
	}
	/* IN_IFADDR_RUNLOCK(); */

	/*
	 * Check for broadcast addresses.
	 *
	 * Only accept broadcast packets that arrive via the matching
	 * interface.  Reception of forwarded directed broadcasts would
	 * be handled via ip_forward() and ether_output() with the loopback
	 * into the stack for SIMPLEX interfaces handled by ether_output().
	 */
	if (ifp != NULL && ifp->if_flags & IFF_BROADCAST) {
		IF_ADDR_LOCK(ifp);
	        TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
			ia = ifatoia(ifa);
			if (satosin(&ia->ia_broadaddr)->sin_addr.s_addr ==
			    ip->ip_dst.s_addr) {
				ifa_ref(ifa);
				IF_ADDR_UNLOCK(ifp);
				goto ours;
			}
			if (ia->ia_netbroadcast.s_addr == ip->ip_dst.s_addr) {
				ifa_ref(ifa);
				IF_ADDR_UNLOCK(ifp);
				goto ours;
			}
#ifdef BOOTP_COMPAT
			if (IA_SIN(ia)->sin_addr.s_addr == INADDR_ANY) {
				ifa_ref(ifa);
				IF_ADDR_UNLOCK(ifp);
				goto ours;
			}
#endif
		}
		IF_ADDR_UNLOCK(ifp);
		ia = NULL;
	}
	/* RFC 3927 2.7: Do not forward datagrams for 169.254.0.0/16. */
	if (IN_LINKLOCAL(ntohl(ip->ip_dst.s_addr))) {
		IPSTAT_INC(ips_cantforward);
		m_freem(m);
		return;
	}
	if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr))) {
		if (V_ip_mrouter) {
			/*
			 * If we are acting as a multicast router, all
			 * incoming multicast packets are passed to the
			 * kernel-level multicast forwarding function.
			 * The packet is returned (relatively) intact; if
			 * ip_mforward() returns a non-zero value, the packet
			 * must be discarded, else it may be accepted below.
			 */
			if (ip_mforward && ip_mforward(ip, ifp, m, 0) != 0) {
				IPSTAT_INC(ips_cantforward);
				m_freem(m);
				return;
			}

			/*
			 * The process-level routing daemon needs to receive
			 * all multicast IGMP packets, whether or not this
			 * host belongs to their destination groups.
			 */
			if (ip->ip_p == IPPROTO_IGMP)
				goto ours;
			IPSTAT_INC(ips_forward);
		}
		/*
		 * Assume the packet is for us, to avoid prematurely taking
		 * a lock on the in_multi hash. Protocols must perform
		 * their own filtering and update statistics accordingly.
		 */
		goto ours;
	}
	if (ip->ip_dst.s_addr == (u_long)INADDR_BROADCAST)
		goto ours;
	if (ip->ip_dst.s_addr == INADDR_ANY)
		goto ours;

	/*
	 * FAITH(Firewall Aided Internet Translator)
	 */
	if (ifp && ifp->if_type == IFT_FAITH) {
		if (V_ip_keepfaith) {
			if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_ICMP) 
				goto ours;
		}
		m_freem(m);
		return;
	}

	/*
	 * Not for us; forward if possible and desirable.
	 */
	if (V_ipforwarding == 0) {
		IPSTAT_INC(ips_cantforward);
		m_freem(m);
	} else {
#ifdef IPSEC
		if (ip_ipsec_fwd(m))
			goto bad;
#endif /* IPSEC */
		ip_forward(m, dchg);
	}
	return;

ours:
#ifdef IPSTEALTH
	/*
	 * IPSTEALTH: Process non-routing options only
	 * if the packet is destined for us.
	 */
	if (V_ipstealth && hlen > sizeof (struct ip) && ip_dooptions(m, 1)) {
		if (ia != NULL)
			ifa_free(&ia->ia_ifa);
		return;
	}
#endif /* IPSTEALTH */

	/* Count the packet in the ip address stats */
	if (ia != NULL) {
		ia->ia_ifa.if_ipackets++;
		ia->ia_ifa.if_ibytes += m->m_pkthdr.len;
		ifa_free(&ia->ia_ifa);
	}

	/*
	 * Attempt reassembly; if it succeeds, proceed.
	 * ip_reass() will return a different mbuf.
	 */
	if (ip->ip_off & (IP_MF | IP_OFFMASK)) {
		m = ip_reass(m);
		if (m == NULL)
			return;
		ip = mtod(m, struct ip *);
		/* Get the header length of the reassembled packet */
		hlen = ip->ip_hl << 2;
	}

	/*
	 * Further protocols expect the packet length to be w/o the
	 * IP header.
	 */
	ip->ip_len -= hlen;

#ifdef IPSEC
	/*
	 * enforce IPsec policy checking if we are seeing last header.
	 * note that we do not visit this with protocols with pcb layer
	 * code - like udp/tcp/raw ip.
	 */
	if (ip_ipsec_input(m))
		goto bad;
#endif /* IPSEC */

	/*
	 * Switch out to protocol's input routine.
	 */
	IPSTAT_INC(ips_delivered);

	(*inetsw[ip_protox[ip->ip_p]].pr_input)(m, hlen);
	return;
bad:
	m_freem(m);
}

/*
 * After maxnipq has been updated, propagate the change to UMA.  The UMA zone
 * max has slightly different semantics than the sysctl, for historical
 * reasons.
 */
static void
maxnipq_update(void)
{

	/*
	 * -1 for unlimited allocation.
	 */
	if (V_maxnipq < 0)
		uma_zone_set_max(V_ipq_zone, 0);
	/*
	 * Positive number for specific bound.
	 */
	if (V_maxnipq > 0)
		uma_zone_set_max(V_ipq_zone, V_maxnipq);
	/*
	 * Zero specifies no further fragment queue allocation -- set the
	 * bound very low, but rely on implementation elsewhere to actually
	 * prevent allocation and reclaim current queues.
	 */
	if (V_maxnipq == 0)
		uma_zone_set_max(V_ipq_zone, 1);
}

static void
ipq_zone_change(void *tag)
{

	if (V_maxnipq > 0 && V_maxnipq < (nmbclusters / 32)) {
		V_maxnipq = nmbclusters / 32;
		maxnipq_update();
	}
}

static int
sysctl_maxnipq(SYSCTL_HANDLER_ARGS)
{
	int error, i;

	i = V_maxnipq;
	error = sysctl_handle_int(oidp, &i, 0, req);
	if (error || !req->newptr)
		return (error);

	/*
	 * XXXRW: Might be a good idea to sanity check the argument and place
	 * an extreme upper bound.
	 */
	if (i < -1)
		return (EINVAL);
	V_maxnipq = i;
	maxnipq_update();
	return (0);
}

SYSCTL_PROC(_net_inet_ip, OID_AUTO, maxfragpackets, CTLTYPE_INT|CTLFLAG_RW,
    NULL, 0, sysctl_maxnipq, "I",
    "Maximum number of IPv4 fragment reassembly queue entries");

/*
 * Take incoming datagram fragment and try to reassemble it into
 * whole datagram.  If the argument is the first fragment or one
 * in between the function will return NULL and store the mbuf
 * in the fragment chain.  If the argument is the last fragment
 * the packet will be reassembled and the pointer to the new
 * mbuf returned for further processing.  Only m_tags attached
 * to the first packet/fragment are preserved.
 * The IP header is *NOT* adjusted out of iplen.
 */
struct mbuf *
ip_reass(struct mbuf *m)
{
	struct ip *ip;
	struct mbuf *p, *q, *nq, *t;
	struct ipq *fp = NULL;
	struct ipqhead *head;
	int i, hlen, next;
	u_int8_t ecn, ecn0;
	u_short hash;

	/* If maxnipq or maxfragsperpacket are 0, never accept fragments. */
	if (V_maxnipq == 0 || V_maxfragsperpacket == 0) {
		IPSTAT_INC(ips_fragments);
		IPSTAT_INC(ips_fragdropped);
		m_freem(m);
		return (NULL);
	}

	ip = mtod(m, struct ip *);
	hlen = ip->ip_hl << 2;

	hash = IPREASS_HASH(ip->ip_src.s_addr, ip->ip_id);
	head = &V_ipq[hash];
	IPQ_LOCK();

	/*
	 * Look for queue of fragments
	 * of this datagram.
	 */
	TAILQ_FOREACH(fp, head, ipq_list)
		if (ip->ip_id == fp->ipq_id &&
		    ip->ip_src.s_addr == fp->ipq_src.s_addr &&
		    ip->ip_dst.s_addr == fp->ipq_dst.s_addr &&
#ifdef MAC
		    mac_ipq_match(m, fp) &&
#endif
		    ip->ip_p == fp->ipq_p)
			goto found;

	fp = NULL;

	/*
	 * Attempt to trim the number of allocated fragment queues if it
	 * exceeds the administrative limit.
	 */
	if ((V_nipq > V_maxnipq) && (V_maxnipq > 0)) {
		/*
		 * drop something from the tail of the current queue
		 * before proceeding further
		 */
		struct ipq *q = TAILQ_LAST(head, ipqhead);
		if (q == NULL) {   /* gak */
			for (i = 0; i < IPREASS_NHASH; i++) {
				struct ipq *r = TAILQ_LAST(&V_ipq[i], ipqhead);
				if (r) {
					IPSTAT_ADD(ips_fragtimeout,
					    r->ipq_nfrags);
					ip_freef(&V_ipq[i], r);
					break;
				}
			}
		} else {
			IPSTAT_ADD(ips_fragtimeout, q->ipq_nfrags);
			ip_freef(head, q);
		}
	}

found:
	/*
	 * Adjust ip_len to not reflect header,
	 * convert offset of this to bytes.
	 */
	ip->ip_len -= hlen;
	if (ip->ip_off & IP_MF) {
		/*
		 * Make sure that fragments have a data length
		 * that's a non-zero multiple of 8 bytes.
		 */
		if (ip->ip_len == 0 || (ip->ip_len & 0x7) != 0) {
			IPSTAT_INC(ips_toosmall); /* XXX */
			goto dropfrag;
		}
		m->m_flags |= M_FRAG;
	} else
		m->m_flags &= ~M_FRAG;
	ip->ip_off <<= 3;


	/*
	 * Attempt reassembly; if it succeeds, proceed.
	 * ip_reass() will return a different mbuf.
	 */
	IPSTAT_INC(ips_fragments);
	m->m_pkthdr.header = ip;

	/* Previous ip_reass() started here. */
	/*
	 * Presence of header sizes in mbufs
	 * would confuse code below.
	 */
	m->m_data += hlen;
	m->m_len -= hlen;

	/*
	 * If first fragment to arrive, create a reassembly queue.
	 */
	if (fp == NULL) {
		fp = uma_zalloc(V_ipq_zone, M_NOWAIT);
		if (fp == NULL)
			goto dropfrag;
#ifdef MAC
		if (mac_ipq_init(fp, M_NOWAIT) != 0) {
			uma_zfree(V_ipq_zone, fp);
			fp = NULL;
			goto dropfrag;
		}
		mac_ipq_create(m, fp);
#endif
		TAILQ_INSERT_HEAD(head, fp, ipq_list);
		V_nipq++;
		fp->ipq_nfrags = 1;
		fp->ipq_ttl = IPFRAGTTL;
		fp->ipq_p = ip->ip_p;
		fp->ipq_id = ip->ip_id;
		fp->ipq_src = ip->ip_src;
		fp->ipq_dst = ip->ip_dst;
		fp->ipq_frags = m;
		m->m_nextpkt = NULL;
		goto done;
	} else {
		fp->ipq_nfrags++;
#ifdef MAC
		mac_ipq_update(m, fp);
#endif
	}

#define GETIP(m)	((struct ip*)((m)->m_pkthdr.header))

	/*
	 * Handle ECN by comparing this segment with the first one;
	 * if CE is set, do not lose CE.
	 * drop if CE and not-ECT are mixed for the same packet.
	 */
	ecn = ip->ip_tos & IPTOS_ECN_MASK;
	ecn0 = GETIP(fp->ipq_frags)->ip_tos & IPTOS_ECN_MASK;
	if (ecn == IPTOS_ECN_CE) {
		if (ecn0 == IPTOS_ECN_NOTECT)
			goto dropfrag;
		if (ecn0 != IPTOS_ECN_CE)
			GETIP(fp->ipq_frags)->ip_tos |= IPTOS_ECN_CE;
	}
	if (ecn == IPTOS_ECN_NOTECT && ecn0 != IPTOS_ECN_NOTECT)
		goto dropfrag;

	/*
	 * Find a segment which begins after this one does.
	 */
	for (p = NULL, q = fp->ipq_frags; q; p = q, q = q->m_nextpkt)
		if (GETIP(q)->ip_off > ip->ip_off)
			break;

	/*
	 * If there is a preceding segment, it may provide some of
	 * our data already.  If so, drop the data from the incoming
	 * segment.  If it provides all of our data, drop us, otherwise
	 * stick new segment in the proper place.
	 *
	 * If some of the data is dropped from the the preceding
	 * segment, then it's checksum is invalidated.
	 */
	if (p) {
		i = GETIP(p)->ip_off + GETIP(p)->ip_len - ip->ip_off;
		if (i > 0) {
			if (i >= ip->ip_len)
				goto dropfrag;
			m_adj(m, i);
			m->m_pkthdr.csum_flags = 0;
			ip->ip_off += i;
			ip->ip_len -= i;
		}
		m->m_nextpkt = p->m_nextpkt;
		p->m_nextpkt = m;
	} else {
		m->m_nextpkt = fp->ipq_frags;
		fp->ipq_frags = m;
	}

	/*
	 * While we overlap succeeding segments trim them or,
	 * if they are completely covered, dequeue them.
	 */
	for (; q != NULL && ip->ip_off + ip->ip_len > GETIP(q)->ip_off;
	     q = nq) {
		i = (ip->ip_off + ip->ip_len) - GETIP(q)->ip_off;
		if (i < GETIP(q)->ip_len) {
			GETIP(q)->ip_len -= i;
			GETIP(q)->ip_off += i;
			m_adj(q, i);
			q->m_pkthdr.csum_flags = 0;
			break;
		}
		nq = q->m_nextpkt;
		m->m_nextpkt = nq;
		IPSTAT_INC(ips_fragdropped);
		fp->ipq_nfrags--;
		m_freem(q);
	}

	/*
	 * Check for complete reassembly and perform frag per packet
	 * limiting.
	 *
	 * Frag limiting is performed here so that the nth frag has
	 * a chance to complete the packet before we drop the packet.
	 * As a result, n+1 frags are actually allowed per packet, but
	 * only n will ever be stored. (n = maxfragsperpacket.)
	 *
	 */
	next = 0;
	for (p = NULL, q = fp->ipq_frags; q; p = q, q = q->m_nextpkt) {
		if (GETIP(q)->ip_off != next) {
			if (fp->ipq_nfrags > V_maxfragsperpacket) {
				IPSTAT_ADD(ips_fragdropped, fp->ipq_nfrags);
				ip_freef(head, fp);
			}
			goto done;
		}
		next += GETIP(q)->ip_len;
	}
	/* Make sure the last packet didn't have the IP_MF flag */
	if (p->m_flags & M_FRAG) {
		if (fp->ipq_nfrags > V_maxfragsperpacket) {
			IPSTAT_ADD(ips_fragdropped, fp->ipq_nfrags);
			ip_freef(head, fp);
		}
		goto done;
	}

	/*
	 * Reassembly is complete.  Make sure the packet is a sane size.
	 */
	q = fp->ipq_frags;
	ip = GETIP(q);
	if (next + (ip->ip_hl << 2) > IP_MAXPACKET) {
		IPSTAT_INC(ips_toolong);
		IPSTAT_ADD(ips_fragdropped, fp->ipq_nfrags);
		ip_freef(head, fp);
		goto done;
	}

	/*
	 * Concatenate fragments.
	 */
	m = q;
	t = m->m_next;
	m->m_next = NULL;
	m_cat(m, t);
	nq = q->m_nextpkt;
	q->m_nextpkt = NULL;
	for (q = nq; q != NULL; q = nq) {
		nq = q->m_nextpkt;
		q->m_nextpkt = NULL;
		m->m_pkthdr.csum_flags &= q->m_pkthdr.csum_flags;
		m->m_pkthdr.csum_data += q->m_pkthdr.csum_data;
		m_cat(m, q);
	}
	/*
	 * In order to do checksumming faster we do 'end-around carry' here
	 * (and not in for{} loop), though it implies we are not going to
	 * reassemble more than 64k fragments.
	 */
	m->m_pkthdr.csum_data =
	    (m->m_pkthdr.csum_data & 0xffff) + (m->m_pkthdr.csum_data >> 16);
#ifdef MAC
	mac_ipq_reassemble(fp, m);
	mac_ipq_destroy(fp);
#endif

	/*
	 * Create header for new ip packet by modifying header of first
	 * packet;  dequeue and discard fragment reassembly header.
	 * Make header visible.
	 */
	ip->ip_len = (ip->ip_hl << 2) + next;
	ip->ip_src = fp->ipq_src;
	ip->ip_dst = fp->ipq_dst;
	TAILQ_REMOVE(head, fp, ipq_list);
	V_nipq--;
	uma_zfree(V_ipq_zone, fp);
	m->m_len += (ip->ip_hl << 2);
	m->m_data -= (ip->ip_hl << 2);
	/* some debugging cruft by sklower, below, will go away soon */
	if (m->m_flags & M_PKTHDR)	/* XXX this should be done elsewhere */
		m_fixhdr(m);
	IPSTAT_INC(ips_reassembled);
	IPQ_UNLOCK();
	return (m);

dropfrag:
	IPSTAT_INC(ips_fragdropped);
	if (fp != NULL)
		fp->ipq_nfrags--;
	m_freem(m);
done:
	IPQ_UNLOCK();
	return (NULL);

#undef GETIP
}

/*
 * Free a fragment reassembly header and all
 * associated datagrams.
 */
static void
ip_freef(struct ipqhead *fhp, struct ipq *fp)
{
	struct mbuf *q;

	IPQ_LOCK_ASSERT();

	while (fp->ipq_frags) {
		q = fp->ipq_frags;
		fp->ipq_frags = q->m_nextpkt;
		m_freem(q);
	}
	TAILQ_REMOVE(fhp, fp, ipq_list);
	uma_zfree(V_ipq_zone, fp);
	V_nipq--;
}

/*
 * IP timer processing;
 * if a timer expires on a reassembly
 * queue, discard it.
 */
void
ip_slowtimo(void)
{
	VNET_ITERATOR_DECL(vnet_iter);
	struct ipq *fp;
	int i;

	VNET_LIST_RLOCK_NOSLEEP();
	IPQ_LOCK();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		for (i = 0; i < IPREASS_NHASH; i++) {
			for(fp = TAILQ_FIRST(&V_ipq[i]); fp;) {
				struct ipq *fpp;

				fpp = fp;
				fp = TAILQ_NEXT(fp, ipq_list);
				if(--fpp->ipq_ttl == 0) {
					IPSTAT_ADD(ips_fragtimeout,
					    fpp->ipq_nfrags);
					ip_freef(&V_ipq[i], fpp);
				}
			}
		}
		/*
		 * If we are over the maximum number of fragments
		 * (due to the limit being lowered), drain off
		 * enough to get down to the new limit.
		 */
		if (V_maxnipq >= 0 && V_nipq > V_maxnipq) {
			for (i = 0; i < IPREASS_NHASH; i++) {
				while (V_nipq > V_maxnipq &&
				    !TAILQ_EMPTY(&V_ipq[i])) {
					IPSTAT_ADD(ips_fragdropped,
					    TAILQ_FIRST(&V_ipq[i])->ipq_nfrags);
					ip_freef(&V_ipq[i],
					    TAILQ_FIRST(&V_ipq[i]));
				}
			}
		}
		CURVNET_RESTORE();
	}
	IPQ_UNLOCK();
	VNET_LIST_RUNLOCK_NOSLEEP();
}

/*
 * Drain off all datagram fragments.
 */
static void
ip_drain_locked(void)
{
	int     i;

	IPQ_LOCK_ASSERT();

	for (i = 0; i < IPREASS_NHASH; i++) {
		while(!TAILQ_EMPTY(&V_ipq[i])) {
			IPSTAT_ADD(ips_fragdropped,
			    TAILQ_FIRST(&V_ipq[i])->ipq_nfrags);
			ip_freef(&V_ipq[i], TAILQ_FIRST(&V_ipq[i]));
		}
	}
}

void
ip_drain(void)
{
	VNET_ITERATOR_DECL(vnet_iter);

	VNET_LIST_RLOCK_NOSLEEP();
	IPQ_LOCK();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		ip_drain_locked();
		CURVNET_RESTORE();
	}
	IPQ_UNLOCK();
	VNET_LIST_RUNLOCK_NOSLEEP();
	in_rtqdrain();
}

/*
 * The protocol to be inserted into ip_protox[] must be already registered
 * in inetsw[], either statically or through pf_proto_register().
 */
int
ipproto_register(short ipproto)
{
	struct protosw *pr;

	/* Sanity checks. */
	if (ipproto <= 0 || ipproto >= IPPROTO_MAX)
		return (EPROTONOSUPPORT);

	/*
	 * The protocol slot must not be occupied by another protocol
	 * already.  An index pointing to IPPROTO_RAW is unused.
	 */
	pr = pffindproto(PF_INET, IPPROTO_RAW, SOCK_RAW);
	if (pr == NULL)
		return (EPFNOSUPPORT);
	if (ip_protox[ipproto] != pr - inetsw)	/* IPPROTO_RAW */
		return (EEXIST);

	/* Find the protocol position in inetsw[] and set the index. */
	for (pr = inetdomain.dom_protosw;
	     pr < inetdomain.dom_protoswNPROTOSW; pr++) {
		if (pr->pr_domain->dom_family == PF_INET &&
		    pr->pr_protocol && pr->pr_protocol == ipproto) {
			ip_protox[pr->pr_protocol] = pr - inetsw;
			return (0);
		}
	}
	return (EPROTONOSUPPORT);
}

int
ipproto_unregister(short ipproto)
{
	struct protosw *pr;

	/* Sanity checks. */
	if (ipproto <= 0 || ipproto >= IPPROTO_MAX)
		return (EPROTONOSUPPORT);

	/* Check if the protocol was indeed registered. */
	pr = pffindproto(PF_INET, IPPROTO_RAW, SOCK_RAW);
	if (pr == NULL)
		return (EPFNOSUPPORT);
	if (ip_protox[ipproto] == pr - inetsw)  /* IPPROTO_RAW */
		return (ENOENT);

	/* Reset the protocol slot to IPPROTO_RAW. */
	ip_protox[ipproto] = pr - inetsw;
	return (0);
}

/*
 * Given address of next destination (final or next hop), return (referenced)
 * internet address info of interface to be used to get there.
 */
struct in_ifaddr *
ip_rtaddr(struct in_addr dst, u_int fibnum)
{
	struct route sro;
	struct sockaddr_in *sin;
	struct in_ifaddr *ia;

	bzero(&sro, sizeof(sro));
	sin = (struct sockaddr_in *)&sro.ro_dst;
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	sin->sin_addr = dst;
	in_rtalloc_ign(&sro, 0, fibnum);

	if (sro.ro_rt == NULL)
		return (NULL);

	ia = ifatoia(sro.ro_rt->rt_ifa);
	ifa_ref(&ia->ia_ifa);
	RTFREE(sro.ro_rt);
	return (ia);
}

u_char inetctlerrmap[PRC_NCMDS] = {
	0,		0,		0,		0,
	0,		EMSGSIZE,	EHOSTDOWN,	EHOSTUNREACH,
	EHOSTUNREACH,	EHOSTUNREACH,	ECONNREFUSED,	ECONNREFUSED,
	EMSGSIZE,	EHOSTUNREACH,	0,		0,
	0,		0,		EHOSTUNREACH,	0,
	ENOPROTOOPT,	ECONNREFUSED
};

/*
 * Forward a packet.  If some error occurs return the sender
 * an icmp packet.  Note we can't always generate a meaningful
 * icmp message because icmp doesn't have a large enough repertoire
 * of codes and types.
 *
 * If not forwarding, just drop the packet.  This could be confusing
 * if ipforwarding was zero but some routing protocol was advancing
 * us as a gateway to somewhere.  However, we must let the routing
 * protocol deal with that.
 *
 * The srcrt parameter indicates whether the packet is being forwarded
 * via a source route.
 */
void
ip_forward(struct mbuf *m, int srcrt)
{
	struct ip *ip = mtod(m, struct ip *);
	struct in_ifaddr *ia;
	struct mbuf *mcopy;
	struct in_addr dest;
	struct route ro;
	int error, type = 0, code = 0, mtu = 0;

	if (m->m_flags & (M_BCAST|M_MCAST) || in_canforward(ip->ip_dst) == 0) {
		IPSTAT_INC(ips_cantforward);
		m_freem(m);
		return;
	}
#ifdef IPSTEALTH
	if (!V_ipstealth) {
#endif
		if (ip->ip_ttl <= IPTTLDEC) {
			icmp_error(m, ICMP_TIMXCEED, ICMP_TIMXCEED_INTRANS,
			    0, 0);
			return;
		}
#ifdef IPSTEALTH
	}
#endif

	ia = ip_rtaddr(ip->ip_dst, M_GETFIB(m));
#ifndef IPSEC
	/*
	 * 'ia' may be NULL if there is no route for this destination.
	 * In case of IPsec, Don't discard it just yet, but pass it to
	 * ip_output in case of outgoing IPsec policy.
	 */
	if (!srcrt && ia == NULL) {
		icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_HOST, 0, 0);
		return;
	}
#endif

	/*
	 * Save the IP header and at most 8 bytes of the payload,
	 * in case we need to generate an ICMP message to the src.
	 *
	 * XXX this can be optimized a lot by saving the data in a local
	 * buffer on the stack (72 bytes at most), and only allocating the
	 * mbuf if really necessary. The vast majority of the packets
	 * are forwarded without having to send an ICMP back (either
	 * because unnecessary, or because rate limited), so we are
	 * really we are wasting a lot of work here.
	 *
	 * We don't use m_copy() because it might return a reference
	 * to a shared cluster. Both this function and ip_output()
	 * assume exclusive access to the IP header in `m', so any
	 * data in a cluster may change before we reach icmp_error().
	 */
	MGETHDR(mcopy, M_DONTWAIT, m->m_type);
	if (mcopy != NULL && !m_dup_pkthdr(mcopy, m, M_DONTWAIT)) {
		/*
		 * It's probably ok if the pkthdr dup fails (because
		 * the deep copy of the tag chain failed), but for now
		 * be conservative and just discard the copy since
		 * code below may some day want the tags.
		 */
		m_free(mcopy);
		mcopy = NULL;
	}
	if (mcopy != NULL) {
		mcopy->m_len = min(ip->ip_len, M_TRAILINGSPACE(mcopy));
		mcopy->m_pkthdr.len = mcopy->m_len;
		m_copydata(m, 0, mcopy->m_len, mtod(mcopy, caddr_t));
	}

#ifdef IPSTEALTH
	if (!V_ipstealth) {
#endif
		ip->ip_ttl -= IPTTLDEC;
#ifdef IPSTEALTH
	}
#endif

	/*
	 * If forwarding packet using same interface that it came in on,
	 * perhaps should send a redirect to sender to shortcut a hop.
	 * Only send redirect if source is sending directly to us,
	 * and if packet was not source routed (or has any options).
	 * Also, don't send redirect if forwarding using a default route
	 * or a route modified by a redirect.
	 */
	dest.s_addr = 0;
	if (!srcrt && V_ipsendredirects &&
	    ia != NULL && ia->ia_ifp == m->m_pkthdr.rcvif) {
		struct sockaddr_in *sin;
		struct rtentry *rt;

		bzero(&ro, sizeof(ro));
		sin = (struct sockaddr_in *)&ro.ro_dst;
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = ip->ip_dst;
		in_rtalloc_ign(&ro, 0, M_GETFIB(m));

		rt = ro.ro_rt;

		if (rt && (rt->rt_flags & (RTF_DYNAMIC|RTF_MODIFIED)) == 0 &&
		    satosin(rt_key(rt))->sin_addr.s_addr != 0) {
#define	RTA(rt)	((struct in_ifaddr *)(rt->rt_ifa))
			u_long src = ntohl(ip->ip_src.s_addr);

			if (RTA(rt) &&
			    (src & RTA(rt)->ia_subnetmask) == RTA(rt)->ia_subnet) {
				if (rt->rt_flags & RTF_GATEWAY)
					dest.s_addr = satosin(rt->rt_gateway)->sin_addr.s_addr;
				else
					dest.s_addr = ip->ip_dst.s_addr;
				/* Router requirements says to only send host redirects */
				type = ICMP_REDIRECT;
				code = ICMP_REDIRECT_HOST;
			}
		}
		if (rt)
			RTFREE(rt);
	}

	/*
	 * Try to cache the route MTU from ip_output so we can consider it for
	 * the ICMP_UNREACH_NEEDFRAG "Next-Hop MTU" field described in RFC1191.
	 */
	bzero(&ro, sizeof(ro));

	error = ip_output(m, NULL, &ro, IP_FORWARDING, NULL, NULL);

	if (error == EMSGSIZE && ro.ro_rt)
		mtu = ro.ro_rt->rt_rmx.rmx_mtu;
	if (ro.ro_rt)
		RTFREE(ro.ro_rt);

	if (error)
		IPSTAT_INC(ips_cantforward);
	else {
		IPSTAT_INC(ips_forward);
		if (type)
			IPSTAT_INC(ips_redirectsent);
		else {
			if (mcopy)
				m_freem(mcopy);
			if (ia != NULL)
				ifa_free(&ia->ia_ifa);
			return;
		}
	}
	if (mcopy == NULL) {
		if (ia != NULL)
			ifa_free(&ia->ia_ifa);
		return;
	}

	switch (error) {

	case 0:				/* forwarded, but need redirect */
		/* type, code set above */
		break;

	case ENETUNREACH:
	case EHOSTUNREACH:
	case ENETDOWN:
	case EHOSTDOWN:
	default:
		type = ICMP_UNREACH;
		code = ICMP_UNREACH_HOST;
		break;

	case EMSGSIZE:
		type = ICMP_UNREACH;
		code = ICMP_UNREACH_NEEDFRAG;

#ifdef IPSEC
		/* 
		 * If IPsec is configured for this path,
		 * override any possibly mtu value set by ip_output.
		 */ 
		mtu = ip_ipsec_mtu(mcopy, mtu);
#endif /* IPSEC */
		/*
		 * If the MTU was set before make sure we are below the
		 * interface MTU.
		 * If the MTU wasn't set before use the interface mtu or
		 * fall back to the next smaller mtu step compared to the
		 * current packet size.
		 */
		if (mtu != 0) {
			if (ia != NULL)
				mtu = min(mtu, ia->ia_ifp->if_mtu);
		} else {
			if (ia != NULL)
				mtu = ia->ia_ifp->if_mtu;
			else
				mtu = ip_next_mtu(ip->ip_len, 0);
		}
		IPSTAT_INC(ips_cantfrag);
		break;

	case ENOBUFS:
		/*
		 * A router should not generate ICMP_SOURCEQUENCH as
		 * required in RFC1812 Requirements for IP Version 4 Routers.
		 * Source quench could be a big problem under DoS attacks,
		 * or if the underlying interface is rate-limited.
		 * Those who need source quench packets may re-enable them
		 * via the net.inet.ip.sendsourcequench sysctl.
		 */
		if (V_ip_sendsourcequench == 0) {
			m_freem(mcopy);
			if (ia != NULL)
				ifa_free(&ia->ia_ifa);
			return;
		} else {
			type = ICMP_SOURCEQUENCH;
			code = 0;
		}
		break;

	case EACCES:			/* ipfw denied packet */
		m_freem(mcopy);
		if (ia != NULL)
			ifa_free(&ia->ia_ifa);
		return;
	}
	if (ia != NULL)
		ifa_free(&ia->ia_ifa);
	icmp_error(mcopy, type, code, dest.s_addr, mtu);
}

void
ip_savecontrol(struct inpcb *inp, struct mbuf **mp, struct ip *ip,
    struct mbuf *m)
{

	if (inp->inp_socket->so_options & (SO_BINTIME | SO_TIMESTAMP)) {
		struct bintime bt;

		bintime(&bt);
		if (inp->inp_socket->so_options & SO_BINTIME) {
			*mp = sbcreatecontrol((caddr_t) &bt, sizeof(bt),
			SCM_BINTIME, SOL_SOCKET);
			if (*mp)
				mp = &(*mp)->m_next;
		}
		if (inp->inp_socket->so_options & SO_TIMESTAMP) {
			struct timeval tv;

			bintime2timeval(&bt, &tv);
			*mp = sbcreatecontrol((caddr_t) &tv, sizeof(tv),
				SCM_TIMESTAMP, SOL_SOCKET);
			if (*mp)
				mp = &(*mp)->m_next;
		}
	}
	if (inp->inp_flags & INP_RECVDSTADDR) {
		*mp = sbcreatecontrol((caddr_t) &ip->ip_dst,
		    sizeof(struct in_addr), IP_RECVDSTADDR, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
	if (inp->inp_flags & INP_RECVTTL) {
		*mp = sbcreatecontrol((caddr_t) &ip->ip_ttl,
		    sizeof(u_char), IP_RECVTTL, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
#ifdef notyet
	/* XXX
	 * Moving these out of udp_input() made them even more broken
	 * than they already were.
	 */
	/* options were tossed already */
	if (inp->inp_flags & INP_RECVOPTS) {
		*mp = sbcreatecontrol((caddr_t) opts_deleted_above,
		    sizeof(struct in_addr), IP_RECVOPTS, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
	/* ip_srcroute doesn't do what we want here, need to fix */
	if (inp->inp_flags & INP_RECVRETOPTS) {
		*mp = sbcreatecontrol((caddr_t) ip_srcroute(m),
		    sizeof(struct in_addr), IP_RECVRETOPTS, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
#endif
	if (inp->inp_flags & INP_RECVIF) {
		struct ifnet *ifp;
		struct sdlbuf {
			struct sockaddr_dl sdl;
			u_char	pad[32];
		} sdlbuf;
		struct sockaddr_dl *sdp;
		struct sockaddr_dl *sdl2 = &sdlbuf.sdl;

		if (((ifp = m->m_pkthdr.rcvif)) 
		&& ( ifp->if_index && (ifp->if_index <= V_if_index))) {
			sdp = (struct sockaddr_dl *)ifp->if_addr->ifa_addr;
			/*
			 * Change our mind and don't try copy.
			 */
			if ((sdp->sdl_family != AF_LINK)
			|| (sdp->sdl_len > sizeof(sdlbuf))) {
				goto makedummy;
			}
			bcopy(sdp, sdl2, sdp->sdl_len);
		} else {
makedummy:	
			sdl2->sdl_len
				= offsetof(struct sockaddr_dl, sdl_data[0]);
			sdl2->sdl_family = AF_LINK;
			sdl2->sdl_index = 0;
			sdl2->sdl_nlen = sdl2->sdl_alen = sdl2->sdl_slen = 0;
		}
		*mp = sbcreatecontrol((caddr_t) sdl2, sdl2->sdl_len,
			IP_RECVIF, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
}

/*
 * XXXRW: Multicast routing code in ip_mroute.c is generally MPSAFE, but the
 * ip_rsvp and ip_rsvp_on variables need to be interlocked with rsvp_on
 * locking.  This code remains in ip_input.c as ip_mroute.c is optionally
 * compiled.
 */
static VNET_DEFINE(int, ip_rsvp_on);
VNET_DEFINE(struct socket *, ip_rsvpd);

#define	V_ip_rsvp_on		VNET(ip_rsvp_on)

int
ip_rsvp_init(struct socket *so)
{

	if (so->so_type != SOCK_RAW ||
	    so->so_proto->pr_protocol != IPPROTO_RSVP)
		return EOPNOTSUPP;

	if (V_ip_rsvpd != NULL)
		return EADDRINUSE;

	V_ip_rsvpd = so;
	/*
	 * This may seem silly, but we need to be sure we don't over-increment
	 * the RSVP counter, in case something slips up.
	 */
	if (!V_ip_rsvp_on) {
		V_ip_rsvp_on = 1;
		V_rsvp_on++;
	}

	return 0;
}

int
ip_rsvp_done(void)
{

	V_ip_rsvpd = NULL;
	/*
	 * This may seem silly, but we need to be sure we don't over-decrement
	 * the RSVP counter, in case something slips up.
	 */
	if (V_ip_rsvp_on) {
		V_ip_rsvp_on = 0;
		V_rsvp_on--;
	}
	return 0;
}

void
rsvp_input(struct mbuf *m, int off)	/* XXX must fixup manually */
{

	if (rsvp_input_p) { /* call the real one if loaded */
		rsvp_input_p(m, off);
		return;
	}

	/* Can still get packets with rsvp_on = 0 if there is a local member
	 * of the group to which the RSVP packet is addressed.  But in this
	 * case we want to throw the packet away.
	 */
	
	if (!V_rsvp_on) {
		m_freem(m);
		return;
	}

	if (V_ip_rsvpd != NULL) { 
		rip_input(m, off);
		return;
	}
	/* Drop the packet */
	m_freem(m);
}
