/*-
 * Copyright (c) 2017-2019 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: John Baldwin <jhb@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_kern_tls.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/ktls.h>
#include <sys/malloc.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>

#include "common/common.h"
#include "crypto/t4_crypto.h"

/*
 * Crypto operations use a key context to store cipher keys and
 * partial hash digests.  They can either be passed inline as part of
 * a work request using crypto or they can be stored in card RAM.  For
 * the latter case, work requests must replace the inline key context
 * with a request to read the context from card RAM.
 *
 * The format of a key context:
 *
 * +-------------------------------+
 * | key context header            |
 * +-------------------------------+
 * | AES key                       |  ----- For requests with AES
 * +-------------------------------+
 * | Hash state                    |  ----- For hash-only requests
 * +-------------------------------+ -
 * | IPAD (16-byte aligned)        |  \
 * +-------------------------------+  +---- For requests with HMAC
 * | OPAD (16-byte aligned)        |  /
 * +-------------------------------+ -
 * | GMAC H                        |  ----- For AES-GCM
 * +-------------------------------+ -
 */

/* Fields in the key context header. */
#define S_TLS_KEYCTX_TX_WR_DUALCK    12
#define M_TLS_KEYCTX_TX_WR_DUALCK    0x1
#define V_TLS_KEYCTX_TX_WR_DUALCK(x) ((x) << S_TLS_KEYCTX_TX_WR_DUALCK)
#define G_TLS_KEYCTX_TX_WR_DUALCK(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_DUALCK) & M_TLS_KEYCTX_TX_WR_DUALCK)
#define F_TLS_KEYCTX_TX_WR_DUALCK    V_TLS_KEYCTX_TX_WR_DUALCK(1U)

#define S_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT 11
#define M_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT 0x1
#define V_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT)
#define G_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT) & \
     M_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT)
#define F_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT \
    V_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT(1U)

#define S_TLS_KEYCTX_TX_WR_SALT_PRESENT 10
#define M_TLS_KEYCTX_TX_WR_SALT_PRESENT 0x1
#define V_TLS_KEYCTX_TX_WR_SALT_PRESENT(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_SALT_PRESENT)
#define G_TLS_KEYCTX_TX_WR_SALT_PRESENT(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_SALT_PRESENT) & \
     M_TLS_KEYCTX_TX_WR_SALT_PRESENT)
#define F_TLS_KEYCTX_TX_WR_SALT_PRESENT \
    V_TLS_KEYCTX_TX_WR_SALT_PRESENT(1U)

#define S_TLS_KEYCTX_TX_WR_TXCK_SIZE 6
#define M_TLS_KEYCTX_TX_WR_TXCK_SIZE 0xf
#define V_TLS_KEYCTX_TX_WR_TXCK_SIZE(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_TXCK_SIZE)
#define G_TLS_KEYCTX_TX_WR_TXCK_SIZE(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_TXCK_SIZE) & \
     M_TLS_KEYCTX_TX_WR_TXCK_SIZE)

#define S_TLS_KEYCTX_TX_WR_TXMK_SIZE 2
#define M_TLS_KEYCTX_TX_WR_TXMK_SIZE 0xf
#define V_TLS_KEYCTX_TX_WR_TXMK_SIZE(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_TXMK_SIZE)
#define G_TLS_KEYCTX_TX_WR_TXMK_SIZE(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_TXMK_SIZE) & \
     M_TLS_KEYCTX_TX_WR_TXMK_SIZE)

#define S_TLS_KEYCTX_TX_WR_TXVALID   0
#define M_TLS_KEYCTX_TX_WR_TXVALID   0x1
#define V_TLS_KEYCTX_TX_WR_TXVALID(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_TXVALID)
#define G_TLS_KEYCTX_TX_WR_TXVALID(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_TXVALID) & M_TLS_KEYCTX_TX_WR_TXVALID)
#define F_TLS_KEYCTX_TX_WR_TXVALID   V_TLS_KEYCTX_TX_WR_TXVALID(1U)

