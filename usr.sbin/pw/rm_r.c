/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 1996
 *	David L. Nugent.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY DAVID L. NUGENT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID L. NUGENT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <libutil.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pwupd.h"

static bool try_dataset_remove(const char *home);
extern char **environ;

/*
 * "rm -r" a directory tree.  If the top-level directory cannot be removed
 * due to EBUSY, indicating that it is a ZFS dataset, and we have emptied
 * it, destroy the dataset.  Return true if any files or directories
 * remain.
 */
bool
rm_r(int rootfd, const char *path, uid_t uid)
{
	int dirfd;
	DIR *d;
	struct dirent  *e;
	struct stat     st;
	const char     *fullpath;
	bool skipped = false;

	fullpath = path;
	if (*path == '/')
		path++;

	dirfd = openat(rootfd, path, O_DIRECTORY);
	if (dirfd == -1) {
		return (true);
	}

	d = fdopendir(dirfd);
	if (d == NULL) {
		(void)close(dirfd);
		return (true);
	}
	while ((e = readdir(d)) != NULL) {
		if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
			continue;

		if (fstatat(dirfd, e->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0)
			continue;
		if (S_ISDIR(st.st_mode)) {
			if (rm_r(dirfd, e->d_name, uid) == true)
				skipped = true;
		} else if (S_ISLNK(st.st_mode) || st.st_uid == uid)
			unlinkat(dirfd, e->d_name, 0);
		else
			skipped = true;
	}
	closedir(d);
	if (fstatat(rootfd, path, &st, AT_SYMLINK_NOFOLLOW) != 0)
		return (skipped);
	if (S_ISLNK(st.st_mode)) {
		if (unlinkat(rootfd, path, 0) == -1)
			skipped = true;
	} else if (st.st_uid == uid) {
		if (unlinkat(rootfd, path, AT_REMOVEDIR) == -1) {
			if (errno == EBUSY && skipped == false)
				skipped = try_dataset_remove(fullpath);
			else
				skipped = true;
		}
	} else
		skipped = true;

	return (skipped);
}

/*
 * If the home directory is a ZFS dataset, attempt to destroy it.
 * Return true if the dataset is not destroyed.
 * This would be more straightforward as a shell script.
 */
static bool
try_dataset_remove(const char *path)
{
	bool skipped = true;
	struct statfs stat;
	const char *argv[] = {
		"/sbin/zfs",
		"destroy",
		NULL,
		NULL
	};
	int status;
	pid_t pid;

	/* see if this is an absolute path (top-level directory) */
	if (*path != '/')
		return (skipped);
	/* see if ZFS is loaded */
	if (kld_isloaded("zfs") == 0)
		return (skipped);
	/* This won't work if root dir is not / (-R option) */
	if (strcmp(conf.rootdir, "/") != 0) {
		warnx("cannot destroy home dataset when -R was used");
		return (skipped);
	}
	/* if so, find dataset name */
	if (statfs(path, &stat) != 0) {
		warn("statfs %s", path);
		return (skipped);
	}
	/*
	 * Check that the path refers to the dataset itself,
	 * not a subdirectory.
	 */
	if (strcmp(stat.f_mntonname, path) != 0)
		return (skipped);
	argv[2] = stat.f_mntfromname;
	if ((skipped = posix_spawn(&pid, argv[0], NULL, NULL,
	    (char *const *) argv, environ)) != 0) {
		warn("Failed to execute '%s %s %s'",
		    argv[0], argv[1], argv[2]);
	} else {
		if (waitpid(pid, &status, 0) != -1 && status != 0) {
			warnx("'%s %s %s' exit status %d\n",
			    argv[0], argv[1], argv[2], status);
		}
	}
	return (skipped);
}
