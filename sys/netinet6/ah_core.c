/*	$FreeBSD$	*/
/*	$KAME: ah_core.c,v 1.59 2003/07/25 10:17:14 itojun Exp $	*/

/*-
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 */

/*
 * RFC1826/2402 authentication header.
 */

/* TODO: have shared routines  for hmac-* algorithms */

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_var.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#endif

#include <netinet6/ipsec.h>
#ifdef INET6
#include <netinet6/ipsec6.h>
#endif
#include <netinet6/ah.h>
#ifdef INET6
#include <netinet6/ah6.h>
#endif
#include <netinet6/ah_aesxcbcmac.h>
#ifdef IPSEC_ESP
#include <netinet6/esp.h>
#ifdef INET6
#include <netinet6/esp6.h>
#endif
#endif
#include <net/pfkeyv2.h>
#include <netkey/keydb.h>
#include <sys/md5.h>
#define MD5_RESULTLEN	16
#include <crypto/sha1.h>
#include <crypto/sha2/sha2.h>
#include <opencrypto/rmd160.h>
#define RIPEMD160_RESULTLEN	20

#include <net/net_osdep.h>

static int ah_sumsiz_1216 __P((struct secasvar *));
static int ah_sumsiz_zero __P((struct secasvar *));
static int ah_common_mature __P((struct secasvar *));
static int ah_none_mature __P((struct secasvar *));
static int ah_none_init __P((struct ah_algorithm_state *, struct secasvar *));
static void ah_none_loop __P((struct ah_algorithm_state *, u_int8_t *, size_t));
static void ah_none_result __P((struct ah_algorithm_state *,
	u_int8_t *, size_t));
static int ah_keyed_md5_mature __P((struct secasvar *));
static int ah_keyed_md5_init __P((struct ah_algorithm_state *,
	struct secasvar *));
static void ah_keyed_md5_loop __P((struct ah_algorithm_state *, u_int8_t *,
	size_t));
static void ah_keyed_md5_result __P((struct ah_algorithm_state *,
	u_int8_t *, size_t));
static int ah_keyed_sha1_init __P((struct ah_algorithm_state *,
	struct secasvar *));
static void ah_keyed_sha1_loop __P((struct ah_algorithm_state *, u_int8_t *,
	size_t));
static void ah_keyed_sha1_result __P((struct ah_algorithm_state *, u_int8_t *,
	size_t));
static int ah_hmac_md5_init __P((struct ah_algorithm_state *,
	struct secasvar *));
static void ah_hmac_md5_loop __P((struct ah_algorithm_state *, u_int8_t *,
	size_t));
static void ah_hmac_md5_result __P((struct ah_algorithm_state *,
	u_int8_t *, size_t));
static int ah_hmac_sha1_init __P((struct ah_algorithm_state *,
	struct secasvar *));
static void ah_hmac_sha1_loop __P((struct ah_algorithm_state *, u_int8_t *,
	size_t));
static void ah_hmac_sha1_result __P((struct ah_algorithm_state *,
	u_int8_t *, size_t));
static int ah_hmac_sha2_256_init __P((struct ah_algorithm_state *,
	struct secasvar *));
static void ah_hmac_sha2_256_loop __P((struct ah_algorithm_state *, u_int8_t *,
	size_t));
static void ah_hmac_sha2_256_result __P((struct ah_algorithm_state *,
	u_int8_t *, size_t));
static int ah_hmac_sha2_384_init __P((struct ah_algorithm_state *,
	struct secasvar *));
static void ah_hmac_sha2_384_loop __P((struct ah_algorithm_state *, u_int8_t *,
	size_t));
static void ah_hmac_sha2_384_result __P((struct ah_algorithm_state *,
	u_int8_t *, size_t));
static int ah_hmac_sha2_512_init __P((struct ah_algorithm_state *,
	struct secasvar *));
static void ah_hmac_sha2_512_loop __P((struct ah_algorithm_state *, u_int8_t *,
	size_t));
static void ah_hmac_sha2_512_result __P((struct ah_algorithm_state *,
	u_int8_t *, size_t));
static int ah_hmac_ripemd160_init __P((struct ah_algorithm_state *,
	struct secasvar *));
static void ah_hmac_ripemd160_loop __P((struct ah_algorithm_state *, u_int8_t *,
	size_t));
static void ah_hmac_ripemd160_result __P((struct ah_algorithm_state *,
	u_int8_t *, size_t));

static void ah_update_mbuf __P((struct mbuf *, int, int,
	const struct ah_algorithm *, struct ah_algorithm_state *));

/* checksum algorithms */
static const struct ah_algorithm ah_algorithms[] = {
	{ ah_sumsiz_1216, ah_common_mature, 128, 128, "hmac-md5",
		ah_hmac_md5_init, ah_hmac_md5_loop,
		ah_hmac_md5_result, },
	{ ah_sumsiz_1216, ah_common_mature, 160, 160, "hmac-sha1",
		ah_hmac_sha1_init, ah_hmac_sha1_loop,
		ah_hmac_sha1_result, },
	{ ah_sumsiz_1216, ah_keyed_md5_mature, 128, 128, "keyed-md5",
		ah_keyed_md5_init, ah_keyed_md5_loop,
		ah_keyed_md5_result, },
	{ ah_sumsiz_1216, ah_common_mature, 160, 160, "keyed-sha1",
		ah_keyed_sha1_init, ah_keyed_sha1_loop,
		ah_keyed_sha1_result, },
	{ ah_sumsiz_zero, ah_none_mature, 0, 2048, "none",
		ah_none_init, ah_none_loop, ah_none_result, },
	{ ah_sumsiz_1216, ah_common_mature, 256, 256,
		"hmac-sha2-256",
		ah_hmac_sha2_256_init, ah_hmac_sha2_256_loop,
		ah_hmac_sha2_256_result, },
	{ ah_sumsiz_1216, ah_common_mature, 384, 384,
		"hmac-sha2-384",
		ah_hmac_sha2_384_init, ah_hmac_sha2_384_loop,
		ah_hmac_sha2_384_result, },
	{ ah_sumsiz_1216, ah_common_mature, 512, 512,
		"hmac-sha2-512",
		ah_hmac_sha2_512_init, ah_hmac_sha2_512_loop,
		ah_hmac_sha2_512_result, },
	{ ah_sumsiz_1216, ah_common_mature, 160, 160,
		"hmac-ripemd160",
		ah_hmac_ripemd160_init, ah_hmac_ripemd160_loop,
		ah_hmac_ripemd160_result, },
	{ ah_sumsiz_1216, ah_common_mature, 128, 128,
		"aes-xcbc-mac",
		ah_aes_xcbc_mac_init, ah_aes_xcbc_mac_loop,
		ah_aes_xcbc_mac_result, },
	{ ah_sumsiz_1216, ah_none_mature, 1, 80, /* TCP_KEYLEN_MIN/MAX */
		"TCP-MD5",
		ah_none_init, ah_none_loop,
		ah_none_result, },
};