#define S_TLS_KEYCTX_TX_WR_FLITCNT   3
#define M_TLS_KEYCTX_TX_WR_FLITCNT   0x1f
#define V_TLS_KEYCTX_TX_WR_FLITCNT(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_FLITCNT)
#define G_TLS_KEYCTX_TX_WR_FLITCNT(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_FLITCNT) & M_TLS_KEYCTX_TX_WR_FLITCNT)

#define S_TLS_KEYCTX_TX_WR_HMACCTRL  0
#define M_TLS_KEYCTX_TX_WR_HMACCTRL  0x7
#define V_TLS_KEYCTX_TX_WR_HMACCTRL(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_HMACCTRL)
#define G_TLS_KEYCTX_TX_WR_HMACCTRL(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_HMACCTRL) & M_TLS_KEYCTX_TX_WR_HMACCTRL)

#define S_TLS_KEYCTX_TX_WR_PROTOVER  4
#define M_TLS_KEYCTX_TX_WR_PROTOVER  0xf
#define V_TLS_KEYCTX_TX_WR_PROTOVER(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_PROTOVER)
#define G_TLS_KEYCTX_TX_WR_PROTOVER(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_PROTOVER) & M_TLS_KEYCTX_TX_WR_PROTOVER)

#define S_TLS_KEYCTX_TX_WR_CIPHMODE  0
#define M_TLS_KEYCTX_TX_WR_CIPHMODE  0xf
#define V_TLS_KEYCTX_TX_WR_CIPHMODE(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_CIPHMODE)
#define G_TLS_KEYCTX_TX_WR_CIPHMODE(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_CIPHMODE) & M_TLS_KEYCTX_TX_WR_CIPHMODE)

#define S_TLS_KEYCTX_TX_WR_AUTHMODE  4
#define M_TLS_KEYCTX_TX_WR_AUTHMODE  0xf
#define V_TLS_KEYCTX_TX_WR_AUTHMODE(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_AUTHMODE)
#define G_TLS_KEYCTX_TX_WR_AUTHMODE(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_AUTHMODE) & M_TLS_KEYCTX_TX_WR_AUTHMODE)

#define S_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL 3
#define M_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL 0x1
#define V_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL)
#define G_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL) & \
     M_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL)
#define F_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL \
    V_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL(1U)

#define S_TLS_KEYCTX_TX_WR_SEQNUMCTRL 1
#define M_TLS_KEYCTX_TX_WR_SEQNUMCTRL 0x3
#define V_TLS_KEYCTX_TX_WR_SEQNUMCTRL(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_SEQNUMCTRL)
#define G_TLS_KEYCTX_TX_WR_SEQNUMCTRL(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_SEQNUMCTRL) & \
     M_TLS_KEYCTX_TX_WR_SEQNUMCTRL)

#define S_TLS_KEYCTX_TX_WR_RXVALID   0
#define M_TLS_KEYCTX_TX_WR_RXVALID   0x1
#define V_TLS_KEYCTX_TX_WR_RXVALID(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_RXVALID)
#define G_TLS_KEYCTX_TX_WR_RXVALID(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_RXVALID) & M_TLS_KEYCTX_TX_WR_RXVALID)
#define F_TLS_KEYCTX_TX_WR_RXVALID   V_TLS_KEYCTX_TX_WR_RXVALID(1U)

#define S_TLS_KEYCTX_TX_WR_IVPRESENT 7
#define M_TLS_KEYCTX_TX_WR_IVPRESENT 0x1
#define V_TLS_KEYCTX_TX_WR_IVPRESENT(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_IVPRESENT)
#define G_TLS_KEYCTX_TX_WR_IVPRESENT(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_IVPRESENT) & \
     M_TLS_KEYCTX_TX_WR_IVPRESENT)
