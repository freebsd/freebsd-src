/*
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
 *
 * $FreeBSD: src/lib/libc/locale/setlocale.c,v 1.25.2.2 2000/09/08 07:31:52 kris Exp $
 */

#ifdef LIBC_RCS
static const char rcsid[] =
  "$FreeBSD: src/lib/libc/locale/setlocale.c,v 1.25.2.2 2000/09/08 07:31:52 kris Exp $";
#endif

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)setlocale.c	8.1 (Berkeley) 7/4/93";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <locale.h>
#include <rune.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "collate.h"
#include "setlocale.h"

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

static char	*currentlocale __P((void));
static char	*loadlocale __P((int));
static int      stub_load_locale __P((const char *));

extern int __time_load_locale __P((const char *)); /* strftime.c */

char *
setlocale(category, locale)
	int category;
	const char *locale;
{
	int i, j, len;
	char *env, *r;

	if (category < LC_ALL || category >= _LC_LAST)
		return (NULL);

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
		env = getenv(categories[category]);

		if (category != LC_ALL && (!env || !*env))
			env = getenv(categories[LC_ALL]);

		if (!env || !*env)
			env = getenv("LANG");

		if (!env || !*env || strchr(env, '/'))
			env = "C";

		(void) strncpy(new_categories[category], env, ENCODING_LEN);
		new_categories[category][ENCODING_LEN] = '\0';
		if (category == LC_ALL) {
			for (i = 1; i < _LC_LAST; ++i) {
				if (!(env = getenv(categories[i])) || !*env)
					env = new_categories[LC_ALL];
				(void)strncpy(new_categories[i], env, ENCODING_LEN);
				new_categories[i][ENCODING_LEN] = '\0';
			}
		}
	} else if (category != LC_ALL)  {
		(void)strncpy(new_categories[category], locale, ENCODING_LEN);
		new_categories[category][ENCODING_LEN] = '\0';
	} else {
		if ((r = strchr(locale, '/')) == NULL) {
			for (i = 1; i < _LC_LAST; ++i) {
				(void)strncpy(new_categories[i], locale, ENCODING_LEN);
				new_categories[i][ENCODING_LEN] = '\0';
			}
		} else {
			for (i = 1; r[1] == '/'; ++r);
			if (!r[1])
				return (NULL);	/* Hmm, just slashes... */
			do {
				len = r - locale > ENCODING_LEN ? ENCODING_LEN : r - locale;
				(void)strncpy(new_categories[i], locale, len);
				new_categories[i][len] = '\0';
				i++;
				locale = r;
				while (*locale == '/')
				    ++locale;
				while (*++r && *r != '/');
			} while (*locale);
			while (i < _LC_LAST) {
				(void)strcpy(new_categories[i],
				    new_categories[i-1]);
				i++;
			}
		}
	}

	if (category)
		return (loadlocale(category));

	for (i = 1; i < _LC_LAST; ++i) {
		(void)strcpy(saved_categories[i], current_categories[i]);
		if (loadlocale(i) == NULL) {
			for (j = 1; j < i; j++) {
				(void)strcpy(new_categories[j],
				     saved_categories[j]);
				/* XXX can fail too */
				(void)loadlocale(j);
			}
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
				(void) strcat(current_locale_string, "/");
				(void) strcat(current_locale_string, current_categories[i]);
			}
			break;
		}
	return (current_locale_string);
}

static char *
loadlocale(category)
	int category;
{
	char *ret;
	char *new = new_categories[category];
	char *old = current_categories[category];

	if (_PathLocale == NULL) {
		char *p = getenv("PATH_LOCALE");

		if (p != NULL
#ifndef __NETBSD_SYSCALLS
			&& !issetugid()
#endif
			) {
			if (strlen(p) + 1/*"/"*/ + ENCODING_LEN +
			    1/*"/"*/ + CATEGORY_LEN >= PATH_MAX)
				return (NULL);
			_PathLocale = strdup(p);
			if (_PathLocale == NULL)
				return (NULL);
		} else
			_PathLocale = _PATH_LOCALE;
	}

	if (strcmp(new, old) == 0)
		return (old);

	if (category == LC_CTYPE) {
		ret = setrunelocale(new) ? NULL : new;
		if (!ret)
			(void)setrunelocale(old);
		else
			(void)strcpy(old, new);
		return (ret);
	}

	if (category == LC_COLLATE) {
		ret = (__collate_load_tables(new) < 0) ? NULL : new;
		if (!ret)
			(void)__collate_load_tables(old);
		else
			(void)strcpy(old, new);
		return (ret);
	}

	if (category == LC_TIME) {
		ret = (__time_load_locale(new) < 0) ? NULL : new;
		if (!ret)
			(void)__time_load_locale(old);
		else
			(void)strcpy(old, new);
		return (ret);
	}

	if (category == LC_MONETARY ||
	    category == LC_MESSAGES ||
	    category == LC_NUMERIC) {
		ret = stub_load_locale(new) ? NULL : new;
		if (!ret)
			(void)stub_load_locale(old);
		else
			(void)strcpy(old, new);
		return (ret);
	}

	/* Just in case...*/
	return (NULL);
}

static int
stub_load_locale(encoding)
const char *encoding;
{
	char name[PATH_MAX];
	struct stat st;

	if (!encoding)
		return(1);
	/*
	 * The "C" and "POSIX" locale are always here.
	 */
	if (!strcmp(encoding, "C") || !strcmp(encoding, "POSIX"))
		return(0);
	if (!_PathLocale)
		return(1);
	/* Range checking not needed, encoding has fixed size */
	strcpy(name, _PathLocale);
	strcat(name, "/");
	strcat(name, encoding);
#if 0
	/*
	 * Some day we will actually look at this file.
	 */
#endif
	return (stat(name, &st) != 0 || !S_ISDIR(st.st_mode));
}
