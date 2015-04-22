/* Copyright (c) 2013, Vsevolod Stakhov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *       * Redistributions of source code must retain the above copyright
 *         notice, this list of conditions and the following disclaimer.
 *       * Redistributions in binary form must reproduce the above copyright
 *         notice, this list of conditions and the following disclaimer in the
 *         documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ucl_internal.h"
#include "ucl_hash.h"
#include "khash.h"
#include "kvec.h"

struct ucl_hash_elt {
	const ucl_object_t *obj;
	size_t ar_idx;
};

struct ucl_hash_struct {
	void *hash;
	kvec_t(const ucl_object_t *) ar;
	bool caseless;
};

static inline uint32_t
ucl_hash_func (const ucl_object_t *o)
{
	return XXH32 (o->key, o->keylen, 0xdeadbeef);
}

static inline int
ucl_hash_equal (const ucl_object_t *k1, const ucl_object_t *k2)
{
	if (k1->keylen == k2->keylen) {
		return strncmp (k1->key, k2->key, k1->keylen) == 0;
	}

	return 0;
}

KHASH_INIT (ucl_hash_node, const ucl_object_t *, struct ucl_hash_elt, 1,
		ucl_hash_func, ucl_hash_equal)

static inline uint32_t
ucl_hash_caseless_func (const ucl_object_t *o)
{
	void *xxh = XXH32_init (0xdeadbeef);
	char hash_buf[64], *c;
	const char *p;
	ssize_t remain = o->keylen;

	p = o->key;
	c = &hash_buf[0];

	while (remain > 0) {
		*c++ = tolower (*p++);

		if (c - &hash_buf[0] == sizeof (hash_buf)) {
			XXH32_update (xxh, hash_buf, sizeof (hash_buf));
			c = &hash_buf[0];
		}
		remain --;
	}

	if (c - &hash_buf[0] != 0) {
		XXH32_update (xxh, hash_buf, c - &hash_buf[0]);
	}

	return XXH32_digest (xxh);
}

static inline int
ucl_hash_caseless_equal (const ucl_object_t *k1, const ucl_object_t *k2)
{
	if (k1->keylen == k2->keylen) {
		return strncasecmp (k1->key, k2->key, k1->keylen) == 0;
	}

	return 0;
}

KHASH_INIT (ucl_hash_caseless_node, const ucl_object_t *, struct ucl_hash_elt, 1,
		ucl_hash_caseless_func, ucl_hash_caseless_equal)

ucl_hash_t*
ucl_hash_create (bool ignore_case)
{
	ucl_hash_t *new;

	new = UCL_ALLOC (sizeof (ucl_hash_t));
	if (new != NULL) {
		kv_init (new->ar);

		new->caseless = ignore_case;
		if (ignore_case) {
			khash_t(ucl_hash_caseless_node) *h = kh_init (ucl_hash_caseless_node);
			new->hash = (void *)h;
		}
		else {
			khash_t(ucl_hash_node) *h = kh_init (ucl_hash_node);
			new->hash = (void *)h;
		}
	}
	return new;
}

void ucl_hash_destroy (ucl_hash_t* hashlin, ucl_hash_free_func *func)
{
	const ucl_object_t *cur, *tmp;

	if (hashlin == NULL) {
		return;
	}

	if (func != NULL) {
		/* Iterate over the hash first */
		khash_t(ucl_hash_node) *h = (khash_t(ucl_hash_node) *)
				hashlin->hash;
		khiter_t k;

		for (k = kh_begin (h); k != kh_end (h); ++k) {
			if (kh_exist (h, k)) {
				cur = (kh_value (h, k)).obj;
				while (cur != NULL) {
					tmp = cur->next;
					func (__DECONST (ucl_object_t *, cur));
					cur = tmp;
				}
			}
		}
	}

	if (hashlin->caseless) {
		khash_t(ucl_hash_caseless_node) *h = (khash_t(ucl_hash_caseless_node) *)
			hashlin->hash;
		kh_destroy (ucl_hash_caseless_node, h);
	}
	else {
		khash_t(ucl_hash_node) *h = (khash_t(ucl_hash_node) *)
			hashlin->hash;
		kh_destroy (ucl_hash_node, h);
	}

	kv_destroy (hashlin->ar);
	UCL_FREE (sizeof (*hashlin), hashlin);
}

void
ucl_hash_insert (ucl_hash_t* hashlin, const ucl_object_t *obj,
		const char *key, unsigned keylen)
{
	khiter_t k;
	int ret;
	struct ucl_hash_elt *elt;

	if (hashlin == NULL) {
		return;
	}

	if (hashlin->caseless) {
		khash_t(ucl_hash_caseless_node) *h = (khash_t(ucl_hash_caseless_node) *)
				hashlin->hash;
		k = kh_put (ucl_hash_caseless_node, h, obj, &ret);
		if (ret > 0) {
			elt = &kh_value (h, k);
			kv_push (const ucl_object_t *, hashlin->ar, obj);
			elt->obj = obj;
			elt->ar_idx = kv_size (hashlin->ar) - 1;
		}
	}
	else {
		khash_t(ucl_hash_node) *h = (khash_t(ucl_hash_node) *)
				hashlin->hash;
		k = kh_put (ucl_hash_node, h, obj, &ret);
		if (ret > 0) {
			elt = &kh_value (h, k);
			kv_push (const ucl_object_t *, hashlin->ar, obj);
			elt->obj = obj;
			elt->ar_idx = kv_size (hashlin->ar) - 1;
		}
	}
}

