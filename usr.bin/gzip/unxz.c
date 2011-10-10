/*	$NetBSD: unxz.c,v 1.5 2011/09/30 01:32:21 christos Exp $	*/

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <lzma.h>

static off_t
unxz(int i, int o, char *pre, size_t prelen, off_t *bytes_in)
{
	lzma_stream strm = LZMA_STREAM_INIT;
	static const int flags = LZMA_TELL_UNSUPPORTED_CHECK|LZMA_CONCATENATED;
	lzma_ret ret;
	lzma_action action = LZMA_RUN;
	off_t bytes_out, bp;
	uint8_t ibuf[BUFSIZ];
	uint8_t obuf[BUFSIZ];

	if (bytes_in == NULL)
		bytes_in = &bp;

	strm.next_in = ibuf;
	memcpy(ibuf, pre, prelen);
	strm.avail_in = read(i, ibuf + prelen, sizeof(ibuf) - prelen);
	if (strm.avail_in == (size_t)-1)
		maybe_err("read failed");
	strm.avail_in += prelen;
	*bytes_in = strm.avail_in;

	if ((ret = lzma_stream_decoder(&strm, UINT64_MAX, flags)) != LZMA_OK)
		maybe_errx("Can't initialize decoder (%d)", ret);

	strm.next_out = NULL;
	strm.avail_out = 0;
	if ((ret = lzma_code(&strm, LZMA_RUN)) != LZMA_OK)
		maybe_errx("Can't read headers (%d)", ret);

	bytes_out = 0;
	strm.next_out = obuf;
	strm.avail_out = sizeof(obuf);

	for (;;) {
		if (strm.avail_in == 0) {
			strm.next_in = ibuf;
			strm.avail_in = read(i, ibuf, sizeof(ibuf));
			switch (strm.avail_in) {
			case (size_t)-1:
				maybe_err("read failed");
				/*NOTREACHED*/
			case 0:
				action = LZMA_FINISH;
				break;
			default:
				*bytes_in += strm.avail_in;
				break;
			}
		}

		ret = lzma_code(&strm, action);

		// Write and check write error before checking decoder error.
		// This way as much data as possible gets written to output
		// even if decoder detected an error.
		if (strm.avail_out == 0 || ret != LZMA_OK) {
			const size_t write_size = sizeof(obuf) - strm.avail_out;

			if (write(o, obuf, write_size) != (ssize_t)write_size)
				maybe_err("write failed");

			strm.next_out = obuf;
			strm.avail_out = sizeof(obuf);
			bytes_out += write_size;
		}

		if (ret != LZMA_OK) {
			if (ret == LZMA_STREAM_END) {
				// Check that there's no trailing garbage.
				if (strm.avail_in != 0 || read(i, ibuf, 1))
					ret = LZMA_DATA_ERROR;
				else {
					lzma_end(&strm);
					return bytes_out;
				}
			}

			const char *msg;
			switch (ret) {
			case LZMA_MEM_ERROR:
				msg = strerror(ENOMEM);
				break;

			case LZMA_FORMAT_ERROR:
				msg = "File format not recognized";
				break;

			case LZMA_OPTIONS_ERROR:
				// FIXME: Better message?
				msg = "Unsupported compression options";
				break;

			case LZMA_DATA_ERROR:
				msg = "File is corrupt";
				break;

			case LZMA_BUF_ERROR:
				msg = "Unexpected end of input";
				break;

			case LZMA_MEMLIMIT_ERROR:
				msg = "Reached memory limit";
				break;

			default:
				maybe_errx("Unknown error (%d)", ret);
				break;
			}
			maybe_errx("%s", msg);

		}
	}
}
