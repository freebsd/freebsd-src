/*
 * 
 * bufaux.h
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * 
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * 
 * Created: Wed Mar 29 02:18:23 1995 ylo
 * 
 */

/* RCSID("$Id: bufaux.h,v 1.4 1999/11/24 19:53:44 markus Exp $"); */

#ifndef BUFAUX_H
#define BUFAUX_H

#include "buffer.h"

/*
 * Stores an BIGNUM in the buffer with a 2-byte msb first bit count, followed
 * by (bits+7)/8 bytes of binary data, msb first.
 */
void    buffer_put_bignum(Buffer * buffer, BIGNUM * value);

/* Retrieves an BIGNUM from the buffer. */
int     buffer_get_bignum(Buffer * buffer, BIGNUM * value);

/* Returns an integer from the buffer (4 bytes, msb first). */
unsigned int buffer_get_int(Buffer * buffer);

/* Stores an integer in the buffer in 4 bytes, msb first. */
void    buffer_put_int(Buffer * buffer, unsigned int value);

/* Returns a character from the buffer (0 - 255). */
int     buffer_get_char(Buffer * buffer);

/* Stores a character in the buffer. */
void    buffer_put_char(Buffer * buffer, int value);

/*
 * Returns an arbitrary binary string from the buffer.  The string cannot be
 * longer than 256k.  The returned value points to memory allocated with
 * xmalloc; it is the responsibility of the calling function to free the
 * data.  If length_ptr is non-NULL, the length of the returned data will be
 * stored there.  A null character will be automatically appended to the
 * returned string, and is not counted in length.
 */
char   *buffer_get_string(Buffer * buffer, unsigned int *length_ptr);

/* Stores and arbitrary binary string in the buffer. */
void    buffer_put_string(Buffer * buffer, const void *buf, unsigned int len);

#endif				/* BUFAUX_H */
