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
static char sccsid[] = "@(#)edit.c	8.3 (Berkeley) 4/2/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pw_scan.h>
#include <pw_util.h>

#include "chpass.h"
#ifdef YP
#include "pw_yp.h"
#endif /* YP */

extern char *tempname;

void
edit(pw)
	struct passwd *pw;
{
	struct stat begin, end;

	for (;;) {
		if (stat(tempname, &begin))
			pw_error(tempname, 1, 1);
		pw_edit(1);
		if (stat(tempname, &end))
			pw_error(tempname, 1, 1);
		if (begin.st_mtime == end.st_mtime) {
			warnx("no changes made");
			pw_error(NULL, 0, 0);
		}
		if (verify(pw))
			break;
		pw_prompt();
	}
}

/*
 * display --
 *	print out the file for the user to edit; strange side-effect:
 *	set conditional flag if the user gets to edit the shell.
 */
void
display(fd, pw)
	int fd;
	struct passwd *pw;
{
	FILE *fp;
	char *bp, *p, *ttoa();

	if (!(fp = fdopen(fd, "w")))
		pw_error(tempname, 1, 1);

	(void)fprintf(fp,
#ifdef YP
	    "#Changing %s information for %s.\n", _use_yp ? "NIS" : "user database", pw->pw_name);
	if (!uid && (!_use_yp || suser_override)) {
#else
	    "#Changing user database information for %s.\n", pw->pw_name);
	if (!uid) {
#endif /* YP */
		(void)fprintf(fp, "Login: %s\n", pw->pw_name);
		(void)fprintf(fp, "Password: %s\n", pw->pw_passwd);
		(void)fprintf(fp, "Uid [#]: %d\n", pw->pw_uid);
		(void)fprintf(fp, "Gid [# or name]: %d\n", pw->pw_gid);
		(void)fprintf(fp, "Change [month day year]: %s\n",
		    ttoa(pw->pw_change));
		(void)fprintf(fp, "Expire [month day year]: %s\n",
		    ttoa(pw->pw_expire));
		(void)fprintf(fp, "Class: %s\n", pw->pw_class);
		(void)fprintf(fp, "Home directory: %s\n", pw->pw_dir);
		(void)fprintf(fp, "Shell: %s\n",
		    *pw->pw_shell ? pw->pw_shell : _PATH_BSHELL);
	}
	/* Only admin can change "restricted" shells. */
#ifdef 0
	else if (ok_shell(pw->pw_shell))
		/*
		 * Make shell a restricted field.  Ugly with a
		 * necklace, but there's not much else to do.
		 */
#else
	else if ((!list[E_SHELL].restricted && ok_shell(pw->pw_shell)) || !uid)
		/*
		 * If change not restrict (table.c) and standard shell
		 *	OR if root, then allow editing of shell.
		 */
#endif
		(void)fprintf(fp, "Shell: %s\n",
		    *pw->pw_shell ? pw->pw_shell : _PATH_BSHELL);
	else
	  list[E_SHELL].restricted = 1;
	bp = pw->pw_gecos;

	p = strsep(&bp, ",");
	if (p)
	  list[E_NAME].save = strdup(p);
	if (!list[E_NAME].restricted || !uid)
	  (void)fprintf(fp, "Full Name: %s\n", p ? p : "");

        p = strsep(&bp, ",");
	if (p)
	  list[E_LOCATE].save = strdup(p);
	if (!list[E_LOCATE].restricted || !uid)
	  (void)fprintf(fp, "Location: %s\n", p ? p : "");

        p = strsep(&bp, ",");
	if (p)
	  list[E_BPHONE].save = strdup(p);
	if (!list[E_BPHONE].restricted || !uid)
	  (void)fprintf(fp, "Office Phone: %s\n", p ? p : "");

        p = strsep(&bp, ",");
	if (p)
	  list[E_HPHONE].save = strdup(p);
	if (!list[E_HPHONE].restricted || !uid)
	  (void)fprintf(fp, "Home Phone: %s\n", p ? p : "");

	(void)fchown(fd, getuid(), getgid());
	(void)fclose(fp);
}

int
verify(pw)
	struct passwd *pw;
{
	ENTRY *ep;
	char *p;
	struct stat sb;
	FILE *fp;
	int len;
	char buf[LINE_MAX];

	if (!(fp = fopen(tempname, "r")))
		pw_error(tempname, 1, 1);
	if (fstat(fileno(fp), &sb))
		pw_error(tempname, 1, 1);
	if (sb.st_size == 0) {
		warnx("corrupted temporary file");
		goto bad;
	}
	while (fgets(buf, sizeof(buf), fp)) {
		if (!buf[0] || buf[0] == '#')
			continue;
		if (!(p = strchr(buf, '\n'))) {
			warnx("line too long");
			goto bad;
		}
		*p = '\0';
		for (ep = list;; ++ep) {
			if (!ep->prompt) {
				warnx("unrecognized field");
				goto bad;
			}
			if (!strncasecmp(buf, ep->prompt, ep->len)) {
				if (ep->restricted && uid) {
					warnx(
					    "you may not change the %s field",
						ep->prompt);
					goto bad;
				}
				if (!(p = strchr(buf, ':'))) {
					warnx("line corrupted");
					goto bad;
				}
				while (isspace(*++p));
				if (ep->except && strpbrk(p, ep->except)) {
					warnx(
				   "illegal character in the \"%s\" field",
					    ep->prompt);
					goto bad;
				}
				if ((ep->func)(p, pw, ep)) {
bad:					(void)fclose(fp);
					return (0);
				}
				break;
			}
		}
	}
	(void)fclose(fp);

	/* Build the gecos field. */
	len = strlen(list[E_NAME].save) + strlen(list[E_BPHONE].save) +
	    strlen(list[E_HPHONE].save) + strlen(list[E_LOCATE].save) + 4;
	if (!(p = malloc(len)))
		err(1, NULL);
	(void)sprintf(pw->pw_gecos = p, "%s,%s,%s,%s", list[E_NAME].save,
	    list[E_LOCATE].save, list[E_BPHONE].save, list[E_HPHONE].save);

	while ((len = strlen(pw->pw_gecos)) && pw->pw_gecos[len - 1] == ',')
		pw->pw_gecos[len - 1] = '\0';

	if (snprintf(buf, sizeof(buf),
	    "%s:%s:%d:%d:%s:%ld:%ld:%s:%s:%s",
	    pw->pw_name, pw->pw_passwd, pw->pw_uid, pw->pw_gid, pw->pw_class,
	    pw->pw_change, pw->pw_expire, pw->pw_gecos, pw->pw_dir,
	    pw->pw_shell) >= sizeof(buf)) {
		warnx("entries too long");
		return (0);
	}
	return (pw_scan(buf, pw));
}
