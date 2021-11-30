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
 * Code for implementing buffered I/O on my own terms.
 *
 */

#include <assert.h>
#include <errno.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif // _WIN32

#include <file.h>
#include <vm.h>

/**
 * Translates an integer into a string.
 * @param val  The value to translate.
 * @param buf  The return parameter.
 */
static void bc_file_ultoa(unsigned long long val, char buf[BC_FILE_ULL_LENGTH])
{
	char buf2[BC_FILE_ULL_LENGTH];
	size_t i, len;

	// We need to make sure the entire thing is zeroed.
	memset(buf2, 0, BC_FILE_ULL_LENGTH);

	// The i = 1 is to ensure that there is a null byte at the end.
	for (i = 1; val; ++i) {

		unsigned long long mod = val % 10;

		buf2[i] = ((char) mod) + '0';
		val /= 10;
	}

	len = i;

	// Since buf2 is reversed, reverse it into buf.
	for (i = 0; i < len; ++i) buf[i] = buf2[len - i - 1];
}

/**
 * Output to the file directly.
 * @param fd   The file descriptor.
 * @param buf  The buffer of data to output.
 * @param n    The number of bytes to output.
 * @return     A status indicating error or success. We could have a fatal I/O
 *             error or EOF.
 */
static BcStatus bc_file_output(int fd, const char *buf, size_t n) {

	size_t bytes = 0;
	sig_atomic_t lock;

	BC_SIG_TRYLOCK(lock);

	// While the number of bytes written is less than intended...
	while (bytes < n) {

		// Write.
		ssize_t written = write(fd, buf + bytes, n - bytes);

		// Check for error and return, if any.
		if (BC_ERR(written == -1)) {

			BC_SIG_TRYUNLOCK(lock);

			return errno == EPIPE ? BC_STATUS_EOF : BC_STATUS_ERROR_FATAL;
		}

		bytes += (size_t) written;
	}

	BC_SIG_TRYUNLOCK(lock);

	return BC_STATUS_SUCCESS;
}

BcStatus bc_file_flushErr(BcFile *restrict f, BcFlushType type)
{
	BcStatus s;

	BC_SIG_ASSERT_LOCKED;

	// If there is stuff to output...
	if (f->len) {

#if BC_ENABLE_HISTORY

		// If history is enabled...
		if (BC_TTY) {

			// If we have been told to save the extras, and there *are*
			// extras...
			if (f->buf[f->len - 1] != '\n' &&
			    (type == BC_FLUSH_SAVE_EXTRAS_CLEAR ||
			     type == BC_FLUSH_SAVE_EXTRAS_NO_CLEAR))
			{
				size_t i;

				// Look for the last newline.
				for (i = f->len - 2; i < f->len && f->buf[i] != '\n'; --i);

				i += 1;

				// Save the extras.
				bc_vec_string(&vm.history.extras, f->len - i, f->buf + i);
			}
			// Else clear the extras if told to.
			else if (type >= BC_FLUSH_NO_EXTRAS_CLEAR) {
				bc_vec_popAll(&vm.history.extras);
			}
		}
#endif // BC_ENABLE_HISTORY

		// Actually output.
		s = bc_file_output(f->fd, f->buf, f->len);
		f->len = 0;
	}
	else s = BC_STATUS_SUCCESS;

	return s;
}

void bc_file_flush(BcFile *restrict f, BcFlushType type) {

	BcStatus s;
	sig_atomic_t lock;

	BC_SIG_TRYLOCK(lock);

	s = bc_file_flushErr(f, type);

	// If we have an error...
	if (BC_ERR(s)) {

		// For EOF, set it and jump.
		if (s == BC_STATUS_EOF) {
			vm.status = (sig_atomic_t) s;
			BC_SIG_TRYUNLOCK(lock);
			BC_JMP;
		}
		// Blow up on fatal error. Okay, not blow up, just quit.
		else bc_vm_fatalError(BC_ERR_FATAL_IO_ERR);
	}

	BC_SIG_TRYUNLOCK(lock);
}

