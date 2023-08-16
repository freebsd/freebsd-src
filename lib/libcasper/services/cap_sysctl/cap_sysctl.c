/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013, 2018 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
 *
 * Portions of this software were developed by Mark Johnston
 * under sponsorship from the FreeBSD Foundation.
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
#include <sys/param.h>
#include <sys/cnv.h>
#include <sys/dnv.h>
#include <sys/nv.h>
#include <sys/sysctl.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <libcasper.h>
#include <libcasper_service.h>

#include "cap_sysctl.h"

/*
 * Limit interface.
 */

struct cap_sysctl_limit {
	cap_channel_t *chan;
	nvlist_t *nv;
};

cap_sysctl_limit_t *
cap_sysctl_limit_init(cap_channel_t *chan)
{
	cap_sysctl_limit_t *limit;
	int error;

	limit = malloc(sizeof(*limit));
	if (limit != NULL) {
		limit->chan = chan;
		limit->nv = nvlist_create(NV_FLAG_NO_UNIQUE);
		if (limit->nv == NULL) {
			error = errno;
			free(limit);
			limit = NULL;
			errno = error;
		}
	}
	return (limit);
}

cap_sysctl_limit_t *
cap_sysctl_limit_name(cap_sysctl_limit_t *limit, const char *name, int flags)
{
	nvlist_t *lnv;
	size_t mibsz;
	int error, mib[CTL_MAXNAME];

	lnv = nvlist_create(0);
	if (lnv == NULL) {
		error = errno;
		if (limit->nv != NULL)
			nvlist_destroy(limit->nv);
		free(limit);
		errno = error;
		return (NULL);
	}
	nvlist_add_string(lnv, "name", name);
	nvlist_add_number(lnv, "operation", flags);

	mibsz = nitems(mib);
	error = cap_sysctlnametomib(limit->chan, name, mib, &mibsz);
	if (error == 0)
		nvlist_add_binary(lnv, "mib", mib, mibsz * sizeof(int));

	nvlist_move_nvlist(limit->nv, "limit", lnv);
	return (limit);
}

cap_sysctl_limit_t *
cap_sysctl_limit_mib(cap_sysctl_limit_t *limit, const int *mibp, u_int miblen,
    int flags)
{
	nvlist_t *lnv;
	int error;

	lnv = nvlist_create(0);
	if (lnv == NULL) {
		error = errno;
		if (limit->nv != NULL)
			nvlist_destroy(limit->nv);
		free(limit);
		errno = error;
		return (NULL);
	}
	nvlist_add_binary(lnv, "mib", mibp, miblen * sizeof(int));
	nvlist_add_number(lnv, "operation", flags);
	nvlist_add_nvlist(limit->nv, "limit", lnv);
	return (limit);
}

int
cap_sysctl_limit(cap_sysctl_limit_t *limit)
{
	cap_channel_t *chan;
	nvlist_t *lnv;

	chan = limit->chan;
	lnv = limit->nv;
	free(limit);

	/* cap_limit_set(3) will always free the nvlist. */
	return (cap_limit_set(chan, lnv));
}

/*
 * Service interface.
 */

static int
do_sysctl(cap_channel_t *chan, nvlist_t *nvl, void *oldp, size_t *oldlenp,
    const void *newp, size_t newlen)
{
	const uint8_t *retoldp;
	size_t oldlen;
	int error;
	uint8_t operation;

	operation = 0;
	if (oldlenp != NULL)
		operation |= CAP_SYSCTL_READ;
	if (newp != NULL)
		operation |= CAP_SYSCTL_WRITE;
	nvlist_add_number(nvl, "operation", (uint64_t)operation);
	if (oldp == NULL && oldlenp != NULL)
		nvlist_add_null(nvl, "justsize");
	else if (oldlenp != NULL)
		nvlist_add_number(nvl, "oldlen", (uint64_t)*oldlenp);
	if (newp != NULL)
		nvlist_add_binary(nvl, "newp", newp, newlen);

	nvl = cap_xfer_nvlist(chan, nvl);
	if (nvl == NULL)
		return (-1);
	error = (int)dnvlist_get_number(nvl, "error", 0);
	if (error != 0) {
		nvlist_destroy(nvl);
		errno = error;
		return (-1);
	}

	if (oldp == NULL && oldlenp != NULL) {
		*oldlenp = (size_t)nvlist_get_number(nvl, "oldlen");
	} else if (oldp != NULL) {
		retoldp = nvlist_get_binary(nvl, "oldp", &oldlen);
		memcpy(oldp, retoldp, oldlen);
		if (oldlenp != NULL)
			*oldlenp = oldlen;
	}

	nvlist_destroy(nvl);

	return (0);
}

