/*	$NetBSD: awi_wep.c,v 1.4 2000/08/14 11:28:03 onoe Exp $	*/
/* $FreeBSD$ */

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Atsushi Onoe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * WEP support framework for the awi driver.
 *
 * No actual encryption capability is provided here, but any can be added
 * to awi_wep_algo table below.
 *
 * Note that IEEE802.11 specification states WEP uses RC4 with 40bit key,
 * which is a proprietary encryption algorithm available under license
 * from RSA Data Security Inc.  Using another algorithm, includes null
 * encryption provided here, the awi driver cannot be able to communicate
 * with other stations.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/sockio.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 4
#include <sys/bus.h>
#else
#include <sys/device.h>
#endif

#include <net/if.h>
#include <net/if_dl.h>
#ifdef __FreeBSD__
#include <net/ethernet.h>
#include <net/if_arp.h>
#else
#include <net/if_ether.h>
#endif
#include <net/if_media.h>
#include <net/if_ieee80211.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#ifdef __FreeBSD__
#include <machine/clock.h>
#endif

#ifdef __NetBSD__
#include <dev/ic/am79c930reg.h>
#include <dev/ic/am79c930var.h>
#include <dev/ic/awireg.h>
#include <dev/ic/awivar.h>

#include <crypto/arc4/arc4.h>
#endif

#ifdef __FreeBSD__
#include <dev/awi/am79c930reg.h>
#include <dev/awi/am79c930var.h>
#include <dev/awi/awireg.h>
#include <dev/awi/awivar.h>

#include <crypto/rc4/rc4.h>
static __inline int
arc4_ctxlen(void)
{
        return sizeof(struct rc4_state);
}

static __inline void
arc4_setkey(void *ctx, u_int8_t *key, int keylen)
{
	rc4_init(ctx, key, keylen);
}

static __inline void
arc4_encrypt(void *ctx, u_int8_t *dst, u_int8_t *src, int len)
{
	rc4_crypt(ctx, src, dst, len);
}
#endif

static void awi_crc_init __P((void));
static u_int32_t awi_crc_update __P((u_int32_t crc, u_int8_t *buf, int len));

static int awi_null_ctxlen __P((void));
static void awi_null_setkey __P((void *ctx, u_int8_t *key, int keylen));
static void awi_null_copy __P((void *ctx, u_int8_t *dst, u_int8_t *src, int len));

/* XXX: the order should be known to wiconfig/user */

static struct awi_wep_algo awi_wep_algo[] = {
/* 0: no wep */
	{ "no" },	/* dummy for no wep */

/* 1: normal wep (arc4) */
	{ "arc4", arc4_ctxlen, arc4_setkey,
	    arc4_encrypt, arc4_encrypt },

/* 2: debug wep (null) */
	{ "null", awi_null_ctxlen, awi_null_setkey,
	    awi_null_copy, awi_null_copy },
			/* dummy for wep without encryption */
};

int
awi_wep_setnwkey(sc, nwkey)
	struct awi_softc *sc;
	struct ieee80211_nwkey *nwkey;
{
	int i, len, error;
	u_int8_t keybuf[AWI_MAX_KEYLEN];

	if (nwkey->i_defkid <= 0 ||
	    nwkey->i_defkid > IEEE80211_WEP_NKID)
		return EINVAL;
	error = 0;
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		if (nwkey->i_key[i].i_keydat == NULL)
			continue;
		len = nwkey->i_key[i].i_keylen;
		if (len > sizeof(keybuf)) {
			error = EINVAL;
			break;
		}
		error = copyin(nwkey->i_key[i].i_keydat, keybuf, len);
		if (error)
			break;
		error = awi_wep_setkey(sc, i, keybuf, len);
		if (error)
			break;
	}
	if (error == 0) {
		sc->sc_wep_defkid = nwkey->i_defkid - 1;
		error = awi_wep_setalgo(sc, nwkey->i_wepon);
		if (error == 0 && sc->sc_enabled) {
			awi_stop(sc);
			error = awi_init(sc);
		}
	}
	return error;
}

int
awi_wep_getnwkey(sc, nwkey)
	struct awi_softc *sc;
	struct ieee80211_nwkey *nwkey;
{
	int i, len, error, suerr;
	u_int8_t keybuf[AWI_MAX_KEYLEN];

	nwkey->i_wepon = awi_wep_getalgo(sc);
	nwkey->i_defkid = sc->sc_wep_defkid + 1;
	/* do not show any keys to non-root user */
#ifdef __FreeBSD__
	suerr = suser(curproc);
#else
	suerr = suser(curproc->p_ucred, &curproc->p_acflag);
#endif
	error = 0;
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		if (nwkey->i_key[i].i_keydat == NULL)
			continue;
		if (suerr) {
			error = suerr;
			break;
		}
		len = sizeof(keybuf);
		error = awi_wep_getkey(sc, i, keybuf, &len);
		if (error)
			break;
		if (nwkey->i_key[i].i_keylen < len) {
			error = ENOSPC;
			break;
		}
		nwkey->i_key[i].i_keylen = len;
		error = copyout(keybuf, nwkey->i_key[i].i_keydat, len);
		if (error)
			break;
	}
	return error;
}

