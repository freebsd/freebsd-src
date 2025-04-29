/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
 * All rights reserved.
 * Copyright (c) 2025 Adrian Chadd <adrian@FreeBSD.org>.
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

/*
 * IEEE 802.11 AES-GCMP crypto support.
 *
 * The AES-GCM crypto routines in sys/net80211/ieee80211_crypto_gcm.[ch]
 * are derived from similar code in hostapd 2.11 (src/crypto/aes-gcm.c).
 * The code is used with the consent of the author and its licence is
 * included in the above source files.
 */
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_crypto_gcm.h>

#include <crypto/rijndael/rijndael.h>

#define AES_BLOCK_LEN 16

/*
 * Note: GCMP_MIC_LEN defined in ieee80211_crypto_gcm.h, as it is also
 * used by the AES-GCM routines for sizing the S and T hashes which are
 * used by GCMP as the MIC.
 */
#define	GCMP_PN_LEN	6
#define	GCMP_IV_LEN	12

struct gcmp_ctx {
	struct ieee80211vap *cc_vap;	/* for diagnostics+statistics */
	struct ieee80211com *cc_ic;
	rijndael_ctx	     cc_aes;
};

static	void *gcmp_attach(struct ieee80211vap *, struct ieee80211_key *);
static	void gcmp_detach(struct ieee80211_key *);
static	int gcmp_setkey(struct ieee80211_key *);
static	void gcmp_setiv(struct ieee80211_key *, uint8_t *);
static	int gcmp_encap(struct ieee80211_key *, struct mbuf *);
static	int gcmp_decap(struct ieee80211_key *, struct mbuf *, int);
static	int gcmp_enmic(struct ieee80211_key *, struct mbuf *, int);
static	int gcmp_demic(struct ieee80211_key *, struct mbuf *, int);

static const struct ieee80211_cipher gcmp = {
	.ic_name	= "AES-GCMP",
	.ic_cipher	= IEEE80211_CIPHER_AES_GCM_128,
	.ic_header	= IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN +
			  IEEE80211_WEP_EXTIVLEN,
	.ic_trailer	= GCMP_MIC_LEN,
	.ic_miclen	= 0,
	.ic_attach	= gcmp_attach,
	.ic_detach	= gcmp_detach,
	.ic_setkey	= gcmp_setkey,
	.ic_setiv	= gcmp_setiv,
	.ic_encap	= gcmp_encap,
	.ic_decap	= gcmp_decap,
	.ic_enmic	= gcmp_enmic,
	.ic_demic	= gcmp_demic,
};

static const struct ieee80211_cipher gcmp_256 = {
	.ic_name	= "AES-GCMP-256",
	.ic_cipher	= IEEE80211_CIPHER_AES_GCM_256,
	.ic_header	= IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN +
			  IEEE80211_WEP_EXTIVLEN,
	.ic_trailer	= GCMP_MIC_LEN,
	.ic_miclen	= 0,
	.ic_attach	= gcmp_attach,
	.ic_detach	= gcmp_detach,
	.ic_setkey	= gcmp_setkey,
	.ic_setiv	= gcmp_setiv,
	.ic_encap	= gcmp_encap,
	.ic_decap	= gcmp_decap,
	.ic_enmic	= gcmp_enmic,
	.ic_demic	= gcmp_demic,
};


static	int gcmp_encrypt(struct ieee80211_key *, struct mbuf *, int hdrlen);
static	int gcmp_decrypt(struct ieee80211_key *, u_int64_t pn,
		struct mbuf *, int hdrlen);

/* number of references from net80211 layer */
static	int nrefs = 0;

static void *
gcmp_attach(struct ieee80211vap *vap, struct ieee80211_key *k)
{
	struct gcmp_ctx *ctx;

	ctx = (struct gcmp_ctx *) IEEE80211_MALLOC(sizeof(struct gcmp_ctx),
		M_80211_CRYPTO, IEEE80211_M_NOWAIT | IEEE80211_M_ZERO);
	if (ctx == NULL) {
		vap->iv_stats.is_crypto_nomem++;
		return (NULL);
	}
	ctx->cc_vap = vap;
	ctx->cc_ic = vap->iv_ic;
	nrefs++;			/* NB: we assume caller locking */
	return (ctx);
}

static void
gcmp_detach(struct ieee80211_key *k)
{
	struct gcmp_ctx *ctx = k->wk_private;

	IEEE80211_FREE(ctx, M_80211_CRYPTO);
	KASSERT(nrefs > 0, ("imbalanced attach/detach"));
	nrefs--;			/* NB: we assume caller locking */
}

