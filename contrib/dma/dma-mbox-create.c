/*
 * Copyright (c) 2010-2014, Simon Schubert <2@0x2c.org>.
 * Copyright (c) 2008 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Simon Schubert <2@0x2c.org>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This binary is setuid root.  Use extreme caution when touching
 * user-supplied information.  Keep the root window as small as possible.
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

#include "dma.h"


static void
logfail(int exitcode, const char *fmt, ...)
{
	int oerrno = errno;
	va_list ap;
	char outs[1024];

	outs[0] = 0;
	if (fmt != NULL) {
		va_start(ap, fmt);
		vsnprintf(outs, sizeof(outs), fmt, ap);
		va_end(ap);
	}

	errno = oerrno;
	if (*outs != 0)
		syslog(LOG_ERR, errno ? "%s: %m" : "%s", outs);
	else
		syslog(LOG_ERR, errno ? "%m" : "unknown error");

	exit(exitcode);
}

/*
 * Create a mbox in /var/mail for a given user, or make sure
 * the permissions are correct for dma.
 */

int
main(int argc, char **argv)
{
	const char *user;
	struct passwd *pw;
	struct group *gr;
	uid_t user_uid;
	gid_t mail_gid;
	int error;
	char fn[PATH_MAX+1];
	int f;

	openlog("dma-mbox-create", 0, LOG_MAIL);

	errno = 0;
	gr = getgrnam(DMA_GROUP);
	if (!gr)
		logfail(EX_CONFIG, "cannot find dma group `%s'", DMA_GROUP);

	mail_gid = gr->gr_gid;

	if (setgid(mail_gid) != 0)
		logfail(EX_NOPERM, "cannot set gid to %d (%s)", mail_gid, DMA_GROUP);
	if (getegid() != mail_gid)
		logfail(EX_NOPERM, "cannot set gid to %d (%s), still at %d", mail_gid, DMA_GROUP, getegid());

	/*
	 * We take exactly one argument: the username.
	 */
	if (argc != 2) {
		errno = 0;
		logfail(EX_USAGE, "no arguments");
	}
	user = argv[1];

	syslog(LOG_NOTICE, "creating mbox for `%s'", user);

	/* the username may not contain a pathname separator */
	if (strchr(user, '/')) {
		errno = 0;
		logfail(EX_DATAERR, "path separator in username `%s'", user);
		exit(1);
	}

	/* verify the user exists */
	errno = 0;
	pw = getpwnam(user);
	if (!pw)
		logfail(EX_NOUSER, "cannot find user `%s'", user);

	user_uid = pw->pw_uid;

	error = snprintf(fn, sizeof(fn), "%s/%s", _PATH_MAILDIR, user);
	if (error < 0 || (size_t)error >= sizeof(fn)) {
		if (error >= 0) {
			errno = 0;
			logfail(EX_USAGE, "mbox path too long");
		}
		logfail(EX_CANTCREAT, "cannot build mbox path for `%s/%s'", _PATH_MAILDIR, user);
	}

	f = open(fn, O_RDONLY|O_CREAT, 0600);
	if (f < 0)
		logfail(EX_NOINPUT, "cannt open mbox `%s'", fn);

	if (fchown(f, user_uid, mail_gid))
		logfail(EX_OSERR, "cannot change owner of mbox `%s'", fn);

	if (fchmod(f, 0620))
		logfail(EX_OSERR, "cannot change permissions of mbox `%s'", fn);

	/* file should be present with the right owner and permissions */

	syslog(LOG_NOTICE, "successfully created mbox for `%s'", user);

	return (0);
}
