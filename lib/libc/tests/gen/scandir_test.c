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

static void
scandir_prepare(const struct atf_tc *tc)
{
	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, mkdir("dir/dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/file", 0644)));
	ATF_REQUIRE_EQ(0, symlink("file", "dir/link"));
	ATF_REQUIRE_EQ(0, mkdir("dir/skip", 0755));
}

static void
scandir_verify(const struct atf_tc *tc, int n, struct dirent **namelist)
{
	ATF_REQUIRE_EQ_MSG(5, n, "return value is %d", n);
	ATF_CHECK_STREQ("link", namelist[0]->d_name);
	ATF_CHECK_STREQ("file", namelist[1]->d_name);
	ATF_CHECK_STREQ("dir", namelist[2]->d_name);
	ATF_CHECK_STREQ("..", namelist[3]->d_name);
	ATF_CHECK_STREQ(".", namelist[4]->d_name);
}

static int
scandir_select(const struct dirent *ent)
{
	return (strcmp(ent->d_name, "skip") != 0);
}

static int
scandir_compare(const struct dirent **a, const struct dirent **b)
{
	return (strcmp((*b)->d_name, (*a)->d_name));
}

ATF_TC(scandir_test);
ATF_TC_HEAD(scandir_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scandir()");
}
ATF_TC_BODY(scandir_test, tc)
{
	struct dirent **namelist = NULL;
	int i, ret;

	scandir_prepare(tc);
	ret = scandir("dir", &namelist, scandir_select, scandir_compare);
	scandir_verify(tc, ret, namelist);
	for (i = 0; i < ret; i++)
		free(namelist[i]);
	free(namelist);
}

ATF_TC(fdscandir_test);
ATF_TC_HEAD(fdscandir_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test fdscandir()");
}
ATF_TC_BODY(fdscandir_test, tc)
{
	struct dirent **namelist = NULL;
	int fd, i, ret;

	scandir_prepare(tc);
	ATF_REQUIRE((fd = open("dir", O_DIRECTORY | O_RDONLY)) >= 0);
	ret = fdscandir(fd, &namelist, scandir_select, scandir_compare);
	scandir_verify(tc, ret, namelist);
	for (i = 0; i < ret; i++)
		free(namelist[i]);
	free(namelist);
	ATF_REQUIRE_EQ(0, close(fd));
}

ATF_TC(scandirat_test);
ATF_TC_HEAD(scandirat_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scandirat()");
}
ATF_TC_BODY(scandirat_test, tc)
{
	struct dirent **namelist = NULL;
	int fd, i, ret;

	scandir_prepare(tc);
	ATF_REQUIRE((fd = open("dir", O_DIRECTORY | O_SEARCH)) >= 0);
	ret = scandirat(fd, ".", &namelist, scandir_select, scandir_compare);
	scandir_verify(tc, ret, namelist);
	for (i = 0; i < ret; i++)
		free(namelist[i]);
	free(namelist);
	ATF_REQUIRE_EQ(0, close(fd));
}

static int
scandir_none(const struct dirent *ent __unused)
{
	return (0);
}

ATF_TC(scandir_none);
ATF_TC_HEAD(scandir_none, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test scandir() when no entries are selected");
}
ATF_TC_BODY(scandir_none, tc)
{
	struct dirent **namelist = NULL;

	ATF_REQUIRE_EQ(0, scandir(".", &namelist, scandir_none, alphasort));
	ATF_REQUIRE(namelist);
	free(namelist);
}

/*
 * Test that scandir() propagates errors from readdir(): we create a
 * directory with enough entries that it can't be read in a single
 * getdirentries() call, then abuse the selection callback to close the
 * file descriptor scandir() is using after the first call, causing the
 * next one to fail, and verify that readdir() returns an error instead of
 * a partial result.  We make two passes, one in which nothing was
 * selected before the error occurred, and one in which everything was.
 */
static int scandir_error_count;
static int scandir_error_fd;
static int scandir_error_select_return;

static int
scandir_error_select(const struct dirent *ent __unused)
{
	if (scandir_error_count++ == 0)
		close(scandir_error_fd);
	return (scandir_error_select_return);
}

ATF_TC(scandir_error);
ATF_TC_HEAD(scandir_error, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test that scandir() propagates errors from readdir()");
}
ATF_TC_BODY(scandir_error, tc)
{
	char path[16];
	struct dirent **namelist = NULL;
	int fd, i;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	for (i = 0; i < 1024; i++) {
		snprintf(path, sizeof(path), "dir/%04x", i);
		ATF_REQUIRE_EQ(0, symlink(path + 4, path));
	}

	/* first pass, select nothing */
	ATF_REQUIRE((fd = open("dir", O_DIRECTORY | O_RDONLY)) >= 0);
	scandir_error_count = 0;
	scandir_error_fd = fd;
	scandir_error_select_return = 0;
	ATF_CHECK_ERRNO(EBADF,
	    fdscandir(fd, &namelist, scandir_error_select, NULL) < 0);
	ATF_CHECK_EQ(NULL, namelist);

	/* second pass, select everything */
	ATF_REQUIRE((fd = open("dir", O_DIRECTORY | O_RDONLY)) >= 0);
	scandir_error_count = 0;
	scandir_error_fd = fd;
	scandir_error_select_return = 1;
	ATF_CHECK_ERRNO(EBADF,
	    fdscandir(fd, &namelist, scandir_error_select, NULL) < 0);
	ATF_CHECK_EQ(NULL, namelist);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, scandir_test);
	ATF_TP_ADD_TC(tp, fdscandir_test);
	ATF_TP_ADD_TC(tp, scandirat_test);
	ATF_TP_ADD_TC(tp, scandir_none);
	ATF_TP_ADD_TC(tp, scandir_error);
	return (atf_no_error());
}
