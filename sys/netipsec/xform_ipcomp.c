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
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/protosw.h>
#include <sys/sysctl.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <net/route.h>
#include <netipsec/ipsec.h>
#include <netipsec/xform.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netipsec/ipsec6.h>
#endif

#include <netipsec/ipcomp.h>
#include <netipsec/ipcomp_var.h>

#include <netipsec/key.h>
#include <netipsec/key_debug.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/deflate.h>
#include <opencrypto/xform.h>

int	ipcomp_enable = 0;
struct	ipcompstat ipcompstat;

SYSCTL_DECL(_net_inet_ipcomp);
SYSCTL_INT(_net_inet_ipcomp, OID_AUTO,
	ipcomp_enable,	CTLFLAG_RW,	&ipcomp_enable,	0, "");
SYSCTL_STRUCT(_net_inet_ipcomp, IPSECCTL_STATS,
	stats,		CTLFLAG_RD,	&ipcompstat,	ipcompstat, "");

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

	return crypto_newsession(&sav->tdb_cryptoid, &cric, crypto_support);
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
	int hlen = IPCOMP_HLENGTH;

	IPSEC_SPLASSERT_SOFTNET(__func__);

	/* Get crypto descriptors */
	crp = crypto_getreq(1);
	if (crp == NULL) {
		m_freem(m);
		DPRINTF(("%s: no crypto descriptors\n", __func__));
		ipcompstat.ipcomps_crypto++;
		return ENOBUFS;
	}
	/* Get IPsec-specific opaque pointer */
	tc = (struct tdb_crypto *) malloc(sizeof (*tc), M_XDATA, M_NOWAIT|M_ZERO);
	if (tc == NULL) {
		m_freem(m);
		crypto_freereq(crp);
		DPRINTF(("%s: cannot allocate tdb_crypto\n", __func__));
		ipcompstat.ipcomps_crypto++;
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

	return crypto_dispatch(crp);
}

#ifdef INET6
#define	IPSEC_COMMON_INPUT_CB(m, sav, skip, protoff, mtag) do {		     \
	if (saidx->dst.sa.sa_family == AF_INET6) {			     \
		error = ipsec6_common_input_cb(m, sav, skip, protoff, mtag); \
	} else {							     \
		error = ipsec4_common_input_cb(m, sav, skip, protoff, mtag); \
	}								     \
} while (0)
#else
#define	IPSEC_COMMON_INPUT_CB(m, sav, skip, protoff, mtag)		     \
	(error = ipsec4_common_input_cb(m, sav, skip, protoff, mtag))
#endif

/*
 * IPComp input callback from the crypto driver.
 */
static int
ipcomp_input_cb(struct cryptop *crp)
{
	struct cryptodesc *crd;
	struct tdb_crypto *tc;
	int skip, protoff;
	struct mtag *mtag;
	struct mbuf *m;
	struct secasvar *sav;
	struct secasindex *saidx;
	int hlen = IPCOMP_HLENGTH, error, clen;
	u_int8_t nproto;
	caddr_t addr;

	NET_LOCK_GIANT();

	crd = crp->crp_desc;

	tc = (struct tdb_crypto *) crp->crp_opaque;
	IPSEC_ASSERT(tc != NULL, ("null opaque crypto data area!"));
	skip = tc->tc_skip;
	protoff = tc->tc_protoff;
	mtag = (struct mtag *) tc->tc_ptr;
	m = (struct mbuf *) crp->crp_buf;

	sav = KEY_ALLOCSA(&tc->tc_dst, tc->tc_proto, tc->tc_spi);
	if (sav == NULL) {
		ipcompstat.ipcomps_notdb++;
		DPRINTF(("%s: SA expired while in crypto\n", __func__));
		error = ENOBUFS;		/*XXX*/
		goto bad;
	}

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
			KEY_FREESAV(&sav);
			error = crypto_dispatch(crp);
			NET_UNLOCK_GIANT();
			return error;
		}

		ipcompstat.ipcomps_noxform++;
		DPRINTF(("%s: crypto error %d\n", __func__, crp->crp_etype));
		error = crp->crp_etype;
		goto bad;
	}
	/* Shouldn't happen... */
	if (m == NULL) {
		ipcompstat.ipcomps_crypto++;
		DPRINTF(("%s: null mbuf returned from crypto\n", __func__));
		error = EINVAL;
		goto bad;
	}
	ipcompstat.ipcomps_hist[sav->alg_comp]++;

	clen = crp->crp_olen;		/* Length of data after processing */

	/* Release the crypto descriptors */
	free(tc, M_XDATA), tc = NULL;
	crypto_freereq(crp), crp = NULL;

	/* In case it's not done already, adjust the size of the mbuf chain */
	m->m_pkthdr.len = clen + hlen + skip;

	if (m->m_len < skip + hlen && (m = m_pullup(m, skip + hlen)) == 0) {
		ipcompstat.ipcomps_hdrops++;		/*XXX*/
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
		ipcompstat.ipcomps_hdrops++;
		DPRINTF(("%s: bad mbuf chain, IPCA %s/%08lx\n", __func__,
			 ipsec_address(&sav->sah->saidx.dst),
			 (u_long) ntohl(sav->spi)));
		goto bad;
	}

	/* Restore the Next Protocol field */
	m_copyback(m, protoff, sizeof (u_int8_t), (u_int8_t *) &nproto);

	IPSEC_COMMON_INPUT_CB(m, sav, skip, protoff, NULL);

	KEY_FREESAV(&sav);
	NET_UNLOCK_GIANT();
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
	NET_UNLOCK_GIANT();
	return error;
}

