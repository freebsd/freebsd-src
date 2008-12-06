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


/* This capability is only available on POSIX systems. */
#if !defined(HAVE_PIPE) || !defined(HAVE_FCNTL) || \
    !(defined(HAVE_FORK) || defined(HAVE_VFORK))

#include "archive.h"

/*
 * On non-Posix systems, allow the program to build, but choke if
 * this function is actually invoked.
 */
int
archive_read_support_compression_program(struct archive *_a, const char *cmd)
{
	archive_set_error(_a, -1,
	    "External compression programs not supported on this platform");
	return (ARCHIVE_FATAL);
}

#else

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

struct program_reader {
	char *cmd;
	int bid;
};

struct program_source {
	char		*description;
	pid_t		 child;
	int		 child_stdin, child_stdout;

	char		*out_buf;
	size_t		 out_buf_len;

	const char	*child_in_buf;
	size_t		 child_in_buf_avail;
};

static int	program_reader_bid(struct archive_reader *,
		    const void *, size_t);
static struct archive_read_source *program_reader_init(struct archive_read *,
    struct archive_reader *, struct archive_read_source *,
    const void *, size_t);
static int	program_reader_free(struct archive_reader *);

static ssize_t	program_source_read(struct archive_read_source *,
    const void **);
static int	program_source_close(struct archive_read_source *);


int
archive_read_support_compression_program(struct archive *_a, const char *cmd)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct archive_reader *reader = __archive_read_get_reader(a);
	struct program_reader *state;

	state = (struct program_reader *)calloc(sizeof (*state), 1);

	if (state == NULL)
		return (ARCHIVE_FATAL);
	if (reader == NULL)
		return (ARCHIVE_FATAL);

	state->cmd = strdup(cmd);
	state->bid = INT_MAX;

	reader->data = state;
	reader->bid = program_reader_bid;
	reader->init = program_reader_init;
	reader->free = program_reader_free;
	return (ARCHIVE_OK);
}

static int
program_reader_free(struct archive_reader *self)
{
	free(self->data);
	return (ARCHIVE_OK);
}

/*
 * If the user used us to register, they must really want us to
 * handle it, so we always bid INT_MAX the first time we're called.
 * After that, we always return zero, lest we end up instantiating
 * an infinite pipeline.
 */
static int
program_reader_bid(struct archive_reader *self, const void *buff, size_t len)
{
	struct program_reader *state = self->data;
	int bid = state->bid;

	(void)buff; /* UNUSED */
	(void)len; /* UNUSED */

	state->bid = 0; /* Don't bid again on this pipeline. */

	return (bid); /* Default: We'll take it if we haven't yet bid. */
}

/*
 * Use select() to decide whether the child is ready for read or write.
 */

static ssize_t
child_read(struct archive_read_source *self, char *buf, size_t buf_len)
{
	struct program_source *state = self->data;
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
		ret = (self->upstream->read)(self->upstream, &child_buf);
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

	if (state->child_stdin == -1) {
		fcntl(state->child_stdout, F_SETFL, 0);
		__archive_check_child(state->child_stdin, state->child_stdout);
		goto restart_read;
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
		state->child_stdin = -1;
		fcntl(state->child_stdout, F_SETFL, 0);
		goto restart_read;
	} else {
		close(state->child_stdin);
		state->child_stdin = -1;
		fcntl(state->child_stdout, F_SETFL, 0);
		return (-1);
	}
}

static struct archive_read_source *
program_reader_init(struct archive_read *a, struct archive_reader *reader,
    struct archive_read_source *upstream, const void *buff, size_t n)
{
	struct program_source	*state;
	struct program_reader   *reader_state;
	struct archive_read_source *self;
	static const size_t out_buf_len = 65536;
	char *out_buf;
	char *description;
	const char *prefix = "Program: ";


	reader_state = (struct program_reader *)reader->data;

	self = (struct archive_read_source *)malloc(sizeof(*self));
	state = (struct program_source *)malloc(sizeof(*state));
	out_buf = (char *)malloc(out_buf_len);
	description = (char *)malloc(strlen(prefix) + strlen(reader_state->cmd) + 1);
	if (self == NULL
	    || state == NULL
	    || out_buf == NULL
	    || description == NULL)
	{
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate input data");
		free(self);
		free(state);
		free(out_buf);
		free(description);
		return (NULL);
	}

	a->archive.compression_code = ARCHIVE_COMPRESSION_PROGRAM;
	state->description = description;
	strcpy(state->description, prefix);
	strcat(state->description, reader_state->cmd);
	a->archive.compression_name = state->description;

	state->out_buf = out_buf;
	state->out_buf_len = out_buf_len;

	state->child_in_buf = buff;
	state->child_in_buf_avail = n;

	if ((state->child = __archive_create_child(reader_state->cmd,
		 &state->child_stdin, &state->child_stdout)) == -1) {
		free(state->out_buf);
		free(state);
		archive_set_error(&a->archive, EINVAL,
		    "Can't initialise filter");
		return (NULL);
	}

	self->data = state;
	self->read = program_source_read;
	self->skip = NULL;
	self->close = program_source_close;
	self->upstream = upstream;
	self->archive = a;

	/* XXX Check that we can read at least one byte? */
	return (self);
}

static ssize_t
program_source_read(struct archive_read_source *self, const void **buff)
{
	struct program_source *state;
	ssize_t bytes, total;
	char *p;

	state = (struct program_source *)self->data;

	total = 0;
	p = state->out_buf;
	while (state->child_stdout != -1) {
		bytes = child_read(self, p, state->out_buf_len - total);
		if (bytes < 0)
			return (bytes);
		if (bytes == 0)
			break;
		total += bytes;
/* TODO: fix this */ /*	a->archive.raw_position += bytes_read; */
	}

	*buff = state->out_buf;
	return (total);
}

static int
program_source_close(struct archive_read_source *self)
{
	struct program_source	*state;
	int status;

	state = (struct program_source *)self->data;

	/* Shut down the child. */
	if (state->child_stdin != -1)
		close(state->child_stdin);
	if (state->child_stdout != -1)
		close(state->child_stdout);
	while (waitpid(state->child, &status, 0) == -1 && errno == EINTR)
		continue;

	/* Release our private data. */
	free(state->out_buf);
	free(state->description);
	free(state);
	free(self);

	return (ARCHIVE_OK);
}

#endif /* !defined(HAVE_PIPE) || !defined(HAVE_VFORK) || !defined(HAVE_FCNTL) */
