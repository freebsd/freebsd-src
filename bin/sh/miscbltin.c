/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
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

#ifndef lint
static char sccsid[] = "@(#)miscbltin.c	8.2 (Berkeley) 4/16/94";
#endif /* not lint */

/*
 * Miscelaneous builtins.
 */

#include "shell.h"
#include "options.h"
#include "var.h"
#include "output.h"
#include "memalloc.h"
#include "error.h"
#include "mystring.h"

#undef eflag

extern char **argptr;		/* argument list for builtin command */


/*
 * The read builtin.  The -e option causes backslashes to escape the
 * following character.
 *
 * This uses unbuffered input, which may be avoidable in some cases.
 */

readcmd(argc, argv)  char **argv; {
	char **ap;
	int backslash;
	char c;
	int eflag;
	char *prompt;
	char *ifs;
	char *p;
	int startword;
	int status;
	int i;

	eflag = 0;
	prompt = NULL;
	while ((i = nextopt("ep:")) != '\0') {
		if (i == 'p')
			prompt = optarg;
		else
			eflag = 1;
	}
	if (prompt && isatty(0)) {
		out2str(prompt);
		flushall();
	}
	if (*(ap = argptr) == NULL)
		error("arg count");
	if ((ifs = bltinlookup("IFS", 1)) == NULL)
		ifs = nullstr;
	status = 0;
	startword = 1;
	backslash = 0;
	STARTSTACKSTR(p);
	for (;;) {
		if (read(0, &c, 1) != 1) {
			status = 1;
			break;
		}
		if (c == '\0')
			continue;
		if (backslash) {
			backslash = 0;
			if (c != '\n')
				STPUTC(c, p);
			continue;
		}
		if (eflag && c == '\\') {
			backslash++;
			continue;
		}
		if (c == '\n')
			break;
		if (startword && *ifs == ' ' && strchr(ifs, c)) {
			continue;
		}
		startword = 0;
		if (backslash && c == '\\') {
			if (read(0, &c, 1) != 1) {
				status = 1;
				break;
			}
			STPUTC(c, p);
		} else if (ap[1] != NULL && strchr(ifs, c) != NULL) {
			STACKSTRNUL(p);
			setvar(*ap, stackblock(), 0);
			ap++;
			startword = 1;
			STARTSTACKSTR(p);
		} else {
			STPUTC(c, p);
		}
	}
	STACKSTRNUL(p);
	setvar(*ap, stackblock(), 0);
	while (*++ap != NULL)
		setvar(*ap, nullstr, 0);
	return status;
}



umaskcmd(argc, argv)  char **argv; {
	int mask;
	char *p;
	int i;

	if ((p = argv[1]) == NULL) {
		INTOFF;
		mask = umask(0);
		umask(mask);
		INTON;
		out1fmt("%.4o\n", mask);	/* %#o might be better */
	} else {
		mask = 0;
		do {
			if ((unsigned)(i = *p - '0') >= 8)
				error("Illegal number: %s", argv[1]);
			mask = (mask << 3) + i;
		} while (*++p != '\0');
		umask(mask);
	}
	return 0;
}
