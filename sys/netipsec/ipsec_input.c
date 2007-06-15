/*	$FreeBSD$	*/
/*	$OpenBSD: ipsec_input.c,v 1.63 2003/02/20 18:35:43 deraadt Exp $	*/
/*-
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * This code was written by John Ioannidis for BSD/OS in Athens, Greece,
 * in November 1995.
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
 * IPsec input processing.
 */

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_enc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>

#include <netinet/ip6.h>
#ifdef INET6
#include <netinet6/ip6_var.h>
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
#include <netipsec/esp.h>
#include <netipsec/esp_var.h>
#include <netipsec/ipcomp_var.h>

#include <netipsec/key.h>
#include <netipsec/keydb.h>

#include <netipsec/xform.h>
#include <netinet6/ip6protosw.h>

#include <machine/in_cksum.h>
#include <machine/stdarg.h>

#define IPSEC_ISTAT(p,x,y,z) ((p) == IPPROTO_ESP ? (x)++ : \
			    (p) == IPPROTO_AH ? (y)++ : (z)++)

static void ipsec4_common_ctlinput(int, struct sockaddr *, void *, int);

/*
 * ipsec_common_input gets called when an IPsec-protected packet
 * is received by IPv4 or IPv6.  It's job is to find the right SA
 * and call the appropriate transform.  The transform callback
 * takes care of further processing (like ingress filtering).
 */
static int
ipsec_common_input(struct mbuf *m, int skip, int protoff, int af, int sproto)
{
	union sockaddr_union dst_address;
	struct secasvar *sav;
	u_int32_t spi;
	int error;

	IPSEC_ISTAT(sproto, espstat.esps_input, ahstat.ahs_input,
		ipcompstat.ipcomps_input);

	IPSEC_ASSERT(m != NULL, ("null packet"));

	IPSEC_ASSERT(sproto == IPPROTO_ESP || sproto == IPPROTO_AH ||
		sproto == IPPROTO_IPCOMP,
		("unexpected security protocol %u", sproto));

	if ((sproto == IPPROTO_ESP && !esp_enable) ||
	    (sproto == IPPROTO_AH && !ah_enable) ||
	    (sproto == IPPROTO_IPCOMP && !ipcomp_enable)) {
		m_freem(m);
		IPSEC_ISTAT(sproto, espstat.esps_pdrops, ahstat.ahs_pdrops,
		    ipcompstat.ipcomps_pdrops);
		return EOPNOTSUPP;
	}

	if (m->m_pkthdr.len - skip < 2 * sizeof (u_int32_t)) {
		m_freem(m);
		IPSEC_ISTAT(sproto, espstat.esps_hdrops, ahstat.ahs_hdrops,
		    ipcompstat.ipcomps_hdrops);
		DPRINTF(("%s: packet too small\n", __func__));
		return EINVAL;
	}

	/* Retrieve the SPI from the relevant IPsec header */
	if (sproto == IPPROTO_ESP)
		m_copydata(m, skip, sizeof(u_int32_t), (caddr_t) &spi);
	else if (sproto == IPPROTO_AH)
		m_copydata(m, skip + sizeof(u_int32_t), sizeof(u_int32_t),
		    (caddr_t) &spi);
	else if (sproto == IPPROTO_IPCOMP) {
		u_int16_t cpi;
		m_copydata(m, skip + sizeof(u_int16_t), sizeof(u_int16_t),
		    (caddr_t) &cpi);
		spi = ntohl(htons(cpi));
	}

	/*
	 * Find the SA and (indirectly) call the appropriate
	 * kernel crypto routine. The resulting mbuf chain is a valid
	 * IP packet ready to go through input processing.
	 */
	bzero(&dst_address, sizeof (dst_address));
	dst_address.sa.sa_family = af;
	switch (af) {
#ifdef INET
	case AF_INET:
		dst_address.sin.sin_len = sizeof(struct sockaddr_in);
		m_copydata(m, offsetof(struct ip, ip_dst),
		    sizeof(struct in_addr),
		    (caddr_t) &dst_address.sin.sin_addr);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		dst_address.sin6.sin6_len = sizeof(struct sockaddr_in6);
		m_copydata(m, offsetof(struct ip6_hdr, ip6_dst),
		    sizeof(struct in6_addr),
		    (caddr_t) &dst_address.sin6.sin6_addr);
		break;
#endif /* INET6 */
	default:
		DPRINTF(("%s: unsupported protocol family %u\n", __func__, af));
		m_freem(m);
		IPSEC_ISTAT(sproto, espstat.esps_nopf, ahstat.ahs_nopf,
		    ipcompstat.ipcomps_nopf);
		return EPFNOSUPPORT;
	}

	/* NB: only pass dst since key_allocsa follows RFC2401 */
	sav = KEY_ALLOCSA(&dst_address, sproto, spi);
	if (sav == NULL) {
		DPRINTF(("%s: no key association found for SA %s/%08lx/%u\n",
			  __func__, ipsec_address(&dst_address),
			  (u_long) ntohl(spi), sproto));
		IPSEC_ISTAT(sproto, espstat.esps_notdb, ahstat.ahs_notdb,
		    ipcompstat.ipcomps_notdb);
		m_freem(m);
		return ENOENT;
	}

	if (sav->tdb_xform == NULL) {
		DPRINTF(("%s: attempted to use uninitialized SA %s/%08lx/%u\n",
			 __func__, ipsec_address(&dst_address),
			 (u_long) ntohl(spi), sproto));
		IPSEC_ISTAT(sproto, espstat.esps_noxform, ahstat.ahs_noxform,
		    ipcompstat.ipcomps_noxform);
		KEY_FREESAV(&sav);
		m_freem(m);
		return ENXIO;
	}

	/*
	 * Call appropriate transform and return -- callback takes care of
	 * everything else.
	 */
	error = (*sav->tdb_xform->xf_input)(m, sav, skip, protoff);
	KEY_FREESAV(&sav);
	return error;
}

