/*
 * Copyright (c) 1988 The Regents of the University of California.
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
static char sccsid[] = "@(#)field.c	5.13 (Berkeley) 2/12/91";
#endif /* not lint */

#include <sys/param.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "chpass.h"
#include "pathnames.h"

/* ARGSUSED */
p_login(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!*p) {
		(void)fprintf(stderr, "chpass: empty login field.\n");
		return(1);
	}
	if (*p == '-') {
		(void)fprintf(stderr,
		    "chpass: login names may not begin with a hyphen.\n");
		return(1);
	}
	if (!(pw->pw_name = strdup(p))) {
		(void)fprintf(stderr, "chpass: can't save entry.\n");
		return(1);
	}
	if (index(p, '.'))
		(void)fprintf(stderr,
		    "chpass: \'.\' is dangerous in a login name.\n");
	for (; *p; ++p)
		if (isupper(*p)) {
			(void)fprintf(stderr,
			    "chpass: upper-case letters are dangerous in a login name.\n");
			break;
		}
	return(0);
}

/* ARGSUSED */
p_passwd(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!*p)
		pw->pw_passwd = "";	/* "NOLOGIN"; */
	else if (!(pw->pw_passwd = strdup(p))) {
		(void)fprintf(stderr, "chpass: can't save password entry.\n");
		return(1);
	}
	
	return(0);
}

/* ARGSUSED */
p_uid(p, pw, ep)
	register char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	int id;

	if (!*p) {
		(void)fprintf(stderr, "chpass: empty uid field.\n");
		return(1);
	}
	if (!isdigit(*p)) {
		(void)fprintf(stderr, "chpass: illegal uid.\n");
		return(1);
	}
	id = atoi(p);
	if ((u_int)id > USHRT_MAX) {
		(void)fprintf(stderr, "chpass: %d > max uid value (%d).\n",
		    id, USHRT_MAX);
		return(1);
	}
	pw->pw_uid = id;
	return(0);
}

/* ARGSUSED */
p_gid(p, pw, ep)
	register char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	struct group *gr;
	int id;

	if (!*p) {
		(void)fprintf(stderr, "chpass: empty gid field.\n");
		return(1);
	}
	if (!isdigit(*p)) {
		if (!(gr = getgrnam(p))) {
			(void)fprintf(stderr,
			    "chpass: unknown group %s.\n", p);
			return(1);
		}
		pw->pw_gid = gr->gr_gid;
		return(0);
	}
	id = atoi(p);
	if ((u_int)id > USHRT_MAX) {
		(void)fprintf(stderr, "chpass: %d > max gid value (%d).\n",
		    id, USHRT_MAX);
		return(1);
	}
	pw->pw_gid = id;
	return(0);
}

/* ARGSUSED */
p_class(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!*p)
		pw->pw_class = "";
	else if (!(pw->pw_class = strdup(p))) {
		(void)fprintf(stderr, "chpass: can't save entry.\n");
		return(1);
	}
	
	return(0);
}

/* ARGSUSED */
p_change(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!atot(p, &pw->pw_change))
		return(0);
	(void)fprintf(stderr, "chpass: illegal date for change field.\n");
	return(1);
}

/* ARGSUSED */
p_expire(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!atot(p, &pw->pw_expire))
		return(0);
	(void)fprintf(stderr, "chpass: illegal date for expire field.\n");
	return(1);
}

/* ARGSUSED */
p_gecos(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!*p)
		ep->save = "";
	else if (!(ep->save = strdup(p))) {
		(void)fprintf(stderr, "chpass: can't save entry.\n");
		return(1);
	}
	return(0);
}

/* ARGSUSED */
p_hdir(p, pw, ep)
	char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	if (!*p) {
		(void)fprintf(stderr, "chpass: empty home directory field.\n");
		return(1);
	}
	if (!(pw->pw_dir = strdup(p))) {
		(void)fprintf(stderr, "chpass: can't save entry.\n");
		return(1);
	}
	return(0);
}

/* ARGSUSED */
p_shell(p, pw, ep)
	register char *p;
	struct passwd *pw;
	ENTRY *ep;
{
	char *t, *ok_shell();

	if (!*p) {
		pw->pw_shell = _PATH_BSHELL;
		return(0);
	}
	/* only admin can change from or to "restricted" shells */
	if (uid && pw->pw_shell && !ok_shell(pw->pw_shell)) {
		(void)fprintf(stderr,
		    "chpass: %s: current shell non-standard.\n", pw->pw_shell);
		return(1);
	}
	if (!(t = ok_shell(p))) {
		if (uid) {
			(void)fprintf(stderr,
			    "chpass: %s: non-standard shell.\n", p);
			return(1);
		}
	}
	else
		p = t;
	if (!(pw->pw_shell = strdup(p))) {
		(void)fprintf(stderr, "chpass: can't save entry.\n");
		return(1);
	}
	return(0);
}
