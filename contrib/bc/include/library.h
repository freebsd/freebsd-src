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
 * The private header for the bc library.
 *
 */

#ifndef LIBBC_PRIVATE_H
#define LIBBC_PRIVATE_H

#ifndef _WIN32

#include <pthread.h>

#endif // _WIN32

#include <bcl.h>

#include <num.h>
#include <vm.h>

#if BC_ENABLE_MEMCHECK

/**
 * A typedef for Valgrind builds. This is to add a generation index for error
 * checking.
 */
typedef struct BclNum
{
	/// The number.
	BcNum n;

	/// The generation index.
	size_t gen_idx;

} BclNum;

/**
 * Clears the generation byte in a BclNumber and returns the value.
 * @param n  The BclNumber.
 * @return   The value of the index.
 */
#define BCL_NO_GEN(n) \
	((n).i & ~(((size_t) UCHAR_MAX) << ((sizeof(size_t) - 1) * CHAR_BIT)))

/**
 * Gets the generation index in a BclNumber.
 * @param n  The BclNumber.
 * @return   The generation index.
 */
#define BCL_GET_GEN(n) ((n).i >> ((sizeof(size_t) - 1) * CHAR_BIT))

/**
 * Turns a BclNumber into a BcNum.
 * @param c  The context.
 * @param n  The BclNumber.
 */
#define BCL_NUM(c, n) ((BclNum*) bc_vec_item(&(c)->nums, BCL_NO_GEN(n)))

/**
 * Clears the generation index top byte in the BclNumber.
 * @param n  The BclNumber.
 */
#define BCL_CLEAR_GEN(n)                                                       \
	do                                                                         \
	{                                                                          \
		(n).i &= ~(((size_t) UCHAR_MAX) << ((sizeof(size_t) - 1) * CHAR_BIT)); \
	}                                                                          \
	while (0)

#define BCL_CHECK_NUM_GEN(c, bn)         \
	do                                   \
	{                                    \
		size_t gen_ = BCL_GET_GEN(bn);   \
		BclNum* ptr_ = BCL_NUM(c, bn);   \
		if (BCL_NUM_ARRAY(ptr_) == NULL) \
		{                                \
			bcl_nonexistentNum();        \
		}                                \
		if (gen_ != ptr_->gen_idx)       \
		{                                \
			bcl_invalidGeneration();     \
		}                                \
	}                                    \
	while (0)

#define BCL_CHECK_NUM_VALID(c, bn)    \
	do                                \
	{                                 \
		size_t idx_ = BCL_NO_GEN(bn); \
		if ((c)->nums.len <= idx_)    \
		{                             \
			bcl_numIdxOutOfRange();   \
		}                             \
		BCL_CHECK_NUM_GEN(c, bn);     \
	}                                 \
	while (0)

/**
 * Returns the limb array of the number.
 * @param bn  The number.
 * @return    The limb array.
 */
#define BCL_NUM_ARRAY(bn) ((bn)->n.num)

/**
 * Returns the limb array of the number for a non-pointer.
 * @param bn  The number.
 * @return    The limb array.
 */
#define BCL_NUM_ARRAY_NP(bn) ((bn).n.num)

/**
 * Returns the BcNum pointer.
 * @param bn  The number.
 * @return    The BcNum pointer.
 */
#define BCL_NUM_NUM(bn) (&(bn)->n)

/**
 * Returns the BcNum pointer for a non-pointer.
 * @param bn  The number.
 * @return    The BcNum pointer.
 */
#define BCL_NUM_NUM_NP(bn) (&(bn).n)

// These functions only abort. They exist to give developers some idea of what
// went wrong when bugs are found, if they look at the Valgrind stack trace.

BC_NORETURN void
bcl_invalidGeneration(void);

BC_NORETURN void
bcl_nonexistentNum(void);

BC_NORETURN void
bcl_numIdxOutOfRange(void);

#else // BC_ENABLE_MEMCHECK

/**
 * A typedef for non-Valgrind builds.
 */
typedef BcNum BclNum;

#define BCL_NO_GEN(n) ((n).i)
#define BCL_NUM(c, n) ((BclNum*) bc_vec_item(&(c)->nums, (n).i))
#define BCL_CLEAR_GEN(n) ((void) (n))

#define BCL_CHECK_NUM_GEN(c, bn)
#define BCL_CHECK_NUM_VALID(c, n)

#define BCL_NUM_ARRAY(bn) ((bn)->num)
#define BCL_NUM_ARRAY_NP(bn) ((bn).num)

#define BCL_NUM_NUM(bn) (bn)
#define BCL_NUM_NUM_NP(bn) (&(bn))

#endif // BC_ENABLE_MEMCHECK

/**
 * A header that sets a jump.
 * @param vm  The thread data.
 * @param l   The label to jump to on error.
 */
#define BC_FUNC_HEADER(vm, l)     \
	do                            \
	{                             \
		BC_SETJMP(vm, l);         \
		vm->err = BCL_ERROR_NONE; \
	}                             \
	while (0)

/**
 * A footer for functions that do not return an error code.
 */
#define BC_FUNC_FOOTER_NO_ERR(vm) \
	do                            \
	{                             \
		BC_UNSETJMP(vm);          \
	}                             \
	while (0)

