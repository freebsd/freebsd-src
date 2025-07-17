/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
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

#include <sys/cdefs.h>
/*
 * IEEE 802.11i AES-CCMP crypto support.
 *
 * Part of this module is derived from similar code in the Host
 * AP driver. The code is used with the consent of the author and
 * it's license is included below.
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

#include <crypto/rijndael/rijndael.h>

#define AES_BLOCK_LEN 16

#define CCMP_128_MIC_LEN		8
#define CCMP_256_MIC_LEN		16

struct ccmp_ctx {
	struct ieee80211vap *cc_vap;	/* for diagnostics+statistics */
	struct ieee80211com *cc_ic;
	rijndael_ctx	     cc_aes;
};

static	void *ccmp_attach(struct ieee80211vap *, struct ieee80211_key *);
static	void ccmp_detach(struct ieee80211_key *);
static	int ccmp_setkey(struct ieee80211_key *);
static	void ccmp_setiv(struct ieee80211_key *, uint8_t *);
static	int ccmp_encap(struct ieee80211_key *, struct mbuf *);
static	int ccmp_decap(struct ieee80211_key *, struct mbuf *, int);
static	int ccmp_enmic(struct ieee80211_key *, struct mbuf *, int);
static	int ccmp_demic(struct ieee80211_key *, struct mbuf *, int);

static const struct ieee80211_cipher ccmp = {
	.ic_name	= "AES-CCM",
	.ic_cipher	= IEEE80211_CIPHER_AES_CCM,
	.ic_header	= IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN +
			  IEEE80211_WEP_EXTIVLEN,
	.ic_trailer	= CCMP_128_MIC_LEN,
	.ic_miclen	= 0,
	.ic_attach	= ccmp_attach,
	.ic_detach	= ccmp_detach,
	.ic_setkey	= ccmp_setkey,
	.ic_setiv	= ccmp_setiv,
	.ic_encap	= ccmp_encap,
	.ic_decap	= ccmp_decap,
	.ic_enmic	= ccmp_enmic,
	.ic_demic	= ccmp_demic,
};

static const struct ieee80211_cipher ccmp_256 = {
	.ic_name	= "AES-CCM-256",
	.ic_cipher	= IEEE80211_CIPHER_AES_CCM_256,
	.ic_header	= IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN +
			    IEEE80211_WEP_EXTIVLEN,
	.ic_trailer	= CCMP_256_MIC_LEN,
	.ic_miclen	= 0,
	.ic_attach	= ccmp_attach,
	.ic_detach	= ccmp_detach,
	.ic_setkey	= ccmp_setkey,
	.ic_setiv	= ccmp_setiv,
	.ic_encap	= ccmp_encap,
	.ic_decap	= ccmp_decap,
	.ic_enmic	= ccmp_enmic,
	.ic_demic	= ccmp_demic,
};

static	int ccmp_encrypt(struct ieee80211_key *, struct mbuf *, int hdrlen);
static	int ccmp_decrypt(struct ieee80211_key *, u_int64_t pn,
		struct mbuf *, int hdrlen);

/* number of references from net80211 layer */
static	int nrefs = 0;

static void *
ccmp_attach(struct ieee80211vap *vap, struct ieee80211_key *k)
{
	struct ccmp_ctx *ctx;

	ctx = (struct ccmp_ctx *) IEEE80211_MALLOC(sizeof(struct ccmp_ctx),
		M_80211_CRYPTO, IEEE80211_M_NOWAIT | IEEE80211_M_ZERO);
	if (ctx == NULL) {
		vap->iv_stats.is_crypto_nomem++;
		return NULL;
	}
	ctx->cc_vap = vap;
	ctx->cc_ic = vap->iv_ic;
	nrefs++;			/* NB: we assume caller locking */
	return ctx;
}

static void
ccmp_detach(struct ieee80211_key *k)
{
	struct ccmp_ctx *ctx = k->wk_private;

	IEEE80211_FREE(ctx, M_80211_CRYPTO);
	KASSERT(nrefs > 0, ("imbalanced attach/detach"));
	nrefs--;			/* NB: we assume caller locking */
}

static int
ccmp_get_trailer_len(struct ieee80211_key *k)
{
	return (k->wk_cipher->ic_trailer);
}

