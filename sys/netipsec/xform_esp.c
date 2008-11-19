/*	$FreeBSD$	*/
/*	$OpenBSD: ip_esp.c,v 1.69 2001/06/26 06:18:59 angelos Exp $ */
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
 * Copyright (c) 2001 Angelos D. Keromytis.
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
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/random.h>
#include <sys/sysctl.h>
#include <sys/vimage.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_ecn.h>
#include <netinet/ip6.h>

#include <net/route.h>
#include <netipsec/ipsec.h>
#include <netipsec/ah.h>
#include <netipsec/ah_var.h>
#include <netipsec/esp.h>
#include <netipsec/esp_var.h>
#include <netipsec/xform.h>

#ifdef INET6
#include <netinet6/ip6_var.h>
#include <netipsec/ipsec6.h>
#include <netinet6/ip6_ecn.h>
#endif

#include <netipsec/key.h>
#include <netipsec/key_debug.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>

#ifdef VIMAGE_GLOBALS
struct	espstat espstat;
static	int esp_max_ivlen;		/* max iv length over all algorithms */
int	esp_enable;
#endif

SYSCTL_DECL(_net_inet_esp);
SYSCTL_V_INT(V_NET, vnet_ipsec,_net_inet_esp, OID_AUTO,
	esp_enable,	CTLFLAG_RW,	esp_enable,	0, "");
SYSCTL_V_STRUCT(V_NET, vnet_ipsec, _net_inet_esp, IPSECCTL_STATS,
	stats,		CTLFLAG_RD,	espstat,	espstat, "");

static int esp_input_cb(struct cryptop *op);
static int esp_output_cb(struct cryptop *crp);

/*
 * NB: this is public for use by the PF_KEY support.
 * NB: if you add support here; be sure to add code to esp_attach below!
 */
struct enc_xform *
esp_algorithm_lookup(int alg)
{
	if (alg >= ESP_ALG_MAX)
		return NULL;
	switch (alg) {
	case SADB_EALG_DESCBC:
		return &enc_xform_des;
	case SADB_EALG_3DESCBC:
		return &enc_xform_3des;
	case SADB_X_EALG_AES:
		return &enc_xform_rijndael128;
	case SADB_X_EALG_BLOWFISHCBC:
		return &enc_xform_blf;
	case SADB_X_EALG_CAST128CBC:
		return &enc_xform_cast5;
	case SADB_X_EALG_SKIPJACK:
		return &enc_xform_skipjack;
	case SADB_EALG_NULL:
		return &enc_xform_null;
	case SADB_X_EALG_CAMELLIACBC:
		return &enc_xform_camellia;
	}
	return NULL;
}

size_t
esp_hdrsiz(struct secasvar *sav)
{
	INIT_VNET_IPSEC(curvnet);
	size_t size;

	if (sav != NULL) {
		/*XXX not right for null algorithm--does it matter??*/
		IPSEC_ASSERT(sav->tdb_encalgxform != NULL,
			("SA with null xform"));
		if (sav->flags & SADB_X_EXT_OLD)
			size = sizeof (struct esp);
		else
			size = sizeof (struct newesp);
		size += sav->tdb_encalgxform->blocksize + 9;
		/*XXX need alg check???*/
		if (sav->tdb_authalgxform != NULL && sav->replay)
			size += ah_hdrsiz(sav);
	} else {
		/*
		 *   base header size
		 * + max iv length for CBC mode
		 * + max pad length
		 * + sizeof (pad length field)
		 * + sizeof (next header field)
		 * + max icv supported.
		 */
		size = sizeof (struct newesp) + V_esp_max_ivlen + 9 + 16;
	}
	return size;
}

/*
 * esp_init() is called when an SPI is being set up.
 */
