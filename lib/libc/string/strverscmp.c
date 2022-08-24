/*-
* SPDX-License-Identifier: BSD-2-Clause
* Copyright (c) 2022 Aymeric Wibo <obiwac@gmail.com>
*/

#include <ctype.h>
#include <stddef.h>

int
strverscmp(const char *s1, const char *s2)
{
	size_t digit_count_1, digit_count_2;
	size_t zeros_count_1, zeros_count_2;
	const unsigned char *num_1, *num_2;
	const unsigned char *u1 = __DECONST(const unsigned char *, s1);
	const unsigned char *u2 = __DECONST(const unsigned char *, s2);

	/*
	 * If pointers are the same, no need to go through to process of
	 * comparing them.
	 */
	if (s1 == s2)
		return (0);

	while (*u1 != '\0' && *u2 != '\0') {
		/* If either character is not a digit, act like strcmp(3). */

		if (!isdigit(*u1) || !isdigit(*u2)) {
			if (*u1 != *u2)
				return (*u1 - *u2);
			u1++;
			u2++;
			continue;
		}
		if (*u1 == '0' || *u2 == '0') {
			/*
			 * Treat leading zeros as if they were the fractional
			 * part of a number, i.e. as if they had a decimal point
			 * in front. First, count the leading zeros (more zeros
			 * == smaller number).
			 */
			zeros_count_1 = 0;
			zeros_count_2 = 0;
			for (; *u1 == '0'; u1++)
				zeros_count_1++;
			for (; *u2 == '0'; u2++)
				zeros_count_2++;
			if (zeros_count_1 != zeros_count_2)
				return (zeros_count_2 - zeros_count_1);

			/* Handle the case where 0 < 09. */
			if (!isdigit(*u1) && isdigit(*u2))
				return (1);
			if (!isdigit(*u2) && isdigit(*u1))
				return (-1);
		} else {
			/*
			 * No leading zeros; we're simply comparing two numbers.
			 * It is necessary to first count how many digits there
			 * are before going back to compare each digit, so that
			 * e.g. 7 is not considered larger than 60.
			 */
			num_1 = u1;
			num_2 = u2;

			/* Count digits (more digits == larger number). */
			for (; isdigit(*u1); u1++)
				;
			for (; isdigit(*u2); u2++)
				;
			digit_count_1 = u1 - num_1;
			digit_count_2 = u2 - num_2;
			if (digit_count_1 != digit_count_2)
				return (digit_count_1 - digit_count_2);

			/*
			 * If there are the same number of digits, go back to
			 * the start of the number.
			 */
			u1 = num_1;
			u2 = num_2;
		}

		/* Compare each digit until there are none left. */
		for (; isdigit(*u1) && isdigit(*u2); u1++, u2++) {
			if (*u1 != *u2)
				return (*u1 - *u2);
		}
	}
	return (*u1 - *u2);
}
