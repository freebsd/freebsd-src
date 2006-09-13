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
 * Confirm that privilege is required in the cases using chown():
 *
 * - If the process euid does not match the file uid.
 *
 * - If the target uid is different than the current uid.
 *
 * - If the target gid changes and we the process is not a member of the new
 *   group.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "main.h"

const gid_t gidset[] = {GID_WHEEL, GID_OWNER};

void
priv_vfs_chown(void)
{
	char fpath[1024];
	int error;

	assert_root();

	/*
	 * Before beginning, set up group set for process.  Place in wheel
	 * and owner groups; don't put in other group so that when we chown
	 * to the other group, it's as a non-member.
	 */
	if (setgroups(2, gidset) < 0)
		err(-1, "setgroups(2, {%d, %d})", GID_WHEEL, GID_OWNER);

	/*
	 * In the first pass, confirm that all works as desired with
	 * privilege.
	 *
	 * Check that chown when non-owner works fine.  Do a no-op change to
	 * avoid other permission checks.  Note that we can't request
	 * (-1, -1) and get an access control check, we have to request
	 * specific uid/gid that are not the same.
	 */
	setup_file(fpath, UID_OWNER, GID_OWNER, 0600);
	if (chown(fpath, -1, GID_OWNER) < 0) {
		warn("chown(%s, -1, %d) as root", fpath, GID_OWNER);
		goto out;
	}
	(void)unlink(fpath);

	/*
	 * Check that chown changing uid works with privilege.
	 */
	setup_file(fpath, UID_OWNER, GID_OWNER, 0600);
	if (chown(fpath, UID_OTHER, -1) < 0) {
		warn("chown(%s, %d, -1) as root", fpath, UID_OTHER);
		goto out;
	}
	(void)unlink(fpath);

	/*
	 * Check that can change the file group to one we are not a member of
	 * when running with privilege.
	 */
	setup_file(fpath, UID_OWNER, GID_OWNER, 0600);
	if (chown(fpath, -1, GID_OTHER) < 0) {
		warn("chown(%s, -1, %d) as root", fpath, GID_OTHER);
		goto out;
	}
	(void)unlink(fpath);

	/*
	 * Now, the same again, but without privilege.
	 *
	 * Confirm that we can't chown a file we don't own, even as a no-op.
	 */
	setup_file(fpath, UID_OWNER, GID_OWNER, 0600);
	set_euid(UID_OTHER);
	error = chown(fpath, -1, GID_OWNER);
	if (error == 0) {
		warnx("chown(%s, -1, %d) succeeded as !root, non-owner",
		    fpath, GID_OWNER);
		goto out;
	}
	if (errno != EPERM) {
		warn("chown(%s, -1, %d) wrong errno %d as !root, non-owner",
		    fpath, GID_OWNER, errno);
		goto out;
	}
	set_euid(UID_ROOT);
	(void)unlink(fpath);

	/*
	 * Check that we can't change the uid of the file without privilege,
	 * even though we own the file.
	 */
	setup_file(fpath, UID_OWNER, GID_OWNER, 0600);
	set_euid(UID_OWNER);
	error = chown(fpath, UID_OTHER, -1);
	if (error == 0) {
		warnx("chown(%s, %d, -1) succeeded as !root", fpath,
		    UID_OTHER);
		goto out;
	}
	if (errno != EPERM) {
		warn("chown(%s, %d, -1) wrong errno %d as !root", fpath,
		    UID_OTHER, errno);
		goto out;
	}
	set_euid(UID_ROOT);
	(void)unlink(fpath);
	
	/*
	 * Check that can't change the file group to one we are not a member
	 * of when running without privilege.
	 */
	setup_file(fpath, UID_OWNER, GID_OWNER, 0600);
	set_euid(UID_OWNER);
	error = chown(fpath, -1, GID_OTHER);
	if (error == 0) {
		warn("chown(%s, -1, %d) succeeded as !root", fpath, GID_OTHER);
		goto out;
	}
	if (errno != EPERM) {
		warn("chown(%s, -1, %d) wrong errno %d as !root", fpath,
		    errno, GID_OTHER);
		goto out;
	}
	set_euid(UID_ROOT);
	(void)unlink(fpath);
out:
	(void)seteuid(UID_ROOT);
	(void)unlink(fpath);
}
