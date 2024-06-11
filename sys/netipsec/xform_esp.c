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
#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/random.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/mutex.h>
#include <machine/atomic.h>

#include <net/if.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_ecn.h>
#include <netinet/ip6.h>

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

#define SPI_SIZE	4

VNET_DEFINE(int, esp_enable) = 1;
VNET_DEFINE_STATIC(int, esp_ctr_compatibility) = 1;
#define V_esp_ctr_compatibility VNET(esp_ctr_compatibility)
VNET_PCPUSTAT_DEFINE(struct espstat, espstat);
VNET_PCPUSTAT_SYSINIT(espstat);

#ifdef VIMAGE
VNET_PCPUSTAT_SYSUNINIT(espstat);
#endif /* VIMAGE */

SYSCTL_DECL(_net_inet_esp);
SYSCTL_INT(_net_inet_esp, OID_AUTO, esp_enable,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(esp_enable), 0, "");
SYSCTL_INT(_net_inet_esp, OID_AUTO, ctr_compatibility,
    CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(esp_ctr_compatibility), 0,
    "Align AES-CTR encrypted transmitted frames to blocksize");
SYSCTL_VNET_PCPUSTAT(_net_inet_esp, IPSECCTL_STATS, stats,
    struct espstat, espstat,
    "ESP statistics (struct espstat, netipsec/esp_var.h");

static MALLOC_DEFINE(M_ESP, "esp", "IPsec ESP");

static int esp_input_cb(struct cryptop *op);
static int esp_output_cb(struct cryptop *crp);

size_t
esp_hdrsiz(struct secasvar *sav)
{
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
		size = sizeof (struct newesp) + EALG_MAX_BLOCK_LEN + 9 + 16;
	}
	return size;
}

/*
 * esp_init() is called when an SPI is being set up.
 */
static int
esp_init(struct secasvar *sav, struct xformsw *xsp)
{
	const struct enc_xform *txform;
	struct crypto_session_params csp;
	int keylen;
	int error;

	txform = enc_algorithm_lookup(sav->alg_enc);
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
	if ((sav->flags & (SADB_X_EXT_OLD | SADB_X_EXT_IV4B)) ==
	    SADB_X_EXT_IV4B) {
		DPRINTF(("%s: 4-byte IV not supported with protocol\n",
			__func__));
		return EINVAL;
	}

	/* subtract off the salt, RFC4106, 8.1 and RFC3686, 5.1 */
	keylen = _KEYLEN(sav->key_enc) - SAV_ISCTRORGCM(sav) * 4 -
	    SAV_ISCHACHA(sav) * 4;
	if (txform->minkey > keylen || keylen > txform->maxkey) {
		DPRINTF(("%s: invalid key length %u, must be in the range "
			"[%u..%u] for algorithm %s\n", __func__,
			keylen, txform->minkey, txform->maxkey,
			txform->name));
		return EINVAL;
	}

	if (SAV_ISCTRORGCM(sav) || SAV_ISCHACHA(sav))
		sav->ivlen = 8;	/* RFC4106 3.1 and RFC3686 3.1 */
	else
		sav->ivlen = txform->ivsize;

	memset(&csp, 0, sizeof(csp));

	/*
	 * Setup AH-related state.
	 */
	if (sav->alg_auth != 0) {
		error = ah_init0(sav, xsp, &csp);
		if (error)
			return error;
	}

	/* NB: override anything set in ah_init0 */
	sav->tdb_xform = xsp;
	sav->tdb_encalgxform = txform;

	/*
	 * Whenever AES-GCM is used for encryption, one
	 * of the AES authentication algorithms is chosen
	 * as well, based on the key size.
	 */
	if (sav->alg_enc == SADB_X_EALG_AESGCM16) {
		switch (keylen) {
		case AES_128_GMAC_KEY_LEN:
			sav->alg_auth = SADB_X_AALG_AES128GMAC;
			sav->tdb_authalgxform = &auth_hash_nist_gmac_aes_128;
			break;
		case AES_192_GMAC_KEY_LEN:
			sav->alg_auth = SADB_X_AALG_AES192GMAC;
			sav->tdb_authalgxform = &auth_hash_nist_gmac_aes_192;
			break;
		case AES_256_GMAC_KEY_LEN:
			sav->alg_auth = SADB_X_AALG_AES256GMAC;
			sav->tdb_authalgxform = &auth_hash_nist_gmac_aes_256;
			break;
		default:
			DPRINTF(("%s: invalid key length %u"
				 "for algorithm %s\n", __func__,
				 keylen, txform->name));
			return EINVAL;
		}
		csp.csp_mode = CSP_MODE_AEAD;
		if (sav->flags & SADB_X_SAFLAGS_ESN)
			csp.csp_flags |= CSP_F_SEPARATE_AAD;
	} else if (sav->alg_enc == SADB_X_EALG_CHACHA20POLY1305) {
		sav->alg_auth = SADB_X_AALG_CHACHA20POLY1305;
		sav->tdb_authalgxform = &auth_hash_poly1305;
		csp.csp_mode = CSP_MODE_AEAD;
		if (sav->flags & SADB_X_SAFLAGS_ESN)
			csp.csp_flags |= CSP_F_SEPARATE_AAD;
	} else if (sav->alg_auth != 0) {
		csp.csp_mode = CSP_MODE_ETA;
		if (sav->flags & SADB_X_SAFLAGS_ESN)
			csp.csp_flags |= CSP_F_ESN;
	} else
		csp.csp_mode = CSP_MODE_CIPHER;

	/* Initialize crypto session. */
	csp.csp_cipher_alg = sav->tdb_encalgxform->type;
	if (csp.csp_cipher_alg != CRYPTO_NULL_CBC) {
		csp.csp_cipher_key = sav->key_enc->key_data;
		csp.csp_cipher_klen = _KEYBITS(sav->key_enc) / 8 -
		    SAV_ISCTRORGCM(sav) * 4 - SAV_ISCHACHA(sav) * 4;
	};
	csp.csp_ivlen = txform->ivsize;

	error = crypto_newsession(&sav->tdb_cryptoid, &csp, V_crypto_support);
	return error;
}