static int
gcmp_get_trailer_len(struct ieee80211_key *k)
{
	return (k->wk_cipher->ic_trailer);
}

static int
gcmp_get_header_len(struct ieee80211_key *k)
{
	return (k->wk_cipher->ic_header);
}

static int
gcmp_setkey(struct ieee80211_key *k)
{
	uint32_t keylen;

	struct gcmp_ctx *ctx = k->wk_private;

	switch (k->wk_cipher->ic_cipher) {
	case IEEE80211_CIPHER_AES_GCM_128:
		keylen = 128;
		break;
	case IEEE80211_CIPHER_AES_GCM_256:
		keylen = 256;
		break;
	default:
		IEEE80211_DPRINTF(ctx->cc_vap, IEEE80211_MSG_CRYPTO,
			"%s: Unexpected cipher (%u)",
			__func__, k->wk_cipher->ic_cipher);
		return (0);
	}

	if (k->wk_keylen != (keylen/NBBY)) {
		IEEE80211_DPRINTF(ctx->cc_vap, IEEE80211_MSG_CRYPTO,
			"%s: Invalid key length %u, expecting %u\n",
			__func__, k->wk_keylen, keylen/NBBY);
		return (0);
	}
	if (k->wk_flags & IEEE80211_KEY_SWENCRYPT)
		rijndael_set_key(&ctx->cc_aes, k->wk_key, k->wk_keylen*NBBY);
	return (1);
}

static void
gcmp_setiv(struct ieee80211_key *k, uint8_t *ivp)
{
	struct gcmp_ctx *ctx = k->wk_private;
	struct ieee80211vap *vap = ctx->cc_vap;
	uint8_t keyid;

	keyid = ieee80211_crypto_get_keyid(vap, k) << 6;

	k->wk_keytsc++;
	ivp[0] = k->wk_keytsc >> 0;		/* PN0 */
	ivp[1] = k->wk_keytsc >> 8;		/* PN1 */
	ivp[2] = 0;				/* Reserved */
	ivp[3] = keyid | IEEE80211_WEP_EXTIV;	/* KeyID | ExtID */
	ivp[4] = k->wk_keytsc >> 16;		/* PN2 */
	ivp[5] = k->wk_keytsc >> 24;		/* PN3 */
	ivp[6] = k->wk_keytsc >> 32;		/* PN4 */
	ivp[7] = k->wk_keytsc >> 40;		/* PN5 */
}

/*
 * Add privacy headers appropriate for the specified key.
 */
static int
gcmp_encap(struct ieee80211_key *k, struct mbuf *m)
{
	const struct ieee80211_frame *wh;
	struct gcmp_ctx *ctx = k->wk_private;
	struct ieee80211com *ic = ctx->cc_ic;
	uint8_t *ivp;
	int hdrlen;
	int is_mgmt;

	hdrlen = ieee80211_hdrspace(ic, mtod(m, void *));
	wh = mtod(m, const struct ieee80211_frame *);
	is_mgmt = IEEE80211_IS_MGMT(wh);

	/*
	 * Check to see if we need to insert IV/MIC.
	 *
	 * Some offload devices don't require the IV to be inserted
	 * as part of the hardware encryption.
	 */
	if (is_mgmt && (k->wk_flags & IEEE80211_KEY_NOIVMGT))
		return (1);
	if (!is_mgmt && (k->wk_flags & IEEE80211_KEY_NOIV))
		return (1);

	/*
	 * Copy down 802.11 header and add the IV, KeyID, and ExtIV.
	 */
	M_PREPEND(m, gcmp_get_header_len(k), IEEE80211_M_NOWAIT);
	if (m == NULL)
		return (0);
	ivp = mtod(m, uint8_t *);
	ovbcopy(ivp + gcmp_get_header_len(k), ivp, hdrlen);
	ivp += hdrlen;

	gcmp_setiv(k, ivp);

	/*
	 * Finally, do software encrypt if needed.
	 */
	if ((k->wk_flags & IEEE80211_KEY_SWENCRYPT) &&
	    !gcmp_encrypt(k, m, hdrlen))
		return (0);

	return (1);
}

/*
 * Add MIC to the frame as needed.
 */
static int
gcmp_enmic(struct ieee80211_key *k, struct mbuf *m, int force)
{
	return (1);
}

