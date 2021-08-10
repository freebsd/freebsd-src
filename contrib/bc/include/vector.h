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
 * Definitions for bc vectors (resizable arrays).
 *
 */

#ifndef BC_VECTOR_H
#define BC_VECTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <status.h>

/// An invalid index for a map to mark when an item does not exist.
#define BC_VEC_INVALID_IDX (SIZE_MAX)

/// The starting capacity for vectors. This is based on the minimum allocation
/// for 64-bit systems.
#define BC_VEC_START_CAP (UINTMAX_C(1)<<5)

/// An alias.
typedef unsigned char uchar;

/**
 * A destructor. Frees the object that @a ptr points to. This is used by vectors
 * to free the memory they own.
 * @param ptr  Pointer to the data to free.
 */
typedef void (*BcVecFree)(void *ptr);

// Forward declaration.
struct BcId;

#if BC_LONG_BIT >= 64

/// An integer to shrink the size of a vector by using these instead of size_t.
typedef uint32_t BcSize;

#else // BC_LONG_BIT >= 64

/// An integer to shrink the size of a vector by using these instead of size_t.
typedef uint16_t BcSize;

#endif // BC_LONG_BIT >= 64

/// An enum of all of the destructors. We use an enum to save space.
typedef enum BcDtorType {

	/// No destructor needed.
	BC_DTOR_NONE,

	/// Vector destructor.
	BC_DTOR_VEC,

	/// BcNum destructor.
	BC_DTOR_NUM,

#if !BC_ENABLE_LIBRARY

#ifndef NDEBUG

	/// BcFunc destructor.
	BC_DTOR_FUNC,

#endif // NDEBUG

	/// BcSlab destructor.
	BC_DTOR_SLAB,

	/// BcConst destructor.
	BC_DTOR_CONST,

	/// BcResult destructor.
	BC_DTOR_RESULT,

#if BC_ENABLE_HISTORY

	/// String destructor for history, which is *special*.
	BC_DTOR_HISTORY_STRING,

#endif // BC_ENABLE_HISTORY
#else // !BC_ENABLE_LIBRARY

	/// Destructor for bcl numbers.
	BC_DTOR_BCL_NUM,

#endif // !BC_ENABLE_LIBRARY

} BcDtorType;

/// The actual vector struct.
typedef struct BcVec {

	/// The vector array itself. This uses a char* because it is compatible with
	/// pointers of all other types, and I can do pointer arithmetic on it.
	char *restrict v;

	/// The length of the vector, which is how many items actually exist.
	size_t len;

	/// The capacity of the vector, which is how many items can fit in the
	/// current allocation.
	size_t cap;

	/// The size of the items in the vector, as returned by sizeof().
	BcSize size;

	/// The destructor as a BcDtorType enum.
	BcSize dtor;

} BcVec;

/**
 * Initializes a vector.
 * @param v      The vector to initialize.
 * @param esize  The size of the elements, as returned by sizeof().
 * @param dtor   The destructor of the elements, as a BcDtorType enum.
 */
void bc_vec_init(BcVec *restrict v, size_t esize, BcDtorType dtor);

/**
 * Expands the vector to have a capacity of @a req items, if it doesn't have
 * enough already.
 * @param v    The vector to expand.
 * @param req  The requested capacity.
 */
void bc_vec_expand(BcVec *restrict v, size_t req);

/**
 * Grow a vector by at least @a n elements.
 * @param v  The vector to grow.
 * @param n  The number of elements to grow the vector by.
 */
void bc_vec_grow(BcVec *restrict v, size_t n);

/**
 * Pops @a n items off the back of the vector. The vector must have at least
 * @a n elements.
 * @param v  The vector to pop off of.
 * @param n  The number of elements to pop off.
 */
void bc_vec_npop(BcVec *restrict v, size_t n);

/**
 * Pops @a n items, starting at index @a idx, off the vector. The vector must
 * have at least @a n elements after the @a idx index. Any remaining elements at
 * the end are moved up to fill the hole.
 * @param v  The vector to pop off of.
 * @param n  The number of elements to pop off.
 * @param idx  The index to start popping at.
 */
void bc_vec_npopAt(BcVec *restrict v, size_t n, size_t idx);

