/*	$NetBSD: str.c,v 1.51 2020/07/03 07:40:13 rillig Exp $	*/

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

#ifndef MAKE_NATIVE
static char rcsid[] = "$NetBSD: str.c,v 1.51 2020/07/03 07:40:13 rillig Exp $";
#else
#include <sys/cdefs.h>
#ifndef lint
#if 0
static char     sccsid[] = "@(#)str.c	5.8 (Berkeley) 6/1/90";
#else
__RCSID("$NetBSD: str.c,v 1.51 2020/07/03 07:40:13 rillig Exp $");
#endif
#endif				/* not lint */
#endif

#include "make.h"

/*-
 * str_concat --
 *	concatenate the two strings, inserting a space or slash between them,
 *	freeing them if requested.
 *
 * returns --
 *	the resulting string in allocated space.
 */
char *
str_concat(const char *s1, const char *s2, int flags)
{
	int len1, len2;
	char *result;

	/* get the length of both strings */
	len1 = strlen(s1);
	len2 = strlen(s2);

	/* allocate length plus separator plus EOS */
	result = bmake_malloc((unsigned int)(len1 + len2 + 2));

	/* copy first string into place */
	memcpy(result, s1, len1);

	/* add separator character */
	if (flags & STR_ADDSPACE) {
		result[len1] = ' ';
		++len1;
	} else if (flags & STR_ADDSLASH) {
		result[len1] = '/';
		++len1;
	}

	/* copy second string plus EOS into place */
	memcpy(result + len1, s2, len2 + 1);

	return result;
}

/*-
 * brk_string --
 *	Fracture a string into an array of words (as delineated by tabs or
 *	spaces) taking quotation marks into account.  Leading tabs/spaces
 *	are ignored.
 *
 * If expand is TRUE, quotes are removed and escape sequences
 *  such as \r, \t, etc... are expanded.
 *
 * returns --
 *	Pointer to the array of pointers to the words.
 *      Memory containing the actual words in *store_words_buf.
 *		Both of these must be free'd by the caller.
 *      Number of words in *store_words_len.
 */
