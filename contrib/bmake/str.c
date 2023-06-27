/*	$NetBSD: str.c,v 1.99 2023/06/23 05:03:04 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

/*
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include "make.h"

/*	"@(#)str.c	5.8 (Berkeley) 6/1/90"	*/
MAKE_RCSID("$NetBSD: str.c,v 1.99 2023/06/23 05:03:04 rillig Exp $");


static HashTable interned_strings;


/* Return the concatenation of s1 and s2, freshly allocated. */
char *
str_concat2(const char *s1, const char *s2)
{
	size_t len1 = strlen(s1);
	size_t len2 = strlen(s2);
	char *result = bmake_malloc(len1 + len2 + 1);
	memcpy(result, s1, len1);
	memcpy(result + len1, s2, len2 + 1);
	return result;
}

/* Return the concatenation of s1, s2 and s3, freshly allocated. */
char *
str_concat3(const char *s1, const char *s2, const char *s3)
{
	size_t len1 = strlen(s1);
	size_t len2 = strlen(s2);
	size_t len3 = strlen(s3);
	char *result = bmake_malloc(len1 + len2 + len3 + 1);
	memcpy(result, s1, len1);
	memcpy(result + len1, s2, len2);
	memcpy(result + len1 + len2, s3, len3 + 1);
	return result;
}

/*
 * Fracture a string into an array of words (as delineated by tabs or spaces)
 * taking quotation marks into account.
 *
 * If expand is true, quotes are removed and escape sequences such as \r, \t,
 * etc... are expanded. In this case, return NULL on parse errors.
 *
 * Returns the fractured words, which must be freed later using Words_Free,
 * unless the returned Words.words was NULL.
 */
SubstringWords
Substring_Words(const char *str, bool expand)
{
	size_t str_len;
	char *words_buf;
	size_t words_cap;
	Substring *words;
	size_t words_len;
	char inquote;
	char *word_start;
	char *word_end;
	const char *str_p;

	/* XXX: why only hspace, not whitespace? */
	cpp_skip_hspace(&str);	/* skip leading space chars. */

	/* words_buf holds the words, separated by '\0'. */
	str_len = strlen(str);
	words_buf = bmake_malloc(str_len + 1);

	words_cap = str_len / 5 > 50 ? str_len / 5 : 50;
	words = bmake_malloc((words_cap + 1) * sizeof(words[0]));

	/*
	 * copy the string; at the same time, parse backslashes,
	 * quotes and build the word list.
	 */
	words_len = 0;
	inquote = '\0';
	word_start = words_buf;
	word_end = words_buf;
	for (str_p = str;; str_p++) {
		char ch = *str_p;
		switch (ch) {
		case '"':
		case '\'':
			if (inquote != '\0') {
				if (inquote == ch)
					inquote = '\0';
				else
					break;
			} else {
				inquote = ch;
				/* Don't miss "" or '' */
				if (word_start == NULL && str_p[1] == inquote) {
					if (!expand) {
						word_start = word_end;
						*word_end++ = ch;
					} else
						word_start = word_end + 1;
					str_p++;
					inquote = '\0';
					break;
				}
			}
			if (!expand) {
				if (word_start == NULL)
					word_start = word_end;
				*word_end++ = ch;
			}
			continue;
		case ' ':
		case '\t':
		case '\n':
			if (inquote != '\0')
				break;
			if (word_start == NULL)
				continue;
			/* FALLTHROUGH */
		case '\0':
			/*
			 * end of a token -- make sure there's enough words
			 * space and save off a pointer.
			 */
			if (word_start == NULL)
				goto done;

			*word_end++ = '\0';
			if (words_len == words_cap) {
				words_cap *= 2;
				words = bmake_realloc(words,
				    (words_cap + 1) * sizeof(words[0]));
			}
			words[words_len++] =
			    Substring_Init(word_start, word_end - 1);
			word_start = NULL;
			if (ch == '\n' || ch == '\0') {
				if (expand && inquote != '\0') {
					SubstringWords res;

					free(words);
					free(words_buf);

					res.words = NULL;
					res.len = 0;
					res.freeIt = NULL;
					return res;
				}
				goto done;
			}
			continue;
		case '\\':
			if (!expand) {
				if (word_start == NULL)
					word_start = word_end;
				*word_end++ = '\\';
				/* catch lonely '\' at end of string */
				if (str_p[1] == '\0')
					continue;
				ch = *++str_p;
				break;
			}

			switch (ch = *++str_p) {
			case '\0':
			case '\n':
				/* hmmm; fix it up as best we can */
				ch = '\\';
				str_p--;
				break;
			case 'b':
				ch = '\b';
				break;
			case 'f':
				ch = '\f';
				break;
			case 'n':
				ch = '\n';
				break;
			case 'r':
				ch = '\r';
				break;
			case 't':
				ch = '\t';
				break;
			}
			break;
		}
		if (word_start == NULL)
			word_start = word_end;
		*word_end++ = ch;
	}
done:
	words[words_len] = Substring_Init(NULL, NULL);	/* useful for argv */

	{
		SubstringWords result;

		result.words = words;
		result.len = words_len;
		result.freeIt = words_buf;
		return result;
	}
}

