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
 * Definitions for dc only.
 *
 */

#ifndef BC_DC_H
#define BC_DC_H

#if DC_ENABLED

#include <status.h>
#include <lex.h>
#include <parse.h>

/**
 * The main function for dc. It just sets variables and passes its arguments
 * through to @a bc_vm_boot().
 */
void
dc_main(int argc, char* argv[]);

// A reference to the dc help text.
extern const char dc_help[];

/**
 * The @a BcLexNext function for dc. (See include/lex.h for a definition of
 * @a BcLexNext.)
 * @param l  The lexer.
 */
void
dc_lex_token(BcLex* l);

/**
 * Returns true if the negative char `_` should be treated as a command or not.
 * dc considers negative a command if it does *not* immediately proceed a
 * number. Otherwise, it's just considered a negative.
 * @param l  The lexer.
 * @return   True if a negative should be treated as a command, false if it
 *           should be treated as a negative sign on a number.
 */
bool
dc_lex_negCommand(BcLex* l);

// References to the signal message and its length.
extern const char dc_sig_msg[];
extern const uchar dc_sig_msg_len;

// References to an array and its length. This array is an array of lex tokens
// that, when encountered, should be treated as commands that take a register.
extern const uint8_t dc_lex_regs[];
extern const size_t dc_lex_regs_len;

// References to an array of tokens and its length. This array corresponds to
// the ASCII table, starting at double quotes. This makes it easy to look up
// tokens for characters.
extern const uint8_t dc_lex_tokens[];
extern const uint8_t dc_parse_insts[];

/**
 * The @a BcParseParse function for dc. (See include/parse.h for a definition of
 * @a BcParseParse.)
 * @param p  The parser.
 */
void
dc_parse_parse(BcParse* p);

/**
 * The @a BcParseExpr function for dc. (See include/parse.h for a definition of
 * @a BcParseExpr.)
 * @param p      The parser.
 * @param flags  Flags that define the requirements that the parsed code must
 *               meet or an error will result. See @a BcParseExpr for more info.
 */
void
dc_parse_expr(BcParse* p, uint8_t flags);

#endif // DC_ENABLED

#endif // BC_DC_H
