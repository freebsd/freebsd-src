/*
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
static const char copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)chroot.c	8.1 (Berkeley) 6/9/93";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <grp.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void);

char	*user;		/* user to switch to before running program */
char	*group;		/* group to switch to ... */
char	*grouplist;	/* group list to switch to ... */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct group	*gp;
	struct passwd	*pw;
	char		*endp, *p;
	const char	*shell;
	gid_t		gid, gidlist[NGROUPS_MAX];
	uid_t		uid;
	int		ch, gids;

	gid = 0;
	uid = 0;
	while ((ch = getopt(argc, argv, "G:g:u:")) != -1) {
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
			if (*grouplist == '\0')
				usage();
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

	if (group != NULL) {
		if (isdigit((unsigned char)*group)) {
			gid = (gid_t)strtoul(group, &endp, 0);
			if (*endp != '\0')
				goto getgroup;
		} else {
 getgroup:
			if ((gp = getgrnam(group)) != NULL)
				gid = gp->gr_gid;
			else
				errx(1, "no such group `%s'", group);
		}
	}

	for (gids = 0;
	    (p = strsep(&grouplist, ",")) != NULL && gids < NGROUPS_MAX; ) {
		if (*p == '\0')
			continue;

		if (isdigit((unsigned char)*p)) {
			gidlist[gids] = (gid_t)strtoul(p, &endp, 0);
			if (*endp != '\0')
				goto getglist;
		} else {
 getglist:
			if ((gp = getgrnam(p)) != NULL)
				gidlist[gids] = gp->gr_gid;
			else
				errx(1, "no such group `%s'", p);
		}
		gids++;
	}
	if (p != NULL && gids == NGROUPS_MAX)
		errx(1, "too many supplementary groups provided");

	if (user != NULL) {
		if (isdigit((unsigned char)*user)) {
			uid = (uid_t)strtoul(user, &endp, 0);
			if (*endp != '\0')
				goto getuser;
		} else {
 getuser:
			if ((pw = getpwnam(user)) != NULL)
				uid = pw->pw_uid;
			else
				errx(1, "no such user `%s'", user);
		}
	}

	if (chdir(argv[0]) == -1 || chroot(".") == -1)
		err(1, "%s", argv[0]);

	if (gids && setgroups(gids, gidlist) == -1)
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
usage()
{
	(void)fprintf(stderr, "usage: chroot [-g group] [-G group,group,...] "
	    "[-u user] newroot [command]\n");
	exit(1);
}
