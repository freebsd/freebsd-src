/*
 * Copyright (c) 1988, 1993, 1994
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
static char sccsid[] = "@(#)field.c	8.4 (Berkeley) 4/2/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "chpass.h"
#include "pathnames.h"

/* ARGSUSED */
int
p_login(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!*p) {
		warnx("empty login field");
		return (1);
	}
	if (*p == '-') {
		warnx("login names may not begin with a hyphen");
		return (1);
	}
	if (!(pw->pw_name = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}
	if (strchr(p, '.'))
		warnx("\'.\' is dangerous in a login name");
	for (; *p; ++p)
		if (isupper(*p)) {
			warnx("upper-case letters are dangerous in a login name");
			break;
		}
	return (0);
}

/* ARGSUSED */
int
p_passwd(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!*p)
		pw->pw_passwd = "";	/* "NOLOGIN"; */
	else if (!(pw->pw_passwd = strdup(p))) {
		warnx("can't save password entry");
		return (1);
	}

	return (0);
}

/* ARGSUSED */
int
p_uid(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	uid_t id;
	char *np;

	if (!*p) {
		warnx("empty uid field");
		return (1);
	}
	if (!isdigit(*p)) {
		warnx("illegal uid");
		return (1);
	}
	errno = 0;
	id = strtoul(p, &np, 10);
	if (*np || (id == ULONG_MAX && errno == ERANGE)) {
		warnx("illegal uid");
		return (1);
	}
	pw->pw_uid = id;
	return (0);
}

/* ARGSUSED */
int
p_gid(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	struct group *gr;
	gid_t id;
	char *np;

	if (!*p) {
		warnx("empty gid field");
		return (1);
	}
	if (!isdigit(*p)) {
		if (!(gr = getgrnam(p))) {
			warnx("unknown group %s", p);
			return (1);
		}
		pw->pw_gid = gr->gr_gid;
		return (0);
	}
	errno = 0;
	id = strtoul(p, &np, 10);
	if (*np || (id == ULONG_MAX && errno == ERANGE)) {
		warnx("illegal gid");
		return (1);
	}
	pw->pw_gid = id;
	return (0);
}

/* ARGSUSED */
int
p_class(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!*p)
		pw->pw_class = "";
	else if (!(pw->pw_class = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}

	return (0);
}

/* ARGSUSED */
int
p_change(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!atot(p, &pw->pw_change))
		return (0);
	warnx("illegal date for change field");
	return (1);
}

/* ARGSUSED */
int
p_expire(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!atot(p, &pw->pw_expire))
		return (0);
	warnx("illegal date for expire field");
	return (1);
}

/* ARGSUSED */
int
p_gecos(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!*p)
		ep->save = "";
	else if (!(ep->save = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}
	return (0);
}

/* ARGSUSED */
int
p_hdir(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!*p) {
		warnx("empty home directory field");
		return (1);
	}
	if (!(pw->pw_dir = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}
	return (0);
}

/* ARGSUSED */
int
p_shell(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	char *t, *ok_shell();
	struct stat sbuf;

	if (!*p) {
		pw->pw_shell = _PATH_BSHELL;
		return (0);
	}
	/* only admin can change from or to "restricted" shells */
	if (uid && pw->pw_shell && !ok_shell(pw->pw_shell)) {
		warnx("%s: current shell non-standard", pw->pw_shell);
		return (1);
	}
	if (!(t = ok_shell(p))) {
		if (uid) {
			warnx("%s: non-standard shell", p);
			return (1);
		}
	}
	else
		p = t;
	if (!(pw->pw_shell = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}
	if (stat(pw->pw_shell, &sbuf) < 0) {
		if (errno == ENOENT)
			warnx("WARNING: shell '%s' does not exist",
			    pw->pw_shell);
		else
			warn("WARNING: can't stat shell '%s'",  pw->pw_shell);
		return (0);
	}
	if (!S_ISREG(sbuf.st_mode)) {
		warnx("WARNING: shell '%s' is not a regular file", 
			pw->pw_shell);
		return (0);
	}
	if ((sbuf.st_mode & (S_IXOTH | S_IXGRP | S_IXUSR)) == 0) {
		warnx("WARNING: shell '%s' is not executable", pw->pw_shell);
		return (0);
	}
	return (0);
}
