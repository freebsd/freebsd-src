/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Beckhoff Automation GmbH & Co. KG
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

#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#define	PAM_SM_SESSION

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_mod_misc.h>

#define	BASE_RUNTIME_DIR_PREFIX	"/var/run/xdg"
#define	RUNTIME_DIR_PREFIX	runtime_dir_prefix != NULL ? runtime_dir_prefix : BASE_RUNTIME_DIR_PREFIX

#define	RUNTIME_DIR_PREFIX_MODE	0711
#define	RUNTIME_DIR_MODE	0700	/* XDG spec */

#define	XDG_MAX_SESSION		100 /* Arbitrary limit because we need one */

static int
_pam_xdg_open(pam_handle_t *pamh, int flags __unused,
    int argc __unused, const char *argv[] __unused)
{
	struct passwd *passwd;
	const char *user;
	const char *runtime_dir_prefix;
	struct stat sb;
	char *runtime_dir = NULL;
	char *xdg_session_file;
	int rv, rt_dir_prefix, rt_dir, session_file, i;

	session_file = -1;
	rt_dir_prefix = -1;
	runtime_dir_prefix = openpam_get_option(pamh, "runtime_dir_prefix");

	/* Get user info */
	rv = pam_get_item(pamh, PAM_USER, (const void **)&user);
	if (rv != PAM_SUCCESS) {
		PAM_VERBOSE_ERROR("Can't get user information");
		goto out;
	}
	if ((passwd = getpwnam(user)) == NULL) {
		PAM_VERBOSE_ERROR("Can't get user information");
		rv = PAM_SESSION_ERR;
		goto out;
	}

	/* Open or create the base xdg directory */
	rt_dir_prefix = open(RUNTIME_DIR_PREFIX, O_DIRECTORY | O_NOFOLLOW);
	if (rt_dir_prefix < 0) {
		rt_dir_prefix = mkdir(RUNTIME_DIR_PREFIX, RUNTIME_DIR_PREFIX_MODE);
		if (rt_dir_prefix != 0) {
			PAM_VERBOSE_ERROR("Can't mkdir %s", RUNTIME_DIR_PREFIX);
			rv = PAM_SESSION_ERR;
			goto out;
		}
		rt_dir_prefix = open(RUNTIME_DIR_PREFIX, O_DIRECTORY | O_NOFOLLOW);
	}

	/* Open or create the user xdg directory */
	rt_dir = openat(rt_dir_prefix, user, O_DIRECTORY | O_NOFOLLOW);
	if (rt_dir < 0) {
		rt_dir = mkdirat(rt_dir_prefix, user, RUNTIME_DIR_MODE);
		if (rt_dir != 0) {
			PAM_VERBOSE_ERROR("mkdir: %s/%s (%d)", RUNTIME_DIR_PREFIX, user, rt_dir);
			rv = PAM_SESSION_ERR;
			goto out;
		}
		rv = fchownat(rt_dir_prefix, user, passwd->pw_uid, passwd->pw_gid, 0);
		if (rv != 0) {
			PAM_VERBOSE_ERROR("fchownat: %s/%s (%d)", RUNTIME_DIR_PREFIX, user, rv);
			rv = unlinkat(rt_dir_prefix, user, AT_REMOVEDIR);
			if (rv == -1)
				PAM_VERBOSE_ERROR("unlinkat: %s/%s (%d)", RUNTIME_DIR_PREFIX, user, errno);
			rv = PAM_SESSION_ERR;
			goto out;
		}
	} else {
		/* Check that the already create dir is correctly owned */
		rv = fstatat(rt_dir_prefix, user, &sb, 0);
		if (rv == -1) {
			PAM_VERBOSE_ERROR("fstatat %s/%s failed (%d)", RUNTIME_DIR_PREFIX, user, errno);
			rv = PAM_SESSION_ERR;
			goto out;
		}
		if (sb.st_uid != passwd->pw_uid ||
		  sb.st_gid != passwd->pw_gid) {
			PAM_VERBOSE_ERROR("%s/%s isn't owned by %d:%d\n", RUNTIME_DIR_PREFIX, user, passwd->pw_uid, passwd->pw_gid);
			rv = PAM_SESSION_ERR;
			goto out;
		}
		/* Test directory mode */
		if ((sb.st_mode & 0x1FF) != RUNTIME_DIR_MODE) {
			PAM_VERBOSE_ERROR("%s/%s have wrong mode\n", RUNTIME_DIR_PREFIX, user);
			rv = PAM_SESSION_ERR;
			goto out;
		}
	}

	/* Setup the environment variable */
	rv = asprintf(&runtime_dir, "XDG_RUNTIME_DIR=%s/%s", RUNTIME_DIR_PREFIX, user);
	if (rv < 0) {
		PAM_VERBOSE_ERROR("asprintf failed %d\n", rv);
		rv = PAM_SESSION_ERR;
		goto out;
	}
	rv = pam_putenv(pamh, runtime_dir);
	if (rv != PAM_SUCCESS) {
		PAM_VERBOSE_ERROR("pam_putenv: failed (%d)", rv);
		rv = PAM_SESSION_ERR;
		goto out;
	}

	/* Setup the session count file */
	for (i = 0; i < XDG_MAX_SESSION; i++) {
		rv = asprintf(&xdg_session_file, "%s/xdg_session.%d", user, i);
		if (rv < 0) {
			PAM_VERBOSE_ERROR("asprintf failed %d\n", rv);
			rv = PAM_SESSION_ERR;
			goto out;
		}
		session_file = openat(rt_dir_prefix, xdg_session_file, O_CREAT | O_EXCL, RUNTIME_DIR_MODE);
		free(xdg_session_file);
		if (session_file >= 0)
			break;
	}
	if (session_file < 0) {
		PAM_VERBOSE_ERROR("Too many sessions");
		rv = PAM_SESSION_ERR;
		goto out;
	}

out:
	if (session_file >= 0)
		close(session_file);
	if (rt_dir_prefix >= 0)
		close(rt_dir_prefix);

	if (runtime_dir)
		free(runtime_dir);
	return (rv);
}

