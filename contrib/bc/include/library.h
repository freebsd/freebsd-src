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
 * The private header for the bc library.
 *
 */

#ifndef LIBBC_PRIVATE_H
#define LIBBC_PRIVATE_H

#include <bcl.h>

#include <num.h>

/**
 * A header for functions that need to lock and setjmp(). It also sets the
 * variable that tells bcl that it is running.
 * @param l  The label to jump to on error.
 */
#define BC_FUNC_HEADER_LOCK(l)   \
	do                           \
	{                            \
		BC_SIG_LOCK;             \
		BC_SETJMP_LOCKED(l);     \
		vm.err = BCL_ERROR_NONE; \
		vm.running = 1;          \
	}                            \
	while (0)

/**
 * A footer to unlock and stop the jumping if an error happened. It also sets
 * the variable that tells bcl that it is running.
 * @param e  The error variable to set.
 */
#define BC_FUNC_FOOTER_UNLOCK(e) \
	do                           \
	{                            \
		BC_SIG_ASSERT_LOCKED;    \
		e = vm.err;              \
		vm.running = 0;          \
		BC_UNSETJMP;             \
		BC_LONGJMP_STOP;         \
		vm.sig_lock = 0;         \
	}                            \
	while (0)

/**
 * A header that sets a jump and sets running.
 * @param l  The label to jump to on error.
 */
#define BC_FUNC_HEADER(l)        \
	do                           \
	{                            \
		BC_SETJMP(l);            \
		vm.err = BCL_ERROR_NONE; \
		vm.running = 1;          \
	}                            \
	while (0)

/**
 * A header that assumes that signals are already locked. It sets a jump and
 * running.
 * @param l  The label to jump to on error.
 */
#define BC_FUNC_HEADER_INIT(l)   \
	do                           \
	{                            \
		BC_SETJMP_LOCKED(l);     \
		vm.err = BCL_ERROR_NONE; \
		vm.running = 1;          \
	}                            \
	while (0)

/**
 * A footer for functions that do not return an error code. It clears running
 * and unlocks the signals. It also stops the jumping.
 */
#define BC_FUNC_FOOTER_NO_ERR \
	do                        \
	{                         \
		vm.running = 0;       \
		BC_UNSETJMP;          \
		BC_LONGJMP_STOP;      \
		vm.sig_lock = 0;      \
	}                         \
	while (0)

/**
 * A footer for functions that *do* return an error code. It clears running and
 * unlocks the signals. It also stops the jumping.
 * @param e  The error variable to set.
 */
#define BC_FUNC_FOOTER(e)      \
	do                         \
	{                          \
		e = vm.err;            \
		BC_FUNC_FOOTER_NO_ERR; \
	}                          \
	while (0)

/**
 * A footer that sets up n based the value of e and sets up the return value in
 * idx.
 * @param c    The context.
 * @param e    The error.
 * @param n    The number.
 * @param idx  The idx to set as the return value.
 */
#define BC_MAYBE_SETUP(c, e, n, idx)                \
	do                                              \
	{                                               \
		if (BC_ERR((e) != BCL_ERROR_NONE))          \
		{                                           \
			if ((n).num != NULL) bc_num_free(&(n)); \
			idx.i = 0 - (size_t) (e);               \
		}                                           \
		else idx = bcl_num_insert(c, &(n));         \
	}                                               \
	while (0)

/**
 * A header to check the context and return an error encoded in a number if it
 * is bad.
 * @param c  The context.
 */
#define BC_CHECK_CTXT(c)                                      \
	do                                                        \
	{                                                         \
		c = bcl_context();                                    \
		if (BC_ERR(c == NULL))                                \
		{                                                     \
			BclNumber n_num;                                  \
			n_num.i = 0 - (size_t) BCL_ERROR_INVALID_CONTEXT; \
			return n_num;                                     \
		}                                                     \
	}                                                         \
	while (0)

/**
 * A header to check the context and return an error directly if it is bad.
 * @param c  The context.
 */
#define BC_CHECK_CTXT_ERR(c)                  \
	do                                        \
	{                                         \
		c = bcl_context();                    \
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
#define BC_CHECK_CTXT_ASSERT(c) \
	do                          \
	{                           \
		c = bcl_context();      \
		assert(c != NULL);      \
	}                           \
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
		if (BC_ERR((n).i >= (c)->nums.len))                        \
		{                                                          \
			if ((n).i > 0 - (size_t) BCL_ERROR_NELEMS) return (n); \
			else                                                   \
			{                                                      \
				BclNumber n_num;                                   \
				n_num.i = 0 - (size_t) BCL_ERROR_INVALID_NUM;      \
				return n_num;                                      \
			}                                                      \
		}                                                          \
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
		if (BC_ERR((n).i >= (c)->nums.len))            \
		{                                              \
			if ((n).i > 0 - (size_t) BCL_ERROR_NELEMS) \
			{                                          \
				return (BclError) (0 - (n).i);         \
			}                                          \
			else return BCL_ERROR_INVALID_NUM;         \
		}                                              \
	}                                                  \
	while (0)

//clang-format on

/**
 * Turns a BclNumber into a BcNum.
 * @param c  The context.
 * @param n  The BclNumber.
 */
#define BC_NUM(c, n) ((BcNum*) bc_vec_item(&(c)->nums, (n).i))

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

#endif // LIBBC_PRIVATE_H
