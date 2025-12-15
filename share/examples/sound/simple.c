/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 The FreeBSD Foundation
 * Copyright (c) 2025 Goran Mekić
 *
 * Portions of this software were developed by Christos Margiolis
 * <christos@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
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

#include <sys/soundcard.h>
#include "oss.h"

/*
 * Split input buffer into channels. The input buffer is in interleaved format,
 * which means if we have 2 channels (L and R), this is what the buffer of 8
 * samples would contain: L,R,L,R,L,R,L,R. The result of this function is a
 * buffer containing: L,L,L,L,R,R,R,R.
 */
static void
to_channels(struct config *config, void *output)
{
	uint8_t *in = config->buf;
	uint8_t *out = output;
	int i, channel, index, offset, byte;

	/* Iterate over bytes in the input buffer */
	for (byte = 0; byte < config->buffer_info.bytes;
	    byte += config->sample_size) {
		/*
		 * Get index of a sample in the input buffer measured in
		 * samples
		 */
		i = byte / config->sample_size;

		/* Get which channel is being processed */
		channel = i % config->audio_info.max_channels;

		/* Get offset of the sample inside a single channel */
		offset = i / config->audio_info.max_channels;

		/* Get index of a sample in the output buffer */
		index = (channel * config->chsamples + offset) *
		    config->sample_size;

		/* Copy singe sample from input to output */
		memcpy(out+index, in+byte, config->sample_size);
	}
}

/*
 * Convert channels into interleaved format and put into output buffer
 */
static void
to_interleaved(struct config *config, void *input)
{
	uint8_t *out = config->buf;
	uint8_t *in = input;
	int i, index, offset, channel, byte;

	/* Iterate over bytes in the input buffer */
	for (byte = 0; byte < config->buffer_info.bytes;
	    byte += config->sample_size) {
		/*
		 * Get index of a sample in the input buffer measured in
		 * samples
		 */
		index = byte / config->sample_size;

		/* Get which channel is being processed */
		channel = index / config->chsamples;

		/* Get offset of the sample inside a single channel */
		offset = index % config->chsamples;

		/* Get index of a sample in the output buffer */
		i = (config->audio_info.max_channels * offset + channel) *
		    config->sample_size;

		/* Copy singe sample from input to output */
		memcpy(out+i, in+byte, config->sample_size);
	}
}

int
main(int argc, char *argv[])
{
	struct config config = {
		.device = "/dev/dsp",
		.mode = O_RDWR,
		.format = AFMT_S32_NE,
		.sample_rate = 48000,
	};
	int32_t *channels;
	int rc, bytes;

	oss_init(&config);
	if (config.format != AFMT_S32_NE)
		errx(1, "Device doesn't support signed 32bit samples. "
			"Check with 'sndctl' if it can be configured for 's32le' format.");
	bytes = config.buffer_info.bytes;
	channels = malloc(bytes);

	for (;;) {
		if ((rc = read(config.fd, config.buf, bytes)) < bytes) {
			warn("Requested %d bytes, but read %d!\n", bytes, rc);
			break;
		}
		/*
		 * Strictly speaking, we could omit "channels" and operate only
		 * using config->buf, but this example tries to show the real
		 * world application usage. The problem is that the buffer is
		 * in interleaved format, and if you'd like to do any
		 * processing and/or mixing, it is easier to do that if samples
		 * are grouped per channel.
		 */
		to_channels(&config, channels);
		to_interleaved(&config, channels);
		if ((rc = write(config.fd, config.buf, bytes)) < bytes) {
			warn("Requested %d bytes, but wrote %d!\n", bytes, rc);
			break;
		}
	}

	free(channels);
	free(config.buf);
	close(config.fd);

	return (0);
}
