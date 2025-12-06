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
 * The public functions for libbc.
 *
 */

#if BC_ENABLE_LIBRARY

#include <setjmp.h>
#include <string.h>
#include <time.h>

#include <bcl.h>

#include <library.h>
#include <num.h>
#include <vm.h>

#ifndef _WIN32
#include <pthread.h>
#endif // _WIN32

// The asserts in this file are important to testing; in many cases, the test
// would not work without the asserts, so don't remove them without reason.
//
// Also, there are many uses of bc_num_clear() here; that is because numbers are
// being reused, and a clean slate is required.
//
// Also, there are a bunch of BC_UNSETJMP between calls to bc_num_init(). That
// is because locals are being initialized, and unlike bc proper, this code
// cannot assume that allocation failures are fatal. So we have to reset the
// jumps every time to ensure that the locals will be correct after jumping.

#if BC_ENABLE_MEMCHECK

BC_NORETURN void
bcl_invalidGeneration(void)
{
	abort();
}

BC_NORETURN void
bcl_nonexistentNum(void)
{
	abort();
}

BC_NORETURN void
bcl_numIdxOutOfRange(void)
{
	abort();
}

#endif // BC_ENABLE_MEMCHECK

static BclTls* tls = NULL;
static BclTls tls_real;

BclError
bcl_start(void)
{
#ifndef _WIN32

	int r;

	if (tls != NULL) return BCL_ERROR_NONE;

	r = pthread_key_create(&tls_real, NULL);
	if (BC_ERR(r != 0)) return BCL_ERROR_FATAL_ALLOC_ERR;

#else // _WIN32

	if (tls != NULL) return BCL_ERROR_NONE;

	tls_real = TlsAlloc();
	if (BC_ERR(tls_real == TLS_OUT_OF_INDEXES))
	{
		return BCL_ERROR_FATAL_ALLOC_ERR;
	}

#endif // _WIN32

	tls = &tls_real;

	return BCL_ERROR_NONE;
}

/**
 * Sets the thread-specific data for the thread.
 * @param vm  The @a BcVm to set as the thread data.
 * @return    An error code, if any.
 */
static BclError
bcl_setspecific(BcVm* vm)
{
#ifndef _WIN32

	int r;

	assert(tls != NULL);

	r = pthread_setspecific(tls_real, vm);
	if (BC_ERR(r != 0)) return BCL_ERROR_FATAL_ALLOC_ERR;

#else // _WIN32

	bool r;

	assert(tls != NULL);

	r = TlsSetValue(tls_real, vm);
	if (BC_ERR(!r)) return BCL_ERROR_FATAL_ALLOC_ERR;

#endif // _WIN32

	return BCL_ERROR_NONE;
}

BcVm*
bcl_getspecific(void)
{
	BcVm* vm;

#ifndef _WIN32

	vm = pthread_getspecific(tls_real);

#else // _WIN32

	vm = TlsGetValue(tls_real);

#endif // _WIN32

	return vm;
}

BclError
bcl_init(void)
{
	BclError e = BCL_ERROR_NONE;
	BcVm* vm;

	assert(tls != NULL);

	vm = bcl_getspecific();
	if (vm != NULL)
	{
		assert(vm->refs >= 1);

		vm->refs += 1;

		return e;
	}

	vm = bc_vm_malloc(sizeof(BcVm));
	if (BC_ERR(vm == NULL)) return BCL_ERROR_FATAL_ALLOC_ERR;

	e = bcl_setspecific(vm);
	if (BC_ERR(e != BCL_ERROR_NONE))
	{
		free(vm);
		return e;
	}

	memset(vm, 0, sizeof(BcVm));

	vm->refs += 1;

	assert(vm->refs == 1);

	// Setting these to NULL ensures that if an error occurs, we only free what
	// is necessary.
	vm->ctxts.v = NULL;
	vm->jmp_bufs.v = NULL;
	vm->out.v = NULL;

	vm->abrt = false;
	vm->leading_zeroes = false;
	vm->digit_clamp = true;

	// The jmp_bufs always has to be initialized first.
	bc_vec_init(&vm->jmp_bufs, sizeof(sigjmp_buf), BC_DTOR_NONE);

	BC_FUNC_HEADER(vm, err);

	bc_vm_init();

	bc_vec_init(&vm->ctxts, sizeof(BclContext), BC_DTOR_NONE);
	bc_vec_init(&vm->out, sizeof(uchar), BC_DTOR_NONE);

#if BC_ENABLE_EXTRA_MATH

	// We need to seed this in case /dev/random and /dev/urandom don't work.
	srand((unsigned int) time(NULL));
	bc_rand_init(&vm->rng);

#endif // BC_ENABLE_EXTRA_MATH

err:

	BC_FUNC_FOOTER(vm, e);

	// This is why we had to set them to NULL.
	if (BC_ERR(vm != NULL && vm->err))
	{
		if (vm->out.v != NULL) bc_vec_free(&vm->out);
		if (vm->jmp_bufs.v != NULL) bc_vec_free(&vm->jmp_bufs);
		if (vm->ctxts.v != NULL) bc_vec_free(&vm->ctxts);
		bcl_setspecific(NULL);
		free(vm);
	}

	return e;
}

BclError
bcl_pushContext(BclContext ctxt)
{
	BclError e = BCL_ERROR_NONE;
	BcVm* vm = bcl_getspecific();

	BC_FUNC_HEADER(vm, err);

	bc_vec_push(&vm->ctxts, &ctxt);

err:

	BC_FUNC_FOOTER(vm, e);
	return e;
}

void
bcl_popContext(void)
{
	BcVm* vm = bcl_getspecific();

	if (vm->ctxts.len) bc_vec_pop(&vm->ctxts);
}