#define F_TLS_KEYCTX_TX_WR_IVPRESENT V_TLS_KEYCTX_TX_WR_IVPRESENT(1U)

#define S_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT 6
#define M_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT 0x1
#define V_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT)
#define G_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT) & \
     M_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT)
#define F_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT \
    V_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT(1U)

#define S_TLS_KEYCTX_TX_WR_RXCK_SIZE 3
#define M_TLS_KEYCTX_TX_WR_RXCK_SIZE 0x7
#define V_TLS_KEYCTX_TX_WR_RXCK_SIZE(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_RXCK_SIZE)
#define G_TLS_KEYCTX_TX_WR_RXCK_SIZE(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_RXCK_SIZE) & \
     M_TLS_KEYCTX_TX_WR_RXCK_SIZE)

#define S_TLS_KEYCTX_TX_WR_RXMK_SIZE 0
#define M_TLS_KEYCTX_TX_WR_RXMK_SIZE 0x7
#define V_TLS_KEYCTX_TX_WR_RXMK_SIZE(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_RXMK_SIZE)
#define G_TLS_KEYCTX_TX_WR_RXMK_SIZE(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_RXMK_SIZE) & \
     M_TLS_KEYCTX_TX_WR_RXMK_SIZE)

#define S_TLS_KEYCTX_TX_WR_IVINSERT  55
#define M_TLS_KEYCTX_TX_WR_IVINSERT  0x1ffULL
#define V_TLS_KEYCTX_TX_WR_IVINSERT(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_IVINSERT)
#define G_TLS_KEYCTX_TX_WR_IVINSERT(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_IVINSERT) & M_TLS_KEYCTX_TX_WR_IVINSERT)

#define S_TLS_KEYCTX_TX_WR_AADSTRTOFST 47
#define M_TLS_KEYCTX_TX_WR_AADSTRTOFST 0xffULL
#define V_TLS_KEYCTX_TX_WR_AADSTRTOFST(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_AADSTRTOFST)
#define G_TLS_KEYCTX_TX_WR_AADSTRTOFST(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_AADSTRTOFST) & \
     M_TLS_KEYCTX_TX_WR_AADSTRTOFST)

#define S_TLS_KEYCTX_TX_WR_AADSTOPOFST 39
#define M_TLS_KEYCTX_TX_WR_AADSTOPOFST 0xffULL
#define V_TLS_KEYCTX_TX_WR_AADSTOPOFST(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_AADSTOPOFST)
#define G_TLS_KEYCTX_TX_WR_AADSTOPOFST(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_AADSTOPOFST) & \
     M_TLS_KEYCTX_TX_WR_AADSTOPOFST)

#define S_TLS_KEYCTX_TX_WR_CIPHERSRTOFST 30
#define M_TLS_KEYCTX_TX_WR_CIPHERSRTOFST 0x1ffULL
#define V_TLS_KEYCTX_TX_WR_CIPHERSRTOFST(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_CIPHERSRTOFST)
#define G_TLS_KEYCTX_TX_WR_CIPHERSRTOFST(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_CIPHERSRTOFST) & \
     M_TLS_KEYCTX_TX_WR_CIPHERSRTOFST)

#define S_TLS_KEYCTX_TX_WR_CIPHERSTOPOFST 23
#define M_TLS_KEYCTX_TX_WR_CIPHERSTOPOFST 0x7f
#define V_TLS_KEYCTX_TX_WR_CIPHERSTOPOFST(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_CIPHERSTOPOFST)
#define G_TLS_KEYCTX_TX_WR_CIPHERSTOPOFST(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_CIPHERSTOPOFST) & \
     M_TLS_KEYCTX_TX_WR_CIPHERSTOPOFST)

