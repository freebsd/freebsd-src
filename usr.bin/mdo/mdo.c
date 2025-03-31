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
#include <syslog.h>
#include <unistd.h>

static void
usage(void)
{
	fprintf(stderr, "usage: mdo [-u username] [-i] [--] [command [args]]\n");
	exit(EXIT_FAILURE);
}

static char *join_strings(char **arr, size_t nlen, const char *delim)
{
	char *buf;
	size_t dlen, tlen, i;

	tlen = 0;
	dlen = strlen(delim);
	for (i = 0; i < nlen; i++)
		tlen += strlen(arr[i]) + dlen;

	if ((buf = malloc(tlen)) == NULL)
		err(1, "malloc()");

	*buf = '\0';
	strcpy(buf, arr[0]);
	for (i = 1; i < nlen; i++) {
		strcpy(buf + strlen(buf), delim);
		strcpy(buf + strlen(buf), arr[i]);
	}
	return buf;
}

int
main(int argc, char **argv)
{
	struct passwd *pw = getpwuid(getuid());
	char original_username[33];
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

	openlog("mdo", LOG_PID | LOG_CONS, LOG_USER);
	strcpy(original_username, pw->pw_name);

	if ((pw = getpwnam(username)) == NULL) {
		if (strspn(username, "0123456789") == strlen(username)) {
			const char *errp = NULL;
			uid_t uid = strtonum(username, 0, UID_MAX, &errp);
			if (errp != NULL) {
				syslog(LOG_ERR, "Failed to login: %s", username);
				err(EXIT_FAILURE, "invalid user ID '%s'", username);
			}
			pw = getpwuid(uid);
		}
		if (pw == NULL) {
			syslog(LOG_ERR, "Failed to login: %s", username);
			err(EXIT_FAILURE, "invalid username '%s'", username);
		}
	}

	wcred.sc_uid = wcred.sc_ruid = wcred.sc_svuid = pw->pw_uid;
	setcred_flags |= SETCREDF_UID | SETCREDF_RUID | SETCREDF_SVUID;

	if (!uidonly) {
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

	if (setcred(setcred_flags, &wcred, sizeof(wcred)) != 0) {
		syslog(LOG_ERR, "calling setcred() failed");
		err(EXIT_FAILURE, "calling setcred() failed");
	}

	if (*argv == NULL) {
		const char *sh = getenv("SHELL");
		if (sh == NULL)
			sh = _PATH_BSHELL;
		execlp(sh, sh, "-i", NULL);
		syslog(LOG_AUTH | LOG_INFO, "USER: %s; COMMAND=%s",
		    original_username, sh);
	} else {
		char *command = join_strings(argv, argc, " ");
		syslog(LOG_AUTH | LOG_INFO, "USER: %s; COMMAND=%s",
		    original_username, command);
		execvp(argv[0], argv);
	}
	err(EXIT_FAILURE, "exec failed");
}

