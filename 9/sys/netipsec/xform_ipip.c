/*	$FreeBSD$	*/
/*	$OpenBSD: ip_ipip.c,v 1.25 2002/06/10 18:04:55 itojun Exp $ */
/*-
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * The original version of this code was written by John Ioannidis
 * for BSD/OS in Athens, Greece, in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Additional features in 1999 by Angelos D. Keromytis.
 *
 * Copyright (C) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 * Copyright (c) 2001, Angelos D. Keromytis.
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

/*
 * IP-inside-IP processing
 */
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_enc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/protosw.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/pfil.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_ecn.h>
#include <netinet/ip_var.h>
#include <netinet/ip_encap.h>
#ifdef MROUTING
#include <netinet/ip_mroute.h>
#endif

#include <netipsec/ipsec.h>
#include <netipsec/xform.h>

#include <netipsec/ipip_var.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netipsec/ipsec6.h>
#include <netinet6/ip6_ecn.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6protosw.h>
#endif

#include <netipsec/key.h>
#include <netipsec/key_debug.h>

#include <machine/stdarg.h>

/*
 * We can control the acceptance of IP4 packets by altering the sysctl
 * net.inet.ipip.allow value.  Zero means drop them, all else is acceptance.
 */
VNET_DEFINE(int, ipip_allow) = 0;
VNET_DEFINE(struct ipipstat, ipipstat);

SYSCTL_DECL(_net_inet_ipip);
SYSCTL_VNET_INT(_net_inet_ipip, OID_AUTO,
	ipip_allow,	CTLFLAG_RW,	&VNET_NAME(ipip_allow),	0, "");
SYSCTL_VNET_STRUCT(_net_inet_ipip, IPSECCTL_STATS,
	stats,		CTLFLAG_RD,	&VNET_NAME(ipipstat),	ipipstat, "");

/* XXX IPCOMP */
#define	M_IPSEC	(M_AUTHIPHDR|M_AUTHIPDGM|M_DECRYPTED)

static void _ipip_input(struct mbuf *m, int iphlen, struct ifnet *gifp);

#ifdef INET6
/*
 * Really only a wrapper for ipip_input(), for use with IPv6.
 */
int
ip4_input6(struct mbuf **m, int *offp, int proto)
{
#if 0
	/* If we do not accept IP-in-IP explicitly, drop.  */
	if (!V_ipip_allow && ((*m)->m_flags & M_IPSEC) == 0) {
		DPRINTF(("%s: dropped due to policy\n", __func__));
		V_ipipstat.ipips_pdrops++;
		m_freem(*m);
		return IPPROTO_DONE;
	}
#endif
	_ipip_input(*m, *offp, NULL);
	return IPPROTO_DONE;
}
#endif /* INET6 */

#ifdef INET
/*
 * Really only a wrapper for ipip_input(), for use with IPv4.
 */
void
ip4_input(struct mbuf *m, int off)
{
#if 0
	/* If we do not accept IP-in-IP explicitly, drop.  */
	if (!V_ipip_allow && (m->m_flags & M_IPSEC) == 0) {
		DPRINTF(("%s: dropped due to policy\n", __func__));
		V_ipipstat.ipips_pdrops++;
		m_freem(m);
		return;
	}
#endif
	_ipip_input(m, off, NULL);
}
#endif /* INET */

/*
 * ipip_input gets called when we receive an IP{46} encapsulated packet,
 * either because we got it at a real interface, or because AH or ESP
 * were being used in tunnel mode (in which case the rcvif element will
 * contain the address of the encX interface associated with the tunnel.
 */

