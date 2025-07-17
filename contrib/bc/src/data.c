/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2024 Gavin D. Howard and contributors.
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
 * Constant data for bc.
 *
 */

#include <assert.h>

#include <opt.h>
#include <args.h>
#include <lex.h>
#include <parse.h>
#include <bc.h>
#include <dc.h>
#include <num.h>
#include <rand.h>
#include <program.h>
#include <history.h>
#include <library.h>
#include <vm.h>

#if !BC_ENABLE_LIBRARY

#if BC_ENABLED

/// The bc signal message and its length.
const char bc_sig_msg[] = "\ninterrupt (type \"quit\" to exit)\n";
const uchar bc_sig_msg_len = (uchar) (sizeof(bc_sig_msg) - 1);

#endif // BC_ENABLED

#if DC_ENABLED

/// The dc signal message and its length.
const char dc_sig_msg[] = "\ninterrupt (type \"q\" to exit)\n";
const uchar dc_sig_msg_len = (uchar) (sizeof(dc_sig_msg) - 1);

#endif // DC_ENABLED

// clang-format off

/// The copyright banner.
const char bc_copyright[] =
	"Copyright (c) 2018-2024 Gavin D. Howard and contributors\n"
	"Report bugs at: https://git.gavinhoward.com/gavin/bc\n\n"
	"This is free software with ABSOLUTELY NO WARRANTY.\n";

// clang-format on

#ifdef __OpenBSD__

#if BC_ENABLE_EXTRA_MATH

#if BC_ENABLE_HISTORY

/// The pledges for starting bc.
const char bc_pledge_start[] = "rpath stdio tty unveil";

/// The final pledges with history enabled.
const char bc_pledge_end_history[] = "rpath stdio tty";

#else // BC_ENABLE_HISTORY

/// The pledges for starting bc.
const char bc_pledge_start[] = "rpath stdio unveil";

#endif // BC_ENABLE_HISTORY

/// The final pledges with history history disabled.
const char bc_pledge_end[] = "rpath stdio";

#else // BC_ENABLE_EXTRA_MATH

#if BC_ENABLE_HISTORY

/// The pledges for starting bc.
const char bc_pledge_start[] = "rpath stdio tty";

/// The final pledges with history enabled.
const char bc_pledge_end_history[] = "stdio tty";

#else // BC_ENABLE_HISTORY

/// The pledges for starting bc.
const char bc_pledge_start[] = "rpath stdio";

#endif // BC_ENABLE_HISTORY

/// The final pledges with history history disabled.
const char bc_pledge_end[] = "stdio";

#endif // BC_ENABLE_EXTRA_MATH

#else // __OpenBSD__

/// The pledges for starting bc.
const char bc_pledge_start[] = "";

#if BC_ENABLE_HISTORY

/// The final pledges with history enabled.
const char bc_pledge_end_history[] = "";

#endif // BC_ENABLE_HISTORY

/// The final pledges with history history disabled.
const char bc_pledge_end[] = "";

#endif // __OpenBSD__

/// The list of long options. There is a zero set at the end for detecting the
/// end.
const BcOptLong bc_args_lopt[] = {

	{ "digit-clamp", BC_OPT_NONE, 'c' },
	{ "expression", BC_OPT_REQUIRED, 'e' },
	{ "file", BC_OPT_REQUIRED, 'f' },
	{ "help", BC_OPT_NONE, 'h' },
	{ "interactive", BC_OPT_NONE, 'i' },
	{ "ibase", BC_OPT_REQUIRED, 'I' },
	{ "leading-zeroes", BC_OPT_NONE, 'z' },
	{ "no-line-length", BC_OPT_NONE, 'L' },
	{ "obase", BC_OPT_REQUIRED, 'O' },
	{ "no-digit-clamp", BC_OPT_NONE, 'C' },
	{ "no-prompt", BC_OPT_NONE, 'P' },
	{ "no-read-prompt", BC_OPT_NONE, 'R' },
	{ "scale", BC_OPT_REQUIRED, 'S' },
#if BC_ENABLE_EXTRA_MATH
	{ "seed", BC_OPT_REQUIRED, 'E' },
#endif // BC_ENABLE_EXTRA_MATH
#if BC_ENABLED
	{ "global-stacks", BC_OPT_BC_ONLY, 'g' },
	{ "mathlib", BC_OPT_BC_ONLY, 'l' },
	{ "quiet", BC_OPT_BC_ONLY, 'q' },
	{ "redefine", BC_OPT_REQUIRED_BC_ONLY, 'r' },
	{ "standard", BC_OPT_BC_ONLY, 's' },
	{ "warn", BC_OPT_BC_ONLY, 'w' },
#endif // BC_ENABLED
	{ "version", BC_OPT_NONE, 'v' },
	{ "version", BC_OPT_NONE, 'V' },
#if DC_ENABLED
	{ "extended-register", BC_OPT_DC_ONLY, 'x' },
#endif // DC_ENABLED
	{ NULL, 0, 0 },

};

#if BC_ENABLE_OSSFUZZ

const char* bc_fuzzer_args_c[] = {
	"bc",
	"-lqc",
	"-e",
	"seed = 82507683022933941343198991100880559238.7080266844215897551270760113"
	"4734858017748592704189096562163085637164174146616055338762825421827784"
	"566630725748836994171142578125",
	NULL,
};

const char* dc_fuzzer_args_c[] = {
	"dc",
	"-xc",
	"-e",
	"82507683022933941343198991100880559238.7080266844215897551270760113"
	"4734858017748592704189096562163085637164174146616055338762825421827784"
	"566630725748836994171142578125j",
	NULL,
};

const char* bc_fuzzer_args_C[] = {
	"bc",
	"-lqC",
	"-e",
	"seed = 82507683022933941343198991100880559238.7080266844215897551270760113"
	"4734858017748592704189096562163085637164174146616055338762825421827784"
	"566630725748836994171142578125",
	NULL,
};

const char* dc_fuzzer_args_C[] = {
	"dc",
	"-xC",
	"-e",
	"82507683022933941343198991100880559238.7080266844215897551270760113"
	"4734858017748592704189096562163085637164174146616055338762825421827784"
	"566630725748836994171142578125j",
	NULL,
};

const size_t bc_fuzzer_args_len = sizeof(bc_fuzzer_args_c) / sizeof(char*);

#if BC_C11

_Static_assert(sizeof(bc_fuzzer_args_C) / sizeof(char*) == bc_fuzzer_args_len,
               "Wrong number of bc fuzzer args");

_Static_assert(sizeof(dc_fuzzer_args_c) / sizeof(char*) == bc_fuzzer_args_len,
               "Wrong number of dc fuzzer args");

_Static_assert(sizeof(dc_fuzzer_args_C) / sizeof(char*) == bc_fuzzer_args_len,
               "Wrong number of dc fuzzer args");

#endif // BC_C11

#endif // BC_ENABLE_OSSFUZZ

// clang-format off

/// The default error category strings.
const char *bc_errs[] = {
	"Math error:",
	"Parse error:",
	"Runtime error:",
	"Fatal error:",
#if BC_ENABLED
	"Warning:",
#endif // BC_ENABLED
};

// clang-format on

/// The error category for each error.
const uchar bc_err_ids[] = {

	BC_ERR_IDX_MATH,  BC_ERR_IDX_MATH,  BC_ERR_IDX_MATH,  BC_ERR_IDX_MATH,

	BC_ERR_IDX_FATAL, BC_ERR_IDX_FATAL, BC_ERR_IDX_FATAL, BC_ERR_IDX_FATAL,
	BC_ERR_IDX_FATAL, BC_ERR_IDX_FATAL, BC_ERR_IDX_FATAL, BC_ERR_IDX_FATAL,
	BC_ERR_IDX_FATAL,

	BC_ERR_IDX_EXEC,  BC_ERR_IDX_EXEC,  BC_ERR_IDX_EXEC,  BC_ERR_IDX_EXEC,
	BC_ERR_IDX_EXEC,  BC_ERR_IDX_EXEC,  BC_ERR_IDX_EXEC,  BC_ERR_IDX_EXEC,
	BC_ERR_IDX_EXEC,  BC_ERR_IDX_EXEC,  BC_ERR_IDX_EXEC,

	BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE,
	BC_ERR_IDX_PARSE,
#if BC_ENABLED
	BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE,
	BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE,
	BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE,

	BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE,
	BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE,
	BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE,
	BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE, BC_ERR_IDX_PARSE,
#endif // BC_ENABLED

};

/// The default error messages. There are NULL pointers because the positions
/// must be preserved for the locales.
const char* const bc_err_msgs[] = {

	"negative number",
	"non-integer number",
	"overflow: number cannot fit",
	"divide by 0",

	"memory allocation failed",
	"I/O error",
	"cannot open file: %s",
	"file is not text: %s",
	"path is a directory: %s",
	"bad command-line option: \"%s\"",
	"option requires an argument: '%c' (\"%s\")",
	"option takes no arguments: '%c' (\"%s\")",
	"bad option argument: \"%s\"",

	"bad ibase: must be [%lu, %lu]",
	"bad obase: must be [%lu, %lu]",
	"bad scale: must be [%lu, %lu]",
	"bad read() expression",
	"read() call inside of a read() call",
	"variable or array element is the wrong type",
#if DC_ENABLED
	"stack has too few elements",
	"stack for register \"%s\" has too few elements",
#else // DC_ENABLED
	NULL,
	NULL,
#endif // DC_ENABLED
#if BC_ENABLED
	"wrong number of parameters; need %zu, have %zu",
	"undefined function: %s()",
	"cannot use a void value in an expression",
#else
	NULL,
	NULL,
	NULL,
#endif // BC_ENABLED

	"end of file",
	"bad character '%c'",
	"string end cannot be found",
	"comment end cannot be found",
	"bad token",
#if BC_ENABLED
	"bad expression",
	"empty expression",
	"bad print or stream statement",
	"bad function definition",
	("bad assignment: left side must be scale, ibase, "
	 "obase, seed, last, var, or array element"),
	"no auto variable found",
	"function parameter or auto \"%s%s\" already exists",
	"block end cannot be found",
	"cannot return a value from void function: %s()",
	"var cannot be a reference: %s",

	"POSIX does not allow names longer than 1 character: %s",
	"POSIX does not allow '#' script comments",
	"POSIX does not allow the following keyword: %s",
	"POSIX does not allow a period ('.') as a shortcut for the last result",
	"POSIX requires parentheses around return expressions",
	"POSIX does not allow the following operator: %s",
	"POSIX does not allow comparison operators outside if statements or loops",
	"POSIX requires 0 or 1 comparison operators per condition",
	"POSIX requires all 3 parts of a for loop to be non-empty",
	"POSIX requires a newline between a semicolon and a function definition",
#if BC_ENABLE_EXTRA_MATH
	"POSIX does not allow exponential notation",
#else
	NULL,
#endif // BC_ENABLE_EXTRA_MATH
	"POSIX does not allow array references as function parameters",
	"POSIX does not allow void functions",
	"POSIX requires the left brace be on the same line as the function header",
	"POSIX does not allow strings to be assigned to variables or arrays",
#endif // BC_ENABLED

};

