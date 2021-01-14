/*	$NetBSD: str.c,v 1.78 2021/01/10 23:59:53 rillig Exp $	*/

/*-
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

/*-
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
MAKE_RCSID("$NetBSD: str.c,v 1.78 2021/01/10 23:59:53 rillig Exp $");

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

/* Return the concatenation of s1, s2, s3 and s4, freshly allocated. */
char *
str_concat4(const char *s1, const char *s2, const char *s3, const char *s4)
{
	size_t len1 = strlen(s1);
	size_t len2 = strlen(s2);
	size_t len3 = strlen(s3);
	size_t len4 = strlen(s4);
	char *result = bmake_malloc(len1 + len2 + len3 + len4 + 1);
	memcpy(result, s1, len1);
	memcpy(result + len1, s2, len2);
	memcpy(result + len1 + len2, s3, len3);
	memcpy(result + len1 + len2 + len3, s4, len4 + 1);
	return result;
}

/*
 * Fracture a string into an array of words (as delineated by tabs or spaces)
 * taking quotation marks into account.
 *
 * If expand is TRUE, quotes are removed and escape sequences such as \r, \t,
 * etc... are expanded. In this case, return NULL on parse errors.
 *
 * Returns the fractured words, which must be freed later using Words_Free,
 * unless the returned Words.words was NULL.
 */
Words
Str_Words(const char *str, Boolean expand)
{
	size_t str_len;
	char *words_buf;
	size_t words_cap;
	char **words;
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
	words = bmake_malloc((words_cap + 1) * sizeof(char *));

	/*
	 * copy the string; at the same time, parse backslashes,
	 * quotes and build the word list.
	 */
	words_len = 0;
	inquote = '\0';
	word_start = words_buf;
	word_end = words_buf;
	for (str_p = str;; ++str_p) {
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
				size_t new_size;
				words_cap *= 2;		/* ramp up fast */
				new_size = (words_cap + 1) * sizeof(char *);
				words = bmake_realloc(words, new_size);
			}
			words[words_len++] = word_start;
			word_start = NULL;
			if (ch == '\n' || ch == '\0') {
				if (expand && inquote != '\0') {
					free(words);
					free(words_buf);
					return (Words){ NULL, 0, NULL };
				}
				goto done;
			}
			continue;
		case '\\':
			if (!expand) {
				if (word_start == NULL)
					word_start = word_end;
				*word_end++ = '\\';
				/* catch '\' at end of line */
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
	words[words_len] = NULL;	/* useful for argv */
	return (Words){ words, words_len, words_buf };
}

/*
 * Str_Match -- Test if a string matches a pattern like "*.[ch]".
 * The following special characters are known *?\[] (as in fnmatch(3)).
 *
 * XXX: this function does not detect or report malformed patterns.
 */
Boolean
Str_Match(const char *str, const char *pat)
{
	for (;;) {
		/*
		 * See if we're at the end of both the pattern and the
		 * string. If so, we succeeded.  If we're at the end of the
		 * pattern but not at the end of the string, we failed.
		 */
		if (*pat == '\0')
			return *str == '\0';
		if (*str == '\0' && *pat != '*')
			return FALSE;

		/*
		 * A '*' in the pattern matches any substring.  We handle this
		 * by calling ourselves for each suffix of the string.
		 */
		if (*pat == '*') {
			pat++;
			while (*pat == '*')
				pat++;
			if (*pat == '\0')
				return TRUE;
			while (*str != '\0') {
				if (Str_Match(str, pat))
					return TRUE;
				str++;
			}
			return FALSE;
		}

		/* A '?' in the pattern matches any single character. */
		if (*pat == '?')
			goto thisCharOK;

		/*
		 * A '[' in the pattern matches a character from a list.
		 * The '[' is followed by the list of acceptable characters,
		 * or by ranges (two characters separated by '-'). In these
		 * character lists, the backslash is an ordinary character.
		 */
		if (*pat == '[') {
			Boolean neg = pat[1] == '^';
			pat += neg ? 2 : 1;

			for (;;) {
				if (*pat == ']' || *pat == '\0') {
					if (neg)
						break;
					return FALSE;
				}
				/* XXX: This naive comparison makes the parser
				 * for the pattern dependent on the actual of
				 * the string.  This is unpredictable. */
				if (*pat == *str)
					break;
				if (pat[1] == '-') {
					if (pat[2] == '\0')
						return neg;
					if (*pat <= *str && pat[2] >= *str)
						break;
					if (*pat >= *str && pat[2] <= *str)
						break;
					pat += 2;
				}
				pat++;
			}
			if (neg && *pat != ']' && *pat != '\0')
				return FALSE;
			while (*pat != ']' && *pat != '\0')
				pat++;
			if (*pat == '\0')
				pat--;
			goto thisCharOK;
		}

		/*
		 * A backslash in the pattern matches the character following
		 * it exactly.
		 */
		if (*pat == '\\') {
			pat++;
			if (*pat == '\0')
				return FALSE;
		}

		if (*pat != *str)
			return FALSE;

	thisCharOK:
		pat++;
		str++;
	}
}