void bc_file_write(BcFile *restrict f, BcFlushType type,
                   const char *buf, size_t n)
{
	sig_atomic_t lock;

	BC_SIG_TRYLOCK(lock);

	// If we have enough to flush, do it.
	if (n > f->cap - f->len) {
		bc_file_flush(f, type);
		assert(!f->len);
	}

	// If the output is large enough to flush by itself, just output it.
	// Otherwise, put it into the buffer.
	if (BC_UNLIKELY(n > f->cap - f->len)) bc_file_output(f->fd, buf, n);
	else {
		memcpy(f->buf + f->len, buf, n);
		f->len += n;
	}

	BC_SIG_TRYUNLOCK(lock);
}

void bc_file_printf(BcFile *restrict f, const char *fmt, ...)
{
	va_list args;
	sig_atomic_t lock;

	BC_SIG_TRYLOCK(lock);

	va_start(args, fmt);
	bc_file_vprintf(f, fmt, args);
	va_end(args);

	BC_SIG_TRYUNLOCK(lock);
}

void bc_file_vprintf(BcFile *restrict f, const char *fmt, va_list args) {

	char *percent;
	const char *ptr = fmt;
	char buf[BC_FILE_ULL_LENGTH];

	BC_SIG_ASSERT_LOCKED;

	// This is a poor man's printf(). While I could look up algorithms to make
	// it as fast as possible, and should when I write the standard library for
	// a new language, for bc, outputting is not the bottleneck. So we cheese it
	// for now.

	// Find each percent sign.
	while ((percent = strchr(ptr, '%')) != NULL) {

		char c;

		// If the percent sign is not where we are, write what's inbetween to
		// the buffer.
		if (percent != ptr) {
			size_t len = (size_t) (percent - ptr);
			bc_file_write(f, bc_flush_none, ptr, len);
		}

		c = percent[1];

		// We only parse some format specifiers, the ones bc uses. If you add
		// more, you need to make sure to add them here.
		if (c == 'c') {

			uchar uc = (uchar) va_arg(args, int);

			bc_file_putchar(f, bc_flush_none, uc);
		}
		else if (c == 's') {

			char *s = va_arg(args, char*);

			bc_file_puts(f, bc_flush_none, s);
		}
#if BC_DEBUG_CODE
		// We only print signed integers in debug code.
		else if (c == 'd') {

			int d = va_arg(args, int);

			// Take care of negative. Let's not worry about overflow.
			if (d < 0) {
				bc_file_putchar(f, bc_flush_none, '-');
				d = -d;
			}

			// Either print 0 or translate and print.
			if (!d) bc_file_putchar(f, bc_flush_none, '0');
			else {
				bc_file_ultoa((unsigned long long) d, buf);
				bc_file_puts(f, bc_flush_none, buf);
			}
		}
#endif // BC_DEBUG_CODE
		else {

			unsigned long long ull;

			// These are the ones that it expects from here. Fortunately, all of
			// these are unsigned types, so they can use the same code, more or
			// less.
			assert((c == 'l' || c == 'z') && percent[2] == 'u');

			if (c == 'z') ull = (unsigned long long) va_arg(args, size_t);
			else ull = (unsigned long long) va_arg(args, unsigned long);

			// Either print 0 or translate and print.
			if (!ull) bc_file_putchar(f, bc_flush_none, '0');
			else {
				bc_file_ultoa(ull, buf);
				bc_file_puts(f, bc_flush_none, buf);
			}
		}

		// Increment to the next spot after the specifier.
		ptr = percent + 2 + (c == 'l' || c == 'z');
	}

	// If we get here, there are no more percent signs, so we just output
	// whatever is left.
	if (ptr[0]) bc_file_puts(f, bc_flush_none, ptr);
}

void bc_file_puts(BcFile *restrict f, BcFlushType type, const char *str) {
	bc_file_write(f, type, str, strlen(str));
}

void bc_file_putchar(BcFile *restrict f, BcFlushType type, uchar c) {

	sig_atomic_t lock;

	BC_SIG_TRYLOCK(lock);

	if (f->len == f->cap) bc_file_flush(f, type);

	assert(f->len < f->cap);

	f->buf[f->len] = (char) c;
	f->len += 1;

	BC_SIG_TRYUNLOCK(lock);
}

void bc_file_init(BcFile *f, int fd, char *buf, size_t cap) {

	BC_SIG_ASSERT_LOCKED;

	f->fd = fd;
	f->buf = buf;
	f->len = 0;
	f->cap = cap;
}

void bc_file_free(BcFile *f) {
	BC_SIG_ASSERT_LOCKED;
	bc_file_flush(f, bc_flush_none);
}
