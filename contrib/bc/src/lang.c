/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2020 Gavin D. Howard and contributors.
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
#include <vm.h>

#ifndef NDEBUG
void bc_id_free(void *id) {
	BC_SIG_ASSERT_LOCKED;
	assert(id != NULL);
	free(((BcId*) id)->name);
}
#endif // NDEBUG

void bc_string_free(void *string) {
	BC_SIG_ASSERT_LOCKED;
	assert(string != NULL && (*((char**) string)) != NULL);
	if (BC_IS_BC) free(*((char**) string));
}

void bc_const_free(void *constant) {
	BcConst *c = constant;
	BC_SIG_ASSERT_LOCKED;
	assert(c->val != NULL);
	free(c->val);
	bc_num_free(&c->num);
}

#if BC_ENABLED
void bc_func_insert(BcFunc *f, BcProgram *p, char *name,
                    BcType type, size_t line)
{
	BcLoc a;
	size_t i, idx;

	assert(f != NULL);

	idx = bc_program_search(p, name, type == BC_TYPE_VAR);

	for (i = 0; i < f->autos.len; ++i) {
		BcLoc *id = bc_vec_item(&f->autos, i);
		if (BC_ERR(idx == id->loc && type == (BcType) id->idx)) {
			const char *array = type == BC_TYPE_ARRAY ? "[]" : "";
			bc_vm_error(BC_ERR_PARSE_DUP_LOCAL, line, name, array);
		}
	}

	a.loc = idx;
	a.idx = type;

	bc_vec_push(&f->autos, &a);
}
#endif // BC_ENABLED

void bc_func_init(BcFunc *f, const char *name) {

	BC_SIG_ASSERT_LOCKED;

	assert(f != NULL && name != NULL);

	bc_vec_init(&f->code, sizeof(uchar), NULL);

	bc_vec_init(&f->consts, sizeof(BcConst), bc_const_free);

#if BC_ENABLED
	if (BC_IS_BC) {

		bc_vec_init(&f->strs, sizeof(char*), bc_string_free);

		bc_vec_init(&f->autos, sizeof(BcLoc), NULL);
		bc_vec_init(&f->labels, sizeof(size_t), NULL);

		f->nparams = 0;
		f->voidfn = false;
	}
#endif // BC_ENABLED

	f->name = name;
}

void bc_func_reset(BcFunc *f) {

	BC_SIG_ASSERT_LOCKED;
	assert(f != NULL);

	bc_vec_npop(&f->code, f->code.len);

	bc_vec_npop(&f->consts, f->consts.len);

#if BC_ENABLED
	if (BC_IS_BC) {

		bc_vec_npop(&f->strs, f->strs.len);

		bc_vec_npop(&f->autos, f->autos.len);
		bc_vec_npop(&f->labels, f->labels.len);

		f->nparams = 0;
		f->voidfn = false;
	}
#endif // BC_ENABLED
}

void bc_func_free(void *func) {

#if BC_ENABLE_FUNC_FREE

	BcFunc *f = (BcFunc*) func;

	BC_SIG_ASSERT_LOCKED;
	assert(f != NULL);

	bc_vec_free(&f->code);

	bc_vec_free(&f->consts);

#if BC_ENABLED
#ifndef NDEBUG
	if (BC_IS_BC) {

		bc_vec_free(&f->strs);

		bc_vec_free(&f->autos);
		bc_vec_free(&f->labels);
	}
#endif // NDEBUG
#endif // BC_ENABLED

#else // BC_ENABLE_FUNC_FREE
	BC_UNUSED(func);
#endif // BC_ENABLE_FUNC_FREE
}

void bc_array_init(BcVec *a, bool nums) {
	BC_SIG_ASSERT_LOCKED;
	if (nums) bc_vec_init(a, sizeof(BcNum), bc_num_free);
	else bc_vec_init(a, sizeof(BcVec), bc_vec_free);
	bc_array_expand(a, 1);
}

void bc_array_copy(BcVec *d, const BcVec *s) {

	size_t i;

	BC_SIG_ASSERT_LOCKED;

	assert(d != NULL && s != NULL);
	assert(d != s && d->size == s->size && d->dtor == s->dtor);

	bc_vec_npop(d, d->len);
	bc_vec_expand(d, s->cap);
	d->len = s->len;

	for (i = 0; i < s->len; ++i) {
		BcNum *dnum = bc_vec_item(d, i), *snum = bc_vec_item(s, i);
		bc_num_createCopy(dnum, snum);
	}
}

void bc_array_expand(BcVec *a, size_t len) {

	assert(a != NULL);

	BC_SIG_ASSERT_LOCKED;

	bc_vec_expand(a, len);

	if (a->size == sizeof(BcNum) && a->dtor == bc_num_free) {
		BcNum n;
		while (len > a->len) {
			bc_num_init(&n, BC_NUM_DEF_SIZE);
			bc_vec_push(a, &n);
		}
	}
	else {
		BcVec v;
		assert(a->size == sizeof(BcVec) && a->dtor == bc_vec_free);
		while (len > a->len) {
			bc_array_init(&v, true);
			bc_vec_push(a, &v);
		}
	}
}

void bc_result_clear(BcResult *r) {
	r->t = BC_RESULT_TEMP;
	bc_num_clear(&r->d.n);
}

#if DC_ENABLED
void bc_result_copy(BcResult *d, BcResult *src) {

	assert(d != NULL && src != NULL);

	BC_SIG_ASSERT_LOCKED;

	d->t = src->t;

	switch (d->t) {

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
#if BC_ENABLED
		case BC_RESULT_ARRAY:
#endif // BC_ENABLED
		case BC_RESULT_ARRAY_ELEM:
		{
			memcpy(&d->d.loc, &src->d.loc, sizeof(BcLoc));
			break;
		}

		case BC_RESULT_STR:
		{
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
			abort();
#endif // NDEBUG
		}
#endif // BC_ENABLED
	}
}
#endif // DC_ENABLED

void bc_result_free(void *result) {

	BcResult *r = (BcResult*) result;

	BC_SIG_ASSERT_LOCKED;

	assert(r != NULL);

	switch (r->t) {

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
#if BC_ENABLED
		case BC_RESULT_ARRAY:
#endif // BC_ENABLED
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