Words
Str_Words(const char *str, bool expand)
{
	SubstringWords swords;
	Words words;
	size_t i;

	swords = Substring_Words(str, expand);
	if (swords.words == NULL) {
		words.words = NULL;
		words.len = 0;
		words.freeIt = NULL;
		return words;
	}

	words.words = bmake_malloc((swords.len + 1) * sizeof(words.words[0]));
	words.len = swords.len;
	words.freeIt = swords.freeIt;
	for (i = 0; i < swords.len + 1; i++)
		words.words[i] = UNCONST(swords.words[i].start);
	free(swords.words);
	return words;
}

/*
 * XXX: In the extreme edge case that one of the characters is from the basic
 * execution character set and the other isn't, the result of the comparison
 * differs depending on whether plain char is signed or unsigned.
 *
 * An example is the character range from \xE4 to 'a', where \xE4 may come
 * from U+00E4 'Latin small letter A with diaeresis'.
 *
 * If char is signed, \xE4 evaluates to -28, the first half of the condition
 * becomes -28 <= '0' && '0' <= 'a', which evaluates to true.
 *
 * If char is unsigned, \xE4 evaluates to 228, the second half of the
 * condition becomes 'a' <= '0' && '0' <= 228, which evaluates to false.
 */
static bool
in_range(char e1, char c, char e2)
{
	return (e1 <= c && c <= e2) || (e2 <= c && c <= e1);
}

/*
 * Test if a string matches a pattern like "*.[ch]". The pattern matching
 * characters are '*', '?' and '[]', as in fnmatch(3).
 *
 * See varmod-match.mk for examples and edge cases.
 */
StrMatchResult
Str_Match(const char *str, const char *pat)
{
	StrMatchResult res = { NULL, false };
	const char *fixed_str, *fixed_pat;
	bool asterisk, matched;

	asterisk = false;
	fixed_str = str;
	fixed_pat = pat;

match_fixed_length:
	str = fixed_str;
	pat = fixed_pat;
	matched = false;
	for (; *pat != '\0' && *pat != '*'; str++, pat++) {
		if (*str == '\0')
			return res;

		if (*pat == '?')	/* match any single character */
			continue;

		if (*pat == '[') {	/* match a character from a list */
			bool neg = pat[1] == '^';
			pat += neg ? 2 : 1;

		next_char_in_list:
			if (*pat == '\0')
				res.error = "Unfinished character list";
			if (*pat == ']' || *pat == '\0') {
				if (neg)
					goto end_of_char_list;
				goto match_done;
			}
			if (*pat == *str)
				goto end_of_char_list;
			if (pat[1] == '-' && pat[2] == '\0') {
				res.error = "Unfinished character range";
				res.matched = neg;
				return res;
			}
			if (pat[1] == '-') {
				if (in_range(pat[0], *str, pat[2]))
					goto end_of_char_list;
				pat += 2;
			}
			pat++;
			goto next_char_in_list;

		end_of_char_list:
			if (neg && *pat != ']' && *pat != '\0')
				goto match_done;
			while (*pat != ']' && *pat != '\0')
				pat++;
			if (*pat == '\0')
				pat--;
			continue;
		}

		if (*pat == '\\')	/* match the next character exactly */
			pat++;
		if (*pat != *str)
			goto match_done;
	}
	matched = true;

match_done:
	if (!asterisk) {
		if (!matched)
			return res;
		if (*pat == '\0') {
			res.matched = *str == '\0';
			return res;
		}
		asterisk = true;
	} else {
		if (!matched) {
			fixed_str++;
			goto match_fixed_length;
		}
		if (*pat == '\0') {
			if (*str == '\0') {
				res.matched = true;
				return res;
			}
			fixed_str += strlen(str);
			goto match_fixed_length;
		}
	}

	while (*pat == '*')
		pat++;
	if (*pat == '\0') {
		res.matched = true;
		return res;
	}
	fixed_str = str;
	fixed_pat = pat;
	goto match_fixed_length;
}

void
Str_Intern_Init(void)
{
	HashTable_Init(&interned_strings);
}

void
Str_Intern_End(void)
{
#ifdef CLEANUP
	HashTable_Done(&interned_strings);
#endif
}

/* Return a canonical instance of str, with unlimited lifetime. */
const char *
Str_Intern(const char *str)
{
	return HashTable_CreateEntry(&interned_strings, str, NULL)->key;
}
