/*-
 * Copyright (c) 2016 Ed Maste <emaste@FreeBSD.org>
 * Copyright (c) 2016 Conrad Meyer <cem@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <atf-c.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "freebsd_test_suite/macros.h"

static int dirfd = -1;
static char *abspath;

static void
touchat(int _dirfd, const char *name)
{
	int fd;

	ATF_REQUIRE((fd = openat(_dirfd, name, O_CREAT | O_TRUNC | O_WRONLY,
	    0777)) >= 0);
	ATF_REQUIRE(close(fd) == 0);
}

static void
prepare_dotdot_tests(void)
{
	char cwd[MAXPATHLEN];

	ATF_REQUIRE(getcwd(cwd, sizeof(cwd)) != NULL);
	asprintf(&abspath, "%s/testdir/d1/f1", cwd);

	ATF_REQUIRE(mkdir("testdir", 0777) == 0);
	ATF_REQUIRE((dirfd = open("testdir", O_RDONLY)) >= 0);

	ATF_REQUIRE(mkdirat(dirfd, "d1", 0777) == 0);
	ATF_REQUIRE(mkdirat(dirfd, "d1/d2", 0777) == 0);
	ATF_REQUIRE(mkdirat(dirfd, "d1/d2/d3", 0777) == 0);
	touchat(dirfd, "d1/f1");
	touchat(dirfd, "d1/d2/f2");
	touchat(dirfd, "d1/d2/d3/f3");
	ATF_REQUIRE(symlinkat("d1/d2/d3", dirfd, "l3") == 0);
	ATF_REQUIRE(symlinkat("../testdir/d1", dirfd, "lup") == 0);
	ATF_REQUIRE(symlinkat("../..", dirfd, "d1/d2/d3/ld1") == 0);
	ATF_REQUIRE(symlinkat("../../f1", dirfd, "d1/d2/d3/lf1") == 0);
}

static void
check_capsicum(void)
{
	ATF_REQUIRE_FEATURE("security_capabilities");
	ATF_REQUIRE_FEATURE("security_capability_mode");
	ATF_REQUIRE_SYSCTL_BOOL("kern.trap_enotcap", false);
}

/*
 * Positive tests
 */
ATF_TC(openat__basic_positive);
ATF_TC_HEAD(openat__basic_positive, tc)
{
	atf_tc_set_md_var(tc, "descr", "Basic positive openat testcases");
}

ATF_TC_BODY(openat__basic_positive, tc)
{
	prepare_dotdot_tests();

	ATF_REQUIRE(openat(dirfd, "d1/d2/d3/f3", O_RDONLY) >= 0);
	ATF_REQUIRE(openat(dirfd, "d1/d2/d3/../../f1", O_RDONLY) >= 0);
	ATF_REQUIRE(openat(dirfd, "l3/f3", O_RDONLY) >= 0);
	ATF_REQUIRE(openat(dirfd, "l3/../../f1", O_RDONLY) >= 0);
	ATF_REQUIRE(openat(dirfd, "../testdir/d1/f1", O_RDONLY) >= 0);
	ATF_REQUIRE(openat(dirfd, "lup/f1", O_RDONLY) >= 0);
	ATF_REQUIRE(openat(dirfd, "l3/ld1", O_RDONLY) >= 0);
	ATF_REQUIRE(openat(dirfd, "l3/lf1", O_RDONLY) >= 0);
	ATF_REQUIRE(open(abspath, O_RDONLY) >= 0);
	ATF_REQUIRE(openat(dirfd, abspath, O_RDONLY) >= 0);
}

ATF_TC(lookup_cap_dotdot__basic);
ATF_TC_HEAD(lookup_cap_dotdot__basic, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Validate cap-mode (testdir)/d1/.. lookup");
}

ATF_TC_BODY(lookup_cap_dotdot__basic, tc)
{
	cap_rights_t rights;

	check_capsicum();
	prepare_dotdot_tests();

	cap_rights_init(&rights, CAP_LOOKUP, CAP_READ);
	ATF_REQUIRE(cap_rights_limit(dirfd, &rights) >= 0);

	ATF_REQUIRE(cap_enter() >= 0);

	ATF_REQUIRE_MSG(openat(dirfd, "d1/..", O_RDONLY) >= 0, "%s",
	    strerror(errno));
}

ATF_TC(lookup_cap_dotdot__advanced);
ATF_TC_HEAD(lookup_cap_dotdot__advanced, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Validate cap-mode (testdir)/d1/.. lookup");
}

ATF_TC_BODY(lookup_cap_dotdot__advanced, tc)
{
	cap_rights_t rights;

	check_capsicum();
	prepare_dotdot_tests();

	cap_rights_init(&rights, CAP_LOOKUP, CAP_READ);
	ATF_REQUIRE(cap_rights_limit(dirfd, &rights) >= 0);

	ATF_REQUIRE(cap_enter() >= 0);

	ATF_REQUIRE(openat(dirfd, "d1/d2/d3/../../f1", O_RDONLY) >= 0);
	ATF_REQUIRE(openat(dirfd, "l3/../../f1", O_RDONLY) >= 0);
	ATF_REQUIRE(openat(dirfd, "l3/ld1", O_RDONLY) >= 0);
	ATF_REQUIRE(openat(dirfd, "l3/lf1", O_RDONLY) >= 0);
}

