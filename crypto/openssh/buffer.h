/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Code for manipulating FIFO buffers.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

/* RCSID("$OpenBSD: buffer.h,v 1.7 2000/12/19 23:17:55 markus Exp $"); */

#ifndef BUFFER_H
#define BUFFER_H

typedef struct {
	char   *buf;		/* Buffer for data. */
	u_int alloc;	/* Number of bytes allocated for data. */
	u_int offset;	/* Offset of first byte containing data. */
	u_int end;	/* Offset of last byte containing data. */
}       Buffer;
/* Initializes the buffer structure. */
void    buffer_init(Buffer * buffer);

/* Frees any memory used for the buffer. */
void    buffer_free(Buffer * buffer);

/* Clears any data from the buffer, making it empty.  This does not actually
   zero the memory. */
void    buffer_clear(Buffer * buffer);

/* Appends data to the buffer, expanding it if necessary. */
void    buffer_append(Buffer * buffer, const char *data, u_int len);

/*
 * Appends space to the buffer, expanding the buffer if necessary. This does
 * not actually copy the data into the buffer, but instead returns a pointer
 * to the allocated region.
 */
void    buffer_append_space(Buffer * buffer, char **datap, u_int len);

/* Returns the number of bytes of data in the buffer. */
u_int buffer_len(Buffer * buffer);

/* Gets data from the beginning of the buffer. */
void    buffer_get(Buffer * buffer, char *buf, u_int len);

/* Consumes the given number of bytes from the beginning of the buffer. */
void    buffer_consume(Buffer * buffer, u_int bytes);

/* Consumes the given number of bytes from the end of the buffer. */
void    buffer_consume_end(Buffer * buffer, u_int bytes);

/* Returns a pointer to the first used byte in the buffer. */
char   *buffer_ptr(Buffer * buffer);

/*
 * Dumps the contents of the buffer to stderr in hex.  This intended for
 * debugging purposes only.
 */
void    buffer_dump(Buffer * buffer);

#endif				/* BUFFER_H */