static int
remove_dir(int fd)
{
	DIR *dirp;
	struct dirent *dp;

	dirp = fdopendir(fd);
	if (dirp == NULL)
		return (-1);

	while ((dp = readdir(dirp)) != NULL) {
		if (dp->d_type == DT_DIR) {
			int dirfd;

			if (strcmp(dp->d_name, ".") == 0 ||
			    strcmp(dp->d_name, "..") == 0)
				continue;
			dirfd = openat(fd, dp->d_name, 0);
			remove_dir(dirfd);
			close(dirfd);
			unlinkat(fd, dp->d_name, AT_REMOVEDIR);
			continue;
		}
		unlinkat(fd, dp->d_name, 0);
	}

	return (0);
}

static int
_pam_xdg_close(pam_handle_t *pamh __unused, int flags __unused,
    int argc __unused, const char *argv[] __unused)
{
	struct passwd *passwd;
	const char *user;
	const char *runtime_dir_prefix;
	struct stat sb;
	char *xdg_session_file;
	int rv, rt_dir_prefix, rt_dir, session_file, i;

	rt_dir = -1;
	rt_dir_prefix = -1;
	runtime_dir_prefix = openpam_get_option(pamh, "runtime_dir_prefix");

	/* Get user info */
	rv = pam_get_item(pamh, PAM_USER, (const void **)&user);
	if (rv != PAM_SUCCESS) {
		PAM_VERBOSE_ERROR("Can't get user information");
		goto out;
	}
	if ((passwd = getpwnam(user)) == NULL) {
		PAM_VERBOSE_ERROR("Can't get user information");
		rv = PAM_SESSION_ERR;
		goto out;
	}

	/* Open the xdg base directory */
	rt_dir_prefix = open(RUNTIME_DIR_PREFIX, O_DIRECTORY | O_NOFOLLOW);
	if (rt_dir_prefix < 0) {
		PAM_VERBOSE_ERROR("open: %s failed (%d)\n", runtime_dir_prefix, rt_dir_prefix);
		rv = PAM_SESSION_ERR;
		goto out;
	}
	/* Check that the already created dir is correctly owned */
	rv = fstatat(rt_dir_prefix, user, &sb, 0);
	if (rv == -1) {
		PAM_VERBOSE_ERROR("fstatat %s/%s failed (%d)", RUNTIME_DIR_PREFIX, user, errno);
		rv = PAM_SESSION_ERR;
		goto out;
	}
	if (sb.st_uid != passwd->pw_uid ||
	    sb.st_gid != passwd->pw_gid) {
		PAM_VERBOSE_ERROR("%s/%s isn't owned by %d:%d\n", RUNTIME_DIR_PREFIX, user, passwd->pw_uid, passwd->pw_gid);
		rv = PAM_SESSION_ERR;
		goto out;
	}
	/* Test directory mode */
	if ((sb.st_mode & 0x1FF) != RUNTIME_DIR_MODE) {
		PAM_VERBOSE_ERROR("%s/%s have wrong mode\n", RUNTIME_DIR_PREFIX, user);
		rv = PAM_SESSION_ERR;
		goto out;
	}

	/* Open the user xdg directory */
	rt_dir = openat(rt_dir_prefix, user, O_DIRECTORY | O_NOFOLLOW);
	if (rt_dir < 0) {
		PAM_VERBOSE_ERROR("openat: %s/%s failed (%d)\n", RUNTIME_DIR_PREFIX, user, rt_dir_prefix);
		rv = PAM_SESSION_ERR;
		goto out;
	}

	/* Get the last session file created */
	for (i = XDG_MAX_SESSION; i >= 0; i--) {
		rv = asprintf(&xdg_session_file, "%s/xdg_session.%d", user, i);
		if (rv < 0) {
			PAM_VERBOSE_ERROR("asprintf failed %d\n", rv);
			rv = PAM_SESSION_ERR;
			goto out;
		}
		session_file = openat(rt_dir_prefix, xdg_session_file, 0);
		if (session_file >= 0) {
			unlinkat(rt_dir_prefix, xdg_session_file, 0);
			free(xdg_session_file);
			break;
		}
		free(xdg_session_file);
	}
	if (session_file < 0) {
		PAM_VERBOSE_ERROR("Can't find session number\n");
		rv = PAM_SESSION_ERR;
		goto out;
	}
	close(session_file);

	/* Final cleanup if last user session */
	if (i == 0) {
		remove_dir(rt_dir);
		if (unlinkat(rt_dir_prefix, user, AT_REMOVEDIR) != 0) {
			PAM_VERBOSE_ERROR("Can't cleanup %s/%s\n", runtime_dir_prefix, user);
			rv = PAM_SESSION_ERR;
			goto out;
		}
	}

	rv = PAM_SUCCESS;
out:
	if (rt_dir >= 0)
		close(rt_dir);
	if (rt_dir_prefix >= 0)
		close(rt_dir_prefix);
	return (rv);
}

PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{

	return (_pam_xdg_open(pamh, flags, argc, argv));
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{

	return (_pam_xdg_close(pamh, flags, argc, argv));
}

PAM_MODULE_ENTRY("pam_xdg");
