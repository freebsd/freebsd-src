/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Nicolas Provost <dev@npsoft.fr>
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

#ifndef _SYS_GEOM_LUKS_H_
#define _SYS_GEOM_LUKS_H_ 1

#include <sys/types.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/libkern.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bio.h>
#include <geom/geom.h>
#include <geom/geom_dbg.h>
#include <opencrypto/cryptodev.h>
#include <opencrypto/rmd160.h>
#include <crypto/sha1.h>
#include <crypto/sha2/sha256.h>
#include <crypto/sha2/sha512.h>
#include <crypto/rijndael/rijndael-api-fst.h>

MALLOC_DECLARE(M_LUKS);

#define G_LUKS_MAGIC 		"LUKS\xBA\xBE"
#define G_LUKS_MAX_SLOTS	8
#define G_LUKS_DIGEST_LEN	20
#define G_LUKS_DIGEST_OFFSET	112
#define G_LUKS_SALT_LEN		32
#define G_LUKS_SALT_OFFSET	132
#define G_LUKS_UUID_LEN		40
#define G_LUKS_STRIPES		4000
#define G_LUKS_SLOT_ALIGN	4096
#define G_LUKS_SECTOR_LEN	512
#define G_LUKS_MAX_KEY_LEN	64
#define G_LUKS_PHDR_LEN		(6+2+32+32+32+4+4+20+32+4+40+(48*8))
#define G_LUKS_MIN_ITER		1000

#define G_LUKS_DG_MAX_LEN	SHA2_512_HASH_LEN
#define G_LUKS_DG_BLOCK_MAX_LEN	SHA2_512_BLOCK_LEN

typedef enum {
	G_LUKS_HASH_UNKNOWN = 0,
	G_LUKS_HASH_SHA1=CRYPTO_SHA1,
	G_LUKS_HASH_SHA256=CRYPTO_SHA2_256,
	G_LUKS_HASH_SHA512=CRYPTO_SHA2_512,
	G_LUKS_HASH_RMD160=CRYPTO_RIPEMD160,
} g_luks_hash;

typedef enum {
	G_LUKS_COP_ENCRYPT = 1,
	G_LUKS_COP_DECRYPT = 2
} g_luks_cop;

typedef enum {
	G_LUKS_CIPHER_UNKNOWN,
	G_LUKS_CIPHER_AES,
} g_luks_cipher;

typedef enum {
	G_LUKS_MODE_UNKNOWN = 0,
	G_LUKS_MODE_ECB,
	G_LUKS_MODE_CBC_PLAIN,
	G_LUKS_MODE_CBC_ESSIV_SHA256,
	G_LUKS_MODE_XTS_PLAIN64,
} g_luks_mode;

struct g_luks_digest_ctx {
	g_luks_hash	alg;
	size_t		output_len;
	size_t		block_len;
	uint8_t		digest[G_LUKS_DG_MAX_LEN];
	union {
		RMD160_CTX rmd160;
		SHA1_CTX sha1;
		SHA256_CTX sha256;
		SHA512_CTX sha512;
	} ctx;
};

struct g_luks_hmac_ctx {
	struct g_luks_digest_ctx	inner;
	struct g_luks_digest_ctx	outer;
};

struct g_luks_cipher_ctx {
	g_luks_cipher			alg;
	g_luks_mode			mode;
	g_luks_cop			cop;
	cipherInstance			ci[2];
	keyInstance			ki[2];
	uint8_t				iv[64];
};

#define G_LUKS_SLOT_ENABLED	0x00ac71f3
#define G_LUKS_SLOT_DISABLED	0x0000dead

/* LUKS key slot descriptor */
struct g_luks_slot {
	uint32_t	active;
	uint32_t	iter;
	uint8_t		salt[G_LUKS_SALT_LEN];
	size_t		salt_len;
	uint32_t	s_offset;
	uint32_t	stripes;
};

/* LUKS phdr data */
struct g_luks_metadata {
	uint16_t		version;
	g_luks_cipher		cipher;
	g_luks_mode		mode;
	g_luks_hash		hash;
	size_t			hash_len;
	size_t			sector_len;
	uint32_t		s_payload;
	off_t			payload_offset;
	uint32_t		mk_len;
	uint8_t			mk_digest[G_LUKS_DIGEST_LEN];
	size_t			mk_digest_len;
	uint8_t			mk_digest_salt[G_LUKS_SALT_LEN];
	size_t			mk_digest_salt_len;
	uint32_t		mk_digest_iter;
	char			uuid[G_LUKS_UUID_LEN];
	struct g_luks_slot	slot[G_LUKS_MAX_SLOTS];
};

