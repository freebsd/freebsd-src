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
 * This program demonstrates low-latency audio pass-through using mmap.
 * Opens input and output audio devices using memory-mapped I/O,
 * synchronizes them in a sync group for simultaneous start,
 * then continuously copies audio data from input to output.
 */

#include <time.h>

#include "oss.h"

/*
 * Get current time in nanoseconds using monotonic clock.
 * Monotonic clock is not affected by system time changes.
 */
static int64_t
gettime_ns(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		err(1, "clock_gettime failed");
	return ((int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec);
}

/*
 * Sleep until the specified absolute time (in nanoseconds).
 * Uses TIMER_ABSTIME for precise timing synchronization.
 */
static void
sleep_until_ns(int64_t target_ns)
{
	struct timespec ts;

	ts.tv_sec = target_ns / 1000000000LL;
	ts.tv_nsec = target_ns % 1000000000LL;
	if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) != 0)
		err(1, "clock_nanosleep failed");
}

/*
 * Calculate the number of frames to process per iteration.
 * Higher sample rates require larger steps to maintain efficiency.
 */
static unsigned
frame_stepping(unsigned sample_rate)
{
	return (16U * (1U + (sample_rate / 50000U)));
}

/*
 * Update the mmap pointer and calculate progress.
 * Returns the absolute progress in bytes.
 *
 * fd: file descriptor for the audio device
 * request: ioctl request (SNDCTL_DSP_GETIPTR or SNDCTL_DSP_GETOPTR)
 * map_pointer: current pointer position in the ring buffer
 * map_progress: absolute progress in bytes
 * buffer_bytes: total size of the ring buffer
 * frag_size: size of each fragment
 * frame_size: size of one audio frame in bytes
 */
static int64_t
update_map_progress(int fd, unsigned long request, int *map_pointer,
    int64_t *map_progress, int buffer_bytes, int frag_size, int frame_size)
{
	count_info info = {};
	unsigned delta, max_bytes, cycles;
	int fragments;

	if (ioctl(fd, request, &info) < 0)
		err(1, "Failed to get mmap pointer");
	if (info.ptr < 0 || info.ptr >= buffer_bytes)
		errx(1, "Pointer out of bounds: %d", info.ptr);
	if ((info.ptr % frame_size) != 0)
		errx(1, "Pointer %d not aligned to frame size %d", info.ptr,
		    frame_size);
	if (info.blocks < 0)
		errx(1, "Invalid block count %d", info.blocks);

	/*
	 * Calculate delta: how many bytes have been processed since last check.
	 * Handle ring buffer wraparound using modulo arithmetic.
	 */
	delta = (info.ptr + buffer_bytes - *map_pointer) % buffer_bytes;

	/*
	 * Adjust delta based on reported blocks available.
	 * This accounts for cases where the pointer has wrapped multiple times.
	 */
	max_bytes = (info.blocks + 1) * frag_size - 1;
	if (max_bytes >= delta) {
		cycles = max_bytes - delta;
		cycles -= cycles % buffer_bytes;
		delta += cycles;
	}

	/* Verify fragment count matches expected value */
	fragments = delta / frag_size;
	if (info.blocks < fragments || info.blocks > fragments + 1)
		warnx("Pointer block mismatch: ptr=%d blocks=%d delta=%u",
		    info.ptr, info.blocks, delta);

	/* Update pointer and progress tracking */
	*map_pointer = info.ptr;
	*map_progress += delta;
	return (*map_progress);
}

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
	unsigned step_frames;
	int64_t frame_ns, start_ns, next_wakeup_ns;
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

	/* Calculate timing parameters */
	step_frames = frame_stepping(config_in.sample_rate);
	frame_ns = 1000000000LL / config_in.sample_rate;

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

	/* Initialize timing and progress tracking */
	start_ns = gettime_ns();
	read_progress = update_map_progress(config_in.fd, SNDCTL_DSP_GETIPTR,
	    &map_pointer, &read_progress, bytes, frag_size, frame_size);
	write_progress = read_progress;
	next_wakeup_ns = start_ns;

	/*
	 * Main processing loop:
	 * 1. Sleep until next scheduled wakeup
	 * 2. Check how much new audio data is available
	 * 3. Copy available data from input to output buffer
	 * 4. Schedule next wakeup
	 */
	for (;;) {
		sleep_until_ns(next_wakeup_ns);
		read_progress = update_map_progress(config_in.fd,
		    SNDCTL_DSP_GETIPTR, &map_pointer, &read_progress, bytes,
		    frag_size, frame_size);

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

		/* Schedule next wakeup based on frame timing */
		next_wakeup_ns += (int64_t)step_frames * frame_ns;
		if (next_wakeup_ns < gettime_ns())
			next_wakeup_ns = gettime_ns();
	}

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
