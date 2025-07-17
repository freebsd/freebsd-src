/*-
 * Copyright 2020 Yuri Pankov <yuripv@FreeBSD.org>
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

#include <locale.h>
#include <stdio.h>

#include <utf8proc.h>

static int
width_of(int32_t wc)
{

	/*
	 * Hangul Jamo medial vowels and final consonants are more of
	 * a combining character, and should be considered zero-width.
	 */
	if (wc >= 0x1160 && wc <= 0x11ff)
		return (0);

	/* No override by default, trust utf8proc's width. */
	return (utf8proc_charwidth(wc));
}

int
main(void)
{
	int32_t wc;
	int i, wcw;
	utf8proc_category_t wcc;

	setlocale(LC_CTYPE, "C.UTF-8");

	printf("%s\n", utf8proc_version());

	for (wc = 0; wc < 0x110000; wc++) {
		wcc = utf8proc_category(wc);
		if (wcc == UTF8PROC_CATEGORY_CC)
			continue;
		wcw = width_of(wc);
		if (wcw == 1)
			continue;

		printf("%04X %d\n", wc, wcw);
	}

	return (0);
}
