/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2006-2009 Ariff Abdullah <ariff@FreeBSD.org>
 * All rights reserved.
 * Copyright (c) 2024-2025 The FreeBSD Foundation
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

#ifndef _SND_PCM_H_
#define _SND_PCM_H_

#include <sys/param.h>

#include <dev/sound/pcm/g711.h>

#ifndef _KERNEL
#include <assert.h>	/* for __assert_unreachable() */
#endif

/*
 * Automatically turn on 64bit arithmetic on suitable archs
 * (amd64 64bit, etc..) for wider 32bit samples / integer processing.
 */
#if LONG_BIT >= 64
#undef SND_PCM_64
#define SND_PCM_64	1
#endif

typedef int32_t intpcm_t;

typedef int32_t intpcm8_t;
typedef int32_t intpcm16_t;
typedef int32_t intpcm24_t;

typedef uint32_t uintpcm_t;

typedef uint32_t uintpcm8_t;
typedef uint32_t uintpcm16_t;
typedef uint32_t uintpcm24_t;

#ifdef SND_PCM_64
typedef int64_t  intpcm32_t;
typedef uint64_t uintpcm32_t;
#else
typedef int32_t  intpcm32_t;
typedef uint32_t uintpcm32_t;
#endif

typedef int64_t intpcm64_t;
typedef uint64_t uintpcm64_t;

/* 32bit fixed point shift */
#define	PCM_FXSHIFT	8

#define PCM_S8_MAX	  0x7f
#define PCM_S8_MIN	 -0x80
#define PCM_S16_MAX	  0x7fff
#define PCM_S16_MIN	 -0x8000
#define PCM_S24_MAX	  0x7fffff
#define PCM_S24_MIN	 -0x800000
#ifdef SND_PCM_64
#if LONG_BIT >= 64
#define PCM_S32_MAX	  0x7fffffffL
#define PCM_S32_MIN	 -0x80000000L
#else
#define PCM_S32_MAX	  0x7fffffffLL
#define PCM_S32_MIN	 -0x80000000LL
#endif
#else
#define PCM_S32_MAX	  0x7fffffff
#define PCM_S32_MIN	(-0x7fffffff - 1)
#endif

/* Bytes-per-sample definition */
#define PCM_8_BPS	1
#define PCM_16_BPS	2
#define PCM_24_BPS	3
#define PCM_32_BPS	4

#define INTPCM_T(v)	((intpcm_t)(v))
#define INTPCM8_T(v)	((intpcm8_t)(v))
#define INTPCM16_T(v)	((intpcm16_t)(v))
#define INTPCM24_T(v)	((intpcm24_t)(v))
#define INTPCM32_T(v)	((intpcm32_t)(v))

static const struct {
	const uint8_t ulaw_to_u8[G711_TABLE_SIZE];
	const uint8_t alaw_to_u8[G711_TABLE_SIZE];
	const uint8_t u8_to_ulaw[G711_TABLE_SIZE];
	const uint8_t u8_to_alaw[G711_TABLE_SIZE];
} xlaw_conv_tables = {
	ULAW_TO_U8,
	ALAW_TO_U8,
	U8_TO_ULAW,
	U8_TO_ALAW
};

/*
 * Functions for reading/writing PCM integer sample values from bytes array.
 * Since every process is done using signed integer (and to make our life less
 * miserable), unsigned sample will be converted to its signed counterpart and
 * restored during writing back.
 */
