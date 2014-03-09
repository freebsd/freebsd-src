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

#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>

#include <libcapsicum.h>
#include <libcasper.h>
#include <nv.h>
#include <pjdlog.h>

static bool
pwd_allowed_cmd(const nvlist_t *limits, const char *cmd)
{

	if (limits == NULL)
		return (true);

	/*
	 * If no limit was set on allowed commands, then all commands
	 * are allowed.
	 */
	if (!nvlist_exists_nvlist(limits, "cmds"))
		return (true);

	limits = nvlist_get_nvlist(limits, "cmds");
	return (nvlist_exists_null(limits, cmd));
}

static int
pwd_allowed_cmds(const nvlist_t *oldlimits, const nvlist_t *newlimits)
{
	const char *name;
	void *cookie;
	int type;

	cookie = NULL;
	while ((name = nvlist_next(newlimits, &type, &cookie)) != NULL) {
		if (type != NV_TYPE_NULL)
			return (EINVAL);
		if (!pwd_allowed_cmd(oldlimits, name))
			return (ENOTCAPABLE);
	}

	return (0);
}

static bool
pwd_allowed_user(const nvlist_t *limits, const char *uname, uid_t uid)
{
	const char *name;
	void *cookie;
	int type;

	if (limits == NULL)
		return (true);

	/*
	 * If no limit was set on allowed users, then all users are allowed.
	 */
	if (!nvlist_exists_nvlist(limits, "users"))
		return (true);

	limits = nvlist_get_nvlist(limits, "users");
	cookie = NULL;
	while ((name = nvlist_next(limits, &type, &cookie)) != NULL) {
		switch (type) {
		case NV_TYPE_NUMBER:
			if (uid != (uid_t)-1 &&
			    nvlist_get_number(limits, name) == (uint64_t)uid) {
				return (true);
			}
			break;
		case NV_TYPE_STRING:
			if (uname != NULL &&
			    strcmp(nvlist_get_string(limits, name),
			    uname) == 0) {
				return (true);
			}
			break;
		default:
			PJDLOG_ABORT("Unexpected type %d.", type);
		}
	}

	return (false);
}

static int
pwd_allowed_users(const nvlist_t *oldlimits, const nvlist_t *newlimits)
{
	const char *name, *uname;
	void *cookie;
	uid_t uid;
	int type;

	cookie = NULL;
	while ((name = nvlist_next(newlimits, &type, &cookie)) != NULL) {
		switch (type) {
		case NV_TYPE_NUMBER:
			uid = (uid_t)nvlist_get_number(newlimits, name);
			uname = NULL;
			break;
		case NV_TYPE_STRING:
			uid = (uid_t)-1;
			uname = nvlist_get_string(newlimits, name);
			break;
		default:
			return (EINVAL);
		}
		if (!pwd_allowed_user(oldlimits, uname, uid))
			return (ENOTCAPABLE);
	}

	return (0);
}

static bool
pwd_allowed_field(const nvlist_t *limits, const char *field)
{

	if (limits == NULL)
		return (true);

	/*
	 * If no limit was set on allowed fields, then all fields are allowed.
	 */
	if (!nvlist_exists_nvlist(limits, "fields"))
		return (true);

	limits = nvlist_get_nvlist(limits, "fields");
	return (nvlist_exists_null(limits, field));
}

static int
pwd_allowed_fields(const nvlist_t *oldlimits, const nvlist_t *newlimits)
{
	const char *name;
	void *cookie;
	int type;

	cookie = NULL;
	while ((name = nvlist_next(newlimits, &type, &cookie)) != NULL) {
		if (type != NV_TYPE_NULL)
			return (EINVAL);
		if (!pwd_allowed_field(oldlimits, name))
			return (ENOTCAPABLE);
	}

	return (0);
}