const struct ah_algorithm *
ah_algorithm_lookup(idx)
	int idx;
{

	switch (idx) {
	case SADB_AALG_MD5HMAC:
		return &ah_algorithms[0];
	case SADB_AALG_SHA1HMAC:
		return &ah_algorithms[1];
	case SADB_X_AALG_MD5:
		return &ah_algorithms[2];
	case SADB_X_AALG_SHA:
		return &ah_algorithms[3];
	case SADB_X_AALG_NULL:
		return &ah_algorithms[4];
	case SADB_X_AALG_SHA2_256:
		return &ah_algorithms[5];
	case SADB_X_AALG_SHA2_384:
		return &ah_algorithms[6];
	case SADB_X_AALG_SHA2_512:
		return &ah_algorithms[7];
	case SADB_X_AALG_RIPEMD160HMAC:
		return &ah_algorithms[8];
	case SADB_X_AALG_AES_XCBC_MAC:
		return &ah_algorithms[9];
	case SADB_X_AALG_TCP_MD5:
		return &ah_algorithms[10];
	default:
		return NULL;
	}
}


static int
ah_sumsiz_1216(sav)
	struct secasvar *sav;
{
	if (!sav)
		panic("ah_sumsiz_1216: null pointer is passed");
	if (sav->flags & SADB_X_EXT_OLD)
		return 16;
	else
		return 12;
}

static int
ah_sumsiz_zero(sav)
	struct secasvar *sav;
{
	if (!sav)
		panic("ah_sumsiz_zero: null pointer is passed");
	return 0;
}

static int
ah_common_mature(sav)
	struct secasvar *sav;
{
	const struct ah_algorithm *algo;

	if (!sav->key_auth) {
		ipseclog((LOG_ERR, "ah_common_mature: no key is given.\n"));
		return 1;
	}

	algo = ah_algorithm_lookup(sav->alg_auth);
	if (!algo) {
		ipseclog((LOG_ERR, "ah_common_mature: unsupported algorithm.\n"));
		return 1;
	}

	if (sav->key_auth->sadb_key_bits < algo->keymin ||
	    algo->keymax < sav->key_auth->sadb_key_bits) {
		ipseclog((LOG_ERR,
		    "ah_common_mature: invalid key length %d for %s.\n",
		    sav->key_auth->sadb_key_bits, algo->name));
		return 1;
	}

	return 0;
}

static int
ah_none_mature(sav)
	struct secasvar *sav;
{
	if (sav->sah->saidx.proto == IPPROTO_AH) {
		ipseclog((LOG_ERR,
		    "ah_none_mature: protocol and algorithm mismatch.\n"));
		return 1;
	}
	return 0;
}

static int
ah_none_init(state, sav)
	struct ah_algorithm_state *state;
	struct secasvar *sav;
{
	state->foo = NULL;
	return 0;
}

static void
ah_none_loop(state, addr, len)
	struct ah_algorithm_state *state;
	u_int8_t * addr;
	size_t len;
{
}

static void
ah_none_result(state, addr, l)
	struct ah_algorithm_state *state;
	u_int8_t *addr;
	size_t l;
{
}

static int
ah_keyed_md5_mature(sav)
	struct secasvar *sav;
{
	/* anything is okay */
	return 0;
}

static int
ah_keyed_md5_init(state, sav)
	struct ah_algorithm_state *state;
	struct secasvar *sav;
{
	size_t padlen;
	size_t keybitlen;
	u_int8_t buf[32];

	if (!state)
		panic("ah_keyed_md5_init: what?");

	state->sav = sav;
	state->foo = (void *)malloc(sizeof(MD5_CTX), M_TEMP, M_NOWAIT);
	if (state->foo == NULL)
		return ENOBUFS;

	MD5Init((MD5_CTX *)state->foo);
	if (state->sav) {
		MD5Update((MD5_CTX *)state->foo,
			(u_int8_t *)_KEYBUF(state->sav->key_auth),
			(u_int)_KEYLEN(state->sav->key_auth));

		/*
		 * Pad after the key.
		 * We cannot simply use md5_pad() since the function
		 * won't update the total length.
		 */
		if (_KEYLEN(state->sav->key_auth) < 56)
			padlen = 64 - 8 - _KEYLEN(state->sav->key_auth);
		else
			padlen = 64 + 64 - 8 - _KEYLEN(state->sav->key_auth);
		keybitlen = _KEYLEN(state->sav->key_auth);
		keybitlen *= 8;

		buf[0] = 0x80;
		MD5Update((MD5_CTX *)state->foo, &buf[0], 1);
		padlen--;

		bzero(buf, sizeof(buf));
		while (sizeof(buf) < padlen) {
			MD5Update((MD5_CTX *)state->foo, &buf[0], sizeof(buf));
			padlen -= sizeof(buf);
		}
		if (padlen) {
			MD5Update((MD5_CTX *)state->foo, &buf[0], padlen);
		}

		buf[0] = (keybitlen >> 0) & 0xff;
		buf[1] = (keybitlen >> 8) & 0xff;
		buf[2] = (keybitlen >> 16) & 0xff;
		buf[3] = (keybitlen >> 24) & 0xff;
		MD5Update((MD5_CTX *)state->foo, buf, 8);
	}

	return 0;
}

static void
ah_keyed_md5_loop(state, addr, len)
	struct ah_algorithm_state *state;
	u_int8_t * addr;
	size_t len;
{
	if (!state)
		panic("ah_keyed_md5_loop: what?");

	MD5Update((MD5_CTX *)state->foo, addr, len);
}

