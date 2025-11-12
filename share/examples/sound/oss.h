/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Goran Mekić
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

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Minimal configuration for OSS. For real world applications, this structure
 * will probably contain many more fields
 */
struct config {
	char   *device;
	int	mode;
	int	fd;
	int	format;
	int	sample_count;
	int	sample_rate;
	int	sample_size;
	int	chsamples;
	int	mmap;
	void   *buf;
	oss_audioinfo audio_info;
	audio_buf_info buffer_info;
};

/*
 * The buffer size used by OSS is (2 ^ exponent) * number_of_fragments.
 * Exponent values range between 4 and 16, so this function looks for the
 * smallest exponent which can fit a buffer of size "x". The fragments
 * determine in how many chunks the buffer will be sliced into, hence if the
 * exponent is 4, and number of fragments is 2, the requested size will be 2^4
 * * 2 = 32. Please note that the buffer size is in bytes, not samples. For
 * example, a 24-bit sample will be represented with 3 bytes. If you're porting
 * an audio application from Linux, you should be aware that 24-bit samples on
 * it are represented with 4 bytes (usually int). The idea of a total buffer
 * size that holds number of fragments is to allow application to be
 * number_of_fragments - 1 late. That's called jitter tolerance.
 *
 * Official OSS development howto:
 * http://manuals.opensound.com/developer/DSP.html
 */
static inline int
size2exp(int x)
{
	int exp = 0;

	while ((1 << exp) < x)
		exp++;

	return (exp);
}

static void
oss_init(struct config *config)
{
	unsigned long request = SNDCTL_DSP_GETOSPACE;
	int tmp = 0;

	if ((config->fd = open(config->device, config->mode)) < 0)
		err(1, "Error opening the device %s", config->device);

	/* Get device information */
	if (ioctl(config->fd, SNDCTL_ENGINEINFO, &config->audio_info) < 0)
		err(1, "Unable to get device info");

	/* Get device capabilities */
	if (ioctl(config->fd, SNDCTL_DSP_GETCAPS, &config->audio_info.caps) < 0)
		err(1, "Unable to get capabilities");

	/* Check if device supports triggering */
	if (!(config->audio_info.caps & PCM_CAP_TRIGGER))
		errx(1, "Device doesn't support triggering!\n");

	/* Handle memory mapped mode */
	if (config->mmap) {
		if (!(config->audio_info.caps & PCM_CAP_MMAP))
			errx(1, "Device doesn't support mmap mode!\n");
		tmp = 0;
		if (ioctl(config->fd, SNDCTL_DSP_COOKEDMODE, &tmp) < 0)
			err(1, "Unable to set cooked mode");
	}

	/* Set sample format */
	if (ioctl(config->fd, SNDCTL_DSP_SETFMT, &config->format) < 0)
		err(1, "Unable to set sample format");

	/* Set sample channels */
	if (ioctl(config->fd, SNDCTL_DSP_CHANNELS, &config->audio_info.max_channels) < 0)
		err(1, "Unable to set channels");

	/* Set sample rate */
	if (ioctl(config->fd, SNDCTL_DSP_SPEED, &config->sample_rate) < 0)
		err(1, "Unable to set sample rate");

	/* Calculate sample size */
	switch (config->format) {
	case AFMT_S8:
	case AFMT_U8:
		config->sample_size = 1;
		break;
	case AFMT_S16_BE:
	case AFMT_S16_LE:
	case AFMT_U16_BE:
	case AFMT_U16_LE:
		config->sample_size = 2;
		break;
	case AFMT_S24_BE:
	case AFMT_S24_LE:
	case AFMT_U24_BE:
	case AFMT_U24_LE:
		config->sample_size = 3;
		break;
	case AFMT_S32_BE:
	case AFMT_S32_LE:
	case AFMT_U32_BE:
	case AFMT_U32_LE:
	case AFMT_F32_BE:
	case AFMT_F32_LE:
		config->sample_size = 4;
		break;
	default:
		errx(1, "Invalid audio format %d", config->format);
		break;
	}

	/*
	 * Set fragment and sample size. This part is optional as OSS has
	 * default values. From the kernel's perspective, there are few things
	 * OSS developers should be aware of:
	 *
	 * - For each sound(4)-created channel, there is a software-facing
	 *   buffer, and a hardware-facing one.
	 * - The sizes of the buffers can be listed in the console with "sndctl
	 *   swbuf hwbuf".
	 * - OSS ioctls only concern software-facing buffer fragments, not
	 *   hardware.
	 *
	 * For USB sound cards, the block size is set according to the
	 * hw.usb.uaudio.buffer_ms sysctl, meaning 2ms at 48kHz gives 0.002 *
	 * 48000 = 96 samples per block. Block size should be set as multiple
	 * of 96, in this case. The OSS driver insists on reading/writing a
	 * certain number of samples at a time, one fragment full of samples.
	 * It is bound to do so at a fixed time frame, to avoid under- and
	 * overruns during communication with the hardware.
	 */
	config->buffer_info.fragments = 2;
	tmp = size2exp(config->sample_size * config->audio_info.max_channels);
	tmp = ((config->buffer_info.fragments) << 16) | tmp;
	if (ioctl(config->fd, SNDCTL_DSP_SETFRAGMENT, &tmp) < 0)
		err(1, "Unable to set fragment size");

	/* Get buffer info */
	if ((config->mode & O_ACCMODE) == O_RDONLY)
		request = SNDCTL_DSP_GETISPACE;
	if (ioctl(config->fd, request, &config->buffer_info) < 0)
		err(1, "Unable to get buffer info");
	if (config->buffer_info.fragments < 1)
		config->buffer_info.fragments = config->buffer_info.fragstotal;
	if (config->buffer_info.bytes < 1)
		config->buffer_info.bytes = config->buffer_info.fragstotal * config->buffer_info.fragsize;
	if (config->buffer_info.bytes < 1) {
		errx(1, "OSS buffer error: buffer size can not be %d\n",
		    config->buffer_info.bytes);
	}
	config->sample_count = config->buffer_info.bytes / config->sample_size;
	config->chsamples = config->sample_count / config->audio_info.max_channels;
	config->buf = malloc(config->buffer_info.bytes);

	printf("bytes: %d, fragments: %d, fragsize: %d, fragstotal: %d, "
	    "samples: %d\n",
	    config->buffer_info.bytes, config->buffer_info.fragments,
	    config->buffer_info.fragsize, config->buffer_info.fragstotal,
	    config->sample_count);

	/* Set the trigger */
	switch (config->mode & O_ACCMODE) {
	case O_RDONLY:
		tmp = PCM_ENABLE_INPUT;
		break;
	case O_WRONLY:
		tmp = PCM_ENABLE_OUTPUT;
		break;
	case O_RDWR:
		tmp = PCM_ENABLE_INPUT | PCM_ENABLE_OUTPUT;
		break;
	default:
		errx(1, "Invalid mode %d", config->mode);
		break;
	}
	if (ioctl(config->fd, SNDCTL_DSP_SETTRIGGER, &tmp) < 0)
		err(1, "Failed to set trigger");
}
