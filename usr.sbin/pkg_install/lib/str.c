#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * Miscellaneous string utilities.
 *
 */

#include "lib.h"

char *
strconcat(const char *s1, const char *s2)
{
    static char tmp[FILENAME_MAX];

    tmp[0] = '\0';
    strncpy(tmp, s1 ? s1 : s2, FILENAME_MAX);
    if (s1 && s2)
	strncat(tmp, s2, FILENAME_MAX - strlen(tmp));
    return tmp;
}

/* Get a string parameter as a file spec or as a "contents follow -" spec */
char *
get_dash_string(char **str)
{
    char *s = *str;

    if (*s == '-')
	*str = copy_string(s + 1);
    else
	*str = fileGetContents(s);
    return *str;
}

/* Do a strlcpy and test for overflow */
int
s_strlcpy(char *dst, const char *src, size_t size)
{
	return (strlcpy(dst, src, size) >= size);
}

/* Do a strlcat and test for overflow */
int
s_strlcat(char *dst, const char *src, size_t size)
{
	return (strlcat(dst, src, size) >= size);
}

/* Rather Obvious */
char *
copy_string(const char *str)
{
    char *ret;

    if (!str)
	ret = NULL;
    else {
	ret = (char *)malloc(strlen(str) + 1);
	strcpy(ret, str);
    }
    return ret;
}

/* Return TRUE if 'str' ends in suffix 'suff' */
Boolean
suffix(const char *str, const char *suff)
{
    char *idx;
    Boolean ret = FALSE;

    idx = strrchr(str, '.');
    if (idx && !strcmp(idx + 1, suff))
	ret = TRUE;
    return ret;
}

/* Assuming str has a suffix, brutally murder it! */
void
nuke_suffix(char *str)
{
    char *idx;

    idx = strrchr(str, '.');
    if (idx)
	*idx = '\0';  /* Yow!  Don't try this on a const! */
}

/* Lowercase a whole string */
void
str_lowercase(char *str)
{
    while (*str) {
	*str = tolower(*str);
	++str;
    }
}

char *
get_string(char *str, int max, FILE *fp)
{
    int len;

    if (!str)
	return NULL;
    str[0] = '\0';
    while (fgets(str, max, fp)) {
	len = strlen(str);
	while (len && isspace(str[len - 1]))
	    str[--len] = '\0';
	if (len)
	   return str;
    }
    return NULL;
}
