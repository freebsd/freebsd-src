/*-
 * Copyright (c) 2014 Hans Petter Selasky
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

#include <sys/queue.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <math.h>
#include <sysexits.h>

#include "int.h"

#define	REF_FREQ 500	/* HZ */

uint32_t voss_ad_last_delay;
uint8_t voss_ad_enabled;
uint8_t voss_ad_output_signal;
uint8_t voss_ad_input_channel;
uint8_t voss_ad_output_channel;

static struct voss_ad {
	double *wave;

	double *sin_a;
	double *cos_a;

	double *sin_b;
	double *cos_b;

	double *buf_a;
	double *buf_b;

	double sum_sin_a;
	double sum_cos_a;

	double sum_sin_b;
	double sum_cos_b;

	uint32_t len_a;
	uint32_t len_b;

	uint32_t offset_a;
	uint32_t offset_b;
} voss_ad;

void
voss_ad_reset(void)
{
	uint32_t x;

	for (x = 0; x != voss_ad.len_a; x++)
		voss_ad.buf_a[x] = 0;

	for (x = 0; x != voss_ad.len_b; x++)
		voss_ad.buf_b[x] = 0;

	voss_ad.sum_sin_a = 0;
	voss_ad.sum_cos_a = 0;
	voss_ad.sum_sin_b = 0;
	voss_ad.sum_cos_b = 0;

	voss_ad.offset_a = 0;
	voss_ad.offset_b = 0;

	voss_ad_last_delay = 0;
}

void
voss_ad_init(uint32_t rate)
{
	double freq;
	int samples;
	int len;
	int x;

	len = sqrt(rate);

	samples = len * len;

	voss_ad.wave = malloc(sizeof(voss_ad.wave[0]) * samples);

	voss_ad.sin_a = malloc(sizeof(voss_ad.sin_a[0]) * len);
	voss_ad.cos_a = malloc(sizeof(voss_ad.cos_a[0]) * len);
	voss_ad.buf_a = malloc(sizeof(voss_ad.buf_a[0]) * len);
	voss_ad.len_a = len;

	voss_ad.sin_b = malloc(sizeof(voss_ad.sin_b[0]) * samples);
	voss_ad.cos_b = malloc(sizeof(voss_ad.cos_b[0]) * samples);
	voss_ad.buf_b = malloc(sizeof(voss_ad.buf_b[0]) * samples);
	voss_ad.len_b = samples;

	if (voss_ad.sin_a == NULL || voss_ad.cos_a == NULL ||
	    voss_ad.sin_b == NULL || voss_ad.cos_b == NULL ||
	    voss_ad.buf_a == NULL || voss_ad.buf_b == NULL)
		errx(EX_SOFTWARE, "Out of memory");

	freq = 1.0;

	while (1) {
		double temp = freq * ((double)rate) / ((double)len);
		if (temp >= REF_FREQ)
			break;
		freq += 1.0;
	}

	for (x = 0; x != len; x++) {
		voss_ad.sin_a[x] = sin(freq * 2.0 * M_PI * ((double)x) / ((double)len));
		voss_ad.cos_a[x] = cos(freq * 2.0 * M_PI * ((double)x) / ((double)len));
		voss_ad.buf_a[x] = 0;
	}

	for (x = 0; x != samples; x++) {

		voss_ad.wave[x] = sin(freq * 2.0 * M_PI * ((double)x) / ((double)len)) *
		  (1.0 + sin(2.0 * M_PI * ((double)x) / ((double)samples))) / 2.0;

		voss_ad.sin_b[x] = sin(2.0 * M_PI * ((double)x) / ((double)samples));
		voss_ad.cos_b[x] = cos(2.0 * M_PI * ((double)x) / ((double)samples));
		voss_ad.buf_b[x] = 0;
	}
}

static double
voss_add_decode_offset(double x /* cos */, double y /* sin */)
{
	uint32_t v;
	double r;

	r = sqrt((x * x) + (y * y));

	if (r == 0.0)
		return (0);

	x /= r;
	y /= r;

	v = 0;

	if (y < 0) {
		v |= 1;
		y = -y;
	}
	if (x < 0) {
		v |= 2;
		x = -x;
	}

	if (y < x) {
		r = acos(y);
	} else {
		r = asin(x);
	}

	switch (v) {
	case 0:
		r = (2.0 * M_PI) - r;
		break;
	case 1:
		r = M_PI + r;
		break;
	case 3:
		r = M_PI - r;
		break;
	default:
		break;
	}
	return (r);
}

double
voss_ad_getput_sample(double sample)
{
	double retval;
	double phase;
	uint32_t xa;
	uint32_t xb;

	xa = voss_ad.offset_a;
	xb = voss_ad.offset_b;
	retval = voss_ad.wave[xb];

	sample -= voss_ad.buf_a[xa];
	voss_ad.sum_sin_a += voss_ad.sin_a[xa] * sample;
	voss_ad.sum_cos_a += voss_ad.cos_a[xa] * sample;
	voss_ad.buf_a[xa] += sample;

	sample = sqrt((voss_ad.sum_sin_a * voss_ad.sum_sin_a) +
	    (voss_ad.sum_cos_a * voss_ad.sum_cos_a));

	sample -= voss_ad.buf_b[xb];
	voss_ad.sum_sin_b += voss_ad.sin_b[xb] * sample;
	voss_ad.sum_cos_b += voss_ad.cos_b[xb] * sample;
	voss_ad.buf_b[xb] += sample;

	if (++xa == voss_ad.len_a)
		xa = 0;

	if (++xb == voss_ad.len_b) {
		xb = 0;

		phase = voss_add_decode_offset(
		  voss_ad.sum_cos_b, voss_ad.sum_sin_b);

		voss_ad_last_delay = (uint32_t)(phase * (double)(voss_ad.len_b) / (2.0 * M_PI)) - (voss_ad.len_a / 2);
		if (voss_ad_last_delay > voss_ad.len_b)
			voss_ad_last_delay = voss_ad.len_b;
	}
	voss_ad.offset_a = xa;
	voss_ad.offset_b = xb;

	return (retval * (1LL << voss_ad_output_signal));
}