/*
 * IPComp output routine, called by ipsec[46]_process_packet()
 */
static int
ipcomp_output(
	struct mbuf *m,
	struct ipsecrequest *isr,
	struct mbuf **mp,
	int skip,
	int protoff
)
{
	struct secasvar *sav;
	struct comp_algo *ipcompx;
	int error, ralen, hlen, maxpacketsize, roff;
	u_int8_t prot;
	struct cryptodesc *crdc;
	struct cryptop *crp;
	struct tdb_crypto *tc;
	struct mbuf *mo;
	struct ipcomp *ipcomp;

	IPSEC_SPLASSERT_SOFTNET(__func__);

	sav = isr->sav;
	IPSEC_ASSERT(sav != NULL, ("null SA"));
	ipcompx = sav->tdb_compalgxform;
	IPSEC_ASSERT(ipcompx != NULL, ("null compression xform"));

	ralen = m->m_pkthdr.len - skip;	/* Raw payload length before comp. */
	hlen = IPCOMP_HLENGTH;

	ipcompstat.ipcomps_output++;

	/* Check for maximum packet size violations. */
	switch (sav->sah->saidx.dst.sa.sa_family) {
#ifdef INET
	case AF_INET:
		maxpacketsize =  IP_MAXPACKET;
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		maxpacketsize =  IPV6_MAXPACKET;
		break;
#endif /* INET6 */
	default:
		ipcompstat.ipcomps_nopf++;
		DPRINTF(("%s: unknown/unsupported protocol family %d, "
		    "IPCA %s/%08lx\n", __func__,
		    sav->sah->saidx.dst.sa.sa_family,
		    ipsec_address(&sav->sah->saidx.dst),
		    (u_long) ntohl(sav->spi)));
		error = EPFNOSUPPORT;
		goto bad;
	}
	if (skip + hlen + ralen > maxpacketsize) {
		ipcompstat.ipcomps_toobig++;
		DPRINTF(("%s: packet in IPCA %s/%08lx got too big "
		    "(len %u, max len %u)\n", __func__,
		    ipsec_address(&sav->sah->saidx.dst),
		    (u_long) ntohl(sav->spi),
		    skip + hlen + ralen, maxpacketsize));
		error = EMSGSIZE;
		goto bad;
	}

	/* Update the counters */
	ipcompstat.ipcomps_obytes += m->m_pkthdr.len - skip;

	m = m_unshare(m, M_NOWAIT);
	if (m == NULL) {
		ipcompstat.ipcomps_hdrops++;
		DPRINTF(("%s: cannot clone mbuf chain, IPCA %s/%08lx\n",
		    __func__, ipsec_address(&sav->sah->saidx.dst),
		    (u_long) ntohl(sav->spi)));
		error = ENOBUFS;
		goto bad;
	}

	/* Inject IPCOMP header */
	mo = m_makespace(m, skip, hlen, &roff);
	if (mo == NULL) {
		ipcompstat.ipcomps_wrap++;
		DPRINTF(("%s: IPCOMP header inject failed for IPCA %s/%08lx\n",
		    __func__, ipsec_address(&sav->sah->saidx.dst),
		    (u_long) ntohl(sav->spi)));
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
	m_copyback(m, protoff, sizeof(u_int8_t), (u_char *) &prot);

	/* Ok now, we can pass to the crypto processing */

	/* Get crypto descriptors */
	crp = crypto_getreq(1);
	if (crp == NULL) {
		ipcompstat.ipcomps_crypto++;
		DPRINTF(("%s: failed to acquire crypto descriptor\n",__func__));
		error = ENOBUFS;
		goto bad;
	}
	crdc = crp->crp_desc;

	/* Compression descriptor */
	crdc->crd_skip = skip + hlen;
	crdc->crd_len = m->m_pkthdr.len - (skip + hlen);
	crdc->crd_flags = CRD_F_COMP;
	crdc->crd_inject = skip + hlen;

	/* Compression operation */
	crdc->crd_alg = ipcompx->type;

	/* IPsec-specific opaque crypto info */
	tc = (struct tdb_crypto *) malloc(sizeof(struct tdb_crypto),
		M_XDATA, M_NOWAIT|M_ZERO);
	if (tc == NULL) {
		ipcompstat.ipcomps_crypto++;
		DPRINTF(("%s: failed to allocate tdb_crypto\n", __func__));
		crypto_freereq(crp);
		error = ENOBUFS;
		goto bad;
	}

	tc->tc_isr = isr;
	tc->tc_spi = sav->spi;
	tc->tc_dst = sav->sah->saidx.dst;
	tc->tc_proto = sav->sah->saidx.proto;
	tc->tc_skip = skip + hlen;

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
	struct tdb_crypto *tc;
	struct ipsecrequest *isr;
	struct secasvar *sav;
	struct mbuf *m;
	int error, skip, rlen;

	NET_LOCK_GIANT();

	tc = (struct tdb_crypto *) crp->crp_opaque;
	IPSEC_ASSERT(tc != NULL, ("null opaque data area!"));
	m = (struct mbuf *) crp->crp_buf;
	skip = tc->tc_skip;
	rlen = crp->crp_ilen - skip;

	isr = tc->tc_isr;
	IPSECREQUEST_LOCK(isr);
	sav = KEY_ALLOCSA(&tc->tc_dst, tc->tc_proto, tc->tc_spi);
	if (sav == NULL) {
		ipcompstat.ipcomps_notdb++;
		DPRINTF(("%s: SA expired while in crypto\n", __func__));
		error = ENOBUFS;		/*XXX*/
		goto bad;
	}
	IPSEC_ASSERT(isr->sav == sav, ("SA changed\n"));

	/* Check for crypto errors */
	if (crp->crp_etype) {
		/* Reset session ID */
		if (sav->tdb_cryptoid != 0)
			sav->tdb_cryptoid = crp->crp_sid;

		if (crp->crp_etype == EAGAIN) {
			KEY_FREESAV(&sav);
			IPSECREQUEST_UNLOCK(isr);
			error = crypto_dispatch(crp);
			NET_UNLOCK_GIANT();
			return error;
		}
		ipcompstat.ipcomps_noxform++;
		DPRINTF(("%s: crypto error %d\n", __func__, crp->crp_etype));
		error = crp->crp_etype;
		goto bad;
	}
	/* Shouldn't happen... */
	if (m == NULL) {
		ipcompstat.ipcomps_crypto++;
		DPRINTF(("%s: bogus return buffer from crypto\n", __func__));
		error = EINVAL;
		goto bad;
	}
	ipcompstat.ipcomps_hist[sav->alg_comp]++;

	if (rlen > crp->crp_olen) {
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
			ipcompstat.ipcomps_nopf++;
			DPRINTF(("%s: unknown/unsupported protocol "
			    "family %d, IPCA %s/%08lx\n", __func__,
			    sav->sah->saidx.dst.sa.sa_family,
			    ipsec_address(&sav->sah->saidx.dst), 
			    (u_long) ntohl(sav->spi)));
			error = EPFNOSUPPORT;
			goto bad;
		}
	} else {
		/* compression was useless, we have lost time */
		/* XXX add statistic */
	}

	/* Release the crypto descriptor */
	free(tc, M_XDATA);
	crypto_freereq(crp);

	/* NB: m is reclaimed by ipsec_process_done. */
	error = ipsec_process_done(m, isr);
	KEY_FREESAV(&sav);
	IPSECREQUEST_UNLOCK(isr);
	NET_UNLOCK_GIANT();
	return error;
bad:
	if (sav)
		KEY_FREESAV(&sav);
	IPSECREQUEST_UNLOCK(isr);
	if (m)
		m_freem(m);
	free(tc, M_XDATA);
	crypto_freereq(crp);
	NET_UNLOCK_GIANT();
	return error;
}

static struct xformsw ipcomp_xformsw = {
	XF_IPCOMP,		XFT_COMP,		"IPcomp",
	ipcomp_init,		ipcomp_zeroize,		ipcomp_input,
	ipcomp_output
};

static void
ipcomp_attach(void)
{
	xform_register(&ipcomp_xformsw);
}
SYSINIT(ipcomp_xform_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_MIDDLE, ipcomp_attach, NULL);
