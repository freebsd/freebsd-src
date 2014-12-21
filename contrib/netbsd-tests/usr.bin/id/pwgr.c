/* $NetBSD: pwgr.c,v 1.1 2012/03/17 16:33:14 jruoho Exp $ */
/*
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file implements replacements for all user/group-related functions
 * called by id(1).  It provides fake but deterministic user and group
 * information.  The details are as such:
 * User root, uid 0, primary group 0 (wheel).
 * User test, uid 100, primary group 100 (users), secondary group 0 (wheel).
 */

#include <sys/types.h>

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

char Login[16];
struct group GrEntry;
struct passwd PwEntry;

gid_t
getgid(void)
{
	return 100;
}

gid_t
getegid(void)
{
	if (getenv("LIBFAKE_EGID_ROOT") != NULL)
		return 0;
	else
		return 100;
}

uid_t
getuid(void)
{
	return 100;
}

uid_t
geteuid(void)
{
	if (getenv("LIBFAKE_EUID_ROOT") != NULL)
		return 0;
	else
		return 100;
}

char *
getlogin(void)
{
	strcpy(Login, "test");
	return Login;
}

struct group *
getgrgid(gid_t gid)
{
	struct group *g = &GrEntry;

	memset(g, 0, sizeof(*g));
	if (gid == 0) {
		g->gr_name = __UNCONST("wheel");
		g->gr_gid = 0;
	} else if (gid == 100) {
		g->gr_name = __UNCONST("users");
		g->gr_gid = 100;
	} else
		g = NULL;

	return g;
}

int
getgrouplist(const char *name, gid_t basegid, gid_t *groups, int *ngroups)
{
	int cnt, ret;

	if (strcmp(name, "root") == 0) {
		if (*ngroups >= 1) {
			groups[0] = basegid;
			cnt = 1;
		}

		ret = (*ngroups >= cnt) ? 0 : -1;
		*ngroups = cnt;
	} else if (strcmp(name, "test") == 0) {
		if (*ngroups >= 1) {
			groups[0] = basegid;
			cnt = 1;
		}

		if (*ngroups >= 2) {
			groups[1] = 0;
			cnt = 2;
		}

		ret = (*ngroups >= cnt) ? 0 : -1;
		*ngroups = cnt;
	} else
		ret = -1;

	return ret;
}

int
getgroups(int gidsetlen, gid_t *gidset)
{
	if (gidsetlen < 2) {
		errno = EINVAL;
		return -1;
	}

	gidset[0] = 100;
	gidset[1] = 0;
	return 2;
}

struct passwd *
getpwnam(const char *login)
{
	struct passwd *p = &PwEntry;

	memset(p, 0, sizeof(*p));
	if (strcmp(login, "root") == 0) {
		p->pw_name = __UNCONST("root");
		p->pw_uid = 0;
		p->pw_gid = 0;
	} else if (strcmp(login, "test") == 0) {
		p->pw_name = __UNCONST("test");
		p->pw_uid = 100;
		p->pw_gid = 100;
	} else
		p = NULL;

	return p;
}

struct passwd *
getpwuid(uid_t uid)
{
	struct passwd *p = &PwEntry;

	memset(p, 0, sizeof(*p));
	if (uid == 0) {
		p->pw_name = __UNCONST("root");
		p->pw_uid = 0;
		p->pw_gid = 0;
	} else if (uid == 100) {
		p->pw_name = __UNCONST("test");
		p->pw_uid = 100;
		p->pw_gid = 100;
	} else
		p = NULL;

	return p;
}
