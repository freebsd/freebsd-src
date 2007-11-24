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
__FBSDID("$FreeBSD$");

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#ifdef HAVE_ERRNO_H
#  include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#ifdef HAVE_LIMITS_H
#  include <limits.h>
#endif
#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_read_private.h"

#include "filter_fork.h"

struct archive_decompress_program {
	char		*description;
	pid_t		 child;
	int		 child_stdin, child_stdout;

	char		*child_out_buf;
	char		*child_out_buf_next;
	size_t		 child_out_buf_len, child_out_buf_avail;

	const char	*child_in_buf;
	size_t		 child_in_buf_avail;
};

static int	archive_decompressor_program_bid(const void *, size_t);
static int	archive_decompressor_program_finish(struct archive_read *);
static int	archive_decompressor_program_init(struct archive_read *,
		    const void *, size_t);
static ssize_t	archive_decompressor_program_read_ahead(struct archive_read *,
		    const void **, size_t);
static ssize_t	archive_decompressor_program_read_consume(struct archive_read *,
		    size_t);

int
archive_read_support_compression_program(struct archive *_a, const char *cmd)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct decompressor_t *decompressor;

	if (cmd == NULL || *cmd == '\0')
		return (ARCHIVE_WARN);

	decompressor = __archive_read_register_compression(a,
		    archive_decompressor_program_bid,
		    archive_decompressor_program_init);
	if (decompressor == NULL)
		return (ARCHIVE_WARN);

	decompressor->config = strdup(cmd);
	return (ARCHIVE_OK);
}

/*
 * If the user used us to register, they must really want us to
 * handle it, so this module always bids INT_MAX.
 */
static int
archive_decompressor_program_bid(const void *buff, size_t len)
{
	(void)buff; /* UNUSED */
	(void)len; /* UNUSED */

	return (INT_MAX); /* Default: We'll take it. */
}

static ssize_t
child_read(struct archive_read *a, char *buf, size_t buf_len)
{
	struct archive_decompress_program *state = a->decompressor->data;
	ssize_t ret, requested;
	const void *child_buf;

	if (state->child_stdout == -1)
		return (-1);

	if (buf_len == 0)
		return (-1);

restart_read:
	requested = buf_len > SSIZE_MAX ? SSIZE_MAX : buf_len;

	do {
		ret = read(state->child_stdout, buf, requested);
	} while (ret == -1 && errno == EINTR);

	if (ret > 0)
		return (ret);
	if (ret == 0 || (ret == -1 && errno == EPIPE)) {
		close(state->child_stdout);
		state->child_stdout = -1;
		return (0);
	}
	if (ret == -1 && errno != EAGAIN)
		return (-1);

	if (state->child_in_buf_avail == 0) {
		child_buf = state->child_in_buf;
		ret = (a->client_reader)(&a->archive,
		    a->client_data,&child_buf);
		state->child_in_buf = (const char *)child_buf;

		if (ret < 0) {
			close(state->child_stdin);
			state->child_stdin = -1;
			fcntl(state->child_stdout, F_SETFL, 0);
			return (-1);
		}
		if (ret == 0) {
			close(state->child_stdin);
			state->child_stdin = -1;
			fcntl(state->child_stdout, F_SETFL, 0);
			goto restart_read;
		}
		state->child_in_buf_avail = ret;
	}

	do {
		ret = write(state->child_stdin, state->child_in_buf,
		    state->child_in_buf_avail);
	} while (ret == -1 && errno == EINTR);

	if (ret > 0) {
		state->child_in_buf += ret;
		state->child_in_buf_avail -= ret;
		goto restart_read;
	} else if (ret == -1 && errno == EAGAIN) {
		__archive_check_child(state->child_stdin, state->child_stdout);
		goto restart_read;
	} else if (ret == 0 || (ret == -1 && errno == EPIPE)) {
		close(state->child_stdin);
		state->child_stdout = -1;
		fcntl(state->child_stdout, F_SETFL, 0);
		goto restart_read;
	} else {
		close(state->child_stdin);
		state->child_stdin = -1;
		fcntl(state->child_stdout, F_SETFL, 0);
		return (-1);
	}
}

