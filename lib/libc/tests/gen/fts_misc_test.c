/*
 * Copyright (c) 2025 Klara, Inc.
 * Copyright (c) 2026 Jitendra Bhati
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <sys/uio.h>

#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#include "fts_test.h"

ATF_TC(fts_unrdir);
ATF_TC_HEAD(fts_unrdir, tc)
{
	atf_tc_set_md_var(tc, "descr", "unreadable directories");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(fts_unrdir, tc)
{
	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, mkdir("dir/unr", 0100));
	ATF_REQUIRE_EQ(0, mkdir("dir/unx", 0400));
	fts_test(tc, &(struct fts_testcase){
		    (char *[]){ "dir", NULL },
		    FTS_PHYSICAL,
		    (struct fts_expect[]){
			    { FTS_D,	"dir",	"dir" },
			    { FTS_D,	"unr",	"unr" },
			    { FTS_DNR,	"unr",	"unr" },
			    { FTS_D,	"unx",	"unx" },
			    { FTS_DP,	"unx",	"unx" },
			    { FTS_DP,	"dir",	"dir" },
			    { 0 }
		    },
	    });
}

ATF_TC(fts_unrdir_nochdir);
ATF_TC_HEAD(fts_unrdir_nochdir, tc)
{
	atf_tc_set_md_var(tc, "descr", "unreadable directories (nochdir)");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(fts_unrdir_nochdir, tc)
{
	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, mkdir("dir/unr", 0100));
	ATF_REQUIRE_EQ(0, mkdir("dir/unx", 0400));
	fts_test(tc, &(struct fts_testcase){
		    (char *[]){ "dir", NULL },
		    FTS_PHYSICAL | FTS_NOCHDIR,
		    (struct fts_expect[]){
			    { FTS_D,	"dir",	"dir" },
			    { FTS_D,	"unr",	"dir/unr" },
			    { FTS_DNR,	"unr",	"dir/unr" },
			    { FTS_D,	"unx",	"dir/unx" },
			    { FTS_DP,	"unx",	"dir/unx" },
			    { FTS_DP,	"dir",	"dir" },
			    { 0 }
		    },
	    });
}

/*
 * With FTS_NOCHDIR and absolute paths, the application may call chdir(2)
 * freely between fts_read() calls without corrupting the traversal.
 */
ATF_TC(nochdir_app_can_chdir);
ATF_TC_HEAD(nochdir_app_can_chdir, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "FTS_NOCHDIR: application chdir between reads does not "
	    "corrupt traversal");
}
ATF_TC_BODY(nochdir_app_can_chdir, tc)
{
	char *cwd, *abspath;
	char *paths[2];
	char pwd[PATH_MAX];
	FTS *fts;
	FTSENT *ent;
	int entries;

	cwd = malloc(PATH_MAX);
	ATF_REQUIRE(cwd != NULL);
	abspath = malloc(PATH_MAX * 2);
	ATF_REQUIRE(abspath != NULL);

	ATF_REQUIRE(getcwd(cwd, PATH_MAX) != NULL);
	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/a", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("dir/b", 0644)));

	snprintf(abspath, PATH_MAX * 2, "%s/dir", cwd);
	paths[0] = abspath;
	paths[1] = NULL;

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR,
	    fts_lexical_compar)) != NULL);

	/*
	 * Chdir to root once after fts_open() but before fts_read().
	 * With FTS_NOCHDIR, fts must not call chdir() itself, so the
	 * process CWD must remain "/" throughout the traversal.
	 */
	ATF_REQUIRE_EQ(0, chdir("/"));

	entries = 0;
	while ((ent = fts_read(fts)) != NULL) {
		ATF_REQUIRE(getcwd(pwd, sizeof(pwd)) != NULL);
		ATF_CHECK_STREQ_MSG("/", pwd,
		    "PWD changed during FTS_NOCHDIR traversal");
		entries++;
	}
	ATF_CHECK_EQ_MSG(0, errno,
	    "traversal ended with errno %d", errno);

	/* FTS_D dir, FTS_F a, FTS_F b, FTS_DP dir = 4 entries */
	ATF_CHECK_EQ_MSG(4, entries,
	    "expected 4 entries, got %d", entries);

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
	free(cwd);
	free(abspath);
}