static BclContext
bcl_contextHelper(BcVm* vm)
{
	if (!vm->ctxts.len) return NULL;
	return *((BclContext*) bc_vec_top(&vm->ctxts));
}

BclContext
bcl_context(void)
{
	BcVm* vm = bcl_getspecific();
	return bcl_contextHelper(vm);
}

void
bcl_free(void)
{
	size_t i;
	BcVm* vm = bcl_getspecific();

	vm->refs -= 1;
	if (vm->refs) return;

#if BC_ENABLE_EXTRA_MATH
	bc_rand_free(&vm->rng);
#endif // BC_ENABLE_EXTRA_MATH
	bc_vec_free(&vm->out);

	for (i = 0; i < vm->ctxts.len; ++i)
	{
		BclContext ctxt = *((BclContext*) bc_vec_item(&vm->ctxts, i));
		bcl_ctxt_free(ctxt);
	}

	bc_vec_free(&vm->ctxts);

	bc_vm_atexit();

	free(vm);
	bcl_setspecific(NULL);
}

void
bcl_end(void)
{
#ifndef _WIN32

	// We ignore the return value.
	pthread_key_delete(tls_real);

#else // _WIN32

	// We ignore the return value.
	TlsFree(tls_real);

#endif // _WIN32

	tls = NULL;
}

void
bcl_gc(void)
{
	bc_vm_freeTemps();
}

bool
bcl_abortOnFatalError(void)
{
	BcVm* vm = bcl_getspecific();

	return vm->abrt;
}

void
bcl_setAbortOnFatalError(bool abrt)
{
	BcVm* vm = bcl_getspecific();

	vm->abrt = abrt;
}

bool
bcl_leadingZeroes(void)
{
	BcVm* vm = bcl_getspecific();

	return vm->leading_zeroes;
}

void
bcl_setLeadingZeroes(bool leadingZeroes)
{
	BcVm* vm = bcl_getspecific();

	vm->leading_zeroes = leadingZeroes;
}

bool
bcl_digitClamp(void)
{
	BcVm* vm = bcl_getspecific();

	return vm->digit_clamp;
}

void
bcl_setDigitClamp(bool digitClamp)
{
	BcVm* vm = bcl_getspecific();

	vm->digit_clamp = digitClamp;
}

BclContext
bcl_ctxt_create(void)
{
	BcVm* vm = bcl_getspecific();
	BclContext ctxt = NULL;

	BC_FUNC_HEADER(vm, err);

	// We want the context to be free of any interference of other parties, so
	// malloc() is appropriate here.
	ctxt = bc_vm_malloc(sizeof(BclCtxt));

	bc_vec_init(&ctxt->nums, sizeof(BclNum), BC_DTOR_BCL_NUM);
	bc_vec_init(&ctxt->free_nums, sizeof(BclNumber), BC_DTOR_NONE);

	ctxt->scale = 0;
	ctxt->ibase = 10;
	ctxt->obase = 10;

err:

	if (BC_ERR(vm->err && ctxt != NULL))
	{
		if (ctxt->nums.v != NULL) bc_vec_free(&ctxt->nums);
		free(ctxt);
		ctxt = NULL;
	}

	BC_FUNC_FOOTER_NO_ERR(vm);

	return ctxt;
}

void
bcl_ctxt_free(BclContext ctxt)
{
	bc_vec_free(&ctxt->free_nums);
	bc_vec_free(&ctxt->nums);
	free(ctxt);
}

void
bcl_ctxt_freeNums(BclContext ctxt)
{
	bc_vec_popAll(&ctxt->nums);
	bc_vec_popAll(&ctxt->free_nums);
}

size_t
bcl_ctxt_scale(BclContext ctxt)
{
	return ctxt->scale;
}

void
bcl_ctxt_setScale(BclContext ctxt, size_t scale)
{
	ctxt->scale = scale;
}

size_t
bcl_ctxt_ibase(BclContext ctxt)
{
	return ctxt->ibase;
}

void
bcl_ctxt_setIbase(BclContext ctxt, size_t ibase)
{
	if (ibase < BC_NUM_MIN_BASE) ibase = BC_NUM_MIN_BASE;
	else if (ibase > BC_NUM_MAX_IBASE) ibase = BC_NUM_MAX_IBASE;
	ctxt->ibase = ibase;
}

size_t
bcl_ctxt_obase(BclContext ctxt)
{
	return ctxt->obase;
}

void
bcl_ctxt_setObase(BclContext ctxt, size_t obase)
{
	ctxt->obase = obase;
}

BclError
bcl_err(BclNumber n)
{
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT_ERR(vm, ctxt);

	// We need to clear the top byte in memcheck mode. We can do this because
	// the parameter is a copy.
	BCL_CLEAR_GEN(n);

	// Errors are encoded as (0 - error_code). If the index is in that range, it
	// is an encoded error.
	if (n.i >= ctxt->nums.len)
	{
		if (n.i > 0 - (size_t) BCL_ERROR_NELEMS) return (BclError) (0 - n.i);
		else return BCL_ERROR_INVALID_NUM;
	}
	else return BCL_ERROR_NONE;
}

/**
 * Inserts a BcNum into a context's list of numbers.
 * @param ctxt  The context to insert into.
 * @param n     The BcNum to insert.
 * @return      The resulting BclNumber from the insert.
 */
