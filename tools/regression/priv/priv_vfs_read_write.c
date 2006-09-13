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
 * This is a joint test of both the read and write privileges with respect to
 * discretionary file system access control (permissions).  Only permissions,
 * not ACL semantics, and only privilege-related checks are performed.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "main.h"

struct test_arguments {
	int	open_flags;
	uid_t	proc_uid;
	gid_t	proc_gid;
	uid_t	file_uid;
	gid_t	file_gid;
};

/*
 * Rather special-purpose, don't reuse.  Will need updating if anything other
 * than O_RDONLY and O_WRONLY are to be used in tests.
 */
static const char *
flags_to_string(int flags)
{

	switch (flags) {
	case O_RDONLY:
		return ("O_RDONLY");

	case O_WRONLY:
		return ("O_WRONLY");

	default:
		return ("unknown");
	}
}

static void
test_perm(struct test_arguments ta, mode_t file_mode, int expected)
{
	uid_t proc_uid, file_uid;
	gid_t proc_gid, file_gid;
	int fd, open_flags;
	char fpath[1024];

	proc_uid = ta.proc_uid;
	proc_gid = ta.proc_gid;
	file_uid = ta.file_uid;
	file_gid = ta.file_gid;
	open_flags = ta.open_flags;

	setup_file(fpath, file_uid, file_gid, file_mode);
	set_creds(proc_uid, proc_gid);

	fd = open(fpath, open_flags);

	if (expected == 0) {
		if (fd <= 0) {
			warn("test_perm(%s, %d, %d, %d, %d, %04o, %d) "
			    "returned %d instead of %d",
			    flags_to_string(open_flags), proc_uid, proc_gid,
			    file_uid, file_gid, file_mode, expected,
			    errno, expected);
			restore_creds();
			(void)unlink(fpath);
			exit(-1);
		}
		close(fd);
	} else {
		if (fd >= 0) {
			warnx("test_perm(%s, %d, %d, %d, %d, %04o, %d)"
			    " returned 0 instead of %d",
			    flags_to_string(open_flags), proc_uid, proc_gid,
			    file_uid, file_gid, file_mode, expected,
			    expected);
			close(fd);
			restore_creds();
			(void)unlink(fpath);
			exit(-1);
		} else if (errno != expected) {
			warn("test_perm(%s, %d, %d, %d, %d, %04o, %d)"
			    " returned %d instead of %d",
			    flags_to_string(open_flags), proc_uid, proc_gid,
			    file_uid, file_gid, file_mode, expected,
			    errno, expected);
			restore_creds();
			(void)unlink(fpath);
			exit(-1);
		}
	}

	restore_creds();
	(void)unlink(fpath);
}

static const gid_t gidset[] = { GID_WHEEL };

static void
preamble(void)
{

	if (getuid() != UID_ROOT)
		errx(-1, "must be run as root");
	if (setgroups(1, gidset) < 0)
		err(-1, "setgroups(1, {%d})", GID_WHEEL);
}

void
priv_vfs_read(void)
{
	struct test_arguments ta;

	preamble();

	ta.open_flags = O_RDONLY;

	/*
	 * Privileged user and file owner.  All tests should pass.
	 */
	ta.proc_uid = UID_ROOT;
	ta.proc_gid = GID_WHEEL;
	ta.file_uid = UID_ROOT;
	ta.file_gid = GID_WHEEL;

	test_perm(ta, 0000, 0);
	test_perm(ta, 0100, 0);
	test_perm(ta, 0200, 0);
	test_perm(ta, 0300, 0);
	test_perm(ta, 0400, 0);
	test_perm(ta, 0500, 0);
	test_perm(ta, 0600, 0);
	test_perm(ta, 0700, 0);

	/*
	 * Privileged user and file group.  All tests should pass.
	 */
	ta.proc_uid = UID_ROOT;
	ta.proc_gid = GID_WHEEL;
	ta.file_uid = UID_OWNER;
	ta.file_gid = GID_WHEEL;

	test_perm(ta, 0000, 0);
	test_perm(ta, 0010, 0);
	test_perm(ta, 0020, 0);
	test_perm(ta, 0030, 0);
	test_perm(ta, 0040, 0);
	test_perm(ta, 0050, 0);
	test_perm(ta, 0060, 0);
	test_perm(ta, 0070, 0);

	/*
	 * Privileged user and file other.  All tests should pass.
	 */
	ta.proc_uid = UID_ROOT;
	ta.proc_gid = GID_WHEEL;
	ta.file_uid = UID_OWNER;
	ta.file_gid = GID_OWNER;

	test_perm(ta, 0000, 0);
	test_perm(ta, 0001, 0);
	test_perm(ta, 0002, 0);
	test_perm(ta, 0003, 0);
	test_perm(ta, 0004, 0);
	test_perm(ta, 0005, 0);
	test_perm(ta, 0006, 0);
	test_perm(ta, 0007, 0);

	/*
	 * Unprivileged user and file owner.  Various DAC failures.
	 */
	ta.proc_uid = UID_OWNER;
	ta.proc_gid = GID_OWNER;
	ta.file_uid = UID_OWNER;
	ta.file_gid = GID_OWNER;

	test_perm(ta, 0000, EACCES);
	test_perm(ta, 0100, EACCES);
	test_perm(ta, 0200, EACCES);
	test_perm(ta, 0300, EACCES);
	test_perm(ta, 0400, 0);
	test_perm(ta, 0500, 0);
	test_perm(ta, 0600, 0);
	test_perm(ta, 0700, 0);

	/*
	 * Unprivileged user and file group.  Various DAC failures.
	 */
	ta.proc_uid = UID_OTHER;
	ta.proc_gid = GID_OWNER;
	ta.file_uid = UID_OWNER;
	ta.file_gid = GID_OWNER;

	test_perm(ta, 0000, EACCES);
	test_perm(ta, 0010, EACCES);
	test_perm(ta, 0020, EACCES);
	test_perm(ta, 0030, EACCES);
	test_perm(ta, 0040, 0);
	test_perm(ta, 0050, 0);
	test_perm(ta, 0060, 0);
	test_perm(ta, 0070, 0);

	/*
	 * Unprivileged user and file other.  Various DAC failures.
	 */
	ta.proc_uid = UID_OTHER;
	ta.proc_gid = GID_OTHER;
	ta.file_uid = UID_OWNER;
	ta.file_gid = GID_OWNER;

	test_perm(ta, 0000, EACCES);
	test_perm(ta, 0001, EACCES);
	test_perm(ta, 0002, EACCES);
	test_perm(ta, 0003, EACCES);
	test_perm(ta, 0004, 0);
	test_perm(ta, 0005, 0);
	test_perm(ta, 0006, 0);
	test_perm(ta, 0007, 0);
}

