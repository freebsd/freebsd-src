/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2007 Eric Anderson <anderson@FreeBSD.org>
 * Copyright (c) 2007 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * Copyright (c) 2025 Dag-Erling Smørgrav <des@FreeBSD.org>
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

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <libutil.h>
#include <stdbool.h>
#include <stdint.h>

static int
expand_impl(const char *buf, uint64_t *num, bool *neg)
{
	char *endptr;
	uintmax_t number;
	unsigned int shift;
	int serrno;

	/*
	 * Skip whitespace and optional sign.
	 */
	while (isspace((unsigned char)*buf))
		buf++;
	if (*buf == '-') {
		*neg = true;
		buf++;
	} else {
		*neg = false;
		if (*buf == '+')
			buf++;
	}

	/*
	 * The next character should be the first digit of the number.  If
	 * we don't enforce this ourselves, strtoumax() will allow further
	 * whitespace and a (second?) sign.
	 */
	if (!isdigit((unsigned char)*buf)) {
		errno = EINVAL;
		return (-1);
	}

	serrno = errno;
	errno = 0;
	number = strtoumax(buf, &endptr, 0);
	if (errno != 0)
		return (-1);
	errno = serrno;

	switch (tolower((unsigned char)*endptr)) {
	case 'e':
		shift = 60;
		endptr++;
		break;
	case 'p':
		shift = 50;
		endptr++;
		break;
	case 't':
		shift = 40;
		endptr++;
		break;
	case 'g':
		shift = 30;
		endptr++;
		break;
	case 'm':
		shift = 20;
		endptr++;
		break;
	case 'k':
		shift = 10;
		endptr++;
		break;
	default:
		shift = 0;
	}

	/*
	 * Treat 'b' as an ignored suffix for all unit except 'b',
	 * otherwise there should be no remaining character(s).
	 */
	if (tolower((unsigned char)*endptr) == 'b')
		endptr++;
	if (*endptr != '\0') {
		errno = EINVAL;
		return (-1);
	}

	/*
	 * Apply the shift and check for overflow.
	 */
	if ((number << shift) >> shift != number) {
		/* Overflow */
		errno = ERANGE;
		return (-1);
	}
	number <<= shift;

	*num = number;
	return (0);
}

int
(expand_number)(const char *buf, int64_t *num)
{
	uint64_t number;
	bool neg;

	/*
	 * Parse the number.
	 */
	if (expand_impl(buf, &number, &neg) != 0)
		return (-1);

	/*
	 * Apply the sign and check for overflow.
	 */
	if (neg) {
		if (number > 0x8000000000000000LLU /* -INT64_MIN */) {
			errno = ERANGE;
			return (-1);
		}
		*num = -number;
	} else {
		if (number > INT64_MAX) {
			errno = ERANGE;
			return (-1);
		}
		*num = number;
	}

	return (0);
}

int
expand_unsigned(const char *buf, uint64_t *num)
{
	uint64_t number;
	bool neg;

	/*
	 * Parse the number.
	 */
	if (expand_impl(buf, &number, &neg) != 0)
		return (-1);

	/*
	 * Negative numbers are out of range.
	 */
	if (neg && number > 0) {
		errno = ERANGE;
		return (-1);
	}

	*num = number;
	return (0);
}
