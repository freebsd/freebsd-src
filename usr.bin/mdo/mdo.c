/*-
 * Copyright(c) 2024 Baptiste Daroussin <bapt@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/limits.h>
#include <sys/ucred.h>

#include <err.h>
#include <paths.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
usage(void)
{
	fprintf(stderr, "usage: mdo [-u username] [-i] [--] [command [args]]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	struct passwd *pw;
	const char *username = "root";
	struct setcred wcred = SETCRED_INITIALIZER;
	u_int setcred_flags = 0;
	bool uidonly = false;
	int ch;

	while ((ch = getopt(argc, argv, "u:i")) != -1) {
		switch (ch) {
		case 'u':
			username = optarg;
			break;
		case 'i':
			uidonly = true;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if ((pw = getpwnam(username)) == NULL) {
		if (strspn(username, "0123456789") == strlen(username)) {
			const char *errp = NULL;
			uid_t uid = strtonum(username, 0, UID_MAX, &errp);
			if (errp != NULL)
				err(EXIT_FAILURE, "invalid user ID '%s'",
				    username);
			pw = getpwuid(uid);
		}
		if (pw == NULL)
			err(EXIT_FAILURE, "invalid username '%s'", username);
	}

	wcred.sc_uid = wcred.sc_ruid = wcred.sc_svuid = pw->pw_uid;
	setcred_flags |= SETCREDF_UID | SETCREDF_RUID | SETCREDF_SVUID;

	if (!uidonly) {
		/*
		 * If there are too many groups specified for some UID, setting
		 * the groups will fail.  We preserve this condition by
		 * allocating one more group slot than allowed, as
		 * getgrouplist() itself is just some getter function and thus
		 * doesn't (and shouldn't) check the limit, and to allow
		 * setcred() to actually check for overflow.
		 */
		const long ngroups_alloc = sysconf(_SC_NGROUPS_MAX) + 2;
		gid_t *const groups = malloc(sizeof(*groups) * ngroups_alloc);
		int ngroups = ngroups_alloc;

		if (groups == NULL)
			err(EXIT_FAILURE, "cannot allocate memory for groups");

		getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups);

		wcred.sc_gid = wcred.sc_rgid = wcred.sc_svgid = pw->pw_gid;
		wcred.sc_supp_groups = groups + 1;
		wcred.sc_supp_groups_nb = ngroups - 1;
		setcred_flags |= SETCREDF_GID | SETCREDF_RGID | SETCREDF_SVGID |
		    SETCREDF_SUPP_GROUPS;
	}

	if (setcred(setcred_flags, &wcred, sizeof(wcred)) != 0)
		err(EXIT_FAILURE, "calling setcred() failed");

	if (*argv == NULL) {
		const char *sh = getenv("SHELL");
		if (sh == NULL)
			sh = _PATH_BSHELL;
		execlp(sh, sh, "-i", NULL);
	} else {
		execvp(argv[0], argv);
	}
	err(EXIT_FAILURE, "exec failed");
}
