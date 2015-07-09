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
  "$FreeBSD$";
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <termios.h>
#include <stdbool.h>
#include <unistd.h>
#include <grp.h>
#include <libutil.h>

#include "pw.h"
#include "bitmap.h"


static struct passwd *lookup_pwent(const char *user);
static void	delete_members(char ***members, int *grmembers, int *i,
    struct carg *arg, struct group *grp);
static int	print_group(struct group * grp);
static gid_t    gr_gidpolicy(struct userconf * cnf, long id);

static void
set_passwd(struct group *grp, bool update)
{
	int		 b;
	int		 istty;
	struct termios	 t, n;
	char		*p, line[256];

	if (conf.fd == '-') {
		grp->gr_passwd = "*";	/* No access */
		return;
	}
	
	if ((istty = isatty(conf.fd))) {
		n = t;
		/* Disable echo */
		n.c_lflag &= ~(ECHO);
		tcsetattr(conf.fd, TCSANOW, &n);
		printf("%sassword for group %s:", update ? "New p" : "P",
		    grp->gr_name);
		fflush(stdout);
	}
	b = read(conf.fd, line, sizeof(line) - 1);
	if (istty) {	/* Restore state */
		tcsetattr(conf.fd, TCSANOW, &t);
		fputc('\n', stdout);
		fflush(stdout);
	}
	if (b < 0)
		err(EX_OSERR, "-h file descriptor");
	line[b] = '\0';
	if ((p = strpbrk(line, " \t\r\n")) != NULL)
		*p = '\0';
	if (!*line)
		errx(EX_DATAERR, "empty password read on file descriptor %d",
		    conf.fd);
	if (conf.precrypted) {
		if (strchr(line, ':') != 0)
			errx(EX_DATAERR, "wrong encrypted passwrd");
		grp->gr_passwd = line;
	} else
		grp->gr_passwd = pw_pwcrypt(line);
}

int
pw_group(int mode, char *name, long id, struct cargs * args)
{
	int		rc;
	struct carg    *arg;
	struct group   *grp = NULL;
	int	        grmembers = 0;
	char           **members = NULL;
	struct userconf	*cnf = conf.userconf;

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
		gid_t next = gr_gidpolicy(cnf, id);
		if (getarg(args, 'q'))
			return next;
		printf("%u\n", next);
		return EXIT_SUCCESS;
	}

	if (mode == M_PRINT && getarg(args, 'a')) {
		SETGRENT();
		while ((grp = GETGRENT()) != NULL)
			print_group(grp);
		ENDGRENT();
		return EXIT_SUCCESS;
	}
	if (id < 0 && name == NULL)
		errx(EX_DATAERR, "group name or id required");

	grp = (name != NULL) ? GETGRNAM(name) : GETGRGID(id);

	if (mode == M_UPDATE || mode == M_DELETE || mode == M_PRINT) {
		if (name == NULL && grp == NULL)	/* Try harder */
			grp = GETGRGID(id);

		if (grp == NULL) {
			if (mode == M_PRINT && getarg(args, 'F')) {
				char	*fmems[1];
				fmems[0] = NULL;
				fakegroup.gr_name = name ? name : "nogroup";
				fakegroup.gr_gid = (gid_t) id;
				fakegroup.gr_mem = fmems;
				return print_group(&fakegroup);
			}
			if (name == NULL)
				errx(EX_DATAERR, "unknown group `%s'", name);
			else
				errx(EX_DATAERR, "unknown group `%ld'", id);
		}
		if (name == NULL)	/* Needed later */
			name = grp->gr_name;

		/*
		 * Handle deletions now
		 */
		if (mode == M_DELETE) {
			rc = delgrent(grp);
			if (rc == -1)
				err(EX_IOERR, "group '%s' not available (NIS?)",
				    name);
			else if (rc != 0) {
				err(EX_IOERR, "group update");
			}
			pw_log(cnf, mode, W_GROUP, "%s(%ld) removed", name, id);
			return EXIT_SUCCESS;
		} else if (mode == M_PRINT)
			return print_group(grp);

		if (id > 0)
			grp->gr_gid = (gid_t) id;

		if (conf.newname != NULL)
			grp->gr_name = pw_checkname(conf.newname, 0);
	} else {
		if (name == NULL)	/* Required */
			errx(EX_DATAERR, "group name required");
		else if (grp != NULL)	/* Exists */
			errx(EX_DATAERR, "group name `%s' already exists", name);

		extendarray(&members, &grmembers, 200);
		members[0] = NULL;
		grp = &fakegroup;
		grp->gr_name = pw_checkname(name, 0);
		grp->gr_passwd = "*";
		grp->gr_gid = gr_gidpolicy(cnf, id);
		grp->gr_mem = members;
	}

	/*
	 * This allows us to set a group password Group passwords is an
	 * antique idea, rarely used and insecure (no secure database) Should
	 * be discouraged, but it is apparently still supported by some
	 * software.
	 */

	if (conf.which == W_GROUP && conf.fd != -1)
		set_passwd(grp, mode == M_UPDATE);

	if (((arg = getarg(args, 'M')) != NULL ||
	    (arg = getarg(args, 'd')) != NULL ||
	    (arg = getarg(args, 'm')) != NULL) && arg->val) {
		int	i = 0;
		char   *p;
		struct passwd	*pwd;

		/* Make sure this is not stay NULL with -M "" */
		extendarray(&members, &grmembers, 200);
		if (arg->ch == 'd')
			delete_members(&members, &grmembers, &i, arg, grp);
		else if (arg->ch == 'm') {
			int	k = 0;

			if (grp->gr_mem != NULL) {
				while (grp->gr_mem[k] != NULL) {
					if (extendarray(&members, &grmembers, i + 2) != -1)
						members[i++] = grp->gr_mem[k];
					k++;
				}
			}
		}

		if (arg->ch != 'd')
			for (p = strtok(arg->val, ", \t"); p != NULL; p = strtok(NULL, ", \t")) {
				int	j;

				/*
				 * Check for duplicates
				 */
				pwd = lookup_pwent(p);
				for (j = 0; j < i && strcmp(members[j], pwd->pw_name) != 0; j++)
					;
				if (j == i && extendarray(&members, &grmembers, i + 2) != -1)
					members[i++] = newstr(pwd->pw_name);
			}
		while (i < grmembers)
			members[i++] = NULL;
		grp->gr_mem = members;
	}

	if (conf.dryrun)
		return print_group(grp);

	if (mode == M_ADD && (rc = addgrent(grp)) != 0) {
		if (rc == -1)
			errx(EX_IOERR, "group '%s' already exists",
			    grp->gr_name);
		else
			err(EX_IOERR, "group update");
	} else if (mode == M_UPDATE && (rc = chggrent(name, grp)) != 0) {
		if (rc == -1)
			errx(EX_IOERR, "group '%s' not available (NIS?)",
			    grp->gr_name);
		else
			err(EX_IOERR, "group update");
	}

	if (conf.newname != NULL)
		name = conf.newname;
	/* grp may have been invalidated */
	if ((grp = GETGRNAM(name)) == NULL)
		errx(EX_SOFTWARE, "group disappeared during update");

	pw_log(cnf, mode, W_GROUP, "%s(%u)", grp->gr_name, grp->gr_gid);

	free(members);

	return EXIT_SUCCESS;
}


