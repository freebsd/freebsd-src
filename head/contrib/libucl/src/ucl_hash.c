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
#include "utlist.h"

ucl_hash_t*
ucl_hash_create (void)
{
	ucl_hash_t *new;

	new = UCL_ALLOC (sizeof (ucl_hash_t));
	if (new != NULL) {
		new->buckets = NULL;
	}
	return new;
}

void ucl_hash_destroy (ucl_hash_t* hashlin, ucl_hash_free_func *func)
{
	ucl_hash_node_t *elt, *tmp;
	const ucl_object_t *cur, *otmp;

	HASH_ITER (hh, hashlin->buckets, elt, tmp) {
		HASH_DELETE (hh, hashlin->buckets, elt);
		if (func) {
			DL_FOREACH_SAFE (elt->data, cur, otmp) {
				/* Need to deconst here */
				func (__DECONST (ucl_object_t *, cur));
			}
		}
		UCL_FREE (sizeof (ucl_hash_node_t), elt);
	}
	UCL_FREE (sizeof (ucl_hash_t), hashlin);
}

void
ucl_hash_insert (ucl_hash_t* hashlin, const ucl_object_t *obj,
		const char *key, unsigned keylen)
{
	ucl_hash_node_t *node;

	node = UCL_ALLOC (sizeof (ucl_hash_node_t));
	node->data = obj;
	HASH_ADD_KEYPTR (hh, hashlin->buckets, key, keylen, node);
}

void ucl_hash_replace (ucl_hash_t* hashlin, const ucl_object_t *old,
		const ucl_object_t *new)
{
	ucl_hash_node_t *node;

	HASH_FIND (hh, hashlin->buckets, old->key, old->keylen, node);
	if (node != NULL) {
		/* Direct replacement */
		node->data = new;
		node->hh.key = new->key;
		node->hh.keylen = new->keylen;
	}
}

const void*
ucl_hash_iterate (ucl_hash_t *hashlin, ucl_hash_iter_t *iter)
{
	ucl_hash_node_t *elt = *iter;

	if (elt == NULL) {
		if (hashlin == NULL || hashlin->buckets == NULL) {
			return NULL;
		}
		elt = hashlin->buckets;
		if (elt == NULL) {
			return NULL;
		}
	}
	else if (elt == hashlin->buckets) {
		return NULL;
	}

	*iter = elt->hh.next ? elt->hh.next : hashlin->buckets;
	return elt->data;
}

bool
ucl_hash_iter_has_next (ucl_hash_iter_t iter)
{
	ucl_hash_node_t *elt = iter;

	return (elt == NULL || elt->hh.prev != NULL);
}


const ucl_object_t*
ucl_hash_search (ucl_hash_t* hashlin, const char *key, unsigned keylen)
{
	ucl_hash_node_t *found;

	if (hashlin == NULL) {
		return NULL;
	}
	HASH_FIND (hh, hashlin->buckets, key, keylen, found);

	if (found) {
		return found->data;
	}
	return NULL;
}

void
ucl_hash_delete (ucl_hash_t* hashlin, const ucl_object_t *obj)
{
	ucl_hash_node_t *found;

	HASH_FIND (hh, hashlin->buckets, obj->key, obj->keylen, found);

	if (found) {
		HASH_DELETE (hh, hashlin->buckets, found);
		UCL_FREE (sizeof (ucl_hash_node_t), found);
	}
}
