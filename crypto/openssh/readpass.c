/*
 * Copyright (c) 1988, 1993
 *      The Regents of the University of California.  All rights reserved.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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

#include "includes.h"
RCSID("$OpenBSD: readpass.c,v 1.15 2001/04/18 21:57:41 markus Exp $");

#include "xmalloc.h"
#include "cli.h"
#include "readpass.h"
#include "pathnames.h"
#include "log.h"
#include "atomicio.h"
#include "ssh.h"

char *
ssh_askpass(char *askpass, char *msg)
{
	pid_t pid;
	size_t len;
	char *nl, *pass;
	int p[2], status;
	char buf[1024];

	if (fflush(stdout) != 0)
		error("ssh_askpass: fflush: %s", strerror(errno));
	if (askpass == NULL)
		fatal("internal error: askpass undefined");
	if (pipe(p) < 0)
		fatal("ssh_askpass: pipe: %s", strerror(errno));
	if ((pid = fork()) < 0)
		fatal("ssh_askpass: fork: %s", strerror(errno));
	if (pid == 0) {
		seteuid(getuid());
		setuid(getuid());
		close(p[0]);
		if (dup2(p[1], STDOUT_FILENO) < 0)
			fatal("ssh_askpass: dup2: %s", strerror(errno));
		execlp(askpass, askpass, msg, (char *) 0);
		fatal("ssh_askpass: exec(%s): %s", askpass, strerror(errno));
	}
	close(p[1]);
	len = read(p[0], buf, sizeof buf);
	close(p[0]);
	while (waitpid(pid, &status, 0) < 0)
		if (errno != EINTR)
			break;
	if (len <= 1)
		return xstrdup("");
	nl = strchr(buf, '\n');
	if (nl)
		*nl = '\0';
	pass = xstrdup(buf);
	memset(buf, 0, sizeof(buf));
	return pass;
}


/*
 * Reads a passphrase from /dev/tty with echo turned off.  Returns the
 * passphrase (allocated with xmalloc), being very careful to ensure that
 * no other userland buffer is storing the password.
 */
/*
 * Note:  the funcationallity of this routing has been moved to
 * cli_read_passphrase().  This routing remains to maintain
 * compatibility with existing code.
 */
char *
read_passphrase(char *prompt, int from_stdin)
{
	char *askpass = NULL;
	int use_askpass = 0, ttyfd;

	if (from_stdin) {
		if (!isatty(STDIN_FILENO))
			use_askpass = 1;
	} else {
		ttyfd = open("/dev/tty", O_RDWR);
		if (ttyfd >= 0)
			close(ttyfd);
		else
			use_askpass = 1;
	}

	if (use_askpass && getenv("DISPLAY")) {
		if (getenv(SSH_ASKPASS_ENV))
			askpass = getenv(SSH_ASKPASS_ENV);
		else
			askpass = _PATH_SSH_ASKPASS_DEFAULT;
		return ssh_askpass(askpass, prompt);
	}

	return cli_read_passphrase(prompt, from_stdin, 0);
}