static int
ccmp_get_header_len(struct ieee80211_key *k)
{
	return (k->wk_cipher->ic_header);
}

/**
 * @brief Return the M parameter to use for CCMP block0 initialisation.
 *
 * M is defined as the number of bytes in the authentication
 * field.
 *
 * See RFC3610, Section 2 (CCM Mode Specification) for more
 * information.
 *
 * The MIC size is defined in 802.11-2020 12.5.3
 * (CTR with CBC-MAC Protocol (CCMP)).
 *
 * CCM-128 - M=8, MIC is 8 octets.
 * CCM-256 - M=16, MIC is 16 octets.
 *
 * @param key	ieee80211_key to calculate M for
 * @retval the number of bytes in the authentication field
 */
static int
ccmp_get_ccm_m(struct ieee80211_key *k)
{
	if (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_AES_CCM)
		return (8);
	if (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_AES_CCM_256)
		return (16);
	return (8); /* XXX default */
}

static int
ccmp_setkey(struct ieee80211_key *k)
{
	uint32_t keylen;
	struct ccmp_ctx *ctx = k->wk_private;

	switch (k->wk_cipher->ic_cipher) {
	case IEEE80211_CIPHER_AES_CCM:
		keylen = 128;
		break;
	case IEEE80211_CIPHER_AES_CCM_256:
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
		return 0;
	}
	if (k->wk_flags & IEEE80211_KEY_SWENCRYPT)
		rijndael_set_key(&ctx->cc_aes, k->wk_key, k->wk_keylen*NBBY);
	return 1;
}

static void
ccmp_setiv(struct ieee80211_key *k, uint8_t *ivp)
{
	struct ccmp_ctx *ctx = k->wk_private;
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
ccmp_encap(struct ieee80211_key *k, struct mbuf *m)
{
	const struct ieee80211_frame *wh;
	struct ccmp_ctx *ctx = k->wk_private;
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
		return 1;
	if ((! is_mgmt) && (k->wk_flags & IEEE80211_KEY_NOIV))
		return 1;

	/*
	 * Copy down 802.11 header and add the IV, KeyID, and ExtIV.
	 */
	M_PREPEND(m, ccmp_get_header_len(k), IEEE80211_M_NOWAIT);
	if (m == NULL)
		return 0;
	ivp = mtod(m, uint8_t *);
	ovbcopy(ivp + ccmp_get_header_len(k), ivp, hdrlen);
	ivp += hdrlen;

	ccmp_setiv(k, ivp);

	/*
	 * Finally, do software encrypt if needed.
	 */
	if ((k->wk_flags & IEEE80211_KEY_SWENCRYPT) &&
	    !ccmp_encrypt(k, m, hdrlen))
		return 0;

	return 1;
}

/*
 * Add MIC to the frame as needed.
 */
static int
ccmp_enmic(struct ieee80211_key *k, struct mbuf *m, int force)
{

	return 1;
}

static __inline uint64_t
READ_6(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5)
{
	uint32_t iv32 = (b0 << 0) | (b1 << 8) | (b2 << 16) | (b3 << 24);
	uint16_t iv16 = (b4 << 0) | (b5 << 8);
	return (((uint64_t)iv16) << 32) | iv32;
}

/*
 * Validate and strip privacy headers (and trailer) for a
 * received frame. The specified key should be correct but
 * is also verified.
 */
static int
ccmp_decap(struct ieee80211_key *k, struct mbuf *m, int hdrlen)
{
	const struct ieee80211_rx_stats *rxs;
	struct ccmp_ctx *ctx = k->wk_private;
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
			"%s", "missing ExtIV for AES-CCM cipher");
		vap->iv_stats.is_rx_ccmpformat++;
		return 0;
	}
	tid = ieee80211_gettid(wh);
	pn = READ_6(ivp[0], ivp[1], ivp[4], ivp[5], ivp[6], ivp[7]);

	noreplaycheck = (k->wk_flags & IEEE80211_KEY_NOREPLAY) != 0;
	noreplaycheck |= (rxs != NULL) && (rxs->c_pktflags & IEEE80211_RX_F_PN_VALIDATED) != 0;
	if (pn <= k->wk_keyrsc[tid] && !noreplaycheck) {
		/*
		 * Replay violation.
		 */
		ieee80211_notify_replay_failure(vap, wh, k, pn, tid);
		vap->iv_stats.is_rx_ccmpreplay++;
		return 0;
	}

	/*
	 * Check if the device handled the decrypt in hardware.
	 * If so we just strip the header; otherwise we need to
	 * handle the decrypt in software.  Note that for the
	 * latter we leave the header in place for use in the
	 * decryption work.
	 */
	if ((k->wk_flags & IEEE80211_KEY_SWDECRYPT) &&
	    !ccmp_decrypt(k, pn, m, hdrlen))
		return 0;

