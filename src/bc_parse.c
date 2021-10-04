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

// Before you embark on trying to understand this code, have you read the
// Development manual (manuals/development.md) and the comment in include/bc.h
// yet? No? Do that first. I'm serious.
//
// The reason is because this file holds the most sensitive and finicky code in
// the entire codebase. Even getting history to work on Windows was nothing
// compared to this. This is where dreams go to die, where dragons live, and
// from which Ken Thompson himself would flee.

static void bc_parse_else(BcParse *p);
static void bc_parse_stmt(BcParse *p);
static BcParseStatus bc_parse_expr_err(BcParse *p, uint8_t flags,
                                       BcParseNext next);
static void bc_parse_expr_status(BcParse *p, uint8_t flags, BcParseNext next);

/**
 * Returns true if an instruction could only have come from a "leaf" expression.
 * For more on what leaf expressions are, read the comment for BC_PARSE_LEAF().
 * @param t  The instruction to test.
 */
static bool bc_parse_inst_isLeaf(BcInst t) {
	return (t >= BC_INST_NUM && t <= BC_INST_MAXSCALE) ||
#if BC_ENABLE_EXTRA_MATH
	        t == BC_INST_TRUNC ||
#endif // BC_ENABLE_EXTRA_MATH
	        t <= BC_INST_DEC;
}

/**
 * Returns true if the *previous* token was a delimiter. A delimiter is anything
 * that can legally end a statement. In bc's case, it could be a newline, a
 * semicolon, and a brace in certain cases.
 * @param p  The parser.
 */
static bool bc_parse_isDelimiter(const BcParse *p) {

	BcLexType t = p->l.t;
	bool good;

	// If it's an obvious delimiter, say so.
	if (BC_PARSE_DELIMITER(t)) return true;

	good = false;

	// If the current token is a keyword, then...beware. That means that we need
	// to check for a "dangling" else, where there was no brace-delimited block
	// on the previous if.
	if (t == BC_LEX_KW_ELSE) {

		size_t i;
		uint16_t *fptr = NULL, flags = BC_PARSE_FLAG_ELSE;

		// As long as going up the stack is valid for a dangling else, keep on.
		for (i = 0; i < p->flags.len && BC_PARSE_BLOCK_STMT(flags); ++i) {

			fptr = bc_vec_item_rev(&p->flags, i);
			flags = *fptr;

			// If we need a brace and don't have one, then we don't have a
			// delimiter.
			if ((flags & BC_PARSE_FLAG_BRACE) && p->l.last != BC_LEX_RBRACE)
				return false;
		}

		// Oh, and we had also better have an if statement somewhere.
		good = ((flags & BC_PARSE_FLAG_IF) != 0);
	}
	else if (t == BC_LEX_RBRACE) {

		size_t i;

		// Since we have a brace, we need to just check if a brace was needed.
		for (i = 0; !good && i < p->flags.len; ++i) {
			uint16_t *fptr = bc_vec_item_rev(&p->flags, i);
			good = (((*fptr) & BC_PARSE_FLAG_BRACE) != 0);
		}
	}

	return good;
}

/**
 * Sets a previously defined exit label. What are labels? See the bc Parsing
 * section of the Development manual (manuals/development.md).
 * @param p  The parser.
 */
static void bc_parse_setLabel(BcParse *p) {

	BcFunc *func = p->func;
	BcInstPtr *ip = bc_vec_top(&p->exits);
	size_t *label;

	assert(func == bc_vec_item(&p->prog->fns, p->fidx));

	// Set the preallocated label to the correct index.
	label = bc_vec_item(&func->labels, ip->idx);
	*label = func->code.len;

	// Now, we don't need the exit label; it is done.
	bc_vec_pop(&p->exits);
}

/**
 * Creates a label and sets it to idx. If this is an exit label, then idx is
 * actually invalid, but it doesn't matter because it will be fixed by
 * bc_parse_setLabel() later.
 * @param p    The parser.
 * @param idx  The index of the label.
 */
static void bc_parse_createLabel(BcParse *p, size_t idx) {
	bc_vec_push(&p->func->labels, &idx);
}

/**
 * Creates a conditional label. Unlike an exit label, this label is set at
 * creation time because it comes *before* the code that will target it.
 * @param p    The parser.
 * @param idx  The index of the label.
 */
static void bc_parse_createCondLabel(BcParse *p, size_t idx) {
	bc_parse_createLabel(p, p->func->code.len);
	bc_vec_push(&p->conds, &idx);
}

/*
 * Creates an exit label to be filled in later by bc_parse_setLabel(). Also, why
 * create a label to be filled in later? Because exit labels are meant to be
 * targeted by code that comes *before* the label. Since we have to parse that
 * code first, and don't know how long it will be, we need to just make sure to
 * reserve a slot to be filled in later when we know.
 *
 * By the way, this uses BcInstPtr because it was convenient. The field idx
 * holds the index, and the field func holds the loop boolean.
 *
 * @param p     The parser.
 * @param idx   The index of the label's position.
 * @param loop  True if the exit label is for a loop or not.
 */
static void bc_parse_createExitLabel(BcParse *p, size_t idx, bool loop) {

	BcInstPtr ip;

	assert(p->func == bc_vec_item(&p->prog->fns, p->fidx));

	ip.func = loop;
	ip.idx = idx;
	ip.len = 0;

	bc_vec_push(&p->exits, &ip);
	bc_parse_createLabel(p, SIZE_MAX);
}

/**
 * Pops the correct operators off of the operator stack based on the current
 * operator. This is because of the Shunting-Yard algorithm. Lower prec means
 * higher precedence.
 * @param p       The parser.
 * @param type    The operator.
 * @param start   The previous start of the operator stack. For more
 *                information, see the bc Parsing section of the Development
 *                manual (manuals/development.md).
 * @param nexprs  A pointer to the current number of expressions that have not
 *                been consumed yet. This is an IN and OUT parameter.
 */
static void bc_parse_operator(BcParse *p, BcLexType type,
                              size_t start, size_t *nexprs)
{
	BcLexType t;
	uchar l, r = BC_PARSE_OP_PREC(type);
	uchar left = BC_PARSE_OP_LEFT(type);

	// While we haven't hit the stop point yet.
	while (p->ops.len > start) {

		// Get the top operator.
		t = BC_PARSE_TOP_OP(p);

		// If it's a right paren, we have reached the end of whatever expression
		// this is no matter what.
		if (t == BC_LEX_LPAREN) break;

		// Break for precedence. Precedence operates differently on left and
		// right associativity, by the way. A left associative operator that
		// matches the current precedence should take priority, but a right
		// associative operator should not.
		l = BC_PARSE_OP_PREC(t);
		if (l >= r && (l != r || !left)) break;

		// Do the housekeeping. In particular, make sure to note that one
		// expression was consumed. (Two were, but another was added.)
		bc_parse_push(p, BC_PARSE_TOKEN_INST(t));
		bc_vec_pop(&p->ops);
		*nexprs -= !BC_PARSE_OP_PREFIX(t);
	}

	bc_vec_push(&p->ops, &type);
}

/**
 * Parses a right paren. In the Shunting-Yard algorithm, it needs to be put on
 * the operator stack. But before that, it needs to consume whatever operators
 * there are until it hits a left paren.
 * @param p       The parser.
 * @param nexprs  A pointer to the current number of expressions that have not
 *                been consumed yet. This is an IN and OUT parameter.
 */
static void bc_parse_rightParen(BcParse *p, size_t *nexprs) {

	BcLexType top;

	// Consume operators until a left paren.
	while ((top = BC_PARSE_TOP_OP(p)) != BC_LEX_LPAREN) {
		bc_parse_push(p, BC_PARSE_TOKEN_INST(top));
		bc_vec_pop(&p->ops);
		*nexprs -= !BC_PARSE_OP_PREFIX(top);
	}

	// We need to pop the left paren as well.
	bc_vec_pop(&p->ops);

	// Oh, and we also want the next token.
	bc_lex_next(&p->l);
}

/**
 * Parses function arguments.
 * @param p      The parser.
 * @param flags  Flags restricting what kind of expressions the arguments can
 *               be.
 */
static void bc_parse_args(BcParse *p, uint8_t flags) {

	bool comma = false;
	size_t nargs;

	bc_lex_next(&p->l);

	// Print and comparison operators not allowed. Well, comparison operators
	// only for POSIX. But we do allow arrays, and we *must* get a value.
	flags &= ~(BC_PARSE_PRINT | BC_PARSE_REL);
	flags |= (BC_PARSE_ARRAY | BC_PARSE_NEEDVAL);

	// Count the arguments and parse them.
	for (nargs = 0; p->l.t != BC_LEX_RPAREN; ++nargs) {

		bc_parse_expr_status(p, flags, bc_parse_next_arg);

		comma = (p->l.t == BC_LEX_COMMA);
		if (comma) bc_lex_next(&p->l);
	}

	// An ending comma is FAIL.
	if (BC_ERR(comma)) bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	// Now do the call with the number of arguments.
	bc_parse_push(p, BC_INST_CALL);
	bc_parse_pushIndex(p, nargs);
}

