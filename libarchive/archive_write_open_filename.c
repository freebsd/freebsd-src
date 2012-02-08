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
__FBSDID("$FreeBSD: head/lib/libarchive/archive_write_open_filename.c 191165 2009-04-17 00:39:35Z kientzle $");

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
#include "archive_string.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

struct write_file_data {
	int		fd;
	char		mbs_filename;
	union {
		char		m[1];
		wchar_t		w[1];
	} filename; /* Must be last! */
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

	if (filename == NULL || filename[0] == '\0')
		return (archive_write_open_fd(a, 1));

	mine = (struct write_file_data *)malloc(sizeof(*mine) + strlen(filename));
	if (mine == NULL) {
		archive_set_error(a, ENOMEM, "No memory");
		return (ARCHIVE_FATAL);
	}
	strcpy(mine->filename.m, filename);
	mine->mbs_filename = 1;
	mine->fd = -1;
	return (archive_write_open(a, mine,
		file_open, file_write, file_close));
}

int
archive_write_open_filename_w(struct archive *a, const wchar_t *filename)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	struct write_file_data *mine;

	if (filename == NULL || filename[0] == L'\0')
		return (archive_write_open_fd(a, 1));

	mine = malloc(sizeof(*mine) + wcslen(filename) * sizeof(wchar_t));
	if (mine == NULL) {
		archive_set_error(a, ENOMEM, "No memory");
		return (ARCHIVE_FATAL);
	}
	wcscpy(mine->filename.w, filename);
	mine->mbs_filename = 0;
	mine->fd = -1;
	return (archive_write_open(a, mine,
		file_open, file_write, file_close));
#else
	/*
	 * POSIX system does not support a wchar_t interface for
	 * open() system call, so we have to translate a wchar_t
	 * filename to multi-byte one and use it.
	 */
	struct archive_string fn;
	int r;

	if (filename == NULL || filename[0] == L'\0')
		return (archive_write_open_fd(a, 1));

	archive_string_init(&fn);
	if (archive_string_append_from_wcs(&fn, filename,
	    wcslen(filename)) != 0) {
		archive_set_error(a, EINVAL,
		    "Failed to convert a wide-character filename to"
		    " a multi-byte filename");
		archive_string_free(&fn);
		return (ARCHIVE_FATAL);
	}
	r = archive_write_open_filename(a, fn.s);
	archive_string_free(&fn);
	return (r);
#endif
}


static int
file_open(struct archive *a, void *client_data)
{
	int flags;
	struct write_file_data *mine;
	struct stat st;

	mine = (struct write_file_data *)client_data;
	flags = O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;

	/*
	 * Open the file.
	 */
	if (mine->mbs_filename) {
		mine->fd = open(mine->filename.m, flags, 0666);
		if (mine->fd < 0) {
			archive_set_error(a, errno, "Failed to open '%s'",
			    mine->filename.m);
			return (ARCHIVE_FATAL);
		}

		if (fstat(mine->fd, &st) != 0) {
			archive_set_error(a, errno, "Couldn't stat '%s'",
			    mine->filename.m);
			return (ARCHIVE_FATAL);
		}
	} else {
#if defined(_WIN32) && !defined(__CYGWIN__)
		mine->fd = _wopen(mine->filename.w, flags, 0666);
		if (mine->fd < 0 && errno == ENOENT) {
			wchar_t *fullpath;
			fullpath = __la_win_permissive_name_w(mine->filename.w);
			if (fullpath != NULL) {
				mine->fd = _wopen(fullpath, flags, 0666);
				free(fullpath);
			}
		}
		if (mine->fd < 0) {
			archive_set_error(a, errno, "Failed to open '%S'",
			    mine->filename.w);
			return (ARCHIVE_FATAL);
		}

		if (fstat(mine->fd, &st) != 0) {
			archive_set_error(a, errno, "Couldn't stat '%S'",
			    mine->filename.w);
			return (ARCHIVE_FATAL);
		}
#else
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "Unexpedted operation in archive_write_open_filename");
		return (ARCHIVE_FATAL);
#endif
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
	for (;;) {
		bytesWritten = write(mine->fd, buff, length);
		if (bytesWritten <= 0) {
			if (errno == EINTR)
				continue;
			archive_set_error(a, errno, "Write error");
			return (-1);
		}
		return (bytesWritten);
	}
}

static int
file_close(struct archive *a, void *client_data)
{
	struct write_file_data	*mine = (struct write_file_data *)client_data;

	(void)a; /* UNUSED */
	close(mine->fd);
	free(mine);
	return (ARCHIVE_OK);
}
