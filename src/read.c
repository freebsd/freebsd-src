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
#include <unistd.h>

#include <read.h>
#include <history.h>
#include <program.h>
#include <vm.h>

static bool bc_read_binary(const char *buf, size_t size) {

	size_t i;

	for (i = 0; i < size; ++i) {
		if (BC_ERR(BC_READ_BIN_CHAR(buf[i]))) return true;
	}

	return false;
}

bool bc_read_buf(BcVec *vec, char *buf, size_t *buf_len) {

	char *nl;

	if (!*buf_len) return false;

	nl = strchr(buf, '\n');

	if (nl != NULL) {

		size_t nllen = (size_t) ((nl + 1) - buf);

		nllen = *buf_len >= nllen ? nllen : *buf_len;

		bc_vec_npush(vec, nllen, buf);
		*buf_len -= nllen;
		memmove(buf, nl + 1, *buf_len + 1);

		return true;
	}

	bc_vec_npush(vec, *buf_len, buf);
	*buf_len = 0;

	return false;
}

BcStatus bc_read_chars(BcVec *vec, const char *prompt) {

	bool done = false;

	assert(vec != NULL && vec->size == sizeof(char));

	BC_SIG_ASSERT_NOT_LOCKED;

	bc_vec_npop(vec, vec->len);

#if BC_ENABLE_PROMPT
	if (BC_USE_PROMPT) {
		bc_file_puts(&vm.fout, prompt);
		bc_file_flush(&vm.fout);
	}
#endif // BC_ENABLE_PROMPT

	if (bc_read_buf(vec, vm.buf, &vm.buf_len)) {
		bc_vec_pushByte(vec, '\0');
		return BC_STATUS_SUCCESS;
	}

	while (!done) {

		ssize_t r;

		BC_SIG_LOCK;

		r = read(STDIN_FILENO, vm.buf + vm.buf_len,
		         BC_VM_STDIN_BUF_SIZE - vm.buf_len);

		if (BC_UNLIKELY(r < 0)) {

			if (errno == EINTR) {

				if (vm.status == (sig_atomic_t) BC_STATUS_QUIT) {
					BC_SIG_UNLOCK;
					return BC_STATUS_QUIT;
				}

				assert(vm.sig);

				vm.status = (sig_atomic_t) BC_STATUS_SUCCESS;
#if BC_ENABLE_PROMPT
				if (BC_USE_PROMPT) bc_file_puts(&vm.fout, prompt);
#endif // BC_ENABLE_PROMPT
				bc_file_flush(&vm.fout);

				BC_SIG_UNLOCK;

				continue;
			}

			BC_SIG_UNLOCK;

			bc_vm_err(BC_ERROR_FATAL_IO_ERR);
		}

		BC_SIG_UNLOCK;

		if (r == 0) {
			bc_vec_pushByte(vec, '\0');
			return BC_STATUS_EOF;
		}

		vm.buf_len += (size_t) r;
		vm.buf[vm.buf_len] = '\0';

		done = bc_read_buf(vec, vm.buf, &vm.buf_len);
	}

	bc_vec_pushByte(vec, '\0');

	return BC_STATUS_SUCCESS;
}

BcStatus bc_read_line(BcVec *vec, const char *prompt) {

	BcStatus s;

#if BC_ENABLE_HISTORY
	if (BC_TTY && !vm.history.badTerm)
		s = bc_history_line(&vm.history, vec, prompt);
	else s = bc_read_chars(vec, prompt);
#else // BC_ENABLE_HISTORY
	s = bc_read_chars(vec, prompt);
#endif // BC_ENABLE_HISTORY

	if (BC_ERR(bc_read_binary(vec->v, vec->len - 1)))
		bc_vm_verr(BC_ERROR_FATAL_BIN_FILE, bc_program_stdin_name);

	return s;
}

void bc_read_file(const char *path, char **buf) {

	BcError e = BC_ERROR_FATAL_IO_ERR;
	size_t size, r;
	struct stat pstat;
	int fd;

	BC_SIG_ASSERT_LOCKED;

	assert(path != NULL);

	fd = open(path, O_RDONLY);
	if (BC_ERR(fd < 0)) bc_vm_verr(BC_ERROR_FATAL_FILE_ERR, path);
	if (BC_ERR(fstat(fd, &pstat) == -1)) goto malloc_err;

	if (BC_ERR(S_ISDIR(pstat.st_mode))) {
		e = BC_ERROR_FATAL_PATH_DIR;
		goto malloc_err;
	}

	size = (size_t) pstat.st_size;
	*buf = bc_vm_malloc(size + 1);

	r = (size_t) read(fd, *buf, size);
	if (BC_ERR(r != size)) goto read_err;

	(*buf)[size] = '\0';

	if (BC_ERR(bc_read_binary(*buf, size))) {
		e = BC_ERROR_FATAL_BIN_FILE;
		goto read_err;
	}

	close(fd);

	return;

read_err:
	free(*buf);
malloc_err:
	close(fd);
	bc_vm_verr(e, path);
}
