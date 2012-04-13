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
__FBSDID("$FreeBSD: head/lib/libarchive/archive_write_set_compression_program.c 201104 2009-12-28 03:14:30Z kientzle $");

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

#if ARCHIVE_VERSION_NUMBER < 4000000
int
archive_write_set_compression_program(struct archive *a, const char *cmd)
{
	__archive_write_filters_free(a);
	return (archive_write_add_filter_program(a, cmd));
}
#endif

/* This capability is only available on POSIX systems. */
#if (!defined(HAVE_PIPE) || !defined(HAVE_FCNTL) || \
    !(defined(HAVE_FORK) || defined(HAVE_VFORK))) && (!defined(_WIN32) || defined(__CYGWIN__))

/*
 * On non-Posix systems, allow the program to build, but choke if
 * this function is actually invoked.
 */
int
archive_write_add_filter_program(struct archive *_a, const char *cmd)
{
	archive_set_error(_a, -1,
	    "External compression programs not supported on this platform");
	return (ARCHIVE_FATAL);
}

#else

#include "filter_fork.h"

struct private_data {
	char		*cmd;
	char		*description;
	pid_t		 child;
	int		 child_stdin, child_stdout;

	char		*child_buf;
	size_t		 child_buf_len, child_buf_avail;
};

static int archive_compressor_program_open(struct archive_write_filter *);
static int archive_compressor_program_write(struct archive_write_filter *,
		    const void *, size_t);
static int archive_compressor_program_close(struct archive_write_filter *);
static int archive_compressor_program_free(struct archive_write_filter *);

/*
 * Add a filter to this write handle that passes all data through an
 * external program.
 */
int
archive_write_add_filter_program(struct archive *_a, const char *cmd)
{
	struct archive_write_filter *f = __archive_write_allocate_filter(_a);
	struct archive_write *a = (struct archive_write *)_a;
	struct private_data *data;
	static const char *prefix = "Program: ";
	archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_add_filter_program");
	data = calloc(1, sizeof(*data));
	if (data == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Out of memory");
		return (ARCHIVE_FATAL);
	}
	data->cmd = strdup(cmd);
	data->description = (char *)malloc(strlen(prefix) + strlen(cmd) + 1);
	strcpy(data->description, prefix);
	strcat(data->description, cmd);

	f->name = data->description;
	f->data = data;
	f->open = &archive_compressor_program_open;
	f->code = ARCHIVE_COMPRESSION_PROGRAM;
	return (ARCHIVE_OK);
}

/*
 * Setup callback.
 */
static int
archive_compressor_program_open(struct archive_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;
	int ret;

	ret = __archive_write_open_filter(f->next_filter);
	if (ret != ARCHIVE_OK)
		return (ret);

	if (data->child_buf == NULL) {
		data->child_buf_len = 65536;
		data->child_buf_avail = 0;
		data->child_buf = malloc(data->child_buf_len);

		if (data->child_buf == NULL) {
			archive_set_error(f->archive, ENOMEM,
			    "Can't allocate compression buffer");
			return (ARCHIVE_FATAL);
		}
	}

	if ((data->child = __archive_create_child(data->cmd,
		 &data->child_stdin, &data->child_stdout)) == -1) {
		archive_set_error(f->archive, EINVAL,
		    "Can't initialise filter");
		return (ARCHIVE_FATAL);
	}

	f->write = archive_compressor_program_write;
	f->close = archive_compressor_program_close;
	f->free = archive_compressor_program_free;
	return (0);
}

