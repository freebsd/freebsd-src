/*-
 * Copyright (C) 1996
 *	David L. Nugent.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY DAVID L. NUGENT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID L. NUGENT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "pw.h"

static int
pw_nisupdate(const char * path, struct passwd * pwd, char const * user, int mode)
{
	char            pfx[32];
	char            pwbuf[PWBUFSZ];
	int             l = sprintf(pfx, "%s:", user);

	/*
	 * Update the passwd file first
	 */
	if (pwd == NULL)
		*pwbuf = '\0';
	else
		fmtpwentry(pwbuf, pwd, PWF_MASTER);
	return fileupdate(path, 0600, pwbuf, pfx, l, mode) != 0;
}

int
addnispwent(const char *path, struct passwd * pwd)
{
	return pw_nisupdate(path, pwd, pwd->pw_name, UPD_CREATE);
}

int
chgnispwent(const char *path, char const * login, struct passwd * pwd)
{
	return pw_nisupdate(path, pwd, login, UPD_REPLACE);
}

int
delnispwent(const char *path, const char *login)
{
	return pw_nisupdate(path, NULL, login, UPD_DELETE);
}