/*
 * fts_name is always NUL-terminated and fts_namelen always equals
 * strlen(fts_name), regardless of traversal options or entry type.
 */
ATF_TC(name_nul_terminated);
ATF_TC_HEAD(name_nul_terminated, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_name is always NUL-terminated with correct fts_namelen");
}
ATF_TC_BODY(name_nul_terminated, tc)
{
	char *paths[] = { "root", NULL };
	FTS *fts;
	FTSENT *ent;

	ATF_REQUIRE_EQ(0, mkdir("root", 0755));
	ATF_REQUIRE_EQ(0, mkdir("root/sub", 0755));
	ATF_REQUIRE_EQ(0, close(creat("root/sub/file.c", 0644)));
	ATF_REQUIRE_EQ(0, symlink("file.c", "root/sub/link"));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL,
	    fts_lexical_compar)) != NULL);

	while ((ent = fts_read(fts)) != NULL) {
		ATF_CHECK_EQ_MSG(strlen(ent->fts_name), ent->fts_namelen,
		    "fts_namelen %zu != strlen(fts_name) %zu for '%s'",
		    ent->fts_namelen, strlen(ent->fts_name), ent->fts_name);
	}

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * Every FTS_D must be paired with exactly one FTS_DP.  fts_level must
 * be FTS_ROOTLEVEL (0) for the root, incrementing by one per level.
 */
ATF_TC(prepost_order_and_levels);
ATF_TC_HEAD(prepost_order_and_levels, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "FTS_D/FTS_DP are paired and fts_level increments correctly");
}
ATF_TC_BODY(prepost_order_and_levels, tc)
{
	char *paths[] = { "top", NULL };
	FTS *fts;
	FTSENT *ent;
	static const int stack_depth = 32;
	struct {
		const char	*name;
		long		 level;
	} stack[32];
	int depth;

	ATF_REQUIRE_EQ(0, mkdir("top", 0755));
	ATF_REQUIRE_EQ(0, mkdir("top/mid", 0755));
	ATF_REQUIRE_EQ(0, mkdir("top/mid/bot", 0755));
	ATF_REQUIRE_EQ(0, close(creat("top/mid/bot/leaf", 0644)));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL,
	    fts_lexical_compar)) != NULL);

	depth = 0;
	while ((ent = fts_read(fts)) != NULL) {
		if (ent->fts_info == FTS_D) {
			ATF_REQUIRE_MSG(depth < stack_depth,
			    "stack overflow in test");
			stack[depth].name  = ent->fts_name;
			stack[depth].level = ent->fts_level;
			depth++;
		} else if (ent->fts_info == FTS_DP) {
			ATF_REQUIRE_MSG(depth > 0,
			    "FTS_DP without matching FTS_D");
			depth--;
			ATF_CHECK_STREQ(stack[depth].name, ent->fts_name);
			ATF_CHECK_EQ(stack[depth].level, ent->fts_level);
		}

		if (ent->fts_info == FTS_D || ent->fts_info == FTS_DP ||
		    ent->fts_info == FTS_F) {
			if (strcmp(ent->fts_name, "top") == 0)
				ATF_CHECK_EQ(FTS_ROOTLEVEL, ent->fts_level);
			else if (strcmp(ent->fts_name, "mid") == 0)
				ATF_CHECK_EQ(1, ent->fts_level);
			else if (strcmp(ent->fts_name, "bot") == 0)
				ATF_CHECK_EQ(2, ent->fts_level);
			else if (strcmp(ent->fts_name, "leaf") == 0)
				ATF_CHECK_EQ(3, ent->fts_level);
		}
	}
	ATF_CHECK_EQ_MSG(0, depth,
	    "%d unmatched FTS_D entries at end", depth);

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * FTSENT fields fts_errno, fts_dev, fts_ino, and fts_nlink must be
 * consistent with what stat(2) returns for successfully visited entries.
 */
