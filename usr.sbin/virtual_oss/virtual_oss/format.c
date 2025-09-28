/*-
 * Copyright (c) 2012-2020 Hans Petter Selasky
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
#include <sys/soundcard.h>

#include <stdint.h>
#include <string.h>

#include "int.h"

void
format_import(uint32_t fmt, const uint8_t *src, uint32_t len,
    int64_t *dst)
{
	const uint8_t *end = src + len;
	int64_t val;

	if (fmt & AFMT_16BIT) {
		while (src != end) {
			if (fmt & (AFMT_S16_LE | AFMT_U16_LE))
				val = src[0] | (src[1] << 8);
			else
				val = src[1] | (src[0] << 8);

			src += 2;

			if (fmt & (AFMT_U16_LE | AFMT_U16_BE))
				val = val ^ 0x8000;

			val <<= (64 - 16);
			val >>= (64 - 16);

			*dst++ = val;
		}

	} else if (fmt & AFMT_24BIT) {
		while (src < end) {
			if (fmt & (AFMT_S24_LE | AFMT_U24_LE))
				val = src[0] | (src[1] << 8) | (src[2] << 16);
			else
				val = src[2] | (src[1] << 8) | (src[0] << 16);

			src += 3;

			if (fmt & (AFMT_U24_LE | AFMT_U24_BE))
				val = val ^ 0x800000;

			val <<= (64 - 24);
			val >>= (64 - 24);

			*dst++ = val;
		}
	} else if (fmt & AFMT_32BIT) {
		while (src < end) {
			int64_t e, m, s;

			if (fmt & (AFMT_S32_LE | AFMT_U32_LE | AFMT_F32_LE))
				val = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
			else
				val = src[3] | (src[2] << 8) | (src[1] << 16) | (src[0] << 24);

			src += 4;

			if (fmt & (AFMT_U32_LE | AFMT_U32_BE))
				val = val ^ 0x80000000LL;

			if (fmt & (AFMT_F32_LE | AFMT_F32_BE)) {
				e = (val >> 23) & 0xff;
				/* NaN, +/- Inf  or too small */
				if (e == 0xff || e < 96) {
					val = 0;
					goto skip;
				}
				s = val & 0x80000000U;
				if (e > 126) {
					val = s == 0 ? format_max(fmt) :
					    -0x80000000LL;
					goto skip;
				}
				m = 0x800000 | (val & 0x7fffff);
				e += 8 - 127;
				if (e < 0)
					m >>= -e;
				else
					m <<= e;
				val = s == 0 ? m : -m;
			}
skip:
			val <<= (64 - 32);
			val >>= (64 - 32);

			*dst++ = val;
		}

	} else if (fmt & AFMT_8BIT) {
		while (src < end) {
			val = src[0];

			src += 1;

			if (fmt & AFMT_U8)
				val = val ^ 0x80;

			val <<= (64 - 8);
			val >>= (64 - 8);

			*dst++ = val;
		}
	}
}

void
format_export(uint32_t fmt, const int64_t *src, uint8_t *dst, uint32_t len)
{
	const uint8_t *end = dst + len;
	int64_t val;

	if (fmt & AFMT_16BIT) {
		while (dst != end) {

			val = *src++;

			if (val > 0x7FFF)
				val = 0x7FFF;
			else if (val < -0x7FFF)
				val = -0x7FFF;

			if (fmt & (AFMT_U16_LE | AFMT_U16_BE))
				val = val ^ 0x8000;

			if (fmt & (AFMT_S16_LE | AFMT_U16_LE)) {
				dst[0] = val;
				dst[1] = val >> 8;
			} else {
				dst[1] = val;
				dst[0] = val >> 8;
			}

			dst += 2;
		}

	} else if (fmt & AFMT_24BIT) {
		while (dst != end) {

			val = *src++;

			if (val > 0x7FFFFF)
				val = 0x7FFFFF;
			else if (val < -0x7FFFFF)
				val = -0x7FFFFF;

			if (fmt & (AFMT_U24_LE | AFMT_U24_BE))
				val = val ^ 0x800000;

			if (fmt & (AFMT_S24_LE | AFMT_U24_LE)) {
				dst[0] = val;
				dst[1] = val >> 8;
				dst[2] = val >> 16;
			} else {
				dst[2] = val;
				dst[1] = val >> 8;
				dst[0] = val >> 16;
			}

			dst += 3;
		}
	} else if (fmt & AFMT_32BIT) {
		while (dst != end) {
			int64_t r, e;

			val = *src++;

			if (val > 0x7FFFFFFFLL)
				val = 0x7FFFFFFFLL;
			else if (val < -0x7FFFFFFFLL)
				val = -0x7FFFFFFFLL;

			if (fmt & (AFMT_F32_LE | AFMT_F32_BE)) {
				if (val == 0)
					r = 0;
				else if (val == format_max(fmt))
					r = 0x3f800000;
				else if (val == -0x80000000LL)
					r = 0x80000000U | 0x3f800000;
				else {
					r = 0;
					if (val < 0) {
						r |= 0x80000000U;
						val = -val;
					}
					e = 127 - 8;
					while ((val & 0x7f000000) != 0) {
						val >>= 1;
						e++;
					}
					while ((val & 0x7f800000) == 0) {
						val <<= 1;
						e--;
					}
					r |= (e & 0xff) << 23;
					r |= val & 0x7fffff;
				}
				val = r;
			}

			if (fmt & (AFMT_U32_LE | AFMT_U32_BE))
				val = val ^ 0x80000000LL;

			if (fmt & (AFMT_S32_LE | AFMT_U32_LE | AFMT_F32_LE)) {
				dst[0] = val;
				dst[1] = val >> 8;
				dst[2] = val >> 16;
				dst[3] = val >> 24;
			} else {
				dst[3] = val;
				dst[2] = val >> 8;
				dst[1] = val >> 16;
				dst[0] = val >> 24;
			}

			dst += 4;
		}

	} else if (fmt & AFMT_8BIT) {
		while (dst != end) {

			val = *src++;

			if (val > 0x7F)
				val = 0x7F;
			else if (val < -0x7F)
				val = -0x7F;

			if (fmt & (AFMT_U8))
				val = val ^ 0x80;

			dst[0] = val;

			dst += 1;
		}
	}
}

