/*-
 * Copyright (c) 2014 SRI International
 * Copyright (c) 2016 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#if !__has_feature(capabilities)
#error "This code requires a CHERI-aware compiler"
#endif

#include <sys/types.h>
#if 0
#include <sys/signal.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <machine/cpuregs.h>
#include <machine/sysarch.h>

#include <cheri/cheri_enter.h>
#include <cheri/cheri_fd.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sysexits.h>
#include <unistd.h>
#endif

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <cheri/sandbox.h>

#include <cheritest-helper.h>
#include <cheritest-helper-internal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "cheritest.h"

/*
 * These tests compress (and decompress) 10K buffers of zeroes in order to
 * validate compiler behaviour, passing data to/from domain crossing, etc.
 * The below constant is the fixed *expected* compressed version of the data,
 * used to evaluate the results of compression.
 */
#define INFLATE_BUFSIZE	(size_t)10*1024

static uint8_t uncompressed_zeroes[INFLATE_BUFSIZE];
static const size_t uncompressed_zeroes_len =
	    sizeof(uncompressed_zeroes) / sizeof(uncompressed_zeroes[0]);

static uint8_t compressed_zeroes[] = {
	0x78, 0x9c, 0xed, 0xc1, 0x01, 0x0d, 0x00, 0x00,
	0x00, 0xc2, 0xa0, 0xf7, 0x4f, 0x6d, 0x0e, 0x37,
	0xa0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x80, 0x37, 0x03, 0x28, 0x00, 0x00,
	0x01,
};
static const size_t compressed_zeroes_len =
	    sizeof(compressed_zeroes) / sizeof(compressed_zeroes[0]);

static void
check_compressed_data(const uint8_t *data, size_t datalen)
{
	size_t i;

	if (datalen != compressed_zeroes_len)
		cheritest_failure_errx("compressed data length wrong ("
		    "expected %zu, got %zu)", compressed_zeroes_len, datalen);
	for (i = 0; i < compressed_zeroes_len; i++) {
		if (data[i] != compressed_zeroes[i])
			cheritest_failure_errx("compressed data wrong at "
			    "byte %zu", i);
	}
}

static void
check_uncompressed_data(const uint8_t *data, size_t datalen)
{
	size_t i;

	if (datalen != uncompressed_zeroes_len)
		cheritest_failure_errx("uncompressed data length wrong ("
		    "expected %zu, got %zu)", uncompressed_zeroes_len, datalen);
	for (i = 0; i < uncompressed_zeroes_len; i++) {
		if (data[i] != uncompressed_zeroes[i])
			cheritest_failure_errx("uncompressed data wrong at "
			    "byte %zu", i);
	}
}

void
test_deflate_zeroes(const struct cheri_test *ctp __unused)
{
	int ret;
	size_t compsize;
	uint8_t *compbuf;
	z_stream zs;

	/*
	 * Be conservative, random inputs may blow up signficantly.
	 * Should really do multiple passes with realloc...
	 */
	compsize = uncompressed_zeroes_len * 2;
	if ((compbuf = malloc(compsize)) == NULL)
		cheritest_failure_err("malloc compbuf");

	memset(&zs, 0, sizeof(zs));
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	if ((ret = deflateInit(&zs, Z_DEFAULT_COMPRESSION)) != Z_OK)
		cheritest_failure_errx("deflateInit returned %d", ret);

	zs.next_in = uncompressed_zeroes;
	zs.avail_in = uncompressed_zeroes_len;
	zs.next_out = compbuf;
	zs.avail_out = compsize;
	if ((ret = deflate(&zs, Z_FINISH)) != Z_STREAM_END)
		cheritest_failure_errx("deflate returned %d", ret);
	if ((ret = deflateEnd(&zs)) != Z_OK)
		cheritest_failure_errx("deflateEnd returned %d", ret);
	check_compressed_data(compbuf, zs.total_out);
	free(compbuf);
	cheritest_success();
}

void
test_inflate_zeroes(const struct cheri_test *ctp __unused)
{
	int ret;
	uint8_t *outbuf;
	z_stream zs;

	if ((outbuf = malloc(uncompressed_zeroes_len)) == NULL)
		cheritest_failure_err("malloc outbuf");
	memset(&zs, 0, sizeof(zs));
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.next_in = compressed_zeroes;
	zs.avail_in = compressed_zeroes_len;
	zs.next_out = outbuf;
	zs.avail_out = uncompressed_zeroes_len;
	if ((ret = inflateInit(&zs)) != Z_OK)
		cheritest_failure_errx("inflateInit returned %d", ret);
	if ((ret = inflate(&zs, Z_FINISH)) != Z_STREAM_END)
		cheritest_failure_errx("inflate returned %d", ret);
	if ((ret = inflateEnd(&zs)) != Z_OK)
		cheritest_failure_errx("inflateEnd returned %d", ret);
	if (zs.total_in != compressed_zeroes_len)
		cheritest_failure_errx("expected to consume %zu bytes, got %zu",
		    compressed_zeroes_len, zs.total_in);
	check_uncompressed_data(outbuf, zs.total_out);
	free(outbuf);
	cheritest_success();
}


void
test_sandbox_inflate_zeroes(const struct cheri_test *ctp __unused)
{
	register_t v;
	struct zstream_proxy zsp;
	uint8_t *outbuf;
	z_stream zs;

	if ((outbuf = malloc(uncompressed_zeroes_len)) == NULL)
		cheritest_failure_err("malloc outbuf");
	memset(&zs, 0, sizeof(zs));
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.next_in = compressed_zeroes;
	zs.avail_in = compressed_zeroes_len;
	zs.next_out = outbuf;
	zs.avail_out = uncompressed_zeroes_len;

	/* Forward arguments into sandbox. */
	memset(&zsp, 0, sizeof(zsp));
	zsp.next_in = cheri_ptr(zs.next_in, zs.avail_in); /* XXX perm */
	zsp.avail_in = zs.avail_in;
	zsp.next_out = cheri_ptr(zs.next_out, zs.avail_out);
	zsp.avail_out = zs.avail_out;
	v = invoke_inflate(cheri_ptr(&zsp, sizeof(struct zstream_proxy)));
	if (v == -1)
		cheritest_failure_errx("sandbox error");

	/* Forward return values out. */
	zs.total_in = zsp.total_in;
	zs.total_out = zsp.total_out;
	if (zs.total_in != compressed_zeroes_len)
		cheritest_failure_errx("expected to consume %zu bytes, got %zu",
		    compressed_zeroes_len, zs.total_in);
	check_uncompressed_data(outbuf, zs.total_out);
	free(outbuf);
	cheritest_success();
}