typedef enum {
	G_LUKS_STATE_RUN = 1,
	G_LUKS_STATE_OPEN = 2,
	G_LUKS_STATE_FORMAT = 4,
	G_LUKS_STATE_INFO = 8,
	G_LUKS_STATE_RO = 0x10,
	G_LUKS_STATE_ONEWR = 0x20,
	G_LUKS_STATE_STOP = 0x40,
	G_LUKS_STATE_ALLOCATED = 0x80,
} g_luks_state;

struct g_luks_softc {
	g_luks_state		state;
	int			writers;
	size_t			mediasize;
	size_t			sectorsize;
	struct g_geom		*geom;
	struct g_consumer	*consumer;
	struct proc		*worker;
	struct mtx		queue_mtx;
	struct bio_queue_head	queue;	
	struct g_luks_cipher_ctx rctx;
	struct g_luks_cipher_ctx wctx;
	struct g_luks_metadata	meta;
	uint8_t			*pbkpass;
	size_t			pbkpass_len;
	uint8_t			*mk;
};

#define G_LUKS_DEBUG(lvl,formatstr, ...) \
	do { \
		if ((lvl) <= g_luks_dbglvl) \
	g_dbg_printf("GEOM_LUKS", -1, NULL, ": " formatstr, ## __VA_ARGS__); \
	} while(0);

void*
g_luks_malloc(size_t len, int flags);

void
g_luks_mfree(uint8_t **p, size_t len);

int
g_luks_cipher_init(struct g_luks_cipher_ctx *ctx, g_luks_cop op,
			g_luks_cipher alg, g_luks_mode mode,
			const uint8_t *key, size_t len);

int
g_luks_cipher_setup_iv(struct g_luks_cipher_ctx *ctx,
			uint64_t iv_source);

int
g_luks_cipher_do_block(struct g_luks_cipher_ctx *ctx, int n, uint8_t *in,
			uint8_t *out, size_t len);

int
g_luks_cipher_do(struct g_luks_cipher_ctx *ctx, uint8_t *in, size_t len);

int
g_luks_cipher_do_to(struct g_luks_cipher_ctx *ctx, uint8_t *in,
			uint8_t *out, size_t len);

void
g_luks_cipher_clear(struct g_luks_cipher_ctx *ctx);

void
g_luks_digest_clear(struct g_luks_digest_ctx *ctx);

int
g_luks_digest_init(struct g_luks_digest_ctx *ctx, int alg);

int
g_luks_digest_final(struct g_luks_digest_ctx *ctx, uint8_t* dest);

int
g_luks_digest_update(struct g_luks_digest_ctx *ctx,
			const uint8_t *data, size_t len);

int
g_luks_digest_output_len(int hash_alg);

g_luks_hash
g_luks_digest_from_str(const char *s, size_t len);

int
g_luks_hmac(int hash_alg,
		const uint8_t *hkey, size_t hkeysize,
		uint8_t *data, size_t datasize,
		uint8_t *md, size_t mdsize);

void
g_luks_hmac_final(struct g_luks_hmac_ctx *ctx,
			uint8_t *md, size_t mdsize);

void
g_luks_hmac_update(struct g_luks_hmac_ctx *ctx,
			uint8_t *data, size_t datasize);

int
g_luks_hmac_init(struct g_luks_hmac_ctx *ctx, int hash_alg,
		const uint8_t *hkey, size_t hkeylen);

int
g_luks_pbkdf2(const uint8_t *passphrase, size_t pass_len,
		uint8_t *iv, size_t iv_len,
		uint8_t *buf, size_t buf_len,
		int iterations, int hash_alg);

int
g_luks_af_merge(int hash_alg, int n_stripes,
			uint8_t *material, size_t mat_len,
			uint8_t *out, size_t out_len);

int
g_luks_af_split(int hash_alg, int n_stripes,
			uint8_t *material, size_t mat_len,
			uint8_t *out, size_t out_len);

#endif /* _SYS_GEOM_G_LUKS_H_ */