static void
ah_keyed_md5_result(state, addr, l)
	struct ah_algorithm_state *state;
	u_int8_t *addr;
	size_t l;
{
	u_char digest[MD5_RESULTLEN];

	if (!state)
		panic("ah_keyed_md5_result: what?");

	if (state->sav) {
		MD5Update((MD5_CTX *)state->foo,
			(u_int8_t *)_KEYBUF(state->sav->key_auth),
			(u_int)_KEYLEN(state->sav->key_auth));
	}
	MD5Final(digest, (MD5_CTX *)state->foo);
	free(state->foo, M_TEMP);
	bcopy(digest, addr, sizeof(digest) > l ? l : sizeof(digest));
}

static int
ah_keyed_sha1_init(state, sav)
	struct ah_algorithm_state *state;
	struct secasvar *sav;
{
	SHA1_CTX *ctxt;
	size_t padlen;
	size_t keybitlen;
	u_int8_t buf[32];

	if (!state)
		panic("ah_keyed_sha1_init: what?");

	state->sav = sav;
	state->foo = (void *)malloc(sizeof(SHA1_CTX), M_TEMP, M_NOWAIT);
	if (!state->foo)
		return ENOBUFS;

	ctxt = (SHA1_CTX *)state->foo;
	SHA1Init(ctxt);

	if (state->sav) {
		SHA1Update(ctxt, (u_int8_t *)_KEYBUF(state->sav->key_auth),
			(u_int)_KEYLEN(state->sav->key_auth));

		/*
		 * Pad after the key.
		 */
		if (_KEYLEN(state->sav->key_auth) < 56)
			padlen = 64 - 8 - _KEYLEN(state->sav->key_auth);
		else
			padlen = 64 + 64 - 8 - _KEYLEN(state->sav->key_auth);
		keybitlen = _KEYLEN(state->sav->key_auth);
		keybitlen *= 8;

		buf[0] = 0x80;
		SHA1Update(ctxt, &buf[0], 1);
		padlen--;

		bzero(buf, sizeof(buf));
		while (sizeof(buf) < padlen) {
			SHA1Update(ctxt, &buf[0], sizeof(buf));
			padlen -= sizeof(buf);
		}
		if (padlen) {
			SHA1Update(ctxt, &buf[0], padlen);
		}

		buf[0] = (keybitlen >> 0) & 0xff;
		buf[1] = (keybitlen >> 8) & 0xff;
		buf[2] = (keybitlen >> 16) & 0xff;
		buf[3] = (keybitlen >> 24) & 0xff;
		SHA1Update(ctxt, buf, 8);
	}

	return 0;
}

static void
ah_keyed_sha1_loop(state, addr, len)
	struct ah_algorithm_state *state;
	u_int8_t * addr;
	size_t len;
{
	SHA1_CTX *ctxt;

	if (!state || !state->foo)
		panic("ah_keyed_sha1_loop: what?");
	ctxt = (SHA1_CTX *)state->foo;

	SHA1Update(ctxt, (u_int8_t *)addr, (size_t)len);
}

static void
ah_keyed_sha1_result(state, addr, l)
	struct ah_algorithm_state *state;
	u_int8_t *addr;
	size_t l;
{
	u_char digest[SHA1_RESULTLEN];	/* SHA-1 generates 160 bits */
	SHA1_CTX *ctxt;

	if (!state || !state->foo)
		panic("ah_keyed_sha1_result: what?");
	ctxt = (SHA1_CTX *)state->foo;

	if (state->sav) {
		SHA1Update(ctxt, (u_int8_t *)_KEYBUF(state->sav->key_auth),
			(u_int)_KEYLEN(state->sav->key_auth));
	}
	SHA1Final((u_int8_t *)digest, ctxt);
	bcopy(digest, addr, sizeof(digest) > l ? l : sizeof(digest));

	free(state->foo, M_TEMP);
}

static int
ah_hmac_md5_init(state, sav)
	struct ah_algorithm_state *state;
	struct secasvar *sav;
{
	u_char *ipad;
	u_char *opad;
	u_char tk[MD5_RESULTLEN];
	u_char *key;
	size_t keylen;
	size_t i;
	MD5_CTX *ctxt;

	if (!state)
		panic("ah_hmac_md5_init: what?");

	state->sav = sav;
	state->foo = (void *)malloc(64 + 64 + sizeof(MD5_CTX), M_TEMP, M_NOWAIT);
	if (!state->foo)
		return ENOBUFS;

	ipad = (u_char *)state->foo;
	opad = (u_char *)(ipad + 64);
	ctxt = (MD5_CTX *)(opad + 64);

	/* compress the key if necessery */
	if (64 < _KEYLEN(state->sav->key_auth)) {
		MD5Init(ctxt);
		MD5Update(ctxt, _KEYBUF(state->sav->key_auth),
			_KEYLEN(state->sav->key_auth));
		MD5Final(&tk[0], ctxt);
		key = &tk[0];
		keylen = 16;
	} else {
		key = _KEYBUF(state->sav->key_auth);
		keylen = _KEYLEN(state->sav->key_auth);
	}

	bzero(ipad, 64);
	bzero(opad, 64);
	bcopy(key, ipad, keylen);
	bcopy(key, opad, keylen);
	for (i = 0; i < 64; i++) {
		ipad[i] ^= 0x36;
		opad[i] ^= 0x5c;
	}

	MD5Init(ctxt);
	MD5Update(ctxt, ipad, 64);

	return 0;
}

static void
ah_hmac_md5_loop(state, addr, len)
	struct ah_algorithm_state *state;
	u_int8_t * addr;
	size_t len;
{
	MD5_CTX *ctxt;

	if (!state || !state->foo)
		panic("ah_hmac_md5_loop: what?");
	ctxt = (MD5_CTX *)(((u_int8_t *)state->foo) + 128);
	MD5Update(ctxt, addr, len);
}

static void
ah_hmac_md5_result(state, addr, l)
	struct ah_algorithm_state *state;
	u_int8_t *addr;
	size_t l;
{
	u_char digest[MD5_RESULTLEN];
	u_char *ipad;
	u_char *opad;
	MD5_CTX *ctxt;

	if (!state || !state->foo)
		panic("ah_hmac_md5_result: what?");

	ipad = (u_char *)state->foo;
	opad = (u_char *)(ipad + 64);
	ctxt = (MD5_CTX *)(opad + 64);

	MD5Final(digest, ctxt);

	MD5Init(ctxt);
	MD5Update(ctxt, opad, 64);
	MD5Update(ctxt, digest, sizeof(digest));
	MD5Final(digest, ctxt);

	bcopy(digest, addr, sizeof(digest) > l ? l : sizeof(digest));

	free(state->foo, M_TEMP);
}

