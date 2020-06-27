/*
 * *****************************************************************************
 *
 * Copyright (c) 2018-2020 Gavin D. Howard and contributors.
 *
 * All rights reserved.
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
 * Code to execute bc programs.
 *
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <setjmp.h>

#include <signal.h>

#include <time.h>

#include <read.h>
#include <parse.h>
#include <program.h>
#include <vm.h>

static void bc_program_addFunc(BcProgram *p, BcFunc *f, BcId *id_ptr);

static inline void bc_program_setVecs(BcProgram *p, BcFunc *f) {
	p->consts = &f->consts;
	p->strs = &f->strs;
}

static void bc_program_type_num(BcResult *r, BcNum *n) {

#if BC_ENABLED
	assert(r->t != BC_RESULT_VOID);
#endif // BC_ENABLED

	if (BC_ERR(!BC_PROG_NUM(r, n))) bc_vm_err(BC_ERROR_EXEC_TYPE);
}

#if BC_ENABLED
static void bc_program_type_match(BcResult *r, BcType t) {

#if DC_ENABLED
	assert(!BC_IS_BC || BC_NO_ERR(r->t != BC_RESULT_STR));
#endif // DC_ENABLED

	if (BC_ERR((r->t != BC_RESULT_ARRAY) != (!t)))
		bc_vm_err(BC_ERROR_EXEC_TYPE);
}
#endif // BC_ENABLED

static size_t bc_program_index(const char *restrict code, size_t *restrict bgn)
{
	uchar amt = (uchar) code[(*bgn)++], i = 0;
	size_t res = 0;

	for (; i < amt; ++i, ++(*bgn)) {
		size_t temp = ((size_t) ((int) (uchar) code[*bgn]) & UCHAR_MAX);
		res |= (temp << (i * CHAR_BIT));
	}

	return res;
}

static void bc_program_prepGlobals(BcProgram *p) {

	size_t i;

	for (i = 0; i < BC_PROG_GLOBALS_LEN; ++i)
		bc_vec_push(p->globals_v + i, p->globals + i);

#if BC_ENABLE_EXTRA_MATH
	bc_rand_push(&p->rng);
#endif // BC_ENABLE_EXTRA_MATH
}

static void bc_program_popGlobals(BcProgram *p, bool reset) {

	size_t i;

	for (i = 0; i < BC_PROG_GLOBALS_LEN; ++i) {
		BcVec *v = p->globals_v + i;
		bc_vec_npop(v, reset ? v->len - 1 : 1);
		p->globals[i] = BC_PROG_GLOBAL(v);
	}

#if BC_ENABLE_EXTRA_MATH
	bc_rand_pop(&p->rng, reset);
#endif // BC_ENABLE_EXTRA_MATH
}

static void bc_program_pushBigdig(BcProgram *p, BcBigDig dig, BcResultType type)
{
	BcResult res;

	res.t = type;

	BC_SIG_LOCK;

	bc_num_createFromBigdig(&res.d.n, dig);
	bc_vec_push(&p->results, &res);

	BC_SIG_UNLOCK;
}

#if BC_ENABLED
static BcVec* bc_program_dereference(const BcProgram *p, BcVec *vec) {

	BcVec *v;
	size_t vidx, nidx, i = 0;

	assert(vec->size == sizeof(uchar));

	vidx = bc_program_index(vec->v, &i);
	nidx = bc_program_index(vec->v, &i);

	v = bc_vec_item(bc_vec_item(&p->arrs, vidx), nidx);

	assert(v->size != sizeof(uchar));

	return v;
}
#endif // BC_ENABLED

size_t bc_program_search(BcProgram *p, const char *id, bool var) {

	BcVec *v, *map;
	size_t i;
	BcResultData data;

	v = var ? &p->vars : &p->arrs;
	map = var ? &p->var_map : &p->arr_map;

	BC_SIG_LOCK;

	if (bc_map_insert(map, id, v->len, &i)) {
		bc_array_init(&data.v, var);
		bc_vec_push(v, &data.v);
	}

	BC_SIG_UNLOCK;

	return ((BcId*) bc_vec_item(map, i))->idx;
}

static inline BcVec* bc_program_vec(const BcProgram *p, size_t idx, BcType type)
{
	const BcVec *v = (type == BC_TYPE_VAR) ? &p->vars : &p->arrs;
	return bc_vec_item(v, idx);
}

static BcNum* bc_program_num(BcProgram *p, BcResult *r) {

	BcNum *n;

	switch (r->t) {

		case BC_RESULT_CONSTANT:
		{
			BcConst *c = bc_vec_item(p->consts, r->d.loc.loc);
			BcBigDig base = BC_PROG_IBASE(p);

			if (c->base != base) {

				if (c->num.num == NULL) {
					BC_SIG_LOCK;
					bc_num_init(&c->num, BC_NUM_RDX(strlen(c->val)));
					BC_SIG_UNLOCK;
				}

				// bc_num_parse() should only do operations that cannot fail.
				bc_num_parse(&c->num, c->val, base, !c->val[1]);

				c->base = base;
			}

			BC_SIG_LOCK;

			n = &r->d.n;

			r->t = BC_RESULT_TEMP;

			bc_num_createCopy(n, &c->num);

			BC_SIG_UNLOCK;

			break;
		}

		case BC_RESULT_STR:
		case BC_RESULT_TEMP:
		case BC_RESULT_IBASE:
		case BC_RESULT_SCALE:
		case BC_RESULT_OBASE:
#if BC_ENABLE_EXTRA_MATH
		case BC_RESULT_SEED:
#endif // BC_ENABLE_EXTRA_MATH
		{
			n = &r->d.n;
			break;
		}

		case BC_RESULT_VAR:
#if BC_ENABLED
		case BC_RESULT_ARRAY:
#endif // BC_ENABLED
		case BC_RESULT_ARRAY_ELEM:
		{
			BcVec *v;
			BcType type = (r->t == BC_RESULT_VAR) ? BC_TYPE_VAR : BC_TYPE_ARRAY;

			v = bc_program_vec(p, r->d.loc.loc, type);

			if (r->t == BC_RESULT_ARRAY_ELEM) {

				size_t idx = r->d.loc.idx;

				v = bc_vec_top(v);

#if BC_ENABLED
				if (v->size == sizeof(uchar)) v = bc_program_dereference(p, v);
#endif // BC_ENABLED

				assert(v->size == sizeof(BcNum));

				if (v->len <= idx) {
					BC_SIG_LOCK;
					bc_array_expand(v, bc_vm_growSize(idx, 1));
					BC_SIG_UNLOCK;
				}

				n = bc_vec_item(v, idx);
			}
			else n = bc_vec_top(v);

			break;
		}

		case BC_RESULT_ONE:
		{
			n = &p->one;
			break;
		}

#if BC_ENABLED
		case BC_RESULT_VOID:
#ifndef NDEBUG
		{
			abort();
		}
#endif // NDEBUG
		// Fallthrough
		case BC_RESULT_LAST:
		{
			n = &p->last;
			break;
		}
#endif // BC_ENABLED
	}

	return n;
}

static void bc_program_operand(BcProgram *p, BcResult **r,
                               BcNum **n, size_t idx)
{
	*r = bc_vec_item_rev(&p->results, idx);

#if BC_ENABLED
	if (BC_ERR((*r)->t == BC_RESULT_VOID)) bc_vm_err(BC_ERROR_EXEC_VOID_VAL);
#endif // BC_ENABLED

	*n = bc_program_num(p, *r);
}

static void bc_program_binPrep(BcProgram *p, BcResult **l, BcNum **ln,
                               BcResult **r, BcNum **rn, size_t idx)
{
	BcResultType lt;

	assert(p != NULL && l != NULL && ln != NULL && r != NULL && rn != NULL);

#ifndef BC_PROG_NO_STACK_CHECK
	if (!BC_IS_BC) {
		if (BC_ERR(!BC_PROG_STACK(&p->results, idx + 2)))
			bc_vm_err(BC_ERROR_EXEC_STACK);
	}
#endif // BC_PROG_NO_STACK_CHECK

	assert(BC_PROG_STACK(&p->results, idx + 2));

	bc_program_operand(p, l, ln, idx + 1);
	bc_program_operand(p, r, rn, idx);

	lt = (*l)->t;

#if BC_ENABLED
	assert(lt != BC_RESULT_VOID && (*r)->t != BC_RESULT_VOID);
#endif // BC_ENABLED

	// We run this again under these conditions in case any vector has been
	// reallocated out from under the BcNums or arrays we had.
	if (lt == (*r)->t && (lt == BC_RESULT_VAR || lt == BC_RESULT_ARRAY_ELEM))
		*ln = bc_program_num(p, *l);

	if (BC_ERR(lt == BC_RESULT_STR)) bc_vm_err(BC_ERROR_EXEC_TYPE);
}

static void bc_program_binOpPrep(BcProgram *p, BcResult **l, BcNum **ln,
                                 BcResult **r, BcNum **rn, size_t idx)
{
	bc_program_binPrep(p, l, ln, r, rn, idx);
	bc_program_type_num(*l, *ln);
	bc_program_type_num(*r, *rn);
}

static void bc_program_assignPrep(BcProgram *p, BcResult **l, BcNum **ln,
                                  BcResult **r, BcNum **rn)
{
	BcResultType lt, min;

	min = BC_RESULT_CONSTANT - ((unsigned int) (BC_IS_BC << 1));

	bc_program_binPrep(p, l, ln, r, rn, 0);

	lt = (*l)->t;

	if (BC_ERR(lt >= min && lt <= BC_RESULT_ONE))
		bc_vm_err(BC_ERROR_EXEC_TYPE);

#if DC_ENABLED
	if(!BC_IS_BC) {

		bool good = (((*r)->t == BC_RESULT_STR || BC_PROG_STR(*rn)) &&
		             lt <= BC_RESULT_ARRAY_ELEM);

		if (!good) bc_program_type_num(*r, *rn);
	}
#else
	assert((*r)->t != BC_RESULT_STR);
#endif // DC_ENABLED
}

static void bc_program_prep(BcProgram *p, BcResult **r, BcNum **n, size_t idx) {

	assert(p != NULL && r != NULL && n != NULL);

#ifndef BC_PROG_NO_STACK_CHECK
	if (!BC_IS_BC) {
		if (BC_ERR(!BC_PROG_STACK(&p->results, idx + 1)))
			bc_vm_err(BC_ERROR_EXEC_STACK);
	}
#endif // BC_PROG_NO_STACK_CHECK

	assert(BC_PROG_STACK(&p->results, idx + 1));

	bc_program_operand(p, r, n, idx);

#if DC_ENABLED
	assert((*r)->t != BC_RESULT_VAR || !BC_PROG_STR(*n));
#endif // DC_ENABLED

	bc_program_type_num(*r, *n);
}

static BcResult* bc_program_prepResult(BcProgram *p) {

	BcResult res;

	bc_result_clear(&res);
	bc_vec_push(&p->results, &res);

	return bc_vec_top(&p->results);
}

static void bc_program_op(BcProgram *p, uchar inst) {

	BcResult *opd1, *opd2, *res;
	BcNum *n1, *n2;
	size_t idx = inst - BC_INST_POWER;

	res = bc_program_prepResult(p);

	bc_program_binOpPrep(p, &opd1, &n1, &opd2, &n2, 1);

	BC_SIG_LOCK;

	bc_num_init(&res->d.n, bc_program_opReqs[idx](n1, n2, BC_PROG_SCALE(p)));

	BC_SIG_UNLOCK;

	bc_program_ops[idx](n1, n2, &res->d.n, BC_PROG_SCALE(p));

	bc_program_retire(p, 1, 2);
}

static void bc_program_read(BcProgram *p) {

	BcStatus s;
	BcParse parse;
	BcVec buf;
	BcInstPtr ip;
	size_t i;
	const char* file;
	BcFunc *f = bc_vec_item(&p->fns, BC_PROG_READ);

	for (i = 0; i < p->stack.len; ++i) {
		BcInstPtr *ip_ptr = bc_vec_item(&p->stack, i);
		if (ip_ptr->func == BC_PROG_READ)
			bc_vm_err(BC_ERROR_EXEC_REC_READ);
	}

	BC_SIG_LOCK;

	file = vm.file;
	bc_parse_init(&parse, p, BC_PROG_READ);
	bc_vec_init(&buf, sizeof(char), NULL);

	BC_SETJMP_LOCKED(exec_err);

	BC_SIG_UNLOCK;

	bc_lex_file(&parse.l, bc_program_stdin_name);
	bc_vec_npop(&f->code, f->code.len);

	s = bc_read_line(&buf, BC_IS_BC ? "read> " : "?> ");
	if (s == BC_STATUS_EOF) bc_vm_err(BC_ERROR_EXEC_READ_EXPR);

	bc_parse_text(&parse, buf.v);
	vm.expr(&parse, BC_PARSE_NOREAD | BC_PARSE_NEEDVAL);

	if (BC_ERR(parse.l.t != BC_LEX_NLINE && parse.l.t != BC_LEX_EOF))
		bc_vm_err(BC_ERROR_EXEC_READ_EXPR);

	if (BC_G) bc_program_prepGlobals(p);

	ip.func = BC_PROG_READ;
	ip.idx = 0;
	ip.len = p->results.len;

	// Update this pointer, just in case.
	f = bc_vec_item(&p->fns, BC_PROG_READ);

	bc_vec_pushByte(&f->code, vm.read_ret);
	bc_vec_push(&p->stack, &ip);
#if DC_ENABLED
	if (!BC_IS_BC) {
		size_t temp = 0;
		bc_vec_push(&p->tail_calls, &temp);
	}
#endif // DC_ENABLED

exec_err:
	BC_SIG_MAYLOCK;
	bc_parse_free(&parse);
	bc_vec_free(&buf);
	vm.file = file;
	BC_LONGJMP_CONT;
}

#if BC_ENABLE_EXTRA_MATH
static void bc_program_rand(BcProgram *p) {
	BcRand rand = bc_rand_int(&p->rng);
	bc_program_pushBigdig(p, (BcBigDig) rand, BC_RESULT_TEMP);
}
#endif // BC_ENABLE_EXTRA_MATH

static void bc_program_printChars(const char *str) {

	const char *nl;
	size_t len = vm.nchars + strlen(str);

	bc_file_puts(&vm.fout, str);
	nl = strrchr(str, '\n');

	if (nl != NULL) len = strlen(nl + 1);

	vm.nchars = len > UINT16_MAX ? UINT16_MAX : (uint16_t) len;
}

static void bc_program_printString(const char *restrict str) {

	size_t i, len = strlen(str);

#if DC_ENABLED
	if (!len && !BC_IS_BC) {
		bc_vm_putchar('\0');
		return;
	}
#endif // DC_ENABLED

	for (i = 0; i < len; ++i) {

		int c = str[i];

		if (c == '\\' && i != len - 1) {

			const char *ptr;

			c = str[++i];
			ptr = strchr(bc_program_esc_chars, c);

			if (ptr != NULL) {
				if (c == 'n') vm.nchars = UINT16_MAX;
				c = bc_program_esc_seqs[(size_t) (ptr - bc_program_esc_chars)];
			}
			else {
				// Just print the backslash. The following
				// character will be printed later.
				bc_vm_putchar('\\');
			}
		}

		bc_vm_putchar(c);
	}
}

static void bc_program_print(BcProgram *p, uchar inst, size_t idx) {

	BcResult *r;
	char *str;
	BcNum *n;
	bool pop = (inst != BC_INST_PRINT);

	assert(p != NULL);

#ifndef BC_PROG_NO_STACK_CHECK
	if (!BC_IS_BC) {
		if (BC_ERR(!BC_PROG_STACK(&p->results, idx + 1)))
			bc_vm_err(BC_ERROR_EXEC_STACK);
	}
#endif // BC_PROG_NO_STACK_CHECK

	assert(BC_PROG_STACK(&p->results, idx + 1));

	assert(BC_IS_BC ||
	       p->strs == &((BcFunc*) bc_vec_item(&p->fns, BC_PROG_MAIN))->strs);

	r = bc_vec_item_rev(&p->results, idx);

#if BC_ENABLED
	if (r->t == BC_RESULT_VOID) {
		if (BC_ERR(pop)) bc_vm_err(BC_ERROR_EXEC_VOID_VAL);
		bc_vec_pop(&p->results);
		return;
	}
#endif // BC_ENABLED

	n = bc_program_num(p, r);

	if (BC_PROG_NUM(r, n)) {
		assert(inst != BC_INST_PRINT_STR);
		bc_num_print(n, BC_PROG_OBASE(p), !pop);
#if BC_ENABLED
		if (BC_IS_BC) bc_num_copy(&p->last, n);
#endif // BC_ENABLED
	}
	else {

		size_t i = (r->t == BC_RESULT_STR) ? r->d.loc.loc : n->scale;

		str = *((char**) bc_vec_item(p->strs, i));

		if (inst == BC_INST_PRINT_STR) bc_program_printChars(str);
		else {
			bc_program_printString(str);
			if (inst == BC_INST_PRINT) bc_vm_putchar('\n');
		}
	}

	if (BC_IS_BC || pop) bc_vec_pop(&p->results);
}

void bc_program_negate(BcResult *r, BcNum *n) {
	bc_num_copy(&r->d.n, n);
	if (BC_NUM_NONZERO(&r->d.n)) r->d.n.neg = !r->d.n.neg;
}

void bc_program_not(BcResult *r, BcNum *n) {
	if (!bc_num_cmpZero(n)) bc_num_one(&r->d.n);
}

#if BC_ENABLE_EXTRA_MATH
void bc_program_trunc(BcResult *r, BcNum *n) {
	bc_num_copy(&r->d.n, n);
	bc_num_truncate(&r->d.n, n->scale);
}
#endif // BC_ENABLE_EXTRA_MATH

static void bc_program_unary(BcProgram *p, uchar inst) {

	BcResult *res, *ptr;
	BcNum *num;

	res = bc_program_prepResult(p);

	bc_program_prep(p, &ptr, &num, 1);

	BC_SIG_LOCK;

	bc_num_init(&res->d.n, num->len);

	BC_SIG_UNLOCK;

	bc_program_unarys[inst - BC_INST_NEG](res, num);
	bc_program_retire(p, 1, 1);
}

static void bc_program_logical(BcProgram *p, uchar inst) {

	BcResult *opd1, *opd2, *res;
	BcNum *n1, *n2;
	bool cond = 0;
	ssize_t cmp;

	res = bc_program_prepResult(p);

	bc_program_binOpPrep(p, &opd1, &n1, &opd2, &n2, 1);

	if (inst == BC_INST_BOOL_AND)
		cond = (bc_num_cmpZero(n1) && bc_num_cmpZero(n2));
	else if (inst == BC_INST_BOOL_OR)
		cond = (bc_num_cmpZero(n1) || bc_num_cmpZero(n2));
	else {

		cmp = bc_num_cmp(n1, n2);

		switch (inst) {

			case BC_INST_REL_EQ:
			{
				cond = (cmp == 0);
				break;
			}

			case BC_INST_REL_LE:
			{
				cond = (cmp <= 0);
				break;
			}

			case BC_INST_REL_GE:
			{
				cond = (cmp >= 0);
				break;
			}

			case BC_INST_REL_NE:
			{
				cond = (cmp != 0);
				break;
			}

			case BC_INST_REL_LT:
			{
				cond = (cmp < 0);
				break;
			}

			case BC_INST_REL_GT:
			{
				cond = (cmp > 0);
				break;
			}
#ifndef NDEBUG
			default:
			{
				abort();
			}
#endif // NDEBUG
		}
	}

	BC_SIG_LOCK;

	bc_num_init(&res->d.n, BC_NUM_DEF_SIZE);

	BC_SIG_UNLOCK;

	if (cond) bc_num_one(&res->d.n);

	bc_program_retire(p, 1, 2);
}

#if DC_ENABLED
static void bc_program_assignStr(BcProgram *p, BcResult *r,
                                 BcVec *v, bool push)
{
	BcNum n2;

	bc_num_clear(&n2);
	n2.scale = r->d.loc.loc;

	assert(BC_PROG_STACK(&p->results, 1 + !push));

	if (!push) bc_vec_pop(v);

	bc_vec_npop(&p->results, 1 + !push);
	bc_vec_push(v, &n2);
}
#endif // DC_ENABLED

static void bc_program_copyToVar(BcProgram *p, size_t idx,
                                 BcType t, bool last)
{
	BcResult *ptr = NULL, r;
	BcVec *vec;
	BcNum *n = NULL;
	bool var = (t == BC_TYPE_VAR);

#if DC_ENABLED
	if (!BC_IS_BC) {

		if (BC_ERR(!BC_PROG_STACK(&p->results, 1)))
			bc_vm_err(BC_ERROR_EXEC_STACK);

		assert(BC_PROG_STACK(&p->results, 1));

		bc_program_operand(p, &ptr, &n, 0);
	}
#endif

#if BC_ENABLED
	if (BC_IS_BC)
	{
		ptr = bc_vec_top(&p->results);

		bc_program_type_match(ptr, t);

		if (last) n = bc_program_num(p, ptr);
		else if (var)
			n = bc_vec_item_rev(bc_program_vec(p, ptr->d.loc.loc, t), 1);
	}
#endif // BC_ENABLED

	vec = bc_program_vec(p, idx, t);

#if DC_ENABLED
	if (ptr->t == BC_RESULT_STR) {
		if (BC_ERR(!var)) bc_vm_err(BC_ERROR_EXEC_TYPE);
		bc_program_assignStr(p, ptr, vec, true);
		return;
	}
#endif // DC_ENABLED

	BC_SIG_LOCK;

	if (var) bc_num_createCopy(&r.d.n, n);
	else {

		BcVec *v = (BcVec*) n, *rv = &r.d.v;
#if BC_ENABLED
		BcVec *parent;
		bool ref, ref_size;

		parent = bc_program_vec(p, ptr->d.loc.loc, t);
		assert(parent != NULL);

		if (!last) v = bc_vec_item_rev(parent, !last);
		assert(v != NULL);

		ref = (v->size == sizeof(BcNum) && t == BC_TYPE_REF);
		ref_size = (v->size == sizeof(uchar));

		if (ref || (ref_size && t == BC_TYPE_REF)) {

			bc_vec_init(rv, sizeof(uchar), NULL);

			if (ref) {

				assert(parent->len >= (size_t) (!last + 1));

				// Make sure the pointer was not invalidated.
				vec = bc_program_vec(p, idx, t);

				bc_vec_pushIndex(rv, ptr->d.loc.loc);
				bc_vec_pushIndex(rv, parent->len - !last - 1);
			}
			// If we get here, we are copying a ref to a ref.
			else bc_vec_npush(rv, v->len * sizeof(uchar), v->v);

			// We need to return early.
			bc_vec_push(vec, &r.d);
			bc_vec_pop(&p->results);

			BC_SIG_UNLOCK;
			return;
		}
		else if (ref_size && t != BC_TYPE_REF) v = bc_program_dereference(p, v);
#endif // BC_ENABLED

		bc_array_init(rv, true);
		bc_array_copy(rv, v);
	}

	bc_vec_push(vec, &r.d);
	bc_vec_pop(&p->results);

	BC_SIG_UNLOCK;
}

static void bc_program_assign(BcProgram *p, uchar inst) {

	BcResult *left, *right, res;
	BcNum *l, *r;
	bool ob, sc, use_val = BC_INST_USE_VAL(inst);

	bc_program_assignPrep(p, &left, &l, &right, &r);

#if DC_ENABLED
	assert(left->t != BC_RESULT_STR);

	if (right->t == BC_RESULT_STR || BC_PROG_STR(r)) {

		size_t idx = right->d.loc.loc;

		if (left->t == BC_RESULT_ARRAY_ELEM) {
			BC_SIG_LOCK;
			bc_num_free(l);
			bc_num_clear(l);
			l->scale = idx;
			bc_vec_npop(&p->results, 2);
			BC_SIG_UNLOCK;
		}
		else {
			BcVec *v = bc_program_vec(p, left->d.loc.loc, BC_TYPE_VAR);
			bc_program_assignStr(p, right, v, false);
		}

		return;
	}
#endif // DC_ENABLED

	if (BC_INST_IS_ASSIGN(inst)) bc_num_copy(l, r);
#if BC_ENABLED
	else {

		BcBigDig scale = BC_PROG_SCALE(p);

		if (!use_val)
			inst -= (BC_INST_ASSIGN_POWER_NO_VAL - BC_INST_ASSIGN_POWER);

		bc_program_ops[inst - BC_INST_ASSIGN_POWER](l, r, l, scale);
	}
#endif // BC_ENABLED

	ob = (left->t == BC_RESULT_OBASE);
	sc = (left->t == BC_RESULT_SCALE);

	if (ob || sc || left->t == BC_RESULT_IBASE) {

		BcVec *v;
		BcBigDig *ptr, *ptr_t, val, max, min;
		BcError e;

		bc_num_bigdig(l, &val);
		e = left->t - BC_RESULT_IBASE + BC_ERROR_EXEC_IBASE;

		if (sc) {
			min = 0;
			max = vm.maxes[BC_PROG_GLOBALS_SCALE];
			v = p->globals_v + BC_PROG_GLOBALS_SCALE;
			ptr_t = p->globals + BC_PROG_GLOBALS_SCALE;
		}
		else {
			min = BC_NUM_MIN_BASE;
			if (BC_ENABLE_EXTRA_MATH && ob && (!BC_IS_BC || !BC_IS_POSIX))
				min = 0;
			max = vm.maxes[ob + BC_PROG_GLOBALS_IBASE];
			v = p->globals_v + BC_PROG_GLOBALS_IBASE + ob;
			ptr_t = p->globals + BC_PROG_GLOBALS_IBASE + ob;
		}

		if (BC_ERR(val > max || val < min)) bc_vm_verr(e, min, max);

		ptr = bc_vec_top(v);
		*ptr = val;
		*ptr_t = val;
	}
#if BC_ENABLE_EXTRA_MATH
	else if (left->t == BC_RESULT_SEED) bc_num_rng(l, &p->rng);
#endif // BC_ENABLE_EXTRA_MATH

	BC_SIG_LOCK;

	if (use_val) {
		bc_num_createCopy(&res.d.n, l);
		res.t = BC_RESULT_TEMP;
		bc_vec_npop(&p->results, 2);
		bc_vec_push(&p->results, &res);
	}
	else bc_vec_npop(&p->results, 2);

	BC_SIG_UNLOCK;
}

static void bc_program_pushVar(BcProgram *p, const char *restrict code,
                               size_t *restrict bgn, bool pop, bool copy)
{
	BcResult r;
	size_t idx = bc_program_index(code, bgn);

	r.t = BC_RESULT_VAR;
	r.d.loc.loc = idx;

#if DC_ENABLED
	if (!BC_IS_BC && (pop || copy)) {

		BcVec *v = bc_program_vec(p, idx, BC_TYPE_VAR);
		BcNum *num = bc_vec_top(v);

		if (BC_ERR(!BC_PROG_STACK(v, 2 - copy))) bc_vm_err(BC_ERROR_EXEC_STACK);

		assert(BC_PROG_STACK(v, 2 - copy));

		if (!BC_PROG_STR(num)) {

			BC_SIG_LOCK;

			r.t = BC_RESULT_TEMP;
			bc_num_createCopy(&r.d.n, num);

			if (!copy) bc_vec_pop(v);

			bc_vec_push(&p->results, &r);

			BC_SIG_UNLOCK;

			return;
		}
		else {
			r.d.loc.loc = num->scale;
			r.t = BC_RESULT_STR;
		}

		if (!copy) bc_vec_pop(v);
	}
#endif // DC_ENABLED

	bc_vec_push(&p->results, &r);
}

static void bc_program_pushArray(BcProgram *p, const char *restrict code,
                                 size_t *restrict bgn, uchar inst)
{
	BcResult r, *operand;
	BcNum *num;
	BcBigDig temp;

	r.d.loc.loc = bc_program_index(code, bgn);

#if BC_ENABLED
	if (inst == BC_INST_ARRAY) {
		r.t = BC_RESULT_ARRAY;
		bc_vec_push(&p->results, &r);
		return;
	}
#endif // BC_ENABLED

	bc_program_prep(p, &operand, &num, 0);
	bc_num_bigdig(num, &temp);

	r.t = BC_RESULT_ARRAY_ELEM;
	r.d.loc.idx = (size_t) temp;
	bc_vec_pop(&p->results);
	bc_vec_push(&p->results, &r);
}

#if BC_ENABLED
static void bc_program_incdec(BcProgram *p, uchar inst) {

	BcResult *ptr, res, copy;
	BcNum *num;
	uchar inst2;

	bc_program_prep(p, &ptr, &num, 0);

	BC_SIG_LOCK;

	copy.t = BC_RESULT_TEMP;
	bc_num_createCopy(&copy.d.n, num);

	BC_SETJMP_LOCKED(exit);

	BC_SIG_UNLOCK;

	res.t = BC_RESULT_ONE;
	inst2 = BC_INST_ASSIGN_PLUS + (inst & 0x01);

	bc_vec_push(&p->results, &res);
	bc_program_assign(p, inst2);

	BC_SIG_LOCK;

	bc_vec_pop(&p->results);
	bc_vec_push(&p->results, &copy);

	BC_UNSETJMP;

	BC_SIG_UNLOCK;

	return;

exit:
	BC_SIG_MAYLOCK;
	bc_num_free(&copy.d.n);
	BC_LONGJMP_CONT;
}

static void bc_program_call(BcProgram *p, const char *restrict code,
                            size_t *restrict idx)
{
	BcInstPtr ip;
	size_t i, nparams = bc_program_index(code, idx);
	BcFunc *f;
	BcVec *v;
	BcLoc *a;
	BcResultData param;
	BcResult *arg;

	ip.idx = 0;
	ip.func = bc_program_index(code, idx);
	f = bc_vec_item(&p->fns, ip.func);

	if (BC_ERR(!f->code.len)) bc_vm_verr(BC_ERROR_EXEC_UNDEF_FUNC, f->name);
	if (BC_ERR(nparams != f->nparams))
		bc_vm_verr(BC_ERROR_EXEC_PARAMS, f->nparams, nparams);
	ip.len = p->results.len - nparams;

	assert(BC_PROG_STACK(&p->results, nparams));

	if (BC_G) bc_program_prepGlobals(p);

	for (i = 0; i < nparams; ++i) {

		size_t j;
		bool last = true;

		arg = bc_vec_top(&p->results);
		if (BC_ERR(arg->t == BC_RESULT_VOID))
			bc_vm_err(BC_ERROR_EXEC_VOID_VAL);

		a = bc_vec_item(&f->autos, nparams - 1 - i);

		// If I have already pushed to a var, I need to make sure I
		// get the previous version, not the already pushed one.
		if (arg->t == BC_RESULT_VAR || arg->t == BC_RESULT_ARRAY) {
			for (j = 0; j < i && last; ++j) {
				BcLoc *loc = bc_vec_item(&f->autos, nparams - 1 - j);
				last = (arg->d.loc.loc != loc->loc ||
				        (!loc->idx) != (arg->t == BC_RESULT_VAR));
			}
		}

		bc_program_copyToVar(p, a->loc, (BcType) a->idx, last);
	}

	BC_SIG_LOCK;

	for (; i < f->autos.len; ++i) {

		a = bc_vec_item(&f->autos, i);
		v = bc_program_vec(p, a->loc, (BcType) a->idx);

		if (a->idx == BC_TYPE_VAR) {
			bc_num_init(&param.n, BC_NUM_DEF_SIZE);
			bc_vec_push(v, &param.n);
		}
		else {
			assert(a->idx == BC_TYPE_ARRAY);
			bc_array_init(&param.v, true);
			bc_vec_push(v, &param.v);
		}
	}

	bc_vec_push(&p->stack, &ip);

	BC_SIG_UNLOCK;
}

static void bc_program_return(BcProgram *p, uchar inst) {

	BcResult *res;
	BcFunc *f;
	BcInstPtr *ip = bc_vec_top(&p->stack);
	size_t i, nops = p->results.len - ip->len;

	assert(BC_PROG_STACK(&p->stack, 2));
	assert(BC_PROG_STACK(&p->results, ip->len + (inst == BC_INST_RET)));

	f = bc_vec_item(&p->fns, ip->func);
	res = bc_program_prepResult(p);

	if (inst == BC_INST_RET) {

		BcNum *num;
		BcResult *operand;

		bc_program_operand(p, &operand, &num, 1);

		BC_SIG_LOCK;

		bc_num_createCopy(&res->d.n, num);
	}
	else if (inst == BC_INST_RET_VOID) res->t = BC_RESULT_VOID;
	else {
		BC_SIG_LOCK;
		bc_num_init(&res->d.n, BC_NUM_DEF_SIZE);
	}

	BC_SIG_MAYUNLOCK;

	// We need to pop arguments as well, so this takes that into account.
	for (i = 0; i < f->autos.len; ++i) {

		BcLoc *a = bc_vec_item(&f->autos, i);
		BcVec *v = bc_program_vec(p, a->loc, (BcType) a->idx);

		bc_vec_pop(v);
	}

	bc_program_retire(p, 1, nops);

	if (BC_G) bc_program_popGlobals(p, false);

	bc_vec_pop(&p->stack);
}
#endif // BC_ENABLED

static void bc_program_builtin(BcProgram *p, uchar inst) {

	BcResult *opd, *res;
	BcNum *num;
	bool len = (inst == BC_INST_LENGTH);

#if BC_ENABLE_EXTRA_MATH
	assert(inst >= BC_INST_LENGTH && inst <= BC_INST_IRAND);
#else // BC_ENABLE_EXTRA_MATH
	assert(inst >= BC_INST_LENGTH && inst <= BC_INST_ABS);
#endif // BC_ENABLE_EXTRA_MATH

#ifndef BC_PROG_NO_STACK_CHECK
	if (!BC_IS_BC) {
		if (BC_ERR(!BC_PROG_STACK(&p->results, 1)))
			bc_vm_err(BC_ERROR_EXEC_STACK);
	}
#endif // BC_PROG_NO_STACK_CHECK

	assert(BC_PROG_STACK(&p->results, 1));

	res = bc_program_prepResult(p);

	bc_program_operand(p, &opd, &num, 1);

	assert(num != NULL);

#if DC_ENABLED
	if (!len && inst != BC_INST_SCALE_FUNC) bc_program_type_num(opd, num);
#endif // DC_ENABLED

	if (inst == BC_INST_SQRT) bc_num_sqrt(num, &res->d.n, BC_PROG_SCALE(p));
	else if (inst == BC_INST_ABS) {

		BC_SIG_LOCK;

		bc_num_createCopy(&res->d.n, num);

		BC_SIG_UNLOCK;

		res->d.n.neg = false;
	}
#if BC_ENABLE_EXTRA_MATH
	else if (inst == BC_INST_IRAND) {

		BC_SIG_LOCK;

		bc_num_init(&res->d.n, num->len - num->rdx);

		BC_SIG_UNLOCK;

		bc_num_irand(num, &res->d.n, &p->rng);
	}
#endif // BC_ENABLE_EXTRA_MATH
	else {

		BcBigDig val = 0;

		if (len) {
#if BC_ENABLED
			if (BC_IS_BC && opd->t == BC_RESULT_ARRAY) {

				BcVec *v = (BcVec*) num;

				if (v->size == sizeof(uchar)) v = bc_program_dereference(p, v);

				assert(v->size == sizeof(BcNum));

				val = (BcBigDig) v->len;
			}
			else
#endif // BC_ENABLED
			{
#if DC_ENABLED
				if (!BC_PROG_NUM(opd, num)) {
					size_t idx;
					char *str;
					idx = opd->t == BC_RESULT_STR ? opd->d.loc.loc : num->scale;
					str = *((char**) bc_vec_item(p->strs, idx));
					val = (BcBigDig) strlen(str);
				}
				else
#endif // DC_ENABLED
				{
					val = (BcBigDig) bc_num_len(num);
				}
			}
		}
		else if (BC_IS_BC || BC_PROG_NUM(opd, num))
			val = (BcBigDig) bc_num_scale(num);

		BC_SIG_LOCK;

		bc_num_createFromBigdig(&res->d.n, val);

		BC_SIG_UNLOCK;
	}

	bc_program_retire(p, 1, 1);
}

#if DC_ENABLED
static void bc_program_divmod(BcProgram *p) {

	BcResult *opd1, *opd2, *res, *res2;
	BcNum *n1, *n2;
	size_t req;

	res2 = bc_program_prepResult(p);
	res = bc_program_prepResult(p);

	// Update the pointer, just in case.
	res2 = bc_vec_item_rev(&p->results, 1);

	bc_program_binOpPrep(p, &opd1, &n1, &opd2, &n2, 2);

	req = bc_num_mulReq(n1, n2, BC_PROG_SCALE(p));

	BC_SIG_LOCK;

	bc_num_init(&res->d.n, req);
	bc_num_init(&res2->d.n, req);

	BC_SIG_UNLOCK;

	bc_num_divmod(n1, n2, &res2->d.n, &res->d.n, BC_PROG_SCALE(p));

	bc_program_retire(p, 2, 2);
}

static void bc_program_modexp(BcProgram *p) {

	BcResult *r1, *r2, *r3, *res;
	BcNum *n1, *n2, *n3;

	if (BC_ERR(!BC_PROG_STACK(&p->results, 3))) bc_vm_err(BC_ERROR_EXEC_STACK);

	assert(BC_PROG_STACK(&p->results, 3));

	res = bc_program_prepResult(p);

	bc_program_operand(p, &r1, &n1, 3);
	bc_program_type_num(r1, n1);

	bc_program_binOpPrep(p, &r2, &n2, &r3, &n3, 1);

	// Make sure that the values have their pointers updated, if necessary.
	// Only array elements are possible.
	if (r1->t == BC_RESULT_ARRAY_ELEM && (r1->t == r2->t || r1->t == r3->t))
		n1 = bc_program_num(p, r1);

	BC_SIG_LOCK;

	bc_num_init(&res->d.n, n3->len);

	BC_SIG_UNLOCK;

	bc_num_modexp(n1, n2, n3, &res->d.n);

	bc_program_retire(p, 1, 3);
}

static void bc_program_stackLen(BcProgram *p) {
	bc_program_pushBigdig(p, (BcBigDig) p->results.len, BC_RESULT_TEMP);
}

static uchar bc_program_asciifyNum(BcProgram *p, BcNum *n) {

	BcNum num;
	BcBigDig val = 0;

	bc_num_clear(&num);

	BC_SETJMP(num_err);

	BC_SIG_LOCK;

	bc_num_createCopy(&num, n);

	BC_SIG_UNLOCK;

	bc_num_truncate(&num, num.scale);
	num.neg = false;

	// This is guaranteed to not have a divide by 0
	// because strmb is equal to UCHAR_MAX + 1.
	bc_num_mod(&num, &p->strmb, &num, 0);

	// This is also guaranteed to not error because num is in the range
	// [0, UCHAR_MAX], which is definitely in range for a BcBigDig. And
	// it is not negative.
	bc_num_bigdig2(&num, &val);

num_err:
	BC_SIG_MAYLOCK;
	bc_num_free(&num);
	BC_LONGJMP_CONT;
	return (uchar) val;
}

static void bc_program_asciify(BcProgram *p) {

	BcResult *r, res;
	BcNum *n;
	char str[2], *str2;
	uchar c;
	size_t idx;

	if (BC_ERR(!BC_PROG_STACK(&p->results, 1))) bc_vm_err(BC_ERROR_EXEC_STACK);

	assert(BC_PROG_STACK(&p->results, 1));

	bc_program_operand(p, &r, &n, 0);

	assert(n != NULL);

	assert(p->strs->len + BC_PROG_REQ_FUNCS == p->fns.len);

	if (BC_PROG_NUM(r, n)) c = bc_program_asciifyNum(p, n);
	else {
		size_t index = r->t == BC_RESULT_STR ? r->d.loc.loc : n->scale;
		str2 = *((char**) bc_vec_item(p->strs, index));
		c = (uchar) str2[0];
	}

	str[0] = (char) c;
	str[1] = '\0';

	BC_SIG_LOCK;

	idx = bc_program_insertFunc(p, str) - BC_PROG_REQ_FUNCS;

	BC_SIG_UNLOCK;

	res.t = BC_RESULT_STR;
	res.d.loc.loc = idx;
	bc_vec_pop(&p->results);
	bc_vec_push(&p->results, &res);
}

static void bc_program_printStream(BcProgram *p) {

	BcResult *r;
	BcNum *n;

	if (BC_ERR(!BC_PROG_STACK(&p->results, 1))) bc_vm_err(BC_ERROR_EXEC_STACK);

	assert(BC_PROG_STACK(&p->results, 1));

	bc_program_operand(p, &r, &n, 0);

	assert(n != NULL);

	if (BC_PROG_NUM(r, n)) bc_num_stream(n, p->strm);
	else {
		size_t idx = (r->t == BC_RESULT_STR) ? r->d.loc.loc : n->scale;
		bc_program_printChars(*((char**) bc_vec_item(p->strs, idx)));
	}
}

static void bc_program_nquit(BcProgram *p, uchar inst) {

	BcResult *opnd;
	BcNum *num;
	BcBigDig val;
	size_t i;

	assert(p->stack.len == p->tail_calls.len);

	if (inst == BC_INST_QUIT) val = 2;
	else {

		bc_program_prep(p, &opnd, &num, 0);
		bc_num_bigdig(num, &val);

		bc_vec_pop(&p->results);
	}

	for (i = 0; val && i < p->tail_calls.len; ++i) {
		size_t calls = *((size_t*) bc_vec_item_rev(&p->tail_calls, i)) + 1;
		if (calls >= val) val = 0;
		else val -= calls;
	}

	if (i == p->stack.len) {
		vm.status = BC_STATUS_QUIT;
		BC_VM_JMP;
	}
	else {
		bc_vec_npop(&p->stack, i);
		bc_vec_npop(&p->tail_calls, i);
	}
}

static void bc_program_execStr(BcProgram *p, const char *restrict code,
                                   size_t *restrict bgn, bool cond, size_t len)
{
	BcResult *r;
	char *str;
	BcFunc *f;
	BcParse prs;
	BcInstPtr ip;
	size_t fidx, sidx;
	BcNum *n;

	assert(p->stack.len == p->tail_calls.len);

	if (BC_ERR(!BC_PROG_STACK(&p->results, 1))) bc_vm_err(BC_ERROR_EXEC_STACK);

	assert(BC_PROG_STACK(&p->results, 1));

	bc_program_operand(p, &r, &n, 0);

	if (cond) {

		bool exec;
		size_t idx, then_idx, else_idx;

		then_idx = bc_program_index(code, bgn);
		else_idx = bc_program_index(code, bgn);

		exec = (r->d.n.len != 0);

		idx = exec ? then_idx : else_idx;

		BC_SIG_LOCK;
		BC_SETJMP_LOCKED(exit);

		if (exec || (else_idx != SIZE_MAX))
			n = bc_vec_top(bc_program_vec(p, idx, BC_TYPE_VAR));
		else goto exit;

		if (BC_ERR(!BC_PROG_STR(n))) bc_vm_err(BC_ERROR_EXEC_TYPE);

		BC_UNSETJMP;
		BC_SIG_UNLOCK;

		sidx = n->scale;
	}
	else {

		// In non-conditional situations, only the top of stack can be executed,
		// and in those cases, variables are not allowed to be "on the stack";
		// they are only put on the stack to be assigned to.
		assert(r->t != BC_RESULT_VAR);

		if (r->t == BC_RESULT_STR) sidx = r->d.loc.loc;
		else return;
	}

	fidx = sidx + BC_PROG_REQ_FUNCS;
	str = *((char**) bc_vec_item(p->strs, sidx));
	f = bc_vec_item(&p->fns, fidx);

	if (!f->code.len) {

		BC_SIG_LOCK;

		bc_parse_init(&prs, p, fidx);
		bc_lex_file(&prs.l, vm.file);

		BC_SETJMP_LOCKED(err);

		BC_SIG_UNLOCK;

		bc_parse_text(&prs, str);
		vm.expr(&prs, BC_PARSE_NOCALL);

		BC_SIG_LOCK;

		BC_UNSETJMP;

		// We can just assert this here because
		// dc should parse everything until EOF.
		assert(prs.l.t == BC_LEX_EOF);

		bc_parse_free(&prs);

		BC_SIG_UNLOCK;
	}

	ip.idx = 0;
	ip.len = p->results.len;
	ip.func = fidx;

	bc_vec_pop(&p->results);

	// Tail call.
	if (p->stack.len > 1 && *bgn == len - 1 && code[*bgn] == BC_INST_POP_EXEC) {
		size_t *call_ptr = bc_vec_top(&p->tail_calls);
		*call_ptr += 1;
		bc_vec_pop(&p->stack);
	}
	else bc_vec_push(&p->tail_calls, &ip.idx);

	bc_vec_push(&p->stack, &ip);

	return;

err:
	BC_SIG_MAYLOCK;
	bc_parse_free(&prs);
	f = bc_vec_item(&p->fns, fidx);
	bc_vec_npop(&f->code, f->code.len);
exit:
	bc_vec_pop(&p->results);
	BC_LONGJMP_CONT;
}

static void bc_program_printStack(BcProgram *p) {

	size_t idx;

	for (idx = 0; idx < p->results.len; ++idx)
		bc_program_print(p, BC_INST_PRINT, idx);
}
#endif // DC_ENABLED

static void bc_program_pushGlobal(BcProgram *p, uchar inst) {

	BcResultType t;

	assert(inst >= BC_INST_IBASE && inst <= BC_INST_SCALE);

	t = inst - BC_INST_IBASE + BC_RESULT_IBASE;
	bc_program_pushBigdig(p, p->globals[inst - BC_INST_IBASE], t);
}

#if BC_ENABLE_EXTRA_MATH
static void bc_program_pushSeed(BcProgram *p) {

	BcResult *res;

	res = bc_program_prepResult(p);
	res->t = BC_RESULT_SEED;

	BC_SIG_LOCK;

	bc_num_init(&res->d.n, 2 * BC_RAND_NUM_SIZE);

	BC_SIG_UNLOCK;

	bc_num_createFromRNG(&res->d.n, &p->rng);
}
#endif // BC_ENABLE_EXTRA_MATH

static void bc_program_addFunc(BcProgram *p, BcFunc *f, BcId *id_ptr) {

	BcInstPtr *ip;

	BC_SIG_ASSERT_LOCKED;

	bc_func_init(f, id_ptr->name);
	bc_vec_push(&p->fns, f);

	// This is to make sure pointers are updated if the array was moved.
	if (BC_IS_BC && p->stack.len) {
		ip = bc_vec_item_rev(&p->stack, 0);
		bc_program_setVecs(p, (BcFunc*) bc_vec_item(&p->fns, ip->func));
	}
	else bc_program_setVecs(p, (BcFunc*) bc_vec_item(&p->fns, BC_PROG_MAIN));
}

size_t bc_program_insertFunc(BcProgram *p, const char *name) {

	BcId *id_ptr;
	BcFunc f;
	bool new;
	size_t idx;

	BC_SIG_ASSERT_LOCKED;

	assert(p != NULL && name != NULL);

	new = bc_map_insert(&p->fn_map, name, p->fns.len, &idx);
	id_ptr = (BcId*) bc_vec_item(&p->fn_map, idx);
	idx = id_ptr->idx;

	if (!new) {
		if (BC_IS_BC) {
			BcFunc *func = bc_vec_item(&p->fns, idx);
			bc_func_reset(func);
		}
	}
	else {

		bc_program_addFunc(p, &f, id_ptr);

#if DC_ENABLED
		if (!BC_IS_BC && strcmp(name, bc_func_main) &&
		    strcmp(name, bc_func_read))
		{
			bc_vec_push(p->strs, &id_ptr->name);
			assert(p->strs->len == p->fns.len - BC_PROG_REQ_FUNCS);
		}
#endif // DC_ENABLED
	}

	return idx;
}

#ifndef NDEBUG
void bc_program_free(BcProgram *p) {

	size_t i;

	BC_SIG_ASSERT_LOCKED;

	assert(p != NULL);

	for (i = 0; i < BC_PROG_GLOBALS_LEN; ++i) bc_vec_free(p->globals_v + i);

	bc_vec_free(&p->fns);
	bc_vec_free(&p->fn_map);
	bc_vec_free(&p->vars);
	bc_vec_free(&p->var_map);
	bc_vec_free(&p->arrs);
	bc_vec_free(&p->arr_map);
	bc_vec_free(&p->results);
	bc_vec_free(&p->stack);

#if BC_ENABLED
	if (BC_IS_BC) bc_num_free(&p->last);
#endif // BC_ENABLED

#if BC_ENABLE_EXTRA_MATH
	bc_rand_free(&p->rng);
#endif // BC_ENABLE_EXTRA_MATH

#if DC_ENABLED
	if (!BC_IS_BC) bc_vec_free(&p->tail_calls);
#endif // DC_ENABLED
}
#endif // NDEBUG

void bc_program_init(BcProgram *p) {

	BcInstPtr ip;
	size_t i;
	BcBigDig val = BC_BASE;

	BC_SIG_ASSERT_LOCKED;

	assert(p != NULL);

	memset(p, 0, sizeof(BcProgram));
	memset(&ip, 0, sizeof(BcInstPtr));

	for (i = 0; i < BC_PROG_GLOBALS_LEN; ++i) {
		bc_vec_init(p->globals_v + i, sizeof(BcBigDig), NULL);
		val = i == BC_PROG_GLOBALS_SCALE ? 0 : val;
		bc_vec_push(p->globals_v + i, &val);
		p->globals[i] = val;
	}

#if DC_ENABLED
	if (!BC_IS_BC) {

		bc_vec_init(&p->tail_calls, sizeof(size_t), NULL);
		i = 0;
		bc_vec_push(&p->tail_calls, &i);

		p->strm = UCHAR_MAX + 1;
		bc_num_setup(&p->strmb, p->strmb_num, BC_NUM_BIGDIG_LOG10);
		bc_num_bigdig2num(&p->strmb, p->strm);
	}
#endif // DC_ENABLED

#if BC_ENABLE_EXTRA_MATH
	srand((unsigned int) time(NULL));
	bc_rand_init(&p->rng);
#endif // BC_ENABLE_EXTRA_MATH

	bc_num_setup(&p->one, p->one_num, BC_PROG_ONE_CAP);
	bc_num_one(&p->one);

#if BC_ENABLED
	if (BC_IS_BC) bc_num_init(&p->last, BC_NUM_DEF_SIZE);
#endif // BC_ENABLED

	bc_vec_init(&p->fns, sizeof(BcFunc), bc_func_free);
	bc_map_init(&p->fn_map);
	bc_program_insertFunc(p, bc_func_main);
	bc_program_insertFunc(p, bc_func_read);

	bc_vec_init(&p->vars, sizeof(BcVec), bc_vec_free);
	bc_map_init(&p->var_map);

	bc_vec_init(&p->arrs, sizeof(BcVec), bc_vec_free);
	bc_map_init(&p->arr_map);

	bc_vec_init(&p->results, sizeof(BcResult), bc_result_free);
	bc_vec_init(&p->stack, sizeof(BcInstPtr), NULL);
	bc_vec_push(&p->stack, &ip);
}

void bc_program_reset(BcProgram *p) {

	BcFunc *f;
	BcInstPtr *ip;

	BC_SIG_ASSERT_LOCKED;

	bc_vec_npop(&p->stack, p->stack.len - 1);
	bc_vec_npop(&p->results, p->results.len);

	if (BC_G) bc_program_popGlobals(p, true);

	f = bc_vec_item(&p->fns, BC_PROG_MAIN);
	ip = bc_vec_top(&p->stack);
	if (BC_IS_BC) bc_program_setVecs(p, f);
	ip->idx = f->code.len;

	if (vm.sig) {
		bc_file_write(&vm.fout, bc_program_ready_msg, bc_program_ready_msg_len);
		bc_file_flush(&vm.fout);
		vm.sig = 0;
	}
}

void bc_program_exec(BcProgram *p) {

	size_t idx;
	BcResult r, *ptr;
	BcInstPtr *ip = bc_vec_top(&p->stack);
	BcFunc *func = bc_vec_item(&p->fns, ip->func);
	char *code = func->code.v;
	bool cond = false;
#if BC_ENABLED
	BcNum *num;
#endif // BC_ENABLED
#ifndef NDEBUG
	size_t jmp_bufs_len;
#endif // NDEBUG

#ifndef NDEBUG
	jmp_bufs_len = vm.jmp_bufs.len;
#endif // NDEBUG

	if (BC_IS_BC) bc_program_setVecs(p, func);
	else bc_program_setVecs(p, (BcFunc*) bc_vec_item(&p->fns, BC_PROG_MAIN));

	while (ip->idx < func->code.len) {

		BC_SIG_ASSERT_NOT_LOCKED;

		uchar inst = (uchar) code[(ip->idx)++];

		switch (inst) {

#if BC_ENABLED
			case BC_INST_JUMP_ZERO:
			{
				bc_program_prep(p, &ptr, &num, 0);
				cond = !bc_num_cmpZero(num);
				bc_vec_pop(&p->results);
			}
			// Fallthrough.
			case BC_INST_JUMP:
			{
				idx = bc_program_index(code, &ip->idx);

				if (inst == BC_INST_JUMP || cond) {

					size_t *addr = bc_vec_item(&func->labels, idx);

					assert(*addr != SIZE_MAX);

					ip->idx = *addr;
				}

				break;
			}

			case BC_INST_CALL:
			{
				assert(BC_IS_BC);

				bc_program_call(p, code, &ip->idx);

				ip = bc_vec_top(&p->stack);
				func = bc_vec_item(&p->fns, ip->func);
				code = func->code.v;

				bc_program_setVecs(p, func);

				break;
			}

			case BC_INST_INC:
			case BC_INST_DEC:
			{
				bc_program_incdec(p, inst);
				break;
			}

			case BC_INST_HALT:
			{
				vm.status = BC_STATUS_QUIT;
				BC_VM_JMP;
				break;
			}

			case BC_INST_RET:
			case BC_INST_RET0:
			case BC_INST_RET_VOID:
			{
				bc_program_return(p, inst);

				ip = bc_vec_top(&p->stack);
				func = bc_vec_item(&p->fns, ip->func);
				code = func->code.v;

				if (BC_IS_BC) bc_program_setVecs(p, func);

				break;
			}
#endif // BC_ENABLED

			case BC_INST_BOOL_OR:
			case BC_INST_BOOL_AND:
			case BC_INST_REL_EQ:
			case BC_INST_REL_LE:
			case BC_INST_REL_GE:
			case BC_INST_REL_NE:
			case BC_INST_REL_LT:
			case BC_INST_REL_GT:
			{
				bc_program_logical(p, inst);
				break;
			}

			case BC_INST_READ:
			{
				bc_program_read(p);

				ip = bc_vec_top(&p->stack);
				func = bc_vec_item(&p->fns, ip->func);
				code = func->code.v;

				if (BC_IS_BC) bc_program_setVecs(p, func);

				break;
			}

#if BC_ENABLE_EXTRA_MATH
			case BC_INST_RAND:
			{
				bc_program_rand(p);
				break;
			}
#endif // BC_ENABLE_EXTRA_MATH

			case BC_INST_MAXIBASE:
			case BC_INST_MAXOBASE:
			case BC_INST_MAXSCALE:
#if BC_ENABLE_EXTRA_MATH
			case BC_INST_MAXRAND:
#endif // BC_ENABLE_EXTRA_MATH
			{
				BcBigDig dig = vm.maxes[inst - BC_INST_MAXIBASE];
				bc_program_pushBigdig(p, dig, BC_RESULT_TEMP);
				break;
			}

			case BC_INST_VAR:
			{
				bc_program_pushVar(p, code, &ip->idx, false, false);
				break;
			}

			case BC_INST_ARRAY_ELEM:
#if BC_ENABLED
			case BC_INST_ARRAY:
#endif // BC_ENABLED
			{
				bc_program_pushArray(p, code, &ip->idx, inst);
				break;
			}

			case BC_INST_IBASE:
			case BC_INST_SCALE:
			case BC_INST_OBASE:
			{
				bc_program_pushGlobal(p, inst);
				break;
			}

#if BC_ENABLE_EXTRA_MATH
			case BC_INST_SEED:
			{
				bc_program_pushSeed(p);
				break;
			}
#endif // BC_ENABLE_EXTRA_MATH

			case BC_INST_LENGTH:
			case BC_INST_SCALE_FUNC:
			case BC_INST_SQRT:
			case BC_INST_ABS:
#if BC_ENABLE_EXTRA_MATH
			case BC_INST_IRAND:
#endif // BC_ENABLE_EXTRA_MATH
			{
				bc_program_builtin(p, inst);
				break;
			}

			case BC_INST_NUM:
			{
				r.t = BC_RESULT_CONSTANT;
				r.d.loc.loc = bc_program_index(code, &ip->idx);
				bc_vec_push(&p->results, &r);
				break;
			}

			case BC_INST_ONE:
#if BC_ENABLED
			case BC_INST_LAST:
#endif // BC_ENABLED
			{
				r.t = BC_RESULT_ONE + (inst - BC_INST_ONE);
				bc_vec_push(&p->results, &r);
				break;
			}

			case BC_INST_PRINT:
			case BC_INST_PRINT_POP:
			case BC_INST_PRINT_STR:
			{
				bc_program_print(p, inst, 0);
				break;
			}

			case BC_INST_STR:
			{
				r.t = BC_RESULT_STR;
				r.d.loc.loc = bc_program_index(code, &ip->idx);
				bc_vec_push(&p->results, &r);
				break;
			}

			case BC_INST_POWER:
			case BC_INST_MULTIPLY:
			case BC_INST_DIVIDE:
			case BC_INST_MODULUS:
			case BC_INST_PLUS:
			case BC_INST_MINUS:
#if BC_ENABLE_EXTRA_MATH
			case BC_INST_PLACES:
			case BC_INST_LSHIFT:
			case BC_INST_RSHIFT:
#endif // BC_ENABLE_EXTRA_MATH
			{
				bc_program_op(p, inst);
				break;
			}

			case BC_INST_NEG:
			case BC_INST_BOOL_NOT:
#if BC_ENABLE_EXTRA_MATH
			case BC_INST_TRUNC:
#endif // BC_ENABLE_EXTRA_MATH
			{
				bc_program_unary(p, inst);
				break;
			}

#if BC_ENABLED
			case BC_INST_ASSIGN_POWER:
			case BC_INST_ASSIGN_MULTIPLY:
			case BC_INST_ASSIGN_DIVIDE:
			case BC_INST_ASSIGN_MODULUS:
			case BC_INST_ASSIGN_PLUS:
			case BC_INST_ASSIGN_MINUS:
#if BC_ENABLE_EXTRA_MATH
			case BC_INST_ASSIGN_PLACES:
			case BC_INST_ASSIGN_LSHIFT:
			case BC_INST_ASSIGN_RSHIFT:
#endif // BC_ENABLE_EXTRA_MATH
			case BC_INST_ASSIGN:
			case BC_INST_ASSIGN_POWER_NO_VAL:
			case BC_INST_ASSIGN_MULTIPLY_NO_VAL:
			case BC_INST_ASSIGN_DIVIDE_NO_VAL:
			case BC_INST_ASSIGN_MODULUS_NO_VAL:
			case BC_INST_ASSIGN_PLUS_NO_VAL:
			case BC_INST_ASSIGN_MINUS_NO_VAL:
#if BC_ENABLE_EXTRA_MATH
			case BC_INST_ASSIGN_PLACES_NO_VAL:
			case BC_INST_ASSIGN_LSHIFT_NO_VAL:
			case BC_INST_ASSIGN_RSHIFT_NO_VAL:
#endif // BC_ENABLE_EXTRA_MATH
#endif // BC_ENABLED
			case BC_INST_ASSIGN_NO_VAL:
			{
				bc_program_assign(p, inst);
				break;
			}

			case BC_INST_POP:
			{
#ifndef BC_PROG_NO_STACK_CHECK
				if (!BC_IS_BC) {
					if (BC_ERR(!BC_PROG_STACK(&p->results, 1)))
						bc_vm_err(BC_ERROR_EXEC_STACK);
				}
#endif // BC_PROG_NO_STACK_CHECK

				assert(BC_PROG_STACK(&p->results, 1));

				bc_vec_pop(&p->results);
				break;
			}

#if DC_ENABLED
			case BC_INST_POP_EXEC:
			{
				assert(BC_PROG_STACK(&p->stack, 2));
				bc_vec_pop(&p->stack);
				bc_vec_pop(&p->tail_calls);
				ip = bc_vec_top(&p->stack);
				func = bc_vec_item(&p->fns, ip->func);
				code = func->code.v;
				break;
			}

			case BC_INST_MODEXP:
			{
				bc_program_modexp(p);
				break;
			}

			case BC_INST_DIVMOD:
			{
				bc_program_divmod(p);
				break;
			}

			case BC_INST_EXECUTE:
			case BC_INST_EXEC_COND:
			{
				cond = (inst == BC_INST_EXEC_COND);
				bc_program_execStr(p, code, &ip->idx, cond, func->code.len);
				ip = bc_vec_top(&p->stack);
				func = bc_vec_item(&p->fns, ip->func);
				code = func->code.v;
				break;
			}

			case BC_INST_PRINT_STACK:
			{
				bc_program_printStack(p);
				break;
			}

			case BC_INST_CLEAR_STACK:
			{
				bc_vec_npop(&p->results, p->results.len);
				break;
			}

			case BC_INST_STACK_LEN:
			{
				bc_program_stackLen(p);
				break;
			}

			case BC_INST_DUPLICATE:
			{
				if (BC_ERR(!BC_PROG_STACK(&p->results, 1)))
					bc_vm_err(BC_ERROR_EXEC_STACK);

				assert(BC_PROG_STACK(&p->results, 1));

				ptr = bc_vec_top(&p->results);

				BC_SIG_LOCK;

				bc_result_copy(&r, ptr);
				bc_vec_push(&p->results, &r);

				BC_SIG_UNLOCK;

				break;
			}

			case BC_INST_SWAP:
			{
				BcResult *ptr2;

				if (BC_ERR(!BC_PROG_STACK(&p->results, 2)))
					bc_vm_err(BC_ERROR_EXEC_STACK);

				assert(BC_PROG_STACK(&p->results, 2));

				ptr = bc_vec_item_rev(&p->results, 0);
				ptr2 = bc_vec_item_rev(&p->results, 1);
				memcpy(&r, ptr, sizeof(BcResult));
				memcpy(ptr, ptr2, sizeof(BcResult));
				memcpy(ptr2, &r, sizeof(BcResult));

				break;
			}

			case BC_INST_ASCIIFY:
			{
				bc_program_asciify(p);
				ip = bc_vec_top(&p->stack);
				func = bc_vec_item(&p->fns, ip->func);
				code = func->code.v;
				break;
			}

			case BC_INST_PRINT_STREAM:
			{
				bc_program_printStream(p);
				break;
			}

			case BC_INST_LOAD:
			case BC_INST_PUSH_VAR:
			{
				bool copy = (inst == BC_INST_LOAD);
				bc_program_pushVar(p, code, &ip->idx, true, copy);
				break;
			}

			case BC_INST_PUSH_TO_VAR:
			{
				idx = bc_program_index(code, &ip->idx);
				bc_program_copyToVar(p, idx, BC_TYPE_VAR, true);
				break;
			}

			case BC_INST_QUIT:
			case BC_INST_NQUIT:
			{
				bc_program_nquit(p, inst);
				ip = bc_vec_top(&p->stack);
				func = bc_vec_item(&p->fns, ip->func);
				code = func->code.v;
				break;
			}
#endif // DC_ENABLED
#ifndef NDEBUG
			default:
			{
				abort();
			}
#endif // NDEBUG
		}

#ifndef NDEBUG
		// This is to allow me to use a debugger to see the last instruction,
		// which will point to which function was the problem.
		assert(jmp_bufs_len == vm.jmp_bufs.len);
#endif // NDEBUG
	}
}

#if BC_DEBUG_CODE
#if BC_ENABLED && DC_ENABLED
void bc_program_printStackDebug(BcProgram *p) {
	bc_file_puts(&vm.fout, "-------------- Stack ----------\n");
	bc_program_printStack(p);
	bc_file_puts(&vm.fout, "-------------- Stack End ------\n");
}

static void bc_program_printIndex(const char *restrict code,
                                  size_t *restrict bgn)
{
	uchar byte, i, bytes = (uchar) code[(*bgn)++];
	ulong val = 0;

	for (byte = 1, i = 0; byte && i < bytes; ++i) {
		byte = (uchar) code[(*bgn)++];
		if (byte) val |= ((ulong) byte) << (CHAR_BIT * i);
	}

	bc_vm_printf(" (%lu) ", val);
}

static void bc_program_printStr(const BcProgram *p, const char *restrict code,
                         size_t *restrict bgn)
{
	size_t idx = bc_program_index(code, bgn);
	char *s;

	s = *((char**) bc_vec_item(p->strs, idx));

	bc_vm_printf(" (\"%s\") ", s);
}

void bc_program_printInst(const BcProgram *p, const char *restrict code,
                          size_t *restrict bgn)
{
	uchar inst = (uchar) code[(*bgn)++];

	bc_vm_printf("Inst[%zu]: %s [%lu]; ", *bgn - 1,
	             bc_inst_names[inst], (unsigned long) inst);

	if (inst == BC_INST_VAR || inst == BC_INST_ARRAY_ELEM ||
	    inst == BC_INST_ARRAY)
	{
		bc_program_printIndex(code, bgn);
	}
	else if (inst == BC_INST_STR) bc_program_printStr(p, code, bgn);
	else if (inst == BC_INST_NUM) {
		size_t idx = bc_program_index(code, bgn);
		BcConst *c = bc_vec_item(p->consts, idx);
		bc_vm_printf("(%s)", c->val);
	}
	else if (inst == BC_INST_CALL ||
	         (inst > BC_INST_STR && inst <= BC_INST_JUMP_ZERO))
	{
		bc_program_printIndex(code, bgn);
		if (inst == BC_INST_CALL) bc_program_printIndex(code, bgn);
	}

	bc_vm_putchar('\n');
}

void bc_program_code(const BcProgram* p) {

	BcFunc *f;
	char *code;
	BcInstPtr ip;
	size_t i;

	for (i = 0; i < p->fns.len; ++i) {

		ip.idx = ip.len = 0;
		ip.func = i;

		f = bc_vec_item(&p->fns, ip.func);
		code = f->code.v;

		bc_vm_printf("func[%zu]:\n", ip.func);
		while (ip.idx < f->code.len) bc_program_printInst(p, code, &ip.idx);
		bc_file_puts(&vm.fout, "\n\n");
	}
}
#endif // BC_ENABLED && DC_ENABLED
#endif // BC_DEBUG_CODE
