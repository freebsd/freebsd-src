/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *
 * @(#)str.c	5.8 (Berkeley) 6/1/90
 */

#ifndef lint
#include <sys/cdefs.h>
__RCSID("$FreeBSD$");
#endif /* not lint */

#include "make.h"

static char **argv, *buffer;
static int argmax, curlen;

/*
 * str_init --
 *	Initialize the strings package
 *
 */
void
str_init()
{
    char *p1;
    argv = (char **)emalloc(((argmax = 50) + 1) * sizeof(char *));
    argv[0] = Var_Value(".MAKE", VAR_GLOBAL, &p1);
}


/*
 * str_end --
 *	Cleanup the strings package
 *
 */
void
str_end()
{
    if (argv) {
	if (argv[0])
	    free(argv[0]);
	free(argv);
    }
    if (buffer)
	free(buffer);
}

/*-
 * str_concat --
 *	concatenate the two strings, inserting a space or slash between them,
 *	freeing them if requested.
 *
 * returns --
 *	the resulting string in allocated space.
 */
char *
str_concat(s1, s2, flags)
	const char *s1, *s2;
	int flags;
{
	int len1, len2;
	char *result;

	/* get the length of both strings */
	len1 = strlen(s1);
	len2 = strlen(s2);

	/* allocate length plus separator plus EOS */
	result = emalloc((u_int)(len1 + len2 + 2));

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

	/* free original strings */
	if (flags & STR_DOFREE) {
		(void)free(__DECONST(void *, s1));
		(void)free(__DECONST(void *, s2));
	}
	return(result);
}

/*-
 * brk_string --
 *	Fracture a string into an array of words (as delineated by tabs or
 *	spaces) taking quotation marks into account.  Leading tabs/spaces
 *	are ignored.
 *
 * returns --
 *	Pointer to the array of pointers to the words.  To make life easier,
 *	the first word is always the value of the .MAKE variable.
 */
char **
brk_string(str, store_argc, expand)
	char *str;
	int *store_argc;
	Boolean expand;
{
	int argc, ch;
	char inquote, *p, *start, *t;
	int len;

	/* skip leading space chars. */
	for (; *str == ' ' || *str == '\t'; ++str)
		continue;

	/* allocate room for a copy of the string */
	if ((len = strlen(str) + 1) > curlen) {
		if (buffer)
		    free(buffer);
		buffer = emalloc(curlen = len);
	}

	/*
	 * copy the string; at the same time, parse backslashes,
	 * quotes and build the argument list.
	 */
	argc = 1;
	inquote = '\0';
	for (p = str, start = t = buffer;; ++p) {
		switch(ch = *p) {
		case '"':
		case '\'':
			if (inquote) {
				if (inquote == ch)
					inquote = '\0';
				else
					break;
			} else {
				inquote = (char) ch;
				/* Don't miss "" or '' */
				if (start == NULL && p[1] == inquote) {
					start = t + 1;
					break;
				}
			}
			if (!expand) {
				if (!start)
					start = t;
				*t++ = ch;
			}
			continue;
		case ' ':
		case '\t':
		case '\n':
			if (inquote)
				break;
			if (!start)
				continue;
			/* FALLTHROUGH */
		case '\0':
			/*
			 * end of a token -- make sure there's enough argv
			 * space and save off a pointer.
			 */
			if (!start)
			    goto done;

			*t++ = '\0';
			if (argc == argmax) {
				argmax *= 2;		/* ramp up fast */
				argv = (char **)erealloc(argv,
				    (argmax + 1) * sizeof(char *));
			}
			argv[argc++] = start;
			start = (char *)NULL;
			if (ch == '\n' || ch == '\0')
				goto done;
			continue;
		case '\\':
			if (!expand) {
				if (!start)
					start = t;
				*t++ = '\\';
				ch = *++p;
				break;
			}

			switch (ch = *++p) {
			case '\0':
			case '\n':
				/* hmmm; fix it up as best we can */
				ch = '\\';
				--p;
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
		if (!start)
			start = t;
		*t++ = (char) ch;
	}
done:	argv[argc] = (char *)NULL;
	*store_argc = argc;
	return(argv);
}

/*
 * Str_FindSubstring -- See if a string contains a particular substring.
 *
 * Results: If string contains substring, the return value is the location of
 * the first matching instance of substring in string.  If string doesn't
 * contain substring, the return value is NULL.  Matching is done on an exact
 * character-for-character basis with no wildcards or special characters.
 *
 * Side effects: None.
 */
char *
Str_FindSubstring(string, substring)
	char *string;			/* String to search. */
	char *substring;		/* Substring to find in string */
{
	char *a, *b;

	/*
	 * First scan quickly through the two strings looking for a single-
	 * character match.  When it's found, then compare the rest of the
	 * substring.
	 */

	for (b = substring; *string != 0; string += 1) {
		if (*string != *b)
			continue;
		a = string;
		for (;;) {
			if (*b == 0)
				return(string);
			if (*a++ != *b++)
				break;
		}
		b = substring;
	}
	return((char *) NULL);
}

/*
 * Str_Match --
 *
 * See if a particular string matches a particular pattern.
 *
 * Results: Non-zero is returned if string matches pattern, 0 otherwise. The
 * matching operation permits the following special characters in the
 * pattern: *?\[] (see the man page for details on what these mean).
 *
 * Side effects: None.
 */
int
Str_Match(string, pattern)
	char *string;		/* String */
	char *pattern;		/* Pattern */
{
	char c2;

	for (;;) {
		/*
		 * See if we're at the end of both the pattern and the
		 * string. If, we succeeded.  If we're at the end of the
		 * pattern but not at the end of the string, we failed.
		 */
		if (*pattern == 0)
			return(!*string);
		if (*string == 0 && *pattern != '*')
			return(0);
		/*
		 * Check for a "*" as the next pattern character.  It matches
		 * any substring.  We handle this by calling ourselves
		 * recursively for each postfix of string, until either we
		 * match or we reach the end of the string.
		 */
		if (*pattern == '*') {
			pattern += 1;
			if (*pattern == 0)
				return(1);
			while (*string != 0) {
				if (Str_Match(string, pattern))
					return(1);
				++string;
			}
			return(0);
		}
		/*
		 * Check for a "?" as the next pattern character.  It matches
		 * any single character.
		 */
		if (*pattern == '?')
			goto thisCharOK;
		/*
		 * Check for a "[" as the next pattern character.  It is
		 * followed by a list of characters that are acceptable, or
		 * by a range (two characters separated by "-").
		 */
		if (*pattern == '[') {
			++pattern;
			for (;;) {
				if ((*pattern == ']') || (*pattern == 0))
					return(0);
				if (*pattern == *string)
					break;
				if (pattern[1] == '-') {
					c2 = pattern[2];
					if (c2 == 0)
						return(0);
					if ((*pattern <= *string) &&
					    (c2 >= *string))
						break;
					if ((*pattern >= *string) &&
					    (c2 <= *string))
						break;
					pattern += 2;
				}
				++pattern;
			}
			while ((*pattern != ']') && (*pattern != 0))
				++pattern;
			goto thisCharOK;
		}
		/*
		 * If the next pattern character is '/', just strip off the
		 * '/' so we do exact matching on the character that follows.
		 */
		if (*pattern == '\\') {
			++pattern;
			if (*pattern == 0)
				return(0);
		}
		/*
		 * There's no special character.  Just make sure that the
		 * next characters of each string match.
		 */
		if (*pattern != *string)
			return(0);
thisCharOK:	++pattern;
		++string;
	}
}


/*-
 *-----------------------------------------------------------------------
 * Str_SYSVMatch --
 *	Check word against pattern for a match (% is wild),
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
Str_SYSVMatch(word, pattern, len)
    char	*word;		/* Word to examine */
    char	*pattern;	/* Pattern to examine against */
    int		*len;		/* Number of characters to substitute */
{
    char *p = pattern;
    char *w = word;
    char *m;

    if (*p == '\0') {
	/* Null pattern is the whole string */
	*len = strlen(w);
	return w;
    }

    if ((m = strchr(p, '%')) != NULL) {
	/* check that the prefix matches */
	for (; p != m && *w && *w == *p; w++, p++)
	     continue;

	if (p != m)
	    return NULL;	/* No match */

	if (*++p == '\0') {
	    /* No more pattern, return the rest of the string */
	    *len = strlen(w);
	    return w;
	}
    }

    m = w;

    /* Find a matching tail */
    do
	if (strcmp(p, w) == 0) {
	    *len = w - m;
	    return m;
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
Str_SYSVSubst(buf, pat, src, len)
    Buffer buf;
    char *pat;
    char *src;
    int   len;
{
    char *m;

    if ((m = strchr(pat, '%')) != NULL) {
	/* Copy the prefix */
	Buf_AddBytes(buf, m - pat, (Byte *) pat);
	/* skip the % */
	pat = m + 1;
    }

    /* Copy the pattern */
    Buf_AddBytes(buf, len, (Byte *) src);

    /* append the rest */
    Buf_AddBytes(buf, strlen(pat), (Byte *) pat);
}
