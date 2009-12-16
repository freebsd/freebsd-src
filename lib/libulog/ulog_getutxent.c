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

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <timeconv.h>

#include "ulog_internal.h"

static FILE *ufile;
static int ufiletype = -1;

void
ulog_endutxent(void)
{
	if (ufile != NULL)
		fclose(ufile);
	ufile = NULL;
}

/*
 * Conversion from on-disk formats to generic ulog_utmpx structure.
 */

static void
ulog_futmp_to_utmpx(const struct futmp *ut, struct ulog_utmpx *utx)
{

	memset(utx, 0, sizeof *utx);
#define	COPY_STRING(field) do {						\
	strncpy(utx->ut_ ## field, ut->ut_ ## field,			\
	    MIN(sizeof utx->ut_ ## field - 1, sizeof ut->ut_ ## field));\
} while (0)
	COPY_STRING(user);
	COPY_STRING(line);
	COPY_STRING(host);
#undef COPY_STRING
#define	MATCH(field, value)	(strcmp(utx->ut_ ## field, (value)) == 0)
	if (MATCH(user, "reboot") && MATCH(line, "~"))
		utx->ut_type = BOOT_TIME;
	else if (MATCH(user, "date") && MATCH(line, "|"))
		utx->ut_type = OLD_TIME;
	else if (MATCH(user, "date") && MATCH(line, "{"))
		utx->ut_type = NEW_TIME;
	else if (MATCH(user, "shutdown") && MATCH(line, "~"))
		utx->ut_type = SHUTDOWN_TIME;
	else if (MATCH(user, "") && MATCH(host, ""))
		utx->ut_type = DEAD_PROCESS;
	else if (!MATCH(user, "") && !MATCH(line, "") && ut->ut_time != 0)
		utx->ut_type = USER_PROCESS;
	else
		utx->ut_type = EMPTY;
	utx->ut_tv.tv_sec = _time32_to_time(ut->ut_time);
	utx->ut_tv.tv_usec = 0;
}

static void
ulog_flastlog_to_utmpx(const struct flastlog *ll, struct ulog_utmpx *utx)
{

	memset(utx, 0, sizeof *utx);
#define	COPY_STRING(field) do {						\
	strncpy(utx->ut_ ## field, ll->ll_ ## field,			\
	    MIN(sizeof utx->ut_ ## field - 1, sizeof ll->ll_ ## field));\
} while (0)
	COPY_STRING(line);
	COPY_STRING(host);
#undef COPY_STRING
	if (!MATCH(line, "") && ll->ll_time != 0)
		utx->ut_type = USER_PROCESS;
	else
		utx->ut_type = EMPTY;
	utx->ut_tv.tv_sec = _time32_to_time(ll->ll_time);
	utx->ut_tv.tv_usec = 0;
}

/*
 * File I/O.
 */

static inline off_t
ulog_tell(void)
{

	if (ufiletype == UTXF_LASTLOG)
		return (ftello(ufile) / sizeof(struct flastlog));
	else
		return (ftello(ufile) / sizeof(struct futmp));
}

static struct ulog_utmpx *
ulog_read(off_t off, int whence, int resolve_user)
{
	static struct ulog_utmpx utx;

	if (ufile == NULL)
		ulog_setutxent();
	if (ufile == NULL)
		return (NULL);

	/* Only allow seeking to move forward. */
	if (whence == SEEK_SET && ulog_tell() > off)
		return (NULL);

	if (ufiletype == UTXF_LASTLOG) {
		struct flastlog ll;
		struct passwd *pw = NULL;
		uid_t uid;

		if (fseeko(ufile, off * sizeof ll, whence) != 0)
			return (NULL);
		uid = ulog_tell();
		if (fread(&ll, sizeof ll, 1, ufile) != 1)
			return (NULL);
		ulog_flastlog_to_utmpx(&ll, &utx);
		if (utx.ut_type == USER_PROCESS && resolve_user)
			pw = getpwuid(uid);
		if (pw != NULL)
			strlcpy(utx.ut_user, pw->pw_name, sizeof utx.ut_user);
		else
			sprintf(utx.ut_user, "%u", (unsigned int)uid);
	} else {
		struct futmp ut;

		if (fseeko(ufile, off * sizeof(struct futmp), whence) != 0)
			return (NULL);
		if (fread(&ut, sizeof ut, 1, ufile) != 1)
			return (NULL);
		ulog_futmp_to_utmpx(&ut, &utx);
	}
	return (&utx);
}

/*
 * getutxent().
 *
 * Read the next entry from the file.
 */

struct ulog_utmpx *
ulog_getutxent(void)
{

	return ulog_read(0, SEEK_CUR, 1);
}

/*
 * ulog_getutxline().
 *
 * Read entries from the file, until reaching an entry which matches the
 * provided TTY device name.  We can optimize the case for utmp files,
 * because they are indexed by TTY device name.
 */

struct ulog_utmpx *
ulog_getutxline(const struct ulog_utmpx *line)
{
	struct ulog_utmpx *utx;

	if (ufile == NULL)
		ulog_setutxent();
	if (ufile == NULL)
		return (NULL);

	if (ufiletype == UTXF_UTMP) {
		unsigned int slot;

		slot = ulog_ttyslot(line->ut_line);
		if (slot == 0)
			return (NULL);
		utx = ulog_read(slot, SEEK_SET, 1);
		if (utx->ut_type == USER_PROCESS &&
		    strcmp(utx->ut_line, line->ut_line) == 0)
			return (utx);
		return (NULL);
	} else {
		for (;;) {
			utx = ulog_read(0, SEEK_CUR, 1);
			if (utx == NULL)
				return (NULL);
			if (utx->ut_type == USER_PROCESS &&
			    strcmp(utx->ut_line, line->ut_line) == 0)
				return (utx);
		}
	}
}

/*
 * ulog_getutxuser().
 *
 * Read entries from the file, until reaching an entry which matches the
 * provided username.  We can optimize the case for lastlog files,
 * because they are indexed by user ID.
 */

struct ulog_utmpx *
ulog_getutxuser(const char *user)
{
	struct ulog_utmpx *utx;

	if (ufiletype == UTXF_LASTLOG) {
		struct passwd *pw;

		pw = getpwnam(user);
		if (pw == NULL)
			return (NULL);
		utx = ulog_read(pw->pw_uid, SEEK_SET, 0);
		if (utx != NULL)
			strlcpy(utx->ut_user, user, sizeof utx->ut_user);
		return (utx);
	} else {
		for (;;) {
			utx = ulog_read(0, SEEK_CUR, 1);
			if (utx == NULL)
				return (NULL);
			if (utx->ut_type == USER_PROCESS &&
			    strcmp(utx->ut_user, user) == 0)
				return (utx);
		}
	}
}

/*
 * ulog_setutxfile().
 *
 * Switch to a different record file.  When no filename is provided, the
 * system default is opened.
 */

int
ulog_setutxfile(int type, const char *file)
{

	/* Supply default files. */
	switch (type) {
	case UTXF_UTMP:
		if (file == NULL)
			file = _PATH_UTMP;
		break;
	case UTXF_WTMP:
		if (file == NULL)
			file = _PATH_WTMP;
		break;
	case UTXF_LASTLOG:
		if (file == NULL)
			file = _PATH_LASTLOG;
		break;
	default:
		return (-1);
	}

	if (ufile != NULL)
		fclose(ufile);
	ufile = fopen(file, "r");
	ufiletype = type;
	if (ufile == NULL)
		return (-1);
	return (0);
}

/*
 * ulog_endutxfile().
 *
 * Close any opened files.
 */

void
ulog_setutxent(void)
{

	ulog_setutxfile(UTXF_UTMP, NULL);
}