/**
 * Parses a function call.
 * @param p      The parser.
 * @param flags  Flags restricting what kind of expressions the arguments can
 *               be.
 */
static void bc_parse_call(BcParse *p, const char *name, uint8_t flags) {

	size_t idx;

	bc_parse_args(p, flags);

	// We just assert this because bc_parse_args() should
	// ensure that the next token is what it should be.
	assert(p->l.t == BC_LEX_RPAREN);

	// We cannot use bc_program_insertFunc() here
	// because it will overwrite an existing function.
	idx = bc_map_index(&p->prog->fn_map, name);

	// The function does not exist yet. Create a space for it. If the user does
	// not define it, it's a *runtime* error, not a parse error.
	if (idx == BC_VEC_INVALID_IDX) {

		BC_SIG_LOCK;

		idx = bc_program_insertFunc(p->prog, name);

		BC_SIG_UNLOCK;

		assert(idx != BC_VEC_INVALID_IDX);

		// Make sure that this pointer was not invalidated.
		p->func = bc_vec_item(&p->prog->fns, p->fidx);
	}
	// The function exists, so set the right function index.
	else idx = ((BcId*) bc_vec_item(&p->prog->fn_map, idx))->idx;

	bc_parse_pushIndex(p, idx);

	// Make sure to get the next token.
	bc_lex_next(&p->l);
}

/**
 * Parses a name/identifier-based expression. It could be a variable, an array
 * element, an array itself (for function arguments), a function call, etc.
 *
 */
static void bc_parse_name(BcParse *p, BcInst *type,
                          bool *can_assign, uint8_t flags)
{
	char *name;

	BC_SIG_LOCK;

	// We want a copy of the name since the lexer might overwrite its copy.
	name = bc_vm_strdup(p->l.str.v);

	BC_SETJMP_LOCKED(err);

	BC_SIG_UNLOCK;

	// We need the next token to see if it's just a variable or something more.
	bc_lex_next(&p->l);

	// Array element or array.
	if (p->l.t == BC_LEX_LBRACKET) {

		bc_lex_next(&p->l);

		// Array only. This has to be a function parameter.
		if (p->l.t == BC_LEX_RBRACKET) {

			// Error if arrays are not allowed.
			if (BC_ERR(!(flags & BC_PARSE_ARRAY)))
				bc_parse_err(p, BC_ERR_PARSE_EXPR);

			*type = BC_INST_ARRAY;
			*can_assign = false;
		}
		else {

			// If we are here, we have an array element. We need to set the
			// expression parsing flags.
			uint8_t flags2 = (flags & ~(BC_PARSE_PRINT | BC_PARSE_REL)) |
			                 BC_PARSE_NEEDVAL;

			bc_parse_expr_status(p, flags2, bc_parse_next_elem);

			// The next token *must* be a right bracket.
			if (BC_ERR(p->l.t != BC_LEX_RBRACKET))
				bc_parse_err(p, BC_ERR_PARSE_TOKEN);

			*type = BC_INST_ARRAY_ELEM;
			*can_assign = true;
		}

		// Make sure to get the next token.
		bc_lex_next(&p->l);

		// Push the instruction and the name of the identifier.
		bc_parse_push(p, *type);
		bc_parse_pushName(p, name, false);
	}
	else if (p->l.t == BC_LEX_LPAREN) {

		// We are parsing a function call; error if not allowed.
		if (BC_ERR(flags & BC_PARSE_NOCALL))
			bc_parse_err(p, BC_ERR_PARSE_TOKEN);

		*type = BC_INST_CALL;
		*can_assign = false;

		bc_parse_call(p, name, flags);
	}
	else {
		// Just a variable.
		*type = BC_INST_VAR;
		*can_assign = true;
		bc_parse_push(p, BC_INST_VAR);
		bc_parse_pushName(p, name, true);
	}

err:
	// Need to make sure to unallocate the name.
	BC_SIG_MAYLOCK;
	free(name);
	BC_LONGJMP_CONT;
}

/**
 * Parses a builtin function that takes no arguments. This includes read(),
 * rand(), maxibase(), maxobase(), maxscale(), and maxrand().
 * @param p     The parser.
 * @param inst  The instruction corresponding to the builtin.
 */
static void bc_parse_noArgBuiltin(BcParse *p, BcInst inst) {

	// Must have a left paren.
	bc_lex_next(&p->l);
	if (BC_ERR(p->l.t != BC_LEX_LPAREN)) bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	// Must have a right paren.
	bc_lex_next(&p->l);
	if ((p->l.t != BC_LEX_RPAREN)) bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	bc_parse_push(p, inst);

	bc_lex_next(&p->l);
}

/**
 * Parses a builtin function that takes 1 argument. This includes length(),
 * sqrt(), abs(), scale(), and irand().
 * @param p      The parser.
 * @param type   The lex token.
 * @param flags  The expression parsing flags for parsing the argument.
 * @param prev   An out parameter; the previous instruction pointer.
 */
