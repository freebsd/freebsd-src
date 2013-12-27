/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
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

#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nv.h>

#include "libcapsicum.h"
#include "libcapsicum_pwd.h"

static struct passwd gpwd;
static char *gbuffer;
static size_t gbufsize;

static int
passwd_resize(void)
{
	char *buf;

	if (gbufsize == 0)
		gbufsize = 1024;
	else
		gbufsize *= 2;

	buf = gbuffer;
	gbuffer = realloc(buf, gbufsize);
	if (gbuffer == NULL) {
		free(buf);
		gbufsize = 0;
		return (ENOMEM);
	}
	memset(gbuffer, 0, gbufsize);

	return (0);
}

static int
passwd_unpack_string(const nvlist_t *nvl, const char *fieldname, char **fieldp,
    char **bufferp, size_t *bufsizep)
{
	const char *str;
	size_t len;

	str = nvlist_get_string(nvl, fieldname);
	len = strlcpy(*bufferp, str, *bufsizep);
	if (len >= *bufsizep)
		return (ERANGE);
	*fieldp = *bufferp;
	*bufferp += len + 1;
	*bufsizep -= len + 1;

	return (0);
}

static int
passwd_unpack(const nvlist_t *nvl, struct passwd *pwd, char *buffer,
    size_t bufsize)
{
	int error;

	if (!nvlist_exists_string(nvl, "pw_name"))
		return (EINVAL);

	memset(pwd, 0, sizeof(*pwd));

	error = passwd_unpack_string(nvl, "pw_name", &pwd->pw_name, &buffer,
	    &bufsize);
	if (error != 0)
		return (error);
	pwd->pw_uid = (uid_t)nvlist_get_number(nvl, "pw_uid");
	pwd->pw_gid = (gid_t)nvlist_get_number(nvl, "pw_gid");
	pwd->pw_change = (time_t)nvlist_get_number(nvl, "pw_change");
	error = passwd_unpack_string(nvl, "pw_passwd", &pwd->pw_passwd, &buffer,
	    &bufsize);
	if (error != 0)
		return (error);
	error = passwd_unpack_string(nvl, "pw_class", &pwd->pw_class, &buffer,
	    &bufsize);
	if (error != 0)
		return (error);
	error = passwd_unpack_string(nvl, "pw_gecos", &pwd->pw_gecos, &buffer,
	    &bufsize);
	if (error != 0)
		return (error);
	error = passwd_unpack_string(nvl, "pw_dir", &pwd->pw_dir, &buffer,
	    &bufsize);
	if (error != 0)
		return (error);
	error = passwd_unpack_string(nvl, "pw_shell", &pwd->pw_shell, &buffer,
	    &bufsize);
	if (error != 0)
		return (error);
	pwd->pw_expire = (time_t)nvlist_get_number(nvl, "pw_expire");
	pwd->pw_fields = (int)nvlist_get_number(nvl, "pw_fields");

	return (0);
}

static int
cap_getpwcommon_r(cap_channel_t *chan, const char *cmd, const char *login,
    uid_t uid, struct passwd *pwd, char *buffer, size_t bufsize,
    struct passwd **result)
{
	nvlist_t *nvl;
	bool getpw_r;
	int error;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", cmd);
	if (strcmp(cmd, "getpwent") == 0 || strcmp(cmd, "getpwent_r") == 0) {
		/* Add nothing. */
	} else if (strcmp(cmd, "getpwnam") == 0 ||
	    strcmp(cmd, "getpwnam_r") == 0) {
		nvlist_add_string(nvl, "name", login);
	} else if (strcmp(cmd, "getpwuid") == 0 ||
	    strcmp(cmd, "getpwuid_r") == 0) {
		nvlist_add_number(nvl, "uid", (uint64_t)uid);
	} else {
		abort();
	}
	nvl = cap_xfer_nvlist(chan, nvl);
	if (nvl == NULL) {
		assert(errno != 0);
		*result = NULL;
		return (errno);
	}
	error = (int)nvlist_get_number(nvl, "error");
	if (error != 0) {
		nvlist_destroy(nvl);
		*result = NULL;
		return (error);
	}

	if (!nvlist_exists_string(nvl, "pw_name")) {
		/* Not found. */
		nvlist_destroy(nvl);
		*result = NULL;
		return (0);
	}

	getpw_r = (strcmp(cmd, "getpwent_r") == 0 ||
	    strcmp(cmd, "getpwnam_r") == 0 || strcmp(cmd, "getpwuid_r") == 0);

	for (;;) {
		error = passwd_unpack(nvl, pwd, buffer, bufsize);
		if (getpw_r || error != ERANGE)
			break;
		assert(buffer == gbuffer);
		assert(bufsize == gbufsize);
		error = passwd_resize();
		if (error != 0)
			break;
		/* Update pointers after resize. */
		buffer = gbuffer;
		bufsize = gbufsize;
	}

	nvlist_destroy(nvl);

	if (error == 0)
		*result = pwd;
	else
		*result = NULL;

	return (error);
}

