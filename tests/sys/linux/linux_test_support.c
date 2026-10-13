/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Vladimir Grebenshchikov <vova@zote.me>
 */

#include <sys/sysctl.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <atf-c.h>
#include "linux_test_support.h"

void
linux_test_helper_path(const atf_tc_t *tc, const char *name, char *path,
    size_t pathlen)
{
	const char *srcdir;
	size_t len;

	len = 0;
	if (sysctlbyname("compat.linux.emul_path", NULL, &len, NULL, 0) != 0)
		atf_tc_skip("Linux ABI is not available: %s", strerror(errno));
	srcdir = atf_tc_get_config_var(tc, "srcdir");
	ATF_REQUIRE_MSG(snprintf(path, pathlen, "%s/%s", srcdir, name) <
	    (int)pathlen, "test helper path is too long");
	ATF_REQUIRE_MSG(access(path, X_OK) == 0,
	    "Linux test helper is not executable (%s): %s", path,
	    strerror(errno));
}

void
linux_test_run(const atf_tc_t *tc, const char *name)
{
	char helper[PATH_MAX];
	int status;
	pid_t child;

	linux_test_helper_path(tc, name, helper, sizeof(helper));
	child = fork();
	ATF_REQUIRE_MSG(child >= 0, "fork failed: %s", strerror(errno));
	if (child == 0) {
		execl(helper, name, (char *)NULL);
		_exit(127);
	}
	ATF_REQUIRE_MSG(waitpid(child, &status, 0) == child,
	    "waitpid failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(WIFEXITED(status), "Linux helper died from signal %d",
	    WTERMSIG(status));
	ATF_CHECK_MSG(WEXITSTATUS(status) == 0,
	    "Linux helper exited with status %d", WEXITSTATUS(status));
}
