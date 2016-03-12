/*-
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
 * All rights reserved.
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

/*
 * IPsec output processing.
 */
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/hhook.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_enc.h>
#include <net/if_var.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>
#include <netinet/ip_ecn.h>
#ifdef INET6
#include <netinet6/ip6_ecn.h>
#endif

#include <netinet/ip6.h>
#ifdef INET6
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#endif
#include <netinet/in_pcb.h>
#ifdef INET6
#include <netinet/icmp6.h>
#endif

#include <netipsec/ipsec.h>
#ifdef INET6
#include <netipsec/ipsec6.h>
#endif
#include <netipsec/ah_var.h>
#include <netipsec/esp_var.h>
#include <netipsec/ipcomp_var.h>

#include <netipsec/xform.h>

#include <netipsec/key.h>
#include <netipsec/keydb.h>
#include <netipsec/key_debug.h>

#include <machine/in_cksum.h>

#ifdef IPSEC_NAT_T
#include <netinet/udp.h>
#endif

int
ipsec_process_done(struct mbuf *m, struct ipsecrequest *isr)
{
	struct tdb_ident *tdbi;
	struct m_tag *mtag;
	struct secasvar *sav;
	struct secasindex *saidx;
	int error;

	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(isr != NULL, ("null ISR"));
	IPSEC_ASSERT(isr->sp != NULL, ("NULL isr->sp"));
	sav = isr->sav;
	IPSEC_ASSERT(sav != NULL, ("null SA"));
	IPSEC_ASSERT(sav->sah != NULL, ("null SAH"));

	saidx = &sav->sah->saidx;
	switch (saidx->dst.sa.sa_family) {
#ifdef INET
	case AF_INET:
		/* Fix the header length, for AH processing. */
		mtod(m, struct ip *)->ip_len = htons(m->m_pkthdr.len);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		/* Fix the header length, for AH processing. */
		if (m->m_pkthdr.len < sizeof (struct ip6_hdr)) {
			error = ENXIO;
			goto bad;
		}
		if (m->m_pkthdr.len - sizeof (struct ip6_hdr) > IPV6_MAXPACKET) {
			/* No jumbogram support. */
			error = ENXIO;	/*?*/
			goto bad;
		}
		mtod(m, struct ip6_hdr *)->ip6_plen =
			htons(m->m_pkthdr.len - sizeof(struct ip6_hdr));
		break;
#endif /* INET6 */
	default:
		DPRINTF(("%s: unknown protocol family %u\n", __func__,
		    saidx->dst.sa.sa_family));
		error = ENXIO;
		goto bad;
	}

	/*
	 * Add a record of what we've done or what needs to be done to the
	 * packet.
	 */
	mtag = m_tag_get(PACKET_TAG_IPSEC_OUT_DONE,
			sizeof(struct tdb_ident), M_NOWAIT);
	if (mtag == NULL) {
		DPRINTF(("%s: could not get packet tag\n", __func__));
		error = ENOMEM;
		goto bad;
	}

	tdbi = (struct tdb_ident *)(mtag + 1);
	tdbi->dst = saidx->dst;
	tdbi->proto = saidx->proto;
	tdbi->spi = sav->spi;
	m_tag_prepend(m, mtag);

	key_sa_recordxfer(sav, m);		/* record data transfer */

	/*
	 * If there's another (bundled) SA to apply, do so.
	 * Note that this puts a burden on the kernel stack size.
	 * If this is a problem we'll need to introduce a queue
	 * to set the packet on so we can unwind the stack before
	 * doing further processing.
	 */
	if (isr->next) {
		/* XXX-BZ currently only support same AF bundles. */
		switch (saidx->dst.sa.sa_family) {
#ifdef INET
		case AF_INET:
			IPSECSTAT_INC(ips_out_bundlesa);
			return (ipsec4_process_packet(m, isr->next));
			/* NOTREACHED */
#endif
#ifdef notyet
#ifdef INET6
		case AF_INET6:
			/* XXX */
			IPSEC6STAT_INC(ips_out_bundlesa);
			return (ipsec6_process_packet(m, isr->next));
			/* NOTREACHED */
#endif /* INET6 */
#endif
		default:
			DPRINTF(("%s: unknown protocol family %u\n", __func__,
			    saidx->dst.sa.sa_family));
			error = ENXIO;
			goto bad;
		}
	}

	/*
	 * We're done with IPsec processing, transmit the packet using the
	 * appropriate network protocol (IP or IPv6).
	 */
	switch (saidx->dst.sa.sa_family) {
#ifdef INET
	case AF_INET:
#ifdef IPSEC_NAT_T
		/*
		 * If NAT-T is enabled, now that all IPsec processing is done
		 * insert UDP encapsulation header after IP header.
		 */
		if (sav->natt_type) {
			struct ip *ip = mtod(m, struct ip *);
			const int hlen = (ip->ip_hl << 2);
			int size, off;
			struct mbuf *mi;
			struct udphdr *udp;

			size = sizeof(struct udphdr);
			if (sav->natt_type == UDP_ENCAP_ESPINUDP_NON_IKE) {
				/*
				 * draft-ietf-ipsec-nat-t-ike-0[01].txt and
				 * draft-ietf-ipsec-udp-encaps-(00/)01.txt,
				 * ignoring possible AH mode
				 * non-IKE marker + non-ESP marker
				 * from draft-ietf-ipsec-udp-encaps-00.txt.
				 */
				size += sizeof(u_int64_t);
			}
			mi = m_makespace(m, hlen, size, &off);
			if (mi == NULL) {
				DPRINTF(("%s: m_makespace for udphdr failed\n",
				    __func__));
				error = ENOBUFS;
				goto bad;
			}

			udp = (struct udphdr *)(mtod(mi, caddr_t) + off);
			if (sav->natt_type == UDP_ENCAP_ESPINUDP_NON_IKE)
				udp->uh_sport = htons(UDP_ENCAP_ESPINUDP_PORT);
			else
				udp->uh_sport =
					KEY_PORTFROMSADDR(&sav->sah->saidx.src);
			udp->uh_dport = KEY_PORTFROMSADDR(&sav->sah->saidx.dst);
			udp->uh_sum = 0;
			udp->uh_ulen = htons(m->m_pkthdr.len - hlen);
			ip->ip_len = htons(m->m_pkthdr.len);
			ip->ip_p = IPPROTO_UDP;

			if (sav->natt_type == UDP_ENCAP_ESPINUDP_NON_IKE)
				*(u_int64_t *)(udp + 1) = 0;
		}
#endif /* IPSEC_NAT_T */

		return ip_output(m, NULL, NULL, IP_RAWOUTPUT, NULL, NULL);
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		/*
		 * We don't need massage, IPv6 header fields are always in
		 * net endian.
		 */
		return ip6_output(m, NULL, NULL, 0, NULL, NULL, NULL);
#endif /* INET6 */
	}
	panic("ipsec_process_done");
bad:
	m_freem(m);
	return (error);
}