#ifdef INET
/*
 * Common input handler for IPv4 AH, ESP, and IPCOMP.
 */
int
ipsec4_common_input(struct mbuf *m, ...)
{
	va_list ap;
	int off, nxt;

	va_start(ap, m);
	off = va_arg(ap, int);
	nxt = va_arg(ap, int);
	va_end(ap);

	return ipsec_common_input(m, off, offsetof(struct ip, ip_p),
				  AF_INET, nxt);
}

void
ah4_input(struct mbuf *m, int off)
{
	ipsec4_common_input(m, off, IPPROTO_AH);
}
void
ah4_ctlinput(int cmd, struct sockaddr *sa, void *v)
{
	if (sa->sa_family == AF_INET &&
	    sa->sa_len == sizeof(struct sockaddr_in))
		ipsec4_common_ctlinput(cmd, sa, v, IPPROTO_AH);
}

void
esp4_input(struct mbuf *m, int off)
{
	ipsec4_common_input(m, off, IPPROTO_ESP);
}
void
esp4_ctlinput(int cmd, struct sockaddr *sa, void *v)
{
	if (sa->sa_family == AF_INET &&
	    sa->sa_len == sizeof(struct sockaddr_in))
		ipsec4_common_ctlinput(cmd, sa, v, IPPROTO_ESP);
}

void
ipcomp4_input(struct mbuf *m, int off)
{
	ipsec4_common_input(m, off, IPPROTO_IPCOMP);
}

/*
 * IPsec input callback for INET protocols.
 * This routine is called as the transform callback.
 * Takes care of filtering and other sanity checks on
 * the processed packet.
 */
