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

#include <sys/types.h>
#include <sys/socket.h>

#include <assert.h>
#include <err.h>
#include <libcasper.h>
#include <netdb.h>
#include <string.h>

#include <casper/cap_net.h>

#include "syslogd_cap.h"

/*
 * Convert the given prop_filter structure into an nvlist.
 * Return a heap allocated pointer to the resulting nvlist.
 */
nvlist_t *
prop_filter_to_nvlist(const struct prop_filter *pfilter)
{
	nvlist_t *nvl_prop_filter = nvlist_create(0);

	nvlist_add_number(nvl_prop_filter, "prop_type", pfilter->prop_type);
	nvlist_add_number(nvl_prop_filter, "cmp_type", pfilter->cmp_type);
	nvlist_add_number(nvl_prop_filter, "cmp_flags", pfilter->cmp_flags);
	nvlist_add_string(nvl_prop_filter, "pflt_strval", pfilter->pflt_strval);
	/*
	 * Do not bother adding pflt_re. It will be recompiled
	 * using pflt_strval later, if applicable.
	 */

	return (nvl_prop_filter);
}

/*
 * Convert the given nvlist into a prop_filter structure.
 * Return a heap allocated pointer to the resulting prop_filter.
 */
struct prop_filter *
nvlist_to_prop_filter(const nvlist_t *nvl_prop_filter)
{
	struct prop_filter *pfilter;

	pfilter = calloc(1, sizeof(*pfilter));
	if (pfilter == NULL)
		err(1, "calloc");
	pfilter->prop_type = nvlist_get_number(nvl_prop_filter, "prop_type");
	pfilter->cmp_type = nvlist_get_number(nvl_prop_filter, "cmp_type");
	pfilter->cmp_flags = nvlist_get_number(nvl_prop_filter, "cmp_flags");
	pfilter->pflt_strval = strdup(nvlist_get_string(nvl_prop_filter,
	    "pflt_strval"));
	if (pfilter->cmp_type == FILT_CMP_REGEX) {
		int re_flags = REG_NOSUB;
		pfilter->pflt_re = calloc(1, sizeof(*pfilter->pflt_re));
		if (pfilter->pflt_re == NULL)
			errx(1, "RE calloc() error");
		if ((pfilter->cmp_flags & FILT_FLAG_EXTENDED) != 0)
			re_flags |= REG_EXTENDED;
		if ((pfilter->cmp_flags & FILT_FLAG_ICASE) != 0)
			re_flags |= REG_ICASE;
		if (regcomp(pfilter->pflt_re, pfilter->pflt_strval,
		    re_flags) != 0)
			errx(1, "RE compilation error");
	}

	return (pfilter);
}

/*
 * Convert the given struct filed into an nvl_filed nvlist.
 * Return a heap allocated pointer to the resulting nvlist.
 */
