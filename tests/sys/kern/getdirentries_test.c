/*-
 * Copyright (c) 2025 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/stat.h>
#include <sys/mount.h>

#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>

#include <atf-c.h>

ATF_TC(getdirentries_ok);
ATF_TC_HEAD(getdirentries_ok, tc)
{
	atf_tc_set_md_var(tc, "descr", "Successfully read a directory.");
}
ATF_TC_BODY(getdirentries_ok, tc)
{
	char dbuf[4096];
	struct dirent *d;
	off_t base;
	ssize_t ret;
	int dd, n;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE((dd = open("dir", O_DIRECTORY | O_RDONLY)) >= 0);
	ATF_REQUIRE((ret = getdirentries(dd, dbuf, sizeof(dbuf), &base)) > 0);
	ATF_REQUIRE_EQ(0, getdirentries(dd, dbuf, sizeof(dbuf), &base));
	ATF_REQUIRE_EQ(base, lseek(dd, 0, SEEK_CUR));
	ATF_CHECK_EQ(0, close(dd));
	for (n = 0, d = (struct dirent *)dbuf;
	     d < (struct dirent *)(dbuf + ret);
	     d = (struct dirent *)((char *)d + d->d_reclen), n++)
		/* nothing */ ;
	ATF_CHECK_EQ((struct dirent *)(dbuf + ret), d);
	ATF_CHECK_EQ(2, n);
}

ATF_TC(getdirentries_ebadf);
ATF_TC_HEAD(getdirentries_ebadf, tc)
{
	atf_tc_set_md_var(tc, "descr", "Attempt to read a directory "
	    "from an invalid descriptor.");
}
ATF_TC_BODY(getdirentries_ebadf, tc)
{
	char dbuf[4096];
	off_t base;
	int fd;

	ATF_REQUIRE((fd = open("file", O_CREAT | O_WRONLY, 0644)) >= 0);
	ATF_REQUIRE_EQ(-1, getdirentries(fd, dbuf, sizeof(dbuf), &base));
	ATF_CHECK_EQ(EBADF, errno);
	ATF_REQUIRE_EQ(0, close(fd));
	ATF_REQUIRE_EQ(-1, getdirentries(fd, dbuf, sizeof(dbuf), &base));
	ATF_CHECK_EQ(EBADF, errno);
}

ATF_TC(getdirentries_efault);
ATF_TC_HEAD(getdirentries_efault, tc)
{
	atf_tc_set_md_var(tc, "descr", "Attempt to read a directory "
	    "to an invalid buffer.");
}
ATF_TC_BODY(getdirentries_efault, tc)
{
	char dbuf[4096];
	off_t base, *basep;
	int dd;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE((dd = open("dir", O_DIRECTORY | O_RDONLY)) >= 0);
	ATF_REQUIRE_EQ(-1, getdirentries(dd, NULL, sizeof(dbuf), &base));
	ATF_CHECK_EQ(EFAULT, errno);
	basep = NULL;
	basep++;
	ATF_REQUIRE_EQ(-1, getdirentries(dd, dbuf, sizeof(dbuf), basep));
	ATF_CHECK_EQ(EFAULT, errno);
	ATF_CHECK_EQ(0, close(dd));
}

ATF_TC(getdirentries_einval);
ATF_TC_HEAD(getdirentries_einval, tc)
{
	atf_tc_set_md_var(tc, "descr", "Attempt to read a directory "
	    "with various invalid parameters.");
}
ATF_TC_BODY(getdirentries_einval, tc)
{
	struct statfs fsb;
	char dbuf[4096];
	off_t base;
	ssize_t ret;
	int dd;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE((dd = open("dir", O_DIRECTORY | O_RDONLY)) >= 0);
	ATF_REQUIRE_EQ(0, fstatfs(dd, &fsb));
	/* nbytes too small */
	ATF_REQUIRE_EQ(-1, getdirentries(dd, dbuf, 8, &base));
	ATF_CHECK_EQ(EINVAL, errno);
	/* nbytes too big */
	ATF_REQUIRE_EQ(-1, getdirentries(dd, dbuf, SIZE_MAX, &base));
	ATF_CHECK_EQ(EINVAL, errno);
	/* invalid position */
	ATF_REQUIRE((ret = getdirentries(dd, dbuf, sizeof(dbuf), &base)) > 0);
	ATF_REQUIRE_EQ(0, getdirentries(dd, dbuf, sizeof(dbuf), &base));
	ATF_REQUIRE(base > 0);
	ATF_REQUIRE_EQ(base + 3, lseek(dd, 3, SEEK_CUR));
	/* known to fail on ufs (FFS2) and zfs, and work on tmpfs */
	if (strcmp(fsb.f_fstypename, "ufs") == 0 ||
	    strcmp(fsb.f_fstypename, "zfs") == 0) {
		atf_tc_expect_fail("incorrectly returns 0 instead of EINVAL "
		    "on %s", fsb.f_fstypename);
	}
	ATF_REQUIRE_EQ(-1, getdirentries(dd, dbuf, sizeof(dbuf), &base));
	ATF_CHECK_EQ(EINVAL, errno);
	ATF_CHECK_EQ(0, close(dd));
}

ATF_TC(getdirentries_enoent);
ATF_TC_HEAD(getdirentries_enoent, tc)
{
	atf_tc_set_md_var(tc, "descr", "Attempt to read a directory "
	    "after it is deleted.");
}
ATF_TC_BODY(getdirentries_enoent, tc)
{
	char dbuf[4096];
	off_t base;
	int dd;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE((dd = open("dir", O_DIRECTORY | O_RDONLY)) >= 0);
	ATF_REQUIRE_EQ(0, rmdir("dir"));
	ATF_REQUIRE_EQ(-1, getdirentries(dd, dbuf, sizeof(dbuf), &base));
	ATF_CHECK_EQ(ENOENT, errno);
}

ATF_TC(getdirentries_enotdir);
ATF_TC_HEAD(getdirentries_enotdir, tc)
{
	atf_tc_set_md_var(tc, "descr", "Attempt to read a directory "
	    "from a descriptor not associated with a directory.");
}
ATF_TC_BODY(getdirentries_enotdir, tc)
{
	char dbuf[4096];
	off_t base;
	int fd;

	ATF_REQUIRE((fd = open("file", O_CREAT | O_RDWR, 0644)) >= 0);
	ATF_REQUIRE_EQ(-1, getdirentries(fd, dbuf, sizeof(dbuf), &base));
	ATF_CHECK_EQ(ENOTDIR, errno);
	ATF_CHECK_EQ(0, close(fd));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, getdirentries_ok);
	ATF_TP_ADD_TC(tp, getdirentries_ebadf);
	ATF_TP_ADD_TC(tp, getdirentries_efault);
	ATF_TP_ADD_TC(tp, getdirentries_einval);
	ATF_TP_ADD_TC(tp, getdirentries_enoent);
	ATF_TP_ADD_TC(tp, getdirentries_enotdir);
	return (atf_no_error());
}
