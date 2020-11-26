/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2020 Gavin D. Howard and contributors.
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
#include <unistd.h>

#include <file.h>
#include <vm.h>

static void bc_file_ultoa(unsigned long long val, char buf[BC_FILE_ULL_LENGTH])
{
	char buf2[BC_FILE_ULL_LENGTH];
	size_t i, len;

	memset(buf2, 0, BC_FILE_ULL_LENGTH);

	// The i = 1 is to ensure that there is a null byte at the end.
	for (i = 1; val; ++i) {
		unsigned long long mod = val % 10;
		buf2[i] = ((char) mod) + '0';
		val /= 10;
	}

	len = i;

	for (i = 0; i < len; ++i) buf[i] = buf2[len - i - 1];
}

static BcStatus bc_file_output(int fd, const char *buf, size_t n) {

	size_t bytes = 0;
	sig_atomic_t lock;

	BC_SIG_TRYLOCK(lock);

	while (bytes < n) {

		ssize_t written = write(fd, buf + bytes, n - bytes);

		if (BC_ERR(written == -1))
			return errno == EPIPE ? BC_STATUS_EOF : BC_STATUS_ERROR_FATAL;

		bytes += (size_t) written;
	}

	BC_SIG_TRYUNLOCK(lock);

	return BC_STATUS_SUCCESS;
}

BcStatus bc_file_flushErr(BcFile *restrict f) {

	BcStatus s;

	if (f->len) {
		s = bc_file_output(f->fd, f->buf, f->len);
		f->len = 0;
	}
	else s = BC_STATUS_SUCCESS;

	return s;
}

void bc_file_flush(BcFile *restrict f) {

	BcStatus s = bc_file_flushErr(f);

	if (BC_ERR(s)) {

		if (s == BC_STATUS_EOF) {
			vm.status = (sig_atomic_t) s;
			BC_VM_JMP;
		}
		else bc_vm_err(BC_ERR_FATAL_IO_ERR);
	}
}

void bc_file_write(BcFile *restrict f, const char *buf, size_t n) {

	if (n > f->cap - f->len) {
		bc_file_flush(f);
		assert(!f->len);
	}

	if (BC_UNLIKELY(n > f->cap - f->len)) bc_file_output(f->fd, buf, n);
	else {
		memcpy(f->buf + f->len, buf, n);
		f->len += n;
	}
}

void bc_file_printf(BcFile *restrict f, const char *fmt, ...) {

	va_list args;

	va_start(args, fmt);
	bc_file_vprintf(f, fmt, args);
	va_end(args);
}

void bc_file_vprintf(BcFile *restrict f, const char *fmt, va_list args) {

	char *percent;
	const char *ptr = fmt;
	char buf[BC_FILE_ULL_LENGTH];

	while ((percent = strchr(ptr, '%')) != NULL) {

		char c;

		if (percent != ptr) {
			size_t len = (size_t) (percent - ptr);
			bc_file_write(f, ptr, len);
		}

		c = percent[1];

		if (c == 'c') {

			uchar uc = (uchar) va_arg(args, int);

			bc_file_putchar(f, uc);
		}
		else if (c == 's') {

			char *s = va_arg(args, char*);

			bc_file_puts(f, s);
		}
#if BC_DEBUG_CODE
		else if (c == 'd') {

			int d = va_arg(args, int);

			if (d < 0) {
				bc_file_putchar(f, '-');
				d = -d;
			}

			if (!d) bc_file_putchar(f, '0');
			else {
				bc_file_ultoa((unsigned long long) d, buf);
				bc_file_puts(f, buf);
			}
		}
#endif // BC_DEBUG_CODE
		else {

			unsigned long long ull;

			assert((c == 'l' || c == 'z') && percent[2] == 'u');

			if (c == 'z') ull = (unsigned long long) va_arg(args, size_t);
			else ull = (unsigned long long) va_arg(args, unsigned long);

			if (!ull) bc_file_putchar(f, '0');
			else {
				bc_file_ultoa(ull, buf);
				bc_file_puts(f, buf);
			}
		}

		ptr = percent + 2 + (c == 'l' || c == 'z');
	}

	if (ptr[0]) bc_file_puts(f, ptr);
}

void bc_file_puts(BcFile *restrict f, const char *str) {
	bc_file_write(f, str, strlen(str));
}

void bc_file_putchar(BcFile *restrict f, uchar c) {
	if (f->len == f->cap) bc_file_flush(f);
	assert(f->len < f->cap);
	f->buf[f->len] = (char) c;
	f->len += 1;
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
	bc_file_flush(f);
}
