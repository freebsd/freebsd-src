/****************************************************************************
 * Copyright 2019-2024,2025 Thomas E. Dickey                                *
 * Copyright 1998-2011,2012 Free Software Foundation, Inc.                  *
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
 *  Author: Thomas E. Dickey                                                *
 ****************************************************************************/

#include <curses.priv.h>

#include <ctype.h>
#include <string.h>

#ifndef USE_ROOT_ACCESS
#if HAVE_SETFSUID && HAVE_SYS_FSUID_H
#include <sys/fsuid.h>
#else
#include <sys/stat.h>
#undef HAVE_SETFSUID
#define HAVE_SETFSUID 0		/* workaround for misconfigured system */
#endif
#endif

#if HAVE_GETAUXVAL && HAVE_SYS_AUXV_H && defined(__GLIBC__) && (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 19)
#include <sys/auxv.h>
#define USE_GETAUXVAL 1
#else
#define USE_GETAUXVAL 0
#endif

#include <tic.h>

MODULE_ID("$Id: access.c,v 1.50 2025/12/27 16:50:06 tom Exp $")

#define LOWERCASE(c) ((isalpha(UChar(c)) && isupper(UChar(c))) ? tolower(UChar(c)) : (c))

#ifdef _NC_MSC
# define ACCESS(FN, MODE) access((FN), (MODE)&(R_OK|W_OK))
#else
# define ACCESS access
#endif

#if USE_DOS_PATHS
#define IsPathDelim(pp) (*(pp) == '/' || *(pp) == '\\')
#define UsesDrive(pp)   (isalpha(UChar((pp)[0])) && (pp)[1] == ':')
#define IsRelative(pp)  (*(pp) == '.' && ((*(pp+1) == '.' && IsPathDelim(pp+2)) || IsPathDelim(pp+1)))

static char *
last_delim(const char *path)
{
    char *result = NULL;
    char *check;
    if ((check = strrchr(path, '\\')) != NULL)
	result = check;
    if ((check = strrchr(path, '/')) != NULL) {
	if ((check - path) > (result - path))
	    result = check;
    }
    return result;
}

/*
 * MinGW32 uses an environment variable to point to the directory containing
 * its executables, without a registry setting to help.
 */
static const char *
msystem_base(void)
{
    const char *result = NULL;
    char *env;

    if ((env = getenv("MSYSTEM")) != NULL
	&& !strcmp(env, "MINGW32")
	&& (env = getenv("WD")) != NULL
	&& UsesDrive(env)) {
	result = env;
    }
    return result;
}

/*
 * For MinGW32, convert POSIX pathnames to DOS syntax, allowing use of stat()
 * and access().
 */
NCURSES_EXPORT(const char *)
_nc_to_dospath(const char *path, char *buffer)
{
    if (UsesDrive(path) || IsRelative(path)) {
	if ((strlen(path) < PATH_MAX) && (strpbrk(path, "/") != NULL)) {
	    char ch;
	    char *ptr = buffer;
	    while ((ch = (*ptr++ = *path++)) != '\0') {
		if (ch == '/')
		    ptr[-1] = '\\';
	    }
	    path = buffer;
	}
    } else if (last_delim(path) != NULL) {
	const char *env;
	char *ptr;
	char *last;
	size_t needed = PATH_MAX - strlen(path) - 3;

	if ((env = msystem_base()) != NULL
	    && strlen(env) < needed
	    && strcpy(buffer, env) != NULL
	    && (last = last_delim(buffer)) != NULL) {
	    char ch;

	    *last = '\0';

	    /*
	     * If that was a trailing "\", eat more until we actually
	     * trim the last leaf, which corresponds to the directory
	     * containing MSYS executables.
	     */
	    while (last != NULL && last[1] == '\0') {
		if ((last = last_delim(buffer)) != NULL) {
		    *last = '\0';
		}
	    }
	    if (last != NULL) {
		if (!strncmp(path, "/usr", 4))
		    path += 4;
		if (IsPathDelim(path)) {
		    while ((last = last_delim(buffer)) != NULL && last[1] == '\0')
			*last = '\0';
		    ptr = buffer + strlen(buffer);
		} else {
		    ptr = buffer + strlen(buffer);
		    *ptr++ = '\\';
		}
		while ((ch = (*ptr++ = *path++)) != '\0') {
		    if (ch == '/')
			ptr[-1] = '\\';
		}
		path = buffer;
	    }
	}
    }
    return path;
}
#endif

NCURSES_EXPORT(char *)
_nc_rootname(char *path)
{
    char *result = _nc_basename(path);
#if !MIXEDCASE_FILENAMES || defined(PROG_EXT)
    static char *temp;
    char *s;

    if ((temp = strdup(result)) != NULL)
	result = temp;
#if !MIXEDCASE_FILENAMES
    for (s = result; *s != '\0'; ++s) {
	*s = (char) LOWERCASE(*s);
    }
#endif
#if defined(PROG_EXT)
    if ((s = strrchr(result, '.')) != NULL) {
	if (!strcmp(s, PROG_EXT))
	    *s = '\0';
    }
#endif
#endif
    return result;
}

/*
 * Check if a string appears to be an absolute pathname.
 */
NCURSES_EXPORT(bool)
_nc_is_abs_path(const char *path)
{
#if defined(__EMX__) || defined(__DJGPP__)
#define is_pathname(s) ((((s) != NULL) && ((s)[0] == '/')) \
		  || (((s)[0] != 0) && ((s)[1] == ':')))
#else
#define is_pathname(s) ((s) != NULL && (s)[0] == '/')
#endif
    return is_pathname(path);
}

/*
 * Return index of the basename
 */