static __inline uint64_t
READ_6(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5)
{
	uint32_t iv32 = (b0 << 0) | (b1 << 8) | (b2 << 16) | (b3 << 24);
	uint16_t iv16 = (b4 << 0) | (b5 << 8);
	return ((((uint64_t)iv16) << 32) | iv32);
}

/*
 * Validate and strip privacy headers (and trailer) for a
 * received frame. The specified key should be correct but
 * is also verified.
 */
static int
gcmp_decap(struct ieee80211_key *k, struct mbuf *m, int hdrlen)
{
	const struct ieee80211_rx_stats *rxs;
	struct gcmp_ctx *ctx = k->wk_private;
	struct ieee80211vap *vap = ctx->cc_vap;
	struct ieee80211_frame *wh;
	uint8_t *ivp, tid;
	uint64_t pn;
	bool noreplaycheck;

	rxs = ieee80211_get_rx_params_ptr(m);

	if ((rxs != NULL) && (rxs->c_pktflags & IEEE80211_RX_F_IV_STRIP) != 0)
		goto finish;

	/*
	 * Header should have extended IV and sequence number;
	 * verify the former and validate the latter.
	 */
	wh = mtod(m, struct ieee80211_frame *);
	ivp = mtod(m, uint8_t *) + hdrlen;
	if ((ivp[IEEE80211_WEP_IVLEN] & IEEE80211_WEP_EXTIV) == 0) {
		/*
		 * No extended IV; discard frame.
		 */
		IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO, wh->i_addr2,
			"%s", "missing ExtIV for AES-GCM cipher");
		vap->iv_stats.is_rx_gcmpformat++;
		return (0);
	}
	tid = ieee80211_gettid(wh);
	pn = READ_6(ivp[0], ivp[1], ivp[4], ivp[5], ivp[6], ivp[7]);

	noreplaycheck = (k->wk_flags & IEEE80211_KEY_NOREPLAY) != 0;
	noreplaycheck |= (rxs != NULL) &&
	    (rxs->c_pktflags & IEEE80211_RX_F_PN_VALIDATED) != 0;
	if (pn <= k->wk_keyrsc[tid] && !noreplaycheck) {
		/*
		 * Replay violation.
		 */
		ieee80211_notify_replay_failure(vap, wh, k, pn, tid);
		vap->iv_stats.is_rx_gcmpreplay++;
		return (0);
	}

	/*
	 * Check if the device handled the decrypt in hardware.
	 * If so we just strip the header; otherwise we need to
	 * handle the decrypt in software.  Note that for the
	 * latter we leave the header in place for use in the
	 * decryption work.
	 */
	if ((k->wk_flags & IEEE80211_KEY_SWDECRYPT) &&
	    !gcmp_decrypt(k, pn, m, hdrlen))
		return (0);

finish:
	/*
	 * Copy up 802.11 header and strip crypto bits.
	 */
	if ((rxs == NULL) || (rxs->c_pktflags & IEEE80211_RX_F_IV_STRIP) == 0) {
		ovbcopy(mtod(m, void *), mtod(m, uint8_t *) +
		    gcmp_get_header_len(k), hdrlen);
		m_adj(m, gcmp_get_header_len(k));
	}

	if ((rxs == NULL) || (rxs->c_pktflags & IEEE80211_RX_F_MIC_STRIP) == 0)
		m_adj(m, -gcmp_get_trailer_len(k));

	/*
	 * Ok to update rsc now.
	 */
	if ((rxs == NULL) || (rxs->c_pktflags & IEEE80211_RX_F_IV_STRIP) == 0) {
		/*
		 * Do not go backwards in the IEEE80211_KEY_NOREPLAY cases
		 * or in case hardware has checked but frames are arriving
		 * reordered (e.g., LinuxKPI drivers doing RSS which we are
		 * not prepared for at all).
		 */
		if (pn > k->wk_keyrsc[tid])
			k->wk_keyrsc[tid] = pn;
	}

	return (1);
}

/*
 * Verify and strip MIC from the frame.
 */
static int
gcmp_demic(struct ieee80211_key *k, struct mbuf *m, int force)
{
	return (1);
}

/*
 * Populate the 12 byte / 96 bit IV buffer.
 */