int
ipsec4_common_input_cb(struct mbuf *m, struct secasvar *sav,
			int skip, int protoff, struct m_tag *mt)
{
	int prot, af, sproto;
	struct ip *ip;
	struct m_tag *mtag;
	struct tdb_ident *tdbi;
	struct secasindex *saidx;
	int error;
#ifdef INET6
#ifdef notyet
	char ip6buf[INET6_ADDRSTRLEN];
#endif
#endif

	IPSEC_SPLASSERT_SOFTNET(__func__);

	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(sav != NULL, ("null SA"));
	IPSEC_ASSERT(sav->sah != NULL, ("null SAH"));
	saidx = &sav->sah->saidx;
	af = saidx->dst.sa.sa_family;
	IPSEC_ASSERT(af == AF_INET, ("unexpected af %u", af));
	sproto = saidx->proto;
	IPSEC_ASSERT(sproto == IPPROTO_ESP || sproto == IPPROTO_AH ||
		sproto == IPPROTO_IPCOMP,
		("unexpected security protocol %u", sproto));

	/* Sanity check */
	if (m == NULL) {
		DPRINTF(("%s: null mbuf", __func__));
		IPSEC_ISTAT(sproto, espstat.esps_badkcr, ahstat.ahs_badkcr,
		    ipcompstat.ipcomps_badkcr);
		KEY_FREESAV(&sav);
		return EINVAL;
	}

	if (skip != 0) {
		/* Fix IPv4 header */
		if (m->m_len < skip && (m = m_pullup(m, skip)) == NULL) {
			DPRINTF(("%s: processing failed for SA %s/%08lx\n",
			    __func__, ipsec_address(&sav->sah->saidx.dst),
			    (u_long) ntohl(sav->spi)));
			IPSEC_ISTAT(sproto, espstat.esps_hdrops, ahstat.ahs_hdrops,
			    ipcompstat.ipcomps_hdrops);
			error = ENOBUFS;
			goto bad;
		}

		ip = mtod(m, struct ip *);
		ip->ip_len = htons(m->m_pkthdr.len);
		ip->ip_off = htons(ip->ip_off);
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(m, ip->ip_hl << 2);
	} else {
		ip = mtod(m, struct ip *);
	}
	prot = ip->ip_p;

#ifdef notyet
	/* IP-in-IP encapsulation */
	if (prot == IPPROTO_IPIP) {
		struct ip ipn;

		if (m->m_pkthdr.len - skip < sizeof(struct ip)) {
			IPSEC_ISTAT(sproto, espstat.esps_hdrops,
			    ahstat.ahs_hdrops,
			    ipcompstat.ipcomps_hdrops);
			error = EINVAL;
			goto bad;
		}
		/* ipn will now contain the inner IPv4 header */
		m_copydata(m, ip->ip_hl << 2, sizeof(struct ip),
		    (caddr_t) &ipn);

		/* XXX PROXY address isn't recorded in SAH */
		/*
		 * Check that the inner source address is the same as
		 * the proxy address, if available.
		 */
		if ((saidx->proxy.sa.sa_family == AF_INET &&
		    saidx->proxy.sin.sin_addr.s_addr !=
		    INADDR_ANY &&
		    ipn.ip_src.s_addr !=
		    saidx->proxy.sin.sin_addr.s_addr) ||
		    (saidx->proxy.sa.sa_family != AF_INET &&
			saidx->proxy.sa.sa_family != 0)) {

			DPRINTF(("%s: inner source address %s doesn't "
			    "correspond to expected proxy source %s, "
			    "SA %s/%08lx\n", __func__,
			    inet_ntoa4(ipn.ip_src),
			    ipsp_address(saidx->proxy),
			    ipsp_address(saidx->dst),
			    (u_long) ntohl(sav->spi)));

			IPSEC_ISTAT(sproto, espstat.esps_pdrops,
			    ahstat.ahs_pdrops,
			    ipcompstat.ipcomps_pdrops);
			error = EACCES;
			goto bad;
		}
	}
#ifdef INET6
	/* IPv6-in-IP encapsulation. */
	if (prot == IPPROTO_IPV6) {
		struct ip6_hdr ip6n;

		if (m->m_pkthdr.len - skip < sizeof(struct ip6_hdr)) {
			IPSEC_ISTAT(sproto, espstat.esps_hdrops,
			    ahstat.ahs_hdrops,
			    ipcompstat.ipcomps_hdrops);
			error = EINVAL;
			goto bad;
		}
		/* ip6n will now contain the inner IPv6 header. */
		m_copydata(m, ip->ip_hl << 2, sizeof(struct ip6_hdr),
		    (caddr_t) &ip6n);

		/*
		 * Check that the inner source address is the same as
		 * the proxy address, if available.
		 */
		if ((saidx->proxy.sa.sa_family == AF_INET6 &&
		    !IN6_IS_ADDR_UNSPECIFIED(&saidx->proxy.sin6.sin6_addr) &&
		    !IN6_ARE_ADDR_EQUAL(&ip6n.ip6_src,
			&saidx->proxy.sin6.sin6_addr)) ||
		    (saidx->proxy.sa.sa_family != AF_INET6 &&
			saidx->proxy.sa.sa_family != 0)) {

			DPRINTF(("%s: inner source address %s doesn't "
			    "correspond to expected proxy source %s, "
			    "SA %s/%08lx\n", __func__,
			    ip6_sprintf(ip6buf, &ip6n.ip6_src),
			    ipsec_address(&saidx->proxy),
			    ipsec_address(&saidx->dst),
			    (u_long) ntohl(sav->spi)));

			IPSEC_ISTAT(sproto, espstat.esps_pdrops,
			    ahstat.ahs_pdrops,
			    ipcompstat.ipcomps_pdrops);
			error = EACCES;
			goto bad;
		}
	}
#endif /* INET6 */
#endif /*XXX*/

	/*
	 * Record what we've done to the packet (under what SA it was
	 * processed). If we've been passed an mtag, it means the packet
	 * was already processed by an ethernet/crypto combo card and
	 * thus has a tag attached with all the right information, but
	 * with a PACKET_TAG_IPSEC_IN_CRYPTO_DONE as opposed to
	 * PACKET_TAG_IPSEC_IN_DONE type; in that case, just change the type.
	 */
	if (mt == NULL && sproto != IPPROTO_IPCOMP) {
		mtag = m_tag_get(PACKET_TAG_IPSEC_IN_DONE,
		    sizeof(struct tdb_ident), M_NOWAIT);
		if (mtag == NULL) {
			DPRINTF(("%s: failed to get tag\n", __func__));
			IPSEC_ISTAT(sproto, espstat.esps_hdrops,
			    ahstat.ahs_hdrops, ipcompstat.ipcomps_hdrops);
			error = ENOMEM;
			goto bad;
		}

		tdbi = (struct tdb_ident *)(mtag + 1);
		bcopy(&saidx->dst, &tdbi->dst, saidx->dst.sa.sa_len);
		tdbi->proto = sproto;
		tdbi->spi = sav->spi;

		m_tag_prepend(m, mtag);
	} else {
		mt->m_tag_id = PACKET_TAG_IPSEC_IN_DONE;
		/* XXX do we need to mark m_flags??? */
	}

	key_sa_recordxfer(sav, m);		/* record data transfer */

#ifdef DEV_ENC
	/*
	 * Pass the mbuf to enc0 for bpf and pfil. We will filter the IPIP
	 * packet later after it has been decapsulated.
	 */
	ipsec_bpf(m, sav, AF_INET);

	if (prot != IPPROTO_IPIP)
		if ((error = ipsec_filter(&m, 1)) != 0)
			return (error);
#endif

	/*
	 * Re-dispatch via software interrupt.
	 */
	if ((error = netisr_queue(NETISR_IP, m))) {
		IPSEC_ISTAT(sproto, espstat.esps_qfull, ahstat.ahs_qfull,
			    ipcompstat.ipcomps_qfull);

		DPRINTF(("%s: queue full; proto %u packet dropped\n",
			__func__, sproto));
		return error;
	}
	return 0;
bad:
	m_freem(m);
	return error;
}