int
awi_wep_getalgo(sc)
	struct awi_softc *sc;
{

	if (sc->sc_wep_algo == NULL)
		return 0;
	return sc->sc_wep_algo - awi_wep_algo;
}

int
awi_wep_setalgo(sc, algo)
	struct awi_softc *sc;
	int algo;
{
	struct awi_wep_algo *awa;
	int ctxlen;

	awi_crc_init();	/* XXX: not belongs here */
	if (algo < 0 || algo > sizeof(awi_wep_algo)/sizeof(awi_wep_algo[0]))
		return EINVAL;
	awa = &awi_wep_algo[algo];
	if (awa->awa_name == NULL)
		return EINVAL;
	if (awa->awa_ctxlen == NULL) {
		awa = NULL;
		ctxlen = 0;
	} else
		ctxlen = awa->awa_ctxlen();
	if (sc->sc_wep_ctx != NULL) {
		free(sc->sc_wep_ctx, M_DEVBUF);
		sc->sc_wep_ctx = NULL;
	}
	if (ctxlen) {
		sc->sc_wep_ctx = malloc(ctxlen, M_DEVBUF, M_NOWAIT);
		if (sc->sc_wep_ctx == NULL)
			return ENOMEM;
	}
	sc->sc_wep_algo = awa;
	return 0;
}

int
awi_wep_setkey(sc, kid, key, keylen)
	struct awi_softc *sc;
	int kid;
	unsigned char *key;
	int keylen;
{

	if (kid < 0 || kid >= IEEE80211_WEP_NKID)
		return EINVAL;
	if (keylen < 0 || keylen + IEEE80211_WEP_IVLEN > AWI_MAX_KEYLEN)
		return EINVAL;
	sc->sc_wep_keylen[kid] = keylen;
	if (keylen > 0)
		memcpy(sc->sc_wep_key[kid] + IEEE80211_WEP_IVLEN, key, keylen);
	return 0;
}

int
awi_wep_getkey(sc, kid, key, keylen)
	struct awi_softc *sc;
	int kid;
	unsigned char *key;
	int *keylen;
{

	if (kid < 0 || kid >= IEEE80211_WEP_NKID)
		return EINVAL;
	if (*keylen < sc->sc_wep_keylen[kid])
		return ENOSPC;
	*keylen = sc->sc_wep_keylen[kid];
	if (*keylen > 0)
		memcpy(key, sc->sc_wep_key[kid] + IEEE80211_WEP_IVLEN, *keylen);
	return 0;
}