static ssize_t
child_write(struct archive_write_filter *f, const char *buf, size_t buf_len)
{
	struct private_data *data = f->data;
	ssize_t ret;

	if (data->child_stdin == -1)
		return (-1);

	if (buf_len == 0)
		return (-1);

restart_write:
	do {
		ret = write(data->child_stdin, buf, buf_len);
	} while (ret == -1 && errno == EINTR);

	if (ret > 0)
		return (ret);
	if (ret == 0) {
		close(data->child_stdin);
		data->child_stdin = -1;
		fcntl(data->child_stdout, F_SETFL, 0);
		return (0);
	}
	if (ret == -1 && errno != EAGAIN)
		return (-1);

	if (data->child_stdout == -1) {
		fcntl(data->child_stdin, F_SETFL, 0);
		__archive_check_child(data->child_stdin, data->child_stdout);
		goto restart_write;
	}

	do {
		ret = read(data->child_stdout,
		    data->child_buf + data->child_buf_avail,
		    data->child_buf_len - data->child_buf_avail);
	} while (ret == -1 && errno == EINTR);

	if (ret == 0 || (ret == -1 && errno == EPIPE)) {
		close(data->child_stdout);
		data->child_stdout = -1;
		fcntl(data->child_stdin, F_SETFL, 0);
		goto restart_write;
	}
	if (ret == -1 && errno == EAGAIN) {
		__archive_check_child(data->child_stdin, data->child_stdout);
		goto restart_write;
	}
	if (ret == -1)
		return (-1);

	data->child_buf_avail += ret;

	ret = __archive_write_filter(f->next_filter,
	    data->child_buf, data->child_buf_avail);
	if (ret <= 0)
		return (-1);

	if ((size_t)ret < data->child_buf_avail) {
		memmove(data->child_buf, data->child_buf + ret,
		    data->child_buf_avail - ret);
	}
	data->child_buf_avail -= ret;
	goto restart_write;
}

/*
 * Write data to the compressed stream.
 */
static int
archive_compressor_program_write(struct archive_write_filter *f,
    const void *buff, size_t length)
{
	ssize_t ret;
	const char *buf;

	buf = buff;
	while (length > 0) {
		ret = child_write(f, buf, length);
		if (ret == -1 || ret == 0) {
			archive_set_error(f->archive, EIO,
			    "Can't write to filter");
			return (ARCHIVE_FATAL);
		}
		length -= ret;
		buf += ret;
	}
	return (ARCHIVE_OK);
}


/*
 * Finish the compression...
 */
static int
archive_compressor_program_close(struct archive_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;
	int ret, r1, status;
	ssize_t bytes_read;

	ret = 0;
	close(data->child_stdin);
	data->child_stdin = -1;
	fcntl(data->child_stdout, F_SETFL, 0);

	for (;;) {
		do {
			bytes_read = read(data->child_stdout,
			    data->child_buf + data->child_buf_avail,
			    data->child_buf_len - data->child_buf_avail);
		} while (bytes_read == -1 && errno == EINTR);

		if (bytes_read == 0 || (bytes_read == -1 && errno == EPIPE))
			break;

		if (bytes_read == -1) {
			archive_set_error(f->archive, errno,
			    "Read from filter failed unexpectedly.");
			ret = ARCHIVE_FATAL;
			goto cleanup;
		}
		data->child_buf_avail += bytes_read;

		ret = __archive_write_filter(f->next_filter,
		    data->child_buf, data->child_buf_avail);
		if (ret != ARCHIVE_OK) {
			ret = ARCHIVE_FATAL;
			goto cleanup;
		}
		data->child_buf_avail = 0;
	}

cleanup:
	/* Shut down the child. */
	if (data->child_stdin != -1)
		close(data->child_stdin);
	if (data->child_stdout != -1)
		close(data->child_stdout);
	while (waitpid(data->child, &status, 0) == -1 && errno == EINTR)
		continue;

	if (status != 0) {
		archive_set_error(f->archive, EIO,
		    "Filter exited with failure.");
		ret = ARCHIVE_FATAL;
	}
	r1 = __archive_write_close_filter(f->next_filter);
	return (r1 < ret ? r1 : ret);
}

static int
archive_compressor_program_free(struct archive_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;
	free(data->cmd);
	free(data->description);
	free(data->child_buf);
	free(data);
	f->data = NULL;
	return (ARCHIVE_OK);
}

#endif /* !defined(HAVE_PIPE) || !defined(HAVE_VFORK) || !defined(HAVE_FCNTL) */
