/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include "namespace.h"
#include <sys/param.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <paths.h>

#include <stdarg.h>
#include "un-namespace.h"
#include "libc_private.h"

static const char execvPe_err_preamble[] = "execvP: ";
static const char execvPe_err_trailer[] = ": path too long\n";

int
execl(const char *name, const char *arg, ...)
{
	va_list ap;
	const char **argv;
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
	argv[0] = arg;
	while ((argv[n] = va_arg(ap, char *)) != NULL)
		n++;
	va_end(ap);
	return (_execve(name, __DECONST(char **, argv), environ));
}

int
execle(const char *name, const char *arg, ...)
{
	va_list ap;
	const char **argv;
	char **envp;
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
	argv[0] = arg;
	while ((argv[n] = va_arg(ap, char *)) != NULL)
		n++;
	envp = va_arg(ap, char **);
	va_end(ap);
	return (_execve(name, __DECONST(char **, argv), envp));
}

int
execlp(const char *name, const char *arg, ...)
{
	va_list ap;
	const char **argv;
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
	argv[0] = arg;
	while ((argv[n] = va_arg(ap, char *)) != NULL)
		n++;
	va_end(ap);
	return (execvp(name, __DECONST(char **, argv)));
}

int
execv(const char *name, char * const *argv)
{
	(void)_execve(name, argv, environ);
	return (-1);
}

int
execvp(const char *name, char * const *argv)
{
	return (__libc_execvpe(name, argv, environ));
}

/*
 * Returns 0 if we don't consider this a terminal condition, -1 if we do.
 */
static int
execvPe_prog(const char *path, char * const *argv, char * const *envp)
{
	struct stat sb;
	const char **memp;
	size_t cnt;
	int save_errno;

	(void)_execve(path, argv, envp);
	/* Grouped roughly by never terminal vs. usually terminal conditions */
	switch (errno) {
	case ELOOP:
	case ENAMETOOLONG:
	case ENOENT:
	case ENOTDIR:
		/* Non-terminal: property of the path we're trying */
		break;
	case ENOEXEC:
		 /*
		  * Failures here are considered terminal because we must handle
		  * this via the ENOEXEC fallback path; doing any further
		  * searching would be categorically incorrect.
		  */

		for (cnt = 0; argv[cnt] != NULL; ++cnt)
			;

		/*
		 * cnt may be 0 above; always allocate at least
		 * 3 entries so that we can at least fit "sh", path, and
		 * the NULL terminator.  We can rely on cnt to take into
		 * account the NULL terminator in all other scenarios,
		 * as we drop argv[0].
		 */
		memp = alloca(MAX(3, cnt + 2) * sizeof(char *));
		assert(memp != NULL);
		if (cnt > 0) {
			memp[0] = argv[0];
			memp[1] = path;
			memcpy(&memp[2], &argv[1], cnt * sizeof(char *));
		} else {
			memp[0] = "sh";
			memp[1] = path;
			memp[2] = NULL;
		}

		(void)_execve(_PATH_BSHELL, __DECONST(char **, memp), envp);
		return (-1);
	case ENOMEM:
	case E2BIG:
		/* Terminal: persistent condition */
		return (-1);
	case ETXTBSY:
		/*
		 * Terminal: we used to retry here, but sh(1) doesn't.
		 */
		return (-1);
	default:
		/*
		 * EACCES may be for an inaccessible directory or
		 * a non-executable file.  Call stat() to decide
		 * which.  This also handles ambiguities for EFAULT
		 * and EIO, and undocumented errors like ESTALE.
		 * We hope that the race for a stat() is unimportant.
		 */
		save_errno = errno;
		if (stat(path, &sb) == -1) {
			/*
			 * We force errno to ENOENT here to disambiguate the
			 * EACCESS case; the results of execve(2) are somewhat
			 * inconclusive because either the file did not exist or
			 * we just don't have search permissions, but the caller
			 * only really wants to see EACCES if the file did exist
			 * but was not accessible.
			 */
			if (save_errno == EACCES)
				errno = ENOENT;
			break;
		}

		errno = save_errno;

		/*
		 * Non-terminal: the file did exist and we just didn't have
		 * access to it, so we surface the EACCES and let the search
		 * continue for a candidate that we do have access to.
		 */
		if (errno == EACCES)
			break;

		/*
		 * All other errors here are terminal, as prescribed by exec(3).
		 */
		return (-1);
	}

	return (0);
}

