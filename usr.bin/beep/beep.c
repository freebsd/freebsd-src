/*-
 * Copyright (c) 2021 Hans Petter Selasky <hselasky@freebsd.org>
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

#include <capsicum_helpers.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <paths.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	SAMPLE_RATE_DEF 48000		/* hz */
#define	SAMPLE_RATE_MAX 48000		/* hz */
#define	SAMPLE_RATE_MIN 8000		/* hz */

#define	DURATION_DEF 150		/* ms */
#define	DURATION_MAX 2000		/* ms */
#define	DURATION_MIN 50			/* ms */

#define	GAIN_DEF 75
#define	GAIN_MAX 100
#define	GAIN_MIN 0

#define	WAVE_POWER 1.25f

#define	DEFAULT_HZ 440

#define	DEFAULT_DEVICE _PATH_DEV "dsp"

static int frequency = DEFAULT_HZ;
static int duration_ms = DURATION_DEF;
static int sample_rate = SAMPLE_RATE_DEF;
static int gain = GAIN_DEF;
static const char *oss_dev = DEFAULT_DEVICE;
static bool background;

/*
 * wave_function_16
 *
 * "phase" should be in the range [0.0f .. 1.0f>
 * "power" should be in the range <0.0f .. 2.0f>
 *
 * The return value is in the range [-1.0f .. 1.0f]
 */
static float
wave_function_16(float phase, float power)
{
	uint16_t x = phase * (1U << 16);
	float retval;
	uint8_t num;

	/* Handle special cases, if any */
	switch (x) {
	case 0xffff:
	case 0x0000:
		return (1.0f);
	case 0x3fff:
	case 0x4000:
	case 0xBfff:
	case 0xC000:
		return (0.0f);
	case 0x7FFF:
	case 0x8000:
		return (-1.0f);
	default:
		break;
	}

	/* Apply Gray coding */
	for (uint16_t mask = 1U << 15; mask != 1; mask /= 2) {
		if (x & mask)
			x ^= (mask - 1);
	}

	/* Find first set bit */
	for (num = 0; num != 14; num++) {
		if (x & (1U << num)) {
			num++;
			break;
		}
	}

	/* Initialize return value */
	retval = 0.0;

	/* Compute the rest of the power series */
	for (; num != 14; num++) {
		if (x & (1U << num)) {
			retval = (1.0f - retval) / 2.0f;
			retval = powf(retval, power);
		} else {
			retval = (1.0f + retval) / 2.0f;
			retval = powf(retval, power);
		}
	}

	/* Check if halfway */
	if (x & (1ULL << 14))
		retval = -retval;

	return (retval);
}

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [parameters]\n"
	    "\t" "-F <frequency in HZ, default %d Hz>\n"
	    "\t" "-D <duration in ms, from %d ms to %d ms, default %d ms>\n"
	    "\t" "-r <sample rate in HZ, from %d Hz to %d Hz, default %d Hz>\n"
	    "\t" "-d <OSS device (default %s)>\n"
	    "\t" "-g <gain from %d to %d, default %d>\n"
	    "\t" "-B Run in background\n"
	    "\t" "-h Show usage\n",
	    getprogname(),
	    DEFAULT_HZ,
	    DURATION_MIN, DURATION_MAX, DURATION_DEF,
	    SAMPLE_RATE_MIN, SAMPLE_RATE_MAX, SAMPLE_RATE_DEF,
	    DEFAULT_DEVICE,
	    GAIN_MIN, GAIN_MAX, GAIN_DEF);
	exit(1);
}

int
main(int argc, char **argv)
{
	float *buffer;
	size_t slope;
	size_t size;
	size_t off;
	float a;
	float d;
	float p;
	int c;
	int f;

	while ((c = getopt(argc, argv, "BF:D:r:g:d:h")) != -1) {
		switch (c) {
		case 'F':
			frequency = strtol(optarg, NULL, 10);
			break;
		case 'D':
			duration_ms = strtol(optarg, NULL, 10);
			if (duration_ms < DURATION_MIN ||
			    duration_ms > DURATION_MAX)
				usage();
			break;
		case 'r':
			sample_rate = strtol(optarg, NULL, 10);
			if (sample_rate < SAMPLE_RATE_MIN ||
			    sample_rate > SAMPLE_RATE_MAX)
				usage();
			break;
		case 'g':
			gain = strtol(optarg, NULL, 10);
			if (gain < GAIN_MIN ||
			    gain > GAIN_MAX)
				usage();
			break;
		case 'd':
			oss_dev = optarg;
			break;
		case 'B':
			background = true;
			break;
		default:
			usage();
			break;
		}
	}

	if (background && daemon(0, 0) != 0)
		errx(1, "daemon(0,0) failed");

	f = open(oss_dev, O_WRONLY);
	if (f < 0)
		err(1, "Failed to open '%s'", oss_dev);

	if (caph_enter() == -1)
		err(1, "Failed to enter capability mode");

	c = 1;				/* mono */
	if (ioctl(f, SOUND_PCM_WRITE_CHANNELS, &c) != 0)
		errx(1, "ioctl SOUND_PCM_WRITE_CHANNELS(1) failed");

	c = AFMT_FLOAT;
	if (ioctl(f, SNDCTL_DSP_SETFMT, &c) != 0)
		errx(1, "ioctl SNDCTL_DSP_SETFMT(AFMT_FLOAT) failed");

	if (ioctl(f, SNDCTL_DSP_SPEED, &sample_rate) != 0)
		errx(1, "ioctl SNDCTL_DSP_SPEED(%d) failed", sample_rate);

	c = (2 << 16);
	while ((1ULL << (c & 63)) < (size_t)(4 * sample_rate / 50))
		c++;
	if (ioctl(f, SNDCTL_DSP_SETFRAGMENT, &c))
		errx(1, "ioctl SNDCTL_DSP_SETFRAGMENT(0x%x) failed", c);

	if (ioctl(f, SNDCTL_DSP_GETODELAY, &c) != 0)
		errx(1, "ioctl SNDCTL_DSP_GETODELAY failed");

	size = ((sample_rate * duration_ms) + 999) / 1000;
	buffer = malloc(sizeof(buffer[0]) * size);
	if (buffer == NULL)
		errx(1, "out of memory");

	/* compute slope duration in samples */
	slope = (DURATION_MIN * sample_rate) / 2000;

	/* compute base gain */
	a = powf(65536.0f, (float)gain / (float)GAIN_MAX) / 65536.0f;

	/* set initial phase and delta */
	p = 0;
	d = (float)frequency / (float)sample_rate;

	/* compute wave */
	for (p = off = 0; off != size; off++, p += d) {
		float sample;

		p = p - floorf(p);
		sample = a * wave_function_16(p, WAVE_POWER);

		if (off < slope)
			sample = sample * off / (float)slope;
		else if (off > (size - slope))
			sample = sample * (size - off - 1) / (float)slope;

		buffer[off] = sample;
	}

	if (write(f, buffer, size * sizeof(buffer[0])) !=
	    (ssize_t)(size * sizeof(buffer[0])))
		errx(1, "failed writing to DSP device(%s)", oss_dev);

	free(buffer);

	/* wait for data to be written */
	while (ioctl(f, SNDCTL_DSP_GETODELAY, &c) == 0) {
		if (c == 0)
			break;
		usleep(10000);
	}

	/* wait for audio to go out */
	usleep(50000);
	close(f);

	return (0);
}