#define S_TLS_KEYCTX_TX_WR_AUTHSRTOFST 14
#define M_TLS_KEYCTX_TX_WR_AUTHSRTOFST 0x1ff
#define V_TLS_KEYCTX_TX_WR_AUTHSRTOFST(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_AUTHSRTOFST)
#define G_TLS_KEYCTX_TX_WR_AUTHSRTOFST(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_AUTHSRTOFST) & \
     M_TLS_KEYCTX_TX_WR_AUTHSRTOFST)

#define S_TLS_KEYCTX_TX_WR_AUTHSTOPOFST 7
#define M_TLS_KEYCTX_TX_WR_AUTHSTOPOFST 0x7f
#define V_TLS_KEYCTX_TX_WR_AUTHSTOPOFST(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_AUTHSTOPOFST)
#define G_TLS_KEYCTX_TX_WR_AUTHSTOPOFST(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_AUTHSTOPOFST) & \
     M_TLS_KEYCTX_TX_WR_AUTHSTOPOFST)

#define S_TLS_KEYCTX_TX_WR_AUTHINSRT 0
#define M_TLS_KEYCTX_TX_WR_AUTHINSRT 0x7f
#define V_TLS_KEYCTX_TX_WR_AUTHINSRT(x) \
    ((x) << S_TLS_KEYCTX_TX_WR_AUTHINSRT)
#define G_TLS_KEYCTX_TX_WR_AUTHINSRT(x) \
    (((x) >> S_TLS_KEYCTX_TX_WR_AUTHINSRT) & \
     M_TLS_KEYCTX_TX_WR_AUTHINSRT)

/* Key Context Programming Operation type */
#define KEY_WRITE_RX			0x1
#define KEY_WRITE_TX			0x2
#define KEY_DELETE_RX			0x4
#define KEY_DELETE_TX			0x8

#define S_KEY_CLR_LOC		4
#define M_KEY_CLR_LOC		0xf
#define V_KEY_CLR_LOC(x)	((x) << S_KEY_CLR_LOC)
#define G_KEY_CLR_LOC(x)	(((x) >> S_KEY_CLR_LOC) & M_KEY_CLR_LOC)
#define F_KEY_CLR_LOC		V_KEY_CLR_LOC(1U)

#define S_KEY_GET_LOC           0
#define M_KEY_GET_LOC           0xf
#define V_KEY_GET_LOC(x)        ((x) << S_KEY_GET_LOC)
#define G_KEY_GET_LOC(x)        (((x) >> S_KEY_GET_LOC) & M_KEY_GET_LOC)

/*
 * Generate the initial GMAC hash state for a AES-GCM key.
 *
 * Borrowed from AES_GMAC_Setkey().
 */
void
t4_init_gmac_hash(const char *key, int klen, char *ghash)
{
	static char zeroes[GMAC_BLOCK_LEN];
	uint32_t keysched[4 * (RIJNDAEL_MAXNR + 1)];
	int rounds;

	rounds = rijndaelKeySetupEnc(keysched, key, klen * 8);
	rijndaelEncrypt(keysched, rounds, zeroes, ghash);
	explicit_bzero(keysched, sizeof(keysched));
}

/* Copy out the partial hash state from a software hash implementation. */
void
t4_copy_partial_hash(int alg, union authctx *auth_ctx, void *dst)
{
	uint32_t *u32;
	uint64_t *u64;
	u_int i;

	u32 = (uint32_t *)dst;
	u64 = (uint64_t *)dst;
	switch (alg) {
	case CRYPTO_SHA1:
	case CRYPTO_SHA1_HMAC:
		for (i = 0; i < SHA1_HASH_LEN / 4; i++)
			u32[i] = htobe32(auth_ctx->sha1ctx.h.b32[i]);
		break;
	case CRYPTO_SHA2_224:
	case CRYPTO_SHA2_224_HMAC:
		for (i = 0; i < SHA2_256_HASH_LEN / 4; i++)
			u32[i] = htobe32(auth_ctx->sha224ctx.state[i]);
		break;
	case CRYPTO_SHA2_256:
	case CRYPTO_SHA2_256_HMAC:
		for (i = 0; i < SHA2_256_HASH_LEN / 4; i++)
			u32[i] = htobe32(auth_ctx->sha256ctx.state[i]);
		break;
	case CRYPTO_SHA2_384:
	case CRYPTO_SHA2_384_HMAC:
		for (i = 0; i < SHA2_512_HASH_LEN / 8; i++)
			u64[i] = htobe64(auth_ctx->sha384ctx.state[i]);
		break;
	case CRYPTO_SHA2_512:
	case CRYPTO_SHA2_512_HMAC:
		for (i = 0; i < SHA2_512_HASH_LEN / 8; i++)
			u64[i] = htobe64(auth_ctx->sha512ctx.state[i]);
		break;
	}
}

