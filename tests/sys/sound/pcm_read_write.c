/*-
 * Copyright (c) 2025 Florian Walpen
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * These tests exercise conversion functions of the sound module, used to read
 * pcm samples from a buffer, and write pcm samples to a buffer. The test cases
 * are non-exhaustive, but should detect systematic errors in conversion of the
 * various sample formats supported. In particular, the test cases establish
 * correctness independent of the machine's endianness, making them suitable to
 * check for architecture-specific problems.
 */

#include <sys/types.h>
#include <sys/soundcard.h>

#include <atf-c.h>
#include <stdio.h>
#include <string.h>

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/pcm.h>
#include <dev/sound/pcm/g711.h>

/* Generic test data, with buffer content matching the sample values. */
static struct afmt_test_data {
	const char *label;
	uint8_t buffer[4];
	size_t size;
	int format;
	intpcm_t value;
	_Static_assert((sizeof(intpcm_t) == 4),
	    "Test data assumes 32bit, adjust negative values to new size.");
} const afmt_tests[] = {
	/* 8 bit sample formats. */
	{"s8_1", {0x01, 0x00, 0x00, 0x00}, 1, AFMT_S8, 0x00000001},
	{"s8_2", {0x81, 0x00, 0x00, 0x00}, 1, AFMT_S8, 0xffffff81},
	{"u8_1", {0x01, 0x00, 0x00, 0x00}, 1, AFMT_U8, 0xffffff81},
	{"u8_2", {0x81, 0x00, 0x00, 0x00}, 1, AFMT_U8, 0x00000001},

	/* 16 bit sample formats. */
	{"s16le_1", {0x01, 0x02, 0x00, 0x00}, 2, AFMT_S16_LE, 0x00000201},
	{"s16le_2", {0x81, 0x82, 0x00, 0x00}, 2, AFMT_S16_LE, 0xffff8281},
	{"s16be_1", {0x01, 0x02, 0x00, 0x00}, 2, AFMT_S16_BE, 0x00000102},
	{"s16be_2", {0x81, 0x82, 0x00, 0x00}, 2, AFMT_S16_BE, 0xffff8182},
	{"u16le_1", {0x01, 0x02, 0x00, 0x00}, 2, AFMT_U16_LE, 0xffff8201},
	{"u16le_2", {0x81, 0x82, 0x00, 0x00}, 2, AFMT_U16_LE, 0x00000281},
	{"u16be_1", {0x01, 0x02, 0x00, 0x00}, 2, AFMT_U16_BE, 0xffff8102},
	{"u16be_2", {0x81, 0x82, 0x00, 0x00}, 2, AFMT_U16_BE, 0x00000182},

	/* 24 bit sample formats. */
	{"s24le_1", {0x01, 0x02, 0x03, 0x00}, 3, AFMT_S24_LE, 0x00030201},
	{"s24le_2", {0x81, 0x82, 0x83, 0x00}, 3, AFMT_S24_LE, 0xff838281},
	{"s24be_1", {0x01, 0x02, 0x03, 0x00}, 3, AFMT_S24_BE, 0x00010203},
	{"s24be_2", {0x81, 0x82, 0x83, 0x00}, 3, AFMT_S24_BE, 0xff818283},
	{"u24le_1", {0x01, 0x02, 0x03, 0x00}, 3, AFMT_U24_LE, 0xff830201},
	{"u24le_2", {0x81, 0x82, 0x83, 0x00}, 3, AFMT_U24_LE, 0x00038281},
	{"u24be_1", {0x01, 0x02, 0x03, 0x00}, 3, AFMT_U24_BE, 0xff810203},
	{"u24be_2", {0x81, 0x82, 0x83, 0x00}, 3, AFMT_U24_BE, 0x00018283},

	/* 32 bit sample formats. */
	{"s32le_1", {0x01, 0x02, 0x03, 0x04}, 4, AFMT_S32_LE, 0x04030201},
	{"s32le_2", {0x81, 0x82, 0x83, 0x84}, 4, AFMT_S32_LE, 0x84838281},
	{"s32be_1", {0x01, 0x02, 0x03, 0x04}, 4, AFMT_S32_BE, 0x01020304},
	{"s32be_2", {0x81, 0x82, 0x83, 0x84}, 4, AFMT_S32_BE, 0x81828384},
	{"u32le_1", {0x01, 0x02, 0x03, 0x04}, 4, AFMT_U32_LE, 0x84030201},
	{"u32le_2", {0x81, 0x82, 0x83, 0x84}, 4, AFMT_U32_LE, 0x04838281},
	{"u32be_1", {0x01, 0x02, 0x03, 0x04}, 4, AFMT_U32_BE, 0x81020304},
	{"u32be_2", {0x81, 0x82, 0x83, 0x84}, 4, AFMT_U32_BE, 0x01828384},

	/* u-law and A-law sample formats. */
	{"mulaw_1", {0x01, 0x00, 0x00, 0x00}, 1, AFMT_MU_LAW, 0xffffff87},
	{"mulaw_2", {0x81, 0x00, 0x00, 0x00}, 1, AFMT_MU_LAW, 0x00000079},
	{"alaw_1", {0x2a, 0x00, 0x00, 0x00}, 1, AFMT_A_LAW, 0xffffff83},
	{"alaw_2", {0xab, 0x00, 0x00, 0x00}, 1, AFMT_A_LAW, 0x00000079}
};

