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
 * Code common to the parsers.
 *
 */

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <limits.h>

#include <parse.h>
#include <program.h>
#include <vm.h>

void
bc_parse_updateFunc(BcParse* p, size_t fidx)
{
	p->fidx = fidx;
	p->func = bc_vec_item(&p->prog->fns, fidx);
}

inline void
bc_parse_pushName(const BcParse* p, char* name, bool var)
{
	bc_parse_pushIndex(p, bc_program_search(p->prog, name, var));
}

/**
 * Updates the function, then pushes the instruction and the index. This is a
 * convenience function.
 * @param p     The parser.
 * @param inst  The instruction to push.
 * @param idx   The index to push.
 */
static inline void
bc_parse_pushInstIdx(BcParse* p, uchar inst, size_t idx)
{
	bc_parse_push(p, inst);
	bc_parse_pushIndex(p, idx);
}

void
bc_parse_addString(BcParse* p)
{
	size_t idx;

	idx = bc_program_addString(p->prog, p->l.str.v);

	// Push the string info.
	bc_parse_pushInstIdx(p, BC_INST_STR, idx);
}

static void
bc_parse_addNum(BcParse* p, const char* string)
{
	BcProgram* prog = p->prog;
	size_t idx;

	// XXX: This function has an implicit assumption: that string is a valid C
	// string with a nul terminator. This is because of the unchecked array
	// accesses below. I can't check this with an assert() because that could
	// lead to out-of-bounds access.
	//
	// XXX: In fact, just for safety's sake, assume that this function needs a
	// non-empty string with a nul terminator, just in case bc_parse_zero or
	// bc_parse_one change in the future, which I doubt.

	BC_SIG_ASSERT_LOCKED;

	// Special case 0.
	if (bc_parse_zero[0] == string[0] && bc_parse_zero[1] == string[1])
	{
		bc_parse_push(p, BC_INST_ZERO);
		return;
	}

	// Special case 1.
	if (bc_parse_one[0] == string[0] && bc_parse_one[1] == string[1])
	{
		bc_parse_push(p, BC_INST_ONE);
		return;
	}

	if (bc_map_insert(&prog->const_map, string, prog->consts.len, &idx))
	{
		BcConst* c;
		BcId* id = bc_vec_item(&prog->const_map, idx);

		// Get the index.
		idx = id->idx;

		// Push an empty constant.
		c = bc_vec_pushEmpty(&prog->consts);

		// Set the fields. We reuse the string in the ID (allocated by
		// bc_map_insert()), because why not?
		c->val = id->name;
		c->base = BC_NUM_BIGDIG_MAX;

		// We need this to be able to tell that the number has not been
		// allocated.
		bc_num_clear(&c->num);
	}
	else
	{
		BcId* id = bc_vec_item(&prog->const_map, idx);
		idx = id->idx;
	}

	bc_parse_pushInstIdx(p, BC_INST_NUM, idx);
}

void
bc_parse_number(BcParse* p)
{
#if BC_ENABLE_EXTRA_MATH
	char* exp = strchr(p->l.str.v, 'e');
	size_t idx = SIZE_MAX;

	// Do we have a number in scientific notation? If so, add a nul byte where
	// the e is.
	if (exp != NULL)
	{
		idx = ((size_t) (exp - p->l.str.v));
		*exp = 0;
	}
#endif // BC_ENABLE_EXTRA_MATH

	bc_parse_addNum(p, p->l.str.v);

#if BC_ENABLE_EXTRA_MATH
	// If we have a number in scientific notation...
	if (exp != NULL)
	{
		bool neg;

		// Figure out if the exponent is negative.
		neg = (*((char*) bc_vec_item(&p->l.str, idx + 1)) == BC_LEX_NEG_CHAR);

		// Add the number and instruction.
		bc_parse_addNum(p, bc_vec_item(&p->l.str, idx + 1 + neg));
		bc_parse_push(p, BC_INST_LSHIFT + neg);
	}
#endif // BC_ENABLE_EXTRA_MATH
}

void
bc_parse_text(BcParse* p, const char* text, BcMode mode)
{
	BC_SIG_LOCK;

	// Make sure the pointer isn't invalidated.
	p->func = bc_vec_item(&p->prog->fns, p->fidx);
	bc_lex_text(&p->l, text, mode);

	BC_SIG_UNLOCK;
}

void
bc_parse_reset(BcParse* p)
{
	BC_SIG_ASSERT_LOCKED;

	// Reset the function if it isn't main and switch to main.
	if (p->fidx != BC_PROG_MAIN)
	{
		bc_func_reset(p->func);
		bc_parse_updateFunc(p, BC_PROG_MAIN);
	}

	// Reset the lexer.
	p->l.i = p->l.len;
	p->l.t = BC_LEX_EOF;

#if BC_ENABLED
	if (BC_IS_BC)
	{
		// Get rid of the bc parser state.
		p->auto_part = false;
		bc_vec_npop(&p->flags, p->flags.len - 1);
		bc_vec_popAll(&p->exits);
		bc_vec_popAll(&p->conds);
		bc_vec_popAll(&p->ops);
	}
#endif // BC_ENABLED

	// Reset the program. This might clear the error.
	bc_program_reset(p->prog);

	// Jump if there is an error.
	if (BC_ERR(vm->status)) BC_JMP;
}

#ifndef NDEBUG
void
bc_parse_free(BcParse* p)
{
	BC_SIG_ASSERT_LOCKED;

	assert(p != NULL);

#if BC_ENABLED
	if (BC_IS_BC)
	{
		bc_vec_free(&p->flags);
		bc_vec_free(&p->exits);
		bc_vec_free(&p->conds);
		bc_vec_free(&p->ops);
		bc_vec_free(&p->buf);
	}
#endif // BC_ENABLED

	bc_lex_free(&p->l);
}
#endif // NDEBUG

void
bc_parse_init(BcParse* p, BcProgram* prog, size_t func)
{
#if BC_ENABLED
	uint16_t flag = 0;
#endif // BC_ENABLED

	BC_SIG_ASSERT_LOCKED;

	assert(p != NULL && prog != NULL);

#if BC_ENABLED
	if (BC_IS_BC)
	{
		// We always want at least one flag set on the flags stack.
		bc_vec_init(&p->flags, sizeof(uint16_t), BC_DTOR_NONE);
		bc_vec_push(&p->flags, &flag);

		bc_vec_init(&p->exits, sizeof(BcInstPtr), BC_DTOR_NONE);
		bc_vec_init(&p->conds, sizeof(size_t), BC_DTOR_NONE);
		bc_vec_init(&p->ops, sizeof(BcLexType), BC_DTOR_NONE);
		bc_vec_init(&p->buf, sizeof(char), BC_DTOR_NONE);

		p->auto_part = false;
	}
#endif // BC_ENABLED

	bc_lex_init(&p->l);

	// Set up the function.
	p->prog = prog;
	bc_parse_updateFunc(p, func);
}
