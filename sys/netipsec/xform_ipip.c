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
#include <net/if_var.h>
#include <net/pfil.h>
#include <net/netisr.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_ecn.h>
#include <netinet/ip_var.h>

#include <netipsec/ipsec.h>
#include <netipsec/xform.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netipsec/ipsec6.h>
#include <netinet6/ip6_ecn.h>
#include <netinet6/in6_var.h>
#include <netinet6/scope6_var.h>
#endif

#include <netipsec/key.h>
#include <netipsec/key_debug.h>

int
ipip_output(struct mbuf *m, struct ipsecrequest *isr, struct mbuf **mp,
    int skip, int protoff)
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
			error = EINVAL;
			goto bad;
		}

		M_PREPEND(m, sizeof(struct ip), M_NOWAIT);
		if (m == 0) {
			DPRINTF(("%s: M_PREPEND failed\n", __func__));
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
		ip_fillid(ipo);

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
			error = ENOBUFS;
			goto bad;
		}

		/* scoped address handling */
		ip6 = mtod(m, struct ip6_hdr *);
		in6_clearscope(&ip6->ip6_src);
		in6_clearscope(&ip6->ip6_dst);
		M_PREPEND(m, sizeof(struct ip6_hdr), M_NOWAIT);
		if (m == 0) {
			DPRINTF(("%s: M_PREPEND failed\n", __func__));
			error = ENOBUFS;
			goto bad;
		}

		/* Initialize IPv6 header */
		ip6o = mtod(m, struct ip6_hdr *);
		ip6o->ip6_flow = 0;
		ip6o->ip6_vfc &= ~IPV6_VERSION_MASK;
		ip6o->ip6_vfc |= IPV6_VERSION;
		ip6o->ip6_hlim = IPV6_DEFHLIM;
		ip6o->ip6_dst = saidx->dst.sin6.sin6_addr;
		ip6o->ip6_src = saidx->src.sin6.sin6_addr;
		ip6o->ip6_plen = htons(m->m_pkthdr.len - sizeof(*ip6));

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
			break;
		}
		default:
			goto nofamily;
		}

		otos = 0;
		ip_ecn_ingress(V_ip6_ipsec_ecn, &otos, &itos);
		ip6o->ip6_flow |= htonl((u_int32_t) otos << 20);
		break;
#endif /* INET6 */

	default:
nofamily:
		DPRINTF(("%s: unsupported protocol family %u\n", __func__,
		    saidx->dst.sa.sa_family));
		error = EAFNOSUPPORT;		/* XXX diffs from openbsd */
		goto bad;
	}

	*mp = m;
	return (0);
bad:
	if (m)
		m_freem(m);
	*mp = NULL;
	return (error);
}

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

static void
ipe4_attach(void)
{

	xform_register(&ipe4_xformsw);
}
SYSINIT(ipe4_xform_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_MIDDLE, ipe4_attach, NULL);