static BclNumber
bcl_num_insert(BclContext ctxt, BclNum* restrict n)
{
	BclNumber idx;

	// If there is a free spot...
	if (ctxt->free_nums.len)
	{
		BclNum* ptr;

		// Get the index of the free spot and remove it.
		idx = *((BclNumber*) bc_vec_top(&ctxt->free_nums));
		bc_vec_pop(&ctxt->free_nums);

		// Copy the number into the spot.
		ptr = bc_vec_item(&ctxt->nums, idx.i);

		memcpy(BCL_NUM_NUM(ptr), n, sizeof(BcNum));

#if BC_ENABLE_MEMCHECK

		ptr->gen_idx += 1;

		if (ptr->gen_idx == UCHAR_MAX)
		{
			ptr->gen_idx = 0;
		}

		idx.i |= (ptr->gen_idx << ((sizeof(size_t) - 1) * CHAR_BIT));

#endif // BC_ENABLE_MEMCHECK
	}
	else
	{
#if BC_ENABLE_MEMCHECK
		n->gen_idx = 0;
#endif // BC_ENABLE_MEMCHECK

		// Just push the number onto the vector because the generation index is
		// 0.
		idx.i = ctxt->nums.len;
		bc_vec_push(&ctxt->nums, n);
	}

	return idx;
}

BclNumber
bcl_num_create(void)
{
	BclError e = BCL_ERROR_NONE;
	BclNum n;
	BclNumber idx;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT(vm, ctxt);

	BC_FUNC_HEADER(vm, err);

	BCL_GROW_NUMS(ctxt);

	bc_num_init(BCL_NUM_NUM_NP(n), BC_NUM_DEF_SIZE);

err:

	BC_FUNC_FOOTER(vm, e);
	BC_MAYBE_SETUP(ctxt, e, n, idx);

	return idx;
}

/**
 * Destructs a number and marks its spot as free.
 * @param ctxt  The context.
 * @param n     The index of the number.
 * @param num   The number to destroy.
 */
static void
bcl_num_dtor(BclContext ctxt, BclNumber n, BclNum* restrict num)
{
	assert(num != NULL && BCL_NUM_ARRAY(num) != NULL);

	BCL_CLEAR_GEN(n);

	bcl_num_destruct(num);
	bc_vec_push(&ctxt->free_nums, &n);

#if BC_ENABLE_MEMCHECK
	num->n.num = NULL;
#endif // BC_ENABLE_MEMCHECK
}

void
bcl_num_free(BclNumber n)
{
	BclNum* num;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT_ASSERT(vm, ctxt);

	BCL_CHECK_NUM_VALID(ctxt, n);

	assert(BCL_NO_GEN(n) < ctxt->nums.len);

	num = BCL_NUM(ctxt, n);

	bcl_num_dtor(ctxt, n, num);
}

BclError
bcl_copy(BclNumber d, BclNumber s)
{
	BclError e = BCL_ERROR_NONE;
	BclNum* dest;
	BclNum* src;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT_ERR(vm, ctxt);

	BCL_CHECK_NUM_VALID(ctxt, d);
	BCL_CHECK_NUM_VALID(ctxt, s);

	BC_FUNC_HEADER(vm, err);

	assert(BCL_NO_GEN(d) < ctxt->nums.len);
	assert(BCL_NO_GEN(s) < ctxt->nums.len);

	dest = BCL_NUM(ctxt, d);
	src = BCL_NUM(ctxt, s);

	assert(dest != NULL && src != NULL);
	assert(BCL_NUM_ARRAY(dest) != NULL && BCL_NUM_ARRAY(src) != NULL);

	bc_num_copy(BCL_NUM_NUM(dest), BCL_NUM_NUM(src));

err:

	BC_FUNC_FOOTER(vm, e);

	return e;
}

BclNumber
bcl_dup(BclNumber s)
{
	BclError e = BCL_ERROR_NONE;
	BclNum *src, dest;
	BclNumber idx;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT(vm, ctxt);

	BCL_CHECK_NUM_VALID(ctxt, s);

	BC_FUNC_HEADER(vm, err);

	BCL_GROW_NUMS(ctxt);

	assert(BCL_NO_GEN(s) < ctxt->nums.len);

	src = BCL_NUM(ctxt, s);

	assert(src != NULL && BCL_NUM_NUM(src) != NULL);

	// Copy the number.
	bc_num_clear(BCL_NUM_NUM(&dest));
	bc_num_createCopy(BCL_NUM_NUM(&dest), BCL_NUM_NUM(src));

err:

	BC_FUNC_FOOTER(vm, e);
	BC_MAYBE_SETUP(ctxt, e, dest, idx);

	return idx;
}

void
bcl_num_destruct(void* num)
{
	BclNum* n = (BclNum*) num;

	assert(n != NULL);

	if (BCL_NUM_ARRAY(n) == NULL) return;

	bc_num_free(BCL_NUM_NUM(n));
	bc_num_clear(BCL_NUM_NUM(n));
}

bool
bcl_num_neg(BclNumber n)
{
	BclNum* num;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT_ASSERT(vm, ctxt);

	BCL_CHECK_NUM_VALID(ctxt, n);

	assert(BCL_NO_GEN(n) < ctxt->nums.len);

	num = BCL_NUM(ctxt, n);

	assert(num != NULL && BCL_NUM_ARRAY(num) != NULL);

	return BC_NUM_NEG(BCL_NUM_NUM(num)) != 0;
}

void
bcl_num_setNeg(BclNumber n, bool neg)
{
	BclNum* num;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT_ASSERT(vm, ctxt);

	BCL_CHECK_NUM_VALID(ctxt, n);

	assert(BCL_NO_GEN(n) < ctxt->nums.len);

	num = BCL_NUM(ctxt, n);

	assert(num != NULL && BCL_NUM_ARRAY(num) != NULL);

	BCL_NUM_NUM(num)->rdx = BC_NUM_NEG_VAL(BCL_NUM_NUM(num), neg);
}

