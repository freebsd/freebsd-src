/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Auxiliary functions for storing and retrieving various data types to/from
 * Buffers.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 *
 * SSH2 packet format added by Markus Friedl
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"
RCSID("$OpenBSD: bufaux.c,v 1.13 2000/09/07 20:27:50 deraadt Exp $");
RCSID("$FreeBSD$");

#include "ssh.h"
#include <openssl/bn.h>
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
	char unsigned *buf = xmalloc(bin_size);
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
	buffer_append(buffer, (char *)buf, oi);

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
	bin = (unsigned char*) buffer_ptr(buffer);
	BN_bin2bn(bin, bytes, value);
	buffer_consume(buffer, bytes);

	return 2 + bytes;
}

/*
 * Stores an BIGNUM in the buffer in SSH2 format.
 */
void
buffer_put_bignum2(Buffer *buffer, BIGNUM *value)
{
	int bytes = BN_num_bytes(value) + 1;
	unsigned char *buf = xmalloc(bytes);
	int oi;
	int hasnohigh = 0;
	buf[0] = '\0';
	/* Get the value of in binary */
	oi = BN_bn2bin(value, buf+1);
	if (oi != bytes-1)
		fatal("buffer_put_bignum: BN_bn2bin() failed: oi %d != bin_size %d",
		      oi, bytes);
	hasnohigh = (buf[1] & 0x80) ? 0 : 1;
	if (value->neg) {
		/**XXX should be two's-complement */
		int i, carry;
		unsigned char *uc = buf;
		log("negativ!");
		for(i = bytes-1, carry = 1; i>=0; i--) {
			uc[i] ^= 0xff;
			if(carry)
				carry = !++uc[i];
		}
	}
	buffer_put_string(buffer, buf+hasnohigh, bytes-hasnohigh);
	memset(buf, 0, bytes);
	xfree(buf);
}

int
buffer_get_bignum2(Buffer *buffer, BIGNUM *value)
{
	/**XXX should be two's-complement */
	int len;
	unsigned char *bin = (unsigned char *)buffer_get_string(buffer, (unsigned int *)&len);
	BN_bin2bn(bin, len, value);
	xfree(bin);
	return len;
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
void
buffer_put_cstring(Buffer *buffer, const char *s)
{
	buffer_put_string(buffer, s, strlen(s));
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
