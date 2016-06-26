/*	$FreeBSD$	*/
/* $OpenBSD: ip_ipcomp.c,v 1.1 2001/07/05 12:08:52 jjbg Exp $ */

/*-
 * Copyright (c) 2001 Jean-Jacques Bernard-Gundol (jj@wabbitt.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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

/* IP payload compression protocol (IPComp), see RFC 2393 */
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/protosw.h>
#include <sys/sysctl.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_encap.h>

#include <net/netisr.h>
#include <net/vnet.h>

#include <netipsec/ipsec.h>
#include <netipsec/xform.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netipsec/ipsec6.h>
#endif

#include <netipsec/ipcomp.h>
#include <netipsec/ipcomp_var.h>

#include <netipsec/key.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/deflate.h>
#include <opencrypto/xform.h>

VNET_DEFINE(int, ipcomp_enable) = 1;
VNET_PCPUSTAT_DEFINE(struct ipcompstat, ipcompstat);
VNET_PCPUSTAT_SYSINIT(ipcompstat);

#ifdef VIMAGE
VNET_PCPUSTAT_SYSUNINIT(ipcompstat);
#endif /* VIMAGE */

SYSCTL_DECL(_net_inet_ipcomp);
SYSCTL_INT(_net_inet_ipcomp, OID_AUTO, ipcomp_enable,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ipcomp_enable), 0, "");
SYSCTL_VNET_PCPUSTAT(_net_inet_ipcomp, IPSECCTL_STATS, stats,
    struct ipcompstat, ipcompstat,
    "IPCOMP statistics (struct ipcompstat, netipsec/ipcomp_var.h");

static int ipcomp_input_cb(struct cryptop *crp);
static int ipcomp_output_cb(struct cryptop *crp);

struct comp_algo *
ipcomp_algorithm_lookup(int alg)
{
	if (alg >= IPCOMP_ALG_MAX)
		return NULL;
	switch (alg) {
	case SADB_X_CALG_DEFLATE:
		return &comp_algo_deflate;
	}
	return NULL;
}

/*
 * RFC 3173 p 2.2. Non-Expansion Policy:
 * If the total size of a compressed payload and the IPComp header, as
 * defined in section 3, is not smaller than the size of the original
 * payload, the IP datagram MUST be sent in the original non-compressed
 * form.
 *
 * When we use IPComp in tunnel mode, for small packets we will receive
 * encapsulated IP-IP datagrams without any compression and without IPComp
 * header.
 */
static int
ipcomp_encapcheck(union sockaddr_union *src, union sockaddr_union *dst)
{
	struct secasvar *sav;

	sav = KEY_ALLOCSA_TUNNEL(src, dst, IPPROTO_IPCOMP);
	if (sav == NULL)
		return (0);
	KEY_FREESAV(&sav);

	if (src->sa.sa_family == AF_INET)
		return (sizeof(struct in_addr) << 4);
	else
		return (sizeof(struct in6_addr) << 4);
}

static int
ipcomp_nonexp_input(struct mbuf **mp, int *offp, int proto)
{
	int isr;

	switch (proto) {
#ifdef INET
	case IPPROTO_IPV4:
		isr = NETISR_IP;
		break;
#endif
#ifdef INET6
	case IPPROTO_IPV6:
		isr = NETISR_IPV6;
		break;
#endif
	default:
		IPCOMPSTAT_INC(ipcomps_nopf);
		m_freem(*mp);
		return (IPPROTO_DONE);
	}
	m_adj(*mp, *offp);
	IPCOMPSTAT_ADD(ipcomps_ibytes, (*mp)->m_pkthdr.len);
	IPCOMPSTAT_INC(ipcomps_input);
	netisr_dispatch(isr, *mp);
	return (IPPROTO_DONE);
}

/*
 * ipcomp_init() is called when an CPI is being set up.
 */