static int
gcmp_init_iv(uint8_t *iv, const struct ieee80211_frame *wh, u_int64_t pn)
{
	uint8_t j_pn[GCMP_PN_LEN];

	/* Construct the pn buffer */
	j_pn[0] = pn >> 40;
	j_pn[1] = pn >> 32;
	j_pn[2] = pn >> 24;
	j_pn[3] = pn >> 16;
	j_pn[4] = pn >> 8;
	j_pn[5] = pn >> 0;

	memcpy(iv, wh->i_addr2, IEEE80211_ADDR_LEN);
	memcpy(iv + IEEE80211_ADDR_LEN, j_pn, GCMP_PN_LEN);

	return (GCMP_IV_LEN); /* 96 bits */
}

/*
 * @brief Encrypt an mbuf.
 *
 * This uses a temporary memory buffer to encrypt; the
 * current AES-GCM code expects things in a contiguous buffer
 * and this avoids the need of breaking out the GCTR and
 * GHASH routines into using mbuf iterators.
 *
 * @param key	ieee80211_key to use
 * @param mbuf	802.11 frame to encrypt
 * @param hdrlen	the length of the 802.11 header, including any padding
 * @returns 0 if error, > 0 if OK.
 */
static int
gcmp_encrypt(struct ieee80211_key *key, struct mbuf *m0, int hdrlen)
{
	struct gcmp_ctx *ctx = key->wk_private;
	struct ieee80211_frame *wh;
	struct mbuf *m = m0;
	int data_len, aad_len, iv_len, ret;
	uint8_t aad[GCM_AAD_LEN];
	uint8_t T[GCMP_MIC_LEN];
	uint8_t iv[GCMP_IV_LEN];
	uint8_t *p_pktbuf = NULL;
	uint8_t *c_pktbuf = NULL;

	wh = mtod(m, struct ieee80211_frame *);
	data_len = m->m_pkthdr.len - (hdrlen + gcmp_get_header_len(key));

	ctx->cc_vap->iv_stats.is_crypto_gcmp++;

	p_pktbuf = IEEE80211_MALLOC(data_len, M_TEMP,
	    IEEE80211_M_NOWAIT | IEEE80211_M_ZERO);
	if (p_pktbuf == NULL) {
		IEEE80211_NOTE_MAC(ctx->cc_vap, IEEE80211_MSG_CRYPTO,
		    wh->i_addr2, "%s",
		    "AES-GCM encrypt failed; couldn't allocate buffer");
		ctx->cc_vap->iv_stats.is_crypto_gcmp_nomem++;
		return (0);
	}
	c_pktbuf = IEEE80211_MALLOC(data_len, M_TEMP,
	    IEEE80211_M_NOWAIT | IEEE80211_M_ZERO);
	if (c_pktbuf == NULL) {
		IEEE80211_NOTE_MAC(ctx->cc_vap, IEEE80211_MSG_CRYPTO,
		    wh->i_addr2, "%s",
		    "AES-GCM encrypt failed; couldn't allocate buffer");
		ctx->cc_vap->iv_stats.is_crypto_gcmp_nomem++;
		IEEE80211_FREE(p_pktbuf, M_TEMP);
		return (0);
	}

	/* Initialise AAD */
	aad_len = ieee80211_crypto_init_aad(wh, aad, GCM_AAD_LEN);

	/* Initialise local Nonce to work on */
	/* TODO: rename iv stuff here to nonce */
	iv_len = gcmp_init_iv(iv, wh, key->wk_keytsc);

	/* Copy mbuf data part into plaintext pktbuf */
	m_copydata(m0, hdrlen + gcmp_get_header_len(key), data_len,
	    p_pktbuf);

	/* Run encrypt */
	/*
	 * Note: aad + 2 to skip over the 2 byte length populated
	 * at the beginning, since it's based on the AAD code in CCMP.
	 */
	ieee80211_crypto_aes_gcm_ae(&ctx->cc_aes, iv, iv_len,
	    p_pktbuf, data_len, aad + 2, aad_len, c_pktbuf, T);

	/* Copy data back over mbuf */
	m_copyback(m0, hdrlen + gcmp_get_header_len(key), data_len,
	    c_pktbuf);

	/* Append MIC */
	ret = m_append(m0, gcmp_get_trailer_len(key), T);
	if (ret == 0) {
		IEEE80211_NOTE_MAC(ctx->cc_vap, IEEE80211_MSG_CRYPTO,
		    wh->i_addr2, "%s",
		    "AES-GCM encrypt failed; couldn't append T");
		ctx->cc_vap->iv_stats.is_crypto_gcmp_nospc++;
	}

	IEEE80211_FREE(p_pktbuf, M_TEMP);
	IEEE80211_FREE(c_pktbuf, M_TEMP);

	return (ret);
}