static int
esp_init(struct secasvar *sav, struct xformsw *xsp)
{
	INIT_VNET_IPSEC(curvnet);
	struct enc_xform *txform;
	struct cryptoini cria, crie;
	int keylen;
	int error;

	txform = esp_algorithm_lookup(sav->alg_enc);
	if (txform == NULL) {
		DPRINTF(("%s: unsupported encryption algorithm %d\n",
			__func__, sav->alg_enc));
		return EINVAL;
	}
	if (sav->key_enc == NULL) {
		DPRINTF(("%s: no encoding key for %s algorithm\n",
			 __func__, txform->name));
		return EINVAL;
	}
	if ((sav->flags&(SADB_X_EXT_OLD|SADB_X_EXT_IV4B)) == SADB_X_EXT_IV4B) {
		DPRINTF(("%s: 4-byte IV not supported with protocol\n",
			__func__));
		return EINVAL;
	}
	keylen = _KEYLEN(sav->key_enc);
	if (txform->minkey > keylen || keylen > txform->maxkey) {
		DPRINTF(("%s: invalid key length %u, must be in the range "
			"[%u..%u] for algorithm %s\n", __func__,
			keylen, txform->minkey, txform->maxkey,
			txform->name));
		return EINVAL;
	}

	/*
	 * NB: The null xform needs a non-zero blocksize to keep the
	 *      crypto code happy but if we use it to set ivlen then
	 *      the ESP header will be processed incorrectly.  The
	 *      compromise is to force it to zero here.
	 */
	sav->ivlen = (txform == &enc_xform_null ? 0 : txform->blocksize);
	sav->iv = (caddr_t) malloc(sav->ivlen, M_XDATA, M_WAITOK);
	if (sav->iv == NULL) {
		DPRINTF(("%s: no memory for IV\n", __func__));
		return EINVAL;
	}
	key_randomfill(sav->iv, sav->ivlen);	/*XXX*/

	/*
	 * Setup AH-related state.
	 */
	if (sav->alg_auth != 0) {
		error = ah_init0(sav, xsp, &cria);
		if (error)
			return error;
	}

	/* NB: override anything set in ah_init0 */
	sav->tdb_xform = xsp;
	sav->tdb_encalgxform = txform;

	/* Initialize crypto session. */
	bzero(&crie, sizeof (crie));
	crie.cri_alg = sav->tdb_encalgxform->type;
	crie.cri_klen = _KEYBITS(sav->key_enc);
	crie.cri_key = sav->key_enc->key_data;
	/* XXX Rounds ? */

	if (sav->tdb_authalgxform && sav->tdb_encalgxform) {
		/* init both auth & enc */
		crie.cri_next = &cria;
		error = crypto_newsession(&sav->tdb_cryptoid,
					  &crie, V_crypto_support);
	} else if (sav->tdb_encalgxform) {
		error = crypto_newsession(&sav->tdb_cryptoid,
					  &crie, V_crypto_support);
	} else if (sav->tdb_authalgxform) {
		error = crypto_newsession(&sav->tdb_cryptoid,
					  &cria, V_crypto_support);
	} else {
		/* XXX cannot happen? */
		DPRINTF(("%s: no encoding OR authentication xform!\n",
			__func__));
		error = EINVAL;
	}
	return error;
}

/*
 * Paranoia.
 */
static int
esp_zeroize(struct secasvar *sav)
{
	/* NB: ah_zerorize free's the crypto session state */
	int error = ah_zeroize(sav);

	if (sav->key_enc)
		bzero(sav->key_enc->key_data, _KEYLEN(sav->key_enc));
	if (sav->iv) {
		free(sav->iv, M_XDATA);
		sav->iv = NULL;
	}
	sav->tdb_encalgxform = NULL;
	sav->tdb_xform = NULL;
	return error;
}

/*
 * ESP input processing, called (eventually) through the protocol switch.
 */
