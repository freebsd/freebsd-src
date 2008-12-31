/*-
 * Copyright (c) 2002-2007 Sam Leffler, Errno Consulting
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
__FBSDID("$FreeBSD: src/sys/net80211/ieee80211_crypto_ccmp.c,v 1.10.6.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * IEEE 802.11i AES-CCMP crypto support.
 *
 * Part of this module is derived from similar code in the Host
 * AP driver. The code is used with the consent of the author and
 * it's license is included below.
 */
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

struct ccmp_ctx {
	struct ieee80211com *cc_ic;	/* for diagnostics */
	rijndael_ctx	     cc_aes;
};

static	void *ccmp_attach(struct ieee80211com *, struct ieee80211_key *);
static	void ccmp_detach(struct ieee80211_key *);
static	int ccmp_setkey(struct ieee80211_key *);
static	int ccmp_encap(struct ieee80211_key *k, struct mbuf *, uint8_t keyid);
static	int ccmp_decap(struct ieee80211_key *, struct mbuf *, int);
static	int ccmp_enmic(struct ieee80211_key *, struct mbuf *, int);
static	int ccmp_demic(struct ieee80211_key *, struct mbuf *, int);

static const struct ieee80211_cipher ccmp = {
	.ic_name	= "AES-CCM",
	.ic_cipher	= IEEE80211_CIPHER_AES_CCM,
	.ic_header	= IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN +
			  IEEE80211_WEP_EXTIVLEN,
	.ic_trailer	= IEEE80211_WEP_MICLEN,
	.ic_miclen	= 0,
	.ic_attach	= ccmp_attach,
	.ic_detach	= ccmp_detach,
	.ic_setkey	= ccmp_setkey,
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
ccmp_attach(struct ieee80211com *ic, struct ieee80211_key *k)
{
	struct ccmp_ctx *ctx;

	MALLOC(ctx, struct ccmp_ctx *, sizeof(struct ccmp_ctx),
		M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ctx == NULL) {
		ic->ic_stats.is_crypto_nomem++;
		return NULL;
	}
	ctx->cc_ic = ic;
	nrefs++;			/* NB: we assume caller locking */
	return ctx;
}

static void
ccmp_detach(struct ieee80211_key *k)
{
	struct ccmp_ctx *ctx = k->wk_private;

	FREE(ctx, M_DEVBUF);
	KASSERT(nrefs > 0, ("imbalanced attach/detach"));
	nrefs--;			/* NB: we assume caller locking */
}

static int
ccmp_setkey(struct ieee80211_key *k)
{
	struct ccmp_ctx *ctx = k->wk_private;

	if (k->wk_keylen != (128/NBBY)) {
		IEEE80211_DPRINTF(ctx->cc_ic, IEEE80211_MSG_CRYPTO,
			"%s: Invalid key length %u, expecting %u\n",
			__func__, k->wk_keylen, 128/NBBY);
		return 0;
	}
	if (k->wk_flags & IEEE80211_KEY_SWCRYPT)
		rijndael_set_key(&ctx->cc_aes, k->wk_key, k->wk_keylen*NBBY);
	return 1;
}

/*
 * Add privacy headers appropriate for the specified key.
 */
static int
ccmp_encap(struct ieee80211_key *k, struct mbuf *m, uint8_t keyid)
{
	struct ccmp_ctx *ctx = k->wk_private;
	struct ieee80211com *ic = ctx->cc_ic;
	uint8_t *ivp;
	int hdrlen;

	hdrlen = ieee80211_hdrspace(ic, mtod(m, void *));

	/*
	 * Copy down 802.11 header and add the IV, KeyID, and ExtIV.
	 */
	M_PREPEND(m, ccmp.ic_header, M_NOWAIT);
	if (m == NULL)
		return 0;
	ivp = mtod(m, uint8_t *);
	ovbcopy(ivp + ccmp.ic_header, ivp, hdrlen);
	ivp += hdrlen;

	k->wk_keytsc++;		/* XXX wrap at 48 bits */
	ivp[0] = k->wk_keytsc >> 0;		/* PN0 */
	ivp[1] = k->wk_keytsc >> 8;		/* PN1 */
	ivp[2] = 0;				/* Reserved */
	ivp[3] = keyid | IEEE80211_WEP_EXTIV;	/* KeyID | ExtID */
	ivp[4] = k->wk_keytsc >> 16;		/* PN2 */
	ivp[5] = k->wk_keytsc >> 24;		/* PN3 */
	ivp[6] = k->wk_keytsc >> 32;		/* PN4 */
	ivp[7] = k->wk_keytsc >> 40;		/* PN5 */

	/*
	 * Finally, do software encrypt if neeed.
	 */
	if ((k->wk_flags & IEEE80211_KEY_SWCRYPT) &&
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
	struct ccmp_ctx *ctx = k->wk_private;
	struct ieee80211_frame *wh;
	uint8_t *ivp;
	uint64_t pn;

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
		IEEE80211_DPRINTF(ctx->cc_ic, IEEE80211_MSG_CRYPTO,
			"[%s] Missing ExtIV for AES-CCM cipher\n",
			ether_sprintf(wh->i_addr2));
		ctx->cc_ic->ic_stats.is_rx_ccmpformat++;
		return 0;
	}
	pn = READ_6(ivp[0], ivp[1], ivp[4], ivp[5], ivp[6], ivp[7]);
	if (pn <= k->wk_keyrsc) {
		/*
		 * Replay violation.
		 */
		ieee80211_notify_replay_failure(ctx->cc_ic, wh, k, pn);
		ctx->cc_ic->ic_stats.is_rx_ccmpreplay++;
		return 0;
	}

	/*
	 * Check if the device handled the decrypt in hardware.
	 * If so we just strip the header; otherwise we need to
	 * handle the decrypt in software.  Note that for the
	 * latter we leave the header in place for use in the
	 * decryption work.
	 */
	if ((k->wk_flags & IEEE80211_KEY_SWCRYPT) &&
	    !ccmp_decrypt(k, pn, m, hdrlen))
		return 0;

	/*
	 * Copy up 802.11 header and strip crypto bits.
	 */
	ovbcopy(mtod(m, void *), mtod(m, uint8_t *) + ccmp.ic_header, hdrlen);
	m_adj(m, ccmp.ic_header);
	m_adj(m, -ccmp.ic_trailer);

	/*
	 * Ok to update rsc now.
	 */
	k->wk_keyrsc = pn;

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
	u_int64_t pn, size_t dlen,
	uint8_t b0[AES_BLOCK_LEN], uint8_t aad[2 * AES_BLOCK_LEN],
	uint8_t auth[AES_BLOCK_LEN], uint8_t s0[AES_BLOCK_LEN])
{
#define	IS_4ADDRESS(wh) \
	((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS)
#define	IS_QOS_DATA(wh)	IEEE80211_QOS_HAS_SEQ(wh)

	/* CCM Initial Block:
	 * Flag (Include authentication header, M=3 (8-octet MIC),
	 *       L=1 (2-octet Dlen))
	 * Nonce: 0x00 | A2 | PN
	 * Dlen */
	b0[0] = 0x59;
	/* NB: b0[1] set below */
	IEEE80211_ADDR_COPY(b0 + 2, wh->i_addr2);
	b0[8] = pn >> 40;
	b0[9] = pn >> 32;
	b0[10] = pn >> 24;
	b0[11] = pn >> 16;
	b0[12] = pn >> 8;
	b0[13] = pn >> 0;
	b0[14] = (dlen >> 8) & 0xff;
	b0[15] = dlen & 0xff;

	/* AAD:
	 * FC with bits 4..6 and 11..13 masked to zero; 14 is always one
	 * A1 | A2 | A3
	 * SC with bits 4..15 (seq#) masked to zero
	 * A4 (if present)
	 * QC (if present)
	 */
	aad[0] = 0;	/* AAD length >> 8 */
	/* NB: aad[1] set below */
	aad[2] = wh->i_fc[0] & 0x8f;	/* XXX magic #s */
	aad[3] = wh->i_fc[1] & 0xc7;	/* XXX magic #s */
	/* NB: we know 3 addresses are contiguous */
	memcpy(aad + 4, wh->i_addr1, 3 * IEEE80211_ADDR_LEN);
	aad[22] = wh->i_seq[0] & IEEE80211_SEQ_FRAG_MASK;
	aad[23] = 0; /* all bits masked */
	/*
	 * Construct variable-length portion of AAD based
	 * on whether this is a 4-address frame/QOS frame.
	 * We always zero-pad to 32 bytes before running it
	 * through the cipher.
	 *
	 * We also fill in the priority bits of the CCM
	 * initial block as we know whether or not we have
	 * a QOS frame.
	 */
	if (IS_4ADDRESS(wh)) {
		IEEE80211_ADDR_COPY(aad + 24,
			((struct ieee80211_frame_addr4 *)wh)->i_addr4);
		if (IS_QOS_DATA(wh)) {
			struct ieee80211_qosframe_addr4 *qwh4 =
				(struct ieee80211_qosframe_addr4 *) wh;
			aad[30] = qwh4->i_qos[0] & 0x0f;/* just priority bits */
			aad[31] = 0;
			b0[1] = aad[30];
			aad[1] = 22 + IEEE80211_ADDR_LEN + 2;
		} else {
			*(uint16_t *)&aad[30] = 0;
			b0[1] = 0;
			aad[1] = 22 + IEEE80211_ADDR_LEN;
		}
	} else {
		if (IS_QOS_DATA(wh)) {
			struct ieee80211_qosframe *qwh =
				(struct ieee80211_qosframe*) wh;
			aad[24] = qwh->i_qos[0] & 0x0f;	/* just priority bits */
			aad[25] = 0;
			b0[1] = aad[24];
			aad[1] = 22 + 2;
		} else {
			*(uint16_t *)&aad[24] = 0;
			b0[1] = 0;
			aad[1] = 22;
		}
		*(uint16_t *)&aad[26] = 0;
		*(uint32_t *)&aad[28] = 0;
	}

	/* Start with the first block and AAD */
	rijndael_encrypt(ctx, b0, auth);
	xor_block(auth, aad, AES_BLOCK_LEN);
	rijndael_encrypt(ctx, auth, auth);
	xor_block(auth, &aad[AES_BLOCK_LEN], AES_BLOCK_LEN);
	rijndael_encrypt(ctx, auth, auth);
	b0[0] &= 0x07;
	b0[14] = b0[15] = 0;
	rijndael_encrypt(ctx, b0, s0);
#undef	IS_QOS_DATA
#undef	IS_4ADDRESS
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

	ctx->cc_ic->ic_stats.is_crypto_ccmp++;

	wh = mtod(m, struct ieee80211_frame *);
	data_len = m->m_pkthdr.len - (hdrlen + ccmp.ic_header);
	ccmp_init_blocks(&ctx->cc_aes, wh, key->wk_keytsc,
		data_len, b0, aad, b, s0);

	i = 1;
	pos = mtod(m, uint8_t *) + hdrlen + ccmp.ic_header;
	/* NB: assumes header is entirely in first mbuf */
	space = m->m_len - (hdrlen + ccmp.ic_header);
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
	xor_block(b, s0, ccmp.ic_trailer);
	return m_append(m0, ccmp.ic_trailer, b);
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
	struct ccmp_ctx *ctx = key->wk_private;
	struct ieee80211_frame *wh;
	uint8_t aad[2 * AES_BLOCK_LEN];
	uint8_t b0[AES_BLOCK_LEN], b[AES_BLOCK_LEN], a[AES_BLOCK_LEN];
	uint8_t mic[AES_BLOCK_LEN];
	size_t data_len;
	int i;
	uint8_t *pos;
	u_int space;

	ctx->cc_ic->ic_stats.is_crypto_ccmp++;

	wh = mtod(m, struct ieee80211_frame *);
	data_len = m->m_pkthdr.len - (hdrlen + ccmp.ic_header + ccmp.ic_trailer);
	ccmp_init_blocks(&ctx->cc_aes, wh, pn, data_len, b0, aad, a, b);
	m_copydata(m, m->m_pkthdr.len - ccmp.ic_trailer, ccmp.ic_trailer, mic);
	xor_block(mic, b, ccmp.ic_trailer);

	i = 1;
	pos = mtod(m, uint8_t *) + hdrlen + ccmp.ic_header;
	space = m->m_len - (hdrlen + ccmp.ic_header);
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
	if (memcmp(mic, a, ccmp.ic_trailer) != 0) {
		IEEE80211_DPRINTF(ctx->cc_ic, IEEE80211_MSG_CRYPTO,
			"[%s] AES-CCM decrypt failed; MIC mismatch\n",
			ether_sprintf(wh->i_addr2));
		ctx->cc_ic->ic_stats.is_rx_ccmpmic++;
		return 0;
	}
	return 1;
}
#undef CCMP_DECRYPT

/*
 * Module glue.
 */
IEEE80211_CRYPTO_MODULE(ccmp, 1);