finish:
	/*
	 * Copy up 802.11 header and strip crypto bits.
	 */
	if (! ((rxs != NULL) && (rxs->c_pktflags & IEEE80211_RX_F_IV_STRIP))) {
		ovbcopy(mtod(m, void *),
		    mtod(m, uint8_t *) + ccmp_get_header_len(k),
		    hdrlen);
		m_adj(m, ccmp_get_header_len(k));
	}

	if ((rxs == NULL) || (rxs->c_pktflags & IEEE80211_RX_F_MIC_STRIP) == 0)
		m_adj(m, -ccmp_get_trailer_len(k));

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

	return 1;
}

/*
 * Verify and strip MIC from the frame.
 */
static int
ccmp_demic(struct ieee80211_key *k, struct mbuf *m, int force)
{
	return 1;
}

static __inline void
xor_block(uint8_t *b, const uint8_t *a, size_t len)
{
	int i;
	for (i = 0; i < len; i++)
		b[i] ^= a[i];
}

/**
 * @brief Initialise the AES-CCM nonce flag field in the b0 CCMP block.
 *
 * The B_0 block is defined in RFC 3610 section 2.2 (Authentication).
 * b0[0] is the CCM flags field, so the nonce used for B_0 starts at
 * b0[1].  Amusingly, b0[1] is also flags, but it's the 802.11 AES-CCM
 * nonce flags field, NOT the CCM flags field.
 *
 * The AES-CCM nonce flags field is defined in 802.11-2020 12.5.3.3.4
 * (Construct CCM nonce).
 *
 * TODO: net80211 currently doesn't support MFP (management frame protection)
 * and so bit 4 is never set.  This routine and ccmp_init_blocks() will
 * need a pointer to the ieee80211_node or a flag that explicitly states
 * the frame will be sent w/ MFP encryption / received w/ MFP decryption.
 *
 * @param wh	the 802.11 header to populate
 * @param b0	the CCM nonce to update (remembering b0[0] is the CCM
 * 		nonce flags, and b0[1] is the AES-CCM nonce flags).
 */
static void
ieee80211_crypto_ccmp_init_nonce_flags(const struct ieee80211_frame *wh,
    char *b0)
{
	if (IEEE80211_IS_DSTODS(wh)) {
		/*
		 * 802.11-2020 12.5.33.3.4 (Construct CCM nonce) mentions
		 * that the low four bits of this byte are the "MPDU priority."
		 * This is defined in 5.1.1.2 (Determination of UP) and
		 * 5.1.1.3 (Interpretation of Priority Parameter in MAC
		 * service primitives).
		 *
		 * The former says "The QoS facility supports eight priority
		 * values, referred to as UPs. The values a UP may take are
		 * the integer values from 0 to 7 and are identical to the
		 * 802.11D priority tags."
		 *
		 * The latter specifically calls out that "Priority parameter
		 * and TID subfield values 0 to 7 are interpreted aas UPs for
		 * the MSDUs" .. and " .. TID subfield values 8 to 15 specify
		 * TIDs that are TS identifiers (TSIDs)" which are used for
		 * TSPEC.  There's a bunch of extra work to be done with frames
		 * received in TIDs 8..15 with no TSPEC, "then the MSDU shall
		 * be sent with priority parameter set to 0."
		 *
		 * All QoS frames (not just QoS data) have TID fields and
		 * thus priorities.  However, the code straight up
		 * copies the 4 bit TID field, rather than a 3 bit MPDU
		 * priority value.  For now, as net80211 doesn't specifically
		 * support TSPEC negotiation, this likely never gets checked.
		 * However as part of any future TSPEC work, this will likely
		 * need to be looked at and checked with interoperability
		 * with other stacks.
		 */
		if (IEEE80211_IS_QOS_ANY(wh)) {
			const struct ieee80211_qosframe_addr4 *qwh4 =
			    (const struct ieee80211_qosframe_addr4 *) wh;
			b0[1] = qwh4->i_qos[0] & 0x0f;	/* prio bits */
		} else {
			b0[1] = 0;
		}
	} else {
		if (IEEE80211_IS_QOS_ANY(wh)) {
			const struct ieee80211_qosframe *qwh =
			    (const struct ieee80211_qosframe *) wh;
			b0[1] = qwh->i_qos[0] & 0x0f;	/* prio bits */
		} else {
			b0[1] = 0;
		}
	}
	/* TODO: populate MFP flag */
}

