/*	$KAME: esp_aesctr.c,v 1.2 2003/07/20 00:29:37 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, 1998 and 2003 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/syslog.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>

#include <netinet6/ipsec.h>
#include <netinet6/esp.h>
#include <netinet6/esp_aesctr.h>

#include <netkey/key.h>

#include <crypto/rijndael/rijndael.h>

#include <net/net_osdep.h>

#define AES_BLOCKSIZE	16

#define NONCESIZE	4
union cblock {
	struct {
		u_int8_t nonce[4];
		u_int8_t iv[16];
		u_int32_t ctr;
	} v __attribute__((__packed__));
	u_int8_t cblock[16];
};

typedef struct {
	u_int32_t	r_ek[(RIJNDAEL_MAXNR+1)*4];
	int		r_nr; /* key-length-dependent number of rounds */
} aesctr_ctx;

int
esp_aesctr_mature(sav)
	struct secasvar *sav;
{
	int keylen;
	const struct esp_algorithm *algo;

	algo = esp_algorithm_lookup(sav->alg_enc);
	if (!algo) {
		ipseclog((LOG_ERR,
		    "esp_aeesctr_mature %s: unsupported algorithm.\n",
		    algo->name));
		return 1;
	}

	keylen = sav->key_enc->sadb_key_bits;
	if (keylen < algo->keymin || algo->keymax < keylen) {
		ipseclog((LOG_ERR,
		    "esp_aesctr_mature %s: invalid key length %d.\n",
		    algo->name, sav->key_enc->sadb_key_bits));
		return 1;
	}

	/* rijndael key + nonce */
	if (!(keylen == 128 + 32 || keylen == 192 + 32 || keylen == 256 + 32)) {
		ipseclog((LOG_ERR,
		    "esp_aesctr_mature %s: invalid key length %d.\n",
		    algo->name, keylen));
		return 1;
	}

	return 0;
}

size_t
esp_aesctr_schedlen(algo)
	const struct esp_algorithm *algo;
{

	return sizeof(aesctr_ctx);
}

int
esp_aesctr_schedule(algo, sav)
	const struct esp_algorithm *algo;
	struct secasvar *sav;
{
	aesctr_ctx *ctx;
	int keylen;

	/* SA key = AES key + nonce */
	keylen = _KEYLEN(sav->key_enc) * 8 - NONCESIZE * 8;

	ctx = (aesctr_ctx *)sav->sched;
	if ((ctx->r_nr = rijndaelKeySetupEnc(ctx->r_ek,
	    (char *)_KEYBUF(sav->key_enc), keylen)) == 0)
		return -1;
	return 0;
}

