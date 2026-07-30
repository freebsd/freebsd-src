/*-
 * Copyright (c) 2026 Kaif Khan
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"

#define __LIBARCHIVE_TEST
#include "archive_string.h"

/*
 * archive_strncpy_l() passes a pointer and a byte count to the string
 * conversion converter and does not require the buffer to be NUL
 * terminated: the readers hand it a __archive_read_ahead() window sized
 * exactly to the field length (mbsnbytes() returns that length when no
 * NUL is present).  The best-effort converter must therefore stop after
 * the declared number of bytes.
 *
 * Requesting a conversion from a charset iconv cannot open selects
 * best_effort_strncat_in_locale() with sc->same == 0.  If a platform's
 * iconv unexpectedly accepts the name the conversion still succeeds and
 * the assertions hold, so this never yields a false failure.
 */
DEFINE_TEST(test_archive_string_conversion_overread)
{
	struct archive *a;
	struct archive_string_conv *sc;

	assert((a = archive_read_new()) != NULL);
	sc = archive_string_conversion_from_charset(a, "NO-SUCH-CHARSET-8859", 1);
	assert(sc != NULL);

	/*
	 * The buffer declares 8 bytes but is followed by 4 more non-NUL
	 * bytes and then a NUL.  A converter that honors the declared length
	 * copies 8 bytes; the over-read walks on to the NUL and copies 12.
	 */
	{
		struct archive_string as;
		static const char data[] = "AAAAAAAABBBB"; /* 8 + 4, then NUL */

		archive_string_init(&as);
		assertEqualInt(0, archive_strncpy_l(&as, data, 8, sc));
		assertEqualInt(8, (int)as.length);
		assertEqualMem(as.s, "AAAAAAAA", 8);
		archive_string_free(&as);
	}

	/*
	 * Exact-length heap buffer with no trailing NUL: any read past the
	 * end is out of bounds and observable to AddressSanitizer.
	 */
	{
		struct archive_string as;
		size_t n = 8;
		char *buf = malloc(n);

		assert(buf != NULL);
		memset(buf, 'A', n);
		archive_string_init(&as);
		assertEqualInt(0, archive_strncpy_l(&as, buf, n, sc)); /* must not read buf[n] */
		assertEqualInt(8, (int)as.length);
		archive_string_free(&as);
		free(buf);
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
