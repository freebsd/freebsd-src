/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Functions for manipulating fifo buffers (that can grow if needed).
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: buffer.c,v 1.8 2000/09/07 20:27:50 deraadt Exp $");

#include "xmalloc.h"
#include "buffer.h"
#include "ssh.h"

/* Initializes the buffer structure. */

void
buffer_init(Buffer *buffer)
{
	buffer->alloc = 4096;
	buffer->buf = xmalloc(buffer->alloc);
	buffer->offset = 0;
	buffer->end = 0;
}

/* Frees any memory used for the buffer. */

void
buffer_free(Buffer *buffer)
{
	memset(buffer->buf, 0, buffer->alloc);
	xfree(buffer->buf);
}

/*
 * Clears any data from the buffer, making it empty.  This does not actually
 * zero the memory.
 */

void
buffer_clear(Buffer *buffer)
{
	buffer->offset = 0;
	buffer->end = 0;
}

/* Appends data to the buffer, expanding it if necessary. */

void
buffer_append(Buffer *buffer, const char *data, unsigned int len)
{
	char *cp;
	buffer_append_space(buffer, &cp, len);
	memcpy(cp, data, len);
}

/*
 * Appends space to the buffer, expanding the buffer if necessary. This does
 * not actually copy the data into the buffer, but instead returns a pointer
 * to the allocated region.
 */

void
buffer_append_space(Buffer *buffer, char **datap, unsigned int len)
{
	u_int	newlen;

	/* If the buffer is empty, start using it from the beginning. */
	if (buffer->offset == buffer->end) {
		buffer->offset = 0;
		buffer->end = 0;
	}
restart:
	/* If there is enough space to store all data, store it now. */
	if (buffer->end + len < buffer->alloc) {
		*datap = buffer->buf + buffer->end;
		buffer->end += len;
		return;
	}
	/*
	 * If the buffer is quite empty, but all data is at the end, move the
	 * data to the beginning and retry.
	 */
	if (buffer->offset > buffer->alloc / 2) {
		memmove(buffer->buf, buffer->buf + buffer->offset,
			buffer->end - buffer->offset);
		buffer->end -= buffer->offset;
		buffer->offset = 0;
		goto restart;
	}
	/* Increase the size of the buffer and retry. */
	newlen = buffer->alloc + len + 32768;
	if (newlen > 0xa00000)
		fatal("buffer_append_space: alloc %u not supported",
		    newlen);
	buffer->buf = xrealloc(buffer->buf, newlen);
	buffer->alloc = newlen;
	goto restart;
}

/* Returns the number of bytes of data in the buffer. */

unsigned int
buffer_len(Buffer *buffer)
{
	return buffer->end - buffer->offset;
}

/* Gets data from the beginning of the buffer. */

void
buffer_get(Buffer *buffer, char *buf, unsigned int len)
{
	if (len > buffer->end - buffer->offset)
		fatal("buffer_get: trying to get more bytes than in buffer");
	memcpy(buf, buffer->buf + buffer->offset, len);
	buffer->offset += len;
}

/* Consumes the given number of bytes from the beginning of the buffer. */

void
buffer_consume(Buffer *buffer, unsigned int bytes)
{
	if (bytes > buffer->end - buffer->offset)
		fatal("buffer_consume: trying to get more bytes than in buffer");
	buffer->offset += bytes;
}

/* Consumes the given number of bytes from the end of the buffer. */

void
buffer_consume_end(Buffer *buffer, unsigned int bytes)
{
	if (bytes > buffer->end - buffer->offset)
		fatal("buffer_consume_end: trying to get more bytes than in buffer");
	buffer->end -= bytes;
}

/* Returns a pointer to the first used byte in the buffer. */

char *
buffer_ptr(Buffer *buffer)
{
	return buffer->buf + buffer->offset;
}

/* Dumps the contents of the buffer to stderr. */

void
buffer_dump(Buffer *buffer)
{
	int i;
	unsigned char *ucp = (unsigned char *) buffer->buf;

	for (i = buffer->offset; i < buffer->end; i++)
		fprintf(stderr, " %02x", ucp[i]);
	fprintf(stderr, "\n");
}