void
ipsec4_common_ctlinput(int cmd, struct sockaddr *sa, void *v, int proto)
{
	/* XXX nothing just yet */
}
#endif /* INET */

#ifdef INET6
/* IPv6 AH wrapper. */
int
ipsec6_common_input(struct mbuf **mp, int *offp, int proto)
{
	int l = 0;
	int protoff;
	struct ip6_ext ip6e;

	if (*offp < sizeof(struct ip6_hdr)) {
		DPRINTF(("%s: bad offset %u\n", __func__, *offp));
		return IPPROTO_DONE;
	} else if (*offp == sizeof(struct ip6_hdr)) {
		protoff = offsetof(struct ip6_hdr, ip6_nxt);
	} else {
		/* Chase down the header chain... */
		protoff = sizeof(struct ip6_hdr);

		do {
			protoff += l;
			m_copydata(*mp, protoff, sizeof(ip6e),
			    (caddr_t) &ip6e);

			if (ip6e.ip6e_nxt == IPPROTO_AH)
				l = (ip6e.ip6e_len + 2) << 2;
			else
				l = (ip6e.ip6e_len + 1) << 3;
			IPSEC_ASSERT(l > 0, ("l went zero or negative"));
		} while (protoff + l < *offp);

		/* Malformed packet check */
		if (protoff + l != *offp) {
			DPRINTF(("%s: bad packet header chain, protoff %u, "
				"l %u, off %u\n", __func__, protoff, l, *offp));
			IPSEC_ISTAT(proto, espstat.esps_hdrops,
				    ahstat.ahs_hdrops,
				    ipcompstat.ipcomps_hdrops);
			m_freem(*mp);
			*mp = NULL;
			return IPPROTO_DONE;
		}
		protoff += offsetof(struct ip6_ext, ip6e_nxt);
	}
	(void) ipsec_common_input(*mp, *offp, protoff, AF_INET6, proto);
	return IPPROTO_DONE;
}

