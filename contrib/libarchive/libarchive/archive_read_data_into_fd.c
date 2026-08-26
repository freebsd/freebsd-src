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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_read_private.h"

/* Maximum amount of data to write at one time. */
#define	MAX_WRITE	(1024 * 1024)

/*
 * This implementation minimizes copying of data and is sparse-file aware.
 */
static int
pad_to(struct archive *a, int fd, int can_lseek,
    char **nulls, int64_t target_offset, int64_t actual_offset)
{
	const size_t nulls_size = 16384;
	size_t to_write;
	ssize_t bytes_written;

	if (can_lseek) {
		actual_offset = lseek(fd,
		    target_offset - actual_offset, SEEK_CUR);
		if (actual_offset != target_offset) {
			archive_set_error(a,
			    actual_offset == -1 ? errno : ARCHIVE_ERRNO_MISC,
			    "Seek error");
			return (ARCHIVE_FATAL);
		}
		return (ARCHIVE_OK);
	}
	if (*nulls == NULL) {
		*nulls = calloc(1, nulls_size);
		if (*nulls == NULL) {
			archive_set_error(a, errno, "Out of memory");
			return (ARCHIVE_FATAL);
		}
	}

	while (target_offset > actual_offset) {
		to_write = nulls_size;
		if (target_offset < actual_offset + (int64_t)nulls_size)
			to_write = (size_t)(target_offset - actual_offset);
		bytes_written = write(fd, *nulls, to_write);
		if (bytes_written < 0) {
			archive_set_error(a, errno, "Write error");
			return (ARCHIVE_FATAL);
		}
		actual_offset += bytes_written;
	}
	return (ARCHIVE_OK);
}


int
archive_read_data_into_fd(struct archive *a, int fd)
{
	struct stat st;
	int r, r2;
	const void *buff;
	size_t size, bytes_to_write;
	ssize_t bytes_written;
	int64_t fd_offset;
	int64_t target_offset;
	int64_t actual_offset = 0;
	int64_t declared_size;
	int can_lseek;
	char *nulls = NULL;

	archive_check_magic(a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_DATA,
	    "archive_read_data_into_fd");

	declared_size = ((struct archive_read *)a)->entry_bytes_declared;

	can_lseek = (fstat(fd, &st) == 0) && S_ISREG(st.st_mode);
	if (can_lseek) {
		fd_offset = lseek(fd, 0, SEEK_CUR);
		if (fd_offset == -1)
			can_lseek = 0;
	}

	while ((r = archive_read_data_block(a, &buff, &size, &target_offset)) ==
	    ARCHIVE_OK) {
		const char *p = buff;
		int overran = 0;

		if (declared_size >= 0) {
			/* We know the expected size, let's enforce that we don't overrun. */
			if (target_offset > declared_size) {
				/* We're already beyond the end? Don't write any more. */
				target_offset = declared_size;
				size = 0;
				overran = 1;
			} else if (size > (uint64_t)(declared_size - target_offset)) {
				/* Above is safe because target_offset <= declared_size */
				/* This block is bigger than the expected remainder */
				size = (size_t)(declared_size - target_offset);
				overran = 1;
			}
			/* Else size <= expected remainder of data and we're OK. */
		}

		if (target_offset > actual_offset) {
			r = pad_to(a, fd, can_lseek, &nulls,
			    target_offset, actual_offset);
			if (r != ARCHIVE_OK)
				break;
			actual_offset = target_offset;
		}
		while (size > 0) {
			bytes_to_write = size;
			if (bytes_to_write > MAX_WRITE)
				bytes_to_write = MAX_WRITE;
			bytes_written = write(fd, p, bytes_to_write);
			if (bytes_written < 0) {
				archive_set_error(a, errno, "Write error");
				r = ARCHIVE_FATAL;
				goto cleanup;
			}
			actual_offset += bytes_written;
			p += bytes_written;
			size -= bytes_written;
		}
		if (overran) {
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "Actual entry size exceeds the declared size "
			    "(%jd); truncated output at the declared size",
			    (intmax_t)declared_size);
			r = ARCHIVE_FAILED;
			goto cleanup;
		}
	}

	if (r == ARCHIVE_EOF && target_offset > actual_offset) {
		int trailing_hole_overran =
		    declared_size >= 0 && target_offset > declared_size;
		if (trailing_hole_overran)
			target_offset = declared_size;
		if (target_offset > actual_offset) {
			r2 = pad_to(a, fd, can_lseek, &nulls,
			    target_offset, actual_offset);
			if (r2 != ARCHIVE_OK)
				r = r2;
			else
				actual_offset = target_offset;
		}
		if (trailing_hole_overran) {
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "Actual entry size exceeds the declared size "
			    "(%jd); truncated output at the declared size",
			    (intmax_t)declared_size);
			r = ARCHIVE_FAILED;
			goto cleanup;
		}
	}

cleanup:
	free(nulls);
	if (r != ARCHIVE_EOF)
		return (r);
	r = ARCHIVE_OK;
	if (can_lseek) {
		int64_t offset = lseek(fd, 0, SEEK_CUR);
		if (offset - fd_offset != actual_offset) {
			archive_set_error(a,
			    offset == -1 ? errno : ARCHIVE_ERRNO_MISC,
			    "Seek error");
			r = ARCHIVE_FATAL;
		}
	}
	return (r);
}