/*
 * Negative tests
 */
ATF_TC(openat__basic_negative);
ATF_TC_HEAD(openat__basic_negative, tc)
{
	atf_tc_set_md_var(tc, "descr", "Basic negative openat testcases");
}

ATF_TC_BODY(openat__basic_negative, tc)
{
	prepare_dotdot_tests();

	ATF_REQUIRE_ERRNO(ENOENT,
	    openat(dirfd, "does-not-exist", O_RDONLY) < 0);
	ATF_REQUIRE_ERRNO(ENOENT,
	    openat(dirfd, "l3/does-not-exist", O_RDONLY) < 0);
}

ATF_TC(capmode__negative);
ATF_TC_HEAD(capmode__negative, tc)
{
	atf_tc_set_md_var(tc, "descr", "Negative Capability mode testcases");
}

ATF_TC_BODY(capmode__negative, tc)
{
	int subdirfd;

	check_capsicum();
	prepare_dotdot_tests();

	ATF_REQUIRE(cap_enter() == 0);

	/* open() not permitted in capability mode */
	ATF_REQUIRE_ERRNO(ECAPMODE, open("testdir", O_RDONLY) < 0);

	/* AT_FDCWD not permitted in capability mode */
	ATF_REQUIRE_ERRNO(ECAPMODE, openat(AT_FDCWD, "d1/f1", O_RDONLY) < 0);

	/* Relative path above dirfd not capable */
	ATF_REQUIRE_ERRNO(ENOTCAPABLE, openat(dirfd, "..", O_RDONLY) < 0);
	ATF_REQUIRE((subdirfd = openat(dirfd, "l3", O_RDONLY)) >= 0);
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    openat(subdirfd, "../../f1", O_RDONLY) < 0);

	/* Absolute paths not capable */
	ATF_REQUIRE_ERRNO(ENOTCAPABLE, openat(dirfd, abspath, O_RDONLY) < 0);

	/* Symlink above dirfd */
	ATF_REQUIRE_ERRNO(ENOTCAPABLE, openat(dirfd, "lup/f1", O_RDONLY) < 0);
}

ATF_TC(lookup_cap_dotdot__negative);
ATF_TC_HEAD(lookup_cap_dotdot__negative, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Validate cap-mode (testdir)/.. lookup fails");
}

ATF_TC_BODY(lookup_cap_dotdot__negative, tc)
{
	cap_rights_t rights;

	check_capsicum();
	prepare_dotdot_tests();

	cap_rights_init(&rights, CAP_LOOKUP, CAP_READ);
	ATF_REQUIRE(cap_rights_limit(dirfd, &rights) >= 0);

	ATF_REQUIRE(cap_enter() >= 0);

	ATF_REQUIRE_ERRNO(ENOTCAPABLE, openat(dirfd, "..", O_RDONLY) < 0);
	ATF_REQUIRE_ERRNO(ENOTCAPABLE, openat(dirfd, "d1/../..", O_RDONLY) < 0);
	ATF_REQUIRE_ERRNO(ENOTCAPABLE, openat(dirfd, "../testdir/d1/f1", O_RDONLY) < 0);
}

ATF_TC(lookup_cap_dotdot__root);
ATF_TC_HEAD(lookup_cap_dotdot__root, tc)
{
	atf_tc_set_md_var(tc, "descr", "Validate cap-mode /.. lookup fails");
}

ATF_TC_BODY(lookup_cap_dotdot__root, tc)
{
	int dfd, dfd2;

	check_capsicum();

	dfd = open("/", O_DIRECTORY);
	ATF_REQUIRE(dfd >= 0);

	dfd2 = openat(dfd, "..", O_DIRECTORY);
	ATF_REQUIRE(dfd2 >= 0);
	ATF_REQUIRE(close(dfd2) == 0);

	ATF_REQUIRE(cap_enter() >= 0);

	dfd2 = openat(dfd, "..", O_DIRECTORY);
	ATF_REQUIRE_ERRNO(ENOTCAPABLE, openat(dfd, "..", O_DIRECTORY));
}

/*
 * ".." from a directory that is a mount root: the lookup must not cross
 * to the covered vnode and leave its starting directory.
 */
static char mntpath[MAXPATHLEN];