/* Normalize sample values in strictly correct (but slow) c. */
static intpcm_t
local_normalize(intpcm_t value, int val_bits, int norm_bits)
{
	/* Avoid undefined or implementation defined behavior. */
	if (val_bits < norm_bits)
		/* Multiply instead of left shift (value may be negative). */
		return (value * (1 << (norm_bits - val_bits)));
	else if (val_bits > norm_bits)
		/* Divide instead of right shift (value may be negative). */
		return (value / (1 << (val_bits - norm_bits)));
	return value;
}

/* Restrict magnitude of sample value to 24bit for 32bit calculations. */
static intpcm_t
local_calc_limit(intpcm_t value, int val_bits)
{
	/*
	 * When intpcm32_t is defined to be 32bit, calculations for mixing and
	 * volume changes use 32bit integers instead of 64bit. To get some
	 * headroom for calculations, 32bit sample values are restricted to
	 * 24bit magnitude in that case. Also avoid implementation defined
	 * behavior here.
	 */
	if (sizeof(intpcm32_t) == (32 / 8) && val_bits == 32)
		/* Divide instead of right shift (value may be negative). */
		return (value / (1 << 8));
	return value;
}

/* Lookup tables to read u-law and A-law sample formats. */
static const uint8_t ulaw_to_u8[G711_TABLE_SIZE] = ULAW_TO_U8;
static const uint8_t alaw_to_u8[G711_TABLE_SIZE] = ALAW_TO_U8;

/* Helper function to read one sample value from a buffer. */
static intpcm_t
local_pcm_read(uint8_t *src, uint32_t format)
{
	intpcm_t value;

	switch (format) {
	case AFMT_S8:
		value = _PCM_READ_S8_NE(src);
		break;
	case AFMT_U8:
		value = _PCM_READ_U8_NE(src);
		break;
	case AFMT_S16_LE:
		value = _PCM_READ_S16_LE(src);
		break;
	case AFMT_S16_BE:
		value = _PCM_READ_S16_BE(src);
		break;
	case AFMT_U16_LE:
		value = _PCM_READ_U16_LE(src);
		break;
	case AFMT_U16_BE:
		value = _PCM_READ_U16_BE(src);
		break;
	case AFMT_S24_LE:
		value = _PCM_READ_S24_LE(src);
		break;
	case AFMT_S24_BE:
		value = _PCM_READ_S24_BE(src);
		break;
	case AFMT_U24_LE:
		value = _PCM_READ_U24_LE(src);
		break;
	case AFMT_U24_BE:
		value = _PCM_READ_U24_BE(src);
		break;
	case AFMT_S32_LE:
		value = _PCM_READ_S32_LE(src);
		break;
	case AFMT_S32_BE:
		value = _PCM_READ_S32_BE(src);
		break;
	case AFMT_U32_LE:
		value = _PCM_READ_U32_LE(src);
		break;
	case AFMT_U32_BE:
		value = _PCM_READ_U32_BE(src);
		break;
	case AFMT_MU_LAW:
		value = _G711_TO_INTPCM(ulaw_to_u8, *src);
		break;
	case AFMT_A_LAW:
		value = _G711_TO_INTPCM(alaw_to_u8, *src);
		break;
	default:
		value = 0;
	}

	return (value);
}

