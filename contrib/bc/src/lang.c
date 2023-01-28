/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2023 Gavin D. Howard and contributors.
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
 * Code to manipulate data structures in programs.
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <lang.h>
#include <program.h>
#include <vm.h>

void
bc_const_free(void* constant)
{
	BcConst* c = constant;

	BC_SIG_ASSERT_LOCKED;

	assert(c->val != NULL);

	bc_num_free(&c->num);
}

#if BC_ENABLED
void
bc_func_insert(BcFunc* f, BcProgram* p, char* name, BcType type, size_t line)
{
	BcAuto a;
	size_t i, idx;

	// The function must *always* be valid.
	assert(f != NULL);

	// Get the index of the variable.
	idx = bc_program_search(p, name, type == BC_TYPE_VAR);

	// Search through all of the other autos/parameters.
	for (i = 0; i < f->autos.len; ++i)
	{
		// Get the auto.
		BcAuto* aptr = bc_vec_item(&f->autos, i);

		// If they match, barf.
		if (BC_ERR(idx == aptr->idx && type == aptr->type))
		{
			const char* array = type == BC_TYPE_ARRAY ? "[]" : "";

			bc_error(BC_ERR_PARSE_DUP_LOCAL, line, name, array);
		}
	}

	// Set the auto.
	a.idx = idx;
	a.type = type;

	// Push it.
	bc_vec_push(&f->autos, &a);
}
#endif // BC_ENABLED

void
bc_func_init(BcFunc* f, const char* name)
{
	BC_SIG_ASSERT_LOCKED;

	assert(f != NULL && name != NULL);

	bc_vec_init(&f->code, sizeof(uchar), BC_DTOR_NONE);

#if BC_ENABLED

	// Only bc needs these things.
	if (BC_IS_BC)
	{
		bc_vec_init(&f->autos, sizeof(BcAuto), BC_DTOR_NONE);
		bc_vec_init(&f->labels, sizeof(size_t), BC_DTOR_NONE);

		f->nparams = 0;
		f->voidfn = false;
	}

#endif // BC_ENABLED

	f->name = name;
}

void
bc_func_reset(BcFunc* f)
{
	BC_SIG_ASSERT_LOCKED;
	assert(f != NULL);

	bc_vec_popAll(&f->code);

#if BC_ENABLED
	if (BC_IS_BC)
	{
		bc_vec_popAll(&f->autos);
		bc_vec_popAll(&f->labels);

		f->nparams = 0;
		f->voidfn = false;
	}
#endif // BC_ENABLED
}

#ifndef NDEBUG
void
bc_func_free(void* func)
{
	BcFunc* f = (BcFunc*) func;

	BC_SIG_ASSERT_LOCKED;
	assert(f != NULL);

	bc_vec_free(&f->code);

#if BC_ENABLED
	if (BC_IS_BC)
	{
		bc_vec_free(&f->autos);
		bc_vec_free(&f->labels);
	}
#endif // BC_ENABLED
}
#endif // NDEBUG

void
bc_array_init(BcVec* a, bool nums)
{
	BC_SIG_ASSERT_LOCKED;

	// Set the proper vector.
	if (nums) bc_vec_init(a, sizeof(BcNum), BC_DTOR_NUM);
	else bc_vec_init(a, sizeof(BcVec), BC_DTOR_VEC);

	// We always want at least one item in the array.
	bc_array_expand(a, 1);
}

void
bc_array_copy(BcVec* d, const BcVec* s)
{
	size_t i;

	BC_SIG_ASSERT_LOCKED;

	assert(d != NULL && s != NULL);
	assert(d != s && d->size == s->size && d->dtor == s->dtor);

	// Make sure to destroy everything currently in d. This will put a lot of
	// temps on the reuse list, so allocating later is not going to be as
	// expensive as it seems. Also, it makes it easier to copy numbers that are
	// strings.
	bc_vec_popAll(d);

	// Preexpand.
	bc_vec_expand(d, s->cap);
	d->len = s->len;

	for (i = 0; i < s->len; ++i)
	{
		BcNum* dnum;
		BcNum* snum;

		dnum = bc_vec_item(d, i);
		snum = bc_vec_item(s, i);

		// We have to create a copy of the number as well.
		if (BC_PROG_STR(snum))
		{
			// NOLINTNEXTLINE
			memcpy(dnum, snum, sizeof(BcNum));
		}
		else bc_num_createCopy(dnum, snum);
	}
}