static int
execvPe(const char *name, const char *path, char * const *argv,
    char * const *envp)
{
	char buf[MAXPATHLEN];
	size_t ln, lp;
	const char *np, *op, *p;
	bool eacces;

	eacces = false;

	/* If it's an absolute or relative path name, it's easy. */
	if (strchr(name, '/') != NULL) {
		/*
		 * We ignore non-terminal conditions because we don't have any
		 * further paths to try -- we can just bubble up the errno from
		 * execve(2) here.
		 */
		(void)execvPe_prog(name, argv, envp);
		return (-1);
	}

	/* If it's an empty path name, fail in the usual POSIX way. */
	if (*name == '\0') {
		errno = ENOENT;
		return (-1);
	}

	op = path;
	ln = strlen(name);
	while (op != NULL) {
		np = strchrnul(op, ':');

		/*
		 * It's a SHELL path -- double, leading and trailing colons
		 * mean the current directory.
		 */
		if (np == op) {
			/* Empty component. */
			p = ".";
			lp = 1;
		} else {
			/* Non-empty component. */
			p = op;
			lp = np - op;
		}

		/* Advance to the next component or terminate after this. */
		if (*np == '\0')
			op = NULL;
		else
			op = np + 1;

		/*
		 * If the path is too long, then complain.  This is a possible
		 * security issue: given a way to make the path too long, the
		 * user may execute the wrong program.
		 *
		 * Remember to exercise caution here with assembling our final
		 * buf and any output, as we may be running in a vfork() context
		 * via posix_spawnp().
		 */
		if (lp + ln + 2 > sizeof(buf)) {
			(void)_write(STDERR_FILENO, execvPe_err_preamble,
			    sizeof(execvPe_err_preamble) - 1);
			(void)_write(STDERR_FILENO, p, lp);
			(void)_write(STDERR_FILENO, execvPe_err_trailer,
			    sizeof(execvPe_err_trailer) - 1);

			continue;
		}

		memcpy(&buf[0], p, lp);
		buf[lp] = '/';
		memcpy(&buf[lp + 1], name, ln);
		buf[lp + ln + 1] = '\0';

		/*
		 * For terminal conditions we can just return immediately.  If
		 * it was non-terminal, we just need to note if we had an
		 * EACCES -- execvPe_prog would do a stat(2) and leave us with
		 * an errno of EACCES only if the file did exist; otherwise it
		 * would coerce it to an ENOENT because we may not know if a
		 * file actually existed there or not.
		 */
		if (execvPe_prog(buf, argv, envp) == -1)
			return (-1);
		if (errno == EACCES)
			eacces = true;
	}

	/*
	 * We don't often preserve errors encountering during the PATH search,
	 * so we override it here.  ENOENT would be misleading if we found a
	 * candidate but couldn't access it, but most of the other conditions
	 * are either terminal or indicate that nothing was there.
	 */
	if (eacces)
		errno = EACCES;
	else
		errno = ENOENT;

	return (-1);
}

int
execvP(const char *name, const char *path, char * const argv[])
{
	return execvPe(name, path, argv, environ);
}

int
__libc_execvpe(const char *name, char * const argv[], char * const envp[])
{
	const char *path;

	/* Get the path we're searching. */
	if ((path = getenv("PATH")) == NULL)
		path = _PATH_DEFPATH;

	return (execvPe(name, path, argv, envp));
}

__weak_reference(__libc_execvpe, execvpe);