/**
 * Pushes one item on the back of the vector. It does a memcpy(), but it assumes
 * that the vector takes ownership of the data.
 * @param v     The vector to push onto.
 * @param data  A pointer to the data to push.
 */
void bc_vec_push(BcVec *restrict v, const void *data);

/**
 * Pushes @a n items on the back of the vector. It does a memcpy(), but it
 * assumes that the vector takes ownership of the data.
 * @param v     The vector to push onto.
 * @param data  A pointer to the elements of data to push.
 */
void bc_vec_npush(BcVec *restrict v, size_t n, const void *data);

/**
 * Push an empty element and return a pointer to it. This is done as an
 * optimization where initializing an item needs a pointer anyway. It removes an
 * extra memcpy().
 * @param v  The vector to push onto.
 * @return   A pointer to the newly-pushed element.
 */
void* bc_vec_pushEmpty(BcVec *restrict v);

/**
 * Pushes a byte onto a bytecode vector. This is a convenience function for the
 * parsers pushing instructions. The vector must be a bytecode vector.
 * @param v     The vector to push onto.
 * @param data  The byte to push.
 */
void bc_vec_pushByte(BcVec *restrict v, uchar data);

/**
 * Pushes and index onto a bytecode vector. The vector must be a bytecode
 * vector. For more info about why and how this is done, see the development
 * manual (manuals/development#bytecode-indices).
 * @param v    The vector to push onto.
 * @param idx  The index to push.
 */
void bc_vec_pushIndex(BcVec *restrict v, size_t idx);

/**
 * Push an item onto the vector at a certain index. The index must be valid
 * (either exists or is equal to the length of the vector). The elements at that
 * index and after are moved back one element and kept in the same order. This
 * is how the map vectors are kept sorted.
 * @param v     The vector to push onto.
 * @param data  A pointer to the data to push.
 * @param idx   The index to push at.
 */
void bc_vec_pushAt(BcVec *restrict v, const void *data, size_t idx);

/**
 * Empties the vector and sets it to the string. The vector must be a valid
 * vector and must have chars as its elements.
 * @param v    The vector to set to the string.
 * @param len  The length of the string. This can be less than the actual length
 *             of the string, but must never be more.
 * @param str  The string to push.
 */
void bc_vec_string(BcVec *restrict v, size_t len, const char *restrict str);

/**
 * Appends the string to the end of the vector, which must be holding a string
 * (nul byte-terminated) already.
 * @param v    The vector to append to.
 * @param str  The string to append (by copying).
 */
void bc_vec_concat(BcVec *restrict v, const char *restrict str);

/**
 * Empties a vector and pushes a nul-byte at the first index. The vector must be
 * a char vector.
 */
void bc_vec_empty(BcVec *restrict v);

#if BC_ENABLE_HISTORY

/**
 * Replaces an item at a particular index. No elements are moved. The index must
 * exist.
 * @param v     The vector to replace an item on.
 * @param idx   The index of the item to replace.
 * @param data  The data to replace the item with.
 */
void bc_vec_replaceAt(BcVec *restrict v, size_t idx, const void *data);

#endif // BC_ENABLE_HISTORY

/**
 * Returns a pointer to the item in the vector at the index. This is the key
 * function for vectors. The index must exist.
 * @param v    The vector.
 * @param idx  The index to the item to get a pointer to.
 * @return     A pointer to the item at @a idx.
 */
void* bc_vec_item(const BcVec *restrict v, size_t idx);

/**
 * Returns a pointer to the item in the vector at the index, reversed. This is
 * another key function for vectors. The index must exist.
 * @param v    The vector.
 * @param idx  The index to the item to get a pointer to.
 * @return     A pointer to the item at len - @a idx - 1.
 */
void* bc_vec_item_rev(const BcVec *restrict v, size_t idx);

/**
 * Zeros a vector. The vector must not be allocated.
 * @param v  The vector to clear.
 */
void bc_vec_clear(BcVec *restrict v);

/**
 * Frees a vector and its elements. This is a destructor.
 * @param vec  A vector as a void pointer.
 */
void bc_vec_free(void *vec);

/**
 * Attempts to insert an item into a map and returns true if it succeeded, false
 * if the item already exists.
 * @param v     The map vector to insert into.
 * @param name  The name of the item to insert. This name is assumed to be owned
 *              by another entity.
 * @param idx   The index of the partner array where the actual item is.
 * @param i     A pointer to an index that will be set to the index of the item
 *              in the map.
 * @return      True if the item was inserted, false if the item already exists.
 */
