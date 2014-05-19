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

#include <sys/param.h>

#include <assert.h>
#include <errno.h>
#include <grp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dnv.h>
#include <nv.h>

#include "libcapsicum.h"
#include "libcapsicum_grp.h"

static struct group ggrp;
static char *gbuffer;
static size_t gbufsize;

static int
group_resize(void)
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
group_unpack_string(const nvlist_t *nvl, const char *fieldname, char **fieldp,
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
group_unpack_members(const nvlist_t *nvl, char ***fieldp, char **bufferp,
    size_t *bufsizep)
{
	const char *mem;
	char **outstrs, *str;
	size_t nmem, datasize, strsize;
	unsigned int ii;

	if (!nvlist_exists_number(nvl, "gr_nmem")) {
		datasize = _ALIGNBYTES + sizeof(char *);
		if (datasize >= *bufsizep)
			return (ERANGE);
		outstrs = (char **)_ALIGN(*bufferp);
		outstrs[0] = NULL;
		*fieldp = outstrs;
		*bufferp += datasize;
		*bufsizep -= datasize;
		return (0);
	}

	nmem = (size_t)nvlist_get_number(nvl, "gr_nmem");
	datasize = _ALIGNBYTES + sizeof(char *) * (nmem + 1);
	for (ii = 0; ii < nmem; ii++) {
		mem = dnvlist_getf_string(nvl, NULL, "gr_mem[%u]", ii);
		if (mem == NULL)
			return (EINVAL);
		datasize += strlen(mem) + 1;
	}

	if (datasize >= *bufsizep)
		return (ERANGE);

	outstrs = (char **)_ALIGN(*bufferp);
	str = (char *)outstrs + sizeof(char *) * (nmem + 1);
	for (ii = 0; ii < nmem; ii++) {
		mem = nvlist_getf_string(nvl, "gr_mem[%u]", ii);
		strsize = strlen(mem) + 1;
		memcpy(str, mem, strsize);
		outstrs[ii] = str;
		str += strsize;
	}
	assert(ii == nmem);
	outstrs[ii] = NULL;

	*fieldp = outstrs;
	*bufferp += datasize;
	*bufsizep -= datasize;

	return (0);
}

static int
group_unpack(const nvlist_t *nvl, struct group *grp, char *buffer,
    size_t bufsize)
{
	int error;

	if (!nvlist_exists_string(nvl, "gr_name"))
		return (EINVAL);

	memset(grp, 0, sizeof(*grp));

	error = group_unpack_string(nvl, "gr_name", &grp->gr_name, &buffer,
	    &bufsize);
	if (error != 0)
		return (error);
	error = group_unpack_string(nvl, "gr_passwd", &grp->gr_passwd, &buffer,
	    &bufsize);
	if (error != 0)
		return (error);
	grp->gr_gid = (gid_t)nvlist_get_number(nvl, "gr_gid");
	error = group_unpack_members(nvl, &grp->gr_mem, &buffer, &bufsize);
	if (error != 0)
		return (error);

	return (0);
}

static int
cap_getgrcommon_r(cap_channel_t *chan, const char *cmd, const char *name,
    gid_t gid, struct group *grp, char *buffer, size_t bufsize,
    struct group **result)
{
	nvlist_t *nvl;
	bool getgr_r;
	int error;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", cmd);
	if (strcmp(cmd, "getgrent") == 0 || strcmp(cmd, "getgrent_r") == 0) {
		/* Add nothing. */
	} else if (strcmp(cmd, "getgrnam") == 0 ||
	    strcmp(cmd, "getgrnam_r") == 0) {
		nvlist_add_string(nvl, "name", name);
	} else if (strcmp(cmd, "getgrgid") == 0 ||
	    strcmp(cmd, "getgrgid_r") == 0) {
		nvlist_add_number(nvl, "gid", (uint64_t)gid);
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

	if (!nvlist_exists_string(nvl, "gr_name")) {
		/* Not found. */
		nvlist_destroy(nvl);
		*result = NULL;
		return (0);
	}

	getgr_r = (strcmp(cmd, "getgrent_r") == 0 ||
	    strcmp(cmd, "getgrnam_r") == 0 || strcmp(cmd, "getgrgid_r") == 0);

	for (;;) {
		error = group_unpack(nvl, grp, buffer, bufsize);
		if (getgr_r || error != ERANGE)
			break;
		assert(buffer == gbuffer);
		assert(bufsize == gbufsize);
		error = group_resize();
		if (error != 0)
			break;
		/* Update pointers after resize. */
		buffer = gbuffer;
		bufsize = gbufsize;
	}

	nvlist_destroy(nvl);

	if (error == 0)
		*result = grp;
	else
		*result = NULL;

	return (error);
}

static struct group *
cap_getgrcommon(cap_channel_t *chan, const char *cmd, const char *name,
    gid_t gid)
{
	struct group *result;
	int error, serrno;

	serrno = errno;

	error = cap_getgrcommon_r(chan, cmd, name, gid, &ggrp, gbuffer,
	    gbufsize, &result);
	if (error != 0) {
		errno = error;
		return (NULL);
	}

	errno = serrno;

	return (result);
}

struct group *
cap_getgrent(cap_channel_t *chan)
{

	return (cap_getgrcommon(chan, "getgrent", NULL, 0));
}

struct group *
cap_getgrnam(cap_channel_t *chan, const char *name)
{

	return (cap_getgrcommon(chan, "getgrnam", name, 0));
}

struct group *
cap_getgrgid(cap_channel_t *chan, gid_t gid)
{

	return (cap_getgrcommon(chan, "getgrgid", NULL, gid));
}

int
cap_getgrent_r(cap_channel_t *chan, struct group *grp, char *buffer,
    size_t bufsize, struct group **result)
{

	return (cap_getgrcommon_r(chan, "getgrent_r", NULL, 0, grp, buffer,
	    bufsize, result));
}

int
cap_getgrnam_r(cap_channel_t *chan, const char *name, struct group *grp,
    char *buffer, size_t bufsize, struct group **result)
{

	return (cap_getgrcommon_r(chan, "getgrnam_r", name, 0, grp, buffer,
	    bufsize, result));
}

int
cap_getgrgid_r(cap_channel_t *chan, gid_t gid, struct group *grp, char *buffer,
    size_t bufsize, struct group **result)
{

	return (cap_getgrcommon_r(chan, "getgrgid_r", NULL, gid, grp, buffer,
	    bufsize, result));
}

int
cap_setgroupent(cap_channel_t *chan, int stayopen)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", "setgroupent");
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

int
cap_setgrent(cap_channel_t *chan)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", "setgrent");
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

void
cap_endgrent(cap_channel_t *chan)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", "endgrent");
	/* Ignore any errors, we have no way to report them. */
	nvlist_destroy(cap_xfer_nvlist(chan, nvl));
}

int
cap_grp_limit_cmds(cap_channel_t *chan, const char * const *cmds, size_t ncmds)
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
cap_grp_limit_fields(cap_channel_t *chan, const char * const *fields,
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
cap_grp_limit_groups(cap_channel_t *chan, const char * const *names,
    size_t nnames, gid_t *gids, size_t ngids)
{
	nvlist_t *limits, *groups;
	unsigned int i;

	if (cap_limit_get(chan, &limits) < 0)
		return (-1);
	if (limits == NULL) {
		limits = nvlist_create(0);
	} else {
		if (nvlist_exists_nvlist(limits, "groups"))
			nvlist_free_nvlist(limits, "groups");
	}
	groups = nvlist_create(0);
	for (i = 0; i < ngids; i++)
		nvlist_addf_number(groups, (uint64_t)gids[i], "gid%u", i);
	for (i = 0; i < nnames; i++)
		nvlist_addf_string(groups, names[i], "name%u", i);
	nvlist_move_nvlist(limits, "groups", groups);
	return (cap_limit_set(chan, limits));
}