static int
esp_input(struct mbuf *m, struct secasvar *sav, int skip, int protoff)
{
	INIT_VNET_IPSEC(curvnet);
	struct auth_hash *esph;
	struct enc_xform *espx;
	struct tdb_ident *tdbi;
	struct tdb_crypto *tc;
	int plen, alen, hlen;
	struct m_tag *mtag;
	struct newesp *esp;

	struct cryptodesc *crde;
	struct cryptop *crp;

	IPSEC_ASSERT(sav != NULL, ("null SA"));
	IPSEC_ASSERT(sav->tdb_encalgxform != NULL, ("null encoding xform"));
	IPSEC_ASSERT((skip&3) == 0 && (m->m_pkthdr.len&3) == 0,
		("misaligned packet, skip %u pkt len %u",
			skip, m->m_pkthdr.len));

	/* XXX don't pullup, just copy header */
	IP6_EXTHDR_GET(esp, struct newesp *, m, skip, sizeof (struct newesp));

	esph = sav->tdb_authalgxform;
	espx = sav->tdb_encalgxform;

	/* Determine the ESP header length */
	if (sav->flags & SADB_X_EXT_OLD)
		hlen = sizeof (struct esp) + sav->ivlen;
	else
		hlen = sizeof (struct newesp) + sav->ivlen;
	/* Authenticator hash size */
	alen = esph ? AH_HMAC_HASHLEN : 0;

	/*
	 * Verify payload length is multiple of encryption algorithm
	 * block size.
	 *
	 * NB: This works for the null algorithm because the blocksize
	 *     is 4 and all packets must be 4-byte aligned regardless
	 *     of the algorithm.
	 */
	plen = m->m_pkthdr.len - (skip + hlen + alen);
	if ((plen & (espx->blocksize - 1)) || (plen <= 0)) {
		DPRINTF(("%s: payload of %d octets not a multiple of %d octets,"
		    "  SA %s/%08lx\n", __func__,
		    plen, espx->blocksize,
		    ipsec_address(&sav->sah->saidx.dst),
		    (u_long) ntohl(sav->spi)));
		V_espstat.esps_badilen++;
		m_freem(m);
		return EINVAL;
	}

	/*
	 * Check sequence number.
	 */
	if (esph && sav->replay && !ipsec_chkreplay(ntohl(esp->esp_seq), sav)) {
		DPRINTF(("%s: packet replay check for %s\n", __func__,
		    ipsec_logsastr(sav)));	/*XXX*/
		V_espstat.esps_replay++;
		m_freem(m);
		return ENOBUFS;		/*XXX*/
	}

	/* Update the counters */
	V_espstat.esps_ibytes += m->m_pkthdr.len - (skip + hlen + alen);

	/* Find out if we've already done crypto */
	for (mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_CRYPTO_DONE, NULL);
	     mtag != NULL;
	     mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_CRYPTO_DONE, mtag)) {
		tdbi = (struct tdb_ident *) (mtag + 1);
		if (tdbi->proto == sav->sah->saidx.proto &&
		    tdbi->spi == sav->spi &&
		    !bcmp(&tdbi->dst, &sav->sah->saidx.dst,
			  sizeof(union sockaddr_union)))
			break;
	}

	/* Get crypto descriptors */
	crp = crypto_getreq(esph && espx ? 2 : 1);
	if (crp == NULL) {
		DPRINTF(("%s: failed to acquire crypto descriptors\n",
			__func__));
		V_espstat.esps_crypto++;
		m_freem(m);
		return ENOBUFS;
	}

	/* Get IPsec-specific opaque pointer */
	if (esph == NULL || mtag != NULL)
		tc = (struct tdb_crypto *) malloc(sizeof(struct tdb_crypto),
		    M_XDATA, M_NOWAIT|M_ZERO);
	else
		tc = (struct tdb_crypto *) malloc(sizeof(struct tdb_crypto) + alen,
		    M_XDATA, M_NOWAIT|M_ZERO);
	if (tc == NULL) {
		crypto_freereq(crp);
		DPRINTF(("%s: failed to allocate tdb_crypto\n", __func__));
		V_espstat.esps_crypto++;
		m_freem(m);
		return ENOBUFS;
	}

	tc->tc_ptr = (caddr_t) mtag;

	if (esph) {
		struct cryptodesc *crda = crp->crp_desc;

		IPSEC_ASSERT(crda != NULL, ("null ah crypto descriptor"));

		/* Authentication descriptor */
		crda->crd_skip = skip;
		crda->crd_len = m->m_pkthdr.len - (skip + alen);
		crda->crd_inject = m->m_pkthdr.len - alen;

		crda->crd_alg = esph->type;
		crda->crd_key = sav->key_auth->key_data;
		crda->crd_klen = _KEYBITS(sav->key_auth);

		/* Copy the authenticator */
		if (mtag == NULL)
			m_copydata(m, m->m_pkthdr.len - alen, alen,
				   (caddr_t) (tc + 1));

		/* Chain authentication request */
		crde = crda->crd_next;
	} else {
		crde = crp->crp_desc;
	}

	/* Crypto operation descriptor */
	crp->crp_ilen = m->m_pkthdr.len; /* Total input length */
	crp->crp_flags = CRYPTO_F_IMBUF | CRYPTO_F_CBIFSYNC;
	crp->crp_buf = (caddr_t) m;
	crp->crp_callback = esp_input_cb;
	crp->crp_sid = sav->tdb_cryptoid;
	crp->crp_opaque = (caddr_t) tc;

	/* These are passed as-is to the callback */
	tc->tc_spi = sav->spi;
	tc->tc_dst = sav->sah->saidx.dst;
	tc->tc_proto = sav->sah->saidx.proto;
	tc->tc_protoff = protoff;
	tc->tc_skip = skip;

	/* Decryption descriptor */
	if (espx) {
		IPSEC_ASSERT(crde != NULL, ("null esp crypto descriptor"));
		crde->crd_skip = skip + hlen;
		crde->crd_len = m->m_pkthdr.len - (skip + hlen + alen);
		crde->crd_inject = skip + hlen - sav->ivlen;

		crde->crd_alg = espx->type;
		crde->crd_key = sav->key_enc->key_data;
		crde->crd_klen = _KEYBITS(sav->key_enc);
		/* XXX Rounds ? */
	}

	if (mtag == NULL)
		return crypto_dispatch(crp);
	else
		return esp_input_cb(crp);
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
 * ESP input callback from the crypto driver.
 */
