/*
 * Copyright (c) 1980, 1988 The Regents of the University of California.
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
char copyright[] =
"@(#) Copyright (c) 1980, 1988 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)from.c	5.7 (Berkeley) 3/1/91";
#endif /* not lint */

#include <sys/types.h>
#include <ctype.h>
#include <pwd.h>
#include <stdio.h>
#include <paths.h>

main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int optind;
	struct passwd *pwd;
	int ch, newline;
	char *file, *sender, *p;
#if MAXPATHLEN > BUFSIZ
	char buf[MAXPATHLEN];
#else
	char buf[BUFSIZ];
#endif

	file = sender = NULL;
	while ((ch = getopt(argc, argv, "f:s:")) != EOF)
		switch((char)ch) {
		case 'f':
			file = optarg;
			break;
		case 's':
			sender = optarg;
			for (p = sender; *p; ++p)
				if (isupper(*p))
					*p = tolower(*p);
			break;
		case '?':
		default:
			fprintf(stderr, "usage: from [-f file] [-s sender] [user]\n");
			exit(1);
		}
	argv += optind;

	if (!file) {
		if (!(file = *argv)) {
			if (!(pwd = getpwuid(getuid()))) {
				fprintf(stderr,
				    "from: no password file entry for you.\n");
				exit(1);
			}
			file = pwd->pw_name;
		}
		(void)sprintf(buf, "%s/%s", _PATH_MAILDIR, file);
		file = buf;
	}
	if (!freopen(file, "r", stdin)) {
		fprintf(stderr, "from: can't read %s.\n", file);
		exit(1);
	}
	for (newline = 1; fgets(buf, sizeof(buf), stdin);) {
		if (*buf == '\n') {
			newline = 1;
			continue;
		}
		if (newline && !strncmp(buf, "From ", 5) &&
		    (!sender || match(buf + 5, sender)))
			printf("%s", buf);
		newline = 0;
	}
	exit(0);
}

match(line, sender)
	register char *line, *sender;
{
	register char ch, pch, first, *p, *t;

	for (first = *sender++;;) {
		if (isspace(ch = *line))
			return(0);
		++line;
		if (isupper(ch))
			ch = tolower(ch);
		if (ch != first)
			continue;
		for (p = sender, t = line;;) {
			if (!(pch = *p++))
				return(1);
			if (isupper(ch = *t++))
				ch = tolower(ch);
			if (ch != pch)
				break;
		}
	}
	/* NOTREACHED */
}
