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
 * Confirm that a generation number isn't returned by stat() when not running
 * with privilege.  In order to differentiate between a generation of 0 and
 * a generation not being returned, we have to create a temporary file known
 * to have a non-0 generation.  We try up to 10 times, and then give up,
 * which is non-ideal, but better than not testing for a problem.
 */

#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "main.h"

/*
 * Can't use setup_file() since the resulting file needs to have specific
 * properties.
 */
void
priv_vfs_generation(void)
{
	char fpath[1024] = "/tmp/priv.XXXXXXXXXX";
	struct stat sb;
	int fd, i;

	assert_root();

	/*
	 * Create a file with a non-0 generation number.  Try ten times,
	 * which gives a high chance of succeeds, fail otherwise.  Not ideal,
	 * since we can't distinguish the file having a generation of 0 from
	 * not being able to query it for access control reasons.  The perils
	 * of an API that changes behavior based on lack of privilege rather
	 * than failing...
	 */
	for (i = 0; i < 10; i++) {
		fd = mkstemp(fpath);
		if (fd < 0)
			err(-1, "mkstemp");
		if (fstat(fd, &sb) < 0) {
			warn("fstat(%s)", fpath);
			close(fd);
			goto out;
		}
		if (sb.st_gen != 0)
			break;
		close(fd);
		(void)unlink(fpath);
		strcpy(fpath, "/tmp/generation.XXXXXXXXXX");
		fd = -1;
	}
	if (fd == -1)
		errx(-1,
		    "could not create file with non-0 generation as root");
	close(fd);

	/*
	 * We've already tested that fstat() works, but try stat() to be
	 * consistent between privileged and unprivileged tests.
	 */
	if (stat(fpath, &sb) < 0) {
		warn("stat(%s) as root", fpath);
		goto out;
	}

	set_euid(UID_OTHER);

	if (stat(fpath, &sb) < 0) {
		warn("stat(%s) as !root", fpath);
		goto out;
	}

	if (sb.st_gen != 0)
		warn("stat(%s) returned generation as !root", fpath);

out:
	(void)seteuid(UID_ROOT);
	(void)unlink(fpath);
}
