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

#if 0
#ifndef lint
static char sccsid[] = "@(#)edit.c	8.3 (Berkeley) 4/2/94";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <md5.h>
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
edit(struct passwd *pw)
{
	struct stat begin, end;
	char *begin_sum, *end_sum;

	for (;;) {
		if (stat(tempname, &begin))
			pw_error(tempname, 1, 1);
		begin_sum = MD5File(tempname, (char *)NULL);
		pw_edit(1);
		if (stat(tempname, &end))
			pw_error(tempname, 1, 1);
		end_sum = MD5File(tempname, (char *)NULL);
		if ((begin.st_mtime == end.st_mtime) &&
		    (strcmp(begin_sum, end_sum) == 0)) {
			warnx("no changes made");
			pw_error(NULL, 0, 0);
		}
		free(begin_sum);
		free(end_sum);
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
display(int fd, struct passwd *pw)
{
	FILE *fp;
	char *bp, *p;

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
		(void)fprintf(fp, "Uid [#]: %lu\n", (unsigned long)pw->pw_uid);
		(void)fprintf(fp, "Gid [# or name]: %lu\n",
		    (unsigned long)pw->pw_gid);
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
#if 0
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
	p = strdup(p ? p : "");
	list[E_NAME].save = p;
	if (!list[E_NAME].restricted || !uid)
	  (void)fprintf(fp, "Full Name: %s\n", p);

        p = strsep(&bp, ",");
	p = strdup(p ? p : "");
	list[E_LOCATE].save = p;
	if (!list[E_LOCATE].restricted || !uid)
	  (void)fprintf(fp, "Office Location: %s\n", p);

        p = strsep(&bp, ",");
	p = strdup(p ? p : "");
	list[E_BPHONE].save = p;
	if (!list[E_BPHONE].restricted || !uid)
	  (void)fprintf(fp, "Office Phone: %s\n", p);

        p = strsep(&bp, ",");
	p = strdup(p ? p : "");
	list[E_HPHONE].save = p;
	if (!list[E_HPHONE].restricted || !uid)
	  (void)fprintf(fp, "Home Phone: %s\n", p);

	bp = strdup(bp ? bp : "");
	list[E_OTHER].save = bp;
	if (!list[E_OTHER].restricted || !uid)
	  (void)fprintf(fp, "Other information: %s\n", bp);

	(void)fchown(fd, getuid(), getgid());
	(void)fclose(fp);
}

int
verify(struct passwd *pw)
{
	ENTRY *ep;
	char *p;
	struct stat sb;
	FILE *fp;
	int len, line;
	static char buf[LINE_MAX];

	if (!(fp = fopen(tempname, "r")))
		pw_error(tempname, 1, 1);
	if (fstat(fileno(fp), &sb))
		pw_error(tempname, 1, 1);
	if (sb.st_size == 0) {
		warnx("corrupted temporary file");
		goto bad;
	}
	line = 0;
	while (fgets(buf, sizeof(buf), fp)) {
		line++;
		if (!buf[0] || buf[0] == '#')
			continue;
		if (!(p = strchr(buf, '\n'))) {
			warnx("line %d too long", line);
			goto bad;
		}
		*p = '\0';
		for (ep = list;; ++ep) {
			if (!ep->prompt) {
				warnx("unrecognized field on line %d", line);
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
					warnx("line %d corrupted", line);
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
	    strlen(list[E_HPHONE].save) + strlen(list[E_LOCATE].save) +
	    strlen(list[E_OTHER].save) + 5;
	if (!(p = malloc(len)))
		err(1, NULL);
	(void)sprintf(pw->pw_gecos = p, "%s,%s,%s,%s,%s", list[E_NAME].save,
	    list[E_LOCATE].save, list[E_BPHONE].save, list[E_HPHONE].save,
	    list[E_OTHER].save);

	while ((len = strlen(pw->pw_gecos)) && pw->pw_gecos[len - 1] == ',')
		pw->pw_gecos[len - 1] = '\0';

	if ((size_t)snprintf(buf, sizeof(buf),
	    "%s:%s:%lu:%lu:%s:%ld:%ld:%s:%s:%s",
	    pw->pw_name, pw->pw_passwd, (unsigned long)pw->pw_uid, 
	    (unsigned long)pw->pw_gid, pw->pw_class, (long)pw->pw_change,
	    (long)pw->pw_expire, pw->pw_gecos, pw->pw_dir,
	    pw->pw_shell) >= sizeof(buf)) {
		warnx("entries too long");
		free(p);
		return (0);
	}
	free(p);
	return (__pw_scan(buf, pw, _PWSCAN_WARN|_PWSCAN_MASTER));
}