static struct ipsecrequest *
ipsec_nextisr(
	struct mbuf *m,
	struct ipsecrequest *isr,
	int af,
	struct secasindex *saidx,
	int *error
)
{
#define	IPSEC_OSTAT(name)	do {		\
	if (isr->saidx.proto == IPPROTO_ESP)	\
		ESPSTAT_INC(esps_##name);	\
	else if (isr->saidx.proto == IPPROTO_AH)\
		AHSTAT_INC(ahs_##name);		\
	else					\
		IPCOMPSTAT_INC(ipcomps_##name);	\
} while (0)
	struct secasvar *sav;

	IPSECREQUEST_LOCK_ASSERT(isr);

	IPSEC_ASSERT(af == AF_INET || af == AF_INET6,
		("invalid address family %u", af));
again:
	/*
	 * Craft SA index to search for proper SA.  Note that
	 * we only fillin unspecified SA peers for transport
	 * mode; for tunnel mode they must already be filled in.
	 */
	*saidx = isr->saidx;
	if (isr->saidx.mode == IPSEC_MODE_TRANSPORT) {
		/* Fillin unspecified SA peers only for transport mode */
		if (af == AF_INET) {
			struct sockaddr_in *sin;
			struct ip *ip = mtod(m, struct ip *);

			if (saidx->src.sa.sa_len == 0) {
				sin = &saidx->src.sin;
				sin->sin_len = sizeof(*sin);
				sin->sin_family = AF_INET;
				sin->sin_port = IPSEC_PORT_ANY;
				sin->sin_addr = ip->ip_src;
			}
			if (saidx->dst.sa.sa_len == 0) {
				sin = &saidx->dst.sin;
				sin->sin_len = sizeof(*sin);
				sin->sin_family = AF_INET;
				sin->sin_port = IPSEC_PORT_ANY;
				sin->sin_addr = ip->ip_dst;
			}
		} else {
			struct sockaddr_in6 *sin6;
			struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);

			if (saidx->src.sin6.sin6_len == 0) {
				sin6 = (struct sockaddr_in6 *)&saidx->src;
				sin6->sin6_len = sizeof(*sin6);
				sin6->sin6_family = AF_INET6;
				sin6->sin6_port = IPSEC_PORT_ANY;
				sin6->sin6_addr = ip6->ip6_src;
				if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_src)) {
					/* fix scope id for comparing SPD */
					sin6->sin6_addr.s6_addr16[1] = 0;
					sin6->sin6_scope_id =
					    ntohs(ip6->ip6_src.s6_addr16[1]);
				}
			}
			if (saidx->dst.sin6.sin6_len == 0) {
				sin6 = (struct sockaddr_in6 *)&saidx->dst;
				sin6->sin6_len = sizeof(*sin6);
				sin6->sin6_family = AF_INET6;
				sin6->sin6_port = IPSEC_PORT_ANY;
				sin6->sin6_addr = ip6->ip6_dst;
				if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_dst)) {
					/* fix scope id for comparing SPD */
					sin6->sin6_addr.s6_addr16[1] = 0;
					sin6->sin6_scope_id =
					    ntohs(ip6->ip6_dst.s6_addr16[1]);
				}
			}
		}
	}

	/*
	 * Lookup SA and validate it.
	 */
	*error = key_checkrequest(isr, saidx);
	if (*error != 0) {
		/*
		 * IPsec processing is required, but no SA found.
		 * I assume that key_acquire() had been called
		 * to get/establish the SA. Here I discard
		 * this packet because it is responsibility for
		 * upper layer to retransmit the packet.
		 */
		switch(af) {
		case AF_INET:
			IPSECSTAT_INC(ips_out_nosa);
			break;
#ifdef INET6
		case AF_INET6:
			IPSEC6STAT_INC(ips_out_nosa);
			break;
#endif
		}
		goto bad;
	}
	sav = isr->sav;
	if (sav == NULL) {
		IPSEC_ASSERT(ipsec_get_reqlevel(isr) == IPSEC_LEVEL_USE,
			("no SA found, but required; level %u",
			ipsec_get_reqlevel(isr)));
		IPSECREQUEST_UNLOCK(isr);
		isr = isr->next;
		/*
		 * If isr is NULL, we found a 'use' policy w/o SA.
		 * Return w/o error and w/o isr so we can drop out
		 * and continue w/o IPsec processing.
		 */
		if (isr == NULL)
			return isr;
		IPSECREQUEST_LOCK(isr);
		goto again;
	}

	/*
	 * Check system global policy controls.
	 */
	if ((isr->saidx.proto == IPPROTO_ESP && !V_esp_enable) ||
	    (isr->saidx.proto == IPPROTO_AH && !V_ah_enable) ||
	    (isr->saidx.proto == IPPROTO_IPCOMP && !V_ipcomp_enable)) {
		DPRINTF(("%s: IPsec outbound packet dropped due"
			" to policy (check your sysctls)\n", __func__));
		IPSEC_OSTAT(pdrops);
		*error = EHOSTUNREACH;
		goto bad;
	}

	/*
	 * Sanity check the SA contents for the caller
	 * before they invoke the xform output method.
	 */
	if (sav->tdb_xform == NULL) {
		DPRINTF(("%s: no transform for SA\n", __func__));
		IPSEC_OSTAT(noxform);
		*error = EHOSTUNREACH;
		goto bad;
	}
	return isr;