#endif // !BC_ENABLE_LIBRARY

/// The destructors corresponding to BcDtorType enum items.
const BcVecFree bc_vec_dtors[] = {
	NULL,
	bc_vec_free,
	bc_num_free,
#if !BC_ENABLE_LIBRARY
#if BC_DEBUG
	bc_func_free,
#endif // BC_DEBUG
	bc_slab_free,
	bc_const_free,
	bc_result_free,
#if BC_ENABLE_HISTORY && !BC_ENABLE_LINE_LIB
	bc_history_string_free,
#endif // BC_ENABLE_HISTORY && !BC_ENABLE_LINE_LIB
#else // !BC_ENABLE_LIBRARY
	bcl_num_destruct,
#endif // !BC_ENABLE_LIBRARY
};

#if !BC_ENABLE_LIBRARY

#if BC_ENABLE_EDITLINE

/// The normal path to the editrc.
const char bc_history_editrc[] = "/.editrc";

/// The length of the normal path to the editrc.
const size_t bc_history_editrc_len = sizeof(bc_history_editrc) - 1;

#endif // BC_ENABLE_EDITLINE

#if BC_ENABLE_HISTORY && !BC_ENABLE_LINE_LIB

/// A flush type for not clearing current extras but not saving new ones either.
const BcFlushType bc_flush_none = BC_FLUSH_NO_EXTRAS_NO_CLEAR;

/// A flush type for clearing extras and not saving new ones.
const BcFlushType bc_flush_err = BC_FLUSH_NO_EXTRAS_CLEAR;

/// A flush type for clearing previous extras and saving new ones.
const BcFlushType bc_flush_save = BC_FLUSH_SAVE_EXTRAS_CLEAR;

/// A list of known bad terminals.
const char* bc_history_bad_terms[] = { "dumb", "cons25", "emacs", NULL };

/// A constant for tabs and its length. My tab handling is dumb and always
/// outputs the entire thing.
const char bc_history_tab[] = "\t";
const size_t bc_history_tab_len = sizeof(bc_history_tab) - 1;

/// A list of wide chars. These are listed in ascending order for efficiency.
const uint32_t bc_history_wchars[][2] = {
	{ 0x1100, 0x115F },   { 0x231A, 0x231B },   { 0x2329, 0x232A },
	{ 0x23E9, 0x23EC },   { 0x23F0, 0x23F0 },   { 0x23F3, 0x23F3 },
	{ 0x25FD, 0x25FE },   { 0x2614, 0x2615 },   { 0x2648, 0x2653 },
	{ 0x267F, 0x267F },   { 0x2693, 0x2693 },   { 0x26A1, 0x26A1 },
	{ 0x26AA, 0x26AB },   { 0x26BD, 0x26BE },   { 0x26C4, 0x26C5 },
	{ 0x26CE, 0x26CE },   { 0x26D4, 0x26D4 },   { 0x26EA, 0x26EA },
	{ 0x26F2, 0x26F3 },   { 0x26F5, 0x26F5 },   { 0x26FA, 0x26FA },
	{ 0x26FD, 0x26FD },   { 0x2705, 0x2705 },   { 0x270A, 0x270B },
	{ 0x2728, 0x2728 },   { 0x274C, 0x274C },   { 0x274E, 0x274E },
	{ 0x2753, 0x2755 },   { 0x2757, 0x2757 },   { 0x2795, 0x2797 },
	{ 0x27B0, 0x27B0 },   { 0x27BF, 0x27BF },   { 0x2B1B, 0x2B1C },
	{ 0x2B50, 0x2B50 },   { 0x2B55, 0x2B55 },   { 0x2E80, 0x2E99 },
	{ 0x2E9B, 0x2EF3 },   { 0x2F00, 0x2FD5 },   { 0x2FF0, 0x2FFB },
	{ 0x3001, 0x303E },   { 0x3041, 0x3096 },   { 0x3099, 0x30FF },
	{ 0x3105, 0x312D },   { 0x3131, 0x318E },   { 0x3190, 0x31BA },
	{ 0x31C0, 0x31E3 },   { 0x31F0, 0x321E },   { 0x3220, 0x3247 },
	{ 0x3250, 0x32FE },   { 0x3300, 0x4DBF },   { 0x4E00, 0xA48C },
	{ 0xA490, 0xA4C6 },   { 0xA960, 0xA97C },   { 0xAC00, 0xD7A3 },
	{ 0xF900, 0xFAFF },   { 0xFE10, 0xFE19 },   { 0xFE30, 0xFE52 },
	{ 0xFE54, 0xFE66 },   { 0xFE68, 0xFE6B },   { 0x16FE0, 0x16FE0 },
	{ 0x17000, 0x187EC }, { 0x18800, 0x18AF2 }, { 0x1B000, 0x1B001 },
	{ 0x1F004, 0x1F004 }, { 0x1F0CF, 0x1F0CF }, { 0x1F18E, 0x1F18E },
	{ 0x1F191, 0x1F19A }, { 0x1F200, 0x1F202 }, { 0x1F210, 0x1F23B },
	{ 0x1F240, 0x1F248 }, { 0x1F250, 0x1F251 }, { 0x1F300, 0x1F320 },
	{ 0x1F32D, 0x1F335 }, { 0x1F337, 0x1F37C }, { 0x1F37E, 0x1F393 },
	{ 0x1F3A0, 0x1F3CA }, { 0x1F3CF, 0x1F3D3 }, { 0x1F3E0, 0x1F3F0 },
	{ 0x1F3F4, 0x1F3F4 }, { 0x1F3F8, 0x1F43E }, { 0x1F440, 0x1F440 },
	{ 0x1F442, 0x1F4FC }, { 0x1F4FF, 0x1F53D }, { 0x1F54B, 0x1F54E },
	{ 0x1F550, 0x1F567 }, { 0x1F57A, 0x1F57A }, { 0x1F595, 0x1F596 },
	{ 0x1F5A4, 0x1F5A4 }, { 0x1F5FB, 0x1F64F }, { 0x1F680, 0x1F6C5 },
	{ 0x1F6CC, 0x1F6CC }, { 0x1F6D0, 0x1F6D2 }, { 0x1F6EB, 0x1F6EC },
	{ 0x1F6F4, 0x1F6F6 }, { 0x1F910, 0x1F91E }, { 0x1F920, 0x1F927 },
	{ 0x1F930, 0x1F930 }, { 0x1F933, 0x1F93E }, { 0x1F940, 0x1F94B },
	{ 0x1F950, 0x1F95E }, { 0x1F980, 0x1F991 }, { 0x1F9C0, 0x1F9C0 },
	{ 0x20000, 0x2FFFD }, { 0x30000, 0x3FFFD },
};

/// The length of the wide chars list.
const size_t bc_history_wchars_len = sizeof(bc_history_wchars) /
                                     sizeof(bc_history_wchars[0]);

