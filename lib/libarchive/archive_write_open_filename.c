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
__FBSDID("$FreeBSD: src/lib/libarchive/archive_write_open_filename.c,v 1.19 2007/01/09 08:05:56 kientzle Exp $");

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

struct write_file_data {
	int		fd;
	char		filename[1];
};

static int	file_close(struct archive *, void *);
static int	file_open(struct archive *, void *);
static ssize_t	file_write(struct archive *, void *, const void *buff, size_t);

int
archive_write_open_file(struct archive *a, const char *filename)
{
	return (archive_write_open_filename(a, filename));
}

int
archive_write_open_filename(struct archive *a, const char *filename)
{
	struct write_file_data *mine;

	if (filename == NULL || filename[0] == '\0') {
		mine = (struct write_file_data *)malloc(sizeof(*mine));
		if (mine == NULL) {
			archive_set_error(a, ENOMEM, "No memory");
			return (ARCHIVE_FATAL);
		}
		mine->filename[0] = '\0'; /* Record that we're using stdout. */
	} else {
		mine = (struct write_file_data *)malloc(sizeof(*mine) + strlen(filename));
		if (mine == NULL) {
			archive_set_error(a, ENOMEM, "No memory");
			return (ARCHIVE_FATAL);
		}
		strcpy(mine->filename, filename);
	}
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

	mine = (struct write_file_data *)client_data;
	flags = O_WRONLY | O_CREAT | O_TRUNC;

	/*
	 * Open the file.
	 */
	if (mine->filename[0] != '\0') {
		mine->fd = open(mine->filename, flags, 0666);
		if (mine->fd < 0) {
			archive_set_error(a, errno, "Failed to open '%s'",
			    mine->filename);
			return (ARCHIVE_FATAL);
		}
	} else {
		/*
		 * NULL filename is stdout.
		 */
		mine->fd = 1;
		/* By default, pad archive when writing to stdout. */
		if (archive_write_get_bytes_in_last_block(a) < 0)
			archive_write_set_bytes_in_last_block(a, 0);
	}

	if (fstat(mine->fd, &st) != 0) {
               archive_set_error(a, errno, "Couldn't stat '%s'",
                   mine->filename);
               return (ARCHIVE_FATAL);
	}

	/*
	 * Set up default last block handling.
	 */
	if (archive_write_get_bytes_in_last_block(a) < 0) {
		if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode) ||
		    S_ISFIFO(st.st_mode))
			/* Pad last block when writing to device or FIFO. */
			archive_write_set_bytes_in_last_block(a, 0);
		else
			/* Don't pad last block otherwise. */
			archive_write_set_bytes_in_last_block(a, 1);
	}

	/*
	 * If the output file is a regular file, don't add it to
	 * itself.  If it's a device file, it's okay to add the device
	 * entry to the output archive.
	 */
	if (S_ISREG(st.st_mode))
		archive_write_set_skip_file(a, st.st_dev, st.st_ino);

	return (ARCHIVE_OK);
}

static ssize_t
file_write(struct archive *a, void *client_data, const void *buff, size_t length)
{
	struct write_file_data	*mine;
	ssize_t	bytesWritten;

	mine = (struct write_file_data *)client_data;
	bytesWritten = write(mine->fd, buff, length);
	if (bytesWritten <= 0) {
		archive_set_error(a, errno, "Write error");
		return (-1);
	}
	return (bytesWritten);
}

static int
file_close(struct archive *a, void *client_data)
{
	struct write_file_data	*mine = (struct write_file_data *)client_data;

	(void)a; /* UNUSED */
	if (mine->filename[0] != '\0')
		close(mine->fd);
	free(mine);
	return (ARCHIVE_OK);
}
