/*-
 * Copyright (c) 1990, 1993
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
static char sccsid[] = "@(#)daemon.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/gen/daemon.c,v 1.6 2003/11/10 22:01:42 ghelmer Exp $");

#include "namespace.h"
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <unistd.h>
#include "un-namespace.h"

int
daemon(nochdir, noclose)
	int nochdir, noclose;
{
	struct sigaction osa, sa;
	int fd;
	pid_t newgrp;
	int oerrno;
	int osa_ok;

	/* A SIGHUP may be thrown when the parent exits below. */
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	osa_ok = _sigaction(SIGHUP, &sa, &osa);

	switch (fork()) {
	case -1:
		return (-1);
	case 0:
		break;
	default:
		_exit(0);
	}

	newgrp = setsid();
	oerrno = errno;
	if (osa_ok != -1)
		_sigaction(SIGHUP, &osa, NULL);

	if (newgrp == -1) {
		errno = oerrno;
		return (-1);
	}

	if (!nochdir)
		(void)chdir("/");

	if (!noclose && (fd = _open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
		(void)_dup2(fd, STDIN_FILENO);
		(void)_dup2(fd, STDOUT_FILENO);
		(void)_dup2(fd, STDERR_FILENO);
		if (fd > 2)
			(void)_close(fd);
	}
	return (0);
}
