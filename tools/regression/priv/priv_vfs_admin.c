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
 * Check that privilege is required for a variety of administrative
 * activities on a file owned by another user.  Admin privileges are required
 * for the following services:
 *
 * - Set file flags.
 * - Set utimes to non-NULL.
 * - Set file mode.
 * - Set file ownership.
 * - Remove a file from a sticky directory. (XXXRW: Not tested here.)
 * - Set the ACL on a file.  (XXXRW: Not tested here.)
 * - Delete the ACL on a file.  (XXXRW: Not tested here.)
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "main.h"

static u_long
getflags(char *fpathp)
{
	struct stat sb;

	if (stat(fpathp, &sb) < 0)
		err(-1, "stat(%s)", fpathp);

	return (sb.st_flags);
}

static void
priv_vfs_admin_chflags(void)
{
	char fpath[1024];
	u_long flags;
	int error;

	/*
	 * Test that setting file flags works as and not as the file owner
	 * when running with privilege, but only as the file owner when
	 * running without privilege.
	 */
	setup_file(fpath, UID_ROOT, GID_WHEEL, 0600);
	flags = getflags(fpath);
	flags |= UF_NODUMP;
	if (chflags(fpath, flags) < 0) {
		warn("chflags(%s, UF_NODUMP) owner as root", fpath);
		goto out;
	}
	(void)unlink(fpath);

	setup_file(fpath, UID_OWNER, GID_OWNER, 0600);
	flags = getflags(fpath);
	flags |= UF_NODUMP;
	if (chflags(fpath, flags) < 0) {
		warn("chflags(%s, UF_NODUMP) !owner as root", fpath);
		goto out;
	}
	(void)unlink(fpath);

	setup_file(fpath, UID_OWNER, GID_OWNER, 0600);
	flags = getflags(fpath);
	flags |= UF_NODUMP;
	set_euid(UID_OWNER);
	if (chflags(fpath, flags) < 0) {
		warn("chflags(%s, UF_NODUMP) owner as !root", fpath);
		goto out;
	}
	set_euid(UID_ROOT);
	(void)unlink(fpath);

	setup_file(fpath, UID_OWNER, GID_OWNER, 0600);
	flags = getflags(fpath);
	flags |= UF_NODUMP;
	set_euid(UID_OTHER);
	error = chflags(fpath, flags);
	if (error == 0) {
		warnx("chflags(%s, UF_NODUMP) succeeded !owner as !root",
		    fpath);
		goto out;
	}
	if (errno != EPERM) {
		warn("chflags(%s, UF_NODUMP) wrong errno %d !owner a !root",
		    fpath, errno);
		goto out;
	}
	set_euid(UID_ROOT);
	(void)unlink(fpath);

out:
	(void)seteuid(UID_ROOT);
	(void)unlink(fpath);
}

static void
priv_vfs_admin_utimes(void)
{
	struct timeval tv[2];
	char fpath[1024];
	int error;

	/*
	 * Actual values don't matter here.
	 */
	tv[0].tv_sec = 0;
	tv[0].tv_usec = 0;
	tv[1].tv_sec = 0;
	tv[1].tv_usec = 0;

	/*
	 * When using a non-NULL argument to utimes(), must either hold
	 * privilege or be the file owner.  Check all four possible
	 * combinations of privilege, ownership.
	 */
	setup_file(fpath, UID_ROOT, GID_WHEEL, 0600);
	if (utimes(fpath, tv) < 0) {
		warn("utimes(%s, !NULL) owner as root", fpath);
		goto out;
	}
	(void)unlink(fpath);

	setup_file(fpath, UID_OWNER, GID_OWNER, 0600);
	if (utimes(fpath, tv) < 0) {
		warn("utimes(%s, !NULL) !owner as root", fpath);
		goto out;
	}
	(void)unlink(fpath);

	setup_file(fpath, UID_OWNER, GID_OWNER, 0600);
	set_euid(UID_OWNER);
	if (utimes(fpath, tv) < 0) {
		warn("utimes(%s, !NULL) owner as !root", fpath);
		goto out;
	}
	set_euid(UID_ROOT);
	(void)unlink(fpath);

	setup_file(fpath, UID_OWNER, GID_OWNER, 0600);
	set_euid(UID_OTHER);
	error = utimes(fpath, tv);
	if (error == 0) {
		warnx("utimes(%s, !NULL) succeeded !owner as !root",
		    fpath);
		goto out;
	}
	if (errno != EPERM) {
		warn("utimes(%s, !NULL) wrong errno %d !owner a !root",
		    fpath, errno);
		goto out;
	}
	set_euid(UID_ROOT);
	(void)unlink(fpath);

out:
	(void)seteuid(UID_ROOT);
	(void)unlink(fpath);
}

