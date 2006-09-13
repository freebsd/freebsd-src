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
 * Test that configuring accounting requires privilege.  First check that
 * accounting is not in use on the system to prevent disrupting the
 * accounting service.  Confirm three different state transitions, both as
 * privileged and non-privileged: disabled to enabled, rotate, and enabled to
 * disabled.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "main.h"

#define	SYSCTL_NAME	"kern.acct_configured"
#define	PATH_TEMPLATE	"/tmp/acct.XXXXXXXXXXX"

void
priv_acct(void)
{
	char fpath1[1024] = PATH_TEMPLATE;
	char fpath2[1024] = PATH_TEMPLATE;
	int error, fd, i;
	size_t len;

	assert_root();

	/*
	 * Check that accounting isn't already configured in the kernel.
	 */
	len = sizeof(i);
	if (sysctlbyname(SYSCTL_NAME, &i, &len, NULL, 0) < 0)
		err(-1, "sysctlbyname(%s)", SYSCTL_NAME);
	if (i != 0)
		errx(-1, "sysctlbyname(%s) indicates accounting configured",
		    SYSCTL_NAME);

	/*
	 * Create two temporary files to use as accounting targets.
	 */
	fd = mkstemp(fpath1);
	if (fd < 0)
		err(-1, "mkstemp");
	close(fd);
	fd = mkstemp(fpath2);
	if (fd < 0) {
		warn("mkstemp");
		(void)unlink(fpath1);
		exit(-1);
	}

	/*
	 * Change the permissions on the file so that access control on the
	 * file doesn't come into play.
	 */
	if (chmod(fpath1, 0666) < 0) {
		warn("chmod(%s, 0666)", fpath1);
		goto out;
	}

	if (chmod(fpath2, 0666) < 0) {
		warn("chmod(%s, 0600)", fpath2);
		goto out;
	}

	/*
	 * Test that privileged can move through entire life cycle.
	 */
	if (acct(fpath1) < 0) {
		warn("acct(NULL -> %s) as root", fpath1);
		goto out;
	}

	if (acct(fpath2) < 0) {
		warn("acct(%s -> %s) as root", fpath1, fpath2);
		goto out;
	}

	if (acct(NULL) < 0) {
		warn("acct(%s -> NULL) as root", fpath1);
		goto out;
	}

	/*
	 * Testing for unprivileged is a bit more tricky, as expect each step
	 * to fail, so must replay various bits of the setup process as root
	 * so that each step can be tested as !root.
	 */
	set_euid(UID_OTHER);
	error = acct(fpath1);
	if (error == 0) {
		warnx("acct(NULL -> %s) succeeded as !root", fpath1);
		goto out;
	}
	if (errno != EPERM) {
		warn("acct(NULL -> %s) wrong errno %d as !root", fpath1,
		    errno);
		goto out;
	}

	set_euid(UID_ROOT);
	if (acct(fpath1) < 0) {
		err(-1, "acct(NULL -> %s) setup for !root", fpath1);
		goto out;
	}

	set_euid(UID_OTHER);
	error = acct(fpath2);
	if (error == 0) {
		warnx("acct(%s -> %s) succeeded as !root", fpath1, fpath2);
		goto out;
	}
	if (errno != EPERM) {
		warn("acct(%s -> %s) wrong errno %d as !root", fpath1,
		    fpath2, errno);
		goto out;
	}

	set_euid(UID_ROOT);
	if (acct(fpath2) < 0) {
		err(-1, "acct(%s -> %s) setup for !root", fpath1, fpath2);
		goto out;
	}

	set_euid(UID_OTHER);
	error = acct(NULL);
	if (error == 0) {
		warnx("acct(%s -> NULL) succeeded as !root", fpath2);
		goto out;
	}
	if (errno != EPERM) {
		warn("acct(%s -> NULL) wrong errno %d as !root", fpath2,
		    errno);
		goto out;
	}

out:
	(void)seteuid(UID_ROOT);
	(void)acct(NULL);
	(void)unlink(fpath1);
	(void)unlink(fpath2);
}