static int
ipcomp_init(struct secasvar *sav, struct xformsw *xsp)
{
	struct comp_algo *tcomp;
	struct cryptoini cric;

	/* NB: algorithm really comes in alg_enc and not alg_comp! */
	tcomp = ipcomp_algorithm_lookup(sav->alg_enc);
	if (tcomp == NULL) {
		DPRINTF(("%s: unsupported compression algorithm %d\n", __func__,
			 sav->alg_comp));
		return EINVAL;
	}
	sav->alg_comp = sav->alg_enc;		/* set for doing histogram */
	sav->tdb_xform = xsp;
	sav->tdb_compalgxform = tcomp;

	/* Initialize crypto session */
	bzero(&cric, sizeof (cric));
	cric.cri_alg = sav->tdb_compalgxform->type;

	return crypto_newsession(&sav->tdb_cryptoid, &cric, V_crypto_support);
}

/*
 * ipcomp_zeroize() used when IPCA is deleted
 */
static int
ipcomp_zeroize(struct secasvar *sav)
{
	int err;

	err = crypto_freesession(sav->tdb_cryptoid);
	sav->tdb_cryptoid = 0;
	return err;
}

/*
 * ipcomp_input() gets called to uncompress an input packet
 */
static int
ipcomp_input(struct mbuf *m, struct secasvar *sav, int skip, int protoff)
{
	struct tdb_crypto *tc;
	struct cryptodesc *crdc;
	struct cryptop *crp;
	struct ipcomp *ipcomp;
	caddr_t addr;
	int hlen = IPCOMP_HLENGTH;

	/*
	 * Check that the next header of the IPComp is not IPComp again, before
	 * doing any real work.  Given it is not possible to do double
	 * compression it means someone is playing tricks on us.
	 */
	if (m->m_len < skip + hlen && (m = m_pullup(m, skip + hlen)) == NULL) {
		IPCOMPSTAT_INC(ipcomps_hdrops);		/*XXX*/
		DPRINTF(("%s: m_pullup failed\n", __func__));
		return (ENOBUFS);
	}
	addr = (caddr_t) mtod(m, struct ip *) + skip;
	ipcomp = (struct ipcomp *)addr;
	if (ipcomp->comp_nxt == IPPROTO_IPCOMP) {
		m_freem(m);
		IPCOMPSTAT_INC(ipcomps_pdrops);	/* XXX have our own stats? */
		DPRINTF(("%s: recursive compression detected\n", __func__));
		return (EINVAL);
	}

	/* Get crypto descriptors */
	crp = crypto_getreq(1);
	if (crp == NULL) {
		m_freem(m);
		DPRINTF(("%s: no crypto descriptors\n", __func__));
		IPCOMPSTAT_INC(ipcomps_crypto);
		return ENOBUFS;
	}
	/* Get IPsec-specific opaque pointer */
	tc = (struct tdb_crypto *) malloc(sizeof (*tc), M_XDATA, M_NOWAIT|M_ZERO);
	if (tc == NULL) {
		m_freem(m);
		crypto_freereq(crp);
		DPRINTF(("%s: cannot allocate tdb_crypto\n", __func__));
		IPCOMPSTAT_INC(ipcomps_crypto);
		return ENOBUFS;
	}
	crdc = crp->crp_desc;

	crdc->crd_skip = skip + hlen;
	crdc->crd_len = m->m_pkthdr.len - (skip + hlen);
	crdc->crd_inject = skip;

	tc->tc_ptr = 0;

	/* Decompression operation */
	crdc->crd_alg = sav->tdb_compalgxform->type;

	/* Crypto operation descriptor */
	crp->crp_ilen = m->m_pkthdr.len - (skip + hlen);
	crp->crp_flags = CRYPTO_F_IMBUF | CRYPTO_F_CBIFSYNC;
	crp->crp_buf = (caddr_t) m;
	crp->crp_callback = ipcomp_input_cb;
	crp->crp_sid = sav->tdb_cryptoid;
	crp->crp_opaque = (caddr_t) tc;

	/* These are passed as-is to the callback */
	tc->tc_spi = sav->spi;
	tc->tc_dst = sav->sah->saidx.dst;
	tc->tc_proto = sav->sah->saidx.proto;
	tc->tc_protoff = protoff;
	tc->tc_skip = skip;
	KEY_ADDREFSA(sav);
	tc->tc_sav = sav;

	return crypto_dispatch(crp);
}

