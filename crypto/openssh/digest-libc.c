/* $OpenBSD: digest-libc.c,v 1.2 2014/02/02 03:44:31 djm Exp $ */
/*
 * Copyright (c) 2013 Damien Miller <djm@mindrot.org>
 * Copyright (c) 2014 Markus Friedl.  All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"
__RCSID("$FreeBSD$");

#include <sys/types.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <md5.h>
#include <rmd160.h>
#include <sha1.h>
#include <sha2.h>

#include "buffer.h"
#include "digest.h"

typedef void md_init_fn(void *mdctx);
typedef void md_update_fn(void *mdctx, const u_int8_t *m, size_t mlen);
typedef void md_final_fn(u_int8_t[], void *mdctx);

struct ssh_digest_ctx {
	int alg;
	void *mdctx;
};

struct ssh_digest {
	int id;
	const char *name;
	size_t block_len;
	size_t digest_len;
	size_t ctx_len;
	md_init_fn *md_init;
	md_update_fn *md_update;
	md_final_fn *md_final;
};

/* NB. Indexed directly by algorithm number */
const struct ssh_digest digests[SSH_DIGEST_MAX] = {
	{
		SSH_DIGEST_MD5,
		"MD5",
		MD5_BLOCK_LENGTH,
		MD5_DIGEST_LENGTH,
		sizeof(MD5_CTX),
		(md_init_fn *) MD5Init,
		(md_update_fn *) MD5Update,
		(md_final_fn *) MD5Final
	},
	{
		SSH_DIGEST_RIPEMD160,
		"RIPEMD160",
		RMD160_BLOCK_LENGTH,
		RMD160_DIGEST_LENGTH,
		sizeof(RMD160_CTX),
		(md_init_fn *) RMD160Init,
		(md_update_fn *) RMD160Update,
		(md_final_fn *) RMD160Final
	},
	{
		SSH_DIGEST_SHA1,
		"SHA1",
		SHA1_BLOCK_LENGTH,
		SHA1_DIGEST_LENGTH,
		sizeof(SHA1_CTX),
		(md_init_fn *) SHA1Init,
		(md_update_fn *) SHA1Update,
		(md_final_fn *) SHA1Final
	},
	{
		SSH_DIGEST_SHA256,
		"SHA256",
		SHA256_BLOCK_LENGTH,
		SHA256_DIGEST_LENGTH,
		sizeof(SHA2_CTX),
		(md_init_fn *) SHA256Init,
		(md_update_fn *) SHA256Update,
		(md_final_fn *) SHA256Final
	},
	{
		SSH_DIGEST_SHA384,
		"SHA384",
		SHA384_BLOCK_LENGTH,
		SHA384_DIGEST_LENGTH,
		sizeof(SHA2_CTX),
		(md_init_fn *) SHA384Init,
		(md_update_fn *) SHA384Update,
		(md_final_fn *) SHA384Final
	},
	{
		SSH_DIGEST_SHA512,
		"SHA512",
		SHA512_BLOCK_LENGTH,
		SHA512_DIGEST_LENGTH,
		sizeof(SHA2_CTX),
		(md_init_fn *) SHA512Init,
		(md_update_fn *) SHA512Update,
		(md_final_fn *) SHA512Final
	}
};

static const struct ssh_digest *
ssh_digest_by_alg(int alg)
{
	if (alg < 0 || alg >= SSH_DIGEST_MAX)
		return NULL;
	if (digests[alg].id != alg) /* sanity */
		return NULL;
	return &(digests[alg]);
}

size_t
ssh_digest_bytes(int alg)
{
	const struct ssh_digest *digest = ssh_digest_by_alg(alg);

	return digest == NULL ? 0 : digest->digest_len;
}

size_t
ssh_digest_blocksize(struct ssh_digest_ctx *ctx)
{
	const struct ssh_digest *digest = ssh_digest_by_alg(ctx->alg);

	return digest == NULL ? 0 : digest->block_len;
}

struct ssh_digest_ctx *
ssh_digest_start(int alg)
{
	const struct ssh_digest *digest = ssh_digest_by_alg(alg);
	struct ssh_digest_ctx *ret;

	if (digest == NULL || (ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	if ((ret->mdctx = calloc(1, digest->ctx_len)) == NULL) {
		free(ret);
		return NULL;
	}
	ret->alg = alg;
	digest->md_init(ret->mdctx);
	return ret;
}

int
ssh_digest_copy_state(struct ssh_digest_ctx *from, struct ssh_digest_ctx *to)
{
	const struct ssh_digest *digest = ssh_digest_by_alg(from->alg);

	if (digest == NULL || from->alg != to->alg)
		return -1;
	memcpy(to->mdctx, from->mdctx, digest->ctx_len);
	return 0;
}

int
ssh_digest_update(struct ssh_digest_ctx *ctx, const void *m, size_t mlen)
{
	const struct ssh_digest *digest = ssh_digest_by_alg(ctx->alg);

	if (digest == NULL)
		return -1;
	digest->md_update(ctx->mdctx, m, mlen);
	return 0;
}

int
ssh_digest_update_buffer(struct ssh_digest_ctx *ctx, const Buffer *b)
{
	return ssh_digest_update(ctx, buffer_ptr(b), buffer_len(b));
}

int
ssh_digest_final(struct ssh_digest_ctx *ctx, u_char *d, size_t dlen)
{
	const struct ssh_digest *digest = ssh_digest_by_alg(ctx->alg);

	if (digest == NULL)
		return -1;
	if (dlen > UINT_MAX)
		return -1;
	if (dlen < digest->digest_len) /* No truncation allowed */
		return -1;
	digest->md_final(d, ctx->mdctx);
	return 0;
}

void
ssh_digest_free(struct ssh_digest_ctx *ctx)
{
	const struct ssh_digest *digest;

	if (ctx != NULL) {
		digest = ssh_digest_by_alg(ctx->alg);
		if (digest) {
			explicit_bzero(ctx->mdctx, digest->ctx_len);
			free(ctx->mdctx);
			explicit_bzero(ctx, sizeof(*ctx));
			free(ctx);
		}
	}
}

int
ssh_digest_memory(int alg, const void *m, size_t mlen, u_char *d, size_t dlen)
{
	struct ssh_digest_ctx *ctx = ssh_digest_start(alg);

	if (ctx == NULL)
		return -1;
	if (ssh_digest_update(ctx, m, mlen) != 0 ||
	    ssh_digest_final(ctx, d, dlen) != 0)
		return -1;
	ssh_digest_free(ctx);
	return 0;
}

int
ssh_digest_buffer(int alg, const Buffer *b, u_char *d, size_t dlen)
{
	return ssh_digest_memory(alg, buffer_ptr(b), buffer_len(b), d, dlen);
}
