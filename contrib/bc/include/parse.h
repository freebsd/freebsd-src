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
 * Definitions for bc's parser.
 *
 */

#ifndef BC_PARSE_H
#define BC_PARSE_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include <status.h>
#include <vector.h>
#include <lex.h>
#include <lang.h>

#define BC_PARSE_REL (UINTMAX_C(1)<<0)
#define BC_PARSE_PRINT (UINTMAX_C(1)<<1)
#define BC_PARSE_NOCALL (UINTMAX_C(1)<<2)
#define BC_PARSE_NOREAD (UINTMAX_C(1)<<3)
#define BC_PARSE_ARRAY (UINTMAX_C(1)<<4)
#define BC_PARSE_NEEDVAL (UINTMAX_C(1)<<5)

#if BC_ENABLED
#define BC_PARSE_CAN_PARSE(p) \
	((p).l.t != BC_LEX_EOF && (p).l.t != BC_LEX_KW_DEFINE)
#else // BC_ENABLED
#define BC_PARSE_CAN_PARSE(p) ((p).l.t != BC_LEX_EOF)
#endif // BC_ENABLED

#define bc_parse_push(p, i) (bc_vec_pushByte(&(p)->func->code, (uchar) (i)))
#define bc_parse_pushIndex(p, idx) (bc_vec_pushIndex(&(p)->func->code, (idx)))

#define bc_parse_err(p, e) (bc_vm_handleError((e), (p)->l.line))
#define bc_parse_verr(p, e, ...) \
	(bc_vm_handleError((e), (p)->l.line, __VA_ARGS__))

typedef struct BcParseNext {
	uchar len;
	uchar tokens[4];
} BcParseNext;

#define BC_PARSE_NEXT_TOKENS(...) .tokens = { __VA_ARGS__ }
#define BC_PARSE_NEXT(a, ...) \
	{ .len = (uchar) (a), BC_PARSE_NEXT_TOKENS(__VA_ARGS__) }

struct BcParse;
struct BcProgram;

typedef void (*BcParseParse)(struct BcParse*);
typedef void (*BcParseExpr)(struct BcParse*, uint8_t);

typedef struct BcParse {

	BcLex l;

#if BC_ENABLED
	BcVec flags;
	BcVec exits;
	BcVec conds;
	BcVec ops;
	BcVec buf;
#endif // BC_ENABLED

	struct BcProgram *prog;
	BcFunc *func;
	size_t fidx;

	bool auto_part;

} BcParse;

void bc_parse_init(BcParse *p, struct BcProgram *prog, size_t func);
void bc_parse_free(BcParse *p);
void bc_parse_reset(BcParse *p);

void bc_parse_addString(BcParse *p);
void bc_parse_number(BcParse *p);
void bc_parse_updateFunc(BcParse *p, size_t fidx);
void bc_parse_pushName(const BcParse* p, char *name, bool var);
void bc_parse_text(BcParse *p, const char *text);

extern const char bc_parse_zero[2];
extern const char bc_parse_one[2];

#endif // BC_PARSE_H