/*
 * IPComp input callback from the crypto driver.
 */
static int
ipcomp_input_cb(struct cryptop *crp)
{
	char buf[INET6_ADDRSTRLEN];
	struct cryptodesc *crd;
	struct tdb_crypto *tc;
	int skip, protoff;
	struct mbuf *m;
	struct secasvar *sav;
	struct secasindex *saidx;
	int hlen = IPCOMP_HLENGTH, error, clen;
	u_int8_t nproto;
	caddr_t addr;

	crd = crp->crp_desc;

	tc = (struct tdb_crypto *) crp->crp_opaque;
	IPSEC_ASSERT(tc != NULL, ("null opaque crypto data area!"));
	skip = tc->tc_skip;
	protoff = tc->tc_protoff;
	m = (struct mbuf *) crp->crp_buf;

	sav = tc->tc_sav;
	IPSEC_ASSERT(sav != NULL, ("null SA!"));

	saidx = &sav->sah->saidx;
	IPSEC_ASSERT(saidx->dst.sa.sa_family == AF_INET ||
		saidx->dst.sa.sa_family == AF_INET6,
		("unexpected protocol family %u", saidx->dst.sa.sa_family));

	/* Check for crypto errors */
	if (crp->crp_etype) {
		/* Reset the session ID */
		if (sav->tdb_cryptoid != 0)
			sav->tdb_cryptoid = crp->crp_sid;

		if (crp->crp_etype == EAGAIN) {
			return crypto_dispatch(crp);
		}
		IPCOMPSTAT_INC(ipcomps_noxform);
		DPRINTF(("%s: crypto error %d\n", __func__, crp->crp_etype));
		error = crp->crp_etype;
		goto bad;
	}
	/* Shouldn't happen... */
	if (m == NULL) {
		IPCOMPSTAT_INC(ipcomps_crypto);
		DPRINTF(("%s: null mbuf returned from crypto\n", __func__));
		error = EINVAL;
		goto bad;
	}
	IPCOMPSTAT_INC(ipcomps_hist[sav->alg_comp]);

	clen = crp->crp_olen;		/* Length of data after processing */

	/* Release the crypto descriptors */
	free(tc, M_XDATA), tc = NULL;
	crypto_freereq(crp), crp = NULL;

	/* In case it's not done already, adjust the size of the mbuf chain */
	m->m_pkthdr.len = clen + hlen + skip;

	if (m->m_len < skip + hlen && (m = m_pullup(m, skip + hlen)) == NULL) {
		IPCOMPSTAT_INC(ipcomps_hdrops);		/*XXX*/
		DPRINTF(("%s: m_pullup failed\n", __func__));
		error = EINVAL;				/*XXX*/
		goto bad;
	}

	/* Keep the next protocol field */
	addr = (caddr_t) mtod(m, struct ip *) + skip;
	nproto = ((struct ipcomp *) addr)->comp_nxt;

	/* Remove the IPCOMP header */
	error = m_striphdr(m, skip, hlen);
	if (error) {
		IPCOMPSTAT_INC(ipcomps_hdrops);
		DPRINTF(("%s: bad mbuf chain, IPCA %s/%08lx\n", __func__,
		    ipsec_address(&sav->sah->saidx.dst, buf, sizeof(buf)),
		    (u_long) ntohl(sav->spi)));
		goto bad;
	}

	/* Restore the Next Protocol field */
	m_copyback(m, protoff, sizeof (u_int8_t), (u_int8_t *) &nproto);

	switch (saidx->dst.sa.sa_family) {
#ifdef INET6
	case AF_INET6:
		error = ipsec6_common_input_cb(m, sav, skip, protoff);
		break;
#endif
#ifdef INET
	case AF_INET:
		error = ipsec4_common_input_cb(m, sav, skip, protoff);
		break;
#endif
	default:
		panic("%s: Unexpected address family: %d saidx=%p", __func__,
		    saidx->dst.sa.sa_family, saidx);
	}

	KEY_FREESAV(&sav);
	return error;
bad:
	if (sav)
		KEY_FREESAV(&sav);
	if (m)
		m_freem(m);
	if (tc != NULL)
		free(tc, M_XDATA);
	if (crp)
		crypto_freereq(crp);
	return error;
}