/*
 * IPsec input callback, called by the transform callback. Takes care of
 * filtering and other sanity checks on the processed packet.
 */
int
ipsec6_common_input_cb(struct mbuf *m, struct secasvar *sav, int skip, int protoff,
    struct m_tag *mt)
{
	int prot, af, sproto;
	struct ip6_hdr *ip6;
	struct m_tag *mtag;
	struct tdb_ident *tdbi;
	struct secasindex *saidx;
	int nxt;
	u_int8_t nxt8;
	int error, nest;
#ifdef notyet
	char ip6buf[INET6_ADDRSTRLEN];
#endif

	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(sav != NULL, ("null SA"));
	IPSEC_ASSERT(sav->sah != NULL, ("null SAH"));
	saidx = &sav->sah->saidx;
	af = saidx->dst.sa.sa_family;
	IPSEC_ASSERT(af == AF_INET6, ("unexpected af %u", af));
	sproto = saidx->proto;
	IPSEC_ASSERT(sproto == IPPROTO_ESP || sproto == IPPROTO_AH ||
		sproto == IPPROTO_IPCOMP,
		("unexpected security protocol %u", sproto));

	/* Sanity check */
	if (m == NULL) {
		DPRINTF(("%s: null mbuf", __func__));
		IPSEC_ISTAT(sproto, espstat.esps_badkcr, ahstat.ahs_badkcr,
		    ipcompstat.ipcomps_badkcr);
		error = EINVAL;
		goto bad;
	}

