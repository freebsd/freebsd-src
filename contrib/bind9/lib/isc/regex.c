/*
 * Copyright (C) 2013-2015  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <config.h>

#include <isc/file.h>
#include <isc/print.h>
#include <isc/regex.h>
#include <isc/string.h>

#if VALREGEX_REPORT_REASON
#define FAIL(x) do { reason = (x); goto error; } while(0)
#else
#define FAIL(x) goto error
#endif

/*
 * Validate the regular expression 'C' locale.
 */
int
isc_regex_validate(const char *c) {
	enum {
		none, parse_bracket, parse_bound,
		parse_ce, parse_ec, parse_cc
	} state = none;
	/* Well known character classes. */
	const char *cc[] = {
		":alnum:", ":digit:", ":punct:", ":alpha:", ":graph:",
		":space:", ":blank:", ":lower:", ":upper:", ":cntrl:",
		":print:", ":xdigit:"
	};
	isc_boolean_t seen_comma = ISC_FALSE;
	isc_boolean_t seen_high = ISC_FALSE;
	isc_boolean_t seen_char = ISC_FALSE;
	isc_boolean_t seen_ec = ISC_FALSE;
	isc_boolean_t seen_ce = ISC_FALSE;
	isc_boolean_t have_atom = ISC_FALSE;
	int group = 0;
	int range = 0;
	int sub = 0;
	isc_boolean_t empty_ok = ISC_FALSE;
	isc_boolean_t neg = ISC_FALSE;
	isc_boolean_t was_multiple = ISC_FALSE;
	unsigned int low = 0;
	unsigned int high = 0;
	const char *ccname = NULL;
	int range_start = 0;
#if VALREGEX_REPORT_REASON
	const char *reason = "";
#endif

	if (c == NULL || *c == 0)
		FAIL("empty string");

	while (c != NULL && *c != 0) {
		switch (state) {
		case none:
			switch (*c) {
			case '\\':	/* make literal */
				++c;
				switch (*c) {
				case '1': case '2': case '3':
				case '4': case '5': case '6':
				case '7': case '8': case '9':
					if ((*c - '0') > sub)
						FAIL("bad back reference");
					have_atom = ISC_TRUE;
					was_multiple = ISC_FALSE;
					break;
				case 0:
					FAIL("escaped end-of-string");
				default:
					goto literal;
				}
				++c;
				break;
			case '[':	/* bracket start */
				++c;
				neg = ISC_FALSE;
				was_multiple = ISC_FALSE;
				seen_char = ISC_FALSE;
				state = parse_bracket;
				break;
			case '{': 	/* bound start */
				switch (c[1]) {
				case '0': case '1': case '2': case '3':
				case '4': case '5': case '6': case '7':
				case '8': case '9':
					if (!have_atom)
						FAIL("no atom");
					if (was_multiple)
						FAIL("was multiple");
					seen_comma = ISC_FALSE;
					seen_high = ISC_FALSE;
					low = high = 0;
					state = parse_bound;
					break;
				default:
					goto literal;
				}
				++c;
				have_atom = ISC_TRUE;
				was_multiple = ISC_TRUE;
				break;
			case '}':
				goto literal;
			case '(':	/* group start */
				have_atom = ISC_FALSE;
				was_multiple = ISC_FALSE;
				empty_ok = ISC_TRUE;
				++group;
				++sub;
				++c;
				break;
			case ')':	/* group end */
				if (group && !have_atom && !empty_ok)
					FAIL("empty alternative");
				have_atom = ISC_TRUE;
				was_multiple = ISC_FALSE;
				if (group != 0)
					--group;
				++c;
				break;
			case '|':	/* alternative seperator */
				if (!have_atom)
					FAIL("no atom");
				have_atom = ISC_FALSE;
				empty_ok = ISC_FALSE;
				was_multiple = ISC_FALSE;
				++c;
				break;
			case '^':
			case '$':
				have_atom = ISC_TRUE;
				was_multiple = ISC_TRUE;
				++c;
				break;
			case '+':
			case '*':
			case '?':
				if (was_multiple)
					FAIL("was multiple");
				if (!have_atom)
					FAIL("no atom");
				have_atom = ISC_TRUE;
				was_multiple = ISC_TRUE;
				++c;
				break;
			case '.':
			default:
			literal:
				have_atom = ISC_TRUE;
				was_multiple = ISC_FALSE;
				++c;
				break;
			}
			break;
		case parse_bound:
			switch (*c) {
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				if (!seen_comma) {
					low = low * 10 + *c - '0';
					if (low > 255)
						FAIL("lower bound too big");
				} else {
					seen_high = ISC_TRUE;
					high = high * 10 + *c - '0';
					if (high > 255)
						FAIL("upper bound too big");
				}
				++c;
				break;
			case ',':
				if (seen_comma)
					FAIL("multiple commas");
				seen_comma = ISC_TRUE;
				++c;
				break;
			default:
			case '{':
				FAIL("non digit/comma");
			case '}':
				if (seen_high && low > high)
					FAIL("bad parse bound");
				seen_comma = ISC_FALSE;
				state = none;
				++c;
				break;
			}
			break;
		case parse_bracket:
			switch (*c) {
			case '^':
				if (seen_char || neg) goto inside;
				neg = ISC_TRUE;
				++c;
				break;
			case '-':
				if (range == 2) goto inside;
				if (!seen_char) goto inside;
				if (range == 1)
					FAIL("bad range");
				range = 2;
				++c;
				break;
			case '[':
				++c;
				switch (*c) {
				case '.':	/* collating element */
					if (range != 0) --range;
					++c;
					state = parse_ce;
					seen_ce = ISC_FALSE;
					break;
				case '=':	/* equivalence class */
					if (range == 2)
					    FAIL("equivalence class in range");
					++c;
					state = parse_ec;
					seen_ec = ISC_FALSE;
					break;
				case ':':	/* character class */
					if (range == 2)
					      FAIL("character class in range");
					ccname = c;
					++c;
					state = parse_cc;
					break;
				}
				seen_char = ISC_TRUE;
				break;
			case ']':
				if (!c[1] && !seen_char)
					FAIL("unfinished brace");
				if (!seen_char)
					goto inside;
				++c;
				range = 0;
				have_atom = ISC_TRUE;
				state = none;
				break;
			default:
			inside:
				seen_char = ISC_TRUE;
				if (range == 2 && (*c & 0xff) < range_start)
					FAIL("out of order range");
				if (range != 0)
					--range;
				range_start = *c & 0xff;
				++c;
				break;
			};
			break;
		case parse_ce:
			switch (*c) {
			case '.':
				++c;
				switch (*c) {
				case ']':
					if (!seen_ce)
						 FAIL("empty ce");
					++c;
					state = parse_bracket;
					break;
				default:
					if (seen_ce)
						range_start = 256;
					else
						range_start = '.';
					seen_ce = ISC_TRUE;
					break;
				}
				break;
			default:
				if (seen_ce)
					range_start = 256;
				else
					range_start = *c;
				seen_ce = ISC_TRUE;
				++c;
				break;
			}
			break;
		case parse_ec:
			switch (*c) {
			case '=':
				++c;
				switch (*c) {
				case ']':
					if (!seen_ec)
						FAIL("no ec");
					++c;
					state = parse_bracket;
					break;
				default:
					seen_ec = ISC_TRUE;
					break;
				}
				break;
			default:
				seen_ec = ISC_TRUE;
				++c;
				break;
			}
			break;
		case parse_cc:
			switch (*c) {
			case ':':
				++c;
				switch (*c) {
				case ']': {
					unsigned int i;
					isc_boolean_t found = ISC_FALSE;
					for (i = 0;
					     i < sizeof(cc)/sizeof(*cc);
					     i++)
					{
						unsigned int len;
						len = strlen(cc[i]);
						if (len !=
						    (unsigned int)(c - ccname))
							continue;
						if (strncmp(cc[i], ccname, len))
							continue;
						found = ISC_TRUE;
					}
					if (!found)
						FAIL("unknown cc");
					++c;
					state = parse_bracket;
					break;
					}
				default:
					break;
				}
				break;
			default:
				++c;
				break;
			}
			break;
		}
	}
	if (group != 0)
		FAIL("group open");
	if (state != none)
		FAIL("incomplete");
	if (!have_atom)
		FAIL("no atom");
	return (sub);

 error:
#if VALREGEX_REPORT_REASON
	fprintf(stderr, "%s\n", reason);
#endif
	return (-1);
}
