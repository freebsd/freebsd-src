/*-
 * Copyright (c) 2003-2004 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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

#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "archive.h"
#include "archive_private.h"

struct read_fd_data {
	int	 fd;
	size_t	 block_size;
	void	*buffer;
};

static int	file_close(struct archive *, void *);
static int	file_open(struct archive *, void *);
static ssize_t	file_read(struct archive *, void *, const void **buff);

int
archive_read_open_fd(struct archive *a, int fd, size_t block_size)
{
	struct read_fd_data *mine;

	mine = malloc(sizeof(*mine));
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
	return (archive_read_open(a, mine, file_open, file_read, file_close));
}

static int
file_open(struct archive *a, void *client_data)
{
	struct read_fd_data *mine = client_data;
	struct stat st;

	if (fstat(mine->fd, &st) != 0) {
		archive_set_error(a, errno, "Can't stat fd %d", mine->fd);
		return (ARCHIVE_FATAL);
	}

	a->skip_file_dev = st.st_dev;
	a->skip_file_ino = st.st_ino;
	return (ARCHIVE_OK);
}

static ssize_t
file_read(struct archive *a, void *client_data, const void **buff)
{
	struct read_fd_data *mine = client_data;

	(void)a; /* UNUSED */
	*buff = mine->buffer;
	return (read(mine->fd, mine->buffer, mine->block_size));
}

static int
file_close(struct archive *a, void *client_data)
{
	struct read_fd_data *mine = client_data;

	(void)a; /* UNUSED */
	free(mine->buffer);
	free(mine);
	return (ARCHIVE_OK);
}
