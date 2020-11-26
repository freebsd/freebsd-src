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
 * The parser for bc.
 *
 */

#if BC_ENABLED

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <setjmp.h>

#include <bc.h>
#include <num.h>
#include <vm.h>

static void bc_parse_else(BcParse *p);
static void bc_parse_stmt(BcParse *p);
static BcParseStatus bc_parse_expr_err(BcParse *p, uint8_t flags,
                                       BcParseNext next);

static bool bc_parse_inst_isLeaf(BcInst t) {
	return (t >= BC_INST_NUM && t <= BC_INST_MAXSCALE) ||
#if BC_ENABLE_EXTRA_MATH
	        t == BC_INST_TRUNC ||
#endif // BC_ENABLE_EXTRA_MATH
	        t <= BC_INST_DEC;
}

static bool bc_parse_isDelimiter(const BcParse *p) {

	BcLexType t = p->l.t;
	bool good = false;

	if (BC_PARSE_DELIMITER(t)) return true;

	if (t == BC_LEX_KW_ELSE) {

		size_t i;
		uint16_t *fptr = NULL, flags = BC_PARSE_FLAG_ELSE;

		for (i = 0; i < p->flags.len && BC_PARSE_BLOCK_STMT(flags); ++i) {

			fptr = bc_vec_item_rev(&p->flags, i);
			flags = *fptr;

			if ((flags & BC_PARSE_FLAG_BRACE) && p->l.last != BC_LEX_RBRACE)
				return false;
		}

		good = ((flags & BC_PARSE_FLAG_IF) != 0);
	}
	else if (t == BC_LEX_RBRACE) {

		size_t i;

		for (i = 0; !good && i < p->flags.len; ++i) {
			uint16_t *fptr = bc_vec_item_rev(&p->flags, i);
			good = (((*fptr) & BC_PARSE_FLAG_BRACE) != 0);
		}
	}

	return good;
}

static void bc_parse_setLabel(BcParse *p) {

	BcFunc *func = p->func;
	BcInstPtr *ip = bc_vec_top(&p->exits);
	size_t *label;

	assert(func == bc_vec_item(&p->prog->fns, p->fidx));

	label = bc_vec_item(&func->labels, ip->idx);
	*label = func->code.len;

	bc_vec_pop(&p->exits);
}

static void bc_parse_createLabel(BcParse *p, size_t idx) {
	bc_vec_push(&p->func->labels, &idx);
}

static void bc_parse_createCondLabel(BcParse *p, size_t idx) {
	bc_parse_createLabel(p, p->func->code.len);
	bc_vec_push(&p->conds, &idx);
}

static void bc_parse_createExitLabel(BcParse *p, size_t idx, bool loop) {

	BcInstPtr ip;

	assert(p->func == bc_vec_item(&p->prog->fns, p->fidx));

	ip.func = loop;
	ip.idx = idx;
	ip.len = 0;

	bc_vec_push(&p->exits, &ip);
	bc_parse_createLabel(p, SIZE_MAX);
}

static void bc_parse_operator(BcParse *p, BcLexType type,
                              size_t start, size_t *nexprs)
{
	BcLexType t;
	uchar l, r = BC_PARSE_OP_PREC(type);
	uchar left = BC_PARSE_OP_LEFT(type);

	while (p->ops.len > start) {

		t = BC_PARSE_TOP_OP(p);
		if (t == BC_LEX_LPAREN) break;

		l = BC_PARSE_OP_PREC(t);
		if (l >= r && (l != r || !left)) break;

		bc_parse_push(p, BC_PARSE_TOKEN_INST(t));
		bc_vec_pop(&p->ops);
		*nexprs -= !BC_PARSE_OP_PREFIX(t);
	}

	bc_vec_push(&p->ops, &type);
}

static void bc_parse_rightParen(BcParse *p, size_t *nexs) {

	BcLexType top;

	while ((top = BC_PARSE_TOP_OP(p)) != BC_LEX_LPAREN) {
		bc_parse_push(p, BC_PARSE_TOKEN_INST(top));
		bc_vec_pop(&p->ops);
		*nexs -= !BC_PARSE_OP_PREFIX(top);
	}

	bc_vec_pop(&p->ops);

	bc_lex_next(&p->l);
}

static void bc_parse_params(BcParse *p, uint8_t flags) {

	bool comma = false;
	size_t nparams;

	bc_lex_next(&p->l);

	flags &= ~(BC_PARSE_PRINT | BC_PARSE_REL);
	flags |= (BC_PARSE_ARRAY | BC_PARSE_NEEDVAL);

	for (nparams = 0; p->l.t != BC_LEX_RPAREN; ++nparams) {

		bc_parse_expr_status(p, flags, bc_parse_next_param);

		comma = (p->l.t == BC_LEX_COMMA);
		if (comma) bc_lex_next(&p->l);
	}

	if (BC_ERR(comma)) bc_parse_err(p, BC_ERR_PARSE_TOKEN);
	bc_parse_push(p, BC_INST_CALL);
	bc_parse_pushIndex(p, nparams);
}

static void bc_parse_call(BcParse *p, const char *name, uint8_t flags) {

	size_t idx;

	bc_parse_params(p, flags);

	// We just assert this because bc_parse_params() should
	// ensure that the next token is what it should be.
	assert(p->l.t == BC_LEX_RPAREN);

	// We cannot use bc_program_insertFunc() here
	// because it will overwrite an existing function.
	idx = bc_map_index(&p->prog->fn_map, name);

	if (idx == BC_VEC_INVALID_IDX) {

		BC_SIG_LOCK;

		idx = bc_program_insertFunc(p->prog, name);

		BC_SIG_UNLOCK;

		assert(idx != BC_VEC_INVALID_IDX);

		// Make sure that this pointer was not invalidated.
		p->func = bc_vec_item(&p->prog->fns, p->fidx);
	}
	else idx = ((BcId*) bc_vec_item(&p->prog->fn_map, idx))->idx;

	bc_parse_pushIndex(p, idx);

	bc_lex_next(&p->l);
}