/// A list of combining characters in Unicode. These are listed in ascending
/// order for efficiency.
const uint32_t bc_history_combo_chars[] = {
	0x0300,  0x0301,  0x0302,  0x0303,  0x0304,  0x0305,  0x0306,  0x0307,
	0x0308,  0x0309,  0x030A,  0x030B,  0x030C,  0x030D,  0x030E,  0x030F,
	0x0310,  0x0311,  0x0312,  0x0313,  0x0314,  0x0315,  0x0316,  0x0317,
	0x0318,  0x0319,  0x031A,  0x031B,  0x031C,  0x031D,  0x031E,  0x031F,
	0x0320,  0x0321,  0x0322,  0x0323,  0x0324,  0x0325,  0x0326,  0x0327,
	0x0328,  0x0329,  0x032A,  0x032B,  0x032C,  0x032D,  0x032E,  0x032F,
	0x0330,  0x0331,  0x0332,  0x0333,  0x0334,  0x0335,  0x0336,  0x0337,
	0x0338,  0x0339,  0x033A,  0x033B,  0x033C,  0x033D,  0x033E,  0x033F,
	0x0340,  0x0341,  0x0342,  0x0343,  0x0344,  0x0345,  0x0346,  0x0347,
	0x0348,  0x0349,  0x034A,  0x034B,  0x034C,  0x034D,  0x034E,  0x034F,
	0x0350,  0x0351,  0x0352,  0x0353,  0x0354,  0x0355,  0x0356,  0x0357,
	0x0358,  0x0359,  0x035A,  0x035B,  0x035C,  0x035D,  0x035E,  0x035F,
	0x0360,  0x0361,  0x0362,  0x0363,  0x0364,  0x0365,  0x0366,  0x0367,
	0x0368,  0x0369,  0x036A,  0x036B,  0x036C,  0x036D,  0x036E,  0x036F,
	0x0483,  0x0484,  0x0485,  0x0486,  0x0487,  0x0591,  0x0592,  0x0593,
	0x0594,  0x0595,  0x0596,  0x0597,  0x0598,  0x0599,  0x059A,  0x059B,
	0x059C,  0x059D,  0x059E,  0x059F,  0x05A0,  0x05A1,  0x05A2,  0x05A3,
	0x05A4,  0x05A5,  0x05A6,  0x05A7,  0x05A8,  0x05A9,  0x05AA,  0x05AB,
	0x05AC,  0x05AD,  0x05AE,  0x05AF,  0x05B0,  0x05B1,  0x05B2,  0x05B3,
	0x05B4,  0x05B5,  0x05B6,  0x05B7,  0x05B8,  0x05B9,  0x05BA,  0x05BB,
	0x05BC,  0x05BD,  0x05BF,  0x05C1,  0x05C2,  0x05C4,  0x05C5,  0x05C7,
	0x0610,  0x0611,  0x0612,  0x0613,  0x0614,  0x0615,  0x0616,  0x0617,
	0x0618,  0x0619,  0x061A,  0x064B,  0x064C,  0x064D,  0x064E,  0x064F,
	0x0650,  0x0651,  0x0652,  0x0653,  0x0654,  0x0655,  0x0656,  0x0657,
	0x0658,  0x0659,  0x065A,  0x065B,  0x065C,  0x065D,  0x065E,  0x065F,
	0x0670,  0x06D6,  0x06D7,  0x06D8,  0x06D9,  0x06DA,  0x06DB,  0x06DC,
	0x06DF,  0x06E0,  0x06E1,  0x06E2,  0x06E3,  0x06E4,  0x06E7,  0x06E8,
	0x06EA,  0x06EB,  0x06EC,  0x06ED,  0x0711,  0x0730,  0x0731,  0x0732,
	0x0733,  0x0734,  0x0735,  0x0736,  0x0737,  0x0738,  0x0739,  0x073A,
	0x073B,  0x073C,  0x073D,  0x073E,  0x073F,  0x0740,  0x0741,  0x0742,
	0x0743,  0x0744,  0x0745,  0x0746,  0x0747,  0x0748,  0x0749,  0x074A,
	0x07A6,  0x07A7,  0x07A8,  0x07A9,  0x07AA,  0x07AB,  0x07AC,  0x07AD,
	0x07AE,  0x07AF,  0x07B0,  0x07EB,  0x07EC,  0x07ED,  0x07EE,  0x07EF,
	0x07F0,  0x07F1,  0x07F2,  0x07F3,  0x0816,  0x0817,  0x0818,  0x0819,
	0x081B,  0x081C,  0x081D,  0x081E,  0x081F,  0x0820,  0x0821,  0x0822,
	0x0823,  0x0825,  0x0826,  0x0827,  0x0829,  0x082A,  0x082B,  0x082C,
	0x082D,  0x0859,  0x085A,  0x085B,  0x08D4,  0x08D5,  0x08D6,  0x08D7,
	0x08D8,  0x08D9,  0x08DA,  0x08DB,  0x08DC,  0x08DD,  0x08DE,  0x08DF,
	0x08E0,  0x08E1,  0x08E3,  0x08E4,  0x08E5,  0x08E6,  0x08E7,  0x08E8,
	0x08E9,  0x08EA,  0x08EB,  0x08EC,  0x08ED,  0x08EE,  0x08EF,  0x08F0,
	0x08F1,  0x08F2,  0x08F3,  0x08F4,  0x08F5,  0x08F6,  0x08F7,  0x08F8,
	0x08F9,  0x08FA,  0x08FB,  0x08FC,  0x08FD,  0x08FE,  0x08FF,  0x0900,
	0x0901,  0x0902,  0x093A,  0x093C,  0x0941,  0x0942,  0x0943,  0x0944,
	0x0945,  0x0946,  0x0947,  0x0948,  0x094D,  0x0951,  0x0952,  0x0953,
	0x0954,  0x0955,  0x0956,  0x0957,  0x0962,  0x0963,  0x0981,  0x09BC,
	0x09C1,  0x09C2,  0x09C3,  0x09C4,  0x09CD,  0x09E2,  0x09E3,  0x0A01,
	0x0A02,  0x0A3C,  0x0A41,  0x0A42,  0x0A47,  0x0A48,  0x0A4B,  0x0A4C,
	0x0A4D,  0x0A51,  0x0A70,  0x0A71,  0x0A75,  0x0A81,  0x0A82,  0x0ABC,
	0x0AC1,  0x0AC2,  0x0AC3,  0x0AC4,  0x0AC5,  0x0AC7,  0x0AC8,  0x0ACD,
	0x0AE2,  0x0AE3,  0x0B01,  0x0B3C,  0x0B3F,  0x0B41,  0x0B42,  0x0B43,
	0x0B44,  0x0B4D,  0x0B56,  0x0B62,  0x0B63,  0x0B82,  0x0BC0,  0x0BCD,
	0x0C00,  0x0C3E,  0x0C3F,  0x0C40,  0x0C46,  0x0C47,  0x0C48,  0x0C4A,
	0x0C4B,  0x0C4C,  0x0C4D,  0x0C55,  0x0C56,  0x0C62,  0x0C63,  0x0C81,
	0x0CBC,  0x0CBF,  0x0CC6,  0x0CCC,  0x0CCD,  0x0CE2,  0x0CE3,  0x0D01,
	0x0D41,  0x0D42,  0x0D43,  0x0D44,  0x0D4D,  0x0D62,  0x0D63,  0x0DCA,
	0x0DD2,  0x0DD3,  0x0DD4,  0x0DD6,  0x0E31,  0x0E34,  0x0E35,  0x0E36,
	0x0E37,  0x0E38,  0x0E39,  0x0E3A,  0x0E47,  0x0E48,  0x0E49,  0x0E4A,
	0x0E4B,  0x0E4C,  0x0E4D,  0x0E4E,  0x0EB1,  0x0EB4,  0x0EB5,  0x0EB6,
	0x0EB7,  0x0EB8,  0x0EB9,  0x0EBB,  0x0EBC,  0x0EC8,  0x0EC9,  0x0ECA,
	0x0ECB,  0x0ECC,  0x0ECD,  0x0F18,  0x0F19,  0x0F35,  0x0F37,  0x0F39,
	0x0F71,  0x0F72,  0x0F73,  0x0F74,  0x0F75,  0x0F76,  0x0F77,  0x0F78,
	0x0F79,  0x0F7A,  0x0F7B,  0x0F7C,  0x0F7D,  0x0F7E,  0x0F80,  0x0F81,
	0x0F82,  0x0F83,  0x0F84,  0x0F86,  0x0F87,  0x0F8D,  0x0F8E,  0x0F8F,
	0x0F90,  0x0F91,  0x0F92,  0x0F93,  0x0F94,  0x0F95,  0x0F96,  0x0F97,
	0x0F99,  0x0F9A,  0x0F9B,  0x0F9C,  0x0F9D,  0x0F9E,  0x0F9F,  0x0FA0,
	0x0FA1,  0x0FA2,  0x0FA3,  0x0FA4,  0x0FA5,  0x0FA6,  0x0FA7,  0x0FA8,
	0x0FA9,  0x0FAA,  0x0FAB,  0x0FAC,  0x0FAD,  0x0FAE,  0x0FAF,  0x0FB0,
	0x0FB1,  0x0FB2,  0x0FB3,  0x0FB4,  0x0FB5,  0x0FB6,  0x0FB7,  0x0FB8,
	0x0FB9,  0x0FBA,  0x0FBB,  0x0FBC,  0x0FC6,  0x102D,  0x102E,  0x102F,
	0x1030,  0x1032,  0x1033,  0x1034,  0x1035,  0x1036,  0x1037,  0x1039,
	0x103A,  0x103D,  0x103E,  0x1058,  0x1059,  0x105E,  0x105F,  0x1060,
	0x1071,  0x1072,  0x1073,  0x1074,  0x1082,  0x1085,  0x1086,  0x108D,
	0x109D,  0x135D,  0x135E,  0x135F,  0x1712,  0x1713,  0x1714,  0x1732,
	0x1733,  0x1734,  0x1752,  0x1753,  0x1772,  0x1773,  0x17B4,  0x17B5,
	0x17B7,  0x17B8,  0x17B9,  0x17BA,  0x17BB,  0x17BC,  0x17BD,  0x17C6,
	0x17C9,  0x17CA,  0x17CB,  0x17CC,  0x17CD,  0x17CE,  0x17CF,  0x17D0,
	0x17D1,  0x17D2,  0x17D3,  0x17DD,  0x180B,  0x180C,  0x180D,  0x1885,
	0x1886,  0x18A9,  0x1920,  0x1921,  0x1922,  0x1927,  0x1928,  0x1932,
	0x1939,  0x193A,  0x193B,  0x1A17,  0x1A18,  0x1A1B,  0x1A56,  0x1A58,
	0x1A59,  0x1A5A,  0x1A5B,  0x1A5C,  0x1A5D,  0x1A5E,  0x1A60,  0x1A62,
	0x1A65,  0x1A66,  0x1A67,  0x1A68,  0x1A69,  0x1A6A,  0x1A6B,  0x1A6C,
	0x1A73,  0x1A74,  0x1A75,  0x1A76,  0x1A77,  0x1A78,  0x1A79,  0x1A7A,
	0x1A7B,  0x1A7C,  0x1A7F,  0x1AB0,  0x1AB1,  0x1AB2,  0x1AB3,  0x1AB4,
	0x1AB5,  0x1AB6,  0x1AB7,  0x1AB8,  0x1AB9,  0x1ABA,  0x1ABB,  0x1ABC,
	0x1ABD,  0x1B00,  0x1B01,  0x1B02,  0x1B03,  0x1B34,  0x1B36,  0x1B37,
	0x1B38,  0x1B39,  0x1B3A,  0x1B3C,  0x1B42,  0x1B6B,  0x1B6C,  0x1B6D,
	0x1B6E,  0x1B6F,  0x1B70,  0x1B71,  0x1B72,  0x1B73,  0x1B80,  0x1B81,
	0x1BA2,  0x1BA3,  0x1BA4,  0x1BA5,  0x1BA8,  0x1BA9,  0x1BAB,  0x1BAC,
	0x1BAD,  0x1BE6,  0x1BE8,  0x1BE9,  0x1BED,  0x1BEF,  0x1BF0,  0x1BF1,
	0x1C2C,  0x1C2D,  0x1C2E,  0x1C2F,  0x1C30,  0x1C31,  0x1C32,  0x1C33,
	0x1C36,  0x1C37,  0x1CD0,  0x1CD1,  0x1CD2,  0x1CD4,  0x1CD5,  0x1CD6,
	0x1CD7,  0x1CD8,  0x1CD9,  0x1CDA,  0x1CDB,  0x1CDC,  0x1CDD,  0x1CDE,
	0x1CDF,  0x1CE0,  0x1CE2,  0x1CE3,  0x1CE4,  0x1CE5,  0x1CE6,  0x1CE7,
	0x1CE8,  0x1CED,  0x1CF4,  0x1CF8,  0x1CF9,  0x1DC0,  0x1DC1,  0x1DC2,
	0x1DC3,  0x1DC4,  0x1DC5,  0x1DC6,  0x1DC7,  0x1DC8,  0x1DC9,  0x1DCA,
	0x1DCB,  0x1DCC,  0x1DCD,  0x1DCE,  0x1DCF,  0x1DD0,  0x1DD1,  0x1DD2,
	0x1DD3,  0x1DD4,  0x1DD5,  0x1DD6,  0x1DD7,  0x1DD8,  0x1DD9,  0x1DDA,
	0x1DDB,  0x1DDC,  0x1DDD,  0x1DDE,  0x1DDF,  0x1DE0,  0x1DE1,  0x1DE2,
	0x1DE3,  0x1DE4,  0x1DE5,  0x1DE6,  0x1DE7,  0x1DE8,  0x1DE9,  0x1DEA,
	0x1DEB,  0x1DEC,  0x1DED,  0x1DEE,  0x1DEF,  0x1DF0,  0x1DF1,  0x1DF2,
	0x1DF3,  0x1DF4,  0x1DF5,  0x1DFB,  0x1DFC,  0x1DFD,  0x1DFE,  0x1DFF,
	0x20D0,  0x20D1,  0x20D2,  0x20D3,  0x20D4,  0x20D5,  0x20D6,  0x20D7,
	0x20D8,  0x20D9,  0x20DA,  0x20DB,  0x20DC,  0x20E1,  0x20E5,  0x20E6,
	0x20E7,  0x20E8,  0x20E9,  0x20EA,  0x20EB,  0x20EC,  0x20ED,  0x20EE,
	0x20EF,  0x20F0,  0x2CEF,  0x2CF0,  0x2CF1,  0x2D7F,  0x2DE0,  0x2DE1,
	0x2DE2,  0x2DE3,  0x2DE4,  0x2DE5,  0x2DE6,  0x2DE7,  0x2DE8,  0x2DE9,
	0x2DEA,  0x2DEB,  0x2DEC,  0x2DED,  0x2DEE,  0x2DEF,  0x2DF0,  0x2DF1,
	0x2DF2,  0x2DF3,  0x2DF4,  0x2DF5,  0x2DF6,  0x2DF7,  0x2DF8,  0x2DF9,
	0x2DFA,  0x2DFB,  0x2DFC,  0x2DFD,  0x2DFE,  0x2DFF,  0x302A,  0x302B,
	0x302C,  0x302D,  0x3099,  0x309A,  0xA66F,  0xA674,  0xA675,  0xA676,
	0xA677,  0xA678,  0xA679,  0xA67A,  0xA67B,  0xA67C,  0xA67D,  0xA69E,
	0xA69F,  0xA6F0,  0xA6F1,  0xA802,  0xA806,  0xA80B,  0xA825,  0xA826,
	0xA8C4,  0xA8C5,  0xA8E0,  0xA8E1,  0xA8E2,  0xA8E3,  0xA8E4,  0xA8E5,
	0xA8E6,  0xA8E7,  0xA8E8,  0xA8E9,  0xA8EA,  0xA8EB,  0xA8EC,  0xA8ED,
	0xA8EE,  0xA8EF,  0xA8F0,  0xA8F1,  0xA926,  0xA927,  0xA928,  0xA929,
	0xA92A,  0xA92B,  0xA92C,  0xA92D,  0xA947,  0xA948,  0xA949,  0xA94A,
	0xA94B,  0xA94C,  0xA94D,  0xA94E,  0xA94F,  0xA950,  0xA951,  0xA980,
	0xA981,  0xA982,  0xA9B3,  0xA9B6,  0xA9B7,  0xA9B8,  0xA9B9,  0xA9BC,
	0xA9E5,  0xAA29,  0xAA2A,  0xAA2B,  0xAA2C,  0xAA2D,  0xAA2E,  0xAA31,
	0xAA32,  0xAA35,  0xAA36,  0xAA43,  0xAA4C,  0xAA7C,  0xAAB0,  0xAAB2,
	0xAAB3,  0xAAB4,  0xAAB7,  0xAAB8,  0xAABE,  0xAABF,  0xAAC1,  0xAAEC,
	0xAAED,  0xAAF6,  0xABE5,  0xABE8,  0xABED,  0xFB1E,  0xFE00,  0xFE01,
	0xFE02,  0xFE03,  0xFE04,  0xFE05,  0xFE06,  0xFE07,  0xFE08,  0xFE09,
	0xFE0A,  0xFE0B,  0xFE0C,  0xFE0D,  0xFE0E,  0xFE0F,  0xFE20,  0xFE21,
	0xFE22,  0xFE23,  0xFE24,  0xFE25,  0xFE26,  0xFE27,  0xFE28,  0xFE29,
	0xFE2A,  0xFE2B,  0xFE2C,  0xFE2D,  0xFE2E,  0xFE2F,  0x101FD, 0x102E0,
	0x10376, 0x10377, 0x10378, 0x10379, 0x1037A, 0x10A01, 0x10A02, 0x10A03,
	0x10A05, 0x10A06, 0x10A0C, 0x10A0D, 0x10A0E, 0x10A0F, 0x10A38, 0x10A39,
	0x10A3A, 0x10A3F, 0x10AE5, 0x10AE6, 0x11001, 0x11038, 0x11039, 0x1103A,
	0x1103B, 0x1103C, 0x1103D, 0x1103E, 0x1103F, 0x11040, 0x11041, 0x11042,
	0x11043, 0x11044, 0x11045, 0x11046, 0x1107F, 0x11080, 0x11081, 0x110B3,
	0x110B4, 0x110B5, 0x110B6, 0x110B9, 0x110BA, 0x11100, 0x11101, 0x11102,
	0x11127, 0x11128, 0x11129, 0x1112A, 0x1112B, 0x1112D, 0x1112E, 0x1112F,
	0x11130, 0x11131, 0x11132, 0x11133, 0x11134, 0x11173, 0x11180, 0x11181,
	0x111B6, 0x111B7, 0x111B8, 0x111B9, 0x111BA, 0x111BB, 0x111BC, 0x111BD,
	0x111BE, 0x111CA, 0x111CB, 0x111CC, 0x1122F, 0x11230, 0x11231, 0x11234,
	0x11236, 0x11237, 0x1123E, 0x112DF, 0x112E3, 0x112E4, 0x112E5, 0x112E6,
	0x112E7, 0x112E8, 0x112E9, 0x112EA, 0x11300, 0x11301, 0x1133C, 0x11340,
	0x11366, 0x11367, 0x11368, 0x11369, 0x1136A, 0x1136B, 0x1136C, 0x11370,
	0x11371, 0x11372, 0x11373, 0x11374, 0x11438, 0x11439, 0x1143A, 0x1143B,
	0x1143C, 0x1143D, 0x1143E, 0x1143F, 0x11442, 0x11443, 0x11444, 0x11446,
	0x114B3, 0x114B4, 0x114B5, 0x114B6, 0x114B7, 0x114B8, 0x114BA, 0x114BF,
	0x114C0, 0x114C2, 0x114C3, 0x115B2, 0x115B3, 0x115B4, 0x115B5, 0x115BC,
	0x115BD, 0x115BF, 0x115C0, 0x115DC, 0x115DD, 0x11633, 0x11634, 0x11635,
	0x11636, 0x11637, 0x11638, 0x11639, 0x1163A, 0x1163D, 0x1163F, 0x11640,
	0x116AB, 0x116AD, 0x116B0, 0x116B1, 0x116B2, 0x116B3, 0x116B4, 0x116B5,
	0x116B7, 0x1171D, 0x1171E, 0x1171F, 0x11722, 0x11723, 0x11724, 0x11725,
	0x11727, 0x11728, 0x11729, 0x1172A, 0x1172B, 0x11C30, 0x11C31, 0x11C32,
	0x11C33, 0x11C34, 0x11C35, 0x11C36, 0x11C38, 0x11C39, 0x11C3A, 0x11C3B,
	0x11C3C, 0x11C3D, 0x11C3F, 0x11C92, 0x11C93, 0x11C94, 0x11C95, 0x11C96,
	0x11C97, 0x11C98, 0x11C99, 0x11C9A, 0x11C9B, 0x11C9C, 0x11C9D, 0x11C9E,
	0x11C9F, 0x11CA0, 0x11CA1, 0x11CA2, 0x11CA3, 0x11CA4, 0x11CA5, 0x11CA6,
	0x11CA7, 0x11CAA, 0x11CAB, 0x11CAC, 0x11CAD, 0x11CAE, 0x11CAF, 0x11CB0,
	0x11CB2, 0x11CB3, 0x11CB5, 0x11CB6, 0x16AF0, 0x16AF1, 0x16AF2, 0x16AF3,
	0x16AF4, 0x16B30, 0x16B31, 0x16B32, 0x16B33, 0x16B34, 0x16B35, 0x16B36,
	0x16F8F, 0x16F90, 0x16F91, 0x16F92, 0x1BC9D, 0x1BC9E, 0x1D167, 0x1D168,
	0x1D169, 0x1D17B, 0x1D17C, 0x1D17D, 0x1D17E, 0x1D17F, 0x1D180, 0x1D181,
	0x1D182, 0x1D185, 0x1D186, 0x1D187, 0x1D188, 0x1D189, 0x1D18A, 0x1D18B,
	0x1D1AA, 0x1D1AB, 0x1D1AC, 0x1D1AD, 0x1D242, 0x1D243, 0x1D244, 0x1DA00,
	0x1DA01, 0x1DA02, 0x1DA03, 0x1DA04, 0x1DA05, 0x1DA06, 0x1DA07, 0x1DA08,
	0x1DA09, 0x1DA0A, 0x1DA0B, 0x1DA0C, 0x1DA0D, 0x1DA0E, 0x1DA0F, 0x1DA10,
	0x1DA11, 0x1DA12, 0x1DA13, 0x1DA14, 0x1DA15, 0x1DA16, 0x1DA17, 0x1DA18,
	0x1DA19, 0x1DA1A, 0x1DA1B, 0x1DA1C, 0x1DA1D, 0x1DA1E, 0x1DA1F, 0x1DA20,
	0x1DA21, 0x1DA22, 0x1DA23, 0x1DA24, 0x1DA25, 0x1DA26, 0x1DA27, 0x1DA28,
	0x1DA29, 0x1DA2A, 0x1DA2B, 0x1DA2C, 0x1DA2D, 0x1DA2E, 0x1DA2F, 0x1DA30,
	0x1DA31, 0x1DA32, 0x1DA33, 0x1DA34, 0x1DA35, 0x1DA36, 0x1DA3B, 0x1DA3C,
	0x1DA3D, 0x1DA3E, 0x1DA3F, 0x1DA40, 0x1DA41, 0x1DA42, 0x1DA43, 0x1DA44,
	0x1DA45, 0x1DA46, 0x1DA47, 0x1DA48, 0x1DA49, 0x1DA4A, 0x1DA4B, 0x1DA4C,
	0x1DA4D, 0x1DA4E, 0x1DA4F, 0x1DA50, 0x1DA51, 0x1DA52, 0x1DA53, 0x1DA54,
	0x1DA55, 0x1DA56, 0x1DA57, 0x1DA58, 0x1DA59, 0x1DA5A, 0x1DA5B, 0x1DA5C,
	0x1DA5D, 0x1DA5E, 0x1DA5F, 0x1DA60, 0x1DA61, 0x1DA62, 0x1DA63, 0x1DA64,
	0x1DA65, 0x1DA66, 0x1DA67, 0x1DA68, 0x1DA69, 0x1DA6A, 0x1DA6B, 0x1DA6C,
	0x1DA75, 0x1DA84, 0x1DA9B, 0x1DA9C, 0x1DA9D, 0x1DA9E, 0x1DA9F, 0x1DAA1,
	0x1DAA2, 0x1DAA3, 0x1DAA4, 0x1DAA5, 0x1DAA6, 0x1DAA7, 0x1DAA8, 0x1DAA9,
	0x1DAAA, 0x1DAAB, 0x1DAAC, 0x1DAAD, 0x1DAAE, 0x1DAAF, 0x1E000, 0x1E001,
	0x1E002, 0x1E003, 0x1E004, 0x1E005, 0x1E006, 0x1E008, 0x1E009, 0x1E00A,
	0x1E00B, 0x1E00C, 0x1E00D, 0x1E00E, 0x1E00F, 0x1E010, 0x1E011, 0x1E012,
	0x1E013, 0x1E014, 0x1E015, 0x1E016, 0x1E017, 0x1E018, 0x1E01B, 0x1E01C,
	0x1E01D, 0x1E01E, 0x1E01F, 0x1E020, 0x1E021, 0x1E023, 0x1E024, 0x1E026,
	0x1E027, 0x1E028, 0x1E029, 0x1E02A, 0x1E8D0, 0x1E8D1, 0x1E8D2, 0x1E8D3,
	0x1E8D4, 0x1E8D5, 0x1E8D6, 0x1E944, 0x1E945, 0x1E946, 0x1E947, 0x1E948,
	0x1E949, 0x1E94A, 0xE0100, 0xE0101, 0xE0102, 0xE0103, 0xE0104, 0xE0105,
	0xE0106, 0xE0107, 0xE0108, 0xE0109, 0xE010A, 0xE010B, 0xE010C, 0xE010D,
	0xE010E, 0xE010F, 0xE0110, 0xE0111, 0xE0112, 0xE0113, 0xE0114, 0xE0115,
	0xE0116, 0xE0117, 0xE0118, 0xE0119, 0xE011A, 0xE011B, 0xE011C, 0xE011D,
	0xE011E, 0xE011F, 0xE0120, 0xE0121, 0xE0122, 0xE0123, 0xE0124, 0xE0125,
	0xE0126, 0xE0127, 0xE0128, 0xE0129, 0xE012A, 0xE012B, 0xE012C, 0xE012D,
	0xE012E, 0xE012F, 0xE0130, 0xE0131, 0xE0132, 0xE0133, 0xE0134, 0xE0135,
	0xE0136, 0xE0137, 0xE0138, 0xE0139, 0xE013A, 0xE013B, 0xE013C, 0xE013D,
	0xE013E, 0xE013F, 0xE0140, 0xE0141, 0xE0142, 0xE0143, 0xE0144, 0xE0145,
	0xE0146, 0xE0147, 0xE0148, 0xE0149, 0xE014A, 0xE014B, 0xE014C, 0xE014D,
	0xE014E, 0xE014F, 0xE0150, 0xE0151, 0xE0152, 0xE0153, 0xE0154, 0xE0155,
	0xE0156, 0xE0157, 0xE0158, 0xE0159, 0xE015A, 0xE015B, 0xE015C, 0xE015D,
	0xE015E, 0xE015F, 0xE0160, 0xE0161, 0xE0162, 0xE0163, 0xE0164, 0xE0165,
	0xE0166, 0xE0167, 0xE0168, 0xE0169, 0xE016A, 0xE016B, 0xE016C, 0xE016D,
	0xE016E, 0xE016F, 0xE0170, 0xE0171, 0xE0172, 0xE0173, 0xE0174, 0xE0175,
	0xE0176, 0xE0177, 0xE0178, 0xE0179, 0xE017A, 0xE017B, 0xE017C, 0xE017D,
	0xE017E, 0xE017F, 0xE0180, 0xE0181, 0xE0182, 0xE0183, 0xE0184, 0xE0185,
	0xE0186, 0xE0187, 0xE0188, 0xE0189, 0xE018A, 0xE018B, 0xE018C, 0xE018D,
	0xE018E, 0xE018F, 0xE0190, 0xE0191, 0xE0192, 0xE0193, 0xE0194, 0xE0195,
	0xE0196, 0xE0197, 0xE0198, 0xE0199, 0xE019A, 0xE019B, 0xE019C, 0xE019D,
	0xE019E, 0xE019F, 0xE01A0, 0xE01A1, 0xE01A2, 0xE01A3, 0xE01A4, 0xE01A5,
	0xE01A6, 0xE01A7, 0xE01A8, 0xE01A9, 0xE01AA, 0xE01AB, 0xE01AC, 0xE01AD,
	0xE01AE, 0xE01AF, 0xE01B0, 0xE01B1, 0xE01B2, 0xE01B3, 0xE01B4, 0xE01B5,
	0xE01B6, 0xE01B7, 0xE01B8, 0xE01B9, 0xE01BA, 0xE01BB, 0xE01BC, 0xE01BD,
	0xE01BE, 0xE01BF, 0xE01C0, 0xE01C1, 0xE01C2, 0xE01C3, 0xE01C4, 0xE01C5,
	0xE01C6, 0xE01C7, 0xE01C8, 0xE01C9, 0xE01CA, 0xE01CB, 0xE01CC, 0xE01CD,
	0xE01CE, 0xE01CF, 0xE01D0, 0xE01D1, 0xE01D2, 0xE01D3, 0xE01D4, 0xE01D5,
	0xE01D6, 0xE01D7, 0xE01D8, 0xE01D9, 0xE01DA, 0xE01DB, 0xE01DC, 0xE01DD,
	0xE01DE, 0xE01DF, 0xE01E0, 0xE01E1, 0xE01E2, 0xE01E3, 0xE01E4, 0xE01E5,
	0xE01E6, 0xE01E7, 0xE01E8, 0xE01E9, 0xE01EA, 0xE01EB, 0xE01EC, 0xE01ED,
	0xE01EE, 0xE01EF,
};

