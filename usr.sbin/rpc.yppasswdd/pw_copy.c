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
 */

#ifndef lint
static char sccsid[] = "@(#)pw_copy.c	8.4 (Berkeley) 4/2/94";
#endif /* not lint */

/*
 * This module is used to copy the master password file, replacing a single
 * record, by chpass(1) and passwd(1).
 */

#include <err.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>

#include <pw_util.h>
#include "yppasswdd_extern.h"

int
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

	snprintf(uidstr, sizeof(uidstr), "%d", pw->pw_uid);
	snprintf(gidstr, sizeof(gidstr), "%d", pw->pw_gid);
	snprintf(chgstr, sizeof(chgstr), "%ld", pw->pw_change);
	snprintf(expstr, sizeof(expstr), "%ld", pw->pw_expire);

	if (!(from = fdopen(ffd, "r"))) {
		pw_error(passfile, 1, 1);
		return(-1);
	}
	if (!(to = fdopen(tfd, "w"))) {
		pw_error(tempname, 1, 1);
		return(-1);
	}
	for (done = 0; fgets(buf, sizeof(buf), from);) {
		if (!strchr(buf, '\n')) {
			yp_error("%s: line too long", passfile);
			pw_error(NULL, 0, 1);
			goto err;
		}
		if (done) {
			(void)fprintf(to, "%s", buf);
			if (ferror(to))
				goto err;
			continue;
		}
		if (!(p = strchr(buf, ':'))) {
			yp_error("%s: corrupted entry", passfile);
			pw_error(NULL, 0, 1);
			goto err;
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
		if (allow_additions) {
			(void)fprintf(to, "%s:%s:%s:%s:%s:%s:%s:%s:%s:%s\n",
			pw->pw_name, pw->pw_passwd,
			pw->pw_fields & _PWF_UID ? uidstr : "",
			pw->pw_fields & _PWF_GID ? gidstr : "",
			pw->pw_class,
			pw->pw_fields & _PWF_CHANGE ? chgstr : "",
			pw->pw_fields & _PWF_EXPIRE ? expstr : "",
			pw->pw_gecos, pw->pw_dir, pw->pw_shell);
		} else {
			yp_error("user \"%s\" not found in %s -- \
NIS maps and password file possibly out of sync", pw->pw_name, passfile);
			goto err;
		}
	}
	if (ferror(to)) {
err:		pw_error(NULL, 1, 1);
		(void)fclose(to);
		(void)fclose(from);
		return(-1);
	}
	(void)fclose(to);
	(void)fclose(from);
	return(0);
}
