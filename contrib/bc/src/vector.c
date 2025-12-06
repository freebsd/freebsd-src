/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2025 Gavin D. Howard and contributors.
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
#include <stdbool.h>

#include <vector.h>
#include <lang.h>
#include <vm.h>

void
bc_vec_grow(BcVec* restrict v, size_t n)
{
	size_t cap, len;
#if !BC_ENABLE_LIBRARY
	sig_atomic_t lock;
#endif // !BC_ENABLE_LIBRARY

	cap = v->cap;
	len = v->len + n;

	// If this is true, we might overflow.
	if (len > SIZE_MAX / 2) cap = len;
	else
	{
		// Keep doubling until larger.
		while (cap < len)
		{
			cap += cap;
		}
	}

	BC_SIG_TRYLOCK(lock);

	v->v = bc_vm_realloc(v->v, bc_vm_arraySize(cap, v->size));
	v->cap = cap;

	BC_SIG_TRYUNLOCK(lock);
}

void
bc_vec_init(BcVec* restrict v, size_t esize, BcDtorType dtor)
{
	BC_SIG_ASSERT_LOCKED;

	assert(v != NULL && esize);

	v->v = bc_vm_malloc(bc_vm_arraySize(BC_VEC_START_CAP, esize));

	v->size = (BcSize) esize;
	v->cap = BC_VEC_START_CAP;
	v->len = 0;
	v->dtor = (BcSize) dtor;
}

void
bc_vec_expand(BcVec* restrict v, size_t req)
{
	assert(v != NULL);

	// Only expand if necessary.
	if (v->cap < req)
	{
#if !BC_ENABLE_LIBRARY
		sig_atomic_t lock;
#endif // !BC_ENABLE_LIBRARY

		BC_SIG_TRYLOCK(lock);

		v->v = bc_vm_realloc(v->v, bc_vm_arraySize(req, v->size));
		v->cap = req;

		BC_SIG_TRYUNLOCK(lock);
	}
}

void
bc_vec_npop(BcVec* restrict v, size_t n)
{
#if !BC_ENABLE_LIBRARY
	sig_atomic_t lock;
#endif // !BC_ENABLE_LIBRARY

	assert(v != NULL && n <= v->len);

	BC_SIG_TRYLOCK(lock);

	if (!v->dtor) v->len -= n;
	else
	{
		const BcVecFree d = bc_vec_dtors[v->dtor];
		size_t esize = v->size;
		size_t len = v->len - n;

		// Loop through and manually destruct every element.
		while (v->len > len)
		{
			d(v->v + (esize * --v->len));
		}
	}

	BC_SIG_TRYUNLOCK(lock);
}

void
bc_vec_npopAt(BcVec* restrict v, size_t n, size_t idx)
{
	char* ptr;
	char* data;
#if !BC_ENABLE_LIBRARY
	sig_atomic_t lock;
#endif // !BC_ENABLE_LIBRARY

	assert(v != NULL);
	assert(idx + n < v->len);

	// Grab start and end pointers.
	ptr = bc_vec_item(v, idx);
	data = bc_vec_item(v, idx + n);

	BC_SIG_TRYLOCK(lock);

	if (v->dtor)
	{
		size_t i;
		const BcVecFree d = bc_vec_dtors[v->dtor];

		// Destroy every popped item.
		for (i = 0; i < n; ++i)
		{
			d(bc_vec_item(v, idx + i));
		}
	}

	v->len -= n;
	// NOLINTNEXTLINE
	memmove(ptr, data, (v->len - idx) * v->size);

	BC_SIG_TRYUNLOCK(lock);
}

void
bc_vec_npush(BcVec* restrict v, size_t n, const void* data)
{
#if !BC_ENABLE_LIBRARY
	sig_atomic_t lock;
#endif // !BC_ENABLE_LIBRARY
	size_t esize;

	assert(v != NULL && data != NULL);

	BC_SIG_TRYLOCK(lock);

	// Grow if necessary.
	if (v->len + n > v->cap) bc_vec_grow(v, n);

	esize = v->size;

	// Copy the elements in.
	// NOLINTNEXTLINE
	memcpy(v->v + (esize * v->len), data, esize * n);
	v->len += n;

	BC_SIG_TRYUNLOCK(lock);
}

