/*-
 * Copyright (c) 2012 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
#include <sys/param.h>

#include <grp.h>
#include <pwd.h>

__FBSDID("$FreeBSD$");

int
pwcache_groupdb(
	int		(*a_setgroupent)(int) __unused,
	void		(*a_endgrent)(void) __unused,
	struct group *	(*a_getgrnam)(const char *) __unused,
	struct group *	(*a_getgrgid)(gid_t) __unused)
{

	return (-1);
}

int
pwcache_userdb(
	int		(*a_setpassent)(int) __unused,
	void		(*a_endpwent)(void) __unused,
	struct passwd *	(*a_getpwnam)(const char *) __unused,
	struct passwd *	(*a_getpwuid)(uid_t) __unused)
{

	return (-1);
}

int
gid_from_group(const char *group, gid_t *gid)
{
	struct group *grp;
	
	if ((grp = getgrnam(group)) == NULL)
		return (-1);
	*gid = grp->gr_gid;
	return (0);
}

int
uid_from_user(const char *user, uid_t *uid)
{
	struct passwd *pwd;

	if ((pwd = getpwnam(user)) == NULL)
		return(-1);
	*uid = pwd->pw_uid;
	return(0);
}