static bool
pwd_pack(const nvlist_t *limits, const struct passwd *pwd, nvlist_t *nvl)
{
	int fields;

	if (pwd == NULL)
		return (true);

	/*
	 * If either name or UID is allowed, we allow it.
	 */
	if (!pwd_allowed_user(limits, pwd->pw_name, pwd->pw_uid))
		return (false);

	fields = pwd->pw_fields;

	if (pwd_allowed_field(limits, "pw_name")) {
		nvlist_add_string(nvl, "pw_name", pwd->pw_name);
	} else {
		nvlist_add_string(nvl, "pw_name", "");
		fields &= ~_PWF_NAME;
	}
	if (pwd_allowed_field(limits, "pw_uid")) {
		nvlist_add_number(nvl, "pw_uid", (uint64_t)pwd->pw_uid);
	} else {
		nvlist_add_number(nvl, "pw_uid", (uint64_t)-1);
		fields &= ~_PWF_UID;
	}
	if (pwd_allowed_field(limits, "pw_gid")) {
		nvlist_add_number(nvl, "pw_gid", (uint64_t)pwd->pw_gid);
	} else {
		nvlist_add_number(nvl, "pw_gid", (uint64_t)-1);
		fields &= ~_PWF_GID;
	}
	if (pwd_allowed_field(limits, "pw_change")) {
		nvlist_add_number(nvl, "pw_change", (uint64_t)pwd->pw_change);
	} else {
		nvlist_add_number(nvl, "pw_change", (uint64_t)0);
		fields &= ~_PWF_CHANGE;
	}
	if (pwd_allowed_field(limits, "pw_passwd")) {
		nvlist_add_string(nvl, "pw_passwd", pwd->pw_passwd);
	} else {
		nvlist_add_string(nvl, "pw_passwd", "");
		fields &= ~_PWF_PASSWD;
	}
	if (pwd_allowed_field(limits, "pw_class")) {
		nvlist_add_string(nvl, "pw_class", pwd->pw_class);
	} else {
		nvlist_add_string(nvl, "pw_class", "");
		fields &= ~_PWF_CLASS;
	}
	if (pwd_allowed_field(limits, "pw_gecos")) {
		nvlist_add_string(nvl, "pw_gecos", pwd->pw_gecos);
	} else {
		nvlist_add_string(nvl, "pw_gecos", "");
		fields &= ~_PWF_GECOS;
	}
	if (pwd_allowed_field(limits, "pw_dir")) {
		nvlist_add_string(nvl, "pw_dir", pwd->pw_dir);
	} else {
		nvlist_add_string(nvl, "pw_dir", "");
		fields &= ~_PWF_DIR;
	}
	if (pwd_allowed_field(limits, "pw_shell")) {
		nvlist_add_string(nvl, "pw_shell", pwd->pw_shell);
	} else {
		nvlist_add_string(nvl, "pw_shell", "");
		fields &= ~_PWF_SHELL;
	}
	if (pwd_allowed_field(limits, "pw_expire")) {
		nvlist_add_number(nvl, "pw_expire", (uint64_t)pwd->pw_expire);
	} else {
		nvlist_add_number(nvl, "pw_expire", (uint64_t)0);
		fields &= ~_PWF_EXPIRE;
	}
	nvlist_add_number(nvl, "pw_fields", (uint64_t)fields);

	return (true);
}

static int
pwd_getpwent(const nvlist_t *limits, const nvlist_t *nvlin, nvlist_t *nvlout)
{
	struct passwd *pwd;

	for (;;) {
		errno = 0;
		pwd = getpwent();
		if (errno != 0)
			return (errno);
		if (pwd_pack(limits, pwd, nvlout))
			return (0);
	}

	/* NOTREACHED */
}

static int
pwd_getpwnam(const nvlist_t *limits, const nvlist_t *nvlin, nvlist_t *nvlout)
{
	struct passwd *pwd;
	const char *name;

	if (!nvlist_exists_string(nvlin, "name"))
		return (EINVAL);
	name = nvlist_get_string(nvlin, "name");
	PJDLOG_ASSERT(name != NULL);

	errno = 0;
	pwd = getpwnam(name);
	if (errno != 0)
		return (errno);

	(void)pwd_pack(limits, pwd, nvlout);

	return (0);
}