nvlist_t *
filed_to_nvlist(const struct filed *filed)
{
	nvlist_t *nvl_filed = nvlist_create(0);
	enum f_type f_type = filed->f_type;
	size_t i, sz;

	nvlist_add_number(nvl_filed, "f_type", f_type);
	nvlist_add_string(nvl_filed, "f_host", filed->f_host);
	nvlist_add_string(nvl_filed, "f_program", filed->f_program);
	if (filed->f_prop_filter != NULL) {
		nvlist_add_nvlist(nvl_filed, "f_prop_filter",
		    prop_filter_to_nvlist(filed->f_prop_filter));
	}
	sz = nitems(filed->f_pmask);
	for (i = 0; i < sz; ++i) {
		nvlist_append_number_array(nvl_filed, "f_pmask",
		    filed->f_pmask[i]);
	}
	sz = nitems(filed->f_pcmp);
	for (i = 0; i < sz; ++i) {
		nvlist_append_number_array(nvl_filed, "f_pcmp",
		    filed->f_pcmp[i]);
	}

	if (filed->f_file >= 0)
		nvlist_add_descriptor(nvl_filed, "f_file", filed->f_file);
	nvlist_add_number(nvl_filed, "f_flags", filed->f_flags);
	if (f_type == F_WALL || f_type == F_USERS) {
		sz = nitems(filed->f_uname);
		for (i = 0; i < sz; ++i) {
			nvlist_append_string_array(nvl_filed, "f_uname",
			    filed->f_uname[i]);
		}
	} else if (f_type == F_FILE || f_type == F_CONSOLE || f_type == F_TTY) {
		nvlist_add_string(nvl_filed, "f_fname", filed->f_fname);
	} else if (f_type == F_FORW) {
		nvlist_add_string(nvl_filed, "f_hname", filed->f_hname);
		nvlist_add_descriptor_array(nvl_filed, "f_addr_fds",
		    filed->f_addr_fds, filed->f_num_addr_fds);
		nvlist_add_binary(nvl_filed, "f_addrs", filed->f_addrs,
		    filed->f_num_addr_fds * sizeof(*filed->f_addrs));
	} else if (filed->f_type == F_PIPE) {
		nvlist_add_string(nvl_filed, "f_pname", filed->f_pname);
		if (filed->f_procdesc >= 0) {
			nvlist_add_descriptor(nvl_filed, "f_procdesc",
			    filed->f_procdesc);
		}
	}

	/*
	 * Book-keeping fields are not transferred.
	 */

	return (nvl_filed);
}

/*
 * Convert the given nvl_filed nvlist into a struct filed.
 * Return a heap allocated pointer to the resulting struct
 * filed.
 */
struct filed *
nvlist_to_filed(const nvlist_t *nvl_filed)
{
	struct filed *filed;
	enum f_type f_type;
	const uint64_t *narr;
	size_t i, sz;

	filed = calloc(1, sizeof(*filed));
	if (filed == NULL)
		err(1, "calloc");

	f_type = filed->f_type = nvlist_get_number(nvl_filed, "f_type");
	(void)strlcpy(filed->f_host, nvlist_get_string(nvl_filed, "f_host"),
	    sizeof(filed->f_host));
	(void)strlcpy(filed->f_program, nvlist_get_string(nvl_filed,
	    "f_program"), sizeof(filed->f_program));
	if (nvlist_exists_nvlist(nvl_filed, "f_prop_filter")) {
		filed->f_prop_filter = nvlist_to_prop_filter(
		    nvlist_get_nvlist(nvl_filed, "f_prop_filter"));
	}
	narr = nvlist_get_number_array(nvl_filed, "f_pmask", &sz);
	assert(sz == nitems(filed->f_pmask));
	for (i = 0; i < sz; ++i)
		filed->f_pmask[i] = narr[i];
	narr = nvlist_get_number_array(nvl_filed, "f_pcmp", &sz);
	assert(sz == nitems(filed->f_pcmp));
	for (i = 0; i < sz; ++i)
		filed->f_pcmp[i] = narr[i];

	if (nvlist_exists_descriptor(nvl_filed, "f_file"))
		filed->f_file = dup(nvlist_get_descriptor(nvl_filed, "f_file"));
	else
		filed->f_file = -1;
	filed->f_flags = nvlist_get_number(nvl_filed, "f_flags");
	if (f_type == F_WALL || f_type == F_USERS) {
		const char * const *f_uname;

		f_uname = nvlist_get_string_array(nvl_filed, "f_uname", &sz);
		assert(sz == nitems(filed->f_uname));
		for (i = 0; i < sz; ++i) {
			(void)strlcpy(filed->f_uname[i], f_uname[i],
			    sizeof(filed->f_uname[i]));
		}
	} else if (f_type == F_FILE || f_type == F_CONSOLE || f_type == F_TTY) {
		(void)strlcpy(filed->f_fname, nvlist_get_string(nvl_filed,
		    "f_fname"), sizeof(filed->f_fname));
	} else if (f_type == F_FORW) {
		const int *f_addr_fds;

		(void)strlcpy(filed->f_hname, nvlist_get_string(nvl_filed,
		    "f_hname"), sizeof(filed->f_hname));

		f_addr_fds = nvlist_get_descriptor_array(nvl_filed,
		    "f_addr_fds", &filed->f_num_addr_fds);
		filed->f_addr_fds = calloc(filed->f_num_addr_fds,
		    sizeof(*f_addr_fds));
		if (filed->f_addr_fds == NULL)
			err(1, "calloc");
		for (i = 0; i < filed->f_num_addr_fds; ++i) {
			filed->f_addr_fds[i] = dup(f_addr_fds[i]);
			if (filed->f_addr_fds[i] < 0)
				err(1, "dup");
		}
	} else if (filed->f_type == F_PIPE) {
		(void)strlcpy(filed->f_pname, nvlist_get_string(nvl_filed,
		    "f_pname"), sizeof(filed->f_pname));
		if (nvlist_exists_descriptor(nvl_filed, "f_procdesc")) {
			filed->f_procdesc = dup(nvlist_get_descriptor(nvl_filed,
			    "f_procdesc"));
		} else {
			filed->f_procdesc = -1;
		}
	}

	/*
	 * Book-keeping fields are not transferred.
	 */

	return (filed);
}