	/* Fix IPv6 header */
	if (m->m_len < sizeof(struct ip6_hdr) &&
	    (m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL) {

		DPRINTF(("%s: processing failed for SA %s/%08lx\n",
		    __func__, ipsec_address(&sav->sah->saidx.dst),
		    (u_long) ntohl(sav->spi)));

		IPSEC_ISTAT(sproto, espstat.esps_hdrops, ahstat.ahs_hdrops,
		    ipcompstat.ipcomps_hdrops);
		error = EACCES;
		goto bad;
	}

	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_plen = htons(m->m_pkthdr.len - sizeof(struct ip6_hdr));

	/* Save protocol */
	m_copydata(m, protoff, 1, (unsigned char *) &prot);

#ifdef notyet
#ifdef INET
	/* IP-in-IP encapsulation */
	if (prot == IPPROTO_IPIP) {
		struct ip ipn;

		if (m->m_pkthdr.len - skip < sizeof(struct ip)) {
			IPSEC_ISTAT(sproto, espstat.esps_hdrops,
			    ahstat.ahs_hdrops,
			    ipcompstat.ipcomps_hdrops);
			error = EINVAL;
			goto bad;
		}
		/* ipn will now contain the inner IPv4 header */
		m_copydata(m, skip, sizeof(struct ip), (caddr_t) &ipn);

		/*
		 * Check that the inner source address is the same as
		 * the proxy address, if available.
		 */
		if ((saidx->proxy.sa.sa_family == AF_INET &&
		    saidx->proxy.sin.sin_addr.s_addr != INADDR_ANY &&
		    ipn.ip_src.s_addr != saidx->proxy.sin.sin_addr.s_addr) ||
		    (saidx->proxy.sa.sa_family != AF_INET &&
			saidx->proxy.sa.sa_family != 0)) {

			DPRINTF(("%s: inner source address %s doesn't "
			    "correspond to expected proxy source %s, "
			    "SA %s/%08lx\n", __func__,
			    inet_ntoa4(ipn.ip_src),
			    ipsec_address(&saidx->proxy),
			    ipsec_address(&saidx->dst),
			    (u_long) ntohl(sav->spi)));

			IPSEC_ISTATsproto, (espstat.esps_pdrops,
			    ahstat.ahs_pdrops, ipcompstat.ipcomps_pdrops);
			error = EACCES;
			goto bad;
		}
	}
#endif /* INET */

	/* IPv6-in-IP encapsulation */
	if (prot == IPPROTO_IPV6) {
		struct ip6_hdr ip6n;

		if (m->m_pkthdr.len - skip < sizeof(struct ip6_hdr)) {
			IPSEC_ISTAT(sproto, espstat.esps_hdrops,
			    ahstat.ahs_hdrops,
			    ipcompstat.ipcomps_hdrops);
			error = EINVAL;
			goto bad;
		}
		/* ip6n will now contain the inner IPv6 header. */
		m_copydata(m, skip, sizeof(struct ip6_hdr),
		    (caddr_t) &ip6n);

		/*
		 * Check that the inner source address is the same as
		 * the proxy address, if available.
		 */
		if ((saidx->proxy.sa.sa_family == AF_INET6 &&
		    !IN6_IS_ADDR_UNSPECIFIED(&saidx->proxy.sin6.sin6_addr) &&
		    !IN6_ARE_ADDR_EQUAL(&ip6n.ip6_src,
			&saidx->proxy.sin6.sin6_addr)) ||
		    (saidx->proxy.sa.sa_family != AF_INET6 &&
			saidx->proxy.sa.sa_family != 0)) {

			DPRINTF(("%s: inner source address %s doesn't "
			    "correspond to expected proxy source %s, "
			    "SA %s/%08lx\n", __func__,
			    ip6_sprintf(ip6buf, &ip6n.ip6_src),
			    ipsec_address(&saidx->proxy),
			    ipsec_address(&saidx->dst),
			    (u_long) ntohl(sav->spi)));

			IPSEC_ISTAT(sproto, espstat.esps_pdrops,
			    ahstat.ahs_pdrops, ipcompstat.ipcomps_pdrops);
			error = EACCES;
			goto bad;
		}
	}
#endif /*XXX*/

	/*
	 * Record what we've done to the packet (under what SA it was
	 * processed). If we've been passed an mtag, it means the packet
	 * was already processed by an ethernet/crypto combo card and
	 * thus has a tag attached with all the right information, but
	 * with a PACKET_TAG_IPSEC_IN_CRYPTO_DONE as opposed to
	 * PACKET_TAG_IPSEC_IN_DONE type; in that case, just change the type.
	 */
	if (mt == NULL && sproto != IPPROTO_IPCOMP) {
		mtag = m_tag_get(PACKET_TAG_IPSEC_IN_DONE,
		    sizeof(struct tdb_ident), M_NOWAIT);
		if (mtag == NULL) {
			DPRINTF(("%s: failed to get tag\n", __func__));
			IPSEC_ISTAT(sproto, espstat.esps_hdrops,
			    ahstat.ahs_hdrops, ipcompstat.ipcomps_hdrops);
			error = ENOMEM;
			goto bad;
		}

		tdbi = (struct tdb_ident *)(mtag + 1);
		bcopy(&saidx->dst, &tdbi->dst, sizeof(union sockaddr_union));
		tdbi->proto = sproto;
		tdbi->spi = sav->spi;

		m_tag_prepend(m, mtag);
	} else {
		if (mt != NULL)
			mt->m_tag_id = PACKET_TAG_IPSEC_IN_DONE;
		/* XXX do we need to mark m_flags??? */
	}

	key_sa_recordxfer(sav, m);

	/* Retrieve new protocol */
	m_copydata(m, protoff, sizeof(u_int8_t), (caddr_t) &nxt8);

	/*
	 * See the end of ip6_input for this logic.
	 * IPPROTO_IPV[46] case will be processed just like other ones
	 */
	nest = 0;
	nxt = nxt8;
	while (nxt != IPPROTO_DONE) {
		if (ip6_hdrnestlimit && (++nest > ip6_hdrnestlimit)) {
			ip6stat.ip6s_toomanyhdr++;
			error = EINVAL;
			goto bad;
		}

		/*
		 * Protection against faulty packet - there should be
		 * more sanity checks in header chain processing.
		 */
		if (m->m_pkthdr.len < skip) {
			ip6stat.ip6s_tooshort++;
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_truncated);
			error = EINVAL;
			goto bad;
		}
		/*
		 * Enforce IPsec policy checking if we are seeing last header.
		 * note that we do not visit this with protocols with pcb layer
		 * code - like udp/tcp/raw ip.
		 */
		if ((inet6sw[ip6_protox[nxt]].pr_flags & PR_LASTHDR) != 0 &&
		    ipsec6_in_reject(m, NULL)) {
			error = EINVAL;
			goto bad;
		}
		nxt = (*inet6sw[ip6_protox[nxt]].pr_input)(&m, &skip, nxt);
	}
	return 0;
bad:
	if (m)
		m_freem(m);
	return error;
}