/* Helper function to read one sample value from a buffer for calculations. */
static intpcm_t
local_pcm_read_calc(uint8_t *src, uint32_t format)
{
	intpcm_t value;

	switch (format) {
	case AFMT_S8:
		value = PCM_READ_S8_NE(src);
		break;
	case AFMT_U8:
		value = PCM_READ_U8_NE(src);
		break;
	case AFMT_S16_LE:
		value = PCM_READ_S16_LE(src);
		break;
	case AFMT_S16_BE:
		value = PCM_READ_S16_BE(src);
		break;
	case AFMT_U16_LE:
		value = PCM_READ_U16_LE(src);
		break;
	case AFMT_U16_BE:
		value = PCM_READ_U16_BE(src);
		break;
	case AFMT_S24_LE:
		value = PCM_READ_S24_LE(src);
		break;
	case AFMT_S24_BE:
		value = PCM_READ_S24_BE(src);
		break;
	case AFMT_U24_LE:
		value = PCM_READ_U24_LE(src);
		break;
	case AFMT_U24_BE:
		value = PCM_READ_U24_BE(src);
		break;
	case AFMT_S32_LE:
		value = PCM_READ_S32_LE(src);
		break;
	case AFMT_S32_BE:
		value = PCM_READ_S32_BE(src);
		break;
	case AFMT_U32_LE:
		value = PCM_READ_U32_LE(src);
		break;
	case AFMT_U32_BE:
		value = PCM_READ_U32_BE(src);
		break;
	case AFMT_MU_LAW:
		value = _G711_TO_INTPCM(ulaw_to_u8, *src);
		break;
	case AFMT_A_LAW:
		value = _G711_TO_INTPCM(alaw_to_u8, *src);
		break;
	default:
		value = 0;
	}

	return (value);
}

/* Helper function to read one normalized sample from a buffer. */
static intpcm_t
local_pcm_read_norm(uint8_t *src, uint32_t format)
{
	intpcm_t value;

	value = local_pcm_read(src, format);
	value <<= (32 - AFMT_BIT(format));
	return (value);
}

/* Lookup tables to write u-law and A-law sample formats. */
static const uint8_t u8_to_ulaw[G711_TABLE_SIZE] = U8_TO_ULAW;
static const uint8_t u8_to_alaw[G711_TABLE_SIZE] = U8_TO_ALAW;

/* Helper function to write one sample value to a buffer. */
static void
local_pcm_write(uint8_t *dst, intpcm_t value, uint32_t format)
{
	switch (format) {
	case AFMT_S8:
		_PCM_WRITE_S8_NE(dst, value);
		break;
	case AFMT_U8:
		_PCM_WRITE_U8_NE(dst, value);
		break;
	case AFMT_S16_LE:
		_PCM_WRITE_S16_LE(dst, value);
		break;
	case AFMT_S16_BE:
		_PCM_WRITE_S16_BE(dst, value);
		break;
	case AFMT_U16_LE:
		_PCM_WRITE_U16_LE(dst, value);
		break;
	case AFMT_U16_BE:
		_PCM_WRITE_U16_BE(dst, value);
		break;
	case AFMT_S24_LE:
		_PCM_WRITE_S24_LE(dst, value);
		break;
	case AFMT_S24_BE:
		_PCM_WRITE_S24_BE(dst, value);
		break;
	case AFMT_U24_LE:
		_PCM_WRITE_U24_LE(dst, value);
		break;
	case AFMT_U24_BE:
		_PCM_WRITE_U24_BE(dst, value);
		break;
	case AFMT_S32_LE:
		_PCM_WRITE_S32_LE(dst, value);
		break;
	case AFMT_S32_BE:
		_PCM_WRITE_S32_BE(dst, value);
		break;
	case AFMT_U32_LE:
		_PCM_WRITE_U32_LE(dst, value);
		break;
	case AFMT_U32_BE:
		_PCM_WRITE_U32_BE(dst, value);
		break;
	case AFMT_MU_LAW:
		*dst = _INTPCM_TO_G711(u8_to_ulaw, value);
		break;
	case AFMT_A_LAW:
		*dst = _INTPCM_TO_G711(u8_to_alaw, value);
		break;
	default:
		value = 0;
	}
}

