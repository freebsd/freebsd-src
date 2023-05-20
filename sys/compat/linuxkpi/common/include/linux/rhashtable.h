/*-
 * Copyright (c) 2023 Bjoern A. Zeeb
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
 *
 * $FreeBSD$
 */

#ifndef	_LINUXKPI_LINUX_RHASHTABLE_H
#define	_LINUXKPI_LINUX_RHASHTABLE_H

#include <linux/kernel.h>	/* pr_debug */

struct rhash_head {
};

struct rhashtable_params {
	uint16_t	head_offset;
	uint16_t	key_len;
	uint16_t	key_offset;
	uint16_t	nelem_hint;
	bool		automatic_shrinking;
};

struct rhashtable {
};

static inline int
rhashtable_init(struct rhashtable *rht,
    const struct rhashtable_params *params)
{

	pr_debug("%s: TODO\n", __func__);
	return (-1);
}

static inline void
rhashtable_destroy(struct rhashtable *rht)
{
	pr_debug("%s: TODO\n", __func__);
}

static inline void *
rhashtable_lookup_fast(struct rhashtable *rht, const void *key,
    const struct rhashtable_params params)
{

	pr_debug("%s: TODO\n", __func__);
	return (NULL);
}

static inline void *
rhashtable_lookup_get_insert_fast(struct rhashtable *rht,
    struct rhash_head *obj, const struct rhashtable_params params)
{

	pr_debug("%s: TODO\n", __func__);
	return (NULL);
}

static inline int
rhashtable_remove_fast(struct rhashtable *rht,
    struct rhash_head *obj, const struct rhashtable_params params)
{

	pr_debug("%s: TODO\n", __func__);
	return (-ENOENT);
}

#endif	/* _LINUXKPI_LINUX_RHASHTABLE_H */
