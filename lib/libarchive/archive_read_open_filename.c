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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "archive.h"

struct read_file_data {
	int	 fd;
	size_t	 block_size;
	void	*buffer;
	char	 filename[1];
};

static int	file_close(struct archive *, void *);
static int	file_open(struct archive *, void *);
static ssize_t	file_read(struct archive *, void *, const void **buff);

int
archive_read_open_file(struct archive *a, const char *filename,
    size_t block_size)
{
	struct read_file_data *mine;

	if (filename == NULL) {
		mine = malloc(sizeof(*mine));
		if (mine == NULL) {
			archive_set_error(a, ENOMEM, "No memory");
			return (ARCHIVE_FATAL);
		}
		mine->filename[0] = 0;
	} else {
		mine = malloc(sizeof(*mine) + strlen(filename));
		if (mine == NULL) {
			archive_set_error(a, ENOMEM, "No memory");
			return (ARCHIVE_FATAL);
		}
		strcpy(mine->filename, filename);
	}
	mine->block_size = block_size;
	mine->buffer = malloc(mine->block_size);
	mine->fd = -1;
	return (archive_read_open(a, mine, file_open, file_read, file_close));
}

static int
file_open(struct archive *a, void *client_data)
{
	struct read_file_data *mine = client_data;

	if (*mine->filename != 0)
		mine->fd = open(mine->filename, O_RDONLY);
	else
		mine->fd = 0;
	if (mine->fd < 0) {
		archive_set_error(a, errno, "Failed to open '%s'",
		    mine->filename);
		return (ARCHIVE_FATAL);
	}
	return (0);
}

static ssize_t
file_read(struct archive *a, void *client_data, const void **buff)
{
	struct read_file_data *mine = client_data;

	(void)a; /* UNUSED */
	*buff = mine->buffer;
	return (read(mine->fd, mine->buffer, mine->block_size));
}

static int
file_close(struct archive *a, void *client_data)
{
	struct read_file_data *mine = client_data;

	(void)a; /* UNUSED */
	if (mine->fd >= 0)
		close(mine->fd);
	free(mine);
	return (ARCHIVE_OK);
}