static int
ah_hmac_sha1_init(state, sav)
	struct ah_algorithm_state *state;
	struct secasvar *sav;
{
	u_char *ipad;
	u_char *opad;
	SHA1_CTX *ctxt;
	u_char tk[SHA1_RESULTLEN];	/* SHA-1 generates 160 bits */
	u_char *key;
	size_t keylen;
	size_t i;

	if (!state)
		panic("ah_hmac_sha1_init: what?");

	state->sav = sav;
	state->foo = (void *)malloc(64 + 64 + sizeof(SHA1_CTX),
			M_TEMP, M_NOWAIT);
	if (!state->foo)
		return ENOBUFS;

	ipad = (u_char *)state->foo;
	opad = (u_char *)(ipad + 64);
	ctxt = (SHA1_CTX *)(opad + 64);

	/* compress the key if necessery */
	if (64 < _KEYLEN(state->sav->key_auth)) {
		SHA1Init(ctxt);
		SHA1Update(ctxt, _KEYBUF(state->sav->key_auth),
			_KEYLEN(state->sav->key_auth));
		SHA1Final(&tk[0], ctxt);
		key = &tk[0];
		keylen = SHA1_RESULTLEN;
	} else {
		key = _KEYBUF(state->sav->key_auth);
		keylen = _KEYLEN(state->sav->key_auth);
	}

	bzero(ipad, 64);
	bzero(opad, 64);
	bcopy(key, ipad, keylen);
	bcopy(key, opad, keylen);
	for (i = 0; i < 64; i++) {
		ipad[i] ^= 0x36;
		opad[i] ^= 0x5c;
	}

	SHA1Init(ctxt);
	SHA1Update(ctxt, ipad, 64);

	return 0;
}

static void
ah_hmac_sha1_loop(state, addr, len)
	struct ah_algorithm_state *state;
	u_int8_t * addr;
	size_t len;
{
	SHA1_CTX *ctxt;

	if (!state || !state->foo)
		panic("ah_hmac_sha1_loop: what?");

	ctxt = (SHA1_CTX *)(((u_char *)state->foo) + 128);
	SHA1Update(ctxt, (u_int8_t *)addr, (size_t)len);
}

static void
ah_hmac_sha1_result(state, addr, l)
	struct ah_algorithm_state *state;
	u_int8_t *addr;
	size_t l;
{
	u_char digest[SHA1_RESULTLEN];	/* SHA-1 generates 160 bits */
	u_char *ipad;
	u_char *opad;
	SHA1_CTX *ctxt;

	if (!state || !state->foo)
		panic("ah_hmac_sha1_result: what?");

	ipad = (u_char *)state->foo;
	opad = (u_char *)(ipad + 64);
	ctxt = (SHA1_CTX *)(opad + 64);

	SHA1Final((u_int8_t *)digest, ctxt);

	SHA1Init(ctxt);
	SHA1Update(ctxt, opad, 64);
	SHA1Update(ctxt, (u_int8_t *)digest, sizeof(digest));
	SHA1Final((u_int8_t *)digest, ctxt);

	bcopy(digest, addr, sizeof(digest) > l ? l : sizeof(digest));

	free(state->foo, M_TEMP);
}

static int
ah_hmac_sha2_256_init(state, sav)
	struct ah_algorithm_state *state;
	struct secasvar *sav;
{
	u_char *ipad;
	u_char *opad;
	SHA256_CTX *ctxt;
	u_char tk[SHA256_DIGEST_LENGTH];
	u_char *key;
	size_t keylen;
	size_t i;

	if (!state)
		panic("ah_hmac_sha2_256_init: what?");

	state->sav = sav;
	state->foo = (void *)malloc(64 + 64 + sizeof(SHA256_CTX),
	    M_TEMP, M_NOWAIT);
	if (!state->foo)
		return ENOBUFS;

	ipad = (u_char *)state->foo;
	opad = (u_char *)(ipad + 64);
	ctxt = (SHA256_CTX *)(opad + 64);

	/* compress the key if necessery */
	if (64 < _KEYLEN(state->sav->key_auth)) {
		bzero(tk, sizeof(tk));
		bzero(ctxt, sizeof(*ctxt));
		SHA256_Init(ctxt);
		SHA256_Update(ctxt, _KEYBUF(state->sav->key_auth),
		    _KEYLEN(state->sav->key_auth));
		SHA256_Final(&tk[0], ctxt);
		key = &tk[0];
		keylen = sizeof(tk) < 64 ? sizeof(tk) : 64;
	} else {
		key = _KEYBUF(state->sav->key_auth);
		keylen = _KEYLEN(state->sav->key_auth);
	}

	bzero(ipad, 64);
	bzero(opad, 64);
	bcopy(key, ipad, keylen);
	bcopy(key, opad, keylen);
	for (i = 0; i < 64; i++) {
		ipad[i] ^= 0x36;
		opad[i] ^= 0x5c;
	}

	bzero(ctxt, sizeof(*ctxt));
	SHA256_Init(ctxt);
	SHA256_Update(ctxt, ipad, 64);

	return 0;
}

static void
ah_hmac_sha2_256_loop(state, addr, len)
	struct ah_algorithm_state *state;
	u_int8_t *addr;
	size_t len;
{
	SHA256_CTX *ctxt;

	if (!state || !state->foo)
		panic("ah_hmac_sha2_256_loop: what?");

	ctxt = (SHA256_CTX *)(((u_char *)state->foo) + 128);
	SHA256_Update(ctxt, (caddr_t)addr, (size_t)len);
}

static void
ah_hmac_sha2_256_result(state, addr, l)
	struct ah_algorithm_state *state;
	u_int8_t *addr;
	size_t l;
{
	u_char digest[SHA256_DIGEST_LENGTH];
	u_char *ipad;
	u_char *opad;
	SHA256_CTX *ctxt;

	if (!state || !state->foo)
		panic("ah_hmac_sha2_256_result: what?");

	ipad = (u_char *)state->foo;
	opad = (u_char *)(ipad + 64);
	ctxt = (SHA256_CTX *)(opad + 64);

	SHA256_Final((caddr_t)digest, ctxt);

	bzero(ctxt, sizeof(*ctxt));
	SHA256_Init(ctxt);
	SHA256_Update(ctxt, opad, 64);
	SHA256_Update(ctxt, (caddr_t)digest, sizeof(digest));
	SHA256_Final((caddr_t)digest, ctxt);

	bcopy(digest, addr, sizeof(digest) > l ? l : sizeof(digest));

	free(state->foo, M_TEMP);
}