char **
brk_string(const char *str, int *store_words_len, Boolean expand,
	char **store_words_buf)
{
	char inquote;
	const char *str_p;
	size_t str_len;
    	char **words;
	int words_len;
	int words_cap = 50;
	char *words_buf, *word_start, *word_end;

	/* skip leading space chars. */
	for (; *str == ' ' || *str == '\t'; ++str)
		continue;

	/* words_buf holds the words, separated by '\0'. */
	str_len = strlen(str);
	words_buf = bmake_malloc(strlen(str) + 1);

	words_cap = MAX((str_len / 5), 50);
	words = bmake_malloc((words_cap + 1) * sizeof(char *));

	/*
	 * copy the string; at the same time, parse backslashes,
	 * quotes and build the word list.
	 */
	words_len = 0;
	inquote = '\0';
	word_start = word_end = words_buf;
	for (str_p = str;; ++str_p) {
		char ch = *str_p;
		switch(ch) {
		case '"':
		case '\'':
			if (inquote) {
				if (inquote == ch)
					inquote = '\0';
				else
					break;
			}
			else {
				inquote = (char) ch;
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
			if (inquote)
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
				words_cap *= 2;		/* ramp up fast */
				words = (char **)bmake_realloc(words,
				    (words_cap + 1) * sizeof(char *));
			}
			words[words_len++] = word_start;
			word_start = NULL;
			if (ch == '\n' || ch == '\0') {
				if (expand && inquote) {
					free(words);
					free(words_buf);
					*store_words_buf = NULL;
					return NULL;
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
				--str_p;
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
done:	words[words_len] = NULL;
	*store_words_len = words_len;
	*store_words_buf = words_buf;
	return words;
}

/*
 * Str_FindSubstring -- See if a string contains a particular substring.
 *
 * Input:
 *	string		String to search.
 *	substring	Substring to find in string.
 *
 * Results: If string contains substring, the return value is the location of
 * the first matching instance of substring in string.  If string doesn't
 * contain substring, the return value is NULL.  Matching is done on an exact
 * character-for-character basis with no wildcards or special characters.
 *
 * Side effects: None.
 */
char *
Str_FindSubstring(const char *string, const char *substring)
{
	const char *a, *b;

	/*
	 * First scan quickly through the two strings looking for a single-
	 * character match.  When it's found, then compare the rest of the
	 * substring.
	 */

	for (b = substring; *string != 0; string++) {
		if (*string != *b)
			continue;
		a = string;
		for (;;) {
			if (*b == 0)
				return UNCONST(string);
			if (*a++ != *b++)
				break;
		}
		b = substring;
	}
	return NULL;
}

/*
 * Str_Match -- Test if a string matches a pattern like "*.[ch]".
 *
 * XXX this function does not detect or report malformed patterns.
 *
 * Results:
 *	Non-zero is returned if string matches the pattern, 0 otherwise. The
 *	matching operation permits the following special characters in the
 *	pattern: *?\[] (as in fnmatch(3)).
 *
 * Side effects: None.
 */
Boolean
Str_Match(const char *str, const char *pat)
{
	for (;;) {
		/*
		 * See if we're at the end of both the pattern and the
		 * string. If, we succeeded.  If we're at the end of the
		 * pattern but not at the end of the string, we failed.
		 */
		if (*pat == 0)
			return *str == 0;
		if (*str == 0 && *pat != '*')
			return FALSE;

		/*
		 * A '*' in the pattern matches any substring.  We handle this
		 * by calling ourselves for each suffix of the string.
		 */
		if (*pat == '*') {
			pat++;
			while (*pat == '*')
				pat++;
			if (*pat == 0)
				return TRUE;
			while (*str != 0) {
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
			pat += 1 + neg;

			for (;;) {
				if (*pat == ']' || *pat == 0) {
					if (neg)
						break;
					return FALSE;
				}
				if (*pat == *str)
					break;
				if (pat[1] == '-') {
					if (pat[2] == 0)
						return neg;
					if (*pat <= *str && pat[2] >= *str)
						break;
					if (*pat >= *str && pat[2] <= *str)
						break;
					pat += 2;
				}
				pat++;
			}
			if (neg && *pat != ']' && *pat != 0)
				return FALSE;
			while (*pat != ']' && *pat != 0)
				pat++;
			if (*pat == 0)
				pat--;
			goto thisCharOK;
		}

		/*
		 * A backslash in the pattern matches the character following
		 * it exactly.
		 */
		if (*pat == '\\') {
			pat++;
			if (*pat == 0)
				return FALSE;
		}

		if (*pat != *str)
			return FALSE;

	thisCharOK:
		pat++;
		str++;
	}
}

/*-
 *-----------------------------------------------------------------------
 * Str_SYSVMatch --
 *	Check word against pattern for a match (% is wild),
 *
 * Input:
 *	word		Word to examine
 *	pattern		Pattern to examine against
 *	len		Number of characters to substitute
 *
 * Results:
 *	Returns the beginning position of a match or null. The number
 *	of characters matched is returned in len.
 *
 * Side Effects:
 *	None
 *
 *-----------------------------------------------------------------------
 */
char *
Str_SYSVMatch(const char *word, const char *pattern, size_t *len,
    Boolean *hasPercent)
{
    const char *p = pattern;
    const char *w = word;
    const char *m;

    *hasPercent = FALSE;
    if (*p == '\0') {
	/* Null pattern is the whole string */
	*len = strlen(w);
	return UNCONST(w);
    }

    if ((m = strchr(p, '%')) != NULL) {
	*hasPercent = TRUE;
	if (*w == '\0') {
		/* empty word does not match pattern */
		return NULL;
	}
	/* check that the prefix matches */
	for (; p != m && *w && *w == *p; w++, p++)
	     continue;

	if (p != m)
	    return NULL;	/* No match */

	if (*++p == '\0') {
	    /* No more pattern, return the rest of the string */
	    *len = strlen(w);
	    return UNCONST(w);
	}
    }

    m = w;

    /* Find a matching tail */
    do
	if (strcmp(p, w) == 0) {
	    *len = w - m;
	    return UNCONST(m);
	}
    while (*w++ != '\0');

    return NULL;
}


/*-
 *-----------------------------------------------------------------------
 * Str_SYSVSubst --
 *	Substitute '%' on the pattern with len characters from src.
 *	If the pattern does not contain a '%' prepend len characters
 *	from src.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Places result on buf
 *
 *-----------------------------------------------------------------------
 */
void
Str_SYSVSubst(Buffer *buf, char *pat, char *src, size_t len,
    Boolean lhsHasPercent)
{
    char *m;

    if ((m = strchr(pat, '%')) != NULL && lhsHasPercent) {
	/* Copy the prefix */
	Buf_AddBytes(buf, m - pat, pat);
	/* skip the % */
	pat = m + 1;
    }
    if (m != NULL || !lhsHasPercent) {
	/* Copy the pattern */
	Buf_AddBytes(buf, len, src);
    }

    /* append the rest */
    Buf_AddBytes(buf, strlen(pat), pat);
}
