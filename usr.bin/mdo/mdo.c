/*-
 * Copyright(c) 2024 Baptiste Daroussin <bapt@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/limits.h>

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
				err(EXIT_FAILURE, "%s", errp);
			pw = getpwuid(uid);
		}
		if (pw == NULL)
			err(EXIT_FAILURE, "invalid username '%s'", username);
	}
	if (!uidonly) {
		if (initgroups(pw->pw_name, pw->pw_gid) == -1)
			err(EXIT_FAILURE, "failed to call initgroups");
		if (setgid(pw->pw_gid) == -1)
			err(EXIT_FAILURE, "failed to call setgid");
	}
	if (setuid(pw->pw_uid) == -1)
		err(EXIT_FAILURE, "failed to call setuid");
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