/*
 * IPComp output routine, called by ipsec[46]_process_packet()
 */
static int
ipcomp_output(struct mbuf *m, struct ipsecrequest *isr, struct mbuf **mp,
    int skip, int protoff)
{
	char buf[INET6_ADDRSTRLEN];
	struct secasvar *sav;
	struct comp_algo *ipcompx;
	int error, ralen, maxpacketsize;
	struct cryptodesc *crdc;
	struct cryptop *crp;
	struct tdb_crypto *tc;

	sav = isr->sav;
	IPSEC_ASSERT(sav != NULL, ("null SA"));
	ipcompx = sav->tdb_compalgxform;
	IPSEC_ASSERT(ipcompx != NULL, ("null compression xform"));

	/*
	 * Do not touch the packet in case our payload to compress
	 * is lower than the minimal threshold of the compression
	 * alogrithm.  We will just send out the data uncompressed.
	 * See RFC 3173, 2.2. Non-Expansion Policy.
	 */
	if (m->m_pkthdr.len <= ipcompx->minlen) {
		IPCOMPSTAT_INC(ipcomps_threshold);
		return ipsec_process_done(m, isr);
	}

	ralen = m->m_pkthdr.len - skip;	/* Raw payload length before comp. */
	IPCOMPSTAT_INC(ipcomps_output);

	/* Check for maximum packet size violations. */
	switch (sav->sah->saidx.dst.sa.sa_family) {
#ifdef INET
	case AF_INET:
		maxpacketsize = IP_MAXPACKET;
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		maxpacketsize = IPV6_MAXPACKET;
		break;
#endif /* INET6 */
	default:
		IPCOMPSTAT_INC(ipcomps_nopf);
		DPRINTF(("%s: unknown/unsupported protocol family %d, "
		    "IPCA %s/%08lx\n", __func__,
		    sav->sah->saidx.dst.sa.sa_family,
		    ipsec_address(&sav->sah->saidx.dst, buf, sizeof(buf)),
		    (u_long) ntohl(sav->spi)));
		error = EPFNOSUPPORT;
		goto bad;
	}
	if (ralen + skip + IPCOMP_HLENGTH > maxpacketsize) {
		IPCOMPSTAT_INC(ipcomps_toobig);
		DPRINTF(("%s: packet in IPCA %s/%08lx got too big "
		    "(len %u, max len %u)\n", __func__,
		    ipsec_address(&sav->sah->saidx.dst, buf, sizeof(buf)),
		    (u_long) ntohl(sav->spi),
		    ralen + skip + IPCOMP_HLENGTH, maxpacketsize));
		error = EMSGSIZE;
		goto bad;
	}

	/* Update the counters */
	IPCOMPSTAT_ADD(ipcomps_obytes, m->m_pkthdr.len - skip);

	m = m_unshare(m, M_NOWAIT);
	if (m == NULL) {
		IPCOMPSTAT_INC(ipcomps_hdrops);
		DPRINTF(("%s: cannot clone mbuf chain, IPCA %s/%08lx\n",
		    __func__, ipsec_address(&sav->sah->saidx.dst, buf,
		    sizeof(buf)), (u_long) ntohl(sav->spi)));
		error = ENOBUFS;
		goto bad;
	}

	/* Ok now, we can pass to the crypto processing. */

	/* Get crypto descriptors */
	crp = crypto_getreq(1);
	if (crp == NULL) {
		IPCOMPSTAT_INC(ipcomps_crypto);
		DPRINTF(("%s: failed to acquire crypto descriptor\n",__func__));
		error = ENOBUFS;
		goto bad;
	}
	crdc = crp->crp_desc;

	/* Compression descriptor */
	crdc->crd_skip = skip;
	crdc->crd_len = ralen;
	crdc->crd_flags = CRD_F_COMP;
	crdc->crd_inject = skip;

	/* Compression operation */
	crdc->crd_alg = ipcompx->type;

	/* IPsec-specific opaque crypto info */
	tc = (struct tdb_crypto *) malloc(sizeof(struct tdb_crypto),
		M_XDATA, M_NOWAIT|M_ZERO);
	if (tc == NULL) {
		IPCOMPSTAT_INC(ipcomps_crypto);
		DPRINTF(("%s: failed to allocate tdb_crypto\n", __func__));
		crypto_freereq(crp);
		error = ENOBUFS;
		goto bad;
	}

	key_addref(isr->sp);
	tc->tc_isr = isr;
	KEY_ADDREFSA(sav);
	tc->tc_sav = sav;
	tc->tc_spi = sav->spi;
	tc->tc_dst = sav->sah->saidx.dst;
	tc->tc_proto = sav->sah->saidx.proto;
	tc->tc_protoff = protoff;
	tc->tc_skip = skip;

	/* Crypto operation descriptor */
	crp->crp_ilen = m->m_pkthdr.len;	/* Total input length */
	crp->crp_flags = CRYPTO_F_IMBUF | CRYPTO_F_CBIFSYNC;
	crp->crp_buf = (caddr_t) m;
	crp->crp_callback = ipcomp_output_cb;
	crp->crp_opaque = (caddr_t) tc;
	crp->crp_sid = sav->tdb_cryptoid;

	return crypto_dispatch(crp);
bad:
	if (m)
		m_freem(m);
	return (error);
}