int
cap_sysctl(cap_channel_t *chan, const int *name, u_int namelen, void *oldp,
    size_t *oldlenp, const void *newp, size_t newlen)
{
	nvlist_t *req;

	req = nvlist_create(0);
	nvlist_add_string(req, "cmd", "sysctl");
	nvlist_add_binary(req, "mib", name, (size_t)namelen * sizeof(int));
	return (do_sysctl(chan, req, oldp, oldlenp, newp, newlen));
}

int
cap_sysctlbyname(cap_channel_t *chan, const char *name, void *oldp,
    size_t *oldlenp, const void *newp, size_t newlen)
{
	nvlist_t *req;

	req = nvlist_create(0);
	nvlist_add_string(req, "cmd", "sysctlbyname");
	nvlist_add_string(req, "name", name);
	return (do_sysctl(chan, req, oldp, oldlenp, newp, newlen));
}

int
cap_sysctlnametomib(cap_channel_t *chan, const char *name, int *mibp,
    size_t *sizep)
{
	nvlist_t *req;
	const void *mib;
	size_t mibsz;
	int error;

	req = nvlist_create(0);
	nvlist_add_string(req, "cmd", "sysctlnametomib");
	nvlist_add_string(req, "name", name);
	nvlist_add_number(req, "operation", 0);
	nvlist_add_number(req, "size", (uint64_t)*sizep);

	req = cap_xfer_nvlist(chan, req);
	if (req == NULL)
		return (-1);
	error = (int)dnvlist_get_number(req, "error", 0);
	if (error != 0) {
		nvlist_destroy(req);
		errno = error;
		return (-1);
	}

	mib = nvlist_get_binary(req, "mib", &mibsz);
	*sizep = mibsz / sizeof(int);

	memcpy(mibp, mib, mibsz); 

	nvlist_destroy(req);

	return (0);
}

/*
 * Service implementation.
 */

/*
 * Validate a sysctl description.  This must consist of an nvlist with either a
 * binary "mib" field or a string "name", and an operation.
 */
static int
sysctl_valid(const nvlist_t *nvl, bool limit)
{
	const char *name;
	void *cookie;
	int type;
	size_t size;
	unsigned int field, fields;

	/* NULL nvl is of course invalid. */
	if (nvl == NULL)
		return (EINVAL);
	if (nvlist_error(nvl) != 0)
		return (nvlist_error(nvl));

#define	HAS_NAME	0x01
#define	HAS_MIB		0x02
#define	HAS_ID		(HAS_NAME | HAS_MIB)
#define	HAS_OPERATION	0x04

	fields = 0;
	cookie = NULL;
	while ((name = nvlist_next(nvl, &type, &cookie)) != NULL) {
		if ((strcmp(name, "name") == 0 && type == NV_TYPE_STRING) ||
		    (strcmp(name, "mib") == 0 && type == NV_TYPE_BINARY)) {
			if (strcmp(name, "mib") == 0) {
				/* A MIB must be an array of integers. */
				(void)cnvlist_get_binary(cookie, &size);
				if (size % sizeof(int) != 0)
					return (EINVAL);
				field = HAS_MIB;
			} else
				field = HAS_NAME;

			/*
			 * A limit may contain both a name and a MIB identifier.
			 */
			if ((fields & field) != 0 ||
			    (!limit && (fields & HAS_ID) != 0))
				return (EINVAL);
			fields |= field;
		} else if (strcmp(name, "operation") == 0) {
			uint64_t mask, operation;

			if (type != NV_TYPE_NUMBER)
				return (EINVAL);

			operation = cnvlist_get_number(cookie);

			/*
			 * Requests can only include the RDWR flags; limits may
			 * also include the RECURSIVE flag.
			 */
			mask = limit ? (CAP_SYSCTL_RDWR |
			    CAP_SYSCTL_RECURSIVE) : CAP_SYSCTL_RDWR;
			if ((operation & ~mask) != 0 ||
			    (operation & CAP_SYSCTL_RDWR) == 0)
				return (EINVAL);
			/* Only one 'operation' can be present. */
			if ((fields & HAS_OPERATION) != 0)
				return (EINVAL);
			fields |= HAS_OPERATION;
		} else if (limit)
			return (EINVAL);
	}

	if ((fields & HAS_OPERATION) == 0 || (fields & HAS_ID) == 0)
		return (EINVAL);

#undef HAS_OPERATION
#undef HAS_ID
#undef HAS_MIB
#undef HAS_NAME

	return (0);
}

