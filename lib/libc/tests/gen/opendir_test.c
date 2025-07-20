/*-
 * Copyright (c) 2025 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include <atf-c.h>

/*
 * Create a directory with a single subdirectory.
 */
static void
opendir_prepare(const struct atf_tc *tc)
{
	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, mkdir("dir/subdir", 0755));
}

/*
 * Assuming dirp represents the directory created by opendir_prepare(),
 * verify that readdir() returns what we expected to see there.
 */
static void
opendir_check(const struct atf_tc *tc, DIR *dirp)
{
	struct dirent *ent;

	ATF_REQUIRE((ent = readdir(dirp)) != NULL);
	ATF_CHECK_EQ(1, ent->d_namlen);
	ATF_CHECK_STREQ(".", ent->d_name);
	ATF_CHECK_EQ(DT_DIR, ent->d_type);
	ATF_REQUIRE((ent = readdir(dirp)) != NULL);
	ATF_CHECK_EQ(2, ent->d_namlen);
	ATF_CHECK_STREQ("..", ent->d_name);
	ATF_CHECK_EQ(DT_DIR, ent->d_type);
	ATF_REQUIRE((ent = readdir(dirp)) != NULL);
	ATF_CHECK_EQ(sizeof("subdir") - 1, ent->d_namlen);
	ATF_CHECK_STREQ("subdir", ent->d_name);
	ATF_CHECK_EQ(DT_DIR, ent->d_type);
	ATF_CHECK(readdir(dirp) == NULL);
	ATF_CHECK(readdir(dirp) == NULL);
}

ATF_TC(opendir_ok);
ATF_TC_HEAD(opendir_ok, tc)
{
	atf_tc_set_md_var(tc, "descr", "Open a directory.");
}
ATF_TC_BODY(opendir_ok, tc)
{
	DIR *dirp;

	opendir_prepare(tc);
	ATF_REQUIRE((dirp = opendir("dir")) != NULL);
	opendir_check(tc, dirp);
	ATF_CHECK_EQ(0, closedir(dirp));
}

ATF_TC(opendir_fifo);
ATF_TC_HEAD(opendir_fifo, tc)
{
	atf_tc_set_md_var(tc, "descr", "Do not hang if given a named pipe.");
}
ATF_TC_BODY(opendir_fifo, tc)
{
	DIR *dirp;
	int fd;

	ATF_REQUIRE((fd = mkfifo("fifo", 0644)) >= 0);
	ATF_REQUIRE_EQ(0, close(fd));
	ATF_REQUIRE((dirp = opendir("fifo")) == NULL);
	ATF_CHECK_EQ(ENOTDIR, errno);
}

ATF_TC(fdopendir_ok);
ATF_TC_HEAD(fdopendir_ok, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Open a directory from a directory descriptor.");
}
ATF_TC_BODY(fdopendir_ok, tc)
{
	DIR *dirp;
	int dd;

	opendir_prepare(tc);
	ATF_REQUIRE((dd = open("dir", O_DIRECTORY | O_RDONLY)) >= 0);
	ATF_REQUIRE((dirp = fdopendir(dd)) != NULL);
	opendir_check(tc, dirp);
	ATF_CHECK_EQ(dd, fdclosedir(dirp));
	ATF_CHECK_EQ(0, close(dd));
}

ATF_TC(fdopendir_ebadf);
ATF_TC_HEAD(fdopendir_ebadf, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Open a directory from an invalid descriptor.");
}
ATF_TC_BODY(fdopendir_ebadf, tc)
{
	DIR *dirp;
	int dd;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE((dd = open("dir", O_DIRECTORY | O_RDONLY)) >= 0);
	ATF_CHECK_EQ(0, close(dd));
	ATF_REQUIRE((dirp = fdopendir(dd)) == NULL);
	ATF_CHECK_EQ(EBADF, errno);
}

ATF_TC(fdopendir_enotdir);
ATF_TC_HEAD(fdopendir_enotdir, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Open a directory from a non-directory descriptor.");
}
ATF_TC_BODY(fdopendir_enotdir, tc)
{
	DIR *dirp;
	int fd;

	ATF_REQUIRE((fd = open("file", O_CREAT | O_RDWR, 0644)) >= 0);
	ATF_REQUIRE((dirp = fdopendir(fd)) == NULL);
	ATF_CHECK_EQ(ENOTDIR, errno);
	ATF_CHECK_EQ(0, close(fd));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, opendir_ok);
	ATF_TP_ADD_TC(tp, fdopendir_ok);
	ATF_TP_ADD_TC(tp, fdopendir_ebadf);
	ATF_TP_ADD_TC(tp, fdopendir_enotdir);
	ATF_TP_ADD_TC(tp, opendir_fifo);
	return (atf_no_error());
}
