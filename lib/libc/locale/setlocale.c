/*
 * Copyright (c) 1996 - 2002 FreeBSD Project
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)setlocale.c	8.1 (Berkeley) 7/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <rune.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "collate.h"
#include "lmonetary.h"	/* for __monetary_load_locale() */
#include "lnumeric.h"	/* for __numeric_load_locale() */
#include "lmessages.h"	/* for __messages_load_locale() */
#include "setlocale.h"
#include "../stdtime/timelocal.h" /* for __time_load_locale() */

/*
 * Category names for getenv()
 */
static char *categories[_LC_LAST] = {
    "LC_ALL",
    "LC_COLLATE",
    "LC_CTYPE",
    "LC_MONETARY",
    "LC_NUMERIC",
    "LC_TIME",
    "LC_MESSAGES",
};

/*
 * Current locales for each category
 */
static char current_categories[_LC_LAST][ENCODING_LEN + 1] = {
    "C",
    "C",
    "C",
    "C",
    "C",
    "C",
    "C",
};

/*
 * The locales we are going to try and load
 */
static char new_categories[_LC_LAST][ENCODING_LEN + 1];
static char saved_categories[_LC_LAST][ENCODING_LEN + 1];

static char current_locale_string[_LC_LAST * (ENCODING_LEN + 1/*"/"*/ + 1)];

static char	*currentlocale(void);
static int      wrap_setrunelocale(char *);
static char	*loadlocale(int);

char *
setlocale(category, locale)
	int category;
	const char *locale;
{
	int i, j, len;
	char *env, *r;

	if (category < LC_ALL || category >= _LC_LAST) {
		errno = EINVAL;
		return (NULL);
	}

	if (!locale)
		return (category != LC_ALL ?
		    current_categories[category] : currentlocale());

	/*
	 * Default to the current locale for everything.
	 */
	for (i = 1; i < _LC_LAST; ++i)
		(void)strcpy(new_categories[i], current_categories[i]);

	/*
	 * Now go fill up new_categories from the locale argument
	 */
	if (!*locale) {
		env = getenv("LC_ALL");

		if (category != LC_ALL && (!env || !*env))
			env = getenv(categories[category]);

		if (!env || !*env)
			env = getenv("LANG");

		if (!env || !*env)
			env = "C";

		(void)strlcpy(new_categories[category], env, ENCODING_LEN + 1);
		if (category == LC_ALL) {
			for (i = 1; i < _LC_LAST; ++i) {
				if (!(env = getenv(categories[i])) || !*env)
					env = new_categories[LC_ALL];
				(void)strlcpy(new_categories[i], env,
					      ENCODING_LEN + 1);
			}
		}
	} else if (category != LC_ALL)
		(void)strlcpy(new_categories[category], locale,
			      ENCODING_LEN + 1);
	else {
		if ((r = strchr(locale, '/')) == NULL) {
			for (i = 1; i < _LC_LAST; ++i)
				(void)strlcpy(new_categories[i], locale,
					      ENCODING_LEN + 1);
		} else {
			for (i = 1; r[1] == '/'; ++r)
				;
			if (!r[1]) {
				errno = EINVAL;
				return (NULL);	/* Hmm, just slashes... */
			}
			do {
				if (i == _LC_LAST)
					break;  /* Too many slashes... */
				len = r - locale > ENCODING_LEN ?
				      ENCODING_LEN : r - locale;
				(void)strlcpy(new_categories[i], locale,
					      len + 1);
				i++;
				locale = r;
				while (*locale == '/')
					++locale;
				while (*++r && *r != '/')
					;
			} while (*locale);
			while (i < _LC_LAST) {
				(void)strcpy(new_categories[i],
					     new_categories[i-1]);
				i++;
			}
		}
	}

	if (category != LC_ALL)
		return (loadlocale(category));

	for (i = 1; i < _LC_LAST; ++i) {
		(void)strcpy(saved_categories[i], current_categories[i]);
		if (loadlocale(i) == NULL) {
			int saverr = errno;

			for (j = 1; j < i; j++) {
				(void)strcpy(new_categories[j],
					     saved_categories[j]);
				(void)loadlocale(j);
			}
			errno = saverr;
			return (NULL);
		}
	}
	return (currentlocale());
}

static char *
currentlocale()
{
	int i;

	(void)strcpy(current_locale_string, current_categories[1]);

	for (i = 2; i < _LC_LAST; ++i)
		if (strcmp(current_categories[1], current_categories[i])) {
			for (i = 2; i < _LC_LAST; ++i) {
				(void)strcat(current_locale_string, "/");
				(void)strcat(current_locale_string,
					     current_categories[i]);
			}
			break;
		}
	return (current_locale_string);
}


static int
wrap_setrunelocale(locale)
	char *locale;
{
	int ret = setrunelocale(locale);

	if (ret != 0) {
		errno = ret;
		return (-1);
	}
	return (0);
}

static char *
loadlocale(category)
	int category;
{
	char *ret;
	char *new = new_categories[category];
	char *old = current_categories[category];
	int saverr;

	if ((new[0] == '.' &&
	     (new[1] == '\0' || (new[1] == '.' && new[2] == '\0'))) ||
	    strchr(new, '/') != NULL) {
		errno = EINVAL;
		return (NULL);
	}

	if (_PathLocale == NULL) {
		char *p = getenv("PATH_LOCALE");

		if (p != NULL
#ifndef __NETBSD_SYSCALLS
			&& !issetugid()
#endif
			) {
			if (strlen(p) + 1/*"/"*/ + ENCODING_LEN +
			    1/*"/"*/ + CATEGORY_LEN >= PATH_MAX) {
				errno = ENAMETOOLONG;
				return (NULL);
			}
			_PathLocale = strdup(p);
			if (_PathLocale == NULL)
				return (NULL);
		} else
			_PathLocale = _PATH_LOCALE;
	}

#define LOAD_CATEGORY(FUNC)                                   \
	{                                                     \
		if (strcmp(new, old) == 0)                    \
			return (old);                         \
		ret = FUNC(new) != 0 ? NULL : new;            \
		if (ret == NULL) {                            \
			saverr = errno;                       \
			if (FUNC(old) != 0 && FUNC("C") == 0) \
				(void)strcpy(old, "C");       \
			errno = saverr;                       \
		} else                                        \
			(void)strcpy(old, new);               \
		return (ret);                                 \
	}

	switch (category) {
	case LC_CTYPE:
		LOAD_CATEGORY(wrap_setrunelocale);
		/* NOTREACHED */
	case LC_COLLATE:
		LOAD_CATEGORY(__collate_load_tables);
		/* NOTREACHED */
	case LC_TIME:
		LOAD_CATEGORY(__time_load_locale);
		/* NOTREACHED */
	case LC_NUMERIC:
		LOAD_CATEGORY(__numeric_load_locale);
		/* NOTREACHED */
	case LC_MONETARY:
		LOAD_CATEGORY(__monetary_load_locale);
		/* NOTREACHED */
	case LC_MESSAGES:
		LOAD_CATEGORY(__messages_load_locale);
		/* NOTREACHED */
	default:
		errno = EINVAL;
		return (NULL);
	}
}