int
esp_aesctr_decrypt(m, off, sav, algo, ivlen)
	struct mbuf *m;
	size_t off;
	struct secasvar *sav;
	const struct esp_algorithm *algo;
	int ivlen;
{
	struct mbuf *s;
	struct mbuf *d, *d0 = NULL, *dp;
	int soff, doff;	/* offset from the head of chain, to head of this mbuf */
	int sn, dn;	/* offset from the head of the mbuf, to meat */
	size_t ivoff, bodyoff;
	union cblock cblock;
	u_int8_t keystream[AES_BLOCKSIZE], *nonce;
	u_int32_t ctr;
	u_int8_t *ivp;
	u_int8_t sbuf[AES_BLOCKSIZE], *sp, *dst;
	struct mbuf *scut;
	int scutoff;
	int i;
	int blocklen;
	aesctr_ctx *ctx;

	if (ivlen != sav->ivlen) {
		ipseclog((LOG_ERR, "esp_aesctr_decrypt %s: "
		    "unsupported ivlen %d\n", algo->name, ivlen));
		goto fail;
	}

	/* assumes blocklen == padbound */
	blocklen = algo->padbound;

	ivoff = off + sizeof(struct newesp);
	bodyoff = off + sizeof(struct newesp) + ivlen;

	/* setup counter block */
	nonce = _KEYBUF(sav->key_enc) + _KEYLEN(sav->key_enc) - NONCESIZE;
	bcopy(nonce, cblock.v.nonce, NONCESIZE);
	m_copydata(m, ivoff, ivlen, cblock.v.iv);
	ctr = 1;

	if (m->m_pkthdr.len < bodyoff) {
		ipseclog((LOG_ERR, "esp_aesctr_decrypt %s: bad len %d/%lu\n",
		    algo->name, m->m_pkthdr.len, (unsigned long)bodyoff));
		goto fail;
	}
	if ((m->m_pkthdr.len - bodyoff) % blocklen) {
		ipseclog((LOG_ERR, "esp_aesctr_decrypt %s: "
		    "payload length must be multiple of %d\n",
		    algo->name, blocklen));
		goto fail;
	}

	s = m;
	d = d0 = dp = NULL;
	soff = doff = sn = dn = 0;
	ivp = sp = NULL;

	/* skip bodyoff */
	while (soff < bodyoff) {
		if (soff + s->m_len > bodyoff) {
			sn = bodyoff - soff;
			break;
		}

		soff += s->m_len;
		s = s->m_next;
	}
	scut = s;
	scutoff = sn;

	/* skip over empty mbuf */
	while (s && s->m_len == 0)
		s = s->m_next;

	while (soff < m->m_pkthdr.len) {
		/* source */
		if (sn + blocklen <= s->m_len) {
			/* body is continuous */
			sp = mtod(s, u_int8_t *) + sn;
		} else {
			/* body is non-continuous */
			m_copydata(s, sn, blocklen, (caddr_t)sbuf);
			sp = sbuf;
		}

		/* destination */
		if (!d || dn + blocklen > d->m_len) {
			if (d)
				dp = d;
			MGET(d, M_DONTWAIT, MT_DATA);
			i = m->m_pkthdr.len - (soff + sn);
			if (d && i > MLEN) {
				MCLGET(d, M_DONTWAIT);
				if ((d->m_flags & M_EXT) == 0) {
					m_free(d);
					d = NULL;
				}
			}
			if (!d) {
				goto nomem;
			}
			if (!d0)
				d0 = d;
			if (dp)
				dp->m_next = d;
			d->m_len = 0;
			d->m_len = (M_TRAILINGSPACE(d) / blocklen) * blocklen;
			if (d->m_len > i)
				d->m_len = i;
			dn = 0;
		}

		/* put counter into counter block */
		cblock.v.ctr = htonl(ctr);

		/* setup keystream */
		ctx = (aesctr_ctx *)sav->sched;
		rijndaelEncrypt(ctx->r_ek, ctx->r_nr, cblock.cblock, keystream);

		bcopy(sp, mtod(d, u_int8_t *) + dn, blocklen);
		dst = mtod(d, u_int8_t *) + dn;
		for (i = 0; i < blocklen; i++)
			dst[i] ^= keystream[i];

		ctr++;

		sn += blocklen;
		dn += blocklen;

		/* find the next source block */
		while (s && sn >= s->m_len) {
			sn -= s->m_len;
			soff += s->m_len;
			s = s->m_next;
		}

		/* skip over empty mbuf */
		while (s && s->m_len == 0)
			s = s->m_next;
	}

	m_freem(scut->m_next);
	scut->m_len = scutoff;
	scut->m_next = d0;

	/* just in case */
	bzero(&cblock, sizeof(cblock));
	bzero(keystream, sizeof(keystream));

	return 0;

fail:
	m_freem(m);
	if (d0)
		m_freem(d0);
	return EINVAL;

nomem:
	m_freem(m);
	if (d0)
		m_freem(d0);
	return ENOBUFS;
}