/// The length of the combining characters list.
const size_t bc_history_combo_chars_len = sizeof(bc_history_combo_chars) /
                                          sizeof(bc_history_combo_chars[0]);
#endif // BC_ENABLE_HISTORY && !BC_ENABLE_LINE_LIB

/// The human-readable name of the main function in bc source code.
const char bc_func_main[] = "(main)";

/// The human-readable name of the read function in bc source code.
const char bc_func_read[] = "(read)";

#if BC_DEBUG_CODE

/// A list of names of instructions for easy debugging output.
const char* bc_inst_names[] = {

#if BC_ENABLED
	"BC_INST_INC",
	"BC_INST_DEC",
#endif // BC_ENABLED

	"BC_INST_NEG",
	"BC_INST_BOOL_NOT",
#if BC_ENABLE_EXTRA_MATH
	"BC_INST_TRUNC",
#endif // BC_ENABLE_EXTRA_MATH

	"BC_INST_POWER",
	"BC_INST_MULTIPLY",
	"BC_INST_DIVIDE",
	"BC_INST_MODULUS",
	"BC_INST_PLUS",
	"BC_INST_MINUS",

#if BC_ENABLE_EXTRA_MATH
	"BC_INST_PLACES",

	"BC_INST_LSHIFT",
	"BC_INST_RSHIFT",
#endif // BC_ENABLE_EXTRA_MATH

	"BC_INST_REL_EQ",
	"BC_INST_REL_LE",
	"BC_INST_REL_GE",
	"BC_INST_REL_NE",
	"BC_INST_REL_LT",
	"BC_INST_REL_GT",

	"BC_INST_BOOL_OR",
	"BC_INST_BOOL_AND",

#if BC_ENABLED
	"BC_INST_ASSIGN_POWER",
	"BC_INST_ASSIGN_MULTIPLY",
	"BC_INST_ASSIGN_DIVIDE",
	"BC_INST_ASSIGN_MODULUS",
	"BC_INST_ASSIGN_PLUS",
	"BC_INST_ASSIGN_MINUS",
#if BC_ENABLE_EXTRA_MATH
	"BC_INST_ASSIGN_PLACES",
	"BC_INST_ASSIGN_LSHIFT",
	"BC_INST_ASSIGN_RSHIFT",
#endif // BC_ENABLE_EXTRA_MATH
	"BC_INST_ASSIGN",

	"BC_INST_ASSIGN_POWER_NO_VAL",
	"BC_INST_ASSIGN_MULTIPLY_NO_VAL",
	"BC_INST_ASSIGN_DIVIDE_NO_VAL",
	"BC_INST_ASSIGN_MODULUS_NO_VAL",
	"BC_INST_ASSIGN_PLUS_NO_VAL",
	"BC_INST_ASSIGN_MINUS_NO_VAL",
#if BC_ENABLE_EXTRA_MATH
	"BC_INST_ASSIGN_PLACES_NO_VAL",
	"BC_INST_ASSIGN_LSHIFT_NO_VAL",
	"BC_INST_ASSIGN_RSHIFT_NO_VAL",
#endif // BC_ENABLE_EXTRA_MATH
#endif // BC_ENABLED
	"BC_INST_ASSIGN_NO_VAL",

	"BC_INST_NUM",
	"BC_INST_VAR",
	"BC_INST_ARRAY_ELEM",
	"BC_INST_ARRAY",

	"BC_INST_ZERO",
	"BC_INST_ONE",

#if BC_ENABLED
	"BC_INST_LAST",
#endif // BC_ENABLED
	"BC_INST_IBASE",
	"BC_INST_OBASE",
	"BC_INST_SCALE",
#if BC_ENABLE_EXTRA_MATH
	"BC_INST_SEED",
#endif // BC_ENABLE_EXTRA_MATH
	"BC_INST_LENGTH",
	"BC_INST_SCALE_FUNC",
	"BC_INST_SQRT",
	"BC_INST_ABS",
	"BC_INST_IS_NUMBER",
	"BC_INST_IS_STRING",
#if BC_ENABLE_EXTRA_MATH
	"BC_INST_IRAND",
#endif // BC_ENABLE_EXTRA_MATH
	"BC_INST_ASCIIFY",
	"BC_INST_READ",
#if BC_ENABLE_EXTRA_MATH
	"BC_INST_RAND",
#endif // BC_ENABLE_EXTRA_MATH
	"BC_INST_MAXIBASE",
	"BC_INST_MAXOBASE",
	"BC_INST_MAXSCALE",
#if BC_ENABLE_EXTRA_MATH
	"BC_INST_MAXRAND",
#endif // BC_ENABLE_EXTRA_MATH

	"BC_INST_PRINT",
	"BC_INST_PRINT_POP",
	"BC_INST_STR",
#if BC_ENABLED
	"BC_INST_PRINT_STR",

	"BC_INST_JUMP",
	"BC_INST_JUMP_ZERO",

	"BC_INST_CALL",

	"BC_INST_RET",
	"BC_INST_RET0",
	"BC_INST_RET_VOID",

	"BC_INST_HALT",
#endif // BC_ENABLED

	"BC_INST_POP",
	"BC_INST_SWAP",
	"BC_INST_MODEXP",
	"BC_INST_DIVMOD",
	"BC_INST_PRINT_STREAM",

#if DC_ENABLED
	"BC_INST_POP_EXEC",

	"BC_INST_EXECUTE",
	"BC_INST_EXEC_COND",

	"BC_INST_PRINT_STACK",
	"BC_INST_CLEAR_STACK",
	"BC_INST_REG_STACK_LEN",
	"BC_INST_STACK_LEN",
	"BC_INST_DUPLICATE",

	"BC_INST_LOAD",
	"BC_INST_PUSH_VAR",
	"BC_INST_PUSH_TO_VAR",

	"BC_INST_QUIT",
	"BC_INST_NQUIT",

	"BC_INST_EXEC_STACK_LEN",
#endif // DC_ENABLED

	"BC_INST_INVALID",
};