/*
 * @brief Decrypt an mbuf.
 *
 * This uses a temporary memory buffer to decrypt; the
 * current AES-GCM code expects things in a contiguous buffer
 * and this avoids the need of breaking out the GCTR and
 * GHASH routines into using mbuf iterators.
 *
 * @param key	ieee80211_key to use
 * @param mbuf	802.11 frame to decrypt
 * @param hdrlen	the length of the 802.11 header, including any padding
 * @returns 0 if error, > 0 if OK.
 */
static int
gcmp_decrypt(struct ieee80211_key *key, u_int64_t pn, struct mbuf *m,
    int hdrlen)
{
	const struct ieee80211_rx_stats *rxs;
	struct gcmp_ctx *ctx = key->wk_private;
	struct ieee80211_frame *wh;
	int data_len, aad_len, iv_len, ret;
	uint8_t aad[GCM_AAD_LEN];
	uint8_t T[GCMP_MIC_LEN];
	uint8_t iv[GCMP_IV_LEN];
	uint8_t *p_pktbuf = NULL;
	uint8_t *c_pktbuf = NULL;

	rxs = ieee80211_get_rx_params_ptr(m);
	if ((rxs != NULL) && (rxs->c_pktflags & IEEE80211_RX_F_DECRYPTED) != 0)
		return (1);

	wh = mtod(m, struct ieee80211_frame *);

	/* Data length doesn't include the MIC at the end */
	data_len = m->m_pkthdr.len -
	    (hdrlen + gcmp_get_header_len(key) + GCMP_MIC_LEN);

	ctx->cc_vap->iv_stats.is_crypto_gcmp++;

	p_pktbuf = IEEE80211_MALLOC(data_len, M_TEMP,
	    IEEE80211_M_NOWAIT | IEEE80211_M_ZERO);
	if (p_pktbuf == NULL) {
		ctx->cc_vap->iv_stats.is_crypto_gcmp_nomem++;
		return (0);
	}
	c_pktbuf = IEEE80211_MALLOC(data_len, M_TEMP,
	    IEEE80211_M_NOWAIT | IEEE80211_M_ZERO);
	if (c_pktbuf == NULL) {
		ctx->cc_vap->iv_stats.is_crypto_gcmp_nomem++;
		IEEE80211_FREE(p_pktbuf, M_TEMP);
		return (0);
	}

	/* Initialise AAD */
	aad_len = ieee80211_crypto_init_aad(wh, aad, GCM_AAD_LEN);

	/* Initialise local IV copy to work on */
	iv_len = gcmp_init_iv(iv, wh, pn);

	/* Copy mbuf into ciphertext pktbuf */
	m_copydata(m, hdrlen + gcmp_get_header_len(key), data_len,
	    c_pktbuf);

	/* Copy the MIC into the tag buffer */
	m_copydata(m, hdrlen + gcmp_get_header_len(key) + data_len,
	    GCMP_MIC_LEN, T);

	/* Run decrypt */
	/*
	 * Note: aad + 2 to skip over the 2 byte length populated
	 * at the beginning, since it's based on the AAD code in CCMP.
	 */
	ret = ieee80211_crypto_aes_gcm_ad(&ctx->cc_aes, iv, iv_len,
	    c_pktbuf, data_len, aad + 2, aad_len, T, p_pktbuf);

	/* If the MIC was stripped by HW/driver we are done. */
	if ((rxs != NULL) && (rxs->c_pktflags & IEEE80211_RX_F_MIC_STRIP) != 0)
		goto skip_ok;

	if (ret != 0) {
		/* Decrypt failure */
		ctx->cc_vap->iv_stats.is_rx_gcmpmic++;
		IEEE80211_NOTE_MAC(ctx->cc_vap, IEEE80211_MSG_CRYPTO,
		    wh->i_addr2, "%s", "AES-GCM decrypt failed; MIC mismatch");
		IEEE80211_FREE(p_pktbuf, M_TEMP);
		IEEE80211_FREE(c_pktbuf, M_TEMP);
		return (0);
	}

skip_ok:
	/* Copy data back over mbuf */
	m_copyback(m, hdrlen + gcmp_get_header_len(key), data_len,
	    p_pktbuf);

	IEEE80211_FREE(p_pktbuf, M_TEMP);
	IEEE80211_FREE(c_pktbuf, M_TEMP);

	return (1);
}

/*
 * Module glue.
 */
IEEE80211_CRYPTO_MODULE(gcmp, 1);
IEEE80211_CRYPTO_MODULE_ADD(gcmp_256);