static void
esp_cleanup(struct secasvar *sav)
{

	crypto_freesession(sav->tdb_cryptoid);
	sav->tdb_cryptoid = NULL;
	sav->tdb_authalgxform = NULL;
	sav->tdb_encalgxform = NULL;
}

/*
 * ESP input processing, called (eventually) through the protocol switch.
 */
static int
esp_input(struct mbuf *m, struct secasvar *sav, int skip, int protoff)
{
	IPSEC_DEBUG_DECLARE(char buf[128]);
	const struct auth_hash *esph;
	const struct enc_xform *espx;
	struct xform_data *xd;
	struct cryptop *crp;
	struct newesp *esp;
	uint8_t *ivp;
	crypto_session_t cryptoid;
	int alen, error, hlen, plen;
	uint32_t seqh;
	const struct crypto_session_params *csp;

	SECASVAR_RLOCK_TRACKER;

	IPSEC_ASSERT(sav != NULL, ("null SA"));
	IPSEC_ASSERT(sav->tdb_encalgxform != NULL, ("null encoding xform"));

	error = EINVAL;
	/* Valid IP Packet length ? */
	if ( (skip&3) || (m->m_pkthdr.len&3) ){
		DPRINTF(("%s: misaligned packet, skip %u pkt len %u",
				__func__, skip, m->m_pkthdr.len));
		ESPSTAT_INC(esps_badilen);
		goto bad;
	}

	if (m->m_len < skip + sizeof(*esp)) {
		m = m_pullup(m, skip + sizeof(*esp));
		if (m == NULL) {
			DPRINTF(("%s: cannot pullup header\n", __func__));
			ESPSTAT_INC(esps_hdrops);	/*XXX*/
			error = ENOBUFS;
			goto bad;
		}
	}
	esp = (struct newesp *)(mtod(m, caddr_t) + skip);

	esph = sav->tdb_authalgxform;
	espx = sav->tdb_encalgxform;

	/* Determine the ESP header and auth length */
	if (sav->flags & SADB_X_EXT_OLD)
		hlen = sizeof (struct esp) + sav->ivlen;
	else
		hlen = sizeof (struct newesp) + sav->ivlen;

	alen = xform_ah_authsize(esph);

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
		    "  SA %s/%08lx\n", __func__, plen, espx->blocksize,
		    ipsec_address(&sav->sah->saidx.dst, buf, sizeof(buf)),
		    (u_long)ntohl(sav->spi)));
		ESPSTAT_INC(esps_badilen);
		goto bad;
	}

	/*
	 * Check sequence number.
	 */
	SECASVAR_RLOCK(sav);
	if (esph != NULL && sav->replay != NULL && sav->replay->wsize != 0) {
		if (ipsec_chkreplay(ntohl(esp->esp_seq), &seqh, sav) == 0) {
			SECASVAR_RUNLOCK(sav);
			DPRINTF(("%s: packet replay check for %s\n", __func__,
			    ipsec_sa2str(sav, buf, sizeof(buf))));
			ESPSTAT_INC(esps_replay);
			error = EACCES;
			goto bad;
		}
		seqh = htonl(seqh);
	}
	cryptoid = sav->tdb_cryptoid;
	SECASVAR_RUNLOCK(sav);

	/* Update the counters */
	ESPSTAT_ADD(esps_ibytes, m->m_pkthdr.len - (skip + hlen + alen));

	/* Get crypto descriptors */
	crp = crypto_getreq(cryptoid, M_NOWAIT);
	if (crp == NULL) {
		DPRINTF(("%s: failed to acquire crypto descriptors\n",
			__func__));
		ESPSTAT_INC(esps_crypto);
		error = ENOBUFS;
		goto bad;
	}

	/* Get IPsec-specific opaque pointer */
	xd = malloc(sizeof(*xd), M_ESP, M_NOWAIT | M_ZERO);
	if (xd == NULL) {
		DPRINTF(("%s: failed to allocate xform_data\n", __func__));
		goto xd_fail;
	}

	if (esph != NULL) {
		crp->crp_op = CRYPTO_OP_VERIFY_DIGEST;
		if (SAV_ISGCM(sav) || SAV_ISCHACHA(sav))
			crp->crp_aad_length = 8; /* RFC4106 5, SPI + SN */
		else
			crp->crp_aad_length = hlen;

		csp = crypto_get_params(crp->crp_session);
		if ((csp->csp_flags & CSP_F_SEPARATE_AAD) &&
		    (sav->replay != NULL) && (sav->replay->wsize != 0)) {
			int aad_skip;

			crp->crp_aad_length += sizeof(seqh);
			crp->crp_aad = malloc(crp->crp_aad_length, M_ESP, M_NOWAIT);
			if (crp->crp_aad == NULL) {
				DPRINTF(("%s: failed to allocate xform_data\n",
					 __func__));
				goto crp_aad_fail;
			}

			/* SPI */
			m_copydata(m, skip, SPI_SIZE, crp->crp_aad);
			aad_skip = SPI_SIZE;

			/* ESN */
			bcopy(&seqh, (char *)crp->crp_aad + aad_skip, sizeof(seqh));
			aad_skip += sizeof(seqh);

			/* Rest of aad */
			if (crp->crp_aad_length - aad_skip > 0)
				m_copydata(m, skip + SPI_SIZE,
					   crp->crp_aad_length - aad_skip,
					   (char *)crp->crp_aad + aad_skip);
		} else
			crp->crp_aad_start = skip;

		if (csp->csp_flags & CSP_F_ESN &&
			   sav->replay != NULL && sav->replay->wsize != 0)
			memcpy(crp->crp_esn, &seqh, sizeof(seqh));

		crp->crp_digest_start = m->m_pkthdr.len - alen;
	}

	/* Crypto operation descriptor */
	crp->crp_flags = CRYPTO_F_CBIFSYNC;
	crypto_use_mbuf(crp, m);
	crp->crp_callback = esp_input_cb;
	crp->crp_opaque = xd;

	/* These are passed as-is to the callback */
	xd->sav = sav;
	xd->protoff = protoff;
	xd->skip = skip;
	xd->cryptoid = cryptoid;
	xd->vnet = curvnet;

	/* Decryption descriptor */
	crp->crp_op |= CRYPTO_OP_DECRYPT;
	crp->crp_payload_start = skip + hlen;
	crp->crp_payload_length = m->m_pkthdr.len - (skip + hlen + alen);

	/* Generate or read cipher IV. */
	if (SAV_ISCTRORGCM(sav) || SAV_ISCHACHA(sav)) {
		ivp = &crp->crp_iv[0];

		/*
		 * AES-GCM and AES-CTR use similar cipher IV formats
		 * defined in RFC 4106 section 4 and RFC 3686 section
		 * 4, respectively.
		 *
		 * The first 4 bytes of the cipher IV contain an
		 * implicit salt, or nonce, obtained from the last 4
		 * bytes of the encryption key.  The next 8 bytes hold
		 * an explicit IV unique to each packet.  This
		 * explicit IV is used as the ESP IV for the packet.
		 * The last 4 bytes hold a big-endian block counter
		 * incremented for each block.  For AES-GCM, the block
		 * counter's initial value is defined as part of the
		 * algorithm.  For AES-CTR, the block counter's
		 * initial value for each packet is defined as 1 by
		 * RFC 3686.
		 *
		 * ------------------------------------------
		 * | Salt | Explicit ESP IV | Block Counter |
		 * ------------------------------------------
		 *  4 bytes     8 bytes          4 bytes
		 */
		memcpy(ivp, sav->key_enc->key_data +
		    _KEYLEN(sav->key_enc) - 4, 4);
		m_copydata(m, skip + hlen - sav->ivlen, sav->ivlen, &ivp[4]);
		if (SAV_ISCTR(sav)) {
			be32enc(&ivp[sav->ivlen + 4], 1);
		}
		crp->crp_flags |= CRYPTO_F_IV_SEPARATE;
	} else if (sav->ivlen != 0)
		crp->crp_iv_start = skip + hlen - sav->ivlen;

	if (V_async_crypto)
		return (crypto_dispatch_async(crp, CRYPTO_ASYNC_ORDERED));
	else
		return (crypto_dispatch(crp));

