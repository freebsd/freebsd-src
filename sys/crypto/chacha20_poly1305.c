/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * This software was developed by Ararat River Consulting, LLC under
 * sponsorship from the FreeBSD Foundation.
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

#include <sys/endian.h>
#include <crypto/chacha20_poly1305.h>
#include <opencrypto/xform_enc.h>

static const uint8_t zeroes[POLY1305_BLOCK_LEN];

void
chacha20_poly1305_encrypt(uint8_t *dst, const uint8_t *src,
    const size_t src_len, const uint8_t *aad, const size_t aad_len,
    const uint8_t *nonce, const size_t nonce_len, const uint8_t *key)
{
	const struct enc_xform *exf;
	void *ctx;
	size_t resid, todo;
	uint64_t lengths[2];

	exf = &enc_xform_chacha20_poly1305;
	ctx = __builtin_alloca(exf->ctxsize);
	exf->setkey(ctx, key, CHACHA20_POLY1305_KEY);
	exf->reinit(ctx, nonce, nonce_len);

	exf->update(ctx, aad, aad_len);
	if (aad_len % POLY1305_BLOCK_LEN != 0)
		exf->update(ctx, zeroes,
		    POLY1305_BLOCK_LEN - aad_len % POLY1305_BLOCK_LEN);

	resid = src_len;
	todo = rounddown2(resid, CHACHA20_NATIVE_BLOCK_LEN);
	if (todo > 0) {
		exf->encrypt_multi(ctx, src, dst, todo);
		exf->update(ctx, dst, todo);
		src += todo;
		dst += todo;
		resid -= todo;
	}
	if (resid > 0) {
		exf->encrypt_last(ctx, src, dst, resid);
		exf->update(ctx, dst, resid);
		dst += resid;
		if (resid % POLY1305_BLOCK_LEN != 0)
			exf->update(ctx, zeroes,
			    POLY1305_BLOCK_LEN - resid % POLY1305_BLOCK_LEN);
	}

	lengths[0] = htole64(aad_len);
	lengths[1] = htole64(src_len);
	exf->update(ctx, lengths, sizeof(lengths));
	exf->final(dst, ctx);

	explicit_bzero(ctx, exf->ctxsize);
	explicit_bzero(lengths, sizeof(lengths));
}

bool
chacha20_poly1305_decrypt(uint8_t *dst, const uint8_t *src,
    const size_t src_len, const uint8_t *aad, const size_t aad_len,
    const uint8_t *nonce, const size_t nonce_len, const uint8_t *key)
{
	const struct enc_xform *exf;
	void *ctx;
	size_t resid, todo;
	union {
		uint64_t lengths[2];
		char tag[POLY1305_HASH_LEN];
	} u;
	bool result;

	if (src_len < POLY1305_HASH_LEN)
		return (false);
	resid = src_len - POLY1305_HASH_LEN;

	exf = &enc_xform_chacha20_poly1305;
	ctx = __builtin_alloca(exf->ctxsize);
	exf->setkey(ctx, key, CHACHA20_POLY1305_KEY);
	exf->reinit(ctx, nonce, nonce_len);

	exf->update(ctx, aad, aad_len);
	if (aad_len % POLY1305_BLOCK_LEN != 0)
		exf->update(ctx, zeroes,
		    POLY1305_BLOCK_LEN - aad_len % POLY1305_BLOCK_LEN);
	exf->update(ctx, src, resid);
	if (resid % POLY1305_BLOCK_LEN != 0)
		exf->update(ctx, zeroes,
		    POLY1305_BLOCK_LEN - resid % POLY1305_BLOCK_LEN);

	u.lengths[0] = htole64(aad_len);
	u.lengths[1] = htole64(resid);
	exf->update(ctx, u.lengths, sizeof(u.lengths));
	exf->final(u.tag, ctx);
	result = (timingsafe_bcmp(u.tag, src + resid, POLY1305_HASH_LEN) == 0);
	if (!result)
		goto out;

	todo = rounddown2(resid, CHACHA20_NATIVE_BLOCK_LEN);
	if (todo > 0) {
		exf->decrypt_multi(ctx, src, dst, todo);
		src += todo;
		dst += todo;
		resid -= todo;
	}
	if (resid > 0)
		exf->decrypt_last(ctx, src, dst, resid);

out:
	explicit_bzero(ctx, exf->ctxsize);
	explicit_bzero(&u, sizeof(u));
	return (result);
}