#endif // BC_DEBUG_CODE

/// A constant string for 0.
const char bc_parse_zero[2] = "0";

/// A constant string for 1.
const char bc_parse_one[2] = "1";

#if BC_ENABLED

/// A list of keywords for bc. This needs to be updated if keywords change.
const BcLexKeyword bc_lex_kws[] = {
	BC_LEX_KW_ENTRY("auto", 4, true),
	BC_LEX_KW_ENTRY("break", 5, true),
	BC_LEX_KW_ENTRY("continue", 8, false),
	BC_LEX_KW_ENTRY("define", 6, true),
	BC_LEX_KW_ENTRY("for", 3, true),
	BC_LEX_KW_ENTRY("if", 2, true),
	BC_LEX_KW_ENTRY("limits", 6, false),
	BC_LEX_KW_ENTRY("return", 6, true),
	BC_LEX_KW_ENTRY("while", 5, true),
	BC_LEX_KW_ENTRY("halt", 4, false),
	BC_LEX_KW_ENTRY("last", 4, false),
	BC_LEX_KW_ENTRY("ibase", 5, true),
	BC_LEX_KW_ENTRY("obase", 5, true),
	BC_LEX_KW_ENTRY("scale", 5, true),
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_ENTRY("seed", 4, false),
#endif // BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_ENTRY("length", 6, true),
	BC_LEX_KW_ENTRY("print", 5, false),
	BC_LEX_KW_ENTRY("sqrt", 4, true),
	BC_LEX_KW_ENTRY("abs", 3, false),
	BC_LEX_KW_ENTRY("is_number", 9, false),
	BC_LEX_KW_ENTRY("is_string", 9, false),
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_ENTRY("irand", 5, false),
#endif // BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_ENTRY("asciify", 7, false),
	BC_LEX_KW_ENTRY("modexp", 6, false),
	BC_LEX_KW_ENTRY("divmod", 6, false),
	BC_LEX_KW_ENTRY("quit", 4, true),
	BC_LEX_KW_ENTRY("read", 4, false),
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_ENTRY("rand", 4, false),
#endif // BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_ENTRY("maxibase", 8, false),
	BC_LEX_KW_ENTRY("maxobase", 8, false),
	BC_LEX_KW_ENTRY("maxscale", 8, false),
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_ENTRY("maxrand", 7, false),
#endif // BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_ENTRY("line_length", 11, false),
	BC_LEX_KW_ENTRY("global_stacks", 13, false),
	BC_LEX_KW_ENTRY("leading_zero", 12, false),
	BC_LEX_KW_ENTRY("stream", 6, false),
	BC_LEX_KW_ENTRY("else", 4, false),
};

