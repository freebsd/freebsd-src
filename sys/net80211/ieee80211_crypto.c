/*-
 * Copyright (c) 2001 Atsushi Onoe
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
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
__FBSDID("$FreeBSD$");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/mbuf.h>   
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/bus.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <machine/atomic.h>
 
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_llc.h>

#include <net80211/ieee80211_var.h>

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h> 
#include <netinet/if_ether.h>
#endif

#include <crypto/rc4/rc4.h>
#define	arc4_ctxlen()			sizeof (struct rc4_state)
#define	arc4_setkey(_c,_k,_l)		rc4_init(_c,_k,_l)
#define	arc4_encrypt(_c,_d,_s,_l)	rc4_crypt(_c,_s,_d,_l)

static	void ieee80211_crc_init(void);
static	u_int32_t ieee80211_crc_update(u_int32_t crc, u_int8_t *buf, int len);

void
ieee80211_crypto_attach(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;

	/*
	 * Setup crypto support.
	 */
	ieee80211_crc_init();
	ic->ic_iv = arc4random();
}

void
ieee80211_crypto_detach(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;

	if (ic->ic_wep_ctx != NULL) {
		free(ic->ic_wep_ctx, M_DEVBUF);
		ic->ic_wep_ctx = NULL;
	}
}

struct mbuf *
ieee80211_wep_crypt(struct ifnet *ifp, struct mbuf *m0, int txflag)
{
	struct ieee80211com *ic = (void *)ifp;
	struct mbuf *m, *n, *n0;
	struct ieee80211_frame *wh;
	int i, left, len, moff, noff, kid;
	u_int32_t iv, crc;
	u_int8_t *ivp;
	void *ctx;
	u_int8_t keybuf[IEEE80211_WEP_IVLEN + IEEE80211_KEYBUF_SIZE];
	u_int8_t crcbuf[IEEE80211_WEP_CRCLEN];

	n0 = NULL;
	if ((ctx = ic->ic_wep_ctx) == NULL) {
		ctx = malloc(arc4_ctxlen(), M_DEVBUF, M_NOWAIT);
		if (ctx == NULL)
			goto fail;
		ic->ic_wep_ctx = ctx;
	}
	m = m0;
	left = m->m_pkthdr.len;
	MGET(n, M_DONTWAIT, m->m_type);
	n0 = n;
	if (n == NULL)
		goto fail;
	M_MOVE_PKTHDR(n, m);
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
	wh = mtod(n, struct ieee80211_frame *);
	left -= len;
	moff = len;
	noff = len;
	if (txflag) {
		kid = ic->ic_wep_txkey;
		wh->i_fc[1] |= IEEE80211_FC1_WEP;
                iv = ic->ic_iv;
		/*
		 * Skip 'bad' IVs from Fluhrer/Mantin/Shamir:
		 * (B, 255, N) with 3 <= B < 8
		 */
		if (iv >= 0x03ff00 &&
		    (iv & 0xf8ff00) == 0x00ff00)
			iv += 0x000100;
		ic->ic_iv = iv + 1;
		/* put iv in little endian to prepare 802.11i */
		ivp = mtod(n, u_int8_t *) + noff;
		for (i = 0; i < IEEE80211_WEP_IVLEN; i++) {
			ivp[i] = iv & 0xff;
			iv >>= 8;
		}
		ivp[IEEE80211_WEP_IVLEN] = kid << 6;	/* pad and keyid */
		noff += IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN;
	} else {
		wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
		ivp = mtod(m, u_int8_t *) + moff;
		kid = ivp[IEEE80211_WEP_IVLEN] >> 6;
		moff += IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN;
	}
	memcpy(keybuf, ivp, IEEE80211_WEP_IVLEN);
	memcpy(keybuf + IEEE80211_WEP_IVLEN, ic->ic_nw_keys[kid].wk_key,
	    ic->ic_nw_keys[kid].wk_len);
	arc4_setkey(ctx, keybuf,
	    IEEE80211_WEP_IVLEN + ic->ic_nw_keys[kid].wk_len);

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
		arc4_encrypt(ctx, mtod(n, caddr_t) + noff,
		    mtod(m, caddr_t) + moff, len);
		if (txflag)
			crc = ieee80211_crc_update(crc,
			    mtod(m, u_int8_t *) + moff, len);
		else
			crc = ieee80211_crc_update(crc,
			    mtod(n, u_int8_t *) + noff, len);
		left -= len;
		moff += len;
		noff += len;
	}
	crc = ~crc;
	if (txflag) {
		*(u_int32_t *)crcbuf = htole32(crc);
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
		arc4_encrypt(ctx, mtod(n, caddr_t) + noff, crcbuf,
		    sizeof(crcbuf));
	} else {
		n->m_len = noff;
		for (noff = 0; noff < sizeof(crcbuf); noff += len) {
			len = sizeof(crcbuf) - noff;
			if (len > m->m_len - moff)
				len = m->m_len - moff;
			if (len > 0)
				arc4_encrypt(ctx, crcbuf + noff,
				    mtod(m, caddr_t) + moff, len);
			m = m->m_next;
			moff = 0;
		}
		if (crc != le32toh(*(u_int32_t *)crcbuf)) {
#ifdef IEEE80211_DEBUG
			if (ieee80211_debug) {
				if_printf(ifp, "decrypt CRC error\n");
				if (ieee80211_debug > 1)
					ieee80211_dump_pkt(n0->m_data,
					    n0->m_len, -1, -1);
			}
#endif
			goto fail;
		}
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
static u_int32_t ieee80211_crc_table[256];

/* Make the table for a fast CRC. */
static void
ieee80211_crc_init(void)
{
	u_int32_t c;
	int n, k;

	for (n = 0; n < 256; n++) {
		c = (u_int32_t)n;
		for (k = 0; k < 8; k++) {
			if (c & 1)
				c = 0xedb88320UL ^ (c >> 1);
			else
				c = c >> 1;
		}
		ieee80211_crc_table[n] = c;
	}
}

/*
 * Update a running CRC with the bytes buf[0..len-1]--the CRC
 * should be initialized to all 1's, and the transmitted value
 * is the 1's complement of the final running CRC
 */

static u_int32_t
ieee80211_crc_update(u_int32_t crc, u_int8_t *buf, int len)
{
	u_int8_t *endbuf;

	for (endbuf = buf + len; buf < endbuf; buf++)
		crc = ieee80211_crc_table[(crc ^ *buf) & 0xff] ^ (crc >> 8);
	return crc;
}
