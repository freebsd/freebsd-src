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

#include "test.h"
__FBSDID("$FreeBSD$");

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/*
 * Read an archive from a block of memory.
 *
 * This is identical to archive_read_open_memory(), except
 * that it goes out of its way to be a little bit unpleasant,
 * in order to better test the libarchive internals.
 */

struct read_memory_data {
	unsigned char	*buffer;
	unsigned char	*end;
	size_t	 read_size;
	size_t copy_buff_size;
	char *copy_buff;
};

static int	memory_read_close(struct archive *, void *);
static int	memory_read_open(struct archive *, void *);
#if ARCHIVE_VERSION_NUMBER < 2000000
static ssize_t	memory_read_skip(struct archive *, void *, size_t request);
#else
static off_t	memory_read_skip(struct archive *, void *, off_t request);
#endif
static ssize_t	memory_read(struct archive *, void *, const void **buff);

int
read_open_memory(struct archive *a, void *buff, size_t size, size_t read_size)
{
	struct read_memory_data *mine;

	mine = (struct read_memory_data *)malloc(sizeof(*mine));
	if (mine == NULL) {
		archive_set_error(a, ENOMEM, "No memory");
		return (ARCHIVE_FATAL);
	}
	memset(mine, 0, sizeof(*mine));
	mine->buffer = (unsigned char *)buff;
	mine->end = mine->buffer + size;
	mine->read_size = read_size;
	mine->copy_buff_size = read_size + 64;
	mine->copy_buff = malloc(mine->copy_buff_size);
	return (archive_read_open2(a, mine, memory_read_open,
		    memory_read, memory_read_skip, memory_read_close));
}

/*
 * There's nothing to open.
 */
static int
memory_read_open(struct archive *a, void *client_data)
{
	(void)a; /* UNUSED */
	(void)client_data; /* UNUSED */
	return (ARCHIVE_OK);
}

/*
 * In order to exercise libarchive's internal read-combining logic,
 * we deliberately copy data for each read to a separate buffer.
 * That way, code that runs off the end of the provided data
 * will screw up.
 */
static ssize_t
memory_read(struct archive *a, void *client_data, const void **buff)
{
	struct read_memory_data *mine = (struct read_memory_data *)client_data;
	size_t size;

	(void)a; /* UNUSED */
	size = mine->end - mine->buffer;
	if (size > mine->read_size)
		size = mine->read_size;
	memset(mine->copy_buff, 0xA5, mine->copy_buff_size);
	memcpy(mine->copy_buff, mine->buffer, size);
	*buff = mine->copy_buff;

        mine->buffer += size;
	return (size);
}

/*
 * How mean can a skip() routine be?  Let's try to find out.
 */
#if ARCHIVE_VERSION_NUMBER < 2000000
static ssize_t
memory_read_skip(struct archive *a, void *client_data, size_t skip)
#else
static off_t
memory_read_skip(struct archive *a, void *client_data, off_t skip)
#endif
{
	struct read_memory_data *mine = (struct read_memory_data *)client_data;

	(void)a; /* UNUSED */
	/* We can't skip by more than is available. */
	if ((off_t)skip > (off_t)(mine->end - mine->buffer))
		skip = mine->end - mine->buffer;
	/* Always do small skips by prime amounts. */
	if (skip > 71)
		skip = 71;
	mine->buffer += skip;
	return (skip);
}

/*
 * Close is just cleaning up our one small bit of data.
 */
static int
memory_read_close(struct archive *a, void *client_data)
{
	struct read_memory_data *mine = (struct read_memory_data *)client_data;
	(void)a; /* UNUSED */
	free(mine->copy_buff);
	free(mine);
	return (ARCHIVE_OK);
}
