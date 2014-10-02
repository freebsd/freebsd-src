/*-
 * Copyright (c) 2014 SRI International
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "cheritest.h"

#define INFLATE_BUFSIZE	(size_t)10*1024

void
test_sandbox_inflate_zeros(const struct cheri_test *ctp __unused)
{
#ifndef NO_SANDBOX
	register_t v;
	struct zstream_proxy zsp;
#endif
	int ret;
	size_t compsize, uncompsize;
	uint8_t *compbuf, *inbuf, *outbuf;
	z_stream zs;

	uncompsize = INFLATE_BUFSIZE;
	/*
	 * Be conservative, random inputs may blow up signficantly.
	 * Should really do multiple passes with realloc...
	 */
	compsize = uncompsize * 2;
	if ((inbuf = calloc(1, uncompsize)) == NULL)
		cheritest_failure_err("calloc inbuf");
	if ((compbuf = malloc(compsize)) == NULL)
		cheritest_failure_err("malloc compbuf");
	if ((outbuf = malloc(uncompsize)) == NULL)
		cheritest_failure_err("malloc outbuf");

	memset(&zs, 0, sizeof(zs));
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	if (deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK)
		cheritest_failure_errx("deflateInit");

	zs.next_in = inbuf;
	zs.avail_in = uncompsize;
	zs.next_out = compbuf;
	zs.avail_out = compsize;
	if ((ret = deflate(&zs, Z_FINISH)) != Z_STREAM_END)
		cheritest_failure_errx("deflate returned %d", ret);
	if ((ret = deflateEnd(&zs)) != Z_OK)
		cheritest_failure_errx("deflateEnd returned %d ret", ret);

	/* BD: could realloc, but why bother */
	compsize = zs.total_out;

	memset(&zs, 0, sizeof(zs));
	zs.next_in = compbuf;
	zs.avail_in = compsize;
	zs.next_out = outbuf;
	zs.avail_out = uncompsize;
#ifndef NO_SANDBOX
	memset(&zsp, 0, sizeof(zsp));
	zsp.next_in = cheri_ptr(zs.next_in, zs.avail_in); /* XXX perm */
	zsp.avail_in = zs.avail_in;
	zsp.next_out = cheri_ptr(zs.next_out, zs.avail_out);
	zsp.avail_out = zs.avail_out;
	v = sandbox_object_cinvoke(cheritest_objectp,
	    CHERITEST_HELPER_OP_INFLATE, 0, 0, 0, 0, 0, 0, 0,
	    sandbox_object_getsystemobject(cheritest_objectp).co_codecap,
	    sandbox_object_getsystemobject(cheritest_objectp).co_datacap,
	    cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap(), cheri_zerocap(),
	    cheri_ptr(&zsp, sizeof(struct zstream_proxy)), cheri_zerocap());
	if (v == -1)
		cheritest_failure_errx("sandbox error");
	zs.total_in = zsp.total_in;
	zs.total_out = zsp.total_out;
#else
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	if (inflateInit(&zs) != Z_OK)
		cheritest_failure_errx("inflateInit");
	if (inflate(&zs, Z_FINISH) != Z_STREAM_END)
		cheritest_failure_errx("inflate");
	if (inflateEnd(&zs) != Z_OK)
		cheritest_failure_errx("inflateEnd");
#endif
	if (zs.total_in != compsize)
		cheritest_failure_errx("expected to consume %zu bytes, got %zu",
		    compsize, zs.total_in);
	if (zs.total_out != uncompsize)
		cheritest_failure_errx("expected %zu bytes out, got %zu",
		    uncompsize, zs.total_out);
	if (memcmp(inbuf, outbuf, uncompsize) != 0)
		cheritest_failure_errx("output does not equal input");
#if 0
	int diff = 0;
	for (int i = 0; i < uncompsize; i++) {
		if (inbuf[i] != outbuf[i]) {
			printf("inbuf and outbuf differ at %d 0x%02u vs 0x%02u\n", i, inbuf[i], outbuf[i]);
			diff++;
		}
	}
	if (diff)
		cheritest_failure_errx("inbuf and outbuf differ at %d locations",
		    diff);
#endif

	cheritest_success();
}
