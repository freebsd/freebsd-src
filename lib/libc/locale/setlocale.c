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
 * $FreeBSD$
 */

#ifdef LIBC_RCS
static const char rcsid[] =
	"$FreeBSD$";
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
};

/*
 * Current locales for each category
 */
static char current_categories[_LC_LAST][32] = {
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
static char new_categories[_LC_LAST][32];
static char saved_categories[_LC_LAST][32];

static char current_locale_string[_LC_LAST * 33];
char *_PathLocale;

static char	*currentlocale __P((void));
static char	*loadlocale __P((int));
static int      stub_load_locale __P((const char *));

extern int __time_load_locale __P((const char *)); /* strftime.c */

#ifdef XPG4
extern int _xpg4_setrunelocale __P((char *));
#endif

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

		if (!env || !*env)
			env = "C";

		(void) strncpy(new_categories[category], env, 31);
		new_categories[category][31] = 0;
		if (category == LC_ALL) {
			for (i = 1; i < _LC_LAST; ++i) {
				if (!(env = getenv(categories[i])) || !*env)
					env = new_categories[LC_ALL];
				(void)strncpy(new_categories[i], env, 31);
				new_categories[i][31] = 0;
			}
		}
	} else if (category != LC_ALL)  {
		(void)strncpy(new_categories[category], locale, 31);
		new_categories[category][31] = 0;
	} else {
		if ((r = strchr(locale, '/')) == 0) {
			for (i = 1; i < _LC_LAST; ++i) {
				(void)strncpy(new_categories[i], locale, 31);
				new_categories[i][31] = 0;
			}
		} else {
			for (i = 1; r[1] == '/'; ++r);
			if (!r[1])
				return (NULL);	/* Hmm, just slashes... */
			do {
				len = r - locale > 31 ? 31 : r - locale;
				(void)strncpy(new_categories[i++], locale, len);
				new_categories[i++][len] = 0;
				locale = r;
				while (*locale == '/')
				    ++locale;
				while (*++r && *r != '/');
			} while (*locale);
			while (i < _LC_LAST)
				(void)strcpy(new_categories[i],
				    new_categories[i-1]);
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

/* To be compatible with crt0 hack */
void
_startup_setlocale(category, locale)
	int category;
	const char *locale;
{
#ifndef XPG4
	(void) setlocale(category, locale);
#endif
}

static char *
currentlocale()
{
	int i;

	(void)strcpy(current_locale_string, current_categories[1]);

	for (i = 2; i < _LC_LAST; ++i)
		if (strcmp(current_categories[1], current_categories[i])) {
			(void) strcpy(current_locale_string, current_categories[1]);
			(void) strcat(current_locale_string, "/");
			(void) strcat(current_locale_string, current_categories[2]);
			(void) strcat(current_locale_string, "/");
			(void) strcat(current_locale_string, current_categories[3]);
			(void) strcat(current_locale_string, "/");
			(void) strcat(current_locale_string, current_categories[4]);
			(void) strcat(current_locale_string, "/");
			(void) strcat(current_locale_string, current_categories[5]);
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

	if (!_PathLocale) {
		if (   !(ret = getenv("PATH_LOCALE"))
		    || getuid() != geteuid()
		    || getgid() != getegid()
		   )
			_PathLocale = _PATH_LOCALE;
		else if (   strlen(ret) + 45 > PATH_MAX
			 || !(_PathLocale = strdup(ret))
			)
			return (NULL);
	}

	if (strcmp(new, old) == 0)
		return (old);

	if (category == LC_CTYPE) {
#ifdef XPG4
		ret = _xpg4_setrunelocale(new) ? NULL : new;
#else
		ret = setrunelocale(new) ? NULL : new;
#endif
		if (!ret) {
#ifdef XPG4
			(void)_xpg4_setrunelocale(old);
#else
			(void)setrunelocale(old);
#endif
		} else
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

	if (category == LC_MONETARY || category == LC_NUMERIC) {
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