static __always_inline __unused intpcm_t
pcm_sample_read(const uint8_t *src, uint32_t fmt)
{
	intpcm_t v, e, m;
	bool s;

	fmt = AFMT_ENCODING(fmt);

	switch (fmt) {
	case AFMT_AC3:
		v = 0;
		break;
	case AFMT_MU_LAW:
		v = _G711_TO_INTPCM(xlaw_conv_tables.ulaw_to_u8, *src);
		break;
	case AFMT_A_LAW:
		v = _G711_TO_INTPCM(xlaw_conv_tables.alaw_to_u8, *src);
		break;
	case AFMT_S8:
		v = INTPCM_T((int8_t)*src);
		break;
	case AFMT_U8:
		v = INTPCM_T((int8_t)(*src ^ 0x80));
		break;
	case AFMT_S16_LE:
		v = INTPCM_T(src[0] | (int8_t)src[1] << 8);
		break;
	case AFMT_S16_BE:
		v = INTPCM_T(src[1] | (int8_t)src[0] << 8);
		break;
	case AFMT_U16_LE:
		v = INTPCM_T(src[0] | (int8_t)(src[1] ^ 0x80) << 8);
		break;
	case AFMT_U16_BE:
		v = INTPCM_T(src[1] | (int8_t)(src[0] ^ 0x80) << 8);
		break;
	case AFMT_S24_LE:
		v = INTPCM_T(src[0] | src[1] << 8 | (int8_t)src[2] << 16);
		break;
	case AFMT_S24_BE:
		v = INTPCM_T(src[2] | src[1] << 8 | (int8_t)src[0] << 16);
		break;
	case AFMT_U24_LE:
		v = INTPCM_T(src[0] | src[1] << 8 |
		    (int8_t)(src[2] ^ 0x80) << 16);
		break;
	case AFMT_U24_BE:
		v = INTPCM_T(src[2] | src[1] << 8 |
		    (int8_t)(src[0] ^ 0x80) << 16);
		break;
	case AFMT_S32_LE:
		v = INTPCM_T(src[0] | src[1] << 8 | src[2] << 16 |
		    (int8_t)src[3] << 24);
		break;
	case AFMT_S32_BE:
		v = INTPCM_T(src[3] | src[2] << 8 | src[1] << 16 |
		    (int8_t)src[0] << 24);
		break;
	case AFMT_U32_LE:
		v = INTPCM_T(src[0] | src[1] << 8 | src[2] << 16 |
		    (int8_t)(src[3] ^ 0x80) << 24);
		break;
	case AFMT_U32_BE:
		v = INTPCM_T(src[3] | src[2] << 8 | src[1] << 16 |
		    (int8_t)(src[0] ^ 0x80) << 24);
		break;
	case AFMT_F32_LE:	/* FALLTHROUGH */
	case AFMT_F32_BE:
		if (fmt == AFMT_F32_LE) {
			v = INTPCM_T(src[0] | src[1] << 8 | src[2] << 16 |
			    (int8_t)src[3] << 24);
		} else {
			v = INTPCM_T(src[3] | src[2] << 8 | src[1] << 16 |
			    (int8_t)src[0] << 24);
		}
		e = (v >> 23) & 0xff;
		/* NaN, +/- Inf  or too small */
		if (e == 0xff || e < 96) {
			v = INTPCM_T(0);
			break;
		}
		s = v & 0x80000000U;
		if (e > 126) {
			v = INTPCM_T((s == 0) ? PCM_S32_MAX : PCM_S32_MIN);
			break;
		}
		m = 0x800000 | (v & 0x7fffff);
		e += 8 - 127;
		if (e < 0)
			m >>= -e;
		else
			m <<= e;
		v = INTPCM_T((s == 0) ? m : -m);
		break;
	default:
		v = 0;
		printf("%s(): unknown format: 0x%08x\n", __func__, fmt);
		__assert_unreachable();
	}

	return (v);
}

/*
 * Read sample and normalize to 32-bit magnitude.
 */
static __always_inline __unused intpcm_t
pcm_sample_read_norm(const uint8_t *src, uint32_t fmt)
{
	return (pcm_sample_read(src, fmt) << (32 - AFMT_BIT(fmt)));
}

/*
 * Read sample and restrict magnitude to 24 bits.
 */
static __always_inline __unused intpcm_t
pcm_sample_read_calc(const uint8_t *src, uint32_t fmt)
{
	intpcm_t v;

	v = pcm_sample_read(src, fmt);

#ifndef SND_PCM_64
	/*
	 * Dynamic range for humans: ~140db.
	 *
	 * 16bit = 96db (close enough)
	 * 24bit = 144db (perfect)
	 * 32bit = 196db (way too much)
	 *
	 * 24bit is pretty much sufficient for our signed integer processing.
	 * Also, to avoid overflow, we truncate 32bit (and only 32bit) samples
	 * down to 24bit (see below for the reason), unless SND_PCM_64 is
	 * defined.
	 */
	if (fmt & AFMT_32BIT)
		v >>= PCM_FXSHIFT;
#endif

	return (v);
}