void
esp6_ctlinput(int cmd, struct sockaddr *sa, void *d)
{
	if (sa->sa_family != AF_INET6 ||
	    sa->sa_len != sizeof(struct sockaddr_in6))
		return;
	if ((unsigned)cmd >= PRC_NCMDS)
		return;

	/* if the parameter is from icmp6, decode it. */
	if (d !=  NULL) {
		struct ip6ctlparam *ip6cp = (struct ip6ctlparam *)d;
		struct mbuf *m = ip6cp->ip6c_m;
		int off = ip6cp->ip6c_off;

		struct ip6ctlparam ip6cp1;

		/*
		 * Notify the error to all possible sockets via pfctlinput2.
		 * Since the upper layer information (such as protocol type,
		 * source and destination ports) is embedded in the encrypted
		 * data and might have been cut, we can't directly call
		 * an upper layer ctlinput function. However, the pcbnotify
		 * function will consider source and destination addresses
		 * as well as the flow info value, and may be able to find
		 * some PCB that should be notified.
		 * Although pfctlinput2 will call esp6_ctlinput(), there is
		 * no possibility of an infinite loop of function calls,
		 * because we don't pass the inner IPv6 header.
		 */
		bzero(&ip6cp1, sizeof(ip6cp1));
		ip6cp1.ip6c_src = ip6cp->ip6c_src;
		pfctlinput2(cmd, sa, (void *)&ip6cp1);

		/*
		 * Then go to special cases that need ESP header information.
		 * XXX: We assume that when ip6 is non NULL,
		 * M and OFF are valid.
		 */

		if (cmd == PRC_MSGSIZE) {
			struct secasvar *sav;
			u_int32_t spi;
			int valid;

			/* check header length before using m_copydata */
			if (m->m_pkthdr.len < off + sizeof (struct esp))
				return;
			m_copydata(m, off + offsetof(struct esp, esp_spi),
				sizeof(u_int32_t), (caddr_t) &spi);
			/*
			 * Check to see if we have a valid SA corresponding to
			 * the address in the ICMP message payload.
			 */
			sav = KEY_ALLOCSA((union sockaddr_union *)sa,
					IPPROTO_ESP, spi);
			valid = (sav != NULL);
			if (sav)
				KEY_FREESAV(&sav);

			/* XXX Further validation? */

			/*
			 * Depending on whether the SA is "valid" and
			 * routing table size (mtudisc_{hi,lo}wat), we will:
			 * - recalcurate the new MTU and create the
			 *   corresponding routing entry, or
			 * - ignore the MTU change notification.
			 */
			icmp6_mtudisc_update(ip6cp, valid);
		}
	} else {
		/* we normally notify any pcb here */
	}
}
#endif /* INET6 */
