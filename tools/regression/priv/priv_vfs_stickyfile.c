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
 * Check that privilege is required to set the sticky bit on a file, but not
 * a directory.  Try with and without privilege.
 */

#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <unistd.h>

#include "main.h"

static void
cleanup(const char *fpath, const char *dpath)
{

	(void)seteuid(UID_ROOT);
	(void)unlink(fpath);
	if (dpath != NULL)
		(void)rmdir(dpath);
}

void
priv_vfs_stickyfile(void)
{
	char fpath[1024] = "/tmp/stickyfile.XXXXXXXXXXX";
	char dpath[1024] = "/tmp/stickyfile.XXXXXXXXXXX", *dpathp;
	int error, fd;

	assert_root();

	fd = mkstemp(fpath);
	if (fd < 0)
		err(-1, "mkstemp");

	dpathp = mkdtemp(dpath);
	if (dpathp == NULL) {
		warn("mkdtemp");
		goto out;
	}

	/*
	 * First, with privilege, set and clear the sticky bit on the file
	 * and directory.
	 */
	if (fchmod(fd, 0600 | S_ISTXT) < 0) {
		warn("fchmod(%s, 0600 | S_ISTXT) on file as root", fpath);
		goto out;
	}

	if (chmod(dpathp, 0700 | S_ISTXT) < 0) {
		warn("chmod(%s, 0600 | S_ISTXT) on dir as root", dpath);
		goto out;
	}

	/*
	 * Reset to remove sticky bit before changing credential.
	 */
	if (fchmod(fd, 0600) < 0) {
		warn("fchmod(%s, 0600) on file as root", fpath);
		goto out;
	}

	if (chmod(dpath, 0700) < 0) {
		warn("chmod(%s, 0600) on dir as root", dpath);
		goto out;
	}

	/*
	 * Chown the file and directory to target user -- we're checking for
	 * the specific right to set the sticky bit, not the general right to
	 * chmod().
	 */
	if (fchown(fd, UID_OTHER, -1) < 0) {
		warn("fchown(%s, %d, -1)", fpath, UID_OTHER);
		goto out;
	}

	if (chown(dpath, UID_OTHER, -1) < 0) {
		warn("chown(%s, %d, -1)", fpath, UID_OTHER);
		goto out;
	}

	/*
	 * Change credential and try again.
	 */
	set_euid(UID_OTHER);

	error = fchmod(fd, 0600 | S_ISTXT);
	if (error == 0) {
		warnx("fchmod(%s, 0600 | S_ISTXT) succeeded on file as "
		    "!root", fpath);
		goto out;
	}
	if (errno != EFTYPE) {
		warn("fchmod(%s, 0600 | S_ISTXT) wrong errno %d as !root",
		    fpath, errno);
		goto out;
	}

	if (chmod(dpathp, 0700 | S_ISTXT) < 0) {
		warn("chmod(%s, 0600 | S_ISTXT) on dir as !root", dpath);
		goto out;
	}
out:
	setuid(UID_ROOT);
	cleanup(fpath, dpathp);
}