/*
 * Host AP crypt: host-based CCMP encryption implementation for Host AP driver
 *
 * Copyright (c) 2003-2004, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 */

static void
ccmp_init_blocks(rijndael_ctx *ctx, struct ieee80211_frame *wh,
	uint32_t m, u_int64_t pn, size_t dlen,
	uint8_t b0[AES_BLOCK_LEN], uint8_t aad[2 * AES_BLOCK_LEN],
	uint8_t auth[AES_BLOCK_LEN], uint8_t s0[AES_BLOCK_LEN])
{
	/*
	 * Map M parameter to encoding
	 * RFC3610, Section 2 (CCM Mode Specification)
	 */
	m = (m - 2) / 2;

	/* CCM Initial Block:
	 *
	 * Flag (Include authentication header,
	 *    M=3 or 7 (8 or 16 octet auth field),
	 *    L=1 (2-octet Dlen))
	 *    Adata=1 (one or more auth blocks present)
	 * Nonce: 0x00 | A2 | PN
	 * Dlen
	 */
	b0[0] = 0x40 | 0x01 | (m << 3);
	/* Init b0[1] (CCM nonce flags) */
	ieee80211_crypto_ccmp_init_nonce_flags(wh, b0);
	IEEE80211_ADDR_COPY(b0 + 2, wh->i_addr2);
	b0[8] = pn >> 40;
	b0[9] = pn >> 32;
	b0[10] = pn >> 24;
	b0[11] = pn >> 16;
	b0[12] = pn >> 8;
	b0[13] = pn >> 0;
	b0[14] = (dlen >> 8) & 0xff;
	b0[15] = dlen & 0xff;

	/* Init AAD */
	(void) ieee80211_crypto_init_aad(wh, aad, 2 * AES_BLOCK_LEN);

	/* Start with the first block and AAD */
	rijndael_encrypt(ctx, b0, auth);
	xor_block(auth, aad, AES_BLOCK_LEN);
	rijndael_encrypt(ctx, auth, auth);
	xor_block(auth, &aad[AES_BLOCK_LEN], AES_BLOCK_LEN);
	rijndael_encrypt(ctx, auth, auth);
	b0[0] &= 0x07;
	b0[14] = b0[15] = 0;
	rijndael_encrypt(ctx, b0, s0);
}

#define	CCMP_ENCRYPT(_i, _b, _b0, _pos, _e, _len) do {	\
	/* Authentication */				\
	xor_block(_b, _pos, _len);			\
	rijndael_encrypt(&ctx->cc_aes, _b, _b);		\
	/* Encryption, with counter */			\
	_b0[14] = (_i >> 8) & 0xff;			\
	_b0[15] = _i & 0xff;				\
	rijndael_encrypt(&ctx->cc_aes, _b0, _e);	\
	xor_block(_pos, _e, _len);			\
} while (0)