bad:
	IPSEC_ASSERT(*error != 0, ("error return w/ no error code"));
	IPSECREQUEST_UNLOCK(isr);
	return NULL;
#undef IPSEC_OSTAT
}

static int
ipsec_encap(struct mbuf **mp, struct secasindex *saidx)
{
#ifdef INET6
	struct ip6_hdr *ip6;
#endif
	struct ip *ip;
	int setdf;
	uint8_t itos, proto;

	ip = mtod(*mp, struct ip *);
	switch (ip->ip_v) {
#ifdef INET
	case IPVERSION:
		proto = IPPROTO_IPIP;
		/*
		 * Collect IP_DF state from the inner header
		 * and honor system-wide control of how to handle it.
		 */
		switch (V_ip4_ipsec_dfbit) {
		case 0:	/* clear in outer header */
		case 1:	/* set in outer header */
			setdf = V_ip4_ipsec_dfbit;
			break;
		default:/* propagate to outer header */
			setdf = (ip->ip_off & ntohs(IP_DF)) != 0;
		}
		itos = ip->ip_tos;
		break;
#endif
#ifdef INET6
	case (IPV6_VERSION >> 4):
		proto = IPPROTO_IPV6;
		ip6 = mtod(*mp, struct ip6_hdr *);
		itos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
		setdf = V_ip4_ipsec_dfbit ? 1: 0;
		/* scoped address handling */
		in6_clearscope(&ip6->ip6_src);
		in6_clearscope(&ip6->ip6_dst);
		break;
#endif
	default:
		return (EAFNOSUPPORT);
	}
	switch (saidx->dst.sa.sa_family) {
#ifdef INET
	case AF_INET:
		if (saidx->src.sa.sa_family != AF_INET ||
		    saidx->src.sin.sin_addr.s_addr == INADDR_ANY ||
		    saidx->dst.sin.sin_addr.s_addr == INADDR_ANY)
			return (EINVAL);
		M_PREPEND(*mp, sizeof(struct ip), M_NOWAIT);
		if (*mp == NULL)
			return (ENOBUFS);
		ip = mtod(*mp, struct ip *);
		ip->ip_v = IPVERSION;
		ip->ip_hl = sizeof(struct ip) >> 2;
		ip->ip_p = proto;
		ip->ip_len = htons((*mp)->m_pkthdr.len);
		ip->ip_ttl = V_ip_defttl;
		ip->ip_sum = 0;
		ip->ip_off = setdf ? htons(IP_DF): 0;
		ip->ip_src = saidx->src.sin.sin_addr;
		ip->ip_dst = saidx->dst.sin.sin_addr;
		ip_ecn_ingress(V_ip4_ipsec_ecn, &ip->ip_tos, &itos);
		ip_fillid(ip);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		if (saidx->src.sa.sa_family != AF_INET6 ||
		    IN6_IS_ADDR_UNSPECIFIED(&saidx->src.sin6.sin6_addr) ||
		    IN6_IS_ADDR_UNSPECIFIED(&saidx->dst.sin6.sin6_addr))
			return (EINVAL);
		M_PREPEND(*mp, sizeof(struct ip6_hdr), M_NOWAIT);
		if (*mp == NULL)
			return (ENOBUFS);
		ip6 = mtod(*mp, struct ip6_hdr *);
		ip6->ip6_flow = 0;
		ip6->ip6_vfc = IPV6_VERSION;
		ip6->ip6_hlim = V_ip6_defhlim;
		ip6->ip6_nxt = proto;
		ip6->ip6_dst = saidx->dst.sin6.sin6_addr;
		/* For link-local address embed scope zone id */
		if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_dst))
			ip6->ip6_dst.s6_addr16[1] =
			    htons(saidx->dst.sin6.sin6_scope_id & 0xffff);
		ip6->ip6_src = saidx->src.sin6.sin6_addr;
		if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_src))
			ip6->ip6_src.s6_addr16[1] =
			    htons(saidx->src.sin6.sin6_scope_id & 0xffff);
		ip6->ip6_plen = htons((*mp)->m_pkthdr.len - sizeof(*ip6));
		ip_ecn_ingress(V_ip6_ipsec_ecn, &proto, &itos);
		ip6->ip6_flow |= htonl((uint32_t)proto << 20);
		break;
