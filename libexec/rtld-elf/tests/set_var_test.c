/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Alex S <iwtcex@gmail.com>
 */

#include <atf-c.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <link.h>
#include <stdio.h>

ATF_TC_WITHOUT_HEAD(set_var_library_path_fds);
ATF_TC_BODY(set_var_library_path_fds, tc)
{
	void *handle;
	char *pathfds;
	int testdir;

	handle = dlopen("libpythagoras.so.0", RTLD_LAZY);
	ATF_REQUIRE(handle == NULL);

	testdir = open(atf_tc_get_config_var(tc, "srcdir"),
	    O_RDONLY | O_DIRECTORY);
	ATF_REQUIRE(testdir >= 0);

	ATF_REQUIRE(asprintf(&pathfds, "%d", testdir) > 0);
	ATF_REQUIRE(rtld_set_var("LIBRARY_PATH_FDS", pathfds) == 0);

	handle = dlopen("libpythagoras.so.0", RTLD_LAZY);
	ATF_REQUIRE(handle != NULL);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, set_var_library_path_fds);
	return atf_no_error();
}
