/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2025 Gavin D. Howard and contributors.
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
 * Adapted from the following:
 *
 * linenoise.c -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * You can find the original source code at:
 *   http://github.com/antirez/linenoise
 *
 * You can find the fork that this code is based on at:
 *   https://github.com/rain-1/linenoise-mob
 *
 * ------------------------------------------------------------------------
 *
 * This code is also under the following license:
 *
 * Copyright (c) 2010-2016, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2013, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * *****************************************************************************
 *
 * Definitions for line history.
 *
 */

#ifndef BC_HISTORY_H
#define BC_HISTORY_H

// These must come before the #if BC_ENABLE_LINE_LIB below because status.h
// defines it.
#include <status.h>
#include <vector.h>

#if BC_ENABLE_LINE_LIB

#include <stdbool.h>
#include <setjmp.h>
#include <signal.h>

extern sigjmp_buf bc_history_jmpbuf;
extern volatile sig_atomic_t bc_history_inlinelib;

#endif // BC_ENABLE_LINE_LIB

#if BC_ENABLE_EDITLINE

#include <stdio.h>
#include <histedit.h>

/**
 * The history struct for editline.
 */
typedef struct BcHistory
{
	/// A place to store the current line.
	EditLine* el;

	/// The history.
	History* hist;

	/// Whether the terminal is bad. This is more or less not used.
	bool badTerm;

} BcHistory;

// The path to the editrc and its length.
extern const char bc_history_editrc[];
extern const size_t bc_history_editrc_len;

#else // BC_ENABLE_EDITLINE

#if BC_ENABLE_READLINE

#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>

/**
 * The history struct for readline.
 */
typedef struct BcHistory
{
	/// A place to store the current line.
	char* line;

	/// Whether the terminal is bad. This is more or less not used.
	bool badTerm;

} BcHistory;

#else // BC_ENABLE_READLINE

#if BC_ENABLE_HISTORY

#include <stddef.h>

#include <signal.h>

#ifndef _WIN32
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#else // _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <io.h>
#include <conio.h>

#define strncasecmp _strnicmp
#define strcasecmp _stricmp

#endif // _WIN32

#include <status.h>
#include <vector.h>
#include <read.h>

/// Default columns.
#define BC_HIST_DEF_COLS (80)

/// Max number of history entries.
#define BC_HIST_MAX_LEN (128)

/// Max length of a line.
#define BC_HIST_MAX_LINE (4095)

/// Max size for cursor position buffer.
#define BC_HIST_SEQ_SIZE (64)

/**
 * The number of entries in the history.
 * @param h  The history data.
 */
#define BC_HIST_BUF_LEN(h) ((h)->buf.len - 1)

/**
 * Read n characters into s and check the error.
 * @param s  The buffer to read into.
 * @param n  The number of bytes to read.
 * @return   True if there was an error, false otherwise.
 */
#define BC_HIST_READ(s, n) (bc_history_read((s), (n)) == -1)

/// Markers for direction when using arrow keys.
#define BC_HIST_NEXT (false)
#define BC_HIST_PREV (true)

#if BC_DEBUG_CODE

// These are just for debugging.

#define BC_HISTORY_DEBUG_BUF_SIZE (1024)

// clang-format off
#define lndebug(...)                                                        \
	do                                                                      \
	{                                                                       \
		if (bc_history_debug_fp.fd == 0)                                    \
		{                                                                   \
			bc_history_debug_buf = bc_vm_malloc(BC_HISTORY_DEBUG_BUF_SIZE); \
			bc_file_init(&bc_history_debug_fp,                              \
			             open("/tmp/lndebug.txt", O_APPEND),                \
		                 BC_HISTORY_DEBUG_BUF_SIZE);                        \
			bc_file_printf(&bc_history_debug_fp,                            \
			       "[%zu %zu %zu] p: %d, rows: %d, "                        \
			       "rpos: %d, max: %zu, oldmax: %d\n",                      \
			       l->len, l->pos, l->oldcolpos, plen, rows, rpos,          \
			       l->maxrows, old_rows);                                   \
		}                                                                   \
		bc_file_printf(&bc_history_debug_fp, ", " __VA_ARGS__);             \
		bc_file_flush(&bc_history_debug_fp);                                \
	}                                                                       \
	while (0)
#else // BC_DEBUG_CODE
#define lndebug(fmt, ...)
#endif // BC_DEBUG_CODE
// clang-format on

