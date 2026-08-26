/*
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * This software was written by Mark Johnston under sponsorship from
 * the FreeBSD Foundation.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/stat.h>
#include <sys/mount.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <atf-c.h>

/*
 * Verify that AT_RESOLVE_BENEATH is respected by various system calls.
 */

static int
setup(void)
{
	int fd, tfd;

	ATF_REQUIRE_EQ(0, mkdir("base", 0755));
	ATF_REQUIRE_EQ(0, mkdir("outside", 0755));
	ATF_REQUIRE((tfd = open("outside/target", O_CREAT | O_WRONLY, 0644)) >=
	    0);
	ATF_REQUIRE(close(tfd) == 0);
	ATF_REQUIRE_EQ(0, mkdir("outside/dir", 0755));
	ATF_REQUIRE((tfd = open("base/file", O_CREAT | O_WRONLY, 0644)) >= 0);
	ATF_REQUIRE(close(tfd) == 0);
	ATF_REQUIRE_EQ(0, mkdir("base/subdir", 0755));
	ATF_REQUIRE((fd = open("base", O_DIRECTORY | O_RDONLY)) >= 0);
	return (fd);
}

ATF_TC_WITHOUT_HEAD(faccessat_beneath);
ATF_TC_BODY(faccessat_beneath, tc)
{
	int fd;

	fd = setup();
	ATF_REQUIRE_EQ(0,
	    faccessat(fd, "file", F_OK, AT_RESOLVE_BENEATH));
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    faccessat(fd, "../outside/target", F_OK,
	    AT_RESOLVE_BENEATH) == -1);
	ATF_REQUIRE(close(fd) == 0);
}

ATF_TC_WITHOUT_HEAD(chflagsat_beneath);
ATF_TC_BODY(chflagsat_beneath, tc)
{
	int fd, ret;

	fd = setup();
	ret = chflagsat(fd, "file", UF_NODUMP, AT_RESOLVE_BENEATH);
	if (ret != 0)
		ATF_REQUIRE_EQ(EOPNOTSUPP, errno);
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    chflagsat(fd, "../outside/target", UF_NODUMP,
	    AT_RESOLVE_BENEATH) == -1);
	ATF_REQUIRE(close(fd) == 0);
}

ATF_TC_WITHOUT_HEAD(fchmodat_beneath);
ATF_TC_BODY(fchmodat_beneath, tc)
{
	int fd;

	fd = setup();
	ATF_REQUIRE_EQ(0,
	    fchmodat(fd, "file", 0644, AT_RESOLVE_BENEATH));
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    fchmodat(fd, "../outside/target", 0644,
	    AT_RESOLVE_BENEATH) == -1);
	ATF_REQUIRE(close(fd) == 0);
}

ATF_TC_WITHOUT_HEAD(fchownat_beneath);
ATF_TC_BODY(fchownat_beneath, tc)
{
	int fd;

	fd = setup();
	ATF_REQUIRE_EQ(0,
	    fchownat(fd, "file", -1, -1, AT_RESOLVE_BENEATH));
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    fchownat(fd, "../outside/target", 0, 0,
	    AT_RESOLVE_BENEATH) == -1);
	ATF_REQUIRE(close(fd) == 0);
}

ATF_TC_WITHOUT_HEAD(utimensat_beneath);
ATF_TC_BODY(utimensat_beneath, tc)
{
	int fd;

	fd = setup();
	ATF_REQUIRE_EQ(0,
	    utimensat(fd, "file", NULL, AT_RESOLVE_BENEATH));
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    utimensat(fd, "../outside/target", NULL,
	    AT_RESOLVE_BENEATH) == -1);
	ATF_REQUIRE(close(fd) == 0);
}

ATF_TC_WITHOUT_HEAD(linkat_beneath);
ATF_TC_BODY(linkat_beneath, tc)
{
	int fd;

	fd = setup();
	ATF_REQUIRE_EQ(0,
	    linkat(fd, "file", fd, "hardlink", AT_RESOLVE_BENEATH));
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    linkat(fd, "../outside/target", fd, "hardlink2",
	    AT_RESOLVE_BENEATH) == -1);
	ATF_REQUIRE(close(fd) == 0);
}