/// The length of the list of bc keywords.
const size_t bc_lex_kws_len = sizeof(bc_lex_kws) / sizeof(BcLexKeyword);

#if BC_C11

// This is here to ensure that BC_LEX_NKWS, which is needed for the
// redefined_kws in BcVm, is correct. If it's correct under C11, it will be
// correct under C99, and I did not know any other way of ensuring they remained
// synchronized.
_Static_assert(sizeof(bc_lex_kws) / sizeof(BcLexKeyword) == BC_LEX_NKWS,
               "BC_LEX_NKWS is wrong.");

#endif // BC_C11

/// An array of booleans that correspond to token types. An entry is true if the
/// token is valid in an expression, false otherwise. This will need to change
/// if tokens change.
const uint8_t bc_parse_exprs[] = {

	// Starts with BC_LEX_EOF.
	BC_PARSE_EXPR_ENTRY(false, false, true, true, true, true, true, true),

	// Starts with BC_LEX_OP_MULTIPLY if extra math is enabled, BC_LEX_OP_DIVIDE
	// otherwise.
	BC_PARSE_EXPR_ENTRY(true, true, true, true, true, true, true, true),

	// Starts with BC_LEX_OP_REL_EQ if extra math is enabled, BC_LEX_OP_REL_LT
	// otherwise.
	BC_PARSE_EXPR_ENTRY(true, true, true, true, true, true, true, true),

#if BC_ENABLE_EXTRA_MATH

	// Starts with BC_LEX_OP_ASSIGN_POWER.
	BC_PARSE_EXPR_ENTRY(true, true, true, true, true, true, true, true),

	// Starts with BC_LEX_OP_ASSIGN_RSHIFT.
	BC_PARSE_EXPR_ENTRY(true, true, false, false, true, true, false, false),

	// Starts with BC_LEX_RBRACKET.
	BC_PARSE_EXPR_ENTRY(false, false, false, false, true, true, true, false),

	// Starts with BC_LEX_KW_BREAK.
	BC_PARSE_EXPR_ENTRY(false, false, false, false, false, false, false, false),

	// Starts with BC_LEX_KW_HALT.
	BC_PARSE_EXPR_ENTRY(false, true, true, true, true, true, true, false),

	// Starts with BC_LEX_KW_SQRT.
	BC_PARSE_EXPR_ENTRY(true, true, true, true, true, true, true, true),

	// Starts with BC_LEX_KW_QUIT.
	BC_PARSE_EXPR_ENTRY(false, true, true, true, true, true, true, true),

	// Starts with BC_LEX_KW_GLOBAL_STACKS.
	BC_PARSE_EXPR_ENTRY(true, true, false, false, 0, 0, 0, 0)

#else // BC_ENABLE_EXTRA_MATH

	// Starts with BC_LEX_OP_ASSIGN_PLUS.
	BC_PARSE_EXPR_ENTRY(true, true, true, false, false, true, true, false),

	// Starts with BC_LEX_COMMA.
	BC_PARSE_EXPR_ENTRY(false, false, false, false, false, true, true, true),

	// Starts with BC_LEX_KW_AUTO.
	BC_PARSE_EXPR_ENTRY(false, false, false, false, false, false, false, false),

	// Starts with BC_LEX_KW_WHILE.
	BC_PARSE_EXPR_ENTRY(false, false, true, true, true, true, true, false),

	// Starts with BC_LEX_KW_SQRT.
	BC_PARSE_EXPR_ENTRY(true, true, true, true, true, true, true, false),

	// Starts with BC_LEX_KW_MAXIBASE.
	BC_PARSE_EXPR_ENTRY(true, true, true, true, true, true, true, false),

	// Starts with  BC_LEX_KW_ELSE.
	BC_PARSE_EXPR_ENTRY(false, 0, 0, 0, 0, 0, 0, 0)

#endif // BC_ENABLE_EXTRA_MATH
};