void
t4_init_hmac_digest(const struct auth_hash *axf, u_int partial_digest_len,
    const char *key, int klen, char *dst)
{
	union authctx auth_ctx;

	hmac_init_ipad(axf, key, klen, &auth_ctx);
	t4_copy_partial_hash(axf->type, &auth_ctx, dst);

	dst += roundup2(partial_digest_len, 16);

	hmac_init_opad(axf, key, klen, &auth_ctx);
	t4_copy_partial_hash(axf->type, &auth_ctx, dst);

	explicit_bzero(&auth_ctx, sizeof(auth_ctx));
}

/*
 * Borrowed from cesa_prep_aes_key().
 *
 * NB: The crypto engine wants the words in the decryption key in reverse
 * order.
 */
void
t4_aes_getdeckey(void *dec_key, const void *enc_key, unsigned int kbits)
{
	uint32_t ek[4 * (RIJNDAEL_MAXNR + 1)];
	uint32_t *dkey;
	int i;

	rijndaelKeySetupEnc(ek, enc_key, kbits);
	dkey = dec_key;
	dkey += (kbits / 8) / 4;

	switch (kbits) {
	case 128:
		for (i = 0; i < 4; i++)
			*--dkey = htobe32(ek[4 * 10 + i]);
		break;
	case 192:
		for (i = 0; i < 2; i++)
			*--dkey = htobe32(ek[4 * 11 + 2 + i]);
		for (i = 0; i < 4; i++)
			*--dkey = htobe32(ek[4 * 12 + i]);
		break;
	case 256:
		for (i = 0; i < 4; i++)
			*--dkey = htobe32(ek[4 * 13 + i]);
		for (i = 0; i < 4; i++)
			*--dkey = htobe32(ek[4 * 14 + i]);
		break;
	}
	MPASS(dkey == dec_key);
	explicit_bzero(ek, sizeof(ek));
}

#ifdef KERN_TLS
/*
 * - keyid management
 * - request to program key?
 */
u_int
t4_tls_key_info_size(const struct ktls_session *tls)
{
	u_int key_info_size, mac_key_size;

	key_info_size = sizeof(struct tx_keyctx_hdr) +
	    tls->params.cipher_key_len;
	if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16) {
		key_info_size += GMAC_BLOCK_LEN;
	} else {
		switch (tls->params.auth_algorithm) {
		case CRYPTO_SHA1_HMAC:
			mac_key_size = SHA1_HASH_LEN;
			break;
		case CRYPTO_SHA2_256_HMAC:
			mac_key_size = SHA2_256_HASH_LEN;
			break;
		case CRYPTO_SHA2_384_HMAC:
			mac_key_size = SHA2_512_HASH_LEN;
			break;
		default:
			__assert_unreachable();
		}
		key_info_size += roundup2(mac_key_size, 16) * 2;
	}
	return (key_info_size);
}

int
t4_tls_proto_ver(const struct ktls_session *tls)
{
	if (tls->params.tls_vminor == TLS_MINOR_VER_ONE)
		return (SCMD_PROTO_VERSION_TLS_1_1);
	else
		return (SCMD_PROTO_VERSION_TLS_1_2);
}

