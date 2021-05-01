/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2021 Gavin D. Howard and contributors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * *****************************************************************************
 *
 * Code to manipulate vectors (resizable arrays).
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <vector.h>
#include <lang.h>
#include <vm.h>

void bc_vec_grow(BcVec *restrict v, size_t n) {

	size_t len, cap = v->cap;
	sig_atomic_t lock;

	len = bc_vm_growSize(v->len, n);

	while (cap < len) cap = bc_vm_growSize(cap, cap);

	BC_SIG_TRYLOCK(lock);
	v->v = bc_vm_realloc(v->v, bc_vm_arraySize(cap, v->size));
	v->cap = cap;
	BC_SIG_TRYUNLOCK(lock);
}

void bc_vec_init(BcVec *restrict v, size_t esize, BcVecFree dtor) {
	BC_SIG_ASSERT_LOCKED;
	assert(v != NULL && esize);
	v->size = esize;
	v->cap = BC_VEC_START_CAP;
	v->len = 0;
	v->dtor = dtor;
	v->v = bc_vm_malloc(bc_vm_arraySize(BC_VEC_START_CAP, esize));
}

void bc_vec_expand(BcVec *restrict v, size_t req) {

	assert(v != NULL);

	if (v->cap < req) {

		sig_atomic_t lock;

		BC_SIG_TRYLOCK(lock);

		v->v = bc_vm_realloc(v->v, bc_vm_arraySize(req, v->size));
		v->cap = req;

		BC_SIG_TRYUNLOCK(lock);
	}
}

void bc_vec_npop(BcVec *restrict v, size_t n) {

	sig_atomic_t lock;

	assert(v != NULL && n <= v->len);

	BC_SIG_TRYLOCK(lock);

	if (v->dtor == NULL) v->len -= n;
	else {
		size_t len = v->len - n;
		while (v->len > len) v->dtor(v->v + (v->size * --v->len));
	}

	BC_SIG_TRYUNLOCK(lock);
}

void bc_vec_npopAt(BcVec *restrict v, size_t n, size_t idx) {

	char* ptr, *data;

	assert(v != NULL);
	assert(idx + n < v->len);

	ptr = bc_vec_item(v, idx);
	data = bc_vec_item(v, idx + n);

	BC_SIG_LOCK;

	if (v->dtor != NULL) {

		size_t i;

		for (i = 0; i < n; ++i) v->dtor(bc_vec_item(v, idx + i));
	}

	v->len -= n;
	memmove(ptr, data, (v->len - idx) * v->size);

	BC_SIG_UNLOCK;
}

void bc_vec_npush(BcVec *restrict v, size_t n, const void *data) {

	sig_atomic_t lock;

	assert(v != NULL && data != NULL);

	BC_SIG_TRYLOCK(lock);

	if (v->len + n > v->cap) bc_vec_grow(v, n);

	memcpy(v->v + (v->size * v->len), data, v->size * n);
	v->len += n;

	BC_SIG_TRYUNLOCK(lock);
}

inline void bc_vec_push(BcVec *restrict v, const void *data) {
	bc_vec_npush(v, 1, data);
}

void bc_vec_pushByte(BcVec *restrict v, uchar data) {
	assert(v != NULL && v->size == sizeof(uchar));
	bc_vec_npush(v, 1, &data);
}

void bc_vec_pushIndex(BcVec *restrict v, size_t idx) {

	uchar amt, nums[sizeof(size_t) + 1];

	assert(v != NULL);
	assert(v->size == sizeof(uchar));

	for (amt = 0; idx; ++amt) {
		nums[amt + 1] = (uchar) idx;
		idx &= ((size_t) ~(UCHAR_MAX));
		idx >>= sizeof(uchar) * CHAR_BIT;
	}

	nums[0] = amt;

	bc_vec_npush(v, amt + 1, nums);
}

static void bc_vec_pushAt(BcVec *restrict v, const void *data, size_t idx) {

	sig_atomic_t lock;

	assert(v != NULL && data != NULL && idx <= v->len);

	BC_SIG_TRYLOCK(lock);

	if (idx == v->len) bc_vec_push(v, data);
	else {

		char *ptr;

		if (v->len == v->cap) bc_vec_grow(v, 1);

		ptr = v->v + v->size * idx;

		memmove(ptr + v->size, ptr, v->size * (v->len++ - idx));
		memmove(ptr, data, v->size);
	}

	BC_SIG_TRYUNLOCK(lock);
}

