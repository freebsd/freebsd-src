/*
 * Copyright 2016 Chris Torek <chris.torek@gmail.com>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(WITH_CASPER)
#include <libcasper.h>
#include <casper/cap_pwd.h>
#include <casper/cap_grp.h>
#endif

#include "rfuncs.h"

/*
 * This is essentially a clone of the BSD basename_r function,
 * which is like POSIX basename() but puts the result in a user
 * supplied buffer.
 *
 * In BSD basename_r, the buffer must be least MAXPATHLEN bytes
 * long.  In our case we take the size of the buffer as an argument.
 *
 * Note that it's impossible in general to do this without
 * a temporary buffer since basename("foo/bar") is "bar",
 * but basename("foo/bar/") is still "bar" -- no trailing
 * slash is allowed.
 *
 * The return value is your supplied buffer <buf>, or NULL if
 * the length of the basename of the supplied <path> equals or
 * exceeds your indicated <bufsize>.
 *
 * As a special but useful case, if you supply NULL for the <buf>
 * argument, we allocate the buffer dynamically to match the
 * basename, i.e., the result is basically strdup()ed for you.
 * In this case <bufsize> is ignored (recommended: pass 0 here).
 */
char *
r_basename(const char *path, char *buf, size_t bufsize)
{
	const char *endp, *comp;
	size_t len;

	/*
	 * NULL or empty path means ".".  This is perhaps overly
	 * forgiving but matches libc basename_r(), and avoids
	 * breaking the code below.
	 */
	if (path == NULL || *path == '\0') {
		comp = ".";
		len = 1;
	} else {
		/*
		 * Back up over any trailing slashes.  If we reach
		 * the top of the path and it's still a trailing
		 * slash, it's also a leading slash and the entire
		 * path is just "/" (or "//", or "///", etc).
		 */
		endp = path + strlen(path) - 1;
		while (*endp == '/' && endp > path)
			endp--;
		/* Invariant: *endp != '/' || endp == path */
		if (*endp == '/') {
			/* then endp==path and hence entire path is "/" */
			comp = "/";
			len = 1;
		} else {
			/*
			 * We handled empty strings earlier, and
			 * we just proved *endp != '/'.  Hence
			 * we have a non-empty basename, ending
			 * at endp.
			 *
			 * Back up one path name component.  The
			 * part between these two is the basename.
			 *
			 * Note that we only stop backing up when
			 * either comp==path, or comp[-1] is '/'.
			 *
			 * Suppose path[0] is '/'.  Then, since *endp
			 * is *not* '/', we had comp>path initially, and
			 * stopped backing up because we found a '/'
			 * (perhaps path[0], perhaps a later '/').
			 *
			 * Or, suppose path[0] is NOT '/'.  Then,
			 * either there are no '/'s at all and
			 * comp==path, or comp[-1] is '/'.
			 *
			 * In all cases, we want all bytes from *comp
			 * to *endp, inclusive.
			 */
			comp = endp;
			while (comp > path && comp[-1] != '/')
				comp--;
			len = (size_t)(endp - comp + 1);
		}
	}
	if (buf == NULL) {
		buf = malloc(len + 1);
		if (buf == NULL)
			return (NULL);
	} else {
		if (len >= bufsize) {
			errno = ENAMETOOLONG;
			return (NULL);
		}
	}
	memcpy(buf, comp, len);
	buf[len] = '\0';
	return (buf);
}

/*
 * This is much like POSIX dirname(), but is reentrant.
 *
 * We examine a path, find the directory portion, and copy that
 * to a user supplied buffer <buf> of the given size <bufsize>.
 *
 * Note that dirname("/foo/bar/") is "/foo", dirname("/foo") is "/",
 * and dirname("////") is "/". However, dirname("////foo/bar") is
 * "////foo" (we do not resolve these leading slashes away -- this
 * matches the BSD libc behavior).
 *
 * The return value is your supplied buffer <buf>, or NULL if
 * the length of the dirname of the supplied <path> equals or
 * exceeds your indicated <bufsize>.
 *
 * As a special but useful case, if you supply NULL for the <buf>
 * argument, we allocate the buffer dynamically to match the
 * dirname, i.e., the result is basically strdup()ed for you.
 * In this case <bufsize> is ignored (recommended: pass 0 here).
 */
