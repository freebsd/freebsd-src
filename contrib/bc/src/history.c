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
 * ------------------------------------------------------------------------
 *
 * Does a number of crazy assumptions that happen to be true in 99.9999% of
 * the 2010 UNIX computers around.
 *
 * References:
 * - http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * - http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html
 *
 * Todo list:
 * - Filter bogus Ctrl+<char> combinations.
 * - Win32 support
 *
 * Bloat:
 * - History search like Ctrl+r in readline?
 *
 * List of escape sequences used by this program, we do everything just
 * with three sequences. In order to be so cheap we may have some
 * flickering effect with some slow terminal, but the lesser sequences
 * the more compatible.
 *
 * EL (Erase Line)
 *    Sequence: ESC [ n K
 *    Effect: if n is 0 or missing, clear from cursor to end of line
 *    Effect: if n is 1, clear from beginning of line to cursor
 *    Effect: if n is 2, clear entire line
 *
 * CUF (CUrsor Forward)
 *    Sequence: ESC [ n C
 *    Effect: moves cursor forward n chars
 *
 * CUB (CUrsor Backward)
 *    Sequence: ESC [ n D
 *    Effect: moves cursor backward n chars
 *
 * The following is used to get the terminal width if getting
 * the width with the TIOCGWINSZ ioctl fails
 *
 * DSR (Device Status Report)
 *    Sequence: ESC [ 6 n
 *    Effect: reports the current cusor position as ESC [ n ; m R
 *            where n is the row and m is the column
 *
 * When multi line mode is enabled, we also use two additional escape
 * sequences. However multi line editing is disabled by default.
 *
 * CUU (CUrsor Up)
 *    Sequence: ESC [ n A
 *    Effect: moves cursor up of n chars.
 *
 * CUD (CUrsor Down)
 *    Sequence: ESC [ n B
 *    Effect: moves cursor down of n chars.
 *
 * When bc_history_clearScreen() is called, two additional escape sequences
 * are used in order to clear the screen and position the cursor at home
 * position.
 *
 * CUP (CUrsor Position)
 *    Sequence: ESC [ H
 *    Effect: moves the cursor to upper left corner
 *
 * ED (Erase Display)
 *    Sequence: ESC [ 2 J
 *    Effect: clear the whole screen
 *
 * *****************************************************************************
 *
 * Code for line history.
 *
 */

#if BC_ENABLE_HISTORY

#if BC_ENABLE_EDITLINE

#include <string.h>
#include <errno.h>
#include <setjmp.h>

#include <history.h>
#include <vm.h>

sigjmp_buf bc_history_jmpbuf;
volatile sig_atomic_t bc_history_inlinelib;

static char* bc_history_prompt;
static char bc_history_no_prompt[] = "";
static HistEvent bc_history_event;
static bool bc_history_use_prompt;

static char*
bc_history_promptFunc(EditLine* el)
{
	BC_UNUSED(el);
	return BC_PROMPT && bc_history_use_prompt ? bc_history_prompt :
	                                            bc_history_no_prompt;
}

void
bc_history_init(BcHistory* h)
{
	BcVec v;
	char* home;

	home = getenv("HOME");

	// This will hold the true path to the editrc.
	bc_vec_init(&v, 1, BC_DTOR_NONE);

	// Initialize the path to the editrc. This is done manually because the
	// libedit I used to test was failing with a NULL argument for the path,
	// which was supposed to automatically do $HOME/.editrc. But it was failing,
	// so I set it manually.
	if (home == NULL)
	{
		bc_vec_string(&v, bc_history_editrc_len - 1, bc_history_editrc + 1);
	}
	else
	{
		bc_vec_string(&v, strlen(home), home);
		bc_vec_concat(&v, bc_history_editrc);
	}

	h->hist = history_init();
	if (BC_ERR(h->hist == NULL)) bc_vm_fatalError(BC_ERR_FATAL_ALLOC_ERR);

	h->el = el_init(vm->name, stdin, stdout, stderr);
	if (BC_ERR(h->el == NULL)) bc_vm_fatalError(BC_ERR_FATAL_ALLOC_ERR);
	el_set(h->el, EL_SIGNAL, 1);

	// I want history and a prompt.
	history(h->hist, &bc_history_event, H_SETSIZE, 100);
	history(h->hist, &bc_history_event, H_SETUNIQUE, 1);
	el_set(h->el, EL_EDITOR, "emacs");
	el_set(h->el, EL_HIST, history, h->hist);
	el_set(h->el, EL_PROMPT, bc_history_promptFunc);

	// I also want to get the user's .editrc.
	el_source(h->el, v.v);

	bc_vec_free(&v);

	h->badTerm = false;
	bc_history_prompt = NULL;
}

void
bc_history_free(BcHistory* h)
{
	if (BC_PROMPT && bc_history_prompt != NULL) free(bc_history_prompt);
	el_end(h->el);
	history_end(h->hist);
}

BcStatus
bc_history_line(BcHistory* h, BcVec* vec, const char* prompt)
{
	BcStatus s = BC_STATUS_SUCCESS;
	const char* line;
	int len;

	BC_SIG_LOCK;

	// If the jump happens here, then a SIGINT occurred.
	if (sigsetjmp(bc_history_jmpbuf, 0))
	{
		bc_vec_string(vec, 1, "\n");
		goto end;
	}

	// This is so the signal handler can handle line libraries properly.
	bc_history_inlinelib = 1;

	if (BC_PROMPT)
	{
		// Make sure to set the prompt.
		if (bc_history_prompt != NULL)
		{
			if (strcmp(bc_history_prompt, prompt))
			{
				free(bc_history_prompt);
				bc_history_prompt = bc_vm_strdup(prompt);
			}
		}
		else bc_history_prompt = bc_vm_strdup(prompt);
	}

	bc_history_use_prompt = true;

	line = NULL;
	len = -1;
	errno = EINTR;

	// Get the line.
	while (line == NULL && len == -1 && errno == EINTR)
	{
		line = el_gets(h->el, &len);
		bc_history_use_prompt = false;
	}

	// If there is no line...
	if (BC_ERR(line == NULL))
	{
		// If this is true, there was an error. Otherwise, it's just EOF.
		if (len == -1)
		{
			if (errno == ENOMEM) bc_err(BC_ERR_FATAL_ALLOC_ERR);
			bc_err(BC_ERR_FATAL_IO_ERR);
		}
		else
		{
			bc_file_printf(&vm->fout, "\n");
			s = BC_STATUS_EOF;
		}
	}
	// If there is a line...
	else
	{
		bc_vec_string(vec, strlen(line), line);

		if (strcmp(line, "") && strcmp(line, "\n"))
		{
			history(h->hist, &bc_history_event, H_ENTER, line);
		}

		s = BC_STATUS_SUCCESS;
	}

end:

	bc_history_inlinelib = 0;

	BC_SIG_UNLOCK;

	return s;
}

#else // BC_ENABLE_EDITLINE

#if BC_ENABLE_READLINE

#include <assert.h>
#include <setjmp.h>
#include <string.h>

#include <history.h>
#include <vm.h>

sigjmp_buf bc_history_jmpbuf;
volatile sig_atomic_t bc_history_inlinelib;

void
bc_history_init(BcHistory* h)
{
	h->line = NULL;
	h->badTerm = false;

	// I want no tab completion.
	rl_bind_key('\t', rl_insert);
}

void
bc_history_free(BcHistory* h)
{
	if (h->line != NULL) free(h->line);
}

BcStatus
bc_history_line(BcHistory* h, BcVec* vec, const char* prompt)
{
	BcStatus s = BC_STATUS_SUCCESS;
	size_t len;

	BC_SIG_LOCK;

	// If the jump happens here, then a SIGINT occurred.
	if (sigsetjmp(bc_history_jmpbuf, 0))
	{
		bc_vec_string(vec, 1, "\n");
		goto end;
	}

	// This is so the signal handler can handle line libraries properly.
	bc_history_inlinelib = 1;

	// Get rid of the last line.
	if (h->line != NULL)
	{
		free(h->line);
		h->line = NULL;
	}

	// Get the line.
	h->line = readline(BC_PROMPT ? prompt : "");

	// If there was a line, add it to the history. Otherwise, just return an
	// empty line. Oh, and NULL actually means EOF.
	if (h->line != NULL && h->line[0])
	{
		add_history(h->line);

		len = strlen(h->line);

		bc_vec_expand(vec, len + 2);

		bc_vec_string(vec, len, h->line);
		bc_vec_concat(vec, "\n");
	}
	else if (h->line == NULL)
	{
		bc_file_printf(&vm->fout, "%s\n", "^D");
		s = BC_STATUS_EOF;
	}
	else bc_vec_string(vec, 1, "\n");

end:

	bc_history_inlinelib = 0;

	BC_SIG_UNLOCK;

	return s;
}

#else // BC_ENABLE_READLINE

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef _WIN32
#include <strings.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#endif // _WIN32

#include <status.h>
#include <vector.h>
#include <history.h>
#include <read.h>
#include <file.h>
#include <vm.h>

#if BC_DEBUG_CODE

/// A file for outputting to when debugging.
BcFile bc_history_debug_fp;

/// A buffer for the above file.
char* bc_history_debug_buf;

#endif // BC_DEBUG_CODE

/**
 * Checks if the code is a wide character.
 * @param cp  The codepoint to check.
 * @return    True if @a cp is a wide character, false otherwise.
 */
static bool
bc_history_wchar(uint32_t cp)
{
	size_t i;

	for (i = 0; i < bc_history_wchars_len; ++i)
	{
		// Ranges are listed in ascending order.  Therefore, once the
		// whole range is higher than the codepoint we're testing, the
		// codepoint won't be found in any remaining range => bail early.
		if (bc_history_wchars[i][0] > cp) return false;

		// Test this range.
		if (bc_history_wchars[i][0] <= cp && cp <= bc_history_wchars[i][1])
		{
			return true;
		}
	}

	return false;
}

/**
 * Checks if the code is a combining character.
 * @param cp  The codepoint to check.
 * @return    True if @a cp is a combining character, false otherwise.
 */
static bool
bc_history_comboChar(uint32_t cp)
{
	size_t i;

	for (i = 0; i < bc_history_combo_chars_len; ++i)
	{
		// Combining chars are listed in ascending order, so once we pass
		// the codepoint of interest, we know it's not a combining char.
		if (bc_history_combo_chars[i] > cp) return false;
		if (bc_history_combo_chars[i] == cp) return true;
	}

	return false;
}

/**
 * Gets the length of previous UTF8 character.
 * @param buf  The buffer of characters.
 * @param pos  The index into the buffer.
 */
static size_t
bc_history_prevCharLen(const char* buf, size_t pos)
{
	size_t end = pos;
	for (pos -= 1; pos < end && (buf[pos] & 0xC0) == 0x80; --pos)
	{
		continue;
	}
	return end - (pos >= end ? 0 : pos);
}

/**
 * Converts UTF-8 to a Unicode code point.
 * @param s    The string.
 * @param len  The length of the string.
 * @param cp   An out parameter for the codepoint.
 * @return     The number of bytes eaten by the codepoint.
 */
static size_t
bc_history_codePoint(const char* s, size_t len, uint32_t* cp)
{
	if (len)
	{
		uchar byte = (uchar) s[0];

		// This is literally the UTF-8 decoding algorithm. Look that up if you
		// don't understand this.

		if ((byte & 0x80) == 0)
		{
			*cp = byte;
			return 1;
		}
		else if ((byte & 0xE0) == 0xC0)
		{
			if (len >= 2)
			{
				*cp = (((uint32_t) (s[0] & 0x1F)) << 6) |
				      ((uint32_t) (s[1] & 0x3F));
				return 2;
			}
		}
		else if ((byte & 0xF0) == 0xE0)
		{
			if (len >= 3)
			{
				*cp = (((uint32_t) (s[0] & 0x0F)) << 12) |
				      (((uint32_t) (s[1] & 0x3F)) << 6) |
				      ((uint32_t) (s[2] & 0x3F));
				return 3;
			}
		}
		else if ((byte & 0xF8) == 0xF0)
		{
			if (len >= 4)
			{
				*cp = (((uint32_t) (s[0] & 0x07)) << 18) |
				      (((uint32_t) (s[1] & 0x3F)) << 12) |
				      (((uint32_t) (s[2] & 0x3F)) << 6) |
				      ((uint32_t) (s[3] & 0x3F));
				return 4;
			}
		}
		else
		{
			*cp = 0xFFFD;
			return 1;
		}
	}

	*cp = 0;

	return 1;
}

/**
 * Gets the length of next grapheme.
 * @param buf      The buffer.
 * @param buf_len  The length of the buffer.
 * @param pos      The index into the buffer.
 * @param col_len  An out parameter for the length of the grapheme on screen.
 * @return         The number of bytes in the grapheme.
 */
static size_t
bc_history_nextLen(const char* buf, size_t buf_len, size_t pos, size_t* col_len)
{
	uint32_t cp;
	size_t beg = pos;
	size_t len = bc_history_codePoint(buf + pos, buf_len - pos, &cp);

	if (bc_history_comboChar(cp))
	{
		BC_UNREACHABLE

#if !BC_CLANG
		if (col_len != NULL) *col_len = 0;

		return 0;
#endif // !BC_CLANG
	}

	// Store the width of the character on screen.
	if (col_len != NULL) *col_len = bc_history_wchar(cp) ? 2 : 1;

	pos += len;

	// Find the first non-combining character.
	while (pos < buf_len)
	{
		len = bc_history_codePoint(buf + pos, buf_len - pos, &cp);

		if (!bc_history_comboChar(cp)) return pos - beg;

		pos += len;
	}

	return pos - beg;
}

/**
 * Gets the length of previous grapheme.
 * @param buf  The buffer.
 * @param pos  The index into the buffer.
 * @return     The number of bytes in the grapheme.
 */
static size_t
bc_history_prevLen(const char* buf, size_t pos)
{
	size_t end = pos;

	// Find the first non-combining character.
	while (pos > 0)
	{
		uint32_t cp;
		size_t len = bc_history_prevCharLen(buf, pos);

		pos -= len;
		bc_history_codePoint(buf + pos, len, &cp);

		// The original linenoise-mob had an extra parameter col_len, like
		// bc_history_nextLen(), which, if not NULL, was set in this if
		// statement. However, we always passed NULL, so just skip that.
		if (!bc_history_comboChar(cp)) return end - pos;
	}

	BC_UNREACHABLE

#if !BC_CLANG
	return 0;
#endif // BC_CLANG
}

/**
 * Reads @a n characters from stdin.
 * @param buf  The buffer to read into. The caller is responsible for making
 *             sure this is big enough for @a n.
 * @param n    The number of characters to read.
 * @return     The number of characters read or less than 0 on error.
 */
static ssize_t
bc_history_read(char* buf, size_t n)
{
	ssize_t ret;

	BC_SIG_ASSERT_LOCKED;

#ifndef _WIN32

	do
	{
		// We don't care about being interrupted.
		ret = read(STDIN_FILENO, buf, n);
	}
	while (ret == EINTR);

#else // _WIN32

	bool good;
	DWORD read;
	HANDLE hn = GetStdHandle(STD_INPUT_HANDLE);

	good = ReadConsole(hn, buf, (DWORD) n, &read, NULL);

	ret = (read != n || !good) ? -1 : 1;

#endif // _WIN32

	return ret;
}

/**
 * Reads a Unicode code point into a buffer.
 * @param buf      The buffer to read into.
 * @param buf_len  The length of the buffer.
 * @param cp       An out parameter for the codepoint.
 * @param nread    An out parameter for the number of bytes read.
 * @return         BC_STATUS_EOF or BC_STATUS_SUCCESS.
 */
static BcStatus
bc_history_readCode(char* buf, size_t buf_len, uint32_t* cp, size_t* nread)
{
	ssize_t n;
	uchar byte;

	assert(buf_len >= 1);

	BC_SIG_LOCK;

	// Read a byte.
	n = bc_history_read(buf, 1);

	BC_SIG_UNLOCK;

	if (BC_ERR(n <= 0)) goto err;

	// Get the byte.
	byte = ((uchar*) buf)[0];

	// Once again, this is the UTF-8 decoding algorithm, but it has reads
	// instead of actual decoding.
	if ((byte & 0x80) != 0)
	{
		if ((byte & 0xE0) == 0xC0)
		{
			assert(buf_len >= 2);

			BC_SIG_LOCK;

			n = bc_history_read(buf + 1, 1);

			BC_SIG_UNLOCK;

			if (BC_ERR(n <= 0)) goto err;
		}
		else if ((byte & 0xF0) == 0xE0)
		{
			assert(buf_len >= 3);

			BC_SIG_LOCK;

			n = bc_history_read(buf + 1, 2);

			BC_SIG_UNLOCK;

			if (BC_ERR(n <= 0)) goto err;
		}
		else if ((byte & 0xF8) == 0xF0)
		{
			assert(buf_len >= 3);

			BC_SIG_LOCK;

			n = bc_history_read(buf + 1, 3);

			BC_SIG_UNLOCK;

			if (BC_ERR(n <= 0)) goto err;
		}
		else
		{
			n = -1;
			goto err;
		}
	}

	// Convert to the codepoint.
	*nread = bc_history_codePoint(buf, buf_len, cp);

	return BC_STATUS_SUCCESS;

err:
	// If we get here, we either had a fatal error of EOF.
	if (BC_ERR(n < 0)) bc_vm_fatalError(BC_ERR_FATAL_IO_ERR);
	else *nread = (size_t) n;
	return BC_STATUS_EOF;
}

/**
 * Gets the column length from beginning of buffer to current byte position.
 * @param buf      The buffer.
 * @param buf_len  The length of the buffer.
 * @param pos      The index into the buffer.
 * @return         The number of columns between the beginning of @a buffer to
 *                 @a pos.
 */
static size_t
bc_history_colPos(const char* buf, size_t buf_len, size_t pos)
{
	size_t ret = 0, off = 0;

	// While we haven't reached the offset, get the length of the next grapheme.
	while (off < pos && off < buf_len)
	{
		size_t col_len, len;

		len = bc_history_nextLen(buf, buf_len, off, &col_len);

		off += len;
		ret += col_len;
	}

	return ret;
}

/**
 * Returns true if the terminal name is in the list of terminals we know are
 * not able to understand basic escape sequences.
 * @return  True if the terminal is a bad terminal.
 */
static inline bool
bc_history_isBadTerm(void)
{
	size_t i;
	bool ret = false;
	char* term = bc_vm_getenv("TERM");

	if (term == NULL) return false;

	for (i = 0; !ret && bc_history_bad_terms[i]; ++i)
	{
		ret = (!strcasecmp(term, bc_history_bad_terms[i]));
	}

	bc_vm_getenvFree(term);

	return ret;
}

/**
 * Enables raw mode (1960's black magic).
 * @param h  The history data.
 */
static void
bc_history_enableRaw(BcHistory* h)
{
	// I don't do anything for Windows because in Windows, you set their
	// equivalent of raw mode and leave it, so I do it in bc_history_init().

#ifndef _WIN32
	struct termios raw;
	int err;

	assert(BC_TTYIN);

	if (h->rawMode) return;

	BC_SIG_LOCK;

	if (BC_ERR(tcgetattr(STDIN_FILENO, &h->orig_termios) == -1))
	{
		bc_vm_fatalError(BC_ERR_FATAL_IO_ERR);
	}

	BC_SIG_UNLOCK;

	// Modify the original mode.
	raw = h->orig_termios;

	// Input modes: no break, no CR to NL, no parity check, no strip char,
	// no start/stop output control.
	raw.c_iflag &= (unsigned int) (~(BRKINT | ICRNL | INPCK | ISTRIP | IXON));

	// Control modes: set 8 bit chars.
	raw.c_cflag |= (CS8);

	// Local modes - choing off, canonical off, no extended functions,
	// no signal chars (^Z,^C).
	raw.c_lflag &= (unsigned int) (~(ECHO | ICANON | IEXTEN | ISIG));

	// Control chars - set return condition: min number of bytes and timer.
	// We want read to give every single byte, w/o timeout (1 byte, no timer).
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;

	BC_SIG_LOCK;

	// Put terminal in raw mode after flushing.
	do
	{
		err = tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
	}
	while (BC_ERR(err < 0) && errno == EINTR);

	BC_SIG_UNLOCK;

	if (BC_ERR(err < 0)) bc_vm_fatalError(BC_ERR_FATAL_IO_ERR);
#endif // _WIN32

	h->rawMode = true;
}

/**
 * Disables raw mode.
 * @param h  The history data.
 */
static void
bc_history_disableRaw(BcHistory* h)
{
	sig_atomic_t lock;

	if (!h->rawMode) return;

	BC_SIG_TRYLOCK(lock);

#ifndef _WIN32
	if (BC_ERR(tcsetattr(STDIN_FILENO, TCSAFLUSH, &h->orig_termios) != -1))
	{
		h->rawMode = false;
	}
#endif // _WIN32

	BC_SIG_TRYUNLOCK(lock);
}

/**
 * Uses the ESC [6n escape sequence to query the horizontal cursor position
 * and return it. On error -1 is returned, on success the position of the
 * cursor.
 * @return  The horizontal cursor position.
 */
static size_t
bc_history_cursorPos(void)
{
	char buf[BC_HIST_SEQ_SIZE];
	char* ptr;
	char* ptr2;
	size_t cols, rows, i;

	BC_SIG_ASSERT_LOCKED;

	// Report cursor location.
	bc_file_write(&vm->fout, bc_flush_none, "\x1b[6n", 4);
	bc_file_flush(&vm->fout, bc_flush_none);

	// Read the response: ESC [ rows ; cols R.
	for (i = 0; i < sizeof(buf) - 1; ++i)
	{
		if (bc_history_read(buf + i, 1) != 1 || buf[i] == 'R') break;
	}

	buf[i] = '\0';

	// This is basically an error; we didn't get what we were expecting.
	if (BC_ERR(buf[0] != BC_ACTION_ESC || buf[1] != '[')) return SIZE_MAX;

	// Parse the rows.
	ptr = buf + 2;
	rows = strtoul(ptr, &ptr2, 10);

	// Here we also didn't get what we were expecting.
	if (BC_ERR(!rows || ptr2[0] != ';')) return SIZE_MAX;

	// Parse the columns.
	ptr = ptr2 + 1;
	cols = strtoul(ptr, NULL, 10);

	if (BC_ERR(!cols)) return SIZE_MAX;

	return cols <= UINT16_MAX ? cols : 0;
}

/**
 * Tries to get the number of columns in the current terminal, or assume 80
 * if it fails.
 * @return  The number of columns in the terminal.
 */
static size_t
bc_history_columns(void)
{

#ifndef _WIN32

	struct winsize ws;
	int ret;

	ret = ioctl(vm->fout.fd, TIOCGWINSZ, &ws);

	if (BC_ERR(ret == -1 || !ws.ws_col))
	{
		// Calling ioctl() failed. Try to query the terminal itself.
		size_t start, cols;

		// Get the initial position so we can restore it later.
		start = bc_history_cursorPos();
		if (BC_ERR(start == SIZE_MAX)) return BC_HIST_DEF_COLS;

		// Go to right margin and get position.
		bc_file_write(&vm->fout, bc_flush_none, "\x1b[999C", 6);
		bc_file_flush(&vm->fout, bc_flush_none);
		cols = bc_history_cursorPos();
		if (BC_ERR(cols == SIZE_MAX)) return BC_HIST_DEF_COLS;

		// Restore position.
		if (cols > start)
		{
			bc_file_printf(&vm->fout, "\x1b[%zuD", cols - start);
			bc_file_flush(&vm->fout, bc_flush_none);
		}

		return cols;
	}

	return ws.ws_col;

#else // _WIN32

	CONSOLE_SCREEN_BUFFER_INFO csbi;

	if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
	{
		return 80;
	}

	return ((size_t) (csbi.srWindow.Right)) - csbi.srWindow.Left + 1;

#endif // _WIN32
}

/**
 * Gets the column length of prompt text. This is probably unnecessary because
 * the prompts that I use are ASCII, but I kept it just in case.
 * @param prompt  The prompt.
 * @param plen    The length of the prompt.
 * @return        The column length of the prompt.
 */
static size_t
bc_history_promptColLen(const char* prompt, size_t plen)
{
	char buf[BC_HIST_MAX_LINE + 1];
	size_t buf_len = 0, off = 0;

	// The original linenoise-mob checked for ANSI escapes here on the prompt. I
	// know the prompts do not have ANSI escapes. I deleted the code.
	while (off < plen)
	{
		buf[buf_len++] = prompt[off++];
	}

	return bc_history_colPos(buf, buf_len, buf_len);
}

/**
 * Rewrites the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal.
 * @param h  The history data.
 */
static void
bc_history_refresh(BcHistory* h)
{
	char* buf = h->buf.v;
	size_t colpos, len = BC_HIST_BUF_LEN(h), pos = h->pos, extras_len = 0;

	BC_SIG_ASSERT_LOCKED;

	bc_file_flush(&vm->fout, bc_flush_none);

	// Get to the prompt column position from the left.
	while (h->pcol + bc_history_colPos(buf, len, pos) >= h->cols)
	{
		size_t chlen = bc_history_nextLen(buf, len, 0, NULL);

		buf += chlen;
		len -= chlen;
		pos -= chlen;
	}

	// Get to the prompt column position from the right.
	while (h->pcol + bc_history_colPos(buf, len, len) > h->cols)
	{
		len -= bc_history_prevLen(buf, len);
	}

	// Cursor to left edge.
	bc_file_write(&vm->fout, bc_flush_none, "\r", 1);

	// Take the extra stuff into account. This is where history makes sure to
	// preserve stuff that was printed without a newline.
	if (h->extras.len > 1)
	{
		extras_len = h->extras.len - 1;

		bc_vec_grow(&h->buf, extras_len);

		len += extras_len;
		pos += extras_len;

		bc_file_write(&vm->fout, bc_flush_none, h->extras.v, extras_len);
	}

	// Write the prompt, if desired.
	if (BC_PROMPT) bc_file_write(&vm->fout, bc_flush_none, h->prompt, h->plen);

	bc_file_write(&vm->fout, bc_flush_none, h->buf.v, len - extras_len);

	// Erase to right.
	bc_file_write(&vm->fout, bc_flush_none, "\x1b[0K", 4);

	// We need to be sure to grow this.
	if (pos >= h->buf.len - extras_len) bc_vec_grow(&h->buf, pos + extras_len);

	// Move cursor to original position. Do NOT move the putchar of '\r' to the
	// printf with colpos. That causes a bug where the cursor will go to the end
	// of the line when there is no prompt.
	bc_file_putchar(&vm->fout, bc_flush_none, '\r');
	colpos = bc_history_colPos(h->buf.v, len - extras_len, pos) + h->pcol;

	// Set the cursor position again.
	if (colpos) bc_file_printf(&vm->fout, "\x1b[%zuC", colpos);

	bc_file_flush(&vm->fout, bc_flush_none);
}

/**
 * Inserts the character(s) 'c' at cursor current position.
 * @param h     The history data.
 * @param cbuf  The character buffer to copy from.
 * @param clen  The number of characters to copy.
 */
static void
bc_history_edit_insert(BcHistory* h, const char* cbuf, size_t clen)
{
	BC_SIG_ASSERT_LOCKED;

	bc_vec_grow(&h->buf, clen);

	// If we are at the end of the line...
	if (h->pos == BC_HIST_BUF_LEN(h))
	{
		size_t colpos = 0, len;

		// Copy into the buffer.
		memcpy(bc_vec_item(&h->buf, h->pos), cbuf, clen);

		// Adjust the buffer.
		h->pos += clen;
		h->buf.len += clen - 1;
		bc_vec_pushByte(&h->buf, '\0');

		// Set the length and column position.
		len = BC_HIST_BUF_LEN(h) + h->extras.len - 1;
		colpos = bc_history_promptColLen(h->prompt, h->plen);
		colpos += bc_history_colPos(h->buf.v, len, len);

		// Do we have the trivial case?
		if (colpos < h->cols)
		{
			// Avoid a full update of the line in the trivial case.
			bc_file_write(&vm->fout, bc_flush_none, cbuf, clen);
			bc_file_flush(&vm->fout, bc_flush_none);
		}
		else bc_history_refresh(h);
	}
	else
	{
		// Amount that we need to move.
		size_t amt = BC_HIST_BUF_LEN(h) - h->pos;

		// Move the stuff.
		memmove(h->buf.v + h->pos + clen, h->buf.v + h->pos, amt);
		memcpy(h->buf.v + h->pos, cbuf, clen);

		// Adjust the buffer.
		h->pos += clen;
		h->buf.len += clen;
		h->buf.v[BC_HIST_BUF_LEN(h)] = '\0';

		bc_history_refresh(h);
	}
}

/**
 * Moves the cursor to the left.
 * @param h  The history data.
 */
static void
bc_history_edit_left(BcHistory* h)
{
	BC_SIG_ASSERT_LOCKED;

	// Stop at the left end.
	if (h->pos <= 0) return;

	h->pos -= bc_history_prevLen(h->buf.v, h->pos);

	bc_history_refresh(h);
}

/**
 * Moves the cursor to the right.
 * @param h  The history data.
 */
static void
bc_history_edit_right(BcHistory* h)
{
	BC_SIG_ASSERT_LOCKED;

	// Stop at the right end.
	if (h->pos == BC_HIST_BUF_LEN(h)) return;

	h->pos += bc_history_nextLen(h->buf.v, BC_HIST_BUF_LEN(h), h->pos, NULL);

	bc_history_refresh(h);
}

/**
 * Moves the cursor to the end of the current word.
 * @param h  The history data.
 */
static void
bc_history_edit_wordEnd(BcHistory* h)
{
	size_t len = BC_HIST_BUF_LEN(h);

	BC_SIG_ASSERT_LOCKED;

	// Don't overflow.
	if (!len || h->pos >= len) return;

	// Find the word, then find the end of it.
	while (h->pos < len && isspace(h->buf.v[h->pos]))
	{
		h->pos += 1;
	}
	while (h->pos < len && !isspace(h->buf.v[h->pos]))
	{
		h->pos += 1;
	}

	bc_history_refresh(h);
}

/**
 * Moves the cursor to the start of the current word.
 * @param h  The history data.
 */
static void
bc_history_edit_wordStart(BcHistory* h)
{
	size_t len = BC_HIST_BUF_LEN(h);

	BC_SIG_ASSERT_LOCKED;

	// Stop with no data.
	if (!len) return;

	// Find the word, the find the beginning of the word.
	while (h->pos > 0 && isspace(h->buf.v[h->pos - 1]))
	{
		h->pos -= 1;
	}
	while (h->pos > 0 && !isspace(h->buf.v[h->pos - 1]))
	{
		h->pos -= 1;
	}

	bc_history_refresh(h);
}

/**
 * Moves the cursor to the start of the line.
 * @param h  The history data.
 */
static void
bc_history_edit_home(BcHistory* h)
{
	BC_SIG_ASSERT_LOCKED;

	// Stop at the beginning.
	if (!h->pos) return;

	h->pos = 0;

	bc_history_refresh(h);
}

/**
 * Moves the cursor to the end of the line.
 * @param h  The history data.
 */
static void
bc_history_edit_end(BcHistory* h)
{
	BC_SIG_ASSERT_LOCKED;

	// Stop at the end of the line.
	if (h->pos == BC_HIST_BUF_LEN(h)) return;

	h->pos = BC_HIST_BUF_LEN(h);

	bc_history_refresh(h);
}

/**
 * Substitutes the currently edited line with the next or previous history
 * entry as specified by 'dir' (direction).
 * @param h    The history data.
 * @param dir  The direction to substitute; true means previous, false next.
 */
static void
bc_history_edit_next(BcHistory* h, bool dir)
{
	const char* dup;
	const char* str;

	BC_SIG_ASSERT_LOCKED;

	// Stop if there is no history.
	if (h->history.len <= 1) return;

	// Duplicate the buffer.
	if (h->buf.v[0]) dup = bc_vm_strdup(h->buf.v);
	else dup = "";

	// Update the current history entry before overwriting it with the next one.
	bc_vec_replaceAt(&h->history, h->history.len - 1 - h->idx, &dup);

	// Show the new entry.
	h->idx += (dir == BC_HIST_PREV ? 1 : SIZE_MAX);

	// Se the index appropriately at the ends.
	if (h->idx == SIZE_MAX)
	{
		h->idx = 0;
		return;
	}
	else if (h->idx >= h->history.len)
	{
		h->idx = h->history.len - 1;
		return;
	}

	// Get the string.
	str = *((char**) bc_vec_item(&h->history, h->history.len - 1 - h->idx));
	bc_vec_string(&h->buf, strlen(str), str);

	assert(h->buf.len > 0);

	// Set the position at the end.
	h->pos = BC_HIST_BUF_LEN(h);

	bc_history_refresh(h);
}

/**
 * Deletes the character at the right of the cursor without altering the cursor
 * position. Basically, this is what happens with the "Delete" keyboard key.
 * @param h  The history data.
 */
static void
bc_history_edit_delete(BcHistory* h)
{
	size_t chlen, len = BC_HIST_BUF_LEN(h);

	BC_SIG_ASSERT_LOCKED;

	// If there is no character, skip.
	if (!len || h->pos >= len) return;

	// Get the length of the character.
	chlen = bc_history_nextLen(h->buf.v, len, h->pos, NULL);

	// Move characters after it into its place.
	memmove(h->buf.v + h->pos, h->buf.v + h->pos + chlen, len - h->pos - chlen);

	// Make the buffer valid again.
	h->buf.len -= chlen;
	h->buf.v[BC_HIST_BUF_LEN(h)] = '\0';

	bc_history_refresh(h);
}

/**
 * Deletes the character to the left of the cursor and moves the cursor back one
 * space. Basically, this is what happens with the "Backspace" keyboard key.
 * @param h  The history data.
 */
static void
bc_history_edit_backspace(BcHistory* h)
{
	size_t chlen, len = BC_HIST_BUF_LEN(h);

	BC_SIG_ASSERT_LOCKED;

	// If there are no characters, skip.
	if (!h->pos || !len) return;

	// Get the length of the previous character.
	chlen = bc_history_prevLen(h->buf.v, h->pos);

	// Move everything back one.
	memmove(h->buf.v + h->pos - chlen, h->buf.v + h->pos, len - h->pos);

	// Make the buffer valid again.
	h->pos -= chlen;
	h->buf.len -= chlen;
	h->buf.v[BC_HIST_BUF_LEN(h)] = '\0';

	bc_history_refresh(h);
}

/**
 * Deletes the previous word, maintaining the cursor at the start of the
 * current word.
 * @param h  The history data.
 */
static void
bc_history_edit_deletePrevWord(BcHistory* h)
{
	size_t diff, old_pos = h->pos;

	BC_SIG_ASSERT_LOCKED;

	// If at the beginning of the line, skip.
	if (!old_pos) return;

	// Find the word, then the beginning of the word.
	while (h->pos > 0 && isspace(h->buf.v[h->pos - 1]))
	{
		h->pos -= 1;
	}
	while (h->pos > 0 && !isspace(h->buf.v[h->pos - 1]))
	{
		h->pos -= 1;
	}

	// Get the difference in position.
	diff = old_pos - h->pos;

	// Move the data back.
	memmove(h->buf.v + h->pos, h->buf.v + old_pos,
	        BC_HIST_BUF_LEN(h) - old_pos + 1);

	// Make the buffer valid again.
	h->buf.len -= diff;

	bc_history_refresh(h);
}

/**
 * Deletes the next word, maintaining the cursor at the same position.
 * @param h  The history data.
 */
static void
bc_history_edit_deleteNextWord(BcHistory* h)
{
	size_t next_end = h->pos, len = BC_HIST_BUF_LEN(h);

	BC_SIG_ASSERT_LOCKED;

	// If at the end of the line, skip.
	if (next_end == len) return;

	// Find the word, then the end of the word.
	while (next_end < len && isspace(h->buf.v[next_end]))
	{
		next_end += 1;
	}
	while (next_end < len && !isspace(h->buf.v[next_end]))
	{
		next_end += 1;
	}

	// Move the stuff into position.
	memmove(h->buf.v + h->pos, h->buf.v + next_end, len - next_end);

	// Make the buffer valid again.
	h->buf.len -= next_end - h->pos;

	bc_history_refresh(h);
}

/**
 * Swaps two characters, the one under the cursor and the one to the left.
 * @param h  The history data.
 */
static void
bc_history_swap(BcHistory* h)
{
	size_t pcl, ncl;
	char auxb[5];

	BC_SIG_ASSERT_LOCKED;

	// If there are no characters, skip.
	if (!h->pos) return;

	// Get the length of the previous and next characters.
	pcl = bc_history_prevLen(h->buf.v, h->pos);
	ncl = bc_history_nextLen(h->buf.v, BC_HIST_BUF_LEN(h), h->pos, NULL);

	// To perform a swap we need:
	// * Nonzero char length to the left.
	// * To not be at the end of the line.
	if (pcl && h->pos != BC_HIST_BUF_LEN(h) && pcl < 5 && ncl < 5)
	{
		// Swap.
		memcpy(auxb, h->buf.v + h->pos - pcl, pcl);
		memcpy(h->buf.v + h->pos - pcl, h->buf.v + h->pos, ncl);
		memcpy(h->buf.v + h->pos - pcl + ncl, auxb, pcl);

		// Reset the position.
		h->pos += ((~pcl) + 1) + ncl;

		bc_history_refresh(h);
	}
}

/**
 * Raises the specified signal. This is a convenience function.
 * @param h    The history data.
 * @param sig  The signal to raise.
 */
static void
bc_history_raise(BcHistory* h, int sig)
{
	// We really don't want to be in raw mode when longjmp()'s are flying.
	bc_history_disableRaw(h);
	raise(sig);
}

/**
 * Handles escape sequences. This function will make sense if you know VT100
 * escape codes; otherwise, it will be confusing.
 * @param h  The history data.
 */
static void
bc_history_escape(BcHistory* h)
{
	char c, seq[3];

	BC_SIG_ASSERT_LOCKED;

	// Read a character into seq.
	if (BC_ERR(BC_HIST_READ(seq, 1))) return;

	c = seq[0];

	// ESC ? sequences.
	if (c != '[' && c != 'O')
	{
		if (c == 'f') bc_history_edit_wordEnd(h);
		else if (c == 'b') bc_history_edit_wordStart(h);
		else if (c == 'd') bc_history_edit_deleteNextWord(h);
	}
	else
	{
		// Read a character into seq.
		if (BC_ERR(BC_HIST_READ(seq + 1, 1)))
		{
			bc_vm_fatalError(BC_ERR_FATAL_IO_ERR);
		}

		// ESC [ sequences.
		if (c == '[')
		{
			c = seq[1];

			if (c >= '0' && c <= '9')
			{
				// Extended escape, read additional byte.
				if (BC_ERR(BC_HIST_READ(seq + 2, 1)))
				{
					bc_vm_fatalError(BC_ERR_FATAL_IO_ERR);
				}

				if (seq[2] == '~')
				{
					switch (c)
					{
						case '1':
						{
							bc_history_edit_home(h);
							break;
						}

						case '3':
						{
							bc_history_edit_delete(h);
							break;
						}

						case '4':
						{
							bc_history_edit_end(h);
							break;
						}

						default:
						{
							break;
						}
					}
				}
				else if (seq[2] == ';')
				{
					// Read two characters into seq.
					if (BC_ERR(BC_HIST_READ(seq, 2)))
					{
						bc_vm_fatalError(BC_ERR_FATAL_IO_ERR);
					}

					if (seq[0] != '5') return;
					else if (seq[1] == 'C') bc_history_edit_wordEnd(h);
					else if (seq[1] == 'D') bc_history_edit_wordStart(h);
				}
			}
			else
			{
				switch (c)
				{
					// Up.
					case 'A':
					{
						bc_history_edit_next(h, BC_HIST_PREV);
						break;
					}

					// Down.
					case 'B':
					{
						bc_history_edit_next(h, BC_HIST_NEXT);
						break;
					}

					// Right.
					case 'C':
					{
						bc_history_edit_right(h);
						break;
					}

					// Left.
					case 'D':
					{
						bc_history_edit_left(h);
						break;
					}

					// Home.
					case 'H':
					case '1':
					{
						bc_history_edit_home(h);
						break;
					}

					// End.
					case 'F':
					case '4':
					{
						bc_history_edit_end(h);
						break;
					}

					case 'd':
					{
						bc_history_edit_deleteNextWord(h);
						break;
					}
				}
			}
		}
		// ESC O sequences.
		else
		{
			switch (seq[1])
			{
				case 'A':
				{
					bc_history_edit_next(h, BC_HIST_PREV);
					break;
				}

				case 'B':
				{
					bc_history_edit_next(h, BC_HIST_NEXT);
					break;
				}

				case 'C':
				{
					bc_history_edit_right(h);
					break;
				}

				case 'D':
				{
					bc_history_edit_left(h);
					break;
				}

				case 'F':
				{
					bc_history_edit_end(h);
					break;
				}

				case 'H':
				{
					bc_history_edit_home(h);
					break;
				}
			}
		}
	}
}

/**
 * Adds a line to the history.
 * @param h     The history data.
 * @param line  The line to add.
 */
static void
bc_history_add(BcHistory* h, char* line)
{
	BC_SIG_ASSERT_LOCKED;

	// If there is something already there...
	if (h->history.len)
	{
		// Get the previous.
		char* s = *((char**) bc_vec_item_rev(&h->history, 0));

		// Check for, and discard, duplicates.
		if (!strcmp(s, line))
		{
			free(line);
			return;
		}
	}

	bc_vec_push(&h->history, &line);
}

/**
 * Adds an empty line to the history. This is separate from bc_history_add()
 * because we don't want it allocating.
 * @param h  The history data.
 */
static void
bc_history_add_empty(BcHistory* h)
{
	const char* line = "";

	BC_SIG_ASSERT_LOCKED;

	// If there is something already there...
	if (h->history.len)
	{
		// Get the previous.
		char* s = *((char**) bc_vec_item_rev(&h->history, 0));

		// Check for, and discard, duplicates.
		if (!s[0]) return;
	}

	bc_vec_push(&h->history, &line);
}

/**
 * Resets the history state to nothing.
 * @param h  The history data.
 */
static void
bc_history_reset(BcHistory* h)
{
	BC_SIG_ASSERT_LOCKED;

	h->oldcolpos = h->pos = h->idx = 0;
	h->cols = bc_history_columns();

	// The latest history entry is always our current buffer, that
	// initially is just an empty string.
	bc_history_add_empty(h);

	// Buffer starts empty.
	bc_vec_empty(&h->buf);
}

/**
 * Prints a control character.
 * @param h  The history data.
 * @param c  The control character to print.
 */
static void
bc_history_printCtrl(BcHistory* h, unsigned int c)
{
	char str[3] = { '^', 'A', '\0' };
	const char newline[2] = { '\n', '\0' };

	BC_SIG_ASSERT_LOCKED;

	// Set the correct character.
	str[1] = (char) (c + 'A' - BC_ACTION_CTRL_A);

	// Concatenate the string.
	bc_vec_concat(&h->buf, str);

	h->pos = BC_HIST_BUF_LEN(h);
	bc_history_refresh(h);

	// Pop the string.
	bc_vec_npop(&h->buf, sizeof(str));
	bc_vec_pushByte(&h->buf, '\0');
	h->pos = 0;

	if (c != BC_ACTION_CTRL_C && c != BC_ACTION_CTRL_D)
	{
		// We sometimes want to print a newline; for the times we don't; it's
		// because newlines are taken care of elsewhere.
		bc_file_write(&vm->fout, bc_flush_none, newline, sizeof(newline) - 1);
		bc_history_refresh(h);
	}
}

/**
 * Edits a line of history. This function is the core of the line editing
 * capability of bc history. It expects 'fd' to be already in "raw mode" so that
 * every key pressed will be returned ASAP to read().
 * @param h       The history data.
 * @param prompt  The prompt.
 * @return        BC_STATUS_SUCCESS or BC_STATUS_EOF.
 */
static BcStatus
bc_history_edit(BcHistory* h, const char* prompt)
{
	BC_SIG_LOCK;

	bc_history_reset(h);

	// Don't write the saved output the first time. This is because it has
	// already been written to output. In other words, don't uncomment the
	// line below or add anything like it.
	// bc_file_write(&vm->fout, bc_flush_none, h->extras.v, h->extras.len - 1);

	// Write the prompt if desired.
	if (BC_PROMPT)
	{
		h->prompt = prompt;
		h->plen = strlen(prompt);
		h->pcol = bc_history_promptColLen(prompt, h->plen);

		bc_file_write(&vm->fout, bc_flush_none, prompt, h->plen);
		bc_file_flush(&vm->fout, bc_flush_none);
	}

	// This is the input loop.
	for (;;)
	{
		BcStatus s;
		char cbuf[32];
		unsigned int c = 0;
		size_t nread = 0;

		BC_SIG_UNLOCK;

		// Read a code.
		s = bc_history_readCode(cbuf, sizeof(cbuf), &c, &nread);
		if (BC_ERR(s)) return s;

		BC_SIG_LOCK;

		switch (c)
		{
			case BC_ACTION_LINE_FEED:
			case BC_ACTION_ENTER:
			{
				// Return the line.
				bc_vec_pop(&h->history);
				BC_SIG_UNLOCK;
				return s;
			}

			case BC_ACTION_TAB:
			{
				// My tab handling is dumb; it just prints 8 spaces every time.
				memcpy(cbuf, bc_history_tab, bc_history_tab_len + 1);
				bc_history_edit_insert(h, cbuf, bc_history_tab_len);
				break;
			}

			case BC_ACTION_CTRL_C:
			{
				bc_history_printCtrl(h, c);

				// Quit if the user wants it.
				if (!BC_SIGINT)
				{
					vm->status = BC_STATUS_QUIT;
					BC_SIG_UNLOCK;
					BC_JMP;
				}

				// Print the ready message.
				bc_file_write(&vm->fout, bc_flush_none, vm->sigmsg, vm->siglen);
				bc_file_write(&vm->fout, bc_flush_none, bc_program_ready_msg,
				              bc_program_ready_msg_len);
				bc_history_reset(h);
				bc_history_refresh(h);

				break;
			}

			case BC_ACTION_BACKSPACE:
			case BC_ACTION_CTRL_H:
			{
				bc_history_edit_backspace(h);
				break;
			}

			// Act as end-of-file or delete-forward-char.
			case BC_ACTION_CTRL_D:
			{
				// Act as EOF if there's no chacters, otherwise emulate Emacs
				// delete next character to match historical gnu bc behavior.
				if (BC_HIST_BUF_LEN(h) == 0)
				{
					bc_history_printCtrl(h, c);
					BC_SIG_UNLOCK;
					return BC_STATUS_EOF;
				}

				bc_history_edit_delete(h);

				break;
			}

			// Swaps current character with previous.
			case BC_ACTION_CTRL_T:
			{
				bc_history_swap(h);
				break;
			}

			case BC_ACTION_CTRL_B:
			{
				bc_history_edit_left(h);
				break;
			}

			case BC_ACTION_CTRL_F:
			{
				bc_history_edit_right(h);
				break;
			}

			case BC_ACTION_CTRL_P:
			{
				bc_history_edit_next(h, BC_HIST_PREV);
				break;
			}

			case BC_ACTION_CTRL_N:
			{
				bc_history_edit_next(h, BC_HIST_NEXT);
				break;
			}

			case BC_ACTION_ESC:
			{
				bc_history_escape(h);
				break;
			}

			// Delete the whole line.
			case BC_ACTION_CTRL_U:
			{
				bc_vec_string(&h->buf, 0, "");
				h->pos = 0;

				bc_history_refresh(h);

				break;
			}

			// Delete from current to end of line.
			case BC_ACTION_CTRL_K:
			{
				bc_vec_npop(&h->buf, h->buf.len - h->pos);
				bc_vec_pushByte(&h->buf, '\0');
				bc_history_refresh(h);
				break;
			}

			// Go to the start of the line.
			case BC_ACTION_CTRL_A:
			{
				bc_history_edit_home(h);
				break;
			}

			// Go to the end of the line.
			case BC_ACTION_CTRL_E:
			{
				bc_history_edit_end(h);
				break;
			}

			// Clear screen.
			case BC_ACTION_CTRL_L:
			{
				bc_file_write(&vm->fout, bc_flush_none, "\x1b[H\x1b[2J", 7);
				bc_history_refresh(h);
				break;
			}

			// Delete previous word.
			case BC_ACTION_CTRL_W:
			{
				bc_history_edit_deletePrevWord(h);
				break;
			}

			default:
			{
				// If we have a control character, print it and raise signals as
				// needed.
				if ((c >= BC_ACTION_CTRL_A && c <= BC_ACTION_CTRL_Z) ||
				    c == BC_ACTION_CTRL_BSLASH)
				{
					bc_history_printCtrl(h, c);
#ifndef _WIN32
					if (c == BC_ACTION_CTRL_Z) bc_history_raise(h, SIGTSTP);
					if (c == BC_ACTION_CTRL_S) bc_history_raise(h, SIGSTOP);
					if (c == BC_ACTION_CTRL_BSLASH)
					{
						bc_history_raise(h, SIGQUIT);
					}
#else // _WIN32
					vm->status = BC_STATUS_QUIT;
					BC_SIG_UNLOCK;
					BC_JMP;
#endif // _WIN32
				}
				// Otherwise, just insert.
				else bc_history_edit_insert(h, cbuf, nread);
				break;
			}
		}
	}

	BC_SIG_UNLOCK;

	return BC_STATUS_SUCCESS;
}

/**
 * Returns true if stdin has more data. This is for multi-line pasting, and it
 * does not work on Windows.
 * @param h  The history data.
 */
static inline bool
bc_history_stdinHasData(BcHistory* h)
{
#ifndef _WIN32
	int n;
	return pselect(1, &h->rdset, NULL, NULL, &h->ts, &h->sigmask) > 0 ||
	       (ioctl(STDIN_FILENO, FIONREAD, &n) >= 0 && n > 0);
#else // _WIN32
	return false;
#endif // _WIN32
}

BcStatus
bc_history_line(BcHistory* h, BcVec* vec, const char* prompt)
{
	BcStatus s;
	char* line;

	assert(vm->fout.len == 0);

	bc_history_enableRaw(h);

	do
	{
		// Do the edit.
		s = bc_history_edit(h, prompt);

		// Print a newline and flush.
		bc_file_write(&vm->fout, bc_flush_none, "\n", 1);
		bc_file_flush(&vm->fout, bc_flush_none);

		BC_SIG_LOCK;

		// If we actually have data...
		if (h->buf.v[0])
		{
			// Duplicate it.
			line = bc_vm_strdup(h->buf.v);

			// Store it.
			bc_history_add(h, line);
		}
		// Add an empty string.
		else bc_history_add_empty(h);

		BC_SIG_UNLOCK;

		// Concatenate the line to the return vector.
		bc_vec_concat(vec, h->buf.v);
		bc_vec_concat(vec, "\n");
	}
	while (!s && bc_history_stdinHasData(h));

	assert(!s || s == BC_STATUS_EOF);

	bc_history_disableRaw(h);

	return s;
}

void
bc_history_string_free(void* str)
{
	char* s = *((char**) str);
	BC_SIG_ASSERT_LOCKED;
	if (s[0]) free(s);
}

void
bc_history_init(BcHistory* h)
{

#ifdef _WIN32
	HANDLE out, in;
#endif // _WIN32

	BC_SIG_ASSERT_LOCKED;

	h->rawMode = false;
	h->badTerm = bc_history_isBadTerm();

	// Just don't initialize with a bad terminal.
	if (h->badTerm) return;

#ifdef _WIN32

	h->orig_in = 0;
	h->orig_out = 0;

	in = GetStdHandle(STD_INPUT_HANDLE);
	out = GetStdHandle(STD_OUTPUT_HANDLE);

	// Set the code pages.
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);

	// Get the original modes.
	if (!GetConsoleMode(in, &h->orig_in) || !GetConsoleMode(out, &h->orig_out))
	{
		// Just mark it as a bad terminal on error.
		h->badTerm = true;
		return;
	}
	else
	{
		// Set the new modes.
		DWORD reqOut = h->orig_out | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		DWORD reqIn = h->orig_in | ENABLE_VIRTUAL_TERMINAL_INPUT;

		// The input handle requires turning *off* some modes. That's why
		// history didn't work before; I didn't read the documentation
		// closely enough to see that most modes were automaticall enabled,
		// and they need to be turned off.
		reqOut |= DISABLE_NEWLINE_AUTO_RETURN | ENABLE_PROCESSED_OUTPUT;
		reqIn &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
		reqIn &= ~(ENABLE_PROCESSED_INPUT);

		// Set the modes; if there was an error, assume a bad terminal and
		// quit.
		if (!SetConsoleMode(in, reqIn) || !SetConsoleMode(out, reqOut))
		{
			h->badTerm = true;
			return;
		}
	}
#endif // _WIN32

	bc_vec_init(&h->buf, sizeof(char), BC_DTOR_NONE);
	bc_vec_init(&h->history, sizeof(char*), BC_DTOR_HISTORY_STRING);
	bc_vec_init(&h->extras, sizeof(char), BC_DTOR_NONE);

#ifndef _WIN32
	FD_ZERO(&h->rdset);
	FD_SET(STDIN_FILENO, &h->rdset);
	h->ts.tv_sec = 0;
	h->ts.tv_nsec = 0;

	sigemptyset(&h->sigmask);
	sigaddset(&h->sigmask, SIGINT);
#endif // _WIN32
}

void
bc_history_free(BcHistory* h)
{
	BC_SIG_ASSERT_LOCKED;
#ifndef _WIN32
	bc_history_disableRaw(h);
#else // _WIN32
	SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), h->orig_in);
	SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), h->orig_out);
#endif // _WIN32
#if BC_DEBUG
	bc_vec_free(&h->buf);
	bc_vec_free(&h->history);
	bc_vec_free(&h->extras);
#endif // BC_DEBUG
}

#if BC_DEBUG_CODE

/**
 * Prints scan codes. This special mode is used by bc history in order to print
 * scan codes on screen for debugging / development purposes.
 * @param h  The history data.
 */
void
bc_history_printKeyCodes(BcHistory* h)
{
	char quit[4];

	bc_vm_printf("Linenoise key codes debugging mode.\n"
	             "Press keys to see scan codes. "
	             "Type 'quit' at any time to exit.\n");

	bc_history_enableRaw(h);
	memset(quit, ' ', 4);

	while (true)
	{
		char c;
		ssize_t nread;

		nread = bc_history_read(&c, 1);
		if (nread <= 0) continue;

		// Shift string to left.
		memmove(quit, quit + 1, sizeof(quit) - 1);

		// Insert current char on the right.
		quit[sizeof(quit) - 1] = c;
		if (!memcmp(quit, "quit", sizeof(quit))) break;

		bc_vm_printf("'%c' %lu (type quit to exit)\n", isprint(c) ? c : '?',
		             (unsigned long) c);

		// Go left edge manually, we are in raw mode.
		bc_vm_putchar('\r', bc_flush_none);
		bc_file_flush(&vm->fout, bc_flush_none);
	}

	bc_history_disableRaw(h);
}
#endif // BC_DEBUG_CODE

#endif // BC_ENABLE_HISTORY

#endif // BC_ENABLE_READLINE

#endif // BC_ENABLE_EDITLINE
