/* $OpenBSD: deflate.c,v 1.3 2001/08/20 02:45:22 hugh Exp $ */

/*-
 * Copyright (c) 2001 Jean-Jacques Bernard-Gundol (jj@wabbitt.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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

/*
 * This file contains a wrapper around the deflate algo compression
 * functions using the zlib library (see net/zlib.{c,h})
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <net/zlib.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/deflate.h>

int window_inflate = -1 * MAX_WBITS;
int window_deflate = -12;

/*
 * This function takes a block of data and (de)compress it using the deflate
 * algorithm
 */

u_int32_t
deflate_global(data, size, decomp, out)
	u_int8_t *data;
	u_int32_t size;
	int decomp;
	u_int8_t **out;
{
	/* decomp indicates whether we compress (0) or decompress (1) */

	z_stream zbuf;
	u_int8_t *output;
	u_int32_t count, result;
	int error, i = 0, j;
	struct deflate_buf buf[ZBUF];

	bzero(&zbuf, sizeof(z_stream));
	for (j = 0; j < ZBUF; j++)
		buf[j].flag = 0;

	zbuf.next_in = data;	/* data that is going to be processed */
	zbuf.zalloc = z_alloc;
	zbuf.zfree = z_free;
	zbuf.opaque = Z_NULL;
	zbuf.avail_in = size;	/* Total length of data to be processed */

	if (!decomp) {
		buf[i].out = malloc((u_long) size, M_CRYPTO_DATA, 
		    M_NOWAIT);
		if (buf[i].out == NULL)
			goto bad;
		buf[i].size = size;
		buf[i].flag = 1;
		i++;
	} else {
		/*
	 	 * Choose a buffer with 4x the size of the input buffer
	 	 * for the size of the output buffer in the case of
	 	 * decompression. If it's not sufficient, it will need to be
	 	 * updated while the decompression is going on
	 	 */

		buf[i].out = malloc((u_long) (size * 4), 
		    M_CRYPTO_DATA, M_NOWAIT);
		if (buf[i].out == NULL)
			goto bad;
		buf[i].size = size * 4;
		buf[i].flag = 1;
		i++;
	}

	zbuf.next_out = buf[0].out;
	zbuf.avail_out = buf[0].size;

	error = decomp ? inflateInit2(&zbuf, window_inflate) :
	    deflateInit2(&zbuf, Z_DEFAULT_COMPRESSION, Z_METHOD,
		    window_deflate, Z_MEMLEVEL, Z_DEFAULT_STRATEGY);

	if (error != Z_OK)
		goto bad;
	for (;;) {
		error = decomp ? inflate(&zbuf, Z_SYNC_FLUSH) :
				 deflate(&zbuf, Z_FINISH);
		if (error != Z_OK && error != Z_STREAM_END)
			goto bad;
		if (decomp && zbuf.avail_in == 0 && error == Z_STREAM_END) {
			/* Done. */
			break;
		} else if (!decomp && error == Z_STREAM_END) {
			/* Done. */
			break;
		} else if (zbuf.avail_out == 0) {
			/* we need more output space, allocate size */
			buf[i].out = malloc((u_long) size,
			    M_CRYPTO_DATA, M_NOWAIT);
			if (buf[i].out == NULL)
				goto bad;
			zbuf.next_out = buf[i].out;
			buf[i].size = size;
			buf[i].flag = 1;
			zbuf.avail_out = buf[i].size;
			i++;
		} else {
			/* Unexpect result. */
			goto bad;
		}
	}

	result = count = zbuf.total_out;

	*out = malloc((u_long) result, M_CRYPTO_DATA, M_NOWAIT);
	if (*out == NULL)
		goto bad;
	if (decomp)
		inflateEnd(&zbuf);
	else
		deflateEnd(&zbuf);
	output = *out;
	for (j = 0; buf[j].flag != 0; j++) {
		if (count > buf[j].size) {
			bcopy(buf[j].out, *out, buf[j].size);
			*out += buf[j].size;
			free(buf[j].out, M_CRYPTO_DATA);
			count -= buf[j].size;
		} else {
			/* it should be the last buffer */
			bcopy(buf[j].out, *out, count);
			*out += count;
			free(buf[j].out, M_CRYPTO_DATA);
			count = 0;
		}
	}
	*out = output;
	return result;

bad:
	*out = NULL;
	for (j = 0; buf[j].flag != 0; j++)
		free(buf[j].out, M_CRYPTO_DATA);
	if (decomp)
		inflateEnd(&zbuf);
	else
		deflateEnd(&zbuf);
	return 0;
}

void *
z_alloc(nil, type, size)
	void *nil;
	u_int type, size;
{
	void *ptr;

	ptr = malloc(type *size, M_CRYPTO_DATA, M_NOWAIT);
	return ptr;
}

void
z_free(nil, ptr)
	void *nil, *ptr;
{
	free(ptr, M_CRYPTO_DATA);
}