static int
ccmp_encrypt(struct ieee80211_key *key, struct mbuf *m0, int hdrlen)
{
	struct ccmp_ctx *ctx = key->wk_private;
	struct ieee80211_frame *wh;
	struct mbuf *m = m0;
	int data_len, i, space;
	uint8_t aad[2 * AES_BLOCK_LEN], b0[AES_BLOCK_LEN], b[AES_BLOCK_LEN],
		e[AES_BLOCK_LEN], s0[AES_BLOCK_LEN];
	uint8_t *pos;

	ctx->cc_vap->iv_stats.is_crypto_ccmp++;

	wh = mtod(m, struct ieee80211_frame *);
	data_len = m->m_pkthdr.len - (hdrlen + ccmp_get_header_len(key));
	ccmp_init_blocks(&ctx->cc_aes, wh, ccmp_get_ccm_m(key),
	    key->wk_keytsc, data_len, b0, aad, b, s0);

	i = 1;
	pos = mtod(m, uint8_t *) + hdrlen + ccmp_get_header_len(key);
	/* NB: assumes header is entirely in first mbuf */
	space = m->m_len - (hdrlen + ccmp_get_header_len(key));
	for (;;) {
		if (space > data_len)
			space = data_len;
		/*
		 * Do full blocks.
		 */
		while (space >= AES_BLOCK_LEN) {
			CCMP_ENCRYPT(i, b, b0, pos, e, AES_BLOCK_LEN);
			pos += AES_BLOCK_LEN, space -= AES_BLOCK_LEN;
			data_len -= AES_BLOCK_LEN;
			i++;
		}
		if (data_len <= 0)		/* no more data */
			break;
		m = m->m_next;
		if (m == NULL) {		/* last buffer */
			if (space != 0) {
				/*
				 * Short last block.
				 */
				CCMP_ENCRYPT(i, b, b0, pos, e, space);
			}
			break;
		}
		if (space != 0) {
			uint8_t *pos_next;
			int space_next;
			int len, dl, sp;
			struct mbuf *n;

			/*
			 * Block straddles one or more mbufs, gather data
			 * into the block buffer b, apply the cipher, then
			 * scatter the results back into the mbuf chain.
			 * The buffer will automatically get space bytes
			 * of data at offset 0 copied in+out by the
			 * CCMP_ENCRYPT request so we must take care of
			 * the remaining data.
			 */
			n = m;
			dl = data_len;
			sp = space;
			for (;;) {
				pos_next = mtod(n, uint8_t *);
				len = min(dl, AES_BLOCK_LEN);
				space_next = len > sp ? len - sp : 0;
				if (n->m_len >= space_next) {
					/*
					 * This mbuf has enough data; just grab
					 * what we need and stop.
					 */
					xor_block(b+sp, pos_next, space_next);
					break;
				}
				/*
				 * This mbuf's contents are insufficient,
				 * take 'em all and prepare to advance to
				 * the next mbuf.
				 */
				xor_block(b+sp, pos_next, n->m_len);
				sp += n->m_len, dl -= n->m_len;
				n = n->m_next;
				if (n == NULL)
					break;
			}

			CCMP_ENCRYPT(i, b, b0, pos, e, space);

			/* NB: just like above, but scatter data to mbufs */
			dl = data_len;
			sp = space;
			for (;;) {
				pos_next = mtod(m, uint8_t *);
				len = min(dl, AES_BLOCK_LEN);
				space_next = len > sp ? len - sp : 0;
				if (m->m_len >= space_next) {
					xor_block(pos_next, e+sp, space_next);
					break;
				}
				xor_block(pos_next, e+sp, m->m_len);
				sp += m->m_len, dl -= m->m_len;
				m = m->m_next;
				if (m == NULL)
					goto done;
			}
			/*
			 * Do bookkeeping.  m now points to the last mbuf
			 * we grabbed data from.  We know we consumed a
			 * full block of data as otherwise we'd have hit
			 * the end of the mbuf chain, so deduct from data_len.
			 * Otherwise advance the block number (i) and setup
			 * pos+space to reflect contents of the new mbuf.
			 */
			data_len -= AES_BLOCK_LEN;
			i++;
			pos = pos_next + space_next;
			space = m->m_len - space_next;
		} else {
			/*
			 * Setup for next buffer.
			 */
			pos = mtod(m, uint8_t *);
			space = m->m_len;
		}
	}
done:
	/* tack on MIC */
	xor_block(b, s0, ccmp_get_trailer_len(key));
	return m_append(m0, ccmp_get_trailer_len(key), b);
}
#undef CCMP_ENCRYPT

#define	CCMP_DECRYPT(_i, _b, _b0, _pos, _a, _len) do {	\
	/* Decrypt, with counter */			\
	_b0[14] = (_i >> 8) & 0xff;			\
	_b0[15] = _i & 0xff;				\
	rijndael_encrypt(&ctx->cc_aes, _b0, _b);	\
	xor_block(_pos, _b, _len);			\
	/* Authentication */				\
	xor_block(_a, _pos, _len);			\
	rijndael_encrypt(&ctx->cc_aes, _a, _a);		\
} while (0)

