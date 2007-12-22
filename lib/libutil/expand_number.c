/*-
 * Copyright (c) 2007 Eric Anderson <anderson@FreeBSD.org>
 * Copyright (c) 2007 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <libutil.h>
#include <stdint.h>

/*
 * Convert an expression of the following forms to a int64_t.
 * 	1) A positive decimal number.
 *	2) A positive decimal number followed by a 'b' or 'B' (mult by 1).
 *	3) A positive decimal number followed by a 'k' or 'K' (mult by 1 << 10).
 *	4) A positive decimal number followed by a 'm' or 'M' (mult by 1 << 20).
 *	5) A positive decimal number followed by a 'g' or 'G' (mult by 1 << 30).
 *	6) A positive decimal number followed by a 't' or 'T' (mult by 1 << 40).
 *	7) A positive decimal number followed by a 'p' or 'P' (mult by 1 << 50).
 *	8) A positive decimal number followed by a 'e' or 'E' (mult by 1 << 60).
 */
int
expand_number(char *buf, int64_t *num)
{
	static const char unit[] = "bkmgtpe";
	char *endptr, s;
	int64_t number;
	int i;

	number = strtoimax(buf, &endptr, 0);

	if (endptr == buf) {
		/* No valid digits. */
		errno = EINVAL;
		return (-1);
	}

	if (*endptr == '\0') {
		/* No unit. */
		*num = number;
		return (0);
	}

	s = tolower(*endptr);
	switch (s) {
	case 'b':
	case 'k':
	case 'm':
	case 'g':
	case 't':
	case 'p':
	case 'e':
		break;
	default:
		/* Unrecognized unit. */
		errno = EINVAL;
		return (-1);
	}

	for (i = 0; unit[i] != '\0'; i++) {
		if (s == unit[i])
			break;
		if ((number < 0 && (number << 10) > number) ||
		    (number >= 0 && (number << 10) < number)) {
			errno = ERANGE;
			return (-1);
		}
		number <<= 10;
	}

	*num = number;
	return (0);
}