ATF_TC(ftsent_fields);
ATF_TC_HEAD(ftsent_fields, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "FTSENT fts_errno/fts_dev/fts_ino/fts_nlink are correct");
}
ATF_TC_BODY(ftsent_fields, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent;
	struct stat sb;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/file", 0644)));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL,
	    fts_lexical_compar)) != NULL);

	while ((ent = fts_read(fts)) != NULL) {
		ATF_CHECK_EQ_MSG(0, ent->fts_errno,
		    "fts_errno != 0 for '%s'", ent->fts_name);

		if (ent->fts_info == FTS_D) {
			ATF_REQUIRE_EQ_MSG(0,
			    stat(ent->fts_accpath, &sb),
			    "stat(%s): %m", ent->fts_accpath);
			ATF_CHECK_EQ(sb.st_dev, ent->fts_dev);
			ATF_CHECK_EQ(sb.st_ino, ent->fts_ino);
			/*
			 * "dir" has exactly two links: one from its
			 * parent and one from its own "." entry.
			 */
			ATF_CHECK_EQ_MSG(2, ent->fts_nlink,
			    "expected fts_nlink == 2 for '%s', got %ju",
			    ent->fts_name, (uintmax_t)ent->fts_nlink);
		}
	}

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * Under FTS_PHYSICAL, symlinks are never followed, so a circular
 * symlink loop cannot cause infinite recursion.  Both symlinks are
 * returned as FTS_SL and traversal terminates.
 */
ATF_TC(symlink_loop_physical);
ATF_TC_HEAD(symlink_loop_physical, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "circular symlink loop under FTS_PHYSICAL terminates");
}
ATF_TC_BODY(symlink_loop_physical, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent;
	int entries;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, symlink("b", "dir/a"));
	ATF_REQUIRE_EQ(0, symlink("a", "dir/b"));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL,
	    fts_lexical_compar)) != NULL);

	entries = 0;
	while ((ent = fts_read(fts)) != NULL) {
		ATF_CHECK_MSG(
		    ent->fts_info == FTS_D  ||
		    ent->fts_info == FTS_DP ||
		    ent->fts_info == FTS_SL,
		    "unexpected fts_info %d for '%s'",
		    ent->fts_info, ent->fts_name);
		ATF_REQUIRE_MSG(++entries < 100,
		    "traversal exceeded 100 entries, probable infinite loop");
	}

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * Cycle detection via dev/ino comparison under FTS_LOGICAL: following
 * a symlink that points back to an ancestor must produce FTS_DC rather
 * than infinite recursion.
 */
ATF_TC(cycle_detection);
ATF_TC_HEAD(cycle_detection, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "cycle via symlink under FTS_LOGICAL yields FTS_DC");
}
ATF_TC_BODY(cycle_detection, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent;
	int saw_dc, entries;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, symlink("..", "dir/up"));

	ATF_REQUIRE((fts = fts_open(paths, FTS_LOGICAL,
	    fts_lexical_compar)) != NULL);

	saw_dc = 0;
	entries = 0;
	while ((ent = fts_read(fts)) != NULL) {
		if (ent->fts_info == FTS_DC)
			saw_dc = 1;
		ATF_REQUIRE_MSG(++entries < 100,
		    "traversal exceeded 100 entries, probable infinite loop");
	}
	ATF_CHECK_MSG(saw_dc != 0,
	    "expected FTS_DC entry for the cycle");

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * fts_close() after the root directory has been deleted must not crash.
 */
