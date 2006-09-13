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
 * If times is NULL, ...  The caller must be the owner of the file, have
 * permission to write the file, or be the super-user.
 *
 * If times is non-NULL, ...  The caller must be the owner of the file or be
 * the super-user.
 *
 * To test these, create a temporary file owned by uid_owner; then run a
 * series of tests as root, owner, and other, along with various modes, to
 * see what is permitted, and if not, what error is returned.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "main.h"

static int
try_utimes(const char *path, mode_t mode, uid_t uid,
    struct timeval *timestamp, int expected)
{
	int error;

	if (chmod(path, mode) < 0) {
		warn("try_utimes(%s, %d, %d, 0x%08x): chmod", path, mode, uid,
		    (u_int)timestamp);
		(void)unlink(path);
		exit(-1);
	}

	if (seteuid(uid) < 0) {
		warn("try_utimes(%s, %d, %d, 0x%08x): seteuid(%d)", path,
		    mode, uid, (u_int)timestamp, uid);
		(void)unlink(path);
		exit(-1);
	}

	error = utimes(path, timestamp);

	if (seteuid(UID_ROOT) < 0) {
		warn("try_utimes(%s, %d, %d, 0x%08x): seteuid(UID_ROOT)",
		    path, mode, uid, (u_int)timestamp);
		(void)unlink(path);
		exit(-1);
	}

	if (expected == 0) {
		if (error != 0) {
			(void)unlink(path);
			errx(-1, "try_utimes(%s, 0%o, %d, 0x%08x) failed %d",
			    path, mode, uid, (u_int)timestamp, errno);
		}
		return (0);
	}

	if (expected == errno)
		return (0);

	(void)unlink(path);
	errx(-1, "try_utimes(%s, 0%o, %d, 0x%08x) wrong err %d", path, mode,
	    uid, (u_int)timestamp, errno);
}

void
test_utimes(void)
{
	char path[128] = "/tmp/utimes.XXXXXXXXX";
	struct timeval timestamp[2];
	int fd;

	if (getuid() != 0)
		errx(-1, "must be run as root");

	fd = mkstemp(path);
	if (fd == -1)
		err(-1, "mkstemp");

	if (chown(path, UID_OWNER, -1) < 0) {
		warn("chown(%s, %d)", path, UID_OWNER);
		(void)unlink(path);
		return;
	}

	bzero(timestamp, sizeof(timestamp));

	try_utimes(path, 0444, UID_ROOT, NULL, 0);
	try_utimes(path, 0444, UID_OWNER, NULL, 0);
	/* Denied by permissions. */
	try_utimes(path, 0444, UID_OTHER, NULL, EACCES);

	try_utimes(path, 0444, UID_ROOT, timestamp, 0);
	try_utimes(path, 0444, UID_OWNER, timestamp, 0);
	try_utimes(path, 0444, UID_OTHER, timestamp, EPERM);

	try_utimes(path, 0644, UID_ROOT, NULL, 0);
	try_utimes(path, 0644, UID_OWNER, NULL, 0);
	/* Denied by permissions. */
	try_utimes(path, 0644, UID_OTHER, NULL, EACCES);

	try_utimes(path, 0644, UID_ROOT, timestamp, 0);
	try_utimes(path, 0644, UID_OWNER, timestamp, 0);
	/* Denied as not owner. */
	try_utimes(path, 0644, UID_OTHER, timestamp, EPERM);

	try_utimes(path, 0666, UID_ROOT, NULL, 0);
	try_utimes(path, 0666, UID_OWNER, NULL, 0);
	try_utimes(path, 0666, UID_OTHER, NULL, 0);

	try_utimes(path, 0666, UID_ROOT, timestamp, 0);
	try_utimes(path, 0666, UID_OWNER, timestamp, 0);
	/* Denied as not owner. */
	try_utimes(path, 0666, UID_OTHER, timestamp, EPERM);

	(void)unlink(path);
}