static int
esp_input_cb(struct cryptop *crp)
{
	INIT_VNET_IPSEC(curvnet);
	u_int8_t lastthree[3], aalg[AH_HMAC_HASHLEN];
	int hlen, skip, protoff, error;
	struct mbuf *m;
	struct cryptodesc *crd;
	struct auth_hash *esph;
	struct enc_xform *espx;
	struct tdb_crypto *tc;
	struct m_tag *mtag;
	struct secasvar *sav;
	struct secasindex *saidx;
	caddr_t ptr;

	crd = crp->crp_desc;
	IPSEC_ASSERT(crd != NULL, ("null crypto descriptor!"));

	tc = (struct tdb_crypto *) crp->crp_opaque;
	IPSEC_ASSERT(tc != NULL, ("null opaque crypto data area!"));
	skip = tc->tc_skip;
	protoff = tc->tc_protoff;
	mtag = (struct m_tag *) tc->tc_ptr;
	m = (struct mbuf *) crp->crp_buf;

	sav = KEY_ALLOCSA(&tc->tc_dst, tc->tc_proto, tc->tc_spi);
	if (sav == NULL) {
		V_espstat.esps_notdb++;
		DPRINTF(("%s: SA gone during crypto (SA %s/%08lx proto %u)\n",
		    __func__, ipsec_address(&tc->tc_dst),
		    (u_long) ntohl(tc->tc_spi), tc->tc_proto));
		error = ENOBUFS;		/*XXX*/
		goto bad;
	}

	saidx = &sav->sah->saidx;
	IPSEC_ASSERT(saidx->dst.sa.sa_family == AF_INET ||
		saidx->dst.sa.sa_family == AF_INET6,
		("unexpected protocol family %u", saidx->dst.sa.sa_family));

	esph = sav->tdb_authalgxform;
	espx = sav->tdb_encalgxform;

	/* Check for crypto errors */
	if (crp->crp_etype) {
		/* Reset the session ID */
		if (sav->tdb_cryptoid != 0)
			sav->tdb_cryptoid = crp->crp_sid;

		if (crp->crp_etype == EAGAIN) {
			KEY_FREESAV(&sav);
			error = crypto_dispatch(crp);
			return error;
		}

		V_espstat.esps_noxform++;
		DPRINTF(("%s: crypto error %d\n", __func__, crp->crp_etype));
		error = crp->crp_etype;
		goto bad;
	}

	/* Shouldn't happen... */
	if (m == NULL) {
		V_espstat.esps_crypto++;
		DPRINTF(("%s: bogus returned buffer from crypto\n", __func__));
		error = EINVAL;
		goto bad;
	}
	V_espstat.esps_hist[sav->alg_enc]++;

	/* If authentication was performed, check now. */
	if (esph != NULL) {
		/*
		 * If we have a tag, it means an IPsec-aware NIC did
		 * the verification for us.  Otherwise we need to
		 * check the authentication calculation.
		 */
		V_ahstat.ahs_hist[sav->alg_auth]++;
		if (mtag == NULL) {
			/* Copy the authenticator from the packet */
			m_copydata(m, m->m_pkthdr.len - AH_HMAC_HASHLEN,
				AH_HMAC_HASHLEN, aalg);

			ptr = (caddr_t) (tc + 1);

			/* Verify authenticator */
			if (bcmp(ptr, aalg, AH_HMAC_HASHLEN) != 0) {
				DPRINTF(("%s: "
		    "authentication hash mismatch for packet in SA %s/%08lx\n",
				    __func__,
				    ipsec_address(&saidx->dst),
				    (u_long) ntohl(sav->spi)));
				V_espstat.esps_badauth++;
				error = EACCES;
				goto bad;
			}
		}

		/* Remove trailing authenticator */
		m_adj(m, -AH_HMAC_HASHLEN);
	}

	/* Release the crypto descriptors */
	free(tc, M_XDATA), tc = NULL;
	crypto_freereq(crp), crp = NULL;

	/*
	 * Packet is now decrypted.
	 */
	m->m_flags |= M_DECRYPTED;

	/*
	 * Update replay sequence number, if appropriate.
	 */
	if (sav->replay) {
		u_int32_t seq;

		m_copydata(m, skip + offsetof(struct newesp, esp_seq),
			   sizeof (seq), (caddr_t) &seq);
		if (ipsec_updatereplay(ntohl(seq), sav)) {
			DPRINTF(("%s: packet replay check for %s\n", __func__,
			    ipsec_logsastr(sav)));
			V_espstat.esps_replay++;
			error = ENOBUFS;
			goto bad;
		}
	}

	/* Determine the ESP header length */
	if (sav->flags & SADB_X_EXT_OLD)
		hlen = sizeof (struct esp) + sav->ivlen;
	else
		hlen = sizeof (struct newesp) + sav->ivlen;

	/* Remove the ESP header and IV from the mbuf. */
	error = m_striphdr(m, skip, hlen);
	if (error) {
		V_espstat.esps_hdrops++;
		DPRINTF(("%s: bad mbuf chain, SA %s/%08lx\n", __func__,
		    ipsec_address(&sav->sah->saidx.dst),
		    (u_long) ntohl(sav->spi)));
		goto bad;
	}

	/* Save the last three bytes of decrypted data */
	m_copydata(m, m->m_pkthdr.len - 3, 3, lastthree);

	/* Verify pad length */
	if (lastthree[1] + 2 > m->m_pkthdr.len - skip) {
		V_espstat.esps_badilen++;
		DPRINTF(("%s: invalid padding length %d for %u byte packet "
			"in SA %s/%08lx\n", __func__,
			 lastthree[1], m->m_pkthdr.len - skip,
			 ipsec_address(&sav->sah->saidx.dst),
			 (u_long) ntohl(sav->spi)));
		error = EINVAL;
		goto bad;
	}

	/* Verify correct decryption by checking the last padding bytes */
	if ((sav->flags & SADB_X_EXT_PMASK) != SADB_X_EXT_PRAND) {
		if (lastthree[1] != lastthree[0] && lastthree[1] != 0) {
			V_espstat.esps_badenc++;
			DPRINTF(("%s: decryption failed for packet in "
				"SA %s/%08lx\n", __func__,
				ipsec_address(&sav->sah->saidx.dst),
				(u_long) ntohl(sav->spi)));
			error = EINVAL;
			goto bad;
		}
	}

	/* Trim the mbuf chain to remove trailing authenticator and padding */
	m_adj(m, -(lastthree[1] + 2));

	/* Restore the Next Protocol field */
	m_copyback(m, protoff, sizeof (u_int8_t), lastthree + 2);

	IPSEC_COMMON_INPUT_CB(m, sav, skip, protoff, mtag);

	KEY_FREESAV(&sav);
	return error;
bad:
	if (sav)
		KEY_FREESAV(&sav);
	if (m != NULL)
		m_freem(m);
	if (tc != NULL)
		free(tc, M_XDATA);
	if (crp != NULL)
		crypto_freereq(crp);
	return error;
}

