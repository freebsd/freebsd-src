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
static char sccsid[] = "@(#)exec.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "namespace.h"
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <paths.h>

#include <stdarg.h>
#include "un-namespace.h"

extern char **environ;

int
execl(const char *name, const char *arg, ...)
{
	va_list ap;
	char **argv;
	int n;

	va_start(ap, arg);
	n = 1;
	while (va_arg(ap, char *) != NULL)
		n++;
	va_end(ap);
	argv = alloca((n + 1) * sizeof(*argv));
	if (argv == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	va_start(ap, arg);
	n = 1;
	argv[0] = (char *)arg;
	while ((argv[n] = va_arg(ap, char *)) != NULL)
		n++;
	va_end(ap);
	return (_execve(name, argv, environ));
}

int
execle(const char *name, const char *arg, ...)
{
	va_list ap;
	char **argv, **envp;
	int n;

	va_start(ap, arg);
	n = 1;
	while (va_arg(ap, char *) != NULL)
		n++;
	va_end(ap);
	argv = alloca((n + 1) * sizeof(*argv));
	if (argv == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	va_start(ap, arg);
	n = 1;
	argv[0] = (char *)arg;
	while ((argv[n] = va_arg(ap, char *)) != NULL)
		n++;
	envp = va_arg(ap, char **);
	va_end(ap);
	return (_execve(name, argv, envp));
}

int
execlp(const char *name, const char *arg, ...)
{
	va_list ap;
	char **argv;
	int n;

	va_start(ap, arg);
	n = 1;
	while (va_arg(ap, char *) != NULL)
		n++;
	va_end(ap);
	argv = alloca((n + 1) * sizeof(*argv));
	if (argv == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	va_start(ap, arg);
	n = 1;
	argv[0] = (char *)arg;
	while ((argv[n] = va_arg(ap, char *)) != NULL)
		n++;
	va_end(ap);
	return (execvp(name, argv));
}

int
execv(name, argv)
	const char *name;
	char * const *argv;
{
	(void)_execve(name, argv, environ);
	return (-1);
}

int
execvp(name, argv)
	const char *name;
	char * const *argv;
{
	char **memp;
	int cnt, lp, ln;
	char *p;
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
	cur = alloca(strlen(path) + 1);
	if (cur == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	strcpy(cur, path);
	path = cur;
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
			(void)_write(STDERR_FILENO, "execvp: ", 8);
			(void)_write(STDERR_FILENO, p, lp);
			(void)_write(STDERR_FILENO, ": path too long\n",
			    16);
			continue;
		}
		bcopy(p, buf, lp);
		buf[lp] = '/';
		bcopy(name, buf + lp + 1, ln);
		buf[lp + ln + 1] = '\0';

retry:		(void)_execve(bp, argv, environ);
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
			memp = alloca((cnt + 2) * sizeof(char *));
			if (memp == NULL) {
				/* errno = ENOMEM; XXX override ENOEXEC? */
				goto done;
			}
			memp[0] = "sh";
			memp[1] = bp;
			bcopy(argv + 1, memp + 2, cnt * sizeof(char *));
			(void)_execve(_PATH_BSHELL, memp, environ);
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
done:
	return (-1);
}