int64_t
format_max(uint32_t fmt)
{
	if (fmt & AFMT_16BIT)
		return (0x7FFF);
	else if (fmt & AFMT_24BIT)
		return (0x7FFFFF);
	else if (fmt & AFMT_32BIT)
		return (0x7FFFFFFF);
	else if (fmt & AFMT_8BIT)
		return (0x7F);
	return (0);
}

void
format_maximum(const int64_t *src, int64_t *dst, uint32_t ch,
    uint32_t samples, int8_t shift)
{
	const int64_t *end = src + (samples * ch);
	int64_t max[ch];
	int64_t temp;
	uint32_t x;

	memset(max, 0, sizeof(max));

	while (src != end) {
		for (x = 0; x != ch; x++) {
			temp = *src++;
			if (temp < 0)
				temp = -temp;
			if (temp > max[x])
				max[x] = temp;
		}
	}

	for (x = 0; x != ch; x++) {
		if (shift < 0)
			max[x] >>= -shift;
		else
			max[x] <<= shift;
		if (dst[x] < max[x])
			dst[x] = max[x];
	}
}

void
format_remix(int64_t *buffer_data, uint32_t in_chans,
    uint32_t out_chans, uint32_t samples)
{
	uint32_t x;

	if (out_chans > in_chans) {
		uint32_t dst = out_chans * (samples - 1);
		uint32_t src = in_chans * (samples - 1);
		uint32_t fill = out_chans - in_chans;

		for (x = 0; x != samples; x++) {
			memset(buffer_data + dst + in_chans, 0, 8 * fill);
			if (src != dst) {
				memcpy(buffer_data + dst,
				    buffer_data + src,
				    in_chans * 8);
			}
			dst -= out_chans;
			src -= in_chans;
		}
	} else if (out_chans < in_chans) {
		uint32_t dst = 0;
		uint32_t src = 0;

		for (x = 0; x != samples; x++) {
			if (src != dst) {
				memcpy(buffer_data + dst,
				    buffer_data + src,
				    out_chans * 8);
			}
			dst += out_chans;
			src += in_chans;
		}
	}
}

void
format_silence(uint32_t fmt, uint8_t *dst, uint32_t len)
{
	const uint8_t *end = dst + len;

	if (fmt & AFMT_16BIT) {
		uint16_t val;

		if (fmt & (AFMT_U16_LE | AFMT_U16_BE))
			val = 1U << 15;
		else
			val = 0;

		while (dst != end) {
			if (fmt & (AFMT_S16_LE | AFMT_U16_LE)) {
				dst[0] = val;
				dst[1] = val >> 8;
			} else {
				dst[1] = val;
				dst[0] = val >> 8;
			}
			dst += 2;
		}

	} else if (fmt & AFMT_24BIT) {
		uint32_t val;

		if (fmt & (AFMT_U24_LE | AFMT_U24_BE))
			val = 1U << 23;
		else
			val = 0;

		while (dst != end) {
			if (fmt & (AFMT_S24_LE | AFMT_U24_LE)) {
				dst[0] = val;
				dst[1] = val >> 8;
				dst[2] = val >> 16;
			} else {
				dst[2] = val;
				dst[1] = val >> 8;
				dst[0] = val >> 16;
			}
			dst += 3;
		}
	} else if (fmt & AFMT_32BIT) {
		uint32_t val;

		if (fmt & (AFMT_U32_LE | AFMT_U32_BE))
			val = 1U << 31;
		else
			val = 0;

		while (dst != end) {
			if (fmt & (AFMT_S32_LE | AFMT_U32_LE | AFMT_F32_LE)) {
				dst[0] = val;
				dst[1] = val >> 8;
				dst[2] = val >> 16;
				dst[3] = val >> 24;
			} else {
				dst[3] = val;
				dst[2] = val >> 8;
				dst[1] = val >> 16;
				dst[0] = val >> 24;
			}
			dst += 4;
		}

	} else if (fmt & AFMT_8BIT) {
		uint8_t val;

		if (fmt & AFMT_U8)
			val = 1U << 7;
		else
			val = 0;

		while (dst != end) {
			dst[0] = val;
			dst += 1;
		}
	}
}