crp_aad_fail:
	free(xd, M_ESP);
xd_fail:
	crypto_freereq(crp);
	ESPSTAT_INC(esps_crypto);
	error = ENOBUFS;
bad:
	m_freem(m);
	key_freesav(&sav);
	return (error);
}

/*
 * ESP input callback from the crypto driver.
 */
static int
esp_input_cb(struct cryptop *crp)
{
	IPSEC_DEBUG_DECLARE(char buf[128]);
	uint8_t lastthree[3];
	const struct auth_hash *esph;
	struct mbuf *m;
	struct xform_data *xd;
	struct secasvar *sav;
	struct secasindex *saidx;
	crypto_session_t cryptoid;
	int hlen, skip, protoff, error, alen;

	SECASVAR_RLOCK_TRACKER;

	m = crp->crp_buf.cb_mbuf;
	xd = crp->crp_opaque;
	CURVNET_SET(xd->vnet);
	sav = xd->sav;
	if (sav->state >= SADB_SASTATE_DEAD) {
		/* saidx is freed */
		DPRINTF(("%s: dead SA %p spi %#x\n", __func__, sav, sav->spi));
		ESPSTAT_INC(esps_notdb);
		error = ESRCH;
		goto bad;
	}
	skip = xd->skip;
	protoff = xd->protoff;
	cryptoid = xd->cryptoid;
	saidx = &sav->sah->saidx;
	esph = sav->tdb_authalgxform;

	/* Check for crypto errors */
	if (crp->crp_etype) {
		if (crp->crp_etype == EAGAIN) {
			/* Reset the session ID */
			if (ipsec_updateid(sav, &crp->crp_session, &cryptoid) != 0)
				crypto_freesession(cryptoid);
			xd->cryptoid = crp->crp_session;
			CURVNET_RESTORE();
			return (crypto_dispatch(crp));
		}

		/* EBADMSG indicates authentication failure. */
		if (!(crp->crp_etype == EBADMSG && esph != NULL)) {
			ESPSTAT_INC(esps_noxform);
			DPRINTF(("%s: crypto error %d\n", __func__,
				crp->crp_etype));
			error = crp->crp_etype;
			goto bad;
		}
	}

	/* Shouldn't happen... */
	if (m == NULL) {
		ESPSTAT_INC(esps_crypto);
		DPRINTF(("%s: bogus returned buffer from crypto\n", __func__));
		error = EINVAL;
		goto bad;
	}
	ESPSTAT_INC(esps_hist[sav->alg_enc]);

	/* If authentication was performed, check now. */
	if (esph != NULL) {
		alen = xform_ah_authsize(esph);
		AHSTAT_INC(ahs_hist[sav->alg_auth]);
		if (crp->crp_etype == EBADMSG) {
			DPRINTF(("%s: authentication hash mismatch for "
			    "packet in SA %s/%08lx\n", __func__,
			    ipsec_address(&saidx->dst, buf, sizeof(buf)),
			    (u_long) ntohl(sav->spi)));
			ESPSTAT_INC(esps_badauth);
			error = EACCES;
			goto bad;
		}
		m->m_flags |= M_AUTHIPDGM;
		/* Remove trailing authenticator */
		m_adj(m, -alen);
	}

	/* Release the crypto descriptors */
	free(xd, M_ESP), xd = NULL;
	free(crp->crp_aad, M_ESP), crp->crp_aad = NULL;
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
		SECASVAR_RLOCK(sav);
		if (ipsec_updatereplay(ntohl(seq), sav)) {
			SECASVAR_RUNLOCK(sav);
			DPRINTF(("%s: packet replay check for %s\n", __func__,
			    ipsec_sa2str(sav, buf, sizeof(buf))));
			ESPSTAT_INC(esps_replay);
			error = EACCES;
			goto bad;
		}
		SECASVAR_RUNLOCK(sav);
	}

	/* Determine the ESP header length */
	if (sav->flags & SADB_X_EXT_OLD)
		hlen = sizeof (struct esp) + sav->ivlen;
	else
		hlen = sizeof (struct newesp) + sav->ivlen;

	/* Remove the ESP header and IV from the mbuf. */
	error = m_striphdr(m, skip, hlen);
	if (error) {
		ESPSTAT_INC(esps_hdrops);
		DPRINTF(("%s: bad mbuf chain, SA %s/%08lx\n", __func__,
		    ipsec_address(&sav->sah->saidx.dst, buf, sizeof(buf)),
		    (u_long) ntohl(sav->spi)));
		goto bad;
	}

	/* Save the last three bytes of decrypted data */
	m_copydata(m, m->m_pkthdr.len - 3, 3, lastthree);

	/* Verify pad length */
	if (lastthree[1] + 2 > m->m_pkthdr.len - skip) {
		ESPSTAT_INC(esps_badilen);
		DPRINTF(("%s: invalid padding length %d for %u byte packet "
		    "in SA %s/%08lx\n", __func__, lastthree[1],
		    m->m_pkthdr.len - skip,
		    ipsec_address(&sav->sah->saidx.dst, buf, sizeof(buf)),
		    (u_long) ntohl(sav->spi)));
		error = EINVAL;
		goto bad;
	}

	/* Verify correct decryption by checking the last padding bytes */
	if ((sav->flags & SADB_X_EXT_PMASK) != SADB_X_EXT_PRAND) {
		if (lastthree[1] != lastthree[0] && lastthree[1] != 0) {
			ESPSTAT_INC(esps_badenc);
			DPRINTF(("%s: decryption failed for packet in "
			    "SA %s/%08lx\n", __func__, ipsec_address(
			    &sav->sah->saidx.dst, buf, sizeof(buf)),
			    (u_long) ntohl(sav->spi)));
			error = EINVAL;
			goto bad;
		}
	}

	/*
	 * RFC4303 2.6:
	 * Silently drop packet if next header field is IPPROTO_NONE.
	 */
	if (lastthree[2] == IPPROTO_NONE)
		goto bad;

	/* Trim the mbuf chain to remove trailing authenticator and padding */
	m_adj(m, -(lastthree[1] + 2));

	/* Restore the Next Protocol field */
	m_copyback(m, protoff, sizeof (u_int8_t), lastthree + 2);

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
	CURVNET_RESTORE();
	return error;