/// An array of data for operators that correspond to token types. Note that a
/// lower precedence *value* means a higher precedence.
const uchar bc_parse_ops[] = {
	BC_PARSE_OP(0, false), BC_PARSE_OP(0, false), BC_PARSE_OP(1, false),
	BC_PARSE_OP(1, false),
#if BC_ENABLE_EXTRA_MATH
	BC_PARSE_OP(2, false),
#endif // BC_ENABLE_EXTRA_MATH
	BC_PARSE_OP(4, false), BC_PARSE_OP(5, true),  BC_PARSE_OP(5, true),
	BC_PARSE_OP(5, true),  BC_PARSE_OP(6, true),  BC_PARSE_OP(6, true),
#if BC_ENABLE_EXTRA_MATH
	BC_PARSE_OP(3, false), BC_PARSE_OP(7, true),  BC_PARSE_OP(7, true),
#endif // BC_ENABLE_EXTRA_MATH
	BC_PARSE_OP(9, true),  BC_PARSE_OP(9, true),  BC_PARSE_OP(9, true),
	BC_PARSE_OP(9, true),  BC_PARSE_OP(9, true),  BC_PARSE_OP(9, true),
	BC_PARSE_OP(11, true), BC_PARSE_OP(10, true), BC_PARSE_OP(8, false),
	BC_PARSE_OP(8, false), BC_PARSE_OP(8, false), BC_PARSE_OP(8, false),
	BC_PARSE_OP(8, false), BC_PARSE_OP(8, false),
#if BC_ENABLE_EXTRA_MATH
	BC_PARSE_OP(8, false), BC_PARSE_OP(8, false), BC_PARSE_OP(8, false),
#endif // BC_ENABLE_EXTRA_MATH
	BC_PARSE_OP(8, false),
};

// These identify what tokens can come after expressions in certain cases.

/// The valid next tokens for normal expressions.
const BcParseNext bc_parse_next_expr = BC_PARSE_NEXT(4, BC_LEX_NLINE,
                                                     BC_LEX_SCOLON,
                                                     BC_LEX_RBRACE, BC_LEX_EOF);

/// The valid next tokens for function argument expressions.
const BcParseNext bc_parse_next_arg = BC_PARSE_NEXT(2, BC_LEX_RPAREN,
                                                    BC_LEX_COMMA);

/// The valid next tokens for expressions in print statements.
const BcParseNext bc_parse_next_print = BC_PARSE_NEXT(4, BC_LEX_COMMA,
                                                      BC_LEX_NLINE,
                                                      BC_LEX_SCOLON,
                                                      BC_LEX_EOF);

/// The valid next tokens for if statement conditions or loop conditions. This
/// is used in for loops for the update expression and for builtin function.
///
/// The name is an artifact of history, and is related to @a BC_PARSE_REL (see
/// include/parse.h). It refers to how POSIX only allows some operators as part
/// of the conditional of for loops, while loops, and if statements.
const BcParseNext bc_parse_next_rel = BC_PARSE_NEXT(1, BC_LEX_RPAREN);

/// The valid next tokens for array element expressions.
const BcParseNext bc_parse_next_elem = BC_PARSE_NEXT(1, BC_LEX_RBRACKET);

/// The valid next tokens for for loop initialization expressions and condition
/// expressions.
const BcParseNext bc_parse_next_for = BC_PARSE_NEXT(1, BC_LEX_SCOLON);

/// The valid next tokens for read expressions.
const BcParseNext bc_parse_next_read = BC_PARSE_NEXT(2, BC_LEX_NLINE,
                                                     BC_LEX_EOF);

/// The valid next tokens for the arguments of a builtin function with multiple
/// arguments.
const BcParseNext bc_parse_next_builtin = BC_PARSE_NEXT(1, BC_LEX_COMMA);

#endif // BC_ENABLED

#if DC_ENABLED

/// A list of instructions that need register arguments in dc.
const uint8_t dc_lex_regs[] = {
	BC_LEX_OP_REL_EQ,  BC_LEX_OP_REL_LE,       BC_LEX_OP_REL_GE,
	BC_LEX_OP_REL_NE,  BC_LEX_OP_REL_LT,       BC_LEX_OP_REL_GT,
	BC_LEX_SCOLON,     BC_LEX_COLON,           BC_LEX_KW_ELSE,
	BC_LEX_LOAD,       BC_LEX_LOAD_POP,        BC_LEX_OP_ASSIGN,
	BC_LEX_STORE_PUSH, BC_LEX_REG_STACK_LEVEL, BC_LEX_ARRAY_LENGTH,
};

/// The length of the list of register instructions.
const size_t dc_lex_regs_len = sizeof(dc_lex_regs) / sizeof(uint8_t);

/// A list corresponding to characters starting at double quote ("). If an entry
/// is BC_LEX_INVALID, then that character needs extra lexing in dc. If it does
/// not, the character can trivially be replaced by the entry. Positions are
/// kept because it corresponds to the ASCII table. This may need to be changed
/// if tokens change.
const uchar dc_lex_tokens[] = {
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_IRAND,
#else // BC_ENABLE_EXTRA_MATH
	BC_LEX_INVALID,
#endif // BC_ENABLE_EXTRA_MATH
	BC_LEX_INVALID,
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_OP_TRUNC,
#else // BC_ENABLE_EXTRA_MATH
	BC_LEX_INVALID,
#endif // BC_ENABLE_EXTRA_MATH
	BC_LEX_OP_MODULUS,
	BC_LEX_INVALID,
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_RAND,
#else // BC_ENABLE_EXTRA_MATH
	BC_LEX_INVALID,
#endif // BC_ENABLE_EXTRA_MATH
	BC_LEX_LPAREN,
	BC_LEX_RPAREN,
	BC_LEX_OP_MULTIPLY,
	BC_LEX_OP_PLUS,
	BC_LEX_EXEC_STACK_LENGTH,
	BC_LEX_OP_MINUS,
	BC_LEX_INVALID,
	BC_LEX_OP_DIVIDE,
	BC_LEX_INVALID,
	BC_LEX_INVALID,
	BC_LEX_INVALID,
	BC_LEX_INVALID,
	BC_LEX_INVALID,
	BC_LEX_INVALID,
	BC_LEX_INVALID,
	BC_LEX_INVALID,
	BC_LEX_INVALID,
	BC_LEX_INVALID,
	BC_LEX_COLON,
	BC_LEX_SCOLON,
	BC_LEX_OP_REL_GT,
	BC_LEX_OP_REL_EQ,
	BC_LEX_OP_REL_LT,
	BC_LEX_KW_READ,
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_OP_PLACES,
#else // BC_ENABLE_EXTRA_MATH
	BC_LEX_INVALID,
#endif // BC_ENABLE_EXTRA_MATH
	BC_LEX_INVALID,
	BC_LEX_INVALID,
	BC_LEX_INVALID,
	BC_LEX_INVALID,
	BC_LEX_INVALID,
	BC_LEX_INVALID,
	BC_LEX_EQ_NO_REG,
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_OP_LSHIFT,
#else // BC_ENABLE_EXTRA_MATH
	BC_LEX_INVALID,
#endif // BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_IBASE,
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_SEED,
#else // BC_ENABLE_EXTRA_MATH
	BC_LEX_INVALID,
#endif // BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_SCALE,
	BC_LEX_LOAD_POP,
	BC_LEX_OP_BOOL_AND,
	BC_LEX_OP_BOOL_NOT,
	BC_LEX_KW_OBASE,
	BC_LEX_KW_STREAM,
	BC_LEX_NQUIT,
	BC_LEX_POP,
	BC_LEX_STORE_PUSH,
	BC_LEX_KW_MAXIBASE,
	BC_LEX_KW_MAXOBASE,
	BC_LEX_KW_MAXSCALE,
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_MAXRAND,
#else // BC_ENABLE_EXTRA_MATH
	BC_LEX_INVALID,
#endif // BC_ENABLE_EXTRA_MATH
	BC_LEX_SCALE_FACTOR,
	BC_LEX_ARRAY_LENGTH,
	BC_LEX_KW_LENGTH,
	BC_LEX_INVALID,
	BC_LEX_INVALID,
	BC_LEX_INVALID,
	BC_LEX_OP_POWER,
	BC_LEX_NEG,
	BC_LEX_INVALID,
	BC_LEX_KW_ASCIIFY,
	BC_LEX_KW_ABS,
	BC_LEX_CLEAR_STACK,
	BC_LEX_DUPLICATE,
	BC_LEX_KW_ELSE,
	BC_LEX_PRINT_STACK,
	BC_LEX_INVALID,
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_OP_RSHIFT,
#else // BC_ENABLE_EXTRA_MATH
	BC_LEX_INVALID,
#endif // BC_ENABLE_EXTRA_MATH
	BC_LEX_STORE_IBASE,
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_STORE_SEED,
#else // BC_ENABLE_EXTRA_MATH
	BC_LEX_INVALID,
#endif // BC_ENABLE_EXTRA_MATH
	BC_LEX_STORE_SCALE,
	BC_LEX_LOAD,
	BC_LEX_OP_BOOL_OR,
	BC_LEX_PRINT_POP,
	BC_LEX_STORE_OBASE,
	BC_LEX_KW_PRINT,
	BC_LEX_KW_QUIT,
	BC_LEX_SWAP,
	BC_LEX_OP_ASSIGN,
	BC_LEX_KW_IS_STRING,
	BC_LEX_KW_IS_NUMBER,
	BC_LEX_KW_SQRT,
	BC_LEX_INVALID,
	BC_LEX_EXECUTE,
	BC_LEX_REG_STACK_LEVEL,
	BC_LEX_STACK_LEVEL,
	BC_LEX_LBRACE,
	BC_LEX_KW_MODEXP,
	BC_LEX_RBRACE,
	BC_LEX_KW_DIVMOD,
	BC_LEX_INVALID
};

