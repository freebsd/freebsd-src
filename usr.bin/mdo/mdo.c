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
#include <syslog.h>
#include <unistd.h>

static void
usage(void)
{
	fprintf(stderr, "usage: mdo [-u username] [-i] [--] [command [args]]\n");
	exit(EXIT_FAILURE);
}

static char*
join_strings(char **arr, int size, const char *delimiter)
{
	int total_length = 0, delimiter_length = strlen(delimiter);
	for (int i = 0; i < size; i++)
		total_length += strlen(arr[i]) + (i < size - 1 ? delimiter_length : 0);
	char *result = malloc(total_length + 1);
	if (!result)
		exit(1);
	result[0] = '\0';
	for (int i = 0; i < size; i++) {
		strcat(result, arr[i]);
		if (i < size - 1)
			strcat(result, delimiter);
	}
	return (result);
}

int
main(int argc, char **argv)
{
	struct passwd *pw = getpwuid(getuid());
	char original_username[33];
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

	openlog("mdo", LOG_PID | LOG_CONS, LOG_USER);
	strcpy(original_username, pw->pw_name);

	if ((pw = getpwnam(username)) == NULL) {
		if (strspn(username, "0123456789") == strlen(username)) {
			const char *errp = NULL;
			uid_t uid = strtonum(username, 0, UID_MAX, &errp);

			if (errp != NULL) {
				syslog(LOG_ERR, "Failed due to: %s", errp);
				err(EXIT_FAILURE, "%s", errp);
			}
			pw = getpwuid(uid);
		}
		if (pw == NULL) {
			syslog(LOG_AUTH | LOG_WARNING, "invalid username: %s", username);
			err(EXIT_FAILURE, "invalid username '%s'", username);
		}
	}

	if (!uidonly) {
		if (initgroups(pw->pw_name, pw->pw_gid) == -1) {
			syslog(LOG_AUTH | LOG_ERR, "USER: %s; failed to call initgroups: %d",
				   original_username,
				   EXIT_FAILURE);
			err(EXIT_FAILURE, "failed to call initgroups");
		}
		if (setgid(pw->pw_gid) == -1) {
			syslog(LOG_AUTH | LOG_ERR, "USER: %s; failed to call setgid: %d",
				   original_username,
				   EXIT_FAILURE);
			err(EXIT_FAILURE, "failed to call setgid");
		}
	}
	if (setuid(pw->pw_uid) == -1) {
		syslog(LOG_AUTH | LOG_ERR, "USER: %s; failed to call setuid: %d",
			   original_username,
			   EXIT_FAILURE);
		err(EXIT_FAILURE, "failed to call setuid");
	}
	if (*argv == NULL) {
		const char *sh = getenv("SHELL");
		if (sh == NULL)
			sh = _PATH_BSHELL;
		syslog(LOG_AUTH | LOG_INFO,
			   "USER: %s; COMMAND=%s",
			   original_username, sh);
		execlp(sh, sh, "-i", NULL);
	} else {
        char *command = join_strings(argv, argc, " ");
		syslog(LOG_AUTH | LOG_INFO, "USER: %s; COMMAND=%s",
			   original_username,
			   command);
        free(command);
		execvp(argv[0], argv);
	}
	err(EXIT_FAILURE, "exec failed");
}
