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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <timeconv.h>

#include "ulog_internal.h"

static FILE *ufile;

void
ulog_endutxent(void)
{
	if (ufile != NULL)
		fclose(ufile);
	ufile = NULL;
}

struct ulog_utmpx *
ulog_getutxent(void)
{
	struct futmp ut;
	static struct ulog_utmpx utx;

	/* Open the utmp file if not already done so. */
	if (ufile == NULL)
		ulog_setutxent();
	if (ufile == NULL)
		return (NULL);

	if (fread(&ut, sizeof ut, 1, ufile) != 1)
		return (NULL);
#define	COPY_STRING(field) do {					\
	free(utx.ut_ ## field); 				\
	utx.ut_ ## field = strndup(ut.ut_ ## field,		\
	    sizeof ut.ut_ ## field);				\
	if (utx.ut_ ## field == NULL)				\
		utx.ut_ ## field = __DECONST(char *, "");	\
} while (0)
	COPY_STRING(user);
	COPY_STRING(line);
	COPY_STRING(host);
	utx.ut_tv.tv_sec = _time32_to_time(ut.ut_time);
	utx.ut_tv.tv_usec = 0;
#define	MATCH(field, value)	(strcmp(utx.ut_ ## field, (value)) == 0)
	if (MATCH(user, "date") && MATCH(line, "|"))
		utx.ut_type = OLD_TIME;
	else if (MATCH(user, "date") && MATCH(line, "{"))
		utx.ut_type = NEW_TIME;
	else if (MATCH(user, "shutdown") && MATCH(line, "~"))
		utx.ut_type = SHUTDOWN_TIME;
	else if (MATCH(user, "reboot") && MATCH(line, "~"))
		utx.ut_type = REBOOT_TIME;
	else if (MATCH(user, "") && MATCH(host, ""))
		utx.ut_type = DEAD_PROCESS;
	else if (!MATCH(user, ""))
		utx.ut_type = USER_PROCESS;
	else
		utx.ut_type = EMPTY;
	
	return (&utx);
}

void
ulog_setutxent(void)
{

	if (ufile != NULL)
		fclose(ufile);
	ufile = fopen(_PATH_UTMP, "r");
}