static void bc_parse_name(BcParse *p, BcInst *type,
                          bool *can_assign, uint8_t flags)
{
	char *name;

	BC_SIG_LOCK;

	name = bc_vm_strdup(p->l.str.v);

	BC_SETJMP_LOCKED(err);

	BC_SIG_UNLOCK;

	bc_lex_next(&p->l);

	if (p->l.t == BC_LEX_LBRACKET) {

		bc_lex_next(&p->l);

		if (p->l.t == BC_LEX_RBRACKET) {

			if (BC_ERR(!(flags & BC_PARSE_ARRAY)))
				bc_parse_err(p, BC_ERR_PARSE_EXPR);

			*type = BC_INST_ARRAY;
			*can_assign = false;
		}
		else {

			uint8_t flags2 = (flags & ~(BC_PARSE_PRINT | BC_PARSE_REL)) |
			    BC_PARSE_NEEDVAL;

			bc_parse_expr_status(p, flags2, bc_parse_next_elem);

			if (BC_ERR(p->l.t != BC_LEX_RBRACKET))
				bc_parse_err(p, BC_ERR_PARSE_TOKEN);

			*type = BC_INST_ARRAY_ELEM;
			*can_assign = true;
		}

		bc_lex_next(&p->l);

		bc_parse_push(p, *type);
		bc_parse_pushName(p, name, false);
	}
	else if (p->l.t == BC_LEX_LPAREN) {

		if (BC_ERR(flags & BC_PARSE_NOCALL))
			bc_parse_err(p, BC_ERR_PARSE_TOKEN);

		*type = BC_INST_CALL;
		*can_assign = false;

		bc_parse_call(p, name, flags);
	}
	else {
		*type = BC_INST_VAR;
		*can_assign = true;
		bc_parse_push(p, BC_INST_VAR);
		bc_parse_pushName(p, name, true);
	}

err:
	BC_SIG_MAYLOCK;
	free(name);
	BC_LONGJMP_CONT;
}

static void bc_parse_noArgBuiltin(BcParse *p, BcInst inst) {

	bc_lex_next(&p->l);
	if (BC_ERR(p->l.t != BC_LEX_LPAREN)) bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	bc_lex_next(&p->l);
	if ((p->l.t != BC_LEX_RPAREN)) bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	bc_parse_push(p, inst);

	bc_lex_next(&p->l);
}