inline void
bc_vec_push(BcVec* restrict v, const void* data)
{
	bc_vec_npush(v, 1, data);
}

void*
bc_vec_pushEmpty(BcVec* restrict v)
{
#if !BC_ENABLE_LIBRARY
	sig_atomic_t lock;
#endif // !BC_ENABLE_LIBRARY
	void* ptr;

	assert(v != NULL);

	BC_SIG_TRYLOCK(lock);

	// Grow if necessary.
	if (v->len + 1 > v->cap) bc_vec_grow(v, 1);

	ptr = v->v + v->size * v->len;
	v->len += 1;

	BC_SIG_TRYUNLOCK(lock);

	return ptr;
}

inline void
bc_vec_pushByte(BcVec* restrict v, uchar data)
{
	assert(v != NULL && v->size == sizeof(uchar));
	bc_vec_npush(v, 1, &data);
}

void
bc_vec_pushIndex(BcVec* restrict v, size_t idx)
{
	uchar amt, nums[sizeof(size_t) + 1];

	assert(v != NULL);
	assert(v->size == sizeof(uchar));

	// Encode the index.
	for (amt = 0; idx; ++amt)
	{
		nums[amt + 1] = (uchar) idx;
		idx &= ((size_t) ~(UCHAR_MAX));
		idx >>= sizeof(uchar) * CHAR_BIT;
	}

	nums[0] = amt;

	// Push the index onto the vector.
	bc_vec_npush(v, amt + 1, nums);
}

void
bc_vec_pushAt(BcVec* restrict v, const void* data, size_t idx)
{
	assert(v != NULL && data != NULL && idx <= v->len);

	BC_SIG_ASSERT_LOCKED;

	// Do the easy case.
	if (idx == v->len) bc_vec_push(v, data);
	else
	{
		char* ptr;
		size_t esize;

		// Grow if necessary.
		if (v->len == v->cap) bc_vec_grow(v, 1);

		esize = v->size;

		ptr = v->v + esize * idx;

		// NOLINTNEXTLINE
		memmove(ptr + esize, ptr, esize * (v->len++ - idx));
		// NOLINTNEXTLINE
		memcpy(ptr, data, esize);
	}
}

void
bc_vec_string(BcVec* restrict v, size_t len, const char* restrict str)
{
#if !BC_ENABLE_LIBRARY
	sig_atomic_t lock;
#endif // !BC_ENABLE_LIBRARY

	assert(v != NULL && v->size == sizeof(char));
	assert(!v->dtor);
	assert(!v->len || !v->v[v->len - 1]);
	assert(v->v != str);

	BC_SIG_TRYLOCK(lock);

	bc_vec_popAll(v);
	bc_vec_expand(v, bc_vm_growSize(len, 1));
	// NOLINTNEXTLINE
	memcpy(v->v, str, len);
	v->len = len;

	bc_vec_pushByte(v, '\0');

	BC_SIG_TRYUNLOCK(lock);
}

void
bc_vec_concat(BcVec* restrict v, const char* restrict str)
{
#if !BC_ENABLE_LIBRARY
	sig_atomic_t lock;
#endif // !BC_ENABLE_LIBRARY

	assert(v != NULL && v->size == sizeof(char));
	assert(!v->dtor);
	assert(!v->len || !v->v[v->len - 1]);
	assert(v->v != str);

	BC_SIG_TRYLOCK(lock);

	// If there is already a string, erase its nul byte.
	if (v->len) v->len -= 1;

	bc_vec_npush(v, strlen(str) + 1, str);

	BC_SIG_TRYUNLOCK(lock);
}

