/*-
 * Copyright (c) 1990, 1993, 1994
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
 *
 * $FreeBSD$
 */

#ifndef lint
static const char sccsid[] = "@(#)pw_copy.c	8.4 (Berkeley) 4/2/94";
#endif /* not lint */

/*
 * This module is used to copy the master password file, replacing a single
 * record, by chpass(1) and passwd(1).
 */

#include <err.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <pw_util.h>
#include "pw_copy.h"

extern char *tempname;

void
pw_copy(ffd, tfd, pw)
	int ffd, tfd;
	struct passwd *pw;
{
	FILE *from, *to;
	int done;
	char *p, buf[8192];
	char uidstr[20];
	char gidstr[20];
	char chgstr[20];
	char expstr[20];

	snprintf(uidstr, sizeof(uidstr), "%lu", (unsigned long)pw->pw_uid);
	snprintf(gidstr, sizeof(gidstr), "%lu", (unsigned long)pw->pw_gid);
	snprintf(chgstr, sizeof(chgstr), "%ld", (long)pw->pw_change);
	snprintf(expstr, sizeof(expstr), "%ld", (long)pw->pw_expire);

	if (!(from = fdopen(ffd, "r")))
		pw_error(_PATH_MASTERPASSWD, 1, 1);
	if (!(to = fdopen(tfd, "w")))
		pw_error(tempname, 1, 1);

	for (done = 0; fgets(buf, sizeof(buf), from);) {
		if (!strchr(buf, '\n')) {
			warnx("%s: line too long", _PATH_MASTERPASSWD);
			pw_error(NULL, 0, 1);
		}
		if (done) {
			(void)fprintf(to, "%s", buf);
			if (ferror(to))
				goto err;
			continue;
		}
		for (p = buf; *p != '\n'; p++)
			if (*p != ' ' && *p != '\t')
				break;
		if (*p == '#' || *p == '\n') {
			(void)fprintf(to, "%s", buf);
			if (ferror(to))
				goto err;
			continue;
		}
		if (!(p = strchr(buf, ':'))) {
			warnx("%s: corrupted entry", _PATH_MASTERPASSWD);
			pw_error(NULL, 0, 1);
		}
		*p = '\0';
		if (strcmp(buf, pw->pw_name)) {
			*p = ':';
			(void)fprintf(to, "%s", buf);
			if (ferror(to))
				goto err;
			continue;
		}
		(void)fprintf(to, "%s:%s:%s:%s:%s:%s:%s:%s:%s:%s\n",
		    pw->pw_name, pw->pw_passwd,
		    pw->pw_fields & _PWF_UID ? uidstr : "",
		    pw->pw_fields & _PWF_GID ? gidstr : "",
		    pw->pw_class,
		    pw->pw_fields & _PWF_CHANGE ? chgstr : "",
		    pw->pw_fields & _PWF_EXPIRE ? expstr : "",
		    pw->pw_gecos, pw->pw_dir, pw->pw_shell);
		done = 1;
		if (ferror(to))
			goto err;
	}
	if (!done) {
#ifdef YP
	/* Ultra paranoid: shouldn't happen. */
		if (getuid())  {
			warnx("%s: not found in %s -- permission denied",
					pw->pw_name, _PATH_MASTERPASSWD);
			pw_error(NULL, 0, 1);
		} else
#endif /* YP */
		(void)fprintf(to, "%s:%s:%s:%s:%s:%s:%s:%s:%s:%s\n",
		    pw->pw_name, pw->pw_passwd,
		    pw->pw_fields & _PWF_UID ? uidstr : "",
		    pw->pw_fields & _PWF_GID ? gidstr : "",
		    pw->pw_class,
		    pw->pw_fields & _PWF_CHANGE ? chgstr : "",
		    pw->pw_fields & _PWF_EXPIRE ? expstr : "",
		    pw->pw_gecos, pw->pw_dir, pw->pw_shell);
	}

	if (ferror(to))
err:		pw_error(NULL, 1, 1);
	(void)fclose(to);
}