int
t4_tls_cipher_mode(const struct ktls_session *tls)
{
	switch (tls->params.cipher_algorithm) {
	case CRYPTO_AES_CBC:
		return (SCMD_CIPH_MODE_AES_CBC);
	case CRYPTO_AES_NIST_GCM_16:
		return (SCMD_CIPH_MODE_AES_GCM);
	default:
		return (SCMD_CIPH_MODE_NOP);
	}
}

int
t4_tls_auth_mode(const struct ktls_session *tls)
{
	switch (tls->params.cipher_algorithm) {
	case CRYPTO_AES_CBC:
		switch (tls->params.auth_algorithm) {
		case CRYPTO_SHA1_HMAC:
			return (SCMD_AUTH_MODE_SHA1);
		case CRYPTO_SHA2_256_HMAC:
			return (SCMD_AUTH_MODE_SHA256);
		case CRYPTO_SHA2_384_HMAC:
			return (SCMD_AUTH_MODE_SHA512_384);
		default:
			return (SCMD_AUTH_MODE_NOP);
		}
	case CRYPTO_AES_NIST_GCM_16:
		return (SCMD_AUTH_MODE_GHASH);
	default:
		return (SCMD_AUTH_MODE_NOP);
	}
}

int
t4_tls_hmac_ctrl(const struct ktls_session *tls)
{
	switch (tls->params.cipher_algorithm) {
	case CRYPTO_AES_CBC:
		return (SCMD_HMAC_CTRL_NO_TRUNC);
	case CRYPTO_AES_NIST_GCM_16:
		return (SCMD_HMAC_CTRL_NOP);
	default:
		return (SCMD_HMAC_CTRL_NOP);
	}
}

static int
tls_cipher_key_size(const struct ktls_session *tls)
{
	switch (tls->params.cipher_key_len) {
	case 128 / 8:
		return (CHCR_KEYCTX_CIPHER_KEY_SIZE_128);
	case 192 / 8:
		return (CHCR_KEYCTX_CIPHER_KEY_SIZE_192);
	case 256 / 8:
		return (CHCR_KEYCTX_CIPHER_KEY_SIZE_256);
	default:
		__assert_unreachable();
	}
}

static int
tls_mac_key_size(const struct ktls_session *tls)
{
	if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16)
		return (CHCR_KEYCTX_MAC_KEY_SIZE_512);
	else {
		switch (tls->params.auth_algorithm) {
		case CRYPTO_SHA1_HMAC:
			return (CHCR_KEYCTX_MAC_KEY_SIZE_160);
		case CRYPTO_SHA2_256_HMAC:
			return (CHCR_KEYCTX_MAC_KEY_SIZE_256);
		case CRYPTO_SHA2_384_HMAC:
			return (CHCR_KEYCTX_MAC_KEY_SIZE_512);
		default:
			__assert_unreachable();
		}
	}
}

void
t4_tls_key_ctx(const struct ktls_session *tls, int direction,
    struct tls_keyctx *kctx)
{
	const struct auth_hash *axf;
	u_int mac_key_size;
	char *hash;