static int
archive_decompressor_program_init(struct archive_read *a, const void *buff, size_t n)
{
	struct archive_decompress_program	*state;
	const char *cmd = a->decompressor->config;
	const char *prefix = "Program: ";


	state = (struct archive_decompress_program *)malloc(sizeof(*state));
	if (!state) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate input data");
		return (ARCHIVE_FATAL);
	}

	a->archive.compression_code = ARCHIVE_COMPRESSION_PROGRAM;
	state->description = (char *)malloc(strlen(prefix) + strlen(cmd) + 1);
	strcpy(state->description, prefix);
	strcat(state->description, cmd);
	a->archive.compression_name = state->description;

	state->child_out_buf_next = state->child_out_buf = malloc(65536);
	if (!state->child_out_buf) {
		free(state);
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate filter buffer");
		return (ARCHIVE_FATAL);
	}
	state->child_out_buf_len = 65536;
	state->child_out_buf_avail = 0;

	state->child_in_buf = buff;
	state->child_in_buf_avail = n;

	if ((state->child = __archive_create_child(cmd,
		 &state->child_stdin, &state->child_stdout)) == -1) {
		free(state->child_out_buf);
		free(state);
		archive_set_error(&a->archive, EINVAL,
		    "Can't initialise filter");
		return (ARCHIVE_FATAL);
	}

	a->decompressor->data = state;
	a->decompressor->read_ahead = archive_decompressor_program_read_ahead;
	a->decompressor->consume = archive_decompressor_program_read_consume;
	a->decompressor->skip = NULL;
	a->decompressor->finish = archive_decompressor_program_finish;

	/* XXX Check that we can read at least one byte? */
	return (ARCHIVE_OK);
}

static ssize_t
archive_decompressor_program_read_ahead(struct archive_read *a, const void **buff,
    size_t min)
{
	struct archive_decompress_program *state;
	ssize_t bytes_read;

	state = (struct archive_decompress_program *)a->decompressor->data;

	if (min > state->child_out_buf_len)
		min = state->child_out_buf_len;

	while (state->child_stdout != -1 && min > state->child_out_buf_avail) {
		if (state->child_out_buf != state->child_out_buf_next) {
			memmove(state->child_out_buf, state->child_out_buf_next,
			    state->child_out_buf_avail);
			state->child_out_buf_next = state->child_out_buf;
		}

		bytes_read = child_read(a,
		    state->child_out_buf + state->child_out_buf_avail,
		    state->child_out_buf_len - state->child_out_buf_avail);
		if (bytes_read == -1)
			return (-1);
		if (bytes_read == 0)
			break;
		state->child_out_buf_avail += bytes_read;
		a->archive.raw_position += bytes_read;
	}

	*buff = state->child_out_buf_next;
	return (state->child_out_buf_avail);
}

static ssize_t
archive_decompressor_program_read_consume(struct archive_read *a, size_t request)
{
	struct archive_decompress_program *state;

	state = (struct archive_decompress_program *)a->decompressor->data;

	state->child_out_buf_next += request;
	state->child_out_buf_avail -= request;

	a->archive.file_position += request;
	return (request);
}

static int
archive_decompressor_program_finish(struct archive_read *a)
{
	struct archive_decompress_program	*state;
	int status;

	state = (struct archive_decompress_program *)a->decompressor->data;

	/* Release our configuration data. */
	free(a->decompressor->config);
	a->decompressor->config = NULL;

	/* Shut down the child. */
	if (state->child_stdin != -1)
		close(state->child_stdin);
	if (state->child_stdout != -1)
		close(state->child_stdout);
	while (waitpid(state->child, &status, 0) == -1 && errno == EINTR)
		continue;

	/* Release our private data. */
	free(state->child_out_buf);
	free(state->description);
	free(state);
	a->decompressor->data = NULL;

	return (ARCHIVE_OK);
}
