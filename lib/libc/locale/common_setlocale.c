#include <locale.h>
#include <string.h>

/*
 * Category names for getenv()
 */
char *_categories[_LC_LAST] = {
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
char _current_categories[_LC_LAST][32] = {
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
char _new_categories[_LC_LAST][32];

char _current_locale_string[_LC_LAST * 33];

char *
_currentlocale()
{
	int i, len;

	(void)strcpy(_current_locale_string, _current_categories[1]);

	for (i = 2; i < _LC_LAST; ++i)
		if (strcmp(_current_categories[1], _current_categories[i])) {
			len = strlen(_current_categories[1]) + 1 +
			      strlen(_current_categories[2]) + 1 +
			      strlen(_current_categories[3]) + 1 +
			      strlen(_current_categories[4]) + 1 +
			      strlen(_current_categories[5]) + 1;
			if (len > sizeof(_current_locale_string))
				return NULL;
			(void) strcpy(_current_locale_string, _current_categories[1]);
			(void) strcat(_current_locale_string, "/");
			(void) strcat(_current_locale_string, _current_categories[2]);
			(void) strcat(_current_locale_string, "/");
			(void) strcat(_current_locale_string, _current_categories[3]);
			(void) strcat(_current_locale_string, "/");
			(void) strcat(_current_locale_string, _current_categories[4]);
			(void) strcat(_current_locale_string, "/");
			(void) strcat(_current_locale_string, _current_categories[5]);
			break;
		}
	return (_current_locale_string);
}