nvlist_t *
cap_readconfigfile(cap_channel_t *chan, const char *path)
{
	nvlist_t *nvl, *nvl_conf;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", "readconfigfile");
	nvlist_add_string(nvl, "path", path);
	/* It is possible that our hostname has changed. */
	nvlist_add_string(nvl, "LocalHostName", LocalHostName);
	nvl = cap_xfer_nvlist(chan, nvl);
	if (nvl == NULL) {
		logerror("Failed to xfer configuration nvlist");
		exit(1);
	}
	nvl_conf = nvlist_take_nvlist(nvl, "nvl_conf");

	nvlist_destroy(nvl);
	return (nvl_conf);
}

/*
 * Now that we're executing as libcasper, we can obtain the
 * resources specified in the configuration.
 */
int
casper_readconfigfile(nvlist_t *nvlin, nvlist_t *nvlout)
{
	const nvlist_t * const *filed_list;
	nvlist_t *nvl_conf;
	size_t n_fileds;
	const char *path;

	/*
	 * Verify that syslogd did not manipulate the
	 * configuration file path.
	 */
	path = nvlist_get_string(nvlin, "path");
	if (strcmp(path, ConfFile) != 0)
		err(1, "Configuration file mismatch: %s != %s", path, ConfFile);

	/* Refresh our copy of LocalHostName, in case it changed. */
	strlcpy(LocalHostName, nvlist_get_string(nvlin, "LocalHostName"),
	    sizeof(LocalHostName));

	nvl_conf = readconfigfile(path);

	/* Remove old filed data in case we are reloading. */
	while (!SLIST_EMPTY(&cfiled_head)) {
		struct cap_filed *cfiled;

		cfiled = SLIST_FIRST(&cfiled_head);
		SLIST_REMOVE_HEAD(&cfiled_head, next);
		free(cfiled);
	}
	/* Record F_PIPE filed data for use in p_open(). */
	if (!nvlist_exists_nvlist_array(nvl_conf, "filed_list"))
		return (0);
	filed_list = nvlist_get_nvlist_array(nvl_conf, "filed_list", &n_fileds);
	for (size_t i = 0; i < n_fileds; ++i) {
		if (nvlist_get_number(filed_list[i], "f_type") == F_PIPE) {
			struct cap_filed *cfiled;
			const char *pipe_cmd;

			cfiled = malloc(sizeof(*cfiled));
			if (cfiled == NULL)
				err(1, "malloc");
			cfiled->idx = i;
			pipe_cmd = nvlist_get_string(filed_list[i], "f_pname");
			strlcpy(cfiled->pipe_cmd, pipe_cmd, sizeof(cfiled->pipe_cmd));
			SLIST_INSERT_HEAD(&cfiled_head, cfiled, next);
		}
	}

	nvlist_move_nvlist(nvlout, "nvl_conf", nvl_conf);
	return (0);
}