static void
priv_vfs_admin_chmod(void)
{
	char fpath[1024];
	int error;

	/*
	 * Test that setting file permissions works either as file owner or
	 * not when running with privilege, but only as file owner when
	* running without privilege.
	 */
	setup_file(fpath, UID_ROOT, GID_WHEEL, 0600);
	if (chmod(fpath, 0640) < 0) {
		warn("chmod(%s, 0640) owner as root", fpath);
		goto out;
	}
	(void)unlink(fpath);

	setup_file(fpath, UID_OWNER, GID_OWNER, 0600);
	if (chmod(fpath, 0640) < 0) {
		warn("chmod(%s, 0640) !owner as root", fpath);
		goto out;
	}
	(void)unlink(fpath);

	setup_file(fpath, UID_OWNER, GID_OWNER, 0600);
	set_euid(UID_OWNER);
	if (chmod(fpath, 0640) < 0) {
		warn("chmod(%s, 0640) owner as !root", fpath);
		goto out;
	}
	set_euid(UID_ROOT);
	(void)unlink(fpath);

	setup_file(fpath, UID_OWNER, GID_OWNER, 0600);
	set_euid(UID_OTHER);
	error = chmod(fpath, 0640);
	if (error == 0) {
		warnx("chmod(%s, 0640) succeeded !owner as !root",
		    fpath);
		goto out;
	}
	if (errno != EPERM) {
		warn("chmod(%s, 0640) wrong errno %d !owner a !root",
		    fpath, errno);
		goto out;
	}
	set_euid(UID_ROOT);
	(void)unlink(fpath);

out:
	(void)seteuid(UID_ROOT);
	(void)unlink(fpath);
}

static const gid_t gidset[] = { GID_WHEEL, GID_OWNER, GID_OTHER};

static void
priv_vfs_admin_chown(void)
{
	char fpath[1024];
	int error;

	/*
	 * Test that the group of the file can only be changed with privilege
	 * or as the owner.  These test is run last as it frobs the group
	 * context.  We change the file group from one group we're in to
	 * another we're in to avoid any other access control checks failing.
	 */
	if (setgroups(3, gidset) < 0)
		err(-1, "priv_vfs_admin_chown:setgroups(3, {%d, %d, %d})",
		    GID_WHEEL, GID_OWNER, GID_OTHER);

	/*
	 * Test that setting file permissions works either as file owner or
	 * not when running with privilege, but only as file owner when
	* running without privilege.
	 */
	setup_file(fpath, UID_ROOT, GID_WHEEL, 0600);
	if (chown(fpath, -1, GID_OWNER) < 0) {
		warn("chown(%s, %d) owner as root", fpath, GID_OWNER);
		goto out;
	}
	(void)unlink(fpath);

	setup_file(fpath, UID_OWNER, GID_OWNER, 0600);
	if (chown(fpath, -1, GID_OWNER) < 0) {
		warn("chown(%s, %d) !owner as root", fpath, GID_OWNER);
		goto out;
	}
	(void)unlink(fpath);

	setup_file(fpath, UID_OWNER, GID_OWNER, 0600);
	set_euid(UID_OWNER);
	if (chown(fpath, -1, GID_OWNER) < 0) {
		warn("chown(%s, %d) owner as !root", fpath, GID_OWNER);
		goto out;
	}
	set_euid(UID_ROOT);
	(void)unlink(fpath);

	setup_file(fpath, UID_OWNER, GID_OWNER, 0600);
	set_euid(UID_OTHER);
	error = chown(fpath, -1, GID_OWNER);
	if (error == 0) {
		warnx("chown(%s, %d) succeeded !owner as !root",
		    fpath, GID_OWNER);
		goto out;
	}
	if (errno != EPERM) {
		warn("chown(%s, %d) wrong errno %d !owner a !root",
		    fpath, GID_OWNER, errno);
		goto out;
	}
	set_euid(UID_ROOT);
	(void)unlink(fpath);

out:
	(void)seteuid(UID_ROOT);
	(void)unlink(fpath);
}

void
priv_vfs_admin(void)
{

	assert_root();

	priv_vfs_admin_chflags();
	priv_vfs_admin_utimes();
	priv_vfs_admin_chmod();
	priv_vfs_admin_chown();	/* Run this last. */
}