/*
 * IPComp output callback from the crypto driver.
 */
static int
ipcomp_output_cb(struct cryptop *crp)
{
	char buf[INET6_ADDRSTRLEN];
	struct tdb_crypto *tc;
	struct ipsecrequest *isr;
	struct secasvar *sav;
	struct mbuf *m;
	int error, skip;

	tc = (struct tdb_crypto *) crp->crp_opaque;
	IPSEC_ASSERT(tc != NULL, ("null opaque data area!"));
	m = (struct mbuf *) crp->crp_buf;
	skip = tc->tc_skip;

	isr = tc->tc_isr;
	IPSEC_ASSERT(isr->sp != NULL, ("NULL isr->sp"));
	IPSECREQUEST_LOCK(isr);
	sav = tc->tc_sav;
	/* With the isr lock released SA pointer can be updated. */
	if (sav != isr->sav) {
		IPCOMPSTAT_INC(ipcomps_notdb);
		DPRINTF(("%s: SA expired while in crypto\n", __func__));
		error = ENOBUFS;		/*XXX*/
		goto bad;
	}

	/* Check for crypto errors */
	if (crp->crp_etype) {
		/* Reset the session ID */
		if (sav->tdb_cryptoid != 0)
			sav->tdb_cryptoid = crp->crp_sid;

		if (crp->crp_etype == EAGAIN) {
			IPSECREQUEST_UNLOCK(isr);
			return crypto_dispatch(crp);
		}
		IPCOMPSTAT_INC(ipcomps_noxform);
		DPRINTF(("%s: crypto error %d\n", __func__, crp->crp_etype));
		error = crp->crp_etype;
		goto bad;
	}
	/* Shouldn't happen... */
	if (m == NULL) {
		IPCOMPSTAT_INC(ipcomps_crypto);
		DPRINTF(("%s: bogus return buffer from crypto\n", __func__));
		error = EINVAL;
		goto bad;
	}
	IPCOMPSTAT_INC(ipcomps_hist[sav->alg_comp]);

	if (crp->crp_ilen - skip > crp->crp_olen) {
		struct mbuf *mo;
		struct ipcomp *ipcomp;
		int roff;
		uint8_t prot;

		/* Compression helped, inject IPCOMP header. */
		mo = m_makespace(m, skip, IPCOMP_HLENGTH, &roff);
		if (mo == NULL) {
			IPCOMPSTAT_INC(ipcomps_wrap);
			DPRINTF(("%s: IPCOMP header inject failed for IPCA %s/%08lx\n",
			    __func__, ipsec_address(&sav->sah->saidx.dst, buf,
			    sizeof(buf)), (u_long) ntohl(sav->spi)));
			error = ENOBUFS;
			goto bad;
		}
		ipcomp = (struct ipcomp *)(mtod(mo, caddr_t) + roff);

		/* Initialize the IPCOMP header */
		/* XXX alignment always correct? */
		switch (sav->sah->saidx.dst.sa.sa_family) {
#ifdef INET
		case AF_INET:
			ipcomp->comp_nxt = mtod(m, struct ip *)->ip_p;
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			ipcomp->comp_nxt = mtod(m, struct ip6_hdr *)->ip6_nxt;
			break;
#endif
		}
		ipcomp->comp_flags = 0;
		ipcomp->comp_cpi = htons((u_int16_t) ntohl(sav->spi));

		/* Fix Next Protocol in IPv4/IPv6 header */
		prot = IPPROTO_IPCOMP;
		m_copyback(m, tc->tc_protoff, sizeof(u_int8_t),
		    (u_char *)&prot);

		/* Adjust the length in the IP header */
		switch (sav->sah->saidx.dst.sa.sa_family) {
#ifdef INET
		case AF_INET:
			mtod(m, struct ip *)->ip_len = htons(m->m_pkthdr.len);
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			mtod(m, struct ip6_hdr *)->ip6_plen =
				htons(m->m_pkthdr.len) - sizeof(struct ip6_hdr);
			break;
#endif /* INET6 */
		default:
			IPCOMPSTAT_INC(ipcomps_nopf);
			DPRINTF(("%s: unknown/unsupported protocol "
			    "family %d, IPCA %s/%08lx\n", __func__,
			    sav->sah->saidx.dst.sa.sa_family,
			    ipsec_address(&sav->sah->saidx.dst, buf,
				sizeof(buf)), (u_long) ntohl(sav->spi)));
			error = EPFNOSUPPORT;
			goto bad;
		}
	} else {
		/* Compression was useless, we have lost time. */
		IPCOMPSTAT_INC(ipcomps_uncompr);
		DPRINTF(("%s: compressions was useless %d - %d <= %d\n",
		    __func__, crp->crp_ilen, skip, crp->crp_olen));
		/* XXX remember state to not compress the next couple
		 *     of packets, RFC 3173, 2.2. Non-Expansion Policy */
	}

	/* Release the crypto descriptor */
	free(tc, M_XDATA);
	crypto_freereq(crp);

	/* NB: m is reclaimed by ipsec_process_done. */
	error = ipsec_process_done(m, isr);
	KEY_FREESAV(&sav);
	IPSECREQUEST_UNLOCK(isr);
	KEY_FREESP(&isr->sp);
	return (error);
bad:
	if (sav)
		KEY_FREESAV(&sav);
	IPSECREQUEST_UNLOCK(isr);
	KEY_FREESP(&isr->sp);
	if (m)
		m_freem(m);
	free(tc, M_XDATA);
	crypto_freereq(crp);
	return (error);
}

