/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
#if 0
static char sccsid[] = "@(#)exec.c	8.1 (Berkeley) 6/4/93";
#endif
static const char rcsid[] =
	"$Id: exec.c,v 1.6 1997/10/14 07:23:16 bde Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <paths.h>

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

extern char **environ;

static char **
buildargv(ap, arg, envpp)
	va_list ap;
	const char *arg;
	char ***envpp;
{
	register char **argv, **nargv;
	register int memsize, off;

	argv = NULL;
	for (off = memsize = 0;; ++off) {
		if (off >= memsize) {
			memsize += 50;	/* Starts out at 0. */
			memsize *= 2;	/* Ramp up fast. */
			nargv = realloc(argv, memsize * sizeof(char *));
			if (nargv == NULL) {
				free(argv);
				return (NULL);
			}
			argv = nargv;
			if (off == 0) {
				argv[0] = (char *)arg;
				off = 1;
			}
		}
		if (!(argv[off] = va_arg(ap, char *)))
			break;
	}
	/* Get environment pointer if user supposed to provide one. */
	if (envpp)
		*envpp = va_arg(ap, char **);
	return (argv);
}

int
#if __STDC__
execl(const char *name, const char *arg, ...)
#else
execl(name, arg, va_alist)
	const char *name;
	const char *arg;
	va_dcl
#endif
{
	va_list ap;
	int sverrno;
	char **argv;

#if __STDC__
	va_start(ap, arg);
#else
	va_start(ap);
#endif
	if ( (argv = buildargv(ap, arg, NULL)) )
		(void)execve(name, argv, environ);
	va_end(ap);
	sverrno = errno;
	free(argv);
	errno = sverrno;
	return (-1);
}

int
#if __STDC__
execle(const char *name, const char *arg, ...)
#else
execle(name, arg, va_alist)
	const char *name;
	const char *arg;
	va_dcl
#endif
{
	va_list ap;
	int sverrno;
	char **argv, **envp;

#if __STDC__
	va_start(ap, arg);
#else
	va_start(ap);
#endif
	if ( (argv = buildargv(ap, arg, &envp)) )
		(void)execve(name, argv, envp);
	va_end(ap);
	sverrno = errno;
	free(argv);
	errno = sverrno;
	return (-1);
}

int
#if __STDC__
execlp(const char *name, const char *arg, ...)
#else
execlp(name, arg, va_alist)
	const char *name;
	const char *arg;
	va_dcl
#endif
{
	va_list ap;
	int sverrno;
	char **argv;

#if __STDC__
	va_start(ap, arg);
#else
	va_start(ap);
#endif
	if ( (argv = buildargv(ap, arg, NULL)) )
		(void)execvp(name, argv);
	va_end(ap);
	sverrno = errno;
	free(argv);
	errno = sverrno;
	return (-1);
}

int
execv(name, argv)
	const char *name;
	char * const *argv;
{
	(void)execve(name, argv, environ);
	return (-1);
}

int
execvp(name, argv)
	const char *name;
	char * const *argv;
{
	char **memp;
	register int cnt, lp, ln;
	register char *p;
	int eacces, save_errno;
	char *bp, *cur, *path, buf[MAXPATHLEN];
	struct stat sb;

	eacces = 0;

	/* If it's an absolute or relative path name, it's easy. */
	if (index(name, '/')) {
		bp = (char *)name;
		cur = path = NULL;
		goto retry;
	}
	bp = buf;

	/* If it's an empty path name, fail in the usual POSIX way. */
	if (*name == '\0') {
		errno = ENOENT;
		return (-1);
	}

	/* Get the path we're searching. */
	if (!(path = getenv("PATH")))
		path = _PATH_DEFPATH;
	cur = path = strdup(path);

	while ( (p = strsep(&cur, ":")) ) {
		/*
		 * It's a SHELL path -- double, leading and trailing colons
		 * mean the current directory.
		 */
		if (!*p) {
			p = ".";
			lp = 1;
		} else
			lp = strlen(p);
		ln = strlen(name);

		/*
		 * If the path is too long complain.  This is a possible
		 * security issue; given a way to make the path too long
		 * the user may execute the wrong program.
		 */
		if (lp + ln + 2 > sizeof(buf)) {
			(void)write(STDERR_FILENO, "execvp: ", 8);
			(void)write(STDERR_FILENO, p, lp);
			(void)write(STDERR_FILENO, ": path too long\n", 16);
			continue;
		}
		bcopy(p, buf, lp);
		buf[lp] = '/';
		bcopy(name, buf + lp + 1, ln);
		buf[lp + ln + 1] = '\0';

retry:		(void)execve(bp, argv, environ);
		switch(errno) {
		case E2BIG:
			goto done;
		case ELOOP:
		case ENAMETOOLONG:
		case ENOENT:
			break;
		case ENOEXEC:
			for (cnt = 0; argv[cnt]; ++cnt)
				;
			memp = malloc((cnt + 2) * sizeof(char *));
			if (memp == NULL)
				goto done;
			memp[0] = "sh";
			memp[1] = bp;
			bcopy(argv + 1, memp + 2, cnt * sizeof(char *));
			(void)execve(_PATH_BSHELL, memp, environ);
			free(memp);
			goto done;
		case ENOMEM:
			goto done;
		case ENOTDIR:
			break;
		case ETXTBSY:
			/*
			 * We used to retry here, but sh(1) doesn't.
			 */
			goto done;
		default:
			/*
			 * EACCES may be for an inaccessible directory or
			 * a non-executable file.  Call stat() to decide
			 * which.  This also handles ambiguities for EFAULT
			 * and EIO, and undocumented errors like ESTALE.
			 * We hope that the race for a stat() is unimportant.
			 */
			save_errno = errno;
			if (stat(bp, &sb) != 0)
				break;
			if (save_errno == EACCES) {
				eacces = 1;
				continue;
			}
			errno = save_errno;
			goto done;
		}
	}
	if (eacces)
		errno = EACCES;
	else
		errno = ENOENT;
done:	if (path)
		free(path);
	return (-1);
}