/*
 * ESP output routine, called by ipsec[46]_process_packet().
 */
static int
esp_output(
	struct mbuf *m,
	struct ipsecrequest *isr,
	struct mbuf **mp,
	int skip,
	int protoff
)
{
	INIT_VNET_IPSEC(curvnet);
	struct enc_xform *espx;
	struct auth_hash *esph;
	int hlen, rlen, plen, padding, blks, alen, i, roff;
	struct mbuf *mo = (struct mbuf *) NULL;
	struct tdb_crypto *tc;
	struct secasvar *sav;
	struct secasindex *saidx;
	unsigned char *pad;
	u_int8_t prot;
	int error, maxpacketsize;

	struct cryptodesc *crde = NULL, *crda = NULL;
	struct cryptop *crp;

	sav = isr->sav;
	IPSEC_ASSERT(sav != NULL, ("null SA"));
	esph = sav->tdb_authalgxform;
	espx = sav->tdb_encalgxform;
	IPSEC_ASSERT(espx != NULL, ("null encoding xform"));

	if (sav->flags & SADB_X_EXT_OLD)
		hlen = sizeof (struct esp) + sav->ivlen;
	else
		hlen = sizeof (struct newesp) + sav->ivlen;

	rlen = m->m_pkthdr.len - skip;	/* Raw payload length. */
	/*
	 * NB: The null encoding transform has a blocksize of 4
	 *     so that headers are properly aligned.
	 */
	blks = espx->blocksize;		/* IV blocksize */

	/* XXX clamp padding length a la KAME??? */
	padding = ((blks - ((rlen + 2) % blks)) % blks) + 2;
	plen = rlen + padding;		/* Padded payload length. */

	if (esph)
		alen = AH_HMAC_HASHLEN;
	else
		alen = 0;

	V_espstat.esps_output++;

	saidx = &sav->sah->saidx;
	/* Check for maximum packet size violations. */
	switch (saidx->dst.sa.sa_family) {
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
		DPRINTF(("%s: unknown/unsupported protocol "
		    "family %d, SA %s/%08lx\n", __func__,
		    saidx->dst.sa.sa_family, ipsec_address(&saidx->dst),
		    (u_long) ntohl(sav->spi)));
		V_espstat.esps_nopf++;
		error = EPFNOSUPPORT;
		goto bad;
	}
	if (skip + hlen + rlen + padding + alen > maxpacketsize) {
		DPRINTF(("%s: packet in SA %s/%08lx got too big "
		    "(len %u, max len %u)\n", __func__,
		    ipsec_address(&saidx->dst), (u_long) ntohl(sav->spi),
		    skip + hlen + rlen + padding + alen, maxpacketsize));
		V_espstat.esps_toobig++;
		error = EMSGSIZE;
		goto bad;
	}

	/* Update the counters. */
	V_espstat.esps_obytes += m->m_pkthdr.len - skip;

	m = m_unshare(m, M_NOWAIT);
	if (m == NULL) {
		DPRINTF(("%s: cannot clone mbuf chain, SA %s/%08lx\n", __func__,
		    ipsec_address(&saidx->dst), (u_long) ntohl(sav->spi)));
		V_espstat.esps_hdrops++;
		error = ENOBUFS;
		goto bad;
	}

	/* Inject ESP header. */
	mo = m_makespace(m, skip, hlen, &roff);
	if (mo == NULL) {
		DPRINTF(("%s: %u byte ESP hdr inject failed for SA %s/%08lx\n",
		    __func__, hlen, ipsec_address(&saidx->dst),
		    (u_long) ntohl(sav->spi)));
		V_espstat.esps_hdrops++;		/* XXX diffs from openbsd */
		error = ENOBUFS;
		goto bad;
	}

	/* Initialize ESP header. */
	bcopy((caddr_t) &sav->spi, mtod(mo, caddr_t) + roff, sizeof(u_int32_t));
	if (sav->replay) {
		u_int32_t replay;

#ifdef REGRESSION
		/* Emulate replay attack when ipsec_replay is TRUE. */
		if (!V_ipsec_replay)
#endif
			sav->replay->count++;
		replay = htonl(sav->replay->count);
		bcopy((caddr_t) &replay,
		    mtod(mo, caddr_t) + roff + sizeof(u_int32_t),
		    sizeof(u_int32_t));
	}

	/*
	 * Add padding -- better to do it ourselves than use the crypto engine,
	 * although if/when we support compression, we'd have to do that.
	 */
	pad = (u_char *) m_pad(m, padding + alen);
	if (pad == NULL) {
		DPRINTF(("%s: m_pad failed for SA %s/%08lx\n", __func__,
		    ipsec_address(&saidx->dst), (u_long) ntohl(sav->spi)));
		m = NULL;		/* NB: free'd by m_pad */
		error = ENOBUFS;
		goto bad;
	}

	/*
	 * Add padding: random, zero, or self-describing.
	 * XXX catch unexpected setting
	 */
	switch (sav->flags & SADB_X_EXT_PMASK) {
	case SADB_X_EXT_PRAND:
		(void) read_random(pad, padding - 2);
		break;
	case SADB_X_EXT_PZERO:
		bzero(pad, padding - 2);
		break;
	case SADB_X_EXT_PSEQ:
		for (i = 0; i < padding - 2; i++)
			pad[i] = i+1;
		break;
	}

	/* Fix padding length and Next Protocol in padding itself. */
	pad[padding - 2] = padding - 2;
	m_copydata(m, protoff, sizeof(u_int8_t), pad + padding - 1);

	/* Fix Next Protocol in IPv4/IPv6 header. */
	prot = IPPROTO_ESP;
	m_copyback(m, protoff, sizeof(u_int8_t), (u_char *) &prot);

	/* Get crypto descriptors. */
	crp = crypto_getreq(esph && espx ? 2 : 1);
	if (crp == NULL) {
		DPRINTF(("%s: failed to acquire crypto descriptors\n",
			__func__));
		V_espstat.esps_crypto++;
		error = ENOBUFS;
		goto bad;
	}

	if (espx) {
		crde = crp->crp_desc;
		crda = crde->crd_next;

		/* Encryption descriptor. */
		crde->crd_skip = skip + hlen;
		crde->crd_len = m->m_pkthdr.len - (skip + hlen + alen);
		crde->crd_flags = CRD_F_ENCRYPT;
		crde->crd_inject = skip + hlen - sav->ivlen;

		/* Encryption operation. */
		crde->crd_alg = espx->type;
		crde->crd_key = sav->key_enc->key_data;
		crde->crd_klen = _KEYBITS(sav->key_enc);
		/* XXX Rounds ? */
	} else
		crda = crp->crp_desc;

	/* IPsec-specific opaque crypto info. */
	tc = (struct tdb_crypto *) malloc(sizeof(struct tdb_crypto),
		M_XDATA, M_NOWAIT|M_ZERO);
	if (tc == NULL) {
		crypto_freereq(crp);
		DPRINTF(("%s: failed to allocate tdb_crypto\n", __func__));
		V_espstat.esps_crypto++;
		error = ENOBUFS;
		goto bad;
	}

	/* Callback parameters */
	tc->tc_isr = isr;
	tc->tc_spi = sav->spi;
	tc->tc_dst = saidx->dst;
	tc->tc_proto = saidx->proto;

	/* Crypto operation descriptor. */
	crp->crp_ilen = m->m_pkthdr.len; /* Total input length. */
	crp->crp_flags = CRYPTO_F_IMBUF | CRYPTO_F_CBIFSYNC;
	crp->crp_buf = (caddr_t) m;
	crp->crp_callback = esp_output_cb;
	crp->crp_opaque = (caddr_t) tc;
	crp->crp_sid = sav->tdb_cryptoid;

	if (esph) {
		/* Authentication descriptor. */
		crda->crd_skip = skip;
		crda->crd_len = m->m_pkthdr.len - (skip + alen);
		crda->crd_inject = m->m_pkthdr.len - alen;

		/* Authentication operation. */
		crda->crd_alg = esph->type;
		crda->crd_key = sav->key_auth->key_data;
		crda->crd_klen = _KEYBITS(sav->key_auth);
	}

	return crypto_dispatch(crp);
bad:
	if (m)
		m_freem(m);
	return (error);
}