size_t
bcl_num_scale(BclNumber n)
{
	BclNum* num;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT_ASSERT(vm, ctxt);

	BCL_CHECK_NUM_VALID(ctxt, n);

	assert(BCL_NO_GEN(n) < ctxt->nums.len);

	num = BCL_NUM(ctxt, n);

	assert(num != NULL && BCL_NUM_ARRAY(num) != NULL);

	return bc_num_scale(BCL_NUM_NUM(num));
}

BclError
bcl_num_setScale(BclNumber n, size_t scale)
{
	BclError e = BCL_ERROR_NONE;
	BclNum* nptr;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT_ERR(vm, ctxt);

	BC_CHECK_NUM_ERR(ctxt, n);

	BCL_CHECK_NUM_VALID(ctxt, n);

	BC_FUNC_HEADER(vm, err);

	assert(BCL_NO_GEN(n) < ctxt->nums.len);

	nptr = BCL_NUM(ctxt, n);

	assert(nptr != NULL && BCL_NUM_ARRAY(nptr) != NULL);

	if (scale > BCL_NUM_NUM(nptr)->scale)
	{
		bc_num_extend(BCL_NUM_NUM(nptr), scale - BCL_NUM_NUM(nptr)->scale);
	}
	else if (scale < BCL_NUM_NUM(nptr)->scale)
	{
		bc_num_truncate(BCL_NUM_NUM(nptr), BCL_NUM_NUM(nptr)->scale - scale);
	}

err:

	BC_FUNC_FOOTER(vm, e);

	return e;
}

size_t
bcl_num_len(BclNumber n)
{
	BclNum* num;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT_ASSERT(vm, ctxt);

	BCL_CHECK_NUM_VALID(ctxt, n);

	assert(BCL_NO_GEN(n) < ctxt->nums.len);

	num = BCL_NUM(ctxt, n);

	assert(num != NULL && BCL_NUM_ARRAY(num) != NULL);

	return bc_num_len(BCL_NUM_NUM(num));
}

static BclError
bcl_bigdig_helper(BclNumber n, BclBigDig* result, bool destruct)
{
	BclError e = BCL_ERROR_NONE;
	BclNum* num;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT_ERR(vm, ctxt);

	BCL_CHECK_NUM_VALID(ctxt, n);

	BC_FUNC_HEADER(vm, err);

	assert(BCL_NO_GEN(n) < ctxt->nums.len);
	assert(result != NULL);

	num = BCL_NUM(ctxt, n);

	assert(num != NULL && BCL_NUM_ARRAY(num) != NULL);

	*result = bc_num_bigdig(BCL_NUM_NUM(num));

err:

	if (destruct)
	{
		bcl_num_dtor(ctxt, n, num);
	}

	BC_FUNC_FOOTER(vm, e);

	return e;
}

BclError
bcl_bigdig(BclNumber n, BclBigDig* result)
{
	return bcl_bigdig_helper(n, result, true);
}

BclError
bcl_bigdig_keep(BclNumber n, BclBigDig* result)
{
	return bcl_bigdig_helper(n, result, false);
}

BclNumber
bcl_bigdig2num(BclBigDig val)
{
	BclError e = BCL_ERROR_NONE;
	BclNum n;
	BclNumber idx;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT(vm, ctxt);

	BC_FUNC_HEADER(vm, err);

	BCL_GROW_NUMS(ctxt);

	bc_num_createFromBigdig(BCL_NUM_NUM_NP(n), val);

err:

	BC_FUNC_FOOTER(vm, e);
	BC_MAYBE_SETUP(ctxt, e, n, idx);

	return idx;
}

/**
 * Sets up and executes a binary operator operation.
 * @param a         The first operand.
 * @param b         The second operand.
 * @param op        The operation.
 * @param req       The function to get the size of the result for
 *                  preallocation.
 * @param destruct  True if the parameters should be consumed, false otherwise.
 * @return          The result of the operation.
 */
static BclNumber
bcl_binary(BclNumber a, BclNumber b, const BcNumBinaryOp op,
           const BcNumBinaryOpReq req, bool destruct)
{
	BclError e = BCL_ERROR_NONE;
	BclNum* aptr;
	BclNum* bptr;
	BclNum c;
	BclNumber idx;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT(vm, ctxt);

	BC_CHECK_NUM(ctxt, a);
	BC_CHECK_NUM(ctxt, b);

	BC_FUNC_HEADER(vm, err);

	BCL_GROW_NUMS(ctxt);

	assert(BCL_NO_GEN(a) < ctxt->nums.len && BCL_NO_GEN(b) < ctxt->nums.len);

	aptr = BCL_NUM(ctxt, a);
	bptr = BCL_NUM(ctxt, b);

	assert(aptr != NULL && bptr != NULL);
	assert(BCL_NUM_ARRAY(aptr) != NULL && BCL_NUM_ARRAY(bptr) != NULL);

	// Clear and initialize the result.
	bc_num_clear(BCL_NUM_NUM_NP(c));
	bc_num_init(BCL_NUM_NUM_NP(c),
	            req(BCL_NUM_NUM(aptr), BCL_NUM_NUM(bptr), ctxt->scale));

	op(BCL_NUM_NUM(aptr), BCL_NUM_NUM(bptr), BCL_NUM_NUM_NP(c), ctxt->scale);

err:

	if (destruct)
	{
		// Eat the operands.
		bcl_num_dtor(ctxt, a, aptr);
		if (b.i != a.i) bcl_num_dtor(ctxt, b, bptr);
	}

	BC_FUNC_FOOTER(vm, e);
	BC_MAYBE_SETUP(ctxt, e, c, idx);

	return idx;
}

BclNumber
bcl_add(BclNumber a, BclNumber b)
{
	return bcl_binary(a, b, bc_num_add, bc_num_addReq, true);
}

BclNumber
bcl_add_keep(BclNumber a, BclNumber b)
{
	return bcl_binary(a, b, bc_num_add, bc_num_addReq, false);
}

