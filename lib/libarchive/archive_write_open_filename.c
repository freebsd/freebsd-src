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
#ifdef HAVE_DMALLOC
#include <dmalloc.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "archive.h"
#include "archive_private.h"

struct write_file_data {
	off_t		offset;
	int		fd;
	char		filename[1];
};

static int	file_close(struct archive *, void *);
static int	file_open(struct archive *, void *);
static ssize_t	file_write(struct archive *, void *, void *buff, size_t);

int
archive_write_open_file(struct archive *a, const char *filename)
{
	return (archive_write_open_file_position(a, filename, 0));
}

int
archive_write_open_file_position(struct archive *a, const char *filename,
    int64_t offset)
{
	struct write_file_data *mine;

	if (filename == NULL) {
		mine = malloc(sizeof(*mine));
		mine->filename[0] = 0;
	} else {
		mine = malloc(sizeof(*mine) + strlen(filename));
		strcpy(mine->filename, filename);
	}
	mine->offset = offset;
	mine->fd = -1;
	return (archive_write_open(a, mine,
		    file_open, file_write, file_close));
}

static int
file_open(struct archive *a, void *client_data)
{
	int flags;
	struct write_file_data *mine;
	struct stat st;

	mine = client_data;
	if (mine->offset == 0)
		flags = O_WRONLY | O_CREAT | O_TRUNC;
	else
		flags = O_WRONLY;

	if (*mine->filename != 0) {
		mine->fd = open(mine->filename, flags, 0666);

		/*
		 * If client hasn't explicitly set the last block
		 * handling, then set it here: If the output is a
		 * block or character device, pad the last block,
		 * otherwise leave it unpadded.
		 */
		if (mine->fd >= 0 && a->bytes_in_last_block < 0) {
			/* Last block will be fully padded. */
			fstat(mine->fd, &st);
			if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode) ||
			    S_ISFIFO(st.st_mode))
				archive_write_set_bytes_in_last_block(a, 0);
			else
				archive_write_set_bytes_in_last_block(a, 1);
		}
	} else {
		mine->fd = 1;
		if (a->bytes_in_last_block < 0) /* Still default? */
			/* Last block will be fully padded. */
			archive_write_set_bytes_in_last_block(a, 0);
	}

	if (mine->fd < 0) {
		archive_set_error(a, errno, "Failed to open");
		return -1;
	}

	if (mine->offset > 0) {
		lseek(mine->fd, mine->offset, SEEK_SET);
		ftruncate(mine->fd, mine->offset);
	}

	return (ARCHIVE_OK);
}

static ssize_t
file_write(struct archive *a, void *client_data, void *buff, size_t length)
{
	struct write_file_data	*mine;

	(void)a; /* UNUSED */
	mine = client_data;
	return (write(mine->fd, buff, length));
}

static int
file_close(struct archive *a, void *client_data)
{
	struct write_file_data	*mine = client_data;

	(void)a; /* UNUSED */
	if (mine->fd >= 0)
		close(mine->fd);
	free(mine);
	return (ARCHIVE_OK);
}
