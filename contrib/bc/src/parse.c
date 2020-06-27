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
 * Code common to the parsers.
 *
 */

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <limits.h>

#include <status.h>
#include <vector.h>
#include <lex.h>
#include <parse.h>
#include <program.h>
#include <vm.h>

void bc_parse_updateFunc(BcParse *p, size_t fidx) {
	p->fidx = fidx;
	p->func = bc_vec_item(&p->prog->fns, fidx);
}

inline void bc_parse_pushName(const BcParse *p, char *name, bool var) {
	bc_parse_pushIndex(p, bc_program_search(p->prog, name, var));
}

static void bc_parse_update(BcParse *p, uchar inst, size_t idx) {
	bc_parse_updateFunc(p, p->fidx);
	bc_parse_push(p, inst);
	bc_parse_pushIndex(p, idx);
}

void bc_parse_addString(BcParse *p) {

	BcFunc *f = BC_IS_BC ? p->func : bc_vec_item(&p->prog->fns, BC_PROG_MAIN);
	size_t idx;

	BC_SIG_LOCK;

	if (BC_IS_BC) {
		const char *str = bc_vm_strdup(p->l.str.v);
		idx = f->strs.len;
		bc_vec_push(&f->strs, &str);
	}
#if DC_ENABLED
	else idx = bc_program_insertFunc(p->prog, p->l.str.v) - BC_PROG_REQ_FUNCS;
#endif // DC_ENABLED

#ifndef NDEBUG
	f = BC_IS_BC ? p->func : bc_vec_item(&p->prog->fns, BC_PROG_MAIN);
	assert(f->strs.len > idx);
#endif // NDEBUG

	bc_parse_update(p, BC_INST_STR, idx);

	BC_SIG_UNLOCK;
}

static void bc_parse_addNum(BcParse *p, const char *string) {

	BcFunc *f = BC_IS_BC ? p->func : bc_vec_item(&p->prog->fns, BC_PROG_MAIN);
	size_t idx;
	BcConst c;

	if (bc_parse_one[0] == string[0] && bc_parse_one[1] == string[1]) {
		bc_parse_push(p, BC_INST_ONE);
		return;
	}

	idx = f->consts.len;

	BC_SIG_LOCK;

	c.val = bc_vm_strdup(string);
	c.base = BC_NUM_BIGDIG_MAX;

	bc_num_clear(&c.num);
	bc_vec_push(&f->consts, &c);

	bc_parse_update(p, BC_INST_NUM, idx);

	BC_SIG_UNLOCK;
}

void bc_parse_number(BcParse *p) {

#if BC_ENABLE_EXTRA_MATH
	char *exp = strchr(p->l.str.v, 'e');
	size_t idx = SIZE_MAX;

	if (exp != NULL) {
		idx = ((size_t) (exp - p->l.str.v));
		*exp = 0;
	}
#endif // BC_ENABLE_EXTRA_MATH

	bc_parse_addNum(p, p->l.str.v);

#if BC_ENABLE_EXTRA_MATH
	if (exp != NULL) {

		bool neg;

		neg = (*((char*) bc_vec_item(&p->l.str, idx + 1)) == BC_LEX_NEG_CHAR);

		bc_parse_addNum(p, bc_vec_item(&p->l.str, idx + 1 + neg));
		bc_parse_push(p, BC_INST_LSHIFT + neg);
	}
#endif // BC_ENABLE_EXTRA_MATH
}

void bc_parse_text(BcParse *p, const char *text) {
	// Make sure the pointer isn't invalidated.
	p->func = bc_vec_item(&p->prog->fns, p->fidx);
	bc_lex_text(&p->l, text);
}

void bc_parse_reset(BcParse *p) {

	BC_SIG_ASSERT_LOCKED;

	if (p->fidx != BC_PROG_MAIN) {
		bc_func_reset(p->func);
		bc_parse_updateFunc(p, BC_PROG_MAIN);
	}

	p->l.i = p->l.len;
	p->l.t = BC_LEX_EOF;
	p->auto_part = false;

#if BC_ENABLED
	if (BC_IS_BC) {
		bc_vec_npop(&p->flags, p->flags.len - 1);
		bc_vec_npop(&p->exits, p->exits.len);
		bc_vec_npop(&p->conds, p->conds.len);
		bc_vec_npop(&p->ops, p->ops.len);
	}
#endif // BC_ENABLED

	bc_program_reset(p->prog);

	if (BC_ERR(vm.status)) BC_VM_JMP;
}

void bc_parse_free(BcParse *p) {

	BC_SIG_ASSERT_LOCKED;

	assert(p != NULL);

#if BC_ENABLED
	if (BC_IS_BC) {
		bc_vec_free(&p->flags);
		bc_vec_free(&p->exits);
		bc_vec_free(&p->conds);
		bc_vec_free(&p->ops);
		bc_vec_free(&p->buf);
	}
#endif // BC_ENABLED

	bc_lex_free(&p->l);
}

void bc_parse_init(BcParse *p, BcProgram *prog, size_t func) {

#if BC_ENABLED
	uint16_t flag = 0;
#endif // BC_ENABLED

	BC_SIG_ASSERT_LOCKED;

	assert(p != NULL && prog != NULL);

#if BC_ENABLED
	if (BC_IS_BC) {
		bc_vec_init(&p->flags, sizeof(uint16_t), NULL);
		bc_vec_push(&p->flags, &flag);
		bc_vec_init(&p->exits, sizeof(BcInstPtr), NULL);
		bc_vec_init(&p->conds, sizeof(size_t), NULL);
		bc_vec_init(&p->ops, sizeof(BcLexType), NULL);
		bc_vec_init(&p->buf, sizeof(char), NULL);
	}
#endif // BC_ENABLED

	bc_lex_init(&p->l);

	p->prog = prog;
	p->auto_part = false;
	bc_parse_updateFunc(p, func);
}