void
priv_vfs_write(void)
{
	struct test_arguments ta;

	preamble();

	ta.open_flags = O_WRONLY;

	/*
	 * Privileged user and file owner.  All tests should pass.
	 */
	ta.proc_uid = UID_ROOT;
	ta.proc_gid = GID_WHEEL;
	ta.file_uid = UID_ROOT;
	ta.file_gid = GID_WHEEL;

	test_perm(ta, 0000, 0);
	test_perm(ta, 0100, 0);
	test_perm(ta, 0200, 0);
	test_perm(ta, 0300, 0);
	test_perm(ta, 0400, 0);
	test_perm(ta, 0500, 0);
	test_perm(ta, 0600, 0);
	test_perm(ta, 0700, 0);

	/*
	 * Privileged user and file group.  All tests should pass.
	 */
	ta.proc_uid = UID_ROOT;
	ta.proc_gid = GID_WHEEL;
	ta.file_uid = UID_OWNER;
	ta.file_gid = GID_WHEEL;

	test_perm(ta, 0000, 0);
	test_perm(ta, 0010, 0);
	test_perm(ta, 0020, 0);
	test_perm(ta, 0030, 0);
	test_perm(ta, 0040, 0);
	test_perm(ta, 0050, 0);
	test_perm(ta, 0060, 0);
	test_perm(ta, 0070, 0);

	/*
	 * Privileged user and file other.  All tests should pass.
	 */
	ta.proc_uid = UID_ROOT;
	ta.proc_gid = GID_WHEEL;
	ta.file_uid = UID_OWNER;
	ta.file_gid = GID_OWNER;

	test_perm(ta, 0000, 0);
	test_perm(ta, 0001, 0);
	test_perm(ta, 0002, 0);
	test_perm(ta, 0003, 0);
	test_perm(ta, 0004, 0);
	test_perm(ta, 0005, 0);
	test_perm(ta, 0006, 0);
	test_perm(ta, 0007, 0);

	/*
	 * Unprivileged user and file owner.  Various DAC failures.
	 */
	ta.proc_uid = UID_OWNER;
	ta.proc_gid = GID_OWNER;
	ta.file_uid = UID_OWNER;
	ta.file_gid = GID_OWNER;

	test_perm(ta, 0000, EACCES);
	test_perm(ta, 0100, EACCES);
	test_perm(ta, 0200, 0);
	test_perm(ta, 0300, 0);
	test_perm(ta, 0400, EACCES);
	test_perm(ta, 0500, EACCES);
	test_perm(ta, 0600, 0);
	test_perm(ta, 0700, 0);

	/*
	 * Unprivileged user and file group.  Various DAC failures.
	 */
	ta.proc_uid = UID_OTHER;
	ta.proc_gid = GID_OWNER;
	ta.file_uid = UID_OWNER;
	ta.file_gid = GID_OWNER;

	test_perm(ta, 0000, EACCES);
	test_perm(ta, 0010, EACCES);
	test_perm(ta, 0020, 0);
	test_perm(ta, 0030, 0);
	test_perm(ta, 0040, EACCES);
	test_perm(ta, 0050, EACCES);
	test_perm(ta, 0060, 0);
	test_perm(ta, 0070, 0);

	/*
	 * Unprivileged user and file other.  Various DAC failures.
	 */
	ta.proc_uid = UID_OTHER;
	ta.proc_gid = GID_OTHER;
	ta.file_uid = UID_OWNER;
	ta.file_gid = GID_OWNER;

	test_perm(ta, 0000, EACCES);
	test_perm(ta, 0001, EACCES);
	test_perm(ta, 0002, 0);
	test_perm(ta, 0003, 0);
	test_perm(ta, 0004, EACCES);
	test_perm(ta, 0005, EACCES);
	test_perm(ta, 0006, 0);
	test_perm(ta, 0007, 0);
}