#endif /* INET6 */
	default:
		return (EAFNOSUPPORT);
	}
	return (0);
}

#ifdef INET
/*
 * IPsec output logic for IPv4.
 */
int
ipsec4_process_packet(struct mbuf *m, struct ipsecrequest *isr)
{
	char sbuf[INET6_ADDRSTRLEN], dbuf[INET6_ADDRSTRLEN];
	struct ipsec_ctx_data ctx;
	union sockaddr_union *dst;
	struct secasindex saidx;
	struct secasvar *sav;
	struct ip *ip;
	int error, i, off;

	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(isr != NULL, ("null isr"));

	IPSECREQUEST_LOCK(isr);		/* insure SA contents don't change */

	isr = ipsec_nextisr(m, isr, AF_INET, &saidx, &error);
	if (isr == NULL) {
		if (error != 0)
			goto bad;
		return EJUSTRETURN;
	}

	sav = isr->sav;
	if (m->m_len < sizeof(struct ip) &&
	    (m = m_pullup(m, sizeof (struct ip))) == NULL) {
		error = ENOBUFS;
		goto bad;
	}

	IPSEC_INIT_CTX(&ctx, &m, sav, AF_INET, IPSEC_ENC_BEFORE);
	if ((error = ipsec_run_hhooks(&ctx, HHOOK_TYPE_IPSEC_OUT)) != 0)
		goto bad;

	ip = mtod(m, struct ip *);
	dst = &sav->sah->saidx.dst;
	/* Do the appropriate encapsulation, if necessary */
	if (isr->saidx.mode == IPSEC_MODE_TUNNEL || /* Tunnel requ'd */
	    dst->sa.sa_family != AF_INET ||	    /* PF mismatch */
	    (dst->sa.sa_family == AF_INET &&	    /* Proxy */
	     dst->sin.sin_addr.s_addr != INADDR_ANY &&
	     dst->sin.sin_addr.s_addr != ip->ip_dst.s_addr)) {
		/* Fix IPv4 header checksum and length */
		ip->ip_len = htons(m->m_pkthdr.len);
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(m, ip->ip_hl << 2);
		error = ipsec_encap(&m, &sav->sah->saidx);
		if (error != 0) {
			DPRINTF(("%s: encapsulation for SA %s->%s "
			    "SPI 0x%08x failed with error %d\n", __func__,
			    ipsec_address(&sav->sah->saidx.src, sbuf,
				sizeof(sbuf)),
			    ipsec_address(&sav->sah->saidx.dst, dbuf,
				sizeof(dbuf)), ntohl(sav->spi), error));
			goto bad;
		}
	}

	IPSEC_INIT_CTX(&ctx, &m, sav, dst->sa.sa_family, IPSEC_ENC_AFTER);
	if ((error = ipsec_run_hhooks(&ctx, HHOOK_TYPE_IPSEC_OUT)) != 0)
		goto bad;

	/*
	 * Dispatch to the appropriate IPsec transform logic.  The
	 * packet will be returned for transmission after crypto
	 * processing, etc. are completed.
	 *
	 * NB: m & sav are ``passed to caller'' who's reponsible for
	 *     for reclaiming their resources.
	 */
	switch(dst->sa.sa_family) {
	case AF_INET:
		ip = mtod(m, struct ip *);
		i = ip->ip_hl << 2;
		off = offsetof(struct ip, ip_p);
		break;
#ifdef INET6
	case AF_INET6:
		i = sizeof(struct ip6_hdr);
		off = offsetof(struct ip6_hdr, ip6_nxt);
		break;
#endif /* INET6 */
	default:
		DPRINTF(("%s: unsupported protocol family %u\n",
		    __func__, dst->sa.sa_family));
		error = EPFNOSUPPORT;
		IPSECSTAT_INC(ips_out_inval);
		goto bad;
	}
	error = (*sav->tdb_xform->xf_output)(m, isr, NULL, i, off);
	IPSECREQUEST_UNLOCK(isr);
	return (error);
bad:
	if (isr)
		IPSECREQUEST_UNLOCK(isr);
	if (m)
		m_freem(m);
	return error;
}
#endif


