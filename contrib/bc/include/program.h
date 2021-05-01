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
 * Definitions for bc programs.
 *
 */

#ifndef BC_PROGRAM_H
#define BC_PROGRAM_H

#include <stddef.h>

#include <status.h>
#include <parse.h>
#include <lang.h>
#include <num.h>
#include <rand.h>

#define BC_PROG_GLOBALS_IBASE (0)
#define BC_PROG_GLOBALS_OBASE (1)
#define BC_PROG_GLOBALS_SCALE (2)

#if BC_ENABLE_EXTRA_MATH
#define BC_PROG_MAX_RAND (3)
#endif // BC_ENABLE_EXTRA_MATH

#define BC_PROG_GLOBALS_LEN (3 + BC_ENABLE_EXTRA_MATH)

#define BC_PROG_ONE_CAP (1)

typedef struct BcProgram {

	BcBigDig globals[BC_PROG_GLOBALS_LEN];
	BcVec globals_v[BC_PROG_GLOBALS_LEN];

#if BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
	BcRNG rng;
#endif // BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND

	BcVec results;
	BcVec stack;

	BcVec *consts;
	BcVec *strs;

	BcVec fns;
	BcVec fn_map;

	BcVec vars;
	BcVec var_map;

	BcVec arrs;
	BcVec arr_map;

#if DC_ENABLED
	BcVec strs_v;

	BcVec tail_calls;

	BcBigDig strm;
	BcNum strmb;
#endif // DC_ENABLED

	BcNum zero;
	BcNum one;

#if BC_ENABLED
	BcNum last;
#endif // BC_ENABLED

#if DC_ENABLED
	// This uses BC_NUM_LONG_LOG10 because it is used in bc_num_ulong2num(),
	// which attempts to realloc, unless it is big enough. This is big enough.
	BcDig strmb_num[BC_NUM_BIGDIG_LOG10];
#endif // DC_ENABLED

	BcDig zero_num[BC_PROG_ONE_CAP];
	BcDig one_num[BC_PROG_ONE_CAP];

} BcProgram;

#define BC_PROG_STACK(s, n) ((s)->len >= ((size_t) (n)))

#define BC_PROG_GLOBAL_PTR(v) (bc_vec_top(v))
#define BC_PROG_GLOBAL(v) (*((BcBigDig*) BC_PROG_GLOBAL_PTR(v)))

#define BC_PROG_IBASE(p) ((p)->globals[BC_PROG_GLOBALS_IBASE])
#define BC_PROG_OBASE(p) ((p)->globals[BC_PROG_GLOBALS_OBASE])
#define BC_PROG_SCALE(p) ((p)->globals[BC_PROG_GLOBALS_SCALE])

#define BC_PROG_MAIN (0)
#define BC_PROG_READ (1)

#define bc_program_retire(p, nres, nops) \
	(bc_vec_npopAt(&(p)->results, (nops), (p)->results.len - (nres + nops)))

#if DC_ENABLED
#define BC_PROG_REQ_FUNCS (2)
#if !BC_ENABLED
// For dc only, last is always true.
#define bc_program_copyToVar(p, name, t, last) \
	bc_program_copyToVar(p, name, t)
#endif // !BC_ENABLED
#else // DC_ENABLED
// For bc, 'pop' and 'copy' are always false.
#define bc_program_pushVar(p, code, bgn, pop, copy) \
	bc_program_pushVar(p, code, bgn)
#ifdef NDEBUG
#define BC_PROG_NO_STACK_CHECK
#endif // NDEBUG
#endif // DC_ENABLED

#define BC_PROG_STR(n) ((n)->num == NULL && !(n)->cap)
#if BC_ENABLED
#define BC_PROG_NUM(r, n) \
	((r)->t != BC_RESULT_ARRAY && (r)->t != BC_RESULT_STR && !BC_PROG_STR(n))
#else // BC_ENABLED
#define BC_PROG_NUM(r, n) ((r)->t != BC_RESULT_STR && !BC_PROG_STR(n))
// For dc, inst is always BC_INST_ARRAY_ELEM.
#define bc_program_pushArray(p, code, bgn, inst) \
	bc_program_pushArray(p, code, bgn)
#endif // BC_ENABLED

typedef void (*BcProgramUnary)(BcResult*, BcNum*);

void bc_program_init(BcProgram *p);
void bc_program_free(BcProgram *p);

#if BC_DEBUG_CODE
#if BC_ENABLED && DC_ENABLED
void bc_program_code(const BcProgram *p);
void bc_program_printInst(const BcProgram *p, const char *code,
                          size_t *restrict bgn);
void bc_program_printStackDebug(BcProgram* p);
#endif // BC_ENABLED && DC_ENABLED
#endif // BC_DEBUG_CODE

size_t bc_program_search(BcProgram *p, const char* id, bool var);
size_t bc_program_insertFunc(BcProgram *p, const char *name);
void bc_program_reset(BcProgram *p);
void bc_program_exec(BcProgram *p);

void bc_program_negate(BcResult *r, BcNum *n);
void bc_program_not(BcResult *r, BcNum *n);
#if BC_ENABLE_EXTRA_MATH
void bc_program_trunc(BcResult *r, BcNum *n);
#endif // BC_ENABLE_EXTRA_MATH

extern const BcNumBinaryOp bc_program_ops[];
extern const BcNumBinaryOpReq bc_program_opReqs[];
extern const BcProgramUnary bc_program_unarys[];
extern const char bc_program_exprs_name[];
extern const char bc_program_stdin_name[];
extern const char bc_program_ready_msg[];
extern const size_t bc_program_ready_msg_len;
extern const char bc_program_esc_chars[];
extern const char bc_program_esc_seqs[];

#endif // BC_PROGRAM_H