NCURSES_EXPORT(unsigned)
_nc_pathlast(const char *path)
{
    const char *test = strrchr(path, '/');
#ifdef __EMX__
    if (test == NULL)
	test = strrchr(path, '\\');
#endif
    if (test == NULL)
	test = path;
    else
	test++;
    return (unsigned) (test - path);
}

NCURSES_EXPORT(char *)
_nc_basename(char *path)
{
    return path + _nc_pathlast(path);
}

NCURSES_EXPORT(int)
_nc_access(const char *path, int mode)
{
    int result;

    FixupPathname(path);

    if (path == NULL) {
	errno = ENOENT;
	result = -1;
    } else if (ACCESS(path, mode) < 0) {
	if ((mode & W_OK) != 0
	    && errno == ENOENT
	    && strlen(path) < PATH_MAX) {
	    char head[PATH_MAX];
	    char *leaf;

	    _nc_STRCPY(head, path, sizeof(head));
	    leaf = _nc_basename(head);
	    if (leaf == NULL)
		leaf = head;
	    *leaf = '\0';
	    if (head == leaf)
		_nc_STRCPY(head, ".", sizeof(head));

	    result = ACCESS(head, R_OK | W_OK | X_OK);
	} else {
	    errno = EPERM;
	    result = -1;
	}
    } else {
	result = 0;
    }
    return result;
}

NCURSES_EXPORT(bool)
_nc_is_dir_path(const char *path)
{
    bool result = FALSE;
    struct stat sb;

    if (_nc_is_path_found(path, &sb)
	&& S_ISDIR(sb.st_mode)) {
	result = TRUE;
    }
    return result;
}

NCURSES_EXPORT(bool)
_nc_is_file_path(const char *path)
{
    bool result = FALSE;
    struct stat sb;

    if (_nc_is_path_found(path, &sb)
	&& S_ISREG(sb.st_mode)) {
	result = TRUE;
    }
    return result;
}

NCURSES_EXPORT(bool)
_nc_is_path_found(const char *path, struct stat * sb)
{
    bool result = FALSE;

    FixupPathname(path);

    if (stat(path, sb) == 0) {
	result = TRUE;
    }
    return result;
}

#if HAVE_GETEUID && HAVE_GETEGID
#define is_posix_elevated() \
	(getuid() != geteuid() \
	 || getgid() != getegid())
#else
#define is_posix_elevated() FALSE
#endif

#if HAVE_ISSETUGID
#define is_elevated() issetugid()
#elif USE_GETAUXVAL && defined(AT_SECURE)
#define is_elevated() \
	(getauxval(AT_SECURE) \
	 ? TRUE \
	 : (errno != ENOENT \
	    ? FALSE \
	    : is_posix_elevated()))
#else
#define is_elevated() is_posix_elevated()
#endif

#if HAVE_SETFSUID
#define lower_privileges() \
	    int save_err = errno; \
	    setfsuid(getuid()); \
	    setfsgid(getgid()); \
	    errno = save_err
#define resume_elevation() \
	    save_err = errno; \
	    setfsuid(geteuid()); \
	    setfsgid(getegid()); \
	    errno = save_err
#else
#define lower_privileges()	/* nothing */
#define resume_elevation()	/* nothing */
#endif

/*
 * Returns true if not running as root or setuid.  We use this check to allow
 * applications to use environment variables that are used for searching lists
 * of directories, etc.
 */
NCURSES_EXPORT(int)
_nc_env_access(void)
{
    int result = TRUE;

#if HAVE_GETUID && HAVE_GETEUID
#if !defined(USE_SETUID_ENVIRON)
    if (is_elevated()) {
	result = FALSE;
    }
#endif
#if !defined(USE_ROOT_ENVIRON)
    if ((getuid() == ROOT_UID) || (geteuid() == ROOT_UID)) {
	result = FALSE;
    }
#endif
#endif /* HAVE_GETUID && HAVE_GETEUID */
    return result;
}

#ifndef USE_ROOT_ACCESS
static int
is_a_file(int fd)
{
    int result = FALSE;
    if (fd >= 0) {
	struct stat sb;
	if (fstat(fd, &sb) == 0) {
	    switch (sb.st_mode & S_IFMT) {
	    case S_IFBLK:
	    case S_IFCHR:
	    case S_IFDIR:
		/* disallow devices and directories */
		break;
	    default:
		/* allow regular files, fifos and sockets */
		result = TRUE;
		break;
	    }
	}
    }
    return result;
}

/*
 * Limit privileges if possible; otherwise disallow access for updating files.
 */
NCURSES_EXPORT(FILE *)
_nc_safe_fopen(const char *path, const char *mode)
{
    FILE *result = NULL;

#if HAVE_SETFSUID
    lower_privileges();
    FixupPathname(path);
    result = fopen(path, mode);
    resume_elevation();
#else
    FixupPathname(path);
    if (!is_elevated() || *mode == 'r') {
	result = fopen(path, mode);
    }
#endif
    if (result != NULL) {
	if (!is_a_file(fileno(result))) {
	    fclose(result);
	    result = NULL;
	}
    }
    return result;
}

NCURSES_EXPORT(int)
_nc_safe_open3(const char *path, int flags, mode_t mode)
{
    int result = -1;
#if HAVE_SETFSUID
    lower_privileges();
    FixupPathname(path);
    result = open(path, flags, mode);
    resume_elevation();
#else
    FixupPathname(path);
    if (!is_elevated() || (flags & O_RDONLY)) {
	result = open(path, flags, mode);
    }
#endif
    if (result >= 0) {
	if (!is_a_file(result)) {
	    close(result);
	    result = -1;
	}
    }
    return result;
}
#endif /* USE_ROOT_ACCESS */