void
bc_vec_empty(BcVec* restrict v)
{
#if !BC_ENABLE_LIBRARY
	sig_atomic_t lock;
#endif // !BC_ENABLE_LIBRARY

	assert(v != NULL && v->size == sizeof(char));
	assert(!v->dtor);

	BC_SIG_TRYLOCK(lock);

	bc_vec_popAll(v);
	bc_vec_pushByte(v, '\0');

	BC_SIG_TRYUNLOCK(lock);
}

#if BC_ENABLE_HISTORY
void
bc_vec_replaceAt(BcVec* restrict v, size_t idx, const void* data)
{
	char* ptr;

	BC_SIG_ASSERT_LOCKED;

	assert(v != NULL);

	ptr = bc_vec_item(v, idx);

	if (v->dtor) bc_vec_dtors[v->dtor](ptr);

	// NOLINTNEXTLINE
	memcpy(ptr, data, v->size);
}
#endif // BC_ENABLE_HISTORY

inline void*
bc_vec_item(const BcVec* restrict v, size_t idx)
{
	assert(v != NULL && v->len && idx < v->len);
	return v->v + v->size * idx;
}

inline void*
bc_vec_item_rev(const BcVec* restrict v, size_t idx)
{
	assert(v != NULL && v->len && idx < v->len);
	return v->v + v->size * (v->len - idx - 1);
}

inline void
bc_vec_clear(BcVec* restrict v)
{
	BC_SIG_ASSERT_LOCKED;
	v->v = NULL;
	v->len = 0;
	v->dtor = BC_DTOR_NONE;
}

void
bc_vec_free(void* vec)
{
	BcVec* v = (BcVec*) vec;
	BC_SIG_ASSERT_LOCKED;
	bc_vec_popAll(v);
	free(v->v);
}

#if !BC_ENABLE_LIBRARY

/**
 * Finds a name in a map by binary search. Returns the index where the item
 * *would* be if it doesn't exist. Callers are responsible for checking that the
 * item exists at the index.
 * @param v     The map.
 * @param name  The name to find.
 * @return      The index of the item with @a name, or where the item would be
 *              if it does not exist.
 */
static size_t
bc_map_find(const BcVec* restrict v, const char* name)
{
	size_t low = 0, high = v->len;

	while (low < high)
	{
		size_t mid = low + (high - low) / 2;
		const BcId* id = bc_vec_item(v, mid);
		int result = strcmp(name, id->name);

		if (!result) return mid;
		else if (result < 0) high = mid;
		else low = mid + 1;
	}

	return low;
}

bool
bc_map_insert(BcVec* restrict v, const char* name, size_t idx,
              size_t* restrict i)
{
	BcId id;

	BC_SIG_ASSERT_LOCKED;

	assert(v != NULL && name != NULL && i != NULL);

	*i = bc_map_find(v, name);

	assert(*i <= v->len);

	if (*i != v->len && !strcmp(name, ((BcId*) bc_vec_item(v, *i))->name))
	{
		return false;
	}

	id.name = bc_slabvec_strdup(&vm->slabs, name);
	id.idx = idx;

	bc_vec_pushAt(v, &id, *i);

	return true;
}

size_t
bc_map_index(const BcVec* restrict v, const char* name)
{
	size_t i;
	BcId* id;

	assert(v != NULL && name != NULL);

	i = bc_map_find(v, name);

	// If out of range, return invalid.
	if (i >= v->len) return BC_VEC_INVALID_IDX;

	id = (BcId*) bc_vec_item(v, i);

	// Make sure the item exists and return appropriately.
	return strcmp(name, id->name) ? BC_VEC_INVALID_IDX : i;
}

#if DC_ENABLED
const char*
bc_map_name(const BcVec* restrict v, size_t idx)
{
	size_t i, len = v->len;

	for (i = 0; i < len; ++i)
	{
		BcId* id = (BcId*) bc_vec_item(v, i);
		if (id->idx == idx) return id->name;
	}

	BC_UNREACHABLE

#if !BC_CLANG
	return "";
#endif // !BC_CLANG
}
#endif // DC_ENABLED

/**
 * Initializes a single slab.
 * @param s  The slab to initialize.
 */