/// A list of instructions that correspond to lex tokens. If an entry is
/// @a BC_INST_INVALID, that lex token needs extra parsing in the dc parser.
/// Otherwise, the token can trivially be replaced by the entry. This needs to
/// be updated if the tokens change.
const uchar dc_parse_insts[] = {
	BC_INST_INVALID,      BC_INST_INVALID,
#if BC_ENABLED
	BC_INST_INVALID,      BC_INST_INVALID,
#endif // BC_ENABLED
	BC_INST_INVALID,      BC_INST_BOOL_NOT,
#if BC_ENABLE_EXTRA_MATH
	BC_INST_TRUNC,
#endif // BC_ENABLE_EXTRA_MATH
	BC_INST_POWER,        BC_INST_MULTIPLY,
	BC_INST_DIVIDE,       BC_INST_MODULUS,
	BC_INST_PLUS,         BC_INST_MINUS,
#if BC_ENABLE_EXTRA_MATH
	BC_INST_PLACES,       BC_INST_LSHIFT,
	BC_INST_RSHIFT,
#endif // BC_ENABLE_EXTRA_MATH
	BC_INST_INVALID,      BC_INST_INVALID,
	BC_INST_INVALID,      BC_INST_INVALID,
	BC_INST_INVALID,      BC_INST_INVALID,
	BC_INST_BOOL_OR,      BC_INST_BOOL_AND,
#if BC_ENABLED
	BC_INST_INVALID,      BC_INST_INVALID,
	BC_INST_INVALID,      BC_INST_INVALID,
	BC_INST_INVALID,      BC_INST_INVALID,
#if BC_ENABLE_EXTRA_MATH
	BC_INST_INVALID,      BC_INST_INVALID,
	BC_INST_INVALID,
#endif // BC_ENABLE_EXTRA_MATH
#endif // BC_ENABLED
	BC_INST_INVALID,      BC_INST_INVALID,
	BC_INST_INVALID,      BC_INST_REL_GT,
	BC_INST_REL_LT,       BC_INST_INVALID,
	BC_INST_INVALID,      BC_INST_INVALID,
	BC_INST_REL_GE,       BC_INST_INVALID,
	BC_INST_REL_LE,       BC_INST_INVALID,
	BC_INST_INVALID,      BC_INST_INVALID,
#if BC_ENABLED
	BC_INST_INVALID,      BC_INST_INVALID,
	BC_INST_INVALID,      BC_INST_INVALID,
	BC_INST_INVALID,      BC_INST_INVALID,
	BC_INST_INVALID,      BC_INST_INVALID,
	BC_INST_INVALID,      BC_INST_INVALID,
	BC_INST_INVALID,
#endif // BC_ENABLED
	BC_INST_IBASE,        BC_INST_OBASE,
	BC_INST_SCALE,
#if BC_ENABLE_EXTRA_MATH
	BC_INST_SEED,
#endif // BC_ENABLE_EXTRA_MATH
	BC_INST_LENGTH,       BC_INST_PRINT,
	BC_INST_SQRT,         BC_INST_ABS,
	BC_INST_IS_NUMBER,    BC_INST_IS_STRING,
#if BC_ENABLE_EXTRA_MATH
	BC_INST_IRAND,
#endif // BC_ENABLE_EXTRA_MATH
	BC_INST_ASCIIFY,      BC_INST_MODEXP,
	BC_INST_DIVMOD,       BC_INST_QUIT,
	BC_INST_INVALID,
#if BC_ENABLE_EXTRA_MATH
	BC_INST_RAND,
#endif // BC_ENABLE_EXTRA_MATH
	BC_INST_MAXIBASE,     BC_INST_MAXOBASE,
	BC_INST_MAXSCALE,
#if BC_ENABLE_EXTRA_MATH
	BC_INST_MAXRAND,
#endif // BC_ENABLE_EXTRA_MATH
	BC_INST_LINE_LENGTH,
#if BC_ENABLED
	BC_INST_INVALID,
#endif // BC_ENABLED
	BC_INST_LEADING_ZERO, BC_INST_PRINT_STREAM,
	BC_INST_INVALID,      BC_INST_EXTENDED_REGISTERS,
	BC_INST_REL_EQ,       BC_INST_INVALID,
	BC_INST_EXECUTE,      BC_INST_PRINT_STACK,
	BC_INST_CLEAR_STACK,  BC_INST_INVALID,
	BC_INST_STACK_LEN,    BC_INST_DUPLICATE,
	BC_INST_SWAP,         BC_INST_POP,
	BC_INST_INVALID,      BC_INST_INVALID,
	BC_INST_INVALID,
#if BC_ENABLE_EXTRA_MATH
	BC_INST_INVALID,
#endif // BC_ENABLE_EXTRA_MATH
	BC_INST_INVALID,      BC_INST_INVALID,
	BC_INST_INVALID,      BC_INST_PRINT_POP,
	BC_INST_NQUIT,        BC_INST_EXEC_STACK_LEN,
	BC_INST_SCALE_FUNC,   BC_INST_INVALID,
};
#endif // DC_ENABLED

#endif // !BC_ENABLE_LIBRARY

#if BC_ENABLE_EXTRA_MATH

/// A constant for the rand multiplier.
const BcRandState bc_rand_multiplier = BC_RAND_MULTIPLIER;

#endif // BC_ENABLE_EXTRA_MATH

// clang-format off

#if BC_LONG_BIT >= 64

/// A constant array for the max of a bigdig number as a BcDig array.
const BcDig bc_num_bigdigMax[] = {
	709551616U,
	446744073U,
	18U,
};

/// A constant array for the max of 2 times a bigdig number as a BcDig array.
const BcDig bc_num_bigdigMax2[] = {
	768211456U,
	374607431U,
	938463463U,
	282366920U,
	340U,
};

#else // BC_LONG_BIT >= 64

/// A constant array for the max of a bigdig number as a BcDig array.
const BcDig bc_num_bigdigMax[] = {
	7296U,
	9496U,
	42U,
};

/// A constant array for the max of 2 times a bigdig number as a BcDig array.
const BcDig bc_num_bigdigMax2[] = {
	1616U,
	955U,
	737U,
	6744U,
	1844U,
};

#endif // BC_LONG_BIT >= 64

// clang-format on

/// The size of the bigdig max array.
const size_t bc_num_bigdigMax_size = sizeof(bc_num_bigdigMax) / sizeof(BcDig);

/// The size of the bigdig max times 2 array.
const size_t bc_num_bigdigMax2_size = sizeof(bc_num_bigdigMax2) / sizeof(BcDig);

/// A string of digits for easy conversion from characters to digits.
const char bc_num_hex_digits[] = "0123456789ABCDEF";

// clang-format off

/// An array for easy conversion from exponent to power of 10.
const BcBigDig bc_num_pow10[BC_BASE_DIGS + 1] = {
	1,
	10,
	100,
	1000,
	10000,
#if BC_BASE_DIGS > 4
	100000,
	1000000,
	10000000,
	100000000,
	1000000000,
#endif // BC_BASE_DIGS > 4
};

// clang-format on

#if !BC_ENABLE_LIBRARY

/// An array of functions for binary operators corresponding to the order of
/// the instructions for the operators.
const BcNumBinaryOp bc_program_ops[] = {
	bc_num_pow,    bc_num_mul,    bc_num_div,
	bc_num_mod,    bc_num_add,    bc_num_sub,
#if BC_ENABLE_EXTRA_MATH
	bc_num_places, bc_num_lshift, bc_num_rshift,
#endif // BC_ENABLE_EXTRA_MATH
};

/// An array of functions for binary operators allocation requests corresponding
/// to the order of the instructions for the operators.
const BcNumBinaryOpReq bc_program_opReqs[] = {
	bc_num_powReq,    bc_num_mulReq,    bc_num_divReq,
	bc_num_divReq,    bc_num_addReq,    bc_num_addReq,
#if BC_ENABLE_EXTRA_MATH
	bc_num_placesReq, bc_num_placesReq, bc_num_placesReq,
#endif // BC_ENABLE_EXTRA_MATH
};

/// An array of unary operator functions corresponding to the order of the
/// instructions.
const BcProgramUnary bc_program_unarys[] = {
	bc_program_negate,
	bc_program_not,
#if BC_ENABLE_EXTRA_MATH
	bc_program_trunc,
#endif // BC_ENABLE_EXTRA_MATH
};

/// A filename for when parsing expressions.
const char bc_program_exprs_name[] = "<exprs>";

/// A filename for when parsing stdin..
const char bc_program_stdin_name[] = "<stdin>";

/// A ready message for SIGINT catching.
const char bc_program_ready_msg[] = "ready for more input\n";

/// The length of the ready message.
const size_t bc_program_ready_msg_len = sizeof(bc_program_ready_msg) - 1;

/// A list of escape characters that a print statement should treat specially.
const char bc_program_esc_chars[] = "ab\\efnqrt";

/// A list of characters corresponding to the escape characters above.
const char bc_program_esc_seqs[] = "\a\b\\\\\f\n\"\r\t";

#endif // !BC_ENABLE_LIBRARY