static int
ah_hmac_sha2_384_init(state, sav)
	struct ah_algorithm_state *state;
	struct secasvar *sav;
{
	u_char *ipad;
	u_char *opad;
	SHA384_CTX *ctxt;
	u_char tk[SHA384_DIGEST_LENGTH];
	u_char *key;
	size_t keylen;
	size_t i;

	if (!state)
		panic("ah_hmac_sha2_384_init: what?");

	state->sav = sav;
	state->foo = (void *)malloc(64 + 64 + sizeof(SHA384_CTX),
	    M_TEMP, M_NOWAIT);
	if (!state->foo)
		return ENOBUFS;
	bzero(state->foo, 64 + 64 + sizeof(SHA384_CTX));

	ipad = (u_char *)state->foo;
	opad = (u_char *)(ipad + 64);
	ctxt = (SHA384_CTX *)(opad + 64);

	/* compress the key if necessery */
	if (64 < _KEYLEN(state->sav->key_auth)) {
		bzero(tk, sizeof(tk));
		bzero(ctxt, sizeof(*ctxt));
		SHA384_Init(ctxt);
		SHA384_Update(ctxt, _KEYBUF(state->sav->key_auth),
		    _KEYLEN(state->sav->key_auth));
		SHA384_Final(&tk[0], ctxt);
		key = &tk[0];
		keylen = sizeof(tk) < 64 ? sizeof(tk) : 64;
	} else {
		key = _KEYBUF(state->sav->key_auth);
		keylen = _KEYLEN(state->sav->key_auth);
	}

	bzero(ipad, 64);
	bzero(opad, 64);
	bcopy(key, ipad, keylen);
	bcopy(key, opad, keylen);
	for (i = 0; i < 64; i++) {
		ipad[i] ^= 0x36;
		opad[i] ^= 0x5c;
	}

	bzero(ctxt, sizeof(*ctxt));
	SHA384_Init(ctxt);
	SHA384_Update(ctxt, ipad, 64);

	return 0;
}

static void
ah_hmac_sha2_384_loop(state, addr, len)
	struct ah_algorithm_state *state;
	u_int8_t *addr;
	size_t len;
{
	SHA384_CTX *ctxt;

	if (!state || !state->foo)
		panic("ah_hmac_sha2_384_loop: what?");

	ctxt = (SHA384_CTX *)(((u_char *)state->foo) + 128);
	SHA384_Update(ctxt, (caddr_t)addr, (size_t)len);
}

static void
ah_hmac_sha2_384_result(state, addr, l)
	struct ah_algorithm_state *state;
	u_int8_t *addr;
	size_t l;
{
	u_char digest[SHA384_DIGEST_LENGTH];
	u_char *ipad;
	u_char *opad;
	SHA384_CTX *ctxt;

	if (!state || !state->foo)
		panic("ah_hmac_sha2_384_result: what?");

	ipad = (u_char *)state->foo;
	opad = (u_char *)(ipad + 64);
	ctxt = (SHA384_CTX *)(opad + 64);

	SHA384_Final((caddr_t)digest, ctxt);

	bzero(ctxt, sizeof(*ctxt));
	SHA384_Init(ctxt);
	SHA384_Update(ctxt, opad, 64);
	SHA384_Update(ctxt, (caddr_t)digest, sizeof(digest));
	SHA384_Final((caddr_t)digest, ctxt);

	bcopy(digest, addr, sizeof(digest) > l ? l : sizeof(digest));

	free(state->foo, M_TEMP);
}

static int
ah_hmac_sha2_512_init(state, sav)
	struct ah_algorithm_state *state;
	struct secasvar *sav;
{
	u_char *ipad;
	u_char *opad;
	SHA512_CTX *ctxt;
	u_char tk[SHA512_DIGEST_LENGTH];
	u_char *key;
	size_t keylen;
	size_t i;

	if (!state)
		panic("ah_hmac_sha2_512_init: what?");

	state->sav = sav;
	state->foo = (void *)malloc(64 + 64 + sizeof(SHA512_CTX),
	    M_TEMP, M_NOWAIT);
	if (!state->foo)
		return ENOBUFS;
	bzero(state->foo, 64 + 64 + sizeof(SHA512_CTX));

	ipad = (u_char *)state->foo;
	opad = (u_char *)(ipad + 64);
	ctxt = (SHA512_CTX *)(opad + 64);

	/* compress the key if necessery */
	if (64 < _KEYLEN(state->sav->key_auth)) {
		bzero(tk, sizeof(tk));
		bzero(ctxt, sizeof(*ctxt));
		SHA512_Init(ctxt);
		SHA512_Update(ctxt, _KEYBUF(state->sav->key_auth),
		    _KEYLEN(state->sav->key_auth));
		SHA512_Final(&tk[0], ctxt);
		key = &tk[0];
		keylen = sizeof(tk) < 64 ? sizeof(tk) : 64;
	} else {
		key = _KEYBUF(state->sav->key_auth);
		keylen = _KEYLEN(state->sav->key_auth);
	}

	bzero(ipad, 64);
	bzero(opad, 64);
	bcopy(key, ipad, keylen);
	bcopy(key, opad, keylen);
	for (i = 0; i < 64; i++) {
		ipad[i] ^= 0x36;
		opad[i] ^= 0x5c;
	}

	bzero(ctxt, sizeof(*ctxt));
	SHA512_Init(ctxt);
	SHA512_Update(ctxt, ipad, 64);

	return 0;
}

static void
ah_hmac_sha2_512_loop(state, addr, len)
	struct ah_algorithm_state *state;
	u_int8_t *addr;
	size_t len;
{
	SHA512_CTX *ctxt;

	if (!state || !state->foo)
		panic("ah_hmac_sha2_512_loop: what?");

	ctxt = (SHA512_CTX *)(((u_char *)state->foo) + 128);
	SHA512_Update(ctxt, (caddr_t)addr, (size_t)len);
}

