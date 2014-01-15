/*-
 * Copyright (c) 2011,2012 Kai Wang
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: ld_exp.h 2525 2012-07-17 17:36:19Z kaiwang27 $
 */

enum ld_exp_op {
	LEOP_ABS,
	LEOP_ADD,
	LEOP_ADDR,
	LEOP_ALIGN,
	LEOP_ALIGNOF,
	LEOP_AND,
	LEOP_ASSIGN,
	LEOP_BLOCK,
	LEOP_CONSTANT,
	LEOP_DIV,
	LEOP_DSA,
	LEOP_DSE,
	LEOP_DSRE,
	LEOP_DEFINED,
	LEOP_EQUAL,
	LEOP_GE,
	LEOP_GREATER,
	LEOP_LENGTH,
	LEOP_LE,
	LEOP_LESSER,
	LEOP_LOADADDR,
	LEOP_LOGICAL_AND,
	LEOP_LOGICAL_OR,
	LEOP_LSHIFT,
	LEOP_MAX,
	LEOP_MIN,
	LEOP_MINUS,
	LEOP_MOD,
	LEOP_MUL,
	LEOP_NE,
	LEOP_NEGATION,
	LEOP_NEXT,
	LEOP_NOT,
	LEOP_OR,
	LEOP_ORIGIN,
	LEOP_RSHIFT,
	LEOP_SEGMENT_START,
	LEOP_SIZEOF,
	LEOP_SIZEOF_HEADERS,
	LEOP_SECTION_NAME,
	LEOP_SUBSTRACT,
	LEOP_SYMBOL,
	LEOP_SYMBOLIC_CONSTANT,
	LEOP_TRINARY,
};

struct ld_exp {
	enum ld_exp_op le_op;	/* expression operator */
	struct ld_exp *le_e1;	/* fisrt operand */
	struct ld_exp *le_e2;	/* second operand */
	struct ld_exp *le_e3;	/* third operand */
	struct ld_script_assign *le_assign; /* assignment */
	char *le_name;		/* symbol/section name */
	unsigned le_par;	/* parenthesis */
	int64_t le_val;		/* constant value */
};

struct ld_exp *ld_exp_assign(struct ld *, struct ld_script_assign *);
struct ld_exp *ld_exp_binary(struct ld *, enum ld_exp_op, struct ld_exp *,
    struct ld_exp *);
struct ld_exp *ld_exp_constant(struct ld *, int64_t);
int64_t ld_exp_eval(struct ld *, struct ld_exp *);
void ld_exp_dump(struct ld *, struct ld_exp *);
struct ld_exp *ld_exp_name(struct ld *, const char *);
struct ld_exp *ld_exp_sizeof_headers(struct ld *);
struct ld_exp *ld_exp_symbol(struct ld *, const char *);
struct ld_exp *ld_exp_symbolic_constant(struct ld *, const char *);
struct ld_exp *ld_exp_trinary(struct ld *, struct ld_exp *, struct ld_exp *,
    struct ld_exp *);
struct ld_exp *ld_exp_unary(struct ld *, enum ld_exp_op, struct ld_exp *);
void ld_exp_free(struct ld_exp *);
