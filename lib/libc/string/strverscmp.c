/*-
* SPDX-License-Identifier: BSD-2-Clause
*
* Copyright (c) 2022 Obiwac,
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
* THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*/

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)strverscmp.c	1.0 6/30/22";
#endif /* LIBC_SCCS and not lint */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <ctype.h>

int strverscmp(const char* s1, const char* s2) {
	// if pointers are aliased, no need to go through to process of comparing them

	if (s1 == s2) {
		return 0;
	}

	const unsigned char* u1 = (const void*) s1;
	const unsigned char* u2 = (const void*) s2;

	for (; *u1 && *u2; u1++, u2++) {
		// leading zeroes; we're dealing with the fractional part of a number

		if (*u1 == '0' || *u2 == '0') {
			// count leading zeros (more leading zeroes == smaller number)

			unsigned n1 = 0;
			unsigned n2 = 0;

			for (; *u1 == '0'; u1++) n1++;
			for (; *u2 == '0'; u2++) n2++;

			if (n1 != n2) {
				return n2 - n1;
			}

			// handle the case where 000 < 09

			if (!*u1 && isdigit(*u2)) return  1;
			if (!*u2 && isdigit(*u1)) return -1;

			// for all other cases, compare each digit until there are none left

			for (; isdigit(*u1) && isdigit(*u2); u1++, u2++) {
				if (*u1 != *u2) {
					return *u1 - *u2;
				}
			}

			u1--, u2--;
		}

		// no leading; we're simply comparing two numbers

		else if (isdigit(*u1) && isdigit(*u2)) {
			const unsigned char* o1 = u1;
			const unsigned char* o2 = u2;

			// count digits (more digits == larger number)

			unsigned n1 = 0;
			unsigned n2 = 0;

			for (; isdigit(*u1); u1++) n1++;
			for (; isdigit(*u2); u2++) n2++;

			if (n1 != n2) {
				return n1 - n2;
			}

			// if there're the same number of digits,
			// go back and compare each digit until there are none left

			u1 = o1, u2 = o2;

			for (; isdigit(*u1) && isdigit(*u2); u1++, u2++) {
				if (*u1 != *u2) {
					return *u1 - *u2;
				}
			}

			u1--, u2--;
		}

		// for the rest, we can just fallback to a regular strcmp

		if (*u1 != *u2) {
			return *u1 - *u2;
		}
	}

	return *u1 - *u2;
}
