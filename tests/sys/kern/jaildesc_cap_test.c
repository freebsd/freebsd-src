/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kory Heard <koryheard@icloud.com>
 */

/*
 * Test Capsicum capability rights for jail descriptors.
 * Tests CAP_JAIL_ATTACH, CAP_JAIL_REMOVE, and CAP_JAIL_SET rights.
 */

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/jail.h>
#include <sys/wait.h>

#include <errno.h>
#include <jail.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#include "freebsd_test_suite/macros.h"

/*
 * Create a jail and return its descriptor using JAIL_GET_DESC flag.
 */
static int
create_jail_with_desc(const char *name)
{
	int desc_fd = -1;
	int jid;

	jid = jail_setv(JAIL_CREATE | JAIL_GET_DESC,
	    "name", name,
	    "path", "/",
	    "persist", NULL,
	    "desc", &desc_fd,
	    NULL);
	if (jid < 0)
		return (-1);

	return (desc_fd);
}

/*
 * Remove a jail by name.
 */
static void
remove_jail_by_name(const char *name)
{
	int jid;

	jid = jail_getid(name);
	if (jid > 0)
		jail_remove(jid);
}

/*
 * Modify a jail using its descriptor (JAIL_USE_DESC).
 * Sets allow.raw_sockets as a harmless modification.
 */
static int
modify_jail_via_desc(int fd)
{

	return (jail_setv(JAIL_UPDATE | JAIL_USE_DESC,
	    "desc", &fd,
	    "allow.raw_sockets", "true",
	    NULL));
}

/*
 * Test CAP_JAIL_SET: verify it permits jail_set and denies without it.
 */
ATF_TC_WITH_CLEANUP(cap_jail_set);
ATF_TC_HEAD(cap_jail_set, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr",
	    "Test CAP_JAIL_SET permits jail_set and denies without it");
}
ATF_TC_BODY(cap_jail_set, tc)
{
	cap_rights_t rights;
	int fd, error;

	ATF_REQUIRE_FEATURE("security_capabilities");

	remove_jail_by_name("cap_set_test");

	fd = create_jail_with_desc("cap_set_test");
	ATF_REQUIRE_MSG(fd >= 0, "create_jail_with_desc failed: %s",
	    strerror(errno));

	/* Limit to CAP_JAIL_SET */
	cap_rights_init(&rights, CAP_JAIL_SET);
	error = cap_rights_limit(fd, &rights);
	ATF_REQUIRE_MSG(error == 0, "cap_rights_limit failed: %s",
	    strerror(errno));

	/* jail_set with JAIL_USE_DESC should succeed */
	error = modify_jail_via_desc(fd);
	ATF_REQUIRE_MSG(error >= 0, "jail_set with CAP_JAIL_SET failed: %s",
	    strerror(errno));

	/* Now limit to empty rights */
	cap_rights_init(&rights);
	error = cap_rights_limit(fd, &rights);
	ATF_REQUIRE_MSG(error == 0, "cap_rights_limit failed: %s",
	    strerror(errno));

	/* jail_set should fail with ENOTCAPABLE */
	error = modify_jail_via_desc(fd);
	ATF_REQUIRE_MSG(error == -1 && errno == ENOTCAPABLE,
	    "jail_set without CAP_JAIL_SET should fail with ENOTCAPABLE, got %s",
	    error >= 0 ? "success" : strerror(errno));

	close(fd);
}
ATF_TC_CLEANUP(cap_jail_set, tc)
{
	remove_jail_by_name("cap_set_test");
}

/*
 * Test CAP_JAIL_ATTACH: verify it permits attach and denies remove.
 */
ATF_TC_WITH_CLEANUP(cap_jail_attach);
ATF_TC_HEAD(cap_jail_attach, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr",
	    "Test CAP_JAIL_ATTACH permits attach and denies remove");
}
ATF_TC_BODY(cap_jail_attach, tc)
{
	cap_rights_t rights;
	int fd, error;

	ATF_REQUIRE_FEATURE("security_capabilities");

	remove_jail_by_name("cap_attach_test");

	fd = create_jail_with_desc("cap_attach_test");
	ATF_REQUIRE_MSG(fd >= 0, "create_jail_with_desc failed: %s",
	    strerror(errno));

	/* Limit to CAP_JAIL_ATTACH */
	cap_rights_init(&rights, CAP_JAIL_ATTACH);
	error = cap_rights_limit(fd, &rights);
	ATF_REQUIRE_MSG(error == 0, "cap_rights_limit failed: %s",
	    strerror(errno));

	/* jail_remove_jd should fail with ENOTCAPABLE */
	error = jail_remove_jd(fd);
	ATF_REQUIRE_MSG(error == -1 && errno == ENOTCAPABLE,
	    "jail_remove_jd should fail with ENOTCAPABLE, got %s",
	    error == 0 ? "success" : strerror(errno));

	close(fd);
}
ATF_TC_CLEANUP(cap_jail_attach, tc)
{
	remove_jail_by_name("cap_attach_test");
}

/*
 * Test CAP_JAIL_REMOVE: verify it permits remove and denies attach.
 */