int
esp_aesctr_encrypt(m, off, plen, sav, algo, ivlen)
	struct mbuf *m;
	size_t off;
	size_t plen;
	struct secasvar *sav;
	const struct esp_algorithm *algo;
	int ivlen;
{
	struct mbuf *s;
	struct mbuf *d, *d0, *dp;
	int soff, doff;	/* offset from the head of chain, to head of this mbuf */
	int sn, dn;	/* offset from the head of the mbuf, to meat */
	size_t ivoff, bodyoff;
	union cblock cblock;
	u_int8_t keystream[AES_BLOCKSIZE], *nonce;
	u_int32_t ctr;
	u_int8_t sbuf[AES_BLOCKSIZE], *sp, *dst;
	struct mbuf *scut;
	int scutoff;
	int i;
	int blocklen;
	aesctr_ctx *ctx;

	if (ivlen != sav->ivlen) {
		ipseclog((LOG_ERR, "esp_aesctr_encrypt %s: "
		    "unsupported ivlen %d\n", algo->name, ivlen));
		m_freem(m);
		return EINVAL;
	}

	/* assumes blocklen == padbound */
	blocklen = algo->padbound;

	ivoff = off + sizeof(struct newesp);
	bodyoff = off + sizeof(struct newesp) + ivlen;

	/* put iv into the packet. */
	/* maybe it is better to overwrite dest, not source */
	m_copyback(m, ivoff, ivlen, sav->iv);

	/* setup counter block */
	nonce = _KEYBUF(sav->key_enc) + _KEYLEN(sav->key_enc) - NONCESIZE;
	bcopy(nonce, cblock.v.nonce, NONCESIZE);
	m_copydata(m, ivoff, ivlen, cblock.v.iv);
	ctr = 1;

	if (m->m_pkthdr.len < bodyoff) {
		ipseclog((LOG_ERR, "esp_aesctr_encrypt %s: bad len %d/%lu\n",
		    algo->name, m->m_pkthdr.len, (unsigned long)bodyoff));
		m_freem(m);
		return EINVAL;
	}
	if ((m->m_pkthdr.len - bodyoff) % blocklen) {
		ipseclog((LOG_ERR, "esp_aesctr_encrypt %s: "
		    "payload length must be multiple of %lu\n",
		    algo->name, (unsigned long)algo->padbound));
		m_freem(m);
		return EINVAL;
	}

	s = m;
	d = d0 = dp = NULL;
	soff = doff = sn = dn = 0;
	sp = NULL;

	/* skip bodyoff */
	while (soff < bodyoff) {
		if (soff + s->m_len > bodyoff) {
			sn = bodyoff - soff;
			break;
		}

		soff += s->m_len;
		s = s->m_next;
	}
	scut = s;
	scutoff = sn;

	/* skip over empty mbuf */
	while (s && s->m_len == 0)
		s = s->m_next;

	while (soff < m->m_pkthdr.len) {
		/* source */
		if (sn + blocklen <= s->m_len) {
			/* body is continuous */
			sp = mtod(s, u_int8_t *) + sn;
		} else {
			/* body is non-continuous */
			m_copydata(s, sn, blocklen, (caddr_t)sbuf);
			sp = sbuf;
		}

		/* destination */
		if (!d || dn + blocklen > d->m_len) {
			if (d)
				dp = d;
			MGET(d, M_DONTWAIT, MT_DATA);
			i = m->m_pkthdr.len - (soff + sn);
			if (d && i > MLEN) {
				MCLGET(d, M_DONTWAIT);
				if ((d->m_flags & M_EXT) == 0) {
					m_free(d);
					d = NULL;
				}
			}
			if (!d) {
				m_freem(m);
				if (d0)
					m_freem(d0);
				return ENOBUFS;
			}
			if (!d0)
				d0 = d;
			if (dp)
				dp->m_next = d;
			d->m_len = 0;
			d->m_len = (M_TRAILINGSPACE(d) / blocklen) * blocklen;
			if (d->m_len > i)
				d->m_len = i;
			dn = 0;
		}

		/* put counter into counter block */
		cblock.v.ctr = htonl(ctr);

		/* setup keystream */
		ctx = (aesctr_ctx *)sav->sched;
		rijndaelEncrypt(ctx->r_ek, ctx->r_nr, cblock.cblock, keystream);

		bcopy(sp, mtod(d, u_int8_t *) + dn, blocklen);
		dst = mtod(d, u_int8_t *) + dn;
		for (i = 0; i < blocklen; i++)
			dst[i] ^= keystream[i];

		ctr++;

		sn += blocklen;
		dn += blocklen;

		/* find the next source block */
		while (s && sn >= s->m_len) {
			sn -= s->m_len;
			soff += s->m_len;
			s = s->m_next;
		}

		/* skip over empty mbuf */
		while (s && s->m_len == 0)
			s = s->m_next;
	}

	m_freem(scut->m_next);
	scut->m_len = scutoff;
	scut->m_next = d0;

	/* just in case */
	bzero(&cblock, sizeof(cblock));
	bzero(keystream, sizeof(keystream));

	key_sa_stir_iv(sav);

	return 0;
}
