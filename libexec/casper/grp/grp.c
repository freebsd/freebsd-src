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
#include <grp.h>
#include <stdlib.h>
#include <string.h>

#include <libcapsicum.h>
#include <libcasper.h>
#include <nv.h>
#include <pjdlog.h>

static bool
grp_allowed_cmd(const nvlist_t *limits, const char *cmd)
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
grp_allowed_cmds(const nvlist_t *oldlimits, const nvlist_t *newlimits)
{
	const char *name;
	void *cookie;
	int type;

	cookie = NULL;
	while ((name = nvlist_next(newlimits, &type, &cookie)) != NULL) {
		if (type != NV_TYPE_NULL)
			return (EINVAL);
		if (!grp_allowed_cmd(oldlimits, name))
			return (ENOTCAPABLE);
	}

	return (0);
}

static bool
grp_allowed_group(const nvlist_t *limits, const char *gname, gid_t gid)
{
	const char *name;
	void *cookie;
	int type;

	if (limits == NULL)
		return (true);

	/*
	 * If no limit was set on allowed groups, then all groups are allowed.
	 */
	if (!nvlist_exists_nvlist(limits, "groups"))
		return (true);

	limits = nvlist_get_nvlist(limits, "groups");
	cookie = NULL;
	while ((name = nvlist_next(limits, &type, &cookie)) != NULL) {
		switch (type) {
		case NV_TYPE_NUMBER:
			if (gid != (gid_t)-1 &&
			    nvlist_get_number(limits, name) == (uint64_t)gid) {
				return (true);
			}
			break;
		case NV_TYPE_STRING:
			if (gname != NULL &&
			    strcmp(nvlist_get_string(limits, name),
			    gname) == 0) {
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
grp_allowed_groups(const nvlist_t *oldlimits, const nvlist_t *newlimits)
{
	const char *name, *gname;
	void *cookie;
	gid_t gid;
	int type;

	cookie = NULL;
	while ((name = nvlist_next(newlimits, &type, &cookie)) != NULL) {
		switch (type) {
		case NV_TYPE_NUMBER:
			gid = (gid_t)nvlist_get_number(newlimits, name);
			gname = NULL;
			break;
		case NV_TYPE_STRING:
			gid = (gid_t)-1;
			gname = nvlist_get_string(newlimits, name);
			break;
		default:
			return (EINVAL);
		}
		if (!grp_allowed_group(oldlimits, gname, gid))
			return (ENOTCAPABLE);
	}

	return (0);
}

static bool
grp_allowed_field(const nvlist_t *limits, const char *field)
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
grp_allowed_fields(const nvlist_t *oldlimits, const nvlist_t *newlimits)
{
	const char *name;
	void *cookie;
	int type;

	cookie = NULL;
	while ((name = nvlist_next(newlimits, &type, &cookie)) != NULL) {
		if (type != NV_TYPE_NULL)
			return (EINVAL);
		if (!grp_allowed_field(oldlimits, name))
			return (ENOTCAPABLE);
	}

	return (0);
}

static bool
grp_pack(const nvlist_t *limits, const struct group *grp, nvlist_t *nvl)
{

	if (grp == NULL)
		return (true);

	/*
	 * If either name or GID is allowed, we allow it.
	 */
	if (!grp_allowed_group(limits, grp->gr_name, grp->gr_gid))
		return (false);

	if (grp_allowed_field(limits, "gr_name"))
		nvlist_add_string(nvl, "gr_name", grp->gr_name);
	else
		nvlist_add_string(nvl, "gr_name", "");
	if (grp_allowed_field(limits, "gr_passwd"))
		nvlist_add_string(nvl, "gr_passwd", grp->gr_passwd);
	else
		nvlist_add_string(nvl, "gr_passwd", "");
	if (grp_allowed_field(limits, "gr_gid"))
		nvlist_add_number(nvl, "gr_gid", (uint64_t)grp->gr_gid);
	else
		nvlist_add_number(nvl, "gr_gid", (uint64_t)-1);
	if (grp_allowed_field(limits, "gr_mem") && grp->gr_mem[0] != NULL) {
		unsigned int ngroups;

		for (ngroups = 0; grp->gr_mem[ngroups] != NULL; ngroups++) {
			nvlist_addf_string(nvl, grp->gr_mem[ngroups],
			    "gr_mem[%u]", ngroups);
		}
		nvlist_add_number(nvl, "gr_nmem", (uint64_t)ngroups);
	}

	return (true);
}

static int
grp_getgrent(const nvlist_t *limits, const nvlist_t *nvlin, nvlist_t *nvlout)
{
	struct group *grp;

	for (;;) {
		errno = 0;
		grp = getgrent();
		if (errno != 0)
			return (errno);
		if (grp_pack(limits, grp, nvlout))
			return (0);
	}

	/* NOTREACHED */
}

static int
grp_getgrnam(const nvlist_t *limits, const nvlist_t *nvlin, nvlist_t *nvlout)
{
	struct group *grp;
	const char *name;

	if (!nvlist_exists_string(nvlin, "name"))
		return (EINVAL);
	name = nvlist_get_string(nvlin, "name");
	PJDLOG_ASSERT(name != NULL);

	errno = 0;
	grp = getgrnam(name);
	if (errno != 0)
		return (errno);

	(void)grp_pack(limits, grp, nvlout);

	return (0);
}

static int
grp_getgrgid(const nvlist_t *limits, const nvlist_t *nvlin, nvlist_t *nvlout)
{
	struct group *grp;
	gid_t gid;

	if (!nvlist_exists_number(nvlin, "gid"))
		return (EINVAL);

	gid = (gid_t)nvlist_get_number(nvlin, "gid");

	errno = 0;
	grp = getgrgid(gid);
	if (errno != 0)
		return (errno);

	(void)grp_pack(limits, grp, nvlout);

	return (0);
}

static int
grp_setgroupent(const nvlist_t *limits, const nvlist_t *nvlin, nvlist_t *nvlout)
{
	int stayopen;

	if (!nvlist_exists_bool(nvlin, "stayopen"))
		return (EINVAL);

	stayopen = nvlist_get_bool(nvlin, "stayopen") ? 1 : 0;

	return (setgroupent(stayopen) == 0 ? EFAULT : 0);
}

static int
grp_setgrent(const nvlist_t *limits, const nvlist_t *nvlin, nvlist_t *nvlout)
{

	return (setgrent() == 0 ? EFAULT : 0);
}

static int
grp_endgrent(const nvlist_t *limits, const nvlist_t *nvlin, nvlist_t *nvlout)
{

	endgrent();

	return (0);
}

static int
grp_limit(const nvlist_t *oldlimits, const nvlist_t *newlimits)
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
	if (oldlimits != NULL && nvlist_exists_nvlist(oldlimits, "groups") &&
	    !nvlist_exists_nvlist(newlimits, "groups")) {
		return (ENOTCAPABLE);
	}

	cookie = NULL;
	while ((name = nvlist_next(newlimits, &type, &cookie)) != NULL) {
		if (type != NV_TYPE_NVLIST)
			return (EINVAL);
		limits = nvlist_get_nvlist(newlimits, name);
		if (strcmp(name, "cmds") == 0)
			error = grp_allowed_cmds(oldlimits, limits);
		else if (strcmp(name, "fields") == 0)
			error = grp_allowed_fields(oldlimits, limits);
		else if (strcmp(name, "groups") == 0)
			error = grp_allowed_groups(oldlimits, limits);
		else
			error = EINVAL;
		if (error != 0)
			return (error);
	}

	return (0);
}

static int
grp_command(const char *cmd, const nvlist_t *limits, nvlist_t *nvlin,
    nvlist_t *nvlout)
{
	int error;

	if (!grp_allowed_cmd(limits, cmd))
		return (ENOTCAPABLE);

	if (strcmp(cmd, "getgrent") == 0 || strcmp(cmd, "getgrent_r") == 0)
		error = grp_getgrent(limits, nvlin, nvlout);
	else if (strcmp(cmd, "getgrnam") == 0 || strcmp(cmd, "getgrnam_r") == 0)
		error = grp_getgrnam(limits, nvlin, nvlout);
	else if (strcmp(cmd, "getgrgid") == 0 || strcmp(cmd, "getgrgid_r") == 0)
		error = grp_getgrgid(limits, nvlin, nvlout);
	else if (strcmp(cmd, "setgroupent") == 0)
		error = grp_setgroupent(limits, nvlin, nvlout);
	else if (strcmp(cmd, "setgrent") == 0)
		error = grp_setgrent(limits, nvlin, nvlout);
	else if (strcmp(cmd, "endgrent") == 0)
		error = grp_endgrent(limits, nvlin, nvlout);
	else
		error = EINVAL;

	return (error);
}

int
main(int argc, char *argv[])
{

	return (service_start("system.grp", PARENT_FILENO, grp_limit,
	    grp_command, argc, argv));
}