void bc_vec_string(BcVec *restrict v, size_t len, const char *restrict str) {

	sig_atomic_t lock;

	assert(v != NULL && v->size == sizeof(char));
	assert(v->dtor == NULL);
	assert(!v->len || !v->v[v->len - 1]);
	assert(v->v != str);

	BC_SIG_TRYLOCK(lock);

	bc_vec_popAll(v);
	bc_vec_expand(v, bc_vm_growSize(len, 1));
	memcpy(v->v, str, len);
	v->len = len;

	bc_vec_pushByte(v, '\0');

	BC_SIG_TRYUNLOCK(lock);
}

void bc_vec_concat(BcVec *restrict v, const char *restrict str) {

	sig_atomic_t lock;

	assert(v != NULL && v->size == sizeof(char));
	assert(v->dtor == NULL);
	assert(!v->len || !v->v[v->len - 1]);
	assert(v->v != str);

	BC_SIG_TRYLOCK(lock);

	if (v->len) v->len -= 1;

	bc_vec_npush(v, strlen(str) + 1, str);

	BC_SIG_TRYUNLOCK(lock);
}

void bc_vec_empty(BcVec *restrict v) {

	sig_atomic_t lock;

	assert(v != NULL && v->size == sizeof(char));
	assert(v->dtor == NULL);

	BC_SIG_TRYLOCK(lock);

	bc_vec_popAll(v);
	bc_vec_pushByte(v, '\0');

	BC_SIG_TRYUNLOCK(lock);
}

#if BC_ENABLE_HISTORY
void bc_vec_replaceAt(BcVec *restrict v, size_t idx, const void *data) {

	char *ptr;

	BC_SIG_ASSERT_LOCKED;

	assert(v != NULL);

	ptr = bc_vec_item(v, idx);

	if (v->dtor != NULL) v->dtor(ptr);

	memcpy(ptr, data, v->size);
}
#endif // BC_ENABLE_HISTORY

inline void* bc_vec_item(const BcVec *restrict v, size_t idx) {
	assert(v != NULL && v->len && idx < v->len);
	return v->v + v->size * idx;
}

inline void* bc_vec_item_rev(const BcVec *restrict v, size_t idx) {
	assert(v != NULL && v->len && idx < v->len);
	return v->v + v->size * (v->len - idx - 1);
}

inline void bc_vec_clear(BcVec *restrict v) {
	BC_SIG_ASSERT_LOCKED;
	v->v = NULL;
	v->len = 0;
	v->dtor = NULL;
}

void bc_vec_free(void *vec) {
	BcVec *v = (BcVec*) vec;
	BC_SIG_ASSERT_LOCKED;
	bc_vec_popAll(v);
	free(v->v);
}

static size_t bc_map_find(const BcVec *restrict v, const char *name) {

	size_t low = 0, high = v->len;

	while (low < high) {

		size_t mid = (low + high) / 2;
		const BcId *id = bc_vec_item(v, mid);
		int result = strcmp(name, id->name);

		if (!result) return mid;
		else if (result < 0) high = mid;
		else low = mid + 1;
	}

	return low;
}

bool bc_map_insert(BcVec *restrict v, const char *name,
                   size_t idx, size_t *restrict i)
{
	BcId id;

	BC_SIG_ASSERT_LOCKED;

	assert(v != NULL && name != NULL && i != NULL);

	*i = bc_map_find(v, name);

	assert(*i <= v->len);

	if (*i != v->len && !strcmp(name, ((BcId*) bc_vec_item(v, *i))->name))
		return false;

	id.name = bc_vm_strdup(name);
	id.idx = idx;

	bc_vec_pushAt(v, &id, *i);

	return true;
}

size_t bc_map_index(const BcVec *restrict v, const char *name) {

	size_t i;

	assert(v != NULL && name != NULL);

	i = bc_map_find(v, name);

	if (i >= v->len) return BC_VEC_INVALID_IDX;

	return strcmp(name, ((BcId*) bc_vec_item(v, i))->name) ?
	    BC_VEC_INVALID_IDX : i;
}
