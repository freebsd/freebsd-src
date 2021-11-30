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
 * Code to handle special I/O for bc.
 *
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>

#include <fcntl.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <unistd.h>
#endif // _WIN32

#include <read.h>
#include <history.h>
#include <program.h>
#include <vm.h>

/**
 * A portability file open function.
 * @param path  The path to the file to open.
 * @param mode  The mode to open in.
 */
static int bc_read_open(const char* path, int mode) {

	int fd;

#ifndef _WIN32
	fd = open(path, mode);
#else // _WIN32
	fd = -1;
	open(&fd, path, mode);
#endif

	return fd;
}

/**
 * Returns true if the buffer data is non-text.
 * @param buf   The buffer to test.
 * @param size  The size of the buffer.
 */
static bool bc_read_binary(const char *buf, size_t size) {

	size_t i;

	for (i = 0; i < size; ++i) {
		if (BC_ERR(BC_READ_BIN_CHAR(buf[i]))) return true;
	}

	return false;
}

bool bc_read_buf(BcVec *vec, char *buf, size_t *buf_len) {

	char *nl;

	// If nothing there, return.
	if (!*buf_len) return false;

	// Find the newline.
	nl = strchr(buf, '\n');

	// If a newline exists...
	if (nl != NULL) {

		// Get the size of the data up to, and including, the newline.
		size_t nllen = (size_t) ((nl + 1) - buf);

		nllen = *buf_len >= nllen ? nllen : *buf_len;

		// Move data into the vector, and move the rest of the data in the
		// buffer up.
		bc_vec_npush(vec, nllen, buf);
		*buf_len -= nllen;
		memmove(buf, nl + 1, *buf_len + 1);

		return true;
	}

	// Just put the data into the vector.
	bc_vec_npush(vec, *buf_len, buf);
	*buf_len = 0;

	return false;
}

BcStatus bc_read_chars(BcVec *vec, const char *prompt) {

	bool done = false;

	assert(vec != NULL && vec->size == sizeof(char));

	BC_SIG_ASSERT_NOT_LOCKED;

	// Clear the vector.
	bc_vec_popAll(vec);

	// Handle the prompt, if desired.
	if (BC_PROMPT) {
		bc_file_puts(&vm.fout, bc_flush_none, prompt);
		bc_file_flush(&vm.fout, bc_flush_none);
	}

	// Try reading from the buffer, and if successful, just return.
	if (bc_read_buf(vec, vm.buf, &vm.buf_len)) {
		bc_vec_pushByte(vec, '\0');
		return BC_STATUS_SUCCESS;
	}

	// Loop until we have something.
	while (!done) {

		ssize_t r;

		BC_SIG_LOCK;

		// Read data from stdin.
		r = read(STDIN_FILENO, vm.buf + vm.buf_len,
		         BC_VM_STDIN_BUF_SIZE - vm.buf_len);

		// If there was an error...
		if (BC_UNLIKELY(r < 0)) {

			// If interupted...
			if (errno == EINTR) {

				// Jump out if we are supposed to quit, which certain signals
				// will require.
				if (vm.status == (sig_atomic_t) BC_STATUS_QUIT) BC_JMP;

				assert(vm.sig);

				// Clear the signal and status.
				vm.sig = 0;
				vm.status = (sig_atomic_t) BC_STATUS_SUCCESS;

				// Print the ready message and prompt again.
				bc_file_puts(&vm.fout, bc_flush_none, bc_program_ready_msg);
				if (BC_PROMPT) bc_file_puts(&vm.fout, bc_flush_none, prompt);
				bc_file_flush(&vm.fout, bc_flush_none);

				BC_SIG_UNLOCK;

				continue;
			}

			BC_SIG_UNLOCK;

			// If we get here, it's bad. Barf.
			bc_vm_fatalError(BC_ERR_FATAL_IO_ERR);
		}

		BC_SIG_UNLOCK;

		// If we read nothing, make sure to terminate the string and return EOF.
		if (r == 0) {
			bc_vec_pushByte(vec, '\0');
			return BC_STATUS_EOF;
		}

		BC_SIG_LOCK;

		// Add to the buffer.
		vm.buf_len += (size_t) r;
		vm.buf[vm.buf_len] = '\0';

		// Read from the buffer.
		done = bc_read_buf(vec, vm.buf, &vm.buf_len);

		BC_SIG_UNLOCK;
	}

	// Terminate the string.
	bc_vec_pushByte(vec, '\0');

	return BC_STATUS_SUCCESS;
}

BcStatus bc_read_line(BcVec *vec, const char *prompt) {

	BcStatus s;

#if BC_ENABLE_HISTORY
	// Get a line from either history or manual reading.
	if (BC_TTY && !vm.history.badTerm)
		s = bc_history_line(&vm.history, vec, prompt);
	else s = bc_read_chars(vec, prompt);
#else // BC_ENABLE_HISTORY
	s = bc_read_chars(vec, prompt);
#endif // BC_ENABLE_HISTORY

	if (BC_ERR(bc_read_binary(vec->v, vec->len - 1)))
		bc_verr(BC_ERR_FATAL_BIN_FILE, bc_program_stdin_name);

	return s;
}

char* bc_read_file(const char *path) {

	BcErr e = BC_ERR_FATAL_IO_ERR;
	size_t size, to_read;
	struct stat pstat;
	int fd;
	char* buf;
	char* buf2;

	BC_SIG_ASSERT_LOCKED;

	assert(path != NULL);

#ifndef NDEBUG
	// Need this to quiet MSan.
	memset(&pstat, 0, sizeof(struct stat));
#endif // NDEBUG

	fd = bc_read_open(path, O_RDONLY);

	// If we can't read a file, we just barf.
	if (BC_ERR(fd < 0)) bc_verr(BC_ERR_FATAL_FILE_ERR, path);

	// The reason we call fstat is to eliminate TOCTOU race conditions. This
	// way, we have an open file, so it's not going anywhere.
	if (BC_ERR(fstat(fd, &pstat) == -1)) goto malloc_err;

	// Make sure it's not a directory.
	if (BC_ERR(S_ISDIR(pstat.st_mode))) {
		e = BC_ERR_FATAL_PATH_DIR;
		goto malloc_err;
	}

	// Get the size of the file and allocate that much.
	size = (size_t) pstat.st_size;
	buf = bc_vm_malloc(size + 1);
	buf2 = buf;
	to_read = size;

	do {

		// Read the file. We just bail if a signal interrupts. This is so that
		// users can interrupt the reading of big files if they want.
		ssize_t r = read(fd, buf2, to_read);
		if (BC_ERR(r < 0)) goto read_err;
		to_read -= (size_t) r;
		buf2 += (size_t) r;
	} while (to_read);

	// Got to have a nul byte.
	buf[size] = '\0';

	if (BC_ERR(bc_read_binary(buf, size))) {
		e = BC_ERR_FATAL_BIN_FILE;
		goto read_err;
	}

	close(fd);

	return buf;

read_err:
	free(buf);
malloc_err:
	close(fd);
	bc_verr(e, path);
	return NULL;
}
