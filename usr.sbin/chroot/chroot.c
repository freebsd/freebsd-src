/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1988, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/types.h>
#include <sys/procctl.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void) __dead2;

static gid_t
resolve_group(const char *group)
{
	char *endp;
	struct group *gp;
	unsigned long gid;

	gp = getgrnam(group);
	if (gp != NULL)
		return (gp->gr_gid);

	/*
	 * Numeric IDs don't need a trip through the database to check them,
	 * POSIX seems to think we should generally accept a numeric ID as long
	 * as it's within the valid range.
	 */
	errno = 0;
	gid = strtoul(group, &endp, 0);
	if (errno == 0 && *endp == '\0' && gid <= GID_MAX)
		return (gid);

	errx(1, "no such group '%s'", group);
}

static uid_t
resolve_user(const char *user)
{
	char *endp;
	struct passwd *pw;
	unsigned long uid;

	pw = getpwnam(user);
	if (pw != NULL)
		return (pw->pw_uid);

	errno = 0;
	uid = strtoul(user, &endp, 0);
	if (errno == 0 && *endp == '\0' && uid <= UID_MAX)
		return (uid);

	errx(1, "no such user '%s'", user);
}

int
main(int argc, char *argv[])
{
	const char	*group, *p, *shell, *user;
	char		*grouplist;
	long		ngroups_max;
	gid_t		gid, *gidlist;
	uid_t		uid;
	int		arg, ch, error, gids;
	bool		nonprivileged;

	gid = 0;
	uid = 0;
	gids = 0;
	user = group = grouplist = NULL;
	gidlist = NULL;
	nonprivileged = false;
	while ((ch = getopt(argc, argv, "G:g:u:n")) != -1) {
		switch(ch) {
		case 'u':
			user = optarg;
			if (*user == '\0')
				usage();
			break;
		case 'g':
			group = optarg;
			if (*group == '\0')
				usage();
			break;
		case 'G':
			grouplist = optarg;

			/*
			 * XXX Why not allow us to drop all of our supplementary
			 * groups?
			 */
			if (*grouplist == '\0')
				usage();
			break;
		case 'n':
			nonprivileged = true;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	if (group != NULL)
		gid = resolve_group(group);

	if (grouplist != NULL) {
		ngroups_max = sysconf(_SC_NGROUPS_MAX);
		if ((gidlist = malloc(sizeof(gid_t) * ngroups_max)) == NULL)
			err(1, "malloc");
		for (gids = 0; (p = strsep(&grouplist, ",")) != NULL &&
		    gids < ngroups_max; ) {
			if (*p == '\0')
				continue;

			gidlist[gids++] = resolve_group(p);
		}
		if (p != NULL && gids == ngroups_max)
			errx(1, "too many supplementary groups provided");
	}

	if (user != NULL)
		uid = resolve_user(user);

	if (nonprivileged) {
		arg = PROC_NO_NEW_PRIVS_ENABLE;
		error = procctl(P_PID, getpid(), PROC_NO_NEW_PRIVS_CTL, &arg);
		if (error != 0)
			err(1, "procctl");
	}

	if (chdir(argv[0]) == -1)
		err(1, "%s", argv[0]);
	if (chroot(".") == -1) {
		if (errno == EPERM && !nonprivileged && geteuid() != 0)
			errx(1, "unprivileged use requires -n");
		err(1, "%s", argv[0]);
	}

	if (gidlist != NULL && setgroups(gids, gidlist) == -1)
		err(1, "setgroups");
	if (group && setgid(gid) == -1)
		err(1, "setgid");
	if (user && setuid(uid) == -1)
		err(1, "setuid");

	if (argv[1]) {
		execvp(argv[1], &argv[1]);
		err(1, "%s", argv[1]);
	}

	if (!(shell = getenv("SHELL")))
		shell = _PATH_BSHELL;
	execlp(shell, shell, "-i", (char *)NULL);
	err(1, "%s", shell);
	/* NOTREACHED */
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: chroot [-g group] [-G group,group,...] "
	    "[-u user] [-n] newroot [command]\n");
	exit(1);
}