static void
_ipip_input(struct mbuf *m, int iphlen, struct ifnet *gifp)
{
#ifdef INET
	register struct sockaddr_in *sin;
#endif
	register struct ifnet *ifp;
	register struct ifaddr *ifa;
	struct ip *ipo;
#ifdef INET6
	register struct sockaddr_in6 *sin6;
	struct ip6_hdr *ip6 = NULL;
	u_int8_t itos;
#endif
	u_int8_t nxt;
	int isr;
	u_int8_t otos;
	u_int8_t v;
	int hlen;

	V_ipipstat.ipips_ipackets++;

	m_copydata(m, 0, 1, &v);

	switch (v >> 4) {
#ifdef INET
        case 4:
		hlen = sizeof(struct ip);
		break;
#endif /* INET */
#ifdef INET6
        case 6:
		hlen = sizeof(struct ip6_hdr);
		break;
#endif
        default:
		V_ipipstat.ipips_family++;
		m_freem(m);
		return /* EAFNOSUPPORT */;
	}

	/* Bring the IP header in the first mbuf, if not there already */
	if (m->m_len < hlen) {
		if ((m = m_pullup(m, hlen)) == NULL) {
			DPRINTF(("%s: m_pullup (1) failed\n", __func__));
			V_ipipstat.ipips_hdrops++;
			return;
		}
	}

	ipo = mtod(m, struct ip *);

#ifdef MROUTING
	if (ipo->ip_v == IPVERSION && ipo->ip_p == IPPROTO_IPV4) {
		if (IN_MULTICAST(((struct ip *)((char *) ipo + iphlen))->ip_dst.s_addr)) {
			ipip_mroute_input (m, iphlen);
			return;
		}
	}
#endif /* MROUTING */

	/* Keep outer ecn field. */
	switch (v >> 4) {
#ifdef INET
	case 4:
		otos = ipo->ip_tos;
		break;
#endif /* INET */
#ifdef INET6
	case 6:
		otos = (ntohl(mtod(m, struct ip6_hdr *)->ip6_flow) >> 20) & 0xff;
		break;
#endif
	default:
		panic("ipip_input: unknown ip version %u (outer)", v>>4);
	}

	/* Remove outer IP header */
	m_adj(m, iphlen);

	/* Sanity check */
	if (m->m_pkthdr.len < sizeof(struct ip))  {
		V_ipipstat.ipips_hdrops++;
		m_freem(m);
		return;
	}

	m_copydata(m, 0, 1, &v);

	switch (v >> 4) {
#ifdef INET
        case 4:
		hlen = sizeof(struct ip);
		break;
#endif /* INET */

#ifdef INET6
        case 6:
		hlen = sizeof(struct ip6_hdr);
		break;
#endif
	default:
		V_ipipstat.ipips_family++;
		m_freem(m);
		return; /* EAFNOSUPPORT */
	}

	/*
	 * Bring the inner IP header in the first mbuf, if not there already.
	 */
	if (m->m_len < hlen) {
		if ((m = m_pullup(m, hlen)) == NULL) {
			DPRINTF(("%s: m_pullup (2) failed\n", __func__));
			V_ipipstat.ipips_hdrops++;
			return;
		}
	}

	/*
	 * RFC 1853 specifies that the inner TTL should not be touched on
	 * decapsulation. There's no reason this comment should be here, but
	 * this is as good as any a position.
	 */

	/* Some sanity checks in the inner IP header */
	switch (v >> 4) {
#ifdef INET
    	case 4:
                ipo = mtod(m, struct ip *);
                nxt = ipo->ip_p;
		ip_ecn_egress(V_ip4_ipsec_ecn, &otos, &ipo->ip_tos);
                break;
#endif /* INET */
#ifdef INET6
    	case 6:
                ip6 = (struct ip6_hdr *) ipo;
                nxt = ip6->ip6_nxt;
		itos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
		ip_ecn_egress(V_ip6_ipsec_ecn, &otos, &itos);
		ip6->ip6_flow &= ~htonl(0xff << 20);
		ip6->ip6_flow |= htonl((u_int32_t) itos << 20);
                break;
#endif
	default:
		panic("ipip_input: unknown ip version %u (inner)", v>>4);
	}

	/* Check for local address spoofing. */
	if ((m->m_pkthdr.rcvif == NULL ||
	    !(m->m_pkthdr.rcvif->if_flags & IFF_LOOPBACK)) &&
	    V_ipip_allow != 2) {
	    	IFNET_RLOCK_NOSLEEP();
		TAILQ_FOREACH(ifp, &V_ifnet, if_link) {
			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
#ifdef INET
				if (ipo) {
					if (ifa->ifa_addr->sa_family !=
					    AF_INET)
						continue;

					sin = (struct sockaddr_in *) ifa->ifa_addr;

					if (sin->sin_addr.s_addr ==
					    ipo->ip_src.s_addr)	{
						V_ipipstat.ipips_spoof++;
						m_freem(m);
						IFNET_RUNLOCK_NOSLEEP();
						return;
					}
				}
#endif /* INET */

#ifdef INET6
				if (ip6) {
					if (ifa->ifa_addr->sa_family !=
					    AF_INET6)
						continue;

					sin6 = (struct sockaddr_in6 *) ifa->ifa_addr;

					if (IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr, &ip6->ip6_src)) {
						V_ipipstat.ipips_spoof++;
						m_freem(m);
						IFNET_RUNLOCK_NOSLEEP();
						return;
					}

				}
#endif /* INET6 */
			}
		}
		IFNET_RUNLOCK_NOSLEEP();
	}

	/* Statistics */
	V_ipipstat.ipips_ibytes += m->m_pkthdr.len - iphlen;

