/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Goran Mekić
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

#include "ossinit.h"

int
main()
{
	config_t config = {
		.device = "/dev/dsp",
		.channels = -1,
		.format = format,
		.frag = -1,
		.sample_rate = 48000,
		.sample_size = sizeof(sample_t),
		.buffer_info.fragments = -1,
		.mmap = 0,
	};

	/* Initialize device */
	oss_init(&config);

	/*
	 * Allocate input and output buffers so that their size match
	 * frag_size
	 */
	int ret;
	int bytes = config.buffer_info.bytes;
	int8_t *ibuf = malloc(bytes);
	int8_t *obuf = malloc(bytes);
	sample_t *channels = malloc(bytes);

	printf(
	    "bytes: %d, fragments: %d, fragsize: %d, fragstotal: %d, samples: %d\n",
	    bytes,
	    config.buffer_info.fragments,
	    config.buffer_info.fragsize,
	    config.buffer_info.fragstotal,
	    config.sample_count
	    );

	/* Minimal engine: read input and copy it to the output */
	for (;;) {
		ret = read(config.fd, ibuf, bytes);
		if (ret < bytes) {
			fprintf(
			    stderr,
			    "Requested %d bytes, but read %d!\n",
			    bytes,
			    ret
			    );
			break;
		}
		oss_split(&config, (sample_t *)ibuf, channels);
		/* All processing will happen here */
		oss_merge(&config, channels, (sample_t *)obuf);
		ret = write(config.fd, obuf, bytes);
		if (ret < bytes) {
			fprintf(
			    stderr,
			    "Requested %d bytes, but wrote %d!\n",
			    bytes,
			    ret
			    );
			break;
		}
	}

	/* Cleanup */
	free(channels);
	free(obuf);
	free(ibuf);
	close(config.fd);
	return (0);
}
