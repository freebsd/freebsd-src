/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * This software was developed by Jake Freeland <jfree@FreeBSD.org>
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

#include <assert.h>
#include <err.h>
#include <string.h>

#include "syslogd_cap.h"

struct cfiled_list cfiled_head;

int
cap_p_open(cap_channel_t *chan, size_t filed_idx, const char *prog,
    int *procdesc)
{
	nvlist_t *nvl = nvlist_create(0);
	int error, pipedesc_w;

	nvlist_add_string(nvl, "cmd", "p_open");
	nvlist_add_number(nvl, "filed_idx", filed_idx);
	nvlist_add_string(nvl, "prog", prog);
	nvl = cap_xfer_nvlist(chan, nvl);
	if (nvl == NULL) {
		logerror("Failed to xfer p_open nvlist");
		exit(1);
	}
	error = nvlist_get_number(nvl, "error");
	if (error != 0) {
		errno = error;
		logerror("Failed to open piped command");
	}
	pipedesc_w = dnvlist_take_descriptor(nvl, "pipedesc_w", -1);
	*procdesc = dnvlist_take_descriptor(nvl, "procdesc", -1);

	nvlist_destroy(nvl);
	return (pipedesc_w);
}

int
casper_p_open(nvlist_t *nvlin, nvlist_t *nvlout)
{
	struct cap_filed *cfiled;
	size_t filed_idx;
	int pipedesc_w, procdesc = -1;
	const char *prog;

	filed_idx = nvlist_get_number(nvlin, "filed_idx");
	prog = nvlist_get_string(nvlin, "prog");
	SLIST_FOREACH(cfiled, &cfiled_head, next) {
		if (cfiled->idx != filed_idx)
			continue;
		if (strcmp(cfiled->pipe_cmd, prog) != 0)
			return (-1);

		pipedesc_w = p_open(prog, &procdesc);
		if (pipedesc_w == -1)
			return (-1);
		nvlist_move_descriptor(nvlout, "pipedesc_w", pipedesc_w);
		nvlist_move_descriptor(nvlout, "procdesc", procdesc);
		return (0);
	}

	return (-1);
}

const char *
cap_ttymsg(cap_channel_t *chan, struct iovec *iov, int iovcnt,
    const char *line, int tmout)
{
	nvlist_t *nvl = nvlist_create(0);
	int error;
	static char errbuf[1024];
	char *ret = NULL;

	nvlist_add_string(nvl, "cmd", "ttymsg");
	for (int i = 0; i < iovcnt; ++i)
		nvlist_append_string_array(nvl, "iov_strs", iov[i].iov_base);
	nvlist_add_string(nvl, "line", line);
	nvlist_add_number(nvl, "tmout", tmout);

	nvl = cap_xfer_nvlist(chan, nvl);
	if (nvl == NULL) {
		logerror("Failed to xfer ttymsg nvlist");
		exit(1);
	}
	error = nvlist_get_number(nvl, "error");
	if (error != 0) {
		errno = error;
		logerror("Failed to ttymsg");
	}
	if (nvlist_exists_string(nvl, "errstr")) {
		const char *errstr = nvlist_get_string(nvl, "errstr");
		(void)strlcpy(errbuf, errstr, sizeof(errbuf));
		ret = errbuf;
	}

	nvlist_destroy(nvl);
	return (ret);
}

int
casper_ttymsg(nvlist_t *nvlin, nvlist_t *nvlout)
{
	char **nvlstrs;
	struct iovec *iov;
	size_t iovcnt;
	int tmout;
	const char *line;

	nvlstrs = nvlist_take_string_array(nvlin, "iov_strs", &iovcnt);
	assert(iovcnt <= TTYMSG_IOV_MAX);
	iov = calloc(iovcnt, sizeof(*iov));
	if (iov == NULL)
		err(EXIT_FAILURE, "calloc");
	for (size_t i = 0; i < iovcnt; ++i) {
		iov[i].iov_base = nvlstrs[i];
		iov[i].iov_len = strlen(nvlstrs[i]);
	}
	line = nvlist_get_string(nvlin, "line");
	tmout = nvlist_get_number(nvlin, "tmout");
	line = ttymsg(iov, iovcnt, line, tmout);
	if (line != NULL)
		nvlist_add_string(nvlout, "errstr", line);

	free(iov);
	return (0);
}

void
cap_wallmsg(cap_channel_t *chan, const struct filed *f, struct iovec *iov,
    int iovcnt)
{
	nvlist_t *nvl = nvlist_create(0);
	int error;

	nvlist_add_string(nvl, "cmd", "wallmsg");
	/*
	 * The filed_to_nvlist() function is not needed
	 * here because wallmsg() only uses f_type and
	 * fu_uname members, which are both inline.
	 */
	nvlist_add_binary(nvl, "filed", f, sizeof(*f));
	for (int i = 0; i < iovcnt; ++i)
		nvlist_append_string_array(nvl, "iov_strs", iov[i].iov_base);

	nvl = cap_xfer_nvlist(chan, nvl);
	if (nvl == NULL) {
		logerror("Failed to xfer wallmsg nvlist");
		exit(1);
	}
	error = nvlist_get_number(nvl, "error");
	if (error != 0) {
		errno = error;
		logerror("Failed to wallmsg");
	}
	nvlist_destroy(nvl);
}

int
casper_wallmsg(nvlist_t *nvlin)
{
	const struct filed *f;
	char **nvlstrs;
	struct iovec *iov;
	size_t sz;

	f = nvlist_get_binary(nvlin, "filed", &sz);
	assert(sz == sizeof(*f));
	nvlstrs = nvlist_take_string_array(nvlin, "iov_strs", &sz);
	assert(sz <= TTYMSG_IOV_MAX);
	iov = calloc(sz, sizeof(*iov));
	if (iov == NULL)
		err(EXIT_FAILURE, "calloc");
	for (size_t i = 0; i < sz; ++i) {
		iov[i].iov_base = nvlstrs[i];
		iov[i].iov_len = strlen(nvlstrs[i]);
	}
	wallmsg(f, iov, sz);

	for (size_t i = 0; i < sz; ++i)
		free(iov[i].iov_base);
	free(iov);
	return (0);
}