ATF_TC(close_after_root_deleted);
ATF_TC_HEAD(close_after_root_deleted, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_close after root deletion must not crash");
}
ATF_TC_BODY(close_after_root_deleted, tc)
{
	char *orig_cwd, *final_cwd;
	char *paths[] = { "dir", NULL };
	FTS *fts;

	orig_cwd = malloc(PATH_MAX);
	ATF_REQUIRE(orig_cwd != NULL);
	final_cwd = malloc(PATH_MAX);
	ATF_REQUIRE(final_cwd != NULL);

	ATF_REQUIRE(getcwd(orig_cwd, PATH_MAX) != NULL);
	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/file", 0644)));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	/* Read first entry then delete the tree. */
	ATF_REQUIRE(fts_read(fts) != NULL);
	ATF_REQUIRE_EQ(0, unlink("dir/file"));
	ATF_REQUIRE_EQ(0, rmdir("dir"));

	/*
	 * Drain traversal -- errors are expected after deletion
	 * but fts_read() must not crash.
	 */
	while (fts_read(fts) != NULL)
		;

	/* fts_close() must not crash regardless of return value. */
	(void)fts_close(fts);

	/*
	 * After fts_close(), the process CWD must be restored to
	 * the original directory even though the traversal root
	 * was deleted mid-traversal.
	 */
	ATF_REQUIRE(getcwd(final_cwd, PATH_MAX) != NULL);
	ATF_CHECK_STREQ_MSG(orig_cwd, final_cwd,
	    "CWD after fts_close should be '%s', got '%s'",
	    orig_cwd, final_cwd);

	free(orig_cwd);
	free(final_cwd);
}

/*
 * fts_close() after the root has been renamed must restore the process
 * CWD to the original directory.
 * Regression test for SVN r77497.
 */
ATF_TC(close_after_root_moved);
ATF_TC_HEAD(close_after_root_moved, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_close after root rename restores CWD (SVN r77497)");
}
ATF_TC_BODY(close_after_root_moved, tc)
{
	char *orig_cwd, *final_cwd;
	char *paths[] = { "dir", NULL };
	FTS *fts;

	orig_cwd = malloc(PATH_MAX);
	ATF_REQUIRE(orig_cwd != NULL);
	final_cwd = malloc(PATH_MAX);
	ATF_REQUIRE(final_cwd != NULL);

	ATF_REQUIRE(getcwd(orig_cwd, PATH_MAX) != NULL);
	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/file", 0644)));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	/* Read first entry then rename the root mid-traversal. */
	ATF_REQUIRE(fts_read(fts) != NULL);
	ATF_REQUIRE_EQ(0, rename("dir", "dir_moved"));

	while (fts_read(fts) != NULL)
		;

	/* fts_close() must not crash. */
	(void)fts_close(fts);

	ATF_REQUIRE(getcwd(final_cwd, PATH_MAX) != NULL);
	ATF_CHECK_STREQ_MSG(orig_cwd, final_cwd,
	    "CWD after fts_close should be '%s', got '%s'",
	    orig_cwd, final_cwd);

	free(orig_cwd);
	free(final_cwd);
}
/*
 * FTS_NOCHDIR with an empty terminal directory must not corrupt the
 * path buffer for subsequent entries.
 * Regression test for SVN r49772.
 */
ATF_TC(nochdir_empty_terminal_dir);
ATF_TC_HEAD(nochdir_empty_terminal_dir, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "FTS_NOCHDIR + empty directory does not corrupt path "
	    "(SVN r49772)");
}
ATF_TC_BODY(nochdir_empty_terminal_dir, tc)
{
	ATF_REQUIRE_EQ(0, mkdir("parent", 0755));
	ATF_REQUIRE_EQ(0, mkdir("parent/empty", 0755));
	ATF_REQUIRE_EQ(0, close(creat("parent/sibling", 0644)));

	fts_test(tc, &(struct fts_testcase){
		    (char *[]){ "parent", NULL },
		    FTS_PHYSICAL | FTS_NOCHDIR,
		    (struct fts_expect[]){
			    { FTS_D,  "parent",  "parent"         },
			    { FTS_D,  "empty",   "parent/empty"   },
			    { FTS_DP, "empty",   "parent/empty"   },
			    { FTS_F,  "sibling", "parent/sibling" },
			    { FTS_DP, "parent",  "parent"         },
			    { 0 }
		    },
	    });
}

