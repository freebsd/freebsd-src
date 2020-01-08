/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2016 Alex Teaca <iateaca@FreeBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef WITHOUT_CAPSICUM
#include <sys/capsicum.h>
#include <capsicum_helpers.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <err.h>
#include <sysexits.h>

#include "audio.h"
#include "pci_hda.h"

/*
 * Audio Player internal data structures
 */

struct audio {
	int fd;
	uint8_t dir;
	uint8_t inited;
	char dev_name[64];
};

/*
 * Audio Player module function definitions
 */

/*
 * audio_init - initialize an instance of audio player
 * @dev_name - the backend sound device used to play / capture
 * @dir - dir = 1 for write mode, dir = 0 for read mode
 */
struct audio *
audio_init(const char *dev_name, uint8_t dir)
{
	struct audio *aud = NULL;
#ifndef WITHOUT_CAPSICUM
	cap_rights_t rights;
	cap_ioctl_t cmds[] = {
	    SNDCTL_DSP_RESET, SNDCTL_DSP_SETFMT, SNDCTL_DSP_CHANNELS,
	    SNDCTL_DSP_SPEED,
#ifdef DEBUG_HDA
	    SNDCTL_DSP_GETOSPACE, SNDCTL_DSP_GETISPACE,
#endif
	};
#endif

	assert(dev_name);

	aud = calloc(1, sizeof(*aud));
	if (!aud)
		return NULL;

	if (strlen(dev_name) < sizeof(aud->dev_name))
		memcpy(aud->dev_name, dev_name, strlen(dev_name) + 1);
	else {
		DPRINTF("dev_name too big");
		free(aud);
		return NULL;
	}

	aud->dir = dir;

	aud->fd = open(aud->dev_name, aud->dir ? O_WRONLY : O_RDONLY, 0);
	if (aud->fd == -1) {
		DPRINTF("Failed to open dev: %s, errno: %d",
		    aud->dev_name, errno);
		free(aud);
		return (NULL);
	}

#ifndef WITHOUT_CAPSICUM
	cap_rights_init(&rights, CAP_IOCTL, CAP_READ, CAP_WRITE);
	if (caph_rights_limit(aud->fd, &rights) == -1)
		errx(EX_OSERR, "Unable to apply rights for sandbox");
	if (caph_ioctls_limit(aud->fd, cmds, nitems(cmds)) == -1)
		errx(EX_OSERR, "Unable to limit ioctl rights for sandbox");
#endif

	return aud;
}

/*
 * audio_set_params - reset the sound device and set the audio params
 * @aud - the audio player to be configured
 * @params - the audio parameters to be set
 */
int
audio_set_params(struct audio *aud, struct audio_params *params)
{
	int audio_fd;
	int format, channels, rate;
	int err;
#if DEBUG_HDA == 1
	audio_buf_info info;
#endif

	assert(aud);
	assert(params);

	if ((audio_fd = aud->fd) < 0) {
		DPRINTF("Incorrect audio device descriptor for %s",
		    aud->dev_name);
		return (-1);
	}

	/* Reset the device if it was previously opened */
	if (aud->inited) {
		err = ioctl(audio_fd, SNDCTL_DSP_RESET, NULL);
		if (err == -1) {
			DPRINTF("Failed to reset fd: %d, errno: %d",
			    aud->fd, errno);
			return (-1);
		}
	} else
		aud->inited = 1;

	/* Set the Format (Bits per Sample) */
	format = params->format;
	err = ioctl(audio_fd, SNDCTL_DSP_SETFMT, &format);
	if (err == -1) {
		DPRINTF("Fail to set fmt: 0x%x errno: %d",
		    params->format, errno);
		return -1;
	}

	/* The device does not support the requested audio format */
	if (format != params->format) {
		DPRINTF("Mismatch format: 0x%x params->format: 0x%x",
		    format, params->format);
		return -1;
	}

	/* Set the Number of Channels */
	channels = params->channels;
	err = ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &channels);
	if (err == -1) {
		DPRINTF("Fail to set channels: %d errno: %d",
		    params->channels, errno);
		return -1;
	}

	/* The device does not support the requested no. of channels */
	if (channels != params->channels) {
		DPRINTF("Mismatch channels: %d params->channels: %d",
		    channels, params->channels);
		return -1;
	}

	/* Set the Sample Rate / Speed */
	rate = params->rate;
	err = ioctl(audio_fd, SNDCTL_DSP_SPEED, &rate);
	if (err == -1) {
		DPRINTF("Fail to set speed: %d errno: %d",
		    params->rate, errno);
		return -1;
	}

	/* The device does not support the requested rate / speed */
	if (rate != params->rate) {
		DPRINTF("Mismatch rate: %d params->rate: %d",
		    rate, params->rate);
		return -1;
	}

#if DEBUG_HDA == 1
	err = ioctl(audio_fd, aud->dir ? SNDCTL_DSP_GETOSPACE :
	    SNDCTL_DSP_GETISPACE, &info);
	if (err == -1) {
		DPRINTF("Fail to get audio buf info errno: %d", errno);
		return -1;
	}
	DPRINTF("fragstotal: 0x%x fragsize: 0x%x",
	    info.fragstotal, info.fragsize);
#endif
	return 0;
}

/*
 * audio_playback - plays samples to the sound device using blocking operations
 * @aud - the audio player used to play the samples
 * @buf - the buffer containing the samples
 * @count - the number of bytes in buffer
 */
int
audio_playback(struct audio *aud, const void *buf, size_t count)
{
	int audio_fd = -1;
	ssize_t len = 0, total = 0;

	assert(aud);
	assert(aud->dir);
	assert(buf);

	audio_fd = aud->fd;
	assert(audio_fd != -1);

	total = 0;
	while (total < count) {
		len = write(audio_fd, buf + total, count - total);
		if (len == -1) {
			DPRINTF("Fail to write to fd: %d, errno: %d",
			    audio_fd, errno);
			return -1;
		}

		total += len;
	}

	return 0;
}

/*
 * audio_record - records samples from the sound device using
 * blocking operations.
 * @aud - the audio player used to capture the samples
 * @buf - the buffer to receive the samples
 * @count - the number of bytes to capture in buffer
 * Returns -1 on error and 0 on success
 */
int
audio_record(struct audio *aud, void *buf, size_t count)
{
	int audio_fd = -1;
	ssize_t len = 0, total = 0;

	assert(aud);
	assert(!aud->dir);
	assert(buf);

	audio_fd = aud->fd;
	assert(audio_fd != -1);

	total = 0;
	while (total < count) {
		len = read(audio_fd, buf + total, count - total);
		if (len == -1) {
			DPRINTF("Fail to write to fd: %d, errno: %d",
			    audio_fd, errno);
			return -1;
		}

		total += len;
	}

	return 0;
}