static void
ah_hmac_sha2_512_result(state, addr, l)
	struct ah_algorithm_state *state;
	u_int8_t *addr;
	size_t l;
{
	u_char digest[SHA512_DIGEST_LENGTH];
	u_char *ipad;
	u_char *opad;
	SHA512_CTX *ctxt;

	if (!state || !state->foo)
		panic("ah_hmac_sha2_512_result: what?");

	ipad = (u_char *)state->foo;
	opad = (u_char *)(ipad + 64);
	ctxt = (SHA512_CTX *)(opad + 64);

	SHA512_Final((caddr_t)digest, ctxt);

	bzero(ctxt, sizeof(*ctxt));
	SHA512_Init(ctxt);
	SHA512_Update(ctxt, opad, 64);
	SHA512_Update(ctxt, (caddr_t)digest, sizeof(digest));
	SHA512_Final((caddr_t)digest, ctxt);

	bcopy(digest, addr, sizeof(digest) > l ? l : sizeof(digest));

	free(state->foo, M_TEMP);
}

static int
ah_hmac_ripemd160_init(state, sav)
	struct ah_algorithm_state *state;
	struct secasvar *sav;
{
	u_char *ipad;
	u_char *opad;
	RMD160_CTX *ctxt;
	u_char tk[RIPEMD160_RESULTLEN];
	u_char *key;
	size_t keylen;
	size_t i;

	if (!state)
		panic("ah_hmac_ripemd160_init: what?");

	state->sav = sav;
	state->foo = (void *)malloc(64 + 64 + sizeof(RMD160_CTX),
	    M_TEMP, M_NOWAIT);
	if (!state->foo)
		return ENOBUFS;
	bzero(state->foo, 64 + 64 + sizeof(RMD160_CTX));

	ipad = (u_char *)state->foo;
	opad = (u_char *)(ipad + 64);
	ctxt = (RMD160_CTX *)(opad + 64);

	/* compress the key if necessery */
	if (64 < _KEYLEN(state->sav->key_auth)) {
		bzero(tk, sizeof(tk));
		bzero(ctxt, sizeof(*ctxt));
		RMD160Init(ctxt);
		RMD160Update(ctxt, _KEYBUF(state->sav->key_auth),
		    _KEYLEN(state->sav->key_auth));
		RMD160Final(&tk[0], ctxt);
		key = &tk[0];
		keylen = sizeof(tk) < 64 ? sizeof(tk) : 64;
	} else {
		key = _KEYBUF(state->sav->key_auth);
		keylen = _KEYLEN(state->sav->key_auth);
	}

	bzero(ipad, 64);
	bzero(opad, 64);
	bcopy(key, ipad, keylen);
	bcopy(key, opad, keylen);
	for (i = 0; i < 64; i++) {
		ipad[i] ^= 0x36;
		opad[i] ^= 0x5c;
	}

	bzero(ctxt, sizeof(*ctxt));
	RMD160Init(ctxt);
	RMD160Update(ctxt, ipad, 64);

	return 0;
}

static void
ah_hmac_ripemd160_loop(state, addr, len)
	struct ah_algorithm_state *state;
	u_int8_t *addr;
	size_t len;
{
	RMD160_CTX *ctxt;

	if (!state || !state->foo)
		panic("ah_hmac_ripemd160_loop: what?");

	ctxt = (RMD160_CTX *)(((u_char *)state->foo) + 128);
	RMD160Update(ctxt, (caddr_t)addr, (size_t)len);
}

static void
ah_hmac_ripemd160_result(state, addr, l)
	struct ah_algorithm_state *state;
	u_int8_t *addr;
	size_t l;
{
	u_char digest[RIPEMD160_RESULTLEN];
	u_char *ipad;
	u_char *opad;
	RMD160_CTX *ctxt;

	if (!state || !state->foo)
		panic("ah_hmac_ripemd160_result: what?");

	ipad = (u_char *)state->foo;
	opad = (u_char *)(ipad + 64);
	ctxt = (RMD160_CTX *)(opad + 64);

	RMD160Final((caddr_t)digest, ctxt);

	bzero(ctxt, sizeof(*ctxt));
	RMD160Init(ctxt);
	RMD160Update(ctxt, opad, 64);
	RMD160Update(ctxt, (caddr_t)digest, sizeof(digest));
	RMD160Final((caddr_t)digest, ctxt);

	bcopy(digest, addr, sizeof(digest) > l ? l : sizeof(digest));

	free(state->foo, M_TEMP);
}

/*------------------------------------------------------------*/

/*
 * go generate the checksum.
 */
static void
ah_update_mbuf(m, off, len, algo, algos)
	struct mbuf *m;
	int off;
	int len;
	const struct ah_algorithm *algo;
	struct ah_algorithm_state *algos;
{
	struct mbuf *n;
	int tlen;

	/* easy case first */
	if (off + len <= m->m_len) {
		(algo->update)(algos, mtod(m, u_int8_t *) + off, len);
		return;
	}

	for (n = m; n; n = n->m_next) {
		if (off < n->m_len)
			break;

		off -= n->m_len;
	}

	if (!n)
		panic("ah_update_mbuf: wrong offset specified");

	for (/* nothing */; n && len > 0; n = n->m_next) {
		if (n->m_len == 0)
			continue;
		if (n->m_len - off < len)
			tlen = n->m_len - off;
		else
			tlen = len;

		(algo->update)(algos, mtod(n, u_int8_t *) + off, tlen);

		len -= tlen;
		off = 0;
	}
}

#ifdef INET
/*
 * Go generate the checksum. This function won't modify the mbuf chain
 * except AH itself.
 *
 * NOTE: the function does not free mbuf on failure.
 * Don't use m_copy(), it will try to share cluster mbuf by using refcnt.
 */
int
ah4_calccksum(m, ahdat, len, algo, sav)
	struct mbuf *m;
	u_int8_t * ahdat;
	size_t len;
	const struct ah_algorithm *algo;
	struct secasvar *sav;
{
	int off;
	int hdrtype;
	size_t advancewidth;
	struct ah_algorithm_state algos;
	u_char sumbuf[AH_MAXSUMSIZE];
	int error = 0;
	int ahseen;
	struct mbuf *n = NULL;

	if ((m->m_flags & M_PKTHDR) == 0)
		return EINVAL;

	ahseen = 0;
	hdrtype = -1;	/* dummy, it is called IPPROTO_IP */