void
xchacha20_poly1305_encrypt(uint8_t *dst, const uint8_t *src,
    const size_t src_len, const uint8_t *aad, const size_t aad_len,
    const uint8_t *nonce, const uint8_t *key)
{
	const struct enc_xform *exf;
	void *ctx;
	size_t resid, todo;
	uint64_t lengths[2];

	exf = &enc_xform_xchacha20_poly1305;
	ctx = __builtin_alloca(exf->ctxsize);
	exf->setkey(ctx, key, XCHACHA20_POLY1305_KEY);
	exf->reinit(ctx, nonce, XCHACHA20_POLY1305_IV_LEN);

	exf->update(ctx, aad, aad_len);
	if (aad_len % POLY1305_BLOCK_LEN != 0)
		exf->update(ctx, zeroes,
		    POLY1305_BLOCK_LEN - aad_len % POLY1305_BLOCK_LEN);

	resid = src_len;
	todo = rounddown2(resid, CHACHA20_NATIVE_BLOCK_LEN);
	if (todo > 0) {
		exf->encrypt_multi(ctx, src, dst, todo);
		exf->update(ctx, dst, todo);
		src += todo;
		dst += todo;
		resid -= todo;
	}
	if (resid > 0) {
		exf->encrypt_last(ctx, src, dst, resid);
		exf->update(ctx, dst, resid);
		dst += resid;
		if (resid % POLY1305_BLOCK_LEN != 0)
			exf->update(ctx, zeroes,
			    POLY1305_BLOCK_LEN - resid % POLY1305_BLOCK_LEN);
	}

	lengths[0] = htole64(aad_len);
	lengths[1] = htole64(src_len);
	exf->update(ctx, lengths, sizeof(lengths));
	exf->final(dst, ctx);

	explicit_bzero(ctx, exf->ctxsize);
	explicit_bzero(lengths, sizeof(lengths));
}

bool
xchacha20_poly1305_decrypt(uint8_t *dst, const uint8_t *src,
    const size_t src_len, const uint8_t *aad, const size_t aad_len,
    const uint8_t *nonce, const uint8_t *key)
{
	const struct enc_xform *exf;
	void *ctx;
	size_t resid, todo;
	union {
		uint64_t lengths[2];
		char tag[POLY1305_HASH_LEN];
	} u;
	bool result;

	if (src_len < POLY1305_HASH_LEN)
		return (false);
	resid = src_len - POLY1305_HASH_LEN;

	exf = &enc_xform_xchacha20_poly1305;
	ctx = __builtin_alloca(exf->ctxsize);
	exf->setkey(ctx, key, XCHACHA20_POLY1305_KEY);
	exf->reinit(ctx, nonce, XCHACHA20_POLY1305_IV_LEN);

	exf->update(ctx, aad, aad_len);
	if (aad_len % POLY1305_BLOCK_LEN != 0)
		exf->update(ctx, zeroes,
		    POLY1305_BLOCK_LEN - aad_len % POLY1305_BLOCK_LEN);
	exf->update(ctx, src, resid);
	if (resid % POLY1305_BLOCK_LEN != 0)
		exf->update(ctx, zeroes,
		    POLY1305_BLOCK_LEN - resid % POLY1305_BLOCK_LEN);

	u.lengths[0] = htole64(aad_len);
	u.lengths[1] = htole64(resid);
	exf->update(ctx, u.lengths, sizeof(u.lengths));
	exf->final(u.tag, ctx);
	result = (timingsafe_bcmp(u.tag, src + resid, POLY1305_HASH_LEN) == 0);
	if (!result)
		goto out;

	todo = rounddown2(resid, CHACHA20_NATIVE_BLOCK_LEN);
	if (todo > 0) {
		exf->decrypt_multi(ctx, src, dst, todo);
		src += todo;
		dst += todo;
		resid -= todo;
	}
	if (resid > 0)
		exf->decrypt_last(ctx, src, dst, resid);

out:
	explicit_bzero(ctx, exf->ctxsize);
	explicit_bzero(&u, sizeof(u));
	return (result);
}
