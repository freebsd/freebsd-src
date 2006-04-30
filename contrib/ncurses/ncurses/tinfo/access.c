/****************************************************************************
 * Copyright (c) 1998,2000,2001 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Thomas E. Dickey <dickey@clark.net> 1998,2000,2001              *
 ****************************************************************************/

#include <curses.priv.h>
#include <tic.h>
#include <nc_alloc.h>

MODULE_ID("$Id: access.c,v 1.9 2001/06/23 22:11:49 tom Exp $")

#define LOWERCASE(c) ((isalpha(UChar(c)) && isupper(UChar(c))) ? tolower(UChar(c)) : (c))

NCURSES_EXPORT(char *)
_nc_rootname(char *path)
{
    char *result = _nc_basename(path);
#if !defined(MIXEDCASE_FILENAMES) || defined(PROG_EXT)
    static char *temp;
    char *s;

    temp = strdup(result);
    result = temp;
#if !defined(MIXEDCASE_FILENAMES)
    int n;
    for (s = result; *s != '\0'; ++s) {
	*s = LOWERCASE(*s);
    }
#endif
#if defined(PROG_EXT)
    if ((s = strrchr(result, '.')) != 0) {
	if (!strcmp(s, PROG_EXT))
	    *s = '\0';
    }
#endif
#endif
    return result;
}

NCURSES_EXPORT(char *)
_nc_basename(char *path)
{
    char *result = strrchr(path, '/');
#ifdef __EMX__
    if (result == 0)
	result = strrchr(path, '\\');
#endif
    if (result == 0)
	result = path;
    else
	result++;
    return result;
}

NCURSES_EXPORT(int)
_nc_access(const char *path, int mode)
{
    if (access(path, mode) < 0) {
	if ((mode & W_OK) != 0
	    && errno == ENOENT
	    && strlen(path) < PATH_MAX) {
	    char head[PATH_MAX];
	    char *leaf = _nc_basename(strcpy(head, path));

	    if (leaf == 0)
		leaf = head;
	    *leaf = '\0';
	    if (head == leaf)
		(void) strcpy(head, ".");

	    return access(head, R_OK | W_OK | X_OK);
	}
	return -1;
    }
    return 0;
}

#ifndef USE_ROOT_ENVIRON
/*
 * Returns true if we allow application to use environment variables that are
 * used for searching lists of directories, etc.
 */
NCURSES_EXPORT(int)
_nc_env_access(void)
{
#if HAVE_ISSETUGID
    if (issetugid())
	return FALSE;
#elif HAVE_GETEUID && HAVE_GETEGID
    if (getuid() != geteuid()
	|| getgid() != getegid())
	return FALSE;
#endif
    return getuid() != 0 && geteuid() != 0;	/* ...finally, disallow root */
}
#endif
