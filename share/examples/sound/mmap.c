/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Goran Mekić
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

/*
 * This program demonstrates low-latency audio pass-through using mmap
 * and kqueue.  It opens input and output audio devices using memory-
 * mapped I/O, synchronizes them in a sync group for simultaneous start,
 * then continuously copies audio data from input to output.  Buffer
 * positions are obtained from kqueue's ext[0] (replacing GETIPTR/
 * GETOPTR ioctls) and error counters from ext[1] (replacing GETERROR).
 */

#include <sys/event.h>

#include "oss.h"

/*
 * Copy data between ring buffers, handling wraparound.
 * The copy starts at 'offset' and copies 'length' bytes.
 * If the copy crosses the buffer boundary, it wraps to the beginning.
 */
static void
copy_ring(void *dstv, const void *srcv, int buffer_bytes, int offset,
    int length)
{
	uint8_t *dst = dstv;
	const uint8_t *src = srcv;
	int first;

	if (length <= 0)
		return;

	/* Calculate bytes to copy before wraparound */
	first = buffer_bytes - offset;
	if (first > length)
		first = length;

	/* Copy first part (up to buffer end or length) */
	memcpy(dst + offset, src + offset, first);

	/* Copy remaining part from beginning of buffer if needed */
	if (first < length)
		memcpy(dst, src, length - first);
}

int
main(int argc, char *argv[])
{
	int ch, bytes;
	int frag_size, frame_size, verbose = 0;
	int map_pointer = 0;
	int64_t read_progress = 0, write_progress = 0;
	oss_syncgroup sync_group = { 0, 0, { 0 } };
	struct config config_in = {
		.device = "/dev/dsp",
		.mode = O_RDONLY | O_EXCL | O_NONBLOCK,
		.format = AFMT_S32_NE,
		.sample_rate = 48000,
		.mmap = 1,
	};
	struct config config_out = {
		.device = "/dev/dsp",
		.mode = O_WRONLY | O_EXCL | O_NONBLOCK,
		.format = AFMT_S32_NE,
		.sample_rate = 48000,
		.mmap = 1,
	};
	struct kevent ev;
	int kq;

	while ((ch = getopt(argc, argv, "v")) != -1) {
		switch (ch) {
		case 'v':
			verbose = 1;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (!verbose)
		printf("Use -v for verbose mode\n");

	oss_init(&config_in);
	oss_init(&config_out);

	/*
	 * Verify input and output have matching ring-buffer geometry.
	 * The passthrough loop copies raw bytes at the same offset in both mmap
	 * buffers, so both devices must expose the same total byte count.
	 * They must also use the same max_channels because frame_size is
	 * derived from that value and all mmap pointers/lengths are expected to
	 * stay aligned to whole frames on both sides. If channels differed, the
	 * same byte offset could land in the middle of a frame on one device.
	 */
	if (config_in.buffer_info.bytes != config_out.buffer_info.bytes)
		errx(1,
		    "Input and output configurations have different buffer sizes");
	if (config_in.audio_info.max_channels !=
	    config_out.audio_info.max_channels)
		errx(1,
		    "Input and output configurations have different number of channels");

	bytes = config_in.buffer_info.bytes;
	frag_size = config_in.buffer_info.fragsize;
	frame_size = config_in.sample_size * config_in.audio_info.max_channels;
	if (frag_size != config_out.buffer_info.fragsize)
		errx(1,
		    "Input and output configurations have different fragment sizes");

	/* Clear output buffer to prevent noise on startup */
	memset(config_out.buf, 0, bytes);

	/* Configure and start sync group */
	sync_group.mode = PCM_ENABLE_INPUT;
	if (ioctl(config_in.fd, SNDCTL_DSP_SYNCGROUP, &sync_group) < 0)
		err(1, "Failed to add input to syncgroup");
	sync_group.mode = PCM_ENABLE_OUTPUT;
	if (ioctl(config_out.fd, SNDCTL_DSP_SYNCGROUP, &sync_group) < 0)
		err(1, "Failed to add output to syncgroup");
	if (ioctl(config_in.fd, SNDCTL_DSP_SYNCSTART, &sync_group.id) < 0)
		err(1, "Starting sync group failed");

	/* Create kqueue and register input device for read events */
	kq = kqueue();
	if (kq < 0)
		err(1, "kqueue failed");
	EV_SET(&ev, config_in.fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
	if (kevent(kq, &ev, 1, NULL, 0, NULL) < 0)
		err(1, "kevent register failed");

	/*
	 * Main processing loop:
	 * Block on kevent() until input data is available.
	 * ext[0] holds the current DMA pointer (GETIPTR/GETOPTR equivalent).
	 * ext[1] holds the xrun count for the channel (GETERROR equivalent).
	 */
	for (;;) {
		int n;
		int ptr;
		unsigned delta;

		n = kevent(kq, NULL, 0, &ev, 1, NULL);
		if (n < 0)
			err(1, "kevent failed");
		if (n == 0)
			continue;

		ptr = (int)ev.ext[0];
		if (ptr < 0 || ptr >= bytes)
			errx(1, "Pointer out of bounds: %d", ptr);
		if ((ptr % frame_size) != 0)
			errx(1, "Pointer %d not aligned to frame size %d", ptr,
			    frame_size);

		/*
		 * Calculate delta: how many bytes have been processed since
		 * last check. Handle ring buffer wraparound.
		 */
		delta = (ptr + bytes - map_pointer) % bytes;

		/* Update pointer and progress tracking */
		map_pointer = ptr;
		read_progress += delta;

		/* Report xruns if any */
		if (ev.ext[1] != 0 && verbose)
			warnx("xruns: %llu", (unsigned long long)ev.ext[1]);

		/* Copy new audio data if available */
		if (read_progress > write_progress) {
			int offset = write_progress % bytes;
			int length = read_progress - write_progress;

			copy_ring(config_out.buf, config_in.buf, bytes, offset,
			    length);
			write_progress = read_progress;
			if (verbose)
				printf("copied %d bytes at %d (abs %lld)\n",
				    length, offset, (long long)write_progress);
		}
	}

	close(kq);
	if (munmap(config_in.buf, bytes) != 0)
		err(1, "Memory unmap failed");
	config_in.buf = NULL;
	if (munmap(config_out.buf, bytes) != 0)
		err(1, "Memory unmap failed");
	config_out.buf = NULL;
	close(config_in.fd);
	close(config_out.fd);

	return (0);
}