static bool
sysctl_allowed(const nvlist_t *limits, const nvlist_t *req)
{
	const nvlist_t *limit;
	uint64_t op, reqop;
	const char *lname, *name, *reqname;
	void *cookie;
	size_t lsize, reqsize;
	const int *lmib, *reqmib;
	int type;

	if (limits == NULL)
		return (true);

	reqmib = dnvlist_get_binary(req, "mib", &reqsize, NULL, 0);
	reqname = dnvlist_get_string(req, "name", NULL);
	reqop = nvlist_get_number(req, "operation");

	cookie = NULL;
	while ((name = nvlist_next(limits, &type, &cookie)) != NULL) {
		assert(type == NV_TYPE_NVLIST);

		limit = cnvlist_get_nvlist(cookie);
		op = nvlist_get_number(limit, "operation");
		if ((reqop & op) != reqop)
			continue;

		if (reqname != NULL) {
			lname = dnvlist_get_string(limit, "name", NULL);
			if (lname == NULL)
				continue;
			if ((op & CAP_SYSCTL_RECURSIVE) == 0) {
				if (strcmp(lname, reqname) != 0)
					continue;
			} else {
				size_t namelen;

				namelen = strlen(lname);
				if (strncmp(lname, reqname, namelen) != 0)
					continue;
				if (reqname[namelen] != '.' &&
				    reqname[namelen] != '\0')
					continue;
			}
		} else {
			lmib = dnvlist_get_binary(limit, "mib", &lsize, NULL, 0);
			if (lmib == NULL)
				continue;
			if (lsize > reqsize || ((op & CAP_SYSCTL_RECURSIVE) == 0 &&
			    lsize < reqsize))
				continue;
			if (memcmp(lmib, reqmib, lsize) != 0)
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
	int error, type;

	cookie = NULL;
	while ((name = nvlist_next(newlimits, &type, &cookie)) != NULL) {
		if (strcmp(name, "limit") != 0 || type != NV_TYPE_NVLIST)
			return (EINVAL);
		nvl = cnvlist_get_nvlist(cookie);
		error = sysctl_valid(nvl, true);
		if (error != 0)
			return (error);
		if (!sysctl_allowed(oldlimits, nvl))
			return (ENOTCAPABLE);
	}

	return (0);
}

static int
nametomib(const nvlist_t *limits, const nvlist_t *nvlin, nvlist_t *nvlout)
{
	const char *name;
	size_t size;
	int error, *mibp;

	if (!sysctl_allowed(limits, nvlin))
		return (ENOTCAPABLE);

	name = nvlist_get_string(nvlin, "name");
	size = (size_t)nvlist_get_number(nvlin, "size");

	mibp = malloc(size * sizeof(*mibp));
	if (mibp == NULL)
		return (ENOMEM);

	error = sysctlnametomib(name, mibp, &size);
	if (error != 0) {
		error = errno;
		free(mibp);
		return (error);
	}

	nvlist_add_binary(nvlout, "mib", mibp, size * sizeof(*mibp));

	return (0);
}

static int
sysctl_command(const char *cmd, const nvlist_t *limits, nvlist_t *nvlin,
    nvlist_t *nvlout)
{
	const char *name;
	const void *newp;
	const int *mibp;
	void *oldp;
	uint64_t operation;
	size_t oldlen, newlen, size;
	size_t *oldlenp;
	int error;

	if (strcmp(cmd, "sysctlnametomib") == 0)
		return (nametomib(limits, nvlin, nvlout));

	if (strcmp(cmd, "sysctlbyname") != 0 && strcmp(cmd, "sysctl") != 0)
		return (EINVAL);
	error = sysctl_valid(nvlin, false);
	if (error != 0)
		return (error);
	if (!sysctl_allowed(limits, nvlin))
		return (ENOTCAPABLE);

	operation = nvlist_get_number(nvlin, "operation");
	if ((operation & CAP_SYSCTL_WRITE) != 0) {
		if (!nvlist_exists_binary(nvlin, "newp"))
			return (EINVAL);
		newp = nvlist_get_binary(nvlin, "newp", &newlen);
		assert(newp != NULL && newlen > 0);
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

	if (strcmp(cmd, "sysctlbyname") == 0) {
		name = nvlist_get_string(nvlin, "name");
		error = sysctlbyname(name, oldp, oldlenp, newp, newlen);
	} else {
		mibp = nvlist_get_binary(nvlin, "mib", &size);
		error = sysctl(mibp, size / sizeof(*mibp), oldp, oldlenp, newp,
		    newlen);
	}
	if (error != 0) {
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

CREATE_SERVICE("system.sysctl", sysctl_limit, sysctl_command, 0);
