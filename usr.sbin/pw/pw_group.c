/*-
 * Copyright (c) 1996 by David L. Nugent <davidn@blaze.net.au>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by David L. Nugent.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DAVID L. NUGENT ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID L. NUGENT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id$
 */

#include <unistd.h>
#include <ctype.h>
#include <termios.h>

#include "pw.h"
#include "bitmap.h"


static int      print_group(struct group * grp, int pretty);
static gid_t    gr_gidpolicy(struct userconf * cnf, struct cargs * args);

int
pw_group(struct userconf * cnf, int mode, struct cargs * args)
{
	struct carg    *a_name = getarg(args, 'n');
	struct carg    *a_gid = getarg(args, 'g');
	struct carg    *arg;
	struct group   *grp = NULL;
	char           *members[_UC_MAXGROUPS];

	static struct group fakegroup =
	{
		"nogroup",
		"*",
		-1,
		NULL
	};

	if (mode == M_PRINT && getarg(args, 'a')) {
		int             pretty = getarg(args, 'p') != NULL;

		setgrent();
		while ((grp = getgrent()) != NULL)
			print_group(grp, pretty);
		endgrent();
		return X_ALLOK;
	}
	if (a_gid == NULL) {
		if (a_name == NULL)
			cmderr(X_CMDERR, "group name or id required\n");

		if (mode != M_ADD && grp == NULL && isdigit(*a_name->val)) {
			(a_gid = a_name)->ch = 'g';
			a_name = NULL;
		}
	}
	grp = (a_name != NULL) ? getgrnam(a_name->val) : getgrgid((gid_t) atoi(a_gid->val));

	if (mode == M_UPDATE || mode == M_DELETE || mode == M_PRINT) {
		if (a_name == NULL && grp == NULL)	/* Try harder */
			grp = getgrgid(atoi(a_gid->val));

		if (grp == NULL) {
			if (mode == M_PRINT && getarg(args, 'F')) {
				fakegroup.gr_name = a_name ? a_name->val : "nogroup";
				fakegroup.gr_gid = a_gid ? (gid_t) atol(a_gid->val) : -1;
				return print_group(&fakegroup, getarg(args, 'p') != NULL);
			}
			cmderr(X_CMDERR, "unknown group `%s'\n", a_name ? a_name->val : a_gid->val);
		}
		if (a_name == NULL)	/* Needed later */
			a_name = addarg(args, 'n', grp->gr_name);

		/*
		 * Handle deletions now
		 */
		if (mode == M_DELETE) {
			gid_t           gid = grp->gr_gid;

			if (delgrent(grp) == -1)
				cmderr(X_NOUPDATE, "Error updating group file: %s\n", strerror(errno));
			pw_log(cnf, mode, W_GROUP, "%s(%ld) removed", a_name->val, (long) gid);
			return X_ALLOK;
		} else if (mode == M_PRINT)
			return print_group(grp, getarg(args, 'p') != NULL);

		if (a_gid)
			grp->gr_gid = (gid_t) atoi(a_gid->val);

		if ((arg = getarg(args, 'l')) != NULL)
			grp->gr_name = arg->val;
	} else {
		if (a_name == NULL)	/* Required */
			cmderr(X_CMDERR, "group name required\n");
		else if (grp != NULL)	/* Exists */
			cmderr(X_EXISTS, "group name `%s' already exists\n", a_name->val);

		memset(members, 0, sizeof members);
		grp = &fakegroup;
		grp->gr_name = a_name->val;
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
				perror("-h file descriptor");
				return X_CMDERR;
			}
			line[b] = '\0';
			if ((p = strpbrk(line, " \t\r\n")) != NULL)
				*p = '\0';
			if (!*line)
				cmderr(X_CMDERR, "empty password read on file descriptor %d\n", fd);
			grp->gr_passwd = pw_pwcrypt(line);
		}
	}
	if ((mode == M_ADD && !addgrent(grp)) || (mode == M_UPDATE && !chggrent(a_name->val, grp))) {
		perror("group update");
		return X_NOUPDATE;
	}
	/* grp may have been invalidated */
	if ((grp = getgrnam(a_name->val)) == NULL)
		cmderr(X_NOTFOUND, "group disappeared during update\n");

	pw_log(cnf, mode, W_GROUP, "%s(%ld)", grp->gr_name, (long) grp->gr_gid);

	return X_ALLOK;
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

		if ((grp = getgrgid(gid)) != NULL && getarg(args, 'o') == NULL)
			cmderr(X_EXISTS, "gid `%ld' has already been allocated\n", (long) grp->gr_gid);
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
		setgrent();
		while ((grp = getgrent()) != NULL)
			if (grp->gr_gid >= (int) cnf->min_gid && grp->gr_gid <= (int) cnf->max_gid)
				bm_setbit(&bm, grp->gr_gid - cnf->min_gid);
		endgrent();

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
			cmderr(X_EXISTS, "unable to allocate a new gid - range fully used\n");
		bm_dealloc(&bm);
	}
	return gid;
}


static int
print_group(struct group * grp, int pretty)
{
	if (!pretty) {
		char            buf[_UC_MAXLINE];

		fmtgrent(buf, grp);
		fputs(buf, stdout);
	} else {
		int             i;

		printf("Group Name : %-10s   #%lu\n"
		       "   Members : ",
		       grp->gr_name, (long) grp->gr_gid);
		for (i = 0; i < _UC_MAXGROUPS && grp->gr_mem[i]; i++)
			printf("%s%s", i ? "," : "", grp->gr_mem[i]);
		fputs("\n\n", stdout);
	}
	return X_ALLOK;
}
