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

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <nv.h>
#include "msgio.h"

#include "libcapsicum.h"
#include "libcapsicum_impl.h"
#include "libcapsicum_service.h"

cap_channel_t *
cap_service_open(const cap_channel_t *chan, const char *name)
{
	cap_channel_t *newchan;
	nvlist_t *nvl;
	int sock, error;

	sock = -1;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", "open");
	nvlist_add_string(nvl, "service", name);
	if (fd_is_valid(STDERR_FILENO))
		nvlist_add_descriptor(nvl, "stderrfd", STDERR_FILENO);
	nvl = cap_xfer_nvlist(chan, nvl);
	if (nvl == NULL)
		return (NULL);
	error = (int)nvlist_get_number(nvl, "error");
	if (error != 0) {
		nvlist_destroy(nvl);
		errno = error;
		return (NULL);
	}
	sock = nvlist_take_descriptor(nvl, "chanfd");
	assert(sock >= 0);
	nvlist_destroy(nvl);
	nvl = NULL;
	if (cred_send(sock) == -1)
		goto fail;
	newchan = cap_wrap(sock);
	if (newchan == NULL)
		goto fail;
	return (newchan);
fail:
	error = errno;
	close(sock);
	errno = error;
	return (NULL);
}

int
cap_service_limit(const cap_channel_t *chan, const char * const *names,
    size_t nnames)
{
	nvlist_t *limits;
	unsigned int i;

	limits = nvlist_create(0);
	for (i = 0; i < nnames; i++)
		nvlist_add_null(limits, names[i]);
	return (cap_limit_set(chan, limits));
}