#ifdef DEV_ENC
	switch (v >> 4) {
#ifdef INET
	case 4:
		ipsec_bpf(m, NULL, AF_INET, ENC_IN|ENC_AFTER);
		break;
#endif
#ifdef INET6
	case 6:
		ipsec_bpf(m, NULL, AF_INET6, ENC_IN|ENC_AFTER);
		break;
#endif
	default:
		panic("%s: bogus ip version %u", __func__, v>>4);
	}
	/* pass the mbuf to enc0 for packet filtering */
	if (ipsec_filter(&m, PFIL_IN, ENC_IN|ENC_AFTER) != 0)
		return;
#endif

	/*
	 * Interface pointer stays the same; if no IPsec processing has
	 * been done (or will be done), this will point to a normal
	 * interface. Otherwise, it'll point to an enc interface, which
	 * will allow a packet filter to distinguish between secure and
	 * untrusted packets.
	 */

	switch (v >> 4) {
#ifdef INET
	case 4:
		isr = NETISR_IP;
		break;
#endif
#ifdef INET6
	case 6:
		isr = NETISR_IPV6;
		break;
#endif
	default:
		panic("%s: bogus ip version %u", __func__, v>>4);
	}

	m_addr_changed(m);

	if (netisr_queue(isr, m)) {	/* (0) on success. */
		V_ipipstat.ipips_qfull++;
		DPRINTF(("%s: packet dropped because of full queue\n",
			__func__));
	}
}