void ucl_hash_replace (ucl_hash_t* hashlin, const ucl_object_t *old,
		const ucl_object_t *new)
{
	khiter_t k;
	int ret;
	struct ucl_hash_elt elt, *pelt;

	if (hashlin == NULL) {
		return;
	}

	if (hashlin->caseless) {
		khash_t(ucl_hash_caseless_node) *h = (khash_t(ucl_hash_caseless_node) *)
				hashlin->hash;
		k = kh_put (ucl_hash_caseless_node, h, old, &ret);
		if (ret == 0) {
			elt = kh_value (h, k);
			kh_del (ucl_hash_caseless_node, h, k);
			k = kh_put (ucl_hash_caseless_node, h, new, &ret);
			pelt = &kh_value (h, k);
			pelt->obj = new;
			pelt->ar_idx = elt.ar_idx;
			kv_A (hashlin->ar, elt.ar_idx) = new;
		}
	}
	else {
		khash_t(ucl_hash_node) *h = (khash_t(ucl_hash_node) *)
				hashlin->hash;
		k = kh_put (ucl_hash_node, h, old, &ret);
		if (ret == 0) {
			elt = kh_value (h, k);
			kh_del (ucl_hash_node, h, k);
			k = kh_put (ucl_hash_node, h, new, &ret);
			pelt = &kh_value (h, k);
			pelt->obj = new;
			pelt->ar_idx = elt.ar_idx;
			kv_A (hashlin->ar, elt.ar_idx) = new;
		}
	}
}

struct ucl_hash_real_iter {
	const ucl_object_t **cur;
	const ucl_object_t **end;
};

const void*
ucl_hash_iterate (ucl_hash_t *hashlin, ucl_hash_iter_t *iter)
{
	struct ucl_hash_real_iter *it = (struct ucl_hash_real_iter *)(*iter);
	const ucl_object_t *ret = NULL;

	if (hashlin == NULL) {
		return NULL;
	}

	if (it == NULL) {
		it = UCL_ALLOC (sizeof (*it));
		it->cur = &hashlin->ar.a[0];
		it->end = it->cur + hashlin->ar.n;
	}

	if (it->cur < it->end) {
		ret = *it->cur++;
	}
	else {
		UCL_FREE (sizeof (*it), it);
		*iter = NULL;
		return NULL;
	}

	*iter = it;

	return ret;
}

bool
ucl_hash_iter_has_next (ucl_hash_t *hashlin, ucl_hash_iter_t iter)
{
	struct ucl_hash_real_iter *it = (struct ucl_hash_real_iter *)(iter);

	return it->cur < it->end - 1;
}


const ucl_object_t*
ucl_hash_search (ucl_hash_t* hashlin, const char *key, unsigned keylen)
{
	khiter_t k;
	const ucl_object_t *ret = NULL;
	ucl_object_t search;
	struct ucl_hash_elt *elt;

	search.key = key;
	search.keylen = keylen;

	if (hashlin == NULL) {
		return NULL;
	}

	if (hashlin->caseless) {
		khash_t(ucl_hash_caseless_node) *h = (khash_t(ucl_hash_caseless_node) *)
						hashlin->hash;

		k = kh_get (ucl_hash_caseless_node, h, &search);
		if (k != kh_end (h)) {
			elt = &kh_value (h, k);
			ret = elt->obj;
		}
	}
	else {
		khash_t(ucl_hash_node) *h = (khash_t(ucl_hash_node) *)
						hashlin->hash;
		k = kh_get (ucl_hash_node, h, &search);
		if (k != kh_end (h)) {
			elt = &kh_value (h, k);
			ret = elt->obj;
		}
	}

	return ret;
}

void
ucl_hash_delete (ucl_hash_t* hashlin, const ucl_object_t *obj)
{
	khiter_t k;
	struct ucl_hash_elt *elt;

	if (hashlin == NULL) {
		return;
	}

	if (hashlin->caseless) {
		khash_t(ucl_hash_caseless_node) *h = (khash_t(ucl_hash_caseless_node) *)
			hashlin->hash;

		k = kh_get (ucl_hash_caseless_node, h, obj);
		if (k != kh_end (h)) {
			elt = &kh_value (h, k);
			kv_A (hashlin->ar, elt->ar_idx) = NULL;
			kh_del (ucl_hash_caseless_node, h, k);
		}
	}
	else {
		khash_t(ucl_hash_node) *h = (khash_t(ucl_hash_node) *)
			hashlin->hash;
		k = kh_get (ucl_hash_node, h, obj);
		if (k != kh_end (h)) {
			elt = &kh_value (h, k);
			kv_A (hashlin->ar, elt->ar_idx) = NULL;
			kh_del (ucl_hash_node, h, k);
		}
	}
}