	/* Key context header. */
	if (direction == KTLS_TX) {
		kctx->u.txhdr.ctxlen = t4_tls_key_info_size(tls) / 16;
		kctx->u.txhdr.dualck_to_txvalid =
		    V_TLS_KEYCTX_TX_WR_SALT_PRESENT(1) |
		    V_TLS_KEYCTX_TX_WR_TXCK_SIZE(tls_cipher_key_size(tls)) |
		    V_TLS_KEYCTX_TX_WR_TXMK_SIZE(tls_mac_key_size(tls)) |
		    V_TLS_KEYCTX_TX_WR_TXVALID(1);
		if (tls->params.cipher_algorithm == CRYPTO_AES_CBC)
			kctx->u.txhdr.dualck_to_txvalid |=
			    V_TLS_KEYCTX_TX_WR_TXOPAD_PRESENT(1);
		kctx->u.txhdr.dualck_to_txvalid =
		    htobe16(kctx->u.txhdr.dualck_to_txvalid);
	} else {
		kctx->u.rxhdr.flitcnt_hmacctrl =
		    V_TLS_KEYCTX_TX_WR_FLITCNT(t4_tls_key_info_size(tls) / 16) |
		    V_TLS_KEYCTX_TX_WR_HMACCTRL(t4_tls_hmac_ctrl(tls));

		kctx->u.rxhdr.protover_ciphmode =
		    V_TLS_KEYCTX_TX_WR_PROTOVER(t4_tls_proto_ver(tls)) |
		    V_TLS_KEYCTX_TX_WR_CIPHMODE(t4_tls_cipher_mode(tls));

		kctx->u.rxhdr.authmode_to_rxvalid =
		    V_TLS_KEYCTX_TX_WR_AUTHMODE(t4_tls_auth_mode(tls)) |
		    V_TLS_KEYCTX_TX_WR_SEQNUMCTRL(3) |
		    V_TLS_KEYCTX_TX_WR_RXVALID(1);

		kctx->u.rxhdr.ivpresent_to_rxmk_size =
		    V_TLS_KEYCTX_TX_WR_IVPRESENT(0) |
		    V_TLS_KEYCTX_TX_WR_RXCK_SIZE(tls_cipher_key_size(tls)) |
		    V_TLS_KEYCTX_TX_WR_RXMK_SIZE(tls_mac_key_size(tls));

		if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16) {
			kctx->u.rxhdr.ivinsert_to_authinsrt =
			    htobe64(V_TLS_KEYCTX_TX_WR_IVINSERT(6ULL) |
				V_TLS_KEYCTX_TX_WR_AADSTRTOFST(1ULL) |
				V_TLS_KEYCTX_TX_WR_AADSTOPOFST(5ULL) |
				V_TLS_KEYCTX_TX_WR_AUTHSRTOFST(14ULL) |
				V_TLS_KEYCTX_TX_WR_AUTHSTOPOFST(16ULL) |
				V_TLS_KEYCTX_TX_WR_CIPHERSRTOFST(14ULL) |
				V_TLS_KEYCTX_TX_WR_CIPHERSTOPOFST(0ULL) |
				V_TLS_KEYCTX_TX_WR_AUTHINSRT(16ULL));
		} else {
			kctx->u.rxhdr.authmode_to_rxvalid |=
			    V_TLS_KEYCTX_TX_WR_CIPHAUTHSEQCTRL(1);
			kctx->u.rxhdr.ivpresent_to_rxmk_size |=
			    V_TLS_KEYCTX_TX_WR_RXOPAD_PRESENT(1);
			kctx->u.rxhdr.ivinsert_to_authinsrt =
			    htobe64(V_TLS_KEYCTX_TX_WR_IVINSERT(6ULL) |
				V_TLS_KEYCTX_TX_WR_AADSTRTOFST(1ULL) |
				V_TLS_KEYCTX_TX_WR_AADSTOPOFST(5ULL) |
				V_TLS_KEYCTX_TX_WR_AUTHSRTOFST(22ULL) |
				V_TLS_KEYCTX_TX_WR_AUTHSTOPOFST(0ULL) |
				V_TLS_KEYCTX_TX_WR_CIPHERSRTOFST(22ULL) |
				V_TLS_KEYCTX_TX_WR_CIPHERSTOPOFST(0ULL) |
				V_TLS_KEYCTX_TX_WR_AUTHINSRT(0ULL));
		}
	}

	/* Key. */
	if (direction == KTLS_RX &&
	    tls->params.cipher_algorithm == CRYPTO_AES_CBC)
		t4_aes_getdeckey(kctx->keys.edkey, tls->params.cipher_key,
		    tls->params.cipher_key_len * 8);
	else
		memcpy(kctx->keys.edkey, tls->params.cipher_key,
		    tls->params.cipher_key_len);

	/* Auth state and implicit IV (salt). */
	hash = kctx->keys.edkey + tls->params.cipher_key_len;
	if (tls->params.cipher_algorithm == CRYPTO_AES_NIST_GCM_16) {
		_Static_assert(offsetof(struct tx_keyctx_hdr, txsalt) ==
		    offsetof(struct rx_keyctx_hdr, rxsalt),
		    "salt offset mismatch");
		memcpy(kctx->u.txhdr.txsalt, tls->params.iv, SALT_SIZE);
		t4_init_gmac_hash(tls->params.cipher_key,
		    tls->params.cipher_key_len, hash);
	} else {
		switch (tls->params.auth_algorithm) {
		case CRYPTO_SHA1_HMAC:
			axf = &auth_hash_hmac_sha1;
			mac_key_size = SHA1_HASH_LEN;
			break;
		case CRYPTO_SHA2_256_HMAC:
			axf = &auth_hash_hmac_sha2_256;
			mac_key_size = SHA2_256_HASH_LEN;
			break;
		case CRYPTO_SHA2_384_HMAC:
			axf = &auth_hash_hmac_sha2_384;
			mac_key_size = SHA2_512_HASH_LEN;
			break;
		default:
			__assert_unreachable();
		}
		t4_init_hmac_digest(axf, mac_key_size, tls->params.auth_key,
		    tls->params.auth_key_len, hash);
	}
}