void
bc_array_expand(BcVec* a, size_t len)
{
	assert(a != NULL);

	BC_SIG_ASSERT_LOCKED;

	bc_vec_expand(a, len);

	// If this is true, then we have a num array.
	if (a->size == sizeof(BcNum) && a->dtor == BC_DTOR_NUM)
	{
		// Initialize numbers until we reach the target.
		while (len > a->len)
		{
			BcNum* n = bc_vec_pushEmpty(a);
			bc_num_init(n, BC_NUM_DEF_SIZE);
		}
	}
	else
	{
		assert(a->size == sizeof(BcVec) && a->dtor == BC_DTOR_VEC);

		// Recursively initialize arrays until we reach the target. Having the
		// second argument of bc_array_init() be true will activate the base
		// case, so we're safe.
		while (len > a->len)
		{
			BcVec* v = bc_vec_pushEmpty(a);
			bc_array_init(v, true);
		}
	}
}

void
bc_result_clear(BcResult* r)
{
	r->t = BC_RESULT_TEMP;
	bc_num_clear(&r->d.n);
}

#if DC_ENABLED
void
bc_result_copy(BcResult* d, BcResult* src)
{
	assert(d != NULL && src != NULL);

	BC_SIG_ASSERT_LOCKED;

	// d is assumed to not be valid yet.
	d->t = src->t;

	// Yes, it depends on what type.
	switch (d->t)
	{
		case BC_RESULT_TEMP:
		case BC_RESULT_IBASE:
		case BC_RESULT_SCALE:
		case BC_RESULT_OBASE:
#if BC_ENABLE_EXTRA_MATH
		case BC_RESULT_SEED:
#endif // BC_ENABLE_EXTRA_MATH
		{
			bc_num_createCopy(&d->d.n, &src->d.n);
			break;
		}

		case BC_RESULT_VAR:
		case BC_RESULT_ARRAY:
		case BC_RESULT_ARRAY_ELEM:
		{
			// NOLINTNEXTLINE
			memcpy(&d->d.loc, &src->d.loc, sizeof(BcLoc));
			break;
		}

		case BC_RESULT_STR:
		{
			// NOLINTNEXTLINE
			memcpy(&d->d.n, &src->d.n, sizeof(BcNum));
			break;
		}

		case BC_RESULT_ZERO:
		case BC_RESULT_ONE:
		{
			// Do nothing.
			break;
		}

#if BC_ENABLED
		case BC_RESULT_VOID:
		case BC_RESULT_LAST:
		{
#ifndef NDEBUG
			// We should *never* try copying either of these.
			abort();
#endif // NDEBUG
		}
#endif // BC_ENABLED
	}
}
#endif // DC_ENABLED

void
bc_result_free(void* result)
{
	BcResult* r = (BcResult*) result;

	BC_SIG_ASSERT_LOCKED;

	assert(r != NULL);

	switch (r->t)
	{
		case BC_RESULT_TEMP:
		case BC_RESULT_IBASE:
		case BC_RESULT_SCALE:
		case BC_RESULT_OBASE:
#if BC_ENABLE_EXTRA_MATH
		case BC_RESULT_SEED:
#endif // BC_ENABLE_EXTRA_MATH
		{
			bc_num_free(&r->d.n);
			break;
		}

		case BC_RESULT_VAR:
		case BC_RESULT_ARRAY:
		case BC_RESULT_ARRAY_ELEM:
		case BC_RESULT_STR:
		case BC_RESULT_ZERO:
		case BC_RESULT_ONE:
#if BC_ENABLED
		case BC_RESULT_VOID:
		case BC_RESULT_LAST:
#endif // BC_ENABLED
		{
			// Do nothing.
			break;
		}
	}
}