static int
pwd_getpwuid(const nvlist_t *limits, const nvlist_t *nvlin, nvlist_t *nvlout)
{
	struct passwd *pwd;
	uid_t uid;

	if (!nvlist_exists_number(nvlin, "uid"))
		return (EINVAL);

	uid = (uid_t)nvlist_get_number(nvlin, "uid");

	errno = 0;
	pwd = getpwuid(uid);
	if (errno != 0)
		return (errno);

	(void)pwd_pack(limits, pwd, nvlout);

	return (0);
}

static int
pwd_setpassent(const nvlist_t *limits, const nvlist_t *nvlin, nvlist_t *nvlout)
{
	int stayopen;

	if (!nvlist_exists_bool(nvlin, "stayopen"))
		return (EINVAL);

	stayopen = nvlist_get_bool(nvlin, "stayopen") ? 1 : 0;

	return (setpassent(stayopen) == 0 ? EFAULT : 0);
}

static int
pwd_setpwent(const nvlist_t *limits, const nvlist_t *nvlin, nvlist_t *nvlout)
{

	setpwent();

	return (0);
}

static int
pwd_endpwent(const nvlist_t *limits, const nvlist_t *nvlin, nvlist_t *nvlout)
{

	endpwent();

	return (0);
}

static int
pwd_limit(const nvlist_t *oldlimits, const nvlist_t *newlimits)
{
	const nvlist_t *limits;
	const char *name;
	void *cookie;
	int error, type;

	if (oldlimits != NULL && nvlist_exists_nvlist(oldlimits, "cmds") &&
	    !nvlist_exists_nvlist(newlimits, "cmds")) {
		return (ENOTCAPABLE);
	}
	if (oldlimits != NULL && nvlist_exists_nvlist(oldlimits, "fields") &&
	    !nvlist_exists_nvlist(newlimits, "fields")) {
		return (ENOTCAPABLE);
	}
	if (oldlimits != NULL && nvlist_exists_nvlist(oldlimits, "users") &&
	    !nvlist_exists_nvlist(newlimits, "users")) {
		return (ENOTCAPABLE);
	}

	cookie = NULL;
	while ((name = nvlist_next(newlimits, &type, &cookie)) != NULL) {
		if (type != NV_TYPE_NVLIST)
			return (EINVAL);
		limits = nvlist_get_nvlist(newlimits, name);
		if (strcmp(name, "cmds") == 0)
			error = pwd_allowed_cmds(oldlimits, limits);
		else if (strcmp(name, "fields") == 0)
			error = pwd_allowed_fields(oldlimits, limits);
		else if (strcmp(name, "users") == 0)
			error = pwd_allowed_users(oldlimits, limits);
		else
			error = EINVAL;
		if (error != 0)
			return (error);
	}

	return (0);
}

static int
pwd_command(const char *cmd, const nvlist_t *limits, nvlist_t *nvlin,
    nvlist_t *nvlout)
{
	int error;

	if (!pwd_allowed_cmd(limits, cmd))
		return (ENOTCAPABLE);

	if (strcmp(cmd, "getpwent") == 0 || strcmp(cmd, "getpwent_r") == 0)
		error = pwd_getpwent(limits, nvlin, nvlout);
	else if (strcmp(cmd, "getpwnam") == 0 || strcmp(cmd, "getpwnam_r") == 0)
		error = pwd_getpwnam(limits, nvlin, nvlout);
	else if (strcmp(cmd, "getpwuid") == 0 || strcmp(cmd, "getpwuid_r") == 0)
		error = pwd_getpwuid(limits, nvlin, nvlout);
	else if (strcmp(cmd, "setpassent") == 0)
		error = pwd_setpassent(limits, nvlin, nvlout);
	else if (strcmp(cmd, "setpwent") == 0)
		error = pwd_setpwent(limits, nvlin, nvlout);
	else if (strcmp(cmd, "endpwent") == 0)
		error = pwd_endpwent(limits, nvlin, nvlout);
	else
		error = EINVAL;

	return (error);
}

int
main(int argc, char *argv[])
{

	return (service_start("system.pwd", PARENT_FILENO, pwd_limit,
	    pwd_command, argc, argv));
}
