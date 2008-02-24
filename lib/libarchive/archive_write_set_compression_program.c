/*-
 * Copyright (c) 2007 Joerg Sonnenberger
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "archive_platform.h"

__FBSDID("$FreeBSD: src/lib/libarchive/archive_write_set_compression_program.c,v 1.1 2007/05/29 01:00:19 kientzle Exp $");

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#ifdef HAVE_ERRNO_H
#  include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_write_private.h"

#include "filter_fork.h"

struct private_data {
	char		*description;
	pid_t		 child;
	int		 child_stdin, child_stdout;

	char		*child_buf;
	size_t		 child_buf_len, child_buf_avail;
};

static int	archive_compressor_program_finish(struct archive_write *);
static int	archive_compressor_program_init(struct archive_write *);
static int	archive_compressor_program_write(struct archive_write *,
		    const void *, size_t);

/*
 * Allocate, initialize and return a archive object.
 */
int
archive_write_set_compression_program(struct archive *_a, const char *cmd)
{
	struct archive_write *a = (struct archive_write *)_a;
	__archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_set_compression_program");
	a->compressor.init = &archive_compressor_program_init;
	a->compressor.config = strdup(cmd);
	return (ARCHIVE_OK);
}

/*
 * Setup callback.
 */
static int
archive_compressor_program_init(struct archive_write *a)
{
	int ret;
	struct private_data *state;
	static const char *prefix = "Program: ";
	char *cmd = a->compressor.config;

	if (a->client_opener != NULL) {
		ret = (a->client_opener)(&a->archive, a->client_data);
		if (ret != ARCHIVE_OK)
			return (ret);
	}

	state = (struct private_data *)malloc(sizeof(*state));
	if (state == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate data for compression");
		return (ARCHIVE_FATAL);
	}
	memset(state, 0, sizeof(*state));

	a->archive.compression_code = ARCHIVE_COMPRESSION_PROGRAM;
	state->description = (char *)malloc(strlen(prefix) + strlen(cmd) + 1);
	strcpy(state->description, prefix);
	strcat(state->description, cmd);
	a->archive.compression_name = state->description;

	state->child_buf_len = a->bytes_per_block;
	state->child_buf_avail = 0;
	state->child_buf = malloc(state->child_buf_len);

	if (state->child_buf == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate data for compression buffer");
		free(state);
		return (ARCHIVE_FATAL);
	}

	if ((state->child = __archive_create_child(cmd,
		 &state->child_stdin, &state->child_stdout)) == -1) {
		archive_set_error(&a->archive, EINVAL,
		    "Can't initialise filter");
		free(state->child_buf);
		free(state);
		return (ARCHIVE_FATAL);
	}

	a->compressor.write = archive_compressor_program_write;
	a->compressor.finish = archive_compressor_program_finish;

	a->compressor.data = state;
	return (0);
}

static ssize_t
child_write(struct archive_write *a, const char *buf, size_t buf_len)
{
	struct private_data *state = a->compressor.data;
	ssize_t ret;

	if (state->child_stdin == -1)
		return (-1);

	if (buf_len == 0)
		return (-1);

restart_write:
	do {
		ret = write(state->child_stdin, buf, buf_len);
	} while (ret == -1 && errno == EINTR);

	if (ret > 0)
		return (ret);
	if (ret == 0) {
		close(state->child_stdin);
		state->child_stdin = -1;
		fcntl(state->child_stdout, F_SETFL, 0);
		return (0);
	}
	if (ret == -1 && errno != EAGAIN)
		return (-1);

	do {
		ret = read(state->child_stdout,
		    state->child_buf + state->child_buf_avail,
		    state->child_buf_len - state->child_buf_avail);
	} while (ret == -1 && errno == EINTR);

	if (ret == 0 || (ret == -1 && errno == EPIPE)) {
		close(state->child_stdout);
		state->child_stdout = -1;
		fcntl(state->child_stdin, F_SETFL, 0);
		goto restart_write;
	}
	if (ret == -1 && errno == EAGAIN) {
		__archive_check_child(state->child_stdin, state->child_stdout);
		goto restart_write;
	}
	if (ret == -1)
		return (-1);

	state->child_buf_avail += ret;

	ret = (a->client_writer)(&a->archive, a->client_data,
	    state->child_buf, state->child_buf_avail);
	if (ret <= 0)
		return (-1);

	if ((size_t)ret < state->child_buf_avail) {
		memmove(state->child_buf, state->child_buf + ret,
		    state->child_buf_avail - ret);
	}
	state->child_buf_avail -= ret;
	a->archive.raw_position += ret;
	goto restart_write;
}

