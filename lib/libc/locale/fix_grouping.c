/*
 * Copyright (c) 2001 Alexey Zelkin
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
 *
 * $FreeBSD$
 */

#include <limits.h>

static const char nogrouping[] = { CHAR_MAX, '\0' };

/*
 * "3;3;-1" -> "\003\003\177"
 * NOTE: one digit numbers assumed!
 */

const char *
__fix_locale_grouping_str(const char *str) {

	char *src, *dst;

	if (str == 0) {
		return nogrouping;
	}
	for (src = (char*)str, dst = (char*)str; *src; src++) {
		char cur;

		/* input string examples: "3;3", "3;2;-1" */
		if (*src == ';')
			continue;
	
		if (*src == '-' && *(src+1) == '1') {
			*dst++ = CHAR_MAX;
			src++;
			continue;
		}

		if (!isdigit(*src)) {
			/* broken grouping string */
			return nogrouping;
		}

		*dst++ = *src - '0';
	}
	*dst = '\0';
	return str;
}
