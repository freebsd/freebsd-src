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

#ifndef O_BINARY
#define O_BINARY 0
#endif

struct read_file_data {
	int	 fd;
	size_t	 block_size;
	void	*buffer;
	mode_t	 st_mode;  /* Mode bits for opened file. */
	char	 can_skip; /* This file supports skipping. */
	char	 filename[1]; /* Must be last! */
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
archive_read_open_file(struct archive *a, const char *filename,
    size_t block_size)
{
	return (archive_read_open_filename(a, filename, block_size));
}

int
archive_read_open_filename(struct archive *a, const char *filename,
    size_t block_size)
{
	struct read_file_data *mine;

	if (filename == NULL || filename[0] == '\0') {
		mine = (struct read_file_data *)malloc(sizeof(*mine));
		if (mine == NULL) {
			archive_set_error(a, ENOMEM, "No memory");
			return (ARCHIVE_FATAL);
		}
		mine->filename[0] = '\0';
	} else {
		mine = (struct read_file_data *)malloc(sizeof(*mine) + strlen(filename));
		if (mine == NULL) {
			archive_set_error(a, ENOMEM, "No memory");
			return (ARCHIVE_FATAL);
		}
		strcpy(mine->filename, filename);
	}
	mine->block_size = block_size;
	mine->buffer = NULL;
	mine->fd = -1;
	/* lseek() almost never works; disable it by default.  See below. */
	mine->can_skip = 0;
	return (archive_read_open2(a, mine, file_open, file_read, file_skip, file_close));
}

static int
file_open(struct archive *a, void *client_data)
{
	struct read_file_data *mine = (struct read_file_data *)client_data;
	struct stat st;

	mine->buffer = malloc(mine->block_size);
	if (mine->buffer == NULL) {
		archive_set_error(a, ENOMEM, "No memory");
		return (ARCHIVE_FATAL);
	}
	if (mine->filename[0] != '\0')
		mine->fd = open(mine->filename, O_RDONLY | O_BINARY);
	else
		mine->fd = 0; /* Fake "open" for stdin. */
	if (mine->fd < 0) {
		archive_set_error(a, errno, "Failed to open '%s'",
		    mine->filename);
		return (ARCHIVE_FATAL);
	}
	if (fstat(mine->fd, &st) == 0) {
		/* If we're reading a file from disk, ensure that we don't
		   overwrite it with an extracted file. */
		if (S_ISREG(st.st_mode)) {
			archive_read_extract_set_skip_file(a, st.st_dev, st.st_ino);
			/*
			 * Enabling skip here is a performance
			 * optimization for anything that supports
			 * lseek().  On FreeBSD, only regular files
			 * and raw disk devices support lseek() and
			 * there's no portable way to determine if a
			 * device is a raw disk device, so we only
			 * enable this optimization for regular files.
			 */
			mine->can_skip = 1;
		}
		/* Remember mode so close can decide whether to flush. */
		mine->st_mode = st.st_mode;
	} else {
		if (mine->filename[0] == '\0')
			archive_set_error(a, errno, "Can't stat stdin");
		else
			archive_set_error(a, errno, "Can't stat '%s'",
			    mine->filename);
		return (ARCHIVE_FATAL);
	}
	return (0);
}

static ssize_t
file_read(struct archive *a, void *client_data, const void **buff)
{
	struct read_file_data *mine = (struct read_file_data *)client_data;
	ssize_t bytes_read;

	*buff = mine->buffer;
	bytes_read = read(mine->fd, mine->buffer, mine->block_size);
	if (bytes_read < 0) {
		if (mine->filename[0] == '\0')
			archive_set_error(a, errno, "Error reading stdin");
		else
			archive_set_error(a, errno, "Error reading '%s'",
			    mine->filename);
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
	struct read_file_data *mine = (struct read_file_data *)client_data;
	off_t old_offset, new_offset;

	if (!mine->can_skip) /* We can't skip, so ... */
		return (0); /* ... skip zero bytes. */

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
		/* If skip failed once, it will probably fail again. */
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
		if (mine->filename[0] == '\0')
			/*
			 * Should never get here, since lseek() on stdin ought
			 * to return an ESPIPE error.
			 */
			archive_set_error(a, errno, "Error seeking in stdin");
		else
			archive_set_error(a, errno, "Error seeking in '%s'",
			    mine->filename);
		return (-1);
	}
	return (new_offset - old_offset);
}

static int
file_close(struct archive *a, void *client_data)
{
	struct read_file_data *mine = (struct read_file_data *)client_data;

	(void)a; /* UNUSED */

	/*
	 * Sometimes, we should flush the input before closing.
	 *   Regular files: faster to just close without flush.
	 *   Devices: must not flush (user might need to
	 *      read the "next" item on a non-rewind device).
	 *   Pipes and sockets:  must flush (otherwise, the
	 *      program feeding the pipe or socket may complain).
	 * Here, I flush everything except for regular files and
	 * device nodes.
	 */
	if (!S_ISREG(mine->st_mode)
	    && !S_ISCHR(mine->st_mode)
	    && !S_ISBLK(mine->st_mode)) {
		ssize_t bytesRead;
		do {
			bytesRead = read(mine->fd, mine->buffer,
			    mine->block_size);
		} while (bytesRead > 0);
	}
	/* If a named file was opened, then it needs to be closed. */
	if (mine->filename[0] != '\0')
		close(mine->fd);
	if (mine->buffer != NULL)
		free(mine->buffer);
	free(mine);
	return (ARCHIVE_OK);
}