ATF_TC_WITHOUT_HEAD(unlinkat_beneath);
ATF_TC_BODY(unlinkat_beneath, tc)
{
	int fd;

	fd = setup();
	ATF_REQUIRE_EQ(0,
	    unlinkat(fd, "file", AT_RESOLVE_BENEATH));
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    unlinkat(fd, "../outside/target",
	    AT_RESOLVE_BENEATH) == -1);
	ATF_REQUIRE(close(fd) == 0);
}

ATF_TC_WITHOUT_HEAD(unlinkat_rmdir_beneath);
ATF_TC_BODY(unlinkat_rmdir_beneath, tc)
{
	int fd;

	fd = setup();
	ATF_REQUIRE_EQ(0,
	    unlinkat(fd, "subdir",
	    AT_REMOVEDIR | AT_RESOLVE_BENEATH));
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    unlinkat(fd, "../outside/dir",
	    AT_REMOVEDIR | AT_RESOLVE_BENEATH) == -1);
	ATF_REQUIRE_EQ(0, access("outside/dir", F_OK));
	ATF_REQUIRE(close(fd) == 0);
}

ATF_TC_WITHOUT_HEAD(funlinkat_beneath);
ATF_TC_BODY(funlinkat_beneath, tc)
{
	int fd;

	fd = setup();
	ATF_REQUIRE_EQ(0,
	    funlinkat(fd, "file", FD_NONE, AT_RESOLVE_BENEATH));
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    funlinkat(fd, "../outside/target", FD_NONE,
	    AT_RESOLVE_BENEATH) == -1);
	ATF_REQUIRE(close(fd) == 0);
}

ATF_TC_WITHOUT_HEAD(funlinkat_rmdir_beneath);
ATF_TC_BODY(funlinkat_rmdir_beneath, tc)
{
	int fd;

	fd = setup();
	ATF_REQUIRE_EQ(0, mkdir("base/subdir2", 0755));
	ATF_REQUIRE_EQ(0,
	    funlinkat(fd, "subdir2", FD_NONE,
	    AT_REMOVEDIR | AT_RESOLVE_BENEATH));
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    funlinkat(fd, "../outside/dir", FD_NONE,
	    AT_REMOVEDIR | AT_RESOLVE_BENEATH) == -1);
	ATF_REQUIRE_EQ(0, access("outside/dir", F_OK));
	ATF_REQUIRE(close(fd) == 0);
}

ATF_TC(getfhat_beneath);
ATF_TC_HEAD(getfhat_beneath, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(getfhat_beneath, tc)
{
	fhandle_t fh;
	int fd;

	fd = setup();
	ATF_REQUIRE_EQ(0,
	    getfhat(fd, "file", &fh, AT_RESOLVE_BENEATH));
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    getfhat(fd, "../outside/target", &fh,
	    AT_RESOLVE_BENEATH) == -1);
	ATF_REQUIRE(close(fd) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, faccessat_beneath);
	ATF_TP_ADD_TC(tp, chflagsat_beneath);
	ATF_TP_ADD_TC(tp, fchmodat_beneath);
	ATF_TP_ADD_TC(tp, fchownat_beneath);
	ATF_TP_ADD_TC(tp, utimensat_beneath);
	ATF_TP_ADD_TC(tp, linkat_beneath);
	ATF_TP_ADD_TC(tp, unlinkat_beneath);
	ATF_TP_ADD_TC(tp, unlinkat_rmdir_beneath);
	ATF_TP_ADD_TC(tp, funlinkat_beneath);
	ATF_TP_ADD_TC(tp, funlinkat_rmdir_beneath);
	ATF_TP_ADD_TC(tp, getfhat_beneath);
	return (atf_no_error());
}