static void bc_parse_builtin(BcParse *p, BcLexType type,
                             uint8_t flags, BcInst *prev)
{
	// Must have a left paren.
	bc_lex_next(&p->l);
	if (BC_ERR(p->l.t != BC_LEX_LPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	bc_lex_next(&p->l);

	// Change the flags as needed for parsing the argument.
	flags &= ~(BC_PARSE_PRINT | BC_PARSE_REL);
	flags |= BC_PARSE_NEEDVAL;

	// Since length can take arrays, we need to specially add that flag.
	if (type == BC_LEX_KW_LENGTH) flags |= BC_PARSE_ARRAY;

	bc_parse_expr_status(p, flags, bc_parse_next_rel);

	// Must have a right paren.
	if (BC_ERR(p->l.t != BC_LEX_RPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	// Adjust previous based on the token and push it.
	*prev = type - BC_LEX_KW_LENGTH + BC_INST_LENGTH;
	bc_parse_push(p, *prev);

	bc_lex_next(&p->l);
}

/**
 * Parses a builtin function that takes 3 arguments. This includes modexp() and
 * divmod().
 */
static void bc_parse_builtin3(BcParse *p, BcLexType type,
                              uint8_t flags, BcInst *prev)
{
	assert(type == BC_LEX_KW_MODEXP || type == BC_LEX_KW_DIVMOD);

	// Must have a left paren.
	bc_lex_next(&p->l);
	if (BC_ERR(p->l.t != BC_LEX_LPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	bc_lex_next(&p->l);

	// Change the flags as needed for parsing the argument.
	flags &= ~(BC_PARSE_PRINT | BC_PARSE_REL);
	flags |= BC_PARSE_NEEDVAL;

	bc_parse_expr_status(p, flags, bc_parse_next_builtin);

	// Must have a comma.
	if (BC_ERR(p->l.t != BC_LEX_COMMA))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	bc_lex_next(&p->l);

	bc_parse_expr_status(p, flags, bc_parse_next_builtin);

	// Must have a comma.
	if (BC_ERR(p->l.t != BC_LEX_COMMA))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	bc_lex_next(&p->l);

	// If it is a divmod, parse an array name. Otherwise, just parse another
	// expression.
	if (type == BC_LEX_KW_DIVMOD) {

		// Must have a name.
		if (BC_ERR(p->l.t != BC_LEX_NAME)) bc_parse_err(p, BC_ERR_PARSE_TOKEN);

		// This is safe because the next token should not overwrite the name.
		bc_lex_next(&p->l);

		// Must have a left bracket.
		if (BC_ERR(p->l.t != BC_LEX_LBRACKET))
			bc_parse_err(p, BC_ERR_PARSE_TOKEN);

		// This is safe because the next token should not overwrite the name.
		bc_lex_next(&p->l);

		// Must have a right bracket.
		if (BC_ERR(p->l.t != BC_LEX_RBRACKET))
			bc_parse_err(p, BC_ERR_PARSE_TOKEN);

		// This is safe because the next token should not overwrite the name.
		bc_lex_next(&p->l);
	}
	else bc_parse_expr_status(p, flags, bc_parse_next_rel);

	// Must have a right paren.
	if (BC_ERR(p->l.t != BC_LEX_RPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	// Adjust previous based on the token and push it.
	*prev = type - BC_LEX_KW_MODEXP + BC_INST_MODEXP;
	bc_parse_push(p, *prev);

	// If we have divmod, we need to assign the modulus to the array element, so
	// we need to push the instructions for doing so.
	if (type == BC_LEX_KW_DIVMOD) {

		// The zeroth element.
		bc_parse_push(p, BC_INST_ZERO);
		bc_parse_push(p, BC_INST_ARRAY_ELEM);

		// Push the array.
		bc_parse_pushName(p, p->l.str.v, false);

		// Swap them and assign. After this, the top item on the stack should
		// be the quotient.
		bc_parse_push(p, BC_INST_SWAP);
		bc_parse_push(p, BC_INST_ASSIGN_NO_VAL);
	}

	bc_lex_next(&p->l);
}

/**
 * Parses the scale keyword. This is special because scale can be a value or a
 * builtin function.
 * @param p           The parser.
 * @param type        An out parameter; the instruction for the parse.
 * @param can_assign  An out parameter; whether the expression can be assigned
 *                    to.
 * @param flags       The expression parsing flags for parsing a scale() arg.
 */
static void bc_parse_scale(BcParse *p, BcInst *type,
                           bool *can_assign, uint8_t flags)
{
	bc_lex_next(&p->l);

	// Without the left paren, it's just the keyword.
	if (p->l.t != BC_LEX_LPAREN) {

		// Set, push, and return.
		*type = BC_INST_SCALE;
		*can_assign = true;
		bc_parse_push(p, BC_INST_SCALE);
		return;
	}

	// Handle the scale function.
	*type = BC_INST_SCALE_FUNC;
	*can_assign = false;

	// Once again, adjust the flags.
	flags &= ~(BC_PARSE_PRINT | BC_PARSE_REL);
	flags |= BC_PARSE_NEEDVAL;

	bc_lex_next(&p->l);

	bc_parse_expr_status(p, flags, bc_parse_next_rel);

	// Must have a right paren.
	if (BC_ERR(p->l.t != BC_LEX_RPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	bc_parse_push(p, BC_INST_SCALE_FUNC);

	bc_lex_next(&p->l);
}

/**
 * Parses and increment or decrement operator. This is a bit complex.
 * @param p           The parser.
 * @param prev        An out parameter; the previous instruction pointer.
 * @param can_assign  An out parameter; whether the expression can be assigned
 *                    to.
 * @param nexs        An in/out parameter; the number of expressions in the
 *                    parse tree that are not used.
 * @param flags       The expression parsing flags for parsing a scale() arg.
 */
static void bc_parse_incdec(BcParse *p, BcInst *prev, bool *can_assign,
                            size_t *nexs, uint8_t flags)
{
	BcLexType type;
	uchar inst;
	BcInst etype = *prev;
	BcLexType last = p->l.last;

	assert(prev != NULL && can_assign != NULL);

	// If we can't assign to the previous token, then we have an error.
	if (BC_ERR(last == BC_LEX_OP_INC || last == BC_LEX_OP_DEC ||
	           last == BC_LEX_RPAREN))
	{
		bc_parse_err(p, BC_ERR_PARSE_ASSIGN);
	}

	// Is the previous instruction for a variable?
	if (BC_PARSE_INST_VAR(etype)) {

		// If so, this is a postfix operator.
		if (!*can_assign) bc_parse_err(p, BC_ERR_PARSE_ASSIGN);

		// Only postfix uses BC_INST_INC and BC_INST_DEC.
		*prev = inst = BC_INST_INC + (p->l.t != BC_LEX_OP_INC);
		bc_parse_push(p, inst);
		bc_lex_next(&p->l);
		*can_assign = false;
	}
	else {

		// This is a prefix operator. In that case, we just convert it to
		// an assignment instruction.
		*prev = inst = BC_INST_ASSIGN_PLUS + (p->l.t != BC_LEX_OP_INC);

		bc_lex_next(&p->l);
		type = p->l.t;

		// Because we parse the next part of the expression
		// right here, we need to increment this.
		*nexs = *nexs + 1;

		// Is the next token a normal identifier?
		if (type == BC_LEX_NAME) {

			// Parse the name.
			uint8_t flags2 = flags & ~BC_PARSE_ARRAY;
			bc_parse_name(p, prev, can_assign, flags2 | BC_PARSE_NOCALL);
		}
		// Is the next token a global?
		else if (type >= BC_LEX_KW_LAST && type <= BC_LEX_KW_OBASE) {
			bc_parse_push(p, type - BC_LEX_KW_LAST + BC_INST_LAST);
			bc_lex_next(&p->l);
		}
		// Is the next token specifically scale, which needs special treatment?
		else if (BC_NO_ERR(type == BC_LEX_KW_SCALE)) {

			bc_lex_next(&p->l);

			// Check that scale() was not used.
			if (BC_ERR(p->l.t == BC_LEX_LPAREN))
				bc_parse_err(p, BC_ERR_PARSE_TOKEN);
			else bc_parse_push(p, BC_INST_SCALE);
		}
		// Now we know we have an error.
		else bc_parse_err(p, BC_ERR_PARSE_TOKEN);

		*can_assign = false;

		bc_parse_push(p, BC_INST_ONE);
		bc_parse_push(p, inst);
	}
}

/**
 * Parses the minus operator. This needs special treatment because it is either
 * subtract or negation.
 * @param p        The parser.
 * @param prev     An in/out parameter; the previous instruction.
 * @param ops_bgn  The size of the operator stack.
 * @param rparen   True if the last token was a right paren.
 * @param binlast  True if the last token was a binary operator.
 * @param nexprs   An in/out parameter; the number of unused expressions.
 */
static void bc_parse_minus(BcParse *p, BcInst *prev, size_t ops_bgn,
                           bool rparen, bool binlast, size_t *nexprs)
{
	BcLexType type;

	bc_lex_next(&p->l);

	// Figure out if it's a minus or a negation.
	type = BC_PARSE_LEAF(*prev, binlast, rparen) ? BC_LEX_OP_MINUS : BC_LEX_NEG;
	*prev = BC_PARSE_TOKEN_INST(type);

	// We can just push onto the op stack because this is the largest
	// precedence operator that gets pushed. Inc/dec does not.
	if (type != BC_LEX_OP_MINUS) bc_vec_push(&p->ops, &type);
	else bc_parse_operator(p, type, ops_bgn, nexprs);
}

/**
 * Parses a string.
 * @param p     The parser.
 * @param inst  The instruction corresponding to how the string was found and
 *              how it should be printed.
 */
static void bc_parse_str(BcParse *p, BcInst inst) {
	bc_parse_addString(p);
	bc_parse_push(p, inst);
	bc_lex_next(&p->l);
}

/**
 * Parses a print statement.
 * @param p  The parser.
 */
static void bc_parse_print(BcParse *p, BcLexType type) {

	BcLexType t;
	bool comma = false;
	BcInst inst = type == BC_LEX_KW_STREAM ?
	              BC_INST_PRINT_STREAM : BC_INST_PRINT_POP;

	bc_lex_next(&p->l);

	t = p->l.t;

	// A print or stream statement has to have *something*.
	if (bc_parse_isDelimiter(p)) bc_parse_err(p, BC_ERR_PARSE_PRINT);

	do {

		// If the token is a string, then print it with escapes.
		// BC_INST_PRINT_POP plays that role for bc.
		if (t == BC_LEX_STR) bc_parse_str(p, inst);
		else {
			// We have an actual number; parse and add a print instruction.
			bc_parse_expr_status(p, BC_PARSE_NEEDVAL, bc_parse_next_print);
			bc_parse_push(p, inst);
		}

		// Is the next token a comma?
		comma = (p->l.t == BC_LEX_COMMA);

		// Get the next token if we have a comma.
		if (comma) bc_lex_next(&p->l);
		else {

			// If we don't have a comma, the statement needs to end.
			if (!bc_parse_isDelimiter(p))
				bc_parse_err(p, BC_ERR_PARSE_TOKEN);
			else break;
		}

		t = p->l.t;

	} while (true);

	// If we have a comma but no token, that's bad.
	if (BC_ERR(comma)) bc_parse_err(p, BC_ERR_PARSE_TOKEN);
}

/**
 * Parses a return statement.
 * @param p  The parser.
 */
static void bc_parse_return(BcParse *p) {

	BcLexType t;
	bool paren;
	uchar inst = BC_INST_RET0;

	// If we are not in a function, that's an error.
	if (BC_ERR(!BC_PARSE_FUNC(p))) bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	// If we are in a void function, make sure to return void.
	if (p->func->voidfn) inst = BC_INST_RET_VOID;

	bc_lex_next(&p->l);

	t = p->l.t;
	paren = (t == BC_LEX_LPAREN);

	// An empty return statement just needs to push the selected instruction.
	if (bc_parse_isDelimiter(p)) bc_parse_push(p, inst);
	else {

		BcParseStatus s;

		// Need to parse the expression whose value will be returned.
		s = bc_parse_expr_err(p, BC_PARSE_NEEDVAL, bc_parse_next_expr);

		// If the expression was empty, just push the selected instruction.
		if (s == BC_PARSE_STATUS_EMPTY_EXPR) {
			bc_parse_push(p, inst);
			bc_lex_next(&p->l);
		}

		// POSIX requires parentheses.
		if (!paren || p->l.last != BC_LEX_RPAREN) {
			bc_parse_err(p, BC_ERR_POSIX_RET);
		}

		// Void functions require an empty expression.
		if (BC_ERR(p->func->voidfn)) {
			if (s != BC_PARSE_STATUS_EMPTY_EXPR)
				bc_parse_verr(p, BC_ERR_PARSE_RET_VOID, p->func->name);
		}
		// If we got here, we want to be sure to end the function with a real
		// return instruction, just in case.
		else bc_parse_push(p, BC_INST_RET);
	}
}

/**
 * Clears flags that indicate the end of an if statement and its block and sets
 * the jump location.
 * @param p  The parser.
 */
static void bc_parse_noElse(BcParse *p) {
	uint16_t *flag_ptr = BC_PARSE_TOP_FLAG_PTR(p);
	*flag_ptr = (*flag_ptr & ~(BC_PARSE_FLAG_IF_END));
	bc_parse_setLabel(p);
}

/**
 * Ends (finishes parsing) the body of a control statement or a function.
 * @param p      The parser.
 * @param brace  True if the body was ended by a brace, false otherwise.
 */
static void bc_parse_endBody(BcParse *p, bool brace) {

	bool has_brace, new_else = false;

	// We cannot be ending a body if there are no bodies to end.
	if (BC_ERR(p->flags.len <= 1)) bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	if (brace) {

		// The brace was already gotten; make sure that the caller did not lie.
		// We check for the requirement of braces later.
		assert(p->l.t == BC_LEX_RBRACE);

		bc_lex_next(&p->l);

		// If the next token is not a delimiter, that is a problem.
		if (BC_ERR(!bc_parse_isDelimiter(p)))
			bc_parse_err(p, BC_ERR_PARSE_TOKEN);
	}

	// Do we have a brace flag?
	has_brace = (BC_PARSE_BRACE(p) != 0);

	do {
		size_t len = p->flags.len;
		bool loop;

		// If we have a brace flag but not a brace, that's a problem.
		if (has_brace && !brace) bc_parse_err(p, BC_ERR_PARSE_TOKEN);

		// Are we inside a loop?
		loop = (BC_PARSE_LOOP_INNER(p) != 0);

		// If we are ending a loop or an else...
		if (loop || BC_PARSE_ELSE(p)) {

			// Loops have condition labels that we have to take care of as well.
			if (loop) {

				size_t *label = bc_vec_top(&p->conds);

				bc_parse_push(p, BC_INST_JUMP);
				bc_parse_pushIndex(p, *label);

				bc_vec_pop(&p->conds);
			}

			bc_parse_setLabel(p);
			bc_vec_pop(&p->flags);
		}
		// If we are ending a function...
		else if (BC_PARSE_FUNC_INNER(p)) {
			BcInst inst = (p->func->voidfn ? BC_INST_RET_VOID : BC_INST_RET0);
			bc_parse_push(p, inst);
			bc_parse_updateFunc(p, BC_PROG_MAIN);
			bc_vec_pop(&p->flags);
		}
		// If we have a brace flag and not an if statement, we can pop the top
		// of the flags stack because they have been taken care of above.
		else if (has_brace && !BC_PARSE_IF(p)) bc_vec_pop(&p->flags);

		// This needs to be last to parse nested if's properly.
		if (BC_PARSE_IF(p) && (len == p->flags.len || !BC_PARSE_BRACE(p))) {

			// Eat newlines.
			while (p->l.t == BC_LEX_NLINE) bc_lex_next(&p->l);

			// *Now* we can pop the flags.
			bc_vec_pop(&p->flags);

			// If we are allowed non-POSIX stuff...
			if (!BC_S) {

				// Have we found yet another dangling else?
				*(BC_PARSE_TOP_FLAG_PTR(p)) |= BC_PARSE_FLAG_IF_END;
				new_else = (p->l.t == BC_LEX_KW_ELSE);

				// Parse the else or end the if statement body.
				if (new_else) bc_parse_else(p);
				else if (!has_brace && (!BC_PARSE_IF_END(p) || brace))
					bc_parse_noElse(p);
			}
			// POSIX requires us to do the bare minimum only.
			else bc_parse_noElse(p);
		}

		// If these are both true, we have "used" the braces that we found.
		if (brace && has_brace) brace = false;

	// This condition was perhaps the hardest single part of the parser. If the
	// flags stack does not have enough, we should stop. If we have a new else
	// statement, we should stop. If we do have the end of an if statement and
	// we have eaten the brace, we should stop. If we do have a brace flag, we
	// should stop.
	} while (p->flags.len > 1 && !new_else && (!BC_PARSE_IF_END(p) || brace) &&
	         !(has_brace = (BC_PARSE_BRACE(p) != 0)));

	// If we have a brace, yet no body for it, that's a problem.
	if (BC_ERR(p->flags.len == 1 && brace))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);
	else if (brace && BC_PARSE_BRACE(p)) {

		// If we make it here, we have a brace and a flag for it.
		uint16_t flags = BC_PARSE_TOP_FLAG(p);

		// This condition ensure that the *last* body is correctly finished by
		// popping its flags.
		if (!(flags & (BC_PARSE_FLAG_FUNC_INNER | BC_PARSE_FLAG_LOOP_INNER)) &&
		    !(flags & (BC_PARSE_FLAG_IF | BC_PARSE_FLAG_ELSE)) &&
		    !(flags & (BC_PARSE_FLAG_IF_END)))
		{
			bc_vec_pop(&p->flags);
		}
	}
}

/**
 * Starts the body of a control statement or function.
 * @param p      The parser.
 * @param flags  The current flags (will be edited).
 */
static void bc_parse_startBody(BcParse *p, uint16_t flags) {
	assert(flags);
	flags |= (BC_PARSE_TOP_FLAG(p) & (BC_PARSE_FLAG_FUNC | BC_PARSE_FLAG_LOOP));
	flags |= BC_PARSE_FLAG_BODY;
	bc_vec_push(&p->flags, &flags);
}

/**
 * Parses an if statement.
 * @param p  The parser.
 */
static void bc_parse_if(BcParse *p) {

	// We are allowed relational operators, and we must have a value.
	size_t idx;
	uint8_t flags = (BC_PARSE_REL | BC_PARSE_NEEDVAL);

	// Get the left paren and barf if necessary.
	bc_lex_next(&p->l);
	if (BC_ERR(p->l.t != BC_LEX_LPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	// Parse the condition.
	bc_lex_next(&p->l);
	bc_parse_expr_status(p, flags, bc_parse_next_rel);

	// Must have a right paren.
	if (BC_ERR(p->l.t != BC_LEX_RPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	bc_lex_next(&p->l);

	// Insert the conditional jump instruction.
	bc_parse_push(p, BC_INST_JUMP_ZERO);

	idx = p->func->labels.len;

	// Push the index for the instruction and create an exit label for an else
	// statement.
	bc_parse_pushIndex(p, idx);
	bc_parse_createExitLabel(p, idx, false);

	bc_parse_startBody(p, BC_PARSE_FLAG_IF);
}

/**
 * Parses an else statement.
 * @param p  The parser.
 */
static void bc_parse_else(BcParse *p) {

	size_t idx = p->func->labels.len;

	// We must be at the end of an if statement.
	if (BC_ERR(!BC_PARSE_IF_END(p)))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	// Push an unconditional jump to make bc jump over the else statement if it
	// executed the original if statement.
	bc_parse_push(p, BC_INST_JUMP);
	bc_parse_pushIndex(p, idx);

	// Clear the else stuff. Yes, that function is misnamed for its use here,
	// but deal with it.
	bc_parse_noElse(p);

	// Create the exit label and parse the body.
	bc_parse_createExitLabel(p, idx, false);
	bc_parse_startBody(p, BC_PARSE_FLAG_ELSE);

	bc_lex_next(&p->l);
}

/**
 * Parse a while loop.
 * @param p  The parser.
 */
static void bc_parse_while(BcParse *p) {

	// We are allowed relational operators, and we must have a value.
	size_t idx;
	uint8_t flags = (BC_PARSE_REL | BC_PARSE_NEEDVAL);

	// Get the left paren and barf if necessary.
	bc_lex_next(&p->l);
	if (BC_ERR(p->l.t != BC_LEX_LPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);
	bc_lex_next(&p->l);

	// Create the labels. Loops need both.
	bc_parse_createCondLabel(p, p->func->labels.len);
	idx = p->func->labels.len;
	bc_parse_createExitLabel(p, idx, true);

	// Parse the actual condition and barf on non-right paren.
	bc_parse_expr_status(p, flags, bc_parse_next_rel);
	if (BC_ERR(p->l.t != BC_LEX_RPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);
	bc_lex_next(&p->l);

	// Now we can push the conditional jump and start the body.
	bc_parse_push(p, BC_INST_JUMP_ZERO);
	bc_parse_pushIndex(p, idx);
	bc_parse_startBody(p, BC_PARSE_FLAG_LOOP | BC_PARSE_FLAG_LOOP_INNER);
}

/**
 * Parse a for loop.
 * @param p  The parser.
 */
static void bc_parse_for(BcParse *p) {

	size_t cond_idx, exit_idx, body_idx, update_idx;

	// Barf on the missing left paren.
	bc_lex_next(&p->l);
	if (BC_ERR(p->l.t != BC_LEX_LPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);
	bc_lex_next(&p->l);

	// The first statement can be empty, but if it is, check for error in POSIX
	// mode. Otherwise, parse it.
	if (p->l.t != BC_LEX_SCOLON)
		bc_parse_expr_status(p, 0, bc_parse_next_for);
	else bc_parse_err(p, BC_ERR_POSIX_FOR);

	// Must have a semicolon.
	if (BC_ERR(p->l.t != BC_LEX_SCOLON)) bc_parse_err(p, BC_ERR_PARSE_TOKEN);
	bc_lex_next(&p->l);

	// These are indices for labels. There are so many of them because the end
	// of the loop must unconditionally jump to the update code. Then the update
	// code must unconditionally jump to the condition code. Then the condition
	// code must *conditionally* jump to the exit.
	cond_idx = p->func->labels.len;
	update_idx = cond_idx + 1;
	body_idx = update_idx + 1;
	exit_idx = body_idx + 1;

	// This creates the condition label.
	bc_parse_createLabel(p, p->func->code.len);

	// Parse an expression if it exists.
	if (p->l.t != BC_LEX_SCOLON) {
		uint8_t flags = (BC_PARSE_REL | BC_PARSE_NEEDVAL);
		bc_parse_expr_status(p, flags, bc_parse_next_for);
	}
	else {

		// Set this for the next call to bc_parse_number because an empty
		// condition means that it is an infinite loop, so the condition must be
		// non-zero. This is safe to set because the current token is a
		// semicolon, which has no string requirement.
		bc_vec_string(&p->l.str, sizeof(bc_parse_one) - 1, bc_parse_one);
		bc_parse_number(p);

		// An empty condition makes POSIX mad.
		bc_parse_err(p, BC_ERR_POSIX_FOR);
	}

	// Must have a semicolon.
	if (BC_ERR(p->l.t != BC_LEX_SCOLON))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);
	bc_lex_next(&p->l);

	// Now we can set up the conditional jump to the exit and an unconditional
	// jump to the body right after. The unconditional jump to the body is
	// because there is update code coming right after the condition, so we need
	// to skip it to get to the body.
	bc_parse_push(p, BC_INST_JUMP_ZERO);
	bc_parse_pushIndex(p, exit_idx);
	bc_parse_push(p, BC_INST_JUMP);
	bc_parse_pushIndex(p, body_idx);

	// Now create the label for the update code.
	bc_parse_createCondLabel(p, update_idx);

	// Parse if not empty, and if it is, let POSIX yell if necessary.
	if (p->l.t != BC_LEX_RPAREN)
		bc_parse_expr_status(p, 0, bc_parse_next_rel);
	else bc_parse_err(p, BC_ERR_POSIX_FOR);

	// Must have a right paren.
	if (BC_ERR(p->l.t != BC_LEX_RPAREN))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	// Set up a jump to the condition right after the update code.
	bc_parse_push(p, BC_INST_JUMP);
	bc_parse_pushIndex(p, cond_idx);
	bc_parse_createLabel(p, p->func->code.len);

	// Create an exit label for the body and start the body.
	bc_parse_createExitLabel(p, exit_idx, true);
	bc_lex_next(&p->l);
	bc_parse_startBody(p, BC_PARSE_FLAG_LOOP | BC_PARSE_FLAG_LOOP_INNER);
}

/**
 * Parse a statement or token that indicates a loop exit. This includes an
 * actual loop exit, the break keyword, or the continue keyword.
 * @param p     The parser.
 * @param type  The type of exit.
 */
static void bc_parse_loopExit(BcParse *p, BcLexType type) {

	size_t i;
	BcInstPtr *ip;

	// Must have a loop. If we don't, that's an error.
	if (BC_ERR(!BC_PARSE_LOOP(p))) bc_parse_err(p, BC_ERR_PARSE_TOKEN);

	// If we have a break statement...
	if (type == BC_LEX_KW_BREAK) {

		// If there are no exits, something went wrong somewhere.
		if (BC_ERR(!p->exits.len)) bc_parse_err(p, BC_ERR_PARSE_TOKEN);

		// Get the exit.
		i = p->exits.len - 1;
		ip = bc_vec_item(&p->exits, i);

		// The condition !ip->func is true if the exit is not for a loop, so we
		// need to find the first actual loop exit.
		while (!ip->func && i < p->exits.len) ip = bc_vec_item(&p->exits, i--);

		// Make sure everything is hunky dory.
		assert(ip != NULL && (i < p->exits.len || ip->func));

		// Set the index for the exit.
		i = ip->idx;
	}
	// If we have a continue statement or just the loop end, jump to the
	// condition (or update for a foor loop).
	else i = *((size_t*) bc_vec_top(&p->conds));

	// Add the unconditional jump.
	bc_parse_push(p, BC_INST_JUMP);
	bc_parse_pushIndex(p, i);

	bc_lex_next(&p->l);
}

/**
 * Parse a function (header).
 * @param p  The parser.
 */
static void bc_parse_func(BcParse *p) {

	bool comma = false, voidfn;
	uint16_t flags;
	size_t idx;

	bc_lex_next(&p->l);

	// Must have a name.
	if (BC_ERR(p->l.t != BC_LEX_NAME)) bc_parse_err(p, BC_ERR_PARSE_FUNC);

	// If the name is "void", and POSIX is not on, mark as void.
	voidfn = (!BC_IS_POSIX && p->l.t == BC_LEX_NAME &&
	          !strcmp(p->l.str.v, "void"));

	// We can safely do this because the expected token should not overwrite the
	// function name.
	bc_lex_next(&p->l);

	// If we *don't* have another name, then void is the name of the function.
	voidfn = (voidfn && p->l.t == BC_LEX_NAME);

	// With a void function, allow POSIX to complain and get a new token.
	if (voidfn) {

		bc_parse_err(p, BC_ERR_POSIX_VOID);

		// We can safely do this because the expected token should not overwrite
		// the function name.
		bc_lex_next(&p->l);
	}

	// Must have a left paren.
	if (BC_ERR(p->l.t != BC_LEX_LPAREN))
		bc_parse_err(p, BC_ERR_PARSE_FUNC);

	// Make sure the functions map and vector are synchronized.
	assert(p->prog->fns.len == p->prog->fn_map.len);

	// Must lock signals because vectors are changed, and the vector functions
	// expect signals to be locked.
	BC_SIG_LOCK;

	// Insert the function by name into the map and vector.
	idx = bc_program_insertFunc(p->prog, p->l.str.v);

	BC_SIG_UNLOCK;

	// Make sure the insert worked.
	assert(idx);

	// Update the function pointer and stuff in the parser and set its void.
	bc_parse_updateFunc(p, idx);
	p->func->voidfn = voidfn;

	bc_lex_next(&p->l);

	// While we do not have a right paren, we are still parsing arguments.
	while (p->l.t != BC_LEX_RPAREN) {

		BcType t = BC_TYPE_VAR;

		// If we have an asterisk, we are parsing a reference argument.
		if (p->l.t == BC_LEX_OP_MULTIPLY) {

			t = BC_TYPE_REF;
			bc_lex_next(&p->l);

			// Let POSIX complain if necessary.
			bc_parse_err(p, BC_ERR_POSIX_REF);
		}

		// If we don't have a name, the argument will not have a name. Barf.
		if (BC_ERR(p->l.t != BC_LEX_NAME))
			bc_parse_err(p, BC_ERR_PARSE_FUNC);

		// Increment the number of parameters.
		p->func->nparams += 1;

		// Copy the string in the lexer so that we can use the lexer again.
		bc_vec_string(&p->buf, p->l.str.len, p->l.str.v);

		bc_lex_next(&p->l);

		// We are parsing an array parameter if this is true.
		if (p->l.t == BC_LEX_LBRACKET) {

			// Set the array type, unless we are already parsing a reference.
			if (t == BC_TYPE_VAR) t = BC_TYPE_ARRAY;

			bc_lex_next(&p->l);

			// The brackets *must* be empty.
			if (BC_ERR(p->l.t != BC_LEX_RBRACKET))
				bc_parse_err(p, BC_ERR_PARSE_FUNC);

			bc_lex_next(&p->l);
		}
		// If we did *not* get a bracket, but we are expecting a reference, we
		// have a problem.
		else if (BC_ERR(t == BC_TYPE_REF))
			bc_parse_verr(p, BC_ERR_PARSE_REF_VAR, p->buf.v);

		// Test for comma and get the next token if it exists.
		comma = (p->l.t == BC_LEX_COMMA);
		if (comma) bc_lex_next(&p->l);

		// Insert the parameter into the function.
		bc_func_insert(p->func, p->prog, p->buf.v, t, p->l.line);
	}

	// If we have a comma, but no parameter, barf.
	if (BC_ERR(comma)) bc_parse_err(p, BC_ERR_PARSE_FUNC);

	// Start the body.
	flags = BC_PARSE_FLAG_FUNC | BC_PARSE_FLAG_FUNC_INNER;
	bc_parse_startBody(p, flags);

	bc_lex_next(&p->l);

	// POSIX requires that a brace be on the same line as the function header.
	// If we don't have a brace, let POSIX throw an error.
	if (p->l.t != BC_LEX_LBRACE) bc_parse_err(p, BC_ERR_POSIX_BRACE);
}

/**
 * Parse an auto list.
 * @param p  The parser.
 */
static void bc_parse_auto(BcParse *p) {

	bool comma, one;

	// Error if the auto keyword appeared in the wrong place.
	if (BC_ERR(!p->auto_part)) bc_parse_err(p, BC_ERR_PARSE_TOKEN);
	bc_lex_next(&p->l);

	p->auto_part = comma = false;

	// We need at least one variable or array.
	one = (p->l.t == BC_LEX_NAME);

	// While we have a variable or array.
	while (p->l.t == BC_LEX_NAME) {

		BcType t;

		// Copy the name from the lexer, so we can use it again.
		bc_vec_string(&p->buf, p->l.str.len - 1, p->l.str.v);

		bc_lex_next(&p->l);

		// If we are parsing an array...
		if (p->l.t == BC_LEX_LBRACKET) {

			t = BC_TYPE_ARRAY;

			bc_lex_next(&p->l);

			// The brackets *must* be empty.
			if (BC_ERR(p->l.t != BC_LEX_RBRACKET))
				bc_parse_err(p, BC_ERR_PARSE_FUNC);

			bc_lex_next(&p->l);
		}
		else t = BC_TYPE_VAR;

		// Test for comma and get the next token if it exists.
		comma = (p->l.t == BC_LEX_COMMA);
		if (comma) bc_lex_next(&p->l);

		// Insert the auto into the function.
		bc_func_insert(p->func, p->prog, p->buf.v, t, p->l.line);
	}

	// If we have a comma, but no auto, barf.
	if (BC_ERR(comma)) bc_parse_err(p, BC_ERR_PARSE_FUNC);

	// If we don't have any variables or arrays, barf.
	if (BC_ERR(!one)) bc_parse_err(p, BC_ERR_PARSE_NO_AUTO);

	// The auto statement should be all that's in the statement.
	if (BC_ERR(!bc_parse_isDelimiter(p)))
		bc_parse_err(p, BC_ERR_PARSE_TOKEN);
}

/**
 * Parses a body.
 * @param p      The parser.
 * @param brace  True if a brace was encountered, false otherwise.
 */
static void bc_parse_body(BcParse *p, bool brace) {

	uint16_t *flag_ptr = BC_PARSE_TOP_FLAG_PTR(p);

	assert(flag_ptr != NULL);
	assert(p->flags.len >= 2);

	// The body flag is for when we expect a body. We got a body, so clear the
	// flag.
	*flag_ptr &= ~(BC_PARSE_FLAG_BODY);

	// If we are inside a function, that means we just barely entered it, and
	// we can expect an auto list.
	if (*flag_ptr & BC_PARSE_FLAG_FUNC_INNER) {

		// We *must* have a brace in this case.
		if (BC_ERR(!brace)) bc_parse_err(p, BC_ERR_PARSE_TOKEN);

		p->auto_part = (p->l.t != BC_LEX_KW_AUTO);

		if (!p->auto_part) {

			// Make sure this is true to not get a parse error.
			p->auto_part = true;

			// Since we already have the auto keyword, parse.
			bc_parse_auto(p);
		}

		// Eat a newline.
		if (p->l.t == BC_LEX_NLINE) bc_lex_next(&p->l);
	}
	else {

		// This is the easy part.
		size_t len = p->flags.len;

		assert(*flag_ptr);

		// Parse a statement.
		bc_parse_stmt(p);

		// This is a very important condition to get right. If there is no
		// brace, and no body flag, and the flags len hasn't shrunk, then we
		// have a body that was not delimited by braces, so we need to end it
		// now, after just one statement.
		if (!brace && !BC_PARSE_BODY(p) && len <= p->flags.len)
			bc_parse_endBody(p, false);
	}
}

/**
 * Parses a statement. This is the entry point for just about everything, except
 * function definitions.
 * @param p  The parser.
 */
static void bc_parse_stmt(BcParse *p) {

	size_t len;
	uint16_t flags;
	BcLexType type = p->l.t;

	// Eat newline.
	if (type == BC_LEX_NLINE) {
		bc_lex_next(&p->l);
		return;
	}

	// Eat auto list.
	if (type == BC_LEX_KW_AUTO) {
		bc_parse_auto(p);
		return;
	}

	// If we reach this point, no auto list is allowed.
	p->auto_part = false;

	// Everything but an else needs to be taken care of here, but else is
	// special.
	if (type != BC_LEX_KW_ELSE) {

		// After an if, no else found.
		if (BC_PARSE_IF_END(p)) {

			// Clear the expectation for else, end body, and return. Returning
			// gives us a clean slate for parsing again.
			bc_parse_noElse(p);
			if (p->flags.len > 1 && !BC_PARSE_BRACE(p))
				bc_parse_endBody(p, false);
			return;
		}
		// With a left brace, we are parsing a body.
		else if (type == BC_LEX_LBRACE) {

			// We need to start a body if we are not expecting one yet.
			if (!BC_PARSE_BODY(p)) {
				bc_parse_startBody(p, BC_PARSE_FLAG_BRACE);
				bc_lex_next(&p->l);
			}
			// If we *are* expecting a body, that body should get a brace. This
			// takes care of braces being on a different line than if and loop
			// headers.
			else {
				*(BC_PARSE_TOP_FLAG_PTR(p)) |= BC_PARSE_FLAG_BRACE;
				bc_lex_next(&p->l);
				bc_parse_body(p, true);
			}

			// If we have reached this point, we need to return for a clean
			// slate.
			return;
		}
		// This happens when we are expecting a body and get a single statement,
		// i.e., a body with no braces surrounding it. Returns after for a clean
		// slate.
		else if (BC_PARSE_BODY(p) && !BC_PARSE_BRACE(p)) {
			bc_parse_body(p, false);
			return;
		}
	}

	len = p->flags.len;
	flags = BC_PARSE_TOP_FLAG(p);

	switch (type) {

		// All of these are valid for expressions.
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
#if BC_ENABLE_EXTRA_MATH
		case BC_LEX_KW_SEED:
#endif // BC_ENABLE_EXTRA_MATH
		case BC_LEX_KW_SQRT:
		case BC_LEX_KW_ABS:
#if BC_ENABLE_EXTRA_MATH
		case BC_LEX_KW_IRAND:
#endif // BC_ENABLE_EXTRA_MATH
		case BC_LEX_KW_ASCIIFY:
		case BC_LEX_KW_MODEXP:
		case BC_LEX_KW_DIVMOD:
		case BC_LEX_KW_READ:
#if BC_ENABLE_EXTRA_MATH
		case BC_LEX_KW_RAND:
#endif // BC_ENABLE_EXTRA_MATH
		case BC_LEX_KW_MAXIBASE:
		case BC_LEX_KW_MAXOBASE:
		case BC_LEX_KW_MAXSCALE:
#if BC_ENABLE_EXTRA_MATH
		case BC_LEX_KW_MAXRAND:
#endif // BC_ENABLE_EXTRA_MATH
		case BC_LEX_KW_LINE_LENGTH:
		case BC_LEX_KW_GLOBAL_STACKS:
		case BC_LEX_KW_LEADING_ZERO:
		{
			bc_parse_expr_status(p, BC_PARSE_PRINT, bc_parse_next_expr);
			break;
		}

		case BC_LEX_KW_ELSE:
		{
			bc_parse_else(p);
			break;
		}

		// Just eat.
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
			// `limits` is a compile-time command, so execute it right away.
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
#if BC_ENABLE_EXTRA_MATH
			bc_vm_printf("BC_RAND_MAX      = %lu\n", BC_MAX_RAND);
#endif // BC_ENABLE_EXTRA_MATH
			bc_vm_printf("MAX Exponent     = %lu\n", BC_MAX_EXP);
			bc_vm_printf("Number of vars   = %lu\n", BC_MAX_VARS);

			bc_lex_next(&p->l);

			break;
		}

		case BC_LEX_KW_STREAM:
		case BC_LEX_KW_PRINT:
		{
			bc_parse_print(p, type);
			break;
		}

		case BC_LEX_KW_QUIT:
		{
			// Quit is a compile-time command. We don't exit directly, so the vm
			// can clean up.
			vm.status = BC_STATUS_QUIT;
			BC_JMP;
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

	// If the flags did not change, we expect a delimiter.
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

	// We should not let an EOF get here unless some partial parse was not
	// completed, in which case, it's the user's fault.
	if (BC_ERR(p->l.t == BC_LEX_EOF)) bc_parse_err(p, BC_ERR_PARSE_EOF);

	// Functions need special parsing.
	else if (p->l.t == BC_LEX_KW_DEFINE) {
		if (BC_ERR(BC_PARSE_NO_EXEC(p))) {
			if (p->flags.len == 1 &&
			    BC_PARSE_TOP_FLAG(p) == BC_PARSE_FLAG_IF_END)
			{
				bc_parse_noElse(p);
			}
			else bc_parse_err(p, BC_ERR_PARSE_TOKEN);
		}
		bc_parse_func(p);
	}

	// Otherwise, parse a normal statement.
	else bc_parse_stmt(p);

exit:

	BC_SIG_MAYLOCK;

	// We need to reset on error.
	if (BC_ERR(((vm.status && vm.status != BC_STATUS_QUIT) || vm.sig)))
		bc_parse_reset(p);

	BC_LONGJMP_CONT;
}

/**
 * Parse an expression. This is the actual implementation of the Shunting-Yard
 * Algorithm.
 * @param p      The parser.
 * @param flags  The flags for what is valid in the expression.
 * @param next   A set of tokens for what is valid *after* the expression.
 * @return       A parse status. In some places, an empty expression is an
 *               error, and sometimes, it is required. This allows this function
 *               to tell the caller if the expression was empty and let the
 *               caller handle it.
 */
static BcParseStatus bc_parse_expr_err(BcParse *p, uint8_t flags,
                                       BcParseNext next)
{
	BcInst prev = BC_INST_PRINT;
	uchar inst = BC_INST_INVALID;
	BcLexType top, t;
	size_t nexprs, ops_bgn;
	uint32_t i, nparens, nrelops;
	bool pfirst, rprn, done, get_token, assign, bin_last, incdec, can_assign;

	// One of these *must* be true.
	assert(!(flags & BC_PARSE_PRINT) || !(flags & BC_PARSE_NEEDVAL));

	// These are set very carefully. In fact, controlling the values of these
	// locals is the biggest part of making this work. ops_bgn especially is
	// important because it marks where the operator stack begins for *this*
	// invocation of this function. That's because bc_parse_expr_err() is
	// recursive (the Shunting-Yard Algorithm is most easily expressed
	// recursively when parsing subexpressions), and each invocation needs to
	// know where to stop.
	//
	// - nparens is the number of left parens without matches.
	// - nrelops is the number of relational operators that appear in the expr.
	// - nexprs is the number of unused expressions.
	// - rprn is a right paren encountered last.
	// - done means the expression has been fully parsed.
	// - get_token is true when a token is needed at the end of an iteration.
	// - assign is true when an assignment statement was parsed last.
	// - incdec is true when the previous operator was an inc or dec operator.
	// - can_assign is true when an assignemnt is valid.
	// - bin_last is true when the previous instruction was a binary operator.
	t = p->l.t;
	pfirst = (p->l.t == BC_LEX_LPAREN);
	nparens = nrelops = 0;
	nexprs = 0;
	ops_bgn = p->ops.len;
	rprn = done = get_token = assign = incdec = can_assign = false;
	bin_last = true;

	// We want to eat newlines if newlines are not a valid ending token.
	// This is for spacing in things like for loop headers.
	if (!(flags & BC_PARSE_NOREAD)) {
		while ((t = p->l.t) == BC_LEX_NLINE) bc_lex_next(&p->l);
	}

	// This is the Shunting-Yard algorithm loop.
	for (; !done && BC_PARSE_EXPR(t); t = p->l.t)
	{
		switch (t) {

			case BC_LEX_OP_INC:
			case BC_LEX_OP_DEC:
			{
				// These operators can only be used with items that can be
				// assigned to.
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
				// The previous token must have been a leaf expression, or the
				// operator is in the wrong place.
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

				// This is true if it was a binary operator last.
				bin_last = (prev == BC_INST_MINUS);
				if (bin_last) incdec = false;

				flags &= ~(BC_PARSE_ARRAY);

				break;
			}

			// All of this group, including the fallthrough, is to parse binary
			// operators.
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
				// We need to make sure the assignment is valid.
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
				// This is true if the operator if the token is a prefix
				// operator. This is only for boolean not.
				if (BC_PARSE_OP_PREFIX(t)) {

					// Prefix operators are only allowed after binary operators
					// or prefix operators.
					if (BC_ERR(!bin_last && !BC_PARSE_OP_PREFIX(p->l.last)))
						bc_parse_err(p, BC_ERR_PARSE_EXPR);
				}
				// If we execute the else, that means we have a binary operator.
				// If the previous operator was a prefix or a binary operator,
				// then a binary operator is not allowed.
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
				// A left paren is *not* allowed right after a leaf expr.
				if (BC_ERR(BC_PARSE_LEAF(prev, bin_last, rprn)))
					bc_parse_err(p, BC_ERR_PARSE_EXPR);

				nparens += 1;
				rprn = incdec = can_assign = false;
				get_token = true;

				// Push the paren onto the operator stack.
				bc_vec_push(&p->ops, &t);

				break;
			}

			case BC_LEX_RPAREN:
			{
				// This needs to be a status. The error is handled in
				// bc_parse_expr_status().
				if (BC_ERR(p->l.last == BC_LEX_LPAREN))
					return BC_PARSE_STATUS_EMPTY_EXPR;

				// The right paren must not come after a prefix or binary
				// operator.
				if (BC_ERR(bin_last || BC_PARSE_PREV_PREFIX(prev)))
					bc_parse_err(p, BC_ERR_PARSE_EXPR);

				// If there are no parens left, we are done, but we need another
				// token.
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

			case BC_LEX_STR:
			{
				// POSIX only allows strings alone.
				if (BC_IS_POSIX) bc_parse_err(p, BC_ERR_POSIX_EXPR_STRING);

				// A string is a leaf and cannot come right after a leaf.
				if (BC_ERR(BC_PARSE_LEAF(prev, bin_last, rprn)))
					bc_parse_err(p, BC_ERR_PARSE_EXPR);

				bc_parse_addString(p);

				get_token = true;
				bin_last = rprn = false;
				nexprs += 1;

				break;
			}

			case BC_LEX_NAME:
			{
				// A name is a leaf and cannot come right after a leaf.
				if (BC_ERR(BC_PARSE_LEAF(prev, bin_last, rprn)))
					bc_parse_err(p, BC_ERR_PARSE_EXPR);

				get_token = bin_last = false;

				bc_parse_name(p, &prev, &can_assign, flags & ~BC_PARSE_NOCALL);

				rprn = (prev == BC_INST_CALL);
				nexprs += 1;
				flags &= ~(BC_PARSE_ARRAY);

				break;
			}

			case BC_LEX_NUMBER:
			{
				// A number is a leaf and cannot come right after a leaf.
				if (BC_ERR(BC_PARSE_LEAF(prev, bin_last, rprn)))
					bc_parse_err(p, BC_ERR_PARSE_EXPR);

				// The number instruction is pushed in here.
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
#if BC_ENABLE_EXTRA_MATH
			case BC_LEX_KW_SEED:
#endif // BC_ENABLE_EXTRA_MATH
			{
				// All of these are leaves and cannot come right after a leaf.
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
#if BC_ENABLE_EXTRA_MATH
			case BC_LEX_KW_IRAND:
#endif // BC_ENABLE_EXTRA_MATH
			case BC_LEX_KW_ASCIIFY:
			{
				// All of these are leaves and cannot come right after a leaf.
				if (BC_ERR(BC_PARSE_LEAF(prev, bin_last, rprn)))
					bc_parse_err(p, BC_ERR_PARSE_EXPR);

				bc_parse_builtin(p, t, flags, &prev);

				rprn = get_token = bin_last = incdec = can_assign = false;
				nexprs += 1;
				flags &= ~(BC_PARSE_ARRAY);

				break;
			}

			case BC_LEX_KW_READ:
#if BC_ENABLE_EXTRA_MATH
			case BC_LEX_KW_RAND:
#endif // BC_ENABLE_EXTRA_MATH
			case BC_LEX_KW_MAXIBASE:
			case BC_LEX_KW_MAXOBASE:
			case BC_LEX_KW_MAXSCALE:
#if BC_ENABLE_EXTRA_MATH
			case BC_LEX_KW_MAXRAND:
#endif // BC_ENABLE_EXTRA_MATH
			case BC_LEX_KW_LINE_LENGTH:
			case BC_LEX_KW_GLOBAL_STACKS:
			case BC_LEX_KW_LEADING_ZERO:
			{
				// All of these are leaves and cannot come right after a leaf.
				if (BC_ERR(BC_PARSE_LEAF(prev, bin_last, rprn)))
					bc_parse_err(p, BC_ERR_PARSE_EXPR);

				// Error if we have read and it's not allowed.
				else if (t == BC_LEX_KW_READ && BC_ERR(flags & BC_PARSE_NOREAD))
					bc_parse_err(p, BC_ERR_EXEC_REC_READ);

				prev = t - BC_LEX_KW_READ + BC_INST_READ;
				bc_parse_noArgBuiltin(p, prev);

				rprn = get_token = bin_last = incdec = can_assign = false;
				nexprs += 1;
				flags &= ~(BC_PARSE_ARRAY);

				break;
			}

			case BC_LEX_KW_SCALE:
			{
				// This is a leaf and cannot come right after a leaf.
				if (BC_ERR(BC_PARSE_LEAF(prev, bin_last, rprn)))
					bc_parse_err(p, BC_ERR_PARSE_EXPR);

				// Scale needs special work because it can be a variable *or* a
				// function.
				bc_parse_scale(p, &prev, &can_assign, flags);

				rprn = get_token = bin_last = false;
				nexprs += 1;
				flags &= ~(BC_PARSE_ARRAY);

				break;
			}

			case BC_LEX_KW_MODEXP:
			case BC_LEX_KW_DIVMOD:
			{
				// This is a leaf and cannot come right after a leaf.
				if (BC_ERR(BC_PARSE_LEAF(prev, bin_last, rprn)))
					bc_parse_err(p, BC_ERR_PARSE_EXPR);

				bc_parse_builtin3(p, t, flags, &prev);

				rprn = get_token = bin_last = incdec = can_assign = false;
				nexprs += 1;
				flags &= ~(BC_PARSE_ARRAY);

				break;
			}

			default:
			{
#ifndef NDEBUG
				// We should never get here, even in debug builds.
				bc_parse_err(p, BC_ERR_PARSE_TOKEN);
				break;
#endif // NDEBUG
			}
		}

		if (get_token) bc_lex_next(&p->l);
	}

	// Now that we have parsed the expression, we need to empty the operator
	// stack.
	while (p->ops.len > ops_bgn) {

		top = BC_PARSE_TOP_OP(p);
		assign = top >= BC_LEX_OP_ASSIGN_POWER && top <= BC_LEX_OP_ASSIGN;

		// There should not be *any* parens on the stack anymore.
		if (BC_ERR(top == BC_LEX_LPAREN || top == BC_LEX_RPAREN))
			bc_parse_err(p, BC_ERR_PARSE_EXPR);

		bc_parse_push(p, BC_PARSE_TOKEN_INST(top));

		// Adjust the number of unused expressions.
		nexprs -= !BC_PARSE_OP_PREFIX(top);
		bc_vec_pop(&p->ops);

		incdec = false;
	}

	// There must be only one expression at the top.
	if (BC_ERR(nexprs != 1)) bc_parse_err(p, BC_ERR_PARSE_EXPR);

	// Check that the next token is correct.
	for (i = 0; i < next.len && t != next.tokens[i]; ++i);
	if (BC_ERR(i == next.len && !bc_parse_isDelimiter(p)))
		bc_parse_err(p, BC_ERR_PARSE_EXPR);

	// Check that POSIX would be happy with the number of relational operators.
	if (!(flags & BC_PARSE_REL) && nrelops)
		bc_parse_err(p, BC_ERR_POSIX_REL_POS);
	else if ((flags & BC_PARSE_REL) && nrelops > 1)
		bc_parse_err(p, BC_ERR_POSIX_MULTIREL);

	// If this is true, then we might be in a situation where we don't print.
	// We would want to have the increment/decrement operator not make an extra
	// copy if it's not necessary.
	if (!(flags & BC_PARSE_NEEDVAL) && !pfirst) {

		// We have the easy case if the last operator was an assignment
		// operator.
		if (assign) {
			inst = *((uchar*) bc_vec_top(&p->func->code));
			inst += (BC_INST_ASSIGN_POWER_NO_VAL - BC_INST_ASSIGN_POWER);
			incdec = false;
		}
		// If we have an inc/dec operator and we are *not* printing, implement
		// the optimization to get rid of the extra copy.
		else if (incdec && !(flags & BC_PARSE_PRINT)) {
			inst = *((uchar*) bc_vec_top(&p->func->code));
			incdec = (inst <= BC_INST_DEC);
			inst = BC_INST_ASSIGN_PLUS_NO_VAL + (inst != BC_INST_INC &&
			                                     inst != BC_INST_ASSIGN_PLUS);
		}

		// This condition allows us to change the previous assignment
		// instruction (which does a copy) for a NO_VAL version, which does not.
		// This condition is set if either of the above if statements ends up
		// being true.
		if (inst >= BC_INST_ASSIGN_POWER_NO_VAL &&
		    inst <= BC_INST_ASSIGN_NO_VAL)
		{
			// Pop the previous assignment instruction and push a new one.
			// Inc/dec needs the extra instruction because it is now a binary
			// operator and needs a second operand.
			bc_vec_pop(&p->func->code);
			if (incdec) bc_parse_push(p, BC_INST_ONE);
			bc_parse_push(p, inst);
		}
	}

	// If we might have to print...
	if ((flags & BC_PARSE_PRINT)) {

		// With a paren first or the last operator not being an assignment, we
		// *do* want to print.
		if (pfirst || !assign) bc_parse_push(p, BC_INST_PRINT);
	}
	// We need to make sure to push a pop instruction for assignment statements
	// that will not print. The print will pop, but without it, we need to pop.
	else if (!(flags & BC_PARSE_NEEDVAL) &&
	         (inst < BC_INST_ASSIGN_POWER_NO_VAL ||
	          inst > BC_INST_ASSIGN_NO_VAL))
	{
		bc_parse_push(p, BC_INST_POP);
	}

	// We want to eat newlines if newlines are not a valid ending token.
	// This is for spacing in things like for loop headers.
	//
	// Yes, this is one case where I reuse a variable for a different purpose;
	// in this case, incdec being true now means that newlines are not valid.
	for (incdec = true, i = 0; i < next.len && incdec; ++i)
		incdec = (next.tokens[i] != BC_LEX_NLINE);
	if (incdec) {
		while (p->l.t == BC_LEX_NLINE) bc_lex_next(&p->l);
	}

	return BC_PARSE_STATUS_SUCCESS;
}

/**
 * Parses an expression with bc_parse_expr_err(), but throws an error if it gets
 * an empty expression.
 * @param p      The parser.
 * @param flags  The flags for what is valid in the expression.
 * @param next   A set of tokens for what is valid *after* the expression.
 */
static void bc_parse_expr_status(BcParse *p, uint8_t flags, BcParseNext next) {

	BcParseStatus s = bc_parse_expr_err(p, flags, next);

	if (BC_ERR(s == BC_PARSE_STATUS_EMPTY_EXPR))
		bc_parse_err(p, BC_ERR_PARSE_EMPTY_EXPR);
}

void bc_parse_expr(BcParse *p, uint8_t flags) {
	assert(p);
	bc_parse_expr_status(p, flags, bc_parse_next_read);
}
#endif // BC_ENABLED