bad:
	if (sav != NULL)
		key_freesav(&sav);
	if (m != NULL)
		m_freem(m);
	if (xd != NULL)
		free(xd, M_ESP);
	if (crp != NULL) {
		free(crp->crp_aad, M_ESP);
		crypto_freereq(crp);
	}
	CURVNET_RESTORE();
	return error;
}
/*
 * ESP output routine, called by ipsec[46]_perform_request().
 */
static int
esp_output(struct mbuf *m, struct secpolicy *sp, struct secasvar *sav,
    u_int idx, int skip, int protoff)
{
	IPSEC_DEBUG_DECLARE(char buf[IPSEC_ADDRSTRLEN]);
	struct cryptop *crp;
	const struct auth_hash *esph;
	const struct enc_xform *espx;
	struct mbuf *mo = NULL;
	struct xform_data *xd;
	struct secasindex *saidx;
	unsigned char *pad;
	uint8_t *ivp;
	uint64_t cntr;
	crypto_session_t cryptoid;
	int hlen, rlen, padding, blks, alen, i, roff;
	int error, maxpacketsize;
	uint8_t prot;
	uint32_t seqh;
	const struct crypto_session_params *csp;

	SECASVAR_RLOCK_TRACKER;

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
	 * RFC4303 2.4 Requires 4 byte alignment.
	 * Old versions of FreeBSD can't decrypt partial blocks encrypted
	 * with AES-CTR. Align payload to native_blocksize (16 bytes)
	 * in order to preserve compatibility.
	 */
	if (SAV_ISCTR(sav) && V_esp_ctr_compatibility)
		blks = MAX(4, espx->native_blocksize);	/* Cipher blocksize */
	else
		blks = MAX(4, espx->blocksize);

	/* XXX clamp padding length a la KAME??? */
	padding = ((blks - ((rlen + 2) % blks)) % blks) + 2;

	alen = xform_ah_authsize(esph);

	ESPSTAT_INC(esps_output);

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
		    saidx->dst.sa.sa_family, ipsec_address(&saidx->dst,
			buf, sizeof(buf)), (u_long) ntohl(sav->spi)));
		ESPSTAT_INC(esps_nopf);
		error = EPFNOSUPPORT;
		goto bad;
	}
	/*
	DPRINTF(("%s: skip %d hlen %d rlen %d padding %d alen %d blksd %d\n",
		__func__, skip, hlen, rlen, padding, alen, blks)); */
	if (skip + hlen + rlen + padding + alen > maxpacketsize) {
		DPRINTF(("%s: packet in SA %s/%08lx got too big "
		    "(len %u, max len %u)\n", __func__,
		    ipsec_address(&saidx->dst, buf, sizeof(buf)),
		    (u_long) ntohl(sav->spi),
		    skip + hlen + rlen + padding + alen, maxpacketsize));
		ESPSTAT_INC(esps_toobig);
		error = EMSGSIZE;
		goto bad;
	}

	/* Update the counters. */
	ESPSTAT_ADD(esps_obytes, m->m_pkthdr.len - skip);

	m = m_unshare(m, M_NOWAIT);
	if (m == NULL) {
		DPRINTF(("%s: cannot clone mbuf chain, SA %s/%08lx\n", __func__,
		    ipsec_address(&saidx->dst, buf, sizeof(buf)),
		    (u_long) ntohl(sav->spi)));
		ESPSTAT_INC(esps_hdrops);
		error = ENOBUFS;
		goto bad;
	}

	/* Inject ESP header. */
	mo = m_makespace(m, skip, hlen, &roff);
	if (mo == NULL) {
		DPRINTF(("%s: %u byte ESP hdr inject failed for SA %s/%08lx\n",
		    __func__, hlen, ipsec_address(&saidx->dst, buf,
		    sizeof(buf)), (u_long) ntohl(sav->spi)));
		ESPSTAT_INC(esps_hdrops);	/* XXX diffs from openbsd */
		error = ENOBUFS;
		goto bad;
	}

	/* Initialize ESP header. */
	bcopy((caddr_t) &sav->spi, mtod(mo, caddr_t) + roff,
	    sizeof(uint32_t));
	SECASVAR_RLOCK(sav);
	if (sav->replay) {
		uint32_t replay;

		SECREPLAY_LOCK(sav->replay);
#ifdef REGRESSION
		/* Emulate replay attack when ipsec_replay is TRUE. */
		if (!V_ipsec_replay)
#endif
			sav->replay->count++;
		replay = htonl((uint32_t)sav->replay->count);

		bcopy((caddr_t) &replay, mtod(mo, caddr_t) + roff +
		    sizeof(uint32_t), sizeof(uint32_t));

		seqh = htonl((uint32_t)(sav->replay->count >> IPSEC_SEQH_SHIFT));
		SECREPLAY_UNLOCK(sav->replay);
	}
	cryptoid = sav->tdb_cryptoid;
	if (SAV_ISCTRORGCM(sav) || SAV_ISCHACHA(sav))
		cntr = sav->cntr++;
	SECASVAR_RUNLOCK(sav);

	/*
	 * Add padding -- better to do it ourselves than use the crypto engine,
	 * although if/when we support compression, we'd have to do that.
	 */
	pad = (u_char *) m_pad(m, padding + alen);
	if (pad == NULL) {
		DPRINTF(("%s: m_pad failed for SA %s/%08lx\n", __func__,
		    ipsec_address(&saidx->dst, buf, sizeof(buf)),
		    (u_long) ntohl(sav->spi)));
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
		arc4random_buf(pad, padding - 2);
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

	/* Get crypto descriptor. */
	crp = crypto_getreq(cryptoid, M_NOWAIT);
	if (crp == NULL) {
		DPRINTF(("%s: failed to acquire crypto descriptor\n",
			__func__));
		ESPSTAT_INC(esps_crypto);
		error = ENOBUFS;
		goto bad;
	}

	/* IPsec-specific opaque crypto info. */
	xd = malloc(sizeof(struct xform_data), M_ESP, M_NOWAIT | M_ZERO);
	if (xd == NULL) {
		DPRINTF(("%s: failed to allocate xform_data\n", __func__));
		goto xd_fail;
	}

	/* Encryption descriptor. */
	crp->crp_payload_start = skip + hlen;
	crp->crp_payload_length = m->m_pkthdr.len - (skip + hlen + alen);
	crp->crp_op = CRYPTO_OP_ENCRYPT;

	/* Generate cipher and ESP IVs. */
	ivp = &crp->crp_iv[0];
	if (SAV_ISCTRORGCM(sav) || SAV_ISCHACHA(sav)) {
		/*
		 * See comment in esp_input() for details on the
		 * cipher IV.  A simple per-SA counter stored in
		 * 'cntr' is used as the explicit ESP IV.
		 */
		memcpy(ivp, sav->key_enc->key_data +
		    _KEYLEN(sav->key_enc) - 4, 4);
		be64enc(&ivp[4], cntr);
		if (SAV_ISCTR(sav)) {
			be32enc(&ivp[sav->ivlen + 4], 1);
		}
		m_copyback(m, skip + hlen - sav->ivlen, sav->ivlen, &ivp[4]);
		crp->crp_flags |= CRYPTO_F_IV_SEPARATE;
	} else if (sav->ivlen != 0) {
		arc4rand(ivp, sav->ivlen, 0);
		crp->crp_iv_start = skip + hlen - sav->ivlen;
		m_copyback(m, crp->crp_iv_start, sav->ivlen, ivp);
	}

	/* Callback parameters */
	xd->sp = sp;
	xd->sav = sav;
	xd->idx = idx;
	xd->cryptoid = cryptoid;
	xd->vnet = curvnet;

	/* Crypto operation descriptor. */
	crp->crp_flags |= CRYPTO_F_CBIFSYNC;
	crypto_use_mbuf(crp, m);
	crp->crp_callback = esp_output_cb;
	crp->crp_opaque = xd;

	if (esph) {
		/* Authentication descriptor. */
		crp->crp_op |= CRYPTO_OP_COMPUTE_DIGEST;
		if (SAV_ISGCM(sav) || SAV_ISCHACHA(sav))
			crp->crp_aad_length = 8; /* RFC4106 5, SPI + SN */
		else
			crp->crp_aad_length = hlen;

		csp = crypto_get_params(crp->crp_session);
		if (csp->csp_flags & CSP_F_SEPARATE_AAD &&
		    sav->replay != NULL) {
			int aad_skip;

			crp->crp_aad_length += sizeof(seqh);
			crp->crp_aad = malloc(crp->crp_aad_length, M_ESP, M_NOWAIT);
			if (crp->crp_aad == NULL) {
				DPRINTF(("%s: failed to allocate xform_data\n",
					 __func__));
				goto crp_aad_fail;
			}

			/* SPI */
			m_copydata(m, skip, SPI_SIZE, crp->crp_aad);
			aad_skip = SPI_SIZE;

			/* ESN */
			bcopy(&seqh, (char *)crp->crp_aad + aad_skip, sizeof(seqh));
			aad_skip += sizeof(seqh);

			/* Rest of aad */
			if (crp->crp_aad_length - aad_skip > 0)
				m_copydata(m, skip + SPI_SIZE,
					   crp->crp_aad_length - aad_skip,
					   (char *)crp->crp_aad + aad_skip);
		} else
			crp->crp_aad_start = skip;

		if (csp->csp_flags & CSP_F_ESN && sav->replay != NULL)
			memcpy(crp->crp_esn, &seqh, sizeof(seqh));

		crp->crp_digest_start = m->m_pkthdr.len - alen;
	}

	if (V_async_crypto)
		return (crypto_dispatch_async(crp, CRYPTO_ASYNC_ORDERED));
	else
		return (crypto_dispatch(crp));

crp_aad_fail:
	free(xd, M_ESP);
xd_fail:
	crypto_freereq(crp);
	ESPSTAT_INC(esps_crypto);
	error = ENOBUFS;
bad:
	if (m)
		m_freem(m);
	key_freesav(&sav);
	key_freesp(&sp);
	return (error);
}
/*
 * ESP output callback from the crypto driver.
 */