/* Helper function to write one calculation sample value to a buffer. */
static void
local_pcm_write_calc(uint8_t *dst, intpcm_t value, uint32_t format)
{
	switch (format) {
	case AFMT_S8:
		PCM_WRITE_S8_NE(dst, value);
		break;
	case AFMT_U8:
		PCM_WRITE_U8_NE(dst, value);
		break;
	case AFMT_S16_LE:
		PCM_WRITE_S16_LE(dst, value);
		break;
	case AFMT_S16_BE:
		PCM_WRITE_S16_BE(dst, value);
		break;
	case AFMT_U16_LE:
		PCM_WRITE_U16_LE(dst, value);
		break;
	case AFMT_U16_BE:
		PCM_WRITE_U16_BE(dst, value);
		break;
	case AFMT_S24_LE:
		PCM_WRITE_S24_LE(dst, value);
		break;
	case AFMT_S24_BE:
		PCM_WRITE_S24_BE(dst, value);
		break;
	case AFMT_U24_LE:
		PCM_WRITE_U24_LE(dst, value);
		break;
	case AFMT_U24_BE:
		PCM_WRITE_U24_BE(dst, value);
		break;
	case AFMT_S32_LE:
		PCM_WRITE_S32_LE(dst, value);
		break;
	case AFMT_S32_BE:
		PCM_WRITE_S32_BE(dst, value);
		break;
	case AFMT_U32_LE:
		PCM_WRITE_U32_LE(dst, value);
		break;
	case AFMT_U32_BE:
		PCM_WRITE_U32_BE(dst, value);
		break;
	case AFMT_MU_LAW:
		*dst = _INTPCM_TO_G711(u8_to_ulaw, value);
		break;
	case AFMT_A_LAW:
		*dst = _INTPCM_TO_G711(u8_to_alaw, value);
		break;
	default:
		value = 0;
	}
}

/* Helper function to write one normalized sample to a buffer. */
static void
local_pcm_write_norm(uint8_t *dst, intpcm_t value, uint32_t format)
{
	local_pcm_write(dst, value >> (32 - AFMT_BIT(format)), format);
}

ATF_TC(pcm_read);
ATF_TC_HEAD(pcm_read, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Read and verify different pcm sample formats.");
}
ATF_TC_BODY(pcm_read, tc)
{
	const struct afmt_test_data *test;
	uint8_t src[4];
	intpcm_t expected, result;
	size_t i;

	for (i = 0; i < nitems(afmt_tests); i++) {
		test = &afmt_tests[i];

		/* Copy byte representation, fill with distinctive pattern. */
		memset(src, 0x66, sizeof(src));
		memcpy(src, test->buffer, test->size);

		/* Read sample at format magnitude. */
		expected = test->value;
		result = local_pcm_read(src, test->format);
		ATF_CHECK_MSG(result == expected,
		    "pcm_read[\"%s\"].value: expected=0x%08x, result=0x%08x",
		    test->label, expected, result);

		/* Read sample at format magnitude, for calculations. */
		expected = local_calc_limit(test->value, test->size * 8);
		result = local_pcm_read_calc(src, test->format);
		ATF_CHECK_MSG(result == expected,
		    "pcm_read[\"%s\"].calc: expected=0x%08x, result=0x%08x",
		    test->label, expected, result);

		/* Read sample at full 32 bit magnitude. */
		expected = local_normalize(test->value, test->size * 8, 32);
		result = local_pcm_read_norm(src, test->format);
		ATF_CHECK_MSG(result == expected,
		    "pcm_read[\"%s\"].norm: expected=0x%08x, result=0x%08x",
		    test->label, expected, result);
	}
}