static int
ccmp_decrypt(struct ieee80211_key *key, u_int64_t pn, struct mbuf *m, int hdrlen)
{
	const struct ieee80211_rx_stats *rxs;
	struct ccmp_ctx *ctx = key->wk_private;
	struct ieee80211vap *vap = ctx->cc_vap;
	struct ieee80211_frame *wh;
	uint8_t aad[2 * AES_BLOCK_LEN];
	uint8_t b0[AES_BLOCK_LEN], b[AES_BLOCK_LEN], a[AES_BLOCK_LEN];
	uint8_t mic[AES_BLOCK_LEN];
	size_t data_len;
	int i;
	uint8_t *pos;
	u_int space;

	rxs = ieee80211_get_rx_params_ptr(m);
	if ((rxs != NULL) && (rxs->c_pktflags & IEEE80211_RX_F_DECRYPTED) != 0)
		return (1);

	ctx->cc_vap->iv_stats.is_crypto_ccmp++;

	wh = mtod(m, struct ieee80211_frame *);
	data_len = m->m_pkthdr.len -
	    (hdrlen + ccmp_get_header_len(key) + ccmp_get_trailer_len(key));
	ccmp_init_blocks(&ctx->cc_aes, wh, ccmp_get_ccm_m(key), pn,
	    data_len, b0, aad, a, b);
	m_copydata(m, m->m_pkthdr.len - ccmp_get_trailer_len(key),
	    ccmp_get_trailer_len(key), mic);
	xor_block(mic, b, ccmp_get_trailer_len(key));

	i = 1;
	pos = mtod(m, uint8_t *) + hdrlen + ccmp_get_header_len(key);
	space = m->m_len - (hdrlen + ccmp_get_header_len(key));
	for (;;) {
		if (space > data_len)
			space = data_len;
		while (space >= AES_BLOCK_LEN) {
			CCMP_DECRYPT(i, b, b0, pos, a, AES_BLOCK_LEN);
			pos += AES_BLOCK_LEN, space -= AES_BLOCK_LEN;
			data_len -= AES_BLOCK_LEN;
			i++;
		}
		if (data_len <= 0)		/* no more data */
			break;
		m = m->m_next;
		if (m == NULL) {		/* last buffer */
			if (space != 0)		/* short last block */
				CCMP_DECRYPT(i, b, b0, pos, a, space);
			break;
		}
		if (space != 0) {
			uint8_t *pos_next;
			u_int space_next;
			u_int len;

			/*
			 * Block straddles buffers, split references.  We
			 * do not handle splits that require >2 buffers
			 * since rx'd frames are never badly fragmented
			 * because drivers typically recv in clusters.
			 */
			pos_next = mtod(m, uint8_t *);
			len = min(data_len, AES_BLOCK_LEN);
			space_next = len > space ? len - space : 0;
			KASSERT(m->m_len >= space_next,
				("not enough data in following buffer, "
				"m_len %u need %u\n", m->m_len, space_next));

			xor_block(b+space, pos_next, space_next);
			CCMP_DECRYPT(i, b, b0, pos, a, space);
			xor_block(pos_next, b+space, space_next);
			data_len -= len;
			i++;

			pos = pos_next + space_next;
			space = m->m_len - space_next;
		} else {
			/*
			 * Setup for next buffer.
			 */
			pos = mtod(m, uint8_t *);
			space = m->m_len;
		}
	}

	/*
	 * If the MIC was stripped by HW/driver we are done.
	 */
	if ((rxs != NULL) && (rxs->c_pktflags & IEEE80211_RX_F_MIC_STRIP) != 0)
		return (1);

	if (memcmp(mic, a, ccmp_get_trailer_len(key)) != 0) {
		IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO, wh->i_addr2,
		    "%s", "AES-CCM decrypt failed; MIC mismatch");
		vap->iv_stats.is_rx_ccmpmic++;
		return 0;
	}
	return 1;
}
#undef CCMP_DECRYPT

/*
 * Module glue.
 */
IEEE80211_CRYPTO_MODULE(ccmp, 1);
IEEE80211_CRYPTO_MODULE_ADD(ccmp_256);
