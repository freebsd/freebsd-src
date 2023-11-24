/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Bjoern A. Zeeb
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

#ifndef _LINUXKPI_CRYPTO_HASH_H
#define _LINUXKPI_CRYPTO_HASH_H

#include <linux/kernel.h>	/* for pr_debug */

struct crypto_shash {
};

struct shash_desc {
	struct crypto_shash	*tfm;
};

static inline struct crypto_shash *
crypto_alloc_shash(const char *algostr, int x, int y)
{

	pr_debug("%s: TODO\n", __func__);
	return (NULL);
}

static inline void
crypto_free_shash(struct crypto_shash *csh)
{
	pr_debug("%s: TODO\n", __func__);
}

static inline int
crypto_shash_init(struct shash_desc *desc)
{
	pr_debug("%s: TODO\n", __func__);
	return (-ENXIO);
}

static inline int
crypto_shash_final(struct shash_desc *desc, uint8_t *mic)
{
	pr_debug("%s: TODO\n", __func__);
	return (-ENXIO);
}

static inline int
crypto_shash_setkey(struct crypto_shash *csh, const uint8_t *key, size_t keylen)
{
	pr_debug("%s: TODO\n", __func__);
	return (-ENXIO);
}

static inline int
crypto_shash_update(struct shash_desc *desc, uint8_t *data, size_t datalen)
{
	pr_debug("%s: TODO\n", __func__);
	return (-ENXIO);
}

static inline void
shash_desc_zero(struct shash_desc *desc)
{

	explicit_bzero(desc, sizeof(*desc));
}

/* XXX review this. */
#define	SHASH_DESC_ON_STACK(desc, tfm)					\
	uint8_t ___ ## desc ## _desc[sizeof(struct shash_desc)];	\
	struct shash_desc *desc = (struct shash_desc *)___ ## desc ## _desc

#endif /* _LINUXKPI_CRYPTO_HASH_H */