/*
 * Write data to the compressed stream.
 */
static int
archive_compressor_program_write(struct archive_write *a, const void *buff,
    size_t length)
{
	struct private_data *state;
	ssize_t ret;
	const char *buf;

	state = (struct private_data *)a->compressor.data;
	if (a->client_writer == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_PROGRAMMER,
		    "No write callback is registered?  "
		    "This is probably an internal programming error.");
		return (ARCHIVE_FATAL);
	}

	buf = buff;
	while (length > 0) {
		ret = child_write(a, buf, length);
		if (ret == -1 || ret == 0) {
			archive_set_error(&a->archive, EIO,
			    "Can't write to filter");
			return (ARCHIVE_FATAL);
		}
		length -= ret;
		buf += ret;
	}

	a->archive.file_position += length;
	return (ARCHIVE_OK);
}


/*
 * Finish the compression...
 */
static int
archive_compressor_program_finish(struct archive_write *a)
{
	int ret, status;
	ssize_t bytes_read, bytes_written;
	struct private_data *state;

	state = (struct private_data *)a->compressor.data;
	ret = 0;
	if (a->client_writer == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_PROGRAMMER,
		    "No write callback is registered?  "
		    "This is probably an internal programming error.");
		ret = ARCHIVE_FATAL;
		goto cleanup;
	}

	/* XXX pad compressed data. */

	close(state->child_stdin);
	state->child_stdin = -1;
	fcntl(state->child_stdout, F_SETFL, 0);

	for (;;) {
		do {
			bytes_read = read(state->child_stdout,
			    state->child_buf + state->child_buf_avail,
			    state->child_buf_len - state->child_buf_avail);
		} while (bytes_read == -1 && errno == EINTR);

		if (bytes_read == 0 || (bytes_read == -1 && errno == EPIPE))
			break;

		if (bytes_read == -1) {
			archive_set_error(&a->archive, errno,
			    "Read from filter failed unexpectedly.");
			ret = ARCHIVE_FATAL;
			goto cleanup;
		}
		state->child_buf_avail += bytes_read;

		bytes_written = (a->client_writer)(&a->archive, a->client_data,
		    state->child_buf, state->child_buf_avail);
		if (bytes_written <= 0) {
			ret = ARCHIVE_FATAL;
			goto cleanup;
		}
		if ((size_t)bytes_written < state->child_buf_avail) {
			memmove(state->child_buf,
			    state->child_buf + bytes_written,
			    state->child_buf_avail - bytes_written);
		}
		state->child_buf_avail -= bytes_written;
		a->archive.raw_position += bytes_written;
	}

	/* XXX pad final compressed block. */

cleanup:
	/* Shut down the child. */
	if (state->child_stdin != -1)
		close(state->child_stdin);
	if (state->child_stdout != -1)
		close(state->child_stdout);
	while (waitpid(state->child, &status, 0) == -1 && errno == EINTR)
		continue;

	if (status != 0) {
		archive_set_error(&a->archive, EIO,
		    "Filter exited with failure.");
		ret = ARCHIVE_FATAL;
	}

	/* Release our configuration data. */
	free(a->compressor.config);
	a->compressor.config = NULL;

	/* Release our private state data. */
	free(state->child_buf);
	free(state->description);
	free(state);
	return (ret);
}