BclNumber
bcl_sub(BclNumber a, BclNumber b)
{
	return bcl_binary(a, b, bc_num_sub, bc_num_addReq, true);
}

BclNumber
bcl_sub_keep(BclNumber a, BclNumber b)
{
	return bcl_binary(a, b, bc_num_sub, bc_num_addReq, false);
}

BclNumber
bcl_mul(BclNumber a, BclNumber b)
{
	return bcl_binary(a, b, bc_num_mul, bc_num_mulReq, true);
}

BclNumber
bcl_mul_keep(BclNumber a, BclNumber b)
{
	return bcl_binary(a, b, bc_num_mul, bc_num_mulReq, false);
}

BclNumber
bcl_div(BclNumber a, BclNumber b)
{
	return bcl_binary(a, b, bc_num_div, bc_num_divReq, true);
}

BclNumber
bcl_div_keep(BclNumber a, BclNumber b)
{
	return bcl_binary(a, b, bc_num_div, bc_num_divReq, false);
}

BclNumber
bcl_mod(BclNumber a, BclNumber b)
{
	return bcl_binary(a, b, bc_num_mod, bc_num_divReq, true);
}

BclNumber
bcl_mod_keep(BclNumber a, BclNumber b)
{
	return bcl_binary(a, b, bc_num_mod, bc_num_divReq, false);
}

BclNumber
bcl_pow(BclNumber a, BclNumber b)
{
	return bcl_binary(a, b, bc_num_pow, bc_num_powReq, true);
}

BclNumber
bcl_pow_keep(BclNumber a, BclNumber b)
{
	return bcl_binary(a, b, bc_num_pow, bc_num_powReq, false);
}

BclNumber
bcl_lshift(BclNumber a, BclNumber b)
{
	return bcl_binary(a, b, bc_num_lshift, bc_num_placesReq, true);
}

BclNumber
bcl_lshift_keep(BclNumber a, BclNumber b)
{
	return bcl_binary(a, b, bc_num_lshift, bc_num_placesReq, false);
}

BclNumber
bcl_rshift(BclNumber a, BclNumber b)
{
	return bcl_binary(a, b, bc_num_rshift, bc_num_placesReq, true);
}

BclNumber
bcl_rshift_keep(BclNumber a, BclNumber b)
{
	return bcl_binary(a, b, bc_num_rshift, bc_num_placesReq, false);
}

static BclNumber
bcl_sqrt_helper(BclNumber a, bool destruct)
{
	BclError e = BCL_ERROR_NONE;
	BclNum* aptr;
	BclNum b;
	BclNumber idx;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT(vm, ctxt);

	BC_CHECK_NUM(ctxt, a);

	BC_FUNC_HEADER(vm, err);

	BCL_GROW_NUMS(ctxt);

	assert(BCL_NO_GEN(a) < ctxt->nums.len);

	aptr = BCL_NUM(ctxt, a);

	bc_num_sqrt(BCL_NUM_NUM(aptr), BCL_NUM_NUM_NP(b), ctxt->scale);

err:

	if (destruct)
	{
		bcl_num_dtor(ctxt, a, aptr);
	}

	BC_FUNC_FOOTER(vm, e);
	BC_MAYBE_SETUP(ctxt, e, b, idx);

	return idx;
}

BclNumber
bcl_sqrt(BclNumber a)
{
	return bcl_sqrt_helper(a, true);
}

BclNumber
bcl_sqrt_keep(BclNumber a)
{
	return bcl_sqrt_helper(a, false);
}

static BclError
bcl_divmod_helper(BclNumber a, BclNumber b, BclNumber* c, BclNumber* d,
                  bool destruct)
{
	BclError e = BCL_ERROR_NONE;
	size_t req;
	BclNum* aptr;
	BclNum* bptr;
	BclNum cnum, dnum;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT_ERR(vm, ctxt);

	BC_CHECK_NUM_ERR(ctxt, a);
	BC_CHECK_NUM_ERR(ctxt, b);

	BC_FUNC_HEADER(vm, err);

	BCL_GROW_NUMS(ctxt);

	assert(c != NULL && d != NULL);

	aptr = BCL_NUM(ctxt, a);
	bptr = BCL_NUM(ctxt, b);

	assert(aptr != NULL && bptr != NULL);
	assert(BCL_NUM_ARRAY(aptr) != NULL && BCL_NUM_ARRAY(bptr) != NULL);

	bc_num_clear(BCL_NUM_NUM_NP(cnum));
	bc_num_clear(BCL_NUM_NUM_NP(dnum));

	req = bc_num_divReq(BCL_NUM_NUM(aptr), BCL_NUM_NUM(bptr), ctxt->scale);

	// Initialize the numbers.
	bc_num_init(BCL_NUM_NUM_NP(cnum), req);
	BC_UNSETJMP(vm);
	BC_SETJMP(vm, err);
	bc_num_init(BCL_NUM_NUM_NP(dnum), req);

	bc_num_divmod(BCL_NUM_NUM(aptr), BCL_NUM_NUM(bptr), BCL_NUM_NUM_NP(cnum),
	              BCL_NUM_NUM_NP(dnum), ctxt->scale);

err:

	if (destruct)
	{
		// Eat the operands.
		bcl_num_dtor(ctxt, a, aptr);
		if (b.i != a.i) bcl_num_dtor(ctxt, b, bptr);
	}

	// If there was an error...
	if (BC_ERR(vm->err))
	{
		// Free the results.
		if (BCL_NUM_ARRAY_NP(cnum) != NULL) bc_num_free(&cnum);
		if (BCL_NUM_ARRAY_NP(cnum) != NULL) bc_num_free(&dnum);

		// Make sure the return values are invalid.
		c->i = 0 - (size_t) BCL_ERROR_INVALID_NUM;
		d->i = c->i;

		BC_FUNC_FOOTER(vm, e);
	}
	else
	{
		BC_FUNC_FOOTER(vm, e);

		// Insert the results into the context.
		*c = bcl_num_insert(ctxt, &cnum);
		*d = bcl_num_insert(ctxt, &dnum);
	}

	return e;
}