ATF_TC_WITH_CLEANUP(cap_jail_remove);
ATF_TC_HEAD(cap_jail_remove, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr",
	    "Test CAP_JAIL_REMOVE permits remove and denies attach");
}
ATF_TC_BODY(cap_jail_remove, tc)
{
	cap_rights_t rights;
	int fd, fd_for_attach, error, status;
	pid_t pid;

	ATF_REQUIRE_FEATURE("security_capabilities");

	remove_jail_by_name("cap_remove_test");

	fd = create_jail_with_desc("cap_remove_test");
	ATF_REQUIRE_MSG(fd >= 0, "create_jail_with_desc failed: %s",
	    strerror(errno));

	/* Create separate fd for attach test (before limiting rights) */
	fd_for_attach = dup(fd);
	ATF_REQUIRE(fd_for_attach >= 0);

	/* Limit fd to CAP_JAIL_REMOVE */
	cap_rights_init(&rights, CAP_JAIL_REMOVE);
	error = cap_rights_limit(fd, &rights);
	ATF_REQUIRE_MSG(error == 0, "cap_rights_limit failed: %s",
	    strerror(errno));

	/* Limit fd_for_attach to CAP_JAIL_REMOVE (wrong right for attach) */
	error = cap_rights_limit(fd_for_attach, &rights);
	ATF_REQUIRE_MSG(error == 0, "cap_rights_limit failed: %s",
	    strerror(errno));

	/* jail_attach_jd should fail with ENOTCAPABLE - test in child */
	pid = fork();
	ATF_REQUIRE(pid >= 0);

	if (pid == 0) {
		error = jail_attach_jd(fd_for_attach);
		if (error == -1 && errno == ENOTCAPABLE)
			_exit(0);
		_exit(1);
	}

	waitpid(pid, &status, 0);
	ATF_REQUIRE_MSG(WIFEXITED(status) && WEXITSTATUS(status) == 0,
	    "jail_attach_jd should fail with ENOTCAPABLE");

	/* jail_remove_jd should succeed */
	error = jail_remove_jd(fd);
	ATF_REQUIRE_MSG(error == 0, "jail_remove_jd failed: %s",
	    strerror(errno));

	close(fd_for_attach);
	close(fd);
}
ATF_TC_CLEANUP(cap_jail_remove, tc)
{
	remove_jail_by_name("cap_remove_test");
}

/*
 * Test combined CAP_JAIL_ATTACH | CAP_JAIL_REMOVE allows both operations.
 */
ATF_TC_WITH_CLEANUP(cap_jail_both);
ATF_TC_HEAD(cap_jail_both, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr",
	    "Test combined rights permit both attach and remove");
}
ATF_TC_BODY(cap_jail_both, tc)
{
	cap_rights_t rights;
	int fd, error, status;
	pid_t pid;

	ATF_REQUIRE_FEATURE("security_capabilities");

	remove_jail_by_name("cap_both_test");

	fd = create_jail_with_desc("cap_both_test");
	ATF_REQUIRE_MSG(fd >= 0, "create_jail_with_desc failed: %s",
	    strerror(errno));

	/* Limit to both rights */
	cap_rights_init(&rights, CAP_JAIL_ATTACH, CAP_JAIL_REMOVE);
	error = cap_rights_limit(fd, &rights);
	ATF_REQUIRE_MSG(error == 0, "cap_rights_limit failed: %s",
	    strerror(errno));

	/* jail_attach_jd should succeed - test in child */
	pid = fork();
	ATF_REQUIRE(pid >= 0);

	if (pid == 0) {
		error = jail_attach_jd(fd);
		_exit(error == 0 ? 0 : 1);
	}

	waitpid(pid, &status, 0);
	ATF_REQUIRE_MSG(WIFEXITED(status) && WEXITSTATUS(status) == 0,
	    "jail_attach_jd should succeed with combined rights");

	/* jail_remove_jd should succeed */
	error = jail_remove_jd(fd);
	ATF_REQUIRE_MSG(error == 0, "jail_remove_jd failed: %s",
	    strerror(errno));

	close(fd);
}
ATF_TC_CLEANUP(cap_jail_both, tc)
{
	remove_jail_by_name("cap_both_test");
}

/*
 * Test that an unlimited jail descriptor allows all operations.
 */
ATF_TC_WITH_CLEANUP(cap_jail_unlimited);
ATF_TC_HEAD(cap_jail_unlimited, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr",
	    "Test unlimited descriptor permits all operations");
}
ATF_TC_BODY(cap_jail_unlimited, tc)
{
	int fd, error, status;
	pid_t pid;

	ATF_REQUIRE_FEATURE("security_capabilities");

	remove_jail_by_name("cap_unlimited_test");

	fd = create_jail_with_desc("cap_unlimited_test");
	ATF_REQUIRE_MSG(fd >= 0, "create_jail_with_desc failed: %s",
	    strerror(errno));

	/* jail_attach_jd should succeed - test in child */
	pid = fork();
	ATF_REQUIRE(pid >= 0);

	if (pid == 0) {
		error = jail_attach_jd(fd);
		_exit(error == 0 ? 0 : 1);
	}

	waitpid(pid, &status, 0);
	ATF_REQUIRE_MSG(WIFEXITED(status) && WEXITSTATUS(status) == 0,
	    "jail_attach_jd should succeed on unlimited descriptor");

	/* jail_remove_jd should succeed */
	error = jail_remove_jd(fd);
	ATF_REQUIRE_MSG(error == 0, "jail_remove_jd failed: %s",
	    strerror(errno));

	close(fd);
}
ATF_TC_CLEANUP(cap_jail_unlimited, tc)
{
	remove_jail_by_name("cap_unlimited_test");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, cap_jail_set);
	ATF_TP_ADD_TC(tp, cap_jail_attach);
	ATF_TP_ADD_TC(tp, cap_jail_remove);
	ATF_TP_ADD_TC(tp, cap_jail_both);
	ATF_TP_ADD_TC(tp, cap_jail_unlimited);

	return (atf_no_error());
}
