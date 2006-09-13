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
 * Test that privilege is required to set the sgid bit on a file with a group
 * that isn't in the process credential.  The file uid owner is set to the
 * uid being tested with, as we are not interested in testing privileges
 * associated with file ownership.
 */

#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "main.h"

static const gid_t gidset_without[] = {GID_WHEEL};
static const gid_t gidset_with[] = {GID_WHEEL, GID_OWNER};

void
priv_vfs_setgid(void)
{
	char fpath[1024];
	int error, fd;

	assert_root();

	setup_file(fpath, UID_ROOT, GID_OWNER, 0644);

	if (setgroups(1, gidset_without) < 0) {
		warn("setgroups(1, {%d})", gidset_without[0]);
		goto out;
	}

	fd = open(fpath, O_RDWR);
	if (fd < 0) {
		warn("open(%s, O_RDWR)", fpath);
		goto out;
	}

	/*
	 * With privilege, set mode on file.
	 */
	if (fchmod(fd, 0600 | S_ISGID) < 0) {
		warn("fchmod(%s, 0600 | S_ISGID) as root", fpath);
		goto out;
	}

	/*
	 * Reset mode and chown file before dropping privilege.
	 */
	if (fchmod(fd, 0600) < 0) {
		warn("fchmod(%s, 0600) as root", fpath);
		goto out;
	}

	if (fchown(fd, UID_OWNER, GID_OWNER) < 0) {
		warn("fchown(%s, %d, %d) as root", fpath, UID_OWNER,
		    GID_OTHER);
		goto out;
	}

	/*
	 * Drop privilege.
	 */
	set_euid(UID_OWNER);

	/*
	 * Without privilege, set mode on file.
	 */
	error = fchmod(fd, 0600 | S_ISGID);
	if (error == 0) {
		warnx("fchmod(%s, 0600 | S_ISGID) succeeded as !root",
		    fpath);
		goto out;
	}
	if (errno != EPERM) {
		warn("fchmod(%s, 0600 | S_ISGID) wrong errno %d as !root",
		    fpath, errno);
		goto out;
	}

	/*
	 * Turn privilege back on so that we confirm privilege isn't required
	 * if we are a group member of the file's group.
	 */
	set_euid(UID_ROOT);

	if (setgroups(2, gidset_with) < 0) {
		warn("setgroups(2, {%d, %d})", gidset_with[0],
		    gidset_with[1]);
		goto out;
	}

	if (seteuid(UID_OWNER) < 0) {
		warn("seteuid(%d) pass 2", UID_OWNER);
		goto out;
	}

	/*
	 * Without privilege, set mode on file (this time with right gid).
	 */
	if (fchmod(fd, 0600 | S_ISGID) < 0) {
		warnx("fchmod(%s, 0600 | S_ISGID) pass 2 as !root", fpath);
		sleep(10);
		goto out;
	}

out:
	seteuid(UID_ROOT);
	(void)unlink(fpath);
}