char *
r_dirname(const char *path, char *buf, size_t bufsize)
{
	const char *endp, *dirpart;
	size_t len;

	/*
	 * NULL or empty path means ".".  This is perhaps overly
	 * forgiving but matches libc dirname(), and avoids breaking
	 * the code below.
	 */
	if (path == NULL || *path == '\0') {
		dirpart = ".";
		len = 1;
	} else {
		/*
		 * Back up over any trailing slashes, then back up
		 * one path name, then back up over more slashes.
		 * In all cases, stop as soon as endp==path so
		 * that we do not back out of the buffer entirely.
		 *
		 * The first loop takes care of trailing slashes
		 * in names like "/foo/bar//" (where the dirname
		 * part is to be "/foo"), the second strips out
		 * the non-dir-name part, and the third leaves us
		 * pointing to the end of the directory component.
		 *
		 * If the entire name is of the form "/foo" or
		 * "//foo" (or "/foo/", etc, but we already
		 * handled trailing slashes), we end up pointing
		 * to the leading "/", which is what we want; but
		 * if it is of the form "foo" (or "foo/", etc) we
		 * point to a non-slash.  So, if (and only if)
		 * endp==path AND *endp is not '/', the dirname is
		 * ".", but in all cases, the LENGTH of the
		 * dirname is (endp-path+1).
		 */
		endp = path + strlen(path) - 1;
		while (endp > path && *endp == '/')
			endp--;
		while (endp > path && *endp != '/')
			endp--;
		while (endp > path && *endp == '/')
			endp--;

		len = (size_t)(endp - path + 1);
		if (endp == path && *endp != '/')
			dirpart = ".";
		else
			dirpart = path;
	}
	if (buf == NULL) {
		buf = malloc(len + 1);
		if (buf == NULL)
			return (NULL);
	} else {
		if (len >= bufsize) {
			errno = ENAMETOOLONG;
			return (NULL);
		}
	}
	memcpy(buf, dirpart, len);
	buf[len] = '\0';
	return (buf);
}

static void
r_pginit(struct r_pgdata *pg)
{

	/* Note: init to half size since the first thing we do is double it */
	pg->r_pgbufsize = 1 << 9;
	pg->r_pgbuf = NULL;	/* note that realloc(NULL) == malloc */
}

static int
r_pgexpand(struct r_pgdata *pg)
{
	size_t nsize;

	nsize = pg->r_pgbufsize << 1;
	if (nsize >= (1 << 20) ||
	    (pg->r_pgbuf = realloc(pg->r_pgbuf, nsize)) == NULL)
		return (ENOMEM);
	return (0);
}

void
r_pgfree(struct r_pgdata *pg)
{

	free(pg->r_pgbuf);
}

struct passwd *
r_getpwuid(uid_t uid, struct r_pgdata *pg)
{
	struct passwd *result = NULL;
	int error;

	r_pginit(pg);
	do {
		error = r_pgexpand(pg);
		if (error == 0)
			error = getpwuid_r(uid, &pg->r_pgun.un_pw,
			    pg->r_pgbuf, pg->r_pgbufsize, &result);
	} while (error == ERANGE);

	return (error ? NULL : result);
}

struct group *
r_getgrgid(gid_t gid, struct r_pgdata *pg)
{
	struct group *result = NULL;
	int error;

	r_pginit(pg);
	do {
		error = r_pgexpand(pg);
		if (error == 0)
			error = getgrgid_r(gid, &pg->r_pgun.un_gr,
			    pg->r_pgbuf, pg->r_pgbufsize, &result);
	} while (error == ERANGE);

	return (error ? NULL : result);
}

#if defined(WITH_CASPER)
struct passwd *
r_cap_getpwuid(cap_channel_t *cap, uid_t uid, struct r_pgdata *pg)
{
	struct passwd *result = NULL;
	int error;

	r_pginit(pg);
	do {
		error = r_pgexpand(pg);
		if (error == 0)
			error = cap_getpwuid_r(cap, uid, &pg->r_pgun.un_pw,
			    pg->r_pgbuf, pg->r_pgbufsize, &result);
	} while (error == ERANGE);

	return (error ? NULL : result);
}

struct group *
r_cap_getgrgid(cap_channel_t *cap, gid_t gid, struct r_pgdata *pg)
{
	struct group *result = NULL;
	int error;

	r_pginit(pg);
	do {
		error = r_pgexpand(pg);
		if (error == 0)
			error = cap_getgrgid_r(cap, gid, &pg->r_pgun.un_gr,
			    pg->r_pgbuf, pg->r_pgbufsize, &result);
	} while (error == ERANGE);

	return (error ? NULL : result);
}
#endif
