/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Klara, Inc.
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

/* This file is #included by gzip.c */

static off_t
unzstd(int in, int out, char *pre, size_t prelen, off_t *bytes_in)
{
	static char *ibuf, *obuf;
	ZSTD_inBuffer zib;
	ZSTD_outBuffer zob;
	ZSTD_DCtx *zds;
	ssize_t res;
	size_t zres;
	size_t bytes_out = 0;
	int eof = 0;

	if (ibuf == NULL)
		ibuf = malloc(BUFLEN);
	if (obuf == NULL)
		obuf = malloc(BUFLEN);
	if (ibuf == NULL || obuf == NULL)
		maybe_err("malloc");

	zds = ZSTD_createDStream();
	ZSTD_initDStream(zds);

	zib.src = pre;
	zib.size = prelen;
	zib.pos = 0;
	if (bytes_in != NULL)
		*bytes_in = prelen;
	zob.dst = obuf;
	zob.size = BUFLEN;
	zob.pos = 0;

	while (!eof) {
		if (zib.pos >= zib.size) {
			res = read(in, ibuf, BUFLEN);
			if (res < 0)
				maybe_err("read");
			if (res == 0)
				eof = 1;
			infile_newdata(res);
			zib.src = ibuf;
			zib.size = res;
			zib.pos = 0;
			if (bytes_in != NULL)
				*bytes_in += res;
		}
		zres = ZSTD_decompressStream(zds, &zob, &zib);
		if (ZSTD_isError(zres)) {
			maybe_errx("%s", ZSTD_getErrorName(zres));
		}
		if (zob.pos > 0) {
			res = write(out, obuf, zob.pos);
			if (res < 0)
				maybe_err("write");
			zob.pos = 0;
			bytes_out += res;
		}
	}
	ZSTD_freeDStream(zds);
	return (bytes_out);
}