int
ipip_output(
	struct mbuf *m,
	struct ipsecrequest *isr,
	struct mbuf **mp,
	int skip,
	int protoff
)
{
	struct secasvar *sav;
	u_int8_t tp, otos;
	struct secasindex *saidx;
	int error;
#if defined(INET) || defined(INET6)
	u_int8_t itos;
#endif
#ifdef INET
	struct ip *ipo;
#endif /* INET */
#ifdef INET6
	struct ip6_hdr *ip6, *ip6o;
#endif /* INET6 */

	sav = isr->sav;
	IPSEC_ASSERT(sav != NULL, ("null SA"));
	IPSEC_ASSERT(sav->sah != NULL, ("null SAH"));

	/* XXX Deal with empty TDB source/destination addresses. */

	m_copydata(m, 0, 1, &tp);
	tp = (tp >> 4) & 0xff;  /* Get the IP version number. */

	saidx = &sav->sah->saidx;
	switch (saidx->dst.sa.sa_family) {
#ifdef INET
	case AF_INET:
		if (saidx->src.sa.sa_family != AF_INET ||
		    saidx->src.sin.sin_addr.s_addr == INADDR_ANY ||
		    saidx->dst.sin.sin_addr.s_addr == INADDR_ANY) {
			DPRINTF(("%s: unspecified tunnel endpoint "
			    "address in SA %s/%08lx\n", __func__,
			    ipsec_address(&saidx->dst),
			    (u_long) ntohl(sav->spi)));
			V_ipipstat.ipips_unspec++;
			error = EINVAL;
			goto bad;
		}

		M_PREPEND(m, sizeof(struct ip), M_DONTWAIT);
		if (m == 0) {
			DPRINTF(("%s: M_PREPEND failed\n", __func__));
			V_ipipstat.ipips_hdrops++;
			error = ENOBUFS;
			goto bad;
		}

		ipo = mtod(m, struct ip *);

		ipo->ip_v = IPVERSION;
		ipo->ip_hl = 5;
		ipo->ip_len = htons(m->m_pkthdr.len);
		ipo->ip_ttl = V_ip_defttl;
		ipo->ip_sum = 0;
		ipo->ip_src = saidx->src.sin.sin_addr;
		ipo->ip_dst = saidx->dst.sin.sin_addr;

		ipo->ip_id = ip_newid();

		/* If the inner protocol is IP... */
		switch (tp) {
		case IPVERSION:
			/* Save ECN notification */
			m_copydata(m, sizeof(struct ip) +
			    offsetof(struct ip, ip_tos),
			    sizeof(u_int8_t), (caddr_t) &itos);

			ipo->ip_p = IPPROTO_IPIP;

			/*
			 * We should be keeping tunnel soft-state and
			 * send back ICMPs if needed.
			 */
			m_copydata(m, sizeof(struct ip) +
			    offsetof(struct ip, ip_off),
			    sizeof(u_int16_t), (caddr_t) &ipo->ip_off);
			ipo->ip_off = ntohs(ipo->ip_off);
			ipo->ip_off &= ~(IP_DF | IP_MF | IP_OFFMASK);
			ipo->ip_off = htons(ipo->ip_off);
			break;
#ifdef INET6
		case (IPV6_VERSION >> 4):
		{
			u_int32_t itos32;

			/* Save ECN notification. */
			m_copydata(m, sizeof(struct ip) +
			    offsetof(struct ip6_hdr, ip6_flow),
			    sizeof(u_int32_t), (caddr_t) &itos32);
			itos = ntohl(itos32) >> 20;
			ipo->ip_p = IPPROTO_IPV6;
			ipo->ip_off = 0;
			break;
		}
#endif /* INET6 */
		default:
			goto nofamily;
		}

		otos = 0;
		ip_ecn_ingress(ECN_ALLOWED, &otos, &itos);
		ipo->ip_tos = otos;
		break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:
		if (IN6_IS_ADDR_UNSPECIFIED(&saidx->dst.sin6.sin6_addr) ||
		    saidx->src.sa.sa_family != AF_INET6 ||
		    IN6_IS_ADDR_UNSPECIFIED(&saidx->src.sin6.sin6_addr)) {
			DPRINTF(("%s: unspecified tunnel endpoint "
			    "address in SA %s/%08lx\n", __func__,
			    ipsec_address(&saidx->dst),
			    (u_long) ntohl(sav->spi)));
			V_ipipstat.ipips_unspec++;
			error = ENOBUFS;
			goto bad;
		}

		/* scoped address handling */
		ip6 = mtod(m, struct ip6_hdr *);
		if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_src))
			ip6->ip6_src.s6_addr16[1] = 0;
		if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_dst))
			ip6->ip6_dst.s6_addr16[1] = 0;

		M_PREPEND(m, sizeof(struct ip6_hdr), M_DONTWAIT);
		if (m == 0) {
			DPRINTF(("%s: M_PREPEND failed\n", __func__));
			V_ipipstat.ipips_hdrops++;
			error = ENOBUFS;
			goto bad;
		}

		/* Initialize IPv6 header */
		ip6o = mtod(m, struct ip6_hdr *);
		ip6o->ip6_flow = 0;
		ip6o->ip6_vfc &= ~IPV6_VERSION_MASK;
		ip6o->ip6_vfc |= IPV6_VERSION;
		ip6o->ip6_plen = htons(m->m_pkthdr.len);
		ip6o->ip6_hlim = V_ip_defttl;
		ip6o->ip6_dst = saidx->dst.sin6.sin6_addr;
		ip6o->ip6_src = saidx->src.sin6.sin6_addr;

		switch (tp) {
#ifdef INET
		case IPVERSION:
			/* Save ECN notification */
			m_copydata(m, sizeof(struct ip6_hdr) +
			    offsetof(struct ip, ip_tos), sizeof(u_int8_t),
			    (caddr_t) &itos);

			/* This is really IPVERSION. */
			ip6o->ip6_nxt = IPPROTO_IPIP;
			break;
#endif /* INET */
		case (IPV6_VERSION >> 4):
		{
			u_int32_t itos32;

			/* Save ECN notification. */
			m_copydata(m, sizeof(struct ip6_hdr) +
			    offsetof(struct ip6_hdr, ip6_flow),
			    sizeof(u_int32_t), (caddr_t) &itos32);
			itos = ntohl(itos32) >> 20;

			ip6o->ip6_nxt = IPPROTO_IPV6;
		}
		default:
			goto nofamily;
		}

		otos = 0;
		ip_ecn_ingress(ECN_ALLOWED, &otos, &itos);
		ip6o->ip6_flow |= htonl((u_int32_t) otos << 20);
		break;
