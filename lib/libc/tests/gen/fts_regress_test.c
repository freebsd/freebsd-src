/*
 * Copyright (c) 2026 Jitendra Bhati
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Regression tests for specific FreeBSD bug reports fixed in fts(3).
 */

#include <sys/stat.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <atf-c.h>

/*
 * Thrash function for file-based race tests: repeatedly creates and
 * deletes a regular file at the given path.
 */
static volatile bool race_stop;

static void *
race_thrash(void *arg)
{
	const char *path = arg;

	while (!race_stop) {
		(void)close(creat(path, 0644));
		(void)unlink(path);
	}
	return (NULL);
}

/*
 * Thrash function for directory-based race tests: repeatedly removes
 * and recreates a directory at the given path.
 */
static void *
dir_thrash(void *arg)
{
	const char *path = arg;

	while (!race_stop) {
		(void)rmdir(path);
		(void)mkdir(path, 0755);
	}
	return (NULL);
}

/*
 * PR 45723: A directory with read but no execute permission must be
 * traversed.  Before the fix, fts_build() gave up silently when
 * chdir() failed, producing no output at all.  The fix falls back to
 * FTS_DONTCHDIR mode so the directory is still traversed using full
 * relative paths.
 *
 * Requires an unprivileged user because root ignores permissions.
 */
ATF_TC(read_no_exec_dir);
ATF_TC_HEAD(read_no_exec_dir, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "directory with read but no execute is traversed via "
	    "FTS_DONTCHDIR fallback");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(read_no_exec_dir, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent;
	bool saw_d, saw_file;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/file", 0644)));
	ATF_REQUIRE_EQ(0, chmod("dir", 0400));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	/*
	 * Before the fix, zero entries were produced.  After the fix,
	 * fts falls back to FTS_DONTCHDIR and traverses using full paths.
	 * Verify the directory is not silently skipped.
	 */
	saw_d = false;
	saw_file = false;
	while ((ent = fts_read(fts)) != NULL) {
		if (ent->fts_info == FTS_D &&
		    strcmp(ent->fts_name, "dir") == 0)
			saw_d = true;
		if (strcmp(ent->fts_name, "file") == 0)
			saw_file = true;
	}

	ATF_CHECK_MSG(saw_d,
	    "FTS_D not returned for directory with mode 0400");
	ATF_CHECK_MSG(saw_file,
	    "file inside mode 0400 directory was not visited");

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * PR 196724: FTS_SLNONE must not be returned for a non-symlink.
 *
 * The fix ensures that FTS_SLNONE is only returned when lstat confirms
 * the entry is actually a symlink.  Exercised by a time-bounded race
 * where a background thread creates and deletes a regular file while
 * fts traverses with FTS_LOGICAL.
 */
ATF_TC(no_slnone_for_nonsymlink);
ATF_TC_HEAD(no_slnone_for_nonsymlink, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "FTS_SLNONE must not be returned for a non-symlink");
}
ATF_TC_BODY(no_slnone_for_nonsymlink, tc)
{
	pthread_t thr;
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent;
	struct timespec start, now, elapsed;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, symlink("nonexistent", "dir/dead"));

	race_stop = false;
	ATF_REQUIRE_EQ(0, pthread_create(&thr, NULL, race_thrash,
	    __DECONST(void *, "dir/victim")));

	clock_gettime(CLOCK_MONOTONIC, &start);
	for (;;) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		timespecsub(&now, &start, &elapsed);
		if (elapsed.tv_sec >= 1)
			break;
		fts = fts_open(paths, FTS_LOGICAL, NULL);
		ATF_REQUIRE(fts != NULL);
		while ((ent = fts_read(fts)) != NULL) {
			if (ent->fts_info == FTS_SLNONE &&
			    ent->fts_statp->st_mode != 0 &&
			    !S_ISLNK(ent->fts_statp->st_mode))
				ATF_CHECK_MSG(0,
				    "FTS_SLNONE returned for non-symlink '%s'",
				    ent->fts_name);
		}
		fts_close(fts);
	}

	race_stop = true;
	pthread_join(thr, NULL);
}

/*
 * PR 262038: fts_build() must detect readdir(2) errors and not treat
 * them as end-of-directory.  The man page specifies that FTS_DNR must
 * immediately follow FTS_D, in place of FTS_DP.
 *
 * Requires an unprivileged user because root ignores permissions.
 */
