/*	$OpenBSD: mailwrapper.c,v 1.16 2004/07/06 03:38:14 millert Exp $	*/
/*	$NetBSD: mailwrapper.c,v 1.9 2003/03/09 08:10:43 mjl Exp $	*/

/*
 * Copyright (c) 1998
 * 	Perry E. Metzger.  All rights reserved.
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
 *    must display the following acknowledgment:
 *	This product includes software developed for the NetBSD Project
 *	by Perry E. Metzger.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <libutil.h>
#include <sysexits.h>
#include <syslog.h>

#include "pathnames.h"

struct arglist {
	size_t argc, maxc;
	char **argv;
};

int main(int, char *[], char *[]);

static void initarg(struct arglist *);
static void addarg(struct arglist *, const char *);

static void
initarg(struct arglist *al)
{
	al->argc = 0;
	al->maxc = 10;
	if ((al->argv = malloc(al->maxc * sizeof(char *))) == NULL)
		err(EX_TEMPFAIL, "malloc");
}

static void
addarg(struct arglist *al, const char *arg)
{

	if (al->argc == al->maxc) {
		al->maxc <<= 1;
		al->argv = realloc(al->argv, al->maxc * sizeof(char *));
		if (al->argv == NULL)
			err(EX_TEMPFAIL, "realloc");
	}
	if (arg == NULL)
		al->argv[al->argc++] = NULL;
	else if ((al->argv[al->argc++] = strdup(arg)) == NULL)
		err(EX_TEMPFAIL, "strdup");
}

int
main(int argc, char *argv[], char *envp[])
{
	FILE *config;
	char *line, *cp, *from, *to, *ap;
	const char *progname;
	size_t len, lineno = 0;
	int i;
	struct arglist al;

	/* change __progname to mailwrapper so we get sensible error messages */
	progname = getprogname();
	setprogname("mailwrapper");

	initarg(&al);
	addarg(&al, argv[0]);

	if ((config = fopen(_PATH_MAILERCONF, "r")) == NULL) {
		addarg(&al, NULL);
		openlog(getprogname(), LOG_PID, LOG_MAIL);
		syslog(LOG_INFO, "cannot open %s, using %s as default MTA",
		    _PATH_MAILERCONF, _PATH_DEFAULTMTA);
		closelog();
		execve(_PATH_DEFAULTMTA, al.argv, envp);
		err(EX_OSERR, "cannot exec %s", _PATH_DEFAULTMTA);
		/*NOTREACHED*/
	}

	for (;;) {
		if ((line = fparseln(config, &len, &lineno, NULL, 0)) == NULL) {
			if (feof(config))
				errx(EX_CONFIG, "no mapping in %s", _PATH_MAILERCONF);
			err(EX_CONFIG, "cannot parse line %lu", (u_long)lineno);
		}

#define	WS	" \t\n"
		cp = line;

		cp += strspn(cp, WS);
		if (cp[0] == '\0') {
			/* empty line */
			free(line);
			continue;
		}

		if ((from = strsep(&cp, WS)) == NULL)
			goto parse_error;

		cp += strspn(cp, WS);

		if ((to = strsep(&cp, WS)) == NULL)
			goto parse_error;

		if (strcmp(from, progname) == 0) {
			for (ap = strsep(&cp, WS); ap != NULL;
			     ap = strsep(&cp, WS)) {
				if (*ap)
				    addarg(&al, ap);
			}
			break;
		}

		free(line);
	}

	(void)fclose(config);

	for (i = 1; i < argc; i++)
		addarg(&al, argv[i]);

	addarg(&al, NULL);
	execve(to, al.argv, envp);
	err(EX_OSERR, "cannot exec %s", to);
	/*NOTREACHED*/
parse_error:
	errx(EX_CONFIG, "parse error in %s at line %lu",
	    _PATH_MAILERCONF, (u_long)lineno);
	/*NOTREACHED*/
}
