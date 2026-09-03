/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <pjdlog.h>

#include "ebuf.h"

#ifndef	PJDLOG_ASSERT
#include <assert.h>
#define	PJDLOG_ASSERT(...)	assert(__VA_ARGS__)
#endif

#define	EBUF_MAGIC	0xeb0f41c
struct ebuf {
	/* Magic to assert the caller uses valid structure. */
	int		 eb_magic;
	/* Address where we did the allocation. */
	unsigned char	*eb_buf;
	/* Allocation end address. */
	unsigned char	*eb_end;
	/* Start of real data. */
	unsigned char	*eb_start;
	/* Size of real data. */
	size_t		 eb_size;
};

static int ebuf_head_extend(struct ebuf *eb, size_t size);
static int ebuf_tail_extend(struct ebuf *eb, size_t size);

/*
 * Allocate an empty ebuf with the expectation that it will later need to
 * hold at least `size` bytes.
 */
struct ebuf *
ebuf_alloc(size_t size)
{
	struct ebuf *eb;
	size_t page_size;
	int rerrno;

	eb = malloc(sizeof(*eb));
	if (eb == NULL)
		return (NULL);
	page_size = getpagesize();
	size += page_size;
	eb->eb_buf = malloc(size);
	if (eb->eb_buf == NULL) {
		rerrno = errno;
		free(eb);
		errno = rerrno;
		return (NULL);
	}
	eb->eb_end = eb->eb_buf + size;
	/*
	 * We set start address for real data not at the first entry, because
	 * we want to be able to add data at the front.
	 */
	eb->eb_start = eb->eb_buf + page_size / 4;
	eb->eb_size = 0;
	eb->eb_magic = EBUF_MAGIC;

	return (eb);
}

/*
 * Free `eb`.
 */
void
ebuf_free(struct ebuf *eb)
{

	PJDLOG_ASSERT(eb != NULL && eb->eb_magic == EBUF_MAGIC);

	eb->eb_magic = 0;

	free(eb->eb_buf);
	free(eb);
}

/*
 * Add `size` bytes to the front of `eb`, copied from `data` if not null
 * and otherwise left uninitialized.
 */
int
ebuf_add_head(struct ebuf *eb, const void *data, size_t size)
{

	PJDLOG_ASSERT(eb != NULL && eb->eb_magic == EBUF_MAGIC);

	if (size > (size_t)(eb->eb_start - eb->eb_buf)) {
		/*
		 * We can't add more entries at the front, so we have to extend
		 * our buffer.
		 */
		if (ebuf_head_extend(eb, size) == -1)
			return (-1);
	}
	PJDLOG_ASSERT(size <= (size_t)(eb->eb_start - eb->eb_buf));

	eb->eb_size += size;
	eb->eb_start -= size;
	/*
	 * If data is NULL the caller just wants to reserve place.
	 */
	if (data != NULL)
		memcpy(eb->eb_start, data, size);

	return (0);
}

/*
 * Add `size` bytes to the back of `eb`, copied from `data` if not null
 * and otherwise left uninitialized.
 */
int
ebuf_add_tail(struct ebuf *eb, const void *data, size_t size)
{

	PJDLOG_ASSERT(eb != NULL && eb->eb_magic == EBUF_MAGIC);

	if (size > (size_t)(eb->eb_end - (eb->eb_start + eb->eb_size))) {
		/*
		 * We can't add more entries at the back, so we have to extend
		 * our buffer.
		 */
		if (ebuf_tail_extend(eb, size) == -1)
			return (-1);
	}
	PJDLOG_ASSERT(size <=
	    (size_t)(eb->eb_end - (eb->eb_start + eb->eb_size)));

	/*
	 * If data is NULL the caller just wants to reserve space.
	 */
	if (data != NULL)
		memcpy(eb->eb_start + eb->eb_size, data, size);
	eb->eb_size += size;

	return (0);
}

/*
 * Trim `size` bytes from the front of `eb`.
 */
void
ebuf_del_head(struct ebuf *eb, size_t size)
{

	PJDLOG_ASSERT(eb != NULL && eb->eb_magic == EBUF_MAGIC);
	PJDLOG_ASSERT(size <= eb->eb_size);

	eb->eb_start += size;
	eb->eb_size -= size;
}

/*
 * Trim size bytes from the back of `eb`.
 */
void
ebuf_del_tail(struct ebuf *eb, size_t size)
{

	PJDLOG_ASSERT(eb != NULL && eb->eb_magic == EBUF_MAGIC);
	PJDLOG_ASSERT(size <= eb->eb_size);

	eb->eb_size -= size;
}

/*
 * Return a pointer to the data contained by `eb`.  The size of the data
 * is returned in `sizep` if not null.
 */
void *
ebuf_data(struct ebuf *eb, size_t *sizep)
{

	PJDLOG_ASSERT(eb != NULL && eb->eb_magic == EBUF_MAGIC);

	if (sizep != NULL)
		*sizep = eb->eb_size;
	return (eb->eb_size > 0 ? eb->eb_start : NULL);
}

/*
 * Return the size of the data contained in `eb`.
 */
size_t
ebuf_size(struct ebuf *eb)
{

	PJDLOG_ASSERT(eb != NULL && eb->eb_magic == EBUF_MAGIC);

	return (eb->eb_size);
}

/*
 * Function adds size + (PAGE_SIZE / 4) bytes at the front of the buffer..
 */
static int
ebuf_head_extend(struct ebuf *eb, size_t size)
{
	unsigned char *newbuf, *newstart;
	size_t newsize, page_size;

	PJDLOG_ASSERT(eb != NULL && eb->eb_magic == EBUF_MAGIC);

	page_size = getpagesize();
	newsize = eb->eb_end - eb->eb_buf + (page_size / 4) + size;

	newbuf = malloc(newsize);
	if (newbuf == NULL)
		return (-1);
	newstart =
	    newbuf + (page_size / 4) + size + (eb->eb_start - eb->eb_buf);

	memcpy(newstart, eb->eb_start, eb->eb_size);

	eb->eb_buf = newbuf;
	eb->eb_start = newstart;
	eb->eb_end = newbuf + newsize;

	return (0);
}

/*
 * Function adds size + ((3 * PAGE_SIZE) / 4) bytes at the back.
 */
static int
ebuf_tail_extend(struct ebuf *eb, size_t size)
{
	unsigned char *newbuf;
	size_t newsize, page_size;

	PJDLOG_ASSERT(eb != NULL && eb->eb_magic == EBUF_MAGIC);

	page_size = getpagesize();
	newsize = eb->eb_end - eb->eb_buf + size + ((3 * page_size) / 4);

	newbuf = realloc(eb->eb_buf, newsize);
	if (newbuf == NULL)
		return (-1);

	eb->eb_start = newbuf + (eb->eb_start - eb->eb_buf);
	eb->eb_buf = newbuf;
	eb->eb_end = newbuf + newsize;

	return (0);
}
