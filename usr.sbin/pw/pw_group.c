/*-
 * Copyright (C) 1996
 *	David L. Nugent.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY DAVID L. NUGENT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID L. NUGENT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/usr.sbin/pw/pw_group.c,v 1.12.2.1 2000/06/28 19:19:04 ache Exp $";
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <termios.h>
#include <unistd.h>

#include "pw.h"
#include "bitmap.h"


static int      print_group(struct group * grp, int pretty);
static gid_t    gr_gidpolicy(struct userconf * cnf, struct cargs * args);

int
pw_group(struct userconf * cnf, int mode, struct cargs * args)
{
	int		rc;
	struct carg    *a_name = getarg(args, 'n');
	struct carg    *a_gid = getarg(args, 'g');
	struct carg    *arg;
	struct group   *grp = NULL;
	int	        grmembers = 0;
	char           **members = NULL;

	static struct group fakegroup =
	{
		"nogroup",
		"*",
		-1,
		NULL
	};

	if (mode == M_LOCK || mode == M_UNLOCK)
		errx(EX_USAGE, "'lock' command is not available for groups");

	/*
	 * With M_NEXT, we only need to return the
	 * next gid to stdout
	 */
	if (mode == M_NEXT) {
		gid_t next = gr_gidpolicy(cnf, args);
		if (getarg(args, 'q'))
			return next;
		printf("%ld\n", (long)next);
		return EXIT_SUCCESS;
	}

	if (mode == M_PRINT && getarg(args, 'a')) {
		int             pretty = getarg(args, 'P') != NULL;

		SETGRENT();
		while ((grp = GETGRENT()) != NULL)
			print_group(grp, pretty);
		ENDGRENT();
		return EXIT_SUCCESS;
	}
	if (a_gid == NULL) {
		if (a_name == NULL)
			errx(EX_DATAERR, "group name or id required");

		if (mode != M_ADD && grp == NULL && isdigit((unsigned char)*a_name->val)) {
			(a_gid = a_name)->ch = 'g';
			a_name = NULL;
		}
	}
	grp = (a_name != NULL) ? GETGRNAM(a_name->val) : GETGRGID((gid_t) atoi(a_gid->val));

	if (mode == M_UPDATE || mode == M_DELETE || mode == M_PRINT) {
		if (a_name == NULL && grp == NULL)	/* Try harder */
			grp = GETGRGID(atoi(a_gid->val));

		if (grp == NULL) {
			if (mode == M_PRINT && getarg(args, 'F')) {
				char	*fmems[1];
				fmems[0] = NULL;
				fakegroup.gr_name = a_name ? a_name->val : "nogroup";
				fakegroup.gr_gid = a_gid ? (gid_t) atol(a_gid->val) : -1;
				fakegroup.gr_mem = fmems;
				return print_group(&fakegroup, getarg(args, 'P') != NULL);
			}
			errx(EX_DATAERR, "unknown group `%s'", a_name ? a_name->val : a_gid->val);
		}
		if (a_name == NULL)	/* Needed later */
			a_name = addarg(args, 'n', grp->gr_name);

		/*
		 * Handle deletions now
		 */
		if (mode == M_DELETE) {
			gid_t           gid = grp->gr_gid;

			rc = delgrent(grp);
			if (rc == -1)
				err(EX_IOERR, "group '%s' not available (NIS?)", grp->gr_name);
			else if (rc != 0) {
				warn("group update");
				return EX_IOERR;
			}
			pw_log(cnf, mode, W_GROUP, "%s(%ld) removed", a_name->val, (long) gid);
			return EXIT_SUCCESS;
		} else if (mode == M_PRINT)
			return print_group(grp, getarg(args, 'P') != NULL);

		if (a_gid)
			grp->gr_gid = (gid_t) atoi(a_gid->val);

		if ((arg = getarg(args, 'l')) != NULL)
			grp->gr_name = pw_checkname((u_char *)arg->val, 0);
	} else {
		if (a_name == NULL)	/* Required */
			errx(EX_DATAERR, "group name required");
		else if (grp != NULL)	/* Exists */
			errx(EX_DATAERR, "group name `%s' already exists", a_name->val);

		extendarray(&members, &grmembers, 200);
		members[0] = NULL;
		grp = &fakegroup;
		grp->gr_name = pw_checkname((u_char *)a_name->val, 0);
		grp->gr_passwd = "*";
		grp->gr_gid = gr_gidpolicy(cnf, args);
		grp->gr_mem = members;
	}

	/*
	 * This allows us to set a group password Group passwords is an
	 * antique idea, rarely used and insecure (no secure database) Should
	 * be discouraged, but it is apparently still supported by some
	 * software.
	 */

	if ((arg = getarg(args, 'h')) != NULL) {
		if (strcmp(arg->val, "-") == 0)
			grp->gr_passwd = "*";	/* No access */
		else {
			int             fd = atoi(arg->val);
			int             b;
			int             istty = isatty(fd);
			struct termios  t;
			char           *p, line[256];

			if (istty) {
				if (tcgetattr(fd, &t) == -1)
					istty = 0;
				else {
					struct termios  n = t;

					/* Disable echo */
					n.c_lflag &= ~(ECHO);
					tcsetattr(fd, TCSANOW, &n);
					printf("%sassword for group %s:", (mode == M_UPDATE) ? "New p" : "P", grp->gr_name);
					fflush(stdout);
				}
			}
			b = read(fd, line, sizeof(line) - 1);
			if (istty) {	/* Restore state */
				tcsetattr(fd, TCSANOW, &t);
				fputc('\n', stdout);
				fflush(stdout);
			}
			if (b < 0) {
				warn("-h file descriptor");
				return EX_OSERR;
			}
			line[b] = '\0';
			if ((p = strpbrk(line, " \t\r\n")) != NULL)
				*p = '\0';
			if (!*line)
				errx(EX_DATAERR, "empty password read on file descriptor %d", fd);
			grp->gr_passwd = pw_pwcrypt(line);
		}
	}

	if (((arg = getarg(args, 'M')) != NULL || (arg = getarg(args, 'm')) != NULL) && arg->val) {
		int	i = 0;
		char   *p;
		struct passwd	*pwd;

		/* Make sure this is not stay NULL with -M "" */
		extendarray(&members, &grmembers, 200);
		if (arg->ch == 'm') {
			int	k = 0;

			while (grp->gr_mem[k] != NULL) {
				if (extendarray(&members, &grmembers, i + 2) != -1) {
					members[i++] = grp->gr_mem[k];
				}
				k++;
			}
		}
		for (p = strtok(arg->val, ", \t"); p != NULL; p = strtok(NULL, ", \t")) {
			int     j;
			if ((pwd = GETPWNAM(p)) == NULL) {
				if (!isdigit((unsigned char)*p) || (pwd = getpwuid((uid_t) atoi(p))) == NULL)
					errx(EX_NOUSER, "user `%s' does not exist", p);
			}
			/*
			 * Check for duplicates
			 */
			for (j = 0; j < i && strcmp(members[j], pwd->pw_name)!=0; j++)
				;
			if (j == i && extendarray(&members, &grmembers, i + 2) != -1)
				members[i++] = newstr(pwd->pw_name);
		}
		while (i < grmembers)
			members[i++] = NULL;
		grp->gr_mem = members;
	}

	if (getarg(args, 'N') != NULL)
		return print_group(grp, getarg(args, 'P') != NULL);

	if (mode == M_ADD && (rc = addgrent(grp)) != 0) {
		if (rc == -1)
			warnx("group '%s' already exists", grp->gr_name);
		else
			warn("group update");
		return EX_IOERR;
	} else if (mode == M_UPDATE && (rc = chggrent(a_name->val, grp)) != 0) {
		if (rc == -1)
			warnx("group '%s' not available (NIS?)", grp->gr_name);
		else
			warn("group update");
		return EX_IOERR;
	}
	/* grp may have been invalidated */
	if ((grp = GETGRNAM(a_name->val)) == NULL)
		errx(EX_SOFTWARE, "group disappeared during update");

	pw_log(cnf, mode, W_GROUP, "%s(%ld)", grp->gr_name, (long) grp->gr_gid);

	if (members)
		free(members);

	return EXIT_SUCCESS;
}