int
t4_alloc_tls_keyid(struct adapter *sc)
{
	vmem_addr_t addr;

	if (sc->vres.key.size == 0)
		return (-1);

	if (vmem_alloc(sc->key_map, TLS_KEY_CONTEXT_SZ, M_NOWAIT | M_FIRSTFIT,
	    &addr) != 0)
		return (-1);

	return (addr);
}

void
t4_free_tls_keyid(struct adapter *sc, int keyid)
{
	vmem_free(sc->key_map, keyid, TLS_KEY_CONTEXT_SZ);
}

void
t4_write_tlskey_wr(const struct ktls_session *tls, int direction, int tid,
    int flags, int keyid, struct tls_key_req *kwr)
{
	kwr->wr_hi = htobe32(V_FW_WR_OP(FW_ULPTX_WR) | F_FW_WR_ATOMIC | flags);
	kwr->wr_mid = htobe32(V_FW_WR_LEN16(DIV_ROUND_UP(TLS_KEY_WR_SZ, 16)) |
	    V_FW_WR_FLOWID(tid));
	kwr->protocol = t4_tls_proto_ver(tls);
	kwr->mfs = htobe16(tls->params.max_frame_len);
	kwr->reneg_to_write_rx = V_KEY_GET_LOC(direction == KTLS_TX ?
	    KEY_WRITE_TX : KEY_WRITE_RX);

	/* master command */
	kwr->cmd = htobe32(V_ULPTX_CMD(ULP_TX_MEM_WRITE) |
	    V_T5_ULP_MEMIO_ORDER(1) | V_T5_ULP_MEMIO_IMM(1));
	kwr->dlen = htobe32(V_ULP_MEMIO_DATA_LEN(TLS_KEY_CONTEXT_SZ >> 5));
	kwr->len16 = htobe32((tid << 8) |
	    DIV_ROUND_UP(TLS_KEY_WR_SZ - sizeof(struct work_request_hdr), 16));
	kwr->kaddr = htobe32(V_ULP_MEMIO_ADDR(keyid >> 5));

	/* sub command */
	kwr->sc_more = htobe32(V_ULPTX_CMD(ULP_TX_SC_IMM));
	kwr->sc_len = htobe32(TLS_KEY_CONTEXT_SZ);
}
#endif