/* Mount tmpfs over the existing directory "mnt". */
static void
mount_tmpfs(bool unionmnt)
{
	struct iovec iov[6];
	char cwd[MAXPATHLEN];
	u_int iovlen;

	ATF_REQUIRE(getcwd(cwd, sizeof(cwd)) != NULL);
	snprintf(mntpath, sizeof(mntpath), "%s/mnt", cwd);
	iov[0].iov_base = __DECONST(char *, "fstype");
	iov[0].iov_len = sizeof("fstype");
	iov[1].iov_base = __DECONST(char *, "tmpfs");
	iov[1].iov_len = sizeof("tmpfs");
	iov[2].iov_base = __DECONST(char *, "fspath");
	iov[2].iov_len = sizeof("fspath");
	iov[3].iov_base = mntpath;
	iov[3].iov_len = strlen(mntpath) + 1;
	iovlen = 4;
	if (unionmnt) {
		iov[4].iov_base = __DECONST(char *, "union");
		iov[4].iov_len = sizeof("union");
		iov[5].iov_base = NULL;
		iov[5].iov_len = 0;
		iovlen = 6;
	}
	ATF_REQUIRE_MSG(nmount(iov, iovlen, 0) == 0, "nmount: %s",
	    strerror(errno));
}

static void
unmount_tmpfs(void)
{
	char cwd[MAXPATHLEN];

	if (getcwd(cwd, sizeof(cwd)) == NULL)
		return;
	snprintf(mntpath, sizeof(mntpath), "%s/mnt", cwd);
	(void)unmount(mntpath, MNT_FORCE);
}

ATF_TC_WITH_CLEANUP(lookup_cap_dotdot__mountroot);
ATF_TC_HEAD(lookup_cap_dotdot__mountroot, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Validate that .. from a mount root stays beneath it");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(lookup_cap_dotdot__mountroot, tc)
{
	int dfd;

	check_capsicum();
	ATF_REQUIRE(mkdir("mnt", 0777) == 0);
	mount_tmpfs(false);

	ATF_REQUIRE((dfd = open(mntpath, O_RDONLY | O_DIRECTORY)) >= 0);
	ATF_REQUIRE(mkdirat(dfd, "sub", 0777) == 0);

	/* A ".." that ends at the start is fine. */
	ATF_REQUIRE_MSG(openat(dfd, "sub/..", O_RDONLY | O_DIRECTORY |
	    O_RESOLVE_BENEATH) >= 0, "%s", strerror(errno));

	ATF_REQUIRE_ERRNO(ENOTCAPABLE, openat(dfd, "..", O_RDONLY |
	    O_DIRECTORY | O_RESOLVE_BENEATH) < 0);

	ATF_REQUIRE(cap_enter() >= 0);

	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    openat(dfd, "..", O_RDONLY | O_DIRECTORY) < 0);
}

ATF_TC_CLEANUP(lookup_cap_dotdot__mountroot, tc)
{
	unmount_tmpfs();
}

/*
 * A name missing from the root of a union mount is looked up in the
 * covered directory.  A relative symlink found there is resolved from
 * the covered directory, and its ".." must not leave the starting
 * directory either.
 */
ATF_TC_WITH_CLEANUP(lookup_cap_dotdot__union);
ATF_TC_HEAD(lookup_cap_dotdot__union, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Validate that symlinks below a union mount root stay beneath it");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(lookup_cap_dotdot__union, tc)
{
	int dfd;

	check_capsicum();
	touchat(AT_FDCWD, "outside");
	ATF_REQUIRE(mkdir("mnt", 0777) == 0);
	touchat(AT_FDCWD, "mnt/lower");
	ATF_REQUIRE(symlink("lower", "mnt/lnklower") == 0);
	ATF_REQUIRE(symlink("../outside", "mnt/lnkout") == 0);
	mount_tmpfs(true);

	ATF_REQUIRE((dfd = open(mntpath, O_RDONLY | O_DIRECTORY)) >= 0);

	/* Entries of the covered directory are still found. */
	ATF_REQUIRE_MSG(openat(dfd, "lower", O_RDONLY |
	    O_RESOLVE_BENEATH) >= 0, "%s", strerror(errno));
	ATF_REQUIRE_MSG(openat(dfd, "lnklower", O_RDONLY |
	    O_RESOLVE_BENEATH) >= 0, "%s", strerror(errno));

	ATF_REQUIRE_ERRNO(ENOTCAPABLE, openat(dfd, "lnkout", O_RDONLY |
	    O_RESOLVE_BENEATH) < 0);

	ATF_REQUIRE(cap_enter() >= 0);

	ATF_REQUIRE_ERRNO(ENOTCAPABLE, openat(dfd, "lnkout", O_RDONLY) < 0);
}

ATF_TC_CLEANUP(lookup_cap_dotdot__union, tc)
{
	unmount_tmpfs();
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, openat__basic_positive);
	ATF_TP_ADD_TC(tp, openat__basic_negative);

	ATF_TP_ADD_TC(tp, capmode__negative);

	ATF_TP_ADD_TC(tp, lookup_cap_dotdot__basic);
	ATF_TP_ADD_TC(tp, lookup_cap_dotdot__advanced);
	ATF_TP_ADD_TC(tp, lookup_cap_dotdot__negative);
	ATF_TP_ADD_TC(tp, lookup_cap_dotdot__root);
	ATF_TP_ADD_TC(tp, lookup_cap_dotdot__mountroot);
	ATF_TP_ADD_TC(tp, lookup_cap_dotdot__union);

	return (atf_no_error());
}
