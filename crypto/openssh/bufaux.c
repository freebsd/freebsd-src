/*
 * 
 * bufaux.c
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * 
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * 
 * Created: Wed Mar 29 02:24:47 1995 ylo
 * 
 * Auxiliary functions for storing and retrieving various data types to/from
 * Buffers.
 *
 */

#include "includes.h"
RCSID("$Id: bufaux.c,v 1.7 1999/11/24 19:53:44 markus Exp $");

#include "ssh.h"
#include <ssl/bn.h>
#include "bufaux.h"
#include "xmalloc.h"
#include "getput.h"

/*
 * Stores an BIGNUM in the buffer with a 2-byte msb first bit count, followed
 * by (bits+7)/8 bytes of binary data, msb first.
 */
void
buffer_put_bignum(Buffer *buffer, BIGNUM *value)
{
	int bits = BN_num_bits(value);
	int bin_size = (bits + 7) / 8;
	char *buf = xmalloc(bin_size);
	int oi;
	char msg[2];

	/* Get the value of in binary */
	oi = BN_bn2bin(value, buf);
	if (oi != bin_size)
		fatal("buffer_put_bignum: BN_bn2bin() failed: oi %d != bin_size %d",
		      oi, bin_size);

	/* Store the number of bits in the buffer in two bytes, msb first. */
	PUT_16BIT(msg, bits);
	buffer_append(buffer, msg, 2);
	/* Store the binary data. */
	buffer_append(buffer, buf, oi);

	memset(buf, 0, bin_size);
	xfree(buf);
}

/*
 * Retrieves an BIGNUM from the buffer.
 */
int
buffer_get_bignum(Buffer *buffer, BIGNUM *value)
{
	int bits, bytes;
	unsigned char buf[2], *bin;

	/* Get the number for bits. */
	buffer_get(buffer, (char *) buf, 2);
	bits = GET_16BIT(buf);
	/* Compute the number of binary bytes that follow. */
	bytes = (bits + 7) / 8;
	if (buffer_len(buffer) < bytes)
		fatal("buffer_get_bignum: input buffer too small");
	bin = buffer_ptr(buffer);
	BN_bin2bn(bin, bytes, value);
	buffer_consume(buffer, bytes);

	return 2 + bytes;
}

/*
 * Returns an integer from the buffer (4 bytes, msb first).
 */
unsigned int 
buffer_get_int(Buffer *buffer)
{
	unsigned char buf[4];
	buffer_get(buffer, (char *) buf, 4);
	return GET_32BIT(buf);
}

/*
 * Stores an integer in the buffer in 4 bytes, msb first.
 */
void 
buffer_put_int(Buffer *buffer, unsigned int value)
{
	char buf[4];
	PUT_32BIT(buf, value);
	buffer_append(buffer, buf, 4);
}

/*
 * Returns an arbitrary binary string from the buffer.  The string cannot
 * be longer than 256k.  The returned value points to memory allocated
 * with xmalloc; it is the responsibility of the calling function to free
 * the data.  If length_ptr is non-NULL, the length of the returned data
 * will be stored there.  A null character will be automatically appended
 * to the returned string, and is not counted in length.
 */
char *
buffer_get_string(Buffer *buffer, unsigned int *length_ptr)
{
	unsigned int len;
	char *value;
	/* Get the length. */
	len = buffer_get_int(buffer);
	if (len > 256 * 1024)
		fatal("Received packet with bad string length %d", len);
	/* Allocate space for the string.  Add one byte for a null character. */
	value = xmalloc(len + 1);
	/* Get the string. */
	buffer_get(buffer, value, len);
	/* Append a null character to make processing easier. */
	value[len] = 0;
	/* Optionally return the length of the string. */
	if (length_ptr)
		*length_ptr = len;
	return value;
}

/*
 * Stores and arbitrary binary string in the buffer.
 */
void 
buffer_put_string(Buffer *buffer, const void *buf, unsigned int len)
{
	buffer_put_int(buffer, len);
	buffer_append(buffer, buf, len);
}

/*
 * Returns a character from the buffer (0 - 255).
 */
int 
buffer_get_char(Buffer *buffer)
{
	char ch;
	buffer_get(buffer, &ch, 1);
	return (unsigned char) ch;
}

/*
 * Stores a character in the buffer.
 */
void 
buffer_put_char(Buffer *buffer, int value)
{
	char ch = value;
	buffer_append(buffer, &ch, 1);
}
