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
#include <sys/sysctl.h>
#include <sys/nv.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <libcapsicum.h>
#include <libcapsicum_sysctl.h>
#include <libcasper.h>
#include <pjdlog.h>

static int
sysctl_check_one(const nvlist_t *nvl, bool islimit)
{
	const char *name;
	void *cookie;
	int type;
	unsigned int fields;

	/* NULL nvl is of course invalid. */
	if (nvl == NULL)
		return (EINVAL);
	if (nvlist_error(nvl) != 0)
		return (nvlist_error(nvl));

#define	HAS_NAME	0x01
#define	HAS_OPERATION	0x02

	fields = 0;
	cookie = NULL;
	while ((name = nvlist_next(nvl, &type, &cookie)) != NULL) {
		/* We accept only one 'name' and one 'operation' in nvl. */
		if (strcmp(name, "name") == 0) {
			if (type != NV_TYPE_STRING)
				return (EINVAL);
			/* Only one 'name' can be present. */
			if ((fields & HAS_NAME) != 0)
				return (EINVAL);
			fields |= HAS_NAME;
		} else if (strcmp(name, "operation") == 0) {
			uint64_t operation;

			if (type != NV_TYPE_NUMBER)
				return (EINVAL);
			/*
			 * We accept only CAP_SYSCTL_READ and
			 * CAP_SYSCTL_WRITE flags.
			 */
			operation = nvlist_get_number(nvl, name);
			if ((operation & ~(CAP_SYSCTL_RDWR)) != 0)
				return (EINVAL);
			/* ...but there has to be at least one of them. */
			if ((operation & (CAP_SYSCTL_RDWR)) == 0)
				return (EINVAL);
			/* Only one 'operation' can be present. */
			if ((fields & HAS_OPERATION) != 0)
				return (EINVAL);
			fields |= HAS_OPERATION;
		} else if (islimit) {
			/* If this is limit, there can be no other fields. */
			return (EINVAL);
		}
	}

	/* Both fields has to be there. */
	if (fields != (HAS_NAME | HAS_OPERATION))
		return (EINVAL);

#undef	HAS_OPERATION
#undef	HAS_NAME

	return (0);
}

static bool
sysctl_allowed(const nvlist_t *limits, const char *chname, uint64_t choperation)
{
	uint64_t operation;
	const char *name;
	void *cookie;
	int type;

	if (limits == NULL)
		return (true);

	cookie = NULL;
	while ((name = nvlist_next(limits, &type, &cookie)) != NULL) {
		PJDLOG_ASSERT(type == NV_TYPE_NUMBER);

		operation = nvlist_get_number(limits, name);
		if ((operation & choperation) != choperation)
			continue;

		if ((operation & CAP_SYSCTL_RECURSIVE) == 0) {
			if (strcmp(name, chname) != 0)
				continue;
		} else {
			size_t namelen;

			namelen = strlen(name);
			if (strncmp(name, chname, namelen) != 0)
				continue;
			if (chname[namelen] != '.' && chname[namelen] != '\0')
				continue;
		}

		return (true);
	}

	return (false);
}

static int
sysctl_limit(const nvlist_t *oldlimits, const nvlist_t *newlimits)
{
	const nvlist_t *nvl;
	const char *name;
	void *cookie;
	uint64_t operation;
	int error, type;

	cookie = NULL;
	while ((name = nvlist_next(newlimits, &type, &cookie)) != NULL) {
		if (type != NV_TYPE_NUMBER)
			return (EINVAL);
		operation = nvlist_get_number(newlimits, name);
		if ((operation & ~(CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE)) != 0)
			return (EINVAL);
		if ((operation & (CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE)) == 0)
			return (EINVAL);
		if (!sysctl_allowed(oldlimits, name, operation))
			return (ENOTCAPABLE);
	}

	return (0);
}

static int
sysctl_command(const char *cmd, const nvlist_t *limits, nvlist_t *nvlin,
    nvlist_t *nvlout)
{
	const char *name;
	const void *newp;
	void *oldp;
	uint64_t operation;
	size_t oldlen, newlen;
	size_t *oldlenp;
	int error;

	if (strcmp(cmd, "sysctl") != 0)
		return (EINVAL);
	error = sysctl_check_one(nvlin, false);
	if (error != 0)
		return (error);

	name = nvlist_get_string(nvlin, "name");
	operation = nvlist_get_number(nvlin, "operation");
	if (!sysctl_allowed(limits, name, operation))
		return (ENOTCAPABLE);

	if ((operation & CAP_SYSCTL_WRITE) != 0) {
		if (!nvlist_exists_binary(nvlin, "newp"))
			return (EINVAL);
		newp = nvlist_get_binary(nvlin, "newp", &newlen);
		PJDLOG_ASSERT(newp != NULL && newlen > 0);
	} else {
		newp = NULL;
		newlen = 0;
	}

	if ((operation & CAP_SYSCTL_READ) != 0) {
		if (nvlist_exists_null(nvlin, "justsize")) {
			oldp = NULL;
			oldlen = 0;
			oldlenp = &oldlen;
		} else {
			if (!nvlist_exists_number(nvlin, "oldlen"))
				return (EINVAL);
			oldlen = (size_t)nvlist_get_number(nvlin, "oldlen");
			if (oldlen == 0)
				return (EINVAL);
			oldp = calloc(1, oldlen);
			if (oldp == NULL)
				return (ENOMEM);
			oldlenp = &oldlen;
		}
	} else {
		oldp = NULL;
		oldlen = 0;
		oldlenp = NULL;
	}

	if (sysctlbyname(name, oldp, oldlenp, newp, newlen) == -1) {
		error = errno;
		free(oldp);
		return (error);
	}

	if ((operation & CAP_SYSCTL_READ) != 0) {
		if (nvlist_exists_null(nvlin, "justsize"))
			nvlist_add_number(nvlout, "oldlen", (uint64_t)oldlen);
		else
			nvlist_move_binary(nvlout, "oldp", oldp, oldlen);
	}

	return (0);
}

int
main(int argc, char *argv[])
{

	return (service_start("system.sysctl", PARENT_FILENO, sysctl_limit,
	    sysctl_command, argc, argv));
}
