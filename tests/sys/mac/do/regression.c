/*
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * This software was developed by Mark Johnston under sponsorship from
 * the FreeBSD Foundation.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/sysctl.h>
#include <sys/ucred.h>

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <atf-c.h>

/*
 * Regression test for a bug which erroneously allowed the setcred() call below.
 */
ATF_TC(empty_supplementary_group_list);
ATF_TC_HEAD(empty_supplementary_group_list, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(empty_supplementary_group_list, tc)
{
	struct setcred cred = SETCRED_INITIALIZER;
	struct passwd *passwd;
	const char *user;
	char path[PATH_MAX], *progname, *rule;
	int flags;

	if (!atf_tc_has_config_var(tc, "unprivileged_user"))
		atf_tc_skip("unprivileged_user not set");

	user = atf_tc_get_config_var(tc, "unprivileged_user");
	passwd = getpwnam(user);
	ATF_REQUIRE(passwd != NULL);
	ATF_REQUIRE_MSG(passwd->pw_uid != 0,
	    "unprivileged user must not be root");
	ATF_REQUIRE_MSG(passwd->pw_gid != 0,
	    "unprivileged user group must not be wheel");

	(void)asprintf(&rule, "uid=%d>uid=%d;gid=0>uid=0",
	    passwd->pw_uid, passwd->pw_uid + 1);
	ATF_REQUIRE(sysctlbyname("security.mac.do.rules",
	    NULL, NULL, rule, strlen(rule)) == 0);

	progname = basename(strdup(getprogname()));
	(void)snprintf(path, sizeof(path), "%s/%s",
	    atf_tc_get_config_var(tc, "srcdir"), progname);
	ATF_REQUIRE(sysctlbyname("security.mac.do.exec_paths",
	    NULL, NULL, path, strlen(path)) == 0);

	ATF_REQUIRE(setgroups(0, NULL) == 0);
	ATF_REQUIRE(setgid(passwd->pw_gid) == 0);
	ATF_REQUIRE(setuid(passwd->pw_uid) == 0);

	/*
	 * Request the UID transition permitted by the first rule while also
	 * setting all primary GIDs to 0.  MAC/do must reject this because GID 0
	 * is not a primary GID of the current credential.
	 */
	cred.sc_uid = cred.sc_ruid = cred.sc_svuid = passwd->pw_uid + 1;
	cred.sc_gid = cred.sc_rgid = cred.sc_svgid = 0;
	flags = SETCREDF_UID | SETCREDF_RUID | SETCREDF_SVUID |
	    SETCREDF_GID | SETCREDF_RGID | SETCREDF_SVGID;
	ATF_REQUIRE_ERRNO(EPERM,
	    setcred(flags, &cred, sizeof(cred)) == -1);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, empty_supplementary_group_list);

	return (atf_no_error());
}