static struct xformsw ipcomp_xformsw = {
	XF_IPCOMP,		XFT_COMP,		"IPcomp",
	ipcomp_init,		ipcomp_zeroize,		ipcomp_input,
	ipcomp_output
};

#ifdef INET
static const struct encaptab *ipe4_cookie = NULL;
extern struct domain inetdomain;
static struct protosw ipcomp4_protosw = {
	.pr_type =	SOCK_RAW,
	.pr_domain =	&inetdomain,
	.pr_protocol =	0 /* IPPROTO_IPV[46] */,
	.pr_flags =	PR_ATOMIC | PR_ADDR | PR_LASTHDR,
	.pr_input =	ipcomp_nonexp_input,
	.pr_output =	rip_output,
	.pr_ctloutput =	rip_ctloutput,
	.pr_usrreqs =	&rip_usrreqs
};

static int
ipcomp4_nonexp_encapcheck(const struct mbuf *m, int off, int proto,
    void *arg __unused)
{
	union sockaddr_union src, dst;
	const struct ip *ip;

	if (V_ipcomp_enable == 0)
		return (0);
	if (proto != IPPROTO_IPV4 && proto != IPPROTO_IPV6)
		return (0);
	bzero(&src, sizeof(src));
	bzero(&dst, sizeof(dst));
	src.sa.sa_family = dst.sa.sa_family = AF_INET;
	src.sin.sin_len = dst.sin.sin_len = sizeof(struct sockaddr_in);
	ip = mtod(m, const struct ip *);
	src.sin.sin_addr = ip->ip_src;
	dst.sin.sin_addr = ip->ip_dst;
	return (ipcomp_encapcheck(&src, &dst));
}
#endif
#ifdef INET6
static const struct encaptab *ipe6_cookie = NULL;
extern struct domain inet6domain;
static struct protosw ipcomp6_protosw = {
	.pr_type =	SOCK_RAW,
	.pr_domain =	&inet6domain,
	.pr_protocol =	0 /* IPPROTO_IPV[46] */,
	.pr_flags =	PR_ATOMIC | PR_ADDR | PR_LASTHDR,
	.pr_input =	ipcomp_nonexp_input,
	.pr_output =	rip6_output,
	.pr_ctloutput =	rip6_ctloutput,
	.pr_usrreqs =	&rip6_usrreqs
};