/*
 * Lookup a passwd entry using a name or UID.
 */
static struct passwd *
lookup_pwent(const char *user)
{
	struct passwd *pwd;

	if ((pwd = GETPWNAM(user)) == NULL &&
	    (!isdigit((unsigned char)*user) ||
	    (pwd = getpwuid((uid_t) atoi(user))) == NULL))
		errx(EX_NOUSER, "user `%s' does not exist", user);

	return (pwd);
}


/*
 * Delete requested members from a group.
 */
static void
delete_members(char ***members, int *grmembers, int *i, struct carg *arg,
    struct group *grp)
{
	bool matchFound;
	char *user;
	char *valueCopy;
	char *valuePtr;
	int k;
	struct passwd *pwd;

	if (grp->gr_mem == NULL)
		return;

	k = 0;
	while (grp->gr_mem[k] != NULL) {
		matchFound = false;
		if ((valueCopy = strdup(arg->val)) == NULL)
			errx(EX_UNAVAILABLE, "out of memory");
		valuePtr = valueCopy;
		while ((user = strsep(&valuePtr, ", \t")) != NULL) {
			pwd = lookup_pwent(user);
			if (strcmp(grp->gr_mem[k], pwd->pw_name) == 0) {
				matchFound = true;
				break;
			}
		}
		free(valueCopy);

		if (!matchFound &&
		    extendarray(members, grmembers, *i + 2) != -1)
			(*members)[(*i)++] = grp->gr_mem[k];

		k++;
	}

	return;
}


static          gid_t
gr_gidpolicy(struct userconf * cnf, long id)
{
	struct group   *grp;
	gid_t           gid = (gid_t) - 1;

	/*
	 * Check the given gid, if any
	 */
	if (id > 0) {
		gid = (gid_t) id;

		if ((grp = GETGRGID(gid)) != NULL && conf.checkduplicate)
			errx(EX_DATAERR, "gid `%u' has already been allocated", grp->gr_gid);
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
print_group(struct group * grp)
{
	if (!conf.pretty) {
		char           *buf = NULL;

		buf = gr_make(grp);
		printf("%s\n", buf);
		free(buf);
	} else {
		int             i;

		printf("Group Name: %-15s   #%lu\n"
		       "   Members: ",
		       grp->gr_name, (long) grp->gr_gid);
		if (grp->gr_mem != NULL) {
			for (i = 0; grp->gr_mem[i]; i++)
				printf("%s%s", i ? "," : "", grp->gr_mem[i]);
		}
		fputs("\n\n", stdout);
	}
	return EXIT_SUCCESS;
}