/*
 * ESP output callback from the crypto driver.
 */
static int
esp_output_cb(struct cryptop *crp)
{
	INIT_VNET_IPSEC(curvnet);
	struct tdb_crypto *tc;
	struct ipsecrequest *isr;
	struct secasvar *sav;
	struct mbuf *m;
	int err, error;

	tc = (struct tdb_crypto *) crp->crp_opaque;
	IPSEC_ASSERT(tc != NULL, ("null opaque data area!"));
	m = (struct mbuf *) crp->crp_buf;

	isr = tc->tc_isr;
	IPSECREQUEST_LOCK(isr);
	sav = KEY_ALLOCSA(&tc->tc_dst, tc->tc_proto, tc->tc_spi);
	if (sav == NULL) {
		V_espstat.esps_notdb++;
		DPRINTF(("%s: SA gone during crypto (SA %s/%08lx proto %u)\n",
		    __func__, ipsec_address(&tc->tc_dst),
		    (u_long) ntohl(tc->tc_spi), tc->tc_proto));
		error = ENOBUFS;		/*XXX*/
		goto bad;
	}
	IPSEC_ASSERT(isr->sav == sav,
		("SA changed was %p now %p\n", isr->sav, sav));

	/* Check for crypto errors. */
	if (crp->crp_etype) {
		/* Reset session ID. */
		if (sav->tdb_cryptoid != 0)
			sav->tdb_cryptoid = crp->crp_sid;

		if (crp->crp_etype == EAGAIN) {
			KEY_FREESAV(&sav);
			IPSECREQUEST_UNLOCK(isr);
			error = crypto_dispatch(crp);
			return error;
		}

		V_espstat.esps_noxform++;
		DPRINTF(("%s: crypto error %d\n", __func__, crp->crp_etype));
		error = crp->crp_etype;
		goto bad;
	}

	/* Shouldn't happen... */
	if (m == NULL) {
		V_espstat.esps_crypto++;
		DPRINTF(("%s: bogus returned buffer from crypto\n", __func__));
		error = EINVAL;
		goto bad;
	}
	V_espstat.esps_hist[sav->alg_enc]++;
	if (sav->tdb_authalgxform != NULL)
		V_ahstat.ahs_hist[sav->alg_auth]++;

	/* Release crypto descriptors. */
	free(tc, M_XDATA);
	crypto_freereq(crp);

#ifdef REGRESSION
	/* Emulate man-in-the-middle attack when ipsec_integrity is TRUE. */
	if (V_ipsec_integrity) {
		static unsigned char ipseczeroes[AH_HMAC_HASHLEN];
		struct auth_hash *esph;

		/*
		 * Corrupt HMAC if we want to test integrity verification of
		 * the other side.
		 */
		esph = sav->tdb_authalgxform;
		if (esph !=  NULL) {
			m_copyback(m, m->m_pkthdr.len - AH_HMAC_HASHLEN,
			    AH_HMAC_HASHLEN, ipseczeroes);
		}
	}
#endif

	/* NB: m is reclaimed by ipsec_process_done. */
	err = ipsec_process_done(m, isr);
	KEY_FREESAV(&sav);
	IPSECREQUEST_UNLOCK(isr);
	return err;
bad:
	if (sav)
		KEY_FREESAV(&sav);
	IPSECREQUEST_UNLOCK(isr);
	if (m)
		m_freem(m);
	free(tc, M_XDATA);
	crypto_freereq(crp);
	return error;
}

