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
RCSID("$OpenBSD: bufaux.c,v 1.36 2005/06/17 02:44:32 djm Exp $");

#include <openssl/bn.h>
#include "bufaux.h"
#include "xmalloc.h"
#include "getput.h"
#include "log.h"

/*
 * Stores an BIGNUM in the buffer with a 2-byte msb first bit count, followed
 * by (bits+7)/8 bytes of binary data, msb first.
 */
int
buffer_put_bignum_ret(Buffer *buffer, const BIGNUM *value)
{
	int bits = BN_num_bits(value);
	int bin_size = (bits + 7) / 8;
	u_char *buf = xmalloc(bin_size);
	int oi;
	char msg[2];

	/* Get the value of in binary */
	oi = BN_bn2bin(value, buf);
	if (oi != bin_size) {
		error("buffer_put_bignum_ret: BN_bn2bin() failed: oi %d != bin_size %d",
		    oi, bin_size);
		return (-1);
	}

	/* Store the number of bits in the buffer in two bytes, msb first. */
	PUT_16BIT(msg, bits);
	buffer_append(buffer, msg, 2);
	/* Store the binary data. */
	buffer_append(buffer, (char *)buf, oi);

	memset(buf, 0, bin_size);
	xfree(buf);

	return (0);
}

void
buffer_put_bignum(Buffer *buffer, const BIGNUM *value)
{
	if (buffer_put_bignum_ret(buffer, value) == -1)
		fatal("buffer_put_bignum: buffer error");
}

/*
 * Retrieves an BIGNUM from the buffer.
 */
int
buffer_get_bignum_ret(Buffer *buffer, BIGNUM *value)
{
	u_int bits, bytes;
	u_char buf[2], *bin;

	/* Get the number for bits. */
	if (buffer_get_ret(buffer, (char *) buf, 2) == -1) {
		error("buffer_get_bignum_ret: invalid length");
		return (-1);
	}
	bits = GET_16BIT(buf);
	/* Compute the number of binary bytes that follow. */
	bytes = (bits + 7) / 8;
	if (bytes > 8 * 1024) {
		error("buffer_get_bignum_ret: cannot handle BN of size %d", bytes);
		return (-1);
	}
	if (buffer_len(buffer) < bytes) {
		error("buffer_get_bignum_ret: input buffer too small");
		return (-1);
	}
	bin = buffer_ptr(buffer);
	BN_bin2bn(bin, bytes, value);
	if (buffer_consume_ret(buffer, bytes) == -1) {
		error("buffer_get_bignum_ret: buffer_consume failed");
		return (-1);
	}
	return (0);
}

void
buffer_get_bignum(Buffer *buffer, BIGNUM *value)
{
	if (buffer_get_bignum_ret(buffer, value) == -1)
		fatal("buffer_get_bignum: buffer error");
}

/*
 * Stores an BIGNUM in the buffer in SSH2 format.
 */
int
buffer_put_bignum2_ret(Buffer *buffer, const BIGNUM *value)
{
	u_int bytes;
	u_char *buf;
	int oi;
	u_int hasnohigh = 0;

	if (BN_is_zero(value)) {
		buffer_put_int(buffer, 0);
		return 0;
	}
	if (value->neg) {
		error("buffer_put_bignum2_ret: negative numbers not supported");
		return (-1);
	}
	bytes = BN_num_bytes(value) + 1; /* extra padding byte */
	if (bytes < 2) {
		error("buffer_put_bignum2_ret: BN too small");
		return (-1);
	}
	buf = xmalloc(bytes);
	buf[0] = 0x00;
	/* Get the value of in binary */
	oi = BN_bn2bin(value, buf+1);
	if (oi < 0 || (u_int)oi != bytes - 1) {
		error("buffer_put_bignum2_ret: BN_bn2bin() failed: "
		    "oi %d != bin_size %d", oi, bytes);
		xfree(buf);
		return (-1);
	}
	hasnohigh = (buf[1] & 0x80) ? 0 : 1;
	buffer_put_string(buffer, buf+hasnohigh, bytes-hasnohigh);
	memset(buf, 0, bytes);
	xfree(buf);
	return (0);
}

void
buffer_put_bignum2(Buffer *buffer, const BIGNUM *value)
{
	if (buffer_put_bignum2_ret(buffer, value) == -1)
		fatal("buffer_put_bignum2: buffer error");
}

int
buffer_get_bignum2_ret(Buffer *buffer, BIGNUM *value)
{
	u_int len;
	u_char *bin;

	if ((bin = buffer_get_string_ret(buffer, &len)) == NULL) {
		error("buffer_get_bignum2_ret: invalid bignum");
		return (-1);
	}

	if (len > 0 && (bin[0] & 0x80)) {
		error("buffer_get_bignum2_ret: negative numbers not supported");
		return (-1);
	}
	if (len > 8 * 1024) {
		error("buffer_get_bignum2_ret: cannot handle BN of size %d", len);
		return (-1);
	}
	BN_bin2bn(bin, len, value);
	xfree(bin);
	return (0);
}

