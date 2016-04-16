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

#include <sys/nv.h>

#include <errno.h>
#include <string.h>

#include "libcapsicum.h"
#include "libcapsicum_sysctl.h"

int
cap_sysctlbyname(cap_channel_t *chan, const char *name, void *oldp,
    size_t *oldlenp, const void *newp, size_t newlen)
{
	nvlist_t *nvl;
	const uint8_t *retoldp;
	uint8_t operation;
	size_t oldlen;

	operation = 0;
	if (oldp != NULL)
		operation |= CAP_SYSCTL_READ;
	if (newp != NULL)
		operation |= CAP_SYSCTL_WRITE;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", "sysctl");
	nvlist_add_string(nvl, "name", name);
	nvlist_add_number(nvl, "operation", (uint64_t)operation);
	if (oldp == NULL && oldlenp != NULL)
		nvlist_add_null(nvl, "justsize");
	else if (oldlenp != NULL)
		nvlist_add_number(nvl, "oldlen", (uint64_t)*oldlenp);
	if (newp != NULL)
		nvlist_add_binary(nvl, "newp", newp, newlen);
	nvl = cap_xfer_nvlist(chan, nvl, 0);
	if (nvl == NULL)
		return (-1);
	if (nvlist_get_number(nvl, "error") != 0) {
		errno = (int)nvlist_get_number(nvl, "error");
		nvlist_destroy(nvl);
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
