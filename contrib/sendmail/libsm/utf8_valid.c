/*
 * Copyright (c) 2020 Proofpoint, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#include <sm/gen.h>
#include <sm/sendmail.h>
#include <sm/ixlen.h>

#if USE_EAI

/*
**  legal utf-8 byte sequence
**  http://www.unicode.org/versions/Unicode6.0.0/ch03.pdf - page 94
**
**   Code Points        1st       2s       3s       4s
**  U+0000..U+007F     00..7F
**  U+0080..U+07FF     C2..DF   80..BF
**  U+0800..U+0FFF     E0       A0..BF   80..BF
**  U+1000..U+CFFF     E1..EC   80..BF   80..BF
**  U+D000..U+D7FF     ED       80..9F   80..BF
**  U+E000..U+FFFF     EE..EF   80..BF   80..BF
**  U+10000..U+3FFFF   F0       90..BF   80..BF   80..BF
**  U+40000..U+FFFFF   F1..F3   80..BF   80..BF   80..BF
**  U+100000..U+10FFFF F4       80..8F   80..BF   80..BF
*/

/*
**  based on
**  https://github.com/lemire/fastvalidate-utf-8.git
**  which is distributed under an MIT license (besides others).
*/

bool
utf8_valid(b, length)
	const char *b;
	size_t length;
{
	const unsigned char *bytes;
	size_t index;

	bytes = (const unsigned char *)b;
	index = 0;
	while (true)
	{
		unsigned char byte1;

		do { /* fast ASCII Path */
			if (index >= length)
				return true;
			byte1 = bytes[index++];
		} while (byte1 < 0x80);
		if (byte1 < 0xE0)
		{
			/* Two-byte form. */
			if (index == length)
				return false;
			if (byte1 < 0xC2 || bytes[index++] > 0xBF)
				return false;
		}
		else if (byte1 < 0xF0)
		{
			/* Three-byte form. */
			if (index + 1 >= length)
				return false;
			unsigned char byte2 = bytes[index++];
			if (byte2 > 0xBF
			    /* Overlong? 5 most significant bits must not all be zero. */
			    || (byte1 == 0xE0 && byte2 < 0xA0)
			    /* Check for illegal surrogate codepoints. */
			    || (byte1 == 0xED && 0xA0 <= byte2)
			    /* Third byte trailing-byte test. */
			    || bytes[index++] > 0xBF)
				return false;
		}
		else
		{

			/* Four-byte form. */
			if (index + 2 >= length)
				return false;
			int byte2 = bytes[index++];
			if (byte2 > 0xBF
			    /* Check that 1 <= plane <= 16. Tricky optimized form of: */
			    /* if (byte1 > (byte) 0xF4 */
			    /*    || byte1 == (byte) 0xF0 && byte2 < (byte) 0x90 */
			    /*    || byte1 == (byte) 0xF4 && byte2 > (byte) 0x8F) */
			    || (((byte1 << 28) + (byte2 - 0x90)) >> 30) != 0
			    /* Third byte trailing-byte test */
			    || bytes[index++] > 0xBF
			    /* Fourth byte trailing-byte test */
			    || bytes[index++] > 0xBF)
				return false;
		}
	}
	/* NOTREACHED */
	return false;
}
#endif /* USE_EAI */