BclError
bcl_divmod(BclNumber a, BclNumber b, BclNumber* c, BclNumber* d)
{
	return bcl_divmod_helper(a, b, c, d, true);
}

BclError
bcl_divmod_keep(BclNumber a, BclNumber b, BclNumber* c, BclNumber* d)
{
	return bcl_divmod_helper(a, b, c, d, false);
}

static BclNumber
bcl_modexp_helper(BclNumber a, BclNumber b, BclNumber c, bool destruct)
{
	BclError e = BCL_ERROR_NONE;
	size_t req;
	BclNum* aptr;
	BclNum* bptr;
	BclNum* cptr;
	BclNum d;
	BclNumber idx;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT(vm, ctxt);

	BC_CHECK_NUM(ctxt, a);
	BC_CHECK_NUM(ctxt, b);
	BC_CHECK_NUM(ctxt, c);

	BC_FUNC_HEADER(vm, err);

	BCL_GROW_NUMS(ctxt);

	assert(BCL_NO_GEN(a) < ctxt->nums.len && BCL_NO_GEN(b) < ctxt->nums.len);
	assert(BCL_NO_GEN(c) < ctxt->nums.len);

	aptr = BCL_NUM(ctxt, a);
	bptr = BCL_NUM(ctxt, b);
	cptr = BCL_NUM(ctxt, c);

	assert(aptr != NULL && bptr != NULL && cptr != NULL);
	assert(BCL_NUM_NUM(aptr) != NULL && BCL_NUM_NUM(bptr) != NULL &&
	       BCL_NUM_NUM(cptr) != NULL);

	// Prepare the result.
	bc_num_clear(BCL_NUM_NUM_NP(d));

	req = bc_num_divReq(BCL_NUM_NUM(aptr), BCL_NUM_NUM(cptr), 0);

	// Initialize the result.
	bc_num_init(BCL_NUM_NUM_NP(d), req);

	bc_num_modexp(BCL_NUM_NUM(aptr), BCL_NUM_NUM(bptr), BCL_NUM_NUM(cptr),
	              BCL_NUM_NUM_NP(d));

err:

	if (destruct)
	{
		// Eat the operands.
		bcl_num_dtor(ctxt, a, aptr);
		if (b.i != a.i) bcl_num_dtor(ctxt, b, bptr);
		if (c.i != a.i && c.i != b.i) bcl_num_dtor(ctxt, c, cptr);
	}

	BC_FUNC_FOOTER(vm, e);
	BC_MAYBE_SETUP(ctxt, e, d, idx);

	return idx;
}

BclNumber
bcl_modexp(BclNumber a, BclNumber b, BclNumber c)
{
	return bcl_modexp_helper(a, b, c, true);
}

BclNumber
bcl_modexp_keep(BclNumber a, BclNumber b, BclNumber c)
{
	return bcl_modexp_helper(a, b, c, false);
}

ssize_t
bcl_cmp(BclNumber a, BclNumber b)
{
	BclNum* aptr;
	BclNum* bptr;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT_ASSERT(vm, ctxt);

	BCL_CHECK_NUM_VALID(ctxt, a);
	BCL_CHECK_NUM_VALID(ctxt, b);

	assert(BCL_NO_GEN(a) < ctxt->nums.len && BCL_NO_GEN(b) < ctxt->nums.len);

	aptr = BCL_NUM(ctxt, a);
	bptr = BCL_NUM(ctxt, b);

	assert(aptr != NULL && bptr != NULL);
	assert(BCL_NUM_NUM(aptr) != NULL && BCL_NUM_NUM(bptr));

	return bc_num_cmp(BCL_NUM_NUM(aptr), BCL_NUM_NUM(bptr));
}

void
bcl_zero(BclNumber n)
{
	BclNum* nptr;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT_ASSERT(vm, ctxt);

	BCL_CHECK_NUM_VALID(ctxt, n);

	assert(BCL_NO_GEN(n) < ctxt->nums.len);

	nptr = BCL_NUM(ctxt, n);

	assert(nptr != NULL && BCL_NUM_NUM(nptr) != NULL);

	bc_num_zero(BCL_NUM_NUM(nptr));
}

void
bcl_one(BclNumber n)
{
	BclNum* nptr;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT_ASSERT(vm, ctxt);

	BCL_CHECK_NUM_VALID(ctxt, n);

	assert(BCL_NO_GEN(n) < ctxt->nums.len);

	nptr = BCL_NUM(ctxt, n);

	assert(nptr != NULL && BCL_NUM_NUM(nptr) != NULL);

	bc_num_one(BCL_NUM_NUM(nptr));
}

BclNumber
bcl_parse(const char* restrict val)
{
	BclError e = BCL_ERROR_NONE;
	BclNum n;
	BclNumber idx;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();
	bool neg;

	BC_CHECK_CTXT(vm, ctxt);

	BC_FUNC_HEADER(vm, err);

	BCL_GROW_NUMS(ctxt);

	assert(val != NULL);

	// We have to take care of negative here because bc's number parsing does
	// not.
	neg = (val[0] == '-');

	if (neg) val += 1;

	if (!bc_num_strValid(val))
	{
		vm->err = BCL_ERROR_PARSE_INVALID_STR;
		goto err;
	}

	// Clear and initialize the number.
	bc_num_clear(BCL_NUM_NUM_NP(n));
	bc_num_init(BCL_NUM_NUM_NP(n), BC_NUM_DEF_SIZE);

	bc_num_parse(BCL_NUM_NUM_NP(n), val, (BcBigDig) ctxt->ibase);

	// Set the negative.
#if BC_ENABLE_MEMCHECK
	n.n.rdx = BC_NUM_NEG_VAL(BCL_NUM_NUM_NP(n), neg);
#else // BC_ENABLE_MEMCHECK
	n.rdx = BC_NUM_NEG_VAL_NP(n, neg);
#endif // BC_ENABLE_MEMCHECK

err:

	BC_FUNC_FOOTER(vm, e);
	BC_MAYBE_SETUP(ctxt, e, n, idx);

	return idx;
}

