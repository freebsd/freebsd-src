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

#include <sys/types.h>

#ifdef HAVE_DMALLOC
#include <dmalloc.h>
#endif
#include <unistd.h>

#include "archive.h"
#include "archive_private.h"

/*
 * This implementation minimizes copying of data.
 */
ssize_t
archive_read_data_into_fd(struct archive *a, int fd)
{
	ssize_t bytes_read, bytes_written, total_written;
	const void *buff;

	total_written = 0;
	while (a->entry_bytes_remaining > 0) {
		bytes_read = (a->compression_read_ahead)(a, &buff,
		    a->entry_bytes_remaining);
		if (bytes_read < 0)
			return (-1);
		if (bytes_read > a->entry_bytes_remaining)
			bytes_read = (ssize_t)a->entry_bytes_remaining;

		bytes_written = write(fd, buff, bytes_read);
		if (bytes_written < 0)
			return (-1);
		(a->compression_read_consume)(a, bytes_written);
		total_written += bytes_written;
		a->entry_bytes_remaining -= bytes_written;
	}
	return (total_written);
}