static void bc_parse_builtin(BcParse *p, BcLexType type,
                             uint8_t flags, BcInst *prev)
{
	bc_lex_next(&p->l);
	if (BC_ERR(p->l.t != BC_LEX_LPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	bc_lex_next(&p->l);

	flags &= ~(BC_PARSE_PRINT | BC_PARSE_REL);
	flags |= BC_PARSE_NEEDVAL;
	if (type == BC_LEX_KW_LENGTH) flags |= BC_PARSE_ARRAY;

	bc_parse_expr_status(p, flags, bc_parse_next_rel);

	if (BC_ERR(p->l.t != BC_LEX_RPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	*prev = type - BC_LEX_KW_LENGTH + BC_INST_LENGTH;
	bc_parse_push(p, *prev);

	bc_lex_next(&p->l);
}

static void bc_parse_scale(BcParse *p, BcInst *type,
                               bool *can_assign, uint8_t flags)
{
	bc_lex_next(&p->l);

	if (p->l.t != BC_LEX_LPAREN) {
		*type = BC_INST_SCALE;
		*can_assign = true;
		bc_parse_push(p, BC_INST_SCALE);
		return;
	}

	*type = BC_INST_SCALE_FUNC;
	*can_assign = false;
	flags &= ~(BC_PARSE_PRINT | BC_PARSE_REL);
	flags |= BC_PARSE_NEEDVAL;

	bc_lex_next(&p->l);

	bc_parse_expr_status(p, flags, bc_parse_next_rel);
	if (BC_ERR(p->l.t != BC_LEX_RPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	bc_parse_push(p, BC_INST_SCALE_FUNC);

	bc_lex_next(&p->l);
}

static void bc_parse_incdec(BcParse *p, BcInst *prev, bool *can_assign,
                            size_t *nexs, uint8_t flags)
{
	BcLexType type;
	uchar inst;
	BcInst etype = *prev;
	BcLexType last = p->l.last;

	assert(prev != NULL && can_assign != NULL);

	if (BC_ERR(last == BC_LEX_OP_INC || last == BC_LEX_OP_DEC ||
	           last == BC_LEX_RPAREN))
	{
		bc_parse_err(p, BC_ERR_PARSE_ASSIGN);
	}

	if (BC_PARSE_INST_VAR(etype)) {

		if (!*can_assign) bc_parse_err(p, BC_ERR_PARSE_ASSIGN);

		*prev = inst = BC_INST_INC + (p->l.t != BC_LEX_OP_INC);
		bc_parse_push(p, inst);
		bc_lex_next(&p->l);
		*can_assign = false;
	}
	else {

		*prev = inst = BC_INST_ASSIGN_PLUS + (p->l.t != BC_LEX_OP_INC);

		bc_lex_next(&p->l);
		type = p->l.t;

		// Because we parse the next part of the expression
		// right here, we need to increment this.
		*nexs = *nexs + 1;

		if (type == BC_LEX_NAME) {
			uint8_t flags2 = flags & ~BC_PARSE_ARRAY;
			bc_parse_name(p, prev, can_assign, flags2 | BC_PARSE_NOCALL);
		}
		else if (type >= BC_LEX_KW_LAST && type <= BC_LEX_KW_OBASE) {
			bc_parse_push(p, type - BC_LEX_KW_LAST + BC_INST_LAST);
			bc_lex_next(&p->l);
		}
		else if (BC_NO_ERR(type == BC_LEX_KW_SCALE)) {

			bc_lex_next(&p->l);

			if (BC_ERR(p->l.t == BC_LEX_LPAREN))
				bc_parse_err(p, BC_ERR_PARSE_TOKEN);
			else bc_parse_push(p, BC_INST_SCALE);
		}
		else bc_parse_err(p, BC_ERR_PARSE_TOKEN);

		*can_assign = false;

		bc_parse_push(p, BC_INST_ONE);
		bc_parse_push(p, inst);
	}
}

static void bc_parse_minus(BcParse *p, BcInst *prev, size_t ops_bgn,
                           bool rparen, bool binlast, size_t *nexprs)
{
	BcLexType type;

	bc_lex_next(&p->l);

	type = BC_PARSE_LEAF(*prev, binlast, rparen) ? BC_LEX_OP_MINUS : BC_LEX_NEG;
	*prev = BC_PARSE_TOKEN_INST(type);

	// We can just push onto the op stack because this is the largest
	// precedence operator that gets pushed. Inc/dec does not.
	if (type != BC_LEX_OP_MINUS) bc_vec_push(&p->ops, &type);
	else bc_parse_operator(p, type, ops_bgn, nexprs);
}

static void bc_parse_str(BcParse *p, char inst) {
	bc_parse_addString(p);
	bc_parse_push(p, inst);
	bc_lex_next(&p->l);
}

static void bc_parse_print(BcParse *p) {

	BcLexType t;
	bool comma = false;

	bc_lex_next(&p->l);

	t = p->l.t;

	if (bc_parse_isDelimiter(p)) bc_parse_err(p, BC_ERR_PARSE_PRINT);

	do {
		if (t == BC_LEX_STR) bc_parse_str(p, BC_INST_PRINT_POP);
		else {
			bc_parse_expr_status(p, BC_PARSE_NEEDVAL, bc_parse_next_print);
			bc_parse_push(p, BC_INST_PRINT_POP);
		}

		comma = (p->l.t == BC_LEX_COMMA);

		if (comma) bc_lex_next(&p->l);
		else {
			if (!bc_parse_isDelimiter(p))
				bc_parse_err(p, BC_ERR_PARSE_TOKEN);
			else break;
		}

		t = p->l.t;
	} while (true);

	if (BC_ERR(comma)) bc_parse_err(p, BC_ERR_PARSE_TOKEN);
}

static void bc_parse_return(BcParse *p) {

	BcLexType t;
	bool paren;
	uchar inst = BC_INST_RET0;

	if (BC_ERR(!BC_PARSE_FUNC(p))) bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	if (p->func->voidfn) inst = BC_INST_RET_VOID;

	bc_lex_next(&p->l);

	t = p->l.t;
	paren = t == BC_LEX_LPAREN;

	if (bc_parse_isDelimiter(p)) bc_parse_push(p, inst);
	else {

		BcParseStatus s;

		s = bc_parse_expr_err(p, BC_PARSE_NEEDVAL, bc_parse_next_expr);

		if (s == BC_PARSE_STATUS_EMPTY_EXPR) {
			bc_parse_push(p, inst);
			bc_lex_next(&p->l);
		}

		if (!paren || p->l.last != BC_LEX_RPAREN) {
			bc_parse_err(p, BC_ERR_POSIX_RET);
		}
		else if (BC_ERR(p->func->voidfn))
			bc_parse_verr(p, BC_ERR_PARSE_RET_VOID, p->func->name);

		bc_parse_push(p, BC_INST_RET);
	}
}

static void bc_parse_noElse(BcParse *p) {
	uint16_t *flag_ptr = BC_PARSE_TOP_FLAG_PTR(p);
	*flag_ptr = (*flag_ptr & ~(BC_PARSE_FLAG_IF_END));
	bc_parse_setLabel(p);
}

static void bc_parse_endBody(BcParse *p, bool brace) {

	bool has_brace, new_else = false;

	if (BC_ERR(p->flags.len <= 1)) bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	if (brace) {

		assert(p->l.t == BC_LEX_RBRACE);

		bc_lex_next(&p->l);
		if (BC_ERR(!bc_parse_isDelimiter(p)))
			bc_parse_err(p, BC_ERR_PARSE_TOKEN);
	}

	has_brace = (BC_PARSE_BRACE(p) != 0);

	do {
		size_t len = p->flags.len;
		bool loop;

		if (has_brace && !brace) bc_parse_err(p, BC_ERR_PARSE_TOKEN);

		loop = (BC_PARSE_LOOP_INNER(p) != 0);

		if (loop || BC_PARSE_ELSE(p)) {

			if (loop) {

				size_t *label = bc_vec_top(&p->conds);

				bc_parse_push(p, BC_INST_JUMP);
				bc_parse_pushIndex(p, *label);

				bc_vec_pop(&p->conds);
			}

			bc_parse_setLabel(p);
			bc_vec_pop(&p->flags);
		}
		else if (BC_PARSE_FUNC_INNER(p)) {
			BcInst inst = (p->func->voidfn ? BC_INST_RET_VOID : BC_INST_RET0);
			bc_parse_push(p, inst);
			bc_parse_updateFunc(p, BC_PROG_MAIN);
			bc_vec_pop(&p->flags);
		}
		else if (BC_PARSE_BRACE(p) && !BC_PARSE_IF(p)) bc_vec_pop(&p->flags);

		// This needs to be last to parse nested if's properly.
		if (BC_PARSE_IF(p) && (len == p->flags.len || !BC_PARSE_BRACE(p))) {

			while (p->l.t == BC_LEX_NLINE) bc_lex_next(&p->l);

			bc_vec_pop(&p->flags);

			if (!BC_S) {

				*(BC_PARSE_TOP_FLAG_PTR(p)) |= BC_PARSE_FLAG_IF_END;
				new_else = (p->l.t == BC_LEX_KW_ELSE);

				if (new_else) bc_parse_else(p);
				else if (!has_brace && (!BC_PARSE_IF_END(p) || brace))
					bc_parse_noElse(p);
			}
			else bc_parse_noElse(p);
		}

		if (brace && has_brace) brace = false;

	} while (p->flags.len > 1 && !new_else && (!BC_PARSE_IF_END(p) || brace) &&
	         !(has_brace = (BC_PARSE_BRACE(p) != 0)));

	if (BC_ERR(p->flags.len == 1 && brace))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);
	else if (brace && BC_PARSE_BRACE(p)) {

		uint16_t flags = BC_PARSE_TOP_FLAG(p);

		if (!(flags & (BC_PARSE_FLAG_FUNC_INNER | BC_PARSE_FLAG_LOOP_INNER)) &&
		    !(flags & (BC_PARSE_FLAG_IF | BC_PARSE_FLAG_ELSE)) &&
		    !(flags & (BC_PARSE_FLAG_IF_END)))
		{
			bc_vec_pop(&p->flags);
		}
	}
}

static void bc_parse_startBody(BcParse *p, uint16_t flags) {
	assert(flags);
	flags |= (BC_PARSE_TOP_FLAG(p) & (BC_PARSE_FLAG_FUNC | BC_PARSE_FLAG_LOOP));
	flags |= BC_PARSE_FLAG_BODY;
	bc_vec_push(&p->flags, &flags);
}

static void bc_parse_if(BcParse *p) {

	size_t idx;
	uint8_t flags = (BC_PARSE_REL | BC_PARSE_NEEDVAL);

	bc_lex_next(&p->l);
	if (BC_ERR(p->l.t != BC_LEX_LPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	bc_lex_next(&p->l);
	bc_parse_expr_status(p, flags, bc_parse_next_rel);
	if (BC_ERR(p->l.t != BC_LEX_RPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	bc_lex_next(&p->l);
	bc_parse_push(p, BC_INST_JUMP_ZERO);

	idx = p->func->labels.len;

	bc_parse_pushIndex(p, idx);
	bc_parse_createExitLabel(p, idx, false);
	bc_parse_startBody(p, BC_PARSE_FLAG_IF);
}

static void bc_parse_else(BcParse *p) {

	size_t idx = p->func->labels.len;

	if (BC_ERR(!BC_PARSE_IF_END(p)))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	bc_parse_push(p, BC_INST_JUMP);
	bc_parse_pushIndex(p, idx);

	bc_parse_noElse(p);

	bc_parse_createExitLabel(p, idx, false);
	bc_parse_startBody(p, BC_PARSE_FLAG_ELSE);

	bc_lex_next(&p->l);
}

static void bc_parse_while(BcParse *p) {

	size_t idx;
	uint8_t flags = (BC_PARSE_REL | BC_PARSE_NEEDVAL);

	bc_lex_next(&p->l);
	if (BC_ERR(p->l.t != BC_LEX_LPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);
	bc_lex_next(&p->l);

	bc_parse_createCondLabel(p, p->func->labels.len);
	idx = p->func->labels.len;
	bc_parse_createExitLabel(p, idx, true);

	bc_parse_expr_status(p, flags, bc_parse_next_rel);
	if (BC_ERR(p->l.t != BC_LEX_RPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);
	bc_lex_next(&p->l);

	bc_parse_push(p, BC_INST_JUMP_ZERO);
	bc_parse_pushIndex(p, idx);
	bc_parse_startBody(p, BC_PARSE_FLAG_LOOP | BC_PARSE_FLAG_LOOP_INNER);
}

static void bc_parse_for(BcParse *p) {

	size_t cond_idx, exit_idx, body_idx, update_idx;

	bc_lex_next(&p->l);
	if (BC_ERR(p->l.t != BC_LEX_LPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);
	bc_lex_next(&p->l);

	if (p->l.t != BC_LEX_SCOLON)
		bc_parse_expr_status(p, 0, bc_parse_next_for);
	else bc_parse_err(p, BC_ERR_POSIX_FOR);

	if (BC_ERR(p->l.t != BC_LEX_SCOLON))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);
	bc_lex_next(&p->l);

	cond_idx = p->func->labels.len;
	update_idx = cond_idx + 1;
	body_idx = update_idx + 1;
	exit_idx = body_idx + 1;

	bc_parse_createLabel(p, p->func->code.len);

	if (p->l.t != BC_LEX_SCOLON) {
		uint8_t flags = (BC_PARSE_REL | BC_PARSE_NEEDVAL);
		bc_parse_expr_status(p, flags, bc_parse_next_for);
	}
	else {

		// Set this for the next call to bc_parse_number.
		// This is safe to set because the current token
		// is a semicolon, which has no string requirement.
		bc_vec_string(&p->l.str, sizeof(bc_parse_one) - 1, bc_parse_one);
		bc_parse_number(p);

		bc_parse_err(p, BC_ERR_POSIX_FOR);
	}

	if (BC_ERR(p->l.t != BC_LEX_SCOLON))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	bc_lex_next(&p->l);

	bc_parse_push(p, BC_INST_JUMP_ZERO);
	bc_parse_pushIndex(p, exit_idx);
	bc_parse_push(p, BC_INST_JUMP);
	bc_parse_pushIndex(p, body_idx);

	bc_parse_createCondLabel(p, update_idx);

	if (p->l.t != BC_LEX_RPAREN)
		bc_parse_expr_status(p, 0, bc_parse_next_rel);
	else bc_parse_err(p, BC_ERR_POSIX_FOR);

	if (BC_ERR(p->l.t != BC_LEX_RPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);
	bc_parse_push(p, BC_INST_JUMP);
	bc_parse_pushIndex(p, cond_idx);
	bc_parse_createLabel(p, p->func->code.len);

	bc_parse_createExitLabel(p, exit_idx, true);
	bc_lex_next(&p->l);
	bc_parse_startBody(p, BC_PARSE_FLAG_LOOP | BC_PARSE_FLAG_LOOP_INNER);
}

static void bc_parse_loopExit(BcParse *p, BcLexType type) {

	size_t i;
	BcInstPtr *ip;

	if (BC_ERR(!BC_PARSE_LOOP(p))) bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	if (type == BC_LEX_KW_BREAK) {

		if (BC_ERR(!p->exits.len)) bc_parse_err(p, BC_ERR_PARSE_TOKEN);

		i = p->exits.len - 1;
		ip = bc_vec_item(&p->exits, i);

		while (!ip->func && i < p->exits.len) ip = bc_vec_item(&p->exits, i--);
		assert(ip != NULL && (i < p->exits.len || ip->func));
		i = ip->idx;
	}
	else i = *((size_t*) bc_vec_top(&p->conds));

	bc_parse_push(p, BC_INST_JUMP);
	bc_parse_pushIndex(p, i);

	bc_lex_next(&p->l);
}

static void bc_parse_func(BcParse *p) {

	bool comma = false, voidfn;
	uint16_t flags;
	size_t idx;

	bc_lex_next(&p->l);

	if (BC_ERR(p->l.t != BC_LEX_NAME))
		bc_parse_err(p, BC_ERR_PARSE_FUNC);

	voidfn = (!BC_IS_POSIX && p->l.t == BC_LEX_NAME &&
	          !strcmp(p->l.str.v, "void"));

	bc_lex_next(&p->l);

	voidfn = (voidfn && p->l.t == BC_LEX_NAME);

	if (voidfn) {
		bc_parse_err(p, BC_ERR_POSIX_VOID);
		bc_lex_next(&p->l);
	}

	if (BC_ERR(p->l.t != BC_LEX_LPAREN))
		bc_parse_err(p, BC_ERR_PARSE_FUNC);

	assert(p->prog->fns.len == p->prog->fn_map.len);

	BC_SIG_LOCK;

	idx = bc_program_insertFunc(p->prog, p->l.str.v);

	BC_SIG_UNLOCK;

	assert(idx);
	bc_parse_updateFunc(p, idx);
	p->func->voidfn = voidfn;

	bc_lex_next(&p->l);

	while (p->l.t != BC_LEX_RPAREN) {

		BcType t = BC_TYPE_VAR;

		if (p->l.t == BC_LEX_OP_MULTIPLY) {
			t = BC_TYPE_REF;
			bc_lex_next(&p->l);
			bc_parse_err(p, BC_ERR_POSIX_REF);
		}

		if (BC_ERR(p->l.t != BC_LEX_NAME))
			bc_parse_err(p, BC_ERR_PARSE_FUNC);

		p->func->nparams += 1;

		bc_vec_string(&p->buf, p->l.str.len, p->l.str.v);

		bc_lex_next(&p->l);

		if (p->l.t == BC_LEX_LBRACKET) {

			if (t == BC_TYPE_VAR) t = BC_TYPE_ARRAY;

			bc_lex_next(&p->l);

			if (BC_ERR(p->l.t != BC_LEX_RBRACKET))
				bc_parse_err(p, BC_ERR_PARSE_FUNC);

			bc_lex_next(&p->l);
		}
		else if (BC_ERR(t == BC_TYPE_REF))
			bc_parse_verr(p, BC_ERR_PARSE_REF_VAR, p->buf.v);

		comma = (p->l.t == BC_LEX_COMMA);
		if (comma) {
			bc_lex_next(&p->l);
		}

		bc_func_insert(p->func, p->prog, p->buf.v, t, p->l.line);
	}

	if (BC_ERR(comma)) bc_parse_err(p, BC_ERR_PARSE_FUNC);

	flags = BC_PARSE_FLAG_FUNC | BC_PARSE_FLAG_FUNC_INNER;
	bc_parse_startBody(p, flags);

	bc_lex_next(&p->l);

	if (p->l.t != BC_LEX_LBRACE) bc_parse_err(p, BC_ERR_POSIX_BRACE);
}

static void bc_parse_auto(BcParse *p) {

	bool comma, one;

	if (BC_ERR(!p->auto_part)) bc_parse_err(p, BC_ERR_PARSE_TOKEN);
	bc_lex_next(&p->l);

	p->auto_part = comma = false;
	one = p->l.t == BC_LEX_NAME;

	while (p->l.t == BC_LEX_NAME) {

		BcType t;

		bc_vec_string(&p->buf, p->l.str.len - 1, p->l.str.v);

		bc_lex_next(&p->l);

		if (p->l.t == BC_LEX_LBRACKET) {

			t = BC_TYPE_ARRAY;

			bc_lex_next(&p->l);

			if (BC_ERR(p->l.t != BC_LEX_RBRACKET))
				bc_parse_err(p, BC_ERR_PARSE_FUNC);

			bc_lex_next(&p->l);
		}
		else t = BC_TYPE_VAR;

		comma = (p->l.t == BC_LEX_COMMA);
		if (comma) bc_lex_next(&p->l);

		bc_func_insert(p->func, p->prog, p->buf.v, t, p->l.line);
	}

	if (BC_ERR(comma)) bc_parse_err(p, BC_ERR_PARSE_FUNC);
	if (BC_ERR(!one)) bc_parse_err(p, BC_ERR_PARSE_NO_AUTO);
	if (BC_ERR(!bc_parse_isDelimiter(p)))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);
}

static void bc_parse_body(BcParse *p, bool brace) {

	uint16_t *flag_ptr = BC_PARSE_TOP_FLAG_PTR(p);

	assert(flag_ptr != NULL);
	assert(p->flags.len >= 2);

	*flag_ptr &= ~(BC_PARSE_FLAG_BODY);

	if (*flag_ptr & BC_PARSE_FLAG_FUNC_INNER) {

		if (BC_ERR(!brace)) bc_parse_err(p, BC_ERR_PARSE_TOKEN);

		p->auto_part = (p->l.t != BC_LEX_KW_AUTO);

		if (!p->auto_part) {

			// Make sure this is true to not get a parse error.
			p->auto_part = true;

			bc_parse_auto(p);
		}

		if (p->l.t == BC_LEX_NLINE) bc_lex_next(&p->l);
	}
	else {

		size_t len = p->flags.len;

		assert(*flag_ptr);

		bc_parse_stmt(p);

		if (!brace && !BC_PARSE_BODY(p) && len <= p->flags.len)
			bc_parse_endBody(p, false);
	}
}

static void bc_parse_stmt(BcParse *p) {

	size_t len;
	uint16_t flags;
	BcLexType type = p->l.t;

	if (type == BC_LEX_NLINE) {
		bc_lex_next(&p->l);
		return;
	}
	if (type == BC_LEX_KW_AUTO) {
		bc_parse_auto(p);
		return;
	}

	p->auto_part = false;

	if (type != BC_LEX_KW_ELSE) {

		if (BC_PARSE_IF_END(p)) {
			bc_parse_noElse(p);
			if (p->flags.len > 1 && !BC_PARSE_BRACE(p))
				bc_parse_endBody(p, false);
			return;
		}
		else if (type == BC_LEX_LBRACE) {

			if (!BC_PARSE_BODY(p)) {
				bc_parse_startBody(p, BC_PARSE_FLAG_BRACE);
				bc_lex_next(&p->l);
			}
			else {
				*(BC_PARSE_TOP_FLAG_PTR(p)) |= BC_PARSE_FLAG_BRACE;
				bc_lex_next(&p->l);
				bc_parse_body(p, true);
			}

			return;
		}
		else if (BC_PARSE_BODY(p) && !BC_PARSE_BRACE(p)) {
			bc_parse_body(p, false);
			return;
		}
	}

	len = p->flags.len;
	flags = BC_PARSE_TOP_FLAG(p);

	switch (type) {

		case BC_LEX_OP_INC:
		case BC_LEX_OP_DEC:
		case BC_LEX_OP_MINUS:
		case BC_LEX_OP_BOOL_NOT:
		case BC_LEX_LPAREN:
		case BC_LEX_NAME:
		case BC_LEX_NUMBER:
		case BC_LEX_KW_IBASE:
		case BC_LEX_KW_LAST:
		case BC_LEX_KW_LENGTH:
		case BC_LEX_KW_OBASE:
		case BC_LEX_KW_SCALE:
#if BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
		case BC_LEX_KW_SEED:
#endif // BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
		case BC_LEX_KW_SQRT:
		case BC_LEX_KW_ABS:
#if BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
		case BC_LEX_KW_IRAND:
#endif // BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
		case BC_LEX_KW_READ:
#if BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
		case BC_LEX_KW_RAND:
#endif // BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
		case BC_LEX_KW_MAXIBASE:
		case BC_LEX_KW_MAXOBASE:
		case BC_LEX_KW_MAXSCALE:
#if BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
		case BC_LEX_KW_MAXRAND:
#endif // BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
		{
			bc_parse_expr_status(p, BC_PARSE_PRINT, bc_parse_next_expr);
			break;
		}

		case BC_LEX_KW_ELSE:
		{
			bc_parse_else(p);
			break;
		}

		case BC_LEX_SCOLON:
		{
			// Do nothing.
			break;
		}

		case BC_LEX_RBRACE:
		{
			bc_parse_endBody(p, true);
			break;
		}

		case BC_LEX_STR:
		{
			bc_parse_str(p, BC_INST_PRINT_STR);
			break;
		}

		case BC_LEX_KW_BREAK:
		case BC_LEX_KW_CONTINUE:
		{
			bc_parse_loopExit(p, p->l.t);
			break;
		}

		case BC_LEX_KW_FOR:
		{
			bc_parse_for(p);
			break;
		}

		case BC_LEX_KW_HALT:
		{
			bc_parse_push(p, BC_INST_HALT);
			bc_lex_next(&p->l);
			break;
		}

		case BC_LEX_KW_IF:
		{
			bc_parse_if(p);
			break;
		}

		case BC_LEX_KW_LIMITS:
		{
			bc_vm_printf("BC_LONG_BIT      = %lu\n", (ulong) BC_LONG_BIT);
			bc_vm_printf("BC_BASE_DIGS     = %lu\n", (ulong) BC_BASE_DIGS);
			bc_vm_printf("BC_BASE_POW      = %lu\n", (ulong) BC_BASE_POW);
			bc_vm_printf("BC_OVERFLOW_MAX  = %lu\n", (ulong) BC_NUM_BIGDIG_MAX);
			bc_vm_printf("\n");
			bc_vm_printf("BC_BASE_MAX      = %lu\n", BC_MAX_OBASE);
			bc_vm_printf("BC_DIM_MAX       = %lu\n", BC_MAX_DIM);
			bc_vm_printf("BC_SCALE_MAX     = %lu\n", BC_MAX_SCALE);
			bc_vm_printf("BC_STRING_MAX    = %lu\n", BC_MAX_STRING);
			bc_vm_printf("BC_NAME_MAX      = %lu\n", BC_MAX_NAME);
			bc_vm_printf("BC_NUM_MAX       = %lu\n", BC_MAX_NUM);
#if BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
			bc_vm_printf("BC_RAND_MAX      = %lu\n", BC_MAX_RAND);
#endif // BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
			bc_vm_printf("MAX Exponent     = %lu\n", BC_MAX_EXP);
			bc_vm_printf("Number of vars   = %lu\n", BC_MAX_VARS);

			bc_lex_next(&p->l);

			break;
		}

		case BC_LEX_KW_PRINT:
		{
			bc_parse_print(p);
			break;
		}

		case BC_LEX_KW_QUIT:
		{
			// Quit is a compile-time command. We don't exit directly,
			// so the vm can clean up. Limits do the same thing.
			vm.status = BC_STATUS_QUIT;
			BC_VM_JMP;
			break;
		}

		case BC_LEX_KW_RETURN:
		{
			bc_parse_return(p);
			break;
		}

		case BC_LEX_KW_WHILE:
		{
			bc_parse_while(p);
			break;
		}

		default:
		{
			bc_parse_err(p, BC_ERR_PARSE_TOKEN);
		}
	}

	if (len == p->flags.len && flags == BC_PARSE_TOP_FLAG(p)) {
		if (BC_ERR(!bc_parse_isDelimiter(p)))
			bc_parse_err(p, BC_ERR_PARSE_TOKEN);
	}

	// Make sure semicolons are eaten.
	while (p->l.t == BC_LEX_SCOLON) bc_lex_next(&p->l);
}

void bc_parse_parse(BcParse *p) {

	assert(p);

	BC_SETJMP(exit);

	if (BC_ERR(p->l.t == BC_LEX_EOF)) bc_parse_err(p, BC_ERR_PARSE_EOF);
	else if (p->l.t == BC_LEX_KW_DEFINE) {
		if (BC_ERR(BC_PARSE_NO_EXEC(p)))
			bc_parse_err(p, BC_ERR_PARSE_TOKEN);
		bc_parse_func(p);
	}
	else bc_parse_stmt(p);

exit:
	BC_SIG_MAYLOCK;
	if (BC_ERR(((vm.status && vm.status != BC_STATUS_QUIT) || vm.sig)))
		bc_parse_reset(p);
	BC_LONGJMP_CONT;
}

static BcParseStatus bc_parse_expr_err(BcParse *p, uint8_t flags,
                                       BcParseNext next)
{
	BcInst prev = BC_INST_PRINT;
	uchar inst = BC_INST_INVALID;
	BcLexType top, t = p->l.t;
	size_t nexprs = 0, ops_bgn = p->ops.len;
	uint32_t i, nparens, nrelops;
	bool pfirst, rprn, done, get_token, assign, bin_last, incdec, can_assign;

	assert(!(flags & BC_PARSE_PRINT) || !(flags & BC_PARSE_NEEDVAL));

	pfirst = (p->l.t == BC_LEX_LPAREN);
	nparens = nrelops = 0;
	rprn = done = get_token = assign = incdec = can_assign = false;
	bin_last = true;

	// We want to eat newlines if newlines are not a valid ending token.
	// This is for spacing in things like for loop headers.
	if (!(flags & BC_PARSE_NOREAD)) {
		while ((t = p->l.t) == BC_LEX_NLINE) bc_lex_next(&p->l);
	}

	for (; !done && BC_PARSE_EXPR(t); t = p->l.t)
	{
		switch (t) {

			case BC_LEX_OP_INC:
			case BC_LEX_OP_DEC:
			{
				if (BC_ERR(incdec)) bc_parse_err(p, BC_ERR_PARSE_ASSIGN);
				bc_parse_incdec(p, &prev, &can_assign, &nexprs, flags);
				rprn = get_token = bin_last = false;
				incdec = true;
				flags &= ~(BC_PARSE_ARRAY);
				break;
			}

#if BC_ENABLE_EXTRA_MATH
			case BC_LEX_OP_TRUNC:
			{
				if (BC_ERR(!BC_PARSE_LEAF(prev, bin_last, rprn)))
					bc_parse_err(p, BC_ERR_PARSE_TOKEN);

				// I can just add the instruction because
				// negative will already be taken care of.
				bc_parse_push(p, BC_INST_TRUNC);
				rprn = can_assign = incdec = false;
				get_token = true;
				flags &= ~(BC_PARSE_ARRAY);
				break;
			}
#endif // BC_ENABLE_EXTRA_MATH

			case BC_LEX_OP_MINUS:
			{
				bc_parse_minus(p, &prev, ops_bgn, rprn, bin_last, &nexprs);
				rprn = get_token = can_assign = false;
				bin_last = (prev == BC_INST_MINUS);
				if (bin_last) incdec = false;
				flags &= ~(BC_PARSE_ARRAY);
				break;
			}

			case BC_LEX_OP_ASSIGN_POWER:
			case BC_LEX_OP_ASSIGN_MULTIPLY:
			case BC_LEX_OP_ASSIGN_DIVIDE:
			case BC_LEX_OP_ASSIGN_MODULUS:
			case BC_LEX_OP_ASSIGN_PLUS:
			case BC_LEX_OP_ASSIGN_MINUS:
#if BC_ENABLE_EXTRA_MATH
			case BC_LEX_OP_ASSIGN_PLACES:
			case BC_LEX_OP_ASSIGN_LSHIFT:
			case BC_LEX_OP_ASSIGN_RSHIFT:
#endif // BC_ENABLE_EXTRA_MATH
			case BC_LEX_OP_ASSIGN:
			{
				if (!BC_PARSE_INST_VAR(prev))
					bc_parse_err(p, BC_ERR_PARSE_ASSIGN);
			}
			// Fallthrough.
			BC_FALLTHROUGH

			case BC_LEX_OP_POWER:
			case BC_LEX_OP_MULTIPLY:
			case BC_LEX_OP_DIVIDE:
			case BC_LEX_OP_MODULUS:
			case BC_LEX_OP_PLUS:
#if BC_ENABLE_EXTRA_MATH
			case BC_LEX_OP_PLACES:
			case BC_LEX_OP_LSHIFT:
			case BC_LEX_OP_RSHIFT:
#endif // BC_ENABLE_EXTRA_MATH
			case BC_LEX_OP_REL_EQ:
			case BC_LEX_OP_REL_LE:
			case BC_LEX_OP_REL_GE:
			case BC_LEX_OP_REL_NE:
			case BC_LEX_OP_REL_LT:
			case BC_LEX_OP_REL_GT:
			case BC_LEX_OP_BOOL_NOT:
			case BC_LEX_OP_BOOL_OR:
			case BC_LEX_OP_BOOL_AND:
			{
				if (BC_PARSE_OP_PREFIX(t)) {
					if (BC_ERR(!bin_last && !BC_PARSE_OP_PREFIX(p->l.last)))
						bc_parse_err(p, BC_ERR_PARSE_EXPR);
				}
				else if (BC_ERR(BC_PARSE_PREV_PREFIX(prev) || bin_last))
					bc_parse_err(p, BC_ERR_PARSE_EXPR);

				nrelops += (t >= BC_LEX_OP_REL_EQ && t <= BC_LEX_OP_REL_GT);
				prev = BC_PARSE_TOKEN_INST(t);
				bc_parse_operator(p, t, ops_bgn, &nexprs);
				rprn = incdec = can_assign = false;
				get_token = true;
				bin_last = !BC_PARSE_OP_PREFIX(t);
				flags &= ~(BC_PARSE_ARRAY);

				break;
			}

			case BC_LEX_LPAREN:
			{
				if (BC_ERR(BC_PARSE_LEAF(prev, bin_last, rprn)))
					bc_parse_err(p, BC_ERR_PARSE_EXPR);

				nparens += 1;
				rprn = incdec = can_assign = false;
				get_token = true;
				bc_vec_push(&p->ops, &t);

				break;
			}

			case BC_LEX_RPAREN:
			{
				// This needs to be a status. The error
				// is handled in bc_parse_expr_status().
				if (BC_ERR(p->l.last == BC_LEX_LPAREN))
					return BC_PARSE_STATUS_EMPTY_EXPR;

				if (BC_ERR(bin_last || BC_PARSE_PREV_PREFIX(prev)))
					bc_parse_err(p, BC_ERR_PARSE_EXPR);

				if (!nparens) {
					done = true;
					get_token = false;
					break;
				}

				nparens -= 1;
				rprn = true;
				get_token = bin_last = incdec = false;

				bc_parse_rightParen(p, &nexprs);

				break;
			}

			case BC_LEX_NAME:
			{
				if (BC_ERR(BC_PARSE_LEAF(prev, bin_last, rprn)))
					bc_parse_err(p, BC_ERR_PARSE_EXPR);

				get_token = bin_last = false;
				bc_parse_name(p, &prev, &can_assign,
				                  flags & ~BC_PARSE_NOCALL);
				rprn = (prev == BC_INST_CALL);
				nexprs += 1;
				flags &= ~(BC_PARSE_ARRAY);

				break;
			}

			case BC_LEX_NUMBER:
			{
				if (BC_ERR(BC_PARSE_LEAF(prev, bin_last, rprn)))
					bc_parse_err(p, BC_ERR_PARSE_EXPR);

				bc_parse_number(p);
				nexprs += 1;
				prev = BC_INST_NUM;
				get_token = true;
				rprn = bin_last = can_assign = false;
				flags &= ~(BC_PARSE_ARRAY);

				break;
			}

			case BC_LEX_KW_IBASE:
			case BC_LEX_KW_LAST:
			case BC_LEX_KW_OBASE:
#if BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
			case BC_LEX_KW_SEED:
#endif // BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
			{
				if (BC_ERR(BC_PARSE_LEAF(prev, bin_last, rprn)))
					bc_parse_err(p, BC_ERR_PARSE_EXPR);

				prev = t - BC_LEX_KW_LAST + BC_INST_LAST;
				bc_parse_push(p, prev);

				get_token = can_assign = true;
				rprn = bin_last = false;
				nexprs += 1;
				flags &= ~(BC_PARSE_ARRAY);

				break;
			}

			case BC_LEX_KW_LENGTH:
			case BC_LEX_KW_SQRT:
			case BC_LEX_KW_ABS:
#if BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
			case BC_LEX_KW_IRAND:
#endif // BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
			{
				if (BC_ERR(BC_PARSE_LEAF(prev, bin_last, rprn)))
					bc_parse_err(p, BC_ERR_PARSE_EXPR);

				bc_parse_builtin(p, t, flags, &prev);
				rprn = get_token = bin_last = incdec = can_assign = false;
				nexprs += 1;
				flags &= ~(BC_PARSE_ARRAY);

				break;
			}

			case BC_LEX_KW_READ:
#if BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
			case BC_LEX_KW_RAND:
#endif // BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
			case BC_LEX_KW_MAXIBASE:
			case BC_LEX_KW_MAXOBASE:
			case BC_LEX_KW_MAXSCALE:
#if BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
			case BC_LEX_KW_MAXRAND:
#endif // BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
			{
				if (BC_ERR(BC_PARSE_LEAF(prev, bin_last, rprn)))
					bc_parse_err(p, BC_ERR_PARSE_EXPR);
				else if (t == BC_LEX_KW_READ && BC_ERR(flags & BC_PARSE_NOREAD))
					bc_parse_err(p, BC_ERR_EXEC_REC_READ);
				else {
					prev = t - BC_LEX_KW_READ + BC_INST_READ;
					bc_parse_noArgBuiltin(p, prev);
				}

				rprn = get_token = bin_last = incdec = can_assign = false;
				nexprs += 1;
				flags &= ~(BC_PARSE_ARRAY);

				break;
			}

			case BC_LEX_KW_SCALE:
			{
				if (BC_ERR(BC_PARSE_LEAF(prev, bin_last, rprn)))
					bc_parse_err(p, BC_ERR_PARSE_EXPR);

				bc_parse_scale(p, &prev, &can_assign, flags);
				rprn = get_token = bin_last = false;
				nexprs += 1;
				flags &= ~(BC_PARSE_ARRAY);

				break;
			}

			default:
			{
#ifndef NDEBUG
				bc_parse_err(p, BC_ERR_PARSE_TOKEN);
				break;
#endif // NDEBUG
			}
		}

		if (get_token) bc_lex_next(&p->l);
	}

	while (p->ops.len > ops_bgn) {

		top = BC_PARSE_TOP_OP(p);
		assign = top >= BC_LEX_OP_ASSIGN_POWER && top <= BC_LEX_OP_ASSIGN;

		if (BC_ERR(top == BC_LEX_LPAREN || top == BC_LEX_RPAREN))
			bc_parse_err(p, BC_ERR_PARSE_EXPR);

		bc_parse_push(p, BC_PARSE_TOKEN_INST(top));

		nexprs -= !BC_PARSE_OP_PREFIX(top);
		bc_vec_pop(&p->ops);

		incdec = false;
	}

	if (BC_ERR(nexprs != 1)) bc_parse_err(p, BC_ERR_PARSE_EXPR);

	for (i = 0; i < next.len && t != next.tokens[i]; ++i);
	if (BC_ERR(i == next.len && !bc_parse_isDelimiter(p)))
		bc_parse_err(p, BC_ERR_PARSE_EXPR);

	if (!(flags & BC_PARSE_REL) && nrelops)
		bc_parse_err(p, BC_ERR_POSIX_REL_POS);
	else if ((flags & BC_PARSE_REL) && nrelops > 1)
		bc_parse_err(p, BC_ERR_POSIX_MULTIREL);

	if (!(flags & BC_PARSE_NEEDVAL) && !pfirst) {

		if (assign) {
			inst = *((uchar*) bc_vec_top(&p->func->code));
			inst += (BC_INST_ASSIGN_POWER_NO_VAL - BC_INST_ASSIGN_POWER);
			incdec = false;
		}
		else if (incdec && !(flags & BC_PARSE_PRINT)) {
			inst = *((uchar*) bc_vec_top(&p->func->code));
			incdec = (inst <= BC_INST_DEC);
			inst = BC_INST_ASSIGN_PLUS_NO_VAL + (inst != BC_INST_INC &&
			                                     inst != BC_INST_ASSIGN_PLUS);
		}

		if (inst >= BC_INST_ASSIGN_POWER_NO_VAL &&
		    inst <= BC_INST_ASSIGN_NO_VAL)
		{
			bc_vec_pop(&p->func->code);
			if (incdec) bc_parse_push(p, BC_INST_ONE);
			bc_parse_push(p, inst);
		}
	}

	if ((flags & BC_PARSE_PRINT)) {
		if (pfirst || !assign) bc_parse_push(p, BC_INST_PRINT);
	}
	else if (!(flags & BC_PARSE_NEEDVAL) &&
	         (inst < BC_INST_ASSIGN_POWER_NO_VAL ||
	          inst > BC_INST_ASSIGN_NO_VAL))
	{
		bc_parse_push(p, BC_INST_POP);
	}

	// We want to eat newlines if newlines are not a valid ending token.
	// This is for spacing in things like for loop headers.
	for (incdec = true, i = 0; i < next.len && incdec; ++i)
		incdec = (next.tokens[i] != BC_LEX_NLINE);
	if (incdec) {
		while (p->l.t == BC_LEX_NLINE) bc_lex_next(&p->l);
	}

	return BC_PARSE_STATUS_SUCCESS;
}

void bc_parse_expr_status(BcParse *p, uint8_t flags, BcParseNext next) {

	BcParseStatus s = bc_parse_expr_err(p, flags, next);

	if (BC_ERR(s == BC_PARSE_STATUS_EMPTY_EXPR))
		bc_parse_err(p, BC_ERR_PARSE_EMPTY_EXPR);
}

void bc_parse_expr(BcParse *p, uint8_t flags) {
	assert(p);
	bc_parse_expr_status(p, flags, bc_parse_next_read);
}
#endif // BC_ENABLED