#ifdef INET6
static int
in6_sa_equal_addrwithscope(const struct sockaddr_in6 *sa, const struct in6_addr *ia)
{
	struct in6_addr ia2;

	memcpy(&ia2, &sa->sin6_addr, sizeof(ia2));
	if (IN6_IS_SCOPE_LINKLOCAL(&sa->sin6_addr))
		ia2.s6_addr16[1] = htons(sa->sin6_scope_id);

	return IN6_ARE_ADDR_EQUAL(ia, &ia2);
}

/*
 * IPsec output logic for IPv6.
 */
int
ipsec6_process_packet(struct mbuf *m, struct ipsecrequest *isr)
{
	char sbuf[INET6_ADDRSTRLEN], dbuf[INET6_ADDRSTRLEN];
	struct ipsec_ctx_data ctx;
	struct secasindex saidx;
	struct secasvar *sav;
	struct ip6_hdr *ip6;
	int error, i, off;
	union sockaddr_union *dst;

	IPSEC_ASSERT(m != NULL, ("ipsec6_process_packet: null mbuf"));
	IPSEC_ASSERT(isr != NULL, ("ipsec6_process_packet: null isr"));

	IPSECREQUEST_LOCK(isr);		/* insure SA contents don't change */

	isr = ipsec_nextisr(m, isr, AF_INET6, &saidx, &error);
	if (isr == NULL) {
		if (error != 0)
			goto bad;
		return EJUSTRETURN;
	}
	sav = isr->sav;
	dst = &sav->sah->saidx.dst;

	IPSEC_INIT_CTX(&ctx, &m, sav, AF_INET6, IPSEC_ENC_BEFORE);
	if ((error = ipsec_run_hhooks(&ctx, HHOOK_TYPE_IPSEC_OUT)) != 0)
		goto bad;

	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_plen = htons(m->m_pkthdr.len - sizeof(*ip6));

	/* Do the appropriate encapsulation, if necessary */
	if (isr->saidx.mode == IPSEC_MODE_TUNNEL || /* Tunnel requ'd */
	    dst->sa.sa_family != AF_INET6 ||        /* PF mismatch */
	    ((dst->sa.sa_family == AF_INET6) &&
	     (!IN6_IS_ADDR_UNSPECIFIED(&dst->sin6.sin6_addr)) &&
	     (!in6_sa_equal_addrwithscope(&dst->sin6,
				  &ip6->ip6_dst)))) {
		if (m->m_pkthdr.len - sizeof(*ip6) > IPV6_MAXPACKET) {
			/* No jumbogram support. */
			error = ENXIO;   /*XXX*/
			goto bad;
		}
		error = ipsec_encap(&m, &sav->sah->saidx);
		if (error != 0) {
			DPRINTF(("%s: encapsulation for SA %s->%s "
			    "SPI 0x%08x failed with error %d\n", __func__,
			    ipsec_address(&sav->sah->saidx.src, sbuf,
				sizeof(sbuf)),
			    ipsec_address(&sav->sah->saidx.dst, dbuf,
				sizeof(dbuf)), ntohl(sav->spi), error));
			goto bad;
		}
	}

	IPSEC_INIT_CTX(&ctx, &m, sav, dst->sa.sa_family, IPSEC_ENC_AFTER);
	if ((error = ipsec_run_hhooks(&ctx, HHOOK_TYPE_IPSEC_OUT)) != 0)
		goto bad;

	switch(dst->sa.sa_family) {
#ifdef INET
	case AF_INET:
		{
		struct ip *ip;
		ip = mtod(m, struct ip *);
		i = ip->ip_hl << 2;
		off = offsetof(struct ip, ip_p);
		}
		break;
#endif /* AF_INET */
	case AF_INET6:
		i = sizeof(struct ip6_hdr);
		off = offsetof(struct ip6_hdr, ip6_nxt);
		break;
	default:
		DPRINTF(("%s: unsupported protocol family %u\n",
				 __func__, dst->sa.sa_family));
		error = EPFNOSUPPORT;
		goto bad;
	}
	error = (*sav->tdb_xform->xf_output)(m, isr, NULL, i, off);
	IPSECREQUEST_UNLOCK(isr);
	return error;
bad:
	IPSEC6STAT_INC(ips_out_inval);
	if (isr)
		IPSECREQUEST_UNLOCK(isr);
	if (m)
		m_freem(m);
	return error;
}
#endif /*INET6*/