	off = 0;

	error = (algo->init)(&algos, sav);
	if (error)
		return error;

	advancewidth = 0;	/* safety */

again:
	/* gory. */
	switch (hdrtype) {
	case -1:	/* first one only */
	    {
		/*
		 * copy ip hdr, modify to fit the AH checksum rule,
		 * then take a checksum.
		 */
		struct ip iphdr;
		size_t hlen;

		m_copydata(m, off, sizeof(iphdr), (caddr_t)&iphdr);
#ifdef _IP_VHL
		hlen = IP_VHL_HL(iphdr.ip_vhl) << 2;
#else
		hlen = iphdr.ip_hl << 2;
#endif
		iphdr.ip_ttl = 0;
		iphdr.ip_sum = htons(0);
		if (ip4_ah_cleartos)
			iphdr.ip_tos = 0;
		iphdr.ip_off = htons(ntohs(iphdr.ip_off) & ip4_ah_offsetmask);
		(algo->update)(&algos, (u_int8_t *)&iphdr, sizeof(struct ip));

		if (hlen != sizeof(struct ip)) {
			u_char *p;
			int i, l, skip;

			if (hlen > MCLBYTES) {
				error = EMSGSIZE;
				goto fail;
			}
			MGET(n, M_DONTWAIT, MT_DATA);
			if (n && hlen > MLEN) {
				MCLGET(n, M_DONTWAIT);
				if ((n->m_flags & M_EXT) == 0) {
					m_free(n);
					n = NULL;
				}
			}
			if (n == NULL) {
				error = ENOBUFS;
				goto fail;
			}
			m_copydata(m, off, hlen, mtod(n, caddr_t));

			/*
			 * IP options processing.
			 * See RFC2402 appendix A.
			 */
			p = mtod(n, u_char *);
			i = sizeof(struct ip);
			while (i < hlen) {
				if (i + IPOPT_OPTVAL >= hlen) {
					ipseclog((LOG_ERR, "ah4_calccksum: "
					    "invalid IP option\n"));
					error = EINVAL;
					goto fail;
				}
				if (p[i + IPOPT_OPTVAL] == IPOPT_EOL ||
				    p[i + IPOPT_OPTVAL] == IPOPT_NOP ||
				    i + IPOPT_OLEN < hlen)
					;
				else {
					ipseclog((LOG_ERR,
					    "ah4_calccksum: invalid IP option "
					    "(type=%02x)\n",
					    p[i + IPOPT_OPTVAL]));
					error = EINVAL;
					goto fail;
				}

				skip = 1;
				switch (p[i + IPOPT_OPTVAL]) {
				case IPOPT_EOL:
				case IPOPT_NOP:
					l = 1;
					skip = 0;
					break;
				case IPOPT_SECURITY:	/* 0x82 */
				case 0x85:	/* Extended security */
				case 0x86:	/* Commercial security */
				case 0x94:	/* Router alert */
				case 0x95:	/* RFC1770 */
					l = p[i + IPOPT_OLEN];
					if (l < 2)
						goto invalopt;
					skip = 0;
					break;
				default:
					l = p[i + IPOPT_OLEN];
					if (l < 2)
						goto invalopt;
					skip = 1;
					break;
				}
				if (l < 1 || hlen - i < l) {
			invalopt:
					ipseclog((LOG_ERR,
					    "ah4_calccksum: invalid IP option "
					    "(type=%02x len=%02x)\n",
					    p[i + IPOPT_OPTVAL],
					    p[i + IPOPT_OLEN]));
					error = EINVAL;
					goto fail;
				}
				if (skip)
					bzero(p + i, l);
				if (p[i + IPOPT_OPTVAL] == IPOPT_EOL)
					break;
				i += l;
			}
			p = mtod(n, u_char *) + sizeof(struct ip);
			(algo->update)(&algos, p, hlen - sizeof(struct ip));

			m_free(n);
			n = NULL;
		}

		hdrtype = (iphdr.ip_p) & 0xff;
		advancewidth = hlen;
		break;
	    }

	case IPPROTO_AH:
	    {
		struct ah ah;
		int siz;
		int hdrsiz;
		int totlen;

		m_copydata(m, off, sizeof(ah), (caddr_t)&ah);
		hdrsiz = (sav->flags & SADB_X_EXT_OLD)
				? sizeof(struct ah)
				: sizeof(struct newah);
		siz = (*algo->sumsiz)(sav);
		totlen = (ah.ah_len + 2) << 2;

		/*
		 * special treatment is necessary for the first one, not others
		 */
		if (!ahseen) {
			if (totlen > m->m_pkthdr.len - off ||
			    totlen > MCLBYTES) {
				error = EMSGSIZE;
				goto fail;
			}
			MGET(n, M_DONTWAIT, MT_DATA);
			if (n && totlen > MLEN) {
				MCLGET(n, M_DONTWAIT);
				if ((n->m_flags & M_EXT) == 0) {
					m_free(n);
					n = NULL;
				}
			}
			if (n == NULL) {
				error = ENOBUFS;
				goto fail;
			}
			m_copydata(m, off, totlen, mtod(n, caddr_t));
			n->m_len = totlen;
			bzero(mtod(n, u_int8_t *) + hdrsiz, siz);
			(algo->update)(&algos, mtod(n, u_int8_t *), n->m_len);
			m_free(n);
			n = NULL;
		} else
			ah_update_mbuf(m, off, totlen, algo, &algos);
		ahseen++;

		hdrtype = ah.ah_nxt;
		advancewidth = totlen;
		break;
	    }

	default:
		ah_update_mbuf(m, off, m->m_pkthdr.len - off, algo, &algos);
		advancewidth = m->m_pkthdr.len - off;
		break;
	}

	off += advancewidth;
	if (off < m->m_pkthdr.len)
		goto again;

	if (len < (*algo->sumsiz)(sav)) {
		error = EINVAL;
		goto fail;
	}

	(algo->result)(&algos, sumbuf, sizeof(sumbuf));
	bcopy(&sumbuf[0], ahdat, (*algo->sumsiz)(sav));

	if (n)
		m_free(n);
	return error;

fail:
	if (n)
		m_free(n);
	return error;
}
#endif

#ifdef INET6
/*
 * Go generate the checksum. This function won't modify the mbuf chain
 * except AH itself.
 *
 * NOTE: the function does not free mbuf on failure.
 * Don't use m_copy(), it will try to share cluster mbuf by using refcnt.
 */
