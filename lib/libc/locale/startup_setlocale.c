/* It is reduced version for use in crt0.c (only 8 bit locales) */

#include <limits.h>
#include <locale.h>
#include <rune.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "common_setlocale.h"
#include "common_rune.h"

char *_PathLocale;

extern int		_none_init __P((_RuneLocale *));
static char             *loadlocale __P((int));
static int		startup_setrunelocale __P((char	*));

void
_startup_setlocale(category, locale)
	int category;
	const char *locale;
{
	int found, i, len;
	char *env, *r;

	if (!PathLocale && !(PathLocale = getenv("PATH_LOCALE")))
		PathLocale = _PATH_LOCALE;

	if (category < 0 || category >= _LC_LAST)
		return;

	if (!locale) {
		if (!category)
			(void) currentlocale();
		return;
	}

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

		if (!env)
			env = getenv(categories[0]);

		if (!env)
			env = getenv("LANG");

		if (!env)
			env = "C";

		(void) strncpy(new_categories[category], env, 31);
		new_categories[category][31] = 0;
		if (!category) {
			for (i = 1; i < _LC_LAST; ++i) {
				if (!(env = getenv(categories[i])))
					env = new_categories[0];
				(void)strncpy(new_categories[i], env, 31);
				new_categories[i][31] = 0;
			}
		}
	} else if (category)  {
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
				return;  /* Hmm, just slashes... */
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

	if (category) {
		(void) loadlocale(category);
		return;
	}

	found = 0;
	for (i = 1; i < _LC_LAST; ++i)
		if (loadlocale(i) != NULL)
			found = 1;
	if (found)
	    (void) currentlocale();
}

static char *
loadlocale(category)
	int category;
{
	if (strcmp(new_categories[category],
	    current_categories[category]) == 0)
		return (current_categories[category]);

	if (category == LC_CTYPE) {
		if (startup_setrunelocale(new_categories[LC_CTYPE]))
			return (NULL);
		(void)strcpy(current_categories[LC_CTYPE],
		    new_categories[LC_CTYPE]);
		return (current_categories[LC_CTYPE]);
	}

	if (category == LC_COLLATE) {
		if (__collate_load_tables(new_categories[LC_COLLATE]) < 0)
			return (NULL);
		(void)strcpy(current_categories[LC_COLLATE],
		    new_categories[LC_COLLATE]);
		return (current_categories[LC_COLLATE]);
	}

	if (!strcmp(new_categories[category], "C") ||
		!strcmp(new_categories[category], "POSIX")) {

		/*
		 * Some day this will need to reset the locale to the default
		 * C locale.  Since we have no way to change them as of yet,
		 * there is no need to reset them.
		 */
		(void)strcpy(current_categories[category],
		    new_categories[category]);
		return (current_categories[category]);
	}

	return NULL;
}

static int
startup_setrunelocale(encoding)
	char *encoding;
{
	FILE *fp;
	char name[PATH_MAX];
	_RuneLocale *rl;

	if (!encoding)
	    return(EFAULT);

	/*
	 * The "C" and "POSIX" locale are always here.
	 */
	if (!strcmp(encoding, "C") || !strcmp(encoding, "POSIX")) {
		_CurrentRuneLocale = &_DefaultRuneLocale;
		return(0);
	}

	(void) strcpy(name, PathLocale);
	(void) strcat(name, "/");
	(void) strcat(name, encoding);
	(void) strcat(name, "/LC_CTYPE");

	if ((fp = fopen(name, "r")) == NULL)
		return(ENOENT);

	if ((rl = _Read_RuneMagi(fp)) == 0) {
		fclose(fp);
		return(EFTYPE);
	}

	if (!rl->encoding[0])
		return(EINVAL);
	else if (!strcmp(rl->encoding, "NONE"))
		return(_none_init(rl));
	else
		return(EINVAL);
}