static struct passwd *
cap_getpwcommon(cap_channel_t *chan, const char *cmd, const char *login,
    uid_t uid)
{
	struct passwd *result;
	int error, serrno;

	serrno = errno;

	error = cap_getpwcommon_r(chan, cmd, login, uid, &gpwd, gbuffer,
	    gbufsize, &result);
	if (error != 0) {
		errno = error;
		return (NULL);
	}

	errno = serrno;

	return (result);
}

struct passwd *
cap_getpwent(cap_channel_t *chan)
{

	return (cap_getpwcommon(chan, "getpwent", NULL, 0));
}

struct passwd *
cap_getpwnam(cap_channel_t *chan, const char *login)
{

	return (cap_getpwcommon(chan, "getpwnam", login, 0));
}

struct passwd *
cap_getpwuid(cap_channel_t *chan, uid_t uid)
{

	return (cap_getpwcommon(chan, "getpwuid", NULL, uid));
}

int
cap_getpwent_r(cap_channel_t *chan, struct passwd *pwd, char *buffer,
    size_t bufsize, struct passwd **result)
{

	return (cap_getpwcommon_r(chan, "getpwent_r", NULL, 0, pwd, buffer,
	    bufsize, result));
}

int
cap_getpwnam_r(cap_channel_t *chan, const char *name, struct passwd *pwd,
    char *buffer, size_t bufsize, struct passwd **result)
{

	return (cap_getpwcommon_r(chan, "getpwnam_r", name, 0, pwd, buffer,
	    bufsize, result));
}

int
cap_getpwuid_r(cap_channel_t *chan, uid_t uid, struct passwd *pwd, char *buffer,
    size_t bufsize, struct passwd **result)
{

	return (cap_getpwcommon_r(chan, "getpwuid_r", NULL, uid, pwd, buffer,
	    bufsize, result));
}

int
cap_setpassent(cap_channel_t *chan, int stayopen)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", "setpassent");
	nvlist_add_bool(nvl, "stayopen", stayopen != 0);
	nvl = cap_xfer_nvlist(chan, nvl);
	if (nvl == NULL)
		return (0);
	if (nvlist_get_number(nvl, "error") != 0) {
		errno = nvlist_get_number(nvl, "error");
		nvlist_destroy(nvl);
		return (0);
	}
	nvlist_destroy(nvl);

	return (1);
}

static void
cap_set_end_pwent(cap_channel_t *chan, const char *cmd)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", cmd);
	/* Ignore any errors, we have no way to report them. */
	nvlist_destroy(cap_xfer_nvlist(chan, nvl));
}

void
cap_setpwent(cap_channel_t *chan)
{

	cap_set_end_pwent(chan, "setpwent");
}

void
cap_endpwent(cap_channel_t *chan)
{

	cap_set_end_pwent(chan, "endpwent");
}

int
cap_pwd_limit_cmds(cap_channel_t *chan, const char * const *cmds, size_t ncmds)
{
	nvlist_t *limits, *nvl;
	unsigned int i;

	if (cap_limit_get(chan, &limits) < 0)
		return (-1);
	if (limits == NULL) {
		limits = nvlist_create(0);
	} else {
		if (nvlist_exists_nvlist(limits, "cmds"))
			nvlist_free_nvlist(limits, "cmds");
	}
	nvl = nvlist_create(0);
	for (i = 0; i < ncmds; i++)
		nvlist_add_null(nvl, cmds[i]);
	nvlist_move_nvlist(limits, "cmds", nvl);
	return (cap_limit_set(chan, limits));
}

int
cap_pwd_limit_fields(cap_channel_t *chan, const char * const *fields,
    size_t nfields)
{
	nvlist_t *limits, *nvl;
	unsigned int i;

	if (cap_limit_get(chan, &limits) < 0)
		return (-1);
	if (limits == NULL) {
		limits = nvlist_create(0);
	} else {
		if (nvlist_exists_nvlist(limits, "fields"))
			nvlist_free_nvlist(limits, "fields");
	}
	nvl = nvlist_create(0);
	for (i = 0; i < nfields; i++)
		nvlist_add_null(nvl, fields[i]);
	nvlist_move_nvlist(limits, "fields", nvl);
	return (cap_limit_set(chan, limits));
}

int
cap_pwd_limit_users(cap_channel_t *chan, const char * const *names,
    size_t nnames, uid_t *uids, size_t nuids)
{
	nvlist_t *limits, *users;
	unsigned int i;

	if (cap_limit_get(chan, &limits) < 0)
		return (-1);
	if (limits == NULL) {
		limits = nvlist_create(0);
	} else {
		if (nvlist_exists_nvlist(limits, "users"))
			nvlist_free_nvlist(limits, "users");
	}
	users = nvlist_create(0);
	for (i = 0; i < nuids; i++)
		nvlist_addf_number(users, (uint64_t)uids[i], "uid%u", i);
	for (i = 0; i < nnames; i++)
		nvlist_addf_string(users, names[i], "name%u", i);
	nvlist_move_nvlist(limits, "users", users);
	return (cap_limit_set(chan, limits));
}