static __always_inline __unused void
pcm_sample_write(uint8_t *dst, intpcm_t v, uint32_t fmt)
{
	intpcm_t r, e;

	fmt = AFMT_ENCODING(fmt);

	if (fmt & (AFMT_F32_LE | AFMT_F32_BE)) {
		if (v == 0)
			r = 0;
		else if (v == PCM_S32_MAX)
			r = 0x3f800000;
		else if (v == PCM_S32_MIN)
			r = 0x80000000U | 0x3f800000;
		else {
			r = 0;
			if (v < 0) {
				r |= 0x80000000U;
				v = -v;
			}
			e = 127 - 8;
			while ((v & 0x7f000000) != 0) {
				v >>= 1;
				e++;
			}
			while ((v & 0x7f800000) == 0) {
				v <<= 1;
				e--;
			}
			r |= (e & 0xff) << 23;
			r |= v & 0x7fffff;
		}
		v = r;
	}

	switch (fmt) {
	case AFMT_AC3:
		*(int16_t *)dst = 0;
		break;
	case AFMT_MU_LAW:
		*dst = _INTPCM_TO_G711(xlaw_conv_tables.u8_to_ulaw, v);
		break;
	case AFMT_A_LAW:
		*dst = _INTPCM_TO_G711(xlaw_conv_tables.u8_to_alaw, v);
		break;
	case AFMT_S8:
		*(int8_t *)dst = v;
		break;
	case AFMT_U8:
		*(int8_t *)dst = v ^ 0x80;
		break;
	case AFMT_S16_LE:
		dst[0] = v;
		dst[1] = v >> 8;
		break;
	case AFMT_S16_BE:
		dst[1] = v;
		dst[0] = v >> 8;
		break;
	case AFMT_U16_LE:
		dst[0] = v;
		dst[1] = (v >> 8) ^ 0x80;
		break;
	case AFMT_U16_BE:
		dst[1] = v;
		dst[0] = (v >> 8) ^ 0x80;
		break;
	case AFMT_S24_LE:
		dst[0] = v;
		dst[1] = v >> 8;
		dst[2] = v >> 16;
		break;
	case AFMT_S24_BE:
		dst[2] = v;
		dst[1] = v >> 8;
		dst[0] = v >> 16;
		break;
	case AFMT_U24_LE:
		dst[0] = v;
		dst[1] = v >> 8;
		dst[2] = (v >> 16) ^ 0x80;
		break;
	case AFMT_U24_BE:
		dst[2] = v;
		dst[1] = v >> 8;
		dst[0] = (v >> 16) ^ 0x80;
		break;
	case AFMT_S32_LE:	/* FALLTHROUGH */
	case AFMT_F32_LE:
		dst[0] = v;
		dst[1] = v >> 8;
		dst[2] = v >> 16;
		dst[3] = v >> 24;
		break;
	case AFMT_S32_BE:	/* FALLTHROUGH */
	case AFMT_F32_BE:
		dst[3] = v;
		dst[2] = v >> 8;
		dst[1] = v >> 16;
		dst[0] = v >> 24;
		break;
	case AFMT_U32_LE:
		dst[0] = v;
		dst[1] = v >> 8;
		dst[2] = v >> 16;
		dst[3] = (v >> 24) ^ 0x80;
		break;
	case AFMT_U32_BE:
		dst[3] = v;
		dst[2] = v >> 8;
		dst[1] = v >> 16;
		dst[0] = (v >> 24) ^ 0x80;
		break;
	default:
		printf("%s(): unknown format: 0x%08x\n", __func__, fmt);
		__assert_unreachable();
	}
}

/*
 * Write sample and normalize to original magnitude.
 */
static __always_inline __unused void
pcm_sample_write_norm(uint8_t *dst, intpcm_t v, uint32_t fmt)
{
	pcm_sample_write(dst, v >> (32 - AFMT_BIT(fmt)), fmt);
}

/*
 * To be used with pcm_sample_read_calc().
 */
static __always_inline __unused void
pcm_sample_write_calc(uint8_t *dst, intpcm_t v, uint32_t fmt)
{
#ifndef SND_PCM_64
	/* Shift back to 32-bit magnitude. */
	if (fmt & AFMT_32BIT)
		v <<= PCM_FXSHIFT;
#endif
	pcm_sample_write(dst, v, fmt);
}

static __always_inline __unused intpcm_t
pcm_clamp(intpcm32_t sample, uint32_t fmt)
{
	fmt = AFMT_ENCODING(fmt);

	switch (AFMT_BIT(fmt)) {
	case 8:
		return ((sample > PCM_S8_MAX) ? PCM_S8_MAX :
		    ((sample < PCM_S8_MIN) ? PCM_S8_MIN : sample));
	case 16:
		return ((sample > PCM_S16_MAX) ? PCM_S16_MAX :
		    ((sample < PCM_S16_MIN) ? PCM_S16_MIN : sample));
	case 24:
		return ((sample > PCM_S24_MAX) ? PCM_S24_MAX :
		    ((sample < PCM_S24_MIN) ? PCM_S24_MIN : sample));
	case 32:
		return ((sample > PCM_S32_MAX) ? PCM_S32_MAX :
		    ((sample < PCM_S32_MIN) ? PCM_S32_MIN : sample));
	default:
		printf("%s(): unknown format: 0x%08x\n", __func__, fmt);
		__assert_unreachable();
	}
}

static __always_inline __unused intpcm_t
pcm_clamp_calc(intpcm32_t sample, uint32_t fmt)
{
#ifndef SND_PCM_64
	if (fmt & AFMT_32BIT) {
		return ((sample > PCM_S24_MAX) ? PCM_S32_MAX :
		    ((sample < PCM_S24_MIN) ? PCM_S32_MIN :
		    sample << PCM_FXSHIFT));
	}
#endif

	return (pcm_clamp(sample, fmt));
}

#endif	/* !_SND_PCM_H_ */
