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

#include <sys/param.h>
#include <fcntl.h>
#include <pwd.h>
#include <string.h>
#include <timeconv.h>
#include <unistd.h>

#include "ulog_internal.h"

static void
ulog_utmpx_to_futmp(const struct ulog_utmpx *utx, struct futmp *ut)
{

	memset(ut, 0, sizeof *ut);
#define	COPY_STRING(field) do {						\
	strncpy(ut->ut_ ## field, utx->ut_ ## field,			\
	    MIN(sizeof ut->ut_ ## field, sizeof utx->ut_ ## field));	\
} while (0)
	switch (utx->ut_type) {
	case BOOT_TIME:
		strcpy(ut->ut_user, "reboot");
		ut->ut_line[0] = '~';
		break;
	case OLD_TIME:
		strcpy(ut->ut_user, "date");
		ut->ut_line[0] = '|';
		break;
	case NEW_TIME:
		strcpy(ut->ut_user, "date");
		ut->ut_line[0] = '{';
		break;
	case USER_PROCESS:
		COPY_STRING(user);
		COPY_STRING(line);
		COPY_STRING(host);
		break;
	case DEAD_PROCESS:
		COPY_STRING(line);
		break;
	case SHUTDOWN_TIME:
		strcpy(ut->ut_user, "shutdown");
		ut->ut_line[0] = '~';
		break;
	}
#undef COPY_STRING
	ut->ut_time = _time_to_time32(utx->ut_tv.tv_sec);
}

static void
ulog_utmpx_to_flastlog(const struct ulog_utmpx *utx, struct flastlog *ll)
{

	memset(ll, 0, sizeof *ll);
#define	COPY_STRING(field) do {						\
	strncpy(ll->ll_ ## field, utx->ut_ ## field,			\
	    MIN(sizeof ll->ll_ ## field, sizeof utx->ut_ ## field));	\
} while (0)
	switch (utx->ut_type) {
	case USER_PROCESS:
		COPY_STRING(line);
		COPY_STRING(host);
		break;
	}
#undef COPY_STRING
	ll->ll_time = _time_to_time32(utx->ut_tv.tv_sec);
}

static void
ulog_write_utmp_fast(const struct futmp *ut)
{
	unsigned int idx;
	char line[sizeof ut->ut_line + 1];
	int fd;

	if ((fd = open(_PATH_UTMP, O_WRONLY|O_CREAT, 0644)) < 0)
		return;
	strlcpy(line, ut->ut_line, sizeof line);
	idx = ulog_ttyslot(line);
	if (idx > 0) {
		lseek(fd, (off_t)(idx * sizeof *ut), SEEK_SET);
		write(fd, ut, sizeof *ut);
	}
	close(fd);
}

static int
ulog_write_utmp_slow(const struct futmp *ut)
{
	struct futmp utf;
	int fd, found;

	if ((fd = open(_PATH_UTMP, O_RDWR, 0)) < 0)
		return (0);
	found = 0;
	while (read(fd, &utf, sizeof utf) == sizeof utf) {
		if (utf.ut_user[0] == '\0' ||
		    strncmp(utf.ut_line, ut->ut_line, sizeof utf.ut_line) != 0)
			continue;
		lseek(fd, -(off_t)sizeof utf, SEEK_CUR);
		write(fd, ut, sizeof *ut);
		found = 1;
	}
	close(fd);
	return (found);
}

static void
ulog_write_wtmp(const struct futmp *ut)
{
	int fd;

	if ((fd = open(_PATH_WTMP, O_WRONLY|O_APPEND, 0)) < 0)
		return;
	write(fd, ut, sizeof *ut);
	close(fd);
}

static void
ulog_write_lastlog(const struct flastlog *ll, const char *user)
{
	struct passwd *pw;
	int fd;

	if ((fd = open(_PATH_LASTLOG, O_WRONLY, 0)) < 0)
		return;
	pw = getpwnam(user);
	if (pw != NULL) {
		lseek(fd, (off_t)(pw->pw_uid * sizeof *ll), SEEK_SET);
		write(fd, ll, sizeof *ll);
	}
	close(fd);
}

struct ulog_utmpx *
ulog_pututxline(const struct ulog_utmpx *utmpx)
{
	static struct ulog_utmpx utx;
	struct futmp ut;
	struct flastlog ll;
	char user[sizeof utmpx->ut_user + 1];

	switch (utmpx->ut_type) {
	case BOOT_TIME:
	case OLD_TIME:
	case NEW_TIME:
	case SHUTDOWN_TIME:
		ulog_utmpx_to_futmp(utmpx, &ut);

		/* Only log to wtmp. */
		ulog_write_wtmp(&ut);
		break;
	case USER_PROCESS:
		ulog_utmpx_to_futmp(utmpx, &ut);
		ulog_utmpx_to_flastlog(utmpx, &ll);

		/* Log to utmp, wtmp and lastlog. */
		ulog_write_utmp_fast(&ut);
		ulog_write_wtmp(&ut);
		strlcpy(user, utmpx->ut_user, sizeof user);
		ulog_write_lastlog(&ll, user);
		break;
	case DEAD_PROCESS:
		ulog_utmpx_to_futmp(utmpx, &ut);

		/* Only log to wtmp if logged in utmp. */
		if (ulog_write_utmp_slow(&ut))
			ulog_write_wtmp(&ut);
		break;
	default:
		return (NULL);
	}

	/* XXX: Can't we just return utmpx itself? */
	memcpy(&utx, utmpx, sizeof utx);
	utx.ut_user[sizeof utx.ut_user - 1] = '\0';
	utx.ut_line[sizeof utx.ut_line - 1] = '\0';
	utx.ut_host[sizeof utx.ut_host - 1] = '\0';
	return (&utx);
}
