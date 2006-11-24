/*-
 * Copyright (c) 2003-2006 Tim Kientzle
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

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
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

struct read_FILE_data {
	FILE    *f;
	size_t	 block_size;
	void	*buffer;
};

static int	file_close(struct archive *, void *);
static int	file_open(struct archive *, void *);
static ssize_t	file_read(struct archive *, void *, const void **buff);
static ssize_t	file_skip(struct archive *, void *, size_t request);

int
archive_read_open_FILE(struct archive *a, FILE *f)
{
	struct read_FILE_data *mine;

	mine = (struct read_FILE_data *)malloc(sizeof(*mine));
	if (mine == NULL) {
		archive_set_error(a, ENOMEM, "No memory");
		return (ARCHIVE_FATAL);
	}
	mine->block_size = 128 * 1024;
	mine->buffer = malloc(mine->block_size);
	if (mine->buffer == NULL) {
		archive_set_error(a, ENOMEM, "No memory");
		free(mine);
		return (ARCHIVE_FATAL);
	}
	mine->f = f;
	return (archive_read_open2(a, mine, file_open, file_read,
		    file_skip, file_close));
}

static int
file_open(struct archive *a, void *client_data)
{
	struct read_FILE_data *mine = (struct read_FILE_data *)client_data;
	struct stat st;

	/*
	 * If we can't fstat() the file, it may just be that
	 * it's not a file.  (FILE * objects can wrap many kinds
	 * of I/O streams.)
	 */
	if (fstat(fileno(mine->f), &st) == 0 && S_ISREG(st.st_mode))
		archive_read_extract_set_skip_file(a, st.st_dev, st.st_ino);

	return (ARCHIVE_OK);
}

static ssize_t
file_read(struct archive *a, void *client_data, const void **buff)
{
	struct read_FILE_data *mine = (struct read_FILE_data *)client_data;
	ssize_t bytes_read;

	*buff = mine->buffer;
	bytes_read = fread(mine->buffer, 1, mine->block_size, mine->f);
	if (bytes_read < 0) {
		archive_set_error(a, errno, "Error reading file");
	}
	return (bytes_read);
}

static ssize_t
file_skip(struct archive *a, void *client_data, size_t request)
{
	struct read_FILE_data *mine = (struct read_FILE_data *)client_data;

	/*
	 * Note: the 'fd' and 'filename' versions round the request
	 * down to a multiple of the block size to ensure proper
	 * operation on block-oriented media such as tapes.  But stdio
	 * doesn't work with such media (it doesn't ensure blocking),
	 * so we don't need to bother.
	 */
#if HAVE_FSEEKO
	if (fseeko(mine->f, request, SEEK_CUR) != 0)
#else
	if (fseek(mine->f, request, SEEK_CUR) != 0)
#endif
	{
		archive_set_error(a, errno, "Error skipping forward");
		return (ARCHIVE_FATAL);
	}
	return (request);
}

static int
file_close(struct archive *a, void *client_data)
{
	struct read_FILE_data *mine = (struct read_FILE_data *)client_data;

	(void)a; /* UNUSED */
	if (mine->buffer != NULL)
		free(mine->buffer);
	free(mine);
	return (ARCHIVE_OK);
}