static int
ipcomp6_nonexp_encapcheck(const struct mbuf *m, int off, int proto,
    void *arg __unused)
{
	union sockaddr_union src, dst;
	const struct ip6_hdr *ip6;

	if (V_ipcomp_enable == 0)
		return (0);
	if (proto != IPPROTO_IPV4 && proto != IPPROTO_IPV6)
		return (0);
	bzero(&src, sizeof(src));
	bzero(&dst, sizeof(dst));
	src.sa.sa_family = dst.sa.sa_family = AF_INET;
	src.sin6.sin6_len = dst.sin6.sin6_len = sizeof(struct sockaddr_in6);
	ip6 = mtod(m, const struct ip6_hdr *);
	src.sin6.sin6_addr = ip6->ip6_src;
	dst.sin6.sin6_addr = ip6->ip6_dst;
	if (IN6_IS_SCOPE_LINKLOCAL(&src.sin6.sin6_addr)) {
		/* XXX: sa6_recoverscope() */
		src.sin6.sin6_scope_id =
		    ntohs(src.sin6.sin6_addr.s6_addr16[1]);
		src.sin6.sin6_addr.s6_addr16[1] = 0;
	}
	if (IN6_IS_SCOPE_LINKLOCAL(&dst.sin6.sin6_addr)) {
		/* XXX: sa6_recoverscope() */
		dst.sin6.sin6_scope_id =
		    ntohs(dst.sin6.sin6_addr.s6_addr16[1]);
		dst.sin6.sin6_addr.s6_addr16[1] = 0;
	}
	return (ipcomp_encapcheck(&src, &dst));
}
#endif

static void
ipcomp_attach(void)
{

#ifdef INET
	ipe4_cookie = encap_attach_func(AF_INET, -1,
	    ipcomp4_nonexp_encapcheck, &ipcomp4_protosw, NULL);
#endif
#ifdef INET6
	ipe6_cookie = encap_attach_func(AF_INET6, -1,
	    ipcomp6_nonexp_encapcheck, &ipcomp6_protosw, NULL);
#endif
	xform_register(&ipcomp_xformsw);
}

SYSINIT(ipcomp_xform_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_MIDDLE,
    ipcomp_attach, NULL);