static void
bc_slab_init(BcSlab* s)
{
	s->s = bc_vm_malloc(BC_SLAB_SIZE);
	s->len = 0;
}

/**
 * Adds a string to a slab and returns a pointer to it, or NULL if it could not
 * be added.
 * @param s    The slab to add to.
 * @param str  The string to add.
 * @param len  The length of the string, including its nul byte.
 * @return     A pointer to the new string in the slab, or NULL if it could not
 *             be added.
 */
static char*
bc_slab_add(BcSlab* s, const char* str, size_t len)
{
	char* ptr;

	assert(s != NULL);
	assert(str != NULL);
	assert(len == strlen(str) + 1);

	if (s->len + len > BC_SLAB_SIZE) return NULL;

	ptr = (char*) (s->s + s->len);

	// NOLINTNEXTLINE
	bc_strcpy(ptr, len, str);

	s->len += len;

	return ptr;
}

void
bc_slab_free(void* slab)
{
	free(((BcSlab*) slab)->s);
}

void
bc_slabvec_init(BcVec* v)
{
	BcSlab* slab;

	assert(v != NULL);

	bc_vec_init(v, sizeof(BcSlab), BC_DTOR_SLAB);

	// We always want to have at least one slab.
	slab = bc_vec_pushEmpty(v);
	bc_slab_init(slab);
}

char*
bc_slabvec_strdup(BcVec* v, const char* str)
{
	char* s;
	size_t len;
	BcSlab slab;
	BcSlab* slab_ptr;

	BC_SIG_ASSERT_LOCKED;

	assert(v != NULL && v->len);

	assert(str != NULL);

	len = strlen(str) + 1;

	// If the len is greater than 128, then just allocate it with malloc.
	if (BC_UNLIKELY(len >= BC_SLAB_SIZE))
	{
		// SIZE_MAX is a marker for these standalone allocations.
		slab.len = SIZE_MAX;
		slab.s = bc_vm_strdup(str);

		// Push the standalone slab.
		bc_vec_pushAt(v, &slab, v->len - 1);

		return slab.s;
	}

	// Add to a slab.
	slab_ptr = bc_vec_top(v);
	s = bc_slab_add(slab_ptr, str, len);

	// If it couldn't be added, add a slab and try again.
	if (BC_UNLIKELY(s == NULL))
	{
		slab_ptr = bc_vec_pushEmpty(v);
		bc_slab_init(slab_ptr);

		s = bc_slab_add(slab_ptr, str, len);

		assert(s != NULL);
	}

	return s;
}

void
bc_slabvec_clear(BcVec* v)
{
	BcSlab* s;
	bool again;

	// This complicated loop exists because of standalone allocations over 128
	// bytes.
	do
	{
		// Get the first slab.
		s = bc_vec_item(v, 0);

		// Either the slab must be valid (not standalone), or there must be
		// another slab.
		assert(s->len != SIZE_MAX || v->len > 1);

		// Do we have to loop again? We do if it's a standalone allocation.
		again = (s->len == SIZE_MAX);

		// Pop the standalone allocation, not the one after it.
		if (again) bc_vec_npopAt(v, 1, 0);
	}
	while (again);

	// If we get here, we know that the first slab is a valid slab. We want to
	// pop all of the other slabs.
	if (v->len > 1) bc_vec_npop(v, v->len - 1);

	// Empty the first slab.
	s->len = 0;
}
#endif // !BC_ENABLE_LIBRARY

#if BC_DEBUG_CODE

void
bc_slabvec_print(BcVec* v, const char* func)
{
	size_t i;
	BcSlab* s;

	bc_file_printf(&vm->ferr, "%s\n", func);

	for (i = 0; i < v->len; ++i)
	{
		s = bc_vec_item(v, i);
		bc_file_printf(&vm->ferr, "%zu { s = %zu, len = %zu }\n", i,
		               (uintptr_t) s->s, s->len);
	}

	bc_file_puts(&vm->ferr, bc_flush_none, "\n");
	bc_file_flush(&vm->ferr, bc_flush_none);
}

#endif // BC_DEBUG_CODE
