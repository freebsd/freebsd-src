/*-
 * Copyright (c) 2006 nCircle Network Security, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert N. M. Watson for the TrustedBSD
 * Project under contract to nCircle Network Security, Inc.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR, NCIRCLE NETWORK SECURITY,
 * INC., OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * There are three cases in which the file system will clear the setuid or
 * setgid bits on a file when running as !root:
 *
 * - When the file is chown()'d and either of the uid or the gid is changed.
 *
 * - The file is written to succeesfully.
 *
 * - An extended attribute of the file is written to successfully.
 *
 * Test each case first as root (that flags aren't cleared), and then as
 * !root, to check they are cleared.
 */

#include <sys/types.h>
#include <sys/extattr.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "main.h"

static const gid_t gidset[] = {GID_WHEEL, GID_OWNER, GID_OTHER};

/*
 * Confirm that the setuid bit is set on a file.  Don't return on failure.
 */
static void
confirm_setuid(char *fpathp, char *test_case)
{
	struct stat sb;

	if (stat(fpathp, &sb) < 0) {
		warn("%s stat(%s)", test_case, fpathp);
		(void)seteuid(UID_ROOT);
		(void)unlink(fpathp);
		exit(-1);
	}
	if (!(sb.st_mode & S_ISUID)) {
		warnx("case %s stat(%s) not setuid", test_case, fpathp);
		(void)seteuid(UID_ROOT);
		(void)unlink(fpathp);
		exit(-1);
	}
}

/*
 * Confirm that the setuid bit is not set on a file.  Don't return on failure.
 */
static void
confirm_notsetuid(char *fpathp, char *test_case)
{
	struct stat sb;

	if (stat(fpathp, &sb) < 0) {
		warn("%s stat(%s)", test_case, fpathp);
		(void)seteuid(UID_ROOT);
		(void)unlink(fpathp);
		exit(-1);
	}
	if (sb.st_mode & S_ISUID) {
		warnx("case %s stat(%s) is setuid", test_case, fpathp);
		(void)seteuid(UID_ROOT);
		(void)unlink(fpathp);
		exit(-1);
	}
}

#define	EA_NAMESPACE	EXTATTR_NAMESPACE_USER
#define	EA_NAME		"clearsugid"
#define	EA_DATA		"test"
#define	EA_SIZE		(strlen(EA_DATA))
void
priv_vfs_clearsugid(void)
{
	char ch, fpath[1024];
	int fd;

	assert_root();

	/*
	 * Before starting on work, set up group IDs so that the process can
	 * change the group ID of the file without privilege, in order to see
	 * the effects.  That way privilege is only required to maintain the
	 * setuid bit.  For the chown() test, we change only the group id, as
	 * that can be done with or without privilege.
	 */
	if (setgroups(3, gidset) < 0)
		err(-1, "setgroups(2, {%d, %d})", GID_WHEEL, GID_OWNER);

	/*
	 * chown() with privilege.
	 */
	setup_file(fpath, UID_ROOT, GID_WHEEL, 0600 | S_ISUID);
	if (chown(fpath, -1, GID_OTHER) < 0)
		warn("chown(%s, -1, %d) as root", fpath, GID_OTHER);
	confirm_setuid(fpath, "chown as root");
	(void)unlink(fpath);

	/*
	 * write() with privilege.
	 */
	setup_file(fpath, UID_ROOT, GID_WHEEL, 0600 | S_ISUID);
	fd = open(fpath, O_RDWR);
	if (fd < 0) {
		warn("open(%s) as root", fpath);
		goto out;
	}
	ch = 0;
	if (write(fd, &ch, sizeof(ch)) < 0) {
		warn("write(%s) as root", fpath);
		goto out;
	}
	close(fd);
	confirm_setuid(fpath, "write as root");
	(void)unlink(fpath);

	/*
	 * extwrite() with privilege.
	 */
	setup_file(fpath, UID_ROOT, GID_WHEEL, 0600 | S_ISUID);
	if (extattr_set_file(fpath, EA_NAMESPACE, EA_NAME, EA_DATA, EA_SIZE)
	    < 0) {
		warn("extattr_set_file(%s, user, %s, %s, %d) as root",
		    fpath, EA_NAME, EA_DATA, EA_SIZE);
		goto out;
	}
	confirm_setuid(fpath, "extwrite as root");
	(void)unlink(fpath);

	/*
	 * chown() without privilege.
	 */
	setup_file(fpath, UID_OWNER, GID_OWNER, 0600 | S_ISUID);
	set_euid(UID_OWNER);
	if (chown(fpath, -1, GID_OTHER) < 0)
		warn("chown(%s, -1, %d) as !root", fpath, GID_OTHER);
	set_euid(UID_ROOT);
	confirm_notsetuid(fpath, "chown as !root");
	(void)unlink(fpath);

	/*
	 * write() without privilege.
	 */
	setup_file(fpath, UID_OWNER, GID_OWNER, 0600 | S_ISUID);
	set_euid(UID_OWNER);
	fd = open(fpath, O_RDWR);
	if (fd < 0) {
		warn("open(%s) as !root", fpath);
		goto out;
	}
	ch = 0;
	if (write(fd, &ch, sizeof(ch)) < 0) {
		warn("write(%s) as !root", fpath);
		goto out;
	}
	close(fd);
	set_euid(UID_ROOT);
	confirm_notsetuid(fpath, "write as !root");
	(void)unlink(fpath);

	/*
	 * extwrite() without privilege.
	 */
	setup_file(fpath, UID_OWNER, GID_OWNER, 0600 | S_ISUID);
	set_euid(UID_OWNER);
	if (extattr_set_file(fpath, EA_NAMESPACE, EA_NAME, EA_DATA, EA_SIZE)
	    < 0) {
		warn("extattr_set_file(%s, user, %s, %s, %d) as !root",
		    fpath, EA_NAME, EA_DATA, EA_SIZE);
		goto out;
	}
	set_euid(UID_ROOT);
	confirm_notsetuid(fpath, "extwrite as !root");
	(void)unlink(fpath);

out:
	(void)seteuid(UID_ROOT);
	(void)unlink(fpath);
}