/*
 * A nonexistent path yields FTS_NS with fts_errno set to a non-zero
 * value identifying why stat(2) failed.
 */
ATF_TC(ns_errno_set);
ATF_TC_HEAD(ns_errno_set, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "FTS_NS entry has non-zero fts_errno");
}
ATF_TC_BODY(ns_errno_set, tc)
{
	char *paths[] = { "nonexistent", NULL };
	FTS *fts;
	FTSENT *ent;

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	ent = fts_read(fts);
	ATF_REQUIRE(ent != NULL);
	ATF_CHECK_EQ(FTS_NS, ent->fts_info);
	ATF_CHECK_MSG(ent->fts_errno != 0,
	    "FTS_NS entry must have non-zero fts_errno");

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * FTS_XDEV prevents traversal from crossing mount points.
 * Mount a tmpfs on a subdirectory and verify fts does not
 * descend into it when FTS_XDEV is set.
 */
ATF_TC_WITH_CLEANUP(xdev);
ATF_TC_HEAD(xdev, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "FTS_XDEV does not cross mount points");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(xdev, tc)
{
	struct iovec iov[4];
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent;
	bool crossed;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, mkdir("dir/mnt", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/file", 0644)));

	iov[0].iov_base = (void *)"fstype";
	iov[0].iov_len  = sizeof("fstype");
	iov[1].iov_base = (void *)"tmpfs";
	iov[1].iov_len  = sizeof("tmpfs");
	iov[2].iov_base = (void *)"fspath";
	iov[2].iov_len  = sizeof("fspath");
	iov[3].iov_base = (void *)"dir/mnt";
	iov[3].iov_len  = sizeof("dir/mnt");

	if (nmount(iov, 4, 0) != 0)
		atf_tc_skip("could not mount tmpfs: %s", strerror(errno));

	ATF_REQUIRE_EQ(0, close(creat("dir/mnt/inside", 0644)));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL | FTS_XDEV,
	    fts_lexical_compar)) != NULL);

	crossed = false;
	while ((ent = fts_read(fts)) != NULL) {
		if (strcmp(ent->fts_name, "inside") == 0)
			crossed = true;
	}
	ATF_CHECK_MSG(!crossed,
	    "FTS_XDEV must not descend into tmpfs mount point");

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}
ATF_TC_CLEANUP(xdev, tc)
{
	(void)unmount("dir/mnt", 0);
}

ATF_TP_ADD_TCS(tp)
{
	fts_check_debug();
	ATF_TP_ADD_TC(tp, fts_unrdir);
	ATF_TP_ADD_TC(tp, fts_unrdir_nochdir);
	ATF_TP_ADD_TC(tp, nochdir_app_can_chdir);
	ATF_TP_ADD_TC(tp, name_nul_terminated);
	ATF_TP_ADD_TC(tp, prepost_order_and_levels);
	ATF_TP_ADD_TC(tp, ftsent_fields);
	ATF_TP_ADD_TC(tp, symlink_loop_physical);
	ATF_TP_ADD_TC(tp, cycle_detection);
	ATF_TP_ADD_TC(tp, close_after_root_deleted);
	ATF_TP_ADD_TC(tp, close_after_root_moved);
	ATF_TP_ADD_TC(tp, nochdir_empty_terminal_dir);
	ATF_TP_ADD_TC(tp, ns_errno_set);
	ATF_TP_ADD_TC(tp, xdev);

	return (atf_no_error());
}