static int
esp_output_cb(struct cryptop *crp)
{
	struct xform_data *xd;
	struct secpolicy *sp;
	struct secasvar *sav;
	struct mbuf *m;
	crypto_session_t cryptoid;
	u_int idx;
	int error;

	xd = (struct xform_data *) crp->crp_opaque;
	CURVNET_SET(xd->vnet);
	m = crp->crp_buf.cb_mbuf;
	sp = xd->sp;
	sav = xd->sav;
	idx = xd->idx;
	cryptoid = xd->cryptoid;

	/* Check for crypto errors. */
	if (crp->crp_etype) {
		if (crp->crp_etype == EAGAIN) {
			/* Reset the session ID */
			if (ipsec_updateid(sav, &crp->crp_session, &cryptoid) != 0)
				crypto_freesession(cryptoid);
			xd->cryptoid = crp->crp_session;
			CURVNET_RESTORE();
			return (crypto_dispatch(crp));
		}
		ESPSTAT_INC(esps_noxform);
		DPRINTF(("%s: crypto error %d\n", __func__, crp->crp_etype));
		error = crp->crp_etype;
		m_freem(m);
		goto bad;
	}

	/* Shouldn't happen... */
	if (m == NULL) {
		ESPSTAT_INC(esps_crypto);
		DPRINTF(("%s: bogus returned buffer from crypto\n", __func__));
		error = EINVAL;
		goto bad;
	}
	free(xd, M_ESP);
	free(crp->crp_aad, M_ESP);
	crypto_freereq(crp);
	ESPSTAT_INC(esps_hist[sav->alg_enc]);
	if (sav->tdb_authalgxform != NULL)
		AHSTAT_INC(ahs_hist[sav->alg_auth]);

#ifdef REGRESSION
	/* Emulate man-in-the-middle attack when ipsec_integrity is TRUE. */
	if (V_ipsec_integrity) {
		static unsigned char ipseczeroes[AH_HMAC_MAXHASHLEN];
		const struct auth_hash *esph;

		/*
		 * Corrupt HMAC if we want to test integrity verification of
		 * the other side.
		 */
		esph = sav->tdb_authalgxform;
		if (esph !=  NULL) {
			int alen;

			alen = xform_ah_authsize(esph);
			m_copyback(m, m->m_pkthdr.len - alen,
			    alen, ipseczeroes);
		}
	}
#endif

	/* NB: m is reclaimed by ipsec_process_done. */
	error = ipsec_process_done(m, sp, sav, idx);
	CURVNET_RESTORE();
	return (error);
bad:
	free(xd, M_ESP);
	free(crp->crp_aad, M_ESP);
	crypto_freereq(crp);
	key_freesav(&sav);
	key_freesp(&sp);
	CURVNET_RESTORE();
	return (error);
}

static struct xformsw esp_xformsw = {
	.xf_type =	XF_ESP,
	.xf_name =	"IPsec ESP",
	.xf_init =	esp_init,
	.xf_cleanup =	esp_cleanup,
	.xf_input =	esp_input,
	.xf_output =	esp_output,
};

SYSINIT(esp_xform_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_MIDDLE,
    xform_attach, &esp_xformsw);
SYSUNINIT(esp_xform_uninit, SI_SUB_PROTO_DOMAIN, SI_ORDER_MIDDLE,
    xform_detach, &esp_xformsw);