/// An enum of useful actions. To understand what these mean, check terminal
/// emulators for their shortcuts or the VT100 codes.
typedef enum BcHistoryAction
{
	BC_ACTION_NULL = 0,
	BC_ACTION_CTRL_A = 1,
	BC_ACTION_CTRL_B = 2,
	BC_ACTION_CTRL_C = 3,
	BC_ACTION_CTRL_D = 4,
	BC_ACTION_CTRL_E = 5,
	BC_ACTION_CTRL_F = 6,
	BC_ACTION_CTRL_H = 8,
	BC_ACTION_TAB = 9,
	BC_ACTION_LINE_FEED = 10,
	BC_ACTION_CTRL_K = 11,
	BC_ACTION_CTRL_L = 12,
	BC_ACTION_ENTER = 13,
	BC_ACTION_CTRL_N = 14,
	BC_ACTION_CTRL_P = 16,
	BC_ACTION_CTRL_S = 19,
	BC_ACTION_CTRL_T = 20,
	BC_ACTION_CTRL_U = 21,
	BC_ACTION_CTRL_W = 23,
	BC_ACTION_CTRL_Z = 26,
	BC_ACTION_ESC = 27,
	BC_ACTION_CTRL_BSLASH = 28,
	BC_ACTION_BACKSPACE = 127

} BcHistoryAction;

/**
 * This represents the state during line editing. We pass this state
 * to functions implementing specific editing functionalities.
 */
typedef struct BcHistory
{
	/// Edited line buffer.
	BcVec buf;

	/// The history.
	BcVec history;

	/// Any material printed without a trailing newline.
	BcVec extras;

	/// Prompt to display.
	const char* prompt;

	/// Prompt length.
	size_t plen;

	/// Prompt column length.
	size_t pcol;

	/// Current cursor position.
	size_t pos;

	/// Previous refresh cursor column position.
	size_t oldcolpos;

	/// Number of columns in terminal.
	size_t cols;

	/// The history index we are currently editing.
	size_t idx;

#ifndef _WIN32
	/// The original terminal state.
	struct termios orig_termios;
#else // _WIN32
	///  The original input console mode.
	DWORD orig_in;

	///  The original output console mode.
	DWORD orig_out;
#endif // _WIN32

	/// These next two are here because pahole found a 4 byte hole here.

	/// Whether we are in rawmode.
	bool rawMode;

	/// Whether the terminal is bad.
	bool badTerm;

#ifndef _WIN32
	/// This is to check if stdin has more data.
	fd_set rdset;

	/// This is to check if stdin has more data.
	struct timespec ts;

	/// This is to check if stdin has more data.
	sigset_t sigmask;
#endif // _WIN32

} BcHistory;

/**
 * Frees strings used by history.
 * @param str  The string to free.
 */
void
bc_history_string_free(void* str);

// A list of terminals that don't work.
extern const char* bc_history_bad_terms[];

// A tab in history and its length.
extern const char bc_history_tab[];
extern const size_t bc_history_tab_len;

// A ctrl+c string.
extern const char bc_history_ctrlc[];

// UTF-8 data arrays.
extern const uint32_t bc_history_wchars[][2];
extern const size_t bc_history_wchars_len;
extern const uint32_t bc_history_combo_chars[];
extern const size_t bc_history_combo_chars_len;

#if BC_DEBUG_CODE

// Debug data.
extern BcFile bc_history_debug_fp;
extern char* bc_history_debug_buf;

/**
 * A function to print keycodes for debugging.
 * @param h  The history data.
 */
void
bc_history_printKeyCodes(BcHistory* h);

#endif // BC_DEBUG_CODE

#endif // BC_ENABLE_HISTORY

#endif // BC_ENABLE_READLINE

#endif // BC_ENABLE_EDITLINE

#if BC_ENABLE_HISTORY

/**
 * Get a line from stdin using history. This returns a status because I don't
 * want to throw errors while the terminal is in raw mode.
 * @param h       The history data.
 * @param vec     A vector to put the line into.
 * @param prompt  The prompt to display, if desired.
 * @return        A status indicating an error, if any. Returning a status here
 *                is better because if we throw an error out of history, we
 *                leave the terminal in raw mode or in some other half-baked
 *                state.
 */
BcStatus
bc_history_line(BcHistory* h, BcVec* vec, const char* prompt);

/**
 * Initialize history data.
 * @param h  The struct to initialize.
 */
void
bc_history_init(BcHistory* h);

/**
 * Free history data (and recook the terminal).
 * @param h  The struct to free.
 */
void
bc_history_free(BcHistory* h);

#endif // BC_ENABLE_HISTORY

#endif // BC_HISTORY_H