static char*
bcl_string_helper(BclNumber n, bool destruct)
{
	BclNum* nptr;
	char* str = NULL;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT_ASSERT(vm, ctxt);

	BCL_CHECK_NUM_VALID(ctxt, n);

	if (BC_ERR(BCL_NO_GEN(n) >= ctxt->nums.len)) return str;

	BC_FUNC_HEADER(vm, err);

	assert(BCL_NO_GEN(n) < ctxt->nums.len);

	nptr = BCL_NUM(ctxt, n);

	assert(nptr != NULL && BCL_NUM_NUM(nptr) != NULL);

	// Clear the buffer.
	bc_vec_popAll(&vm->out);

	// Print to the buffer.
	bc_num_print(BCL_NUM_NUM(nptr), (BcBigDig) ctxt->obase, false);
	bc_vec_pushByte(&vm->out, '\0');

	// Just dup the string; the caller is responsible for it.
	str = bc_vm_strdup(vm->out.v);

err:

	if (destruct)
	{
		// Eat the operand.
		bcl_num_dtor(ctxt, n, nptr);
	}

	BC_FUNC_FOOTER_NO_ERR(vm);

	return str;
}

char*
bcl_string(BclNumber n)
{
	return bcl_string_helper(n, true);
}

char*
bcl_string_keep(BclNumber n)
{
	return bcl_string_helper(n, false);
}

#if BC_ENABLE_EXTRA_MATH

static BclNumber
bcl_irand_helper(BclNumber a, bool destruct)
{
	BclError e = BCL_ERROR_NONE;
	BclNum* aptr;
	BclNum b;
	BclNumber idx;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT(vm, ctxt);

	BC_CHECK_NUM(ctxt, a);

	BC_FUNC_HEADER(vm, err);

	BCL_GROW_NUMS(ctxt);

	assert(BCL_NO_GEN(a) < ctxt->nums.len);

	aptr = BCL_NUM(ctxt, a);

	assert(aptr != NULL && BCL_NUM_NUM(aptr) != NULL);

	// Clear and initialize the result.
	bc_num_clear(BCL_NUM_NUM_NP(b));
	bc_num_init(BCL_NUM_NUM_NP(b), BC_NUM_DEF_SIZE);

	bc_num_irand(BCL_NUM_NUM(aptr), BCL_NUM_NUM_NP(b), &vm->rng);

err:

	if (destruct)
	{
		// Eat the operand.
		bcl_num_dtor(ctxt, a, aptr);
	}

	BC_FUNC_FOOTER(vm, e);
	BC_MAYBE_SETUP(ctxt, e, b, idx);

	return idx;
}

BclNumber
bcl_irand(BclNumber a)
{
	return bcl_irand_helper(a, true);
}

BclNumber
bcl_irand_keep(BclNumber a)
{
	return bcl_irand_helper(a, false);
}

/**
 * Helps bcl_frand(). This is separate because the error handling is easier that
 * way. It is also easier to do ifrand that way.
 * @param b       The return parameter.
 * @param places  The number of decimal places to generate.
 */
static void
bcl_frandHelper(BcNum* restrict b, size_t places)
{
	BcNum exp, pow, ten;
	BcDig exp_digs[BC_NUM_BIGDIG_LOG10];
	BcDig ten_digs[BC_NUM_BIGDIG_LOG10];
	BcVm* vm = bcl_getspecific();

	// Set up temporaries.
	bc_num_setup(&exp, exp_digs, BC_NUM_BIGDIG_LOG10);
	bc_num_setup(&ten, ten_digs, BC_NUM_BIGDIG_LOG10);

	ten.num[0] = 10;
	ten.len = 1;

	bc_num_bigdig2num(&exp, (BcBigDig) places);

	// Clear the temporary that might need to grow.
	bc_num_clear(&pow);

	// Initialize the temporary that might need to grow.
	bc_num_init(&pow, bc_num_powReq(&ten, &exp, 0));

	BC_SETJMP(vm, err);

	// Generate the number.
	bc_num_pow(&ten, &exp, &pow, 0);
	bc_num_irand(&pow, b, &vm->rng);

	// Make the number entirely fraction.
	bc_num_shiftRight(b, places);

err:

	bc_num_free(&pow);
	BC_LONGJMP_CONT(vm);
}

BclNumber
bcl_frand(size_t places)
{
	BclError e = BCL_ERROR_NONE;
	BclNum n;
	BclNumber idx;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT(vm, ctxt);

	BC_FUNC_HEADER(vm, err);

	BCL_GROW_NUMS(ctxt);

	// Clear and initialize the number.
	bc_num_clear(BCL_NUM_NUM_NP(n));
	bc_num_init(BCL_NUM_NUM_NP(n), BC_NUM_DEF_SIZE);

	bcl_frandHelper(BCL_NUM_NUM_NP(n), places);

err:

	BC_FUNC_FOOTER(vm, e);
	BC_MAYBE_SETUP(ctxt, e, n, idx);

	return idx;
}

/**
 * Helps bc_ifrand(). This is separate because error handling is easier that
 * way.
 * @param a       The limit for bc_num_irand().
 * @param b       The return parameter.
 * @param places  The number of decimal places to generate.
 */
