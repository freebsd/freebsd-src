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
	"$Id$";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "pwupd.h"

int
fmtgrentry(char *buf, struct group * grp, int type)
{
	int             i, l;

	if (type == PWF_STANDARD)
		l = sprintf(buf, "%s:*:%ld:", grp->gr_name, (long) grp->gr_gid);
	else
		l = sprintf(buf, "%s:%s:%ld:", grp->gr_name, grp->gr_passwd, (long) grp->gr_gid);

	/*
	 * Now, list members
	 */
	for (i = 0; i < 200 && grp->gr_mem[i]; i++)
		l += sprintf(buf + l, "%s%s", i ? "," : "", grp->gr_mem[i]);
	buf[l++] = '\n';
	buf[l] = '\0';
	return l;
}


int
fmtgrent(char *buf, struct group * grp)
{
	return fmtgrentry(buf, grp, PWF_STANDARD);
}


static int
gr_update(struct group * grp, char const * group, int mode)
{
	int             l;
	char            pfx[32];
	char            grbuf[MAXPWLINE];

	endgrent();
	l = sprintf(pfx, "%s:", group);

	/*
	 * Update the group file
	 */
	if (grp == NULL)
		*grbuf = '\0';
	else
		fmtgrentry(grbuf, grp, PWF_PASSWD);
	return fileupdate(_PATH_GROUP, 0644, grbuf, pfx, l, mode);
}


int
addgrent(struct group * grp)
{
	return gr_update(grp, grp->gr_name, UPD_CREATE);
}

int
chggrent(char const * login, struct group * grp)
{
	return gr_update(grp, login, UPD_REPLACE);
}

int
delgrent(struct group * grp)
{
	return gr_update(NULL, grp->gr_name, UPD_DELETE);
}