static struct xformsw esp_xformsw = {
	XF_ESP,		XFT_CONF|XFT_AUTH,	"IPsec ESP",
	esp_init,	esp_zeroize,		esp_input,
	esp_output
};

static void
esp_attach(void)
{
#define	MAXIV(xform)					\
	if (xform.blocksize > V_esp_max_ivlen)		\
		V_esp_max_ivlen = xform.blocksize		\

	V_esp_enable = 1;
	V_esp_max_ivlen = 0;

	MAXIV(enc_xform_des);		/* SADB_EALG_DESCBC */
	MAXIV(enc_xform_3des);		/* SADB_EALG_3DESCBC */
	MAXIV(enc_xform_rijndael128);	/* SADB_X_EALG_AES */
	MAXIV(enc_xform_blf);		/* SADB_X_EALG_BLOWFISHCBC */
	MAXIV(enc_xform_cast5);		/* SADB_X_EALG_CAST128CBC */
	MAXIV(enc_xform_skipjack);	/* SADB_X_EALG_SKIPJACK */
	MAXIV(enc_xform_null);		/* SADB_EALG_NULL */
	MAXIV(enc_xform_camellia);	/* SADB_X_EALG_CAMELLIACBC */

	xform_register(&esp_xformsw);
#undef MAXIV
}
SYSINIT(esp_xform_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_MIDDLE, esp_attach, NULL);