static void
bcl_ifrandHelper(BcNum* restrict a, BcNum* restrict b, size_t places)
{
	BcNum ir, fr;
	BcVm* vm = bcl_getspecific();

	// Clear the integer and fractional numbers.
	bc_num_clear(&ir);
	bc_num_clear(&fr);

	// Initialize the integer and fractional numbers.
	bc_num_init(&ir, BC_NUM_DEF_SIZE);
	bc_num_init(&fr, BC_NUM_DEF_SIZE);

	BC_SETJMP(vm, err);

	bc_num_irand(a, &ir, &vm->rng);
	bcl_frandHelper(&fr, places);

	bc_num_add(&ir, &fr, b, 0);

err:

	bc_num_free(&fr);
	bc_num_free(&ir);
	BC_LONGJMP_CONT(vm);
}

static BclNumber
bcl_ifrand_helper(BclNumber a, size_t places, bool destruct)
{
	BclError e = BCL_ERROR_NONE;
	BclNum* aptr;
	BclNum b;
	BclNumber idx;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT(vm, ctxt);
	BC_CHECK_NUM(ctxt, a);

	BC_FUNC_HEADER(vm, err);

	BCL_GROW_NUMS(ctxt);

	assert(BCL_NO_GEN(a) < ctxt->nums.len);

	aptr = BCL_NUM(ctxt, a);

	assert(aptr != NULL && BCL_NUM_NUM(aptr) != NULL);

	// Clear and initialize the number.
	bc_num_clear(BCL_NUM_NUM_NP(b));
	bc_num_init(BCL_NUM_NUM_NP(b), BC_NUM_DEF_SIZE);

	bcl_ifrandHelper(BCL_NUM_NUM(aptr), BCL_NUM_NUM_NP(b), places);

err:

	if (destruct)
	{
		// Eat the oprand.
		bcl_num_dtor(ctxt, a, aptr);
	}

	BC_FUNC_FOOTER(vm, e);
	BC_MAYBE_SETUP(ctxt, e, b, idx);

	return idx;
}

BclNumber
bcl_ifrand(BclNumber a, size_t places)
{
	return bcl_ifrand_helper(a, places, true);
}

BclNumber
bcl_ifrand_keep(BclNumber a, size_t places)
{
	return bcl_ifrand_helper(a, places, false);
}

static BclError
bcl_rand_seedWithNum_helper(BclNumber n, bool destruct)
{
	BclError e = BCL_ERROR_NONE;
	BclNum* nptr;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT_ERR(vm, ctxt);
	BC_CHECK_NUM_ERR(ctxt, n);

	BC_FUNC_HEADER(vm, err);

	assert(BCL_NO_GEN(n) < ctxt->nums.len);

	nptr = BCL_NUM(ctxt, n);

	assert(nptr != NULL && BCL_NUM_NUM(nptr) != NULL);

	bc_num_rng(BCL_NUM_NUM(nptr), &vm->rng);

err:

	if (destruct)
	{
		// Eat the oprand.
		bcl_num_dtor(ctxt, n, nptr);
	}

	BC_FUNC_FOOTER(vm, e);

	return e;
}

BclError
bcl_rand_seedWithNum(BclNumber n)
{
	return bcl_rand_seedWithNum_helper(n, true);
}

BclError
bcl_rand_seedWithNum_keep(BclNumber n)
{
	return bcl_rand_seedWithNum_helper(n, false);
}

BclError
bcl_rand_seed(unsigned char seed[BCL_SEED_SIZE])
{
	BclError e = BCL_ERROR_NONE;
	size_t i;
	ulong vals[BCL_SEED_ULONGS];
	BcVm* vm = bcl_getspecific();

	BC_FUNC_HEADER(vm, err);

	// Fill the array.
	for (i = 0; i < BCL_SEED_SIZE; ++i)
	{
		ulong val = ((ulong) seed[i])
		            << (((ulong) CHAR_BIT) * (i % sizeof(ulong)));
		vals[i / sizeof(long)] |= val;
	}

	bc_rand_seed(&vm->rng, vals[0], vals[1], vals[2], vals[3]);

err:

	BC_FUNC_FOOTER(vm, e);

	return e;
}

void
bcl_rand_reseed(void)
{
	BcVm* vm = bcl_getspecific();

	bc_rand_srand(bc_vec_top(&vm->rng.v));
}

BclNumber
bcl_rand_seed2num(void)
{
	BclError e = BCL_ERROR_NONE;
	BclNum n;
	BclNumber idx;
	BclContext ctxt;
	BcVm* vm = bcl_getspecific();

	BC_CHECK_CTXT(vm, ctxt);

	BC_FUNC_HEADER(vm, err);

	// Clear and initialize the number.
	bc_num_clear(BCL_NUM_NUM_NP(n));
	bc_num_init(BCL_NUM_NUM_NP(n), BC_NUM_DEF_SIZE);

	bc_num_createFromRNG(BCL_NUM_NUM_NP(n), &vm->rng);

err:

	BC_FUNC_FOOTER(vm, e);
	BC_MAYBE_SETUP(ctxt, e, n, idx);

	return idx;
}

BclRandInt
bcl_rand_int(void)
{
	BcVm* vm = bcl_getspecific();

	return (BclRandInt) bc_rand_int(&vm->rng);
}

BclRandInt
bcl_rand_bounded(BclRandInt bound)
{
	BcVm* vm = bcl_getspecific();

	if (bound <= 1) return 0;
	return (BclRandInt) bc_rand_bounded(&vm->rng, (BcRand) bound);
}

#endif // BC_ENABLE_EXTRA_MATH

#endif // BC_ENABLE_LIBRARY
