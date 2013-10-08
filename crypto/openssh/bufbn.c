/* $OpenBSD: bufbn.c,v 1.7 2013/05/17 00:13:13 djm Exp $*/
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

#include <sys/types.h>

#include <openssl/bn.h>

#include <string.h>
#include <stdarg.h>

#include "xmalloc.h"
#include "buffer.h"
#include "log.h"
#include "misc.h"

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
		free(buf);
		return (-1);
	}

	/* Store the number of bits in the buffer in two bytes, msb first. */
	put_u16(msg, bits);
	buffer_append(buffer, msg, 2);
	/* Store the binary data. */
	buffer_append(buffer, buf, oi);

	memset(buf, 0, bin_size);
	free(buf);

	return (0);
}

void
buffer_put_bignum(Buffer *buffer, const BIGNUM *value)
{
	if (buffer_put_bignum_ret(buffer, value) == -1)
		fatal("buffer_put_bignum: buffer error");
}

/*
 * Retrieves a BIGNUM from the buffer.
 */
int
buffer_get_bignum_ret(Buffer *buffer, BIGNUM *value)
{
	u_int bits, bytes;
	u_char buf[2], *bin;

	/* Get the number of bits. */
	if (buffer_get_ret(buffer, (char *) buf, 2) == -1) {
		error("buffer_get_bignum_ret: invalid length");
		return (-1);
	}
	bits = get_u16(buf);
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
	if (BN_bin2bn(bin, bytes, value) == NULL) {
		error("buffer_get_bignum_ret: BN_bin2bn failed");
		return (-1);
	}
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
 * Stores a BIGNUM in the buffer in SSH2 format.
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
		free(buf);
		return (-1);
	}
	hasnohigh = (buf[1] & 0x80) ? 0 : 1;
	buffer_put_string(buffer, buf+hasnohigh, bytes-hasnohigh);
	memset(buf, 0, bytes);
	free(buf);
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
		free(bin);
		return (-1);
	}
	if (len > 8 * 1024) {
		error("buffer_get_bignum2_ret: cannot handle BN of size %d",
		    len);
		free(bin);
		return (-1);
	}
	if (BN_bin2bn(bin, len, value) == NULL) {
		error("buffer_get_bignum2_ret: BN_bin2bn failed");
		free(bin);
		return (-1);
	}
	free(bin);
	return (0);
}

void
buffer_get_bignum2(Buffer *buffer, BIGNUM *value)
{
	if (buffer_get_bignum2_ret(buffer, value) == -1)
		fatal("buffer_get_bignum2: buffer error");
}
