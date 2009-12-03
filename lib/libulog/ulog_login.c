/*-
 * Copyright (c) 2009 Ed Schouten <ed@FreeBSD.org>
 * All rights reserved.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <fcntl.h>
#include <inttypes.h>
#include <paths.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <timeconv.h>
#include <ttyent.h>

#include "ulog_internal.h"

void
ulog_login(const char *line, const char *user, const char *host)
{
	struct futmp fu;
	struct flastlog fl;
	int fd;

	/* Remove /dev/ component. */
	if (strncmp(line, _PATH_DEV, sizeof _PATH_DEV - 1) == 0)
		line += sizeof _PATH_DEV - 1;

	/* Prepare log entries. */
	memset(&fu, 0, sizeof fu);
	strlcpy(fu.ut_line, line, sizeof fu.ut_line);
	strlcpy(fu.ut_user, user, sizeof fu.ut_user);
	if (host != NULL)
		strlcpy(fu.ut_host, host, sizeof fu.ut_host);
	fu.ut_time = _time_to_time32(time(NULL));

	fl.ll_time = fu.ut_time;
	memcpy(fl.ll_line, fu.ut_line, sizeof fl.ll_line);
	memcpy(fl.ll_host, fu.ut_host, sizeof fl.ll_host);

	/* Update utmp entry. */
	if ((fd = open(_PATH_UTMP, O_WRONLY|O_CREAT, 0644)) >= 0) {
		struct ttyent *ty;
		int idx;

		setttyent();
		for (idx = 1; (ty = getttyent()) != NULL; ++idx) {
			if (strcmp(ty->ty_name, line) != 0)
				continue;
			lseek(fd, (off_t)(idx * sizeof fu), L_SET);
			write(fd, &fu, sizeof fu);
			break;
		}
		endttyent();
		close(fd);
	}

	/* Add wtmp entry. */
	if ((fd = open(_PATH_WTMP, O_WRONLY|O_APPEND, 0)) >= 0) {
		write(fd, &fu, sizeof fu);
		close(fd);
	}

	/* Update lastlog entry. */
	if ((fd = open(_PATH_LASTLOG, O_WRONLY, 0)) >= 0) {
		struct passwd *pw;

		pw = getpwnam(user);
		if (pw != NULL) {
			lseek(fd, (off_t)(pw->pw_uid * sizeof fl), L_SET);
			write(fd, &fl, sizeof fl);
		}
		close(fd);
	}
}

void
ulog_logout(const char *line)
{
	struct futmp ut;
	int fd, found;

	/* Remove /dev/ component. */
	if (strncmp(line, _PATH_DEV, sizeof _PATH_DEV - 1) == 0)
		line += sizeof _PATH_DEV - 1;

	/* Mark entry in utmp as logged out. */
	if ((fd = open(_PATH_UTMP, O_RDWR, 0)) < 0)
		return;
	found = 0;
	while (read(fd, &ut, sizeof ut) == sizeof ut) {
		if (ut.ut_user[0] == '\0' ||
		    strncmp(ut.ut_line, line, sizeof ut.ut_line) != 0)
			continue;
		memset(ut.ut_user, 0, sizeof ut.ut_user);
		memset(ut.ut_host, 0, sizeof ut.ut_host);
		ut.ut_time = _time_to_time32(time(NULL));
		lseek(fd, -(off_t)sizeof ut, L_INCR);
		write(fd, &ut, sizeof ut);
		found = 1;
	}
	close(fd);
	if (!found)
		return;

	/* utmp entry found. Also add logout entry to wtmp. */
	if ((fd = open(_PATH_WTMP, O_WRONLY|O_APPEND, 0)) >= 0) {
		write(fd, &ut, sizeof ut);
		close(fd);
	}
}