int
ah6_calccksum(m, ahdat, len, algo, sav)
	struct mbuf *m;
	u_int8_t * ahdat;
	size_t len;
	const struct ah_algorithm *algo;
	struct secasvar *sav;
{
	int newoff, off;
	int proto, nxt;
	struct mbuf *n = NULL;
	int error;
	int ahseen;
	struct ah_algorithm_state algos;
	u_char sumbuf[AH_MAXSUMSIZE];

	if ((m->m_flags & M_PKTHDR) == 0)
		return EINVAL;

	error = (algo->init)(&algos, sav);
	if (error)
		return error;

	off = 0;
	proto = IPPROTO_IPV6;
	nxt = -1;
	ahseen = 0;

 again:
	newoff = ip6_nexthdr(m, off, proto, &nxt);
	if (newoff < 0)
		newoff = m->m_pkthdr.len;
	else if (newoff <= off) {
		error = EINVAL;
		goto fail;
	}

	switch (proto) {
	case IPPROTO_IPV6:
		/*
		 * special treatment is necessary for the first one, not others
		 */
		if (off == 0) {
			struct ip6_hdr ip6copy;

			if (newoff - off != sizeof(struct ip6_hdr)) {
				error = EINVAL;
				goto fail;
			}

			m_copydata(m, off, newoff - off, (caddr_t)&ip6copy);
			/* RFC2402 */
			ip6copy.ip6_flow = 0;
			ip6copy.ip6_vfc &= ~IPV6_VERSION_MASK;
			ip6copy.ip6_vfc |= IPV6_VERSION;
			ip6copy.ip6_hlim = 0;
			in6_clearscope(&ip6copy.ip6_src); /* XXX */
			in6_clearscope(&ip6copy.ip6_dst); /* XXX */
			(algo->update)(&algos, (u_int8_t *)&ip6copy,
				       sizeof(struct ip6_hdr));
		} else {
			newoff = m->m_pkthdr.len;
			ah_update_mbuf(m, off, m->m_pkthdr.len - off, algo,
			    &algos);
		}
		break;

	case IPPROTO_AH:
	    {
		int siz;
		int hdrsiz;

		hdrsiz = (sav->flags & SADB_X_EXT_OLD)
				? sizeof(struct ah)
				: sizeof(struct newah);
		siz = (*algo->sumsiz)(sav);

		/*
		 * special treatment is necessary for the first one, not others
		 */
		if (!ahseen) {
			if (newoff - off > MCLBYTES) {
				error = EMSGSIZE;
				goto fail;
			}
			MGET(n, M_DONTWAIT, MT_DATA);
			if (n && newoff - off > MLEN) {
				MCLGET(n, M_DONTWAIT);
				if ((n->m_flags & M_EXT) == 0) {
					m_free(n);
					n = NULL;
				}
			}
			if (n == NULL) {
				error = ENOBUFS;
				goto fail;
			}
			m_copydata(m, off, newoff - off, mtod(n, caddr_t));
			n->m_len = newoff - off;
			bzero(mtod(n, u_int8_t *) + hdrsiz, siz);
			(algo->update)(&algos, mtod(n, u_int8_t *), n->m_len);
			m_free(n);
			n = NULL;
		} else
			ah_update_mbuf(m, off, newoff - off, algo, &algos);
		ahseen++;
		break;
	    }

	case IPPROTO_HOPOPTS:
	case IPPROTO_DSTOPTS:
	 {
		struct ip6_ext *ip6e;
		int hdrlen, optlen;
		u_int8_t *p, *optend, *optp;

		if (newoff - off > MCLBYTES) {
			error = EMSGSIZE;
			goto fail;
		}
		MGET(n, M_DONTWAIT, MT_DATA);
		if (n && newoff - off > MLEN) {
			MCLGET(n, M_DONTWAIT);
			if ((n->m_flags & M_EXT) == 0) {
				m_free(n);
				n = NULL;
			}
		}
		if (n == NULL) {
			error = ENOBUFS;
			goto fail;
		}
		m_copydata(m, off, newoff - off, mtod(n, caddr_t));
		n->m_len = newoff - off;

		ip6e = mtod(n, struct ip6_ext *);
		hdrlen = (ip6e->ip6e_len + 1) << 3;
		if (newoff - off < hdrlen) {
			 error = EINVAL;
			 m_free(n);
			 n = NULL;
			 goto fail;
		}
		p = mtod(n, u_int8_t *);
		optend = p + hdrlen;

		/*
		 * ICV calculation for the options header including all
		 * options.  This part is a little tricky since there are
		 * two type of options; mutable and immutable.  We try to
		 * null-out mutable ones here.
		 */
		optp = p + 2;
		while (optp < optend) {
			if (optp[0] == IP6OPT_PAD1)
				optlen = 1;
			else {
				if (optp + 2 > optend) {
					error = EINVAL;
					m_free(n);
					n = NULL;
					goto fail;
				}
				optlen = optp[1] + 2;
			}

			if (optp + optlen > optend) {
				error = EINVAL;
				m_free(n);
				n = NULL;
				goto fail;
			}

			if (optp[0] & IP6OPT_MUTABLE)
				bzero(optp + 2, optlen - 2);

			optp += optlen;
		}

		(algo->update)(&algos, mtod(n, u_int8_t *), n->m_len);
		m_free(n);
		n = NULL;
		break;
	 }

	case IPPROTO_ROUTING:
		/*
		 * For an input packet, we can just calculate `as is'.
		 * For an output packet, we assume ip6_output have already
		 * made packet how it will be received at the final
		 * destination.
		 */
		/* FALLTHROUGH */

	default:
		ah_update_mbuf(m, off, newoff - off, algo, &algos);
		break;
	}

	if (newoff < m->m_pkthdr.len) {
		proto = nxt;
		off = newoff;
		goto again;
	}

	if (len < (*algo->sumsiz)(sav)) {
		error = EINVAL;
		goto fail;
	}

	(algo->result)(&algos, sumbuf, sizeof(sumbuf));
	bcopy(&sumbuf[0], ahdat, (*algo->sumsiz)(sav));

	/* just in case */
	if (n)
		m_free(n);
	return 0;
fail:
	/* just in case */
	if (n)
		m_free(n);
	return error;
}
#endif