static          gid_t
gr_gidpolicy(struct userconf * cnf, struct cargs * args)
{
	struct group   *grp;
	gid_t           gid = (gid_t) - 1;
	struct carg    *a_gid = getarg(args, 'g');

	/*
	 * Check the given gid, if any
	 */
	if (a_gid != NULL) {
		gid = (gid_t) atol(a_gid->val);

		if ((grp = GETGRGID(gid)) != NULL && getarg(args, 'o') == NULL)
			errx(EX_DATAERR, "gid `%ld' has already been allocated", (long) grp->gr_gid);
	} else {
		struct bitmap   bm;

		/*
		 * We need to allocate the next available gid under one of
		 * two policies a) Grab the first unused gid b) Grab the
		 * highest possible unused gid
		 */
		if (cnf->min_gid >= cnf->max_gid) {	/* Sanity claus^H^H^H^Hheck */
			cnf->min_gid = 1000;
			cnf->max_gid = 32000;
		}
		bm = bm_alloc(cnf->max_gid - cnf->min_gid + 1);

		/*
		 * Now, let's fill the bitmap from the password file
		 */
		SETGRENT();
		while ((grp = GETGRENT()) != NULL)
			if ((gid_t)grp->gr_gid >= (gid_t)cnf->min_gid &&
                            (gid_t)grp->gr_gid <= (gid_t)cnf->max_gid)
				bm_setbit(&bm, grp->gr_gid - cnf->min_gid);
		ENDGRENT();

		/*
		 * Then apply the policy, with fallback to reuse if necessary
		 */
		if (cnf->reuse_gids)
			gid = (gid_t) (bm_firstunset(&bm) + cnf->min_gid);
		else {
			gid = (gid_t) (bm_lastset(&bm) + 1);
			if (!bm_isset(&bm, gid))
				gid += cnf->min_gid;
			else
				gid = (gid_t) (bm_firstunset(&bm) + cnf->min_gid);
		}

		/*
		 * Another sanity check
		 */
		if (gid < cnf->min_gid || gid > cnf->max_gid)
			errx(EX_SOFTWARE, "unable to allocate a new gid - range fully used");
		bm_dealloc(&bm);
	}
	return gid;
}


static int
print_group(struct group * grp, int pretty)
{
	if (!pretty) {
		int		buflen = 0;
		char           *buf = NULL;

		fmtgrent(&buf, &buflen, grp);
		fputs(buf, stdout);
		free(buf);
	} else {
		int             i;

		printf("Group Name: %-15s   #%lu\n"
		       "   Members: ",
		       grp->gr_name, (long) grp->gr_gid);
		for (i = 0; grp->gr_mem[i]; i++)
			printf("%s%s", i ? "," : "", grp->gr_mem[i]);
		fputs("\n\n", stdout);
	}
	return EXIT_SUCCESS;
}