#endif /* INET6 */

	default:
nofamily:
		DPRINTF(("%s: unsupported protocol family %u\n", __func__,
		    saidx->dst.sa.sa_family));
		V_ipipstat.ipips_family++;
		error = EAFNOSUPPORT;		/* XXX diffs from openbsd */
		goto bad;
	}

	V_ipipstat.ipips_opackets++;
	*mp = m;

#ifdef INET
	if (saidx->dst.sa.sa_family == AF_INET) {
#if 0
		if (sav->tdb_xform->xf_type == XF_IP4)
			tdb->tdb_cur_bytes +=
			    m->m_pkthdr.len - sizeof(struct ip);
#endif
		V_ipipstat.ipips_obytes += m->m_pkthdr.len - sizeof(struct ip);
	}
#endif /* INET */

#ifdef INET6
	if (saidx->dst.sa.sa_family == AF_INET6) {
#if 0
		if (sav->tdb_xform->xf_type == XF_IP4)
			tdb->tdb_cur_bytes +=
			    m->m_pkthdr.len - sizeof(struct ip6_hdr);
#endif
		V_ipipstat.ipips_obytes +=
		    m->m_pkthdr.len - sizeof(struct ip6_hdr);
	}
#endif /* INET6 */

	return 0;
bad:
	if (m)
		m_freem(m);
	*mp = NULL;
	return (error);
}

#ifdef IPSEC
#if defined(INET) || defined(INET6)
static int
ipe4_init(struct secasvar *sav, struct xformsw *xsp)
{
	sav->tdb_xform = xsp;
	return 0;
}

static int
ipe4_zeroize(struct secasvar *sav)
{
	sav->tdb_xform = NULL;
	return 0;
}

static int
ipe4_input(struct mbuf *m, struct secasvar *sav, int skip, int protoff)
{
	/* This is a rather serious mistake, so no conditional printing. */
	printf("%s: should never be called\n", __func__);
	if (m)
		m_freem(m);
	return EOPNOTSUPP;
}

static struct xformsw ipe4_xformsw = {
	XF_IP4,		0,		"IPv4 Simple Encapsulation",
	ipe4_init,	ipe4_zeroize,	ipe4_input,	ipip_output,
};

extern struct domain inetdomain;
#endif /* INET || INET6 */
#ifdef INET
static struct protosw ipe4_protosw = {
	.pr_type =	SOCK_RAW,
	.pr_domain =	&inetdomain,
	.pr_protocol =	IPPROTO_IPV4,
	.pr_flags =	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input =	ip4_input,
	.pr_ctloutput =	rip_ctloutput,
	.pr_usrreqs =	&rip_usrreqs
};
#endif /* INET */
#if defined(INET6) && defined(INET)
static struct ip6protosw ipe6_protosw = {
	.pr_type =	SOCK_RAW,
	.pr_domain =	&inetdomain,
	.pr_protocol =	IPPROTO_IPV6,
	.pr_flags =	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input =	ip4_input6,
	.pr_ctloutput =	rip_ctloutput,
	.pr_usrreqs =	&rip_usrreqs
};
#endif /* INET6 && INET */

#if defined(INET)
/*
 * Check the encapsulated packet to see if we want it
 */
static int
ipe4_encapcheck(const struct mbuf *m, int off, int proto, void *arg)
{
	/*
	 * Only take packets coming from IPSEC tunnels; the rest
	 * must be handled by the gif tunnel code.  Note that we
	 * also return a minimum priority when we want the packet
	 * so any explicit gif tunnels take precedence.
	 */
	return ((m->m_flags & M_IPSEC) != 0 ? 1 : 0);
}
#endif /* INET */

static void
ipe4_attach(void)
{

	xform_register(&ipe4_xformsw);
	/* attach to encapsulation framework */
	/* XXX save return cookie for detach on module remove */
#ifdef INET
	(void) encap_attach_func(AF_INET, -1,
		ipe4_encapcheck, &ipe4_protosw, NULL);
#endif
#if defined(INET6) && defined(INET)
	(void) encap_attach_func(AF_INET6, -1,
		ipe4_encapcheck, (struct protosw *)&ipe6_protosw, NULL);
#endif
}
SYSINIT(ipe4_xform_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_MIDDLE, ipe4_attach, NULL);
#endif	/* IPSEC */