ATF_TC(readdir_error_detected);
ATF_TC_HEAD(readdir_error_detected, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "readdir errors produce FTS_DNR with fts_errno set");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(readdir_error_detected, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/file", 0644)));

	/*
	 * Mode 0100: execute only, no read.  chdir() succeeds but
	 * opendir/readdir fails.  fts must return FTS_D then FTS_DNR
	 * (not FTS_DP) per the man page.
	 */
	ATF_REQUIRE_EQ(0, chmod("dir", 0100));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	ATF_REQUIRE((ent = fts_read(fts)) != NULL);
	ATF_CHECK_EQ_MSG(FTS_D, ent->fts_info,
	    "expected FTS_D, got %d", ent->fts_info);

	ATF_REQUIRE((ent = fts_read(fts)) != NULL);
	ATF_CHECK_EQ_MSG(FTS_DNR, ent->fts_info,
	    "expected FTS_DNR, got %d", ent->fts_info);
	ATF_CHECK_MSG(ent->fts_errno != 0,
	    "FTS_DNR must have non-zero fts_errno");

	ATF_REQUIRE_EQ_MSG(NULL, fts_read(fts),
	    "expected NULL after FTS_DNR");

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * SVN r246641: fts_safe_changedir() uses O_DIRECTORY to prevent a
 * TOCTOU substitution attack where a directory is replaced with a
 * non-directory between stat and open.  Exercised by a time-bounded
 * race where a background thread repeatedly removes and recreates
 * dir/sub while fts traverses.
 */
ATF_TC(odirectory_changedir);
ATF_TC_HEAD(odirectory_changedir, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_safe_changedir handles concurrent dir/file substitution");
}
ATF_TC_BODY(odirectory_changedir, tc)
{
	pthread_t thr;
	char *paths[] = { "dir", NULL };
	FTS *fts;
	struct timespec start, now, elapsed;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, mkdir("dir/sub", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/sub/file", 0644)));

	/*
	 * Background thread races to remove and recreate dir/sub as a
	 * directory.  With O_DIRECTORY the open fails safely if dir/sub
	 * is temporarily absent or replaced.
	 */
	race_stop = false;
	ATF_REQUIRE_EQ(0, pthread_create(&thr, NULL, dir_thrash,
	    __DECONST(void *, "dir/sub")));

	clock_gettime(CLOCK_MONOTONIC, &start);
	for (;;) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		timespecsub(&now, &start, &elapsed);
		if (elapsed.tv_sec >= 1)
			break;
		fts = fts_open(paths, FTS_PHYSICAL, NULL);
		ATF_REQUIRE(fts != NULL);
		while (fts_read(fts) != NULL)
			;
		fts_close(fts);
	}

	race_stop = true;
	pthread_join(thr, NULL);
}

/*
 * SVN r261589: fts must not double-free when the directory tree is
 * concurrently modified.  Exercised by a time-bounded race where a
 * background thread creates and deletes a file during traversal.
 */
ATF_TC(concurrent_modification);
ATF_TC_HEAD(concurrent_modification, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "no crash when tree modified during traversal");
}
ATF_TC_BODY(concurrent_modification, tc)
{
	pthread_t thr;
	char *paths[] = { "dir", NULL };
	FTS *fts;
	struct timespec start, now, elapsed;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/stable", 0644)));

	race_stop = false;
	ATF_REQUIRE_EQ(0, pthread_create(&thr, NULL, race_thrash,
	    __DECONST(void *, "dir/victim")));

	clock_gettime(CLOCK_MONOTONIC, &start);
	for (;;) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		timespecsub(&now, &start, &elapsed);
		if (elapsed.tv_sec >= 1)
			break;
		fts = fts_open(paths, FTS_PHYSICAL, NULL);
		ATF_REQUIRE(fts != NULL);
		while (fts_read(fts) != NULL)
			;
		fts_close(fts);
	}

	race_stop = true;
	pthread_join(thr, NULL);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, read_no_exec_dir);
	ATF_TP_ADD_TC(tp, no_slnone_for_nonsymlink);
	ATF_TP_ADD_TC(tp, readdir_error_detected);
	ATF_TP_ADD_TC(tp, odirectory_changedir);
	ATF_TP_ADD_TC(tp, concurrent_modification);

	return (atf_no_error());
}
