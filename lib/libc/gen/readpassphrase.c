/*
 * Copyright (c) 2000 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$OpenBSD: readpassphrase.c,v 1.7 2001/08/07 19:34:11 millert Exp $";
#endif /* LIBC_SCCS and not lint */
include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "namespace.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <readpassphrase.h>
#include "un-namespace.h"

char *
readpassphrase(prompt, buf, bufsiz, flags)
	const char *prompt;
	char *buf;
	size_t bufsiz;
	int flags;
{
	struct termios term, oterm;
	char ch, *p, *end;
	int input, output;
	sigset_t oset, nset;

	/* I suppose we could alloc on demand in this case (XXX). */
	if (bufsiz == 0) {
		errno = EINVAL;
		return(NULL);
	}

	/*
	 * Read and write to /dev/tty if available.  If not, read from
	 * stdin and write to stderr unless a tty is required.
	 */
	if ((input = output = _open(_PATH_TTY, O_RDWR)) == -1) {
		if (flags & RPP_REQUIRE_TTY) {
			errno = ENOTTY;
			return(NULL);
		}
		input = STDIN_FILENO;
		output = STDERR_FILENO;
	}

	/*
	 * We block SIGINT and SIGTSTP so the terminal is not left
	 * in an inconsistent state (ie: no echo).  It would probably
	 * be better to simply catch these though.
	 */
	sigemptyset(&nset);
	sigaddset(&nset, SIGINT);
	sigaddset(&nset, SIGTSTP);
	(void)_sigprocmask(SIG_BLOCK, &nset, &oset);

	/* Turn off echo if possible. */
	if (tcgetattr(input, &oterm) == 0) {
		memcpy(&term, &oterm, sizeof(term));
		if (!(flags & RPP_ECHO_ON) && (term.c_lflag & ECHO))
			term.c_lflag &= ~ECHO;
		if (term.c_cc[VSTATUS] != _POSIX_VDISABLE)
			term.c_cc[VSTATUS] = _POSIX_VDISABLE;
		(void)tcsetattr(input, TCSAFLUSH|TCSASOFT, &term);
	} else {
		memset(&term, 0, sizeof(term));
		memset(&oterm, 0, sizeof(oterm));
	}

	(void)_write(output, prompt, strlen(prompt));
	end = buf + bufsiz - 1;
	for (p = buf; _read(input, &ch, 1) == 1 && ch != '\n' && ch != '\r';) {
		if (p < end) {
			if ((flags & RPP_SEVENBIT))
				ch &= 0x7f;
			if (isalpha(ch)) {
				if ((flags & RPP_FORCELOWER))
					ch = tolower(ch);
				if ((flags & RPP_FORCEUPPER))
					ch = toupper(ch);
			}
			*p++ = ch;
		}
	}
	*p = '\0';
	if (!(term.c_lflag & ECHO))
		(void)_write(output, "\n", 1);

	/* Restore old terminal settings and signal mask. */
	if (memcmp(&term, &oterm, sizeof(term)) != 0)
		(void)tcsetattr(input, TCSAFLUSH|TCSASOFT, &oterm);
	(void)_sigprocmask(SIG_SETMASK, &oset, NULL);
	if (input != STDIN_FILENO)
		(void)_close(input);
	return(buf);
}

char *
getpass(prompt)
        const char *prompt;
{
	static char buf[_PASSWORD_LEN + 1];

	return(readpassphrase(prompt, buf, sizeof(buf), RPP_ECHO_OFF));
}
