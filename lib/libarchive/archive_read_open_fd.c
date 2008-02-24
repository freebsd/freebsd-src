/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
__FBSDID("$FreeBSD: src/lib/libarchive/archive_read_open_fd.c,v 1.13 2007/06/26 03:06:48 kientzle Exp $");

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "archive.h"

struct read_fd_data {
	int	 fd;
	size_t	 block_size;
	char	 can_skip;
	void	*buffer;
};

static int	file_close(struct archive *, void *);
static int	file_open(struct archive *, void *);
static ssize_t	file_read(struct archive *, void *, const void **buff);
#if ARCHIVE_API_VERSION < 2
static ssize_t	file_skip(struct archive *, void *, size_t request);
#else
static off_t	file_skip(struct archive *, void *, off_t request);
#endif

int
archive_read_open_fd(struct archive *a, int fd, size_t block_size)
{
	struct read_fd_data *mine;

	mine = (struct read_fd_data *)malloc(sizeof(*mine));
	if (mine == NULL) {
		archive_set_error(a, ENOMEM, "No memory");
		return (ARCHIVE_FATAL);
	}
	mine->block_size = block_size;
	mine->buffer = malloc(mine->block_size);
	if (mine->buffer == NULL) {
		archive_set_error(a, ENOMEM, "No memory");
		free(mine);
		return (ARCHIVE_FATAL);
	}
	mine->fd = fd;
	/* lseek() hardly ever works, so disable it by default.  See below. */
	mine->can_skip = 0;
	return (archive_read_open2(a, mine, file_open, file_read, file_skip, file_close));
}

static int
file_open(struct archive *a, void *client_data)
{
	struct read_fd_data *mine = (struct read_fd_data *)client_data;
	struct stat st;

	if (fstat(mine->fd, &st) != 0) {
		archive_set_error(a, errno, "Can't stat fd %d", mine->fd);
		return (ARCHIVE_FATAL);
	}

	if (S_ISREG(st.st_mode)) {
		archive_read_extract_set_skip_file(a, st.st_dev, st.st_ino);
		/*
		 * Enabling skip here is a performance optimization for
		 * anything that supports lseek().  On FreeBSD, only
		 * regular files and raw disk devices support lseek() and
		 * there's no portable way to determine if a device is
		 * a raw disk device, so we only enable this optimization
		 * for regular files.
		 */
		mine->can_skip = 1;
	}
	return (ARCHIVE_OK);
}

static ssize_t
file_read(struct archive *a, void *client_data, const void **buff)
{
	struct read_fd_data *mine = (struct read_fd_data *)client_data;
	ssize_t bytes_read;

	*buff = mine->buffer;
	bytes_read = read(mine->fd, mine->buffer, mine->block_size);
	if (bytes_read < 0) {
		archive_set_error(a, errno, "Error reading fd %d", mine->fd);
	}
	return (bytes_read);
}

#if ARCHIVE_API_VERSION < 2
static ssize_t
file_skip(struct archive *a, void *client_data, size_t request)
#else
static off_t
file_skip(struct archive *a, void *client_data, off_t request)
#endif
{
	struct read_fd_data *mine = (struct read_fd_data *)client_data;
	off_t old_offset, new_offset;

	if (!mine->can_skip)
		return (0);

	/* Reduce request to the next smallest multiple of block_size */
	request = (request / mine->block_size) * mine->block_size;
	if (request == 0)
		return (0);

	/*
	 * Hurray for lazy evaluation: if the first lseek fails, the second
	 * one will not be executed.
	 */
	if (((old_offset = lseek(mine->fd, 0, SEEK_CUR)) < 0) ||
	    ((new_offset = lseek(mine->fd, request, SEEK_CUR)) < 0))
	{
		/* If seek failed once, it will probably fail again. */
		mine->can_skip = 0;

		if (errno == ESPIPE)
		{
			/*
			 * Failure to lseek() can be caused by the file
			 * descriptor pointing to a pipe, socket or FIFO.
			 * Return 0 here, so the compression layer will use
			 * read()s instead to advance the file descriptor.
			 * It's slower of course, but works as well.
			 */
			return (0);
		}
		/*
		 * There's been an error other than ESPIPE. This is most
		 * likely caused by a programmer error (too large request)
		 * or a corrupted archive file.
		 */
		archive_set_error(a, errno, "Error seeking");
		return (-1);
	}
	return (new_offset - old_offset);
}

static int
file_close(struct archive *a, void *client_data)
{
	struct read_fd_data *mine = (struct read_fd_data *)client_data;

	(void)a; /* UNUSED */
	if (mine->buffer != NULL)
		free(mine->buffer);
	free(mine);
	return (ARCHIVE_OK);
}
