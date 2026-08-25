/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Alex S <iwtcex@gmail.com>
 * Copyright 2026 The FreeBSD Foundation
 *
 * Portions of this software were developed by
 * Konstantin Belousov <kib@FreeBSD.org> under sponsorship from
 * the FreeBSD Foundation.
 */

#include <atf-c.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <link.h>
#include <stdio.h>

ATF_TC_WITHOUT_HEAD(dlopen_hash);
ATF_TC_BODY(dlopen_hash, tc)
{
	void *handle;
	char *pathfds;
	char *name;
	int testdir;

	handle = dlopen("libpythagoras.so.0", RTLD_LAZY);
	ATF_REQUIRE(handle == NULL);

	testdir = open(atf_tc_get_config_var(tc, "srcdir"),
	    O_RDONLY | O_DIRECTORY);
	ATF_REQUIRE(testdir >= 0);

	ATF_REQUIRE(asprintf(&pathfds, "%d", testdir) > 0);
	ATF_REQUIRE(rtld_set_var("LIBRARY_PATH_FDS", pathfds) == 0);

	ATF_REQUIRE(asprintf(&name, "#%d/libpythagoras.so.0", testdir) > 0);
	handle = dlopen(name,  RTLD_LAZY);
	ATF_REQUIRE(handle != NULL);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, dlopen_hash);
	return atf_no_error();
}