struct mbuf *
awi_wep_encrypt(sc, m0, txflag)
	struct awi_softc *sc;
	struct mbuf *m0;
	int txflag;
{
	struct mbuf *m, *n, *n0;
	struct ieee80211_frame *wh;
	struct awi_wep_algo *awa;
	int left, len, moff, noff, keylen, kid;
	u_int32_t iv, crc;
	u_int8_t *key, *ivp;
	void *ctx;
	u_int8_t crcbuf[IEEE80211_WEP_CRCLEN];

	n0 = NULL;
	awa = sc->sc_wep_algo;
	if (awa == NULL)
		goto fail;
	ctx = sc->sc_wep_ctx;
	m = m0;
	left = m->m_pkthdr.len;
	MGET(n, M_DONTWAIT, m->m_type);
	n0 = n;
	if (n == NULL)
		goto fail;
	M_COPY_PKTHDR(n, m);
	len = IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_CRCLEN;
	if (txflag) {
		n->m_pkthdr.len += len;
	} else {
		n->m_pkthdr.len -= len;
		left -= len;
	}
	n->m_len = MHLEN;
	if (n->m_pkthdr.len >= MINCLSIZE) {
		MCLGET(n, M_DONTWAIT);
		if (n->m_flags & M_EXT)
			n->m_len = n->m_ext.ext_size;
	}
	len = sizeof(struct ieee80211_frame);
	memcpy(mtod(n, caddr_t), mtod(m, caddr_t), len);
	left -= len;
	moff = len;
	noff = len;
	if (txflag) {
		kid = sc->sc_wep_defkid;
		wh = mtod(n, struct ieee80211_frame *);
		wh->i_fc[1] |= IEEE80211_FC1_WEP;
		iv = random();
		/*
		 * store IV, byte order is not the matter since it's random.
		 * assuming IEEE80211_WEP_IVLEN is 3
		 */
		ivp = mtod(n, u_int8_t *) + noff;
		ivp[0] = (iv >> 16) & 0xff;
		ivp[1] = (iv >> 8) & 0xff;
		ivp[2] = iv & 0xff;
		ivp[3] = kid & 0x03;	/* clear pad and keyid */
		noff += IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN;
	} else {
		ivp = mtod(m, u_int8_t *) + moff;
		moff += IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN;
		kid = ivp[IEEE80211_WEP_IVLEN] & 0x03;
	}
	key = sc->sc_wep_key[kid];
	keylen = sc->sc_wep_keylen[kid];
	/* assuming IEEE80211_WEP_IVLEN is 3 */
	key[0] = ivp[0];
	key[1] = ivp[1];
	key[2] = ivp[2];
	awa->awa_setkey(ctx, key, IEEE80211_WEP_IVLEN + keylen);

	/* encrypt with calculating CRC */
	crc = ~0;
	while (left > 0) {
		len = m->m_len - moff;
		if (len == 0) {
			m = m->m_next;
			moff = 0;
			continue;
		}
		if (len > n->m_len - noff) {
			len = n->m_len - noff;
			if (len == 0) {
				MGET(n->m_next, M_DONTWAIT, n->m_type);
				if (n->m_next == NULL)
					goto fail;
				n = n->m_next;
				n->m_len = MLEN;
				if (left >= MINCLSIZE) {
					MCLGET(n, M_DONTWAIT);
					if (n->m_flags & M_EXT)
						n->m_len = n->m_ext.ext_size;
				}
				noff = 0;
				continue;
			}
		}
		if (len > left)
			len = left;
		if (txflag) {
			awa->awa_encrypt(ctx, mtod(n, caddr_t) + noff,
			    mtod(m, caddr_t) + moff, len);
			crc = awi_crc_update(crc, mtod(m, caddr_t) + moff, len);
		} else {
			awa->awa_decrypt(ctx, mtod(n, caddr_t) + noff,
			    mtod(m, caddr_t) + moff, len);
			crc = awi_crc_update(crc, mtod(n, caddr_t) + noff, len);
		}
		left -= len;
		moff += len;
		noff += len;
	}
	crc = ~crc;
	if (txflag) {
		LE_WRITE_4(crcbuf, crc);
		if (n->m_len >= noff + sizeof(crcbuf))
			n->m_len = noff + sizeof(crcbuf);
		else {
			n->m_len = noff;
			MGET(n->m_next, M_DONTWAIT, n->m_type);
			if (n->m_next == NULL)
				goto fail;
			n = n->m_next;
			n->m_len = sizeof(crcbuf);
			noff = 0;
		}
		awa->awa_encrypt(ctx, mtod(n, caddr_t) + noff, crcbuf,
		    sizeof(crcbuf));
	} else {
		n->m_len = noff;
		for (noff = 0; noff < sizeof(crcbuf); noff += len) {
			len = sizeof(crcbuf) - noff;
			if (len > m->m_len - moff)
				len = m->m_len - moff;
			if (len > 0)
				awa->awa_decrypt(ctx, crcbuf + noff,
				    mtod(m, caddr_t) + moff, len);
			m = m->m_next;
			moff = 0;
		}
		if (crc != LE_READ_4(crcbuf))
			goto fail;
	}
	m_freem(m0);
	return n0;

  fail:
	m_freem(m0);
	m_freem(n0);
	return NULL;
}

/*
 * CRC 32 -- routine from RFC 2083
 */

/* Table of CRCs of all 8-bit messages */
static u_int32_t awi_crc_table[256];
static int awi_crc_table_computed = 0;

/* Make the table for a fast CRC. */
static void
awi_crc_init()
{
	u_int32_t c;
	int n, k;

	if (awi_crc_table_computed)
		return;
	for (n = 0; n < 256; n++) {
		c = (u_int32_t)n;
		for (k = 0; k < 8; k++) {
			if (c & 1)
				c = 0xedb88320UL ^ (c >> 1);
			else
				c = c >> 1;
		}
		awi_crc_table[n] = c;
	}
	awi_crc_table_computed = 1;
}

/*
 * Update a running CRC with the bytes buf[0..len-1]--the CRC
 * should be initialized to all 1's, and the transmitted value
 * is the 1's complement of the final running CRC
 */

static u_int32_t
awi_crc_update(crc, buf, len)
	u_int32_t crc;
	u_int8_t *buf;
	int len;
{
	u_int8_t *endbuf;

	for (endbuf = buf + len; buf < endbuf; buf++)
		crc = awi_crc_table[(crc ^ *buf) & 0xff] ^ (crc >> 8);
	return crc;
}

/*
 * Null -- do nothing but copy.
 */

static int
awi_null_ctxlen()
{

	return 0;
}

static void
awi_null_setkey(ctx, key, keylen)
	void *ctx;
	u_char *key;
	int keylen;
{
}

static void
awi_null_copy(ctx, dst, src, len)
	void *ctx;
	u_char *dst;
	u_char *src;
	int len;
{

	memcpy(dst, src, len);
}