ATF_TC(pcm_write);
ATF_TC_HEAD(pcm_write, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Write and verify different pcm sample formats.");
}
ATF_TC_BODY(pcm_write, tc)
{
	const struct afmt_test_data *test;
	uint8_t expected[4];
	uint8_t dst[4];
	intpcm_t value;
	size_t i;

	for (i = 0; i < nitems(afmt_tests); i++) {
		test = &afmt_tests[i];

		/* Write sample of format specific magnitude. */
		memcpy(expected, test->buffer, sizeof(expected));
		memset(dst, 0x00, sizeof(dst));
		value = test->value;
		local_pcm_write(dst, value, test->format);
		ATF_CHECK_MSG(memcmp(dst, expected, sizeof(dst)) == 0,
		    "pcm_write[\"%s\"].value: "
		    "expected={0x%02x, 0x%02x, 0x%02x, 0x%02x}, "
		    "result={0x%02x, 0x%02x, 0x%02x, 0x%02x}, ", test->label,
		    expected[0], expected[1], expected[2], expected[3],
		    dst[0], dst[1], dst[2], dst[3]);

		/* Write sample of format specific, calculation magnitude. */
		memcpy(expected, test->buffer, sizeof(expected));
		memset(dst, 0x00, sizeof(dst));
		value = local_calc_limit(test->value, test->size * 8);
		if (value != test->value) {
			/*
			 * 32 bit sample was reduced to 24 bit resolution
			 * for calculation, least significant byte is lost.
			 */
			if (test->format & AFMT_BIGENDIAN)
				expected[3] = 0x00;
			else
				expected[0] = 0x00;
		}
		local_pcm_write_calc(dst, value, test->format);
		ATF_CHECK_MSG(memcmp(dst, expected, sizeof(dst)) == 0,
		    "pcm_write[\"%s\"].calc: "
		    "expected={0x%02x, 0x%02x, 0x%02x, 0x%02x}, "
		    "result={0x%02x, 0x%02x, 0x%02x, 0x%02x}, ", test->label,
		    expected[0], expected[1], expected[2], expected[3],
		    dst[0], dst[1], dst[2], dst[3]);

		/* Write normalized sample of full 32 bit magnitude. */
		memcpy(expected, test->buffer, sizeof(expected));
		memset(dst, 0x00, sizeof(dst));
		value = local_normalize(test->value, test->size * 8, 32);
		local_pcm_write_norm(dst, value, test->format);
		ATF_CHECK_MSG(memcmp(dst, expected, sizeof(dst)) == 0,
		    "pcm_write[\"%s\"].norm: "
		    "expected={0x%02x, 0x%02x, 0x%02x, 0x%02x}, "
		    "result={0x%02x, 0x%02x, 0x%02x, 0x%02x}, ", test->label,
		    expected[0], expected[1], expected[2], expected[3],
		    dst[0], dst[1], dst[2], dst[3]);
	}
}

ATF_TC(pcm_format_bits);
ATF_TC_HEAD(pcm_format_bits, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Verify bit width of different pcm sample formats.");
}
ATF_TC_BODY(pcm_format_bits, tc)
{
	const struct afmt_test_data *test;
	size_t bits;
	size_t i;

	for (i = 0; i < nitems(afmt_tests); i++) {
		test = &afmt_tests[i];

		/* Check bit width determined for given sample format. */
		bits = AFMT_BIT(test->format);
		ATF_CHECK_MSG(bits == test->size * 8,
		    "format_bits[%zu].size: expected=%zu, result=%zu",
		    i, test->size * 8, bits);
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, pcm_read);
	ATF_TP_ADD_TC(tp, pcm_write);
	ATF_TP_ADD_TC(tp, pcm_format_bits);

	return atf_no_error();
}