/**
 * A footer for functions that *do* return an error code.
 * @param vm  The thread data.
 * @param e   The error variable to set.
 */
#define BC_FUNC_FOOTER(vm, e)      \
	do                             \
	{                              \
		e = vm->err;               \
		BC_FUNC_FOOTER_NO_ERR(vm); \
	}                              \
	while (0)

/**
 * A footer that sets up n based the value of e and sets up the return value in
 * idx.
 * @param c    The context.
 * @param e    The error.
 * @param bn   The number.
 * @param idx  The idx to set as the return value.
 */
#define BC_MAYBE_SETUP(c, e, bn, idx)                                          \
	do                                                                         \
	{                                                                          \
		if (BC_ERR((e) != BCL_ERROR_NONE))                                     \
		{                                                                      \
			if (BCL_NUM_ARRAY_NP(bn) != NULL) bc_num_free(BCL_NUM_NUM_NP(bn)); \
			idx.i = 0 - (size_t) (e);                                          \
		}                                                                      \
		else idx = bcl_num_insert(c, &(bn));                                   \
	}                                                                          \
	while (0)

/**
 * A header to check the context and return an error encoded in a number if it
 * is bad.
 * @param c  The context.
 */
#define BC_CHECK_CTXT(vm, c)                                   \
	do                                                         \
	{                                                          \
		c = bcl_contextHelper(vm);                             \
		if (BC_ERR(c == NULL))                                 \
		{                                                      \
			BclNumber n_num_;                                  \
			n_num_.i = 0 - (size_t) BCL_ERROR_INVALID_CONTEXT; \
			return n_num_;                                     \
		}                                                      \
	}                                                          \
	while (0)

/**
 * A header to check the context and return an error directly if it is bad.
 * @param c  The context.
 */
#define BC_CHECK_CTXT_ERR(vm, c)              \
	do                                        \
	{                                         \
		c = bcl_contextHelper(vm);            \
		if (BC_ERR(c == NULL))                \
		{                                     \
			return BCL_ERROR_INVALID_CONTEXT; \
		}                                     \
	}                                         \
	while (0)

/**
 * A header to check the context and abort if it is bad.
 * @param c  The context.
 */
#define BC_CHECK_CTXT_ASSERT(vm, c) \
	do                              \
	{                               \
		c = bcl_contextHelper(vm);  \
		assert(c != NULL);          \
	}                               \
	while (0)

/**
 * A header to check the number in the context and return an error encoded as a
 * @param c  The context.
 * number if it is bad.
 * @param n  The BclNumber.
 */
#define BC_CHECK_NUM(c, n)                                         \
	do                                                             \
	{                                                              \
		size_t no_gen_ = BCL_NO_GEN(n);                            \
		if (BC_ERR(no_gen_ >= (c)->nums.len))                      \
		{                                                          \
			if ((n).i > 0 - (size_t) BCL_ERROR_NELEMS) return (n); \
			else                                                   \
			{                                                      \
				BclNumber n_num_;                                  \
				n_num_.i = 0 - (size_t) BCL_ERROR_INVALID_NUM;     \
				return n_num_;                                     \
			}                                                      \
		}                                                          \
		BCL_CHECK_NUM_GEN(c, n);                                   \
	}                                                              \
	while (0)

//clang-format off

/**
 * A header to check the number in the context and return an error directly if
 * it is bad.
 * @param c  The context.
 * @param n  The BclNumber.
 */
#define BC_CHECK_NUM_ERR(c, n)                         \
	do                                                 \
	{                                                  \
		size_t no_gen_ = BCL_NO_GEN(n);                \
		if (BC_ERR(no_gen_ >= (c)->nums.len))          \
		{                                              \
			if ((n).i > 0 - (size_t) BCL_ERROR_NELEMS) \
			{                                          \
				return (BclError) (0 - (n).i);         \
			}                                          \
			else return BCL_ERROR_INVALID_NUM;         \
		}                                              \
		BCL_CHECK_NUM_GEN(c, n);                       \
	}                                                  \
	while (0)

//clang-format on

/**
 * Grows the context's nums array if necessary.
 * @param c  The context.
 */
#define BCL_GROW_NUMS(c)                  \
	do                                    \
	{                                     \
		if ((c)->free_nums.len == 0)      \
		{                                 \
			bc_vec_grow(&((c)->nums), 1); \
		}                                 \
	}                                     \
	while (0)

/**
 * Frees a BcNum for bcl. This is a destructor.
 * @param num  The BcNum to free, as a void pointer.
 */
void
bcl_num_destruct(void* num);

/// The actual context struct.
typedef struct BclCtxt
{
	/// The context's scale.
	size_t scale;

	/// The context's ibase.
	size_t ibase;

	/// The context's obase.
	size_t obase;

	/// A vector of BcNum numbers.
	BcVec nums;

	/// A vector of BclNumbers. These are the indices in nums that are currently
	/// not used (because they were freed).
	BcVec free_nums;

} BclCtxt;

/**
 * Returns the @a BcVm for the current thread.
 * @return  The vm for the current thread.
 */
BcVm*
bcl_getspecific(void);

#ifndef _WIN32

typedef pthread_key_t BclTls;

#else // _WIN32

typedef DWORD BclTls;

#endif // _WIN32

#endif // LIBBC_PRIVATE_H