void
buffer_get_bignum2(Buffer *buffer, BIGNUM *value)
{
	if (buffer_get_bignum2_ret(buffer, value) == -1)
		fatal("buffer_get_bignum2: buffer error");
}

/*
 * Returns integers from the buffer (msb first).
 */

int
buffer_get_short_ret(u_short *ret, Buffer *buffer)
{
	u_char buf[2];

	if (buffer_get_ret(buffer, (char *) buf, 2) == -1)
		return (-1);
	*ret = GET_16BIT(buf);
	return (0);
}

u_short
buffer_get_short(Buffer *buffer)
{
	u_short ret;

	if (buffer_get_short_ret(&ret, buffer) == -1)
		fatal("buffer_get_short: buffer error");

	return (ret);
}

int
buffer_get_int_ret(u_int *ret, Buffer *buffer)
{
	u_char buf[4];

	if (buffer_get_ret(buffer, (char *) buf, 4) == -1)
		return (-1);
	*ret = GET_32BIT(buf);
	return (0);
}

u_int
buffer_get_int(Buffer *buffer)
{
	u_int ret;

	if (buffer_get_int_ret(&ret, buffer) == -1)
		fatal("buffer_get_int: buffer error");

	return (ret);
}

int
buffer_get_int64_ret(u_int64_t *ret, Buffer *buffer)
{
	u_char buf[8];

	if (buffer_get_ret(buffer, (char *) buf, 8) == -1)
		return (-1);
	*ret = GET_64BIT(buf);
	return (0);
}

u_int64_t
buffer_get_int64(Buffer *buffer)
{
	u_int64_t ret;

	if (buffer_get_int64_ret(&ret, buffer) == -1)
		fatal("buffer_get_int: buffer error");

	return (ret);
}

/*
 * Stores integers in the buffer, msb first.
 */
void
buffer_put_short(Buffer *buffer, u_short value)
{
	char buf[2];

	PUT_16BIT(buf, value);
	buffer_append(buffer, buf, 2);
}

void
buffer_put_int(Buffer *buffer, u_int value)
{
	char buf[4];

	PUT_32BIT(buf, value);
	buffer_append(buffer, buf, 4);
}

void
buffer_put_int64(Buffer *buffer, u_int64_t value)
{
	char buf[8];

	PUT_64BIT(buf, value);
	buffer_append(buffer, buf, 8);
}

/*
 * Returns an arbitrary binary string from the buffer.  The string cannot
 * be longer than 256k.  The returned value points to memory allocated
 * with xmalloc; it is the responsibility of the calling function to free
 * the data.  If length_ptr is non-NULL, the length of the returned data
 * will be stored there.  A null character will be automatically appended
 * to the returned string, and is not counted in length.
 */
void *
buffer_get_string_ret(Buffer *buffer, u_int *length_ptr)
{
	u_char *value;
	u_int len;

	/* Get the length. */
	len = buffer_get_int(buffer);
	if (len > 256 * 1024) {
		error("buffer_get_string_ret: bad string length %u", len);
		return (NULL);
	}
	/* Allocate space for the string.  Add one byte for a null character. */
	value = xmalloc(len + 1);
	/* Get the string. */
	if (buffer_get_ret(buffer, value, len) == -1) {
		error("buffer_get_string_ret: buffer_get failed");
		xfree(value);
		return (NULL);
	}
	/* Append a null character to make processing easier. */
	value[len] = 0;
	/* Optionally return the length of the string. */
	if (length_ptr)
		*length_ptr = len;
	return (value);
}

void *
buffer_get_string(Buffer *buffer, u_int *length_ptr)
{
	void *ret;

	if ((ret = buffer_get_string_ret(buffer, length_ptr)) == NULL)
		fatal("buffer_get_string: buffer error");
	return (ret);
}

/*
 * Stores and arbitrary binary string in the buffer.
 */
void
buffer_put_string(Buffer *buffer, const void *buf, u_int len)
{
	buffer_put_int(buffer, len);
	buffer_append(buffer, buf, len);
}
void
buffer_put_cstring(Buffer *buffer, const char *s)
{
	if (s == NULL)
		fatal("buffer_put_cstring: s == NULL");
	buffer_put_string(buffer, s, strlen(s));
}

/*
 * Returns a character from the buffer (0 - 255).
 */
int
buffer_get_char_ret(char *ret, Buffer *buffer)
{
	if (buffer_get_ret(buffer, ret, 1) == -1) {
		error("buffer_get_char_ret: buffer_get_ret failed");
		return (-1);
	}
	return (0);
}

int
buffer_get_char(Buffer *buffer)
{
	char ch;

	if (buffer_get_char_ret(&ch, buffer) == -1)
		fatal("buffer_get_char: buffer error");
	return (u_char) ch;
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
