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
 * Test privilege check on /dev/io.  By default, the permissions also protect
 * against non-superuser access, so this program will modify permissions on
 * /dev/io to allow group access for the wheel group, and revert the change
 * on exit.  This is not good for run-time security, but is necessary to test
 * the checks properly.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "main.h"

#define	NEW_PERMS	0660
#define	DEV_IO		"/dev/io"
#define	EXPECTED_PERMS	0600

static mode_t	saved_perms;

static void
save_perms(void)
{
	struct stat sb;

	if (stat(DEV_IO, &sb) < 0)
		err(-1, "save_perms: stat(%s)", DEV_IO);

	saved_perms = sb.st_mode & ALLPERMS;

	if (saved_perms != EXPECTED_PERMS)
		err(-1, "save_perms: perms = 0%o; expected 0%o", saved_perms,
		    EXPECTED_PERMS);

}

static void
set_perms(void)
{

	if (chmod(DEV_IO, NEW_PERMS) < 0)
		err(-1, "set_perms: chmod(%s, 0%o)", DEV_IO, NEW_PERMS);
}

static void
restore_perms(void)
{

	if (chmod(DEV_IO, saved_perms) < 0)
		err(-1, "restore_perms: chmod(%s, 0%o)", DEV_IO, saved_perms);
}

static void
try_open(const char *test_case, uid_t uid, int expected)
{
	int fd;

	set_euid(uid);
	fd = open(DEV_IO, O_RDONLY);
	if (expected == 0) {
		if (fd == -1) {
			warn("try_open: %s open(%s) errno %d", DEV_IO,
			    test_case, errno);
			goto out;
		}
		close(fd);
		goto out;
	}
	if (fd >= 0) {
		warn("try_open: %s open(%s) unexpected success", test_case,
		    DEV_IO);
		close(fd);
		goto out;
	}
	if (errno == expected)
		goto out;
	warn("try_open: %s open(%s) wrong errno %d, expected %d", DEV_IO,
	    test_case, errno, expected);
out:
	set_euid(UID_ROOT);
}

void
priv_io(void)
{

	assert_root();

	save_perms();

	try_open("root:0600", UID_ROOT, 0);
	try_open("other", UID_OTHER, EACCES);

	set_perms();

	try_open("root:0660", UID_ROOT, 0);
	try_open("other", UID_OTHER, EPERM);

	restore_perms();
}