bool bc_map_insert(BcVec *restrict v, const char *name,
                   size_t idx, size_t *restrict i);

/**
 * Returns the index of the item with @a name in the map, or BC_VEC_INVALID_IDX
 * if it doesn't exist.
 * @param v     The map vector.
 * @param name  The name of the item to find.
 * @return      The index in the map of the item with @a name, or
 *              BC_VEC_INVALID_IDX if the item does not exist.
 */
size_t bc_map_index(const BcVec *restrict v, const char *name);

#if DC_ENABLED

/**
 * Returns the name of the item at index @a idx in the map.
 * @param v    The map vector.
 * @param idx  The index.
 * @return     The name of the item at @a idx.
 */
const char* bc_map_name(const BcVec *restrict v, size_t idx);

#endif // DC_ENABLED

/**
 * Pops one item off of the vector.
 * @param v  The vector to pop one item off of.
 */
#define bc_vec_pop(v) (bc_vec_npop((v), 1))

/**
 * Pops all items off of the vector.
 * @param v  The vector to pop all items off of.
 */
#define bc_vec_popAll(v) (bc_vec_npop((v), (v)->len))

/**
 * Return a pointer to the last item in the vector, or first if it's being
 * treated as a stack.
 * @param v  The vector to get the top of stack of.
 */
#define bc_vec_top(v) (bc_vec_item_rev((v), 0))

/**
 * Initializes a vector to serve as a map.
 * @param v  The vector to initialize.
 */
#define bc_map_init(v) (bc_vec_init((v), sizeof(BcId), BC_DTOR_NONE))

/// A reference to the array of destructors.
extern const BcVecFree bc_vec_dtors[];

#if !BC_ENABLE_LIBRARY

/// The allocated size of slabs.
#define BC_SLAB_SIZE (4096)

/// A slab for allocating strings.
typedef struct BcSlab {

	/// The actual allocation.
	char *s;

	/// How many bytes of the slab are taken.
	size_t len;

} BcSlab;

/**
 * Frees a slab. This is a destructor.
 * @param slab  The slab as a void pointer.
 */
void bc_slab_free(void *slab);

/**
 * Initializes a slab vector.
 * @param v  The vector to initialize.
 */
void bc_slabvec_init(BcVec *restrict v);

/**
 * Duplicates the string using slabs in the slab vector.
 * @param v    The slab vector.
 * @param str  The string to duplicate.
 * @return     A pointer to the duplicated string, owned by the slab vector.
 */
char* bc_slabvec_strdup(BcVec *restrict v, const char *str);

#if BC_ENABLED

/**
 * Undoes the last allocation on the slab vector. This allows bc to have a
 * heap-based stacks for strings. This is used by the bc parser.
 */
void bc_slabvec_undo(BcVec *restrict v, size_t len);

#endif // BC_ENABLED

/**
 * Clears a slab vector. This deallocates all but the first slab and clears the
 * first slab.
 * @param v  The slab vector to clear.
 */
void bc_slabvec_clear(BcVec *restrict v);

#if BC_DEBUG_CODE

/**
 * Prints all of the items in a slab vector, in order.
 * @param v  The vector whose items will be printed.
 */
void bc_slabvec_print(BcVec *v, const char *func);

#endif // BC_DEBUG_CODE

/// A convenience macro for freeing a vector of slabs.
#define bc_slabvec_free bc_vec_free

#ifndef _WIN32

/**
 * A macro to get rid of a warning on Windows.
 * @param d  The destination string.
 * @param l  The length of the destination string. This has to be big enough to
 *           contain @a s.
 * @param s  The source string.
 */
#define bc_strcpy(d, l, s) strcpy(d, s)

#else // _WIN32

/**
 * A macro to get rid of a warning on Windows.
 * @param d  The destination string.
 * @param l  The length of the destination string. This has to be big enough to
 *           contain @a s.
 * @param s  The source string.
 */
#define bc_strcpy(d, l, s) strcpy_s(d, l, s)

#endif // _WIN32

#endif // !BC_ENABLE_LIBRARY

#endif // BC_VECTOR_H
